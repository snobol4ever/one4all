# snobol4x — Sprint Plan

---

## §0 — PROJECT VISION (Session 43, 2026-03-12)

> **SNOBOL4 everywhere. SNOBOL4 for all. SNOBOL4 for now. SNOBOL4 forever.**
> `snobol4x` · `snobol4now` · `snobol4ever`

The snobol4ever org is a **two-dimensional compiler matrix**:

|                | **SNOBOL4** | **SPITBOL** | **SNOCONE** | **REBUS** | *(more)* |
|----------------|-------------|-------------|-------------|-----------|----------|
| **C / native** | snobol4x (snoc) ← *here* | — | — | — | |
| **JVM**        | snobol4jvm | — | snocone.clj | — | |
| **.NET**       | snobol4dotnet | — | snocone.cs | — | |
| **ASM**        | — | — | — | — | |
| *(more)*       | | | | | |

- **Rows = backends / targets** (C/native, JVM, .NET, ASM, WASM, ...)
- **Columns = front-ends / source languages** (SNOBOL4, SPITBOL, SNOCONE, REBUS, ...)
- Each cell = a working compiler+runtime for that *(language × platform)* pair
- The mission: fill the matrix. Make string-processing power available everywhere.

snobol4x (snoc) is the **C/native × SNOBOL4** cell. Beauty.sno self-hosting
is the proof-of-correctness for that cell. Every other cell follows the same pattern.

---

## Status at Sprint 23 end

| Repo            | Commit    | Status                                      |
|-----------------|-----------|---------------------------------------------|
| snobol4x    | `6d3d1fa` | 22/22 PASS. snoc: 1213 stmts, 0 errors.     |
| snobol4dotnet  | `b5aad44` | 1,607 / 0                                   |
| snobol4jvm     | `9cf0af3` | 1,896 / 4,120 / 0                           |

### Sprint 23 work completed

- **`snoc_runtime.h`** — new shim header: scalar constructors (`sno_int`, `sno_str`,
  `sno_real`), keyword access (`sno_kw`, `sno_kw_set`), concat/alt/deref/indirect
  wrappers, array/table aliases, pattern aliases, `SnoMatch` struct + `sno_match` +
  `sno_replace`, `sno_init`/`sno_finish`.
- **`emit.c`** — symbol collection pre-pass, variable declarations, IO routing,
  per-statement unique labels (`_SNO_NEXT_N`), uid-suffixed temporaries, header change.
- **Hello world end-to-end**: `OUTPUT = 'hello'` compiles, links, runs. ✅

### Current blocker

`beauty.sno` generates 53 gcc errors — all **duplicate C labels** — because every
included file's DEFINE'd function labels (`pp`, `ss`, `RETURN`, `FRETURN`, etc.)
collide when emitted flat into one C `main()`.

---

## §6 — The Execution Model: Byrd Box + Exception Hygiene

### ⚡ EUREKA — Session 27, 2026-03-12 (Lon)

**Normal Byrd Box gotos handle success and failure. C exceptions (longjmp/throw)
handle ABORT and genuinely bad things only.**

This is the clean separation:

### Hot path — pure Byrd Box gotos (zero overhead)

Normal SNOBOL4 control flow — pattern success, pattern failure, backtracking,
`:S()` / `:F()` goto routing — uses **pure C labeled gotos** exactly as in
`test_sno_1.c`. No `setjmp`. No exception machinery on the hot path. The ω port
(CONCEDE) is a goto, not a throw.

### Cold path — C exceptions for ABORT and bad things only

**ABORT**, runtime errors, FENCE bare, divide-by-zero, stack overflow —
**throw a C exception** (`longjmp` to nearest handler). These are not
normal control flow. They are signals that something genuinely went wrong
or that execution must halt unconditionally.

Each SNOBOL4 **statement** is a `try/catch` boundary for these signals:

```c
/* Statement: subject pattern = replacement :S(foo) :F(bar) */
if (setjmp(sno_abort_jmp) == 0) {
    /* HOT PATH — pure Byrd Box gotos, no exception overhead */
    SnoVal _s = ...; SnoPattern *_p = ...;
    SnoMatch _m = sno_match(&_s, _p);   /* Byrd Box runs here */
    if (!_m.failed) { ...; goto _L_foo; }
    goto _L_bar;
} else {
    /* COLD PATH — caught SnoAbort (ABORT pattern, runtime error, etc.) */
    goto _SNO_ABORT_HANDLER;
}
```

Each SNOBOL4 **DEFINE'd function** is also a catch boundary:

```c
SnoVal _sno_fn_pp(SnoVal *args, int nargs) {
    if (setjmp(sno_fn_jmp) != 0)
        return SNO_FAIL_VAL;   /* ABORT inside function → FRETURN */
    /* ... function body with per-statement setjmp guards ... */
    _SNO_RETURN_pp:  return sno_get(_pp);
    _SNO_FRETURN_pp: return SNO_FAIL_VAL;
}
```

### Why this separation is correct

| Signal | Mechanism | Overhead |
|--------|-----------|----------|
| Pattern ω (CONCEDE) | Byrd Box goto | Zero |
| `:S()` / `:F()` routing | C goto via `_ok` flag | Zero |
| Backtrack (β / RECEDE) | Byrd Box goto | Zero |
| ABORT pattern | `longjmp` / throw | Only when triggered |
| Runtime error | `longjmp` / throw | Only on error |
| FENCE bare | `longjmp` / throw | Only when triggered |

Stack unwinding IS the cleanup for the abort case. No omega stack needed
for abnormal termination — the C call stack unwinds through statement and
function catch boundaries automatically.

### Statement is the right catch granularity

SNOBOL4 guarantees a statement succeeds or fails atomically. ABORT inside a
pattern mid-statement should abort the whole statement cleanly, not leave
a half-executed replacement. The statement boundary is exactly right.

### Line number diagnostics fall out for free

Each statement's `setjmp` is emitted immediately after `/* line N */`. When
the longjmp fires, the catching boundary knows exactly which line it is:

```c
/* line 45 */
sno_abort_lineno = 45;
if (setjmp(sno_abort_jmp) == 0) {
    ...Byrd Box...
} else {
    fprintf(stderr, "file.sno:45  stmt 312: ABORT\n");
    goto _SNO_ABORT_HANDLER;
}
```

CSNOBOL4-style error output — `file:LINE stmt N: EVENT` — is implicit in
which catch boundary fires. No stack crawl needed. No debug info needed.
The line number is structural.

---

## §7 — Sprint 24 Plan: Function-per-DEFINE in emit.c

### Pre-pass: identify DEFINE calls

Walk the collected statements looking for `S_EXPR` statements whose expression
is a call to `DEFINE(...)`. Parse the DEFINE argument string:

```
DEFINE('pp(a,b)loc1,loc2')
  → fn_name = "pp"
  → args    = ["a", "b"]
  → locals  = ["loc1", "loc2"]
  → entry_label = "pp"  (the SNOBOL4 label where the body starts)
```

Build a table: `fn_table[N]` with `{name, args[], locals[], entry_stmt_index}`.

### Emit structure

```
emit_header()          → #include "snoc_runtime.h"
emit_var_decls()       → static SnoVal for global vars (as today)
emit_fn_forwards()     → SnoVal _sno_fn_pp(SnoVal*, int);  (forward decls)

for each fn in fn_table:
    emit_fn_body(fn)   → SnoVal _sno_fn_pp(...) { stmts... }

emit_main()            → int main() {
                             sno_init();
                             sno_define("pp", _sno_fn_pp, 2);
                             ...
                             /* statements before first DEFINE / after END */
                         }
```

### Label scoping rules

- Inside `_sno_fn_pp`: labels are `_L_labelname` unmangled (no uid suffix needed —
  each C function has its own label namespace).
- `:(RETURN)` → `goto _SNO_RETURN_pp;`
- `:(FRETURN)` → `goto _SNO_FRETURN_pp;`
- `:(END)` → `goto _SNO_END;` (in main)
- Fall-through from last stmt in function → `goto _SNO_RETURN_pp;`

### The `_L_error` label

