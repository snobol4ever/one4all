#!/bin/bash
# S-1 unit test: standalone parse_expr_pat_from_str against bison/lex TUs
set -e
SRC=/home/claude/one4all/src/frontend/snobol4
ROOT=/home/claude/one4all/src
gcc -O0 -g -Wall -Wno-unused-function -Wno-unused-variable \
    -Wno-incompatible-pointer-types \
    -I"$SRC" -I"$ROOT" \
    -DUNIT_TEST_PARSE_EXPR \
    "$SRC/snobol4.tab.c" "$SRC/snobol4.lex.c" \
    /tmp/test_parse_expr_pat.c \
    -o /tmp/test_parse_expr_pat
echo "Build OK"
