/*
 * lower_icn_data.c — Data-constructor and declaration handlers (SR-10)
 *
 * AST kinds handled:
 *   AST_MAKELIST  [e1, e2, ...]  list constructor
 *   AST_RECORD    record declaration (type constructor)
 *   AST_FIELD     E.name  field access
 *   AST_GLOBAL    global varname  (declaration; no runtime emission needed)
 *   AST_INITIAL   initial { body }  once-on-first-call block
 *
 * AST_INITIAL uses a per-AST NV sentinel variable (__initial_<ptr>__)
 * to implement the once-flag.  See the detailed comment in lower.c
 * from which this handler was extracted.
 *
 * Authors: Lon Jones Cherryholmes · Claude Sonnet 4.6
 */

#include "lower_ctx.h"
#include <stdio.h>

/* ── AST_MAKELIST ────────────────────────────────────────────── */

static void lower_makelist(LowerCtx *c, const AST_t *e)
{
    SM_Program *p = c->p;
    for (int i = 0; i < e->nchildren; i++) lower_expr(c, e->children[i]);
    sm_emit_si(p, SM_CALL_FN, "MAKELIST", (int64_t)e->nchildren);
}

/* ── AST_RECORD ──────────────────────────────────────────────── */

static void lower_record(LowerCtx *c, const AST_t *e)
{
    SM_Program *p = c->p;
    sm_emit_s(p, SM_PUSH_LIT_S, e->sval ? e->sval : "");
    for (int i = 0; i < e->nchildren; i++) lower_expr(c, e->children[i]);
    sm_emit_si(p, SM_CALL_FN, "RECORD_MAKE", (int64_t)e->nchildren + 1);
}

/* ── AST_FIELD ───────────────────────────────────────────────── */

static void lower_field(LowerCtx *c, const AST_t *e)
{
    SM_Program *p = c->p;
    lower_expr(c, e->nchildren > 0 ? e->children[0] : NULL);
    sm_emit_s(p, SM_PUSH_LIT_S, e->sval ? e->sval : "");
    sm_emit_si(p, SM_CALL_FN, "FIELD_GET", 2);
}

/* ── AST_GLOBAL ──────────────────────────────────────────────── */

static void lower_global(LowerCtx *c, const AST_t *e)
{
    SM_Program *p = c->p;
    (void)e;
    sm_emit(p, SM_PUSH_NULL);
}

/* ── AST_INITIAL ─────────────────────────────────────────────── */
/*
 * Icon `initial { ... }` runs its body the FIRST time the enclosing
 * procedure is called, then skips on every subsequent call.  In SM
 * mode the once-flag persists across calls as a per-AST NV sentinel.
 *
 * Shape:
 *   PUSH_VAR __initial_<ptr>__   ; null on first call
 *   CALL_FN  NONNULL 1           ; FAIL if null, succeed if set
 *   JUMP_S   L_skip              ; sentinel set → skip body
 *   VOID_POP                     ; drop FAILDESCR from NONNULL
 *   [body assignments]
 *   PUSH_LIT_I 1
 *   STORE_VAR __initial_<ptr>__  ; sentinel := 1
 *   VOID_POP
 *   JUMP     L_done
 * L_skip:
 *   VOID_POP                     ; drop sentinel value
 * L_done:
 *   PUSH_NULL
 */
static void lower_initial(LowerCtx *c, const AST_t *e)
{
    SM_Program *p = c->p;
    char sentinel[64];
    snprintf(sentinel, sizeof(sentinel), "__initial_%lx__",
             (unsigned long)(uintptr_t)e);

    sm_emit_s(p, SM_PUSH_VAR, sentinel);
    sm_emit_si(p, SM_CALL_FN, "NONNULL", 1);
    int skip_jump = sm_emit_i(p, SM_JUMP_S, 0);

    sm_emit(p, SM_VOID_POP);

    for (int i = 0; i < e->nchildren; i++) {
        if (!e->children[i]) continue;
        lower_expr(c, e->children[i]);
        sm_emit(p, SM_VOID_POP);
    }

    sm_emit_i(p, SM_PUSH_LIT_I, 1);
    sm_emit_s(p, SM_STORE_VAR, sentinel);
    sm_emit(p, SM_VOID_POP);

    int done_jump = sm_emit_i(p, SM_JUMP, 0);

    int skip_pc = sm_label(p);
    sm_patch_jump(p, skip_jump, skip_pc);
    sm_emit(p, SM_VOID_POP);

    int done_pc = sm_label(p);
    sm_patch_jump(p, done_jump, done_pc);

    sm_emit(p, SM_PUSH_NULL);
}

/* ── Registration ─────────────────────────────────────────────── */

void lower_icn_data_register(LowerHandler tbl[AST_KIND_COUNT])
{
    tbl[AST_MAKELIST] = lower_makelist;
    tbl[AST_RECORD]   = lower_record;
    tbl[AST_FIELD]    = lower_field;
    tbl[AST_GLOBAL]   = lower_global;
    tbl[AST_INITIAL]  = lower_initial;
}
