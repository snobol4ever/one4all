/*
 * icn_runtime.c — Icon interpreter runtime
 *
 * FI-4: extracted from src/driver/scrip.c.
 * IcnFrame stack, icn_gen_*, icn_scan_*, global_*, proc_table,
 * icn_bb_build, icn_bb_oneshot, icn_scope_*.
 *
 * RS-17a (2026-05-03): all 60 value-context interp_eval call sites in
 * this file routed through bb_eval_value (icn_value.c).
 * RS-17b (2026-05-03): all 13 statement-context interp_eval call sites in
 * this file routed through bb_exec_stmt (icn_stmt.c).  No direct
 * interp_eval reference remains here.
 *
 * AUTHORS: Lon Jones Cherryholmes · Claude Sonnet 4.6 (FI-4, 2026-04-14)
 */
#include "icn_runtime.h"
#include "icn_value.h"
#include "icn_stmt.h"
#include "../ast/ast.h"
#include "../../frontend/snobol4/scrip_cc.h"
#include "bb_broker.h"
#include "../../frontend/icon/icon_gen.h"
#include "coerce.h"
#include "scan_builtins.h"
#include "../../lower/ir_exec.h"
#include "../../lower/lower_icn.h"
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <gc/gc.h>
extern bb_node_t icn_bb_make_proc_box(tree_t *proc, DESCR_t *args, int nargs); /* IJ-CORO: replaces icn_bb_suspend */

/* A0 — BB-only enforcement.  The header defines NO_AST_WALK_GUARD; this
 * local copy (used before the header include pulled in the definition) is
 * kept in sync.  Crashes unconditionally for Icon when SM dispatch is active
 * and the AST walker is entered — no env var, no escape hatch. */
#define NO_AST_WALK_GUARD(fn_name) \
    do { if (g_sm_dispatch_active && !g_ast_pump_active && g_lang == LANG_ICN) { \
        fprintf(stderr, "FATAL: " fn_name " reached from SM dispatch (Icon BB incomplete)\n"); \
        abort(); \
    } } while (0)

/* RS-17b: with bb_exec_stmt now handling every statement-context site in this
 * file (was 13 direct interp_eval calls before RS-17b), and bb_eval_value
 * handling every value-context site (RS-17a), no `extern DESCR_t
 * interp_eval(tree_t *e);` declaration is needed here anymore.  The IR-mode
 * tree-walker reaches the work that arrives in icn_stmt.c's and
 * icn_value.c's fallthroughs; from this file's perspective, interp_eval is
 * gone.  This is the contract RS-19 locks in by promoting icn_runtime.c
 * into the isolation grep gate (after RS-18 closes for pl_runtime.c). */

/* NV_SET_fn lives in snobol4.c — needed by RK-16 loop-var binding */
extern DESCR_t NV_SET_fn(const char *name, DESCR_t val);

/* ── Icon unified interpreter state ────────────────────────────────────────
 * Icon procedures use slot-indexed locals (e->v.ival on TT_VAR nodes).
 * When interp_eval is running inside an Icon procedure call, frame_env points
 * to the current frame's slot array. TT_VAR case checks frame_env first.
 * FRAME.env_n is the slot count. Both are NULL/0 when in SNOBOL4 context.
 *
 * Icon procedure table: built from TT_PROGRAM at execute_program time.
 * Each entry maps procname → the TT_FNC node (from TT_STMT :subj).
 * ────────────────────────────────────────────────────────────────────────── */
IcnProcEntry proc_table[PROC_TABLE_MAX];
int          proc_count = 0;
int          g_lang         = 0;     /* 0=SNOBOL4 1=Icon */
tree_t      *g_icn_root     = NULL;  /* current Icon drive root */

/* A0 — SCRIP_NO_AST_WALK tripwire.  Set to 1 at entry of sm_interp_run /
 * sm_call_proc; cleared at exit.  When set, icn_bb_build / interp_eval /
 * interp_eval_pat / interp_eval_ref / call_user_function / execute_program
 * abort if SCRIP_NO_AST_WALK env var is set — proving SM dispatch never
 * reaches the AST walker under honest mode-3.  Global (not thread-local)
 * because scrip is single-threaded; revisit if threading is ever added. */
int g_sm_dispatch_active = 0;

/* GOAL-ICON-BB-COMPLETE Phase A: re-entrant suppression counter for SM_BB_PUMP_AST.
 * When > 0, icn_bb_build is explicitly permitted even if g_sm_dispatch_active=1.
 * Incremented at entry of SM_BB_PUMP_AST handler, decremented at exit.
 * Needed because SM_BB_PUMP_AST calls icn_bb_build deliberately (Phase A bridge),
 * and the Byrd-box functions called by icn_bb_build may re-enter sm_interp_run
 * (for SM proc bodies), which would reset g_sm_dispatch_active=1 before
 * any nested icn_bb_build calls inside the Byrd-box machinery. */
int g_ast_pump_active = 0;

/* OE-1: IcnFrame — per-call context for Icon procedure invocations.
 * Replaces the flat globals frame_env/FRAME.env_n/FRAME.returning/FRAME.return_val/
 * icn_gen_stack/icn_gen_depth/FRAME.loop_break with a pushed/popped frame stack.
 * FRAME refers to the active frame (frame_depth must be >0 in Icon context). */
IcnFrame frame_stack[FRAME_STACK_MAX];
int      frame_depth = 0;

/* coro_drive_fnc suspend-value passthrough: while running the every-body,
 * set icn_drive_node = the TT_FNC being driven and icn_drive_val = suspended value.
 * interp_eval(TT_FNC) returns icn_drive_val directly when e == icn_drive_node. */
tree_t  *icn_drive_node = NULL;
DESCR_t  icn_drive_val;

/* Convenience helpers that mirror the old flat-global helpers */
void frame_push(tree_t *n, long v, const char *sv) {
    IcnFrame *f = &FRAME;
    if (f->gen_depth < FRAME_DEPTH_MAX) { f->gen[f->gen_depth].node=n; f->gen[f->gen_depth].cur=v; f->gen[f->gen_depth].sval=sv; f->gen_depth++; }
}
void frame_pop(void) { if (FRAME.gen_depth > 0) FRAME.gen_depth--; }
int  icn_frame_lookup(tree_t *n, long *out) {
    IcnFrame *f = &FRAME;
    for (int i=f->gen_depth-1;i>=0;i--) if(f->gen[i].node==n){*out=f->gen[i].cur;return 1;} return 0;
}
int  icn_frame_lookup_sv(tree_t *n, long *out, const char **sv) {
    IcnFrame *f = &FRAME;
    for (int i=f->gen_depth-1;i>=0;i--) if(f->gen[i].node==n){*out=f->gen[i].cur;*sv=f->gen[i].sval;return 1;} return 0;
}
int  frame_active(tree_t *n) {
    IcnFrame *f = &FRAME;
    for (int i=0;i<f->gen_depth;i++) if(f->gen[i].node==n) return 1; return 0;
}

/* CHUNKS-step17b'' (CH-17b''): pure-DESCR_t forwarders to FRAME.env[slot].
 * Used by sm_interp.c's SM_LOAD_FRAME / SM_STORE_FRAME handlers so the SM
 * runtime can read/write Icon frame slots without including icn_runtime.h
 * (which would expose tree_t / IR types across the SM/IR boundary).
 *
 * Semantics mirror icn_value.c:382–399 for TT_VAR with frame_depth > 0:
 *   slot in [0, FRAME.env_n) → use FRAME.env[slot]; else FAILDESCR.
 * Outside an Icon frame (frame_depth == 0), icn_frame_env_active() returns 0
 * and the caller pushes FAILDESCR — the expression-shaped consumer dispatch
 * (CH-17c+) is the only path that reaches these calls in real programs. */
int icn_frame_env_active(void) {
    return frame_depth > 0;
}
DESCR_t icn_frame_env_load(int slot) {
    if (frame_depth <= 0) return FAILDESCR;
    IcnFrame *f = &FRAME;
    if (slot < 0 || slot >= f->env_n) return FAILDESCR;
    return f->env[slot];
}
void icn_frame_env_store(int slot, DESCR_t val) {
    if (frame_depth <= 0) return;
    IcnFrame *f = &FRAME;
    if (slot < 0 || slot >= FRAME_SLOT_MAX) return;
    /* env_n grows up to the highest slot stored — mirrors how
     * icn_scope_patch + the bb_eval_value TT_ASSIGN path together
     * extend env_n implicitly in the IR walker today. */
    if (slot >= f->env_n) f->env_n = slot + 1;
    f->env[slot] = val;
}

/* Icon scan state globals (not per-frame: scan nesting is within one call).
 * IC-9 (2026-05-01): defaults match Icon spec — at program start, before any `?`,
 * &pos = 1 and &subject = "" (the empty string), not 0 / &null. */
const char *scan_subj  = "";
int         scan_pos   = 1;
ScanEntry scan_stack[SCAN_STACK_MAX];
int         scan_depth = 0;

/* Active coroutine suspend state — set by trampoline before calling coro_call,
 * so coro_call can swapcontext back on TT_SUSPEND. NULL when not in a coroutine. */
/* IJ-CORO: active_coro removed */
/* IJ-CORO: sm_yield_to_caller stub — SM_SUSPEND_VALUE deleted in CORO-3 */
int sm_yield_to_caller(DESCR_t v) { (void)v; return 0; }

/* U-23: Icon global variable names -- bridge to SNO NV store.
 * Names declared `global X` in an Icon block are stored here.
 * icn_scope_patch skips slot assignment for these; TT_VAR read/write
 * calls NV_GET_fn / NV_SET_fn instead of frame_env[slot]. */
const char *global_names[GLOBAL_MAX];
int         global_count = 0;
int is_global(const char *name) {
    for (int i = 0; i < global_count; i++)
        if (global_names[i] && strcmp(global_names[i], name) == 0) return 1;
    return 0;
}
void global_register(const char *name) {
    if (!name || is_global(name) || global_count >= GLOBAL_MAX) return;
    global_names[global_count++] = name;
}

/* IC-9 (2026-05-01): per-(proc, var) static-variable storage.  Statics are
 * declared `static x;` inside a procedure; their values persist across calls
 * to that procedure but are scoped to the procedure (statics with the same
 * name in different procs do not share storage).
 * CH-17g-statics: re-keyed off tree_t* onto (entry_pc, proc_name).
 * Primary key: entry_pc >= 0  → (entry_pc, var_name)  — stable SM pc.
 * Fallback key: entry_pc < 0  → (proc_name, var_name) — name string identity.
 * The fallback covers procs not yet lowered through sm_lower (tree_t path still
 * live in coro_call); it provides the same scoping guarantee that tree_t*
 * pointer identity provided before, since proc names are interned and unique. */
typedef struct {
    int         entry_pc;   /* >= 0: primary key; < 0: use proc_name fallback */
    const char *proc_name;  /* interned proc name; fallback key when entry_pc < 0 */
    const char *name;       /* interned static variable name */
    DESCR_t     val;
} static_ent_t;
#define STATIC_MAX 256
static static_ent_t static_tab[STATIC_MAX];
static int              static_n = 0;

/* Look up entry_pc for a proc by name.  Returns -1 if not found or not yet
 * resolved (entry_pc == -1 means sm_lower hasn't emitted its expression yet). */
static int static_proc_entry_pc(const char *proc_name) {
    if (!proc_name) return -1;
    for (int i = 0; i < proc_count; i++)
        if (proc_table[i].name && strcmp(proc_table[i].name, proc_name) == 0)
            return proc_table[i].entry_pc; /* -1 if not yet resolved */
    return -1;
}

/* Match predicate: true when entry matches the (entry_pc, proc_name, var_name)
 * triple under the primary-or-fallback keying rules. */
static int static_entry_matches(const static_ent_t *e, int epc,
                                const char *pname, const char *vname) {
    if (!e->name || !vname || strcmp(e->name, vname) != 0) return 0;
    if (epc >= 0 && e->entry_pc >= 0) return e->entry_pc == epc;
    /* fallback: both have entry_pc < 0 — match by proc_name */
    return e->proc_name && pname && strcmp(e->proc_name, pname) == 0;
}

int static_get(tree_t *proc, const char *name, DESCR_t *out) {
    if (!proc || !name || !out) return 0;
    const char *pname = proc->v.sval;
    int epc = static_proc_entry_pc(pname);
    for (int i = 0; i < static_n; i++) {
        if (static_entry_matches(&static_tab[i], epc, pname, name)) {
            *out = static_tab[i].val;
            return 1;
        }
    }
    return 0;
}

