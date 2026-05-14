/* ir_exec.h — DCG graph-walk executor: IR_exec_once, IR_exec_pump (LR-2)
 * AUTHORS: Lon Jones Cherryholmes · Claude Sonnet 4.6 (LR-2, 2026-05-14)
 *
 * Drives a wired IR_prog_t (Directed Cyclic Graph) produced by lower.
 * Two entry points:
 *
 *   IR_exec_once(cfg)          — drive cfg from entry to first succ or fail.
 *                                Returns the value on succ; FAILDESCR on fail.
 *                                Used by SM_EXEC_DCG.
 *
 *   IR_exec_pump(cfg, body_fn, ctx) — drive cfg to exhaustion, calling body_fn
 *                                once per value produced.  Returns tick count
 *                                (number of values produced; 0 = always-fail).
 *                                Used by SM_PUMP_DCG.
 *
 * The executor is a pointer-chasing state machine: no label table, no symbol
 * lookup — port_succ / port_fail / port_resume are direct C pointers.
 * Back-edges (cycles) terminate via generator exhaustion: resume → fail.
 *
 * Per-kind evaluation lives in IR_exec_node() in ir_exec.c.
 * Scalar kinds (LIT_*, VAR, BINOP, UNOP) self-evaluate and route to port_succ
 * or port_fail.  Generative kinds (EVERY, WHILE, PAT_ARB, etc.) use nd->state
 * and nd->counter to track position across resume calls.
 *
 * LR-2: infrastructure only.  All per-kind cases that are not yet implemented
 * fall through to a FAIL result — safe and explicit.  Standalone unit test
 * exercises the wired scalar + literal + alternate subsets.
 */
#pragma once
#ifndef IR_EXEC_H
#define IR_EXEC_H

#include "scrip_ir.h"

/* Callback type for IR_exec_pump: called once per value produced.
 * Return 0 to continue pumping; non-zero to stop early. */
typedef int (*IR_body_fn)(DESCR_t value, void * ctx);

/* Drive cfg once from entry: follow port_start → ... → port_succ (return value)
 * or port_fail (return FAILDESCR).  Resets node runtime state before driving.
 * cfg must be a fully wired IR_prog_t (all port_* set by lower). */
DESCR_t IR_exec_once(IR_prog_t * cfg);

/* Drive cfg to exhaustion: call body_fn(value, ctx) for each value produced.
 * Returns the total tick count (number of successful body_fn calls).
 * Resets node runtime state before first drive; leaves cfg exhausted after. */
int IR_exec_pump(IR_prog_t * cfg, IR_body_fn body_fn, void * ctx);

/* Evaluate one node in isolation: compute nd->value, return port_succ or
 * port_fail.  Called by the graph walker for self-evaluating kinds.
 * For generative kinds (state machine), updates nd->state / nd->counter.
 * Exported so the unit test can drive individual nodes directly. */
IR_t * IR_exec_node(IR_t * nd);

#endif /* IR_EXEC_H */
