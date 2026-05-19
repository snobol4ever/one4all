#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "emit_ir.h"
#include "emit_core.h"
#include "sm_prog.h"
/*----------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------*/
/* emit_jvm.c — JVM Jasmin emitter for IR_t generator (BB) nodes.
   Each emit_jvm_bb_* function emits the Jasmin class text for one IR_PAT_* node kind,
   parameterised by the node's payload (sval, ival, ival2).
   Reference: src/runtime/jvm/bb_boxes.j (one class per box kind).
   Scalar nodes are handled by emit_jvm_scalar (via SM_Program walk).
   Generator nodes are handled by emit_jvm_generator (dispatches to the 19 functions below).
   Both are registered in g_emit_vtable_jvm at the bottom of this file. */
/*----------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------*/
/* Forward declarations. */
extern SM_Program * sm_preamble(const tree_t * ast_prog);
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
/* Emit an integer push instruction choosing the most compact opcode. */
static void jvm_push_int(FILE * out, long v) {
    if (v >= -1 && v <= 5)        { fprintf(out, "    iconst_%ld\n", v == -1 ? (long)'m' : v); if (v == -1) fprintf(out, "    iconst_m1\n"); }
    else if (v >= -128 && v <= 127) { fprintf(out, "    bipush %ld\n", v); }
    else if (v >= -32768 && v <= 32767) { fprintf(out, "    sipush %ld\n", v); }
    else                            { fprintf(out, "    ldc %ld\n", v); }
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
/* Sanitize a SNOBOL4 label/function name to a valid JVM method name. */
static void jvm_sanitize_name(char * dst, size_t dsz, const char * src) {
    size_t j = 0;
    for (size_t i = 0; src[i] && j + 1 < dsz; i++) {
        char c = src[i];
        if ((c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') || (c >= '0' && c <= '9') || c == '_') dst[j++] = c;
        else dst[j++] = '_';
    }
    if (j == 0 && j + 4 < dsz) { dst[j++] = 's'; dst[j++] = 'n'; dst[j++] = 'o'; }
    dst[j] = '\0';
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
/* Emit the SM instructions for a range [lo, hi) into the current open method.
   fn_names/fn_pcs/fn_count: the table of all user-function entry points (for call resolution).
   n: total SM instruction count (for bounds checking).
   in_body: 1 if emitting body code (RETURN/HALT go to sm_pc_body_end), 0 if function body.
   in_my_method: byte array of size n; in_my_method[t]=1 if PC t is defined in this method. */
static void emit_jvm_one_instr(SM_Program * sm, int i, int n,
                                const char ** fn_names, const int * fn_pcs, int fn_count,
                                int in_body, const char * in_my_method, FILE * out) {
    SM_Instr * instr = &sm->instrs[i];
    switch (instr->op) {
    case SM_LABEL: break;
    case SM_STNO:  sm_stno(instr, out); break;
    case SM_PUSH_LIT_I: sm_push_lit_i(instr, out); break;
    case SM_PUSH_LIT_S: sm_push_lit_s(instr, out); break;
    case SM_PUSH_LIT_F: sm_push_lit_f(instr, out); break;
    case SM_PUSH_NULL: case SM_PUSH_NULL_NOFLIP: sm_push_null(instr, out); break;
    case SM_PUSH_VAR:  sm_push_var(instr, out); break;
    case SM_STORE_VAR: sm_store_var(instr, out); break;
    case SM_VOID_POP: sm_void_pop(instr, out); break;
    case SM_CONCAT:   sm_concat(instr, out); break;
    case SM_NEG:      sm_neg(instr, out); break;
    case SM_COERCE_NUM: sm_coerce_num(instr, out); break;
    case SM_EXP:      sm_exp(instr, out); break;
    case SM_ADD:      sm_add(instr, out); break;
    case SM_SUB:      sm_sub(instr, out); break;
    case SM_MUL:      sm_mul(instr, out); break;
    case SM_DIV:      sm_div(instr, out); break;
    case SM_MOD:      sm_mod(instr, out); break;
    case SM_ACOMP: sm_acomp(instr, out); break;
    case SM_LCOMP: sm_lcomp(instr, out); break;
    case SM_JUMP: {
        int target = (int)instr->a[0].i;
        const char * end_lbl = in_body ? "sm_pc_body_end" : "sm_pc_fn_end";
        if (target >= 0 && target < n && in_my_method[target]) fprintf(out, "    goto_w sm_pc_%d\n", target);
        else if (target >= 0 && target < n) {
            fprintf(out, "    invokestatic rt/SnoRt/halt_tos()V\n");
            fprintf(out, "    iconst_0\n    invokestatic java/lang/System/exit(I)V\n");
            fprintf(out, "    return\n");
        }
        else fprintf(out, "    goto_w %s\n", end_lbl);
        break;
    }
    case SM_JUMP_S: {
        int target = (int)instr->a[0].i;
        if (target >= 0 && target < n && in_my_method[target]) {
            fprintf(out, "    invokestatic rt/SnoRt/last_ok()Z\n");
            fprintf(out, "    ifeq sm_pc_%d_skip\n", i);
            fprintf(out, "    goto_w sm_pc_%d\n", target);
            fprintf(out, "sm_pc_%d_skip:\n", i);
        } else if (target >= 0 && target < n) {
            fprintf(out, "    invokestatic rt/SnoRt/last_ok()Z\n");
            fprintf(out, "    ifeq sm_pc_%d_skip\n", i);
            fprintf(out, "    invokestatic rt/SnoRt/halt_tos()V\n");
            fprintf(out, "    iconst_0\n    invokestatic java/lang/System/exit(I)V\n");
            fprintf(out, "    return\n");
            fprintf(out, "sm_pc_%d_skip:\n", i);
        }
        break;
    }
    case SM_JUMP_F: {
        int target = (int)instr->a[0].i;
        if (target >= 0 && target < n && in_my_method[target]) {
            fprintf(out, "    invokestatic rt/SnoRt/last_ok()Z\n");
            fprintf(out, "    ifne sm_pc_%d_skip\n", i);
            fprintf(out, "    goto_w sm_pc_%d\n", target);
            fprintf(out, "sm_pc_%d_skip:\n", i);
        } else if (target >= 0 && target < n) {
            fprintf(out, "    invokestatic rt/SnoRt/last_ok()Z\n");
            fprintf(out, "    ifne sm_pc_%d_skip\n", i);
            fprintf(out, "    invokestatic rt/SnoRt/halt_tos()V\n");
            fprintf(out, "    iconst_0\n    invokestatic java/lang/System/exit(I)V\n");
            fprintf(out, "    return\n");
            fprintf(out, "sm_pc_%d_skip:\n", i);
        }
        break;
    }
    case SM_CALL_FN: case SM_SUSPEND_VALUE: {
        const char * cname = instr->a[0].s ? instr->a[0].s : "";
        if (!cname[0]) {
            jvm_push_int2(out, 0); jvm_push_int2(out, 1);
            fprintf(out, "    invokestatic rt/SnoRt/do_return(II)I\n    pop\n");
            fprintf(out, "    invokestatic rt/SnoRt/fn_return_push()V\n");
            fprintf(out, "    return\n"); break;
        }
        int entry_pc = -1;
        for (int k = 0; k < fn_count; k++) {
            if (fn_names[k] && strcmp(fn_names[k], cname) == 0) { entry_pc = fn_pcs[k]; break; }
        }
        if (entry_pc >= 0) {
            char mname[256]; jvm_sanitize_name(mname, sizeof mname, cname);
            jvm_emit_ldc_string(out, cname);
            jvm_push_int2(out, (long)instr->a[1].i);
            fprintf(out, "    invokestatic rt/SnoRt/bind_params(Ljava/lang/String;I)V\n");
            fprintf(out, "    invokestatic Prog/sno_fn_%s()V\n", mname);
        } else {
            jvm_emit_ldc_string(out, cname);
            jvm_push_int2(out, (long)instr->a[1].i);
            fprintf(out, "    invokestatic rt/SnoRt/call(Ljava/lang/String;I)V\n");
        }
        break;
    }
    case SM_RETURN:
        jvm_push_int2(out, 0); jvm_push_int2(out, 1);
        fprintf(out, "    invokestatic rt/SnoRt/do_return(II)I\n    pop\n");
        fprintf(out, "    invokestatic rt/SnoRt/fn_return_push()V\n");
        fprintf(out, "    return\n"); break;
    case SM_RETURN_S:
        fprintf(out, "    invokestatic rt/SnoRt/last_ok()Z\n    ifeq sm_pc_%d_rs_skip\n", i);
        jvm_push_int2(out, 0); jvm_push_int2(out, 1);
        fprintf(out, "    invokestatic rt/SnoRt/do_return(II)I\n    pop\n");
        fprintf(out, "    invokestatic rt/SnoRt/fn_return_push()V\n");
        fprintf(out, "    return\nsm_pc_%d_rs_skip:\n", i); break;
    case SM_RETURN_F:
        fprintf(out, "    invokestatic rt/SnoRt/last_ok()Z\n    ifne sm_pc_%d_rf_skip\n", i);
        jvm_push_int2(out, 0); jvm_push_int2(out, 1);
        fprintf(out, "    invokestatic rt/SnoRt/do_return(II)I\n    pop\n");
        fprintf(out, "    invokestatic rt/SnoRt/fn_return_push()V\n");
        fprintf(out, "    return\nsm_pc_%d_rf_skip:\n", i); break;
    case SM_FRETURN:
        jvm_push_int2(out, 1); jvm_push_int2(out, 0);
        fprintf(out, "    invokestatic rt/SnoRt/do_return(II)I\n    pop\n");
        fprintf(out, "    invokestatic rt/SnoRt/fn_return_push()V\n");
        fprintf(out, "    return\n"); break;
    case SM_FRETURN_S:
        fprintf(out, "    invokestatic rt/SnoRt/last_ok()Z\n    ifeq sm_pc_%d_fs_skip\n", i);
        jvm_push_int2(out, 1); jvm_push_int2(out, 0);
        fprintf(out, "    invokestatic rt/SnoRt/do_return(II)I\n    pop\n");
        fprintf(out, "    invokestatic rt/SnoRt/fn_return_push()V\n");
        fprintf(out, "    return\nsm_pc_%d_fs_skip:\n", i); break;
    case SM_FRETURN_F:
        fprintf(out, "    invokestatic rt/SnoRt/last_ok()Z\n    ifne sm_pc_%d_ff_skip\n", i);
        jvm_push_int2(out, 1); jvm_push_int2(out, 0);
        fprintf(out, "    invokestatic rt/SnoRt/do_return(II)I\n    pop\n");
        fprintf(out, "    invokestatic rt/SnoRt/fn_return_push()V\n");
        fprintf(out, "    return\nsm_pc_%d_ff_skip:\n", i); break;
    case SM_NRETURN:
        jvm_push_int2(out, 2); jvm_push_int2(out, 0);
        fprintf(out, "    invokestatic rt/SnoRt/do_return(II)I\n    pop\n");
        fprintf(out, "    invokestatic rt/SnoRt/fn_return_push()V\n");
        fprintf(out, "    return\n"); break;
    case SM_NRETURN_S:
        fprintf(out, "    invokestatic rt/SnoRt/last_ok()Z\n    ifeq sm_pc_%d_ns_skip\n", i);
        jvm_push_int2(out, 2); jvm_push_int2(out, 0);
        fprintf(out, "    invokestatic rt/SnoRt/do_return(II)I\n    pop\n");
        fprintf(out, "    invokestatic rt/SnoRt/fn_return_push()V\n");
        fprintf(out, "    return\nsm_pc_%d_ns_skip:\n", i); break;
    case SM_NRETURN_F:
        fprintf(out, "    invokestatic rt/SnoRt/last_ok()Z\n    ifne sm_pc_%d_nf_skip\n", i);
        jvm_push_int2(out, 2); jvm_push_int2(out, 0);
        fprintf(out, "    invokestatic rt/SnoRt/do_return(II)I\n    pop\n");
        fprintf(out, "    invokestatic rt/SnoRt/fn_return_push()V\n");
        fprintf(out, "    return\nsm_pc_%d_nf_skip:\n", i); break;
    case SM_DEFINE_ENTRY: case SM_DEFINE: break;
    case SM_HALT: {
        const char * end_lbl = in_body ? "sm_pc_body_end" : "sm_pc_fn_end";
        fprintf(out, "    invokestatic rt/SnoRt/halt_tos()V\n");
        fprintf(out, "    goto_w %s\n", end_lbl);
        break;
    }
    case SM_PAT_LIT:
        jvm_emit_ldc_string(out, instr->a[0].s ? instr->a[0].s : "");
        fprintf(out, "    invokestatic rt/SnoPat/lit(Ljava/lang/String;)Lrt/SnoPat;\n");
        fprintf(out, "    invokestatic rt/SnoRt/push_obj(Ljava/lang/Object;)V\n"); break;
    case SM_PAT_ANY:
        fprintf(out, "    invokestatic rt/SnoRt/pop_obj()Ljava/lang/Object;\n");
        fprintf(out, "    dup\n    ifnonnull pat_any_nn_%d\n    pop\n    ldc \"\"\n    goto pat_any_done_%d\n", i, i);
        fprintf(out, "pat_any_nn_%d:\n    invokevirtual java/lang/Object/toString()Ljava/lang/String;\n", i);
        fprintf(out, "pat_any_done_%d:\n    invokestatic rt/SnoPat/any(Ljava/lang/String;)Lrt/SnoPat;\n", i);
        fprintf(out, "    invokestatic rt/SnoRt/push_obj(Ljava/lang/Object;)V\n"); break;
    case SM_PAT_NOTANY:
        fprintf(out, "    invokestatic rt/SnoRt/pop_obj()Ljava/lang/Object;\n");
        fprintf(out, "    dup\n    ifnonnull pat_nany_nn_%d\n    pop\n    ldc \"\"\n    goto pat_nany_done_%d\n", i, i);
        fprintf(out, "pat_nany_nn_%d:\n    invokevirtual java/lang/Object/toString()Ljava/lang/String;\n", i);
        fprintf(out, "pat_nany_done_%d:\n    invokestatic rt/SnoPat/notany(Ljava/lang/String;)Lrt/SnoPat;\n", i);
        fprintf(out, "    invokestatic rt/SnoRt/push_obj(Ljava/lang/Object;)V\n"); break;
    case SM_PAT_SPAN:
        fprintf(out, "    invokestatic rt/SnoRt/pop_obj()Ljava/lang/Object;\n");
        fprintf(out, "    dup\n    ifnonnull pat_span_nn_%d\n    pop\n    ldc \"\"\n    goto pat_span_done_%d\n", i, i);
        fprintf(out, "pat_span_nn_%d:\n    invokevirtual java/lang/Object/toString()Ljava/lang/String;\n", i);
        fprintf(out, "pat_span_done_%d:\n    invokestatic rt/SnoPat/span(Ljava/lang/String;)Lrt/SnoPat;\n", i);
        fprintf(out, "    invokestatic rt/SnoRt/push_obj(Ljava/lang/Object;)V\n"); break;
    case SM_PAT_BREAK:
        fprintf(out, "    invokestatic rt/SnoRt/pop_obj()Ljava/lang/Object;\n");
        fprintf(out, "    dup\n    ifnonnull pat_brk_nn_%d\n    pop\n    ldc \"\"\n    goto pat_brk_done_%d\n", i, i);
        fprintf(out, "pat_brk_nn_%d:\n    invokevirtual java/lang/Object/toString()Ljava/lang/String;\n", i);
        fprintf(out, "pat_brk_done_%d:\n    invokestatic rt/SnoPat/brk(Ljava/lang/String;)Lrt/SnoPat;\n", i);
        fprintf(out, "    invokestatic rt/SnoRt/push_obj(Ljava/lang/Object;)V\n"); break;
    case SM_PAT_LEN:
        fprintf(out, "    invokestatic rt/SnoRt/pop_obj()Ljava/lang/Object;\n");
        fprintf(out, "    invokestatic rt/SnoRt/coerce_to_long(Ljava/lang/Object;)J\n");
        fprintf(out, "    invokestatic rt/SnoPat/len(J)Lrt/SnoPat;\n");
        fprintf(out, "    invokestatic rt/SnoRt/push_obj(Ljava/lang/Object;)V\n"); break;
    case SM_PAT_POS:
        fprintf(out, "    invokestatic rt/SnoRt/pop_obj()Ljava/lang/Object;\n");
        fprintf(out, "    invokestatic rt/SnoRt/coerce_to_long(Ljava/lang/Object;)J\n");
        fprintf(out, "    invokestatic rt/SnoPat/pos(J)Lrt/SnoPat;\n");
        fprintf(out, "    invokestatic rt/SnoRt/push_obj(Ljava/lang/Object;)V\n"); break;
    case SM_PAT_RPOS:
        fprintf(out, "    invokestatic rt/SnoRt/pop_obj()Ljava/lang/Object;\n");
        fprintf(out, "    invokestatic rt/SnoRt/coerce_to_long(Ljava/lang/Object;)J\n");
        fprintf(out, "    invokestatic rt/SnoPat/rpos(J)Lrt/SnoPat;\n");
        fprintf(out, "    invokestatic rt/SnoRt/push_obj(Ljava/lang/Object;)V\n"); break;
    case SM_PAT_TAB:
        fprintf(out, "    invokestatic rt/SnoRt/pop_obj()Ljava/lang/Object;\n");
        fprintf(out, "    invokestatic rt/SnoRt/coerce_to_long(Ljava/lang/Object;)J\n");
        fprintf(out, "    invokestatic rt/SnoPat/tab(J)Lrt/SnoPat;\n");
        fprintf(out, "    invokestatic rt/SnoRt/push_obj(Ljava/lang/Object;)V\n"); break;
    case SM_PAT_RTAB:
        fprintf(out, "    invokestatic rt/SnoRt/pop_obj()Ljava/lang/Object;\n");
        fprintf(out, "    invokestatic rt/SnoRt/coerce_to_long(Ljava/lang/Object;)J\n");
        fprintf(out, "    invokestatic rt/SnoPat/rtab(J)Lrt/SnoPat;\n");
        fprintf(out, "    invokestatic rt/SnoRt/push_obj(Ljava/lang/Object;)V\n"); break;
    case SM_PAT_ARB:
        fprintf(out, "    invokestatic rt/SnoPat/arb()Lrt/SnoPat;\n");
        fprintf(out, "    invokestatic rt/SnoRt/push_obj(Ljava/lang/Object;)V\n"); break;
    case SM_PAT_ARBNO:
        fprintf(out, "    invokestatic rt/SnoRt/pop_obj()Ljava/lang/Object;\n");
        fprintf(out, "    invokestatic rt/SnoRt/coerce_to_pat(Ljava/lang/Object;)Lrt/SnoPat;\n");
        fprintf(out, "    invokestatic rt/SnoPat/arbno(Lrt/SnoPat;)Lrt/SnoPat;\n");
        fprintf(out, "    invokestatic rt/SnoRt/push_obj(Ljava/lang/Object;)V\n"); break;
    case SM_PAT_REM:
        fprintf(out, "    invokestatic rt/SnoPat/rem()Lrt/SnoPat;\n");
        fprintf(out, "    invokestatic rt/SnoRt/push_obj(Ljava/lang/Object;)V\n"); break;
    case SM_PAT_BAL:
        fprintf(out, "    invokestatic rt/SnoPat/bal()Lrt/SnoPat;\n");
        fprintf(out, "    invokestatic rt/SnoRt/push_obj(Ljava/lang/Object;)V\n"); break;
    case SM_PAT_FENCE0:
        fprintf(out, "    invokestatic rt/SnoPat/fence0()Lrt/SnoPat;\n");
        fprintf(out, "    invokestatic rt/SnoRt/push_obj(Ljava/lang/Object;)V\n"); break;
    case SM_PAT_FENCE1:
        fprintf(out, "    invokestatic rt/SnoRt/pop_obj()Ljava/lang/Object;\n");
        fprintf(out, "    invokestatic rt/SnoRt/coerce_to_pat(Ljava/lang/Object;)Lrt/SnoPat;\n");
        fprintf(out, "    invokestatic rt/SnoPat/fence1(Lrt/SnoPat;)Lrt/SnoPat;\n");
        fprintf(out, "    invokestatic rt/SnoRt/push_obj(Ljava/lang/Object;)V\n"); break;
    case SM_PAT_ABORT:
        fprintf(out, "    invokestatic rt/SnoPat/abort_()Lrt/SnoPat;\n");
        fprintf(out, "    invokestatic rt/SnoRt/push_obj(Ljava/lang/Object;)V\n"); break;
    case SM_PAT_FAIL:
        fprintf(out, "    invokestatic rt/SnoPat/fail_()Lrt/SnoPat;\n");
        fprintf(out, "    invokestatic rt/SnoRt/push_obj(Ljava/lang/Object;)V\n"); break;
    case SM_PAT_SUCCEED:
        fprintf(out, "    invokestatic rt/SnoPat/succeed_()Lrt/SnoPat;\n");
        fprintf(out, "    invokestatic rt/SnoRt/push_obj(Ljava/lang/Object;)V\n"); break;
    case SM_PAT_EPS:
        fprintf(out, "    invokestatic rt/SnoPat/eps()Lrt/SnoPat;\n");
        fprintf(out, "    invokestatic rt/SnoRt/push_obj(Ljava/lang/Object;)V\n"); break;
    case SM_PAT_CAT:
        fprintf(out, "    invokestatic rt/SnoRt/pop_obj()Ljava/lang/Object;\n");
        fprintf(out, "    invokestatic rt/SnoRt/coerce_to_pat(Ljava/lang/Object;)Lrt/SnoPat;\n");
        fprintf(out, "    invokestatic rt/SnoRt/pop_obj()Ljava/lang/Object;\n");
        fprintf(out, "    invokestatic rt/SnoRt/coerce_to_pat(Ljava/lang/Object;)Lrt/SnoPat;\n");
        fprintf(out, "    swap\n");
        fprintf(out, "    invokestatic rt/SnoPat/cat(Lrt/SnoPat;Lrt/SnoPat;)Lrt/SnoPat;\n");
        fprintf(out, "    invokestatic rt/SnoRt/push_obj(Ljava/lang/Object;)V\n"); break;
    case SM_PAT_ALT:
        fprintf(out, "    invokestatic rt/SnoRt/pop_obj()Ljava/lang/Object;\n");
        fprintf(out, "    invokestatic rt/SnoRt/coerce_to_pat(Ljava/lang/Object;)Lrt/SnoPat;\n");
        fprintf(out, "    invokestatic rt/SnoRt/pop_obj()Ljava/lang/Object;\n");
        fprintf(out, "    invokestatic rt/SnoRt/coerce_to_pat(Ljava/lang/Object;)Lrt/SnoPat;\n");
        fprintf(out, "    swap\n");
        fprintf(out, "    invokestatic rt/SnoPat/alt(Lrt/SnoPat;Lrt/SnoPat;)Lrt/SnoPat;\n");
        fprintf(out, "    invokestatic rt/SnoRt/push_obj(Ljava/lang/Object;)V\n"); break;
    case SM_PAT_DEREF:
        fprintf(out, "    invokestatic rt/SnoRt/pop_obj()Ljava/lang/Object;\n");
        fprintf(out, "    invokestatic rt/SnoPat/deref(Ljava/lang/Object;)Lrt/SnoPat;\n");
        fprintf(out, "    invokestatic rt/SnoRt/push_obj(Ljava/lang/Object;)V\n"); break;
    case SM_PAT_REFNAME:
        jvm_emit_ldc_string(out, instr->a[0].s ? instr->a[0].s : "");
        fprintf(out, "    invokestatic rt/SnoPat/refname(Ljava/lang/String;)Lrt/SnoPat;\n");
        fprintf(out, "    invokestatic rt/SnoRt/push_obj(Ljava/lang/Object;)V\n"); break;
    case SM_PAT_CAPTURE: {
        int kind = (int)instr->a[1].i;
        fprintf(out, "    invokestatic rt/SnoRt/pop_obj()Ljava/lang/Object;\n");
        fprintf(out, "    invokestatic rt/SnoRt/coerce_to_pat(Ljava/lang/Object;)Lrt/SnoPat;\n");
        jvm_emit_ldc_string(out, instr->a[0].s ? instr->a[0].s : "");
        jvm_push_int2(out, kind);
        fprintf(out, "    invokestatic rt/SnoPat/capture(Lrt/SnoPat;Ljava/lang/String;I)Lrt/SnoPat;\n");
        fprintf(out, "    invokestatic rt/SnoRt/push_obj(Ljava/lang/Object;)V\n"); break;
    }
    case SM_PAT_CAPTURE_FN: {
        const char * fname    = instr->a[0].s ? instr->a[0].s : "";
        const char * namelist = instr->a[2].s ? instr->a[2].s : "";
        fprintf(out, "    invokestatic rt/SnoRt/pop_obj()Ljava/lang/Object;\n");
        fprintf(out, "    invokestatic rt/SnoRt/coerce_to_pat(Ljava/lang/Object;)Lrt/SnoPat;\n");
        jvm_emit_ldc_string(out, fname);
        jvm_emit_ldc_string(out, namelist);
        fprintf(out, "    invokestatic rt/SnoPat/captureFn(Lrt/SnoPat;Ljava/lang/String;Ljava/lang/String;)Lrt/SnoPat;\n");
        fprintf(out, "    invokestatic rt/SnoRt/push_obj(Ljava/lang/Object;)V\n"); break;
    }
    case SM_PAT_CAPTURE_FN_ARGS: {
        const char * fname = instr->a[0].s ? instr->a[0].s : "";
        int nargs = (int)instr->a[2].i;
        fprintf(out, "    bipush %d\n", nargs);
        fprintf(out, "    anewarray java/lang/Object\n");
        for (int k = nargs - 1; k >= 0; k--) {
            fprintf(out, "    dup\n    bipush %d\n", k);
            fprintf(out, "    invokestatic rt/SnoRt/pop_obj()Ljava/lang/Object;\n");
            fprintf(out, "    aastore\n");
        }
        fprintf(out, "    invokestatic rt/SnoRt/pop_obj()Ljava/lang/Object;\n");
        fprintf(out, "    invokestatic rt/SnoRt/coerce_to_pat(Ljava/lang/Object;)Lrt/SnoPat;\n");
        fprintf(out, "    swap\n");
        jvm_emit_ldc_string(out, fname);
        fprintf(out, "    swap\n");
        fprintf(out, "    invokestatic rt/SnoPat/captureFnArgs(Lrt/SnoPat;Ljava/lang/String;[Ljava/lang/Object;)Lrt/SnoPat;\n");
        fprintf(out, "    invokestatic rt/SnoRt/push_obj(Ljava/lang/Object;)V\n"); break;
    }
    case SM_PAT_USERCALL: {
        const char * fname = instr->a[0].s ? instr->a[0].s : "";
        jvm_emit_ldc_string(out, fname);
        fprintf(out, "    invokestatic rt/SnoPat/usercall(Ljava/lang/String;)Lrt/SnoPat;\n");
        fprintf(out, "    invokestatic rt/SnoRt/push_obj(Ljava/lang/Object;)V\n"); break;
    }
    case SM_PAT_USERCALL_ARGS: {
        const char * fname = instr->a[0].s ? instr->a[0].s : "";
        int nargs = (int)instr->a[1].i;
        fprintf(out, "    bipush %d\n", nargs);
        fprintf(out, "    anewarray java/lang/Object\n");
        for (int k = nargs - 1; k >= 0; k--) {
            fprintf(out, "    dup\n    bipush %d\n", k);
            fprintf(out, "    invokestatic rt/SnoRt/pop_obj()Ljava/lang/Object;\n");
            fprintf(out, "    aastore\n");
        }
        jvm_emit_ldc_string(out, fname);
        fprintf(out, "    swap\n");
        fprintf(out, "    invokestatic rt/SnoPat/usercallArgs(Ljava/lang/String;[Ljava/lang/Object;)Lrt/SnoPat;\n");
        fprintf(out, "    invokestatic rt/SnoRt/push_obj(Ljava/lang/Object;)V\n"); break;
    }
    case SM_EXEC_STMT: {
        const char * sname = instr->a[0].s ? instr->a[0].s : "";
        int has_repl = (int)instr->a[1].i;
        jvm_emit_ldc_string(out, sname);
        jvm_push_int2(out, has_repl);
        fprintf(out, "    invokestatic rt/SnoRt/sno_exec_stmt(Ljava/lang/String;I)V\n"); break;
    }
    default: break;
    }
}
/*----------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------*/
/* Emit a range [lo,hi) using emit_jvm_one_instr. */
static void emit_jvm_sm_range(SM_Program * sm, int lo, int hi, int n,
                               const char ** fn_names, const int * fn_pcs, int fn_count,
                               FILE * out) {
    char * in_my = (char *)calloc((size_t)n, 1);
    for (int i = lo; i < hi; i++) in_my[i] = 1;
    fprintf(out, "    ; ── SM instructions %d..%d ──\n", lo, hi - 1);
    for (int i = lo; i < hi; i++) {
        fprintf(out, "sm_pc_%d:\n", i);
        emit_jvm_one_instr(sm, i, n, fn_names, fn_pcs, fn_count, 0, in_my, out);
    }
    fprintf(out, "sm_pc_fn_end:\n");
    free(in_my);
}
/*----------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------*/
/* Main orchestrator: split SM_Program into body + per-function static methods.
   The "body" contains all instructions NOT inside a function body (define_entry..end_label).
   Each function is emitted as sno_fn_NAME() with only its own PC range.
   Labels within the body (including end-labels after each function) stay in sno_body.
   Cross-method SM_JUMP is therefore impossible because the body is contiguous. */
static int emit_jvm_from_sm(SM_Program * sm, FILE * out) {
    if (!sm || !out) return 0;
    int n = sm->count;
    if (n == 0) return 0;
    /* pass 1: build function entry table + find each function's end PC.
       The SNOBOL4 DEFINE pattern preceeds each define_entry (or group of adjacent
       define_entries sharing an end label) with an SM_JUMP whose target is the
       post-function _end label. Walk backwards to find that JUMP; its target is
       the group's end PC. Each function in a group ends at the next define_entry
       (or at the group end PC, for the last one). */
    int * fn_pcs   = (int *)calloc((size_t)n, sizeof(int));
    int * fn_ends  = (int *)calloc((size_t)n, sizeof(int));
    const char ** fn_names = (const char **)calloc((size_t)n, sizeof(const char *));
    int fn_count = 0;
    for (int i = 0; i < n; i++) {
        SM_Instr * ins = &sm->instrs[i];
        if (ins->op == SM_LABEL && ins->a[2].i && ins->a[0].s) {
            fn_pcs[fn_count] = i;
            fn_names[fn_count] = ins->a[0].s;
            fn_count++;
        }
    }
    /* pass 2: compute end_pc for each function.
       First pass: find SM_JUMP-over target for each define_entry that has one.
       Second pass: for define_entries with no preceding SM_JUMP-over (back-to-back
       in a group), inherit the group_end from the preceding function.
       Final: cap to next-define-entry to keep each function's range disjoint. */
    int * group_ends = (int *)calloc((size_t)fn_count, sizeof(int));
    for (int k = 0; k < fn_count; k++) group_ends[k] = -1;
    for (int k = 0; k < fn_count; k++) {
        int p = fn_pcs[k];
        for (int j = p - 1; j >= 0; j--) {
            SM_Instr * pi = &sm->instrs[j];
            if (pi->op == SM_LABEL && pi->a[2].i) break;
            if (pi->op == SM_LABEL) break;
            if (pi->op == SM_HALT) break;
            if (pi->op == SM_JUMP) {
                int t = (int)pi->a[0].i;
                if (t > p && t <= n) { group_ends[k] = t; }
                break;
            }
        }
    }
    /* inherit: any function with group_ends[k] == -1 inherits from k-1 */
    for (int k = 0; k < fn_count; k++) {
        if (group_ends[k] < 0) {
            if (k > 0) group_ends[k] = group_ends[k - 1];
            else group_ends[k] = n;
        }
    }
    /* cap each function's end to the next define_entry's PC */
    for (int k = 0; k < fn_count; k++) {
        int my_end = group_ends[k];
        if (k + 1 < fn_count && fn_pcs[k + 1] < my_end) my_end = fn_pcs[k + 1];
        if (my_end > n) my_end = n;
        fn_ends[k] = my_end;
    }
    free(group_ends);
    /* pass 2: build boolean array — is_fn_body[i]=1 if PC i is inside a function body */
    char * is_fn_body = (char *)calloc((size_t)n, 1);
    for (int k = 0; k < fn_count; k++)
        for (int j = fn_pcs[k]; j < fn_ends[k]; j++) is_fn_body[j] = 1;
    /* emit sno_body: all PCs where is_fn_body==0, preserving order */
    /* body's "in_my_method" array is the negation of is_fn_body */
    char * body_in_my = (char *)calloc((size_t)n, 1);
    for (int i = 0; i < n; i++) body_in_my[i] = is_fn_body[i] ? 0 : 1;
    fprintf(out, ".method public static sno_body()V\n");
    fprintf(out, "    .limit stack 16\n    .limit locals 4\n");
    fprintf(out, "    ; ── SM body (non-function PCs) ──\n");
    for (int i = 0; i < n; i++) {
        if (is_fn_body[i]) continue;
        fprintf(out, "sm_pc_%d:\n", i);
        emit_jvm_one_instr(sm, i, n, fn_names, fn_pcs, fn_count, 1, body_in_my, out);
    }
    fprintf(out, "sm_pc_body_end:\n");
    fprintf(out, "    return\n");
    fprintf(out, ".end method\n");
    free(body_in_my);
    /* emit one static method per user function */
    for (int k = 0; k < fn_count; k++) {
        char mname[256]; jvm_sanitize_name(mname, sizeof mname, fn_names[k]);
        fprintf(out, ".method public static sno_fn_%s()V\n", mname);
        fprintf(out, "    .limit stack 16\n    .limit locals 4\n");
        emit_jvm_sm_range(sm, fn_pcs[k], fn_ends[k], n, fn_names, fn_pcs, fn_count, out);
        fprintf(out, "    return\n");
        fprintf(out, ".end method\n");
    }
    free(fn_pcs); free(fn_names); free(fn_ends); free(is_fn_body);
    return 0;
}
/*----------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------*/
/* Forward declarations for emit_jvm_program. */
static int emit_jvm_prologue(IR_block_t * cfg, FILE * out);
static int emit_jvm_epilogue(IR_block_t * cfg, FILE * out);
/*----------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------*/
int emit_jvm_program(const tree_t * ast_prog, FILE * out) {
    if (!ast_prog || !out) return 1;
    SM_Program * sm = sm_preamble(ast_prog);
    if (!sm) return 1;
    /* EC-3-prep: install EMIT_JVM mode for the duration of this emission so
       future SM_templates / BB_templates that read bb_emit_mode see the right
       target. The silo itself doesn't yet read bb_emit_mode (its switch arms
       hardcode JVM-specific output), so this is a no-op for current trunk
       — only matters once SM_templates start dispatching via IS_JVM. */
    bb_emit_mode_t saved_mode = bb_emit_mode;
    FILE *         saved_out  = bb_emit_out;
    emit_mode_set(EMIT_JVM, out);
    emit_jvm_prologue(NULL, out);
    emit_jvm_from_sm(sm, out);
    emit_jvm_epilogue(NULL, out);
    emit_mode_set(saved_mode, saved_out);
    sm_prog_free(sm);
    return 0;
}
/*----------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------*/
static int emit_jvm_prologue(IR_block_t * cfg, FILE * out) {
    fprintf(out, "; JVM Jasmin output — generated by scrip --jit-emit --target=jvm\n");
    fprintf(out, ".class public Prog\n");
    fprintf(out, ".super java/lang/Object\n");
    fprintf(out, ".method public static main([Ljava/lang/String;)V\n");
    fprintf(out, "    .limit stack 4\n");
    fprintf(out, "    .limit locals 2\n");
    fprintf(out, "    invokestatic rt/SnoRt/init()V\n");
    fprintf(out, "    invokestatic Prog/sno_body()V\n");
    fprintf(out, "    invokestatic rt/SnoRt/finalize_rt()I\n");
    fprintf(out, "    pop\n");
    fprintf(out, "    return\n");
    fprintf(out, ".end method\n");
    return 0;
}
/*----------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------*/
static int emit_jvm_epilogue(IR_block_t * cfg, FILE * out) {
    (void)cfg; (void)out;
    return 0;
}
/*----------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------*/
IR_emit_vtable_t g_emit_vtable_jvm = { "jvm", emit_jvm_scalar, emit_jvm_generator, emit_jvm_prologue, emit_jvm_epilogue };
