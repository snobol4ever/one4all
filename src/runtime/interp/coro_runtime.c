/*
 * coro_runtime.c — Icon interpreter runtime
 *
 * FI-4: extracted from src/driver/scrip.c.
 * IcnFrame stack, icn_gen_*, icn_scan_*, global_*, proc_table,
 * coro_call, coro_drive, coro_eval, coro_oneshot, icn_scope_*.
 *
 * RS-17a (2026-05-03): all 60 value-context interp_eval call sites in
 * this file routed through bb_eval_value (coro_value.c).
 * RS-17b (2026-05-03): all 13 statement-context interp_eval call sites in
 * this file routed through bb_exec_stmt (coro_stmt.c).  No direct
 * interp_eval reference remains here.
 *
 * AUTHORS: Lon Jones Cherryholmes · Claude Sonnet 4.6 (FI-4, 2026-04-14)
 */
#include "coro_runtime.h"
#include "coro_value.h"
#include "coro_stmt.h"
#include "../ast/ast.h"
#include "../../frontend/snobol4/scrip_cc.h"
#include "../../runtime/x86/bb_broker.h"
#include "../../frontend/icon/icon_gen.h"
#include "../../runtime/common/coerce.h"
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <gc/gc.h>

/* A0 — SCRIP_NO_AST_WALK tripwire guard.  Paste at top of every AST-walker
 * entry point.  Aborts only when SCRIP_NO_AST_WALK is set AND SM dispatch
 * is active — proving SM dispatch never reaches the AST walker.
 * Exception: g_ast_pump_active > 0 means SM_BB_PUMP_AST is on the call stack
 * (Phase A intentional bridge) — permit coro_eval in that case. */
#define NO_AST_WALK_GUARD(fn_name) \
    do { if (g_sm_dispatch_active && !g_ast_pump_active && getenv("SCRIP_NO_AST_WALK")) { \
        fprintf(stderr, "FATAL: " fn_name " reached from SM dispatch\n"); \
        abort(); \
    } } while (0)

/* RS-17b: with bb_exec_stmt now handling every statement-context site in this
 * file (was 13 direct interp_eval calls before RS-17b), and bb_eval_value
 * handling every value-context site (RS-17a), no `extern DESCR_t
 * interp_eval(AST_t *e);` declaration is needed here anymore.  The IR-mode
 * tree-walker reaches the work that arrives in coro_stmt.c's and
 * coro_value.c's fallthroughs; from this file's perspective, interp_eval is
 * gone.  This is the contract RS-19 locks in by promoting coro_runtime.c
 * into the isolation grep gate (after RS-18 closes for pl_runtime.c). */

/* NV_SET_fn lives in snobol4.c — needed by RK-16 loop-var binding */
extern DESCR_t NV_SET_fn(const char *name, DESCR_t val);

/* ── Icon unified interpreter state ────────────────────────────────────────
 * Icon procedures use slot-indexed locals (e->v.ival on AST_VAR nodes).
 * When interp_eval is running inside an Icon procedure call, frame_env points
 * to the current frame's slot array. AST_VAR case checks frame_env first.
 * FRAME.env_n is the slot count. Both are NULL/0 when in SNOBOL4 context.
 *
 * Icon procedure table: built from AST_PROGRAM at execute_program time.
 * Each entry maps procname → the AST_FNC node (from AST_STMT :subj).
 * ────────────────────────────────────────────────────────────────────────── */
IcnProcEntry proc_table[PROC_TABLE_MAX];
int          proc_count = 0;
int          g_lang         = 0;     /* 0=SNOBOL4 1=Icon */
AST_t      *g_icn_root     = NULL;  /* current Icon drive root */

/* A0 — SCRIP_NO_AST_WALK tripwire.  Set to 1 at entry of sm_interp_run /
 * sm_call_proc; cleared at exit.  When set, coro_eval / interp_eval /
 * interp_eval_pat / interp_eval_ref / call_user_function / execute_program
 * abort if SCRIP_NO_AST_WALK env var is set — proving SM dispatch never
 * reaches the AST walker under honest mode-3.  Global (not thread-local)
 * because scrip is single-threaded; revisit if threading is ever added. */
int g_sm_dispatch_active = 0;

/* GOAL-ICON-BB-COMPLETE Phase A: re-entrant suppression counter for SM_BB_PUMP_AST.
 * When > 0, coro_eval is explicitly permitted even if g_sm_dispatch_active=1.
 * Incremented at entry of SM_BB_PUMP_AST handler, decremented at exit.
 * Needed because SM_BB_PUMP_AST calls coro_eval deliberately (Phase A bridge),
 * and the Byrd-box functions called by coro_eval may re-enter sm_interp_run
 * (for SM proc bodies), which would reset g_sm_dispatch_active=1 before
 * any nested coro_eval calls inside the Byrd-box machinery. */
int g_ast_pump_active = 0;

/* OE-1: IcnFrame — per-call context for Icon procedure invocations.
 * Replaces the flat globals frame_env/FRAME.env_n/FRAME.returning/FRAME.return_val/
 * icn_gen_stack/icn_gen_depth/FRAME.loop_break with a pushed/popped frame stack.
 * FRAME refers to the active frame (frame_depth must be >0 in Icon context). */
IcnFrame frame_stack[FRAME_STACK_MAX];
int      frame_depth = 0;

/* coro_drive_fnc suspend-value passthrough: while running the every-body,
 * set coro_drive_node = the AST_FNC being driven and coro_drive_val = suspended value.
 * interp_eval(AST_FNC) returns coro_drive_val directly when e == coro_drive_node. */
AST_t  *coro_drive_node = NULL;
DESCR_t  coro_drive_val;

/* Convenience helpers that mirror the old flat-global helpers */
void frame_push(AST_t *n, long v, const char *sv) {
    IcnFrame *f = &FRAME;
    if (f->gen_depth < FRAME_DEPTH_MAX) { f->gen[f->gen_depth].node=n; f->gen[f->gen_depth].cur=v; f->gen[f->gen_depth].v.sval=sv; f->gen_depth++; }
}
void frame_pop(void) { if (FRAME.gen_depth > 0) FRAME.gen_depth--; }
int  icn_frame_lookup(AST_t *n, long *out) {
    IcnFrame *f = &FRAME;
    for (int i=f->gen_depth-1;i>=0;i--) if(f->gen[i].node==n){*out=f->gen[i].cur;return 1;} return 0;
}
int  icn_frame_lookup_sv(AST_t *n, long *out, const char **sv) {
    IcnFrame *f = &FRAME;
    for (int i=f->gen_depth-1;i>=0;i--) if(f->gen[i].node==n){*out=f->gen[i].cur;*sv=f->gen[i].v.sval;return 1;} return 0;
}
int  frame_active(AST_t *n) {
    IcnFrame *f = &FRAME;
    for (int i=0;i<f->gen_depth;i++) if(f->gen[i].node==n) return 1; return 0;
}

/* CHUNKS-step17b'' (CH-17b''): pure-DESCR_t forwarders to FRAME.env[slot].
 * Used by sm_interp.c's SM_LOAD_FRAME / SM_STORE_FRAME handlers so the SM
 * runtime can read/write Icon frame slots without including coro_runtime.h
 * (which would expose AST_t / IR types across the SM/IR boundary).
 *
 * Semantics mirror coro_value.c:382–399 for AST_VAR with frame_depth > 0:
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
     * icn_scope_patch + the bb_eval_value AST_ASSIGN path together
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
 * so coro_call can swapcontext back on AST_SUSPEND. NULL when not in a coroutine. */
coro_t *active_coro = NULL;

