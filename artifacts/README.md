# Artifacts

## Session 95 — 2026-03-15

### beauty_tramp_session95.c
- **md5:** cc34e62fee07676e12d0824c14fe6e85
- **lines:** 15639
- **CHANGED from session94** (31dfdcbf...)
- **compile status:** 0 errors, warnings only
- **crosscheck:** 106/106 pass (rungs 1–11 complete)

### Changes since session94
- fix(emit): `block_roman_end` undefined — alias emitted after `block_START` when first stmt has non-START label
- fix(emit): skip START in forward-decl loop and label table loop (no duplicate block_START)
- fix(emit): `E_IDX` subscript assignment now emits `aset` instead of falling through to `iset`
- fix(runtime): `ARRAY`, `TABLE`, `CONVERT`, `COPY` builtins registered in `SNO_INIT_fn`
- fix(corpus): `beauty.sno` lines 405–406 — `'comment'`→`'Comment'`, `'control'`→`'Control'` (case-sensitive variable names)
- fix(runner): crosscheck harness feeds `.input` files to programs that read INPUT

### Active bug / next action
- Rungs 1–11 all pass 100%. Sprint 3 (`crosscheck-ladder`) is COMPLETE.
- Next: Sprint 4 (`compiled-byrd-boxes-full`) — inline all pattern variables as static Byrd boxes, drop engine.c entirely. Gates on rung 11 being complete (it is).

## Session 96 — 2026-03-15

- **Artifact:** beauty_tramp_session96.c
- **Lines:** 15639
- **MD5:** 23999fc4dfbc67d272cec723b3dbb90d
- **Changed from previous:** CHANGED (was cc34e62f)
- **Compile status:** 0 errors, warnings only (-w suppressed)
- **What changed:** emit_byrd.c fix attempt — byrd_cond_emit_assigns now emits push_val for ~ captures. Fix is INCOMPLETE (pushes too many items — see Active Bug below).
- **Active bug:** byrd_cond_emit_assigns pushes ALL cond_ vars at _PAT_γ including epsilon ~ '' from untaken alternation branches. Reduce("Stmt",7) pops 7 but 18 are pushed. Fix must be inline push_val in emit_cond do_capture section + save/restore @S stack on backtrack.
