/*
 * emit_wasm_icon.c — Icon × WASM emitter (IW-session)
 *
 * Structural oracles (read before editing):
 *   ByrdBox/byrd_box.py  genc()       — flat-goto C template per node
 *   ByrdBox/test_icon-4.py            — return-function Python = direct WAT map
 *   jcon-master/tran/irgen.icn        — authoritative four-port wiring per AST node
 *   one4all/src/backend/emit_jvm_icon.c — JVM encoding reference (8k lines)
 *   one4all/src/backend/emit_x64_icon.c — x64 encoding reference
 *
 * Translation principle (BACKEND-WASM.md §Control Flow Model):
 *   x64/JVM encode each Byrd port as a flat label + jmp/goto.
 *   WASM has no flat goto — structured control only.
 *   Each Byrd port becomes a WAT function with return_call (zero-stack-growth tail call).
 *
 * IW-8: Rewritten to consume EXPR_t** (lowered IR from icon_lower.c).
 *   ICN_PROC  → E_FNC (proc decl: sval=name, ival=nparams)
 *   ICN_CALL  → E_FNC (call: sval=fname, children[0]=E_VAR name, [1..]=args)
 *   ICN_INT   → E_ILIT (ival)
 *   ICN_REAL  → E_FLIT (fval)
 *   ICN_STR   → E_QLIT (sval)
 *   ICN_VAR   → E_VAR  (sval)
 *   ICN_ADD   → E_ADD, ICN_SUB → E_SUB, ICN_MUL → E_MPY
 *   ICN_DIV   → E_DIV, ICN_MOD → E_MOD, ICN_NEG → E_NEG
 *   ICN_LT    → E_LT, LE→E_LE, GT→E_GT, GE→E_GE, EQ→E_EQ, NE→E_NE
 *   ICN_TO    → E_TO, ICN_ALT → E_GENALT
 *   ICN_EVERY → E_EVERY, ICN_RETURN → E_RETURN, ICN_FAIL → E_FAIL
 *   ICN_ASSIGN→ E_ASSIGN
 *
 * RULES.md §BYRD BOXES: emit labels+gotos, never interpret IR nodes at emit-time.
 */

#include "icon_ast.h"
#include "icon_emit.h"
#include "emit_wasm.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* ── §1  WAT output macros ────────────────────────────────────────────────── */

static FILE *icon_wasm_out = NULL;

void emit_wasm_icon_set_out(FILE *f) { icon_wasm_out = f; }

#define WI(fmt, ...)  fprintf(icon_wasm_out, fmt, ##__VA_ARGS__)

/* Generator state memory layout — same as before (IW-2) */
#define ICON_GEN_STATE_BASE  0x20000   /* page 2 (131072): safe above string heap+data */
#define ICON_GEN_SLOT_BYTES  64
#define ICON_GEN_MAX_SLOTS   256

/* Retcont call stack — handles recursive proc calls (IW-9).
 * Stack pointer at ICON_RETCONT_SP_ADDR (i32); stack grows upward.
 * Each frame = 4 bytes (one i32 retcont table index).
 * Separate from gen-state area (0x20000–0x23FFF).
 * Layout: [ICON_RETCONT_SP_ADDR] = current SP (points to next free slot)
 *          [ICON_RETCONT_STACK_BASE .. +4096] = frame storage */
#define ICON_RETCONT_SP_ADDR    0x24000   /* 147456: SP global in memory */
#define ICON_RETCONT_STACK_BASE 0x24004   /* 147460: stack data starts here */
#define ICON_RETCONT_MAX_DEPTH  1024      /* max recursion depth */

static int icon_gen_slot_next = 0;

/* M-IW-P01: funcref table for call-site esucc trampolines.
 * Each user-proc call site registers its esucc WAT func name here.
 * At module end we emit (table N funcref) + (elem ...).
 * $icn_retcont global holds the table index the callee should return_call_indirect to. */
#define ICN_RETCONT_MAX 256
static char icn_retcont_funcs[ICN_RETCONT_MAX][64];
static int  icn_retcont_count = 0;

static void icn_retcont_reset(void) { icn_retcont_count = 0; }

