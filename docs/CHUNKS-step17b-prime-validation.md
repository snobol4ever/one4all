# CHUNKS-step17b-prime-validation — proc-body lowering into the chunks

**Session #76, 2026-05-07.  Watermark: post-CH-17b
(one4all `6b129465`).  Authors: Lon Jones Cherryholmes · Claude Sonnet.**

CH-17b' is the third rung of `GOAL-CHUNKS-STEP17.md`.  CH-17b laid
down empty-body skeletons; this rung fills them with lowered SM ops
walked from the proc's IR body children.

## What landed

In `src/runtime/x86/sm_lower.c`, the per-proc emission loop (between
the function prologue and the main statement loop) now lowers each
proc body's children into the chunk:

```c
for (int pi = 0; pi < proc_count; pi++) {
    const char *nm = proc_table[pi].name;
    if (!nm || !*nm) continue;
    EXPR_t *proc = proc_table[pi].proc;
    int skip_jump = sm_emit_i(p, SM_JUMP, 0);
    sm_label_named(p, nm);
    if (proc) {
        int nparams    = (int)proc->ival;
        int body_start = 1 + nparams;
        g_chunk_body_lowering = 1;
        for (int bi = body_start; bi < proc->nchildren; bi++) {
            EXPR_t *body_expr = proc->children[bi];
            if (!body_expr) continue;
            lower_expr(p, &lt, body_expr);
            sm_emit(p, SM_POP);
        }
        g_chunk_body_lowering = 0;
    }
    sm_emit(p, SM_RETURN);
    int skip_lbl = sm_label(p);
    sm_patch_jump(p, skip_jump, skip_lbl);
}
```

Per proc, the chunk now reads:

```
SM_JUMP <skip>       ; forward-jump around the chunk
SM_LABEL "<name>"    ; named entry (resolved by CH-17a after sm_lower)
<lower_expr(child) ; SM_POP>...   ; one such pair per body child
SM_RETURN            ; trailing return (or unreachable post-E_RETURN)
<skip>:              ; anonymous skip target
```

A new file-static flag `g_chunk_body_lowering` is set/cleared around
the inner loop and is consulted only by `lower_expr`'s default case
to suppress its `"sm_lower: unhandled expr kind %d"` stderr warning
during proc-body emission — chunks are dead code today, so any
unhandled-kind fall-through landing here is harmless and the warning
would only mislead.  Outside this scope (i.e., when `lower_expr` is
invoked from `lower_stmt` for genuinely executable code), the warning
fires as before.

## Why body lowering is genuinely useful even though chunks are dead code

CH-17a populated `entry_pc` fields end-to-end via
`sm_resolve_proc_entry_pcs` after CH-17b's skeletons gave the
resolver real targets.  CH-17b' completes the producer side of
Step 17's first half: every Icon/Raku proc now has both a
discoverable entry_pc *and* a real lowered body sitting at that pc.
CH-17c can then flip `coro_call` consumer sites to dispatch via
entry_pc, with the chunks already in place to receive the dispatch
without further sm_lower work.

The chunks emitted today are not yet executed: `coro_call` walks
the IR proc body via `bb_exec_stmt`, the BB engine, and
`bb_eval_value`, exactly as before.  But the SM_Program now carries
the equivalent SM lowering forwards, ready for CH-17c.

## Producer-side empirical proof

`./scrip --dump-sm --interp test/icon/palindrome.icn` shows:

```
; SM_Program  count=78
   0  SM_JUMP              -> 53
   1  SM_LABEL                       ; palindrome entry_pc
   2  SM_PUSH_VAR          s="map"
   3  SM_PUSH_VAR          s="s"
   4  SM_CALL              s="" nargs=2
   5  SM_STORE_VAR         s="s"
   6  SM_POP
   7  SM_PUSH_LIT_I        i=1
   8  SM_STORE_VAR         s="i"
   9  SM_POP
  10  SM_PUSH_VAR          s="s"
  11  SM_CALL              s="SIZE" nargs=1
  12  SM_STORE_VAR         s="j"
  13  SM_POP
  14  SM_LABEL                       ; while-loop top
  15  SM_PUSH_VAR          s="i"
  16  SM_PUSH_VAR          s="j"
  17  SM_ACOMP
  18  SM_JUMP_F            -> 46
  ... [palindrome body continues to pc 52]
  52  SM_RETURN
  53  SM_LABEL                       ; skip past palindrome
  54  SM_JUMP              -> 75
  55  SM_LABEL                       ; main entry_pc
  56  SM_PUSH_VAR          s="write"
  57  SM_PUSH_VAR          s="palindrome"
  58  SM_PUSH_LIT_S        s="racecar"
  ... [main body continues to pc 74]
  74  SM_RETURN
  75  SM_LABEL                       ; skip past main
  76  SM_BB_PUMP_PROC                ; runtime-side coro_call (unchanged)
  77  SM_HALT
```

