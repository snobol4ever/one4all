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
                    │  emit_sm_halt(emitter_t *e)        │       per SM opcode
                    │  emit_sm_push_lit_i(emitter_t *e)  │
                    │  emit_sm_jump(emitter_t *e)        │
                    │  …                              │
                    └─────────────┬───────────────────┘
                                  │ calls e->ret(e),
                                  │       e->mov_reg_imm64(e, ...),
                                  │       e->call_plt(e, "rt_push_int"),
                                  │       e->comment(e, "…")
                                  ▼
                  ┌───────────────────────────────────┐
                  │  emitter_t vtable surface (emitter.h) │
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

| Backend          | Output       | Consumer                       | Applies to |
|------------------|--------------|--------------------------------|------------|
| `emitter_binary`  | x86-64 bytes | mode-3 in-process JIT          | SM and BB templates |
| `emitter_text`    | `MNEMONIC <args>` | mode-4's per-call-site `.s` line — but for now this emits **macro invocations** matching the macro names defined by `macro_def` (e.g., `PUSH_INT 42`); the underlying instruction sequence is hidden in the macro body | SM and BB templates |
| `emitter_macro_def` | `.macro NAME params / <body> / .endm` | `sm_macros.s` regeneration | **SM templates only** — BB has no macro layer |

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
 * Emitted three ways via emitter_t: bytes (mode-3), macro invocation
 * (mode-4 site), macro body (sm_macros.s regen). */

#include "../emitter.h"

void emit_sm_halt(emitter_t *e)
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

**Naming.**  The vtable type is `emitter_t`; the header is
`emitter.h`.  Both names predate this rung — the existing BB-side
`emitter_text.c` and `emitter_binary.c` use them — and this rung
just *extends* the same vocabulary to cover the SM side and the
new macro-definition backend.  No new vtable type is introduced.

(Sess 2026-05-11 housekeeping: the earlier "`_v`" suffix on the
header and struct, plus the `ev_` prefix on inline helpers, were
removed in the same commit that landed this third-pass amendment;
banned project-wide.  If you find one in a comment or commit
message, treat it as a typo and rename it.  Live source is clean.)

`emitter.h` defines one C struct of function pointers plus per-backend
opaque state.

