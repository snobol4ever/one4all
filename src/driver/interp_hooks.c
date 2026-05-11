/*
 * interp_hooks.c — hook functions and IR print utilities
 *
 * Split from interp.c by RS-3 (GOAL-REWRITE-SCRIP).
 * AUTHORS: Lon Jones Cherryholmes · Claude Sonnet 4.6
 * DATE:    2026-05-02
 */

#include "interp_private.h"

/* ══════════════════════════════════════════════════════════════════════════
 * main
 * ══════════════════════════════════════════════════════════════════════════ */

/* _eval_str_impl_fn — EVAL(string) hook for pattern-context strings.
 * Uses bison parse_expr_pat_from_str (snobol4.tab.c) which produces
 * AST_t with correct AST_e values directly — no CMPILE/CMPND_t bridge.
 *
 * In SM mode, this path remains via interp_eval_pat. interp_eval_pat is
 * SM-safe by construction: all user-function dispatch flows through APPLY_fn
 * (label_table_clear_stmts() in scrip.c nulls the IR label table after
 * sm_lower, so interp_eval's AST_FNC label_lookup always returns NULL and
 * falls through to APPLY_fn → _usercall_hook → RS-11 SM dispatch).
 *
 * eval_node was considered as an SM-mode shortcut but lacks coverage of the
 * pattern-primitive node kinds (AST_LEN, AST_TAB, AST_BREAK, AST_SPAN, AST_ANY,
 * AST_NOTANY, AST_ARB, AST_ARBNO, AST_REM, AST_FAIL, AST_SUCCEED, AST_FENCE, AST_POS, ...)
 * that EVAL('LEN(2)') etc. produce. interp_eval_pat handles these via
 * pat_len()/pat_tab()/etc. helpers; eval_node would silently coerce them
 * to STRING. RS-12 explored the route and reverted. */
DESCR_t _eval_str_impl_fn(const char *s) {
    AST_t *tree = parse_expr_pat_from_str(s);
    if (!tree) return FAILDESCR;
    return interp_eval_pat(tree);
}

DESCR_t _eval_pat_impl_fn(DESCR_t pat) {
    /* Run DT_P pattern against empty subject — used by EVAL_fn for *func() patterns.
     * If function fails at match time, EVAL fails. */
    extern int exec_stmt(const char *, DESCR_t *, DESCR_t, DESCR_t *, int);
    DESCR_t subj = STRVAL("");
    int ok = exec_stmt("", &subj, pat, NULL, 0);
    return ok ? NULVCL : FAILDESCR;
}


/* label_exists — called by LABEL() builtin via sno_set_label_exists_hook.
 * RS-13: in SM mode, the IR label table is cleared by label_table_clear_stmts()
 * after sm_lower, so label_lookup always returns NULL. Use sm_label_pc_lookup
 * against the live SM_Program instead. In IR mode, fall back to label_lookup. */
int _label_exists_fn(const char *name) {
    if (g_current_sm_prog)
        return sm_label_pc_lookup(g_current_sm_prog, name) >= 0;
    return label_lookup(name) != NULL;
}

/* S-10: forward declarations so _usercall_hook can call them */
DESCR_t _builtin_IDENT(DESCR_t *args, int nargs);
DESCR_t _builtin_DIFFER(DESCR_t *args, int nargs);
DESCR_t _builtin_DATA(DESCR_t *args, int nargs);

/* _usercall_hook: calls user functions via call_user_function;
 * for pure builtins (FNCEX_fn && no body label) uses APPLY_fn directly
 * so FAILDESCR propagates correctly (DYN-74: fixes *ident(1,2) in EVAL).
 * U-22: cross-call extension — if name not found in SNO label table,
 * try proc_table (BB_PUMP one-shot) then g_pl_pred_table (BB_ONCE). */
