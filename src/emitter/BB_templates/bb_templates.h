/* bb_templates.h — forward declarations for all BB template functions.
   Include this in emit_core.c so emit_bb_node can call them.
   Each function is defined in BB_templates/bb_<kind>.c. */
#pragma once
#include "emit_core.h"
#include "emit_ir.h"
#include <stdio.h>

void bb_lit    (BB_t *nd, FILE *out);
void bb_any    (BB_t *nd, FILE *out);
void bb_notany (BB_t *nd, FILE *out);
void bb_span   (BB_t *nd, FILE *out);
void bb_break  (BB_t *nd, FILE *out);
void bb_arb    (BB_t *nd, FILE *out);
void bb_arbno  (BB_t *nd, FILE *out);
void bb_cat    (BB_t *nd, FILE *out);
void bb_alt    (BB_t *nd, FILE *out);
void bb_len    (BB_t *nd, FILE *out);
void bb_pos    (BB_t *nd, FILE *out);
void bb_tab    (BB_t *nd, FILE *out);
void bb_rem    (BB_t *nd, FILE *out);
void bb_fence  (BB_t *nd, FILE *out);
void bb_abort  (BB_t *nd, FILE *out);
void bb_capture(BB_t *nd, FILE *out, int imm);
