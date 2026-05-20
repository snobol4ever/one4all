/* emit_globals.c — EC-UNI-10: single global instance of sm_emit_t.
 * See emit_globals.h for design notes and field semantics.  BSS-initialized; the
 * dispatcher loops in emit_program() / emit_bb() populate it per iteration. */
#include "emit_globals.h"
sm_emit_t g_emit;
