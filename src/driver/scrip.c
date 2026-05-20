#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <ctype.h>
#include <setjmp.h>
#include <time.h>
#include <unistd.h>
#include <gc.h>
#include "../frontend/snobol4/scrip_cc.h"
#include "../frontend/snocone/snocone_driver.h"
#include "../frontend/prolog/prolog_driver.h"
#include "../frontend/prolog/term.h"
#include "../frontend/prolog/prolog_runtime.h"
#include "../frontend/prolog/prolog_atom.h"
#include "../frontend/prolog/prolog_builtin.h"
#include "../frontend/prolog/pl_broker.h"       /* pl_box_choice, pl_box_* — S-BB-7; pl_exec_goal removed U-11 */
#include "../frontend/icon/icon_driver.h"
#include "../frontend/raku/raku_driver.h"
#include "../frontend/rebus/rebus_lower.h"
#include "../frontend/icon/icon_gen.h"
#include "../frontend/icon/icon_lex.h"    /* IcnTkKind — TK_AUG* for TT_AUGOP in unified interp */
/*----------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------*/
extern void ir_print_node   (const tree_t *e, FILE *f);
/*----------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------*/
extern void ir_set_print_width(int w);   /* --dump-width N: inline/multiline threshold */
/*----------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------*/
extern void ir_print_node_nl(const tree_t *e, FILE *f);
/*----------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------*/
#include "snobol4.h"
#include "sil_macros.h"
#include "snobol4_runtime_shim.h"
#include "lower.h"
#include "sm_interp.h"
#include "SM.h"
#include "bb_build.h"
#include "sm_jit_interp.h"
#include "emit_sm.h"
#include "emit.h"
#include "emit_bb.h"
#include "scrip_sm.h"
#include "sync_monitor.h"
#include "sm_image.h"
/*----------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------*/
extern DESCR_t pat_at_cursor(const char *varname);
/*----------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------*/
static void stmt_init(void) {}
/*----------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------*/
extern DESCR_t      eval_expr(const char *src);
/*----------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------*/
extern const char  *exec_code(DESCR_t code_block);
/*----------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------*/
extern int exec_stmt(const char *subj_name,
                          DESCR_t    *subj_var,
                          DESCR_t     pat,
                          DESCR_t    *repl,
                          int         has_repl);
