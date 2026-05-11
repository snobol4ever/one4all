/*
 * templates/sm_halt.c — SM_HALT per-opcode template.
 *
 * The FIRST template in the EM-MODE4-IS-MODE3-DUMP retrofit (sub-rung
 * -c, sess 2026-05-11).  ONE function describes SM_HALT's effect; the
 * `emitter_t` vtable walks it to produce native bytes (mode-3),
 * macro-invocation text (mode-4), or macro-definition body
 * (sm_macros.s regeneration).
 *
 * ⛔ KNOWN OPEN ARCHITECTURAL QUESTION — read before touching this
 * file or wiring additional backends:
 *
 * Mode-3 and mode-4 today emit DIFFERENT instructions for SM_HALT:
 *
 *   Mode-3 (`emit_halt_blob` in sm_codegen.c):
 *     inc dword [r13+20]   ; pc++ (st->pc lives at [r13+20])
 *     ret                  ; returns out of sm_jit_run's call frame
 *
 *   Mode-4 (`emit_sm_halt` in sm_codegen_x64_emit.c via
 *           `SM_TPL_NULLARY` in sm_emit_template.c):
 *     call rt_halt_tos@PLT  ; calls into libscrip_rt to handle exit
 *
 * The instructions differ because the environments differ.  Mode-3
 * runs in-process: r13 points at a live SM_State; ret unwinds to
 * sm_jit_run's C caller which exits the loop.  Mode-4 is a standalone
 * binary linked against libscrip_rt.so: there is no sm_jit_run; the
 * emitted main() wrapper handles termination; rt_halt_tos pops the
 * TOS for exit-code propagation.
 *
 * The design doc's SM_HALT example
 * (MIGRATION-MODE4-IS-MODE3-DUMP.md §"The template-as-program")
 * shows the mode-3 form `inc [r13+20]; ret` as ALL THREE backends'
 * output.  That is NOT what mode-4 emits today, and may not be the
 * right semantics for a standalone binary.  Resolving which
 * production is correct for mode-4 is a Lon-decision (sub-rung -c
 * carved scope to mode-3 only; design question deferred).
 *
 * SUB-RUNG -c SCOPE:
 *   - Define the template function below with the mode-3 instruction
 *     sequence (the design doc's spec).
 *   - Wire ONLY MODE-3 through it: replace `emit_halt_blob` in
 *     sm_codegen.c with `emit_sm_halt(binary_emitter)`.
 *   - DO NOT wire mode-4 through this template (yet).
 *   - DO NOT regenerate `sm_macros.s` HALT entry from this template
 *     (yet).
 *   - The template's text-backend and macro_def-backend productions
 *     CAN be reviewed by humans (passing this template to either
 *     produces a readable result) but they are not consumed by any
 *     production code path until the open question resolves.
 *
 * BYTE-IDENTITY INVARIANT (sub-rung -c):
 *   The mode-3 binary backend walks this template and MUST produce
 *   exactly the same 5 bytes as the legacy `emit_halt_blob`:
 *     41 ff 45 14   ; inc dword [r13+20]
 *     c3            ; ret
 *   The gate `test_gate_em_template_byte_identity.sh` enforces this
 *   on the SM_HALT site for every SM program in a small fixture set.
 *
 * Authors: Lon Jones Cherryholmes · Claude Opus 4.7
 * Sprint:  EM-MODE4-IS-MODE3-DUMP-c / GOAL-MODE4-EMIT
 */

#include "../emitter.h"

/*
 * emit_sm_halt — describe the SM_HALT opcode for emission.
 *
 * Sprinkle model: this template body uses both instruction primitives
 * (emit_inc_mem_r13_disp8, emit_ret — narrow-vtable inline helpers
 * building bb_insn_desc_t and dispatching through emitter_t.emit_insn)
 * and formatting primitives (e->comment — added by sub-rung -b).
 * Each backend implements or no-ops each call per the design.
 *
 * Backend productions:
 *   - emitter_binary:   writes bytes  41 ff 45 14 c3 into bb_emit buffer.
 *   - emitter_text:     writes a comment line + two GAS asm lines.
 *     (TEXT_MODE_INVOCATION: that's the full per-call-site output;
 *      TEXT_MODE_DEFINITION: same lines wrapped by macro_begin/end.)
 *   - emitter_macro_def: emits ".macro HALT / inc dword ptr [r13+20] /
 *                         ret / .endm".
 *
 * The template body does not branch on backend.  It describes SM_HALT's
 * effect; backends decide how to render each call.
 */
void emit_sm_halt(emitter_t *e)
{
    if (!e) return;

    /* Documentation comment — text backends render; binary backend
     * ignores.  The vtable's `comment` slot is set in sub-rung -b. */
    EMIT_OPT(e, comment, e, "SM_HALT — exit sm_jit_run via ret");

    /* inc dword [r13 + 20]  — st->pc++  (pc field at offset 20 of SM_State).
     * Narrow-vtable inline helper builds the bb_insn_desc_t and
     * dispatches through emit_insn — proven path. */
    emit_inc_mem_r13_disp8(e, 20);

    /* ret  — returns to sm_jit_run's frame, which unwinds to the
     * C caller that invoked it; that caller exits the interpreter
     * loop.  In a future mode-4 retrofit the semantics may differ
     * (see KNOWN OPEN ARCHITECTURAL QUESTION above). */
    emit_ret(e);

    /* pad_to_blob_size — design-doc vtable hook.  Today's mode-3
     * uses variable-size blobs (per-pc address table in
     * g_blob_addrs[]) so this is a no-op even in binary mode.
     * Templates that target a fixed-blob architecture would honor
     * this; SM_HALT does not need it. */
    EMIT_OPT(e, pad_to_blob_size, e);
}
