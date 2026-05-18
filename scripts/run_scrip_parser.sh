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
# SL-2 (2026-05-17): qize.sc is re-included.  The "Append-through-function-parameter"
# theory recorded earlier was a misdiagnosis — the actual bug was a use-after-free
# in snocone_parse.y's sc_finalize_if_else_pst, triggered by any function with
# three or more chained `else if` clauses (qize.sc's Qize fits that shape).
# Fixed by giving each `if_head` its own heap-allocated IfHead snapshot instead of
# sharing a single ScParseState slot that nested ifs clobbered.
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

# PST-RB-5e (2026-05-17): lower.sc and lower_driver.sc were deleted from corpus/SCRIP
# and will be re-translated from lower.c when that file stabilizes.  The PST-era
# parsers (snobol4, snocone, icon, prolog, raku, rebus) now emit pure AST via TDump
# and need no SCRIP-side lowering pass.
LOWER=()

DRIVER="$SD/parser_${LANG}.sc"
if [ ! -f "$DRIVER" ]; then echo "SKIP no driver: $DRIVER"; exit 0; fi

# Load language-specific helpers (defined outside parser file per PST rules)
HELPERS=()
if [ -f "$SD/${LANG}_helpers.sc" ]; then
    HELPERS=("$SD/${LANG}_helpers.sc")
fi

if [ -n "$SRC" ]; then
    timeout 30 "$SCRIP" --interp "${RUNTIME[@]}" "${LOWER[@]}" "${HELPERS[@]}" "$DRIVER" < "$SRC"
else
    timeout 30 "$SCRIP" --interp "${RUNTIME[@]}" "${LOWER[@]}" "${HELPERS[@]}" "$DRIVER"
fi
