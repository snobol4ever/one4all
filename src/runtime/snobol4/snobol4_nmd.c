#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <gc/gc.h>
#include "snobol4.h"
#include "sil_macros.h"
#include "name_t.h"
typedef struct {
    int         live;
    NAME_t      name;
    const char *substr;
    int         slen;
} NAME_entry_t;
static NAME_ctx_t  g_root_ctx  = { NULL, 0, 0, NULL };
static NAME_ctx_t *g_ctx_current = &g_root_ctx;
/*----------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------*/
static inline NAME_entry_t *ctx_entries(NAME_ctx_t *ctx)
{
    return (NAME_entry_t *)ctx->entries;
}
/*----------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------*/
static void ctx_ensure_capacity(NAME_ctx_t *ctx, int need)
{
    if (need <= ctx->cap) return;
    int newcap = ctx->cap ? ctx->cap * 2 : 64;
    while (newcap < need) newcap *= 2;
    NAME_entry_t *fresh = (NAME_entry_t *)GC_MALLOC((size_t)newcap * sizeof(NAME_entry_t));
    NAME_entry_t *old   = ctx_entries(ctx);
    if (old && ctx->top > 0)
        memcpy(fresh, old, (size_t)ctx->top * sizeof(NAME_entry_t));
    ctx->entries = fresh;
    ctx->cap     = newcap;
}
/*----------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------*/
static const char *dup_substr(const char *s, int len)
{
    if (!s || len <= 0) return "";
    char *copy = (char *)GC_MALLOC((size_t)len + 1);
    memcpy(copy, s, (size_t)len);
    copy[len] = '\0';
    return copy;
}
/*----------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------*/
static inline void *idx_to_handle(int i) { return (void *)(intptr_t)(i + 1); }
/*----------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------*/
void NAME_ctx_enter(NAME_ctx_t *ctx)
{
    if (!ctx) return;
    ctx->entries = NULL;
    ctx->cap     = 0;
    ctx->top     = 0;
    ctx->parent  = g_ctx_current;
    g_ctx_current = ctx;
}
/*----------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------*/
void NAME_ctx_leave(void)
{
    NAME_ctx_t *ctx = g_ctx_current;
    if (!ctx || ctx == &g_root_ctx) return;
    g_ctx_current = ctx->parent;
    ctx->entries = NULL;
    ctx->cap     = 0;
    ctx->top     = 0;
    ctx->parent  = NULL;
}
/*----------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------*/
void *NAME_push(const NAME_t *nm, const char *substr, int slen)
{
    if (!nm) return NULL;
    NAME_ctx_t *ctx = g_ctx_current;
    ctx_ensure_capacity(ctx, ctx->top + 1);
    int idx = ctx->top;
    NAME_entry_t *e = &ctx_entries(ctx)[idx];
    e->live      = 1;
    e->name      = *nm;
    e->substr    = dup_substr(substr, slen);
    e->slen      = (slen > 0) ? slen : 0;
    ctx->top++;
    return idx_to_handle(idx);
}
/*----------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------*/
void NAME_pop(void)
{
    NAME_ctx_t *ctx = g_ctx_current;
    if (ctx->top <= 0) return;
    NAME_entry_t *es = ctx_entries(ctx);
    while (ctx->top > 0 && !es[ctx->top - 1].live) ctx->top--;
    if (ctx->top <= 0) return;
    es[ctx->top - 1].live = 0;
    ctx->top--;
}
/*----------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------*/
static int same_var_target(const NAME_entry_t *a, const NAME_entry_t *b)
{
    if (a->name.kind != b->name.kind) return 0;
    if (a->name.kind == NM_PTR)
        return a->name.var_ptr && a->name.var_ptr == b->name.var_ptr;
    if (a->name.kind == NM_VAR)
        return a->name.var_name && b->name.var_name
               && strcmp(a->name.var_name, b->name.var_name) == 0;
    return 0;
}
/*----------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------*/
void NAME_commit(void)
{
    NAME_ctx_t *ctx = g_ctx_current;
    NAME_entry_t *es = ctx_entries(ctx);
    for (int i = 0; i < ctx->top; i++) {
        NAME_entry_t *e = &es[i];
        if (!e->live) continue;
        if (e->name.kind == NM_VAR || e->name.kind == NM_PTR) {
            int superseded = 0;
            for (int j = i + 1; j < ctx->top; j++) {
                NAME_entry_t *f = &es[j];
                if (!f->live) continue;
                if (f->name.kind == NM_CALL) break;
                if (same_var_target(e, f)) { superseded = 1; break; }
            }
            if (superseded) continue;
        }
        DESCR_t val = { .v = DT_S, .slen = (uint32_t)e->slen,
                        .s = (char *)e->substr };
        name_commit_value(&e->name, val);
    }
    ctx->top = 0;
}
