/*
 * scrip_rt.c — libscrip_rt.so implementation (M-JITEM-X64 / EM-1..EM-7-pre)
 *
 * Authors: Lon Jones Cherryholmes · Claude Sonnet
 * Date: 2026-05-06; EM-7-revert 2026-05-07
 *
 * EM-1: init / finalize skeleton.
 * EM-2: push_int / pop_int / halt / unhandled_op.
 * EM-3: push_str / pop_descr / arith / nv_get(stub) / nv_set(stub) / pop_void.
 * EM-4: last_ok flag.
 * EM-5: push_chunk_descr.
 * EM-6: [REVERTED in EM-7-revert, session #72] The full pattern-builder
 *   ABI (scrip_rt_pat_*, scrip_rt_exec_stmt) and the runtime pat-stack
 *   (g_pat_stack[], g_pat_sp) are removed.  Lon's correction: this
 *   brokered descriptor-tree-then-broker model was the wrong architecture
 *   for emitted code.  See GOAL-MODE4-EMIT.md "Design Discoveries"
 *   section.  EM-7a/b/c will reintroduce pattern emit using the proven
 *   dual-mode bb_emit infrastructure (bb_flat in EMIT_TEXT mode for
 *   invariant sub-trees baked as flat .text chunks; bb_emit BINARY mode
 *   for variant nodes built into bb_pool RX memory at runtime).
 *   The full SNOBOL4 runtime stays linked in (bb_pool, snobol4_pattern,
 *   stmt_exec, etc.) — those objects will be called by EM-7c via the
 *   bb_build_flat / bb_build_binary entries already proven in mode-3.
 * EM-7-pre keepers: scrip_rt_concat / scrip_rt_push_null /
 *   scrip_rt_coerce_num / scrip_rt_call / scrip_rt_do_return.  These are
 *   Phase 1/4/5 concerns, orthogonal to BB / pattern matching.
 *
 * Value type throughout: DESCR_t (snobol4.h / descr.h).
 *
 * State:
 *   g_vstack[]   — DESCR_t value stack (cap = VSTACK_CAP).
 *   g_vtop       — next free slot (0 = empty).
 *   g_halt_rc    — rc from most recent scrip_rt_halt().
 *   g_halt_set   — nonzero once halt has been called.
 *   g_last_ok    — success flag for SM_JUMP_S / SM_JUMP_F.
 */

#include "scrip_rt.h"

/* Full SNOBOL4 runtime headers — .so links all runtime objects -fPIC. */
#include "../../runtime/x86/snobol4.h"
#include "../../runtime/x86/descr.h"
#include "../../runtime/x86/sil_macros.h"   /* EM-7: IS_NAMEPTR / IS_NAMEVAL / NAME_DEREF_PTR */
#include "../../runtime/x86/bb_pool.h"
#include "../../runtime/x86/bb_box.h"       /* EM-7c: bb_box_fn + exec_stmt_blob */
#include "../../runtime/x86/bb_build.h"     /* EM-7c-variant: g_bb_mode + BB_MODE_LIVE */

#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/*==============================================================================
 * Forward declarations — symbols in snobol4.c / stmt_exec.c / snobol4_pattern.c
 *============================================================================*/

extern void    SNO_INIT_fn(void);
extern int     exec_stmt(const char *subj_name, DESCR_t *subj_var,
                         DESCR_t pat, DESCR_t *repl, int has_repl);
extern DESCR_t NV_GET_fn(const char *name);
extern DESCR_t NV_SET_fn(const char *name, DESCR_t val);
extern DESCR_t NAME_fn(const char *varname);
extern char   *VARVAL_fn(DESCR_t v);

extern void    register_fn(const char *name,
                           DESCR_t (*fn)(DESCR_t *, int),
                           int min_args, int max_args);

/* pat_*() constructors — snobol4_pattern.c */
extern DESCR_t pat_lit(const char *s);
extern DESCR_t pat_span(const char *chars);
extern DESCR_t pat_break_(const char *chars);
extern DESCR_t pat_any_cs(const char *chars);
extern DESCR_t pat_notany(const char *chars);
extern DESCR_t pat_len(int64_t n);
extern DESCR_t pat_pos(int64_t n);
extern DESCR_t pat_rpos(int64_t n);
extern DESCR_t pat_tab(int64_t n);
extern DESCR_t pat_rtab(int64_t n);
extern DESCR_t pat_arb(void);
extern DESCR_t pat_arbno(DESCR_t inner);
extern DESCR_t pat_rem(void);
extern DESCR_t pat_fence(void);
extern DESCR_t pat_fence_p(DESCR_t inner);
extern DESCR_t pat_fail(void);
extern DESCR_t pat_abort(void);
extern DESCR_t pat_succeed(void);
extern DESCR_t pat_bal(void);
extern DESCR_t pat_epsilon(void);
extern DESCR_t pat_cat(DESCR_t left, DESCR_t right);
extern DESCR_t pat_alt(DESCR_t left, DESCR_t right);
extern DESCR_t pat_ref(const char *name);
extern DESCR_t pat_assign_imm(DESCR_t child, DESCR_t var);
extern DESCR_t pat_assign_cond(DESCR_t child, DESCR_t var);
extern DESCR_t pat_at_cursor(const char *varname);
extern DESCR_t pat_user_call(const char *name, DESCR_t *args, int nargs);

/* User-call hook for *func() in pattern position (snobol4.c extern) */
extern DESCR_t (*g_user_call_hook)(const char *, DESCR_t *, int);

/*==============================================================================
 * Internal state
 *============================================================================*/

#define VSTACK_CAP   65536  /* EM-7: beauty.sno self-host needs deep stack (256 was insufficient) */

static DESCR_t g_vstack[VSTACK_CAP];
static int     g_vtop    = 0;

