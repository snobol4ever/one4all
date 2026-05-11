/*
 * lower_ctx.c — Lowering context helpers (SR-2, SR-3)
 *
 * SR-2: LabelTable — name→instruction-index registry with forward-reference
 *       patch list.  GC_MALLOC throughout; labtab_free() is a no-op shim.
 * SR-3: emit_goto, kw_canonicalize, expression_scope_walk moved here from
 *       sm_lower.c so cohort files (SR-4+) can use them without a dependency
 *       on the monolithic sm_lower.c.
 *
 * Authors: Lon Jones Cherryholmes · Claude Sonnet 4.6
 * Date: 2026-05-11
 */

#include "lower_ctx.h"
#include "sm_prog.h"

#include "../../runtime/interp/coro_runtime.h"
#include "../ast/ast.h"

#include <stdio.h>
#include <string.h>
#include <ctype.h>
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

/* ── Keyword canonicalization ───────────────────────────────────────────── */

/* Return a GC-allocated uppercase copy of `raw`.  No length cap. */
char *kw_canonicalize(const char *raw)
{
    if (!raw) raw = "";
    size_t n = strlen(raw);
    char *buf = GC_MALLOC(n + 1);
    for (size_t i = 0; i < n; i++)
        buf[i] = (char)toupper((unsigned char)raw[i]);
    buf[n] = '\0';
    return buf;
}

/* ── Proc-body scope walk ───────────────────────────────────────────────── */

/* Walk the proc body AST and populate `sc` with non-global variable names.
 * Mirrors coro_runtime.c's icn_scope_patch without mutating the IR.
 * Globals bridge to the NV store and do not get frame slots.
 * Names starting with '&' are keywords — skipped. */
void expression_scope_walk(IcnScope *sc, AST_t *e)
{
    if (!e) return;
    if (e->kind == AST_GLOBAL) {
        for (int i = 0; i < e->nchildren; i++)
            if (e->children[i] && e->children[i]->sval)
                scope_add(sc, e->children[i]->sval);
        return;
    }
    if (e->kind == AST_VAR && e->sval) {
        if (e->sval[0] != '&' && !is_global(e->sval))
            scope_add(sc, e->sval);
    }
    for (int i = 0; i < e->nchildren; i++)
        expression_scope_walk(sc, e->children[i]);
}

/* ── Emit a goto target (possibly forward ref) ──────────────────────────── */

/* Emit SM_JUMP / SM_JUMP_S / SM_JUMP_F for a named SNOBOL4 goto target.
 * RETURN / FRETURN / NRETURN map to the corresponding return opcodes.
 * All other targets patch immediately if defined; otherwise register a
 * forward patch.  Returns the index of the emitted jump instruction. */
int emit_goto(LowerCtx *c, sm_opcode_t op, const char *target)
{
    SM_Program *p = c->p;
    LabelTable *labtab = &c->labtab;
    if (!target) return -1;

    /* Special targets (case-insensitive per SNOBOL4 spec). */
    if (strcasecmp(target, "RETURN") == 0) {
        if (op == SM_JUMP_S) return sm_emit(p, SM_RETURN_S);
        if (op == SM_JUMP_F) return sm_emit(p, SM_RETURN_F);
        return sm_emit(p, SM_RETURN);
    }
    if (strcasecmp(target, "FRETURN") == 0) {
        if (op == SM_JUMP_S) return sm_emit(p, SM_FRETURN_S);
        if (op == SM_JUMP_F) return sm_emit(p, SM_FRETURN_F);
        return sm_emit(p, SM_FRETURN);
    }
    if (strcasecmp(target, "NRETURN") == 0) {
        if (op == SM_JUMP_S) return sm_emit(p, SM_NRETURN_S);
        if (op == SM_JUMP_F) return sm_emit(p, SM_NRETURN_F);
        return sm_emit(p, SM_NRETURN);
    }

    int idx = sm_emit_i(p, op, 0);
    int resolved = labtab_find(labtab, target);
    if (resolved >= 0)
        sm_patch_jump(p, idx, resolved);
    else
        labtab_patch_later(labtab, idx, target);
    return idx;
}
