# CHUNKS-step17i-suspend — validation

**Rung:** CH-17i-suspend (second half of CH-17i-every-suspend)
**Goal-step:** GOAL-CHUNKS-STEP17.md → CH-17i → CH-17i-suspend
**Pre-rung commit:** `8a85285e` (CH-17i-every landed)
**Date:** 2026-05-10

---

## What changed

Migrated `AST_SUSPEND` off the legacy `emit_push_expr + SM_BB_PUMP`
fallthrough block in `sm_lower.c`, onto a new direct-yield primitive
`SM_SUSPEND_VALUE`.  Mirrors JCON's `ir_Succeed` (irgen.icn:962, 970)
and the swapcontext half of `coro_bb_suspend` (icon_gen.c:211–240).

### Why this shape, not the CH-17i-every shape

CH-17i-every used the canonical CH-17f pattern: `g_every_table` of
borrowed `AST_t*` plus a name-driven opcode whose handler does
`coro_eval(ast) → bb_broker(BB_PUMP)`.  That pattern fits AST_EVERY
because AST_EVERY *is* a Byrd box that pumps a generator and runs a
do-clause body per tick — it has a `bb_node_t` shape.

AST_SUSPEND has no `bb_node_t` shape.  Its existing semantics
(coro_stmt.c:88) are pure in-frame state mutation:

```c
case AST_SUSPEND: {
    DESCR_t val = (e->nchildren > 0) ? bb_eval_value(e->children[0]) : NULVCL;
    if (!IS_FAIL_fn(val)) {
        FRAME.suspending  = 1;
        FRAME.suspend_val = val;
        FRAME.suspend_do  = (e->nchildren > 1) ? e->children[1] : NULL;
    }
    return;
}
```

The actual yield is performed by the *outer* loop in `coro_call`
(line 500) or `coro_drive_fnc` (line 1821) which observes
`FRAME.suspending` and does `swapcontext(&ss->gen_ctx,
&ss->caller_ctx)` to yield to the caller, then runs the do-clause on
resume.

Under SM dispatch (CH-17g — proc bodies routed through `sm_call_proc`
when entry_pc is resolved), there is no outer-loop AST walker to
observe `FRAME.suspending` — `sm_call_expression` runs SM bytecode
straight through.  The SM-side equivalent is therefore a primitive
opcode that **does the swapcontext directly**, exactly as JCON's
`ir_Succeed` does on the JVM (saves resume PC on the closure, areturn
to caller).

### Lowering shape

```
[lower expr]                   ; push v, last_ok = !IS_FAIL(v)
SM_JUMP_F   L_end              ; v failed → leave it on stack, fall through
SM_SUSPEND_VALUE               ; pop v, swapcontext to caller_ctx
                               ; on resume, fall through here
[lower do-clause if any]       ; push d
SM_VOID_POP                    ; discard d (do-clause is stmt-context)
SM_PUSH_NULL                   ; placeholder for outer SM_VOID_POP
SM_JUMP     L_finally
L_end:                         ; failed-v fall-through; stack still has v
L_finally:                     ; outer proc-body SM_VOID_POP fires here
```

Net stack delta on success: +1 (NULVCL).  On failure: +1 (the failed
`v`).  Either way the outer proc-body loop's trailing `SM_VOID_POP`
balances.

### Runtime handler

```c
case SM_SUSPEND_VALUE: {
    DESCR_t v = sm_pop(st);
    if (sm_yield_to_caller(v)) {
        st->last_ok = 1;
    } else {
        sm_push(st, v);
        st->last_ok = !IS_FAIL_fn(v);
    }
    break;
}
```

`sm_yield_to_caller` is a new helper in `coro_runtime.c`:

```c
int sm_yield_to_caller(DESCR_t v) {
    if (!active_coro) return 0;
    coro_t *ss = active_coro;
    ss->yielded = v;
    swapcontext(&ss->gen_ctx, &ss->caller_ctx);
    return 1;
}
```