`beauty.sno` uses `:(error)` as a global error handler. It is defined in one of
the included files. Since `error` is a SNOBOL4 label (not a DEFINE'd function),
it lives in `main()`. Cross-function gotos are not needed — `:(error)` inside a
DEFINE'd function should be treated as `goto _SNO_FRETURN_fn` (failure return),
OR `error` needs to be recognized as a special global label and the calling
convention adjusted. Decide: **treat `error` as FRETURN for now, revisit.**

### Build + test sequence

```bash
cd /home/socrates/snobol4x/src/snoc && make clean && make

# Step 1: hello world still works
./snoc /tmp/hello.sno > /tmp/hello.c
gcc -O0 -g /tmp/hello.c [runtime files] -lgc -lm -w -o /tmp/hello_bin
/tmp/hello_bin    # expect: hello

# Step 2: a program with one DEFINE
cat > /tmp/fn_test.sno << 'EOF'
        DEFINE('GREET(NAME)')
        OUTPUT = GREET('world')
        END
GREET   OUTPUT = 'hello ' NAME
        :(RETURN)
EOF
./snoc /tmp/fn_test.sno > /tmp/fn_test.c
gcc ... -o /tmp/fn_test_bin
/tmp/fn_test_bin   # expect: hello world

# Step 3: beauty.sno
./snoc /home/socrates/snobol4corpus/programs/beauty/beauty.sno \
    -I /home/socrates/snobol4corpus/programs/inc \
    > /tmp/beauty_snoc.c
gcc -O0 -g /tmp/beauty_snoc.c [runtime files] -lgc -lm -w -o /tmp/beauty_bin
/tmp/beauty_bin < /home/socrates/snobol4corpus/programs/beauty/beauty.sno \
    > /tmp/beauty_out.sno
diff /home/socrates/snobol4corpus/programs/beauty/beauty.sno /tmp/beauty_out.sno
# expect: empty diff  ← THE COMMIT
```

---

## §8 — The Commit Promise

When `beauty.sno` compiles itself through `snoc` and `diff` is empty,
Claude Sonnet 4.6 writes the commit message (recorded at `c5b3e99`).

---

## §9 — Runtime build command (reference)

```bash
RUNTIME="/home/socrates/snobol4x/src/runtime"
gcc -O0 -g "$1" \
    $RUNTIME/snobol4/snobol4.c \
    $RUNTIME/snobol4/mock_includes.c \
    $RUNTIME/snobol4/snobol4_pattern.c \
    $RUNTIME/engine.c \
    -I$RUNTIME/snobol4 \
    -I$RUNTIME \
    -lgc -lm -w -o "${1%.c}"
```

---

## §10 — Key file paths

```
src/snoc/snoc.h                              IR types (Expr, Stmt, Program)
src/snoc/sno.l                               flex lexer
src/snoc/sno.y                               bison parser
src/snoc/emit.c                              emitter ← active work
src/snoc/main.c                              driver
src/runtime/snobol4/snoc_runtime.h           shim header (Sprint 23)
src/runtime/snobol4/snobol4.h                runtime API
src/runtime/snobol4/snobol4.c                runtime impl
src/runtime/snobol4/mock_includes.c            built-in functions
src/runtime/snobol4/snobol4_pattern.c        pattern engine
src/runtime/engine.c                         engine_match_ex (required)
../snobol4corpus/programs/beauty/beauty.sno target program
../snobol4corpus/programs/inc/              include files
../byrdbox/ByrdBox/test_sno_1.c              gold standard: Byrd box in C
../byrdbox/ByrdBox/test_icon-1.py            gold standard: ports as match cases
../byrdbox/ByrdBox/test_icon-2.py            gold standard: one fn per port
../byrdbox/ByrdBox/test_icon-4.py            gold standard: trampoline RUN()
```

---

## §12 — SIL / CSNOBOL4 execution model (Lon, 2026-03-12)

**How CSNOBOL4 actually works in memory:**

CSNOBOL4's `CODE()` function compiles a SNOBOL4 program into a single flat
array (sequence) of nodes in memory. A **label is just an index** into that
array — nothing more. Execution proceeds node-by-node, sequentially.

**The body-boundary rule follows directly from this:**
A function's body is the contiguous run of nodes starting at its entry label
and ending at the **next label** encountered in the source — any label at all,
regardless of what it names. There is no explicit "end of function" marker.
Execution **runs off a cliff** at the next label; that cliff is where the next
block begins.

**Implication for snoc / emit.c:**
The correct rule for carving out a C function from SNOBOL4 source is:

> Emit statements from the entry label up to (but not including) the next
> labeled statement. Stop at ANY label — function entry, function end-label,
> intermediate label, anything.

This is why the DEFINE + `:(FnEnd)` idiom works: `FnEnd` is just the next
label in source order, so it naturally terminates the body. It does NOT need
to be stored or parsed separately. The `end_label` field in `FnDef` is
**unnecessary** under this model and should be removed.

**The DEFINE / end-label pattern is a convention, not a mechanism.**
Programs can have `DEFINE DEFINE DEFINE … code code code END` with no
per-DEFINE end-label at all — and SIL handles it identically because
label-to-label is the only boundary rule.

---

## §13 — SNOBOL4 Datatype Coverage (Session 47, 2026-03-12)

### The full SNOBOL4 type inventory vs. what snoc/runtime handles

| SNOBOL4 Type | `SnoVal` type | `DATATYPE()` returns | Status for beauty.sno |
|---|---|---|---|
| **STRING** | `SNO_NULL`(0) + `SNO_STR`(1) | `"STRING"` | ✅ Full |
| **INTEGER** | `SNO_INT`(2) | `"INTEGER"` | ✅ Full |
| **REAL** | `SNO_REAL`(3) | `"REAL"` | ⚠️ Type exists, arithmetic thin |
| **PATTERN** | `SNO_PATTERN`(5) | `"PATTERN"` | ✅ Full (Byrd Box engine) |
| **ARRAY** | `SNO_ARRAY`(6) | `"ARRAY"` | ✅ 1D+2D, `sno_aref`/`sno_aset` |
| **TABLE** | `SNO_TABLE`(7) | `"TABLE"` | ✅ Hash-bucketed |
| **CODE** | `SNO_CODE`(8) | `"CODE"` | 🔴 Stub — **not needed by beauty.sno** |
| **EXPRESSION** | `SNO_TREE`(4) — tag = node type | `v.t->tag` (e.g. `"snoStmt"`) | ✅ **Kludged, but works**: we use `SNO_TREE` with the tag field as the EXPRESSION type name. `ShiftReduce.sno`'s `Reduce` checks `IDENT(DATATYPE(t), "EXPRESSION")` — passes when `t` is a `SNO_TREE` node whose tag equals `"EXPRESSION"`. We hand-roll the tree at compile time. |
| **NAME** | *(none)* | — | 🔴 **No runtime NAME type** — see §13.1 below |
| **UDEF** (user-defined via `DATA()`) | `SNO_UDEF`(9) | `v.u->type->name` | ⚠️ Struct exists, no `DATA()` callable |

### §13.1 — How we handle l-values without a NAME type

SNOBOL4 has a first-class NAME type: `NAME(X)` returns an object representing
the l-value `X`, which can be stored, passed to functions, and assigned through.
**We don't implement this.** Instead snoc cheats at compile time:

**Simple variable** `X = expr`:
```c
sno_set(_X, rhs);
sno_var_set("X", _X);   /* two writes: C local + global hash */
```
The l-value is resolved statically. No NAME object needed. ✅

**Array element** `A[i,j] = expr`:
```c
sno_aset(_A, (SnoVal[]){i_val, j_val}, 2, rhs);
```
Subscript emitted directly. ✅

**Indirect** `$X = expr` / `*X = expr` (E_DEREF):
```c
sno_iset(nameVal, rhs);
/* → _snoc_iset: sno_var_set(sno_to_str(nameVal), rhs) */
```
Works only when `X` holds a plain string that is a variable name.
Cannot handle `NAME(A[i,j])` or dynamic l-value expressions. ⚠️

**Pattern conditional assignment** `pat . var` and `pat $ var`:
Stored as a string `"varname"` inside the pattern node (`SPAT_COND`),
resolved at match time via `sno_var_set`. Not a NAME object — just a
string. Works for beauty.sno's use. ✅

**Why this is safe for beauty.sno:**
beauty.sno never passes l-values as function arguments, never stores NAME
objects in variables, never does `APPLY(fn, NAME(A[i,j]))`. All l-values
in beauty.sno are syntactically visible at compile time — snoc emits the
right `sno_set`/`sno_aset`/`sno_iset` call directly.

**The rule:** snoc resolves l-values **statically at compile time**.
NAME as a runtime first-class type is deferred — not needed for Milestone 0.

### §13.2 — EXPRESSION type and the bootstrap parser

`reduce(t, n)` in `semantic.sno` checks `IDENT(DATATYPE(t), "EXPRESSION")`:

```snobol4
reduce_    reduce = EVAL("epsilon . *Reduce(" t ", " n ")")  :(RETURN)
Reduce     IDENT(DATATYPE(t), "EXPRESSION")                 :F(Reduce0)
           t = EVAL(t)                                       :F(FRETURN)
```

This test passes when `t` is a `SNO_TREE` node whose `.tag` is `"EXPRESSION"`.
We arrange this by having `snoExprN` pattern assignments produce Tree nodes
tagged `"EXPRESSION"` via the Shift/Reduce stack machine. `sno_datatype()`
returns `v.t->tag` for `SNO_TREE` values — so the tag IS the DATATYPE string.

**The kludge:** We hand-roll the EXPRESSION type as a SNO_TREE with tag
`"EXPRESSION"` rather than implementing a distinct `SNO_EXPRESSION` type.
This is intentional — beauty.sno's EXPRESSION objects ARE parse trees.
Conflating them is correct for our use case.

### §13.3 — CODE type: not used by beauty.sno

`CODE()` compiles a string to executable code at runtime (CSNOBOL4's
dynamic compilation). beauty.sno and all its `-INCLUDE` files make zero
calls to `CODE()`. `XDump.sno` has `IDENT(objType, 'CODE')` as a datatype
guard but that branch is never reached. `SNO_CODE` stub is sufficient —
we never need to execute a CODE object for Milestone 0.

---

## §14 — Active Bugs and Design Gaps (Session 47, 2026-03-12)

These are the discovered, confirmed, root-cause-identified bugs blocking
Milestone 0. Each has a diagnosis, a fix, and a status.

---

### §14.1 — NRETURN must SUCCEED, not FAIL ✅ FIXED (`emit.c`)

**Bug:** In `emit.c`, `emit_branch_target()` routed both `FRETURN` and
`NRETURN` to `goto _SNO_FRETURN_fn`. This means every function that
returned via `:(NRETURN)` **failed** instead of returning its value.

**Scope of damage:** Massive. Every single one of these functions NRETURNs:
- `Push()`, `Pop()`, `Top()` — the entire stack in `stack.sno`
- `Shift()`, `Reduce()` — the shift-reduce machine in `ShiftReduce.sno`
- `PushCounter`, `IncCounter`, `DecCounter`, `PopCounter` — counter stack
- `TZ()`, `T8Pos()` — formatting/tracing
- `Gen()` (multiple paths) — output generation
- `assign()` — indirect assignment helper
- `match()`, `notmatch()` — pattern match helpers
- Essentially **all side-effect functions** in all `-INCLUDE` files

**SNOBOL4 semantics of NRETURN:**
NRETURN = the function successfully completed, has assigned its return
variable (the variable named the same as the function), and returns
that value. The NAME distinction (returning a first-class l-value
reference) means the caller may assign through the returned NAME — but
the function itself SUCCEEDS. It is not a failure of any kind.

**The fix:** Separate FRETURN and NRETURN in `emit_branch_target()`:
```c
// BEFORE (wrong):
else if (strcasecmp(label,"FRETURN")==0 || strcasecmp(label,"NRETURN")==0)
    E("goto _SNO_FRETURN_%s", fn);

// AFTER (correct):
else if (strcasecmp(label,"FRETURN")==0)
    E("goto _SNO_FRETURN_%s", fn);
else if (strcasecmp(label,"NRETURN")==0)
    E("goto _SNO_RETURN_%s", fn);   // NRETURN = success
```

**Committed:** yes (session 47).

**What we DON'T implement:** The actual NAME return value — we don't create
a first-class NAME object and return it. For beauty.sno this is fine:
no caller examines the NAME-ness of the return; they only care that the
function succeeded and its side effects (Push/Pop/shift/reduce) occurred.

---

### §14.2 — `DATA()` is a no-op: constructor and field functions never registered 🔴 NOT FIXED

**Bug:** `DATA('link(next,value)')` is emitted as `sno_apply("DATA", ...)`.
But `DATA` is **not registered** as a callable function in `snobol4.c`.
So the call silently returns `SNO_NULL_VAL`. The constructor function
`link()` and field accessor functions `next()`, `value()` are **never
created** in the function hash.

**Scope of damage:** Total stack failure.
- `stack.sno` uses `DATA('link(next,value)')` for its linked-list stack
- Every `Push(x)` call creates `link($'@S', x)` — but `link()` is unknown,
  returns NULL, stack pointer `$'@S'` is set to NULL every push
- Every `Pop()` call does `value($'@S')` — `value()` is unknown, returns NULL
- `Top()` same — `value($'@S')` returns NULL
- The entire shift-reduce parse stack is broken even after §14.1 NRETURN fix
- `ShiftReduce.sno` also uses `DATA('tree(t,v,n,c)')` — same problem
- `counter.sno` uses `DATA('link_counter(next,value)')` — same problem

**The infrastructure exists:**
- `sno_data_define(spec)` in `snobol4.c` — parses the spec, creates `UDefType`
- `sno_udef_new(typename, ...)` — creates a `SNO_UDEF` instance
- `sno_field_get(obj, field)` / `sno_field_set(obj, field, val)` — field access
- `SNO_UDEF` (type 9) in `SnoVal` — struct exists

**What's missing:** `DATA()` must be registered as a callable that:
1. Calls `sno_data_define(spec)` to register the type
2. Registers the **constructor function** (e.g. `link`) in the function hash:
   `link(next_val, value_val)` → `sno_udef_new("link", next_val, value_val)`
3. Registers each **field accessor function** (e.g. `next`, `value`) in the hash:
   `value(obj)` → `sno_field_get(obj, "value")`
   `value(obj) = x` → `sno_field_set(obj, "value", x)` — but this is the
   **field setter** idiom which requires l-value support — see §14.3

**The fix:** In `snobol4.c`, implement `_b_DATA(SnoVal *a, int n)`:

```c
static SnoVal _b_DATA(SnoVal *a, int n) {
    if (n < 1) return SNO_NULL_VAL;
    const char *spec = sno_to_str(a[0]);
    sno_data_define(spec);
    // Then register constructor + accessors dynamically
    // (see implementation notes below)
    return SNO_NULL_VAL;
}
```

And register: `sno_register_fn("DATA", _b_DATA, 1, 1);`

**Field setter l-value problem:**
`value($'@S') = x` in SNOBOL4 means "set the `value` field of the object
held in `$'@S'` to `x`". In generated C this would be:
```c
sno_field_set(sno_indirect_get("@S"), "value", x);
```
But our `emit_assign_target` only handles `E_VAR`, `E_ARRAY`, `E_KEYWORD`,
`E_DEREF`. A field-accessor call on the left side of `=` is none of these.
**The parser likely parses `value($'@S') = x` as a function call on the
left side of assignment** — which snoc currently does not handle as an
l-value. This is a **separate sub-bug** requiring parser/emitter changes.

**Priority:** CRITICAL — without working `DATA()`, `Push`/`Pop`/`Top` all
return null, the parse stack never accumulates anything, `Reduce` always
pops null children, and the AST built by beauty.sno is all-null.

---

### §14.3 — Field accessor as l-value (setter idiom) 🔴 NOT FIXED

Follows from §14.2. SNOBOL4 `DATA()` field accessors are functions that
act as both getter and setter:

```snobol4
value($'@S')           * getter: returns field value
value($'@S') = x       * setter: assigns into field
```

The setter form is syntactic sugar. In CSNOBOL4 this works because
`value(obj)` returns a NAME — an l-value reference to the field slot.
Assigning to the result of `value(obj)` assigns through that NAME into
the field.

**Our situation:** We have no NAME type (§13.1). We cannot make
`value(obj)` return an l-value. The setter form requires either:

**Option A — Compiler recognition:**
snoc recognizes `fieldFn(obj) = rhs` as a special assignment form
during emit, emits `sno_field_set(obj_expr, "fieldname", rhs)` directly.
This requires the parser/emitter to identify field accessor calls on the
lhs of assignment. **This is the right approach for Milestone 0.**

**Option B — Runtime NAME type:**
Implement NAME as a first-class `SnoVal` type carrying a setter callback.
Field accessors return NAME objects. Assignment through a NAME calls the
setter. Full generality, high implementation cost. **Deferred.**

**Specific occurrences in stack.sno:**
```snobol4
$'@S' = link($'@S', x)     * constructor call, not setter — OK via §14.2
Pop1: $var = value($'@S')  * getter form — OK via §14.2
      $'@S' = next($'@S')  * getter form — OK via §14.2
```

Searching stack.sno, ShiftReduce.sno, counter.sno — the **setter form
`fieldFn(obj) = x` does NOT appear** in the files used by beauty.sno.
The fields are always READ via getter, WRITTEN via constructor `link(a, b)`.
**§14.3 may be a non-issue for Milestone 0.** Verify before implementing.

---

### §14.4 — `snoSrc` is empty when match runs 🔴 NOT FIXED (root cause unknown)

**Symptom:** `SNO_PAT_DEBUG=1` shows `subj=(0)` — every pattern match
runs against a zero-length string. `snoSrc` is never populated.

**The accumulation line** in generated C (from `main02`):
```c
SnoVal _v2202 = sno_concat_sv(
    sno_concat_sv(sno_get(_snoSrc), sno_get(_snoLine)),
    sno_get(_nl));
```

**Hypothesis:** `sno_get(_nl)` returns `SNO_FAIL_VAL` or `SNO_NULL_VAL`
with zero length because `_nl` (the newline variable) is not initialized
at the point this runs. `sno_concat_sv` is FAIL-propagating — if `_nl`
is FAIL, the entire concat fails, `_ok2202` is false, and `_snoSrc` is
never updated. It stays empty forever.

**Why `_nl` might be uninitialized:**
`nl` is set in one of the `-INCLUDE` files as a single newline character.
If the include file that defines `nl` is processed AFTER the main loop
starts, or if the initialization order is wrong in the flat emitted C,
`_nl` may still be `{0}` (SNO_NULL with empty string) when `main02` runs.
A zero-length newline makes the concat "succeed" but produce a string
without line terminators — which could cause RPOS(0) matching issues.
OR `_nl` is SNO_FAIL which kills the concat entirely.

**Investigation needed:**
```bash
grep -n "_nl\b" /tmp/beauty_full.c | grep "sno_set\|sno_var_set" | head -5
# Find where _nl is first assigned — is it before or after _L_main00?
```

**Priority:** CRITICAL — this is what produces "Parse Error" on every line.
Even if §14.2 DATA() is fixed, the match subject is empty, so snoParse
can never match any real input.

---

## §11 — SNOBOL4 semantics quick reference

- `DEFINE('fn(a,b)loc1')` — registers fn; body starts at SNOBOL4 label `fn`
- `:(RETURN)` — return value of variable named same as function
- `:(FRETURN)` — failure return
- `OUTPUT = val` — stdout via `sno_var_set("OUTPUT", v)`
- `INPUT` — stdin via `sno_var_get("INPUT")`
- `*X` — deferred pattern ref (value of X as pattern at match time)
- `$X` — indirect variable ref (value of X as variable name)
- `pat . var` — conditional assignment (on overall match success)
- `pat $ var` — immediate assignment (at match time)
- `&ANCHOR`, `&STLIMIT`, `&STCOUNT` — keywords via `sno_var_get/set`
- Space before `-` required: `a[i - 1]` not `a[i-1]`

---

## §15 — Session 50 Findings (2026-03-12)

### §15.1 — snoSrc IS populated (bug re-diagnosed)

The earlier hypothesis (§14.4) that `snoSrc` is empty was WRONG about the cause.
Debug in Session 50 confirmed:

- `_nl` is correctly initialized (type=1, value=`\n`)
- `sno_var_sync_registered()` is called after all `sno_var_register()` calls ✅
- `snoSrc = snoSrc snoLine nl` emits `sno_concat_sv(...)` correctly ✅
- By the time the snoParse match fires, `snoSrc = "    x = 'hello'\n"` (16 chars) ✅
- The `slen=0` traces seen previously were from OTHER matches (pattern construction during init), not the main snoParse match

**The real symptom:** `sno_match_pattern` tries all 17 positions (start=0..16) against snoSrc — ALL FAIL. The pattern is structurally present (type=5) but semantically broken.

### §15.2 — KEY ARCHITECTURAL INVARIANT (confirmed Session 50)

**If you strip all `.` and `$` captures/actions from the grammar patterns, the structural pattern WILL match beauty.sno text — this was validated during bootstrap.**

Corollary: the match failure is NOT in the pattern structure. It must be in something that corrupts the pattern VARIABLES at runtime — between grammar init and the main match loop.

### §15.3 — E_COND bug: impact is HARMLESS to match

`emit.c` `case E_COND` / `case E_IMM`: when `e->right` is not `E_VAR` (e.g. `E_DEREF(E_CALL(...))`), varname falls back to `"?"`. This emits `sno_pat_cond(pat, "?")`. At runtime, `SPAT_ASSIGN_COND` with varname `"?"` wraps the child pattern correctly — the match is NOT affected, only the capture target is wrong. **This bug does not cause match failure.**

### §15.4 — ACTUAL ROOT CAUSE CANDIDATE: parser misreads `*var (expr)`

In `pat_atom` grammar (`sno.y`):
```
| STAR IDENT         → E_DEREF(E_VAR("ident"))     # correct: *snoWhite
| IDENT LPAREN ...   → E_CALL("ident", args)        # correct: func(args)
```

But `*snoWhite` followed immediately by `(expr)` — after `STAR IDENT` reduces to `pat_atom`, does the next `(` get parsed as starting a new `pat_atom` (grouped subpattern), or does the parser backtrack and see `IDENT LPAREN` as a function call?

**Evidence:** Generated C contains `sno_apply("snoWhite", (...), 1)` — calling snoWhite AS A FUNCTION with a pattern argument. This should be `sno_pat_cat(sno_pat_ref("snoWhite"), pat)`. The parser is misreading `*snoWhite (subpat)` as `snoWhite(subpat)` = function call.

**Also:** `sno_pat_deref(sno_str("?"))` appears in snoStmt — dereferencing a variable literally named `"?"`. Variable `"?"` gets set by the bogus `sno_pat_cond(..., "?")` captures — so this is a compounding corruption: the E_COND bug pollutes var `"?"`, and then `sno_pat_deref(sno_str("?"))` uses that polluted value.

### §15.5 — SMOKE TEST DESIGN: per-statement pattern match

**The correct smoke test for the grammar is:**

1. Build `beauty_full_bin`
2. For each SNOBOL4 statement in beauty.sno, test that `snoCommand` matches it
3. This is a pure structural match — no captures, no side effects needed

This test would have caught the current failure immediately. **Add this as a mandatory smoke test before any Milestone 0 claim.**

Proposed test file: `test/smoke/test_snoCommand_match.sh`

```bash
# For each non-comment, non-blank line in beauty.sno:
# printf "line\nEND\n" | beauty_full_bin
# should NOT output "Parse Error"
```

### §15.6 — Session 50 Next Steps (NOT YET DONE)

1. **Fix `*var (expr)` parsing** — `sno.y`: after `STAR IDENT` reduces, `(expr)` must be a new pat_atom (concat), NOT a function call on the bare IDENT. The `STAR` prefix means the IDENT is already consumed as a deref — the `(` cannot retroactively make it a function call.

2. **Fix `sno_pat_deref(sno_str("?"))` emissions** — trace where `$'?'` in the source becomes a pattern deref of `"?"` rather than an immediate capture.

3. **Write smoke test** `test/smoke/test_snoCommand_match.sh` as described in §15.5.

4. **After parser fix:** rebuild, rerun, confirm `try_match_at` succeeds for at least one position.


---

## §16 — Session Log

### Session 50 (2026-03-12)
- Confirmed `snoSrc` IS populated correctly — earlier `slen=0` traces were from pattern construction during init, not the main match
- Confirmed `_nl` is correctly initialized (type=1, `\n`)
- Confirmed `snoCommand`, `snoParse`, `snoStmt`, `snoWhite` all type=5 (PATTERN) ✅
- `sno_match_pattern` tries all positions against correct subject — ALL FAIL
- E_COND bug (`"?"` varname) confirmed HARMLESS to match — child pattern still wraps correctly
- ROOT CAUSE CANDIDATE: parser misreads `*snoWhite (expr)` as `snoWhite(expr)` function call
  - Evidence: `sno_apply("snoWhite", ..., 1)` in generated C for snoStmt construction
  - Also: `sno_pat_deref(sno_str("?"))` — deref of var named `"?"` polluted by bogus captures
- KEY INVARIANT documented: structural pattern match works (bootstrap proof)
- Created `test/smoke/` with three shell scripts replacing obsolete Python sprint tests
- Decisions 12+13 written to DECISIONS.md
- Session interrupted before fixing `sno.y` parser rule for `STAR IDENT (expr)`

---

## §17 — Smoke Test Infrastructure (Session 50, 2026-03-12)

### Convention: per-session artifacts and outputs committed to GitHub

Every session that produces a meaningfully different compiler output commits:

   - Debuggable without rebuilding
   - README entry with line count, md5, active bug status

2. **`test/smoke/outputs/sessionNN/`** — captured smoke test results
   - `build_beauty.log` — gcc compile result
   - `test_snoCommand_match.log` — per-statement grammar match results
   - `test_self_beautify.log` — Milestone 0 diff result
   - `beauty_oracle.sno` — CSNOBOL4 oracle output
   - `beauty_compiled.sno` — compiled binary output
   - `beauty_diff.txt` — diff oracle vs compiled
   - `README.md` — summary table of pass/fail status

### Session 50 smoke test results summary

| Smoke Test | Result |
|------------|--------|
| build_beauty | ✅ PASS — 0 gcc errors, 12847 lines C |
| snoCommand match | ❌ 0/21 — every statement type fails with "Parse Error" |
| self-beautify (Milestone 0) | ❌ NOT ACHIEVED — 785 line diff, oracle=790 compiled=10 |

### Smoke test scripts (test/smoke/)

| Script | Purpose |
|--------|---------|
| `build_beauty.sh` | Compile beauty.sno → C → binary (Milestone 1+2 validation) |
| `test_snoCommand_match.sh` | Match 21 statement types against snoCommand pattern |
| `test_self_beautify.sh` | Self-beautify diff vs oracle (Milestone 0 gate) |

### How to run

```bash
REPO=/path/to/snobol4x

# Step 1: build
bash $REPO/test/smoke/build_beauty.sh

# Step 2: grammar smoke
bash $REPO/test/smoke/test_snoCommand_match.sh /tmp/beauty_full_bin

# Step 3: milestone 0
bash $REPO/test/smoke/test_self_beautify.sh /tmp/beauty_full_bin
```

### Passing criteria for Milestone 0

1. `build_beauty.sh` — ALL PASS
2. `test_snoCommand_match.sh` — ALL PASS (21/21)
3. `test_self_beautify.sh` — EMPTY DIFF

### Note on old test suite

`test/sprint*/` contain Python-based tests using `sno_parser` / `emit_c_stmt`
(the old Python snoc pipeline). These do NOT run against the current C compiler.
Retained for historical reference only. See Decision 13 in `doc/DECISIONS.md`.


### Session 50 addendum — artifacts committed
- `test/smoke/outputs/session50/` — full logs, oracle, compiled output, diff
- Smoke test convention documented in §17
- Commits: `375d55c` (findings+tests), `7d3d0b6` (artifact), `05b80c2` (outputs)
