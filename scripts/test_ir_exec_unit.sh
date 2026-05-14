#!/usr/bin/env bash
# test_ir_exec_unit.sh — standalone unit test for ir_exec_once / ir_exec_pump (LR-2)
# Builds a small C driver that constructs ir_graph_t instances by hand,
# wires ports directly, and drives them through ir_exec_once / ir_exec_pump.
# No scrip binary, no corpus, no oracle required.
# PASS/FAIL printed per case; exits 0 if all pass, 1 otherwise.
set -euo pipefail
HERE="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
ROOT="$(cd "$HERE/.." && pwd)"
SRC="$ROOT/src"
COMMON="$SRC/runtime/common"
X86="$SRC/runtime/x86"

DRIVER=/tmp/ir_exec_unit_driver.c
BIN=/tmp/ir_exec_unit

cat > "$DRIVER" << 'CEOF'
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
/* Pull in the IR types and exec functions directly */
#include "scrip_ir.h"
#include "ir_exec.h"

static int g_pass = 0;
static int g_fail = 0;

static void check(const char * name, int ok) {
    if (ok) { printf("  PASS %s\n", name); g_pass++; }
    else     { printf("  FAIL %s\n", name); g_fail++; }
}

/* ── helper: wire a single self-evaluating node as a trivial graph ── */
static ir_graph_t * single_node(ir_kind_t kind, int lang) {
    ir_graph_t * cfg = ir_graph_alloc(4, lang);
    ir_node_t  * nd  = ir_alloc_node(cfg, kind, lang);
    /* terminal: port_succ = NULL means "done, value is in nd->value" */
    nd->port_start  = nd;   /* entry drives eval_node directly */
    nd->port_succ   = NULL; /* terminal success */
    nd->port_fail   = NULL; /* terminal fail    */
    nd->port_resume = NULL;
    cfg->entry = nd;
    return cfg;
}

/* ── helper: collect pump values ── */
static int64_t g_collected[64];
static int     g_ncollected;
static int pump_collect(DESCR_t v, void * ctx) {
    (void)ctx;
    if (g_ncollected < 64) g_collected[g_ncollected++] = v.i;
    return 0; /* keep going */
}

