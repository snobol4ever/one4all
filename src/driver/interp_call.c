#include "interp_private.h"
CallFrame  call_stack[CALL_STACK_MAX];
int        call_depth = 0;
IcnInitEnt init_tab[ICN_INIT_MAX];
int        icn_init_n = 0;
/*----------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------*/
void icn_init_update_snapshot(char **snames, DESCR_t *svals, int nsaved) {
    for (int ei = 0; ei < icn_init_n; ei++) {
        IcnInitEnt *ent = &init_tab[ei];
        for (int si = 0; si < ent->ns; si++) {
            for (int ni = 0; ni < nsaved; ni++) {
                if (snames[ni] && strcmp(snames[ni], ent->s[si].nm) == 0) {
                    ent->s[si].val = NV_GET_fn(ent->s[si].nm);
                    break;
                }
            }
        }
    }
}
/*----------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------*/
void icn_init_save_frame(void) {
    if (frame_depth <= 0) return;
    IcnFrame *f = &frame_stack[frame_depth - 1];
    for (int ei = 0; ei < icn_init_n; ei++) {
        IcnInitEnt *ent = &init_tab[ei];
        for (int si = 0; si < ent->ns; si++) {
            int slot = scope_get(&f->sc, ent->s[si].nm);
            if (slot >= 0 && slot < f->env_n) {
                ent->s[si].val = f->env[slot];
            } else {
                ent->s[si].val = NV_GET_fn(ent->s[si].nm);
            }
        }
    }
}
/*----------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------*/
int shadow_get(const char *name, DESCR_t *out) {
    for (int d = call_depth - 1; d >= 0; d--) {
        CallFrame *fr = &call_stack[d];
        for (int j = 0; j < fr->nshadow; j++)
            if (strcmp(fr->shadow[j].name, name) == 0) { *out = fr->shadow[j].val; return 1; }
    }
    return 0;
}
/*----------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------*/
void shadow_set_cur(const char *name, DESCR_t val) {
    if (call_depth <= 0) return;
    CallFrame *fr = &call_stack[call_depth - 1];
    for (int j = 0; j < fr->nshadow; j++)
        if (strcmp(fr->shadow[j].name, name) == 0) { fr->shadow[j].val = val; return; }
    if (fr->nshadow < SHADOW_MAX) {
        strncpy(fr->shadow[fr->nshadow].name, name, 63);
        fr->shadow[fr->nshadow].name[63] = '\0';
        fr->shadow[fr->nshadow].val = val;
        fr->nshadow++;
    }
}
/*----------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------*/
int shadow_has(const char *name) {
    for (int d = call_depth - 1; d >= 0; d--) {
        CallFrame *fr = &call_stack[d];
        for (int j = 0; j < fr->nshadow; j++)
            if (strcmp(fr->shadow[j].name, name) == 0) return 1;
    }
    return 0;
}
/*----------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------*/
/* is_current_frame_local: returns 1 if `name` is bound as the current frame's
 * function-return slot, a parameter, or a local. Used to suppress the TT_VAR
 * fallback in interp_eval that would otherwise treat a parameter named the
 * same as the function (e.g. `DEFINE('upr(upr)')` SNOBOL4 idiom) as a
 * recursive function call when its value is NULVCL.
 *
 * Walks `saved_names[]` which the call setup pre-populates with the
 * retname (index 0) + parameter names (indices 1..np) + local names
 * (indices np+1..np+nl). */