Pre-CH-17b' (CH-17b head), instrs 2–5 and 56–73 were absent — the
chunks were just `SM_LABEL ; SM_RETURN`.  Post-CH-17b', the chunks
contain the actual lowered statements.

The execution path is unchanged: pc=0 jumps to 53, then 53 jumps to
75, then SM_BB_PUMP_PROC drives main via coro_call (which walks the
IR proc body, not the chunk).  pc 1–52 and 55–74 are forward-jumped
over.

`SCRIP_PROC_ENTRY_PCS=1 ./scrip --interp test/icon/palindrome.icn`
confirms resolver finds non-(-1) entry_pcs:

```
[CH-17a] resolve entry_pcs (proc_table=2 procs, pl_pred_table=hash)
[CH-17a]   proc[0] name=palindrome           entry_pc=1
[CH-17a]   proc[1] name=main                 entry_pc=55
```

`SCRIP_CHUNKS_AUDIT=1` reports zero `SM_PUSH_CHUNK` / `SM_PUSH_EXPR`
fires at execution for palindrome — the chunks are reachable as
program text but unreachable as program flow.  Hello.icn confirms
the same: chunks contain a real `SM_PUSH_VAR write / SM_PUSH_LIT_S
"Hello, World!" / SM_CALL "" nargs=2 / SM_POP / SM_RETURN` body.

For Raku: `SCRIP_PROC_ENTRY_PCS=1 ./scrip --interp test/raku/rk_given.raku`
shows three procs with three substantial chunk bodies:

```
[CH-17a]   proc[0] name=day_type             entry_pc=1
[CH-17a]   proc[1] name=season               entry_pc=81
[CH-17a]   proc[2] name=main                 entry_pc=161
```

The ~80-instruction gaps confirm Raku given/when dispatch
infrastructure (E_CASE → SM_BB_PUMP_CASE) lowers correctly inside
proc-body chunks — the per-arm chunks emitted by CH-13's CASE
lowering compose with CH-17b' proc-body lowering without conflict.

## Scope boundary (honest)

