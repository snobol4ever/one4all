# SNOBOL4-tiny — Bootstrap Strategy

## The Forth Lesson

Every viable tiny Forth follows the same playbook:

1. **~12 primitives in native code** — the irreducible minimum: store, fetch,
   branch, arithmetic, I/O. Everything else is defined in terms of these.
2. **NEXT** — a single 3-instruction dispatch loop. The entire "interpreter"
   is: fetch the word pointer, advance IP, jump to it. That's it.
3. **The dictionary is self-extending** — once you have `:` and `;`, the
   system builds itself. You write the rest of Forth *in Forth*.

lbForth bootstraps from ~12 C primitives. milliFORTH does it in 11 words and
512 bytes. The discipline: **keep the seed kernel as small as possible, then
write everything else in the target language.** The smaller the seed, the more
the system proves itself.

---

## The Direct Analogy

| Forth concept | SNOBOL4-tiny equivalent |
|---|---|
| ~12 native primitives | ~8 primitive pattern nodes: LIT, ANY, POS, RPOS, LEN, SPAN, BREAK, ARB |
| NEXT (3-instruction inner loop) | The α/β/γ/ω wiring dispatcher |
| `: word ... ;` (colon definition) | `NAME = pattern` (SNOBOL4 pattern assignment) |
| Dictionary | Pattern table (named flat IR graph) |
| Self-extension (write Forth in Forth) | Write complex patterns using primitive patterns |
| Metacompiler | Compile SNOBOL4 patterns using emit_c.py driven *by* SNOBOL4python |

---

## Three-Phase Bootstrap

### Phase 1 — Seed Kernel (Sprints 0–4)

The 8 primitive nodes hardcoded as C templates in `emit_c.py`. This is the
"12 primitives in ASM" equivalent. The primitive set:

| Node | Irreducible? | Notes |
|------|-------------|-------|
| LIT(s) | Yes | Atomic — no smaller unit |
| ANY(cs) | Yes | Single-char charset test |
| POS(n) | Yes | Cursor assertion |
| RPOS(n) | Yes | Cursor assertion from end |
| LEN(n) | Yes | Fixed advance |
| SPAN(cs) | Yes | Greedy 1+ scan |
| BREAK(cs) | Yes | Scan-until primitive |
| ARB | Yes | Non-deterministic length |

**Key discipline (from Forth):** Never add a new primitive if it can be
expressed in terms of existing ones. ARBNO is *derivable* from ARB + CAT + ALT
once those work. Resist hardcoding it until the primitive set is proven complete.

### Phase 2 — Self-Hosting Emitter (Sprint 5+)

Write the code generator itself as a SNOBOL4 program, running on SNOBOL4-jvm
or SNOBOL4-python. The emitter reads a pattern description and emits C.

This is our `: ... ;` — SNOBOL4-tiny gains the ability to extend itself.
The emitter is just another SNOBOL4 pattern-matching program. It should be
written as one.

Target: `src/codegen/emit.sno` — a SNOBOL4 program that reads IR node
descriptions (one per line) and emits C-with-gotos. Runs on SNOBOL4-jvm
for validation, on SNOBOL4-python for speed.

### Phase 3 — Bootstrap Closure (Sprint 8+)

The emitter emits itself. A SNOBOL4-tiny compiled program that, when run,
produces output identical to running the same program on CSNOBOL4 or SPITBOL.

Test:
```bash
# Compile emit.sno with SNOBOL4-tiny compiler
./snobol4-tiny emit.sno > emit_compiled

# Run both versions on the same input
echo "LIT hello" | ./emit_compiled > tiny_output.txt
echo "LIT hello" | snobol4 emit.sno > oracle_output.txt

diff tiny_output.txt oracle_output.txt && echo "BOOTSTRAP CLOSED"
```

This is our Snocone Step 9 equivalent: the compiler compiles itself and
produces identical output to the reference oracle.

---

## The NEXT Equivalent

In Forth, NEXT is the 3-instruction heart of the entire system:

```asm
NEXT: lw  W, (IP)    ; fetch next word address
      add IP, 4      ; advance instruction pointer
      jmp (W)        ; jump to it
```

In SNOBOL4-tiny, the equivalent is the **wiring discipline** itself. The
"inner interpreter" is not a loop — it is the static goto graph baked into
the compiled output. There is no dispatch at runtime; the wiring IS the
execution. This is why SNOBOL4-tiny is faster than SPITBOL's threaded model:
SPITBOL still has a NEXT-equivalent (`succp`: 3 instructions per node).
We have zero.

The price: the graph must be fully known at compile time. CODE/EVAL is the
escape hatch — it re-enters the compiler and extends the graph at runtime,
exactly as Forth's `:` extends the dictionary.

---

## Primitive Minimality Test

Before adding any new primitive node to `emit_c.py`, ask:

1. Can it be expressed as CAT, ALT, or ARBNO of existing primitives?
2. If yes — don't add it. Write the derivation instead.
3. If no — add it, document why it's irreducible.

Examples:
- `TAB(n)` = `POS(n)` | advance cursor — *derivable*, not a primitive
- `RTAB(n)` = `RPOS(n)` equivalent — *derivable*
- `FENCE` = a commitment operator — *not derivable*, requires new node type
- `BAL` = balanced-parens scanner — *not derivable* from existing charset ops

---

## Validation Chain

At each phase, correctness is checked against the oracle chain:

```
SNOBOL4-tiny output
    == SNOBOL4-jvm output
    == SNOBOL4-dotnet output
    == CSNOBOL4 output
    == SPITBOL output
```

Three oracles must agree. If jvm and dotnet agree but SPITBOL disagrees,
use the majority. If all three disagree, flag for manual review.

---

## References

- Brad Rodriguez, *Moving Forth* (1994) — definitive guide to Forth kernel design
- larsbrinkhoff/lbForth — bootstraps from 12 C primitives to full self-hosting Forth
- milliFORTH (fuzzballcat) — 11 words, 512 bytes, complete Forth
- eForth (Bill Muench, C.H. Moore, 1990) — portable Forth from ~30 primitives
- Peter Byrd, *Coroutining Facilities in Prolog* (1980) — origin of the box model
