#!/usr/bin/env bash
# test_monitor_2way_sync_step.sh — restored sync-step monitor
#
# Restored from one4all git history (commits 6c5eee41, e3d2bdb6, 245af434, a4a27ab7).
# The historic 3-way harness compared CSNOBOL4 + SPITBOL + scrip-emitted-x86-ASM,
# but the run-asm pipeline was retired (commit 2c760e3d, April 7).  scrip's IR/SM/JIT
# executors do not implement LOAD()/TRACE() builtins yet, so they cannot speak the
# FIFO protocol directly.  This harness restores the historic 2-way subset
# (CSNOBOL4 oracle + SPITBOL participant) and is the working baseline.
#
# Wire protocol — RS/US (ASCII 0x1E / 0x1F):
#   KIND \x1E name \x1F value \x1E
# Per-participant FIFO pair:
#   <name>.ready — participant writes events, controller reads
#   <name>.go    — controller writes 'G' (go) or 'S' (stop), participant reads
#
# Usage:  bash test_monitor_2way_sync_step.sh <file.sno> [tracepoints.conf]
# Exit:   0 = all participants agreed   1 = divergence   2 = timeout/error
#
# Path discipline (RULES.md): every path derived from $0 or hardcoded;
# no env-var dependencies for tool location.
set -uo pipefail

SNO=${1:?Usage: test_monitor_2way_sync_step.sh <file.sno> [tracepoints_conf]}
HERE="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
MON_DIR="$HERE/monitor"
CONF=${2:-$MON_DIR/tracepoints.conf}

X64_DIR="${X64_DIR:-/home/claude/x64}"
SPITBOL="$X64_DIR/bin/sbl"
CSNOBOL4="/home/claude/csnobol4/snobol4"
INC="${INC:-/home/claude/corpus/programs/include}"

CSN_SO="$MON_DIR/monitor_ipc_sync.so"
SPL_SO="$X64_DIR/monitor_ipc_spitbol.so"

TIMEOUT="${MONITOR_TIMEOUT:-10}"

# Check prerequisites
[[ -x "$SPITBOL"  ]] || { echo "FAIL spitbol not built: $SPITBOL";   exit 2; }
[[ -x "$CSNOBOL4" ]] || { echo "FAIL csnobol4 not built: $CSNOBOL4"; exit 2; }
[[ -f "$CSN_SO"   ]] || { echo "FAIL CSN .so missing: $CSN_SO   — run build_monitor.sh"; exit 2; }
[[ -f "$SPL_SO"   ]] || { echo "FAIL SPL .so missing: $SPL_SO";  exit 2; }
[[ -f "$MON_DIR/inject_traces.py" ]] || { echo "FAIL inject_traces.py missing: $MON_DIR/inject_traces.py"; exit 2; }
[[ -f "$MON_DIR/monitor_sync.py"  ]] || { echo "FAIL monitor_sync.py missing: $MON_DIR/monitor_sync.py";   exit 2; }
[[ -f "$CONF" ]] || { echo "FAIL tracepoints.conf missing: $CONF"; exit 2; }

TMP=$(mktemp -d /tmp/monitor_2way_XXXXXX)


base="$(basename "$SNO" .sno)"
STDIN_SRC="/dev/null"
[[ -f "${SNO%.sno}.input" ]] && STDIN_SRC="${SNO%.sno}.input"

echo "[2way] program: $base"
echo "[2way] tmp:     $TMP"

# ── Step 1: inject traces ────────────────────────────────────────────────
SNO_LIB="$INC" python3 "$MON_DIR/inject_traces.py" "$SNO" "$CONF" > "$TMP/instr.sno"
inj_lines=$(wc -l < "$TMP/instr.sno")
orig_lines=$(wc -l < "$SNO")
echo "[2way] inject:  $orig_lines source -> $inj_lines instrumented lines"

# ── Step 2: create FIFOs (one pair per participant) ──────────────────────
# csn = oracle (participant 0); spl = SPITBOL (participant 1)
NAMES="csn spl"
for p in $NAMES; do
    mkfifo "$TMP/$p.ready"
    mkfifo "$TMP/$p.go"
done
READY_PATHS="$TMP/csn.ready,$TMP/spl.ready"
GO_PATHS="$TMP/csn.go,$TMP/spl.go"

# ── Step 3: launch participants (background) ─────────────────────────────
# Both oracles run in case-sensitive mode (-f).  Beauty's double-function
# trick (visit/Visit, etc.) requires it.  Per RULES.md "Case-sensitive
# name space — always, for every .sno and .inc".
#
# CSNOBOL4: -P256k pattern stack; -bf = banner-off + fold-off.
# SPITBOL:  -bf same meaning (SN-30 made this work on x64).
MONITOR_READY_PIPE="$TMP/csn.ready" \
MONITOR_GO_PIPE="$TMP/csn.go" \
MONITOR_SO="$CSN_SO" \
    timeout "$((TIMEOUT*2))" "$CSNOBOL4" -bf -P256k -S 64k -I"$INC" "$TMP/instr.sno" \
    < "$STDIN_SRC" > "$TMP/csn.out" 2>"$TMP/csn.err" &
CSN_PID=$!

MONITOR_READY_PIPE="$TMP/spl.ready" \
MONITOR_GO_PIPE="$TMP/spl.go" \
MONITOR_SO="$SPL_SO" \
SETL4PATH=".:$INC" \
    timeout "$((TIMEOUT*2))" "$SPITBOL" -bf "$TMP/instr.sno" \
    < "$STDIN_SRC" > "$TMP/spl.out" 2>"$TMP/spl.err" &
SPL_PID=$!

# ── Step 4: launch sync controller ───────────────────────────────────────
MONITOR_LOG_DIR="$TMP" \
MONITOR_IGNORE_FILE="$CONF" \
python3 "$MON_DIR/monitor_sync.py" \
    "$TIMEOUT" \
    "csn,spl" \
    "$READY_PATHS" \
    "$GO_PATHS" > "$TMP/ctrl.out" 2>&1 &
CTRL_PID=$!

# ── Step 5: wait + reap ──────────────────────────────────────────────────
wait $CTRL_PID
CTRL_RC=$?

for pid in $CSN_PID $SPL_PID; do kill "$pid" 2>/dev/null || true; done
wait 2>/dev/null || true

# ── Step 6: report ────────────────────────────────────────────────────────
echo ""
echo "── controller output ──"
cat "$TMP/ctrl.out"
echo ""
echo "── csn stdout (head) ──"; head -20 "$TMP/csn.out"
echo "── csn stderr (head) ──"; head -10 "$TMP/csn.err"
echo "── spl stdout (head) ──"; head -20 "$TMP/spl.out"
echo "── spl stderr (head) ──"; head -10 "$TMP/spl.err"

echo ""
echo "── csn events log (head) ──"
[[ -f "$TMP/csn.events.log" ]] && head -20 "$TMP/csn.events.log" || echo "  (no log)"
echo "── spl events log (head) ──"
[[ -f "$TMP/spl.events.log" ]] && head -20 "$TMP/spl.events.log" || echo "  (no log)"

# Persist the artefacts before TMP cleanup so caller can inspect
cp -r "$TMP" /tmp/monitor_2way_last 2>/dev/null
echo ""
echo "[2way] artefacts archived: /tmp/monitor_2way_last/"
echo "[2way] exit: $CTRL_RC"
exit $CTRL_RC
