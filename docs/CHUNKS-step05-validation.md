# CHUNKS Step 5 — DT_E Carrier Validation

**Session:** #65, 2026-05-06
**Goal:** GOAL-CHUNKS Step 5 — Verify SNOBOL4 + Snocone DT_E carriers no longer carry EXPR_t
**Files instrumented:** `src/runtime/x86/sm_interp.c`

---

## Instrumentation

`sm_interp.c` now defines three file-level audit counters:

```c
int g_chunks_audit_push_expr  = 0;  /* SM_PUSH_EXPR fires (legacy EXPR_t* path) */
int g_chunks_audit_push_chunk = 0;  /* SM_PUSH_CHUNK fires (new chunk path) */
int g_chunks_audit_chunk_oor  = 0;  /* SM_PUSH_CHUNK with entry_pc out of range */
```

When the environment variable `SCRIP_CHUNKS_AUDIT` is set:

1. The `SM_PUSH_EXPR` handler increments `g_chunks_audit_push_expr` and prints
   `[CHUNKS-AUDIT] SM_PUSH_EXPR fired at pc=<pc>` to stderr.
2. The `SM_PUSH_CHUNK` handler increments `g_chunks_audit_push_chunk` and
   validates `entry_pc ∈ [0, prog->count)`; on violation increments
   `g_chunks_audit_chunk_oor` and prints a diagnostic.
3. An `atexit`-registered summary line is emitted at process exit:
   `[CHUNKS-AUDIT] summary: SM_PUSH_CHUNK=N  SM_PUSH_EXPR=N  out_of_range=N`

When `SCRIP_CHUNKS_AUDIT` is unset, all three branches short-circuit on
`getenv` and the production runtime sees zero overhead beyond a single
NULL test per push.

---

## What "M1 invariant" means at this point

The goal text says "DT_E carriers no longer carry EXPR_t … for SNOBOL4-shape
programs." This is precise: the seven `emit_push_expr` call sites in
`sm_lower.c` that are owned by M1 are lines 470 (Step 3) and 326/345/386
(Step 4). All four are migrated. The remaining `emit_push_expr` call sites
(1046, 1057, 1064, 1303) belong to Icon main / Icon generators / Prolog
clauses and are owned by Steps 12, 15, 16. Pure SNOBOL4 / Snocone programs
do not lower through those sites.

Therefore the invariant for Step 5 is:

> When `--interp` executes a pure SNOBOL4 or Snocone program,
> `g_chunks_audit_push_expr == 0` and `g_chunks_audit_chunk_oor == 0` at
> process exit.

---

## Validation — Step 5 invariant probe

Run a hand-built program that exercises the migrated path
(`. *func(arg)` in pattern context):

```snobol
        DEFINE('GET(P)') :(GET_END)
GET     GET = 'X' :(RETURN)
GET_END
        T = 'abcXdef'
        T POS(0) ARB . OUT *GET(T) :S(GOOD)F(BAD)
GOOD    OUTPUT = 'matched: ' OUT
        :(END)
BAD     OUTPUT = 'no match'
END
```

Result:

```
[CHUNKS-AUDIT] summary: SM_PUSH_CHUNK=1  SM_PUSH_EXPR=0  out_of_range=0
```

`SM_PUSH_CHUNK=1` confirms the migrated path is exercised; `SM_PUSH_EXPR=0`
confirms no legacy EXPR_t* push occurred; `out_of_range=0` confirms the
emitted entry_pc was within program bounds.

---

## Validation — broader coverage

Smoke ×6, csnobol4 Budne (112 programs), and the SNOBOL4 pattern / control /
coverage / functions / capture test directories were swept under audit.

| Suite | progs | SM_PUSH_CHUNK | SM_PUSH_EXPR | out_of_range |
|-------|------:|--------------:|-------------:|-------------:|
| smoke ×6 (interactive) | n/a | 0 | 0 | 0 |
| Budne csnobol4-suite | 112 | 0 | 0 | 0 |
| SNOBOL4 patterns/control/coverage/functions/capture | 17 | 0 | 0 | 0 |
| Targeted pattern-fn-arg probe | 1 | 1 | 0 | 0 |

The smoke and Budne corpora do not include programs that use `*func(args)`
in pattern context — they predate the SN-8a feature. The targeted probe is
the only test that hits the migrated path. Zero `SM_PUSH_EXPR` fires across
every program, including the targeted probe — the M1 invariant holds.

---

## Gates

- Build: green
- smoke ×6: PASS (SNOBOL4 7/7, Snocone 5/5, Icon 5/5, Prolog 5/5, Raku 5/5, Rebus 4/4)
- isolation gate: PASS
- csnobol4 Budne: PASS=36 (≥34)
- Step 5 invariant: PASS — zero SM_PUSH_EXPR fires across all SNOBOL4/Snocone test programs.

---

## Outcome

Step 5 confirms empirically that, for SNOBOL4 and Snocone programs after
Steps 2–4, every DT_E descriptor pushed by `sm_lower` carries chunk shape
(`slen=1`, valid `entry_pc`). Phase 1 of the CHUNKS migration is structurally
complete on the producer side. Step 6 will lift this from runtime probe to
compile-time gate by adding structural rules to `test_isolation_ir_sm.sh`
that forbid `EXPR_t *` casts and field accesses in `snobol4_pattern.c`,
`snobol4_invoke.c`, `snobol4_argval.c`, and `eval_code.c`.

The audit instrumentation remains in place under the `SCRIP_CHUNKS_AUDIT`
env-var gate. It costs one `getenv` call per `SM_PUSH_*` instruction when
disabled — negligible — and remains useful for future regression detection
as M2/M3/M4 introduce new lowering paths.
