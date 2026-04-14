/*
 * scrip.c — unified SCRIP driver
 *
 * One binary, all modes. Frontend inferred from file extension.
 *
 * Usage:
 *   scrip [mode] [bb] [target] [options] <file> [-- program-args...]
 *
 * Execution modes (default: --sm-run):
 *   --ir-run         interpret via IR tree-walk (correctness reference)
 *   --sm-run         interpret SM_Program via dispatch loop  [DEFAULT]
 *   --jit-run        lower SM_Program to x86 bytes -> mmap slab -> jump in
 *
 * Byrd Box pattern mode (default: --bb-driver):
 *   --bb-driver      pattern matching via driver/broker
 *   --bb-live        pattern matching live-wired in exec memory
 *
 * Diagnostic options:
 *   --dump-ir        print IR after frontend
 *   --dump-sm        print SM_Program after lowering
 *   --dump-bb        print BB-GRAPH for each statement
 *   --trace          MONITOR trace output (for two-way diff vs SPITBOL)
 *   --bench          print wall-clock time after execution
 *
 * Frontend inferred from extension:
 *   .sno=SNOBOL4  .icn=Icon  .pl=Prolog  .sc=Snocone  .reb=Rebus  .spt=SPITBOL
 *
 * AUTHORS: Lon Jones Cherryholmes · Claude Sonnet 4.6
 * SPRINT:  M-SCRIP-U0
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ucontext.h>
#include <sys/stat.h>
#include <ctype.h>
#include <setjmp.h>
#include <time.h>
#include <gc.h>

/* ── frontend ─────────────────────────────────────────────────────────── */
#include "../frontend/snobol4/scrip_cc.h"
/* CMPILE.h removed — bison/flex path only (GOAL-REMOVE-CMPILE S-5) */
extern Program *sno_parse(FILE *f, const char *filename);
#include "../frontend/snocone/snocone_driver.h"
#include "../frontend/snocone/snocone_cf.h"
#include "../frontend/prolog/prolog_driver.h"
#include "../frontend/prolog/term.h"            /* Term — needed by Prolog globals block */
#include "../frontend/prolog/prolog_runtime.h"  /* Trail — needed by Prolog globals block */
#include "../frontend/prolog/prolog_atom.h"     /* prolog_atom_name — U-23: 64-bit ptr, must be declared */
#include "../frontend/prolog/prolog_builtin.h"  /* interp_exec_pl_builtin declaration */
#include "../frontend/prolog/pl_broker.h"       /* pl_box_choice, pl_box_* — S-BB-7; pl_exec_goal removed U-11 */
#include "../frontend/icon/icon_driver.h"
#include "../frontend/raku/raku_driver.h"
#include "../frontend/rebus/rebus_lower.h"
#include "../frontend/icon/icon_gen.h"    /* icn_bb_to/by/iterate/suspend, state types — U-17 */
#include "../frontend/icon/icon_lex.h"    /* IcnTkKind — TK_AUG* for E_AUGOP in unified interp */

/* ir_print_node — from src/ir/ir_print.c (linked via Makefile) */
extern void ir_print_node   (const EXPR_t *e, FILE *f);
extern void ir_print_node_nl(const EXPR_t *e, FILE *f);

/* ── runtime ──────────────────────────────────────────────────────────── */
#include "../runtime/x86/snobol4.h"
#include "../runtime/x86/sil_macros.h"   /* SIL macro translations — both RT and SM axes */
#include "../runtime/x86/snobol4_runtime_shim.h"

/* ── SM stack machine (M-SCRIP-U3) ───────────────────────────────────── */
#include "../runtime/x86/sm_lower.h"
#include "../runtime/x86/sm_interp.h"
#include "../runtime/x86/sm_prog.h"
#include "../runtime/x86/bb_build.h"    /* M-BB-LIVE-WIRE: bb_mode_t, g_bb_mode */
#include "../runtime/x86/sm_codegen.h"  /* M-JIT-RUN: sm_codegen, sm_jit_run */
#include "../runtime/x86/sm_image.h"    /* M-JIT-RUN: sm_image_init */

/* pat_at_cursor not exposed in snobol4.h — forward-declare here */
extern DESCR_t pat_at_cursor(const char *varname);

/* stmt_init — stubbed: SM/IR paths init via SNO_INIT_fn() in snobol4.c */
static void stmt_init(void) {}

