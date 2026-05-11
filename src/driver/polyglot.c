/*
 * polyglot.c — polyglot runtime init and dispatch (SCRIP unified driver)
 *
 * Contains: ScripModule registry, polyglot_init(), polyglot_execute(),
 *           parse_scrip_polyglot().
 *
 * Extracted from scrip.c by GOAL-FULL-INTEGRATION FI-7.
 * After this step scrip.c is main() + arg parse + frontend dispatch only.
 *
 * AUTHORS: Lon Jones Cherryholmes · Claude Sonnet 4.6
 * DATE:    2026-04-14
 * PURPOSE: Polyglot layer — separated to enable parallel frontend sessions
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <gc.h>

#include "frontend/snobol4/scrip_cc.h"
extern CODE_t *sno_parse(FILE *f, const char *filename);
extern CODE_t *sno_parse_string(const char *src);
extern int64_t kw_case;   /* &CASE: 0=fold, 1=sensitive; default 1 matches SPITBOL */
#include "frontend/prolog/prolog_driver.h"
#include "frontend/prolog/prolog_atom.h"
#include "frontend/icon/icon_driver.h"
#include "frontend/raku/raku_driver.h"
#include "frontend/rebus/rebus_lower.h"
#include "runtime/interp/coro_runtime.h"
#include "runtime/interp/pl_runtime.h"
#include "driver/interp.h"
#include "driver/polyglot.h"
#include "runtime/x86/lower.h"   /* CH-17g-irrun-lowers: sm_lower */
#include "runtime/x86/sm_prog.h"    /* CH-17g-irrun-lowers: sm_prog_free */
#include "driver/scrip_sm.h"        /* CH-17g-irrun-lowers: sm_resolve_irrun_entry_pcs */

ScripModuleRegistry g_registry;   /* zero-initialised; nmod==0 for single-lang */

/* CH-17g-irrun-lowers: set by scrip.c --ir-run non-SNO path to populate
 * entry_pcs before proc_table_call dispatches.  Default 0. */
int g_irrun_lowers = 0;



/* ══════════════════════════════════════════════════════════════════════════
 * polyglot_lang_mask — compute bitmask of languages present in prog  (FI-8)
 *
 * Returns a uint32_t with bit (1u << LANG_X) set for each language X that
 * appears in at least one STMT_t.lang field.  Callers pass this into
 * polyglot_init so per-language init is skipped when not needed.
 * ══════════════════════════════════════════════════════════════════════════ */
uint32_t polyglot_lang_mask(CODE_t *prog)
{
    uint32_t mask = 0;
    if (!prog) return mask;
    for (STMT_t *s = prog->head; s; s = s->next) {
        if (s->lang >= 0 && s->lang < 32)
            mask |= (1u << s->lang);
    }
    return mask;
}

/* ══════════════════════════════════════════════════════════════════════════
 * polyglot_init — language-selective runtime init  (U-14, FI-8)
 *
 * lang_mask: bitmask of languages present (compute via polyglot_lang_mask).
 * Only inits runtimes for languages actually used — single-lang .sno programs
 * skip Icon and Prolog init entirely.
 *
 * Replaces the three separate init sequences that lived in execute_program,
 * icn_execute_program_unified, and pl_execute_program_unified.
 * ══════════════════════════════════════════════════════════════════════════ */

/* FI-8: counters to verify lazy-init correctness in tests */
int g_fi8_icn_init_count = 0;
int g_fi8_pl_init_count  = 0;