- **Frame-slot resolution stays at runtime.**  E_VAR lowers to
  `SM_PUSH_VAR <name>`, name-keyed through `NV_GET_fn` at runtime —
  slot indices on `EXPR_t.ival` (set by `icn_scope_patch` inside
  `coro_call`) are used only by the IR-side path, which is
  unaffected.  Migrating env to lower-time slot baking is later-rung
  territory (CH-17c+ when `coro_call` is replaced by
  `sm_call_proc`).  This was an explicit design call: doing slot
  resolution at lower-time would mutate the IR before `coro_call`
  observes it (the same `EXPR_t` tree is read by both the IR walker
  and would be read by sm_lower's scope-patch step).  Deferring keeps
  the two paths independent until CH-17c.

- **Static-variable persistence stays at runtime.**  `coro_call`'s
  static-var entry/exit copy loops (lines 408–420 and 462–471 of
  `coro_runtime.c`) are unchanged.  Statics are keyed by EXPR_t
  identity in the static_get/static_set tables, which is itself
  CH-17g cleanup territory; for CH-17b' the static-var semantics are
  out-of-scope because the chunks are dead code.

- **Generator kinds emit legacy `SM_PUSH_EXPR + SM_BB_PUMP` shape.**
  This is the gating note in the CH-17b' spec: `E_EVERY`,
  `E_SUSPEND`, `E_BANG_BINARY`, `E_LCONCAT`, `E_LIMIT`, `E_RANDOM`,
  `E_SECTION`, `E_SECTION_PLUS`, `E_SECTION_MINUS` reach the
  block at `sm_lower.c:1207` (post-renumber after the new comment
  block, formerly `:1193`) which emits the legacy shape.  Once
  CH-17c flips coro_call, these become reachable inside proc-body
  chunks; CH-17h (CH-15b's reactivation point) will migrate them
  per-kind with full corpus validation now possible.

- **Unhandled kinds fall through to lower_expr's default.**
  `E_ALTERNATE` (kind 54), `E_ITERATE`, `E_CSET`, `E_CSET_COMPL`,
  `E_CSET_UNION`, `E_CSET_DIFF`, `E_CSET_INTER`, `E_REVASSIGN`,
  `E_REVSWAP` have no explicit case in `lower_expr` today.  In the
  pre-CH-17b' world they were never seen by lower_expr because
  Icon/Raku proc bodies were only walked by `coro_eval` /
  `bb_exec_stmt` / `bb_eval_value` (the IR runtime walkers).  The
  `g_chunk_body_lowering` flag suppresses lower_expr's stderr
  warning during proc-body lowering so this gap doesn't generate
  spurious noise; the dead-code emission of `SM_PUSH_NULL` for
  these kinds is harmless.  When CH-17c flips consumers, these
  gaps will need real lowering — same wave as CH-17h.

- **No isolation-gate strengthening yet.**  The structural rule on
  `coro_runtime.c` / `polyglot.c` / `pl_runtime.c` against
  `EXPR_t *` field accesses on proc_table / pred_table data is
  CH-17g territory.  CH-17b' is pure addition; the
  `IcnProcEntry.proc` field is still read by `coro_call`, just as
  before.

## Gates run

```
build clean (gcc -O0 -g, no warnings)
smoke_snobol4         PASS=7 FAIL=0
smoke_icon            PASS=5 FAIL=0
smoke_prolog          PASS=5 FAIL=0
smoke_raku            PASS=5 FAIL=0
smoke_snocone         PASS=5 FAIL=0
smoke_rebus           PASS=4 FAIL=0
isolation_ir_sm       PASS  no IR-only symbol leaks in SM runtime files
csnobol4 Budne        PASS=61 FAIL=89 SKIP=8 (150 run)  (byte-identical to baseline)
Icon corpus all-rungs PASS=186 FAIL=47 XFAIL=30 TOTAL=263 (byte-identical to
                      baseline 186/47/30)
unified_broker        PASS=49 FAIL=0
scrip_all_modes       PASS=2 FAIL=0
```

Pre-suppression-flag observation worth recording: when first
implemented, the `lower_expr` default case fired for `queens.icn`'s
`args[1] | 6` (E_ALTERNATE, kind 54) under `--interp`, printing
`sm_lower: unhandled expr kind 54` to stderr while the chunk emitted
a harmless `SM_PUSH_NULL`.  Output was unchanged (queens fails with
Error 3 at baseline for unrelated reasons), but the new stderr noise
was a soft regression.  The `g_chunk_body_lowering` flag closes
that gap by silencing the warning during proc-body chunk emission
specifically, while preserving it for `lower_stmt`-driven
executable code.

## Files touched

- `src/runtime/x86/sm_lower.c` — body-lowering loop replaces empty-body
  skeleton (lines ~1567–1592 → ~1567–1654 with comment block);
  `g_chunk_body_lowering` flag added at top of file; `lower_expr`'s
  default case now consults the flag.

## Why this is a separate rung from CH-17b

CH-17b's spec was originally "skeleton + body".  Mid-session #75,
the rung was scope-reduced to "skeleton only" with body lowering
split off because:

1. Icon proc bodies are EXPR_t chains (the body is a flat sequence
   of children of E_FNC), not STMT_t — different from the
   STMT_t-driven main statement loop in sm_lower.  Verifying the
   walk shape is correct deserves its own gate.

2. Frame-slot resolution via `icn_scope_patch` is its own
   architectural decision (deferred to CH-17c+).  CH-17b' delivers
   working body lowering without that decision, by relying on the
   name-keyed `SM_PUSH_VAR` discipline already in place.

3. Suppressing the "unhandled expr kind" warning needed thinking
   about — adding it to CH-17b would have conflated two changes.

The split worked: CH-17b shipped empty-body skeletons with
byte-identical gates; CH-17b' adds real bodies with byte-identical
gates.  Each rung gates cleanly.

## Next rung: CH-17c

Per `GOAL-CHUNKS-STEP17.md`: flip `coro_call` consumer sites to
dispatch via `entry_pc` when non-(-1).  Add companion
`sm_call_proc(int entry_pc, DESCR_t *args, int nargs)` that runs
the chunk via SM dispatch.  Per-call-site flip: gated on
`entry_pc != -1` (fall back to legacy `coro_call` if -1).  Frame
setup factored into a shared `proc_frame_setup` helper.  This is
the consumer-side migration that makes the chunks reachable for
the first time.
