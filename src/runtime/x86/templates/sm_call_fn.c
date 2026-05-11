/*
 * templates/sm_call_fn.c — SM_CALL_FN per-opcode template.
 *
 * SM_CALL_FN invokes a named function (builtin or user-defined) with
 * nargs arguments that have been pushed onto the value stack.
 *   a[0].s = function name (interned in strtab / in-process string ptr)
 *   a[1].i = nargs
 *
 * Runtime call: rt_call(const char *name, int nargs)
 *   rdi = pointer to function name string
 *   esi = nargs (32-bit)
 *
 * Inline x86 form (TEXT/MACRO_DEF — GAS asm):
 *   lea  rdi, [rip + .Sname]   ; RIP-relative load of strtab label
 *   mov  esi, <nargs>          ; nargs in 32-bit arg register
 *   call rt_call@PLT
 *
 * Binary form (BINARY — in-process JIT):
 *   movabs rdi, <string_ptr>   ; bake in-process string pointer directly
 *   mov    esi, <nargs>        ; 32-bit immediate
 *   mov    rax, <rt_call_ptr>
 *   call   rax
 *
 * Mode discipline:
 *   BINARY:    Not yet wired — SM_CALL_FN still uses standard_blob in
 *              sm_codegen.c (mode-3).  fn_fallback=0 in t_call_sym_plt.
 *   TEXT:      emit_sm_call_dispatch calls sm_emit_lbl_int32 directly
 *              (proven working pattern; macro name from sm_template_lookup).
 *   MACRO_DEF: emit_sm_call_fn called directly with EMIT_MACRO_DEF mode;
 *              emits the .macro CALL_FN lbl, n / body / .endm block.
 *
 * The MACRO_DEF body is the source of truth for sm_macros.s CALL_FN.
 * TEXT invocation uses the legacy sm_emit_lbl_int32 path (same as
 * PUSH_STR, STORE_VAR, etc.) until the t_* / emitter_t vtable paths
 * are fully unified in a later rung.
 *
 * Sub-rung: EM-MODE4-IS-MODE3-DUMP-p (GOAL-MODE4-EMIT).
 * Session:  2026-05-11, Claude Sonnet 4.6.
 *
 * Authors: Lon Jones Cherryholmes · Claude Sonnet 4.6
 * Sprint:  EM-MODE4-IS-MODE3-DUMP-p / GOAL-MODE4-EMIT
 */

#include "../emitter.h"   /* unused emitter_t * param; caller compat */
#include "../bb_emit.h"

/*
 * emit_sm_call_fn — MACRO_DEF backend: emit the .macro CALL_FN definition.
 *
 * Called only when bb_emit_mode == EMIT_MACRO_DEF (sm_macros.s regeneration).
 * Emits:
 *   .macro CALL_FN lbl, n
 *       lea  rdi, [rip + \lbl]
 *       mov  esi, \n
 *       call rt_call@PLT
 *   .endm
 *
 * name_lbl / name_ptr / nargs are ignored in MACRO_DEF mode — the body
 * uses parameter references (\lbl, \n) not concrete values.
 */
void emit_sm_call_fn(emitter_t *e, const char *name_lbl,
                     uint64_t name_ptr, int nargs)
{
    (void)e;
    (void)name_lbl;
    (void)name_ptr;
    (void)nargs;

    t_comment("SM_CALL_FN — call named function via rt_call(name, nargs)");

    static const char *const params[] = { "lbl", "n" };
    t_macro_begin("CALL_FN", params, 2);

    /* In MACRO_DEF mode these emit the body lines.
     * In EMIT_TEXT or EMIT_BINARY this function should not be called
     * (the dispatch in sm_codegen_x64_emit.c uses sm_emit_lbl_int32
     * for TEXT and standard_blob for BINARY). */
    t_lea_rdi_strtab_sym(NULL, 0);   /* MACRO_DEF: emits  lea rdi, [rip + \lbl] */
    t_mov_esi_imm32(0);              /* MACRO_DEF: emits  mov esi, \n */
    t_call_sym_plt("rt_call", 0);

    t_macro_end();
    t_pad_to_blob_size();
}