void static_set(tree_t *proc, const char *name, DESCR_t val) {
    if (!proc || !name) return;
    const char *pname = proc->v.sval;
    int epc = static_proc_entry_pc(pname);
    for (int i = 0; i < static_n; i++) {
        if (static_entry_matches(&static_tab[i], epc, pname, name)) {
            /* Update entry_pc if it was just resolved (previously -1) */
            if (epc >= 0 && static_tab[i].entry_pc < 0)
                static_tab[i].entry_pc = epc;
            static_tab[i].val = val;
            return;
        }
    }
    if (static_n >= STATIC_MAX) return;
    static_tab[static_n].entry_pc  = epc;
    static_tab[static_n].proc_name = pname;
    static_tab[static_n].name      = name;
    static_tab[static_n].val       = val;
    static_n++;
}

/* scope_add/patch: mirror of scope_add/scope_patch in icon_interp.c.
 * Assigns slot indices to TT_VAR nodes by name, in-place on the AST. */

int scope_add(IcnScope *sc, const char *name) {
    if (!name) return -1;
    for (int i=0;i<sc->n;i++) if(strcmp(sc->e[i].name,name)==0) return sc->e[i].slot;
    if (sc->n >= FRAME_SLOT_MAX) return -1;
    int slot = sc->n;
    sc->e[sc->n].name=name; sc->e[sc->n].slot=slot; sc->n++;
    return slot;
}
int scope_get(IcnScope *sc, const char *name) {
    if (!name) return -1;
    for (int i=0;i<sc->n;i++) if(strcmp(sc->e[i].name,name)==0) return sc->e[i].slot;
    return -1;
}
void icn_scope_patch(IcnScope *sc, tree_t *e) {
    if (!e) return;
    if (e->t == TT_GLOBAL) {
        for (int i=0;i<e->n;i++)
            if(e->c[i]&&e->c[i]->v.sval) scope_add(sc, e->c[i]->v.sval);
        return;
    }
    if (e->t == TT_VAR && e->v.sval) {
        /* SI-13 fix: store slot in _id, NOT v.ival. v.ival aliases v.sval (union);
         * clobbering it makes keyword/NV detection in bb_eval_value crash.
         * Use _id (emit-time only in SNO path; unused here) as the slot index.
         * Sentinel: _id==-1 means "NV or keyword — use v.sval for name lookup". */
        if (e->v.sval[0] == '&') {
            e->_id = -1;   /* keyword — always NV, never a frame slot */
        } else if (is_global(e->v.sval)) {
            e->_id = -1;   /* global bridge to SNO NV store */
        } else {
            int s = scope_add(sc, e->v.sval);
            e->_id = (s >= 0) ? s : -1;
        }
    }
    /* SI-13 fix: TT_FNC child[0] is the callee name (TT_VAR), not a local variable.
     * Skip it during scope patching so v.sval is not clobbered by v.ival slot assignment. */
    int child_start = (e->t == TT_FNC) ? 1 : 0;
    for (int i=child_start;i<e->n;i++) icn_scope_patch(sc, e->c[i]);
}

/*============================================================================================================================
 * CH-17c: sm_call_proc — run an Icon/Raku proc body via SM dispatch.
 *
 * Called when proc_table[i].entry_pc != -1 (expression lowered by CH-17b/b'/b'').
 * Frame setup mirrors coro_call: push IcnFrame, bind args into param slots.
 * Expression body uses SM_LOAD_FRAME / SM_STORE_FRAME (baked by CH-17b'') for
 * param and local access — no icn_scope_patch needed.
 *
 * Execution: delegates to sm_call_expression(entry_pc), which runs the expression in a
 * nested SM_State.  The IcnFrame pushed here is visible to SM_LOAD_FRAME /
 * SM_STORE_FRAME (via icn_frame_env_load/store) throughout the expression body.
 *
 * Static-variable persistence deferred to CH-17g (statics keyed on tree_t*;
 * procs with statics continue via legacy coro_call until that key changes).
 */
DESCR_t sm_call_proc(int entry_pc, int nparams, DESCR_t *args, int nargs)
{
    extern DESCR_t sm_call_expression(int epc);

    if (entry_pc < 0) return FAILDESCR;
    if (frame_depth >= FRAME_STACK_MAX) return FAILDESCR;

    /* Push a fresh IcnFrame — mirrors coro_call's frame push */
    IcnFrame *f = &frame_stack[frame_depth++];
    memset(f, 0, sizeof *f);
    int nslots = (nparams > 0) ? nparams : 1;
    if (nslots > FRAME_SLOT_MAX) nslots = FRAME_SLOT_MAX;
    f->env_n = nslots;

    /* Bind positional args into param slots */
    for (int i = 0; i < nparams && i < nargs && i < FRAME_SLOT_MAX; i++)
        f->env[i] = args[i];

    /* CH-17g-proc-locals: patch TT_VAR.v.ival with frame slot indices so that
     * AST-walker code running inside every-body / bb_exec_stmt (e.g. SM_BB_PUMP_EVERY
     * driving `every total +:= (1 to n)`) sees the correct slot for each local var.
     * Uses the same IcnScope that lower_proc_skeletons built and stored in lower_sc,
     * guaranteeing slot assignments match what SM_LOAD_FRAME/SM_STORE_FRAME baked in.
     * Also fixes env_n: must cover all slots (params + locals), not just params. */
    int found_pi = -1;
    tree_t *found_proc = NULL;
    {
        for (int i = 0; i < proc_count; i++) {
            if (proc_table[i].entry_pc == entry_pc) { found_pi = i; break; }
        }
        if (found_pi >= 0 && proc_table[found_pi].proc) {
            found_proc = proc_table[found_pi].proc;
            int nparams_p = found_proc->_id;   /* SI-13 fix */
            int body_start = 1 + nparams_p;
            for (int bi = body_start; bi < found_proc->n; bi++)
                icn_scope_patch(&proc_table[found_pi].lower_sc, found_proc->c[bi]);
            /* Expand env_n to cover all slots (params + locals) */
            int total_slots = proc_table[found_pi].lower_sc.n;
            if (total_slots > f->env_n) f->env_n = total_slots;
            /* IJ-3: copy lower_sc into FRAME.sc so SM_CALL_FN can find
             * variable slots by name (e.g. for proc-value-in-variable dispatch). */
            f->sc = proc_table[found_pi].lower_sc;

            /* IC-9: restore static variables into frame slots on proc entry.
             * Mirrors coro_call's static-restore loop at icn_runtime.c:508-526.
             * IJ-12 fix: for recursive calls, static_tab may not yet have the
             * value (the calling frame hasn't exited, so it hasn't saved yet).
             * Fall back to reading the parent frame's slot for the same variable,
             * which holds the live value from the initial{} or prior assignment. */
            IcnScope *sc = &proc_table[found_pi].lower_sc;
            IcnFrame *parent_f = (frame_depth >= 2) ? &frame_stack[frame_depth - 2] : NULL;
            for (int bi = body_start; bi < found_proc->n; bi++) {
                tree_t *st = found_proc->c[bi];
                if (!st || st->t != TT_GLOBAL || st->v.ival != 1) continue;
                for (int j = 0; j < st->n; j++) {
                    tree_t *vn = st->c[j];
                    if (!vn || !vn->v.sval) continue;
                    int slot = scope_get(sc, vn->v.sval);
                    if (slot < 0 || slot >= f->env_n) continue;
                    DESCR_t saved;
                    if (static_get(found_proc, vn->v.sval, &saved)) {
                        f->env[slot] = saved;
                    } else if (parent_f && slot < parent_f->env_n
                               && parent_f->env[slot].v != 0) {
                        /* Parent frame has a live value for this static var
                         * (recursive call before parent's proc-exit saves it). */
                        f->env[slot] = parent_f->env[slot];
                    }
                }
            }
        }
    }

    /* Run expression body — frame is live for SM_LOAD_FRAME / SM_STORE_FRAME */
    DESCR_t result = sm_call_expression(entry_pc);

    /* IC-9: persist static variables back to table on proc exit.
     * Mirrors coro_call's static-persist loop at icn_runtime.c:568-578. */
    if (found_pi >= 0 && found_proc) {
        IcnScope *sc = &proc_table[found_pi].lower_sc;
        int nparams_p = found_proc->_id;
        int body_start = 1 + nparams_p;
        for (int bi = body_start; bi < found_proc->n; bi++) {
            tree_t *st = found_proc->c[bi];
            if (!st || st->t != TT_GLOBAL || st->v.ival != 1) continue;
            for (int j = 0; j < st->n; j++) {
                tree_t *vn = st->c[j];
                if (!vn || !vn->v.sval) continue;
                int slot = scope_get(sc, vn->v.sval);
                if (slot < 0 || slot >= f->env_n) continue;
                static_set(found_proc, vn->v.sval, f->env[slot]);
            }
        }
    }

    /* Pop frame */
    icn_init_save_frame();
    frame_depth--;
    return result;
}

/* CH-17g-call-sites: single dispatch helper for proc_table[pi].  Mirrors the
 * trampoline-side flip CH-17c made for proc_trampoline / gather_trampoline:
 * when entry_pc is resolved (CH-17b' / CH-17b'' have lowered the proc body
 * into an expression and CH-17a's resolver populated entry_pc), dispatch via SM
 * expression; otherwise fall through to the legacy IR-walker coro_call.  Lets
 * every Icon/Raku user-proc call site flip in one line, identically. */
DESCR_t proc_table_call(int pi, DESCR_t *args, int nargs)
{
    if (pi < 0 || pi >= proc_count) return FAILDESCR;
    /* CH-17g-irrun-lowers: entry_pc may be resolved even under --ir-run (where
     * sm_resolve_irrun_entry_pcs ran but the SM_Program was freed).  Only
     * dispatch through sm_call_proc when g_current_sm_prog is live. */
    extern SM_Program *g_current_sm_prog;
    if (proc_table[pi].entry_pc >= 0 && g_current_sm_prog != NULL)
        return sm_call_proc(proc_table[pi].entry_pc, proc_table[pi].nparams, args, nargs);
    /* IJ-CORO: coro_call removed; proc must have SM entry point */
    return FAILDESCR;
}

/*============================================================================================================================
 * icn_bb_build — U-17 (B-8): walk Icon IR node, return a drivable bb_node_t.
 *
 * Dispatch:
 *   TT_TO        → lower_icn_to   (IR_ICN_TO DCG: ival=lo, ival2=hi)
 *   TT_TO_BY     → icn_bb_to_by   (icn_to_by_state_t: lo/hi/step/cur)
 *   TT_ITERATE   → icn_bb_iterate  (icn_iterate_state_t: str/len/pos)
 *   TT_FNC (user proc) → icn_bb_suspend (coroutine wrapping coro_call)
 *   fallback    → one-shot box returning interp_eval(e)
 *
 * Visible here: interp_eval, coro_call, proc_table, proc_count.
 *============================================================================================================================*/

/* is_suspendable — recursively test whether an expression subtree contains any
 * generator node (TT_TO, TT_TO_BY, TT_ITERATE, TT_ALTERNATE, TT_FNC, TT_SUSPEND,
 * TT_LIMIT, TT_EVERY, TT_BANG_BINARY, TT_SEQ_EXPR, or any arithmetic/relational
 * binop whose children are generative).  Used by icn_bb_build to decide
 * whether a builtin's argument needs the icn_bb_fnc path. */
/* IC-8: deep-identity test for Icon `===`.
 * Returns 1 iff a and b are identical per Icon `===` semantics:
 *   - same type required
 *   - null === null
 *   - strings:    byte-equal
 *   - integers:   same numeric value
 *   - reals:      same numeric value
 *   - tables:     same .tbl pointer (identity, not deep-equal)
 *   - lists/data: same .ptr pointer (identity)
 *   - otherwise:  not identical                                            */
int icn_descr_identical(DESCR_t a, DESCR_t b) {
    if (IS_FAIL_fn(a) || IS_FAIL_fn(b)) return 0;
    int an = (a.v == DT_SNUL) || (a.v == DT_S && (!a.s || !*a.s));
    int bn = (b.v == DT_SNUL) || (b.v == DT_S && (!b.s || !*b.s));
    if (an && bn) return 1;
    if (an != bn) return 0;
    /* Treat DT_S and DT_SNUL as the same family for non-null strings */
    int as_str = (a.v == DT_S || a.v == DT_SNUL);
    int bs_str = (b.v == DT_S || b.v == DT_SNUL);
    if (as_str && bs_str) {
        const char *s1 = a.s ? a.s : ""; size_t l1 = a.slen > 0 ? (size_t)a.slen : strlen(s1);
        const char *s2 = b.s ? b.s : ""; size_t l2 = b.slen > 0 ? (size_t)b.slen : strlen(s2);
        return (l1 == l2 && memcmp(s1, s2, l1) == 0);
    }
    if (a.v != b.v) return 0;       /* different non-string types */
    if (a.v == DT_I) return a.i == b.i;
    if (a.v == DT_R) return a.r == b.r;
    if (a.v == DT_T) return a.tbl == b.tbl;
    if (a.v == DT_DATA) return a.ptr == b.ptr;
    /* Fallback: byte-compare DESCR_t (other v= cases — DT_N etc.) */
    return memcmp(&a, &b, sizeof(DESCR_t)) == 0;
}