/* EM-7c-variant (session #80, 2026-05-07): g_pat_stack[] reintroduced for
 * the variant-pattern path.  The architecture distinction from EM-7-pre
 * (session #71) is *not* the existence of a runtime pat-stack — that was
 * always going to be needed for variant patterns whose tree is built at
 * runtime (the alternative is a per-variant-node bb_pool emit walker, which
 * is the destination architecture but a much larger rung).  The distinction
 * is the *Phase-3 routing*: EM-7-pre routed through bb_broker (descriptor
 * walker) by relying on the default g_bb_mode == BB_MODE_DRIVER; EM-7c-variant
 * sets g_bb_mode = BB_MODE_LIVE in scrip_rt_init so exec_stmt routes through
 * bb_build_flat / bb_build_binary and a direct call to the resulting bb_box_fn,
 * matching the proven mode-3 pipeline that the goal file calls "mode-4's
 * existence proof".
 *
 * Future rung (EM-7c-variant-bb-pool-emit, the architectural ideal): replace
 * the runtime PATND_t build with per-variant-node bb_pool emit driven by an
 * emit-time partition, so invariant subtrees of partly-variant patterns
 * resolve via linker-baked _pat_inv_<pid>_<sid>_α labels rather than being
 * rebuilt into bb_pool at runtime. */
#define PATSTACK_CAP 256
static DESCR_t g_pat_stack[PATSTACK_CAP];
static int     g_pat_sp = 0;

static int     g_halt_rc  = 0;
static int     g_halt_set = 0;
static int     g_last_ok  = 1;  /* default success at process start */

/*==============================================================================
 * Internal stack helpers
 *============================================================================*/

static void vstack_push(DESCR_t d)
{
    if (g_vtop >= VSTACK_CAP) {
        fprintf(stderr, "libscrip_rt: SM value stack overflow (cap=%d).\n", VSTACK_CAP);
        abort();
    }
    g_vstack[g_vtop++] = d;
}

static DESCR_t vstack_pop(void)
{
    if (g_vtop <= 0) {
        fprintf(stderr, "libscrip_rt: SM value stack underflow.\n");
        abort();
    }
    return g_vstack[--g_vtop];
}

/* EM-7c-variant: pat-stack helpers — used by scrip_rt_pat_*() to assemble
 * patterns at runtime from the SM_PAT_* opcode sequence emitted into the
 * mode-4 binary.  See block comment on g_pat_stack[] for the architectural
 * note. */
static void pat_push(DESCR_t d)
{
    if (g_pat_sp >= PATSTACK_CAP) {
        fprintf(stderr, "libscrip_rt: pat-stack overflow (cap=%d).\n", PATSTACK_CAP);
        abort();
    }
    g_pat_stack[g_pat_sp++] = d;
}

static DESCR_t pat_pop_internal(void)
{
    if (g_pat_sp <= 0) {
        fprintf(stderr, "libscrip_rt: pat-stack underflow.\n");
        abort();
    }
    return g_pat_stack[--g_pat_sp];
}

/* EM-7c-variant: vstack coerce helpers — pop TOS as string or int with
 * the coercions the SM_PAT_*-takes-charset-or-int contract expects.
 * Mirror sm_interp.c's pop-then-VARVAL_fn / pop-then-(arg.v==DT_I?...). */
static const char *vstack_pop_str(void)
{
    DESCR_t d = vstack_pop();
    if (d.v == DT_S) return d.s ? d.s : "";
    char *s = VARVAL_fn(d);
    return s ? s : "";
}

static int64_t vstack_pop_int64(void)
{
    DESCR_t d = vstack_pop();
    if (d.v == DT_I) return d.i;
    return to_int(d);
}

/*==============================================================================
 * EM-6 builtin shims — registered with register_fn so exec_stmt can
 * dispatch *IDENT / *DIFFER in pattern position.
 *============================================================================*/

static DESCR_t _rt_IDENT(DESCR_t *a, int n)
{
    /* IDENT(x[,y]): succeed if x == y (or x is null-string when n==1). */
    const char *s1 = (n >= 1 && a[0].v == DT_S) ? (a[0].s ? a[0].s : "") : "";
    const char *s2 = (n >= 2 && a[1].v == DT_S) ? (a[1].s ? a[1].s : "") : "";
    if (n == 1) return (s1[0] == '\0') ? NULVCL : FAILDESCR;
    return (strcmp(s1, s2) == 0) ? a[0] : FAILDESCR;
}

static DESCR_t _rt_DIFFER(DESCR_t *a, int n)
{
    const char *s1 = (n >= 1 && a[0].v == DT_S) ? (a[0].s ? a[0].s : "") : "";
    const char *s2 = (n >= 2 && a[1].v == DT_S) ? (a[1].s ? a[1].s : "") : "";
    if (n == 1) return (s1[0] != '\0') ? a[0] : FAILDESCR;
    return (strcmp(s1, s2) != 0) ? a[0] : FAILDESCR;
}

/*==============================================================================
 * EM-7d-usercall-reentrant: native chunk function pointer registry
 *
 * The emitter walks SM_Program for SM_LABEL instructions with a[0].s set
 * (SNOBOL4 named function entries) and emits a .data table in the .s file.
 * scrip_rt_register_chunks() is called from the emitted main() before
 * scrip_rt_init; it populates g_chunk_reg[] so _rt_usercall can dispatch
 * user-defined SNOBOL4 functions by direct fn(args,nargs) without touching
 * the interpreter call stack.
 *============================================================================*/

#define CHUNK_REG_MAX 256

typedef struct { const char *name; void *fn; } ChunkRegEntry;
static ChunkRegEntry g_chunk_reg[CHUNK_REG_MAX];
static int           g_chunk_reg_count = 0;

void scrip_rt_register_chunks(const rt_chunk_entry *tbl)
{
    if (!tbl) return;
    for (; tbl->name && g_chunk_reg_count < CHUNK_REG_MAX; tbl++) {
        g_chunk_reg[g_chunk_reg_count].name = tbl->name;
        g_chunk_reg[g_chunk_reg_count].fn   = tbl->fn;
        g_chunk_reg_count++;
    }
}

/* EM-7c-capture: patch a heap cap_t's fn pointer to the baked child blob.
 * cap_ptr points to the cap_t; fn field is first (offset 0). */
