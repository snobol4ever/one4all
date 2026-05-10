/*
 * sm_lower.c — IR → SM_Program compiler pass (M-SCRIP-U3)
 *
 * Walks a CODE_t* (linked list of STMT_t, each holding AST_t trees)
 * and emits a flat SM_Program instruction sequence.
 *
 * SNOBOL4 statement model:
 *   label:  subject  pattern = replacement  :(goto)
 *
 * SM lowering strategy per statement:
 *   1. Emit SM_LABEL for stmt->label (if present) → recorded in label_table
 *   2. Eval subject   → value on stack
 *   3. If pattern:    → emit SM_PAT_* tree; emit SM_EXEC_STMT
 *      Else if replacement only: emit subject eval + SM_STORE_VAR
 *   4. Gotos: SM_JUMP_S / SM_JUMP_F / SM_JUMP (patched after all stmts)
 *
 * Authors: Lon Jones Cherryholmes · Claude Sonnet 4.6
 * Date: 2026-04-06
 */

#include "sm_lower.h"
#include "sm_prog.h"
#include "sm_interp.h"   /* CH-17i-every-suspend: every_table_register */

#include "../../frontend/snobol4/scrip_cc.h"
#include "../ast/ast.h"
#include "../../runtime/common/ast_clone.h"   /* RS-9b: ast_gc_clone */
#include "../../runtime/interp/coro_runtime.h"  /* CH-17b: proc_table for expression skeletons */
#include "../../runtime/interp/pl_runtime.h"    /* CH-17d: g_pl_pred_table for pred-expression skeletons */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>
#include <ctype.h>
#include <gc/gc.h>
#include "snobol4.h"   /* FNCEX_fn, FUNC_NPARAMS_fn */

/* RS-9b: emit SM_PUSH_EXPR with a GC-cloned AST_t* so the SM_Program
 * owns its AST_t* values independently of the calloc-based IR tree.
 * All SM_PUSH_EXPR emissions go through this helper. */
static void emit_push_expr(SM_Program *p, const AST_t *e)
{
    sm_emit_ptr(p, SM_PUSH_EXPR, (void *)ast_gc_clone(e));
}

/* CH-17b': suppress lower_expr's "unhandled expr kind" stderr warning while
 * lowering proc-body expressions.  These expressions are dead code today (forward-jumped
 * over; coro_call still walks IR for real execution), so any unhandled-kind
 * fall-through landing here is harmless — the warning would only mislead.  The
 * flag is set/cleared around the per-proc emission loop in sm_lower; outside
 * that scope (i.e., when lower_expr is invoked from lower_stmt for genuinely
 * executable code), the warning fires as before.  CH-17c will flip coro_call
 * to dispatch via entry_pc, at which point the unhandled kinds in proc bodies
 * become reachable and will need real lowering — that is later-rung territory
 * (CH-17h reactivates CH-15b for the Icon generator kinds; the cset and
 * AST_ALTERNATE / AST_ITERATE / AST_REVASSIGN / AST_REVSWAP gaps will be filled by
 * the same wave). */
static int g_expression_body_lowering = 0;

/* CHUNKS-step17b'' (CH-17b''): per-proc IcnScope active during expression-body
 * lowering.  Mirrors what coro_call's icn_scope_patch builds at runtime — but
 * built at lower-time and consulted (read-only) by the AST_VAR / AST_ASSIGN cases
 * in lower_expr so expressions emit SM_LOAD_FRAME slot / SM_STORE_FRAME slot for
 * params + locals, leaving SM_PUSH_VAR / SM_STORE_VAR for true globals,
 * builtins, and other-proc references.
 *
 * Built and torn down in the per-proc emission loop below; NULL outside that
 * loop.  AST_VAR / AST_ASSIGN consult it gated on g_expression_body_lowering so the
 * stmt-level lowering (which still walks IR via coro_call) is unaffected. */
static IcnScope *g_expression_scope = NULL;

/* Read-only mirror of coro_runtime.c's icn_scope_patch — walks the proc body's
 * AST_t tree and grows `sc` with non-global AST_VAR + AST_GLOBAL-decl names.  Does
 * NOT mutate AST_t.ival in place (the IR walker's slot indices were already
 * written by coro_call earlier when the proc was first invoked, OR will be
 * written next time it runs the IR path; both are safe because slot numbers
 * are deterministic — same scope-add order yields same slots).  This walker
 * gives us the same scope without committing to mutate IR at this time. */
static void expression_scope_walk(IcnScope *sc, AST_t *e) {
    if (!e) return;
    if (e->kind == AST_GLOBAL) {
        for (int i = 0; i < e->nchildren; i++)
            if (e->children[i] && e->children[i]->sval)
                scope_add(sc, e->children[i]->sval);
        return;
    }
    if (e->kind == AST_VAR && e->sval) {
        /* Mirror icn_scope_patch: globals (registered via global_register at
         * polyglot_init time) bridge to the SNO NV store — they DON'T get a
         * frame slot.  Non-global, non-keyword vars become slots in entry
         * order (params first, then AST_GLOBAL-decl names, then any encountered
         * here).  Names starting with '&' are keywords — leave them alone
         * (lowered as SM_PUSH_VAR with verbatim sval). */
        if (e->sval[0] != '&' && !is_global(e->sval))
            scope_add(sc, e->sval);
    }
    for (int i = 0; i < e->nchildren; i++)
        expression_scope_walk(sc, e->children[i]);
}

/* ── Label resolution table ─────────────────────────────────────────────── */

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

#define LABEL_TABLE_INIT 64

static void lt_init(LabelTable *lt)
{
    lt->labels      = malloc(LABEL_TABLE_INIT * sizeof(LabelEntry));
    lt->labels_cap  = LABEL_TABLE_INIT;
    lt->nlabels     = 0;
    lt->patches     = malloc(LABEL_TABLE_INIT * sizeof(PatchEntry));
    lt->patches_cap = LABEL_TABLE_INIT;
    lt->npatches    = 0;
}

/* Record a defined label → its SM_LABEL instruction index */
static void lt_define(LabelTable *lt, const char *name, int instr_idx)
{
    if (lt->nlabels >= lt->labels_cap) {
        lt->labels_cap *= 2;
        lt->labels = realloc(lt->labels, lt->labels_cap * sizeof(LabelEntry));
        if (!lt->labels) { fprintf(stderr, "sm_lower: label table OOM\n"); abort(); }
    }
    lt->labels[lt->nlabels].name      = strdup(name);
    lt->labels[lt->nlabels].instr_idx = instr_idx;
    lt->nlabels++;
}

/* Find a label by name; returns instr_idx or -1 */
static int lt_find(const LabelTable *lt, const char *name)
{
    /* SN-26c-stmt637: case-SENSITIVE label compare per SN-31 (case-sensitive
     * default).  strcasecmp here collided distinct labels like `visitEnd` and
     * `VisitEnd` (both present in beauty.sno via the double-function trick),
     * sending SM gotos to the wrong target.  IR's goto resolution
     * (interp.c lookup_label_stmt) is case-sensitive — this matches that. */
    for (int i = 0; i < lt->nlabels; i++)
        if (strcmp(lt->labels[i].name, name) == 0)
            return lt->labels[i].instr_idx;
    return -1;
}

/* Record a forward-reference patch */
static void lt_patch_later(LabelTable *lt, int jump_instr_idx, const char *name)
{
    if (lt->npatches >= lt->patches_cap) {
        lt->patches_cap *= 2;
        lt->patches = realloc(lt->patches, lt->patches_cap * sizeof(PatchEntry));
        if (!lt->patches) { fprintf(stderr, "sm_lower: patch table OOM\n"); abort(); }
    }
    lt->patches[lt->npatches].jump_instr_idx = jump_instr_idx;
    lt->patches[lt->npatches].target_name    = strdup(name);
    lt->npatches++;
}

/* Resolve all forward patches; returns 0 on success, -1 on unresolved ref */
static int lt_resolve(LabelTable *lt, SM_Program *p)
{
    int ok = 0;
    for (int i = 0; i < lt->npatches; i++) {
        const char *name = lt->patches[i].target_name;
        int target = lt_find(lt, name);
        if (target < 0) {
            /* SNOBOL4 convention: goto an undefined label = Error 24.
             * Patch to last instruction (SM_HALT) so execution terminates
             * cleanly rather than jumping to pc=0 and re-running the program. */
            fprintf(stderr, "sm_lower: undefined label '%s' treated as Error 24 (halt)\n", name);
            target = (p->count > 0) ? p->count - 1 : 0;
            ok = -1;
        }
        sm_patch_jump(p, lt->patches[i].jump_instr_idx, target);
    }
    return ok;
}

static void lt_free(LabelTable *lt)
{
    for (int i = 0; i < lt->nlabels; i++)  free(lt->labels[i].name);
    for (int i = 0; i < lt->npatches; i++) free(lt->patches[i].target_name);
    free(lt->labels);  lt->labels  = NULL;
    free(lt->patches); lt->patches = NULL;
}

/* ── Emit a goto target (possibly forward ref) ──────────────────────────── */

/*
 * Emit a SM_JUMP / SM_JUMP_S / SM_JUMP_F for a named SNOBOL4 goto target.
 * If the target is already defined, patch immediately.
 * Otherwise register a forward patch.
 * Special target "RETURN" → SM_RETURN; "FRETURN" → SM_FRETURN.
 * Returns the index of the emitted jump instruction.
 */
static int emit_goto(SM_Program *p, LabelTable *lt,
                     sm_opcode_t op, const char *target)
{
    if (!target) return -1;

    /* Special names (case-insensitive per SNOBOL4 spec).
     * The conditionality (SM_JUMP vs SM_JUMP_S vs SM_JUMP_F) is preserved
     * by mapping to the appropriate conditional return opcode. */
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

    /* Emit the jump with a placeholder target (0) */
    int idx = sm_emit_i(p, op, 0);

    int resolved = lt_find(lt, target);
    if (resolved >= 0) {
        sm_patch_jump(p, idx, resolved);
    } else {
        lt_patch_later(lt, idx, target);
    }
    return idx;
}

/* ── Expression lowering ────────────────────────────────────────────────── */

/* Forward declaration */
static void lower_expr(SM_Program *p, LabelTable *lt, const AST_t *e);

/* F-2 RS-7: boilerplate macros for the two most common lowering patterns.
 * LOWER2(op)     — lower both children then emit binary op (arith, etc.)
 * LOWER1_PAT(op) — lower child[0] then emit single-arg pattern op */
#define CH0(e) ((e)->nchildren > 0 ? (e)->children[0] : NULL)
#define CH1(e) ((e)->nchildren > 1 ? (e)->children[1] : NULL)
#define LOWER2(op)     do { lower_expr(p,lt,CH0(e)); lower_expr(p,lt,CH1(e)); sm_emit(p,(op)); return; } while(0)
#define LOWER1_VAL(op) do { lower_expr(p,lt,CH0(e)); sm_emit(p,(op)); return; } while(0)
#define LOWER1_PAT(op) do { lower_pat_expr(p,lt,CH0(e)); sm_emit(p,(op)); return; } while(0)

/* TL-2: extract arg *names* from a *fn(var,var,...) AST_FNC subtree so
 * SM_PAT_CAPTURE_FN can carry them in a[2].s for flush-time resolution.
 *
 * Returns:
 *   NULL  — if the AST_FNC has no children, any child is not AST_VAR with a
 *           non-NULL sval, or allocation failed.  Callers treat NULL as
 *           "fall back to legacy eager-eval path" (pat_assign_callcap with
 *           NULL args).
 *   non-NULL — a GC_strdup-lifetime '\t'-separated list of variable names.
 *
 * '\t' is safe because SNOBOL4 identifiers match [A-Za-z_$.][...], never
 * containing whitespace.  Callers split on '\t' to recover individual names. */
static const char *sm_pat_capture_fn_arg_names(const AST_t *fnc)
{
    if (!fnc || fnc->nchildren <= 0) return NULL;
    size_t total_len = 0;
    for (int i = 0; i < fnc->nchildren; i++) {
        const AST_t *c = fnc->children[i];
        if (!c || c->kind != AST_VAR || !c->sval) return NULL;
        total_len += strlen(c->sval) + 1;  /* name + separator/terminator */
    }
    char *buf = (char *)GC_MALLOC(total_len);
    if (!buf) return NULL;
    char *p = buf;
    for (int i = 0; i < fnc->nchildren; i++) {
        const char *name = fnc->children[i]->sval;
        size_t n = strlen(name);
        memcpy(p, name, n);
        p += n;
        *p++ = (i + 1 < fnc->nchildren) ? '\t' : '\0';
    }
    return buf;
}

