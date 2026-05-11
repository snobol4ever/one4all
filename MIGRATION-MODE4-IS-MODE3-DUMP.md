# MIGRATION-MODE4-IS-MODE3-DUMP.md — template-vtable retrofit

**Status:** design.  Authored sess 2026-05-11 (Claude Opus 4.7) per
Lon pivot.  This document supersedes the "SEG_CODE disassembly"
framing in earlier drafts of `GOAL-MODE4-EMIT.md`'s
`EM-MODE4-IS-MODE3-DUMP` rung; the goal-file rung text is amended in
the same commit to point here.

---

## The principle, stated once

ARCH-x86.md §"Stack machine (SM_Program)" says:

> EMITTER walks same SM_Program → native code.  One instruction set.
> No divergence between interpreter and emitter.

Today two emitters walk SM_Program: `sm_codegen.c` writes native
bytes into `SEG_CODE` (mode-3); `sm_codegen_x64_emit.c` writes GAS
Intel-syntax `.s` text (mode-4) using a parallel set of per-opcode
emit functions.  The two productions cannot drift in theory but do
in practice — every prior mode-4 watermark has been about chasing a
mode-3 invariant the parallel emitter missed.

The fix is the same one the BB side already uses: a single
template walked twice (or three times), with the *backend* picked
at the call site, never inside the template.  ARCH-x86.md §"Dual-
mode emitter (TEXT / BINARY)" already documents this on the BB
side; the SM side has drifted and the retrofit brings it back.

---

## The architecture

### Layers

```
                    ┌─────────────────────────────────┐
                    │  per-opcode template (C)        │   <-- ONE source of truth
                    │  emit_sm_halt(emit_v *e)        │       per SM opcode
                    │  emit_sm_push_lit_i(emit_v *e)  │
                    │  emit_sm_jump(emit_v *e)        │
                    │  …                              │
                    └─────────────┬───────────────────┘
                                  │ calls e->ret(e),
                                  │       e->mov_reg_imm64(e, ...),
                                  │       e->call_plt(e, "rt_push_int"),
                                  │       e->comment(e, "…")
                                  ▼
                  ┌───────────────────────────────────┐
                  │  emit_v vtable surface (emit_v.h) │
                  └─────┬───────────┬─────────┬───────┘
                        │           │         │
              ┌─────────▼──┐  ┌─────▼────┐  ┌─▼─────────────────────┐
              │ binary     │  │ text     │  │ macro_def             │
              │ backend    │  │ backend  │  │ backend               │
              │            │  │          │  │                       │
              │ writes raw │  │ writes   │  │ writes a `.macro NAME │
              │ x86 bytes  │  │ GAS asm  │  │ … .endm` block — one  │
              │ into       │  │ text to  │  │ per template invocation,│
              │ SEG_CODE   │  │ a FILE*  │  │ used to regenerate    │
              │            │  │          │  │ sm_macros.s           │
              └────────────┘  └──────────┘  └───────────────────────┘
                  ▲                ▲                  ▲
                  │                │                  │
                  │ called by      │ called by        │ called by
                  │ sm_codegen     │ sm_codegen_x64   │ a generator
                  │ (mode-3)       │ _emit (mode-4)   │ tool that
                  │                │                  │ regenerates
                  │                │                  │ sm_macros.s
                  │                │                  │ on demand
```

### Why three backends not two

Lon's clarification (sess 2026-05-11): the mode-4 `.s` file MUST stay
readable.  That means mode-4 emits *macro invocations*
(`PUSH_INT 42`) not the underlying instruction sequence (`movabs rdi,
42 ; call rt_push_int@PLT`).  But the macro body itself must be
generated from the template — otherwise the macro is a separate
hand-maintained source of truth and the divergence problem returns
on the macro layer.

The three backends cover three productions of the same template:

| Backend          | Output       | Consumer                       |
|------------------|--------------|--------------------------------|
| `emit_v_binary`  | x86-64 bytes | mode-3 in-process JIT          |
| `emit_v_text`    | `MNEMONIC <args>` | mode-4's per-call-site `.s` line — but for now this emits **macro invocations** matching the macro names defined by `macro_def` (e.g., `PUSH_INT 42`); the underlying instruction sequence is hidden in the macro body |
| `emit_v_macro_def` | `.macro NAME params / <body> / .endm` | `sm_macros.s` regeneration |

In practice mode-4's per-call-site emission may walk the same template
with **text backend in "invocation mode"** (emit one line like
`PUSH_INT 42` and stop) while the macro file regeneration walks with
**text backend in "definition mode"** (emit the full body of the
macro).  This is one boolean flag on the text backend, not a fourth
backend.

### The template-as-program

Each per-opcode template is a C function that reads like a small
program describing the opcode's effect in mnemonics and comments.

```c
/* sm_templates/halt.c — SM_HALT template.  ONE source of truth.
 * Emitted three ways via emit_v: bytes (mode-3), macro invocation
 * (mode-4 site), macro body (sm_macros.s regen). */

#include "../emit_v.h"

void emit_sm_halt(emit_v *e)
{
    e->comment(e, "SM_HALT — exit sm_jit_run via ret");

    /* pc++ : st->pc is at [r13+20] */
    e->inc_mem_disp8(e, REG_R13, 20);

    /* return to sm_jit_run's frame */
    e->ret(e);

    /* binary backend: NOP-pad to ME3_BLOB_SIZE.
     * text backend: no-op (whitespace/layout, not semantics).
     * macro_def backend: no-op (macros don't pad). */
    e->pad_to_blob_size(e);
}
```

Reading that function tells a reviewer the *complete* story of SM_HALT
in three productions.  No "see also sm_macros.s" footnote; no
"sm_codegen_x64_emit.c also emits this differently" footnote.

### The vtable surface

`emit_v.h` defines one C struct of function pointers plus per-backend
opaque state.

```c
/* emit_v.h — surface for all three backends */

#include <stdio.h>
#include <stdint.h>

typedef enum {
    REG_RAX, REG_RBX, REG_RCX, REG_RDX, REG_RSI, REG_RDI,
    REG_R8, REG_R9, REG_R10, REG_R11, REG_R12, REG_R13,
    REG_R14, REG_R15, REG_RBP, REG_RSP
} emit_reg_t;

typedef struct emit_v {
    /* — opcode-level primitives (one x86 instruction per call) — */
    void (*mov_reg_imm64)  (struct emit_v *e, emit_reg_t r, uint64_t imm);
    void (*mov_reg_imm32)  (struct emit_v *e, emit_reg_t r, uint32_t imm);
    void (*mov_reg_reg)    (struct emit_v *e, emit_reg_t dst, emit_reg_t src);
    void (*mov_mem_imm32)  (struct emit_v *e, emit_reg_t base, int32_t disp, uint32_t imm);
    void (*inc_mem_disp8)  (struct emit_v *e, emit_reg_t base, int8_t disp);
    void (*sub_rsp_imm8)   (struct emit_v *e, int8_t imm);
    void (*add_rsp_imm8)   (struct emit_v *e, int8_t imm);
    void (*call_reg)       (struct emit_v *e, emit_reg_t r);
    void (*call_plt)       (struct emit_v *e, const char *sym);
    void (*jmp_rel32_sym)  (struct emit_v *e, const char *sym);
    void (*jmp_rel32_pc)   (struct emit_v *e, int target_pc);
    void (*je_rel32_pc)    (struct emit_v *e, int target_pc);
    void (*jne_rel32_pc)   (struct emit_v *e, int target_pc);
    void (*ret)            (struct emit_v *e);
    void (*push_reg)       (struct emit_v *e, emit_reg_t r);
    void (*pop_reg)        (struct emit_v *e, emit_reg_t r);
    void (*lea_rip_sym)    (struct emit_v *e, emit_reg_t r, const char *sym);

    /* — structural (binary: records offset; text: writes line) — */
    void (*label)          (struct emit_v *e, const char *name);
    void (*pc_label)       (struct emit_v *e, int pc);
    void (*pad_to_blob_size)(struct emit_v *e);

    /* — formatting / readability (text only; binary ignores) — */
    void (*comment)        (struct emit_v *e, const char *text);
    void (*banner)         (struct emit_v *e, const char *text);
    void (*column_break)   (struct emit_v *e);
    void (*blank_line)     (struct emit_v *e);

    /* — macro_def-only hooks (binary + text invocation-mode ignore) — */
    void (*macro_begin)    (struct emit_v *e, const char *name,
                            const char *const *params, int nparams);
    void (*macro_param_ref)(struct emit_v *e, const char *name);  /* emits \param in macro body */
    void (*macro_end)      (struct emit_v *e);

    /* — opaque per-backend state — */
    void *state;
} emit_v;
```

