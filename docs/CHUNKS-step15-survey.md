# CHUNKS-step15-survey — empirical reachability of remaining Step 15 kinds

**Session #74, 2026-05-07.  Watermark: post-CH-15a (one4all `dd673da1`).**

This is a survey rung, not a producer migration.  It records an
empirical finding that shapes how the remaining CHUNKS Step 15
sub-rungs (CH-15b onward) are sequenced.

## Finding

The legacy fall-through block in `src/runtime/x86/sm_lower.c:1192–1204`
that routes E_EVERY, E_SUSPEND, E_BANG_BINARY, E_LCONCAT, E_LIMIT,
E_RANDOM, E_SECTION, E_SECTION_PLUS, E_SECTION_MINUS through
`emit_push_expr + SM_BB_PUMP` is **not exercised** by any program
audited today.  Two surveys ran:

**`test/` corpus** (per-frontend smoke fixtures): 46 audited programs,
**0 fire `SM_PUSH_EXPR`** under `--interp` with `SCRIP_CHUNKS_AUDIT=1`.
2 programs hit `SM_PUSH_CHUNK=20` (Raku CASE per CH-13); the rest
hit `SM_PUSH_CHUNK=0`.

**Cross-language corpus** (`/home/claude/corpus/programs/`):

```
lang          audited expr-fired chunk-fired
icon              200          0          0
snocone            47          0          0
raku               39          0          5
scrip              21          0          2
snobol4             6          0          1
prolog              4          0          0
```

(Most Prolog corpus programs error before reaching audit-summary
under `--interp`; that itself is consistent with the broader
picture — Prolog clauses reach `sm_lower` only for the small subset
that the current dispatcher handles.  Step 16's territory.)

The Icon row is the headline result: **200 Icon programs from the
authoritative corpus, every one audits `SM_PUSH_EXPR=0`**.  Of the
118 Icon corpus programs that explicitly use Icon generator
operators (per a separate keyword survey
`grep -lE "every |\\\\|!" *.icn`), every audited program shows
`SM_PUSH_EXPR=0` — those programs don't exercise the dispatcher
arm in `--interp` mode.

## Why

Confirmed by re-reading CH-15a's closed-rung note in `GOAL-CHUNKS.md`:

> Icon `every i := 1 to 5 do …` does NOT audit a chunk emission —
> that path runs through `coro_pump_proc_by_name("main", …)` →
> `coro_eval(main_body)`, pure IR walking inside the proc body.
> Once Step 17 lowers Icon proc bodies through sm_lower,
> every-bodies containing `1 to n` will hit the new chunk emission
> automatically; the Raku-side firing today proves the producer
> wiring is end-to-end.

In other words: the Icon proc-body IR walk in `coro_eval` is upstream
of `sm_lower` for everything except the Raku top-level expressions
that already route through the SM dispatcher.  Until Step 17
(proc_table → entry_pcs) lands, the dispatcher arm we would migrate
in CH-15b onward sees no real input.

## Implication for sequencing

CH-15b — bundling any of the remaining nine kinds — is **infrastructure
preparation, not a behaviour change**.  The migration would lay down
SM-chunk recipes whose first real test case arrives only after Step 17.
Two consequences:

1. The standard "exercise via real program + diff against oracle"
   validation pattern used by CH-15a does not apply.  Validation
   must be hand-built (audit on a synthetic program, or a unit test
   that constructs an EXPR_t and feeds it through `lower_expr`).

2. Sequencing options:

   - **Option A — Defer until Step 17 lands.**  Then real programs
     exercise the dispatcher, every CH-15b sub-rung gets its
     corpus diff, and the producer/consumer pairs land with their
     normal validation discipline.  Trade: longer wall-clock
     before Step 15 closes; cleaner per-rung gates.

   - **Option B — Migrate now with synthetic gates.**  Hand-build
     a Raku-flavored test for each migrated kind that routes
     through the SM dispatcher today.  Trade: more synthetic-test
     code; faster Step 15 closure; possible drift if Step 17
     reveals semantics the synthetic test missed.

The value of laying down the recipe early (Option B) is constrained
by another structural observation: the remaining kinds split into
two classes by composability:

| Class | Kinds | Inner generator? |
|-------|-------|------------------|
| Self-contained | E_RANDOM, E_SECTION, E_SECTION_PLUS, E_SECTION_MINUS | No — value→value |
| Generator consumers | E_EVERY, E_SUSPEND, E_LIMIT, E_BANG_BINARY, E_LCONCAT | Yes — pump α/β on a child BB box |

Self-contained kinds are direct analogues of E_TO/E_TO_BY: a
fixed chunk shape, no recursive composition.  They could be
migrated under Option B with low risk.

Generator consumers face a real architectural question.  The inner
child gen may be:
  (a) an already-migrated generator → composes naturally via a new
      `SM_BB_PUMP_INNER_SM` opcode that drives an inner chunk;
  (b) an un-migrated kind (E_FNC user proc, in particular) → must
      still fall back through `coro_eval`, which re-introduces an
      EXPR_t pointer into the SM-mode runtime path — contradicting
      the goal's spirit.

The cleanest migration order is therefore: **wait for Step 17**
(which converts the dominant class (b) producer — proc bodies — to
chunk-driven), then migrate consumers, knowing every inner gen is
reachable as a chunk.

## Recommendation

Defer CH-15b.  Reorder Step 15 to land **after** Step 17.

Practically, this means the next inline rung from CH-15a should be
Step 16 (Prolog clauses) or Step 17 (proc_table) rather than
CH-15b.  Step 16 is the smaller unit; Step 17 is the architectural
unblock.  Lon's call which to take next.

If a future session disagrees — the recipe value of laying down a
self-contained-kind chunk template now (E_RANDOM is the cleanest)
is real even without behaviour change — that session can pick up
CH-15b under Option B with E_RANDOM alone, leaving consumer kinds
for post-Step-17.

## Files touched by this rung

- `docs/CHUNKS-step15-survey.md` (this file, new)
- `GOAL-CHUNKS.md` (Step 15 note updated to reference this survey)
- `PLAN.md` (CHUNKS row updated; next-inline pointer flipped to Step 16/17)

No source code touched.  Gates: smoke ×6 PASS (7/7, 5/5, 5/5, 5/5,
5/5, 4/4); isolation gate PASS; unified_broker PASS=49.
csnobol4 Budne not run — csnobol4 binary not built in this session
container; survey rung does not touch any code paths that Budne
exercises.

Empirical audit reach: 46 `test/` programs + 317 cross-corpus
programs (200 Icon + 47 snocone + 39 Raku + 21 scrip + 6 SNOBOL4
+ 4 Prolog) = **363 audited programs, zero `SM_PUSH_EXPR` fires**
in `--interp`.

one4all @ HEAD pre-rung: `dd673da1`.  Session #74, 2026-05-07.
