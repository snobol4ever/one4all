#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include "emit_ir.h"
#include "sm_prog.h"
#include "../ast/ast.h"
/*----------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------*/
/* emit_wasm.c — WASM emitter for SNOBOL4.
   BB generator nodes: each emit_wasm_bb_NODE writes a $bb_sid_nid_new WAT func that allocates an arena
   slot via $arena_alloc, writes the node's payload, and returns the handle.  The α/β bodies come
   verbatim from bb_boxes.wat included at the top of every emitted .wat file.
   Scalar nodes: emit_wasm_from_sm walks SM_Program and emits a $main WAT function using a
   block/br_table dispatch loop (since WASM has no goto).
   Entry point: emit_wasm_program(ast_prog, out). */
/*----------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------*/
extern SM_Program * sm_preamble(const tree_t * ast_prog);
/*----------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------*/
/* String table — deduplicate literal strings into (data ...) segments.
   Strings are placed starting at STR_DATA_BASE = 0x50000 (above runtime fixed data at 0x31010).
   We keep a flat list of (sval, address, length) triples. */
#define STRTAB_MAX 1024
#define STR_DATA_BASE 0x50000
typedef struct { const char * s; int addr; int len; } StrEntry;
static StrEntry g_strtab[STRTAB_MAX];
static int g_strtab_n   = 0;
static int g_str_next   = STR_DATA_BASE;
/*----------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------*/
static void strtab_reset(void) { g_strtab_n = 0; g_str_next = STR_DATA_BASE; }
/*----------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------*/
/* intern_str: add string to table if not present; return address. */
static int intern_str(const char * s) {
    int len = s ? (int)strlen(s) : 0;
    for (int i = 0; i < g_strtab_n; i++)
        if (g_strtab[i].len == len && (len == 0 || memcmp(g_strtab[i].s, s, len) == 0)) return g_strtab[i].addr;
    if (g_strtab_n >= STRTAB_MAX) return STR_DATA_BASE;
    int addr = g_str_next;
    g_strtab[g_strtab_n].s    = s;
    g_strtab[g_strtab_n].addr = addr;
    g_strtab[g_strtab_n].len  = len;
    g_strtab_n++;
    g_str_next += len + 1;
    return addr;
}
/*----------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------*/
/* emit_wasm_data_segments: emit (data ...) for all interned strings. */
static void emit_wasm_data_segments(FILE * out) {
    for (int i = 0; i < g_strtab_n; i++) {
        fprintf(out, "  (data (i32.const 0x%x) \"", g_strtab[i].addr);
        const char * s = g_strtab[i].s;
        for (int j = 0; j < g_strtab[i].len; j++) {
            unsigned char c = (unsigned char)s[j];
            if      (c == '"')  fprintf(out, "\\\"");
            else if (c == '\\') fprintf(out, "\\\\");
            else if (c < 0x20 || c > 0x7e) fprintf(out, "\\%02x", c);
            else fputc(c, out);
        }
        fprintf(out, "\")\n");
    }
}
/*----------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------*/
/* ── BB constructor emitters ─────────────────────────────────────────────────────────────────────────────────────────────────────────────────────────────────────────────────────────────────────── */
/*----------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------*/
/* emit_wasm_new_header: emit start of a _new constructor func; returns handle via $arena_alloc. */
static void wasm_new_hdr(FILE * out, int sid, int nid) {
    fprintf(out, "  (func $bb_%d_%d_new (result i32)\n", sid, nid);
    fprintf(out, "    (local $h i32)\n");
    fprintf(out, "    (local.set $h (call $arena_alloc))\n");
}
static void wasm_new_store4(FILE * out, int off, int val_expr_printed) { (void)val_expr_printed; (void)off; }
/*----------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------*/
static void emit_wasm_bb_lit(IR_t * nd, FILE * out, int sid, int nid) {
    int addr = intern_str(nd->sval);
    int len  = nd->sval ? (int)strlen(nd->sval) : 0;
    wasm_new_hdr(out, sid, nid);
    fprintf(out, "    (i32.store (i32.add (local.get $h) (i32.const 4)) (i32.const 0x%x))\n", addr);
    fprintf(out, "    (i32.store (i32.add (local.get $h) (i32.const 8)) (i32.const %d))\n", len);
    fprintf(out, "    (local.get $h)\n  )\n");
}
/*----------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------*/
static void emit_wasm_bb_any(IR_t * nd, FILE * out, int sid, int nid) {
    int addr = intern_str(nd->sval);
    wasm_new_hdr(out, sid, nid);
    fprintf(out, "    (i32.store (i32.add (local.get $h) (i32.const 4)) (i32.const 0x%x))\n", addr);
    fprintf(out, "    (local.get $h)\n  )\n");
}
/*----------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------*/
static void emit_wasm_bb_notany(IR_t * nd, FILE * out, int sid, int nid) {
    int addr = intern_str(nd->sval);
    wasm_new_hdr(out, sid, nid);
    fprintf(out, "    (i32.store (i32.add (local.get $h) (i32.const 4)) (i32.const 0x%x))\n", addr);
    fprintf(out, "    (local.get $h)\n  )\n");
}
/*----------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------*/
static void emit_wasm_bb_span(IR_t * nd, FILE * out, int sid, int nid) {
    int addr = intern_str(nd->sval);
    wasm_new_hdr(out, sid, nid);
    fprintf(out, "    (i32.store (i32.add (local.get $h) (i32.const 4)) (i32.const 0x%x))\n", addr);
    fprintf(out, "    (local.get $h)\n  )\n");
}
/*----------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------*/
static void emit_wasm_bb_break(IR_t * nd, FILE * out, int sid, int nid) {
    int addr = intern_str(nd->sval);
    wasm_new_hdr(out, sid, nid);
    fprintf(out, "    (i32.store (i32.add (local.get $h) (i32.const 4)) (i32.const 0x%x))\n", addr);
    fprintf(out, "    (local.get $h)\n  )\n");
}
/*----------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------*/
static void emit_wasm_bb_len(IR_t * nd, FILE * out, int sid, int nid) {
    wasm_new_hdr(out, sid, nid);
    fprintf(out, "    (i32.store (i32.add (local.get $h) (i32.const 4)) (i32.const %lld))\n", (long long)nd->ival);
    fprintf(out, "    (local.get $h)\n  )\n");
}
/*----------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------*/
/* IR_PAT_POS: nd->n == 0 → absolute POS, nd->n == 1 → relative RPOS */
static void emit_wasm_bb_pos(IR_t * nd, FILE * out, int sid, int nid) {
    const char * fn = (nd->n == 1) ? "bb_rpos_new" : "bb_pos_new";
    wasm_new_hdr(out, sid, nid);
    fprintf(out, "    (i32.store (i32.add (local.get $h) (i32.const 4)) (i32.const %lld))\n", (long long)nd->ival);
    fprintf(out, "    (local.get $h)\n  )\n");
    /* tag in slot so dispatcher can route to correct α/β */
    (void)fn;
}
/*----------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------*/
/* IR_PAT_TAB: nd->n == 0 → TAB, nd->n == 1 → RTAB */
static void emit_wasm_bb_tab(IR_t * nd, FILE * out, int sid, int nid) {
    wasm_new_hdr(out, sid, nid);
    fprintf(out, "    (i32.store (i32.add (local.get $h) (i32.const 4)) (i32.const %lld))\n", (long long)nd->ival);
    fprintf(out, "    (local.get $h)\n  )\n");
}
/*----------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------*/
static void emit_wasm_bb_rem(IR_t * nd, FILE * out, int sid, int nid) {
    (void)nd;
    wasm_new_hdr(out, sid, nid);
    fprintf(out, "    (local.get $h)\n  )\n");
}
/*----------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------*/
static void emit_wasm_bb_arb(IR_t * nd, FILE * out, int sid, int nid) {
    (void)nd;
    wasm_new_hdr(out, sid, nid);
    fprintf(out, "    (local.get $h)\n  )\n");
}
/*----------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------*/
static void emit_wasm_bb_arbno(IR_t * nd, FILE * out, int sid, int nid) {
    /* child handle will be wired by wire_pat after all nodes are created */
    (void)nd;
    wasm_new_hdr(out, sid, nid);
    fprintf(out, "    (local.get $h)\n  )\n");
}
/*----------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------*/
static void emit_wasm_bb_cat(IR_t * nd, FILE * out, int sid, int nid) {
    (void)nd;
    wasm_new_hdr(out, sid, nid);
    fprintf(out, "    (local.get $h)\n  )\n");
}
/*----------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------*/
static void emit_wasm_bb_alt(IR_t * nd, FILE * out, int sid, int nid) {
    (void)nd;
    wasm_new_hdr(out, sid, nid);
    fprintf(out, "    (local.get $h)\n  )\n");
}
/*----------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------*/
static void emit_wasm_bb_assign_imm(IR_t * nd, FILE * out, int sid, int nid) {
    int addr = intern_str(nd->sval);
    wasm_new_hdr(out, sid, nid);
    fprintf(out, "    (i32.store (i32.add (local.get $h) (i32.const 4)) (i32.const 0x%x))\n", addr);
    fprintf(out, "    (local.get $h)\n  )\n");
}
/*----------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------*/
static void emit_wasm_bb_assign_cond(IR_t * nd, FILE * out, int sid, int nid) {
    int addr = intern_str(nd->sval);
    wasm_new_hdr(out, sid, nid);
    fprintf(out, "    (i32.store (i32.add (local.get $h) (i32.const 4)) (i32.const 0x%x))\n", addr);
    fprintf(out, "    (local.get $h)\n  )\n");
}
/*----------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------*/
static void emit_wasm_bb_fence(IR_t * nd, FILE * out, int sid, int nid) {
    (void)nd;
    wasm_new_hdr(out, sid, nid);
    fprintf(out, "    (local.get $h)\n  )\n");
}
/*----------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------*/
static void emit_wasm_bb_abort(IR_t * nd, FILE * out, int sid, int nid) {
    (void)nd;
    wasm_new_hdr(out, sid, nid);
    fprintf(out, "    (local.get $h)\n  )\n");
}
/*----------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------*/
static void emit_wasm_bb_callout(IR_t * nd, FILE * out, int sid, int nid) {
    (void)nd;
    wasm_new_hdr(out, sid, nid);
    fprintf(out, "    (local.get $h)\n  )\n");
}
/*----------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------*/
/* emit_wasm_generator — dispatch to the correct constructor emitter. */
int emit_wasm_generator(IR_t * nd, FILE * out) {
    if (!nd || !out) return 0;
    int sid = nd->ival ? (int)nd->ival : 0;
    int nid = ir_node_id(nd);
    switch (nd->t) {
    case IR_PAT_LIT:         emit_wasm_bb_lit(nd, out, sid, nid);         break;
    case IR_PAT_ANY:         emit_wasm_bb_any(nd, out, sid, nid);         break;
    case IR_PAT_NOTANY:      emit_wasm_bb_notany(nd, out, sid, nid);      break;
    case IR_PAT_SPAN:        emit_wasm_bb_span(nd, out, sid, nid);        break;
    case IR_PAT_BREAK:       emit_wasm_bb_break(nd, out, sid, nid);       break;
    case IR_PAT_LEN:         emit_wasm_bb_len(nd, out, sid, nid);         break;
    case IR_PAT_POS:         emit_wasm_bb_pos(nd, out, sid, nid);         break;
    case IR_PAT_TAB:         emit_wasm_bb_tab(nd, out, sid, nid);         break;
    case IR_PAT_REM:         emit_wasm_bb_rem(nd, out, sid, nid);         break;
    case IR_PAT_ARB:         emit_wasm_bb_arb(nd, out, sid, nid);         break;
    case IR_PAT_ARBNO:       emit_wasm_bb_arbno(nd, out, sid, nid);       break;
    case IR_PAT_CAT:         emit_wasm_bb_cat(nd, out, sid, nid);         break;
    case IR_PAT_ALT:         emit_wasm_bb_alt(nd, out, sid, nid);         break;
    case IR_PAT_ASSIGN_IMM:  emit_wasm_bb_assign_imm(nd, out, sid, nid);  break;
    case IR_PAT_ASSIGN_COND: emit_wasm_bb_assign_cond(nd, out, sid, nid); break;
    case IR_PAT_FENCE:       emit_wasm_bb_fence(nd, out, sid, nid);       break;
    case IR_PAT_ABORT:       emit_wasm_bb_abort(nd, out, sid, nid);       break;
    case IR_PAT_CALLOUT:     emit_wasm_bb_callout(nd, out, sid, nid);     break;
    default:
        fprintf(out, "  ;; emit_wasm_generator: unhandled kind %d\n", nd->t);
        break;
    }
    return 0;
}
/*----------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------*/
/* ── Scalar SM walker ────────────────────────────────────────────────────────────────────────────────────────────────────────────────────────────────────────────────────────────────────────────── */
/*----------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------*/
/* emit_wasm_from_sm: walk SM_Program; emit a flat if-else dispatch loop.
   Each instruction i is guarded by (if (i32.eq $pc i) ...).
   This avoids deeply-nested blocks that exceed WASM validator limits.
   Jump targets set $pc directly and br $lp to re-dispatch. */
