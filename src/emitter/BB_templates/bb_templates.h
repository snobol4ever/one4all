/* bb_templates.h — forward declarations for all BB template functions.
   Include this in emit_core.c so emit_bb_node can call them.
   Each function is defined in BB_templates/bb_<kind>.c. */
#pragma once
#include "emit_core.h"
#include "emit_ir.h"
#include <stdio.h>

void bb_lit    (IR_t *nd, FILE *out);
void bb_any    (IR_t *nd, FILE *out);
void bb_notany (IR_t *nd, FILE *out);
void bb_span   (IR_t *nd, FILE *out);
void bb_break  (IR_t *nd, FILE *out);
void bb_arb    (IR_t *nd, FILE *out);
void bb_arbno  (IR_t *nd, FILE *out);
void bb_cat    (IR_t *nd, FILE *out);
void bb_alt    (IR_t *nd, FILE *out);
void bb_len    (IR_t *nd, FILE *out);
void bb_pos    (IR_t *nd, FILE *out);
void bb_tab    (IR_t *nd, FILE *out);
void bb_rem    (IR_t *nd, FILE *out);
void bb_fence  (IR_t *nd, FILE *out);
void bb_abort  (IR_t *nd, FILE *out);
void bb_capture(IR_t *nd, FILE *out, int imm);
