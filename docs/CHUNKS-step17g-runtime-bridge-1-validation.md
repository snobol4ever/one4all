# CH-17g-runtime-bridge-1 — refactor `icn_call_builtin` into name-based helper + EXPR_t tail

**Rung:** CH-17g-runtime-bridge-1 (first of three sub-rungs in the bridge plan)
**Session:** 2026-05-09
**Predecessor:** CH-17g-runtime-bridge-DESIGN
(`docs/CHUNKS-step17g-runtime-bridge-design.md`).

## What this rung does

Pure refactor.  Extracts an EXPR_t-free helper from `icn_call_builtin`:

```c
int icn_try_call_builtin_by_name(const char *fn,
                                 DESCR_t *args,
                                 int nargs,
                                 DESCR_t *out);
```

Returns 1 if the call was handled (and writes the result to `*out`),
0 otherwise.  The helper has no IR handle: only the function name
and pre-evaluated args.

Initial scope: `write` and `writes`.  Each branch is a verbatim copy
of the same logic that lived inline in `icn_call_builtin`.
`icn_call_builtin` is reorganised to delegate to the new helper
*after* the Raku/SCAN dispatch (those need EXPR_t) but *before* the
existing user-proc / clone-or-fallback paths.

## Why this rung exists

`icn_call_builtin` is the path through which Icon's `write` (and ~30
other Icon builtins) reaches stdout when an Icon program is run via
`--ir-run`.  The function takes `EXPR_t *call` as its first argument
because some builtins (Raku/SCAN dispatchers, mutators that write
back through `children[1]`'s lvalue identity, generator builtins
inspecting `children[i]` structurally) need it.  But the bulk of
the dispatched builtins use only `call->children[0]->sval` and the
already-evaluated `args[]` array.

`SM_CALL_FN`'s handler in `sm_interp.c` has the name and the args
but no IR call node.  So when a chunk emits
`SM_CALL_FN s="write" nargs=1`, dispatch fails on `INVOKE_fn /
APPLY_fn` (the SNOBOL4 builtin registry) because Icon's `write`
isn't registered there.

CH-17g-runtime-bridge-1 prepares the surface.
CH-17g-runtime-bridge-2 (next rung) wires
`icn_try_call_builtin_by_name` into `SM_CALL_FN` after
`INVOKE_fn`'s FAIL.

## Invariants preserved

1. `icn_call_builtin` still takes `EXPR_t *call` and dispatches the
   same set of builtins it always did.
2. The dispatch order inside `icn_call_builtin` is unchanged:
   Raku → SCAN → (write/writes via the new helper) → user proc →
   clone-or-fallback.
3. Behaviour for every existing caller (`coro_bb_fnc`, the BB
   adapters in `coro_value.c` and `coro_runtime.c`) is identical.
4. The helper's branches are exact copies of the inlined code they
   replace.  Future drift between them is prevented by
   `icn_call_builtin` calling the helper directly rather than
   carrying its own copies.

## Files touched

| File | Change |
|------|--------|
| `src/driver/interp_eval.c` | Added `icn_try_call_builtin_by_name` (~30 lines, two branches: write, writes).  Removed those branches from inside `icn_call_builtin`'s body and replaced with a one-line delegation. |
| `src/driver/interp_private.h` | Declared `icn_try_call_builtin_by_name` next to the existing `icn_call_builtin` declaration. |

No new opcodes, no new IR fields, no `sm_lower.c` changes.

## Gates

| Gate | Result |
|------|--------|
| smoke snobol4 | PASS=7 FAIL=0 |
| smoke icon | PASS=5 FAIL=0 |
| smoke prolog | PASS=5 FAIL=0 |
| smoke raku | PASS=5 FAIL=0 |
| smoke snocone | PASS=5 FAIL=0 |
| smoke rebus | PASS=4 FAIL=0 |
| isolation_ir_sm | PASS — no IR-only symbol leaks in SM runtime files |
| unified_broker | PASS=49 FAIL=0 |
| scrip_all_modes | PASS=2 FAIL=0 |
| Icon corpus `--ir-run` (`test_icon_ir_all_rungs.sh`) | PASS=186 FAIL=47 XFAIL=30 TOTAL=263 |

All byte-identical to the baseline established by CH-17g-statics.

## Manual verification

```
$ ./scrip --ir-run /tmp/probe.icn
hello from icon proc
```

The legacy IR-walker path still routes through `icn_call_builtin` →
`icn_try_call_builtin_by_name` for `write`, producing the same
output as before the refactor.

`--interp` of the same program still FATALs with "Undefined function
or operation" — the helper is defined but not yet wired into
`SM_CALL_FN`.  That's CH-17g-runtime-bridge-2's work.

## Next rung

**CH-17g-runtime-bridge-2** — wire `icn_try_call_builtin_by_name`
into `SM_CALL_FN`.  In `sm_interp.c`, after the existing `INVOKE_fn`
fallback returns FAIL, call the helper.  Gate: `--interp` of the
trivial Icon proc produces output identical to `--ir-run`; standard
set byte-identical.
