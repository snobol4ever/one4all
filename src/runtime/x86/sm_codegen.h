/*
 * sm_codegen.h — SM_Program → x86 in-memory code (M-JIT-RUN)
 *
 * Authors: Lon Jones Cherryholmes · Claude Sonnet 4.6
 * Date: 2026-04-07
 */

#ifndef SM_CODEGEN_H
#define SM_CODEGEN_H

#include "sm_prog.h"
#include "sm_interp.h"   /* SM_State */
#include <setjmp.h>
#include <stdio.h>

/*
 * sm_codegen — compile prog into SEG_DISPATCH + SEG_CODE.
 * Must be called after sm_image_init().
 * Returns 0 on success, -1 on error.
 */
int sm_codegen(SM_Program *prog);

/*
 * sm_jit_run — execute the codegen'd program.
 * Must be called after sm_codegen().
 * Returns 0 on normal halt.
 */
int sm_jit_run(SM_Program *prog, SM_State *st);

/* Unwind JIT call stack after longjmp error recovery; restores saved NV
 * slots and resets call_depth to 0.  Called by sm_run_with_recovery. */
void sm_jit_unwind_call_stack(SM_State *st);

int sm_jit_run_plain(SM_Program *prog, SM_State *st);

/*
 * sm_codegen_text — TEXT mode: emit SM_Program as GNU-as .s to `out`.
 * src_path is optional (used for source-line annotations); may be NULL.
 * Returns 0 on success, -1 on error.
 * (ESP-13: moved from sm_codegen_x64_emit.c)
 */
int sm_codegen_text(SM_Program *prog, FILE *out, const char *src_path);

/* EDP-2: set by scrip.c --jit-emit-inline; read by sm_codegen_text pipeline. */
extern int g_jit_emit_inline;

/* IM-5: step-limit — run at most n statements then return */
extern int     g_jit_step_limit;
extern int     g_jit_steps_done;
extern jmp_buf g_jit_step_jmp;
int sm_jit_run_steps(SM_Program *prog, SM_State *st, int n);

#endif /* SM_CODEGEN_H */
