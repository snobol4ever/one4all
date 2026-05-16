#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include "emit_ir.h"
#include "sm_prog.h"
/*----------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------*/
extern SM_Program * sm_preamble(const tree_t * ast_prog);
/*----------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------*/
/* net_escape_ldstr — emit a MSIL ldstr literal with proper Unicode escaping.
   MSIL ldstr: double-quote -> \", backslash -> \\, non-printable ASCII -> \uXXXX. */
static void net_escape_ldstr(FILE * out, const char * s) {
    fprintf(out, "    ldstr      \"");
    if (!s) { fprintf(out, "\""); return; }
    for (const unsigned char * p = (const unsigned char *)s; *p; p++) {
        if (*p == '"')        { fprintf(out, "\\\""); }
        else if (*p == '\\')  { fprintf(out, "\\\\"); }
        else if (*p < 0x20 || *p == 0x7f) { fprintf(out, "\\u%04X", (unsigned)*p); }
        else                  { fputc(*p, out); }
    }
    fprintf(out, "\"\n");
}
/*----------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------*/
/* net_push_i4 — compact integer push. */
static void net_push_i4(FILE * out, int v) {
    if (v >= 0 && v <= 8)   { fprintf(out, "    ldc.i4.%d\n", v); }
    else if (v == -1)        { fprintf(out, "    ldc.i4.m1\n"); }
    else if (v >= -128 && v <= 127) { fprintf(out, "    ldc.i4.s   %d\n", v); }
    else                     { fprintf(out, "    ldc.i4     %d\n", v); }
}
/*----------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------*/
/* net_class_hdr — emit standard nested class opening for a pattern box. */
static void net_class_hdr(FILE * out, int sid, int nid) {
    fprintf(out, ".class nested public auto ansi beforefieldinit pat_%d_%d\n", sid, nid);
    fprintf(out, "       extends [mscorlib]System.Object\n");
    fprintf(out, "       implements [boxes]Snobol4.Runtime.Boxes.IByrdBox\n{\n");
}
/*----------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------*/
/* net_ctor_none — .ctor with no payload fields. */
static void net_ctor_none(FILE * out, int sid, int nid) {
    fprintf(out, "  .method public specialname rtspecialname instance void .ctor() cil managed\n  {\n");
    fprintf(out, "    .maxstack 1\n");
    fprintf(out, "    ldarg.0\n    call       instance void [mscorlib]System.Object::.ctor()\n    ret\n  }\n");
}
/*----------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------*/
/* net_alpha_hdr / net_beta_hdr — open Alpha / Beta method. */
static void net_alpha_hdr(FILE * out) {
    fprintf(out, "  .method public virtual instance valuetype [boxes]Snobol4.Runtime.Boxes.Spec\n");
    fprintf(out, "          Alpha(class [boxes]Snobol4.Runtime.Boxes.MatchState ms) cil managed\n  {\n");
}
static void net_beta_hdr(FILE * out) {
    fprintf(out, "  .method public virtual instance valuetype [boxes]Snobol4.Runtime.Boxes.Spec\n");
    fprintf(out, "          Beta(class [boxes]Snobol4.Runtime.Boxes.MatchState ms) cil managed\n  {\n");
}
/*----------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------*/
/* net_fail_ret — emit the ldsfld Spec::Fail + ret sequence. */
static void net_fail_ret(FILE * out) {
    fprintf(out, "    ldsfld     valuetype [boxes]Snobol4.Runtime.Boxes.Spec [boxes]Snobol4.Runtime.Boxes.Spec::Fail\n");
    fprintf(out, "    ret\n");
}
/*----------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------*/
/* net_cursor_load — ldarg.1; ldfld Cursor */
static void net_cursor_load(FILE * out) {
    fprintf(out, "    ldarg.1\n");
    fprintf(out, "    ldfld      int32 [boxes]Snobol4.Runtime.Boxes.MatchState::Cursor\n");
}
/*----------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------*/
/* net_cursor_store — prepare ms then value then stfld Cursor */
static void net_cursor_store_seq(FILE * out) {
    fprintf(out, "    stfld      int32 [boxes]Snobol4.Runtime.Boxes.MatchState::Cursor\n");
}
/*----------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------*/
/* net_spec_of — call Spec::Of(int32, int32) */
static void net_spec_of(FILE * out) {
    fprintf(out, "    call       valuetype [boxes]Snobol4.Runtime.Boxes.Spec [boxes]Snobol4.Runtime.Boxes.Spec::Of(int32, int32)\n");
}
/*----------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------*/
/* net_spec_zw — call Spec::ZeroWidth(int32) */
static void net_spec_zw(FILE * out) {
    fprintf(out, "    call       valuetype [boxes]Snobol4.Runtime.Boxes.Spec [boxes]Snobol4.Runtime.Boxes.Spec::ZeroWidth(int32)\n");
}
/*----------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------*/
/* net_ms_length — callvirt MatchState::get_Length() */
static void net_ms_length(FILE * out) {
    fprintf(out, "    callvirt   instance int32 [boxes]Snobol4.Runtime.Boxes.MatchState::get_Length()\n");
}
/*----------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------*/
/* emit_net_bb_lit — IR_PAT_LIT: literal string match. */
static void emit_net_bb_lit(IR_t * nd, FILE * out, int sid, int nid) {
    const char * lit = nd->sval ? nd->sval : "";
    net_class_hdr(out, sid, nid);
    fprintf(out, "  .field private string _lit\n  .field private int32  _len\n");
    fprintf(out, "  .method public specialname rtspecialname instance void .ctor(string lit) cil managed\n  {\n");
    fprintf(out, "    .maxstack 3\n    ldarg.0\n    call       instance void [mscorlib]System.Object::.ctor()\n");
    fprintf(out, "    ldarg.0\n    ldarg.1\n    dup\n    brtrue     LIT_%d_%d_NN\n    pop\n    ldstr      \"\"\n", sid, nid);
    fprintf(out, "  LIT_%d_%d_NN:\n", sid, nid);
    fprintf(out, "    stfld      string pat_%d_%d::_lit\n", sid, nid);
    fprintf(out, "    ldarg.0\n    ldarg.0\n");
    fprintf(out, "    ldfld      string pat_%d_%d::_lit\n", sid, nid);
    fprintf(out, "    callvirt   instance int32 [mscorlib]System.String::get_Length()\n");
    fprintf(out, "    stfld      int32 pat_%d_%d::_len\n    ret\n  }\n", sid, nid);
    net_alpha_hdr(out);
    fprintf(out, "    .maxstack 4\n    .locals init (valuetype [boxes]Snobol4.Runtime.Boxes.Spec V_r)\n");
    net_cursor_load(out);
    fprintf(out, "    ldfld      int32 pat_%d_%d::_len\n    add\n", sid, nid);
    fprintf(out, "    ldarg.1\n"); net_ms_length(out);
    fprintf(out, "    bgt        LIT_%d_%d_A_FAIL\n", sid, nid);
    fprintf(out, "    ldarg.1\n");
    net_cursor_load(out);
    fprintf(out, "    ldfld      string pat_%d_%d::_lit\n", sid, nid);
    fprintf(out, "    callvirt   instance bool [boxes]Snobol4.Runtime.Boxes.MatchState::MatchesAt(int32, string)\n");
    fprintf(out, "    brfalse    LIT_%d_%d_A_FAIL\n", sid, nid);
    net_cursor_load(out);
    fprintf(out, "    ldfld      int32 pat_%d_%d::_len\n", sid, nid);
    net_spec_of(out);
    fprintf(out, "    stloc.0\n");
    fprintf(out, "    ldarg.1\n    ldarg.1\n");
    net_cursor_load(out);
    fprintf(out, "    ldfld      int32 pat_%d_%d::_len\n    add\n", sid, nid);
    fprintf(out, "    stfld      int32 [boxes]Snobol4.Runtime.Boxes.MatchState::Cursor\n");
    fprintf(out, "    ldloc.0\n    ret\n");
    fprintf(out, "  LIT_%d_%d_A_FAIL:\n", sid, nid);
    net_fail_ret(out);
    fprintf(out, "  }\n");
    net_beta_hdr(out);
    fprintf(out, "    .maxstack 3\n");
    fprintf(out, "    ldarg.1\n    ldarg.1\n");
    net_cursor_load(out);
    fprintf(out, "    ldfld      int32 pat_%d_%d::_len\n    sub\n", sid, nid);
    fprintf(out, "    stfld      int32 [boxes]Snobol4.Runtime.Boxes.MatchState::Cursor\n");
    net_fail_ret(out);
    fprintf(out, "  }\n}\n");
    fprintf(out, "// constructor call site: newobj pat_%d_%d::.ctor with ldstr literal\n", sid, nid);
    net_escape_ldstr(out, lit);
    fprintf(out, "    newobj     instance void pat_%d_%d::.ctor(string)\n", sid, nid);
}
/*----------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------*/
/* emit_net_bb_charset — shared for ANY, NOTANY, SPAN, BREAK (all have string _chars). */
static void net_charset_class(FILE * out, int sid, int nid, const char * kind, const char * tag) {
    net_class_hdr(out, sid, nid);
    fprintf(out, "  .field private string _chars\n");
    fprintf(out, "  .method public specialname rtspecialname instance void .ctor(string chars) cil managed\n  {\n");
    fprintf(out, "    .maxstack 3\n    ldarg.0\n    call       instance void [mscorlib]System.Object::.ctor()\n");
    fprintf(out, "    ldarg.0\n    ldarg.1\n    dup\n    brtrue     %s_%d_%d_NN\n    pop\n    ldstr      \"\"\n", tag, sid, nid);
    fprintf(out, "  %s_%d_%d_NN:\n", tag, sid, nid);
    fprintf(out, "    stfld      string pat_%d_%d::_chars\n    ret\n  }\n", sid, nid);
}
/*----------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------*/
static void emit_net_bb_any(IR_t * nd, FILE * out, int sid, int nid) {
    const char * chars = nd->sval ? nd->sval : "";
    net_charset_class(out, sid, nid, "ANY", "ANY");
    net_alpha_hdr(out);
    fprintf(out, "    .maxstack 3\n    .locals init (valuetype [boxes]Snobol4.Runtime.Boxes.Spec V_r)\n");
    fprintf(out, "    ldarg.1\n");
    net_cursor_load(out);
    fprintf(out, "    ldfld      string pat_%d_%d::_chars\n", sid, nid);
    fprintf(out, "    callvirt   instance bool [boxes]Snobol4.Runtime.Boxes.MatchState::CharInSet(int32, string)\n");
    fprintf(out, "    brfalse    ANY_%d_%d_A_FAIL\n", sid, nid);
    net_cursor_load(out);
    fprintf(out, "    ldc.i4.1\n"); net_spec_of(out); fprintf(out, "    stloc.0\n");
    fprintf(out, "    ldarg.1\n    ldarg.1\n"); net_cursor_load(out);
    fprintf(out, "    ldc.i4.1\n    add\n    stfld      int32 [boxes]Snobol4.Runtime.Boxes.MatchState::Cursor\n");
    fprintf(out, "    ldloc.0\n    ret\n");
    fprintf(out, "  ANY_%d_%d_A_FAIL:\n", sid, nid);
    net_fail_ret(out); fprintf(out, "  }\n");
    net_beta_hdr(out);
    fprintf(out, "    .maxstack 3\n    ldarg.1\n    ldarg.1\n"); net_cursor_load(out);
    fprintf(out, "    ldc.i4.1\n    sub\n    stfld      int32 [boxes]Snobol4.Runtime.Boxes.MatchState::Cursor\n");
    net_fail_ret(out); fprintf(out, "  }\n}\n");
    net_escape_ldstr(out, chars);
    fprintf(out, "    newobj     instance void pat_%d_%d::.ctor(string)\n", sid, nid);
}
/*----------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------*/
static void emit_net_bb_notany(IR_t * nd, FILE * out, int sid, int nid) {
    const char * chars = nd->sval ? nd->sval : "";
    net_charset_class(out, sid, nid, "NOTANY", "NOTANY");
    net_alpha_hdr(out);
    fprintf(out, "    .maxstack 3\n    .locals init (valuetype [boxes]Snobol4.Runtime.Boxes.Spec V_r)\n");
    fprintf(out, "    ldarg.1\n"); net_cursor_load(out);
    fprintf(out, "    ldfld      string pat_%d_%d::_chars\n", sid, nid);
    fprintf(out, "    callvirt   instance bool [boxes]Snobol4.Runtime.Boxes.MatchState::CharInSet(int32, string)\n");
    fprintf(out, "    brtrue     NOTANY_%d_%d_A_FAIL\n", sid, nid);
    net_cursor_load(out); fprintf(out, "    ldc.i4.1\n"); net_spec_of(out); fprintf(out, "    stloc.0\n");
    fprintf(out, "    ldarg.1\n    ldarg.1\n"); net_cursor_load(out);
    fprintf(out, "    ldc.i4.1\n    add\n    stfld      int32 [boxes]Snobol4.Runtime.Boxes.MatchState::Cursor\n");
    fprintf(out, "    ldloc.0\n    ret\n");
    fprintf(out, "  NOTANY_%d_%d_A_FAIL:\n", sid, nid); net_fail_ret(out); fprintf(out, "  }\n");
    net_beta_hdr(out);
    fprintf(out, "    .maxstack 3\n    ldarg.1\n    ldarg.1\n"); net_cursor_load(out);
    fprintf(out, "    ldc.i4.1\n    sub\n    stfld      int32 [boxes]Snobol4.Runtime.Boxes.MatchState::Cursor\n");
    net_fail_ret(out); fprintf(out, "  }\n}\n");
    net_escape_ldstr(out, chars);
    fprintf(out, "    newobj     instance void pat_%d_%d::.ctor(string)\n", sid, nid);
}
/*----------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------*/
static void emit_net_bb_span(IR_t * nd, FILE * out, int sid, int nid) {
    const char * chars = nd->sval ? nd->sval : "";
    net_class_hdr(out, sid, nid);
    fprintf(out, "  .field private string _chars\n  .field private int32  _count\n");
    fprintf(out, "  .method public specialname rtspecialname instance void .ctor(string chars) cil managed\n  {\n");
    fprintf(out, "    .maxstack 3\n    ldarg.0\n    call       instance void [mscorlib]System.Object::.ctor()\n");
    fprintf(out, "    ldarg.0\n    ldarg.1\n    dup\n    brtrue     SP_%d_%d_NN\n    pop\n    ldstr      \"\"\n", sid, nid);
    fprintf(out, "  SP_%d_%d_NN:\n    stfld      string pat_%d_%d::_chars\n    ret\n  }\n", sid, nid, sid, nid);
    net_alpha_hdr(out);
    fprintf(out, "    .maxstack 3\n    .locals init (valuetype [boxes]Snobol4.Runtime.Boxes.Spec V_r)\n");
    fprintf(out, "    ldarg.0\n    ldc.i4.0\n    stfld      int32 pat_%d_%d::_count\n", sid, nid);
    fprintf(out, "  SP_%d_%d_LOOP:\n", sid, nid);
    fprintf(out, "    ldarg.1\n"); net_cursor_load(out); fprintf(out, "    ldarg.0\n    ldfld      int32 pat_%d_%d::_count\n    add\n", sid, nid);
    fprintf(out, "    ldfld      string pat_%d_%d::_chars\n", sid, nid);
    fprintf(out, "    callvirt   instance bool [boxes]Snobol4.Runtime.Boxes.MatchState::CharInSet(int32, string)\n");
    fprintf(out, "    brfalse    SP_%d_%d_DONE\n", sid, nid);
    fprintf(out, "    ldarg.0\n    ldarg.0\n    ldfld      int32 pat_%d_%d::_count\n    ldc.i4.1\n    add\n    stfld      int32 pat_%d_%d::_count\n", sid, nid, sid, nid);
    fprintf(out, "    br         SP_%d_%d_LOOP\n", sid, nid);
    fprintf(out, "  SP_%d_%d_DONE:\n    ldarg.0\n    ldfld      int32 pat_%d_%d::_count\n    ldc.i4.0\n    ble        SP_%d_%d_FAIL\n", sid, nid, sid, nid, sid, nid);
    net_cursor_load(out); fprintf(out, "    ldarg.0\n    ldfld      int32 pat_%d_%d::_count\n", sid, nid);
    net_spec_of(out); fprintf(out, "    stloc.0\n");
    fprintf(out, "    ldarg.1\n    ldarg.1\n"); net_cursor_load(out);
    fprintf(out, "    ldarg.0\n    ldfld      int32 pat_%d_%d::_count\n    add\n", sid, nid);
    fprintf(out, "    stfld      int32 [boxes]Snobol4.Runtime.Boxes.MatchState::Cursor\n");
    fprintf(out, "    ldloc.0\n    ret\n");
    fprintf(out, "  SP_%d_%d_FAIL:\n", sid, nid); net_fail_ret(out); fprintf(out, "  }\n");
    net_beta_hdr(out);
    fprintf(out, "    .maxstack 3\n    ldarg.1\n    ldarg.1\n"); net_cursor_load(out);
    fprintf(out, "    ldarg.0\n    ldfld      int32 pat_%d_%d::_count\n    sub\n", sid, nid);
    fprintf(out, "    stfld      int32 [boxes]Snobol4.Runtime.Boxes.MatchState::Cursor\n");
    net_fail_ret(out); fprintf(out, "  }\n}\n");
    net_escape_ldstr(out, chars); fprintf(out, "    newobj     instance void pat_%d_%d::.ctor(string)\n", sid, nid);
}
/*----------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------*/
static void emit_net_bb_break(IR_t * nd, FILE * out, int sid, int nid) {
    const char * chars = nd->sval ? nd->sval : "";
    net_class_hdr(out, sid, nid);
    fprintf(out, "  .field private string _chars\n  .field private int32  _count\n");
    fprintf(out, "  .method public specialname rtspecialname instance void .ctor(string chars) cil managed\n  {\n");
    fprintf(out, "    .maxstack 3\n    ldarg.0\n    call       instance void [mscorlib]System.Object::.ctor()\n");
    fprintf(out, "    ldarg.0\n    ldarg.1\n    dup\n    brtrue     BRK_%d_%d_NN\n    pop\n    ldstr      \"\"\n", sid, nid);
    fprintf(out, "  BRK_%d_%d_NN:\n    stfld      string pat_%d_%d::_chars\n    ret\n  }\n", sid, nid, sid, nid);
    net_alpha_hdr(out);
    fprintf(out, "    .maxstack 4\n    .locals init (valuetype [boxes]Snobol4.Runtime.Boxes.Spec V_r)\n");
    fprintf(out, "    ldarg.0\n    ldc.i4.0\n    stfld      int32 pat_%d_%d::_count\n", sid, nid);
    fprintf(out, "  BRK_%d_%d_LOOP:\n", sid, nid);
    net_cursor_load(out); fprintf(out, "    ldarg.0\n    ldfld      int32 pat_%d_%d::_count\n    add\n", sid, nid);
    fprintf(out, "    ldarg.1\n"); net_ms_length(out);
    fprintf(out, "    bge        BRK_%d_%d_EOS\n", sid, nid);
    fprintf(out, "    ldarg.1\n"); net_cursor_load(out); fprintf(out, "    ldarg.0\n    ldfld      int32 pat_%d_%d::_count\n    add\n", sid, nid);
    fprintf(out, "    ldarg.0\n    ldfld      string pat_%d_%d::_chars\n", sid, nid);
    fprintf(out, "    callvirt   instance bool [boxes]Snobol4.Runtime.Boxes.MatchState::CharInSet(int32, string)\n");
    fprintf(out, "    brtrue     BRK_%d_%d_FOUND\n", sid, nid);
    fprintf(out, "    ldarg.0\n    ldarg.0\n    ldfld      int32 pat_%d_%d::_count\n    ldc.i4.1\n    add\n    stfld      int32 pat_%d_%d::_count\n", sid, nid, sid, nid);
    fprintf(out, "    br         BRK_%d_%d_LOOP\n", sid, nid);
    fprintf(out, "  BRK_%d_%d_EOS:\n", sid, nid); net_fail_ret(out);
    fprintf(out, "  BRK_%d_%d_FOUND:\n", sid, nid);
    net_cursor_load(out); fprintf(out, "    ldarg.0\n    ldfld      int32 pat_%d_%d::_count\n", sid, nid);
    net_spec_of(out); fprintf(out, "    stloc.0\n");
    fprintf(out, "    ldarg.1\n    ldarg.1\n"); net_cursor_load(out);
    fprintf(out, "    ldarg.0\n    ldfld      int32 pat_%d_%d::_count\n    add\n", sid, nid);
    fprintf(out, "    stfld      int32 [boxes]Snobol4.Runtime.Boxes.MatchState::Cursor\n");
    fprintf(out, "    ldloc.0\n    ret\n  }\n");
    net_beta_hdr(out);
    fprintf(out, "    .maxstack 3\n    ldarg.1\n    ldarg.1\n"); net_cursor_load(out);
    fprintf(out, "    ldarg.0\n    ldfld      int32 pat_%d_%d::_count\n    sub\n", sid, nid);
    fprintf(out, "    stfld      int32 [boxes]Snobol4.Runtime.Boxes.MatchState::Cursor\n");
    net_fail_ret(out); fprintf(out, "  }\n}\n");
    net_escape_ldstr(out, chars); fprintf(out, "    newobj     instance void pat_%d_%d::.ctor(string)\n", sid, nid);
}
/*----------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------*/
static void emit_net_bb_len(IR_t * nd, FILE * out, int sid, int nid) {
    int n = (int)nd->ival;
    net_class_hdr(out, sid, nid);
    fprintf(out, "  .field private int32 _n\n");
    fprintf(out, "  .method public specialname rtspecialname instance void .ctor(int32 n) cil managed\n  {\n");
    fprintf(out, "    .maxstack 2\n    ldarg.0\n    call       instance void [mscorlib]System.Object::.ctor()\n");
    fprintf(out, "    ldarg.0\n    ldarg.1\n    stfld      int32 pat_%d_%d::_n\n    ret\n  }\n", sid, nid);
    net_alpha_hdr(out);
    fprintf(out, "    .maxstack 4\n    .locals init (valuetype [boxes]Snobol4.Runtime.Boxes.Spec V_r)\n");
    net_cursor_load(out); fprintf(out, "    ldarg.0\n    ldfld      int32 pat_%d_%d::_n\n    add\n", sid, nid);
    fprintf(out, "    ldarg.1\n"); net_ms_length(out); fprintf(out, "    bgt        LEN_%d_%d_FAIL\n", sid, nid);
    net_cursor_load(out); fprintf(out, "    ldarg.0\n    ldfld      int32 pat_%d_%d::_n\n", sid, nid);
    net_spec_of(out); fprintf(out, "    stloc.0\n");
    fprintf(out, "    ldarg.1\n    ldarg.1\n"); net_cursor_load(out);
    fprintf(out, "    ldarg.0\n    ldfld      int32 pat_%d_%d::_n\n    add\n", sid, nid);
    fprintf(out, "    stfld      int32 [boxes]Snobol4.Runtime.Boxes.MatchState::Cursor\n");
    fprintf(out, "    ldloc.0\n    ret\n");
    fprintf(out, "  LEN_%d_%d_FAIL:\n", sid, nid); net_fail_ret(out); fprintf(out, "  }\n");
    net_beta_hdr(out);
    fprintf(out, "    .maxstack 3\n    ldarg.1\n    ldarg.1\n"); net_cursor_load(out);
    fprintf(out, "    ldarg.0\n    ldfld      int32 pat_%d_%d::_n\n    sub\n", sid, nid);
    fprintf(out, "    stfld      int32 [boxes]Snobol4.Runtime.Boxes.MatchState::Cursor\n");
    net_fail_ret(out); fprintf(out, "  }\n}\n");
    net_push_i4(out, n); fprintf(out, "    newobj     instance void pat_%d_%d::.ctor(int32)\n", sid, nid);
}
/*----------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------*/
static void emit_net_bb_pos(IR_t * nd, FILE * out, int sid, int nid) {
    int n = (int)nd->ival;
    net_class_hdr(out, sid, nid);
    fprintf(out, "  .field private int32 _n\n");
    fprintf(out, "  .method public specialname rtspecialname instance void .ctor(int32 n) cil managed\n  {\n");
    fprintf(out, "    .maxstack 2\n    ldarg.0\n    call       instance void [mscorlib]System.Object::.ctor()\n");
    fprintf(out, "    ldarg.0\n    ldarg.1\n    stfld      int32 pat_%d_%d::_n\n    ret\n  }\n", sid, nid);
    net_alpha_hdr(out);
    fprintf(out, "    .maxstack 2\n");
    net_cursor_load(out); fprintf(out, "    ldarg.0\n    ldfld      int32 pat_%d_%d::_n\n", sid, nid);
    fprintf(out, "    bne.un     POS_%d_%d_FAIL\n", sid, nid);
    net_cursor_load(out); net_spec_zw(out); fprintf(out, "    ret\n");
    fprintf(out, "  POS_%d_%d_FAIL:\n", sid, nid); net_fail_ret(out); fprintf(out, "  }\n");
    net_beta_hdr(out); fprintf(out, "    .maxstack 1\n"); net_fail_ret(out); fprintf(out, "  }\n}\n");
    net_push_i4(out, n); fprintf(out, "    newobj     instance void pat_%d_%d::.ctor(int32)\n", sid, nid);
}
/*----------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------*/
static void emit_net_bb_rpos(IR_t * nd, FILE * out, int sid, int nid) {
    int n = (int)nd->ival;
    net_class_hdr(out, sid, nid);
    fprintf(out, "  .field private int32 _n\n");
    fprintf(out, "  .method public specialname rtspecialname instance void .ctor(int32 n) cil managed\n  {\n");
    fprintf(out, "    .maxstack 2\n    ldarg.0\n    call       instance void [mscorlib]System.Object::.ctor()\n");
    fprintf(out, "    ldarg.0\n    ldarg.1\n    stfld      int32 pat_%d_%d::_n\n    ret\n  }\n", sid, nid);
    net_alpha_hdr(out);
    fprintf(out, "    .maxstack 3\n"); net_cursor_load(out);
    fprintf(out, "    ldarg.1\n"); net_ms_length(out); fprintf(out, "    ldarg.0\n    ldfld      int32 pat_%d_%d::_n\n    sub\n", sid, nid);
    fprintf(out, "    bne.un     RPOS_%d_%d_FAIL\n", sid, nid);
    net_cursor_load(out); net_spec_zw(out); fprintf(out, "    ret\n");
    fprintf(out, "  RPOS_%d_%d_FAIL:\n", sid, nid); net_fail_ret(out); fprintf(out, "  }\n");
    net_beta_hdr(out); fprintf(out, "    .maxstack 1\n"); net_fail_ret(out); fprintf(out, "  }\n}\n");
    net_push_i4(out, n); fprintf(out, "    newobj     instance void pat_%d_%d::.ctor(int32)\n", sid, nid);
}
/*----------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------*/
static void emit_net_bb_tab(IR_t * nd, FILE * out, int sid, int nid) {
    int n = (int)nd->ival;
    net_class_hdr(out, sid, nid);
    fprintf(out, "  .field private int32 _n\n  .field private int32 _advance\n");
    fprintf(out, "  .method public specialname rtspecialname instance void .ctor(int32 n) cil managed\n  {\n");
    fprintf(out, "    .maxstack 2\n    ldarg.0\n    call       instance void [mscorlib]System.Object::.ctor()\n");
    fprintf(out, "    ldarg.0\n    ldarg.1\n    stfld      int32 pat_%d_%d::_n\n    ret\n  }\n", sid, nid);
    net_alpha_hdr(out);
    fprintf(out, "    .maxstack 3\n    .locals init (valuetype [boxes]Snobol4.Runtime.Boxes.Spec V_r)\n");
    net_cursor_load(out); fprintf(out, "    ldarg.0\n    ldfld      int32 pat_%d_%d::_n\n    bgt        TAB_%d_%d_FAIL\n", sid, nid, sid, nid);
    fprintf(out, "    ldarg.0\n    ldarg.0\n    ldfld      int32 pat_%d_%d::_n\n", sid, nid);
    net_cursor_load(out); fprintf(out, "    sub\n    stfld      int32 pat_%d_%d::_advance\n", sid, nid);
    net_cursor_load(out); fprintf(out, "    ldarg.0\n    ldfld      int32 pat_%d_%d::_advance\n", sid, nid);
    net_spec_of(out); fprintf(out, "    stloc.0\n");
    fprintf(out, "    ldarg.1\n    ldarg.0\n    ldfld      int32 pat_%d_%d::_n\n", sid, nid);
    fprintf(out, "    stfld      int32 [boxes]Snobol4.Runtime.Boxes.MatchState::Cursor\n");
    fprintf(out, "    ldloc.0\n    ret\n");
    fprintf(out, "  TAB_%d_%d_FAIL:\n", sid, nid); net_fail_ret(out); fprintf(out, "  }\n");
    net_beta_hdr(out);
    fprintf(out, "    .maxstack 3\n    ldarg.1\n    ldarg.1\n"); net_cursor_load(out);
    fprintf(out, "    ldarg.0\n    ldfld      int32 pat_%d_%d::_advance\n    sub\n", sid, nid);
    fprintf(out, "    stfld      int32 [boxes]Snobol4.Runtime.Boxes.MatchState::Cursor\n");
    net_fail_ret(out); fprintf(out, "  }\n}\n");
    net_push_i4(out, n); fprintf(out, "    newobj     instance void pat_%d_%d::.ctor(int32)\n", sid, nid);
}
/*----------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------*/
static void emit_net_bb_rtab(IR_t * nd, FILE * out, int sid, int nid) {
    int n = (int)nd->ival;
    net_class_hdr(out, sid, nid);
    fprintf(out, "  .field private int32 _n\n  .field private int32 _advance\n");
    fprintf(out, "  .method public specialname rtspecialname instance void .ctor(int32 n) cil managed\n  {\n");
    fprintf(out, "    .maxstack 2\n    ldarg.0\n    call       instance void [mscorlib]System.Object::.ctor()\n");
    fprintf(out, "    ldarg.0\n    ldarg.1\n    stfld      int32 pat_%d_%d::_n\n    ret\n  }\n", sid, nid);
    net_alpha_hdr(out);
    fprintf(out, "    .maxstack 4\n    .locals init (int32 V_target, valuetype [boxes]Snobol4.Runtime.Boxes.Spec V_r)\n");
    fprintf(out, "    ldarg.1\n"); net_ms_length(out); fprintf(out, "    ldarg.0\n    ldfld      int32 pat_%d_%d::_n\n    sub\n    stloc.0\n", sid, nid);
    net_cursor_load(out); fprintf(out, "    ldloc.0\n    bgt        RTAB_%d_%d_FAIL\n", sid, nid);
    fprintf(out, "    ldarg.0\n    ldloc.0\n"); net_cursor_load(out); fprintf(out, "    sub\n    stfld      int32 pat_%d_%d::_advance\n", sid, nid);
    net_cursor_load(out); fprintf(out, "    ldarg.0\n    ldfld      int32 pat_%d_%d::_advance\n", sid, nid);
    net_spec_of(out); fprintf(out, "    stloc.1\n    ldarg.1\n    ldloc.0\n");
    fprintf(out, "    stfld      int32 [boxes]Snobol4.Runtime.Boxes.MatchState::Cursor\n");
    fprintf(out, "    ldloc.1\n    ret\n");
    fprintf(out, "  RTAB_%d_%d_FAIL:\n", sid, nid); net_fail_ret(out); fprintf(out, "  }\n");
    net_beta_hdr(out);
    fprintf(out, "    .maxstack 3\n    ldarg.1\n    ldarg.1\n"); net_cursor_load(out);
    fprintf(out, "    ldarg.0\n    ldfld      int32 pat_%d_%d::_advance\n    sub\n", sid, nid);
    fprintf(out, "    stfld      int32 [boxes]Snobol4.Runtime.Boxes.MatchState::Cursor\n");
    net_fail_ret(out); fprintf(out, "  }\n}\n");
    net_push_i4(out, n); fprintf(out, "    newobj     instance void pat_%d_%d::.ctor(int32)\n", sid, nid);
}
/*----------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------*/
static void emit_net_bb_rem(IR_t * nd, FILE * out, int sid, int nid) {
    (void)nd;
    net_class_hdr(out, sid, nid);
    net_ctor_none(out, sid, nid);
    net_alpha_hdr(out);
    fprintf(out, "    .maxstack 3\n    .locals init (valuetype [boxes]Snobol4.Runtime.Boxes.Spec V_r, int32 V_len)\n");
    fprintf(out, "    ldarg.1\n"); net_ms_length(out); net_cursor_load(out); fprintf(out, "    sub\n    stloc.1\n");
    net_cursor_load(out); fprintf(out, "    ldloc.1\n"); net_spec_of(out); fprintf(out, "    stloc.0\n");
    fprintf(out, "    ldarg.1\n    ldarg.1\n"); net_ms_length(out);
    fprintf(out, "    stfld      int32 [boxes]Snobol4.Runtime.Boxes.MatchState::Cursor\n");
    fprintf(out, "    ldloc.0\n    ret\n  }\n");
    net_beta_hdr(out); fprintf(out, "    .maxstack 1\n"); net_fail_ret(out); fprintf(out, "  }\n}\n");
    fprintf(out, "    newobj     instance void pat_%d_%d::.ctor()\n", sid, nid);
}
/*----------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------*/
static void emit_net_bb_arb(IR_t * nd, FILE * out, int sid, int nid) {
    (void)nd;
    net_class_hdr(out, sid, nid);
    fprintf(out, "  .field private int32 _count\n  .field private int32 _start\n");
    net_ctor_none(out, sid, nid);
    net_alpha_hdr(out);
    fprintf(out, "    .maxstack 2\n");
    fprintf(out, "    ldarg.0\n    ldc.i4.0\n    stfld      int32 pat_%d_%d::_count\n", sid, nid);
    fprintf(out, "    ldarg.0\n    ldarg.1\n    ldfld      int32 [boxes]Snobol4.Runtime.Boxes.MatchState::Cursor\n    stfld      int32 pat_%d_%d::_start\n", sid, nid);
    net_cursor_load(out); net_spec_zw(out); fprintf(out, "    ret\n  }\n");
    net_beta_hdr(out);
    fprintf(out, "    .maxstack 3\n");
    fprintf(out, "    ldarg.0\n    ldarg.0\n    ldfld      int32 pat_%d_%d::_count\n    ldc.i4.1\n    add\n    stfld      int32 pat_%d_%d::_count\n", sid, nid, sid, nid);
    fprintf(out, "    ldarg.0\n    ldfld      int32 pat_%d_%d::_start\n", sid, nid);
    fprintf(out, "    ldarg.0\n    ldfld      int32 pat_%d_%d::_count\n    add\n", sid, nid);
    fprintf(out, "    ldarg.1\n"); net_ms_length(out);
    fprintf(out, "    bgt        ARB_%d_%d_FAIL\n", sid, nid);
    fprintf(out, "    ldarg.1\n    ldarg.0\n    ldfld      int32 pat_%d_%d::_start\n", sid, nid);
    fprintf(out, "    ldarg.0\n    ldfld      int32 pat_%d_%d::_count\n    add\n", sid, nid);
    fprintf(out, "    stfld      int32 [boxes]Snobol4.Runtime.Boxes.MatchState::Cursor\n");
    fprintf(out, "    ldarg.0\n    ldfld      int32 pat_%d_%d::_start\n", sid, nid);
    fprintf(out, "    ldarg.0\n    ldfld      int32 pat_%d_%d::_count\n", sid, nid);
    net_spec_of(out); fprintf(out, "    ret\n");
    fprintf(out, "  ARB_%d_%d_FAIL:\n", sid, nid); net_fail_ret(out); fprintf(out, "  }\n}\n");
    fprintf(out, "    newobj     instance void pat_%d_%d::.ctor()\n", sid, nid);
}
/*----------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------*/
static void emit_net_bb_arbno(IR_t * nd, FILE * out, int sid, int nid) {
    (void)nd;
    net_class_hdr(out, sid, nid);
    fprintf(out, "  .field private class [boxes]Snobol4.Runtime.Boxes.IByrdBox _body\n");
    fprintf(out, "  .field private int32[] _matchStart\n  .field private int32[] _matchLen\n");
    fprintf(out, "  .field private int32[] _startStack\n  .field private int32   _depth\n");
    fprintf(out, "  .method public specialname rtspecialname instance void .ctor(class [boxes]Snobol4.Runtime.Boxes.IByrdBox body) cil managed\n  {\n");
    fprintf(out, "    .maxstack 2\n    ldarg.0\n    call       instance void [mscorlib]System.Object::.ctor()\n");
    fprintf(out, "    ldarg.0\n    ldarg.1\n    stfld      class [boxes]Snobol4.Runtime.Boxes.IByrdBox pat_%d_%d::_body\n", sid, nid);
    fprintf(out, "    ldarg.0\n    ldc.i4     64\n    newarr     [mscorlib]System.Int32\n    stfld      int32[] pat_%d_%d::_matchStart\n", sid, nid);
    fprintf(out, "    ldarg.0\n    ldc.i4     64\n    newarr     [mscorlib]System.Int32\n    stfld      int32[] pat_%d_%d::_matchLen\n", sid, nid);
    fprintf(out, "    ldarg.0\n    ldc.i4     64\n    newarr     [mscorlib]System.Int32\n    stfld      int32[] pat_%d_%d::_startStack\n    ret\n  }\n", sid, nid);
    net_alpha_hdr(out);
    fprintf(out, "    .maxstack 4\n    .locals init (int32 V_startHere, valuetype [boxes]Snobol4.Runtime.Boxes.Spec V_br)\n");
    fprintf(out, "    ldarg.0\n    ldc.i4.0\n    stfld      int32 pat_%d_%d::_depth\n", sid, nid);
    fprintf(out, "    ldarg.0\n    ldfld      int32[] pat_%d_%d::_matchStart\n    ldc.i4.0\n", sid, nid);
    net_cursor_load(out); fprintf(out, "    stelem.i4\n");
    fprintf(out, "    ldarg.0\n    ldfld      int32[] pat_%d_%d::_matchLen\n    ldc.i4.0\n    ldc.i4.0\n    stelem.i4\n", sid, nid);
    fprintf(out, "    ldarg.0\n    ldfld      int32[] pat_%d_%d::_startStack\n    ldc.i4.0\n", sid, nid);
    net_cursor_load(out); fprintf(out, "    stelem.i4\n");
    fprintf(out, "  ARBNO_%d_%d_LOOP:\n", sid, nid);
    net_cursor_load(out); fprintf(out, "    stloc.0\n");
    fprintf(out, "    ldarg.0\n    ldfld      class [boxes]Snobol4.Runtime.Boxes.IByrdBox pat_%d_%d::_body\n    ldarg.1\n", sid, nid);
    fprintf(out, "    callvirt   instance valuetype [boxes]Snobol4.Runtime.Boxes.Spec [boxes]Snobol4.Runtime.Boxes.IByrdBox::Alpha(class [boxes]Snobol4.Runtime.Boxes.MatchState)\n");
    fprintf(out, "    stloc.1\n    ldloca.s   V_br\n");
    fprintf(out, "    call       instance bool [boxes]Snobol4.Runtime.Boxes.Spec::get_IsFail()\n");
    fprintf(out, "    brtrue     ARBNO_%d_%d_STOP\n", sid, nid);
    net_cursor_load(out); fprintf(out, "    ldloc.0\n    beq        ARBNO_%d_%d_STOP\n", sid, nid);
    fprintf(out, "    ldarg.0\n    ldfld      int32 pat_%d_%d::_depth\n    ldc.i4     63\n    bge        ARBNO_%d_%d_STOP\n", sid, nid, sid, nid);
    fprintf(out, "    ldarg.0\n    ldarg.0\n    ldfld      int32 pat_%d_%d::_depth\n    ldc.i4.1\n    add\n    stfld      int32 pat_%d_%d::_depth\n", sid, nid, sid, nid);
    fprintf(out, "    ldarg.0\n    ldfld      int32[] pat_%d_%d::_matchStart\n    ldarg.0\n    ldfld      int32 pat_%d_%d::_depth\n", sid, nid, sid, nid);
    fprintf(out, "    ldarg.0\n    ldfld      int32[] pat_%d_%d::_matchStart\n    ldc.i4.0\n    ldelem.i4\n    stelem.i4\n", sid, nid);
    fprintf(out, "    ldarg.0\n    ldfld      int32[] pat_%d_%d::_matchLen\n    ldarg.0\n    ldfld      int32 pat_%d_%d::_depth\n", sid, nid, sid, nid);
    fprintf(out, "    ldarg.0\n    ldfld      int32[] pat_%d_%d::_matchLen\n", sid, nid);
    fprintf(out, "    ldarg.0\n    ldfld      int32 pat_%d_%d::_depth\n    ldc.i4.1\n    sub\n    ldelem.i4\n", sid, nid);
    fprintf(out, "    ldloca.s   V_br\n    ldfld      int32 [boxes]Snobol4.Runtime.Boxes.Spec::Length\n    add\n    stelem.i4\n");
    fprintf(out, "    ldarg.0\n    ldfld      int32[] pat_%d_%d::_startStack\n    ldarg.0\n    ldfld      int32 pat_%d_%d::_depth\n", sid, nid, sid, nid);
    net_cursor_load(out); fprintf(out, "    stelem.i4\n    br         ARBNO_%d_%d_LOOP\n", sid, nid);
    fprintf(out, "  ARBNO_%d_%d_STOP:\n", sid, nid);
    fprintf(out, "    ldarg.0\n    ldfld      int32[] pat_%d_%d::_matchStart\n", sid, nid);
    fprintf(out, "    ldarg.0\n    ldfld      int32 pat_%d_%d::_depth\n    ldelem.i4\n", sid, nid);
    fprintf(out, "    ldarg.0\n    ldfld      int32[] pat_%d_%d::_matchLen\n", sid, nid);
    fprintf(out, "    ldarg.0\n    ldfld      int32 pat_%d_%d::_depth\n    ldelem.i4\n", sid, nid);
    net_spec_of(out); fprintf(out, "    ret\n  }\n");
    net_beta_hdr(out);
    fprintf(out, "    .maxstack 3\n    ldarg.0\n    ldfld      int32 pat_%d_%d::_depth\n    ldc.i4.0\n    ble        ARBNO_%d_%d_BFAIL\n", sid, nid, sid, nid);
    fprintf(out, "    ldarg.0\n    ldarg.0\n    ldfld      int32 pat_%d_%d::_depth\n    ldc.i4.1\n    sub\n    stfld      int32 pat_%d_%d::_depth\n", sid, nid, sid, nid);
    fprintf(out, "    ldarg.1\n    ldarg.0\n    ldfld      int32[] pat_%d_%d::_startStack\n", sid, nid);
    fprintf(out, "    ldarg.0\n    ldfld      int32 pat_%d_%d::_depth\n    ldelem.i4\n", sid, nid);
    fprintf(out, "    stfld      int32 [boxes]Snobol4.Runtime.Boxes.MatchState::Cursor\n");
    fprintf(out, "    ldarg.0\n    ldfld      int32[] pat_%d_%d::_matchStart\n", sid, nid);
    fprintf(out, "    ldarg.0\n    ldfld      int32 pat_%d_%d::_depth\n    ldelem.i4\n", sid, nid);
    fprintf(out, "    ldarg.0\n    ldfld      int32[] pat_%d_%d::_matchLen\n", sid, nid);
    fprintf(out, "    ldarg.0\n    ldfld      int32 pat_%d_%d::_depth\n    ldelem.i4\n", sid, nid);
    net_spec_of(out); fprintf(out, "    ret\n");
    fprintf(out, "  ARBNO_%d_%d_BFAIL:\n", sid, nid); net_fail_ret(out); fprintf(out, "  }\n}\n");
    fprintf(out, "// ARBNO body box must be on stack before newobj\n");
    fprintf(out, "    newobj     instance void pat_%d_%d::.ctor(class [boxes]Snobol4.Runtime.Boxes.IByrdBox)\n", sid, nid);
}
/*----------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------*/
static void emit_net_bb_cat(IR_t * nd, FILE * out, int sid, int nid) {
    (void)nd;
    net_class_hdr(out, sid, nid);
    fprintf(out, "  .field private class [boxes]Snobol4.Runtime.Boxes.IByrdBox _left\n");
    fprintf(out, "  .field private class [boxes]Snobol4.Runtime.Boxes.IByrdBox _right\n");
    fprintf(out, "  .field private int32 _mStart\n  .field private int32 _mLen\n");
    fprintf(out, "  .method public specialname rtspecialname instance void .ctor(class [boxes]Snobol4.Runtime.Boxes.IByrdBox left, class [boxes]Snobol4.Runtime.Boxes.IByrdBox right) cil managed\n  {\n");
    fprintf(out, "    .maxstack 2\n    ldarg.0\n    call       instance void [mscorlib]System.Object::.ctor()\n");
    fprintf(out, "    ldarg.0\n    ldarg.1\n    stfld      class [boxes]Snobol4.Runtime.Boxes.IByrdBox pat_%d_%d::_left\n", sid, nid);
    fprintf(out, "    ldarg.0\n    ldarg.2\n    stfld      class [boxes]Snobol4.Runtime.Boxes.IByrdBox pat_%d_%d::_right\n    ret\n  }\n", sid, nid);
    net_alpha_hdr(out);
    fprintf(out, "    .maxstack 3\n    .locals init (valuetype [boxes]Snobol4.Runtime.Boxes.Spec V_lr, valuetype [boxes]Snobol4.Runtime.Boxes.Spec V_rr)\n");
    fprintf(out, "    ldarg.0\n"); net_cursor_load(out); fprintf(out, "    stfld      int32 pat_%d_%d::_mStart\n", sid, nid);
    fprintf(out, "    ldarg.0\n    ldfld      class [boxes]Snobol4.Runtime.Boxes.IByrdBox pat_%d_%d::_left\n    ldarg.1\n", sid, nid);
    fprintf(out, "    callvirt   instance valuetype [boxes]Snobol4.Runtime.Boxes.Spec [boxes]Snobol4.Runtime.Boxes.IByrdBox::Alpha(class [boxes]Snobol4.Runtime.Boxes.MatchState)\n");
    fprintf(out, "    stloc.0\n    ldloca.s   V_lr\n    call       instance bool [boxes]Snobol4.Runtime.Boxes.Spec::get_IsFail()\n");
    fprintf(out, "    brtrue     CAT_%d_%d_FAIL\n", sid, nid);
    fprintf(out, "    ldarg.0\n    ldloca.s   V_lr\n    ldfld      int32 [boxes]Snobol4.Runtime.Boxes.Spec::Length\n    stfld      int32 pat_%d_%d::_mLen\n", sid, nid);
    fprintf(out, "    ldarg.0\n    ldfld      class [boxes]Snobol4.Runtime.Boxes.IByrdBox pat_%d_%d::_right\n    ldarg.1\n", sid, nid);
    fprintf(out, "    callvirt   instance valuetype [boxes]Snobol4.Runtime.Boxes.Spec [boxes]Snobol4.Runtime.Boxes.IByrdBox::Alpha(class [boxes]Snobol4.Runtime.Boxes.MatchState)\n");
    fprintf(out, "    stloc.1\n    ldloca.s   V_rr\n    call       instance bool [boxes]Snobol4.Runtime.Boxes.Spec::get_IsFail()\n");
    fprintf(out, "    brtrue     CAT_%d_%d_FAIL\n", sid, nid);
    fprintf(out, "    ldarg.0\n    ldfld      int32 pat_%d_%d::_mStart\n", sid, nid);
    fprintf(out, "    ldarg.0\n    ldfld      int32 pat_%d_%d::_mLen\n", sid, nid);
    fprintf(out, "    ldloca.s   V_rr\n    ldfld      int32 [boxes]Snobol4.Runtime.Boxes.Spec::Length\n    add\n");
    net_spec_of(out); fprintf(out, "    ret\n");
    fprintf(out, "  CAT_%d_%d_FAIL:\n", sid, nid); net_fail_ret(out); fprintf(out, "  }\n");
    net_beta_hdr(out);
    fprintf(out, "    .maxstack 2\n    .locals init (valuetype [boxes]Snobol4.Runtime.Boxes.Spec V_rr)\n");
    fprintf(out, "    ldarg.0\n    ldfld      class [boxes]Snobol4.Runtime.Boxes.IByrdBox pat_%d_%d::_right\n    ldarg.1\n", sid, nid);
    fprintf(out, "    callvirt   instance valuetype [boxes]Snobol4.Runtime.Boxes.Spec [boxes]Snobol4.Runtime.Boxes.IByrdBox::Beta(class [boxes]Snobol4.Runtime.Boxes.MatchState)\n");
    fprintf(out, "    stloc.0\n    ldloca.s   V_rr\n    call       instance bool [boxes]Snobol4.Runtime.Boxes.Spec::get_IsFail()\n");
    fprintf(out, "    brfalse    CAT_%d_%d_BNOK\n", sid, nid);
    fprintf(out, "    ldarg.0\n    ldfld      class [boxes]Snobol4.Runtime.Boxes.IByrdBox pat_%d_%d::_left\n    ldarg.1\n", sid, nid);
    fprintf(out, "    callvirt   instance valuetype [boxes]Snobol4.Runtime.Boxes.Spec [boxes]Snobol4.Runtime.Boxes.IByrdBox::Beta(class [boxes]Snobol4.Runtime.Boxes.MatchState)\n");
    fprintf(out, "    ret\n");
    fprintf(out, "  CAT_%d_%d_BNOK:\n", sid, nid);
    fprintf(out, "    ldarg.0\n    ldfld      int32 pat_%d_%d::_mStart\n", sid, nid);
    fprintf(out, "    ldarg.0\n    ldfld      int32 pat_%d_%d::_mLen\n", sid, nid);
    fprintf(out, "    ldloca.s   V_rr\n    ldfld      int32 [boxes]Snobol4.Runtime.Boxes.Spec::Length\n    add\n");
    net_spec_of(out); fprintf(out, "    ret\n  }\n}\n");
    fprintf(out, "// CAT: left-box then right-box must be on stack before newobj\n");
    fprintf(out, "    newobj     instance void pat_%d_%d::.ctor(class [boxes]Snobol4.Runtime.Boxes.IByrdBox, class [boxes]Snobol4.Runtime.Boxes.IByrdBox)\n", sid, nid);
}
/*----------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------*/
static void emit_net_bb_alt(IR_t * nd, FILE * out, int sid, int nid) {
    int nkids = nd->c ? 0 : 0;
    (void)nd; (void)nkids;
    net_class_hdr(out, sid, nid);
    fprintf(out, "  .field private class [boxes]Snobol4.Runtime.Boxes.IByrdBox[] _children\n  .field private int32 _idx\n  .field private int32 _savedPos\n");
    fprintf(out, "  .method public specialname rtspecialname instance void .ctor(class [boxes]Snobol4.Runtime.Boxes.IByrdBox[] children) cil managed\n  {\n");
    fprintf(out, "    .maxstack 2\n    ldarg.0\n    call       instance void [mscorlib]System.Object::.ctor()\n");
    fprintf(out, "    ldarg.0\n    ldarg.1\n    stfld      class [boxes]Snobol4.Runtime.Boxes.IByrdBox[] pat_%d_%d::_children\n    ret\n  }\n", sid, nid);
    net_alpha_hdr(out);
    fprintf(out, "    .maxstack 4\n    .locals init (valuetype [boxes]Snobol4.Runtime.Boxes.Spec V_r)\n");
    fprintf(out, "    ldarg.0\n    ldc.i4.0\n    stfld      int32 pat_%d_%d::_idx\n", sid, nid);
    fprintf(out, "    ldarg.0\n    ldarg.1\n    ldfld      int32 [boxes]Snobol4.Runtime.Boxes.MatchState::Cursor\n    stfld      int32 pat_%d_%d::_savedPos\n", sid, nid);
    fprintf(out, "  ALT_%d_%d_LOOP:\n", sid, nid);
    fprintf(out, "    ldarg.0\n    ldfld      int32 pat_%d_%d::_idx\n", sid, nid);
    fprintf(out, "    ldarg.0\n    ldfld      class [boxes]Snobol4.Runtime.Boxes.IByrdBox[] pat_%d_%d::_children\n    ldlen\n    conv.i4\n", sid, nid);
    fprintf(out, "    bge        ALT_%d_%d_FAIL\n", sid, nid);
    fprintf(out, "    ldarg.1\n    ldarg.0\n    ldfld      int32 pat_%d_%d::_savedPos\n", sid, nid);
    fprintf(out, "    stfld      int32 [boxes]Snobol4.Runtime.Boxes.MatchState::Cursor\n");
    fprintf(out, "    ldarg.0\n    ldfld      class [boxes]Snobol4.Runtime.Boxes.IByrdBox[] pat_%d_%d::_children\n", sid, nid);
    fprintf(out, "    ldarg.0\n    ldfld      int32 pat_%d_%d::_idx\n    ldelem.ref\n    ldarg.1\n", sid, nid);
    fprintf(out, "    callvirt   instance valuetype [boxes]Snobol4.Runtime.Boxes.Spec [boxes]Snobol4.Runtime.Boxes.IByrdBox::Alpha(class [boxes]Snobol4.Runtime.Boxes.MatchState)\n");
    fprintf(out, "    stloc.0\n");
    fprintf(out, "    ldarg.0\n    ldarg.0\n    ldfld      int32 pat_%d_%d::_idx\n    ldc.i4.1\n    add\n    stfld      int32 pat_%d_%d::_idx\n", sid, nid, sid, nid);
    fprintf(out, "    ldloca.s   V_r\n    call       instance bool [boxes]Snobol4.Runtime.Boxes.Spec::get_IsFail()\n");
    fprintf(out, "    brtrue     ALT_%d_%d_LOOP\n", sid, nid);
    fprintf(out, "    ldloc.0\n    ret\n");
    fprintf(out, "  ALT_%d_%d_FAIL:\n", sid, nid); net_fail_ret(out); fprintf(out, "  }\n");
    net_beta_hdr(out);
    fprintf(out, "    .maxstack 4\n    .locals init (valuetype [boxes]Snobol4.Runtime.Boxes.Spec V_r)\n");
    fprintf(out, "  ALT_%d_%d_BLOOP:\n", sid, nid);
    fprintf(out, "    ldarg.0\n    ldfld      int32 pat_%d_%d::_idx\n", sid, nid);
    fprintf(out, "    ldarg.0\n    ldfld      class [boxes]Snobol4.Runtime.Boxes.IByrdBox[] pat_%d_%d::_children\n    ldlen\n    conv.i4\n", sid, nid);
    fprintf(out, "    bge        ALT_%d_%d_BFAIL\n", sid, nid);
    fprintf(out, "    ldarg.1\n    ldarg.0\n    ldfld      int32 pat_%d_%d::_savedPos\n", sid, nid);
    fprintf(out, "    stfld      int32 [boxes]Snobol4.Runtime.Boxes.MatchState::Cursor\n");
    fprintf(out, "    ldarg.0\n    ldfld      class [boxes]Snobol4.Runtime.Boxes.IByrdBox[] pat_%d_%d::_children\n", sid, nid);
    fprintf(out, "    ldarg.0\n    ldfld      int32 pat_%d_%d::_idx\n    ldelem.ref\n    ldarg.1\n", sid, nid);
    fprintf(out, "    callvirt   instance valuetype [boxes]Snobol4.Runtime.Boxes.Spec [boxes]Snobol4.Runtime.Boxes.IByrdBox::Alpha(class [boxes]Snobol4.Runtime.Boxes.MatchState)\n");
    fprintf(out, "    stloc.0\n");
    fprintf(out, "    ldarg.0\n    ldarg.0\n    ldfld      int32 pat_%d_%d::_idx\n    ldc.i4.1\n    add\n    stfld      int32 pat_%d_%d::_idx\n", sid, nid, sid, nid);
    fprintf(out, "    ldloca.s   V_r\n    call       instance bool [boxes]Snobol4.Runtime.Boxes.Spec::get_IsFail()\n");
    fprintf(out, "    brtrue     ALT_%d_%d_BLOOP\n    ldloc.0\n    ret\n", sid, nid);
    fprintf(out, "  ALT_%d_%d_BFAIL:\n", sid, nid); net_fail_ret(out); fprintf(out, "  }\n}\n");
    fprintf(out, "// ALT: IByrdBox[] array must be on stack before newobj\n");
    fprintf(out, "    newobj     instance void pat_%d_%d::.ctor(class [boxes]Snobol4.Runtime.Boxes.IByrdBox[])\n", sid, nid);
}
/*----------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------*/
static void emit_net_bb_capture(IR_t * nd, FILE * out, int sid, int nid, int imm) {
    const char * varname = nd->sval ? nd->sval : "";
    net_class_hdr(out, sid, nid);
    fprintf(out, "  .field private class [boxes]Snobol4.Runtime.Boxes.IByrdBox _child\n  .field private string _varname\n  .field private bool _immediate\n");
    fprintf(out, "  .method public specialname rtspecialname instance void .ctor(class [boxes]Snobol4.Runtime.Boxes.IByrdBox child, string varname, bool imm) cil managed\n  {\n");
    fprintf(out, "    .maxstack 2\n    ldarg.0\n    call       instance void [mscorlib]System.Object::.ctor()\n");
    fprintf(out, "    ldarg.0\n    ldarg.1\n    stfld      class [boxes]Snobol4.Runtime.Boxes.IByrdBox pat_%d_%d::_child\n", sid, nid);
    fprintf(out, "    ldarg.0\n    ldarg.2\n    dup\n    brtrue     CAP_%d_%d_NN\n    pop\n    ldstr      \"\"\n", sid, nid);
    fprintf(out, "  CAP_%d_%d_NN:\n    stfld      string pat_%d_%d::_varname\n", sid, nid, sid, nid);
    fprintf(out, "    ldarg.0\n    ldarg.3\n    stfld      bool pat_%d_%d::_immediate\n    ret\n  }\n", sid, nid);
    net_alpha_hdr(out);
    fprintf(out, "    .maxstack 3\n    .locals init (valuetype [boxes]Snobol4.Runtime.Boxes.Spec V_cr, int32 V_start, int32 V_len, string V_matched)\n");
    fprintf(out, "    ldarg.1\n    ldfld      int32 [boxes]Snobol4.Runtime.Boxes.MatchState::Cursor\n    stloc.1\n");
    fprintf(out, "    ldarg.0\n    ldfld      class [boxes]Snobol4.Runtime.Boxes.IByrdBox pat_%d_%d::_child\n    ldarg.1\n", sid, nid);
    fprintf(out, "    callvirt   instance valuetype [boxes]Snobol4.Runtime.Boxes.Spec [boxes]Snobol4.Runtime.Boxes.IByrdBox::Alpha(class [boxes]Snobol4.Runtime.Boxes.MatchState)\n");
    fprintf(out, "    stloc.0\n    ldloca.s   V_cr\n    call       instance bool [boxes]Snobol4.Runtime.Boxes.Spec::get_IsFail()\n");
    fprintf(out, "    brtrue     CAPC_%d_%d_FAIL\n", sid, nid);
    fprintf(out, "    ldloca.s   V_cr\n    ldfld      int32 [boxes]Snobol4.Runtime.Boxes.Spec::Start\n    stloc.1\n");
    fprintf(out, "    ldloca.s   V_cr\n    ldfld      int32 [boxes]Snobol4.Runtime.Boxes.Spec::Length\n    stloc.2\n");
    fprintf(out, "    ldarg.1\n    callvirt   instance string [boxes]Snobol4.Runtime.Boxes.MatchState::get_Subject()\n");
    fprintf(out, "    ldloc.1\n    ldloc.2\n    callvirt   instance string [mscorlib]System.String::Substring(int32, int32)\n    stloc.3\n");
    fprintf(out, "    ldarg.0\n    ldfld      string pat_%d_%d::_varname\n", sid, nid);
    fprintf(out, "    ldstr      \"OUTPUT\"\n    call       bool [mscorlib]System.String::op_Equality(string, string)\n");
    fprintf(out, "    brfalse    CAPC_%d_%d_NOTOUT\n", sid, nid);
    fprintf(out, "    ldloc.3\n    call       void [mscorlib]System.Console::WriteLine(string)\n");
    fprintf(out, "    br         CAPC_%d_%d_DONE\n", sid, nid);
    fprintf(out, "  CAPC_%d_%d_NOTOUT:\n", sid, nid);
    fprintf(out, "  CAPC_%d_%d_DONE:\n    ldloc.0\n    ret\n", sid, nid);
    fprintf(out, "  CAPC_%d_%d_FAIL:\n", sid, nid); net_fail_ret(out); fprintf(out, "  }\n");
    net_beta_hdr(out);
    fprintf(out, "    .maxstack 2\n    ldarg.0\n    ldfld      class [boxes]Snobol4.Runtime.Boxes.IByrdBox pat_%d_%d::_child\n    ldarg.1\n", sid, nid);
    fprintf(out, "    callvirt   instance valuetype [boxes]Snobol4.Runtime.Boxes.Spec [boxes]Snobol4.Runtime.Boxes.IByrdBox::Beta(class [boxes]Snobol4.Runtime.Boxes.MatchState)\n");
    fprintf(out, "    ret\n  }\n}\n");
    fprintf(out, "// CAPTURE: child box, varname string, imm bool must be on stack\n");
    fprintf(out, "// child box already on stack from prior emit\n");
    net_escape_ldstr(out, varname);
    net_push_i4(out, imm);
    fprintf(out, "    newobj     instance void pat_%d_%d::.ctor(class [boxes]Snobol4.Runtime.Boxes.IByrdBox, string, bool)\n", sid, nid);
}
/*----------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------*/
static void emit_net_bb_fence(IR_t * nd, FILE * out, int sid, int nid) {
    (void)nd;
    net_class_hdr(out, sid, nid);
    net_ctor_none(out, sid, nid);
    net_alpha_hdr(out);
    fprintf(out, "    .maxstack 1\n");
    net_cursor_load(out); net_spec_zw(out); fprintf(out, "    ret\n  }\n");
    net_beta_hdr(out); fprintf(out, "    .maxstack 1\n"); net_fail_ret(out); fprintf(out, "  }\n}\n");
    fprintf(out, "    newobj     instance void pat_%d_%d::.ctor()\n", sid, nid);
}
/*----------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------*/
static void emit_net_bb_abort(IR_t * nd, FILE * out, int sid, int nid) {
    (void)nd;
    net_class_hdr(out, sid, nid);
    net_ctor_none(out, sid, nid);
    net_alpha_hdr(out); fprintf(out, "    .maxstack 1\n"); net_fail_ret(out); fprintf(out, "  }\n");
    net_beta_hdr(out); fprintf(out, "    .maxstack 1\n"); net_fail_ret(out); fprintf(out, "  }\n}\n");
    fprintf(out, "    newobj     instance void pat_%d_%d::.ctor()\n", sid, nid);
}
/*----------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------*/
/* emit_net_generator — dispatch to the 19 BB emitter functions. */
static int emit_net_generator(IR_t * nd, FILE * out) {
    static int sid = 0, nid = 0;
    sid++; nid = (int)((size_t)nd % 99991);
    switch (nd->t) {
    case IR_PAT_LIT:         emit_net_bb_lit(nd, out, sid, nid);                  break;
    case IR_PAT_ANY:         emit_net_bb_any(nd, out, sid, nid);                  break;
    case IR_PAT_NOTANY:      emit_net_bb_notany(nd, out, sid, nid);               break;
    case IR_PAT_SPAN:        emit_net_bb_span(nd, out, sid, nid);                 break;
    case IR_PAT_BREAK:       emit_net_bb_break(nd, out, sid, nid);                break;
    case IR_PAT_LEN:         emit_net_bb_len(nd, out, sid, nid);                  break;
    case IR_PAT_POS:         if (nd->ival2) emit_net_bb_rpos(nd, out, sid, nid);
                             else           emit_net_bb_pos(nd, out, sid, nid);   break;
    case IR_PAT_TAB:         if (nd->ival2) emit_net_bb_rtab(nd, out, sid, nid);
                             else           emit_net_bb_tab(nd, out, sid, nid);   break;
    case IR_PAT_REM:         emit_net_bb_rem(nd, out, sid, nid);                  break;
    case IR_PAT_ARB:         emit_net_bb_arb(nd, out, sid, nid);                  break;
    case IR_PAT_ARBNO:       emit_net_bb_arbno(nd, out, sid, nid);                break;
    case IR_PAT_CAT:         emit_net_bb_cat(nd, out, sid, nid);                  break;
    case IR_PAT_ALT:         emit_net_bb_alt(nd, out, sid, nid);                  break;
    case IR_PAT_ASSIGN_IMM:  emit_net_bb_capture(nd, out, sid, nid, 1);           break;
    case IR_PAT_ASSIGN_COND: emit_net_bb_capture(nd, out, sid, nid, 0);           break;
    case IR_PAT_FENCE:       emit_net_bb_fence(nd, out, sid, nid);                break;
    case IR_PAT_ABORT:       emit_net_bb_abort(nd, out, sid, nid);                break;
    default:
        fprintf(out, "// [net generator kind=%d unimplemented]\n", (int)nd->t);
        break;
    }
    return 0;
}
/*----------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------*/
/* emit_net_scalar — emit MSIL for one SM_Instr as a SnoRt static call. */
static int emit_net_scalar(IR_t * nd, FILE * out) {
    (void)nd; (void)out;
    return 0;
}
/*----------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------*/
/* net_parse_define_proto — parse a DEFINE prototype string "FuncName(p1,p2,...)[locals]"
 * Fills *out_fname (malloc'd) with the function name, and returns a malloc'd NULL-terminated
 * array of malloc'd param names. Caller must free both. On no parens, returns empty list. */
