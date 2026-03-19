/*
 * emit_byrd_jvm.c — JVM Jasmin text emitter for sno2c
 *
 * Consumes the same Program* IR as emit_byrd_asm.c.
 * Emits Jasmin assembler text (.j files) assembled by jasmin.jar.
 *
 * Pipeline:
 *   sno2c -jvm prog.sno > prog.j
 *   java -jar jasmin.jar prog.j -d outdir/
 *   java -cp outdir/ <ClassName>
 *
 * Sprint map:
 *   J0  M-JVM-HELLO   — skeleton: null program → exit 0          ← NOW
 *   J1  M-JVM-LIT     — OUTPUT = 'hello' correct
 *   J2  M-JVM-ASSIGN  — variable assign + arithmetic
 *   J3  M-JVM-GOTO    — :S/:F branching
 *   J4  M-JVM-PATTERN — Byrd boxes: LIT/SEQ/ALT/ARBNO
 *   J5  M-JVM-CAPTURE — . and $ capture
 *   J-R1..J-R5        — corpus ladder 106/106
 *
 * Design:
 *   Each SNOBOL4 program becomes one public JVM class with a
 *   public static main([Ljava/lang/String;)V method.
 *   Class name is derived from the source filename (or "SnobolProg"
 *   if reading stdin).
 *
 *   Three-column Jasmin layout mirrors the NASM backend:
 *     label:       instruction    operands
 *
 *   null/failure is represented as Java null (aconst_null).
 *   Success leaves a non-null value on the stack.
 *   :S/:F goto uses ifnull / ifnonnull — mirrors JCON bc_conditional_transfer_to.
 *
 * Reference:
 *   JCON gen_bc.icn  — Icon→JVM blueprint (α/β/γ/ω → four Labels per node)
 *   emit_byrd_asm.c  — direct structural oracle (same IR, same corpus)
 *   Jasmin docs      — http://jasmin.sourceforge.net/
 */

#include "sno2c.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>

/* -----------------------------------------------------------------------
 * Output helpers — three-column layout
 * ----------------------------------------------------------------------- */

static FILE *jvm_out;

#define COL_LBL   0
#define COL_INSTR 24
#define COL_OPS   36

static int jvm_col = 0;

static void jc(char c) {
    fputc(c, jvm_out);
    if (c == '\n') jvm_col = 0;
    else if ((c & 0xC0) != 0x80) jvm_col++;
}

static void js(const char *s) { for (; *s; s++) jc(*s); }

static void jpad(int col) {
    if (jvm_col >= col) { jc('\n'); }
    while (jvm_col < col) jc(' ');
}

/* J(fmt, ...) — emit raw text (no column management) */
static void J(const char *fmt, ...) {
    va_list ap;
    va_start(ap, fmt);
    vfprintf(jvm_out, fmt, ap);
    va_end(ap);
    /* update col tracker for simple cases */
    /* (not perfect for all fmt strings, but adequate) */
}

/* JL(label, instr, ops) — three-column line */
static void JL(const char *label, const char *instr, const char *ops) {
    jvm_col = 0;
    if (label && label[0]) {
        js(label); jc(':');
    }
    jpad(COL_INSTR);
    js(instr);
    if (ops && ops[0]) {
        jpad(COL_OPS);
        js(ops);
    }
    jc('\n');
}

/* JI(instr, ops) — instruction with no label */
static void JI(const char *instr, const char *ops) {
    JL("", instr, ops);
}

/* JC(comment) — comment line */
static void JC(const char *comment) {
    J("; %s\n", comment);
}

/* JSep — visual separator between SNOBOL4 statements */
static void JSep(const char *tag) {
    J("; === %s ", tag ? tag : "");
    int used = 7 + (tag ? (int)strlen(tag) + 1 : 0);
    for (int i = used; i < 72; i++) fputc('=', jvm_out);
    J("\n");
}

/* -----------------------------------------------------------------------
 * Class name derivation from filename
 * ----------------------------------------------------------------------- */

static char jvm_classname[256];

static void jvm_set_classname(const char *filename) {
    if (!filename || strcmp(filename, "<stdin>") == 0) {
        strcpy(jvm_classname, "SnobolProg");
        return;
    }
    /* strip directory */
    const char *base = strrchr(filename, '/');
    base = base ? base + 1 : filename;
    /* strip extension */
    char buf[256];
    strncpy(buf, base, sizeof buf - 1); buf[sizeof buf - 1] = '\0';
    char *dot = strrchr(buf, '.');
    if (dot) *dot = '\0';
    /* sanitize: replace non-alnum with _ */
    char *p = buf;
    if (!isalpha((unsigned char)*p) && *p != '_') *p = '_';
    for (; *p; p++)
        if (!isalnum((unsigned char)*p) && *p != '_') *p = '_';
    /* capitalise first letter (Java class convention) */
    buf[0] = (char)toupper((unsigned char)buf[0]);
    strncpy(jvm_classname, buf, sizeof jvm_classname - 1);
}

/* -----------------------------------------------------------------------
 * J0 skeleton — emit a null program that runs and exits 0
 * ----------------------------------------------------------------------- */

static void jvm_emit_header(void) {
    J(".class public %s\n", jvm_classname);
    J(".super java/lang/Object\n");
    J("\n");
    /* static field: PrintStream for OUTPUT */
    J("; Runtime field: System.out cached at class load\n");
    J(".field static sno_stdout Ljava/io/PrintStream;\n");
    J("\n");
    /* static initialiser: cache System.out */
    J(".method static <clinit>()V\n");
    J("    .limit stack 1\n");
    J("    .limit locals 0\n");
    J("    getstatic       java/lang/System/out Ljava/io/PrintStream;\n");
    J("    putstatic       %s/sno_stdout Ljava/io/PrintStream;\n", jvm_classname);
    J("    return\n");
    J(".end method\n");
    J("\n");
}

static void jvm_emit_main_open(void) {
    J(".method public static main([Ljava/lang/String;)V\n");
    J("    .limit stack 8\n");
    J("    .limit locals 32\n");
    J("\n");
}

static void jvm_emit_main_close(void) {
    JC("program end");
    JI("return", "");
    J(".end method\n");
}

/* -----------------------------------------------------------------------
 * Public entry point
 * ----------------------------------------------------------------------- */

void jvm_emit(Program *prog, FILE *out, const char *filename) {
    jvm_out = out;
    jvm_set_classname(filename);

    JC("Generated by sno2c -jvm");
    JC("Assemble: java -jar jasmin.jar <file>.j -d .");
    JC("Run:      java <classname>");
    J("\n");

    jvm_emit_header();
    jvm_emit_main_open();

    /* J0: walk statements — emit stubs only (no body yet) */
    if (prog && prog->head) {
        for (STMT_t *s = prog->head; s; s = s->next) {
            if (s->is_end) {
                JSep("END");
                char lbuf[128];
                snprintf(lbuf, sizeof lbuf, "L_END");
                J("%s:\n", lbuf);
                JI("nop", "");
                break;
            }
            if (s->label) {
                char lbuf[128];
                snprintf(lbuf, sizeof lbuf, "L_%s", s->label);
                JSep(s->label);
                J("%s:\n", lbuf);
                JI("nop", "; stub — J1+ will fill body");
            }
        }
    }

    jvm_emit_main_close();
}
