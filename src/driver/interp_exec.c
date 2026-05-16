#include "interp_private.h"
#define PL_PRED_TABLE_SIZE PL_PRED_TABLE_SIZE_FWD
/*----------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------*/
static int ast_prog_find_idx(const tree_t *prog, const tree_t *stmt)
{
    if (!prog || !stmt) return -1;
    for (int i = 0; i < prog->n; i++)
        if (prog->c[i] == stmt) return i;
    return -1;
}
/*----------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------*/
static inline tree_t        *s_expr(const tree_t *s, const char *tag) {
    return stmt_attr_expr(stmt_attr_find(s, tag)); }
/*----------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------*/
static inline const char   *s_str (const tree_t *s, const char *tag) {
    return stmt_attr_str(stmt_attr_find(s, tag)); }
/*----------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------*/
static inline int           s_has (const tree_t *s, const char *tag) {
    return stmt_attr_find(s, tag) != NULL; }
/*----------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------*/
static inline int           s_int (const tree_t *s, const char *tag) {
    const char *v = s_str(s, tag); return v ? atoi(v) : 0; }
const tree_t *g_exec_prog = NULL;
/*----------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------*/
void execute_program(const tree_t *prog)
{
    NO_AST_WALK_GUARD("execute_program");
    g_exec_prog = prog;
    polyglot_init(prog, polyglot_lang_mask(prog));
    g_lang = 0;
    g_sno_err_active = 1;
    int              ci        = 0;
    int              stno      = 0;
    int              succeeded = 1;
    DESCR_t          subj_val  = NULVCL;
    const char      *subj_name = NULL;
    const char      *target    = NULL;
    int              nch       = prog ? prog->n : 0;
    const tree_t     *s         = NULL;
    int              s_is_end  = 0;
    const char      *s_label   = NULL;
    int              s_lang    = 0;
    int              s_has_eq  = 0;
    tree_t           *s_subject = NULL;
    tree_t           *s_pattern = NULL;
    tree_t           *s_repl    = NULL;
    tree_t           *go_s_attr = NULL;
    tree_t           *go_f_attr = NULL;
    tree_t           *go_u_attr = NULL;
    const char      *goto_s    = NULL;
    const char      *goto_f    = NULL;
    const char      *goto_u    = NULL;
    tree_t           *goto_s_expr = NULL;
    tree_t           *goto_f_expr = NULL;
    tree_t           *goto_u_expr = NULL;
    while (ci < nch) {
        s = prog->c[ci];
        if (!s) { ci++; continue; }
        if (s->t == TT_END) break;
        s_is_end  = (s->t == TT_END);
        s_label   = s_str(s, ":lbl");
        s_lang    = s_int(s, ":lang");
        s_has_eq  = s_has(s, ":eq");
        s_subject = s_expr(s, ":subj");
        s_pattern = s_expr(s, ":pat");
        s_repl    = s_expr(s, ":repl");
        /* PST-SN4-1c: TT_GOTO_S/F/U children */
        go_s_attr   = stmt_goto_find(s, TT_GOTO_S);
        go_f_attr   = stmt_goto_find(s, TT_GOTO_F);
        go_u_attr   = stmt_goto_find(s, TT_GOTO_U);
        goto_s      = goto_node_str(go_s_attr);
        goto_f      = goto_node_str(go_f_attr);
        goto_u      = goto_node_str(go_u_attr);
        goto_s_expr = goto_node_expr(go_s_attr);
        goto_f_expr = goto_node_expr(go_f_attr);
        goto_u_expr = goto_node_expr(go_u_attr);
        if (!s_label && !s_subject && !s_pattern && !s_repl &&
            !goto_u && !goto_u_expr && !goto_s && !goto_s_expr && !goto_f && !goto_f_expr) {
            stno = s_int(s, ":stno");
            kw_stno = stno;
            g_sno_err_stmt = stno;
            ci++;
            continue;
        }
        stno = s_int(s, ":stno");
        comm_stno(stno);
        kw_stno = stno;
        {
            extern void mon_emit_label_bin(int64_t stno);
            mon_emit_label_bin((int64_t)stno);
        }
        {
            static int s_trace_init = 0, s_trace_on = 0;
            if (!s_trace_init) {
                const char *e = getenv("ONE4ALL_STEP_TRACE");
                s_trace_on = (e && e[0] == '1');
                s_trace_init = 1;
            }
            if (s_trace_on)
                fprintf(stderr, "IRSTEP %d stno=%d lang=%d label=%s\n",
                        g_ir_steps_done + 1, stno, s_lang,
                        (s_label && s_label[0]) ? s_label : "-");
        }
        if (g_ir_step_limit > 0 && g_ir_steps_done++ >= g_ir_step_limit)
            longjmp(g_ir_step_jmp, 1);
        if (g_opt_trace)
            fprintf(stderr, "TRACE stmt %d\n", stno);
        int _err = setjmp(g_sno_err_jmp);
        if (_err != 0) {
            if (sno_err_is_terminal(_err)) break;
            succeeded = 0;
            target    = NULL;
            if (goto_f && *goto_f)
                target = goto_f;
            goto do_goto;
        }
        if (s_lang == LANG_ICN || s_lang == LANG_RAKU) {
            ci++; continue;
        }
        if (s_lang == LANG_PL) {
            if (s_subject) {
                int sv_pl = g_pl_active;
                g_pl_active = 1;
                interp_eval(s_subject);
                g_pl_active = sv_pl;
            }
            ci++; continue;
        }
        if (s_subject && (s_subject->t == TT_CHOICE ||
                          s_subject->t == TT_UNIFY  ||
                          s_subject->t == TT_CLAUSE)) {
            ci++; continue;
        }
        subj_val  = NULVCL;
        subj_name = NULL;
        if (s_subject) {
            if (s_subject->t == TT_VAR && s_subject->v.sval) {
                subj_name = s_subject->v.sval;
                if (s_pattern)
                    subj_val = NV_GET_fn(subj_name);
            } else if (s_subject->t == TT_INDIRECT && s_subject->n > 0) {
                tree_t *ic = s_subject->c[0];
                if (ic->t == TT_QLIT && ic->v.sval) {
                    subj_name = ic->v.sval;
                } else if (ic->t == TT_VAR && ic->v.sval) {
                    DESCR_t xv = NV_GET_fn(ic->v.sval);
                    subj_name = VARVAL_fn(xv);
                } else {
                    DESCR_t nd = interp_eval(ic);
                    subj_name = VARVAL_fn(nd);
                }
                if (subj_name) {
                    char *fn = GC_strdup(subj_name); sno_fold_name(fn);
                    subj_name = fn;
                }
                if (subj_name && s_pattern) {
                    subj_val = NV_GET_fn(subj_name);
                } else if (!subj_name)
                    subj_val = interp_eval(s_subject);
            } else if (s_subject->t == TT_FNC && s_has_eq && !s_pattern) {
            } else {
                subj_val = interp_eval(s_subject);
            }
        }
        succeeded = 1;
        if (s_pattern) {
            DESCR_t pat_d = interp_eval_pat(s_pattern);
            if (g_opt_dump_bb && pat_d.v == DT_P && pat_d.p)
                patnd_print((PATND_t *)pat_d.p, stderr);
            if (IS_FAIL_fn(pat_d)) {
                succeeded = 0;
            } else {
                DESCR_t  repl_val;
                int      has_repl = 0;
                if (s_has_eq && s_repl) {
                    repl_val = interp_eval(s_repl);
                    has_repl = !IS_FAIL_fn(repl_val);
                } else if (s_has_eq) {
                    repl_val = NULVCL;
                    has_repl = 1;
                }
                Σ = subj_name ? subj_name : "";
                succeeded = exec_stmt(
                    subj_name,
                    subj_name ? NULL : &subj_val,
                    pat_d,
                    has_repl ? &repl_val : NULL,
                    has_repl);
            }
        } else if (s_has_eq && subj_name) {
            DESCR_t repl_val = s_repl
                ? interp_eval(s_repl)
                : NULVCL;
            if (IS_FAIL_fn(repl_val)) {
                succeeded = 0;
            } else {
                set_and_trace(subj_name, repl_val);
                succeeded = 1;
            }
        } else if (s_has_eq && s_subject &&
                   s_subject->t == TT_IDX) {
            tree_t *idx_e = s_subject;
            if (idx_e->n >= 2) {
                DESCR_t base = interp_eval(idx_e->c[0]);
                DESCR_t idx  = interp_eval(idx_e->c[1]);
                DESCR_t repl_val = s_repl ? interp_eval(s_repl) : NULVCL;
                if (IS_FAIL_fn(base) || IS_FAIL_fn(idx) || IS_FAIL_fn(repl_val)) {
                    succeeded = 0;
                } else {
                    if (idx_e->n >= 3) {
                        DESCR_t idx2 = interp_eval(idx_e->c[2]);
                        subscript_set2(base, idx, idx2, repl_val);
                    } else {
                        subscript_set(base, idx, repl_val);
                    }
                    { const char *base_nm = (idx_e->c[0] &&
                                             idx_e->c[0]->t == TT_VAR)
                                           ? idx_e->c[0]->v.sval : NULL;
                      if (base_nm) comm_var(base_nm, repl_val); }
                    succeeded = 1;
                }
            } else { succeeded = 0; }
        } else if (s_has_eq && s_subject &&
                   s_subject->t == TT_KEYWORD && s_subject->v.sval) {
            DESCR_t repl_val = s_repl ? interp_eval(s_repl) : NULVCL;
            if (IS_FAIL_fn(repl_val)) {
                succeeded = 0;
            } else {
                g_kw_ctx = 1;
                NV_SET_fn(s_subject->v.sval, repl_val);
                g_kw_ctx = 0;
                succeeded = 1;
            }
        } else if (s_has_eq && s_subject &&
                   s_subject->t == TT_INDIRECT) {
            tree_t *ichild = s_subject->n > 0 ? s_subject->c[0] : NULL;
            DESCR_t repl_val = s_repl ? interp_eval(s_repl) : NULVCL;
            if (IS_FAIL_fn(repl_val)) {
                succeeded = 0;
            } else {
                DESCR_t name_d = ichild ? interp_eval(ichild) : NULVCL;
                const char *nm0 = VARVAL_fn(name_d);
                if (!nm0 || !*nm0) {
                    succeeded = 0;
                } else {
                    char *nm = GC_strdup(nm0); sno_fold_name(nm);
                    set_and_trace(nm, repl_val);
                    succeeded = 1;
                }
            }
        } else if (s_has_eq && s_subject &&
                   s_subject->t == TT_FNC && s_subject->v.sval &&
                   s_subject->n >= 1) {
            DESCR_t repl_val = s_repl ? interp_eval(s_repl) : NULVCL;
            if (IS_FAIL_fn(repl_val)) {
                succeeded = 0;
            } else if (strcmp(s_subject->v.sval, "ITEM") == 0 &&
                       s_subject->n >= 2) {
                DESCR_t base = interp_eval(s_subject->c[0]);
                DESCR_t idx  = interp_eval(s_subject->c[1]);
                if (IS_FAIL_fn(base) || IS_FAIL_fn(idx)) {
                    succeeded = 0;
                } else if (s_subject->n >= 3) {
                    DESCR_t idx2 = interp_eval(s_subject->c[2]);
                    if (IS_FAIL_fn(idx2)) { succeeded = 0; }
                    else { subscript_set2(base, idx, idx2, repl_val); succeeded = 1; }
                } else {
                    subscript_set(base, idx, repl_val);
                    succeeded = 1;
                }
            } else {
                DESCR_t obj = interp_eval(s_subject->c[0]);
                if (IS_FAIL_fn(obj)) {
                    succeeded = 0;
                } else {
                    FIELD_SET_fn(obj, s_subject->v.sval, repl_val);
                    succeeded = 1;
                }
            }
        } else if (s_has_eq && s_subject &&
                   s_subject->t == TT_FNC && s_subject->v.sval &&
                   s_subject->n == 0) {
            DESCR_t rv = s_repl ? interp_eval(s_repl) : NULVCL;
            if (!IS_FAIL_fn(rv)) {
                DESCR_t fres = call_user_function(s_subject->v.sval, NULL, 0);
                if (NAME_SET(fres, rv)) { succeeded = 1; }
                else { succeeded = 0; }
            } else succeeded = 0;
        } else if (s_subject && !s_pattern && !s_has_eq) {
            if (IS_FAIL_fn(subj_val)) succeeded = 0;
        }
        target = NULL;
        if (goto_u || goto_u_expr || goto_s || goto_s_expr || goto_f || goto_f_expr) {
            if (goto_u && *goto_u)
                target = goto_u;
            else if (goto_u_expr) {
                DESCR_t cv = interp_eval(goto_u_expr);
                target = (cv.v == DT_S && cv.s) ? cv.s : NULL;
            } else if (succeeded && goto_s && *goto_s)
                target = goto_s;
            else if (succeeded && goto_s_expr) {
                DESCR_t cv = interp_eval(goto_s_expr);
                target = (cv.v == DT_S && cv.s) ? cv.s : NULL;
            } else if (!succeeded && goto_f && *goto_f)
                target = goto_f;
            else if (!succeeded && goto_f_expr) {
                DESCR_t cv = interp_eval(goto_f_expr);
                target = (cv.v == DT_S && cv.s) ? cv.s : NULL;
            }
        }
        do_goto:
        if (target) {
            if (strcmp(target, "END") == 0) break;
            if (strcmp(target, "RETURN") == 0 || strcmp(target, "FRETURN") == 0) break;
            const tree_t *dest = label_lookup(target);
            if (dest) {
                int dest_ci = ast_prog_find_idx(prog, dest);
                if (dest_ci >= 0) { ci = dest_ci; continue; }
            }
            sno_runtime_error(24, NULL);
            break;
        }
        ci++;
    }
    if (g_polyglot && g_registry.nmod > 0) {
        for (int _mi = 0; _mi < g_registry.nmod; _mi++) {
            ScripModule *_m = &g_registry.mods[_mi];
            if (_m->lang == LANG_ICN || _m->lang == LANG_RAKU) {
                int _pend = _m->icn_proc_start + _m->proc_count;
                int _found = 0;
                g_lang = 1;
                for (int _pi = _m->icn_proc_start; _pi < _pend && _pi < proc_count; _pi++) {
                    if (strcmp(proc_table[_pi].name, "main") == 0)
                        { proc_table_call(_pi, NULL, 0); _found=1; break; }
                }
                if (!_found)
                    for (int _pi=0; _pi<proc_count; _pi++)
                        if (strcmp(proc_table[_pi].name,"main")==0)
                            { proc_table_call(_pi,NULL,0); break; }
                g_lang = 0;
            } else if (_m->lang == LANG_PL) {
                tree_t *pl_main = pl_pred_table_lookup(&g_pl_pred_table, "main/0");
                if (pl_main) {
                    int sv_pl = g_pl_active; g_pl_active = 1;
                    interp_eval(pl_main);
                    g_pl_active = sv_pl;
                }
            }
        }
        return;
    }
    if (proc_count > 0) {
        for (int _i = 0; _i < proc_count; _i++) {
            if (strcmp(proc_table[_i].name, "main") == 0) {
                proc_table_call(_i, NULL, 0);
                break;
            }
        }
    }
    {
        tree_t *pl_main = pl_pred_table_lookup(&g_pl_pred_table, "main/0");
        if (pl_main) {
            int sv_pl = g_pl_active;
            g_pl_active = 1;
            interp_eval(pl_main);
            g_pl_active = sv_pl;
        }
    }
}
/*----------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------*/
void execute_program_steps(const tree_t *prog, int n) {
    g_ir_step_limit = n;
    g_ir_steps_done = 0;
    if (setjmp(g_ir_step_jmp) == 0)
        execute_program(prog);
    g_ir_step_limit = 0;
    g_ir_steps_done = 0;
}
