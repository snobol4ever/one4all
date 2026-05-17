/*=============================================================================
 * lower_sno.c — Snocone tree_t → portable SNOBOL4 source emitter.
 *
 * Implements `scrip --dump-sno`.  See lower_sno.h for the contract and
 * GOAL-PARSER-SC-TRANSPILE.md for the rung sequence.
 *
 * SCT-1 scope (this file's first cut): emit enough to cover parser_snobol4.sc
 * (253 lines).  Other parsers may emit `* TODO: TT_xxx` placeholders for tags
 * not yet handled — that's intentional and surfaces exactly what's left for
 * SCT-2..SCT-6.
 *===========================================================================*/
#include "lower_sno.h"
#include "../include/ast.h"
#include <stdio.h>
#include <string.h>
#include <stdlib.h>

/*-----------------------------------------------------------------------------
 * State for the emitter — line counter, indent depth (for readability of
 * function bodies; SNOBOL4 doesn't care about column past col 1, but humans do).
 *---------------------------------------------------------------------------*/
typedef struct {
    FILE *out;
    int   lines;
    int   in_stmt;        /* are we mid-statement (no newline yet)? */
    const char *pending_label; /* if non-NULL, next stmt emits this as col-1 label */
} sno_ctx_t;

static void emit(sno_ctx_t *c, const char *fmt, ...);
static void emit_nl(sno_ctx_t *c);
static void emit_node(sno_ctx_t *c, const tree_t *n);
static void emit_stmt(sno_ctx_t *c, const tree_t *stmt);
static void emit_program(sno_ctx_t *c, const tree_t *prog);

#include <stdarg.h>
static void emit(sno_ctx_t *c, const char *fmt, ...) {
    va_list ap; va_start(ap, fmt);
    vfprintf(c->out, fmt, ap);
    va_end(ap);
}
static void emit_nl(sno_ctx_t *c) {
    fputc('\n', c->out);
    c->lines++;
    c->in_stmt = 0;
}

/*-----------------------------------------------------------------------------
 * Safe sval access — many TT_* nodes carry their textual value in v.sval, but
 * some carry an integer or float.  This helper protects against NULL deref
 * when the parser leaves v.sval unset on a node that the emitter expects to
 * be a string (a bug, but the emitter should still produce a diagnostic that
 * makes the bug visible rather than crashing).
 *---------------------------------------------------------------------------*/
static const char *sval_or(const tree_t *n, const char *fallback) {
    if (!n) return fallback;
    if (n->v.sval) return n->v.sval;
    return fallback;
}

/*-----------------------------------------------------------------------------
 * emit_expr — emit a tree_t as a SNOBOL4 expression (not a full statement).
 * Used for SUBJECT, PATTERN, REPLACEMENT components.
 *
 * NOTE on associativity: this version emits parentheses around every binary
 * operator combination.  That's verbose but correct and portable.  A later
 * rung (post-SCT-1) can introduce a SPITBOL priority table (Ch.15) and elide
 * parens where unambiguous.  For SCT-1 the priority is correctness, not
 * elegance.
 *---------------------------------------------------------------------------*/