void polyglot_init(CODE_t *prog, uint32_t lang_mask)
{
    if (!prog) return;

    /* ── SNO: label table + DEFINE prescan — always required ────────── */
    label_table_build(prog);
    prescan_defines(prog);
    kw_case = 1;   /* default case-sensitive, matching SPITBOL oracle (&CASE=1) */

    /* ── ICN: proc table — only when Icon or Raku stmts present ─────── */
    if (lang_mask & ((1u << LANG_ICN) | (1u << LANG_RAKU))) {
        g_fi8_icn_init_count++;
        proc_count = 0; global_count = 0;  /* U-23: reset global name bridge */
        frame_depth = 0;
        memset(frame_stack, 0, sizeof frame_stack);
        scan_subj = ""; scan_pos = 1; scan_depth = 0;
        g_icn_root = NULL;
    }

    /* ── PL: pred table + trail — only when Prolog stmts present ────── */
    if (lang_mask & (1u << LANG_PL)) {
        g_fi8_pl_init_count++;
        prolog_atom_init();
        memset(&g_pl_pred_table, 0, sizeof g_pl_pred_table);
        trail_init(&g_pl_trail);
        g_pl_cut_flag = 0;
        g_pl_env      = NULL;
        g_pl_active   = 0;
    }

    /* ── Registry reset  (U-21) ─────────────────────────────────────── */
    memset(&g_registry, 0, sizeof g_registry);
    g_registry.main_mod = -1;

    /* ── Single pass: populate flat tables + registry  (U-14 + U-21) ── */
    int cur_lang = -1;   /* language of the module currently being built */
    int mod_idx  = -1;   /* index into g_registry.mods, or -1 if none open */

    for (STMT_t *s = prog->head; s; s = s->next) {

        /* ── Registry: detect module boundary on lang change  (U-21) ── */
        if (s->lang != cur_lang) {
            /* Close the previous module if one is open */
            if (mod_idx >= 0 && g_registry.mods[mod_idx].first) {
                /* last was set on the previous iteration */
            }
            /* Open a new module slot */
            if (g_registry.nmod < SCRIP_MOD_MAX) {
                cur_lang = s->lang;
                mod_idx  = g_registry.nmod++;
                ScripModule *m = &g_registry.mods[mod_idx];
                m->lang             = s->lang;
                m->name             = NULL;   /* no name-tag syntax yet */
                m->first            = s;
                m->last             = s;
                m->nstmts           = 0;
                m->sno_label_start  = label_count;   /* labels added below */
                m->sno_label_count  = 0;
                m->icn_proc_start   = proc_count;
                m->proc_count   = 0;
            }
        }

        /* Update last/nstmts for the current module */
        if (mod_idx >= 0) {
            g_registry.mods[mod_idx].last = s;
            g_registry.mods[mod_idx].nstmts++;
        }

        if (!s->subject) continue;

        if (s->lang == LANG_ICN || s->lang == LANG_RAKU) {
            /* Icon / Raku: collect AST_FNC procedure definitions.
             * raku_lower produces same AST_FNC shape; share proc_table (RK-6). */
            AST_t *proc = s->subject;
            /* U-23: collect global variable names from AST_GLOBAL decl stmts */
            if (proc->kind == AST_GLOBAL) {
                for (int _gi = 0; _gi < proc->nchildren; _gi++)
                    if (proc->children[_gi] && proc->children[_gi]->sval)
                        global_register(proc->children[_gi]->sval);
            }
            if (proc->kind == AST_RECORD && proc->sval && *proc->sval) {
                /* IC-5: record type declaration — register before main() runs */
                char spec[256]; int pos = 0;
                pos += snprintf(spec+pos, sizeof(spec)-pos, "%s(", proc->sval);
                for (int _ri = 0; _ri < proc->nchildren && pos < (int)sizeof(spec)-2; _ri++) {
                    if (_ri > 0) spec[pos++] = ',';
                    const char *fn2 = (proc->children[_ri] && proc->children[_ri]->sval)
                                      ? proc->children[_ri]->sval : "";
                    pos += snprintf(spec+pos, sizeof(spec)-pos, "%s", fn2);
                }
                if (pos < (int)sizeof(spec)-1) spec[pos++] = ')';
                spec[pos] = '\0';
                icn_record_register(spec);
            }
            if (proc->kind == AST_FNC && proc->sval && *proc->sval) {
                const char *name = proc->sval;

                if (proc_count < PROC_TABLE_MAX) {
                    proc_table[proc_count].name     = name;
                    proc_table[proc_count].proc     = proc;
                    proc_table[proc_count].entry_pc = -1;  /* CH-17a: resolved post-sm_lower */
                    proc_table[proc_count].nparams  = (int)proc->ival;  /* CH-17c */
                    proc_count++;
                    if (mod_idx >= 0) g_registry.mods[mod_idx].proc_count++;
                    /* Detect main module  (U-21) */
                    if (strcmp(name, "main") == 0 && g_registry.main_mod < 0)
                        g_registry.main_mod = mod_idx;
                }
            }
            /* RK-26: evaluate AST_RECORD immediately so class types are registered
             * in sc_dat_types before main() calls raku_new. */
            if (proc->kind == AST_RECORD) {
                interp_eval(proc);
            }
        } else if (s->lang == LANG_PL) {
            /* Prolog: collect AST_CHOICE / AST_CLAUSE predicate definitions */
            AST_t *subj = s->subject;
                if ((subj->kind == AST_CHOICE || subj->kind == AST_CLAUSE) && subj->sval) {
                pl_pred_table_insert(&g_pl_pred_table, subj->sval, subj);
                g_pl_active = 1;
                /* Detect main module  (U-21) */
                if (strcmp(subj->sval, "main/0") == 0 && g_registry.main_mod < 0)
                    g_registry.main_mod = mod_idx;
                    }
        } else if (s->lang == LANG_SNO) {
            /* SNO label range tracking for registry  (U-21) */
            if (mod_idx >= 0 && s->label && *s->label) {
                /* label_table was already built above; count labels in this module */
                g_registry.mods[mod_idx].sno_label_count++;
            }
        }
    }
}


