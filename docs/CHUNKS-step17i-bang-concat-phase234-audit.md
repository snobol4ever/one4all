# CHUNKS-step17i-bang-concat — Phases 2/3/4 audit & deferral

**Rung:** CH-17i-bang-concat Phases 2 / 3 / 4 (follow-on to Phase 1)
**Goal-step:** GOAL-CHUNKS-STEP17.md → CH-17i → CH-17i-bang-concat
**Pre-audit commit:** `a8a064a0` (CH-17i-bang-concat Phase 1 landed)
**Date:** 2026-05-10

---

## TL;DR

After Phase 1 (AST_LCONCAT scalar value-path) landed, the remaining
phases of CH-17i-bang-concat have **no empirical anchor in any of the
four corpora SCRIP exercises today**.  An audit-counter sweep across
706 programs (271 Icon + 186 Raku + 114 Snocone + 135 Prolog) under
`--interp` with `SCRIP_EXPRS_AUDIT=1` reports **zero programs fire
SM_PUSH_EXPR**.

Per the documented precedent set by CH-15-SURVEY (which deferred CH-15b
on identical reasoning), Phases 2 / 3 / 4 are deferred until a corpus
program forces the issue.  This doc IS the deferral artifact — future
sessions should consult it before re-running the same audit.

**Important — deferred ≠ optional.**  The kinds named in this doc
(AST_BANG_BINARY, AST_LCONCAT-generative, AST_LIMIT, AST_RANDOM,
AST_SECTION*) are **required for Mode 4** (`--compile`) on
Icon and Prolog.  An emitted standalone binary cannot fall back to
`coro_eval` IR-walking — every kind a program uses must be present
as compiled SM in the artifact.  The reason the audit currently
shows zero fires is precisely the architectural workaround that
Mode 4 cannot use: proc bodies execute via `coro_pump_proc_by_name`
→ `coro_eval(body)`, an in-host-process tree walk.  When
**CH-17g-irrun-execution** lands and routes proc bodies through SM
dispatch, every Icon program with a generator inside `main()` will
fire SM_PUSH_EXPR for the un-migrated kinds — and those phases
become unblocked with concrete anchors.

The deferral is **sequencing**, not omission: each phase has a
required slot in the M4.5 ladder before Mode 4 for Icon/Prolog ships.
See §"Architectural sequencing" below.

---

## What the open phases would migrate

After Phase 1, `sm_lower.c`'s `lower_expr` legacy fallthrough block
(line 1410, post-Phase-1 numbering) still contains:

```c
case AST_BANG_BINARY:
case AST_LIMIT:
case AST_RANDOM:
case AST_SECTION:
case AST_SECTION_MINUS:
case AST_SECTION_PLUS:
    emit_push_expr(p, e);
    sm_emit(p, SM_BB_PUMP);
    return;
```

Plus AST_LCONCAT's `is_suspendable(child)` branch (line 1402–1405)
which still falls through to the same legacy shape.

The CH-17i-bang-concat rung's stated phase plan:

| Phase | Kind | Path | Status |
|-------|------|------|--------|
| 1 | AST_LCONCAT | scalar value | ✅ LANDED `a8a064a0` |
| 2 | AST_LCONCAT | generative   | DEFERRED — no corpus anchor |
| 3 | AST_BANG_BINARY | scalar value | DEFERRED — no corpus anchor |
| 4 | AST_BANG_BINARY | generative   | DEFERRED — no corpus anchor |

Sister rungs in the survey list (`CH-17i-section`, `CH-17i-limit-random`)
cover the remaining kinds in the legacy fallthrough; the same audit
result applies to them.

---

## Empirical sweep

```
$ for f in <corpus>/*; do
    SCRIP_EXPRS_AUDIT=1 timeout 5 ./scrip --interp "$f" < /dev/null 2>/dev/null \
      | grep "CHUNKS-AUDIT" \
      | grep -E "SM_PUSH_EXPR=[1-9]"
  done
```

| Corpus  | Programs | SM_PUSH_EXPR fires |
|---------|----------|--------------------|
| Icon    | 271      | 0                  |
| Raku    | 186      | 0                  |
| Snocone | 114      | 0                  |
| Prolog  | 135      | 0                  |
| **Total** | **706**  | **0**              |

All four legacy-fallthrough kinds covered by CH-17i-bang-concat (and
its sister rungs CH-17i-section / CH-17i-limit-random) are unreachable
under `--interp` on the entire current corpus.

