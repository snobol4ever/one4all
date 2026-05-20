/* sm_bb_calls.c — SM_BB_ONCE_PROC and SM_BB_PUMP_PROC templates (EC-UNI-13(d)).
   Bodies are the verbatim union of per-backend arms.  Today only the x86 arm
   carries logic; the four other silo walkers route SM_BB_ONCE_PROC and
   SM_BB_PUMP_PROC through their `default: break;` arm (= no-op), because no
   frontend lowers to these opcodes for JVM/JS/NET/WASM yet.  The stubs
   preserve that exact behaviour:

     x86  : emit_sm.c::emit_sm_bb_once_proc_dispatch    (rt_pl_once via SM template,
            PJ-9c Prolog predicate invocation)
          : emit_sm.c::emit_sm_bb_pump_proc_dispatch    (direct call .L<entry_pc>
            via SM_CALL_EXPRESSION template, IJ-HELLO-3 Icon proc invocation;
            falls back to rt_unhandled_sm if proc not in g_stage2.proc_table)
     JVM  : emit_core.c::emit_jvm_one_instr  default: break;
     JS   : emit_core.c::emit_js_from_sm     default: break;
     NET  : emit_core.c::emit_net_from_sm    default: break;
     WASM : emit_core.c::emit_wasm_from_sm   default: ;; unhandled SM opcode

   No refactor.  No helper extraction.  Wrapping `if (IS_<BE>)` is the only
   addition.  Return: 0 in every arm (no terminal jump consumed); convention
   matches sm_define_entry/sm_define.  Phase B per-backend goals (JVM/JS/NET/
   WASM) will fill the IS_<BE> arms when their frontends start emitting these
   opcodes; today the stubs are honest no-ops. */
#include "sm_template_common.h"
#include "emit_sm.h"
/*----------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------*/
int sm_bb_once_proc(void) {
    const SM_t * instr = g_emit.instr;
    FILE *       out   = g_emit.out;
    int          i     = g_emit.i;
    (void)i;
    if (IS_X86)  return emit_sm_bb_once_proc_dispatch(out, instr, g_emit.i);
    if (IS_JVM)  { return 0; }
    if (IS_JS)   { return 0; }
    if (IS_NET)  { return 0; }
    if (IS_WASM) { return 0; }
    return 0;
}
/*----------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------*/
int sm_bb_pump_proc(void) {
    const SM_t * instr = g_emit.instr;
    FILE *       out   = g_emit.out;
    int          i     = g_emit.i;
    (void)i;
    if (IS_X86)  return emit_sm_bb_pump_proc_dispatch(out, instr, g_emit.i);
    if (IS_JVM)  { return 0; }
    if (IS_JS)   { return 0; }
    if (IS_NET)  { return 0; }
    if (IS_WASM) { return 0; }
    return 0;
}
