#pragma once
#ifndef EMIT_IR_H
#define EMIT_IR_H
#include <stdio.h>
#include "scrip_ir.h"
/*----------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------*/
/* emit_ir_block — single entry point for all IR_t-based emitters.
   cfg: IR_block_t from lower. out: destination FILE*. target: "x86","jvm","js","wasm","net","c".
   Returns 0 on success, non-zero on error. */
int  emit_ir_block(IR_block_t * cfg, FILE * out, const char * target);
/*----------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------*/
/* ir_node_id — stable integer identity for a DCG node (pointer mod 100000). */
int  ir_node_id(IR_t * nd);
/*----------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------*/
/* ir_is_generator — 1 for all generator node kinds (IR_PAT_*, IR_ICN_*, IR_PL_*, IR_SCAN etc), 0 for scalar. */
int  ir_is_generator(IR_e k);
/*----------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------*/
/* ir_walk — DFS pre-order over all reachable IR_t nodes from cfg->entry, visiting each exactly once. */
void ir_walk(IR_block_t * cfg, void (*visit)(IR_t * nd, void * ctx), void * ctx);
/*----------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------*/
/* Per-target entry points — x86 wraps sm_codegen_text; jvm/js are stubs until their GOALs complete. */
int emit_ir_block_jvm (IR_block_t * cfg, FILE * out);
int emit_ir_block_js  (IR_block_t * cfg, FILE * out);
int emit_ir_block_x86 (IR_block_t * cfg, FILE * out);
int emit_ir_block_net (IR_block_t * cfg, FILE * out);
int emit_ir_block_wasm(IR_block_t * cfg, FILE * out);
int emit_ir_block_c   (IR_block_t * cfg, FILE * out);
#endif
