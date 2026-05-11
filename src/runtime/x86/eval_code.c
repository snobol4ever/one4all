/*
 * eval_code.c — M-DYN-6: EVAL and CODE via dynamic path
 *
 * PUBLIC API
 * ----------
 *   DESCR_t     eval_expr(const char *src)
 *       Parse src as a SNOBOL4 expression via parse_expr_from_str(),
 *       walk the tree_t IR tree, evaluate to a DESCR_t.
 *       Returns FAILDESCR on parse or eval failure.
 *
 *   DESCR_t     code(const char *src)
 *       Parse src as SNOBOL4 statements via sno_parse() (fmemopen),
 *       stash the CODE_t* in a DT_C DESCR_t.
 *       Returns FAILDESCR on parse failure.
 *
 *   const char *exec_code(DESCR_t code_block)
 *       Execute a DT_C code block statement by statement.
 *       Returns the first unconditional/success goto label encountered,
 *       or "" on fall-through success, or NULL on failure.
 *
 * DESIGN
 * ------
 *   EVAL and CODE are not special.  They are the runtime doing what it
 *   always does with source that arrived late (ARCH-byrd-dynamic.md).
 *
 *   eval_expr: parse_expr_from_str → eval_node (recursive tree_t walk)
 *   code:      fmemopen → sno_parse → CODE_t* stored as DT_C
 *   exec_code: walk CODE_t stmts, call exec_stmt per stmt,
 *                     resolve gotos, return first branch target.
 *
 * RELATION TO EXISTING EVAL_fn
 * -----------------------------
 *   snobol4_pattern.c already has EVAL_fn() — a hand-rolled mini-parser
 *   covering the beauty.sno pattern-expression subset.  That path is
 *   preserved.  eval_expr() is the full-expression path; it is called
 *   from a new EVAL_fn wrapper in snobol4_pattern.c (see patch note at
 *   bottom of this file) only after the existing fast path declines.
 *
 *   For M-DYN-6 we wire the new path in by replacing EVAL_fn in
 *   snobol4_pattern.c with a thin wrapper that first tries the full
 *   parse path; the old _ev_expr hand-roller becomes a fallback.
 *
 * AUTHORS: Lon Jones Cherryholmes · Claude Sonnet 4.6
 * DATE:    2026-04-01
 * SPRINT:  DYN-7
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>

/* ── runtime ─────────────────────────────────────────────────────────── */
#include "snobol4.h"
#include "sil_macros.h"   /* SIL macro translations — RT + SM axes */
#include "sm_interp.h"    /* CHUNKS-step02: sm_call_expression for DT_E expression dispatch */

/* ── frontend: bison/flex parse entry points (CMPILE removed) */
#include "../../frontend/snobol4/scrip_cc.h"
/* parse_expr_pat_from_str, sno_parse_string declared in scrip_cc.h */

/* exec_stmt — the five-phase executor */
extern int exec_stmt(const char  *subj_name,
                          DESCR_t     *subj_var,
                          DESCR_t      pat,
                          DESCR_t     *repl,
                          int          has_repl);

/* subject globals (defined in test driver or main runtime) */
extern const char *Σ;
extern int         Ω;
extern int         Δ;

/* ══════════════════════════════════════════════════════════════════════════
 * eval_node — recursive tree_t → DESCR_t evaluator
 * ══════════════════════════════════════════════════════════════════════════ */

