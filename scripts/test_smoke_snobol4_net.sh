#!/usr/bin/env bash
# scripts/test_smoke_snobol4_net.sh — SN4-NET-4 gate: full .NET pipeline on 7 smoke programs
# scrip --sm-emit --target=net -> .il -> ilasm + SnoRt.il -> .exe -> mono -> verify output
set -uo pipefail
HERE="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
SCRIP="${SCRIP:-$HERE/../scrip}"
ORACLE="${ORACLE:-/home/claude/x64/bin/sbl}"
SNORT_IL="$HERE/../src/runtime/net/SnoRt.il"
PASS=0; FAIL=0
run_smoke() {
    local name="$1"
    local sno="$2"
    local tmp
    tmp=$(mktemp -d)
    local prog="$tmp/$name.sno"
    printf '%s\n' "$sno" > "$prog"
    local oracle_out
    oracle_out=$(timeout 8 "$ORACLE" -b "$prog" < /dev/null 2>/dev/null || true)
    local il_file="$tmp/$name.il"
    if ! timeout 8 "$SCRIP" --sm-emit --target=net "$prog" < /dev/null > "$il_file" 2>/dev/null; then
        echo "FAIL $name (emit)"
        FAIL=$((FAIL+1))
        rm -rf "$tmp"
        return
    fi
    local exe_file="$tmp/$name.exe"
    if ! timeout 15 ilasm /output:"$exe_file" "$il_file" "$SNORT_IL" > /dev/null 2>&1; then
        echo "FAIL $name (ilasm)"
        FAIL=$((FAIL+1))
        rm -rf "$tmp"
        return
    fi
    local net_out
    net_out=$(timeout 8 mono "$exe_file" < /dev/null 2>/dev/null || true)
    rm -rf "$tmp"
    if [ "$oracle_out" = "$net_out" ]; then
        echo "PASS $name"
        PASS=$((PASS+1))
    else
        echo "FAIL $name (output mismatch)"
        echo "  oracle: $(printf '%q' "$oracle_out")"
        echo "  net:    $(printf '%q' "$net_out")"
        FAIL=$((FAIL+1))
    fi
}
run_smoke "null"      "END"
run_smoke "lit_hello" "               OUTPUT = 'hello'"$'\nEND'
run_smoke "concat"    "               OUTPUT = 'foo' 'bar'"$'\nEND'
run_smoke "arith_add" "               OUTPUT = 3 + 4"$'\nEND'
run_smoke "arith_sub" "               OUTPUT = 10 - 3"$'\nEND'
run_smoke "arith_mul" "               OUTPUT = 6 * 7"$'\nEND'
run_smoke "arith_div" "               OUTPUT = 20 / 4"$'\nEND'
echo "---"
echo "PASS=$PASS FAIL=$FAIL"
[ "$FAIL" -eq 0 ]
