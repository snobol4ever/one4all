/*
 * ast_clone.c — IR tree cloning into GC memory, and CODE_t freeing (RS-9b)
 * AUTHORS: Lon Jones Cherryholmes · Claude Sonnet 4.6 (RS-9b, 2026-05-02)
 */

#include "ast_clone.h"
#include <stdlib.h>
#include <string.h>
#include <gc/gc.h>

/* ── ast_gc_clone ──────────────────────────────────────────────────────────
 * Deep-copy AST_t subtree rooted at e into GC-managed memory.
 * Every field is copied; sval is GC_strdup'd; children[] is GC_malloc'd.
 * The clone is structurally identical but lives in GC heap — safe to keep
 * after the original calloc-based IR is freed. */
AST_t *ast_gc_clone(const AST_t *e)
{
    if (!e) return NULL;
    AST_t *c = GC_malloc(sizeof(AST_t));
    c->kind      = e->kind;
    c->ival      = e->ival;
    c->dval      = e->dval;
    c->id        = e->id;
    c->nchildren = e->nchildren;
    c->nalloc    = e->nchildren;
    c->sval      = e->sval ? GC_strdup(e->sval) : NULL;
    if (e->nchildren > 0) {
        c->children = GC_malloc((size_t)e->nchildren * sizeof(AST_t *));
        for (int i = 0; i < e->nchildren; i++)
            c->children[i] = ast_gc_clone(e->children[i]);
    } else {
        c->children = NULL;
    }
    return c;
}

/* ── expr_free (internal) ───────────────────────────────────────────────────
 * Free a calloc-based AST_t tree recursively. */
static void expr_free(AST_t *e)
{
    if (!e) return;
    free(e->sval);
    for (int i = 0; i < e->nchildren; i++)
        expr_free(e->children[i]);
    free(e->children);
    free(e);
}

/* ── stmt_free (internal) ───────────────────────────────────────────────────
 * Free one STMT_t and all its AST_t fields. */
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
 * Free a CODE_t and all its STMT_t / AST_t nodes.
 * Walks the linked list; frees exports/imports lists.
 * Call only after sm_lower() has consumed the program and any AST_t*
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
