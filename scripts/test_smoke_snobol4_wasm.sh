#!/usr/bin/env bash
# test_smoke_snobol4_wasm.sh — SN4-WASM-4 gate: full WASM pipeline on 7 smoke programs
# scrip --sm-emit --target=wasm → .wat file → wat2wasm → .wasm → node sno_host.mjs → verify output
set -uo pipefail
HERE="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
SCRIP="${SCRIP:-$HERE/../scrip}"
ORACLE="${ORACLE:-/home/claude/x64/bin/sbl}"
HOST_MJS="${HOST_MJS:-$HERE/../src/runtime/wasm/sno_host.mjs}"
RUNTIME_WAT="${RUNTIME_WAT:-$HERE/../src/runtime/wasm/sno_runtime.wat}"
RUNTIME_WASM="${RUNTIME_WAT%.wat}.wasm"
PASS=0; FAIL=0

if [ ! -x "$SCRIP" ]; then echo "SKIP: scrip not found: $SCRIP"; exit 0; fi
if ! command -v wat2wasm >/dev/null 2>&1; then echo "SKIP: wat2wasm not found"; exit 0; fi
if ! command -v node >/dev/null 2>&1; then echo "SKIP: node not found"; exit 0; fi
if [ ! -f "$HOST_MJS" ]; then echo "SKIP: sno_host.mjs not found: $HOST_MJS"; exit 0; fi

# Pre-compile sno_runtime.wat if stale
if [ ! -f "$RUNTIME_WASM" ] || [ "$RUNTIME_WAT" -nt "$RUNTIME_WASM" ]; then
    if ! wat2wasm "$RUNTIME_WAT" -o "$RUNTIME_WASM" 2>/dev/null; then
        echo "FAIL: could not compile sno_runtime.wat"
        exit 1
    fi
fi

run_smoke() {
    local name="$1"
    local sno="$2"
    local tmp
    tmp=$(mktemp -d)
    local prog="$tmp/$name.sno"
    printf '%s\nEND\n' "$sno" > "$prog"
    # Oracle output
    local oracle_out=""
    if [ -x "$ORACLE" ]; then
        oracle_out=$(timeout 8 "$ORACLE" -b "$prog" < /dev/null 2>/dev/null || true)
    fi
    # Emit WAT
    local wat_file="$tmp/$name.wat"
    if ! timeout 8 "$SCRIP" --sm-emit --target=wasm "$prog" > "$wat_file" 2>/dev/null; then
        echo "FAIL $name (emit)"
        FAIL=$((FAIL+1))
        rm -rf "$tmp"
        return
    fi
    # Assemble
    local wasm_file="$tmp/$name.wasm"
    if ! timeout 8 wat2wasm "$wat_file" -o "$wasm_file" 2>/dev/null; then
        echo "FAIL $name (wat2wasm)"
        FAIL=$((FAIL+1))
        rm -rf "$tmp"
        return
    fi
    # Run
    local wasm_out
    wasm_out=$(timeout 8 node "$HOST_MJS" "$wasm_file" < /dev/null 2>/dev/null || true)
    rm -rf "$tmp"
    if [ "$oracle_out" = "$wasm_out" ]; then
        echo "PASS $name"
        PASS=$((PASS+1))
    else
        echo "FAIL $name (output mismatch)"
        echo "  oracle: $(printf '%q' "$oracle_out")"
        echo "  wasm:   $(printf '%q' "$wasm_out")"
        FAIL=$((FAIL+1))
    fi
}

run_smoke "null"      ""
run_smoke "lit_hello" "OUTPUT = 'hello'"
run_smoke "concat"    "OUTPUT = 'foo' 'bar'"
run_smoke "arith_add" "OUTPUT = 3 + 4"
run_smoke "store_var" "X = 'stored'; OUTPUT = X"
run_smoke "cond_jump" "X = 1; (X = 2) :S(ok); ok OUTPUT = X"
run_smoke "multi_out" "OUTPUT = 'line1'; OUTPUT = 'line2'"

echo "---"
echo "PASS=$PASS FAIL=$FAIL"
[ "$FAIL" -eq 0 ]