void scrip_rt_patch_cap_fn(void *cap_ptr, void *child_fn)
{
    if (!cap_ptr || !child_fn) return;
    /* fn is the first field in cap_t — cast and set */
    void **fn_slot = (void **)cap_ptr;
    *fn_slot = child_fn;
}

/* EM-7c-arbno: allocate a fresh arbno_t for a baked ARBNO blob.
 * bb_arbno_new is declared in bb_box.h via the opaque extern in bb_flat.c.
 * Here we call it directly since libscrip_rt.so links bb_boxes.c. */
extern void *bb_arbno_new(void *fn, void *state);  /* arbno_t* opaque */
void scrip_rt_init_arbno(void **slot_ptr, void *child_fn)
{
    if (!slot_ptr || !child_fn) return;
    *slot_ptr = bb_arbno_new(child_fn, NULL);
}

/* Look up a user function by name in the chunk registry.
 * Returns fn pointer or NULL if not found. */
static void *chunk_reg_lookup(const char *name)
{
    if (!name || !*name) return NULL;
    for (int i = 0; i < g_chunk_reg_count; i++) {
        if (strcmp(g_chunk_reg[i].name, name) == 0)
            return g_chunk_reg[i].fn;
    }
    return NULL;
}

/* Call a native chunk (a SNOBOL4 user-defined function body emitted as a
 * sequence of SM ops ending in `ret`) with the given arguments.
 *
 * SNOBOL4 calling convention: formal parameters are bound into the NV table
 * by name before the body executes (body does `SM_PUSH_VAR "N"`, not vstack
 * pop).  We use FUNC_PARAM_fn(name, i) to get the i-th param name, save
 * the old NV value, bind the arg, call the chunk, then restore.
 *
 * The chunk pushes its return value onto the vstack before `ret`; we pop it.
 *
 * NOTE: no call-stack depth tracking, no local-variable isolation — this is
 * a direct-call model.  Recursive SNOBOL4 functions will reuse the same NV
 * slots; the last binding wins.  For beauty.sno's patterns (non-recursive)
 * this is correct.  Full isolation is a follow-up rung. */
static DESCR_t call_native_chunk(const char *fname, void *fn,
                                  DESCR_t *args, int nargs)
{
    /* Bind formal parameters into the NV table, saving old values. */
    DESCR_t saved[32];
    const char *pnames[32];
    int nbound = 0;
    for (int k = 0; k < nargs && k < 32; k++) {
        const char *pname = FUNC_PARAM_fn(fname, k);
        if (!pname || !*pname) break;
        pnames[nbound] = pname;
        saved[nbound]  = NV_GET_fn(pname);
        NV_SET_fn(pname, args[k]);
        nbound++;
    }

    /* Snapshot the value-stack depth so we can restore it: the SNOBOL4
     * user-function calling convention is "value of the function = NV[fname]"
     * (the body executes `fname = expr`, which is SM_STORE_VAR popping TOS
     * into NV[fname]).  The chunk does NOT push its retval onto vstack
     * before `ret` — that's the SM-interp's job (sm_interp.c:1208-1210
     * does `NV_GET_fn(retval_name)` after the chunk returns).  We mirror
     * that here: read NV[fname] for the retval; ignore vstack residue. */
    int saved_vtop = g_vtop;

    /* Call the native chunk.  It runs its SM body and executes `ret`.
     * Calling convention: void(void) at the ABI level — the chunk
     * reads/writes the global vstack and NV table directly. */
    typedef void (*chunk_fn_t)(void);
    chunk_fn_t cfn = (chunk_fn_t)fn;
    cfn();

    /* SNOBOL4 user-function retval convention: read NV[fname].  Mirrors
     * sm_interp.c:1208-1210 user-function branch.  If the body never
     * assigned to fname, NV_GET_fn returns the function's NV slot's
     * default (DT_SNUL) — same behaviour as the interpreter. */
    DESCR_t result = NV_GET_fn(fname ? fname : "");

    /* Restore vstack depth — drop any residue the body pushed and didn't
     * pop.  Mirrors sm_interp.c:1215-1222 which restores caller_sp.  Since
     * we share one global vstack, "restore" = truncate to pre-call depth. */
    g_vtop = saved_vtop;

    /* Restore saved parameter values. */
    for (int k = nbound - 1; k >= 0; k--)
        NV_SET_fn(pnames[k], saved[k]);

    return result;
}

static DESCR_t _rt_usercall(const char *name, DESCR_t *args, int nargs)
{
    /* Dispatch *func() in pattern position (Phase-3 BB match context).
     * EM-7d-usercall-reentrant: look up in the native chunk registry first.
     * If found, call via direct fn pointer — no interpreter call stack needed.
     * Falls back to C-builtin dispatch (register_fn entries) then FAILDESCR. */
    if (!name || !*name) return FAILDESCR;

    /* 1. Native chunk registry (user-defined SNOBOL4 functions emitted as
     *    .LpcN chunks in the .s file). */
    void *fn = chunk_reg_lookup(name);
    if (fn) return call_native_chunk(name, fn, args, nargs);

    /* 2. C builtin registered via register_fn (DT_E fn pointer in NV table). */
    DESCR_t nv = NV_GET_fn(name);
    if (!IS_FAIL_fn(nv) && nv.v == DT_E && nv.ptr) {
        typedef DESCR_t (*cfn_t)(DESCR_t *, int);
        cfn_t cfn = (cfn_t)nv.ptr;
        return cfn(args, nargs);
    }

    /* 3. Not found — pattern match fails (safe: no interpreter re-entry). */
    return FAILDESCR;
}

/*==============================================================================
 * EM-1 entries
 *============================================================================*/

