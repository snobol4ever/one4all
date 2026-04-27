#!/usr/bin/env bash
# test_monitor_3way_sync_step_auto.sh — auto-mode binary sync-step monitor.
#
# This is the SN-26-auto successor to test_monitor_3way_sync_step_bin.sh.
# Key differences from its predecessor:
#
#   1. NO source preprocessing.  The user's .sno runs unmodified.
#      No inject_traces*.py step.  Per RULES.md "Sync-step monitor —
#      keyword catch-alls only, no source preprocessing".
#
#   2. NO shared names file.  Each participant writes its own names
#      sidecar (per-participant MONITOR_NAMES_OUT) at process exit;
#      the controller resolves name_id -> string per participant via
#      the 4-part spec NAME:READY:GO:NAMES.
#
#   3. Env-var driven.  Each runtime reads MONITOR_BIN=1 +
#      MONITOR_READY_PIPE + MONITOR_GO_PIPE + MONITOR_NAMES_OUT and
#      activates its own catch-all trace.  scrip additionally honors
#      SCRIP_TRACE=1 / SCRIP_FTRACE=1 for catch-all activation.
#
# Today (SN-26-auto-controller landed; oracle bridges still open),
# the SCRIP_ONLY=1 mode is the only mode that actually runs end-to-end.
# CSNOBOL4 and SPITBOL participants are gated on the oracle-side
# bridges that emit binary records via runtime patches (SN-26-csn-bridge,
# SN-26-spl-bridge); when those land, the default (3-way) mode will
# work without further harness changes.
#
# Usage:
#   bash test_monitor_3way_sync_step_auto.sh <file.sno>
#   SCRIP_ONLY=1 bash test_monitor_3way_sync_step_auto.sh <file.sno>
#
# Environment overrides:
#   SCRIP_ONLY=1          — only launch scrip; useful for validation
#                           while CSN/SPL bridges are pending.
#   MONITOR_TIMEOUT=N     — per-participant timeout (default 15).
#   STDIN_SRC=path        — file fed to each participant on stdin.
#                           Default: /dev/null, or <file>.input if it exists.
#
# Exit:
#   0 — all participants agreed
#   1 — divergence
#   2 — timeout / EOF on a participant / setup failure
#   3 — protocol error
#
# Requires (run once per session):
#   bash scripts/install_system_packages.sh
#   bash scripts/build_scrip.sh
# And for full 3-way (when bridges land):
#   bash scripts/build_csnobol4_oracle.sh   # with SN-26-csn-bridge applied
#   bash scripts/build_spitbol_oracle.sh    # with SN-26-spl-bridge applied
#
# AUTHORS: Lon Jones Cherryholmes · Claude Sonnet 4.6  DATE: 2026-04-26

set -uo pipefail

SNO=${1:?Usage: test_monitor_3way_sync_step_auto.sh <file.sno>}
HERE="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
MON_DIR="$HERE/monitor"

X64_DIR="${X64_DIR:-/home/claude/x64}"
SPITBOL="$X64_DIR/bin/sbl"
CSNOBOL4="/home/claude/csnobol4/snobol4"
SCRIP="${SCRIP:-$HERE/../scrip}"
INC="${INC:-/home/claude/corpus/programs/include}"

TIMEOUT="${MONITOR_TIMEOUT:-15}"
SCRIP_ONLY="${SCRIP_ONLY:-0}"

# ── Prerequisites ──────────────────────────────────────────────────────
[[ -f "$SNO" ]]                          || { echo "FAIL .sno not found: $SNO"; exit 2; }
[[ -x "$SCRIP" ]]                        || { echo "FAIL scrip not built: $SCRIP — run build_scrip.sh"; exit 2; }
[[ -f "$MON_DIR/monitor_sync_bin.py" ]]  || { echo "FAIL monitor_sync_bin.py missing"; exit 2; }

if [[ "$SCRIP_ONLY" != "1" ]]; then
    [[ -x "$CSNOBOL4" ]]                 || { echo "FAIL csnobol4 not built: $CSNOBOL4 (or set SCRIP_ONLY=1)"; exit 2; }
    [[ -x "$SPITBOL" ]]                  || { echo "FAIL spitbol not built: $SPITBOL (or set SCRIP_ONLY=1)"; exit 2; }
fi

TMP=$(mktemp -d /tmp/monitor_auto_XXXXXX)

base="$(basename "$SNO" .sno)"
STDIN_SRC="${STDIN_SRC:-/dev/null}"
[[ "$STDIN_SRC" = "/dev/null" && -f "${SNO%.sno}.input" ]] && STDIN_SRC="${SNO%.sno}.input"

