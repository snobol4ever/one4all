# CH-17g-runtime-bridge-lcomp — SM_LCOMP carries operator EKind; sm_interp gains handler

**Rung:** CH-17g-runtime-bridge-lcomp (sibling of bridge-acomp; closes
the asymmetry the bridge-acomp validation doc explicitly flagged as a
follow-on rung).
**Session:** 2026-05-09
**Predecessor:** CH-17g-runtime-bridge-acomp
(`docs/CHUNKS-step17g-runtime-bridge-acomp-validation.md`).

## What this rung does

Mirrors bridge-acomp for the string/lexicographic relational operators.
Same shape bug, same fix pattern, same mechanism — just on the string
side of the comparison family.

Two coupled changes — neither sufficient alone:

1. **Lowering bug at `sm_lower.c:872`.**  All six string comparison
   EKinds (E_LLT, E_LLE, E_LGT, E_LGE, E_LEQ, E_LNE) collapsed onto a
   single `SM_LCOMP` opcode emitted with no argument.  The comparator
   was unrecoverable at runtime.

2. **Missing handler in `sm_interp.c`.**  Opcode 80 was registered in
   `sm_prog.c:185` and emitted by `sm_lower` for every E_LLT/E_LEQ/etc.
   but had no `case SM_LCOMP:` arm in the switch — fell through to the
   `default` FATAL.

## Why this rung exists despite no corpus surface today

The bridge-acomp validation doc noted: *"No corpus test surfaces
SM_LCOMP today (no Icon program in `test/icon` reaches a string relop
under `--interp` after the bridge family); when one does it will
arrive as the next surface."*

Two reasons to land it preventatively rather than reactively:

1. **Symmetry.**  The pair of fixes is one architectural unit.  Leaving
   half the bug in the tree means the next session reading
   `sm_codegen.c`'s "stubbed by design" comment sees an inconsistent
   story (SM_ACOMP refreshed, SM_LCOMP not).
2. **Surface area is mechanical.**  The fix is line-for-line the same
   shape as bridge-acomp; deferring it would mean re-loading context
   later for a rung that takes the same time today.

## Implementation

### Lowering (`src/runtime/x86/sm_lower.c`)

```c
/* before: */
sm_emit(p, SM_LCOMP);
/* after: */
sm_emit_i(p, SM_LCOMP, (int64_t)e->kind);
```

`a[0].i` now carries the EKind (E_LLT=72, E_LLE=73, E_LGT=74, E_LGE=75,
E_LEQ=76, E_LNE=77 from `src/ir/ir.h:166–171`).  Comment at the case
arm extended to record the rung and the runtime semantics.

### Runtime handler (`src/runtime/x86/sm_interp.c`)

Inserted directly after `SM_ACOMP`:

```c
case SM_LCOMP: {
    DESCR_t r = sm_pop(st);
    DESCR_t l = sm_pop(st);
    if (l.v == DT_FAIL || r.v == DT_FAIL) {
        sm_push(st, FAILDESCR); st->last_ok = 0; break;
    }
    const char *ls = VARVAL_fn(l); if (!ls) ls = "";
    const char *rs = VARVAL_fn(r); if (!rs) rs = "";
    int cmp = strcmp(ls, rs);
    int op = (int)ins->a[0].i;
    int ok;
    switch (op) {
        case E_LLT: ok = (cmp <  0); break;
        case E_LLE: ok = (cmp <= 0); break;
        case E_LGT: ok = (cmp >  0); break;
        case E_LGE: ok = (cmp >= 0); break;
        case E_LEQ: ok = (cmp == 0); break;
        case E_LNE: ok = (cmp != 0); break;
        default:    ok = (cmp == 0); break;  /* legacy emit safety net */
    }
    if (ok) { sm_push(st, r);          st->last_ok = 1; }
    else    { sm_push(st, FAILDESCR);  st->last_ok = 0; }
    break;
}
```

