# SNOBOL4-tiny — Open Architecture Decisions

This file is where undecided questions are laid out, argued, and resolved.
Once a decision is made, the conclusion moves to DESIGN.md and the entry
here is marked **DECIDED**.

---

## Decision 1: What language do we write the compiler in?

### The question

The compiler has several distinct components. The language choice does not
have to be the same for all of them. The components are:

| Component | What it does |
|-----------|-------------|
| Parser | Reads SNOBOL4 source, builds AST |
| IR builder | Walks AST, constructs node graph |
| Code generator | Walks IR graph, emits C / ASM / JVM / MSIL |
| Runtime | Executes compiled programs (str_t, match loop, I/O) |
| Test harness | Drives sprints, diffs against oracles |

The runtime almost certainly must be C (or ASM for the inner loop) — it is
the substrate that compiled programs link against. That part is not in
question.

The question is what hosts the parser + IR builder + code generator.

### Candidates

#### A. Python (current)
`ir.py` and `emit_c.py` are already written in Python. The SNOBOL4python
library (our own, on PyPI) is available for pattern matching within the
compiler itself.

- **Pro:** Fast to write. Rich stdlib. ir.py already exists.
- **Pro:** SNOBOL4python lets us use SNOBOL4 patterns *inside* the compiler
  for parsing — dog-fooding the pattern library from day one.
- **Pro:** Shortest path to a working Sprint 2/3/4.
- **Con:** Python is not SNOBOL4. Phase 2 (self-hosting emitter) requires
  a rewrite anyway.
- **Con:** Adds a Python dependency to the build chain.

#### B. SNOBOL4 (on SNOBOL4-jvm or SNOBOL4-python)
Write the compiler as a SNOBOL4 program from the start. The compiler runs
on SNOBOL4-jvm (or CSNOBOL4 / SPITBOL as oracle). The target of compilation
is C-with-gotos.

- **Pro:** True self-hosting from the start — the compiler is a SNOBOL4
  program compiled by a SNOBOL4 engine. Phase 2 is already done.
- **Pro:** Every line of compiler code is a test of our own pattern semantics.
- **Pro:** Snocone (`src/codegen/emit.sno`) becomes the natural host.
- **Con:** Slower to bootstrap early sprints — we must write a SNOBOL4
  parser in SNOBOL4 before we can parse SNOBOL4.
- **Con:** Debugging is harder without Python's stack traces and repls.

#### C. C (the runtime language itself)
Write the compiler as a C program. The compiler reads source and writes
C-with-gotos output.

- **Pro:** Zero additional language dependencies.
- **Pro:** The compiler and runtime share one language — easy to merge them
  later into a single-binary tool.
- **Con:** String handling and pattern matching in C is painful. The whole
  point of SNOBOL4 is that *it* is the right language for this task.
- **Con:** Farthest from self-hosting.

#### D. Hybrid: Python now, SNOBOL4 later (the Forth metacompiler path)
Keep Python for Phases 1–2 (seed kernel + working compiler). Once the
compiler can compile non-trivial SNOBOL4, rewrite the compiler in SNOBOL4
and use it to compile itself. The Python version becomes the bootstrap
oracle — exactly how Forth metacompilers work (Lisp bootstraps Forth,
then Forth replaces the Lisp).

- **Pro:** Best of both worlds. Fast iteration now, self-hosting later.
- **Pro:** This is exactly the lbForth strategy: Lisp metacompiler →
  working kernel → Forth metacompiler replaces the Lisp.
- **Pro:** The Python compiler becomes a permanent validation tool — run
  both compilers on the same input and diff outputs.
- **Con:** Two compilers to maintain during transition.

### Current thinking

Option D (hybrid) matches our sprint plan most naturally. Python drives
Sprints 0–4. The SNOBOL4 emitter (`emit.sno`) is the Sprint 5 deliverable.
After Sprint 5, the Python compiler is frozen as oracle and all new
development happens in SNOBOL4.

**Status: UNDECIDED — needs explicit sign-off before Sprint 5.**

---

## Decision 2: What language does SNOBOL4-tiny implement first?

### The question

Do we implement full SNOBOL4 from the start, or do we first implement a
smaller, more tightly defined language — and if so, how small?

This breaks into a spectrum:

### Option A: Full SNOBOL4 from the start
Implement the complete SNOBOL4 language: statements, goto-driven control
flow, all primitives, DATA(), DEFINE(), CODE/EVAL, arithmetic, I/O.

- **Pro:** The test corpus (SNOBOL4-corpus, Gimpel library) applies
  immediately. Every sprint result is directly comparable to SPITBOL.
- **Con:** The full language is large. Too many things to get right before
  the compiler produces a single working program.

### Option B: A minimal pattern-only sublanguage first

