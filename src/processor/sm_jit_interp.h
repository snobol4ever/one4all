#ifndef EMIT_SM_BINARY_H
#define EMIT_SM_BINARY_H
#include "SM.h"
#include "sm_interp.h"
#include <setjmp.h>
/*================================================================================================================================================================================*/
int  SM_codegen              (SM_sequence_t * prog);
int  sm_jit_run              (SM_sequence_t * prog, SM_State * st);
int  sm_jit_run_plain        (SM_sequence_t * prog, SM_State * st);
int  sm_jit_run_steps        (SM_sequence_t * prog, SM_State * st, int n);
void sm_jit_unwind_call_stack(SM_State * st);
extern int     g_jit_step_limit;
extern int     g_jit_steps_done;
extern jmp_buf g_jit_step_jmp;
/*================================================================================================================================================================================*/
#endif