```c
/* emitter.h — surface for all three backends */

#include <stdio.h>
#include <stdint.h>

typedef enum {
    REG_RAX, REG_RBX, REG_RCX, REG_RDX, REG_RSI, REG_RDI,
    REG_R8, REG_R9, REG_R10, REG_R11, REG_R12, REG_R13,
    REG_R14, REG_R15, REG_RBP, REG_RSP
} emit_reg_t;

typedef struct emitter_t {
    /* — opcode-level primitives (one x86 instruction per call) — */
    void (*mov_reg_imm64)  (struct emitter_t *e, emit_reg_t r, uint64_t imm);
    void (*mov_reg_imm32)  (struct emitter_t *e, emit_reg_t r, uint32_t imm);
    void (*mov_reg_reg)    (struct emitter_t *e, emit_reg_t dst, emit_reg_t src);
    void (*mov_mem_imm32)  (struct emitter_t *e, emit_reg_t base, int32_t disp, uint32_t imm);
    void (*mov_reg_mem)    (struct emitter_t *e, emit_reg_t dst, emit_reg_t base, int32_t disp);
    void (*mov_mem_reg)    (struct emitter_t *e, emit_reg_t base, int32_t disp, emit_reg_t src);
    void (*inc_mem_disp8)  (struct emitter_t *e, emit_reg_t base, int8_t disp);
    void (*sub_rsp_imm8)   (struct emitter_t *e, int8_t imm);
    void (*add_rsp_imm8)   (struct emitter_t *e, int8_t imm);
    void (*cmp_reg_imm32)  (struct emitter_t *e, emit_reg_t r, int32_t imm);
    void (*test_reg_reg)   (struct emitter_t *e, emit_reg_t a, emit_reg_t b);
    void (*call_reg)       (struct emitter_t *e, emit_reg_t r);
    void (*call_plt)       (struct emitter_t *e, const char *sym);
    void (*jmp_rel32_sym)  (struct emitter_t *e, const char *sym);
    void (*jmp_rel32_pc)   (struct emitter_t *e, int target_pc);
    void (*je_rel32_sym)   (struct emitter_t *e, const char *sym);
    void (*jne_rel32_sym)  (struct emitter_t *e, const char *sym);
    void (*je_rel32_pc)    (struct emitter_t *e, int target_pc);
    void (*jne_rel32_pc)   (struct emitter_t *e, int target_pc);
    void (*ret)            (struct emitter_t *e);
    void (*push_reg)       (struct emitter_t *e, emit_reg_t r);
    void (*pop_reg)        (struct emitter_t *e, emit_reg_t r);
    void (*lea_rip_sym)    (struct emitter_t *e, emit_reg_t r, const char *sym);
    void (*xor_reg_reg)    (struct emitter_t *e, emit_reg_t dst, emit_reg_t src);

    /* — structural (binary: records offset; text: writes line) — */
    void (*label)          (struct emitter_t *e, const char *name);
    void (*pc_label)       (struct emitter_t *e, int pc);
    void (*pad_to_blob_size)(struct emitter_t *e);
    void (*section)        (struct emitter_t *e, const char *name);   /* .text, .data, .rodata */
    void (*directive)      (struct emitter_t *e, const char *line);   /* arbitrary GAS directive */
    void (*data_quad)      (struct emitter_t *e, uint64_t val);       /* .quad imm */
    void (*data_quad_sym)  (struct emitter_t *e, const char *sym);    /* .quad sym */
    void (*data_string)    (struct emitter_t *e, const char *bytes, size_t len);

    /* — BB-port primitives (binary: emits jmp / label; text: writes port-named label,
     *   honors three-column LAW; both honor α/β/γ/ω as semantic ports) — */
    void (*bb_port_label)  (struct emitter_t *e, const char *box_prefix, char port);
                                                   /* port ∈ {'α','β','γ','ω'} */
    void (*bb_port_jmp)    (struct emitter_t *e, const char *box_prefix, char port);
                                                   /* jmp to <box_prefix>_<port> */
    void (*bb_box_banner)  (struct emitter_t *e, const char *kind, const char *args);
                                                   /* text: 120-char #---- rule
                                                      + "# BOX <kind>(<args>) [<prefix>]"
                                                      binary: NO-OP */

    /* — formatting / readability (text honors; binary ignores) — */
    void (*comment)        (struct emitter_t *e, const char *text);
    void (*banner)         (struct emitter_t *e, const char *text);    /* 120-char #==== rule + label */
    void (*minor_break)    (struct emitter_t *e, const char *text);    /* 120-char #---- rule + label */
    void (*column_break)   (struct emitter_t *e);                       /* align next emission to next column */
    void (*blank_line)     (struct emitter_t *e);

    /* — macro_def-only hooks (binary + text invocation-mode ignore) — */
    void (*macro_begin)    (struct emitter_t *e, const char *name,
                            const char *const *params, int nparams);
    void (*macro_param_ref)(struct emitter_t *e, const char *name);    /* emits \param in macro body */
    void (*macro_end)      (struct emitter_t *e);

    /* — opaque per-backend state — */
    void *state;
} emitter_t;
```

Three backend factories:

```c
/* emitter_binary.c */
emitter_t *emitter_binary_create(int seg);                   /* seg = SEG_CODE typically */
void    emitter_binary_destroy(emitter_t *e);

/* emitter_text.c */
typedef enum { TEXT_MODE_INVOCATION, TEXT_MODE_DEFINITION } emitter_text_mode_t;
emitter_t *emitter_text_create(FILE *out, emitter_text_mode_t mode);
void    emitter_text_destroy(emitter_t *e);

/* emitter_macro_def.c — thin wrapper around emitter_text in DEFINITION mode */
emitter_t *emitter_macro_def_create(FILE *out);
void    emitter_macro_def_destroy(emitter_t *e);
```

### Three-call-site discipline

A template's body is the same regardless of caller:

