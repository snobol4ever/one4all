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
    IR_LIT_I,
    IR_LIT_S,
    IR_LIT_F,
    IR_LIT_NUL,
    IR_VAR,
    IR_ASSIGN,
    IR_AUGOP,
    IR_BINOP,
    IR_UNOP,
    IR_CALL,
    IR_SEQ,
    IR_FAIL,
    IR_SUCCEED,
    IR_GOTO,
    IR_RETURN,
    IR_IF,
    IR_ALTERNATE,
    IR_TO_BY,
    IR_EVERY,
    IR_WHILE,
    IR_UNTIL,
    IR_REPEAT,
    IR_ALT,
    IR_LIMIT,
    IR_SUSPEND,
    IR_PROC,
    IR_SCAN,
    IR_NONNULL,
    IR_INTERROGATE,
    IR_NOT,
    IR_PAT_LIT,
    IR_PAT_ANY,
    IR_PAT_SPAN,
    IR_PAT_BREAK,
    IR_PAT_ARB,
    IR_PAT_ARBNO,
    IR_PAT_CAT,
    IR_PAT_ALT,
    IR_PAT_ASSIGN_IMM,
    IR_PAT_ASSIGN_COND,
    IR_PAT_LEN,
    IR_PAT_NOTANY,
    IR_PAT_POS,
    IR_PAT_TAB,
    IR_PAT_REM,
    IR_PAT_FENCE,
    IR_PAT_ABORT,
    IR_PAT_CALLOUT,     /* *Fn() deferred call                                                    */
    IR_PL_CHOICE,
    IR_PL_UNIFY,
    IR_PL_CUT,
    IR_PL_CALL,
    IR_ICN_TO,
    IR_ICN_UPTO,
    IR_ICN_EVERY,
    IR_ICN_TO_BY,
    IR_ICN_ITERATE,
    IR_ICN_ALTERNATE,   /* A|B — opaque=icn_alt_dcg_t*{gen[2],which}; left first then right              */
    IR_ICN_LIMIT,       /* gen\N — opaque=icn_lim_dcg_t*{gen,max,count}; yield up to max ticks            */
    IR_ICN_BINOP,       /* arith/relop with generative operands; opaque=icn_binop_dcg_t*                    */
    IR_ICN_TO_NESTED,   /* (lo_gen) to (hi_gen) cross-product; opaque=icn_to_nested_state_t*                */
    IR_ICN_PROC_GEN,    /* user proc generator via GeneratorState; opaque=GeneratorState*                            */
    IR_E_COUNT
} IR_e;
typedef struct IR_t IR_t;
struct IR_t {
    IR_e           t;
    IR_t         * α;
    IR_t         * β;
    IR_t         * γ;
    IR_t         * ω;
    IR_t        ** c;
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
    IR_t    * entry;
    IR_t   ** all;
    int            n;
    int            lang;    /* IR_LANG_* — language that produced this graph    */
} IR_block_t;
/*----------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------*/
IR_block_t * IR_alloc(int max_nodes, int lang);
/*----------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------*/
IR_t       * IR_node_alloc(IR_block_t * cfg, IR_e t);
/*----------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------*/
void         IR_reset(IR_block_t * cfg);
/*----------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------*/
void         IR_free(IR_block_t * cfg);
/*----------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------*/
void         IR_print(const IR_block_t * cfg, FILE * fp);
/*----------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------*/
const char * IR_e_name(IR_e k);
#endif