The corpus does contain programs whose Icon source uses these
operators — `!` (bang), `[i:j]` (sections), `?n` (random), `\E`
(limit) — but they reach evaluation through the IR tree-walk inside
`coro_eval`, not through the SM lowering's fallthrough.  Specifically:
proc bodies execute via `SM_BB_PUMP_PROC "main"` →
`coro_pump_proc_by_name` → `coro_eval(main_body)`, an AST_t* walk of
the proc body.  Generators inside the body are reached via
`bb_eval_value` / `coro_eval` recursion, never through `sm_lower`'s
per-kind dispatch on the listed kinds.

This is the same architectural gap that motivated Step 17 itself:
proc bodies are not lowered, they're walked.  CH-17g-irrun-execution
(unstarted as of this writing, `- [ ]` in GOAL-CHUNKS-STEP17.md) is
the ladder rung that would route proc-body execution through SM
dispatch.  Until it lands, the kinds in this audit will continue to
have zero empirical reach under `--interp` regardless of any lowering
rung that targets them.

---

## Targeted probe — Icon `!` and `|||` use sites

To rule out the possibility that bang/lconcat use sites simply aren't
in the broad sweep, the rung11_bang_augconcat_* programs (which the
CH-17i-survey-mode3 doc flagged as bang-related) were re-probed under
`SCRIP_EXPRS_AUDIT=1`:

| Program | Mode | rc | Output | Audit |
|---------|------|----|--------|----|
| rung11_bang_augconcat_augconcat        | --ir | 0   | `hello world` | n/a |
| ″                                      | --sm | 139 | (segfault)    | (no summary printed) |
| rung11_bang_augconcat_augconcat_chain  | --ir | 0   | `foo-bar`     | n/a |
| ″                                      | --sm | (rc=0) | err msg      | `SM_PUSH_EXPR=0` |
| rung11_bang_augconcat_augconcat_loop   | --ir | 0   | `hello`       | n/a |
| ″                                      | --sm | (rc=0) | err msg      | `SM_PUSH_EXPR=0` |
| rung11_bang_augconcat_bang_concat      | --ir | 0   | `xy`          | n/a |
| ″                                      | --sm | 0   | (empty)       | `SM_PUSH_EXPR=0` |
| rung11_bang_augconcat_bang_str         | --ir | 0   | `a\nb\nc`     | n/a |
| ″                                      | --sm | 0   | `a\nb\nc`     | `SM_PUSH_EXPR=0` |
| rung15_real_swap_lconcat               | --ir | 0   | `hello world` | n/a |
| ″                                      | --sm | 0   | `hello world` | `SM_PUSH_EXPR=0` (Phase 1) |

Findings:

1. **`bang_str` already PASSes all three modes** (`every write(!s)`).
   The bang here is reached from inside the `every` body's
   `bb_eval_value` recursion — `coro_bb_bang_binary` (or the
   single-child generator path) handles it directly.  No SM lowering
   touches AST_BANG_BINARY for this program.

2. **`bang_concat` produces wrong output under `--interp`** (empty
   string instead of `xy`).  But the audit reports zero
   `SM_PUSH_EXPR` fires — the bug is not in AST_BANG_BINARY's
   lowering.  The program uses `every result ||:= !s`, and the
   broken accumulation is caused by the `||:=` (augmented-concat)
   handling inside the `coro_eval(every)` path under SM dispatch,
   not by the bang.  Out of scope for CH-17i-bang-concat per the
   Phase-1-validation deferral note.

3. **The 4 augconcat-only programs** (no bang at all — pure `||:=`)
   either segfault or print `Error 5: undefined function` under
   `--interp`.  Same root cause as (2) — augmented-assign lowering.
   Separate rung territory.

4. **`rung15_real_swap_lconcat` is now clean** post-Phase-1, as
   documented in `CHUNKS-step17i-bang-concat-phase1-validation.md`.

The conclusion holds: no current corpus program would be flipped
FAIL→PASS by Phases 2 / 3 / 4 lowering changes.

---

## Why deferral is the right call