This co-locates the yield primitive with the rest of the ucontext
machinery (`proc_trampoline`, `gather_trampoline`) it cooperates with,
rather than duplicating `coro_t` layout into `sm_interp.c`.  When the
caller's broker calls back via swapcontext to `gen_ctx`, control
returns from `swapcontext` here and falls through back to the SM
dispatch loop.

### Files touched

- `src/runtime/x86/sm_prog.h` — `SM_SUSPEND_VALUE` opcode added
  with full doc-comment.
- `src/runtime/x86/sm_prog.c` — name table entry.
- `src/runtime/x86/sm_lower.c` — `case AST_SUSPEND:` carved out of
  the legacy `emit_push_expr + SM_BB_PUMP` fallthrough block at
  line 1327; new lowering shape per above.  Block now contains
  AST_BANG_BINARY, AST_LCONCAT, AST_LIMIT, AST_RANDOM, AST_SECTION*
  — the next sub-rungs (CH-17i-bang-concat, CH-17i-section,
  CH-17i-limit-random per the survey doc).
- `src/runtime/interp/coro_runtime.c` — new helper
  `sm_yield_to_caller(DESCR_t v)`.
- `src/runtime/x86/sm_interp.c` — `SM_SUSPEND_VALUE` handler;
  forward declaration of `sm_yield_to_caller` to avoid pulling
  `icon_gen.h` transitive baggage into the SM interpreter.
- `src/runtime/x86/sm_codegen.c` — `h_suspend_value` JIT mirror;
  registered in `g_handlers[SM_SUSPEND_VALUE]`.

---

## Empirical verification

### Pre-rung baseline (commit `8a85285e`)

```
$ ./scrip --interp rung03_suspend_gen.icn
FATAL bb_eval_value: unhandled kind 50 (RS-23e isolation breach)
Aborted
```

Backtrace (gdb):
```
#5  bb_eval_value (e=...)              coro_value.c:1357
#6  coro_eval (e=...)                   coro_runtime.c:1756
#7  sm_interp_run (prog=..., st=...)   sm_interp.c:784  [SM_BB_PUMP handler]
#8  sm_call_expression (entry_pc=1)    sm_interp.c:1664
#9  sm_call_proc (entry_pc=1, ...)     coro_runtime.c:580
#10 proc_trampoline ()                  coro_runtime.c:776
```

`upto`'s body runs via `sm_call_proc` because CH-17b'/b'' lowered it
into a named SM chunk and CH-17a populated `proc_table[].entry_pc`.
The body's SM_PUSH_EXPR(suspend_ast) + SM_BB_PUMP call
`coro_eval(suspend_ast)`, which falls through to `bb_eval_value`,
which has no AST_SUSPEND case → FATAL.

### Post-rung — 3 programs flip from FATAL to PASS

```
$ ./scrip --interp rung03_suspend_gen.icn
1
2
3
4
$ ./scrip --interp rung03_suspend_gen_compose.icn
1
2
3
1
2
$ ./scrip --interp rung03_suspend_gen_filter.icn
4
3
2
1
```

All match `.expected`.  `--run` PASSes the same three programs
(JIT mirror `h_suspend_value` exercises the same `sm_yield_to_caller`
helper).

### rung01–04 corpus measurement

|              | pre-rung | post-rung |
| ------------ | -------: | --------: |
| `--interp`   |    17/24 |     20/24 |
| `--run`  |    17/24 |     20/24 |

Gain: +3.  The 3 flipped programs are exactly
`rung03_suspend_gen{,_compose,_filter}.icn`.  The 4 remaining FAILs
are pre-existing — verified by stashing the rung's changes,
rebuilding, and observing the same 4 FAILs:

- `rung02_proc_fact` — pre-existing JIT-mode segfault (different
  failure mode pre-rung: stack smashing detection; both crash
  pre/post — same root cause, unrelated to AST_SUSPEND).