/*============================================================================================================================
 * pl_directive_max_var_slot — walk a lowered Prolog directive subject EXPR
 * and return the largest AST_VAR ival found, or -1 if none.
 *
 * PL-12 (2026-04-30 #3): used by polyglot_execute's LANG_PL branch to size
 * a per-directive cenv. Without this, directives like
 *   :- assertz(test_g(hello)), test_g(G), write(G).
 * passed env=NULL to interp_exec_pl_builtin, which made every AST_VAR read
 * mint a fresh disconnected Term*var via pl_unified_term_from_expr —
 * unify could not thread bindings between conjuncts. The walk uses an
 * iterative explicit stack (no recursion) to avoid blowing C stack on
 * deeply nested goal trees built by the lowerer.
 *============================================================================================================================*/
static int pl_directive_max_var_slot(AST_t *root)
{
    if (!root) return -1;
    int max_slot = -1;
    enum { CAP = 512 };
    AST_t *stk[CAP];
    int top = 0;
    stk[top++] = root;
    while (top > 0) {
        AST_t *e = stk[--top];
        if (!e) continue;
        if (e->kind == AST_VAR && (int)e->ival > max_slot) max_slot = (int)e->ival;
        for (int i = 0; i < e->nchildren && top < CAP; i++)
            if (e->children[i]) stk[top++] = e->children[i];
    }
    return max_slot;
}


/*============================================================================================================================
 * polyglot_execute — OE-7: ONE top-level entry point for all languages.
 *
 * For polyglot programs: calls execute_program (which runs all SNO stmts and
 * the U-23 registry walk for ICN/PL).  Ensures g_lang=1 is set for each ICN
 * module dispatch (the U-23 block in execute_program was missing this).
 *
 * For single-language programs: routes to the appropriate legacy entry point.
 * OE-8 will retire the legacy entry points entirely.
 *============================================================================================================================*/
