/*
 * rebus_lower.c — Rebus AST → unified IR lowering pass
 *
 * Milestone: M-G5-LOWER-REBUS-FIX
 * Authored: 2026-03-30, G-9 s19, Claude Sonnet 4.6
 *
 * Walks RProgram* (from rebus_parse()) and produces CODE_t*
 * (STMT_t list with tree_t nodes using tree_e values).
 *
 * Architecture per archive/doc/IR_LOWER_REBUS.md:
 *   - P-component (pattern) → SNOBOL4 tree_e pool
 *   - L-component (control) → Icon tree_e pool / label+goto STMT_t chains
 *
 * Control-flow lowering uses the same label/goto STMT_t pattern as
 * snocone_control.c.  Each structured construct becomes labeled SNOBOL4-style
 * STMT_t nodes with flat goto_s/goto_f/goto_u fields (RS-1).
 */

#include "rebus.h"
#include "rebus_lower.h"
#include "../../frontend/snobol4/scrip_cc.h"
#include "../ast/ast.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* -------------------------------------------------------------------------
 * State
 * ---------------------------------------------------------------------- */

typedef struct {
    CODE_t    *prog;
    const char *filename;
    int         nerrors;
    int         label_ctr;
    /* Current function name (for return value assignment) */
    char       *fname;
    /* Loop stack: label pairs for next/exit */
    char       *loop_top[64];
    char       *loop_end[64];
    int         loop_depth;
} RebLow;

/* -------------------------------------------------------------------------
 * Helpers
 * ---------------------------------------------------------------------- */

static char *newlab(RebLow *L) {
    char buf[32];
    snprintf(buf, sizeof buf, "rb_%d", ++L->label_ctr);
    return strdup(buf);
}

static void emit(RebLow *L, STMT_t *s) {
    if (!L->prog->head) { L->prog->head = L->prog->tail = s; }
    else                { L->prog->tail->next = s; L->prog->tail = s; }
    L->prog->nstmts++;
}

static STMT_t *blank_stmt(void) { return stmt_new(); }

/* Emit a label-only statement (no expression, no goto). */
static void emit_label(RebLow *L, const char *lab) {
    STMT_t *s = blank_stmt();
    s->label = strdup(lab);
    emit(L, s);
}

/* Emit an unconditional goto. */
static void emit_goto(RebLow *L, const char *target) {
    STMT_t *s = blank_stmt();
    s->goto_u = strdup(target);
    emit(L, s);
}

/* -------------------------------------------------------------------------
 * Expression lowering: RExpr* → tree_t*
 * ---------------------------------------------------------------------- */

static tree_t *lower_expr(RebLow *L, RExpr *e);

/* Build TT_FNC node with given name and N children. */
static tree_t *make_fnc(const char *name, int n, ...) {
    tree_t *f = ast_node_new(TT_FNC);
    f->v.sval = strdup(name);
    va_list ap; va_start(ap, n);
    for (int i = 0; i < n; i++)
        expr_add_child(f, va_arg(ap, tree_t *));
    va_end(ap);
    return f;
}