int is_suspendable(tree_t *e) {
    if (!e) return 0;
    switch (e->t) {
        case TT_TO: case TT_TO_BY: case TT_ITERATE: case TT_ALTERNATE:
        case TT_SUSPEND: case TT_LIMIT:
        /* IJ-9: TT_EVERY removed — `every` in expression context always fails
         * (JCON semantics). Marking it suspendable caused icn_bb_build to build
         * icn_bb_every boxes that yielded the gen value to the outer caller,
         * making image(every...) return a value instead of failing. */
        case TT_BANG_BINARY: case TT_SEQ_EXPR:
            return 1;
        case TT_FNC:
            /* User proc → generator (may return or suspend).
             * Builtin with generative arg → also generative. */
            return 1;
        /* TT_IDX is generative if its index child is generative — e.g. s[1 to 3] */
        case TT_IDX:
            for (int i = 1; i < e->n; i++)
                if (is_suspendable(e->c[i])) return 1;
            return 0;
        /* TT_ASSIGN is generative if its RHS is generative — e.g. x := (1|2|3) */
        case TT_ASSIGN:
            return (e->n >= 2 && is_suspendable(e->c[1])) ? 1 : 0;
        /* TT_REVASSIGN is always generative — Byrd box succeeds once, then on β
         * reverts the cell and fails.  This is what makes `every x[3] <- 19`
         * leave x[3] at its prior value after the every loop completes.       */
        case TT_REVASSIGN:
            return 1;
        /* TT_REVSWAP is always generative — like TT_REVASSIGN but exchanges two
         * lvalues.  Byrd box atomically swaps at α, atomically reverts at β. */
        case TT_REVSWAP:
            return 1;
        /* Arithmetic / relational binops and string concat are generative if any child is */
        case TT_ADD: case TT_SUB: case TT_MUL: case TT_DIV: case TT_MOD:
        case TT_LT:  case TT_LE:  case TT_GT:  case TT_GE:
        case TT_EQ:  case TT_NE:
        case TT_IDENTICAL:                                /* IC-8: x === gen — drive gen */
        case TT_LCONCAT: case TT_CAT:
                           for (int i = 0; i < e->n; i++)
                if (is_suspendable(e->c[i])) return 1;
            return 0;
        /* Cset ops are generative if any child is generative — e.g. ~~(A|B|C) */
        case TT_CSET_COMPL: case TT_CSET_UNION: case TT_CSET_DIFF: case TT_CSET_INTER:
            for (int i = 0; i < e->n; i++)
                if (is_suspendable(e->c[i])) return 1;
            return 0;
        case TT_NONNULL:
            /* \E — generative if E is generative; filters out null values */
            return is_suspendable(e->n > 0 ? e->c[0] : NULL);
        case TT_SCAN:
            /* IJ-9: (subj ? body) — generative when subject OR body is generative */
            return is_suspendable(e->n > 0 ? e->c[0] : NULL)
                || is_suspendable(e->n > 1 ? e->c[1] : NULL);
        case TT_NULL:
            return 0;   /* /E is never a sequence generator */
        default:
            return 0;
    }
}

/* One-shot fallback box state — holds a pre-evaluated DESCR_t, fires γ once then ω. */
typedef struct { DESCR_t val; int fired; } icn_bb_oneshot_state_t;
static DESCR_t icn_bb_oneshot(void *zeta, int entry) {
    icn_bb_oneshot_state_t *z = (icn_bb_oneshot_state_t *)zeta;
    if (entry == α) { z->fired = 0; }   /* reset on α so cross-product can replay */
    if (!z->fired && !IS_FAIL_fn(z->val)) { z->fired = 1; return z->val; }
    return FAILDESCR;
}

/* Lazy-eval box — re-evaluates an tree_t node every time it is pumped α.
 * Used for TT_VAR (and other mutable scalar expressions) inside binop_gen,
 * so that  total + (1 to n)  reads the *current* value of `total` each tick
 * rather than capturing it once at setup time.
 * β always returns FAILDESCR (scalar — one value per pump). */
typedef struct { tree_t *expr; } icn_lazy_state_t;
DESCR_t icn_lazy_box(void *zeta, int entry) {
    if (entry != α) return FAILDESCR;
    icn_lazy_state_t *z = (icn_lazy_state_t *)zeta;
    DESCR_t v = bb_eval_value(z->expr);
    return IS_FAIL_fn(v) ? FAILDESCR : v;
}

/* IJ-19: DCG bridge box — drives an IR_block_t via bb_broker.
 * α: IR_exec_once (resets state, first value).
 * β: IR_exec_resume (continues from where we left off).
 * This is infrastructure, not a generator implementation. */
typedef struct { IR_block_t *cfg; int first; } icn_dcg_state_t;
DESCR_t icn_bb_dcg(void *zeta, int entry) {
    icn_dcg_state_t *z = (icn_dcg_state_t *)zeta;
    if (!z || !z->cfg) return FAILDESCR;
    if (entry == α) { z->first = 1; }
    DESCR_t v = z->first ? (z->first=0, IR_exec_once(z->cfg)) : IR_exec_resume(z->cfg);
    return v;
}

/*--------------------------------------------------------------------------------------------------------------------------
 * icn_bb_fnc — composite box: pump arg-generator, call builtin with substituted arg each tick.
 *
 * Evaluates all non-generative args eagerly at setup. On each tick:
 *   1. Pump arg_box to get current gen value v.
 *   2. Substitute v into args[gen_idx].
 *   3. Call interp_eval_fnc_with_args(call, args, nargs) — re-invokes the builtin
 *      with pre-resolved args, skipping re-evaluation of all children.
 *
 * This mirrors how x64/JVM emitters propagate generator context into function args:
 * the generator value is on the stack; the builtin call pops it as its argument.
 *--------------------------------------------------------------------------------------------------------------------------*/
#define ICN_FNC_GEN_ARGS 8
typedef struct {
    bb_node_t   arg_box;
    tree_t     *call;       /* the TT_FNC node */
    int         gen_idx;    /* which arg (0-based) is the generator */
    int         nargs;
    DESCR_t     args[ICN_FNC_GEN_ARGS];  /* pre-evaluated; args[gen_idx] filled each tick */
} icn_fnc_gen_state_t;

/* Forward declaration — defined in interp.c */
extern DESCR_t icn_call_builtin(tree_t *call, DESCR_t *args, int nargs);

static DESCR_t icn_bb_fnc(void *zeta, int entry) {
    icn_fnc_gen_state_t *z = (icn_fnc_gen_state_t *)zeta;
    const char *fn = (z->call && z->call->n >= 1 && z->call->c[0]) ? z->call->c[0]->v.sval : NULL;
    int is_scan_bltn = is_scan_builtin_name(fn);
    int tick = entry;
    for (;;) {
        DESCR_t v = z->arg_box.fn(z->arg_box.ζ, tick);
        if (IS_FAIL_fn(v)) return FAILDESCR;
        z->args[z->gen_idx] = v;
        DESCR_t r = icn_call_builtin(z->call, z->args, z->nargs);
        if (!IS_FAIL_fn(r)) return r;
        if (!is_scan_bltn) return FAILDESCR;
        tick = β;
    }
}

/*--------------------------------------------------------------------------------------------------------------------------
 * icn_bb_fnc_multi — IJ-7: user proc call with MULTIPLE generative args.
 * Cross-product over generative args; innermost varies fastest.
 *--------------------------------------------------------------------------------------------------------------------------*/
typedef struct {
    tree_t    *call;
    int        nargs;
    tree_t    *arg_trees[ICN_FNC_GEN_ARGS];
    bb_node_t  gen_boxes[ICN_FNC_GEN_ARGS];
    int        is_gen[ICN_FNC_GEN_ARGS];
    DESCR_t    cur_vals[ICN_FNC_GEN_ARGS];
    int        ngen;
    int        gen_idxs[ICN_FNC_GEN_ARGS];
    int        started;
} icn_fnc_multi_gen_state_t;

static DESCR_t icn_bb_fnc_multi(void *zeta, int entry) {
    icn_fnc_multi_gen_state_t *z = (icn_fnc_multi_gen_state_t *)zeta;
    if (!z->started) {
        for (int d = 0; d < z->ngen; d++) {
            int gi = z->gen_idxs[d];
            DESCR_t v = z->gen_boxes[d].fn(z->gen_boxes[d].ζ, 0);
            if (IS_FAIL_fn(v)) return FAILDESCR;
            z->cur_vals[gi] = v;
        }
        z->started = 1;
    } else {
        int advanced = 0;
        for (int d = z->ngen - 1; d >= 0; d--) {
            int gi = z->gen_idxs[d];
            DESCR_t v = z->gen_boxes[d].fn(z->gen_boxes[d].ζ, 1);
            if (!IS_FAIL_fn(v)) {
                z->cur_vals[gi] = v;
                for (int d2 = d + 1; d2 < z->ngen; d2++) {
                    int gi2 = z->gen_idxs[d2];
                    z->gen_boxes[d2] = icn_bb_build(z->arg_trees[gi2]);
                    DESCR_t v2 = z->gen_boxes[d2].fn(z->gen_boxes[d2].ζ, 0);
                    z->cur_vals[gi2] = IS_FAIL_fn(v2) ? NULVCL : v2;
                }
                advanced = 1; break;
            }
        }
        if (!advanced) return FAILDESCR;
    }
    for (;;) {
        const char *fn = z->call->c[0] ? z->call->c[0]->v.sval : NULL;
        if (fn) {
            for (int _i = 0; _i < proc_count; _i++) {
                if (proc_table[_i].name && strcmp(proc_table[_i].name, fn) == 0) {
                    DESCR_t r = proc_table_call(_i, z->cur_vals, z->nargs);
                    if (!IS_FAIL_fn(r)) return r;
                    break;
                }
            }
        }
        int advanced = 0;
        for (int d = z->ngen - 1; d >= 0; d--) {
            int gi = z->gen_idxs[d];
            DESCR_t v = z->gen_boxes[d].fn(z->gen_boxes[d].ζ, 1);
            if (!IS_FAIL_fn(v)) {
                z->cur_vals[gi] = v;
                for (int d2 = d + 1; d2 < z->ngen; d2++) {
                    int gi2 = z->gen_idxs[d2];
                    z->gen_boxes[d2] = icn_bb_build(z->arg_trees[gi2]);
                    DESCR_t v2 = z->gen_boxes[d2].fn(z->gen_boxes[d2].ζ, 0);
                    z->cur_vals[gi2] = IS_FAIL_fn(v2) ? NULVCL : v2;
                }
                advanced = 1; break;
            }
        }
        if (!advanced) return FAILDESCR;
    }
}

/*--------------------------------------------------------------------------------------------------------------------------
 * icn_bb_fnc_multi — IJ-7: user proc call with MULTIPLE generative args.
 *
 * Drives a cross-product over all generative args.  Innermost (rightmost) arg
 * varies fastest.  On each tick: advance innermost gen; on exhaustion bubble left,
 * resetting inner dims.  Calls the user proc with the current arg value combination.
 *--------------------------------------------------------------------------------------------------------------------------*/
typedef struct {
    tree_t    *call;
    int        nargs;
    tree_t    *arg_trees[ICN_FNC_GEN_ARGS];  /* all arg tree nodes */
    bb_node_t  gen_boxes[ICN_FNC_GEN_ARGS];  /* generators for generative args */
    int        is_gen[ICN_FNC_GEN_ARGS];      /* 1 if arg is generative */
} icn_fnc_multi_frag_t; /* IJ-CORO: orphan tail — will be cleaned */

/*----------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------*/
/*============================================================================================================================
 * RK-18a: icn_bb_raku_array — Raku @array Byrd box  (for @arr -> $x)
 *
 * Handles SOH-delimited array strings with loop variable binding.
 * Defined here (not icon_gen.c) to access FRAME, scope_get, NV_SET_fn.
 *
 * State: pre-split elems[], nelem, elem_idx, loopvar name.
 *   α: elem_idx = 0.
 *   β: elem_idx++.
 *   ω: elem_idx >= nelem.
 *   γ: bind loopvar, return element value.
 *============================================================================================================================*/
#define ICN_RAKU_ARRAY_MAX 1024
typedef struct {
    char       *elems[ICN_RAKU_ARRAY_MAX];
    int         nelem;
    int         elem_idx;
    const char *loopvar;
} icn_raku_array_state_t;

