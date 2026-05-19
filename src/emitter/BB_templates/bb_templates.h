/* bb_templates.h — forward declarations for all BB template functions.
   Include this in emit_core.c so emit_bb_node can call them.
   Each function is defined in BB_templates/ec_bb_<kind>.c. */
#pragma once
#include "emit_core.h"
#include "emit_ir.h"
#include <stdio.h>

void ec_bb_lit    (IR_t *nd, FILE *out);
void ec_bb_any    (IR_t *nd, FILE *out);
void ec_bb_notany (IR_t *nd, FILE *out);
void ec_bb_span   (IR_t *nd, FILE *out);
void ec_bb_break  (IR_t *nd, FILE *out);
void ec_bb_arb    (IR_t *nd, FILE *out);
void ec_bb_arbno  (IR_t *nd, FILE *out);
void ec_bb_cat    (IR_t *nd, FILE *out);
void ec_bb_alt    (IR_t *nd, FILE *out);
void ec_bb_len    (IR_t *nd, FILE *out);
void ec_bb_pos    (IR_t *nd, FILE *out);
void ec_bb_tab    (IR_t *nd, FILE *out);
void ec_bb_rem    (IR_t *nd, FILE *out);
void ec_bb_fence  (IR_t *nd, FILE *out);
void ec_bb_abort  (IR_t *nd, FILE *out);
void ec_bb_capture(IR_t *nd, FILE *out, int imm);
