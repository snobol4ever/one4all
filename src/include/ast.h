#ifndef TREE_H
#define TREE_H
#ifdef __cplusplus
extern "C" {
#endif
/*================================================================================================================================================================================*/
typedef enum tree_e {
    TT_QLIT,              TT_ILIT,              TT_FLIT,              TT_CSET,              TT_NUL,
    TT_VAR,               TT_KEYWORD,           TT_INDIRECT,          TT_DEFER,
    TT_INTERROGATE,       TT_NAME,              TT_MNS,               TT_PLS,
    TT_ADD,               TT_SUB,               TT_MUL,               TT_DIV,               TT_MOD,               TT_POW,
    TT_SEQ,               TT_CAT,               TT_ALT,               TT_VLIST,             TT_OPSYN,
    TT_ARB,               TT_ARBNO,             TT_POS,               TT_RPOS,
    TT_ANY,               TT_NOTANY,            TT_SPAN,              TT_BREAK,             TT_BREAKX,
    TT_LEN,               TT_TAB,               TT_RTAB,              TT_REM,
    TT_FAIL,              TT_SUCCEED,           TT_FENCE,             TT_ABORT,             TT_BAL,
    TT_CAPT_COND_ASGN,    TT_CAPT_IMMED_ASGN,   TT_CAPT_CURSOR,
    TT_FNC,               TT_IDX,               TT_ASSIGN,            TT_SCAN,              TT_SWAP,
    TT_SUSPEND,           TT_TO,                TT_TO_BY,             TT_LIMIT,
    TT_ALTERNATE,         TT_ITERATE,           TT_MAKELIST,
    TT_UNIFY,             TT_CLAUSE,            TT_CHOICE,            TT_CUT,               TT_TRAIL_MARK,        TT_TRAIL_UNWIND,
    TT_LT,                TT_LE,                TT_GT,                TT_GE,                TT_EQ,                TT_NE,
    TT_LLT,               TT_LLE,               TT_LGT,               TT_LGE,               TT_LEQ,               TT_LNE,
    TT_CSET_COMPL,        TT_CSET_UNION,        TT_CSET_DIFF,         TT_CSET_INTER,        TT_LCONCAT,
    TT_NONNULL,           TT_NULL,              TT_NOT,               TT_SIZE,              TT_RANDOM,            TT_IDENTICAL,         TT_AUGOP,
    TT_SEQ_EXPR,          TT_EVERY,             TT_WHILE,             TT_UNTIL,             TT_REPEAT,
    TT_IF,                TT_CASE,              TT_RETURN,            TT_PROC_FAIL,         TT_LOOP_BREAK,        TT_LOOP_NEXT,         TT_BANG_BINARY,
    TT_SECTION,           TT_SECTION_PLUS,      TT_SECTION_MINUS,
    TT_RECORD,            TT_FIELD,             TT_GLOBAL,            TT_LOCAL,             TT_STATIC_DECL,       TT_INITIAL,           TT_REVASSIGN,         TT_REVSWAP,
    TT_PROGRAM,           TT_STMT,              TT_END,               TT_ATTR,
    TT_GOTO_S,            TT_GOTO_F,            TT_GOTO_U,
    TT_KIND_COUNT
} tree_e;
/*--------------------------------------------------------------------------------------------------------------------------------------------------------------------------------*/
typedef enum {
    AUGOP_ADD        = 1,
    AUGOP_SUB        = 2,
    AUGOP_MUL        = 3,
    AUGOP_DIV        = 4,
    AUGOP_MOD        = 5,
    AUGOP_POW        = 6,
    AUGOP_CONCAT     = 7,
    AUGOP_CSET_UNION = 8,
    AUGOP_CSET_DIFF  = 9,
    AUGOP_CSET_INTER = 10,
    AUGOP_SCAN       = 11,
    AUGOP_EQ         = 12,
    AUGOP_SEQ        = 13,
    AUGOP_LT         = 14,
    AUGOP_LE         = 15,
    AUGOP_GT         = 16,
    AUGOP_GE         = 17,
    AUGOP_NE         = 18,
    AUGOP_SLT        = 19,
    AUGOP_SLE        = 20,
    AUGOP_SGT        = 21,
    AUGOP_SGE        = 22,
    AUGOP_SNE        = 23,
} AugOp_e;
/*--------------------------------------------------------------------------------------------------------------------------------------------------------------------------------*/
typedef struct tree_t tree_t;
struct tree_t {
    tree_e      t;
    union {
        char      * sval;
        long long   ival;
        double      dval;
    } v;
    int         n;
    tree_t   ** c;
    int         _nalloc;
    int         _id;
};
/*--------------------------------------------------------------------------------------------------------------------------------------------------------------------------------*/
#include <stdlib.h>
static inline void ast_push(tree_t * p, tree_t * child) {
    if (p->n >= p->_nalloc) {
        p->_nalloc = p->_nalloc ? p->_nalloc * 2 : 4;
        p->c = (tree_t **)realloc(p->c, (size_t)p->_nalloc * sizeof(tree_t *));
    }
    p->c[p->n++] = child;
}
/*--------------------------------------------------------------------------------------------------------------------------------------------------------------------------------*/
static inline tree_t * ast_pop(tree_t * p) {
    tree_t * child;
    if (p->n == 0) return NULL;
    child = p->c[--p->n];
    if (p->n == 0) {
        free(p->c); p->c = NULL; p->_nalloc = 0;
    } else if (p->n < p->_nalloc / 4) {
        p->_nalloc /= 2;
        p->c = (tree_t **)realloc(p->c, (size_t)p->_nalloc * sizeof(tree_t *));
    }
    return child;
}
/*--------------------------------------------------------------------------------------------------------------------------------------------------------------------------------*/
static inline tree_t * ast_node_new(tree_e kind) {
    tree_t * e = (tree_t *)calloc(1, sizeof(tree_t));
    e->t = kind;
    return e;
}
/*================================================================================================================================================================================*/
#ifdef IR_DEFINE_NAMES
static const char * const tt_e_name[TT_KIND_COUNT] = {
    [TT_QLIT]             = "TT_QLIT",           [TT_ILIT]             = "TT_ILIT",           [TT_FLIT]             = "TT_FLIT",
    [TT_CSET]             = "TT_CSET",           [TT_NUL]              = "TT_NUL",
    [TT_VAR]              = "TT_VAR",            [TT_KEYWORD]          = "TT_KEYWORD",         [TT_INDIRECT]         = "TT_INDIRECT",
    [TT_DEFER]            = "TT_DEFER",
    [TT_INTERROGATE]      = "TT_INTERROGATE",    [TT_NAME]             = "TT_NAME",
    [TT_MNS]              = "TT_MNS",            [TT_PLS]              = "TT_PLS",
    [TT_ADD]              = "TT_ADD",            [TT_SUB]              = "TT_SUB",             [TT_MUL]              = "TT_MUL",
    [TT_DIV]              = "TT_DIV",            [TT_MOD]              = "TT_MOD",             [TT_POW]              = "TT_POW",
    [TT_SEQ]              = "TT_SEQ",            [TT_CAT]              = "TT_CAT",             [TT_ALT]              = "TT_ALT",
    [TT_VLIST]            = "TT_VLIST",          [TT_OPSYN]            = "TT_OPSYN",
    [TT_ARB]              = "TT_ARB",            [TT_ARBNO]            = "TT_ARBNO",
    [TT_POS]              = "TT_POS",            [TT_RPOS]             = "TT_RPOS",
    [TT_ANY]              = "TT_ANY",            [TT_NOTANY]           = "TT_NOTANY",          [TT_SPAN]             = "TT_SPAN",
    [TT_BREAK]            = "TT_BREAK",          [TT_BREAKX]           = "TT_BREAKX",
    [TT_LEN]              = "TT_LEN",            [TT_TAB]              = "TT_TAB",             [TT_RTAB]             = "TT_RTAB",
    [TT_REM]              = "TT_REM",
    [TT_FAIL]             = "TT_FAIL",           [TT_SUCCEED]          = "TT_SUCCEED",         [TT_FENCE]            = "TT_FENCE",
    [TT_ABORT]            = "TT_ABORT",          [TT_BAL]              = "TT_BAL",
    [TT_CAPT_COND_ASGN]   = "TT_CAPT_COND_ASGN", [TT_CAPT_IMMED_ASGN]  = "TT_CAPT_IMMED_ASGN", [TT_CAPT_CURSOR]      = "TT_CAPT_CURSOR",
    [TT_FNC]              = "TT_FNC",            [TT_IDX]              = "TT_IDX",             [TT_ASSIGN]           = "TT_ASSIGN",
    [TT_SCAN]             = "TT_SCAN",           [TT_SWAP]             = "TT_SWAP",
    [TT_SUSPEND]          = "TT_SUSPEND",        [TT_TO]               = "TT_TO",              [TT_TO_BY]            = "TT_TO_BY",
    [TT_LIMIT]            = "TT_LIMIT",          [TT_ALTERNATE]        = "TT_ALTERNATE",       [TT_ITERATE]          = "TT_ITERATE",
    [TT_MAKELIST]         = "TT_MAKELIST",
    [TT_UNIFY]            = "TT_UNIFY",          [TT_CLAUSE]           = "TT_CLAUSE",          [TT_CHOICE]           = "TT_CHOICE",
    [TT_CUT]              = "TT_CUT",            [TT_TRAIL_MARK]       = "TT_TRAIL_MARK",      [TT_TRAIL_UNWIND]     = "TT_TRAIL_UNWIND",
    [TT_LT]               = "TT_LT",             [TT_LE]               = "TT_LE",              [TT_GT]               = "TT_GT",
    [TT_GE]               = "TT_GE",             [TT_EQ]               = "TT_EQ",              [TT_NE]               = "TT_NE",
    [TT_LLT]              = "TT_LLT",            [TT_LLE]              = "TT_LLE",             [TT_LGT]              = "TT_LGT",
    [TT_LGE]              = "TT_LGE",            [TT_LEQ]              = "TT_LEQ",             [TT_LNE]              = "TT_LNE",
    [TT_CSET_COMPL]       = "TT_CSET_COMPL",     [TT_CSET_UNION]       = "TT_CSET_UNION",      [TT_CSET_DIFF]        = "TT_CSET_DIFF",
    [TT_CSET_INTER]       = "TT_CSET_INTER",     [TT_LCONCAT]          = "TT_LCONCAT",
    [TT_NONNULL]          = "TT_NONNULL",        [TT_NULL]             = "TT_NULL",            [TT_NOT]              = "TT_NOT",
    [TT_SIZE]             = "TT_SIZE",           [TT_RANDOM]           = "TT_RANDOM",          [TT_IDENTICAL]        = "TT_IDENTICAL",
    [TT_AUGOP]            = "TT_AUGOP",
    [TT_SEQ_EXPR]         = "TT_SEQ_EXPR",       [TT_EVERY]            = "TT_EVERY",           [TT_WHILE]            = "TT_WHILE",
    [TT_UNTIL]            = "TT_UNTIL",          [TT_REPEAT]           = "TT_REPEAT",          [TT_IF]               = "TT_IF",
    [TT_CASE]             = "TT_CASE",           [TT_RETURN]           = "TT_RETURN",          [TT_PROC_FAIL]        = "TT_PROC_FAIL",
    [TT_LOOP_BREAK]       = "TT_LOOP_BREAK",     [TT_LOOP_NEXT]        = "TT_LOOP_NEXT",       [TT_BANG_BINARY]      = "TT_BANG_BINARY",
    [TT_SECTION]          = "TT_SECTION",        [TT_SECTION_PLUS]     = "TT_SECTION_PLUS",    [TT_SECTION_MINUS]    = "TT_SECTION_MINUS",
    [TT_RECORD]           = "TT_RECORD",         [TT_FIELD]            = "TT_FIELD",           [TT_GLOBAL]           = "TT_GLOBAL",
    [TT_LOCAL]            = "TT_LOCAL",          [TT_STATIC_DECL]      = "TT_STATIC_DECL",
    [TT_INITIAL]          = "TT_INITIAL",        [TT_REVASSIGN]        = "TT_REVASSIGN",       [TT_REVSWAP]          = "TT_REVSWAP",
    [TT_PROGRAM]          = "TT_PROGRAM",        [TT_STMT]             = "TT_STMT",            [TT_END]              = "TT_END",
    [TT_ATTR]             = "TT_ATTR",           [TT_GOTO_S]           = "TT_GOTO_S",          [TT_GOTO_F]           = "TT_GOTO_F",
    [TT_GOTO_U]           = "TT_GOTO_U",
};
#endif
/*================================================================================================================================================================================*/
#ifdef __cplusplus
}
#endif
#endif