DESCR_t eval_node(tree_t *e)
{
    if (!e) return NULVCL;

    switch (e->t) {

    /* ── deferred expression — freeze child as DT_E (EXPRESSION type) ── */
    case TT_DEFER:
        /* *X  (STRFN/UOP_STR) — produce a DT_E EXPRESSION descriptor.
         * The descriptor holds a pointer to the child tree_t*.
         * EVAL_fn thaws it by calling eval_node on the child.
         * Do NOT evaluate the child here — that is EVAL()'s job. */
        if (e->n < 1) return NULVCL;
        {
            DESCR_t d;
            d.v    = DT_E;
            d.slen = 0;
            d.s    = NULL;           /* clear union first... */
            d.ptr  = e->c[0]; /* ...then store ptr last (ptr and s share union) */
            return d;
        }

    /* ── literals ────────────────────────────────────────────────────── */
    case TT_ILIT:
        return INTVAL(e->v.ival);

    case TT_FLIT:
        return REALVAL(e->v.dval);

    case TT_QLIT:
        return e->v.sval ? STRVAL(e->v.sval) : NULVCL;

    case TT_NUL:
        return NULVCL;

    /* ── variable / keyword reference ────────────────────────────────── */
    case TT_VAR:
        if (e->v.sval && *e->v.sval)
            return NV_GET_fn(e->v.sval);
        return NULVCL;

    case TT_KEYWORD: {
        /* &KEYWORD — prepend '&' for the NV table key */
        if (!e->v.sval || !*e->v.sval) return NULVCL;
        char kbuf[128];
        snprintf(kbuf, sizeof kbuf, "&%s", e->v.sval);
        return NV_GET_fn(kbuf);
    }

    /* ── unary minus ─────────────────────────────────────────────────── */
    case TT_MNS:
        if (e->n < 1) return FAILDESCR;
        return neg(eval_node(e->c[0]));

    /* ── arithmetic ──────────────────────────────────────────────────── */
    case TT_ADD: {
        if (e->n < 2) return FAILDESCR;
        DESCR_t l = eval_node(e->c[0]);
        DESCR_t r = eval_node(e->c[1]);
        if (IS_FAIL_fn(l) || IS_FAIL_fn(r)) return FAILDESCR;
        return add(l, r);
    }
    case TT_SUB: {
        if (e->n < 2) return FAILDESCR;
        DESCR_t l = eval_node(e->c[0]);
        DESCR_t r = eval_node(e->c[1]);
        if (IS_FAIL_fn(l) || IS_FAIL_fn(r)) return FAILDESCR;
        return sub(l, r);
    }
    case TT_MUL: {
        if (e->n < 2) return FAILDESCR;
        DESCR_t l = eval_node(e->c[0]);
        DESCR_t r = eval_node(e->c[1]);
        if (IS_FAIL_fn(l) || IS_FAIL_fn(r)) return FAILDESCR;
        return mul(l, r);
    }
    case TT_DIV: {
        if (e->n < 2) return FAILDESCR;
        DESCR_t l = eval_node(e->c[0]);
        DESCR_t r = eval_node(e->c[1]);
        if (IS_FAIL_fn(l) || IS_FAIL_fn(r)) return FAILDESCR;
        return DIVIDE_fn(l, r);
    }
    case TT_POW: {
        if (e->n < 2) return FAILDESCR;
        DESCR_t l = eval_node(e->c[0]);
        DESCR_t r = eval_node(e->c[1]);
        if (IS_FAIL_fn(l) || IS_FAIL_fn(r)) return FAILDESCR;
        return POWER_fn(l, r);
    }

    /* ── string concatenation ────────────────────────────────────────── */
    case TT_CAT:
    case TT_SEQ: {
        /* S-9 fix: EVAL("LEN(1) LEN(1)") must return PATTERN not STRING.
         * In SNOBOL4, space-separated terms in an expression are concatenation.
         * When the accumulated value is a pattern, concatenation is pat_cat (pattern
         * concatenation), not CONCAT_fn (string concatenation).  CONCAT_fn coerces
         * patterns to strings, destroying the type. */
        if (e->n == 0) return NULVCL;
        DESCR_t acc = eval_node(e->c[0]);
        if (IS_FAIL_fn(acc)) return FAILDESCR;
        for (int i = 1; i < e->n; i++) {
            DESCR_t next = eval_node(e->c[i]);
            if (IS_FAIL_fn(next)) return FAILDESCR;
            if (acc.v == DT_P || next.v == DT_P) {
                extern DESCR_t pat_cat(DESCR_t a, DESCR_t b);
                acc = pat_cat(acc, next);
            } else {
                acc = CONCAT_fn(acc, next);
            }
            if (IS_FAIL_fn(acc)) return FAILDESCR;
        }
        return acc;
    }

    /* ── assignment: subject = replacement (value context → yield repl) */
    case TT_ASSIGN: {
        if (e->n < 2) return FAILDESCR;
        /* left child is lvalue (TT_VAR or TT_INDIRECT) */
        DESCR_t val = eval_node(e->c[1]);
        if (IS_FAIL_fn(val)) return FAILDESCR;
        tree_t *lv = e->c[0];
        if (lv && lv->t == TT_VAR && lv->v.sval)
            NV_SET_fn(lv->v.sval, val);
        else if (lv && lv->t == TT_INDIRECT && lv->n > 0) {
            DESCR_t name_d = eval_node(lv->c[0]);
            const char *nm = VARVAL_fn(name_d);
            if (nm && *nm) {
                char *fn = GC_strdup(nm); sno_fold_name(fn);  /* SN-19 lex-fold on runtime name */
                NV_SET_fn(fn, val);
            }
        }
        return val;
    }

    /* ── indirect reference $expr ────────────────────────────────────── */
    case TT_INDIRECT: {
        if (e->n < 1) return FAILDESCR;
        DESCR_t name_d = eval_node(e->c[0]);
        const char *nm = VARVAL_fn(name_d);
        if (!nm || !*nm) return NULVCL;
        char *fn = GC_strdup(nm); sno_fold_name(fn);  /* SN-19 lex-fold on runtime name */
        return NV_GET_fn(fn);
    }

    /* ── function call ───────────────────────────────────────────────── */
    case TT_FNC: {
        if (!e->v.sval || !*e->v.sval) return FAILDESCR;
        int nargs = e->n;
        DESCR_t *args = nargs > 0
            ? (DESCR_t *)alloca((size_t)nargs * sizeof(DESCR_t))
            : NULL;
        for (int i = 0; i < nargs; i++) {
            args[i] = eval_node(e->c[i]);
            if (IS_FAIL_fn(args[i])) return FAILDESCR;
        }
        return APPLY_fn(e->v.sval, args, nargs);
    }

    /* ── array/table subscript ───────────────────────────────────────── */
    case TT_IDX: {
        if (e->n < 2) return FAILDESCR;
        DESCR_t base = eval_node(e->c[0]);
        if (IS_FAIL_fn(base)) return FAILDESCR;
        if (e->n == 2) {
            DESCR_t idx = eval_node(e->c[1]);
            if (IS_FAIL_fn(idx)) return FAILDESCR;
            return subscript_get(base, idx);
        } else {
            DESCR_t i1 = eval_node(e->c[1]);
            DESCR_t i2 = eval_node(e->c[2]);
            if (IS_FAIL_fn(i1) || IS_FAIL_fn(i2)) return FAILDESCR;
            return subscript_get2(base, i1, i2);
        }
    }

    /* ── .X  name-of — return DT_N lvalue descriptor ────────────────── */
    case TT_NAME: {
        /* DOTFN: .X yields the name (lvalue) of X, not its value.
         * Child is TT_VAR (variable name) or TT_KEYWORD. */
        if (e->n < 1) return FAILDESCR;
        tree_t *child = e->c[0];
        if (!child) return FAILDESCR;
        if (child->t == TT_VAR && child->v.sval)
            return NAME_fn(child->v.sval);
        if (child->t == TT_KEYWORD && child->v.sval) {
            char kbuf[128];
            snprintf(kbuf, sizeof kbuf, "&%s", child->v.sval);
            return NAME_fn(kbuf);
        }
        DESCR_t inner = eval_node(child);
        if (IS_FAIL_fn(inner)) return FAILDESCR;
        const char *nm = VARVAL_fn(inner);
        if (!nm || !*nm) return FAILDESCR;
        return NAME_fn(nm);
    }

    /* ── +X  unary plus — numeric coerce ─────────────────────────────── */
    case TT_PLS: {
        if (e->n < 1) return FAILDESCR;
        DESCR_t arg = eval_node(e->c[0]);
        if (IS_FAIL_fn(arg)) return FAILDESCR;
        return APPLY_fn("PLS", &arg, 1);
    }

    /* ── ?X  interrogation — succeed→null, fail→FAIL ─────────────────── */
    case TT_INTERROGATE: {
        /* SIL QUES: evaluate child; if it FAILs → FAIL; else → NULVCL.
         * Used for conditional pattern: ?pat succeeds iff pat matches. */
        if (e->n < 1) return FAILDESCR;
        DESCR_t res = eval_node(e->c[0]);
        if (IS_FAIL_fn(res)) return FAILDESCR;
        return NULVCL;
    }

    /* ── X | Y  pattern alternation (value context: ORFN) ───────────── */
    case TT_ALT: {
        /* In value context, alternation is a pattern constructor.
         * Build it via CMPILE: re-stringify as "(L)|(R)" then eval_via_cmpile.
         * For the common case of two pattern children, use PATVAL + pat_alt. */
        if (e->n < 1) return NULVCL;
        extern DESCR_t pat_alt(DESCR_t a, DESCR_t b);
        DESCR_t acc = eval_node(e->c[0]);
        if (IS_FAIL_fn(acc)) return FAILDESCR;
        acc = PATVAL_fn(acc);
        if (IS_FAIL_fn(acc)) return FAILDESCR;
        for (int i = 1; i < e->n; i++) {
            DESCR_t rhs = eval_node(e->c[i]);
            if (IS_FAIL_fn(rhs)) return FAILDESCR;
            rhs = PATVAL_fn(rhs);
            if (IS_FAIL_fn(rhs)) return FAILDESCR;
            acc = pat_alt(acc, rhs);
            if (IS_FAIL_fn(acc)) return FAILDESCR;
        }
        return acc;
    }

    /* ── X . Y  conditional capture (NAMFN) ─────────────────────────── */
    case TT_CAPT_COND_ASGN: {
        if (e->n < 2) return FAILDESCR;
        DESCR_t pat  = eval_node(e->c[0]);
        if (IS_FAIL_fn(pat)) return FAILDESCR;
        pat = PATVAL_fn(pat);
        if (IS_FAIL_fn(pat)) return FAILDESCR;
        /* When capture target is TT_INDIRECT (e.g. REM . $'$B'), resolve the
         * variable name without dereferencing — return NAME_fn(resolved_name)
         * so bb_nme_emit_binary gets varname="$B", not the value of $B. */
        DESCR_t name;
        tree_t *tgt = e->c[1];
        if (tgt && tgt->t == TT_INDIRECT && tgt->n > 0) {
            tree_t *ic = tgt->c[0];
            const char *nm = NULL;
            if (ic->t == TT_QLIT && ic->v.sval)        nm = ic->v.sval;
            else if (ic->t == TT_VAR  && ic->v.sval) { DESCR_t xv = NV_GET_fn(ic->v.sval); nm = VARVAL_fn(xv); }
            else                                      { DESCR_t nd = eval_node(ic);        nm = VARVAL_fn(nd); }
            if (!nm) return FAILDESCR;
            char *fn = GC_strdup(nm); sno_fold_name(fn);  /* SN-19 */
            name = NAME_fn(fn);
        } else {
            name = eval_node(tgt);
        }
        if (IS_FAIL_fn(name)) return FAILDESCR;
        return pat_assign_cond(pat, name);
    }

    /* ── X $ Y  immediate capture (DOLFN) ───────────────────────────── */
    case TT_CAPT_IMMED_ASGN: {
        if (e->n < 2) return FAILDESCR;
        DESCR_t pat  = eval_node(e->c[0]);
        if (IS_FAIL_fn(pat)) return FAILDESCR;
        pat = PATVAL_fn(pat);
        if (IS_FAIL_fn(pat)) return FAILDESCR;
        /* Same TT_INDIRECT target resolution as TT_CAPT_COND_ASGN above. */
        DESCR_t name;
        tree_t *tgt = e->c[1];
        if (tgt && tgt->t == TT_INDIRECT && tgt->n > 0) {
            tree_t *ic = tgt->c[0];
            const char *nm = NULL;
            if (ic->t == TT_QLIT && ic->v.sval)        nm = ic->v.sval;
            else if (ic->t == TT_VAR  && ic->v.sval) { DESCR_t xv = NV_GET_fn(ic->v.sval); nm = VARVAL_fn(xv); }
            else                                      { DESCR_t nd = eval_node(ic);        nm = VARVAL_fn(nd); }
            if (!nm) return FAILDESCR;
            char *fn = GC_strdup(nm); sno_fold_name(fn);  /* SN-19 */
            name = NAME_fn(fn);
        } else {
            name = eval_node(tgt);
        }
        if (IS_FAIL_fn(name)) return FAILDESCR;
        return pat_assign_imm(pat, name);
    }

    /* ── @X  cursor capture ──────────────────────────────────────────── */
    case TT_CAPT_CURSOR: {
        /* @VAR — cursor-position capture: build XATP("@", varname) node.
         * Child is TT_VAR or TT_NAME holding the capture variable name. */
        if (e->n < 1) return FAILDESCR;
        tree_t *child = e->c[0];
        const char *varname = NULL;
        if (child && child->t == TT_VAR  && child->v.sval) varname = child->v.sval;
        if (child && child->t == TT_NAME && child->n > 0
                && child->c[0] && child->c[0]->v.sval)
            varname = child->c[0]->v.sval;
        if (!varname) return FAILDESCR;
        { extern DESCR_t pat_at_cursor(const char *varname);
          return pat_at_cursor(varname); }
    }

    /* ── X ? Y  scan (BIQSFN) ───────────────────────────────────────── */
    case TT_SCAN: {
        /* Subject ? Pattern — in value context evaluate the subject,
         * coerce pattern, apply match; return matched substring or FAIL. */
        if (e->n < 2) return FAILDESCR;
        DESCR_t subj = eval_node(e->c[0]);
        if (IS_FAIL_fn(subj)) return FAILDESCR;
        DESCR_t pat  = eval_node(e->c[1]);
        if (IS_FAIL_fn(pat)) return FAILDESCR;
        pat = PATVAL_fn(pat);
        if (IS_FAIL_fn(pat)) return FAILDESCR;
        return APPLY_fn("__scan", &subj, 1);   /* stub: full scan via exec_stmt */
    }

    /* ── unhandled nodes ─────────────────────────────────────────────── */
    default:
        return NULVCL;
    }
}

