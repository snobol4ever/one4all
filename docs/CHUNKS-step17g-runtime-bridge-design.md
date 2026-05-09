# CH-17g-runtime-bridge — design note: how chunks dispatch Icon/Raku builtins from inside SM

**Rung:** CH-17g-runtime-bridge (design phase; implementation TBD)
**Session:** 2026-05-09
**Predecessor:** CH-17g-final-SURVEY (`docs/CHUNKS-step17g-final-survey.md`)
documented the gap.  This note documents the architectural shape of
the fix.

## The gap

Today, `--sm-run` of a trivial Icon program:

```icon
procedure main()
   write("hello from icon proc")
end
```

FATALs with `** Error 5 in statement 0 / Undefined function or operation`,
even though `SCRIP_PROC_ENTRY_PCS=1` confirms `main`'s entry_pc resolves
to chunk 1.  The chunk lowering is correct; the dispatch is broken.

## Where the chunk emits the call

`./scrip --sm-run --dump-sm /tmp/probe.icn`:

```
   0  SM_JUMP              -> 6
   1  SM_LABEL
   2  SM_PUSH_LIT_S        s="hello from icon proc"
   3  SM_CALL              s="write" nargs=1
   4  SM_POP
   5  SM_RETURN
   6  SM_LABEL
   7  SM_BB_PUMP_PROC
   8  SM_HALT
```