static void emit_expr(sno_ctx_t *c, const tree_t *e) {
    if (!e) { emit(c, "''"); return; }
    switch (e->t) {
    /*--- Atoms ---------------------------------------------------------------*/
    case TT_QLIT: {
        /* Quoted literal.  Pick the delimiter that doesn't appear in the
         * body.  SNOBOL4 has no backslash escapes (ARCH-SNOCONE.md "String
         * literals — the SNOBOL4 rule"), so if both delimiters appear in
         * the body we have no portable encoding — emit with single quotes
         * and a comment marker so the issue is visible. */
        const char *s = sval_or(e, "");
        int has_sq = strchr(s, '\'') != NULL;
        int has_dq = strchr(s, '"')  != NULL;
        if (!has_sq)        emit(c, "'%s'", s);
        else if (!has_dq)   emit(c, "\"%s\"", s);
        else                emit(c, "'%s'/*BOTH-QUOTES*/", s);
        break;
    }
    case TT_ILIT:
        emit(c, "%lld", e->v.ival);
        break;
    case TT_FLIT:
        emit(c, "%s", sval_or(e, "0.0"));
        break;
    case TT_VAR:
        emit(c, "%s", sval_or(e, "?VAR?"));
        break;
    case TT_KEYWORD:
        /* The parser stores the name without the leading '&'; restore it. */
        emit(c, "&%s", sval_or(e, "?KW?"));
        break;
    case TT_NUL:
        emit(c, "''");
        break;
    /*--- Unary operators (SPITBOL Manual Ch.15 line 9740+) -------------------
     * Emit unary operators tight against their operand.  SCRIP's SNOBOL4
     * parser rejects forms like `(. dummy)` with a space — `.dummy` is
     * the required form.  Same for the other unary operators.  This
     * matches the SPITBOL convention (the manual shows `.X`, `$X`, `*X`
     * etc. throughout).
     */
    case TT_MNS:         emit(c, "(-");  emit_expr(c, e->c[0]); emit(c, ")"); break;
    case TT_PLS:         emit(c, "(+");  emit_expr(c, e->c[0]); emit(c, ")"); break;
    case TT_NOT:         emit(c, "(~");  emit_expr(c, e->c[0]); emit(c, ")"); break;
    case TT_INTERROGATE: emit(c, "(?");  emit_expr(c, e->c[0]); emit(c, ")"); break;
    case TT_DEFER:       emit(c, "(*");  emit_expr(c, e->c[0]); emit(c, ")"); break;
    case TT_INDIRECT:    emit(c, "($");  emit_expr(c, e->c[0]); emit(c, ")"); break;
    case TT_NAME:        emit(c, "(.");  emit_expr(c, e->c[0]); emit(c, ")"); break;
    case TT_CAPT_CURSOR:
        /* Unary `@` — cursor capture.  Used as `@var` in a pattern. */
        emit(c, "(@"); emit_expr(c, e->c[0]); emit(c, ")");
        break;
    /*--- Binary arithmetic (Ch.15 priorities 6,8,9,11) -----------------------*/
    case TT_ADD: emit(c, "("); emit_expr(c, e->c[0]); emit(c, " + "); emit_expr(c, e->c[1]); emit(c, ")"); break;
    case TT_SUB: emit(c, "("); emit_expr(c, e->c[0]); emit(c, " - "); emit_expr(c, e->c[1]); emit(c, ")"); break;
    case TT_MUL: emit(c, "("); emit_expr(c, e->c[0]); emit(c, " * "); emit_expr(c, e->c[1]); emit(c, ")"); break;
    case TT_DIV: emit(c, "("); emit_expr(c, e->c[0]); emit(c, " / "); emit_expr(c, e->c[1]); emit(c, ")"); break;
    case TT_POW: emit(c, "("); emit_expr(c, e->c[0]); emit(c, " ** "); emit_expr(c, e->c[1]); emit(c, ")"); break;
    /*--- Pattern composition --------------------------------------------------
     * TT_SEQ → concatenation (space).  TT_ALT → alternation (|).
     * Per ARCH-SNOCONE.md, space is the portable form for concat/match
     * (priority 4 right-assoc).  Standard SNOBOL4 has only space-as-match;
     * `?` is a SPITBOL-only explicit form. */
    case TT_SEQ:
    case TT_CAT: {
        int i;
        if (e->n == 0) { emit(c, "''"); break; }
        emit(c, "(");
        for (i = 0; i < e->n; i++) {
            if (i) emit(c, " ");
            emit_expr(c, e->c[i]);
        }
        emit(c, ")");
        break;
    }
    case TT_ALT: {
        int i;
        if (e->n == 0) { emit(c, "FAIL"); break; }
        emit(c, "(");
        for (i = 0; i < e->n; i++) {
            if (i) emit(c, " | ");
            emit_expr(c, e->c[i]);
        }
        emit(c, ")");
        break;
    }
    case TT_VLIST: {
        /* Alternative-evaluation (Ch.15 footnote): (e1, e2, e3) tries each
         * left-to-right; the first to succeed yields the value.  This is a
         * SPITBOL extension — portable but not standard SNOBOL4. */
        int i;
        emit(c, "(");
        for (i = 0; i < e->n; i++) {
            if (i) emit(c, ", ");
            emit_expr(c, e->c[i]);
        }
        emit(c, ")");
        break;
    }
    /*--- Pattern-capture binary operators (Ch.15 line 9885+) ----------------*/
    case TT_CAPT_COND_ASGN:
        /* pat . var — assigns if outer match succeeds */
        emit(c, "("); emit_expr(c, e->c[0]); emit(c, " . "); emit_expr(c, e->c[1]); emit(c, ")");
        break;
    case TT_CAPT_IMMED_ASGN:
        /* pat $ var — assigns as soon as pat matches */
        emit(c, "("); emit_expr(c, e->c[0]); emit(c, " $ "); emit_expr(c, e->c[1]); emit(c, ")");
        break;
    /*--- Function call -------------------------------------------------------*/
    case TT_FNC: {
        /* v.sval holds the function name; children are arguments. */
        int i;
        emit(c, "%s(", sval_or(e, "?FN?"));
        for (i = 0; i < e->n; i++) {
            if (i) emit(c, ",");
            emit_expr(c, e->c[i]);
        }
        emit(c, ")");
        break;
    }
    case TT_IDX: {
        /* base[idx] */
        emit_expr(c, e->c[0]);
        emit(c, "<");
        emit_expr(c, e->c[1]);
        emit(c, ">");
        break;
    }
    /*--- Pattern primitives (named in SPITBOL Ch.18/Ch.19) -------------------*/
    case TT_ARB:     emit(c, "ARB"); break;
    case TT_REM:     emit(c, "REM"); break;
    case TT_BAL:     emit(c, "BAL"); break;
    case TT_FAIL:    emit(c, "FAIL"); break;
    case TT_SUCCEED: emit(c, "SUCCEED"); break;
    case TT_ABORT:   emit(c, "ABORT"); break;
    /* FENCE/ARBNO are emitted as TT_FNC most of the time; the bare TT_FENCE
     * tag is for the zero-arg form, the TT_FNC('FENCE', expr) form is for
     * FENCE(pat).  Same for ARBNO. */
    case TT_FENCE:
        if (e->n == 0) emit(c, "FENCE");
        else { emit(c, "FENCE("); emit_expr(c, e->c[0]); emit(c, ")"); }
        break;
    case TT_ARBNO:
        if (e->n == 1) { emit(c, "ARBNO("); emit_expr(c, e->c[0]); emit(c, ")"); }
        else           { emit(c, "ARBNO()"); /* invalid; placeholder */ }
        break;
    case TT_ASSIGN:
        /* Embedded assignment expression: (a = b) — SPITBOL extension. */
        emit(c, "("); emit_expr(c, e->c[0]); emit(c, " = "); emit_expr(c, e->c[1]); emit(c, ")");
        break;
    case TT_SCAN:
        /* Embedded pattern match: (subj ? pat) — SPITBOL extension.
         * Note: the explicit `?` form is SPITBOL-only.  For maximum portability
         * a future rung may rewrite (subj ? pat) into an inline SUBJ PAT
         * statement, but inside an expression there is no portable form.
         * Emit as space-match in parens to keep standard-SNOBOL4 compatibility. */
        emit(c, "("); emit_expr(c, e->c[0]); emit(c, " "); emit_expr(c, e->c[1]); emit(c, ")");
        break;
    /*--- Default: emit a diagnostic so the missing node-kind is visible -----
     * NOTE: This emits a valid SNOBOL4 expression (a string literal) so the
     * surrounding statement still parses.  When this string appears in the
     * output it surfaces "TT_NN needs handling in lower_sno.c" without
     * making the rest of the file invalid.  Use `grep '?TT_'` on the
     * --dump-sno output to find the unhandled tags.
     *---------------------------------------------------------------------*/
    default:
        emit(c, "'?TT_%d?'", (int)e->t);
        break;
    }
}

