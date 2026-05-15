#!/bin/bash
# scripts/test_smoke_snobol4_net_bb_gate.sh — SN4-NET-2 gate
# Generates all 19 BB class stubs via a small C driver + emit_net.c, assembles with ilasm.
# Expected: ilasm exits 0 (Operation completed successfully) for all 19.
set -e
HERE="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
REPO="$(cd "$HERE/.." && pwd)"
SRC="$REPO/src"
OUT="/tmp/sn4_net_bb_gate"
mkdir -p "$OUT"

# Build a minimal test driver that exercises all 19 emitters
cat > "$OUT/bb_gate_driver.c" << 'CEOF'
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* Stub IR_t for gate testing */
typedef enum {
    IR_PAT_LIT=24, IR_PAT_ANY, IR_PAT_SPAN, IR_PAT_BREAK, IR_PAT_ARB,
    IR_PAT_ARBNO, IR_PAT_CAT, IR_PAT_ALT, IR_PAT_ASSIGN_IMM, IR_PAT_ASSIGN_COND,
    IR_PAT_LEN, IR_PAT_NOTANY, IR_PAT_POS, IR_PAT_TAB, IR_PAT_REM,
    IR_PAT_FENCE, IR_PAT_ABORT
} IR_e;

struct IR_t { IR_e t; long ival; long ival2; double dval; const char * sval; const char * sval2; void ** c; int nc; void * alpha; void * beta; void * gamma; void * omega; void * opaque; };
typedef struct IR_t IR_t;

/* Forward declare the 19 emitter functions from emit_net.c */
void emit_net_bb_lit(IR_t*,FILE*,int,int);
void emit_net_bb_any(IR_t*,FILE*,int,int);
void emit_net_bb_notany(IR_t*,FILE*,int,int);
void emit_net_bb_span(IR_t*,FILE*,int,int);
void emit_net_bb_break(IR_t*,FILE*,int,int);
void emit_net_bb_len(IR_t*,FILE*,int,int);
void emit_net_bb_pos(IR_t*,FILE*,int,int);
void emit_net_bb_rpos(IR_t*,FILE*,int,int);
void emit_net_bb_tab(IR_t*,FILE*,int,int);
void emit_net_bb_rtab(IR_t*,FILE*,int,int);
void emit_net_bb_rem(IR_t*,FILE*,int,int);
void emit_net_bb_arb(IR_t*,FILE*,int,int);
void emit_net_bb_arbno(IR_t*,FILE*,int,int);
void emit_net_bb_cat(IR_t*,FILE*,int,int);
void emit_net_bb_alt(IR_t*,FILE*,int,int);
void emit_net_bb_capture(IR_t*,FILE*,int,int,int);
void emit_net_bb_fence(IR_t*,FILE*,int,int);
void emit_net_bb_abort(IR_t*,FILE*,int,int);