/* ── eval_code.c ─────────────────────────────────────────────────────── */
extern DESCR_t      eval_expr(const char *src);
extern const char  *exec_code(DESCR_t code_block);

/* ── exec_stmt (from stmt_exec.c) ────────────────────────────────── */
extern int exec_stmt(const char *subj_name,
                          DESCR_t    *subj_var,
                          DESCR_t     pat,
                          DESCR_t    *repl,
                          int         has_repl);

/* subject globals owned by stmt_exec.c — extern here */
extern const char *Σ;
extern int         Ω;
extern int         Δ;

#include "../runtime/interp/icn_runtime.h"
#include "../runtime/interp/pl_runtime.h"
#include "interp.h"   /* FI-6: interp loop extracted to interp.c */

ScripModuleRegistry g_registry;   /* zero-initialised; nmod==0 for single-lang */



/* icn_drive: drive generators embedded in e, re-executing ICN_CUR.body_root each tick.
 * Returns tick count. Mirrors icn_exec_driven in icon_interp.c but uses DESCR_t.
 * OE-2: root parameter removed — body root stored in ICN_CUR.body_root. */
/* ══════════════════════════════════════════════════════════════════════════
 * polyglot_init — one pass, all three runtime tables  (U-14)
 *
 * Replaces the three separate init sequences that lived in execute_program,
 * icn_execute_program_unified, and pl_execute_program_unified.
 * Safe to call for single-language Programs — the lang-tagged tables are
 * simply empty when no statements of that language are present.
 * ══════════════════════════════════════════════════════════════════════════ */
