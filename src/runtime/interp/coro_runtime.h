/*
 * coro_runtime.h — Icon interpreter runtime API
 *
 * FI-4: declarations for all symbols moved from scrip.c to coro_runtime.c.
 * Include this in scrip.c (and anywhere else that needs Icon runtime access).
 *
 * AUTHORS: Lon Jones Cherryholmes · Claude Sonnet 4.6 (FI-4, 2026-04-14)
 */
#ifndef CORO_RUNTIME_H
#define CORO_RUNTIME_H

#include "../ast/ast.h"
#include "../../frontend/snobol4/scrip_cc.h"
#include "../../runtime/x86/bb_broker.h"
#include "../../frontend/icon/icon_gen.h"

/*------------------------------------------------------------------------
 * Constants
 *------------------------------------------------------------------------*/
#define FRAME_SLOT_MAX        64
#define PROC_TABLE_MAX       256
#define FRAME_DEPTH_MAX         16
#define FRAME_STACK_MAX      256
#define SCAN_STACK_MAX  16
#define GLOBAL_MAX      64

/*------------------------------------------------------------------------
 * Types
 *------------------------------------------------------------------------*/
/* CH-17a: entry_pc is the SM_Program pc of the proc body's named expression.
 * Populated by sm_resolve_proc_entry_pcs(SM_Program*) after sm_lower runs.
 * -1 means no expression emitted yet (CH-17b will start emitting Icon/Raku proc
 * expressions; until then every entry remains -1 and consumers fall back to the
 * legacy proc-pointer path).  Once CH-17g lands, the proc field is deleted. */
/* CH-17a: entry_pc is the SM_Program pc of the proc body's named expression.
 * CH-17c: nparams cached from proc->ival so sm_call_proc can bind args
 *         without reading the AST_t. */
typedef struct { const char *name; AST_t *proc; int entry_pc; int nparams; } IcnProcEntry;

typedef struct { AST_t *node; long cur; const char *sval; } IcnGenEntry_d;

/* IM-10: moved above IcnFrame so IcnFrame.sc can embed IcnScope by value */
typedef struct { const char *name; int slot; } IcnScopeEnt;
typedef struct { IcnScopeEnt e[FRAME_SLOT_MAX]; int n; } IcnScope;

typedef struct {
    DESCR_t       env[FRAME_SLOT_MAX];
    int           env_n;
    int           returning;
    DESCR_t       return_val;
    IcnGenEntry_d gen[FRAME_DEPTH_MAX];
    int           gen_depth;
    int           loop_break;
    int           loop_next;    /* 1 = `next` requested → skip body remainder, advance loop */
    AST_t       *body_root;
    /* IM-10: slot→name map, copied from the scope built in coro_call.
     * Allows sync_monitor to name local variables in snapshots. */
    IcnScope      sc;
    /* suspend coroutine state: set by AST_SUSPEND, cleared by coro_drive */
    int           suspending;   /* 1 = procedure is suspending a value     */
    DESCR_t       suspend_val;  /* the value being suspended               */
    AST_t       *suspend_do;   /* do-clause to run on resumption, or NULL */
} IcnFrame;

/*------------------------------------------------------------------------
 * Globals (defined in coro_runtime.c)
 *------------------------------------------------------------------------*/
extern IcnProcEntry proc_table[PROC_TABLE_MAX];
extern int          proc_count;
extern int          g_lang;        /* 0=SNOBOL4 1=Icon */
extern AST_t      *g_icn_root;

extern IcnFrame     frame_stack[FRAME_STACK_MAX];
extern int          frame_depth;
#define FRAME (frame_stack[frame_depth - 1])

extern const char  *scan_subj;
extern coro_t *active_coro;
extern int          scan_pos;
typedef struct { const char *subj; int pos; } ScanEntry;
extern ScanEntry scan_stack[SCAN_STACK_MAX];
extern int          scan_depth;

extern const char  *global_names[GLOBAL_MAX];
extern int          global_count;

/*------------------------------------------------------------------------
 * Functions (defined in coro_runtime.c)
 *------------------------------------------------------------------------*/
void    frame_push(AST_t *n, long v, const char *sv);
void    frame_pop(void);
int     icn_frame_lookup(AST_t *n, long *out);
int     icn_frame_lookup_sv(AST_t *n, long *out, const char **sv);
int     frame_active(AST_t *n);