```c
/* mode-3 driver, in sm_codegen.c, replacing inline byte writes */
emitter_t *e = emitter_binary_create(SEG_CODE);
emit_sm_halt(e);
emitter_binary_destroy(e);

/* mode-4 driver, in sm_codegen_x64_emit.c, per call site
 * — runs the template in invocation mode, which renders the macro
 *   invocation line `HALT` and stops (does not expand the body) */
emitter_t *e = emitter_text_create(out, TEXT_MODE_INVOCATION);
emit_sm_halt(e);
emitter_text_destroy(e);

/* sm_macros.s regenerator, runs once per build
 * — runs every template in definition mode, which renders
 *   `.macro HALT / <full body> / .endm` */
FILE *macs = fopen("sm_macros.s", "w");
emitter_t *e = emitter_macro_def_create(macs);
emit_sm_halt(e);          /* emits .macro HALT ; … ; .endm */
emit_sm_push_lit_i(e);    /* emits .macro PUSH_INT val ; … ; .endm */
emit_sm_push_lit_s(e);    /* emits .macro PUSH_LIT_S lbl, len ; … ; .endm */
…
emitter_macro_def_destroy(e);
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
`emitter_text.c`'s implementation comments.

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
`emitter_macro_def` and invokes every per-opcode template once.  The
output IS `sm_macros.s`.  The file is committed (for build-without-
generator portability) and `make` regenerates it when any template
changes.

Hand-editing `sm_macros.s` is forbidden after the retrofit completes
— a header comment in the regenerated file says so.

---

## SM and BB are co-equal under this architecture

**Correction to earlier framing (sess 2026-05-11, second pass with Lon):**
SM and BB are not phased — SM first, BB deferred.  They are co-equal
from day one.  Every SM instruction gets its own template C function;
every BB box gets its own template C function.  They share the
`emitter_t` surface, the backends, and the same one-file-per-emission-unit
discipline.

File-system layout:

```
src/runtime/x86/
    emitter.h                          shared surface
    emitter_binary.c                   binary backend (writes x86 bytes)
    emitter_text.c                     text backend (writes GAS asm text)
    emitter_macro_def.c                macro-definition backend (regen sm_macros.s)
    templates/
        sm_halt.c                     ┐
        sm_push_lit_i.c               │
        sm_push_lit_s.c               │  one C file per SM opcode
        sm_jump.c                     │  (≈40 files when complete)
        sm_jump_s.c                   │
        sm_jump_f.c                   │
        sm_call_fn.c                  │
        sm_return.c                   │
        sm_nreturn.c                  │
        sm_freturn.c                  │
        sm_add.c, sm_sub.c, …         │
        sm_pat_lit.c                  │
        sm_pat_capture.c              │
        …                             ┘
        bb_xchr.c                     ┐
        bb_xspnc.c                    │
        bb_xanyc.c                    │  one C file per BB box
        bb_xbrkc.c                    │  (~25 files when complete)
        bb_xnnyc.c                    │
        bb_xlnth.c                    │
        bb_xtb.c, bb_xrtb.c           │
        bb_xfnce.c                    │
        bb_xdsar.c, bb_xatp.c         │
        bb_xbrkx.c                    │
        bb_xcallcap.c                 │
        bb_xcat.c, bb_xor.c           │
        bb_xstar.c, bb_xfarb.c        │
        bb_xposi.c, bb_xrpsi.c        │
        bb_xnme.c, bb_xfnme.c         │
        bb_xarbn.c, bb_xfail.c        │
        …                             ┘
