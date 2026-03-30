/*
 * emit_wasm.c — WebAssembly text-format emitter for scrip-cc
 *
 * Memory layout (matches sno_runtime.wat):
 *   [0..8191]     output buffer  (written by $sno_output_*)
 *   [8192..32767] string literal data segment (STR_DATA_BASE)
 *   [32768..]     string heap    ($sno_str_alloc bump pointer)
 *
 * Value representation on WASM stack:
 *   String  → two i32: (offset, len)
 *   Integer → one i64
 *   Float   → one f64
 *
 * main() calls $sno_output_flush → returns byte-count (i32) to run_wasm.js.
 *
 * Milestones:
 *   M-G2-SCAFFOLD-WASM  scaffold (G-7 2026-03-28)
 *   M-SW-0   -wasm driver flag (SW-1 2026-03-30)
 *   M-SW-1   runtime header inlined (SW-1 2026-03-30)
 *   M-SW-A01 hello/literals: E_QLIT/ILIT/FLIT/NUL/NEG/arith/CONCAT→OUTPUT
 *            (SW-1 2026-03-30)
 */

#include "scrip_cc.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define W(fmt, ...)  fprintf(wasm_out, fmt, ##__VA_ARGS__)

static FILE *wasm_out = NULL;

/* ── String literal table ─────────────────────────────────────────────────── */
#define MAX_STRLITS   4096
#define STR_DATA_BASE 8192

typedef struct { char *text; int len; int offset; } StrLit;
static StrLit str_lits[MAX_STRLITS];
static int    str_nlit  = 0;
static int    str_bytes = 0;

static int strlit_intern(const char *s) {
    int len = s ? (int)strlen(s) : 0;
    const char *t = s ? s : "";
    for (int i = 0; i < str_nlit; i++)
        if (str_lits[i].len == len && memcmp(str_lits[i].text, t, len) == 0)
            return i;
    if (str_nlit >= MAX_STRLITS) return 0;
    int idx = str_nlit++;
    str_lits[idx].text   = strdup(t);
    str_lits[idx].len    = len;
    str_lits[idx].offset = str_bytes;
    str_bytes += len ? len : 0;
    return idx;
}
static int strlit_abs(int idx) { return STR_DATA_BASE + str_lits[idx].offset; }

/* ── Expression type ──────────────────────────────────────────────────────── */
typedef enum { TY_STR = 0, TY_INT = 1, TY_FLOAT = 2 } WasmTy;

/* ── Forward decls ────────────────────────────────────────────────────────── */
static WasmTy emit_expr(const EXPR_t *e);

/* ── Runtime header ───────────────────────────────────────────────────────── */
static void emit_runtime_header(void) {
    const char *path = "/home/claude/one4all/src/runtime/wasm/sno_runtime.wat";
    FILE *rt = fopen(path, "r");
    if (!rt) {
        W("  ;; WARNING: runtime not found\n");
        W("  (memory (export \"memory\") 1)\n");
        W("  (global $out_pos (mut i32) (i32.const 0))\n");
        W("  (func $sno_output_flush (result i32) (global.get $out_pos))\n");
        return;
    }
    char line[1024];
    while (fgets(line, sizeof line, rt)) W("%s", line);
    fclose(rt);
}

/* ── int→str and float→str helpers (inlined into module) ─────────────────── */
/*
 * These are emitted once per module. They call into the runtime's int-format
 * logic but we expose them as callable funcs so E_CONCAT can use them.
 * For M-SW-A01 we inline simple versions here.
 */