static tree_t *lower_expr(RebLow *L, RExpr *e) {
    if (!e) { tree_t *n = ast_node_new(TT_NUL); return n; }

    switch (e->kind) {

    /* --- Literals --- */
    case RE_STR:  { tree_t *x = ast_node_new(TT_QLIT); x->v.sval = strdup(e->sval); return x; }
    case RE_INT:  { tree_t *x = ast_node_new(TT_ILIT); x->v.ival = e->ival;          return x; }
    case RE_REAL: { tree_t *x = ast_node_new(TT_FLIT); x->v.dval = e->dval;          return x; }
    case RE_NULL: { return ast_node_new(TT_NUL); }

    /* --- References --- */
    case RE_VAR:     { tree_t *x = ast_node_new(TT_VAR); x->v.sval = strdup(e->sval); return x; }
    case RE_KEYWORD: { tree_t *x = ast_node_new(TT_KEYWORD);  x->v.sval = strdup(e->sval); return x; }

    /* --- Unary arithmetic --- */
    case RE_NEG:   return expr_unary(TT_MNS, lower_expr(L, e->left));
    case RE_POS:   return lower_expr(L, e->left); /* identity — drop */
    case RE_NOT:   return make_fnc("DIFFER", 1, lower_expr(L, e->left));
    case RE_VALUE: return make_fnc("IDENT",  1, lower_expr(L, e->left));

    /* --- Icon generator bang --- */
    case RE_BANG:   return expr_unary(TT_ITERATE,  lower_expr(L, e->left));

    /* --- Binary arithmetic --- */
    case RE_ADD: return expr_binary(TT_ADD,    lower_expr(L,e->left), lower_expr(L,e->right));
    case RE_SUB: return expr_binary(TT_SUB,    lower_expr(L,e->left), lower_expr(L,e->right));
    case RE_MUL: return expr_binary(TT_MUL,    lower_expr(L,e->left), lower_expr(L,e->right));
    case RE_DIV: return expr_binary(TT_DIV,    lower_expr(L,e->left), lower_expr(L,e->right));
    case RE_POW: return expr_binary(TT_POW,    lower_expr(L,e->left), lower_expr(L,e->right));
    case RE_MOD: return make_fnc("REMDR", 2,  lower_expr(L,e->left), lower_expr(L,e->right));

    /* --- String/pattern --- */
    case RE_STRCAT: return expr_binary(TT_CAT, lower_expr(L,e->left), lower_expr(L,e->right));
    case RE_PATCAT: return expr_binary(TT_CAT, lower_expr(L,e->left), lower_expr(L,e->right));
    case RE_ALT:    return expr_binary(TT_ALT,    lower_expr(L,e->left), lower_expr(L,e->right));

    /* --- Comparison (SNOBOL4 pool) --- */
    case RE_EQ:  return make_fnc("EQ",     2, lower_expr(L,e->left), lower_expr(L,e->right));
    case RE_NE:  return make_fnc("NE",     2, lower_expr(L,e->left), lower_expr(L,e->right));
    case RE_LT:  return make_fnc("LT",     2, lower_expr(L,e->left), lower_expr(L,e->right));
    case RE_LE:  return make_fnc("LE",     2, lower_expr(L,e->left), lower_expr(L,e->right));
    case RE_GT:  return make_fnc("GT",     2, lower_expr(L,e->left), lower_expr(L,e->right));
    case RE_GE:  return make_fnc("GE",     2, lower_expr(L,e->left), lower_expr(L,e->right));
    case RE_SEQ: return make_fnc("IDENT",  2, lower_expr(L,e->left), lower_expr(L,e->right));
    case RE_SNE: return make_fnc("DIFFER", 2, lower_expr(L,e->left), lower_expr(L,e->right));
    case RE_SLT: return make_fnc("LLT",    2, lower_expr(L,e->left), lower_expr(L,e->right));
    case RE_SLE: return make_fnc("LLE",    2, lower_expr(L,e->left), lower_expr(L,e->right));
    case RE_SGT: return make_fnc("LGT",    2, lower_expr(L,e->left), lower_expr(L,e->right));
    case RE_SGE: return make_fnc("LGE",    2, lower_expr(L,e->left), lower_expr(L,e->right));

    /* --- Assignment --- */
    case RE_ASSIGN:
        return expr_binary(TT_ASSIGN, lower_expr(L,e->left), lower_expr(L,e->right));
    case RE_EXCHANGE:
        return make_fnc("EXCHG", 2, lower_expr(L,e->left), lower_expr(L,e->right));
    case RE_ADDASSIGN:
        return expr_binary(TT_ASSIGN, lower_expr(L,e->left),
               expr_binary(TT_ADD, lower_expr(L,e->left), lower_expr(L,e->right)));
    case RE_SUBASSIGN:
        return expr_binary(TT_ASSIGN, lower_expr(L,e->left),
               expr_binary(TT_SUB, lower_expr(L,e->left), lower_expr(L,e->right)));
    case RE_CATASSIGN:
        return expr_binary(TT_ASSIGN, lower_expr(L,e->left),
               expr_binary(TT_CAT, lower_expr(L,e->left), lower_expr(L,e->right)));

    /* --- Call / subscript --- */
    case RE_CALL: {
        tree_t *f = ast_node_new(TT_FNC);
        f->v.sval = strdup(e->sval);
        for (int i = 0; i < e->nargs; i++)
            expr_add_child(f, lower_expr(L, e->args[i]));
        return f;
    }
    case RE_SUB_IDX: {
        tree_t *f = ast_node_new(TT_IDX);
        expr_add_child(f, lower_expr(L, e->left));
        for (int i = 0; i < e->nargs; i++)
            expr_add_child(f, lower_expr(L, e->args[i]));
        return f;
    }
    case RE_RANGE:
        return expr_binary(TT_IDX, lower_expr(L,e->left), lower_expr(L,e->right));

    /* --- Pattern captures (SNOBOL4 pool) --- */
    case RE_COND:   return expr_binary(TT_CAPT_COND_ASGN, lower_expr(L,e->left), lower_expr(L,e->right));
    case RE_IMM:    return expr_binary(TT_CAPT_IMMED_ASGN,  lower_expr(L,e->left), lower_expr(L,e->right));
    case RE_CURSOR: { tree_t *x = ast_node_new(TT_CAPT_CURSOR); x->v.sval = strdup(e->sval); return x; }
    case RE_DEREF:  return expr_unary(TT_INDIRECT, lower_expr(L, e->left));
    case RE_PATOPT: return expr_unary(TT_ARBNO, lower_expr(L, e->left)); /* ~pat → TT_ARBNO */

    /* --- Augmented (generic) --- */
    case RE_AUG:
        /* Synthesize: lhs := lhs augop rhs */
        {
            tree_e op;
            switch (e->augop) {
            case RE_ADD: op = TT_ADD; break;
            case RE_SUB: op = TT_SUB; break;
            case RE_MUL: op = TT_MUL; break;
            case RE_DIV: op = TT_DIV; break;
            default:     op = TT_CAT; break;
            }
            return expr_binary(TT_ASSIGN, lower_expr(L,e->left),
                   expr_binary(op, lower_expr(L,e->left), lower_expr(L,e->right)));
        }

    /* --- RS_ASSIGN forwarded through RExpr --- */
    case RE_ASSIGN + 100: /* guard: shouldn't reach here */ break;

    default:
        fprintf(stderr, "rebus_lower: unhandled REKind %d at line %d\n",
                e->kind, e->lineno);
        L->nerrors++;
        return ast_node_new(TT_NUL);
    }
    return ast_node_new(TT_NUL);
}

