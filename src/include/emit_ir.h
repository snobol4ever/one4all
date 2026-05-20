#pragma once
#ifndef EMIT_IR_H
#define EMIT_IR_H
#include <stdio.h>
#include "BB.h"
#include "../ast/ast.h"
/*----------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------*/
/* bb_node_id — stable integer identity for a DCG node (pointer mod 100000). */
int  bb_node_id(BB_t * nd);
/*----------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------*/
/* bb_is_generator — 1 for all generator/pattern node kinds, 0 for scalar. */
int  bb_is_generator(BB_op_t k);
/*----------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------*/
/* bb_walk — DFS pre-order over all reachable BB_t nodes from cfg->entry, visiting each exactly once. */
void bb_walk(BB_graph_t * cfg, void (*visit)(BB_t * nd, void * ctx), void * ctx);
#endif
