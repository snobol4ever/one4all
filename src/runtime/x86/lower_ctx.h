/*
 * lower_ctx.h — Lowering context for lower.c (SR-1)
 *
 * Threads the state previously carried by two file-scope globals
 * (g_expression_body_lowering, g_expression_scope) plus the explicit
 * (SM_Program *p, LabelTable *labtab) parameter pair through a single
 * pointer argument.  Every dataflow becomes visible at the call site.
 *
 * SR-1 is structural only: the LabelTable types still live here in
 * their original shape and are still allocated/freed with malloc/free.
 * SR-2 will move the labtab_* family to lower_ctx.c and migrate to GC.
 *
 * Authors: Lon Jones Cherryholmes · Claude Sonnet 4.6
 * Date: 2026-05-11
 */

#ifndef LOWER_CTX_H
#define LOWER_CTX_H

#include "sm_prog.h"
#include "../../frontend/snobol4/scrip_cc.h"      /* CODE_t */
#include "../ast/ast.h"                            /* AST_t */
#include "../../runtime/interp/coro_runtime.h"    /* IcnScope */

/* ── Label resolution table ─────────────────────────────────────────────
 *
 * SR-1: types kept verbatim from lower.c.  SR-2 moves labtab_*
 * implementations to lower_ctx.c and migrates to GC allocation.
 */

typedef struct {
    char *name;         /* SNOBOL4 label string (interned) */
    int   instr_idx;    /* SM_Program instruction index of the SM_LABEL instr */
} LabelEntry;

typedef struct {
    /* Forward-reference patches: a goto whose target isn't defined yet */
    int   jump_instr_idx;   /* index of the SM_JUMP / SM_JUMP_S / SM_JUMP_F */
    char *target_name;      /* label name to resolve */
} PatchEntry;

typedef struct {
    LabelEntry *labels;
    int         nlabels;
    int         labels_cap;

    PatchEntry *patches;
    int         npatches;
    int         patches_cap;
} LabelTable;

/* ── Lowering context ────────────────────────────────────────────────────
 *
 * One LowerCtx is created per lower() invocation.  All lowering
 * functions (lower_expr, lower_stmt, lower_pat_expr, emit helpers)
 * receive `LowerCtx *c` and reach the SM_Program via `c->p` and the
 * label table via `c->labtab`.
 *
 * Expression-body lowering state:
 *   When true, lower_expr is walking an Icon/Raku proc body emitted
 *   into the SM_Program as a forward-jumped expression (CH-17b/b').
 *   The `expression_scope` is the per-proc IcnScope built fresh for
 *   that proc; AST_VAR / AST_ASSIGN consult it to emit
 *   SM_LOAD_FRAME / SM_STORE_FRAME for in-scope names.
 *
 *   Outside the per-proc loop in lower(), expression_body_lowering
 *   is 0 and expression_scope is NULL — the statement-context lowering
 *   path is unaffected.
 */
/* SR-14: bitset tracking which AST kinds hit lower_unhandled() during lowering.
 * Sized for AST_KIND_COUNT bits (currently ≤128; two uint64_t suffice).
 * Post-lowering, lower() reports any set bits by name. */
#define LOWER_UNHANDLED_WORDS 4   /* 4×64 = 256 bits — headroom for future kinds */

typedef struct {
    SM_Program  *p;                          /* output program (owned) */
    LabelTable   labtab;                         /* label resolution + patches */
    int          expression_body_lowering;   /* CH-17b': set during proc-body emit */
    IcnScope    *expression_scope;           /* CH-17b'': active per-proc scope */
    unsigned long long unhandled_kinds[LOWER_UNHANDLED_WORDS]; /* SR-14: bitset */
} LowerCtx;

