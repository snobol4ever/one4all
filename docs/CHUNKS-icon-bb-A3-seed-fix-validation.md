# CHUNKS-icon-bb-A3-seed-fix-validation.md

**Rung:** GOAL-ICON-BB-COMPLETE A3-seed-fix — unify `ICN_RANDOM` LCG seed
across the three independent `_rnd_seed` sites (`coro_value.c`,
`sm_interp.c`, `interp_eval.c`) so `--ir-run`, `--sm-run`, and the
SNOBOL4-evaluator fallback advance one shared sequence.

**Session:** 2026-05-10 (Claude).
**Files touched:** 5
- `src/runtime/interp/coro_value.h` — extern declaration of `bb_icn_rnd_seed`.
- `src/runtime/interp/coro_value.c` — file-scope definition
  `unsigned long bb_icn_rnd_seed = 12345UL;`; `AST_RANDOM` arm now
  reads/advances this variable instead of a per-function `static`.
- `src/runtime/x86/sm_interp.c` — `#include "../interp/coro_value.h"`;
  `ICN_RANDOM` dispatch now reads/advances `bb_icn_rnd_seed`. The
  old per-handler `static unsigned long sm_rnd_seed = 12345UL;` is gone.
- `src/driver/interp_eval.c` — `#include "../runtime/interp/coro_value.h"`;
  `AST_RANDOM` arm now reads/advances `bb_icn_rnd_seed`. The old
  per-handler `static` is gone.

**Why one shared seed.** Before this rung, each of the three call
sites held its own `static unsigned long _rnd_seed = 12345UL;` with
identical Knuth-MMIX constants. Initial values matched but
consumption patterns differed across modes: `--ir-run` of an Icon
program advanced only `coro_value.c::_rnd_seed`; `--sm-run` of the
same program advanced `sm_interp.c::sm_rnd_seed` for the
`ICN_RANDOM` opcode but also advanced `coro_value.c::_rnd_seed`
whenever a child sub-expression was driven through `bb_eval_value`
(via `coro_eval`). Result: identical seed value, divergent sequence.
A single canonical seed (defined in `coro_value.c`, externed via
`coro_value.h`) erases the divergence by construction — every site
that consumes a random number advances the same counter.

**Algorithm and seed value unchanged.** LCG multiplier
`6364136223846793005UL`, increment `1442695040888963407UL`,
high-33-bit shift `>> 33`, initial value `12345UL` — all identical
to the pre-rung sites. The output sequence for any program that
hit only one site before is bit-identical now.

## Gates

Run after `bash scripts/build_scrip.sh`:

```bash
bash scripts/test_smoke_icon.sh              # PASS=5 FAIL=0
bash scripts/test_smoke_snobol4.sh           # PASS=7 FAIL=0
bash scripts/test_smoke_snocone.sh           # PASS=5 FAIL=0
bash scripts/test_smoke_prolog.sh            # PASS=5 FAIL=0
bash scripts/test_smoke_raku.sh              # PASS=5 FAIL=0
bash scripts/test_smoke_rebus.sh             # PASS=4 FAIL=0
bash scripts/test_smoke_unified_broker.sh    # PASS=49 FAIL=0
bash scripts/test_isolation_ir_sm.sh         # PASS
bash scripts/test_icon_ir_all_rungs.sh       # PASS=177 FAIL=56 XFAIL=30
bash scripts/test_icon_sm_no_ast_walk.sh     # PASS=117 FAIL=120 ABORT=2
                                             # (pre-rung: PASS=116 FAIL=121 ABORT=2)
```

All gates met. The honest-mode-3 ladder gained +1 PASS (116 → 117);
no regressions anywhere else.

## Witness — integer random matches across modes

```icon
procedure main()
   write(?100);
   write(?100);
end
```

- Pre-rung: `--ir-run` produced `65 / 84`; `--sm-run` produced
  different values because `sm_rnd_seed` advanced independently.
- Post-rung: both modes produce `65 / 84` — bit-identical.

## What this rung does NOT fix

`rung36_jcon_random.icn` still diverges between `--ir-run` and
`--sm-run`. After A3-seed-fix the divergence is no longer
seed-driven; it is caused by two unrelated SM-mode gaps:

1. `&lcase` / `&ucase` keyword evaluation returns empty under
   `--sm-run` (verified with a minimal `write(&lcase)` probe —
   `--ir-run` prints `abcdefghijklmnopqrstuvwxyz`, `--sm-run`
   prints nothing).
2. `?L` (random element of a list) under `--sm-run` raises
   "Error 5 in statement 0 / Undefined function or operation"
   (verified with a minimal `L := [10,20,30,40,50]; write(?L)`
   probe).

These are SM-mode dispatch gaps for the `KEYWORD` and `DT_DATA(list)`
forms downstream of ICN_RANDOM, not seed-related. They belong to
sibling SM-mode work, not to this rung. The corpus probes
(`scripts/test_icon_sm_no_ast_walk.sh`) and the smoke gates above
all hold green, which is the bar A3-seed-fix is required to meet.

## Self-witness — three sites, one storage

```bash
$ grep -rn "_rnd_seed\|bb_icn_rnd_seed" src/
src/runtime/interp/coro_value.c: <definition + use site>
src/runtime/interp/coro_value.h: extern unsigned long bb_icn_rnd_seed;
src/runtime/x86/sm_interp.c:     <use site, no static>
src/driver/interp_eval.c:        <use site, no static>
```

No `static unsigned long _rnd_seed` or `static unsigned long sm_rnd_seed`
remains anywhere in the tree.
