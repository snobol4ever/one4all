#ifndef CMPILE_H
#define CMPILE_H
/*
 * CMPILE.h — public API for the SNOBOL4 SIL lex/parser (CMPILE.c)
 *
 * Types:
 *   CMPND_t  — parse/expression node.  stype is a SIL equ.h code (QLITYP,
 *              ILITYP, VARTYP, ADDFN, MPYFN, DOTFN, ...).  children[] are
 *              CMPND_t* sub-nodes.  text/ival/fval carry leaf payloads.
 *
 *   CMPILE_t — compiled statement.  One per SNOBOL4 source statement.
 *              subject/pattern/replacement are CMPND_t* parse trees.
 *              Linked via ->next.
 *
 * Usage:
 *   cmpile_init();                        // once at startup
 *   cmpile_add_include("/some/dir");      // optional -I paths
 *   CMPILE_t *prog = cmpile_file(f, path);
 *   for (CMPILE_t *s = prog; s; s = s->next)
 *       cmpile_print(s, stdout, 0, idx++);
 *   cmpile_free(prog);
 *
 * Authors: Lon Jones Cherryholmes · Claude Sonnet 4.6  2026-04-05
 */

#include <stdio.h>

/* -------------------------------------------------------------------------
 * CMPND_t — parse/expression node
 * stype values are SIL equ.h codes; see CMPILE.c for the full #define list.
 * Key codes: QLITYP=1 ILITYP=2 VARTYP=3 FNCTYP=5 FLITYP=6 ARYTYP=7
 *            ADDFN=201 SUBFN=202 MPYFN=203 DIVFN=204 EXPFN=205 ORFN=206
 *            NAMFN=207 DOLFN=208 PLSFN=301 MNSFN=302 DOTFN=303 INDFN=304
 *            STRFN=305 ATFN=308 KEYFN=310 NEGFN=311 QUESFN=313
 * ------------------------------------------------------------------------- */
typedef struct CMPND_t CMPND_t;
struct CMPND_t {
    int       stype;          /* SIL STYPE code (equ.h) */
    char     *text;           /* token text: var name, literal, operator */
    CMPND_t **children;       /* child nodes (realloc-grown) */
    int       nchildren, nalloc;
    long long ival;           /* integer payload (ILITYP) */
    double    fval;           /* float payload   (FLITYP) */
};

/* -------------------------------------------------------------------------
 * CMPILE_t — compiled statement (one per SNOBOL4 source statement)
 * ------------------------------------------------------------------------- */
typedef struct CMPILE_t CMPILE_t;
struct CMPILE_t {
    char     *label;          /* column-1 label, or NULL */
    CMPND_t  *subject;        /* subject expression */
    CMPND_t  *pattern;        /* pattern expression, or NULL */
    CMPND_t  *replacement;    /* replacement expression, or NULL */
    int       has_eq;         /* 1 if '=' present in source */
    int       is_scan;        /* 1 if binary '?' scan operator */
    char     *go_s;           /* :S(label) or NULL */
    char     *go_f;           /* :F(label) or NULL */
    char     *go_u;           /* :(label)  or NULL */
    int       is_end;         /* 1 if END statement */
    CMPILE_t *next;           /* next statement in program */
};

/* -------------------------------------------------------------------------
 * Lifecycle
 * ------------------------------------------------------------------------- */

/* cmpile_init — call once before any parsing (initialises syntax tables). */
void cmpile_init(void);

/* cmpile_add_include — add a directory to the -INCLUDE search path. */
void cmpile_add_include(const char *path);

/* cmpile_file — parse an open FILE*; returns CMPILE_t linked list.
 * base_path is used to resolve -INCLUDE directives (NULL = stdin/string). */
CMPILE_t *cmpile_file(FILE *f, const char *base_path);

/* cmpile_string — parse a source string; returns CMPILE_t linked list.
 * Primary use: EVAL() builtin. */
CMPILE_t *cmpile_string(const char *src);

/* cmpile_free — free a CMPILE_t linked list (shallow; CMPND_t trees intact). */
void cmpile_free(CMPILE_t *s);

/* -------------------------------------------------------------------------
 * Dump / inspect
 * ------------------------------------------------------------------------- */

/* cmpnd_print_sexp — dump CMPND_t as S-expression.
 * oneline=0: indented pretty-print.  oneline=1: flat one-liner for diff.
 * depth: indent level; callers pass 0 for root. */
void cmpnd_print_sexp(CMPND_t *n, FILE *out, int oneline, int depth);

/* cmpnd_to_expr — lower a CMPND_t parse node to EXPR_t IR (shared IR).
 * Defined in snobol4_pattern.c; declared here so callers can link against it
 * without pulling in the full runtime.  Returns NULL for empty/unknown nodes. */
struct EXPR_t;   /* forward — full definition in scrip_cc.h */
struct EXPR_t *cmpnd_to_expr(CMPND_t *n);

/* cmpile_print — dump one CMPILE_t statement.
 * oneline=0: labelled fields.  oneline=1: single-line S-expression.
 * idx: statement sequence number (caller-supplied, for display). */
void cmpile_print(CMPILE_t *s, FILE *out, int oneline, int idx);

#endif /* CMPILE_H */
CMPND_t *cmpile_eval_expr(const char *src);
