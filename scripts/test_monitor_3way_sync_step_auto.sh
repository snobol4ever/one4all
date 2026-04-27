#!/usr/bin/env bash
# test_monitor_3way_sync_step_auto.sh — auto-mode binary sync-step monitor.
#
# Canonical sync-step harness for the SNOBOL4 frontend ladder.  Drives
# any subset of {csn, spl, scr} participants — see PARTICIPANTS env var.
# Replaces the entire pre-SN-26-harness-rewrite family of harnesses
# (test_monitor_3way_sync_step_bin.sh, test_monitor_5way_ipc.sh,
# test_monitor_sync_step.sh, test_monitor_3way.sh) which all required
# inject_traces*.py source preprocessing.  Per RULES.md "Sync-step
# monitor — keyword catch-alls only, no source preprocessing":
#
#   1. NO source preprocessing.  The user's .sno runs unmodified.
#      No inject_traces*.py step.
#
#   2. NO sidecar names file.  Per SN-26-bridge-coverage-e (streaming
#      intern), each participant emits MWK_NAME_DEF records inline on
#      the wire as new names are interned.  The controller builds a
#      per-participant intern table from those wire records.  Spec is
#      now NAME:READY:GO (3-part, no names path).
#
#   3. Env-var driven.  Each runtime reads MONITOR_BIN=1 (scrip only) +
#      MONITOR_READY_PIPE + MONITOR_GO_PIPE and activates its own
#      catch-all trace.  scrip additionally honors SCRIP_TRACE=1 /
#      SCRIP_FTRACE=1 for catch-all activation.
#
# CSNOBOL4 and SPITBOL participants require the SN-26-csn-bridge and
# SN-26-spl-bridge runtime patches to fire on the wire — those landed
# in csnobol4 (session #26) and x64 (session #27).  Both are silently
# no-op when MONITOR_READY_PIPE is unset.
#
# Usage:
#   bash test_monitor_3way_sync_step_auto.sh <file.sno>
#   SCRIP_ONLY=1 bash test_monitor_3way_sync_step_auto.sh <file.sno>
#   PARTICIPANTS="csn spl"     bash test_monitor_3way_sync_step_auto.sh <file.sno>
#   PARTICIPANTS="spl scr"     bash test_monitor_3way_sync_step_auto.sh <file.sno>
#   PARTICIPANTS="csn spl scr" bash test_monitor_3way_sync_step_auto.sh <file.sno>   # default
#   PARTICIPANTS="csn spl dot" bash test_monitor_3way_sync_step_auto.sh <file.sno>   # GOAL-NET-BEAUTY-SELF
#
# Environment overrides:
#   PARTICIPANTS="..."    — space-separated list from {csn, spl, scr, dot} in
#                           desired order.  First entry is the oracle
#                           the controller compares against.
#                           Default: "csn spl scr" (3-way).
#   SCRIP_ONLY=1          — alias for PARTICIPANTS="scr" (back-compat).
#   MONITOR_TIMEOUT=N     — per-participant timeout (default 15).
#   STDIN_SRC=path        — file fed to each participant on stdin.
#                           Default: /dev/null, or <file>.input if it exists.
#   SNO4_DLL=path         — Snobol4.dll location (default $SNO4_REPO/Snobol4/bin/Release/net10.0/Snobol4.dll).
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
SNO4_REPO="${SNO4_REPO:-/home/claude/snobol4dotnet}"
SNO4_DLL="${SNO4_DLL:-$SNO4_REPO/Snobol4/bin/Release/net10.0/Snobol4.dll}"
INC="${INC:-/home/claude/corpus/programs/include}"

TIMEOUT="${MONITOR_TIMEOUT:-15}"
SCRIP_ONLY="${SCRIP_ONLY:-0}"

# PARTICIPANTS env var (preferred):  e.g. "csn spl scr", "csn spl", "spl scr".
# SCRIP_ONLY=1 is back-compat alias for PARTICIPANTS="scr".
if [[ -n "${PARTICIPANTS:-}" ]]; then
    read -r -a PARTICIPANTS <<< "$PARTICIPANTS"
elif [[ "$SCRIP_ONLY" = "1" ]]; then
    PARTICIPANTS=(scr)
else
    PARTICIPANTS=(csn spl scr)   # default 3-way; first entry is oracle
fi

# Validate participant names.
for p in "${PARTICIPANTS[@]}"; do
    case "$p" in
        csn|spl|scr|dot) ;;
        *) echo "FAIL unknown participant '$p' (allowed: csn, spl, scr, dot)"; exit 2 ;;
    esac
done

want_csn=0; want_spl=0; want_scr=0; want_dot=0
for p in "${PARTICIPANTS[@]}"; do
    case "$p" in
        csn) want_csn=1 ;;
        spl) want_spl=1 ;;
        scr) want_scr=1 ;;
        dot) want_dot=1 ;;
    esac
done

