/*
 * scrip_rt.h — public ABI for libscrip_rt.so (M-JITEM-X64 / EM-1..EM-6)
 *
 * Authors: Lon Jones Cherryholmes · Claude Sonnet
 * Date: 2026-05-06
 *
 * libscrip_rt.so is the runtime support library that mode-4-emitted
 * binaries link against.  It carries language-level semantics — pattern
 * matcher, NV table, builtins, GC, generator BB pump (post-EM-10),
 * Prolog backtracking machinery (post-EM-14) — so that emitted
 * executables contain only compiled SM chunks plus calls into this ABI.
 *
 * Value type: DESCR_t (from descr.h / snobol4.h).  The separate
 * ScripRtVal/ScripRtTag types that existed through EM-5 are gone.
 * DESCR_t is the one true descriptor; every stack slot is a DESCR_t.
 * DT_* tag constants (DT_SNUL=0, DT_S=1, DT_I=6, DT_E=11, DT_FAIL=99)
 * come from descr.h which emitted binaries include via snobol4.h.
 *
 * EM-1 surface:
 *   scrip_rt_init      — process startup; receives argc/argv
 *   scrip_rt_finalize  — process shutdown; returns the program rc
 *
 * EM-2 surface:
 *   scrip_rt_push_int  — push a DT_I integer onto the SM value stack
 *   scrip_rt_pop_int   — pop TOS as int64_t
 *   scrip_rt_halt      — record the program's exit code
 *   scrip_rt_unhandled_op — runtime trap for unimplemented opcodes
 *
 * EM-3 surface:
 *   scrip_rt_push_str  — push a DT_S string onto the SM stack
 *   scrip_rt_pop_descr — pop TOS into a DESCR_t (SM_STORE_VAR codegen)
 *   scrip_rt_arith     — binary arithmetic (ADD/SUB/MUL/DIV/MOD)
 *   scrip_rt_nv_get    — load named variable onto stack (real in EM-6)
 *   scrip_rt_nv_set    — store TOS into named variable  (real in EM-6)
 *   scrip_rt_pop_void  — pop and discard TOS (SM_POP)
 *
 * EM-4 surface:
 *   scrip_rt_last_ok     — read success flag (SM_JUMP_S / SM_JUMP_F)
 *   scrip_rt_set_last_ok — write success flag
 *
 * EM-5 surface:
 *   scrip_rt_push_chunk_descr — push DT_E chunk descriptor (SM_PUSH_CHUNK)
 *   SM_CALL_CHUNK / SM_RETURN are baked direct (call .LpcN / ret).
 *
 * EM-6 surface:
 *   scrip_rt_pat_lit        — SM_PAT_LIT    (a[0].s = literal)
 *   scrip_rt_pat_span       — SM_PAT_SPAN   (pops charset from vstack)
 *   scrip_rt_pat_break      — SM_PAT_BREAK  (pops charset from vstack)
 *   scrip_rt_pat_any        — SM_PAT_ANY    (pops charset from vstack)
 *   scrip_rt_pat_notany     — SM_PAT_NOTANY (pops charset from vstack)
 *   scrip_rt_pat_len        — SM_PAT_LEN    (pops int from vstack)
 *   scrip_rt_pat_pos        — SM_PAT_POS    (pops int from vstack)
 *   scrip_rt_pat_rpos       — SM_PAT_RPOS   (pops int from vstack)
 *   scrip_rt_pat_tab        — SM_PAT_TAB    (pops int from vstack)
 *   scrip_rt_pat_rtab       — SM_PAT_RTAB   (pops int from vstack)
 *   scrip_rt_pat_arb        — SM_PAT_ARB    (no arg)
 *   scrip_rt_pat_arbno      — SM_PAT_ARBNO  (pops inner from patstack)
 *   scrip_rt_pat_rem        — SM_PAT_REM
 *   scrip_rt_pat_fence      — SM_PAT_FENCE
 *   scrip_rt_pat_fence1     — SM_PAT_FENCE1 (pops child from patstack)
 *   scrip_rt_pat_fail       — SM_PAT_FAIL
 *   scrip_rt_pat_abort      — SM_PAT_ABORT
 *   scrip_rt_pat_succeed    — SM_PAT_SUCCEED
 *   scrip_rt_pat_bal        — SM_PAT_BAL
 *   scrip_rt_pat_eps        — SM_PAT_EPS
 *   scrip_rt_pat_cat        — SM_PAT_CAT    (pops right then left)
 *   scrip_rt_pat_alt        — SM_PAT_ALT    (pops right then left)
 *   scrip_rt_pat_deref      — SM_PAT_DEREF  (pops DESCR_t from vstack)
 *   scrip_rt_pat_refname    — SM_PAT_REFNAME (a[0].s = var name)
 *   scrip_rt_pat_capture    — SM_PAT_CAPTURE (a[0].s=var, a[1].i=kind)
 *   scrip_rt_pat_boxval     — SM_PAT_BOXVAL (pat-stack top -> vstack)
 *   scrip_rt_exec_stmt      — SM_EXEC_STMT  (a[0].s=subj, a[1].i=has_repl)
 *
 * Stability: every symbol exported here is part of the mode-4 ABI.
 * Additions are backward-compatible; signatures must not change.
 */

