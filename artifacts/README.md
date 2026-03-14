# Artifacts — beauty_tramp generated C snapshots

## Session 74 — 2026-03-14

**File:** beauty_tramp_session74.c  
**Lines:** 30108  
**md5:** 2925548631caf0b659c04669bef5b6ef  
**Status:** CHANGED from session73

### What changed
- `emit_pretty.h` extracted — shared 3-column formatter (PLG/PL/PS/PG macros)
- `emit_byrd.c` now uses shared header (`#define PRETTY_OUT byrd_out`)
- `emit.c` now uses shared header (`#define PRETTY_OUT out`)
- `goto_target_str()` + `emit_pretty_goto()` helpers added — capture goto targets for 3-column output
- `emit_ok_goto()` helper extracted — replaces 3× identical cond-goto tail blocks
- All structural label/goto lines in `emit_stmt` and `emit_fn` converted to PLG/PL/PS/PG
- Compiles clean, same behavior as session73

### Active bug
`c` field of UDEF tree node returns SSTR (type=1) instead of ARRAY — `START` produces empty output.
See SESSION.md for full trace.

---

## Session 73 — 2026-03-15

**File:** beauty_tramp_session73.c  
**Lines:** 30108  
**md5:** 95c6eb104a1ab7cf5c8415c9fbbf9245  
**Status:** CHANGED from session69

### What changed
- `emit_byrd.c` emit_simple_val: strips outer single-quote pairs from E_STR sval
- `emit.c` computed-goto dispatch: strips all quote chars before strcmp chain
- Two fixes committed in `5837bf1`

### Active bug
Same `c` field / START bug.

---

## Session 75 — 2026-03-14

**File:** beauty_tramp_session75.c  
**Lines:** 30628  
**md5:** d89f93d4e1c9c60f1dbf2481d3f503d9  
**Status:** CHANGED from session74

### What changed
- `emit_expr` E_CONCAT: chains ≥ 3 deep now emit multi-line indented `concat_sv`
- Long lines reduced from 68 → 17 (chains of 2 with large args stay inline)
- Remaining 17 long lines are `aply`/`strchr` embedded contexts, not solvable at E_CONCAT level

### Active bug
`c` field SSTR / START empty output — pre-existing.

---

## Session 76 — 2026-03-15

**File:** beauty_tramp_session76.c  
**Lines:** 31782  
**md5:** 2028344da06f3f862deae3efedf4bc9b  
**Status:** CHANGED from session75

### What changed
- Sprint `cnode-build`: `emit_cnode.h` + `emit_cnode.c` — CNode IR, arena allocator,
  `build_expr`/`build_pat`, `cn_flat_print`, `cn_flat_width`, `pp_cnode`. 0 mismatches
  vs `emit_expr` on full beauty.sno. Committed `160f69b`.
- Sprint `cnode-wire` (partial): `PP_EXPR`/`PP_PAT` macros added to emit.c.
  5 `SnoVal _v = emit_expr(...)` call sites wired to `pp_cnode`. Compiles, smoke tests pass.
  Long lines >200: 10 (down from 36). Max line still 3440 (emit_byrd SPAN/strchr, not expr).
  **NOT YET COMMITTED** — pending this handoff.

### Active bug
`c` field SSTR / START empty output — deferred, M-CNODE first.
