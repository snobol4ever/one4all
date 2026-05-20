#pragma once
#ifndef SCRIP_IR_H
#define SCRIP_IR_H
#include <stdint.h>
#include <stdio.h>
#include "descr.h"
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
#define IR_LANG_SNO  1
#define IR_LANG_SCO  2
#define IR_LANG_REB  3
#define IR_LANG_ICN  4
#define IR_LANG_PL   5
#define IR_LANG_RKU  6
typedef enum {
    BB_LIT_I,
    BB_LIT_S,
    BB_LIT_F,
    BB_LIT_NUL,
    BB_VAR,
    BB_ASSIGN,
    BB_AUGOP,
    BB_BINOP,
    BB_UNOP,
    BB_CALL,
    BB_SEQ,
    BB_FAIL,
    BB_SUCCEED,
    BB_GOTO,
    BB_RETURN,
    BB_IF,
    BB_ALTERNATE,
    BB_TO_BY,
    BB_EVERY,
    BB_WHILE,
    BB_UNTIL,
    BB_REPEAT,
    BB_ALT,
    BB_SIZE,
    BB_CASE,
    BB_LIMIT,
    BB_SUSPEND,
    BB_PROC,
    BB_SCAN,
    BB_NONNULL,
    BB_INTERROGATE,
    BB_NOT,
    BB_PAT_LIT,
    BB_PAT_ANY,
    BB_PAT_SPAN,
    BB_PAT_BREAK,
    BB_PAT_ARB,
    BB_PAT_ARBNO,
    BB_PAT_CAT,
    BB_PAT_ALT,
    BB_PAT_ASSIGN_IMM,
    BB_PAT_ASSIGN_COND,
    BB_PAT_LEN,
    BB_PAT_NOTANY,
    BB_PAT_POS,
    BB_PAT_TAB,
    BB_PAT_REM,
    BB_PAT_FENCE,
    BB_PAT_ABORT,
    BB_PAT_CALLOUT,     /* *Fn() deferred call                                                    */
    BB_PL_CHOICE,
    BB_PL_UNIFY,
    BB_PL_CUT,
    BB_PL_CALL,
    BB_PL_BUILTIN,      /* Prolog builtin call: sval=name, c[0..n-1]=arg IR nodes                  */
    BB_PL_VAR,          /* Prolog variable read: ival=slot index into g_pl_env[]                   */
    BB_PL_ATOM,         /* Prolog atom literal: sval=atom name                                     */
    BB_PL_ARITH,        /* Prolog arithmetic: sval=op, c[0]=lhs, c[1]=rhs                         */
    BB_PL_ALT,          /* Prolog inline disjunction (';'): c[0]=branch-A BB_t*, c[1]=branch-B BB_t* (no opaque) */
    BB_PL_SEQ,          /* Prolog conjunction body: short-circuit on first failure; succeed if all succeed.       */
    BB_ICN_TO,
    BB_ICN_UPTO,
    /* BB_ICN_EVERY removed (DAI-1, IJ-DEL-ICN-AST). Mode-2/3/4 use BB_EVERY. */
    BB_ICN_TO_BY,
    BB_ICN_ITERATE,
    BB_ICN_ALTERNATE,   /* A|B — opaque=icn_alt_dcg_t*{gen[2],which}; left first then right              */
    BB_ICN_LIMIT,       /* gen\N — opaque=icn_lim_dcg_t*{gen,max,count}; yield up to max ticks            */
    BB_ICN_BINOP,       /* arith/relop with generative operands; opaque=icn_binop_dcg_t*                    */
    BB_ICN_TO_NESTED,   /* (lo_gen) to (hi_gen) cross-product; opaque=icn_to_nested_state_t*                */
    BB_ICN_PROC_GEN,    /* user proc generator via GeneratorState; opaque=GeneratorState*                            */
    BB_BREAK,           /* Icon break — set FRAME.loop_break=1; propagate ω                                         */
    BB_NEXT,            /* Icon next — set FRAME.loop_next=1; propagate ω                                           */
    BB_IDENTICAL,       /* Icon === — c[0]=lhs, c[1]=rhs; succeed with rhs if identical, else ω                     */
    BB_NULL_TEST,       /* Icon \x null-test — c[0]=operand; succeed with &null if null, else ω                     */
    BB_RANDOM,          /* Icon ?E random element — c[0]=operand; succeed with random value, else ω                 */
    BB_NEG,             /* Icon -E unary minus — c[0]=operand; succeed with -E (numeric coerce), else ω             */
    BB_POS,             /* Icon +E unary plus  — c[0]=operand; succeed with +E (numeric coerce), else ω             */
    BB_CSET_COMPL,      /* Icon ~E cset complement — c[0]=operand; coerce to string, complement vs &cset (256 chars) */
    BB_CSET_UNION,      /* Icon E1++E2 cset union — c[0]/c[1]=operands; coerce to strings, canonical(union(a,b))    */
    BB_CSET_DIFF,       /* Icon E1--E2 cset difference — c[0]/c[1]=operands; coerce to strings, canonical(diff(a,b))*/
    BB_CSET_INTER,      /* Icon E1**E2 cset intersection — c[0]/c[1]=operands; coerce to strings, canonical(inter)  */
    BB_ICN_SCAN,        /* Icon subj ? body — c[0]=subj, c[1]=body; saves+sets scan_subj/scan_pos, restores on exit */
    BB_ICN_KEYWORD,     /* Icon &name keyword read — sval=full name with leading '&' (e.g. "&subject", "&pos")      */
    BB_BINOP_GEN,       /* Generator-aware binop — same encoding as BB_BINOP (ival=op, ival2=is_relop, c[0]/c[1]   */
                        /* = lhs/rhs) but yields the cross-product when either operand is a generator. state==0   */
                        /* fresh, state==1 active; on beta resumes c[1] first, then advances c[0] and re-seeds c[1] */
    BB_ICN_IDX,         /* Icon s[i] subscript — c[0]=base, c[1]=index; calls subscript_get(base,idx); FAIL on OOB  */
    BB_ICN_SECTION,     /* Icon s[i:j] / s[i+:n] / s[i-:n] section — c[0]=base,c[1]=i,c[2]=j; ival=section-kind     */
    BB_ICN_LIST_BANG,   /* Icon !L list/table/record generator — c[0]=iterable expr; state=pos; α resets, β advances  */
    BB_ICN_RECORD_DEF,  /* Icon `record T(f1,f2,...)` — sval=spec string; α runs DEFDAT+sc_dat_register once          */
    BB_ICN_FIELD_GET,   /* Icon obj.field read  — c[0]=object expr, sval=field name; calls data_field_ptr            */
    BB_ICN_FIELD_SET,   /* Icon obj.field := rhs — c[0]=object expr, c[1]=rhs expr, sval=field name; data_field_ptr  */
    BB_ICN_IDX_SET,     /* Icon base[idx] := rhs — c[0]=base, c[1]=index, c[2]=rhs; calls subscript_set              */
    BB_ICN_KEY_GEN,     /* Icon key(t) generator — c[0]=table expr; yields each key in bucket order                   */
    BB_SWAP,            /* Icon x :=: y swap. c[0]=lhs var, c[1]=rhs var. Eval c[1], eval c[0], store rhs-value into  */
                        /* lhs slot, lhs-value into rhs slot. Succeeds with the value that ended up in c[0]'s slot.   */
                        /* Frame-slot path mirrors BB_ASSIGN (scope_get/FRAME.env[slot]); global-NV fallback for both. */
    BB_SEQ_EXPR,        /* Value-of-last sequence (Icon (e1;e2;e3) paren-seq, {} blocks). Distinct from BB_SEQ which   */
                        /* models proc-body-falls-off-end (returns FAILDESCR). α: head statements once + drive tail   */
                        /* fresh; β: resume tail only (NEVER re-fire the head — preserves side-effect-once semantics  */
                        /* under every/while pumping). state==0 fresh, ==1 active.                                    */
    BB_INITIAL,         /* Icon `initial expr` — run c[0] on first ever entry to this procedure, no-op on subsequent.  */
                        /* has-run flag stored in nd->ival3 (NOT cleared by bb_reset or IR_snapshot_state, so survives */
                        /* recursive re-entry and the per-call bb_reset of the proc body). Always succeeds via γ.      */
    BB_ICN_LCONCAT,     /* Icon E1 ||| E2 list concat — c[0]/c[1] operands. If both args are icnlist values, builds a  */
                        /* fresh icnlist by appending all elements; otherwise falls back to string concat with         */
                        /* numeric coercion (matches TT_LCONCAT spec in legacy bb_eval_value). Dispatches via helper   */
                        /* icn_lconcat_d in icn_value.c so AST-walk and IR paths stay in lock-step.                    */
    BB_ICN_FIND_GEN,    /* Icon find(needle, hay [, start [, stop]]) generator. Yields each match position 1..N.       */
                        /* c[0]=needle, c[1]=hay, optional c[2]=start, c[3]=stop. counter tracks last-yielded position */
                        /* (1-based, 0 means fresh); opaque caches needle/hay strings. α resolves, β advances.         */
    BB_ICN_SEQ_GEN,     /* Icon seq([start [, step]]) infinite arithmetic-progression generator. Yields start, start+  */
                        /* step, start+2step, ...  c[0]=optional start (default 1), c[1]=optional step (default 1).    */
                        /* counter holds last-yielded value (ival used as cached step). α reads args, β advances.      */
                        /* Always pair with BB_LIMIT / `\N` or `every` with break — otherwise infinite loop.            */
    IR_E_COUNT
} BB_op_t;
typedef struct BB_t BB_t;
struct BB_t {
    BB_op_t           t;
    BB_t         * α;
    BB_t         * β;
    BB_t         * γ;
    BB_t         * ω;
    BB_t        ** c;
    int            n;
    union {
        int64_t        ival;
        double         dval;
        const char   * sval;
    };
    const char   * sval2;
    int64_t        ival2;
    int64_t        ival3;
    void         * opaque;
    DESCR_t        value;
    int64_t        counter;
    int            state;
};
typedef struct {
    BB_t    * entry;
    BB_t   ** all;
    int            n;
    int            max;
    int            lang;    /* IR_LANG_* — language that produced this graph    */
} BB_graph_t;
/*----------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------*/
BB_graph_t * BB_alloc(int max_nodes, int lang);
/*----------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------*/
BB_t       * BB_node_alloc(BB_graph_t * cfg, BB_op_t t);
/*----------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------*/
void         bb_reset(BB_graph_t * cfg);
/*----------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------*/
typedef struct { DESCR_t value; int64_t counter; int state; } IR_node_state_t;
/*----------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------*/
IR_node_state_t * IR_snapshot_state(BB_graph_t * cfg);
/*----------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------*/
void              IR_restore_state(BB_graph_t * cfg, IR_node_state_t * snap);
/*----------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------*/
void         BB_free(BB_graph_t * cfg);
/*----------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------*/
void         bb_print(const BB_graph_t * cfg, FILE * fp);
/*----------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------*/
const char * IR_e_name(BB_op_t k);
#endif
