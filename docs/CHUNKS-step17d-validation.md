# CHUNKS-step17d-validation.md — CH-17d: sm_lower emits named-chunk skeletons for Prolog predicates

**Session:** #83, 2026-05-07
**Rung:** CH-17d — producer-side: named-chunk skeletons for every Prolog predicate in g_pl_pred_table
**one4all HEAD after landing:** (see git log)

---

## What landed

### Files touched

- `src/runtime/x86/sm_lower.c` — two changes:
  1. Added `#include "../../runtime/interp/pl_runtime.h"` (for `Pl_PredEntry`,
     `PL_PRED_TABLE_SIZE_FWD`, `g_pl_pred_table`).
  2. Added Prolog pred-chunk emission loop immediately after the Icon/Raku
     proc-chunk loop (before the "First pass: lower all statements" comment).

### Chunk skeleton shape (one per predicate, keyed by "name/arity")

```
SM_JUMP <skip_pred_NN>     ; forward-jump around the chunk
SM_LABEL "name/arity"      ; named entry — sm_label_pc_lookup target
SM_RETURN                  ; skeleton body — CH-17f will fill this in
<skip_pred_NN>:            ; anonymous skip target
```

### Why skeleton-only

Body lowering for Prolog (full E_CHOICE/E_CLAUSE IR walk) is CH-17f territory,
gated on CH-17e's consumer-side infrastructure (pl_box_choice_pc etc.).  This
rung only validates that sm_resolve_proc_entry_pcs (CH-17a) can find non-(-1)
entry_pcs for every predicate.  The chunks are forward-jumped over; nothing
reaches them at runtime.

---

## Empirical proof — SCRIP_PROC_ENTRY_PCS=1

```
$ SCRIP_PROC_ENTRY_PCS=1 ./scrip --interp /home/claude/corpus/programs/prolog/palindrome.pl
[CH-17a] resolve entry_pcs (proc_table=0 procs, pl_pred_table=hash)
[CH-17a]   pred  key=palindrome/2         entry_pc=1
[CH-17a]   pred  key=main/0               entry_pc=5
[CH-17a] summary: pl_total=2 pl_resolved=2 (others=-1 are CH-17b/d territory)
```

Before CH-17d all Prolog entry_pcs were -1 (noted in CH-17a summary comment:
"others=-1 are CH-17b/d territory").  After CH-17d every predicate in the
table resolves to a valid pc.  The abort that follows is the pre-existing
--interp Prolog crash (consumer still dormant — expected).

---

## Gates (all byte-identical to CH-17c baseline)

| Gate | Result |
|------|--------|
| smoke SNOBOL4 | PASS=7 FAIL=0 |
| smoke Icon | PASS=5 FAIL=0 |
| smoke Prolog | PASS=5 FAIL=0 |
| smoke Raku | PASS=5 FAIL=0 |
| smoke Snocone | PASS=5 FAIL=0 |
| isolation gate (test_isolation_ir_sm.sh) | PASS |
| unified_broker | PASS=49 FAIL=0 |
| Budne (csnobol4) | PASS=61 FAIL=89 SKIP=8 (pre-existing, baseline-identical) |
| Icon --ir-run all rungs | PASS=186 FAIL=47 XFAIL=30 TOTAL=263 |

---

## Next rung

**CH-17e** — flip Prolog consumers: add `pl_box_choice_pc`, `pl_box_clause_pc`,
`pl_box_builtin_pc`; flip `pl_pred_table_lookup` to return entry_pc; flip each
consumer site.  After CH-17e, `--interp` Prolog should work end-to-end for
the first time.