/* ══════════════════════════════════════════════════════════════════════════
 * eval_expr — public entry point
 *
 * Parses src via CMPILE's EXPR() entry (cmpile_eval_expr), lowers the
 * CMPND_t parse tree to tree_t IR via cmpnd_to_expr(), then evaluates.
 * This is the SIL CONVEX/CONVE path — no bison/flex involved.
 * ══════════════════════════════════════════════════════════════════════════ */

DESCR_t eval_expr(const char *src)
{
    if (!src || !*src) return NULVCL;

    tree_t *tree = parse_expr_pat_from_str(src);
    if (!tree) return FAILDESCR;

    return eval_node(tree);
}


/* ══════════════════════════════════════════════════════════════════════════
 * code — parse statement block, return DT_C DESCR_t
 * ══════════════════════════════════════════════════════════════════════════ */

DESCR_t code(const char *src)
{
    if (!src || !*src) return FAILDESCR;

    /* SI-6: use sno_parse_string_ast — stores TT_PROGRAM* in DT_C. */
    extern tree_t *sno_parse_string_ast(const char *src, CODE_t **code_out);
    tree_t *ast = sno_parse_string_ast(src, NULL);

    if (!ast || ast->n == 0) return FAILDESCR;

    DESCR_t d;
    d.v   = DT_C;
    d.ptr = ast;           /* tree_t* stored as generic GC pointer */
    d.slen = 0;
    return d;
}