/* -------------------------------------------------------------------------
 * Statement lowering: RStmt* → STMT_t chain
 * ---------------------------------------------------------------------- */

static void lower_stmt(RebLow *L, RStmt *s);

static void lower_stmt(RebLow *L, RStmt *s) {
    if (!s) return;

    switch (s->kind) {

    case RS_EXPR:
    case RS_ASSIGN: {
        /* If the top-level expression is a simple assignment (lhs := rhs),
         * emit as a STMT_t with subject=lhs, has_eq=1, replacement=rhs.
         * This routes through the statement-level assignment path which
         * correctly handles OUTPUT, INPUT, and other SNOBOL4 keywords.
         * Nested assignments (e.g. x := y := z) still go through TT_ASSIGN. */
        RExpr *ex = s->expr;
        if (ex && (ex->kind == RE_ASSIGN    ||
                   ex->kind == RE_ADDASSIGN ||
                   ex->kind == RE_SUBASSIGN ||
                   ex->kind == RE_CATASSIGN) && ex->left) {
            STMT_t *st = blank_stmt();
            st->lineno = s->lineno;
            if (ex->kind == RE_ASSIGN) {
                st->subject     = lower_expr(L, ex->left);
                st->replacement = lower_expr(L, ex->right);
                st->has_eq      = 1;
            } else {
                /* x +:= e  →  subject=x, has_eq=1, replacement=x+e */
                tree_t *lhs = lower_expr(L, ex->left);
                tree_t *rhs = lower_expr(L, ex->right);
                tree_e   op  = (ex->kind == RE_ADDASSIGN) ? TT_ADD :
                              (ex->kind == RE_SUBASSIGN) ? TT_SUB : TT_CAT;
                /* Need a fresh copy of lhs for the rhs operand */
                tree_t *lhs2 = lower_expr(L, ex->left);
                st->subject     = lhs;
                st->replacement = expr_binary(op, lhs2, rhs);
                st->has_eq      = 1;
            }
            emit(L, st);
        } else {
            STMT_t *st = blank_stmt();
            st->lineno  = s->lineno;
            st->subject = lower_expr(L, ex);
            emit(L, st);
        }
        break;
    }

    case RS_MATCH: {
        STMT_t *st = blank_stmt();
        st->lineno  = s->lineno;
        st->subject = lower_expr(L, s->expr);
        st->pattern = lower_expr(L, s->pat);
        emit(L, st);
        break;
    }

    case RS_REPLACE: {
        STMT_t *st = blank_stmt();
        st->lineno      = s->lineno;
        st->subject     = lower_expr(L, s->expr);
        st->pattern     = lower_expr(L, s->pat);
        st->replacement = lower_expr(L, s->repl);
        emit(L, st);
        break;
    }

    case RS_REPLN: {
        STMT_t *st = blank_stmt();
        st->lineno      = s->lineno;
        st->subject     = lower_expr(L, s->expr);
        st->pattern     = lower_expr(L, s->pat);
        st->replacement = ast_node_new(TT_NUL);
        emit(L, st);
        break;
    }

    /* --- if cond then s1 [else s2]
     *   [cond stmt] :S(L_then) :F(L_else)
     *   L_then: [s1]  goto L_end
     *   L_else: [s2]   (omitted if no alt)
     *   L_end:
     */
    case RS_IF: {
        char *l_then = newlab(L);
        char *l_else = newlab(L);
        char *l_end  = newlab(L);
        STMT_t *cst  = blank_stmt();
        cst->lineno  = s->lineno;
        cst->subject = lower_expr(L, s->expr);
        cst->goto_s = strdup(l_then);
        cst->goto_f = strdup(l_else);
        emit(L, cst);
        emit_label(L, l_then);
        lower_stmt(L, s->body);
        emit_goto(L, l_end);
        emit_label(L, l_else);
        if (s->alt) lower_stmt(L, s->alt);
        emit_label(L, l_end);
        free(l_then); free(l_else); free(l_end);
        break;
    }

    /* --- unless cond then s
     *   [cond stmt] :S(L_end) :F(L_body)
     *   L_body: [s]
     *   L_end:
     */
    case RS_UNLESS: {
        char *l_body = newlab(L);
        char *l_end  = newlab(L);
        STMT_t *cst  = blank_stmt();
        cst->lineno  = s->lineno;
        cst->subject = lower_expr(L, s->expr);
        cst->goto_s = strdup(l_end);
        cst->goto_f = strdup(l_body);
        emit(L, cst);
        emit_label(L, l_body);
        lower_stmt(L, s->body);
        emit_label(L, l_end);
        free(l_body); free(l_end);
        break;
    }

    /* --- while cond do s
     *   L_top: [cond] :S(L_body) :F(L_end)
     *   L_body: [s]   goto L_top
     *   L_end:
     */
    case RS_WHILE: {
        char *l_top  = newlab(L);
        char *l_body = newlab(L);
        char *l_end  = newlab(L);
        L->loop_top[L->loop_depth]   = l_top;
        L->loop_end[L->loop_depth++] = l_end;
        emit_label(L, l_top);
        STMT_t *cst  = blank_stmt();
        cst->lineno  = s->lineno;
        cst->subject = lower_expr(L, s->expr);
        cst->goto_s = strdup(l_body);
        cst->goto_f = strdup(l_end);
        emit(L, cst);
        emit_label(L, l_body);
        lower_stmt(L, s->body);
        emit_goto(L, l_top);
        emit_label(L, l_end);
        L->loop_depth--;
        free(l_top); free(l_body); free(l_end);
        break;
    }

    /* --- until cond do s (loop while cond FAILS)
     *   L_top: [cond] :S(L_end) :F(L_body)
     *   L_body: [s]   goto L_top
     *   L_end:
     */
    case RS_UNTIL: {
        char *l_top  = newlab(L);
        char *l_body = newlab(L);
        char *l_end  = newlab(L);
        L->loop_top[L->loop_depth]   = l_top;
        L->loop_end[L->loop_depth++] = l_end;
        emit_label(L, l_top);
        STMT_t *cst  = blank_stmt();
        cst->lineno  = s->lineno;
        cst->subject = lower_expr(L, s->expr);
        cst->goto_s = strdup(l_end);
        cst->goto_f = strdup(l_body);
        emit(L, cst);
        emit_label(L, l_body);
        lower_stmt(L, s->body);
        emit_goto(L, l_top);
        emit_label(L, l_end);
        L->loop_depth--;
        free(l_top); free(l_body); free(l_end);
        break;
    }

    /* --- repeat s  (infinite loop, exit via RS_EXIT)
     *   L_top: [s]  goto L_top
     *   L_end: (reached only via exit)
     */
    case RS_REPEAT: {
        char *l_top = newlab(L);
        char *l_end = newlab(L);
        L->loop_top[L->loop_depth]   = l_top;
        L->loop_end[L->loop_depth++] = l_end;
        emit_label(L, l_top);
        lower_stmt(L, s->body);
        emit_goto(L, l_top);
        emit_label(L, l_end);
        L->loop_depth--;
        free(l_top); free(l_end);
        break;
    }

    /* --- for id from e1 to e2 [by e3] do s
     *   id := e1
     *   L_top: GT(id, e2) :S(L_end)   (or GE if no by)
     *   [s]
     *   id := id + e3  (or + 1)
     *   goto L_top
     *   L_end:
     */
    case RS_FOR: {
        char *l_top = newlab(L);
        char *l_end = newlab(L);
        L->loop_top[L->loop_depth]   = l_top;
        L->loop_end[L->loop_depth++] = l_end;

        /* id := e1 */
        tree_t *var = ast_node_new(TT_VAR); var->v.sval = strdup(s->for_var);
        STMT_t *init = blank_stmt();
        init->subject = expr_binary(TT_ASSIGN, var, lower_expr(L, s->for_from));
        emit(L, init);

        /* L_top: GT(id, e2) :S(L_end) */
        emit_label(L, l_top);
        tree_t *var2 = ast_node_new(TT_VAR); var2->v.sval = strdup(s->for_var);
        STMT_t *test = blank_stmt();
        test->subject = make_fnc("GT", 2, var2, lower_expr(L, s->for_to));
        test->goto_s = strdup(l_end);
        emit(L, test);

        lower_stmt(L, s->body);

        /* id := id + step */
        tree_t *var3 = ast_node_new(TT_VAR); var3->v.sval = strdup(s->for_var);
        tree_t *var4 = ast_node_new(TT_VAR); var4->v.sval = strdup(s->for_var);
        tree_t *step = s->for_by ? lower_expr(L, s->for_by)
                                 : ({ tree_t *one = ast_node_new(TT_ILIT); one->v.ival = 1; one; });
        STMT_t *inc  = blank_stmt();
        inc->subject = expr_binary(TT_ASSIGN, var3,
                       expr_binary(TT_ADD, var4, step));
        emit(L, inc);
        emit_goto(L, l_top);
        emit_label(L, l_end);
        L->loop_depth--;
        free(l_top); free(l_end);
        break;
    }

    /* --- case expr of { clauses }
     *   Lowered as a chain of GT/succeed branches.
     *   case E of { v1: s1; v2: s2; default: sd }
     *   →  EQ(E,v1) :S(L1) :F(try_v2)
     *      L1: [s1]  goto L_end
     *      try_v2: EQ(E,v2) :S(L2) :F(try_def)
     *      L2: [s2]  goto L_end
     *      try_def: [sd]
     *      L_end:
     */
    case RS_CASE: {
        char *l_end = newlab(L);
        /* Materialize case expr into a temp variable for repeated comparison */
        char tmpbuf[32];
        snprintf(tmpbuf, sizeof tmpbuf, "rb_case_%d", L->label_ctr);
        tree_t *tmpvar = ast_node_new(TT_VAR); tmpvar->v.sval = strdup(tmpbuf);
        STMT_t *assign = blank_stmt();
        assign->subject = expr_binary(TT_ASSIGN, tmpvar, lower_expr(L, s->case_expr));
        emit(L, assign);

        char *l_next = NULL;
        for (RCase *c = s->cases; c; c = c->next) {
            if (l_next) { emit_label(L, l_next); free(l_next); l_next = NULL; }
            if (c->is_default) {
                lower_stmt(L, c->body);
                emit_goto(L, l_end);
            } else {
                char *l_match = newlab(L);
                l_next = newlab(L);
                tree_t *tv = ast_node_new(TT_VAR); tv->v.sval = strdup(tmpbuf);
                STMT_t *cst = blank_stmt();
                cst->subject = make_fnc("IDENT", 2, tv, lower_expr(L, c->guard));
                cst->goto_s = strdup(l_match);
                cst->goto_f = strdup(l_next);
                emit(L, cst);
                emit_label(L, l_match);
                lower_stmt(L, c->body);
                emit_goto(L, l_end);
            }
        }
        if (l_next) { emit_label(L, l_next); free(l_next); }
        emit_label(L, l_end);
        free(l_end);
        break;
    }

    /* --- return [expr] */
    case RS_RETURN: {
        if (s->retval && L->fname) {
            /* fname = retval */
            tree_t *fn = ast_node_new(TT_VAR); fn->v.sval = strdup(L->fname);
            STMT_t *assign = blank_stmt();
            assign->subject     = fn;
            assign->replacement = lower_expr(L, s->retval);
            assign->has_eq      = 1;
            emit(L, assign);
        }
        /* :(RETURN) — goto pseudo-label, not a function call */
        emit_goto(L, "RETURN");
        break;
    }

    /* --- fail */
    case RS_FAIL: {
        /* :(FRETURN) — goto pseudo-label */
        emit_goto(L, "FRETURN");
        break;
    }

    /* --- stop */
    case RS_STOP: {
        STMT_t *st = blank_stmt();
        st->is_end = 1;
        emit(L, st);
        break;
    }

    /* --- exit (break from loop) */
    case RS_EXIT: {
        if (L->loop_depth > 0) {
            emit_goto(L, L->loop_end[L->loop_depth - 1]);
        } else {
            STMT_t *st = blank_stmt(); st->is_end = 1; emit(L, st);
        }
        break;
    }

    /* --- next (continue to loop top) */
    case RS_NEXT: {
        if (L->loop_depth > 0)
            emit_goto(L, L->loop_top[L->loop_depth - 1]);
        break;
    }

    /* --- compound { s1; s2; ... } */
    case RS_COMPOUND: {
        for (int i = 0; i < s->nstmts; i++)
            lower_stmt(L, s->stmts[i]);
        /* Also walk ->next chain if present */
        break;
    }

    default:
        fprintf(stderr, "rebus_lower: unhandled RSKind %d at line %d\n",
                s->kind, s->lineno);
        L->nerrors++;
        break;
    }

    /* Walk ->next sibling chain (compound body or sequential stmts) */
    if (s->next) lower_stmt(L, s->next);
}