int emit_wasm_from_sm(SM_Program * sm, FILE * out) {
    if (!sm || !out || sm->count == 0) return 0;
    int n = sm->count;
    fprintf(out, "    (block $done\n");
    fprintf(out, "      (loop $lp\n");
    for (int i = 0; i < n; i++) {
        SM_Instr * ins = &sm->instrs[i];
        int has_jump = 0;
        /* guard: only execute instruction i when $pc == i */
        fprintf(out, "        (if (i32.eq (local.get $pc) (i32.const %d)) (then\n", i);
        switch (ins->op) {
        case SM_STNO:
            fprintf(out, "          (call $sno_set_stno (i32.const %lld))\n", ins->a[0].i);
            break;
        case SM_LABEL:
            break;
        case SM_PUSH_LIT_I:
            fprintf(out, "          (call $sno_push_int (i32.const %lld))\n", ins->a[0].i);
            break;
        case SM_PUSH_LIT_S: {
            int addr = intern_str(ins->a[0].s);
            int len  = ins->a[0].s ? (int)strlen(ins->a[0].s) : 0;
            fprintf(out, "          (call $sno_push_str (i32.const 0x%x) (i32.const %d))\n", addr, len);
            break;
        }
        case SM_PUSH_LIT_CS: {
            int addr = intern_str(ins->a[0].s);
            int len  = ins->a[0].s ? (int)strlen(ins->a[0].s) : 0;
            fprintf(out, "          (call $sno_push_str (i32.const 0x%x) (i32.const %d))\n", addr, len);
            break;
        }
        case SM_PUSH_LIT_F:
            fprintf(out, "          (call $sno_push_real (f64.const %.17g))\n", ins->a[0].f);
            break;
        case SM_PUSH_NULL:
        case SM_PUSH_NULL_NOFLIP:
            fprintf(out, "          (call $sno_push_null)\n");
            break;
        case SM_PUSH_VAR: {
            int addr = intern_str(ins->a[0].s);
            int len  = ins->a[0].s ? (int)strlen(ins->a[0].s) : 0;
            fprintf(out, "          (call $sno_push_var (i32.const 0x%x) (i32.const %d))\n", addr, len);
            break;
        }
        case SM_STORE_VAR: {
            int addr = intern_str(ins->a[0].s);
            int len  = ins->a[0].s ? (int)strlen(ins->a[0].s) : 0;
            fprintf(out, "          (call $sno_store_var (i32.const 0x%x) (i32.const %d))\n", addr, len);
            break;
        }
        case SM_VOID_POP:
            fprintf(out, "          (call $sno_pop_void)\n");
            break;
        case SM_CONCAT:
            fprintf(out, "          (call $sno_concat)\n");
            break;
        case SM_NEG:
            fprintf(out, "          (call $sno_neg)\n");
            break;
        case SM_COERCE_NUM:
            fprintf(out, "          (call $sno_coerce_num)\n");
            break;
        case SM_EXP:
            fprintf(out, "          (call $sno_exp_op)\n");
            break;
        case SM_ADD:  fprintf(out, "          (call $sno_arith (i32.const 0))\n"); break;
        case SM_SUB:  fprintf(out, "          (call $sno_arith (i32.const 1))\n"); break;
        case SM_MUL:  fprintf(out, "          (call $sno_arith (i32.const 2))\n"); break;
        case SM_DIV:  fprintf(out, "          (call $sno_arith (i32.const 3))\n"); break;
        case SM_MOD:  fprintf(out, "          (call $sno_arith (i32.const 4))\n"); break;
        case SM_LCOMP:
            fprintf(out, "          (call $sno_lcomp (i32.const %lld))\n", ins->a[0].i);
            break;
        case SM_ACOMP:
            fprintf(out, "          (call $sno_acomp (i32.const %lld))\n", ins->a[0].i);
            break;
        case SM_HALT:
            fprintf(out, "          (call $sno_halt_tos)\n");
            fprintf(out, "          (br $done)\n");
            has_jump = 1;
            break;
        case SM_JUMP:
            fprintf(out, "          (i32.const %lld) (local.set $pc) (br $lp)\n", ins->a[0].i);
            has_jump = 1;
            break;
        case SM_JUMP_S:
            fprintf(out, "          (if (call $sno_last_ok)\n");
            fprintf(out, "            (then (i32.const %lld) (local.set $pc))\n", ins->a[0].i);
            fprintf(out, "            (else (i32.const %d)   (local.set $pc)))\n", i + 1);
            fprintf(out, "          (br $lp)\n");
            has_jump = 1;
            break;
        case SM_JUMP_F:
            fprintf(out, "          (if (i32.eqz (call $sno_last_ok))\n");
            fprintf(out, "            (then (i32.const %lld) (local.set $pc))\n", ins->a[0].i);
            fprintf(out, "            (else (i32.const %d)   (local.set $pc)))\n", i + 1);
            fprintf(out, "          (br $lp)\n");
            has_jump = 1;
            break;
        case SM_CALL_FN: {
            int addr = intern_str(ins->a[0].s);
            int len  = ins->a[0].s ? (int)strlen(ins->a[0].s) : 0;
            fprintf(out, "          (call $sno_call (i32.const 0x%x) (i32.const %d) (i32.const %lld))\n",
                    addr, len, ins->a[1].i);
            break;
        }
        case SM_RETURN:
        case SM_FRETURN:
        case SM_NRETURN:
        case SM_RETURN_S:
        case SM_RETURN_F:
        case SM_FRETURN_S:
        case SM_FRETURN_F:
        case SM_NRETURN_S:
        case SM_NRETURN_F:
            fprintf(out, "          (call $sno_do_return (i32.const 0) (i32.const 0))\n");
            break;
        case SM_INCR:
        case SM_DECR:
            fprintf(out, "          (call $sno_arith (i32.const %d))\n", ins->op == SM_INCR ? 0 : 1);
            break;
        default:
            fprintf(out, "          ;; unhandled SM opcode %d\n", ins->op);
            break;
        }
        if (!has_jump) {
            fprintf(out, "          (i32.const %d) (local.set $pc) (br $lp)\n", i + 1);
        }
        fprintf(out, "        ))\n");  /* close (if ...) (then ...) */
    }
    /* If $pc falls off end (no instruction matched), exit */
    fprintf(out, "        (br $done)\n");
    fprintf(out, "      ) ;; end loop $lp\n");
    fprintf(out, "    ) ;; end block $done\n");
    return 0;
}

