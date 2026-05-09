#!/usr/bin/env bash
# test_smoke_snocone_parse_j.sh — GOAL-SNOCONE-LANG-SPACE LS-4.l gate
#
# Builds and runs test_snocone_parse_j, which verifies the LS-4.l
# parser fixes:
#
#   1. Binary `.` and `$` (AST_CAPT_COND_ASGN / AST_CAPT_IMMED_ASGN)
#      added at expr12, between expr11 (exponent) and expr15
#      (subscript).  Mirrors snobol4.y:159-161.
#
#   2. sc_split_subject_pattern fires from sc_append_stmt,
#      sc_make_cond_fail_stmt, and sc_make_cond_succ_stmt — splits
#      AST_SCAN(subj, pat) and AST_SEQ(name, rest...) out of the
#      subject slot into separate s->subject / s->pattern slots so
#      the runtime's pattern-match engine fires.  Mirrors
#      snobol4.y:248-270.
#
# Together these close the 12 fence/match/semantic/trace beauty
# 3-mode FAILs (12/42 → 0/42 = full LS-4.l acceptance).
#
# AUTHORS: Lon Jones Cherryholmes · Claude Sonnet
# Commit identity: LCherryholmes / lcherryh@yahoo.com  (RULES.md)

set -euo pipefail

HERE="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
ONE4ALL="$(cd "$HERE/.." && pwd)"
TEST="$ONE4ALL/test/frontend/snocone/test_snocone_parse_j.c"
SRCDIR="$ONE4ALL/src/frontend/snocone"
BIN="/tmp/test_snocone_parse_j"

cc -Wall -o "$BIN" \
    "$TEST" \
    "$SRCDIR/snocone_parse.tab.c" \
    "$SRCDIR/snocone_lex.c" \
    -I "$SRCDIR" \
    -I "$ONE4ALL/src/frontend/snobol4" \
    -I "$ONE4ALL/src"

"$BIN"