/* ══════════════════════════════════════════════════════════════════════════
 * exec_code — run a DT_C block, return first goto label (or ""/NULL)
 * ══════════════════════════════════════════════════════════════════════════ */

/*
 * Execute each statement in the code block in order.
 * For each statement:
 *   - Phase 1: evaluate subject expression → subj_name / subj_val
 *   - Phase 2/3: if pattern present, call exec_stmt
 *   - Goto resolution: check success/failure/uncond goto fields
 * Return the first goto label we need to branch to (caller resolves).
 *
 * Simplified model for M-DYN-6:
 *   - Subject-only statements (no pattern) are assignments or OUTPUT.
 *   - We eval the subject expression and, if the statement has an
 *     assignment (has_eq), assign to the subject variable.
 *   - Pattern statements go through exec_stmt.
 *   - Goto is returned as a string for the caller to dispatch.
 */
const char *exec_code(DESCR_t code_block)
{
    if (code_block.v != DT_C || !code_block.ptr) return NULL;
    const tree_t *prog = (const tree_t *)code_block.ptr;  /* SI-6: TT_PROGRAM* */

    for (int _ci = 0; _ci < prog->n; _ci++) {
        const tree_t *s = prog->c[_ci];
        if (!s) continue;
        if (s->t == TT_END) return "";

        /* SI-6: read stmt fields via stmt_attr helpers */
        int has_eq = stmt_attr_find(s, ":eq") != NULL;
        tree_t *subject     = stmt_attr_expr(stmt_attr_find(s, ":subj"));
        tree_t *pattern     = stmt_attr_expr(stmt_attr_find(s, ":pat"));
        tree_t *replacement = stmt_attr_expr(stmt_attr_find(s, ":repl"));
        const char *goto_u = stmt_attr_str(stmt_attr_find(s, ":go"));
        const char *goto_s = stmt_attr_str(stmt_attr_find(s, ":goS"));
        const char *goto_f = stmt_attr_str(stmt_attr_find(s, ":goF"));

        /* Evaluate subject */
        DESCR_t subj_val = NULVCL;
        const char *subj_name = NULL;

        if (subject) {
            if (subject->t == TT_VAR && subject->v.sval) {
                subj_name = subject->v.sval;
                subj_val  = NV_GET_fn(subj_name);
            } else {
                subj_val = eval_node(subject);
            }
        }

        int succeeded = 1;

        if (pattern) {
            DESCR_t pat_d = eval_node(pattern);
            if (IS_FAIL_fn(pat_d)) {
                succeeded = 0;
            } else {
                DESCR_t repl_val;
                int has_repl = 0;
                if (has_eq && replacement) {
                    repl_val = eval_node(replacement);
                    has_repl = !IS_FAIL_fn(repl_val);
                }
                succeeded = exec_stmt(
                    subj_name,
                    subj_name ? NULL : &subj_val,
                    pat_d,
                    has_repl ? &repl_val : NULL,
                    has_repl);
            }
        } else if (has_eq && replacement && subj_name) {
            DESCR_t repl_val = eval_node(replacement);
            if (IS_FAIL_fn(repl_val)) {
                succeeded = 0;
            } else {
                NV_SET_fn(subj_name, repl_val);
                succeeded = 1;
            }
        } else if (subject && !pattern && !has_eq) {
            if (IS_FAIL_fn(subj_val)) succeeded = 0;
        }

        /* Goto resolution */
        if (goto_u || goto_s || goto_f) {
            if (goto_u && *goto_u) return goto_u;
            if (succeeded && goto_s && *goto_s) return goto_s;
            if (!succeeded && goto_f && *goto_f) return goto_f;
        }
    }

    return "";
}