/*----------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------*/
/* emit_wasm_prologue: emit WAT module header, imports, and open $main. */
static int emit_wasm_prologue(FILE * out, SM_Program * sm) {
    fprintf(out, "(module\n");
    fprintf(out, "  ;; imports from sno_runtime\n");
    fprintf(out, "  (import \"sno\" \"memory\"          (memory 8))\n");
    fprintf(out, "  (import \"sno\" \"sno_init\"         (func $sno_init))\n");
    fprintf(out, "  (import \"sno\" \"sno_finalize\"     (func $sno_finalize))\n");
    fprintf(out, "  (import \"sno\" \"sno_push_int\"     (func $sno_push_int    (param i32)))\n");
    fprintf(out, "  (import \"sno\" \"sno_push_str\"     (func $sno_push_str    (param i32 i32)))\n");
    fprintf(out, "  (import \"sno\" \"sno_push_real\"    (func $sno_push_real   (param f64)))\n");
    fprintf(out, "  (import \"sno\" \"sno_push_null\"    (func $sno_push_null))\n");
    fprintf(out, "  (import \"sno\" \"sno_push_var\"     (func $sno_push_var    (param i32 i32)))\n");
    fprintf(out, "  (import \"sno\" \"sno_store_var\"    (func $sno_store_var   (param i32 i32)))\n");
    fprintf(out, "  (import \"sno\" \"sno_pop_void\"     (func $sno_pop_void))\n");
    fprintf(out, "  (import \"sno\" \"sno_concat\"       (func $sno_concat))\n");
    fprintf(out, "  (import \"sno\" \"sno_neg\"          (func $sno_neg))\n");
    fprintf(out, "  (import \"sno\" \"sno_exp_op\"       (func $sno_exp_op))\n");
    fprintf(out, "  (import \"sno\" \"sno_coerce_num\"   (func $sno_coerce_num))\n");
    fprintf(out, "  (import \"sno\" \"sno_arith\"        (func $sno_arith       (param i32)))\n");
    fprintf(out, "  (import \"sno\" \"sno_acomp\"        (func $sno_acomp       (param i32)))\n");
    fprintf(out, "  (import \"sno\" \"sno_lcomp\"        (func $sno_lcomp       (param i32)))\n");
    fprintf(out, "  (import \"sno\" \"sno_last_ok\"      (func $sno_last_ok     (result i32)))\n");
    fprintf(out, "  (import \"sno\" \"sno_set_last_ok\"  (func $sno_set_last_ok (param i32)))\n");
    fprintf(out, "  (import \"sno\" \"sno_set_stno\"     (func $sno_set_stno   (param i32)))\n");
    fprintf(out, "  (import \"sno\" \"sno_halt_tos\"     (func $sno_halt_tos))\n");
    fprintf(out, "  (import \"sno\" \"sno_call\"         (func $sno_call        (param i32 i32 i32)))\n");
    fprintf(out, "  (import \"sno\" \"sno_do_return\"    (func $sno_do_return   (param i32 i32)))\n");
    fprintf(out, "  ;; arena allocator (from bb_boxes.wat)\n");
    fprintf(out, "  (import \"bb\"  \"arena_alloc\"      (func $arena_alloc     (result i32)))\n");
    (void)sm;
    return 0;
}
/*----------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------*/
/* emit_wasm_epilogue: close $main, emit data segments, close module. */
static int emit_wasm_epilogue(FILE * out) {
    fprintf(out, "  ;; string data segments\n");
    emit_wasm_data_segments(out);
    fprintf(out, ")\n");
    return 0;
}
/*----------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------*/
/* emit_wasm_scalar: vtable callback for scalar IR nodes (not used in SM-walk path; kept for IR walk). */
int emit_wasm_scalar(IR_t * nd, FILE * out) {
    if (!nd || !out) return 0;
    fprintf(out, "  ;; wasm scalar kind=%d (handled via SM walk)\n", nd->t);
    return 0;
}
/*----------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------*/
/* emit_wasm_program: main entry point.
   1. Build SM_Program from AST.
   2. First-pass SM walk to intern all strings into the string table.
   3. Emit WAT module: prologue (imports) + BB _new constructors + $main + data + close. */
