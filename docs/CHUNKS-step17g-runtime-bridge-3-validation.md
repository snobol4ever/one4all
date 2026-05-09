# CH-17g-runtime-bridge-3 — extend `icn_try_call_builtin_by_name` coverage to eight pure value-transform builtins

**Rung:** CH-17g-runtime-bridge-3 (third of the bridge plan; bridge-1 and
bridge-2 closed sess 2026-05-09)
**Session:** 2026-05-09
**Predecessor:** CH-17g-runtime-bridge-2
(`docs/CHUNKS-step17g-runtime-bridge-2-validation.md`).

## What this rung does

Extends the Icon-builtin dispatch helper from `write` / `writes` to cover
eight additional EXPR_t-free, pure value-transform builtins:

| Builtin | Arity | Body shape |
|---------|-------|-----------|
| `integer(x)` | 1 | int → int; real → trunc; string → parse (incl. radix) |
| `real(x)` | 1 | real → real; int → real; string → strtod |
| `string(x)` | 1 | str → str; int/real → snprintf into GC buf |
| `numeric(x)` | 1 | int/real → as-is; string → int-then-real parse |
| `char(n)` | 1 | int / numeric-string → 1-byte string |
| `ord(s)` | 1 | first byte → int |
| `type(x)` | 1 | runtime tag → "integer"/"real"/"string"/"list"/"table"/<record-tag> |
| `image(x)` (0 or 1 arg) | 0 or 1 | structured printable repr; strings get C-style escapes |

Each branch is a verbatim port of the equivalent in-eval branch in
`interp_eval.c`'s E_FNC switch, with two mechanical changes:

1. `interp_eval(e->children[i])` → `args[i-1]` (already pre-evaluated by
   the SM_CALL_FN handler before invoking the helper).
2. `return X;` → `*out = X; return 1;` to honour the helper's
   1=handled / 0=fall-through contract.

The in-eval branches are **not removed** — they remain on the legacy
`coro_bb_fnc → icn_call_builtin → interp_eval` IR-walker path that
serves `--ir-run`.  Bridge-3 is purely additive.

## Why the eight names, in this batch, in this order

Bridge plan principle (recorded in bridge-DESIGN doc): "coverage extends
one builtin at a time."  The eight names above all share these
properties that make them safe for a single rung:

- **Pure value transforms.** Output depends only on argument values, not
  on the call's IR structure.  No `e->children[i]->kind` inspection,
  no `e->children[i]->ival` slot index reads, no `&pos`/`&subject`
  mutation.
- **Single-pass evaluation.** Each arg is read exactly once at the top
  and reused.  No re-evaluation that could double-fire on side effects.
- **No write-back through children[i] lvalue identity.** None of these
  builtins mutate their args (contrast `push`, `pop`, `pull`, `get`,
  which write back to `e->children[1]` and require lvalue access).
- **Frame-independent.** No FRAME.env reads or writes; no static-var
  storage interaction.

Builtins explicitly **excluded** from this rung (planned for follow-on
rungs or for the per-kind chunk migration under CH-17h):

| Builtin | Why deferred |
|---------|-------------|
| `read()`, `tab(p)`, `move(n)`, `find(s)`, `upto(c)`, `match(s)`, `any(c)`, `many(c)` | Touch `&subject`/`&pos` scan-context state; need careful re-entrancy review |
| `repl(s,n)`, `left/right/center(s,i,p)`, `reverse(s)`, `map(s,c1,c2)`, `trim(s,c)` | Mostly EXPR_t-free but multi-arg with default-handling; queue for follow-on |
| `copy(x)`, `list(n,x)`, `table(d)`, `set(L)` | EXPR_t-free; queue for follow-on |
| `push`, `pop`, `pull`, `get`, `put` | Write back through `e->children[1]` lvalue identity; need EXPR_t-aware path or per-kind chunk migration under CH-17h |
| `image(?)` with structures | The 1-arg `image` covered here handles int/real/string/table/record/&null; full-fidelity list/set/coexpression image is a corner case for a future rung |

## Empirical proof

```
$ cat /tmp/probe2.icn
procedure main()
    write(integer("42"));
    write(real("3.14"));
    write(string(42));
    write(numeric("17"));
    write(char(65));
    write(ord("A"));
    write(type(42));
    write(type("hi"));
    write(type(3.14));
    write(image(42));
    write(image("hello"));
end

$ ./scrip --ir-run /tmp/probe2.icn
42
3.14
42
17
A
65
integer
string
real
42
"hello"

$ diff <(./scrip --ir-run /tmp/probe2.icn) <(./scrip --sm-run /tmp/probe2.icn)
$ # (empty — byte-identical)
```

