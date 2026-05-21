/* bb_pl_atom.c — BB template for Prolog BB atom.
   One file per Byrd Box per RULES.md (BB_templates folder rule).
   Honest no-op stub across all five backends: no frontend lowers a Prolog BB
   graph to native today; Prolog execution is handled at runtime by IR_exec_node
   in src/lower/ir_exec.c.  Phase B will fill these arms when frontends start
   emitting Prolog BB graphs as native code. */
#include "bb_template_common.h"
/*----------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------*/
void bb_pl_atom(void)    { if (IS_X86) return; if (IS_JVM) return; if (IS_JS) return; if (IS_NET) return; if (IS_WASM) return; }
