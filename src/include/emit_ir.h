#pragma once
#ifndef EMIT_IR_H
#define EMIT_IR_H
#include <stdio.h>
#include "IR.h"
#include "../ast/ast.h"
/*----------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------*/
/* emit_ir_block — ONE entry point for ALL 6 backends × ALL 6 frontends.
   cfg:    IR_block_t produced by lower from any frontend (SNOBOL4, Snocone, Rebus, Icon, Prolog, Raku).
   out:    destination FILE*.
   target: "x86" | "jvm" | "js" | "wasm" | "net" | "c"
   Walks the IR_t DCG once. At each node, dispatches to the correct C template function set for
   the chosen target. No per-target fan-out at this level — target selects a vtable; the walk is shared.
   Returns 0 on success, non-zero on error. */
int  emit_ir_block(IR_block_t * cfg, FILE * out, const char * target);
/*----------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------*/
/* ir_node_id — stable integer identity for a DCG node (pointer mod 100000). */
int  ir_node_id(IR_t * nd);
/*----------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------*/
/* ir_is_generator — 1 for all generator/pattern node kinds, 0 for scalar. */
int  ir_is_generator(IR_e k);
/*----------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------*/
/* ir_walk — DFS pre-order over all reachable IR_t nodes from cfg->entry, visiting each exactly once.
   Handles cycles via visited set keyed on ir_node_id. */
void ir_walk(IR_block_t * cfg, void (*visit)(IR_t * nd, void * ctx), void * ctx);
/*----------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------*/
/* IR_emit_vtable_t — per-target template function set.
   emit_ir_block selects the vtable matching target, then walks the DCG calling these functions.
   Each target (x86, jvm, js, wasm, net, c) provides one populated vtable.
   Unimplemented slots are NULL — emit_ir_block skips them with a stub comment. */
typedef struct {
    const char * target_name;
    int (*emit_scalar)   (IR_t * nd, FILE * out);
    int (*emit_generator)(IR_t * nd, FILE * out);
    int (*emit_prologue) (IR_block_t * cfg, FILE * out);
    int (*emit_epilogue) (IR_block_t * cfg, FILE * out);
} IR_emit_vtable_t;
/*----------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------*/
/* Vtable instances — one per target. Defined in their respective emit_TARGET.c files.
   x86 vtable wraps the existing sm_codegen_text path during transition.
   jvm/js/wasm/net/c vtables are populated by their respective GOALs. */
extern IR_emit_vtable_t g_emit_vtable_x86;
extern IR_emit_vtable_t g_emit_vtable_jvm;
extern IR_emit_vtable_t g_emit_vtable_js;
extern IR_emit_vtable_t g_emit_vtable_wasm;
extern IR_emit_vtable_t g_emit_vtable_net;
extern IR_emit_vtable_t g_emit_vtable_c;
/*----------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------*/
/* emit_js_program — JS-specific entry point. Builds SM_Program from AST, emits JS directly.
   This bypasses the IR walk path for pattern-only treatment.
   Designed for --target=js: handles scalars via SM_Program walk + patterns via IR factories. */
int emit_js_program(const tree_t * ast_prog, FILE * out);
#endif