/* -------------------------------------------------------------------------
 * Declaration lowering
 * ---------------------------------------------------------------------- */

static void lower_decl(RebLow *L, RDecl *d) {
    if (!d) return;
    switch (d->kind) {

    case RD_RECORD: {
        /* DEFINE('Name(f1,f2,...)')  DATA call */
        char buf[1024]; int pos = 0;
        pos += snprintf(buf + pos, sizeof buf - pos, "%s(", d->name);
        for (int i = 0; i < d->nfields; i++) {
            if (i) pos += snprintf(buf + pos, sizeof buf - pos, ",");
            pos += snprintf(buf + pos, sizeof buf - pos, "%s", d->fields[i]);
        }
        snprintf(buf + pos, sizeof buf - pos, ")");
        STMT_t *st = blank_stmt();
        tree_t *arg = ast_node_new(TT_QLIT); arg->v.sval = strdup(buf);
        st->subject = make_fnc("DATA", 1, arg);
        emit(L, st);
        break;
    }

    case RD_FUNCTION: {
        /* DEFINE('Name(p1,...)/l1,...')  then function body between goto-around and label */
        char buf[2048]; int pos = 0;
        pos += snprintf(buf + pos, sizeof buf - pos, "%s(", d->name);
        for (int i = 0; i < d->nparams; i++) {
            if (i) pos += snprintf(buf + pos, sizeof buf - pos, ",");
            pos += snprintf(buf + pos, sizeof buf - pos, "%s", d->params[i]);
        }
        pos += snprintf(buf + pos, sizeof buf - pos, ")");
        if (d->nlocals > 0) {
            pos += snprintf(buf + pos, sizeof buf - pos, "/");
            for (int i = 0; i < d->nlocals; i++) {
                if (i) pos += snprintf(buf + pos, sizeof buf - pos, ",");
                pos += snprintf(buf + pos, sizeof buf - pos, "%s", d->locals[i]);
            }
        }

        /* DEFINE call */
        STMT_t *def_st = blank_stmt();
        tree_t *arg = ast_node_new(TT_QLIT); arg->v.sval = strdup(buf);
        def_st->subject = make_fnc("DEFINE", 1, arg);
        emit(L, def_st);

        /* goto around the body */
        char *l_end = newlab(L);
        emit_goto(L, l_end);

        /* function entry label */
        emit_label(L, d->name);

        /* initial clause (guarded by a flag) */
        if (d->initial) {
            char flagbuf[64];
            snprintf(flagbuf, sizeof flagbuf, "rb_init_%s", d->name);
            tree_t *flag = ast_node_new(TT_VAR); flag->v.sval = strdup(flagbuf);
            char *l_done = newlab(L);
            STMT_t *chk = blank_stmt();
            chk->subject = flag;
            chk->goto_s = strdup(l_done);
            emit(L, chk);
            lower_stmt(L, d->initial);
            /* set flag */
            tree_t *fv = ast_node_new(TT_VAR); fv->v.sval = strdup(flagbuf);
            tree_t *one = ast_node_new(TT_ILIT); one->v.ival = 1;
            STMT_t *fst = blank_stmt();
            fst->subject = expr_binary(TT_ASSIGN, fv, one);
            emit(L, fst);
            emit_label(L, l_done); free(l_done);
        }

        /* body */
        char *saved_fname = L->fname;
        L->fname = strdup(d->name);
        lower_stmt(L, d->body);
        free(L->fname);
        L->fname = saved_fname;

        /* implicit :(RETURN) at end of function — goto pseudo-label, not TT_FNC */
        emit_goto(L, "RETURN");

        emit_label(L, l_end); free(l_end);
        break;
    }
    }

    if (d->next) lower_decl(L, d->next);
}

