# CHUNKS-step17i-every — Validation

**Date:** 2026-05-10
**Sub-rung:** CH-17i-every (AST_EVERY half of CH-17i-every-suspend)
**Status:** LANDED

## What changed

Migrated `AST_EVERY` off the legacy `emit_push_expr + SM_BB_PUMP` path
onto a CH-17f-style pattern: register the AST in `g_every_table` at
lower-time, emit `SM_BB_PUMP_EVERY <every_id>` — no `AST_t*` in SM
bytecode or value stack. Runtime handler resolves through the table
(mirrors `g_pl_pred_table` for Prolog) and drives via
`coro_eval(AST_EVERY) + bb_broker(BB_PUMP, NULL, NULL)`.

Body-fn is **NULL**, not `pump_print`: `coro_bb_every` already runs
the AST_EVERY do-clause (e.g. `write(v)`) per tick via
`bb_exec_stmt(z->body)`; passing `pump_print` would double-print
every yielded value (verified empirically — first attempt produced
`1\n1\n2\n2\n…`).

Stack discipline: handler pushes `NULVCL` so the trailing
`SM_VOID_POP` from proc-body lowering's `lower_expr(body); SM_VOID_POP`
loop is balanced. The legacy `emit_push_expr + SM_BB_PUMP` shape was
net-stack-zero (push 1, pop 1) — root cause of the 111 `--interp`
divergences in the CH-17i survey (sess 2026-05-09 `dfe68c5b`).

`AST_SUSPEND` deferred to follow-on sub-rung **CH-17i-suspend** —
yields to a coroutine caller, different stack discipline.

## Files modified

- `src/runtime/x86/sm_prog.h` — `+SM_BB_PUMP_EVERY` enum
- `src/runtime/x86/sm_prog.c` — `+SM_BB_PUMP_EVERY` name table entry
- `src/runtime/x86/sm_interp.h` — `+every_table_register/lookup/reset`
- `src/runtime/x86/sm_interp.c` — `+g_every_table` impl,
  `+SM_BB_PUMP_EVERY` handler
- `src/runtime/x86/sm_codegen.c` — `+h_bb_pump_every` JIT mirror,
  registered in handler table
- `src/runtime/x86/sm_lower.c` — `+sm_interp.h` include; carved
  `case AST_EVERY:` out of legacy fallthrough; emits
  `SM_BB_PUMP_EVERY <every_id>` after `every_table_register(e)`

## Gates

| Gate                          | Before        | After         |
|-------------------------------|---------------|---------------|
| smoke snobol4                 | PASS=7 FAIL=0 | PASS=7 FAIL=0 |
| smoke icon                    | PASS=5 FAIL=0 | PASS=5 FAIL=0 |
| smoke prolog                  | PASS=5 FAIL=0 | PASS=5 FAIL=0 |
| smoke raku                    | PASS=5 FAIL=0 | PASS=5 FAIL=0 |
| smoke snocone                 | PASS=5 FAIL=0 | PASS=5 FAIL=0 |
| smoke rebus                   | PASS=4 FAIL=0 | PASS=4 FAIL=0 |
| isolation_ir_sm               | PASS          | PASS          |
| unified_broker                | PASS=49 FAIL=0| PASS=49 FAIL=0|
| broad_unified_broker          | PASS=6 FAIL=0 | PASS=6 FAIL=0 |
| scrip_all_modes               | PASS=2 FAIL=0 | PASS=2 FAIL=0 |
| Icon `--ir-run` corpus        | 177/56/30/263 | 177/56/30/263 |
| **Icon `--interp` rung01–04** | **5/24**      | **17/24**     |

## Rung01 byte-identical to expected (all six)

- `rung01_paper_compound`  (every write((1 to 2) to (2 to 3)))
- `rung01_paper_lt`        (every write(2 < (1 to 4)))
- `rung01_paper_mult`      (every write((1 to 3) * (1 to 2)))
- `rung01_paper_nested_to` (every write(3 < ((1 to 3) * (1 to 2))))
- `rung01_paper_paper_expr`(every write(5 > ((1 to 2) * (3 to 4))))
- `rung01_paper_to5`       (every write(1 to 5))

## Rung02 byte-identical (6 of 8)

PASS: nested_add, nested_filter, paper_mul, range, relfilter, add_proc.
The two FAIL (`proc_fact`, `proc_locals`) are **pre-existing** —
both fail under `--ir-run` too with
`sm_call_expression: invalid entry_pc 1`. Not regressions.

## Rung03 / rung04 (7 still fail)

All AST_SUSPEND-dependent — out of scope for this sub-rung.
FATAL on AST kind 50 (= AST_SUSPEND). Next sub-rung
**CH-17i-suspend** addresses these.
