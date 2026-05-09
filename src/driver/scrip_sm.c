/*
 * scrip_sm.c — shared SM-mode preamble + run-with-recovery helpers (RS-14).
 *
 * Implementations of sm_preamble() and sm_run_with_recovery() — see
 * scrip_sm.h for design rationale.
 *
 * Authors: Lon Jones Cherryholmes · Claude Sonnet 4.7
 * Date: 2026-05-03
 */

#include <stdio.h>
#include <stdlib.h>
#include <setjmp.h>
#include "scrip_sm.h"
#include "../runtime/x86/sm_lower.h"
#include "../runtime/x86/sm_prog.h"           /* CH-17a: sm_label_pc_lookup */
#include "../runtime/common/ir_clone.h"
#include "../runtime/interp/coro_runtime.h"   /* CH-17a: proc_table */
#include "../runtime/interp/pl_runtime.h"     /* CH-17a: g_pl_pred_table */
#include "interp_private.h"   /* label_table_build, prescan_defines, label_table_clear_stmts */
#include "polyglot.h"         /* RS-26: polyglot_lang_mask, polyglot_init, LANG_SNO */

extern int g_sno_err_active;
extern jmp_buf g_sno_err_jmp;

/* CH-17a: resolve entry_pcs from sm_lower's named-label table.
 *
 * After sm_lower runs, every Icon/Raku proc and Prolog predicate in proc_table /
 * g_pl_pred_table looks itself up by name in the SM_Program's label table.  When
 * sm_lower has emitted a named SM_LABEL expression for that proc (CH-17b for Icon/Raku,
 * CH-17d for Prolog), the lookup returns a valid pc; otherwise -1.  CH-17a is
 * pure scaffolding — sm_lower does not yet emit named proc-body expressions for any
 * frontend, so every entry_pc remains -1 here.  Subsequent rungs flip producers,
 * then consumers, then delete the legacy AST_t* paths.
 *
 * Env var SCRIP_PROC_ENTRY_PCS=1 prints a summary line per resolved entry. */
static void sm_resolve_proc_entry_pcs(SM_Program *p)
{
    int show = getenv("SCRIP_PROC_ENTRY_PCS") != NULL;
    if (show)
        fprintf(stderr, "[CH-17a] resolve entry_pcs (proc_table=%d procs, pl_pred_table=hash)\n",
                proc_count);
    /* Icon / Raku proc table */
    for (int i = 0; i < proc_count; i++) {
        const char *nm = proc_table[i].name;
        int pc = nm ? sm_label_pc_lookup(p, nm) : -1;
        proc_table[i].entry_pc = pc;
        if (show)
            fprintf(stderr, "[CH-17a]   proc[%d] name=%-20s entry_pc=%d\n",
                    i, nm ? nm : "(null)", pc);
    }
    /* Prolog pred table — walk all hash buckets */
    extern unsigned pl_pred_hash(const char *);
    int pl_total = 0, pl_resolved = 0;
    for (int b = 0; b < PL_PRED_TABLE_SIZE_FWD; b++) {
        for (Pl_PredEntry *e = g_pl_pred_table.buckets[b]; e; e = e->next) {
            int pc = e->key ? sm_label_pc_lookup(p, e->key) : -1;
            e->entry_pc = pc;
            pl_total++;
            if (pc >= 0) pl_resolved++;
            if (show)
                fprintf(stderr, "[CH-17a]   pred  key=%-20s entry_pc=%d\n",
                        e->key ? e->key : "(null)", pc);
        }
    }
    if (show)
        fprintf(stderr, "[CH-17a] summary: pl_total=%d pl_resolved=%d (others=-1 are CH-17b/d territory)\n",
                pl_total, pl_resolved);
}

SM_Program *sm_preamble(void *prog_void)
{
    CODE_t *prog = (CODE_t *)prog_void;
    label_table_build(prog);
    prescan_defines(prog);
    g_sno_err_active = 1;   /* arm so sno_runtime_error longjmps safely */

    /* RS-26: symmetric preamble — populate proc_table (Icon/Raku) and
     * g_pl_pred_table (Prolog) from the live IR.  For pure-SNO programs the
     * lang_mask is just (1<<LANG_SNO) and the per-language init branches
     * inside polyglot_init are guarded — adds no observable behaviour for
     * SNOBOL4.  For Icon/Prolog this is what makes coro_call /
     * pl_pred_table_lookup find their targets when SM_BB_PUMP / SM_BB_ONCE
     * fires inside sm_interp_run. */
    uint32_t lang_mask = polyglot_lang_mask(prog);
    polyglot_init(prog, lang_mask);

    SM_Program *sm = sm_lower(prog);
    if (!sm) {
        fprintf(stderr, "scrip: sm_lower failed\n");
        return NULL;
    }

    /* CH-17a: resolve entry_pcs for every proc / Prolog predicate.  Pure
     * scaffolding: today every entry resolves to -1 because sm_lower does not
     * yet emit named proc-body expressions (CH-17b/d will).  Consumers still use
     * the legacy proc/AST_t* paths until CH-17c/e flip them. */
    sm_resolve_proc_entry_pcs(sm);

    /* RS-9b: SM_Program is self-contained for SNOBOL4 — emit_push_expr
     * GC-clones the AST_t subtrees so SM owns them.  Free the IR.
     *
     * RS-26: but for non-SNO frontends, BB drives the live IR — the proc/
     * pred tables populated above hold AST_t* into prog that survive only
     * if the IR survives.  Gate code_free on lang_mask: pure-SNO programs
     * still get the RS-9b behaviour; mixed or non-SNO programs keep the IR
     * alive for the duration of execution.
     *
     * RS-9c: g_current_sm_prog must be set so _usercall_hook detects SM
     * bodies, regardless of whether IR is freed. */
    g_current_sm_prog = sm;
    if (lang_mask == (1u << LANG_SNO)) {
        code_free(prog);
        label_table_clear_stmts();
    }
    return sm;
}

void sm_run_with_recovery(SM_Program *sm, sm_runner_fn runner)
{
    SM_State st;
    sm_state_init(&st);

    /* Arm g_sno_err_jmp: sno_runtime_error longjmps here on error.
     * We treat each error as statement failure: mark last_ok=0, advance pc,
     * and re-enter the runner.  Mirrors execute_program's per-stmt setjmp
     * pattern and prevents longjmp into an uninitialized jmp_buf. */
    int hybrid_err;
    while (1) {
        hybrid_err = setjmp(g_sno_err_jmp);
        if (hybrid_err != 0) {
            /* runtime error fired mid-statement: mark fail, advance past
             * the current instruction and continue */
            st.last_ok = 0;
            st.sp = 0;  /* reset value stack — state is undefined after error */
            if (st.pc < sm->count) st.pc++;  /* skip offending instruction */
            /* drain to next SM_STNO boundary so we resume cleanly */
            while (st.pc < sm->count &&
                   sm->instrs[st.pc].op != SM_STNO &&
                   sm->instrs[st.pc].op != SM_HALT)
                st.pc++;
        }
        int rc = runner(sm, &st);
        if (rc == 0 || rc < -1) break;  /* halted or fatal */
        if (st.pc >= sm->count) break;
    }
}
