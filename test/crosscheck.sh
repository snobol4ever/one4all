#!/usr/bin/env bash
# test/crosscheck.sh — scrip crosscheck: same programs, all backends, outputs must agree
# Usage: CORPUS=/home/claude/corpus bash test/crosscheck.sh
# From:  /home/claude/one4all/
#
# Backends: x86 (nasm), JVM (jasmin+java), NET (ilasm+mono), WASM (wabt)
# Each backend skipped gracefully if tools not present.
# Oracle: SPITBOL x64 (/home/claude/x64/bin/sbl -b)
#
# Sections:
#   1. x86  — compile .sno -> .s -> nasm -> run, diff vs .ref
#   2. JVM  — compile .sno -> .j -> jasmin -> java, diff vs .ref
#   3. NET  — compile .sno -> .il -> ilasm -> mono, diff vs .ref
#   4. WASM — compile .sno -> .wat -> wabt -> run, diff vs .ref

set -uo pipefail
ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
SCRIP="${SCRIP:-$ROOT/scrip}"
CORPUS="${CORPUS:-/home/claude/corpus}"
JASMIN="${JASMIN:-$ROOT/src/backend/jasmin.jar}"
SPITBOL="${SPITBOL:-/home/claude/x64/bin/sbl}"
INC="${INC:-$CORPUS/programs/snobol4/demo/inc}"
TIMEOUT="${TIMEOUT:-15}"

PASS=0; FAIL=0; SKIP=0
FAILURES=""

GREEN='\033[0;32m'; RED='\033[0;31m'; YELLOW='\033[0;33m'; RESET='\033[0m'

have() { command -v "$1" &>/dev/null; }

result() {
    local status="$1" label="$2"
    if [ "$status" = "PASS" ]; then
        PASS=$((PASS+1))
    elif [ "$status" = "SKIP" ]; then
        SKIP=$((SKIP+1))
    else
        FAIL=$((FAIL+1))
        FAILURES="${FAILURES}  FAIL ${label}\n"
    fi
}

echo "=== scrip crosscheck ==="
echo ""

# ── 1. x86 ───────────────────────────────────────────────────────────────────
echo "── x86 ──"
if ! have nasm; then
    echo -e "${YELLOW}SKIP${RESET} x86 section (nasm not found)"
    SKIP=$((SKIP+1))
else
    bash "$ROOT/test/crosscheck/run_crosscheck_x86_rung.sh" "$CORPUS/crosscheck/patterns" \
         "$CORPUS/crosscheck/assign" "$CORPUS/crosscheck/arith_new" \
         "$CORPUS/crosscheck/control_new" 2>/dev/null | tail -3
fi

# ── 2. JVM ───────────────────────────────────────────────────────────────────
echo "── JVM ──"
if ! have java || [ ! -f "$JASMIN" ]; then
    echo -e "${YELLOW}SKIP${RESET} JVM section (java or jasmin not found)"
    SKIP=$((SKIP+1))
else
    bash "$ROOT/test/crosscheck/run_crosscheck_jvm_rung.sh" "$CORPUS/crosscheck/patterns" \
         "$CORPUS/crosscheck/assign" "$CORPUS/crosscheck/arith_new" 2>/dev/null | tail -3
fi

# ── 3. NET ───────────────────────────────────────────────────────────────────
echo "── NET ──"
if ! have mono && ! have dotnet; then
    echo -e "${YELLOW}SKIP${RESET} NET section (mono/dotnet not found)"
    SKIP=$((SKIP+1))
else
    bash "$ROOT/test/crosscheck/run_crosscheck_net_rung.sh" "$CORPUS/crosscheck/patterns" \
         "$CORPUS/crosscheck/assign" 2>/dev/null | tail -3
fi

# ── 4. WASM ──────────────────────────────────────────────────────────────────
echo "── WASM ──"
if ! have wasm-interp && ! have wasmtime; then
    echo -e "${YELLOW}SKIP${RESET} WASM section (wasm runtime not found)"
    SKIP=$((SKIP+1))
else
    bash "$ROOT/test/run_wasm_corpus_rung.sh" 2>/dev/null | tail -3
fi

echo ""
echo "PASS=$PASS FAIL=$FAIL SKIP=$SKIP"
[ -n "$FAILURES" ] && printf "$FAILURES" | head -40
[ $FAIL -eq 0 ]
