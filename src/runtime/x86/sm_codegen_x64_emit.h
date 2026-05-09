/*
 * sm_codegen_x64_emit.h — SM_Program → standalone x86-64 asm/binary
 *                        (M-JITEM-X64, GOAL-MODE4-EMIT EM-1+)
 *
 * Authors: Lon Jones Cherryholmes · Claude Sonnet
 * Date: 2026-05-06
 *
 * This is the mode-4 emitter entry point. Distinct from sm_codegen.c's
 * --jit-run path, which builds an in-process RX slab and jumps in.
 * mode-4 emits standalone assembler text that, when assembled and linked
 * against libscrip_rt.so, runs the program as an independent ELF binary.
 *
 * EM-1 scope (this commit): wiring + literal-zero stub. The function
 * writes a minimal asm program whose `main` returns 0 (and ignores the
 * SM_Program entirely). Subsequent rungs (EM-2..EM-9) progressively
 * cover the SM opcode set.
 */

#ifndef SM_CODEGEN_X64_EMIT_H
#define SM_CODEGEN_X64_EMIT_H

#include <stdio.h>
#include "sm_prog.h"

/*
 * sm_codegen_x64_emit — emit asm for prog to `out`.
 *
 * EM-1: writes a literal-zero program (System V AMD64 main returning 0)
 * regardless of prog contents. Returns 0 on success, -1 on I/O error.
 *
 * EM-4-readability: optional `src_path` is the path of the source file
 * being compiled (.sno, .sc, etc.).  When non-NULL, the emitter reads
 * the file once and emits each statement's verbatim source text as a
 * page-break banner above its asm block, plus inline annotations on
 * variable / string-literal references where the asm alone is opaque.
 * Pass NULL to suppress source preservation (synthetic-program tests).
 *
 * Subsequent EM-N rungs extend coverage; the function signature is stable.
 */
int sm_codegen_x64_emit(SM_Program *prog, FILE *out, const char *src_path);

/*
 * EM-7a: Phase-2 SM simulator helpers.
 *
 * flat_is_eligible_node(p):
 *   Single-node invariance check (does NOT recurse into children).
 *   Returns 1 if this node's kind is bakeable at emit time.
 *
 * patnd_is_fully_invariant(p):
 *   Whole-tree invariance check — equivalent to bb_flat.c's
 *   flat_is_eligible() but callable from the emitter.
 *
 * sm_phase2_to_patnd(prog, start, end, out_variant):
 *   Walk SM instructions [start, end) (Phase-2 window) and reconstruct
 *   the PATND_t tree using the same pat_* constructors as the interpreter.
 *   Sets *out_variant = 1 if any node depends on a runtime value.
 *   Returns root DT_P DESCR_t (or pat_epsilon() if window is empty).
 */
#include "snobol4.h"   /* DESCR_t, PATND_t — forward decl only in header */
int     flat_is_eligible_node(const PATND_t *p);
int     patnd_is_fully_invariant(const PATND_t *p);
DESCR_t sm_phase2_to_patnd(const SM_Program *prog,
                            int phase2_start, int phase2_end,
                            int *out_variant);

#endif /* SM_CODEGEN_X64_EMIT_H */
