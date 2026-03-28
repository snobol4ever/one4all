# IR_AUDIT.md — Pre-Reorg Frontend IR Audit

**Produced by:** G-7 session, 2026-03-28
**Milestone:** M-G0-IR-AUDIT
**Purpose:** Map every node kind produced by each of the six frontends to the
unified `EKind` enum target. Gaps (node kinds not yet in `EKind`) are flagged
for Phase 1 (M-G1-IR-HEADER-DEF).

The **unified IR** is defined in `GRAND_MASTER_REORG.md` § The Shared IR.
The **target header** is `src/ir/ir.h` (created in M-G1-IR-HEADER-DEF).

---

## Summary

| Frontend | Own enum | Node kinds | Gap count | Status |
|----------|----------|-----------|-----------|--------|
| SNOBOL4 | `EKind` (sno2c.h) | 21 | 0 | Already the shared enum |
| Prolog | Uses `EKind` directly | 6 | 0 | Already wired |
| Snocone | `SnoconeKind` (lex tokens) → lowers to `EKind` | 0 new | 0 | Lowers cleanly to SNOBOL4 EKind |
| Icon | `IcnKind` (icon_ast.h) | 60 | ~35 need new EKind entries | Largest gap |
| Rebus | `REKind` + `RSKind` (rebus.h) | 48 expr + 17 stmt | ~20 need new EKind entries | Significant gap |
| Scrip | None | 0 | 0 | Polyglot container — no new semantics |

---

## Frontend 1 — SNOBOL4

**Source:** `src/frontend/snobol4/sno2c.h`
**Own enum:** `EKind` — this IS the shared enum. All other frontends map to it.

| EKind | Meaning | Already shared? |
|-------|---------|----------------|
| `E_QLIT` | String literal match | ✅ |
| `E_ILIT` | Integer literal | ✅ |
| `E_FLIT` | Float literal | ✅ |
| `E_NULV` | Null/empty value | ✅ |
| `E_VART` | Variable reference | ✅ |
| `E_KW` | `&IDENT` keyword | ✅ |
| `E_INDR` | `$expr` indirect / immediate-assign target | ✅ |
| `E_STAR` | `*expr` deferred/indirect pattern reference | ✅ |
| `E_MNS` | Unary minus | ✅ |
| `E_ADD` | Addition | ✅ |
| `E_SUB` | Subtraction | ✅ |
| `E_MPY` | Multiplication | ✅ |
| `E_DIV` | Division | ✅ |
| `E_EXPOP` | Exponentiation (`**`) | ✅ |
| `E_CONC` | Concatenation / sequence (n-ary) | ✅ |
| `E_OPSYN` | `&` operator: reduce(left, right) | ✅ |
| `E_OR` | `\|` pattern alternation (n-ary) | ✅ |
| `E_NAM` | `expr . var` conditional assignment | ✅ |
| `E_DOL` | `expr $ var` immediate assignment | ✅ |
| `E_FNC` | `f(args)` function call | ✅ |
| `E_ARY` | `a[subs]` named array subscript | ✅ |
| `E_IDX` | `expr[subs]` postfix subscript | ✅ |
| `E_ATP` | `@var` cursor position capture | ✅ |
| `E_ASGN` | `var = expr` assignment in expression context | ✅ |
| `E_UNIFY` | `=/2` Prolog unification | ✅ |
| `E_CLAUSE` | Prolog Horn clause | ✅ |
| `E_CHOICE` | Prolog predicate choice point | ✅ |
| `E_CUT` | Prolog `!` cut / FENCE | ✅ |
| `E_TRAIL_MARK` | Save trail top | ✅ |
| `E_TRAIL_UNWIND` | Restore trail | ✅ |

**Gap count: 0.** SNOBOL4 is the source of truth. The unified `ir.h` is
this enum extended with Icon and Rebus additions.

Also note: the GRAND_MASTER_REORG.md shared IR table lists additional node kinds
(`E_ARB`, `E_ARBNO`, `E_POS`, `E_RPOS`, `E_DOT`, `E_DOLLAR`, `E_SUSPEND`,
`E_TO`, `E_TO_BY`, `E_LIMIT`, `E_ALT_GEN`, `E_BANG`, `E_SCAN`, `E_SWAP`,
`E_POW`) that do not yet exist in `sno2c.h`. These are planned additions for
Icon and other frontends — added in M-G1-IR-HEADER-DEF.

---

## Frontend 2 — Prolog

