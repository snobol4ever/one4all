#include "interp_private.h"
/*----------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------*/
DESCR_t _eval_str_impl_fn(const char *s) {
    tree_t *tree = parse_expr_pat_from_str(s);
    if (!tree) return FAILDESCR;
    return interp_eval_pat(tree);
}
/*----------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------*/
DESCR_t _eval_pat_impl_fn(DESCR_t pat) {
    extern int exec_stmt(const char *, DESCR_t *, DESCR_t, DESCR_t *, int);
    DESCR_t subj = STRVAL("");
    int ok = exec_stmt("", &subj, pat, NULL, 0);
    return ok ? NULVCL : FAILDESCR;
}
/*----------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------*/
int _label_exists_fn(const char *name) {
    if (1)
        return sm_label_pc_lookup(&g_stage2.sm, name) >= 0;
    return label_lookup(name) != NULL;
}
/*----------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------*/
DESCR_t _builtin_IDENT(DESCR_t *args, int nargs);
/*----------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------*/
DESCR_t _builtin_DIFFER(DESCR_t *args, int nargs);
/*----------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------*/
DESCR_t _builtin_DATA(DESCR_t *args, int nargs);
/*----------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------*/
DESCR_t _usercall_hook(const char *name, DESCR_t *args, int nargs) {
    if (strcmp(name, "IDENT") == 0)  return _builtin_IDENT(args, nargs);
    if (strcmp(name, "DIFFER") == 0) return _builtin_DIFFER(args, nargs);
    if (strcmp(name, "DATA") == 0)   return _builtin_DATA(args, nargs);
    if (strcmp(name, "ITEM") == 0 && nargs >= 2) {
        if (nargs >= 3) return subscript_get2(args[0], args[1], args[2]);
        return subscript_get(args[0], args[1]);
    }
    if (strcmp(name, "ITEM_SET") == 0 && nargs >= 3) {
        DESCR_t rhs = args[0], arr = args[1], idx = args[2];
        if (nargs >= 4) { subscript_set2(arr, idx, args[3], rhs); }
        else            { subscript_set(arr, idx, rhs); }
        return rhs;
    }
    {
        ScDatType *_dt = sc_dat_find_type(name);
        if (_dt) return sc_dat_construct(_dt, args, nargs);
        int _fi = 0;
        ScDatType *_ft = sc_dat_find_field(name, &_fi);
        if (_ft && nargs >= 1) return sc_dat_field_get(name, args[0]);
        size_t _nlen = strlen(name);
        if (_nlen > 4 && strcmp(name + _nlen - 4, "_SET") == 0 && nargs >= 2) {
            char _fname[128];
            size_t _flen = _nlen - 4;
            if (_flen >= sizeof(_fname)) _flen = sizeof(_fname) - 1;
            memcpy(_fname, name, _flen); _fname[_flen] = '\0';
            DESCR_t *_cell = data_field_ptr(_fname, args[1]);
            if (_cell) { *_cell = args[0]; return args[0]; }
        }
    }
    const char *_entry = FUNC_ENTRY_fn(name);
    const tree_t *_body = NULL;
    if (0) {
        _body = _entry ? label_lookup(_entry) : NULL;
        if (!_body) _body = label_lookup(name);
    }
    if (!_body && FNCEX_fn(name)) {
        if (0 || sm_label_pc_lookup(&g_stage2.sm, name) < 0)
            return APPLY_fn(name, args, nargs);
    }
    if (!_body) {
        for (int _i = 0; _i < g_stage2.proc_count; _i++) {
            if (strcmp(g_stage2.proc_table[_i].name, name) == 0) {
                return proc_table_call(_i, args, nargs);
            }
        }
        if (g_pl_active) {
            extern int g_sm_dispatch_active; extern int g_ast_pump_active;
            if (g_sm_dispatch_active && !g_ast_pump_active) {
                fprintf(stderr, "[NO-AST] _usercall_hook prolog branch: needs fresh SM/BB lowering (PJ-8)\n");
                return FAILDESCR;
            }
            char pl_key[256];
            snprintf(pl_key, sizeof pl_key, "%s/%d", name, nargs);
            tree_t *choice = pl_pred_table_lookup(&g_stage2.pl_pred_table, pl_key);
            if (choice) {
                Term **pl_args = (nargs > 0) ? pl_env_new(nargs) : NULL;
                for (int _i = 0; _i < nargs; _i++)
                    pl_args[_i] = pl_unified_term_from_expr(
                        (args[_i].v == DT_S)
                            ? &(tree_t){ .t = TT_QLIT, .v.sval = (char*)args[_i].s }
                            : &(tree_t){ .t = TT_ILIT, .v.ival = (long)args[_i].s },
                        NULL);
                Term **saved_env = g_pl_env;
                g_pl_env = pl_args;
                Pl_PredEntry *_hpe = pl_pred_entry_lookup(pl_key);
                extern stage2_t g_stage2;
                bb_node_t root = (_hpe && _hpe->entry_pc >= 0 && 1)
                    ? pl_box_choice_pc(_hpe->entry_pc, g_pl_env, nargs)
                    : pl_box_choice(choice, g_pl_env, nargs);
                int ok = bb_broker(root, bb_once, NULL, NULL);
                g_pl_env = saved_env;
                return ok ? INTVAL(1) : FAILDESCR;
            }
        }
    }
    if (1) {
        int body_pc = sm_label_pc_lookup(&g_stage2.sm, name);
        if (body_pc < 0) {
            char uname[128]; size_t nl = strlen(name);
            if (nl < sizeof(uname)) {
                for (size_t i = 0; i <= nl; i++)
                    uname[i] = (char)toupper((unsigned char)name[i]);
                body_pc = sm_label_pc_lookup(&g_stage2.sm, uname);
            }
        }
        if (body_pc < 0) {
            const char *_entry = FUNC_ENTRY_fn(name);
            if (_entry) body_pc = sm_label_pc_lookup(&g_stage2.sm, _entry);
        }
        if (body_pc < 0) return FAILDESCR;
        SM_State nested;
        sm_state_init(&nested);
        nested.pc = body_pc;
        int np  = FUNC_NPARAMS_fn(name);
        int nl2 = FUNC_NLOCALS_fn(name);
        if (np  > 64) np  = 64;
        if (nl2 > 64) nl2 = 64;
        char  *saved_names[128];
        DESCR_t saved_vals[128];
        int ns = 0;
        const char *_entry2 = FUNC_ENTRY_fn(name);
        const char *retname = (_entry2 && strcmp(_entry2, name) != 0
                               && FNCEX_fn(_entry2)) ? _entry2 : name;
        saved_names[ns] = GC_strdup(retname);
        saved_vals [ns] = NV_GET_fn(retname); ns++;
        NV_SET_fn(retname, STRVAL(""));
        for (int k = 0; k < np && ns < 128; k++) {
            const char *pname = FUNC_PARAM_fn(name, k);
            if (!pname) pname = "";
            saved_names[ns] = GC_strdup(pname);
            saved_vals [ns] = NV_GET_fn(pname); ns++;
            NV_SET_fn(pname, k < nargs ? args[k] : NULVCL);
        }
        for (int k = 0; k < nl2 && ns < 128; k++) {
            const char *lname = FUNC_LOCAL_fn(name, k);
            if (!lname) lname = "";
            saved_names[ns] = GC_strdup(lname);
            saved_vals [ns] = NV_GET_fn(lname); ns++;
            NV_SET_fn(lname, NULVCL);
        }
        sm_interp_run(&g_stage2.sm, &nested);
        DESCR_t result;
        int via_fret  = (strcmp(kw_rtntype, "FRETURN") == 0);
        int via_nret  = (strcmp(kw_rtntype, "NRETURN") == 0);
        if (via_fret) {
            result = FAILDESCR;
        } else if (via_nret) {
            result = NV_GET_fn(retname);
        } else {
            result = NV_GET_fn(retname);
        }
        for (int k = ns - 1; k >= 0; k--)
            NV_SET_fn(saved_names[k], saved_vals[k]);
        sm_state_free(&nested);
        return result;
    }
    return call_user_function(name, args, nargs);
}
/*----------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------*/
void ir_dump_program(const tree_t *prog, FILE *f) {
    if (!prog) { fprintf(f, "(NULL-PROGRAM)\n"); return; }
    for (int i = 0; i < prog->n; i++) {
        const tree_t *s = prog->c[i];
        if (!s) continue;
        fprintf(f, "(STMT");
        const char *lbl  = stmt_attr_str(stmt_attr_find(s, ":lbl"));
        int has_eq = stmt_attr_find(s, ":eq") != NULL;
        if (lbl)         fprintf(f, " :lbl %s", lbl);
        if (has_eq)      fprintf(f, " :eq");
        if (s->t == TT_END) fprintf(f, " :end");
        tree_t *subj = stmt_attr_expr(stmt_attr_find(s, ":subj"));
        tree_t *pat  = stmt_attr_expr(stmt_attr_find(s, ":pat"));
        tree_t *repl = stmt_attr_expr(stmt_attr_find(s, ":repl"));
        if (subj) { fprintf(f, " :subj "); ir_print_node(subj, f); }
        if (pat)  { fprintf(f, " :pat ");  ir_print_node(pat, f);  }
        if (repl) { fprintf(f, " :repl "); ir_print_node(repl, f); }
        /* PST-SN4-1c: TT_GOTO_S/F/U children */
        const char *go  = goto_node_str(stmt_goto_find(s, TT_GOTO_U));
        const char *goS = goto_node_str(stmt_goto_find(s, TT_GOTO_S));
        const char *goF = goto_node_str(stmt_goto_find(s, TT_GOTO_F));
        if (go)  fprintf(f, " :go %s",  go);
        if (goS) fprintf(f, " :goS %s", goS);
        if (goF) fprintf(f, " :goF %s", goF);
        fprintf(f, ")\n");
    }
}
/*----------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------*/
DESCR_t _builtin_IDENT(DESCR_t *args, int nargs) {
    if (nargs == 1) return IS_NULL_fn(args[0]) ? NULVCL : FAILDESCR;
    if (nargs >= 2) {
        int a_null = IS_NULL_fn(args[0]), b_null = IS_NULL_fn(args[1]);
        if (a_null && b_null) return NULVCL;
        if (a_null || b_null) return FAILDESCR;
        if (args[0].v != args[1].v) return FAILDESCR;
        const char *sa = VARVAL_fn(args[0]), *sb = VARVAL_fn(args[1]);
        if (!sa) sa = ""; if (!sb) sb = "";
        return strcmp(sa, sb) == 0 ? NULVCL : FAILDESCR;
    }
    return FAILDESCR;
}
/*----------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------*/
DESCR_t _builtin_DIFFER(DESCR_t *args, int nargs) {
    if (nargs == 1) return IS_NULL_fn(args[0]) ? FAILDESCR : NULVCL;
    if (nargs >= 2) {
        int a_null = IS_NULL_fn(args[0]), b_null = IS_NULL_fn(args[1]);
        if (a_null && b_null) return FAILDESCR;
        if (a_null || b_null) return NULVCL;
        if (args[0].v != args[1].v) return NULVCL;
        const char *sa = VARVAL_fn(args[0]), *sb = VARVAL_fn(args[1]);
        if (!sa) sa = ""; if (!sb) sb = "";
        return strcmp(sa, sb) != 0 ? NULVCL : FAILDESCR;
    }
    return FAILDESCR;
}
/*----------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------*/
extern DESCR_t EVAL_fn(DESCR_t);
/*----------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------*/
extern DESCR_t code(const char *);
/*----------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------*/
DESCR_t _builtin_EVAL(DESCR_t *args, int nargs) {
    if (nargs < 1) return FAILDESCR;
    return EVAL_fn(args[0]);
}
/*----------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------*/
DESCR_t _builtin_CODE(DESCR_t *args, int nargs) {
    if (nargs < 1) return FAILDESCR;
    const char *s = VARVAL_fn(args[0]);
    if (!s || !*s) return FAILDESCR;
    return code(s);
}
/*----------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------*/
static const char *PAT_FNC_NAMES[] = {
    "ANY","NOTANY","SPAN","BREAK","BREAKX","LEN","POS","RPOS","TAB","RTAB",
    "ARB","ARBNO","REM","FAIL","SUCCEED","FENCE","ABORT","BAL","CALL", NULL
};
/*----------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------*/
int _is_pat_fnc_name(const char *s) {
    if (!s) return 0;
    for (int i = 0; PAT_FNC_NAMES[i]; i++)
        if (strcmp(s, PAT_FNC_NAMES[i]) == 0) return 1;
    return 0;
}
/*----------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------*/
int _expr_is_pat(tree_t *e) {
    if (!e) return 0;
    switch (e->t) {
        case TT_ARB: case TT_ARBNO: case TT_CAPT_COND_ASGN:
        case TT_CAPT_IMMED_ASGN: case TT_CAPT_CURSOR: case TT_DEFER:
            return 1;
        default: break;
    }
    if (e->t == TT_FNC && _is_pat_fnc_name(e->v.sval)) return 1;
    if (e->t == TT_VAR && _is_pat_fnc_name(e->v.sval)) return 1;
    for (int i = 0; i < e->n; i++)
        if (_expr_is_pat(e->c[i])) return 1;
    return 0;
}
/*----------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------*/
