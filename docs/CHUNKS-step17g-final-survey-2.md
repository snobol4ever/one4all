# CH-17g-final-SURVEY-2 — preconditions are NOT yet met

**Session:** 2026-05-09 (post CH-17g-irrun-lowers + CH-17-RENAME-FINAL)
**Verdict:** Do not land CH-17g-final today.  CH-17g-irrun-lowers
landed correctly as a structural addition, but it does not satisfy
its asserted role as a precondition for CH-17g-final.  Three live
consumers of `proc_table[i].proc` remain — one is producer-side
(harmless), two are runtime-side (load-bearing under `--ir-run`).

This survey supersedes the "preconditions met" line at the bottom
of `CHUNKS-step17g-irrun-lowers-validation.md` and the matching
PLAN.md table line "Both CH-17g-final preconditions met
(runtime-bridge + irrun-lowers)".

## What CH-17g-final's spec demands

From `GOAL-CHUNKS-STEP17.md` `#### CH-17g-final` (lines 419–446):

  - delete `AST_t *proc` field from `IcnProcEntry`
  - delete the legacy body of `coro_call(AST_t*, ...)` (its scope
    build, its `interp_eval` body loop) and the `coro_drive_fnc`
    wrapper
  - lift `lang_mask == (1u << LANG_SNO)` gate on `code_free` in
    `scrip_sm.c` — IR freed unconditionally for all six frontends

Each of these requires that no live execution path read a field
or table whose lifetime depends on the IR.

## Empirical probe

Single-line diagnostic patch in `src/driver/polyglot.c:178`:

```c
proc_table[proc_count].proc     = NULL; /* CH-17g-final probe */
```

Trivial Icon program:

```icon
procedure main()
  write("hello from icon")
end
```

Results:

| Mode        | Without probe                | With proc=NULL probe          |
|-------------|------------------------------|-------------------------------|
| `--ir-run`  | `hello from icon\n` (rc=0)   | **SIGSEGV** (rc=139)          |
| `--sm-run`  | `hello from icon\n` (rc=0)   | empty output (rc=0)           |
| `--dump-sm` | (multi-instr chunk)          | 2 instrs: `SM_BB_PUMP_PROC`, `SM_HALT` |

The `--sm-run` empty-output result is the more telling failure:
the SM_Program is structurally truncated.  No FATAL, no abort —
the program runs end-to-end, but the chunk is empty.

## The three live consumers of `proc_table[i].proc`

`grep -n 'proc_table\[.*\]\.proc' src/runtime/interp/coro_runtime.c
src/runtime/x86/sm_lower.c src/driver/polyglot.c`:

### 1. `sm_lower.c:1757` — producer-side (harmless in principle, blocking in practice)

```c
for (int pi = 0; pi < proc_count; pi++) {
    const char *nm = proc_table[pi].name;
    if (!nm || !*nm) continue;
    AST_t *proc = proc_table[pi].proc;       /* registered by polyglot_init */
    ...
    if (proc) {
        int nparams    = (int)proc->ival;
        int body_start = 1 + nparams;
        /* lower each body child, emit chunk ops */
    }
}
```

This is producer-side: it runs while IR is alive (just after
`polyglot_init`, before `code_free` — both gated paths).  Reading
`proc->children[]` here is what produces the chunk body.

**Why it blocks deletion of the field**: even if every runtime-
side consumer were retired, the producer still has to read the
proc body somewhere to emit it.  That "somewhere" reads through
*some* IR pointer.  Today it is `proc_table[i].proc`.  After
CH-17g-final's prescribed deletion, it would have to be a
parallel structure — likely a separate `AST_t *proc_ir[]` array
that lives only between `polyglot_init` and `sm_lower`, then is
freed.  This is a small refactor, not a blocker, but it is work
not described in CH-17g-final's spec.

### 2. `coro_runtime.c:603` — `proc_table_call` legacy fallback (load-bearing under `--ir-run`)

```c
DESCR_t proc_table_call(int pi, DESCR_t *args, int nargs) {
    if (pi < 0 || pi >= proc_count) return FAILDESCR;
    extern SM_Program *g_current_sm_prog;
    if (proc_table[pi].entry_pc >= 0 && g_current_sm_prog != NULL)
        return sm_call_proc(proc_table[pi].entry_pc, ...);
    return coro_call(proc_table[pi].proc, args, nargs);  /* ← live under --ir-run */
}
```

