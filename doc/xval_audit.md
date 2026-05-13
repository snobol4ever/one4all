# EM-XVAL-DESCR EXVAL-1 — bb_box_fn Return Value Audit

**Date:** sess 2026-05-12 (Claude Sonnet 4.6)
**One4all HEAD:** a2b65fb9
**Goal:** Identify every `bb_box_fn` call site that assumes SNOBOL4 σ/δ layout
(i.e., reads `.s` / `.slen` from the returned `DESCR_t` without checking `.v`).

---

## Summary

Four files call `bb_box_fn` or consume its return value via a `body_fn` callback:
- `src/runtime/x86/stmt_exec.c`
- `src/runtime/rt/rt.c`
- `src/runtime/x86/bb_broker.c`
- `src/runtime/x86/bb_flat.c` (indirect, via flat blob execution)

---

## 1. `stmt_exec.c`

### 1a. `scan_body_fn_u9` (line 736–740) — **SNOBOL4-SPECIFIC σ/δ assumption**

```c
static void scan_body_fn_u9(DESCR_t val, void *arg) {
    scan_result_t *r = (scan_result_t *)arg;
    r->end   = Δ;
    r->start = Δ - (int)val.slen;   /* scan position = end − match length */
}
```

**Assumption:** `val.slen` is the match length (number of characters consumed).
This is the SNOBOL4 `descr_match(σ, δ)` convention where `slen = δ = match length`.

**Verdict:** SNOBOL4-specific. For Prolog (boolean result) or Icon (arbitrary value),
`val.slen` has no meaning as a cursor delta. This site **must** check `val.v` or use
a frontend-specific body_fn.

**Classification: SNOBOL4-ONLY**

### 1b. `bb_usercall` return value (line 455–491) — **SNOBOL4-specific**

```c
UC = descr_match(Σ + Δ, 0);        goto UC_γ;
...
UC = descr_match(Σ + Δ, rl);
```

`UC` is constructed as a SNOBOL4 match span. The `UC_γ` return passes `UC` to the
caller as the box return value. This is correct for SNOBOL4 but would need a
different shape for Icon/Prolog.

**Classification: SNOBOL4-ONLY**

### 1c. `bb_deferred_var` (line 677–687) — **type-agnostic (passes through)**

```c
DVAR = ζ->child_fn(ζ->child_state, α);
if (IS_FAIL_fn(DVAR)) goto DVAR_ω;
...
DVAR_γ: return DVAR;
```

Only checks `IS_FAIL_fn(DVAR)` — does not read `.s` or `.slen`.
**Classification: TYPE-AGNOSTIC ✓**

### 1d. `exec_stmt` Phase 3 match result (lines 917–922)

```c
int ticks = bb_broker(root, BB_SCAN, scan_body_fn_u9, &scan_res);
...
match_start = scan_res.start;
match_end   = scan_res.end;
```

The σ/δ assumption is in `scan_body_fn_u9` (1a above). The call site itself is
type-agnostic — it just reads the pre-computed `scan_res.{start,end}`.
**Classification: DELEGATED TO scan_body_fn_u9**

---

## 2. `rt.c` — `rt_bb_*` functions

### 2a. `rt_bb_arb` (line 1368–1373) — **SNOBOL4-specific**

```c
if (port == 0) { ζ->count = 0; ζ->start = Δ; return descr_match(Σ+Δ, 0); }
...
DESCR_t ARB = descr_match(Σ+Δ, ζ->count); Δ += ζ->count;
return ARB;
```

Returns `descr_match(σ, δ)` — pure SNOBOL4 match span.
**Classification: SNOBOL4-ONLY**

### 2b. `rt_bb_len` (line ~1380) — **SNOBOL4-specific**

```c
DESCR_t LEN = descr_match(Σ+Δ, ζ->n); Δ += ζ->n;
```
**Classification: SNOBOL4-ONLY**

### 2c. `rt_bb_tab` / `rt_bb_rtab` (lines ~1393, ~1405) — **SNOBOL4-specific**

Both construct `descr_match(Σ+Δ, advance)` and advance Δ.
**Classification: SNOBOL4-ONLY**

### 2d. `rt_bb_bal` / `rt_bb_breakx` (lines ~1424, ~1438) — **SNOBOL4-specific**

Both return `descr_match(Σ+Δ, ζ->δ)` and advance Δ.
**Classification: SNOBOL4-ONLY**

### 2e. `rt_bb_span` / `rt_bb_brk` / `rt_bb_any` / `rt_bb_notany` (lines ~1455–1489) — **SNOBOL4-specific**

All return `descr_match(Σ+Δ, n)` and advance Δ by n or 1.
**Classification: SNOBOL4-ONLY**

### 2f. `rt_bb_arbno` (lines ~1507–1513) — **SNOBOL4-specific**

