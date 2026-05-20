#!/usr/bin/env bash
# rename_phase2_recase_by_layer.sh — second-pass rename for the IR Rename rung.
# Implements Lon's rule: UPPERCASE prefix for functions manipulating SM_t/BB_t data;
# lowercase prefix for the runtime that materializes them.
#
# Inputs from Phase 1 (rename_ir_to_bb_and_sm.sh):
#   - IR_t/IR_block_t/IR_e and tag families renamed to BB_*  ✓
#   - SM_Instr/SM_Program/etc renamed to SM_t/SM_sequence_t ✓
#   - sm_prog_* renamed to sm_seq_* (WRONG case — phase 2 fixes)
#   - IR_alloc/free/etc renamed to bb_* (WRONG case — phase 2 fixes)
#
# This phase:
#   1. Re-case the data-manipulating bb_* functions back to BB_*
#   2. Re-case sm_seq_* to SM_seq_*  and g_current_sm_seq to g_current_SM_seq
#   3. Re-case SM_templates fns and emit/label helpers to SM_*
#   4. Add the 46 missing graph IR tags (IR_ALT, IR_BINOP, IR_SUCCEED, etc.)
#   5. Leave bb_pool.h functions (bb_alloc/bb_free/bb_seal) ALONE — they're runtime
#   6. Leave sm_interp_*/sm_jit_*/sm_push/sm_pop/sm_peek etc. ALONE — runtime
set -uo pipefail
HERE="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
ONE4ALL="$HERE/.."
CORPUS="${CORPUS:-/home/claude/corpus}"
roots=("$ONE4ALL/src" "$ONE4ALL/include")
[ -d "$CORPUS" ] && roots+=("$CORPUS")
files=$(find "${roots[@]}" -type f \( -name '*.c' -o -name '*.h' -o -name '*.inc' \) \
        ! -name '*.tab.c' ! -name '*.tab.h' ! -name '*.lex.c' ! -name 'snobol4.c' 2>/dev/null)
