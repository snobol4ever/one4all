# CHUNKS Step 13 — Raku CASE migrated to chunk dispatch

**Date:** 2026-05-06 · **Session:** continuing CH-13 after CH-12 close
**HEAD before:** one4all @ `0a38d055` (post-CH-12)

## Scope

Migrate `sm_lower.c:1062` (E_CASE / Raku given/when) away from the
legacy `emit_push_expr + SM_BB_PUMP` wrapper to a chunk-based
dispatch.  The wrapper-level synthesis becomes EXPR_t-free; per-arm
body content is recursively lowered via the existing `lower_expr`
machinery into chunks.

This rung mirrors the CHUNKS-step12 template: a new opcode
(`SM_BB_PUMP_CASE`) replaces the SM_PUSH_EXPR + SM_BB_PUMP pair, with
the runtime-side dispatch helper inline in `sm_interp.c`'s and
`sm_codegen.c`'s opcode handlers.  No EXPR_t is constructed at lowering
time and none is walked by either handler.

## What changed

### Producer — `src/runtime/x86/sm_lower.c`

The `case E_CASE:` branch in `lower_expr` previously emitted:

```c
emit_push_expr(p, e);
sm_emit(p, SM_BB_PUMP);
```

It now emits a canonical CASE wrapper:

  1. The topic (child[0]) lowered as a chunk → `SM_PUSH_CHUNK`.
  2. For each non-default arm, a triple pushed:
     - `SM_PUSH_LIT_I cmp_kind` (E_LEQ for `==`, else E_EQ),
     - `SM_PUSH_CHUNK` of the arm value,
     - `SM_PUSH_CHUNK` of the arm body.
  3. If a default is present, `SM_PUSH_CHUNK` of the default body.
  4. `SM_BB_PUMP_CASE ncases, has_default`.

Each chunk follows the same forward-jump-around shape used by
Steps 2/3/4 (`SM_JUMP skip / entry / lower_expr child / SM_RETURN /
skip-label / SM_PUSH_CHUNK entry_pc, 0`).

The Icon-style pair-layout fall-through (preserved for any future
Icon producer; not exercised by the Raku frontend today) emits a
defensive `SM_PUSH_NULL` thunk + zero-arm `SM_BB_PUMP_CASE` and
returns.  When Icon E_CASE is added in a later rung, this branch
becomes that rung's territory.

### Opcodes — `src/runtime/x86/sm_prog.h`, `sm_prog.c`

New opcode `SM_BB_PUMP_CASE` added between `SM_BB_PUMP_PROC` and the
`/* Functions */` group.  Operands: `a[0].i = ncases`, `a[1].i =
has_default`.  Name registered in `opnames[]`.

### Interpreter handler — `src/runtime/x86/sm_interp.c`

New `case SM_BB_PUMP_CASE:` after the SM_BB_PUMP_PROC handler.  Pops
the default-body descriptor (if `has_default`), then ncases triples
in reverse — accumulating into source-ordered C arrays — then pops
the topic descriptor.  Evaluates the topic chunk via `sm_call_chunk`,
walks arms running each value chunk and comparing against topic per
the cmp_kind (E_LEQ → string equality; else integer-or-string per
`IS_INT_fn`/`VARVAL_fn`), runs the matching body chunk on first hit,
or the default body if no arm matched.  Result pushed onto value
stack; `last_ok` reflects whether any arm or default matched.

The comparison logic mirrors `coro_value.c:947`'s E_CASE evaluator
verbatim — the two paths must remain semantically identical.

### Codegen handler — `src/runtime/x86/sm_codegen.c`

New `h_bb_pump_case()` registered as `g_handlers[SM_BB_PUMP_CASE]`.
Logic identical to the sm_interp.c handler, using the codegen-side
`POP()`/`PUSH()`/`STATE`/`CUR_INS` macros.  The two handlers are
explicit lockstep mirrors; any change to one needs the same change
to the other.

## Validation

### Audit-counter sweep

`SCRIP_CHUNKS_AUDIT=1 ./scrip --interp test/raku/rk_given.raku`:

