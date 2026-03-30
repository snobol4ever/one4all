/*
 * emit_wasm_prolog.c — Prolog IR → WebAssembly text-format emitter
 *
 * Consumes E_CHOICE/E_CLAUSE/E_UNIFY/E_CUT/E_TRAIL_MARK/E_TRAIL_UNWIND
 * nodes produced by prolog_lower() and emits a WebAssembly Text (.wat) file.
 *
 * Entry point: prolog_emit_wasm(Program *prog, FILE *out, const char *filename)
 * Called from driver/main.c when -pl -wasm flags are both set.
 *
 * Design:
 *   - Shares emit_wasm.c string table via emit_wasm.h API for atom literals.
 *     emit_wasm_strlit_intern() / emit_wasm_strlit_abs() / emit_wasm_data_segment().
 *   - Each Prolog-specific EKind gets a case here ONLY — never in emit_wasm.c.
 *   - Byrd-box four-port model: α/β/γ/ω encoded as tail-call WAT functions.
 *     WASM has no arbitrary goto; return_call is zero-overhead tail dispatch.
 *   - Runtime imports from "pl" namespace (pl_runtime.wat), not "sno".
 *
 * Port encoding (mirrors emit_x64_prolog.c and emit_jvm_prolog.c):
 *   α  — try:     initial entry, attempt first clause head unification
 *   β  — retry:   backtrack, unwind trail, attempt next clause
 *   γ  — succeed: head unified + body executed, signal caller
 *   ω  — fail:    all clauses exhausted, propagate failure up
 *
 * Milestones:
 *   M-PW-SCAFFOLD  stub scaffold, -pl -wasm wired (PW-1 2026-03-30)
 *   M-PW-HELLO     write/1 atom + nl/0 + initialization(main) (PW-2 2026-03-30)
 */

#include "scrip_cc.h"
#include "emit_wasm.h"
#include "../frontend/prolog/prolog_atom.h"
#include "../frontend/prolog/prolog_parse.h"
#include "../frontend/prolog/prolog_lower.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <stdarg.h>

/* -------------------------------------------------------------------------
 * Output state
 * ------------------------------------------------------------------------- */

static FILE *wpl_out = NULL;

static void W(const char *fmt, ...) {
    va_list ap;
    va_start(ap, fmt);
    vfprintf(wpl_out, fmt, ap);
    va_end(ap);
}

/* -------------------------------------------------------------------------
 * Name mangling — safe WAT identifiers from functor/arity
 * foo/2 → pl_foo_2   (mirrors emit_jvm_prolog.c pj_mangle convention)
 * ------------------------------------------------------------------------- */
static char mangle_buf[512];
static const char *pl_mangle(const char *functor, int arity) {
    int di = 0;
    mangle_buf[di++] = 'p'; mangle_buf[di++] = 'l'; mangle_buf[di++] = '_';
    for (const char *s = functor; *s && di < 480; s++) {
        char c = *s;
        if (isalnum((unsigned char)c) || c == '_')
            mangle_buf[di++] = c;
        else {
            mangle_buf[di++] = '_';
            mangle_buf[di++] = "0123456789abcdef"[(unsigned char)c >> 4];
            mangle_buf[di++] = "0123456789abcdef"[(unsigned char)c & 0xf];
        }
    }
    mangle_buf[di++] = '_';
    int a = arity < 0 ? 0 : arity;
    if (a >= 10) { mangle_buf[di++] = '0' + a / 10; }
    mangle_buf[di++] = '0' + a % 10;
    mangle_buf[di] = '\0';
    return mangle_buf;
}

/* suppress unused-function warning until milestone uses it */
static const char *pl_mangle_unused(const char *f, int a) { return pl_mangle(f,a); }
static void _pl_mangle_ref(void) __attribute__((unused));
static void _pl_mangle_ref(void) { (void)pl_mangle_unused(NULL,0); }

/* -------------------------------------------------------------------------
 * Runtime imports (pl namespace, pl_runtime.wat)
 * ------------------------------------------------------------------------- */
static void emit_pl_runtime_imports(void) {
    W(";; --- Prolog WASM runtime imports (pl_runtime.wat) ---\n");
    W("  (import \"pl\" \"memory\"         (memory 2))\n");
    W("  (import \"pl\" \"trail_mark\"      (func $trail_mark      (result i32)))\n");
    W("  (import \"pl\" \"trail_unwind\"    (func $trail_unwind    (param i32)))\n");
    W("  (import \"pl\" \"output_str\"      (func $pl_output_str   (param i32 i32)))\n");
    W("  (import \"pl\" \"output_flush\"    (func $pl_output_flush (result i32)))\n");
    W("  (import \"pl\" \"output_nl\"       (func $pl_output_nl))\n");
    W("  (import \"pl\" \"unify_atom\"      (func $pl_unify_atom   (param i32 i32) (result i32)))\n");
    W("  (import \"pl\" \"var_bind\"        (func $pl_var_bind     (param i32 i32)))\n");
    W("  (import \"pl\" \"var_deref\"       (func $pl_var_deref    (param i32) (result i32)))\n");
    W("\n");
}