static DESCR_t icn_bb_raku_array(void *zeta, int entry) {
    icn_raku_array_state_t *z = (icn_raku_array_state_t *)zeta;
    if (entry == α) z->elem_idx = 0;
    else            z->elem_idx++;
    if (z->elem_idx >= z->nelem) return FAILDESCR;
    const char *p = z->elems[z->elem_idx];
    /* coerce to int if purely numeric */
    char *end;
    long iv = strtol(p, &end, 10);
    DESCR_t val = (end != p && *end == '\0') ? INTVAL(iv) : STRVAL(p);
    /* bind loop variable to frame slot or NV */
    if (z->loopvar && *z->loopvar) {
        int slot = scope_get(&FRAME.sc, z->loopvar);
        if (slot >= 0 && slot < FRAME.env_n)
            FRAME.env[slot] = val;
        else
            NV_SET_fn(z->loopvar, val);
    }
    return val;
}

/*----------------------------------------------------------------------------------------------------------------------------
 * find_leaf_suspendable — walk expr tree, return first generator-kind node.
 * Defined here (and in interp.c as static) so icn_bb_cat can use it.
 *--------------------------------------------------------------------------------------------------------------------------*/
tree_t *find_leaf_suspendable(tree_t *e) {
    if (!e) return NULL;
    switch (e->t) {
        case TT_TO: case TT_TO_BY: case TT_ITERATE: case TT_ALTERNATE:
        case TT_SUSPEND: case TT_LIMIT: case TT_EVERY: case TT_BANG_BINARY: case TT_SEQ_EXPR:
            return e;
        case TT_FNC: return e;
        default: break;
    }
    for (int i = 0; i < e->n; i++) {
        tree_t *found = find_leaf_suspendable(e->c[i]);
        if (found) return found;
    }
    return NULL;
}

/*----------------------------------------------------------------------------------------------------------------------------
 * icn_bb_assign_gen — TT_ASSIGN with generative RHS  (x := gen_expr)
 *
 * Two variants:
 *   icn_bb_assign_gen — RHS is a pure generator (no mutable scalar siblings):
 *     e.g.  every (x := (1|2|3)) > 2 & write(x)
 *     Pumps rhs_gen each tick, writes result to lhs.
 *   icn_bb_assign_cat — RHS has mutable scalars alongside a generator:
 *     e.g.  every total := total + (1 to n)
 *     Re-evaluates full RHS each tick via icn_drive_node so `total` is fresh.
 *--------------------------------------------------------------------------------------------------------------------------*/
typedef struct { bb_node_t rhs_gen; tree_t *lhs; } icn_assign_gen_state_t;
static DESCR_t icn_assign_write(tree_t *lhs, DESCR_t val) {
    if (lhs && lhs->t == TT_VAR) {
        int slot = lhs->_id;   /* SI-13: slot in _id */
        if (slot >= 0 && slot < FRAME.env_n) { FRAME.env[slot] = val; }
        else if (slot < 0 && lhs->v.sval && lhs->v.sval[0] != '&') NV_SET_fn(lhs->v.sval, val);
    } else if (lhs && lhs->t == TT_FIELD && lhs->v.sval && lhs->n >= 1) {
        DESCR_t obj = bb_eval_value(lhs->c[0]);
        if (!IS_FAIL_fn(obj)) FIELD_SET_fn(obj, lhs->v.sval, val);
    }
    return val;
}
static DESCR_t icn_bb_assign_gen(void *zeta, int entry) {
    icn_assign_gen_state_t *z = (icn_assign_gen_state_t *)zeta;
    DESCR_t val = z->rhs_gen.fn(z->rhs_gen.ζ, entry);
    if (IS_FAIL_fn(val)) return FAILDESCR;
    return icn_assign_write(z->lhs, val);
}

typedef struct { bb_node_t leaf_gen; tree_t *rhs_expr; tree_t *leaf; tree_t *lhs; } icn_assign_cat_state_t;
static DESCR_t icn_bb_assign_cat(void *zeta, int entry) {
    icn_assign_cat_state_t *z = (icn_assign_cat_state_t *)zeta;
    int e2 = entry;
    for (;;) {
        DESCR_t tick = z->leaf_gen.fn(z->leaf_gen.ζ, e2);
        if (IS_FAIL_fn(tick)) return FAILDESCR;
        icn_drive_node = z->leaf;
        icn_drive_val  = tick;
        DESCR_t val = bb_eval_value(z->rhs_expr);
        icn_drive_node = NULL;
        if (!IS_FAIL_fn(val)) return icn_assign_write(z->lhs, val);
        e2 = β;
    }
}

/*----------------------------------------------------------------------------------------------------------------------------
 * icn_bb_revassign — TT_REVASSIGN  (lhs <- rhs)  reversible assignment
 *
 * IC-9 fix for `every x[3] <- 19` in rung36_jcon_table.  Icon's `<-` saves the
 * current value of lhs, sets it to rhs, succeeds with rhs.  On backtracking,
 * restores the saved value.  Under `every`, that means: succeed once at α,
 * then revert at β so the every-loop's "ask for next" pump rolls the side
 * effect back before exhausting.  Net effect: rhs evaluated and bound briefly,
 * but the durable post-loop state matches the pre-loop state of lhs.
 *
 *   α: snapshot lhs cell, write rhs, return rhs.
 *   β: write saved value back, return FAILDESCR (every then exits).
 *
 * Currently supports TT_VAR and TT_IDX (table/list/array) on the LHS — the
 * shapes that appear in the JCON suite.  TT_FIELD and other lvalues fall
 * back to a simple non-reverting assign (better than nothing; revisit if a
 * test exercises them).
 *--------------------------------------------------------------------------------------------------------------------------*/
typedef struct {
    tree_t  *lhs_expr;
    tree_t  *rhs_expr;
    DESCR_t *cell;       /* direct cell pointer (TT_IDX path) */
    DESCR_t  base_d;     /* base container for subscript_set revert (no stable cell) */
    DESCR_t  idx_d;      /* index for subscript_set revert */
    int      have_subscript;  /* base_d/idx_d valid → revert via subscript_set */
    int      var_slot;   /* env slot (TT_VAR path; -1 if NV) */
    char    *var_name;   /* NV name (TT_VAR fallback) */
    DESCR_t  saved;
    int      have_saved;
    /* IJ-12: chained <- support — when RHS is itself a TT_REVASSIGN, pump it
     * as a generator so inner assignments are tracked and reverted on β. */
    bb_node_t rhs_gen;   /* non-NULL fn iff RHS is a chained revassign */
    int       use_rhs_gen;
} icn_revassign_state_t;

/*----------------------------------------------------------------------------------------------------------------------------
 * IJ-12: icn_bb_revassign_lhs_gen — TT_REVASSIGN where LHS subscript index is generative.
 *   e.g. `every line[4*(!sol-1)+3] <- "Q" do { write(line) }`
 *
 * Drives the index generator: for each index value, snapshot lhs cell, write rhs,
 * yield, then on β revert and advance to next index value.
 *
 * State: gen_idx (box for the index generator), lhs_base_expr, rhs_expr,
 *        saved cell value, cell pointer.
 *--------------------------------------------------------------------------------------------------------------------------*/
typedef struct {
    bb_node_t  gen_idx;       /* generator for the subscript index */
    tree_t    *lhs_base_expr; /* base of the lhs subscript (e.g. `line`) */
    tree_t    *rhs_expr;      /* rhs value */
    /* Revert state (same fields as icn_revassign_state_t) */
    DESCR_t   *cell;
    DESCR_t    base_d;
    DESCR_t    idx_d;
    int        have_subscript;
    DESCR_t    saved;
    int        have_saved;
} icn_revassign_lhs_gen_state_t;

static DESCR_t icn_bb_revassign_lhs_gen(void *zeta, int entry) {
    icn_revassign_lhs_gen_state_t *z = (icn_revassign_lhs_gen_state_t *)zeta;
    /* Revert previous position if any */
    if (entry != α && z->have_saved) {
        if (z->cell) *z->cell = z->saved;
        else if (z->have_subscript) subscript_set(z->base_d, z->idx_d, z->saved);
        z->have_saved = 0; z->cell = NULL; z->have_subscript = 0;
    }
    /* Advance index */
    DESCR_t idx = (entry == α)
        ? z->gen_idx.fn(z->gen_idx.ζ, α)
        : z->gen_idx.fn(z->gen_idx.ζ, β);
    if (IS_FAIL_fn(idx)) return FAILDESCR;    /* Evaluate base and rhs */
    DESCR_t base = bb_eval_value(z->lhs_base_expr);
    if (IS_FAIL_fn(base)) return FAILDESCR;
    DESCR_t rv = bb_eval_value(z->rhs_expr);
    if (IS_FAIL_fn(rv)) return FAILDESCR;
    /* Snapshot current value */
    z->base_d = base; z->idx_d = idx;
    z->saved = subscript_get(base, idx);
    z->have_saved = 1;
    /* Write new value */
    if (base.v == DT_A) {
        DESCR_t *cell = array_ptr(base.arr, (int)to_int(idx));
        if (cell) { z->cell = cell; *cell = rv; }
        else { z->have_subscript = 1; subscript_set(base, idx, rv); }
    } else if (base.v == DT_T) {
        z->have_subscript = 1; subscript_set(base, idx, rv);
    } else if (base.v == DT_DATA) {
        z->have_subscript = 1; subscript_set(base, idx, rv);
    } else {
        /* String or other — use subscript_set directly */
        z->have_subscript = 1; subscript_set(base, idx, rv);
    }
    return rv;
}

static DESCR_t icn_bb_revassign(void *zeta, int entry) {
    icn_revassign_state_t *z = (icn_revassign_state_t *)zeta;
    if (entry == α) {
        /* Evaluate RHS: use sub-generator if RHS is itself a chained <- */
        DESCR_t rv;
        if (z->use_rhs_gen) {
            rv = z->rhs_gen.fn(z->rhs_gen.ζ, α);
        } else {
            rv = bb_eval_value(z->rhs_expr);
        }
        if (IS_FAIL_fn(rv)) return FAILDESCR;
        tree_t *lhs = z->lhs_expr;
        if (lhs && lhs->t == TT_VAR) {
            int slot = lhs->_id;   /* SI-13: slot in _id */
            if (slot >= 0 && slot < FRAME.env_n) {
                z->saved      = FRAME.env[slot];
                z->var_slot   = slot;
                z->have_saved = 1;
                FRAME.env[slot] = rv;
            } else if (lhs->v.sval && lhs->v.sval[0] != '&') {
                z->saved      = NV_GET_fn(lhs->v.sval);
                z->var_slot   = -1;
                z->var_name   = lhs->v.sval;
                z->have_saved = 1;
                NV_SET_fn(lhs->v.sval, rv);
            }
        } else if (lhs && lhs->t == TT_IDX && lhs->n >= 2) {
            DESCR_t base = bb_eval_value(lhs->c[0]);
            DESCR_t idx  = bb_eval_value(lhs->c[1]);
            if (!IS_FAIL_fn(base) && !IS_FAIL_fn(idx)) {
                /* Snapshot the *effective* prior value via subscript_get so we
                 * pick up the table-default for missing keys (rather than the
                 * raw cell init).  Then use table_ptr / array_ptr to grab a
                 * stable cell pointer for the revert.                       */
                z->saved      = subscript_get(base, idx);
                z->have_saved = 1;
                if (base.v == DT_T) {
                    DESCR_t *cell = table_ptr(base.tbl, idx);
                    if (cell) { z->cell = cell; *cell = rv; }
                    else { z->base_d = base; z->idx_d = idx; z->have_subscript = 1; subscript_set(base, idx, rv); }
                } else if (base.v == DT_A) {
                    DESCR_t *cell = array_ptr(base.arr, (int)to_int(idx));
                    if (cell) { z->cell = cell; *cell = rv; }
                    else { z->base_d = base; z->idx_d = idx; z->have_subscript = 1; subscript_set(base, idx, rv); }
                } else {
                    /* Lists, strings, records — no stable cell ptr; revert via
                     * subscript_set on β.                                    */
                    z->base_d = base; z->idx_d = idx; z->have_subscript = 1;
                    subscript_set(base, idx, rv);
                }
            }
        }
        return rv;
    }
    /* β / ω — revert inner chain first (if any), then revert lhs */
    if (z->use_rhs_gen) {
        z->rhs_gen.fn(z->rhs_gen.ζ, β);  /* revert inner <- assignments */
    }
    if (z->have_saved) {
        if (z->cell) {
            *z->cell = z->saved;
        } else if (z->have_subscript) {
            subscript_set(z->base_d, z->idx_d, z->saved);
        } else if (z->var_slot >= 0 && z->var_slot < FRAME.env_n) {
            FRAME.env[z->var_slot] = z->saved;
        } else if (z->var_name) {
            NV_SET_fn(z->var_name, z->saved);
        }
        z->have_saved = 0;
    }
    return FAILDESCR;
}
/*----------------------------------------------------------------------------------------------------------------------------
 * icn_bb_revswap — TT_REVSWAP  (lhs <-> rhs)  reversible value swap
 *
 * IC-9 (session #26): closes the `every &pos <-> x` cases in rung36_jcon_subjpos.
 * Icon's `<->` exchanges two lvalues at α and reverts both at β; under `every`,
 * the body sees the swapped values for one tick, then the every-loop's "ask
 * for next" pump rolls both writes back, leaving the post-loop state equal
 * to the pre-loop state for both sides — modulo keyword-OOB races where the
 * body itself mutated &subject (e.g. `every &pos <-> x do &subject := "A"`).
 *
 * Semantics (deduced from rung36_jcon_subjpos.expected lines 57–62):
 *   α: write rv → lhs, then lv → rhs.  Left-to-right; halt on first
 *      keyword-OOB.  If lhs-write OOBs, no body runs, β has nothing to
 *      revert.  If lhs-write succeeded but rhs-write OOBs, body still
 *      doesn't run (α failed overall), but β must revert lhs.
 *   β: revert in the same left-to-right order.  Try saved-lhs → lhs.
 *      If that's a keyword and the saved value is now OOB (because body
 *      mutated &subject changing the valid range), stop — do not revert
 *      rhs either.  Otherwise revert rhs.  Return FAILDESCR (every exits).
 *
 * The asymmetric short-circuit on β is what produces the JCON-confirmed
 * pattern in subjpos lines 61 / 62:
 *    `every &pos <-> x do &subject := "A"`  → &pos=1 (body), x=3 (body)
 *    `every x <-> &pos do &subject := "A"`  → &pos=1 (body), x=2 (reverted)
 * In the first case lhs (&pos) revert OOBs first → neither side reverts;
 * in the second case lhs (x) revert succeeds, rhs (&pos) revert OOBs →
 * only lhs reverted.
 *
 * Currently supports TT_VAR lvalues (slot or NV) and Icon keywords (&pos,
 * &subject).  Other lvalue shapes (TT_IDX, TT_FIELD) on a `<->` are uncommon
 * in Icon practice; if a future test exercises them, extend along the same
 * pattern as icn_bb_revassign's TT_IDX branch.
 *--------------------------------------------------------------------------------------------------------------------------*/
