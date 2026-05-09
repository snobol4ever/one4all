# CH-17g-runtime-bridge-4 — extend `icn_try_call_builtin_by_name` to multi-arg pure transforms + read/stop

**Rung:** CH-17g-runtime-bridge-4 (continuation of bridge coverage; bridge-3
landed sess 2026-05-09 with the 8 single-arg pure value transforms)
**Session:** 2026-05-09
**Predecessor:** CH-17g-runtime-bridge-3
(`docs/CHUNKS-step17g-runtime-bridge-3-validation.md`).

## What this rung does

Extends `icn_try_call_builtin_by_name` from 10 names (after bridge-3) to
27 names by adding 17 more EXPR_t-free Icon builtins:

| Family | Names | Arity |
|--------|-------|-------|
| String transforms | `repl`, `reverse`, `map`, `trim` | 1–3 |
| Pad/truncate | `left`, `right`, `center` | 1–3 |
| Math | `abs`, `max`, `min`, `sqrt` | 1–N |
| Containers | `copy`, `list`, `table` | 0–2 |
| I/O | `read`, `reads` | 0 / 1 |
| Process control | `stop` | 0+ (current impl ignores args) |

Each branch is a verbatim port of the equivalent in-eval branch in
`interp_eval.c`'s E_FNC switch with the same two mechanical changes
applied throughout bridge-3:

1. `interp_eval(e->children[i])` → `args[i-1]` (already pre-evaluated by
   the SM_CALL_FN handler before invoking the helper).
2. `return X;` → `*out = X; return 1;` to honour the helper's
   1=handled / 0=fall-through contract.

The in-eval branches are **not removed** — they remain on the legacy
`coro_bb_fnc → icn_call_builtin → interp_eval` IR-walker path that
serves `--ir-run`.  Pure additive, same as bridge-3.

## Why this batch

All 17 names share the constraints from bridge-3's selection criteria:

- Pure value transforms / IO that depends only on argument values.
- Single-pass argument evaluation.
- No write-back through `e->children[i]` lvalue identity.
- No `&pos`/`&subject` mutation (these touch scan-context state and
  remain deferred to a future rung).
- Frame-independent.

Three notes on subtleties handled in this rung:

### Note 1: `trim`'s `g_lang` read

`trim` reads the global `g_lang` to choose between Icon and Raku
semantics.  Verified safe in chunk path: `polyglot_execute`
(`polyglot.c:260`) sets `g_lang = 1` for any Icon program before any
proc dispatch happens, and the value persists through the
SM_CALL_FN handler invocation.  The chunk inherits the same
runtime context the IR walker would have.

### Note 2: `max`/`min` arg loop bounds

The in-eval branches loop `for (int _j = 2; _j <= nargs; _j++)` because
`e->children[1]` is the first arg and `e->children[nargs]` is the last.
The helper's `args[]` is 0-indexed and tightly sized to `nargs`, so the
ported loop becomes `for (int _j = 1; _j < nargs; _j++)`.  Verified
byte-identical with `max(3,7,1,9,5)` → `9` and `min(3,7,1,9,5)` → `1`.

### Note 3: `stop()` ignores args

The in-eval branch is `if (!strcmp(fn,"stop")) { exit(0); }` — no arity
check, no arg processing, no write-to-stderr.  This is not Icon-spec
conformant (Icon `stop(s1,s2,...)` writes args to `&errout` then
exits with status 1), but it is what the legacy IR-walker does.  The
helper mirrors verbatim; the spec gap is pre-existing and tracked
separately if it ever surfaces.

## Empirical proof

```
$ cat /tmp/probe3.icn
procedure main()
    write(repl("ab", 3));
    write(reverse("hello"));
    write(map("Hello"));
    write(map("hello", "el", "EL"));
    write(trim("hello   ", " "));
    write(left("hi", 6, "."));
    write(right("hi", 6, "."));
    write(center("hi", 6, "."));
    write(abs(-7));
    write(max(3, 7, 1, 9, 5));
    write(min(3, 7, 1, 9, 5));
    write(sqrt(16));
    write(image(list(3, "x")));
    write(image(table()));
end

$ ./scrip --ir-run /tmp/probe3.icn
ababab
olleh
hello
hELLo
hello
hi....
....hi
..hi..
7
9
1
4.0
record
table(0)

$ diff <(./scrip --ir-run /tmp/probe3.icn) <(./scrip --sm-run /tmp/probe3.icn)
$ # (empty — byte-identical)
```

```
$ cat /tmp/probe_read.icn
procedure main()
    line := read();
    write("got: ", line);
end

$ echo "hello stdin" | ./scrip --ir-run /tmp/probe_read.icn
got: hello stdin

$ diff <(echo "hello stdin" | ./scrip --ir-run /tmp/probe_read.icn) \
       <(echo "hello stdin" | ./scrip --sm-run /tmp/probe_read.icn)
$ # (empty — byte-identical)
```

## What this exposes — and what it doesn't

