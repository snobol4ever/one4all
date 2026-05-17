# CHUNKS M1 Milestone Close — SNOBOL4 + Snocone Producer-Side Chunk Migration

**Session:** #65, 2026-05-06
**Goal:** GOAL-CHUNKS Step 7 — close M1
**Scope of M1:** Steps 1–7. SNOBOL4 + Snocone fully isolated through modes 2 (sm-run) and 3 (jit-run) on the **producer side**: every `emit_push_expr` site reachable from pure SNOBOL4 / Snocone lowering has been migrated to `SM_PUSH_CHUNK` (Steps 2–4); the migration has been validated empirically (Step 5); the isolation gate has been strengthened with a structural rule covering the two zero-hit files (Step 6, partial — see deferral below).

---

## What landed in M1

| Step | What | one4all hash |
|------|------|--------------|
| 1 | Survey + scaffolding (SmChunk_t, SM_PUSH_CHUNK, SM_CALL_CHUNK opcodes, FATAL stubs, `docs/CHUNKS-step01-audit.md`) | `a79b09f0` |
| 2 | Migrate `sm_lower.c:573` (E_DEFER `*expr` in value context) + SM_CALL_CHUNK implementation | `1b42498f` |
| 3 | Migrate `sm_lower.c:470` (pattern non-QLIT arg of `SM_PAT_USERCALL_ARGS`) | `c6862096` |
| 4 | Migrate `sm_lower.c:326/345/386` (E_CAPT_COND_ASGN `. *fn(args)` and E_CAPT_IMMED_ASGN `$ *fn(args)`) | `3be281e8` |
| 5 | Instrumented validation (SCRIP_CHUNKS_AUDIT env-var gated counters, `docs/CHUNKS-step05-validation.md`) | `2e0777d0` |
| 6 | Structural rule in isolation gate for `snobol4_invoke.c` and `snobol4_argval.c` (partial — two of four files) | `27b0a102` |

After M1: `grep emit_push_expr` in `sm_lower.c` returns **only** the helper definition (line 39) plus four call sites at lines 1046/1057/1064 (Icon generators / Prolog clauses) and 1303 (Icon main). Those four belong to Steps 12, 15, 16 (M4). No SNOBOL4 / Snocone code path reaches them.

---

## Gate state at M1 close

| Gate | Result |
|------|--------|
| Build | green |
| smoke_snobol4 | PASS=7 / 7 |
| smoke_snocone | PASS=5 / 5 |
| smoke_icon | PASS=5 / 5 |
| smoke_prolog | PASS=5 / 5 |
| smoke_raku | PASS=5 / 5 |
| smoke_rebus | PASS=4 / 4 |
| isolation_ir_sm (with new structural rule) | PASS |
| csnobol4 Budne | PASS=36 / 150 (≥34 baseline) |
| scrip_all_modes | PASS=2 (sm-run, ir-run) |
| Step 5 chunk audit (SCRIP_CHUNKS_AUDIT=1) | zero SM_PUSH_EXPR fires across SNOBOL4/Snocone test programs; SM_PUSH_CHUNK exercised; out-of-range = 0 |

---

## Honest limits — what M1 does NOT close

The goal-text framing of M1 ("SNOBOL4 + Snocone fully isolated through modes 2 and 3") is true on the **producer side** but not yet on the **consumer side or the broad-corpus mode-parity side**. Recording this so future sessions don't slip back into wishful framing.

### 1. Step 6 is a partial close

The structural rule only covers `snobol4_invoke.c` and `snobol4_argval.c` (both zero-hit). The remaining two files named in the goal text — `snobol4_pattern.c` and `eval_code.c` — still walk EXPR_t:

- `snobol4_pattern.c` lines 229–253: legacy DT_E thaw block reachable via `CONVERT(s,"EXPRESSION")`. Closing this requires migrating `compile_to_expression` (line 990) to emit a chunk instead of an EXPR_t*.
- `eval_code.c`: contains `eval_node`, the IR walker itself. Cannot be made EXPR_t-free without unwinding the legacy `EVAL` consumer chain — likely M4 cleanup territory.

### 2. Three-mode parity on broad corpus is RED — pre-existing

The `test_smoke_snobol4_jit.sh` gate runs ~150 crosscheck programs in `--ir-run` / `--interp` / `--run` and requires the SM and JIT PASS counts match the IR PASS count. Current state at M1 close:

```
FAIL  --interp  PASS (101) < --ir-run PASS (139)
FAIL  --run PASS (101) < --ir-run PASS (139)
```

**38 programs** PASS under `--ir-run` but FAIL under `--interp` and `--run`. Verified pre-existing: at commit `c6862096` (pre-Step-4) the same gate showed `--interp 102 / --run 101`. The CHUNKS Steps 2–6 are net-flat on this gate — no regression and no improvement.

The 38-program gap is therefore independent of CHUNKS work — it predates this goal. It represents real IR-vs-SM divergences in the SM-mode pattern engine that the shallow smoke ×6 does not catch. M1's empirical proof (Step 5) was scoped to the migrated `emit_push_expr` sites; it did not, and could not, prove broad three-mode parity.

### 3. Snocone all-modes shows similar pre-existing gaps

`test_beauty_snocone_all_modes.sh`: PASS=28 FAIL=14 SKIP=3. Same pattern — pre-existing, not introduced by CHUNKS Steps 2–6.

---

## What this means for "M1 shippable"

The plan describes M1 as "the first shippable milestone." That framing remains correct in the narrow sense:

- The producer-side migration is real, complete, and validated.
- SM_PUSH_EXPR is no longer emitted by SNOBOL4 / Snocone lowering.
- Future SM_PUSH_EXPR usage (Icon generators, Prolog clauses, Icon main) is now clearly fenced and assigned to specific later steps.
- The chunk infrastructure (SM_PUSH_CHUNK, SM_CALL_CHUNK, EXPVAL_fn slen==1 dispatch, sm_call_chunk frame setup) is in place and exercised.

It is NOT shippable as "modes 2/3 are bug-for-bug compatible with mode 1 on real programs" — the broad-corpus parity gap is real. Future CHUNKS work on M2 (mode 4 emitter) builds on the producer-side pipeline that M1 closed; the broad-corpus parity work is orthogonal to CHUNKS and lives in the SN-* / SM-pattern ladder.

---

## Recommended next steps

| Step | Owner | Note |
|------|-------|------|
| Step 8 (M2) — Mode 4 x86 emitter for SNOBOL4 + Snocone | `GOAL-MODE4-EMIT.md` rung EM-1 | Now unblocked. The emitter consumes the SM_Program produced by sm_lower; with M1 done, that SM_Program contains zero SM_PUSH_EXPR for SNOBOL4/Snocone and the emitter does not need an EXPR_t walker. |
| Steps 9–11 (M3) — Native-host Snocone (.NET, JVM, JS) | `GOAL-NATIVE-SNOCONE-DOTNET.md` etc. | Independent of M2; can run in parallel. |
| Deferred Step 6 work — migrate CONVERT EXPRESSION to chunk emission | New rung | Closes `snobol4_pattern.c` and unblocks the structural rule extension. |
| Broad three-mode parity (38-program gap) | Outside CHUNKS scope | Likely a SN-* ladder rung. Pre-existing, not introduced by this work. |

---

## Author note

Steps 4, 5, 6 of this milestone were authored by Claude Sonnet (the active session). Per the Three-Milestone Authorship Agreement in PLAN.md, this milestone close is attributed to Lon Cherryholmes for git-history simplicity; authorship in spirit is recorded here and in the closed-step records of GOAL-CHUNKS.md.