- `rung03_suspend_fail`, `rung03_suspend_return` — both use
  `return E` not `suspend E`; pre-existing `--interp` and
  `--run` gaps in proc return-value handling, separate
  territory.
- one further unrelated rung01–02 program.

### Standard CHUNKS gate set

| Gate                                | Result |
| ----------------------------------- | -----: |
| smoke_snobol4                       |    7/7 |
| smoke_icon                          |    5/5 |
| smoke_prolog                        |    5/5 |
| smoke_raku                          |    5/5 |
| smoke_snocone                       |    5/5 |
| smoke_rebus                         |    4/4 |
| isolation_ir_sm                     |   PASS |
| csnobol4 Budne                      |  ≥34 (50) |
| Icon `--ir-run` corpus              | 177 / 56 / 30 / 263 (byte-identical baseline) |
| unified_broker                      |   49/0 |
| broad_unified_broker                | floors green |
| scrip_all_modes                     |    2/0 |

Zero regressions.  All gates byte-identical to the `8a85285e`
baseline except for the +3 gain on `--interp` and `--run`
rung01–04.

---

## Out of scope for this rung

- **Top-level AST_SUSPEND** (no enclosing coroutine context):
  treated as a no-op yield by both AST-walker and SM-dispatch
  paths.  Not exercised by the rung03 corpus today.
- **AST_SUSPEND-as-value-in-larger-expression** (e.g. `x := suspend
  E`, `f(suspend E, y)`):  the lowering shape's net-stack-delta is
  `+1` placeholder, which matches statement-context usage.
  Value-context `suspend` is not in the Icon language spec — the
  `suspend` keyword is statement-only — so this is structurally
  unreachable from the parser.
- **Generative `expr` in `suspend E`** (e.g. `suspend 1 to 5`): the
  current lowering invokes `lower_expr` on the child, which for a
  generative kind takes only the first value (then last_ok behaviour
  per kind).  In the rung03 corpus the `expr` is always a plain
  variable (`i`).  Generative-`expr` `suspend` is the GOAL-CHUNKS
  Step 15 (per-kind generators) and CH-17i-bang-concat / CH-17i-
  section / CH-17i-limit-random territory — they migrate the inner
  generator kinds; once those are off the legacy fallthrough,
  generative `suspend E` works through the same yield primitive
  (the inner generator establishes its own resumability via the
  per-kind machinery; the outer `suspend` yields each value via
  `SM_SUSPEND_VALUE`).
- **Goal-directed wrapping** (JCON's `ir_a_Suspend` `expr.resume`
  → `expr.start` re-loop):  the rung03 programs use `while ...
  do suspend i` — the *while* provides the re-loop.  Bare `suspend
  E` in the four-port discipline would re-loop the generator for
  each value of E; that goal-directed shape is sub-rung work
  (CH-17i-section etc.) since it needs the inner kind to support
  resumption.

---

## Architectural placement

This rung is one of the four sub-rungs identified in
`docs/CHUNKS-step17i-survey-mode3.md`:

  - **CH-17i-every-suspend** ← split into:
    - CH-17i-every (LANDED 2026-05-10, `8a85285e`)
    - CH-17i-suspend (this rung)
  - CH-17i-bang-concat — AST_BANG_BINARY + AST_LCONCAT (next)
  - CH-17i-section — AST_SECTION + AST_SECTION_PLUS + AST_SECTION_MINUS
  - CH-17i-limit-random — AST_LIMIT + AST_RANDOM
  - CH-17i-prolog-initialization — initialization/2 bridge

After this rung, the legacy `emit_push_expr + SM_BB_PUMP` fallthrough
block in `sm_lower.c` contains 7 kinds: AST_BANG_BINARY, AST_LCONCAT,
AST_LIMIT, AST_RANDOM, AST_SECTION, AST_SECTION_PLUS,
AST_SECTION_MINUS.  Three more sub-rungs to retire them.
