# CHUNKS-step17b-validation — sm_lower emits named-chunk SKELETONS

**Session #75 (continued), 2026-05-07.  Watermark: post-CH-17a
(one4all `0cb31ca4`).**

CH-17b is the second rung of `GOAL-CHUNKS-STEP17.md`.  Spec was
scope-reduced from "skeleton + body" to "skeleton only" mid-session,
splitting body lowering into a separate rung CH-17b'.

## What landed

`sm_lower` (in `src/runtime/x86/sm_lower.c`) gains a top-of-function
loop, before the main statement loop, that emits a chunk skeleton
for every entry currently in `proc_table`:

```c
for (int pi = 0; pi < proc_count; pi++) {
    const char *nm = proc_table[pi].name;
    if (!nm || !*nm) continue;
    int skip_jump = sm_emit_i(p, SM_JUMP, 0);
    sm_label_named(p, nm);
    sm_emit(p, SM_RETURN);
    int skip_lbl = sm_label(p);
    sm_patch_jump(p, skip_jump, skip_lbl);
}
```

This emits, per proc:

```
SM_JUMP <skip>      ; forward-jump around the chunk
SM_LABEL "<name>"   ; named entry (recorded by sm_label_named for
                    ; sm_label_pc_lookup); CH-17a's resolver finds
                    ; this and writes proc_table[pi].entry_pc = pc
SM_RETURN           ; empty body — CH-17b' adds real body lowering
<skip>:             ; anonymous skip target
```

`sm_lower.c` now `#include`s `coro_runtime.h` to reach `proc_table`
and `proc_count` (populated by `polyglot_init` in `sm_preamble`,
which runs before `sm_lower`).

## Empirical reach

```
$ ./scrip --interp --dump-sm test/icon/hello.icn
; SM_Program  count=6
   0  SM_JUMP              -> 3
   1  SM_LABEL
   2  SM_RETURN
   3  SM_LABEL
   4  SM_BB_PUMP_PROC
   5  SM_HALT

$ SCRIP_PROC_ENTRY_PCS=1 ./scrip --interp test/icon/hello.icn
[CH-17a] resolve entry_pcs (proc_table=1 procs, pl_pred_table=hash)
[CH-17a]   proc[0] name=main                 entry_pc=1
[CH-17a] summary: pl_total=0 pl_resolved=0 ...
Hello, World!

$ SCRIP_PROC_ENTRY_PCS=1 ./scrip --interp test/raku/rk_given.raku
[CH-17a] resolve entry_pcs (proc_table=3 procs, pl_pred_table=hash)
[CH-17a]   proc[0] name=day_type             entry_pc=1
[CH-17a]   proc[1] name=season               entry_pc=5
[CH-17a]   proc[2] name=main                 entry_pc=9
```

CH-17a's resolver — the half-API laid down last rung — now finds
entry_pcs for every Icon and Raku proc.  The resolver-to-emitter
end-to-end is verified.

The `--dump-sm` (without `--interp`) path doesn't invoke
`polyglot_init` and so doesn't see `proc_table` populated; the
skeleton emission is conditional on what `proc_table` contains.
This is correct: emission keys off the existing `polyglot_init` →
`sm_lower` ordering in `sm_preamble`.

## Why skeleton-only

Body lowering was originally bundled into CH-17b in
GOAL-CHUNKS-STEP17.md.  Mid-session inspection revealed:

1. Icon proc bodies are EXPR_t chains (not STMT_t), so they need
   `lower_expr` recursion, not `lower_stmt`.
2. Frame-slot resolution via `icn_scope_patch` happens at runtime
   inside `coro_call` today, mutating `E_VAR.ival` in place.
   Migrating that to lower-time is its own architectural decision.
3. Body lowering hits generator kinds (E_EVERY, E_SUSPEND, etc.)
   that fire SM_PUSH_EXPR — overlapping CH-15b's territory.

Skeleton-only:
- Lands the entry_pc-resolution end-to-end test (resolver finds
  real pcs, not -1).
- Lands ahead of CH-17c (consumer flip) so CH-17c has a real
  entry_pc to migrate to.
- Risks nothing because the chunks are forward-jumped over and
  unread.

CH-17b' covers body lowering as a separate rung.

## What this rung does NOT do

- Does NOT lower proc bodies.  The `SM_RETURN` immediately after
  `SM_LABEL` makes each chunk a no-op.
- Does NOT change consumer code.  `coro_call`, `coro_drive`,
  `coro_value`, `pl_box_choice`, `interp_hooks`, `polyglot.c`'s
  Icon and Prolog dispatch still walk `EXPR_t *proc` /
  `EXPR_t *choice` exactly as before.  CH-17c flips Icon/Raku
  consumers; CH-17e flips Prolog.
- Does NOT touch Prolog predicate emission.  CH-17d does that.
- Does NOT delete the `EXPR_t *proc` field.  CH-17g does that.

## Files touched (1)

- `src/runtime/x86/sm_lower.c` — new include + skeleton-emission
  loop at top of `sm_lower`.

22 lines added.  No deletions; no signatures changed.

## Gates

  - smoke ×6 PASS (SNOBOL4 7/7, Icon 5/5, Prolog 5/5, Raku 5/5,
    Snocone 5/5, Rebus 4/4) — all byte-identical to baseline
  - isolation gate PASS
  - `unified_broker` PASS=49 — byte-identical to baseline
  - `test_crosscheck_icon` PASS=4 FAIL=0 SKIP=0
  - `SCRIP_CHUNKS_AUDIT=1` regressions: zero — Icon hello still
    audits SM_PUSH_CHUNK=0 SM_PUSH_EXPR=0; Raku rk_given still
    audits SM_PUSH_CHUNK=20 SM_PUSH_EXPR=0 (CH-13 baseline).

Full Icon corpus (186/47/30 of 263) baseline not run in this
session — CH-17b is sufficiently small and audit-clean that a full
corpus pass adds little.  CH-17c (consumer-side flip) is when the
full corpus baseline becomes load-bearing.

## Closed-rung pointer

CH-17b closes.  Next inline: **CH-17b'** — lower Icon/Raku proc
bodies into the chunks (replace the immediate `SM_RETURN` with
real lowered SM ops).

one4all @ HEAD pre-rung: `0cb31ca4`.  Session #75, 2026-05-07.
