
#ifndef EMIT_WALK_H
#define EMIT_WALK_H

#include "sm_prog.h"
#include "emit_flat.h"
#include <stdio.h>

/* RW-5: emit SM_Program as GNU-as .s (renamed from sm_codegen_text). */
int emit_walk_codegen(SM_Program *prog, FILE *out, const char *src_path);

/* RW-5: sub-flag: inline BB blob addresses (renamed from g_jit_emit_inline). */
extern int g_emit_inline;

/* RW-5: is PATND_t node eligible for flat-BB? (moved here from emit_sm_text). */
int emit_flat_eligible(const PATND_t *p);

/* RW-5: is PATND_t tree fully invariant? (moved here from emit_sm_text). */
int emit_flat_invariant(const PATND_t *p);

/* RW-5: phase-2 stack-sim → PATND_t reconstruction (renamed from sm_phase2_to_patnd). */
DESCR_t emit_walk_phase2(const SM_Program *prog,
                         int phase2_start, int phase2_end,
                         int *out_variant);

/* ── Backward-compat aliases ── */
#define sm_codegen_text(prog,out,src)    emit_walk_codegen(prog,out,src)
#define g_jit_emit_inline                g_emit_inline
#define flat_is_eligible_node(p)         emit_flat_eligible(p)
#define patnd_is_fully_invariant(p)      emit_flat_invariant(p)
#define sm_phase2_to_patnd(pr,s,e,ov)    emit_walk_phase2(pr,s,e,ov)

#endif
