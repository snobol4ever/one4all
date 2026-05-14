#ifndef LOWER_ICN_H
#define LOWER_ICN_H
#include "scrip_ir.h"
#include "../processor/bb_box.h"
typedef struct { bb_node_t gen[2]; int which; } icn_alt_dcg_t;
typedef struct { bb_node_t gen; int64_t max; int64_t count; } icn_lim_dcg_t;
/* Build compile-time DCG for Icon upto(cset, str) with scalar args. */
IR_block_t *lower_icn_upto(const char *cset, const char *hay);
IR_block_t *lower_icn_to(int64_t lo, int64_t hi);
IR_block_t *lower_icn_every(bb_node_t *gen, void *body);
IR_block_t *lower_icn_to_by(int64_t lo, int64_t hi, int64_t step);
IR_block_t *lower_icn_iterate(const char *str, int64_t len);
IR_block_t *lower_icn_alternate(bb_node_t left, bb_node_t right);
IR_block_t *lower_icn_limit(bb_node_t gen, int64_t max);
#endif
