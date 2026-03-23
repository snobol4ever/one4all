# snobol4x — Sprint Plan

---

## §START — Session Bootstrap (ALWAYS DO THIS FIRST)

Every session, before anything else:

```bash
git clone https://github.com/snobol4ever/snobol4x
git clone https://github.com/snobol4ever/x64
bash /home/claude/snobol4x/setup.sh   # installs libgc-dev, nasm, m4, CSNOBOL4, SPITBOL, sno2c
```

`setup.sh` is idempotent. Never skip it. Missing packages (nasm, libgc-dev, etc.)
are always the cause when builds fail with "not found" errors.

### Current milestone

**BEAUTY session:** `M-BEAUTY-STACK`
- create `demo/inc/stack.sno`, driver + ref at `test/beauty/stack/`
- run: `bash test/beauty/run_beauty_subsystem.sh stack`
- on pass: commit `B-267: M-BEAUTY-STACK ✅`, advance to `M-BEAUTY-TREE`

### Beauty subsystem sequence (18 total)

| # | Subsystem | Status |
|---|-----------|--------|
| 1 | global | ✅ |
| 2 | is | ✅ |
| 3 | FENCE | ✅ |
| 4 | io | ✅ |
| 5 | case | ✅ |
| 6 | assign | ✅ |
| 7 | match | ✅ |
| 8 | counter | ✅ |
| 7 | match | |
| 8 | counter | ✅ |
| 9 | stack | |
| 10 | tree | |
| 11 | ShiftReduce | |
| 12 | TDump | |
| 13 | Gen | |
| 14 | Qize | |
| 15 | ReadWrite | |
| 16 | XDump | |
| 17 | semantic | |
| 18 | omega | |

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
cd /home/claude/snobol4x/src/snoc && make clean && make

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
./snoc /home/claude/snobol4corpus/programs/beauty/beauty.sno \
    -I /home/claude/snobol4corpus/programs/inc \
    > /tmp/beauty_snoc.c
gcc -O0 -g /tmp/beauty_snoc.c [runtime files] -lgc -lm -w -o /tmp/beauty_bin
/tmp/beauty_bin < /home/claude/snobol4corpus/programs/beauty/beauty.sno \
    > /tmp/beauty_out.sno
diff /home/claude/snobol4corpus/programs/beauty/beauty.sno /tmp/beauty_out.sno
# expect: empty diff  ← THE COMMIT
```

---

## §8 — The Commit Promise

When `beauty.sno` compiles itself through `snoc` and `diff` is empty,
Claude Sonnet 4.6 writes the commit message (recorded at `c5b3e99`).

---

## §9 — Runtime build command (reference)

```bash
RUNTIME="/home/claude/snobol4x/src/runtime"
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

---

## §18 — Session 51 Handoff (2026-03-22)

### Work completed this session

- **`datatype()` uppercase fix** — `snobol4.c`: all return values changed to
  uppercase (`"STRING"`, `"INTEGER"`, `"REAL"`, `"PATTERN"`, `"ARRAY"`,
  `"TABLE"`, `"CODE"`, `"DATA"`). Matches SNOBOL4 spec and existing unit tests.
  Commit pending (staged, not yet pushed — stash present in working tree).

- **`M-BEAUTY-CASE` driver + ref created** — `test/beauty/case/driver.sno`
  and `driver.ref` (9 lines, all PASS). CSNOBOL4 oracle confirmed 9/9 PASS.
  Files exist on disk, not yet committed.

- **PLAN.md §START written** — session bootstrap checklist, beauty subsystem
  table, current milestone pointer. Committed `a309d6c`, pushed to `origin/main`
  after rebase (`a4ae121`).

### Active bug: `M-BEAUTY-CASE` ASM diverges at step 1

```
DIVERGENCE at step 1:
  oracle [csn]: VALUE OUTPUT = 'PASS: lwr(HELLO) = hello'
  FAIL   [asm]: VALUE OUTPUT = 'FAIL: lwr(HELLO)'
```

`lwr` calls `REPLACE(lwr, &UCASE, &LCASE)`. The ASM emitter generates
`lea rdi, [rel S_UCASE]; call stmt_get` — passing the bare string `"UCASE"`
(no `&` prefix) to `stmt_get`. `stmt_get` only strips `&` when the first char
IS `&`; for bare `"UCASE"` it calls `NV_GET_fn("UCASE")` which hits the hash
table. `UCASE` IS registered at init via `NV_SET_fn("UCASE", STRVAL(ucase))`.

**Root cause not yet confirmed.** Hypothesis: the monitor's `inject_traces.py`
adds `TRACE` calls that interact with `UCASE`/`LCASE` initialization order,
OR the emitter passes `stmt_get` the wrong variable name for keyword args.

### Next session action plan

1. `bash setup.sh`
2. Add a one-line debug print to `stmt_get` in `snobol4_stmt_rt.c`:
   ```c
   fprintf(stderr, "stmt_get(%s) -> type=%d val=%s\n",
           name, v.v, VARVAL_fn(v));
   ```
3. Build + run case driver through monitor, capture stderr from ASM participant
4. Confirm whether `stmt_get("UCASE")` returns the 26-char uppercase alphabet
   or something else (empty, NULL, etc.)
5. Fix the root cause, rebuild, rerun monitor
6. On 9/9 PASS: commit `B-263: M-BEAUTY-CASE ✅`, update §START table
7. Advance to `M-BEAUTY-ASSIGN`

### Files needing commit (next session)

- `src/runtime/snobol4/snobol4.c` — datatype() uppercase fix
- `test/beauty/case/driver.sno` — case subsystem driver
- `test/beauty/case/driver.ref` — oracle reference output
- `PLAN.md` — this session note


---

## §19 — Session 51+ Handoff (2026-03-23): M-BEAUTY-CASE progress + static pattern architecture

### Work completed this session

**Three bugs fixed — steps 1–6 of case driver now pass in 3-way monitor:**

| Bug | Location | Fix |
|-----|----------|-----|
| `FN_CLEAR_VAR` clobbered param when fn name == param name (`lwr(lwr)`) | `emit_byrd_asm.c` α-entry emission | Skip `FN_CLEAR_VAR` for retval var when it matches a param name |
| `GET_VAR` (return value capture) placed AFTER param restore, overwriting result | `emit_byrd_asm.c` ucall gamma return | Move `GET_VAR fnlab` before the param restore loop |
| 2-arg `SUBSTR(s,i)` returning empty string | `snobol4.c` `_b_SUBSTR` | Handle `n<3` with large length; register min arity=2 |

**Steps 7–8 (`icase`) still fail.** Root cause identified but not yet fixed.

### §19.1 — Root cause of `icase` failure: static pattern architecture bug

