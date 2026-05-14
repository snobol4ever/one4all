/* scrip_ir.h — Universal generator IR: ir_node_t / ir_graph_t / ir_kind_t (LR-0)
 * AUTHORS: Lon Jones Cherryholmes · Claude Sonnet 4.6 (LR-0, 2026-05-14)
 *
 * Directed Cyclic Graph (DCG) for all goal-directed computation across all six languages.
 * Built at lower time; driven by ir_exec (ir_exec.h).  SM_EXEC_DCG(ir_graph_t*) is the
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
#include "../../runtime/x86/descr.h"

/*==================================================================================================
 * Language tags — stored in ir_node_t.lang and ir_graph_t.lang
 *================================================================================================*/
#define IR_LANG_SNO  1   /* SNOBOL4  */
#define IR_LANG_SCO  2   /* Snocone  */
#define IR_LANG_REB  3   /* Rebus    */
#define IR_LANG_ICN  4   /* Icon     */
#define IR_LANG_PL   5   /* Prolog   */
#define IR_LANG_RKU  6   /* Raku     */

/*==================================================================================================
 * ir_kind_t — kind of every ir_node_t
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
    IR_PAT_POS,         /* POS(n) / RPOS(n)                                                      */
    IR_PAT_TAB,         /* TAB(n) / RTAB(n)                                                      */
    IR_PAT_REM,         /* REM                                                                    */
    IR_PAT_FENCE,       /* FENCE                                                                  */
    IR_PAT_ABORT,       /* ABORT                                                                  */
    IR_PAT_CALLOUT,     /* *Fn() deferred call                                                    */
    /* ── Prolog specific ──────────────────────────────────────────────────────────────────────── */
    IR_PL_CHOICE,       /* clause alternation (choice point)                                      */
    IR_PL_UNIFY,        /* unification                                                            */
    IR_PL_CUT,          /* !                                                                      */
    IR_PL_CALL,         /* call(Goal)                                                             */
    IR_KIND_COUNT       /* sentinel — number of kinds                                             */
} ir_kind_t;

/*==================================================================================================
 * ir_node_t — one node in the DCG
 * Four control ports are NULL until lower wires them bottom-up.
 *================================================================================================*/
typedef struct ir_node ir_node_t;
struct ir_node {
    ir_kind_t      kind;
    /* ── Four control ports (wired by lower; back-edges create cycles) ─────────────────────── */
    ir_node_t    * port_start;   /* entry: first evaluation attempt                             */
    ir_node_t    * port_resume;  /* backtrack: try next value (NULL → scalar / non-generative)  */
    ir_node_t    * port_succ;    /* success continuation (value in .value)                      */
    ir_node_t    * port_fail;    /* failure continuation (no value)                             */
    /* ── Children (pre-wiring tree; four ports wired during lower, bottom-up) ──────────────── */
    ir_node_t   ** c;            /* child array                                                 */
    int            n;            /* child count                                                 */
    /* ── Payload ────────────────────────────────────────────────────────────────────────────── */
    union {
        int64_t        ival;     /* LIT_I; also op code for BINOP / UNOP                        */
        double         dval;     /* LIT_F                                                       */
        const char   * sval;     /* LIT_S, VAR name, CALL name                                 */
        struct { ir_node_t * l; ir_node_t * r; int op; } binop;
        struct { const char * name; int nargs; } call;
    };
    /* ── Runtime execution state (live during ir_exec graph walk) ───────────────────────────── */
    DESCR_t        value;        /* current result value                                        */
    int64_t        counter;      /* TO_BY position, LIMIT count, etc.                           */
    int            state;        /* executor state machine (0 = fresh)                          */
    /* ── Graph bookkeeping ──────────────────────────────────────────────────────────────────── */
    int            id;           /* unique within ir_graph_t — set by ir_alloc_node             */
    int            generative;   /* 1 if port_resume is meaningful                              */
    int            visited;      /* scratch for traversal algorithms                            */
    int            lang;         /* IR_LANG_* — which language produced this node               */
};

/*==================================================================================================
 * ir_graph_t — a complete wired generator DCG for one procedure or pattern
 *================================================================================================*/
typedef struct {
    ir_node_t    * entry;   /* == root node's port_start                      */
    ir_node_t   ** all;     /* flat array of all nodes (for reset / GC / print) */
    int            n;       /* count of nodes in .all                           */
    int            lang;    /* IR_LANG_* — language that produced this graph    */
} ir_graph_t;

/*==================================================================================================
 * API — alloc / free / reset / print
 *================================================================================================*/

/* Allocate a fresh ir_graph_t with capacity for max_nodes nodes.
 * All node slots are NULL.  Returns NULL on OOM. */
ir_graph_t * ir_graph_alloc(int max_nodes, int lang);

/* Allocate one ir_node_t, append it to cfg->all, assign cfg-unique id.
 * Returns NULL if cfg->all is full (capacity = max_nodes passed to ir_graph_alloc). */
ir_node_t  * ir_alloc_node(ir_graph_t * cfg, ir_kind_t kind, int lang);

/* Reset all runtime state (value, counter, state, visited) in every node of cfg.
 * Call before re-executing a graph. */
void         ir_graph_reset(ir_graph_t * cfg);

/* Free cfg and all its nodes.  After return cfg is invalid. */
void         ir_graph_free(ir_graph_t * cfg);

/* Print a human-readable dump of cfg to fp (for debugging / --dump-ir). */
void         ir_graph_print(const ir_graph_t * cfg, FILE * fp);

/* Return the canonical name string for a kind (e.g. "IR_PAT_ARB"). */
const char * ir_kind_name(ir_kind_t k);

#endif /* SCRIP_IR_H */
