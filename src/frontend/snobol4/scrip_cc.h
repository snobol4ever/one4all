#ifndef SCRIP_CC_H
#define SCRIP_CC_H
/*
 * scrip_cc.h — shared IR and frontend glue for the SCRIP compiler
 *
 * All six frontends (SNOBOL4, Snocone, Icon, Prolog, Raku, Rebus) produce
 * the shared tree_t tree defined in ir/ast.h.  The pipeline waist is:
 *
 *   frontend → AST_PROGRAM (AST_STMT children) → lower() → SM_Program → runtime
 *
 * SNOBOL4 (SI-4): emits AST_PROGRAM directly via sno_parse_ast().
 * Snocone/Icon/Prolog/Raku/Rebus (SI-5): compile fn builds CODE_t internally,
 *   calls code_to_ast() to wrap into AST_PROGRAM, passes via out_ast param.
 *   Full migration to direct AST_STMT emission deferred to GOAL-SNOCONE-SM-LOWER.
 *
 * Statement structure (AST_STMT tagged-attribute children — SI-3):
 *   :lbl :lang :line :stno :subj :pat :eq :repl :goS/:goF/:go
 *
 * Expression layout (tree_t children[]):
 *   leaves  (AST_QLIT/AST_ILIT/AST_FLIT/AST_NUL/AST_VAR/AST_KEYWORD)  nchildren=0
 *   unary   (AST_MNS/AST_CAPT_CURSOR/AST_INDIRECT/...)                 nchildren=1
 *   binary  (AST_ADD/.../AST_ASSIGN/AST_CAPT_COND_ASGN/AST_IDX)       nchildren=2
 *   n-ary   (AST_SEQ/AST_CAT/AST_ALT)                                  nchildren>=0
 *   call    (AST_FNC)                                                   nchildren=nargs
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdarg.h>

/* ---- expression node kinds — from shared IR ---- */
/*
 * M-G1-IR-HEADER-WIRE: AST_e is now defined in ir/ast.h (the single source
 * of truth for all canonical node kinds).
 *
 * M-G3-ALIAS-CLEANUP: IR_COMPAT_ALIASES section removed — dead code,
 * never enabled. All code uses canonical AST_e names directly:
 * AST_VAR, AST_ALT, AST_MNS, AST_POW, AST_CAPT_COND_ASGN, AST_CAPT_IMMED_ASGN,
 * AST_CAPT_CURSOR, AST_NUL, AST_ASSIGN, AST_SCAN, AST_ITERATE, AST_ALTERNATE, AST_IDX.
 *
 * ir.h defines tree_t (with fval, nalloc, id) when included first.
 * scrip_cc.h defines a compatible subset when included standalone.
 * Both are guarded by EXPR_T_DEFINED to prevent double-definition.
 */
#include "ast/ast.h"

/*
 * tree_t — unified n-ary expression node.
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
/* tree_t is defined in ir/ast.h (included above) — ir.h is the sole owner.
 * FI-0A: scrip_cc.h no longer carries a duplicate struct body. */

/* NULL-safe named accessors */
#define expr_left(e)     ((e) && (e)->n >= 1 ? (e)->c[0] : NULL)
#define expr_right(e)    ((e) && (e)->n >= 2 ? (e)->c[1] : NULL)
#define expr_arg(e, i)   ((e) && (i) >= 0 && (i) < (e)->n ? (e)->c[(i)] : NULL)
#define expr_nargs(e)    ((e) ? (e)->n : 0)

/* ---- source language tags (U-12) ---- */
#define LANG_SNO   0   /* SNOBOL4 */
#define LANG_ICN   1   /* Icon    */
#define LANG_PL    2   /* Prolog  */
#define LANG_RAKU  3   /* Raku    */
#define LANG_SCRIP 4   /* shared constants (U-23) */
#define LANG_REB   5   /* Rebus   */

/* ---- statement ---- */
/* SI-8 NOTE: STMT_t/CODE_t remain in use by snocone/prolog/raku/rebus frontends
 * internally.  They build CODE_t and call code_to_ast() to hand AST_PROGRAM
 * to the driver; sm_preamble/execute_program no longer receive CODE_t (SI-6).
 * Full deletion of STMT_t/CODE_t and direct AST_STMT emission from all four
 * frontends is deferred to GOAL-SNOCONE-SM-LOWER (SL-1+). */
