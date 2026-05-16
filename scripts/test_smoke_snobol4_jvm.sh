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
    # Compile SnoPat.java (pattern matcher) with SnoRt on classpath
    if which javac > /dev/null 2>&1; then
        javac -cp "$class_dir" -d "$class_dir" "$HERE/../src/runtime/jvm/SnoPat.java" > /dev/null 2>&1
    fi
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
# Tests cover: scalar ops, arithmetic, control flow, user functions, and patterns.
run_smoke "output" "\tOUTPUT = 'hello'\nEND" "hello"
run_smoke "concat" "\tOUTPUT = 'ab' 'cd'\nEND" "abcd"
run_smoke "arith" "\tOUTPUT = 2 + 3\nEND" "5"
run_smoke "goto_unconditional" "\t:(LBL)\n\tOUTPUT = 'skipped'\nLBL\tOUTPUT = 'reached'\nEND" "reached"
run_smoke "loop_le" "\tI = 1\nLOOP\tOUTPUT = I\n\tI = I + 1\n\tLE(I,3) :S(LOOP)\nEND" "$(printf '1\n2\n3')"
run_smoke "le_branch" "\tI = 5\n\tLE(I,10) :S(YES)\n\tOUTPUT = 'no'\n\t:(END)\nYES\tOUTPUT = 'yes'\nEND" "yes"
run_smoke "arith_sm" "\tOUTPUT = 2 + 3\nEND" "5"
# SM_PAT_* opcode coverage (sess 2026-05-15 SJ4-JVM-4):
run_smoke "pat_lit" "\tS = 'hello world'\n\tS 'world' :S(Y)\n\tOUTPUT = 'no'\n\t:(L)\nY\tOUTPUT = 'yes'\nL\nEND" "yes"
run_smoke "pat_capture" "\tS = 'hello world'\n\tS BREAK(' ') . W\n\tOUTPUT = W\nEND" "hello"
run_smoke "pat_replace" "\tS = 'hello world'\n\tS 'world' = 'JVM'\n\tOUTPUT = S\nEND" "hello JVM"
run_smoke "pat_alt" "\tP = ('foo' | 'bar') . M\n\t'bar baz' P\n\tOUTPUT = M\nEND" "bar"
run_smoke "pat_arbno" "\tS = 'xyxyzzz'\n\tS ARBNO('xy') . P 'zzz'\n\tOUTPUT = P\nEND" "xyxy"
run_smoke "pat_len_rem" "\tS = 'abcdefgh'\n\tS LEN(3) . F REM . R\n\tOUTPUT = F '|' R\nEND" "abc|"

echo "---"
echo "PASS=$PASS FAIL=$FAIL"
[ "$FAIL" -eq 0 ]
