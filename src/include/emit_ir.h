#pragma once
#ifndef EMIT_IR_H
#define EMIT_IR_H
#include <stdio.h>
#include "BB.h"
#include "../ast/ast.h"
/*----------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------*/
/* ir_node_id — stable integer identity for a DCG node (pointer mod 100000). */
int  ir_node_id(BB_t * nd);
/*----------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------*/
/* ir_is_generator — 1 for all generator/pattern node kinds, 0 for scalar. */
int  ir_is_generator(BB_op_t k);
/*----------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------*/
/* ir_walk — DFS pre-order over all reachable BB_t nodes from cfg->entry, visiting each exactly once. */
void ir_walk(BB_graph_t * cfg, void (*visit)(BB_t * nd, void * ctx), void * ctx);
#endif
