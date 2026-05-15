#include "interp_private.h"
char g_raku_exception[512] = "";
Raku_match g_raku_match;
const char *g_raku_subject = "";
#define RAKU_FH_MAX 64
FILE *raku_fh_table[RAKU_FH_MAX];
char *raku_fh_name[RAKU_FH_MAX];
int   raku_fh_init = 0;
/*----------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------*/
void raku_fh_ensure_init(void) {
    if (raku_fh_init) return;
    memset(raku_fh_table,0,sizeof raku_fh_table);
    memset(raku_fh_name,0,sizeof raku_fh_name);
    raku_fh_table[0]=stdin; raku_fh_table[1]=stdout; raku_fh_table[2]=stderr;
    raku_fh_name[0]="&input"; raku_fh_name[1]="&output"; raku_fh_name[2]="&errout";
    raku_fh_init=1;
}
/*----------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------*/
int raku_fh_alloc(FILE *fp) {
    raku_fh_ensure_init();
    for(int i=3;i<RAKU_FH_MAX;i++) if(!raku_fh_table[i]){raku_fh_table[i]=fp;raku_fh_name[i]=NULL;return i;}
    return -1;
}
/*----------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------*/
FILE *raku_fh_get(int idx){
    raku_fh_ensure_init();
    if(idx<0||idx>=RAKU_FH_MAX) return NULL;
    return raku_fh_table[idx];
}
/*----------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------*/
void raku_fh_free(int idx){
    if(raku_fh_init&&idx>=3&&idx<RAKU_FH_MAX){ raku_fh_table[idx]=NULL; }
}
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
extern int         Σlen;
int g_polyglot = 0;
int g_opt_trace   = 0;
int g_opt_dump_bb = 0;
int      g_ir_step_limit = 0;
int      g_ir_steps_done = 0;
jmp_buf  g_ir_step_jmp;
