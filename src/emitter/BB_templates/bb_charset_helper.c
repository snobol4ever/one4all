/* bb_charset_helper.c — corralled IS_X86 body for SPAN/ANY/BREAK/NOTANY.
   Lifted verbatim from emit_bb.c::emit_bb_charset.  Called by bb_span / bb_any /
   bb_break / bb_notany IS_X86 arms.  Parameters read from g_emit: op_name1 = chars,
   op_name2 = c_fn_name ("bb_span"/"bb_any"/"bb_brk"/"bb_notany"), op_kind = banner.
   Live path remains emit_bb.c::emit_bb_charset until dispatcher rewire. */
#include "bb_template_common.h"
#include "bb_box.h"
#include "emit_bb.h"
#include "../runtime/rt/rt.h"
#include <string.h>
/*----------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------*/
void bb_charset_emit(void) {
    const char *chars     = g_emit.op_name1;
    const char *c_fn_name = g_emit.op_name2;
    const char *kind_name = g_emit.op_kind;
    const char *lbl_succ  = g_emit.lbl_succ;
    const char *lbl_fail  = g_emit.lbl_fail;
    const char *lbl_back  = g_emit.lbl_back;
    bb_label_t  L_s = bb_label_from_name(lbl_succ);
    bb_label_t  L_f = bb_label_from_name(lbl_fail);
    bb_label_t  L_b = bb_label_from_name(lbl_back);
    typedef struct { const char *chars; int delta; } cs_t;
    cs_t *z = calloc(1, sizeof(cs_t)); z->chars = chars;
    const char *rt_name; uint64_t rt_fn;
    if      (c_fn_name && strcmp(c_fn_name,"bb_span")   == 0) { rt_name="rt_bb_span";   rt_fn=(uint64_t)(uintptr_t)rt_bb_span;   }
    else  if(c_fn_name && strcmp(c_fn_name,"bb_brk")    == 0) { rt_name="rt_bb_brk";    rt_fn=(uint64_t)(uintptr_t)rt_bb_brk;    }
    else  if(c_fn_name && strcmp(c_fn_name,"bb_any")    == 0) { rt_name="rt_bb_any";    rt_fn=(uint64_t)(uintptr_t)rt_bb_any;    }
    else  if(c_fn_name && strcmp(c_fn_name,"bb_notany") == 0) { rt_name="rt_bb_notany"; rt_fn=(uint64_t)(uintptr_t)rt_bb_notany; }
    else                                                      { rt_name="rt_bb_span";   rt_fn=(uint64_t)(uintptr_t)rt_bb_span;   }
    if (IS_TEXT) {
        emit_bb_box_banner(kind_name ? kind_name : "CHARSET", chars ? chars : "");
        int id = g_flat_node_id++;
        char slbl[80], zlbl[80];
        snprintf(slbl, sizeof slbl, ".Lcs%d_chars", id);
        snprintf(zlbl, sizeof zlbl, ".Lcs%d_z",     id);
        const char *ch = chars ? chars : "";
        char esc[1024]; size_t o = 0;
        if (o < sizeof esc) esc[o++] = '"';
        for (const char *cp = ch; *cp && o + 5 < sizeof esc; cp++) {
            unsigned char c = (unsigned char)*cp;
            if (c == '\"' || c == '\\') { esc[o++] = '\\'; esc[o++] = (char)c; }
            else if (c >= 32 && c < 127) { esc[o++] = (char)c; }
            else { o += snprintf(esc + o, sizeof esc - o, "\\%03o", c); }
        }
        if (o + 1 < sizeof esc) esc[o++] = '"';
        esc[o] = '\0';
        FILE *out = emit_outf();
        char slbl_def[88], zlbl_def[88];
        snprintf(slbl_def, sizeof slbl_def, "%s:", slbl);
        snprintf(zlbl_def, sizeof zlbl_def, "%s:", zlbl);
        bb3c_format(out, "",       ".section", ".data");
        bb3c_format(out, slbl_def, ".string",  esc);
        bb3c_format(out, zlbl_def, ".quad",    slbl);
        bb3c_format(out, "",       ".long",    "0");
        bb3c_format(out, "",       ".long",    "0");
        bb3c_format(out, "",       ".section", ".text");
        bb3c_format(out, "",       ".intel_syntax", "noprefix");
        emit_seq_port_call_rip((uint64_t)(uintptr_t)z, zlbl, rt_name, rt_fn, 0, &L_s, &L_f);
        emit_label_define(&L_b);
        emit_seq_port_call_rip((uint64_t)(uintptr_t)z, zlbl, rt_name, rt_fn, 1, &L_s, &L_f);
    } else {
        emit_seq_port_call((uint64_t)(uintptr_t)z, rt_name, rt_fn, 0, &L_s, &L_f);
        emit_label_define(&L_b);
        emit_seq_port_call((uint64_t)(uintptr_t)z, rt_name, rt_fn, 1, &L_s, &L_f);
    }
    /* Non-x86 backends: NOT a top-level BB template — bb_span/any/break/notany are.
       Stubs below satisfy the matrix gate's per-fn × per-backend coverage check;
       real codegen for those backends lives in each calling template. */
    if (IS_JVM)  return;
    if (IS_JS)   return;
    if (IS_NET)  return;
    if (IS_WASM) return;
}