void scrip_rt_init(int argc, char **argv)
{
    (void)argc; (void)argv;

    /* EM-6: bring up the full SNOBOL4 runtime.
     * Mirrors the init sequence in scrip.c's SM-run mode block. */
    bb_pool_init();
    SNO_INIT_fn();

    /* EM-7c-variant: set BB pattern mode to LIVE so exec_stmt's Phase-3
     * routes through bb_build_flat / bb_build_binary -> direct bb_box_fn
     * call, NOT through bb_broker.  This is the routing distinction that
     * separates the corrected mode-4 architecture from EM-7-pre's reverted
     * brokered model.  Mode-3 with --bb-live is described in
     * GOAL-MODE4-EMIT.md as "mode-4's existence proof"; we want emitted
     * binaries to take the same code path. */
    g_bb_mode = BB_MODE_LIVE;

    register_fn("IDENT",  _rt_IDENT,  1, 2);
    register_fn("DIFFER", _rt_DIFFER, 1, 2);

    g_user_call_hook = _rt_usercall;
}

int scrip_rt_finalize(void)
{
    return g_halt_set ? g_halt_rc : 0;
}

/*==============================================================================
 * EM-2 entries
 *============================================================================*/

void scrip_rt_push_int(int64_t v)
{
    vstack_push(INTVAL(v));
}

int64_t scrip_rt_pop_int(void)
{
    DESCR_t d = vstack_pop();
    if (d.v != DT_I) {
        fprintf(stderr,
            "libscrip_rt: scrip_rt_pop_int: TOS is not DT_I (tag=%d).\n", d.v);
        abort();
    }
    return d.i;
}

void scrip_rt_halt(int rc)
{
    g_halt_rc  = rc;
    g_halt_set = 1;
}

void scrip_rt_halt_tos(void)
{
    /* Safe-pop TOS as DT_I exit code; use 0 if TOS is missing or not DT_I.
     * This lets SM_HALT work for both synthetic tests (PUSH_LIT_I N + HALT)
     * and real programs (HALT with non-integer TOS → normal exit rc=0). */
    int rc = 0;
    if (g_vtop > 0) {
        DESCR_t d = g_vstack[g_vtop - 1];
        if (d.v == DT_I) { rc = (int)d.i; g_vtop--; }
        /* Non-DT_I TOS: leave stack intact, rc stays 0 */
    }
    g_halt_rc  = rc;
    g_halt_set = 1;
}

void scrip_rt_unhandled_op(int op)
{
    fprintf(stderr,
        "libscrip_rt: unhandled SM opcode %d reached in emitted code.\n"
        "  (scrip --dump-sm to identify; subsequent EM-N rungs shrink the set)\n",
        op);
    abort();
}

/*==============================================================================
 * EM-3 entries
 *============================================================================*/

void scrip_rt_push_str(const char *s, uint32_t slen)
{
    DESCR_t d;
    d.v    = DT_S;
    d.slen = slen ? slen : (uint32_t)(s ? strlen(s) : 0);
    d.s    = (char *)s;
    vstack_push(d);
}

void scrip_rt_pop_descr(DESCR_t *out)
{
    if (!out) { fprintf(stderr, "libscrip_rt: pop_descr: NULL out ptr.\n"); abort(); }
    *out = vstack_pop();
}

void scrip_rt_arith(int op)
{
    /* op: SM_ADD=17 SM_SUB=18 SM_MUL=19 SM_DIV=20 SM_MOD=22 */
    DESCR_t r = vstack_pop();
    DESCR_t l = vstack_pop();

    int64_t lv = (l.v == DT_I) ? l.i : to_int(l);
    int64_t rv = (r.v == DT_I) ? r.i : to_int(r);
    int64_t result;

    switch (op) {
        case 17: result = lv + rv; break;
        case 18: result = lv - rv; break;
        case 19: result = lv * rv; break;
        case 20:
            if (!rv) { fprintf(stderr, "libscrip_rt: SM_DIV by zero.\n"); abort(); }
            result = lv / rv; break;
        case 22:
            if (!rv) { fprintf(stderr, "libscrip_rt: SM_MOD by zero.\n"); abort(); }
            result = lv % rv; break;
        default:
            fprintf(stderr, "libscrip_rt: scrip_rt_arith: bad op=%d.\n", op);
            abort();
    }
    vstack_push(INTVAL(result));
}

void scrip_rt_nv_get(const char *name)
{
    vstack_push(NV_GET_fn(name ? name : ""));
}

void scrip_rt_nv_set(const char *name)
{
    DESCR_t val = vstack_pop();
    /* Mirror sm_interp.c SM_STORE_VAR: if RHS is DT_FAIL, the statement fails;
     * no assignment occurs and last_ok=0.  This is how LINE = INPUT :F(DONE)
     * detects EOF in mode-4 — scrip_rt_nv_get("INPUT") pushes DT_FAIL, then
     * scrip_rt_nv_set("LINE") propagates the failure to last_ok. */
    if (val.v == DT_FAIL) {
        vstack_push(val);   /* balanced push so subsequent pops don't underflow */
        g_last_ok = 0;
        return;
    }
    NV_SET_fn(name ? name : "", val);
    g_last_ok = 1;
}

void scrip_rt_pop_void(void)
{
    (void)vstack_pop();
}

/*==============================================================================
 * EM-4 entries
 *============================================================================*/

int scrip_rt_last_ok(void)  { return g_last_ok; }
void scrip_rt_set_last_ok(int ok) { g_last_ok = ok ? 1 : 0; }

/*==============================================================================
 * EM-5 entries
 *============================================================================*/

void scrip_rt_push_chunk_descr(int64_t entry_pc, int64_t arity)
{
    /* DT_E chunk descriptor: .i = entry_pc, .slen = arity.
     * Mirrors sm_interp.c's DT_E handling for SM_PUSH_CHUNK. */
    DESCR_t d;
    d.v    = DT_E;
    d.slen = (uint32_t)arity;
    d.i    = entry_pc;
    vstack_push(d);
}

