# CH-17h-SURVEY — generator-kind dispatcher arm at sm_lower.c:1303 is dead code today

**Rung:** CH-17h-SURVEY (CH-15-SURVEY-style audit before per-kind migration)
**Session:** 2026-05-09
**Methodology:** temporary `getenv("SCRIP_CH17H_SURVEY")` instrumentation in
`emit_push_expr` and at the case-arm at `sm_lower.c:1303`, writing kind hits
to `/tmp/ch17h_audit.log`; corpus run; instrumentation reverted; clean tree
verified by `git diff` empty before commit.

## Finding

The dispatcher arm at `sm_lower.c:1303` matches nine Icon generator kinds
(`E_EVERY`, `E_SUSPEND`, `E_BANG_BINARY`, `E_LCONCAT`, `E_LIMIT`, `E_RANDOM`,
`E_SECTION`, `E_SECTION_MINUS`, `E_SECTION_PLUS`) and emits the legacy
`emit_push_expr + SM_BB_PUMP` pair.  Surveyed against:

| Test surface                            | runs   | hits |
|-----------------------------------------|-------:|-----:|
| smoke snobol4                           | 7      | 0    |
| smoke icon                              | 5      | 0    |
| smoke raku                              | 5      | 0    |
| smoke prolog                            | 5      | 0    |
| smoke snocone                           | 5      | 0    |
| smoke rebus                             | 4      | 0    |
| `test_smoke_unified_broker.sh`          | 49     | 0    |
| `test_icon_ir_all_rungs.sh` (full corpus) | 263  | 0    |
| `test_interp_broad_corpus_and_beauty.sh` | full   | 0    |
| `test_regression_full_corpus.sh`        | full   | 0    |
| Hand-crafted `every`/`suspend`/section program | 1 | 0    |

**Total `emit_push_expr` calls reaching the line-1303 arm in any test surface
exercised today: zero.**

The hand-crafted test program contained `every i := 1 to 5 do write(i)`,
`every i := !"abc" do write(i)`, `every write(seq())` calling a
`suspend 1 | 2 | 3` proc, and a string section `"hello"[2:4]`.  Output was
correct (`1\n2\n3\n4\n5\na\nb\nc\n1\n2\n3\nel\n`); the dispatcher arm did not
fire even once.

## Root cause (matches CH-15-SURVEY's diagnosis)

`sm_lower` walks the IR top-down through `lower_stmt` for stmt-context bodies
and through `lower_expr` for nested expressions.  The nine generator kinds at
this arm are reached only when one of them appears as a sub-expression that
`lower_expr` is asked to lower as a *value*.

In practice today:

1. `every E do S` lowers via the dedicated `E_EVERY` handling in `lower_stmt`
   (broker pumps), not as a value-context expression.  The `case E_EVERY:`
   arm at line 1303 of `lower_expr` is unreachable from stmt-context.
2. `suspend E` similarly lowers via `lower_stmt`'s suspend-aware path.
3. `s[i:j]` sections inside Icon expressions either fold into pattern-match
   primitives or get walked by `coro_eval` at runtime — they never reach
   `lower_expr` at the top level.
4. `!E` iterate, `\E` limit, `?L` random — same: walked by the runtime
   broker through IR, not lowered into SM ops.

CH-17b' lowered proc *bodies* through `sm_lower`, but those bodies are still
unreachable: `coro_call` walks the original IR and `coro_drive_fnc` is the
generator engine.  The lowered chunks are forward-jumped over.  Once
CH-17g-final flips the call layer to dispatch via `entry_pc`, these arms
will finally start firing — and only then can per-kind migration be
validated against real corpus runs.

This mirrors CH-15-SURVEY's finding for the line-1192 arm: dead today,
becomes live only after Step 17 closes.

## Recommendation

Defer per-kind migration of all nine generator kinds until **CH-17g-final**
lands.  Until then, any migration would be unvalidated by corpus runs (zero
fires) and worse: the wrong shape, because the consumer pattern (broker pump
on chunks, not on EXPR_t) needs the entry_pc dispatch infrastructure that
CH-17g-final delivers.

CH-17h was sequenced in `GOAL-CHUNKS-STEP17.md` before CH-17g-final
specifically because the spec author worried CH-17g-final couldn't drop the
legacy `coro_call` body without these kinds migrating first.  But this
survey shows the legacy body is the *only* code path these kinds reach
today; deleting it is what *creates* the test surface for migration, not
what blocks it.

**Reversed order:** land CH-17g-final first (it's structurally tractable;
the legacy `coro_call` body becomes dead weight after CH-17g-call-sites and
CH-17g-statics, not a live consumer); then CH-17h migrates the generator
kinds with real corpus validation in place.

If Lon prefers the original order, the alternative is to merge CH-17h into
CH-17g-final as a single rung — they are coupled.

## Files touched (all reverted before commit)

`src/runtime/x86/sm_lower.c` — temporary instrumentation in `emit_push_expr`
and at the case arm at line 1303.  `git diff` confirmed empty before this
doc was written.

## Gates re-confirmed after revert

| Gate | Result |
|------|--------|
| smoke snobol4 | PASS=7 |
| smoke icon | PASS=5 |
| smoke raku | PASS=5 |
| smoke prolog | PASS=5 |
| smoke snocone | PASS=5 |
| smoke rebus | PASS=4 |
| isolation | PASS |
| unified_broker | PASS=49 |

All byte-identical to the CH-17g-statics post-land baseline.
