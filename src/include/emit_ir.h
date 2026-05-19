#pragma once
#ifndef EMIT_IR_H
#define EMIT_IR_H
#include <stdio.h>
#include "IR.h"
#include "../ast/ast.h"
/*----------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------*/
/* ir_node_id — stable integer identity for a DCG node (pointer mod 100000). */
int  ir_node_id(IR_t * nd);
/*----------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------*/
/* ir_is_generator — 1 for all generator/pattern node kinds, 0 for scalar. */
int  ir_is_generator(IR_e k);
/*----------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------*/
/* ir_walk — DFS pre-order over all reachable IR_t nodes from cfg->entry, visiting each exactly once. */
void ir_walk(IR_block_t * cfg, void (*visit)(IR_t * nd, void * ctx), void * ctx);
#endif
