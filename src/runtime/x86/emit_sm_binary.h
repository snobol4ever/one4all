/*
 * emit_sm_binary.h — SM_Program → x86 in-memory code (mode 3, --jit-run)
 *
 * Authors: Lon Jones Cherryholmes · Claude Sonnet 4.6
 */

#ifndef EMIT_SM_BINARY_H
#define EMIT_SM_BINARY_H

#include "sm_prog.h"
#include "sm_interp.h"   /* SM_State */
#include <setjmp.h>

/* sm_codegen — compile prog into SEG_CODE (binary blobs + trampoline).
 * Must be called after sm_image_init(). Returns 0 on success, -1 on error. */
int sm_codegen(SM_Program *prog);

/* sm_jit_run — execute the codegen'd program. Returns 0 on normal halt. */
int sm_jit_run(SM_Program *prog, SM_State *st);
int sm_jit_run_plain(SM_Program *prog, SM_State *st);

/* Unwind JIT call stack after longjmp error recovery. */
void sm_jit_unwind_call_stack(SM_State *st);

/* IM-5: step-limit — run at most n statements then return */
extern int     g_jit_step_limit;
extern int     g_jit_steps_done;
extern jmp_buf g_jit_step_jmp;
int sm_jit_run_steps(SM_Program *prog, SM_State *st, int n);

#endif /* EMIT_SM_BINARY_H */