/*==============================================================================
 * EM-7c — pattern match for pre-built BB blobs (mode-4 emit path)
 *
 * The mode-4 emitter bakes invariant pattern sub-trees as flat .text
 * chunks via bb_build_flat_text(), with externally-visible entry
 * symbols `_pat_inv_<id>_α` etc.  At runtime, the emitted binary
 * pushes the subject and replacement on the SM value stack and calls
 * scrip_rt_match_blob(blob_α, sname, has_repl).
 *
 * Stack contract (top-of-stack first, top last popped):
 *   [repl_or_zero]    ← top
 *   [subj_descr]
 *
 * Parameters:
 *   blob_α   — address of `_pat_inv_<id>_α`
 *   subj_name    — subject variable name for write-back, or NULL
 *   has_repl     — 1 if a replacement is present
 *
 * Calls exec_stmt_blob() (declared in bb_box.h, defined in stmt_exec.c)
 * with the popped subject + replacement.  Stores the :S/:F result on
 * the libscrip_rt last-ok flag (so SM_JUMP_S / SM_JUMP_F see it).
 *============================================================================*/

extern void scrip_rt_set_last_ok(int v);   /* defined below */

void scrip_rt_match_blob(void *blob_α,
                         const char *subj_name,
                         int has_repl)
{
    /* Pop replacement (always present — sm_lower emits SM_PUSH_LIT_I 0
     * when has_repl=0 to keep the value-stack shape uniform). */
    DESCR_t repl = vstack_pop();
    DESCR_t subj = vstack_pop();

    bb_box_fn root_fn = (bb_box_fn)blob_α;
    int ok = exec_stmt_blob(subj_name,
                            &subj,
                            root_fn,
                            has_repl ? &repl : NULL,
                            has_repl);
    scrip_rt_set_last_ok(ok);
}

/*==============================================================================
 * EM-7c-variant entries — pattern-construction ABI (session #80, 2026-05-07)
 *
 * Reintroduces the SM_PAT_* runtime ABI that was deleted in EM-7-revert.
 * The architectural distinction from EM-7-pre is *not* the existence of
 * a runtime pat-stack — that's necessary for any path-β style variant
 * pattern build at runtime — but the Phase-3 routing.  EM-7-pre relied
 * on the default g_bb_mode = BB_MODE_DRIVER, routing exec_stmt through
 * bb_broker (the descriptor walker).  This rung sets g_bb_mode =
 * BB_MODE_LIVE in scrip_rt_init so exec_stmt routes through
 * bb_build_flat / bb_build_binary -> direct bb_box_fn call, which is
 * the proven mode-3 pipeline ("mode-4's existence proof" per
 * GOAL-MODE4-EMIT.md).
 *
 * Each scrip_rt_pat_*() mirrors the corresponding case in sm_interp.c's
 * SM_PAT_* dispatcher byte-for-byte.  Args come from the SM value stack
 * (charsets, ints, vars) for kinds whose argument is computed; from the
 * pat-stack (children of CAT/ALT/ARBNO/FENCE1/CAPTURE) for kinds whose
 * argument is itself a pattern; from the function's parameters
 * (literals, var names) for kinds whose argument is compile-time
 * constant.  The result of each is pushed on the pat-stack.
 *
 * SM_PAT_BOXVAL bridges pat-stack -> value-stack; emitted as a single
 * call to scrip_rt_pat_boxval.  Used wherever a pattern is the value
 * of a value-stack expression (e.g., assignment to WPAT in wordcount).
 *
 * SM_EXEC_STMT for variant patterns calls scrip_rt_match_variant, which
 * pops [subj][repl_or_zero] from value-stack, pops the pattern from
 * pat-stack, and calls exec_stmt with all five.  exec_stmt in
 * BB_MODE_LIVE then handles Phases 3-5 with bb_build_flat/binary.
 *============================================================================*/

void scrip_rt_pat_lit(const char *s)
{
    pat_push(pat_lit(s ? s : ""));
}

void scrip_rt_pat_refname(const char *name)
{
    pat_push(pat_ref(name ? name : ""));
}

void scrip_rt_pat_span(void)
{
    const char *cs = vstack_pop_str();
    pat_push(pat_span(cs));
}

void scrip_rt_pat_break(void)
{
    const char *cs = vstack_pop_str();
    pat_push(pat_break_(cs));
}

void scrip_rt_pat_any(void)
{
    const char *cs = vstack_pop_str();
    pat_push(pat_any_cs(cs));
}

void scrip_rt_pat_notany(void)
{
    const char *cs = vstack_pop_str();
    pat_push(pat_notany(cs));
}

void scrip_rt_pat_len(void)   { pat_push(pat_len (vstack_pop_int64())); }
void scrip_rt_pat_pos(void)   { pat_push(pat_pos (vstack_pop_int64())); }
void scrip_rt_pat_rpos(void)  { pat_push(pat_rpos(vstack_pop_int64())); }
void scrip_rt_pat_tab(void)   { pat_push(pat_tab (vstack_pop_int64())); }
void scrip_rt_pat_rtab(void)  { pat_push(pat_rtab(vstack_pop_int64())); }

void scrip_rt_pat_arb(void)     { pat_push(pat_arb());     }
void scrip_rt_pat_rem(void)     { pat_push(pat_rem());     }
void scrip_rt_pat_fence(void)   { pat_push(pat_fence());   }
void scrip_rt_pat_fail(void)    { pat_push(pat_fail());    }
void scrip_rt_pat_abort(void)   { pat_push(pat_abort());   }
void scrip_rt_pat_succeed(void) { pat_push(pat_succeed()); }
void scrip_rt_pat_bal(void)     { pat_push(pat_bal());     }
void scrip_rt_pat_eps(void)     { pat_push(pat_epsilon()); }

void scrip_rt_pat_arbno(void)
{
    DESCR_t inner = pat_pop_internal();
    pat_push(pat_arbno(inner));
}

void scrip_rt_pat_fence1(void)
{
    DESCR_t child = pat_pop_internal();
    pat_push(pat_fence_p(child));
}

void scrip_rt_pat_cat(void)
{
    DESCR_t right = pat_pop_internal();
    DESCR_t left  = pat_pop_internal();
    pat_push(pat_cat(left, right));
}

void scrip_rt_pat_alt(void)
{
    DESCR_t right = pat_pop_internal();
    DESCR_t left  = pat_pop_internal();
    pat_push(pat_alt(left, right));
}

