/*
 * lower_ctx.c — LabelTable implementation and shared lowering helpers
 *
 * LabelTable: two-phase label resolution for SNOBOL4 GOTOs.
 *   Phase 1 (lower_stmt): record defined labels; queue forward-ref patches.
 *   Phase 2 (labtab_resolve): patch all forward refs; undefined → SM_HALT.
 *
 * kw_canonicalize: uppercase a keyword string (GC-allocated, no length cap).
 * expression_scope_walk: populate an IcnScope with variable names from a
 *   proc body AST, skipping globals and initial{} vars (which use NV).
 *
 * Authors: Lon Jones Cherryholmes · Claude Sonnet 4.6
 */

#include "lower_ctx.h"
#include "sm_prog.h"
#include "../../runtime/interp/coro_runtime.h"
#include "../ast/ast.h"
#include <stdio.h>
#include <string.h>
#include <ctype.h>
#include <gc/gc.h>

#define LABTAB_INIT 64

void labtab_init(LabelTable *lt)
{
    lt->labels   = GC_MALLOC(LABTAB_INIT * sizeof(LabelEntry));
    lt->labels_cap = LABTAB_INIT; lt->nlabels = 0;
    lt->patches  = GC_MALLOC(LABTAB_INIT * sizeof(PatchEntry));
    lt->patches_cap = LABTAB_INIT; lt->npatches = 0;
}

void labtab_free(LabelTable *lt) { (void)lt; }  /* GC owns everything */

void labtab_define(LabelTable *lt, const char *name, int instr_idx)
{
    if (lt->nlabels >= lt->labels_cap) {
        lt->labels_cap *= 2;
        lt->labels = GC_REALLOC(lt->labels, lt->labels_cap * sizeof(LabelEntry));
    }
    lt->labels[lt->nlabels].name      = GC_strdup(name);
    lt->labels[lt->nlabels].instr_idx = instr_idx;
    lt->nlabels++;
}

/* Case-sensitive: distinct labels like visitEnd/VisitEnd must not collide. */
int labtab_find(const LabelTable *lt, const char *name)
{
    for (int i = 0; i < lt->nlabels; i++)
        if (strcmp(lt->labels[i].name, name) == 0) return lt->labels[i].instr_idx;
    return -1;
}

void labtab_patch_later(LabelTable *lt, int jump_instr_idx, const char *name)
{
    if (lt->npatches >= lt->patches_cap) {
        lt->patches_cap *= 2;
        lt->patches = GC_REALLOC(lt->patches, lt->patches_cap * sizeof(PatchEntry));
    }
    lt->patches[lt->npatches].jump_instr_idx = jump_instr_idx;
    lt->patches[lt->npatches].target_name    = GC_strdup(name);
    lt->npatches++;
}

/* Undefined goto → Error 24 (patch to SM_HALT). */
int labtab_resolve(LabelTable *lt, SM_Program *p)
{
    int ok = 0;
    for (int i = 0; i < lt->npatches; i++) {
        const char *name = lt->patches[i].target_name;
        int target = labtab_find(lt, name);
        if (target < 0) {
            fprintf(stderr, "sm_lower: undefined label '%s' → Error 24\n", name);
            target = p->count > 0 ? p->count - 1 : 0;
            ok = -1;
        }
        sm_patch_jump(p, lt->patches[i].jump_instr_idx, target);
    }
    return ok;
}

char *kw_canonicalize(const char *raw)
{
    if (!raw) raw = "";
    size_t n = strlen(raw);
    char *buf = GC_MALLOC(n + 1);
    for (size_t i = 0; i < n; i++) buf[i] = (char)toupper((unsigned char)raw[i]);
    buf[n] = '\0';
    return buf;
}

void expression_scope_walk(IcnScope *sc, tree_t *e)
{
    if (!e) return;
    if (e->t == AST_GLOBAL) {
        for (int i = 0; i < e->n; i++)
            if (e->c[i] && e->c[i]->v.sval)
                scope_add(sc, e->c[i]->v.sval);
        return;
    }
    /* Skip initial{} subtrees — vars there must use NV, not frame slots. */
    if (e->t == AST_INITIAL) return;
    if (e->t == AST_VAR && e->v.sval && e->v.sval[0] != '&' && !is_global(e->v.sval))
        scope_add(sc, e->v.sval);
    for (int i = 0; i < e->n; i++)
        expression_scope_walk(sc, e->c[i]);
}
