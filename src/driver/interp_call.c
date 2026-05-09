/*
 * interp_call.c — call frame, shadow table, call_user_function
 *
 * Split from interp.c by RS-3 (GOAL-REWRITE-SCRIP).
 * AUTHORS: Lon Jones Cherryholmes · Claude Sonnet 4.6
 * DATE:    2026-05-02
 */

#include "interp_private.h"

/* ══════════════════════════════════════════════════════════════════════════
 * call_stack — RETURN/FRETURN longjmp infrastructure
 * ══════════════════════════════════════════════════════════════════════════ */


/* SN-3: shadow table for params/locals whose names collide with SPITBOL builtins
 * (e.g. LEN, ANY, SPAN — NV_SET_fn cannot override these in the SPITBOL NV store).
 * Checked in AST_VAR and NV_SET before NV_GET_fn/NV_SET_fn. */


CallFrame  call_stack[CALL_STACK_MAX];
int        call_depth = 0;

/* ── IC-5: AST_INITIAL persistence — file-scope table keyed on AST_t node id ── */
IcnInitEnt init_tab[ICN_INIT_MAX];
int        icn_init_n = 0;

/* Called just before NV restore in call_user_function to update snapshots */
void icn_init_update_snapshot(char **snames, DESCR_t *svals, int nsaved) {
    /* For each init entry, check if any tracked var appears in snames (locals).
     * If so, capture its current NV value (pre-restore = end-of-call value). */
    for (int ei = 0; ei < icn_init_n; ei++) {
        IcnInitEnt *ent = &init_tab[ei];
        for (int si = 0; si < ent->ns; si++) {
            for (int ni = 0; ni < nsaved; ni++) {
                if (snames[ni] && strcasecmp(snames[ni], ent->s[si].nm) == 0) {
                    ent->s[si].val = NV_GET_fn(ent->s[si].nm);
                    break;
                }
            }
        }
    }
}

/* IC-5: Save current ICN frame's local values back into init_tab snapshots.
 * Called by coro_call just before popping the frame, so initial-block
 * statics (x in "initial x := 10") persist across calls. */
void icn_init_save_frame(void) {
    if (frame_depth <= 0) return;
    IcnFrame *f = &frame_stack[frame_depth - 1];
    for (int ei = 0; ei < icn_init_n; ei++) {
        IcnInitEnt *ent = &init_tab[ei];
        for (int si = 0; si < ent->ns; si++) {
            /* Find this variable's slot in the current frame scope */
            int slot = scope_get(&f->sc, ent->s[si].nm);
            if (slot >= 0 && slot < f->env_n) {
                ent->s[si].val = f->env[slot];
            } else {
                /* Global variable — read from NV */
                ent->s[si].val = NV_GET_fn(ent->s[si].nm);
            }
        }
    }
}


/* SN-3: shadow table helpers — check active frames top-down
 * SN-19 stage-2: names arrive canonical (folded at lex/ingest), plain strcmp is correct. */
int shadow_get(const char *name, DESCR_t *out) {
    for (int d = call_depth - 1; d >= 0; d--) {
        CallFrame *fr = &call_stack[d];
        for (int j = 0; j < fr->nshadow; j++)
            if (strcmp(fr->shadow[j].name, name) == 0) { *out = fr->shadow[j].val; return 1; }
    }
    return 0;
}
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
int shadow_has(const char *name) {
    for (int d = call_depth - 1; d >= 0; d--) {
        CallFrame *fr = &call_stack[d];
        for (int j = 0; j < fr->nshadow; j++)
            if (strcmp(fr->shadow[j].name, name) == 0) return 1;
    }
    return 0;
}