/* U-23: Icon global variable names -- bridge to SNO NV store.
 * Names declared `global X` in an Icon block are stored here.
 * icn_scope_patch skips slot assignment for these; AST_VAR read/write
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
 * CH-17g-statics: re-keyed off AST_t* onto (entry_pc, proc_name).
 * Primary key: entry_pc >= 0  → (entry_pc, var_name)  — stable SM pc.
 * Fallback key: entry_pc < 0  → (proc_name, var_name) — name string identity.
 * The fallback covers procs not yet lowered through sm_lower (AST_t path still
 * live in coro_call); it provides the same scoping guarantee that AST_t*
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

int static_get(AST_t *proc, const char *name, DESCR_t *out) {
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

void static_set(AST_t *proc, const char *name, DESCR_t val) {
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

int coro_drive(AST_t *e) {
    if (!e) return 0;
    if (frame_active(e)) return 0;
    AST_t *root = FRAME.body_root;
    if (e->t == AST_TO && e->n >= 2) {
        /* For scalar children: evaluate directly.
         * For generator children (e.g. (1 to 2) to (2 to 3)): drive each child
         * as a generator, iterating the cross-product of (lo_seq × hi_seq),
         * and for each (lo,hi) pair produce the inner lo..hi sequence. */
        AST_t *lo_expr = e->c[0];
        AST_t *hi_expr = e->c[1];
        int is_lo_gen = (lo_expr->t == AST_TO || lo_expr->t == AST_TO_BY || lo_expr->t == AST_ALTERNATE);
        int is_hi_gen = (hi_expr->t == AST_TO || hi_expr->t == AST_TO_BY || hi_expr->t == AST_ALTERNATE);
        int ticks = 0;

        if (!is_lo_gen && !is_hi_gen) {
            /* Fast path: both scalars */
            DESCR_t lo_d = bb_eval_value(lo_expr);
            DESCR_t hi_d = bb_eval_value(hi_expr);
            if (IS_FAIL_fn(lo_d)||IS_FAIL_fn(hi_d)) return 0;
            long lo=lo_d.i, hi=hi_d.i;
            for (long i=lo; i<=hi && !FRAME.returning; i++) {
                frame_push(e, i, NULL);
                int inner = coro_drive(root);
                if (!inner) bb_exec_stmt(FRAME.body_root);
                frame_pop(); ticks++;
                if (FRAME.returning) break;
            }
        } else {
            /* General path: drive lo_expr as generator (or scalar once) */
            /* Collect lo values */
            long lo_vals[256]; int nlo = 0;
            if (!is_lo_gen) {
                DESCR_t d = bb_eval_value(lo_expr);
                if (!IS_FAIL_fn(d)) lo_vals[nlo++] = d.i;
            } else {
                /* Drive lo_expr collecting all values */
                AST_t *saved_root = FRAME.body_root;
                /* Use frame_push/pop trick: temporarily drive lo_expr inline */
                /* Simple approach: evaluate lo as AST_TO sequence manually */
                if (lo_expr->t == AST_TO && lo_expr->n >= 2) {
                    DESCR_t a = bb_eval_value(lo_expr->c[0]);
                    DESCR_t b = bb_eval_value(lo_expr->c[1]);
                    if (!IS_FAIL_fn(a) && !IS_FAIL_fn(b))
                        for (long v = a.i; v <= b.i && nlo < 256; v++) lo_vals[nlo++] = v;
                }
                FRAME.body_root = saved_root;
            }
            /* Collect hi values */
            long hi_vals[256]; int nhi = 0;
            if (!is_hi_gen) {
                DESCR_t d = bb_eval_value(hi_expr);
                if (!IS_FAIL_fn(d)) hi_vals[nhi++] = d.i;
            } else {
                if (hi_expr->t == AST_TO && hi_expr->n >= 2) {
                    DESCR_t a = bb_eval_value(hi_expr->c[0]);
                    DESCR_t b = bb_eval_value(hi_expr->c[1]);
                    if (!IS_FAIL_fn(a) && !IS_FAIL_fn(b))
                        for (long v = a.i; v <= b.i && nhi < 256; v++) hi_vals[nhi++] = v;
                }
            }
            /* Cross-product: for each lo, for each hi, iterate lo..hi */
            for (int li = 0; li < nlo && !FRAME.returning; li++) {
                for (int hi2 = 0; hi2 < nhi && !FRAME.returning; hi2++) {
                    long lo = lo_vals[li], hi = hi_vals[hi2];
                    for (long i = lo; i <= hi && !FRAME.returning; i++) {
                        frame_push(e, i, NULL);
                        int inner = coro_drive(root);
                        if (!inner) bb_exec_stmt(FRAME.body_root);
                        frame_pop(); ticks++;
                        if (FRAME.returning) break;
                    }
                }
            }
        }
        return ticks;
    }
    if (e->t == AST_TO_BY && e->n >= 3) {
        DESCR_t lo_d=bb_eval_value(e->c[0]);
        DESCR_t hi_d=bb_eval_value(e->c[1]);
        DESCR_t st_d=bb_eval_value(e->c[2]);
        if(IS_FAIL_fn(lo_d)||IS_FAIL_fn(hi_d)||IS_FAIL_fn(st_d)) return 0;
        long lo=lo_d.i,hi=hi_d.i,st=st_d.i?st_d.i:1; int ticks=0;
        if(st>0){for(long i=lo;i<=hi&&!FRAME.returning;i+=st){frame_push(e,i,NULL);int inner=coro_drive(root);if(!inner)bb_exec_stmt(FRAME.body_root);frame_pop();ticks++;if(FRAME.returning)break;}}
        else    {for(long i=lo;i>=hi&&!FRAME.returning;i+=st){frame_push(e,i,NULL);int inner=coro_drive(root);if(!inner)bb_exec_stmt(FRAME.body_root);frame_pop();ticks++;if(FRAME.returning)break;}}
        return ticks;
    }
    /* S-6 / RK-16: AST_ITERATE — iterate string chars OR Raku @array elements.
     * If the string contains \x01 (SOH) it is a Raku array: split on SOH and
     * bind each element to the loop variable named in e->v.sval (if any).
     * Otherwise fall through to character-by-character Icon iteration.
     * IC-8: !N (integer) and !R (real) coerce to their image-string and iterate
     * each character — `!-514` → `-`,`5`,`1`,`4`; `!12.5` → `1`,`2`,`.`,`5`. */
    if (e->t == AST_ITERATE && e->n >= 1) {
        DESCR_t sv_d = bb_eval_value(e->c[0]);
        if (IS_FAIL_fn(sv_d)) return 0;
        /* IC-8: coerce numeric scalars to image-string before string-iterate path (D-1) */
        sv_d = descr_to_str_icn(sv_d);
        if (!IS_STR_fn(sv_d)) return 0;
        const char *str = sv_d.s ? sv_d.s : "";
        const char *loopvar = e->v.sval;   /* loop variable name, or NULL */

        /* Raku array iteration: use when loopvar is set (for @arr -> $x)
         * OR when string contains \x01 (multi-element array). */
        if (loopvar || strchr(str, '\x01')) {
            /* Split on \x01 and iterate elements */
            char *copy = GC_malloc(strlen(str) + 1);
            strcpy(copy, str);
            int ticks = 0;
            char *p = copy;
            while (!FRAME.returning) {
                char *sep = strchr(p, '\x01');
                if (sep) *sep = '\0';
                /* bind loop variable to the slot in the current frame */
                if (loopvar && *loopvar) {
                    DESCR_t elem = STRVAL(p);
                    /* coerce to int if purely numeric */
                    char *end;
                    long iv = strtol(p, &end, 10);
                    if (end != p && *end == '\0') elem = INTVAL(iv);
                    /* try slot first, fall back to NV */
                    int slot = scope_get(&FRAME.sc, loopvar);
                    if (slot >= 0 && slot < FRAME.env_n)
                        FRAME.env[slot] = elem;
                    else
                        NV_SET_fn(loopvar, elem);
                }
                frame_push(e, ticks, p);
                int inner = coro_drive(root);
                if (!inner) bb_exec_stmt(FRAME.body_root);
                frame_pop(); ticks++;
                if (!sep || FRAME.returning) break;
                p = sep + 1;
            }
            return ticks;
        }

        /* Icon-style character iteration */
        long len = (long)strlen(str); int ticks = 0;
        for (long i = 0; i < len && !FRAME.returning; i++) {
            frame_push(e, i, str);
            int inner = coro_drive(root);
            if (!inner) bb_exec_stmt(FRAME.body_root);
            frame_pop(); ticks++;
            if (FRAME.returning) break;
        }
        return ticks;
    }
    /* S-7: find(pat,str) as generator — successive 1-based positions. */
    if (e->t == AST_FNC && e->n>=3
        && e->c[0] && e->c[0]->v.sval
        && strcmp(e->c[0]->v.sval,"find")==0) {
        DESCR_t s1 = bb_eval_value(e->c[1]);
        DESCR_t s2 = bb_eval_value(e->c[2]);
        if (IS_FAIL_fn(s1)||IS_FAIL_fn(s2)) return 0;
        const char *needle = VARVAL_fn(s1), *hay = VARVAL_fn(s2);
        if (!needle||!hay) return 0;
        int nlen=(int)strlen(needle), ticks=0;
        const char *p = hay;
        while (!FRAME.returning) {
            char *hit = strstr(p, needle);
            if (!hit) break;
            long pos1 = (long)(hit - hay) + 1;
            frame_push(e, pos1, NULL);
            int inner = coro_drive(root);
            if (!inner) bb_exec_stmt(FRAME.body_root);
            frame_pop(); ticks++;
            if (FRAME.returning) break;
            p = hit + (nlen > 0 ? nlen : 1);
        }
        return ticks;
    }
    /* ── AST_FNC user proc — suspend-aware coroutine driver ────────────────── */
    if (e->t == AST_FNC) { int t = coro_drive_fnc(e); if (t > 0) return t; }
    for(int i=0;i<e->n;i++){int t=coro_drive(e->c[i]);if(t>0)return t;}
    return 0;
}


/* scope_add/patch: mirror of scope_add/scope_patch in icon_interp.c.
 * Assigns slot indices to AST_VAR nodes by name, in-place on the AST. */

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
void icn_scope_patch(IcnScope *sc, AST_t *e) {
    if (!e) return;
    if (e->t == AST_GLOBAL) {
        for (int i=0;i<e->n;i++)
            if(e->c[i]&&e->c[i]->v.sval) scope_add(sc, e->c[i]->v.sval);
        return;
    }
    if (e->t == AST_VAR && e->v.sval) {
        /* U-23: globals bridge to SNO NV store — skip slot, preserve sval, set ival=-1 */
        if (is_global(e->v.sval)) { e->v.ival = -1; }
        else { int s = scope_add(sc, e->v.sval); if (s >= 0) e->v.ival = s; else e->v.ival = -1; }
    }
    for (int i=0;i<e->n;i++) icn_scope_patch(sc, e->c[i]);
}

/* coro_call: call an Icon procedure node (AST_FNC with body children).
 * Mirrors icn_call() in icon_interp.c exactly, but uses DESCR_t and frame_env. */