void polyglot_execute(CODE_t *prog) {
    if (!prog) return;
    if (g_polyglot) {
        execute_program(prog);   /* runs SNO + U-23 ICN/PL registry dispatch */
        return;
    }
    /* Single-language .icn or .pl — detect from first statement's lang field */
    int slang = prog->head ? prog->head->lang : LANG_SNO;
    uint32_t mask = polyglot_lang_mask(prog);
    polyglot_init(prog, mask);
    /* CH-17g-irrun-lowers: if requested, run sm_lower to resolve entry_pcs
     * in proc_table / g_pl_pred_table before dispatch.  Enabled by scrip.c
     * for the --ir-run non-SNO path so proc_table_call sees entry_pc >= 0. */
    if (g_irrun_lowers)
        sm_resolve_irrun_entry_pcs(prog);
    if (slang == LANG_ICN) {
        g_lang = 1;
        for (int i = 0; i < proc_count; i++) {
            if (strcmp(proc_table[i].name, "main") == 0) {
                proc_table_call(i, NULL, 0);   /* CH-17g-call-sites */
                return;
            }
        }
        fprintf(stderr, "icon: no main procedure\n");
    } else if (slang == LANG_PL) {
        g_pl_active = 1;
        /* Execute non-AST_CHOICE/AST_CLAUSE LANG_PL stmts as directives before main/0.
         * PL-12 (2026-04-30 #3): each directive gets a fresh cenv sized to its
         * largest AST_VAR slot. Without this, env=NULL caused
         * pl_unified_term_from_expr to mint a fresh Term*var on every AST_VAR
         * read, so two references to the same logical variable G in
         *   :- assertz(test_g(hello)), test_g(G), write(G).
         * could not unify (the assertz callee bound a fresh var, the write
         * read another fresh var, both disconnected). The directive printed
         * `_G0` instead of `hello`. Walking the lowered EXPR for the max
         * AST_VAR ival and allocating cenv = pl_env_new(max+1) provides one
         * shared env for the whole directive body, so unify can thread
         * bindings the way it does for clause bodies. */
        for (STMT_t *_s = prog->head; _s; _s = _s->next) {
            if (_s->lang != LANG_PL) continue;
            if (!_s->subject) continue;
            if (_s->subject->kind == AST_CHOICE || _s->subject->kind == AST_CLAUSE) continue;
            int _max_slot = pl_directive_max_var_slot(_s->subject);
            Term **_dir_env = (_max_slot >= 0) ? pl_env_new(_max_slot + 1) : NULL;
            Term **_saved   = g_pl_env;
            if (_dir_env) g_pl_env = _dir_env;
            interp_exec_pl_builtin(_s->subject, _dir_env);
            g_pl_env = _saved;
            if (_dir_env) free(_dir_env);
        }
        /* CH-17e: run main/0 via SM expression when entry_pc resolved */
        Pl_PredEntry *_main_pe = pl_pred_entry_lookup("main/0");
        AST_t *main_choice = pl_pred_table_lookup(&g_pl_pred_table, "main/0");
        extern SM_Program *g_current_sm_prog;
        if (_main_pe && _main_pe->entry_pc >= 0 && g_current_sm_prog != NULL) {
            extern DESCR_t sm_call_expression(int);
            sm_call_expression(_main_pe->entry_pc);
        } else if (main_choice) interp_eval(main_choice);
        else fprintf(stderr, "prolog: no main/0 predicate\n");
        g_pl_active = 0;
    } else if (slang == LANG_REB) {
        execute_program(prog);   /* Rebus lowers to SNO-style label/goto chains — FI-1B */
    } else {
        execute_program(prog);   /* SNO single-lang fallback */
    }
}

/*============================================================================================================================
 * parse_scrip_polyglot — parse a fenced polyglot .scrip/.md file into one CODE_t*  (U-13)
 *
 * Scans the source for fenced code blocks:
 *   ```SNOBOL4  ...  ```
 *   ```Icon     ...  ```
 *   ```Prolog   ...  ```
 * Each block is compiled with its own frontend.  All resulting STMT_t chains
 * are appended in source order into one CODE_t*, with st->lang already set
 * by each frontend (U-12).  Unrecognised fence languages are skipped silently.
 *============================================================================================================================*/