static void lower_pat_expr(SM_Program *p, LabelTable *lt, const AST_t *e)
{
    if (!e) return;

    switch (e->kind) {

    /* Literals in pattern context → SM_PAT_LIT */
    case AST_QLIT:
        sm_emit_s(p, SM_PAT_LIT, e->sval ? e->sval : "");
        return;

    /* Variable → dereference, then pattern */
    case AST_VAR:
        sm_emit_s(p, SM_PUSH_VAR, e->sval);
        sm_emit(p, SM_PAT_DEREF);
        return;

    /* Primitives */
    case AST_ARB:      sm_emit(p, SM_PAT_ARB);     return;
    case AST_REM:      sm_emit(p, SM_PAT_REM);      return;
    case AST_FAIL:     sm_emit(p, SM_PAT_FAIL);     return;
    case AST_SUCCEED:  sm_emit(p, SM_PAT_SUCCEED);  return;
    case AST_FENCE:
        if (e->nchildren > 0) {
            lower_pat_expr(p, lt, e->children[0]);
            sm_emit(p, SM_PAT_FENCE1);
        } else {
            sm_emit(p, SM_PAT_FENCE);
        }
        return;
    case AST_ABORT:    sm_emit(p, SM_PAT_ABORT);    return;
    case AST_BAL:      sm_emit(p, SM_PAT_BAL);      return;

    /* Parameterised primitives — child[0] is the argument expr */
    case AST_ANY:    lower_expr(p, lt, CH0(e)); sm_emit(p, SM_PAT_ANY);    return;
    case AST_NOTANY: lower_expr(p, lt, CH0(e)); sm_emit(p, SM_PAT_NOTANY); return;
    case AST_SPAN:   lower_expr(p, lt, CH0(e)); sm_emit(p, SM_PAT_SPAN);   return;
    case AST_BREAK:  lower_expr(p, lt, CH0(e)); sm_emit(p, SM_PAT_BREAK);  return;
    case AST_BREAKX: lower_expr(p, lt, CH0(e)); sm_emit(p, SM_PAT_BREAK);  return; /* BREAKX → BREAK */
    case AST_LEN:    lower_expr(p, lt, CH0(e)); sm_emit(p, SM_PAT_LEN);    return;
    case AST_POS:    lower_expr(p, lt, CH0(e)); sm_emit(p, SM_PAT_POS);    return;
    case AST_RPOS:   lower_expr(p, lt, CH0(e)); sm_emit(p, SM_PAT_RPOS);   return;
    case AST_TAB:    lower_expr(p, lt, CH0(e)); sm_emit(p, SM_PAT_TAB);    return;
    case AST_RTAB:   lower_expr(p, lt, CH0(e)); sm_emit(p, SM_PAT_RTAB);   return;
    case AST_ARBNO:  LOWER1_PAT(SM_PAT_ARBNO);

    /* Concatenation (sequence in pattern) → left then right, then SM_PAT_CAT */
    case AST_SEQ:
    case AST_CAT:
        for (int i = 0; i < e->nchildren; i++)
            lower_pat_expr(p, lt, e->children[i]);
        /* n-ary: emit n-1 SM_PAT_CAT */
        for (int i = 1; i < e->nchildren; i++)
            sm_emit(p, SM_PAT_CAT);
        return;

    /* Alternation */
    case AST_ALT:
        for (int i = 0; i < e->nchildren; i++)
            lower_pat_expr(p, lt, e->children[i]);
        for (int i = 1; i < e->nchildren; i++)
            sm_emit(p, SM_PAT_ALT);
        return;

    /* Captures */
    case AST_CAPT_COND_ASGN:
        /* child[0] = sub-pattern, child[1] = variable; a[1].i=0 → cond (.V) */
        lower_pat_expr(p, lt, e->nchildren > 0 ? e->children[0] : NULL);
        if (e->nchildren > 1 && e->children[1]) {
            AST_t *var_expr = e->children[1];
            /* Detect . *func() — AST_DEFER(AST_FNC) — emit SM_PAT_CAPTURE_FN */
            if (var_expr->kind == AST_DEFER
                    && var_expr->nchildren > 0
                    && var_expr->children[0]
                    && var_expr->children[0]->kind == AST_FNC
                    && var_expr->children[0]->sval) {
                const AST_t *fnc = var_expr->children[0];
                const char *namelist = sm_pat_capture_fn_arg_names(fnc);
                if (namelist || fnc->nchildren == 0) {
                    /* TL-2 name-stash path (all args are plain AST_VAR, or no args). */
                    int idx = sm_emit_s(p, SM_PAT_CAPTURE_FN, fnc->sval);
                    p->instrs[idx].a[1].i = 0;  /* conditional */
                    p->instrs[idx].a[2].s = namelist;
                } else {
                    /* SN-8a: args-on-stack path — eager-eval each arg, then
                     * SM_PAT_CAPTURE_FN_ARGS pops them and calls pat_assign_callcap.
                     * SN-26c-parseerr-c: defer AST_FNC sub-args as compiled SM expressions.
                     * SN-26c-parseerr-d: also defer AST_VAR — when args are mixed
                     * (e.g. literal+var) the all_vars name-stash fast path
                     * doesn't fire, and an AST_VAR set by an earlier capture in
                     * the same pattern would be eagerly read at build time
                     * instead of at match time.
                     * SN-32b (SM/JIT parity with IR-side -t fix): defer all
                     * non-AST_QLIT args.  Compound expressions like `nTop()+1`
                     * (AST_ADD wrapping AST_FNC) eagerly invoked via lower_expr
                     * cause the inner function to fire at pattern-build time,
                     * not at match time — same shape as SN-26-bridge-coverage-t
                     * but on the SM lowering side.  Thaw at match time via
                     * EVAL_fn → EXPVAL_fn (name_t.c:97) handles all AST_t
                     * shapes including compound.
                     * CHUNKS-step04: non-AST_QLIT args are now lowered as compiled
                     * SM expressions (SM_JUMP skip + body + SM_RETURN + SM_PUSH_EXPRESSION)
                     * so DT_E carries entry_pc, not AST_t*.  At match time
                     * EVAL_fn → EXPVAL_fn dispatches the expression via sm_call_expression
                     * (slen==1 path).  Same emission shape as Steps 2/3. */
                    for (int i = 0; i < fnc->nchildren; i++) {
                        AST_t *arg = fnc->children[i];
                        if (arg && arg->kind == AST_QLIT)
                            lower_expr(p, lt, arg);   /* string lit — eager OK */
                        else if (arg) {
                            int skip_jump = sm_emit_i(p, SM_JUMP, 0);
                            int entry_pc  = sm_label(p);
                            lower_expr(p, lt, arg);
                            sm_emit(p, SM_RETURN);
                            int skip_lbl  = sm_label(p);
                            sm_patch_jump(p, skip_jump, skip_lbl);
                            sm_emit_ii(p, SM_PUSH_EXPRESSION, (int64_t)entry_pc, 0);
                        } else
                            lower_expr(p, lt, arg);
                    }
                    int idx = sm_emit_s(p, SM_PAT_CAPTURE_FN_ARGS, fnc->sval);
                    p->instrs[idx].a[1].i = 0;              /* conditional */
                    p->instrs[idx].a[2].i = fnc->nchildren; /* nargs */
                }
            } else {
                int idx = sm_emit_s(p, SM_PAT_CAPTURE, var_expr->sval);
                p->instrs[idx].a[1].i = 0;  /* conditional */
            }
        }
        return;
    case AST_CAPT_IMMED_ASGN:
        /* a[1].i=1 → immediate ($V) */
        lower_pat_expr(p, lt, e->nchildren > 0 ? e->children[0] : NULL);
        if (e->nchildren > 1 && e->children[1]) {
            AST_t *var_expr = e->children[1];
            /* Detect $ *func() — AST_DEFER(AST_FNC) — emit SM_PAT_CAPTURE_FN */
            if (var_expr->kind == AST_DEFER
                    && var_expr->nchildren > 0
                    && var_expr->children[0]
                    && var_expr->children[0]->kind == AST_FNC
                    && var_expr->children[0]->sval) {
                const AST_t *fnc = var_expr->children[0];
                const char *namelist = sm_pat_capture_fn_arg_names(fnc);
                if (namelist || fnc->nchildren == 0) {
                    int idx = sm_emit_s(p, SM_PAT_CAPTURE_FN, fnc->sval);
                    p->instrs[idx].a[1].i = 1;  /* immediate */
                    p->instrs[idx].a[2].s = namelist;
                } else {
                    /* SN-8a: args-on-stack path for $ *fn(args).
                     * SN-26c-parseerr-c: defer AST_FNC sub-args as compiled SM expressions.
                     * SN-26c-parseerr-d: also defer AST_VAR (see twin site).
                     * SN-32b: defer all non-AST_QLIT args (mirrors -t fix on IR side).
                     * CHUNKS-step04: same expression emission pattern as the . *fn site above
                     * and Steps 2/3.  DT_E now carries entry_pc, not AST_t*. */
                    for (int i = 0; i < fnc->nchildren; i++) {
                        AST_t *arg = fnc->children[i];
                        if (arg && arg->kind == AST_QLIT)
                            lower_expr(p, lt, arg);
                        else if (arg) {
                            int skip_jump = sm_emit_i(p, SM_JUMP, 0);
                            int entry_pc  = sm_label(p);
                            lower_expr(p, lt, arg);
                            sm_emit(p, SM_RETURN);
                            int skip_lbl  = sm_label(p);
                            sm_patch_jump(p, skip_jump, skip_lbl);
                            sm_emit_ii(p, SM_PUSH_EXPRESSION, (int64_t)entry_pc, 0);
                        } else
                            lower_expr(p, lt, arg);
                    }
                    int idx = sm_emit_s(p, SM_PAT_CAPTURE_FN_ARGS, fnc->sval);
                    p->instrs[idx].a[1].i = 1;              /* immediate */
                    p->instrs[idx].a[2].i = fnc->nchildren; /* nargs */
                }
            } else {
                int idx = sm_emit_s(p, SM_PAT_CAPTURE, var_expr->sval);
                p->instrs[idx].a[1].i = 1;  /* immediate */
            }
        }
        return;
    case AST_CAPT_CURSOR:
        /* Two forms from the parser:
         *   unary @var  → nchildren=1, children[0] = var-name node (ATFN)
         *   binary X@V  → nchildren=2, children[0] = sub-pat, children[1] = var-name
         * For unary @var there is no sub-pattern — emit epsilon implicitly
         * (pat_pop in SM_PAT_CAPTURE will get pat_epsilon via pat_cat). */
        if (e->nchildren == 1) {
            /* unary @var: no sub-pattern child — child[0] IS the variable name */
            const char *vname = (e->children[0] && e->children[0]->sval)
                                 ? e->children[0]->sval : "";
            sm_emit(p, SM_PAT_EPS);          /* push epsilon as sub-pattern */
            int idx = sm_emit_s(p, SM_PAT_CAPTURE, vname);
            p->instrs[idx].a[1].i = 2;      /* cursor */
        } else {
            lower_pat_expr(p, lt, e->nchildren > 0 ? e->children[0] : NULL);
            if (e->nchildren > 1 && e->children[1]) {
                int idx = sm_emit_s(p, SM_PAT_CAPTURE, e->children[1]->sval);
                p->instrs[idx].a[1].i = 2;  /* cursor (@V) */
            }
        }
        return;

    /* Deferred pattern reference: *VAR   or   bare *FN() in pattern context */
    case AST_DEFER: {
        AST_t *ch = e->nchildren > 0 ? e->children[0] : NULL;
        /* SN-17a: bare *fn() in a pattern — emit SM_PAT_USERCALL so the engine
         * invokes fn at match time and the call's FAIL propagates as pattern FAIL.
         * Without this, AST_FNC falls through to lower_expr → SM_CALL_FN → SM_PAT_DEREF
         * which evaluates fn ONCE at build time and treats the return as a pattern,
         * skipping the per-position sweep SPITBOL performs. */
        if (ch && ch->kind == AST_FNC && ch->sval) {
            if (ch->nchildren == 0) {
                /* Bare *fn() — no args.  Use the namelist-less SM_PAT_USERCALL
                 * since pat_user_call(fname, NULL, 0) is the correct call. */
                int idx = sm_emit_s(p, SM_PAT_USERCALL, ch->sval);
                p->instrs[idx].a[2].s = NULL;
            } else {
                /* SN-8a / SN-32b: args-on-stack path for bare *fn(args).
                 *
                 * IMPORTANT (SN-32b root-cause fix): the previous fast-path
                 * `if (namelist) SM_PAT_USERCALL` was BROKEN.  It packed the
                 * arg names into ins->a[2].s, but the SM_PAT_USERCALL handler
                 * in sm_interp.c calls `pat_user_call(fname, NULL, 0)`,
                 * silently discarding the args.  At match time bb_usercall
                 * invokes the user function with zero args — so `*upr(tx)`
                 * called upr() with no parameter, returning empty string,
                 * before the real *upr(tx) call from a higher-level pattern
                 * fired.  This was visible at step 2023 of the 2-way harness
                 * on `beauty.sno < beauty.sno`: scrip-SM emitted `CALL upr`
                 * inside upr's own body where SPITBOL emitted `VALUE upr=`.
                 *
                 * Fix: ALWAYS take the args-on-stack path when nchildren>0.
                 * Defer non-AST_QLIT args via SM_PUSH_EXPR so match-time
                 * EVAL_fn thaw resolves variables in their captured state.
                 * This mirrors the IR-side path at interp.c:4188-4208
                 * exactly (SN-26c-parseerr-c/-d, SN-32b parity).
                 *
                 * SN-26c-parseerr-c (Bug B): when an arg is itself a function
                 * call (AST_FNC), DEFER via SM_PUSH_EXPR so the inner call
                 * evaluates at match time, not at pattern-build time.
                 * SN-26c-parseerr-d: also defer AST_VAR.
                 * SN-32b: defer all non-AST_QLIT args (mirrors IR -t fix).
                 *
                 * The match-time path (bb_usercall in stmt_exec.c) thaws each
                 * DT_E via EVAL_fn before invoking the user function. */
                for (int i = 0; i < ch->nchildren; i++) {
                    AST_t *arg = ch->children[i];
                    if (arg && arg->kind == AST_QLIT)
                        lower_expr(p, lt, arg);
                    else if (arg) {
                        /* CHUNKS-step03: defer non-AST_QLIT pattern arg as a
                         * compiled SM expression so DT_E carries an entry_pc, not
                         * an AST_t*.  At match time bb_usercall thaws each
                         * DT_E via EVAL_fn → EXPVAL_fn, which dispatches the
                         * expression via sm_call_expression (slen==1 path).  Same
                         * emission shape as the AST_DEFER expression lowering. */
                        int skip_jump = sm_emit_i(p, SM_JUMP, 0);
                        int entry_pc  = sm_label(p);
                        lower_expr(p, lt, arg);
                        sm_emit(p, SM_RETURN);
                        int skip_lbl  = sm_label(p);
                        sm_patch_jump(p, skip_jump, skip_lbl);
                        sm_emit_ii(p, SM_PUSH_EXPRESSION, (int64_t)entry_pc, 0);
                    } else
                        lower_expr(p, lt, arg);
                }
                int idx = sm_emit_s(p, SM_PAT_USERCALL_ARGS, ch->sval);
                p->instrs[idx].a[1].i = ch->nchildren;  /* nargs */
            }
            return;
        }
        /* SN-6: *var — emit SM_PAT_REFNAME so the name (not the current value)
         * reaches bb_deferred_var at match time.  Self-recursive patterns like
         *   primary = integer | '(' *primary ')'
         * need the XDSAR ref built from NAME, not from PRIMARY's in-progress
         * value at pattern-build time.  Mirrors --ir-run's pat_ref(child->sval)
         * branch in interp_eval_pat AST_DEFER. */
        if (ch && ch->kind == AST_VAR && ch->sval) {
            sm_emit_s(p, SM_PAT_REFNAME, ch->sval);
            return;
        }
        lower_expr(p, lt, ch);
        sm_emit(p, SM_PAT_DEREF);
        return;
    }

    /* Function call in pattern context → eval as value, then deref as pat */
    case AST_FNC:
        lower_expr(p, lt, e);
        sm_emit(p, SM_PAT_DEREF);
        return;

    default:
        /* Value expression used as pattern — eval and deref */
        lower_expr(p, lt, e);
        sm_emit(p, SM_PAT_DEREF);
        return;
    }
}