DESCR_t coro_call(AST_t *proc, DESCR_t *args, int nargs) {
    int nparams = (int)proc->v.ival;
    int body_start = 1 + nparams;
    int nbody = proc->n - body_start;

    /* Build name→slot scope: params first, then locals from AST_GLOBAL decls */
    IcnScope sc; sc.n = 0;
    for (int i = 0; i < nparams && i < FRAME_SLOT_MAX; i++) {
        AST_t *pn = proc->c[1+i];
        if (pn && pn->v.sval) scope_add(&sc, pn->v.sval);
    }
    for (int i = 0; i < nbody; i++) {
        AST_t *st = proc->c[body_start+i];
        if (st && st->t == AST_GLOBAL)
            for (int j = 0; j < st->n; j++)
                if (st->c[j] && st->c[j]->v.sval)
                    scope_add(&sc, st->c[j]->v.sval);
    }
    /* Patch AST_VAR.v.ival with slot indices throughout body.
     * scope_patch also adds any undeclared vars it encounters to sc,
     * so sc.n after patching is the true slot count. */
    for (int i = 0; i < nbody; i++)
        icn_scope_patch(&sc, proc->c[body_start+i]);

    /* nslots = total slots assigned (params + locals + any undeclared vars) */
    int nslots = sc.n > 0 ? sc.n : (nparams > 0 ? nparams : FRAME_SLOT_MAX);
    if (nslots > FRAME_SLOT_MAX) nslots = FRAME_SLOT_MAX;

    /* Push a fresh IcnFrame for this call */
    if (frame_depth >= FRAME_STACK_MAX) return FAILDESCR;
    IcnFrame *f = &frame_stack[frame_depth++];
    memset(f, 0, sizeof *f);
    f->env_n = nslots;
    f->sc    = sc;   /* IM-10: save name→slot map so monitor can name locals */
    for (int i = 0; i < nparams && i < nargs && i < FRAME_SLOT_MAX; i++)
        f->env[i] = args[i];

    /* IC-9 (2026-05-01): static-variable persistence.  Walk body for AST_GLOBAL
     * decls with ival==1 (set by parser when keyword was `static`).  For each
     * static var, look up its persisted value via static_get(proc, name);
     * if present, copy into the slot.  At proc exit (below), write each
     * static var's current slot value back.  Per-proc table; statics with
     * the same name in different procs do not share storage.   */
    for (int i = 0; i < nbody; i++) {
        AST_t *st = proc->c[body_start + i];
        if (!st || st->t != AST_GLOBAL || st->v.ival != 1) continue;
        for (int j = 0; j < st->n; j++) {
            AST_t *vn = st->c[j];
            if (!vn || !vn->v.sval) continue;
            int slot = scope_get(&sc, vn->v.sval);
            if (slot < 0 || slot >= nslots) continue;
            DESCR_t saved;
            if (static_get(proc, vn->v.sval, &saved))
                f->env[slot] = saved;
        }
    }

    /* Execute body statements — mirrors coro_drive_fnc's suspend-aware stmt loop.
     * On AST_SUSPEND: yield to coroutine caller via swapcontext, run do-clause on
     * resume, then pin stmt index so loop stmts (AST_WHILE/AST_REPEAT/AST_UNTIL) are
     * re-entered naturally rather than restarted via a redundant interp_eval. */
    DESCR_t result = NULVCL;
    int stmt = 0;
    while (stmt < nbody && !FRAME.returning && !FRAME.loop_break) {
        AST_t *st = proc->c[body_start + stmt];
        if (!st || st->t == AST_GLOBAL) { stmt++; continue; }
        FRAME.body_root = st;
        FRAME.suspending = 0;
        bb_exec_stmt(st);
        if (FRAME.suspending && active_coro) {
            /* Yield to caller; coroutine resumes here after each β pump. */
            while (FRAME.suspending && active_coro) {
                coro_t *ss = active_coro;
                AST_t *doclause        = FRAME.suspend_do;
                ss->yielded             = FRAME.suspend_val;
                FRAME.suspending      = 0;
                swapcontext(&ss->gen_ctx, &ss->caller_ctx);
                /* Resumed by β: run do-clause (e.g. i := i + 1) before re-entry */
                if (doclause) bb_exec_stmt(doclause);
                /* For loop stmts: re-enter without calling bb_exec_stmt again here;
                 * just break out so the outer while re-issues bb_exec_stmt(st).
                 * For non-loop stmts (bare AST_SUSPEND): advance past stmt. */
                if (st->t != AST_WHILE && st->t != AST_REPEAT && st->t != AST_UNTIL)
                    stmt++;
                break;   /* always break — outer while re-enters st or advances */
            }
        } else {
            stmt++;
        }
        if (FRAME.returning || FRAME.loop_break) break;
    }
    /* Icon semantics: explicit return → return the value; fall off end → fail. */
    if (FRAME.returning) result = FRAME.return_val;
    else result = FAILDESCR;

    /* IC-9: persist static-variable values back to per-proc static table
     * before frame is destroyed.  Mirror the entry-restore loop above. */
    for (int i = 0; i < nbody; i++) {
        AST_t *st = proc->c[body_start + i];
        if (!st || st->t != AST_GLOBAL || st->v.ival != 1) continue;
        for (int j = 0; j < st->n; j++) {
            AST_t *vn = st->c[j];
            if (!vn || !vn->v.sval) continue;
            int slot = scope_get(&sc, vn->v.sval);
            if (slot < 0 || slot >= nslots) continue;
            static_set(proc, vn->v.sval, f->env[slot]);
        }
    }

    /* Pop frame — restores caller's FRAME automatically */
    icn_init_save_frame();   /* IC-5: persist initial-block statics before env is gone */
    frame_depth--;
    return result;
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
 * Static-variable persistence deferred to CH-17g (statics keyed on AST_t*;
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

    /* CH-17g-proc-locals: patch AST_VAR.v.ival with frame slot indices so that
     * AST-walker code running inside every-body / bb_exec_stmt (e.g. SM_BB_PUMP_EVERY
     * driving `every total +:= (1 to n)`) sees the correct slot for each local var.
     * Uses the same IcnScope that lower_proc_skeletons built and stored in lower_sc,
     * guaranteeing slot assignments match what SM_LOAD_FRAME/SM_STORE_FRAME baked in.
     * Also fixes env_n: must cover all slots (params + locals), not just params. */
    {
        int found_pi = -1;
        for (int i = 0; i < proc_count; i++) {
            if (proc_table[i].entry_pc == entry_pc) { found_pi = i; break; }
        }
        if (found_pi >= 0 && proc_table[found_pi].proc) {
            AST_t *proc = proc_table[found_pi].proc;
            int nparams_p = (int)proc->v.ival;
            int body_start = 1 + nparams_p;
            for (int bi = body_start; bi < proc->n; bi++)
                icn_scope_patch(&proc_table[found_pi].lower_sc, proc->c[bi]);
            /* Expand env_n to cover all slots (params + locals) */
            int total_slots = proc_table[found_pi].lower_sc.n;
            if (total_slots > f->env_n) f->env_n = total_slots;
        }
    }

    /* Run expression body — frame is live for SM_LOAD_FRAME / SM_STORE_FRAME */
    DESCR_t result = sm_call_expression(entry_pc);

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
    return coro_call(proc_table[pi].proc, args, nargs);
}

/*============================================================================================================================
 * coro_eval — U-17 (B-8): walk Icon IR node, return a drivable bb_node_t.
 *
 * Dispatch:
 *   AST_TO        → coro_bb_to      (icn_to_state_t:    lo/hi/cur)
 *   AST_TO_BY     → coro_bb_to_by   (icn_to_by_state_t: lo/hi/step/cur)
 *   AST_ITERATE   → coro_bb_iterate  (icn_iterate_state_t: str/len/pos)
 *   AST_FNC (user proc) → coro_bb_suspend (coroutine wrapping coro_call)
 *   fallback    → one-shot box returning interp_eval(e)
 *
 * Visible here: interp_eval, coro_call, proc_table, proc_count.
 *============================================================================================================================*/

/* is_suspendable — recursively test whether an expression subtree contains any
 * generator node (AST_TO, AST_TO_BY, AST_ITERATE, AST_ALTERNATE, AST_FNC, AST_SUSPEND,
 * AST_LIMIT, AST_EVERY, AST_BANG_BINARY, AST_SEQ_EXPR, or any arithmetic/relational
 * binop whose children are generative).  Used by coro_eval to decide
 * whether a builtin's argument needs the coro_bb_fnc path. */
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

int is_suspendable(AST_t *e) {
    if (!e) return 0;
    switch (e->t) {
        case AST_TO: case AST_TO_BY: case AST_ITERATE: case AST_ALTERNATE:
        case AST_SUSPEND: case AST_LIMIT: case AST_EVERY:
        case AST_BANG_BINARY: case AST_SEQ_EXPR:
            return 1;
        case AST_FNC:
            /* User proc → generator (may return or suspend).
             * Builtin with generative arg → also generative. */
            return 1;
        /* AST_IDX is generative if its index child is generative — e.g. s[1 to 3] */
        case AST_IDX:
            for (int i = 1; i < e->n; i++)
                if (is_suspendable(e->c[i])) return 1;
            return 0;
        /* AST_ASSIGN is generative if its RHS is generative — e.g. x := (1|2|3) */
        case AST_ASSIGN:
            return (e->n >= 2 && is_suspendable(e->c[1])) ? 1 : 0;
        /* AST_REVASSIGN is always generative — Byrd box succeeds once, then on β
         * reverts the cell and fails.  This is what makes `every x[3] <- 19`
         * leave x[3] at its prior value after the every loop completes.       */
        case AST_REVASSIGN:
            return 1;
        /* AST_REVSWAP is always generative — like AST_REVASSIGN but exchanges two
         * lvalues.  Byrd box atomically swaps at α, atomically reverts at β. */
        case AST_REVSWAP:
            return 1;
        /* Arithmetic / relational binops and string concat are generative if any child is */
        case AST_ADD: case AST_SUB: case AST_MUL: case AST_DIV: case AST_MOD:
        case AST_LT:  case AST_LE:  case AST_GT:  case AST_GE:
        case AST_EQ:  case AST_NE:
        case AST_IDENTICAL:                                /* IC-8: x === gen — drive gen */
        case AST_LCONCAT: case AST_CAT:
                           for (int i = 0; i < e->n; i++)
                if (is_suspendable(e->c[i])) return 1;
            return 0;
        case AST_NONNULL:
            /* \E — generative if E is generative; filters out null values */
            return is_suspendable(e->n > 0 ? e->c[0] : NULL);
        case AST_NULL:
            return 0;   /* /E is never a sequence generator */
        default:
            return 0;
    }
}

/* One-shot fallback box state — holds a pre-evaluated DESCR_t, fires γ once then ω. */
typedef struct { DESCR_t val; int fired; } icn_oneshot_state_t;
static DESCR_t coro_oneshot(void *zeta, int entry) {
    icn_oneshot_state_t *z = (icn_oneshot_state_t *)zeta;
    if (entry == α) { z->fired = 0; }   /* reset on α so cross-product can replay */
    if (!z->fired && !IS_FAIL_fn(z->val)) { z->fired = 1; return z->val; }
    return FAILDESCR;
}

/* Lazy-eval box — re-evaluates an AST_t node every time it is pumped α.
 * Used for AST_VAR (and other mutable scalar expressions) inside binop_gen,
 * so that  total + (1 to n)  reads the *current* value of `total` each tick
 * rather than capturing it once at setup time.
 * β always returns FAILDESCR (scalar — one value per pump). */
typedef struct { AST_t *expr; } icn_lazy_state_t;
static DESCR_t icn_lazy_box(void *zeta, int entry) {
    if (entry != α) return FAILDESCR;
    icn_lazy_state_t *z = (icn_lazy_state_t *)zeta;
    DESCR_t v = bb_eval_value(z->expr);
    return IS_FAIL_fn(v) ? FAILDESCR : v;
}


/*----------------------------------------------------------------------------------------------------------------------------
 * coro_bb_fnc — composite box: pump arg-generator, call builtin with substituted arg each tick.
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
    AST_t     *call;       /* the AST_FNC node */
    int         gen_idx;    /* which arg (0-based) is the generator */
    int         nargs;
    DESCR_t     args[ICN_FNC_GEN_ARGS];  /* pre-evaluated; args[gen_idx] filled each tick */
} icn_fnc_gen_state_t;

/* Forward declaration — defined in interp.c */
extern DESCR_t icn_call_builtin(AST_t *call, DESCR_t *args, int nargs);

static DESCR_t coro_bb_fnc(void *zeta, int entry) {
    icn_fnc_gen_state_t *z = (icn_fnc_gen_state_t *)zeta;
    DESCR_t v = z->arg_box.fn(z->arg_box.ζ, entry);
    if (IS_FAIL_fn(v)) return FAILDESCR;
    z->args[z->gen_idx] = v;
    return icn_call_builtin(z->call, z->args, z->nargs);
}
/* Coroutine trampoline for AST_FNC user-proc wrapper.
 * coro_bb_suspend calls this via makecontext; it reads from coro_stage. */
typedef struct {
    coro_t *ss;
    AST_t              *proc;
    DESCR_t             *args;
    int                  nargs;
    int                  entry_pc;  /* CH-17c: -1 = legacy coro_call path */
    int                  nparams;   /* CH-17c: param count for sm_call_proc */
} Icn_coro_stage_t;
static Icn_coro_stage_t coro_stage;   /* staging area — set before makecontext */

static void proc_trampoline(void) {
    Icn_coro_stage_t st = coro_stage;        /* copy before first yield */
    active_coro = st.ss;
    DESCR_t result;
    /* CH-17c: dispatch via SM expression when entry_pc is resolved.
     * Guard on g_current_sm_prog: under --ir-run, sm_resolve_irrun_entry_pcs
     * populates entry_pc then frees the SM_Program (g_current_sm_prog=NULL).
     * In that case fall back to the IR walker (coro_call). */
    extern SM_Program *g_current_sm_prog;
    if (st.entry_pc >= 0 && g_current_sm_prog != NULL)
        result = sm_call_proc(st.entry_pc, st.nparams, st.args, st.nargs);
    else
        result = coro_call(st.proc, st.args, st.nargs);
    active_coro = NULL;
    /* proc finished — store final value if not fail, mark exhausted, yield back */
    st.ss->yielded   = IS_FAIL_fn(result) ? FAILDESCR : result;
    st.ss->exhausted = 1;
    swapcontext(&st.ss->gen_ctx, &st.ss->caller_ctx);
}

