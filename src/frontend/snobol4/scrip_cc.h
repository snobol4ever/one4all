#ifndef SCRIP_CC_H
#define SCRIP_CC_H
/*
 * scrip_cc.h — IR for the scrip-cc SNOBOL4→C compiler
 *
 * ONE expression type.  The emitter decides whether to call
 * pat_* or * based on emission context (subject vs pattern field).
 *
 * Statement structure:
 *   [label]  subject  [pattern]  [= replacement]  [: goto]
 *
 * subject, pattern, replacement are all AST_t*.
 * The emitter receives context when walking each field.
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdarg.h>

/* ---- expression node kinds — from shared IR ---- */
/*
 * M-G1-IR-HEADER-WIRE: AST_e is now defined in ir/ir.h (the single source
 * of truth for all canonical node kinds).
 *
 * M-G3-ALIAS-CLEANUP: IR_COMPAT_ALIASES section removed — dead code,
 * never enabled. All code uses canonical AST_e names directly:
 * AST_VAR, AST_ALT, AST_MNS, AST_POW, AST_CAPT_COND_ASGN, AST_CAPT_IMMED_ASGN,
 * AST_CAPT_CURSOR, AST_NUL, AST_ASSIGN, AST_SCAN, AST_ITERATE, AST_ALTERNATE, AST_IDX.
 *
 * ir.h defines AST_t (with fval, nalloc, id) when included first.
 * scrip_cc.h defines a compatible subset when included standalone.
 * Both are guarded by EXPR_T_DEFINED to prevent double-definition.
 */
#include "ir/ir.h"

/*
 * AST_t — unified n-ary expression node.
 *
 * All structural children live in the `children` array (realloc-grown via
 * expr_add_child).  Use the named accessor macros — never index children[]
 * directly in backends; the macros give NULL-safe bounds-checked access.
 *
 * Layout by kind:
 *   leaves  (AST_QLIT/AST_ILIT/AST_FLIT/AST_NUL/AST_VAR/AST_KEYWORD)     nchildren=0
 *   unary   (AST_MNS/AST_CAPT_CURSOR/AST_INDIRECT/...)                nchildren=1
 *   binary  (AST_ADD/AST_SUB/AST_MUL/AST_DIV/AST_POW/AST_OPSYN/
 *            AST_ASSIGN/AST_CAPT_COND_ASGN/AST_CAPT_IMMED_ASGN/AST_IDX)      nchildren=2
 *   n-ary   (AST_SEQ / AST_CAT / AST_ALT)                   nchildren>=0
 *   call    (AST_FNC)                                       nchildren=nargs
 *   subscript (AST_IDX)                                     children[0]=base, children[1..]=indices
 *
 * Named accessors (NULL-safe):
 *   expr_left(e)    — children[0]
 *   expr_right(e)   — children[1]
 *   expr_arg(e, i)  — children[i]
 *   expr_nargs(e)   — nchildren
 */
/* AST_t is defined in ir/ir.h (included above) — ir.h is the sole owner.
 * FI-0A: scrip_cc.h no longer carries a duplicate struct body. */

/* NULL-safe named accessors */
#define expr_left(e)     ((e) && (e)->nchildren >= 1 ? (e)->children[0] : NULL)
#define expr_right(e)    ((e) && (e)->nchildren >= 2 ? (e)->children[1] : NULL)
#define expr_arg(e, i)   ((e) && (i) >= 0 && (i) < (e)->nchildren ? (e)->children[(i)] : NULL)
#define expr_nargs(e)    ((e) ? (e)->nchildren : 0)

/* ---- source language tags (U-12) ---- */
#define LANG_SNO   0   /* SNOBOL4 */
#define LANG_ICN   1   /* Icon    */
#define LANG_PL    2   /* Prolog  */
#define LANG_RAKU  3   /* Raku    */
#define LANG_SCRIP 4   /* shared constants (U-23) */
#define LANG_REB   5   /* Rebus   */