**Source:** `src/frontend/prolog/prolog_lower.h`
**Own enum:** None — uses `EKind` directly from `sno2c.h`.

All six Prolog node kinds are already in `EKind`:
`E_CLAUSE`, `E_CHOICE`, `E_UNIFY`, `E_CUT`, `E_TRAIL_MARK`, `E_TRAIL_UNWIND`.

**Gap count: 0.** Prolog is fully wired to the shared enum.

---

## Frontend 3 — Snocone

**Source:** `src/frontend/snocone/snocone_lower.h`, `snocone_lex.h`
**Own enum:** `SnoconeKind` — but this is a **lexer token enum**, not an IR enum.
The `snocone_lower.c` pass converts tokens directly to `EKind` nodes.

The operator mapping (from `snocone_lower.h`) is complete and documented:

| Snocone token | Maps to | Notes |
|--------------|---------|-------|
| `SNOCONE_ASSIGN` | assignment stmt | via STMT_t, not EXPR_t |
| `SNOCONE_PLUS` | `E_ADD` | ✅ |
| `SNOCONE_MINUS` | `E_SUB` / `E_MNS` | ✅ |
| `SNOCONE_STAR` | `E_MPY` / `E_INDR` | ✅ |
| `SNOCONE_SLASH` | `E_DIV` | ✅ |
| `SNOCONE_CARET` | `E_EXPOP` | ✅ |
| `SNOCONE_CONCAT` | `E_CONC` | ✅ |
| `SNOCONE_OR` | `E_OR` | ✅ |
| `SNOCONE_PIPE` | `E_CONC` | ✅ |
| `SNOCONE_PERIOD` | `E_NAM` | ✅ |
| `SNOCONE_DOLLAR` | `E_DOL` | ✅ |
| `SNOCONE_AT` | `E_ATP` | ✅ |
| `SNOCONE_AMPERSAND` | `E_KW` | ✅ |
| `SNOCONE_TILDE` | `E_FNC("NOT",1)` | ✅ via E_FNC |
| `SNOCONE_EQ/NE/LT/GT/LE/GE` | `E_FNC("EQ",2)` etc. | ✅ via E_FNC |
| `SNOCONE_STR_*` | `E_FNC("LLT",2)` etc. | ✅ via E_FNC |
| `SNOCONE_PERCENT` | `E_FNC("REMDR",2)` | ✅ via E_FNC |
| `SNOCONE_CALL` | `E_FNC(name, nargs)` | ✅ |
| `SNOCONE_ARRAY_REF` | `E_IDX(name, nargs)` | ✅ |

**Gap count: 0.** Snocone lowers entirely to existing `EKind` nodes.
No new enum values needed. Phase 5 audit (`M-G5-LOWER-SNOCONE-AUDIT`) is
expected to be trivial.

Snocone's control-flow keywords (`if/else/while/for/return/etc.`) become
`STMT_t` IR nodes at the statement level, not `EXPR_t` — no new `EKind` needed.

---

## Frontend 4 — Icon

**Source:** `src/frontend/icon/icon_ast.h`
**Own enum:** `IcnKind` — 60 node kinds (ICN_KIND_COUNT sentinel).

This is the largest gap. Every `IcnKind` must either map to an existing `EKind`
or require a new `EKind` entry in `ir.h`.

### Mapping table

