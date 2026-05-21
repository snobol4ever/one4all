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
 *                 IN_BODY, IN_MY_METHOD, PC_TO_FN, FN_NAMES, FN_PCS, FN_COUNT,
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
    /* BB Byrd-box port labels (for x86 BB templates lifted from emit_bb.c).
       Set by the dispatcher (emit_flat_ir / emit_bb_node) before calling the template;
       read by the template's IS_X86 arm.  α (alpha-succ), β (beta-back), γ (gamma-fail).
       These are LABEL-NAME STRINGS (not bb_label_t pointers): the label *name* is the
       Snocone-translatable identity, since in Snocone a label is identified by a name
       string and its offset is looked up in a name-keyed table.  C strings are the one
       admitted pointer type here because Snocone strings transliterate to const char *.
       Today these fields are not yet filled by emit_bb_node (the original emit_bb_x*
       fns are the live path); the templates' IS_X86 arms exist in shape only, pending
       rewire in the next sweep. */
    const char *                 lbl_succ;      /* α: success continuation — name string */
    const char *                 lbl_fail;      /* γ: failure continuation — name string */
    const char *                 lbl_back;      /* β: backtrack target    — name string */
    /* BB per-op template parameters — what the lifted emit_bb_x* fns took as args.
       The dispatcher (emit_bb_node) fills these per node; templates read from here
       instead of taking parameters.  All are values: scalars + strings + a single
       function-pointer scalar (bb_box_fn, kept as void * here to avoid pulling
       bb_box.h's bb_scan enum into emit_globals.h scope) for the capture/name cases.
       Fields are union-style by convention — the active subset depends on the node kind. */
    void *                       child_fn;      /* xcallcap/xfnme/xnme: child Byrd-box fn (bb_box_fn cast as void *) */
    const char *                 op_name1;      /* fnc_name (CALLCAP), varname (CAP_x / DEREF / USERPAT), chars (SPAN/ANY/BREAK/NOTANY) */
    const char *                 op_name2;      /* c_fn_name (charset) */
    const char *                 op_kind;       /* kind_name (charset) — "SPAN"/"ANY"/"BREAK"/"NOTANY" */
    /* JVM body/method gate -------------------------------------------- */
    int                          in_body;       /* 1 if emitting function body method */
    const char *                 in_my_method;  /* byte[n]: 1 if PC i belongs to current method */
    /* NET/JVM function table ------------------------------------------ */
    const int *                  pc_to_fn;      /* PC → fn index, -1 if none */
    const char **                fn_names;      /* fn index → name */
    const int *                  fn_pcs;        /* fn index → entry PC (EC-UNI-13(b): added for SM_CALL_FN/SM_SUSPEND_VALUE NET arm) */
    int                          fn_count;      /* size of fn_names */
    /* Pending EC-UNI-3-beauty fields, now globals --------------------- */
    const struct SM_sequence_t * prog;          /* full SM sequence (NRETURN/STNO need it) */
    const struct SrcLines *      srclines;      /* source-line annotations for x86 GAS */
} sm_emit_t;
extern sm_emit_t g_emit;
/* TEMPLATE_ADDR_* macros — used by BB templates in BB_templates/ lifted from emit_bb.c.
   Σ and Σlen are the runtime subject buffer / length, declared in bb_box.h. */
extern const char *Σ;
extern int         Σlen;
#define TEMPLATE_ADDR_SIGMA   ((uint64_t)(uintptr_t)&Σ)
#define TEMPLATE_ADDR_SIGLEN  ((uint64_t)(uintptr_t)&Σlen)
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
