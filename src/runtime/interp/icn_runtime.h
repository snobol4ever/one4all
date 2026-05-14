/*
 * icn_runtime.h — Icon interpreter runtime API
 *
 * FI-4: declarations for all symbols moved from scrip.c to icn_runtime.c.
 * Include this in scrip.c (and anywhere else that needs Icon runtime access).
 *
 * AUTHORS: Lon Jones Cherryholmes · Claude Sonnet 4.6 (FI-4, 2026-04-14)
 */
#ifndef ICN_RUNTIME_H
#define ICN_RUNTIME_H

#include "../ast/ast.h"
#include "../../frontend/snobol4/scrip_cc.h"
#include "../../runtime/x86/bb_broker.h"
#include "../../frontend/icon/icon_gen.h"

/*------------------------------------------------------------------------
 * Constants
 *------------------------------------------------------------------------*/
#define FRAME_SLOT_MAX        64
/* IB-10 post: coro stack size — raised 256KB→1MB to accommodate _usercall_hook's
 * ~400KB -O0 frame.  Defined here (not in icn_runtime.c) so icon_gen.c can
 * use it for the uc_stack.ss_size argument to makecontext. */
#define CORO_STACK_SZ         (1024 * 1024)
#define PROC_TABLE_MAX       256
#define FRAME_DEPTH_MAX         16
#define FRAME_STACK_MAX      256
#define EVERY_GEN_SLOT_MAX    16   /* GOAL-ICON-BB-COMPLETE: max nested every-gen coroutines per frame */
#define SCAN_STACK_MAX  16
#define GLOBAL_MAX      64

/*------------------------------------------------------------------------
 * Types
 *------------------------------------------------------------------------*/
/* GOAL-ICON-BB-COMPLETE: forward-declare SmGenState (defined in sm_interp.h)
 * so IcnFrame can hold per-call SM generator states for pure-SM every loops. */
struct SmGenState;

/* CH-17a: entry_pc is the SM_Program pc of the proc body's named expression.
 * Populated by sm_resolve_proc_entry_pcs(SM_Program*) after sm_lower runs.
 * -1 means no expression emitted yet (CH-17b will start emitting Icon/Raku proc
 * expressions; until then every entry remains -1 and consumers fall back to the
 * legacy proc-pointer path).  Once CH-17g lands, the proc field is deleted. */
typedef struct { tree_t *node; long cur; const char *sval; } IcnGenEntry_d;

/* IM-10: moved above IcnFrame so IcnFrame.sc can embed IcnScope by value */
typedef struct { const char *name; int slot; } IcnScopeEnt;
typedef struct { IcnScopeEnt e[FRAME_SLOT_MAX]; int n; } IcnScope;

/* CH-17a: entry_pc is the SM_Program pc of the proc body's named expression.
 * CH-17c: nparams cached from proc->v.ival so sm_call_proc can bind args without reading tree_t.
 * CH-17g-proc-locals: lower_sc stores finalized slot map from lower_proc_skeletons so
 *   sm_call_proc can icn_scope_patch() body AST nodes for every-body AST walker. */
typedef struct { const char *name; tree_t *proc; int entry_pc; int nparams; IcnScope lower_sc; } IcnProcEntry;

typedef struct {
    DESCR_t       env[FRAME_SLOT_MAX];
    int           env_n;
    int           returning;
    DESCR_t       return_val;
    IcnGenEntry_d gen[FRAME_DEPTH_MAX];
    int           gen_depth;
    int           loop_break;
    int           loop_next;    /* 1 = `next` requested → skip body remainder, advance loop */
    tree_t       *body_root;
    /* IM-10: slot→name map, copied from the scope built in coro_call.
     * Allows sync_monitor to name local variables in snapshots. */
    IcnScope      sc;
    /* suspend coroutine state: set by TT_SUSPEND, cleared by coro_drive */
    int           suspending;   /* 1 = procedure is suspending a value     */
    DESCR_t       suspend_val;  /* the value being suspended               */
    tree_t       *suspend_do;   /* do-clause to run on resumption, or NULL */
    /* GOAL-ICON-BB-COMPLETE: per-call SM generator states for pure-SM every loops.
     * Indexed by slot id baked at lower-time (SM_GEN_TICK a[1].i).
     * NULL = not yet started; non-NULL = active or exhausted SmGenState. */
    struct SmGenState *every_gen[EVERY_GEN_SLOT_MAX];
} IcnFrame;

/*------------------------------------------------------------------------
 * Globals (defined in icn_runtime.c)
 *------------------------------------------------------------------------*/
extern IcnProcEntry proc_table[PROC_TABLE_MAX];
extern int          proc_count;
extern int          g_lang;        /* 0=SNOBOL4 1=Icon */
extern tree_t      *g_icn_root;

extern IcnFrame     frame_stack[FRAME_STACK_MAX];
extern int          frame_depth;
#define FRAME (frame_stack[frame_depth - 1])

extern const char  *scan_subj;
/* IJ-CORO: active_coro/coro_t removed — swapcontext machinery deleted */
extern int          scan_pos;
typedef struct { const char *subj; int pos; } ScanEntry;
extern ScanEntry scan_stack[SCAN_STACK_MAX];
extern int          scan_depth;

