#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include "emit_ir.h"
#include "emit_core.h"
#include "sm_prog.h"
#include "../ast/ast.h"
/*Forward declaration for sm_preamble (defined in scrip_sm.h) */
extern SM_Program *sm_preamble(const tree_t *ast_prog);
/*----------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------*/
/* JS Emitter for IR_t → JavaScript code generation.
   Each IR_PAT_* node kind gets an emit_js_bb_NODE function that generates a JavaScript factory function.
   Scalar nodes (IR_LIT_I, IR_VAR, etc.) are emitted into the switch/dispatch loop by emit_js_scalar.
   Generator nodes (IR_PAT_*) are emitted as factory functions by emit_js_generator. */
/*----------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------*/
/* Helper: escape a string for JS string literal — double-quote → \", backslash → \\, etc. */
static void js_escape_string(FILE * out, const char * s) {
    fprintf(out, "\"");
    for (; s && *s; s++) {
        unsigned char c = (unsigned char)*s;
        if      (c == '"')  fprintf(out, "\\\"");
        else if (c == '\\') fprintf(out, "\\\\");
        else if (c == '\n') fprintf(out, "\\n");
        else if (c == '\r') fprintf(out, "\\r");
        else if (c == '\t') fprintf(out, "\\t");
        else if (c < 0x20 || c > 0x7e) fprintf(out, "\\x%02x", c);
        else fprintf(out, "%c", c);
    }
    fprintf(out, "\"");
}
/*----------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------*/
/* IR_PAT_LIT — literal string match. Parameters: nd->sval = literal text. */
static int emit_js_bb_lit(IR_t * nd, FILE * out) {
    int nid = ir_node_id(nd);
    fprintf(out, "function make_pat_%d_%d(ms) { const lit = ", nd->ival, nid);
    js_escape_string(out, nd->sval);
    fprintf(out, "; const len = lit.length; let self = { succ: null, fail: null,\n");
    fprintf(out, "alpha() { if (ms.delta + len > ms.omega || ms.sigma.slice(ms.delta, ms.delta + len) !== lit) { self.fail.alpha(); return; } ms.delta += len; self.succ.alpha(); },\n");
    fprintf(out, "beta() { ms.delta -= len; self.fail.alpha(); }\n");
    fprintf(out, "}; return self; }\n");
    return 0;
}
/*----------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------*/
/* IR_PAT_SPAN — match one or more chars in charset. Parameters: nd->sval = charset. */
static int emit_js_bb_span(IR_t * nd, FILE * out) {
    int nid = ir_node_id(nd);
    fprintf(out, "function make_pat_%d_%d(ms) { const chars = ", nd->ival, nid);
    js_escape_string(out, nd->sval);
    fprintf(out, "; let delta = 0; let self = { succ: null, fail: null,\n");
    fprintf(out, "alpha() { delta = 0; while (ms.delta + delta < ms.omega && chars.indexOf(ms.sigma[ms.delta + delta]) >= 0) delta++; if (delta <= 0) { self.fail.alpha(); return; } const r = ms.sigma.slice(ms.delta, ms.delta + delta); ms.delta += delta; self.succ.alpha(); return r; },\n");
    fprintf(out, "beta() { ms.delta -= delta; self.fail.alpha(); }\n");
    fprintf(out, "}; return self; }\n");
    return 0;
}
/*----------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------*/
/* IR_PAT_BREAK — match until char in charset. Parameters: nd->sval = charset. */
static int emit_js_bb_break(IR_t * nd, FILE * out) {
    int nid = ir_node_id(nd);
    fprintf(out, "function make_pat_%d_%d(ms) { const chars = ", nd->ival, nid);
    js_escape_string(out, nd->sval);
    fprintf(out, "; let delta = 0; let self = { succ: null, fail: null,\n");
    fprintf(out, "alpha() { delta = 0; while (ms.delta + delta < ms.omega && chars.indexOf(ms.sigma[ms.delta + delta]) < 0) delta++; const r = ms.sigma.slice(ms.delta, ms.delta + delta); ms.delta += delta; self.succ.alpha(); return r; },\n");
    fprintf(out, "beta() { ms.delta -= delta; self.fail.alpha(); }\n");
    fprintf(out, "}; return self; }\n");
    return 0;
}
/*----------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------*/
/* IR_PAT_ANY — match one char if in charset. Parameters: nd->sval = charset. */
static int emit_js_bb_any(IR_t * nd, FILE * out) {
    int nid = ir_node_id(nd);
    fprintf(out, "function make_pat_%d_%d(ms) { const chars = ", nd->ival, nid);
    js_escape_string(out, nd->sval);
    fprintf(out, "; let self = { succ: null, fail: null,\n");
    fprintf(out, "alpha() { if (ms.delta >= ms.omega || chars.indexOf(ms.sigma[ms.delta]) < 0) { self.fail.alpha(); return; } const r = ms.sigma.slice(ms.delta, ms.delta + 1); ms.delta++; self.succ.alpha(); return r; },\n");
    fprintf(out, "beta() { ms.delta--; self.fail.alpha(); }\n");
    fprintf(out, "}; return self; }\n");
    return 0;
}
/*----------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------*/
/* IR_PAT_NOTANY — match one char NOT in charset. Parameters: nd->sval = charset. */
static int emit_js_bb_notany(IR_t * nd, FILE * out) {
    int nid = ir_node_id(nd);
    fprintf(out, "function make_pat_%d_%d(ms) { const chars = ", nd->ival, nid);
    js_escape_string(out, nd->sval);
    fprintf(out, "; let self = { succ: null, fail: null,\n");
    fprintf(out, "alpha() { if (ms.delta >= ms.omega || chars.indexOf(ms.sigma[ms.delta]) >= 0) { self.fail.alpha(); return; } const r = ms.sigma.slice(ms.delta, ms.delta + 1); ms.delta++; self.succ.alpha(); return r; },\n");
    fprintf(out, "beta() { ms.delta--; self.fail.alpha(); }\n");
    fprintf(out, "}; return self; }\n");
    return 0;
}
/*----------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------*/
/* IR_PAT_LEN — match exactly N characters. Parameters: nd->ival = N. */
static int emit_js_bb_len(IR_t * nd, FILE * out) {
    int nid = ir_node_id(nd);
    int64_t n = nd->ival;
    fprintf(out, "function make_pat_%d_%d(ms) { const n = %ld; let self = { succ: null, fail: null,\n", nd->ival, nid, n);
    fprintf(out, "alpha() { if (ms.delta + n > ms.omega) { self.fail.alpha(); return; } const r = ms.sigma.slice(ms.delta, ms.delta + n); ms.delta += n; self.succ.alpha(); return r; },\n");
    fprintf(out, "beta() { ms.delta -= n; self.fail.alpha(); }\n");
    fprintf(out, "}; return self; }\n");
    return 0;
}
/*----------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------*/
/* IR_PAT_POS — match at absolute or relative position (variant in nd->n: 0=abs, 1=rel). */
static int emit_js_bb_pos(IR_t * nd, FILE * out) {
    int nid = ir_node_id(nd);
    int64_t n = nd->ival;
    if (nd->n == 1) {
        fprintf(out, "function make_pat_%d_%d(ms) { const n = %ld; let self = { succ: null, fail: null,\n", nd->ival, nid, n);
        fprintf(out, "alpha() { if (ms.delta !== ms.omega - n) { self.fail.alpha(); return; } self.succ.alpha(); return ''; },\n");
        fprintf(out, "beta() { self.fail.alpha(); }\n");
    } else {
        fprintf(out, "function make_pat_%d_%d(ms) { const n = %ld; let self = { succ: null, fail: null,\n", nd->ival, nid, n);
        fprintf(out, "alpha() { if (ms.delta !== n) { self.fail.alpha(); return; } self.succ.alpha(); return ''; },\n");
        fprintf(out, "beta() { self.fail.alpha(); }\n");
    }
    fprintf(out, "}; return self; }\n");
    return 0;
}
/*----------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------*/
/* IR_PAT_TAB — match up to absolute or relative position (variant in nd->n: 0=abs, 1=rel). */
static int emit_js_bb_tab(IR_t * nd, FILE * out) {
    int nid = ir_node_id(nd);
    int64_t n = nd->ival;
    if (nd->n == 1) {
        fprintf(out, "function make_pat_%d_%d(ms) { const n = %ld; let delta = 0; let self = { succ: null, fail: null,\n", nd->ival, nid, n);
        fprintf(out, "alpha() { const tgt = ms.omega - n; if (ms.delta > tgt) { self.fail.alpha(); return; } delta = tgt - ms.delta; const r = ms.sigma.slice(ms.delta, ms.delta + delta); ms.delta += delta; self.succ.alpha(); return r; },\n");
        fprintf(out, "beta() { ms.delta -= delta; self.fail.alpha(); }\n");
    } else {
        fprintf(out, "function make_pat_%d_%d(ms) { const n = %ld; let delta = 0; let self = { succ: null, fail: null,\n", nd->ival, nid, n);
        fprintf(out, "alpha() { if (ms.delta > n || ms.delta > ms.omega) { self.fail.alpha(); return; } delta = n - ms.delta; if (ms.delta + delta > ms.omega) delta = ms.omega - ms.delta; const r = ms.sigma.slice(ms.delta, ms.delta + delta); ms.delta += delta; self.succ.alpha(); return r; },\n");
        fprintf(out, "beta() { ms.delta -= delta; self.fail.alpha(); }\n");
    }
    fprintf(out, "}; return self; }\n");
    return 0;
}
/*----------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------*/
/* IR_PAT_REM — match remainder (from cursor to end). No parameters. */
static int emit_js_bb_rem(IR_t * nd, FILE * out) {
    int nid = ir_node_id(nd);
    fprintf(out, "function make_pat_%d_%d(ms) { let delta = 0; let self = { succ: null, fail: null,\n", nd->ival, nid);
    fprintf(out, "alpha() { delta = ms.omega - ms.delta; const r = ms.sigma.slice(ms.delta, ms.delta + delta); ms.delta += delta; self.succ.alpha(); return r; },\n");
    fprintf(out, "beta() { ms.delta -= delta; self.fail.alpha(); }\n");
    fprintf(out, "}; return self; }\n");
    return 0;
}
/*----------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------*/
/* IR_PAT_ARB — arbitrary length match (greedy). No parameters. */
static int emit_js_bb_arb(IR_t * nd, FILE * out) {
    int nid = ir_node_id(nd);
    fprintf(out, "function make_pat_%d_%d(ms) { let delta = 0; let self = { succ: null, fail: null,\n", nd->ival, nid);
    fprintf(out, "alpha() { delta = ms.omega - ms.delta; const r = ms.sigma.slice(ms.delta, ms.delta + delta); ms.delta += delta; self.succ.alpha(); return r; },\n");
    fprintf(out, "beta() { if (delta <= 0) { self.fail.alpha(); return; } delta--; ms.delta--; const r = ms.sigma.slice(ms.delta, ms.delta + delta + 1); return r; }\n");
    fprintf(out, "}; return self; }\n");
    return 0;
}
/*----------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------*/
/* IR_PAT_ARBNO — zero or more repetitions. Parameters: nd->c[0] = body node. */
static int emit_js_bb_arbno(IR_t * nd, FILE * out) {
    int nid = ir_node_id(nd);
    fprintf(out, "function make_pat_%d_%d(ms) { const stack = []; let self = { succ: null, fail: null,\n", nd->ival, nid);
    fprintf(out, "alpha() { stack.length = 0; stack.push({ start: ms.delta }); while (true) { const frame = stack[stack.length - 1]; const br = self.body.alpha();\n");
    fprintf(out, "if (br === null) { return ms.sigma.slice(stack[0].start, ms.delta - stack[0].start); }\n");
    fprintf(out, "if (ms.delta === frame.start) { return ms.sigma.slice(stack[0].start, ms.delta - stack[0].start); }\n");
    fprintf(out, "stack.push({ start: ms.delta }); } },\n");
    fprintf(out, "beta() { if (stack.length <= 1) { self.fail.alpha(); return; } stack.pop(); const frame = stack[stack.length - 1]; ms.delta = frame.start; return ms.sigma.slice(stack[0].start, ms.delta - stack[0].start); }\n");
    fprintf(out, "}; return self; }\n");
    return 0;
}
/*----------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------*/
/* IR_PAT_CAT — concatenation (sequence). Parameters: nd->c[0] = left, nd->c[1] = right. */
static int emit_js_bb_cat(IR_t * nd, FILE * out) {
    int nid = ir_node_id(nd);
    fprintf(out, "function make_pat_%d_%d(ms) { let self = { succ: null, fail: null,\n", nd->ival, nid);
    fprintf(out, "alpha() { const lr = self.left.alpha(); if (lr === null) { self.fail.alpha(); return; }\n");
    fprintf(out, "let rr = self.right.alpha(); while (rr === null) { const lr2 = self.left.beta(); if (lr2 === null) { self.fail.alpha(); return; } rr = self.right.alpha(); }\n");
    fprintf(out, "self.succ.alpha(); return rr; },\n");
    fprintf(out, "beta() { let rr = self.right.beta(); while (rr === null) { const lr = self.left.beta(); if (lr === null) { self.fail.alpha(); return; } rr = self.right.alpha(); }\n");
    fprintf(out, "return rr; }\n");
    fprintf(out, "}; return self; }\n");
    return 0;
}
/*----------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------*/
/* IR_PAT_ALT — alternation (choice). Parameters: nd->c[0..n-1] = children. */
static int emit_js_bb_alt(IR_t * nd, FILE * out) {
    int nid = ir_node_id(nd);
    fprintf(out, "function make_pat_%d_%d(ms) { const children = self.children || []; let idx = 0; let self = { succ: null, fail: null,\n", nd->ival, nid);
    fprintf(out, "alpha() { idx = 0; while (idx < children.length) { const r = children[idx].alpha(); if (r !== null) { self.succ.alpha(); return r; } idx++; } self.fail.alpha(); return null; },\n");
    fprintf(out, "beta() { idx--; if (idx >= 0 && idx < children.length) { const r = children[idx].beta(); if (r !== null) { return r; } return self.beta(); } self.fail.alpha(); return null; }\n");
    fprintf(out, "}; return self; }\n");
    return 0;
}
/*----------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------*/
/* IR_PAT_ASSIGN_IMM — immediate capture. Parameters: nd->sval = varname, nd->ival2 = immediate flag. */
static int emit_js_bb_assign_imm(IR_t * nd, FILE * out) {
    int nid = ir_node_id(nd);
    fprintf(out, "function make_pat_%d_%d(ms) { const varname = ", nd->ival, nid);
    js_escape_string(out, nd->sval);
    fprintf(out, "; let self = { succ: null, fail: null,\n");
    fprintf(out, "alpha() { const cr = self.child.alpha(); if (cr === null) { self.fail.alpha(); return; } ms._do_capture(cr, varname, 1); self.succ.alpha(); return cr; },\n");
    fprintf(out, "beta() { const cr = self.child.beta(); if (cr === null) { self.fail.alpha(); return; } ms._do_capture(cr, varname, 1); return cr; }\n");
    fprintf(out, "}; return self; }\n");
    return 0;
}
/*----------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------*/
/* IR_PAT_ASSIGN_COND — conditional capture. Parameters: nd->sval = varname. */
static int emit_js_bb_assign_cond(IR_t * nd, FILE * out) {
    int nid = ir_node_id(nd);
    fprintf(out, "function make_pat_%d_%d(ms) { const varname = ", nd->ival, nid);
    js_escape_string(out, nd->sval);
    fprintf(out, "; let self = { succ: null, fail: null,\n");
    fprintf(out, "alpha() { const cr = self.child.alpha(); if (cr === null) { self.fail.alpha(); return; } ms._do_capture(cr, varname, 0); self.succ.alpha(); return cr; },\n");
    fprintf(out, "beta() { const cr = self.child.beta(); if (cr === null) { self.fail.alpha(); return; } ms._do_capture(cr, varname, 0); return cr; }\n");
    fprintf(out, "}; return self; }\n");
    return 0;
}
/*----------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------*/
/* IR_PAT_FENCE — fence (cut). No parameters. */
static int emit_js_bb_fence(IR_t * nd, FILE * out) {
    int nid = ir_node_id(nd);
    fprintf(out, "function make_pat_%d_%d(ms) { let self = { succ: null, fail: null,\n", nd->ival, nid);
    fprintf(out, "alpha() { self.succ.alpha(); return ''; },\n");
    fprintf(out, "beta() { self.fail.alpha(); return null; }\n");
    fprintf(out, "}; return self; }\n");
    return 0;
}
/*----------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------*/
/* IR_PAT_ABORT — abort (forced failure). No parameters. */
static int emit_js_bb_abort(IR_t * nd, FILE * out) {
    int nid = ir_node_id(nd);
    fprintf(out, "function make_pat_%d_%d(ms) { let self = { succ: null, fail: null,\n", nd->ival, nid);
    fprintf(out, "alpha() { self.fail.alpha(); return null; },\n");
    fprintf(out, "beta() { self.fail.alpha(); return null; }\n");
    fprintf(out, "}; return self; }\n");
    return 0;
}
/*----------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------*/
/* Dispatch table for generator nodes — maps IR_e to emitter function. */
typedef int (*emit_js_gen_fn)(IR_t * nd, FILE * out);
static const struct { IR_e kind; emit_js_gen_fn fn; } g_js_gen_emitters[] = {
    { IR_PAT_LIT,         emit_js_bb_lit },
    { IR_PAT_SPAN,        emit_js_bb_span },
    { IR_PAT_BREAK,       emit_js_bb_break },
    { IR_PAT_ANY,         emit_js_bb_any },
    { IR_PAT_NOTANY,      emit_js_bb_notany },
    { IR_PAT_LEN,         emit_js_bb_len },
    { IR_PAT_POS,         emit_js_bb_pos },
    { IR_PAT_TAB,         emit_js_bb_tab },
    { IR_PAT_REM,         emit_js_bb_rem },
    { IR_PAT_ARB,         emit_js_bb_arb },
    { IR_PAT_ARBNO,       emit_js_bb_arbno },
    { IR_PAT_CAT,         emit_js_bb_cat },
    { IR_PAT_ALT,         emit_js_bb_alt },
    { IR_PAT_ASSIGN_IMM,  emit_js_bb_assign_imm },
    { IR_PAT_ASSIGN_COND, emit_js_bb_assign_cond },
    { IR_PAT_FENCE,       emit_js_bb_fence },
    { IR_PAT_ABORT,       emit_js_bb_abort },
    { -1, NULL }
};
/*----------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------*/
/* emit_js_generator — dispatcher for pattern/generator nodes. */
int emit_js_generator(IR_t * nd, FILE * out) {
    for (int i = 0; g_js_gen_emitters[i].fn; i++) {
        if (g_js_gen_emitters[i].kind == nd->t) {
            return g_js_gen_emitters[i].fn(nd, out);
        }
    }
    fprintf(stderr, "emit_js_generator: unhandled IR kind %d\n", nd->t);
    return 1;
}
/*----------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------*/
/* emit_js_program — public entry point for JS emission from tree_t AST. */
int emit_js_program(const tree_t * ast_prog, FILE * out) {
    if (!ast_prog || !out) return 1;
    SM_Program *sm = sm_preamble(ast_prog);
    if (!sm) return 1;
    /* EC-3-prep: install EMIT_JS mode for the duration of this emission. See
       parallel comment in emit_jvm_program — pure infrastructure today, the
       silo doesn't yet read bb_emit_mode but future SM_templates will. */
    bb_emit_mode_t saved_mode = bb_emit_mode;
    FILE *         saved_out  = bb_emit_out;
    emit_mode_set(EMIT_JS, out);
    /* Prologue: header + runtime require + _init. */
    fprintf(out, "'use strict';\n");
    fprintf(out, "const rt = require('/home/claude/one4all/src/runtime/js/sno_runtime.js');\n");
    fprintf(out, "rt._init();\n");
    /* Pre-scan: emit user-fn entry-PC registration for all SM_LABEL with a name.
     * This includes both define_entry labels and plain labels (for alt-entry
     * forms like DEFINE("FOO(X)", "ALT") where ALT is a plain SM_LABEL). */
    fprintf(out, "rt._register_label_pcs({");
    int first = 1;
    for (int i = 0; i < sm->count; i++) {
        SM_Instr *in = &sm->instrs[i];
        if (in->op == SM_LABEL && in->a[0].s && in->a[0].s[0]) {
            if (!first) fprintf(out, ",");
            js_escape_string(out, in->a[0].s);
            fprintf(out, ":%d", i);
            first = 0;
        }
    }
    fprintf(out, "});\n");
    fprintf(out, "let _pc = 0;\n");
    fprintf(out, "loop: while (true) { switch (_pc) {\n");
    emit_js_from_sm(sm, out);
    emit_js_epilogue(NULL, out);
    emit_mode_set(saved_mode, saved_out);
    sm_prog_free(sm);
    return 0;
}
/*----------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------*/
/* emit_js_from_sm — walk SM_Program and emit JS for scalar statements. Use instruction index as case label. */
int emit_js_from_sm(SM_Program * sm, FILE * out) {
    if (!sm || !out || sm->count == 0) return 0;
    for (int i = 0; i < sm->count; i++) {
        SM_Instr * instr = &sm->instrs[i];
        fprintf(out, "case %d: ", i);
        int has_continue = 0;
        switch (instr->op) {
        case SM_STNO:
            fprintf(out, "rt.set_stno(%lld); ", instr->a[0].i);
            break;
        case SM_LABEL:
            break;
        case SM_PUSH_LIT_I:
            fprintf(out, "rt.push_int(%lld); ", instr->a[0].i);
            break;
        case SM_PUSH_LIT_S:
            fprintf(out, "rt.push_str(");
            js_escape_string(out, instr->a[0].s);
            fprintf(out, ", %d); ", (int)(instr->a[0].s ? strlen(instr->a[0].s) : 0));
            break;
        case SM_PUSH_LIT_F:
            fprintf(out, "rt.push_real_bits(%.17g); ", instr->a[0].f);
            break;
        case SM_PUSH_NULL:
        case SM_PUSH_NULL_NOFLIP:
            fprintf(out, "rt.push_null(); ");
            break;
        case SM_PUSH_VAR:
            fprintf(out, "rt.push_var(");
            js_escape_string(out, instr->a[0].s ? instr->a[0].s : "");
            fprintf(out, "); ");
            break;
        case SM_STORE_VAR:
            fprintf(out, "rt.store_var(");
            js_escape_string(out, instr->a[0].s ? instr->a[0].s : "");
            fprintf(out, "); ");
            break;
        case SM_VOID_POP:
            fprintf(out, "rt.pop_void(); ");
            break;
        case SM_ADD:
            fprintf(out, "rt.arith('add'); ");
            break;
        case SM_SUB:
            fprintf(out, "rt.arith('sub'); ");
            break;
        case SM_MUL:
            fprintf(out, "rt.arith('mul'); ");
            break;
        case SM_DIV:
            fprintf(out, "rt.arith('div'); ");
            break;
        case SM_MOD:
            fprintf(out, "rt.arith('mod'); ");
            break;
        case SM_CONCAT:
            fprintf(out, "rt.concat(); ");
            break;
        case SM_NEG:
            fprintf(out, "rt.neg(); ");
            break;
        case SM_COERCE_NUM:
            fprintf(out, "rt.coerce_num(); ");
            break;
        case SM_EXP:
            fprintf(out, "rt.exp_op(); ");
            break;
        case SM_HALT:
            fprintf(out, "break loop; ");
            has_continue = 1;
            break;
        case SM_JUMP:
            fprintf(out, "_pc = %lld; continue; ", instr->a[0].i);
            has_continue = 1;
            break;
        case SM_JUMP_S:
            fprintf(out, "if (rt.last_ok()) _pc = %lld; else _pc = %d; continue; ", instr->a[0].i, i + 1);
            has_continue = 1;
            break;
        case SM_JUMP_F:
            fprintf(out, "if (!rt.last_ok()) _pc = %lld; else _pc = %d; continue; ", instr->a[0].i, i + 1);
            has_continue = 1;
            break;
        case SM_SUSPEND_VALUE:
            fprintf(out, "{ let _r = rt.call_or_jump(");
            js_escape_string(out, instr->a[0].s ? instr->a[0].s : "");
            fprintf(out, ", %lld, %d); if (_r >= 0) { _pc = _r; continue; } } ", instr->a[1].i, i + 1);
            fprintf(out, "rt.set_last_ok(!rt._is_fail(rt._peek())); ");
            break;
        case SM_CALL_FN:
            fprintf(out, "{ let _r = rt.call_or_jump(");
            js_escape_string(out, instr->a[0].s ? instr->a[0].s : "");
            fprintf(out, ", %lld, %d); if (_r >= 0) { _pc = _r; continue; } } ", instr->a[1].i, i + 1);
            break;
        case SM_RETURN:
            fprintf(out, "{ let _r = rt.fn_return(0, 0); if (_r === -2) { break loop; } _pc = _r; continue; } ");
            has_continue = 1;
            break;
        case SM_FRETURN:
            fprintf(out, "{ let _r = rt.fn_return(1, 0); if (_r === -2) { break loop; } _pc = _r; continue; } ");
            has_continue = 1;
            break;
        case SM_NRETURN:
            fprintf(out, "{ let _r = rt.fn_return(2, 0); if (_r === -2) { break loop; } _pc = _r; continue; } ");
            has_continue = 1;
            break;
        case SM_RETURN_S:
            fprintf(out, "{ let _r = rt.fn_return(0, 1); if (_r === -1) { _pc = %d; continue; } if (_r === -2) { break loop; } _pc = _r; continue; } ", i + 1);
            has_continue = 1;
            break;
        case SM_RETURN_F:
            fprintf(out, "{ let _r = rt.fn_return(0, 2); if (_r === -1) { _pc = %d; continue; } if (_r === -2) { break loop; } _pc = _r; continue; } ", i + 1);
            has_continue = 1;
            break;
        case SM_FRETURN_S:
            fprintf(out, "{ let _r = rt.fn_return(1, 1); if (_r === -1) { _pc = %d; continue; } if (_r === -2) { break loop; } _pc = _r; continue; } ", i + 1);
            has_continue = 1;
            break;
        case SM_FRETURN_F:
            fprintf(out, "{ let _r = rt.fn_return(1, 2); if (_r === -1) { _pc = %d; continue; } if (_r === -2) { break loop; } _pc = _r; continue; } ", i + 1);
            has_continue = 1;
            break;
        case SM_NRETURN_S:
            fprintf(out, "{ let _r = rt.fn_return(2, 1); if (_r === -1) { _pc = %d; continue; } if (_r === -2) { break loop; } _pc = _r; continue; } ", i + 1);
            has_continue = 1;
            break;
        case SM_NRETURN_F:
            fprintf(out, "{ let _r = rt.fn_return(2, 2); if (_r === -1) { _pc = %d; continue; } if (_r === -2) { break loop; } _pc = _r; continue; } ", i + 1);
            has_continue = 1;
            break;
        case SM_DEFINE_ENTRY:
            /* Marker after a define_entry label; no runtime action (registration done by DEFINE builtin). */
            break;
        case SM_PUSH_EXPRESSION:
            fprintf(out, "rt.push_null(); ");  /* expression-thunk stub */
            break;
        case SM_PAT_LIT:
            fprintf(out, "rt.pat_lit(");
            js_escape_string(out, instr->a[0].s ? instr->a[0].s : "");
            fprintf(out, "); ");
            break;
        case SM_PAT_SPAN:    fprintf(out, "rt.pat_span(); ");    break;
        case SM_PAT_BREAK:   fprintf(out, "rt.pat_break(); ");   break;
        case SM_PAT_ANY:     fprintf(out, "rt.pat_any(); ");     break;
        case SM_PAT_NOTANY:  fprintf(out, "rt.pat_notany(); ");  break;
        case SM_PAT_LEN:     fprintf(out, "rt.pat_len(); ");     break;
        case SM_PAT_POS:     fprintf(out, "rt.pat_pos(); ");     break;
        case SM_PAT_RPOS:    fprintf(out, "rt.pat_rpos(); ");    break;
        case SM_PAT_TAB:     fprintf(out, "rt.pat_tab(); ");     break;
        case SM_PAT_RTAB:    fprintf(out, "rt.pat_rtab(); ");    break;
        case SM_PAT_REM:     fprintf(out, "rt.pat_rem(); ");     break;
        case SM_PAT_ARB:     fprintf(out, "rt.pat_arb(); ");     break;
        case SM_PAT_ARBNO:   fprintf(out, "rt.pat_arbno(); ");   break;
        case SM_PAT_BAL:     fprintf(out, "rt.pat_bal(); ");     break;
        case SM_PAT_FAIL:    fprintf(out, "rt.pat_fail(); ");    break;
        case SM_PAT_SUCCEED: fprintf(out, "rt.pat_succeed(); "); break;
        case SM_PAT_ABORT:   fprintf(out, "rt.pat_abort(); ");   break;
        case SM_PAT_FENCE0:  fprintf(out, "rt.pat_fence(); ");   break;
        case SM_PAT_EPS:     fprintf(out, "rt.pat_eps(); ");     break;
        case SM_PAT_CAT:     fprintf(out, "rt.pat_cat(); ");     break;
        case SM_PAT_ALT:     fprintf(out, "rt.pat_alt(); ");     break;
        case SM_PAT_DEREF:   fprintf(out, "rt.pat_deref(); ");   break;
        case SM_PAT_REFNAME:
            fprintf(out, "rt.pat_refname(");
            js_escape_string(out, instr->a[0].s ? instr->a[0].s : "");
            fprintf(out, "); ");
            break;
        case SM_PAT_CAPTURE:
            fprintf(out, "rt.pat_capture(");
            js_escape_string(out, instr->a[0].s ? instr->a[0].s : "");
            fprintf(out, ", %lld); ", instr->a[1].i);
            break;
        case SM_PAT_CAPTURE_FN:
            fprintf(out, "rt.pat_capture_fn(");
            js_escape_string(out, instr->a[0].s ? instr->a[0].s : "");
            fprintf(out, ", %lld, ", instr->a[1].i);
            js_escape_string(out, instr->a[2].s ? instr->a[2].s : "");
            fprintf(out, "); ");
            break;
        case SM_PAT_CAPTURE_FN_ARGS:
            fprintf(out, "rt.pat_capture_fn_args(");
            js_escape_string(out, instr->a[0].s ? instr->a[0].s : "");
            fprintf(out, ", %lld, %lld); ", instr->a[1].i, instr->a[2].i);
            break;
        case SM_PAT_USERCALL:
            fprintf(out, "rt.pat_usercall(");
            js_escape_string(out, instr->a[0].s ? instr->a[0].s : "");
            fprintf(out, "); ");
            break;
        case SM_PAT_USERCALL_ARGS:
            fprintf(out, "rt.pat_usercall_args(");
            js_escape_string(out, instr->a[0].s ? instr->a[0].s : "");
            fprintf(out, ", %lld); ", instr->a[1].i);
            break;
        case SM_EXEC_STMT:
            fprintf(out, "rt.exec_stmt(");
            js_escape_string(out, instr->a[0].s ? instr->a[0].s : "");
            fprintf(out, ", %lld); ", instr->a[1].i);
            break;
        default:
            break;
        }
        if (!has_continue) {
            fprintf(out, "_pc = %d; continue; ", i + 1);
        }
    }
    return 0;
}
/*----------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------*/
/* emit_js_scalar — emit stack machine operations for scalar IR nodes. */
int emit_js_scalar(IR_t * nd, FILE * out) {
    if (!nd || !out) return 0;
    switch (nd->t) {
    case IR_LIT_I:
        fprintf(out, "rt.push_int(%lld); ", nd->ival);
        break;
    case IR_LIT_S: {
        fprintf(out, "rt.push_str(");
        js_escape_string(out, nd->sval);
        fprintf(out, ", %d); ", (int)strlen(nd->sval ? nd->sval : ""));
        break;
    }
    case IR_LIT_F:
        fprintf(out, "rt.push_real_bits(%.17g); ", nd->dval);
        break;
    case IR_LIT_NUL:
        fprintf(out, "rt.push_null(); ");
        break;
    case IR_VAR:
        fprintf(out, "rt.push_var(\"");
        if (nd->sval) fprintf(out, "%s", nd->sval);
        fprintf(out, "\"); ");
        break;
    case IR_ASSIGN:
        fprintf(out, "rt.store_var(\"");
        if (nd->sval) fprintf(out, "%s", nd->sval);
        fprintf(out, "\"); ");
        break;
    case IR_UNOP:
        if (nd->ival == 1) fprintf(out, "rt.neg(); ");  /* NEG */
        else fprintf(out, "/* unop %lld */ ", nd->ival);
        break;
    case IR_BINOP:
        switch (nd->ival) {
        case 1: fprintf(out, "rt.arith('add'); "); break;  /* ADD */
        case 2: fprintf(out, "rt.arith('sub'); "); break;  /* SUB */
        case 3: fprintf(out, "rt.arith('mul'); "); break;  /* MUL */
        case 4: fprintf(out, "rt.arith('div'); "); break;  /* DIV */
        case 5: fprintf(out, "rt.concat(); "); break;      /* CAT */
        case 10: fprintf(out, "rt.acomp('eq'); "); break;  /* EQ */
        case 11: fprintf(out, "rt.acomp('ne'); "); break;  /* NE */
        case 12: fprintf(out, "rt.acomp('lt'); "); break;  /* LT */
        case 13: fprintf(out, "rt.acomp('le'); "); break;  /* LE */
        case 14: fprintf(out, "rt.acomp('gt'); "); break;  /* GT */
        case 15: fprintf(out, "rt.acomp('ge'); "); break;  /* GE */
        case 20: fprintf(out, "rt.lcomp('eq'); "); break;  /* LEQ */
        case 21: fprintf(out, "rt.lcomp('ne'); "); break;  /* LNE */
        case 22: fprintf(out, "rt.lcomp('lt'); "); break;  /* LLT */
        case 23: fprintf(out, "rt.lcomp('le'); "); break;  /* LLE */
        case 24: fprintf(out, "rt.lcomp('gt'); "); break;  /* LGT */
        case 25: fprintf(out, "rt.lcomp('ge'); "); break;  /* LGE */
        default: fprintf(out, "/* binop %lld */ ", nd->ival);
        }
        break;
    case IR_CALL:
        fprintf(out, "rt.call(\"");
        if (nd->sval) fprintf(out, "%s", nd->sval);
        fprintf(out, "\", %lld); ", nd->ival);
        break;
    case IR_SUCCEED:
        fprintf(out, "rt.set_last_ok(1); ");
        break;
    case IR_FAIL:
        fprintf(out, "rt.set_last_ok(0); ");
        break;
    case IR_SEQ:
        /* No-op — sequencing handled by statement order */
        break;
    case IR_SCAN:
        /* Pattern matching — implemented via factories */
        if (nd->c && nd->c[0]) emit_js_generator(nd->c[0], out);
        break;
    case IR_GOTO:
        fprintf(out, "_pc=%lld; continue; ", nd->ival);
        break;
    case IR_RETURN:
        fprintf(out, "rt.do_return(%lld, %lld); ", nd->ival, nd->ival2);
        break;
    default:
        fprintf(out, "/* scalar stub kind=%d */ ", nd->t);
    }
    return 0;
}
/*----------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------*/
/* emit_js_prologue — emit JS file header, runtime setup, and switch loop entry. */
int emit_js_prologue(IR_block_t * cfg, FILE * out) {
    fprintf(out, "'use strict';\n");
    fprintf(out, "const rt = require('/home/claude/one4all/src/runtime/js/sno_runtime.js');\n");
    fprintf(out, "rt._init();\n");
    fprintf(out, "let _pc = 0;\n");
    fprintf(out, "loop: while (true) { switch (_pc) {\n");
    return 0;
}
/*----------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------*/
/* emit_js_epilogue — close switch loop, finalize runtime. */
int emit_js_epilogue(IR_block_t * cfg, FILE * out) {
    (void)cfg;
    fprintf(out, "default: break loop;\n");
    fprintf(out, "}} rt._finalize();\n");
    return 0;
}
