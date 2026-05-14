#ifndef LOWER_ICN_H
#define LOWER_ICN_H
#include "scrip_ir.h"
/* Build compile-time DCG for Icon upto(cset, str) with scalar args. */
IR_block_t *lower_icn_upto(const char *cset, const char *hay);
#endif
