#include <string.h>
#include <stdint.h>
#include <stdlib.h>
#include "emit_ir.h"
/*----------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------*/
int ir_node_id(IR_t * nd) { return (int)((uintptr_t)nd % 100000u); }
/*----------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------*/
int ir_is_generator(IR_e k) {
    if (k >= IR_PAT_LIT    && k <= IR_PAT_CALLOUT)  return 1;
    if (k >= IR_PL_CHOICE  && k <= IR_PL_CALL)      return 1;
    if (k >= IR_ICN_TO     && k <= IR_ICN_PROC_GEN) return 1;
    if (k == IR_SCAN || k == IR_ALTERNATE || k == IR_TO_BY ||
        k == IR_EVERY || k == IR_WHILE    || k == IR_LIMIT || k == IR_SUSPEND) return 1;
    return 0;
}
/*----------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------*/
#define IR_WALK_MAX 4096
static int g_visited[IR_WALK_MAX];
static int g_vcount = 0;
static void ir_walk_rec(IR_t * nd, void (*visit)(IR_t *, void *), void * ctx) {
    if (!nd) return;
    int id = ir_node_id(nd);
    for (int i = 0; i < g_vcount; i++) if (g_visited[i] == id) return;
    if (g_vcount < IR_WALK_MAX) g_visited[g_vcount++] = id;
    visit(nd, ctx);
    ir_walk_rec(nd->α, visit, ctx);
    ir_walk_rec(nd->β, visit, ctx);
    ir_walk_rec(nd->γ, visit, ctx);
    ir_walk_rec(nd->ω, visit, ctx);
    if (nd->c) for (int i = 0; i < nd->n; i++) ir_walk_rec(nd->c[i], visit, ctx);
}
/*----------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------*/
void ir_walk(IR_block_t * cfg, void (*visit)(IR_t *, void *), void * ctx) {
    if (!cfg || !cfg->entry) return;
    g_vcount = 0;
    ir_walk_rec(cfg->entry, visit, ctx);
}
/*----------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------*/
int emit_ir_block(IR_block_t * cfg, FILE * out, const char * target) {
    if (!out || !target) return 1;
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
int emit_ir_block_jvm (IR_block_t * cfg, FILE * out) { (void)cfg; fprintf(out, "; JVM stub\n");    return 0; }
int emit_ir_block_js  (IR_block_t * cfg, FILE * out) { (void)cfg; fprintf(out, "// JS stub\n");    return 0; }
int emit_ir_block_x86 (IR_block_t * cfg, FILE * out) { (void)cfg; fprintf(out, "; x86 stub\n");   return 0; }
int emit_ir_block_net (IR_block_t * cfg, FILE * out) { (void)cfg; fprintf(out, "// .NET stub\n");  return 0; }
int emit_ir_block_wasm(IR_block_t * cfg, FILE * out) { (void)cfg; fprintf(out, ";; WASM stub\n");  return 0; }
int emit_ir_block_c   (IR_block_t * cfg, FILE * out) { (void)cfg; fprintf(out, "/* C stub */\n");  return 0; }
