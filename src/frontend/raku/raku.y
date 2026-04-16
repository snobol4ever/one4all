%define api.prefix {raku_yy}

%code requires {
/*
 * raku.y — Tiny-Raku Bison grammar
 *
 * FI-3: builds EXPR_t/STMT_t directly — no intermediate RakuNode AST.
 *
 * AUTHORS: Lon Jones Cherryholmes · Claude Sonnet 4.6
 */
#include "../../ir/ir.h"
#include "../snobol4/scrip_cc.h"

typedef struct ExprList {
    EXPR_t **items;
    int      count;
    int      cap;
} ExprList;
}

%{
#include "../../ir/ir.h"
#include "../snobol4/scrip_cc.h"
#include "raku.tab.h"   /* pulls in ExprList from %code requires */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

extern int  raku_yylex(void);
extern int  raku_get_lineno(void);
void raku_yyerror(const char *msg) {
    fprintf(stderr, "raku parse error line %d: %s\n", raku_get_lineno(), msg);
}

/*--------------------------------------------------------------------
 * ExprList helpers
 *--------------------------------------------------------------------*/
static ExprList *exprlist_new(void) {
    ExprList *l = calloc(1, sizeof *l);
    if (!l) { fprintf(stderr, "raku: OOM\n"); exit(1); }
    return l;
}
static ExprList *exprlist_append(ExprList *l, EXPR_t *e) {
    if (l->count >= l->cap) {
        l->cap = l->cap ? l->cap * 2 : 8;
        l->items = realloc(l->items, l->cap * sizeof(EXPR_t *));
        if (!l->items) { fprintf(stderr, "raku: OOM\n"); exit(1); }
    }
    l->items[l->count++] = e;
    return l;
}
static void exprlist_free(ExprList *l) { if (l) { free(l->items); free(l); } }

/*--------------------------------------------------------------------
 * Build helpers (logic from raku_lower.c, inlined for direct IR)
 *--------------------------------------------------------------------*/
static const char *strip_sigil(const char *s) {
    if (s && (s[0]=='$'||s[0]=='@'||s[0]=='%')) return s+1;
    return s;
}
static EXPR_t *leaf_sval(EKind k, const char *s) {
    EXPR_t *e = expr_new(k); e->sval = intern(s); return e;
}
static EXPR_t *var_node(const char *name) {
    return leaf_sval(E_VAR, strip_sigil(name));
}
/* make_call: E_FNC + children[0]=E_VAR(name) for icn_interp_eval layout */
static EXPR_t *make_call(const char *name) {
    EXPR_t *e = leaf_sval(E_FNC, name);
    EXPR_t *n = expr_new(E_VAR); n->sval = intern(name);
    expr_add_child(e, n);
    return e;
}
/* make_seq: ExprList → E_SEQ_EXPR, frees list */
static EXPR_t *make_seq(ExprList *stmts) {
    EXPR_t *seq = expr_new(E_SEQ_EXPR);
    if (stmts) {
        for (int i = 0; i < stmts->count; i++) expr_add_child(seq, stmts->items[i]);
        exprlist_free(stmts);
    }
    return seq;
}
/* lower_interp_str: "hello $var" → left-associative E_CAT chain */
static EXPR_t *lower_interp_str(const char *s) {
    int len = s ? (int)strlen(s) : 0;
    EXPR_t *result = NULL;
    char litbuf[4096]; int litpos = 0, i = 0;
    while (i < len) {
        if (s[i]=='$' && i+1<len &&
            (s[i+1]=='_'||(s[i+1]>='A'&&s[i+1]<='Z')||(s[i+1]>='a'&&s[i+1]<='z'))) {
            if (litpos>0) { litbuf[litpos]='\0';
                EXPR_t *lit=leaf_sval(E_QLIT,litbuf);
                result=result?expr_binary(E_CAT,result,lit):lit; litpos=0; }
            i++;
            char vname[256]; int vlen=0;
            while (i<len&&(s[i]=='_'||(s[i]>='A'&&s[i]<='Z')||(s[i]>='a'&&s[i]<='z')||(s[i]>='0'&&s[i]<='9')))
                { if(vlen<255) vname[vlen++]=s[i]; i++; }
            vname[vlen]='\0';
            EXPR_t *var=leaf_sval(E_VAR,vname);
            result=result?expr_binary(E_CAT,result,var):var;
        } else { if(litpos<4095) litbuf[litpos++]=s[i]; i++; }
    }
    if (litpos>0) { litbuf[litpos]='\0';
        EXPR_t *lit=leaf_sval(E_QLIT,litbuf);
        result=result?expr_binary(E_CAT,result,lit):lit; }
    return result ? result : leaf_sval(E_QLIT,"");
}
/* make_for_range: for lo..hi -> $v body → explicit while-loop */
static EXPR_t *make_for_range(EXPR_t *lo, EXPR_t *hi, const char *vname, EXPR_t *body_seq) {
    EXPR_t *init = expr_binary(E_ASSIGN, leaf_sval(E_VAR,vname), lo);
    EXPR_t *cond = expr_binary(E_LE, leaf_sval(E_VAR,vname), hi);
    EXPR_t *one  = expr_new(E_ILIT); one->ival = 1;
    EXPR_t *incr = expr_binary(E_ADD, leaf_sval(E_VAR,vname), one);
    expr_add_child(body_seq, expr_binary(E_ASSIGN, leaf_sval(E_VAR,vname), incr));
    EXPR_t *wloop = expr_binary(E_WHILE, cond, body_seq);
    EXPR_t *seq   = expr_new(E_SEQ_EXPR);
    expr_add_child(seq, init); expr_add_child(seq, wloop);
    return seq;
}

/*--------------------------------------------------------------------
 * Program output
 *--------------------------------------------------------------------*/
Program *raku_prog_result = NULL;

static void add_proc(EXPR_t *e) {
    if (!e) return;
    if (!raku_prog_result) raku_prog_result = calloc(1, sizeof(Program));
    STMT_t *st = calloc(1, sizeof(STMT_t));
    st->subject = e; st->lineno = 0; st->lang = LANG_RAKU;
    if (!raku_prog_result->head) raku_prog_result->head = raku_prog_result->tail = st;
    else { raku_prog_result->tail->next = st; raku_prog_result->tail = st; }
    raku_prog_result->nstmts++;
}

/* SUB_TAG: sentinel bit to distinguish sub defs from body stmts in stmt_list */
#define SUB_TAG 0x40000000

/* RK-26: Raku method table — maps "ClassName::method" → E_FNC proc name */
#define RAKU_METH_MAX 256
typedef struct { char key[128]; char procname[128]; } RakuMethEntry;
static RakuMethEntry raku_meth_table[RAKU_METH_MAX];
static int           raku_meth_ntypes = 0;

static void raku_meth_register(const char *classname, const char *methname, const char *procname) {
    if (raku_meth_ntypes >= RAKU_METH_MAX) return;
    RakuMethEntry *e = &raku_meth_table[raku_meth_ntypes++];
    snprintf(e->key,      sizeof e->key,      "%s::%s", classname, methname);
    snprintf(e->procname, sizeof e->procname,  "%s",     procname);
}

/* Emit the extern declaration so interp.c can call raku_meth_lookup */
const char *raku_meth_lookup(const char *classname, const char *methname) {
    char key[128];
    snprintf(key, sizeof key, "%s::%s", classname, methname);
    for (int i = 0; i < raku_meth_ntypes; i++)
        if (strcmp(raku_meth_table[i].key, key) == 0)
            return raku_meth_table[i].procname;
    return NULL;
}


%}

