#ifndef LOWER_ICN_H
#define LOWER_ICN_H
#include "scrip_ir.h"
#include "../processor/bb_box.h"
/* Build compile-time DCG for Icon upto(cset, str) with scalar args. */
IR_block_t *lower_icn_upto(const char *cset, const char *hay);
IR_block_t *lower_icn_to(int64_t lo, int64_t hi);
IR_block_t *lower_icn_every(bb_node_t *gen, void *body);
#endif