void scrip_rt_pat_deref(void)
{
    /* Mirror sm_interp.c's SM_PAT_DEREF case: pop value-stack TOS,
     * dispatch by tag.  DT_P → already a pattern; DT_S → wrap in literal;
     * else look up by name (deferred ref). */
    DESCR_t v = vstack_pop();
    if (v.v == DT_P) {
        pat_push(v);
    } else if (v.v == DT_S && v.s) {
        pat_push(pat_lit(v.s));
    } else {
        char *name = VARVAL_fn(v);
        pat_push(pat_ref(name ? name : ""));
    }
}

void scrip_rt_pat_capture(const char *varname, int kind)
{
    /* a[0].s = varname, a[1].i = kind (0=cond, 1=imm, 2=cursor) */
    DESCR_t child = pat_pop_internal();
    DESCR_t var   = NAME_fn(varname ? varname : "");
    if (kind == 1)
        pat_push(pat_assign_imm(child, var));
    else if (kind == 2)
        pat_push(pat_cat(child, pat_at_cursor(varname ? varname : "")));
    else
        pat_push(pat_assign_cond(child, var));
}

void scrip_rt_pat_boxval(void)
{
    /* Move pat-stack TOS to value-stack as DT_P. */
    vstack_push(pat_pop_internal());
}

/* SM_PAT_CAPTURE_FN: . *func() / $ *func() — no-args form.
 * a[0].s=fname, a[1].i=is_imm(0=cond/1=imm), a[2].s=namelist (tab-sep, or NULL).
 * Pops child from pat-stack; pushes XCALLCAP node. */
void scrip_rt_pat_capture_fn(const char *fname, int is_imm, const char *namelist)
{
    DESCR_t child = pat_pop_internal();
    if (!fname) fname = "";
    if (namelist && namelist[0]) {
        int nnames = 1;
        for (const char *q = namelist; *q; q++) if (*q == '\t') nnames++;
        char **names = (char **)GC_MALLOC((size_t)nnames * sizeof(char *));
        int ni = 0;
        const char *start = namelist;
        for (const char *q = namelist; ; q++) {
            if (*q == '\t' || *q == '\0') {
                size_t len = (size_t)(q - start);
                char *nm = (char *)GC_MALLOC(len + 1);
                memcpy(nm, start, len);  nm[len] = '\0';
                names[ni++] = nm;
                if (*q == '\0') break;
                start = q + 1;
            }
        }
        pat_push(is_imm
            ? pat_assign_callcap_named_imm(child, fname, NULL, 0, names, nnames)
            : pat_assign_callcap_named    (child, fname, NULL, 0, names, nnames));
    } else {
        pat_push(is_imm
            ? pat_assign_callcap_named_imm(child, fname, NULL, 0, NULL, 0)
            : pat_assign_callcap          (child, fname, NULL, 0));
    }
}

/* SM_PAT_CAPTURE_FN_ARGS: . *func(args) / $ *func(args) — args-on-stack form.
 * a[0].s=fname, a[1].i=is_imm, a[2].i=nargs.
 * Pops nargs from vstack (last-pushed=last arg), pops child from pat-stack. */
void scrip_rt_pat_capture_fn_args(const char *fname, int is_imm, int nargs)
{
    if (!fname) fname = "";
    DESCR_t *argv = nargs > 0
        ? (DESCR_t *)GC_MALLOC((size_t)nargs * sizeof(DESCR_t))
        : NULL;
    for (int i = nargs - 1; i >= 0; i--) argv[i] = vstack_pop();
    DESCR_t child = pat_pop_internal();
    pat_push(is_imm
        ? pat_assign_callcap_named_imm(child, fname, argv, nargs, NULL, 0)
        : pat_assign_callcap          (child, fname, argv, nargs));
}

/* SM_PAT_USERCALL: bare *func() — no-args, no child.
 * a[0].s=fname. Builds XATP deferred-usercall node. */
void scrip_rt_pat_usercall(const char *fname)
{
    if (!fname) fname = "";
    pat_push(pat_user_call(fname, NULL, 0));
}

/* SM_PAT_USERCALL_ARGS: bare *func(args) — args-on-stack form.
 * a[0].s=fname, a[1].i=nargs. Pops nargs from vstack. */
void scrip_rt_pat_usercall_args(const char *fname, int nargs)
{
    if (!fname) fname = "";
    DESCR_t *argv = nargs > 0
        ? (DESCR_t *)GC_MALLOC((size_t)nargs * sizeof(DESCR_t))
        : NULL;
    for (int i = nargs - 1; i >= 0; i--) argv[i] = vstack_pop();
    pat_push(pat_user_call(fname, argv, nargs));
}

/*==============================================================================
 * scrip_rt_match_variant — SM_EXEC_STMT for variant patterns
 *
 * Stack contract (top-of-stack last popped):
 *   pat-stack:  [pattern_descr]  ← pat-stack TOS
 *   vstack:     [subj_descr]
 *               [repl_or_zero]   ← vstack TOS
 *
 * The replacement slot is ALWAYS pushed (as INTVAL(0) when has_repl=0,
 * matching sm_lower's emission convention) so we always pop two from
 * the vstack regardless of has_repl.
 *
 * Arguments:
 *   subj_name — subject NV name for write-back, or NULL/"" for none
 *   has_repl  — 1 if the replacement is real, 0 if it's the dummy zero
 *
 * Side effects:
 *   - Calls exec_stmt(subj_name, &subj, pat, has_repl?&repl:NULL, has_repl).
 *     In BB_MODE_LIVE (set by scrip_rt_init), Phases 3-5 route through
 *     bb_build_flat/binary -> direct bb_box_fn call.
 *   - Sets g_last_ok from exec_stmt's return (so SM_JUMP_S/F observe it).
 *   - Resets g_pat_sp = 0 after each statement (defensive — patterns
 *     do not leak across statements).
 *============================================================================*/