int emit_wasm_program(const tree_t * ast_prog, FILE * out) {
    if (!ast_prog || !out) return 1;
    strtab_reset();
    SM_Program * sm = sm_preamble(ast_prog);
    if (!sm) return 1;
    /* Pre-pass: intern all strings so data segments are emitted before $main references them. */
    for (int i = 0; i < sm->count; i++) {
        SM_Instr * ins = &sm->instrs[i];
        if (ins->op == SM_PUSH_LIT_S && ins->a[0].s) intern_str(ins->a[0].s);
        else if (ins->op == SM_PUSH_VAR  && ins->a[0].s) intern_str(ins->a[0].s);
        else if (ins->op == SM_STORE_VAR && ins->a[0].s) intern_str(ins->a[0].s);
        else if (ins->op == SM_CALL_FN   && ins->a[0].s) intern_str(ins->a[0].s);
    }
    /* intern fixed keyword strings */
    intern_str("OUTPUT");
    intern_str("INPUT");
    emit_wasm_prologue(out, sm);
    /* $main function */
    fprintf(out, "  (func $main (export \"main\")\n");
    fprintf(out, "    (local $pc i32)\n");
    fprintf(out, "    (local $tmp i32)\n");
    fprintf(out, "    (call $sno_init)\n");
    emit_wasm_from_sm(sm, out);
    fprintf(out, "    (call $sno_finalize)\n");
    fprintf(out, "  )\n");
    emit_wasm_epilogue(out);
    sm_prog_free(sm);
    return 0;
}
/*----------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------*/
/* vtable registration for the IR walk path (used by emit_ir_block). */
IR_emit_vtable_t g_emit_vtable_wasm = {
    "wasm",
    emit_wasm_scalar,
    emit_wasm_generator,
    NULL,
    NULL
};
