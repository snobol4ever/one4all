#ifndef ICN_VALUE_H
#define ICN_VALUE_H
#include "../ast/ast.h"
#include "snobol4.h"
/*----------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------*/
/* Surviving DESCR_t-input helpers (definitions live in icn_runtime.c after DAI-5b-1).               */
/* bb_eval_value declaration removed in DAI-5b-2; the Icon-specific tree_t* AST walker is gone.       */
/*----------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------*/
DESCR_t icn_str_concat_d(DESCR_t a, DESCR_t b);
DESCR_t icn_lconcat_d(DESCR_t a, DESCR_t b);
extern unsigned long bb_icn_rnd_seed;
#endif
