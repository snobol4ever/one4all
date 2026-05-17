#ifndef LOWER_SNO_H
#define LOWER_SNO_H
/*=============================================================================
 * lower_sno — Snocone tree_t → portable SNOBOL4 source emitter.
 *
 * This is the implementation of `scrip --dump-sno`.  It walks a tree_t AST
 * produced by any of the six SCRIP frontends (Snocone, SNOBOL4, Icon, Prolog,
 * Raku, Rebus) and emits a portable SNOBOL4 source program to `out`.
 *
 * Target dialect:
 *   - SPITBOL x64 (primary)              — /home/claude/x64/bin/sbl -bf
 *   - SCRIP --sm-run / --ir-run / --jit-run
 *   - CSNOBOL4 (only when needed for Silly target)
 *
 * The output uses ONLY constructs in the intersection of the above runtimes,
 * per GOAL-PARSER-SC-TRANSPILE.md Implementation Constraint 5.  In practice
 * this means:
 *   - Implicit pattern match (space, SPITBOL priority 4), NOT explicit `?`
 *   - &FULLSCAN = 1 at top of file (accepted as no-op by SPITBOL, required
 *     by standard SNOBOL4 for the heuristic-free pattern matching SCRIP
 *     parsers depend on)
 *   - Gimpel-template function definitions (DEFINE + label + body + RETURN
 *     + end-label), per SPITBOL Manual Ch.8 (line 5973 of /tmp/spitbol.txt).
 *   - Goto-field syntax `:S(L)`, `:F(L)`, `:(L)` per Ch.14 (line 9576+).
 *
 * Trust LOWER:  if the AST is wrong, the emitter passes the wrong shape
 * through faithfully.  Per goal file:
 *   "The transpiler is mechanical — it must not 'smart-correct' anything
 *    from the AST."
 * Bugs surfaced by sync-monitor divergence are fixed in the producing
 * frontend / LOWER, not in this file.
 *
 * Author: GOAL-PARSER-SC-TRANSPILE.md — SCT-1.  Opened 2026-05-17 by Lon.
 *===========================================================================*/
#include <stdio.h>
struct tree_t;
/*-----------------------------------------------------------------------------
 * tree_to_sno — emit a portable SNOBOL4 program for `ast` to `out`.
 *
 * The AST is treated as read-only; the emitter does not mutate or free it.
 * Output is always followed by an `END` statement (SPITBOL Manual Ch.14,
 * line 9704+).  Returns the number of source lines emitted, or -1 on error.
 *---------------------------------------------------------------------------*/
int tree_to_sno(const struct tree_t *ast, FILE *out);
#endif
