/*============================================================================================================================
 * icon_gen.h — Icon Value-Generator Byrd Box Types (GOAL-ICN-BROKER B-1, GOAL-UNIFIED-BROKER U-7)
 *
 * U-7: icn_box_fn → bb_box_fn; icn_gen_t → bb_node_t (unified with SNOBOL4 / Prolog boxes).
 *
 * All Icon generator boxes share the universal Byrd box signature:
 *   DESCR_t (*bb_box_fn)(void *zeta, int entry)
 *
 * Same four-signal protocol:
 *   entry == α (0)  fresh entry    — initialise state, produce first value (γ) or fail (ω)
 *   entry == β (1)  backtrack      — advance state, produce next value (γ) or fail (ω)
 *   return IS_FAIL_fn(result)  →  ω fired (exhausted)
 *   return !IS_FAIL_fn(result) →  γ fired (value = result)
 *============================================================================================================================*/

#ifndef ICON_GEN_H
#define ICON_GEN_H

#include <stdlib.h>
#include <string.h>
#include "../../runtime/x86/bb_broker.h"   /* bb_box_fn, bb_node_t, BrokerMode, bb_broker, DESCR_t, FAILDESCR, IS_FAIL_fn, α/β */
#include "../../runtime/x86/snobol4.h"     /* TBBLK_t, TBPAIR_t, table_new, table_get, table_set, TABLE_BUCKETS */

/*----------------------------------------------------------------------------------------------------------------------------
 * ICN_FAIL_GEN — a generator that immediately fires ω.  Used as a sentinel / no-op.
 * Uses bb_node_t (U-7: was icn_gen_t).
 *--------------------------------------------------------------------------------------------------------------------------*/
static DESCR_t icn_fail_box(void *zeta, int entry) { (void)zeta; (void)entry; return FAILDESCR; }
static const bb_node_t ICN_FAIL_GEN = { icn_fail_box, NULL, 0 };

/*----------------------------------------------------------------------------------------------------------------------------
 * icn_gen_enter — allocate (or reuse) per-invocation state, matching bb_enter() pattern.
 *--------------------------------------------------------------------------------------------------------------------------*/
static inline void *icn_gen_enter(void **pp, size_t size) {
    void *p = *pp;
    if (size) {
        if (p) memset(p, 0, size);
        else   p = *pp = calloc(1, size);
    }
    return p;
}
#define ICN_ENTER(ref, T)  ((T *)icn_gen_enter((void **)(ref), sizeof(T)))

/*----------------------------------------------------------------------------------------------------------------------------
 * Box state types — allocated by coro_eval (in scrip.c) and passed as zeta
 *--------------------------------------------------------------------------------------------------------------------------*/
typedef struct { long lo; long hi; long cur; }                                        icn_to_state_t;
/*----------------------------------------------------------------------------------------------------------------------------
 * icn_to_nested_state_t — state for (lo_gen) to (hi_gen) cross-product box
 * Pre-collects all lo/hi values, then iterates lo × hi × inner.
 *--------------------------------------------------------------------------------------------------------------------------*/
#define ICN_TO_NESTED_MAX 256
typedef struct {
    long lo_vals[ICN_TO_NESTED_MAX];
    long hi_vals[ICN_TO_NESTED_MAX];
    int  nlo, nhi;
    int  li, hi2;   /* outer lo/hi pair indices */
    long cur;       /* current value in inner lo..hi range */
} icn_to_nested_state_t;
DESCR_t icn_bb_to_nested(void *zeta, int entry);
typedef struct { long lo; long hi; long step; long cur; }                             icn_to_by_state_t;
typedef struct { double lo; double hi; double step; double cur; }                    icn_to_by_real_state_t;
DESCR_t icn_bb_to_by_real(void *zeta, int entry);
typedef struct { const char *str; long len; long pos; char ch[2]; }                  icn_iterate_state_t;
typedef struct { TBBLK_t *tbl; int bucket; TBPAIR_t *entry; }                        icn_tbl_iterate_state_t;
typedef struct { TBBLK_t *tbl; int bucket; TBPAIR_t *entry; }                        icn_tbl_key_iterate_state_t;
typedef struct { DESCR_t list_obj; int pos; }                                         icn_list_iterate_state_t;
DESCR_t icn_bb_list_iterate(void *zeta, int entry);
DESCR_t icn_bb_tbl_key_iterate(void *zeta, int entry);
/* IC-9 (2026-05-01): !record — yield each field value of a DT_DATA record (non-icnlist).
 *   inst is the live DATINST_t descriptor; pos walks 0..type->nfields. */
