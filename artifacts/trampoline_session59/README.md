# beauty_tramp_session59.c

## Changes since session 57
- `a3ea9ef` — Technique 1 struct-passing: fix static re-entrancy bug in all named pattern functions
  - `pat_X_t` struct typedef per named pattern; heap-allocated on entry==0
  - `#define field z->field` aliases so code body is unchanged
  - child frame pointers (`deref_N_z`) in parent struct for E_DEREF calls
  - `byrd_emit_named_typedecls` emits struct fwdecls before function fwdecls

## Stats
- Lines: 27483
- md5: c5ccf03fdeff1cfffb7ec6eaf986bc85
- gcc errors: 0
- gcc warnings: suppressed with -w

## Active bug
`emit_imm` (`$ capture`) stores the captured span into a local `str_t var_nl`
inside the named pattern function body, but never calls `var_set("nl", ...)`.
Result: `var_get("nl")` returns empty in `pat_Label`'s `BREAK(' ' tab nl ';')`,
causing bare label lines like `START` to fail with Parse Error.

## Symptom
```
printf 'X = 1\n'    | beauty_tramp_bin   → X = 1        ✅
printf '* comment\n'| beauty_tramp_bin   → * comment    ✅
printf 'START\n'    | beauty_tramp_bin   → Parse Error  ❌
```