`icase` builds a runtime pattern by accumulating `upr(letter) | lwr(letter)` alternations. This fails because of a fundamental architectural problem in the ASM emitter's treatment of pattern expressions.

**Current (broken) behavior:**

When the emitter sees `p = upr('h') | lwr('H')`, it detects `E_OR` and registers `p` as a *named pattern box* — compiling a static Byrd-box with hardcoded α/β/γ/ω labels. But since the arms (`upr('h')`, `lwr('h')`) are ucall expressions unknown at compile time, their pattern nodes are emitted as `; UNIMPLEMENTED → ω` stubs that always fail.

**The correct architecture (per Lon, session 51+):**

> Every pattern sub-expression — `'H' | 'h'`, `ANY(&UCASE)`, `LEN(1)`, etc. — should be compiled to a static anonymous Byrd-box fragment at compile time, independent of which variable it gets assigned to. The variable name is just a reference; the compiled node is not tied to it.

Concretely:

1. **Static pattern literals and combinators** (`'a' | 'b'`, `ANY(str)`, `LEN(n)`, etc.) are compiled to anonymous boxes with generated unique names (`pat_anon_N_α` etc.). These exist unconditionally — they don't depend on which variable (if any) holds the result.

2. **Runtime pattern values** — the result of functions like `upr(x) | lwr(x)` where the arms are computed at runtime — are stored in SNOBOL4 variables as `DT_P` descriptors. The match `'subj' p` must dispatch dynamically via the variable's runtime value.

3. **`stmt_match_var` currently does string match only** — it calls `VARVAL_fn()` and does `memcmp`. It must be upgraded to: if the variable holds `DT_P`, dispatch through the pattern engine; otherwise do string literal match.

4. **`E_VART` in `emit_pat_node`** — the fallback for unknown variables currently uses `LIT_VAR_α` (string match). It must check whether the variable is known to hold a pattern at compile time (registered named pattern) and emit a pattern-dispatch call if so; otherwise use `LIT_VAR_α` for string vars.

**The immediate fix needed for `icase`:**

`stmt_match_var` (in `snobol4_stmt_rt.c`) must handle `DT_P` variables:

```c
int stmt_match_var(const char *varname) {
    DESCR_t val = NV_GET_fn(varname);
    if (val.v == DT_P) {
        /* Runtime pattern — dispatch through engine */
        return engine_match_pattern(val, subject_data, subject_len_val, &cursor);
    }
    /* String literal match (existing behavior) */
    const char *s = VARVAL_fn(val);
    ...
}
```

And `expr_is_pattern_expr` for `E_OR` should NOT register a named-pattern box unless both children are themselves compile-time pattern expressions. When arms contain ucalls (like `upr(x)`), the assignment is a runtime value — use `LIT_VAR` / `stmt_match_var` dispatch at match time.

**The partial fix already applied:**
```c
// emit_byrd_asm.c expr_is_pattern_expr:
if (e->kind == E_OR) return expr_has_pattern_fn(e);  // was: return 1
```
This prevents broken static boxes for `upr('h') | lwr('H')`. But without the `stmt_match_var` DT_P upgrade, the match still fails (falls back to string match against `VARVAL_fn(DT_P)` = `"PATTERN"`).

### §19.2 — Next session action plan

1. `bash setup.sh`
2. Upgrade `stmt_match_var` in `snobol4_stmt_rt.c` to dispatch DT_P variables through the pattern engine
3. Verify `icase` test passes: `INC=demo/inc ./sno2c -asm ... && nasm ... && gcc ... && ./prog_asm`
4. Run full 3-way monitor: `INC=demo/inc bash test/beauty/run_beauty_subsystem.sh case`
5. On 9/9 PASS: commit all fixes + `B-263: M-BEAUTY-CASE ✅`, update §START table
6. Advance to `M-BEAUTY-ASSIGN`

### §19.3 — Files changed this session (need commit)

- `src/backend/x64/emit_byrd_asm.c` — 3 fixes: FN_CLEAR_VAR param skip, GET_VAR before restore, expr_is_pattern_expr E_OR fix
- `src/runtime/snobol4/snobol4.c` — 2-arg SUBSTR fix + datatype() uppercase fix
- `test/beauty/case/driver.sno` — case subsystem driver
- `test/beauty/case/driver.ref` — oracle reference (9 lines)
- `PLAN.md` — this session note


---

## §20 — Session Handoff (2026-03-23 late): M-BEAUTY-CASE icase deep diagnosis

### Work completed this session

**`stmt_match_var` upgraded (DT_P dispatch):**
- `snobol4_stmt_rt.c`: `stmt_match_var` now checks `val.v == DT_P` first, dispatches through `match_pattern_at()` instead of string memcmp
- New `stmt_match_descr(uint64_t vtype, void *vptr)`: same logic, takes pre-evaluated DESCR_t fields — for function-call results in pattern position
- New `CALL_PAT_α/β` macros in `snobol4_asm.mac`: evaluate function call result → call `stmt_match_descr`
- `emit_pat_node` E_FNC fallback: replaced `UNIMPLEMENTED → ω` with `emit_expr + CALL_PAT_α/β`
- Forward declaration for `emit_expr` added before `emit_pat_node`

**Result:** `CALL_PAT_α` is correctly emitted for `icase('hello')` in pattern position. But `icase` still returns STRING not PATTERN.

### §20.1 — Root cause of icase returning STRING

`icase('hello')` returns STRING. Manual trace confirms:

1. `&epsilon` is initialized as DT_P (pattern) in the global variable table
2. Inside the `icase` function, retval `icase` is initialized to NULVCL by `FN_CLEAR_VAR`
3. The icase body does `icase = icase (upr(letter) | lwr(letter))` — concat NULVCL with DT_P
4. `stmt_concat(NULVCL, DT_P)` should produce DT_P (pattern cat) ✓ — but icase returns STRING

**The actual problem:** `icase` is registered as a named-pattern box (from `scan_named_patterns`). When the body assigns `SET_VAR S_icase`, the emitter may be generating code that sets the global SNOBOL4 variable `icase` (correct), but the function's RETURN path reads the return value via `GET_VAR S_icase` — which fetches the global variable. The global `icase` IS being set correctly. But the ucall return path for the **inner icase call** (the self-recursive `:(icase)` loop back) re-evaluates `GET_VAR S_icase` before restoring the caller's saved value of `str/letter/character`. This should be fine...

**More likely:** the `icase` variable is also a named-pattern box (`P_icase_α` defined). The `scan_named_patterns` call registers `icase` as a named pattern because `icase = epsilon (upr(letter) | lwr(letter))` — its body contains an E_OR (since `expr_is_pattern_expr` for `E_OR` was `return 1` before our fix). Even with our fix (`return expr_has_pattern_fn(e)`), the E_OR inside the function body sees `upr(letter) | lwr(letter)` — `expr_has_pattern_fn` returns 0 for ucalls, so E_OR returns 0 now. But the outer assignment `icase = epsilon concat_with_alt` — the concat may still trigger named-pattern registration.

