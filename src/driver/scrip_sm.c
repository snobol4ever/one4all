#include <stdio.h>
#include <stdlib.h>
#include <setjmp.h>
#include "scrip_sm.h"
#include "lower.h"
#include "../frontend/snobol4/scrip_cc.h"
#include "SM.h"
#include "sm_jit_interp.h"
#include "../runtime/interp/icn_runtime.h"
#include "../runtime/interp/pl_runtime.h"
#include "interp_private.h"
#include "polyglot.h"
extern int g_sno_err_active;
extern jmp_buf g_sno_err_jmp;
/*----------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------*/
static void sm_resolve_proc_entry_pcs(SM_sequence_t *p)
{
    int show = getenv("SCRIP_PROC_ENTRY_PCS") != NULL;
    if (show)
        fprintf(stderr, "[CH-17a] resolve entry_pcs (proc_table=%d procs, pl_pred_table=hash)\n",
                proc_count);
    for (int i = 0; i < proc_count; i++) {
        const char *nm = proc_table[i].name;
        int pc = nm ? sm_label_pc_lookup(p, nm) : -1;
        proc_table[i].entry_pc = pc;
        if (show)
            fprintf(stderr, "[CH-17a]   proc[%d] name=%-20s entry_pc=%d\n",
                    i, nm ? nm : "(null)", pc);
    }
    extern unsigned pl_pred_hash(const char *);
    int pl_total = 0, pl_resolved = 0;
    for (int b = 0; b < PL_PRED_TABLE_SIZE_FWD; b++) {
        for (Pl_PredEntry *e = g_stage2.pl_pred_table.buckets[b]; e; e = e->next) {
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
/*----------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------*/
stage2_t *sm_preamble(const tree_t *ast_prog){
    g_sno_err_active = 1;
    stage2_t *s2 = lower(ast_prog);
    if (!s2) {
        fprintf(stderr, "scrip: sm_lower failed\n");
        return NULL;
    }
    if (polyglot_lang_mask(ast_prog) & (1u << LANG_ICN)) {
        extern int g_lang;
        g_lang = LANG_ICN;
    }
    sm_resolve_proc_entry_pcs(&s2->sm);
    return s2;
}
/*----------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------*/
void sm_run_with_recovery(SM_sequence_t *sm, sm_runner_fn runner)
{
    SM_State st;
    sm_state_init(&st);
    int hybrid_err;
    while (1) {
        hybrid_err = setjmp(g_sno_err_jmp);
        if (hybrid_err != 0) {
            st.last_ok = 0;
            st.sp = 0;
            if (runner == sm_jit_run)
                sm_jit_unwind_call_stack(&st);
            if (st.pc < sm->count) st.pc++;
            while (st.pc < sm->count &&
                   sm->instrs[st.pc].op != SM_STNO &&
                   sm->instrs[st.pc].op != SM_HALT)
                st.pc++;
        }
        int rc = runner(sm, &st);
        if (rc == 0 || rc < -1) break;
        if (st.pc >= sm->count) break;
    }
}
