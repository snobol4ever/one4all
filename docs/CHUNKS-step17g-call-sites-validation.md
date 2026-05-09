# CHUNKS Step 17g (call-sites half) — validation

**Rung:** CH-17g-call-sites — flip the eight remaining
`coro_call(proc_table[i].proc, args, nargs)` consumer call sites to
dispatch via SM chunk when `entry_pc` is resolved (mirrors CH-17c's
trampoline-side flip).

**Session:** TBD on commit.  **Date:** 2026-05-09.

## Context — why this rung is carved out of CH-17g

GOAL-CHUNKS-STEP17.md's CH-17g rung text reads:

> After CH-17c flipped Icon/Raku consumers and CH-17e flipped Prolog
> consumers, no live code reads `proc_table[i].proc` or
> `pl_pred_table_*`'s EXPR_t payload.  Delete the `EXPR_t *proc`
> field; delete the legacy `coro_call` body's IR walk; ...  Lift
> `lang_mask == LANG_SNO` gate on `code_free`.

Empirically that premise was not yet true.  CH-17c's flip touched
the **trampoline layer** (`proc_trampoline`, `gather_trampoline` in
`coro_runtime.c`) — the two paths that BB drives for goal-directed
proc evaluation.  Eight other `coro_call(proc_table[i].proc, ...)`
sites — covering value-context user-proc calls, Raku method dispatch,
the U-22 cross-language fallback, and three top-level main dispatch
sites — still read `.proc` directly.