static void lower_expr(SM_Program *p, LabelTable *lt, const AST_t *e)
{
    if (!e) {
        sm_emit(p, SM_PUSH_NULL);
        return;
    }

    switch (e->kind) {

    /* ── Literals ── */
    case AST_QLIT:
        sm_emit_s(p, SM_PUSH_LIT_S, e->sval ? e->sval : "");
        return;
    case AST_ILIT:
        sm_emit_i(p, SM_PUSH_LIT_I, (int64_t)e->ival);
        return;
    case AST_FLIT:
        sm_emit_f(p, SM_PUSH_LIT_F, e->dval);
        return;
    case AST_NULL:
    case AST_NUL:
        sm_emit(p, SM_PUSH_NULL);
        return;

    /* ── References ── */
    case AST_VAR: {
        const char *vn = e->sval ? e->sval : "";
        /* CHUNKS-step17b'' (CH-17b''): inside expression-body lowering, consult the
         * per-proc scope: if `vn` resolved to a frame slot, emit SM_LOAD_FRAME.
         * Globals, keywords ('&'-prefixed), and unscoped names fall through
         * to SM_PUSH_VAR — same shape as the legacy emission outside expressions. */
        if (g_expression_body_lowering && g_expression_scope && vn[0] && vn[0] != '&') {
            int slot = scope_get(g_expression_scope, vn);
            if (slot >= 0) {
                sm_emit_i(p, SM_LOAD_FRAME, slot);
                return;
            }
        }
        sm_emit_s(p, SM_PUSH_VAR, vn);
        return;
    }
    case AST_KEYWORD: {
        /* Keywords are registered uppercase (LCASE, UCASE, etc.) but the lexer
         * preserves the source case after stripping '&'. Uppercase before lookup. */
        const char *kraw = e->sval ? e->sval : "";
        char kup[64]; int ki = 0;
        while (kraw[ki] && ki < 63) { kup[ki] = (char)toupper((unsigned char)kraw[ki]); ki++; }
        kup[ki] = '\0';
        sm_emit_s(p, SM_PUSH_VAR, kup);
        return;
    }
    case AST_INDIRECT: {
        /* $expr — eval name-string, look up variable → push value on value stack.
         * Special case: $.var<idx> parses as AST_INDIRECT(AST_IDX(AST_NAME(AST_VAR("v")),idx...))
         * Lower as: push var value directly + IDX, bypassing INDIR_GET. */
        AST_t *ch = e->nchildren > 0 ? e->children[0] : NULL;
        /* $.var<idx> => AST_INDIRECT(AST_NAME(AST_IDX(AST_VAR("v"), idx...)))
         * Push var value directly, then indices, then call IDX. */
        if (ch && ch->kind == AST_NAME && ch->nchildren == 1) {
            AST_t *inner = ch->children[0];
            if (inner && inner->kind == AST_IDX && inner->nchildren >= 2
                    && inner->children[0] && inner->children[0]->kind == AST_VAR
                    && inner->children[0]->sval) {
                const char *vn = inner->children[0]->sval;
                sm_emit_s(p, SM_PUSH_VAR, vn);
                for (int i = 1; i < inner->nchildren; i++)
                    lower_expr(p, lt, inner->children[i]);
                sm_emit_si(p, SM_CALL_FN, "IDX", (int64_t)inner->nchildren);
                return;
            }
        }
        lower_expr(p, lt, ch);
        sm_emit_si(p, SM_CALL_FN, "INDIR_GET", 1);
        return;
    }
    case AST_DEFER: {
        /* CHUNKS-step02: *expr in value context — lower child as a compiled
         * SM expression so DT_E carries an entry_pc, not an AST_t*.
         *
         * Emission shape:
         *   SM_JUMP  skip_expression          ; jump around the expression body
         *   chunk_start: (entry_pc)
         *   <lower_expr(child)>          ; body: leaves result on stack
         *   SM_RETURN                    ; return to caller
         *   skip_expression: (label)
         *   SM_PUSH_EXPRESSION entry_pc, 0   ; push DT_E{slen=1, i=entry_pc}
         */
        const AST_t *child = e->nchildren > 0 ? e->children[0] : NULL;
        int skip_jump = sm_emit_i(p, SM_JUMP, 0);   /* forward jump, patched below */
        int entry_pc  = sm_label(p);                 /* expression entry point */
        if (child)
            lower_expr(p, lt, child);
        else
            sm_emit(p, SM_PUSH_NULL);
        sm_emit(p, SM_RETURN);
        int skip_lbl = sm_label(p);
        sm_patch_jump(p, skip_jump, skip_lbl);
        sm_emit_ii(p, SM_PUSH_EXPRESSION, (int64_t)entry_pc, 0);
        return;
    }

    /* ── Arithmetic ── */
    case AST_ADD: LOWER2(SM_ADD);
    case AST_SUB: LOWER2(SM_SUB);
    case AST_MUL: LOWER2(SM_MUL);
    case AST_DIV: LOWER2(SM_DIV);
    case AST_POW: LOWER2(SM_EXP);
    case AST_MOD: LOWER2(SM_MOD);
    case AST_MNS: LOWER1_VAL(SM_NEG);
    case AST_PLS: LOWER1_VAL(SM_COERCE_NUM);

    /* ── Goal-directed value-context disjunction ─────────────────────── */
    /* SPITBOL `(a, b, c)` paren-list and Snocone `||`.  Eager left-to-right
     * evaluation; first non-failing arm's value is the result; FAIL if all
     * arms fail.  Lower as: eval each arm, JUMP_S to done on success,
     * POP failure value before next arm.  Last arm's value (success or
     * fail) is left on stack if no earlier arm succeeded. */
    case AST_VLIST: {
        if (e->nchildren == 0) {
            sm_emit(p, SM_PUSH_NULL);
            return;
        }
        if (e->nchildren == 1) {
            lower_expr(p, lt, e->children[0]);
            return;
        }
        int njs = e->nchildren - 1;
        int *jumps = (int *)malloc((size_t)njs * sizeof(int));
        for (int i = 0; i < e->nchildren; i++) {
            lower_expr(p, lt, e->children[i]);
            if (i < e->nchildren - 1) {
                jumps[i] = sm_emit_i(p, SM_JUMP_S, 0);  /* if success, jump to done */
                sm_emit(p, SM_VOID_POP);                       /* discard failure value */
            }
        }
        int done = sm_label(p);
        for (int i = 0; i < njs; i++)
            sm_patch_jump(p, jumps[i], done);
        free(jumps);
        return;
    }

    /* ── String concatenation (or pattern concatenation if any child is *X) ── */
    case AST_CAT:
    case AST_SEQ: {
        /* SN-26c-parseerr-g: parallel to interp.c AST_SEQ fix.  In value
         * context, AST_DEFER lowers to SM_PUSH_EXPR (DT_E frozen expr).
         * SM_CONCAT(DT_E, string) produces garbage that won't match.
         * If any child is AST_DEFER, this SEQ is building a pattern —
         * lower as pattern and use SM_PAT_CAT.  Mirror in JIT (sm_codegen.c)
         * is automatic since codegen reads the same SM ops. */
        int has_defer = 0;
        for (int j = 0; j < e->nchildren; j++) {
            const AST_t *cj = e->children[j];
            if (cj && cj->kind == AST_DEFER) { has_defer = 1; break; }
        }
        if (has_defer) {
            for (int i = 0; i < e->nchildren; i++)
                lower_pat_expr(p, lt, e->children[i]);
            for (int i = 1; i < e->nchildren; i++)
                sm_emit(p, SM_PAT_CAT);
            /* SM_PAT_BOXVAL removed by ME-1: SM_PAT_CAT leaves result on value stack */
        } else {
            for (int i = 0; i < e->nchildren; i++)
                lower_expr(p, lt, e->children[i]);
            for (int i = 1; i < e->nchildren; i++)
                sm_emit(p, SM_CONCAT);
        }
        return;
    }

    /* ── Assignment ── */
    case AST_ASSIGN:
        /* child[1] = rhs value; child[0] = lhs variable */
        lower_expr(p, lt, e->nchildren > 1 ? e->children[1] : NULL);
        if (e->nchildren > 0 && e->children[0]) {
            const AST_t *lhs = e->children[0];
            if (lhs->kind == AST_VAR) {
                const char *vn = lhs->sval ? lhs->sval : "";
                /* CHUNKS-step17b'' (CH-17b''): inside expression-body lowering,
                 * consult per-proc scope.  Frame slot → SM_STORE_FRAME slot.
                 * Globals / keywords / unscoped names → SM_STORE_VAR (NV store). */
                if (g_expression_body_lowering && g_expression_scope && vn[0] && vn[0] != '&') {
                    int slot = scope_get(g_expression_scope, vn);
                    if (slot >= 0) {
                        sm_emit_i(p, SM_STORE_FRAME, slot);
                        return;
                    }
                }
                sm_emit_s(p, SM_STORE_VAR, vn);
            }
            else if (lhs->kind == AST_KEYWORD) {
                /* uppercase keyword name for NV store, matching read path */
                const char *kraw = lhs->sval ? lhs->sval : "";
                char kup[64]; int ki = 0;
                while (kraw[ki] && ki < 63) { kup[ki] = (char)toupper((unsigned char)kraw[ki]); ki++; }
                kup[ki] = '\0';
                sm_emit_s(p, SM_STORE_VAR, kup);
            }
            else if (lhs->kind == AST_FNC && lhs->sval) {
                /* Field mutator: fname(obj) = val  →  push obj, SM_CALL_FN fname_SET 2
                 * Stack on entry to setter: [val, obj] (val pushed first above) */
                lower_expr(p, lt, lhs->nchildren > 0 ? lhs->children[0] : NULL);
                char setname[256];
                snprintf(setname, sizeof(setname), "%s_SET", lhs->sval);
                sm_emit_si(p, SM_CALL_FN, setname, 2);
            } else {
                /* Computed lhs — push lhs expr, then generic store */
                lower_expr(p, lt, lhs);
                sm_emit_si(p, SM_CALL_FN, "ASGN", 2);
            }
        }
        return;

    /* ── Function / builtin call ── */
    case AST_FNC: {
        int nargs = e->nchildren;
        /* CHUNKS-step02: EVAL(*expr) special case — when EVAL is called with a
         * single AST_DEFER argument, emit the expression inline + SM_CALL_EXPRESSION instead
         * of SM_PUSH_EXPRESSION + SM_CALL_FN "EVAL".  This avoids routing through
         * EXPVAL_fn from C mid-dispatch, keeping everything on the same SM stack. */
        if (nargs == 1 && e->sval && strcmp(e->sval, "EVAL") == 0
                && e->children[0] && e->children[0]->kind == AST_DEFER) {
            const AST_t *defer = e->children[0];
            const AST_t *child = defer->nchildren > 0 ? defer->children[0] : NULL;
            int skip_jump = sm_emit_i(p, SM_JUMP, 0);
            int entry_pc  = sm_label(p);
            if (child)
                lower_expr(p, lt, child);
            else
                sm_emit(p, SM_PUSH_NULL);
            sm_emit(p, SM_RETURN);
            int skip_lbl = sm_label(p);
            sm_patch_jump(p, skip_jump, skip_lbl);
            sm_emit_ii(p, SM_CALL_EXPRESSION, (int64_t)entry_pc, 0);
            return;
        }
        /* CH-17c: Icon-style AST_FNC — name in sval is NULL; children[0] is the
         * callee name node (AST_VAR with sval = function name), children[1..] are args.
         * Emit only the arg children; use children[0]->sval as the SM_CALL_FN name.
         * This fixes the stack-leak / empty-name bug in proc-body expressions that
         * contain user proc calls (noted in CH-17b'' handoff). */
        if (!e->sval && nargs >= 1 && e->children[0] && e->children[0]->sval) {
            const char *fn = e->children[0]->sval;
            int real_nargs = nargs - 1;   /* children[1..nargs-1] are args */
            for (int i = 1; i <= real_nargs; i++)
                lower_expr(p, lt, e->children[i]);
            sm_emit_si(p, SM_CALL_FN, fn, (int64_t)real_nargs);
            return;
        }
        for (int i = 0; i < nargs; i++)
            lower_expr(p, lt, e->children[i]);
        sm_emit_si(p, SM_CALL_FN, e->sval ? e->sval : "", (int64_t)nargs);
        return;
    }

    /* ── Array / table subscript ── */
    case AST_IDX:
        for (int i = 0; i < e->nchildren; i++)
            lower_expr(p, lt, e->children[i]);
        sm_emit_si(p, SM_CALL_FN, "IDX", (int64_t)e->nchildren);
        return;

    /* ── Relational comparisons (numeric) → SM_ACOMP(op) ──
     * a[0].i carries the operator EKind (AST_EQ/AST_NE/AST_LT/AST_LE/AST_GT/AST_GE).
     * Runtime semantics (Icon-style): on success pushes the RIGHT operand
     * and sets last_ok=1; on failure pushes FAILDESCR and clears last_ok.
     * SM_JUMP_F dispatches on last_ok.  CH-17g-runtime-bridge-acomp,
     * sess 2026-05-09: prior emission was `sm_emit(p, SM_ACOMP)` with no
     * operator info — collapsed all six EKinds to a single opcode and
     * left the comparator unrecoverable at runtime.  See
     * docs/CHUNKS-step17g-runtime-bridge-acomp-validation.md. */
    case AST_EQ:
    case AST_NE:
    case AST_LT:
    case AST_LE:
    case AST_GT:
    case AST_GE: {
        lower_expr(p, lt, e->nchildren > 0 ? e->children[0] : NULL);
        lower_expr(p, lt, e->nchildren > 1 ? e->children[1] : NULL);
        sm_emit_i(p, SM_ACOMP, (int64_t)e->kind);
        return;
    }

    /* ── Relational comparisons (string/lexicographic) → SM_LCOMP(op) ──
     * a[0].i carries the operator EKind (AST_LLT/AST_LLE/AST_LGT/AST_LGE/AST_LEQ/AST_LNE).
     * Runtime semantics (Icon-style, mirrors STRREL macro in interp_eval.c):
     * on success pushes the RIGHT operand and sets last_ok=1; on failure
     * pushes FAILDESCR and clears last_ok.  CH-17g-runtime-bridge-lcomp,
     * sess 2026-05-09: sibling fix to bridge-acomp — same shape bug, prior
     * emission was `sm_emit(p, SM_LCOMP)` with no operator info, collapsing
     * all six string relop EKinds onto one opcode. */
    case AST_LLT:
    case AST_LLE:
    case AST_LGT:
    case AST_LGE:
    case AST_LEQ:
    case AST_LNE:
        lower_expr(p, lt, e->nchildren > 0 ? e->children[0] : NULL);
        lower_expr(p, lt, e->nchildren > 1 ? e->children[1] : NULL);
        sm_emit_i(p, SM_LCOMP, (int64_t)e->kind);
        return;

    /* ── Interrogation ?X → succeed if X succeeds ── */
    case AST_INTERROGATE:
        lower_expr(p, lt, e->nchildren > 0 ? e->children[0] : NULL);
        /* result already on stack; success/failure propagates */
        return;

    /* ── Name reference .X — push DT_N name descriptor onto value stack ── */
    case AST_NAME: {
        /* Push the variable name as a string, then NAME_PUSH converts to DT_N */
        const char *vname = (e->nchildren > 0 && e->children[0] && e->children[0]->sval)
                            ? e->children[0]->sval : "";
        sm_emit_s(p, SM_PUSH_LIT_S, vname);
        sm_emit_si(p, SM_CALL_FN, "NAME_PUSH", 1);
        return;
    }

    /* ── Scan E ? E ── */
    case AST_SCAN:
        lower_pat_expr(p, lt, e->nchildren > 1 ? e->children[1] : NULL);
        lower_expr(p, lt, e->nchildren > 0 ? e->children[0] : NULL);
        sm_emit_i(p, SM_PUSH_LIT_I, 0);   /* no replacement */
        sm_emit(p, SM_EXEC_STMT);
        sm_emit(p, SM_PUSH_NULL_NOFLIP);   /* balance value stack; preserve last_ok from scan */
        return;

    /* ── OPSYN operator & / @ / | — dispatch via APPLY_fn(sval, args, n) ── */
    case AST_OPSYN: {
        /* sval is either:
         *   mangled: "BIATFN(@)", "ORFN(|)", "BIAMFN(&)" — op char between '(' and ')'
         *   bare:    "BARFN", "AROWFN" — unary ops from uop_names[] table
         * Extract the bare operator char so APPLY_fn finds the opsyn alias. */
        const char *raw = e->sval ? e->sval : "&";
        const char *op = raw;
        static char op_buf[4];
        const char *lp = strchr(raw, '(');
        if (lp && lp[1] && lp[2] == ')') {
            /* mangled form: extract char between parens */
            op_buf[0] = lp[1]; op_buf[1] = '\0';
            op = op_buf;
        } else if (strcmp(raw, "BARFN")  == 0) { op = "|"; }
        else if (strcmp(raw, "AROWFN") == 0) { op = "^"; }
        for (int i = 0; i < e->nchildren; i++)
            lower_expr(p, lt, e->children[i]);
        sm_emit_si(p, SM_CALL_FN, op, (int64_t)e->nchildren);
        return;
    }

    /* ── Swap :=: ── */
    case AST_SWAP:
        lower_expr(p, lt, e->nchildren > 0 ? e->children[0] : NULL);
        lower_expr(p, lt, e->nchildren > 1 ? e->children[1] : NULL);
        sm_emit_si(p, SM_CALL_FN, "SWAP", 2);
        return;

    /* ── Pattern primitives used as values (not already handled above) ── */
    case AST_ALT:
    case AST_ARB:  case AST_REM:  case AST_FAIL: case AST_SUCCEED:
    case AST_FENCE: case AST_ABORT: case AST_BAL:
    case AST_ANY:  case AST_NOTANY: case AST_SPAN: case AST_BREAK: case AST_BREAKX:
    case AST_LEN:  case AST_POS:  case AST_RPOS: case AST_TAB: case AST_RTAB:
    case AST_ARBNO:
    case AST_CAPT_COND_ASGN: case AST_CAPT_IMMED_ASGN: case AST_CAPT_CURSOR:
        lower_pat_expr(p, lt, e);
        /* SM_PAT_BOXVAL removed by ME-1: lower_pat_expr leaves result on value stack */
        return;

    /* ══════════════════════════════════════════════════════════════════════
     * FI-10: SM lowering for non-SNOBOL4 EKinds
     *
     * Functional nodes → inline SM opcodes / control-flow via sm_label/patch.
     * Generator nodes  → SM_PUSH_EXPR + SM_BB_PUMP  (delegate to BB broker).
     * Prolog nodes     → SM_PUSH_EXPR + SM_BB_ONCE.
     * ══════════════════════════════════════════════════════════════════════ */

    /* ── Sequential expression list: eval all, leave last on stack ─────── */
    case AST_SEQ_EXPR:
        if (e->nchildren == 0) { sm_emit(p, SM_PUSH_NULL); return; }
        for (int i = 0; i < e->nchildren; i++) {
            lower_expr(p, lt, e->children[i]);
            if (i < e->nchildren - 1) sm_emit(p, SM_VOID_POP);
        }
        return;

    /* ── Icon/Raku if-then[-else] ───────────────────────────────────────── */
    case AST_IF: {
        if (e->nchildren < 1) { sm_emit(p, SM_PUSH_NULL); return; }
        lower_expr(p, lt, e->children[0]);              /* condition */
        int jf = sm_emit_i(p, SM_JUMP_F, 0);           /* jump-if-fail to else */
        if (e->nchildren > 1) lower_expr(p, lt, e->children[1]);
        else                  sm_emit(p, SM_PUSH_NULL);
        int jend = sm_emit_i(p, SM_JUMP, 0);           /* jump past else */
        int else_lbl = sm_label(p);
        sm_patch_jump(p, jf,   else_lbl);
        if (e->nchildren > 2) lower_expr(p, lt, e->children[2]);
        else                  sm_emit(p, SM_PUSH_NULL);
        int end_lbl = sm_label(p);
        sm_patch_jump(p, jend, end_lbl);
        return;
    }

    /* ── while (cond) body — functional form ───────────────────────────── */
    case AST_WHILE: {
        int top_lbl = sm_label(p);
        if (e->nchildren < 1) { sm_emit(p, SM_PUSH_NULL); return; }
        lower_expr(p, lt, e->children[0]);              /* condition */
        int jf = sm_emit_i(p, SM_JUMP_F, 0);           /* exit on fail */
        sm_emit(p, SM_VOID_POP);
        if (e->nchildren > 1) { lower_expr(p, lt, e->children[1]); sm_emit(p, SM_VOID_POP); }
        sm_emit_i(p, SM_JUMP, top_lbl);
        int end_lbl = sm_label(p);
        sm_patch_jump(p, jf, end_lbl);
        sm_emit(p, SM_PUSH_NULL);
        return;
    }

    /* ── until (!cond) body ─────────────────────────────────────────────── */
    case AST_UNTIL: {
        int top_lbl = sm_label(p);
        if (e->nchildren < 1) { sm_emit(p, SM_PUSH_NULL); return; }
        lower_expr(p, lt, e->children[0]);
        int js = sm_emit_i(p, SM_JUMP_S, 0);           /* exit when cond succeeds */
        sm_emit(p, SM_VOID_POP);
        if (e->nchildren > 1) { lower_expr(p, lt, e->children[1]); sm_emit(p, SM_VOID_POP); }
        sm_emit_i(p, SM_JUMP, top_lbl);
        int end_lbl = sm_label(p);
        sm_patch_jump(p, js, end_lbl);
        sm_emit(p, SM_PUSH_NULL);
        return;
    }

    /* ── repeat body — loops until break ───────────────────────────────── */
    case AST_REPEAT: {
        int top_lbl = sm_label(p);
        if (e->nchildren > 0) { lower_expr(p, lt, e->children[0]); sm_emit(p, SM_VOID_POP); }
        sm_emit_i(p, SM_JUMP, top_lbl);
        sm_emit(p, SM_PUSH_NULL);
        return;
    }

    /* ── loop control ───────────────────────────────────────────────────── */
    case AST_LOOP_BREAK:
        /* In SM context: push result (if any) and halt the enclosing loop.
         * SM doesn't track loop nesting — emit JUMP to a sentinel; sm_interp
         * treats a SM_JUMP with target == current pc+1 as break. */
        if (e->nchildren > 0) lower_expr(p, lt, e->children[0]);
        else sm_emit(p, SM_PUSH_NULL);
        /* SM_JUMP to self+1 signals break to sm_interp loop handler */
        sm_emit_i(p, SM_JUMP, p->count + 1);
        return;

    case AST_LOOP_NEXT:
        sm_emit(p, SM_PUSH_NULL);
        return;

    /* ── return / freturn from user function ────────────────────────────── */
    case AST_RETURN:
        if (e->nchildren > 0) lower_expr(p, lt, e->children[0]);
        else sm_emit(p, SM_PUSH_NULL);
        sm_emit(p, SM_RETURN);
        return;

    case AST_PROC_FAIL:
        sm_emit(p, SM_PUSH_NULL);
        sm_emit(p, SM_FRETURN);
        return;

    /* ── logical not: succeed if child fails, fail if child succeeds ────── */
    case AST_NOT: {
        lower_expr(p, lt, e->nchildren > 0 ? e->children[0] : NULL);
        int js = sm_emit_i(p, SM_JUMP_S, 0);   /* child succeeded → fail */
        /* child failed → ~ succeeds: discard child value, push null, last_ok=1 */
        sm_emit(p, SM_VOID_POP);
        sm_emit(p, SM_PUSH_NULL);
        int jend = sm_emit_i(p, SM_JUMP, 0);
        int fail_lbl = sm_label(p);
        sm_patch_jump(p, js, fail_lbl);
        /* child succeeded → ~ fails: discard child value, push fail, last_ok=0 */
        sm_emit(p, SM_VOID_POP);
        sm_emit_si(p, SM_CALL_FN, "FAIL", 0);     /* push fail descriptor */
        int end_lbl = sm_label(p);
        sm_patch_jump(p, jend, end_lbl);
        return;
    }

    /* ── augmented assignment (+=, -=, ||=, etc.) ──────────────────────── */
    case AST_AUGOP:
        lower_expr(p, lt, e->nchildren > 0 ? e->children[0] : NULL);
        lower_expr(p, lt, e->nchildren > 1 ? e->children[1] : NULL);
        sm_emit_i(p, SM_PUSH_LIT_I, (int64_t)(e->ival));  /* operator token */
        sm_emit_si(p, SM_CALL_FN, "AUGOP", 3);
        return;

    /* ── string/list size *e ────────────────────────────────────────────── */
    case AST_SIZE:
        lower_expr(p, lt, e->nchildren > 0 ? e->children[0] : NULL);
        sm_emit_si(p, SM_CALL_FN, "SIZE", 1);
        return;

    /* ── non-null test \e — succeed with e if e != null, else fail ──────── */
    case AST_NONNULL:
        lower_expr(p, lt, e->nchildren > 0 ? e->children[0] : NULL);
        sm_emit_si(p, SM_CALL_FN, "NONNULL", 1);
        return;

    /* ── string identity ===  / non-identity ~=== ───────────────────────── */
    case AST_IDENTICAL:
        lower_expr(p, lt, e->nchildren > 0 ? e->children[0] : NULL);
        lower_expr(p, lt, e->nchildren > 1 ? e->children[1] : NULL);
        sm_emit_si(p, SM_CALL_FN, "IDENTICAL", 2);
        return;

    /* ── list/array literal [a, b, c] ──────────────────────────────────── */
    case AST_MAKELIST:
        for (int i = 0; i < e->nchildren; i++)
            lower_expr(p, lt, e->children[i]);
        sm_emit_si(p, SM_CALL_FN, "MAKELIST", (int64_t)e->nchildren);
        return;

    /* ── record construction ────────────────────────────────────────────── */
    case AST_RECORD:
        sm_emit_s(p, SM_PUSH_LIT_S, e->sval ? e->sval : "");
        for (int i = 0; i < e->nchildren; i++)
            lower_expr(p, lt, e->children[i]);
        sm_emit_si(p, SM_CALL_FN, "RECORD_MAKE", (int64_t)e->nchildren + 1);
        return;

    /* ── field access r.field ───────────────────────────────────────────── */
    case AST_FIELD:
        lower_expr(p, lt, e->nchildren > 0 ? e->children[0] : NULL);
        sm_emit_s(p, SM_PUSH_LIT_S, e->sval ? e->sval : "");
        sm_emit_si(p, SM_CALL_FN, "FIELD_GET", 2);
        return;

    /* ── declarative / init nodes — no runtime action at expression level ─ */
    case AST_GLOBAL:
    case AST_INITIAL:
        sm_emit(p, SM_PUSH_NULL);
        return;

    /* ══════════════════════════════════════════════════════════════════════
     * Generator/backtracking nodes → BB broker
     * SM_PUSH_EXPR bakes the AST_t* in; SM_BB_PUMP drives via bb_broker.
     * Prolog-specific nodes use SM_BB_ONCE (find one solution).
     * ══════════════════════════════════════════════════════════════════════ */

    /* ══════════════════════════════════════════════════════════════════════
     * Generator/backtracking nodes → BB broker
     * SM_PUSH_EXPR bakes the AST_t* in; SM_BB_PUMP drives via bb_broker.
     * Prolog-specific nodes use SM_BB_ONCE (find one solution).
     * ══════════════════════════════════════════════════════════════════════ */

    /* CHUNKS-step15a: AST_TO / AST_TO_BY migrated to SM chunks + SM_BB_PUMP_SM.
     * gen-local slot layout (shared across AST_TO and AST_TO_BY):
     *   glocal[0] = lo   (integer lower bound)
     *   glocal[1] = hi   (integer upper bound)
     *   glocal[2] = cur  (current value, updated each resume)
     *   glocal[3] = step (AST_TO_BY only; AST_TO implicitly 1)
     *
     * Expression shape (AST_TO):
     *   SM_JUMP  skip_pc          ; jump over body to SM_BB_PUMP_SM
     * entry_pc:
     *   SM_RESUME                 ; documentation hook for future JIT
     *   lower_expr(lo)            ; evaluate lo bound
     *   SM_STORE_GLOCAL 0         ; glocal[0] = lo (value stays on stack)
     *   SM_VOID_POP                    ; discard
     *   lower_expr(hi)            ; evaluate hi bound
     *   SM_STORE_GLOCAL 1         ; glocal[1] = hi
     *   SM_VOID_POP
     *   SM_LOAD_GLOCAL 0          ; cur = lo
     *   SM_STORE_GLOCAL 2         ; glocal[2] = cur
     *   SM_VOID_POP
     * loop_pc:
     *   SM_LOAD_GLOCAL 2          ; push cur
     *   SM_LOAD_GLOCAL 1          ; push hi
     *   SM_ICMP_GT                ; last_ok = (cur > hi)
     *   SM_JUMP_S exit_pc         ; exhausted → exit
     *   SM_LOAD_GLOCAL 2          ; push cur as yielded value
     *   SM_SUSPEND                ; yield; resume after this point
     *   SM_LOAD_GLOCAL 2          ; push cur
     *   SM_INCR step              ; cur += step (1 for AST_TO)
     *   SM_STORE_GLOCAL 2         ; glocal[2] = new cur
     *   SM_VOID_POP
     *   SM_JUMP loop_pc           ; next iteration
     * exit_pc:
     *   SM_PUSH_NULL              ; ω: generator exhausted
     *   SM_RETURN
     * skip_pc:
     *   SM_BB_PUMP_SM entry_pc    ; drive the expression as a generator
     */
    case AST_TO: {
        const AST_t *lo_expr = (e->nchildren > 0) ? e->children[0] : NULL;
        const AST_t *hi_expr = (e->nchildren > 1) ? e->children[1] : NULL;
        int skip_jump = sm_emit_i(p, SM_JUMP, 0);       /* forward jump — patched to skip_pc */
        int entry_pc  = sm_label(p);                     /* expression entry point */
        sm_emit(p, SM_RESUME);                           /* JIT hook */
        /* initialise glocals: lo, hi, cur */
        if (lo_expr) lower_expr(p, lt, lo_expr);
        else         sm_emit_i(p, SM_PUSH_LIT_I, 0);
        sm_emit_i(p, SM_STORE_GLOCAL, 0);               /* glocal[0] = lo */
        sm_emit(p, SM_VOID_POP);
        if (hi_expr) lower_expr(p, lt, hi_expr);
        else         sm_emit_i(p, SM_PUSH_LIT_I, 0);
        sm_emit_i(p, SM_STORE_GLOCAL, 1);               /* glocal[1] = hi */
        sm_emit(p, SM_VOID_POP);
        sm_emit_i(p, SM_LOAD_GLOCAL, 0);                /* cur = lo */
        sm_emit_i(p, SM_STORE_GLOCAL, 2);               /* glocal[2] = cur */
        sm_emit(p, SM_VOID_POP);
        /* loop: test cur > hi; yield cur; cur++ */
        int loop_pc = sm_label(p);
        sm_emit_i(p, SM_LOAD_GLOCAL, 2);                /* push cur */
        sm_emit_i(p, SM_LOAD_GLOCAL, 1);                /* push hi */
        sm_emit(p, SM_ICMP_GT);                          /* last_ok = (cur > hi) */
        int exit_jump = sm_emit_i(p, SM_JUMP_S, 0);     /* patched to exit_pc */
        sm_emit_i(p, SM_LOAD_GLOCAL, 2);                /* push cur = yielded value */
        sm_emit(p, SM_SUSPEND);                          /* yield cur */
        sm_emit_i(p, SM_LOAD_GLOCAL, 2);                /* push cur for increment */
        sm_emit_i(p, SM_INCR, 1);                        /* cur + 1 */
        sm_emit_i(p, SM_STORE_GLOCAL, 2);               /* glocal[2] = cur+1 */
        sm_emit(p, SM_VOID_POP);
        sm_emit_i(p, SM_JUMP, loop_pc);                  /* back to loop */
        int exit_pc_here = sm_label(p);
        sm_patch_jump(p, exit_jump, exit_pc_here);
        sm_emit(p, SM_PUSH_NULL);                        /* ω */
        sm_emit(p, SM_RETURN);
        int skip_pc = sm_label(p);
        sm_patch_jump(p, skip_jump, skip_pc);
        sm_emit_ii(p, SM_PUSH_EXPRESSION, (int64_t)entry_pc, 0); /* push expression descriptor */
        sm_emit(p, SM_BB_PUMP_SM);                           /* pop + drive as generator */
        return;
    }

    case AST_TO_BY: {
        /* AST_TO_BY(lo, hi, step) — same shape as AST_TO but step is glocal[3].
         * Children: [0]=lo, [1]=hi, [2]=step.
         * Mirrors coro_bb_to_by semantics:
         *   step > 0: exit when cur > hi
         *   step < 0: exit when cur < hi
         *   step == 0: treated as +1 (degenerate) — same as positive case.
         * Loop dispatches on step sign each iteration via SM_ICMP_LT against 0. */
        const AST_t *lo_expr   = (e->nchildren > 0) ? e->children[0] : NULL;
        const AST_t *hi_expr   = (e->nchildren > 1) ? e->children[1] : NULL;
        const AST_t *step_expr = (e->nchildren > 2) ? e->children[2] : NULL;
        int skip_jump = sm_emit_i(p, SM_JUMP, 0);
        int entry_pc  = sm_label(p);
        sm_emit(p, SM_RESUME);
        /* initialise glocals: lo, hi, step, cur */
        if (lo_expr) lower_expr(p, lt, lo_expr);
        else         sm_emit_i(p, SM_PUSH_LIT_I, 0);
        sm_emit_i(p, SM_STORE_GLOCAL, 0);
        sm_emit(p, SM_VOID_POP);
        if (hi_expr) lower_expr(p, lt, hi_expr);
        else         sm_emit_i(p, SM_PUSH_LIT_I, 0);
        sm_emit_i(p, SM_STORE_GLOCAL, 1);
        sm_emit(p, SM_VOID_POP);
        if (step_expr) lower_expr(p, lt, step_expr);
        else           sm_emit_i(p, SM_PUSH_LIT_I, 1);
        sm_emit_i(p, SM_STORE_GLOCAL, 3);               /* glocal[3] = step */
        sm_emit(p, SM_VOID_POP);
        sm_emit_i(p, SM_LOAD_GLOCAL, 0);                /* cur = lo */
        sm_emit_i(p, SM_STORE_GLOCAL, 2);
        sm_emit(p, SM_VOID_POP);
        /* loop: dispatch on step sign for exit test */
        int loop_pc = sm_label(p);
        sm_emit_i(p, SM_LOAD_GLOCAL, 3);                /* push step */
        sm_emit_i(p, SM_PUSH_LIT_I, 0);                 /* push 0 */
        sm_emit(p, SM_ICMP_LT);                          /* last_ok = (step < 0) */
        int neg_branch = sm_emit_i(p, SM_JUMP_S, 0);    /* if step<0 → neg_test */
        /* positive-step exit test: cur > hi */
        sm_emit_i(p, SM_LOAD_GLOCAL, 2);                /* push cur */
        sm_emit_i(p, SM_LOAD_GLOCAL, 1);                /* push hi */
        sm_emit(p, SM_ICMP_GT);
        int exit_jump_pos = sm_emit_i(p, SM_JUMP_S, 0); /* if cur>hi → exit */
        int body_jump = sm_emit_i(p, SM_JUMP, 0);        /* skip neg_test → body */
        /* negative-step exit test: cur < hi */
        int neg_pc = sm_label(p);
        sm_patch_jump(p, neg_branch, neg_pc);
        sm_emit_i(p, SM_LOAD_GLOCAL, 2);                /* push cur */
        sm_emit_i(p, SM_LOAD_GLOCAL, 1);                /* push hi */
        sm_emit(p, SM_ICMP_LT);
        int exit_jump_neg = sm_emit_i(p, SM_JUMP_S, 0); /* if cur<hi → exit */
        /* body: yield cur; cur += step */
        int body_pc = sm_label(p);
        sm_patch_jump(p, body_jump, body_pc);
        sm_emit_i(p, SM_LOAD_GLOCAL, 2);                /* yield cur */
        sm_emit(p, SM_SUSPEND);
        sm_emit_i(p, SM_LOAD_GLOCAL, 2);                /* cur for addition */
        sm_emit_i(p, SM_LOAD_GLOCAL, 3);                /* step */
        sm_emit(p, SM_ADD);                              /* cur + step */
        sm_emit_i(p, SM_STORE_GLOCAL, 2);
        sm_emit(p, SM_VOID_POP);
        sm_emit_i(p, SM_JUMP, loop_pc);
        int exit_pc_here = sm_label(p);
        sm_patch_jump(p, exit_jump_pos, exit_pc_here);
        sm_patch_jump(p, exit_jump_neg, exit_pc_here);
        sm_emit(p, SM_PUSH_NULL);
        sm_emit(p, SM_RETURN);
        int skip_pc = sm_label(p);
        sm_patch_jump(p, skip_jump, skip_pc);
        sm_emit_ii(p, SM_PUSH_EXPRESSION, (int64_t)entry_pc, 0);
        sm_emit(p, SM_BB_PUMP_SM);
        return;
    }

    /* CHUNKS-step17i-every-suspend: AST_EVERY migrated off legacy
     * emit_push_expr + SM_BB_PUMP onto a CH-17f-style pattern.
     * Register the AST in g_every_table at lower-time; emit
     * SM_BB_PUMP_EVERY <every_id> — no AST_t* in SM bytecode or value stack.
     * The runtime handler looks up by id, builds the box via coro_eval
     * (existing IR-side icn_every_state_t), drives via bb_broker(BB_PUMP,
     * pump_print), and pushes NULVCL to balance the trailing SM_VOID_POP
     * from proc-body lowering's `lower_expr(body); SM_VOID_POP` loop.
     *
     * Pre-rung (legacy emit_push_expr + SM_BB_PUMP): net stack delta 0;
     * trailing SM_VOID_POP underflowed when reached via sm_call_proc
     * (CH-17g) in proc-body-lowered chunks — root cause of all 111
     * --sm-run divergences in the CH-17i survey (sess 2026-05-09).
     * Post-rung: net delta +1 (NULVCL), SM_VOID_POP balanced. */
    case AST_EVERY: {
        int every_id = every_table_register((AST_t *)e);
        sm_emit_i(p, SM_BB_PUMP_EVERY, (int64_t)every_id);
        return;
    }

    /* CHUNKS-step17i-suspend: AST_SUSPEND lowering — direct yield primitive.
     *
     * Mirrors coro_stmt.c:88's runtime semantics in SM bytecode.  No
     * coro_eval, no broker, no AST_t* on the SM stack.  See SM_SUSPEND_VALUE
     * doc-comment in sm_prog.h for the lowering shape and rationale.
     *
     * Children:
     *   children[0] = expr (yield value)
     *   children[1] = do-clause (optional)
     *
     * Net stack delta: +1 (push NULVCL placeholder for outer SM_VOID_POP) on
     * success path; +1 (failed v left on stack) on failure path.  Either
     * way the outer proc-body loop's trailing SM_VOID_POP balances. */
    case AST_SUSPEND: {
        /* Lower the value expression — must produce one value on stack. */
        if (e->nchildren > 0 && e->children[0])
            lower_expr(p, lt, e->children[0]);
        else
            sm_emit(p, SM_PUSH_NULL);

        /* If the value failed, skip yield + do-clause; leave failed v on stack. */
        int j_end = sm_emit_i(p, SM_JUMP_F, 0);

        /* Yield: pop v, swapcontext to caller.  On resume, fall through. */
        sm_emit(p, SM_SUSPEND_VALUE);

        /* Run do-clause (if present); discard its value. */
        if (e->nchildren > 1 && e->children[1]) {
            lower_expr(p, lt, e->children[1]);
            sm_emit(p, SM_VOID_POP);
        }

        /* Push NULVCL placeholder for outer proc-body SM_VOID_POP. */
        sm_emit(p, SM_PUSH_NULL);
        int j_done = sm_emit_i(p, SM_JUMP, 0);

        /* L_end: failed-v fall-through. Stack still has the failed value. */
        int lbl_end = sm_label(p);
        sm_patch_jump(p, j_end, lbl_end);

        /* L_finally: both paths converge; outer SM_VOID_POP fires here. */
        int lbl_finally = sm_label(p);
        sm_patch_jump(p, j_done, lbl_finally);
        return;
    }

    /* CHUNKS-step17i-bang-concat Phase 1 — AST_LCONCAT scalar value-path lowering.
     *
     * Icon `|||` (AST_LCONCAT) is a string-concat alias when both operands are
     * non-generative scalars — see interp_eval.c:3827, which simply does
     * VARVAL_fn(c0) ++ VARVAL_fn(c1).  AST_CAT's else-branch (line 740) shows
     * the canonical SM shape for this: lower each child, emit SM_CONCAT
     * between adjacent pairs.  Mirror it.
     *
     * Pre-rung: AST_LCONCAT fell through to legacy emit_push_expr + SM_BB_PUMP,
     * which is net-stack-zero — broke value-context use (e.g.,
     * `s := "hello" ||| " world"` in rung15_real_swap_lconcat: stack underflow
     * on AST_ASSIGN's RHS pop).
     *
     * If any child is is_suspendable (gen ||| gen, gen ||| str, etc.), the
     * scalar SM_CONCAT shape is wrong — coro_eval would route through
     * coro_bb_binop with a cross-product yielding multiple values.  That case
     * is Phase 2 of CH-17i-bang-concat (unified SM_BB_PUMP_AST opcode); until
     * Phase 2 lands, fall through to the legacy path so behaviour is
     * unchanged for the gen case.
     */
    case AST_LCONCAT: {
        int has_gen = 0;
        for (int j = 0; j < e->nchildren; j++) {
            if (is_suspendable(e->children[j])) { has_gen = 1; break; }
        }
        if (!has_gen) {
            if (e->nchildren < 1) { sm_emit(p, SM_PUSH_NULL); return; }
            for (int i = 0; i < e->nchildren; i++)
                lower_expr(p, lt, e->children[i]);
            for (int i = 1; i < e->nchildren; i++)
                sm_emit(p, SM_CONCAT);
            return;
        }
        /* GOAL-ICON-BB-COMPLETE Phase A (A1): generative AST_LCONCAT via SM_BB_PUMP_AST.
         * Registers the AST node in g_ast_pump_table; SM_BB_PUMP_AST drives
         * coro_eval -> bb_node alpha at runtime.  No raw AST_t* in SM bytecode. */
        sm_emit_i(p, SM_BB_PUMP_AST, (int64_t)ast_pump_table_register((AST_t *)e));
        return;
    }

    /* GOAL-ICON-BB-COMPLETE Phase A (A1): AST_BANG_BINARY migrated off legacy fallthrough.
     * Remaining kinds stay on legacy until their own Phase A rung lands. */
    case AST_BANG_BINARY:
        sm_emit_i(p, SM_BB_PUMP_AST, (int64_t)ast_pump_table_register((AST_t *)e));
        return;

    /* GOAL-ICON-BB-COMPLETE Phase A (A4 pulled forward): AST_ITERATE (!E).
     * coro_bb_iterate already implements the full Byrd-box for string/list iterate.
     * Route through SM_BB_PUMP_AST — no raw AST_t* on SM stack. */
    case AST_ITERATE:
        sm_emit_i(p, SM_BB_PUMP_AST, (int64_t)ast_pump_table_register((AST_t *)e));
        return;

    /* GOAL-ICON-BB-COMPLETE Phase A (A2): AST_SECTION / AST_SECTION_MINUS /
     * AST_SECTION_PLUS migrated off legacy fallthrough.
     * These are scalar (one-shot) operations.  Lower children via lower_expr
     * so SM_LOAD_FRAME / SM_PUSH_VAR correctly resolve local and global vars,
     * then call a typed SM runtime helper that mirrors bb_section().
     * Three children: children[0]=string, children[1]=lo, children[2]=hi. */
    case AST_SECTION:
        if (e->nchildren >= 3) {
            lower_expr(p, lt, e->children[0]);
            lower_expr(p, lt, e->children[1]);
            lower_expr(p, lt, e->children[2]);
            sm_emit_si(p, SM_CALL_FN, "ICN_SECTION_RANGE", 3);
        } else {
            sm_emit(p, SM_PUSH_NULL);
        }
        return;
    case AST_SECTION_PLUS:
        if (e->nchildren >= 3) {
            lower_expr(p, lt, e->children[0]);
            lower_expr(p, lt, e->children[1]);
            lower_expr(p, lt, e->children[2]);
            sm_emit_si(p, SM_CALL_FN, "ICN_SECTION_PLUS", 3);
        } else {
            sm_emit(p, SM_PUSH_NULL);
        }
        return;
    case AST_SECTION_MINUS:
        if (e->nchildren >= 3) {
            lower_expr(p, lt, e->children[0]);
            lower_expr(p, lt, e->children[1]);
            lower_expr(p, lt, e->children[2]);
            sm_emit_si(p, SM_CALL_FN, "ICN_SECTION_MINUS", 3);
        } else {
            sm_emit(p, SM_PUSH_NULL);
        }
        return;

    /* Icon generators — remaining kinds still use legacy emit_push_expr + SM_BB_PUMP.
     * LIMIT is handled honestly when wrapped in EVERY (coro_bb_every drives it);
     * direct LIMIT usage is rare in corpus. Keep on legacy until a failing test requires it. */
    case AST_LIMIT:
        emit_push_expr(p, e);
        sm_emit(p, SM_BB_PUMP);
        return;

    /* GOAL-ICON-BB-COMPLETE A3: AST_RANDOM (?E) — scalar SM lowering.
     * AST_RANDOM is one-shot (not a generator). Lower child via lower_expr
     * so SM_LOAD_FRAME / SM_PUSH_VAR correctly resolve locals/globals,
     * then call ICN_RANDOM which mirrors bb_eval_value's AST_RANDOM arm. */
    case AST_RANDOM:
        if (e->nchildren >= 1) {
            lower_expr(p, lt, e->children[0]);
            sm_emit_si(p, SM_CALL_FN, "ICN_RANDOM", 1);
        } else {
            sm_emit(p, SM_PUSH_NULL);
        }
        return;

    /* Prolog backtracking nodes */
    case AST_CHOICE:
        /* CH-17f: AST_CHOICE in value context — emit SM_BB_ONCE_PROC by key.
         * Mirrors the lower_stmt fix; no raw AST_t* pushed to SM stack. */
        if (e->sval) {
            const char *key = e->sval;
            int arity = 0;
            const char *sl = strrchr(key, '/');
            if (sl) arity = atoi(sl + 1);
            sm_emit_si(p, SM_BB_ONCE_PROC, key, (int64_t)arity);
        } else {
            emit_push_expr(p, e);
            sm_emit(p, SM_BB_ONCE);
        }
        return;
    case AST_CLAUSE:
    case AST_CUT:
    case AST_UNIFY:
    case AST_TRAIL_MARK:
    case AST_TRAIL_UNWIND:
        /* These are children of AST_CHOICE walked by the broker, never
         * lowered standalone from sm_lower.  Keep legacy path as safety
         * fallback; should be unreachable in practice. */
        emit_push_expr(p, e);
        sm_emit(p, SM_BB_ONCE);
        return;

    /* ── Raku: typed variable declaration — no runtime action ───────────── */
    case AST_CASE: {
        /* CHUNKS-step13: Raku CASE dispatch — replaces the legacy
         * emit_push_expr + SM_BB_PUMP wrapper.  Lower each piece (topic,
         * per-arm value and body, optional default body) as its own
         * expression and emit the canonical CASE wrapper.  Stack layout
         * pushed for SM_BB_PUMP_CASE:
         *
         *   topic_chunk
         *   cmp_kind_0  val_chunk_0  body_chunk_0
         *   cmp_kind_1  val_chunk_1  body_chunk_1
         *   ...
         *   default_body_chunk        (only if has_default)
         *
         * IR layout produced by raku.y (RK-18d):
         *   AST_CASE[ topic, cmpnode_0, val_0, body_0, ..., (AST_NUL, AST_NUL, default_body)? ]
         * cmpnode_i is an AST_ILIT carrying the AST_e cmp kind in .ival,
         * or AST_NUL marking the trailing default triple.
         *
         * Wrapper-level synthesis is now AST_t-free.  Per-arm body
         * content is recursively lowered by the existing lower_expr
         * machinery — any deferred IR walking inside arm bodies for
         * not-yet-migrated kinds is M4-cleanup territory, mirroring
         * how CHUNKS-step12 deferred the proc_table IR walk to Step 17. */

        if (e->nchildren < 1) {
            /* Defensive: empty CASE — nothing to dispatch. */
            sm_emit(p, SM_PUSH_NULL);
            return;
        }

        /* Detect Raku triple layout: child count after topic is a
         * multiple of 3, AND child[1] is AST_ILIT or AST_NUL.  Mirrors the
         * detection in coro_value.c:947. */
        int is_raku_layout = (e->nchildren >= 4 && (e->nchildren - 1) % 3 == 0 &&
            e->children[1] && (e->children[1]->kind == AST_ILIT || e->children[1]->kind == AST_NUL));
        if (!is_raku_layout) {
            /* Icon-style pair layout: not exercised by the Raku frontend
             * today (Icon uses AST_IF chains for case-of), but the legacy
             * fall-through preserves behaviour for any future producer.
             * Keep going with a deferred-body wrap as one expression. */
            int skip_jump = sm_emit_i(p, SM_JUMP, 0);
            int entry_pc  = sm_label(p);
            /* Lower whole CASE body as a single thunk that delegates to
             * coro_eval via the SM stack — but coro_eval(AST_t*) is the
             * very thing we're eliminating, so for now emit a NULVCL
             * placeholder + diagnostic.  No Raku/Icon program reaches this
             * branch under current frontends.  When Icon AST_CASE is added,
             * a separate rung will lower the pair layout. */
            sm_emit(p, SM_PUSH_NULL);
            sm_emit(p, SM_RETURN);
            int skip_lbl  = sm_label(p);
            sm_patch_jump(p, skip_jump, skip_lbl);
            sm_emit_ii(p, SM_PUSH_EXPRESSION, (int64_t)entry_pc, 0);
            sm_emit_ii(p, SM_BB_PUMP_CASE, 0, 0);  /* zero arms, no default */
            return;
        }

        /* Helper macro: emit (jump-around + entry + lower(child) + RETURN) →
         * SM_PUSH_EXPRESSION entry_pc, 0.  Same shape used in Steps 2/3/4. */
        #define EMIT_CHUNK_OF(child_expr) do {                                  \
            int _skip = sm_emit_i(p, SM_JUMP, 0);                                \
            int _entry = sm_label(p);                                            \
            lower_expr(p, lt, (child_expr));                                     \
            sm_emit(p, SM_RETURN);                                               \
            int _after = sm_label(p);                                            \
            sm_patch_jump(p, _skip, _after);                                     \
            sm_emit_ii(p, SM_PUSH_EXPRESSION, (int64_t)_entry, 0);                    \
        } while (0)

        /* Walk triples, count arms, detect trailing default. */
        int total_triples = (e->nchildren - 1) / 3;
        int has_default   = 0;
        int default_idx   = -1;  /* triple index of the default, if any */
        if (total_triples > 0) {
            int last_i = 1 + (total_triples - 1) * 3;
            AST_t *last_cmp = e->children[last_i];
            if (last_cmp && last_cmp->kind == AST_NUL) {
                has_default = 1;
                default_idx = total_triples - 1;
            }
        }
        int ncases = total_triples - (has_default ? 1 : 0);

        /* 1. Push topic as expression. */
        EMIT_CHUNK_OF(e->children[0]);

        /* 2. Push each arm: cmp_kind (literal int), val expression, body expression. */
        for (int t = 0; t < total_triples; t++) {
            if (t == default_idx) continue;  /* default handled last */
            int base = 1 + t * 3;
            AST_t *cmpnode = e->children[base];
            AST_t *val     = e->children[base + 1];
            AST_t *body    = e->children[base + 2];
            int cmp_kind = (cmpnode && cmpnode->kind == AST_ILIT) ? (int)cmpnode->ival : (int)AST_EQ;
            sm_emit_i(p, SM_PUSH_LIT_I, (int64_t)cmp_kind);
            EMIT_CHUNK_OF(val);
            EMIT_CHUNK_OF(body);
        }

        /* 3. Push default body expression if present. */
        if (has_default) {
            int base = 1 + default_idx * 3;
            AST_t *body = e->children[base + 2];
            EMIT_CHUNK_OF(body);
        }

        /* 4. Dispatch. */
        sm_emit_ii(p, SM_BB_PUMP_CASE, (int64_t)ncases, (int64_t)has_default);

        #undef EMIT_CHUNK_OF
        return;
    }

    default:
        if (!g_expression_body_lowering)
            fprintf(stderr, "sm_lower: unhandled expr kind %d\n", (int)e->kind);
        sm_emit(p, SM_PUSH_NULL);
        return;
    }
}

