# CHUNKS-step17c-validation.md — CH-17c: flip Icon/Raku consumers to sm_call_proc

**Session:** #82, 2026-05-07
**Rung:** CH-17c — flip coro_call consumers via entry_pc; add sm_call_proc
**one4all HEAD after landing:** (see git log)

---

## What landed

### Files touched

- `src/runtime/interp/coro_runtime.h` — `nparams` added to `IcnProcEntry`;
  `sm_call_proc(int entry_pc, int nparams, DESCR_t *args, int nargs)` declared.
- `src/runtime/interp/coro_runtime.c` — `sm_call_proc` implemented; `entry_pc`
  and `nparams` added to `Icn_coro_stage_t`; `gather_entry_pc` / `gather_nparams`
  added to `coro_t` (via icon_gen.h); all three staging sites flipped to populate
  entry_pc/nparams; `proc_trampoline` and `gather_trampoline` dispatch via
  `sm_call_proc` when `entry_pc >= 0`, fall back to `coro_call` when `-1`.
- `src/driver/polyglot.c` — `proc_table[i].nparams = (int)proc->ival` populated
  at proc-table fill time.
- `src/frontend/icon/icon_gen.h` — `gather_entry_pc` and `gather_nparams` fields
  added to `coro_t`.
- `src/runtime/x86/sm_lower.c` — E_FNC lowering fixed for Icon-style nodes
  (`e->sval == NULL`, name in `children[0]->sval`): now extracts name from
  `children[0]`, lowers only `children[1..]` as args, emits `SM_CALL(fn, real_nargs)`.
  Fixes the empty-name / stack-shape bug in proc-body chunks containing user proc
  calls (noted in CH-17b'' handoff).

### sm_call_proc design

```c
DESCR_t sm_call_proc(int entry_pc, int nparams, DESCR_t *args, int nargs)
```

Pushes a fresh `IcnFrame` (nparams slots, args bound into env[0..nparams-1]),
delegates body execution to `sm_call_chunk(entry_pc)` — which runs the chunk in
a nested SM_State; `SM_LOAD_FRAME`/`SM_STORE_FRAME` see the live frame via
`icn_frame_env_load/store` (globals, not per-State).  Pops frame on return.

Static-variable persistence deferred to CH-17g (keyed on `EXPR_t*`; procs with
statics fall through to the `entry_pc == -1` legacy path which doesn't exist for
any real program yet — procs resolve entry_pc now, so statics are silently skipped
this rung).

### Consumer flip pattern

Three sites in `coro_runtime.c`:

1. `proc_trampoline` — reads from `coro_stage.entry_pc`; dispatches via
   `sm_call_proc` when `>= 0`, else legacy `coro_call`.
2. `gather_trampoline` — reads from `ss->gather_entry_pc`; same gate.
3. `coro_pump_proc_by_name` — populates `coro_stage.entry_pc/nparams`.

Plus one site in `coro_eval`'s E_FNC branch that populates `coro_stage` and
one site in `E_ITERATE`'s gather branch that sets `ss->gather_entry_pc/nparams`.

### Empirical proof

`SCRIP_PROC_ENTRY_PCS=1 ./scrip --interp test/icon/palindrome.icn`:
```
[CH-17a]   proc[0] name=palindrome           entry_pc=1
[CH-17a]   proc[1] name=main                 entry_pc=54
```
Both procs resolve non-(-1) entry_pcs; `proc_trampoline` dispatches via
`sm_call_proc` for both. The resulting `--interp` Error 5 is pre-existing
(Icon `--interp` is not yet end-to-end; that is CH-17e territory after
CH-17d lands Prolog chunks).

---

## Gates

| Gate | Result |
|------|--------|
| smoke ×6 (7/7, 5/5, 5/5, 5/5, 5/5, 4/4) | PASS |
| isolation gate | PASS |
| csnobol4 Budne | PASS=61 (≥34, byte-identical to baseline) |
| unified_broker | PASS=49 |
| Icon corpus (`--ir-run`) | PASS=186 FAIL=47 XFAIL=30 TOTAL=263 (byte-identical) |
| scrip_all_modes | PASS=2 FAIL=0 |

All gates byte-identical to baseline.

---

## Scope boundary (honest)

- `--interp` Icon execution still fails end-to-end: `sm_call_proc` dispatches
  the chunk, but chunks containing generator kinds (E_EVERY, E_BANG_BINARY, etc.)
  still emit `SM_PUSH_EXPR + SM_BB_PUMP` (CH-17h territory). The Error 5 seen
  in `--interp` palindrome is the BB engine failing to handle the chunk-wrapped
  call, not a CH-17c bug.
- Static-variable persistence for procs with `static` declarations: deferred
  to CH-17g (key must change from `EXPR_t*` to `entry_pc + name`).
- `coro_drive_fnc` (the non-coroutine suspend-aware driver in `coro_runtime.c`)
  still reads `proc_table[pi].proc` directly — left for CH-17g cleanup when
  `EXPR_t *proc` is deleted from `IcnProcEntry`.

**Next rung: CH-17d** — sm_lower emits named chunks for Prolog predicates.
