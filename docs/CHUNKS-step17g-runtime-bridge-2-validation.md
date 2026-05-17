# CH-17g-runtime-bridge-2 — wire `icn_try_call_builtin_by_name` into `SM_CALL_FN`

**Rung:** CH-17g-runtime-bridge-2 (second of three sub-rungs in the bridge plan)
**Session:** 2026-05-09
**Predecessor:** CH-17g-runtime-bridge-1
(`docs/CHUNKS-step17g-runtime-bridge-1-validation.md`).

## What this rung does

Wires the Icon-builtin dispatch helper extracted in bridge-1 into
`SM_CALL_FN`'s execution path in `sm_interp.c`.  After this rung:

```
$ ./scrip --interp /tmp/probe.icn
hello from icon proc
```

— and the output is byte-identical to `--ir-run`.

Before this rung, the same command FATALed with `Error 5: Undefined
function or operation` because `SM_CALL_FN`'s dispatch path consulted
only the SNOBOL4 builtin registry (`INVOKE_fn` → `APPLY_fn`).  Icon
builtins like `write` were never registered there.

## The placement subtlety

CH-17g-runtime-bridge-DESIGN proposed placing the helper call **after**
`INVOKE_fn` returned FAIL:

```c
result = INVOKE_fn(name, args, nargs);
if (result.v == DT_FAIL && name) {
    /* try Icon helper */
}
```

Empirical testing showed this never fires: `APPLY_fn` raises a SNOBOL4
runtime error via `sno_err` and **`longjmp`s out** through
`g_sno_err_jmp` when a name is not in any registry.  Control never
returns to the if-block.

The corrected placement tries the helper **before** `INVOKE_fn`:

```c
if (name) {
    DESCR_t icn_out;
    if (icn_try_call_builtin_by_name(name, args, nargs, &icn_out)) {
        result = icn_out;
        goto sm_call_invoke_done;
    }
}
result = INVOKE_fn(name, args, nargs);
sm_call_invoke_done: ;
```

Safety of the reordering:
- The helper recognises only a fixed, known set of Icon builtin names
  (`write`, `writes` initially).  For any other name it returns 0 and
  control falls through to `INVOKE_fn` exactly as before.
- For names the helper does recognise, `INVOKE_fn` is bypassed —
  but `INVOKE_fn` would have FAILed (or `longjmp`ed) on those names
  anyway, since they're not in the SNOBOL4 registry.
- No SNOBOL4 builtin is shadowed: SNOBOL4 has no `write` / `writes`
  builtin, and the helper's recognition list is restricted to names
  unique to Icon.

If a future rung adds an Icon builtin name that overlaps with a
SNOBOL4 builtin (none today; future-hypothetical), the order would
need to flip back to "INVOKE_fn first, helper as fallback" — and
that fallback would need to wrap `INVOKE_fn` in a `setjmp` to catch
the `longjmp`.  Today, the simpler "helper first" placement is
correct and minimal.

## What works now

```
$ cat /tmp/probe.icn
procedure main()
   write("hello from icon proc")
end

$ ./scrip --ir-run /tmp/probe.icn
hello from icon proc
$ ./scrip --interp /tmp/probe.icn
hello from icon proc

$ diff <(./scrip --ir-run /tmp/probe.icn) <(./scrip --interp /tmp/probe.icn)
$ # BYTE-IDENTICAL
```

Multi-call program also clean:

```
$ cat /tmp/probe2.icn
procedure main()
   write("hello");
   writes("no newline");
   write(" — has newline now");
   write(42);
end

$ ./scrip --interp /tmp/probe2.icn
hello
no newline — has newline now
42

$ diff <(./scrip --ir-run /tmp/probe2.icn) <(./scrip --interp /tmp/probe2.icn)
$ # BYTE-IDENTICAL
```

The chunk path `SM_BB_PUMP_PROC → coro_pump_proc_by_name → proc_trampoline →
sm_call_proc(entry_pc=1) → sm_call_chunk(1) → sm_interp_run` now reaches
`SM_CALL_FN s="write" nargs=1` in the chunk body and dispatches via
`icn_try_call_builtin_by_name` to the same Icon `write` implementation
that `--ir-run` uses.

## What still doesn't work

`--interp` of any Icon program that calls a builtin not yet covered
by `icn_try_call_builtin_by_name` (initial scope: `write`, `writes`)
will FATAL or `longjmp` out.  The corpus has 263 Icon programs;
many use `read`, `integer`, `string`, `type`, `copy`, `list`,
`table`, etc.  Coverage extends one builtin at a time as future
rungs add branches to the helper.

`--ir-run` execution is unchanged for all Icon programs.  This rung
adds capability to `--interp`; it does not regress `--ir-run`.

## Files touched

| File | Change |
|------|--------|
| `src/runtime/x86/sm_interp.c` | Wired `icn_try_call_builtin_by_name` into `SM_CALL_FN`'s dispatch path before `INVOKE_fn`.  +24 −1.  Local `extern` declaration of the helper inline at the call site (consistent with sm_interp.c's existing style for cross-module externs). |

No new opcodes, no IR fields, no `sm_lower.c` changes.

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
| `./scrip --interp /tmp/probe.icn` (`procedure main() write("hello from icon proc") end`) | byte-identical to `--ir-run` |
| Multi-call Icon program (`/tmp/probe2.icn`, four calls to write/writes) | byte-identical to `--ir-run` |

Standard set byte-identical to baseline.  The new gate (Icon
`--interp` of trivial program produces correct output) was the
explicit success criterion in the bridge plan; it now passes.

## Next rung

Two options:

- **CH-17g-runtime-bridge-3** — Raku/SCAN bridges.  Add similar helpers
  for Raku and SCAN-context dispatch if corpus crosscheck reveals
  programs that fail under `--interp` and would benefit.

- **CH-17g-irrun-lowers** — invoke `sm_lower` /
  `sm_resolve_proc_entry_pcs` from `--ir-run` so `entry_pc >= 0`
  for every proc regardless of mode.  Now safer than at the
  CH-17g-final-SURVEY decision point, because chunk dispatch
  works end-to-end for trivial programs.  Still needs a runtime
  flag gating chunk dispatch in `--ir-run` mode for programs
  that hit unbridged builtins, since the bridge today only covers
  `write`/`writes` and there are ~30 more Icon builtins.

Recommendation: extend the helper to cover more Icon builtins
(integer, string, real, char, type, copy, list, table, read, repl,
upto, find, any, many, tab, move, match, …) before tackling
irrun-lowers — each new branch lights up another slice of the
corpus under `--interp`.