```

Naming convention `sm_<opcode>.c` / `bb_<box>.c` so a directory
listing reads like the instruction set itself.  Both kinds live side
by side because they share the surface; there is no architectural
reason to fork them into separate trees.

The existing `bb_emit.c` EMIT_TEXT/EMIT_BINARY mode switch collapses
into `emitter_binary` / `emitter_text` — one vtable across SM and BB.
The existing per-box C functions in `bb_boxes.c` / `bb_flat.c` migrate
one box at a time into their per-box template file.

---

## macro_def is SM-only — the one asymmetry

⛔ **Co-equal does not mean symmetric on every backend.**  Among the
three backends, `macro_def` is **SM-only**.  BB box templates never
call `macro_begin` / `macro_end` / `macro_param_ref`, and the
`emitter_macro_def` driver never instantiates a BB template.

Why: `sm_macros.s` is a library of parameterized GAS `.macro`
definitions (`HALT`, `PUSH_INT val`, `PUSH_LIT_S lbl, len`, …) that
mode-4's per-call-site emission invokes by name to keep `.s`
readable.  Each SM opcode maps to one macro; the macro body is the
instruction sequence that opcode lowers to.  BB boxes have no such
macro layer: each BB box (whether emitted as a flat-glob inlined
into a pattern's `.text` blob or as a dispatched proc) appears in
the `.s` file as straight-line asm with α/β/γ/ω labels — no `.macro`
wrapper, no parameter substitution.  A BB box's parameters (the
literal bytes it matches, the cset it spans, the length to advance)
are baked into the box's DATA section at emit time, not passed as
macro arguments.

Concretely:

- `templates/sm_halt.c` calls `macro_begin / macro_param_ref /
  macro_end` — three backends produce three different things from it.
- `templates/bb_xchr.c` calls `bb_port_label`, instruction primitives,
  and comments — **two backends** (binary, text) produce two things.
  The macro_def driver never invokes a `bb_*.c` template.

| Backend            | Invoked on SM templates | Invoked on BB templates |
|--------------------|:-----------------------:|:-----------------------:|
| `emitter_binary`    | yes — mode-3            | yes — mode-3            |
| `emitter_text`      | yes — mode-4            | yes — mode-4            |
| `emitter_macro_def` | yes — sm_macros.s regen | **no — never**          |

The `emitter_macro_def` driver's main loop iterates over the SM
opcode-template list only.  There is no `bb_macros.s` regeneration
counterpart; if `bb_macros.s` exists in the tree as a hand-edited
file, it stays hand-edited (or, more likely, the BB-side
macro file goes away entirely after the retrofit since flat-glob
emission writes everything inline).

The `macro_begin / macro_end / macro_param_ref` slots on the
`emitter_t` struct exist for SM templates' use.  BB templates simply
don't call them.  Static analysis can confirm: `grep macro_ templates/bb_*.c`
should return zero hits at any point in the project's life.

---

## The "sprinkle" model — generous surface, per-backend ignore rights

A template C function can issue *any* call on the `emitter_t` surface,
not just instruction emission.  Comments, banners, blank lines,
column breaks, section markers, structural assertions — all of these
are first-class operations on the surface.  Each backend chooses
which calls to implement and which to no-op:

| Call category               | binary backend | text backend | macro_def backend (SM templates only) |
|-----------------------------|:--------------:|:------------:|:-----------------:|
| Instruction (`mov_reg_imm64`, `call_plt`, `ret`, etc.) | emits bytes | emits mnemonic line | emits inside `.macro` body |
| Symbolic label              | records offset | writes label line | writes label line |
| `pc_label`                  | records offset for patching | writes `.LN:` line | n/a |
| `comment`                   | NO-OP          | writes `# …`  | writes `# …` |
| `banner`                    | NO-OP          | writes 120-char rule + text | usually NO-OP |
| `blank_line`                | NO-OP          | emits blank line | NO-OP |
| `column_break`              | NO-OP          | aligns next call to next column | NO-OP |
| `section_marker(".text")`   | NO-OP (always SEG_CODE) | writes `.section .text` | NO-OP |
| `pad_to_blob_size`          | emits NOPs     | NO-OP         | NO-OP |
| `macro_begin/end`           | NO-OP          | NO-OP in invocation mode; emits `.macro/.endm` in definition mode (same as macro_def) | emits `.macro/.endm` |
| `macro_param_ref(name)`     | substitutes the bound value | writes the bound value (invocation) or `\<name>` (definition) | writes `\<name>` |
| `bb_port_label` / `bb_port_jmp` | records offset / emits jmp | writes port-named label / `jmp <port>` | **never reached** — BB templates never call macro_def |
| `bb_box_banner`             | NO-OP          | writes 120-char #---- + "BOX <kind>(<args>) [<prefix>]" | **never reached** — same reason |

The template author writes the *full intent* of an opcode or box —
its instructions, its commentary, its visual layout, its
structural assertions about column alignment — in one place.  Each
backend renders the calls it cares about and ignores the rest.  This
is what "sprinkle" means: a template's body can have as much
cosmetic and explanatory content as the author wants; the binary
backend silently drops it; the text backend honors it; the macro-def
backend selects per-call-type.

Reading a template C file therefore gives a reviewer the *complete*
picture of how the opcode is emitted in every production — including
the comments and formatting that will appear in `.s`.  No need to
cross-reference a separate formatting layer.

