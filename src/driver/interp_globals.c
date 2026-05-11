/*
 * interp_globals.c — interpreter global variables
 *
 * Split from interp.c by RS-3 (GOAL-REWRITE-SCRIP).
 * AUTHORS: Lon Jones Cherryholmes · Claude Sonnet 4.6
 * DATE:    2026-05-02
 */

#include "interp_private.h"

/* ── RK-25: Raku exception global ─────────────────────────────────────── */
char g_raku_exception[512] = "";   /* set by raku_die, read by raku_try */
Raku_match g_raku_match;        /* last regex match result */
const char *g_raku_subject = ""; /* subject of last match */
/* RK-38: file handle table */
#define RAKU_FH_MAX 64
FILE *raku_fh_table[RAKU_FH_MAX];
int   raku_fh_init = 0;
void raku_fh_ensure_init(void) {
    if (raku_fh_init) return;
    memset(raku_fh_table,0,sizeof raku_fh_table);
    raku_fh_table[0]=stdin; raku_fh_table[1]=stdout; raku_fh_table[2]=stderr;
    raku_fh_init=1;
}
int raku_fh_alloc(FILE *fp) {
    raku_fh_ensure_init();
    for(int i=3;i<RAKU_FH_MAX;i++) if(!raku_fh_table[i]){raku_fh_table[i]=fp;return i;}
    return -1;
}
FILE *raku_fh_get(int idx){
    raku_fh_ensure_init();
    if(idx<0||idx>=RAKU_FH_MAX) return NULL;
    return raku_fh_table[idx];
}
void raku_fh_free(int idx){
    if(raku_fh_init&&idx>=3&&idx<RAKU_FH_MAX) raku_fh_table[idx]=NULL;
}

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
extern int         Σlen;

/* SI-6: g_prog (CODE_t*) removed — TT_PROGRAM is now the sole program rep. */
int g_polyglot = 0; /* U-23: 1 when running a fenced polyglot .scrip file */
int g_opt_trace   = 0;  /* --trace:   print STMT N on each statement */
int g_opt_dump_bb = 0;  /* --dump-bb: print PATND tree before each match */

/* IM-3: IR step-limit for in-process sync monitor */
int      g_ir_step_limit = 0;   /* 0 = unlimited; N = stop after N stmts */
int      g_ir_steps_done = 0;
jmp_buf  g_ir_step_jmp;

