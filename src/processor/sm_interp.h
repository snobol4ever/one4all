#ifndef SM_INTERP_H
#define SM_INTERP_H
#include <stdlib.h>
#include <setjmp.h>
#include "SM.h"
#include "snobol4.h"
/*================================================================================================================================================================================*/
#define SM_CALL_STACK_MAX 256
#define SM_SAVED_NV_MAX    64
typedef struct {
    int      ret_pc;
    int      ret_ok;
    char   * retval_name;
    int      nsaved;
    char   * saved_names[SM_SAVED_NV_MAX];
    DESCR_t  saved_vals [SM_SAVED_NV_MAX];
    int      ret_jump_s_pc;
    int      ret_jump_f_pc;
    int      caller_sp;
    DESCR_t * caller_stack;
} SmCallFrame;
/*--------------------------------------------------------------------------------------------------------------------------------------------------------------------------------*/
#define SM_GEN_LOCAL_MAX 8
struct GeneratorState {
    int      entry_pc;
    int      resume_pc;
    int      started;
    DESCR_t  yielded;
    DESCR_t * stack;
    int      sp;
    int      stack_cap;
    int      last_ok;
    DESCR_t  locals[SM_GEN_LOCAL_MAX];
    int      saved_frame_depth;
};
/*--------------------------------------------------------------------------------------------------------------------------------------------------------------------------------*/
typedef struct {
    DESCR_t    * stack;
    int          sp;
    int          stack_cap;
    int          last_ok;
    int          pc;
    int          jit_epilogue_pending;
    int          jit_in_call;
    jmp_buf      err_jmp;
    int          err_fail_pc;
    int          err_armed;
    SmCallFrame  call_stack[SM_CALL_STACK_MAX];
    int          call_depth;
} SM_State;
/*--------------------------------------------------------------------------------------------------------------------------------------------------------------------------------*/
int  sm_interp_run        (SM_sequence_t * prog, SM_State * st);
int  sm_interp_run_inner  (SM_sequence_t * prog, SM_State * st);
int  sm_interp_run_steps  (SM_sequence_t * prog, SM_State * st, int n);
void sm_state_init        (SM_State * st);
void sm_push              (SM_State * st, DESCR_t d);
DESCR_t sm_pop            (SM_State * st);
DESCR_t sm_peek           (SM_State * st);
DESCR_t sm_call_expression(int entry_pc);
/*--------------------------------------------------------------------------------------------------------------------------------------------------------------------------------*/
extern int     g_sm_step_limit;
extern int     g_sm_steps_done;
extern jmp_buf g_sm_step_jmp;
/*--------------------------------------------------------------------------------------------------------------------------------------------------------------------------------*/
GeneratorState * generator_state_new     (int entry_pc);
GeneratorState * generator_state_new_proc(int pi, DESCR_t * args, int nargs);
int bb_broker_drive_sm    (GeneratorState * gs, void (*body_fn)(DESCR_t val, void * arg), void * arg);
int bb_broker_drive_sm_one(GeneratorState * gs, DESCR_t * out);
extern GeneratorState * g_current_generator_state;
/*--------------------------------------------------------------------------------------------------------------------------------------------------------------------------------*/
struct tree_t;
int            every_table_register(struct tree_t * ast);
struct tree_t * every_table_lookup (int id);
void           every_table_reset   (void);
/*================================================================================================================================================================================*/
#endif