typedef struct STMT_t STMT_t;
struct STMT_t {
    char    *label;
    tree_t  *subject;
    tree_t  *pattern;
    tree_t  *replacement;
    char    *goto_s, *goto_f, *goto_u;
    tree_t  *goto_s_expr, *goto_f_expr, *goto_u_expr;
    int      lineno, stno, is_end, has_eq, lang;
    STMT_t  *next;
};

/* ---- program ---- */
typedef struct ExportEntry {
    char              *name;
    struct ExportEntry *next;
} ExportEntry;

typedef struct ImportEntry {
    char              *lang;
    char              *name;
    char              *method;
    struct ImportEntry *next;
} ImportEntry;

/* SI-8: CODE_t_opaque body defined here; used by snocone/prolog/raku/rebus
 * frontends internally. sm_preamble/execute_program no longer receive CODE_t
 * (since SI-6); full deletion deferred to GOAL-SNOCONE-SM-LOWER. */
struct CODE_t_opaque {
    STMT_t      *head;
    STMT_t      *tail;
    int          nstmts;
    ExportEntry *exports;
    ImportEntry *imports;
};
typedef struct CODE_t_opaque CODE_t;

/* ---- allocators (wrappers around tree_new/tree_push from ast.h) ---- */
/* tree_new(k) is defined in ast.h — use it directly where possible.   */
static inline tree_t *expr_new(AST_e k) { return tree_new(k); }
static inline STMT_t *stmt_new(void) { return calloc(1, sizeof(STMT_t)); }

/* expr_add_child: alias for tree_push */
static inline void expr_add_child(tree_t *e, tree_t *child) {
    tree_push(e, child);
}

/* Convenience: build a unary node (one child). */
static inline tree_t *expr_unary(AST_e k, tree_t *operand) {
    tree_t *e = tree_new(k); tree_push(e, operand); return e;
}

/* Convenience: build a binary node (two children). */
static inline tree_t *expr_binary(AST_e k, tree_t *left, tree_t *right) {
    tree_t *e = tree_new(k); tree_push(e, left); tree_push(e, right); return e;
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
tree_t  *parse_expr_from_str(const char *src);
tree_t  *parse_expr_pat_from_str(const char *src); /* bison: bare expr -> tree_t, pattern slot */
CODE_t *sno_parse_string(const char *src);         /* bison: multi-stmt string -> CODE_t* */
tree_t  *sno_parse_string_ast(const char *src, CODE_t **code_out); /* SI-6: multi-stmt string -> AST_PROGRAM */
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

/* ---- SI-2/SI-3 shim: kept for snocone/prolog/raku/rebus frontends ---- */
/* These frontends build CODE_t internally and call code_to_ast() to produce
 * their out_ast.  sm_preamble no longer uses them (SI-6).  Full migration to
 * direct AST_STMT emission is deferred to GOAL-SNOCONE-SM-LOWER (SL-1+). */
tree_t       *stmt_to_ast(const STMT_t *s);
tree_t       *code_to_ast(const CODE_t *prog);
tree_t       *stmt_attr_find(const tree_t *stmt, const char *tag);
tree_t       *stmt_attr_expr(const tree_t *attr);
const char  *stmt_attr_str(const tree_t *attr);

/* ---- SI-4: direct AST_STMT/AST_END builders (stmt_ast.c) -------------- */
/* Used by snobol4.y to emit AST tree directly without the STMT_t intermediary. */
tree_t       *ast_stmt_new(AST_e kind);
tree_t       *ast_attr_leaf(const char *tag, const char *val);
tree_t       *ast_attr_int(const char *tag, int ival);
tree_t       *ast_attr_expr(const char *tag, tree_t *expr);

/* sno_parse_ast — parse a SNOBOL4 file and return an AST_PROGRAM directly.
 * If code_out is non-NULL, a minimal CODE_t stub is also returned for
 * --dump-ir-bison (parse_expr_pat_from_str internal use only). */
tree_t       *sno_parse_ast(FILE *f, const char *filename, CODE_t **code_out);

/* ---- error ---- */
void sno_error(int lineno, const char *fmt, ...);
extern int   sno_nerrors;
extern char *yyfilename;
extern int   lineno_stmt;

#endif