/* ---- statement ---- */
typedef struct STMT_t STMT_t;
struct STMT_t {
    char    *label;
    AST_t  *subject;
    AST_t  *pattern;
    AST_t  *replacement;
    /* goto fields (RS-1): flattened; set from parser goto_label_expr results */
    char    *goto_s;              /* :S(label) — on success */
    char    *goto_f;              /* :F(label) — on failure */
    char    *goto_u;              /* :(label)  — unconditional */
    AST_t  *goto_s_expr;         /* :S(expr)  — computed success */
    AST_t  *goto_f_expr;         /* :F(expr)  — computed failure */
    AST_t  *goto_u_expr;         /* :(expr)   — computed unconditional */
    int      lineno;
    int      stno;    /* SN-26-bridge-coverage-j: source-statement number,
                         1-based, sequential, including blank statements.
                         Used as &STNO and emitted as MWK_LABEL payload.
                         Must be set at parse time so backward gotos
                         report the correct source stno (not a linear
                         execution counter). */
    int      is_end;
    int      has_eq;
    int      lang;    /* LANG_SNO / LANG_ICN / LANG_PL / LANG_RAKU / LANG_SCRIP / LANG_REB (U-12) */
    STMT_t  *next;
};

/* ---- program ---- */
/* ---- EXPORT / IMPORT lists (linker sprint LP-4) ---- */

typedef struct ExportEntry {
    char              *name;   /* exported symbol name, e.g. "WORDCOUNT" */
    struct ExportEntry *next;
} ExportEntry;

typedef struct ImportEntry {
    char              *lang;   /* source language prefix, e.g. "SNOBOL4"  */
    char              *name;   /* assembly base name,     e.g. "Greet_lib" */
    char              *method; /* exported method name,   e.g. "GREET"     */
    struct ImportEntry *next;
} ImportEntry;

typedef struct {
    STMT_t      *head;
    STMT_t      *tail;
    int          nstmts;
    ExportEntry *exports;   /* singly-linked list of EXPORT directives */
    ImportEntry *imports;   /* singly-linked list of IMPORT directives */
} CODE_t;

/* CODE_t — the IR for a list of statements (what CODE operates on).
 * AST_t is the IR for one expression (what EVAL operates on).
 * See eval_code.c: eval_expr / code / exec_code.
 */

/* ---- allocators ---- */
static inline AST_t *expr_new(AST_e k) {
    AST_t *e = calloc(1, sizeof *e); e->kind = k; return e;
}
static inline STMT_t  *stmt_new(void)  { return calloc(1, sizeof(STMT_t)); }

/* Append one child — the only way to grow a node's children array. */
static inline void expr_add_child(AST_t *e, AST_t *child) {
    e->children = realloc(e->children,
                          (size_t)(e->nchildren + 1) * sizeof(AST_t *));
    e->children[e->nchildren++] = child;
}

/* Convenience: build a unary node (one child). */
static inline AST_t *expr_unary(AST_e k, AST_t *operand) {
    AST_t *e = expr_new(k);
    expr_add_child(e, operand);
    return e;
}

/* Convenience: build a binary node (two children). */
static inline AST_t *expr_binary(AST_e k, AST_t *left, AST_t *right) {
    AST_t *e = expr_new(k);
    expr_add_child(e, left);
    expr_add_child(e, right);
    return e;
}

/* ---- string helpers ---- */
static inline char *intern(const char *s) { return s ? strdup(s) : NULL; }
static inline char *intern_n(const char *s, int n) {
    char *p = malloc(n+1); memcpy(p,s,n); p[n]='\0'; return p;
}

/* ---- public API ---- */
void     sno_add_include_dir(const char *d);
void     sno_reset(void);          /* reset per-file state between multi-file compilations */
CODE_t *sno_parse(FILE *f, const char *filename);
AST_t  *parse_expr_from_str(const char *src);
AST_t  *parse_expr_pat_from_str(const char *src); /* bison: bare expr -> AST_t, pattern slot */
CODE_t *sno_parse_string(const char *src);         /* bison: multi-stmt string -> CODE_t* */
void     c_emit(CODE_t *prog, FILE *out);

/* SN-19: case-sensitivity control. Default = case-INsensitive (fold to upper).
 * Pass 1 to switch to case-SENsitive mode (CSNOBOL4 -f equivalent). */
void     sno_set_case_sensitive(int on);
int      sno_get_case_sensitive(void);

/* SN-19: runtime fold helper — fold an identifier string in-place to the same
 * canonical case the lexer uses. No-op in case-sensitive mode. Call on names
 * that enter the runtime from user-data strings (DEFINE spec, OPSYN, APPLY
 * string arg, etc.) so they match lexer-sourced names. */
void     sno_fold_name(char *name);

/* emit_byrd.c interface now internal to emit_byrd_c.c */

/* ---- error ---- */
void sno_error(int lineno, const char *fmt, ...);
extern int   sno_nerrors;
extern char *yyfilename;
extern int   lineno_stmt;

#endif