`test/icon/queens.icn` was a previously-diverging program now reaching
deeper into chunk execution.  After bridge-4 it gets past `repl` and
`list` and now FATALs on `sm_interp: unhandled opcode 82 (SM_ACOMP) at
pc=92`.  This is a **different problem** — it's an opcode-handler gap
in `sm_interp.c`, not a builtin-coverage gap in the bridge.  Tracked
separately (queens uses Icon's array constructor `[a,b,c]` syntax
which lowers to `SM_ACOMP`; the chunk-path `sm_interp` doesn't yet
handle that opcode, but the IR-walker does).  Not regressed by this
rung — surfaced by it.

`test/icon/meander.icn` similarly reaches further now (uses `read`,
`integer`, `tab`, `upto`, `move`, `find`, `repl`).  After bridge-4 it
gets past `read` and `integer` and FATALs on the next unbridged
builtin, `tab` (which is scan-context, the next bridge family).

## Gates

| Gate | Result |
|------|--------|
| smoke ×6 (snobol4, icon, prolog, raku, snocone, rebus) | PASS (7/7, 5/5, 5/5, 5/5, 5/5, 4/4) |
| isolation gate | PASS — no IR-only symbol leaks in SM runtime files |
| unified_broker | PASS=49 FAIL=0 (byte-identical to baseline) |
| scrip_all_modes | PASS=2 FAIL=0 |
| Icon corpus `--ir-run` (test_icon_ir_all_rungs) | PASS=186 FAIL=47 XFAIL=30 TOTAL=263 (byte-identical to baseline) |
| **NEW: trivial Icon proc using all 14 multi-arg names, `--sm-run` byte-identical to `--ir-run`** | PASS — 14 calls covering repl, reverse, map (×2 forms), trim, left, right, center, abs, max, min, sqrt, image(list(...)), image(table()) |
| **NEW: read() under `--sm-run` byte-identical to `--ir-run`** | PASS |

## Scan-context family deferred

The next major bridge family is the **scan-context builtins** —
`tab`, `move`, `find`, `upto`, `match`, `any`, `many`.  These read
and mutate `&pos` and `&subject`, both file-scope globals in
`interp_eval.c`.  They will need their own rung because:

1. `&pos` / `&subject` interact with the `?` scan operator which
   pushes/pops a scan environment (see `scan_try_call_builtin` —
   already in the bridge-2 dispatch path before the helper).
2. Some signatures take cset args (e.g. `upto(c)`) which are
   produced by `&letters`, `&ucase`, `&digits` etc. — these are
   keyword reads that need to be confirmed safe under chunk
   dispatch.
3. The existing `scan_try_call_builtin` path may already handle some
   of these — needs investigation before bridging duplicates.

Deferred to a future rung: CH-17g-runtime-bridge-5 (scan-context).

## Files

- `src/driver/interp_eval.c` (+261 −1) — 17 new branches in
  `icn_try_call_builtin_by_name`, between the `image` branch and
  the closing `return 0;`.

No new opcodes, no IR fields, no `sm_lower.c` changes, no
`sm_interp.c` changes (bridge-2's wire-up at SM_CALL_FN already
routes every name through the helper).

## What still doesn't work under `--sm-run`

Programs using:
- **Scan-context builtins**: `tab`, `move`, `find`, `upto`, `match`,
  `any`, `many` — bridge-5 territory.
- **Mutator builtins**: `push`, `pop`, `pull`, `get`, `put`, `delete`,
  `insert` — write back through `e->children[1]` lvalue identity, need
  EXPR_t-aware path or per-kind chunk migration under CH-17h.
- **Generator-shape builtins**: `seq`, `every`-driven generators,
  `!container` iteration — these are not E_FNC but their own E_*
  kinds, addressed by CH-15b / CH-17h.
- **Opcode coverage gaps**: `SM_ACOMP` (array composition `[a,b,c]`),
  surfaced by `queens.icn` after bridge-4 — separate `sm_interp`
  fix, not a bridge issue.

## Coverage summary across bridge-1 through bridge-4

| Rung | Names added | Cumulative count |
|------|-------------|------------------|
| bridge-1 (refactor) | (write, writes — extracted from inline) | 2 |
| bridge-2 (wire) | (none — wires existing 2 into SM_CALL_FN) | 2 |
| bridge-3 | integer, real, string, numeric, char, ord, type, image | 10 |
| bridge-4 | repl, reverse, map, trim, left, right, center, abs, max, min, sqrt, copy, list, table, read, reads, stop | 27 |

## Next rung options (still awaits Lon decision)

1. **bridge-5 (scan-context)** — `tab`, `move`, `find`, `upto`, `match`,
   `any`, `many`.  Needs `&pos`/`&subject` care.
2. **CH-17g-irrun-lowers** — make `--ir-run` invoke `sm_lower` /
   `sm_resolve_proc_entry_pcs` so `entry_pc >= 0` regardless of mode.
   Unblocks CH-17g-final.
3. **`SM_ACOMP` opcode handler** — surfaced by queens.icn after this
   rung; small fix in `sm_interp.c` to handle array-composition
   opcodes inside chunks.