extern const char *Σ;
extern int         Ω;
extern int         Δ;
#include "../runtime/interp/icn_runtime.h"
#include "../runtime/interp/pl_runtime.h"
#include "driver/polyglot.h"
/*----------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------*/
int main(int argc, char **argv)
{
    int mode_interp        = 0;
    int mode_run           = 0;
    int mode_compile       = 0;
    int mode_monitor       = 0;
    int opt_bb_format       = 0;
    int bb_driver          = 0;
    int bb_live            = 0;
    int dump_ast           = 0;
    int dump_ast_bison     = 0;
    int dump_sm            = 0;
    int dump_bb            = 0;
    int dump_sno           = 0;
    int opt_trace          = 0;
    int opt_bench          = 0;
    const char * target_name = NULL;
    int argi = 1;
    while (argi < argc && argv[argi][0] == '-' && argv[argi][1] == '-') {
        if      (strcmp(argv[argi], "--interp")        == 0) { mode_interp        = 1; argi++; }
        else if (strcmp(argv[argi], "--run")           == 0) { mode_run           = 1; argi++; }
        else if (strcmp(argv[argi], "--compile")       == 0) { mode_compile       = 1; if (!target_name) target_name = "x86"; argi++; }
        else if (strcmp(argv[argi], "--monitor")       == 0) { mode_monitor       = 1; argi++; }
        else if (strncmp(argv[argi], "--target=", 9)   == 0) { target_name = argv[argi] + 9; mode_compile = 1; argi++; }
        else if (strcmp(argv[argi], "--bb-format")     == 0) { opt_bb_format      = 1; argi++; }
        else if (strcmp(argv[argi], "--bb=brokered")   == 0) { bb_driver          = 1; argi++; }
        else if (strcmp(argv[argi], "--bb=wired")      == 0) { bb_live            = 1; argi++; }
        else if (strcmp(argv[argi], "--dump-ast")      == 0) { dump_ast           = 1; argi++; }
        else if (strcmp(argv[argi], "--dump-ast-bison") == 0) { dump_ast_bison    = 1; argi++; }
        else if (strcmp(argv[argi], "--dump-sm")       == 0) { dump_sm            = 1; argi++; }
        else if (strcmp(argv[argi], "--dump-bb")       == 0) { dump_bb            = 1; argi++; }
        else if (strcmp(argv[argi], "--dump-sno")      == 0) { dump_sno           = 1; argi++; }
        else if (strcmp(argv[argi], "--dump-width")    == 0) {
            if (argi + 1 < argc) { ir_set_print_width(atoi(argv[++argi])); argi++; }
        }
        else if (strcmp(argv[argi], "--trace")         == 0) { opt_trace          = 1; argi++; }
        else if (strcmp(argv[argi], "--bench")         == 0) { opt_bench          = 1; argi++; }
        else if (strcmp(argv[argi], "--case-sensitive") == 0) { argi++; }
        else if (strcmp(argv[argi], "--fold-case")     == 0) {
            fprintf(stderr, "scrip: --fold-case is no longer supported; SCRIP is case-sensitive only\n");
            return 1;
        }
        else break;
    }
    int mode_compile_x86 = (mode_compile && target_name && strcmp(target_name, "x86") == 0);
    if (mode_compile_x86 && (mode_interp || mode_run || mode_monitor)) {
        fprintf(stderr,
            "scrip: --compile (x86) is mutually exclusive with "
            "--interp / --run / --monitor\n");
        return 1;
    }
    if (!mode_interp && !mode_run && !mode_monitor && !mode_compile)
        mode_run = 1;
    /* BB knob is exposed only under --interp.
       --run and --compile force wired BBs. Rationale: brokered BB execution
       requires a runtime broker structure that's natural for the interpreter
       but absent / not supported in the JIT-emit / asm-emit paths. */
    if (bb_driver && (mode_run || mode_compile)) {
        fprintf(stderr,
            "scrip: --bb=brokered is only valid under --interp; "
            "--run and --compile force --bb=wired\n");
        return 1;
    }
    if (bb_driver && bb_live) {
        fprintf(stderr,
            "scrip: --bb=brokered and --bb=wired are mutually exclusive\n");
        return 1;
    }
    /* Default-resolution:
       Under --interp: default is --bb=brokered (the historic default).
       Under --run / --compile: forced to --bb=wired (per the guard above).
       Under --monitor: legacy default-to-wired preserved. */
    if (!bb_driver && !bb_live) {
        if (mode_interp) bb_driver = 1;
        else             bb_live   = 1;
    }
    if (bb_live)   g_bb_mode = BB_MODE_LIVE;
    if (bb_driver) g_bb_mode = BB_MODE_BROKERED;
    if (argi >= argc) {
        fprintf(stderr,
            "usage: scrip [mode] [bb] [options] <file> [-- program-args...]\n"
            "\n"
            "Execution modes (default: --run):\n"
            "  --interp         interpret SM_sequence_t via dispatch loop\n"
            "  --run            SM_sequence_t -> x86 bytes -> mmap slab -> jump in  [DEFAULT]\n"
            "  --compile        emit standalone x86-64 asm to stdout (links libscrip_rt.so)\n"
            "  --target=ARCH    emit code for the named backend (x86, jvm, js, wasm); implies --compile\n"
            "  --monitor        in-process sync comparator (AST vs SM vs JIT)\n"
            "\n"
            "Byrd Box mode (under --interp; --run and --compile force wired):\n"
            "  --bb=brokered    pattern matching via driver/broker  [DEFAULT under --interp]\n"
            "  --bb=wired       live-wired BB blobs in exec memory (requires M-DYN-B* blobs)\n"
            "\n"
            "Diagnostic options:\n"
            "  --dump-ast       print AST after frontend\n"
            "  --dump-sm        print SM_sequence_t after lowering\n"
            "  --dump-bb        print BB-GRAPH for each statement\n"
            "  --dump-sno       transpile AST to portable SNOBOL4 source (GOAL-PARSER-SC-TRANSPILE.md SCT-1)\n"
            "  --trace          MONITOR trace output (diff vs CSNOBOL4)\n"
            "  --bench          print wall-clock time after execution\n"
            "  --dump-ast-bison dump AST via old Bison/Flex parser\n"
            "\n"
            "Frontend inferred from file extension:\n"
            "  .sno=SNOBOL4  .icn=Icon  .pl=Prolog  .sc=Snocone  .reb=Rebus\n"
        );
        return 1;
    }
    extern void sno_add_include_dir(const char *d);
    struct timespec _t0, _t1, _t2, _t3;
    if (opt_bench) clock_gettime(CLOCK_MONOTONIC, &_t0);
    int first_file_argi = argi; (void)first_file_argi;
    int has_non_sno = 0;
    for (int fi = argi; fi < argc; fi++) {
        const char *d = strrchr(argv[fi], '.');
        if (d && (strcmp(d,".pl")==0 || strcmp(d,".icn")==0 ||
                  strcmp(d,".raku")==0 || strcmp(d,".reb")==0 ||
                  strcmp(d,".sc")==0 || strcmp(d,".scrip")==0 || strcmp(d,".md")==0))
            has_non_sno = 1;
    }
    CODE_t *sub = NULL;
    tree_t  *ast_prog = NULL;
    #define MERGE_AST(sub_ast) do { \
        if (sub_ast) { \
            if (!ast_prog) { ast_prog = sub_ast; } \
            else { \
                if (ast_prog->n > 0) { \
                    tree_t *_last = ast_prog->c[ast_prog->n-1]; \
                    if (_last && _last->t == TT_END) ast_prog->n--; \
                } \
                for (int _i = 0; _i < (sub_ast)->n; _i++) { \
                    ast_push(ast_prog, (sub_ast)->c[_i]); \
                } \
                if ((sub_ast)->c) free((char *)(sub_ast)->c - sizeof(size_t)); free(sub_ast); \
            } \
        } \
    } while(0)
    for (; argi < argc; argi++) {
        const char *input_path = argv[argi];
        {
            char dirbuf[4096];
            strncpy(dirbuf, input_path, sizeof dirbuf - 1);
            dirbuf[sizeof dirbuf - 1] = '\0';
            char *sl = strrchr(dirbuf, '/');
            if (sl) { *sl = '\0'; sno_add_include_dir(dirbuf); }
            else     { sno_add_include_dir("."); }
            const char *sno_lib = getenv("SNO_LIB");
            if (sno_lib && *sno_lib) sno_add_include_dir(sno_lib);
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
            sno_add_include_dir(".");
        }
        const char *dot = strrchr(input_path, '.');
        int lang_snocone  = dot && strcmp(dot, ".sc")   == 0;
        int lang_prolog   = dot && strcmp(dot, ".pl")   == 0;
        int lang_icon     = dot && strcmp(dot, ".icn")  == 0;
        int lang_raku     = dot && strcmp(dot, ".raku") == 0;
        int lang_rebus    = dot && strcmp(dot, ".reb")  == 0;
        int lang_polyglot = dot && (strcmp(dot, ".scrip") == 0 || strcmp(dot, ".md") == 0);
        sub = NULL;
        if (lang_polyglot) {
            g_polyglot = 1;
            FILE *f = fopen(input_path, "r");
            if (!f) { fprintf(stderr, "scrip: cannot open '%s'\n", input_path); return 1; }
            fseek(f, 0, SEEK_END); long flen = ftell(f); rewind(f);
            char *src = malloc(flen + 1);
            if (!src) { fprintf(stderr, "scrip: out of memory\n"); return 1; }
            fread(src, 1, flen, f); src[flen] = '\0'; fclose(f);
            tree_t *sub_ast = parse_scrip_polyglot(src, input_path);
            free(src);
            MERGE_AST(sub_ast);
        } else if (lang_snocone || lang_prolog || lang_icon || lang_raku || lang_rebus) {
            FILE *f = fopen(input_path, "r");
            if (!f) { fprintf(stderr, "scrip: cannot open '%s'\n", input_path); return 1; }
            fseek(f, 0, SEEK_END); long flen = ftell(f); rewind(f);
            char *src = malloc(flen + 1);
            if (!src) { fprintf(stderr, "scrip: out of memory\n"); return 1; }
            fread(src, 1, flen, f); src[flen] = '\0'; fclose(f);
            tree_t *sub_ast = NULL;
            if (lang_icon)         icon_compile(src, input_path, &sub_ast);
            else if (lang_raku)    raku_compile(src, input_path, &sub_ast);
            else if (lang_prolog)  prolog_compile(src, input_path, &sub_ast);
            else if (lang_rebus)   rebus_compile(src, input_path, &sub_ast);
            else                   snocone_compile(src, input_path, &sub_ast);
            free(src);
            if (dump_ast && sub_ast) {
                /* PST-RB-5i: extend --dump-ast to all non-SNOBOL4 frontends
                   (was lang_snocone only; rebus/icon/prolog/raku now print
                   their tree_t and exit, matching the snocone behavior so
                   parser_*.sc validation has a reference). */
                ir_dump_program(sub_ast, stdout); return 0;
            }
            /* SCT-1c (2026-05-17, was SCT-1): --dump-sno previously returned
             * after the first file.  Now it merges into ast_prog so the
             * runtime prelude (global.sc + tree.sc + stack.sc + counter.sc +
             * ShiftReduce.sc + semantic.sc + tdump.sc) can be passed ahead
             * of the parser_*.sc file.  Actual dump fires after the multi-
             * file loop completes (search for "dump_sno &&" below). */
            MERGE_AST(sub_ast);
        } else if (dump_ast) {
            FILE *f = fopen(input_path, "r");
            if (!f) { fprintf(stderr, "scrip: cannot open '%s'\n", input_path); return 1; }
            if (opt_bench) clock_gettime(CLOCK_MONOTONIC, &_t1);
            tree_t *sub_ast = sno_parse_ast(f, input_path, NULL);
            fclose(f);
            ir_dump_program(sub_ast, stdout);
            return 0;
        } else if (dump_sno) {
            /* SCT-1c: SNOBOL4 input case.  Accumulate (mirror of the
             * snocone/rebus/etc. case above) — actual dump fires after the
             * multi-file loop completes. */
            FILE *f = fopen(input_path, "r");
            if (!f) { fprintf(stderr, "scrip: cannot open '%s'\n", input_path); return 1; }
            if (opt_bench) clock_gettime(CLOCK_MONOTONIC, &_t1);
            tree_t *sub_ast = sno_parse_ast(f, input_path, NULL);
            fclose(f);
            MERGE_AST(sub_ast);
        } else {
            FILE *f = fopen(input_path, "r");
            if (!f) { fprintf(stderr, "scrip: cannot open '%s'\n", input_path); return 1; }
            tree_t *sub_ast = sno_parse_ast(f, input_path, dump_ast_bison ? &sub : NULL);
            fclose(f);
            if (dump_ast_bison) { ir_dump_program(sub, stdout); return 0; }
            MERGE_AST(sub_ast);
        }
        if (!ast_prog) {
            fprintf(stderr, "scrip: parse failed for '%s'\n", input_path);
            return 1;
        }
    }
    if (opt_bench) clock_gettime(CLOCK_MONOTONIC, &_t1);
    const char *input_path = argv[argc - 1];
    if (opt_bench) clock_gettime(CLOCK_MONOTONIC, &_t2);
    if (!ast_prog) {
        fprintf(stderr, "scrip: parse failed for '%s'\n", input_path);
        return 1;
    }
    {
        extern void bb_pool_init(void);
        bb_pool_init();
    }
    extern void SNO_INIT_fn(void);
    SNO_INIT_fn();
    stmt_init();
    register_fn("IDENT",  _builtin_IDENT,  1, 2);
    register_fn("DIFFER", _builtin_DIFFER, 1, 2);
    register_fn("EVAL",   _builtin_EVAL,   1, 1);
    register_fn("CODE",   _builtin_CODE,   1, 1);
    register_fn("DATA",   _builtin_DATA,   1, 1);
    register_fn("print",  _builtin_print,  0, 99);
    extern DESCR_t (*g_user_call_hook)(const char *, DESCR_t *, int);
    g_user_call_hook = _usercall_hook;
    {
        extern void sno_set_label_exists_hook(int (*fn)(const char *));
        sno_set_label_exists_hook(_label_exists_fn);
    }
    {
        extern DESCR_t (*g_eval_pat_hook)(DESCR_t pat);
        g_eval_pat_hook = _eval_pat_impl_fn;
    }
    {
        extern DESCR_t (*g_eval_str_hook)(const char *s);
        g_eval_str_hook = _eval_str_impl_fn;
    }
    g_opt_trace   = opt_trace;
    g_opt_dump_bb = dump_bb;
    if (dump_sm && !mode_interp) {
        label_table_build(ast_prog);
        prescan_defines(ast_prog);
        SM_sequence_t *sm0 = lower(ast_prog);
        if (!sm0) { fprintf(stderr, "scrip: sm_lower failed\n"); return 1; }
        sm_seq_print(sm0, stdout);
        SM_seq_free(sm0);
        return 0;
    }
    if (dump_sno) {
        /* SCT-1c: emit the accumulated multi-file AST as portable SNOBOL4.
         * Mirrors the dump_sm shape above — runs at end of the multi-file
         * loop, after MERGE_AST has built ast_prog from every input.
         *
         * Use case: `scrip --dump-sno global.sc tree.sc ... parser_*.sc > p.sno`
         * yields a single self-contained .sno that SPITBOL/SCRIP can run
         * without external dependencies. */
        extern int tree_to_sno(const tree_t *ast, FILE *out);
        tree_to_sno(ast_prog, stdout);
        return 0;
    }
    if (mode_compile_x86) {
        g_emit_inline = 0;
        g_bb_emit_format  = opt_bb_format;
        SM_sequence_t *sm = sm_preamble(ast_prog);
        if (!sm) return 1;
        if (sm_codegen_text(sm, stdout, input_path) != 0) {
            fprintf(stderr, "scrip: sm_codegen_text failed\n");
            SM_seq_free(sm);
            return 1;
        }
        SM_seq_free(sm);
        return 0;
    }
    if (mode_compile && target_name && strcmp(target_name, "x86") != 0) {
        if (strcmp(target_name, "js") == 0) {
            if (emit_program(ast_prog, stdout, EMIT_JS) != 0) {
                fprintf(stderr, "scrip: emit_program(js) failed\n");
                return 1;
            }
        } else if (strcmp(target_name, "jvm") == 0) {
            if (emit_program(ast_prog, stdout, EMIT_JVM) != 0) {
                fprintf(stderr, "scrip: emit_program(jvm) failed\n");
                return 1;
            }
        } else if (strcmp(target_name, "net") == 0) {
            if (emit_program(ast_prog, stdout, EMIT_NET) != 0) {
                fprintf(stderr, "scrip: emit_program(net) failed\n");
                return 1;
            }
        } else if (strcmp(target_name, "wasm") == 0) {
            if (emit_program(ast_prog, stdout, EMIT_WASM) != 0) {
                fprintf(stderr, "scrip: emit_program(wasm) failed\n");
                return 1;
            }
        }
        return 0;
    }
    if (mode_monitor) {
        label_table_build(ast_prog);
        prescan_defines(ast_prog);
        g_sno_err_active = 1;
        int div_stmt = sync_monitor_run(ast_prog, 1, input_path);
        if (div_stmt != 0) {
            fprintf(stderr, "scrip --monitor: DIVERGE at stmt %d\n", div_stmt);
            return 1;
        }
        return 0;
    } else if (mode_interp) {
        SM_sequence_t *sm = sm_preamble(ast_prog);
        if (!sm) return 1;
        if (dump_sm) {
            sm_seq_print(sm, stdout);
            SM_seq_free(sm);
            return 0;
        }
        sm_run_with_recovery(sm, sm_interp_run);
        SM_seq_free(sm);
    } else if (mode_run) {
        SM_sequence_t *sm = sm_preamble(ast_prog);
        if (!sm) return 1;
        if (dump_sm) { sm_seq_print(sm, stdout); SM_seq_free(sm); return 0; }
        if (sm_image_init() != 0) {
            fprintf(stderr, "scrip: sm_image_init failed\n");
            SM_seq_free(sm); return 1;
        }
        if (SM_codegen(sm) != 0) {
            fprintf(stderr, "scrip: SM_codegen failed\n");
            SM_seq_free(sm); return 1;
        }
        sm_run_with_recovery(sm, sm_jit_run);
        SM_seq_free(sm);
    } else if (has_non_sno) {
        SM_sequence_t *sm = sm_preamble(ast_prog);
        if (!sm) return 1;
        sm_run_with_recovery(sm, sm_interp_run);
        SM_seq_free(sm);
    } else {
        SM_sequence_t *sm = sm_preamble(ast_prog);
        if (!sm) return 1;
        sm_run_with_recovery(sm, sm_interp_run);
        SM_seq_free(sm);
    }
    if (opt_bench) {
        clock_gettime(CLOCK_MONOTONIC, &_t3);
        double parse_ms = (_t1.tv_sec - _t0.tv_sec)*1e3 + (_t1.tv_nsec - _t0.tv_nsec)/1e6;
        double lower_ms = (_t2.tv_sec - _t1.tv_sec)*1e3 + (_t2.tv_nsec - _t1.tv_nsec)/1e6;
        double exec_ms  = (_t3.tv_sec - _t2.tv_sec)*1e3 + (_t3.tv_nsec - _t2.tv_nsec)/1e6;
        fprintf(stderr, "BENCH parse=%.2fms lower=%.2fms exec=%.2fms total=%.2fms\n",
                parse_ms, lower_ms, exec_ms, parse_ms + lower_ms + exec_ms);
    }
    if (getenv("BINARY_AUDIT") || getenv("SNO_BINARY_BOXES")) {
        extern void bin_audit_print(void);
        bin_audit_print();
    }
    return 0;
}