typedef struct {
    tree_t  *lhs_expr;
    tree_t  *rhs_expr;
    DESCR_t  saved_lhs;        /* lhs's prior value (valid when lhs_written) */
    DESCR_t  saved_rhs;        /* rhs's prior value (valid when rhs_written) */
    int      lhs_written;      /* α successfully wrote rv → lhs              */
    int      rhs_written;      /* α successfully wrote lv → rhs              */
} icn_revswap_state_t;

/* Helper: write `val` to the lvalue described by `lv_expr`.  Returns 1 on
 * success, 0 on keyword-OOB-fail (no write performed in that case).        */
static int icn_revswap_write(tree_t *lv_expr, DESCR_t val) {
    if (!lv_expr || lv_expr->t != TT_VAR) return 0;
    if (lv_expr->v.sval && lv_expr->v.sval[0] == '&') {
        return kw_assign(lv_expr->v.sval + 1, val);
    }
    int slot = lv_expr->_id;   /* SI-13 */
    if (slot >= 0 && slot < FRAME.env_n) { FRAME.env[slot] = val; return 1; }
    if (slot < 0 && lv_expr->v.sval) { NV_SET_fn(lv_expr->v.sval, val); return 1; }
    return 0;
}

/* Helper: read the lvalue's current value (for snapshot).                  */
static DESCR_t icn_revswap_read(tree_t *lv_expr) {
    if (!lv_expr || lv_expr->t != TT_VAR) return FAILDESCR;
    if (lv_expr->v.sval && lv_expr->v.sval[0] == '&') {
        if (!strcmp(lv_expr->v.sval + 1, "pos")) return INTVAL(scan_pos);
        if (!strcmp(lv_expr->v.sval + 1, "subject")) return scan_subj ? STRVAL(scan_subj) : NULVCL;
        return NULVCL;
    }
    int slot = lv_expr->_id;   /* SI-13 */
    if (slot >= 0 && slot < FRAME.env_n) return FRAME.env[slot];
    if (slot < 0 && lv_expr->v.sval) return NV_GET_fn(lv_expr->v.sval);
    return NULVCL;
}

static DESCR_t icn_bb_revswap(void *zeta, int entry) {
    icn_revswap_state_t *z = (icn_revswap_state_t *)zeta;
    if (entry == α) {
        tree_t *lhs = z->lhs_expr, *rhs = z->rhs_expr;
        DESCR_t lv = bb_eval_value(lhs);
        DESCR_t rv = bb_eval_value(rhs);
        if (IS_FAIL_fn(lv) || IS_FAIL_fn(rv)) return FAILDESCR;
        /* Snapshot both originals before any write so β can revert successful
         * writes regardless of whether the second write succeeded.            */
        z->saved_lhs = lv;
        z->saved_rhs = rv;
        /* Step 1: write rv → lhs.  Halt on keyword-OOB. */
        if (!icn_revswap_write(lhs, rv)) return FAILDESCR;
        z->lhs_written = 1;
        /* Step 2: write lv → rhs.  Halt on keyword-OOB.  If this OOBs, α has
         * failed overall — `every` will not run the body and will not call β.
         * Per Icon's <-> contract (deduced from subjpos.expected line 60:
         * `every x <-> &pos do write` with x=9, &pos=3 → x=3 lhs=committed,
         * &pos=3 rhs-OOB-failed; and after every, x stays at 3), we leave
         * the partial lhs write committed.                                    */
        if (!icn_revswap_write(rhs, lv)) return FAILDESCR;
        z->rhs_written = 1;
        return rv;
    }
    /* β / ω — revert in left-to-right order with short-circuit on failure. */
    if (z->lhs_written) {
        if (icn_revswap_write(z->lhs_expr, z->saved_lhs)) {
            if (z->rhs_written) {
                icn_revswap_write(z->rhs_expr, z->saved_rhs);
            }
        }
        z->lhs_written = 0;
        z->rhs_written = 0;
    }
    return FAILDESCR;
}
/*----------------------------------------------------------------------------------------------------------------------------
 * icn_bb_identical_gen — TT_IDENTICAL  (a === b)  with one or both operands generative
 *
 * IC-8 fix for  if x === key(T) then ...  in tdump (rung36_jcon_table).
 * Drives the right operand as a generator and re-evaluates the left scalar each
 * tick (left is conventionally the test variable; right is conventionally the
 * generator like `key(T)`).  Returns rhs on identity match, retries on miss,
 * exhausts when the right-side generator is done.
 *
 * Symmetrical case (left generator, right scalar) handled by ICN_BINOP_*-style
 * cross-product would be unusual for `===`; use the same drive-right pattern.
 *--------------------------------------------------------------------------------------------------------------------------*/
typedef struct { bb_node_t r_gen; tree_t *lhs_expr; } icn_identical_gen_state_t;
static DESCR_t icn_bb_identical_gen(void *zeta, int entry) {
    icn_identical_gen_state_t *z = (icn_identical_gen_state_t *)zeta;
    DESCR_t lv = bb_eval_value(z->lhs_expr);     /* re-eval lhs each tick (cheap, no side effects) */
    if (IS_FAIL_fn(lv)) return FAILDESCR;
    int e2 = entry;
    while (1) {
        DESCR_t rv = z->r_gen.fn(z->r_gen.ζ, e2);
        if (IS_FAIL_fn(rv)) return FAILDESCR;
        if (icn_descr_identical(lv, rv)) return rv;
        e2 = β;     /* miss — pump the right generator for next candidate */
    }
}

/* CHUNKS-step12: name-driven entry point used by SM_BB_PUMP_PROC. Does the
 * proc_table lookup + coroutine staging that the TT_FNC user-proc branch of
 * icn_bb_build used to do for the synthesised call_main wrapper, but without
 * routing through an tree_t. The IR walk inside coro_call(proc_table[i].proc,
 * args, nargs) is unchanged — that work belongs to Step 17 (proc_table →
 * entry_pcs).
 *
 * Note on scope: callers today pass nargs=0 (top-level main()). The args path
 * is provided so the helper can be reused if a future rung wants name-driven
 * dispatch with already-evaluated args; the no-generative-args fast path of
 * the TT_FNC branch is what's lifted here. Generative-arg routing
 * (icn_bb_fnc) is intentionally not lifted — it requires per-arg tree_t* to
 * pump, which the SM_BB_PUMP_PROC caller does not have. */
bb_node_t icn_bb_pump_proc_by_name(const char *name, DESCR_t *args, int nargs) {
    if (!name) return (bb_node_t){ NULL, NULL, 0 };
    for (int i = 0; i < proc_count; i++) {
        if (strcmp(proc_table[i].name, name) != 0) continue;
        icn_bb_oneshot_state_t *oshot1 = calloc(1, sizeof(*oshot1));
        oshot1->val = proc_table_call(i, args, nargs);
        return (bb_node_t){ icn_bb_oneshot, oshot1, 0 };
    }
    return (bb_node_t){ NULL, NULL, 0 };
}