%union {
    long      ival;
    double    dval;
    char     *sval;
    EXPR_t   *node;
    ExprList *list;
}

%token <ival> LIT_INT
%token <dval> LIT_FLOAT
%token <sval> LIT_STR LIT_INTERP_STR LIT_REGEX LIT_MATCH_GLOBAL LIT_SUBST
%token <sval> VAR_SCALAR VAR_ARRAY VAR_HASH VAR_TWIGIL IDENT
%token <ival> VAR_CAPTURE
%token <sval> VAR_NAMED_CAPTURE

%token KW_MY KW_SAY KW_PRINT KW_IF KW_ELSE KW_ELSIF KW_WHILE KW_FOR
%token KW_SUB KW_GATHER KW_TAKE KW_RETURN
%token KW_GIVEN KW_WHEN KW_DEFAULT
%token KW_EXISTS KW_DELETE KW_UNLESS KW_UNTIL KW_REPEAT
%token KW_MAP KW_GREP KW_SORT
%token KW_TRY KW_CATCH KW_DIE
%token KW_CLASS KW_METHOD KW_HAS KW_NEW
%token OP_FATARROW

%token OP_RANGE OP_RANGE_EX
%token OP_ARROW
%token OP_EQ OP_NE OP_LE OP_GE
%token OP_SEQ OP_SNE
%token OP_AND OP_OR
%token OP_BIND
%token OP_SMATCH
%token OP_DIV

%type <node> stmt expr atom range_expr cmp_expr add_expr closure
%type <node> mul_expr unary_expr postfix_expr call_expr block
%type <node> if_stmt while_stmt for_stmt sub_decl given_stmt
%type <node> unless_stmt until_stmt repeat_stmt class_decl
%type <list> stmt_list arg_list param_list when_list named_arg_list class_body_list

