# CH-17g-irrun-execution — empirical survey of Step 2 attempt

**Session: 2026-05-09.  one4all HEAD `0c72978d` (CH-17g-final-SURVEY-2).**

## Summary

Attempted CH-17g-irrun-execution Step 2 (route `--ir-run` non-SNO
through `sm_preamble` + `sm_run_with_recovery`, mirroring `--interp`).
The dispatch arm flip was a one-line edit; build clean.  Probe.icn
remained byte-identical.  But three gates regressed against the
pre-rung baseline:

| Gate                | Pre-rung baseline | Post-Step-2 | Verdict |
|---------------------|-------------------|-------------|---------|
| smoke SNOBOL4       | PASS=7 FAIL=0     | PASS=7 FAIL=0 | unchanged |
| smoke Icon          | PASS=5 FAIL=0     | **PASS=3 FAIL=2** | **REGRESSION** |
| smoke Prolog        | PASS=5 FAIL=0     | PASS=5 FAIL=0 | unchanged |
| smoke Raku          | PASS=5 FAIL=0     | PASS=5 FAIL=0 | unchanged |
| smoke Snocone       | PASS=5 FAIL=0     | PASS=5 FAIL=0 | unchanged |
| smoke Rebus         | PASS=4 FAIL=0     | PASS=4 FAIL=0 | unchanged |
| isolation           | PASS              | PASS        | unchanged |
| unified_broker      | PASS=49 FAIL=0    | **PASS=22 FAIL=27** | **REGRESSION** |

Reverted per the rung's Rollback Signal section.  Post-revert:
all gates green at baseline.

## What the regressions look like

### Icon smoke — `if_expr` and `every`

`smoke_icon.sh` defines two failing tests; reproduced standalone:

```icon
procedure main()
  x := 10;
  if x > 5 then write("big"); else write("small");
end
```

Pre-rung: prints `big`.  Post-Step-2 `--ir-run`: prints `big\n5`.
The expression `x > 5` returns `5` (Icon's `>` returns the right
operand on success); the AST walker silently discarded that value
in stmt context.  SM dispatch leaks the descriptor onto the value
stack and `write` (or stmt-end flushing) emits it.  This is a
**producer gap or pop-discipline gap** — `lower_stmt`/`lower_expr`
for `if`-condition slots needs an `SM_POP` after the value-test
that `--interp` already needs but currently lacks for the if/while
condition path.

```icon
procedure main()
  every write(1 to 3);
end
```

Pre-rung: prints `1\n2\n3`.  Post-Step-2: `sm_interp: stack underflow;
Aborted`.  `1 to 3` is the E_TO generator that CH-15a migrated to
SM_BB_PUMP_SM, but `every`'s consumer side still expects
SM_PUSH_EXPR + SM_BB_PUMP shape per CH-15-SURVEY (line-1192
dispatcher arm).  Under `--ir-run` the AST walker handled the
generator chain end-to-end; under SM dispatch the producer is
migrated and the consumer is not.

### unified_broker — 49 → 22 PASS

27 unified-broker tests now FAIL.  Not investigated per-test — the
regression class is the same: any unified-broker test that exercises
non-SNO frontends now flows through SM dispatch and hits the gaps
above (or sibling gaps in the same producer/consumer mismatch family).

### Icon `--ir-run` corpus

Not re-run post-Step-2 (the smoke regression is sufficient to halt).
Expectation per the analysis below: the corpus PASS count would
drop similarly because the surface that fails in smoke is hit by
many corpus programs.

## What this evidence means for the rung graph

The rung's Rollback Signal section anticipated this case:

> if any byte-identity check in Step 4 diverges and the cause
> cannot be traced to a known SM-side gap with a clear follow-on
> rung path, revert the Step 2 dispatch change (one-line revert)
> and surface the divergence as a survey doc before re-attempting.

The cause CAN be traced to known SM-side gaps with a clear
follow-on rung path: CH-17i-mode3-completeness, which the carving
in this same session set up to enumerate and close the buckets.
The empirical finding is:

**CH-17g-irrun-execution's gates as currently spec'd cannot be met
at HEAD `0c72978d`.**  The gate list demands smoke ×6 byte-identical
and unified_broker PASS=49, which the SM dispatch path cannot
deliver today for the Icon corpus surface.

