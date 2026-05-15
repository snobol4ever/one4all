#include <string.h>
#include <stdint.h>
#include "emit_ir.h"
/*----------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------*/
int ir_node_id(IR_t * nd) { return (int)((uintptr_t)nd % 100000u); }
/*----------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------*/
int emit_ir_block(IR_block_t * cfg, FILE * out, const char * target) {
    if (!cfg || !out || !target) return 1;
    if (strcmp(target, "jvm")  == 0) return emit_ir_block_jvm(cfg, out);
    if (strcmp(target, "js")   == 0) return emit_ir_block_js (cfg, out);
    if (strcmp(target, "x86")  == 0) return emit_ir_block_x86(cfg, out);
    if (strcmp(target, "net")  == 0) return emit_ir_block_net(cfg, out);
    if (strcmp(target, "wasm") == 0) return emit_ir_block_wasm(cfg, out);
    if (strcmp(target, "c")    == 0) return emit_ir_block_c  (cfg, out);
    fprintf(out, "; emit_ir_block: unknown target '%s'\n", target);
    return 1;
}
/*----------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------*/
int emit_ir_block_jvm (IR_block_t * cfg, FILE * out) { (void)cfg; fprintf(out, "; JVM stub\n");  return 0; }
int emit_ir_block_js  (IR_block_t * cfg, FILE * out) { (void)cfg; fprintf(out, "// JS stub\n");  return 0; }
int emit_ir_block_x86 (IR_block_t * cfg, FILE * out) { (void)cfg; fprintf(out, "; x86 stub\n"); return 0; }
int emit_ir_block_net (IR_block_t * cfg, FILE * out) { (void)cfg; fprintf(out, "// .NET stub\n"); return 0; }
int emit_ir_block_wasm(IR_block_t * cfg, FILE * out) { (void)cfg; fprintf(out, ";; WASM stub\n"); return 0; }
int emit_ir_block_c   (IR_block_t * cfg, FILE * out) { (void)cfg; fprintf(out, "/* C stub */\n"); return 0; }
