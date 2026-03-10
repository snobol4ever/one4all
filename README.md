# SNOBOL4-tiny

A native SNOBOL4 compiler targeting x86-64 ASM, JVM bytecode, and MSIL.
Stackless. Goal-directed like Icon. Faster than SPITBOL.

Part of the [SNOBOL4-plus](https://github.com/SNOBOL4-plus) organization.

---

## What This Is

SNOBOL4-tiny compiles SNOBOL4 programs to native code using a
**Byrd Box** compilation model. Every expression — pattern or arithmetic —
is a generator with four labeled entry points:

| Label | Meaning |
|-------|---------|
| **α** | Enter fresh (start) |
| **β** | Resume (backtrack, try again) |
| **γ** | Succeed (pass value up) |
| **ω** | Fail (propagate failure up) |

This gives SNOBOL4-tiny true **goal-directed evaluation** (like Icon), compiled
to straight-line native code with no interpreter loop and no indirect dispatch.

---

## Why Not Just SPITBOL?

SPITBOL is the fastest SNOBOL4 implementation in existence. It uses the hardware
x86 stack (`rsp`) as the backtracking history stack — fast, but bounded:

| Limitation | SPITBOL | SNOBOL4-tiny |
|------------|---------|--------------|
| Backtrack stack | Hardware `rsp` (OS-bounded) | Heap `_t` structs (unbounded) |
| Goal-directed eval | No | Yes — every expr is a generator |
| Node dispatch | Indirect `jmp [pcode]` per node | Fully inlined template codegen |
| SIMD primitives | No (one char/cycle) | SSE2/AVX2 (16–32 bytes/cycle) |
| Targets | x86-64 only | x86-64, JVM bytecode, MSIL |
| CODE/EVAL | Interpreter mode switch | Runtime JIT (compiler always on) |

---

## The α/β/γ/ω Protocol

Every compiled node owns four labels. Concatenation wires γ of one node
to α of the next; failure wires ω back to β of the nearest choice point.

```c
// Example: LEN(3) compiled inline
len3_α:
    if (cursor + 3 > subject_len) goto len3_ω;
    saved_cursor = cursor;
    cursor += 3;
    goto len3_γ;          // succeed — pass control forward

len3_β:                   // resume (backtrack into us)
    cursor = saved_cursor;
    goto len3_ω;          // LEN is deterministic — immediately fail

len3_γ: /* ... next node's α ... */
len3_ω: /* ... previous choice point's β ... */
```

Alternation (`P | Q`) wires ω of P to α of Q, and Q's ω exits the choice.
ARBNO wraps a node's γ back to its own α, with ω exiting the loop.

---

## Architecture

```
SNOBOL4 source
    → Parser          → AST
    → IR builder      → Node graph (named flat table, supports cycles)
    → Code generator  → x86-64 ASM  /  JVM bytecode  /  MSIL
```

**The runtime already exists.** `SNOBOL4c.c` (1,064 lines of C) is a complete
SNOBOL4 pattern interpreter covering all 43 node types. Its match engine uses
four actions — PROCEED / SUCCESS / FAILURE / RECEDE — which are exactly the
α/β/γ/ω protocol implemented as an interpreter rather than compiled gotos.
The `.h` files (`C_PATTERN.h`, `CALC_PATTERN.h`, etc.) are pre-compiled
pattern data that the interpreter executes. A compiler emits C-with-gotos
(the `test_sno_*.c` format) instead — same semantics, zero dispatch cost.

**The parser is already written.** `Beautiful.sno` (SNOBOL4-dotnet repo)
contains a complete 17-level SNOBOL4 expression and statement parser written
as SNOBOL4 patterns (`snoExpr` through `snoExpr17`, `snoStmt`, `snoParse`).
Sprint 5: serialize those patterns into `SNOBOL4_EXPRESSION_PATTERN.h`,
`#include` it, add a 5-line stdin loop. No yacc. No new grammar.
The seed kernel executes the parser as pattern data — the Forth move.

---

## Sprint Plan

Development is test-driven. The test suite *is* the specification.
Each sprint adds exactly one new mechanism.

| Sprint | What | Mechanism |
|--------|------|-----------|
| 0 | Null program | Full runtime skeleton: α/β/γ/ω, output, entry/exit |
| 1 | Single token: `"hello"`, `POS(0)`, `RPOS(0)` | Literal, cursor primitives |
| 2 | Two-token sequences: `POS(0) RPOS(0)` | Concatenation wiring |
| 3 | Alternation: `"a" \| "b"` | Choice point, β backtrack |
| 4 | Assignment: `SPAN(DIGITS) $ OUTPUT` | Capture, immediate assign |
| 5 | ARBNO | Generator loop with γ→α rewire |
| 6 | Named patterns, recursive refs | REF node, cycle in IR graph |
| 7 | Multiple statements, variable subjects | Statement loop, env |
| 8 | CODE / EVAL | Runtime JIT, two-tier allocation |

Each completed sprint is tagged as a named snapshot (not a version number).

---

## Bootstrap Strategy

SNOBOL4-tiny follows the **Forth kernel discipline**: keep the seed as small
as possible, then build everything else in the language itself.

The analogy is direct:

| Forth | SNOBOL4-tiny |
|-------|-------------|
| ~12 native primitives | 8 primitive pattern nodes (LIT, ANY, POS, RPOS, LEN, SPAN, BREAK, ARB) |
| NEXT (3-instruction dispatch) | α/β/γ/ω wiring baked into compiled gotos — **zero** dispatch cost |
| `: word ... ;` defines new words | `NAME = pattern` defines new patterns |
| Dictionary (self-extending) | Named IR graph (already built) |
| Write Forth in Forth | Write the emitter in SNOBOL4 |

**Three phases:**

1. **Seed kernel (Sprints 0–4):** 8 primitive C templates in `emit_c.py`.
   Never add a primitive that can be expressed from existing ones — ARBNO
   is derivable from ARB + CAT + ALT and should be written in SNOBOL4, not
   hardcoded.

2. **Self-hosting emitter (Sprint 5+):** Rewrite `emit_c.py` as
   `src/codegen/emit.sno` — a SNOBOL4 program that reads IR descriptions
   and emits C. Runs on SNOBOL4-jvm for validation, SNOBOL4-python for speed.

3. **Bootstrap closure (Sprint 8+):** The emitter compiles itself and
   produces output identical to the CSNOBOL4/SPITBOL oracle. Same test
   as Snocone Step 9 in the sibling repos.

See [`doc/BOOTSTRAP.md`](doc/BOOTSTRAP.md) for the full design.

---

## Validation

Correctness is validated against three oracles:

- **SPITBOL x64** — speed reference
- **CSNOBOL4 2.3.3** — conformance reference
- **SNOBOL4-jvm / SNOBOL4-dotnet** — sibling implementations in this org

Test corpus: `SNOBOL4-corpus` (shared submodule), Gimpel library, Shafto AI corpus.

---

## Open Decisions

Two foundational questions are currently on the table. See
[`doc/DECISIONS.md`](doc/DECISIONS.md) for the full analysis.

**Decision 1 — Compiler implementation language: RESOLVED**
No yacc. No new grammar. `Beautiful.sno` (SNOBOL4-dotnet) contains a complete
17-level SNOBOL4 expression and statement parser written as SNOBOL4 patterns
(`snoExpr` through `snoExpr17`, `snoStmt`, `snoParse`). Sprint 5: serialize
those patterns into `SNOBOL4_EXPRESSION_PATTERN.h`, `#include` it in
`SNOBOL4c.c`, add a 5-line stdin loop. The seed kernel executes the parser as
pattern data. The language parses itself. See `doc/DECISIONS.md`.

**Decision 2 — What language does SNOBOL4-tiny implement first: DECIDED**
Expressions first, statements second. Sequence B → C → D confirmed:
- **B** (Sprints 0–4): single pattern, stdin/stdout, no naming — already underway
- **C** (Sprints 5–6): two named patterns with mutual recursion — the minimum
  for a real language, validates the graph IR, first thing no other pattern
  language can express
- **D** (Sprint 7+): full SNOBOL4 statement model — recognizable SNOBOL4,
  programs run unchanged on CSNOBOL4/SPITBOL

The question of whether Stage C deserves its own name is open.

---

## Repository Layout

```
doc/            Design notes, α/β/γ/ω paper, ByrdBox reference
src/
  codegen/      Template emitter (Python → ASM/JVM/MSIL)
  runtime/      Static runtime: str_t, output_t, enter/exit
  ir/           Node graph: named flat table, REF nodes
test/
  sprint0/      Null program
  sprint1/      Single token
  sprint2/      Two-token sequences
  sprint3/      Alternation
  sprint4/      Assignment / capture
snapshots/      Tagged checkpoint outputs per sprint
bench/          Benchmarks vs SPITBOL, CSNOBOL4
```

---

## Collaborators

- **Lon Jones Cherryholmes** ([@LCherryholmes](https://github.com/LCherryholmes)) —
  compiler architecture, x86-64 codegen, SNOBOL4-jvm author
- **Jeffrey Cooper, M.D.** ([@jcooper0](https://github.com/jcooper0)) —
  SNOBOL4-dotnet author, MSIL target

---

## Related Repos

| Repo | What |
|------|------|
| [SNOBOL4-jvm](https://github.com/SNOBOL4-plus/SNOBOL4-jvm) | Full SNOBOL4 → JVM bytecode (Clojure) |
| [SNOBOL4-dotnet](https://github.com/SNOBOL4-plus/SNOBOL4-dotnet) | Full SNOBOL4 → MSIL (C#) |
| [SNOBOL4-python](https://github.com/SNOBOL4-plus/SNOBOL4-python) | Pattern library (PyPI) |
| [SNOBOL4-corpus](https://github.com/SNOBOL4-plus/SNOBOL4-corpus) | Shared test corpus |

---

## Polyglot Parsing: Alt IS the Dispatcher

SNOBOL4-tiny's stdin loop matches each input line against a root `PATTERN`.
That root pattern is an **alternation** (`|`) of grammars:

```c
root = snoStmt | ednExpr | incStmt | ...
```

The backtracking engine dispatches between grammars automatically — no format
detection, no switch statement. Each new language is one more arm, one more
`*_PATTERN.h` file. This is not a feature; it falls out of `Alt` already existing.

**Target grammar arms:**

| Arm | Language | Source | Sprint |
|-----|----------|--------|--------|
| `snoStmt` | SNOBOL4 | `Beautiful.sno` serialized | 5 |
| `ednExpr` | EDN (Clojure data literals) | New `EDN_PATTERN.h` | 7 |
| `incStmt` | INC macro/include files | New `INC_PATTERN.h` | 8 |

**The architectural consequence:** SNOBOL4-tiny is not a SNOBOL4 compiler.
It is a **grammar-driven compiler compiler**. The input language is whatever
grammar is loaded into the root Alt at compile time.