CODE_t *parse_scrip_polyglot(const char *src, const char *filename)
{
    CODE_t *result = calloc(1, sizeof(CODE_t));
    if (!result) return NULL;

    const char *p = src;

    while (*p) {
        /* Find next ``` fence open */
        const char *fence = strstr(p, "```");
        if (!fence) break;

        /* Read the language tag on the same line as the fence open */
        const char *tag_start = fence + 3;
        const char *tag_end   = tag_start;
        while (*tag_end && *tag_end != '\n' && *tag_end != '\r') tag_end++;

        /* Trim trailing whitespace from tag */
        while (tag_end > tag_start && (tag_end[-1] == ' ' || tag_end[-1] == '\t')) tag_end--;

        int tag_len = (int)(tag_end - tag_start);

        /* Advance past the fence-open line */
        p = tag_end;
        while (*p && *p != '\n') p++;
        if (*p == '\n') p++;

        /* Detect language (case-insensitive) */
        int lang = -1;
        if      (tag_len == 7 && strncasecmp(tag_start, "SNOBOL4", 7) == 0) lang = LANG_SNO;
        else if (tag_len == 4 && strncasecmp(tag_start, "Icon",    4) == 0) lang = LANG_ICN;
        else if (tag_len == 6 && strncasecmp(tag_start, "Prolog",  6) == 0) lang = LANG_PL;
        else if (tag_len == 4 && strncasecmp(tag_start, "Raku",    4) == 0) lang = LANG_RAKU; /* RK-5 */
        else if (tag_len == 5 && strncasecmp(tag_start, "Scrip",   5) == 0) lang = LANG_SCRIP; /* U-23: shared constants */
        else if (tag_len == 6 && strncasecmp(tag_start, "SCRIP",   5) == 0) lang = LANG_SCRIP;
        else if (tag_len == 5 && strncasecmp(tag_start, "Rebus",   5) == 0) lang = LANG_REB;  /* FI-1B */

        /* Find the matching fence close ``` */
        const char *block_start = p;
        const char *close = strstr(p, "```");
        if (!close) break;   /* unterminated block — stop */

        /* Extract block text */
        int   blen = (int)(close - block_start);
        char *block = malloc(blen + 1);
        if (!block) { p = close + 3; continue; }
        memcpy(block, block_start, blen);
        block[blen] = '\0';

        /* Advance past fence close */
        p = close + 3;
        while (*p && *p != '\n') p++;
        if (*p == '\n') p++;

        if (lang < 0) { free(block); continue; }   /* unknown language — skip */

        /* Compile block with the appropriate frontend */
        CODE_t *sub = NULL;
        if (lang == LANG_SNO || lang == LANG_SCRIP) {
            sub = sno_parse_string(block);
            /* LANG_SCRIP: compiled as SNO — shared constants are SNO assignments.
             * U-23: these execute first (SNO section), writing to NV store
             * which Icon/Prolog can then read via icn_global / nv_get. */
        } else if (lang == LANG_ICN) {
            sub = icon_compile(block, filename);
            /* icon_driver.c sets st->lang=LANG_ICN (U-12) */
        } else if (lang == LANG_PL) {
            sub = prolog_compile(block, filename);
            /* prolog_lower.c sets st->lang=LANG_PL (U-12) */
        } else if (lang == LANG_RAKU) {
            sub = raku_compile(block, filename);
            /* raku_driver.c sets st->lang=LANG_RAKU (RK-5) */
        } else if (lang == LANG_REB) {
            sub = rebus_compile(block, filename);
            /* rebus_compile sets st->lang=LANG_REB (FI-1B) */
        }
        free(block);

        if (!sub || !sub->head) { free(sub); continue; }

        /* Append sub's STMT_t chain to result */
        if (!result->head) {
            result->head = sub->head;
            result->tail = sub->tail;
        } else {
            result->tail->next = sub->head;
            result->tail       = sub->tail;
        }
        result->nstmts += sub->nstmts;
        free(sub);   /* free the wrapper, not the STMT_t chain */
    }

    return result;
}
