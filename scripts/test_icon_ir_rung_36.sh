#!/usr/bin/env bash
# test_icon_ir_rung_36.sh — rung36: JCON integration suite (75 tests) — IC-7
# Tests marked .xfail are known-unimplemented features (co-expressions, large integers,
# &error trapping, etc.) — they count as XFAIL, not FAIL.
# Authors: LCherryholmes · Claude Sonnet 4.6   DATE: 2026-04-16
set -euo pipefail
HERE="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
SCRIP="${SCRIP:-$HERE/../scrip}"
CORPUS="${CORPUS:-/home/claude/corpus/programs/icon}"
PASS=0; FAIL=0; XFAIL=0

if [ ! -x "$SCRIP" ];  then echo "SKIP scrip not found at $SCRIP";  exit 0; fi
if [ ! -d "$CORPUS" ]; then echo "SKIP corpus not found at $CORPUS"; exit 0; fi

run() {
    local base="$CORPUS/$1"
    [ -f "${base}.xfail" ] && { echo "  XFAIL $1"; XFAIL=$((XFAIL+1)); return; }
    [ -f "${base}.expected" ] || { echo "  SKIP  $1 (no .expected)"; return; }
    local stdin_f="${base}.stdin"
    local got want
    if [ -f "$stdin_f" ]; then
        got=$(timeout 30 "$SCRIP" --interp "${base}.icn" < "$stdin_f"  2>/dev/null) || true
    else
        got=$(timeout 30 "$SCRIP" --interp "${base}.icn" < /dev/null   2>/dev/null) || true
    fi
    want=$(cat "${base}.expected")
    if [ "$got" = "$want" ]; then
        echo "  PASS $1"; PASS=$((PASS+1))
    else
        echo "  FAIL $1"
        echo "    want: $(echo "$want" | head -3 | tr '\n' '|')"
        echo "    got:  $(echo "$got"  | head -3 | tr '\n' '|')"
        FAIL=$((FAIL+1))
    fi
}

echo "=== rung36: JCON integration suite ==="
run rung36_jcon_args
run rung36_jcon_arith
run rung36_jcon_augment
run rung36_jcon_btrees
run rung36_jcon_case
run rung36_jcon_center
run rung36_jcon_checkfpx
run rung36_jcon_ck
run rung36_jcon_coerce
run rung36_jcon_collate
run rung36_jcon_concord
run rung36_jcon_cxprimes
run rung36_jcon_diffwrds
run rung36_jcon_endetab
run rung36_jcon_errkwds
run rung36_jcon_errors
run rung36_jcon_evalx
run rung36_jcon_every
run rung36_jcon_fncs
run rung36_jcon_fncs1
run rung36_jcon_geddump
run rung36_jcon_gener
run rung36_jcon_genqueen
run rung36_jcon_htprep
run rung36_jcon_image
run rung36_jcon_io
run rung36_jcon_iobig
run rung36_jcon_kross
run rung36_jcon_kwds
run rung36_jcon_large
run rung36_jcon_left
run rung36_jcon_level
run rung36_jcon_lexcmp
run rung36_jcon_lgint
run rung36_jcon_lists
run rung36_jcon_map
run rung36_jcon_mathfunc
run rung36_jcon_meander
run rung36_jcon_mffsol
run rung36_jcon_mindfa
run rung36_jcon_misc
run rung36_jcon_nargs
run rung36_jcon_numeric
run rung36_jcon_others
run rung36_jcon_parse
run rung36_jcon_prefix
run rung36_jcon_prepro
run rung36_jcon_primes
run rung36_jcon_profsum
run rung36_jcon_proto
run rung36_jcon_queens
run rung36_jcon_radix
run rung36_jcon_random
run rung36_jcon_recent
run rung36_jcon_recogn
run rung36_jcon_record
run rung36_jcon_right
run rung36_jcon_roman
run rung36_jcon_scan
run rung36_jcon_scan1
run rung36_jcon_scan2
run rung36_jcon_sets
run rung36_jcon_sieve
run rung36_jcon_sorting
run rung36_jcon_statics
run rung36_jcon_string
run rung36_jcon_string1
run rung36_jcon_struct
run rung36_jcon_subjpos
run rung36_jcon_substring
run rung36_jcon_table
run rung36_jcon_toby
run rung36_jcon_trim
run rung36_jcon_var
run rung36_jcon_wordcnt

echo ""
echo "PASS=$PASS FAIL=$FAIL XFAIL=$XFAIL TOTAL=$((PASS+FAIL+XFAIL))"
[ "$FAIL" -eq 0 ]