/* The program being interpreted (set in main before execute_program) */
DESCR_t call_user_function(const char *fname, DESCR_t *args, int nargs)
{
    if (call_depth >= CALL_STACK_MAX) return FAILDESCR;

    /* ── Gather param and local names via source-case accessors ── */
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

    /* fname as uppercase (NV store is case-insensitive but uppercase is canonical) */
    char ufname[128];
    {
        size_t flen = strlen(fname);
        if (flen >= sizeof(ufname)) flen = sizeof(ufname)-1;
        for (size_t i = 0; i <= flen; i++)
            ufname[i] = (char)toupper((unsigned char)fname[i]);
    }

    /* ── Determine retname: the NV variable the body writes its return value into.
     * For a normal call: retname == fname (body writes "fact" = ...).
     * For an OPSYN alias: fname="facto" but entry_label="fact"; the body writes
     * "fact", so we must save/restore "fact" and read it back on RETURN.
     * We use FUNC_ENTRY_fn(fname) as retname whenever it differs from fname
     * (case-insensitively) — that's the canonical body name. ── */
    const char *entry_pre = FUNC_ENTRY_fn(fname);
    const char *retname = fname;
    /* For OPSYN aliases: fname="facto", entry_label="fact" — entry_label IS a registered
     * function whose body writes "fact=...".  Use entry_label as the return-value slot.
     * For alternate-entry: fname="fact2", entry_label="fact2_entry" — entry_label is just
     * a label, NOT a registered function; body still writes "fact2=...".  Use fname.
     * SN-19 stage-2: both names arrive canonical, plain strcmp is correct. */
    if (entry_pre && strcmp(entry_pre, fname) != 0 && FNCEX_fn(entry_pre))
        retname = entry_pre;

    /* SN-26-bridge-coverage-n: emit CALL on the wire BEFORE the entry-pass
     * NV writes.  SPITBOL's bridge fires CALL at bpf09 (before parameter
     * binding); scrip's comm_call must fire in the same relative order so
     * the wire records the function entry rather than the entry-pass clear
     * of the return slot.  Without this, NV_SET_fn(retname, "") at the
     * save/clear pass below fires comm_var first, which the controller
     * sees as `VALUE retname=''` — categorically a different event from
     * SPITBOL's `CALL fname`.  Beauty line 119 (`snoExprList = nPush()`,
     * stno=709) is the canonical reproducer.
     *
     * Pass retname (canonical body name) not fname (caller-side alias).
     * For OPSYN aliases like `OPSYN('&', 'reduce', 2)`, fname='&' but
     * retname='reduce' — SPITBOL reports the body name, so scrip aligns. */
    comm_call(retname);   /* T-2: FUNCTION trace CALL event */

    /* ── Save current values of retname-var, params, locals ──
     * SN-26-bridge-coverage-n: silence the wire — these NV writes are
     * interpreter mechanism, not user-visible assignments.  SPITBOL's
     * SIL doesn't emit comm_var for the equivalent internal stores. */
    monitor_quiet_depth++;
    int nsaved = 1 + np + nl;
    char   **snames = GC_malloc((size_t)nsaved * sizeof(char *));
    DESCR_t *svals  = GC_malloc((size_t)nsaved * sizeof(DESCR_t));
    /* Save/clear the return-value slot using retname (may differ from fname for OPSYN).
     * NV store is case-sensitive: function body writes "fact" not "FACT". */
    snames[0] = GC_strdup(retname);
    svals[0]  = NV_GET_fn(retname);
    NV_SET_fn(retname, STRVAL(""));    /* BUG-QIZE: clear return slot to empty string,
                                        * not NULVCL, so DIFFER(retvar) fails on entry.
                                        * SPITBOL treats cleared retname as "" (zero-length
                                        * string); NULVCL has a type tag that makes DIFFER
                                        * succeed (non-null) → divergence on Qize body. */
    for (int i = 0; i < np; i++) {
        snames[1+i] = pnames[i];
        /* If this param name aliases retname (e.g. DEFINE('f(f)')), the NV cell
         * is shared.  Record the pre-call global (already in svals[0]), then
         * write the arg.  The body writes retname= to set return value — same
         * NV cell as the param, which is correct SIL behaviour. */
        if (strcmp(pnames[i], retname) == 0)  /* SN-19: both canonical from lexer */
            svals[1+i] = svals[0];          /* dedup: same original global */
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

    /* ── Push call frame ── */
    CallFrame *fr = &call_stack[call_depth++];
    kw_fnclevel = call_depth;  /* &FNCLEVEL tracks live nesting depth */
    strncpy(fr->fname, retname, sizeof(fr->fname)-1);
    fr->fname[sizeof(fr->fname)-1] = '\0';
    fr->nshadow = 0;  /* SN-3: clear shadow table for this frame */
    /* SN-3: register params/locals whose names collide with SPITBOL builtins
     * (e.g. LEN, ANY, SPAN — NV_GET_fn returns the builtin descriptor, ignoring
     * NV_SET_fn writes). Shadow table takes priority in AST_VAR lookup. */
    for (int i = 0; i < np; i++)
        if (_is_pat_fnc_name(pnames[i]))
            shadow_set_cur(pnames[i], (i < nargs) ? args[i] : NULVCL);
    for (int i = 0; i < nl; i++)
        if (_is_pat_fnc_name(lnames[i]))
            shadow_set_cur(lnames[i], NULVCL);
    fr->saved_names = snames;
    fr->saved_vals  = svals;
    fr->nsaved      = nsaved;
    fr->retval_cell = STRVAL("");  /* cleared return slot, matches NV_SET clear above */
    fr->retval_set  = 0;

    DESCR_t retval = NULVCL;

    /* SN-26c-parseerr-f: save outer scan context so a recursive call to
     * exec_stmt inside the function body (e.g. "subject pattern" in match())
     * doesn't clobber the caller's Σ/Δ/Ω/Σlen globals. */
    const char *saved_Σ    = Σ;
    int         saved_Δ    = Δ;
    int         saved_Ω    = Ω;
    int         saved_Σlen = Σlen;

    int ret_kind = setjmp(fr->ret_env);
    if (ret_kind == 0) {
        /* ── Find body label: use entry_label (supports OPSYN aliases and
         * alternate entry points), then fall back to fname/ufname ── */
        const char *entry = FUNC_ENTRY_fn(fname);
        STMT_t *body = entry ? label_lookup(entry) : NULL;
        if (!body) body = label_lookup(fname);
        if (!body) body = label_lookup(ufname);

        /* SIL UNDF: no body label AND not a registered builtin → Error 5 (soft) */
        if (!body && !FNCEX_fn(fname) && !FNCEX_fn(ufname)) {
            sno_runtime_error(5, NULL);
            /* longjmp taken above; this line only reached if !g_sno_err_active */
            retval = FAILDESCR;
            goto fn_done;
        }

        if (body) {
            STMT_t *s = body;
            while (s) {
                if (s->is_end) break;
                if (s->subject && (s->subject->kind == AST_CHOICE ||
                                   s->subject->kind == AST_UNIFY  ||
                                   s->subject->kind == AST_CLAUSE)) {
                    s = s->next; continue;
                }
                /* SN-26-bridge-coverage-n: fire MWK_LABEL on every executed
                 * statement inside a function body — same coverage as the
                 * top-level execute_program loop.  SPITBOL's bridge fires
                 * LABEL via SIL's stmgo for every executed stmt regardless
                 * of nesting; scrip matches by emitting here.  Skipped for
                 * non-statement IR nodes (AST_CHOICE/AST_UNIFY/AST_CLAUSE) above. */
                {
                    extern void mon_emit_label_bin(int64_t stno);
                    mon_emit_label_bin((int64_t)s->stno);
                }

                DESCR_t     subj_val  = NULVCL;
                const char *subj_name = NULL;
                if (s->subject) {
                    if (s->subject->kind == AST_VAR && s->subject->sval) {
                        subj_name = s->subject->sval;
                        /* Only read value when needed for pattern match */
                        if (s->pattern)
                            subj_val = NV_GET_fn(subj_name);
                    } else if (s->subject->kind == AST_INDIRECT && s->subject->nchildren > 0) {
                        /* $'$B' or $X as subject — resolve to variable name for write-back.
                         * child is AST_QLIT "$B" (literal) or AST_VAR X (runtime indirect).
                         * SN-26-bridge-coverage-y: if the resolved value is a DT_N NAMEPTR
                         * (e.g. assign(name,...) where name was bound as `.snoBrackets`),
                         * recover the variable name from the NV cell pointer instead of
                         * letting VARVAL_fn read junk from the union's .s slot. */
                        AST_t *ic = s->subject->children[0];
                        if (ic->kind == AST_QLIT && ic->sval) {
                            subj_name = ic->sval;  /* $'name' — literal name, use directly */
                        } else if (ic->kind == AST_VAR && ic->sval) {
                            DESCR_t xv = NV_GET_fn(ic->sval); /* $X — indirect */
                            if (IS_NAMEPTR(xv)) {
                                const char *_rn = NV_name_from_ptr((const DESCR_t*)xv.ptr);
                                subj_name = _rn ? _rn : NULL;
                            } else if (xv.v == DT_N && xv.slen == 0 && xv.s) {
                                subj_name = xv.s;  /* NAMEVAL form */
                            } else {
                                subj_name = VARVAL_fn(xv);
                            }
                        } else {
                            DESCR_t nd = interp_eval(ic);
                            subj_name = VARVAL_fn(nd);
                        }
                        if (subj_name) {
                            /* SN-19: $name as subject — lex-fold the runtime-sourced name */
                            char *fn = GC_strdup(subj_name); sno_fold_name(fn);
                            subj_name = fn;
                        }
                        if (subj_name && s->pattern) {
                            subj_val = NV_GET_fn(subj_name);
                        } else if (!subj_name)
                            subj_val = interp_eval(s->subject);
                    } else if (s->subject->kind == AST_FNC && s->has_eq && !s->pattern) {
                        /* SN-6 session 14: same guard as top-level execute_program
                         * loop (~L4142).  Dedicated branches below (ITEM/FIELD setter
                         * at ~L673, NRETURN lvalue-assign at ~L699) call the function
                         * exactly once to obtain the assignment target.  Evaluating
                         * s->subject here would double-call the function inside a
                         * user function body — expr_eval.sno Binary() does
                         * Push() = EVAL(...) and needs this guard to match SPITBOL. */
                    } else {
                        subj_val = interp_eval(s->subject);
                    }
                }

                int succeeded = 1;
                if (s->pattern) {
                    DESCR_t pat_d = interp_eval_pat(s->pattern);
                    if (IS_FAIL_fn(pat_d)) {
                        succeeded = 0;
                    } else {
                        DESCR_t repl_val; int has_repl = 0;
                        if (s->has_eq && s->replacement) {
                            repl_val = interp_eval(s->replacement);
                            has_repl = !IS_FAIL_fn(repl_val);
                        }
                        Σ = subj_name ? subj_name : "";
                        succeeded = exec_stmt(subj_name,
                            subj_name ? NULL : &subj_val,
                            pat_d, has_repl ? &repl_val : NULL, has_repl);
                    }
                } else if (s->has_eq && subj_name) {
                    /* Plain assignment: X = expr  — always value context.
                     * *expr produces DT_E EXPRESSION (RUNTIME-6), not pattern. */
                    DESCR_t repl_val = s->replacement
                        ? interp_eval(s->replacement)
                        : NULVCL;
                    /* BP-1: if the RHS was a NRETURN function call, interp_eval
                     * NAME_DEREFs the DT_N (value context). But we want to store the
                     * DT_N itself so the caller can later use $nm for indirect assign.
                     * kw_rtntype is set by call_user_function before it returns and
                     * is still valid here (no nested call between interp_eval return
                     * and this check). Re-fetch from the function's return variable. */
                    if (strcmp(kw_rtntype, "NRETURN") == 0      /* SN-19: literal uppercase */
                            && s->replacement && s->replacement->kind == AST_FNC
                            && s->replacement->sval) {
                        DESCR_t raw = NV_GET_fn(s->replacement->sval);
                        if (IS_NAME(raw)) repl_val = raw;
                    }
                    if (IS_FAIL_fn(repl_val)) succeeded = 0;
                    else {
                        /* NRETURN lvalue write-through: subj_name may be a zero-param
                         * user fn returning DT_N (name ref). Only check when not already
                         * inside a function body (call_depth==0) to avoid re-entrant
                         * assignment during body execution (e.g. "ref_a = .a" in body). */
                        if (call_depth == 0 && FNCEX_fn(subj_name)
                                && FUNC_NPARAMS_fn(subj_name) == 0) {
                            DESCR_t fres = call_user_function(subj_name, NULL, 0);
                            if (NAME_SET(fres, repl_val)) { succeeded = 1; }
                            else { set_and_trace(subj_name, repl_val); succeeded = 1; }
                        } else { set_and_trace(subj_name, repl_val); succeeded = 1; }
                    }
                } else if (s->has_eq && s->subject && s->subject->kind == AST_KEYWORD && s->subject->sval) {
                    DESCR_t repl_val = s->replacement ? interp_eval(s->replacement) : NULVCL;
                    if (IS_FAIL_fn(repl_val)) succeeded = 0;
                    else {
                        /* SIL ASGNIC: delegate to ASGNIC_fn (snobol4.c export).
                         * Coerces to INTEGER and writes keyword global.
                         * Falls back to NV_SET_fn for unrecognised names (safety). */
                        if (!ASGNIC_fn(s->subject->sval, repl_val))
                            NV_SET_fn(s->subject->sval, repl_val);
                        succeeded = 1;
                    }
                } else if (s->has_eq && s->subject && s->subject->kind == AST_IDX &&
                           s->subject->nchildren >= 2) {
                    DESCR_t base = interp_eval(s->subject->children[0]);
                    DESCR_t idx  = interp_eval(s->subject->children[1]);
                    DESCR_t rv   = s->replacement ? interp_eval(s->replacement) : NULVCL;
                    if (IS_FAIL_fn(base)||IS_FAIL_fn(idx)||IS_FAIL_fn(rv)) succeeded = 0;
                    else {
                        if (s->subject->nchildren == 3) {
                            DESCR_t idx2 = interp_eval(s->subject->children[2]);
                            subscript_set2(base, idx, idx2, rv);
                        } else { subscript_set(base, idx, rv); }
                        /* SN-26-bridge-coverage-g: fire VALUE record for subscript store
                         * inside user function body. */
                        { const char *base_nm = (s->subject->children[0] &&
                                                 s->subject->children[0]->kind == AST_VAR)
                                               ? s->subject->children[0]->sval : NULL;
                          if (base_nm) comm_var(base_nm, rv); }
                        succeeded = 1;
                    }
                } else if (s->has_eq && s->subject && s->subject->kind == AST_FNC &&
                           s->subject->sval && s->subject->nchildren >= 1) {
                    /* ITEM(arr,i[,j]) = val  or  field(obj) = val at statement level */
                    DESCR_t rv = s->replacement ? interp_eval(s->replacement) : NULVCL;
                    if (!IS_FAIL_fn(rv)) {
                        if (strcmp(s->subject->sval, "ITEM") == 0 && s->subject->nchildren >= 2) {  /* SN-19 */
                            DESCR_t base = interp_eval(s->subject->children[0]);
                            DESCR_t idx  = interp_eval(s->subject->children[1]);
                            if (!IS_FAIL_fn(base) && !IS_FAIL_fn(idx)) {
                                if (s->subject->nchildren >= 3) {
                                    DESCR_t idx2 = interp_eval(s->subject->children[2]);
                                    if (!IS_FAIL_fn(idx2)) subscript_set2(base, idx, idx2, rv);
                                } else {
                                    subscript_set(base, idx, rv);
                                }
                                succeeded = 1;
                            } else succeeded = 0;
                        } else {
                            /* DATA field setter: fname(obj) = val */
                            DESCR_t obj = interp_eval(s->subject->children[0]);
                            if (!IS_FAIL_fn(obj)) {
                                FIELD_SET_fn(obj, s->subject->sval, rv);
                                /* SN-26-bridge-coverage-v: SPITBOL fires sysmw
                                 * (`<lval>` sentinel VALUE) on every aggregate
                                 * element store, including DATA fields.  Mirror
                                 * here for harness symmetry. */
                                comm_var("<lval>", rv);
                                succeeded = 1;
                            } else succeeded = 0;
                        }
                    } else succeeded = 0;
                } else if (s->has_eq && s->subject && s->subject->kind == AST_FNC &&
                           s->subject->sval && s->subject->nchildren == 0) {
                    /* NRETURN lvalue assign: ref_a() = val  (zero-arg fn call as lvalue)
                     * Call the function; if result is DT_N write through to named variable. */
                    DESCR_t fres = call_user_function(s->subject->sval, NULL, 0);
                    if (IS_NAME(fres)) {
                        DESCR_t rv = s->replacement ? interp_eval(s->replacement) : NULVCL;
                        if (IS_FAIL_fn(rv)) succeeded = 0;
                        else { succeeded = NAME_SET(fres, rv) ? 1 : 0; }
                    } else succeeded = 0;
                } else if (s->has_eq && s->subject && s->subject->kind == AST_INDIRECT) {
                    AST_t *ichild = s->subject->nchildren > 0 ? s->subject->children[0] : NULL;
                    DESCR_t repl_val = s->replacement ? interp_eval(s->replacement) : NULVCL;
                    if (IS_FAIL_fn(repl_val)) { succeeded = 0; }
                    else {
                        /* Evaluate the inner expr to get a NAME or string to indirect through */
                        DESCR_t ind_val = ichild ? interp_eval(ichild) : NULVCL;
                        /* If it's already a DT_N (e.g. $Push where Push = .stk[1]),
                         * write directly through the pointer — SIL ASGNVV semantics.
                         * SN-26-bridge-coverage-y: recover variable name from the NV cell
                         * pointer (same as name_t.c NM_PTR fix, session #55) and emit
                         * comm_var so SPITBOL's asinp asnpa fire-point is mirrored. */
                        if (IS_NAMEPTR(ind_val)) {
                            *(DESCR_t*)ind_val.ptr = repl_val;
                            { const char *_rn = NV_name_from_ptr((const DESCR_t*)ind_val.ptr);
                              comm_var(_rn ? _rn : "<lval>", repl_val); }
                            succeeded = 1;
                        } else {
                            /* Otherwise treat as string variable name */
                            const char *nm0 = VARVAL_fn(ind_val);
                            if (!nm0 || !*nm0) { succeeded = 0; }
                            else {
                                /* SN-19: $name = val — lex-fold the runtime-sourced name */
                                char *nm = GC_strdup(nm0); sno_fold_name(nm);
                                /* If the named variable itself holds a DT_N, write through.
                                 * SN-26-bridge-coverage-y: emit comm_var here too. */
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
                } else if (s->subject && !s->pattern && !s->has_eq) {
                    if (IS_FAIL_fn(subj_val)) succeeded = 0;
                }

                const char *target = NULL;
                if (s->goto_u || s->goto_u_expr || s->goto_s || s->goto_s_expr || s->goto_f || s->goto_f_expr) {
                    if (s->goto_u && *s->goto_u)
                        target = s->goto_u;
                    else if (s->goto_u_expr) {
                        DESCR_t cv = interp_eval(s->goto_u_expr);
                        target = (cv.v == DT_S && cv.s) ? cv.s : NULL;
                    } else if (succeeded && s->goto_s && *s->goto_s)
                        target = s->goto_s;
                    else if (succeeded && s->goto_s_expr) {
                        DESCR_t cv = interp_eval(s->goto_s_expr);
                        target = (cv.v == DT_S && cv.s) ? cv.s : NULL;
                    } else if (!succeeded && s->goto_f && *s->goto_f)
                        target = s->goto_f;
                    else if (!succeeded && s->goto_f_expr) {
                        DESCR_t cv = interp_eval(s->goto_f_expr);
                        target = (cv.v == DT_S && cv.s) ? cv.s : NULL;
                    }
                }

                if (target) {
                        if (strcmp(target, "END") == 0) break;  /* SN-19: canonical */
                    if (strcmp(target, "RETURN") == 0) {  /* SN-19: canonical from lexer */
                        retval = fr->retval_set ? fr->retval_cell : NV_GET_fn(fr->fname);
                        strncpy(kw_rtntype, "RETURN",  sizeof(kw_rtntype)-1);
                        goto fn_done;
                    }
                    if (strcmp(target, "FRETURN") == 0) {  /* SN-19 */
                        retval = FAILDESCR;
                        strncpy(kw_rtntype, "FRETURN", sizeof(kw_rtntype)-1);
                        goto fn_done;
                    }
                    if (strcmp(target, "NRETURN") == 0) {  /* SN-19 */
                        /* NRETURN: return DT_N from fn return var as-is;
                         * caller (AST_FNC) applies NAME_DEREF (slen discriminates
                         * NAMEPTR from NAMEVAL). */
                        retval = fr->retval_set ? fr->retval_cell : NV_GET_fn(fr->fname);
                        strncpy(kw_rtntype, "NRETURN", sizeof(kw_rtntype)-1);
                        goto fn_done;
                    }
                    STMT_t *dest = label_lookup(target);
                    if (dest) { s = dest; continue; }
                    break;
                }
                s = s->next;
            }
        }
        /* fell off body without RETURN — return function's name variable */
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
    /* SN-26c-parseerr-f: restore outer scan context clobbered by recursive exec_stmt */
    Σ    = saved_Σ;
    Δ    = saved_Δ;
    Ω    = saved_Ω;
    Σlen = saved_Σlen;
    comm_return(retname, retval);  /* T-2: FUNCTION trace RETURN event;
                                      use retname (body name) not fname
                                      (alias) for OPSYN parity with SPITBOL. */
    /* ── IC-5: snapshot initial-block locals before they're wiped ── */
    icn_init_update_snapshot(snames, svals, nsaved);
    /* ── Restore saved variables and pop frame ──
     * SN-26-bridge-coverage-n: silence the wire — these are mechanism. */
    monitor_quiet_depth++;
    for (int i = 0; i < nsaved; i++)
        NV_SET_fn(snames[i], svals[i]);
    monitor_quiet_depth--;
    call_depth--;
    kw_fnclevel = call_depth;
    return retval;
}

