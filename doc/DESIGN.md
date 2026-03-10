# SNOBOL4-tiny — Design Notes

## The Byrd Box Model

The alpha/beta/gamma/omega protocol comes from Peter Byrd's 1980 box model for
Prolog execution, adapted here for SNOBOL4/Icon-style goal-directed evaluation.

Each compiled expression node owns exactly four labeled entry points:

- **alpha** — Enter fresh. Initialize node state. Attempt first solution.
- **beta**  — Resume. The downstream consumer failed; try next solution.
- **gamma** — Succeed. Wire to the next node's alpha (or match success).
- **omega** — Fail. Wire to the enclosing choice point's beta (or match failure).

Wiring rules:

```
Concatenation (P Q):
    P_gamma -> Q_alpha      (P succeeded, start Q)
    Q_omega -> P_beta       (Q failed, backtrack into P)

Alternation (P | Q):
    P_omega -> Q_alpha      (P failed, try Q)
    Q_omega -> outer_omega  (Q failed, propagate up)
    P_gamma -> outer_gamma
    Q_gamma -> outer_gamma

ARBNO(P):
    enter_alpha -> P_alpha  (try P once more)
    P_gamma -> enter_alpha  (P matched, loop back)
    P_omega -> outer_gamma  (P failed, exit with accumulated match)
    outer_beta -> P_beta    (downstream failed, undo last P)
```

## IR Design

The IR is a named flat table of nodes (a graph, not a tree) to support
recursive patterns like:

```snobol4
EXPR = TERM ('+' TERM)*
```

Cycles are handled via REF nodes. At parse time, a forward reference emits
REF("EXPR"). At codegen time, REF("EXPR") emits a jump to EXPR_alpha.

Node types:

| Node       | alpha behavior                  | beta behavior              |
|------------|---------------------------------|----------------------------|
| LIT(s)     | Match literal s at cursor       | Restore cursor, fail       |
| ANY(cs)    | Match one char in charset cs    | Restore cursor, fail       |
| SPAN(cs)   | Match 1+ chars in cs            | Backtrack one char at a time |
| BREAK(cs)  | Match 0+ chars not in cs        | Deterministic — fail       |
| LEN(n)     | Advance cursor by n             | Restore cursor, fail       |
| POS(n)     | Assert cursor == n              | Fail                       |
| RPOS(n)    | Assert cursor == len-n          | Fail                       |
| ARB        | Try 0 chars, then 1, 2, ...     | Advance one char, retry    |
| ARBNO(P)   | Try 0 repetitions, then 1, ...  | Undo last P                |
| ALT(P,Q)   | Try P                           | On P_omega, try Q          |
| CAT(P,Q)   | P then Q                        | On Q_omega, backtrack P    |
| ASSIGN(P,V)| Match P, assign V on gamma      | Pass beta through to P     |
| REF(name)  | jmp name_alpha                  | jmp name_beta              |

## Static Allocation

All working state for pattern nodes is allocated statically at compile time.
Each node instance gets a unique name prefix: lit7_, span3_, arb12_, etc.

For .bss (x86-64 NASM):

```nasm
section .bss
    lit7_saved_cursor:  resq 1
    span3_saved_cursor: resq 1
    span3_delta:        resq 1
    arb12_len:          resq 1
```

For recursive patterns, temporaries are arrays indexed by a depth counter:

```c
typedef struct { int64_t saved_cursor; } arbno5_t;
static arbno5_t arbno5_stack[64];
static int      arbno5_depth = 0;
```

CODE/EVAL dynamic patterns use heap allocation (two-tier: static fast path
for compiled patterns, heap for runtime-generated ones).

## Code Generation Strategy

Template expansion: each node type has a parameterized C (or ASM) template.

Template for LIT(s) in C-with-gotos:

```c
/* LIT("{s}") — node {id} */
{id}_alpha:
    if (cursor + {len} > subject_len) goto {id}_omega;
    if (memcmp(subject + cursor, "{s}", {len}) != 0) goto {id}_omega;
    {id}_saved_cursor = cursor;
    cursor += {len};
    goto {gamma};
{id}_beta:
    cursor = {id}_saved_cursor;
    goto {omega};
```

The emitter substitutes {s}, {len}, {id}, {gamma}, {omega}.
No AST walking at runtime. No indirect dispatch.

## Multi-Target IR

The same node graph drives all three backends:

- x86-64 ASM (NASM): jmp/je/jne, rsi = cursor, rdi = subject ptr
- JVM bytecode (ASM library): GOTO, IF_ICMPNE, locals for cursor/saved
- MSIL (ILGenerator): Br, Bne_Un, locals

## CODE / EVAL

CODE(s) and EVAL(s) are re-entrant calls into the same compiler pipeline.
No interpreter fallback. No mode switch.
For x86-64: compiled code lands in mmap(PROT_EXEC).
For JVM: ClassLoader.defineClass().
For MSIL: DynamicMethod.

---

## The Forth Analogy — Why This Architecture

SNOBOL4-tiny's architecture is deliberately modeled on the Forth kernel/dictionary
split. Forth's power comes from having an irreducibly small native kernel (~12
primitives) and then building everything else in the language itself. The same
discipline applies here.