/* -------------------------------------------------------------------------
 * Public entry point
 * ---------------------------------------------------------------------- */

CODE_t *rebus_lower(RProgram *rp) {
    if (!rp) return NULL;

    RebLow L = {0};
    L.prog     = calloc(1, sizeof(CODE_t));
    L.filename = "<rebus>";

    lower_decl(&L, rp->decls);

    if (L.nerrors > 0) {
        fprintf(stderr, "rebus_lower: %d error(s)\n", L.nerrors);
        return NULL;
    }
    return L.prog;
}

/* -------------------------------------------------------------------------
 * rebus_compile — full pipeline: src string -> TT_PROGRAM via *out_ast
 *
 * FI-1A: mirrors icon_compile().
 * Internally builds CODE_t/STMT_t; code_to_ast() wraps into TT_PROGRAM.
 * Direct AST emission deferred to GOAL-SNOCONE-SM-LOWER.
 * ---------------------------------------------------------------------- */
void rebus_compile(const char *src, const char *filename, tree_t **out_ast) {
    if (!filename) filename = "<stdin>";
    if (out_ast) *out_ast = NULL;

    sno_set_case_sensitive(1);

    FILE *f = fmemopen((void *)src, strlen(src), "r");
    if (!f) {
        fprintf(stderr, "rebus_compile: fmemopen failed\n");
        return;
    }
    RProgram *rp = rebus_parse(f, filename);
    fclose(f);
    if (!rp) {
        fprintf(stderr, "rebus_compile: parse error in %s\n", filename);
        return;
    }

    CODE_t *prog = rebus_lower(rp);
    if (!prog) return;

    for (STMT_t *st = prog->head; st; st = st->next)
        st->lang = LANG_REB;

    STMT_t *call_st = calloc(1, sizeof(STMT_t));
    call_st->subject = make_fnc("MAIN", 0);
    call_st->lang    = LANG_REB;
    if (!prog->head) prog->head = prog->tail = call_st;
    else           { prog->tail->next = call_st; prog->tail = call_st; }
    prog->nstmts++;

    if (out_ast) *out_ast = code_to_ast(prog);
}
