#!/usr/bin/env bash
# test_smoke_snobol4_jvm_sj4jvm1.sh — SJ4-JVM-1 gate: verify all 19 BB templates emit valid Jasmin
# For each IR_PAT_* node kind, emit a hand-crafted minimal .sno program that exercises that box,
# then assemble the resulting .j file with jasmin.jar (assembly success = template is valid Jasmin).
set -uo pipefail
HERE="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
SCRIP="${SCRIP:-$HERE/../scrip}"
JASMIN="${JASMIN:-$HERE/../src/backend/jasmin.jar}"
PASS=0; FAIL=0
check_jasmin() {
    local name="$1" sno="$2"
    local tmp=$(mktemp -d)
    echo "$sno" > "$tmp/t.sno"
    timeout 8 "$SCRIP" --sm-emit --target=jvm "$tmp/t.sno" > "$tmp/t.j" 2>/dev/null < /dev/null
    if timeout 10 java -jar "$JASMIN" "$tmp/t.j" -d "$tmp/" > "$tmp/jasmin.out" 2>&1; then
        echo "PASS $name"
        PASS=$((PASS+1))
    else
        echo "FAIL $name"
        cat "$tmp/jasmin.out" | head -5
        FAIL=$((FAIL+1))
    fi
    rm -rf "$tmp"
}
# One .sno per box kind — minimal program that references that pattern function
check_jasmin LIT        "OUTPUT = 'hello' 'world'"
check_jasmin ANY        "OUTPUT = ANY('abc')"
check_jasmin NOTANY     "OUTPUT = NOTANY('xyz')"
check_jasmin SPAN       "OUTPUT = SPAN('abc')"
check_jasmin BREAK      "OUTPUT = BREAK('xyz')"
check_jasmin ARB        "OUTPUT = ARB"
check_jasmin ARBNO      "OUTPUT = ARBNO(LEN(1))"
check_jasmin CAT        "OUTPUT = LEN(1) LEN(2)"
check_jasmin ALT        "OUTPUT = LEN(1) | LEN(2)"
check_jasmin LEN        "OUTPUT = LEN(3)"
check_jasmin POS        "OUTPUT = POS(0)"
check_jasmin RPOS       "OUTPUT = RPOS(0)"
check_jasmin TAB        "OUTPUT = TAB(3)"
check_jasmin RTAB       "OUTPUT = RTAB(0)"
check_jasmin REM        "OUTPUT = REM"
check_jasmin FENCE      "OUTPUT = FENCE"
check_jasmin ABORT      "OUTPUT = ABORT"
check_jasmin ASSIGN_IMM "X = '' ; OUTPUT = LEN(1) $ X"
check_jasmin ASSIGN_COND "X = '' ; OUTPUT = LEN(1) . X"
echo "---"
echo "PASS=$PASS FAIL=$FAIL"
[ "$FAIL" -eq 0 ]
