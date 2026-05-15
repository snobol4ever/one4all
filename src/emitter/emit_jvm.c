#include <stdio.h>
#include <string.h>
#include "emit_ir.h"
/*----------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------*/
/* emit_jvm.c — JVM Jasmin emitter for IR_t generator (BB) nodes.
   Each emit_jvm_bb_* function emits the Jasmin class text for one IR_PAT_* node kind,
   parameterised by the node's payload (sval, ival, ival2).
   Reference: src/runtime/jvm/bb_boxes.j (one class per box kind).
   Scalar nodes are handled by emit_jvm_scalar (stub — SJ4-JVM-2 wires SnoRt.j calls).
   Generator nodes are handled by emit_jvm_generator (dispatches to the 19 functions below).
   Both are registered in g_emit_vtable_jvm at the bottom of this file. */
/*----------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------*/
/* Helpers — emit the standard class header and inner-class declarations used by every box. */
static void jvm_class_header(FILE * out, const char * name) {
    fprintf(out, ".class public bb/bb_%s\n", name);
    fprintf(out, ".super bb/bb_box\n");
    fprintf(out, ".inner class public static final spec inner bb/bb_box$Spec outer bb/bb_box\n");
    fprintf(out, ".inner class public static final matchstate inner bb/bb_box$MatchState outer bb/bb_box\n");
}
/*----------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------*/
static void jvm_init_ms_only(FILE * out, const char * name) {
    fprintf(out, ".method public <init>(Lbb/bb_box$MatchState;)V\n");
    fprintf(out, "    .limit stack 2\n    .limit locals 2\n");
    fprintf(out, "    aload_0\n    aload_1\n");
    fprintf(out, "    invokespecial bb/bb_box/<init>(Lbb/bb_box$MatchState;)V\n");
    fprintf(out, "    return\n.end method\n");
}
/*----------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------*/
static void jvm_init_ms_str(FILE * out, const char * name, const char * field) {
    fprintf(out, ".method public <init>(Lbb/bb_box$MatchState;Ljava/lang/String;)V\n");
    fprintf(out, "    .limit stack 3\n    .limit locals 3\n");
    fprintf(out, "    aload_0\n    aload_1\n");
    fprintf(out, "    invokespecial bb/bb_box/<init>(Lbb/bb_box$MatchState;)V\n");
    fprintf(out, "    aload_0\n    aload_2\n");
    fprintf(out, "    putfield bb/bb_%s/%s Ljava/lang/String;\n", name, field);
    fprintf(out, "    return\n.end method\n");
}
/*----------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------*/
static void jvm_init_ms_int(FILE * out, const char * name, const char * field) {
    fprintf(out, ".method public <init>(Lbb/bb_box$MatchState;I)V\n");
    fprintf(out, "    .limit stack 3\n    .limit locals 3\n");
    fprintf(out, "    aload_0\n    aload_1\n");
    fprintf(out, "    invokespecial bb/bb_box/<init>(Lbb/bb_box$MatchState;)V\n");
    fprintf(out, "    aload_0\n    iload_2\n");
    fprintf(out, "    putfield bb/bb_%s/%s I\n", name, field);
    fprintf(out, "    aload_0\n    aconst_null\n");
    fprintf(out, "    putfield bb/bb_%s/dyn Ljava/util/function/IntSupplier;\n", name);
    fprintf(out, "    return\n.end method\n");
}
/*----------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------*/
/* Emit the val() helper used by LEN, POS, TAB, RPOS, RTAB. */
static void jvm_val_helper(FILE * out, const char * name) {
    fprintf(out, ".method private val()I\n");
    fprintf(out, "    .limit stack 2\n    .limit locals 1\n");
    fprintf(out, "    aload_0\n");
    fprintf(out, "    getfield bb/bb_%s/dyn Ljava/util/function/IntSupplier;\n", name);
    fprintf(out, "    ifnull %s_val_static\n", name);
    fprintf(out, "    aload_0\n");
    fprintf(out, "    getfield bb/bb_%s/dyn Ljava/util/function/IntSupplier;\n", name);
    fprintf(out, "    invokeinterface java/util/function/IntSupplier/getAsInt()I 1\n");
    fprintf(out, "    ireturn\n");
    fprintf(out, "%s_val_static:\n", name);
    fprintf(out, "    aload_0\n    getfield bb/bb_%s/n I\n", name);
    fprintf(out, "    ireturn\n.end method\n");
}
/*----------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------*/
/* Escape a SNOBOL4 string literal for use as a Jasmin ldc string operand.
   Jasmin ldc strings use Java string literal syntax inside double-quotes. */
