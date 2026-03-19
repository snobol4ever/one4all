# archive/ir — Python IR prototype (pre-C-compiler era)

Archived session182. These files have no build role and will not acquire one.

## What's here

| File | What it was |
|------|-------------|
| `ir.py` | Python dataclasses for `Expr`, `PatExpr`, `Stmt`, `Program` — prototype SNOBOL4 statement IR. Fossil ancestor of `EXPR_t`/`STMT_t` in `frontend/snobol4/sno2c.h`. Used by abandoned Python frontends in `src/frontend/icon/` and `src/backend/net/`. |
| `byrd_ir.py` | Python dataclass model of Byrd Box four-port IR (α/β/σ/φ), ported from Jcon's `ir.icn`. Designed as shared IR for JVM and MSIL backends. Never connected to a live build. Reference: Jcon paper https://www2.cs.arizona.edu/icon/jcon/impl.pdf |
| `lower.py` | Pattern AST → Chunk/Goto lowering pass (irgen.icn equivalent). The `_emit()` function is the **canonical reference** for how `src/backend/c/emit_byrd.c` wires Seq/Alt/Arbno four-port control flow — `emit_byrd.c` cites it in comments. Smoke test at bottom is runnable and passes. |

## What moved out (NOT archived — still live)

`emit_cnode.c` and `emit_cnode.h` were formerly in `src/ir/byrd/` but are
active production code used by the C backend. They were moved to
`src/backend/c/` where they belong (session182).

## Why these are worth keeping (not deleting)

`lower.py` in particular is executable design documentation. The four-port
wiring for Seq, Alt, and Arbno in `_emit()` is correct and matches the gold
standard. If/when the JVM or MSIL backends are built seriously, `lower.py`
is the starting point, not a blank page.