The smallest useful SNOBOL4-tiny program is a single pattern match against
a single input string, producing output. No statements. No control flow.
No variables except OUTPUT.

Concretely: stdin → one pattern match → stdout.

```
POS(0) SPAN('0123456789') $ OUTPUT RPOS(0)
```

This is one pattern, one subject (stdin), one action (write to OUTPUT).
It is complete goal-directed evaluation. It is not SNOBOL4's full statement
model, but it is SNOBOL4's *heart*.

- **Pro:** We already have this in sprint0/sprint1. The path is direct.
- **Pro:** Every primitive is immediately testable against SPITBOL/CSNOBOL4.
- **Con:** Not yet a language — just a pattern engine. No way to name
  subpatterns or chain statements.

### Option C: Two-pattern minimum with mutual recursion (the real minimum)

One pattern alone cannot express recursion. Two patterns that reference each
other can. This is the minimum for a *language* rather than a pattern engine:

```
* Two patterns, mutual recursion, one is "main"
WORD   = SPAN(LETTERS)
MAIN   = POS(0) ARBNO(WORD ' ') RPOS(0)
```

More precisely, the minimum viable language has:
1. **Named pattern definitions** — `NAME = pattern-expression`
2. **Pattern references** — `*NAME` (deferred evaluation, avoids infinite
   recursion at definition time)
3. **One designated entry point** — `MAIN` or the last-defined pattern
4. **One input source** — stdin (the subject string)
5. **One output mechanism** — `$ OUTPUT` capture-and-print

No statements. No goto. No arithmetic. No DATA(). No DEFINE(). No CODE().
Just: define patterns, reference patterns, match stdin, print captures.

This is already Turing-complete for string recognition (it can express any
context-free grammar). It is a clean, small, self-contained language.

- **Pro:** The REF node (already in ir.py) is the only new mechanism needed
  beyond what Sprints 0–4 establish.
- **Pro:** Mutual recursion is the first test that the IR graph (not tree)
  design is correct. This is the right moment to validate it.
- **Pro:** This is genuinely a different, smaller language than SNOBOL4.
  It could have its own name. (SNOBOL4-tiny is the compiler; the language
  it implements at this stage could be called something else.)
- **Con:** Still not SNOBOL4. Users cannot run existing SNOBOL4 programs.

### Option D: Minimal SNOBOL4 statement model

Add the statement model on top of Option C: a subject string, a pattern,
optional replacement, optional goto. This makes it recognizable SNOBOL4:

```snobol4
LINE = INPUT
LINE  SPAN(LETTERS) $ OUTPUT
```

The minimum additions over Option C:
- Variable assignment (`VAR = expr`)
- The pattern-match statement (`subject  pattern`)
- Unconditional goto (`:(LABEL)`)
- Success/failure branches (`:S(LABEL) :F(LABEL)`)
- `INPUT` / `OUTPUT` special variables
- `END` statement

This is the SNOBOL4 statement model stripped of everything else. It can
run a meaningful subset of real SNOBOL4 programs.

- **Pro:** Now it is SNOBOL4. Programs written for this subset run on
  CSNOBOL4 and SPITBOL unchanged.
- **Con:** Significantly more to implement than Option C.

### Current thinking

The right sequence is **B → C → D**, not a choice between them.

- Sprints 0–4: Option B (single pattern, stdin/stdout, no naming)
- Sprint 5–6: Option C (two patterns, mutual recursion, named definitions,
  REF node validated)
- Sprint 7+: Option D (full statement model, recognizable SNOBOL4)

The key insight is that Option C is where the language becomes *interesting*:
mutual recursion is the first thing that cannot be expressed in any other
pattern language. It validates the graph IR. It is the moment SNOBOL4-tiny
becomes more than a fancy regex engine.

Option C also answers the naming question: the language at stage C could
legitimately be called **SNOBOL4-tiny** — a real language with named
patterns and mutual recursion, just without the statement model. Stage D
graduates it to a SNOBOL4 subset.

**Status: DECIDED — 2026-03-10**

**Decision: Expressions first, statements second. The B→C→D sequence is confirmed.**

Rationale: Pattern expressions are the heart of SNOBOL4. The statement model
(subject, replacement, goto) is a wrapper around expression evaluation. Getting
expressions right first — including mutual recursion — means the hard part is
proven before the statement model is layered on top. Every sprint through Stage C
is directly validatable against CSNOBOL4/SPITBOL pattern semantics without the
additional complexity of the statement interpreter.

The naming question (does Stage C deserve its own name?) remains open.

---

## Standing Rule

When a decision is made, update this file (mark DECIDED, record the choice
and rationale), then copy the conclusion to DESIGN.md.
Push both files in the same commit.