/*-----------------------------------------------------------------------------
 * emit_stmt — emit a TT_STMT (or label/goto/return node) as a full SNOBOL4
 * statement line.  Per ARCH-SNOCONE.md "Lowering map" and SPITBOL Ch.14
 * the form is `LABEL  SUBJECT  PATTERN  = REPLACEMENT  :GOTO`.  Children of
 * TT_STMT are tagged-shape nodes (:subj, :pat, :repl, :eq, :go, :goS, :goF,
 * :lbl) — the same shape parser_*.sc emits.
 *---------------------------------------------------------------------------*/
static void emit_stmt(sno_ctx_t *c, const tree_t *s) {
    int i;
    const tree_t *subj = NULL, *pat = NULL, *repl = NULL;
    const tree_t *go_s = NULL, *go_f = NULL, *go_u = NULL;
    const tree_t *lbl = NULL;
    int has_eq = 0;

    if (!s) { emit_nl(c); return; }

    /* TT_STMT has children whose tags are descriptive strings stored in v.sval
     * (e.g. ":subj", ":pat", ":repl", ":eq", ":goS").  Walk and bin them. */
    if (s->t == TT_STMT) {
        for (i = 0; i < s->n; i++) {
            const tree_t *ch = s->c[i];
            const char *tag = sval_or(ch, "");
            if      (!strcmp(tag, ":subj")) subj = ch->n ? ch->c[0] : NULL;
            else if (!strcmp(tag, ":pat"))  pat  = ch->n ? ch->c[0] : NULL;
            else if (!strcmp(tag, ":repl")) repl = ch->n ? ch->c[0] : NULL;
            else if (!strcmp(tag, ":eq"))   has_eq = 1;
            else if (!strcmp(tag, ":goS") || ch->t == TT_GOTO_S) go_s = ch;
            else if (!strcmp(tag, ":goF") || ch->t == TT_GOTO_F) go_f = ch;
            else if (!strcmp(tag, ":go")  || ch->t == TT_GOTO_U) go_u = ch;
            else if (!strcmp(tag, ":lbl")) lbl  = ch;
        }
    }

    /* TT_DEFINE as the statement subject — Snocone `function name(args) { body }`.
     * Emit the Gimpel template (SPITBOL Manual Ch.8 line 5973):
     *
     *     DEFINE('name(args)locals')    :(name_end)
     * name body
     *     name = result                 :(RETURN)
     * name_end
     *
     * TT_DEFINE has three children:
     *   c[0] = TT_QLIT  -- function name
     *   c[1] = TT_QLIT  -- prototype string "name(arg1,arg2)" (DEFINE() arg)
     *   c[2] = TT_PROGRAM -- function body
     */
    if (subj && subj->t == TT_DEFINE && subj->n >= 3) {
        const char *fname = (subj->c[0] && subj->c[0]->v.sval) ? subj->c[0]->v.sval : "_fn";
        const char *proto = (subj->c[1] && subj->c[1]->v.sval) ? subj->c[1]->v.sval : "_fn()";
        /* Gimpel template — SPITBOL Manual Ch.8 line 5973:
         *
         *     DEFINE('proto')              :(name_end)
         * name first_body_stmt
         *     ...rest of body...
         *     :(RETURN)
         * name_end OUTPUT =
         *
         * NOTE 1: The function-name label MUST share a line with the
         * first body statement.  SCRIP's SNOBOL4 parser rejects a
         * label-only line (label at col 1, no subject/pattern/repl/goto).
         * The `pending_label` context field threads the name through to
         * the first emit_stmt call so it lands on the correct line.
         *
         * NOTE 2: The end-label `name_end` also can't be label-only.
         * Pad it with a no-op `OUTPUT =` (assigns empty string to
         * OUTPUT, which simply emits a blank line — semantically
         * acceptable when this label is jumped to only as the skip-
         * over-body target).  Alternative: `name_end &CASE = &CASE`
         * which is purely no-op, but `OUTPUT =` is what beauty.sno
         * conventionally uses for this idiom.
         */
        emit(c, "\tDEFINE('%s')\t:(%s_end)", proto, fname);
        emit_nl(c);
        if (subj->c[2] && subj->c[2]->t == TT_PROGRAM && subj->c[2]->n > 0) {
            int j;
            c->pending_label = fname;
            for (j = 0; j < subj->c[2]->n; j++) emit_node(c, subj->c[2]->c[j]);
            /* If pending_label is still set, the body never emitted a
             * statement (e.g. empty body).  Force-emit a no-op so the
             * label has a host line. */
            if (c->pending_label) {
                emit(c, "%s\tOUTPUT =", c->pending_label);
                emit_nl(c);
                c->pending_label = NULL;
            }
        } else {
            /* Empty body — emit a no-op statement carrying the label. */
            emit(c, "%s\tOUTPUT =", fname);
            emit_nl(c);
        }
        emit(c, "\t:(RETURN)");
        emit_nl(c);
        emit(c, "%s_end\tOUTPUT =", fname);
        emit_nl(c);
        return;
    }

    /* Column-1 label, if any.  pending_label from the context takes
     * precedence over a :lbl child (used to inject the function-name
     * label onto the first body statement in the Gimpel template). */
    if (c->pending_label) {
        emit(c, "%s\t", c->pending_label);
        c->pending_label = NULL;
    } else if (lbl) {
        emit(c, "%s\t", sval_or(lbl, "L"));
    } else {
        emit(c, "\t");
    }

    /* Subject (and optional pattern, replacement). */
    if (subj) emit_expr(c, subj);
    if (pat)  { emit(c, " "); emit_expr(c, pat); }
    if (has_eq) {
        emit(c, " =");
        if (repl) { emit(c, " "); emit_expr(c, repl); }
    } else if (repl) {
        /* Pattern-match with replacement is the only form that has a repl
         * without :eq; but :eq is required by Snocone's emitter.  If we see
         * repl without :eq, emit it anyway so a missing :eq is visible. */
        emit(c, " = "); emit_expr(c, repl);
    }

    /* Goto field. */
    if (go_s || go_f || go_u) {
        emit(c, "\t:");
        if (go_s) emit(c, "S(%s)", sval_or(go_s, "L"));
        if (go_f) emit(c, "F(%s)", sval_or(go_f, "L"));
        if (go_u) emit(c, "(%s)",  sval_or(go_u, "L"));
    }
    emit_nl(c);
}

