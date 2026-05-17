# CHUNKS-step15a — E_TO + E_TO_BY producer migration

**Closed:** session #73, 2026-05-07.  First producer for `SM_BB_PUMP_SM`
and the gen-local opcodes.  Two Icon/Raku generator kinds — `E_TO`
(`lo to hi`) and `E_TO_BY` (`lo to hi by step`) — migrated from the
legacy `emit_push_expr + SM_BB_PUMP` shape to compiled SM chunks driven
by `SM_PUSH_CHUNK + SM_BB_PUMP_SM`.

## What landed

Two new comparison opcodes:

```
SM_ICMP_GT   ; pop r,l ; last_ok = (l.i > r.i) ; push nothing
SM_ICMP_LT   ; pop r,l ; last_ok = (l.i < r.i) ; push nothing
```

Both are pure `last_ok`-setting comparisons with no stack residue —
exactly the shape needed for SM_JUMP_S/SM_JUMP_F driven loop-exit tests
inside generator chunks.  `SM_ACOMP` was considered but rejected: it
pushes -1/0/1 (extra cleanup) and has no SM-interp handler today
(JIT-only by design); its semantics also include string-coerce paths
unnecessary for this site.  `SM_ICMP_*` are minimal, named after their
operand kind (integer-compare), and trivially mirrored in JIT-codegen
named-FATAL stubs (M5 territory).

In `sm_lower.c`, `case E_TO:` and `case E_TO_BY:` are broken out of the
shared "generators → emit_push_expr + SM_BB_PUMP" block at line 1033
into per-kind chunked lowerings.  The remaining un-migrated kinds
(`E_EVERY`, `E_SUSPEND`, `E_BANG_BINARY`, `E_LCONCAT`, `E_LIMIT`,
`E_RANDOM`, `E_SECTION`, `E_SECTION_MINUS`, `E_SECTION_PLUS`) keep the
legacy emission — these are the next CH-15 sub-rungs.

### Chunk shape: `E_TO(lo, hi)`

```
   SM_JUMP  skip_pc            ; jump over chunk body
entry_pc:
   SM_RESUME                   ; documentation hook for JIT
   <lower lo>                  ; evaluate lo bound
   SM_STORE_GLOCAL 0           ; glocal[0] = lo (value stays on stack)
   SM_POP                      ; discard
   <lower hi>                  ; evaluate hi bound
   SM_STORE_GLOCAL 1           ; glocal[1] = hi
   SM_POP
   SM_LOAD_GLOCAL 0            ; cur = lo
   SM_STORE_GLOCAL 2           ; glocal[2] = cur
   SM_POP
loop_pc:
   SM_LOAD_GLOCAL 2            ; push cur
   SM_LOAD_GLOCAL 1            ; push hi
   SM_ICMP_GT                  ; last_ok = (cur > hi)
   SM_JUMP_S exit_pc           ; exhausted → exit
   SM_LOAD_GLOCAL 2            ; push cur as yielded value
   SM_SUSPEND                  ; yield cur; resume after this
   SM_LOAD_GLOCAL 2            ; push cur
   SM_INCR 1                   ; cur + 1
   SM_STORE_GLOCAL 2           ; glocal[2] = cur+1
   SM_POP
   SM_JUMP loop_pc             ; next iteration
exit_pc:
   SM_PUSH_NULL                ; ω: generator exhausted
   SM_RETURN
skip_pc:
   SM_PUSH_CHUNK entry_pc, 0   ; push DT_E{slen=1, i=entry_pc}
   SM_BB_PUMP_SM               ; pop chunk, drive as generator
```

### Chunk shape: `E_TO_BY(lo, hi, step)`

Same skeleton as `E_TO` but with two refinements:

1. `step` lives in `glocal[3]`; `cur += step` uses `SM_LOAD_GLOCAL 3 +
   SM_ADD` (runtime step value, not an immediate).
2. The loop-exit test dispatches on the sign of `step` each iteration:
   `step < 0` → use `SM_ICMP_LT (cur < hi)`; otherwise `SM_ICMP_GT
   (cur > hi)`.  Mirrors `coro_bb_to_by` semantics in
   `frontend/icon/icon_gen.c:78`:
   ```
   if step > 0 && cur > hi → ω
   if step < 0 && cur < hi → ω
   ```

## Gen-local slot allocation (E_TO / E_TO_BY)