int main(void) {
    FILE * out = fopen("/tmp/sn4_net_bb_gate/all19.il", "w");
    if (!out) { fprintf(stderr, "cannot open output\n"); return 1; }
    /* assembly header + SnoRt class + boxes extern */
    fprintf(out, ".assembly extern mscorlib {}\n");
    fprintf(out, ".assembly extern boxes {}\n");
    fprintf(out, ".assembly all19 {}\n");
    fprintf(out, ".module all19.exe\n");
    fprintf(out, ".class public auto ansi beforefieldinit Prog extends [mscorlib]System.Object\n{\n");
    /* Emit all 19 BB class stubs as nested classes */
    IR_t nd = {0};
    nd.sval = "hello"; nd.ival = 3; nd.ival2 = 0;
    nd.t = IR_PAT_LIT;         emit_net_bb_lit(&nd, out, 1, 1);
    nd.sval = "abc";
    nd.t = IR_PAT_ANY;         emit_net_bb_any(&nd, out, 2, 2);
    nd.t = IR_PAT_NOTANY;      emit_net_bb_notany(&nd, out, 3, 3);
    nd.t = IR_PAT_SPAN;        emit_net_bb_span(&nd, out, 4, 4);
    nd.t = IR_PAT_BREAK;       emit_net_bb_break(&nd, out, 5, 5);
    nd.sval = NULL; nd.ival = 5;
    nd.t = IR_PAT_LEN;         emit_net_bb_len(&nd, out, 6, 6);
    nd.ival = 2; nd.ival2 = 0;
    nd.t = IR_PAT_POS;         emit_net_bb_pos(&nd, out, 7, 7);
    nd.ival = 1; nd.ival2 = 1;
    emit_net_bb_rpos(&nd, out, 8, 8);
    nd.ival = 4; nd.ival2 = 0;
    nd.t = IR_PAT_TAB;         emit_net_bb_tab(&nd, out, 9, 9);
    nd.ival = 2; nd.ival2 = 1;
    emit_net_bb_rtab(&nd, out, 10, 10);
    nd.ival = 0; nd.ival2 = 0;
    nd.t = IR_PAT_REM;         emit_net_bb_rem(&nd, out, 11, 11);
    nd.t = IR_PAT_ARB;         emit_net_bb_arb(&nd, out, 12, 12);
    nd.t = IR_PAT_ARBNO;       emit_net_bb_arbno(&nd, out, 13, 13);
    nd.t = IR_PAT_CAT;         emit_net_bb_cat(&nd, out, 14, 14);
    nd.t = IR_PAT_ALT;         emit_net_bb_alt(&nd, out, 15, 15);
    nd.sval = "X"; nd.ival = 1;
    nd.t = IR_PAT_ASSIGN_IMM;  emit_net_bb_capture(&nd, out, 16, 16, 1);
    nd.sval = "Y";
    nd.t = IR_PAT_ASSIGN_COND; emit_net_bb_capture(&nd, out, 17, 17, 0);
    nd.sval = NULL;
    nd.t = IR_PAT_FENCE;       emit_net_bb_fence(&nd, out, 18, 18);
    nd.t = IR_PAT_ABORT;       emit_net_bb_abort(&nd, out, 19, 19);
    fprintf(out, "  .method public static void Main() cil managed\n  {\n");
    fprintf(out, "    .entrypoint\n    .maxstack 1\n    ret\n  }\n");
    fprintf(out, "}\n");
    fclose(out);
    return 0;
}
CEOF

# Compile the driver with the emit_net.c BB functions (stubbing out dependencies)
cat > "$OUT/stubs.c" << 'SEOF'
/* Stubs for emit_net.c dependencies not needed for BB gate */
#include <stdio.h>
typedef void* SM_Program;
typedef void* tree_t;
typedef void* IR_block_t;
SM_Program * sm_preamble(const tree_t * p) { (void)p; return NULL; }
void sm_prog_free(SM_Program * p) { (void)p; }
SEOF

# Extract just the BB emitter functions (not the SM walker or vtable which need full includes)
# Use a simplified compile with stub IR types
gcc -O0 -w \
    -I"$SRC/include" \
    -I"$SRC/emitter" \
    -include "$OUT/bb_gate_types.h" \
    "$OUT/bb_gate_driver.c" \
    "$OUT/stubs.c" \
    2>"$OUT/compile.err" || true

echo "Gate: checking bb_gate_types.h approach..."

