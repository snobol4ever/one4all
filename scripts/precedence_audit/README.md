# scripts/precedence_audit/

SCT-9g empirical probes that ground `.github/PRECEDENCE-AUDIT.md`.

Each probe is a standalone SNOBOL4 program that exercises a single
precedence/associativity question. Run under live SPITBOL to confirm
the manual's claim against actual evaluation.

## Usage

```bash
/home/claude/x64/bin/sbl -b probe_assoc.sno
/home/claude/x64/bin/sbl -b mixed_op_truth.sno
```

## Files

- `probe_assoc.sno` — Probes every operator priority in SPITBOL Ch.15
  table, using non-commutative operations (`10-3-2`, `100/10/2`,
  `2^3^2`, `A=B=7`) so left vs right associativity produces different
  observable values. **Result: every probe matches Ch.15 exactly.**

- `mixed_op_truth.sno` — Probes mixed-tag arithmetic chains
  (`20-5+2`, `100-30+10-5`). These are the smoking-gun expressions
  where `.sc`'s right-recursive grammar produces a different tree
  shape (and ultimately a different evaluated value) than the C
  frontend's left-recursive grammar. **Result: SPITBOL evaluates
  `20-5+2`=17 (left-assoc) — matching C, not `.sc`.**

## When to extend

Add a new probe whenever the audit needs to verify a new
operator-combination ground truth that the manual leaves ambiguous
or where two grammars disagree. Keep each probe a single concern.
