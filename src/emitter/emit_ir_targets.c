#include <stdio.h>
#include "emit_ir.h"
/*----------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------*/
/* Stub prologue used by all unimplemented targets — prints a single comment line. */
static int stub_prologue(IR_block_t * cfg, FILE * out) { (void)cfg; return 0; }
static int stub_epilogue(IR_block_t * cfg, FILE * out) { (void)cfg; return 0; }
/*----------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------*/
/* x86: prologue/epilogue/scalar/generator populated by emit_sm.c / emit_bb.c work (existing path).
   For now all slots are NULL — emit_ir_block prints stub comments for each node.
   Populated by GOAL-IR-EMITTER-PREREQ follow-on work wiring sm_codegen_text here. */
IR_emit_vtable_t g_emit_vtable_x86  = { "x86",  NULL, NULL, stub_prologue, stub_epilogue };
/*----------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------*/
/* jvm: populated by GOAL-SN4-JVM-EMIT. */
IR_emit_vtable_t g_emit_vtable_jvm  = { "jvm",  NULL, NULL, stub_prologue, stub_epilogue };
/*----------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------*/
/* js: populated by GOAL-SN4-JS-EMIT. */
IR_emit_vtable_t g_emit_vtable_js   = { "js",   NULL, NULL, stub_prologue, stub_epilogue };
/*----------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------*/
/* wasm, net, c: future GOALs. */
IR_emit_vtable_t g_emit_vtable_wasm = { "wasm", NULL, NULL, stub_prologue, stub_epilogue };
IR_emit_vtable_t g_emit_vtable_net  = { "net",  NULL, NULL, stub_prologue, stub_epilogue };
IR_emit_vtable_t g_emit_vtable_c    = { "c",    NULL, NULL, stub_prologue, stub_epilogue };