/* Register an esucc func name; return its table index. */
static int icn_retcont_register(const char *fname) {
    for (int i = 0; i < icn_retcont_count; i++)
        if (strcmp(icn_retcont_funcs[i], fname) == 0) return i;
    if (icn_retcont_count >= ICN_RETCONT_MAX) return 0;
    int idx = icn_retcont_count++;
    snprintf(icn_retcont_funcs[idx], 64, "%s", fname);
    return idx;
}

/* ── §1b  String literal intern table — shared via emit_wasm.h ───────────── */
/* emit_wasm.c owns the table; call emit_wasm_strlit_* directly.             */

/* Pre-scan EXPR_t tree and intern every E_QLIT string. */
static void icn_prescan_node(const EXPR_t *n) {
    if (!n) return;
    if (n->kind == E_QLIT && n->sval)
        emit_wasm_strlit_intern(n->sval);
    for (int i = 0; i < n->nchildren; i++)
        icn_prescan_node(n->children[i]);
}

            /* Number of live icn_intN slots to save = all slots used so far.
             * wasm_icon_ctr is the NEXT id to allocate; all ids < id are live. */
            int nints_to_save = id;  /* save icn_int0..id-1 */
            if (nints_to_save > ICON_FRAME_MAX_INTS) nints_to_save = ICON_FRAME_MAX_INTS;
            int callee_nparams = icn_proc_reg_lookup(fname);  /* IW-10: for frame save/restore */

            if (nargs > 0) {
                /* Two-pass: emit all arg expressions first to get their start names,
                 * then emit esucc trampolines that forward to the next arg's start. */
                int actual_nargs = (nargs < 8) ? nargs : 8;
                char arg_starts[8][64];
                char arg_esucc_names[8][64];
                int  arg_ids[8];
                char dummy_resume[64];

                for (int ai = 0; ai < actual_nargs; ai++) {
                    EXPR_t *arg = n->children[1 + ai];
                    snprintf(arg_esucc_names[ai], 64, "icon%d_arg%d_esucc", id, ai);
                    emit_expr_wasm(arg, arg_esucc_names[ai], fail,
                                   arg_starts[ai], dummy_resume);
                    arg_ids[ai] = wasm_icon_ctr - 1;
                }

                /* sa -> first arg's start */
                WI("  (func $%s (result i32)  return_call $%s)\n", sa, arg_starts[0]);

                /* esucc[ai]: store param, call next arg start or docall */
                for (int ai = 0; ai < actual_nargs; ai++) {
                    char next_buf[64];
                    if (ai < actual_nargs - 1)
                        snprintf(next_buf, sizeof next_buf, "%s", arg_starts[ai + 1]);
                    else
                        snprintf(next_buf, sizeof next_buf, "icon%d_docall", id);
                    WI("  (func $%s (result i32)\n", arg_esucc_names[ai]);
                    WI("    global.get $icn_int%d\n", arg_ids[ai]);
                    WI("    global.set $icn_param%d\n", ai);
                    WI("    return_call $%s)\n", next_buf);
                }
                /* Register esucc in retcont table; docall sets $icn_retcont then calls proc */
                int retcont_idx = icn_retcont_register(esucc);
                WI("  (func $icon%d_docall (result i32)\n", id);
                emit_frame_push(nints_to_save, callee_nparams);  /* IW-10: save live ints + callee params */
                WI("    i32.const %d\n", retcont_idx);
                WI("    call $icn_retcont_push\n");  /* IW-9: stack push for recursion */
                WI("    return_call $icn_proc_%s_start)\n", fname);
            }

            WI("  (func $%s (result i32)\n", esucc);
            emit_frame_pop(nints_to_save, callee_nparams);   /* IW-10: restore live ints + callee params */
            WI("    global.get $icn_retval\n");
            WI("    global.set $icn_int%d\n", id);
            WI("    return_call $%s)\n", succ);
            WI("  (func $%s (result i32)  return_call $%s)\n", ra, fail);
            break;
        }
        emit_icn_stub(n, id, fail);
        break;
    }

    /* ── return [E] ──────────────────────────────────────────────────────── */
    case E_RETURN:
        WI("  ;; E_RETURN (node %d)\n", id);
        if (strcmp(icn_cur_proc_name, "main") != 0 && icn_cur_proc_name[0] != '\0') {
            if (n->nchildren >= 1) {
                char e_start[64], e_resume[64];
                char esucc[64];
                wfn(esucc, sizeof esucc, id, "esucc");
                int e_id = wasm_icon_ctr;
                emit_expr_wasm(n->children[0], esucc, fail, e_start, e_resume);
                WI("  (func $%s (result i32)\n", esucc);
                WI("    global.get $icn_int%d\n", e_id);
                WI("    global.set $icn_retval\n");
                WI("    return_call $%s)\n", succ);
                WI("  (func $%s (result i32)  return_call $%s)\n", sa, e_start);
            } else {
                WI("  (func $%s (result i32)\n", sa);
                WI("    i64.const 0\n");
                WI("    global.set $icn_retval\n");
                WI("    return_call $%s)\n", succ);
            }
        } else {
            WI("  (func $%s (result i32)  return_call $%s)\n", sa, succ);
        }
        WI("  (func $%s (result i32)  return_call $%s)\n", ra, fail);
        break;

    /* ── fail ────────────────────────────────────────────────────────────── */
    case E_FAIL:
        WI("  ;; E_FAIL (node %d)\n", id);
        WI("  (func $%s (result i32)  return_call $%s)\n", sa, fail);
        WI("  (func $%s (result i32)  return_call $%s)\n", ra, fail);
        break;

    /* ── if/then/else (E_IF) ─────────────────────────────────────────────── */
    case E_IF: {
        /* children[0]=cond, children[1]=then_body, children[2]=else_body (opt)
         * Wiring (four-port):
         *   sa  → cond.start
         *   cond succeeds → then.start (or succ if no then)
         *   cond fails    → else.start (or fail if no else)
         *   then succeeds → succ;  then fails → fail
         *   else succeeds → succ;  else fails → fail
         *   ra  → fail  (if/then is not a generator) */
        if (n->nchildren < 1) { emit_icn_stub(n, id, fail); break; }
        EXPR_t *cond  = n->children[0];
        EXPR_t *thenb = (n->nchildren > 1) ? n->children[1] : NULL;
        EXPR_t *elseb = (n->nchildren > 2) ? n->children[2] : NULL;

        char cond_start[64], cond_resume[64];
        char then_start[64], then_resume[64];
        char else_start[64], else_resume[64];

        /* Emit condition; its succ=then_entry, fail=else_entry */
        char then_entry[64], else_entry[64];
        wfn(then_entry, sizeof then_entry, id, "then_entry");
        wfn(else_entry, sizeof else_entry, id, "else_entry");

        emit_expr_wasm(cond, then_entry, else_entry, cond_start, cond_resume);

        if (thenb)
            emit_expr_wasm(thenb, succ, fail, then_start, then_resume);
        if (elseb)
            emit_expr_wasm(elseb, succ, fail, else_start, else_resume);

        WI("  ;; E_IF  (node %d)\n", id);
        WI("  (func $%s (result i32)  return_call $%s)\n", sa, cond_start);
        WI("  (func $%s (result i32)  return_call $%s)\n", ra, fail);
        /* then_entry: cond succeeded — enter then branch or go to succ */
        WI("  (func $%s (result i32)  return_call $%s)\n",
           then_entry, thenb ? then_start : succ);
        /* else_entry: cond failed — enter else branch, or skip (go to succ) if no else.
         * Icon semantics: "if E then S" with no else — cond failure just skips S. */
        WI("  (func $%s (result i32)  return_call $%s)\n",
           else_entry, elseb ? else_start : succ);
        break;
    }

    /* ── Global declaration (skip — no code to emit) ─────────────────────── */
    case E_GLOBAL:
        WI("  ;; E_GLOBAL \"%s\" (node %d) — decl only\n", n->sval ? n->sval : "", id);
        WI("  (func $%s (result i32)  return_call $%s)\n", sa, succ);
        WI("  (func $%s (result i32)  return_call $%s)\n", ra, fail);
        break;

    /* ── Unimplemented: stub-fail ─────────────────────────────────────────── */
    default:
        emit_icn_stub(n, id, fail);
        break;
    }

    if (out_start)  snprintf(out_start,  64, "%s", sa);
    if (out_resume) snprintf(out_resume, 64, "%s", ra);
}

