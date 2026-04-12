#!/usr/bin/env bash
# test/smoke.sh — scrip smoke test
# Does it build and run? Three programs, fast, loud on failure.
# Usage: bash test/smoke.sh
# From:  /home/claude/one4all/

set -uo pipefail
INTERP="${INTERP:-./scrip}"
PASS=0; FAIL=0

check() {
    local label="$1" expected="$2" got="$3"
    if [ "$got" = "$expected" ]; then
        echo "PASS $label"
        PASS=$((PASS+1))
    else
        echo "FAIL $label"
        echo "  expected: $(printf '%s' "$expected" | head -1)"
        echo "  got:      $(printf '%s' "$got" | head -1)"
        FAIL=$((FAIL+1))
    fi
}

check "hello" \
    "Hello, World!" \
    "$(printf "        OUTPUT = 'Hello, World!'\nEND\n" | $INTERP /dev/stdin 2>/dev/null)"

check "arithmetic" \
    "7" \
    "$(printf "        OUTPUT = 3 + 4\nEND\n" | $INTERP /dev/stdin 2>/dev/null)"

check "pattern" \
    "matched" \
    "$(printf "        X = 'hello'\n        X 'hello'   :S(YES)F(NO)\nYES     OUTPUT = 'matched'  :(END)\nNO      OUTPUT = 'failed'\nEND\n" | $INTERP /dev/stdin 2>/dev/null)"

echo ""
echo "PASS=$PASS FAIL=$FAIL"
[ $FAIL -eq 0 ]
