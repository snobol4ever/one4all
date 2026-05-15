#pragma once
#ifndef EMIT_IR_H
#define EMIT_IR_H
#include <stdio.h>
#include "scrip_ir.h"
/*----------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------*/
/* emit_ir_block — single entry point for all IR_t-based emitters.
   cfg:    IR_block_t produced by IR_alloc / IR_lower_pat (the DCG).
   out:    destination FILE* for text output.
   target: one of "x86", "jvm", "js", "wasm", "net", "c".
   Returns 0 on success, non-zero on error. */
int emit_ir_block(IR_block_t * cfg, FILE * out, const char * target);
/*----------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------*/
/* ir_node_id — stable integer identity for a DCG node.
   Uses pointer address mod 100000 as surrogate (IR_t has no id field). */
int ir_node_id(IR_t * nd);
/*----------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------*/
/* Per-target stubs — each target implements its own file; stubs return 0. */
int emit_ir_block_jvm(IR_block_t * cfg, FILE * out);
int emit_ir_block_js (IR_block_t * cfg, FILE * out);
int emit_ir_block_x86(IR_block_t * cfg, FILE * out);
int emit_ir_block_net(IR_block_t * cfg, FILE * out);
int emit_ir_block_wasm(IR_block_t * cfg, FILE * out);
int emit_ir_block_c  (IR_block_t * cfg, FILE * out);
#endif