/* ── §5  Public entry points ──────────────────────────────────────────────── */

int emit_wasm_icon_node(const EXPR_t *n, FILE *out) {
    /* Legacy hook — kept for compatibility. Not used in primary file path. */
    (void)n; (void)out; return 0;
}

void emit_wasm_icon_globals(FILE *out) {
    emit_wasm_icon_set_out(out);
    WI("  ;; Icon node-value globals\n");
    for (int i = 0; i < 64; i++)
        WI("  (global $icn_int%d (mut i64) (i64.const 0))\n", i);
    for (int i = 0; i < 16; i++)
        WI("  (global $icn_flt%d (mut f64) (f64.const 0))\n", i);
    WI("  ;; M-IW-P01: proc call/return globals\n");
    WI("  (global $icn_retval (mut i64) (i64.const 0))\n");
    WI("  ;; IW-9: retcont stack (handles recursion)\n");
    WI("  ;; SP stored at mem[0x%x]; stack data at mem[0x%x]\n",
       ICON_RETCONT_SP_ADDR, ICON_RETCONT_STACK_BASE);
    /* Inline SP init — runtime initialises to stack base on first call via data segment */
    WI("  (func $icn_retcont_push (param $idx i32)\n");
    WI("    (local $sp i32)\n");
    /* load SP; if zero (uninitialised), set to stack base */
    WI("    i32.const %d\n", ICON_RETCONT_SP_ADDR);
    WI("    i32.load\n");
    WI("    local.set $sp\n");
    WI("    (if (i32.eqz (local.get $sp)) (then\n");
    WI("      i32.const %d\n", ICON_RETCONT_SP_ADDR);
    WI("      i32.const %d\n", ICON_RETCONT_STACK_BASE);
    WI("      i32.store\n");
    WI("      i32.const %d\n", ICON_RETCONT_STACK_BASE);
    WI("      local.set $sp))\n");
    /* mem[sp] = idx */
    WI("    local.get $sp\n");
    WI("    local.get $idx\n");
    WI("    i32.store\n");
    /* SP += 4 */
    WI("    i32.const %d\n", ICON_RETCONT_SP_ADDR);
    WI("    local.get $sp\n");
    WI("    i32.const 4\n");
    WI("    i32.add\n");
    WI("    i32.store)\n");
    WI("  (func $icn_retcont_pop (result i32)\n");
    WI("    (local $sp i32)\n");
    /* sp = current_sp - 4 */
    WI("    i32.const %d\n", ICON_RETCONT_SP_ADDR);
    WI("    i32.load\n");
    WI("    i32.const 4\n");
    WI("    i32.sub\n");
    WI("    local.set $sp\n");
    /* store new SP */
    WI("    i32.const %d\n", ICON_RETCONT_SP_ADDR);
    WI("    local.get $sp\n");
    WI("    i32.store\n");
    /* return mem[sp] */
    WI("    local.get $sp\n");
    WI("    i32.load)\n");
    WI("  (global $icn_retcont (mut i32) (i32.const 0))\n");
    for (int i = 0; i < 8; i++)
        WI("  (global $icn_param%d (mut i64) (i64.const 0))\n", i);
}

