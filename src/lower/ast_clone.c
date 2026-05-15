#include "ast_clone.h"
#include <stdlib.h>
#include <string.h>
#include <gc/gc.h>
/*----------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------*/
tree_t *ast_gc_clone(const tree_t *e)
{
    if (!e) return NULL;
    tree_t *c = GC_malloc(sizeof(tree_t));
    c->t      = e->t;
    c->v.ival      = e->v.ival;
    c->v.dval      = e->v.dval;
    c->_id        = e->_id;
    c->n = e->n;
    c->_nalloc    = e->n;
    switch (e->t) {
        case TT_QLIT: case TT_VAR: case TT_KEYWORD: case TT_FNC:
        case TT_IDX:  case TT_CSET: case TT_ATTR:
            c->v.sval = e->v.sval ? GC_strdup(e->v.sval) : NULL;
            break;
        default:
            break;
    }
    if (e->n > 0) {
        c->c = GC_malloc((size_t)e->n * sizeof(tree_t *));
        for (int i = 0; i < e->n; i++)
            c->c[i] = ast_gc_clone(e->c[i]);
    } else {
        c->c = NULL;
    }
    return c;
}
/*----------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------*/
static void expr_free(tree_t *e)
{
    if (!e) return;
    free(e->v.sval);
    for (int i = 0; i < e->n; i++)
        expr_free(e->c[i]);
    free(e->c);
    free(e);
}
/*----------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------*/
static void stmt_free(STMT_t *s)
{
    if (!s) return;
    free((char *)s->label);
    expr_free(s->subject);
    expr_free(s->pattern);
    expr_free(s->replacement);
    free((char *)s->goto_u);
    free((char *)s->goto_s);
    free((char *)s->goto_f);
    expr_free(s->goto_u_expr);
    expr_free(s->goto_s_expr);
    expr_free(s->goto_f_expr);
    free(s);
}
/*----------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------*/
void code_free(CODE_t *prog)
{
    if (!prog) return;
    STMT_t *s = prog->head;
    while (s) {
        STMT_t *next = s->next;
        stmt_free(s);
        s = next;
    }
    ExportEntry *ex = prog->exports;
    while (ex) {
        ExportEntry *nx = ex->next;
        free((char *)ex->name);
        free(ex);
        ex = nx;
    }
    ImportEntry *im = prog->imports;
    while (im) {
        ImportEntry *nx = im->next;
        free((char *)im->lang);
        free((char *)im->name);
        free((char *)im->method);
        free(im);
        im = nx;
    }
    free(prog);
}
