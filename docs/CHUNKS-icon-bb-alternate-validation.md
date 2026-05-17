# CHUNKS-icon-bb-alternate-validation.md

**Rung:** GOAL-ICON-BB-COMPLETE A4 — CH-17i-alternate (AST_ALTERNATE).
Migrate `AST_ALTERNATE` (`E1 | E2 | E3 | …`, Icon's n-ary
generator-alternation) off the SM default fallthrough onto a real
SM lowering that drives the existing `coro_bb_alternate` Byrd box
via `SM_BB_PUMP_AST`.

**Session:** 2026-05-10 (Claude).

## Why this rung was needed

Pre-rung, `AST_ALTERNATE` had no `case` in `sm_lower.c`'s
`lower_expr` switch. Lowering hit the `default:` arm at
`sm_lower.c:1621`, which emits `SM_PUSH_NULL` and prints a stderr
warning when `g_expression_body_lowering` is false. Consequence:
in value context, `x := 1 | 2 | 3` under `--interp` assigned
`&null` to `x` and `write(x)` printed a blank line, while
`--ir-run` correctly assigned `1` and printed `1`.

Detection probe (pre-rung):

```icon
procedure main()
   x := 1 | 2 | 3;
   write(x);
   write(image(x));
end
```

| Mode | Output |
|------|--------|
| `--ir-run` | `1` / `1` |
| `--interp` (pre-rung) | blank / `&null` |

In `every` context this same expression already worked, because
`AST_EVERY`'s SM lowering at `sm_lower.c:1317` pumps its body
through a different path that consults `coro_eval` on the body
expression directly. The pre-rung gap was strictly the
value-context use — assignment, function arguments, etc.

## Implementation

`AST_ALTERNATE` joins the family of kinds already routed through
`SM_BB_PUMP_AST` (A1's `AST_BANG_BINARY` and `AST_LCONCAT`-gen,
A4-pulled-forward's `AST_ITERATE`). The pattern: register the
AST node in `g_ast_pump_table`, emit `SM_BB_PUMP_AST` with the
returned id. At runtime, the opcode handler at `sm_interp.c:1060`
looks up the AST node, calls `coro_eval(ast)` to build a
`bb_node_t`, and pulls one value with `node.fn(node.ζ, α)`.

`coro_bb_alternate` itself is unchanged — already implemented at
`coro_runtime.c:1418-1437`. It builds a left-recursive chain
`alt(alt(gen[0], gen[1]), gen[2]) …` so exhausting each branch
falls through to the next, exactly matching Icon's left-to-right
alternation semantics.

Files touched (1):

- `src/runtime/x86/sm_lower.c` — added `case AST_ALTERNATE:`
  immediately after the `AST_ITERATE` arm, mirroring A1/ITERATE
  shape: `sm_emit_i(p, SM_BB_PUMP_AST, register(e))`.

No changes to `sm_interp.c` (the `SM_BB_PUMP_AST` handler is
already AST-kind-agnostic), no changes to `coro_runtime.c` (the
Byrd-box implementation predates this rung), no new opcode.

## Gates

```bash
bash scripts/build_scrip.sh                  # clean
bash scripts/test_smoke_icon.sh              # PASS=5 FAIL=0
bash scripts/test_smoke_snobol4.sh           # PASS=7 FAIL=0
bash scripts/test_smoke_snocone.sh           # PASS=5 FAIL=0
bash scripts/test_smoke_prolog.sh            # PASS=5 FAIL=0
bash scripts/test_smoke_raku.sh              # PASS=5 FAIL=0
bash scripts/test_smoke_rebus.sh             # PASS=4 FAIL=0
bash scripts/test_smoke_unified_broker.sh    # PASS=49 FAIL=0
bash scripts/test_isolation_ir_sm.sh         # PASS
bash scripts/test_icon_ir_all_rungs.sh       # PASS=177 FAIL=56 XFAIL=30 (unchanged)
bash scripts/test_icon_sm_no_ast_walk.sh     # PASS=122 FAIL=115 ABORT=2
                                             # (pre-rung: PASS=117 FAIL=120 ABORT=2)
                                             # delta: +5 PASS, -5 FAIL — A4 progress
```

All gates met. Honest mode-3 dial moved +5 PASS without regressing
the ir-run baseline or any smoke. The ABORT count is unchanged at 2
(those programs segfault under `SCRIP_NO_AST_WALK=1` for reasons
unrelated to `AST_ALTERNATE`).

## Witnesses

### Output witness (anchor)

```icon
procedure main()
   x := 1 | 2 | 3;
   write(x);
end
```

- `--ir-run` → `1`
- `--interp` → `1`
- `SCRIP_NO_AST_WALK=1 --interp` → `1`

All three modes produce identical output. The honest mode-3 run
no longer falls back to the AST walker for the alternation.

### Structural witness

`SM_BB_PUMP_AST` is the Phase-A bridge opcode. It increments
`g_ast_pump_active`, which exempts the immediately-called
`coro_eval` from the `SCRIP_NO_AST_WALK=1` tripwire. This is the
documented "honest" bridge — `coro_eval` is reached, but only
through an explicit SM opcode whose contract is "drive this AST
node via its Byrd-box one tick". No SM dispatch reaches into the
AST walker by accident.

### Progress witness (corpus)

`test_icon_sm_no_ast_walk.sh` reports +5 programs flipped from
FAIL/ABORT to honest PASS. The diff between pre-rung and post-rung
runs of the script identifies which programs gained honest mode-3
status; this is the goal's progress dial.

## What this rung does NOT do

- **Does not delete the legacy fallthrough.** The
  `default:` arm at `sm_lower.c:1621` still emits `SM_PUSH_NULL`
  for unhandled AST kinds. Rung A7 (CH-17i-fallthrough-delete)
  replaces that with a `fprintf+abort` once Phases A1–A6 have
  drained all remaining kinds.

- **Does not change `--ir-run` behaviour.** Verified by the
  unchanged 177/56/30 baseline. The lowering change is SM-side
  only; the AST walker (`coro_eval` → `coro_bb_alternate`) is
  untouched.

- **Does not collapse `is_suspendable`.** `AST_ALTERNATE` remains
  in the `is_suspendable` set at `coro_runtime.c:688`; that is
  correct, because alternation **is** generative — A4 just makes
  the SM lowering drive the existing generator via
  `SM_BB_PUMP_AST` instead of the default `SM_PUSH_NULL`.

## Next rungs

- **A5 — CH-17i-seqexpr-gen** — `AST_SEQ_EXPR` (semicolon-joined
  parens that produce sequences). Same `SM_BB_PUMP_AST` shape
  expected; verify against `coro_value.c:899` (already grouped
  with `AST_ALTERNATE` and `AST_LIMIT` there).

- **A6** is unused (the goal file numbers `A4`/`A5`/`A6` cover the
  remaining legacy-fallthrough kinds; A4 closes ALTERNATE, A5
  closes SEQ_EXPR, A6 will be the final cleanup).

- **A7 — CH-17i-fallthrough-delete** — delete the legacy default,
  replace with an `abort()` arm. Witnessed by the audit script
  reporting zero `SM_PUSH_EXPR` fires across the whole Icon corpus.