/*-----------------------------------------------------------------------------
 * emit_node — dispatch for top-level nodes inside a TT_PROGRAM body.
 * Knows about: TT_STMT (a regular statement), TT_DEFINE (function definition
 * — emits the Gimpel template), TT_PROGRAM (nested block — recurse).
 *---------------------------------------------------------------------------*/
static void emit_node(sno_ctx_t *c, const tree_t *n) {
    if (!n) return;
    switch (n->t) {
    case TT_STMT:
        emit_stmt(c, n);
        break;
    case TT_PROGRAM:
        emit_program(c, n);
        break;
    case TT_DEFINE:
        /* Top-level function definition — Snocone `function f(args) { body }`.
         * The frontend currently emits this as a sequence of TT_STMT nodes
         * including DEFINE(...), goto-around, label, body, end-label.  When
         * that's the shape coming in, the children handle themselves and we
         * fall through here only if a parser produces a literal TT_DEFINE
         * top-level wrapper.  Pass children through. */
        {
            int i;
            for (i = 0; i < n->n; i++) emit_node(c, n->c[i]);
        }
        break;
    case TT_RETURN:
        /* `return` in a Snocone function body — emit `:(RETURN)` goto. */
        emit(c, "\t:(RETURN)");
        emit_nl(c);
        break;
    case TT_PROC_FAIL:
        /* `freturn` — failure return.  Emit `:(FRETURN)`. */
        emit(c, "\t:(FRETURN)");
        emit_nl(c);
        break;
    case TT_NRETURN:
        /* `nreturn` — name return.  Emit `:(NRETURN)`. */
        emit(c, "\t:(NRETURN)");
        emit_nl(c);
        break;
    case TT_END:
        /* End-of-program marker — handled by the program-level emitter. */
        break;
    default:
        /* A bare expression at top level — emit it as a statement with no
         * label and no goto.  Bare-expr statements may fail silently per
         * ARCH-SNOCONE.md. */
        if (c->pending_label) {
            emit(c, "%s\t", c->pending_label);
            c->pending_label = NULL;
        } else {
            emit(c, "\t");
        }
        emit_expr(c, n);
        emit_nl(c);
        break;
    }
}