%right '=' OP_BIND
%left  OP_OR
%left  OP_AND
%left  '!'
%left  OP_EQ OP_NE '<' '>' OP_LE OP_GE OP_SEQ OP_SNE OP_SMATCH
%left  OP_RANGE OP_RANGE_EX
%left  '~'
%left  '+' '-'
%left  '*' '/' '%' OP_DIV
%right UMINUS
%left  '.'

%%

program
    : stmt_list
        {
            ExprList *all = $1;
            /* Partition: subs (ival & SUB_TAG) vs body stmts */
            if (all) {
                /* Pass 1: emit sub defs */
                for (int i = 0; i < all->count; i++) {
                    EXPR_t *e = all->items[i];
                    if (!e || !(e->kind==E_FNC && (e->ival & SUB_TAG))) continue;
                    e->ival &= ~SUB_TAG;   /* restore real nparams */
                    add_proc(e);
                    all->items[i] = NULL;  /* mark consumed */
                }
                /* Pass 2: wrap remaining body stmts in synthetic "main" E_FNC */
                int has_body = 0;
                for (int i = 0; i < all->count; i++) if (all->items[i]) { has_body=1; break; }
                if (has_body) {
                    EXPR_t *mf = leaf_sval(E_FNC, "main"); mf->ival = 0;
                    EXPR_t *mn = expr_new(E_VAR); mn->sval = intern("main");
                    expr_add_child(mf, mn);
                    for (int i = 0; i < all->count; i++)
                        if (all->items[i]) expr_add_child(mf, all->items[i]);
                    add_proc(mf);
                }
                exprlist_free(all);
            }
        }
    ;

stmt_list
    : /* empty */    { $$ = exprlist_new(); }
    | stmt_list stmt { $$ = exprlist_append($1, $2); }
    ;

stmt
    : KW_MY VAR_SCALAR '=' expr ';'
        { $$ = expr_binary(E_ASSIGN, var_node($2), $4); }
    | KW_MY VAR_ARRAY '=' expr ';'
        { $$ = expr_binary(E_ASSIGN, var_node($2), $4); }
    | KW_MY VAR_HASH '=' expr ';'
        { $$ = expr_binary(E_ASSIGN, var_node($2), $4); }
    /* RK-19: typed variable declarations — type annotation (IDENT) silently discarded */
    | KW_MY IDENT VAR_SCALAR '=' expr ';'
        { free($2); $$ = expr_binary(E_ASSIGN, var_node($3), $5); }
    | KW_MY IDENT VAR_ARRAY '=' expr ';'
        { free($2); $$ = expr_binary(E_ASSIGN, var_node($3), $5); }
    | KW_MY IDENT VAR_HASH '=' expr ';'
        { free($2); $$ = expr_binary(E_ASSIGN, var_node($3), $5); }
    | KW_MY IDENT VAR_SCALAR ';'
        { free($2); $$ = expr_binary(E_ASSIGN, var_node($3), leaf_sval(E_QLIT, "")); }
    | KW_MY IDENT VAR_ARRAY ';'
        { free($2); $$ = expr_binary(E_ASSIGN, var_node($3), leaf_sval(E_QLIT, "")); }
    | KW_MY IDENT VAR_HASH ';'
        { free($2); $$ = expr_binary(E_ASSIGN, var_node($3), leaf_sval(E_QLIT, "")); }
    | KW_SAY expr ';'
        { EXPR_t *c=make_call("write"); expr_add_child(c,$2); $$=c; }
    | KW_PRINT expr ';'
        { EXPR_t *c=make_call("writes"); expr_add_child(c,$2); $$=c; }
    | KW_TAKE expr ';'
        { $$=expr_unary(E_SUSPEND,$2); }
    | KW_RETURN expr ';'
        { EXPR_t *r=expr_new(E_RETURN); expr_add_child(r,$2); $$=r; }
    | KW_RETURN ';'
        { $$=expr_new(E_RETURN); }
    | VAR_SCALAR '=' expr ';'
        { $$=expr_binary(E_ASSIGN,var_node($1),$3); }
    /* RK-26: $obj.field = expr — field write */
    | VAR_SCALAR '.' IDENT '=' expr ';'
        { EXPR_t *fe=expr_new(E_FIELD);
          fe->sval=(char*)intern($3); free($3);
          expr_add_child(fe,var_node($1));
          $$=expr_binary(E_ASSIGN,fe,$5); }
    | VAR_ARRAY '[' expr ']' '=' expr ';'
        { EXPR_t *c=make_call("arr_set");
          expr_add_child(c,var_node($1)); expr_add_child(c,$3); expr_add_child(c,$6); $$=c; }
    | VAR_HASH '<' IDENT '>' '=' expr ';'
        { EXPR_t *c=make_call("hash_set");
          expr_add_child(c,var_node($1)); expr_add_child(c,leaf_sval(E_QLIT,$3)); expr_add_child(c,$6); $$=c; }
    | VAR_HASH '{' expr '}' '=' expr ';'
        { EXPR_t *c=make_call("hash_set");
          expr_add_child(c,var_node($1)); expr_add_child(c,$3); expr_add_child(c,$6); $$=c; }
    | KW_DELETE VAR_HASH '<' IDENT '>' ';'
        { EXPR_t *c=make_call("hash_delete");
          expr_add_child(c,var_node($2)); expr_add_child(c,leaf_sval(E_QLIT,$4)); $$=c; }
    | KW_DELETE VAR_HASH '{' expr '}' ';'
        { EXPR_t *c=make_call("hash_delete");
          expr_add_child(c,var_node($2)); expr_add_child(c,$4); $$=c; }
    | expr ';' { $$=$1; }
    | if_stmt           { $$=$1; }
    | while_stmt        { $$=$1; }
    | for_stmt          { $$=$1; }
    | given_stmt        { $$=$1; }
    /* RK-25: try/CATCH as block-level stmts (no trailing semicolon) */
    | KW_TRY block
        { EXPR_t *c=make_call("raku_try");
          expr_add_child(c,$2); $$=c; }
    | KW_TRY block KW_CATCH block
        { EXPR_t *c=make_call("raku_try");
          expr_add_child(c,$2); expr_add_child(c,$4); $$=c; }
    | unless_stmt       { $$=$1; }
    | until_stmt        { $$=$1; }
    | repeat_stmt       { $$=$1; }
    | sub_decl          { $$=$1; }
    | class_decl        { $$=$1; }
    ;

