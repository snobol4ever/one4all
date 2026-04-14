/*
 * polyglot.h — polyglot runtime init and dispatch public interface
 *
 * AUTHORS: Lon Jones Cherryholmes · Claude Sonnet 4.6
 * DATE:    2026-04-14
 * PURPOSE: GOAL-FULL-INTEGRATION FI-7 — polyglot layer extracted from scrip.c
 */

#ifndef POLYGLOT_H
#define POLYGLOT_H

#include "frontend/snobol4/scrip_cc.h"
#include "driver/interp.h"   /* ScripModule, ScripModuleRegistry, g_registry */

/* g_polyglot is declared in interp.h (owned by interp.c) */

void     polyglot_init   (Program *prog);
void     polyglot_execute(Program *prog);
Program *parse_scrip_polyglot(const char *src, const char *filename);

#endif /* POLYGLOT_H */
