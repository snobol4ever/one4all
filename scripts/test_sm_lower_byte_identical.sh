#!/usr/bin/env bash
# test_sm_lower_byte_identical.sh — SM bytecode byte-identical gate (SR-1)
#
# Compiles a corpus of small programs across all six frontends,
# captures --dump-sm output, and compares md5 hashes against a baked
# baseline.  Any drift from baseline = FAIL.
#
# Usage:
#   bash scripts/test_sm_lower_byte_identical.sh            # compare
#   bash scripts/test_sm_lower_byte_identical.sh --bake     # bake baseline
#
# AUTHORS: Lon Jones Cherryholmes · Claude Sonnet 4.6  DATE: 2026-05-11

HERE="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
SCRIP="${HERE}/../scrip"
CORPUS="/home/claude/corpus"
BASELINE="${HERE}/sm_lower_baseline.txt"

PASS=0; FAIL=0; SKIP=0
BAKE=0
[ "${1:-}" = "--bake" ] && BAKE=1

if [ ! -x "$SCRIP" ]; then
    echo "SKIP scrip not built at $SCRIP"
    exit 0
fi

if [ ! -d "$CORPUS" ]; then
    echo "SKIP corpus not found at $CORPUS"
    exit 0
fi

# ------------------------------------------------------------------
# Program list: ~10 per frontend × 6 = ~60 total
# Format: "label  flag  path"
# ------------------------------------------------------------------
declare -a PROGRAMS=(
    # SNOBOL4 (21 feat programs — all parse cleanly)
    "sno_f01  --dump-sm  $CORPUS/programs/snobol4/feat/f01_core_labels_goto.sno"
    "sno_f02  --dump-sm  $CORPUS/programs/snobol4/feat/f02_string_ops.sno"
    "sno_f03  --dump-sm  $CORPUS/programs/snobol4/feat/f03_numeric.sno"
    "sno_f04  --dump-sm  $CORPUS/programs/snobol4/feat/f04_pattern_primitives.sno"
    "sno_f05  --dump-sm  $CORPUS/programs/snobol4/feat/f05_capture.sno"
    "sno_f06  --dump-sm  $CORPUS/programs/snobol4/feat/f06_builtins_predicates.sno"
    "sno_f07  --dump-sm  $CORPUS/programs/snobol4/feat/f07_keywords.sno"
    "sno_f08  --dump-sm  $CORPUS/programs/snobol4/feat/f08_data_array_table.sno"
    "sno_f09  --dump-sm  $CORPUS/programs/snobol4/feat/f09_functions.sno"
    "sno_f10  --dump-sm  $CORPUS/programs/snobol4/feat/f10_io_basic.sno"

    # Icon
    "icn_family    --dump-sm  $CORPUS/programs/icon/demo/family_icon.icn"
    "icn_parser    --dump-sm  $CORPUS/programs/icon/demo/icon_parser.icn"
    "icn_recog     --dump-sm  $CORPUS/programs/icon/demo/icon_recognizer.icn"

    # Prolog
    "pl_family     --dump-sm  $CORPUS/programs/prolog/demo/family_prolog.pl"
    "pl_parser     --dump-sm  $CORPUS/programs/prolog/demo/prolog_parser.pl"
    "pl_recog      --dump-sm  $CORPUS/programs/prolog/demo/prolog_recognizer.pl"

    # Raku
    "rk_arith_add  --dump-sm  $CORPUS/programs/raku/parser/arith_add.raku"
    "rk_arith_chain --dump-sm $CORPUS/programs/raku/parser/arith_chain.raku"
    "rk_arith_mul  --dump-sm  $CORPUS/programs/raku/parser/arith_mul.raku"
    "rk_arith_prec --dump-sm  $CORPUS/programs/raku/parser/arith_prec.raku"
    "rk_arr_get    --dump-sm  $CORPUS/programs/raku/parser/arr_get.raku"
    "rk_for_range  --dump-sm  $CORPUS/programs/raku/parser/for_range.raku"
    "rk_str_chars  --dump-sm  $CORPUS/programs/raku/parser/str_chars.raku"
    "rk_logic_or   --dump-sm  $CORPUS/programs/raku/parser/logic_or.raku"

    # Snocone
    "sc_literals   --dump-sm  $CORPUS/programs/snocone/corpus/sc1_literals.sc"
    "sc_assign     --dump-sm  $CORPUS/programs/snocone/corpus/sc2_assign.sc"
    "sc_control    --dump-sm  $CORPUS/programs/snocone/corpus/sc4_control.sc"
    "sc_strings    --dump-sm  $CORPUS/programs/snocone/corpus/sc8_strings.sc"
    "sc_wordcount  --dump-sm  $CORPUS/programs/snocone/corpus/sc10_wordcount.sc"

    # Rebus
    "reb_btrees    --dump-sm  $CORPUS/programs/rebus/binary_trees.reb"
)

# ------------------------------------------------------------------
# Bake mode: generate baseline
# ------------------------------------------------------------------
if [ "$BAKE" = "1" ]; then
    echo "=== Baking SM lower baseline ==="
    rm -f "$BASELINE"
    for entry in "${PROGRAMS[@]}"; do
        read -r label flag path <<< "$entry"
        if [ ! -f "$path" ]; then
            echo "  SKIP $label (file missing: $path)"
            continue
        fi
        hash=$(timeout 15 "$SCRIP" $flag "$path" 2>/dev/null | md5sum | awk '{print $1}')
        printf '%s  %s\n' "$hash" "$label" >> "$BASELINE"
        echo "  BAKED $label  $hash"
    done
    echo "Baseline written: $BASELINE"
    exit 0
fi

# ------------------------------------------------------------------
# Compare mode
# ------------------------------------------------------------------
if [ ! -f "$BASELINE" ]; then
    echo "SKIP baseline not baked — run: bash $0 --bake"
    exit 0
fi

echo "=== SM lower byte-identical gate ==="

declare -A EXPECTED
while IFS='  ' read -r hash label; do
    EXPECTED["$label"]="$hash"
done < "$BASELINE"

for entry in "${PROGRAMS[@]}"; do
    read -r label flag path <<< "$entry"
    if [ ! -f "$path" ]; then
        echo "  SKIP $label (file missing)"
        SKIP=$((SKIP+1))
        continue
    fi
    expected="${EXPECTED[$label]:-}"
    if [ -z "$expected" ]; then
        echo "  SKIP $label (not in baseline)"
        SKIP=$((SKIP+1))
        continue
    fi
    actual=$(timeout 15 "$SCRIP" $flag "$path" 2>/dev/null | md5sum | awk '{print $1}')
    if [ "$actual" = "$expected" ]; then
        echo "  PASS $label"
        PASS=$((PASS+1))
    else
        echo "  FAIL $label  expected=$expected  got=$actual"
        FAIL=$((FAIL+1))
    fi
done

echo ""
echo "PASS=$PASS FAIL=$FAIL SKIP=$SKIP"
[ "$FAIL" -eq 0 ]
