#pragma once
#ifndef LOWER_PAT_DCG_H
#define LOWER_PAT_DCG_H
#include "scrip_ir.h"
#include "../ast/ast.h"
/*----------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------*/
IR_block_t * IR_lower_pat(const tree_t * pat_tree);
#endif
