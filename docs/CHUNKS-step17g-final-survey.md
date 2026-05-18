# CH-17g-final-SURVEY — legacy `coro_call` body is the live `--ir-run` path; CH-17h-SURVEY's "dead weight" framing is wrong

**Rung:** CH-17g-final-SURVEY (precondition audit before deletion)
**Session:** 2026-05-09
**Methodology:** instrument `coro_call` and `proc_table_call` with
`fprintf(stderr, ...)` probes, build, run trivial Icon program in
`--ir-run` mode, observe.  Probes reverted before commit; `git diff`
clean.

## Finding

The legacy body of `coro_call(EXPR_t *proc, DESCR_t *args, int nargs)`
is the **live, hot, only consumer** of Icon and Raku user-proc dispatch
in `--ir-run` mode.  It is not dead weight.  Deleting it breaks every
Icon and Raku program that runs under `--ir-run` — the same corpus that
the CHUNKS standard gate set uses to validate every rung
(`test_icon_all_rungs.sh`, baseline 186/47/30).

This contradicts the recommendation in `CHUNKS-step17h-survey.md`:

> "the legacy body becomes dead weight after CH-17g-call-sites and
> CH-17g-statics, not a live consumer; deleting it is what *creates*
> the test surface for migration, not what blocks it."

The empirical reality is the opposite.  Receipts below.

## Empirical evidence

Probe added at the head of `coro_call` and `proc_table_call`:

```c
DESCR_t proc_table_call(int pi, DESCR_t *args, int nargs) {
    fprintf(stderr, "[CH17G-PROBE] proc_table_call pi=%d entry_pc=%d nparams=%d\n",
            pi, pi>=0?proc_table[pi].entry_pc:-99,
            pi>=0?proc_table[pi].nparams:-99);
    if (pi < 0 || pi >= proc_count) return FAILDESCR;
    if (proc_table[pi].entry_pc >= 0)
        return sm_call_proc(proc_table[pi].entry_pc, ..., args, nargs);
    return coro_call(proc_table[pi].proc, args, nargs);
}

DESCR_t coro_call(EXPR_t *proc, DESCR_t *args, int nargs) {
    fprintf(stderr, "[CH17G-PROBE] coro_call ENTERED proc=%p\n", (void*)proc);
    int nparams = (int)proc->ival;
    /* ... rest unchanged ... */
}
```

Test program:
```icon
procedure main()
   write("hello from icon proc")
end
```

Run output (after `bash scripts/build_scrip.sh`):
```
$ ./scrip --ir-run /tmp/probe.icn < /dev/null
[CH17G-PROBE] proc_table_call pi=0 entry_pc=-1 nparams=0
[CH17G-PROBE] coro_call ENTERED proc=0x...
hello from icon proc
```

**Two facts visible here:**