typedef struct { DESCR_t inst; int pos; }                                             icn_record_iterate_state_t;
DESCR_t icn_bb_record_iterate(void *zeta, int entry);
typedef struct { const char *needle; const char *hay; int nlen; const char *next; }  icn_find_state_t;
/* IC-9 (2026-05-02): find/upto with generative subject — drive subject gen, exhaust positions per subject */
typedef struct {
    bb_node_t   subj_gen;   /* generator of subject strings */
    const char *needle;
    int         nlen;
    const char *hay;        /* current subject */
    const char *next;       /* scan cursor in current hay */
    int         subj_entry; /* entry for next subj_gen pump (α on first, β after) */
} icn_find_gen_subj_t;
DESCR_t icn_bb_find_subj(void *zeta, int entry);
typedef struct {
    bb_node_t   subj_gen;
    const char *cset;       /* set of chars for upto */
    const char *hay;
    int         slen;
    int         pos;        /* 0-based scan position */
    int         subj_entry;
} icn_upto_gen_subj_t;
DESCR_t icn_bb_upto_subj(void *zeta, int entry);

/*----------------------------------------------------------------------------------------------------------------------------
 * Box function declarations — implemented in icon_gen.c
 *--------------------------------------------------------------------------------------------------------------------------*/
DESCR_t icn_bb_to(void *zeta, int entry);
DESCR_t icn_bb_to_by(void *zeta, int entry);
DESCR_t icn_bb_iterate(void *zeta, int entry);
DESCR_t icn_bb_tbl_iterate(void *zeta, int entry);
DESCR_t icn_bb_find(void *zeta, int entry);
typedef struct { const char *s; const char *c1; const char *c2; const char *c3; int slen; int pos; int endp; } icn_bal_state_t;
DESCR_t icn_bb_bal(void *zeta, int entry);
DESCR_t icn_bb_binop(void *zeta, int entry);
DESCR_t icn_bb_alternate(void *zeta, int entry);

/*----------------------------------------------------------------------------------------------------------------------------
 * icn_bb_binop — generative binary operator box (IC-2a)
 *
 * Handles arithmetic and relational ops where one or both operands are generators.
 * JCON irgen.icn §4.3: funcs-set ops — right.failure → left.resume (goal-directed retry).
 * Relational ops (LT/GT/LE/GE/EQ/NE): on comparison failure → resume right (not left).
 *--------------------------------------------------------------------------------------------------------------------------*/
typedef enum {
    ICN_BINOP_ADD, ICN_BINOP_SUB, ICN_BINOP_MUL, ICN_BINOP_DIV, ICN_BINOP_MOD,
    ICN_BINOP_LT, ICN_BINOP_LE, ICN_BINOP_GT, ICN_BINOP_GE, ICN_BINOP_EQ, ICN_BINOP_NE,
    ICN_BINOP_CONCAT,  /* || string concatenation */
} IcnBinopKind;

typedef struct {
    bb_node_t    left;          /* left operand generator (may be oneshot) */
    bb_node_t    right;         /* right operand generator (may be oneshot) */
    IcnBinopKind op;
    int          is_relop;      /* 1 = relational: failure retries right, not left */
    DESCR_t      left_val;      /* current left value */
    DESCR_t      right_val;     /* current right value */
    int          phase;         /* 0=need left, 1=need right, 2=have both */
} icn_binop_gen_state_t;

/*----------------------------------------------------------------------------------------------------------------------------
 * icn_bb_alternate — TT_ALTERNATE Byrd box (IC-2a)
 *
 * JCON irgen.icn ir_a_Alt: try left until ω, then switch to right.
 * Binary variant: which=0 → pumping left; which=1 → pumping right.
 *--------------------------------------------------------------------------------------------------------------------------*/
typedef struct {
    bb_node_t gen[2];
    int       which;   /* 0 = left active, 1 = right active */
} icn_alternate_state_t;

/*----------------------------------------------------------------------------------------------------------------------------
 * Forward declaration of tree_t needed by IC-2b state structs below.
 * When ir.h is already included this is a harmless redundant typedef.
 *--------------------------------------------------------------------------------------------------------------------------*/
#ifndef EXPR_T_DEFINED
#define EXPR_T_DEFINED
typedef struct tree_t tree_t;
#endif

/*----------------------------------------------------------------------------------------------------------------------------
 * icn_bb_limit — TT_LIMIT Byrd box  (gen \ N)   IC-2b
 *
 * Drives inner generator, yields each value, stops after N ticks.
 * State: gen (inner box), max (limit count), count (ticks so far).
 *--------------------------------------------------------------------------------------------------------------------------*/