Cross-check against existing test/icon programs that exercise these
builtins: `test/icon/generators.icn` is now byte-identical between
`--ir-run` and `--sm-run` (it uses `integer` and other now-bridged
names).  `test/icon/meander.icn` and `test/icon/queens.icn` still
diverge under `--sm-run` because they use unbridged builtins
(`read`, `tab`, `find`, `move`, `repl`, `list`); coverage extension
will pick those up in follow-on rungs.

## Important placement subtlety — same as bridge-2

The helper is dispatched **first** at the SM_CALL_FN handler, before
the SNOBOL4 `INVOKE_fn`/`APPLY_fn` registry lookup.  Reason recorded in
bridge-2's validation doc: `APPLY_fn` raises a SNOBOL4 runtime error
via `sno_err` and `longjmp`s out through `g_sno_err_jmp` when a name
is not in any registry — control never returns to a post-`INVOKE_fn`
fallback.  The helper-first ordering keeps all dispatch decisions
inside C control flow.

Safety against shadowing: none of the eight new names are defined in
the SNOBOL4 builtin registry (verified by grep on the registry
population in `runtime/x86/snobol4.c`).  If a future Icon builtin
name overlaps with a SNOBOL4 builtin, the order would need to flip
back with a `setjmp` wrapper around `INVOKE_fn`.

## Gates

| Gate | Result |
|------|--------|
| smoke ×6 (snobol4, icon, prolog, raku, snocone, rebus) | PASS (7/7, 5/5, 5/5, 5/5, 5/5, 4/4) |
| isolation gate | PASS — no IR-only symbol leaks in SM runtime files |
| unified_broker | PASS=49 FAIL=0 (byte-identical to baseline) |
| scrip_all_modes | PASS=2 FAIL=0 |
| Icon corpus `--ir-run` (test_icon_ir_all_rungs) | PASS=186 FAIL=47 XFAIL=30 TOTAL=263 (byte-identical to baseline) |
| csnobol4 Budne suite | PASS=50 FAIL=100 SKIP=8 (matches CH-17g-call-sites baseline; environmental variance vs CH-17f's recorded 61) |
| **NEW: trivial Icon proc using new builtins, `--sm-run` byte-identical to `--ir-run`** | PASS — 11 calls covering all eight names |
| **NEW: test/icon/generators.icn `--sm-run` byte-identical to `--ir-run`** | PASS (was diverging pre-bridge-3) |

## What still doesn't work under `--sm-run`

Programs using Icon builtins not yet in the helper still surface
`Error 5: Undefined function or operation`:

- `read`, `tab`, `move`, `find`, `upto`, `repl`, `list`, `stop` —
  exercised by `test/icon/meander.icn`, `test/icon/queens.icn`
- `push`, `pop`, `pull`, `get`, `put`, `delete`, `insert`, `member` —
  list/table mutators
- `*` as size-of (`*L`, `*s`, `*T`) when reached in chunk paths —
  this is an operator, not a name-dispatched builtin; goes through
  a different opcode

Coverage extends incrementally per the bridge plan.

## Files

- `src/driver/interp_eval.c` (+162 −1) — eight new branches in
  `icn_try_call_builtin_by_name`, between the `writes` branch and
  the closing `return 0;`.

No new opcodes, no IR fields, no `sm_lower.c` changes, no
`sm_interp.c` changes (bridge-2's wire-up at the SM_CALL_FN handler
already routes every name through the helper).

## Next rung options

Per the broader sequencing question still "Awaits Lon decision":

1. **Continue bridge-3 coverage** — multi-arg pure transforms
   (`repl`, `left`, `right`, `center`, `reverse`, `map`, `trim`, `copy`,
   `list`, `table`).  Same low-risk pattern.
2. **CH-17g-irrun-lowers** — make `--ir-run` invoke `sm_lower` and
   `sm_resolve_proc_entry_pcs` so `entry_pc >= 0` regardless of mode.
   Unblocks CH-17g-final.
3. **Begin scan-context bridge** — `tab`, `move`, `find`, `upto`,
   `match`, `any`, `many` need careful `&pos`/`&subject` handling;
   may interact with the existing `scan_try_call_builtin` helper
   that bridge-2 already routes through.
