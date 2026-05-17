#include "rebus.h"
#include "rebus_lower.h"
#include "../../frontend/snobol4/scrip_cc.h"
#include "../ast/ast.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
typedef struct {
    CODE_t    *prog;
    const char *filename;
    int         nerrors;
    int         label_ctr;
    char       *fname;
    char       *loop_top[64];
    char       *loop_end[64];
    int         loop_depth;
} RebLow;
/*----------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------*/
static char *newlab(RebLow *L) {
    char buf[32];
    snprintf(buf, sizeof buf, "rb_%d", ++L->label_ctr);
    return strdup(buf);
}
/*----------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------*/
static void emit(RebLow *L, STMT_t *s) {
    if (!L->prog->head) { L->prog->head = L->prog->tail = s; }
    else                { L->prog->tail->next = s; L->prog->tail = s; }
    L->prog->nstmts++;
}
/*----------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------*/
static STMT_t *blank_stmt(void) { return stmt_new(); }
/*----------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------*/
static void emit_label(RebLow *L, const char *lab) {
    STMT_t *s = blank_stmt();
    s->label = strdup(lab);
    emit(L, s);
}
/*----------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------*/
static void emit_goto(RebLow *L, const char *target) {
    STMT_t *s = blank_stmt();
    s->goto_u = strdup(target);
    emit(L, s);
}
/*----------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------*/
static tree_t *make_fnc(const char *name, int n, ...) {
    tree_t *f = ast_node_new(TT_FNC);
    f->v.sval = strdup(name);
    va_list ap; va_start(ap, n);
    for (int i = 0; i < n; i++)
        expr_add_child(f, va_arg(ap, tree_t *));
    va_end(ap);
    return f;
}
/*----------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------*/
/* PST-RB-5c: old-path lower_expr(RExpr*) and lower_stmt(RStmt*) deleted. */
/* PST-RB-5b: walk tree_t produced by the rewritten parser */
static tree_t *lower_tree_expr(RebLow *L, tree_t *e);
/*----------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------*/
static tree_t *lower_tree_expr(RebLow *L, tree_t *e) {
    if (!e) return ast_node_new(TT_NUL);
    switch (e->t) {
    /* Literals and leaves — pass through unchanged */
    case TT_QLIT: case TT_ILIT: case TT_FLIT: case TT_NUL:
    case TT_VAR:  case TT_KEYWORD: case TT_CAPT_CURSOR:
        return e;
    /* Arithmetic / logical unary */
    case TT_MNS:      return expr_unary(TT_MNS, lower_tree_expr(L, e->c[0]));
    case TT_NOT:      return make_fnc("DIFFER", 1, lower_tree_expr(L, e->c[0]));
    case TT_NONNULL:  return make_fnc("IDENT",  1, lower_tree_expr(L, e->c[0]));
    case TT_ITERATE:  return expr_unary(TT_ITERATE, lower_tree_expr(L, e->c[0]));
    case TT_INDIRECT: return expr_unary(TT_INDIRECT, lower_tree_expr(L, e->c[0]));
    /* Arithmetic binary */
    case TT_ADD: return expr_binary(TT_ADD, lower_tree_expr(L,e->c[0]), lower_tree_expr(L,e->c[1]));
    case TT_SUB: return expr_binary(TT_SUB, lower_tree_expr(L,e->c[0]), lower_tree_expr(L,e->c[1]));
    case TT_MUL: return expr_binary(TT_MUL, lower_tree_expr(L,e->c[0]), lower_tree_expr(L,e->c[1]));
    case TT_DIV: return expr_binary(TT_DIV, lower_tree_expr(L,e->c[0]), lower_tree_expr(L,e->c[1]));
    case TT_POW: return expr_binary(TT_POW, lower_tree_expr(L,e->c[0]), lower_tree_expr(L,e->c[1]));
    case TT_MOD: return make_fnc("REMDR", 2, lower_tree_expr(L,e->c[0]), lower_tree_expr(L,e->c[1]));
    case TT_CAT: return expr_binary(TT_CAT, lower_tree_expr(L,e->c[0]), lower_tree_expr(L,e->c[1]));
    case TT_ALT: return expr_binary(TT_ALT, lower_tree_expr(L,e->c[0]), lower_tree_expr(L,e->c[1]));
    /* Comparisons — now TT_EQ etc from parser, lower to FNC calls */
    case TT_EQ:  return make_fnc("EQ",     2, lower_tree_expr(L,e->c[0]), lower_tree_expr(L,e->c[1]));
    case TT_NE:  return make_fnc("NE",     2, lower_tree_expr(L,e->c[0]), lower_tree_expr(L,e->c[1]));
    case TT_LT:  return make_fnc("LT",     2, lower_tree_expr(L,e->c[0]), lower_tree_expr(L,e->c[1]));
    case TT_LE:  return make_fnc("LE",     2, lower_tree_expr(L,e->c[0]), lower_tree_expr(L,e->c[1]));
    case TT_GT:  return make_fnc("GT",     2, lower_tree_expr(L,e->c[0]), lower_tree_expr(L,e->c[1]));
    case TT_GE:  return make_fnc("GE",     2, lower_tree_expr(L,e->c[0]), lower_tree_expr(L,e->c[1]));
    case TT_LEQ: return make_fnc("IDENT",  2, lower_tree_expr(L,e->c[0]), lower_tree_expr(L,e->c[1]));
    case TT_LNE: return make_fnc("DIFFER", 2, lower_tree_expr(L,e->c[0]), lower_tree_expr(L,e->c[1]));
    case TT_LLT: return make_fnc("LLT",    2, lower_tree_expr(L,e->c[0]), lower_tree_expr(L,e->c[1]));
    case TT_LLE: return make_fnc("LLE",    2, lower_tree_expr(L,e->c[0]), lower_tree_expr(L,e->c[1]));
    case TT_LGT: return make_fnc("LGT",    2, lower_tree_expr(L,e->c[0]), lower_tree_expr(L,e->c[1]));
    case TT_LGE: return make_fnc("LGE",    2, lower_tree_expr(L,e->c[0]), lower_tree_expr(L,e->c[1]));
    /* Assignment */
    case TT_ASSIGN: return expr_binary(TT_ASSIGN, lower_tree_expr(L,e->c[0]), lower_tree_expr(L,e->c[1]));
    case TT_SWAP:   return make_fnc("EXCHG", 2, lower_tree_expr(L,e->c[0]), lower_tree_expr(L,e->c[1]));
    /* Captures */
    case TT_CAPT_COND_ASGN:  return expr_binary(TT_CAPT_COND_ASGN,  lower_tree_expr(L,e->c[0]), lower_tree_expr(L,e->c[1]));
    case TT_CAPT_IMMED_ASGN: return expr_binary(TT_CAPT_IMMED_ASGN, lower_tree_expr(L,e->c[0]), lower_tree_expr(L,e->c[1]));
    /* Call / index */
    case TT_FNC: {
        tree_t *f = ast_node_new(TT_FNC);
        f->v.sval = e->v.sval ? strdup(e->v.sval) : NULL;
        for (int i = 0; i < e->n; i++)
            expr_add_child(f, lower_tree_expr(L, e->c[i]));
        return f;
    }
    case TT_IDX: {
        tree_t *idx = ast_node_new(TT_IDX);
        for (int i = 0; i < e->n; i++)
            expr_add_child(idx, lower_tree_expr(L, e->c[i]));
        return idx;
    }
    default:
        fprintf(stderr, "rebus_lower: lower_tree_expr unhandled TT_%d\n", e->t);
        L->nerrors++;
        return ast_node_new(TT_NUL);
    }
}
/*----------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------*/
static void lower_tree_stmt(RebLow *L, tree_t *s) {
    if (!s) return;
    switch (s->t) {
    case TT_PROGRAM: {
        for (int i = 0; i < s->n; i++)
            lower_tree_stmt(L, s->c[i]);
        break;
    }
    case TT_SCAN: {
        STMT_t *st = blank_stmt();
        st->subject = lower_tree_expr(L, s->c[0]);
        st->pattern = lower_tree_expr(L, s->c[1]);
        if (s->n >= 3) st->replacement = lower_tree_expr(L, s->c[2]);
        emit(L, st);
        break;
    }
    case TT_IF: {
        /* c[0]=cond c[1]=then c[2]=else (optional) */
        char *l_then = newlab(L);
        char *l_else = newlab(L);
        char *l_end  = newlab(L);
        STMT_t *cst  = blank_stmt();
        cst->subject = lower_tree_expr(L, s->c[0]);
        cst->goto_s = strdup(l_then);
        cst->goto_f = strdup(l_else);
        emit(L, cst);
        emit_label(L, l_then);
        lower_tree_stmt(L, s->c[1]);
        emit_goto(L, l_end);
        emit_label(L, l_else);
        if (s->n >= 3) lower_tree_stmt(L, s->c[2]);
        emit_label(L, l_end);
        free(l_then); free(l_else); free(l_end);
        break;
    }
    case TT_WHILE: {
        char *l_top  = newlab(L);
        char *l_body = newlab(L);
        char *l_end  = newlab(L);
        L->loop_top[L->loop_depth]   = l_top;
        L->loop_end[L->loop_depth++] = l_end;
        emit_label(L, l_top);
        STMT_t *cst  = blank_stmt();
        cst->subject = lower_tree_expr(L, s->c[0]);
        cst->goto_s = strdup(l_body);
        cst->goto_f = strdup(l_end);
        emit(L, cst);
        emit_label(L, l_body);
        lower_tree_stmt(L, s->c[1]);
        emit_goto(L, l_top);
        emit_label(L, l_end);
        L->loop_depth--;
        free(l_top); free(l_body); free(l_end);
        break;
    }
    case TT_UNTIL: {
        char *l_top  = newlab(L);
        char *l_body = newlab(L);
        char *l_end  = newlab(L);
        L->loop_top[L->loop_depth]   = l_top;
        L->loop_end[L->loop_depth++] = l_end;
        emit_label(L, l_top);
        STMT_t *cst  = blank_stmt();
        cst->subject = lower_tree_expr(L, s->c[0]);
        cst->goto_s = strdup(l_end);
        cst->goto_f = strdup(l_body);
        emit(L, cst);
        emit_label(L, l_body);
        lower_tree_stmt(L, s->c[1]);
        emit_goto(L, l_top);
        emit_label(L, l_end);
        L->loop_depth--;
        free(l_top); free(l_body); free(l_end);
        break;
    }
    case TT_REPEAT: {
        char *l_top = newlab(L);
        char *l_end = newlab(L);
        L->loop_top[L->loop_depth]   = l_top;
        L->loop_end[L->loop_depth++] = l_end;
        emit_label(L, l_top);
        lower_tree_stmt(L, s->c[0]);
        emit_goto(L, l_top);
        emit_label(L, l_end);
        L->loop_depth--;
        free(l_top); free(l_end);
        break;
    }
    case TT_FOR: {
        /* v.sval=var c[0]=from c[1]=to c[2]=by_or_NUL c[3]=body */
        char *l_top = newlab(L);
        char *l_end = newlab(L);
        L->loop_top[L->loop_depth]   = l_top;
        L->loop_end[L->loop_depth++] = l_end;
        tree_t *var = ast_node_new(TT_VAR); var->v.sval = strdup(s->v.sval);
        STMT_t *init = blank_stmt();
        init->subject = expr_binary(TT_ASSIGN, var, lower_tree_expr(L, s->c[0]));
        emit(L, init);
        emit_label(L, l_top);
        tree_t *var2 = ast_node_new(TT_VAR); var2->v.sval = strdup(s->v.sval);
        STMT_t *test = blank_stmt();
        test->subject = make_fnc("GT", 2, var2, lower_tree_expr(L, s->c[1]));
        test->goto_s = strdup(l_end);
        emit(L, test);
        lower_tree_stmt(L, s->c[3]);
        tree_t *var3 = ast_node_new(TT_VAR); var3->v.sval = strdup(s->v.sval);
        tree_t *var4 = ast_node_new(TT_VAR); var4->v.sval = strdup(s->v.sval);
        tree_t *step = (s->c[2] && s->c[2]->t != TT_NUL)
                       ? lower_tree_expr(L, s->c[2])
                       : ({ tree_t *one = ast_node_new(TT_ILIT); one->v.ival = 1; one; });
        STMT_t *inc  = blank_stmt();
        inc->subject = expr_binary(TT_ASSIGN, var3, expr_binary(TT_ADD, var4, step));
        emit(L, inc);
        emit_goto(L, l_top);
        emit_label(L, l_end);
        L->loop_depth--;
        free(l_top); free(l_end);
        break;
    }
    case TT_CASE: {
        /* c[0]=expr c[1..]=TT_IF clauses (guard=c[0], body=c[1]) */
        char *l_end = newlab(L);
        char tmpbuf[32];
        snprintf(tmpbuf, sizeof tmpbuf, "rb_case_%d", L->label_ctr);
        tree_t *tmpvar = ast_node_new(TT_VAR); tmpvar->v.sval = strdup(tmpbuf);
        STMT_t *assign = blank_stmt();
        assign->subject = expr_binary(TT_ASSIGN, tmpvar, lower_tree_expr(L, s->c[0]));
        emit(L, assign);
        char *l_next = NULL;
        for (int i = 1; i < s->n; i++) {
            tree_t *clause = s->c[i]; /* TT_IF node */
            if (l_next) { emit_label(L, l_next); free(l_next); l_next = NULL; }
            tree_t *guard = clause->c[0];
            tree_t *body  = clause->c[1];
            if (guard->t == TT_NUL) {
                /* default clause */
                lower_tree_stmt(L, body);
                emit_goto(L, l_end);
            } else {
                char *l_match = newlab(L);
                l_next = newlab(L);
                tree_t *tv = ast_node_new(TT_VAR); tv->v.sval = strdup(tmpbuf);
                STMT_t *cst = blank_stmt();
                cst->subject = make_fnc("IDENT", 2, tv, lower_tree_expr(L, guard));
                cst->goto_s = strdup(l_match);
                cst->goto_f = strdup(l_next);
                emit(L, cst);
                emit_label(L, l_match);
                lower_tree_stmt(L, body);
                emit_goto(L, l_end);
            }
        }
        if (l_next) { emit_label(L, l_next); free(l_next); }
        emit_label(L, l_end);
        free(l_end);
        break;
    }
    case TT_RETURN: {
        if (s->n > 0 && L->fname) {
            tree_t *fn = ast_node_new(TT_VAR); fn->v.sval = strdup(L->fname);
            STMT_t *assign = blank_stmt();
            assign->subject     = fn;
            assign->replacement = lower_tree_expr(L, s->c[0]);
            assign->has_eq      = 1;
            emit(L, assign);
        }
        emit_goto(L, "RETURN");
        break;
    }
    case TT_PROC_FAIL:  emit_goto(L, "FRETURN"); break;
    case TT_END:        { STMT_t *st = blank_stmt(); st->is_end = 1; emit(L, st); break; }
    case TT_LOOP_BREAK: {
        if (L->loop_depth > 0) emit_goto(L, L->loop_end[L->loop_depth - 1]);
        else { STMT_t *st = blank_stmt(); st->is_end = 1; emit(L, st); }
        break;
    }
    case TT_LOOP_NEXT: {
        if (L->loop_depth > 0) emit_goto(L, L->loop_top[L->loop_depth - 1]);
        break;
    }
    /* Expression-as-statement */
    default: {
        STMT_t *st = blank_stmt();
        tree_t *ex = lower_tree_expr(L, s);
        /* detect assignment at statement level */
        if (ex->t == TT_ASSIGN) {
            st->subject     = ex->c[0];
            st->replacement = ex->c[1];
            st->has_eq      = 1;
        } else {
            st->subject = ex;
        }
        emit(L, st);
        break;
    }
    }
}
/*----------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------*/
static void lower_decl(RebLow *L, RDecl *d) {
    if (!d) return;
    switch (d->kind) {
    case RD_RECORD: {
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
        STMT_t *def_st = blank_stmt();
        tree_t *arg = ast_node_new(TT_QLIT); arg->v.sval = strdup(buf);
        def_st->subject = make_fnc("DEFINE", 1, arg);
        emit(L, def_st);
        char *l_end = newlab(L);
        emit_goto(L, l_end);
        emit_label(L, d->name);
        /* PST-RB-5b: use body_tree and initial_tree (tree_t) */
        if (d->initial_tree) {
            char flagbuf[64];
            snprintf(flagbuf, sizeof flagbuf, "rb_init_%s", d->name);
            tree_t *flag = ast_node_new(TT_VAR); flag->v.sval = strdup(flagbuf);
            char *l_done = newlab(L);
            STMT_t *chk = blank_stmt();
            chk->subject = flag;
            chk->goto_s = strdup(l_done);
            emit(L, chk);
            lower_tree_stmt(L, d->initial_tree);
            tree_t *fv = ast_node_new(TT_VAR); fv->v.sval = strdup(flagbuf);
            tree_t *one = ast_node_new(TT_ILIT); one->v.ival = 1;
            STMT_t *fst = blank_stmt();
            fst->subject = expr_binary(TT_ASSIGN, fv, one);
            emit(L, fst);
            emit_label(L, l_done); free(l_done);
        }
        char *saved_fname = L->fname;
        L->fname = strdup(d->name);
        if (d->body_tree) lower_tree_stmt(L, d->body_tree);
        free(L->fname);
        L->fname = saved_fname;
        emit_goto(L, "RETURN");
        emit_label(L, l_end); free(l_end);
        break;
    }
    }
    if (d->next) lower_decl(L, d->next);
}
/*----------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------*/
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
/*----------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------*/
void rebus_compile(const char *src, const char *filename, tree_t **out_ast) {
    if (!filename) filename = "<stdin>";
    if (out_ast) *out_ast = NULL;
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
