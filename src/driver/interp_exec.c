/*
 * interp_exec.c — statement execution loop (execute_program)
 *
 * Split from interp.c by RS-3 (GOAL-REWRITE-SCRIP).
 * SI-6: rewritten to walk AST_PROGRAM children (AST_STMT/AST_END nodes)
 *       via stmt_attr_find; CODE_t/STMT_t removed.
 * AUTHORS: Lon Jones Cherryholmes · Claude Sonnet 4.6
 * DATE:    2026-05-02
 */

#include "interp_private.h"

/* ══════════════════════════════════════════════════════════════════════════
 * Prolog IR interpreter block — pl_execute_program_unified + helpers
 * (recovered from scrip.c bca2b79a; removed by 476fd067 accidentally)
 * ══════════════════════════════════════════════════════════════════════════ */

#define PL_PRED_TABLE_SIZE PL_PRED_TABLE_SIZE_FWD

/* SI-6: find the child index of a given AST_STMT node in prog->children[].
 * Used after label_lookup() to translate a pointer-to-stmt into an index. */
static int ast_prog_find_idx(const AST_t *prog, const AST_t *stmt)
{
    if (!prog || !stmt) return -1;
    for (int i = 0; i < prog->nchildren; i++)
        if (prog->children[i] == stmt) return i;
    return -1;
}

/* SI-6: accessors — fetch named attribute from an AST_STMT node. */
static inline AST_t        *s_expr(const AST_t *s, const char *tag) {
    return stmt_attr_expr(stmt_attr_find(s, tag)); }
static inline const char   *s_str (const AST_t *s, const char *tag) {
    return stmt_attr_str(stmt_attr_find(s, tag)); }
static inline int           s_has (const AST_t *s, const char *tag) {
    return stmt_attr_find(s, tag) != NULL; }
static inline int           s_int (const AST_t *s, const char *tag) {
    const char *v = s_str(s, tag); return v ? atoi(v) : 0; }

