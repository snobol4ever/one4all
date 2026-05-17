# CHUNKS-step17i-bang-concat-phase1 — validation

**Rung:** CH-17i-bang-concat Phase 1 (AST_LCONCAT scalar value-path)
**Goal-step:** GOAL-CHUNKS-STEP17.md → CH-17i → CH-17i-bang-concat
**Pre-rung commit:** `fd1c2b6a` (CH-17i-suspend landed)
**Date:** 2026-05-10

---

## What changed

Added a scalar value-path lowering for `AST_LCONCAT` (Icon `|||` operator)
in `sm_lower.c`'s `lower_expr`, mirroring `AST_CAT`'s else-branch.  When
all children are non-generative (per `is_suspendable`), the new case
emits `lower_expr(c_i)` for each child followed by `SM_CONCAT` between
adjacent pairs — producing one string value on the stack.

Previously, `AST_LCONCAT` fell through to the legacy
`emit_push_expr + SM_BB_PUMP` block at sm_lower.c:1371.  That shape is
net-stack-zero: `SM_PUSH_EXPR` pushes a DT_E descriptor and `SM_BB_PUMP`
pops it without pushing a result.  In value context (e.g. `s := "hello"
||| " world"`) the consumer pop (here `AST_ASSIGN`'s `SM_STORE_VAR`)
underflows.  The legacy `pump_print` body-fn was also semantically wrong
for value context — it printed each yielded value rather than capturing
one.

If any child is `is_suspendable` (gen ||| str, gen ||| gen, etc.), the
case still falls through to the legacy `emit_push_expr + SM_BB_PUMP`
path.  That generative case is Phase 2 of CH-17i-bang-concat and remains
deferred — its inventory is empirically zero on the current Icon corpus
(see "Generative case empirics" below).

### Why this shape, not the CH-17i-every shape

CH-17i-every used a `g_<kind>_table` + name-driven opcode whose handler
does `coro_eval(ast) → bb_broker(BB_PUMP)`.  That pattern fits AST_EVERY
because AST_EVERY *is* a Byrd box that pumps a generator and runs a
do-clause body per tick — it has a `bb_node_t` shape.

AST_LCONCAT-scalar has no `bb_node_t` shape.  Its existing semantics
(`interp_eval.c:3827`) are pure value computation:

```c
case AST_LCONCAT: {
    if (e->nchildren < 2) return NULVCL;
    DESCR_t a = interp_eval(e->children[0]);
    DESCR_t b = interp_eval(e->children[1]);
    /* coerce to strings, concat, return */
    ...
}
```

The SM-side equivalent is exactly what AST_CAT already does: lower
children + `SM_CONCAT`.  No broker, no coroutine, no opcode invention.

### Lowering shape

```
   [lower c_0]                ; push c_0 value
   [lower c_1]                ; push c_1 value
   SM_CONCAT                  ; pop c_1, c_0 → push c_0 ++ c_1
   [lower c_2]                ; push c_2 value (if any)
   SM_CONCAT                  ; pop c_2, top → push concat
   ...
```

Net stack delta: +1 (one string value).  Matches `AST_CAT`'s contract
exactly.  Empirically Icon `|||` is parsed as a binary operator
(nchildren == 2 in practice; the loop generalises to N).

---

## Files touched

  - `src/runtime/x86/sm_lower.c` — carve `AST_LCONCAT` out of the
    legacy fallthrough block; add explicit case with scalar/gen split.

No new opcodes.  No runtime helper additions.  No JIT codegen changes
(the JIT mirrors SM ops automatically; SM_CONCAT was already supported).

---

## Empirical anchor

`rung15_real_swap_lconcat.icn`:

```icon
procedure main()
  s := "hello" ||| " world";
  write(s);
end
```

| Mode | Pre-rung | Post-rung |
|------|----------|-----------|
| `--ir-run`  | PASS (output: `hello world`) | PASS unchanged |
| `--interp`  | FAIL (rc=134 SIGABRT, "sm_interp: stack underflow") | **PASS** byte-identical to ir-run |
| `--run` | FAIL (rc=134 SIGABRT, same stack underflow) | **PASS** byte-identical to ir-run |

Audit-counter sweep with `SCRIP_EXPRS_AUDIT=1`:

```
$ SCRIP_EXPRS_AUDIT=1 ./scrip --interp rung15_real_swap_lconcat.icn
[CHUNKS-AUDIT] summary: SM_PUSH_EXPRESSION=0  SM_PUSH_EXPR=0  out_of_range=0
hello world
```

Pre-rung the same audit reported `SM_PUSH_EXPR fired at pc=3 (legacy
AST_t* path)` followed by stack underflow.  Post-rung the legacy
fallthrough is unreachable for this program — the new scalar value path
takes over.

---

## Generative case empirics

The legacy fallthrough remains for `is_suspendable(child) == 1` cases
(Phase 2 territory).  Sweep of Icon corpus (271 programs) under
`--interp` with `SCRIP_EXPRS_AUDIT=1`: zero programs fire `SM_PUSH_EXPR`
post-rung.  This means the AST_LCONCAT generative case (`gen ||| gen`,
`gen ||| str`, etc.) is **not exercised by the current Icon corpus**.

Phase 2 is therefore infrastructure-prep, not behaviour-fix.  Defer
until a corpus program needs it (and document that program as the
empirical anchor when it does).  Same recommendation pattern as
CH-15-SURVEY's deferral of CH-15b.

---

## Gates

| Gate | Pre-rung | Post-rung | Δ |
|------|----------|-----------|---|
| `smoke_snobol4` | 7/7 | 7/7 | byte-identical |
| `smoke_icon` | 5/5 | 5/5 | byte-identical |
| `smoke_prolog` | 5/5 | 5/5 | byte-identical |
| `smoke_raku` | 5/5 | 5/5 | byte-identical |
| `smoke_snocone` | 5/5 | 5/5 | byte-identical |
| `smoke_rebus` | 4/4 | 4/4 | byte-identical |
| `test_isolation_ir_sm.sh` | PASS | PASS | byte-identical |
| `test_smoke_unified_broker.sh` | 49/0 | 49/0 | byte-identical |
| `test_smoke_scrip_all_modes.sh` | 2/0 | 2/0 | byte-identical |
| `test_icon_ir_all_rungs.sh` | 177/56/30 | 177/56/30 | byte-identical |
| Icon corpus `--interp` | 100/163 | 101/162 | **+1 PASS** (rung15_real_swap_lconcat) |
| `--interp` rung01–04 | 20/24 | 20/24 | unchanged (the 4 FAILs are pre-existing: rung02_proc_fact, rung02_proc_locals, rung03_suspend_fail, rung03_suspend_return) |

The +1 in the broad `--interp` Icon corpus is exactly
`rung15_real_swap_lconcat`.  The four other rung15 programs
(rung15_real_swap_real_literal, _real_var, _swap_basic, _swap_str)
were already at their final state pre-rung — _real_literal and _real_var
PASS, _swap_basic and _swap_str FAIL (separate `:=:` swap territory,
not LCONCAT).

---

## What this rung does NOT do

  - **Does not migrate AST_BANG_BINARY.**  AST_BANG_BINARY (`!E`) has
    no scalar SM-opcode mirror — even with one child it's "apply proc
    to each list element", which needs runtime list iteration.
    Phase 3 of CH-17i-bang-concat (when it lands) will handle this.
  - **Does not handle AST_LCONCAT generative case.**  The legacy
    fallthrough remains for `is_suspendable(child)` programs.  Per the
    sweep above, the Icon corpus does not exercise this case today;
    Phase 2 is deferred until a corpus program forces the issue.
  - **Does not unblock rung11_bang_augconcat_*.**  Those programs FAIL
    under `--interp` via a different mechanism (no `SM_PUSH_EXPR` fire
    in pre-rung audit) — likely augmented-concat (`||:=`) lowering.
    Separate territory; deferred per the goal note.
  - **Does not flip rung15_real_swap_swap_basic / _swap_str.**  Those
    test `:=:` (swap operator), not `|||`.  Different rung.

---

## Commit

```
CH-17i-bang-concat Phase 1: AST_LCONCAT scalar value-path lowering

Mirror AST_CAT's else-branch in sm_lower.c: when AST_LCONCAT's children
are non-generative, lower each child as a value and emit SM_CONCAT
between adjacent pairs.  Falls through to the legacy emit_push_expr +
SM_BB_PUMP path when any child is is_suspendable (Phase 2 territory,
empirically not exercised by current Icon corpus — deferred).

Headline gain: rung15_real_swap_lconcat (`s := "hello" ||| " world"`)
flips --interp / --run FAIL→PASS byte-identical to --ir-run output.
Pre-rung the program SIGABRTed with "sm_interp: stack underflow" because
the legacy fallthrough was net-stack-zero — AST_ASSIGN's SM_STORE_VAR
underflowed when popping the absent RHS value.

Files touched: src/runtime/x86/sm_lower.c (one new case, ~30 lines).
No new opcodes, no runtime additions, no JIT codegen changes.

Gates byte-identical: smoke ×6 (7/5/5/5/5/4), isolation, unified_broker
49/0, scrip_all_modes 2/0, Icon --ir-run 177/56/30.
Icon --interp corpus: 100/163 → 101/162 (+1 = rung15_real_swap_lconcat).

Documented: docs/CHUNKS-step17i-bang-concat-phase1-validation.md
```