Three backend factories:

```c
/* emit_v_binary.c */
emit_v *emit_v_binary_create(int seg);                   /* seg = SEG_CODE typically */
void    emit_v_binary_destroy(emit_v *e);

/* emit_v_text.c */
typedef enum { TEXT_MODE_INVOCATION, TEXT_MODE_DEFINITION } emit_v_text_mode;
emit_v *emit_v_text_create(FILE *out, emit_v_text_mode mode);
void    emit_v_text_destroy(emit_v *e);

/* emit_v_macro_def.c — thin wrapper around emit_v_text in DEFINITION mode */
emit_v *emit_v_macro_def_create(FILE *out);
void    emit_v_macro_def_destroy(emit_v *e);
```

### Three-call-site discipline

A template's body is the same regardless of caller:

```c
/* mode-3 driver, in sm_codegen.c, replacing inline byte writes */
emit_v *e = emit_v_binary_create(SEG_CODE);
emit_sm_halt(e);
emit_v_binary_destroy(e);

/* mode-4 driver, in sm_codegen_x64_emit.c, per call site
 * — runs the template in invocation mode, which renders the macro
 *   invocation line `HALT` and stops (does not expand the body) */
emit_v *e = emit_v_text_create(out, TEXT_MODE_INVOCATION);
emit_sm_halt(e);
emit_v_text_destroy(e);

/* sm_macros.s regenerator, runs once per build
 * — runs every template in definition mode, which renders
 *   `.macro HALT / <full body> / .endm` */
FILE *macs = fopen("sm_macros.s", "w");
emit_v *e = emit_v_macro_def_create(macs);
emit_sm_halt(e);          /* emits .macro HALT ; … ; .endm */
emit_sm_push_lit_i(e);    /* emits .macro PUSH_INT val ; … ; .endm */
emit_sm_push_lit_s(e);    /* emits .macro PUSH_LIT_S lbl, len ; … ; .endm */
…
emit_v_macro_def_destroy(e);
fclose(macs);
```

The mode-3 caller does not know about macros; the mode-4 caller does
not know about bytes; the macro-def caller does not know about
SEG_CODE — and the template is unaware of all three.

### How invocation mode works

When the text backend is in `TEXT_MODE_INVOCATION` it accumulates the
template's calls in a small buffer and at the end writes ONE line:

```
                        HALT
```

— the macro name plus whatever operand the template's "argument"
calls expressed.  Argument capture happens via designated entry
points: a template wraps its body in `e->macro_begin(...)` /
`e->macro_end(...)` (definition mode) or relies on those being
no-ops while it issues one or two `e->macro_param_ref(...)` to
declare its operands (invocation mode uses those refs to compose the
single output line).  Detailed render rules live in
`emit_v_text.c`'s implementation comments.

### Mode-3 control flow gets fixed for free

