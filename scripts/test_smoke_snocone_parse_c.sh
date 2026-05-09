#!/usr/bin/env bash
# test_smoke_snocone_parse_c.sh — GOAL-SNOCONE-LANG-SPACE LS-4.c gate
#
# Builds and runs test_snocone_parse_c, which verifies that the new
# Bison-based Snocone parser handles the LS-4.c additions:
#   * synthetic concat T_CONCAT       → AST_SEQ (n-ary fold)
#   * pattern alternation `|`         → AST_ALT (n-ary fold)
#   * pattern match `?`               → AST_SCAN
#   * compound-assigns (+= -= *= /= ^=) lower to AST_ASSIGN over the
#     corresponding binop, with a clone of the LHS in the RHS subtree
#   * the precedence chain:  =  <  ?  <  |  <  concat  <  ==  <  +/-  <  */  <  ^
#     (each operator binds tighter than the one to its left)
#
# Headline gate from goal file: parses `s = 'hello' ' world'`.
#
# Side-channel test — the LS-4.c parser is not yet wired into scrip's
# production driver path (that happens at LS-4.j).
#
# AUTHORS: Lon Jones Cherryholmes · Claude Sonnet
# Commit identity: LCherryholmes / lcherryh@yahoo.com  (RULES.md)

set -euo pipefail

HERE="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
ONE4ALL="$(cd "$HERE/.." && pwd)"
TEST="$ONE4ALL/test/frontend/snocone/test_snocone_parse_c.c"
SRCDIR="$ONE4ALL/src/frontend/snocone"
BIN="/tmp/test_snocone_parse_c"

cc -Wall -o "$BIN" \
    "$TEST" \
    "$SRCDIR/snocone_parse.tab.c" \
    "$SRCDIR/snocone_lex.c" \
    -I "$SRCDIR" \
    -I "$ONE4ALL/src/frontend/snobol4" \
    -I "$ONE4ALL/src"

"$BIN"
