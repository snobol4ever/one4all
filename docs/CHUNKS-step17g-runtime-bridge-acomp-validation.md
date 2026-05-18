# CH-17g-runtime-bridge-acomp — SM_ACOMP carries operator EKind; sm_interp gains handler

**Rung:** CH-17g-runtime-bridge-acomp (a "Next rung options" item from
GOAL-CHUNKS-STEP17.md after bridge-4 — option (c): "SM_ACOMP opcode
handler in sm_interp.c — surfaced by queens.icn — small fix unblocks
array-composition under chunks").
**Session:** 2026-05-09
**Predecessor:** CH-17g-runtime-bridge-4
(`docs/CHUNKS-step17g-runtime-bridge-4-validation.md`).

## What this rung does

Closes the runtime gap surfaced by `test/icon/queens.icn` under
`--interp` after bridge-4 widened Icon builtin reach.  Probe sequence:

```
$ ./scrip --interp test/icon/queens.icn
sm_interp: unhandled opcode 82 (SM_ACOMP) at pc=92
-Queens:
```

Two coupled changes — neither is sufficient alone:

1. **Lowering bug (sm_lower.c:859).**  All six numeric comparison
   EKinds (E_EQ, E_NE, E_LT, E_LE, E_GT, E_GE) collapsed onto a
   single `SM_ACOMP` opcode emitted with no argument.  The
   comparator was unrecoverable at runtime; even adding a handler
   would have been incorrect.

2. **Missing handler (sm_interp.c).**  Opcode 82 was registered in
   `sm_prog.c:185` and emitted by sm_lower for every E_EQ/E_NE/etc.
   but had no `case SM_ACOMP:` arm in `sm_interp.c`'s switch — fell
   through to the `default` FATAL.

## Mechanism investigated

Probe under `--interp --dump-sm` for `if i = 0 then write("eq")`
followed by `if i < 1 then write("lt")`:

```
   7  SM_ACOMP             ; (no arg)
   8  SM_JUMP_F            -> 12
  18  SM_ACOMP             ; (no arg)
  19  SM_JUMP_F            -> 23
```

Both `=` and `<` produced byte-identical opcode bytes.  `SM_JUMP_F`
tests `last_ok` (sm_interp.c:276), so the runtime needs `last_ok`
set per the operator's success/failure semantics — but with no
operator info reaching the handler, it cannot be set correctly.

The sm_codegen.c comment that this opcode was "stubbed by design"
because Icon "bypasses sm_lower and goes through icn_runtime.c
directly" was accurate at the time of writing but is now stale:
CH-17b' (sess #78) made sm_lower walk Icon proc bodies, and
chunk-side dispatch (CH-17c, CH-17g-runtime-bridge-1..4) routes
Icon execution through `--interp`.

## Implementation

### Lowering (`src/runtime/x86/sm_lower.c`)

```c
/* before: */
sm_emit(p, SM_ACOMP);   /* leaves -1/0/1 on stack; stmt goto uses it */
/* after: */
sm_emit_i(p, SM_ACOMP, (int64_t)e->kind);
```

`a[0].i` now carries the EKind (E_EQ=67, E_NE=68, E_LT=63, E_LE=64,
E_GT=65, E_GE=66 from `src/ir/ir.h:155–160`).  Comment at the case
arm extended to record the rung and the runtime semantics.

### Runtime handler (`src/runtime/x86/sm_interp.c`)

Inserted directly after `SM_DECR`:

```c
case SM_ACOMP: {
    DESCR_t r = sm_pop(st);
    DESCR_t l = sm_pop(st);
    if (l.v == DT_FAIL || r.v == DT_FAIL) {
        sm_push(st, FAILDESCR); st->last_ok = 0; break;
    }
    if (l.v == DT_SNUL) l = INTVAL(0);
    if (r.v == DT_SNUL) r = INTVAL(0);
    double lv = (l.v == DT_R) ? l.r : (double)((l.v == DT_I) ? l.i : 0);
    double rv = (r.v == DT_R) ? r.r : (double)((r.v == DT_I) ? r.i : 0);
    int op = (int)ins->a[0].i;
    int ok;
    switch (op) {
        case E_EQ: ok = (lv == rv); break;
        case E_NE: ok = (lv != rv); break;
        case E_LT: ok = (lv <  rv); break;
        case E_LE: ok = (lv <= rv); break;
        case E_GT: ok = (lv >  rv); break;
        case E_GE: ok = (lv >= rv); break;
        default:   ok = (lv == rv); break;  /* legacy emit safety net */
    }
    if (ok) { sm_push(st, r);          st->last_ok = 1; }
    else    { sm_push(st, FAILDESCR);  st->last_ok = 0; }
    break;
}
```

**Semantics chosen:** Icon-style relops — on success return the
RIGHT operand (so `every write(2 < (1 to 4))` yields `3, 4`), on
failure return FAILDESCR.  Mirrors the `NUMREL` macro in
`interp_eval.c:3162–3171` line for line.  SNUL→0 coercion matches
the convention of `SM_ADD`/`SM_SUB` (sm_interp.c:454–455).  The
`default` arm exists only as a safety net for any pre-bridge-acomp
SM_Program that might be loaded without an EKind argument; it is
unreachable on freshly lowered code.

### Disassembly (`src/runtime/x86/sm_prog.c`)

`SM_ACOMP` added to the `i=` print case alongside SM_INCR/SM_DECR/
SM_LCOMP/SM_RCOMP/SM_TRIM, so `--dump-sm` shows the operator EKind:

```
   7  SM_ACOMP             i=67   ; E_EQ
  18  SM_ACOMP             i=63   ; E_LT
```

### Stale comment (`src/runtime/x86/sm_codegen.c`)

The "stubbed by design" comment at line 1177 lumped SM_ACOMP and
SM_LCOMP together with the Icon-bypasses-sm_lower rationale.  That
rationale no longer holds for SM_ACOMP (CH-17b' onward) and was
about to be wrong about SM_LCOMP too — comment refreshed to remove
SM_ACOMP from the stubbed list, narrow the SM_LCOMP entry, and
note SM_LCOMP as a follow-on rung.  JIT codegen for SM_ACOMP is M5
territory (named-FATAL pattern) — still h_unimpl in the JIT,
intentional, the interpreter handler is what unblocks `--interp`.

## Why SM_LCOMP was NOT migrated in this rung

Same shape bug exists in `sm_lower.c:872` (string compare collapses
E_LLT/E_LLE/E_LGT/E_LGE/E_LEQ/E_LNE onto a single argument-less
SM_LCOMP) and `sm_interp.c` has no handler for opcode 80 either.
Bridge-acomp is scoped to numeric — string relops are a separate
follow-on (call it bridge-lcomp).  Discipline: small contained
rung, one observable problem at a time.  No corpus test surfaces
SM_LCOMP today (no Icon program in `test/icon` reaches a string
relop under `--interp` after the bridge family); when one does it
will arrive as the next surface.

## What this does and does not unblock

**Unblocks immediately:**
- `queens.icn` under `--interp` no longer FATALs on opcode 82
  (now runs to "0 solutions total." — a different correctness
  surface, not a SM_ACOMP issue; queens.icn under `--ir-run`
  ALSO produces wrong output, "Error 3 ... Erroneous array or
  table reference" — pre-existing).
- Any Icon proc body using `=`, `~=`, `<`, `<=`, `>`, `>=` between
  numerics dispatches correctly under `--interp`.
- Trivial probes:
  ```
  $ cat /tmp/probe_eq.icn
  procedure main()
      i := 0;
      if i = 0 then write("eq");
      if i < 1 then write("lt");
  end
  $ ./scrip --interp /tmp/probe_eq.icn
  eq
  lt
  ```

**Does NOT unblock (out of scope):**
- String relops via SM_LCOMP — same shape bug, deferred.
- `if`-statement value leak under `--interp` (a stray operand
  appears on stdout after `if-then` cases) — pre-existing,
  reproducible without any SM_ACOMP path; unrelated to this rung.
- queens.icn correctness — independent issue (array indexing).
- Other bridge-4 surfaced gaps (`tab`/`move`/`find`/scan-context;
  CH-17g-irrun-lowers).

## Gates

All gates byte-identical to baseline (CH-17g-runtime-bridge-4 @ `5e526155`):

| Gate | Result |
|------|--------|
| Smoke ×6 (snobol4/icon/prolog/raku/rebus/snocone) | PASS=7/5/5/5/4/5, FAIL=0 |
| `test_isolation_ir_sm.sh` | PASS — no IR-only symbol leaks in SM runtime files |
| `test_smoke_unified_broker.sh` | PASS=49 FAIL=0 |
| `test_smoke_scrip_all_modes.sh` | PASS=2 FAIL=0 |
| Icon `--ir-run` corpus (test_icon_all_rungs.sh) | PASS=186 FAIL=47 XFAIL=30 TOTAL=263 |

**New gate:** `queens.icn --interp` no longer FATALs on opcode 82 — PASS.

**New gate:** `if i = 0` / `if i < 1` probe under `--interp`
produces `eq\nlt\n` (matches `--ir-run` modulo the pre-existing
if-then trailing-value leak) — PASS.

## Files

- `src/runtime/x86/sm_lower.c` (1 line behavioural — `sm_emit` →
  `sm_emit_i` carrying EKind; comment expanded).
- `src/runtime/x86/sm_interp.c` (+39 lines — SM_ACOMP handler).
- `src/runtime/x86/sm_prog.c` (1 line — print case).
- `src/runtime/x86/sm_codegen.c` (comment refresh; no behavioural
  change — JIT path still h_unimpl by design).
- `docs/CHUNKS-step17g-runtime-bridge-acomp-validation.md` (this
  file).
