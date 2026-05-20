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
    TT_NONNULL,           TT_NULL,              TT_NOT,               TT_SIZE,              TT_RANDOM,            TT_IDENTICAL,         TT_AUGOP,             TT_MATCH_UNARY,
    TT_SEQ_EXPR,          TT_EVERY,             TT_WHILE,             TT_UNTIL,             TT_REPEAT,            TT_FOR,               TT_DO_WHILE,
    TT_IF,                TT_UNLESS,            TT_CASE,              TT_RETURN,            TT_PROC_FAIL,         TT_NRETURN,           TT_LOOP_BREAK,        TT_LOOP_NEXT,         TT_BANG_BINARY,       TT_DEFINE,
    TT_SECTION,           TT_SECTION_PLUS,      TT_SECTION_MINUS,
    TT_RECORD,            TT_FIELD,             TT_GLOBAL,            TT_LOCAL,             TT_STATIC_DECL,       TT_INITIAL,           TT_REVASSIGN,         TT_REVSWAP,
    TT_PROGRAM,           TT_STMT,              TT_END,               TT_ATTR,
    TT_GOTO_S,            TT_GOTO_F,            TT_GOTO_U,
    TT_GATHER,
    TT_FUNCTION,          TT_RECORD_DECL,
    TT_SAY,               TT_SAY_FH,
    TT_PRINT,             TT_PRINT_FH,
    TT_PROC_DECL,
    TT_SUB_DECL,
    TT_DIE,
    TT_TRY,
    TT_ARR_GET,           TT_ARR_SET,
    TT_HASH_GET,          TT_HASH_SET,          TT_HASH_DELETE,       TT_HASH_EXISTS,
    TT_CLASS_DECL,
    TT_FOR_RANGE,
    TT_DECL,
    TT_SMATCH,
    TT_NEW,
    TT_METHCALL,
    TT_MAP,               TT_GREP,              TT_SORT,
    TT_CAPTURE,           TT_NAMED_CAPTURE,
    TT_TWIGIL_FIELD,
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
};
/*--------------------------------------------------------------------------------------------------------------------------------------------------------------------------------*/
#include <stdlib.h>
#include <string.h>
/* capacity stored as size_t prefix word immediately before p->c[0] */
#define AST_CAP(p)         (*(size_t *)((char *)(p)->c - sizeof(size_t)))
#define AST_SET_CAP(p, v)  (*(size_t *)((char *)(p)->c - sizeof(size_t)) = (size_t)(v))
static inline void ast_push(tree_t * p, tree_t * child) {
    size_t cap = p->c ? AST_CAP(p) : 0;
    if ((size_t)p->n >= cap) {
        size_t new_cap = cap ? cap * 2 : 4;
        char * block = (char *)realloc(p->c ? (char *)p->c - sizeof(size_t) : NULL, sizeof(size_t) + new_cap * sizeof(tree_t *));
        p->c = (tree_t **)(block + sizeof(size_t));
        AST_SET_CAP(p, new_cap);
    }
    p->c[p->n++] = child;
}
/*--------------------------------------------------------------------------------------------------------------------------------------------------------------------------------*/
static inline tree_t * ast_pop(tree_t * p) {
    tree_t * child;
    if (p->n == 0) return NULL;
    child = p->c[--p->n];
    return child;
}
/*--------------------------------------------------------------------------------------------------------------------------------------------------------------------------------*/
static inline tree_t * ast_node_new(tree_e kind) {
    tree_t * e = (tree_t *)calloc(1, sizeof(tree_t));
    e->t = kind;
    return e;
}
/*--------------------------------------------------------------------------------------------------------------------------------------------------------------------------------*/
/* TT_FIELD layout: c[0]=object expr, c[1]=TT_VAR(field_name). Use this macro everywhere. */
#define ICN_FIELD_NAME(e) ((e)->n >= 2 && (e)->c[1] ? (e)->c[1]->v.sval : NULL)
/*================================================================================================================================================================================*/
#ifdef BB_DEFINE_NAMES
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
    [TT_AUGOP]            = "TT_AUGOP",          [TT_MATCH_UNARY]      = "TT_MATCH_UNARY",
    [TT_SEQ_EXPR]         = "TT_SEQ_EXPR",       [TT_EVERY]            = "TT_EVERY",           [TT_WHILE]            = "TT_WHILE",
    [TT_UNTIL]            = "TT_UNTIL",          [TT_REPEAT]           = "TT_REPEAT",          [TT_FOR]              = "TT_FOR",            [TT_DO_WHILE]         = "TT_DO_WHILE",        [TT_IF]               = "TT_IF",             [TT_UNLESS]           = "TT_UNLESS",
    [TT_CASE]             = "TT_CASE",           [TT_RETURN]           = "TT_RETURN",          [TT_PROC_FAIL]        = "TT_PROC_FAIL",       [TT_NRETURN]          = "TT_NRETURN",
    [TT_LOOP_BREAK]       = "TT_LOOP_BREAK",     [TT_LOOP_NEXT]        = "TT_LOOP_NEXT",       [TT_BANG_BINARY]      = "TT_BANG_BINARY",     [TT_DEFINE]           = "TT_DEFINE",
    [TT_SECTION]          = "TT_SECTION",        [TT_SECTION_PLUS]     = "TT_SECTION_PLUS",    [TT_SECTION_MINUS]    = "TT_SECTION_MINUS",
    [TT_RECORD]           = "TT_RECORD",         [TT_FIELD]            = "TT_FIELD",           [TT_GLOBAL]           = "TT_GLOBAL",
    [TT_LOCAL]            = "TT_LOCAL",          [TT_STATIC_DECL]      = "TT_STATIC_DECL",
    [TT_INITIAL]          = "TT_INITIAL",        [TT_REVASSIGN]        = "TT_REVASSIGN",       [TT_REVSWAP]          = "TT_REVSWAP",
    [TT_PROGRAM]          = "TT_PROGRAM",        [TT_STMT]             = "TT_STMT",            [TT_END]              = "TT_END",
    [TT_ATTR]             = "TT_ATTR",           [TT_GOTO_S]           = "TT_GOTO_S",          [TT_GOTO_F]           = "TT_GOTO_F",
    [TT_GOTO_U]           = "TT_GOTO_U",         [TT_GATHER]           = "TT_GATHER",
    [TT_FUNCTION]         = "TT_FUNCTION",       [TT_RECORD_DECL]      = "TT_RECORD_DECL",
    [TT_SAY]              = "TT_SAY",             [TT_SAY_FH]           = "TT_SAY_FH",
    [TT_PRINT]            = "TT_PRINT",           [TT_PRINT_FH]         = "TT_PRINT_FH",
    [TT_PROC_DECL]        = "TT_PROC_DECL",
    [TT_SUB_DECL]         = "TT_SUB_DECL",
    [TT_DIE]              = "TT_DIE",
    [TT_TRY]              = "TT_TRY",
    [TT_ARR_GET]          = "TT_ARR_GET",         [TT_ARR_SET]          = "TT_ARR_SET",
    [TT_HASH_GET]         = "TT_HASH_GET",         [TT_HASH_SET]         = "TT_HASH_SET",
    [TT_HASH_DELETE]      = "TT_HASH_DELETE",      [TT_HASH_EXISTS]      = "TT_HASH_EXISTS",
    [TT_CLASS_DECL]       = "TT_CLASS_DECL",
    [TT_FOR_RANGE]        = "TT_FOR_RANGE",
    [TT_DECL]             = "TT_DECL",
    [TT_SMATCH]           = "TT_SMATCH",
    [TT_NEW]              = "TT_NEW",
    [TT_METHCALL]         = "TT_METHCALL",
    [TT_MAP]              = "TT_MAP",             [TT_GREP]             = "TT_GREP",             [TT_SORT]             = "TT_SORT",
    [TT_CAPTURE]          = "TT_CAPTURE",         [TT_NAMED_CAPTURE]    = "TT_NAMED_CAPTURE",
    [TT_TWIGIL_FIELD]     = "TT_TWIGIL_FIELD",
};
#endif
/*================================================================================================================================================================================*/
#ifdef __cplusplus
}
#endif
#endif