/* ── Statement lowering ─────────────────────────────────────────────────── */

static void lower_stmt(SM_Program *p, LabelTable *lt, const STMT_t *s)
{
    /* SN-32a-blank: blank source line (no label/subject/pattern/replacement/
     * goto).  Per the Green Book and SPITBOL's `stmgo` SIL: blank lines are
     * empty stmts that bump &STNO but not &STCOUNT, and do not fire LABEL
     * on the wire.  IR mirrors this in execute_program (interp.c).  SM
     * achieves the same by emitting nothing — the next non-blank stmt's
     * SM_STNO will fire with its own (later) source stno, which is
     * exactly what an observer of &STNO would see.  Skipping the
     * lowering also prevents `kw_stcount` from being bumped via
     * comm_stno on what is by definition a non-executing line. */
    if (!s->is_end
        && (!s->label || !s->label[0])
        && !s->subject && !s->pattern && !s->replacement
        && !s->goto_u && !s->goto_u_expr && !s->goto_s && !s->goto_s_expr && !s->goto_f && !s->goto_f_expr) {
        return;
    }

    /* 0. Define label BEFORE SM_STNO so backward branches land on the STNO.
     * If SM_LABEL came after SM_STNO, a JUMP_S/JUMP_F targeting this label
     * would skip the STNO on re-entry — causing sm_steps_done to under-count
     * loop iterations and diverge from the IR step counter. (SN-26c-stmt153) */
    if (s->label && s->label[0]) {
        int lbl_idx = sm_label_named(p, s->label);
        lt_define(lt, s->label, lbl_idx);
        /* ME-7: tag SM_LABEL with `a[2].i = 1` when this label is the entry
         * point of a DEFINE'd user function.  prescan_defines() (called from
         * sm_preamble before sm_lower) has already populated the function
         * table, so FUNC_IS_ENTRY_LABEL is authoritative here.  Mode-3 codegen
         * (ME-6) reads this flag to emit `push rbp; mov rbp, rsp` prologue for
         * DEFINE'd-function entries and not for ordinary :S(label) targets. */
        if (FUNC_IS_ENTRY_LABEL(s->label)) {
            p->instrs[p->count - 1].a[2].i = 1;
        }
    }

    /* 1. Statement counter tick — increments &STCOUNT / &STNO.
     * SN-32a-stno: pass source stno as operand so sm_interp / sm_codegen
     * can report `&STNO` correctly after backward gotos and label-skips.
     * Mirrors the IR-side fix in SN-26-bridge-coverage-j (interp.c reads
     * s->stno instead of incrementing a linear counter).
     * EM-4-readability: also carry s->lineno in a[1].i so the mode-4
     * emitter can map each statement boundary back to its source line and
     * print verbatim source text in a banner.  sm_interp.c reads only
     * a[0].i (stno); a[1].i is purely informational and ignored there. */
    sm_emit_ii(p, SM_STNO, (int64_t)s->stno, (int64_t)s->lineno);

    /* END statement → SM_HALT */
    if (s->is_end) {
        sm_emit(p, SM_HALT);
        return;
    }

    /* OE-9: language-aware dispatch — ICN and PL stmts use BB opcodes.
     * LANG_SNO (0) falls through to the existing SNOBOL4 lowering path. */
    if (s->lang == LANG_ICN) {
        /* RS-26b: Icon proc/global/record defs are registered in proc_table by
         * polyglot_init() inside sm_preamble() before sm_lower() runs.  There is
         * nothing to emit per-def: the BB engine reaches procs via coro_call, not
         * by SM_BB_PUMPing the raw proc AST_FNC node.  sm_lower() synthesises a
         * single SM_PUSH_EXPR + SM_BB_PUMP for the main() call after the stmt loop.
         * This branch should now be unreachable; kept as a safety fallback. */
        return;
    }
    if (s->lang == LANG_PL) {
        /* CH-17f: Prolog statement — emit SM_BB_ONCE_PROC "name/arity", arity
         * instead of the legacy lower_expr(AST_CHOICE) + SM_BB_ONCE path that
         * pushed a raw AST_t* and called coro_eval(AST_CHOICE) at runtime.
         * s->subject is always an AST_CHOICE node whose sval = "name/arity". */
        if (s->subject && s->subject->kind == AST_CHOICE && s->subject->sval) {
            const char *key = s->subject->sval;
            int arity = 0;
            const char *sl = strrchr(key, '/');
            if (sl) arity = atoi(sl + 1);
            sm_emit_si(p, SM_BB_ONCE_PROC, key, (int64_t)arity);
        } else {
            /* Fallback for non-AST_CHOICE subjects (directives etc.) */
            if (s->subject)
                lower_expr(p, lt, s->subject);
            else
                sm_emit(p, SM_PUSH_NULL);
            sm_emit(p, SM_BB_ONCE);
        }
        goto emit_gotos;
    }

    /*
     * 2. Pattern match statement:
     *      subject  pattern  [= replacement]  :(goto)
     *
     * SM layout (Option A — pattern tree emitted FIRST):
     *   lower pattern tree → SM_PAT_* sequence
     *     (parameterised ops like SM_PAT_LEN pop their args from value stack;
     *      those args are pushed by lower_pat_expr via lower_expr internally,
     *      so they are consumed before subject is on the stack)
     *   eval subject → value stack
     *   eval replacement → value stack  (or INTVAL(0) if no replacement)
     *   SM_EXEC_STMT  (value stack top: repl; below: subj; pat-stack: built pat)
     *
     * This avoids interleaving parameterised-pattern value-stack args
     * with the subject descriptor.
     */
    if (s->pattern) {
        lower_pat_expr(p, lt, s->pattern);

        if (s->subject)
            lower_expr(p, lt, s->subject);
        else
            sm_emit(p, SM_PUSH_NULL);

        if (s->has_eq && s->replacement)
            lower_expr(p, lt, s->replacement);
        else if (s->has_eq)
            sm_emit_si(p, SM_PUSH_LIT_S, "", 0);  /* X pat = with no RHS → empty string replacement */
        else
            sm_emit_i(p, SM_PUSH_LIT_I, 0);       /* no = at all → no replacement */

        /* a[0].s = subject variable name for write-back (NULL if not a simple var);
         * a[1].i = has_eq flag.
         * Use sm_emit_si so a[0].s is strdup'd — pure-SNO programs free the IR
         * (code_free in scrip_sm.c) immediately after sm_lower returns, which would
         * leave a dangling pointer if we stored s->subject->sval directly. */
        {
            const char *sname = NULL;
            if (s->subject && (s->subject->kind == AST_VAR || s->subject->kind == AST_KEYWORD))
                sname = s->subject->sval;
            sm_emit_si(p, SM_EXEC_STMT, sname, (int64_t)s->has_eq);
        }
        goto emit_gotos;
    }

    /*
     * 3. Pure assignment / expression statement:
     *      label:  expr = value   :(goto)
     *      or just:  expr         :(goto)
     */
    if (s->subject) {
        if (s->has_eq) {
            /* Assignment: rhs is replacement (or null if omitted) */
            if (s->replacement)
                lower_expr(p, lt, s->replacement);
            else
                sm_emit(p, SM_PUSH_NULL);   /* X =   → assign null */
            /* lhs */
            if (s->subject->kind == AST_VAR || s->subject->kind == AST_KEYWORD) {
                sm_emit_s(p, SM_STORE_VAR, s->subject->sval ? s->subject->sval : "");
            } else if (s->subject->kind == AST_INDIRECT) {
                lower_expr(p, lt, s->subject->nchildren > 0 ? s->subject->children[0] : NULL);
                sm_emit_si(p, SM_CALL_FN, "ASGN_INDIR", 2);
            } else if (s->subject->kind == AST_IDX) {
                /* a<i> = rhs  or  a<i,j> = rhs — stack: rhs already pushed above.
                 * Push base, then indices; sm_interp IDX_SET pops all and calls subscript_set. */
                int nc = s->subject->nchildren;  /* child[0]=base, child[1]=i, [2]=j */
                for (int ci = 0; ci < nc; ci++) lower_expr(p, lt, s->subject->children[ci]);
                sm_emit_si(p, SM_CALL_FN, "IDX_SET", (int64_t)(nc + 1)); /* +1 for rhs */
            } else if (s->subject->kind == AST_FNC && s->subject->sval) {
                /* NRETURN lvalue or field mutator: fname(...) = rhs
                 * If fname is a zero-param user function (NRETURN path), emit
                 * NRETURN_ASGN pseudo-call: stack = [rhs], fname in a[0].s.
                 * Otherwise field mutator: push obj, call fname_SET(rhs, obj). */
                if (s->subject->nchildren == 0) {
                    /* Zero-arg call on LHS: NRETURN path (forward-declared fns allowed).
                     * NRETURN_ASGN calls fn at runtime; if it returns DT_N writes through
                     * the name, else falls back to fname_SET field-mutator convention. */
                    sm_emit_si(p, SM_CALL_FN, "NRETURN_ASGN", 1);
                    p->instrs[p->count - 1].a[1].s = GC_strdup(s->subject->sval);
                } else {
                    /* Multi-arg LHS: field mutator fname(obj,...) = rhs.
                     * Special case: ITEM(arr, i [,j]) = rhs — push all args, call ITEM_SET. */
                    if (strcasecmp(s->subject->sval, "ITEM") == 0) {
                        int nc = s->subject->nchildren;
                        for (int ci = 0; ci < nc; ci++)
                            lower_expr(p, lt, s->subject->children[ci]);
                        sm_emit_si(p, SM_CALL_FN, "ITEM_SET", (int64_t)(nc + 1)); /* +1 for rhs */
                    } else {
                        lower_expr(p, lt, s->subject->nchildren > 0 ? s->subject->children[0] : NULL);
                        char _setname[256];
                        snprintf(_setname, sizeof(_setname), "%s_SET", s->subject->sval);
                        sm_emit_si(p, SM_CALL_FN, _setname, 2);
                    }
                }
            } else {
                lower_expr(p, lt, s->subject);
                sm_emit_si(p, SM_CALL_FN, "ASGN", 2);
            }
        } else {
            /* Bare expression statement, result unused.
             *
             * SNOBOL4 special-case: a statement whose entire body is the bare
             * keyword RETURN / FRETURN / NRETURN (no `=`, no pattern, no goto)
             * is equivalent to `:(RETURN)` — it should fire the corresponding
             * return opcode, not push-then-pop a variable named "RETURN".
             *
             * This mirrors emit_goto's treatment of those names as goto
             * targets (lines ~222-238 in this file) and unblocks the
             * `define` smoke test under --sm-run / --jit-run: the smoke
             * source ends its DOUBLE body with bare `RETURN` on its own
             * line, which previously lowered as
             *   SM_PUSH_VAR s="RETURN"; SM_VOID_POP
             * leaving the function with no way to return to caller.
             */
            if (s->subject->kind == AST_VAR && s->subject->sval) {
                if (strcasecmp(s->subject->sval, "RETURN") == 0) {
                    sm_emit(p, SM_RETURN);
                    goto emit_gotos;
                }
                if (strcasecmp(s->subject->sval, "FRETURN") == 0) {
                    sm_emit(p, SM_FRETURN);
                    goto emit_gotos;
                }
                if (strcasecmp(s->subject->sval, "NRETURN") == 0) {
                    sm_emit(p, SM_NRETURN);
                    goto emit_gotos;
                }
            }
            lower_expr(p, lt, s->subject);
            sm_emit(p, SM_VOID_POP);  /* expression statement, result unused */
        }
    }

emit_gotos: {
    /* RS-1: goto fields now flat in STMT_t */
    if (!s->goto_u && !s->goto_u_expr && !s->goto_s && !s->goto_s_expr && !s->goto_f && !s->goto_f_expr) return;

    if (s->goto_u && s->goto_u[0]) {
        emit_goto(p, lt, SM_JUMP, s->goto_u);
        return;
    }

    /* Computed gotos → SM_JUMP_INDIR (not yet supported; fall back) */
    if (s->goto_u_expr) {
        sm_emit_s(p, SM_PUSH_LIT_S, "(computed-goto)");
        sm_emit(p, SM_JUMP_INDIR);
        return;
    }

    if (s->goto_s && s->goto_s[0])
        emit_goto(p, lt, SM_JUMP_S, s->goto_s);
    if (s->goto_f && s->goto_f[0])
        emit_goto(p, lt, SM_JUMP_F, s->goto_f);
    }
}