| IcnKind | Maps to EKind | Notes |
|---------|--------------|-------|
| `ICN_INT` | `E_ILIT` | ✅ exists |
| `ICN_REAL` | `E_FLIT` | ✅ exists |
| `ICN_STR` | `E_QLIT` | ✅ exists |
| `ICN_CSET` | `E_QLIT` or new `E_CSET` | ⚠ cset semantics differ from string lit |
| `ICN_VAR` | `E_VART` | ✅ exists |
| `ICN_ADD` | `E_ADD` | ✅ exists |
| `ICN_SUB` | `E_SUB` | ✅ exists |
| `ICN_MUL` | `E_MPY` | ✅ exists |
| `ICN_DIV` | `E_DIV` | ✅ exists |
| `ICN_MOD` | new `E_MOD` | ❌ not in EKind — add |
| `ICN_POW` | `E_POW` (planned) | ❌ add to ir.h |
| `ICN_NEG` | `E_MNS` | ✅ exists |
| `ICN_POS` | new `E_UPLUS` or fold into E_ILIT | ❌ add or fold |
| `ICN_RANDOM` | new `E_RANDOM` | ❌ Icon-specific |
| `ICN_COMPLEMENT` | new `E_CSET_COMP` | ❌ Icon cset complement |
| `ICN_CSET_UNION` | new `E_CSET_UNION` | ❌ Icon `++` |
| `ICN_CSET_DIFF` | new `E_CSET_DIFF` | ❌ Icon `--` |
| `ICN_CSET_INTER` | new `E_CSET_INTER` | ❌ Icon `**` |
| `ICN_BANG_BINARY` | new `E_INVOKE` | ❌ invoke with list args |
| `ICN_SECTION_PLUS` | new `E_SECTION_PLUS` | ❌ `E[i+:n]` |
| `ICN_SECTION_MINUS` | new `E_SECTION_MINUS` | ❌ `E[i-:n]` |
| `ICN_LT/LE/GT/GE/EQ/NE` | `E_FNC` or new `E_LT` etc. | ⚠ goal-directed — differ from SNOBOL4 |
| `ICN_SLT/SLE/SGT/SGE/SEQ/SNE` | `E_FNC` or new `E_SLT` etc. | ⚠ string comparisons |
| `ICN_CONCAT` | `E_CONC` | ✅ exists |
| `ICN_LCONCAT` | new `E_LCONCAT` | ❌ list concat `\|\|\|` |
| `ICN_TO` | `E_TO` (planned) | ❌ add to ir.h |
| `ICN_TO_BY` | `E_TO_BY` (planned) | ❌ add to ir.h |
| `ICN_ALT` | `E_ALT_GEN` (planned) | ❌ add to ir.h |
| `ICN_AND` | new `E_AND` | ❌ Icon `&` conjunction |
| `ICN_BANG` | `E_BANG` (planned) | ❌ add to ir.h |
| `ICN_SIZE` | new `E_SIZE` | ❌ Icon `*E` |
| `ICN_LIMIT` | `E_LIMIT` (planned) | ❌ add to ir.h |
| `ICN_NOT` | `E_CUT` is FENCE/cut; need new `E_NOT` | ❌ Icon `not` |
| `ICN_NONNULL` | new `E_NONNULL` | ❌ Icon `\E` |
| `ICN_NULL` | new `E_NULL_TEST` | ❌ Icon `/E` |
| `ICN_SEQ_EXPR` | `E_CONC` or new `E_SEQ_EXPR` | ⚠ expression sequence `;` |
| `ICN_EVERY` | new `E_EVERY` | ❌ Icon `every` |
| `ICN_WHILE` | new `E_WHILE` | ❌ |
| `ICN_UNTIL` | new `E_UNTIL` | ❌ |
| `ICN_REPEAT` | new `E_REPEAT` | ❌ |
| `ICN_IF` | new `E_IF` | ❌ |
| `ICN_CASE` | new `E_CASE` | ❌ |
| `ICN_ASSIGN` | `E_ASGN` | ✅ exists |
| `ICN_AUGOP` | new `E_AUGOP` | ❌ augmented assignment family |
| `ICN_SWAP` | `E_SWAP` (planned) | ❌ add to ir.h |
| `ICN_IDENTICAL` | new `E_IDENTICAL` | ❌ Icon `===` |
| `ICN_MATCH` | `E_FNC` or new `E_MATCH` | ⚠ scan `=E` |
| `ICN_SCAN` | `E_SCAN` (planned) | ❌ add to ir.h |
| `ICN_SCAN_AUGOP` | new `E_SCAN_AUGOP` | ❌ `E ?:= body` |
| `ICN_CALL` | `E_FNC` | ✅ exists |
| `ICN_RETURN` | new `E_RETURN` | ❌ procedure return |
| `ICN_SUSPEND` | `E_SUSPEND` (planned) | ❌ add to ir.h |
| `ICN_FAIL` | new `E_FAIL` | ❌ explicit fail |
| `ICN_BREAK` | new `E_BREAK` | ❌ loop break |
| `ICN_NEXT` | new `E_NEXT` | ❌ loop next |
| `ICN_PROC` | new `E_PROC` | ❌ procedure declaration |
| `ICN_FIELD` | new `E_FIELD` | ❌ `E.name` record field |
| `ICN_SUBSCRIPT` | `E_IDX` | ✅ exists |
| `ICN_SECTION` | new `E_SECTION` | ❌ `E[i:j]` |
| `ICN_MAKELIST` | new `E_MAKELIST` | ❌ `[e1,e2,...]` |
| `ICN_RECORD` | new `E_RECORD_DECL` | ❌ record declaration |
| `ICN_GLOBAL` | new `E_GLOBAL_DECL` | ❌ global declaration |
| `ICN_INITIAL` | new `E_INITIAL` | ❌ initial clause |