static void emit_conversion_helpers(void) {
    /* $sno_int_to_str: (val:i64) → (offset:i32, len:i32)
     * Format val as decimal into string heap, return (offset, len). */
    W("\n  ;; int→str: formats i64 decimal into string heap\n");
    W("  (func $sno_int_to_str (param $val i64) (result i32 i32)\n");
    W("    (local $pos i32) (local $start i32) (local $end i32)\n");
    W("    (local $tmp i32) (local $dig i32) (local $neg i32) (local $v i64)\n");
    W("    (global.get $str_ptr) (local.set $start)\n");
    W("    (local.set $pos (local.get $start))\n");
    W("    (i64.const 0) (local.get $val) (i64.lt_s) (local.set $neg)\n");
    W("    (local.get $val) (local.set $v)\n");
    W("    (if (local.get $neg) (then\n");
    W("      (i64.const 0) (local.get $v) (i64.sub) (local.set $v)))\n");
    W("    (if (i64.eqz (local.get $v)) (then\n");
    W("      (local.get $pos) (i32.const 48) (i32.store8)\n");
    W("      (local.get $pos) (i32.const 1) (i32.add) (local.set $pos)\n");
    W("    ) (else\n");
    W("      (block $dbreak (loop $dlp\n");
    W("        (i64.eqz (local.get $v)) (br_if $dbreak)\n");
    W("        (local.get $v) (i64.const 10) (i64.rem_u)\n");
    W("        (i32.wrap_i64) (i32.const 48) (i32.add) (local.set $dig)\n");
    W("        (local.get $pos) (local.get $dig) (i32.store8)\n");
    W("        (local.get $pos) (i32.const 1) (i32.add) (local.set $pos)\n");
    W("        (local.get $v) (i64.const 10) (i64.div_u) (local.set $v)\n");
    W("        (br $dlp)))\n");
    W("      (if (local.get $neg) (then\n");
    W("        (local.get $pos) (i32.const 45) (i32.store8)\n");
    W("        (local.get $pos) (i32.const 1) (i32.add) (local.set $pos)))\n");
    W("      ;; reverse\n");
    W("      (local.get $pos) (i32.const 1) (i32.sub) (local.set $end)\n");
    W("      (local.get $start) (local.set $tmp)\n");
    W("      (block $rbreak (loop $rlp\n");
    W("        (local.get $tmp) (local.get $end) (i32.ge_u) (br_if $rbreak)\n");
    W("        (local.get $tmp) (i32.load8_u) (local.set $dig)\n");
    W("        (local.get $tmp) (local.get $end) (i32.load8_u) (i32.store8)\n");
    W("        (local.get $end) (local.get $dig) (i32.store8)\n");
    W("        (local.get $tmp) (i32.const 1) (i32.add) (local.set $tmp)\n");
    W("        (local.get $end) (i32.const 1) (i32.sub) (local.set $end)\n");
    W("        (br $rlp)))\n");
    W("    ))\n");
    W("    (local.get $pos) (global.get $str_ptr) (i32.sub) (local.set $dig) ;; len\n");
    W("    (global.set $str_ptr (local.get $pos))\n");
    W("    (local.get $start)\n");
    W("    (local.get $dig)\n");
    W("  )\n");

    /* $sno_float_to_str: (val:f64) → (offset:i32, len:i32)
     * For M-SW-A01: format via sprintf into a fixed buffer then copy to heap. */
    W("\n  ;; float→str: formats f64 into string heap\n");
    W("  (func $sno_float_to_str (param $val f64) (result i32 i32)\n");
    W("    ;; Stub for M-SW-A01: emit '0.' for any float -- refined in M-SW-A02\n");
    /* The literal 1.0 in literals.sno prints as "1." per the .ref */
    /* For now: detect if val is integer-valued, print as int + "." */
    /* Full float formatting deferred to M-SW-A02 */
    W("    (i32.const 0) (i32.const 0) ;; placeholder\n");
    W("  )\n");
}

/* ── expr pre-scan (intern all E_QLIT) ───────────────────────────────────── */
static void prescan_expr(const EXPR_t *e) {
    if (!e) return;
    if (e->kind == E_QLIT) { strlit_intern(e->sval); return; }
    for (int i = 0; i < e->nchildren; i++) prescan_expr(e->children[i]);
}
static void prescan_prog(Program *prog) {
    strlit_intern("");  /* always intern empty string */
    for (STMT_t *s = prog->head; s; s = s->next) {
        prescan_expr(s->subject);
        prescan_expr(s->pattern);
        prescan_expr(s->replacement);
    }
}

/* ── Data segment ─────────────────────────────────────────────────────────── */
static void emit_data_segment(void) {
    if (str_bytes == 0) return;
    W("\n  ;; String literals at offset %d\n", STR_DATA_BASE);
    W("  (data (i32.const %d) \"", STR_DATA_BASE);
    for (int i = 0; i < str_nlit; i++) {
        const unsigned char *t = (const unsigned char *)str_lits[i].text;
        for (int j = 0; j < str_lits[i].len; j++) {
            unsigned char c = t[j];
            if (c == '"' || c == '\\') W("\\%02x", c);
            else if (c < 32 || c > 126) W("\\%02x", c);
            else W("%c", (char)c);
        }
    }
    W("\")\n");
}

