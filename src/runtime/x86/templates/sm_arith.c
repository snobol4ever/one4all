/*
 * templates/sm_arith.c — SM arithmetic opcode templates.
 *
 * SM_ADD, SM_SUB, SM_MUL, SM_DIV, SM_MOD all route through rt_arith(int op)
 * with the SM opcode enum value passed in rdi as the discriminator.
 *
 * Emits: movabs rdi, <op_enum>; call rt_arith@PLT
 *
 * No mode-3/mode-4 divergence: both modes call rt_arith.
 * Mode-3 still uses Standard blob (pending ME-4+); template wired to mode-4.
 *
 * Sub-rung: EM-MODE4-IS-MODE3-DUMP-l (GOAL-MODE4-EMIT).
 * Session:  2026-05-11, Claude Sonnet 4.6.
 *
 * Authors: Lon Jones Cherryholmes · Claude Sonnet 4.6
 * Sprint:  EM-MODE4-IS-MODE3-DUMP-l / GOAL-MODE4-EMIT
 */

#include "../emitter.h"
#include "templates.h"

/*
 * emit_sm_arith_op — single entry point covering all five arithmetic ops.
 *   op_enum:    the SM opcode integer (SM_ADD=17, SM_SUB=18 … SM_MOD=22).
 *   macro_name: the GAS macro name for this op (ADD_NUM/SUB_NUM/etc.).
 */
void emit_sm_arith_op(emitter_t *e, int op_enum, const char *macro_name)
{
    if (!e) return;
    EMIT_OPT(e, comment, e, macro_name ? macro_name : "SM_ARITH");
    EMIT_OPT(e, macro_begin, e, macro_name ? macro_name : "ARITH", NULL, 0);
    emit_mov_rdi_imm64(e, (uint64_t)(unsigned)op_enum);
    emit_call_sym_plt(e, "rt_arith", 0);
    EMIT_OPT(e, macro_end, e);
    EMIT_OPT(e, pad_to_blob_size, e);
}
