/* bb_icn_to_nested.c — BB template for BB_ICN_TO_NESTED.
   One file per Byrd Box per RULES.md (BB_templates folder rule).
   Honest no-op stub across all five backends.  This kind is handled today by the
   AST/runtime path in src/lower/ir_exec.c and src/lower/lower_*.c; this template
   slot exists so the BB layer is total over BB_op_t and future native-codegen
   work has a place to land.  Phase B fills the arms when a frontend lowers
   directly to native code for BB_ICN_TO_NESTED. */
#include "bb_template_common.h"
/*----------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------*/
void bb_icn_to_nested(void)    { if (IS_X86) return; if (IS_JVM) return; if (IS_JS) return; if (IS_NET) return; if (IS_WASM) return; }
