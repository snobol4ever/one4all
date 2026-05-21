/* bb_capture.c — BB template for capture (pat . var).
   One file per Byrd Box per RULES.md (BB_templates folder rule).
   Function body is byte-identical to the consolidated bb_pat.c original
   that this file restores; separators and includes match the original per-file
   shape pre-EC-UNI-13(a). */
#include "bb_template_common.h"
#include "bb_box.h"
#include "emit_bb.h"
#include "../runtime/rt/rt.h"
/*----------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------*/
void bb_capture(int imm) {
    BB_t * nd = g_emit.node; FILE * out = g_emit.out;
    int nid = bb_node_id(nd); int sid = 0;
    if (IS_X86) {
        /* Lifted from emit_bb.c::emit_bb_xcallcap / emit_bb_xfnme / emit_bb_xnme.
           Three siblings dispatched by g_emit.op_name1:
             - non-NULL fnc_name + NULL varname  -> CALLCAP (emit_bb_xcallcap)
             - imm=1 + non-NULL varname          -> CAP_IMM  (emit_bb_xfnme)
             - imm=0 + non-NULL varname          -> CAP_COND (emit_bb_xnme)
           Parameters corraled from g_emit: child_fn, op_name1 (fnc_name or
           varname), lbl_succ/lbl_fail/lbl_back.  Live path still emit_bb.c. */
        bb_box_fn   child_fn = (bb_box_fn)g_emit.child_fn;
        const char *name     = g_emit.op_name1;
        const char *lbl_succ = g_emit.lbl_succ;
        const char *lbl_fail = g_emit.lbl_fail;
        const char *lbl_back = g_emit.lbl_back;
        bb_label_t  L_s = bb_label_from_name(lbl_succ);
        bb_label_t  L_f = bb_label_from_name(lbl_fail);
        bb_label_t  L_b = bb_label_from_name(lbl_back);
        const char *banner_kind = (imm ? "CAP_IMM" : "CAP_COND");
        int   cap_imm = imm;
        void *z = NULL;
        const char *combo_imm_flag, *combo_callcap_flag;
        if (!name || name[0] == 0) {
            /* fall through — nothing to do; live path covers it */
            return;
        }
        /* Decide variant: emit_bb_xcallcap was called with fnc_name (and no
           varname).  The dispatcher path for that is BB_PAT_ASSIGN_*_with_call.
           For the lift, we model it by treating op_name1 as varname for
           CAP_IMM/CAP_COND, and op_name2 as fnc_name for CALLCAP.  Today the
           dispatcher does not fill either, so we go down the CAP_*  branches. */
        if (g_emit.op_name2 && g_emit.op_name2[0]) {
            /* xcallcap path */
            const char *fnc_name = g_emit.op_name2;
            banner_kind = "CALLCAP";
            z = bb_cap_new_call(child_fn, NULL, fnc_name, NULL, 0, NULL, 0, 0);
            emit_bb_box_banner(banner_kind, fnc_name);
            combo_imm_flag = "0"; combo_callcap_flag = "1";
            if (IS_TEXT) {
                char zlbl[80]; emit_bb_ptr_slot(zlbl);
                const char *clbl = child_fn ? child_cache_get_lbl(child_fn) : NULL;
                if (clbl && g_cap_fixup_cb) {
                    char combo[320];
                    snprintf(combo, sizeof combo, "%s|%s|%s|%s|%s",
                             zlbl, clbl, fnc_name, combo_imm_flag, combo_callcap_flag);
                    g_cap_fixup_cb((void*)1, combo);
                }
                emit_seq_port_call_rip((uint64_t)(uintptr_t)z, zlbl, "rt_bb_cap",
                                       (uint64_t)(uintptr_t)rt_bb_cap, 0, &L_s, &L_f);
                emit_label_define(&L_b);
                emit_seq_port_call_rip((uint64_t)(uintptr_t)z, zlbl, "rt_bb_cap",
                                       (uint64_t)(uintptr_t)rt_bb_cap, 1, &L_s, &L_f);
                return;
            }
            emit_seq_port_call((uint64_t)(uintptr_t)z, "rt_bb_cap",
                               (uint64_t)(uintptr_t)rt_bb_cap, 0, &L_s, &L_f);
            emit_label_define(&L_b);
            emit_seq_port_call((uint64_t)(uintptr_t)z, "rt_bb_cap",
                               (uint64_t)(uintptr_t)rt_bb_cap, 1, &L_s, &L_f);
            return;
        }
        /* xfnme (imm=1) or xnme (imm=0) path */
        z = bb_cap_new(child_fn, NULL, name, NULL, cap_imm);
        emit_bb_box_banner(banner_kind, name);
        combo_imm_flag     = cap_imm ? "1" : "0";
        combo_callcap_flag = "0";
        if (IS_TEXT) {
            char zlbl[80]; emit_bb_ptr_slot(zlbl);
            const char *clbl = child_fn ? child_cache_get_lbl(child_fn) : NULL;
            if (clbl && g_cap_fixup_cb) {
                char combo[320];
                snprintf(combo, sizeof combo, "%s|%s|%s|%s|%s",
                         zlbl, clbl, name, combo_imm_flag, combo_callcap_flag);
                g_cap_fixup_cb((void*)1, combo);
            }
            emit_seq_port_call_rip((uint64_t)(uintptr_t)z, zlbl, "rt_bb_cap",
                                   (uint64_t)(uintptr_t)rt_bb_cap, 0, &L_s, &L_f);
            emit_label_define(&L_b);
            emit_seq_port_call_rip((uint64_t)(uintptr_t)z, zlbl, "rt_bb_cap",
                                   (uint64_t)(uintptr_t)rt_bb_cap, 1, &L_s, &L_f);
            return;
        }
        emit_seq_port_call((uint64_t)(uintptr_t)z, "rt_bb_cap",
                           (uint64_t)(uintptr_t)rt_bb_cap, 0, &L_s, &L_f);
        emit_label_define(&L_b);
        emit_seq_port_call((uint64_t)(uintptr_t)z, "rt_bb_cap",
                           (uint64_t)(uintptr_t)rt_bb_cap, 1, &L_s, &L_f);
        return;
    }
    if (IS_BIN) return; /* x86 binary: emit_flat_body path, not emit_bb_node */
    if (IS_JVM) {
        (void)imm;
        jvm_class_hdr(out, "capture");
        emit_textf(".inner interface public static abstract var_setter inner bb/bb_capture$VarSetter outer bb/bb_capture\n");
        emit_textf(".field private final child Lbb/bb_box;\n.field private final varname Ljava/lang/String;\n");
        emit_textf(".field private final immediate Z\n.field private final setter Lbb/bb_capture$VarSetter;\n");
        emit_textf(".field private pending_start I\n.field private pending_len I\n.field private has_pending Z\n");
        emit_textf(".method public <init>(Lbb/bb_box$MatchState;Lbb/bb_box;Ljava/lang/String;ZLbb/bb_capture$VarSetter;)V\n    .limit stack 3\n    .limit locals 6\n");
        emit_textf("    aload_0\n    aload_1\n    invokespecial bb/bb_box/<init>(Lbb/bb_box$MatchState;)V\n");
        emit_textf("    aload_0\n    aload_2\n    putfield bb/bb_capture/child Lbb/bb_box;\n");
        emit_textf("    aload_0\n    aload_3\n    putfield bb/bb_capture/varname Ljava/lang/String;\n");
        emit_textf("    aload_0\n    iload 4\n    putfield bb/bb_capture/immediate Z\n");
        emit_textf("    aload_0\n    aload 5\n    putfield bb/bb_capture/setter Lbb/bb_capture$VarSetter;\n    return\n.end method\n");
        emit_textf(".method public \316\261()Lbb/bb_box$Spec;\n    .limit stack 3\n    .limit locals 2\n");
        emit_textf("    aload_0\n    getfield bb/bb_capture/child Lbb/bb_box;\n    invokevirtual bb/bb_box/\316\261()Lbb/bb_box$Spec;\n    astore_1\n");
        emit_textf("    aload_0\n    aload_1\n    invokevirtual bb/bb_capture/runChild(Lbb/bb_box$Spec;)Lbb/bb_box$Spec;\n    areturn\n.end method\n");
        emit_textf(".method public \316\262()Lbb/bb_box$Spec;\n    .limit stack 3\n    .limit locals 2\n");
        emit_textf("    aload_0\n    getfield bb/bb_capture/child Lbb/bb_box;\n    invokevirtual bb/bb_box/\316\262()Lbb/bb_box$Spec;\n    astore_1\n");
        emit_textf("    aload_0\n    aload_1\n    invokevirtual bb/bb_capture/runChild(Lbb/bb_box$Spec;)Lbb/bb_box$Spec;\n    areturn\n.end method\n");
        (void)sid; (void)nid; return;
    }
    if (IS_JS) {
        emit_textf("function make_pat_%d_%d(ms) { const varname = ", nd->ival, nid);
        js_escape(out, nd->sval);
        emit_textf("; let self = { succ: null, fail: null,\n");
        emit_textf("alpha() { const cr = self.child.alpha(); if (cr === null) { self.fail.alpha(); return; } ms._do_capture(cr, varname, %d); self.succ.alpha(); return cr; },\n", imm);
        emit_textf("beta() { const cr = self.child.beta(); if (cr === null) { self.fail.alpha(); return; } ms._do_capture(cr, varname, %d); return cr; }\n", imm);
        emit_textf("}; return self; }\n");
        return;
    }
    if (IS_NET) {
        const char * varname = nd->sval ? nd->sval : "";
        net_class_hdr(out, sid, nid);
        emit_textf("  .field private class [boxes]Snobol4.Runtime.Boxes.IByrdBox _child\n  .field private string _varname\n  .field private bool _immediate\n");
        emit_textf("  .method public specialname rtspecialname instance void .ctor(class [boxes]Snobol4.Runtime.Boxes.IByrdBox child, string varname, bool imm) cil managed\n  {\n");
        emit_textf("    .maxstack 2\n    ldarg.0\n    call       instance void [mscorlib]System.Object::.ctor()\n");
        emit_textf("    ldarg.0\n    ldarg.1\n    stfld      class [boxes]Snobol4.Runtime.Boxes.IByrdBox pat_%d_%d::_child\n", sid, nid);
        emit_textf("    ldarg.0\n    ldarg.2\n    dup\n    brtrue     CAP_%d_%d_NN\n    pop\n    ldstr      \"\"\n  CAP_%d_%d_NN:\n    stfld      string pat_%d_%d::_varname\n", sid, nid, sid, nid, sid, nid);
        emit_textf("    ldarg.0\n    ldarg.3\n    stfld      bool pat_%d_%d::_immediate\n    ret\n  }\n", sid, nid);
        net_alpha_hdr(out);
        emit_textf("    .maxstack 3\n    .locals init (valuetype [boxes]Snobol4.Runtime.Boxes.Spec V_cr, int32 V_start, int32 V_len, string V_matched)\n");
        emit_textf("    ldarg.1\n    ldfld      int32 [boxes]Snobol4.Runtime.Boxes.MatchState::Cursor\n    stloc.1\n");
        emit_textf("    ldarg.0\n    ldfld      class [boxes]Snobol4.Runtime.Boxes.IByrdBox pat_%d_%d::_child\n    ldarg.1\n", sid, nid);
        emit_textf("    callvirt   instance valuetype [boxes]Snobol4.Runtime.Boxes.Spec [boxes]Snobol4.Runtime.Boxes.IByrdBox::Alpha(class [boxes]Snobol4.Runtime.Boxes.MatchState)\n");
        emit_textf("    stloc.0\n    ldloca.s   V_cr\n    call       instance bool [boxes]Snobol4.Runtime.Boxes.Spec::get_IsFail()\n    brtrue     CAPC_%d_%d_FAIL\n", sid, nid);
        emit_textf("    ldloca.s   V_cr\n    ldfld      int32 [boxes]Snobol4.Runtime.Boxes.Spec::Start\n    stloc.1\n");
        emit_textf("    ldloca.s   V_cr\n    ldfld      int32 [boxes]Snobol4.Runtime.Boxes.Spec::Length\n    stloc.2\n");
        emit_textf("    ldarg.1\n    callvirt   instance string [boxes]Snobol4.Runtime.Boxes.MatchState::get_Subject()\n");
        emit_textf("    ldloc.1\n    ldloc.2\n    callvirt   instance string [mscorlib]System.String::Substring(int32, int32)\n    stloc.3\n");
        emit_textf("    ldarg.0\n    ldfld      string pat_%d_%d::_varname\n", sid, nid);
        emit_textf("    ldstr      \"OUTPUT\"\n    call       bool [mscorlib]System.String::op_Equality(string, string)\n");
        emit_textf("    brfalse    CAPC_%d_%d_NOTOUT\n    ldloc.3\n    call       void [mscorlib]System.Console::WriteLine(string)\n", sid, nid);
        emit_textf("    br         CAPC_%d_%d_DONE\n  CAPC_%d_%d_NOTOUT:\n  CAPC_%d_%d_DONE:\n    ldloc.0\n    ret\n", sid, nid, sid, nid, sid, nid);
        emit_textf("  CAPC_%d_%d_FAIL:\n", sid, nid); net_fail_ret(out); emit_textf("  }\n");
        net_beta_hdr(out);
        emit_textf("    .maxstack 2\n    ldarg.0\n    ldfld      class [boxes]Snobol4.Runtime.Boxes.IByrdBox pat_%d_%d::_child\n    ldarg.1\n", sid, nid);
        emit_textf("    callvirt   instance valuetype [boxes]Snobol4.Runtime.Boxes.Spec [boxes]Snobol4.Runtime.Boxes.IByrdBox::Beta(class [boxes]Snobol4.Runtime.Boxes.MatchState)\n    ret\n  }\n}\n");
        net_escape_ldstr(out, varname); net_push_i4(out, imm);
        emit_textf("    newobj     instance void pat_%d_%d::.ctor(class [boxes]Snobol4.Runtime.Boxes.IByrdBox, string, bool)\n", sid, nid);
    }
    /* IS_WASM: n/a — BB WASM never landed in original code */
}