static void jvm_emit_ldc_string(FILE * out, const char * s) {
    fprintf(out, "    ldc \"");
    for (const char * p = s; * p; p++) {
        if (* p == '"')  { fprintf(out, "\\\""); }
        else if (* p == '\\') { fprintf(out, "\\\\"); }
        else if (* p == '\n') { fprintf(out, "\\n"); }
        else if (* p == '\r') { fprintf(out, "\\r"); }
        else if (* p == '\t') { fprintf(out, "\\t"); }
        else                  { fputc(* p, out); }
    }
    fprintf(out, "\"\n");
}
/*----------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------*/
/* Emit an integer push instruction choosing the most compact opcode. */
static void jvm_push_int(FILE * out, long v) {
    if (v >= -1 && v <= 5)        { fprintf(out, "    iconst_%ld\n", v == -1 ? (long)'m' : v); if (v == -1) fprintf(out, "    iconst_m1\n"); }
    else if (v >= -128 && v <= 127) { fprintf(out, "    bipush %ld\n", v); }
    else if (v >= -32768 && v <= 32767) { fprintf(out, "    sipush %ld\n", v); }
    else                            { fprintf(out, "    ldc %ld\n", v); }
}
/*----------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------*/
static void jvm_push_int2(FILE * out, long v) {
    if (v == -1) { fprintf(out, "    iconst_m1\n"); return; }
    if (v >= 0 && v <= 5) { fprintf(out, "    iconst_%ld\n", v); return; }
    if (v >= -128 && v <= 127) { fprintf(out, "    bipush %ld\n", v); return; }
    if (v >= -32768 && v <= 32767) { fprintf(out, "    sipush %ld\n", v); return; }
    fprintf(out, "    ldc %ld\n", v);
}
/*----------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------*/
/* ── IR_PAT_LIT ── */
static void emit_jvm_bb_lit(IR_t * nd, FILE * out, int sid, int nid) {
    char tag[32]; snprintf(tag, sizeof tag, "lit_%d_%d", sid, nid);
    jvm_class_header(out, "lit");
    fprintf(out, ".field private final lit Ljava/lang/String;\n");
    fprintf(out, ".field private final len I\n");
    fprintf(out, ".method public <init>(Lbb/bb_box$MatchState;Ljava/lang/String;)V\n");
    fprintf(out, "    .limit stack 3\n    .limit locals 3\n");
    fprintf(out, "    aload_0\n    aload_1\n    invokespecial bb/bb_box/<init>(Lbb/bb_box$MatchState;)V\n");
    fprintf(out, "    aload_0\n    aload_2\n    putfield bb/bb_lit/lit Ljava/lang/String;\n");
    fprintf(out, "    aload_0\n    aload_2\n    invokevirtual java/lang/String/length()I\n    putfield bb/bb_lit/len I\n");
    fprintf(out, "    return\n.end method\n");
    fprintf(out, ".method public \316\261()Lbb/bb_box$Spec;\n");
    fprintf(out, "    .limit stack 6\n    .limit locals 2\n");
    fprintf(out, "    aload_0\n    getfield bb/bb_lit/ms Lbb/bb_box$MatchState;\n    getfield bb/bb_box$MatchState/delta I\n");
    fprintf(out, "    aload_0\n    getfield bb/bb_lit/len I\n    iadd\n");
    fprintf(out, "    aload_0\n    getfield bb/bb_lit/ms Lbb/bb_box$MatchState;\n    getfield bb/bb_box$MatchState/omega I\n");
    fprintf(out, "    if_icmpgt %s_omega\n", tag);
    fprintf(out, "    aload_0\n    getfield bb/bb_lit/ms Lbb/bb_box$MatchState;\n    getfield bb/bb_box$MatchState/sigma Ljava/lang/String;\n");
    fprintf(out, "    aload_0\n    getfield bb/bb_lit/ms Lbb/bb_box$MatchState;\n    getfield bb/bb_box$MatchState/delta I\n");
    fprintf(out, "    aload_0\n    getfield bb/bb_lit/lit Ljava/lang/String;\n    iconst_0\n");
    fprintf(out, "    aload_0\n    getfield bb/bb_lit/len I\n");
    fprintf(out, "    invokevirtual java/lang/String/regionMatches(ILjava/lang/String;II)Z\n");
    fprintf(out, "    ifeq %s_omega\n", tag);
    fprintf(out, "    aload_0\n    getfield bb/bb_lit/ms Lbb/bb_box$MatchState;\n    getfield bb/bb_box$MatchState/delta I\n    istore_1\n");
    fprintf(out, "    aload_0\n    getfield bb/bb_lit/ms Lbb/bb_box$MatchState;\n    dup\n    getfield bb/bb_box$MatchState/delta I\n");
    fprintf(out, "    aload_0\n    getfield bb/bb_lit/len I\n    iadd\n    putfield bb/bb_box$MatchState/delta I\n");
    fprintf(out, "    new bb/bb_box$Spec\n    dup\n    iload_1\n    aload_0\n    getfield bb/bb_lit/len I\n");
    fprintf(out, "    invokespecial bb/bb_box$Spec/<init>(II)V\n    areturn\n");
    fprintf(out, "%s_omega:\n    aconst_null\n    areturn\n.end method\n", tag);
    fprintf(out, ".method public \316\262()Lbb/bb_box$Spec;\n");
    fprintf(out, "    .limit stack 4\n    .limit locals 1\n");
    fprintf(out, "    aload_0\n    getfield bb/bb_lit/ms Lbb/bb_box$MatchState;\n    dup\n    getfield bb/bb_box$MatchState/delta I\n");
    fprintf(out, "    aload_0\n    getfield bb/bb_lit/len I\n    isub\n    putfield bb/bb_box$MatchState/delta I\n");
    fprintf(out, "    aconst_null\n    areturn\n.end method\n");
}
/*----------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------*/
/* ── IR_PAT_ANY ── */
static void emit_jvm_bb_any(IR_t * nd, FILE * out, int sid, int nid) {
    char tag[32]; snprintf(tag, sizeof tag, "any_%d_%d", sid, nid);
    jvm_class_header(out, "any");
    fprintf(out, ".field private final chars Ljava/lang/String;\n");
    jvm_init_ms_str(out, "any", "chars");
    fprintf(out, ".method public \316\261()Lbb/bb_box$Spec;\n");
    fprintf(out, "    .limit stack 5\n    .limit locals 2\n");
    fprintf(out, "    aload_0\n    getfield bb/bb_any/ms Lbb/bb_box$MatchState;\n    getfield bb/bb_box$MatchState/delta I\n");
    fprintf(out, "    aload_0\n    getfield bb/bb_any/ms Lbb/bb_box$MatchState;\n    getfield bb/bb_box$MatchState/omega I\n");
    fprintf(out, "    if_icmpge %s_omega\n", tag);
    fprintf(out, "    aload_0\n    getfield bb/bb_any/chars Ljava/lang/String;\n");
    fprintf(out, "    aload_0\n    getfield bb/bb_any/ms Lbb/bb_box$MatchState;\n    getfield bb/bb_box$MatchState/sigma Ljava/lang/String;\n");
    fprintf(out, "    aload_0\n    getfield bb/bb_any/ms Lbb/bb_box$MatchState;\n    getfield bb/bb_box$MatchState/delta I\n");
    fprintf(out, "    invokevirtual java/lang/String/charAt(I)C\n");
    fprintf(out, "    invokevirtual java/lang/String/indexOf(I)I\n");
    fprintf(out, "    iflt %s_omega\n", tag);
    fprintf(out, "    aload_0\n    getfield bb/bb_any/ms Lbb/bb_box$MatchState;\n    getfield bb/bb_box$MatchState/delta I\n    istore_1\n");
    fprintf(out, "    aload_0\n    getfield bb/bb_any/ms Lbb/bb_box$MatchState;\n    dup\n    getfield bb/bb_box$MatchState/delta I\n    iconst_1\n    iadd\n    putfield bb/bb_box$MatchState/delta I\n");
    fprintf(out, "    new bb/bb_box$Spec\n    dup\n    iload_1\n    iconst_1\n    invokespecial bb/bb_box$Spec/<init>(II)V\n    areturn\n");
    fprintf(out, "%s_omega:\n    aconst_null\n    areturn\n.end method\n", tag);
    fprintf(out, ".method public \316\262()Lbb/bb_box$Spec;\n");
    fprintf(out, "    .limit stack 4\n    .limit locals 1\n");
    fprintf(out, "    aload_0\n    getfield bb/bb_any/ms Lbb/bb_box$MatchState;\n    dup\n    getfield bb/bb_box$MatchState/delta I\n    iconst_1\n    isub\n    putfield bb/bb_box$MatchState/delta I\n");
    fprintf(out, "    aconst_null\n    areturn\n.end method\n");
}
/*----------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------*/
/* ── IR_PAT_NOTANY ── */
static void emit_jvm_bb_notany(IR_t * nd, FILE * out, int sid, int nid) {
    char tag[32]; snprintf(tag, sizeof tag, "notany_%d_%d", sid, nid);
    jvm_class_header(out, "notany");
    fprintf(out, ".field private final chars Ljava/lang/String;\n");
    jvm_init_ms_str(out, "notany", "chars");
    fprintf(out, ".method public \316\261()Lbb/bb_box$Spec;\n");
    fprintf(out, "    .limit stack 5\n    .limit locals 2\n");
    fprintf(out, "    aload_0\n    getfield bb/bb_notany/ms Lbb/bb_box$MatchState;\n    getfield bb/bb_box$MatchState/delta I\n");
    fprintf(out, "    aload_0\n    getfield bb/bb_notany/ms Lbb/bb_box$MatchState;\n    getfield bb/bb_box$MatchState/omega I\n");
    fprintf(out, "    if_icmpge %s_omega\n", tag);
    fprintf(out, "    aload_0\n    getfield bb/bb_notany/chars Ljava/lang/String;\n");
    fprintf(out, "    aload_0\n    getfield bb/bb_notany/ms Lbb/bb_box$MatchState;\n    getfield bb/bb_box$MatchState/sigma Ljava/lang/String;\n");
    fprintf(out, "    aload_0\n    getfield bb/bb_notany/ms Lbb/bb_box$MatchState;\n    getfield bb/bb_box$MatchState/delta I\n");
    fprintf(out, "    invokevirtual java/lang/String/charAt(I)C\n");
    fprintf(out, "    invokevirtual java/lang/String/indexOf(I)I\n");
    fprintf(out, "    ifge %s_omega\n", tag);
    fprintf(out, "    aload_0\n    getfield bb/bb_notany/ms Lbb/bb_box$MatchState;\n    getfield bb/bb_box$MatchState/delta I\n    istore_1\n");
    fprintf(out, "    aload_0\n    getfield bb/bb_notany/ms Lbb/bb_box$MatchState;\n    dup\n    getfield bb/bb_box$MatchState/delta I\n    iconst_1\n    iadd\n    putfield bb/bb_box$MatchState/delta I\n");
    fprintf(out, "    new bb/bb_box$Spec\n    dup\n    iload_1\n    iconst_1\n    invokespecial bb/bb_box$Spec/<init>(II)V\n    areturn\n");
    fprintf(out, "%s_omega:\n    aconst_null\n    areturn\n.end method\n", tag);
    fprintf(out, ".method public \316\262()Lbb/bb_box$Spec;\n");
    fprintf(out, "    .limit stack 4\n    .limit locals 1\n");
    fprintf(out, "    aload_0\n    getfield bb/bb_notany/ms Lbb/bb_box$MatchState;\n    dup\n    getfield bb/bb_box$MatchState/delta I\n    iconst_1\n    isub\n    putfield bb/bb_box$MatchState/delta I\n");
    fprintf(out, "    aconst_null\n    areturn\n.end method\n");
}
/*----------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------*/
/* ── IR_PAT_SPAN ── */
static void emit_jvm_bb_span(IR_t * nd, FILE * out, int sid, int nid) {
    char tag[32]; snprintf(tag, sizeof tag, "span_%d_%d", sid, nid);
    jvm_class_header(out, "span");
    fprintf(out, ".field private final chars Ljava/lang/String;\n");
    fprintf(out, ".field private matched_len I\n");
    jvm_init_ms_str(out, "span", "chars");
    fprintf(out, ".method public \316\261()Lbb/bb_box$Spec;\n");
    fprintf(out, "    .limit stack 6\n    .limit locals 3\n");
    fprintf(out, "    aload_0\n    iconst_0\n    putfield bb/bb_span/matched_len I\n");
    fprintf(out, "%s_loop:\n", tag);
    fprintf(out, "    aload_0\n    getfield bb/bb_span/ms Lbb/bb_box$MatchState;\n    getfield bb/bb_box$MatchState/delta I\n");
    fprintf(out, "    aload_0\n    getfield bb/bb_span/matched_len I\n    iadd\n    istore_1\n");
    fprintf(out, "    iload_1\n    aload_0\n    getfield bb/bb_span/ms Lbb/bb_box$MatchState;\n    getfield bb/bb_box$MatchState/omega I\n    if_icmpge %s_done\n", tag);
    fprintf(out, "    aload_0\n    getfield bb/bb_span/chars Ljava/lang/String;\n");
    fprintf(out, "    aload_0\n    getfield bb/bb_span/ms Lbb/bb_box$MatchState;\n    getfield bb/bb_box$MatchState/sigma Ljava/lang/String;\n    iload_1\n    invokevirtual java/lang/String/charAt(I)C\n");
    fprintf(out, "    invokevirtual java/lang/String/indexOf(I)I\n    iflt %s_done\n", tag);
    fprintf(out, "    aload_0\n    dup\n    getfield bb/bb_span/matched_len I\n    iconst_1\n    iadd\n    putfield bb/bb_span/matched_len I\n");
    fprintf(out, "    goto %s_loop\n", tag);
    fprintf(out, "%s_done:\n    aload_0\n    getfield bb/bb_span/matched_len I\n    ifle %s_omega\n", tag, tag);
    fprintf(out, "    aload_0\n    getfield bb/bb_span/ms Lbb/bb_box$MatchState;\n    getfield bb/bb_box$MatchState/delta I\n    istore_2\n");
    fprintf(out, "    aload_0\n    getfield bb/bb_span/ms Lbb/bb_box$MatchState;\n    dup\n    getfield bb/bb_box$MatchState/delta I\n    aload_0\n    getfield bb/bb_span/matched_len I\n    iadd\n    putfield bb/bb_box$MatchState/delta I\n");
    fprintf(out, "    new bb/bb_box$Spec\n    dup\n    iload_2\n    aload_0\n    getfield bb/bb_span/matched_len I\n    invokespecial bb/bb_box$Spec/<init>(II)V\n    areturn\n");
    fprintf(out, "%s_omega:\n    aconst_null\n    areturn\n.end method\n", tag);
    fprintf(out, ".method public \316\262()Lbb/bb_box$Spec;\n");
    fprintf(out, "    .limit stack 4\n    .limit locals 1\n");
    fprintf(out, "    aload_0\n    getfield bb/bb_span/ms Lbb/bb_box$MatchState;\n    dup\n    getfield bb/bb_box$MatchState/delta I\n    aload_0\n    getfield bb/bb_span/matched_len I\n    isub\n    putfield bb/bb_box$MatchState/delta I\n");
    fprintf(out, "    aconst_null\n    areturn\n.end method\n");
}
/*----------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------*/
/* ── IR_PAT_BREAK ── */
static void emit_jvm_bb_break(IR_t * nd, FILE * out, int sid, int nid) {
    char tag[32]; snprintf(tag, sizeof tag, "brk_%d_%d", sid, nid);
    jvm_class_header(out, "brk");
    fprintf(out, ".field private final chars Ljava/lang/String;\n");
    fprintf(out, ".field private matched_len I\n");
    jvm_init_ms_str(out, "brk", "chars");
    fprintf(out, ".method public \316\261()Lbb/bb_box$Spec;\n");
    fprintf(out, "    .limit stack 6\n    .limit locals 3\n");
    fprintf(out, "    aload_0\n    iconst_0\n    putfield bb/bb_brk/matched_len I\n");
    fprintf(out, "%s_loop:\n", tag);
    fprintf(out, "    aload_0\n    getfield bb/bb_brk/ms Lbb/bb_box$MatchState;\n    getfield bb/bb_box$MatchState/delta I\n");
    fprintf(out, "    aload_0\n    getfield bb/bb_brk/matched_len I\n    iadd\n    istore_1\n");
    fprintf(out, "    iload_1\n    aload_0\n    getfield bb/bb_brk/ms Lbb/bb_box$MatchState;\n    getfield bb/bb_box$MatchState/omega I\n    if_icmpge %s_omega\n", tag);
    fprintf(out, "    aload_0\n    getfield bb/bb_brk/chars Ljava/lang/String;\n");
    fprintf(out, "    aload_0\n    getfield bb/bb_brk/ms Lbb/bb_box$MatchState;\n    getfield bb/bb_box$MatchState/sigma Ljava/lang/String;\n    iload_1\n    invokevirtual java/lang/String/charAt(I)C\n");
    fprintf(out, "    invokevirtual java/lang/String/indexOf(I)I\n    ifge %s_found\n", tag);
    fprintf(out, "    aload_0\n    dup\n    getfield bb/bb_brk/matched_len I\n    iconst_1\n    iadd\n    putfield bb/bb_brk/matched_len I\n");
    fprintf(out, "    goto %s_loop\n", tag);
    fprintf(out, "%s_found:\n", tag);
    fprintf(out, "    aload_0\n    getfield bb/bb_brk/ms Lbb/bb_box$MatchState;\n    getfield bb/bb_box$MatchState/delta I\n    istore_2\n");
    fprintf(out, "    aload_0\n    getfield bb/bb_brk/ms Lbb/bb_box$MatchState;\n    dup\n    getfield bb/bb_box$MatchState/delta I\n    aload_0\n    getfield bb/bb_brk/matched_len I\n    iadd\n    putfield bb/bb_box$MatchState/delta I\n");
    fprintf(out, "    new bb/bb_box$Spec\n    dup\n    iload_2\n    aload_0\n    getfield bb/bb_brk/matched_len I\n    invokespecial bb/bb_box$Spec/<init>(II)V\n    areturn\n");
    fprintf(out, "%s_omega:\n    aconst_null\n    areturn\n.end method\n", tag);
    fprintf(out, ".method public \316\262()Lbb/bb_box$Spec;\n");
    fprintf(out, "    .limit stack 4\n    .limit locals 1\n");
    fprintf(out, "    aload_0\n    getfield bb/bb_brk/ms Lbb/bb_box$MatchState;\n    dup\n    getfield bb/bb_box$MatchState/delta I\n    aload_0\n    getfield bb/bb_brk/matched_len I\n    isub\n    putfield bb/bb_box$MatchState/delta I\n");
    fprintf(out, "    aconst_null\n    areturn\n.end method\n");
}
/*----------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------*/
/* ── IR_PAT_ARB ── */
static void emit_jvm_bb_arb(IR_t * nd, FILE * out, int sid, int nid) {
    char tag[32]; snprintf(tag, sizeof tag, "arb_%d_%d", sid, nid);
    jvm_class_header(out, "arb");
    fprintf(out, ".field private arb_count I\n.field private arb_start I\n");
    jvm_init_ms_only(out, "arb");
    fprintf(out, ".method public \316\261()Lbb/bb_box$Spec;\n");
    fprintf(out, "    .limit stack 5\n    .limit locals 1\n");
    fprintf(out, "    aload_0\n    iconst_0\n    putfield bb/bb_arb/arb_count I\n");
    fprintf(out, "    aload_0\n    aload_0\n    getfield bb/bb_arb/ms Lbb/bb_box$MatchState;\n    getfield bb/bb_box$MatchState/delta I\n    putfield bb/bb_arb/arb_start I\n");
    fprintf(out, "    new bb/bb_box$Spec\n    dup\n    aload_0\n    getfield bb/bb_arb/ms Lbb/bb_box$MatchState;\n    getfield bb/bb_box$MatchState/delta I\n    iconst_0\n    invokespecial bb/bb_box$Spec/<init>(II)V\n    areturn\n.end method\n");
    fprintf(out, ".method public \316\262()Lbb/bb_box$Spec;\n");
    fprintf(out, "    .limit stack 5\n    .limit locals 1\n");
    fprintf(out, "    aload_0\n    dup\n    getfield bb/bb_arb/arb_count I\n    iconst_1\n    iadd\n    putfield bb/bb_arb/arb_count I\n");
    fprintf(out, "    aload_0\n    getfield bb/bb_arb/arb_start I\n    aload_0\n    getfield bb/bb_arb/arb_count I\n    iadd\n");
    fprintf(out, "    aload_0\n    getfield bb/bb_arb/ms Lbb/bb_box$MatchState;\n    getfield bb/bb_box$MatchState/omega I\n    if_icmpgt %s_omega\n", tag);
    fprintf(out, "    aload_0\n    getfield bb/bb_arb/ms Lbb/bb_box$MatchState;\n    aload_0\n    getfield bb/bb_arb/arb_start I\n    putfield bb/bb_box$MatchState/delta I\n");
    fprintf(out, "    new bb/bb_box$Spec\n    dup\n    aload_0\n    getfield bb/bb_arb/ms Lbb/bb_box$MatchState;\n    getfield bb/bb_box$MatchState/delta I\n    aload_0\n    getfield bb/bb_arb/arb_count I\n    invokespecial bb/bb_box$Spec/<init>(II)V\n");
    fprintf(out, "    aload_0\n    getfield bb/bb_arb/ms Lbb/bb_box$MatchState;\n    dup\n    getfield bb/bb_box$MatchState/delta I\n    aload_0\n    getfield bb/bb_arb/arb_count I\n    iadd\n    putfield bb/bb_box$MatchState/delta I\n");
    fprintf(out, "    areturn\n");
    fprintf(out, "%s_omega:\n    aconst_null\n    areturn\n.end method\n", tag);
}
/*----------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------*/
/* ── IR_PAT_ARBNO ── */
static void emit_jvm_bb_arbno(IR_t * nd, FILE * out, int sid, int nid) {
    char tag[32]; snprintf(tag, sizeof tag, "arbno_%d_%d", sid, nid);
    jvm_class_header(out, "arbno");
    fprintf(out, ".field private static final MAX_DEPTH I = 64\n");
    fprintf(out, ".field private final body Lbb/bb_box;\n");
    fprintf(out, ".field private final frame_start [I\n.field private final frame_match_st [I\n.field private final frame_match_ln [I\n");
    fprintf(out, ".field private depth I\n");
    fprintf(out, ".method public <init>(Lbb/bb_box$MatchState;Lbb/bb_box;)V\n");
    fprintf(out, "    .limit stack 4\n    .limit locals 3\n");
    fprintf(out, "    aload_0\n    aload_1\n    invokespecial bb/bb_box/<init>(Lbb/bb_box$MatchState;)V\n");
    fprintf(out, "    aload_0\n    aload_2\n    putfield bb/bb_arbno/body Lbb/bb_box;\n");
    fprintf(out, "    aload_0\n    bipush 64\n    newarray int\n    putfield bb/bb_arbno/frame_start [I\n");
    fprintf(out, "    aload_0\n    bipush 64\n    newarray int\n    putfield bb/bb_arbno/frame_match_st [I\n");
    fprintf(out, "    aload_0\n    bipush 64\n    newarray int\n    putfield bb/bb_arbno/frame_match_ln [I\n");
    fprintf(out, "    return\n.end method\n");
    fprintf(out, ".method public \316\261()Lbb/bb_box$Spec;\n");
    fprintf(out, "    .limit stack 5\n    .limit locals 1\n");
    fprintf(out, "    aload_0\n    iconst_0\n    putfield bb/bb_arbno/depth I\n");
    fprintf(out, "    aload_0\n    getfield bb/bb_arbno/frame_match_st [I\n    iconst_0\n    aload_0\n    getfield bb/bb_arbno/ms Lbb/bb_box$MatchState;\n    getfield bb/bb_box$MatchState/delta I\n    iastore\n");
    fprintf(out, "    aload_0\n    getfield bb/bb_arbno/frame_match_ln [I\n    iconst_0\n    iconst_0\n    iastore\n");
    fprintf(out, "    aload_0\n    getfield bb/bb_arbno/frame_start [I\n    iconst_0\n    aload_0\n    getfield bb/bb_arbno/ms Lbb/bb_box$MatchState;\n    getfield bb/bb_box$MatchState/delta I\n    iastore\n");
    fprintf(out, "    aload_0\n    invokevirtual bb/bb_arbno/tryBody()Lbb/bb_box$Spec;\n    areturn\n.end method\n");
    fprintf(out, ".method public \316\262()Lbb/bb_box$Spec;\n");
    fprintf(out, "    .limit stack 5\n    .limit locals 1\n");
    fprintf(out, "    aload_0\n    getfield bb/bb_arbno/depth I\n    ifle %s_beta_omega\n", tag);
    fprintf(out, "    aload_0\n    dup\n    getfield bb/bb_arbno/depth I\n    iconst_1\n    isub\n    putfield bb/bb_arbno/depth I\n");
    fprintf(out, "    aload_0\n    getfield bb/bb_arbno/ms Lbb/bb_box$MatchState;\n    aload_0\n    getfield bb/bb_arbno/frame_start [I\n    aload_0\n    getfield bb/bb_arbno/depth I\n    iaload\n    putfield bb/bb_box$MatchState/delta I\n");
    fprintf(out, "    new bb/bb_box$Spec\n    dup\n    aload_0\n    getfield bb/bb_arbno/frame_match_st [I\n    aload_0\n    getfield bb/bb_arbno/depth I\n    iaload\n    aload_0\n    getfield bb/bb_arbno/frame_match_ln [I\n    aload_0\n    getfield bb/bb_arbno/depth I\n    iaload\n    invokespecial bb/bb_box$Spec/<init>(II)V\n    areturn\n");
    fprintf(out, "%s_beta_omega:\n    aconst_null\n    areturn\n.end method\n", tag);
    fprintf(out, ".method private tryBody()Lbb/bb_box$Spec;\n");
    fprintf(out, "    .limit stack 6\n    .limit locals 4\n");
    fprintf(out, "%s_tryBody_loop:\n", tag);
    fprintf(out, "    aload_0\n    getfield bb/bb_arbno/body Lbb/bb_box;\n    invokevirtual bb/bb_box/\316\261()Lbb/bb_box$Spec;\n    astore_1\n    aload_1\n    ifnull %s_tryBody_omega\n", tag);
    fprintf(out, "    aload_0\n    getfield bb/bb_arbno/ms Lbb/bb_box$MatchState;\n    getfield bb/bb_box$MatchState/delta I\n    aload_0\n    getfield bb/bb_arbno/frame_start [I\n    aload_0\n    getfield bb/bb_arbno/depth I\n    iaload\n    if_icmpne %s_tryBody_advance\n", tag);
    fprintf(out, "    new bb/bb_box$Spec\n    dup\n    aload_0\n    getfield bb/bb_arbno/frame_match_st [I\n    aload_0\n    getfield bb/bb_arbno/depth I\n    iaload\n    aload_0\n    getfield bb/bb_arbno/frame_match_ln [I\n    aload_0\n    getfield bb/bb_arbno/depth I\n    iaload\n    invokespecial bb/bb_box$Spec/<init>(II)V\n    areturn\n");
    fprintf(out, "%s_tryBody_advance:\n", tag);
    fprintf(out, "    aload_0\n    getfield bb/bb_arbno/frame_match_st [I\n    aload_0\n    getfield bb/bb_arbno/depth I\n    iaload\n    istore_2\n");
    fprintf(out, "    aload_0\n    getfield bb/bb_arbno/frame_match_ln [I\n    aload_0\n    getfield bb/bb_arbno/depth I\n    iaload\n    aload_1\n    getfield bb/bb_box$Spec/len I\n    iadd\n");
    fprintf(out, "    aload_0\n    getfield bb/bb_arbno/depth I\n    iconst_1\n    iadd\n    bipush 64\n    if_icmpge %s_tryBody_full\n", tag);
    fprintf(out, "    aload_0\n    dup\n    getfield bb/bb_arbno/depth I\n    iconst_1\n    iadd\n    putfield bb/bb_arbno/depth I\n");
    fprintf(out, "    aload_0\n    getfield bb/bb_arbno/frame_match_st [I\n    aload_0\n    getfield bb/bb_arbno/depth I\n    iload_2\n    iastore\n");
    fprintf(out, "    istore 3\n    aload_0\n    getfield bb/bb_arbno/frame_match_ln [I\n    aload_0\n    getfield bb/bb_arbno/depth I\n    iload 3\n    iastore\n");
    fprintf(out, "    aload_0\n    getfield bb/bb_arbno/frame_start [I\n    aload_0\n    getfield bb/bb_arbno/depth I\n    aload_0\n    getfield bb/bb_arbno/ms Lbb/bb_box$MatchState;\n    getfield bb/bb_box$MatchState/delta I\n    iastore\n");
    fprintf(out, "    goto %s_tryBody_loop\n", tag);
    fprintf(out, "%s_tryBody_full:\n", tag);
    fprintf(out, "    new bb/bb_box$Spec\n    dup_x1\n    swap\n    iload_2\n    swap\n    invokespecial bb/bb_box$Spec/<init>(II)V\n    areturn\n");
    fprintf(out, "%s_tryBody_omega:\n", tag);
    fprintf(out, "    new bb/bb_box$Spec\n    dup\n    aload_0\n    getfield bb/bb_arbno/frame_match_st [I\n    aload_0\n    getfield bb/bb_arbno/depth I\n    iaload\n    aload_0\n    getfield bb/bb_arbno/frame_match_ln [I\n    aload_0\n    getfield bb/bb_arbno/depth I\n    iaload\n    invokespecial bb/bb_box$Spec/<init>(II)V\n    areturn\n.end method\n");
}
/*----------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------*/
/* ── IR_PAT_CAT (sequence) ── */
static void emit_jvm_bb_cat(IR_t * nd, FILE * out, int sid, int nid) {
    char tag[32]; snprintf(tag, sizeof tag, "seq_%d_%d", sid, nid);
    jvm_class_header(out, "seq");
    fprintf(out, ".field private final left Lbb/bb_box;\n.field private final right Lbb/bb_box;\n");
    fprintf(out, ".field private matched_start I\n.field private matched_len I\n");
    fprintf(out, ".method public <init>(Lbb/bb_box$MatchState;Lbb/bb_box;Lbb/bb_box;)V\n");
    fprintf(out, "    .limit stack 3\n    .limit locals 4\n");
    fprintf(out, "    aload_0\n    aload_1\n    invokespecial bb/bb_box/<init>(Lbb/bb_box$MatchState;)V\n");
    fprintf(out, "    aload_0\n    aload_2\n    putfield bb/bb_seq/left Lbb/bb_box;\n");
    fprintf(out, "    aload_0\n    aload_3\n    putfield bb/bb_seq/right Lbb/bb_box;\n");
    fprintf(out, "    return\n.end method\n");
    fprintf(out, ".method public \316\261()Lbb/bb_box$Spec;\n");
    fprintf(out, "    .limit stack 6\n    .limit locals 2\n");
    fprintf(out, "    aload_0\n    aload_0\n    getfield bb/bb_seq/ms Lbb/bb_box$MatchState;\n    getfield bb/bb_box$MatchState/delta I\n    putfield bb/bb_seq/matched_start I\n");
    fprintf(out, "    aload_0\n    iconst_0\n    putfield bb/bb_seq/matched_len I\n");
    fprintf(out, "    aload_0\n    getfield bb/bb_seq/left Lbb/bb_box;\n    invokevirtual bb/bb_box/\316\261()Lbb/bb_box$Spec;\n    astore_1\n    aload_1\n    ifnull %s_omega\n", tag);
    fprintf(out, "    aload_0\n    dup\n    getfield bb/bb_seq/matched_len I\n    aload_1\n    getfield bb/bb_box$Spec/len I\n    iadd\n    putfield bb/bb_seq/matched_len I\n");
    fprintf(out, "    aload_0\n    invokevirtual bb/bb_seq/rightAlpha()Lbb/bb_box$Spec;\n    areturn\n");
    fprintf(out, "%s_omega:\n    aconst_null\n    areturn\n.end method\n", tag);
    fprintf(out, ".method public \316\262()Lbb/bb_box$Spec;\n");
    fprintf(out, "    .limit stack 6\n    .limit locals 2\n");
    fprintf(out, "    aload_0\n    getfield bb/bb_seq/right Lbb/bb_box;\n    invokevirtual bb/bb_box/\316\262()Lbb/bb_box$Spec;\n    astore_1\n    aload_1\n    ifnull %s_beta_right_omega\n", tag);
    fprintf(out, "    new bb/bb_box$Spec\n    dup\n    aload_0\n    getfield bb/bb_seq/matched_start I\n    aload_0\n    getfield bb/bb_seq/matched_len I\n    aload_1\n    getfield bb/bb_box$Spec/len I\n    iadd\n    invokespecial bb/bb_box$Spec/<init>(II)V\n    areturn\n");
    fprintf(out, "%s_beta_right_omega:\n    aload_0\n    invokevirtual bb/bb_seq/leftBeta()Lbb/bb_box$Spec;\n    areturn\n.end method\n", tag);
    fprintf(out, ".method private rightAlpha()Lbb/bb_box$Spec;\n");
    fprintf(out, "    .limit stack 6\n    .limit locals 2\n");
    fprintf(out, "    aload_0\n    getfield bb/bb_seq/right Lbb/bb_box;\n    invokevirtual bb/bb_box/\316\261()Lbb/bb_box$Spec;\n    astore_1\n    aload_1\n    ifnull %s_rA_omega\n", tag);
    fprintf(out, "    new bb/bb_box$Spec\n    dup\n    aload_0\n    getfield bb/bb_seq/matched_start I\n    aload_0\n    getfield bb/bb_seq/matched_len I\n    aload_1\n    getfield bb/bb_box$Spec/len I\n    iadd\n    invokespecial bb/bb_box$Spec/<init>(II)V\n    areturn\n");
    fprintf(out, "%s_rA_omega:\n    aload_0\n    invokevirtual bb/bb_seq/leftBeta()Lbb/bb_box$Spec;\n    areturn\n.end method\n", tag);
    fprintf(out, ".method private leftBeta()Lbb/bb_box$Spec;\n");
    fprintf(out, "    .limit stack 6\n    .limit locals 2\n");
    fprintf(out, "    aload_0\n    getfield bb/bb_seq/left Lbb/bb_box;\n    invokevirtual bb/bb_box/\316\262()Lbb/bb_box$Spec;\n    astore_1\n    aload_1\n    ifnull %s_lB_omega\n", tag);
    fprintf(out, "    aload_0\n    aload_1\n    getfield bb/bb_box$Spec/len I\n    putfield bb/bb_seq/matched_len I\n");
    fprintf(out, "    aload_0\n    invokevirtual bb/bb_seq/rightAlpha()Lbb/bb_box$Spec;\n    areturn\n");
    fprintf(out, "%s_lB_omega:\n    aconst_null\n    areturn\n.end method\n", tag);
}
/*----------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------*/
/* ── IR_PAT_ALT ── */
static void emit_jvm_bb_alt(IR_t * nd, FILE * out, int sid, int nid) {
    char tag[32]; snprintf(tag, sizeof tag, "alt_%d_%d", sid, nid);
    jvm_class_header(out, "alt");
    fprintf(out, ".field private final children [Lbb/bb_box;\n.field private final n I\n.field private current I\n.field private position I\n");
    fprintf(out, ".method public <init>(Lbb/bb_box$MatchState;[Lbb/bb_box;)V\n");
    fprintf(out, "    .limit stack 3\n    .limit locals 3\n");
    fprintf(out, "    aload_0\n    aload_1\n    invokespecial bb/bb_box/<init>(Lbb/bb_box$MatchState;)V\n");
    fprintf(out, "    aload_0\n    aload_2\n    putfield bb/bb_alt/children [Lbb/bb_box;\n");
    fprintf(out, "    aload_0\n    aload_2\n    arraylength\n    putfield bb/bb_alt/n I\n    return\n.end method\n");
    fprintf(out, ".method public \316\261()Lbb/bb_box$Spec;\n");
    fprintf(out, "    .limit stack 3\n    .limit locals 1\n");
    fprintf(out, "    aload_0\n    aload_0\n    getfield bb/bb_alt/ms Lbb/bb_box$MatchState;\n    getfield bb/bb_box$MatchState/delta I\n    putfield bb/bb_alt/position I\n");
    fprintf(out, "    aload_0\n    iconst_1\n    putfield bb/bb_alt/current I\n");
    fprintf(out, "    aload_0\n    invokevirtual bb/bb_alt/tryAlpha()Lbb/bb_box$Spec;\n    areturn\n.end method\n");
    fprintf(out, ".method public \316\262()Lbb/bb_box$Spec;\n");
    fprintf(out, "    .limit stack 4\n    .limit locals 2\n");
    fprintf(out, "    aload_0\n    getfield bb/bb_alt/children [Lbb/bb_box;\n    aload_0\n    getfield bb/bb_alt/current I\n    iconst_1\n    isub\n    aaload\n");
    fprintf(out, "    invokevirtual bb/bb_box/\316\262()Lbb/bb_box$Spec;\n    astore_1\n    aload_1\n    ifnull %s_beta_omega\n", tag);
    fprintf(out, "    aload_1\n    areturn\n");
    fprintf(out, "%s_beta_omega:\n    aconst_null\n    areturn\n.end method\n", tag);
    fprintf(out, ".method private tryAlpha()Lbb/bb_box$Spec;\n");
    fprintf(out, "    .limit stack 4\n    .limit locals 2\n");
    fprintf(out, "%s_try_loop:\n", tag);
    fprintf(out, "    aload_0\n    getfield bb/bb_alt/current I\n    aload_0\n    getfield bb/bb_alt/n I\n    if_icmpgt %s_try_omega\n", tag);
    fprintf(out, "    aload_0\n    getfield bb/bb_alt/ms Lbb/bb_box$MatchState;\n    aload_0\n    getfield bb/bb_alt/position I\n    putfield bb/bb_box$MatchState/delta I\n");
    fprintf(out, "    aload_0\n    getfield bb/bb_alt/children [Lbb/bb_box;\n    aload_0\n    getfield bb/bb_alt/current I\n    iconst_1\n    isub\n    aaload\n");
    fprintf(out, "    invokevirtual bb/bb_box/\316\261()Lbb/bb_box$Spec;\n    astore_1\n    aload_1\n    ifnull %s_try_next\n    aload_1\n    areturn\n", tag);
    fprintf(out, "%s_try_next:\n    aload_0\n    dup\n    getfield bb/bb_alt/current I\n    iconst_1\n    iadd\n    putfield bb/bb_alt/current I\n    goto %s_try_loop\n", tag, tag);
    fprintf(out, "%s_try_omega:\n    aconst_null\n    areturn\n.end method\n", tag);
}
/*----------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------*/
/* ── IR_PAT_LEN ── */
static void emit_jvm_bb_len(IR_t * nd, FILE * out, int sid, int nid) {
    char tag[32]; snprintf(tag, sizeof tag, "len_%d_%d", sid, nid);
    jvm_class_header(out, "len");
    fprintf(out, ".field private final n I\n.field private final dyn Ljava/util/function/IntSupplier;\n");
    jvm_init_ms_int(out, "len", "n");
    jvm_val_helper(out, "len");
    fprintf(out, ".method public \316\261()Lbb/bb_box$Spec;\n");
    fprintf(out, "    .limit stack 5\n    .limit locals 2\n");
    fprintf(out, "    aload_0\n    getfield bb/bb_len/ms Lbb/bb_box$MatchState;\n    getfield bb/bb_box$MatchState/delta I\n    aload_0\n    invokevirtual bb/bb_len/val()I\n    iadd\n");
    fprintf(out, "    aload_0\n    getfield bb/bb_len/ms Lbb/bb_box$MatchState;\n    getfield bb/bb_box$MatchState/omega I\n    if_icmpgt %s_omega\n", tag);
    fprintf(out, "    aload_0\n    getfield bb/bb_len/ms Lbb/bb_box$MatchState;\n    getfield bb/bb_box$MatchState/delta I\n    istore_1\n");
    fprintf(out, "    aload_0\n    getfield bb/bb_len/ms Lbb/bb_box$MatchState;\n    dup\n    getfield bb/bb_box$MatchState/delta I\n    aload_0\n    invokevirtual bb/bb_len/val()I\n    iadd\n    putfield bb/bb_box$MatchState/delta I\n");
    fprintf(out, "    new bb/bb_box$Spec\n    dup\n    iload_1\n    aload_0\n    invokevirtual bb/bb_len/val()I\n    invokespecial bb/bb_box$Spec/<init>(II)V\n    areturn\n");
    fprintf(out, "%s_omega:\n    aconst_null\n    areturn\n.end method\n", tag);
    fprintf(out, ".method public \316\262()Lbb/bb_box$Spec;\n");
    fprintf(out, "    .limit stack 4\n    .limit locals 1\n");
    fprintf(out, "    aload_0\n    getfield bb/bb_len/ms Lbb/bb_box$MatchState;\n    dup\n    getfield bb/bb_box$MatchState/delta I\n    aload_0\n    invokevirtual bb/bb_len/val()I\n    isub\n    putfield bb/bb_box$MatchState/delta I\n");
    fprintf(out, "    aconst_null\n    areturn\n.end method\n");
}
/*----------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------*/
/* ── IR_PAT_POS (ival2=0) and IR_PAT_TAB (ival2=0) share the pos/tab structure. ── */
static void emit_jvm_bb_pos(IR_t * nd, FILE * out, int sid, int nid) {
    char tag[32]; snprintf(tag, sizeof tag, "pos_%d_%d", sid, nid);
    jvm_class_header(out, "pos");
    fprintf(out, ".field private final n I\n.field private final dyn Ljava/util/function/IntSupplier;\n");
    jvm_init_ms_int(out, "pos", "n");
    jvm_val_helper(out, "pos");
    fprintf(out, ".method public \316\261()Lbb/bb_box$Spec;\n");
    fprintf(out, "    .limit stack 5\n    .limit locals 1\n");
    fprintf(out, "    aload_0\n    getfield bb/bb_pos/ms Lbb/bb_box$MatchState;\n    getfield bb/bb_box$MatchState/delta I\n    aload_0\n    invokevirtual bb/bb_pos/val()I\n    if_icmpne %s_omega\n", tag);
    fprintf(out, "    new bb/bb_box$Spec\n    dup\n    aload_0\n    getfield bb/bb_pos/ms Lbb/bb_box$MatchState;\n    getfield bb/bb_box$MatchState/delta I\n    iconst_0\n    invokespecial bb/bb_box$Spec/<init>(II)V\n    areturn\n");
    fprintf(out, "%s_omega:\n    aconst_null\n    areturn\n.end method\n", tag);
    fprintf(out, ".method public \316\262()Lbb/bb_box$Spec;\n    .limit stack 1\n    .limit locals 1\n    aconst_null\n    areturn\n.end method\n");
}
/*----------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------*/
/* ── IR_PAT_POS (rpos=1) ── */
static void emit_jvm_bb_rpos(IR_t * nd, FILE * out, int sid, int nid) {
    char tag[32]; snprintf(tag, sizeof tag, "rpos_%d_%d", sid, nid);
    jvm_class_header(out, "rpos");
    fprintf(out, ".field private final n I\n.field private final dyn Ljava/util/function/IntSupplier;\n");
    jvm_init_ms_int(out, "rpos", "n");
    jvm_val_helper(out, "rpos");
    fprintf(out, ".method public \316\261()Lbb/bb_box$Spec;\n");
    fprintf(out, "    .limit stack 5\n    .limit locals 1\n");
    fprintf(out, "    aload_0\n    getfield bb/bb_rpos/ms Lbb/bb_box$MatchState;\n    getfield bb/bb_box$MatchState/delta I\n");
    fprintf(out, "    aload_0\n    getfield bb/bb_rpos/ms Lbb/bb_box$MatchState;\n    getfield bb/bb_box$MatchState/omega I\n    aload_0\n    invokevirtual bb/bb_rpos/val()I\n    isub\n");
    fprintf(out, "    if_icmpne %s_omega\n", tag);
    fprintf(out, "    new bb/bb_box$Spec\n    dup\n    aload_0\n    getfield bb/bb_rpos/ms Lbb/bb_box$MatchState;\n    getfield bb/bb_box$MatchState/delta I\n    iconst_0\n    invokespecial bb/bb_box$Spec/<init>(II)V\n    areturn\n");
    fprintf(out, "%s_omega:\n    aconst_null\n    areturn\n.end method\n", tag);
    fprintf(out, ".method public \316\262()Lbb/bb_box$Spec;\n    .limit stack 1\n    .limit locals 1\n    aconst_null\n    areturn\n.end method\n");
}
/*----------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------*/
/* ── IR_PAT_TAB (rtab=0) ── */
static void emit_jvm_bb_tab(IR_t * nd, FILE * out, int sid, int nid) {
    char tag[32]; snprintf(tag, sizeof tag, "tab_%d_%d", sid, nid);
    jvm_class_header(out, "tab");
    fprintf(out, ".field private final n I\n.field private final dyn Ljava/util/function/IntSupplier;\n.field private advance I\n");
    jvm_init_ms_int(out, "tab", "n");
    jvm_val_helper(out, "tab");
    fprintf(out, ".method public \316\261()Lbb/bb_box$Spec;\n");
    fprintf(out, "    .limit stack 5\n    .limit locals 3\n");
    fprintf(out, "    aload_0\n    getfield bb/bb_tab/ms Lbb/bb_box$MatchState;\n    getfield bb/bb_box$MatchState/delta I\n    aload_0\n    invokevirtual bb/bb_tab/val()I\n    if_icmpgt %s_omega\n", tag);
    fprintf(out, "    aload_0\n    invokevirtual bb/bb_tab/val()I\n    aload_0\n    getfield bb/bb_tab/ms Lbb/bb_box$MatchState;\n    getfield bb/bb_box$MatchState/delta I\n    isub\n    istore_1\n");
    fprintf(out, "    aload_0\n    iload_1\n    putfield bb/bb_tab/advance I\n");
    fprintf(out, "    aload_0\n    getfield bb/bb_tab/ms Lbb/bb_box$MatchState;\n    getfield bb/bb_box$MatchState/delta I\n    istore_2\n");
    fprintf(out, "    aload_0\n    getfield bb/bb_tab/ms Lbb/bb_box$MatchState;\n    aload_0\n    invokevirtual bb/bb_tab/val()I\n    putfield bb/bb_box$MatchState/delta I\n");
    fprintf(out, "    new bb/bb_box$Spec\n    dup\n    iload_2\n    iload_1\n    invokespecial bb/bb_box$Spec/<init>(II)V\n    areturn\n");
    fprintf(out, "%s_omega:\n    aconst_null\n    areturn\n.end method\n", tag);
    fprintf(out, ".method public \316\262()Lbb/bb_box$Spec;\n");
    fprintf(out, "    .limit stack 4\n    .limit locals 1\n");
    fprintf(out, "    aload_0\n    getfield bb/bb_tab/ms Lbb/bb_box$MatchState;\n    dup\n    getfield bb/bb_box$MatchState/delta I\n    aload_0\n    getfield bb/bb_tab/advance I\n    isub\n    putfield bb/bb_box$MatchState/delta I\n");
    fprintf(out, "    aconst_null\n    areturn\n.end method\n");
}
/*----------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------*/
/* ── IR_PAT_TAB (rtab=1) ── */
static void emit_jvm_bb_rtab(IR_t * nd, FILE * out, int sid, int nid) {
    char tag[32]; snprintf(tag, sizeof tag, "rtab_%d_%d", sid, nid);
    jvm_class_header(out, "rtab");
    fprintf(out, ".field private final n I\n.field private final dyn Ljava/util/function/IntSupplier;\n.field private advance I\n");
    jvm_init_ms_int(out, "rtab", "n");
    jvm_val_helper(out, "rtab");
    fprintf(out, ".method public \316\261()Lbb/bb_box$Spec;\n");
    fprintf(out, "    .limit stack 5\n    .limit locals 4\n");
    fprintf(out, "    aload_0\n    getfield bb/bb_rtab/ms Lbb/bb_box$MatchState;\n    getfield bb/bb_box$MatchState/omega I\n    aload_0\n    invokevirtual bb/bb_rtab/val()I\n    isub\n    istore_1\n");
    fprintf(out, "    aload_0\n    getfield bb/bb_rtab/ms Lbb/bb_box$MatchState;\n    getfield bb/bb_box$MatchState/delta I\n    iload_1\n    if_icmpgt %s_omega\n", tag);
    fprintf(out, "    iload_1\n    aload_0\n    getfield bb/bb_rtab/ms Lbb/bb_box$MatchState;\n    getfield bb/bb_box$MatchState/delta I\n    isub\n    istore_2\n");
    fprintf(out, "    aload_0\n    iload_2\n    putfield bb/bb_rtab/advance I\n");
    fprintf(out, "    aload_0\n    getfield bb/bb_rtab/ms Lbb/bb_box$MatchState;\n    getfield bb/bb_box$MatchState/delta I\n    istore_3\n");
    fprintf(out, "    aload_0\n    getfield bb/bb_rtab/ms Lbb/bb_box$MatchState;\n    iload_1\n    putfield bb/bb_box$MatchState/delta I\n");
    fprintf(out, "    new bb/bb_box$Spec\n    dup\n    iload_3\n    iload_2\n    invokespecial bb/bb_box$Spec/<init>(II)V\n    areturn\n");
    fprintf(out, "%s_omega:\n    aconst_null\n    areturn\n.end method\n", tag);
    fprintf(out, ".method public \316\262()Lbb/bb_box$Spec;\n");
    fprintf(out, "    .limit stack 4\n    .limit locals 1\n");
    fprintf(out, "    aload_0\n    getfield bb/bb_rtab/ms Lbb/bb_box$MatchState;\n    dup\n    getfield bb/bb_box$MatchState/delta I\n    aload_0\n    getfield bb/bb_rtab/advance I\n    isub\n    putfield bb/bb_box$MatchState/delta I\n");
    fprintf(out, "    aconst_null\n    areturn\n.end method\n");
}
/*----------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------*/
/* ── IR_PAT_REM ── */
static void emit_jvm_bb_rem(IR_t * nd, FILE * out, int sid, int nid) {
    jvm_class_header(out, "rem");
    jvm_init_ms_only(out, "rem");
    fprintf(out, ".method public \316\261()Lbb/bb_box$Spec;\n");
    fprintf(out, "    .limit stack 6\n    .limit locals 2\n");
    fprintf(out, "    aload_0\n    getfield bb/bb_rem/ms Lbb/bb_box$MatchState;\n    getfield bb/bb_box$MatchState/delta I\n    istore_1\n");
    fprintf(out, "    aload_0\n    getfield bb/bb_rem/ms Lbb/bb_box$MatchState;\n    dup\n    getfield bb/bb_box$MatchState/omega I\n    putfield bb/bb_box$MatchState/delta I\n");
    fprintf(out, "    new bb/bb_box$Spec\n    dup\n    iload_1\n    aload_0\n    getfield bb/bb_rem/ms Lbb/bb_box$MatchState;\n    getfield bb/bb_box$MatchState/delta I\n    iload_1\n    isub\n    invokespecial bb/bb_box$Spec/<init>(II)V\n    areturn\n.end method\n");
    fprintf(out, ".method public \316\262()Lbb/bb_box$Spec;\n    .limit stack 1\n    .limit locals 1\n    aconst_null\n    areturn\n.end method\n");
}
/*----------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------*/
/* ── IR_PAT_FENCE ── */
static void emit_jvm_bb_fence(IR_t * nd, FILE * out, int sid, int nid) {
    jvm_class_header(out, "fence");
    jvm_init_ms_only(out, "fence");
    fprintf(out, ".method public \316\261()Lbb/bb_box$Spec;\n");
    fprintf(out, "    .limit stack 5\n    .limit locals 1\n");
    fprintf(out, "    new bb/bb_box$Spec\n    dup\n    aload_0\n    getfield bb/bb_fence/ms Lbb/bb_box$MatchState;\n    getfield bb/bb_box$MatchState/delta I\n    iconst_0\n    invokespecial bb/bb_box$Spec/<init>(II)V\n    areturn\n.end method\n");
    fprintf(out, ".method public \316\262()Lbb/bb_box$Spec;\n    .limit stack 1\n    .limit locals 1\n    aconst_null\n    areturn\n.end method\n");
}
/*----------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------*/
/* ── IR_PAT_ABORT ── */
static void emit_jvm_bb_abort(IR_t * nd, FILE * out, int sid, int nid) {
    jvm_class_header(out, "abort");
    fprintf(out, ".inner class public static final abort_exception inner bb/bb_abort$AbortException outer bb/bb_abort\n");
    jvm_init_ms_only(out, "abort");
    fprintf(out, ".method public \316\261()Lbb/bb_box$Spec;\n");
    fprintf(out, "    .limit stack 2\n    .limit locals 1\n");
    fprintf(out, "    new bb/bb_abort$AbortException\n    dup\n    invokespecial bb/bb_abort$AbortException/<init>()V\n    athrow\n.end method\n");
    fprintf(out, ".method public \316\262()Lbb/bb_box$Spec;\n");
    fprintf(out, "    .limit stack 2\n    .limit locals 1\n");
    fprintf(out, "    new bb/bb_abort$AbortException\n    dup\n    invokespecial bb/bb_abort$AbortException/<init>()V\n    athrow\n.end method\n");
}
/*----------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------*/
/* ── IR_PAT_ASSIGN_IMM and IR_PAT_ASSIGN_COND — both use bb_capture; immediate flag differs. ── */
static void emit_jvm_bb_capture(IR_t * nd, FILE * out, int sid, int nid, int immediate) {
    char tag[32]; snprintf(tag, sizeof tag, "cap_%d_%d", sid, nid);
    jvm_class_header(out, "capture");
    fprintf(out, ".inner interface public static abstract var_setter inner bb/bb_capture$VarSetter outer bb/bb_capture\n");
    fprintf(out, ".field private final child Lbb/bb_box;\n.field private final varname Ljava/lang/String;\n");
    fprintf(out, ".field private final immediate Z\n.field private final setter Lbb/bb_capture$VarSetter;\n");
    fprintf(out, ".field private pending_start I\n.field private pending_len I\n.field private has_pending Z\n");
    fprintf(out, ".method public <init>(Lbb/bb_box$MatchState;Lbb/bb_box;Ljava/lang/String;ZLbb/bb_capture$VarSetter;)V\n");
    fprintf(out, "    .limit stack 3\n    .limit locals 6\n");
    fprintf(out, "    aload_0\n    aload_1\n    invokespecial bb/bb_box/<init>(Lbb/bb_box$MatchState;)V\n");
    fprintf(out, "    aload_0\n    aload_2\n    putfield bb/bb_capture/child Lbb/bb_box;\n");
    fprintf(out, "    aload_0\n    aload_3\n    putfield bb/bb_capture/varname Ljava/lang/String;\n");
    fprintf(out, "    aload_0\n    iload 4\n    putfield bb/bb_capture/immediate Z\n");
    fprintf(out, "    aload_0\n    aload 5\n    putfield bb/bb_capture/setter Lbb/bb_capture$VarSetter;\n");
    fprintf(out, "    return\n.end method\n");
    fprintf(out, ".method public \316\261()Lbb/bb_box$Spec;\n");
    fprintf(out, "    .limit stack 3\n    .limit locals 2\n");
    fprintf(out, "    aload_0\n    getfield bb/bb_capture/child Lbb/bb_box;\n    invokevirtual bb/bb_box/\316\261()Lbb/bb_box$Spec;\n    astore_1\n");
    fprintf(out, "    aload_0\n    aload_1\n    invokevirtual bb/bb_capture/runChild(Lbb/bb_box$Spec;)Lbb/bb_box$Spec;\n    areturn\n.end method\n");
    fprintf(out, ".method public \316\262()Lbb/bb_box$Spec;\n");
    fprintf(out, "    .limit stack 3\n    .limit locals 2\n");
    fprintf(out, "    aload_0\n    getfield bb/bb_capture/child Lbb/bb_box;\n    invokevirtual bb/bb_box/\316\262()Lbb/bb_box$Spec;\n    astore_1\n");
    fprintf(out, "    aload_0\n    aload_1\n    invokevirtual bb/bb_capture/runChild(Lbb/bb_box$Spec;)Lbb/bb_box$Spec;\n    areturn\n.end method\n");
    fprintf(out, ".method private runChild(Lbb/bb_box$Spec;)Lbb/bb_box$Spec;\n");
    fprintf(out, "    .limit stack 5\n    .limit locals 3\n");
    fprintf(out, "    aload_1\n    ifnonnull %s_got_match\n", tag);
    fprintf(out, "    aload_0\n    iconst_0\n    putfield bb/bb_capture/has_pending Z\n    aconst_null\n    areturn\n");
    fprintf(out, "%s_got_match:\n", tag);
    fprintf(out, "    aload_0\n    getfield bb/bb_capture/varname Ljava/lang/String;\n    ifnull %s_skip\n", tag);
    fprintf(out, "    aload_0\n    getfield bb/bb_capture/varname Ljava/lang/String;\n    invokevirtual java/lang/String/isEmpty()Z\n    ifne %s_skip\n", tag);
    fprintf(out, "    aload_0\n    getfield bb/bb_capture/ms Lbb/bb_box$MatchState;\n    getfield bb/bb_box$MatchState/sigma Ljava/lang/String;\n");
    fprintf(out, "    aload_1\n    getfield bb/bb_box$Spec/start I\n    aload_1\n    getfield bb/bb_box$Spec/start I\n    aload_1\n    getfield bb/bb_box$Spec/len I\n    iadd\n");
    fprintf(out, "    invokevirtual java/lang/String/substring(II)Ljava/lang/String;\n    astore_2\n");
    fprintf(out, "    aload_0\n    getfield bb/bb_capture/immediate Z\n    ifeq %s_deferred\n", tag);
    fprintf(out, "    aload_0\n    getfield bb/bb_capture/setter Lbb/bb_capture$VarSetter;\n    aload_0\n    getfield bb/bb_capture/varname Ljava/lang/String;\n    aload_2\n");
    fprintf(out, "    invokeinterface bb/bb_capture$VarSetter/set(Ljava/lang/String;Ljava/lang/String;)V 3\n    goto %s_skip\n", tag);
    fprintf(out, "%s_deferred:\n", tag);
    fprintf(out, "    aload_0\n    aload_1\n    getfield bb/bb_box$Spec/start I\n    putfield bb/bb_capture/pending_start I\n");
    fprintf(out, "    aload_0\n    aload_1\n    getfield bb/bb_box$Spec/len I\n    putfield bb/bb_capture/pending_len I\n");
    fprintf(out, "    aload_0\n    iconst_1\n    putfield bb/bb_capture/has_pending Z\n");
    fprintf(out, "%s_skip:\n    aload_1\n    areturn\n.end method\n", tag);
    fprintf(out, ".method public commitPending()V\n");
    fprintf(out, "    .limit stack 8\n    .limit locals 2\n");
    fprintf(out, "    aload_0\n    getfield bb/bb_capture/has_pending Z\n    ifeq %s_commit_done\n", tag);
    fprintf(out, "    aload_0\n    getfield bb/bb_capture/varname Ljava/lang/String;\n    ifnull %s_commit_done\n", tag);
    fprintf(out, "    aload_0\n    getfield bb/bb_capture/varname Ljava/lang/String;\n    invokevirtual java/lang/String/isEmpty()Z\n    ifne %s_commit_done\n", tag);
    fprintf(out, "    aload_0\n    getfield bb/bb_capture/setter Lbb/bb_capture$VarSetter;\n    aload_0\n    getfield bb/bb_capture/varname Ljava/lang/String;\n");
    fprintf(out, "    aload_0\n    getfield bb/bb_capture/ms Lbb/bb_box$MatchState;\n    getfield bb/bb_box$MatchState/sigma Ljava/lang/String;\n");
    fprintf(out, "    aload_0\n    getfield bb/bb_capture/pending_start I\n    aload_0\n    getfield bb/bb_capture/pending_start I\n    aload_0\n    getfield bb/bb_capture/pending_len I\n    iadd\n");
    fprintf(out, "    invokevirtual java/lang/String/substring(II)Ljava/lang/String;\n");
    fprintf(out, "    invokeinterface bb/bb_capture$VarSetter/set(Ljava/lang/String;Ljava/lang/String;)V 3\n");
    fprintf(out, "    aload_0\n    iconst_0\n    putfield bb/bb_capture/has_pending Z\n");
    fprintf(out, "%s_commit_done:\n    return\n.end method\n", tag);
}
/*----------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------*/
/* ── Top-level dispatch ── */
static int emit_jvm_scalar(IR_t * nd, FILE * out) {
    fprintf(out, "; [jvm scalar kind=%d stub — SJ4-JVM-2]\n", (int)nd->t);
    return 0;
}
/*----------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------*/
static int emit_jvm_generator(IR_t * nd, FILE * out) {
    int sid = 0, nid = ir_node_id(nd);
    switch (nd->t) {
    case IR_PAT_LIT:         emit_jvm_bb_lit(nd, out, sid, nid);              break;
    case IR_PAT_ANY:         emit_jvm_bb_any(nd, out, sid, nid);              break;
    case IR_PAT_SPAN:        emit_jvm_bb_span(nd, out, sid, nid);             break;
    case IR_PAT_BREAK:       emit_jvm_bb_break(nd, out, sid, nid);            break;
    case IR_PAT_ARB:         emit_jvm_bb_arb(nd, out, sid, nid);              break;
    case IR_PAT_ARBNO:       emit_jvm_bb_arbno(nd, out, sid, nid);            break;
    case IR_PAT_CAT:         emit_jvm_bb_cat(nd, out, sid, nid);              break;
    case IR_PAT_ALT:         emit_jvm_bb_alt(nd, out, sid, nid);              break;
    case IR_PAT_LEN:         emit_jvm_bb_len(nd, out, sid, nid);              break;
    case IR_PAT_NOTANY:      emit_jvm_bb_notany(nd, out, sid, nid);           break;
    case IR_PAT_POS:         if (nd->ival2) emit_jvm_bb_rpos(nd, out, sid, nid); else emit_jvm_bb_pos(nd, out, sid, nid); break;
    case IR_PAT_TAB:         if (nd->ival2) emit_jvm_bb_rtab(nd, out, sid, nid); else emit_jvm_bb_tab(nd, out, sid, nid); break;
    case IR_PAT_REM:         emit_jvm_bb_rem(nd, out, sid, nid);              break;
    case IR_PAT_FENCE:       emit_jvm_bb_fence(nd, out, sid, nid);            break;
    case IR_PAT_ABORT:       emit_jvm_bb_abort(nd, out, sid, nid);            break;
    case IR_PAT_ASSIGN_IMM:  emit_jvm_bb_capture(nd, out, sid, nid, 1);       break;
    case IR_PAT_ASSIGN_COND: emit_jvm_bb_capture(nd, out, sid, nid, 0);       break;
    default:
        fprintf(out, "; [jvm generator kind=%d unimplemented]\n", (int)nd->t);
        break;
    }
    return 0;
}
/*----------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------*/
static int emit_jvm_prologue(IR_block_t * cfg, FILE * out) {
    fprintf(out, "; JVM Jasmin output — generated by scrip --sm-emit --target=jvm\n");
    return 0;
}
/*----------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------*/
static int emit_jvm_epilogue(IR_block_t * cfg, FILE * out) { return 0; }
/*----------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------*/
IR_emit_vtable_t g_emit_vtable_jvm = { "jvm", emit_jvm_scalar, emit_jvm_generator, emit_jvm_prologue, emit_jvm_epilogue };
