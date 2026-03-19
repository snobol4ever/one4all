/*
 * sc_lower.h -- Snocone postfix → EXPR_t/STMT_t IR  (Sprint SC2)
 *
 * Step 3 of the Snocone frontend pipeline:
 *
 *   sc_lex()     -> ScTokenArray    (sc_lex.c)
 *   sc_parse()   -> ScParseResult   (sc_parse.c)
 *   sc_lower()   -> ScLowerResult   (this file)
 *
 * sc_lower() walks the postfix ScPToken[] from sc_parse, evaluates it
 * using an operand stack of EXPR_t*, and assembles STMT_t IR nodes at
 * each SC_NEWLINE boundary.
 *
 * Operator mapping (from bconv table in snocone.sc / JVM snocone_emitter.clj):
 *
 *   SC_ASSIGN    ->  subject=lhs, replacement=rhs  (assignment stmt)
 *   SC_PLUS      ->  E_ADD
 *   SC_MINUS     ->  E_SUB (binary) / E_MNS (unary)
 *   SC_STAR      ->  E_MPY
 *   SC_SLASH     ->  E_DIV
 *   SC_CARET     ->  E_EXPOP
 *   SC_CONCAT    ->  E_CONC  (blank concat — juxtaposition)
 *   SC_OR        ->  E_OR    (pattern alternation || )
 *   SC_PIPE      ->  E_CONC  (single | — also string concat in SNOBOL4)
 *   SC_PERIOD    ->  E_NAM   (conditional capture  .)
 *   SC_DOLLAR    ->  E_DOL   (immediate capture    $)
 *   SC_AT        ->  E_ATP   (@var — cursor position)
 *   SC_AMPERSAND ->  E_KW    (unary & — keyword reference)
 *   SC_TILDE     ->  E_FNC("NOT",1)  (logical negation)
 *   SC_QUESTION  ->  E_FNC("DIFFER",2) or unary: E_FNC("DIFFER",1)
 *   SC_EQ        ->  E_FNC("EQ",2)
 *   SC_NE        ->  E_FNC("NE",2)
 *   SC_LT        ->  E_FNC("LT",2)
 *   SC_GT        ->  E_FNC("GT",2)
 *   SC_LE        ->  E_FNC("LE",2)
 *   SC_GE        ->  E_FNC("GE",2)
 *   SC_STR_IDENT  -> E_FNC("IDENT",2)
 *   SC_STR_DIFFER -> E_FNC("DIFFER",2)
 *   SC_STR_LT    ->  E_FNC("LLT",2)
 *   SC_STR_GT    ->  E_FNC("LGT",2)
 *   SC_STR_LE    ->  E_FNC("LLE",2)
 *   SC_STR_GE    ->  E_FNC("LGE",2)
 *   SC_STR_EQ    ->  E_FNC("LEQ",2)
 *   SC_STR_NE    ->  E_FNC("LNE",2)
 *   SC_PERCENT   ->  E_FNC("REMDR",2)
 *   SC_STAR (unary) -> E_INDR (indirect reference)
 *   SC_CALL      ->  E_FNC(name, nargs)
 *   SC_ARRAY_REF ->  E_IDX(name, nargs)
 *
 * Statement assembly at SC_NEWLINE:
 *   Stack top is the expression for this line.
 *   If the expression root is E_ASGN or the top SC_ASSIGN produced a
 *   two-operand form, split into subject + replacement fields of STMT_t.
 *   Otherwise the expression is the subject field (pattern match or
 *   expression-only statement).
 */

#ifndef SC_LOWER_H
#define SC_LOWER_H

#include "sc_parse.h"
#include "sno2c.h"      /* EXPR_t, STMT_t, Program */

/* ---------------------------------------------------------------------------
 * Lower result
 * ------------------------------------------------------------------------- */
typedef struct {
    Program *prog;      /* heap-allocated IR program; caller frees */
    int      nerrors;   /* number of lowering errors (0 = clean)   */
} ScLowerResult;

/* ---------------------------------------------------------------------------
 * Public API
 *
 * sc_lower(ptoks, count, filename)
 *   Walk the postfix ScPToken array from sc_parse() and produce IR.
 *   filename is used for error messages (may be NULL).
 *   On success: result.nerrors == 0, result.prog != NULL.
 *   On error:   result.nerrors > 0, result.prog may be partial.
 *
 * sc_lower_free(r)
 *   Free the Program (STMT_t/EXPR_t chain) owned by the result.
 * ------------------------------------------------------------------------- */
ScLowerResult sc_lower(const ScPToken *ptoks, int count, const char *filename);
void          sc_lower_free(ScLowerResult *r);

#endif /* SC_LOWER_H */