static char ** net_parse_define_proto(const char * proto, char ** out_fname, int * out_n) {
    *out_fname = NULL;
    *out_n = 0;
    if (!proto) return NULL;
    const char * lp = strchr(proto, '(');
    const char * rp = lp ? strchr(lp, ')') : NULL;
    if (!lp) {
        /* No parens — function with no formal parameters */
        size_t flen = strlen(proto);
        char * fn = (char *)malloc(flen + 1);
        memcpy(fn, proto, flen); fn[flen] = '\0';
        *out_fname = fn;
        return NULL;
    }
    size_t flen = (size_t)(lp - proto);
    char * fn = (char *)malloc(flen + 1);
    memcpy(fn, proto, flen); fn[flen] = '\0';
    *out_fname = fn;
    if (!rp || rp <= lp + 1) return NULL;
    /* Parse comma-separated param list between lp+1 and rp */
    int cap = 4, count = 0;
    char ** params = (char **)malloc((size_t)(cap + 1) * sizeof(char *));
    const char * s = lp + 1;
    while (s < rp) {
        while (s < rp && (*s == ' ' || *s == '\t')) s++;
        const char * pstart = s;
        while (s < rp && *s != ',' && *s != ' ' && *s != '\t') s++;
        size_t plen = (size_t)(s - pstart);
        if (plen > 0) {
            if (count >= cap) { cap *= 2; params = (char **)realloc(params, (size_t)(cap + 1) * sizeof(char *)); }
            char * p = (char *)malloc(plen + 1);
            memcpy(p, pstart, plen); p[plen] = '\0';
            params[count++] = p;
        }
        while (s < rp && (*s == ' ' || *s == '\t')) s++;
        if (s < rp && *s == ',') s++;
    }
    params[count] = NULL;
    *out_n = count;
    return params;
}
/*----------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------*/
/* emit_net_from_sm — walk SM_Program and emit MSIL switch-dispatch loop. */
static int emit_net_from_sm(SM_Program * sm, FILE * out) {
    if (!sm || !out) return 0;
    int n = sm->count;
    int * fn_pcs = NULL;
    const char ** fn_names = NULL;
    char *** fn_params = NULL;   /* fn_params[k] = NULL-terminated array of param names for function k */
    int * fn_nparams = NULL;
    int fn_count = 0;
    int * pc_to_fn = NULL;
    if (n > 0) {
        fn_pcs = (int *)calloc((size_t)n, sizeof(int));
        fn_names = (const char **)calloc((size_t)n, sizeof(const char *));
        fn_params = (char ***)calloc((size_t)n, sizeof(char **));
        fn_nparams = (int *)calloc((size_t)n, sizeof(int));
        pc_to_fn = (int *)malloc((size_t)n * sizeof(int));
        for (int p = 0; p < n; p++) pc_to_fn[p] = -1;
        /* First pass: find define_entry labels */
        for (int i = 0; i < n; i++) {
            SM_Instr * ins = &sm->instrs[i];
            if (ins->op == SM_LABEL && ins->a[2].i && ins->a[0].s) {
                fn_pcs[fn_count] = i;
                fn_names[fn_count] = ins->a[0].s;
                fn_count++;
            }
        }
        /* Second pass: scan for SM_CALL_FN "DEFINE" with preceding PUSH_LIT_S,
         * parse the prototype, and link param lists to function entries by name.
         * Note: scrip's SNOBOL4 lowering emits all named calls as SM_CALL_FN — the
         * SM_SUSPEND_VALUE opcode is reserved for generator-style yields and is not
         * used by the SNOBOL4 frontend.  (The sm_prog.c dump names[] array contains
         * an extra "SM_GEN_TICK" entry not in the header enum, causing the dump to
         * print SM_CALL_FN instructions as "SM_SUSPEND_VALUE" and SM_RETURN
         * instructions as "SM_CALL_FN" — a cosmetic mismatch that does not change
         * the actual enum value.) */
        for (int i = 1; i < n; i++) {
            SM_Instr * ins = &sm->instrs[i];
            if (ins->op != SM_CALL_FN && ins->op != SM_SUSPEND_VALUE) continue;
            if (!ins->a[0].s || strcmp(ins->a[0].s, "DEFINE") != 0) continue;
            SM_Instr * prev = &sm->instrs[i - 1];
            if (prev->op != SM_PUSH_LIT_S && prev->op != SM_PUSH_LIT_CS) continue;
            if (!prev->a[0].s) continue;
            char * fname = NULL; int npar = 0;
            char ** pars = net_parse_define_proto(prev->a[0].s, &fname, &npar);
            if (fname) {
                for (int k = 0; k < fn_count; k++) {
                    if (fn_names[k] && strcmp(fn_names[k], fname) == 0) {
                        fn_params[k] = pars;
                        fn_nparams[k] = npar;
                        pars = NULL;
                        break;
                    }
                }
                free(fname);
                if (pars) {
                    for (int q = 0; q < npar; q++) free(pars[q]);
                    free(pars);
                }
            }
        }
        /* Third pass: build a per-PC mapping of which function (if any) each PC belongs to.
         * A function's body extends from its entry_pc through the corresponding SM_JUMP-around
         * target minus one. We find this by looking backward from each entry_pc for the
         * SM_JUMP that jumps to a PC strictly past entry_pc — that's the around-jump. */
        for (int k = 0; k < fn_count; k++) {
            int entry = fn_pcs[k];
            int around_target = -1;
            for (int p = entry - 1; p >= 0; p--) {
                SM_Instr * pi = &sm->instrs[p];
                if (pi->op == SM_JUMP && (int)pi->a[0].i > entry) {
                    around_target = (int)pi->a[0].i;
                    break;
                }
            }
            int body_end = (around_target > 0) ? around_target - 1 : entry;
            for (int p = entry; p <= body_end && p < n; p++) {
                if (pc_to_fn[p] < 0) pc_to_fn[p] = k;
            }
        }
    }
    fprintf(out, "    .locals init (int32 _pc)\n");
    fprintf(out, "    ldc.i4.0\n    stloc      _pc\n");
    fprintf(out, "  NET_DISPATCH:\n    ldloc      _pc\n");
    fprintf(out, "    switch (");
    for (int i = 0; i < n; i++) { fprintf(out, "NET_L%d", i); if (i < n - 1) fprintf(out, ", "); }
    fprintf(out, ")\n    br         NET_DONE\n");
    for (int i = 0; i < n; i++) {
        SM_Instr * instr = &sm->instrs[i];
        fprintf(out, "  NET_L%d:\n", i);
        int has_continue = 0;
        switch (instr->op) {
        case SM_STNO:
            net_push_i4(out, (int)instr->a[0].i);
            fprintf(out, "    call       void SnoRt::set_stno(int32)\n");
            break;
        case SM_PUSH_LIT_I:
            net_push_i4(out, (int)instr->a[0].i);
            fprintf(out, "    call       void SnoRt::push_int(int32)\n");
            break;
        case SM_PUSH_LIT_S:
        case SM_PUSH_LIT_CS:
            net_escape_ldstr(out, instr->a[0].s ? instr->a[0].s : "");
            net_push_i4(out, instr->a[0].s ? (int)strlen(instr->a[0].s) : 0);
            fprintf(out, "    call       void SnoRt::push_str(string, int32)\n");
            break;
        case SM_PUSH_LIT_F:
            fprintf(out, "    ldc.r8     %.17g\n", instr->a[0].f);
            fprintf(out, "    call       void SnoRt::push_real(float64)\n");
            break;
        case SM_PUSH_NULL:
        case SM_PUSH_NULL_NOFLIP:
            fprintf(out, "    call       void SnoRt::push_null()\n");
            break;
        case SM_PUSH_VAR:
            net_escape_ldstr(out, instr->a[0].s ? instr->a[0].s : "");
            fprintf(out, "    call       void SnoRt::push_var(string)\n");
            break;
        case SM_STORE_VAR:
            net_escape_ldstr(out, instr->a[0].s ? instr->a[0].s : "");
            fprintf(out, "    call       void SnoRt::store_var(string)\n");
            break;
        case SM_VOID_POP:
            fprintf(out, "    call       void SnoRt::pop_void()\n");
            break;
        case SM_CONCAT:
            fprintf(out, "    call       void SnoRt::concat()\n");
            break;
        case SM_NEG:
            fprintf(out, "    call       void SnoRt::negate()\n");
            break;
        case SM_COERCE_NUM:
            fprintf(out, "    call       void SnoRt::coerce_num()\n");
            break;
        case SM_EXP:
            fprintf(out, "    call       void SnoRt::exp_op()\n");
            break;
        case SM_ADD:
            fprintf(out, "    ldc.i4.1\n    call       void SnoRt::arith(int32)\n");
            break;
        case SM_SUB:
            fprintf(out, "    ldc.i4.2\n    call       void SnoRt::arith(int32)\n");
            break;
        case SM_MUL:
            fprintf(out, "    ldc.i4.3\n    call       void SnoRt::arith(int32)\n");
            break;
        case SM_DIV:
            fprintf(out, "    ldc.i4.4\n    call       void SnoRt::arith(int32)\n");
            break;
        case SM_MOD:
            net_push_i4(out, 6);
            fprintf(out, "    call       void SnoRt::arith(int32)\n");
            break;
        case SM_ACOMP:
            net_push_i4(out, (int)instr->a[0].i);
            fprintf(out, "    call       void SnoRt::acomp(int32)\n");
            break;
        case SM_LCOMP:
            net_push_i4(out, (int)instr->a[0].i);
            fprintf(out, "    call       void SnoRt::lcomp(int32)\n");
            break;
        case SM_HALT:
            fprintf(out, "    call       void SnoRt::halt_tos()\n");
            fprintf(out, "    br         NET_DONE\n");
            has_continue = 1;
            break;
        case SM_LABEL:
            if (instr->a[2].i && instr->a[0].s) {
                /* define_entry: set last_ok=true so the function-entry NRETURN_F
                 * guard (which fires only on last_ok==false) is skipped. */
                fprintf(out, "    ldc.i4.1\n");
                fprintf(out, "    call       void SnoRt::set_last_ok(bool)\n");
                /* Bind args to params in reverse pop order */
                int k = -1;
                for (int q = 0; q < fn_count; q++) {
                    if (fn_pcs[q] == i) { k = q; break; }
                }
                if (k >= 0 && fn_params[k] && fn_nparams[k] > 0) {
                    for (int p = fn_nparams[k] - 1; p >= 0; p--) {
                        net_escape_ldstr(out, fn_params[k][p]);
                        fprintf(out, "    call       void SnoRt::store_var(string)\n");
                    }
                }
            }
            break;
        case SM_JUMP:
            fprintf(out, "    ldc.i4     %lld\n    stloc      _pc\n    br         NET_DISPATCH\n", instr->a[0].i);
            has_continue = 1;
            break;
        case SM_JUMP_S:
            fprintf(out, "    call       bool SnoRt::last_ok()\n");
            fprintf(out, "    brfalse    NET_L%d\n", i + 1);
            fprintf(out, "    ldc.i4     %lld\n    stloc      _pc\n    br         NET_DISPATCH\n", instr->a[0].i);
            has_continue = 1;
            break;
        case SM_JUMP_F:
            fprintf(out, "    call       bool SnoRt::last_ok()\n");
            fprintf(out, "    brtrue     NET_L%d\n", i + 1);
            fprintf(out, "    ldc.i4     %lld\n    stloc      _pc\n    br         NET_DISPATCH\n", instr->a[0].i);
            has_continue = 1;
            break;
        case SM_SUSPEND_VALUE:
        case SM_CALL_FN: {
            const char * cname = instr->a[0].s ? instr->a[0].s : "";
            int entry_pc = -1;
            for (int k = 0; k < fn_count; k++) {
                if (fn_names[k] && strcmp(fn_names[k], cname) == 0) { entry_pc = fn_pcs[k]; break; }
            }
            if (entry_pc >= 0) {
                net_push_i4(out, i + 1);
                fprintf(out, "    call       void SnoRt::push_ret_pc(int32)\n");
                net_push_i4(out, entry_pc);
                fprintf(out, "    stloc      _pc\n    br         NET_DISPATCH\n");
                has_continue = 1;
            } else {
                net_escape_ldstr(out, cname);
                net_push_i4(out, (int)instr->a[1].i);
                fprintf(out, "    call       void SnoRt::sno_call(string, int32)\n");
            }
            break;
        }
        case SM_RETURN:
        case SM_RETURN_S:
        case SM_RETURN_F: {
            int fk = (i >= 0 && i < n && pc_to_fn) ? pc_to_fn[i] : -1;
            const char * fname = (fk >= 0) ? fn_names[fk] : NULL;
            /* _S: skip if !last_ok; _F: skip if last_ok */
            if (instr->op == SM_RETURN_S) {
                fprintf(out, "    call       bool SnoRt::last_ok()\n");
                fprintf(out, "    brfalse    NET_L%d\n", i + 1);
            } else if (instr->op == SM_RETURN_F) {
                fprintf(out, "    call       bool SnoRt::last_ok()\n");
                fprintf(out, "    brtrue     NET_L%d\n", i + 1);
            }
            if (fname) {
                net_escape_ldstr(out, fname);
                fprintf(out, "    call       void SnoRt::push_var(string)\n");
            } else {
                fprintf(out, "    call       void SnoRt::push_null()\n");
            }
            net_push_i4(out, 0);
            net_push_i4(out, 1);
            fprintf(out, "    call       void SnoRt::do_return(int32, bool)\n");
            fprintf(out, "    call       int32 SnoRt::pop_ret_pc()\n");
            fprintf(out, "    stloc      _pc\n    br         NET_DISPATCH\n");
            has_continue = 1;
            break;
        }
        case SM_FRETURN:
        case SM_FRETURN_S:
        case SM_FRETURN_F:
            if (instr->op == SM_FRETURN_S) {
                fprintf(out, "    call       bool SnoRt::last_ok()\n");
                fprintf(out, "    brfalse    NET_L%d\n", i + 1);
            } else if (instr->op == SM_FRETURN_F) {
                fprintf(out, "    call       bool SnoRt::last_ok()\n");
                fprintf(out, "    brtrue     NET_L%d\n", i + 1);
            }
            fprintf(out, "    call       void SnoRt::push_null()\n");
            net_push_i4(out, 1);
            net_push_i4(out, 0);
            fprintf(out, "    call       void SnoRt::do_return(int32, bool)\n");
            fprintf(out, "    call       int32 SnoRt::pop_ret_pc()\n");
            fprintf(out, "    stloc      _pc\n    br         NET_DISPATCH\n");
            has_continue = 1;
            break;
        case SM_NRETURN:
        case SM_NRETURN_S:
        case SM_NRETURN_F: {
            int fk = (i >= 0 && i < n && pc_to_fn) ? pc_to_fn[i] : -1;
            const char * fname = (fk >= 0) ? fn_names[fk] : NULL;
            if (instr->op == SM_NRETURN_S) {
                fprintf(out, "    call       bool SnoRt::last_ok()\n");
                fprintf(out, "    brfalse    NET_L%d\n", i + 1);
            } else if (instr->op == SM_NRETURN_F) {
                fprintf(out, "    call       bool SnoRt::last_ok()\n");
                fprintf(out, "    brtrue     NET_L%d\n", i + 1);
            }
            if (fname) {
                net_escape_ldstr(out, fname);
                fprintf(out, "    call       void SnoRt::push_var(string)\n");
            } else {
                fprintf(out, "    call       void SnoRt::push_null()\n");
            }
            net_push_i4(out, 2);
            net_push_i4(out, 0);
            fprintf(out, "    call       void SnoRt::do_return(int32, bool)\n");
            fprintf(out, "    call       int32 SnoRt::pop_ret_pc()\n");
            fprintf(out, "    stloc      _pc\n    br         NET_DISPATCH\n");
            has_continue = 1;
            break;
        }
        case SM_DEFINE_ENTRY:
        case SM_DEFINE:
        case SM_EXEC_STMT:
        case SM_PUSH_EXPRESSION:
        case SM_CALL_EXPRESSION:
        case SM_PUSH_EXPR:
        case SM_INCR:
        case SM_DECR:
        case SM_LOAD_FRAME:
        case SM_STORE_FRAME:
        case SM_LOAD_GLOCAL:
        case SM_STORE_GLOCAL:
        case SM_SUSPEND:
        case SM_PAT_LIT:
        case SM_PAT_ANY:
        case SM_PAT_NOTANY:
        case SM_PAT_SPAN:
        case SM_PAT_BREAK:
        case SM_PAT_LEN:
        case SM_PAT_POS:
        case SM_PAT_RPOS:
        case SM_PAT_TAB:
        case SM_PAT_RTAB:
        case SM_PAT_REM:
        case SM_PAT_BAL:
        case SM_PAT_FENCE0:
        case SM_PAT_FENCE1:
        case SM_PAT_ABORT:
        case SM_PAT_FAIL:
        case SM_PAT_SUCCEED:
        case SM_PAT_EPS:
        case SM_PAT_ALT:
        case SM_PAT_CAT:
        case SM_PAT_DEREF:
        case SM_PAT_REFNAME:
        case SM_PAT_CAPTURE:
        case SM_PAT_CAPTURE_FN:
        case SM_PAT_CAPTURE_FN_ARGS:
        case SM_PAT_USERCALL:
        case SM_PAT_USERCALL_ARGS:
        case SM_BB_PUMP:
        case SM_BB_ONCE:
        case SM_BB_EVAL:
        case SM_BB_ONCE_PROC:
        case SM_BB_PUMP_PROC:
        case SM_BB_PUMP_CASE:
        case SM_BB_PUMP_SM:
        case SM_BB_PUMP_EVERY:
        case SM_EXEC_BB:
        case SM_PUMP_BB:
        case SM_ICMP_GT:
        case SM_ICMP_LT:
            break;
        default:
            fprintf(out, "    // [net SM op %d unimplemented]\n", (int)instr->op);
            break;
        }
        if (!has_continue && i + 1 < n) { net_push_i4(out, i + 1); fprintf(out, "    stloc      _pc\n    br         NET_DISPATCH\n"); }
    }
    fprintf(out, "  NET_DONE:\n");
    if (fn_params) {
        for (int k = 0; k < fn_count; k++) {
            if (fn_params[k]) {
                for (int q = 0; q < fn_nparams[k]; q++) free(fn_params[k][q]);
                free(fn_params[k]);
            }
        }
        free(fn_params);
    }
    free(fn_nparams);
    free(pc_to_fn);
    free(fn_pcs);
    free(fn_names);
    return 0;
}
/*----------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------*/
static int emit_net_prologue(IR_block_t * cfg, FILE * out) {
    (void)cfg;
    fprintf(out, "// .NET MSIL output -- generated by scrip --sm-emit --target=net\n");
    fprintf(out, ".assembly extern mscorlib {}\n");
    fprintf(out, ".assembly extern boxes {}\n");
    fprintf(out, ".assembly Prog {}\n");
    fprintf(out, ".module Prog.exe\n");
    fprintf(out, ".class public auto ansi beforefieldinit Prog extends [mscorlib]System.Object\n{\n");
    fprintf(out, "  .method public static void Main() cil managed\n  {\n");
    fprintf(out, "    .entrypoint\n    .maxstack 8\n");
    fprintf(out, "    call       void SnoRt::_init()\n");
    return 0;
}
/*----------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------*/
static int emit_net_epilogue(IR_block_t * cfg, FILE * out) {
    (void)cfg;
    fprintf(out, "    call       void SnoRt::_finalize()\n");
    fprintf(out, "    ret\n  }\n}\n");
    return 0;
}
/*----------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------*/
int emit_net_program(const tree_t * ast_prog, FILE * out) {
    if (!ast_prog || !out) return 1;
    SM_Program * sm = sm_preamble(ast_prog);
    if (!sm) return 1;
    emit_net_prologue(NULL, out);
    emit_net_from_sm(sm, out);
    emit_net_epilogue(NULL, out);
    sm_prog_free(sm);
    return 0;
}
/*----------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------*/
IR_emit_vtable_t g_emit_vtable_net = { "net", emit_net_scalar, emit_net_generator, emit_net_prologue, emit_net_epilogue };