static void emit_program(sno_ctx_t *c, const tree_t *prog) {
    int i;
    if (!prog) return;
    for (i = 0; i < prog->n; i++) emit_node(c, prog->c[i]);
}

/*-----------------------------------------------------------------------------
 * Public entry — emit a complete program.  Wraps with the &FULLSCAN = 1
 * prelude required by Snocone (parser_snocone.sc line 1) and the closing END
 * statement required by SPITBOL Ch.14.
 *---------------------------------------------------------------------------*/
int tree_to_sno(const tree_t *ast, FILE *out) {
    sno_ctx_t c = { out, 0, 0, NULL };
    if (!ast || !out) return -1;
    /* Prelude — Snocone parser_*.sc files all expect &FULLSCAN nonzero.
     * SPITBOL accepts this as a no-op; standard SNOBOL4 requires it for
     * deferred-pattern semantics to work the Snocone way. */
    emit(&c, "* Generated by scrip --dump-sno  (SCT-1, lower_sno.c)\n");
    emit(&c, "\t&FULLSCAN = 1\n");
    c.lines += 2;
    /* The AST root is normally TT_PROGRAM.  If it's a bare node, wrap it. */
    if (ast->t == TT_PROGRAM) emit_program(&c, ast);
    else                      emit_node(&c, ast);
    emit(&c, "END\n");
    c.lines++;
    return c.lines;
}