DESCR_t _usercall_hook(const char *name, DESCR_t *args, int nargs) {
    /* S-10 fix: handle scrip.c-only predicates directly so *IDENT(x)/*DIFFER(x)
     * in pattern context correctly fail/succeed via bb_usercall -> g_user_call_hook. */
    if (strcmp(name, "IDENT") == 0)  return _builtin_IDENT(args, nargs);  /* SN-19 */
    if (strcmp(name, "DIFFER") == 0) return _builtin_DIFFER(args, nargs); /* SN-19 */
    if (strcmp(name, "DATA") == 0)   return _builtin_DATA(args, nargs);   /* SN-19 */
    /* ITEM(arr,i) read and ITEM_SET(rhs,arr,i) write — SM emits these for ITEM() syntax.
     * ITEM read: args[0]=arr, args[1]=i, [args[2]=j for 2D].
     * ITEM_SET write: args[0]=rhs, args[1]=arr, args[2]=i, [args[3]=j for 2D]. */
    if (strcmp(name, "ITEM") == 0 && nargs >= 2) {        /* SN-19 */
        if (nargs >= 3) return subscript_get2(args[0], args[1], args[2]);
        return subscript_get(args[0], args[1]);
    }
    if (strcmp(name, "ITEM_SET") == 0 && nargs >= 3) {    /* SN-19 */
        DESCR_t rhs = args[0], arr = args[1], idx = args[2];
        if (nargs >= 4) { subscript_set2(arr, idx, args[3], rhs); }
        else            { subscript_set(arr, idx, rhs); }
        return rhs;
    }
    /* SC-1: DATA constructor/field-accessor/field-mutator dispatch via sc_dat registry.
     * Must precede label lookup so struct names shadow any same-named labels. */
    {
        ScDatType *_dt = sc_dat_find_type(name);
        if (_dt) return sc_dat_construct(_dt, args, nargs);
        int _fi = 0;
        ScDatType *_ft = sc_dat_find_field(name, &_fi);
        if (_ft && nargs >= 1) return sc_dat_field_get(name, args[0]);
        /* Field mutator: fname_SET(rhs, obj) — sm_lower emits this for fname(obj)=rhs.
         * Strip the _SET suffix, look up the field, write through data_field_ptr.
         * SN-19 stage-2: name arrives canonical, suffix emitted canonical, strcmp correct */
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
    /* Check for a body label (user-defined function).
     * RS-13: skip in SM mode — the IR label table is cleared after sm_lower,
     * so label_lookup always returns NULL. SM-bodied functions are dispatched
     * by the RS-11 block below via sm_label_pc_lookup. */
    const char *_entry = FUNC_ENTRY_fn(name);
    const AST_t *_body = NULL;
    if (!g_current_sm_prog) {
        _body = _entry ? label_lookup(_entry) : NULL;
        if (!_body) _body = label_lookup(name);
        if (!_body) {
            char _uf[128]; size_t _fl = strlen(name);
            if (_fl >= sizeof(_uf)) _fl = sizeof(_uf) - 1;
            for (size_t _i = 0; _i <= _fl; _i++)
                _uf[_i] = (char)toupper((unsigned char)name[_i]);
            _body = label_lookup(_uf);
        }
    }
    /* Pure builtin (no body) AND registered as builtin: use APPLY_fn for correct failure */
    if (!_body && FNCEX_fn(name)) {
        /* RS-11: SM-bodied user functions (DEFINE'd, FNCEX_fn=true, no IR body)
         * must fall through to the RS-11 block below for SM dispatch.
         * Only use APPLY_fn for pure C builtins (those with no SM body PC). */
        if (!g_current_sm_prog || sm_label_pc_lookup(g_current_sm_prog, name) < 0)
            return APPLY_fn(name, args, nargs);
    }

    /* ── U-22: cross-language fallback ────────────────────────────────
     * Name not found in SNO label/builtin tables.  Try Icon, then Prolog.
     * This lets SNOBOL4 source call Icon procedures and Prolog predicates
     * by name, the same way the linker resolves an undefined symbol. */
    if (!_body) {
        /* Try Icon proc table (case-sensitive — Icon is case-sensitive) */
        for (int _i = 0; _i < proc_count; _i++) {
            if (strcmp(proc_table[_i].name, name) == 0) {
                /* Call as one-shot: drive the Icon proc and return its value.
                 * proc_table_call returns FAILDESCR if the procedure fails.
                 * CH-17g-call-sites: dispatches via SM expression when entry_pc resolved. */
                return proc_table_call(_i, args, nargs);
            }
        }
        /* Try Prolog pred table: name/arity key, e.g. "color/1" */
        if (g_pl_active) {
            char pl_key[256];
            snprintf(pl_key, sizeof pl_key, "%s/%d", name, nargs);
            AST_t *choice = pl_pred_table_lookup(&g_pl_pred_table, pl_key);
            if (choice) {
                /* Set up Prolog arg Term** from DESCR_t args, drive BB_ONCE */
                Term **pl_args = (nargs > 0) ? pl_env_new(nargs) : NULL;
                for (int _i = 0; _i < nargs; _i++)
                    pl_args[_i] = pl_unified_term_from_expr(
                        /* wrap DESCR_t as a literal AST_t leaf */
                        (args[_i].v == DT_S)
                            ? &(AST_t){ .t = AST_QLIT, .v.sval = (char*)args[_i].s }
                            : &(AST_t){ .t = AST_ILIT, .v.ival = (long)args[_i].s },
                        NULL);
                Term **saved_env = g_pl_env;
                g_pl_env = pl_args;
                /* CH-17e: prefer SM-expression path when entry_pc resolved */
                Pl_PredEntry *_hpe = pl_pred_entry_lookup(pl_key);
                extern SM_Program *g_current_sm_prog;
                bb_node_t root = (_hpe && _hpe->entry_pc >= 0 && g_current_sm_prog != NULL)
                    ? pl_box_choice_pc(_hpe->entry_pc, g_pl_env, nargs)
                    : pl_box_choice(choice, g_pl_env, nargs);
                int ok = bb_broker(root, BB_ONCE, NULL, NULL);
                g_pl_env = saved_env;
                return ok ? INTVAL(1) : FAILDESCR;
            }
        }
    }

    /* RS-11: in SM execution mode, dispatch SM-bodied functions synchronously
     * rather than falling through to call_user_function (IR tree-walk).
     * RS-10's guard was correct in blocking call_user_function, but too broad:
     * it also blocked pattern-context .*func() calls (bb_usercall / NM_CALL)
     * to SM-bodied user functions that have no C builtin entry.
     *
     * For SM-bodied functions: push a call frame onto a fresh SM_State that
     * shares the live NV table, run to completion, read the result and set
     * kw_rtntype so bb_usercall's NRETURN detection works correctly.
     *
     * For functions with no SM body (and no builtin, DATA, Icon, Prolog match
     * above): return FAILDESCR — never enter call_user_function / interp_eval. */
    if (g_current_sm_prog) {
        int body_pc = sm_label_pc_lookup(g_current_sm_prog, name);
        if (body_pc < 0) {
            /* try uppercase */
            char uname[128]; size_t nl = strlen(name);
            if (nl < sizeof(uname)) {
                for (size_t i = 0; i <= nl; i++)
                    uname[i] = (char)toupper((unsigned char)name[i]);
                body_pc = sm_label_pc_lookup(g_current_sm_prog, uname);
            }
        }
        if (body_pc < 0) {
            const char *_entry = FUNC_ENTRY_fn(name);
            if (_entry) body_pc = sm_label_pc_lookup(g_current_sm_prog, _entry);
        }
        if (body_pc < 0) return FAILDESCR; /* no SM body, no builtin → fail cleanly */

        /* Run the SM body synchronously in a nested SM_State.
         * The nested state shares the live NV table (global) but has its own
         * value stack and call stack so it does not corrupt the outer SM run. */
        SM_State nested;
        sm_state_init(&nested);
        nested.pc = body_pc;

        /* Bind params and locals, save originals */
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

        sm_interp_run(g_current_sm_prog, &nested);

        /* Read result and detect return type from kw_rtntype (set by SM_RETURN) */
        DESCR_t result;
        int via_fret  = (strcmp(kw_rtntype, "FRETURN") == 0);
        int via_nret  = (strcmp(kw_rtntype, "NRETURN") == 0);
        if (via_fret) {
            result = FAILDESCR;
        } else if (via_nret) {
            result = NV_GET_fn(retname); /* DT_N cell */
        } else {
            result = NV_GET_fn(retname);
        }

        /* Restore NV */
        for (int k = ns - 1; k >= 0; k--)
            NV_SET_fn(saved_names[k], saved_vals[k]);

        sm_state_free(&nested);
        return result;
    }

    /* User-defined (has body) OR unknown: call_user_function handles both */
    return call_user_function(name, args, nargs);
}


/* ── ir_dump_program — print AST_PROGRAM as IR sexp — one line per statement.
 * SI-6: walks AST_PROGRAM children, reads fields via stmt_attr_find.
 * Accepts CODE_t* for --dump-ir-bison (callers pass sno_parse_ast CODE_t as AST*). */
void ir_dump_program(const AST_t *prog, FILE *f) {
    if (!prog) { fprintf(f, "(NULL-PROGRAM)\n"); return; }
    for (int i = 0; i < prog->n; i++) {
        const AST_t *s = prog->c[i];
        if (!s) continue;
        fprintf(f, "(STMT");
        const char *lbl  = stmt_attr_str(stmt_attr_find(s, ":lbl"));
        int has_eq = stmt_attr_find(s, ":eq") != NULL;
        if (lbl)         fprintf(f, " :lbl %s", lbl);
        if (has_eq)      fprintf(f, " :eq");
        if (s->t == AST_END) fprintf(f, " :end");
        AST_t *subj = stmt_attr_expr(stmt_attr_find(s, ":subj"));
        AST_t *pat  = stmt_attr_expr(stmt_attr_find(s, ":pat"));
        AST_t *repl = stmt_attr_expr(stmt_attr_find(s, ":repl"));
        if (subj) { fprintf(f, " :subj "); ir_print_node(subj, f); }
        if (pat)  { fprintf(f, " :pat ");  ir_print_node(pat, f);  }
        if (repl) { fprintf(f, " :repl "); ir_print_node(repl, f); }
        const char *go  = stmt_attr_str(stmt_attr_find(s, ":go"));
        const char *goS = stmt_attr_str(stmt_attr_find(s, ":goS"));
        const char *goF = stmt_attr_str(stmt_attr_find(s, ":goF"));
        if (go)  fprintf(f, " :go %s",  go);
        if (goS) fprintf(f, " :goS %s", goS);
        if (goF) fprintf(f, " :goF %s", goF);
        fprintf(f, ")\n");
    }
}

/* ── S-10 fix: IDENT/DIFFER wrappers for register_fn ──────────────────────
 * IDENT and DIFFER are scrip.c-only builtins not in the binary runtime's
 * APPLY_fn table.  When *IDENT(x) fires at match time via deferred_call_fn,
 * APPLY_fn("IDENT",...) returns non-FAILDESCR for unknown names → T_FUNC
 * always succeeds.  Registering these wrappers makes APPLY_fn dispatch them
 * correctly so *IDENT(n(x)) fails when n(x) is not the null string. */
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

/* ── EVAL/CODE wrappers ─────────────────────────────────────────────────────
 * EVAL and CODE are not registered in the binary APPLY_fn table.
 * FNCEX_fn("EVAL")=0 so _usercall_hook falls through to call_user_function
 * which finds no body → returns NULVCL (STRING) instead of DT_P.
 * Fix: register wrappers so FNCEX_fn("EVAL")=1 → APPLY_fn dispatches here
 * → EVAL_fn → CONVE_fn → EXPVAL_fn → correct DT_P for pattern expressions. */
extern DESCR_t EVAL_fn(DESCR_t);
extern DESCR_t code(const char *);
DESCR_t _builtin_EVAL(DESCR_t *args, int nargs) {
    if (nargs < 1) return FAILDESCR;
    return EVAL_fn(args[0]);
}
DESCR_t _builtin_CODE(DESCR_t *args, int nargs) {
    if (nargs < 1) return FAILDESCR;
    const char *s = VARVAL_fn(args[0]);
    if (!s || !*s) return FAILDESCR;
    return code(s);
}

/* SC-1: print(v...) — Snocone print builtin: outputs each arg on its own line */