if_stmt
    : KW_IF '(' expr ')' block
        { EXPR_t *e=expr_new(E_IF); expr_add_child(e,$3); expr_add_child(e,$5); $$=e; }
    | KW_IF '(' expr ')' block KW_ELSE block
        { EXPR_t *e=expr_new(E_IF); expr_add_child(e,$3); expr_add_child(e,$5); expr_add_child(e,$7); $$=e; }
    | KW_IF '(' expr ')' block KW_ELSE if_stmt
        { EXPR_t *e=expr_new(E_IF); expr_add_child(e,$3); expr_add_child(e,$5); expr_add_child(e,$7); $$=e; }
    ;

while_stmt
    : KW_WHILE '(' expr ')' block
        { $$=expr_binary(E_WHILE,$3,$5); }
    ;

/* RK-20: unless — if !cond */
unless_stmt
    : KW_UNLESS '(' expr ')' block
        { EXPR_t *e=expr_new(E_IF); expr_add_child(e,expr_unary(E_NOT,$3)); expr_add_child(e,$5); $$=e; }
    | KW_UNLESS '(' expr ')' block KW_ELSE block
        { EXPR_t *e=expr_new(E_IF); expr_add_child(e,expr_unary(E_NOT,$3)); expr_add_child(e,$5); expr_add_child(e,$7); $$=e; }
    ;

/* RK-20: until — loop while cond is falsy */
until_stmt
    : KW_UNTIL '(' expr ')' block
        { EXPR_t *e=expr_new(E_UNTIL); expr_add_child(e,$3); expr_add_child(e,$5); $$=e; }
    ;

/* RK-20: repeat — unconditional loop (use last to break) */
repeat_stmt
    : KW_REPEAT block
        { EXPR_t *e=expr_new(E_REPEAT); expr_add_child(e,$2); $$=e; }
    ;

for_stmt
    /* for expr -> $v body: if expr is E_TO (range), use while-loop lowering */
    : KW_FOR expr OP_ARROW VAR_SCALAR block
        { EXPR_t *iter=$2; const char *vname=strip_sigil($4);
          if (iter->kind==E_TO) {
              /* range case: lo=children[0], hi=children[1] */
              $$ = make_for_range(iter->children[0], iter->children[1], vname, $5);
          } else {
              /* Always wrap in E_ITERATE so loopvar goes on the wrapper node.
               * RK-16/RK-21: gen->sval = loopvar name for icn_drive / E_EVERY binding. */
              const char *vn = intern(strip_sigil($4));
              EXPR_t *gen = expr_unary(E_ITERATE, iter);
              gen->sval = (char *)vn;
              $$=expr_binary(E_EVERY,gen,$5);
          } }
    | KW_FOR expr block
        { EXPR_t *gen=($2->kind==E_VAR)?expr_unary(E_ITERATE,$2):$2;
          $$=expr_binary(E_EVERY,gen,$3); }
    ;

