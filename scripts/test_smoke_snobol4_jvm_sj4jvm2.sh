#!/usr/bin/env bash
# test_smoke_snobol4_jvm_sj4jvm2.sh — SJ4-JVM-2 gate: SnoRt.j assembles and runs correctly
set -uo pipefail
HERE="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
JASMIN="${JASMIN:-$HERE/../src/backend/jasmin.jar}"
SNORT_J="$HERE/../src/runtime/jvm/SnoRt.j"
SNORT_MS_J="$HERE/../src/runtime/jvm/SnoRtMatchState.j"
PASS=0; FAIL=0
tmp=$(mktemp -d)
# Step 1: assemble SnoRt.j
if timeout 15 java -jar "$JASMIN" "$SNORT_J" -d "$tmp/" > "$tmp/asm.out" 2>&1; then
    echo "PASS SnoRt.j assembles"
    PASS=$((PASS+1))
else
    echo "FAIL SnoRt.j assembly"; cat "$tmp/asm.out"; FAIL=$((FAIL+1))
fi
# Step 2: assemble SnoRtMatchState.j
if timeout 15 java -jar "$JASMIN" "$SNORT_MS_J" -d "$tmp/" > "$tmp/asm2.out" 2>&1; then
    echo "PASS SnoRtMatchState.j assembles"
    PASS=$((PASS+1))
else
    echo "FAIL SnoRtMatchState.j assembly"; cat "$tmp/asm2.out"; FAIL=$((FAIL+1))
fi
# Step 3: push_str + halt_tos prints the string
cat > "$tmp/TestHalt.j" << 'EOF'
.class public TestHalt
.super java/lang/Object
.method public static main([Ljava/lang/String;)V
    .limit stack 3
    .limit locals 1
    invokestatic rt/SnoRt/init()V
    ldc "hello world"
    invokestatic rt/SnoRt/push_str(Ljava/lang/String;)V
    invokestatic rt/SnoRt/halt_tos()V
    return
.end method
EOF
java -jar "$JASMIN" "$tmp/TestHalt.j" -d "$tmp/" > /dev/null 2>&1
got=$(timeout 5 java -cp "$tmp" TestHalt 2>/dev/null)
if [ "$got" = "hello world" ]; then
    echo "PASS push_str+halt_tos"
    PASS=$((PASS+1))
else
    echo "FAIL push_str+halt_tos: got='$got'"
    FAIL=$((FAIL+1))
fi
# Step 4: push_int + halt_tos
cat > "$tmp/TestInt.j" << 'EOF'
.class public TestInt
.super java/lang/Object
.method public static main([Ljava/lang/String;)V
    .limit stack 4
    .limit locals 1
    invokestatic rt/SnoRt/init()V
    ldc2_w 42
    invokestatic rt/SnoRt/push_int(J)V
    invokestatic rt/SnoRt/halt_tos()V
    return
.end method
EOF
java -jar "$JASMIN" "$tmp/TestInt.j" -d "$tmp/" > /dev/null 2>&1
got=$(timeout 5 java -cp "$tmp" TestInt 2>/dev/null)
if [ "$got" = "42" ]; then
    echo "PASS push_int+halt_tos"
    PASS=$((PASS+1))
else
    echo "FAIL push_int+halt_tos: got='$got'"
    FAIL=$((FAIL+1))
fi
# Step 5: store_var OUTPUT prints
cat > "$tmp/TestOut.j" << 'EOF'
.class public TestOut
.super java/lang/Object
.method public static main([Ljava/lang/String;)V
    .limit stack 3
    .limit locals 1
    invokestatic rt/SnoRt/init()V
    ldc "from OUTPUT"
    invokestatic rt/SnoRt/push_str(Ljava/lang/String;)V
    ldc "OUTPUT"
    invokestatic rt/SnoRt/store_var(Ljava/lang/String;)V
    return
.end method
EOF
java -jar "$JASMIN" "$tmp/TestOut.j" -d "$tmp/" > /dev/null 2>&1
got=$(timeout 5 java -cp "$tmp" TestOut 2>/dev/null)
if [ "$got" = "from OUTPUT" ]; then
    echo "PASS store_var(OUTPUT)"
    PASS=$((PASS+1))
else
    echo "FAIL store_var(OUTPUT): got='$got'"
    FAIL=$((FAIL+1))
fi
rm -rf "$tmp"
echo "---"
echo "PASS=$PASS FAIL=$FAIL"
[ "$FAIL" -eq 0 ]
