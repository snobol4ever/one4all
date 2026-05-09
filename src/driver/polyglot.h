/*
 * polyglot.h — polyglot runtime init and dispatch public interface
 *
 * AUTHORS: Lon Jones Cherryholmes · Claude Sonnet 4.6
 * DATE:    2026-04-14
 * PURPOSE: GOAL-FULL-INTEGRATION FI-7 — polyglot layer extracted from scrip.c
 */

#ifndef POLYGLOT_H
#define POLYGLOT_H

#include <stdint.h>
#include "frontend/snobol4/scrip_cc.h"
#include "driver/interp.h"   /* ScripModule, ScripModuleRegistry, g_registry */

/* g_polyglot is declared in interp.h (owned by interp.c) */

/* FI-8: lazy-init verification counters (extern for test scripts) */
extern int g_fi8_icn_init_count;
extern int g_fi8_pl_init_count;

/* CH-17g-irrun-lowers: when set, polyglot_execute runs sm_lower +
 * sm_resolve_proc_entry_pcs after polyglot_init so entry_pcs are
 * populated before proc_table_call dispatches.  Set by scrip.c for
 * the --ir-run non-SNO path.  Default 0 (disabled). */
extern int g_irrun_lowers;

uint32_t polyglot_lang_mask(CODE_t *prog);
void     polyglot_init   (CODE_t *prog, uint32_t lang_mask);
void     polyglot_execute(CODE_t *prog);
CODE_t *parse_scrip_polyglot(const char *src, const char *filename);

#endif /* POLYGLOT_H */
