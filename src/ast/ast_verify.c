#define BB_DEFINE_NAMES
#include "scrip_cc.h"
#include <stdio.h>
#include <string.h>
/*================================================================================================================================================================================*/
typedef enum { SPEC_EXACT, SPEC_MIN, SPEC_ANY } SpecKind;
typedef struct { SpecKind sk; int n; int need_sval; } KindSpec;
#define EXACT(n)    { SPEC_EXACT, (n), 0 }
#define EXACT_S(n)  { SPEC_EXACT, (n), 1 }
#define MIN(n)      { SPEC_MIN,   (n), 0 }
#define MIN_S(n)    { SPEC_MIN,   (n), 1 }
#define ANY_KIND    { SPEC_ANY,   0,   0 }
#define ANY_S       { SPEC_ANY,   0,   1 }
static const KindSpec kind_spec[TT_KIND_COUNT] = {
    [TT_QLIT]            = EXACT_S(0),  [TT_ILIT]          = EXACT(0),   [TT_FLIT]        = EXACT(0),
    [TT_CSET]            = EXACT_S(0),  [TT_NUL]           = EXACT(0),
    [TT_VAR]             = ANY_S,       [TT_KEYWORD]       = EXACT_S(0), [TT_INDIRECT]    = EXACT(1),   [TT_DEFER]       = EXACT(1),
    [TT_MNS]             = EXACT(1),    [TT_PLS]           = EXACT(1),
    [TT_ADD]             = EXACT(2),    [TT_SUB]           = EXACT(2),   [TT_MUL]         = EXACT(2),
    [TT_DIV]             = EXACT(2),    [TT_MOD]           = EXACT(2),   [TT_POW]         = EXACT(2),
    [TT_SEQ]             = MIN(2),      [TT_ALT]           = MIN(2),     [TT_OPSYN]       = EXACT(2),
    [TT_ARB]             = EXACT(0),    [TT_ARBNO]         = EXACT(1),   [TT_POS]         = EXACT(1),   [TT_RPOS]        = EXACT(1),
    [TT_ANY]             = EXACT(1),    [TT_NOTANY]        = EXACT(1),   [TT_SPAN]        = EXACT(1),
    [TT_BREAK]           = EXACT(1),    [TT_BREAKX]        = EXACT(1),
    [TT_LEN]             = EXACT(1),    [TT_TAB]           = EXACT(1),   [TT_RTAB]        = EXACT(1),
    [TT_REM]             = EXACT(0),    [TT_FAIL]          = EXACT(0),   [TT_SUCCEED]     = EXACT(0),
    [TT_FENCE]           = ANY_KIND,    [TT_ABORT]         = EXACT(0),   [TT_BAL]         = EXACT(0),
    [TT_CAPT_COND_ASGN]  = MIN_S(1),   [TT_CAPT_IMMED_ASGN] = MIN_S(1), [TT_CAPT_CURSOR] = EXACT_S(0),
    [TT_FNC]             = ANY_S,       [TT_IDX]           = MIN(1),     [TT_ASSIGN]      = EXACT(2),
    [TT_SCAN]            = EXACT(2),    [TT_SWAP]          = EXACT(2),
    [TT_SUSPEND]         = EXACT(1),    [TT_TO]            = EXACT(2),   [TT_TO_BY]       = EXACT(3),   [TT_LIMIT]       = EXACT(2),
    [TT_ALTERNATE]       = MIN(2),      [TT_ITERATE]       = EXACT(1),   [TT_MAKELIST]    = ANY_KIND,
    [TT_UNIFY]           = EXACT(2),    [TT_CLAUSE]        = MIN(1),     [TT_CHOICE]      = MIN(1),
    [TT_CUT]             = EXACT(0),    [TT_TRAIL_MARK]    = EXACT(0),   [TT_TRAIL_UNWIND]= EXACT(0),
};
/*--------------------------------------------------------------------------------------------------------------------------------------------------------------------------------*/
typedef struct { int count; FILE * f; } VerifyState;
static void violation(VerifyState * vs, const char * path, const char * msg) {
    if (vs->f) fprintf(vs->f, "ast_verify: %s: %s\n", path, msg);
    vs->count++;
}
/*--------------------------------------------------------------------------------------------------------------------------------------------------------------------------------*/
static void verify_node(const tree_t * e, const char * path, VerifyState * vs, int depth) {
    char child_path[512];
    char msg[128];
    const char * kname;
    const KindSpec * spec;
    int i;
    if (!e)        { violation(vs, path, "NULL node pointer");                          return; }
    if (depth > 256) { violation(vs, path, "tree depth > 256 (cycle or runaway tree)"); return; }
    if ((int)e->t < 0 || e->t >= TT_KIND_COUNT) {
        snprintf(msg, sizeof msg, "invalid kind %d", (int)e->t);
        violation(vs, path, msg);
        return;
    }
    kname = tt_e_name[e->t] ? tt_e_name[e->t] : "?";
    spec  = &kind_spec[e->t];
    if (spec->need_sval && !e->v.sval) {
        snprintf(msg, sizeof msg, "%s requires non-NULL sval", kname);
        violation(vs, path, msg);
    }
    if (spec->sk == SPEC_EXACT && e->n != spec->n) {
        snprintf(msg, sizeof msg, "%s: expected %d children, got %d", kname, spec->n, e->n);
        violation(vs, path, msg);
    } else if (spec->sk == SPEC_MIN && e->n < spec->n) {
        snprintf(msg, sizeof msg, "%s: expected >= %d children, got %d", kname, spec->n, e->n);
        violation(vs, path, msg);
    }
    for (i = 0; i < e->n; i++) {
        snprintf(child_path, sizeof child_path, "%s[%d]", path, i);
        if (!e->c[i]) violation(vs, child_path, "NULL child pointer");
        else          verify_node(e->c[i], child_path, vs, depth + 1);
    }
}
/*--------------------------------------------------------------------------------------------------------------------------------------------------------------------------------*/
int ir_verify_node(const tree_t * e, const char * path, FILE * err) {
    VerifyState vs = { 0, err ? err : stderr };
    verify_node(e, path ? path : "root", &vs, 0);
    return vs.count;
}
/*--------------------------------------------------------------------------------------------------------------------------------------------------------------------------------*/
int ir_verify_program(const CODE_t * prog, FILE * err) {
    VerifyState vs;
    const STMT_t * s;
    int stmt_idx;
    char path[64];
    if (!prog) { if (err) fprintf(err, "ast_verify: NULL CODE_t pointer\n"); return 1; }
    vs       = (VerifyState){ 0, err ? err : stderr };
    stmt_idx = 0;
    for (s = prog->head; s; s = s->next, stmt_idx++) {
        snprintf(path, sizeof path, "stmt[%d].subject",     stmt_idx); if (s->subject)     verify_node(s->subject,     path, &vs, 0);
        snprintf(path, sizeof path, "stmt[%d].pattern",     stmt_idx); if (s->pattern)     verify_node(s->pattern,     path, &vs, 0);
        snprintf(path, sizeof path, "stmt[%d].replacement", stmt_idx); if (s->replacement) verify_node(s->replacement, path, &vs, 0);
    }
    return vs.count;
}
/*================================================================================================================================================================================*/
#ifdef AST_VERIFY_TEST
#include <stdlib.h>
#include <assert.h>
static tree_t * mk(tree_e k)                            { tree_t * e = calloc(1, sizeof *e); e->t = k; return e; }
static void add_child(tree_t * parent, tree_t * child)  { parent->c = realloc(parent->c, (size_t)(parent->n + 1) * sizeof(tree_t *)); parent->c[parent->n++] = child; }
/*--------------------------------------------------------------------------------------------------------------------------------------------------------------------------------*/
int main(void) {
    int failures = 0;
    { tree_t *a=mk(TT_ASSIGN), *v=mk(TT_VAR), *n=mk(TT_ILIT); v->v.sval="x"; n->v.ival=1; add_child(a,v); add_child(a,n);
      int e=ir_verify_node(a,"test1",stderr); if(e!=0){fprintf(stderr,"FAIL test1: got %d\n",e);failures++;}else fprintf(stderr,"PASS test1\n"); }
    { tree_t *v=mk(TT_VAR);
      int e=ir_verify_node(v,"test2",NULL); if(e!=1){fprintf(stderr,"FAIL test2: got %d\n",e);failures++;}else fprintf(stderr,"PASS test2\n"); }
    { tree_t *a=mk(TT_ADD), *n=mk(TT_ILIT); add_child(a,n);
      int e=ir_verify_node(a,"test3",NULL); if(e!=1){fprintf(stderr,"FAIL test3: got %d\n",e);failures++;}else fprintf(stderr,"PASS test3\n"); }
    { tree_t *s=mk(TT_SEQ), *q1=mk(TT_QLIT), *q2=mk(TT_QLIT); q1->v.sval="hello"; q2->v.sval="world"; add_child(s,q1); add_child(s,q2);
      int e=ir_verify_node(s,"test4",stderr); if(e!=0){fprintf(stderr,"FAIL test4: got %d\n",e);failures++;}else fprintf(stderr,"PASS test4\n"); }
    { tree_t *q=mk(TT_QLIT);
      int e=ir_verify_node(q,"test5",NULL); if(e!=1){fprintf(stderr,"FAIL test5: got %d\n",e);failures++;}else fprintf(stderr,"PASS test5\n"); }
    { tree_t *a=mk(TT_ASSIGN), *lhs=mk(TT_VAR), *add=mk(TT_ADD), *one=mk(TT_ILIT), *two=mk(TT_ILIT);
      lhs->v.sval="result"; one->v.ival=1; two->v.ival=2;
      add_child(add,one); add_child(add,two); add_child(a,lhs); add_child(a,add);
      int e=ir_verify_node(a,"test6",stderr); if(e!=0){fprintf(stderr,"FAIL test6: got %d\n",e);failures++;}else fprintf(stderr,"PASS test6\n"); }
    fprintf(stderr, "\n%s — %d failure(s)\n", failures==0?"ALL PASS":"FAILURES PRESENT", failures);
    return failures ? 1 : 0;
}
#endif
