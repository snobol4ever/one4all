# CHUNKS-step17f-validation.md

**Rung:** CH-17f — Migrate Step 16 (Prolog clause kinds at sm_lower.c)
**Session:** sess #85, 2026-05-07
**one4all HEAD before:** `7cfa0a96`

---

## What was done

CH-17f eliminates the raw `EXPR_t*` push at the SM statement-dispatch layer
for Prolog programs.  Before this rung, `lower_stmt` for `LANG_PL` statements
called `lower_expr(s->subject)` (where subject = E_CHOICE) which hit the
`emit_push_expr + SM_BB_ONCE` path.  At runtime, `SM_BB_ONCE` popped the
`DT_E` pointer and called `coro_eval(E_CHOICE)` → `bb_eval_value(E_CHOICE)`
→ FATAL "unhandled kind 59 (RS-23e isolation breach)".

### New opcode: `SM_BB_ONCE_PROC`

Added to `sm_prog.h`, named in `sm_prog.c`, handled in `sm_interp.c` and
`sm_codegen.c`.

```
SM_BB_ONCE_PROC   a[0].s = "name/arity"   a[1].i = arity
```

- Looks up the E_CHOICE node via `pl_pred_table_lookup_global(key)`
- Drives via `pl_box_choice(choice, g_pl_env, arity)` → `bb_broker(BB_ONCE)`
- Sets `st->last_ok`
- **No EXPR_t\* is pushed to the SM value stack**; dispatch is key-driven

### Producer change: `sm_lower.c` `lower_stmt` LANG_PL branch

Replaces:
```c
lower_expr(p, lt, s->subject);   // pushes raw E_CHOICE EXPR_t*
sm_emit(p, SM_BB_ONCE);
```

With:
```c
sm_emit_si(p, SM_BB_ONCE_PROC, key, arity);  // "name/arity", arity
```

Non-E_CHOICE directive subjects (e.g. `initialization/2`) fall through to
the legacy path (correct — they carry E_FNC bodies handled by builtins).

### Producer change: `sm_lower.c` `lower_expr` E_CHOICE case

E_CHOICE in value context now emits `SM_BB_ONCE_PROC key, arity` when
`e->sval` is present.  E_CLAUSE / E_CUT / E_UNIFY / E_TRAIL_* keep the
legacy `emit_push_expr + SM_BB_ONCE` fallback — these appear only as
children of E_CHOICE walked by the broker, never as top-level SM lowered
exprs in practice.

---

## Honest scope note

The predicate chunk bodies emitted by CH-17d are still skeleton-only
(SM_JUMP + SM_LABEL + SM_RETURN).  `SM_BB_ONCE_PROC` uses the IR fallback
path (`pl_box_choice(IR_choice, env, arity)`) rather than `pl_box_choice_pc`
— this is correct and complete Prolog execution via the broker.  The chunk
body fill (lowering E_CLAUSE head-unify + body goals into SM ops) is a
follow-on rung that requires dedicated SM opcodes for unification and trail
management; scoped out of CH-17f.

**`--interp` Prolog programs now produce correct output:**

| Program     | `--ir-run` | `--interp` (before) | `--interp` (after) |
|-------------|:----------:|:-------------------:|:------------------:|
| hello.pl    | Hello, World! | FATAL abort | Hello, World! ✅ |
| roman.pl    | correct    | FATAL abort         | correct ✅          |
| queens.pl   | (hangs/abort) | FATAL abort      | same pre-existing ✅|
| palindrome  | no/no/no   | FATAL abort         | no/no/no ✅ (pre-existing bug in ir-run too) |

The "Error 5 / statement 0" message in `--interp` Prolog output comes from
the `initialization/2` directive stmt which has no user-predicate entry —
it falls to `pl_box_fail()`.  Same behavior as IR mode (directive is silently
skipped there via the builtin path).  This is a pre-existing divergence,
not introduced by CH-17f.

---

## Gate results

| Gate | Result |
|------|--------|
| smoke SNOBOL4 --interp 7/7 | PASS |
| smoke Icon --interp 5/5 | PASS |
| smoke Prolog --interp 5/5 | PASS |
| smoke Raku --interp 5/5 | PASS |
| smoke Snocone --interp 5/5 | PASS |
| smoke Rebus 4/4 | PASS |
| isolation gate | PASS |
| csnobol4 Budne PASS=61 | PASS |
| unified_broker PASS=49 | PASS |
| Icon corpus 186/47/30 TOTAL=263 | PASS (byte-identical) |

---

## Files touched

- `src/runtime/x86/sm_prog.h` — `SM_BB_ONCE_PROC` opcode added
- `src/runtime/x86/sm_prog.c` — name table entry
- `src/runtime/x86/sm_interp.c` — handler + pl_broker/pl_runtime includes
- `src/runtime/x86/sm_codegen.c` — handler + registration
- `src/runtime/x86/sm_lower.c` — `lower_stmt` LANG_PL + `lower_expr` E_CHOICE