Today `sm_codegen.c` emits a "standard blob" of `inc pc ; mov rax,
handler ; call rax ; jmp trampoline` for most opcodes — the threaded-
call shape ME-3 documented.  That's because each opcode handler is a
C function and the blob is just a trampoline into it.  Under the new
template architecture each opcode template is laid down inline, so
the standard blob disappears for opcodes whose templates are pure
native (HALT, PUSH_LIT_I, PUSH_VAR, STORE_VAR, POP, ADD, JUMP, LABEL,
etc.).  Opcodes whose templates legitimately need a runtime call
(SM_CALL_FN, SM_PAT_*) keep `e->call_plt(e, "rt_xxx")` in their
template — the call is explicit, not a wrapper around a threaded
handler.

This is the *EM-MODE4-IS-MODE3-DUMP-b* outcome (mode-3 native control
flow) achieved as a byproduct of the retrofit, not as a separate
sub-rung.

### Mode-4 disk dump

Mode-4 stops being a parallel walker of SM_Program.  Instead it
walks SM_Program the same way mode-3 does — by calling each
opcode's template — but with the text backend underneath in
invocation mode.  The `.s` file's text section is the result.

The mode-4 file as a whole still owns its auxiliary sections:
.rodata strtab, .data expression registry, file header, libscrip_rt
PLT decl block, `main()` wrapper, `.include "sm_macros.s"` line.
Those stay in `sm_codegen_x64_emit.c` (or migrate to a sibling
`sm_emit_aux.c`).  Only the .text section's per-instruction emission
is replaced by template calls.

### sm_macros.s regeneration

A small generator program (or build-time make rule) instantiates
`emit_v_macro_def` and invokes every per-opcode template once.  The
output IS `sm_macros.s`.  The file is committed (for build-without-
generator portability) and `make` regenerates it when any template
changes.

Hand-editing `sm_macros.s` is forbidden after the retrofit completes
— a header comment in the regenerated file says so.

---

## BB-side discipline (deferred, in scope long-term)

Per Lon (Q2 sess 2026-05-11): one C file per Byrd box.  That mirrors
the SM side: each XCHR/XSPNC/XANYC/XBRKC/XNNYC/XLNTH/XTB/XRTB/XFNCE/
XDSAR/XATP/XBRKX/XCALLCAP/XCAT/XOR/XSTAR/XFARB/XPOSI/XRPSI/XNME/XFNME
becomes its own template C file calling through the same `emit_v`
surface.  The existing `bb_emit.c` EMIT_TEXT/EMIT_BINARY mode switch
collapses into `emit_v_binary` / `emit_v_text` — one vtable
across SM and BB.

Done not as part of this rung but as the natural follow-on once SM
side is uniform.  Carved as a separate post-rung in the goal file.

---

## Cross-language future

Per Lon (Q5 sess 2026-05-11) and ARCH-SCRIP.md mode 4: x86 today;
JVM, .NET, JS, WASM, C tomorrow.  When that work begins, the `emit_v`
surface raises one level: opcode-level primitives become host-neutral
("push int constant 42", "call runtime function rt_push_int") and
each backend (x86, JVM, .NET, JS, WASM, C) provides its own
implementation.  Per-opcode templates need no change — they already
speak in terms of the abstract operations.

The work is small at that point because the hard work (one template
per opcode; mode-3 and mode-4 share it) is done now.

---

## Sequencing

1. **EM-MODE4-IS-MODE3-DUMP-a (this commit)** — design doc + goal-file
   amendment.  No code.

2. **EM-MODE4-IS-MODE3-DUMP-b — vtable skeleton.**  Add
   `src/runtime/x86/emit_v.h` with the struct, three skeleton backend
   impls (`emit_v_binary.c`, `emit_v_text.c`, `emit_v_macro_def.c`
   — last is a thin wrapper over text in DEFINITION mode).  Wire into
   Makefile.  Nothing calls them yet.  Gates green.

3. **EM-MODE4-IS-MODE3-DUMP-c — first opcode end-to-end: SM_HALT.**
   New file `src/runtime/x86/sm_templates/halt.c`.  Wire mode-3's halt
   blob through it (replaces `emit_halt_blob` inline writes).  Wire
   mode-4's halt emission through it (replaces `emit_sm_halt` in
   `sm_codegen_x64_emit.c`).  Wire `sm_macros.s` HALT macro
   regeneration through it (new generator tool
   `tools/regen_sm_macros.c` invoked at build time).  Gates green —
   bytes mode-3 emits byte-for-byte unchanged; `.s` mode-4 emits
   layout-identical or `HALT`-macro-invocation-identical; sm_macros.s
   regenerated identical to current hand-maintained version (or with
   a documented small formatting diff).

