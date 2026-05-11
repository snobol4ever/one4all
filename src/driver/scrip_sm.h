/*
 * scrip_sm.h — shared SM-mode preamble + run-with-recovery helpers (RS-14).
 *
 * Three execution modes share the same SM_Program lifecycle:
 *   2. SM gen/interp  — sm_lower → sm_interp_run
 *   3. SM gen/exec    — sm_lower → sm_codegen → sm_jit_run
 *   4. SM gen/asm/link/exec (future)
 *
 * Each repeats:
 *   - label_table_build / prescan_defines
 *   - g_sno_err_active = 1
 *   - lower(prog) → SM_Program*
 *   - g_current_sm_prog = sm
 *   - code_free(prog) ; label_table_clear_stmts()
 *   - per-statement setjmp/error-recovery loop driving sm_interp_run / sm_jit_run
 *
 * sm_preamble() collapses the first six steps; sm_run_with_recovery() wraps
 * the seventh, taking the per-mode runner as a function pointer.
 *
 * Authors: Lon Jones Cherryholmes · Claude Sonnet 4.7
 * Date: 2026-05-03
 */

#ifndef SCRIP_SM_H
#define SCRIP_SM_H

#include "../runtime/x86/sm_prog.h"
#include "../runtime/x86/sm_interp.h"
#include "../ast/ast.h"  /* AST_t */

/* `prog` is a CODE_t* — declared opaque here to avoid coupling driver
 * orchestration to frontend/snobol4/scrip_cc.h. The implementation casts. */

/* Build label table, prescan DEFINEs, lower IR to SM_Program.
 * SI-6: takes AST_PROGRAM directly — CODE_t is gone.
 * After this call:
 *   - g_current_sm_prog is set to the returned SM_Program*
 *   - g_sno_err_active is armed for sno_runtime_error longjmp safety
 *
 * Returns NULL on failure (sm_lower error). On success, returned pointer
 * must be freed with sm_prog_free() after run completes. */
SM_Program *sm_preamble(const AST_t *ast_prog);

/* CH-17g-irrun-lowers: run sm_lower + sm_resolve_proc_entry_pcs on ast_prog
 * and then free the SM_Program. */
void sm_resolve_irrun_entry_pcs(const AST_t *ast_prog);

/* Driver-mode runner signature: takes program + state, returns
 *   0  = normal halt
 *   1  = step-limit reached (resume)
 *  -1  = step-boundary checkpoint (resume)
 *  <-1 = fatal */
typedef int (*sm_runner_fn)(SM_Program *prog, SM_State *st);

/* Per-statement setjmp/error-recovery loop.
 * Initialises a fresh SM_State, then repeatedly:
 *   - setjmp(g_sno_err_jmp): on error, mark fail, advance past offending
 *     instruction, drain to next SM_STNO boundary, re-enter
 *   - call runner(sm, &st)
 *   - exit on rc=0 or rc<-1 or pc>=count
 *
 * Identical body for both --sm-run (runner=sm_interp_run) and --jit-run
 * (runner=sm_jit_run). */
void sm_run_with_recovery(SM_Program *sm, sm_runner_fn runner);

#endif /* SCRIP_SM_H */
