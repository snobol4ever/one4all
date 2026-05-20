#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include "sync_monitor.h"
#include "snobol4.h"
#include "runtime/interp/icn_runtime.h"
#include "runtime/interp/pl_runtime.h"
#include "lower.h"
#include "SM.h"
#include "sm_interp.h"
#include "sm_jit_interp.h"
#include "sm_image.h"
#include "interp.h"
#include "frontend/snobol4/scrip_cc.h"  /* tree_t, stmt_attr_* */
#include "frontend/prolog/term.h"
#include "frontend/prolog/prolog_atom.h"
#ifdef WITH_CSNOBOL4
typedef struct { char *name; char *val_str; } CsnNvPair;
/*----------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------*/
int  csnobol4_run_steps(const char *sno_path, int step_limit,
                        CsnNvPair **out_pairs, int *out_count);
/*----------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------*/
void csn_nv_snapshot_free(CsnNvPair *pairs, int n);
#else
typedef struct { char *name; char *val_str; } CsnNvPair;
/*----------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------*/
static int  csnobol4_run_steps(const char *p, int n, CsnNvPair **o, int *c)
    { (void)p;(void)n; *o=NULL;*c=0; return -1; }
/*----------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------*/
static void csn_nv_snapshot_free(CsnNvPair *pairs, int n)
    { (void)pairs;(void)n; }