Two valid paths forward:

### Option A — Re-sequence: CH-17i-survey-mode3 first

Run CH-17i-survey-mode3 BEFORE attempting CH-17g-irrun-execution
again.  The survey doc enumerates every bucket; CH-17i-mode3-completeness
sub-rungs land each bucket; once the Icon and Prolog `--ir-run`
PASS subsets pass byte-identical under `--interp` (i.e., the gates
CH-17g-irrun-execution wants are achievable), THEN flip the
dispatch arm.  Step 2 of CH-17g-irrun-execution becomes the
*last* step of the umbrella — the act that converts mode 2 from
"AST walker that happens to coincide with SM dispatch on the
PASS subset" to "actually SM dispatch."

This option matches the goal-file framing where modes 2 and 3
converge once both can handle the supported surface.  The work
is the same; the order is "make mode 3 cover the surface, then
flip mode 2."

### Option B — Land Step 2 with relaxed gates; gap list lives in this doc

Mark CH-17g-irrun-execution as DEFERRED with this survey as its
deferral artifact.  CH-17i-survey-mode3 absorbs Step 2 itself
plus the gap audit in one sweep.

### Recommendation

**Option A.**  CH-17g-irrun-execution stays a real rung but is
re-sequenced to land *after* CH-17i-mode3-completeness rather
than before CH-17i-survey-mode3.  This costs nothing — the
flip itself is one line, already designed and tested.  The
sequencing change matches the goal file's amended sub-rung list.

The next executable rung this session should land is
CH-17i-survey-mode3: build the empirical bucket list from the
post-Step-2 evidence captured above, plus the broader corpus
audit, plus the Prolog-side audit (queens.pl FATAL on kind 59
already documented; the Prolog corpus has 5 PASS / 1 FAIL out
of 6 programs under `--ir-run` per this session's pre-rung
capture).

## Pre-rung baseline (for any future re-attempt)

| Item                            | Value |
|---------------------------------|-------|
| one4all HEAD                    | `0c72978d` |
| Icon corpus `--ir-run`          | PASS=177 FAIL=56 XFAIL=30 TOTAL=263 |
| Prolog corpus (`test/prolog/*.pl`) `--ir-run` PASS subset | hello, palindrome, roman, sentences, wordcount (5/6) |
| Prolog corpus `--ir-run` FAIL   | queens.pl (kind 59 / E_CHOICE) |
| `/tmp/probe.icn --ir-run` md5   | `883a26c5abfd0b454cb149c88ca26fe6` |
| `/tmp/probe.icn --interp` md5   | `883a26c5abfd0b454cb149c88ca26fe6` |
| smoke Icon                      | PASS=5 FAIL=0 |
| smoke Prolog                    | PASS=5 FAIL=0 |
| unified_broker                  | PASS=49 FAIL=0 |
| isolation                       | PASS |
| scrip_all_modes                 | PASS=2 FAIL=0 (NET skipped) |

Note: PLAN.md's GOAL-CHUNKS row tail asserts Icon corpus baseline
of 186/47/30; the actual measured baseline in this session is
177/56/30.  Discrepancy of 9 programs that PLAN.md's recorded
state thinks PASS but currently FAIL.  Not a CH-17 blocker, but
a flag for the next session that touches the Icon corpus baseline.

## Concrete bucket starters for CH-17i-survey-mode3

From this session's evidence:

| Bucket | Example | Recipe |
|--------|---------|--------|
| stmt-context value leak | `if x > 5 then ...` printing `5` | add SM_POP after if/while/until condition value |
| generator producer/consumer mismatch | `every write(1 to 3)` stack underflow | finish CH-15b Icon generator migrations OR back-port consumer to expect SM_BB_PUMP_SM |
| unhandled clause kind in `--interp` | queens.pl kind 59 / E_CHOICE | CH-17f reactivation (E_CHOICE/E_CLAUSE chunk bodies) |

This is a starter list, not the full survey.  The full survey runs
the Icon corpus PASS subset and the Prolog corpus PASS subset
under `--interp` and enumerates every divergence.
