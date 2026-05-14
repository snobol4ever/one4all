/* scrip_ir.h — Universal generator IR: IR_t (node) / IR_block_t (graph) / IR_e (LR-0)
 * AUTHORS: Lon Jones Cherryholmes · Claude Sonnet 4.6 (LR-0, 2026-05-14)
 *
 * Directed Cyclic Graph (DCG) for all goal-directed computation across all six languages.
 * Built at lower time; driven by ir_exec (ir_exec.h).  SM_EXEC_BB(IR_block_t*) is the
 * sole SM→BB entry point.  Tree-shaped subgraphs serialise to SM arrays; cyclic subgraphs
 * (patterns, generators, choice points) route through ir_exec directly.
 *
 * LR-0: infrastructure only — alloc / free / print / reset.  No lowering changes.
 * All gates pass: this header is included by scrip_ir.c only; nothing else includes it yet.
 */
#pragma once
#ifndef SCRIP_IR_H
#define SCRIP_IR_H

#include <stdint.h>
#include <stdio.h>
#include "descr.h"

/*==================================================================================================
 * Value constructors — mirrors snobol4.h macros; defined here so ir_exec.c
 * does not need to include the full snobol4.h / GC headers.
 *================================================================================================*/
#ifndef NULVCL
#  define NULVCL       ((DESCR_t){ .v = DT_SNUL, .slen = 0, .s = "" })
#endif
#ifndef INTVAL
#  define INTVAL(i_)   ((DESCR_t){ .v = DT_I, .i = (int64_t)(i_) })
#endif
#ifndef REALVAL
#  define REALVAL(r_)  ((DESCR_t){ .v = DT_R, .r = (double)(r_) })
#endif
#ifndef STRVAL
#  define STRVAL(s_)   ((DESCR_t){ .v = DT_S, .slen = 0, .s = (s_) })
#endif

/*==================================================================================================
 * Language tags — stored in IR_t.lang and IR_block_t.lang
 *================================================================================================*/
#define IR_LANG_SNO  1   /* SNOBOL4  */
#define IR_LANG_SCO  2   /* Snocone  */
#define IR_LANG_REB  3   /* Rebus    */
#define IR_LANG_ICN  4   /* Icon     */
#define IR_LANG_PL   5   /* Prolog   */
#define IR_LANG_RKU  6   /* Raku     */

/*==================================================================================================
 * IR_e — kind of every IR_t node
 *================================================================================================*/
typedef enum {
    /* ── Universal scalar ─────────────────────────────────────────────────────────────────────── */
    IR_LIT_I,           /* integer literal                                                        */
    IR_LIT_S,           /* string literal                                                         */
    IR_LIT_F,           /* real literal                                                           */
    IR_LIT_NUL,         /* &null / zero                                                           */
    IR_VAR,             /* variable read                                                          */
    IR_ASSIGN,          /* var := expr                                                            */
    IR_AUGOP,           /* var op:= expr                                                          */
    IR_BINOP,           /* l op r                                                                 */
    IR_UNOP,            /* op e                                                                   */
    IR_CALL,            /* proc(args…) — user or builtin                                          */
    IR_SEQ,             /* stmt ; stmt (sequence)                                                 */
    IR_FAIL,            /* always fails                                                           */
    IR_SUCCEED,         /* always succeeds, returns &null                                         */
    IR_GOTO,            /* unconditional jump (wired graph only)                                  */
    IR_RETURN,          /* return expr                                                            */
    /* ── Universal generators ─────────────────────────────────────────────────────────────────── */
    IR_ALTERNATE,       /* A | B                                                                  */
    IR_TO_BY,           /* i to j by k                                                            */
    IR_EVERY,           /* every expr do body                                                     */
    IR_WHILE,           /* while expr do body                                                     */
    IR_LIMIT,           /* expr \ n                                                               */
    IR_SUSPEND,         /* suspend expr                                                           */
    IR_PROC,            /* procedure root node                                                    */
    /* ── Icon / Snocone specific ──────────────────────────────────────────────────────────────── */
    IR_SCAN,            /* subj ? body                                                            */
    IR_NONNULL,         /* \expr                                                                  */
    IR_INTERROGATE,     /* /expr                                                                  */
    /* ── SNOBOL4 / Snocone / Rebus pattern ────────────────────────────────────────────────────── */
    IR_PAT_LIT,         /* literal string match                                                   */
    IR_PAT_ANY,         /* ANY(cset)                                                              */
    IR_PAT_SPAN,        /* SPAN(cset) — back-edge on fail-short                                   */
    IR_PAT_BREAK,       /* BREAK(cset)                                                            */
    IR_PAT_ARB,         /* ARB — back-edge on fail-longer                                         */
    IR_PAT_ARBNO,       /* ARBNO(p)                                                               */
    IR_PAT_CAT,         /* P1 P2 — B.fail→A.resume back-edge                                     */
    IR_PAT_ALT,         /* P1 | P2                                                                */
    IR_PAT_ASSIGN_IMM,  /* P . V                                                                  */
    IR_PAT_ASSIGN_COND, /* P $ V                                                                  */
    IR_PAT_LEN,         /* LEN(n) — match exactly n chars                                        */
    IR_PAT_NOTANY,      /* NOTANY(cset) — match one char NOT in charset                          */
    IR_PAT_POS,         /* POS(n) / RPOS(n) — ival=0 left, ival=1 right                         */
    IR_PAT_TAB,         /* TAB(n) / RTAB(n) — ival=0 left, ival=1 right                         */
    IR_PAT_REM,         /* REM                                                                    */
    IR_PAT_FENCE,       /* FENCE                                                                  */
    IR_PAT_ABORT,       /* ABORT                                                                  */
    IR_PAT_CALLOUT,     /* *Fn() deferred call                                                    */
    /* ── Prolog specific ──────────────────────────────────────────────────────────────────────── */
    IR_PL_CHOICE,       /* clause alternation (choice point)                                      */
    IR_PL_UNIFY,        /* unification                                                            */
    IR_PL_CUT,          /* !                                                                      */
    IR_PL_CALL,         /* call(Goal)                                                             */
    /* ── Icon specific ────────────────────────────────────────────────────────────────────────── */
    IR_ICN_TO,          /* i to j — integer range generator; ival=lo, ival2=hi                   */
    IR_ICN_UPTO,        /* upto(cset,str) — positions where cset char appears; sval=cset,sval2=str */
    IR_ICN_EVERY,       /* every gen [do body] — drives child bb_node_t via opaque; body in sval2 */
    IR_ICN_TO_BY,       /* i to j by k — integer range with step; ival=lo, ival2=hi, ival3=step      */
    IR_ICN_ITERATE,     /* !str — char-by-char iteration; sval2=str, ival=len, counter=pos               */
    IR_ICN_ALTERNATE,   /* A|B — opaque=icn_alt_dcg_t*{gen[2],which}; left first then right              */
    IR_ICN_LIMIT,       /* gen\N — opaque=icn_lim_dcg_t*{gen,max,count}; yield up to max ticks            */
    IR_ICN_BINOP,       /* arith/relop with generative operands; opaque=icn_binop_dcg_t*                    */
    IR_E_COUNT       /* sentinel — number of kinds                                             */
} IR_e;