int main(void) {
    printf("ir_exec unit test (LR-2)\n\n");

    /* ── Test 1: IR_LIT_I — integer literal ── */
    {
        ir_graph_t * cfg = single_node(IR_LIT_I, IR_LANG_SNO);
        cfg->entry->ival = 42;
        DESCR_t v = ir_exec_once(cfg);
        check("IR_LIT_I value=42", v.v == DT_I && v.i == 42);
        ir_graph_free(cfg);
    }

    /* ── Test 2: IR_LIT_S — string literal ── */
    {
        ir_graph_t * cfg = single_node(IR_LIT_S, IR_LANG_SNO);
        cfg->entry->sval = "hello";
        DESCR_t v = ir_exec_once(cfg);
        check("IR_LIT_S value=hello", v.v == DT_S && strcmp(v.s, "hello") == 0);
        ir_graph_free(cfg);
    }

    /* ── Test 3: IR_LIT_NUL — null ── */
    {
        ir_graph_t * cfg = single_node(IR_LIT_NUL, IR_LANG_SNO);
        DESCR_t v = ir_exec_once(cfg);
        check("IR_LIT_NUL value=null", v.v == DT_SNUL);
        ir_graph_free(cfg);
    }

    /* ── Test 4: IR_FAIL — always fails ── */
    {
        ir_graph_t * cfg = ir_graph_alloc(4, IR_LANG_SNO);
        ir_node_t  * nd  = ir_alloc_node(cfg, IR_FAIL, IR_LANG_SNO);
        nd->port_start  = nd;
        nd->port_fail   = NULL;
        nd->port_succ   = NULL;
        nd->port_resume = NULL;
        cfg->entry = nd;
        DESCR_t v = ir_exec_once(cfg);
        check("IR_FAIL returns FAILDESCR", IS_FAIL_fn(v));
        ir_graph_free(cfg);
    }

    /* ── Test 5: IR_SUCCEED — always succeeds with null ── */
    {
        ir_graph_t * cfg = single_node(IR_SUCCEED, IR_LANG_SNO);
        DESCR_t v = ir_exec_once(cfg);
        check("IR_SUCCEED value=null", v.v == DT_SNUL);
        ir_graph_free(cfg);
    }

    /* ── Test 6: two-node chain — LIT_I → SUCCEED (port_succ wired) ── */
    {
        ir_graph_t * cfg = ir_graph_alloc(8, IR_LANG_ICN);
        ir_node_t * lit  = ir_alloc_node(cfg, IR_LIT_I, IR_LANG_ICN);
        ir_node_t * succ = ir_alloc_node(cfg, IR_SUCCEED, IR_LANG_ICN);
        lit->ival        = 99;
        lit->port_start  = lit;
        lit->port_succ   = succ;
        lit->port_fail   = NULL;
        lit->port_resume = NULL;
        succ->port_start  = succ;
        succ->port_succ   = NULL;
        succ->port_fail   = NULL;
        succ->port_resume = NULL;
        cfg->entry = lit;
        DESCR_t v = ir_exec_once(cfg);
        /* walker: lit evaluates → port_succ = succ → succ evaluates → port_succ = NULL terminal */
        check("chain LIT_I->SUCCEED terminal=null", v.v == DT_SNUL);
        ir_graph_free(cfg);
    }

    /* ── Test 7: IR_TO_BY range 1..3 via ir_exec_pump ── */
    {
        ir_graph_t * cfg  = ir_graph_alloc(16, IR_LANG_ICN);
        ir_node_t * from  = ir_alloc_node(cfg, IR_LIT_I, IR_LANG_ICN);
        ir_node_t * to    = ir_alloc_node(cfg, IR_LIT_I, IR_LANG_ICN);
        ir_node_t * toby  = ir_alloc_node(cfg, IR_TO_BY, IR_LANG_ICN);
        from->ival = 1; from->port_start = from; from->port_succ = NULL;
        to->ival   = 3; to->port_start   = to;   to->port_succ   = NULL;
        /* pre-set values directly so TO_BY can read them */
        from->value = INTVAL(1);
        to->value   = INTVAL(3);
        /* wire TO_BY children */
        toby->c = malloc(2 * sizeof(ir_node_t *));
        toby->c[0] = from; toby->c[1] = to; toby->n = 2;
        toby->port_start  = toby;
        toby->port_succ   = NULL;  /* terminal per value */
        toby->port_resume = toby;  /* back-edge: self-resume */
        toby->port_fail   = NULL;
        cfg->entry = toby;
        g_ncollected = 0;
        int ticks = ir_exec_pump(cfg, pump_collect, NULL);
        check("IR_TO_BY 1..3 tick=3", ticks == 3);
        check("IR_TO_BY values 1,2,3",
              g_ncollected == 3 &&
              g_collected[0] == 1 && g_collected[1] == 2 && g_collected[2] == 3);
        ir_graph_free(cfg);   /* ir_graph_free handles toby->c via nd->c free loop */
    }

    /* ── Test 8: ir_graph_reset clears state ── */
    {
        ir_graph_t * cfg = single_node(IR_LIT_I, IR_LANG_SNO);
        cfg->entry->ival = 7;
        ir_exec_once(cfg);
        cfg->entry->state   = 5;
        cfg->entry->counter = 99;
        ir_graph_reset(cfg);
        check("ir_graph_reset clears state",
              cfg->entry->state == 0 && cfg->entry->counter == 0 &&
              IS_FAIL_fn(cfg->entry->value));
        ir_graph_free(cfg);
    }

    /* ── Test 9: ir_graph_print does not crash ── */
    {
        ir_graph_t * cfg = single_node(IR_LIT_S, IR_LANG_SNO);
        cfg->entry->sval = "test";
        ir_graph_print(cfg, stdout);
        check("ir_graph_print no crash", 1);
        ir_graph_free(cfg);
    }

    /* ── Test 10: null cfg to ir_exec_once returns FAILDESCR ── */
    {
        DESCR_t v = ir_exec_once(NULL);
        check("ir_exec_once(NULL)=FAILDESCR", IS_FAIL_fn(v));
    }

    printf("\nPASS=%d FAIL=%d\n", g_pass, g_fail);
    return g_fail > 0 ? 1 : 0;
}
CEOF

# compile: include common/ and x86/ for headers; link with -lm only (no GC — driver is standalone)
gcc -O0 -g -w \
    -I"$COMMON" \
    -I"$X86" \
    -I"$SRC" \
    "$DRIVER" \
    "$COMMON/scrip_ir.c" \
    "$COMMON/ir_exec.c" \
    -lm \
    -o "$BIN" 2>&1

"$BIN"