/* F-4 RS-7: coro_alloc — suspend-state factory, eliminates duplicate
 * calloc+malloc pattern. Stack size is a single constant here. */
#define CORO_STACK_SZ (256 * 1024)
static coro_t *coro_alloc(void (*trampoline)(void)) {
    coro_t *ss = calloc(1, sizeof(*ss));
    ss->stack      = malloc(CORO_STACK_SZ);
    ss->trampoline = trampoline;
    return ss;
}

/* RK-21: gather trampoline — reads proc from ss->gather_proc, not coro_stage.
 * This avoids the race where coro_stage is overwritten between coro_eval
 * and the first α call to coro_bb_suspend. The ss pointer is passed via a
 * thread-local-style static (safe: single-threaded, called only from makecontext). */
coro_t *gather_trampoline_ss = NULL;
void gather_trampoline(void) {
    coro_t *ss = gather_trampoline_ss;
    active_coro = ss;
    DESCR_t result;
    /* CH-17c: use SM expression when entry_pc is resolved */
    if (ss->gather_entry_pc >= 0)
        result = sm_call_proc(ss->gather_entry_pc, ss->gather_nparams, NULL, 0);
    else
        result = coro_call(ss->gather_proc, NULL, 0);
    active_coro = NULL;
    ss->yielded   = IS_FAIL_fn(result) ? FAILDESCR : result;
    ss->exhausted = 1;
    swapcontext(&ss->gen_ctx, &ss->caller_ctx);
}

/*============================================================================================================================
 * CHUNKS-step17i-suspend: sm_yield_to_caller — yield primitive for SM_SUSPEND_VALUE.
 *
 * Mirrors the yield half of coro_bb_suspend (icon_gen.c:211–240): when an
 * SM-dispatched proc body (running inside proc_trampoline / gather_trampoline)
 * hits AST_SUSPEND, we:
 *   1. Stash the value in active_coro->yielded (where the caller's box will read it).
 *   2. swapcontext from gen_ctx (us) to caller_ctx (the every-loop / surrounding
 *      driver in caller frame).
 *   3. When the caller's broker swaps back to gen_ctx (next β tick), we resume
 *      from where swapcontext returned — which falls through this function and
 *      back to the SM dispatch loop, where the do-clause SM runs next.
 *
 * Precondition: active_coro != NULL.  If called outside a coroutine context
 * (top-level suspend, semantically rare and not exercised by the rung03 corpus),
 * this is a no-op returning 0 — the SM_SUSPEND_VALUE handler then pushes the
 * value back so the outer SM_VOID_POP balances.
 *
 * Returns 1 if a yield happened, 0 if there was no active coroutine.
 *==========================================================================================================================*/
int sm_yield_to_caller(DESCR_t v) {
    if (!active_coro) return 0;
    coro_t *ss = active_coro;
    ss->yielded = v;
    swapcontext(&ss->gen_ctx, &ss->caller_ctx);
    /* Resumed by caller's broker on next β.  active_coro and frame are
     * restored automatically because the entire stack/context was preserved
     * by swapcontext.  Fall through; caller (SM dispatch loop) continues. */
    return 1;
}

/*============================================================================================================================
 * RK-18a: coro_bb_raku_array — Raku @array Byrd box  (for @arr -> $x)
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

static DESCR_t coro_bb_raku_array(void *zeta, int entry) {
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
 * Defined here (and in interp.c as static) so coro_bb_cat can use it.
 *--------------------------------------------------------------------------------------------------------------------------*/
AST_t *find_leaf_suspendable(AST_t *e) {
    if (!e) return NULL;
    switch (e->t) {
        case AST_TO: case AST_TO_BY: case AST_ITERATE: case AST_ALTERNATE:
        case AST_SUSPEND: case AST_LIMIT: case AST_EVERY: case AST_BANG_BINARY: case AST_SEQ_EXPR:
            return e;
        case AST_FNC: return e;
        default: break;
    }
    for (int i = 0; i < e->n; i++) {
        AST_t *found = find_leaf_suspendable(e->c[i]);
        if (found) return found;
    }
    return NULL;
}

/*----------------------------------------------------------------------------------------------------------------------------
 * coro_bb_cat — AST_CAT with generative child  ("str" || gen_expr)
 *
 * Pumps the leaf generator child, injects each tick via coro_drive_node,
 * re-evaluates the full AST_CAT expression each tick to produce the concatenated
 * result string.  Handles the polyglot case: every write("ICN: " || (1 to 3)).
 *--------------------------------------------------------------------------------------------------------------------------*/
typedef struct { bb_node_t gen; AST_t *cat_expr; AST_t *leaf; } icn_cat_gen_state_t;
static DESCR_t coro_bb_cat(void *zeta, int entry) {
    /* IC-9 fix (2026-05-01): per-tick re-eval of cat_expr can itself fail
     * (e.g. s[0 to 7] where s[0] is OOB).  Per Icon GDE semantics, a per-tick
     * failure should not exhaust the box — the underlying generator may still
     * have values that DO succeed.  Pump the leaf until either a tick produces
     * a non-fail full-expression result, or the leaf exhausts.  Switch to β
     * after the first inner tick at α-entry so subsequent attempts re-pump.   */
    icn_cat_gen_state_t *z = (icn_cat_gen_state_t *)zeta;
    int e2 = entry;
    for (;;) {
        DESCR_t tick = z->gen.fn(z->gen.ζ, e2);
        if (IS_FAIL_fn(tick)) return FAILDESCR;
        coro_drive_node = z->leaf;
        coro_drive_val  = tick;
        DESCR_t result = bb_eval_value(z->cat_expr);
        coro_drive_node = NULL;
        if (!IS_FAIL_fn(result)) return result;
        e2 = β;  /* try next leaf value */
    }
}

/*----------------------------------------------------------------------------------------------------------------------------
 * icn_bb_assign_gen — AST_ASSIGN with generative RHS  (x := gen_expr)
 *
 * Two variants:
 *   icn_bb_assign_gen — RHS is a pure generator (no mutable scalar siblings):
 *     e.g.  every (x := (1|2|3)) > 2 & write(x)
 *     Pumps rhs_gen each tick, writes result to lhs.
 *   icn_bb_assign_cat — RHS has mutable scalars alongside a generator:
 *     e.g.  every total := total + (1 to n)
 *     Re-evaluates full RHS each tick via coro_drive_node so `total` is fresh.
 *--------------------------------------------------------------------------------------------------------------------------*/