```c
fr->matched = descr_match(Σ+Δ, 0); fr->start = Δ;
...
ARBNO = descr_match_cat(fr->matched, br);
```

Uses `descr_match_cat` to concatenate match spans. Entirely SNOBOL4 σ/δ.
**Classification: SNOBOL4-ONLY**

### 2g. `rt_bb_cap` (lines ~1562–1592) — **SNOBOL4-specific**

```c
char *s = (char *)GC_MALLOC(cr.slen + 1);
if (cr.s && cr.slen > 0) memcpy(s, cr.s, (size_t)cr.slen);
s[cr.slen] = '\0';
...
(void) NAME_push(&ζ->name, cr.s, (int)cr.slen);
```

Reads `.s` and `.slen` directly from child box return without checking `.v`.
Assumes SNOBOL4 string match result.
**Classification: SNOBOL4-ONLY (most critical)**

### 2h. `rt_bb_atp` (line ~1550)

```c
return descr_match(Σ+Δ, 0);
```

Always succeeds with a zero-length match. Type-agnostic success signal.
**Classification: SNOBOL4-ONLY (but trivial)**

### 2i. `rt_bb_rem` — (line ~1550) 

Returns `descr_match(Σ+Δ, 0)` — zero length match at current position.
**Classification: SNOBOL4-ONLY (trivial)**

---

## 3. `bb_broker.c` — **TYPE-AGNOSTIC ✓**

```c
DESCR_t val = fn(root.ζ, α);
if (!IS_FAIL_fn(val)) { ... }
```

`bb_broker` itself only calls `IS_FAIL_fn()` on the return value — it never reads
`.s` or `.slen`. The `body_fn` callback receives the full `DESCR_t` and is
responsible for interpreting it.

- `BB_SCAN`: delegates interpretation to `scan_body_fn_u9` → SNOBOL4-specific (above)
- `BB_PUMP`: delegates to Icon body_fn → Icon-specific (checks `.v` in coro_value.c)
- `BB_ONCE`: delegates to Prolog body_fn → Prolog-specific (boolean interpretation)

**Classification: TYPE-AGNOSTIC ✓ (per-mode interpretation in body_fn)**

---

## 4. `bb_flat.c` — **BINARY MODE: no return-value read**

In flat/live mode, the blob runs to completion via inline jmps. The result is
the final state of Δ (cursor position), not a `DESCR_t`. The broker calls the
flat blob as `fn(NULL, α)` and reads `IS_FAIL_fn(val)` only.

**Classification: TYPE-AGNOSTIC ✓ (Δ carries match result, not return DESCR_t fields)**

---

## Classification Summary

| Site | File | Assumption | Action needed for EXVAL-2/3 |
|------|------|-----------|--------------------------|
| `scan_body_fn_u9` | stmt_exec.c:736 | `val.slen` = match length | Add `val.v == DT_S` check; for non-SNOBOL4 use `val.i` (Icon int) or `val.v != DT_FAIL` (Prolog bool) |
| `bb_usercall` UC construction | stmt_exec.c:466–491 | constructs SNOBOL4 spans | SNOBOL4-only path; no change needed unless multi-frontend usercall added |
| ALL `rt_bb_*` functions | rt.c:1368–1592 | construct + return `descr_match()` | These ARE the SNOBOL4 implementations; correct by definition |
| `rt_bb_cap` | rt.c:1579–1581 | reads `.s`/`.slen` from child result | Must check `cr.v == DT_S` before accessing `.s`/`.slen` |
| `bb_broker` itself | bb_broker.c:44–79 | only `IS_FAIL_fn()` | Already type-agnostic ✓ |
| Flat blobs | bb_flat.c | Δ-based; no `.s`/`.slen` read | Already type-agnostic ✓ |

---

## Key Finding for EXVAL-2

The most critical site requiring generalization in EXVAL-2/3 is:

1. **`scan_body_fn_u9`** — the SNOBOL4 match callback. It assumes `val.slen`
   is the match length. For multi-frontend correctness, `exec_stmt`'s Phase 3
   scan body needs to dispatch on `val.v`:
   - `DT_S`: SNOBOL4 span — use `val.slen` for cursor delta
   - `DT_I` / `DT_BOOL`: Icon/Prolog success — use `Δ` directly (cursor already advanced by box)
   - `DT_FAIL` / `IS_FAIL_fn`: failure — already handled

2. **`rt_bb_cap`** — reads `.s`/`.slen` without type check. Must add `cr.v == DT_S`
   guard before accessing those fields.

All other `rt_bb_*` functions are SNOBOL4-specific by design (they construct SNOBOL4
match spans and advance Δ). These do NOT need generalization for Prolog/Icon because
Prolog/Icon use different BB box implementations entirely (Icon BB boxes return Icon
values via `bb_eval_value`; Prolog boxes return boolean `DESCR_t` from `pl_broker.c`).

