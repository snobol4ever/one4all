# CHUNKS-step16-survey — Prolog cluster (sm_lower.c:1213) reachability + consumer-side blocker

**Session #75, 2026-05-07.  Watermark: post-CH-15-SURVEY (one4all `5d240d10`).**

This is a survey rung, not a producer migration.  It records two
empirical findings about the Step 16 territory (E_CHOICE, E_CLAUSE,
E_CUT, E_UNIFY, E_TRAIL_MARK, E_TRAIL_UNWIND at `sm_lower.c:1213`)
that shape sequencing.

The survey shape mirrors CHUNKS-step15-survey: characterize whether
the producer fires on real programs, characterize the consumer
side, recommend.

## Finding A — the producer DOES fire on real Prolog programs

Unlike CH-15-SURVEY (where the Icon dispatcher arm at sm_lower.c:1192–
1204 was empirically dead on every audited program because Icon proc
bodies route through `coro_eval` upstream of `sm_lower`), the Prolog
cluster at sm_lower.c:1213 fires for **every** Prolog statement that
reaches `sm_lower`.

Static evidence (`scrip --dump-sm`):

| Program | SM_PUSH_EXPR + SM_BB_ONCE pairs in SM_Program |
|---------|-----------------------------------------------|
| test/prolog/hello.pl       | 2 |
| test/prolog/palindrome.pl  | 3 |
| test/prolog/queens.pl      | 7+ |
| test/prolog/roman.pl       | 3 |
| test/prolog/sentences.pl   | 7+ |
| test/prolog/wordcount.pl   | 5+ |

Tagged-call-site instrumentation on `emit_push_expr` (env-gated
`[PE-SITE-B]` print at the Prolog case in `sm_lower.c:1213`)
confirms the case dispatches on E_CHOICE (kind 59) and similar
neighbours for each Prolog clause body lowered.

Note on numeric kind values: the `enum EXPR_e` line numbers in
`src/ir/ir.h` are SIL legacy reference comments, NOT the actual
enum values.  At runtime, `E_DEFER=8`, `E_UNIFY=57`, `E_CLAUSE=58`,
`E_CHOICE=59`, `E_CUT=60`, `E_TRAIL_MARK=61`, `E_TRAIL_UNWIND=62`,
`E_FNC=45`.  Earlier confusion in this session: a FATAL
`bb_eval_value: unhandled kind 59` was initially read as an E_DEFER
problem; the actual culprit is E_CHOICE.

## Finding B — the consumer is broken; fixing it depends on Step 17

`SM_BB_ONCE`'s handler in `sm_interp.c:738–749` (mirrored in
`sm_codegen.c:317–325`) does:

```c
DESCR_t expr_d = sm_pop(st);
EXPR_t *expr   = (EXPR_t *)expr_d.ptr;
bb_node_t node = coro_eval(expr);
int ticks = bb_broker(node, BB_ONCE, NULL, NULL);
```

`coro_eval` (`coro_runtime.c:1049`) does not switch on any of the
six Prolog kinds.  E_CHOICE/E_CLAUSE/E_CUT/E_UNIFY/E_TRAIL_*
fall through to the oneshot fallback at line 1610, which calls
`bb_eval_value(e)`.  `bb_eval_value` (`coro_value.c:377`) does not
handle these kinds either; it FATALs as
"unhandled kind N (RS-23e isolation breach)".

Empirical: every program in `test/prolog/*.pl` (6 of 6) aborts
under `--interp` before atexit fires — the audit summary cannot
print because the process SIGABRTs first.  The Prolog smoke gate
(`scripts/test_smoke_prolog.sh`) does not exercise `--interp`; it
only runs `--ir-run`.  Hence smoke is green while every Prolog
program crashes in SM mode.

**The right consumer for these kinds already exists** — in
`src/frontend/prolog/pl_broker.h`:

| Kind            | Right BB-box constructor                                  |
|-----------------|-----------------------------------------------------------|
| E_CHOICE        | `pl_box_choice(EXPR_t *choice, Term **caller_args, int arity)` |
| E_CLAUSE        | `pl_box_clause(EXPR_t *ec, Term **caller_args, int arity)`     |
| E_CUT           | `pl_box_cut()`                                                  |
| E_UNIFY         | `pl_box_builtin(EXPR_t *goal, Term **env)` (interp_exec_pl_builtin dispatches)  |
| E_TRAIL_MARK    | same — `pl_box_builtin`                                         |
| E_TRAIL_UNWIND  | same — `pl_box_builtin`                                         |

But every constructor takes either an **EXPR_t pointer** or a
**Term env**.  No SM_chunk-shaped variant exists today.  The Prolog
runtime is deeply IR-bound — Term construction reads `e->kind`,
`e->sval`, `e->ival`, `e->children[]`; head unification walks the
clause head's children; trail/env scoping is keyed on EXPR_t identity.

A faithful Step 16 — "lower each kind as a chunk that ends in
SM_RETURN; consumer pops chunk + drives via Prolog BB" — has no
backing API to call into.  The choice is:

1. **Build chunk-shaped Prolog runtime entry points first.**
   `pl_box_choice_pc(int entry_pc, Term **args, int arity)` and
   friends, where `entry_pc` is interpreted by the BB engine
   running SM dispatch on the chunk body.  This is approximately
   half of Step 17.