/* -------------------------------------------------------------------------
 * Stub body for unimplemented goals
 * ------------------------------------------------------------------------- */
static void emit_goal_stub(const EXPR_t *g) {
    const char *kind_name = "unknown";
    if (g) {
        switch (g->kind) {
            case E_CHOICE:       kind_name = "E_CHOICE";      break;
            case E_CLAUSE:       kind_name = "E_CLAUSE";      break;
            case E_UNIFY:        kind_name = "E_UNIFY";       break;
            case E_CUT:          kind_name = "E_CUT";         break;
            case E_TRAIL_MARK:   kind_name = "E_TRAIL_MARK";  break;
            case E_TRAIL_UNWIND: kind_name = "E_TRAIL_UNWIND";break;
            default:             kind_name = "other";         break;
        }
    }
    W("    ;; STUB: %s not yet implemented (PW milestone pending)\n", kind_name);
    W("    unreachable\n");
}

/* -------------------------------------------------------------------------
 * emit_write_atom — M-PW-HELLO
 * Output a single atom/integer arg via $pl_output_str using shared string table.
 * ------------------------------------------------------------------------- */
static void emit_write_atom(const EXPR_t *arg) {
    if (!arg) return;

    if (arg->kind == E_QLIT && arg->sval) {
        int idx = emit_wasm_strlit_intern(arg->sval);
        int off = emit_wasm_strlit_abs(idx);
        int len = emit_wasm_strlit_len(idx);
        W("    ;; write('%s') off=%d len=%d\n", arg->sval, off, len);
        W("    (i32.const %d)\n", off);
        W("    (i32.const %d)\n", len);
        W("    (call $pl_output_str)\n");
        return;
    }

    if (arg->kind == E_ILIT) {
        char numbuf[32];
        snprintf(numbuf, sizeof numbuf, "%ld", arg->ival);
        int idx = emit_wasm_strlit_intern(numbuf);
        int off = emit_wasm_strlit_abs(idx);
        int len = emit_wasm_strlit_len(idx);
        W("    ;; write(%ld)\n", arg->ival);
        W("    (i32.const %d)\n", off);
        W("    (i32.const %d)\n", len);
        W("    (call $pl_output_str)\n");
        return;
    }

    W("    ;; write/1 arg kind=%d — stub until later milestone\n", (int)arg->kind);
    W("    unreachable\n");
}

/* -------------------------------------------------------------------------
 * emit_pl_goal — Goal emission dispatch
 * All Prolog-specific EKinds handled here — never in emit_wasm.c.
 * ------------------------------------------------------------------------- */
static void emit_pl_goal(const EXPR_t *goal) {
    if (!goal) return;

    if (goal->kind == E_CUT)         { emit_goal_stub(goal); return; } /* M-PW-B03 */
    if (goal->kind == E_UNIFY)       { emit_goal_stub(goal); return; } /* M-PW-A01 */
    if (goal->kind == E_TRAIL_MARK)  { emit_goal_stub(goal); return; } /* M-PW-A01 */
    if (goal->kind == E_TRAIL_UNWIND){ emit_goal_stub(goal); return; } /* M-PW-A01 */
    if (goal->kind == E_CHOICE)      { emit_goal_stub(goal); return; } /* M-PW-A01 */
    if (goal->kind == E_CLAUSE)      { emit_goal_stub(goal); return; } /* M-PW-A01 */

    /* E_SEQ — goal conjunction: (A, B) → emit each child goal */
    if (goal->kind == E_SEQ) {
        for (int i = 0; i < goal->nchildren; i++)
            emit_pl_goal(goal->children[i]);
        return;
    }

    /* E_FNC — builtin goals */
    if (goal->kind == E_FNC && goal->sval) {
        const char *fn = goal->sval;

        if (strcasecmp(fn, "nl") == 0) {
            W("    ;; nl/0\n");
            W("    (call $pl_output_nl)\n");
            return;
        }
        if (strcasecmp(fn, "write") == 0 || strcasecmp(fn, "writeln") == 0) {
            if (goal->nchildren >= 1) emit_write_atom(goal->children[0]);
            if (strcasecmp(fn, "writeln") == 0) W("    (call $pl_output_nl)\n");
            return;
        }
        if (strcasecmp(fn, "halt") == 0) {
            W("    ;; halt/0\n");
            W("    (call $pl_output_flush)\n");
            W("    drop\n");
            W("    return\n");
            return;
        }
        if (strcasecmp(fn, "true") == 0) {
            W("    ;; true/0\n");
            return;
        }
        if (strcasecmp(fn, "fail") == 0) {
            W("    ;; fail/0 — stub (M-PW-B01)\n");
            W("    unreachable\n");
            return;
        }

        W("    ;; STUB builtin: %s/%d\n", fn, goal->nchildren);
        W("    unreachable\n");
        return;
    }

    emit_goal_stub(goal);
}