void scrip_rt_match_variant(const char *subj_name, int has_repl)
{
    DESCR_t repl   = vstack_pop();   /* always pop: real repl or INTVAL(0) */
    DESCR_t subj_d = vstack_pop();
    DESCR_t pat_d  = (g_pat_sp > 0) ? pat_pop_internal() : pat_epsilon();

    int ok = exec_stmt(subj_name, &subj_d, pat_d,
                       has_repl ? &repl : NULL, has_repl);
    g_last_ok = ok ? 1 : 0;
    g_pat_sp  = 0;
}

/*==============================================================================
 * EM-7 entries
 *============================================================================*/

void scrip_rt_concat(void)
{
    /* SM_CONCAT: pop right then left; push CONCAT_fn(left, right). */
    DESCR_t r = vstack_pop();
    DESCR_t l = vstack_pop();
    DESCR_t result = CONCAT_fn(l, r);
    vstack_push(result);
    g_last_ok = (result.v != DT_FAIL);
}

void scrip_rt_push_null(void)
{
    /* SM_PUSH_NULL: push null (empty-string) descriptor; null is non-fail. */
    vstack_push(NULVCL);
    g_last_ok = 1;
}

void scrip_rt_coerce_num(void)
{
    /* SM_COERCE_NUM: unary +; coerce string to int (or real if not integer). */
    DESCR_t v = vstack_pop();
    if (v.v == DT_S) {
        int64_t iv = to_int(v);
        if (iv != 0 || (v.s && v.s[0] == '0')) {
            vstack_push(INTVAL(iv));
        } else {
            double rv = to_real(v);
            vstack_push(REALVAL(rv));
        }
    } else {
        vstack_push(v);
    }
    g_last_ok = 1;
}

/* SM_CALL_FN helpers — pseudo-calls inlined here to avoid a full INVOKE_fn round trip.
 * The full pseudo-call vocabulary mirrors sm_interp.c's SM_CALL_FN handler. */
extern DESCR_t subscript_get(DESCR_t arr, DESCR_t idx);
extern DESCR_t subscript_get2(DESCR_t arr, DESCR_t i, DESCR_t j);
extern int     subscript_set(DESCR_t arr, DESCR_t idx, DESCR_t val);
extern int     subscript_set2(DESCR_t arr, DESCR_t i, DESCR_t j, DESCR_t val);
extern void    sno_fold_name(char *name);

/* Local fold-get / fold-set helpers (mirror nv_fold_get/set in sm_interp.c).
 * Note: scrip is case-sensitive by default (RULES.md SN-31), so fold is a no-op
 * in practice — but the SIL-level NV layer may still want canonical names for
 * cases where the source explicitly uppercases.  Cheap; safe; matches interp. */
static DESCR_t _rt_nv_fold_get(const char *raw)
{
    if (!raw || !*raw) return NULVCL;
    char *n = GC_strdup(raw); sno_fold_name(n);
    return NV_GET_fn(n);
}
static void _rt_nv_fold_set(const char *raw, DESCR_t val)
{
    if (!raw || !*raw) return;
    char *n = GC_strdup(raw); sno_fold_name(n);
    NV_SET_fn(n, val);
}

void scrip_rt_call(const char *name, int nargs)
{
    /* Pop nargs from value stack into args[] in original order. */
    DESCR_t args[32];
    if (nargs > 32) nargs = 32;
    for (int k = nargs - 1; k >= 0; k--) args[k] = vstack_pop();

    /* ── Pseudo-calls (handled inline, mirror sm_interp.c) ────────────── */
    if (name && strcmp(name, "INDIR_GET") == 0) {
        /* $expr: name on stack, push value */
        DESCR_t name_d = args[0];
        DESCR_t val;
        if (IS_NAMEPTR(name_d))      val = NAME_DEREF_PTR(name_d);
        else if (IS_NAMEVAL(name_d)) val = _rt_nv_fold_get(name_d.s);
        else                         val = _rt_nv_fold_get(VARVAL_fn(name_d));
        vstack_push(val);
        g_last_ok = 1;
        return;
    }
    if (name && strcmp(name, "NAME_PUSH") == 0) {
        /* .X: pop name string, push DT_N NAMEVAL */
        DESCR_t name_d = args[0];
        const char *vname0 = VARVAL_fn(name_d);
        char *vname = GC_strdup(vname0 ? vname0 : ""); sno_fold_name(vname);
        vstack_push(NAMEVAL(vname));
        g_last_ok = 1;
        return;
    }
    if (name && strcmp(name, "ASGN_INDIR") == 0) {
        DESCR_t name_d = args[1];
        DESCR_t val    = args[0];
        int ok = 0;
        if (IS_NAMEPTR(name_d)) { *(DESCR_t*)name_d.ptr = val; ok = 1; }
        else if (IS_NAMEVAL(name_d)) { _rt_nv_fold_set(name_d.s, val); ok = 1; }
        else {
            const char *vname0 = VARVAL_fn(name_d);
            if (vname0 && *vname0) { _rt_nv_fold_set(vname0, val); ok = 1; }
        }
        vstack_push(val);
        g_last_ok = ok;
        return;
    }
    if (name && strcmp(name, "IDX") == 0) {
        if (nargs == 2) {
            DESCR_t r = subscript_get(args[0], args[1]);
            vstack_push(r);
            g_last_ok = (r.v != DT_FAIL);
        } else if (nargs == 3) {
            DESCR_t r = subscript_get2(args[0], args[1], args[2]);
            vstack_push(r);
            g_last_ok = (r.v != DT_FAIL);
        } else {
            /* N-dim via ITEM builtin */
            DESCR_t r = INVOKE_fn("ITEM", args, nargs);
            vstack_push(r);
            g_last_ok = (r.v != DT_FAIL);
        }
        return;
    }
    if (name && strcmp(name, "IDX_SET") == 0) {
        if (nargs == 3) {        /* val, base, idx */
            DESCR_t val = args[0]; DESCR_t base = args[1]; DESCR_t idx = args[2];
            g_last_ok = subscript_set(base, idx, val);
            vstack_push(val);
        } else if (nargs == 4) {
            DESCR_t val = args[0]; DESCR_t base = args[1];
            DESCR_t i = args[2]; DESCR_t j = args[3];
            g_last_ok = subscript_set2(base, i, j, val);
            vstack_push(val);
        } else {
            DESCR_t r = INVOKE_fn("ITEM_SET", args, nargs);
            g_last_ok = (r.v != DT_FAIL);
            vstack_push(args[0]);  /* val */
        }
        return;
    }

    /* SN-6: SNOBOL4 semantics — if any argument is FAIL, the call fails
     * without invoking the function. */
    for (int k = 0; k < nargs; k++) {
        if (args[k].v == DT_FAIL) {
            vstack_push(FAILDESCR);
            g_last_ok = 0;
            return;
        }
    }

    /* Default dispatch: try native chunk registry first (user-defined SNOBOL4
     * functions emitted as .LpcN chunks in the .s file), then INVOKE_fn for
     * builtins.  In mode-4, INVOKE_fn → g_user_call_hook → _rt_usercall
     * would also hit the registry, but short-circuiting here avoids the
     * extra indirection and the risk of interpreter call-stack corruption. */
    void *cfn = chunk_reg_lookup(name ? name : "");
    if (cfn) {
        DESCR_t result = call_native_chunk(name, cfn, args, nargs);
        vstack_push(result);
        g_last_ok = (result.v != DT_FAIL);
        return;
    }
    DESCR_t result = INVOKE_fn(name ? name : "", args, nargs);
    vstack_push(result);
    g_last_ok = (result.v != DT_FAIL);
}

