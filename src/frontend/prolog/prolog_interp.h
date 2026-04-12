#ifndef PROLOG_INTERP_H
#define PROLOG_INTERP_H
/*
 * prolog_interp.h — Prolog IR interpreter
 *
 * One-to-one mirror of prolog_emit.c:
 *   pl_exec_goal   ↔ emit_goal
 *   pl_exec_body   ↔ emit_body
 *   pl_exec_clause ↔ emit_clause
 *   pl_exec_choice ↔ emit_choice
 *   pl_eval_arith  ↔ emit_arith_expr
 *
 * Entry point: pl_execute_program(prog)
 */
#include "../snobol4/scrip_cc.h"   /* Program */
void pl_execute_program(Program *prog);
#endif
