/* bb_atp.c — BB template for user-defined pattern (atp / USERPAT).
   Corraled from emit_bb.c::emit_bb_xatp.  No BB_op_t kind dispatches here yet;
   the file exists so the body is in its proper template location, awaiting wire-up.
   Live path remains emit_bb.c::emit_bb_xatp called from emit_bb.c's own walker. */
#include "bb_template_common.h"
#include "bb_box.h"          /* atp_t, bb_atp_new */
#include "emit_bb.h"         /* g_flat_node_id, ... */
#include "../runtime/rt/rt.h"/* rt_bb_atp */
/*----------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------*/
void bb_atp_template(void) {
    if (IS_X86) {
        /* Lifted from emit_bb.c::emit_bb_xatp.  Reads g_emit.op_name1 (varname)
           and g_emit.lbl_* (name strings).  Pointer-laundering helpers
           (emit_seq_port_call_rip, emit_label_define) via transient
           bb_label_from_name scaffolding. */
        const char *varname  = g_emit.op_name1;
        const char *lbl_succ = g_emit.lbl_succ;
        const char *lbl_fail = g_emit.lbl_fail;
        const char *lbl_back = g_emit.lbl_back;
        bb_label_t  L_s = bb_label_from_name(lbl_succ);
        bb_label_t  L_f = bb_label_from_name(lbl_fail);
        bb_label_t  L_b = bb_label_from_name(lbl_back);
        emit_bb_box_banner("USERPAT", varname ? varname : "");
        int id = g_flat_node_id++;
        char zlbl[80], vlbl[80];
        snprintf(zlbl, sizeof zlbl, ".Latp%d_z",     id);
        snprintf(vlbl, sizeof vlbl, ".Latp%d_vname", id);
        const char *vn = varname ? varname : "";
        flat_data_section();
        flat3c_label(vlbl); flat_data_string(vn);
        flat3c_label(zlbl); flat_data_long(0); flat_data_long(0); flat_data_quad(vlbl);
        flat_text_section(); flat_intel_syntax();
        atp_t *z = bb_atp_new(vn);
        emit_seq_port_call_rip((uint64_t)(uintptr_t)z, zlbl, "rt_bb_atp",
                               (uint64_t)(uintptr_t)rt_bb_atp, 0, &L_s, &L_f);
        emit_label_define(&L_b);
        emit_seq_port_call_rip((uint64_t)(uintptr_t)z, zlbl, "rt_bb_atp",
                               (uint64_t)(uintptr_t)rt_bb_atp, 1, &L_s, &L_f);
        return;
    }
    if (IS_BIN) return;
    if (IS_JVM) return;
    if (IS_JS)  return;
    if (IS_NET) return;
    if (IS_WASM) return;
}
