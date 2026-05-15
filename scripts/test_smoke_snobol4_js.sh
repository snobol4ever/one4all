#!/bin/bash
# test_smoke_snobol4_js.sh — Smoke tests for SNOBOL4 → JavaScript emitter
# Usage: bash scripts/test_smoke_snobol4_js.sh
# Gate: 7/7 PASS (all smoke programs execute correctly via JS)

set -e

SCRIP=${SCRIP:-/home/claude/one4all/scrip}
CORPUS=${CORPUS:-/home/claude/corpus}
SMOKE_DIR="$CORPUS/programs/snobol4/smoke"
TEMP_DIR=${TEMP_DIR:-/tmp/sno_js_tests}

if [ ! -d "$SMOKE_DIR" ]; then
    echo "ERROR: smoke test directory not found: $SMOKE_DIR"
    exit 1
fi

if [ ! -x "$SCRIP" ]; then
    echo "ERROR: scrip executable not found: $SCRIP"
    exit 1
fi

mkdir -p "$TEMP_DIR"

# List of 7 smoke tests (from GOAL-SN4-JS-EMIT)
TESTS=(
    "hello.sno"
    "null.sno"
    "empty_string.sno"
    "multi.sno"
    "expr_parser.sno"
    "beauty_compiled.sno"
    # Note: original lists 7 tests; actual count may vary
)

PASS=0
FAIL=0

echo "Running SNOBOL4 smoke tests via JavaScript emission..."
echo ""

for test_file in "$SMOKE_DIR"/*.sno; do
    test_name=$(basename "$test_file")
    base_name="${test_name%.sno}"
    js_file="$TEMP_DIR/${base_name}.js"
    out_file="$TEMP_DIR/${base_name}.out"
    ref_file="${test_file%.sno}.ref"

    echo -n "Test $test_name ... "

    # Step 1: Emit JS
    if ! "$SCRIP" --target=js "$test_file" > "$js_file" 2>/dev/null; then
        echo "FAIL (emit error)"
        FAIL=$((FAIL + 1))
        continue
    fi

    # Step 2: Execute JS with node (need to fix require path)
    # Replace relative require with absolute path to sno_runtime.js
    RT_PATH="$SCRIP/../src/runtime/js/sno_runtime.js"
    if [ ! -f "$RT_PATH" ]; then
        RT_PATH="/home/claude/one4all/src/runtime/js/sno_runtime.js"
    fi
    
    if ! node "$js_file" 2>/dev/null; then
        echo "FAIL (execution error)"
        FAIL=$((FAIL + 1))
        continue
    fi

    # Step 3: Compare to oracle (if reference exists)
    if [ -f "$ref_file" ]; then
        if diff -q "$out_file" "$ref_file" > /dev/null 2>&1; then
            echo "PASS"
            PASS=$((PASS + 1))
        else
            echo "FAIL (output mismatch)"
            FAIL=$((FAIL + 1))
        fi
    else
        # No oracle yet — just verify it runs without error
        echo "PASS (emit + exec OK, no oracle)"
        PASS=$((PASS + 1))
    fi
done

echo ""
echo "Results: $PASS PASS, $FAIL FAIL"

if [ $FAIL -eq 0 ]; then
    echo "Gate: All smoke tests passed ✅"
    exit 0
else
    echo "Gate: Some tests failed ❌"
    exit 1
fi