```
[CHUNKS-AUDIT] summary: SM_PUSH_CHUNK=20  SM_PUSH_EXPR=0  out_of_range=0
```

20 chunk pushes accounting:

  * Both `day_type` and `season` define a CASE with 4 arms +
    1 default each.
  * Per arm: 1 val chunk + 1 body chunk = 2 chunks (the cmp_kind is
    `SM_PUSH_LIT_I`, not a chunk).
  * Per CASE: 1 topic chunk + 4 × 2 arm chunks + 1 default chunk = 10.
  * Two CASEs total = 20 chunks.

Counts match exactly.  `SM_PUSH_EXPR=0` empirically proves the
wrapper-level synthesis is now EXPR_t-free for the Raku CASE path.

### Pre/post execution comparison on `rk_given.raku`

| Mode | Pre-CH-13 | Post-CH-13 |
|------|-----------|------------|
| `--ir-run` | `Mon: weekday\nSat: weekend\nweekday\nhot\ncold\nunknown` (PASS) | unchanged (PASS) |
| `--interp` | `sm_interp: stack underflow` (Aborted) | reaches default branch in all 6 calls; outputs "weekday" twice (broken at deeper Raku-SM-mode level — the surrounding `say`/sub infrastructure is broken; this is pre-existing per the Raku full-suite baseline 29/0/0 across IR/SM/JIT) |
| `--run` | `sm_interp: stack underflow` (Aborted) | same as `--interp` — no longer underflows |

The CASE wrapper itself no longer underflows.  Full Raku --interp /
--run remains broken at the surrounding-infrastructure level
(`say(1+2)` already fails the same way pre-CH-13); fixing that is
NOT CH-13's scope and is consistent with the Raku full-suite baseline.

### Scope boundary (honest)

CH-13 migrates the **wrapper-level synthesis** only.  The path
`bb_eval_value(E_CASE)` in `coro_value.c:947` — used when a CASE
appears in value context inside a BB-driven evaluator (Icon every
bodies, Prolog clause bodies) — still walks E_CASE's children via
the EXPR_e cmp-kind dispatch.  That path is unreachable from the
SM-mode Raku given-stmt route after CH-13, but lives on for any
future value-context CASE and for Icon if/when E_CASE is added there.

Cleaning up the IR-walking inside `coro_value.c:947` is M4-cleanup
territory, mirroring how CHUNKS-step12 deferred the `coro_call`
proc_table walk to Step 17.

## Gates

All baselines preserved exactly:

| Gate | Pre-CH-13 | Post-CH-13 |
|------|-----------|------------|
| smoke_snobol4 | 7/7 | 7/7 |
| smoke_icon    | 5/5 | 5/5 |
| smoke_prolog  | 5/5 | 5/5 |
| smoke_raku    | 5/5 | 5/5 |
| smoke_snocone | 5/5 | 5/5 |
| smoke_rebus   | 4/4 | 4/4 |
| isolation_ir_sm | PASS | PASS |
| csnobol4 Budne | PASS=36 FAIL=114 | PASS=36 FAIL=114 |
| Icon corpus | 186/47/30 (TOTAL=263) | 186/47/30 (TOTAL=263) |
| unified_broker | PASS=49 | PASS=49 |
| Raku full suite (IR/SM/JIT) | 29/0/0 | 29/0/0 |

Build: clean.

## Status of `emit_push_expr` call sites

After CH-13, the remaining `emit_push_expr` sites in `sm_lower.c`
are at lines 1046, 1057 (the `case E_TRAIL_MARK / E_TRAIL_UNWIND`
fall-through and the `case E_CHOICE / E_CLAUSE / E_CUT / E_UNIFY /
E_TRAIL_MARK / E_TRAIL_UNWIND` Prolog group).  Both belong to
Step 16 (Prolog clauses).  And the helper definition itself at
line 39.  Three call sites total remain.

The other line that previously hit emit_push_expr (line 1064 in the
GOAL-CHUNKS narrative) — the E_CASE site — is now gone.  The four
line numbers cited in the goal text ("lines 1046, 1057 ... 1064")
collapse to two surviving sites + the helper after CH-13.