| Slot | E_TO   | E_TO_BY |
|------|--------|---------|
| 0    | lo     | lo      |
| 1    | hi     | hi      |
| 2    | cur    | cur     |
| 3    | —      | step    |

Slots 4–7 unused by these kinds; reserved for future generator kinds
(E_BANG_BINARY, E_LIMIT, E_SECTION_*).  `SM_GEN_LOCAL_MAX = 8` from
CH-14b accommodates them.

## Empirical proof

Producer-side: a Raku range expression `say 1..3` (which lowers to
`E_TO(1, 3)` in expression context) audits as
`SM_PUSH_CHUNK=1, SM_PUSH_EXPR=0` under `--interp` with
`SCRIP_CHUNKS_AUDIT=1`.  The chunk produces yields 1, 2, 3 in both
`--interp` and `--run`.

Why Icon `every i := 1 to 5 do …` does NOT audit a chunk emission today:
that path runs entirely through `coro_pump_proc_by_name(\"main\", …)` →
`coro_eval(main_body)` — pure IR walking inside the proc body.  The
Icon proc-body lowering through `sm_lower` is Step 17 territory.  Once
that lands, `every`-bodies that contain `1 to n` will hit the new
chunk emission automatically — the Raku chunk fires today proves the
producer side is fully wired.

## What did NOT change

- `coro_bb_to` and `coro_bb_to_by` (in `frontend/icon/icon_gen.c`)
  remain.  They are the IR-walking path used by `coro_eval` for
  Icon proc bodies.  Step 17 unwires that path; CH-15a does not.
- `case E_TO:` and `case E_TO_BY:` in `coro_runtime.c:1057` and
  `coro_runtime.c:1090` (the IR-walking E_TO/E_TO_BY) remain.  Same
  reason — Step 17 deletes them, not CH-15a.
- The legacy `emit_push_expr + SM_BB_PUMP` path remains for the
  un-migrated generator kinds in the case-block at sm_lower.c:1174.
  Each subsequent CH-15 sub-rung (CH-15b: E_EVERY/E_SUSPEND/…)
  migrates more kinds out of that block.

## Gates

```
smoke ×6     PASS  (snobol4 7/7, icon 5/5, prolog 5/5, raku 5/5,
                    snocone 5/5, rebus 4/4)
isolation gate                 PASS
csnobol4 Budne                 PASS=36 FAIL=114 SKIP=8 (≥34, baseline)
unified_broker                 PASS=49 FAIL=0
Icon corpus all-rungs          PASS=186 FAIL=47 XFAIL=30 TOTAL=263
                               (byte-identical to baseline 186/47/30)
Raku full suite                ir 29/0, sm 0/29, jit 0/29
                               (unchanged — same pre-existing baseline as
                               CH-13's note)
SNOBOL4 jit smoke              ir 139, sm 101, jit 101
                               (unchanged — same Step 7 baseline)
scrip_all_modes                PASS=2 FAIL=0
```

## Files touched

- `src/runtime/x86/sm_prog.h` — `SM_ICMP_GT`, `SM_ICMP_LT` enum entries.
- `src/runtime/x86/sm_prog.c` — opnames table extended.
- `src/runtime/x86/sm_interp.c` — `case SM_ICMP_GT:`, `case SM_ICMP_LT:` handlers.
- `src/runtime/x86/sm_codegen.c` — `h_icmp_gt`, `h_icmp_lt` named-FATAL stubs + handler-table entries.
- `src/runtime/x86/sm_lower.c` — `case E_TO:` and `case E_TO_BY:` carved out of the shared generator block; full chunk emission per shape above.

## Surviving `emit_push_expr` call sites in `sm_lower.c`

After CH-15a:
- Line 39 — helper definition itself.
- Lines ~1184 and ~1195 — Icon-generators-residual block and Prolog-backtracking block (both shrunk; the Icon block lost E_TO + E_TO_BY).

The `grep emit_push_expr src/runtime/x86/sm_lower.c | wc -l` count
drops by zero (still 3 call sites) but the case-fall-through count
into the Icon block drops from 10 kinds to 8 kinds.

## Next inline rung

CH-15b — pick the next kind(s) from the residual Icon block.  Easiest
candidates: `E_EVERY` (statement-level driver, no internal state) and
`E_SUSPEND` (forwards a yielded value).  Or bundle one of the
SECTION_* trio if the gen-local layout for them is similar.