The closed CH-15-SURVEY doc (`docs/CHUNKS-step15-survey.md`,
session #74) established the precedent:

> The remaining-kinds dispatcher arm at sm_lower.c:1192–1204 is
> dead code on real corpora today. … Implication: CH-15b is
> infrastructure-prep, not a behaviour change, and the standard
> "real program + diff against oracle" validation pattern does not
> apply.  Recommendation: defer CH-15b until Step 17 is complete,
> then migrate remaining kinds with full corpus validation.

CH-17i-bang-concat Phases 2 / 3 / 4 sit in exactly the same
architectural position.  Building the unified `SM_BB_PUMP_AST`
opcode + `g_ast_pump_table` infrastructure today (the design
documented in goal note (3)) would land code that no test exercises
— the same anti-pattern CH-15-SURVEY explicitly called out.

A second copy of the same architectural unblock (CH-17g-irrun-execution
for proc bodies) gates these phases just as Step 17 gated CH-15b.
Until proc bodies execute via SM dispatch, the kinds enumerated above
are **structurally unreachable** from any SM lowering site, regardless
of how that lowering is shaped.

---

## Architectural sequencing — why this matters for Mode 4

The reachability finding above ("zero SM_PUSH_EXPR fires across 706
programs") is true today but it is **not a statement about whether
the kinds are needed** — it is a statement about whether the *current*
runtime exercises them.  Today's runtime executes Icon/Prolog proc
bodies via:

```
SM_BB_PUMP_PROC "main"
  → coro_pump_proc_by_name("main", ...)
    → coro_eval(main_body)            ← AST_t* tree walk in host process
      → bb_eval_value(child)          ← recursive AST_t* walk
        → coro_bb_bang_binary(...)    ← per-kind generator drivers
        → coro_bb_to_by(...)
        → ...
```

This walker lives in `libscrip_rt.so` / the in-process interpreter.
It is reachable from `--ir-run` and `--interp` and `--run`
(Mode 1, Mode 2, Mode 3) because all three execute scrip in-process
and link the runtime in.

**Mode 4 cannot link this walker in.**  An emitted asm/x86 binary
(`./prog`) is a standalone executable that calls into
`libscrip_rt.so` for **language-level builtins** (string concat,
table lookup, NV resolution, pattern matcher, etc.) — but it does
not embed an AST_t walker.  GOAL-CHUNKS.md is explicit:

> Mode 4 emission becomes possible because the emitted asm
> executable contains no EXPR_t walker — it contains compiled
> chunks plus calls into a runtime support library
> (`libscrip_rt.so` or the JVM/.NET/JS host equivalent) that
> implements language-level builtins (pattern matcher, NV table,
> iterator advance, cset membership, etc.).

Therefore, every Icon/Prolog program that mode 4 must run end-to-end
— the 177-PASS `--ir-run` Icon set + the 4-PASS Prolog smoke set —
must compile fully to SM.  Every kind those programs use, in every
context (scalar value, generative, statement-level), must lower to
SM ops the emitted backend can codegen.

The legacy fallthrough block at `sm_lower.c:1410-1418` cannot
contain any kinds by the time **CH-17i-mode4-icon-prolog** ships.

### The sequence

The goal file already names the order; this audit doc surfaces it
explicitly so future sessions don't read the deferral as
"indefinite":

```
1. CH-17g-irrun-execution           (proc bodies → SM dispatch)
       ↓ kinds in legacy fallthrough start firing
       ↓ audit becomes positive evidence
2. CH-17i-bang-concat Phases 2,3,4  (this doc's deferred phases)
   CH-17i-section
   CH-17i-limit-random
   CH-17i-prolog-initialization
       ↓ legacy fallthrough block empty for Icon+Prolog
3. CH-17i-mode3-completeness        (Icon/Prolog --interp == --ir-run PASS)
       ↓
4. CH-17i-mode4-icon-prolog         (--compile covers Icon+Prolog)
       ↓
5. CH-17i-final-isolation           (locks the property in CI)
```

Each step is a prerequisite for the next.  The "deferred" status of
Phases 2 / 3 / 4 means "blocked on step 1", not "optional".  Once
CH-17g-irrun-execution lands, the deferred phases become the
critical path for Mode 4 on Icon/Prolog.

### Why not just do them now?

Two reasons, both empirical:

1. **No test would exercise the new code.**  The unified
   `SM_BB_PUMP_AST` opcode + `g_ast_pump_table` infrastructure
   would land with zero coverage.  CH-15-SURVEY explicitly named
   this as the wrong move ("infrastructure-prep, not a behaviour
   change … the standard 'real program + diff against oracle'
   validation pattern does not apply").  The fix history of
   `coro_bb_*` generator drivers shows they were tuned against
   real failing programs; building their SM analogues blind is
   how subtle semantic divergences slip through.

2. **The right shape may differ post-CH-17g.**  CH-17g-irrun-execution's
   choices about how proc bodies thread through SM dispatch
   (e.g., how `FRAME` state is preserved, how nested generator
   contexts surface in the value stack) will inform what the
   per-kind lowering looks like.  Building the per-kind lowering
   first risks designing against the wrong contract and rewriting
   later.

The deferral pays for itself when CH-17g lands: the per-kind
lowering can be written against real failing programs (each "first
non-zero `FIRES:` count" becomes a concrete anchor), with
oracle-diff validation, in the shape CH-17g's runtime contract
actually demands.

---

## When to revisit

Revisit Phases 2 / 3 / 4 (and the sister rungs CH-17i-section,
CH-17i-limit-random) when **any** of these conditions hold:

1. **CH-17g-irrun-execution lands** — proc bodies route through SM
   dispatch.  At that point every Icon program with a generator
   inside `main()` will fire SM_PUSH_EXPR for the un-migrated
   kinds.  Re-run the audit; the FIRES count will be non-zero.
   Migrate the kinds the audit flags.

2. **A new corpus program is added** that exercises one of the
   listed kinds in a way that reaches the SM lowering directly
   (e.g., a top-level statement-list program that uses `!` or
   `|||` or `[i:j]` outside any proc).  The audit will flag it;
   land the relevant phase with that program as anchor.

3. **A non-Icon frontend** (Raku, Snocone, Rebus) starts emitting
   AST_BANG_BINARY / AST_LCONCAT / AST_LIMIT / AST_RANDOM /
   AST_SECTION* in lowering positions that aren't proc-body-walked
   (e.g., a Raku slice `@arr[1..3]` that lowers to AST_SECTION at
   the SM-dispatch top level).  The audit catches this for free.

In all three cases the audit script is the trigger: a `FIRES > 0`
result is the empirical anchor that unblocks the corresponding phase.

---

## Audit script (canonical form)

For future sessions, the canonical audit invocation is:

```bash
cd /home/claude/one4all
for f in /home/claude/corpus/programs/icon/*.icn \
         /home/claude/corpus/programs/raku/**/*.raku \
         /home/claude/corpus/programs/snocone/**/*.sc \
         /home/claude/corpus/programs/prolog/**/*.pl; do
  [ -f "$f" ] || continue
  audit=$(SCRIP_EXPRS_AUDIT=1 timeout 5 ./scrip --interp "$f" \
            < /dev/null 2>/dev/null | grep "CHUNKS-AUDIT")
  echo "$audit" | grep -qE "SM_PUSH_EXPR=[1-9]" \
    && echo "FIRES: $(basename "$f") :: $audit"
done
```

A non-empty `FIRES:` output is the trigger: route the named program
into the matching phase's empirical anchor and migrate the kind it
exercises.  An empty output means the legacy fallthrough is still
unreachable — defer.

---

## Gates

This rung writes documentation only.  No source touched, no opcode
added, no behaviour change.  All standard gates byte-identical to
the Phase-1-landed baseline:

| Gate | Value |
|------|-------|
| `smoke_snobol4` | 7/7 |
| `smoke_icon` | 5/5 |
| `smoke_prolog` | 5/5 |
| `smoke_raku` | 5/5 |
| `smoke_snocone` | 5/5 |
| `smoke_rebus` | 4/4 |
| `test_isolation_ir_sm.sh` | PASS |
| `test_smoke_unified_broker.sh` | 49/0 |
| `test_smoke_scrip_all_modes.sh` | 2/0 |
| `test_icon_ir_all_rungs.sh` | 177/56/30 (TOTAL=263) |

Audit sweep performed at `scrip` HEAD = `a8a064a0`.

---

## Cross-references

- `docs/CHUNKS-step15-survey.md` — the precedent for "infrastructure
  with no anchor → defer".
- `docs/CHUNKS-step16-survey.md` — the same pattern applied to
  Prolog; Step 16 deferred until Step 17 lands.
- `docs/CHUNKS-step17i-bang-concat-phase1-validation.md` — Phase 1
  close.  The "Generative case empirics" section reports the same
  zero-fires result for Phase 2 specifically; this doc generalises
  the finding to Phases 3 / 4 and the sister rungs.
- `docs/CHUNKS-step17i-survey-mode3.md` — the Icon+Prolog gap audit
  that listed CH-17i-bang-concat as a sub-rung.  This doc updates
  that listing with empirical findings.

---

## Goal-file update

`GOAL-CHUNKS-STEP17.md` updated in the same commit:

- Phase 1's note marked LANDED (already was).
- Phase 2 deferral cross-references this audit doc.
- Phases 3 / 4 deferral added with same cross-reference.
- Sub-rung order in note (6) annotated: CH-17i-section and
  CH-17i-limit-random share the same deferral basis.
- PLAN.md Step entry updated to reflect that the rung's open
  phases are deferred-pending-CH-17g-irrun-execution rather than
  open-and-actionable.
