/*
 * icon_emit.h — Tiny-ICON four-port x64 ASM emitter API
 *
 * Translates an IcnNode AST to NASM x64 assembly using the Byrd Box
 * four-port model as described in Proebsting 1996.
 *
 * Ports — named with their Greek letters throughout this codebase:
 *   α  — initial entry        (synthesized label: icon_N_α)
 *   β  — re-entry / resume    (synthesized label: icon_N_β)
 *   γ  — success departure    (inherited, passed as label string in IcnPorts.γ)
 *   ω  — failure departure    (inherited, passed as label string in IcnPorts.ω)
 *
 * α and β are synthesized (emitted inline per node).
 * γ and ω are inherited — passed down as strings from parent to child.
 *
 * Extra label: icon_N_code  (intermediate logic, e.g. ICN_TO counter check)
 *
 * Bounded flag: all four ports emitted unconditionally (deferred optimization).
 *
 * Note: generated NASM labels use UTF-8 (α β γ ω) directly — NASM 2.x
 * accepts UTF-8 in local labels.  These are never exported (not in 'global'),
 * so there is no ELF symbol-table issue.
 */

#ifndef ICON_EMIT_H
#define ICON_EMIT_H

#include "icon_ast.h"
#include <stdio.h>

/* -------------------------------------------------------------------------
 * IcnPorts — inherited γ/ω labels threaded from parent to child
 * -------------------------------------------------------------------------*/
typedef struct {
    char γ[64];   /* success port — jump here on γ (succeed) */
    char ω[64];   /* failure port — jump here on ω (fail)    */
} IcnPorts;

/* -------------------------------------------------------------------------
 * IcnEmitter state
 * -------------------------------------------------------------------------*/
typedef struct {
    FILE *out;
    int   node_id;
    int   bounded;
    char  errmsg[256];
    int   had_error;
} IcnEmitter;

/* -------------------------------------------------------------------------
 * API
 * -------------------------------------------------------------------------*/
void icn_emit_init(IcnEmitter *em, FILE *out);
void icn_emit_file(IcnEmitter *em, IcnNode **nodes, int count);
void icn_emit_proc(IcnEmitter *em, IcnNode *proc);

/* Emit one expression; returns synthesized α/β labels in *out_α, *out_β */
void icn_emit_expr(IcnEmitter *em, IcnNode *n, IcnPorts ports,
                   char *out_α, char *out_β);

int  icn_new_id   (IcnEmitter *em);
void icn_label_α  (int id, char *buf, size_t sz);
void icn_label_β  (int id, char *buf, size_t sz);
void icn_label_code(int id, char *buf, size_t sz);

#endif /* ICON_EMIT_H */
