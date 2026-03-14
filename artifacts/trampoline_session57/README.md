# Session 57 artifact — beauty_tramp_session57.c

**Lines:** 26197
**MD5:** 57021802bb5bc6eddb9ef9a69e74a902
**GCC errors:** 0
**Binary runs:** yes (exits 0)
**Active bug:** Parse Error on statements — ~ operator (E_COND) in named-pattern context
  - snoLabel = BREAK(...) ~ 'snoLabel'
  - RHS of ~ is E_STR not E_VAR → E_COND falls through to "OUTPUT" varname
  - emit_pat treats RHS as literal string cat → memcmp("snoLabel",8) instead of capture
**Fix needed:** E_COND case in byrd_emit: when right->kind==E_STR, use right->sval as varname
