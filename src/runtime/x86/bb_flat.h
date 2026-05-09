/*
 * bb_flat.h — Flat-Glob Invariant Pattern Emitter (M-DYN-FLAT)
 *
 * bb_build_flat(p) walks an entire invariant PATND_t tree and emits
 * ALL sub-box code into ONE contiguous buffer with direct jmps between
 * boxes — no call/ret, no bb_seq trampoline, no per-node heap zeta.
 *
 * Layout:
 *   [entry]  cmp esi,0; je PAT_α; jmp PAT_β
 *   [PAT_α]  first box α code → jmp next_α or jmp PAT_ω
 *   [PAT_β]  first box β code → jmp prev_β or jmp PAT_ω
 *   ...      all sub-boxes inlined flat, wired by direct jmps
 *   [PAT_γ]  rax=σ, rdx=δ; ret
 *   [PAT_ω]  xor eax,eax; xor edx,edx; ret
 *   [data]   mutable slots: Δ_save values, matched.δ accumulators
 *
 * r10 = &Δ (global cursor pointer), loaded once at entry.
 * All cursor reads/writes use [r10] directly — no per-box reload.
 *
 * Only invariant patterns are eligible (patnd_is_invariant() == 1).
 * Variant nodes (XDSAR, XVAR, XATP, XFNME, XNME, XFARB, XSTAR)
 * fall back to bb_build_binary_node() trampoline path.
 *
 * Authors: Lon Jones Cherryholmes · Claude Sonnet 4.6
 * Sprint:  RT-129 / M-DYN-FLAT
 */

#ifndef BB_FLAT_H
#define BB_FLAT_H

#include <stdio.h>      /* FILE — bb_build_flat_text */
#include "bb_pool.h"
#include "snobol4.h"
#include "bb_box.h"
#include "emitter_v.h"  /* emitter_v — for bb_flat_set_intern_str signature */

/*
 * Build a flat-globbed blob for an entire invariant PATND_t tree.
 * Returns NULL if p is variant or allocation fails — caller uses
 * bb_build_binary_node() trampoline fallback.
 */
bb_box_fn bb_build_flat(PATND_t *p);

/*
 * EM-7b: TEXT-mode counterpart to bb_build_flat.
 *
 * Emits the same flat-globbed code as GAS text directives into `out`,
 * with externally-visible top-level labels:
 *
 *   <prefix>_α       (entry — forward attempt)
 *   <prefix>_β       (re-entry — backtrack)
 *   <prefix>_γ       (success exit)
 *   <prefix>_ω       (failure exit)
 *
 * `.global` directives are emitted for all four so the `.s` exposes
 * them to the assembler/linker.  The EM-7c emitter (variant-node
 * runtime emitter) wires its γ/ω jmps to these symbols.
 *
 * Typical prefix: `_pat_inv_<pid>_<sid>`.
 *
 * Caller is responsible for emitting any `.text` section header /
 * surrounding scaffolding before/after this call.
 *
 * Returns 0 on success, -1 if `p` is variant (caller falls through
 * to the runtime emitter for variant nodes).
 *
 * NOTE (EM-7c followup): internal node labels (xcatN_mid_g, litN_b,
 * etc.) currently do NOT include the prefix — they collide if more
 * than one flat pattern is emitted into the same `.s` namespace.
 * For EM-7b's gate, one pattern per emission is fine; EM-7c will
 * extend this to namespace internals.
 */
int bb_build_flat_text(PATND_t *p, FILE *out, const char *prefix);

/* EM-7c-symbolic: set the intern_str callback used by bb_build_flat_text.
 * Call before each emit run so bb_flat.c can route literal strings through
 * the SM-side strtab instead of baking raw process pointers. */
void bb_flat_set_intern_str(const char *(*fn)(emitter_v *, const char *));

/*
 * EM-7c: reset internal label/slot counters between unrelated emit runs.
 *
 * Called by sm_codegen_x64_emit() at the start of an emit so that the
 * `xcatN_mid_g`, `altN_c0b`, ... internal labels start at N=0 for each
 * output `.s` file.  Within a single emit run, do NOT call this between
 * patterns — the counter must monotonically increment across patterns
 * sharing the same `.s` namespace to avoid label collisions.
 */
void bb_build_flat_text_reset(void);

/* EM-7c-bb-macros: write the BB macro library to the given path.
 * Call once per emit run (writes "bb_macros.s" in CWD).
 * Returns 0 on success, -1 on I/O error. */
int bb_macros_write_to_path(const char *path);

/* EM-7c-capture: install a callback that is invoked each time flat_emit_node
 * emits an XNME/XFNME child sub-blob.  sm_codegen_x64_emit installs this
 * to collect cap fixups for the startup preamble. */
void bb_flat_set_cap_fixup_cb(void (*cb)(void *cap_ptr, const char *child_α_label));

#endif /* BB_FLAT_H */