#endif
/*----------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------*/
void exec_snapshot_take(ExecSnapshot *s) {
    if (!s) return;
    s->nv_count = nv_snapshot(&s->nv_pairs);
    s->kw_stcount      = kw_stcount;
    s->kw_stlimit      = kw_stlimit;
    s->kw_anchor       = kw_anchor;
    s->frame_depth = frame_depth;
    s->pl_trail_mark   = trail_mark(&g_pl_trail);
    s->pl_locals       = NULL;
    s->pl_locals_count = 0;
    {
        int top = trail_mark(&g_pl_trail);
        if (top > 0 && g_pl_trail.stack) {
            s->pl_locals = malloc((size_t)top * sizeof(*s->pl_locals));
            int out = 0;
            for (int ti = 0; ti < top; ti++) {
                Term *var = g_pl_trail.stack[ti];
                if (!var) continue;
                char nbuf[32];
                snprintf(nbuf, sizeof nbuf, "_V%d", var->saved_slot);
                Term *val = term_deref(var);
                char vbuf[256];
                if (!val || val->tag == TT_VAR) {
                    snprintf(vbuf, sizeof vbuf, "_");
                } else if (val->tag == TERM_ATOM) {
                    const char *aname = prolog_atom_name(val->atom_id);
                    snprintf(vbuf, sizeof vbuf, "%s", aname ? aname : "?");
                } else if (val->tag == TERM_INT) {
                    snprintf(vbuf, sizeof vbuf, "%ld", val->ival);
                } else if (val->tag == TERM_FLOAT) {
                    snprintf(vbuf, sizeof vbuf, "%g", val->fval);
                } else if (val->tag == TERM_COMPOUND) {
                    const char *fn = prolog_atom_name(val->compound.functor);
                    snprintf(vbuf, sizeof vbuf, "%s/%d", fn ? fn : "?", val->compound.arity);
                } else {
                    snprintf(vbuf, sizeof vbuf, "<term>");
                }
                s->pl_locals[out].name    = strdup(nbuf);
                s->pl_locals[out].val_str = strdup(vbuf);
                out++;
            }
            s->pl_locals_count = out;
        }
    }
    s->frame_locals       = NULL;
    s->frame_locals_count = 0;
    if (frame_depth > 0) {
        int total = 0;
        for (int fi = 0; fi < frame_depth; fi++)
            total += frame_stack[fi].sc.n;
        if (total > 0) {
            s->frame_locals = malloc((size_t)total * sizeof(NvPair));
            int out = 0;
            for (int fi = 0; fi < frame_depth; fi++) {
                IcnFrame *f = &frame_stack[fi];
                for (int si = 0; si < f->sc.n; si++) {
                    s->frame_locals[out].name = f->sc.e[si].name;
                    int slot = f->sc.e[si].slot;
                    s->frame_locals[out].val  = (slot >= 0 && slot < f->env_n)
                                              ? f->env[slot] : NULVCL;
                    out++;
                }
            }
            s->frame_locals_count = out;
        }
    }
    s->last_ok = -1;
}
/*----------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------*/
void exec_snapshot_restore(const ExecSnapshot *s) {
    if (!s) return;
    nv_restore(s->nv_pairs, s->nv_count);
    kw_stcount = s->kw_stcount;
    kw_stlimit = s->kw_stlimit;
    kw_anchor  = s->kw_anchor;
    frame_depth = 0;
    trail_unwind(&g_pl_trail, s->pl_trail_mark);
}
/*----------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------*/
void exec_snapshot_free(ExecSnapshot *s) {
    if (!s) return;
    s->nv_pairs = NULL;
    s->nv_count = 0;
    free(s->label_path);
    s->label_path     = NULL;
    s->label_path_n   = 0;
    s->label_path_cap = 0;
    free(s->frame_locals);
    s->frame_locals       = NULL;
    s->frame_locals_count = 0;
    for (int i = 0; i < s->pl_locals_count; i++) {
        free(s->pl_locals[i].name);
        free(s->pl_locals[i].val_str);
    }
    free(s->pl_locals);
    s->pl_locals       = NULL;
    s->pl_locals_count = 0;
}
/*----------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------*/
static void label_path_append(ExecSnapshot *s, const char *lbl) {
    if (s->label_path_n >= s->label_path_cap) {
        int newcap = s->label_path_cap ? s->label_path_cap * 2 : 16;
        s->label_path = realloc(s->label_path, (size_t)newcap * sizeof(const char *));
        s->label_path_cap = newcap;
    }
    s->label_path[s->label_path_n++] = lbl;
}
/*----------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------*/
static void label_path_print(const char *tag, const ExecSnapshot *s) {
    fprintf(stderr, "  %-4s path:", tag);
    int printed = 0;
    for (int i = 0; i < s->label_path_n; i++) {
        if (!s->label_path[i]) continue;
        fprintf(stderr, "%s[%s]", printed ? " \u2192 " : " ", s->label_path[i]);
        printed++;
    }
    if (!printed) fprintf(stderr, " (no labels reached)");
    fprintf(stderr, "\n");
}
/*----------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------*/
static int snap_diff(const ExecSnapshot *a, const char *a_name,
                     const ExecSnapshot *b, const char *b_name,
                     int verbose) {
    int ndiff = 0;
    for (int i = 0; i < a->nv_count; i++) {
        const char *name = a->nv_pairs[i].name;
        DESCR_t     va   = a->nv_pairs[i].val;
        DESCR_t     vb   = NULVCL;
        int found = 0;
        for (int j = 0; j < b->nv_count; j++) {
            if (strcmp(b->nv_pairs[j].name, name) == 0) {
                vb = b->nv_pairs[j].val; found = 1; break;
            }
        }
        const char *sa = VARVAL_fn(va);
        const char *sb = found ? VARVAL_fn(vb) : "";
        if (!sa) sa = "";
        if (!sb) sb = "";
        if (strcmp(sa, sb) != 0) {
            ndiff++;
            if (verbose)
                fprintf(stderr, "    %-12s  %s=%-20s  %s=%s\n",
                        name, a_name, sa, b_name, sb);
        }
    }
    for (int j = 0; j < b->nv_count; j++) {
        const char *name = b->nv_pairs[j].name;
        int found = 0;
        for (int i = 0; i < a->nv_count; i++) {
            if (strcmp(a->nv_pairs[i].name, name) == 0) { found = 1; break; }
        }
        if (!found) {
            const char *sb = VARVAL_fn(b->nv_pairs[j].val);
            if (!sb) sb = "";
            if (*sb) {
                ndiff++;
                if (verbose)
                    fprintf(stderr, "    %-12s  %s=%-20s  %s=%s\n",
                            name, a_name, "", b_name, sb);
            }
        }
    }
    return ndiff;
}
/*----------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------*/
int sync_monitor_run(const tree_t *prog, int verbose, const char *sno_path) {
    SM_sequence_t *sm_prog = lower(prog);
    if (!sm_prog) { fprintf(stderr, "sync_monitor: sm_lower failed\n"); return -1; }
    if (sm_image_init() != 0) {
        fprintf(stderr, "sync_monitor: sm_image_init failed\n");
        SM_seq_free(sm_prog); return -1;
    }
    if (SM_codegen(sm_prog) != 0) {
        fprintf(stderr, "sync_monitor: SM_codegen failed\n");
        SM_seq_free(sm_prog); return -1;
    }
    ExecSnapshot baseline = {0};
    exec_snapshot_take(&baseline);
    int nstmts = prog ? prog->n : 0;
    const char **ir_labels = calloc((size_t)(nstmts + 1), sizeof(const char *));
    { for (int i = 0; i < nstmts; i++) {
        const tree_t *s = prog->c[i];
        const char *lbl = s ? stmt_attr_str(stmt_attr_find(s, ":lbl")) : NULL;
        ir_labels[i+1] = (lbl && lbl[0]) ? lbl : NULL; } }
    ExecSnapshot ir_path  = {0};
    ExecSnapshot sm_path  = {0};
    ExecSnapshot jit_path = {0};
    int diverge_at = 0;
    for (int n = 1; n <= nstmts; n++) {
        ExecSnapshot ir_snap  = {0};
        ExecSnapshot sm_snap  = {0};
        ExecSnapshot jit_snap = {0};
        CsnNvPair   *csn_pairs = NULL;
        int          csn_count = 0;
        exec_snapshot_restore(&baseline);
        execute_program_steps(prog, n);
        exec_snapshot_take(&ir_snap);
        exec_snapshot_restore(&baseline);
        SM_State sm_st; sm_state_init(&sm_st);
        sm_interp_run_steps(sm_prog, &sm_st, n);
        exec_snapshot_take(&sm_snap);
        sm_snap.last_ok = sm_st.last_ok;
        exec_snapshot_restore(&baseline);
        SM_State jit_st; sm_state_init(&jit_st);
        sm_jit_run_steps(sm_prog, &jit_st, n);
        exec_snapshot_take(&jit_snap);
        jit_snap.last_ok = jit_st.last_ok;
        const char *lbl_n = (n <= sm_prog->stno_count) ? sm_prog->stno_labels[n] : NULL;
        label_path_append(&ir_path,  ir_labels[n]);
        label_path_append(&sm_path,  lbl_n);
        label_path_append(&jit_path, lbl_n);
        if (sno_path && n == nstmts) {
            exec_snapshot_restore(&baseline);
            csnobol4_run_steps(sno_path, n, &csn_pairs, &csn_count);
        }
        int ir_sm      = snap_diff(&ir_snap, "IR", &sm_snap,  "SM",  0);
        int ir_jit     = snap_diff(&ir_snap, "IR", &jit_snap, "JIT", 0);
        int ok_diverge = (sm_snap.last_ok != jit_snap.last_ok);
        int ir_csn = 0;
        if (sno_path && csn_pairs) {
            for (int si = 0; si < csn_count; si++) {
                int found = 0;
                for (int ii = 0; ii < ir_snap.nv_count; ii++) {
                    if (strcmp(ir_snap.nv_pairs[ii].name, csn_pairs[si].name) == 0) {
                        found = 1;
                        const char *iv = VARVAL_fn(ir_snap.nv_pairs[ii].val);
                        if (!iv) iv = "";
                        if (strcmp(iv, csn_pairs[si].val_str) != 0)
                            ir_csn++;
                        break;
                    }
                }
                (void)found;
            }
        }
        if (ir_sm || ir_jit || ok_diverge || ir_csn) {
            const char *hdr_lbl = ir_labels[n] ? ir_labels[n] : "-";
            int lineno = 0;
            { if (n-1 < prog->n) {
                const tree_t *ws = prog->c[n-1];
                const char *lv = ws ? stmt_attr_str(stmt_attr_find(ws, ":line")) : NULL;
                if (lv) lineno = atoi(lv); } }
            fprintf(stderr, "DIVERGE at stmt %d [label: %s, line %d]\n", n, hdr_lbl, lineno);
            label_path_print("IR",  &ir_path);
            label_path_print("SM",  &sm_path);
            label_path_print("JIT", &jit_path);
            auto void print_exec_line(const char *tag, const ExecSnapshot *s);
            void print_exec_line(const char *tag, const ExecSnapshot *s) {
                if (s->last_ok < 0) fprintf(stderr, "  %-4s last_ok=?\n", tag);
                else                fprintf(stderr, "  %-4s last_ok=%d\n", tag, s->last_ok);
            }
            print_exec_line("IR",  &ir_snap);
            print_exec_line("SM",  &sm_snap);
            print_exec_line("JIT", &jit_snap);
            if (ir_snap.frame_locals_count > 0) {
                fprintf(stderr, "  ICN locals (IR):\n");
                for (int li = 0; li < ir_snap.frame_locals_count; li++) {
                    const char *v = VARVAL_fn(ir_snap.frame_locals[li].val);
                    fprintf(stderr, "    %-16s = %s\n",
                            ir_snap.frame_locals[li].name, v ? v : "");
                }
            }
            if (ir_snap.pl_locals_count > 0) {
                fprintf(stderr, "  Prolog bindings (IR):\n");
                for (int pi = 0; pi < ir_snap.pl_locals_count; pi++)
                    fprintf(stderr, "    %-16s = %s\n",
                            ir_snap.pl_locals[pi].name,
                            ir_snap.pl_locals[pi].val_str);
            }
            if (ir_sm) {
                fprintf(stderr, "  IR vs SM (%d var(s) differ):\n", ir_sm);
                snap_diff(&ir_snap, "IR", &sm_snap, "SM", 1);
            }
            if (ir_jit) {
                fprintf(stderr, "  IR vs JIT (%d var(s) differ):\n", ir_jit);
                snap_diff(&ir_snap, "IR", &jit_snap, "JIT", 1);
            }
            if (ok_diverge && !ir_sm && !ir_jit)
                fprintf(stderr, "  SM last_ok=%d vs JIT last_ok=%d (NV agrees)\n",
                        sm_snap.last_ok, jit_snap.last_ok);
            if (ir_csn) {
                fprintf(stderr, "  IR vs CSN (%d var(s) differ):\n", ir_csn);
                for (int si = 0; si < csn_count; si++) {
                    for (int ii = 0; ii < ir_snap.nv_count; ii++) {
                        if (strcmp(ir_snap.nv_pairs[ii].name, csn_pairs[si].name) == 0) {
                            const char *iv = VARVAL_fn(ir_snap.nv_pairs[ii].val);
                            if (!iv) iv = "";
                            if (strcmp(iv, csn_pairs[si].val_str) != 0)
                                fprintf(stderr, "    %-16s  IR=%-20s  CSN=%s\n",
                                        csn_pairs[si].name, iv, csn_pairs[si].val_str);
                            break;
                        }
                    }
                }
            }
            diverge_at = n;
            exec_snapshot_free(&ir_snap);
            exec_snapshot_free(&sm_snap);
            exec_snapshot_free(&jit_snap);
            break;
        }
        if (verbose >= 1)
            fprintf(stderr, "  stmt %4d/%d: IR=SM=JIT agree\n", n, nstmts);
        exec_snapshot_free(&ir_snap);
        exec_snapshot_free(&sm_snap);
        exec_snapshot_free(&jit_snap);
        csn_nv_snapshot_free(csn_pairs, csn_count); csn_pairs = NULL;
    }
    if (!diverge_at && verbose >= 1)
        fprintf(stderr, "sync_monitor: all %d statements agree across IR/SM/JIT\n", nstmts);
    free(ir_labels);
    exec_snapshot_free(&ir_path);
    exec_snapshot_free(&sm_path);
    exec_snapshot_free(&jit_path);
    exec_snapshot_free(&baseline);
    SM_seq_free(sm_prog);
    return diverge_at;
}