/* ── Lowering primitives available to cohort files ──────────────────────
 *
 * The LOWER* macros are the workhorse of the per-kind handlers.  They
 * lower one or two children and emit a single SM opcode.  Each macro
 * assumes the caller has `LowerCtx *c`, `SM_Program *p` (typically
 * `c->p`), and `const AST_t *e` in scope — the same shape as
 * `lower_expr` itself.  Phase-2 cohort files will mirror that prelude.
 *
 * `lower_expr` and `lower_pat_expr` themselves are declared in
 * `lower.c` (still `static` until Phase-2 splits cohort files out).
 * They are visible to the macros via the include order in `lower.c`;
 * a cohort TU (Phase-2+) will see them via cohort header glue.
 *
 * `emit_push_expr` is the SM_PUSH_EXPR helper from RS-9b: every emission
 * GC-clones the AST_t so the SM_Program owns its descendants
 * independently of the calloc-based IR tree.  Inlined here so cohorts
 * can use it without a translation-unit dependency on lower.c.
 */

/* ── LabelTable API (implementation in lower_ctx.c) ─────────────────────
 *
 * SR-2: these functions moved from lower.c to lower_ctx.c and migrated
 * to GC allocation.  labtab_free() is a no-op shim; the GC reclaims
 * storage automatically.
 */
void labtab_init       (LabelTable *labtab);
void labtab_free       (LabelTable *labtab);   /* no-op; GC handles it */
void labtab_define     (LabelTable *labtab, const char *name, int instr_idx);
int  labtab_find       (const LabelTable *labtab, const char *name);
void labtab_patch_later(LabelTable *labtab, int jump_instr_idx, const char *name);
int  labtab_resolve    (LabelTable *labtab, SM_Program *p);

/* ── SR-3: helpers moved from lower.c ───────────────────────────────── */

/* Return a GC-allocated uppercase copy of `raw`.  No length cap. */
char *kw_canonicalize(const char *raw);

/* Walk a proc body AST, adding non-global variable names to `sc`. */
void expression_scope_walk(IcnScope *sc, AST_t *e);

/* Emit a SM_JUMP/SM_JUMP_S/SM_JUMP_F for a named SNOBOL4 goto target. */
int emit_goto(LowerCtx *c, sm_opcode_t op, const char *target);

/* SR-4: LowerHandler typedef — handler function pointer type.
 * Defined here so lower_ctx.h is self-contained for any future split. */
typedef void (*LowerHandler)(LowerCtx *c, const AST_t *e);

/* lower_expr and lower_pat_expr: non-static since SR-5.
 * Cohort files that recursed (AST_INDIRECT, AST_DEFER, etc.) called lower_expr
 * directly.  Now that all cohorts are merged into lower.c, these are still
 * declared here for use by lower_pat_expr (which has a switch that calls
 * lower_expr) and lower_stmt. */
void lower_expr    (LowerCtx *c, const AST_t *e);
void lower_pat_expr(LowerCtx *c, const AST_t *e);
void lower_stmt    (LowerCtx *c, const STMT_t *s);  /* lower_stmt.c (SR-12) */

/* sm_pat_capture_fn_arg_names: used by lower_pat_expr inside lower.c. */
const char *sm_pat_capture_fn_arg_names(const AST_t *fnc);

/* SR-14: explicit fallback for kinds not yet implemented. */
void lower_unhandled(LowerCtx *c, const AST_t *e);

#include "../common/ast_clone.h"   /* ast_gc_clone — used by emit_push_expr */

static inline void emit_push_expr(LowerCtx *c, const AST_t *e)
{
    sm_emit_ptr(c->p, SM_PUSH_EXPR, (void *)ast_gc_clone(e));
}

#define CH0(t) ((t)->nchildren > 0 ? (t)->children[0] : NULL)
#define CH1(t) ((t)->nchildren > 1 ? (t)->children[1] : NULL)

#define LOWER2(op)     do { lower_expr(c,CH0(t));     lower_expr(c,CH1(t));     sm_emit(p,(op)); return; } while(0)
#define LOWER1_VAL(op) do { lower_expr(c,CH0(t));                               sm_emit(p,(op)); return; } while(0)
#define LOWER1_PAT(op) do { lower_pat_expr(c,CH0(t));                           sm_emit(p,(op)); return; } while(0)

#endif /* LOWER_CTX_H */
