/* emit_globals.h — EC-UNI-10: single global structure carrying all per-pass / per-instruction /
 *                              per-BB-node template state.
 *
 * The end-state design (see GOAL-HEADQUARTERS.md → EC-UNI End-state design) replaces today's
 * pattern of passing `(const SM_t *, const sm_ctx_t *, FILE *)` / `(BB_t *, FILE *)` into every
 * SM/BB template with parameterless calls `void sm_<op>(void)` / `void bb_<kind>(void)` that read
 * everything they need from this single global `g_emit` instance.
 *
 * Layout: one flat struct, mirroring the eventual Snocone bootstrap shape
 *   DATA('Sm_emit(BACKEND, IS_BINARY, OUT, I, N, INSTR, NODE, SID, NID,
 *                 IN_BODY, IN_MY_METHOD, PC_TO_FN, FN_NAMES, FN_COUNT,
 *                 PROG, SRCLINES)')
 *
 * Re-entrancy: g_emit is NOT re-entrant.  Single-threaded by construction.  If a template ever
 * calls another template (none today), the caller must save/restore the fields it owns. */
#ifndef EMIT_GLOBALS_H
#define EMIT_GLOBALS_H
#include "emit_core.h"
#include "SM.h"
#include "BB.h"
#include <stdio.h>
struct SM_sequence_t;
struct SrcLines;
/* The single global structure read by every SM/BB template fn at Layer 1 and by every Layer-2
 * per-backend helper.  Pass-wide fields are set once at the top of emit_program() and emit_bb().
 * Per-instruction fields are set by the dispatcher loop in emit_program() (and the corresponding
 * BB walker) once per iteration before calling the template fn. */
typedef struct {
    /* Pass-wide ---------------------------------------------------------- */
    int                          backend;       /* EMIT_TEXT/EMIT_BINARY_WIRED/EMIT_JVM/... — drives IS_<BE> macros via bb_emit_mode */
    int                          is_binary;     /* TEXT (0) vs BIN (1) — today only x86 has both */
    FILE *                       out;           /* output sink (dissolves at EC-UNI-11/12 once emit_text/emit_byte primitives land) */
    /* SM per-instruction ----------------------------------------------- */
    int                          i;             /* current PC */
    int                          n;             /* total instruction count */
    const SM_t *                 instr;         /* current SM instruction */
    /* BB per-node ----------------------------------------------------- */
    BB_t *                       node;          /* current BB node */
    int                          sid;           /* statement id (today: 0) */
    int                          nid;           /* node id = bb_node_id(node) */
    /* JVM body/method gate -------------------------------------------- */
    int                          in_body;       /* 1 if emitting function body method */
    const char *                 in_my_method;  /* byte[n]: 1 if PC i belongs to current method */
    /* NET/JVM function table ------------------------------------------ */
    const int *                  pc_to_fn;      /* PC → fn index, -1 if none */
    const char **                fn_names;      /* fn index → name */
    int                          fn_count;      /* size of fn_names */
    /* Pending EC-UNI-3-beauty fields, now globals --------------------- */
    const struct SM_sequence_t * prog;          /* full SM sequence (NRETURN/STNO need it) */
    const struct SrcLines *      srclines;      /* source-line annotations for x86 GAS */
} sm_emit_t;
extern sm_emit_t g_emit;
/* g_emit lifecycle:
 *   - emit_program() sets g_emit.backend, g_emit.out, g_emit.is_binary at entry; templates not
 *     referencing g_emit yet at EC-UNI-10(a).
 *   - EC-UNI-10(b) migrates the 7 ctx-bearing SM templates (sm_jump/sm_jump_s/sm_jump_f/sm_halt/
 *     sm_return/sm_freturn/sm_nreturn) + their producer call sites; sm_ctx.h is deleted.
 *   - EC-UNI-10(c) migrates BB templates and the remaining FILE *out-bearing SM templates.
 *
 * After EC-UNI-10(c) every template fn signature is void sm_<name>(void) / void bb_<name>(void)
 * (and int returns where they exist today).  IS_<BE> macros stay shape (g_emit.backend == EMIT_<BE>). */
#endif