# Actually simpler: just compile emit_net.c in isolation with stubs and run the driver
cat > "$OUT/bb_gate_types.h" << 'HEOF'
#pragma once
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
typedef enum {
    IR_PAT_LIT=24, IR_PAT_ANY, IR_PAT_SPAN, IR_PAT_BREAK, IR_PAT_ARB,
    IR_PAT_ARBNO, IR_PAT_CAT, IR_PAT_ALT, IR_PAT_ASSIGN_IMM, IR_PAT_ASSIGN_COND,
    IR_PAT_LEN, IR_PAT_NOTANY, IR_PAT_POS, IR_PAT_TAB, IR_PAT_REM,
    IR_PAT_FENCE, IR_PAT_ABORT, IR_PAT_CALLOUT, IR_PAT_FENCE0=50
} IR_e;
struct IR_t { IR_e t; long ival; long ival2; double dval; const char * sval; const char * sval2; struct IR_t ** c; int nc; void * alpha; void * beta; void * gamma; void * omega; void * opaque; };
typedef struct IR_t IR_t;
typedef void* IR_block_t;
typedef void* SM_Program;
typedef void* tree_t;
typedef struct { const char * target_name; int (*emit_scalar)(IR_t*,FILE*); int (*emit_generator)(IR_t*,FILE*); int (*emit_prologue)(IR_block_t*,FILE*); int (*emit_epilogue)(IR_block_t*,FILE*); } IR_emit_vtable_t;
typedef struct { long i; double f; const char * s; void * ptr; } SM_Arg;
typedef enum { SM_LABEL=0,SM_JUMP,SM_JUMP_S,SM_JUMP_F,SM_HALT,SM_STNO,SM_PUSH_LIT_S,SM_PUSH_LIT_CS,SM_PUSH_LIT_I,SM_PUSH_LIT_F,SM_PUSH_NULL,SM_PUSH_NULL_NOFLIP,SM_PUSH_VAR,SM_PUSH_EXPR,SM_PUSH_EXPRESSION,SM_CALL_EXPRESSION,SM_STORE_VAR,SM_VOID_POP,SM_ADD,SM_SUB,SM_MUL,SM_DIV,SM_EXP,SM_MOD,SM_CONCAT,SM_COERCE_NUM,SM_NEG } sm_opcode_t;
typedef struct { sm_opcode_t op; SM_Arg a[4]; } SM_Instr;
struct SM_Program_s { SM_Instr * instrs; int count; };
typedef struct SM_Program_s SM_Program_real;
int emit_ir_block(IR_block_t*,FILE*,const char*) { return 0; }
int ir_node_id(IR_t*n){return(int)((size_t)n%100000);}
int ir_is_generator(IR_e k){return k>=24&&k<=50;}
void ir_walk(IR_block_t*c,void(*v)(IR_t*,void*),void*x){(void)c;(void)v;(void)x;}
extern IR_emit_vtable_t g_emit_vtable_net;
SM_Program * sm_preamble(const tree_t * p){(void)p;return NULL;}
void sm_prog_free(SM_Program*p){(void)p;}
HEOF

gcc -O0 -w \
    -include "$OUT/bb_gate_types.h" \
    -I"$SRC/include" \
    "$SRC/emitter/emit_net.c" \
    "$OUT/bb_gate_driver.c" \
    -o "$OUT/bb_gate" 2>"$OUT/compile.err"

if [ $? -ne 0 ]; then
    echo "FAIL: driver compile error:"
    cat "$OUT/compile.err"
    exit 1
fi

"$OUT/bb_gate" 2>"$OUT/run.err"
if [ $? -ne 0 ]; then
    echo "FAIL: driver runtime error:"
    cat "$OUT/run.err"
    exit 1
fi

# Assemble with ilasm (referencing boxes.dll)
BOXES="$REPO/src/runtime/net/boxes.dll"
if [ ! -f "$BOXES" ]; then
    echo "SKIP: boxes.dll not found at $BOXES; building it now"
    ilasm /dll /output:"$BOXES" "$REPO/src/runtime/net/bb_boxes.il" > "$OUT/boxes_build.log" 2>&1
fi

ilasm /output:"$OUT/all19.exe" \
      "$OUT/all19.il" \
      "$REPO/src/runtime/net/SnoRt.il" \
      > "$OUT/ilasm.log" 2>&1
RC=$?
if [ $RC -ne 0 ]; then
    echo "FAIL: ilasm error:"
    cat "$OUT/ilasm.log"
    exit 1
fi
echo "PASS: all 19 BB class stubs assembled with ilasm"
