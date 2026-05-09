# CH-17g-statics Validation — static-var storage re-keyed off EXPR_t*

**Rung:** CH-17g-statics  
**Session:** 2026-05-09  
**One4all commit:** (this commit)

## What landed

`coro_runtime.c:static_tab[]` re-keyed from `EXPR_t *proc` pointer identity
onto `(int entry_pc, const char *proc_name)`:

- `static_ent_t` struct: replaced `EXPR_t *proc` field with `int entry_pc` +
  `const char *proc_name`.
- New `static_proc_entry_pc(const char *proc_name)` file-static helper:
  walks `proc_table[]` by name, returns `proc_table[i].entry_pc` (may be -1
  if sm_lower hasn't emitted the chunk yet).
- New `static_entry_matches(...)` predicate: primary key `(entry_pc, var_name)`
  when both sides have `entry_pc >= 0`; fallback to `(proc_name, var_name)`
  otherwise — covers the legacy `coro_call` path where entry_pc is still -1.
- `static_get` / `static_set`: signatures **unchanged** (still take `EXPR_t *proc`
  for compatibility with `coro_call` callers); internally extract `proc->sval`
  and resolve `entry_pc` via `static_proc_entry_pc`.  No external callsites exist.
- On `static_set` update: if a slot was stored with `entry_pc == -1` and the
  entry_pc has since been resolved, the slot is upgraded in place.

## Why the fallback is safe

Icon proc names are interned strings (unique per source proc, guaranteed by
`icon_parse` which writes `proc->sval = intern_n(...)`).  String identity under
`strcmp` provides exactly the same scoping guarantee that `EXPR_t*` pointer
identity provided: two procs with the same name cannot exist in the same
program; two procs with different names will never collide.

## No live runtime path now keys storage on EXPR_t identity

`static_get` and `static_set` are the only consumers of `static_tab[]`.
Both are file-static to `coro_runtime.c`; no external callers exist (confirmed
by `grep -rn static_get/static_set src/` returning only coro_runtime.c).

## Gates — byte-identical to CH-17g-call-sites baseline

| Gate | Result |
|------|--------|
| smoke snobol4 | PASS=7 FAIL=0 |
| smoke icon | PASS=5 FAIL=0 |
| smoke raku | PASS=5 FAIL=0 |
| smoke prolog | PASS=5 FAIL=0 |
| smoke snocone | PASS=5 FAIL=0 |
| smoke rebus | PASS=4 FAIL=0 |
| isolation | PASS |
| unified_broker | PASS=49 |
| scrip_all_modes | PASS=2 |
| Icon --ir-run | PASS=186 FAIL=47 XFAIL=30 TOTAL=263 |
| rung36_jcon_statics | PASS |

## Files changed

- `src/runtime/interp/coro_runtime.c` — static_ent_t struct + static_get/set + helpers
- `docs/CHUNKS-step17g-statics-validation.md` — this file

## Next rung

**CH-17g-final** — drop `EXPR_t *proc` from `IcnProcEntry`; lift `code_free`
gate so IR is freed unconditionally for all six frontends.  Preconditions:
CH-17g-call-sites ✅, CH-17g-statics ✅, CH-17h (remaining generator kinds).
CH-17h must land before CH-17g-final can drop the legacy `coro_call` body.