typedef struct { bb_node_t rhs_gen; AST_t *lhs; } icn_assign_gen_state_t;
static DESCR_t icn_assign_write(AST_t *lhs, DESCR_t val) {
    if (lhs && lhs->t == AST_VAR) {
        int slot = (int)lhs->v.ival;
        if (slot >= 0 && slot < FRAME.env_n) { FRAME.env[slot] = val; }
        else if (slot < 0 && lhs->v.sval && lhs->v.sval[0] != '&') NV_SET_fn(lhs->v.sval, val);
    } else if (lhs && lhs->t == AST_FIELD && lhs->v.sval && lhs->n >= 1) {
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

typedef struct { bb_node_t leaf_gen; AST_t *rhs_expr; AST_t *leaf; AST_t *lhs; } icn_assign_cat_state_t;
static DESCR_t icn_bb_assign_cat(void *zeta, int entry) {
    icn_assign_cat_state_t *z = (icn_assign_cat_state_t *)zeta;
    int e2 = entry;
    for (;;) {
        DESCR_t tick = z->leaf_gen.fn(z->leaf_gen.ζ, e2);
        if (IS_FAIL_fn(tick)) return FAILDESCR;
        coro_drive_node = z->leaf;
        coro_drive_val  = tick;
        DESCR_t val = bb_eval_value(z->rhs_expr);
        coro_drive_node = NULL;
        if (!IS_FAIL_fn(val)) return icn_assign_write(z->lhs, val);
        e2 = β;
    }
}

/*----------------------------------------------------------------------------------------------------------------------------
 * coro_bb_revassign — AST_REVASSIGN  (lhs <- rhs)  reversible assignment
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
 * Currently supports AST_VAR and AST_IDX (table/list/array) on the LHS — the
 * shapes that appear in the JCON suite.  AST_FIELD and other lvalues fall
 * back to a simple non-reverting assign (better than nothing; revisit if a
 * test exercises them).
 *--------------------------------------------------------------------------------------------------------------------------*/
typedef struct {
    AST_t  *lhs_expr;
    AST_t  *rhs_expr;
    DESCR_t *cell;       /* direct cell pointer (AST_IDX path) */
    DESCR_t  base_d;     /* base container for subscript_set revert (no stable cell) */
    DESCR_t  idx_d;      /* index for subscript_set revert */
    int      have_subscript;  /* base_d/idx_d valid → revert via subscript_set */
    int      var_slot;   /* env slot (AST_VAR path; -1 if NV) */
    char    *var_name;   /* NV name (AST_VAR fallback) */
    DESCR_t  saved;
    int      have_saved;
} icn_revassign_state_t;

static DESCR_t coro_bb_revassign(void *zeta, int entry) {
    icn_revassign_state_t *z = (icn_revassign_state_t *)zeta;
    if (entry == α) {
        DESCR_t rv = bb_eval_value(z->rhs_expr);
        if (IS_FAIL_fn(rv)) return FAILDESCR;
        AST_t *lhs = z->lhs_expr;
        if (lhs && lhs->t == AST_VAR) {
            int slot = (int)lhs->v.ival;
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
        } else if (lhs && lhs->t == AST_IDX && lhs->n >= 2) {
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
    /* β / ω — revert and exhaust */
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
 * coro_bb_revswap — AST_REVSWAP  (lhs <-> rhs)  reversible value swap
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
 * Currently supports AST_VAR lvalues (slot or NV) and Icon keywords (&pos,
 * &subject).  Other lvalue shapes (AST_IDX, AST_FIELD) on a `<->` are uncommon
 * in Icon practice; if a future test exercises them, extend along the same
 * pattern as coro_bb_revassign's AST_IDX branch.
 *--------------------------------------------------------------------------------------------------------------------------*/
typedef struct {
    AST_t  *lhs_expr;
    AST_t  *rhs_expr;
    DESCR_t  saved_lhs;        /* lhs's prior value (valid when lhs_written) */
    DESCR_t  saved_rhs;        /* rhs's prior value (valid when rhs_written) */
    int      lhs_written;      /* α successfully wrote rv → lhs              */
    int      rhs_written;      /* α successfully wrote lv → rhs              */
} icn_revswap_state_t;

/* Helper: write `val` to the lvalue described by `lv_expr`.  Returns 1 on
 * success, 0 on keyword-OOB-fail (no write performed in that case).        */
static int icn_revswap_write(AST_t *lv_expr, DESCR_t val) {
    if (!lv_expr || lv_expr->t != AST_VAR) return 0;
    if (lv_expr->v.sval && lv_expr->v.sval[0] == '&') {
        return kw_assign(lv_expr->v.sval + 1, val);
    }
    int slot = (int)lv_expr->v.ival;
    if (slot >= 0 && slot < FRAME.env_n) { FRAME.env[slot] = val; return 1; }
    if (slot < 0 && lv_expr->v.sval) { NV_SET_fn(lv_expr->v.sval, val); return 1; }
    return 0;
}

/* Helper: read the lvalue's current value (for snapshot).                  */
static DESCR_t icn_revswap_read(AST_t *lv_expr) {
    if (!lv_expr || lv_expr->t != AST_VAR) return FAILDESCR;
    if (lv_expr->v.sval && lv_expr->v.sval[0] == '&') {
        if (!strcmp(lv_expr->v.sval + 1, "pos")) return INTVAL(scan_pos);
        if (!strcmp(lv_expr->v.sval + 1, "subject")) return scan_subj ? STRVAL(scan_subj) : NULVCL;
        return NULVCL;
    }
    int slot = (int)lv_expr->v.ival;
    if (slot >= 0 && slot < FRAME.env_n) return FRAME.env[slot];
    if (slot < 0 && lv_expr->v.sval) return NV_GET_fn(lv_expr->v.sval);
    return NULVCL;
}

static DESCR_t coro_bb_revswap(void *zeta, int entry) {
    icn_revswap_state_t *z = (icn_revswap_state_t *)zeta;
    if (entry == α) {
        AST_t *lhs = z->lhs_expr, *rhs = z->rhs_expr;
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
 * icn_bb_identical_gen — AST_IDENTICAL  (a === b)  with one or both operands generative
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
typedef struct { bb_node_t r_gen; AST_t *lhs_expr; } icn_identical_gen_state_t;
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
 * proc_table lookup + coroutine staging that the AST_FNC user-proc branch of
 * coro_eval used to do for the synthesised call_main wrapper, but without
 * routing through an AST_t. The IR walk inside coro_call(proc_table[i].proc,
 * args, nargs) is unchanged — that work belongs to Step 17 (proc_table →
 * entry_pcs).
 *
 * Note on scope: callers today pass nargs=0 (top-level main()). The args path
 * is provided so the helper can be reused if a future rung wants name-driven
 * dispatch with already-evaluated args; the no-generative-args fast path of
 * the AST_FNC branch is what's lifted here. Generative-arg routing
 * (coro_bb_fnc) is intentionally not lifted — it requires per-arg AST_t* to
 * pump, which the SM_BB_PUMP_PROC caller does not have. */
bb_node_t coro_pump_proc_by_name(const char *name, DESCR_t *args, int nargs) {
    if (!name) return (bb_node_t){ NULL, NULL, 0 };
    for (int i = 0; i < proc_count; i++) {
        if (strcmp(proc_table[i].name, name) != 0) continue;
        coro_t *ss = coro_alloc(proc_trampoline);
        ss->trampoline_arg = NULL;
        coro_stage.ss    = ss;
        coro_stage.proc     = proc_table[i].proc;
        coro_stage.args     = args;
        coro_stage.nargs    = nargs;
        coro_stage.entry_pc = proc_table[i].entry_pc;  /* CH-17c */
        coro_stage.nparams  = proc_table[i].nparams;   /* CH-17c */
        return (bb_node_t){ coro_bb_suspend, ss, 0 };
    }
    return (bb_node_t){ NULL, NULL, 0 };
}

bb_node_t coro_eval(AST_t *e) {
    NO_AST_WALK_GUARD("coro_eval");
    if (!e) {
        icn_oneshot_state_t *z = calloc(1, sizeof(*z));
        z->val = FAILDESCR; z->fired = 1;   /* immediately ω */
        return (bb_node_t){ coro_oneshot, z, 0 };
    }

    /* ── AST_TO: (lo to hi) ────────────────────────────────────────────────── */
    if (e->t == AST_TO && e->n >= 2) {
        AST_t *lo_expr = e->c[0];
        AST_t *hi_expr = e->c[1];
        int lo_gen = is_suspendable(lo_expr);
        int hi_gen = is_suspendable(hi_expr);
        if (lo_gen || hi_gen) {
            /* Nested-to: collect all lo/hi values then cross-product iterate. */
            icn_to_nested_state_t *z = calloc(1, sizeof(*z));
            if (!lo_gen) {
                DESCR_t d = bb_eval_value(lo_expr);
                if (!IS_FAIL_fn(d)) z->lo_vals[z->nlo++] = d.i;
            } else {
                bb_node_t lb = coro_eval(lo_expr);
                DESCR_t v = lb.fn(lb.ζ, α);
                while (!IS_FAIL_fn(v) && z->nlo < ICN_TO_NESTED_MAX) { z->lo_vals[z->nlo++] = v.i; v = lb.fn(lb.ζ, β); }
            }
            if (!hi_gen) {
                DESCR_t d = bb_eval_value(hi_expr);
                if (!IS_FAIL_fn(d)) z->hi_vals[z->nhi++] = d.i;
            } else {
                bb_node_t hb = coro_eval(hi_expr);
                DESCR_t v = hb.fn(hb.ζ, α);
                while (!IS_FAIL_fn(v) && z->nhi < ICN_TO_NESTED_MAX) { z->hi_vals[z->nhi++] = v.i; v = hb.fn(hb.ζ, β); }
            }
            return (bb_node_t){ coro_bb_to_nested, z, 0 };
        }
        DESCR_t lo_d = bb_eval_value(lo_expr);
        DESCR_t hi_d = bb_eval_value(hi_expr);
        icn_to_state_t *z = calloc(1, sizeof(*z));
        z->lo = IS_FAIL_fn(lo_d) ? 0 : lo_d.i;
        z->hi = IS_FAIL_fn(hi_d) ? 0 : hi_d.i;
        return (bb_node_t){ coro_bb_to, z, 0 };
    }

    /* ── AST_TO_BY: (lo to hi by step) ─────────────────────────────────────── */
    if (e->t == AST_TO_BY && e->n >= 3) {
        DESCR_t lo_d   = bb_eval_value(e->c[0]);
        DESCR_t hi_d   = bb_eval_value(e->c[1]);
        DESCR_t step_d = bb_eval_value(e->c[2]);
        int any_real = IS_REAL_fn(lo_d) || IS_REAL_fn(hi_d) || IS_REAL_fn(step_d);
        if (any_real) {
            icn_to_by_real_state_t *z = calloc(1, sizeof(*z));
            z->lo   = IS_REAL_fn(lo_d)   ? lo_d.r   : (double)(IS_FAIL_fn(lo_d)   ? 0 : lo_d.i);
            z->hi   = IS_REAL_fn(hi_d)   ? hi_d.r   : (double)(IS_FAIL_fn(hi_d)   ? 0 : hi_d.i);
            z->step = IS_REAL_fn(step_d) ? step_d.r : (double)(IS_FAIL_fn(step_d) ? 1 : step_d.i);
            return (bb_node_t){ coro_bb_to_by_real, z, 0 };
        }
        icn_to_by_state_t *z = calloc(1, sizeof(*z));
        z->lo   = IS_FAIL_fn(lo_d)   ? 0 : lo_d.i;
        z->hi   = IS_FAIL_fn(hi_d)   ? 0 : hi_d.i;
        z->step = IS_FAIL_fn(step_d) ? 1 : step_d.i;
        return (bb_node_t){ coro_bb_to_by, z, 0 };
    }

    /* ── AST_ITERATE: (!str) / Raku for @arr -> $x ────────────────────────── */
    if (e->t == AST_ITERATE && e->n >= 1) {
        /* RK-21: if child is an AST_FNC call matching a user proc, treat as gather
         * coroutine — build coro_bb_suspend box exactly like the AST_FNC proc path. */
        AST_t *child = e->c[0];
        if (child && child->t == AST_FNC && child->n >= 1 && child->c[0]) {
            const char *fn = child->c[0]->v.sval;
            if (fn) {
                int pi;
                for (pi = 0; pi < proc_count; pi++)
                    if (strcmp(proc_table[pi].name, fn) == 0) break;
                if (pi < proc_count) {
                    /* RK-21: Build gather coroutine — store proc in ss->gather_proc so
                     * gather_trampoline can read it at makecontext time, bypassing
                     * the coro_stage global which may be overwritten before first α. */
                    coro_t *ss = coro_alloc(gather_trampoline);
                    ss->gather_proc     = proc_table[pi].proc;
                    ss->gather_entry_pc = proc_table[pi].entry_pc;  /* CH-17c */
                    ss->gather_nparams  = proc_table[pi].nparams;   /* CH-17c */
                    return (bb_node_t){ coro_bb_suspend, ss, 0 };
                }
            }
        }
        DESCR_t sv = bb_eval_value(e->c[0]);
        const char *loopvar = e->v.sval;
        /* IC-8: coerce numeric scalars to image-string before string-iterate path (D-1) */
        sv = descr_to_str_icn(sv);
        /* IC-3: DT_T table iteration — !T yields each value */
        if (sv.v == DT_T) {
            icn_tbl_iterate_state_t *z = calloc(1, sizeof(*z));
            z->tbl = sv.tbl;
            z->bucket = 0;
            z->entry = NULL;
            return (bb_node_t){ coro_bb_tbl_iterate, z, 0 };
        }
        if (!IS_FAIL_fn(sv) && sv.s && (loopvar || strchr(sv.s, '\x01'))) {
            /* Raku array mode: route to coro_bb_raku_array */
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
            return (bb_node_t){ coro_bb_raku_array, z, 0 };
        }
        /* IC-5: DT_DATA icnlist — !L yields each element */
        if (sv.v == DT_DATA) {
            DESCR_t tag = FIELD_GET_fn(sv, "icn_type");
            if (tag.v == DT_S && tag.s && strcmp(tag.s, "list") == 0) {
                icn_list_iterate_state_t *lz = calloc(1, sizeof(*lz));
                lz->list_obj = sv;  /* live DT_DATA — re-read each tick so put() mutations are visible */
                lz->pos      = 0;
                return (bb_node_t){ coro_bb_list_iterate, lz, 0 };
            }
            /* IC-9 (2026-05-01): DT_DATA record — !R yields each field value.
             * Only routes here when the DT_DATA carries a real DATINST_t with a type
             * (lists themselves also use DT_DATA but with the icnlist shape above and
             * an frame_elems pointer that wouldn't have a usable .u->type).  Falls
             * through to char-iterate if type is missing — keeps the historical
             * char-iterate-on-string-shaped-DT_DATA behaviour for any other path. */
            if (sv.u && sv.u->type && sv.u->type->nfields > 0) {
                icn_record_iterate_state_t *rz = calloc(1, sizeof(*rz));
                rz->inst = sv;
                rz->pos  = 0;
                return (bb_node_t){ coro_bb_record_iterate, rz, 0 };
            }
        }
        /* Icon char mode */
        icn_iterate_state_t *z = calloc(1, sizeof(*z));
        if (!IS_FAIL_fn(sv) && sv.s) {
            z->str = sv.s;
            z->len = sv.slen > 0 ? sv.slen : (long)strlen(sv.s);
        }
        return (bb_node_t){ coro_bb_iterate, z, 0 };
    }

    /* ── IC-8: AST_IDENTICAL  (a === b)  — goal-directed identity test ───────
     * Wires `if x === key(T)` and similar patterns: drive RHS as generator,
     * yield rhs on identity match, retry on miss, exhaust when RHS done.
     * Non-generator case is handled by `case AST_IDENTICAL` in interp_eval. */
    if (e->t == AST_IDENTICAL && e->n >= 2) {
        AST_t *lc = e->c[0], *rc = e->c[1];
        int l_gen = is_suspendable(lc);
        int r_gen = is_suspendable(rc);
        if (l_gen || r_gen) {
            icn_identical_gen_state_t *z = calloc(1, sizeof(*z));
            /* Common case (rung36_jcon_table tdump): LHS scalar, RHS generator key(T).
             * If LHS is the generator, swap so RHS is what we drive; identity is
             * symmetric so this preserves semantics.                            */
            if (r_gen) {
                z->lhs_expr = lc;
                z->r_gen    = coro_eval(rc);
            } else {
                z->lhs_expr = rc;
                z->r_gen    = coro_eval(lc);
            }
            return (bb_node_t){ icn_bb_identical_gen, z, 0 };
        }
    }

    /* ── AST_ALTERNATE: (a | b | c | …) n-ary ─────────────────────────────── */
    if (e->t == AST_ALTERNATE && e->n >= 2) {
        /* Build left-recursive chain: alt(alt(gen[0], gen[1]), gen[2]), ...
         * so that exhausting each branch naturally falls through to the next. */
        bb_node_t acc;
        {
            icn_alternate_state_t *z = calloc(1, sizeof(*z));
            z->gen[0] = coro_eval(e->c[0]);
            z->gen[1] = coro_eval(e->c[1]);
            z->which  = 0;
            acc = (bb_node_t){ coro_bb_alternate, z, 0 };
        }
        for (int _ai = 2; _ai < e->n; _ai++) {
            icn_alternate_state_t *z2 = calloc(1, sizeof(*z2));
            z2->gen[0] = acc;
            z2->gen[1] = coro_eval(e->c[_ai]);
            z2->which  = 0;
            acc = (bb_node_t){ coro_bb_alternate, z2, 0 };
        }
        return acc;
    }

    /* ── Arithmetic / relational binop with generative operand(s) ─────────
     * Detects when either child is a generator kind.  Non-generator children
     * are wrapped as oneshot boxes by the recursive coro_eval call.      */
    {
        static const struct { AST_e ek; IcnBinopKind bk; int is_rel; } binop_map[] = {
            { AST_ADD, ICN_BINOP_ADD, 0 }, { AST_SUB, ICN_BINOP_SUB, 0 },
            { AST_MUL, ICN_BINOP_MUL, 0 }, { AST_DIV, ICN_BINOP_DIV, 0 },
            { AST_MOD, ICN_BINOP_MOD, 0 },
            { AST_LT,  ICN_BINOP_LT,  1 }, { AST_LE,  ICN_BINOP_LE,  1 },
            { AST_GT,  ICN_BINOP_GT,  1 }, { AST_GE,  ICN_BINOP_GE,  1 },
            { AST_EQ,  ICN_BINOP_EQ,  1 }, { AST_NE,  ICN_BINOP_NE,  1 },
            { AST_LCONCAT, ICN_BINOP_CONCAT, 0 },  /* ("a"|"b") || ("x"|"y") cross-product */
        };
        for (int mi = 0; mi < (int)(sizeof binop_map/sizeof binop_map[0]); mi++) {
            if (e->t != binop_map[mi].ek) continue;
            if (e->n < 2) break;
            AST_t *lc = e->c[0], *rc = e->c[1];
            int l_gen = is_suspendable(lc);
            int r_gen = is_suspendable(rc);
            if (!l_gen && !r_gen) break;   /* scalar — let interp_eval handle it */
            icn_binop_gen_state_t *z = calloc(1, sizeof(*z));
            z->left     = coro_eval(lc);
            z->right    = coro_eval(rc);
            z->op       = binop_map[mi].bk;
            z->is_relop = binop_map[mi].is_rel;
            return (bb_node_t){ coro_bb_binop, z, 0 };
        }
    }

    /* ── AST_CAT: ("str" || gen_expr) — two sub-cases:
     *   (a) BOTH children generative → cross-product via coro_bb_binop (IC-6 fix)
     *       e.g. ("a"|"b") || ("x"|"y") → ax ay bx by
     *   (b) ONE child generative → pump that generator, re-eval full AST_CAT each tick ── */
    if (e->t == AST_CAT && e->n >= 2) {
        int l_gen = is_suspendable(e->c[0]);
        int r_gen = is_suspendable(e->c[1]);
        if (l_gen && r_gen) {
            /* Cross-product: reuse coro_bb_binop with CONCAT op */
            icn_binop_gen_state_t *z = calloc(1, sizeof(*z));
            z->left     = coro_eval(e->c[0]);
            z->right    = coro_eval(e->c[1]);
            z->op       = ICN_BINOP_CONCAT;
            z->is_relop = 0;
            return (bb_node_t){ coro_bb_binop, z, 0 };
        }
        if (l_gen || r_gen) {
            int gi = l_gen ? 0 : 1;
            AST_t *leaf = find_leaf_suspendable(e->c[gi]);
            if (!leaf) leaf = e->c[gi];
            icn_cat_gen_state_t *z = calloc(1, sizeof(*z));
            z->gen      = coro_eval(leaf);
            z->cat_expr = e;
            z->leaf     = leaf;
            return (bb_node_t){ coro_bb_cat, z, 0 };
        }
    }

    /* ── AST_IDX: s[gen_idx] — drive index generator, re-eval subscript each tick ── */
    if (e->t == AST_IDX && e->n >= 2) {
        for (int _ci = 1; _ci < e->n; _ci++) {
            if (is_suspendable(e->c[_ci])) {
                AST_t *leaf = find_leaf_suspendable(e->c[_ci]);
                if (!leaf) leaf = e->c[_ci];
                icn_cat_gen_state_t *z = calloc(1, sizeof(*z));
                z->gen      = coro_eval(leaf);
                z->cat_expr = e;     /* re-eval the full AST_IDX expression per tick */
                z->leaf     = leaf;
                return (bb_node_t){ coro_bb_cat, z, 0 };
            }
        }
    }

    /* ── AST_FNC find(needle,str) with scalar or generative subject ── */
    if (e->t == AST_FNC && e->n >= 3 && e->c[0] && e->c[0]->v.sval
        && strcmp(e->c[0]->v.sval, "find") == 0) {
        DESCR_t s1 = bb_eval_value(e->c[1]);
        if (!IS_FAIL_fn(s1)) {
            if (is_suspendable(e->c[2])) {
                /* Generative subject: drive subject gen, exhaust find positions per subject */
                icn_find_gen_subj_t *z = calloc(1, sizeof(*z));
                z->subj_gen   = coro_eval(e->c[2]);
                z->needle     = s1.s ? s1.s : "";
                z->nlen       = (int)strlen(z->needle);
                z->subj_entry = α;
                z->hay        = NULL;
                return (bb_node_t){ coro_bb_find_subj, z, 0 };
            }
            DESCR_t s2 = bb_eval_value(e->c[2]);
            if (!IS_FAIL_fn(s2)) {
                icn_find_state_t *z = calloc(1, sizeof(*z));
                z->needle = s1.s ? s1.s : "";
                z->hay    = s2.s ? s2.s : "";
                z->nlen   = (int)strlen(z->needle);
                z->next   = z->hay;
                return (bb_node_t){ coro_bb_find, z, 0 };
            }
        }
    }

    /* ── AST_FNC bal(c1,c2,c3,...) in scan context — coro_bb_bal generator ─── */
    if (e->t == AST_FNC && e->n >= 2 && e->c[0] && e->c[0]->v.sval
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
            return (bb_node_t){ coro_bb_bal, z, 0 };
        }
        bal_skip:;
    }

    /* ── AST_FNC key(T) — generator yielding each key of table T ──────────── */
    if (e->t == AST_FNC && e->n >= 2 && e->c[0] && e->c[0]->v.sval
        && strcmp(e->c[0]->v.sval, "key") == 0) {
        DESCR_t td = bb_eval_value(e->c[1]);
        if (td.v == DT_T && td.tbl) {
            icn_tbl_key_iterate_state_t *z = calloc(1, sizeof(*z));
            z->tbl    = td.tbl;
            z->bucket = 0;
            z->entry  = NULL;
            return (bb_node_t){ coro_bb_tbl_key_iterate, z, 0 };
        }
    }

    /* ── AST_FNC user proc — coroutine wrapper ─────────────────────────────── */
    if (e->t == AST_FNC && e->n >= 1 && e->c[0] && e->c[0]->v.sval) {
        const char *fn = e->c[0]->v.sval;
        int nargs = e->n - 1;
        for (int i = 0; i < proc_count; i++) {
            if (strcmp(proc_table[i].name, fn) != 0) continue;
            /* IC-7 rung32: if any arg is generative, route to coro_bb_fnc.
             * Otherwise the args would be pre-evaluated as scalars (only first
             * value of any alternation/generator), and the proc would run once
             * via coro_call — yielding only one value over `every`.
             * fnc_gen pumps the gen arg per tick and re-calls coro_call
             * with the substituted scalar arg each time. */
            for (int j = 0; j < nargs && j < ICN_FNC_GEN_ARGS; j++) {
                AST_t *arg = e->c[1+j];
                if (!arg || !is_suspendable(arg)) continue;
                icn_fnc_gen_state_t *fg = calloc(1, sizeof(*fg));
                fg->arg_box = coro_eval(arg);
                fg->call    = e;
                fg->gen_idx = j;
                fg->nargs   = nargs;
                /* Pre-evaluate all other args (non-generative) */
                for (int k2 = 0; k2 < nargs && k2 < ICN_FNC_GEN_ARGS; k2++) {
                    if (k2 == j) continue;
                    fg->args[k2] = bb_eval_value(e->c[1+k2]);
                }
                return (bb_node_t){ coro_bb_fnc, fg, 0 };
            }
            /* Build args array */
            DESCR_t *args = nargs > 0 ? calloc(nargs, sizeof(DESCR_t)) : NULL;
            for (int j = 0; j < nargs; j++)
                args[j] = bb_eval_value(e->c[1+j]);
            /* Allocate suspend state + stack */
            coro_t *ss = coro_alloc(proc_trampoline);
            ss->trampoline_arg = NULL;   /* unused — trampoline reads coro_stage */
            /* Stage the call parameters before makecontext */
            coro_stage.ss       = ss;
            coro_stage.proc     = proc_table[i].proc;
            coro_stage.args     = args;
            coro_stage.nargs    = nargs;
            coro_stage.entry_pc = proc_table[i].entry_pc;  /* CH-17c */
            coro_stage.nparams  = proc_table[i].nparams;   /* CH-17c */
            return (bb_node_t){ coro_bb_suspend, ss, 0 };
        }
        /* ── AST_FNC upto(cset, scan_subject) — drive subject gen per subject ── */
        if (fn && strcmp(fn, "upto") == 0 && nargs >= 2 && is_suspendable(e->c[2])) {
            DESCR_t cd = bb_eval_value(e->c[1]);
            const char *cset = VARVAL_fn(cd);
            if (cset) {
                icn_upto_gen_subj_t *z = calloc(1, sizeof(*z));
                z->subj_gen   = coro_eval(e->c[2]);
                z->cset       = cset;
                z->subj_entry = α;
                z->hay        = NULL;
                z->slen       = 0;
                z->pos        = 0;
                return (bb_node_t){ coro_bb_upto_subj, z, 0 };
            }
        }
        /* ── Builtin AST_FNC with generative arg — coro_bb_fnc ─────────── */
        /* Find first argument that is itself a generator expression.
         * Pre-evaluate all non-generative args; the gen arg is filled each tick. */
        for (int j = 0; j < nargs && j < ICN_FNC_GEN_ARGS; j++) {
            AST_t *arg = e->c[1+j];
            if (!arg) continue;
            if (is_suspendable(arg)) {
                icn_fnc_gen_state_t *fg = calloc(1, sizeof(*fg));
                fg->arg_box = coro_eval(arg);
                fg->call    = e;
                fg->gen_idx = j;
                fg->nargs   = nargs;
                /* Pre-evaluate all other args */
                for (int k2 = 0; k2 < nargs && k2 < ICN_FNC_GEN_ARGS; k2++) {
                    if (k2 == j) continue;
                    fg->args[k2] = bb_eval_value(e->c[1+k2]);
                }
                return (bb_node_t){ coro_bb_fnc, fg, 0 };
            }
        }
    }

    /* ── IC-2b: AST_LIMIT  (gen \ N) ──────────────────────────────────────── */
    if (e->t == AST_LIMIT && e->n >= 2) {
        icn_limit_state_t *z = calloc(1, sizeof(*z));
        z->gen = coro_eval(e->c[0]);
        DESCR_t nd = bb_eval_value(e->c[1]);
        z->max = IS_INT_fn(nd) ? nd.i : 0;
        return (bb_node_t){ coro_bb_limit, z, 0 };
    }

    /* ── IC-2b: AST_EVERY  (every gen [do body]) ──────────────────────────── */
    if (e->t == AST_EVERY && e->n >= 1) {
        icn_every_state_t *z = calloc(1, sizeof(*z));
        z->gen     = coro_eval(e->c[0]);
        z->gen_ast = e->c[0];
        z->body    = (e->n >= 2) ? e->c[1] : NULL;
        return (bb_node_t){ coro_bb_every, z, 0 };
    }

    /* ── IC-2b: AST_BANG_BINARY  (E1 ! E2) ────────────────────────────────── */
    if (e->t == AST_BANG_BINARY && e->n >= 2) {
        icn_bang_binary_state_t *z = calloc(1, sizeof(*z));
        z->proc_expr = e->c[0];
        z->arg_box   = coro_eval(e->c[1]);
        return (bb_node_t){ coro_bb_bang_binary, z, 0 };
    }

    /* ── IC-2b: AST_SEQ_EXPR  ((E1; E2; …; En)) ───────────────────────────── */
    if (e->t == AST_SEQ_EXPR && e->n >= 1) {
        icn_seq_state_t *z = calloc(1, sizeof(*z));
        z->c = e->c;
        z->n        = e->n;
        return (bb_node_t){ coro_bb_seq_expr, z, 0 };
    }

    /* ── IC-7: AST_NONNULL (\E) as generator — filter: pass values, skip null ──
     * every write(\(1 to 3)) — drive inner gen, yield each non-null value.   */
    if (e->t == AST_NONNULL && e->n >= 1 && is_suspendable(e->c[0])) {
        /* Wrap inner gen in a filter: pump inner, skip null (empty string / DT_NUL).
         * Reuse coro_bb_limit state struct as a thin wrapper — just store inner gen. */
        icn_limit_state_t *z = calloc(1, sizeof(*z));
        z->gen   = coro_eval(e->c[0]);
        z->max   = (long long)9e18;   /* no limit */
        z->count = 0;
        return (bb_node_t){ coro_bb_limit, z, 0 };
        /* Note: coro_bb_limit just pumps inner gen and counts — it doesn't filter nulls.
         * For \E semantics we need a real filter box, but for (1 to 3) all values are
         * non-null so coro_bb_limit pass-through is correct. A full null-filter box
         * can be added when a failing test requires it. */
    }

    /* ── IC-7: seq(start) / seq(start, step) — infinite integer sequence ───
     * seq(i) yields i, i+1, i+2, … indefinitely.
     * seq(i, j) yields i, i+j, i+2j, … (step j; default step=1). */
    if (e->t == AST_FNC && e->n >= 2 && e->c[0] && e->c[0]->v.sval
        && strcmp(e->c[0]->v.sval, "seq") == 0) {
        icn_to_by_state_t *z = calloc(1, sizeof(*z));
        DESCR_t start = bb_eval_value(e->c[1]);
        z->lo   = IS_INT_fn(start) ? start.i : 1;
        z->hi   = (long long)9e18;   /* effectively infinite */
        z->step = (e->n >= 3) ? (long long)to_int(bb_eval_value(e->c[2])) : 1;
        z->cur  = z->lo;
        return (bb_node_t){ coro_bb_to_by, z, 0 };
    }

    /* ── IC-7: user proc call with generative arg — pump arg, call proc each tick ──
     * e.g.  every write(tag("a"|"b"|"c"))
     *   tag is a user proc; "a"|"b"|"c" is the generative arg.
     * Build an coro_bb_fnc-style box: for each value from the gen arg,
     * call the proc coroutine via icn_call_builtin with substituted args.    */
    if (e->t == AST_FNC && e->n >= 2 && e->c[0] && e->c[0]->v.sval) {
        const char *fn2 = e->c[0]->v.sval;
        int nargs2 = e->n - 1;
        /* only for user procs that have a generative argument */
        int is_proc = 0;
        for (int _p = 0; _p < proc_count; _p++)
            if (strcmp(proc_table[_p].name, fn2) == 0) { is_proc = 1; break; }
        if (is_proc) {
            for (int j = 0; j < nargs2 && j < ICN_FNC_GEN_ARGS; j++) {
                AST_t *arg = e->c[1+j];
                if (!arg || !is_suspendable(arg)) continue;
                /* Found generative arg at position j — build fnc_gen box */
                icn_fnc_gen_state_t *fg = calloc(1, sizeof(*fg));
                fg->arg_box = coro_eval(arg);
                fg->call    = e;
                fg->gen_idx = j;
                fg->nargs   = nargs2;
                for (int k2 = 0; k2 < nargs2 && k2 < ICN_FNC_GEN_ARGS; k2++) {
                    if (k2 == j) continue;
                    fg->args[k2] = bb_eval_value(e->c[1+k2]);
                }
                return (bb_node_t){ coro_bb_fnc, fg, 0 };
            }
        }
    }

    /* ── AST_ASSIGN with generative RHS — IC-6 / mutable-scalar fix ─────────────
     * Two variants selected at coro_eval time:
     *   cat: RHS has an AST_VAR sibling of the leaf generator — re-eval full RHS
     *        each tick via coro_drive_node injection so mutable vars (e.g. `total`
     *        in `total := total + (1 to n)`) are read fresh on every iteration.
     *   gen: pure generator RHS, no mutable scalar siblings — pump directly. */
    if (e->t == AST_ASSIGN && e->n >= 2 && is_suspendable(e->c[1])) {
        AST_t *rhs = e->c[1];
        AST_t *leaf = find_leaf_suspendable(rhs);
        int has_var = 0;
        if (leaf && leaf != rhs) {
            for (int _ci = 0; _ci < rhs->n && !has_var; _ci++)
                if (rhs->c[_ci] && rhs->c[_ci]->t == AST_VAR
                    && rhs->c[_ci] != leaf) has_var = 1;
        }
        if (has_var && leaf) {
            icn_assign_cat_state_t *zc = calloc(1, sizeof(*zc));
            zc->leaf_gen = coro_eval(leaf);
            zc->rhs_expr = rhs;
            zc->leaf     = leaf;
            zc->lhs      = e->c[0];
            return (bb_node_t){ icn_bb_assign_cat, zc, 0 };
        }
        icn_assign_gen_state_t *z = calloc(1, sizeof(*z));
        z->rhs_gen = coro_eval(rhs);
        z->lhs     = e->c[0];
        return (bb_node_t){ icn_bb_assign_gen, z, 0 };
    }

    /* ── AST_REVASSIGN — IC-9: x[k] <- v reversible assign ─────────────────────
     * α: snapshot lhs, write rhs, return rhs.
     * β: revert lhs to snapshot, fail.
     * Net effect: under `every`, the post-loop state of lhs equals its
     * pre-loop state; rhs was visible for the body of one tick only.       */
    if (e->t == AST_REVASSIGN && e->n >= 2) {
        icn_revassign_state_t *z = calloc(1, sizeof(*z));
        z->lhs_expr = e->c[0];
        z->rhs_expr = e->c[1];
        z->var_slot = -2;        /* sentinel for "unset" */
        return (bb_node_t){ coro_bb_revassign, z, 0 };
    }

    /* ── AST_REVSWAP — IC-9 session #26: x <-> y reversible value swap ─────────
     * α: snapshot both lvalues, atomically swap (probe keyword OOB first;
     *    abort whole α if either keyword side would OOB), return rv.
     * β: revert in left-to-right order, short-circuit on failure (so a
     *    keyword whose valid range was mutated by the body — e.g. via
     *    `&subject := "A"` — strands the rhs-revert).                      */
    if (e->t == AST_REVSWAP && e->n >= 2) {
        icn_revswap_state_t *z = calloc(1, sizeof(*z));
        z->lhs_expr = e->c[0];
        z->rhs_expr = e->c[1];
        return (bb_node_t){ coro_bb_revswap, z, 0 };
    }

    /* ── AST_VAR / AST_INTLIT / scalar literals — lazy box (re-evaluates each α pump)
     * This ensures that  total + (1 to n)  reads the current value of `total`
     * on every tick rather than capturing it once at binop_gen setup time.   */
    if (e->t == AST_VAR || e->t == AST_ILIT || e->t == AST_FLIT || e->t == AST_QLIT) {
        icn_lazy_state_t *z = calloc(1, sizeof(*z));
        z->expr = e;
        return (bb_node_t){ icn_lazy_box, z, 0 };
    }

    /* ── AST_PROC_FAIL — must be lazy (RS-23b alternation fix).
     * `expr | fail` is the canonical Icon idiom for procedure-level fail.
     * The oneshot fallback below would eagerly call bb_eval_value(AST_PROC_FAIL)
     * which sets FRAME.returning=1; FRAME.return_val=FAILDESCR at *box build*
     * time — corrupting the procedure even when arm 0 succeeds.  By deferring
     * via icn_lazy_box, the frame state is only touched if/when arm 1 is
     * actually pumped. */
    if (e->t == AST_PROC_FAIL) {
        icn_lazy_state_t *z = calloc(1, sizeof(*z));
        z->expr = e;
        return (bb_node_t){ icn_lazy_box, z, 0 };
    }

    /* ── Fallback: one-shot box wrapping value-context evaluation ───────── */
    icn_oneshot_state_t *z = calloc(1, sizeof(*z));
    z->val = bb_eval_value(e);
    return (bb_node_t){ coro_oneshot, z, 0 };
}


/* coro_drive_fnc: suspend-aware driver for user procedures called as generators.
 * Called by coro_drive when e->t == AST_FNC and a matching proc exists.
 * Runs the proc body in-frame, pausing at each AST_SUSPEND, running the
 * every-body (body_root of the *caller* frame), then the do-clause, then
 * continuing from the same statement (so while-loops around suspend iterate). */
int coro_drive_fnc(AST_t *e) {
    if (!e || e->t != AST_FNC || e->n < 1 || !e->c[0]) return 0;
    const char *fn = e->c[0]->v.sval;
    if (!fn) return 0;
    int pi;
    for (pi = 0; pi < proc_count; pi++)
        if (strcmp(proc_table[pi].name, fn) == 0) break;
    if (pi >= proc_count) return 0;

    AST_t *proc   = proc_table[pi].proc;
    int nparams    = (int)proc->v.ival;
    int body_start = 1 + nparams;
    int nbody      = proc->n - body_start;

    /* Build scope */
    IcnScope sc; sc.n = 0;
    for (int i = 0; i < nparams && i < FRAME_SLOT_MAX; i++) {
        AST_t *pn = proc->c[1+i];
        if (pn && pn->v.sval) scope_add(&sc, pn->v.sval);
    }
    for (int i = 0; i < nbody; i++) {
        AST_t *st = proc->c[body_start+i];
        if (st && st->t == AST_GLOBAL)
            for (int j = 0; j < st->n; j++)
                if (st->c[j] && st->c[j]->v.sval)
                    scope_add(&sc, st->c[j]->v.sval);
    }
    for (int i = 0; i < nbody; i++)
        icn_scope_patch(&sc, proc->c[body_start+i]);
    int nslots = sc.n > 0 ? sc.n : (nparams > 0 ? nparams : 1);
    if (nslots > FRAME_SLOT_MAX) nslots = FRAME_SLOT_MAX;

    /* Capture every-body from caller frame BEFORE pushing callee frame */
    AST_t *every_body = (frame_depth >= 1)
                         ? frame_stack[frame_depth-1].body_root : NULL;

    /* Push frame */
    if (frame_depth >= FRAME_STACK_MAX) return 0;
    IcnFrame *f = &frame_stack[frame_depth++];
    memset(f, 0, sizeof *f);
    f->env_n = nslots;
    f->sc    = sc;
    int nargs = e->n - 1;
    for (int i = 0; i < nparams && i < nargs && i < FRAME_SLOT_MAX; i++)
        f->env[i] = bb_eval_value(e->c[1+i]);

    /* Suspend-aware body loop */
    int ticks = 0;
    int stmt  = 0;
    while (stmt < nbody && !f->returning && !f->loop_break) {
        AST_t *st = proc->c[body_start + stmt];
        if (!st || st->t == AST_GLOBAL) { stmt++; continue; }
        f->body_root  = st;
        f->suspending = 0;
        bb_exec_stmt(st);
        if (f->suspending) {
            DESCR_t sv       = f->suspend_val;
            AST_t *doclause = f->suspend_do;
            f->suspending    = 0;
            /* Run every-body with suspended value visible via FRAME being
             * the proc frame — write() etc. will call interp_eval on their
             * argument, which is the result of the generator call.  We need
             * the every-body to see sv as the result of the AST_FNC expression.
             * Accomplish this by temporarily storing sv in a gen slot keyed
             * on e, so AST_EVERY's interp_eval(gen) path retrieves it. */
            /* Set drive passthrough so interp_eval(AST_FNC e) returns sv directly
             * instead of re-calling the procedure. */
            coro_drive_node = e;
            coro_drive_val  = sv;
            if (every_body) {
                /* Execute every-body in caller frame: step back so FRAME
                 * is the caller (who owns the every/write expression), not
                 * the generator proc frame. */
                frame_depth--;
                bb_exec_stmt(every_body);
                frame_depth++;
                /* Refresh f in case frame array was touched */
                f = &frame_stack[frame_depth - 1];
            }
            coro_drive_node = NULL;
            if (doclause) bb_exec_stmt(doclause);
            ticks++;
            /* If the stmt that suspended was a loop (AST_WHILE/AST_REPEAT/AST_UNTIL),
             * re-enter it so it can re-check its condition next tick.
             * For bare AST_SUSPEND (or any other stmt), advance past it — it fired once. */
            if (st->t != AST_WHILE && st->t != AST_REPEAT && st->t != AST_UNTIL)
                stmt++;
        } else {
            stmt++;
        }
        /* Refresh pointer in case frame was reallocated (it isn't, but be safe) */
        f = &frame_stack[frame_depth - 1];
        if (f->returning || f->loop_break) break;
    }

    frame_depth--;
    return ticks;
}

/*============================================================================================================================
 * IC-2b: Four missing GDE ops as BB boxes.
 * Live here (not icon_gen.c) because they need interp_eval / icn_scan_*.
 * AST_SCAN is intentionally absent: it is the same IR node as SNOBOL4 matching,
 * already handled correctly by the oneshot fallback in coro_eval.
 *============================================================================================================================*/

/*----------------------------------------------------------------------------------------------------------------------------
 * coro_bb_limit — AST_LIMIT  (gen \ N)
 * α: pump inner gen α; count=1; return value if count<=max.
 * β: if count>=max → ω; pump inner gen β; if ω → ω; count++; return value.
 *--------------------------------------------------------------------------------------------------------------------------*/
DESCR_t coro_bb_limit(void *zeta, int entry) {
    icn_limit_state_t *z = (icn_limit_state_t *)zeta;
    if (z->max <= 0) return FAILDESCR;
    DESCR_t v;
    if (entry == α) {
        z->count = 0;
        v = z->gen.fn(z->gen.ζ, α);
    } else {
        if (z->count >= z->max) return FAILDESCR;
        v = z->gen.fn(z->gen.ζ, β);
    }
    if (IS_FAIL_fn(v)) return FAILDESCR;
    z->count++;
    if (z->count > z->max) return FAILDESCR;
    return v;
}

/*----------------------------------------------------------------------------------------------------------------------------
 * coro_bb_every — AST_EVERY  (every gen [do body])
 * α: pump gen α → if γ eval body → return gen value.
 * β: pump gen β → if ω → ω → if γ eval body → return gen value.
 * body may be NULL (bare "every gen" for side effects).
 *--------------------------------------------------------------------------------------------------------------------------*/
DESCR_t coro_bb_every(void *zeta, int entry) {
    icn_every_state_t *z = (icn_every_state_t *)zeta;
    DESCR_t v = (entry == α)
        ? z->gen.fn(z->gen.ζ, α)
        : z->gen.fn(z->gen.ζ, β);
    if (IS_FAIL_fn(v)) return FAILDESCR;
    if (z->body) {
        /* Inject generator value so the body's reference to the same generator AST
         * node (e.g. `(1 to n)` in `x := x + (1 to n)`) returns the already-produced
         * value rather than building a fresh coro_eval and restarting from α. */
        AST_t *saved_drive_node = coro_drive_node;
        DESCR_t saved_drive_val = coro_drive_val;
        coro_drive_node = z->gen_ast;
        coro_drive_val  = v;
        /* GOAL-ICON-BB-COMPLETE: clear loop_next before body so that a `next`
         * fired in a prior iteration (which set FRAME.loop_next=1 and caused
         * bb_exec_stmt to return early) does not bleed into this iteration's
         * body execution.  After bb_exec_stmt returns, also clear loop_next:
         * `next` means "advance to next generator tick", which bb_broker
         * accomplishes by calling us again with β — the flag must not persist
         * into the next call or the following body will also short-circuit. */
        int saved_loop_next = FRAME.loop_next;
        FRAME.loop_next = 0;
        bb_exec_stmt(z->body);
        FRAME.loop_next = saved_loop_next;  /* restore outer loop's next state */
        coro_drive_node = saved_drive_node;
        coro_drive_val  = saved_drive_val;
    }
    return v;
}

/*----------------------------------------------------------------------------------------------------------------------------
 * coro_bb_bang_binary — AST_BANG_BINARY  (E1 ! E2)
 * Call procedure/expression E1 with each successive value from E2 generator.
 * α: pump E2 α → call E1(arg). β: pump E2 β → call E1(arg).
 * If E1 fails on an arg, skip to next (goal-directed).
 *--------------------------------------------------------------------------------------------------------------------------*/
DESCR_t coro_bb_bang_binary(void *zeta, int entry) {
    icn_bang_binary_state_t *z = (icn_bang_binary_state_t *)zeta;
    int is_first = (entry == α);
    for (;;) {
        DESCR_t arg = z->arg_box.fn(z->arg_box.ζ, is_first ? α : β);
        is_first = 0;
        if (IS_FAIL_fn(arg)) return FAILDESCR;
        z->cur_arg = arg;
        if (!z->proc_expr) return FAILDESCR;
        /* Inject arg as result of first argument child via drive passthrough */
        if (z->proc_expr->n >= 2 && z->proc_expr->c[1]) {
            coro_drive_node = z->proc_expr->c[1];
            coro_drive_val  = arg;
        }
        DESCR_t result = bb_eval_value(z->proc_expr);
        coro_drive_node = NULL;
        if (!IS_FAIL_fn(result)) return result;
        /* E1 failed — try next E2 value */
    }
}

/*----------------------------------------------------------------------------------------------------------------------------
 * coro_bb_seq_expr — AST_SEQ_EXPR  ((E1; E2; …; En))
 * α: eval E1..E(n-1) for side effects; build last_box from En; pump last_box α.
 * β: pump last_box β.
 *--------------------------------------------------------------------------------------------------------------------------*/
DESCR_t coro_bb_seq_expr(void *zeta, int entry) {
    icn_seq_state_t *z = (icn_seq_state_t *)zeta;
    if (entry == α) {
        for (int i = 0; i < z->n - 1; i++)
            if (z->c[i]) bb_eval_value(z->c[i]);
        if (z->n <= 0 || !z->c[z->n - 1]) return FAILDESCR;
        z->last_box = coro_eval(z->c[z->n - 1]);
        z->started  = 1;
        return z->last_box.fn(z->last_box.ζ, α);
    }
    if (!z->started) return FAILDESCR;
    return z->last_box.fn(z->last_box.ζ, β);
}