#ifndef SCRIP_RT_H
#define SCRIP_RT_H

#include <stdint.h>

/* DESCR_t is the value type for all stack operations.  Emitted binaries
 * that include snobol4.h / descr.h get the full struct definition; this
 * forward declaration lets the ABI prototypes compile without snobol4.h. */
#ifndef DESCR_T_DEFINED
#define DESCR_T_DEFINED
typedef struct DESCR_t DESCR_t;
#endif

#ifdef __cplusplus
extern "C" {
#endif

/* ── EM-1 surface ────────────────────────────────────────────────────── */

void scrip_rt_init(int argc, char **argv);
int  scrip_rt_finalize(void);

/* ── EM-2 surface ────────────────────────────────────────────────────── */

void    scrip_rt_push_int(int64_t v);        /* push DT_I integer */
int64_t scrip_rt_pop_int(void);              /* pop TOS as int64_t (aborts if not DT_I) */
void    scrip_rt_halt(int rc);
void    scrip_rt_halt_tos(void);  /* safe-pop TOS as rc (DT_I) else 0 */
void    scrip_rt_unhandled_op(int op);

/* ── EM-3 surface ────────────────────────────────────────────────────── */

void scrip_rt_push_str(const char *s, uint32_t slen); /* push DT_S; slen=0 -> strlen */
void scrip_rt_pop_descr(DESCR_t *out);       /* pop TOS into *out (SM_STORE_VAR) */
void scrip_rt_arith(int op);                 /* SM_ADD=17 SUB=18 MUL=19 DIV=20 MOD=22 */
void scrip_rt_nv_get(const char *name);      /* push value of named variable */
void scrip_rt_nv_set(const char *name);      /* pop TOS -> named variable */
void scrip_rt_pop_void(void);                /* pop and discard TOS (SM_POP) */

/* ── EM-4 surface ────────────────────────────────────────────────────── */

int  scrip_rt_last_ok(void);
void scrip_rt_set_last_ok(int ok);

/* ── EM-5 surface ────────────────────────────────────────────────────── */

/* Push DT_E chunk descriptor {entry_pc, arity} (SM_PUSH_CHUNK).
 * SM_CALL_CHUNK with known entry_pc bakes direct `call .LpcN`. */
void scrip_rt_push_chunk_descr(int64_t entry_pc, int64_t arity);

/* ── EM-7c surface — pre-built BB blob match (mode-4 emit path) ──────── */

/* The mode-4 emitter bakes invariant pattern sub-trees as flat .text
 * chunks (via bb_build_flat_text); at SM_EXEC_STMT, the emitted code
 * pushes [subj][repl_or_zero] on the value stack and calls this entry.
 *
 *   blob_α  — address of `_pat_inv_<id>_α` (the baked entry)
 *   subj_name   — subject NV name for write-back, or NULL
 *   has_repl    — 1 if a real replacement is on the stack, 0 if dummy
 *
 * Result lands on libscrip_rt's last-ok flag (SM_JUMP_S/F observe it).
 */
void scrip_rt_match_blob(void *blob_α,
                         const char *subj_name,
                         int has_repl);

/* ── EM-7c-variant surface — pattern-construction ABI (session #80) ──── */

/* The mode-4 emitter emits one call per SM_PAT_* opcode for variant
 * patterns (those with at least one runtime-dependent leaf — *VAR,
 * BREAK(VAR), LEN(VAR), etc.).  Each call mirrors the corresponding
 * sm_interp.c case: builds a PATND_t fragment, pushes it on the
 * libscrip_rt pat-stack.  SM_PAT_BOXVAL bridges pat-stack → vstack.
 * SM_EXEC_STMT for variant patterns calls scrip_rt_match_variant,
 * which dispatches to exec_stmt (which in BB_MODE_LIVE — set by
 * scrip_rt_init — routes Phase-3 through bb_build_flat/binary +
 * direct bb_box_fn call, NOT through bb_broker).
 *
 * Distinction from the EM-7-pre ABI that was reverted in session #72:
 * the runtime mechanics are similar (a pat-stack of DESCR_t fragments)
 * but the Phase-3 route is correct.  Future rung
 * EM-7c-variant-bb-pool-emit will replace this entire ABI with
 * per-variant-node bb_pool emit driven by an emit-time partition,
 * matching the ideal architecture in GOAL-MODE4-EMIT.md.
 */

void scrip_rt_pat_lit     (const char *s);
void scrip_rt_pat_refname (const char *name);
void scrip_rt_pat_span    (void);
void scrip_rt_pat_break   (void);
void scrip_rt_pat_any     (void);
void scrip_rt_pat_notany  (void);
void scrip_rt_pat_len     (void);
void scrip_rt_pat_pos     (void);
void scrip_rt_pat_rpos    (void);
void scrip_rt_pat_tab     (void);
void scrip_rt_pat_rtab    (void);
void scrip_rt_pat_arb     (void);
void scrip_rt_pat_arbno   (void);
void scrip_rt_pat_rem     (void);
void scrip_rt_pat_fence   (void);
void scrip_rt_pat_fence1  (void);
void scrip_rt_pat_fail    (void);
void scrip_rt_pat_abort   (void);
void scrip_rt_pat_succeed (void);
void scrip_rt_pat_bal     (void);
void scrip_rt_pat_eps     (void);
void scrip_rt_pat_cat     (void);
void scrip_rt_pat_alt     (void);
void scrip_rt_pat_deref   (void);
void scrip_rt_pat_capture (const char *varname, int kind);
void scrip_rt_pat_boxval  (void);
void scrip_rt_pat_capture_fn     (const char *fname, int is_imm, const char *namelist);
void scrip_rt_pat_capture_fn_args(const char *fname, int is_imm, int nargs);
void scrip_rt_pat_usercall       (const char *fname);
void scrip_rt_pat_usercall_args  (const char *fname, int nargs);

void scrip_rt_match_variant(const char *subj_name, int has_repl);

/* ── EM-6 surface — REMOVED in EM-7-revert (session #72) ─────────────────
 *
 * The brokered Phase-3 pattern-builder ABI was here.  EM-7c-variant
 * (session #80) reintroduces a pattern-construction ABI above with
 * the corrected Phase-3 routing (BB_MODE_LIVE / bb_build_*, not
 * bb_broker).  See GOAL-MODE4-EMIT.md "Design Discoveries" for why
 * the broker route was wrong and EM-7c-variant block above for what
 * still needs to land in EM-7c-variant-bb-pool-emit.
 * ──────────────────────────────────────────────────────────────────────── */

/* ── EM-7-pre keepers ──────────────────────────────────────────────────── */

/* SM_CONCAT: pop two DESCRs from vstack, push concatenation result. */
void scrip_rt_concat(void);

/* SM_PUSH_NULL: push null (empty-string) descriptor, set last_ok=1. */
void scrip_rt_push_null(void);

/* SM_COERCE_NUM: pop TOS, coerce string→int/real if needed, push result. */
void scrip_rt_coerce_num(void);

/* SM_CALL: general function call.
 * name:  function name (INDIR_GET / NAME_PUSH / IDX / ... or user/builtin).
 * nargs: number of arguments already popped from SM vstack into the runtime's
 *        internal arg buffer by the emitter's call-setup sequence.
 * The function name is the compile-time constant from SM_Instr.a[0].s.
 * Args were pushed left-to-right; this function pops nargs from vstack,
 * dispatches, and pushes one result.  Sets last_ok. */
void scrip_rt_call(const char *name, int nargs);

/* SM_RETURN / SM_FRETURN / SM_NRETURN and conditional variants.
 * kind:  0=RETURN, 1=FRETURN, 2=NRETURN.
 * cond:  0=unconditional, 1=only-if-last_ok (_S), 2=only-if-not-last_ok (_F).
 * In mode-4 native-call model, RETURN is a bare ret — the emitter emits `ret`
 * directly for unconditional RETURN.  This helper handles the cases that need
 * runtime logic: FRETURN (push FAILDESCR, clear last_ok), NRETURN (push name
 * descriptor), and all conditional variants that must check g_last_ok first.
 * Returns 1 if the return should execute (condition met), 0 if not (fall
 * through to next instruction). */
int scrip_rt_do_return(int kind, int cond);

/* EM-7d-usercall-reentrant: native chunk function pointer registry.
 *
 * The emitter walks the SM_Program for SM_LABEL instructions with a[0].s
 * set (SNOBOL4 function entry labels) and emits a .data table of
 * rt_chunk_entry records in the .s file, terminated by {NULL, NULL}.
 * scrip_rt_register_chunks() is called from main() (before scrip_rt_init)
 * with a pointer to that table; it populates g_chunk_registry so that
 * _rt_usercall can dispatch user-defined SNOBOL4 functions by direct
 * call/ret without touching the interpreter call stack.
 *
 * typedef matches the layout emitted in the .s:
 *   .quad <name_ptr>   (const char* in .rodata)
 *   .quad <fn_ptr>     (void* pointing at .LpcN in .text)
 */
typedef struct {
    const char *name;   /* SNOBOL4 function name (upper-cased) */
    void       *fn;     /* native entry point: .LpcN label in .text */
} rt_chunk_entry;

/* Register a NULL-terminated array of rt_chunk_entry records.
 * Called from the emitted main() before scrip_rt_init.
 * Safe to call with NULL (no-op) for programs with no user functions. */
void scrip_rt_register_chunks(const rt_chunk_entry *tbl);

/* EM-7c-capture: patch a heap cap_t's fn pointer to point to a baked child blob.
 * Called from the emitted binary's preamble (before scrip_rt_init) once per
 * XNME/XFNME blob whose child α address is only known at link time. */
void scrip_rt_patch_cap_fn(void *cap_ptr, void *child_fn);

/* EM-7c-arbno: allocate a fresh arbno_t for a baked ARBNO blob and store
 * the pointer in *slot_ptr.  child_fn is the child blob's α entry.
 * Called from the emitted binary's preamble before scrip_rt_init. */
void scrip_rt_init_arbno(void **slot_ptr, void *child_fn);

/* SM_PAT_CAPTURE_FN_ARGS / SM_PAT_USERCALL_ARGS runtime helpers were
 * REMOVED in EM-7-revert (session #72) along with the rest of the
 * brokered Phase-3 path. */

#ifdef __cplusplus
}
#endif

#endif /* SCRIP_RT_H */
