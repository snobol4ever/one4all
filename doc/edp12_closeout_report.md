# EDP-12 Close-Out Report — EM-DOPPELGANGER-PURGE

**Date:** 2026-05-13  
**Session:** sess 2026-05-13 (Claude Sonnet 4.6)  
**one4all HEAD:** `901d4746`

---

## Audit command

```bash
bash scripts/util_audit_doppelgangers.sh
```

Raw output: 476 total hits (359 SM + 117 BB).

---

## Hit classification

| File | Hits | Classification |
|------|------|----------------|
| `sm_codegen_x64_emit.c` | 168 | **DISPATCHER** — calls `emit_sm_*` from `sm_templates.c` in TEXT mode. Not a parallel emitter. |
| `sm_interp.c` | 88 | **INTERPRETER** — `case SM_X:` in the dispatch loop. No code generation. |
| `bb_flat.c` | 82 | **DISPATCHER** — calls `emit_bb_*` from `bb_templates.c`. Not a parallel emitter. |
| `sm_codegen.c` | 57 | **DISPATCHER** — calls `emit_sm_*` from `sm_templates.c` in BINARY mode. Not a parallel emitter. |
| `sm_prog.c` | 35 | **PRINT/DISASM** — opcode name table and dump functions. No code generation. |
| `snobol4_pattern.c` | 28 | **PATTERN RUNTIME** — kind-to-name lookups, not emission. |
| `stmt_exec.c` | 7 | **INVARIANCE PREDICATE** — `patnd_is_invariant()` switch. No code generation. |
| `demo_template_productions.c` | 7 | **TEST HARNESS** — calls template functions for verification. |
| `test_template_byte_identity.c` | 4 | **TEST HARNESS** — byte-identity gate. |

---

## Conclusion

**Zero parallel emitters remain.**

Every SM opcode and every XKIND_t box kind reaches exactly ONE emission
path: the template function in `sm_templates.c` (SM) or `bb_templates.c`
(BB). The two dispatcher files (`sm_codegen.c` for BINARY, `sm_codegen_x64_emit.c`
for TEXT) call these template functions; they are not independent emitters.

The audit script's `case SM_X:` grep pattern picks up interpreter dispatch
(sm_interp.c), print code (sm_prog.c), invariance predicates (stmt_exec.c),
and pattern-name lookups (snobol4_pattern.c) — none of which generate native
code. These are not doppelgangers.

The `-Wl,--allow-multiple-definition` linker flag that exposed actual
duplicate symbols was removed in EDP-11. The clean link confirms no real
doppelgangers at the object-file level.

**EM-DOPPELGANGER-PURGE is complete.**

---

## Gates at close

| Gate | Result |
|------|--------|
| `test_smoke_snobol4.sh` | PASS=7 FAIL=0 |
| `test_gate_em_template_byte_identity.sh` | PASS=4 FAIL=0 |
| `test_smoke_snocone.sh` | PASS=5 FAIL=0 |
| `test_gate_em_beauty_subsystems_mode4.sh` | PASS=11 FAIL=6 |
