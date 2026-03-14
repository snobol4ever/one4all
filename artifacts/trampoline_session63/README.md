# Session 63 artifact

## Changes since session 62
- emit.c: scan DEFINE function bodies for named-pattern assignments (Parse, Command, Stmt, Label, Control, Comment, Compiland now compiled)
- emit.c: expr_contains_pattern now recurses into E_IMM/E_COND left children (Function, BuiltinVar, SpecialNm, ProtKwd, UnprotKwd now compiled)
- emit.c: pass 0a pre-registration moved before emit_fn so *PatName inside DEFINE bodies resolves to compiled path (fixes SpecialNm fallback in ss())
- emit_byrd.c: NamedPat.emitted flag prevents duplicate emission of same-named patterns from multiple DEFINE bodies

## Stats
- Lines: 26514
- MD5: c565e55dba5be8504d4679a95d58e3c8
- GCC: 0 errors (with engine.c + snobol4_pattern.c)

## Compiled named patterns
- 196 total (was 112 in session 62)
- Core grammar now compiled: Parse, Command, Stmt, Label, Control, Comment, Compiland
- Leaf patterns now compiled: Function, BuiltinVar, SpecialNm, ProtKwd, UnprotKwd
- Remaining match_pattern_at calls: 33, all bch/qqdlm (genuine runtime-dynamic locals — correct fallback)

## Active bug
pat_Expr left-recursive infinite loop on C stack.
Grammar: Expr → Expr14 → Expr13 → ... → Expr (left-recursive, cursor unchanged on first call).
Compiled Byrd boxes recurse via direct C function calls before cursor advances.
Stack overflow even at 64MB stack — this is unbounded recursion, not just deep recursion.

## Next action
Add cursor-advancement guard at α entry of each recursive grammar pattern:
if fresh call (entry==0) and cursor == last_entry_cursor for this frame → goto ω (fail immediately).
This mirrors what FENCE does at SNOBOL4 level but at the C compiled level.
Or: examine why pat_Expr recurses without advancing — the ARBNO/FENCE structure
should prevent this. May be an emit_byrd bug in FENCE or ARBNO wiring.