extern const char  *global_names[GLOBAL_MAX];
extern int          global_count;

/*------------------------------------------------------------------------
 * Functions (defined in icn_runtime.c)
 *------------------------------------------------------------------------*/
void    frame_push(tree_t *n, long v, const char *sv);
void    frame_pop(void);
int     icn_frame_lookup(tree_t *n, long *out);
int     icn_frame_lookup_sv(tree_t *n, long *out, const char **sv);
int     frame_active(tree_t *n);

int     is_global(const char *name);
void    global_register(const char *name);


extern DESCR_t  coro_drive_val;    /* the suspended value to return          */

int     scope_add(IcnScope *sc, const char *name);
int     scope_get(IcnScope *sc, const char *name);
void    icn_scope_patch(IcnScope *sc, tree_t *e);

/* CH-17c: SM-dispatch entry for proc bodies lowered into named expressions. */
DESCR_t sm_call_proc(int entry_pc, int nparams, DESCR_t *args, int nargs);
/* CH-17g-call-sites: dispatch helper for proc_table[pi].  Routes through
 * sm_call_proc when entry_pc is resolved, else falls back to coro_call.
 * Replaces ~8 hand-coded `coro_call(proc_table[pi].proc, ...)` call sites
 * across icn_value.c, raku_builtins.c, interp_eval.c, interp_hooks.c,
 * interp_exec.c, polyglot.c — symmetrical to CH-17c's trampoline flip. */
DESCR_t proc_table_call(int pi, DESCR_t *args, int nargs);
bb_node_t icn_bb_build(tree_t *e);
/* CHUNKS-step12: build a bb_node_t for a user proc identified by name + args,
 * skipping the synthesised TT_FNC + icn_bb_build routing. The proc's IR body is
 * still walked inside coro_call (Step 17 territory); this entry point eliminates
 * the wrapper-level tree_t synthesis and the icn_bb_build(TT_FNC) traversal that
 * surrounded it. Returns a (NULL, NULL, 0) bb_node_t if the named proc is not
 * found in proc_table — caller should treat that as last_ok=0. */
bb_node_t icn_bb_pump_proc_by_name(const char *name, DESCR_t *args, int nargs);
int       is_suspendable(tree_t *e);
void      icn_init_save_frame(void);  /* IC-5: save initial-block statics before frame pop */

/* IC-8: real-to-string formatter (defined in driver/interp.c). Used by !N real iteration. */
const char *real_str(double r, char *buf, int bufsz);

/* IC-8: deep-identity test for Icon `===` (defined in icn_runtime.c). */
int icn_descr_identical(DESCR_t a, DESCR_t b);

/* RS-22f-cset: cset arithmetic helpers (defined in frontend/icon/icon_runtime.c).
 * Each takes NUL-terminated char strings representing the cset and returns a
 * pointer into a static arena (thread-unsafe but consistent with Icon single-thread). */
const char *icn_cset_complement(const char *cs);
const char *icn_cset_union(const char *a, const char *b);
const char *icn_cset_diff(const char *a, const char *b);
const char *icn_cset_inter(const char *a, const char *b);
const char *icn_cset_canonical(const char *cs);

/* IC-9 (session #26): Icon-keyword assign / probe (defined in driver/interp.c).
 * Used by icn_bb_revswap to perform atomic swap-with-revert on &pos / &subject.
 * kw_assign returns 1 on success, 0 on OOB-fail (and writes nothing on fail).
 * icn_kw_can_assign answers the same question without writing. */
int kw_assign(const char *kw, DESCR_t val);
int icn_kw_can_assign(const char *kw, DESCR_t val);

/* IJ-11: central Icon keyword read — returns correct DESCR_t for any &kw name
 * (without the leading '&').  Returns FAILDESCR for unknown / generative keywords. */
DESCR_t icn_kw_read(const char *kw);
/* IJ-11: if ptr is a registered keyword-cset pointer, return its "&name"; else NULL. */
const char *icn_kw_cset_name(const char *ptr);
/* IJ-11: returns stored length for keyword csets (handles NUL-inclusive like &ascii). -1 = not a kw cset. */
int icn_kw_cset_len(const char *ptr);

/* RS-23c: exported so icn_value.c / icn_stmt.c can use it in TT_EVERY handling. */
tree_t *find_leaf_suspendable(tree_t *e);

/* A0 — SCRIP_NO_AST_WALK tripwire.  Set to 1 while SM dispatch is active;
 * guards in icn_bb_build / interp_eval / etc. abort if the env var is set.
 * g_ast_pump_active: re-entrant counter for intentional icn_bb_build bridges
 * (SM_BB_PUMP_EVERY and similar — exempts these from the tripwire). */
extern int g_sm_dispatch_active;
extern int g_ast_pump_active;

#define NO_AST_WALK_GUARD(fn_name) \
    do { if (g_sm_dispatch_active && !g_ast_pump_active && getenv("SCRIP_NO_AST_WALK")) { \
        fprintf(stderr, "FATAL: " fn_name " reached from SM dispatch\n"); \
        abort(); \
    } } while (0)

#endif /* ICN_RUNTIME_H */