int is_current_frame_local(const char *name) {
    if (call_depth <= 0 || !name) return 0;
    CallFrame *fr = &call_stack[call_depth - 1];
    if (!fr->saved_names) return 0;
    for (int i = 0; i < fr->nsaved; i++) {
        if (fr->saved_names[i] && strcmp(fr->saved_names[i], name) == 0)
            return 1;
    }
    return 0;
}
/*----------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------*/
DESCR_t call_user_function(const char *fname, DESCR_t *args, int nargs)
{
    NO_AST_WALK_GUARD("call_user_function");
    if (call_depth >= CALL_STACK_MAX) return FAILDESCR;
    int np = FUNC_NPARAMS_fn(fname);
    int nl = FUNC_NLOCALS_fn(fname);
    char *pnames[64]; if (np > 64) np = 64;
    char *lnames[64]; if (nl > 64) nl = 64;
    for (int i = 0; i < np; i++) {
        const char *p = FUNC_PARAM_fn(fname, i);
        pnames[i] = p ? GC_strdup(p) : GC_strdup("");
    }
    for (int i = 0; i < nl; i++) {
        const char *l = FUNC_LOCAL_fn(fname, i);
        lnames[i] = l ? GC_strdup(l) : GC_strdup("");
    }
    char ufname[128];
    {
        size_t flen = strlen(fname);
        if (flen >= sizeof(ufname)) flen = sizeof(ufname)-1;
        for (size_t i = 0; i <= flen; i++) ufname[i] = fname[i];
    }
    const char *entry_pre = FUNC_ENTRY_fn(fname);
    const char *retname = fname;
    if (entry_pre && strcmp(entry_pre, fname) != 0 && FNCEX_fn(entry_pre))
        retname = entry_pre;
    comm_call(retname);
    monitor_quiet_depth++;
    int nsaved = 1 + np + nl;
    char   **snames = GC_malloc((size_t)nsaved * sizeof(char *));
    DESCR_t *svals  = GC_malloc((size_t)nsaved * sizeof(DESCR_t));
    snames[0] = GC_strdup(retname);
    svals[0]  = NV_GET_fn(retname);
    NV_SET_fn(retname, STRVAL(""));    /* BUG-QIZE: clear return slot to empty string,
                                        * not NULVCL, so DIFFER(retvar) fails on entry.
                                        * SPITBOL treats cleared retname as "" (zero-length
                                        * string); NULVCL has a type tag that makes DIFFER
                                        * succeed (non-null) → divergence on Qize body. */
    for (int i = 0; i < np; i++) {
        snames[1+i] = pnames[i];
        if (strcmp(pnames[i], retname) == 0)
            svals[1+i] = svals[0];
        else
            svals[1+i] = NV_GET_fn(pnames[i]);
        NV_SET_fn(pnames[i], (i < nargs) ? args[i] : NULVCL);
    }
    for (int i = 0; i < nl; i++) {
        snames[1+np+i] = lnames[i];
        svals[1+np+i]  = NV_GET_fn(lnames[i]);
        NV_SET_fn(lnames[i], NULVCL);
    }
    monitor_quiet_depth--;
    CallFrame *fr = &call_stack[call_depth++];
    kw_fnclevel = call_depth;
    strncpy(fr->fname, retname, sizeof(fr->fname)-1);
    fr->fname[sizeof(fr->fname)-1] = '\0';
    fr->nshadow = 0;
    for (int i = 0; i < np; i++)
        if (_is_pat_fnc_name(pnames[i]))
            shadow_set_cur(pnames[i], (i < nargs) ? args[i] : NULVCL);
    for (int i = 0; i < nl; i++)
        if (_is_pat_fnc_name(lnames[i]))
            shadow_set_cur(lnames[i], NULVCL);
    fr->saved_names = snames;
    fr->saved_vals  = svals;
    fr->nsaved      = nsaved;
    fr->retval_cell = STRVAL("");
    fr->retval_set  = 0;
    DESCR_t retval = NULVCL;
    const char *saved_Σ    = Σ;
    int         saved_Δ    = Δ;
    int         saved_Ω    = Ω;
    int         saved_Σlen = Σlen;
    int ret_kind = setjmp(fr->ret_env);
    if (ret_kind == 0) {
        const char *entry = FUNC_ENTRY_fn(fname);
        const tree_t *body = entry ? label_lookup(entry) : NULL;
        if (!body) body = label_lookup(fname);
        if (!body) body = label_lookup(ufname);
        if (!body && !FNCEX_fn(fname) && !FNCEX_fn(ufname)) {
            sno_runtime_error(5, NULL);
            retval = FAILDESCR;
            goto fn_done;
        }
        if (body && g_exec_prog) {
            int ci = 0;
            for (int _i = 0; _i < g_exec_prog->n; _i++)
                if (g_exec_prog->c[_i] == body) { ci = _i; break; }
            int nch = g_exec_prog->n;
            while (ci < nch) {
                const tree_t *s = g_exec_prog->c[ci];
                if (!s) { ci++; continue; }
                if (s->t == TT_END) break;
                if (s->t != TT_STMT) { ci++; continue; }
                tree_t *s_subject = stmt_attr_expr(stmt_attr_find(s, ":subj"));
                tree_t *s_pattern = stmt_attr_expr(stmt_attr_find(s, ":pat"));
                tree_t *s_repl    = stmt_attr_expr(stmt_attr_find(s, ":repl"));
                int    s_has_eq  = stmt_attr_find(s, ":eq") != NULL;
                int    s_stno    = 0;
                { const char *_v = stmt_attr_str(stmt_attr_find(s, ":stno")); if (_v) s_stno = atoi(_v); }
                if (s_subject && (s_subject->t == TT_CHOICE ||
                                  s_subject->t == TT_UNIFY  ||
                                  s_subject->t == TT_CLAUSE)) {
                    ci++; continue;
                }
                {
                    extern void mon_emit_label_bin(int64_t stno);
                    mon_emit_label_bin((int64_t)s_stno);
                }
                DESCR_t     subj_val  = NULVCL;
                const char *subj_name = NULL;
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
                            if (IS_NAMEPTR(xv)) {
                                const char *_rn = NV_name_from_ptr((const DESCR_t*)xv.ptr);
                                subj_name = _rn ? _rn : NULL;
                            } else if (xv.v == DT_N && xv.slen == 0 && xv.s) {
                                subj_name = xv.s;
                            } else {
                                subj_name = VARVAL_fn(xv);
                            }
                        } else {
                            DESCR_t nd = interp_eval(ic);
                            subj_name = VARVAL_fn(nd);
                        }
                        if (subj_name) {
                            char *fn = GC_strdup(subj_name);
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
                int succeeded = 1;
                if (s_pattern) {
                    DESCR_t pat_d = interp_eval_pat(s_pattern);
                    if (IS_FAIL_fn(pat_d)) {
                        succeeded = 0;
                    } else {
                        DESCR_t repl_val; int has_repl = 0;
                        if (s_has_eq && s_repl) {
                            repl_val = interp_eval(s_repl);
                            has_repl = !IS_FAIL_fn(repl_val);
                        }
                        Σ = subj_name ? subj_name : "";
                        succeeded = exec_stmt(subj_name,
                            subj_name ? NULL : &subj_val,
                            pat_d, has_repl ? &repl_val : NULL, has_repl);
                    }
                } else if (s_has_eq && subj_name) {
                    DESCR_t repl_val = s_repl ? interp_eval(s_repl) : NULVCL;
                    if (strcmp(kw_rtntype, "NRETURN") == 0
                            && s_repl && s_repl->t == TT_FNC && s_repl->v.sval) {
                        DESCR_t raw = NV_GET_fn(s_repl->v.sval);
                        if (IS_NAME(raw)) repl_val = raw;
                    }
                    if (IS_FAIL_fn(repl_val)) succeeded = 0;
                    else {
                        if (call_depth == 0 && FNCEX_fn(subj_name)
                                && FUNC_NPARAMS_fn(subj_name) == 0) {
                            DESCR_t fres = call_user_function(subj_name, NULL, 0);
                            if (NAME_SET(fres, repl_val)) { succeeded = 1; }
                            else { set_and_trace(subj_name, repl_val); succeeded = 1; }
                        } else { set_and_trace(subj_name, repl_val); succeeded = 1; }
                    }
                } else if (s_has_eq && s_subject && s_subject->t == TT_KEYWORD && s_subject->v.sval) {
                    DESCR_t repl_val = s_repl ? interp_eval(s_repl) : NULVCL;
                    if (IS_FAIL_fn(repl_val)) succeeded = 0;
                    else {
                        if (!ASGNIC_fn(s_subject->v.sval, repl_val))
                            NV_SET_fn(s_subject->v.sval, repl_val);
                        succeeded = 1;
                    }
                } else if (s_has_eq && s_subject && s_subject->t == TT_IDX &&
                           s_subject->n >= 2) {
                    DESCR_t base = interp_eval(s_subject->c[0]);
                    DESCR_t idx  = interp_eval(s_subject->c[1]);
                    DESCR_t rv   = s_repl ? interp_eval(s_repl) : NULVCL;
                    if (IS_FAIL_fn(base)||IS_FAIL_fn(idx)||IS_FAIL_fn(rv)) succeeded = 0;
                    else {
                        if (s_subject->n == 3) {
                            DESCR_t idx2 = interp_eval(s_subject->c[2]);
                            subscript_set2(base, idx, idx2, rv);
                        } else { subscript_set(base, idx, rv); }
                        { const char *base_nm = (s_subject->c[0] &&
                                                 s_subject->c[0]->t == TT_VAR)
                                               ? s_subject->c[0]->v.sval : NULL;
                          if (base_nm) comm_var(base_nm, rv); }
                        succeeded = 1;
                    }
                } else if (s_has_eq && s_subject && s_subject->t == TT_FNC &&
                           s_subject->v.sval && s_subject->n >= 1) {
                    DESCR_t rv = s_repl ? interp_eval(s_repl) : NULVCL;
                    if (!IS_FAIL_fn(rv)) {
                        if (strcmp(s_subject->v.sval, "ITEM") == 0 && s_subject->n >= 2) {
                            DESCR_t base = interp_eval(s_subject->c[0]);
                            DESCR_t idx  = interp_eval(s_subject->c[1]);
                            if (!IS_FAIL_fn(base) && !IS_FAIL_fn(idx)) {
                                if (s_subject->n >= 3) {
                                    DESCR_t idx2 = interp_eval(s_subject->c[2]);
                                    if (!IS_FAIL_fn(idx2)) subscript_set2(base, idx, idx2, rv);
                                } else {
                                    subscript_set(base, idx, rv);
                                }
                                succeeded = 1;
                            } else succeeded = 0;
                        } else {
                            DESCR_t obj = interp_eval(s_subject->c[0]);
                            if (!IS_FAIL_fn(obj)) {
                                FIELD_SET_fn(obj, s_subject->v.sval, rv);
                                comm_var("<lval>", rv);
                                succeeded = 1;
                            } else succeeded = 0;
                        }
                    } else succeeded = 0;
                } else if (s_has_eq && s_subject && s_subject->t == TT_FNC &&
                           s_subject->v.sval && s_subject->n == 0) {
                    DESCR_t fres = call_user_function(s_subject->v.sval, NULL, 0);
                    if (IS_NAME(fres)) {
                        DESCR_t rv = s_repl ? interp_eval(s_repl) : NULVCL;
                        if (IS_FAIL_fn(rv)) succeeded = 0;
                        else { succeeded = NAME_SET(fres, rv) ? 1 : 0; }
                    } else succeeded = 0;
                } else if (s_has_eq && s_subject && s_subject->t == TT_INDIRECT) {
                    tree_t *ichild = s_subject->n > 0 ? s_subject->c[0] : NULL;
                    DESCR_t repl_val = s_repl ? interp_eval(s_repl) : NULVCL;
                    if (IS_FAIL_fn(repl_val)) { succeeded = 0; }
                    else {
                        DESCR_t ind_val = ichild ? interp_eval(ichild) : NULVCL;
                        if (IS_NAMEPTR(ind_val)) {
                            *(DESCR_t*)ind_val.ptr = repl_val;
                            { const char *_rn = NV_name_from_ptr((const DESCR_t*)ind_val.ptr);
                              comm_var(_rn ? _rn : "<lval>", repl_val); }
                            succeeded = 1;
                        } else {
                            const char *nm0 = VARVAL_fn(ind_val);
                            if (!nm0 || !*nm0) { succeeded = 0; }
                            else {
                                char *nm = GC_strdup(nm0);
                                DESCR_t named = NV_GET_fn(nm);
                                if (IS_NAMEPTR(named)) {
                                    NAME_DEREF_PTR(named) = repl_val;
                                    { const char *_rn = NV_name_from_ptr((const DESCR_t*)named.ptr);
                                      comm_var(_rn ? _rn : "<lval>", repl_val); }
                                    succeeded = 1;
                                } else {
                                    set_and_trace(nm, repl_val); succeeded = 1;
                                }
                            }
                        }
                    }
                } else if (s_subject && !s_pattern && !s_has_eq) {
                    if (IS_FAIL_fn(subj_val)) succeeded = 0;
                }
                const char *target = NULL;
                /* PST-SN4-1c: TT_GOTO_S/F/U children */
                tree_t *go_s_attr   = stmt_goto_find(s, TT_GOTO_S);
                tree_t *go_f_attr   = stmt_goto_find(s, TT_GOTO_F);
                tree_t *go_u_attr   = stmt_goto_find(s, TT_GOTO_U);
                const char *goto_s  = goto_node_str(go_s_attr);
                const char *goto_f  = goto_node_str(go_f_attr);
                const char *goto_u  = goto_node_str(go_u_attr);
                tree_t *goto_s_expr = goto_node_expr(go_s_attr);
                tree_t *goto_f_expr = goto_node_expr(go_f_attr);
                tree_t *goto_u_expr = goto_node_expr(go_u_attr);
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
                if (target) {
                    if (strcmp(target, "END") == 0) break;
                    if (strcmp(target, "RETURN") == 0) {
                        retval = fr->retval_set ? fr->retval_cell : NV_GET_fn(fr->fname);
                        strncpy(kw_rtntype, "RETURN",  sizeof(kw_rtntype)-1);
                        goto fn_done;
                    }
                    if (strcmp(target, "FRETURN") == 0) {
                        retval = FAILDESCR;
                        strncpy(kw_rtntype, "FRETURN", sizeof(kw_rtntype)-1);
                        goto fn_done;
                    }
                    if (strcmp(target, "NRETURN") == 0) {
                        retval = fr->retval_set ? fr->retval_cell : NV_GET_fn(fr->fname);
                        strncpy(kw_rtntype, "NRETURN", sizeof(kw_rtntype)-1);
                        goto fn_done;
                    }
                    const tree_t *dest = label_lookup(target);
                    if (dest) {
                        for (int _j = 0; _j < nch; _j++)
                            if (g_exec_prog->c[_j] == dest) { ci = _j; break; }
                        continue;
                    }
                    break;
                }
                ci++;
            }
        }
        retval = fr->retval_set ? fr->retval_cell : NV_GET_fn(fr->fname);
        strncpy(kw_rtntype, "RETURN",  sizeof(kw_rtntype)-1);
    } else if (ret_kind == 1) {
        retval = fr->retval_set ? fr->retval_cell : NV_GET_fn(fr->fname);
        strncpy(kw_rtntype, "RETURN",  sizeof(kw_rtntype)-1);
    } else {
        retval = FAILDESCR;
        strncpy(kw_rtntype, "FRETURN", sizeof(kw_rtntype)-1);
    }
fn_done:
    Σ    = saved_Σ;
    Δ    = saved_Δ;
    Ω    = saved_Ω;
    Σlen = saved_Σlen;
    comm_return(retname, retval);  /* T-2: FUNCTION trace RETURN event;
                                      use retname (body name) not fname
                                      (alias) for OPSYN parity with SPITBOL. */
    icn_init_update_snapshot(snames, svals, nsaved);
    monitor_quiet_depth++;
    for (int i = 0; i < nsaved; i++)
        NV_SET_fn(snames[i], svals[i]);
    monitor_quiet_depth--;
    call_depth--;
    kw_fnclevel = call_depth;
    return retval;
}