/* ══════════════════════════════════════════════════════════════════════════
 * RT-6: EXPVAL_fn — execute a DT_E EXPRESSION with full save/restore
 *
 * SIL EXPVAL: saves system state (NAM frame, subject globals), executes
 * the frozen tree_t* child via eval_node, then restores state on exit.
 * Fully re-entrant — nested EVAL() calls stack save frames correctly.
 *
 * DT_E holds ptr = frozen tree_t* (set by TT_DEFER in eval_node above).
 * ══════════════════════════════════════════════════════════════════════════ */

DESCR_t EXPVAL_fn(DESCR_t expr_d)
{
    if (expr_d.v == DT_E) {
        /* CHUNKS-step03: chunk DT_E (slen==1) — dispatch via sm_call_expression
         * which runs a fresh nested SM_State with local err_jmp save/restore.
         * Used for the stored-expression thaw path (bb_usercall, pat_to_patnd). */
        if (expr_d.slen == 1) {
            int entry_pc = (int)expr_d.i;
            return sm_call_expression(entry_pc);
        }

        /* Legacy: Frozen tree_t* — thaw and evaluate with NAM frame isolation */
        if (!expr_d.ptr) return FAILDESCR;

        /* Save subject globals (SIL: WPTR/XCL/YCL/TCL) */
        const char *save_Σ = Σ;
        int         save_Ω = Ω;
        int         save_Δ = Δ;

        /* Save NAM frame (SIL: NAMICL/NHEDCL) — push fresh ctx.
         * SN-23c: replaces NAME_save()/NAME_discard(cookie) with a child
         * ctx.  Captures inside an EVAL'd expression are local to that
         * expression: the child ctx's entries die on leave, never
         * escaping into the outer match's commit range.  Exactly the
         * semantics the old discard was approximating. */
        NAME_ctx_t eval_ctx;
        NAME_ctx_enter(&eval_ctx);

        DESCR_t result = eval_node((tree_t *)expr_d.ptr);

        /* Restore NAM frame — tear down eval_ctx.  Captures inside an
         * EXPRESSION are local and do not propagate out (same semantics
         * as the pre-SN-23c NAME_discard). */
        NAME_ctx_leave();

        /* Restore subject globals */
        Σ = save_Σ;
        Ω = save_Ω;
        Δ = save_Δ;

        return result;
    }
    if (expr_d.v == DT_C) {
        /* DT_C code block — run via exec_code (no save/restore needed;
         * exec_code is a full statement executor with its own frame) */
        exec_code(expr_d);
        return NULVCL;
    }
    /* Anything else: evaluate as expression string */
    const char *s = VARVAL_fn(expr_d);
    if (!s || !*s) return NULVCL;
    return eval_expr(s);
}