4. **EM-MODE4-IS-MODE3-DUMP-d through -k — one opcode per rung**, in
   this order:
   - SM_PUSH_LIT_I
   - SM_PUSH_LIT_S
   - SM_VOID_POP
   - SM_JUMP
   - SM_JUMP_S / SM_JUMP_F
   - SM_ADD / SM_SUB / SM_MUL / SM_DIV / SM_MOD / SM_EXP (one rung)
   - SM_LABEL / SM_STNO
   - SM_CALL_FN (the big one)
   - SM_RETURN / SM_NRETURN / SM_FRETURN and conditional variants

   Each lands its own template C file under `sm_templates/`.  Each
   deletes the corresponding inline byte writes from `sm_codegen.c`
   AND the corresponding per-opcode function in
   `sm_codegen_x64_emit.c` AND the corresponding hand-maintained
   macro in `sm_macros.s` (now regenerated from the template).

5. **EM-MODE4-IS-MODE3-DUMP-l — pattern opcodes** (SM_PAT_*).  Same
   discipline.

6. **EM-MODE4-IS-MODE3-DUMP-m — `sm_macros.s` deleted from VCS;
   regenerated at build.**  Make rule added.  Test that build from a
   clean tree works without a checked-in `sm_macros.s`.  Drop the
   file from git (or keep with the "DO NOT EDIT — generated" header
   if portability concerns warrant).

7. **EM-MODE4-IS-MODE3-DUMP-n — close the rung.**  Run
   `test_gate_em_beauty_subsystems_mode4.sh` — gate must not regress.
   If `EM-7d-beauty-subsystems` baseline (PASS=4 FAIL=13) improves as
   a byproduct (likely, since mode-4 is now byte-identical to mode-3
   on all retrofitted opcodes), capture the new baseline.

---

## Gates

Every rung must pass before commit:

| Gate                                      | Required |
|-------------------------------------------|----------|
| `test_smoke_snobol4.sh`                   | PASS=7   |
| `test_smoke_unified_broker.sh`            | PASS=49  |
| `test_gate_em_beauty_subsystems_mode4.sh` | ≥ baseline (4) |
| 5 tracked artifacts `gcc -c` clean        | yes      |

New gate added with rung -c (first opcode landing):
`test_gate_em_template_byte_identity.sh` — for each retrofitted
opcode, emit a tiny program exercising it under both mode-3
(captures SEG_CODE bytes via `--dump-seg-code`) and mode-4 (`.s` →
assemble → `objdump -d`), diff the byte sequences modulo
loader-relocation noise.  Any divergence is a template bug.

---

## Why this is the right design, finally

Past rungs solved subproblems (alignment, dual-entry labels, banner
formatting, label naming, GAS syntax fidelity) without addressing the
underlying duplication.  The duplication was the cause of most of
those subproblems being mode-4-only — mode-3 had been fixed earlier
in `sm_codegen.c` but mode-4's parallel emitter didn't get the fix.

After the retrofit there is ONE place to fix any opcode: its
template.  Mode-3, mode-4, and `sm_macros.s` regenerate together
from that one source.  Divergence is impossible by construction.

The rung name `EM-MODE4-IS-MODE3-DUMP` is preserved because the
*outcome* is the same (mode-4 produces what mode-3 produces).  The
*mechanism* is the template-vtable, not literal SEG_CODE disassembly.
The previous mechanism (literal disassembly of SEG_CODE bytes to
asm text) was hard because the bytes lose mnemonic structure, and
recovering it requires a side-table that's almost as big as the
template would have been.  The template approach skips the lossy
intermediate and emits both productions directly from the source.