**Semantics chosen:** Icon-style string relops — on success return the
RIGHT operand (so e.g. `every write("a" << ("aa"|"ab"))` would yield
`"aa", "ab"`), on failure return FAILDESCR.  Mirrors the `STRREL`
macro in `interp_eval.c:3184–3194` line for line.  `VARVAL_fn` already
externed at `sm_interp.c:63`.  The `default` arm exists only as a
safety net for any pre-bridge-lcomp SM_Program loaded without an
EKind argument; unreachable on freshly lowered code.

### Disassembly (`src/runtime/x86/sm_prog.c`)

No change needed — `SM_LCOMP` was already in the `i=` print case
alongside SM_INCR/SM_DECR/SM_RCOMP/SM_TRIM (predates bridge-acomp).
`--dump-sm` now shows the operator EKind:

```
   N  SM_LCOMP             i=76   ; E_LEQ  (==)
```

### Stale comment (`src/runtime/x86/sm_codegen.c`)

bridge-acomp had narrowed the SM_LCOMP entry and left it as a
follow-on rung.  Comment refreshed: SM_LCOMP entry removed entirely;
the closing note about SM_ACOMP extended to cover SM_LCOMP too.

## Probe verification

```icon
procedure main()
    s := "abc";
    if s == "abc" then write("seq");
    if s << "abd" then write("slt");
    if s >> "abb" then write("sgt");
end
```

```
$ ./scrip --interp /tmp/probe_strrel.icn
seq
slt
sgt
```

All three branches taken correctly.  Trailing-value-after-if-then leak
(observed but not new — same pre-existing artifact bridge-acomp
documented; reproducible without any SM_LCOMP path).

```
$ ./scrip --ir-run /tmp/probe_strrel.icn
seq
slt
sgt
```

Byte-identical modulo the trailing-leak artifact.

## Gates

All gates byte-identical to baseline (CH-17g-runtime-bridge-acomp @ `d0d1ddfb`):

| Gate | Result |
|------|--------|
| Smoke ×6 (snobol4/icon/prolog/raku/rebus/snocone) | PASS=7/5/5/5/4/5, FAIL=0 |
| `test_isolation_ir_sm.sh` | PASS — no IR-only symbol leaks in SM runtime files |
| `test_smoke_unified_broker.sh` | PASS=49 FAIL=0 |
| `test_smoke_scrip_all_modes.sh` | PASS=2 FAIL=0 |
| Icon `--ir-run` corpus (test_icon_all_rungs.sh) | PASS=186 FAIL=47 XFAIL=30 TOTAL=263 |

**New gate:** string relop probe under `--interp` produces correct
output for `==`, `<<`, `>>`.

## Files

- `src/runtime/x86/sm_lower.c` (1 line behavioural — `sm_emit` →
  `sm_emit_i` carrying EKind; comment expanded).
- `src/runtime/x86/sm_interp.c` (+47 lines — SM_LCOMP handler).
- `src/runtime/x86/sm_codegen.c` (comment refresh; no behavioural
  change — JIT path still h_unimpl by design for both ACOMP and LCOMP).
- `docs/CHUNKS-step17g-runtime-bridge-lcomp-validation.md` (this file).

## Architectural note

This rung completes the comparison-family migration.  In the
"chunks-as-deferred-expressions" framing of GOAL-CHUNKS (the goal that
SM chunks supersede `EXPR_t*` as the unit of compiled work), the
bridge-acomp/lcomp pair is a small but pure example: the operator
information that *was* implicit in the `EXPR_t.kind` field at runtime
is now explicit in the SM_Program as `a[0].i`.  Lowering moved the
fact from "in the IR at runtime" to "baked into the opcode stream at
lower-time."  Same denotational meaning, different operational shape —
which is exactly what `EXPR_t* → entry_pc` is doing at the proc-call
layer for CH-17 as a whole.