**Gap count: ~35 new EKind entries needed for Icon.**

Note: several ⚠ entries (goal-directed comparisons, `ICN_SEQ_EXPR`) need
design discussion during M-G5-LOWER-ICON-AUDIT — they may fold into existing
kinds or require new ones depending on backend wiring decisions.

---

## Frontend 5 — Rebus

**Source:** `src/frontend/rebus/rebus.h`
**Own enums:** `REKind` (48 expression kinds) + `RSKind` (17 statement kinds) + `RDKind` (2 declaration kinds).

Rebus is a SNOBOL4/Icon hybrid: P-component (patterns) maps to SNOBOL4 `EKind`,
L-component (control flow) maps to Icon-style nodes.

### REKind mapping

| REKind | Maps to EKind | Notes |
|--------|--------------|-------|
| `RE_STR` | `E_QLIT` | ✅ exists |
| `RE_INT` | `E_ILIT` | ✅ exists |
| `RE_REAL` | `E_FLIT` | ✅ exists |
| `RE_NULL` | `E_NULV` | ✅ exists |
| `RE_VAR` | `E_VART` | ✅ exists |
| `RE_KEYWORD` | `E_KW` | ✅ exists |
| `RE_NEG` | `E_MNS` | ✅ exists |
| `RE_POS` | `E_UPLUS` (new, shared with Icon) | ❌ add |
| `RE_NOT` | `E_FNC("DIFFER",1)` or `E_NOT` | ⚠ fold or add |
| `RE_VALUE` | `E_FNC("IDENT",1)` | ✅ via E_FNC |
| `RE_BANG` | `E_BANG` (planned) | ❌ add to ir.h |
| `RE_ADD` | `E_ADD` | ✅ |
| `RE_SUB` | `E_SUB` | ✅ |
| `RE_MUL` | `E_MPY` | ✅ |
| `RE_DIV` | `E_DIV` | ✅ |
| `RE_MOD` | `E_MOD` (shared with Icon) | ❌ add |
| `RE_POW` | `E_POW` (planned, shared with Icon) | ❌ add |
| `RE_STRCAT` | `E_CONC` | ✅ |
| `RE_PATCAT` | `E_CONC` | ✅ |
| `RE_ALT` | `E_OR` | ✅ |
| `RE_EQ/NE/LT/LE/GT/GE` | `E_FNC` (SNOBOL4-style) | ✅ via E_FNC |
| `RE_SEQ/SNE/SLT/SLE/SGT/SGE` | `E_FNC` (SNOBOL4-style) | ✅ via E_FNC |
| `RE_ASSIGN` | `E_ASGN` | ✅ |
| `RE_EXCHANGE` | `E_SWAP` (planned) | ❌ add |
| `RE_ADDASSIGN` | `E_AUGOP` (shared with Icon) | ❌ add |
| `RE_SUBASSIGN` | `E_AUGOP` | ❌ same |
| `RE_CATASSIGN` | `E_AUGOP` | ❌ same |
| `RE_CALL` | `E_FNC` | ✅ |
| `RE_SUB_IDX` | `E_IDX` | ✅ |
| `RE_RANGE` | `E_SECTION_PLUS` (shared with Icon) | ❌ add |
| `RE_COND` | `E_NAM` | ✅ |
| `RE_IMM` | `E_DOL` | ✅ |
| `RE_CURSOR` | `E_ATP` | ✅ |
| `RE_DEREF` | `E_STAR` | ✅ |
| `RE_PATOPT` | new `E_PATOPT` (Rebus-specific) | ❌ add |
| `RE_AUG` | `E_AUGOP` | ❌ add |

### RSKind mapping