typedef struct {
    bb_node_t gen;
    long      max;
    long      count;
} icn_limit_state_t;
DESCR_t icn_bb_limit(void *zeta, int entry);
DESCR_t icn_bb_cset_compl(void *zeta, int entry);

/*----------------------------------------------------------------------------------------------------------------------------
 * icn_bb_scan_gen -- TT_SCAN with generative subject  (gen ? body)  IJ-7
 *
 * Pumps subject generator; for each subject value runs the scan body.
 * Yields body result per successful scan; tries next subject on body failure.
 * State: subject gen box, body tree_t*.
 *--------------------------------------------------------------------------------------------------------------------------*/
typedef struct {
    bb_node_t  subj_gen;      /* subject alternation/generator */
    tree_t    *body;          /* scan body (c[1]) */
    int        started;
    /* IJ-9: body generator state — valid when body_live=1 */
    bb_node_t  body_gen;      /* generator built from body via coro_eval */
    int        body_live;     /* 1 = body_gen has been started and may still produce */
    /* body_subj/body_pos: the scan context for THIS subject's body run.
     * body_pos advances as the body consumes characters; restored between β ticks. */
    const char *body_subj;    /* subject string installed for this body run */
    int         body_pos;     /* latest scan_pos seen from body (advances on each tick) */
} icn_scan_gen_state_t;
DESCR_t icn_bb_scan_gen(void *zeta, int entry);

/*----------------------------------------------------------------------------------------------------------------------------
 * icn_bb_every — TT_EVERY Byrd box  (every gen [do body])   IC-2b
 *
 * Drives inner generator to exhaustion; evaluates body tree_t* per tick.
 * body may be NULL (bare "every gen" — just drives gen to exhaustion for side effects).
 * Yields body result (or gen value if no body) per tick.
 * State: gen (inner box), body (tree_t*), started flag.
 *--------------------------------------------------------------------------------------------------------------------------*/
typedef struct {
    bb_node_t  gen;
    tree_t    *gen_ast; /* AST node of generator child — for coro_drive_node injection */
    tree_t    *body;   /* may be NULL */
    int        started;
} icn_every_state_t;
DESCR_t icn_bb_every(void *zeta, int entry);

/*----------------------------------------------------------------------------------------------------------------------------
 * icn_bb_mutual — TT_SEQ (Icon A & B) when B is also generative   IJ-12
 *
 * Implements JCON ir_a_Mutual semantics: A is the outer generator, B is the
 * inner generator.  A produces one value; B is driven to exhaustion; then A
 * advances.  On β: resume B first; if B exhausted, advance A and restart B.
 *
 * State: gen_a / gen_b (boxes), ast_a / ast_b (for fresh B construction).
 *--------------------------------------------------------------------------------------------------------------------------*/
typedef struct {
    bb_node_t  gen_a;       /* outer generator (A) */
    bb_node_t  gen_b;       /* inner generator (B) — rebuilt from ast_b each A tick */
    tree_t    *ast_b;       /* B AST — used to build a fresh gen_b when A advances */
    int        b_started;   /* has gen_b been started on the current A tick? */
    int        a_started;   /* has gen_a been started? */
} icn_mutual_state_t;
DESCR_t icn_bb_mutual(void *zeta, int entry);

/*----------------------------------------------------------------------------------------------------------------------------
 * icn_bb_bang_binary — TT_BANG_BINARY Byrd box  (E1 ! E2)   IC-2b
 *
 * Invoke E1 (a procedure tree_t*) with successive values from E2 (a generator).
 * E1 is re-evaluated for each value produced by E2.
 * State: proc_expr (TT_FNC tree_t*), arg_box (generator for E2), current arg DESCR_t.
 *--------------------------------------------------------------------------------------------------------------------------*/
typedef struct {
    tree_t   *proc_expr;   /* E1 — the callable */
    bb_node_t arg_box;     /* E2 — the argument generator */
    DESCR_t   cur_arg;     /* current argument value */
} icn_bang_binary_state_t;
DESCR_t icn_bb_bang_binary(void *zeta, int entry);

/*----------------------------------------------------------------------------------------------------------------------------
 * icn_bb_seq_expr — TT_SEQ_EXPR Byrd box  (E1; E2; …; En)   IC-2b
 *
 * Evaluates each child in order; result = last child.
 * If last child is a generator, its values are forwarded.
 * State: children array, count, last-child box.
 *--------------------------------------------------------------------------------------------------------------------------*/