void emit_wasm_icon_str_globals(FILE *out) {
    emit_wasm_icon_set_out(out);
    int nlit = emit_wasm_strlit_count();
    if (nlit == 0) return;
    WI("  ;; Icon string literal (offset,len) globals\n");
    for (int i = 0; i < nlit; i++) {
        WI("  (global $icn_strlit_off%d (mut i32) (i32.const 0))\n", i);
        WI("  (global $icn_strlit_len%d (mut i32) (i32.const 0))\n", i);
    }
}

int is_icon_node(int kind) {
    return (kind >= E_ILIT && kind < E_KIND_COUNT);
}

/* ── Emit one E_FNC proc node as a WAT function group ────────────────────── */
/*
 * EXPR_t proc layout (from icon_lower.c ICN_PROC case):
 *   e->kind          = E_FNC
 *   e->sval          = proc name
 *   e->ival          = nparams
 *   e->children[0]   = E_VAR name node (sval = proc name)
 *   e->children[1..np] = E_VAR param nodes
 *   e->children[np+1..] = body statements
 */
static void emit_wasm_icon_proc(const EXPR_t *proc) {
    if (!proc || proc->kind != E_FNC) return;
    /* A proc decl has nchildren >= 1 and children[0] is E_VAR with proc name.
     * Distinguish proc-decl from call-site E_FNC: proc sval matches children[0]->sval. */
    const char *pname = proc->sval;
    if (!pname || !pname[0]) return;

    int nparams = (int)proc->ival;
    int body_start = 1 + nparams;  /* children[0]=name, [1..np]=params, [np+1..]=body */
    int nstmts = proc->nchildren - body_start;
    if (nstmts < 0) nstmts = 0;

    WI("\n  ;; ── Procedure %s (%d params, %d stmts) ──\n", pname, nparams, nstmts);

    icon_gen_slot_next = 0;

    snprintf(icn_cur_proc_name, sizeof icn_cur_proc_name, "%s", pname);
    icn_cur_nparams = (nparams < 8) ? nparams : 8;
    for (int i = 0; i < icn_cur_nparams; i++) {
        EXPR_t *pnode = proc->children[1 + i];
        const char *pn = (pnode && pnode->sval) ? pnode->sval : "";
        snprintf(icn_cur_params[i], 64, "%s", pn);
    }

    /* M-IW-V01: scan proc body for local vars (E_ASSIGN LHS that aren't params) */
    icn_locals_reset();
    for (int i = 0; i < nstmts; i++)
        icn_locals_scan(proc->children[body_start + i], pname);

    /* Emit (global $icn_lv_PROC_VAR ...) for each discovered local */
    icn_emit_local_globals(pname);

    if (nstmts == 0) {
        WI("  (func $icn_proc_%s_start (result i32)  return_call $icn_prog_end)\n", pname);
        return;
    }

    #define MAX_STMTS_PER_PROC 64
    char stmt_start [MAX_STMTS_PER_PROC][64];
    char stmt_resume[MAX_STMTS_PER_PROC][64];

    if (nstmts > MAX_STMTS_PER_PROC) {
        WI("  ;; WARNING: too many stmts in %s (%d > %d)\n", pname, nstmts, MAX_STMTS_PER_PROC);
        nstmts = MAX_STMTS_PER_PROC;
    }

    char chain_names[MAX_STMTS_PER_PROC][64];
    for (int i = 0; i < nstmts; i++)
        snprintf(chain_names[i], 64, "icn_%s_chain%d", pname, i);

    for (int i = 0; i < nstmts; i++) {
        const EXPR_t *stmt = proc->children[body_start + i];
        emit_expr_wasm(stmt, chain_names[i], "icn_program_fail",
                       stmt_start[i], stmt_resume[i]);
    }

    /* Non-main procs return via $icn_retcont trampoline (M-IW-P01).
     * main returns via icn_prog_end as before. */
    int is_main_proc = (strcmp(pname, "main") == 0);
    const char *last_succ = is_main_proc ? "icn_prog_end"
                                         : (char[64]){};
    char retcont_func[64];
    if (!is_main_proc)
        snprintf(retcont_func, sizeof retcont_func, "icn_proc_%s_retcont", pname);

    for (int i = 0; i < nstmts; i++) {
        const char *next;
        char next_buf[64];
        if (i + 1 < nstmts) {
            next = stmt_start[i+1];
        } else {
            next = is_main_proc ? "icn_prog_end" : retcont_func;
        }
        (void)last_succ;
        WI("  (func $%s (result i32)  return_call $%s)  ;; chain %d->%d\n",
           chain_names[i], next, i, i+1);
        (void)next_buf;
    }

    /* Emit retcont trampoline for non-main procs */
    if (!is_main_proc) {
        WI("  (func $%s (result i32)\n", retcont_func);
        WI("    call $icn_retcont_pop\n");  /* IW-9: pop stack for recursion */
        WI("    return_call_indirect (type $cont_t))\n");
    }

    WI("  (func $icn_proc_%s_start (result i32)  return_call $%s)\n",
       pname, stmt_start[0]);
}

