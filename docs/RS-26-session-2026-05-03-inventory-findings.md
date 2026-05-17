# RS-26 inventory — routing Icon and Prolog through the SM pipeline

**Session:** 2026-05-03 (RS-21 / RS-25-investigation closure session)
**Author:** Lon Jones Cherryholmes · Claude Sonnet
**Status:** read-only inventory — no code changes in this document's session

This document captures what is already in place for Icon/Prolog SM routing
and what specifically blocks it.  A subsequent session takes RS-26 from
this inventory.

---

## TL;DR

Three out of four required pieces are already done.

| Piece | Status |
|-------|--------|
| `sm_lower.c` Icon dispatch (LANG_ICN → SM_PUSH_EXPR + SM_BB_PUMP) | ✅ already there (lines 1048-1054) |
| `sm_lower.c` Prolog dispatch (LANG_PL → lower subject + SM_BB_ONCE) | ✅ already there (lines 1055-1064) |
| `emit_push_expr` GC-clones EXPR_t subtrees so SM owns them post-`code_free` | ✅ already there (RS-9b, sm_lower.c:39-42) |
| Driver routing: `scrip.c` short-circuit + `proc_table` survival across `code_free` | ❌ blocking |

The driver-level work is the entire RS-26 scope.

---

## Scope of frontend kinds

### Icon emits 72 distinct EXPR kinds

`grep -hoE "\bE_[A-Z_]+\b" src/frontend/icon/icon_parse.c src/frontend/icon/icon_gen.c | sort -u`

### `sm_lower.c` covers 67 of them

The 9 Icon kinds NOT in `sm_lower.c`:

```
E_ALTERNATE
E_CSET
E_CSET_COMPL
E_CSET_DIFF
E_CSET_INTER
E_CSET_UNION
E_ITERATE
E_REVASSIGN
E_REVSWAP
```

These are NOT a blocker for RS-26.  The Icon LANG_ICN dispatch in
`sm_lower.c` does NOT walk the EXPR tree to lower per-kind — it does
`emit_push_expr(p, s->subject); sm_emit(p, SM_BB_PUMP)`.  The whole
EXPR_t tree is handed (cloned) to the BB engine, which walks it
itself.  So `sm_lower.c` only needs per-kind cases when SNOBOL4
shares a kind with Icon (E_ADD, E_VAR, etc.).  The 9 missing kinds
above are Icon-only and never reach the SNOBOL4 lowering path.

### Prolog emits 15 distinct EXPR kinds; all 15 are in `sm_lower.c`

```
E_ADD E_CHOICE E_CLAUSE E_CUT E_DIV E_FLIT E_FNC E_ILIT
E_MUL E_QLIT E_SUB E_TRAIL_MARK E_TRAIL_UNWIND E_UNIFY E_VAR
```

So Prolog has zero gap.

---

## What `polyglot_execute` does that `sm_preamble` does not

`polyglot_execute(prog)` for single-language Icon (`scrip.c` line 247-301
of `polyglot.c`):

1. `polyglot_init(prog, mask)` —
   (a) `label_table_build(prog)`
   (b) `prescan_defines(prog)`
   (c) `kw_case = 1`
   (d) Icon: `proc_count = 0`, `frame_depth = 0`, etc.; **walks `prog`
       to populate `proc_table[]` with `EXPR_t*` pointers into the live
       IR** (lines 142-178 of polyglot.c).
   (e) Prolog: `prolog_atom_init`, `g_pl_pred_table` reset, **walks
       `prog` to populate `g_pl_pred_table` with `EXPR_t*` choice
       pointers** (similar pattern lower in the same loop).
2. For Icon: `coro_call(proc_table[i].proc, NULL, 0)` — invokes the
   `main` proc via the BB engine, which walks the IR tree pointed to
   by `proc_table[main].proc`.