echo "[auto] program:    $base"
echo "[auto] tmp:        $TMP"
echo "[auto] mode:       $([[ "$SCRIP_ONLY" = "1" ]] && echo SCRIP_ONLY || echo 3-way)"
echo "[auto] stdin:      $STDIN_SRC"

# ── Create per-participant FIFO pairs and names-out paths ──────────────
if [[ "$SCRIP_ONLY" = "1" ]]; then
    PARTICIPANTS=(scr)
else
    PARTICIPANTS=(csn spl scr)   # csn first → oracle
fi

for p in "${PARTICIPANTS[@]}"; do
    mkfifo "$TMP/$p.ready"
    mkfifo "$TMP/$p.go"
    : > "$TMP/$p.names"   # touch — the participant overwrites at exit
done

# ── Launch participants in background ──────────────────────────────────

PIDS=()

# CSNOBOL4 — participant 0 (oracle), if 3-way mode.
# NOTE: requires SN-26-csn-bridge to actually emit on the wire.  Without
# it, csnobol4 won't open the FIFO and the harness will block waiting.
if [[ "$SCRIP_ONLY" != "1" ]]; then
    MONITOR_BIN=1 \
    MONITOR_READY_PIPE="$TMP/csn.ready" \
    MONITOR_GO_PIPE="$TMP/csn.go" \
    MONITOR_NAMES_OUT="$TMP/csn.names" \
        timeout "$((TIMEOUT*2))" "$CSNOBOL4" -bf -P256k -S 64k -I"$INC" "$SNO" \
        < "$STDIN_SRC" > "$TMP/csn.out" 2> "$TMP/csn.err" &
    PIDS+=($!)

    # SPITBOL x64 — participant 1
    MONITOR_BIN=1 \
    MONITOR_READY_PIPE="$TMP/spl.ready" \
    MONITOR_GO_PIPE="$TMP/spl.go" \
    MONITOR_NAMES_OUT="$TMP/spl.names" \
    SETL4PATH=".:$INC" \
        timeout "$((TIMEOUT*2))" "$SPITBOL" -bf "$SNO" \
        < "$STDIN_SRC" > "$TMP/spl.out" 2> "$TMP/spl.err" &
    PIDS+=($!)
fi

# scrip --ir-run — always launched.  Catch-all activation via env vars
# only; no source modification, no LOAD-chain.
MONITOR_BIN=1 \
MONITOR_READY_PIPE="$TMP/scr.ready" \
MONITOR_GO_PIPE="$TMP/scr.go" \
MONITOR_NAMES_OUT="$TMP/scr.names" \
SCRIP_TRACE=1 \
SCRIP_FTRACE=1 \
SNO_LIB="$INC" \
    timeout "$((TIMEOUT*2))" "$SCRIP" --ir-run "$SNO" \
    < "$STDIN_SRC" > "$TMP/scr.out" 2> "$TMP/scr.err" &
PIDS+=($!)

# ── Launch controller using the new 4-part spec ────────────────────────
SPECS=()
for p in "${PARTICIPANTS[@]}"; do
    SPECS+=("$p:$TMP/$p.ready:$TMP/$p.go:$TMP/$p.names")
done

python3 "$MON_DIR/monitor_sync_bin.py" "${SPECS[@]}" > "$TMP/ctrl.out" 2>&1 &
CTRL_PID=$!

# ── Wait + reap ────────────────────────────────────────────────────────
wait "$CTRL_PID"
CTRL_RC=$?

# Make sure participants are reaped — SIGTERM in case they're still
# blocked on the (now-closed) go FIFO.
for pid in "${PIDS[@]}"; do kill "$pid" 2>/dev/null || true; done
wait 2>/dev/null || true

# ── Report ─────────────────────────────────────────────────────────────
echo
echo "── controller output ──"
cat "$TMP/ctrl.out"

for p in "${PARTICIPANTS[@]}"; do
    if [[ -s "$TMP/$p.out" ]]; then
        echo
        echo "── $p stdout (head) ──"
        head -20 "$TMP/$p.out"
    fi
    if [[ -s "$TMP/$p.err" ]]; then
        echo "── $p stderr (head) ──"
        head -20 "$TMP/$p.err"
    fi
    if [[ -s "$TMP/$p.names" ]]; then
        n=$(wc -l < "$TMP/$p.names")
        echo "── $p names ($n) ──"
        head -10 "$TMP/$p.names"
    fi
done

# Preserve artifacts.
rm -rf /tmp/monitor_auto_last
cp -a "$TMP" /tmp/monitor_auto_last
echo
echo "[auto] artefacts:  /tmp/monitor_auto_last/"
echo "[auto] exit:       $CTRL_RC"

exit "$CTRL_RC"
