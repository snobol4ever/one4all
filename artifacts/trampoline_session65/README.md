# Session 65 Artifact — beauty_tramp_session65.c

## Changes since session 64
- fix(emit_byrd): E_VAR in pattern context → implicit deref, not epsilon
  - bare `nl`, `epsilon`, `Space` etc. in patterns were silently epsilon
  - match_pattern_at count: 9 → 122 (all legitimate dynamic refs)
  - infinite ARBNO loop eliminated
- fix(emit_byrd): ~ operator (E_COND) now emits Shift() for tree-stack pushes
  - Shift calls in generated C: 0 → 48
  - Reduce('Stmt',7) stack underflow was root cause of Internal Error

## Stats
- Lines: 28382
- md5: e4b6d731a9dc99d947c33e586f8bf17d
- gcc errors: 0

## Active symptom
- `printf 'START\n' | beauty_tramp_bin` → Internal Error
- `printf 'X = 1\n' | beauty_tramp_bin` → Parse Error
- Comments pass: `printf '* hello\n'` → `* hello` ✓

## Root cause under investigation
Shift() calls now emitted for ~ operators (48 total). Still hitting
Internal Error on START → Reduce('Stmt',7) stack underflow persists.
Next: add debug to count actual Shift calls fired for the START parse
path and confirm stack depth before Reduce fires.
