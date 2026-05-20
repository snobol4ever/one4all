/* bb_pl.c — Prolog BB templates (EC-UNI-13(e)).
   Four BB kinds — BB_PL_ARITH, BB_PL_ATOM, BB_PL_BUILTIN, BB_PL_CALL — have no native
   emission today.  No frontend lowers a Prolog BB graph to x86/JVM/JS/NET/WASM; Prolog
   execution is handled at runtime by IR_exec_node in src/lower/ir_exec.c, not by the
   emitter.  The four templates below are honest no-op stubs across all five backends,
   mirroring 13(d)'s sm_bb_calls.c for the JVM/JS/NET/WASM arms — except here the x86
   arm is also a no-op because there is no existing emit_bb_pl_*_dispatch helper in
   emit_sm.c to delegate to.  The HQ note (\"Bodies pulled from the four case BB_PL_*:
   arms in emit_sm.c\") was stale: the only case BB_PL_* labels in the emitter live in
   the type-classification predicate pl_ir_kind_uses_sval, not in emission code.
   Phase B will fill these arms when frontends start emitting Prolog BB graphs as
   native code.  Until then, emit_bb_node's default arm correctly flags any BB_PL_*
   kind reaching the dispatcher with a \"; [emit_bb_node: kind=%d unhandled]\" comment;
   bb_pl.c is not yet wired into that switch and intentionally so. */
#include "bb_template_common.h"
/*----------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------*/
void bb_pl_arith(void)   { if (IS_X86) return; if (IS_JVM) return; if (IS_JS) return; if (IS_NET) return; if (IS_WASM) return; }
/*----------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------*/
void bb_pl_atom(void)    { if (IS_X86) return; if (IS_JVM) return; if (IS_JS) return; if (IS_NET) return; if (IS_WASM) return; }
/*----------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------*/
void bb_pl_builtin(void) { if (IS_X86) return; if (IS_JVM) return; if (IS_JS) return; if (IS_NET) return; if (IS_WASM) return; }
/*----------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------*/
void bb_pl_call(void)    { if (IS_X86) return; if (IS_JVM) return; if (IS_JS) return; if (IS_NET) return; if (IS_WASM) return; }
