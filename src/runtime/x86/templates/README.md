# src/runtime/x86/templates/ — per-opcode emission templates

This directory holds **one C file per SM opcode and per BB box kind**.
Each template is a single function that describes the opcode's effect
in mnemonics, comments, and structural markers, walked through the
`emitter_t` vtable (`emitter.h`).  Three backends consume each template:

| Backend           | Output             | Consumer                       |
|-------------------|--------------------|--------------------------------|
| `emitter_binary`  | x86-64 bytes       | mode-3 in-process JIT          |
| `emitter_text`    | GAS Intel-syntax   | mode-4's per-call-site `.s`    |
| `emitter_macro_def` | `.macro NAME ... .endm` body | `sm_macros.s` regeneration (SM only) |

`emitter_macro_def` is SM-only — BB templates have no `.macro` layer
and never call into the macro_def surface.

Per the design doc (`one4all/MIGRATION-MODE4-IS-MODE3-DUMP.md`), templates
are the single source of truth for each opcode's emission.  Today's
parallel emitters (`sm_codegen.c` bytes; `sm_codegen_x64_emit.c` text)
will be retired opcode-by-opcode as templates land.

## Status

| Sub-rung | Landed | Templates added |
|----------|--------|-----------------|
| -b (vtable skeleton)   | yes  | none — surface only |
| -c (SM_HALT)           | open | `sm_halt.c` |
| -d (bb_xchr)           | open | `bb_xchr.c` |
| -e..-p alternating     | open | one per rung |

## Naming

| Template kind | File prefix | Function name        | Example |
|---------------|-------------|----------------------|---------|
| SM opcode     | `sm_`       | `emit_sm_<opcode>`   | `sm_halt.c` → `emit_sm_halt` |
| BB box kind   | `bb_`       | `emit_bb_<kind>`     | `bb_xchr.c` → `emit_bb_xchr` |

## Sprinkle model

A template may call any `emitter_t` method — instruction emission, comment,
banner, blank line, formatting marker, structural marker, BB-port primitive,
macro hook.  Each backend implements or no-ops each call per the design.
Templates do not branch on the backend; the *backend* picks what to do
with each call.

## Static-analysis invariant

`grep macro_ src/runtime/x86/templates/bb_*.c` must always return empty.
If it ever returns hits, a BB template is illegally calling into the
SM-only macro surface; the template must be rewritten without those calls.

## Cross-references

- `one4all/MIGRATION-MODE4-IS-MODE3-DUMP.md` — full design.
- `.github/GOAL-MODE4-EMIT.md` — active rung and sub-rung list.
- `.github/ARCH-x86.md` — backend architecture.
- `.github/ARCH-SCRIP.md` — execution mode definitions.