A worked example for SM_HALT showing the sprinkle:

```c
/* sm_halt.c — SM_HALT template.  Sprinkle: comments, banner, blob-pad.
 * Each backend implements what it cares about. */

#include "../emitter.h"

void emit_sm_halt(emitter_t *e)
{
    e->macro_begin(e, "HALT", NULL, 0);          /* macro_def: emits ".macro HALT"
                                                    text/invocation: NO-OP
                                                    binary: NO-OP */
    e->comment(e, "SM_HALT — exit sm_jit_run via ret");
                                                  /* text: writes "# SM_HALT — …"
                                                    binary: NO-OP
                                                    macro_def: writes "# SM_HALT — …" */

    e->inc_mem_disp8(e, REG_R13, 20);            /* pc++ : 4 bytes / "inc dword [r13+20]"
                                                    / "inc dword [r13+20]" inside macro body */

    e->ret(e);                                    /* return to sm_jit_run's frame:
                                                    1 byte / "ret" / "ret" */

    e->pad_to_blob_size(e);                       /* binary: NOP-pads to ME3_BLOB_SIZE
                                                    text: NO-OP
                                                    macro_def: NO-OP */

    e->macro_end(e);                              /* macro_def: emits ".endm"
                                                    others: NO-OP */
}
```

Read top-to-bottom that's a complete description of SM_HALT for every
backend.  Every meaningful behavior is on the page; the layout is the
intent.

---

## Cross-language future

Per Lon (Q5 sess 2026-05-11) and ARCH-SCRIP.md mode 4: x86 today;
JVM, .NET, JS, WASM, C tomorrow.  When that work begins, the `emitter_t`
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
   `src/runtime/x86/emitter.h` with the struct.  Three skeleton backend
   impls: `emitter_binary.c`, `emitter_text.c`, `emitter_macro_def.c`
   (last is a thin wrapper over text in DEFINITION mode).  Create
   `src/runtime/x86/templates/` directory.  Wire into Makefile.
   Nothing calls them yet.  Gates green.

3. **EM-MODE4-IS-MODE3-DUMP-c — first SM opcode end-to-end: SM_HALT.**
   New file `src/runtime/x86/templates/sm_halt.c`.  Wire mode-3's halt
   blob through it (replaces `emit_halt_blob` inline writes in
   `sm_codegen.c`).  Wire mode-4's halt emission through it (replaces
   `emit_sm_halt` in `sm_codegen_x64_emit.c`).  Wire `sm_macros.s`
   HALT macro regeneration through it (new generator tool
   `tools/regen_macros.c` invoked at build time).  New gate
   `test_gate_em_template_byte_identity.sh` validates mode-3 bytes ==
   mode-4-asm-then-bytes on a trivial HALT-exercising program.  Gates
   green.

4. **EM-MODE4-IS-MODE3-DUMP-d — first BB box end-to-end: bb_xchr.**
   New file `src/runtime/x86/templates/bb_xchr.c`.  Wire `bb_flat.c`'s
   `flat_emit_*_xchr` calls through it.  XCHR is the simplest box
   (one-character literal compare) — proves the BB-side vtable surface
   works.  Existing `bb_emit.c` EMIT_TEXT/EMIT_BINARY mode switch
   delegates to the new vtable for this box; once all boxes are
   migrated the mode switch deletes.  Gates green.

5. **EM-MODE4-IS-MODE3-DUMP-e through -p — one emission unit per
   rung**, alternating between SM opcodes and BB boxes so the gate
   surface gets a workout from both directions.  Suggested order
   (refine on entry):

   | Rung | Unit | Type |
   |------|------|------|
   | -e | `sm_push_lit_i.c` | SM |
   | -f | `bb_xlit.c` | BB (literal byte sequence) |
   | -g | `sm_push_lit_s.c` | SM |
   | -h | `bb_xspnc.c` | BB |
   | -i | `sm_void_pop.c` | SM |
   | -j | `bb_xanyc.c` | BB |
   | -k | `sm_jump.c` | SM |
   | -l | `bb_xbrkc.c` | BB |
   | -m | `sm_jump_s.c` + `sm_jump_f.c` | SM (one rung covers both) |
   | -n | `bb_xnnyc.c` | BB |
   | -o | arithmetic family: `sm_add.c sm_sub.c sm_mul.c sm_div.c sm_mod.c sm_exp.c` | SM (one rung) |
   | -p | `bb_xlnth.c`, `bb_xtb.c`, `bb_xrtb.c` | BB (one rung — they share structure) |

   Each rung lands its own template C file(s) under `templates/`.
   Each deletes the corresponding inline byte writes from
   `sm_codegen.c` (or `bb_flat.c` / `bb_boxes.c` for BB) AND the
   corresponding per-opcode/per-box function in
   `sm_codegen_x64_emit.c` (or `bb_emit.c`) AND the corresponding
   hand-maintained macro in `sm_macros.s` / `bb_macros.s`.