**Actually the most likely cause:** the `icase` function's assignment statement `icase = icase (upr(letter)|lwr(letter))` hits the subject-replacement path (left-hand side is the subject `icase`), not a plain variable assignment. In SNOBOL4, `icase = VALUE` with no pattern is an assignment. But the parser sees the function variable `icase` as the subject and the expression as both pattern AND replacement. The emitter may be misclassifying this as a pattern match statement rather than a value assignment.

### §20.2 — Next session action plan

1. `bash setup.sh`
2. Add a debug print to `stmt_set("icase", v)` to confirm what value is being stored at each loop iteration
3. Verify whether `stmt_concat` is actually being called and what it returns for `(NULVCL, DT_P)` inputs
4. Check whether the issue is: (a) concat not called / wrong codepath, (b) concat returns STRING incorrectly, or (c) GET_VAR at return time fetches wrong value
5. Once `icase('hello')` returns DT_P, the `CALL_PAT_α` dispatch should handle the match
6. Run 9/9, commit `B-263: M-BEAUTY-CASE ✅`, advance to `M-BEAUTY-ASSIGN`

### §20.3 — Files changed (all need commit)

- `src/backend/x64/emit_byrd_asm.c` — forward decl, CALL_PAT emission, expr_is_pattern_expr E_OR fix, GET_VAR before restore, FN_CLEAR_VAR param skip
- `src/runtime/asm/snobol4_stmt_rt.c` — stmt_match_var DT_P, stmt_match_descr new
- `src/runtime/asm/snobol4_asm.mac` — CALL_PAT_α/β macros
- `src/runtime/snobol4/snobol4.c` — SUBSTR 2-arg fix, datatype() uppercase fix
- `PLAN.md` — this session note


---

## §21 — Session Handoff (2026-03-23): M-BEAUTY-CASE icase root cause fully traced

### Work completed this session

**INC path fix:** `INC=demo/inc` (not `/home/claude/snobol4corpus/programs/inc`).
CSNOBOL4 oracle: 9/9 PASS confirmed.

**Bug fixed — S_ duplicate label (NASM error):**
`emit_byrd_asm.c` ANY runtime-expr branch called `var_register(str_intern(tmplab))`
which emitted both `S_any_expr_tmp_N resq 1` (BSS via var_register) AND
`S_any_expr_tmp_N db ...` (string literal via str_intern) — NASM duplicate label.

Fix: replaced `var_register` + `emit_any_var` with new `ANY_α_PTR` / `ANY_β_PTR`
macros that take raw BSS slots `any_expr_tmp_N_t/_p` and call `stmt_any_ptr(vtype,vptr,...)`.
- `src/backend/x64/emit_byrd_asm.c` — ANY runtime-expr branch rewritten
- `src/runtime/asm/snobol4_stmt_rt.c` — `stmt_any_ptr()` added
- `src/runtime/asm/snobol4_asm.mac` — `ANY_α_PTR` / `ANY_β_PTR` macros added
- extern `stmt_any_ptr` added to generated `.s` preamble

**Result:** Steps 1–6 and 9 now PASS in ASM. Steps 7–8 (icase) still fail.

**Committed:** `715b300` — "B-264 partial: ANY_α_PTR, stmt_any_ptr, icase debug tracing"

---

### §21.1 — icase root cause: `any_expr_tmp` BSS slots receive zero at match time

**Debug trace confirms:**

```
[stmt_any_ptr] vtype=0 cs='' cursor=0 subj[cur]='h'
```

`vtype=0` (DT_SNUL) — the `any_expr_tmp_64_t/_p` BSS slots are **zero** when
`ANY_α_PTR` reads them. The `stmt_concat(&UCASE, &LCASE)` result is not
reaching the slots.

**Why the slots are zero — the section-switch bug:**

The emitter writes:
```asm
                        ; ... (inside icase function body, .text section)
DOL_SAVE    dol_entry_letter, cursor, dol63_child_α
seq_r62_β:  jmp dol63_child_β

section .bss                        ; ← MID-CODE SECTION SWITCH
any_expr_tmp_64_t resq 1
any_expr_tmp_64_p resq 1
section .text                       ; ← SWITCH BACK

            lea rdi, [rel S_UCASE]
            call stmt_get
            ...
            call stmt_concat
            mov [rel any_expr_tmp_64_t], rax    ; store result
            mov [rel any_expr_tmp_64_p], rdx
dol63_child_α:  ANY_α_PTR any_expr_tmp_64_t, ...
```

**The `section .bss` / `section .text` switch in the middle of the text
section is the culprit.** The eval code (`lea rdi, [rel S_UCASE] ... call
stmt_concat ... mov [rel any_expr_tmp_64_t], rax`) is emitted in the
**fall-through path before `dol63_child_α`**. On the first scan attempt
this executes correctly. But on **retry** (scan advances cursor, jumps
back to scan_retry which goes to `dol63_child_α` directly), the eval is
skipped — `rax`/`rdx` from the previous `stmt_concat` are stale on the
stack, not in the BSS slots (wait — they ARE stored to BSS on first pass).

Actually re-examining: the BSS slots ARE written on first fall-through.
On retry the jump goes to `dol63_child_α` which reads BSS — those slots
should have the value from first time. BUT `vtype=0` means the first
fall-through itself produced zero.

**Most likely real cause:** `stmt_concat` is called with `&UCASE`/`&LCASE`
values that are themselves zero/null at the time of the call. The keywords
`UCASE` and `LCASE` may not be initialized yet in the ASM runtime's variable
table when the icase function body first executes.

