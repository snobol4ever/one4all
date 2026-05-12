#!/usr/bin/env bash
# run_scrip_parser.sh — run a SCRIP parser_<lang>.sc driver against a source file.
# Load order mirrors beauty.sno -INCLUDE chain exactly (beauty.sno is the source of truth).
# Usage: run_scrip_parser.sh <lang> [source_file]
#   lang: snobol4 | snocone | icon | prolog | raku | rebus
#   source_file: path to source file; omit to read stdin
# AUTHORS: Lon Jones Cherryholmes · Claude Sonnet 4.6  DATE: 2026-05-12
HERE="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
SCRIP="${SCRIP:-$HERE/../scrip}"
SD="${SD:-/home/claude/corpus/SCRIP}"

LANG="${1:-snobol4}"
SRC="${2:-}"

if [ ! -f "$SCRIP" ]; then echo "SKIP scrip not found: $SCRIP"; exit 0; fi
if [ ! -d "$SD" ];   then echo "SKIP corpus/SCRIP not found: $SD"; exit 0; fi

# Shared runtime — beauty.sno INCLUDE order:
#   global, case, assign, match, counter, stack, tree, ShiftReduce,
#   TDump, Gen, Qize, semantic, omega, trace
# NOTE: qize.sc omitted — loading it breaks Append-through-function-parameter
#   (pre-existing scrip Snocone interpreter bug, tracked in SL-2 notes).
#   Three Error 5 "Undefined SQize" appear at load time for shift()-based label
#   patterns; those patterns are non-fatal and the parser runs correctly for
#   non-label inputs.  Re-add qize.sc here once that bug is fixed.
RUNTIME=(
    "$SD/global.sc"
    "$SD/case.sc"
    "$SD/assign.sc"
    "$SD/match.sc"
    "$SD/counter.sc"
    "$SD/stack.sc"
    "$SD/tree.sc"
    "$SD/ShiftReduce.sc"
    "$SD/tdump.sc"
    "$SD/gen.sc"
    "$SD/qize.sc"
    "$SD/semantic.sc"
    "$SD/omega.sc"
    "$SD/trace.sc"
)

# SL-13: lower pipeline (parser_snobol4.sc calls Lower_collect + Lower_run)
LOWER=(
    "$SD/lower.sc"
    "$SD/lower_driver.sc"
)

DRIVER="$SD/parser_${LANG}.sc"
if [ ! -f "$DRIVER" ]; then echo "SKIP no driver: $DRIVER"; exit 0; fi

if [ -n "$SRC" ]; then
    timeout 30 "$SCRIP" --ir-run "${RUNTIME[@]}" "${LOWER[@]}" "$DRIVER" < "$SRC"
else
    timeout 30 "$SCRIP" --ir-run "${RUNTIME[@]}" "${LOWER[@]}" "$DRIVER"
fi
