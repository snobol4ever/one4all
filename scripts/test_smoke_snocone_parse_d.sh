#!/usr/bin/env bash
# test_smoke_snocone_parse_d.sh — GOAL-SNOCONE-LANG-SPACE LS-4.d gate
#
# Builds and runs test_snocone_parse_d, which verifies that the new
# Bison-based Snocone parser handles the LS-4.d addition:
#   * postfix subscript `a[i, j]` → AST_IDX(a, i, j)  (n-ary)
#   * left-recursive chaining: `a[i][j]` → AST_IDX(AST_IDX(a, i), j)
#   * empty subscript: `a[]` → AST_IDX(a)  (uses the empty-list arm)
#   * string keys: `T['key']` → AST_IDX(T, QLIT(key))
#   * expression children: `a[1+2, 3*4]` → AST_IDX(a, ADD(1,2), MUL(3,4))
#   * call-then-subscript: `f(x)[i]` → AST_IDX(FNC(f,x), i)
#   * the precedence relation: subscript at expr15 binds tighter than
#     exponentiation at expr11, so `a[i] ^ 2` is `(a[i]) ^ 2`
#   * compound-assigns with subscript LHS: `a[i] += 1` → distinct
#     cloned subtree, no node sharing (no double-free at cleanup)
#
# Headline gate from goal file: parses `a[i, j]`.
#
# Side-channel test — the LS-4.d parser is not yet wired into scrip's
# production driver path (that happens at LS-4.j).  Until then, the
# new parser is exercised through this standalone test against the IR
# helpers and the FSM lexer.
#
# AUTHORS: Lon Jones Cherryholmes · Claude Sonnet
# Commit identity: LCherryholmes / lcherryh@yahoo.com  (RULES.md)

set -euo pipefail

HERE="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
ONE4ALL="$(cd "$HERE/.." && pwd)"
TEST="$ONE4ALL/test/frontend/snocone/test_snocone_parse_d.c"
SRCDIR="$ONE4ALL/src/frontend/snocone"
BIN="/tmp/test_snocone_parse_d"

cc -Wall -o "$BIN" \
    "$TEST" \
    "$SRCDIR/snocone_parse.tab.c" \
    "$SRCDIR/snocone_lex.c" \
    -I "$SRCDIR" \
    -I "$ONE4ALL/src/frontend/snobol4" \
    -I "$ONE4ALL/src"

"$BIN"
