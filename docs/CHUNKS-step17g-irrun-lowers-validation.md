# CH-17g-irrun-lowers validation — sess 2026-05-09

## Rung

`--ir-run` invokes `sm_lower` / `sm_resolve_proc_entry_pcs` so
`entry_pc` resolves in proc_table / g_pl_pred_table regardless of
execution mode.  Precondition for CH-17g-final (which deletes
`EXPR_t *proc` from IcnProcEntry).

## Root cause (from CH-17g-final-SURVEY)

`sm_resolve_proc_entry_pcs` is called from `sm_preamble`, which
`--ir-run` non-SNO never invokes — `scrip.c:557` dispatches to
`polyglot_execute` directly.  Therefore every `entry_pc` stays -1
under `--ir-run`, the `proc_table_call` chunk-dispatch branch is
skipped, and the legacy `coro_call` body is the only live path.

## Implementation

### New flag `g_irrun_lowers` (polyglot.h / polyglot.c)

Exported integer; default 0.  When set, `polyglot_execute` calls
`sm_resolve_irrun_entry_pcs(prog)` after `polyglot_init` and before
the language-specific dispatch.

### New helper `sm_resolve_irrun_entry_pcs` (scrip_sm.h / scrip_sm.c)

Runs `sm_lower(prog)` → `sm_resolve_proc_entry_pcs(sm)` →
`sm_prog_free(sm)`.  Does NOT free the IR (polyglot_execute's BB
engine needs it alive).  Does NOT set `g_current_sm_prog` (the SM
is discarded immediately).

### scrip.c `--ir-run` non-SNO path

Sets `g_irrun_lowers = 1` before `polyglot_execute`, clears after.

### Guard pattern at every SM-dispatch site

`proc_table_call`, `pl_chunk_fn` (pl_broker.c), and all five
`pl_box_choice_pc` call sites in polyglot.c / interp_hooks.c /
interp_eval.c / pl_runtime.c now gate the SM path on
`g_current_sm_prog != NULL`.  Entry_pc resolved + SM absent →
fall through to legacy `coro_call` / `interp_eval` / `pl_box_choice`
path.  This is the correct --ir-run behaviour: entry_pcs resolve
for observability/future use, but execution remains on the IR path.

## Verification

```
SCRIP_PROC_ENTRY_PCS=1 ./scrip --ir-run hello.icn
# [CH-17a]   proc[0] name=main  entry_pc=1
# hello from icon

SCRIP_PROC_ENTRY_PCS=1 ./scrip --ir-run hello.pl
# [CH-17a]   pred  key=main/0  entry_pc=1
# Hello, World!
```

Both `--ir-run` and `--sm-run` produce identical output; entry_pcs
visible under SCRIP_PROC_ENTRY_PCS=1 in both modes.

## Gates

- Build: **PASS**
- Smoke ×6: **PASS** (SNO 7/7, ICN 5/5, PL 5/5, RK 5/5, SC 5/5, RB 4/4)
- Isolation gate: **PASS**
- unified_broker: **PASS=49**
- Specific gate: `SCRIP_PROC_ENTRY_PCS=1 --ir-run` shows non-(-1)
  entry_pcs for Icon proc + Prolog predicate: **PASS**

## Files changed

- `src/driver/polyglot.h` — `extern int g_irrun_lowers`
- `src/driver/polyglot.c` — flag definition + hook after polyglot_init
  + `extern SM_Program *g_current_sm_prog` guard at main/0 Prolog site
  + `#include` sm_lower.h, sm_prog.h, scrip_sm.h
- `src/driver/scrip_sm.h` — `sm_resolve_irrun_entry_pcs` declaration
- `src/driver/scrip_sm.c` — `sm_resolve_irrun_entry_pcs` implementation
- `src/driver/scrip.c` — set/clear `g_irrun_lowers` around non-SNO `--ir-run`
- `src/runtime/interp/coro_runtime.c` — `proc_table_call` guards on
  `g_current_sm_prog != NULL`
- `src/frontend/prolog/pl_broker.c` — `pl_chunk_fn` guards on
  `g_current_sm_prog != NULL` (void* extern — sm_prog.h not included)
- `src/driver/interp_hooks.c` — `pl_box_choice_pc` site guarded
- `src/driver/interp_eval.c` — two `pl_box_choice_pc` sites guarded
- `src/runtime/interp/pl_runtime.c` — two `pl_box_choice_pc` sites
  guarded (void* extern — sm_prog.h not included)
