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
#include <ucontext.h>
#include "../../runtime/x86/bb_broker.h"   /* bb_box_fn, bb_node_t, BrokerMode, bb_broker, DESCR_t, FAILDESCR, IS_FAIL_fn, α/β */

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
 * Box state types — allocated by icn_eval_gen (in scrip.c) and passed as zeta
 *--------------------------------------------------------------------------------------------------------------------------*/
typedef struct { long lo; long hi; long cur; }                                        icn_to_state_t;
typedef struct { long lo; long hi; long step; long cur; }                             icn_to_by_state_t;
typedef struct { const char *str; long len; long pos; char ch[2]; }                  icn_iterate_state_t;
typedef struct { const char *needle; const char *hay; int nlen; const char *next; }  icn_find_state_t;
typedef struct {
    ucontext_t  gen_ctx;
    ucontext_t  caller_ctx;
    char       *stack;
    DESCR_t     yielded;
    int         exhausted;
    int         started;
    void      (*trampoline)(void);
    void       *trampoline_arg;
} icn_suspend_state_t;

/*----------------------------------------------------------------------------------------------------------------------------
 * Box function declarations — implemented in icon_gen.c
 *--------------------------------------------------------------------------------------------------------------------------*/
DESCR_t icn_bb_to(void *zeta, int entry);
DESCR_t icn_bb_to_by(void *zeta, int entry);
DESCR_t icn_bb_iterate(void *zeta, int entry);
DESCR_t icn_bb_suspend(void *zeta, int entry);
DESCR_t icn_bb_find(void *zeta, int entry);
DESCR_t icn_bb_binop_gen(void *zeta, int entry);
DESCR_t icn_bb_alternate(void *zeta, int entry);

/*----------------------------------------------------------------------------------------------------------------------------
 * icn_bb_binop_gen — generative binary operator box (IC-2a)
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
 * icn_bb_alternate — E_ALTERNATE Byrd box (IC-2a)
 *
 * JCON irgen.icn ir_a_Alt: try left until ω, then switch to right.
 * Binary variant: which=0 → pumping left; which=1 → pumping right.
 *--------------------------------------------------------------------------------------------------------------------------*/
typedef struct {
    bb_node_t gen[2];
    int       which;   /* 0 = left active, 1 = right active */
} icn_alternate_state_t;

/*----------------------------------------------------------------------------------------------------------------------------
 * icn_eval_gen — walk an EXPR_t tree and return a bb_node_t.
 * Declared here; implemented in scrip.c (B-8) where interp_eval and proc tables are visible.
 * Uses EXPR_t* — callers must have ir.h in scope.
 *--------------------------------------------------------------------------------------------------------------------------*/
#ifndef EXPR_T_DEFINED
#define EXPR_T_DEFINED
typedef struct EXPR_t EXPR_t;  /* minimal forward when ir.h not yet included */
#endif
bb_node_t icn_eval_gen(EXPR_t *e);

#endif /* ICON_GEN_H */