/*==================================================================================================
 * IR_t — one node in the DCG
 * Four control ports are NULL until lower wires them bottom-up.
 *================================================================================================*/
typedef struct IR_t IR_t;
struct IR_t {
    IR_e           t;
    IR_t         * α;            /* entry: first evaluation attempt                             */
    IR_t         * β;            /* backtrack: try next value (NULL → scalar/non-generative)    */
    IR_t         * γ;            /* success continuation (value in .value)                      */
    IR_t         * ω;            /* failure continuation (no value)                             */
    IR_t        ** c;            /* child array (pre-wiring tree)                               */
    int            n;            /* child count                                                 */
    union {
        int64_t        ival;     /* LIT_I; op code for BINOP/UNOP                               */
        double         dval;     /* LIT_F                                                       */
        const char   * sval;     /* LIT_S, VAR name, CALL name, charset                        */
    };
    const char   * sval2;       /* second string arg (IR_ICN_UPTO hay; others: NULL) — not reset by IR_reset */
    int64_t        ival2;       /* second integer arg (IR_ICN_TO hi; others: 0) — not reset by IR_reset     */
    int64_t        ival3;       /* third integer arg (IR_ICN_TO_BY step; others: 0) — not reset by IR_reset   */
    void         * opaque;      /* opaque payload pointer — not reset by IR_reset (IR_ICN_EVERY child gen)  */
    DESCR_t        value;        /* current result value (live during ir_exec graph walk)       */
    int64_t        counter;      /* generative scratch: chars consumed, step position, etc.     */
    int            state;        /* executor state machine (0 = fresh)                          */
};

/*==================================================================================================
 * IR_block_t — a complete wired generator DCG for one procedure or pattern
 *================================================================================================*/
typedef struct {
    IR_t    * entry;        /* == root node's α                      */
    IR_t   ** all;          /* flat array of all nodes (for reset / GC / print) */
    int            n;       /* count of nodes in .all                           */
    int            lang;    /* IR_LANG_* — language that produced this graph    */
} IR_block_t;

/*==================================================================================================
 * API — alloc / free / reset / print
 *================================================================================================*/

/* Allocate a fresh IR_block_t with capacity for max_nodes nodes.
 * All node slots are NULL.  Returns NULL on OOM. */
IR_block_t * IR_alloc(int max_nodes, int lang);

/* Allocate one IR_t node, append it to cfg->all, assign cfg-unique id.
 * Returns NULL if cfg->all is full (capacity = max_nodes passed to IR_alloc). */
IR_t       * IR_node_alloc(IR_block_t * cfg, IR_e t);

/* Reset all runtime state (value, counter, state, visited) in every node of cfg.
 * Call before re-executing a graph. */
void         IR_reset(IR_block_t * cfg);

/* Free cfg and all its nodes.  After return cfg is invalid. */
void         IR_free(IR_block_t * cfg);

/* Print a human-readable dump of cfg to fp (for debugging / --dump-ir). */
void         IR_print(const IR_block_t * cfg, FILE * fp);

/* Return the canonical name string for a kind (e.g. "IR_PAT_ARB"). */
const char * IR_e_name(IR_e k);

#endif /* SCRIP_IR_H */
