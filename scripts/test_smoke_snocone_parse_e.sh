#!/usr/bin/env bash
# test_smoke_snocone_parse_e.sh — GOAL-SNOCONE-LANG-SPACE LS-4.e gate
#
# Builds and runs test_snocone_parse_e, which verifies that the new
# Bison-based Snocone parser handles the LS-4.e addition:
#   * *expr  → AST_DEFER        (deferred evaluation)
#   * .expr  → AST_NAME         (name reference)
#   * $expr  → AST_INDIRECT     (variable indirection)
#   * @expr  → AST_CAPT_CURSOR  (cursor position capture)
#   * ~expr  → AST_NOT          (negate success/failure)
#   * ?expr  → AST_INTERROGATE  (interrogation)
#   * &expr  → AST_OPSYN("&")   (bare amp — OPSYN slot)
#   * %expr /expr #expr |expr =expr  → AST_OPSYN(op)  (OPSYN slots)
#   * Chains: ~.x → AST_NOT(AST_NAME(x))
#   * Unary binds tighter than binary: a + *b → AST_ADD(a, AST_DEFER(b))
#
# Side-channel test — the LS-4.e parser is not yet wired into scrip's
# production driver path (that happens at LS-4.j).
#
# AUTHORS: Lon Jones Cherryholmes · Claude Sonnet
# Commit identity: LCherryholmes / lcherryh@yahoo.com  (RULES.md)

set -euo pipefail

HERE="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
ONE4ALL="$(cd "$HERE/.." && pwd)"
TEST="$ONE4ALL/test/frontend/snocone/test_snocone_parse_e.c"
SRCDIR="$ONE4ALL/src/frontend/snocone"
BIN="/tmp/test_snocone_parse_e"

cc -Wall -o "$BIN" \
    "$TEST" \
    "$SRCDIR/snocone_parse.tab.c" \
    "$SRCDIR/snocone_lex.c" \
    -I "$SRCDIR" \
    -I "$ONE4ALL/src/frontend/snobol4" \
    -I "$ONE4ALL/src"

"$BIN"
