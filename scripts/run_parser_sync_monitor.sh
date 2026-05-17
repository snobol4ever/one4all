#!/usr/bin/env bash
# run_parser_sync_monitor.sh — GOAL-PARSER-SC-TRANSPILE.md SCT-7.
#
# Transpiles a parser_<lang>.sc to portable SNOBOL4 via `scrip --dump-sno`,
# then drives the existing 2-way IPC sync-step monitor with the resulting
# .sno file as input.  Both SPITBOL x64 and SCRIP --sm-run execute the
# SAME transpiled .sno; the monitor reports the first divergence.
#
# The transpiler is the new piece; the monitor harness was built earlier
# (see scripts/test_monitor_2way_spitbol_vs_sm.sh and friends).  This
# script is the glue that ties them together for the parser-transpile
# rung sequence.
#
# Usage:
#   bash scripts/run_parser_sync_monitor.sh <lang> <sample-input>
#
# Examples:
#   bash scripts/run_parser_sync_monitor.sh snobol4  corpus/programs/snobol4/parser/atom_id.sno
#   bash scripts/run_parser_sync_monitor.sh rebus    corpus/programs/rebus/parser/paren.reb
#   bash scripts/run_parser_sync_monitor.sh snocone  corpus/programs/snocone/corpus/sc1_literals.sc
#   bash scripts/run_parser_sync_monitor.sh icon     corpus/programs/icon/parser/fail_stmt.icn
#   bash scripts/run_parser_sync_monitor.sh raku     corpus/programs/raku/parser/str_chars.raku
#   bash scripts/run_parser_sync_monitor.sh prolog   corpus/programs/prolog/rung01_hello_hello.pl
#
# Exit codes:
#   0 — both runtimes agreed throughout
#   1 — divergence detected (monitor prints last-agree + first-disagree)
#   2 — transpile failed (the .sc → .sno step itself failed)
#   3 — usage error
set -uo pipefail

LANG=${1:?Usage: run_parser_sync_monitor.sh <lang> <sample-input>}
SAMPLE=${2:?Usage: run_parser_sync_monitor.sh <lang> <sample-input>}

HERE="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
REPO_ROOT="$(cd "$HERE/.." && pwd)"

PARSER_SC="$REPO_ROOT/../corpus/SCRIP/parser_${LANG}.sc"
if [[ ! -f "$PARSER_SC" ]]; then
    # Try alternative layout
    PARSER_SC="${CORPUS_ROOT:-$REPO_ROOT/../corpus}/SCRIP/parser_${LANG}.sc"
fi
if [[ ! -f "$PARSER_SC" ]]; then
    echo "run_parser_sync_monitor: cannot find parser_${LANG}.sc" >&2
    exit 3
fi
if [[ ! -f "$SAMPLE" ]]; then
    echo "run_parser_sync_monitor: cannot find sample input '$SAMPLE'" >&2
    exit 3
fi

SCRIP="${SCRIP:-$REPO_ROOT/scrip}"
if [[ ! -x "$SCRIP" ]]; then
    echo "run_parser_sync_monitor: scrip not built at $SCRIP" >&2
    exit 2
fi

SNO_OUT="/tmp/parser_${LANG}.sno"
echo "[run_parser_sync_monitor] transpiling $PARSER_SC -> $SNO_OUT"
if ! "$SCRIP" --dump-sno "$PARSER_SC" > "$SNO_OUT"; then
    echo "run_parser_sync_monitor: --dump-sno failed for $PARSER_SC" >&2
    exit 2
fi

# Sanity: did we emit any TT_xxx placeholders? Warn but don't abort —
# the monitor will tell us if those tags actually affect execution.
PLACEHOLDERS=$(grep -c "?TT_\|BOTH-QUOTES" "$SNO_OUT" || true)
if [[ "$PLACEHOLDERS" -gt 0 ]]; then
    echo "[run_parser_sync_monitor] WARNING: $PLACEHOLDERS unhandled-tag placeholder(s) in transpiled output"
    grep -n "?TT_\|BOTH-QUOTES" "$SNO_OUT" | head -5
fi

# The actual sync-step is delegated to the existing harness (which knows
# how to launch SPITBOL and SCRIP --sm-run with the IPC monitor wired up).
# That script takes the .sno as its argument and reads SAMPLE from stdin.
echo "[run_parser_sync_monitor] driving 2-way monitor: SPITBOL vs SCRIP --sm-run"
echo "[run_parser_sync_monitor] sample: $SAMPLE"
exec bash "$HERE/test_monitor_2way_spitbol_vs_sm.sh" "$SNO_OUT" < "$SAMPLE"