1. `proc_table_call` IS being reached (CH-17g-call-sites' flip works).
2. **`entry_pc = -1`** for the only proc.  The CH-17a resolver never
   ran.  CH-17g-call-sites' `if (entry_pc >= 0)` branch is skipped.
   The fallback `coro_call(proc_table[pi].proc, ...)` is what
   produces the program's output.

## Root cause: `--ir-run` does not invoke `sm_lower`

`sm_resolve_proc_entry_pcs` (added by CH-17a) is called from
`scrip_sm.c:sm_preamble`, which is invoked from `scrip.c:524`
(`mode_sm_run`) and `scrip.c:540` (`mode_jit_run`).

For `--ir-run` with non-SNO IR, `scrip.c:557–561` dispatches
to `polyglot_execute(prog)` (or `execute_program(prog)` for SNO-only).
**Neither path calls `sm_lower`.**  Therefore
`sm_resolve_proc_entry_pcs` never runs.  Therefore every
`proc_table[i].entry_pc` stays at its `polyglot.c:172` initial value
of `-1`.  Therefore `proc_table_call`'s `entry_pc >= 0` branch is
never taken in `--ir-run` mode.

The Icon corpus gate (`test_icon_all_rungs.sh`) runs every
program under `--ir-run`.  Its 186 PASS programs all reach the
legacy `coro_call` body.

## What CH-17h-SURVEY got right and what it got wrong

CH-17h-SURVEY's audit of `sm_lower.c:1303` was **correct**: those
nine generator kinds (`E_EVERY`, `E_SUSPEND`, `E_BANG_BINARY`,
`E_LCONCAT`, `E_LIMIT`, `E_RANDOM`, `E_SECTION`, `E_SECTION_PLUS`,
`E_SECTION_MINUS`) are dead at that lowering site today on real
corpora.  That part of the survey's instrumentation and conclusion
holds.

What the survey then **inferred wrongly** was that this dead-lowerer
status implies the **runtime** path through `coro_call` is also
dormant.  These are two different code paths:

| What's dead today                       | What's live today                       |
|----------------------------------------|----------------------------------------|
| `sm_lower.c:1303` arm — these kinds, when encountered as **values inside expressions during SM lowering**, never reach this dispatcher. | `coro_runtime.c:431` — `coro_call`'s body, when invoked from `proc_table_call`'s fallback, **is** the engine that walks proc-body IR statement-by-statement. |
| Resolution: a chunk-side migration of how these kinds get *lowered into SM* (CH-17h proper). | Resolution: **either** make the chunk-side path actually run end-to-end so `entry_pc >= 0` produces correct output, **or** route `--ir-run` through `sm_lower` first so entry_pcs resolve, **or** keep the legacy body. |

CH-17h's lowering-site fix doesn't reach the runtime side.  Even with
all nine kinds migrated in `sm_lower.c`, the runtime would still need
the chunks executed end-to-end before `coro_call` could be deleted.

## What's actually needed for CH-17g-final to be safe

CH-17g-final's deletion targets — `EXPR_t *proc` field, legacy
`coro_call` body, `coro_drive_fnc` — can only be deleted when **both**
of the following hold:

1. The chunk path produces correct output for every Icon/Raku/Prolog
   program in the corpus (currently the chunks are skeleton-or-stub:
   `--interp /tmp/probe.icn` returns FATAL "Undefined function" on
   `write()` even though `entry_pc` resolves to 1).
2. `--ir-run` invokes `sm_lower` (and therefore
   `sm_resolve_proc_entry_pcs`) before dispatch, so `entry_pc >= 0`
   for every proc by the time `proc_table_call` is reached.

Today, neither holds.

### Verifying (1): `--interp` of trivial Icon proc fails

```
$ ./scrip --interp /tmp/probe.icn < /dev/null
** Error 5 in statement 0
   Undefined function or operation
```

Even though `SCRIP_PROC_ENTRY_PCS=1` confirms `main` resolves to
`entry_pc=1`, the chunk body cannot dispatch `write` as a builtin from
inside the chunk SM execution.  The infrastructure for builtin
dispatch from chunks (likely an `SM_CALL_BUILTIN` opcode or a chunk
hook into `coro_value.c`'s builtin table) does not exist.

### Verifying (2): `--ir-run` does not run `sm_lower`

See "Root cause" above: `scrip.c:557–561` for non-SNO `--ir-run`
dispatches to `polyglot_execute` directly.  No SM lowering happens.

## Recommendation

CH-17g-final as scoped is **not yet executable**.  Its preconditions
in `GOAL-CHUNKS-STEP17.md` should be amended with two new rungs:

- **CH-17g-runtime-bridge** — make chunks dispatch builtins.  Either:
  add `SM_CALL_BUILTIN` opcode that walks the same `coro_value.c`
  builtin table the IR walker uses; **or** extend `sm_call_chunk` to
  dispatch a chunk-internal call by name through a lookup that hits
  both proc_table and the builtin registry.  Gate: `--interp` of
  every Icon hello-world variant produces output identical to
  `--ir-run`.
- **CH-17g-irrun-lowers** — invoke `sm_lower` (or at minimum
  `sm_resolve_proc_entry_pcs`) from the `--ir-run` path before
  `polyglot_execute`.  Gate: corpus baseline byte-identical;
  `entry_pc` resolves to `>=0` for every proc in
  `SCRIP_PROC_ENTRY_PCS=1` output regardless of mode.

Once both land, CH-17g-final can delete the legacy body cleanly.

Alternative framing: merge CH-17h, CH-17g-runtime-bridge, and
CH-17g-irrun-lowers into CH-17g-final as a single rung — they are
the coupled set of work that CH-17h-SURVEY's recommendation
implicitly assumed was already done.

## Files touched (all reverted before commit)

`src/runtime/interp/coro_runtime.c` — temporary `fprintf` probes in
`coro_call` and `proc_table_call`.  `git diff` confirmed empty before
this doc was written.

## Gates re-confirmed after revert

| Gate | Result |
|------|--------|
| smoke snobol4 | PASS=7 |
| smoke icon | PASS=5 |
| smoke raku | PASS=5 |
| smoke prolog | PASS=5 |
| smoke snocone | PASS=5 |
| smoke rebus | PASS=4 |
| isolation | PASS |
| unified_broker | PASS=49 |
| Icon corpus `--ir-run` | PASS=186 FAIL=47 XFAIL=30 TOTAL=263 |

All byte-identical to the CH-17g-statics post-land baseline.