| RSKind | Maps to EKind | Notes |
|--------|--------------|-------|
| `RS_EXPR` | (stmt wrapper, not EKind) | — |
| `RS_IF` | `E_IF` (shared with Icon) | ❌ add |
| `RS_UNLESS` | new `E_UNLESS` or `E_IF` negated | ❌ |
| `RS_WHILE` | `E_WHILE` (shared with Icon) | ❌ add |
| `RS_UNTIL` | `E_UNTIL` (shared with Icon) | ❌ add |
| `RS_REPEAT` | `E_REPEAT` (shared with Icon) | ❌ add |
| `RS_FOR` | new `E_FOR` | ❌ |
| `RS_CASE` | `E_CASE` (shared with Icon) | ❌ add |
| `RS_EXIT` | new `E_EXIT` | ❌ |
| `RS_NEXT` | `E_NEXT` (shared with Icon) | ❌ add |
| `RS_FAIL` | `E_FAIL` (shared with Icon) | ❌ add |
| `RS_RETURN` | `E_RETURN` (shared with Icon) | ❌ add |
| `RS_STOP` | new `E_STOP` | ❌ |
| `RS_MATCH` | `E_SCAN` or `E_FNC` | ⚠ |
| `RS_REPLACE` | `E_SCAN` + `E_ASGN` | ⚠ |
| `RS_COMPOUND` | `E_CONC` or (stmt wrapper) | ⚠ |

**Gap count: ~20 new EKind entries needed for Rebus** (most shared with Icon).

---

## Frontend 6 — Scrip

**Source:** None — no `src/frontend/scrip/` directory exists yet.

Scrip is **not a new language** — it is a polyglot compilation unit format.
A `.scrip` file is a container that can hold sections in any of the five
other languages (SNOBOL4, Icon, Prolog, Snocone, Rebus), compiled together
and linked so that procedures in any section can call procedures in any
other section.

**The Scrip frontend's job:**
1. Detect language sections within a polyglot source file
2. Dispatch each section to the appropriate frontend parser/lowerer
3. Wire cross-language call/return linkage at the IR level

**New node kinds introduced by Scrip:** None.
The IR nodes produced are exactly those of the constituent language sections.
Cross-language calls are `E_FNC` nodes — the same kind used for any function call.

**Gap count: 0.** Scrip adds no new `EKind` entries.

---

## Unified New EKind Entries Required

The following new kinds must be added to `ir.h` in M-G1-IR-HEADER-DEF,
beyond what currently exists in `sno2c.h`. Kinds shared by multiple frontends
are listed once.

### Planned (already in GRAND_MASTER_REORG.md shared IR table)
These are already designed — just need to be formally added to the enum:

`E_ARB`, `E_ARBNO`, `E_POS`, `E_RPOS`, `E_DOT`, `E_DOLLAR`,
`E_SUSPEND`, `E_TO`, `E_TO_BY`, `E_LIMIT`, `E_ALT_GEN`,
`E_BANG`, `E_SCAN`, `E_SWAP`, `E_POW`, `E_MOD`

### Icon/Rebus shared additions (new, not yet in reorg doc)
`E_AUGOP`, `E_RETURN`, `E_FAIL`, `E_BREAK`, `E_NEXT`, `E_IF`,
`E_WHILE`, `E_UNTIL`, `E_REPEAT`, `E_CASE`, `E_PROC`,
`E_UPLUS`, `E_SECTION_PLUS`, `E_SECTION_MINUS`, `E_SECTION`

### Icon-specific additions
`E_MOD` *(if not SNOBOL4-compatible)*, `E_RANDOM`, `E_CSET`,
`E_CSET_COMP`, `E_CSET_UNION`, `E_CSET_DIFF`, `E_CSET_INTER`,
`E_INVOKE`, `E_AND`, `E_SIZE`, `E_NOT`, `E_NONNULL`, `E_NULL_TEST`,
`E_EVERY`, `E_LCONCAT`, `E_IDENTICAL`, `E_SCAN_AUGOP`,
`E_MAKELIST`, `E_FIELD`, `E_RECORD_DECL`, `E_GLOBAL_DECL`, `E_INITIAL`

### Rebus-specific additions
`E_UNLESS`, `E_FOR`, `E_EXIT`, `E_STOP`, `E_PATOPT`

### Design decisions deferred to M-G5 per-frontend audits
Several ⚠ entries require decisions during the Phase 5 lower audits:
- Icon goal-directed comparisons (`ICN_LT` etc.) — fold into `E_FNC` or new kinds?
- `ICN_SEQ_EXPR` (`;`) — new `E_SEQ_EXPR` or reuse `E_CONC`?
- Rebus `RS_UNLESS` — new `E_UNLESS` or `E_IF` with negated condition?
- Rebus pattern match statements — `E_SCAN` or distinct `E_MATCH`/`E_REPLACE`?

These do not block M-G1 (the enum can include them as stubs) but their
wiring is resolved in Phase 5.

---

*M-G0-IR-AUDIT complete. Next: M-G1-IR-HEADER-DEF.*
