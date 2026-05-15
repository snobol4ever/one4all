#!/usr/bin/env bash
# test_smoke_snobol4_jvm.sh — SJ4-JVM-3 gate: full JVM pipeline on 7 smoke programs
# scrip --sm-emit --target=jvm → .j file → jasmin.jar → .class → java → verify output
set -uo pipefail
HERE="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
SCRIP="${SCRIP:-$HERE/../scrip}"
JASMIN="${JASMIN:-$HERE/../src/backend/jasmin.jar}"
ORACLE="${ORACLE:-/home/claude/x64/bin/sbl}"
PASS=0; FAIL=0
SMOKE_DIR="$HERE/../test/smoke"
run_smoke() {
    local name="$1"
    local sno="$2"
    local tmp=$(mktemp -d)
    local prog="$tmp/$name.sno"
    echo "$sno" > "$prog"
    # Get oracle output
    local oracle_out=$(timeout 8 "$ORACLE" -b "$prog" 2>/dev/null)
    # Compile to JVM
    local j_file="$tmp/$name.j"
    if ! timeout 8 "$SCRIP" --sm-emit --target=jvm "$prog" > "$j_file" 2>/dev/null; then
        echo "FAIL $name (emit)"
        FAIL=$((FAIL+1))
        rm -rf "$tmp"
        return
    fi
    # Assemble
    local class_dir="$tmp/classes"
    mkdir -p "$class_dir"
    if ! timeout 15 java -jar "$JASMIN" "$j_file" -d "$class_dir" > /dev/null 2>&1; then
        echo "FAIL $name (jasmin)"
        FAIL=$((FAIL+1))
        rm -rf "$tmp"
        return
    fi
    # Copy runtime classes
    mkdir -p "$class_dir/rt"
    java -jar "$JASMIN" "$HERE/../src/runtime/jvm/SnoRt.j" -d "$class_dir" > /dev/null 2>&1
    java -jar "$JASMIN" "$HERE/../src/runtime/jvm/SnoRtMatchState.j" -d "$class_dir" > /dev/null 2>&1
    # Run
    local prog_name=$(basename "$j_file" .j)
    local prog_class="${prog_name^}" # capitalize first letter
    local jvm_out=$(timeout 5 java -cp "$class_dir" "$prog_class" 2>/dev/null || echo "")
    rm -rf "$tmp"
    if [ "$oracle_out" = "$jvm_out" ]; then
        echo "PASS $name"
        PASS=$((PASS+1))
    else
        echo "FAIL $name (output mismatch)"
        echo "  oracle: '$oracle_out'"
        echo "  jvm:    '$jvm_out'"
        FAIL=$((FAIL+1))
    fi
}
# Run each smoke program
run_smoke "null" "END"
run_smoke "lit_hello" "OUTPUT = 'hello'"
run_smoke "pos0" "OUTPUT = POS(0)"
run_smoke "rpos0" "OUTPUT = RPOS(0)"
run_smoke "arith_sm" "OUTPUT = 3 + 4"
run_smoke "define" "DEFINE('foo()'); OUTPUT = foo()"
run_smoke "goto_s" "X = 'a' ; OUTPUT = X ; (X = 'b') :S(end) ; OUTPUT = X ; end"
echo "---"
echo "PASS=$PASS FAIL=$FAIL"
[ "$FAIL" -eq 0 ]
