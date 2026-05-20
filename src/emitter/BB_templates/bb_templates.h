/* bb_templates.h — forward declarations for all BB template functions.
   Include this in emit_core.c so emit_bb_node can call them.
   Each function is defined in BB_templates/bb_<kind>.c.
   EC-UNI-10(c): all top-level BB templates are parameterless and read g_emit.node / g_emit.out.
   bb_capture keeps `int imm` as a genuine call-site discriminator (BB_PAT_ASSIGN_IMM vs
   BB_PAT_ASSIGN_COND), mirroring the sm_pat_any_i(int i) precedent in the remaining SM
   family files. */
#pragma once
#include "emit_core.h"
#include "emit_globals.h"
#include "emit_ir.h"
#include <stdio.h>

void bb_lit    (void);
void bb_any    (void);
void bb_notany (void);
void bb_span   (void);
void bb_break  (void);
void bb_arb    (void);
void bb_arbno  (void);
void bb_cat    (void);
void bb_alt    (void);
void bb_len    (void);
void bb_pos    (void);
void bb_tab    (void);
void bb_rem    (void);
void bb_fence  (void);
void bb_abort  (void);
void bb_capture(int imm);
