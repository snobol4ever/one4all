#ifndef PARSER_OUTPUT_H
#define PARSER_OUTPUT_H
#include <stdint.h>
#include "ast.h"
/*----------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------*/
/* ParserOutput — the single package that crosses the parser -> lower boundary.                                                                                                                       */
/* Owned at the parser/driver layer; freed by parser_output_free once lower() has consumed it (IEP-7).  lower() borrows the contents and may not retain pointers past return.                          */
/*                                                                                                                                                                                                    */
/* Fields:                                                                                                                                                                                            */
/*   prog       AST root.  Storage owned by code_free (ast_clone.c).  Read-only to lower.                                                                                                              */
/*   lang_mask  Bitset of (1u << LANG_*) for every language present in prog.  Derived from polyglot_lang_mask(prog).                                                                                   */
/*                                                                                                                                                                                                    */
/* What is NOT yet a field (Design B, intentional): the three pre-lower side-tables (label_table, proc_table, g_pl_pred_table) remain file-scope globals because they are read at runtime by emitters  */
/* and the interpreter (~121 read sites).  Migrating them into this struct is a follow-up rung (Design A) once IR-CD-5 lands.  At that point the build function below sets the struct fields and the   */
/* globals turn into thin wrappers around po->{label,proc,pl}_table.                                                                                                                                  */
typedef struct ParserOutput {
    const tree_t *prog;
    uint32_t      lang_mask;
} ParserOutput;
/*----------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------*/
/* Build the package from a parsed AST.  Side-effects: populates the file-scope label_table, proc_table, and g_pl_pred_table.  Calls label_table_build, prescan_defines, polyglot_lang_mask, and       */
/* polyglot_init in that order.  Returns a value (not a pointer) — the package is two machine words; no allocation.                                                                                    */
ParserOutput parser_output_build(const tree_t *prog);
#endif