SED_PROG=$(cat <<'SED_RULES'
# === Group A: IR data-manipulating functions back to UPPERCASE ===
# (Phase 1 incorrectly made them lowercase; pool collision proved it.)
s/\bbb_node_alloc\b/BB_node_alloc/g
s/\bbb_lower_pat\b/BB_lower_pat/g
s/\bbb_alloc\b/BB_alloc/g
s/\bbb_reset\b/BB_reset/g
s/\bbb_print\b/BB_print/g
# bb_free has TWO meanings: IR free AND pool free.  Disambiguate by signature in code:
# IR call:   bb_free(BB_graph_t *)  ← becomes BB_free
# Pool call: bb_free(bb_buf_t, size_t) ← stays bb_free (it's runtime)
# Mechanical sed can't tell them apart, so we rename ALL bb_free to BB_free FIRST,
# then restore the pool sites by hand below in Group F.
s/\bbb_free\b/BB_free/g
# bb_exec_once/pump/node are RUNTIME — leave lowercase.  No sed for them.
# === Group B: SM sequence helpers back to UPPERCASE ===
s/\bsm_seq_new\b/SM_seq_new/g
s/\bsm_seq_free\b/SM_seq_free/g
s/\bsm_seq_print\b/SM_seq_print/g
s/\bsm_seq_dcg_add\b/SM_seq_dcg_add/g
s/\bg_current_sm_seq\b/g_current_SM_seq/g
# === Group C: SM emit / label table helpers to UPPERCASE ===
s/\bsm_emit\b/SM_emit/g
s/\bsm_emit_f\b/SM_emit_f/g
s/\bsm_emit_i\b/SM_emit_i/g
s/\bsm_emit_ii\b/SM_emit_ii/g
s/\bsm_emit_ptr\b/SM_emit_ptr/g
s/\bsm_emit_s\b/SM_emit_s/g
s/\bsm_emit_si\b/SM_emit_si/g
s/\bsm_emit_sii\b/SM_emit_sii/g
s/\bsm_emit_sip\b/SM_emit_sip/g
s/\bsm_label\b/SM_label/g
s/\bsm_label_named\b/SM_label_named/g
s/\bsm_label_pc_lookup\b/SM_label_pc_lookup/g
s/\bsm_patch_jump\b/SM_patch_jump/g
s/\bsm_stno_label_record\b/SM_stno_label_record/g
s/\bsm_codegen\b/SM_codegen/g
# === Group D: SM_templates dispatch functions to UPPERCASE ===
# These take an SM_t and emit target code.  Data-side of the compile pipeline.
s/\bsm_halt\b/SM_halt/g
s/\bsm_jump\b/SM_jump/g
s/\bsm_jump_s\b/SM_jump_s/g
s/\bsm_jump_f\b/SM_jump_f/g
s/\bsm_return\b/SM_return/g
s/\bsm_freturn\b/SM_freturn/g
s/\bsm_nreturn\b/SM_nreturn/g
s/\bsm_push_lit_i\b/SM_push_lit_i/g
s/\bsm_push_lit_s\b/SM_push_lit_s/g
s/\bsm_push_lit_f\b/SM_push_lit_f/g
s/\bsm_push_null\b/SM_push_null/g
s/\bsm_push_var\b/SM_push_var/g
s/\bsm_store_var\b/SM_store_var/g
s/\bsm_void_pop\b/SM_void_pop/g
s/\bsm_concat\b/SM_concat/g
s/\bsm_neg\b/SM_neg/g
s/\bsm_coerce_num\b/SM_coerce_num/g
s/\bsm_exp\b/SM_exp/g
s/\bsm_add\b/SM_add/g
s/\bsm_sub\b/SM_sub/g
s/\bsm_mul\b/SM_mul/g
s/\bsm_div\b/SM_div/g
s/\bsm_mod\b/SM_mod/g
s/\bsm_stno\b/SM_stno/g
s/\bsm_acomp\b/SM_acomp/g
s/\bsm_lcomp\b/SM_lcomp/g
s/\bsm_exec_stmt\b/SM_exec_stmt/g
# Pattern templates — all 28 sm_pat_* in SM_templates/
s/\bsm_pat_abort\b/SM_pat_abort/g
s/\bsm_pat_alt\b/SM_pat_alt/g
s/\bsm_pat_any\b/SM_pat_any/g
s/\bsm_pat_any_i\b/SM_pat_any_i/g
s/\bsm_pat_arb\b/SM_pat_arb/g
s/\bsm_pat_arbno\b/SM_pat_arbno/g
s/\bsm_pat_bal\b/SM_pat_bal/g
s/\bsm_pat_break\b/SM_pat_break/g
s/\bsm_pat_capture\b/SM_pat_capture/g
s/\bsm_pat_capture_fn\b/SM_pat_capture_fn/g
s/\bsm_pat_capture_fn_args\b/SM_pat_capture_fn_args/g
s/\bsm_pat_cat\b/SM_pat_cat/g
s/\bsm_pat_deref\b/SM_pat_deref/g
s/\bsm_pat_eps\b/SM_pat_eps/g
s/\bsm_pat_fail\b/SM_pat_fail/g
s/\bsm_pat_len\b/SM_pat_len/g
s/\bsm_pat_lit\b/SM_pat_lit/g
s/\bsm_pat_notany\b/SM_pat_notany/g
s/\bsm_pat_pos\b/SM_pat_pos/g
s/\bsm_pat_refname\b/SM_pat_refname/g
s/\bsm_pat_rem\b/SM_pat_rem/g
s/\bsm_pat_rpos\b/SM_pat_rpos/g
s/\bsm_pat_rtab\b/SM_pat_rtab/g
s/\bsm_pat_span\b/SM_pat_span/g
s/\bsm_pat_succeed\b/SM_pat_succeed/g
s/\bsm_pat_tab\b/SM_pat_tab/g
s/\bsm_pat_usercall\b/SM_pat_usercall/g
s/\bsm_pat_usercall_args\b/SM_pat_usercall_args/g
# === Group E: 46 missing graph IR tags ===
s/\bIR_ALTERNATE\b/BB_ALTERNATE/g
s/\bIR_ALT\b/BB_ALT/g
s/\bIR_ASSIGN\b/BB_ASSIGN/g
s/\bIR_AUGOP\b/BB_AUGOP/g
s/\bIR_BINOP_GEN\b/BB_BINOP_GEN/g
s/\bIR_BINOP\b/BB_BINOP/g
s/\bIR_BREAK\b/BB_BREAK/g
s/\bIR_CALL\b/BB_CALL/g
s/\bIR_CASE\b/BB_CASE/g
s/\bIR_CSET_COMPL\b/BB_CSET_COMPL/g
s/\bIR_CSET_DIFF\b/BB_CSET_DIFF/g
s/\bIR_CSET_INTER\b/BB_CSET_INTER/g
s/\bIR_CSET_UNION\b/BB_CSET_UNION/g
s/\bIR_DEFINE_NAMES\b/BB_DEFINE_NAMES/g
s/\bIR_EVERY\b/BB_EVERY/g
s/\bIR_FAIL\b/BB_FAIL/g
s/\bIR_GOTO\b/BB_GOTO/g
s/\bIR_IDENTICAL\b/BB_IDENTICAL/g
s/\bIR_IF\b/BB_IF/g
s/\bIR_INITIAL\b/BB_INITIAL/g
s/\bIR_INTERROGATE\b/BB_INTERROGATE/g
s/\bIR_LIMIT\b/BB_LIMIT/g
s/\bIR_NEG\b/BB_NEG/g
s/\bIR_NEXT\b/BB_NEXT/g
s/\bIR_NONNULL\b/BB_NONNULL/g
s/\bIR_NOT\b/BB_NOT/g
s/\bIR_NULL_TEST\b/BB_NULL_TEST/g
s/\bIR_POS\b/BB_POS/g
s/\bIR_PROC\b/BB_PROC/g
s/\bIR_RANDOM\b/BB_RANDOM/g
s/\bIR_REPEAT\b/BB_REPEAT/g
s/\bIR_RETURN\b/BB_RETURN/g
s/\bIR_SCAN\b/BB_SCAN/g
s/\bIR_SEQ_EXPR\b/BB_SEQ_EXPR/g
s/\bIR_SEQ\b/BB_SEQ/g
s/\bIR_SIZE\b/BB_SIZE/g
s/\bIR_SUCCEED\b/BB_SUCCEED/g
s/\bIR_SUSPEND\b/BB_SUSPEND/g
s/\bIR_SWAP\b/BB_SWAP/g
s/\bIR_TO_BY\b/BB_TO_BY/g
s/\bIR_UNKNOWN\b/BB_UNKNOWN/g
s/\bIR_UNOP\b/BB_UNOP/g
s/\bIR_UNTIL\b/BB_UNTIL/g
s/\bIR_VAR\b/BB_VAR/g
s/\bIR_WHILE\b/BB_WHILE/g
# IR_E_COUNT, IR_WALK_MAX, IR_IS_GEN_KIND, IR_IS_GEN_KIND_TO, IR_EXEC_H, IR_LANG_* — UNTOUCHED.
SED_RULES
)
modified=0
echo "$files" | while read -r f; do
    [ -z "$f" ] && continue
    before=$(md5sum "$f" 2>/dev/null | awk '{print $1}')
    sed -i "$SED_PROG" "$f" 2>/dev/null
    after=$(md5sum "$f" 2>/dev/null | awk '{print $1}')
    [ "$before" != "$after" ] && modified=$((modified + 1))