given_stmt
    : KW_GIVEN expr '{' when_list '}'
        { /* RK-18d: E_CASE[ topic, cmpnode0, val0, body0, cmpnode1, val1, body1, ... ]
           * cmp kind stored in separate E_ILIT node (ival=EKind) to avoid corrupting val->ival. */
          EXPR_t *ec=expr_new(E_CASE);
          expr_add_child(ec,$2);
          ExprList *whens=$4;
          for(int i=0;i<whens->count;i++){
              EXPR_t *pair=whens->items[i];
              EKind cmp=(EKind)(pair->ival);
              EXPR_t *val=pair->children[0], *body=pair->children[1];
              EXPR_t *cn=expr_new(E_ILIT); cn->ival=(long)cmp;
              expr_add_child(ec,cn); expr_add_child(ec,val); expr_add_child(ec,body);
          }
          exprlist_free(whens);
          $$=ec; }
    | KW_GIVEN expr '{' when_list KW_DEFAULT block '}'
        { /* RK-18d: E_CASE with default: E_NUL cmpnode + E_NUL val + body at end. */
          EXPR_t *ec=expr_new(E_CASE);
          expr_add_child(ec,$2);
          ExprList *whens=$4;
          for(int i=0;i<whens->count;i++){
              EXPR_t *pair=whens->items[i];
              EKind cmp=(EKind)(pair->ival);
              EXPR_t *val=pair->children[0], *body=pair->children[1];
              EXPR_t *cn=expr_new(E_ILIT); cn->ival=(long)cmp;
              expr_add_child(ec,cn); expr_add_child(ec,val); expr_add_child(ec,body);
          }
          exprlist_free(whens);
          expr_add_child(ec,expr_new(E_NUL)); expr_add_child(ec,expr_new(E_NUL)); expr_add_child(ec,$6);
          $$=ec; }
    ;

when_list
    : /* empty */  { $$=exprlist_new(); }
    | when_list KW_WHEN expr block
        { EKind cmp=($3->kind==E_QLIT)?E_LEQ:E_EQ;
          EXPR_t *pair=expr_new(E_SEQ_EXPR);
          pair->ival=(long)cmp;
          expr_add_child(pair,$3); expr_add_child(pair,$4);
          $$=exprlist_append($1,pair); }
    ;

sub_decl
    : KW_SUB IDENT '(' param_list ')' block
        { ExprList *params=$4; int np=params?params->count:0;
          EXPR_t *e=leaf_sval(E_FNC,$2); e->ival=(long)np|SUB_TAG;
          EXPR_t *nn=expr_new(E_VAR); nn->sval=intern($2); expr_add_child(e,nn);
          if(params){ for(int i=0;i<np;i++) expr_add_child(e,params->items[i]); exprlist_free(params); }
          EXPR_t *body=$6;
          for(int i=0;i<body->nchildren;i++) expr_add_child(e,body->children[i]);
          $$=e; }
    | KW_SUB IDENT '(' ')' block
        { EXPR_t *e=leaf_sval(E_FNC,$2); e->ival=(long)0|SUB_TAG;
          EXPR_t *nn=expr_new(E_VAR); nn->sval=intern($2); expr_add_child(e,nn);
          EXPR_t *body=$5;
          for(int i=0;i<body->nchildren;i++) expr_add_child(e,body->children[i]);
          $$=e; }
    ;

/* ── RK-26: class declaration ─────────────────────────────────────────────
 * class Name { has $.f1; has $.f2; method m($p) { ... } }
 * ──────────────────────────────────────────────────────────────────────── */
class_decl
    : KW_CLASS IDENT '{' class_body_list '}'
        {
            const char *cname = intern($2); free($2);
            ExprList *body = $4;
            EXPR_t *rec = expr_new(E_RECORD);
            rec->sval = (char *)cname;
            if (body) {
                for (int i = 0; i < body->count; i++) {
                    EXPR_t *item = body->items[i];
                    if (!item) continue;
                    if (item->kind == E_VAR) {
                        expr_add_child(rec, item);
                    } else if (item->kind == E_FNC && (item->ival & SUB_TAG)) {
                        char fullname[256];
                        snprintf(fullname, sizeof fullname, "%s__%s", cname, item->sval);
                        const char *fname = intern(fullname);
                        raku_meth_register(cname, item->sval, fname);
                        item->sval = (char *)fname;
                        if (item->nchildren > 0 && item->children[0]->kind == E_VAR)
                            item->children[0]->sval = (char *)fname;
                        item->ival &= ~SUB_TAG;
                        add_proc(item);
                        body->items[i] = NULL;
                    }
                }
                exprlist_free(body);
            }
            add_proc(rec);
            $$ = expr_new(E_NUL);
        }
    ;