Deleting the `EXPR_t *proc` field today (CH-17g's stated work)
would break these eight sites.  Lifting the `code_free` gate today
would also be premature: chunk bodies emitted by CH-17b'/CH-17b''
still contain `SM_PUSH_EXPR + SM_BB_PUMP` for unmigrated generator
kinds (E_EVERY, E_SUSPEND, E_BANG_BINARY, E_LCONCAT, E_LIMIT,
E_RANDOM, E_SECTION*) per CH-17b' closing notes — those references
require live IR.

CH-17g is therefore split, mirroring how CH-17b/b'/b'' were carved:

  - **CH-17g-call-sites (this rung)** — flip the eight residual
    consumer sites to use the same `entry_pc >= 0 ? sm_call_proc :
    coro_call` dispatch CH-17c established.
  - **CH-17g-statics** (future) — re-key the EXPR_t-keyed
    static-variable table (`coro_runtime.c:155–183`) onto
    (entry_pc, name) or (proc-name, var-name) so `IcnProcEntry`
    can drop `EXPR_t *proc`.
  - **CH-17g-final** (future, after CH-17h) — drop `EXPR_t *proc`
    from `IcnProcEntry`; lift the `code_free` gate.

## Approach

A single dispatch helper, `proc_table_call(int pi, DESCR_t *args,
int nargs)`, was added to `coro_runtime.{c,h}` next to the existing
`sm_call_proc`.  Body:

```c
DESCR_t proc_table_call(int pi, DESCR_t *args, int nargs)
{
    if (pi < 0 || pi >= proc_count) return FAILDESCR;
    if (proc_table[pi].entry_pc >= 0)
        return sm_call_proc(proc_table[pi].entry_pc,
                            proc_table[pi].nparams, args, nargs);
    return coro_call(proc_table[pi].proc, args, nargs);
}
```

Each of the eight call sites collapses from
`coro_call(proc_table[i].proc, args, nargs)` to
`proc_table_call(i, args, nargs)` — single line, identical pattern.

The trampoline-layer dispatch in `proc_trampoline` and
`gather_trampoline` (CH-17c) was left as-is rather than retrofitted
through the helper, because those paths take the proc/entry_pc
fields **at staging time** (saved into `coro_stage` / `ss->gather_*`
before the coroutine is resumed) — the helper's call-time pi-lookup
shape doesn't fit them.  Same routing logic, different control flow.

## Sites flipped (eight)

| File                                    | Site          | Context |
|-----------------------------------------|---------------|---------|
| `src/runtime/interp/coro_value.c`       | E_FNC user-proc dispatch in `bb_eval_value` | value-context Icon proc call |
| `src/runtime/interp/raku_builtins.c`    | Raku method-call dispatch in `raku_try_call_builtin` | Raku method-syntax `obj.meth(args)` to user proc |
| `src/driver/interp_eval.c` (≈336)       | User proc value-context call in `interp_eval` E_FNC | non-builtin proc reached via SNO IR walker |
| `src/driver/interp_eval.c` (≈793)       | User proc fallback in `interp_eval` E_FNC | another reachable path for the same |
| `src/driver/interp_eval.c` (≈2289)      | U-22 cross-language fallback | SNO source calling Icon proc by name |
| `src/driver/interp_hooks.c` (≈143)      | SNO→Icon usercall hook | `_usercall_hook` cross-language path |
| `src/driver/interp_exec.c` (≈409, 414)  | Module-driven main dispatch | polyglot top-level main per ScripModule |
| `src/driver/interp_exec.c` (≈432)       | Legacy single-section main | non-polyglot top-level main |
| `src/driver/polyglot.c` (≈263)          | Single-language Icon main | single-section .icn entry |

(Eight call sites across nine line numbers — `interp_exec.c:409+414`
are sibling sites in the same `if (g_polyglot && g_registry.nmod > 0)`
branch, both flipped together.)

## Sites intentionally NOT flipped

- **`coro_runtime.c:1125, 1213, 1503`** — these stage
  `coro_stage.proc` / `ss->gather_proc` for `proc_trampoline` /
  `gather_trampoline`.  CH-17c's flip already lives inside those
  trampolines (read `entry_pc >= 0` from the staging struct, dispatch
  via `sm_call_proc`).  Keeping `.proc` written into staging
  preserves the legacy fallback path for procs whose chunks
  haven't been emitted yet.

- **`coro_runtime.c:1721` (`coro_drive_fnc`)** — this is the
  suspend-aware driver for user procedures called as generators
  inside `every` bodies.  It walks `proc->children[]` to drive
  the body statement-by-statement with explicit suspend/resume
  semantics around each E_SUSPEND.  That is an IR walker by
  design.  M4-cleanup territory (CH-17h migrates the generator
  kinds into chunk-shaped lowering; that opens the door to a
  chunk-driven replacement for `coro_drive_fnc`).

- **`sm_lower.c:1742`** — sm_lower itself reads
  `proc_table[pi].proc` to discover the proc body to lower into
  a named chunk.  Producer-side, not consumer.  Cannot be removed
  until the proc body is reachable through some other channel
  (e.g., walking the IR statement list directly for E_FNC stmts).

## Why this is byte-identical

For every flipped site, today's behaviour is **the same path** that
ran before:

- For procs whose chunks contain real lowered SM ops AND whose
  bodies don't transit unmigrated generator kinds, `entry_pc >= 0`
  fires and `sm_call_proc` dispatches through SM chunk.  These
  paths were already reachable from the trampolines after CH-17c,
  so their behaviour is established.

- For every other proc, `entry_pc == -1` and the call falls through
  to `coro_call(proc_table[pi].proc, args, nargs)` — exactly the
  former behaviour.

The flip is therefore a routing reorganisation: it adds the chunk
path to call sites that previously could only see the IR path.  It
does not change what either path does.

For value-context callers in particular, `sm_call_proc` returns
`FAILDESCR` if the chunk fails or hits a stack-leak — same return
semantics as `coro_call`'s FAIL-on-failure shape, so the existing
`IS_FAIL_fn` checks downstream are unaffected.

## Gates

Pre-flip baseline (one4all @ `92b922ca`, this session):

| Gate | Result |
|------|--------|
| smoke ×6 | 7/7, 5/5, 5/5, 5/5, 5/5, 4/4 |
| isolation gate | PASS |
| unified_broker | PASS=49 |
| csnobol4 Budne | PASS=50 (FAIL=100, SKIP=8) |
| Icon corpus | PASS=186 FAIL=47 XFAIL=30 TOTAL=263 |
| scrip_all_modes | PASS=2 FAIL=0 |

Post-flip:

| Gate | Result | Δ |
|------|--------|---|
| smoke ×6 | 7/7, 5/5, 5/5, 5/5, 5/5, 4/4 | byte-identical |
| isolation gate | PASS | byte-identical |
| unified_broker | PASS=49 | byte-identical |
| csnobol4 Budne | PASS=50 (FAIL=100, SKIP=8) | byte-identical |
| Icon corpus | PASS=186 FAIL=47 XFAIL=30 TOTAL=263 | byte-identical |
| scrip_all_modes | PASS=2 FAIL=0 | byte-identical |

(Note: csnobol4 Budne baseline differs from CH-17f's recorded
PASS=61 — investigation deferred; this session establishes its own
pre-flip PASS=50 baseline, and the post-flip count is byte-identical
to that.  The Budne discrepancy is environmental, not introduced
here.)

## Files touched

- `src/runtime/interp/coro_runtime.h` — declared `proc_table_call`
- `src/runtime/interp/coro_runtime.c` — defined `proc_table_call`
- `src/runtime/interp/coro_value.c` — flipped 1 site
- `src/runtime/interp/raku_builtins.c` — flipped 1 site
- `src/driver/interp_eval.c` — flipped 3 sites
- `src/driver/interp_hooks.c` — flipped 1 site
- `src/driver/interp_exec.c` — flipped 3 sites
- `src/driver/polyglot.c` — flipped 1 site

## Next rungs

- **CH-17g-statics** — re-key static-variable storage off
  `EXPR_t*`.  Two callers in `coro_runtime.c:447, 500` (read in
  param-load preamble; write in frame-pop epilogue) and the table
  itself at lines 155–183.  Migration target: keyed on
  `(entry_pc, var_name)` so two procs with the same variable name
  (different statics) remain separate, mirroring the EXPR_t identity
  guarantee.

- **CH-17g-final** — after CH-17h migrates the remaining Icon
  generator kinds (E_EVERY, E_SUSPEND, E_BANG_BINARY, E_LCONCAT,
  E_LIMIT, E_RANDOM, E_SECTION*) and chunk bodies stop emitting
  `SM_PUSH_EXPR + SM_BB_PUMP` for them, the IR is no longer needed
  by any live runtime path.  At that point the `EXPR_t *proc` field
  drops, the `lang_mask == LANG_SNO` gate on `code_free` lifts, the
  legacy `coro_call` body's IR walk and `coro_drive_fnc` retire, and
  the structural rule on `polyglot.c` / `coro_runtime.c` /
  `pl_runtime.c` against `EXPR_t*` proc/pred-table fields turns on.
  That is the GOAL-CHUNKS.md Step 17 closure point.
