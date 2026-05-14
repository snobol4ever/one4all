/* lower_pat_dcg.h -- build ir_graph_t from SNOBOL4 pattern tree_t (LR-S1)
 * AUTHORS: Lon Jones Cherryholmes · Claude Sonnet 4.6 (LR-S1, 2026-05-14) */
#pragma once
#ifndef LOWER_PAT_DCG_H
#define LOWER_PAT_DCG_H
#include "../../runtime/common/scrip_ir.h"
#include "../ast/ast.h"
/* Build ir_graph_t DCG from pat_tree. Returns NULL if any node kind is
 * unsupported (caller falls back to existing bb_node_t path). */
ir_graph_t * lower_pat_build_dcg(const tree_t * pat_tree);
#endif /* LOWER_PAT_DCG_H */