/* ── Public entry point ─────────────────────────────────────────────────── */

SM_Program *sm_lower(const CODE_t *prog)
{
    if (!prog) return NULL;

    SM_Program *p  = sm_prog_new();
    LabelTable  lt;
    lt_init(&lt);

    /* CH-17b: emit named-expression skeletons for every Icon/Raku proc.
     * CH-17b': fill the expressions with lowered body SM ops.
     *
     * Expression shape (one per proc in proc_table):
     *
     *   SM_JUMP <skip_proc_NN>     ; forward-jump around the expression
     *   SM_LABEL "<proc_name>"     ; named entry — sm_label_pc_lookup target
     *   <lowered body>             ; CH-17b': lower_expr each body child + SM_VOID_POP
     *   SM_RETURN                  ; trailing return (or unreachable after AST_RETURN)
     *   <skip_proc_NN>:            ; anonymous skip target
     *
     * After sm_lower returns, sm_resolve_proc_entry_pcs (CH-17a) walks
     * proc_table and sets entry_pc to the SM_LABEL's pc via
     * sm_label_pc_lookup.  The expressions are unreachable today
     * (coro_call still walks IR) but the entry_pcs validate end-to-end.
     *
     * CH-17b' body-lowering details:
     *
     *   - Body children of AST_FNC live at proc->children[1+nparams..nchildren-1].
     *     proc->ival is nparams.  Param-name children at [1..1+nparams-1] are
     *     not body — they declare parameters (slot-bound at runtime by
     *     icn_scope_patch in coro_call); skipped here because lowering them
     *     would emit SM_PUSH_VAR <param-name> for no benefit, the parameter
     *     values having already been bound by the caller's frame.
     *
     *   - AST_GLOBAL declarations (and AST_GLOBAL with ival==1 for `static`) are
     *     not skipped here: lower_expr handles them as no-ops emitting
     *     SM_PUSH_NULL, which the trailing SM_VOID_POP discards.  Static-variable
     *     persistence semantics still live inside coro_call (lines 408-420 of
     *     coro_runtime.c) and remain unchanged — that's CH-17g cleanup.
     *
     *   - Each body-child expr is value-context-lowered via lower_expr; we
     *     emit SM_VOID_POP after each to discard the result, mirroring how
     *     statement-context evaluation discards the value of an expression
     *     statement.  The kinds that appear at proc-body top level
     *     (AST_IF, AST_WHILE, AST_UNTIL, AST_REPEAT, AST_RETURN, AST_PROC_FAIL,
     *     AST_LOOP_BREAK, AST_LOOP_NEXT, AST_ASSIGN, AST_FNC, AST_SEQ_EXPR, AST_GLOBAL,
     *     AST_INITIAL, ...) all have lower_expr cases with single-value-push
     *     stack discipline.  Generator kinds (AST_EVERY, AST_SUSPEND,
     *     AST_BANG_BINARY, AST_TO, AST_TO_BY, ...) currently emit
     *     SM_PUSH_EXPR + SM_BB_PUMP — that's the gating note in
     *     GOAL-CHUNKS-STEP17.md CH-17b': those legacy emissions are
     *     acceptable because the expressions are unreachable until CH-17c flips
     *     coro_call to dispatch via entry_pc.  Unhandled kinds (AST_ALTERNATE,
     *     AST_ITERATE, AST_CSET_*, AST_REVASSIGN, AST_REVSWAP) hit lower_expr's
     *     default case which would normally fprintf to stderr — silenced
     *     here via g_expression_body_lowering since these expressions are dead code.
     *     AST_RETURN's lower_expr case already emits SM_RETURN, after which
     *     our trailing SM_VOID_POP + SM_RETURN is dead code — harmless.
     *
     *   - The shared LabelTable `lt` is reused.  Icon proc bodies do not use
     *     SNOBOL4-style stmt labels, so collisions are not a concern in
     *     practice; if any did appear they'd be statement-scoped within the
     *     expression and resolve via the same forward-patch machinery.
     *
     *   - CH-17b'': frame-slot resolution IS pre-built at lower-time.  Per
     *     proc, an IcnScope is constructed mirroring coro_runtime.c's
     *     icn_scope_patch (params first, then AST_GLOBAL-decl names, then any
     *     non-global AST_VAR encountered in the body).  The scope is installed
     *     via g_expression_scope so lower_expr's AST_VAR / AST_ASSIGN cases emit
     *     SM_LOAD_FRAME / SM_STORE_FRAME for in-scope names, falling back to
     *     SM_PUSH_VAR / SM_STORE_VAR for globals / keywords / unscoped names.
     *     This mirrors what bb_eval_value does at runtime when frame_depth>0,
     *     but bakes the slot decisions at lower-time.  IR is NOT mutated:
     *     scope_add only writes into the side table.  Slot order matches
     *     icn_scope_patch's order, so when CH-17c flips coro_call to dispatch
     *     via sm_call_proc, args[i] populates FRAME.env[i] at the same slot
     *     that the expression's SM_LOAD_FRAME reads from.
     *
     *   - Empty-body skeleton (and the expressions themselves) remain safe:
     *     forward-jumped over by the SM_JUMP; even if a future caller
     *     invoked one before CH-17c, it would just SM_RETURN. */
    for (int pi = 0; pi < proc_count; pi++) {
        const char *nm = proc_table[pi].name;
        if (!nm || !*nm) continue;
        AST_t *proc = proc_table[pi].proc;       /* registered by polyglot_init */

        int skip_jump = sm_emit_i(p, SM_JUMP, 0); /* patched to skip_lbl below */
        /* sm_label_named records the name in a[0].s and the pc in a[1].i
         * — making it findable via sm_label_pc_lookup(p, name). */
        sm_label_named(p, nm);

        /* CH-17b' + CH-17b'': lower each body child as a value expression and
         * pop the result.  body_start = 1 + nparams.  Defensive on missing IR.
         *
         *   g_expression_body_lowering — silences lower_expr's "unhandled expr kind"
         *   stderr warning for the duration; expressions are dead code today so the
         *   warning would only mislead.  Also gates the new frame-slot
         *   emission below on expression-body context.
         *
         *   g_expression_scope (CH-17b'') — per-proc IcnScope built fresh: params
         *   become slots 0..nparams-1; AST_GLOBAL-decl locals follow; any
         *   non-global AST_VAR encountered in the body extends the scope.
         *   The walker mirrors icn_scope_patch but without IR mutation. */
        if (proc) {
            int nparams    = (int)proc->ival;
            int body_start = 1 + nparams;

            /* Build per-proc scope.  Stored on the stack — automatically torn
             * down when this `if` block exits.  scope_add returns the same
             * slot for the same name, so re-adding params via the body walk
             * is safe.  Order matches icn_scope_patch (params, then E_GLOBALs
             * via the AST_GLOBAL branch of expression_scope_walk, then encountered
             * E_VARs in tree-walk order). */
            IcnScope expression_sc; expression_sc.n = 0;
            for (int i = 0; i < nparams && i < FRAME_SLOT_MAX; i++) {
                AST_t *pn = proc->children[1+i];
                if (pn && pn->sval) scope_add(&expression_sc, pn->sval);
            }
            for (int bi = body_start; bi < proc->nchildren; bi++)
                expression_scope_walk(&expression_sc, proc->children[bi]);

            g_expression_scope          = &expression_sc;
            g_expression_body_lowering  = 1;
            for (int bi = body_start; bi < proc->nchildren; bi++) {
                AST_t *body_expr = proc->children[bi];
                if (!body_expr) continue;
                lower_expr(p, &lt, body_expr);
                sm_emit(p, SM_VOID_POP);
            }
            g_expression_body_lowering  = 0;
            g_expression_scope          = NULL;
        }

        sm_emit(p, SM_RETURN);
        int skip_lbl = sm_label(p);               /* anonymous skip target */
        sm_patch_jump(p, skip_jump, skip_lbl);
    }

    /* CH-17d: emit named-expression skeletons for every Prolog predicate.
     *
     * Symmetrical to the Icon/Raku proc-expression loop above, but driven by
     * g_pl_pred_table instead of proc_table.
     *
     * Expression shape (one per predicate, keyed by "name/arity"):
     *
     *   SM_JUMP <skip_pred_NN>     ; forward-jump around the expression
     *   SM_LABEL "name/arity"      ; named entry — sm_label_pc_lookup target
     *   SM_RETURN                  ; skeleton body — CH-17f will fill this in
     *   <skip_pred_NN>:            ; anonymous skip target
     *
     * Producer fires; consumer is dormant.  sm_resolve_proc_entry_pcs
     * (CH-17a) walks g_pl_pred_table after sm_lower returns and populates
     * entry_pc via sm_label_pc_lookup — previously all Prolog entry_pcs
     * stayed -1.  The expressions are forward-jumped over so nothing reaches
     * them at runtime; gates are byte-identical to baseline.
     *
     * Body lowering (full AST_CHOICE/AST_CLAUSE IR walk) is CH-17f territory.
     * This rung only validates that the resolver finds non-(-1) entry_pcs. */
    for (int b = 0; b < PL_PRED_TABLE_SIZE_FWD; b++) {
        for (Pl_PredEntry *e = g_pl_pred_table.buckets[b]; e; e = e->next) {
            if (!e->key || !*e->key) continue;
            int skip_jump_pl = sm_emit_i(p, SM_JUMP, 0); /* patched below */
            sm_label_named(p, e->key);                    /* named entry point */
            sm_emit(p, SM_RETURN);                        /* skeleton — CH-17f fills body */
            int skip_lbl_pl  = sm_label(p);
            sm_patch_jump(p, skip_jump_pl, skip_lbl_pl);
        }
    }

    /* First pass: lower all statements */
    int stno = 0;
    int has_icn = 0;   /* RS-26b: set when any LANG_ICN stmt is seen */
    for (const STMT_t *s = prog->head; s; s = s->next) {
        if (s->lang == LANG_ICN) {
            /* RS-26b: Icon proc/global/record defs are registered by polyglot_init.
             * Skip emitting per-def instructions; synthesise a single main() call below. */
            has_icn = 1;
            sm_stno_label_record(p, ++stno, NULL);
            continue;
        }
        lower_stmt(p, &lt, s);
        /* IM-9: record source label for this statement (1-based, matches IR stno) */
        sm_stno_label_record(p, ++stno, (s->label && s->label[0]) ? s->label : NULL);
    }

    /* CHUNKS-step12: synthesise top-level main() pump for Icon programs.
     * Was: build a synthetic AST_FNC("main") node, emit_push_expr it, follow
     *      with SM_BB_PUMP — which read DT_E.ptr as AST_t* at runtime and
     *      handed it to coro_eval, an IR walker.
     * Now: emit SM_BB_PUMP_PROC "main", 0 directly. The handler does the
     *      proc_table lookup and stages the coroutine without constructing
     *      or walking any AST_t at this layer. The IR walk that remains
     *      lives entirely inside coro_call(proc_table[main].proc, ...) and
     *      is Step 17's territory (proc_table → entry_pcs).
     *
     * Generator-orthogonal: works whether or not main's body uses
     *   generators, because main's body execution path (coro_call → IR
     *   walk) is unchanged — only the wrapper-level synthesis is migrated. */
    if (has_icn) {
        sm_emit_si(p, SM_BB_PUMP_PROC, "main", 0);
    }

    /* Implicit HALT at end if not already there */
    if (p->count == 0 || p->instrs[p->count - 1].op != SM_HALT)
        sm_emit(p, SM_HALT);

    /* Second pass: resolve all forward label references */
    /* Unresolved labels are patched to HALT (Error 24) by lt_resolve */
    lt_resolve(&lt, p);

    lt_free(&lt);
    return p;
}