/* SM_RETURN / SM_FRETURN / SM_NRETURN and conditional variants.
 *
 * In mode-4 with native call/ret, the chunk simply executes `ret` for plain
 * RETURN — the return value sits on the value stack already (value chunk's
 * body pushed it).  But FRETURN must replace TOS with FAILDESCR; NRETURN
 * must pop the value and push the function's name as a NAMEVAL descriptor.
 * Conditional variants check g_last_ok before doing anything.
 *
 * Returns 1 if the return should fire (caller emits `ret`), 0 if not (caller
 * falls through).  Note: when condition not met, this function does NOT
 * touch the value stack — the chunk continues normally.
 *
 * In the unconditional+plain RETURN case, the emitter does NOT call this
 * function — it emits `ret` directly.  This function exists for FRETURN /
 * NRETURN and conditional variants. */
int scrip_rt_do_return(int kind, int cond)
{
    /* cond: 0=unconditional, 1=only if last_ok, 2=only if !last_ok */
    if (cond == 1 && !g_last_ok) return 0;
    if (cond == 2 &&  g_last_ok) return 0;

    /* kind: 0=RETURN, 1=FRETURN, 2=NRETURN */
    if (kind == 1) {
        /* FRETURN: discard whatever the body produced, push FAILDESCR.
         * Body already pushed its retval (if any) — pop it; push FAIL. */
        if (g_vtop > 0) (void)vstack_pop();
        vstack_push(FAILDESCR);
        g_last_ok = 0;
    } else if (kind == 2) {
        /* NRETURN: pop body's retval; push a NAMEVAL.  Mode-4 deviation:
         * we don't track the function's retval-slot name at this layer, so
         * we use the raw value as a NAME if it's a string, else FAILDESCR.
         * This is enough for the dot-star NRETURN idiom (push_list = .dummy)
         * because the body explicitly assigned a NAMEVAL via NAME_PUSH. */
        DESCR_t v = (g_vtop > 0) ? vstack_pop() : FAILDESCR;
        if (v.v == DT_N) {
            vstack_push(v);          /* already a NAME */
        } else if (v.v == DT_S && v.s) {
            char *n = GC_strdup(v.s); sno_fold_name(n);
            vstack_push(NAMEVAL(n));
        } else {
            vstack_push(FAILDESCR);
        }
        g_last_ok = 1;
    } else {
        /* RETURN: leave TOS alone; just `ret`. */
        g_last_ok = (g_vtop > 0 && g_vstack[g_vtop - 1].v != DT_FAIL);
    }
    return 1;
}

/* SM_PAT_CAPTURE_FN_ARGS / SM_PAT_USERCALL_ARGS runtime helpers and
 * their pat_assign_callcap[_named_imm] externs were REMOVED in
 * EM-7-revert (session #72) along with the rest of the brokered
 * Phase-3 path.  The underlying pat_assign_callcap* primitives in
 * snobol4_pattern.c remain available for EM-7c. */

/*==============================================================================
 * EM-6 stubs — symbols pulled in transitively by eval_code.c / eval_pat.c
 * that belong to the polyglot driver (sm_interp.c, interp_eval.c, etc.).
 * These are not exercised by the EM-6 SNOBOL4 pattern gate; they will be
 * replaced by real implementations in later rungs (EM-10+).
 *============================================================================*/

#include "../../runtime/x86/sm_interp.h"  /* DESCR_t sm_call_chunk(int) */
#include "../../runtime/x86/sm_prog.h"    /* sm_opcode_name */

/* sm_call_chunk: used by eval_code.c when a DT_E chunk is EVAL'd.
 * Not exercised in EM-6 SNOBOL4 pattern gate (no chunk-via-EVAL paths). */
DESCR_t sm_call_chunk(int entry_pc)
{
    fprintf(stderr,
        "libscrip_rt: sm_call_chunk(%d) called — DT_E EVAL dispatch "
        "not yet wired in EM-6.  Add to EM-10 scope.\n", entry_pc);
    abort();
}

/* sm_opcode_name: used by sm_interp diagnostics pulled in via sm_interp.h.
 * Provide a minimal implementation for the .so. */
const char *sm_opcode_name(sm_opcode_t op)
{
    (void)op;
    return "?";
}

/* _is_pat_fnc_name / _expr_is_pat: used by eval_pat.c.
 * These decide at eval time whether a function call is pattern-returning.
 * Not exercised in EM-6 gate; safe stubs. */
#include "../../driver/interp_private.h"
int _is_pat_fnc_name(const char *s)  { (void)s; return 0; }
int _expr_is_pat(EXPR_t *e)          { (void)e; return 0; }