void polyglot_init(Program *prog)
{
    if (!prog) return;

    /* ── SNO: label table + DEFINE prescan ─────────────────────────── */
    label_table_build(prog);
    prescan_defines(prog);

    /* ── ICN: proc table ────────────────────────────────────────────── */
    icn_proc_count = 0; icn_global_count = 0;  /* U-23: reset global name bridge */
    icn_frame_depth = 0;
    memset(icn_frame_stack, 0, sizeof icn_frame_stack);
    icn_scan_subj = NULL; icn_scan_pos = 0; icn_scan_depth = 0;
    g_icn_root = NULL;

    /* ── PL: pred table + trail ─────────────────────────────────────── */
    prolog_atom_init();
    memset(&g_pl_pred_table, 0, sizeof g_pl_pred_table);
    trail_init(&g_pl_trail);
    g_pl_cut_flag = 0;
    g_pl_env      = NULL;
    g_pl_active   = 0;

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
                m->icn_proc_start   = icn_proc_count;
                m->icn_proc_count   = 0;
            }
        }

        /* Update last/nstmts for the current module */
        if (mod_idx >= 0) {
            g_registry.mods[mod_idx].last = s;
            g_registry.mods[mod_idx].nstmts++;
        }

        if (!s->subject) continue;

        if (s->lang == LANG_ICN || s->lang == LANG_RAKU) {
            /* Icon / Raku: collect E_FNC procedure definitions.
             * raku_lower produces same E_FNC shape; share icn_proc_table (RK-6). */
            EXPR_t *proc = s->subject;
            /* U-23: collect global variable names from E_GLOBAL decl stmts */
            if (proc->kind == E_GLOBAL) {
                for (int _gi = 0; _gi < proc->nchildren; _gi++)
                    if (proc->children[_gi] && proc->children[_gi]->sval)
                        icn_global_register(proc->children[_gi]->sval);
            }
            if (proc->kind == E_FNC && proc->sval && *proc->sval) {
                const char *name = proc->sval;

                if (icn_proc_count < ICN_PROC_MAX) {
                    icn_proc_table[icn_proc_count].name = name;
                    icn_proc_table[icn_proc_count].proc = proc;
                    icn_proc_count++;
                    if (mod_idx >= 0) g_registry.mods[mod_idx].icn_proc_count++;
                    /* Detect main module  (U-21) */
                    if (strcmp(name, "main") == 0 && g_registry.main_mod < 0)
                        g_registry.main_mod = mod_idx;
                }
            }
        } else if (s->lang == LANG_PL) {
            /* Prolog: collect E_CHOICE / E_CLAUSE predicate definitions */
            EXPR_t *subj = s->subject;
                if ((subj->kind == E_CHOICE || subj->kind == E_CLAUSE) && subj->sval) {
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
 * polyglot_execute — OE-7: ONE top-level entry point for all languages.
 *
 * For polyglot programs: calls execute_program (which runs all SNO stmts and
 * the U-23 registry walk for ICN/PL).  Ensures g_lang=1 is set for each ICN
 * module dispatch (the U-23 block in execute_program was missing this).
 *
 * For single-language programs: routes to the appropriate legacy entry point.
 * OE-8 will retire the legacy entry points entirely.
 *============================================================================================================================*/
static void polyglot_execute(Program *prog) {
    if (!prog) return;
    if (g_polyglot) {
        execute_program(prog);   /* runs SNO + U-23 ICN/PL registry dispatch */
        return;
    }
    /* Single-language .icn or .pl — detect from first statement's lang field */
    int slang = prog->head ? prog->head->lang : LANG_SNO;
    polyglot_init(prog);
    if (slang == LANG_ICN) {
        g_lang = 1;
        for (int i = 0; i < icn_proc_count; i++) {
            if (strcmp(icn_proc_table[i].name, "main") == 0) {
                icn_call_proc(icn_proc_table[i].proc, NULL, 0);
                return;
            }
        }
        fprintf(stderr, "icon: no main procedure\n");
    } else if (slang == LANG_PL) {
        g_pl_active = 1;
        EXPR_t *main_choice = pl_pred_table_lookup(&g_pl_pred_table, "main/0");
        if (main_choice) interp_eval(main_choice);
        else fprintf(stderr, "prolog: no main/0 predicate\n");
        g_pl_active = 0;
    } else if (slang == LANG_REB) {
        execute_program(prog);   /* Rebus lowers to SNO-style label/goto chains — FI-1B */
    } else {
        execute_program(prog);   /* SNO single-lang fallback */
    }
}

/*============================================================================================================================
 * parse_scrip_polyglot — parse a fenced polyglot .scrip/.md file into one Program*  (U-13)
 *
 * Scans the source for fenced code blocks:
 *   ```SNOBOL4  ...  ```
 *   ```Icon     ...  ```
 *   ```Prolog   ...  ```
 * Each block is compiled with its own frontend.  All resulting STMT_t chains
 * are appended in source order into one Program*, with st->lang already set
 * by each frontend (U-12).  Unrecognised fence languages are skipped silently.
 *============================================================================================================================*/
static Program *parse_scrip_polyglot(const char *src, const char *filename)
{
    Program *result = calloc(1, sizeof(Program));
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
        Program *sub = NULL;
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

int main(int argc, char **argv)
{
    /* ── flag parsing ─────────────────────────────────────────────────── */

    /* Execution modes — mutually exclusive (default: --sm-run) */
    int mode_ir_run        = 0;  /* --ir-run   : interpret via IR tree-walk (correctness ref) */
    int mode_sm_run        = 0;  /* --sm-run   : interpret SM_Program via dispatch loop [DEFAULT] */
    int mode_jit_run       = 0;  /* --jit-run  : SM_Program -> x86 bytes -> mmap slab -> jump in */

    /* Byrd Box pattern mode — independent switch (default: --bb-driver) */
    int bb_driver          = 0;  /* --bb-driver : pattern matching via driver/broker */
    int bb_live            = 0;  /* --bb-live   : live-wired in exec memory */



    /* Diagnostic options */
    int dump_parse         = 0;  /* --dump-parse      */
    int dump_parse_flat    = 0;  /* --dump-parse-flat */
    int dump_ir            = 0;  /* --dump-ir   : print IR after frontend */
    int dump_ir_bison      = 0;  /* --dump-ir-bison : IR via old Bison/Flex parser */
    int dump_sm            = 0;  /* --dump-sm   : print SM_Program after lowering */
    int dump_bb            = 0;  /* --dump-bb   : print BB-GRAPH per statement */
    int opt_trace          = 0;  /* --trace     : MONITOR trace output */
    int opt_bench          = 0;  /* --bench     : print wall-clock time after execution */

    int argi = 1;
    while (argi < argc && argv[argi][0] == '-' && argv[argi][1] == '-') {
        /* execution modes */
        if      (strcmp(argv[argi], "--ir-run")        == 0) { mode_ir_run        = 1; argi++; }
        else if (strcmp(argv[argi], "--sm-run")        == 0) { mode_sm_run        = 1; argi++; }
        else if (strcmp(argv[argi], "--jit-run")       == 0) { mode_jit_run       = 1; argi++; }
        /* BB pattern mode */
        else if (strcmp(argv[argi], "--bb-driver")     == 0) { bb_driver          = 1; argi++; }
        else if (strcmp(argv[argi], "--bb-live")       == 0) { bb_live            = 1; argi++; }
        /* diagnostic */
        else if (strcmp(argv[argi], "--dump-parse")      == 0) { dump_parse      = 1; argi++; }
        else if (strcmp(argv[argi], "--dump-parse-flat") == 0) { dump_parse_flat = 1; argi++; }
        else if (strcmp(argv[argi], "--dump-ir")         == 0) { dump_ir         = 1; argi++; }
        else if (strcmp(argv[argi], "--dump-ir-bison")   == 0) { dump_ir_bison   = 1; argi++; }
        else if (strcmp(argv[argi], "--dump-sm")         == 0) { dump_sm         = 1; argi++; }
        else if (strcmp(argv[argi], "--dump-bb")         == 0) { dump_bb         = 1; argi++; }
        else if (strcmp(argv[argi], "--trace")           == 0) { opt_trace       = 1; argi++; }
        else if (strcmp(argv[argi], "--bench")           == 0) { opt_bench       = 1; argi++; }
        else break;
    }

    /* Default execution mode: --sm-run */
    if (!mode_ir_run && !mode_sm_run && !mode_jit_run)
        mode_sm_run = 1;

    /* Default BB mode: --bb-driver unless --bb-live explicitly set */
    if (!bb_driver && !bb_live) bb_driver = 1;

    /* Suppress unused warning for bb_driver (not yet wired to stmt_exec.c guard) */
    (void)bb_driver;

    /* M-BB-LIVE-WIRE: propagate BB mode to stmt_exec.c */
    if (bb_live) g_bb_mode = BB_MODE_LIVE;

    if (argi >= argc) {
        fprintf(stderr,
            "usage: scrip [mode] [bb] [options] <file> [-- program-args...]\n"
            "\n"
            "Execution modes (default: --sm-run):\n"
            "  --ir-run         interpret via IR tree-walk (correctness reference)\n"
            "  --sm-run         interpret SM_Program via dispatch loop  [DEFAULT]\n"
            "  --jit-run        SM_Program -> x86 bytes -> mmap slab -> jump in\n"
            "\n"
            "Byrd Box pattern mode (default: --bb-driver):\n"
            "  --bb-driver      pattern matching via driver/broker\n"
            "  --bb-live        live-wired BB blobs in exec memory (requires M-DYN-B* blobs)\n"
            "\n"
            "Diagnostic options:\n"
            "  --dump-ir        print IR after frontend\n"
            "  --dump-sm        print SM_Program after lowering\n"
            "  --dump-bb        print BB-GRAPH for each statement\n"
            "  --trace          MONITOR trace output (diff vs SPITBOL)\n"
            "  --bench          print wall-clock time after execution\n"
            "  --dump-parse     dump CMPILE parse tree\n"
            "  --dump-parse-flat  dump CMPILE parse tree (one line)\n"
            "  --dump-ir-bison  dump IR via old Bison/Flex parser\n"
            "\n"
            "Frontend inferred from file extension:\n"
            "  .sno=SNOBOL4  .spt=SPITBOL  .icn=Icon  .pl=Prolog  .sc=Snocone  .reb=Rebus\n"
        );
        return 1;
    }
    const char *input_path = argv[argi];

    /* Set up include search dirs before parsing:
     * 1. Directory of the input file itself
     * 2. SNO_LIB env var (corpus root for 'lib/xxx.sno' includes)
     * 3. Current working directory */
    {
        extern void sno_add_include_dir(const char *d);
        /* dir of input file */
        char dirbuf[4096];
        strncpy(dirbuf, input_path, sizeof dirbuf - 1);
        dirbuf[sizeof dirbuf - 1] = '\0';
        char *sl = strrchr(dirbuf, '/');
        if (sl) { *sl = '\0'; sno_add_include_dir(dirbuf); }
        else     { sno_add_include_dir("."); }
        /* SNO_LIB env var */
        const char *sno_lib = getenv("SNO_LIB");
        if (sno_lib && *sno_lib) sno_add_include_dir(sno_lib);
        /* Auto-detect corpus root: walk up from file dir looking for lib/ subdir.
         * Handles 'lib/math.sno' style includes without requiring SNO_LIB. */
        {
            char walk[4096];
            strncpy(walk, input_path, sizeof walk - 1);
            walk[sizeof walk - 1] = '\0';
            char *p = strrchr(walk, '/');
            while (p) {
                *p = '\0';
                char probe[4096];
                snprintf(probe, sizeof probe, "%s/lib", walk);
                struct stat st;
                if (stat(probe, &st) == 0 && S_ISDIR(st.st_mode)) {
                    sno_add_include_dir(walk);
                    break;
                }
                p = strrchr(walk, '/');
            }
        }
        /* cwd fallback */
        sno_add_include_dir(".");
    }

    FILE *f = fopen(input_path, "r");
    if (!f) {
        fprintf(stderr, "scrip: cannot open '%s'\n", input_path);
        return 1;
    }

    struct timespec _t0, _t1, _t2, _t3;
    if (opt_bench) clock_gettime(CLOCK_MONOTONIC, &_t0);

    /* ── parse ──────────────────────────────────────────────────────────────
     * --dump-parse / --dump-parse-flat / --dump-ir  →  CMPILE (hand-written)
     * everything else (--ir-run, --sm-run, --dump-ir-bison)  →  sno_parse (Bison/Flex)
     * sno_parse is the proven path: PASS=190 baseline.
     * .sc extension  →  snocone_compile() (lex+parse+lower in one call) */
    /* detect Snocone frontend by file extension */
    int lang_snocone = 0;
    { const char *dot = strrchr(input_path, '.'); if (dot && strcmp(dot, ".sc") == 0) lang_snocone = 1; }
    int lang_prolog = 0;
    { const char *dot = strrchr(input_path, '.'); if (dot && strcmp(dot, ".pl") == 0) lang_prolog = 1; }
    int lang_icon = 0;
    { const char *dot = strrchr(input_path, '.'); if (dot && strcmp(dot, ".icn") == 0) lang_icon = 1; }
    int lang_raku = 0;
    { const char *dot = strrchr(input_path, '.'); if (dot && strcmp(dot, ".raku") == 0) lang_raku = 1; }
    int lang_rebus = 0;
    { const char *dot = strrchr(input_path, '.'); if (dot && strcmp(dot, ".reb") == 0) lang_rebus = 1; }
    int lang_polyglot = 0;  /* U-13: .scrip or .md → fenced polyglot */
    { const char *dot = strrchr(input_path, '.');
      if (dot && (strcmp(dot, ".scrip") == 0 || strcmp(dot, ".md") == 0)) lang_polyglot = 1; }

    Program *prog = NULL;
    if (lang_polyglot) { g_polyglot = 1;
        /* U-13: read whole file, split fenced blocks, compile each, merge into one Program* */
        fseek(f, 0, SEEK_END); long flen = ftell(f); rewind(f);
        char *src = malloc(flen + 1);
        if (!src) { fprintf(stderr, "scrip: out of memory\n"); return 1; }
        fread(src, 1, flen, f); src[flen] = '\0'; fclose(f);
        if (opt_bench) clock_gettime(CLOCK_MONOTONIC, &_t1);
        prog = parse_scrip_polyglot(src, input_path);
        free(src);
    } else if (lang_snocone || lang_prolog || lang_icon || lang_raku || lang_rebus) {
        /* Read whole file into buffer */
        fseek(f, 0, SEEK_END); long flen = ftell(f); rewind(f);
        char *src = malloc(flen + 1);
        if (!src) { fprintf(stderr, "scrip: out of memory\n"); return 1; }
        fread(src, 1, flen, f); src[flen] = '\0'; fclose(f);
        if (opt_bench) clock_gettime(CLOCK_MONOTONIC, &_t1);
        prog = lang_raku   ? raku_compile(src, input_path)
             : lang_prolog ? prolog_compile(src, input_path)
             : lang_icon   ? icon_compile(src, input_path)
             : lang_rebus  ? rebus_compile(src, input_path)
             :               snocone_cf_compile(src, input_path);
        free(src);
    } else if (dump_parse || dump_parse_flat || dump_ir) {
        /* --dump-parse / --dump-parse-flat / --dump-ir: bison path (CMPILE removed) */
        fclose(f);
        if (opt_bench) clock_gettime(CLOCK_MONOTONIC, &_t1);
        FILE *f3 = fopen(input_path, "r");
        if (!f3) { fprintf(stderr, "scrip: cannot re-open '%s'\n", input_path); return 1; }
        Program *dprog = sno_parse(f3, input_path);
        fclose(f3);
        ir_dump_program(dprog, stdout);
        return 0;
    } else {
        fclose(f);
        if (opt_bench) clock_gettime(CLOCK_MONOTONIC, &_t1);
        FILE *f2 = fopen(input_path, "r");
        if (!f2) { fprintf(stderr, "scrip: cannot re-open '%s'\n", input_path); return 1; }
        prog = sno_parse(f2, input_path);
        fclose(f2);
        if (dump_ir_bison) { ir_dump_program(prog, stdout); return 0; }
    }

    if (opt_bench) clock_gettime(CLOCK_MONOTONIC, &_t2);

    if (!prog || !prog->head) {
        fprintf(stderr, "scrip: parse failed for '%s'\n", input_path);
        return 1;
    }

    /* Initialise binary box pool (M-DYN-B1) */
    {
        extern void bb_pool_init(void);
        bb_pool_init();
    }

    /* Initialise all builtins (GT, LT, SIZE, DATATYPE, etc.) registered in snobol4.c */
    extern void SNO_INIT_fn(void);
    SNO_INIT_fn();

    stmt_init();
    g_prog = prog;

    /* S-10 fix: register scrip.c-only builtins so APPLY_fn can dispatch them
     * at match time (used by *IDENT(x) / *DIFFER(x) in pattern position). */
    register_fn("IDENT",  _builtin_IDENT,  1, 2);
    register_fn("DIFFER", _builtin_DIFFER, 1, 2);
    register_fn("EVAL",   _builtin_EVAL,   1, 1);
    register_fn("CODE",   _builtin_CODE,   1, 1);
    register_fn("DATA",   _builtin_DATA,   1, 1);
    register_fn("print",  _builtin_print,  0, 99);

    /* Wire user-function dispatch hook (wrapper defined above main) */
    extern DESCR_t (*g_user_call_hook)(const char *, DESCR_t *, int);
    g_user_call_hook = _usercall_hook;

    /* Wire LABEL() predicate hook */
    {
        extern void sno_set_label_exists_hook(int (*fn)(const char *));
        sno_set_label_exists_hook(_label_exists_fn);
    }

    /* Wire DT_P eval hook: EVAL(*func(args)) runs pattern against empty subject */
    {
        extern DESCR_t (*g_eval_pat_hook)(DESCR_t pat);
        g_eval_pat_hook = _eval_pat_impl_fn;
    }

    /* Wire DT_S eval hook: EVAL(string) containing complex operators
     * (E_DEFER, cursor-assign) routes through interp_eval_pat. */
    {
        extern DESCR_t (*g_eval_str_hook)(const char *s);
        g_eval_str_hook = _eval_str_impl_fn;
    }

    /* ── Set diagnostic globals ─────────────────────────────────────── */
    g_opt_trace   = opt_trace;
    g_opt_dump_bb = dump_bb;

    /* ── --dump-sm with --ir-run: lower-only, no execution ─────────── */
    if (dump_sm && !mode_sm_run) {
        label_table_build(prog);
        prescan_defines(prog);
        SM_Program *sm0 = sm_lower(prog);
        if (!sm0) { fprintf(stderr, "scrip: sm_lower failed\n"); return 1; }
        sm_prog_print(sm0, stdout);
        sm_prog_free(sm0);
        return 0;
    }

    if (lang_polyglot) {
        polyglot_execute(prog);   /* OE-7: polyglot takes priority — SM layer not yet polyglot-aware (OE-9/10/11) */
    } else if (mode_sm_run) {
        /* --sm-run: SM-LOWER path — IR → SM_Program → sm_interp_run.
         * Must mirror execute_program setup: build label table and register
         * DEFINE'd functions so call_user_function can find bodies via
         * g_user_call_hook → _usercall_hook → label_lookup. */
        label_table_build(prog);
        prescan_defines(prog);
        g_sno_err_active = 1;   /* arm so sno_runtime_error longjmps safely */
        SM_Program *sm = sm_lower(prog);
        if (!sm) {
            fprintf(stderr, "scrip: sm_lower failed\n");
            return 1;
        }
        /* ── --dump-sm: print SM_Program and exit ───────────────────── */
        if (dump_sm) {
            sm_prog_print(sm, stdout);
            sm_prog_free(sm);
            return 0;
        }
        SM_State st;
        sm_state_init(&st);
        /* Arm g_sno_err_jmp: sno_runtime_error longjmps here on error.
         * We treat each error as statement failure: mark last_ok=0, advance pc,
         * and re-enter the interp loop.  This mirrors execute_program's per-stmt
         * setjmp pattern and prevents longjmp into an uninitialized jmp_buf. */
        int hybrid_err;
        while (1) {
            hybrid_err = setjmp(g_sno_err_jmp);
            if (hybrid_err != 0) {
                /* runtime error fired mid-statement: mark fail, advance past
                 * the current instruction and continue */
                st.last_ok = 0;
                st.sp = 0;  /* reset value stack — state is undefined after error */
                if (st.pc < sm->count) st.pc++;  /* skip offending instruction */
                /* drain to next SM_STNO boundary so we resume cleanly */
                while (st.pc < sm->count &&
                       sm->instrs[st.pc].op != SM_STNO &&
                       sm->instrs[st.pc].op != SM_HALT)
                    st.pc++;
            }
            int rc = sm_interp_run(sm, &st);
            if (rc == 0 || rc < -1) break;  /* halted or fatal */
            if (st.pc >= sm->count) break;
        }
        sm_prog_free(sm);
    } else if (mode_jit_run) {
        /* --jit-run: SM-LOWER → sm_codegen → sm_jit_run.
         * Same preamble as --sm-run; codegen replaces sm_interp_run. */
        label_table_build(prog);
        prescan_defines(prog);
        g_sno_err_active = 1;
        SM_Program *sm = sm_lower(prog);
        if (!sm) { fprintf(stderr, "scrip: sm_lower failed\n"); return 1; }
        if (dump_sm) { sm_prog_print(sm, stdout); sm_prog_free(sm); return 0; }
        if (sm_image_init() != 0) {
            fprintf(stderr, "scrip: sm_image_init failed\n");
            sm_prog_free(sm); return 1;
        }
        if (sm_codegen(sm) != 0) {
            fprintf(stderr, "scrip: sm_codegen failed\n");
            sm_prog_free(sm); return 1;
        }
        SM_State st;
        sm_state_init(&st);
        int hybrid_err;
        while (1) {
            hybrid_err = setjmp(g_sno_err_jmp);
            if (hybrid_err != 0) {
                st.last_ok = 0;
                st.sp = 0;
                if (st.pc < sm->count) st.pc++;
                while (st.pc < sm->count &&
                       sm->instrs[st.pc].op != SM_STNO &&
                       sm->instrs[st.pc].op != SM_HALT)
                    st.pc++;
            }
            int rc = sm_jit_run(sm, &st);
            if (rc == 0 || rc < -1) break;
            if (st.pc >= sm->count) break;
        }
        sm_prog_free(sm);
    } else if (lang_prolog) {
        polyglot_execute(prog);
    } else if (lang_icon) {
        polyglot_execute(prog);
    } else if (lang_rebus) {
        polyglot_execute(prog);
    } else {
        execute_program(prog);
    }
    if (opt_bench) {
        clock_gettime(CLOCK_MONOTONIC, &_t3);
        double parse_ms = (_t1.tv_sec - _t0.tv_sec)*1e3 + (_t1.tv_nsec - _t0.tv_nsec)/1e6;
        double lower_ms = (_t2.tv_sec - _t1.tv_sec)*1e3 + (_t2.tv_nsec - _t1.tv_nsec)/1e6;
        double exec_ms  = (_t3.tv_sec - _t2.tv_sec)*1e3 + (_t3.tv_nsec - _t2.tv_nsec)/1e6;
        fprintf(stderr, "BENCH parse=%.2fms lower=%.2fms exec=%.2fms total=%.2fms\n",
                parse_ms, lower_ms, exec_ms, parse_ms + lower_ms + exec_ms);
    }
    /* M-DYN-B13: BINARY_AUDIT=1 is canonical; SNO_BINARY_BOXES=1 is legacy alias */
    if (getenv("BINARY_AUDIT") || getenv("SNO_BINARY_BOXES")) {
        extern void bin_audit_print(void);
        bin_audit_print();
    }
    return 0;
}
