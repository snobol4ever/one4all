#!/usr/bin/env bash
# test_smoke_snobol4_jvm.sh — SJ4-JVM-3 gate: full JVM pipeline on smoke programs
# scrip --sm-emit --target=jvm → .j file → jasmin.jar → .class → java → verify output
set -uo pipefail
HERE="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
SCRIP="${SCRIP:-$HERE/../scrip}"
JASMIN="${JASMIN:-$HERE/../src/backend/jasmin.jar}"
ORACLE="${ORACLE:-/home/claude/x64/bin/sbl}"
PASS=0; FAIL=0

run_smoke() {
    local label="$1"
    local source="$2"
    local expected="$3"
    local tmp=$(mktemp -d)
    local prog="$tmp/prog.sno"
    printf "%b" "$source" > "$prog"
    # Get oracle output
    local oracle_out=$(timeout 8 "$ORACLE" -b "$prog" 2>/dev/null || echo "")
    # Compile to JVM
    local j_file="$tmp/Prog.j"
    if ! timeout 8 "$SCRIP" --sm-emit --target=jvm "$prog" > "$j_file" 2>/dev/null; then
        echo "FAIL $label (emit)"
        FAIL=$((FAIL+1))
        rm -rf "$tmp"
        return
    fi
    # Assemble
    local class_dir="$tmp/classes"
    mkdir -p "$class_dir"
    if ! timeout 15 java -jar "$JASMIN" "$j_file" -d "$class_dir" > /dev/null 2>&1; then
        echo "FAIL $label (jasmin)"
        FAIL=$((FAIL+1))
        rm -rf "$tmp"
        return
    fi
    # Copy runtime classes
    mkdir -p "$class_dir/rt"
    java -jar "$JASMIN" "$HERE/../src/runtime/jvm/SnoRt.j" -d "$class_dir" > /dev/null 2>&1
    java -jar "$JASMIN" "$HERE/../src/runtime/jvm/SnoRtMatchState.j" -d "$class_dir" > /dev/null 2>&1
    # Run
    local jvm_out=$(timeout 5 java -cp "$class_dir" Prog 2>/dev/null || echo "")
    rm -rf "$tmp"
    if [ "$expected" = "$jvm_out" ]; then
        echo "PASS $label"
        PASS=$((PASS+1))
    else
        echo "FAIL $label (output mismatch)"
        echo "  expected: '$expected'"
        echo "  jvm:      '$jvm_out'"
        FAIL=$((FAIL+1))
    fi
}

# Run each smoke program with indentation (required by parser)
run_smoke "output" "\tOUTPUT = 'hello'\nEND" "hello"
run_smoke "concat" "\tOUTPUT = 'ab' 'cd'\nEND" "abcd"
run_smoke "arith" "\tOUTPUT = 3 + 4\nEND" "7"
run_smoke "pattern" "\t'abc' ? OUTPUT = &MATCHED\nEND" "abc"
run_smoke "goto_s" "\tX = 'a' ; OUTPUT = X ; (X = 'b') :S(end) ; OUTPUT = X\nend\nEND" "a"
run_smoke "define" "\tDEFINE('foo()'); OUTPUT = foo()\nfoo\t.\nEND" "."
run_smoke "arith_sm" "\tOUTPUT = 10 / 3\nEND" "3"

echo "---"
echo "PASS=$PASS FAIL=$FAIL"
[ "$FAIL" -eq 0 ]