class_body_list
    : /* empty */  { $$ = exprlist_new(); }
    | class_body_list KW_HAS VAR_TWIGIL ';'
        { EXPR_t *fv = leaf_sval(E_VAR, $3); free($3);
          $$ = exprlist_append($1, fv); }
    | class_body_list KW_HAS VAR_SCALAR ';'
        { EXPR_t *fv = leaf_sval(E_VAR, strip_sigil($3)); free($3);
          $$ = exprlist_append($1, fv); }
    | class_body_list KW_METHOD IDENT '(' param_list ')' block
        { ExprList *params = $5; int np = params ? params->count : 0;
          EXPR_t *e = leaf_sval(E_FNC, $3);
          e->ival = (long)(np + 1) | SUB_TAG;
          EXPR_t *nn = expr_new(E_VAR); nn->sval = intern($3); expr_add_child(e, nn);
          expr_add_child(e, leaf_sval(E_VAR, "self"));
          if (params) { for (int i = 0; i < np; i++) expr_add_child(e, params->items[i]); exprlist_free(params); }
          EXPR_t *body = $7;
          for (int i = 0; i < body->nchildren; i++) expr_add_child(e, body->children[i]);
          free($3);
          $$ = exprlist_append($1, e); }
    | class_body_list KW_METHOD IDENT '(' ')' block
        { EXPR_t *e = leaf_sval(E_FNC, $3);
          e->ival = (long)(1) | SUB_TAG;
          EXPR_t *nn = expr_new(E_VAR); nn->sval = intern($3); expr_add_child(e, nn);
          expr_add_child(e, leaf_sval(E_VAR, "self"));
          EXPR_t *body = $6;
          for (int i = 0; i < body->nchildren; i++) expr_add_child(e, body->children[i]);
          free($3);
          $$ = exprlist_append($1, e); }
    ;

named_arg_list
    : IDENT OP_FATARROW expr
        { $$ = exprlist_new();
          exprlist_append($$, leaf_sval(E_QLIT, $1)); free($1);
          exprlist_append($$, $3); }
    | named_arg_list ',' IDENT OP_FATARROW expr
        { exprlist_append($1, leaf_sval(E_QLIT, $3)); free($3);
          exprlist_append($1, $5);
          $$ = $1; }
    ;

param_list
    : VAR_SCALAR             { $$=exprlist_append(exprlist_new(),var_node($1)); }
    | param_list ',' VAR_SCALAR { $$=exprlist_append($1,var_node($3)); }
    ;

block
    : '{' stmt_list '}'  { $$=make_seq($2); }
    ;

/* closure: '{' expr '}' — for map/grep/sort bodies; $_ is bound by caller */
closure
    : '{' expr '}'  { $$=$2; }
    ;

expr
    : VAR_SCALAR '=' expr  { $$=expr_binary(E_ASSIGN,var_node($1),$3); }
    | KW_GATHER block      {
          /* RK-21: gather { block } → anonymous coroutine sub + call.
           * 1. Build E_FNC def with SUB_TAG (like sub_decl) named __gather_N.
           * 2. add_proc() so it lands in the proc table.
           * 3. Return a call E_FNC (no SUB_TAG) so icn_eval_gen wraps it as
           *    icn_bb_suspend — a BB_PUMP coroutine collecting E_SUSPEND (take) values. */
          static int gather_seq = 0;
          char gname[32]; snprintf(gname, sizeof gname, "__gather_%d", gather_seq++);
          /* Build the def node */
          EXPR_t *def = leaf_sval(E_FNC, gname); def->ival = (long)0 | SUB_TAG;
          EXPR_t *dn  = expr_new(E_VAR); dn->sval = intern(gname);
          expr_add_child(def, dn);
          /* Splice block children into def */
          EXPR_t *blk = $2;
          for (int i = 0; i < blk->nchildren; i++) expr_add_child(def, blk->children[i]);
          def->ival &= ~SUB_TAG;   /* strip sentinel — restore real nparams (0) for icn_call_proc */
          add_proc(def);
          /* Build the call node (no SUB_TAG) */
          EXPR_t *call = leaf_sval(E_FNC, gname);
          EXPR_t *cn   = expr_new(E_VAR); cn->sval = intern(gname);
          expr_add_child(call, cn);
          $$ = call;
      }
    | cmp_expr             { $$=$1; }
    ;

