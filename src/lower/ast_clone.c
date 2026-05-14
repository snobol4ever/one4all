/*
 * ast_clone.c — IR tree cloning into GC memory, and CODE_t freeing (RS-9b)
 * AUTHORS: Lon Jones Cherryholmes · Claude Sonnet 4.6 (RS-9b, 2026-05-02)
 */

#include "ast_clone.h"
#include <stdlib.h>
#include <string.h>
#include <gc/gc.h>

/* ── ast_gc_clone ──────────────────────────────────────────────────────────
 * Deep-copy tree_t subtree rooted at e into GC-managed memory.
 * Every field is copied; sval is GC_strdup'd; children[] is GC_malloc'd.
 * The clone is structurally identical but lives in GC heap — safe to keep
 * after the original calloc-based IR is freed. */
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
    /* Only node types that store a string in v.sval should GC_strdup it.
     * Other types (TT_ILIT, TT_FLIT, TT_AUGOP, etc.) store ival/dval in
     * the same union — calling GC_strdup on an integer cast to char* crashes. */
    switch (e->t) {
        case TT_QLIT: case TT_VAR: case TT_KEYWORD: case TT_FNC:
        case TT_IDX:  case TT_CSET: case TT_ATTR:
            c->v.sval = e->v.sval ? GC_strdup(e->v.sval) : NULL;
            break;
        default:
            /* v already copied via c->v.ival = e->v.ival above */
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

/* ── expr_free (internal) ───────────────────────────────────────────────────
 * Free a calloc-based tree_t tree recursively. */
static void expr_free(tree_t *e)
{
    if (!e) return;
    free(e->v.sval);
    for (int i = 0; i < e->n; i++)
        expr_free(e->c[i]);
    free(e->c);
    free(e);
}

/* ── stmt_free (internal) ───────────────────────────────────────────────────
 * Free one STMT_t and all its tree_t fields. */
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

/* ── code_free ──────────────────────────────────────────────────────────────
 * Free a CODE_t and all its STMT_t / tree_t nodes.
 * Walks the linked list; frees exports/imports lists.
 * Call only after lower() has consumed the program and any tree_t*
 * pointers stored in SM_PUSH_EXPR have been cloned via ast_gc_clone(). */
void code_free(CODE_t *prog)
{
    if (!prog) return;
    STMT_t *s = prog->head;
    while (s) {
        STMT_t *next = s->next;
        stmt_free(s);
        s = next;
    }
    /* Free export/import lists (strings are strdup'd in parser) */
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