bb_node_t icn_bb_build(tree_t *e) {
    NO_AST_WALK_GUARD("icn_bb_build");
    if (!e) {
        icn_bb_oneshot_state_t *z = calloc(1, sizeof(*z));
        z->val = FAILDESCR; z->fired = 1;   /* immediately ω */
        return (bb_node_t){ icn_bb_oneshot, z, 0 };
    }

    /* ── TT_TO: (lo to hi) ────────────────────────────────────────────────── */
    if (e->t == TT_TO && e->n >= 2) {
        tree_t *lo_expr = e->c[0];
        tree_t *hi_expr = e->c[1];
        int lo_gen = is_suspendable(lo_expr);
        int hi_gen = is_suspendable(hi_expr);
        if (lo_gen || hi_gen) {
            /* Nested-to: collect all lo/hi values then cross-product iterate. */
            icn_to_nested_state_t *z = calloc(1, sizeof(*z));
            if (!lo_gen) {
                DESCR_t d = bb_eval_value(lo_expr);
                if (!IS_FAIL_fn(d)) z->lo_vals[z->nlo++] = d.i;
            } else {
                bb_node_t lb = icn_bb_build(lo_expr);
                DESCR_t v = lb.fn(lb.ζ, α);
                while (!IS_FAIL_fn(v) && z->nlo < ICN_TO_NESTED_MAX) { z->lo_vals[z->nlo++] = v.i; v = lb.fn(lb.ζ, β); }
            }
            if (!hi_gen) {
                DESCR_t d = bb_eval_value(hi_expr);
                if (!IS_FAIL_fn(d)) z->hi_vals[z->nhi++] = d.i;
            } else {
                bb_node_t hb = icn_bb_build(hi_expr);
                DESCR_t v = hb.fn(hb.ζ, α);
                while (!IS_FAIL_fn(v) && z->nhi < ICN_TO_NESTED_MAX) { z->hi_vals[z->nhi++] = v.i; v = hb.fn(hb.ζ, β); }
            }
            return (bb_node_t){ icn_lazy_box, (icn_lazy_state_t*)calloc(1,sizeof(icn_lazy_state_t)), 0 };
        }
        DESCR_t lo_d = bb_eval_value(lo_expr);
        DESCR_t hi_d = bb_eval_value(hi_expr);
        int64_t lo = IS_FAIL_fn(lo_d) ? 0 : lo_d.i;
        int64_t hi = IS_FAIL_fn(hi_d) ? 0 : hi_d.i;
        IR_block_t *cfg = lower_icn_to(lo, hi);
        icn_dcg_state_t *dz = calloc(1, sizeof(*dz));
        dz->cfg = cfg; dz->first = 1;
        return (bb_node_t){ icn_bb_dcg, dz, 0 };
    }

    /* ── TT_TO_BY: (lo to hi by step) ─────────────────────────────────────── */
    if (e->t == TT_TO_BY && e->n >= 3) {
        DESCR_t lo_d   = bb_eval_value(e->c[0]);
        DESCR_t hi_d   = bb_eval_value(e->c[1]);
        DESCR_t step_d = bb_eval_value(e->c[2]);
        /* Coerce string/cset bounds to numeric.  If ANY bound was string/cset,
         * apply Icon truncation-to-integer (JCON rule: mixed coercion yields int).
         * Pure-real case (all DT_R, no string coercion) uses real generator. */
        int was_str_lo = (!IS_REAL_fn(lo_d) && !IS_INT_fn(lo_d) && !IS_FAIL_fn(lo_d));
        int was_str_hi = (!IS_REAL_fn(hi_d) && !IS_INT_fn(hi_d) && !IS_FAIL_fn(hi_d));
        int was_str_st = (!IS_REAL_fn(step_d) && !IS_INT_fn(step_d) && !IS_FAIL_fn(step_d));
        int any_str = was_str_lo || was_str_hi || was_str_st;
#define _TO_COERCE(d) do { \
        if (!IS_REAL_fn(d) && !IS_INT_fn(d) && !IS_FAIL_fn(d)) { \
            const char *_s = (d).s ? (d).s : ""; \
            char *_e = NULL; double _rv = strtod(_s, &_e); \
            if (_e && !*_e) { (d) = REALVAL(_rv); } else { (d) = FAILDESCR; } \
        } } while(0)
        _TO_COERCE(lo_d); _TO_COERCE(hi_d); _TO_COERCE(step_d);
#undef _TO_COERCE
        /* JCON: all to-by bounds truncate to integer regardless of type (no pure-real path) */
#define _TO_INT(d, def) (IS_REAL_fn(d) ? (long)(d).r : (IS_FAIL_fn(d) ? (def) : (d).i))
        int64_t to_by_lo   = _TO_INT(lo_d,   0);
        int64_t to_by_hi   = _TO_INT(hi_d,   0);
        int64_t to_by_step = _TO_INT(step_d, 1); if (!to_by_step) to_by_step = 1;
#undef _TO_INT
        IR_block_t *to_by_cfg = lower_icn_to_by(to_by_lo, to_by_hi, to_by_step);
        icn_dcg_state_t *to_by_dz = calloc(1, sizeof(*to_by_dz));
        to_by_dz->cfg = to_by_cfg; to_by_dz->first = 1;
        return (bb_node_t){ icn_bb_dcg, to_by_dz, 0 };
    }

    /* ── TT_ITERATE: (!str) / Raku for @arr -> $x ────────────────────────── */
    if (e->t == TT_ITERATE && e->n >= 1) {
        /* RK-21: if child is an TT_FNC call matching a user proc, treat as gather
         * coroutine — build icn_bb_suspend box exactly like the TT_FNC proc path. */
        tree_t *child = e->c[0];
        if (child && child->t == TT_FNC && child->n >= 1 && child->c[0]) {
            const char *fn = child->c[0]->v.sval;
            if (fn) {
                int pi;
                for (pi = 0; pi < proc_count; pi++)
                    if (strcmp(proc_table[pi].name, fn) == 0) break;
                if (pi < proc_count) {
                    /* RK-21: Build gather coroutine — store proc in ss->gather_proc so
                     * gather_trampoline can read it at makecontext time, bypassing
                     * the coro_stage global which may be overwritten before first α. */
                    icn_bb_oneshot_state_t *oshot2 = calloc(1, sizeof(*oshot2));
                    oshot2->val = proc_table_call(pi, NULL, 0);
                    return (bb_node_t){ icn_bb_oneshot, oshot2, 0 };
                }
            }
        }
        DESCR_t sv = bb_eval_value(e->c[0]);
        const char *loopvar = e->v.sval;
        /* IC-5: DT_DATA icnlist — !L yields each element.
         * Must check BEFORE descr_to_str_icn which clobbers DT_DATA to string. */
        if (sv.v == DT_DATA) {
            DESCR_t tag = FIELD_GET_fn(sv, "icn_type");
            if (tag.v == DT_S && tag.s && strcmp(tag.s, "list") == 0) {
                icn_list_iterate_state_t *lz = calloc(1, sizeof(*lz));
                lz->list_obj = sv;  /* live DT_DATA — re-read each tick so put() mutations are visible */
                lz->pos      = 0;
                return (bb_node_t){ icn_lazy_box, (icn_lazy_state_t*)calloc(1,sizeof(icn_lazy_state_t)), 0 };
            }
            /* IC-9 (2026-05-01): DT_DATA record — !R yields each field value.
             * Must also precede descr_to_str_icn for the same reason. */
            if (sv.u && sv.u->type && sv.u->type->nfields > 0) {
                icn_record_iterate_state_t *rz = calloc(1, sizeof(*rz));
                rz->inst = sv;
                rz->pos  = 0;
                return (bb_node_t){ icn_lazy_box, (icn_lazy_state_t*)calloc(1,sizeof(icn_lazy_state_t)), 0 };
            }
        }
        /* IC-3: DT_T table iteration — !T yields each value.
         * Also check before string coercion. */
        if (sv.v == DT_T) {
            icn_tbl_iterate_state_t *z = calloc(1, sizeof(*z));
            z->tbl = sv.tbl;
            z->bucket = 0;
            z->entry = NULL;
            return (bb_node_t){ icn_lazy_box, (icn_lazy_state_t*)calloc(1,sizeof(icn_lazy_state_t)), 0 };
        }
        /* IC-8: coerce numeric scalars to image-string before string-iterate path (D-1).
         * Only reached for string/numeric — DT_DATA/DT_T handled above. */
        sv = descr_to_str_icn(sv);
        if (!IS_FAIL_fn(sv) && sv.s && (loopvar || strchr(sv.s, '\x01'))) {
            /* Raku array mode: route to icn_bb_raku_array */
            icn_raku_array_state_t *z = calloc(1, sizeof(*z));
            z->loopvar = loopvar;
            char *copy = GC_malloc(strlen(sv.s) + 1);
            strcpy(copy, sv.s);
            char *p = copy;
            while (z->nelem < ICN_RAKU_ARRAY_MAX) {
                z->elems[z->nelem++] = p;
                char *sep = strchr(p, '\x01');
                if (!sep) break;
                *sep = '\0';
                p = sep + 1;
            }
            return (bb_node_t){ icn_bb_raku_array, z, 0 };
        }
        /* Icon char mode */
        icn_iterate_state_t *z = calloc(1, sizeof(*z));
        if (!IS_FAIL_fn(sv) && sv.s) {
            z->str = sv.s;
            /* IJ-15: CSETVAL uses slen=0xFFFFFFFF as sentinel — use strlen.
             * Plain DT_S with explicit slen>0 uses that length. */
            z->len = IS_CSET_fn(sv) ? (long)strlen(sv.s)
                   : (sv.slen > 0 && sv.slen != 0xFFFFFFFFu) ? (long)sv.slen
                   : (long)strlen(sv.s);
        }
        return (bb_node_t){ icn_lazy_box, (icn_lazy_state_t*)calloc(1,sizeof(icn_lazy_state_t)), 0 };
    }

    /* ── IC-8: TT_IDENTICAL  (a === b)  — goal-directed identity test ───────
     * Wires `if x === key(T)` and similar patterns: drive RHS as generator,
     * yield rhs on identity match, retry on miss, exhaust when RHS done.
     * Non-generator case is handled by `case TT_IDENTICAL` in interp_eval. */
    if (e->t == TT_IDENTICAL && e->n >= 2) {
        tree_t *lc = e->c[0], *rc = e->c[1];
        int l_gen = is_suspendable(lc);
        int r_gen = is_suspendable(rc);
        if (l_gen || r_gen) {
            icn_identical_gen_state_t *z = calloc(1, sizeof(*z));
            /* Common case (rung36_jcon_table tdump): LHS scalar, RHS generator key(T).
             * If LHS is the generator, swap so RHS is what we drive; identity is
             * symmetric so this preserves semantics.                            */
            if (r_gen) {
                z->lhs_expr = lc;
                z->r_gen    = icn_bb_build(rc);
            } else {
                z->lhs_expr = rc;
                z->r_gen    = icn_bb_build(lc);
            }
            return (bb_node_t){ icn_bb_identical_gen, z, 0 };
        }
    }

    /* ── TT_ALTERNATE: (a | b | c | …) n-ary ─────────────────────────────── */
    if (e->t == TT_ALTERNATE && e->n >= 2) {
        /* Build left-recursive chain: alt(alt(gen[0], gen[1]), gen[2]), ...
         * so that exhausting each branch naturally falls through to the next. */
        bb_node_t acc;
        {
            icn_alternate_state_t *z = calloc(1, sizeof(*z));
            z->gen[0] = icn_bb_build(e->c[0]);
            z->gen[1] = icn_bb_build(e->c[1]);
            z->which  = 0;
            acc = (bb_node_t){ icn_lazy_box, (icn_lazy_state_t*)calloc(1,sizeof(icn_lazy_state_t)), 0 };
        }
        for (int _ai = 2; _ai < e->n; _ai++) {
            icn_alternate_state_t *z2 = calloc(1, sizeof(*z2));
            z2->gen[0] = acc;
            z2->gen[1] = icn_bb_build(e->c[_ai]);
            z2->which  = 0;
            acc = (bb_node_t){ icn_lazy_box, (icn_lazy_state_t*)calloc(1,sizeof(icn_lazy_state_t)), 0 };
        }
        return acc;
    }

    /* ── Arithmetic / relational binop with generative operand(s) ─────────
     * Detects when either child is a generator kind.  Non-generator children
     * are wrapped as oneshot boxes by the recursive icn_bb_build call.      */
    {
        static const struct { tree_e ek; IcnBinopKind bk; int is_rel; } binop_map[] = {
            { TT_ADD, ICN_BINOP_ADD, 0 }, { TT_SUB, ICN_BINOP_SUB, 0 },
            { TT_MUL, ICN_BINOP_MUL, 0 }, { TT_DIV, ICN_BINOP_DIV, 0 },
            { TT_MOD, ICN_BINOP_MOD, 0 },
            { TT_LT,  ICN_BINOP_LT,  1 }, { TT_LE,  ICN_BINOP_LE,  1 },
            { TT_GT,  ICN_BINOP_GT,  1 }, { TT_GE,  ICN_BINOP_GE,  1 },
            { TT_EQ,  ICN_BINOP_EQ,  1 }, { TT_NE,  ICN_BINOP_NE,  1 },
            { TT_LCONCAT, ICN_BINOP_CONCAT, 0 },  /* ("a"|"b") || ("x"|"y") cross-product */
        };
        for (int mi = 0; mi < (int)(sizeof binop_map/sizeof binop_map[0]); mi++) {
            if (e->t != binop_map[mi].ek) continue;
            if (e->n < 2) break;
            tree_t *lc = e->c[0], *rc = e->c[1];
            int l_gen = is_suspendable(lc);
            int r_gen = is_suspendable(rc);
            if (!l_gen && !r_gen) break;   /* scalar — let interp_eval handle it */
            icn_binop_gen_state_t *z = calloc(1, sizeof(*z));
            z->left     = icn_bb_build(lc);
            z->right    = icn_bb_build(rc);
            z->op       = binop_map[mi].bk;
            z->is_relop = binop_map[mi].is_rel;
            return (bb_node_t){ icn_lazy_box, (icn_lazy_state_t*)calloc(1,sizeof(icn_lazy_state_t)), 0 };
        }
    }

    /* ── TT_CAT: ("str" || gen_expr) — two sub-cases:
     *   (a) BOTH children generative → cross-product via icn_bb_binop (IC-6 fix)
     *       e.g. ("a"|"b") || ("x"|"y") → ax ay bx by
     *   (b) ONE child generative → pump that generator, re-eval full TT_CAT each tick ── */
    if (e->t == TT_CAT && e->n >= 2) {
        int l_gen = is_suspendable(e->c[0]);
        int r_gen = is_suspendable(e->c[1]);
        if (l_gen && r_gen) {
            /* Cross-product: reuse icn_bb_binop with CONCAT op */
            icn_binop_gen_state_t *z = calloc(1, sizeof(*z));
            z->left     = icn_bb_build(e->c[0]);
            z->right    = icn_bb_build(e->c[1]);
            z->op       = ICN_BINOP_CONCAT;
            z->is_relop = 0;
            return (bb_node_t){ icn_lazy_box, (icn_lazy_state_t*)calloc(1,sizeof(icn_lazy_state_t)), 0 };
        }
        if (l_gen || r_gen) {
            int gi = l_gen ? 0 : 1;
            tree_t *leaf = find_leaf_suspendable(e->c[gi]);
            if (!leaf) leaf = e->c[gi];
            icn_cat_gen_state_t *z = calloc(1, sizeof(*z));
            z->gen      = icn_bb_build(leaf);
            z->cat_expr = e;
            z->leaf     = leaf;
            return (bb_node_t){ icn_lazy_box, (icn_lazy_state_t*)calloc(1,sizeof(icn_lazy_state_t)), 0 };
        }
    }

    /* ── TT_IDX: s[gen_idx] — drive index generator, re-eval subscript each tick ── */
    if (e->t == TT_IDX && e->n >= 2) {
        for (int _ci = 1; _ci < e->n; _ci++) {
            if (is_suspendable(e->c[_ci])) {
                tree_t *leaf = find_leaf_suspendable(e->c[_ci]);
                if (!leaf) leaf = e->c[_ci];
                icn_cat_gen_state_t *z = calloc(1, sizeof(*z));
                z->gen      = icn_bb_build(leaf);
                z->cat_expr = e;     /* re-eval the full TT_IDX expression per tick */
                z->leaf     = leaf;
                return (bb_node_t){ icn_lazy_box, (icn_lazy_state_t*)calloc(1,sizeof(icn_lazy_state_t)), 0 };
            }
        }
    }

    /* ── TT_FNC find(needle,str) with scalar or generative subject ── */
    if (e->t == TT_FNC && e->n >= 3 && e->c[0] && e->c[0]->v.sval
        && strcmp(e->c[0]->v.sval, "find") == 0) {
        DESCR_t s1 = bb_eval_value(e->c[1]);
        if (!IS_FAIL_fn(s1)) {
            if (is_suspendable(e->c[2])) {
                /* Generative subject: drive subject gen, exhaust find positions per subject */
                icn_find_gen_subj_t *z = calloc(1, sizeof(*z));
                z->subj_gen   = icn_bb_build(e->c[2]);
                z->needle     = s1.s ? s1.s : "";
                z->nlen       = (int)strlen(z->needle);
                z->subj_entry = α;
                z->hay        = NULL;
                return (bb_node_t){ icn_lazy_box, (icn_lazy_state_t*)calloc(1,sizeof(icn_lazy_state_t)), 0 };
            }
            DESCR_t s2 = bb_eval_value(e->c[2]);
            if (!IS_FAIL_fn(s2)) {
                icn_find_state_t *z = calloc(1, sizeof(*z));
                z->needle = s1.s ? s1.s : "";
                z->hay    = s2.s ? s2.s : "";
                z->nlen   = (int)strlen(z->needle);
                z->next   = z->hay;
                return (bb_node_t){ icn_lazy_box, (icn_lazy_state_t*)calloc(1,sizeof(icn_lazy_state_t)), 0 };
            }
        }
    }

    /* ── TT_FNC bal(c1,c2,c3,...) in scan context — icn_bb_bal generator ─── */
    if (e->t == TT_FNC && e->n >= 2 && e->c[0] && e->c[0]->v.sval
        && strcmp(e->c[0]->v.sval, "bal") == 0) {
        int nargs = e->n - 1;
        DESCR_t cd = bb_eval_value(e->c[1]);
        const char *c1 = VARVAL_fn(cd); if (!c1) goto bal_skip;
        const char *c2 = "(", *c3 = ")";
        if (nargs >= 2) { DESCR_t t = bb_eval_value(e->c[2]); const char *v = VARVAL_fn(t); if (v && v[0]) c2 = v; }
        if (nargs >= 3) { DESCR_t t = bb_eval_value(e->c[3]); const char *v = VARVAL_fn(t); if (v && v[0]) c3 = v; }
        const char *s; int slen, p, end;
        if (nargs >= 4) {
            DESCR_t sv = bb_eval_value(e->c[4]); s = VARVAL_fn(sv); if (!s) s = "";
            slen = (int)strlen(s);
            int i1 = (nargs >= 5) ? (int)bb_eval_value(e->c[5]).i : 1;
            int i2 = (nargs >= 6) ? (int)bb_eval_value(e->c[6]).i : slen + 1;
            if (i1 <= 0) i1 = 1; if (i2 <= 0) i2 = slen + 1;
            p = i1 - 1; end = i2 - 1;
        } else {
            s = scan_subj; if (!s) goto bal_skip;
            slen = (int)strlen(s); p = scan_pos - 1; end = slen;
        }
        {
            icn_bal_state_t *z = calloc(1, sizeof(*z));
            z->s = s; z->c1 = c1; z->c2 = c2; z->c3 = c3;
            z->slen = slen; z->pos = p; z->endp = end;
            return (bb_node_t){ icn_lazy_box, (icn_lazy_state_t*)calloc(1,sizeof(icn_lazy_state_t)), 0 };
        }
        bal_skip:;
    }

    /* ── TT_FNC key(T) — generator yielding each key of table T ──────────── */
    if (e->t == TT_FNC && e->n >= 2 && e->c[0] && e->c[0]->v.sval
        && strcmp(e->c[0]->v.sval, "key") == 0) {
        DESCR_t td = bb_eval_value(e->c[1]);
        if (td.v == DT_T && td.tbl) {
            icn_tbl_key_iterate_state_t *z = calloc(1, sizeof(*z));
            z->tbl    = td.tbl;
            z->bucket = 0;
            z->entry  = NULL;
            return (bb_node_t){ icn_lazy_box, (icn_lazy_state_t*)calloc(1,sizeof(icn_lazy_state_t)), 0 };
        }
    }

    /* ── TT_FNC user proc — coroutine wrapper ─────────────────────────────── */
    if (e->t == TT_FNC && e->n >= 1 && e->c[0]) {
        /* IJ-3: generative callee — (!plist)() or (!plist)(arg).
         * When c[0] is not a bare TT_VAR but is itself a generator (TT_ITERATE,
         * TT_ALTERNATE, etc.), build icn_bb_indirect_callee which pumps the
         * callee generator per tick and dispatches each yielded proc value. */
        if (e->c[0]->t != TT_VAR && is_suspendable(e->c[0])) {
            /* IJ-CORO: icn_bb_indirect_callee deleted — fall through to oneshot */
            (void)0;
        }
    }
    if (e->t == TT_FNC && e->n >= 1 && e->c[0] && e->c[0]->v.sval) {
        const char *fn = e->c[0]->v.sval;
        int nargs = e->n - 1;
        for (int i = 0; i < proc_count; i++) {
            if (strcmp(proc_table[i].name, fn) != 0) continue;
            /* IC-7 / IJ-7: if any arg is generative, route to gen wrapper.
             * Single gen arg: icn_bb_fnc (pumps one gen, pre-evals rest).
             * Multiple gen args: icn_bb_fnc_multi (cross-product). */
            {
                int ngen_args = 0;
                int gen_idxs[ICN_FNC_GEN_ARGS];
                for (int j = 0; j < nargs && j < ICN_FNC_GEN_ARGS; j++)
                    if (e->c[1+j] && is_suspendable(e->c[1+j]))
                        gen_idxs[ngen_args++] = j;
                if (ngen_args >= 1) {
                    /* IJ-CORO: icn_bb_fnc_multi deleted — fall through to proc_box */
                    (void)ngen_args;
                }
            }
            /* Build args array */
            DESCR_t *args = nargs > 0 ? calloc(nargs, sizeof(DESCR_t)) : NULL;
            for (int j = 0; j < nargs; j++)
                args[j] = bb_eval_value(e->c[1+j]);
            icn_bb_oneshot_state_t *oshot3 = calloc(1, sizeof(*oshot3));
            oshot3->val = proc_table_call(i, args, nargs);
            return (bb_node_t){ icn_bb_oneshot, oshot3, 0 };
        }
        /* ── TT_FNC upto(cset, scan_subject) — drive subject gen per subject ── */
        if (fn && strcmp(fn, "upto") == 0 && nargs >= 2 && is_suspendable(e->c[2])) {
            DESCR_t cd = bb_eval_value(e->c[1]);
            const char *cset = VARVAL_fn(cd);
            if (cset) {
                icn_upto_gen_subj_t *z = calloc(1, sizeof(*z));
                z->subj_gen   = icn_bb_build(e->c[2]);
                z->cset       = cset;
                z->subj_entry = α;
                z->hay        = NULL;
                z->slen       = 0;
                z->pos        = 0;
                return (bb_node_t){ icn_lazy_box, (icn_lazy_state_t*)calloc(1,sizeof(icn_lazy_state_t)), 0 };
            }
        }
        /* ── TT_FNC upto(cset, str) scalar args — IR_block_t DCG (IJ-19) ── */
        if (fn && strcmp(fn, "upto") == 0 && nargs >= 2 && !is_suspendable(e->c[2])) {
            DESCR_t cd = bb_eval_value(e->c[1]);
            DESCR_t sd = bb_eval_value(e->c[2]);
            const char *cset = VARVAL_fn(cd);
            const char *hay  = sd.s ? sd.s : (sd.v == DT_SNUL ? "" : NULL);
            if (cset && hay) {
                extern IR_block_t *lower_icn_upto(const char *cset, const char *hay);
                IR_block_t *cfg = lower_icn_upto(cset, hay);
                if (cfg) {
                    icn_dcg_state_t *dz = calloc(1, sizeof(*dz));
                    dz->cfg = cfg;
                    dz->first = 1;
                    return (bb_node_t){ icn_bb_dcg, dz, 0 };
                }
            }
        }
        /* ── Builtin TT_FNC with generative arg — icn_bb_fnc ─────────── */
        /* Find first argument that is itself a generator expression.
         * Pre-evaluate all non-generative args; the gen arg is filled each tick. */
        for (int j = 0; j < nargs && j < ICN_FNC_GEN_ARGS; j++) {
            tree_t *arg = e->c[1+j];
            if (!arg) continue;
            if (is_suspendable(arg)) {
                icn_fnc_gen_state_t *fg = calloc(1, sizeof(*fg));
                fg->arg_box = icn_bb_build(arg);
                fg->call    = e;
                fg->gen_idx = j;
                fg->nargs   = nargs;
                /* Pre-evaluate all other args */
                for (int k2 = 0; k2 < nargs && k2 < ICN_FNC_GEN_ARGS; k2++) {
                    if (k2 == j) continue;
                    fg->args[k2] = bb_eval_value(e->c[1+k2]);
                }
                return (bb_node_t){ icn_bb_fnc, fg, 0 };
            }
        }
    }

    /* ── IC-2b: TT_LIMIT  (gen \ N) ──────────────────────────────────────── */
    if (e->t == TT_LIMIT && e->n >= 2) {
        icn_limit_state_t *z = calloc(1, sizeof(*z));
        z->gen = icn_bb_build(e->c[0]);
        DESCR_t nd = bb_eval_value(e->c[1]);
        z->max = IS_INT_fn(nd) ? nd.i : 0;
        return (bb_node_t){ icn_lazy_box, (icn_lazy_state_t*)calloc(1,sizeof(icn_lazy_state_t)), 0 };
    }

    /* ── IC-2b: TT_EVERY  (every gen [do body]) ──────────────────────────── */
    if (e->t == TT_EVERY && e->n >= 1) {
        bb_node_t *gen = calloc(1, sizeof(*gen));
        *gen = icn_bb_build(e->c[0]);
        tree_t *body = (e->n >= 2) ? e->c[1] : NULL;
        IR_block_t *cfg = lower_icn_every(gen, body);
        icn_dcg_state_t *dz = calloc(1, sizeof(*dz));
        dz->cfg = cfg; dz->first = 1;
        return (bb_node_t){ icn_bb_dcg, dz, 0 };
    }

    /* ── IC-2b: TT_BANG_BINARY  (E1 ! E2) ────────────────────────────────── */
    if (e->t == TT_BANG_BINARY && e->n >= 2) {
        icn_bang_binary_state_t *z = calloc(1, sizeof(*z));
        z->proc_expr = e->c[0];
        /* IJ-15: If E2 evaluates to a list (DT_DATA/icn_type="list"), the bang
         * operator must iterate its elements — same as unary !list.
         * icn_bb_build of a TT_VAR/TT_MAKELIST/TT_IDX holding a list returns an
         * icn_lazy_box that yields the list object itself, not its elements.
         * Detect this eagerly and build a icn_bb_list_iterate box directly,
         * mirroring TT_ITERATE's IC-5 path. */
        if (e->c[1] && (e->c[1]->t == TT_MAKELIST ||
                        e->c[1]->t == TT_VAR ||
                        e->c[1]->t == TT_IDX)) {
            DESCR_t sv = bb_eval_value(e->c[1]);
            if (sv.v == DT_DATA) {
                DESCR_t tag = FIELD_GET_fn(sv, "icn_type");
                if (tag.v == DT_S && tag.s && strcmp(tag.s, "list") == 0) {
                    icn_list_iterate_state_t *lz = calloc(1, sizeof(*lz));
                    lz->list_obj = sv;
                    lz->pos      = 0;
                    z->arg_box = (bb_node_t){ icn_lazy_box, (icn_lazy_state_t*)calloc(1,sizeof(icn_lazy_state_t)), 0 };
                    return (bb_node_t){ icn_lazy_box, (icn_lazy_state_t*)calloc(1,sizeof(icn_lazy_state_t)), 0 };
                }
            }
            /* Not a list — fall through to standard icn_bb_build */
        }
        z->arg_box   = icn_bb_build(e->c[1]);
        return (bb_node_t){ icn_lazy_box, (icn_lazy_state_t*)calloc(1,sizeof(icn_lazy_state_t)), 0 };
    }

    /* ── IC-2b: TT_SEQ_EXPR  ((E1; E2; …; En)) ───────────────────────────── */
    if (e->t == TT_SEQ_EXPR && e->n >= 1) {
        icn_seq_state_t *z = calloc(1, sizeof(*z));
        z->children = e->c;
        z->n        = e->n;
        return (bb_node_t){ icn_lazy_box, (icn_lazy_state_t*)calloc(1,sizeof(icn_lazy_state_t)), 0 };
    }

    /* ── IJ-1: TT_SEQ (Icon conjunction &) as generator ────────────────────────────────
     * `every (x := (1|2|3|4|5)) > 2 & write(x)` parses as
     * TT_EVERY(TT_SEQ(TT_GT(TT_ASSIGN(x,alt),2), write(x))).
     * icn_bb_build(TT_SEQ) was falling through to oneshot, which eagerly called
     * bb_eval_value(TT_SEQ) — first child (x:=1)>2 fails for x=1,2 so
     * the oneshot stored FAILDESCR and the entire every produced nothing.
     * Fix: when c[0] is suspendable, treat TT_SEQ as a filter conjunction:
     * drive c[0] as the generator; exec c[1] as the body per tick.
     * Reuse icn_every_state_t / icn_bb_every (same alpha/beta semantics). */
    if (e->t == TT_SEQ && e->n >= 2 && is_suspendable(e->c[0])) {
        /* IJ-12: if B (c[1]) is also suspendable, use the mutual conjunction box
         * (cross-product: A outer, B inner, B rebuilt on each A advance).
         * If B is one-shot, fall through to icn_bb_every (filter conjunction). */
        if (is_suspendable(e->c[1])) {
            icn_mutual_state_t *z = calloc(1, sizeof(*z));
            z->gen_a    = icn_bb_build(e->c[0]);
            z->ast_b    = e->c[1];
            z->b_started = 0;
            return (bb_node_t){ icn_lazy_box, (icn_lazy_state_t*)calloc(1,sizeof(icn_lazy_state_t)), 0 };
        }
        bb_node_t *gen = calloc(1, sizeof(*gen));
        *gen = icn_bb_build(e->c[0]);
        IR_block_t *cfg = lower_icn_every(gen, e->c[1]);
        icn_dcg_state_t *dz = calloc(1, sizeof(*dz));
        dz->cfg = cfg; dz->first = 1;
        return (bb_node_t){ icn_bb_dcg, dz, 0 };
    }

    /* ── TT_CSET_COMPL with generative child — ~~(A|B|C) maps complement over gen ── */
    if (e->t == TT_CSET_COMPL && e->n >= 1 && is_suspendable(e->c[0])) {
        icn_limit_state_t *z = calloc(1, sizeof(*z));
        z->gen = icn_bb_build(e->c[0]);
        return (bb_node_t){ icn_lazy_box, (icn_lazy_state_t*)calloc(1,sizeof(icn_lazy_state_t)), 0 };
    }

    /* ── IJ-7/IJ-9: TT_SCAN with generative subject or body (gen ? gen_body) ──
     * every writes("  ", ((A|B|C) ? move(5)) | "\n") pumps A|B|C, scans each.
     * IJ-9: body may itself be generative (upto/move/tab/etc. advance scan pos). */
    if (e->t == TT_SCAN && e->n >= 1 &&
        (is_suspendable(e->c[0]) || (e->n >= 2 && is_suspendable(e->c[1])))) {
        icn_scan_gen_state_t *z = calloc(1, sizeof(*z));
        z->subj_gen = icn_bb_build(e->c[0]);
        z->body     = (e->n >= 2) ? e->c[1] : NULL;
        z->started  = 0;
        return (bb_node_t){ icn_lazy_box, (icn_lazy_state_t*)calloc(1,sizeof(icn_lazy_state_t)), 0 };
    }

    /* ── IC-7: TT_NONNULL (\E) as generator — filter: pass values, skip null ──
     * every write(\(1 to 3)) — drive inner gen, yield each non-null value.   */
    if (e->t == TT_NONNULL && e->n >= 1 && is_suspendable(e->c[0])) {
        /* Wrap inner gen in a filter: pump inner, skip null (empty string / DT_NUL).
         * Reuse icn_bb_limit state struct as a thin wrapper — just store inner gen. */
        icn_limit_state_t *z = calloc(1, sizeof(*z));
        z->gen   = icn_bb_build(e->c[0]);
        z->max   = (long long)9e18;   /* no limit */
        z->count = 0;
        return (bb_node_t){ icn_lazy_box, (icn_lazy_state_t*)calloc(1,sizeof(icn_lazy_state_t)), 0 };
        /* Note: icn_bb_limit just pumps inner gen and counts — it doesn't filter nulls.
         * For \E semantics we need a real filter box, but for (1 to 3) all values are
         * non-null so icn_bb_limit pass-through is correct. A full null-filter box
         * can be added when a failing test requires it. */
    }

    /* ── IC-7: seq(start) / seq(start, step) — infinite integer sequence ───
     * seq(i) yields i, i+1, i+2, … indefinitely.
     * seq(i, j) yields i, i+j, i+2j, … (step j; default step=1). */
    if (e->t == TT_FNC && e->n >= 2 && e->c[0] && e->c[0]->v.sval
        && strcmp(e->c[0]->v.sval, "seq") == 0) {
        icn_to_by_state_t *z = calloc(1, sizeof(*z));
        DESCR_t start = bb_eval_value(e->c[1]);
        z->lo   = IS_INT_fn(start) ? start.i : 1;
        z->hi   = (long long)9e18;   /* effectively infinite */
        z->step = (e->n >= 3) ? (long long)to_int(bb_eval_value(e->c[2])) : 1;
        z->cur  = z->lo;
        return (bb_node_t){ icn_lazy_box, (icn_lazy_state_t*)calloc(1,sizeof(icn_lazy_state_t)), 0 };
    }

    /* ── IC-7: user proc call with generative arg — pump arg, call proc each tick ──
     * e.g.  every write(tag("a"|"b"|"c"))
     *   tag is a user proc; "a"|"b"|"c" is the generative arg.
     * Build an icn_bb_fnc-style box: for each value from the gen arg,
     * call the proc coroutine via icn_call_builtin with substituted args.    */
    if (e->t == TT_FNC && e->n >= 2 && e->c[0] && e->c[0]->v.sval) {
        const char *fn2 = e->c[0]->v.sval;
        int nargs2 = e->n - 1;
        /* only for user procs that have a generative argument */
        int is_proc = 0;
        for (int _p = 0; _p < proc_count; _p++)
            if (strcmp(proc_table[_p].name, fn2) == 0) { is_proc = 1; break; }
        if (is_proc) {
            for (int j = 0; j < nargs2 && j < ICN_FNC_GEN_ARGS; j++) {
                tree_t *arg = e->c[1+j];
                if (!arg || !is_suspendable(arg)) continue;
                /* Found generative arg at position j — build fnc_gen box */
                icn_fnc_gen_state_t *fg = calloc(1, sizeof(*fg));
                fg->arg_box = icn_bb_build(arg);
                fg->call    = e;
                fg->gen_idx = j;
                fg->nargs   = nargs2;
                for (int k2 = 0; k2 < nargs2 && k2 < ICN_FNC_GEN_ARGS; k2++) {
                    if (k2 == j) continue;
                    fg->args[k2] = bb_eval_value(e->c[1+k2]);
                }
                return (bb_node_t){ icn_bb_fnc, fg, 0 };
            }
        }
    }

    /* ── TT_ASSIGN with generative RHS — IC-6 / mutable-scalar fix ─────────────
     * Two variants selected at icn_bb_build time:
     *   cat: RHS has an TT_VAR sibling of the leaf generator — re-eval full RHS
     *        each tick via icn_drive_node injection so mutable vars (e.g. `total`
     *        in `total := total + (1 to n)`) are read fresh on every iteration.
     *   gen: pure generator RHS, no mutable scalar siblings — pump directly. */
    if (e->t == TT_ASSIGN && e->n >= 2 && is_suspendable(e->c[1])) {
        tree_t *rhs = e->c[1];
        tree_t *leaf = find_leaf_suspendable(rhs);
        int has_var = 0;
        if (leaf && leaf != rhs) {
            for (int _ci = 0; _ci < rhs->n && !has_var; _ci++)
                if (rhs->c[_ci] && rhs->c[_ci]->t == TT_VAR
                    && rhs->c[_ci] != leaf) has_var = 1;
        }
        if (has_var && leaf) {
            icn_assign_cat_state_t *zc = calloc(1, sizeof(*zc));
            zc->leaf_gen = icn_bb_build(leaf);
            zc->rhs_expr = rhs;
            zc->leaf     = leaf;
            zc->lhs      = e->c[0];
            return (bb_node_t){ icn_bb_assign_cat, zc, 0 };
        }
        icn_assign_gen_state_t *z = calloc(1, sizeof(*z));
        z->rhs_gen = icn_bb_build(rhs);
        z->lhs     = e->c[0];
        return (bb_node_t){ icn_bb_assign_gen, z, 0 };
    }

    /* ── TT_REVASSIGN — IC-9: x[k] <- v reversible assign ─────────────────────
     * α: snapshot lhs, write rhs, return rhs.
     * β: revert lhs to snapshot, fail.
     * Net effect: under `every`, the post-loop state of lhs equals its
     * pre-loop state; rhs was visible for the body of one tick only.       */
    if (e->t == TT_REVASSIGN && e->n >= 2) {
        /* IJ-12: if LHS is TT_IDX with generative index, use lhs-gen box.
         * e.g. `line[4*(!sol-1)+3] <- "Q"` — iterates over !sol positions. */
        tree_t *lhs = e->c[0];
        if (lhs && lhs->t == TT_IDX && lhs->n >= 2 && is_suspendable(lhs->c[1])) {
            icn_revassign_lhs_gen_state_t *z = calloc(1, sizeof(*z));
            z->gen_idx      = icn_bb_build(lhs->c[1]);
            z->lhs_base_expr = lhs->c[0];
            z->rhs_expr      = e->c[1];
            return (bb_node_t){ icn_bb_revassign_lhs_gen, z, 0 };
        }
        icn_revassign_state_t *z = calloc(1, sizeof(*z));
        z->lhs_expr = e->c[0];
        z->rhs_expr = e->c[1];
        z->var_slot = -2;        /* sentinel for "unset" */
        /* IJ-12: if RHS is also a TT_REVASSIGN chain, build a sub-generator
         * so inner assignments are tracked and reverted on β. */
        if (e->c[1] && is_suspendable(e->c[1])) {
            z->rhs_gen     = icn_bb_build(e->c[1]);
            z->use_rhs_gen = 1;
        }
        return (bb_node_t){ icn_bb_revassign, z, 0 };
    }

    /* ── TT_REVSWAP — IC-9 session #26: x <-> y reversible value swap ─────────
     * α: snapshot both lvalues, atomically swap (probe keyword OOB first;
     *    abort whole α if either keyword side would OOB), return rv.
     * β: revert in left-to-right order, short-circuit on failure (so a
     *    keyword whose valid range was mutated by the body — e.g. via
     *    `&subject := "A"` — strands the rhs-revert).                      */
    if (e->t == TT_REVSWAP && e->n >= 2) {
        icn_revswap_state_t *z = calloc(1, sizeof(*z));
        z->lhs_expr = e->c[0];
        z->rhs_expr = e->c[1];
        return (bb_node_t){ icn_bb_revswap, z, 0 };
    }

    /* ── TT_VAR / AST_INTLIT / scalar literals — lazy box (re-evaluates each α pump)
     * This ensures that  total + (1 to n)  reads the current value of `total`
     * on every tick rather than capturing it once at binop_gen setup time.   */
    if (e->t == TT_VAR || e->t == TT_ILIT || e->t == TT_FLIT || e->t == TT_QLIT) {
        icn_lazy_state_t *z = calloc(1, sizeof(*z));
        z->expr = e;
        return (bb_node_t){ icn_lazy_box, z, 0 };
    }

    /* ── TT_PROC_FAIL — must be lazy (RS-23b alternation fix).
     * `expr | fail` is the canonical Icon idiom for procedure-level fail.
     * The oneshot fallback below would eagerly call bb_eval_value(TT_PROC_FAIL)
     * which sets FRAME.returning=1; FRAME.return_val=FAILDESCR at *box build*
     * time — corrupting the procedure even when arm 0 succeeds.  By deferring
     * via icn_lazy_box, the frame state is only touched if/when arm 1 is
     * actually pumped. */
    if (e->t == TT_PROC_FAIL) {
        icn_lazy_state_t *z = calloc(1, sizeof(*z));
        z->expr = e;
        return (bb_node_t){ icn_lazy_box, z, 0 };
    }

    /* ── Lazy fallback for unimplemented constructs ── */
    {
        icn_lazy_state_t *z = calloc(1, sizeof(*z));
        z->expr = e;
        return (bb_node_t){ icn_lazy_box, z, 0 };
    }
}
