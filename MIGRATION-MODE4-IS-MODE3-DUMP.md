# MIGRATION-MODE4-IS-MODE3-DUMP.md — template / t_* model

**Status:** implementation underway.  Sub-rungs -a through -q complete.
See `GOAL-MODE4-EMIT.md` for active rung and gate counts.

---

## The principle

ARCH-x86.md §"Stack machine (SM_Program)" says:

> EMITTER walks same SM_Program → native code.  One instruction set.
> No divergence between interpreter and emitter.

One template C function per SM opcode and per BB box is the mechanism.
Three output modes (BINARY, TEXT, MACRO_DEF) are driven by one global
switch — `bb_emit_mode` in `bb_emit.h`.  There is no vtable; there are
no `e->method(e, ...)` calls inside template bodies.

---

## The t_* model

Templates call **`t_*` free-standing helpers** declared in `bb_emit.h`.
Each helper reads `bb_emit_mode` and routes its output accordingly:

| `bb_emit_mode`   | Output                          | Consumer                    |
|------------------|---------------------------------|-----------------------------|
| `EMIT_BINARY`    | raw x86-64 bytes into SEG_CODE  | mode-3 in-process JIT       |
| `EMIT_TEXT`      | GAS asm text to a `FILE*`       | mode-4 `.s` file            |
| `EMIT_MACRO_DEF` | `.macro NAME … .endm` body      | `sm_macros.s` regeneration  |

`EMIT_MACRO_DEF` is **SM templates only** — BB templates never call
`t_macro_begin` / `t_macro_end`, and `EMIT_MACRO_DEF` is never set
when emitting BB boxes.

### Template signature

```c
void emit_sm_foo(emitter_t *e, /* opcode-specific args */);
void emit_bb_bar(emitter_t *e, /* box-specific args */);
```

`emitter_t *e` appears on every signature for call-site compatibility
but is **unused inside the body** — always `(void)e;` at the top.
Do not call through it.  The `t_*` helpers are the only output path.

### Canonical SM template shape

```c
#include "../emitter.h"   /* unused emitter_t * param */
#include "../bb_emit.h"

void emit_sm_push_lit_i(emitter_t *e, int64_t val)
{
    (void)e;

    t_comment("SM_PUSH_LIT_I — push integer literal");

    static const char *const params[] = { "val" };
    t_macro_begin("PUSH_INT", params, 1);   /* MACRO_DEF: .macro PUSH_INT val */

    t_mov_rdi_imm64((uint64_t)val);         /* BINARY: movabs rdi,val  TEXT: mov rdi, val */
    t_call_sym_plt("rt_push_int", 0);       /* BINARY: call via rax    TEXT: call rt_push_int@PLT */

    t_macro_end();                          /* MACRO_DEF: .endm         others: no-op */
    t_pad_to_blob_size();                   /* BINARY: NOP-pad          others: no-op */
}
```

### Current t_* surface (bb_emit.h)

```
t_comment(text)
t_bb_box_banner(kind, args)
t_inc_mem_r13_disp8(disp)
t_ret()
t_pad_to_blob_size()
t_mov_rdi_imm64(val)
t_call_sym_plt(sym, fn_fallback)
t_macro_begin(name, params, nparams)
t_macro_end()
t_test_rax_rax()
t_emit_jmp(target, kind)
t_noop_macro(macro_name)
t_banner_stno(stno, lineno, src_text)
t_lea_rdi_strtab_sym(sym_label, in_proc_ptr)
t_mov_esi_imm32(val)
t_mov_edi_imm32(val)
t_test_eax_eax()
t_jz_retskip(pc)
t_retskip_label(pc)
```

New helpers are added to `bb_emit.h` / `bb_emit.c` as each rung requires.

---

## File layout

```
src/runtime/x86/
    emitter.h                  (emitter_t struct — parameter compat only)
    bb_emit.h / bb_emit.c      t_* helpers + bb_emit_mode switch
    templates/
        sm_halt.c              one C file per SM opcode
        sm_push_lit_i.c
        sm_void_pop.c
        sm_jump.c
        sm_arith.c
        sm_nullary_rt.c
        sm_label_stno.c
        sm_call_fn.c
        sm_return.c
        …
        bb_xchr.c              one C file per BB box
        bb_xspnc.c
        bb_xlnth.c
        bb_xbrkx.c
        bb_xposi.c
        bb_xfarb.c
        …
```

---

## Sequencing

Sub-rungs land one emission unit at a time, alternating SM and BB.
Each rung deletes the corresponding inline byte-writes from
`sm_codegen.c` (or `bb_flat.c`) AND the corresponding per-opcode
function in `sm_codegen_x64_emit.c`.

Active rung and remaining work: `GOAL-MODE4-EMIT.md`.

---

## Gates (every rung before commit)

| Gate                                      | Required |
|-------------------------------------------|----------|
| `test_smoke_snobol4.sh`                   | PASS=7   |
| `test_smoke_unified_broker.sh`            | PASS=49  |
| `test_gate_em_template_byte_identity.sh`  | PASS=4/4 |
| 5 tracked artifacts `gcc -c` clean        | yes      |

---

## Why this is right

One template per opcode.  One `bb_emit_mode` switch.  Mode-3, mode-4,
and `sm_macros.s` regeneration all use the same template bodies.
Divergence between modes is impossible by construction — there is only
one place to read or fix any opcode's emission.