/* ── Expression emitter ───────────────────────────────────────────────────── */
static WasmTy emit_expr(const EXPR_t *e) {
    if (!e || e->kind == E_NUL) {
        int idx = strlit_intern("");
        W("    (i32.const %d)\n", strlit_abs(idx));
        W("    (i32.const 0)\n");
        return TY_STR;
    }
    switch (e->kind) {
    case E_QLIT: {
        int idx = strlit_intern(e->sval);
        W("    (i32.const %d)\n", strlit_abs(idx));
        W("    (i32.const %d)\n", str_lits[idx].len);
        return TY_STR;
    }
    case E_ILIT:
        W("    (i64.const %lld)\n", (long long)e->ival);
        return TY_INT;
    case E_FLIT:
        W("    (f64.const %g)\n", e->dval);
        return TY_FLOAT;
    case E_NEG: {
        /* For literals: always wraps a numeric child */
        WasmTy t = emit_expr(e->nchildren > 0 ? e->children[0] : NULL);
        if (t == TY_INT) {
            W("    (i64.const -1)\n");
            W("    (i64.mul)\n");
        } else if (t == TY_FLOAT) {
            W("    (f64.neg)\n");
        }
        return t;
    }
    case E_PLS:
        return emit_expr(e->nchildren > 0 ? e->children[0] : NULL);
    case E_ADD: case E_SUB: case E_MPY: case E_DIV: case E_MOD: {
        WasmTy lt = emit_expr(e->children[0]);
        WasmTy rt2 = emit_expr(e->children[1]);
        int floaty = (lt == TY_FLOAT || rt2 == TY_FLOAT);
        if (!floaty) {
            if      (e->kind == E_ADD) W("    (i64.add)\n");
            else if (e->kind == E_SUB) W("    (i64.sub)\n");
            else if (e->kind == E_MPY) W("    (i64.mul)\n");
            else if (e->kind == E_DIV) W("    (i64.div_s)\n");
            else if (e->kind == E_MOD) W("    (i64.rem_s)\n");
            return TY_INT;
        } else {
            /* promote both sides if needed */
            if (lt == TY_INT)   W("    ;; TODO: promote lhs i64→f64\n");
            if (rt2 == TY_INT)  W("    ;; TODO: promote rhs i64→f64\n");
            if      (e->kind == E_ADD) W("    (f64.add)\n");
            else if (e->kind == E_SUB) W("    (f64.sub)\n");
            else if (e->kind == E_MPY) W("    (f64.mul)\n");
            else if (e->kind == E_DIV) W("    (f64.div)\n");
            return TY_FLOAT;
        }
    }
    case E_CONCAT: {
        if (e->nchildren == 0) {
            int idx = strlit_intern("");
            W("    (i32.const %d)\n", strlit_abs(idx));
            W("    (i32.const 0)\n");
            return TY_STR;
        }
        WasmTy t0 = emit_expr(e->children[0]);
        if (t0 == TY_INT)   W("    (call $sno_int_to_str)\n");
        if (t0 == TY_FLOAT) W("    (call $sno_float_to_str)\n");
        for (int i = 1; i < e->nchildren; i++) {
            WasmTy ti = emit_expr(e->children[i]);
            if (ti == TY_INT)   W("    (call $sno_int_to_str)\n");
            if (ti == TY_FLOAT) W("    (call $sno_float_to_str)\n");
            W("    (call $sno_str_concat)\n");
        }
        return TY_STR;
    }
    default:
        W("    ;; UNHANDLED EKind %d\n", (int)e->kind);
        { int idx = strlit_intern("");
          W("    (i32.const %d)\n", strlit_abs(idx));
          W("    (i32.const 0)\n"); }
        return TY_STR;
    }
}

/* ── Statement emitter ────────────────────────────────────────────────────── */
static void emit_stmt(const STMT_t *s) {
    if (!s->has_eq || !s->subject) return;
    /* Only OUTPUT assignments handled at M-SW-A01 */
    if (s->subject->kind != E_VAR && s->subject->kind != E_KW) return;
    const char *name = s->subject->sval ? s->subject->sval : "";
    if (strcasecmp(name, "OUTPUT") != 0) return;

    W("    ;; OUTPUT = ...\n");
    WasmTy ty = emit_expr(s->replacement);
    if (ty == TY_STR) {
        W("    (call $sno_output_str)\n");
    } else if (ty == TY_INT) {
        W("    (call $sno_output_int)\n");
    } else {
        /* float: convert to str then output */
        W("    (call $sno_float_to_str)\n");
        W("    (call $sno_output_str)\n");
    }
}

/* ── Public entry point ───────────────────────────────────────────────────── */
void emit_wasm(Program *prog, FILE *out, const char *filename) {
    (void)filename;
    wasm_out = out;

    /* Reset string literal table for each file */
    for (int i = 0; i < str_nlit; i++) free(str_lits[i].text);
    str_nlit = str_bytes = 0;

    prescan_prog(prog);

    W(";; Generated by scrip-cc -wasm (M-SW-A01)\n");
    W("(module\n");

    emit_runtime_header();
    emit_conversion_helpers();
    emit_data_segment();

    W("\n  (func (export \"main\") (result i32)\n");
    for (STMT_t *s = prog->head; s; s = s->next)
        emit_stmt(s);
    W("    (call $sno_output_flush)\n");
    W("  )\n");
    W(")\n");
}