cmp_expr
    : cmp_expr OP_AND add_expr  { $$=expr_binary(E_SEQ,$1,$3); }
    | cmp_expr OP_OR  add_expr  { $$=expr_binary(E_ALT,$1,$3); }
    | add_expr OP_EQ  add_expr  { $$=expr_binary(E_EQ,$1,$3); }
    | add_expr OP_NE  add_expr  { $$=expr_binary(E_NE,$1,$3); }
    | add_expr '<'    add_expr  { $$=expr_binary(E_LT,$1,$3); }
    | add_expr '>'    add_expr  { $$=expr_binary(E_GT,$1,$3); }
    | add_expr OP_LE  add_expr  { $$=expr_binary(E_LE,$1,$3); }
    | add_expr OP_GE  add_expr  { $$=expr_binary(E_GE,$1,$3); }
    | add_expr OP_SEQ add_expr  { $$=expr_binary(E_LEQ,$1,$3); }
    | add_expr OP_SNE add_expr  { $$=expr_binary(E_LNE,$1,$3); }
    | add_expr OP_SMATCH LIT_REGEX
        { /* RK-23: $s ~~ /pattern/ */
          EXPR_t *c = make_call("raku_match");
          expr_add_child(c, $1);
          expr_add_child(c, leaf_sval(E_QLIT, $3));
          $$ = c; }
    | add_expr OP_SMATCH LIT_MATCH_GLOBAL
        { /* RK-37: $s ~~ m:g/pat/ — global match, returns SOH-sep match list */
          EXPR_t *c = make_call("raku_match_global");
          expr_add_child(c, $1);
          expr_add_child(c, leaf_sval(E_QLIT, $3));
          $$ = c; }
    | add_expr OP_SMATCH LIT_SUBST
        { /* RK-37: $s ~~ s/pat/repl/[g] — substitution */
          EXPR_t *c = make_call("raku_subst");
          expr_add_child(c, $1);
          expr_add_child(c, leaf_sval(E_QLIT, $3));
          $$ = c; }
    | range_expr               { $$=$1; }
    ;

range_expr
    : add_expr OP_RANGE    add_expr { $$=expr_binary(E_TO,$1,$3); }
    | add_expr OP_RANGE_EX add_expr { $$=expr_binary(E_TO,$1,$3); }
    | add_expr                      { $$=$1; }
    ;

add_expr
    : add_expr '+' mul_expr  { $$=expr_binary(E_ADD,$1,$3); }
    | add_expr '-' mul_expr  { $$=expr_binary(E_SUB,$1,$3); }
    | add_expr '~' mul_expr  { $$=expr_binary(E_CAT,$1,$3); }
    | mul_expr               { $$=$1; }
    ;

mul_expr
    : mul_expr '*'    unary_expr  { $$=expr_binary(E_MUL,$1,$3); }
    | mul_expr '/'    unary_expr  { $$=expr_binary(E_DIV,$1,$3); }
    | mul_expr '%'    unary_expr  { $$=expr_binary(E_MOD,$1,$3); }
    | mul_expr OP_DIV unary_expr  { $$=expr_binary(E_DIV,$1,$3); }
    | unary_expr                  { $$=$1; }
    ;

unary_expr
    : '-' unary_expr %prec UMINUS  { $$=expr_unary(E_MNS,$2); }
    | '!' unary_expr               { $$=expr_unary(E_NOT,$2); }
    | postfix_expr                 { $$=$1; }
    ;

postfix_expr : call_expr { $$=$1; } ;

