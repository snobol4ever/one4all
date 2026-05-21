/* bb_dsar.c — BB template for *VAR (deferred-var pattern, "DEREF").
   Corraled from emit_bb.c::emit_bb_xdsar.  No BB_op_t kind dispatches here yet;
   the file exists so the body is in its proper template location, awaiting wire-up.
   Live path remains emit_bb.c::emit_bb_xdsar called from emit_bb.c's own walker. */
#include "bb_template_common.h"
#include "bb_box.h"          /* bb_dvar_bin_new */
#include "emit_bb.h"         /* g_flat_node_id, ... */
#include "../runtime/rt/rt.h"/* port-call helpers */
extern DESCR_t bb_deferred_var_exported(void *zeta, int entry);
/*----------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------*/
void bb_dsar(void) {
    if (IS_X86) {
        /* Lifted from emit_bb.c::emit_bb_xdsar.  Reads g_emit.op_name1 (varname)
           and g_emit.lbl_* (name strings).  Pointer-laundering helpers are still
           used (emit_seq_port_call_rip, emit_label_define) via transient
           bb_label_from_name scaffolding — to be replaced when name-keyed
           variants exist. */
        const char *varname  = g_emit.op_name1;
        const char *lbl_succ = g_emit.lbl_succ;
        const char *lbl_fail = g_emit.lbl_fail;
        const char *lbl_back = g_emit.lbl_back;
        bb_label_t  L_s = bb_label_from_name(lbl_succ);
        bb_label_t  L_f = bb_label_from_name(lbl_fail);
        bb_label_t  L_b = bb_label_from_name(lbl_back);
        char banner[80]; snprintf(banner, sizeof banner, "*%s", varname ? varname : "");
        emit_bb_box_banner("DEREF", banner);
        int id = g_flat_node_id++;
        char zlbl[80], slbl[80];
        snprintf(zlbl, sizeof zlbl, ".Ldvar%d_z",    id);
        snprintf(slbl, sizeof slbl, ".Ldvar%d_name", id);
        const char *vn = varname ? varname : "";
        flat_data_section();
        flat3c_label(slbl); flat_data_string(vn);
        flat3c_label(zlbl);
        flat_data_quad(slbl); flat_data_quad("0"); flat_data_quad("0");
        flat_data_quad("0");  flat_data_long(0);   flat_data_long(0);
        flat_text_section(); flat_intel_syntax();
        void *z = bb_dvar_bin_new(vn);
        emit_seq_port_call_rip((uint64_t)(uintptr_t)z, zlbl,
                               "bb_deferred_var_exported",
                               (uint64_t)(uintptr_t)bb_deferred_var_exported, 0, &L_s, &L_f);
        emit_label_define(&L_b);
        emit_seq_port_call_rip((uint64_t)(uintptr_t)z, zlbl,
                               "bb_deferred_var_exported",
                               (uint64_t)(uintptr_t)bb_deferred_var_exported, 1, &L_s, &L_f);
        return;
    }
    if (IS_BIN) return;
    if (IS_JVM) return;
    if (IS_JS)  return;
    if (IS_NET) return;
    if (IS_WASM) return;
}