3. For Prolog: per-directive `interp_exec_pl_builtin` (using `g_pl_env`
   sized to the directive's max E_VAR slot), then `interp_eval(main_choice)`
   for `main/0`.

`sm_preamble(prog)` (`scrip_sm.c`):

1. `label_table_build(prog)`
2. `prescan_defines(prog)`
3. `g_sno_err_active = 1`
4. `sm = sm_lower(prog)` — produces SM_Program; `emit_push_expr` calls
   `expr_gc_clone` so SM owns its EXPR_t subtrees.
5. `g_current_sm_prog = sm`
6. **`code_free(prog)`** — destroys the original IR
7. **`label_table_clear_stmts()`** — nulls IR `STMT_t*` in label table

The asymmetry:

| What | polyglot_execute | sm_preamble |
|------|------------------|-------------|
| `label_table_build`         | ✓ | ✓ |
| `prescan_defines`           | ✓ | ✓ |
| `polyglot_init` (proc/pred tables, frame_stack, atoms) | ✓ | ✗ |
| `sm_lower`                  | ✗ | ✓ |
| `code_free(prog)`           | ✗ | ✓ |
| `label_table_clear_stmts()` | ✗ | ✓ |

The two divergences blocking RS-26:

**(D1)** `sm_preamble` doesn't run `polyglot_init`.  No `proc_table`,
no `g_pl_pred_table`, no Icon frame setup.  Without these the BB
engine has no way to dispatch user-defined Icon procs or Prolog
predicates.

**(D2)** `sm_preamble` runs `code_free(prog)` and
`label_table_clear_stmts()`.  For Icon/Prolog this is a memory
safety violation: `proc_table[*].proc` still points into the freed
IR.  The cloned EXPR_t pointers stored inside SM_BB_PUMP instructions
survive `code_free` (they're in GC memory), but the proc table's
proc-body pointers do NOT.

---

## Recommended RS-26 implementation path

The directive (RS-20) is "BB stays where it makes sense; SM is the
carrier; isolation absolute."  RS-26 honors this by routing Icon/Prolog
through `sm_preamble` + `sm_run_with_recovery` while keeping their BB
execution intact.  The IR must stay alive when BB drives it.

Three small changes:

**(C1) Symmetric preamble.**  In `sm_preamble`, add `polyglot_init(prog,
polyglot_lang_mask(prog))` after `prescan_defines`.  This populates
`proc_table` and `g_pl_pred_table` from the live IR.  For pure-SNO
programs this adds nothing observable (the lang_mask is just LANG_SNO
and the per-language branches are guarded).

**(C2) IR retention for non-SNO programs.**  In `sm_preamble`, gate
`code_free(prog)` and `label_table_clear_stmts()` on
`(polyglot_lang_mask(prog) == (1u << LANG_SNO))`.  When the program
contains any non-SNO statement, the IR stays alive — its proc/pred
table pointers must remain valid for `coro_call` and Prolog
dispatch to work.  This is consistent with the RS-9b rationale (which
freed the IR because SM_Program is self-contained for SNOBOL4): for
non-SNO, SM_Program's BB_PUMP / BB_ONCE opcodes hand off to a BB engine
that needs the unrelocated IR for cross-references.

**(C3) Driver routing.**  In `scrip.c` lines 456-457, remove the
`has_non_sno` short-circuit for the single-language case.  Multi-
language polyglot files (`.scrip` with mixed fences, `.md`) still
need `polyglot_execute` because mode-1 is the only path that handles
the registry-based per-module dispatch — keep that branch but tighten
its predicate from `has_non_sno` to `g_polyglot` (the multi-module flag
that `polyglot_execute` itself already checks).

After these three changes, `--interp /path/to/factorial.icn` should:

1. Parse Icon source (frontend dispatch in scrip.c, unchanged).
2. `sm_preamble` runs `polyglot_init` (Icon branch fires; `proc_table`
   filled), then `sm_lower` produces an SM_Program where the lone
   non-END statement (the `every write(...)` invocation, or a top-level
   call to `main()`) becomes `SM_PUSH_EXPR + SM_BB_PUMP + SM_HALT`.
   IR is NOT freed (mask shows LANG_ICN).
3. `sm_run_with_recovery` enters `sm_interp_run`.  SM dispatch walks
   the program: SM_PUSH_EXPR pushes the cloned EXPR_t onto the value
   stack; SM_BB_PUMP pops it, calls `coro_eval(expr)` to build a
   bb_node, drives via `bb_broker(root, BB_PUMP, ...)`.  The BB engine
   walks the IR (live, unfreed), reaches `proc_table[main].proc`,
   recurses through `coro_call` → `bb_exec_stmt` → `bb_eval_value`
   for the proc body.

Same for Prolog with SM_BB_ONCE.

---

## Where the isolation gate stands after RS-26 lands

Currently the RS-15 grep gate is **vacuously** satisfied for Icon and
Prolog because mode 2/3 doesn't run their programs.  After RS-26
those programs flow through `sm_preamble` + `sm_run_with_recovery`,
which in turn executes SM_BB_PUMP / SM_BB_ONCE, which call into
`coro_runtime.c` and `pl_runtime.c` (in the gate, IR-free per
RS-17/18/19) and the BB adapters `coro_value.c` and `coro_stmt.c`
(NOT yet in the gate, with documented `interp_eval` fallthrough).

So: RS-26 lands → the gate becomes substantive for Icon/Prolog
(`coro_runtime.c` and `pl_runtime.c` truly never call interp_eval
in a path SM mode reaches).  RS-22 absorbs the value-context kinds
that the BB adapters' fallthrough still routes through interp_eval,
and RS-23 promotes the adapters into the gate.

The path from here:

```
RS-26 (this work)
  └─ Icon/Prolog flow through sm_preamble + SM dispatch
     └─ RS-22  (absorb E_FNC, E_ASSIGN, etc. into bb_eval_value)
        └─ RS-23  (remove interp_eval extern from BB adapters; promote into gate)
           └─ RS-24  (strip dead Icon-frame switch from interp_eval.c)
              └─ RS-25  (final SM-shape verification — observable thin SM for Icon/Prolog)
```

---

## Verification commands a fresh session can run

After RS-26 lands:

```bash
# Force --interp on Icon and confirm SM_BB_PUMP shape
./scrip --interp --dump-sm /home/claude/corpus/programs/icon/rung01_paper_mult.icn
# Expected output: dominated by SM_PUSH_EXPR / SM_BB_PUMP / SM_STNO / SM_HALT

# Confirm runtime behavior unchanged
./scrip --interp /home/claude/corpus/programs/icon/rung01_paper_mult.icn
# Expected: 1 / 2 / 2 / 4 / 3 / 6  (unchanged from polyglot_execute path)

# Run the existing smoke
bash scripts/test_smoke_icon.sh    # all 5 PASS
bash scripts/test_smoke_prolog.sh  # all 5 PASS

# Isolation gate becomes substantive (was vacuously green for Icon/Prolog)
bash scripts/test_isolation_ir_sm.sh
```

## Update — what RS-26a actually landed and why RS-26b exists

RS-26a (committed in this session) landed C1 + C2 only:

- **C1:** `sm_preamble` calls `polyglot_init(prog, polyglot_lang_mask(prog))`
  after `prescan_defines`.  Pure-SNO is unchanged (lang_mask gates each
  per-language init branch); for Icon/Prolog this populates `proc_table`
  and `g_pl_pred_table` from the live IR.
- **C2:** `code_free(prog)` and `label_table_clear_stmts()` are gated on
  `lang_mask == (1<<LANG_SNO)`.  The IR survives for non-SNO programs.

C3 (the driver routing flip from `has_non_sno` to `g_polyglot`) was
attempted, built clean, but failed correctness on
`corpus/programs/icon/rung01_paper_mult.icn`:

```
$ ./scrip rung01_paper_mult.icn       # expected: 1 2 2 4 3 6
1
1            ← spurious
2
2
4
3
6
```

The cause is a real semantic mismatch, surfaced by RS-25-investigation
but not understood until C3 was tried:

`polyglot_execute` for single-language Icon does NOT iterate statements.
It runs `polyglot_init` (which registers `main` in `proc_table`) and
then directly calls `coro_call(proc_table[main].proc, NULL, 0)`.  The
proc *definitions* are never executed — they are registered.

The SM dispatch path (`sm_interp_run`) iterates the SM_Program — every
SM_PUSH_EXPR + SM_BB_PUMP fires.  When Icon proc defs lower to
SM_PUSH_EXPR + SM_BB_PUMP, the BB engine treats them as
generator-context expressions and drives them.  Driving `procedure
main() ...` is *not* the same as calling `main()`; it appears to be
where the spurious `1` comes from.

So C3 needs a companion change in `sm_lower.c`: Icon proc def
statements should not lower to BB_PUMP at all (proc registration is
already done by polyglot_init in the preamble), and a synthetic
top-level statement representing `main()` (or whatever entry point
the front-end identifies) should lower to a single SM_BB_PUMP.  The
shape becomes:

```
SM_STNO              stmt=0
SM_PUSH_EXPR         <call-main expr>
SM_BB_PUMP
SM_HALT
```

i.e. one BB_PUMP for the whole Icon program, exactly the RS-20 thesis.

That's RS-26b.  The full lowering change is small but touches Icon /
Raku / Prolog dispatch in `sm_lower.c`'s LANG_ICN / LANG_PL handling
(lines 1048-1064).  The companion driver flip from `has_non_sno` to
`g_polyglot` then becomes safe.

After RS-26b lands, the verification commands in the previous section
should produce `1 2 2 4 3 6` and the SM dump should show a 4-instruction
program for `rung01_paper_mult.icn`.