### The NEXT Equivalent

In Forth, NEXT is the 3-instruction heartbeat of the entire system:

```asm
NEXT: lw  W, (IP)    ; fetch next word pointer
      add IP, 4      ; advance instruction pointer  
      jmp (W)        ; jump to code
```

SPITBOL uses an equivalent: `succp` (3 instructions: load pthen, load pcode,
jmp). Every pattern node pays this cost at runtime.

SNOBOL4-tiny pays **zero**. The α/β/γ/ω wiring is baked into the compiled
output as static gotos. There is no dispatch loop — the wiring IS the execution.
This is the fundamental speed advantage over SPITBOL's threaded model.

The price: the graph must be fully known at compile time. CODE/EVAL is the
escape hatch — it re-enters the compiler and extends the graph at runtime,
exactly as Forth's `:` extends the dictionary.

### Primitive Minimality

Before adding any new primitive node, ask: can it be expressed as CAT, ALT,
or ARBNO of existing primitives? If yes — don't add it. Write the derivation.

ARBNO is *derivable* from ARB + CAT + ALT once those work. TAB(n) is
derivable from POS(n). These are not primitives — they are library words,
written in SNOBOL4 the same way Forth's higher words are written in Forth.

Irreducible primitives (nothing smaller can express them):
LIT, ANY, POS, RPOS, LEN, SPAN, BREAK, ARB.

Derivable (write in SNOBOL4, not as C templates):
ARBNO, TAB, RTAB, and most compound patterns.

See `doc/BOOTSTRAP.md` for the full three-phase bootstrap strategy.

---

## Open Decisions

See `doc/DECISIONS.md` for the two foundational questions. Decision 2
(scope/sequencing) is now settled — see below.

## Key Design Decisions (Settled)

### Expressions first, statements second (decided 2026-03-10)

SNOBOL4-tiny implements pattern expressions before the SNOBOL4 statement
model. Stages:

- **Stage B** (Sprints 0–4): single pattern, stdin/stdout, no naming
- **Stage C** (Sprints 5–6): named patterns, mutual recursion, REF node —
  the minimum for a real language
- **Stage D** (Sprint 7+): SNOBOL4 statement model (variables, goto,
  INPUT/OUTPUT, END)

Rationale: expressions are the hard part. The statement model is a wrapper.
Getting the expression semantics — including mutual recursion — proven first
means every subsequent layer builds on a validated foundation.

Decision 1 (compiler implementation language) remains open — see
`doc/DECISIONS.md`.

---

## The Interpreter/Compiler Duality (key discovery)

`SNOBOL4c.c` is a complete SNOBOL4 pattern interpreter in C. Its match engine
uses four actions:

```c
#define PROCEED 0   // ↓  enter a node fresh
#define SUCCESS 1   // ↑  node succeeded, return up
#define FAILURE 2   // ↗  node failed, backtrack
#define RECEDE  3   // ↖  downstream failed, retry
```

These are **the same four states as α/β/γ/ω** — not by coincidence. Both
are implementations of the Byrd Box model. The difference is execution strategy:

| | SNOBOL4c.c interpreter | SNOBOL4-tiny compiler |
|---|---|---|
| Dispatch | `switch(type<<2\|action)` at runtime | Static `goto` labels baked into C output |
| State | `state_t Z` on heap | Named static variables per node |
| Backtrack stack | `Ω_push/Ω_pop` (heap-allocated tracks array) | Implicit in goto graph structure |
| Pattern data | `static const PATTERN` structs (`.h` files) | Inlined C-with-gotos (`.c` files) |
| Recursion | `psi` stack of saved states on heap | Static arrays with depth index |
| Speed | One `switch` dispatch per node per action | Zero dispatch — goto IS execution |

The `.h` files (`BEAD_PATTERN.h`, `C_PATTERN.h`, `CALC_PATTERN.h`, etc.) are
**compiled pattern data** — the output of `_bootstrap.c` (a Python C extension
that walks SNOBOL4python pattern objects and dumps them as C struct declarations).
They are `#include`d directly into `SNOBOL4c.c`.

The `test_sno_*.c` files are what a SNOBOL4-tiny **compiler** emits instead —
the same patterns, but as inlined C-with-gotos rather than interpreted structs.

### The two routes from here

**Route A — Interpreter + yacc front-end:**
Add a yacc/lex parser to `SNOBOL4c.c` that reads SNOBOL4 source and emits
`.h` files (static PATTERN struct declarations). The interpreter executes them.
Fastest path to a working system. Self-hosting deferred.

**Route B — Interpreter parses itself, emits compiled output:**
Write the parser as SNOBOL4 patterns inside `SNOBOL4c.c`. The parser matches
SNOBOL4 source text and emits C-with-gotos (the `test_sno_*.c` format).
The interpreter is the compiler's front-end. Self-hosting from day one.
This is the Forth move: the language parses itself.

**Plan: Route A now (Sprint 0–4), Route B as Sprint 5 (Phase 2 bootstrap).**
`SNOBOL4c.c` is the seed kernel — the "12 primitives in C" of our Forth analogy.
The interpreter is done. What remains is the parser.