Under `--ir-run`, `g_current_sm_prog == NULL` (`sm_resolve_irrun_entry_pcs`
discards the SM_Program with `sm_prog_free` immediately after
populating entry_pcs).  The guard short-circuits the SM path; the
legacy `coro_call(proc_table[pi].proc, ...)` path runs every
`main()` call and every user-proc invocation.

The probe's `--ir-run` SIGSEGV proves this: with proc=NULL,
`coro_call`'s scope-build dereferences NULL.

### 3. `coro_runtime.c:1179, 1267, 1557, 1775` — trampoline staging + `coro_drive_fnc`

```c
ss->gather_proc     = proc_table[pi].proc;        /* line 1267 */
coro_stage.proc     = proc_table[i].proc;         /* lines 1179, 1557 */
AST_t *proc   = proc_table[pi].proc;              /* line 1775 — coro_drive_fnc */
```

Each staging site has a parallel entry_pc/nparams pair already
(`coro_stage.entry_pc`, `ss->gather_entry_pc`, …).  The trampolines
dispatch on `if (entry_pc >= 0) sm_call_proc else coro_call(proc, ...)`.
Same shape as `proc_table_call`: legacy AST_t* path is the
fallback used when SM is not live, which is every `--ir-run` non-SNO
program.

`coro_drive_fnc` (line 1775) is the "IR walker by design"
explicitly called out in `CH-17g-call-sites LANDED` ("intentionally
NOT flipped — IR walker by design, M4-cleanup territory (CH-17h)").

## Why CH-17g-irrun-lowers does not satisfy its asserted role

CH-17g-irrun-lowers's validation doc says:

> Entry_pcs visible under SCRIP_PROC_ENTRY_PCS=1 in both modes.

This is true and useful for *observability* (a future tool can
report entry_pcs without changing execution mode).

But the same doc also says, accurately:

> Entry_pc resolved + SM absent → fall through to legacy
> `coro_call` / `interp_eval` / `pl_box_choice` path.  This is
> the correct --ir-run behaviour: entry_pcs resolve for
> observability/future use, but execution remains on the IR path.

That clause — "execution remains on the IR path" — is exactly
the structural fact that blocks CH-17g-final.  Deleting the IR
path while `--ir-run` non-SNO depends on it would break the
entire 186/47/30 Icon corpus and Prolog corpus baseline.

CH-17g-final-SURVEY (2026-05-09, the original) caught the
runtime-side gap and recommended splitting the precondition
into runtime-bridge + irrun-lowers.  The runtime-bridge half
was about making `--sm-run` produce correct output (delivered:
27 builtin names bridged + SM_ACOMP/SM_LCOMP).  The irrun-lowers
half was framed as "make `entry_pc` resolve in `--ir-run`" — but
*resolving the value* is necessary, not sufficient.  *Executing
through it* is what CH-17g-final actually requires.

## What an actually-sufficient set of preconditions looks like

To delete `proc_table[i].proc` without breaking `--ir-run`,
`--ir-run` non-SNO must run on chunks, not on `coro_call`'s
legacy body.  That means one of:

### Option A — `--ir-run` becomes an alias for `--sm-run` for non-SNO

Simplest.  Drops a user-visible mode contract that today exists
(non-SNO `--ir-run` → `polyglot_execute` walks IR; `--sm-run`
→ chunks via `sm_call_proc`).  The two outputs are already
byte-identical for bridged programs, so the alias is observable
only in failure modes (a chunk gap that the IR path doesn't have).

This is a policy change, not a code-correctness change.  Needs
your call.

### Option B — `--ir-run` keeps SM live (don't `sm_prog_free` it)

Change `sm_resolve_irrun_entry_pcs` to retain the SM_Program
after populating entry_pcs — set `g_current_sm_prog = sm` —
and free it on process exit.  The dispatch guards
(`g_current_sm_prog != NULL`) then take the SM path under
`--ir-run` too.  `coro_call`'s legacy body becomes unreachable,
the field can be deleted, the gate can be lifted.

Cost: SM_Program lives twice as long; memory bumps by
`sizeof(SM_Program) + instrs*sizeof(SM_Instr)` for the duration
of execution.  Negligible for the tested corpus.

Risk: anything in `polyglot_execute`'s BB engine that today
walks live IR (E_VAR.ival mutation in `icn_scope_patch`, the
proc tables) must continue to work.  The SM dispatch path is
already proven to handle hello-world; it has not been proven to
handle every Icon corpus program — `--sm-run`'s known gaps
(meander.icn `tab()`, queens.icn array references, etc.) would
become `--ir-run`'s gaps too unless bridged.

### Option C — keep both paths; rename `--ir-run` to acknowledge the legacy walker

Land everything except deleting `proc_table[i].proc` and
`coro_call`'s body.  Rename the gate to mirror reality: the
"unconditional" `code_free` in CH-17g-final's spec becomes
"conditional on execution mode" instead of "conditional on
language".

This is the smallest code change but the largest spec change.
GOAL-CHUNKS Step 17's stated closure criterion ("IR freed
unconditionally for all six frontends") would need amendment.

## Producer-side cleanup is independent

The `sm_lower.c:1757` reader is a separate concern that any of
A/B/C must address: lowering needs to walk the proc body once.
Three options:

  - leave it alone — sm_lower runs while IR is alive in all three
    options, the field exists at lowering time, dies after.
  - introduce a separate `AST_t **proc_ir` array used only between
    `polyglot_init` and `sm_lower`, freed after — eliminates the
    field but adds a parallel structure.
  - inline the lowering loop into `polyglot_init` so the field
    is read once at registration time and never stored —
    largest refactor, cleanest end state.

I recommend deferring this decision until Option A/B/C is chosen.

## Gates with the probe (for the record)

With `proc_table[proc_count].proc = NULL`:

  - `--ir-run` /tmp/probe.icn: SIGSEGV
  - `--sm-run` /tmp/probe.icn: empty output, rc=0 (chunk truncated to SM_BB_PUMP_PROC + SM_HALT)

Probe reverted before commit.

## Decision (Lon, sess 2026-05-09)

**Option A.**  Rationale: AST and SM are both deleted between
phases to enforce separation and isolation.  Option B (keep SM
live under `--ir-run`) violates that principle.  Option C
(amend Step 17's "free unconditionally" criterion) preserves
the legacy AST walker indefinitely and likewise violates it.
Option A is the only path that ends with one execution mode,
one set of consumers, and IR/SM both freed between phases.

User-facing impact: `--ir-run` and `--sm-run` produce identical
output for non-SNO programs; the flag distinction collapses.
SNOBOL4 retains both because SNOBOL4 has its own non-SM
interpreter (`execute_program`) that is an entirely separate
path from the AST walker this rung retires.

## Carved rung: CH-17g-irrun-execution

Distinct from CH-17g-irrun-lowers (which delivered observability:
entry_pcs visible).  Scope:

  - `scrip.c` non-SNO `--ir-run` dispatch: route to the same
    `sm_preamble` + `sm_run_with_recovery` path as `--sm-run`.
  - Drop `g_irrun_lowers` flag and `sm_resolve_irrun_entry_pcs`
    helper — superseded; the SM_Program now lives across
    execution under `--ir-run` non-SNO via the standard
    `sm_preamble` path, not via a discard-after-resolve hook.
  - Verify byte-identical output for the Icon corpus 186/47/30
    and Prolog smoke under the new dispatch.
  - SNOBOL4 path unchanged; legacy `--ir-run` SNOBOL4 dispatch
    in `scrip.c:557–561` retained as-is.

After CH-17g-irrun-execution:
  - the legacy `coro_call(AST_t*, ...)` body becomes unreachable
  - the four trampoline-staging .proc readers can be deleted
  - `proc_table[i].proc` field can be deleted
  - producer-side `sm_lower.c:1757` cleanup (separate AST_t** array
    used only between polyglot_init and sm_lower) can land in the
    same or a follow-on rung
  - `code_free` gate can be lifted unconditionally

Then CH-17g-final closes Step 17.

## Files

  - this doc (new)
  - probe was `src/driver/polyglot.c:178` — reverted

## Gates

  - smoke icon: PASS=5 FAIL=0 (clean baseline, post-revert)
  - build: clean
  - --ir-run hello.icn: PASS
  - --sm-run hello.icn: PASS