2. **Migrate the producer to chunks while leaving the consumer at
   `coro_eval`.**  Trades one broken pathway (legacy SM_PUSH_EXPR →
   `coro_eval` → `bb_eval_value` FATAL) for another (SM_PUSH_CHUNK →
   `coro_eval` → still no handler for these kinds — same FATAL).
   Net change: zero observable behaviour difference, but with a
   chunk in the descriptor that no Prolog code path knows how to
   read.  Worse than the legacy state, not better.

3. **Land a partial migration (E_CUT only, say) where the consumer
   can be wired through `pl_box_cut()` without IR walking.**
   E_CUT's runtime is `g_pl_cut_flag = 1`; no children, no env.
   This is the only kind that survives consumer migration without
   Step 17.  The other five all carry IR sub-trees the runtime
   walks at match time.

## Finding C — sm_lower.c:1402 is a second SM_BB_ONCE producer

Independent of the line-1213 site, **every** Prolog statement gets
an unconditional `SM_BB_ONCE` at the statement-level wrapper
(sm_lower.c:1395–1404):

```c
if (s->lang == LANG_PL) {
    if (s->subject) lower_expr(p, lt, s->subject);
    else            sm_emit(p, SM_PUSH_NULL);
    sm_emit(p, SM_BB_ONCE);
    goto emit_gotos;
}
```

Static dump shows the consequence: every Prolog statement emits
either `SM_BB_ONCE` (when subject lowers cleanly to a value, like
the `:- initialization(main, main)` directive at pc=4 of
`hello.pl`) OR `SM_PUSH_EXPR + SM_BB_ONCE + SM_BB_ONCE` (where
the inner subject's lowering hit case 1213 and emitted its own
SM_PUSH_EXPR + SM_BB_ONCE pair, then the wrapper emitted a second
SM_BB_ONCE on top).

The "two stacked SM_BB_ONCE" shape is structurally wrong by
itself — the second one pops a value pushed by the first
(BB tick count, an integer), not a DT_E.  This is a pre-existing
bug in the Prolog statement lowering, not Step 16's scope, but it
means the line-1402 producer also needs migration to a
chunk-shaped wrapper before --interp Prolog can possibly work.

The line-1402 site is therefore Step 16-adjacent: closing Step 16
without addressing line-1402 leaves --interp Prolog still broken,
even after the line-1213 producer migrates correctly.

## Recommendation

Defer the full Step 16 migration until **after Step 17 lands**.
Step 17 establishes:

  - `proc_table[i].entry_pc` — a chunk pc per Icon proc
  - `pl_pred_table_insert(name, entry_pc)` — a chunk pc per Prolog
    predicate
  - the surrounding plumbing for `sm_call_proc(int entry_pc, ...)`

Once that infrastructure exists, the per-kind SM lowering in
Step 16 can be implemented with a clear consumer-side target:
`pl_box_choice_pc`, `pl_box_clause_pc`, etc., layered on top of
Step 17's entry_pc machinery.

Validation discipline at that point becomes: **Prolog smoke gate
extended to --interp**, plus full Prolog corpus crosscheck.
Today neither is possible because every program aborts.

This matches the precedent set by CH-15-SURVEY: when the producer
migration would land in front of a non-functional consumer, the
right move is to wait for the unblocking step and then validate
with real corpus diff.

## Difference from CH-15-SURVEY

CH-15 (Icon generators): producer arm is **dead** today, migration
is infrastructure prep with no behavioural effect either way.

CH-16 (Prolog cluster): producer arm is **live and feeding into a
broken consumer**, migration without a consumer fix would be
no-op-or-worse, migration with a consumer fix needs Step 17's
entry_pc infrastructure first.

Both surveys converge on the same recommendation — defer until
Step 17 — for different reasons.

## Next inline pointer

After this rung lands, the GOAL-CHUNKS active line is:

  - **Step 17** (proc/pred table → entry_pcs) — the architectural
    unblock that gates real validation of both Step 15 (Icon
    remaining kinds) and Step 16 (Prolog cluster).

Step 16 returns to active when Step 17 closes.

## Files touched by this rung

- `docs/CHUNKS-step16-survey.md` (this file, new)
- `GOAL-CHUNKS.md` (Step 16 line annotated to reference this survey)
- `PLAN.md` (CHUNKS row updated; next-inline pointer flipped to Step 17)

No source code touched.

## Gates

  - smoke ×6 PASS (SNOBOL4 7/7, Icon 5/5, Prolog 5/5, Raku 5/5,
    Snocone 5/5, Rebus 4/4)
  - isolation gate PASS
  - unified_broker PASS=49

csnobol4 Budne not run — csnobol4 binary not built in this session
container; survey rung does not touch any code paths Budne
exercises.

Empirical reach for Finding A: 6 `test/prolog/` programs, every
one shows ≥1 SM_PUSH_EXPR + SM_BB_ONCE pair in static SM dump.
Tagged-site instrumentation on a synthetic 2-clause program
(`/tmp/simple.pl`) confirmed the line-1213 case dispatches on
E_CHOICE.

Empirical reach for Finding B: 6 of 6 `test/prolog/` programs
abort under `--interp` before atexit fires; FATAL is
`bb_eval_value: unhandled kind 59 (RS-23e isolation breach)`
where 59 = E_CHOICE in the runtime enum.

one4all @ HEAD pre-rung: `5d240d10`.  Session #75, 2026-05-07.
