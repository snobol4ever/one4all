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
   Strings are placed starting at STR_DATA_BASE = 0x100000 (1MB).  This is high enough to be safe
   above all runtime regions: stack (0x00000..0x0FFFF), var table (0x20000..0x2FFFF), static
   literals (0x30000..0x3FFFF), output buffer (0x40000..0x4FFFF), BB arena (0x50000..0x5FFFF),
   and the dynamic string heap (which bumps upward from 0x60000).  The memory is 64 pages (4MB)
   total, so we have 1MB headroom for program literals.
   We keep a flat list of (sval, address, length) triples. */
#define STRTAB_MAX 4096
#define STR_DATA_BASE 0x100000
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
/* intern_name: case-fold name to uppercase (SNOBOL4 default), then intern.
   Used for variable / function / keyword names — anywhere case-insensitive identifier
   semantics apply.  Literal string content (PUSH_LIT_S payloads) MUST use intern_str. */
static int intern_name(const char * s) {
    if (!s) return intern_str(s);
    int len = (int)strlen(s);
    static char buf[256];
    if (len >= (int)sizeof(buf)) len = (int)sizeof(buf) - 1;
    for (int i = 0; i < len; i++) {
        unsigned char c = (unsigned char)s[i];
        buf[i] = (c >= 'a' && c <= 'z') ? (char)(c - 32) : (char)c;
    }
    buf[len] = 0;
    /* Linear lookup over existing strtab (case-sensitive against the upper-cased version). */
    for (int i = 0; i < g_strtab_n; i++)
        if (g_strtab[i].len == len && (len == 0 || memcmp(g_strtab[i].s, buf, (size_t)len) == 0)) return g_strtab[i].addr;
    /* Allocate a persistent copy because buf is static. */
    char * persistent = (char *)malloc((size_t)len + 1);
    memcpy(persistent, buf, (size_t)len);
    persistent[len] = 0;
    if (g_strtab_n >= STRTAB_MAX) { free(persistent); return STR_DATA_BASE; }
    int addr = g_str_next;
    g_strtab[g_strtab_n].s    = persistent;
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
/* User-defined function table for SN4-WASM-5f.
   Populated by a pre-scan over SM_Program before $main emission.
   Each entry: name, entry_pc, param names (parsed from preceding SM_PUSH_LIT_S "FNAME(P1,P2,...)").
   Lookup is linear (case-sensitive — names already canonical at this layer). */
#define USERFNS_MAX     1024
#define USERFN_PARAMS_MAX 32
typedef struct {
    char * name;
    int    entry_pc;
    int    nparams;
    char * params[USERFN_PARAMS_MAX];
} UserFn;
static UserFn g_userfns[USERFNS_MAX];
static int    g_userfns_n = 0;
/*----------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------*/
static void userfns_reset(void) {
    for (int i = 0; i < g_userfns_n; i++) {
        free(g_userfns[i].name);
        for (int k = 0; k < g_userfns[i].nparams; k++) free(g_userfns[i].params[k]);
    }
    g_userfns_n = 0;
}
/*----------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------*/
static UserFn * userfn_find(const char * name) {
    if (!name) return NULL;
    int len = (int)strlen(name);
    for (int i = 0; i < g_userfns_n; i++) {
        if (!g_userfns[i].name) continue;
        if ((int)strlen(g_userfns[i].name) != len) continue;
        int match = 1;
        for (int k = 0; k < len; k++) {
            unsigned char a = (unsigned char)name[k];
            unsigned char b = (unsigned char)g_userfns[i].name[k];
            if (a >= 'a' && a <= 'z') a = (unsigned char)(a - 32);
            if (b >= 'a' && b <= 'z') b = (unsigned char)(b - 32);
            if (a != b) { match = 0; break; }
        }
        if (match) return &g_userfns[i];
    }
    return NULL;
}
/*----------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------*/
/* parse_define_signature: input like "FACT(X)" or "DOIT(X,Y)" or "FOO()" — populates fn->name & params.
   The fn->name is set to the part before '(' (caller already knows the name, but we store it anyway).
   Returns 0 on success, nonzero on parse failure (in which case caller should leave entry alone). */
static int parse_define_signature(UserFn * fn, const char * sig) {
    if (!sig) return 1;
    const char * lp = strchr(sig, '(');
    if (!lp) {
        fn->nparams = 0;
        return 0;
    }
    const char * p = lp + 1;
    while (*p && *p != ')') {
        while (*p == ' ' || *p == '\t' || *p == ',') p++;
        if (*p == ')' || !*p) break;
        const char * start = p;
        while (*p && *p != ',' && *p != ')' && *p != ' ' && *p != '\t') p++;
        int len = (int)(p - start);
        if (len > 0 && fn->nparams < USERFN_PARAMS_MAX) {
            char * nm = (char *)malloc((size_t)len + 1);
            memcpy(nm, start, (size_t)len);
            nm[len] = 0;
            fn->params[fn->nparams++] = nm;
        }
    }
    return 0;
}
/*----------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------*/
/* pre_scan_userfns: walk SM_Program and collect all SM_LABEL with define_entry (a[2].i=1).
   For each, find the most recent SM_PUSH_LIT_S preceding an SM_SUSPEND_VALUE s="DEFINE" with this fn name
   to extract the parameter list.  Also intern fn name into the string table for later use. */
static void pre_scan_userfns(SM_Program * sm) {
    userfns_reset();
    for (int i = 0; i < sm->count; i++) {
        SM_Instr * ins = &sm->instrs[i];
        if (ins->op == SM_LABEL && ins->a[2].i == 1 && ins->a[0].s && ins->a[0].s[0]) {
            if (g_userfns_n >= USERFNS_MAX) break;
            UserFn * fn = &g_userfns[g_userfns_n++];
            fn->name = strdup(ins->a[0].s);
            fn->entry_pc = i;
            fn->nparams = 0;
            /* Scan backward for the DEFINE("FNAME(...)") signature.
               DEFINE in SNOBOL4 is dispatched through SM_CALL_FN (not SM_SUSPEND_VALUE);
               accept either opcode for robustness across frontends. */
            for (int j = i - 1; j >= 0; j--) {
                SM_Instr * jn = &sm->instrs[j];
                int is_call = (jn->op == SM_CALL_FN || jn->op == SM_SUSPEND_VALUE);
                if (is_call && jn->a[0].s && strcmp(jn->a[0].s, "DEFINE") == 0) {
                    /* The previous SM_PUSH_LIT_S should be the signature string. */
                    if (j >= 1
                        && (sm->instrs[j-1].op == SM_PUSH_LIT_S
                            || sm->instrs[j-1].op == SM_PUSH_LIT_CS)
                        && sm->instrs[j-1].a[0].s) {
                        const char * sig = sm->instrs[j-1].a[0].s;
                        const char * lp  = strchr(sig, '(');
                        int namelen = lp ? (int)(lp - sig) : (int)strlen(sig);
                        if (namelen > 0 && (int)strlen(fn->name) == namelen
                            && strncmp(sig, fn->name, (size_t)namelen) == 0) {
                            parse_define_signature(fn, sig);
                            break;
                        }
                    }
                }
            }
            intern_name(fn->name);
            for (int k = 0; k < fn->nparams; k++) intern_name(fn->params[k]);
        }
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
            int addr = intern_name(ins->a[0].s);
            int len  = ins->a[0].s ? (int)strlen(ins->a[0].s) : 0;
            fprintf(out, "          (call $sno_push_var (i32.const 0x%x) (i32.const %d))\n", addr, len);
            break;
        }
        case SM_STORE_VAR: {
            int addr = intern_name(ins->a[0].s);
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
            /* SM_CALL_FN with a name = call (builtin or user fn).
               SM_CALL_FN with NULL name = implicit return (end of function body, fall through). */
            const char * cname = ins->a[0].s;
            int nargs = (int)ins->a[1].i;
            if (cname && cname[0]) {
                UserFn * fn = userfn_find(cname);
                if (fn) {
                    /* User-defined function call. */
                    int name_addr = intern_name(fn->name);
                    int name_len  = (int)strlen(fn->name);
                    fprintf(out, "          ;; user-fn call: %s (entry_pc=%d, nparams=%d)\n",
                            fn->name, fn->entry_pc, fn->nparams);
                    /* Open a frame: push returns frame address into a local so save_var calls reference it. */
                    fprintf(out, "          (local.set $fr (call $sno_call_frame_push\n");
                    fprintf(out, "                            (i32.const %d) (i32.const 0x%x) (i32.const %d)))\n",
                            i + 1, name_addr, name_len);
                    /* Save retname binding. */
                    fprintf(out, "          (call $sno_save_var (local.get $fr) (i32.const 0x%x) (i32.const %d))\n",
                            name_addr, name_len);
                    /* Save each param binding. */
                    for (int k = 0; k < fn->nparams; k++) {
                        int p_addr = intern_name(fn->params[k]);
                        int p_len  = (int)strlen(fn->params[k]);
                        fprintf(out, "          (call $sno_save_var (local.get $fr) (i32.const 0x%x) (i32.const %d))\n",
                                p_addr, p_len);
                    }
                    /* Clear retname to empty string. */
                    fprintf(out, "          (call $sno_clear_var (i32.const 0x%x) (i32.const %d))\n",
                            name_addr, name_len);
                    /* Bind formals from value stack (args are pushed in order; topmost = last param).
                       Walk params right-to-left so we pop in reverse. */
                    int nbind = (nargs < fn->nparams) ? nargs : fn->nparams;
                    for (int k = nbind - 1; k >= 0; k--) {
                        int p_addr = intern_name(fn->params[k]);
                        int p_len  = (int)strlen(fn->params[k]);
                        fprintf(out, "          (call $sno_set_var_from_tos (i32.const 0x%x) (i32.const %d))\n",
                                p_addr, p_len);
                    }
                    /* Drop any extra args (nargs > nparams). */
                    for (int k = fn->nparams; k < nargs; k++) {
                        fprintf(out, "          (call $sno_pop_to_null)\n");
    fprintf(out, "  ;; Pattern matching functions (SN4-WASM-5g)\n");
    fprintf(out, "  (import \"sno\" \"sno_pat_lit\"     (func $sno_pat_lit     (param i32 i32)))\n");
    fprintf(out, "  (import \"sno\" \"sno_pat_any\"     (func $sno_pat_any))\n");
    fprintf(out, "  (import \"sno\" \"sno_pat_notany\"  (func $sno_pat_notany))\n");
    fprintf(out, "  (import \"sno\" \"sno_pat_span\"    (func $sno_pat_span))\n");
    fprintf(out, "  (import \"sno\" \"sno_pat_break\"   (func $sno_pat_break))\n");
    fprintf(out, "  (import \"sno\" \"sno_pat_len\"     (func $sno_pat_len))\n");
    fprintf(out, "  (import \"sno\" \"sno_pat_pos\"     (func $sno_pat_pos))\n");
    fprintf(out, "  (import \"sno\" \"sno_pat_rpos\"    (func $sno_pat_rpos))\n");
    fprintf(out, "  (import \"sno\" \"sno_pat_tab\"     (func $sno_pat_tab))\n");
    fprintf(out, "  (import \"sno\" \"sno_pat_rtab\"    (func $sno_pat_rtab))\n");
    fprintf(out, "  (import \"sno\" \"sno_pat_rem\"     (func $sno_pat_rem))\n");
    fprintf(out, "  (import \"sno\" \"sno_pat_arb\"     (func $sno_pat_arb))\n");
    fprintf(out, "  (import \"sno\" \"sno_pat_arbno\"   (func $sno_pat_arbno))\n");
    fprintf(out, "  (import \"sno\" \"sno_pat_bal\"     (func $sno_pat_bal))\n");
    fprintf(out, "  (import \"sno\" \"sno_pat_fail\"    (func $sno_pat_fail))\n");
    fprintf(out, "  (import \"sno\" \"sno_pat_succeed\" (func $sno_pat_succeed))\n");
    fprintf(out, "  (import \"sno\" \"sno_pat_abort\"   (func $sno_pat_abort))\n");
    fprintf(out, "  (import \"sno\" \"sno_pat_fence\"   (func $sno_pat_fence))\n");
    fprintf(out, "  (import \"sno\" \"sno_pat_eps\"     (func $sno_pat_eps))\n");
    fprintf(out, "  (import \"sno\" \"sno_pat_cat\"     (func $sno_pat_cat))\n");
    fprintf(out, "  (import \"sno\" \"sno_pat_alt\"     (func $sno_pat_alt))\n");
    fprintf(out, "  (import \"sno\" \"sno_pat_deref\"   (func $sno_pat_deref))\n");
    fprintf(out, "  (import \"sno\" \"sno_pat_refname\" (func $sno_pat_refname (param i32 i32)))\n");
    fprintf(out, "  (import \"sno\" \"sno_pat_capture\" (func $sno_pat_capture (param i32 i32 i32)))\n");
    fprintf(out, "  (import \"sno\" \"sno_exec_stmt\"   (func $sno_exec_stmt   (param i32 i32 i32)))\n");
                    }
                    /* Clear locals not yet supported (would need scanning DEFINE 3rd-arg signature). */
                    /* Commit the frame. */
                    fprintf(out, "          (call $sno_call_frame_close)\n");
                    /* Jump to entry PC. */
                    fprintf(out, "          (i32.const %d) (local.set $pc) (br $lp)\n", fn->entry_pc);
                    has_jump = 1;
                    break;
                }
                /* Builtin or unknown function — dispatch through $sno_call. */
                int addr = intern_name(cname);
                int len  = (int)strlen(cname);
                fprintf(out, "          (call $sno_call (i32.const 0x%x) (i32.const %d) (i32.const %d))\n",
                        addr, len, nargs);
            } else {
                /* SM_CALL_FN with NULL name = implicit RETURN at end of fn body. */
                fprintf(out, "          (local.set $tmp (call $sno_fn_return (i32.const 0) (i32.const 0)))\n");
                fprintf(out, "          (if (i32.eq (local.get $tmp) (i32.const -2))\n");
                fprintf(out, "            (then (br $done))\n");
                fprintf(out, "            (else (local.set $pc (local.get $tmp)) (br $lp)))\n");
                has_jump = 1;
            }
            break;
        }
        case SM_SUSPEND_VALUE: {
            /* SM_SUSPEND_VALUE s="NAME" nargs=N — call a function (builtin or user-defined).
               This is the primary call opcode used by the SNOBOL4 frontend. */
            const char * cname = ins->a[0].s;
            int nargs = (int)ins->a[1].i;
            if (cname && cname[0]) {
                UserFn * fn = userfn_find(cname);
                if (fn) {
                    int name_addr = intern_name(fn->name);
                    int name_len  = (int)strlen(fn->name);
                    fprintf(out, "          ;; user-fn call (suspend_value): %s\n", fn->name);
                    fprintf(out, "          (local.set $fr (call $sno_call_frame_push\n");
                    fprintf(out, "                            (i32.const %d) (i32.const 0x%x) (i32.const %d)))\n",
                            i + 1, name_addr, name_len);
                    fprintf(out, "          (call $sno_save_var (local.get $fr) (i32.const 0x%x) (i32.const %d))\n",
                            name_addr, name_len);
                    for (int k = 0; k < fn->nparams; k++) {
                        int p_addr = intern_name(fn->params[k]);
                        int p_len  = (int)strlen(fn->params[k]);
                        fprintf(out, "          (call $sno_save_var (local.get $fr) (i32.const 0x%x) (i32.const %d))\n",
                                p_addr, p_len);
                    }
                    fprintf(out, "          (call $sno_clear_var (i32.const 0x%x) (i32.const %d))\n",
                            name_addr, name_len);
                    int nbind = (nargs < fn->nparams) ? nargs : fn->nparams;
                    for (int k = nbind - 1; k >= 0; k--) {
                        int p_addr = intern_name(fn->params[k]);
                        int p_len  = (int)strlen(fn->params[k]);
                        fprintf(out, "          (call $sno_set_var_from_tos (i32.const 0x%x) (i32.const %d))\n",
                                p_addr, p_len);
                    }
                    for (int k = fn->nparams; k < nargs; k++) {
                        fprintf(out, "          (call $sno_pop_to_null)\n");
    fprintf(out, "  ;; Pattern matching functions (SN4-WASM-5g)\n");
    fprintf(out, "  (import \"sno\" \"sno_pat_lit\"     (func $sno_pat_lit     (param i32 i32)))\n");
    fprintf(out, "  (import \"sno\" \"sno_pat_any\"     (func $sno_pat_any))\n");
    fprintf(out, "  (import \"sno\" \"sno_pat_notany\"  (func $sno_pat_notany))\n");
    fprintf(out, "  (import \"sno\" \"sno_pat_span\"    (func $sno_pat_span))\n");
    fprintf(out, "  (import \"sno\" \"sno_pat_break\"   (func $sno_pat_break))\n");
    fprintf(out, "  (import \"sno\" \"sno_pat_len\"     (func $sno_pat_len))\n");
    fprintf(out, "  (import \"sno\" \"sno_pat_pos\"     (func $sno_pat_pos))\n");
    fprintf(out, "  (import \"sno\" \"sno_pat_rpos\"    (func $sno_pat_rpos))\n");
    fprintf(out, "  (import \"sno\" \"sno_pat_tab\"     (func $sno_pat_tab))\n");
    fprintf(out, "  (import \"sno\" \"sno_pat_rtab\"    (func $sno_pat_rtab))\n");
    fprintf(out, "  (import \"sno\" \"sno_pat_rem\"     (func $sno_pat_rem))\n");
    fprintf(out, "  (import \"sno\" \"sno_pat_arb\"     (func $sno_pat_arb))\n");
    fprintf(out, "  (import \"sno\" \"sno_pat_arbno\"   (func $sno_pat_arbno))\n");
    fprintf(out, "  (import \"sno\" \"sno_pat_bal\"     (func $sno_pat_bal))\n");
    fprintf(out, "  (import \"sno\" \"sno_pat_fail\"    (func $sno_pat_fail))\n");
    fprintf(out, "  (import \"sno\" \"sno_pat_succeed\" (func $sno_pat_succeed))\n");
    fprintf(out, "  (import \"sno\" \"sno_pat_abort\"   (func $sno_pat_abort))\n");
    fprintf(out, "  (import \"sno\" \"sno_pat_fence\"   (func $sno_pat_fence))\n");
    fprintf(out, "  (import \"sno\" \"sno_pat_eps\"     (func $sno_pat_eps))\n");
    fprintf(out, "  (import \"sno\" \"sno_pat_cat\"     (func $sno_pat_cat))\n");
    fprintf(out, "  (import \"sno\" \"sno_pat_alt\"     (func $sno_pat_alt))\n");
    fprintf(out, "  (import \"sno\" \"sno_pat_deref\"   (func $sno_pat_deref))\n");
    fprintf(out, "  (import \"sno\" \"sno_pat_refname\" (func $sno_pat_refname (param i32 i32)))\n");
    fprintf(out, "  (import \"sno\" \"sno_pat_capture\" (func $sno_pat_capture (param i32 i32 i32)))\n");
    fprintf(out, "  (import \"sno\" \"sno_exec_stmt\"   (func $sno_exec_stmt   (param i32 i32 i32)))\n");
                    }
                    fprintf(out, "          (call $sno_call_frame_close)\n");
                    fprintf(out, "          (i32.const %d) (local.set $pc) (br $lp)\n", fn->entry_pc);
                    has_jump = 1;
                    break;
                }
                /* Builtin dispatch. */
                int addr = intern_name(cname);
                int len  = (int)strlen(cname);
                fprintf(out, "          (call $sno_call (i32.const 0x%x) (i32.const %d) (i32.const %d))\n",
                        addr, len, nargs);
            } else {
                /* No name — should not happen for SUSPEND_VALUE; emit a no-op. */
                fprintf(out, "          ;; SM_SUSPEND_VALUE with NULL name (no-op)\n");
            }
            break;
        }
        case SM_RETURN:
        case SM_FRETURN:
        case SM_NRETURN: {
            int kind = (ins->op == SM_FRETURN) ? 1 : ((ins->op == SM_NRETURN) ? 2 : 0);
            fprintf(out, "          (local.set $tmp (call $sno_fn_return (i32.const %d) (i32.const 0)))\n", kind);
            fprintf(out, "          (if (i32.eq (local.get $tmp) (i32.const -2))\n");
            fprintf(out, "            (then (br $done))\n");
            fprintf(out, "            (else (local.set $pc (local.get $tmp)) (br $lp)))\n");
            has_jump = 1;
            break;
        }
        case SM_RETURN_S:
        case SM_FRETURN_S:
        case SM_NRETURN_S: {
            int kind = (ins->op == SM_FRETURN_S) ? 1 : ((ins->op == SM_NRETURN_S) ? 2 : 0);
            fprintf(out, "          (local.set $tmp (call $sno_fn_return (i32.const %d) (i32.const 1)))\n", kind);
            fprintf(out, "          (if (i32.eq (local.get $tmp) (i32.const -1))\n");
            fprintf(out, "            (then (i32.const %d) (local.set $pc) (br $lp))\n", i + 1);
            fprintf(out, "            (else (if (i32.eq (local.get $tmp) (i32.const -2))\n");
            fprintf(out, "              (then (br $done))\n");
            fprintf(out, "              (else (local.set $pc (local.get $tmp)) (br $lp)))))\n");
            has_jump = 1;
            break;
        }
        case SM_RETURN_F:
        case SM_FRETURN_F:
        case SM_NRETURN_F: {
            int kind = (ins->op == SM_FRETURN_F) ? 1 : ((ins->op == SM_NRETURN_F) ? 2 : 0);
            fprintf(out, "          (local.set $tmp (call $sno_fn_return (i32.const %d) (i32.const 2)))\n", kind);
            fprintf(out, "          (if (i32.eq (local.get $tmp) (i32.const -1))\n");
            fprintf(out, "            (then (i32.const %d) (local.set $pc) (br $lp))\n", i + 1);
            fprintf(out, "            (else (if (i32.eq (local.get $tmp) (i32.const -2))\n");
            fprintf(out, "              (then (br $done))\n");
            fprintf(out, "              (else (local.set $pc (local.get $tmp)) (br $lp)))))\n");
            has_jump = 1;
            break;
        }
        case SM_INCR:
        case SM_DECR:
            fprintf(out, "          (call $sno_arith (i32.const %d))\n", ins->op == SM_INCR ? 0 : 1);
            break;
        /* Pattern matching opcodes (SN4-WASM-5g) */
        case SM_PAT_LIT: {
            int addr = intern_str(ins->a[0].s ? ins->a[0].s : "");
            fprintf(out, "          (call $sno_pat_lit (i32.const 0x%x) (i32.const %lld))\n", addr, (long long)ins->a[1].i);
            break;
        }
        case SM_PAT_ANY:
            fprintf(out, "          (call $sno_pat_any)\n");
            break;
        case SM_PAT_NOTANY:
            fprintf(out, "          (call $sno_pat_notany)\n");
            break;
        case SM_PAT_SPAN:
            fprintf(out, "          (call $sno_pat_span)\n");
            break;
        case SM_PAT_BREAK:
            fprintf(out, "          (call $sno_pat_break)\n");
            break;
        case SM_PAT_LEN:
            fprintf(out, "          (call $sno_pat_len)\n");
            break;
        case SM_PAT_POS:
            fprintf(out, "          (call $sno_pat_pos)\n");
            break;
        case SM_PAT_RPOS:
            fprintf(out, "          (call $sno_pat_rpos)\n");
            break;
        case SM_PAT_TAB:
            fprintf(out, "          (call $sno_pat_tab)\n");
            break;
        case SM_PAT_RTAB:
            fprintf(out, "          (call $sno_pat_rtab)\n");
            break;
        case SM_PAT_REM:
            fprintf(out, "          (call $sno_pat_rem)\n");
            break;
        case SM_PAT_ARB:
            fprintf(out, "          (call $sno_pat_arb)\n");
            break;
        case SM_PAT_ARBNO:
            fprintf(out, "          (call $sno_pat_arbno)\n");
            break;
        case SM_PAT_BAL:
            fprintf(out, "          (call $sno_pat_bal)\n");
            break;
        case SM_PAT_FAIL:
            fprintf(out, "          (call $sno_pat_fail)\n");
            break;
        case SM_PAT_SUCCEED:
            fprintf(out, "          (call $sno_pat_succeed)\n");
            break;
        case SM_PAT_ABORT:
            fprintf(out, "          (call $sno_pat_abort)\n");
            break;
        case SM_PAT_FENCE0:
        case SM_PAT_FENCE1:
            fprintf(out, "          (call $sno_pat_fence)\n");
            break;
        case SM_PAT_EPS:
            fprintf(out, "          (call $sno_pat_eps)\n");
            break;
        case SM_PAT_CAT:
            fprintf(out, "          (call $sno_pat_cat)\n");
            break;
        case SM_PAT_ALT:
            fprintf(out, "          (call $sno_pat_alt)\n");
            break;
        case SM_PAT_DEREF:
            fprintf(out, "          (call $sno_pat_deref)\n");
            break;
        case SM_PAT_REFNAME: {
            int addr = intern_name(ins->a[0].s ? ins->a[0].s : "");
            fprintf(out, "          (call $sno_pat_refname (i32.const 0x%x) (i32.const %lld))\n", addr, (long long)ins->a[1].i);
            break;
        }
        case SM_PAT_CAPTURE: {
            int addr = intern_name(ins->a[0].s ? ins->a[0].s : "");
            fprintf(out, "          (call $sno_pat_capture (i32.const 0x%x) (i32.const %lld) (i32.const %lld))\n", addr, (long long)ins->a[1].i, (long long)ins->a[2].i);
            break;
        }
        case SM_PAT_CAPTURE_FN:
            fprintf(out, "          ;; SM_PAT_CAPTURE_FN not yet implemented\n");
            break;
        case SM_PAT_CAPTURE_FN_ARGS:
            fprintf(out, "          ;; SM_PAT_CAPTURE_FN_ARGS not yet implemented\n");
            break;
        case SM_PAT_USERCALL:
            fprintf(out, "          ;; SM_PAT_USERCALL not yet implemented\n");
            break;
        case SM_PAT_USERCALL_ARGS:
            fprintf(out, "          ;; SM_PAT_USERCALL_ARGS not yet implemented\n");
            break;
        case SM_EXEC_STMT:
            fprintf(out, "          (call $sno_exec_stmt (i32.const 0) (i32.const 0) (i32.const 0))\n");
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
    fprintf(out, "  (import \"sno\" \"memory\"          (memory 64))\n");
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
    fprintf(out, "  (import \"sno\" \"sno_fn_return\"    (func $sno_fn_return   (param i32 i32) (result i32)))\n");
    fprintf(out, "  (import \"sno\" \"sno_call_frame_push\" (func $sno_call_frame_push (param i32 i32 i32) (result i32)))\n");
    fprintf(out, "  (import \"sno\" \"sno_call_frame_close\" (func $sno_call_frame_close))\n");
    fprintf(out, "  (import \"sno\" \"sno_save_var\"     (func $sno_save_var    (param i32 i32 i32)))\n");
    fprintf(out, "  (import \"sno\" \"sno_clear_var\"    (func $sno_clear_var   (param i32 i32)))\n");
    fprintf(out, "  (import \"sno\" \"sno_set_var_from_tos\" (func $sno_set_var_from_tos (param i32 i32)))\n");
    fprintf(out, "  (import \"sno\" \"sno_pop_to_null\"  (func $sno_pop_to_null))\n");
    fprintf(out, "  ;; Pattern matching functions (SN4-WASM-5g)\n");
    fprintf(out, "  (import \"sno\" \"sno_pat_lit\"     (func $sno_pat_lit     (param i32 i32)))\n");
    fprintf(out, "  (import \"sno\" \"sno_pat_any\"     (func $sno_pat_any))\n");
    fprintf(out, "  (import \"sno\" \"sno_pat_notany\"  (func $sno_pat_notany))\n");
    fprintf(out, "  (import \"sno\" \"sno_pat_span\"    (func $sno_pat_span))\n");
    fprintf(out, "  (import \"sno\" \"sno_pat_break\"   (func $sno_pat_break))\n");
    fprintf(out, "  (import \"sno\" \"sno_pat_len\"     (func $sno_pat_len))\n");
    fprintf(out, "  (import \"sno\" \"sno_pat_pos\"     (func $sno_pat_pos))\n");
    fprintf(out, "  (import \"sno\" \"sno_pat_rpos\"    (func $sno_pat_rpos))\n");
    fprintf(out, "  (import \"sno\" \"sno_pat_tab\"     (func $sno_pat_tab))\n");
    fprintf(out, "  (import \"sno\" \"sno_pat_rtab\"    (func $sno_pat_rtab))\n");
    fprintf(out, "  (import \"sno\" \"sno_pat_rem\"     (func $sno_pat_rem))\n");
    fprintf(out, "  (import \"sno\" \"sno_pat_arb\"     (func $sno_pat_arb))\n");
    fprintf(out, "  (import \"sno\" \"sno_pat_arbno\"   (func $sno_pat_arbno))\n");
    fprintf(out, "  (import \"sno\" \"sno_pat_bal\"     (func $sno_pat_bal))\n");
    fprintf(out, "  (import \"sno\" \"sno_pat_fail\"    (func $sno_pat_fail))\n");
    fprintf(out, "  (import \"sno\" \"sno_pat_succeed\" (func $sno_pat_succeed))\n");
    fprintf(out, "  (import \"sno\" \"sno_pat_abort\"   (func $sno_pat_abort))\n");
    fprintf(out, "  (import \"sno\" \"sno_pat_fence\"   (func $sno_pat_fence))\n");
    fprintf(out, "  (import \"sno\" \"sno_pat_eps\"     (func $sno_pat_eps))\n");
    fprintf(out, "  (import \"sno\" \"sno_pat_cat\"     (func $sno_pat_cat))\n");
    fprintf(out, "  (import \"sno\" \"sno_pat_alt\"     (func $sno_pat_alt))\n");
    fprintf(out, "  (import \"sno\" \"sno_pat_deref\"   (func $sno_pat_deref))\n");
    fprintf(out, "  (import \"sno\" \"sno_pat_refname\" (func $sno_pat_refname (param i32 i32)))\n");
    fprintf(out, "  (import \"sno\" \"sno_pat_capture\" (func $sno_pat_capture (param i32 i32 i32)))\n");
    fprintf(out, "  (import \"sno\" \"sno_exec_stmt\"   (func $sno_exec_stmt   (param i32 i32 i32)))\n");
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
    userfns_reset();
    SM_Program * sm = sm_preamble(ast_prog);
    if (!sm) return 1;
    /* Pre-pass: intern all strings so data segments are emitted before $main references them. */
    for (int i = 0; i < sm->count; i++) {
        SM_Instr * ins = &sm->instrs[i];
        if (ins->op == SM_PUSH_LIT_S && ins->a[0].s) intern_str(ins->a[0].s);
        else if (ins->op == SM_PUSH_LIT_CS && ins->a[0].s) intern_str(ins->a[0].s);
        else if (ins->op == SM_PUSH_VAR  && ins->a[0].s) intern_name(ins->a[0].s);
        else if (ins->op == SM_STORE_VAR && ins->a[0].s) intern_name(ins->a[0].s);
        else if (ins->op == SM_CALL_FN   && ins->a[0].s) intern_name(ins->a[0].s);
        else if (ins->op == SM_SUSPEND_VALUE && ins->a[0].s) intern_name(ins->a[0].s);
        else if (ins->op == SM_PAT_LIT && ins->a[0].s) intern_str(ins->a[0].s);
        else if (ins->op == SM_PAT_REFNAME && ins->a[0].s) intern_name(ins->a[0].s);
        else if (ins->op == SM_PAT_CAPTURE && ins->a[0].s) intern_name(ins->a[0].s);
    }
    /* intern fixed keyword strings */
    intern_str("OUTPUT");
    intern_str("INPUT");
    /* Build user-fn table from SM_LABEL define_entry markers (also interns names + params). */
    pre_scan_userfns(sm);
    emit_wasm_prologue(out, sm);
    /* $main function */
    fprintf(out, "  (func $main (export \"main\")\n");
    fprintf(out, "    (local $pc i32)\n");
    fprintf(out, "    (local $tmp i32)\n");
    fprintf(out, "    (local $fr i32)\n");
    fprintf(out, "    (call $sno_init)\n");
    emit_wasm_from_sm(sm, out);
    fprintf(out, "    (call $sno_finalize)\n");
    fprintf(out, "  )\n");
    emit_wasm_epilogue(out);
    sm_prog_free(sm);
    return 0;
}
/* g_emit_vtable_wasm defined in emit_ir_targets.c */