(Op shown as `SM_CALL` here is the same opcode renamed `SM_CALL_FN` in
EM-7c-three-column-non-bb's symmetry pass.)

The chunk pushes the literal, emits `SM_CALL_FN s="write" nargs=1`,
pops the result, returns.  Architecturally clean.

## Why dispatch fails

`SM_CALL_FN`'s handler in `sm_interp.c:931–1212` walks this sequence:

1. Special pseudo-calls (`INDIR_GET`, `NAME_PUSH`, `ASGN_INDIR`,
   `NRETURN_ASGN`, `IDX`, `IDX_SET`) handled inline.
2. Pop nargs args.
3. DATA field accessor/mutator dispatch (`sc_dat_field_call`).
4. SM-native user function: `sm_label_pc_lookup(prog, name)` — finds
   user-defined SNOBOL4 fns by SM_LABEL.
5. Fallback: `INVOKE_fn(name, args, nargs)` →  `APPLY_fn(name, ...)`
   — SNOBOL4 builtin registry.

**None of these reach Icon's `write`.**  Icon `write` lives in
`src/driver/interp_eval.c:309`, inside the function
`icn_call_builtin(EXPR_t *call, DESCR_t *args, int nargs)`.  That
function is on the legacy IR-walker path
(`bb_exec_stmt → bb_eval_value → coro_eval/interp_eval → icn_call_builtin`).
It has never been wired into the SM dispatch sequence.

`APPLY_fn` returns FAIL for `"write"` because Icon builtins are not
registered through `register_fn`.  The SM call returns FAIL, and the
chunk's caller surfaces it as `Error 5`.

## Inventory of what `icn_call_builtin` covers

`src/driver/interp_eval.c` line 284 onwards.  Three layers of dispatch
before the switch on `fn`:

1. `raku_try_call_builtin(call, &out)` — Raku-specific builtins that
   re-evaluate args from `call->children[]` (ignore the pre-evaluated
   `args`).  Needs EXPR_t.
2. `scan_try_call_builtin(call, args, nargs, &out)` — SCAN-context
   builtins.  Uses pre-evaluated `args` but takes `EXPR_t *call` for
   the name lookup and structural inspection.
3. The if-chain: `write`, `writes`, plus ~30 more Icon builtins
   (`integer`, `string`, `real`, `char`, `type`, `copy`, `list`,
   `table`, `read`, `repl`, `upto`, `find`, `any`, `many`, `tab`,
   `move`, `match`, `*proc`, …).  Most use only `(fn, args, nargs)`;
   a few (e.g. generator builtins) need `call->children[]`.
4. User proc fallback: walk `proc_table[]`, call
   `proc_table_call(i, args, nargs)`.

After that, the function falls through to the IR-walker's E_FNC
handling, which knows the rest of the language semantics.

## Two possible bridges

### Option A — extract a name-based helper

Define a new function:

```c
/* In src/driver/interp_eval.c (or a new file): */
int icn_try_call_builtin_by_name(const char *fn, DESCR_t *args, int nargs,
                                 DESCR_t *out)
{
    /* Same body as the if-chain in icn_call_builtin starting at line 309,
     * but: skip the EXPR_t-required branches (those return 0 = "not handled");
     * for everything else (write, writes, integer, string, ...) compute
     * result and *out = it, return 1. */
}
```

Then in `sm_interp.c`'s `SM_CALL_FN` handler, after the
`INVOKE_fn` line:

```c
result = INVOKE_fn(name, args, nargs);
if (result.v == DT_FAIL) {
    DESCR_t icn_out;
    if (icn_try_call_builtin_by_name(name, args, nargs, &icn_out))
        result = icn_out;
}
```

**Pros:**
- Surgical, two-step change.
- Doesn't introduce IR construction at the SM call site.
- Existing `icn_call_builtin` keeps working; the new helper is
  extracted from it (or made a small subset shared between them).
- The Raku/SCAN dispatch helpers can be wired in too; their
  `EXPR_t *call` requirement is satisfiable by passing NULL when
  the caller has no IR (their bodies handle NULL gracefully or
  short-circuit on it).

**Cons:**
- Code duplication if `icn_call_builtin` and the by-name version
  drift apart.  Mitigated by making one a thin wrapper over the
  other — `icn_call_builtin(call, args, nargs)` becomes
  `icn_try_call_builtin_by_name(call->children[0]->sval, args, nargs, &out)`
  plus the EXPR_t-required tail.
- Builtins that need `call->children[]` (generators, BB-aware ones)
  remain unreachable from chunks.  Chunks would FATAL on those.
  This is OK for now: the trivial-Icon-program case unblocks
  CH-17g-irrun-lowers, and the EXPR_t-required builtins migrate
  one at a time (each becomes a per-kind chunk producer eventually).

### Option B — register Icon builtins in the SNOBOL4 fn table

```c
register_fn("write",  _icn_write_fn,  0, 99);
register_fn("writes", _icn_writes_fn, 0, 99);
/* ... */
```

Where `_icn_write_fn` is a wrapper that calls the same body as
`icn_call_builtin`'s `if (!strcmp(fn, "write"))` branch.

`APPLY_fn` then finds `write` and dispatches it.

**Pros:**
- Zero changes in `sm_interp.c`.
- Icon and SNOBOL4 share one builtin registry — closer to "one runtime."

**Cons:**
- Cross-language pollution.  In a SNOBOL4-only program, `write` would
  now resolve to the Icon implementation rather than failing with
  "undefined function" — semantic creep.
- Mitigation requires gating on `lang_mask` at registration or
  per-program, which is awkward.
- Icon's variadic `write` semantics (FAIL propagation, &null handling)
  don't map cleanly onto the SNOBOL4 builtin contract; `register_fn`'s
  arity range `(0, 99)` papers over but doesn't capture that.

### Recommendation

**Option A.**  The cross-language pollution risk in B is a real
problem; A keeps language semantics segregated and the fix
self-contained in the SM call dispatch path.

## Implementation rungs

### CH-17g-runtime-bridge-1 — extract `icn_try_call_builtin_by_name`

Refactor `icn_call_builtin` so its body splits into:
- `icn_try_call_builtin_by_name(fn, args, nargs, &out)` — the
  EXPR_t-free subset (write, writes, integer, string, real, char,
  type, copy, list, table, read, repl, upto, find, any, many, tab,
  move, match, ?proc, …).  Returns 1 if handled.
- `icn_call_builtin(call, args, nargs)` — wraps the by-name helper
  for the EXPR_t-free cases, falls through to its existing logic
  for builtins that need EXPR_t.

Gate: existing Icon corpus 186/47/30 byte-identical (this is pure
refactor; no new dispatch sites).

### CH-17g-runtime-bridge-2 — wire the helper into SM_CALL_FN

In `sm_interp.c`'s `SM_CALL_FN` handler, after the existing
`INVOKE_fn` fallback, add the `icn_try_call_builtin_by_name` call.

Gate:
- `--sm-run` of `procedure main() write("hello") end` now produces
  `hello\n` instead of FATAL.
- Curated subset of trivial Icon programs (no generators, no
  every/suspend, no E_FNC calls into not-yet-bridged builtins) run
  end-to-end under `--sm-run` and produce output identical to
  `--ir-run`.
- Standard set byte-identical (this is pure addition; the bridge
  fires only when `INVOKE_fn` would have FAILed).

### CH-17g-runtime-bridge-3 — Raku/SCAN bridges, if needed

Same pattern, applied to `raku_try_call_builtin` and
`scan_try_call_builtin`.  Optional — only land when corpus
crosscheck reveals a Raku/SCAN program that fails under `--sm-run`
and would benefit.

## Then: CH-17g-irrun-lowers

Once CH-17g-runtime-bridge unblocks the chunk path for trivial
programs, CH-17g-irrun-lowers becomes safe:

- Invoke `sm_lower` + `sm_resolve_proc_entry_pcs` from the `--ir-run`
  non-SNO dispatch path before `polyglot_execute`.
- `proc_table_call`'s `entry_pc >= 0` branch fires, dispatching via
  chunks.  For trivial programs covered by the bridge, output is
  identical.  For programs hitting un-bridged builtins (generators,
  every/suspend, …), the chunk dispatch FATALs — so a per-program
  fallback is needed:

  Either:
  - (a) gate the chunk dispatch behind a flag that's off by default
    in `--ir-run`, on by default in `--sm-run` — same as today's
    effective behavior, just made explicit; or
  - (b) detect the failure and fall back to the legacy path; or
  - (c) accept the breakage and migrate per-builtin until the corpus
    is fully bridged.

  (a) is the safe, behavior-preserving choice.  (c) is the eventual
  destination.  (b) is brittle.

## Future per-kind migration

Each of the line-1303 generator kinds (CH-17h's targets:
E_EVERY, E_SUSPEND, E_BANG_BINARY, …) is a candidate for its own
chunk-side migration once the runtime bridge is in place.  Today,
the line-1303 dispatcher arm emits `emit_push_expr + SM_BB_PUMP`
inside chunks — that's the "EXPR_t pushed to SM stack" failure
mode CHUNKS exists to eliminate.  Each kind migrates by pairing:

- A chunk-side producer (lower the kind into pure SM ops).
- A chunk-side consumer (a new SM opcode like `SM_BB_PUMP_EVERY`
  or `SM_BB_PUMP_SUSPEND`, mirroring CH-17f's `SM_BB_ONCE_PROC`).

CH-17g-runtime-bridge is a precondition: until chunks can dispatch
*any* Icon builtin, none of these kind-specific consumers can be
tested end-to-end on real corpora.

## Files this rung will touch

| File | Change |
|------|--------|
| `src/driver/interp_eval.c` | Extract `icn_try_call_builtin_by_name` (~30 builtins). |
| `src/driver/interp_eval.h` | Declare new function. |
| `src/runtime/x86/sm_interp.c` | Wire the helper into `SM_CALL_FN` after `INVOKE_fn`. |

No new opcodes.  No new IR fields.  No `sm_lower.c` changes.

## Gates plan

| Step | Gate |
|------|------|
| CH-17g-runtime-bridge-1 (refactor) | Standard set byte-identical |
| CH-17g-runtime-bridge-2 (wire) | + `--sm-run` of trivial Icon proc produces output |
| CH-17g-runtime-bridge-3 (Raku/SCAN) | + Raku/SCAN smoke under `--sm-run` if applicable |
