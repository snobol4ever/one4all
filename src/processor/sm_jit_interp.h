#ifndef EMIT_SM_BINARY_H
#define EMIT_SM_BINARY_H
#include "sm_prog.h"
#include "sm_interp.h"
#include <setjmp.h>
/*---- mode-3 binary codegen + runner -------------------------------------*/
/* sm_codegen — compile prog into SEG_CODE (binary blobs + trampoline). */
int  sm_codegen         (SM_Program * prog);
/* sm_jit_run — execute the codegen'd program. Returns 0 on normal halt. */
int  sm_jit_run         (SM_Program * prog, SM_State * st);
int  sm_jit_run_plain   (SM_Program * prog, SM_State * st);
int  sm_jit_run_steps   (SM_Program * prog, SM_State * st, int n);
/* sm_jit_unwind_call_stack — unwind JIT call stack after longjmp error recovery. */
void sm_jit_unwind_call_stack(SM_State * st);
extern int     g_jit_step_limit;
extern int     g_jit_steps_done;
extern jmp_buf g_jit_step_jmp;
#endif