int     is_global(const char *name);
void    global_register(const char *name);

int     coro_drive(AST_t *e);
int     coro_drive_fnc(AST_t *e);   /* suspend-aware driver for user proc generators */

/* Set by coro_drive_fnc while running the every-body with a suspended value.
 * interp_eval(AST_FNC) checks this: if the AST_FNC node matches coro_drive_node,
 * return coro_drive_val directly instead of calling the procedure again. */
extern AST_t  *coro_drive_node;   /* the AST_FNC node currently being driven */
extern DESCR_t  coro_drive_val;    /* the suspended value to return          */

int     scope_add(IcnScope *sc, const char *name);
int     scope_get(IcnScope *sc, const char *name);
void    icn_scope_patch(IcnScope *sc, AST_t *e);

DESCR_t coro_call(AST_t *proc, DESCR_t *args, int nargs);
/* CH-17c: SM-dispatch entry for proc bodies lowered into named expressions. */
DESCR_t sm_call_proc(int entry_pc, int nparams, DESCR_t *args, int nargs);
/* CH-17g-call-sites: dispatch helper for proc_table[pi].  Routes through
 * sm_call_proc when entry_pc is resolved, else falls back to coro_call.
 * Replaces ~8 hand-coded `coro_call(proc_table[pi].proc, ...)` call sites
 * across coro_value.c, raku_builtins.c, interp_eval.c, interp_hooks.c,
 * interp_exec.c, polyglot.c — symmetrical to CH-17c's trampoline flip. */
DESCR_t proc_table_call(int pi, DESCR_t *args, int nargs);
bb_node_t coro_eval(AST_t *e);
/* CHUNKS-step12: build a bb_node_t for a user proc identified by name + args,
 * skipping the synthesised AST_FNC + coro_eval routing. The proc's IR body is
 * still walked inside coro_call (Step 17 territory); this entry point eliminates
 * the wrapper-level AST_t synthesis and the coro_eval(AST_FNC) traversal that
 * surrounded it. Returns a (NULL, NULL, 0) bb_node_t if the named proc is not
 * found in proc_table — caller should treat that as last_ok=0. */
bb_node_t coro_pump_proc_by_name(const char *name, DESCR_t *args, int nargs);
int       is_suspendable(AST_t *e);
void      icn_init_save_frame(void);  /* IC-5: save initial-block statics before frame pop */

/* IC-8: real-to-string formatter (defined in driver/interp.c). Used by !N real iteration. */
const char *real_str(double r, char *buf, int bufsz);

/* IC-8: deep-identity test for Icon `===` (defined in coro_runtime.c). */
int icn_descr_identical(DESCR_t a, DESCR_t b);

/* RS-22f-cset: cset arithmetic helpers (defined in frontend/icon/icon_runtime.c).
 * Each takes NUL-terminated char strings representing the cset and returns a
 * pointer into a static arena (thread-unsafe but consistent with Icon single-thread). */
const char *icn_cset_complement(const char *cs);
const char *icn_cset_union(const char *a, const char *b);
const char *icn_cset_diff(const char *a, const char *b);
const char *icn_cset_inter(const char *a, const char *b);

/* IC-9 (session #26): Icon-keyword assign / probe (defined in driver/interp.c).
 * Used by coro_bb_revswap to perform atomic swap-with-revert on &pos / &subject.
 * kw_assign returns 1 on success, 0 on OOB-fail (and writes nothing on fail).
 * icn_kw_can_assign answers the same question without writing. */
int kw_assign(const char *kw, DESCR_t val);
int icn_kw_can_assign(const char *kw, DESCR_t val);

/* RS-23c: exported so coro_value.c / coro_stmt.c can use it in AST_EVERY handling. */
AST_t *find_leaf_suspendable(AST_t *e);

/* A0 — SCRIP_NO_AST_WALK tripwire.  Set to 1 while SM dispatch is active;
 * guards in coro_eval / interp_eval / etc. abort if the env var is set. */
extern int g_sm_dispatch_active;

#define NO_AST_WALK_GUARD(fn_name) \
    do { if (g_sm_dispatch_active && getenv("SCRIP_NO_AST_WALK")) { \
        fprintf(stderr, "FATAL: " fn_name " reached from SM dispatch\n"); \
        abort(); \
    } } while (0)

#endif /* CORO_RUNTIME_H */
