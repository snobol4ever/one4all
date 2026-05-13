#!/usr/bin/env bash
# test_gate_em8_snocone_jit_emit.sh — EM-8 gate:
# five Snocone smoke programs compiled via --jit-emit --x64 produce correct output.
# Gate: PASS=5 FAIL=0.
#
# Self-contained per RULES.md: paths derived from $0; no env deps.

HERE="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
ROOT="$(cd "$HERE/.." && pwd)"
SCRIP="${SCRIP:-$ROOT/scrip}"
RT_DIR="${RT_DIR:-$ROOT/out}"
TIMEOUT="${TIMEOUT:-8}"

if [ ! -x "$SCRIP" ]; then
    echo "SKIP scrip not built at $SCRIP"
    exit 0
fi
if [ ! -f "$RT_DIR/libscrip_rt.so" ]; then
    echo "SKIP libscrip_rt.so not built — run: make libscrip_rt"
    exit 0
fi

TMP="$(mktemp -d)"
trap 'rm -rf "$TMP"' EXIT

PASS=0; FAIL=0; FAILS=""

emit_test() {
    local label="$1" expected="$2"
    local src="$TMP/${label}.sc"
    cat > "$src"
    "$SCRIP" --jit-emit --x64 "$src" > "$TMP/${label}.s" 2>"$TMP/${label}.emit.err" < /dev/null
    if [ $? -ne 0 ] || [ ! -s "$TMP/${label}.s" ]; then
        echo "  FAIL $label (emit failed: $(head -1 "$TMP/${label}.emit.err"))"
        FAIL=$((FAIL+1)); FAILS="$FAILS $label(emit)"; return
    fi
    gcc -no-pie "$TMP/${label}.s" -L"$RT_DIR" -lscrip_rt \
        -Wl,-rpath,"$RT_DIR" -o "$TMP/${label}.prog" 2>"$TMP/${label}.link.err"
    if [ $? -ne 0 ]; then
        echo "  FAIL $label (link failed: $(head -1 "$TMP/${label}.link.err"))"
        FAIL=$((FAIL+1)); FAILS="$FAILS $label(link)"; return
    fi
    local actual
    actual=$(timeout "$TIMEOUT" "$TMP/${label}.prog" < /dev/null 2>/dev/null)
    if [ "$actual" = "$expected" ]; then
        echo "  PASS $label"; PASS=$((PASS+1))
    else
        echo "  FAIL $label (expected: $(echo "$expected"|head -1) got: $(echo "$actual"|head -1))"
        FAIL=$((FAIL+1)); FAILS="$FAILS $label(diff)"
    fi
}

echo "=== EM-8 Snocone jit-emit smoke ==="

emit_test "output" "hello" << 'EOF'
OUTPUT = "hello";
EOF

emit_test "arith" "5" << 'EOF'
OUTPUT = 2 + 3;
EOF

emit_test "procedure" "42" << 'EOF'
function Double(n) {
    Double = n + n; return;
}
OUTPUT = Double(21);
EOF

emit_test "if_eq" "yes" << 'EOF'
if (EQ(2 + 2, 4)) { OUTPUT = "yes"; } else { OUTPUT = "no"; }
EOF

emit_test "while" "$(printf '1\n2\n3')" << 'EOF'
i = 1;
while (LE(i, 3)) {
    OUTPUT = i;
    i = i + 1;
}
EOF

echo ""
echo "PASS=$PASS FAIL=$FAIL"
[ -n "$FAILS" ] && echo "FAILS:$FAILS"
[ "$FAIL" -eq 0 ]