void execute_program(const AST_t *prog)
{
    NO_AST_WALK_GUARD("execute_program");
    polyglot_init(prog, polyglot_lang_mask(prog));   /* U-14 / FI-8: language-selective init */
    g_lang = 0;  /* SNOBOL4 mode */

    /* No hardcoded step_limit — &STLIMIT (kw_stlimit) governs via comm_stno().
     * comm_stno() increments kw_stcount and fires sno_runtime_error(22) when
     * kw_stlimit >= 0 && kw_stcount > kw_stlimit.  The setjmp handler below
     * breaks the loop for terminal error codes (22 = STLIMIT exceeded). */

    /* Arm runtime-error longjmp.  sno_runtime_error() prints the message
     * and longjmps here with the error code.  We treat it as statement
     * failure (SNOBOL4 spec: runtime error → fail branch, then END).
     * Terminal errors (code 20-23, 26-27, 29-31, 39) exit the loop. */
    g_sno_err_active = 1;

    /* Hoist per-iteration state above setjmp: C99 forbids goto crossing an
     * initializer, and longjmp re-enters at the setjmp call each iteration. */
    int         ci        = 0;   /* index into prog->children[] */
    int         stno      = 0;
    int         succeeded = 1;
    DESCR_t     subj_val  = NULVCL;
    const char *subj_name = NULL;
    const char *target    = NULL;
    int         nch       = prog ? prog->nchildren : 0;

    while (ci < nch) {
        const AST_t *s = prog->children[ci];
        if (!s) { ci++; continue; }

        if (s->kind == AST_END) break;  /* U-23: polyglot multi-section dispatch handles remaining modules */

        /* SI-6: read stmt fields via attrs */
        int         s_is_end  = (s->kind == AST_END);
        const char *s_label   = s_str(s, ":lbl");
        int         s_lang    = s_int(s, ":lang");
        int         s_has_eq  = s_has(s, ":eq");
        AST_t      *s_subject = s_expr(s, ":subj");
        AST_t      *s_pattern = s_expr(s, ":pat");
        AST_t      *s_repl    = s_expr(s, ":repl");

        /* goto attrs — may be static label (leaf child) or expr */
        AST_t *go_s_attr = stmt_attr_find(s, ":goS");
        AST_t *go_f_attr = stmt_attr_find(s, ":goF");
        AST_t *go_u_attr = stmt_attr_find(s, ":go");
        /* static labels (leaf child of attr node or NULL) */
        const char *goto_s      = go_s_attr ? stmt_attr_str(go_s_attr)  : NULL;
        const char *goto_f      = go_f_attr ? stmt_attr_str(go_f_attr)  : NULL;
        const char *goto_u      = go_u_attr ? stmt_attr_str(go_u_attr)  : NULL;
        /* computed labels (expr child when no leaf) */
        AST_t *goto_s_expr = (go_s_attr && !goto_s) ? stmt_attr_expr(go_s_attr) : NULL;
        AST_t *goto_f_expr = (go_f_attr && !goto_f) ? stmt_attr_expr(go_f_attr) : NULL;
        AST_t *goto_u_expr = (go_u_attr && !goto_u) ? stmt_attr_expr(go_u_attr) : NULL;

        /* Empty statement (blank source line — Green Book treats blank
         * lines as empty statements, SPITBOL/CSNOBOL4 advance &STNO but
         * not &STCOUNT through them).  Detect by total absence of
         * label/subject/pattern/replacement/goto, fire the LABEL event
         * for sync-step parity, advance stno locally, but do NOT call
         * comm_stno (which would bump &STCOUNT). */
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

        /* SN-26-bridge-coverage-f: fire MWK_LABEL on every statement entry. */
        {
            extern void mon_emit_label_bin(int64_t stno);
            mon_emit_label_bin((int64_t)stno);
        }

        /* SN-26c-stmt637 probe: trace each IR step -> source stmt number */
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

        /* IM-3: step-limit — stop after exactly g_ir_step_limit statements */
        if (g_ir_step_limit > 0 && g_ir_steps_done++ >= g_ir_step_limit)
            longjmp(g_ir_step_jmp, 1);

        /* ── --trace: print statement number to stderr ─────────────── */
        if (g_opt_trace)
            fprintf(stderr, "TRACE stmt %d\n", stno);

        /* Catch runtime errors (longjmp from sno_runtime_error).
         * Terminal errors (stlimit, storage, etc.) break the loop.
         * Soft errors: take :F branch if present, else advance. */
        int _err = setjmp(g_sno_err_jmp);
        if (_err != 0) {
            /* message already printed by sno_runtime_error() */
            if (sno_err_is_terminal(_err)) break;   /* &STLIMIT, storage, etc. */
            succeeded = 0;
            target    = NULL;
            if (goto_f && *goto_f)
                target = goto_f;
            goto do_goto;
        }

        /* ── U-15: per-statement dispatch by st->lang ─────────────── */
        if (s_lang == LANG_ICN || s_lang == LANG_RAKU) {
            /* Icon / Raku AST_STMT nodes are procedure definitions — already registered
             * in proc_table by polyglot_init.  Skip inline; main() is
             * called once after the SNO/PL statement loop completes. */
            ci++; continue;
        }
        if (s_lang == LANG_PL) {
            /* Prolog statement: evaluate subject as a goal with pl active */
            if (s_subject) {
                int sv_pl = g_pl_active;
                g_pl_active = 1;
                interp_eval(s_subject);
                g_pl_active = sv_pl;
            }
            ci++; continue;
        }
        /* LANG_SNO (0): fall through to existing SNOBOL4 path below.
         * Also skip any stray Prolog/Icon IR nodes that have lang==LANG_SNO
         * (shouldn't happen after U-12/U-13, but keep guard for safety). */
        if (s_subject && (s_subject->kind == AST_CHOICE ||
                          s_subject->kind == AST_UNIFY  ||
                          s_subject->kind == AST_CLAUSE)) {
            ci++; continue;
        }

        /* ── evaluate subject ──────────────────────────────────────── */
        subj_val  = NULVCL;
        subj_name = NULL;

        if (s_subject) {
            if (s_subject->kind == AST_VAR && s_subject->sval) {
                subj_name = s_subject->sval;
                /* Only read the value when we need to match against it.
                 * Pure assignment (has_eq, no pattern) only needs the name —
                 * calling NV_GET_fn on a function name triggers a spurious
                 * zero-arg call (APPLY_fn → g_user_call_hook), causing Error 5. */
                if (s_pattern)
                    subj_val = NV_GET_fn(subj_name);
            } else if (s_subject->kind == AST_INDIRECT && s_subject->nchildren > 0) {
                /* $'$B' or $X as subject — resolve to variable name for write-back */
                AST_t *ic = s_subject->children[0];
                if (ic->kind == AST_QLIT && ic->sval) {
                    subj_name = ic->sval;  /* $'name' — literal name, use directly */
                } else if (ic->kind == AST_VAR && ic->sval) {
                    DESCR_t xv = NV_GET_fn(ic->sval);
                    subj_name = VARVAL_fn(xv);
                } else {
                    DESCR_t nd = interp_eval(ic);
                    subj_name = VARVAL_fn(nd);
                }
                if (subj_name) {
                    /* SN-19: $name as subject — lex-fold the runtime-sourced name */
                    char *fn = GC_strdup(subj_name); sno_fold_name(fn);
                    subj_name = fn;
                }
                if (subj_name && s_pattern) {
                    subj_val = NV_GET_fn(subj_name);
                } else if (!subj_name)
                    subj_val = interp_eval(s_subject);
            } else if (s_subject->kind == AST_FNC && s_has_eq && !s_pattern) {
                /* SN-6 fix: fn() = val / fn(args) = val — LHS-as-fn assignment.
                 * The dedicated branches below (ITEM/FIELD setter, NRETURN lvalue-assign)
                 * call the function exactly once to obtain the assignment target.
                 * Evaluating it here would be a spurious second call. */
            } else {
                subj_val = interp_eval(s_subject);
            }
        }

        succeeded = 1;

        /* ── pattern match ─────────────────────────────────────────── */
        if (s_pattern) {
            /* S-10 fix: pattern must be evaluated in pattern context so *func()
             * produces XATP nodes, not frozen DT_E expressions. */
            DESCR_t pat_d = interp_eval_pat(s_pattern);
            /* ── --dump-bb: print PATND tree before match ─────────── */
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
                    /* X ? PAT =   (empty replacement) — replace matched
                     * portion with null string, advancing subject cursor */
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

        /* ── pure assignment (direct or null) ─────────────────────── */
        } else if (s_has_eq && subj_name) {
            /* X = expr  OR  X =  (null assign, no replacement node).
             * Always value context — *expr produces DT_E (RUNTIME-6). */
            DESCR_t repl_val = s_repl
                ? interp_eval(s_repl)
                : NULVCL;
            if (IS_FAIL_fn(repl_val)) {
                succeeded = 0;
            } else {
                set_and_trace(subj_name, repl_val);
                succeeded = 1;
            }

        /* ── subscript assignment: A<i> = expr ─────────────────────── */
        } else if (s_has_eq && s_subject &&
                   s_subject->kind == AST_IDX) {
            AST_t *idx_e = s_subject;
            if (idx_e->nchildren >= 2) {
                DESCR_t base = interp_eval(idx_e->children[0]);
                DESCR_t idx  = interp_eval(idx_e->children[1]);
                DESCR_t repl_val = s_repl ? interp_eval(s_repl) : NULVCL;
                if (IS_FAIL_fn(base) || IS_FAIL_fn(idx) || IS_FAIL_fn(repl_val)) {
                    succeeded = 0;
                } else {
                    if (idx_e->nchildren >= 3) {
                        DESCR_t idx2 = interp_eval(idx_e->children[2]);
                        subscript_set2(base, idx, idx2, repl_val);
                    } else {
                        subscript_set(base, idx, repl_val);
                    }
                    /* SN-26-bridge-coverage-g: fire VALUE record for subscript store. */
                    { const char *base_nm = (idx_e->children[0] &&
                                             idx_e->children[0]->kind == AST_VAR)
                                           ? idx_e->children[0]->sval : NULL;
                      if (base_nm) comm_var(base_nm, repl_val); }
                    succeeded = 1;
                }
            } else { succeeded = 0; }

        /* ── keyword assignment: &KW = expr ───────────────────────── */
        } else if (s_has_eq && s_subject &&
                   s_subject->kind == AST_KEYWORD && s_subject->sval) {
            DESCR_t repl_val = s_repl ? interp_eval(s_repl) : NULVCL;
            if (IS_FAIL_fn(repl_val)) {
                succeeded = 0;
            } else {
                g_kw_ctx = 1;             /* signal Error 7 guard in NV_SET_fn */
                NV_SET_fn(s_subject->sval, repl_val);
                g_kw_ctx = 0;
                succeeded = 1;
            }

        /* ── indirect assignment: $expr = rhs ─────────────────────── */
        } else if (s_has_eq && s_subject &&
                   s_subject->kind == AST_INDIRECT) {
            AST_t *ichild = s_subject->nchildren > 0 ? s_subject->children[0] : NULL;
            DESCR_t repl_val = s_repl ? interp_eval(s_repl) : NULVCL;
            if (IS_FAIL_fn(repl_val)) {
                succeeded = 0;
            } else {
                DESCR_t name_d = ichild ? interp_eval(ichild) : NULVCL;
                const char *nm0 = VARVAL_fn(name_d);
                if (!nm0 || !*nm0) {
                    succeeded = 0;
                } else {
                    char *nm = GC_strdup(nm0); sno_fold_name(nm);  /* SN-19 */
                    set_and_trace(nm, repl_val);
                    succeeded = 1;
                }
            }

        /* ── ITEM setter or DATA field setter: fname(obj[,i]) = expr ── */
        } else if (s_has_eq && s_subject &&
                   s_subject->kind == AST_FNC && s_subject->sval &&
                   s_subject->nchildren >= 1) {
            DESCR_t repl_val = s_repl ? interp_eval(s_repl) : NULVCL;
            if (IS_FAIL_fn(repl_val)) {
                succeeded = 0;
            } else if (strcmp(s_subject->sval, "ITEM") == 0 &&  /* SN-19 */
                       s_subject->nchildren >= 2) {
                DESCR_t base = interp_eval(s_subject->children[0]);
                DESCR_t idx  = interp_eval(s_subject->children[1]);
                if (IS_FAIL_fn(base) || IS_FAIL_fn(idx)) {
                    succeeded = 0;
                } else if (s_subject->nchildren >= 3) {
                    DESCR_t idx2 = interp_eval(s_subject->children[2]);
                    if (IS_FAIL_fn(idx2)) { succeeded = 0; }
                    else { subscript_set2(base, idx, idx2, repl_val); succeeded = 1; }
                } else {
                    subscript_set(base, idx, repl_val);
                    succeeded = 1;
                }
            } else {
                DESCR_t obj = interp_eval(s_subject->children[0]);
                if (IS_FAIL_fn(obj)) {
                    succeeded = 0;
                } else {
                    FIELD_SET_fn(obj, s_subject->sval, repl_val);
                    succeeded = 1;
                }
            }

        /* ── NRETURN lvalue assign: fn() = expr  (zero-arg fn call as lvalue) ── */
        } else if (s_has_eq && s_subject &&
                   s_subject->kind == AST_FNC && s_subject->sval &&
                   s_subject->nchildren == 0) {
            DESCR_t rv = s_repl ? interp_eval(s_repl) : NULVCL;
            if (!IS_FAIL_fn(rv)) {
                DESCR_t fres = call_user_function(s_subject->sval, NULL, 0);
                /* Use NAME_SET: slen discriminates NAMEPTR (interior ptr) from NAMEVAL (name string) */
                if (NAME_SET(fres, rv)) { succeeded = 1; }
                else { succeeded = 0; }
            } else succeeded = 0;

        /* ── expression-only (side effects, e.g. bare function call) ─ */
        } else if (s_subject && !s_pattern && !s_has_eq) {
            if (IS_FAIL_fn(subj_val)) succeeded = 0;
        }

        /* ── goto resolution ───────────────────────────────────────── */
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
            /* Check for END pseudo-label */
            if (strcmp(target, "END") == 0) break;  /* SN-19: canonical */
            /* RETURN/FRETURN at top-level (outside a call) → treat as END */
            if (strcmp(target, "RETURN") == 0 || strcmp(target, "FRETURN") == 0) break;  /* SN-19 */
            const AST_t *dest = label_lookup(target);
            if (dest) {
                int dest_ci = ast_prog_find_idx(prog, dest);
                if (dest_ci >= 0) { ci = dest_ci; continue; }
            }
            /* Unknown label — Error 24: Undefined or erroneous goto */
            sno_runtime_error(24, NULL);
            break;
        }

        ci++;
    }

    /* ── U-23: section-ordered polyglot dispatch ──────────────────────────
     * When g_polyglot=1 (multi-section .scrip), execute modules in registry
     * order so interleaved SNO/ICN/PL sections see each other's NV values.
     * The SNO main loop above already ran all SNO stmts (they are NOT
     * re-run here).  For Icon and Prolog sections we call their main/0
     * in source order relative to SNO sections.
     *
     * U-23 multi-section model:
     *   - SNO sections: already executed by main loop (in order, END skipped).
     *   - ICN sections: find the "main" proc for each ICN module in order
     *     and call it.  If multiple ICN modules share a proc table, only one
     *     main will be present — that is correct.
     *   - PL  sections: call main/0 once after all PL clauses are loaded.
     *
     * For single-section .scrip (g_polyglot=0), fall through to legacy dispatch.
     */
    if (g_polyglot && g_registry.nmod > 0) {
        /* U-23: two-pass dispatch. SNO already ran; dispatch ICN/PL in registry order. */
        for (int _mi = 0; _mi < g_registry.nmod; _mi++) {
            ScripModule *_m = &g_registry.mods[_mi];
            if (_m->lang == LANG_ICN || _m->lang == LANG_RAKU) {
                int _pend = _m->icn_proc_start + _m->proc_count;
                int _found = 0;
                g_lang = 1;   /* OE-7: Icon top-level mode required for coro_call */
                for (int _pi = _m->icn_proc_start; _pi < _pend && _pi < proc_count; _pi++) {
                    if (strcmp(proc_table[_pi].name, "main") == 0)
                        { proc_table_call(_pi, NULL, 0); _found=1; break; }   /* CH-17g-call-sites */
                }
                if (!_found)
                    for (int _pi=0; _pi<proc_count; _pi++)
                        if (strcmp(proc_table[_pi].name,"main")==0)
                            { proc_table_call(_pi,NULL,0); break; }   /* CH-17g-call-sites */
                g_lang = 0;
            } else if (_m->lang == LANG_PL) {
                AST_t *pl_main = pl_pred_table_lookup(&g_pl_pred_table, "main/0");
                if (pl_main) {
                    int sv_pl = g_pl_active; g_pl_active = 1;
                    interp_eval(pl_main);
                    g_pl_active = sv_pl;
                }
            }
        }
        return;
    }

    /* ── Legacy single-section dispatch (U-15 / U-19) ──────────────────── */
    if (proc_count > 0) {
        for (int _i = 0; _i < proc_count; _i++) {
            if (strcmp(proc_table[_i].name, "main") == 0) {
                proc_table_call(_i, NULL, 0);   /* CH-17g-call-sites */
                break;
            }
        }
    }
    {
        AST_t *pl_main = pl_pred_table_lookup(&g_pl_pred_table, "main/0");
        if (pl_main) {
            int sv_pl = g_pl_active;
            g_pl_active = 1;
            interp_eval(pl_main);
            g_pl_active = sv_pl;
        }
    }
}

/* IM-3: execute_program_steps — run at most N statements then return.
 * Sets up g_ir_step_jmp so the step-limit longjmp lands here safely. */
void execute_program_steps(const AST_t *prog, int n) {
    g_ir_step_limit = n;
    g_ir_steps_done = 0;
    if (setjmp(g_ir_step_jmp) == 0)
        execute_program(prog);
    g_ir_step_limit = 0;
    g_ir_steps_done = 0;
}