/* -------------------------------------------------------------------------
 * emit_pl_main — emit (func $main (export "main"))
 *
 * M-PW-HELLO: walks program statements, emits goal bodies.
 * prolog_lower() for  main :- write(hello), nl.
 * produces statement(s) with subject = E_SEQ or E_FNC goal tree.
 * ------------------------------------------------------------------------- */
/* -------------------------------------------------------------------------
 * emit_pl_choice_body — for a single-clause E_CHOICE (deterministic fact),
 * inline the body goals directly. This handles main/0 for M-PW-HELLO.
 *
 * E_CLAUSE layout:
 *   dval          = (double)n_args  (arity)
 *   children[0..n_args-1] = head arg patterns (skip for arity-0)
 *   children[n_args..]    = body goals
 * ------------------------------------------------------------------------- */
static void emit_pl_choice_body(EXPR_t *choice) {
    if (!choice || choice->kind != E_CHOICE) return;
    /* Single clause: nchildren == 1, children[0] is E_CLAUSE */
    if (choice->nchildren == 1 && choice->children[0]->kind == E_CLAUSE) {
        EXPR_t *clause = choice->children[0];
        int n_args = (int)clause->dval;
        /* Emit body goals: children[n_args..nchildren-1] */
        for (int i = n_args; i < clause->nchildren; i++)
            emit_pl_goal(clause->children[i]);
        return;
    }
    /* Multi-clause: stub until M-PW-A01 */
    W("    ;; STUB: multi-clause E_CHOICE — M-PW-A01\n");
    W("    unreachable\n");
}

static void emit_pl_main(Program *prog) {
    W("  (func (export \"main\") (result i32)\n");
    W("    ;; Prolog program — M-PW-HELLO\n");

    if (prog && prog->head) {
        /* Pass 1: scan for E_CHOICE with sval "main/0" and emit its body */
        int found_main = 0;
        for (STMT_t *s = prog->head; s; s = s->next) {
            if (!s->subject) continue;
            EXPR_t *g = s->subject;
            if (g->kind == E_CHOICE && g->sval &&
                strcmp(g->sval, "main/0") == 0) {
                emit_pl_choice_body(g);
                found_main = 1;
            }
        }

        /* Pass 2: handle non-E_CHOICE directives (skip known meta-directives) */
        if (!found_main) {
            for (STMT_t *s = prog->head; s; s = s->next) {
                if (!s->subject) continue;
                EXPR_t *g = s->subject;
                if (g->kind == E_CHOICE) continue;
                if (g->kind == E_FNC && g->sval) {
                    const char *fn = g->sval;
                    /* Skip meta-directives with no runtime effect */
                    if (strcmp(fn, "initialization") == 0 ||
                        strcmp(fn, "dynamic")         == 0 ||
                        strcmp(fn, "discontiguous")   == 0 ||
                        strcmp(fn, "module")          == 0 ||
                        strcmp(fn, "use_module")      == 0 ||
                        strcmp(fn, "style_check")     == 0) continue;
                    emit_pl_goal(g);
                }
            }
        }
    }

    W("    ;; flush → return byte count\n");
    W("    (call $pl_output_flush)\n");
    W("  )\n");
}

/* -------------------------------------------------------------------------
 * prescan — intern all atom literals before emitting data segment
 * ------------------------------------------------------------------------- */
static void prescan_goal(const EXPR_t *g) {
    if (!g) return;
    if (g->kind == E_QLIT && g->sval)
        emit_wasm_strlit_intern(g->sval);
    if (g->kind == E_ILIT) {
        char numbuf[32];
        snprintf(numbuf, sizeof numbuf, "%ld", g->ival);
        emit_wasm_strlit_intern(numbuf);
    }
    for (int i = 0; i < g->nchildren; i++)
        prescan_goal(g->children[i]);
}

static void prescan_pl_prog(Program *prog) {
    emit_wasm_strlit_intern("");  /* always intern empty string */
    if (!prog) return;
    for (STMT_t *s = prog->head; s; s = s->next) {
        prescan_goal(s->subject);
        prescan_goal(s->pattern);
        prescan_goal(s->replacement);
    }
}

/* -------------------------------------------------------------------------
 * Public entry point
 * Called from driver/main.c when -pl and -wasm are both set.
 * ------------------------------------------------------------------------- */
void prolog_emit_wasm(Program *prog, FILE *out, const char *filename) {
    (void)filename;
    wpl_out = out;

    /* Share output stream with emit_wasm.c, reset string table */
    emit_wasm_set_out(out);
    emit_wasm_strlit_reset();

    /* Pre-scan to intern all atom literals */
    prescan_pl_prog(prog);

    W(";; Generated by scrip-cc -pl -wasm (M-PW-HELLO)\n");
    W("(module\n");
    W("\n");

    emit_pl_runtime_imports();
    emit_wasm_data_segment();    /* shared string literal data block */

    W("\n");
    emit_pl_main(prog);
    W("\n");
    W(") ;; end module\n");
}