typedef struct {
    tree_t   **children;   /* pointer into the TT_SEQ_EXPR tree_t children array */
    int        n;
    bb_node_t  last_box;   /* generator box for last child (may be oneshot) */
    int        started;
} icn_seq_state_t;
DESCR_t icn_bb_seq_expr(void *zeta, int entry);

typedef struct { bb_node_t gen; struct tree_t *cat_expr; struct tree_t *leaf; } icn_cat_gen_state_t;
DESCR_t icn_bb_cat(void *zeta, int entry);
bb_node_t coro_eval(tree_t *e);

/* RK-21: gather coroutine trampoline — defined in icn_runtime.c, referenced in icon_gen.c */
extern void gather_trampoline(void);


/*----------------------------------------------------------------------------------------------------------------------------
 * State types for all 43 JCON BBs (IJ-18..43)
 *--------------------------------------------------------------------------------------------------------------------------*/
typedef struct { tree_t *expr; int frame_popped; } icn_not_state_t;
typedef struct { tree_t *expr; int started; int ever_succeeded; bb_node_t inner; } icn_repalt_state_t;
typedef struct { tree_t *expr; tree_t *body; } icn_while_state_t;
typedef struct { tree_t *expr; tree_t *body; } icn_until_state_t;
typedef struct { tree_t *body; } icn_repeat_state_t;
typedef struct { bb_node_t obj_gen; const char *field; } icn_field_gen_state_t;
typedef struct { const char *kw; int fired; DESCR_t val; } icn_kw_gen_state_t;
typedef enum { ICN_SEC_RANGE, ICN_SEC_PLUS, ICN_SEC_MINUS } icn_sec_kind_t;
#define ICN_CASE_MAX     32
#define ICN_COMPOUND_MAX 32
#define ICN_LISTCON_MAX  64
typedef struct {
    DESCR_t disc; tree_t *clause_exprs[ICN_CASE_MAX]; tree_t *clause_bodies[ICN_CASE_MAX];
    int n_clauses; tree_t *dflt; int cur_clause; bb_node_t body_box; int body_started;
} icn_case_state_t;
typedef struct { tree_t *children[ICN_COMPOUND_MAX]; int n; bb_node_t last_box; int started; } icn_compound_state_t;
typedef struct {
    bb_node_t val_gen; bb_node_t left_gen; bb_node_t right_gen;
    tree_t *val_expr; tree_t *left_expr; tree_t *right_expr;
    icn_sec_kind_t kind; DESCR_t cur_val; DESCR_t cur_left;
    int val_started; int left_started; int right_started;
} icn_section_gen_state_t;
typedef struct { tree_t *children[ICN_LISTCON_MAX]; int n; int fired; } icn_listcon_state_t;
typedef struct {
    tree_t *proc; int body_start; int nbody; int stmt_idx;
    bb_node_t expr_box; int in_suspend; tree_t *suspend_body; int frame_popped;
} icn_proc_state_t;
typedef struct { icn_proc_state_t base; DESCR_t args[16]; int nargs; } icn_proc_call_state_t;
/* Non-generative state types */
typedef struct { int dummy; }                              icn_noop_state_t;
typedef struct { long long val; }                          icn_intlit_state_t;
typedef struct { double val; }                             icn_reallit_state_t;
typedef struct { const char *s; }                          icn_strlit_state_t;
typedef struct { const char *s; }                          icn_csetlit_state_t;
typedef struct { int dummy; }                              icn_global_state_t;
typedef struct { tree_t *cond; tree_t *then_e; tree_t *else_e; } icn_if_state_t;
typedef struct { tree_t *body; }                           icn_initial_state_t;
typedef struct { int dummy; }                              icn_invocable_state_t;
typedef struct { int dummy; }                              icn_link_state_t;
typedef struct { const char *name; int nfields; }          icn_record_state_t;
typedef struct { tree_t *expr; }                           icn_return_state_t;
typedef struct { int dummy; }                              icn_fail_state_t;
typedef struct { const char *op; tree_t *operand; }        icn_unop_state_t;
typedef struct { int dummy; }                              icn_next_state_t;
typedef struct { tree_t *expr; }                           icn_break_state_t;
typedef struct { tree_t *expr; }                           icn_create_state_t;
typedef struct { int dummy; }                              icn_coexplist_state_t;
typedef struct { int dummy; }                              icn_arglist_state_t;
typedef struct { tree_t *proc; }                           icn_procdecl_state_t;
typedef struct { tree_t *body; }                           icn_procbody_state_t;
typedef struct { tree_t *init; tree_t *body; }             icn_proccode_state_t;
#endif /* ICON_GEN_H */