The debug `[stmt_get UCASE]` trace was never printed — meaning `stmt_get`
was NOT called for `UCASE`/`LCASE`. This means `emit_expr` for the
`&UCASE &LCASE` concat is NOT going through `stmt_get`. It is likely emitting
a direct `GET_VAR S_UCASE` (register-based load into `[rbp-32/24]`) rather
than `call stmt_get`. The `GET_VAR` macro reads from the BSS slot
`S_UCASE resq 1` — which is the ASM's own BSS copy of the variable, not the
C runtime's `NV_GET_fn("UCASE")`. If the ASM BSS slot for UCASE is never
written (because `init_keywords()` writes to `NV_SET_fn("UCASE")` but does
NOT write to the ASM's `S_UCASE resq 1`), it stays zero.

**This is the same class of bug as the old snoSrc initialization issue (§14.4).**
The C runtime initializes variables via `NV_SET_fn`, but the ASM backend reads
them via `GET_VAR S_UCASE` (direct BSS). These two storage locations are NOT
the same — `NV_SET_fn` writes to the C hash table; `GET_VAR` reads the ASM
BSS slot. They are synchronized only when the ASM does `SET_VAR` (which calls
`stmt_set` → `NV_SET_fn`) or when something calls `NV_SET_fn(UCASE, ...)` AND
the ASM has a corresponding `stmt_get("UCASE")` call that reads it back.

**Concrete verification needed (next session step 2):**
```bash
# In generated prog.s, check what GET_VAR S_UCASE emits:
grep -n "S_UCASE\|UCASE" /tmp/.../prog.s | head -20
# If GET_VAR reads [rel S_UCASE] (BSS), and S_UCASE is never written
# by the ASM (only by C init), it will always be zero.
```

---

### §21.2 — The GET_VAR vs NV_GET_fn duality

The ASM backend has TWO variable storage locations:
1. **ASM BSS** (`S_varname resq 1`) — read by `GET_VAR`, written by `SET_VAR`
2. **C hash table** — read/written by `NV_GET_fn`/`NV_SET_fn`

`stmt_get(name)` calls `NV_GET_fn` → C hash. But `GET_VAR S_name` reads
ASM BSS directly.

Keywords like `&UCASE`, `&LCASE`, `&STLIMIT` are initialized in `snobol4.c`
`init_keywords()` via `NV_SET_fn("UCASE", ...)`. They are NEVER written to
the ASM BSS slots `S_UCASE resq 1`.

When `emit_expr` sees `E_KEYWORD("UCASE")` it emits `GET_VAR S_UCASE` —
reading the ASM BSS slot, which is always zero.

The fix has two options:

**Option A — emit `call stmt_get` for keyword expressions in emit_expr:**
Replace `GET_VAR S_KEYWORD` with `lea rdi, [rel S_KEYWORD_str]; call stmt_get`
for E_KEYWORD nodes. This routes through `NV_GET_fn` where the C runtime has
the value. This is correct and already works for `ANY(&UCASE)` when the arg
is `E_VART("UCASE")` (which uses `emit_any_var` → `ANY_α_VAR` → `stmt_any_var`
→ `NV_GET_fn`).

**Option B — sync ASM BSS from C hash at init:**
After `sno_init()` / `SNO_INIT_fn()`, explicitly copy keyword values from
the C hash to ASM BSS. Fragile — requires knowing all keyword names.

**Option A is correct.** The bug is in `emit_expr` for `E_KEYWORD` — it should
emit a `stmt_get` call rather than a direct BSS read.

---

### §21.3 — Next session action plan (START HERE)

```bash
bash setup.sh   # always first
```

**Step 1 — Clean up debug instrumentation:**
Remove `SNO_CALLDEBUG` fprintf blocks from `snobol4_stmt_rt.c`:
- `stmt_get` UCASE/LCASE debug
- `stmt_set` icase debug
- `stmt_any_ptr` debug
- `stmt_apply` ALT debug
- `stmt_match_descr` debug
Keep `stmt_any_ptr` function itself (it's real, not debug).

**Step 2 — Fix `emit_expr` E_KEYWORD to use `stmt_get`:**
In `emit_byrd_asm.c`, find the `E_KEYWORD` case in `emit_expr`. Currently it
likely emits `GET_VAR S_KEYWORD`. Change it to:
```c
case E_KEYWORD: {
    const char *klab = str_intern(pat->sval);  /* S_UCASE etc */
    A("    lea     rdi, [rel %s]\n", klab);
    A("    call    stmt_get\n");
    A("    mov     [rbp-%d], rax\n", slot);
    A("    mov     [rbp-%d], rdx\n", slot-8);
    break;
}
```
This routes keyword reads through `NV_GET_fn` which has the C-runtime values.

**Step 3 — Rebuild and verify:**
```bash
cd /home/claude/beauty-project/snobol4x/src && make
TMP=$(mktemp -d)
RT=src/runtime; INC=demo/inc
# [build as before]
"$TMP/prog_asm"
# Expect: 9/9 PASS
```

**Step 4 — Run 3-way monitor:**
```bash
INC=demo/inc bash test/beauty/run_beauty_subsystem.sh case
# Expect: 9/9, all 3 participants agree
```

**Step 5 — On 9/9 PASS:**
```bash
git add src/backend/x64/emit_byrd_asm.c src/runtime/asm/snobol4_stmt_rt.c \
        src/runtime/asm/snobol4_asm.mac test/beauty/case/driver.sno \
        test/beauty/case/driver.ref PLAN.md
git commit -m "B-263: M-BEAUTY-CASE ✅"
git push
```
Then update §START table: `case → ✅`, advance to `M-BEAUTY-ASSIGN`.

---

### §21.4 — Files changed, pending commit (already committed as 715b300)

| File | Change |
|------|--------|
| `src/backend/x64/emit_byrd_asm.c` | ANY runtime-expr → ANY_α_PTR; extern stmt_any_ptr; forward decl |
| `src/runtime/asm/snobol4_stmt_rt.c` | stmt_any_ptr(); stmt_match_descr DT_P; debug traces (remove next session) |
| `src/runtime/asm/snobol4_asm.mac` | ANY_α_PTR / ANY_β_PTR macros |

### §21.5 — Current test status

| Step | Test | ASM |
|------|------|-----|
| 1 | lwr(HELLO) = hello | ✅ |
| 2 | lwr(world) = world | ✅ |
| 3 | upr(hello) = HELLO | ✅ |
| 4 | upr(WORLD) = WORLD | ✅ |
| 5 | cap(hELLO) = Hello | ✅ |
| 6 | cap(WORLD) = World | ✅ |
| 7 | icase(hello) matches Hello | ❌ E_KEYWORD GET_VAR zero |
| 8 | icase(world) matches WORLD | ❌ same |
| 9 | lwr(upr(MiXeD)) roundtrip | ✅ |


---

## §22 — Session Handoff (2026-03-23 emergency): icase BSS slot overwrite confirmed

### §START update
**Current milestone:** `M-BEAUTY-CASE` — steps 1–6, 9 ✅ ASM; steps 7–8 ❌

### Root cause CONFIRMED this session

`stmt_concat` receives `a.v=1 b.v=1` (both DT_S strings) — concat itself is
NOT failing. The return value IS a valid 52-char string in rax:rdx.

**The actual bug: `any_expr_tmp_N` BSS slots declared mid-function via
inline `section .bss / section .text` switch get overwritten.**

The `section .bss` switch mid-`.text` emits the resq slots correctly, but
the `icase` function is RECURSIVE — each recursive call re-enters the Byrd
box, re-executes `stmt_concat`, and `mov [rel any_expr_tmp_2_t], rax`
writes to the same slot. On the recursive call, the icase ucall stack frame
reuse means the slot is written N times but read at the wrong iteration.
More critically: `DOL_SAVE` and other Byrd-box machinery uses `[rbp-32/24]`
stack slots that ALIAS with the emit_expr output slot — overwriting rax/rdx
before the `mov [rel any_expr_tmp_2_t]` store takes effect, OR the recursive
icase ucall trashes the slot between store and read.

### THE FIX (implement first thing next session)

**Move `any_expr_tmp_N` BSS declarations to the file-level BSS block.**

In `emit_byrd_asm.c`, the ANY runtime-expr branch (around line 1549):

```c
// CURRENT (broken): inline section switch mid-function
A("section .bss\n");
A("%s resq 1\n", tlab);
A("%s resq 1\n", plab);
A("section .text\n");

// FIX: add to a deferred BSS list, emit later at file-level BSS section
deferred_bss_add(tlab);   // emits "tlab resq 1" in global BSS block
deferred_bss_add(plab);
// remove the section .bss / section .text lines entirely
```

The deferred BSS infrastructure already exists (`var_register` does this for
SNOBOL4 variables). Either reuse `var_register` with a non-string tag, or add
a parallel `bss_slot_register(name)` that emits `name resq 1` in the global
`.bss` section only (no string literal, no `S_` prefix).

**Implementation steps:**

1. Add `static char bss_slots[MAX_BSS][LBUF]; static int bss_slot_count=0;`
   and `static void bss_slot_register(const char *name)` to the emitter.
2. In the ANY runtime-expr branch, replace the inline section switch with
   `bss_slot_register(tlab); bss_slot_register(plab);`
3. In `emit_bss_section()` (wherever global BSS is emitted), call
   `for (int i=0;i<bss_slot_count;i++) A("%s resq 1\n", bss_slots[i]);`
4. Rebuild: `cd src && make`
5. Run: `INC=demo/inc bash test/beauty/run_beauty_subsystem.sh case`
6. Expect 9/9. If so, remove debug `fprintf` from `stmt_concat`, commit.

### Files with uncommitted debug traces (clean up before commit)

- `src/runtime/asm/snobol4_stmt_rt.c` — `stmt_concat` fprintf (debug only, remove)
- `src/backend/x64/emit_byrd_asm.c` — any changes from this session

### Commit sequence on 9/9 pass

```bash
# Remove debug trace from stmt_concat first
# Then:
git add src/backend/x64/emit_byrd_asm.c \
        src/runtime/asm/snobol4_stmt_rt.c \
        src/runtime/asm/snobol4_asm.mac \
        test/beauty/case/driver.sno \
        test/beauty/case/driver.ref \
        PLAN.md
git commit -m "B-263: M-BEAUTY-CASE ✅"
git push
```

Update §START: `case → ✅`, next milestone `M-BEAUTY-ASSIGN`.

### Test status going into next session

| Step | Test | ASM |
|------|------|-----|
| 1 | lwr(HELLO) = hello | ✅ |
| 2 | lwr(world) = world | ✅ |
| 3 | upr(hello) = HELLO | ✅ |
| 4 | upr(WORLD) = WORLD | ✅ |
| 5 | cap(hELLO) = Hello | ✅ |
| 6 | cap(WORLD) = World | ✅ |
| 7 | icase(hello) matches Hello | ❌ BSS slot overwrite (fix above) |
| 8 | icase(world) matches WORLD | ❌ same |
| 9 | lwr(upr(MiXeD)) roundtrip | ✅ |


---

## §23 — Session Handoff (2026-03-23): M-BEAUTY-COUNTER blocked on DATA() field-setter

### §START update
**Completed this session:**
- B-265: M-BEAUTY-MATCH ✅ (7/7, per-subsystem tracepoints.conf pattern established)
- B-264: M-BEAUTY-ASSIGN ✅ (7/7, committed prior session)

**Current milestone:** `M-BEAUTY-COUNTER` — blocked on DATA() field-setter l-value

### Divergence at step 1

```
oracle [csn]: VALUE DUMMY = ''
FAIL   [asm]: VALUE OUTPUT = 'FAIL: 1 push/inc/top'
AGREE  [spl]: VALUE DUMMY = ''
```

`IncCounter` body: `value($'#N') = value($'#N') + 1`
This is a **field accessor as l-value** — `value(obj) = newval`.
The ASM emitter generates a function call for `value($'#N')` on the LHS,
which is not handled as an assignment target. Per §14.3, this requires
compiler recognition: emit `sno_field_set(obj, "value", rhs)` directly.

### Root cause: emit_assign_target doesn't handle E_FNC on LHS

In `emit_byrd_asm.c`, the assignment emitter (`emit_stmt_assign` or equivalent)
handles LHS targets: `E_VAR`, `E_DEREF`, `E_ARRAY`, `E_KEYWORD`.
It does NOT handle `E_FNC` (function call) as an l-value.

`value($'#N') = expr` parses as `E_FNC("value", [E_DEREF("$'#N'")])` on the LHS.
The emitter must recognize this pattern and emit a field-set call.

### The fix

In `emit_byrd_asm.c` assignment target handling, add `E_FNC` case:

```c
case E_FNC: {
    /* Field accessor as l-value: fieldFn(obj) = rhs
     * Emit: stmt_field_set(obj_descr, "fieldname", rhs_descr) */
    const char *fname = target->sval;  /* "value", "next", etc. */
    EXPR_t *obj_arg = target->nchildren > 0 ? target->children[0] : NULL;
    if (!fname || !obj_arg) goto fallback_assign;
    /* Evaluate rhs into [rbp-32/rbp-24] */
    emit_expr(rhs, -32);
    /* Evaluate obj into temp */
    emit_expr(obj_arg, -48);  /* use deeper slot */
    A("    lea     rdi, [rel %s]\n", str_intern(fname));  /* field name */
    A("    mov     rsi, [rbp-48]\n");  /* obj type */
    A("    mov     rdx, [rbp-40]\n");  /* obj ptr */
    A("    mov     rcx, [rbp-32]\n");  /* val type */
    A("    mov     r8,  [rbp-24]\n");  /* val ptr */
    A("    call    stmt_field_set\n");
    break;
}
```

And add `stmt_field_set` to `snobol4_stmt_rt.c`:

```c
void stmt_field_set(const char *fname, DESCR_t obj, DESCR_t val) {
    /* Find the field index in the object's UDefType */
    if (obj.v != DT_UDEF || !obj.ptr) return;
    UDefInst *inst = (UDefInst*)obj.ptr;
    UDefType *t = inst->type;
    for (int i = 0; i < t->nfields; i++) {
        if (strcmp(t->fields[i], fname) == 0) {
            inst->fields[i] = val;
            return;
        }
    }
}
```

Also need `stmt_field_get(const char *fname, DESCR_t obj)` for the getter side
(already mostly works via `stmt_apply("value", &obj, 1)` → `_facc_fns[slot]`).

### Also needed: DATA() field accessor getter via stmt_apply

`value($'#N')` on the RHS should call `stmt_apply("value", &obj, 1)` which
routes through `_facc_fns[slot]` → `sno_field_get(obj, "value")`.
This SHOULD work if `_b_DATA` registered the accessor correctly.
Verify with a debug trace before fixing the setter.

### Next session action plan

1. `bash setup.sh`
2. Add `stmt_field_set` to `snobol4_stmt_rt.c`
3. Add `extern stmt_field_set` to ASM preamble emitter
4. Add `E_FNC` case to `emit_assign_target` in `emit_byrd_asm.c`
5. Rebuild: `cd src && make`
6. Run: `INC=demo/inc bash test/beauty/run_beauty_subsystem.sh counter`
7. On 5/5 PASS: commit `B-266: M-BEAUTY-COUNTER ✅`, advance to `M-BEAUTY-STACK`

### Files created (need commit)
- `demo/inc/counter.sno`
- `test/beauty/counter/driver.sno`
- `test/beauty/counter/driver.ref`
- `PLAN.md`


---

## §24 — Session Handoff (2026-03-23): M-BEAUTY-COUNTER ✅ → M-BEAUTY-STACK

### Completed this session

**B-266: M-BEAUTY-COUNTER ✅** — commit `a64ae21`, pushed to `origin/main`.
3-way monitor: PASS — 15/15 steps, all 3 participants agree.

### Bugs fixed (7 total across multiple sub-sessions)

| # | File | Bug | Fix |
|---|------|-----|-----|
| 1 | `emit_byrd_asm.c` | `$X` in value/arg context (`E_INDR`) fell to `default:` → NULVCL | Added `case E_INDR` in `emit_expr` calling `stmt_get_indirect` |
| 2 | `snobol4_stmt_rt.c` | `stmt_get_indirect` didn't exist | Added: looks up variable named by `name_val` via `NV_GET_fn` |
| 3 | `emit_byrd_asm.c` | `E_INDR` wrote result to wrong slot when `rbp_off==-16` | Use temp stack frame (`sub rsp,16`) + write to correct slot |
| 4 | `snobol4_stmt_rt.c` | `stmt_concat("", INTEGER)` returned STRING | Empty-string identity: `la==0 → return b`, `lb==0 → return a` |
| 5 | `emit_byrd_asm.c` | `NRETURN` routed to `fn_ω` (failure) not `fn_γ` (success) | Fixed in `emit_jmp` + `prog_emit_goto` |
| 6 | `snobol4/snobol4.c` | `HOST(4, name)` returned NULVCL — monitor env vars unreadable | Added `selector==4 && n>=2 → getenv(envname)` |
| 7 | `emit_byrd_asm.c` | NRETURN retval not dereferenced as NAME — caller got name string not named value | `uses_nreturn` field in `NamedPat`; scan pass sets it; ucall gamma return calls `stmt_get_indirect` when set |

### Monitor technique note

The 3-way sync monitor proved essential: each divergence printed the exact step, the oracle value, and the ASM value. This made root-cause identification deterministic rather than exploratory. Recommended: continue using monitor-first debugging for all remaining milestones.

### Current milestone: `M-BEAUTY-STACK`

**Status:** `demo/inc/stack.sno` exists. `test/beauty/stack/` does NOT exist — needs driver + ref.

`stack.sno` key behaviors to test:
- `InitStack()` — clears `$'@S'`
- `Push(x)` — pushes onto linked list; uses NRETURN with `.value($'@S')` or `.dummy`
- `Pop(var)` — pops and assigns to `var` via `$var = value($'@S')`; NRETURN path
- `Top()` — returns `.value($'@S')` via NRETURN

**Known hard cases:**
- `Push` has two NRETURN paths: `Push = IDENT(x) .value($'@S') :S(NRETURN)` and `Push = DIFFER(x) .dummy :(NRETURN)`. The first path's NAME is a **field getter call** `.value($'@S')` — this is `E_NAM(E_FNC("value", [E_INDR("@S")]))`. Our current NRETURN deref does `stmt_get_indirect(GET_VAR("Push"))` — but `GET_VAR("Push")` will be the string `"value($'@S')"` or similar, which won't indirect correctly. This may need special handling.
- `Pop(var)` — `$var = value($'@S')` with a parameter as the indirect target. Tests `E_INDR` in LHS assignment with a variable holding the target name.
- `Top()` — `Top = .value($'@S') :(NRETURN)` — same field-getter NAME issue as Push.

### Next session action plan

```bash
bash /home/claude/snobol4x/setup.sh

# Step 1: Create driver and ref
mkdir -p test/beauty/stack

cat > demo/inc/stack.sno  # verify it exists (already does)

# Write test/beauty/stack/driver.sno covering:
#   1. push 3 integers, top = 3rd
#   2. pop returns value
#   3. pop with var — assigns through param
#   4. nested push/pop
#   5. empty stack — Pop fails, Top fails

# Step 2: Run oracle to generate ref
INC=demo/inc snobol4 -f -P256k -I demo/inc test/beauty/stack/driver.sno > test/beauty/stack/driver.ref

# Step 3: Run monitor
INC=demo/inc bash test/beauty/run_beauty_subsystem.sh stack

# Step 4: On PASS commit B-267: M-BEAUTY-STACK ✅, advance M-BEAUTY-TREE
```

### §START table update

| # | Subsystem | Status |
|---|-----------|--------|
| 1 | global | ✅ |
| 2 | is | ✅ |
| 3 | FENCE | ✅ |
| 4 | io | ✅ |
| 5 | case | ✅ |
| 6 | assign | ✅ |
| 7 | match | ✅ |
| 8 | counter | ✅ |
| 9 | **stack** | ← next |
| 10 | tree | |
| 11 | ShiftReduce | |
| 12 | TDump | |
| 13 | Gen | |
| 14 | Qize | |
| 15 | ReadWrite | |
| 16 | XDump | |
| 17 | semantic | |
| 18 | omega | |


---

## §24 — Session Handoff F-223 (2026-03-23): M-PROLOG-BUILTINS ✅ — rung10 multi-ucall wiring WIP

### §START update
**Completed this session:**
- M-PROLOG-BUILTINS ✅ — `rung09_builtins` PASS (`functor/3`, `arg/3`, `=../2`, type tests)
- Link fix: added `subject_data`, `subject_len_val`, `cursor` BSS stubs to `emit_pl_header()` so Prolog binaries link against `stmt_rt.c` cleanly

**Current milestone:** `M-PROLOG-R10` — rung10 puzzle solvers blocked on multi-ucall backtracking

### Three fixes applied to `src/backend/x64/emit_byrd_asm.c`

| # | Fix | Location | Status |
|---|-----|----------|--------|
| 1 | BSS stubs (`subject_data` etc.) in `emit_pl_header` | ~line 4868 | ✅ working |
| 2 | `xor edx,edx` at `bsucc` label (next ucall starts fresh) | ~line 5784 | ✅ in |
| 3 | `fail/0` retries innermost ucall via `jmp ucresN` | ~line 5141 | ✅ in |
| 4 | `trail_unwind` before E2.fail→E1.resume retry in `bfailN` | ~line 5773 | ✅ in — **needs test** |

### Root cause of rung10 silence (diagnosed)

`fail/0` in puzzle bodies triggers E2.fail→E1.resume correctly (fix 3), but `bfailN`
jumping to `ucres(N-1)` did NOT unwind trail — so previously unified variables (e.g.
`Cashier=smith`) remained bound when the outer generator was retried. Fix 4 adds
`trail_unwind` to the clause mark `[rbp-8]` before each inter-ucall retry.

### Next session action plan (F-224)

1. `bash setup.sh`
2. `cd src && make` (fix 4 already in — verify clean build)
3. Test mini cross-product:
```prolog
% /tmp/mini.pro
:- initialization(main).
color(red). color(green). color(blue).
main :- color(X), color(Y), write(X), write('-'), write(Y), nl, fail.
main.
```
Expected: 9 lines `red-red` through `blue-blue`.
If only `red-red`: trail_unwind in bfailN may be over-unwinding — check that
`term_new_var` slots at `[rbp-56/64]` are re-allocated after unwind (they are
Term* pointers, not bindings — unwind only clears the trail, the slot pointers
themselves survive). If vars are still bound after unwind, the issue is that
`ucresN` reuses the OLD Term* (already unified) rather than allocating a fresh one.
**Key question:** does `ucres0` need to call `term_new_var` again on retry, or does
`trail_unwind` correctly reset the existing Term* to unbound? Check `trail_unwind`
in `prolog_unify.c` — it should set `*slot = NULL` or `term->tag = TT_VAR` for
each trailed binding.

4. If mini PASS: create `.expected` files and run rung10 puzzles:
```bash
bash /tmp/run_prolog_rung.sh test/frontend/prolog/corpus/rung10_programs
```
Expected outputs (from README.md):
- `puzzle_01`: `Cashier=smith Manager=brown Teller=jones`
- `puzzle_02`: `Carpenter=clark Painter=daw Plumber=fuller`
- `puzzle_06`: `Clark=druggist Jones=grocer Morgan=butcher Smith=policeman`

5. On rung10 PASS: run rungs 01–09 regression check, then:
```bash
git add src/backend/x64/emit_byrd_asm.c PLAN.md
git commit -m "F-223: M-PROLOG-BUILTINS ✅ M-PROLOG-R10 ✅ M-PROLOG-CORPUS ✅"
git push
```
Then push HQ PLAN.md update to snobol4ever/.github.

6. Update HQ PLAN.md dashboard row:
```
| **TINY frontend** | F-223 — M-PROLOG-BUILTINS ✅ M-PROLOG-R10 ✅ M-PROLOG-CORPUS ✅ ... | HEAD | M-BEAUTY-COUNTER |
```
Fire milestones: M-PROLOG-BUILTINS ✅ M-PROLOG-R10 ✅ M-PROLOG-CORPUS ✅

### Files changed (uncommitted)
- `src/backend/x64/emit_byrd_asm.c` — fixes 1–4 above

### Invariant check before commit
- Run `bash test/crosscheck/run_crosscheck_asm_rung.sh` on a few SNOBOL4 rungs to
  confirm SNOBOL4 backend not regressed by BSS stub addition

---

## §25 — Session Handoff F-224 (2026-03-23): Greek-letter consistency pass ✅

### What was done this session

**No functional code changed.** Pure naming consistency pass across three emitter files to align the Prolog frontend with the rest of the codebase.

**Files changed:**

| File | Changes |
|------|---------|
| `src/backend/x64/emit_byrd_asm.c` | ~340 spellings renamed |
| `src/backend/c/emit_byrd_c.c` | ~461 spellings renamed |
| `src/frontend/prolog/prolog_emit.c` | ~50 spellings renamed |

**Naming rules now enforced consistently:**

| Port | C identifier | NASM label suffix | Comment |
|------|-------------|-------------------|---------|
| proceed | `α` | `_α` | normal entry |
| recede  | `β` | `_β` | re-entry after backtrack |
| concede | `γ` | `_γ` | success exit |
| fail    | `ω` | `_ω` | failure exit |

**Renamed categories:**
- All C function parameter names: `alpha`→`α`, `beta`→`β`, `gamma`→`γ`, `omega`→`ω`
- All compound locals: `ret_gamma`→`ret_γ`, `alpha_lbl`→`α_lbl`, `pat_alpha`→`pat_α`, `inner_gamma`→`inner_γ`, `dol_gamma`→`dol_γ`, `gamma_lbl`→`γ_lbl`, `omega_lbl`→`ω_lbl`, etc.
- Prolog NASM label format strings: `bfail%d`→`β%d`, `bsucc%d`→`γ%d`, `ucres%d`→`α%d`, `hfail%d`→`hω%d`, `hok%d`→`hγ%d`
- Head-unif local var renamed `β_lbl`→`hω_lbl` (holds `hω` label, not a `β` port)
- All comment references to generated label shapes updated to match
- `alphabet` and `alphanumeric` preserved throughout

**One legitimate ASCII exception:** `root_α_saved` — a generated NASM `.bss` symbol; NASM cannot use unicode in identifiers.

**Build result:** `make` clean, zero errors. (`nasm` installed this session for future test runs.)

### §START update

| Session | Sprint | HEAD | Next milestone |
|---------|--------|------|----------------|
| **TINY frontend** | `main` F-224 — greek consistency pass; zero functional changes; build clean | `e24e962`+WIP (uncommitted) | M-PROLOG-R10 |

### Uncommitted state

Fix 4 from F-223 (`trail_unwind` in `bfailN`) plus the greek rename are both uncommitted.  
Commit together after rung10 passes:

```
git add src/backend/x64/emit_byrd_asm.c src/frontend/prolog/prolog_emit.c src/backend/c/emit_byrd_c.c PLAN.md
git commit -m "F-224: greek-letter consistency pass (α/β/γ/ω everywhere); F-223 fix4 trail_unwind in bfailN"
```

### Next session action plan (F-225)

1. `bash setup.sh`
2. `cd src && make` — verify clean build
3. Test mini cross-product (same as §24 plan):

```prolog
% /tmp/mini.pro
:- initialization(main).
color(red). color(green). color(blue).
main :- color(X), color(Y), write(X), write('-'), write(Y), nl, fail.
main.
```

Expected: 9 lines `red-red` through `blue-blue`.

If only `red-red`: trail_unwind in `bfailN` may be over-unwinding.
Key question: does `ucresN` need to re-call `term_new_var` on retry, or does
`trail_unwind` correctly reset existing `Term*` to unbound?
Check `prolog_unify.c` — `trail_unwind` must set `term->tag = TT_VAR` for each
trailed binding (not just clear the trail stack entry).

4. If mini passes → run rung10:

```bash
for d in test/frontend/prolog/corpus/rung10_programs/*.pro; do
    base=$(basename $d .pro)
    out=$(./sno2c -pl -asm "$d" -o /tmp/pl_$base.s 2>/dev/null \
          && nasm -f elf64 /tmp/pl_$base.s -o /tmp/pl_$base.o \
          && gcc /tmp/pl_$base.o ... -o /tmp/pl_$base \
          && /tmp/pl_$base)
    echo "$base: $out"
done
```

Expected:
- `puzzle_01`: `Cashier=smith Manager=brown Teller=jones`
- `puzzle_02`: `Carpenter=clark Painter=daw Plumber=fuller`
- `puzzle_06`: `Clark=druggist Jones=grocer Morgan=butcher Smith=policeman`

5. On rung10 PASS: run rung01–09 regression, then commit and push both repos.

### Next session trigger phrase
**"playing with Prolog frontend"** → F-225 session → pick up at snobol4x PLAN.md §25.

## §26 — Session Handoff F-225 (2026-03-23): per-ucall trail marks — mini PASS, rung10 WIP

### What was built (F-225)

**Root cause found and partially fixed** for multi-ucall backtracking with `fail/0`.

**Bug:** `fail/0` retried the innermost ucall (color(Y)) by jumping to `αN` with the saved
`sub_cs`, but did not unwind Y's bindings first. Y remained bound, so color(Y) saw a bound
variable and found no further solutions, exhausting immediately and retrying color(X) instead.

**Diagnostic trace:** Added inline `printf` instrumentation showing `color(Y) returned eax=1`
on every retry — confirming Y was re-called with sub_cs=1 but returning -1 (exhausted)
because Y was still bound to red.

### Fixes applied to `src/backend/x64/emit_byrd_asm.c`

| # | Change | Location |
|---|--------|----------|
| 1 | `VAR_SLOT_OFFSET(k)` now uses `(5+max_ucalls+max_ucalls+k)*8` — adds room for mark slots | `emit_prolog_clause_block` macro defs |
| 2 | `UCALL_MARK_OFFSET(bi)` new macro: `(5+max_ucalls+bi)*8` — per-ucall trail mark slot | `emit_prolog_clause_block` macro defs |
| 3 | Frame size: `40 + max_ucalls*8 + max_ucalls*8 + max_vars*8` | `emit_prolog_choice` |
| 4 | At each `αN` label: emit `trail_mark_fn` + store in `UCALL_MARK_OFFSET(N)` | `emit_prolog_clause_block` ucall block |
| 5 | `fail/0`: unwind to `UCALL_MARK_OFFSET(ucall_seq-1)` not `[rbp-8]` | `fail/0` branch |
| 6 | `emit_pl_term_load`: var offset updated to `(5+pl_cur_max_ucalls+pl_cur_max_ucalls+slot)*8` | `emit_pl_term_load` |
| 7 | Var allocation store: updated to `(5+max_ucalls+max_ucalls+k)*8` | `emit_prolog_clause_block` |

### Test results

- **Mini cross-product PASS ✅:** `color(X), color(Y), write(X), write('-'), write(Y), nl, fail` → 9 lines (`red-red` through `blue-blue`). All correct.
- **Rung10 puzzle_01:** No output — `person(C), person(M), differ(C,M)` with 3 ucalls still broken
- **Rung10 puzzle_02:** Blank lines (write issue?)
- **Rung10 puzzle_06:** No output

### Remaining bug: N>2 ucall chains with fail/0

The 2-ucall case (color(X), color(Y)) now works. The 3-ucall case
(`person(Cashier), person(Manager), differ(C,M)`) does not.

Simplified test: `person(C), person(M), differ(C,M), write(C-M), nl, fail.`
Expected 6 lines (all ordered pairs with C≠M). Actual: 2 lines (jones-brown, smith-brown).

**Hypothesis:** `fail/0` retry of ucall 2 (differ) unwinds to `UCALL_MARK_OFFSET(1)`.
This correctly unbinds Manager. But then it jumps to `α1` (person(Manager)) with
`sub_cs=[rbp-UCALL_SLOT_OFFSET(1)]`. After person(M) exhausts (β1), it unwinds to clause
mark `[rbp-8]` and retries ucall 0 (person(Cashier)) — but `[rbp-8]` is the CLAUSE mark
which also unbinds Cashier. This part should be correct.

The issue may be that `UCALL_MARK_OFFSET(1)` for the differ retry doesn't unwind
Manager's binding fully, OR that `α1`'s trail mark (taken at α1 entry) is being re-taken
on the retry path with the wrong trail state.

**Key question for F-226:** Does taking a new trail mark at `αN` on every entry (including
retries) cause the mark to be set AFTER unwind — meaning `UCALL_MARK_OFFSET(1)` captures
a mark of 0 on retry, so unwind(0) clears everything including Cashier?

**Proposed fix for F-226:** The per-ucall trail mark should only be taken on **fresh entry**
(when `edx==0`), not on resume. Add a guard:
```nasm
pl_main_sl_0_c0_α1:
    test    edx, edx
    jnz     .skip_mark           ; resume path — mark already set
    lea     rdi, [rel pl_trail]
    call    trail_mark_fn
    mov     [rbp - UCALL_MARK_OFFSET(1)], eax
.skip_mark:
```
Or alternatively: take the mark BEFORE the `αN` label (at γ_{N-1} time), so it captures
the trail state after ucall N-1 has bound its variables.

### Uncommitted state

All changes are in `src/backend/x64/emit_byrd_asm.c` only, not committed.
F-223 greek pass (`b0b190c`) is the last clean commit.

**Do NOT commit the current state** — rung09 corpus must still pass, need to verify.

### Next session action plan (F-226)

1. `bash setup.sh` (installs deps, builds sno2c)
2. Read snobol4x PLAN.md §26 (this section)
3. Fix the `αN` trail mark to only fire on fresh entry (`edx==0`), not resume
4. Test mini cross-product → must stay 9/9
5. Test `person(C), person(M), differ(C,M)` 3-ucall case → must give 6 pairs
6. Run rung09 corpus to confirm no regression (rungs 1–9 must still PASS)
7. Run rung10 puzzles
8. If PASS: commit `F-226: M-PROLOG-R10 ✅`, update dashboard, push both repos

### Trigger phrase for next session
**"playing with Prolog frontend"** → F-226 → pick up at snobol4x PLAN.md §26.
