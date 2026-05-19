#include <string.h>
#include <stdint.h>
#include <stdlib.h>
#include <stdio.h>
#include "emit_ir.h"
#include "emit_core.h"
/*----------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------*/
int ir_node_id(IR_t * nd) { return (int)((uintptr_t)nd % 100000u); }
/*----------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------*/
int ir_is_generator(IR_e k) {
    if (k >= IR_PAT_LIT   && k <= IR_PAT_CALLOUT)  return 1;
    if (k >= IR_PL_CHOICE && k <= IR_PL_CALL)      return 1;
    if (k >= IR_ICN_TO    && k <= IR_ICN_PROC_GEN) return 1;
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
typedef struct { IR_emit_vtable_t * vt; FILE * out; } emit_walk_ctx_t;
static void emit_visit_node(IR_t * nd, void * ctx) {
    emit_walk_ctx_t * c = (emit_walk_ctx_t *)ctx;
    if (ir_is_generator(nd->t)) {
        if (c->vt->emit_generator) c->vt->emit_generator(nd, c->out);
        else fprintf(c->out, "; [%s stub generator kind=%d]\n", c->vt->target_name, (int)nd->t);
    } else {
        if (c->vt->emit_scalar)    c->vt->emit_scalar(nd, c->out);
        else fprintf(c->out, "; [%s stub scalar kind=%d]\n", c->vt->target_name, (int)nd->t);
    }
}
/*----------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------*/
int emit_ir_block(IR_block_t * cfg, FILE * out, const char * target) {
    if (!out || !target) return 1;
    IR_emit_vtable_t * vt = NULL;
    if (strcmp(target, "x86")  == 0) vt = &g_emit_vtable_x86;
    else if (strcmp(target, "jvm")  == 0) vt = &g_emit_vtable_jvm;
    else if (strcmp(target, "js")   == 0) vt = &g_emit_vtable_js;
    else if (strcmp(target, "wasm") == 0) vt = &g_emit_vtable_wasm;
    else if (strcmp(target, "net")  == 0) vt = &g_emit_vtable_net;
    else if (strcmp(target, "c")    == 0) vt = &g_emit_vtable_c;
    if (!vt) { fprintf(out, "; emit_ir_block: unknown target '%s'\n", target); return 1; }
    if (emit_prologue(cfg, out) != 0) return 1;
    if (cfg && cfg->entry) {
        emit_walk_ctx_t ctx = { vt, out };
        ir_walk(cfg, emit_visit_node, &ctx);
    }
    if (emit_epilogue(cfg, out) != 0) return 1;
    return 0;
}
