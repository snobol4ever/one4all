#ifndef ICN_VALUE_H
#define ICN_VALUE_H
#include "../ast/ast.h"
#include "snobol4.h"
/*----------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------*/
DESCR_t bb_eval_value(tree_t *e);
DESCR_t icn_str_concat_d(DESCR_t a, DESCR_t b);
DESCR_t icn_lconcat_d(DESCR_t a, DESCR_t b);
extern unsigned long bb_icn_rnd_seed;
#endif
