# CHUNKS-step17a-validation — entry_pc scaffolding

**Session #75 (continued), 2026-05-07.  Watermark: post-CH-16-SURVEY
(one4all `86167095`).**

CH-17a is the first rung of the Step 17 sub-goal carved out into
`GOAL-CHUNKS-STEP17.md`.  Pure addition: no producer flips, no
consumer flips, no observable behaviour change.

## What landed

1. `IcnProcEntry` (in `src/runtime/interp/coro_runtime.h`) gains an
   `int entry_pc` field, after the existing `EXPR_t *proc`:

   ```c
   typedef struct {
       const char *name;
       EXPR_t     *proc;
       int         entry_pc;   /* CH-17a: SM pc of named proc-body chunk; -1 today */
   } IcnProcEntry;
   ```

2. `Pl_PredEntry` (in `src/runtime/interp/pl_runtime.h`) gains an
   `int entry_pc` field, after the existing `EXPR_t *choice`:

   ```c
   typedef struct Pl_PredEntry_t {
       const char *key;
       EXPR_t     *choice;
       struct Pl_PredEntry_t *next;
       int         entry_pc;   /* CH-17a: SM pc of named pred chunk; -1 today */
   } Pl_PredEntry;
   ```

3. Both fields are initialised to `-1` at insertion time:
   - `polyglot.c:172` writes `entry_pc = -1` after the `proc` write.
   - `pl_pred_table_insert` writes `entry_pc = -1` on every new entry.

4. `scrip_sm.c` gains a static helper
   `sm_resolve_proc_entry_pcs(SM_Program *p)` invoked immediately
   after `sm_lower(prog)` returns successfully.  The helper walks
   `proc_table` and `g_pl_pred_table`, looks up each name via
   `sm_label_pc_lookup(p, name)`, and stores the result into the
   matching `entry_pc` slot.  Today every lookup returns -1
   because sm_lower does not yet emit named chunks for proc bodies
   (CH-17b/d territory).

5. Diagnostic env var `SCRIP_PROC_ENTRY_PCS=1` prints the
   resolution summary per program.  Empirical output:

   ```
   $ SCRIP_PROC_ENTRY_PCS=1 ./scrip --interp test/icon/hello.icn
   [CH-17a] resolve entry_pcs (proc_table=1 procs, pl_pred_table=hash)
   [CH-17a]   proc[0] name=main                 entry_pc=-1
   [CH-17a] summary: pl_total=0 pl_resolved=0 (others=-1 are CH-17b/d territory)
   Hello, World!

   $ SCRIP_PROC_ENTRY_PCS=1 ./scrip --interp /tmp/simple.pl
   [CH-17a] resolve entry_pcs (proc_table=0 procs, pl_pred_table=hash)
   [CH-17a]   pred  key=fact/1               entry_pc=-1
   [CH-17a]   pred  key=main/0               entry_pc=-1
   [CH-17a] summary: pl_total=2 pl_resolved=0 ...
   <pre-existing CH-16-SURVEY FATAL still fires here>

   $ SCRIP_PROC_ENTRY_PCS=1 ./scrip --interp test/snobol4/hello/hello.sno
   [CH-17a] resolve entry_pcs (proc_table=0 procs, pl_pred_table=hash)
   [CH-17a] summary: pl_total=0 pl_resolved=0 ...
   HELLO WORLD
   ```

   For SNOBOL4 the resolver runs but has nothing to look up (no
   procs, no predicates) — zero overhead path.

## Files touched

- `src/runtime/interp/coro_runtime.h` — `IcnProcEntry` field add
- `src/runtime/interp/pl_runtime.h` — `Pl_PredEntry` field add
- `src/runtime/interp/pl_runtime.c` — `pl_pred_table_insert`
  initialiser
- `src/driver/polyglot.c` — `proc_table[*].entry_pc = -1` at population
- `src/driver/scrip_sm.c` — new helper + call site

Five files; pure addition.  No deletions; no signatures changed; no
existing code paths altered.

## What this rung does NOT do

- Does NOT make `sm_lower` emit named chunks for any proc.
  Every `entry_pc` resolves to -1 today.  CH-17b emits Icon/Raku
  proc-body chunks; CH-17d emits Prolog predicate chunks.
- Does NOT change consumer code.  `coro_call`, `coro_drive`,
  `coro_value`, `pl_box_choice`, `interp_hooks`, `polyglot.c`'s
  Icon and Prolog dispatch still walk `EXPR_t *proc` /
  `EXPR_t *choice` exactly as before.  CH-17c/e flip those.
- Does NOT delete the `EXPR_t *proc` field.  CH-17g does that
  after every consumer is on entry_pcs.
- Does NOT lift the `code_free` gate.  CH-17g handles that too.
- Does NOT fix the broken `--interp` Prolog path documented in
  CH-16-SURVEY.  That requires CH-17e (consumer-side migration
  for Prolog).

## Gates

  - smoke ×6 PASS (SNOBOL4 7/7, Icon 5/5, Prolog 5/5, Raku 5/5,
    Snocone 5/5, Rebus 4/4) — all byte-identical to baseline
  - isolation gate PASS (`no IR-only symbol leaks in SM runtime files`)
  - `unified_broker` PASS=49 — byte-identical to baseline
  - `SCRIP_CHUNKS_AUDIT=1` regressions: zero — Icon hello still
    audits SM_PUSH_CHUNK=0 SM_PUSH_EXPR=0; Raku rk_given still
    audits SM_PUSH_CHUNK=20 SM_PUSH_EXPR=0 (CH-13 baseline).

csnobol4 Budne not run — csnobol4 binary not built in this session
container; CH-17a does not touch any code paths Budne exercises.

## Why this rung was carved into a sub-goal

GOAL-CHUNKS.md's Step 17 paragraph is one sentence per concern but
collectively touches:

- the proc_table struct and every consumer
- `coro_call`'s scope build, env init, body loop, static-var
  persistence
- the Prolog pred table and every consumer (`pl_box_choice`,
  `pl_box_clause`, `pl_box_builtin`, `pl_pred_table_lookup`)
- `polyglot.c`'s Icon and Prolog dispatch paths
- `sm_lower`'s coverage (Icon proc bodies and Prolog clauses
  must be lowered as named chunks)
- the `code_free` gate in `scrip_sm.c`
- the isolation gate's structural rules

That's a multi-session ladder.  `GOAL-CHUNKS-STEP17.md` (new in
session #75) names the rungs CH-17a..CH-17h and gives each its
own gate set.  CH-17a is the smallest possible scaffolding rung
that lets later rungs populate entry_pcs without re-touching
struct definitions every time.

## Closed-rung pointer

CH-17a closes.  Next inline: **CH-17b** — sm_lower emits named
proc-body chunks for Icon/Raku procs.

one4all @ HEAD pre-rung: `86167095`.  Session #75, 2026-05-07.
