#!/usr/bin/env bash
# test_sn4_wasm_ladder_safe.sh — SN4-WASM-5 progress gate
# Pattern mirrors scripts/test_sn4_js_ladder_safe.sh: each .sno tested individually
# (fresh scrip + wat2wasm + node host per program) to avoid one bad program taking
# down the batch.  Compares stdout against the co-located .ref oracle.
#
# Pipeline:  .sno  -->  scrip --sm-emit --target=wasm  -->  .wat
#                 -->  wat2wasm                       -->  .wasm
#                 -->  node sno_host.mjs              -->  stdout
#                 -->  diff vs .ref
#
# Usage:
#   bash scripts/test_sn4_wasm_ladder_safe.sh             # quiet summary
#   bash scripts/test_sn4_wasm_ladder_safe.sh --verbose   # per-program PASS/FAIL
#
# Exits 0 if PASS >= MIN_PASS (default 10, override via env), else 1.
set -uo pipefail
HERE="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
SCRIP="${SCRIP:-$HERE/../scrip}"
CORPUS="${CORPUS:-/home/claude/corpus}"
WASM_RUNTIME_WAT="${WASM_RUNTIME_WAT:-$HERE/../src/runtime/wasm/sno_runtime.wat}"
WASM_RUNTIME_WASM="${WASM_RUNTIME_WAT%.wat}.wasm"
WASM_BB_WAT="${WASM_BB_WAT:-$HERE/../src/runtime/wasm/bb_boxes.wat}"
WASM_BB_WASM="${WASM_BB_WAT%.wat}.wasm"
HOST_MJS="${HOST_MJS:-$HERE/../src/runtime/wasm/sno_host.mjs}"
TIMEOUT="${TIMEOUT:-6}"
MIN_PASS="${MIN_PASS:-10}"
VERBOSE="${1:-}"

if [ ! -x "$SCRIP" ];                 then echo "SKIP: scrip not found: $SCRIP";   exit 0; fi
if ! command -v wat2wasm >/dev/null;  then echo "SKIP: wat2wasm not found";        exit 0; fi
if ! command -v node     >/dev/null;  then echo "SKIP: node not found";            exit 0; fi
if [ ! -f "$HOST_MJS" ];              then echo "SKIP: sno_host.mjs not found";    exit 0; fi
if [ ! -d "$CORPUS" ];                then echo "SKIP: corpus not found";          exit 0; fi

if [ ! -f "$WASM_RUNTIME_WASM" ] || [ "$WASM_RUNTIME_WAT" -nt "$WASM_RUNTIME_WASM" ]; then
    if ! wat2wasm "$WASM_RUNTIME_WAT" -o "$WASM_RUNTIME_WASM" 2>/dev/null; then
        echo "FAIL: could not compile sno_runtime.wat"; exit 1
    fi
fi
if [ -f "$WASM_BB_WAT" ] && { [ ! -f "$WASM_BB_WASM" ] || [ "$WASM_BB_WAT" -nt "$WASM_BB_WASM" ]; }; then
    wat2wasm "$WASM_BB_WAT" -o "$WASM_BB_WASM" 2>/dev/null || true
fi

PASS=0; FAIL=0; SKIP=0
TMPD=$(mktemp -d); trap "rm -rf $TMPD" EXIT

# Skip list — programs that segfault in the scrip frontend (not WASM-specific).
# scanerr.sno: SIGSEGV in lower.c:304 (emit_pat_capture) on deferred-expr capture
# *TAB(X) / *ANY(X) / *LEN(X) — var_node->v.sval is 0x1 (uninit) for the unary-*
# operand.  Crashes on ALL targets and on --interp / --ir-run (frontend bug).
# To be fixed in a separate upstream session; tracked in PLAN.md.
SKIP_LIST=" scanerr "

run_one() {
    local sno="$1"
    [ -f "$sno" ] || return 0
    local base; base=$(basename "$sno" .sno)
    local dir;  dir=$(dirname "$sno")
    local ref="$dir/$base.ref"
    [ -f "$ref" ] || return 0
    case " $SKIP_LIST " in
        *" $base "*)
            [[ "$VERBOSE" == "--verbose" ]] && echo "SKIP $base [upstream frontend bug]"
            SKIP=$((SKIP+1)); return 0 ;;
    esac
    local wat="$TMPD/$base.wat"
    local wasm="$TMPD/$base.wasm"
    local out="$TMPD/$base.out"
    local inp="$dir/$base.input"
    if ! timeout "$TIMEOUT" "$SCRIP" --sm-emit --target=wasm "$sno" > "$wat" 2>/dev/null; then
        [[ "$VERBOSE" == "--verbose" ]] && echo "FAIL $base [emit]"
        FAIL=$((FAIL+1)); return 0
    fi
    if ! timeout "$TIMEOUT" wat2wasm "$wat" -o "$wasm" 2>/dev/null; then
        [[ "$VERBOSE" == "--verbose" ]] && echo "FAIL $base [wat2wasm]"
        FAIL=$((FAIL+1)); return 0
    fi
    local stdin_arg="< /dev/null"
    [ -f "$inp" ] && stdin_arg="< \"$inp\""
    export SNO_RUNTIME_WASM="$WASM_RUNTIME_WASM"
    export SNO_BB_BOXES_WASM="$WASM_BB_WASM"
    if ! eval "timeout $TIMEOUT node \"$HOST_MJS\" \"$wasm\" $stdin_arg > \"$out\" 2>/dev/null"; then
        [[ "$VERBOSE" == "--verbose" ]] && echo "FAIL $base [node]"
        FAIL=$((FAIL+1)); return 0
    fi
    if diff -q "$out" "$ref" > /dev/null 2>&1; then
        [[ "$VERBOSE" == "--verbose" ]] && echo "PASS $base"
        PASS=$((PASS+1))
    else
        [[ "$VERBOSE" == "--verbose" ]] && echo "FAIL $base [diff]"
        FAIL=$((FAIL+1))
    fi
}

echo "=== SNOBOL4 -> WASM ladder (safe) ==="
for sno in "$CORPUS/programs/csnobol4-suite/"*.sno; do run_one "$sno"; done
for sno in "$CORPUS/programs/snobol4/demo/"*.sno; do run_one "$sno"; done
for sno in "$CORPUS/programs/snobol4/feat/"*.sno; do run_one "$sno"; done

TOTAL=$((PASS+FAIL+SKIP))
echo "PASS=$PASS FAIL=$FAIL SKIP=$SKIP TOTAL=$TOTAL"
[ "$PASS" -ge "$MIN_PASS" ] && exit 0 || exit 1
