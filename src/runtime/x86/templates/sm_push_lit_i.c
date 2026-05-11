/*
 * templates/sm_push_lit_i.c — SM_PUSH_LIT_I per-opcode template.
 *
 * SM_PUSH_LIT_I pushes a 64-bit integer literal onto the value stack.
 * Runtime call: rt_push_int(int64_t val).
 *
 * Inline x86 form:
 *   mov  rdi, <val>          ; load immediate into first arg register
 *   call rt_push_int@PLT     ; (TEXT) call the runtime; (BINARY) baked
 *                            ;        indirect via rax — see t_call_sym_plt
 *
 * No mode-3-vs-mode-4 divergence in semantics: both modes call rt_push_int
 * at runtime.  In practice mode-3 currently uses the Standard blob (a C
 * thunk in sm_codegen.c's emit_pushlit_blob); this template describes the
 * inline form mode-4 emits and that mode-3 will adopt when ME-4+ lands.
 * The fn_fallback argument to t_call_sym_plt is therefore 0 today: mode-4
 * resolves rt_push_int via @PLT at link time; mode-3 never walks this
 * template.  If mode-3 is ever wired through it, the fallback must be
 * (uint64_t)(uintptr_t)&rt_push_int and rt_push_int must be linked into
 * the scrip binary (currently libscrip_rt.so only).
 *
 * Three-way pattern: body calls t_* helpers that read bb_emit_mode.
 * No `emitter_t *e` is read; parameter preserved for caller compat.
 *
 * Authors: Lon Jones Cherryholmes · Claude Sonnet 4.6 / Claude Opus 4.7
 * Sprint:  EM-MODE4-IS-MODE3-DUMP / GOAL-MODE4-EMIT
 */

#include "../emitter.h"   /* unused emitter_t * param; compatibility */
#include "../bb_emit.h"

void emit_sm_push_lit_i(emitter_t *e, int64_t val)
{
    (void)e;

    t_comment("SM_PUSH_LIT_I — push integer literal");

    /* MACRO_DEF mode: macro takes one parameter named `val`. */
    static const char *const params[] = { "val" };
    t_macro_begin("PUSH_INT", params, 1);

    t_mov_rdi_imm64((uint64_t)val);
    t_call_sym_plt("rt_push_int", 0);   /* mode-4 resolves via @PLT */

    t_macro_end();
    t_pad_to_blob_size();
}