call_expr
    : IDENT '(' arg_list ')'
        { EXPR_t *e=make_call($1);
          ExprList *args=$3;
          if(args){ for(int i=0;i<args->count;i++) expr_add_child(e,args->items[i]); exprlist_free(args); }
          $$=e; }
    | IDENT '(' ')'  { $$=make_call($1); }
    /* RK-26: ClassName.new(named_args) */
    | IDENT '.' KW_NEW '(' named_arg_list ')'
        { EXPR_t *c=make_call("raku_new");
          expr_add_child(c,leaf_sval(E_QLIT,$1)); free($1);
          ExprList *nargs=$5;
          if(nargs){ for(int i=0;i<nargs->count;i++) expr_add_child(c,nargs->items[i]); exprlist_free(nargs); }
          $$=c; }
    | IDENT '.' KW_NEW '(' ')'
        { EXPR_t *c=make_call("raku_new");
          expr_add_child(c,leaf_sval(E_QLIT,$1)); free($1);
          $$=c; }
    /* RK-26: $obj.method(args) */
    | atom '.' IDENT '(' arg_list ')'
        { EXPR_t *c=make_call("raku_mcall");
          expr_add_child(c,$1);
          expr_add_child(c,leaf_sval(E_QLIT,$3)); free($3);
          ExprList *args=$5;
          if(args){ for(int i=0;i<args->count;i++) expr_add_child(c,args->items[i]); exprlist_free(args); }
          $$=c; }
    | atom '.' IDENT '(' ')'
        { EXPR_t *c=make_call("raku_mcall");
          expr_add_child(c,$1);
          expr_add_child(c,leaf_sval(E_QLIT,$3)); free($3);
          $$=c; }
    /* RK-26: $obj.field (field read) */
    | atom '.' IDENT
        { EXPR_t *fe=expr_new(E_FIELD);
          fe->sval=(char*)intern($3); free($3);
          expr_add_child(fe,$1);
          $$=fe; }
    /* RK-24: map/grep/sort higher-order list ops */
    /* RK-25: die as expr (try/CATCH are stmt-level) */
    | KW_DIE expr
        { EXPR_t *c=make_call("raku_die");
          expr_add_child(c,$2); $$=c; }
    | KW_MAP closure expr
        { EXPR_t *c=make_call("raku_map");
          expr_add_child(c,$2); expr_add_child(c,$3); $$=c; }
    | KW_GREP closure expr
        { EXPR_t *c=make_call("raku_grep");
          expr_add_child(c,$2); expr_add_child(c,$3); $$=c; }
    | KW_SORT expr
        { EXPR_t *c=make_call("raku_sort");
          expr_add_child(c,$2); $$=c; }
    | KW_SORT closure expr
        { EXPR_t *c=make_call("raku_sort");
          expr_add_child(c,$2); expr_add_child(c,$3); $$=c; }
    | atom           { $$=$1; }
    ;

arg_list
    : expr              { $$=exprlist_append(exprlist_new(),$1); }
    | arg_list ',' expr { $$=exprlist_append($1,$3); }
    ;

atom
    : LIT_INT         { EXPR_t *e=expr_new(E_ILIT); e->ival=$1; $$=e; }
    | LIT_FLOAT       { EXPR_t *e=expr_new(E_FLIT); e->dval=$1; $$=e; }
    | LIT_STR         { $$=leaf_sval(E_QLIT,$1); }
    | LIT_INTERP_STR  { $$=lower_interp_str($1); }
    | VAR_SCALAR      { $$=var_node($1); }
    | VAR_ARRAY       { $$=var_node($1); }
    | VAR_HASH        { $$=var_node($1); }
    | VAR_CAPTURE
        { /* RK-34: $0/$1 positional capture */
          EXPR_t *c=make_call("raku_capture");
          EXPR_t *idx=expr_new(E_ILIT); idx->ival=$1;
          expr_add_child(c,idx); $$=c; }
    | VAR_NAMED_CAPTURE
        { /* RK-35: $<name> named capture */
          EXPR_t *c=make_call("raku_named_capture");
          expr_add_child(c,leaf_sval(E_QLIT,$1)); $$=c; }
    | VAR_ARRAY '[' expr ']'
        { EXPR_t *c=make_call("arr_get"); expr_add_child(c,var_node($1)); expr_add_child(c,$3); $$=c; }
    | VAR_HASH '<' IDENT '>'
        { EXPR_t *c=make_call("hash_get"); expr_add_child(c,var_node($1)); expr_add_child(c,leaf_sval(E_QLIT,$3)); $$=c; }
    | VAR_HASH '{' expr '}'
        { EXPR_t *c=make_call("hash_get"); expr_add_child(c,var_node($1)); expr_add_child(c,$3); $$=c; }
    | KW_EXISTS VAR_HASH '<' IDENT '>'
        { EXPR_t *c=make_call("hash_exists"); expr_add_child(c,var_node($2)); expr_add_child(c,leaf_sval(E_QLIT,$4)); $$=c; }
    | KW_EXISTS VAR_HASH '{' expr '}'
        { EXPR_t *c=make_call("hash_exists"); expr_add_child(c,var_node($2)); expr_add_child(c,$4); $$=c; }
    | IDENT           { $$=var_node($1); }
    /* RK-26: $.field / $!field inside a method → E_FIELD(self, fieldname) */
    | VAR_TWIGIL
        { EXPR_t *fe=expr_new(E_FIELD);
          fe->sval=(char*)intern($1); free($1);
          expr_add_child(fe, leaf_sval(E_VAR, "self"));
          $$=fe; }
    | '(' expr ')'    { $$=$2; }
    ;

%%

/* ── Parse entry (sets up flex buffer and calls yyparse) ─────────────── */
extern void *raku_yy_scan_string(const char *);
extern void  raku_yy_delete_buffer(void *);

Program *raku_parse_string(const char *src) {
    raku_prog_result = NULL;
    void *buf = raku_yy_scan_string(src);
    raku_yyparse();
    raku_yy_delete_buffer(buf);
    return raku_prog_result;
}
