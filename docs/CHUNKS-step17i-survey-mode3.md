# CHUNKS-step17i-survey-mode3 — Icon + Prolog `--interp` gap audit

**Rung:** CH-17i-survey-mode3  
**Date:** 2026-05-09  
**one4all HEAD:** `b19f75ba` (pre-survey; no source touched)

---

## Supported-surface definition

**Icon:** corpus `programs/icon/*.icn` + `test/icon/coverage/*.icn`.  
Survey: 177 programs reach `--ir-run` PASS vs `.expected` oracle. 30 XFAIL (known).

**Prolog:** `test/prolog/*.pl` + `test/prolog/coverage/*.pl`.  
Survey: 4 programs produce non-empty `--ir-run` output (hello, palindrome, roman, coverage_net_gaps).

---

## Icon results: 177 PASS, 111 diverge under `--interp`, 30 XFAIL

Zero missing builtins. Zero missing opcodes. All 111 failures are **semantic**.

### Root-cause analysis

Every failing program has a `procedure` body that is lowered as a chunk. The chunk
body contains generator or scan operations emitted as the legacy `SM_PUSH_EXPR + SM_BB_PUMP`
shape (CH-17b' gating note: generator kinds emit legacy until CH-17h reactivates CH-15b).
When `sm_call_proc` dispatches into the chunk via `entry_pc`, the legacy SM_PUSH_EXPR
instruction fires — it pushes a raw `AST_t*` as a DESCR_t onto the SM stack.
`SM_BB_PUMP` tries to drive it, fails to find a consumer context, and the stack
underflows. Programs where the proc body contains ONLY non-generator ops (write,
arithmetic, string calls bridged in bridge-1..4) succeed; programs with any generator
or scan usage in a user proc fail.

**Two sub-buckets within SEMANTIC:**

1. **Stack underflow** (≈60 programs) — proc contains `every`, `to`/`to-by`, `!` (bang),
   `|` (alternation), `\`/`limit`, section expressions, or any generator-producing
   expression. `SM_PUSH_EXPR` pushes a raw AST_t; `SM_BB_PUMP`/`SM_BB_PUMP_SM` fires,
   finds no generator frame, aborts with `sm_interp: stack underflow`.

2. **Silent empty or Error 5** (≈51 programs) — proc body uses AUGOP (`:= +=` etc.),
   scan-context builtins (`tab`, `move`, `find`, `match`, `upto`, `any`, `many`),
   or list/table mutation ops (`get`, `put`, `push`, `pop`, `pull`). These emit
   SM ops that have no handler or that silently produce wrong results because the
   BB engine is not in scope during SM dispatch.

Both sub-buckets share the same architectural root: **proc bodies were lowered in
CH-17b' with `SM_PUSH_EXPR + SM_BB_PUMP` for all generator kinds, and that shape
is incompatible with SM dispatch.**

---

## Prolog results: 4 PASS, 1 diverges under `--interp`

`coverage_net_gaps.pl` — FAIL.  
Cause: `:- initialization(main, main)` directive emits `SM_CALL_FN s="initialization" nargs=2`.
The `initialization/2` predicate is not in `icn_try_call_builtin_by_name` (Icon-oriented bridge)
and is not in the SNOBOL4 builtin registry. Falls through to `APPLY_fn` → Error 5.

**Bucket: missing builtin** — `initialization/2` (Prolog directive, not an Icon builtin).
Fix: add a `SM_CALL_FN` dispatch arm for Prolog directive builtins in `sm_interp.c`,
or handle `:- initialization` at the lowering layer (emit `SM_CALL_FN "main" nargs=0`
directly instead of delegating to the `initialization/2` predicate).

---

## Prioritised sub-rung list

### Icon — one coupled rung (not per-program)

All 111 failures share a single root cause: generator kinds inside proc bodies emit
`SM_PUSH_EXPR + SM_BB_PUMP` which is incompatible with SM dispatch. The fix is not
per-kind (there are 9 generator kinds: E_EVERY, E_SUSPEND, E_BANG_BINARY, E_LCONCAT,
E_LIMIT, E_RANDOM, E_SECTION, E_SECTION_PLUS, E_SECTION_MINUS) but architectural:

**CH-17i-mode3-completeness-icon-generators** — Migrate each generator kind inside
proc-body chunks to emit a proper SM opcode (mirroring CH-17f's `SM_BB_ONCE_PROC`
pattern). Each kind needs: (a) a chunk-side producer in `sm_lower.c` that emits a
named-proc opcode instead of `SM_PUSH_EXPR + SM_BB_PUMP`; (b) a consumer handler in
`sm_interp.c`. This is CH-17h's work (which was surveyed as dead-code-on-real-corpora
under `--ir-run` because the generator kinds inside proc bodies were never reachable
via `coro_call` in `--ir-run` mode — but they ARE reachable under `--interp` once
`sm_call_proc` dispatches into the chunk). Sub-rungs per kind:

| Sub-rung | Kind(s) | Programs unblocked |
|---|---|---|
| CH-17i-every-suspend | E_EVERY, E_SUSPEND | rung01, rung02, rung03, rung13_alt*, rung14_limit*, rung16, rung32_strretval*, rung34, rung35_block* |
| CH-17i-bang-concat | E_BANG_BINARY, E_LCONCAT | rung11_bang*, rung15_lconcat |
| CH-17i-section | E_SECTION, E_SECTION_PLUS, E_SECTION_MINUS | rung20_section* |
| CH-17i-limit-random | E_LIMIT, E_RANDOM | rung14_limit* (overlap with every), rung30 |

Additionally, scan-context builtins (`tab`, `move`, `find`, `match`, `upto`, `any`,
`many`) need bridge-5 treatment (separate rung; these need `&pos`/`&subject` context).

### Prolog — one small fix

**CH-17i-mode3-prolog-initialization** — Handle `:- initialization(Goal, main)`
in `sm_lower.c`: instead of emitting `SM_CALL_FN "initialization" nargs=2`, lower
the inner goal directly (emit `SM_CALL_FN goal_name nargs=0`). Unblocks
`coverage_net_gaps.pl`. One-file change in `sm_lower.c`.

---

## Gates

Survey-only rung — no source touched. This doc is the artifact.  
Gates confirmed byte-identical at survey time: smoke ×6 PASS, isolation PASS,
unified_broker PASS=49.
