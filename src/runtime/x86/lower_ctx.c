/*
 * lower_ctx.c — LabelTable implementation for the SM lowering pass (SR-2)
 *
 * Owns the label-resolution table: a name→instruction-index registry
 * with a forward-reference patch list.  The table knows nothing of
 * SM_Program internals — it is a pure name-to-int registry plus a
 * list of (jump_instr_idx, target_name) pairs that labtab_resolve()
 * closes out after all statements have been lowered.
 *
 * Memory: GC_MALLOC / GC_REALLOC / GC_strdup throughout.
 * labtab_free() is retained as a no-op shim so sm_lower.c call sites
 * compile unchanged; the GC reclaims storage automatically.
 *
 * Authors: Lon Jones Cherryholmes · Claude Sonnet 4.6
 * Date: 2026-05-11
 */

#include "lower_ctx.h"
#include "sm_prog.h"   /* sm_patch_jump */

#include <stdio.h>
#include <string.h>
#include <gc/gc.h>

#define LABEL_TABLE_INIT 64

/* ── Lifecycle ──────────────────────────────────────────────────────────── */

void labtab_init(LabelTable *labtab)
{
    labtab->labels      = GC_MALLOC(LABEL_TABLE_INIT * sizeof(LabelEntry));
    labtab->labels_cap  = LABEL_TABLE_INIT;
    labtab->nlabels     = 0;
    labtab->patches     = GC_MALLOC(LABEL_TABLE_INIT * sizeof(PatchEntry));
    labtab->patches_cap = LABEL_TABLE_INIT;
    labtab->npatches    = 0;
}

/* GC reclaims storage; no manual free needed. */
void labtab_free(LabelTable *labtab)
{
    (void)labtab;   /* no-op: GC handles it */
}

/* ── Label definition ───────────────────────────────────────────────────── */

/*
 * Record a defined label → its SM_LABEL instruction index.
 * name is interned via GC_strdup; the table grows by doubling.
 */
void labtab_define(LabelTable *labtab, const char *name, int instr_idx)
{
    if (labtab->nlabels >= labtab->labels_cap) {
        labtab->labels_cap *= 2;
        labtab->labels = GC_REALLOC(labtab->labels,
                                     labtab->labels_cap * sizeof(LabelEntry));
    }
    labtab->labels[labtab->nlabels].name      = GC_strdup(name);
    labtab->labels[labtab->nlabels].instr_idx = instr_idx;
    labtab->nlabels++;
}

/* ── Label lookup ───────────────────────────────────────────────────────── */

/*
 * Find a label by name; returns instr_idx or -1 if not yet defined.
 *
 * SN-26c-stmt637: case-SENSITIVE compare per SN-31 (case-sensitive
 * default).  strcasecmp previously collided distinct labels like
 * `visitEnd` and `VisitEnd` (both present in beauty.sno via the
 * double-function trick), sending SM gotos to the wrong target.
 * IR's goto resolution (interp.c lookup_label_stmt) is case-sensitive
 * — this matches that.
 */
int labtab_find(const LabelTable *labtab, const char *name)
{
    for (int i = 0; i < labtab->nlabels; i++)
        if (strcmp(labtab->labels[i].name, name) == 0)
            return labtab->labels[i].instr_idx;
    return -1;
}

/* ── Forward-reference patches ──────────────────────────────────────────── */

/*
 * Record a forward-reference patch: a jump whose target isn't defined yet.
 * The target name is GC_strdup-interned; resolved by labtab_resolve().
 */
void labtab_patch_later(LabelTable *labtab, int jump_instr_idx, const char *name)
{
    if (labtab->npatches >= labtab->patches_cap) {
        labtab->patches_cap *= 2;
        labtab->patches = GC_REALLOC(labtab->patches,
                                      labtab->patches_cap * sizeof(PatchEntry));
    }
    labtab->patches[labtab->npatches].jump_instr_idx = jump_instr_idx;
    labtab->patches[labtab->npatches].target_name    = GC_strdup(name);
    labtab->npatches++;
}

/* ── Resolution ─────────────────────────────────────────────────────────── */

/*
 * Resolve all forward patches after all statements have been lowered.
 * Returns 0 on success, -1 if any label was undefined.
 *
 * SNOBOL4 convention: a goto to an undefined label is Error 24.
 * We patch to the last instruction (SM_HALT) so execution terminates
 * cleanly rather than jumping to pc=0 and re-running the program.
 */
int labtab_resolve(LabelTable *labtab, SM_Program *p)
{
    int ok = 0;
    for (int i = 0; i < labtab->npatches; i++) {
        const char *name = labtab->patches[i].target_name;
        int target = labtab_find(labtab, name);
        if (target < 0) {
            fprintf(stderr,
                    "sm_lower: undefined label '%s' treated as Error 24 (halt)\n",
                    name);
            target = (p->count > 0) ? p->count - 1 : 0;
            ok = -1;
        }
        sm_patch_jump(p, labtab->patches[i].jump_instr_idx, target);
    }
    return ok;
}