6. **EM-MODE4-IS-MODE3-DUMP-q — SM_LABEL / SM_STNO.**  Structural
   markers; one rung.

7. **EM-MODE4-IS-MODE3-DUMP-r — SM_CALL_FN.**  The big SM rung; uses
   `lea_rip_sym`, `call_plt`, expression-registry interaction.

8. **EM-MODE4-IS-MODE3-DUMP-s — SM_RETURN / SM_NRETURN / SM_FRETURN
   family**, including conditional variants.  Once this lands, the
   ABI alignment work the earlier watermark cycled through (thunks,
   `push rbp`, etc.) is decided inside ONE template file — fixing
   alignment in one place fixes it for both mode-3 and mode-4 by
   construction.

9. **EM-MODE4-IS-MODE3-DUMP-t — remaining BB boxes.**  XFNCE, XDSAR,
   XATP, XBRKX, XCALLCAP, XCAT, XOR, XSTAR, XFARB, XPOSI, XRPSI,
   XNME, XFNME, XARBN, XFAIL.  Bundle in 2-3 rungs by structural
   similarity.

10. **EM-MODE4-IS-MODE3-DUMP-u — pattern SM opcodes** (`SM_PAT_*`).
    Same discipline.

11. **EM-MODE4-IS-MODE3-DUMP-v — `sm_macros.s` and `bb_macros.s`
    become generated artifacts**, make rule added, files dropped from
    git in favor of build-time regen (or kept with a "DO NOT EDIT —
    generated" header).

12. **EM-MODE4-IS-MODE3-DUMP-w — rung close.**  Run
    `test_gate_em_beauty_subsystems_mode4.sh` — gate must improve from
    baseline (PASS=4 FAIL=13) since byte-identity is by construction.
    Capture new baseline.  Delete `emitter_t.h` / `emitter_text.c` /
    `emitter_binary.c` (the BB-side legacy vtable subsumed by
    `emitter.h`).  Delete `sm_emit_template.c` / `sm_emit_template.h`
    (the SM-side macro-renderer subsumed by templates +
    `emitter_text.c`).  File-count delta proves consolidation.

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

---

## BB template correct shape (sess 2026-05-11, corrected)

**What the byrd-reference `.s` files show:** each box port is ONE macro invocation:

```
seq_l0_α:   RPOS_α  1, cursor, subject_len_val, seq_r0_α, P_3_ω  ; RPOS(1)
seq_l0_β:   RPOS_β  cursor, P_3_ω
```

The macro body in `bb_macros.s` contains the actual instructions. The template does NOT emit raw instructions — it emits one port-call per port through the vtable. The binary backend emits bytes; the text backend emits the macro invocation line. Same vtable call, different backend rendering.

**Correct BB template shape:**

```c
void emit_bb_xrpsi(emitter_t *e, int n,
                   bb_label_t *lbl_succ, bb_label_t *lbl_fail, bb_label_t *lbl_β)
{
    EMIT_OPT(e, bb_box_banner, e, "RPOS", n);        /* banner + comment */
    e->bb_port_alpha(e, "RPOS", n, lbl_succ, lbl_fail); /* α port: one macro call */
    EMIT_LABEL(e, lbl_β);
    e->bb_port_beta(e,  "RPOS", lbl_fail);           /* β port: one macro call */
}
```

**What went wrong in sub-rungs -d through -m:** templates emitted raw instructions for the binary path and delegated text to callbacks into `bb_flat.c`. This inverted the design. The fix is `EM-TEMPLATE-PURITY` in `GOAL-MODE4-EMIT.md`.