/* ══════════════════════════════════════════════════════════════════════════
 * RT-7: CONVE_fn — compile a string to a DT_E EXPRESSION descriptor
 *
 * SIL CONVE/CONVEX: parse the string as an expression via CMPILE,
 * lower to tree_t IR, wrap in a DT_E descriptor (frozen tree_t*).
 * Returns FAILDESCR on parse failure.
 * ══════════════════════════════════════════════════════════════════════════ */

DESCR_t CONVE_fn(DESCR_t str_d)
{
    const char *s = VARVAL_fn(str_d);
    if (!s || !*s) return FAILDESCR;

    tree_t *tree = parse_expr_pat_from_str(s);
    if (!tree) return FAILDESCR;

    DESCR_t d;
    d.v    = DT_E;
    d.slen = 0;
    d.s    = NULL;   /* clear union first... */
    d.ptr  = tree;   /* ...then store ptr last (ptr and s share union) */
    return d;
}

/* ══════════════════════════════════════════════════════════════════════════
 * RT-7: CODE_fn — compile a string to a DT_C CODE descriptor
 *
 * SIL CODER: parse string as SNOBOL4 statements, return DT_C.
 * This wraps the existing code() function with the DESCR_t input sig.
 * ══════════════════════════════════════════════════════════════════════════ */

DESCR_t CODE_fn(DESCR_t str_d)
{
    const char *s = VARVAL_fn(str_d);
    if (!s || !*s) return FAILDESCR;
    return code(s);
}