# ── Prerequisites ──────────────────────────────────────────────────────
[[ -f "$SNO" ]]                          || { echo "FAIL .sno not found: $SNO"; exit 2; }
[[ -f "$MON_DIR/monitor_sync_bin.py" ]]  || { echo "FAIL monitor_sync_bin.py missing"; exit 2; }
[[ "$want_scr" = "1" ]] && [[ ! -x "$SCRIP" ]]    && { echo "FAIL scrip not built: $SCRIP — run build_scrip.sh"; exit 2; }
[[ "$want_csn" = "1" ]] && [[ ! -x "$CSNOBOL4" ]] && { echo "FAIL csnobol4 not built: $CSNOBOL4"; exit 2; }
[[ "$want_spl" = "1" ]] && [[ ! -x "$SPITBOL" ]]  && { echo "FAIL spitbol not built: $SPITBOL"; exit 2; }
[[ "$want_dot" = "1" ]] && [[ ! -f "$SNO4_DLL" ]] && { echo "FAIL snobol4dotnet not built: $SNO4_DLL — dotnet build Snobol4/Snobol4.csproj -c Release -p:EnableWindowsTargeting=true"; exit 2; }
[[ "$want_dot" = "1" ]] && ! command -v dotnet >/dev/null 2>&1 && { echo "FAIL dotnet command missing — apt-get install -y dotnet-sdk-10.0"; exit 2; }
:

TMP=$(mktemp -d /tmp/monitor_auto_XXXXXX)

base="$(basename "$SNO" .sno)"
STDIN_SRC="${STDIN_SRC:-/dev/null}"
[[ "$STDIN_SRC" = "/dev/null" && -f "${SNO%.sno}.input" ]] && STDIN_SRC="${SNO%.sno}.input"

echo "[auto] program:    $base"
echo "[auto] tmp:        $TMP"
echo "[auto] mode:       ${PARTICIPANTS[*]}"
echo "[auto] stdin:      $STDIN_SRC"

# ── Create per-participant FIFO pairs and names-out paths ──────────────
for p in "${PARTICIPANTS[@]}"; do
    mkfifo "$TMP/$p.ready"
    mkfifo "$TMP/$p.go"
    : > "$TMP/$p.names"   # touch — the participant overwrites at exit
done

# ── Launch participants in background ──────────────────────────────────

PIDS=()

# CSNOBOL4 — oracle when present.
# Requires SN-26-csn-bridge applied to v311.sil (already in current
# csnobol4 HEAD as of session #25/#26).  The bridge is silently no-op
# when MONITOR_READY_PIPE is unset.
if [[ "$want_csn" = "1" ]]; then
    MONITOR_BIN=1 \
    MONITOR_READY_PIPE="$TMP/csn.ready" \
    MONITOR_GO_PIPE="$TMP/csn.go" \
    MONITOR_NAMES_OUT="$TMP/csn.names" \
        timeout "$((TIMEOUT*2))" "$CSNOBOL4" -bf -P256k -S 64k -I"$INC" "$SNO" \
        < "$STDIN_SRC" > "$TMP/csn.out" 2> "$TMP/csn.err" &
    PIDS+=($!)
fi

# SPITBOL x64 — secondary oracle.
# Requires SN-26-spl-bridge applied to sbl.min (already in current x64
# HEAD as of session #27).  Silently no-op when env vars unset.
if [[ "$want_spl" = "1" ]]; then
    MONITOR_BIN=1 \
    MONITOR_READY_PIPE="$TMP/spl.ready" \
    MONITOR_GO_PIPE="$TMP/spl.go" \
    MONITOR_NAMES_OUT="$TMP/spl.names" \
    SETL4PATH=".:$INC" \
        timeout "$((TIMEOUT*2))" "$SPITBOL" -bf "$SNO" \
        < "$STDIN_SRC" > "$TMP/spl.out" 2> "$TMP/spl.err" &
    PIDS+=($!)
fi

# scrip --ir-run — catch-all activated via SCRIP_TRACE/SCRIP_FTRACE only;
# no source modification, no LOAD-chain.
if [[ "$want_scr" = "1" ]]; then
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
fi

# snobol4dotnet — runtime under test for GOAL-NET-BEAUTY-SELF.
# MonitorIpc.cs reads MONITOR_READY_PIPE / MONITOR_GO_PIPE / MONITOR_NAMES_OUT
# at first emit; silently no-op if any of those env vars is unset (S-2-bridge-1
# dormancy guarantee).  Fire-points landed in S-2-bridge-2/3/4:
#   - Executive.Assign chokepoint  → VALUE on every lvalue store
#   - ExecuteProgramDefinedFunction → CALL/EmitValue/RETURN at fn entry/exit
# Run with -bf for case-sensitive identifiers (matches csn/spl invocation).
if [[ "$want_dot" = "1" ]]; then
    MONITOR_BIN=1 \
    MONITOR_READY_PIPE="$TMP/dot.ready" \
    MONITOR_GO_PIPE="$TMP/dot.go" \
    MONITOR_NAMES_OUT="$TMP/dot.names" \
        timeout "$((TIMEOUT*2))" dotnet "$SNO4_DLL" -bf "$SNO" \
        < "$STDIN_SRC" > "$TMP/dot.out" 2> "$TMP/dot.err" &
    PIDS+=($!)
fi

# ── Launch controller using the SN-26-bridge-coverage-e 3-part spec ────
# Names live on the wire (MWK_NAME_DEF records); no sidecar names path
# in the spec.  The MONITOR_NAMES_OUT env vars set above are now ignored
# by csn/spl/scr runtimes (per SN-26-e); they're left in the participant
# launches as harmless legacy env so other runtimes (e.g. snobol4dotnet
# 'dot' lane) that still honor them continue to work until their own -e
# patches land.
SPECS=()
for p in "${PARTICIPANTS[@]}"; do
    SPECS+=("$p:$TMP/$p.ready:$TMP/$p.go")
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