/*
 * emit_wasm_icon_file() — top-level entry point for Icon × WASM compilation.
 * Receives EXPR_t** lowered procs from icon_lower_file().
 * IW-8: updated from IcnNode** to EXPR_t**.
 */
void emit_wasm_icon_file(EXPR_t **procs, int count, FILE *out,
                          const char *filename) {
    (void)filename;
    emit_wasm_icon_set_out(out);

    /* Prescan: intern all E_QLIT strings so globals declared before funcs */
    emit_wasm_strlit_reset();
    icn_retcont_reset();
    icn_proc_reg_reset();
    for (int i = 0; i < count; i++) {
        icn_prescan_node(procs[i]);
        /* Register proc name → nparams for call-site frame save/restore (IW-10) */
        if (procs[i] && procs[i]->kind == E_FNC && procs[i]->sval &&
            procs[i]->nchildren >= 1 &&
            procs[i]->children[0] && procs[i]->children[0]->sval &&
            strcmp(procs[i]->sval, procs[i]->children[0]->sval) == 0)
            icn_proc_reg_add(procs[i]->sval, (int)procs[i]->ival);
    }

    WI(";; Generated by scrip-cc -icn -wasm (IW-8)\n");
    WI("(module\n");

    WI("  ;; M-IW-P01: continuation type for return_call_indirect\n");
    WI("  (type $cont_t (func (result i32)))\n");
    WI("  ;; Memory imported from runtime module\n");
    WI("  (import \"sno\" \"memory\" (memory 3))  ;; page0=output/heap page1=str literals page2=gen state\n");
    WI("  ;; Memory + base runtime imports shared with SNOBOL4 (emit_wasm.h)\n");
    emit_wasm_runtime_imports_sno_base(icon_wasm_out, 3,
        "page0=output/heap page1=str literals page2=gen state");
    /* Icon-specific: no additional sno-namespace imports beyond base set */

    emit_wasm_icon_globals(out);
    emit_wasm_icon_str_globals(out);
    emit_wasm_data_segment();

    /* Emit all proc declarations (E_FNC with sval matching children[0]->sval) */
    for (int i = 0; i < count; i++) {
        if (procs[i] && procs[i]->kind == E_FNC &&
            procs[i]->nchildren >= 1 &&
            procs[i]->children[0] && procs[i]->children[0]->kind == E_VAR &&
            procs[i]->sval && procs[i]->children[0]->sval &&
            strcmp(procs[i]->sval, procs[i]->children[0]->sval) == 0) {
            emit_wasm_icon_proc(procs[i]);
        }
    }

    WI("\n  ;; ── Terminal functions ──\n");
    WI("  (func $icn_prog_end (result i32)\n");
    WI("    call $sno_output_flush)\n");
    WI("  (func $icn_program_fail (result i32)\n");
    WI("    call $sno_output_flush)\n");

    /* Exported main: find the main proc */
    WI("\n  ;; ── Exported main entry ──\n");
    WI("  (func (export \"main\") (result i32)\n");
    int found_main = 0;
    for (int i = 0; i < count; i++) {
        if (procs[i] && procs[i]->kind == E_FNC &&
            procs[i]->sval && strcmp(procs[i]->sval, "main") == 0 &&
            procs[i]->nchildren >= 1 &&
            procs[i]->children[0] && procs[i]->children[0]->sval &&
            strcmp(procs[i]->children[0]->sval, "main") == 0) {
            WI("    return_call $icn_proc_main_start)\n");
            found_main = 1;
            break;
        }
    }
    if (!found_main) {
        WI("    ;; no main procedure found\n");
        WI("    call $sno_output_flush)\n");
    }

    /* M-IW-P01: emit funcref table for call-site esucc return trampolines */
    if (icn_retcont_count > 0) {
        WI("\n  ;; M-IW-P01: retcont funcref table (%d entries)\n", icn_retcont_count);
        WI("  (table %d funcref)\n", icn_retcont_count);
        WI("  (elem (i32.const 0)");
        for (int i = 0; i < icn_retcont_count; i++)
            WI(" $%s", icn_retcont_funcs[i]);
        WI(")\n");
    }

    WI(")\n");
}
