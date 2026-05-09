#!/usr/bin/env bash
# test_smoke_snocone_parse_b.sh — GOAL-SNOCONE-LANG-SPACE LS-4.b gate
#
# Builds and runs test_snocone_parse_b, which verifies that the new
# Bison-based Snocone parser handles the LS-4.b additions:
#   * the 14 comparison/identity operators (== != < > <= >= and the
#     :==:/:!=:/:<:/:>:/:<=:/:>=: lexical family and ::/:!: identity
#     operators), each lowering to an AST_FNC named call;
#   * the T_FUNCTION call-form `EQ(2+2, 4)` lowering to
#     AST_FNC("EQ", AST_ADD(...), AST_ILIT(4));
#   * the precedence relation: comparisons sit BELOW arithmetic add/sub
#     so `a + b == c + d` parses as `EQ(a+b, c+d)`.
#
# Side-channel test — the LS-4.b parser is not yet wired into scrip's
# production driver path (that happens at LS-4.j).  Until then, the
# new parser is exercised through this standalone test against the IR
# helpers and the FSM lexer.
#
# AUTHORS: Lon Jones Cherryholmes · Claude Sonnet
# Commit identity: LCherryholmes / lcherryh@yahoo.com  (RULES.md)

set -euo pipefail

HERE="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
ONE4ALL="$(cd "$HERE/.." && pwd)"
TEST="$ONE4ALL/test/frontend/snocone/test_snocone_parse_b.c"
SRCDIR="$ONE4ALL/src/frontend/snocone"
BIN="/tmp/test_snocone_parse_b"

cc -Wall -o "$BIN" \
    "$TEST" \
    "$SRCDIR/snocone_parse.tab.c" \
    "$SRCDIR/snocone_lex.c" \
    -I "$SRCDIR" \
    -I "$ONE4ALL/src/frontend/snobol4" \
    -I "$ONE4ALL/src"

"$BIN"
