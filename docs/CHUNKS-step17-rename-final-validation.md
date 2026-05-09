# CH-17-RENAME-FINAL validation — sess 2026-05-09

## Rung

Drop legacy `EXPR_t`/`EXPR_e` aliases; confirm zero residual chunk
vocabulary in code symbols.  Precondition: CH-17-RENAME-a through
CH-17-RENAME-h all landed.

## Precondition check

```
grep -rn "\bEXPR_t\b\|\bEXPR_e\b" src/ test/ scripts/
```

Result: zero hits outside `scripts/test_isolation_ir_sm.sh` lines
88–116, which contain a historical comment block and the grep regex
`IR_FIELDS_RE='(\(EXPR_t[[:space:]]*\*\)|...)'` that enforces the
structural rule.  Those references are kept verbatim: they document
the rule and the pattern it matches.  No live C symbol named `EXPR_t`
or `EXPR_e` exists anywhere in the tree.

## Alias situation

The AR-1 commit (sess 2026-05-09, `4c96e9e7`) renamed the actual
struct and enum in-place:

```
src/ast/ast.h:44   typedef enum AST_e { ... } AST_e;
src/ast/ast.h:251  typedef struct AST_t AST_t;
src/ast/ast.h:253  struct AST_t { AST_e kind; ... };
```

No transitional `typedef EXPR_t AST_t` shim was ever introduced;
the rename was direct.  FINAL therefore has no aliases to delete —
the canonical form is already in place.

## chunk vocabulary audit (code symbols)

Gate: `grep -rni "\bchunk\b" src/runtime/ src/driver/ src/frontend/`
must return zero outside historical `CHUNKS-step`-tagged comments.

CH-17-RENAME-g-cleanup (sess 2026-05-09, `bcdc7e2c`) completed the
symbol sweep.  Remaining `chunk` occurrences in src/ are all inside
comments tagged `CHUNKS-step04`, `CHUNKS-step17b''`, `CHUNKS-step05`
etc. — historical closed-rung records, kept verbatim per convention.

Audit result: **zero live code symbols with `chunk` vocabulary**.

## Mapping table (EXPR_t → AST_t; chunk → expression)

| Old name              | New name                  | File(s)         |
|-----------------------|---------------------------|-----------------|
| `EXPR_t`              | `AST_t`                   | ast.h + all src |
| `EXPR_e`              | `AST_e`                   | ast.h + all src |
| `E_*` enum members    | `AST_*`                   | ast.h + all src |
| `SmChunk_t`           | `SmExpression_t`          | sm_prog.h       |
| `SM_PUSH_CHUNK`       | `SM_PUSH_EXPRESSION`      | sm_prog.h       |
| `SM_CALL_CHUNK`       | `SM_CALL_EXPRESSION`      | sm_prog.h       |
| `sm_call_chunk`       | `sm_call_expression`      | sm_interp.c/h   |
| `push_chunk_descr`    | `push_expression_descr`   | sm_interp.c     |
| `g_chunk_body_lowering` | `g_expression_body_lowering` | sm_lower.c  |
| `sm_emit_push_chunk`  | `sm_emit_push_expression` | sm_lower.c      |
| `sm_emit_call_chunk`  | `sm_emit_call_expression` | sm_lower.c      |
| `g_chunk_scope`       | `g_expression_scope`      | sm_lower.c      |
| `chunk_scope_walk`    | `expression_scope_walk`   | sm_lower.c      |
| `SCRIP_CHUNKS_AUDIT`  | `SCRIP_EXPRS_AUDIT`       | sm_interp.c     |
| `g_chunks_audit_*`    | `g_exprs_audit_*`         | sm_interp.c     |
| `CHUNK_REG_MAX`       | `EXPRESSION_REG_MAX`      | rt.c            |
| `g_chunk_reg_count`   | `g_expression_reg_count`  | rt.c            |
| `ChunkRegEntry`       | `ExpressionRegEntry`      | rt.c/h          |
| `rt_chunk_entry`      | `rt_expression_entry`     | rt.h            |
| `scrip_rt_register_chunks` | `scrip_rt_register_expressions` | rt.h  |
| `pl_chunk_t`          | `pl_expression_t`         | pl_runtime.h    |
| `g_chunk_reg`         | `g_expression_reg`        | rt.c            |

## Gates

- Build: **PASS**
- Smoke ×6: **PASS** (SNO 7/7, ICN 5/5, PL 5/5, RK 5/5, SC 5/5, RB 4/4)
- Isolation gate: **PASS**
- unified_broker: **PASS=49**
