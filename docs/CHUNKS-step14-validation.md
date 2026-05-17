# CHUNKS-step14 validation — Generator infrastructure (SM_SUSPEND / SM_RESUME)

**Session:** #70, 2026-05-06
**one4all HEAD before:** `4afb18c8`

## What landed

- `sm_prog.h`: `SM_SUSPEND`, `SM_RESUME` opcodes added. `SM_INTERP_SUSPENDED = 1`
  return-code constant. Forward typedef `SmGenState` (struct defined in `sm_interp.h`).
- `sm_prog.c`: opnames table updated.
- `sm_interp.h`: Full `struct SmGenState` definition. `sm_gen_state_new()` and
  `bb_broker_drive_sm()` declarations.
- `sm_interp.c`: `g_current_gen_state` pointer. `SM_SUSPEND` handler (snapshots
  resume_pc + value stack into SmGenState, returns SM_INTERP_SUSPENDED). `SM_RESUME`
  no-op handler (documentation marker; resume is implicit on re-entry). `sm_gen_state_new()`
  and `bb_broker_drive_sm()` implementations.
- `sm_codegen.c`: Named-FATAL stubs for SM_SUSPEND / SM_RESUME (JIT gen is M5/EM-10).
- `sm_interp_test.c`: `test_generator_suspend_resume` — hand-built SM program yields
  10, 20, 30 via SM_SUSPEND; driven by `bb_broker_drive_sm`; re-drive of exhausted
  generator returns 0.  Stubs guarded with `#ifndef FULL_RUNTIME_LINKED`.
- `scripts/test_sm_generator_ch14.sh`: gate script (links against full scrip objects).

## Gate results

```
CH-14 generator gate:        PASS (17/17 tests, including 7 new generator tests)
smoke ×6:                    PASS (SNOBOL4 7/7, Icon 5/5, Prolog 5/5, Raku 5/5, Snocone 5/5, Rebus 4/4)
isolation gate:              PASS
```

## Honest scope

SM_SUSPEND / SM_RESUME are infrastructure only — no production frontend emits them yet.
Steps 15 (Icon generators per-kind) and 16 (Prolog clauses) will be the first real
producers. The JIT codegen path (sm_codegen.c) is intentionally left as named-FATAL
stubs until M5 (Step 19 / EM-10).

bb_broker_drive_sm drives an SM chunk via the interpreter (--interp path only).
The JIT path (--run) will need EM-10+ to support generators in emitted code.