done
echo "Phase 2 sed done."
# === Group F: restore pool functions back to lowercase ===
# The Group A sed renamed all bb_free → BB_free indiscriminately, including the
# three pool sites in emit_bb.c that take (bb_buf_t, size_t).  Restore those by
# hand using context: pool sites are recognizable by `bb_buf_t buf = ...` patterns.
POOL_H="$ONE4ALL/src/processor/bb_pool.h"
POOL_C="$ONE4ALL/src/processor/bb_pool.c"
if [ -f "$POOL_H" ]; then
    # bb_pool.h declares the three pool functions — Group A turned them to BB_*. Undo.
    sed -i 's/\bBB_alloc\b/bb_alloc/g; s/\bBB_free\b/bb_free/g' "$POOL_H"
    echo "  restored: bb_pool.h"
fi
if [ -f "$POOL_C" ]; then
    sed -i 's/\bBB_alloc\b/bb_alloc/g; s/\bBB_free\b/bb_free/g' "$POOL_C"
    echo "  restored: bb_pool.c"
fi
# Restore emit_bb.c pool-using sites — they hold bb_buf_t.
EMIT_BB="$ONE4ALL/src/emitter/emit_bb.c"
if [ -f "$EMIT_BB" ]; then
    # Lines like "bb_buf_t buf = BB_alloc(FLAT_BUF_MAX)" — pool call (single size arg).
    # Lines like "BB_free(buf, FLAT_BUF_MAX)" — pool call (two args buf + size).
    # The 3-arg BB_alloc(int, int) IR call doesn't appear in emit_bb.c.
    sed -i 's/BB_alloc(FLAT_BUF_MAX)/bb_alloc(FLAT_BUF_MAX)/g' "$EMIT_BB"
    sed -i 's/BB_free(buf, FLAT_BUF_MAX)/bb_free(buf, FLAT_BUF_MAX)/g' "$EMIT_BB"
    echo "  restored: emit_bb.c (pool sites)"
fi
echo "Done."
