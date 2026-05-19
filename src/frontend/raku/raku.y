%define api.prefix {raku_yy}
%code requires {
#include "../../ast/ast.h"
#include "../snobol4/scrip_cc.h"
typedef struct ExprList {
    tree_t **items;
    int      count;
    int      cap;
} ExprList;
}
%{
#include "../../ast/ast.h"
#include "../snobol4/scrip_cc.h"
#include "raku.tab.h"
#include "raku_driver.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
/*----------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------*/
extern int  raku_yylex(void);
/*----------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------*/
extern int  raku_get_lineno(void);
/*----------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------*/
void raku_yyerror(const char *msg) {
    fprintf(stderr, "raku parse error line %d: %s\n", raku_get_lineno(), msg);
}
/*----------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------*/
static ExprList *exprlist_new(void) {
    ExprList *l = calloc(1, sizeof *l);
    if (!l) { fprintf(stderr, "raku: OOM\n"); exit(1); }
    return l;
}
/*----------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------*/
static ExprList *exprlist_append(ExprList *l, tree_t *e) {
    if (l->count >= l->cap) {
        l->cap = l->cap ? l->cap * 2 : 8;
        l->items = realloc(l->items, l->cap * sizeof(tree_t *));
        if (!l->items) { fprintf(stderr, "raku: OOM\n"); exit(1); }
    }
    l->items[l->count++] = e;
    return l;
}
/*----------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------*/
static void exprlist_free(ExprList *l) { if (l) { free(l->items); free(l); } }
/*----------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------*/
static const char *strip_sigil(const char *s) {
    if (s && (s[0]=='$'||s[0]=='@'||s[0]=='%')) return s+1;
    return s;
}
/*----------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------*/
static tree_t *leaf_sval(tree_e k, const char *s) {
    tree_t *e = ast_node_new(k); e->v.sval = intern(s); return e;
}
/*----------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------*/
static tree_t *var_node(const char *name) {
    return leaf_sval(TT_VAR, strip_sigil(name));
}
/*----------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------*/
static tree_t *make_call(const char *name) {
    tree_t *e = leaf_sval(TT_FNC, name);
    tree_t *n = ast_node_new(TT_VAR); n->v.sval = intern(name);
    expr_add_child(e, n);
    return e;
}
/*----------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------*/
static tree_t *make_seq(ExprList *stmts) {
    tree_t *seq = ast_node_new(TT_SEQ_EXPR);
    if (stmts) {
        for (int i = 0; i < stmts->count; i++) expr_add_child(seq, stmts->items[i]);
        exprlist_free(stmts);
    }
    return seq;
}
/*----------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------*/
static tree_t *lower_interp_str(const char *s) {
    int len = s ? (int)strlen(s) : 0;
    tree_t *result = NULL;
    char litbuf[4096]; int litpos = 0, i = 0;
    while (i < len) {
        if (s[i]=='$' && i+1<len &&
            (s[i+1]=='_'||(s[i+1]>='A'&&s[i+1]<='Z')||(s[i+1]>='a'&&s[i+1]<='z'))) {
            if (litpos>0) { litbuf[litpos]='\0';
                tree_t *lit=leaf_sval(TT_QLIT,litbuf);
                result=result?expr_binary(TT_CAT,result,lit):lit; litpos=0; }
            i++;
            char vname[256]; int vlen=0;
            while (i<len&&(s[i]=='_'||(s[i]>='A'&&s[i]<='Z')||(s[i]>='a'&&s[i]<='z')||(s[i]>='0'&&s[i]<='9')))
                { if(vlen<255) vname[vlen++]=s[i]; i++; }
            vname[vlen]='\0';
            tree_t *var=leaf_sval(TT_VAR,vname);
            result=result?expr_binary(TT_CAT,result,var):var;
        } else { if(litpos<4095) litbuf[litpos++]=s[i]; i++; }
    }
    if (litpos>0) { litbuf[litpos]='\0';
        tree_t *lit=leaf_sval(TT_QLIT,litbuf);
        result=result?expr_binary(TT_CAT,result,lit):lit; }
    return result ? result : leaf_sval(TT_QLIT,"");
}
/*----------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------*/
static tree_t *make_for_range(tree_t *lo, tree_t *hi, const char *vname, tree_t *body) {
    tree_t *init = expr_binary(TT_ASSIGN, leaf_sval(TT_VAR,vname), lo);
    tree_t *cond = expr_binary(TT_LE, leaf_sval(TT_VAR,vname), hi);
    tree_t *one  = ast_node_new(TT_ILIT); one->v.ival = 1;
    tree_t *incr_rhs = expr_binary(TT_ADD, leaf_sval(TT_VAR,vname), one);
    tree_t *incr = expr_binary(TT_ASSIGN, leaf_sval(TT_VAR,vname), incr_rhs);
    tree_t *body2 = ast_node_new(TT_SEQ_EXPR);
    for (int i = 0; i < body->n; i++) expr_add_child(body2, body->c[i]);
    expr_add_child(body2, incr);
    tree_t *wloop = expr_binary(TT_WHILE, cond, body2);
    tree_t *seq   = ast_node_new(TT_SEQ_EXPR);
    expr_add_child(seq, init); expr_add_child(seq, wloop);
    return seq;
}
tree_t *raku_prog_result = NULL;
/*----------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------*/
static void add_proc(tree_t *e) {
    if (!e) return;
    if (!raku_prog_result) raku_prog_result = ast_stmt_new(TT_PROGRAM);
    tree_t *st = ast_stmt_new(TT_STMT);
    expr_add_child(st, ast_attr_int(":lang", LANG_RAKU));
    expr_add_child(st, ast_attr_int(":line", 0));
    expr_add_child(st, ast_attr_int(":stno", 0));
    expr_add_child(st, ast_attr_expr(":subj", e));
    expr_add_child(raku_prog_result, st);
}
#define RAKU_METH_MAX 256
typedef struct { char key[128]; char procname[128]; } RakuMethEntry;
static RakuMethEntry raku_meth_table[RAKU_METH_MAX];
static int           raku_meth_ntypes = 0;
/*----------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------*/
static void raku_meth_register(const char *classname, const char *methname, const char *procname) {
    if (raku_meth_ntypes >= RAKU_METH_MAX) return;
    RakuMethEntry *e = &raku_meth_table[raku_meth_ntypes++];
    snprintf(e->key,      sizeof e->key,      "%s::%s", classname, methname);
    snprintf(e->procname, sizeof e->procname,  "%s",     procname);
}
/*----------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------*/
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
    tree_t  *node;
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
            if (all) {
                for (int i = 0; i < all->count; i++)
                    if (all->items[i]) add_proc(all->items[i]);
                exprlist_free(all);
            }
        }
    ;
stmt_list
    :    { $$ = exprlist_new(); }
    | stmt_list stmt { $$ = exprlist_append($1, $2); }
    ;
stmt
    : KW_MY VAR_SCALAR '=' expr ';'
        { $$ = expr_binary(TT_ASSIGN, var_node($2), $4); }
    | KW_MY VAR_ARRAY '=' expr ';'
        { $$ = expr_binary(TT_ASSIGN, var_node($2), $4); }
    | KW_MY VAR_HASH '=' expr ';'
        { $$ = expr_binary(TT_ASSIGN, var_node($2), $4); }
    | KW_MY IDENT VAR_SCALAR '=' expr ';'
        { tree_t *e=ast_node_new(TT_DECL); ast_push(e,leaf_sval(TT_VAR,$2)); free($2); ast_push(e,var_node($3)); ast_push(e,$5); $$=e; }
    | KW_MY IDENT VAR_ARRAY '=' expr ';'
        { tree_t *e=ast_node_new(TT_DECL); ast_push(e,leaf_sval(TT_VAR,$2)); free($2); ast_push(e,var_node($3)); ast_push(e,$5); $$=e; }
    | KW_MY IDENT VAR_HASH '=' expr ';'
        { tree_t *e=ast_node_new(TT_DECL); ast_push(e,leaf_sval(TT_VAR,$2)); free($2); ast_push(e,var_node($3)); ast_push(e,$5); $$=e; }
    | KW_MY IDENT VAR_SCALAR ';'
        { tree_t *e=ast_node_new(TT_DECL); ast_push(e,leaf_sval(TT_VAR,$2)); free($2); ast_push(e,var_node($3)); $$=e; }
    | KW_MY IDENT VAR_ARRAY ';'
        { tree_t *e=ast_node_new(TT_DECL); ast_push(e,leaf_sval(TT_VAR,$2)); free($2); ast_push(e,var_node($3)); $$=e; }
    | KW_MY IDENT VAR_HASH ';'
        { tree_t *e=ast_node_new(TT_DECL); ast_push(e,leaf_sval(TT_VAR,$2)); free($2); ast_push(e,var_node($3)); $$=e; }
    | KW_SAY expr ';'
        { tree_t *c=ast_node_new(TT_SAY); expr_add_child(c,$2); $$=c; }
    | KW_SAY '(' expr ',' expr ')' ';'
        { tree_t *c=ast_node_new(TT_SAY_FH); expr_add_child(c,$3); expr_add_child(c,$5); $$=c; }
    | KW_PRINT expr ';'
        { tree_t *c=ast_node_new(TT_PRINT); expr_add_child(c,$2); $$=c; }
    | KW_PRINT '(' expr ',' expr ')' ';'
        { tree_t *c=ast_node_new(TT_PRINT_FH); expr_add_child(c,$3); expr_add_child(c,$5); $$=c; }
    | KW_TAKE expr ';'
        { $$=expr_unary(TT_SUSPEND,$2); }
    | KW_RETURN expr ';'
        { tree_t *r=ast_node_new(TT_RETURN); expr_add_child(r,$2); $$=r; }
    | KW_RETURN ';'
        { $$=ast_node_new(TT_RETURN); }
    | VAR_SCALAR '=' expr ';'
        { $$=expr_binary(TT_ASSIGN,var_node($1),$3); }
    | VAR_SCALAR '.' IDENT '=' expr ';'
        { tree_t *fe=ast_node_new(TT_FIELD);
          fe->v.sval=(char*)intern($3); free($3);
          expr_add_child(fe,var_node($1));
          $$=expr_binary(TT_ASSIGN,fe,$5); }
    | VAR_ARRAY '[' expr ']' '=' expr ';'
        { tree_t *c=ast_node_new(TT_ARR_SET);
          ast_push(c,var_node($1)); ast_push(c,$3); ast_push(c,$6); $$=c; }
    | VAR_HASH '<' IDENT '>' '=' expr ';'
        { tree_t *c=ast_node_new(TT_HASH_SET);
          ast_push(c,var_node($1)); ast_push(c,leaf_sval(TT_QLIT,$3)); ast_push(c,$6); $$=c; }
    | VAR_HASH '{' expr '}' '=' expr ';'
        { tree_t *c=ast_node_new(TT_HASH_SET);
          ast_push(c,var_node($1)); ast_push(c,$3); ast_push(c,$6); $$=c; }
    | KW_DELETE VAR_HASH '<' IDENT '>' ';'
        { tree_t *c=ast_node_new(TT_HASH_DELETE);
          ast_push(c,var_node($2)); ast_push(c,leaf_sval(TT_QLIT,$4)); $$=c; }
    | KW_DELETE VAR_HASH '{' expr '}' ';'
        { tree_t *c=ast_node_new(TT_HASH_DELETE);
          ast_push(c,var_node($2)); ast_push(c,$4); $$=c; }
    | expr ';' { $$=$1; }
    | if_stmt           { $$=$1; }
    | while_stmt        { $$=$1; }
    | for_stmt          { $$=$1; }
    | given_stmt        { $$=$1; }
    | KW_TRY block
        { tree_t *e=ast_node_new(TT_TRY); ast_push(e,$2); $$=e; }
    | KW_TRY block KW_CATCH block
        { tree_t *e=ast_node_new(TT_TRY); ast_push(e,$2); ast_push(e,$4); $$=e; }
    | unless_stmt       { $$=$1; }
    | until_stmt        { $$=$1; }
    | repeat_stmt       { $$=$1; }
    | sub_decl          { $$=$1; }
    | class_decl        { $$=$1; }
    ;
if_stmt
    : KW_IF '(' expr ')' block
        { tree_t *e=ast_node_new(TT_IF); expr_add_child(e,$3); expr_add_child(e,$5); $$=e; }
    | KW_IF '(' expr ')' block KW_ELSE block
        { tree_t *e=ast_node_new(TT_IF); expr_add_child(e,$3); expr_add_child(e,$5); expr_add_child(e,$7); $$=e; }
    | KW_IF '(' expr ')' block KW_ELSE if_stmt
        { tree_t *e=ast_node_new(TT_IF); expr_add_child(e,$3); expr_add_child(e,$5); expr_add_child(e,$7); $$=e; }
    ;
while_stmt
    : KW_WHILE '(' expr ')' block
        { $$=expr_binary(TT_WHILE,$3,$5); }
    ;
unless_stmt
    : KW_UNLESS '(' expr ')' block
        { tree_t *e=ast_node_new(TT_UNLESS); ast_push(e,$3); ast_push(e,$5); $$=e; }
    | KW_UNLESS '(' expr ')' block KW_ELSE block
        { tree_t *e=ast_node_new(TT_UNLESS); ast_push(e,$3); ast_push(e,$5); ast_push(e,$7); $$=e; }
    ;
until_stmt
    : KW_UNTIL '(' expr ')' block
        { tree_t *e=ast_node_new(TT_UNTIL); expr_add_child(e,$3); expr_add_child(e,$5); $$=e; }
    ;
repeat_stmt
    : KW_REPEAT block
        { tree_t *e=ast_node_new(TT_REPEAT); expr_add_child(e,$2); $$=e; }
    ;
for_stmt
    : KW_FOR add_expr OP_RANGE add_expr OP_ARROW VAR_SCALAR block
        { const char *vn = intern(strip_sigil($6)); free($6);
          tree_t *r = ast_node_new(TT_FOR_RANGE);
          ast_push(r, leaf_sval(TT_VAR, vn)); ast_push(r, $2); ast_push(r, $4); ast_push(r, $7);
          tree_t *ex = ast_node_new(TT_ILIT); ex->v.ival = 0; ast_push(r, ex);
          $$ = r; }
    | KW_FOR add_expr OP_RANGE_EX add_expr OP_ARROW VAR_SCALAR block
        { const char *vn = intern(strip_sigil($6)); free($6);
          tree_t *r = ast_node_new(TT_FOR_RANGE);
          ast_push(r, leaf_sval(TT_VAR, vn)); ast_push(r, $2); ast_push(r, $4); ast_push(r, $7);
          tree_t *ex = ast_node_new(TT_ILIT); ex->v.ival = 1; ast_push(r, ex);
          $$ = r; }
    | KW_FOR expr OP_ARROW VAR_SCALAR block
        { const char *vn = intern(strip_sigil($4)); free($4);
          tree_t *gen = expr_unary(TT_ITERATE, $2);
          gen->v.sval = (char *)vn;
          $$ = expr_binary(TT_EVERY, gen, $5); }
    | KW_FOR expr block
        { tree_t *gen = expr_unary(TT_ITERATE, $2);
          $$ = expr_binary(TT_EVERY, gen, $3); }
    ;
given_stmt
    : KW_GIVEN expr '{' when_list '}'
        { /* PRF-12-given: when_list is now flat [val0, body0, val1, body1, ...] — no pair nodes. */
          tree_t *ec=ast_node_new(TT_CASE);
          expr_add_child(ec,$2);
          ExprList *whens=$4;
          for(int i=0;i<whens->count;i++) expr_add_child(ec,whens->items[i]);
          exprlist_free(whens);
          $$=ec; }
    | KW_GIVEN expr '{' when_list KW_DEFAULT block '}'
        {
          tree_t *ec=ast_node_new(TT_CASE);
          expr_add_child(ec,$2);
          ExprList *whens=$4;
          for(int i=0;i<whens->count;i++) expr_add_child(ec,whens->items[i]);
          exprlist_free(whens);
          expr_add_child(ec,ast_node_new(TT_NUL)); expr_add_child(ec,$6);
          $$=ec; }
    ;
when_list
    :  { $$=exprlist_new(); }
    | when_list KW_WHEN expr block
        { /* PRF-12-given: push val and body directly — no intermediate TT_SEQ_EXPR pair. */
          exprlist_append($1,$3); exprlist_append($1,$4);
          $$=$1; }
    ;
sub_decl
    : KW_SUB IDENT '(' param_list ')' block
        { ExprList *params=$4; int np=params?params->count:0;
          tree_t *e=leaf_sval(TT_SUB_DECL,$2); e->v.ival=(long long)np;
          tree_t *nn=ast_node_new(TT_VAR); nn->v.sval=intern($2); expr_add_child(e,nn);
          if(params){ for(int i=0;i<np;i++) expr_add_child(e,params->items[i]); exprlist_free(params); }
          tree_t *body=$6;
          for(int i=0;i<body->n;i++) expr_add_child(e,body->c[i]);
          $$=e; }
    | KW_SUB IDENT '(' ')' block
        { tree_t *e=leaf_sval(TT_SUB_DECL,$2); e->v.ival=(long long)0;
          tree_t *nn=ast_node_new(TT_VAR); nn->v.sval=intern($2); expr_add_child(e,nn);
          tree_t *body=$5;
          for(int i=0;i<body->n;i++) expr_add_child(e,body->c[i]);
          $$=e; }
    ;
class_decl
    : KW_CLASS IDENT '{' class_body_list '}'
        {
            const char *cname = intern($2); free($2);
            ExprList *body = $4;
            tree_t *cd = ast_node_new(TT_CLASS_DECL);
            ast_push(cd, leaf_sval(TT_VAR, cname));
            if (body) {
                for (int i = 0; i < body->count; i++)
                    if (body->items[i]) ast_push(cd, body->items[i]);
                exprlist_free(body);
            }
            $$ = cd;
        }
    ;
class_body_list
    :  { $$ = exprlist_new(); }
    | class_body_list KW_HAS VAR_TWIGIL ';'
        { tree_t *fv = leaf_sval(TT_VAR, $3); free($3);
          $$ = exprlist_append($1, fv); }
    | class_body_list KW_HAS VAR_SCALAR ';'
        { tree_t *fv = leaf_sval(TT_VAR, strip_sigil($3)); free($3);
          $$ = exprlist_append($1, fv); }
    | class_body_list KW_METHOD IDENT '(' param_list ')' block
        { ExprList *params = $5; int np = params ? params->count : 0;
          tree_t *e = ast_node_new(TT_SUB_DECL);
          e->v.ival = (long long)(np + 1);
          tree_t *nn = ast_node_new(TT_VAR); nn->v.sval = intern($3); expr_add_child(e, nn);
          if (params) { for (int i = 0; i < np; i++) expr_add_child(e, params->items[i]); exprlist_free(params); }
          tree_t *body = $7;
          for (int i = 0; i < body->n; i++) expr_add_child(e, body->c[i]);
          free($3);
          $$ = exprlist_append($1, e); }
    | class_body_list KW_METHOD IDENT '(' ')' block
        { tree_t *e = ast_node_new(TT_SUB_DECL);
          e->v.ival = (long long)(1);
          tree_t *nn = ast_node_new(TT_VAR); nn->v.sval = intern($3); expr_add_child(e, nn);
          tree_t *body = $6;
          for (int i = 0; i < body->n; i++) expr_add_child(e, body->c[i]);
          free($3);
          $$ = exprlist_append($1, e); }
    ;
named_arg_list
    : IDENT OP_FATARROW expr
        { $$ = exprlist_new();
          exprlist_append($$, leaf_sval(TT_QLIT, $1)); free($1);
          exprlist_append($$, $3); }
    | named_arg_list ',' IDENT OP_FATARROW expr
        { exprlist_append($1, leaf_sval(TT_QLIT, $3)); free($3);
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
closure
    : '{' expr '}'  { $$=$2; }
    ;
expr
    : VAR_SCALAR '=' expr  { $$=expr_binary(TT_ASSIGN,var_node($1),$3); }
    | KW_GATHER block      {
          tree_t *g = ast_node_new(TT_GATHER);
          tree_t *blk = $2;
          for (int i = 0; i < blk->n; i++) expr_add_child(g, blk->c[i]);
          $$ = g;
      }
    | cmp_expr             { $$=$1; }
    ;
cmp_expr
    : cmp_expr OP_AND add_expr  { $$=expr_binary(TT_SEQ,$1,$3); }
    | cmp_expr OP_OR  add_expr  { $$=expr_binary(TT_ALT,$1,$3); }
    | add_expr OP_EQ  add_expr  { $$=expr_binary(TT_EQ,$1,$3); }
    | add_expr OP_NE  add_expr  { $$=expr_binary(TT_NE,$1,$3); }
    | add_expr '<'    add_expr  { $$=expr_binary(TT_LT,$1,$3); }
    | add_expr '>'    add_expr  { $$=expr_binary(TT_GT,$1,$3); }
    | add_expr OP_LE  add_expr  { $$=expr_binary(TT_LE,$1,$3); }
    | add_expr OP_GE  add_expr  { $$=expr_binary(TT_GE,$1,$3); }
    | add_expr OP_SEQ add_expr  { $$=expr_binary(TT_LEQ,$1,$3); }
    | add_expr OP_SNE add_expr  { $$=expr_binary(TT_LNE,$1,$3); }
    | add_expr OP_SMATCH LIT_REGEX
        { tree_t *c = ast_node_new(TT_SMATCH);
          ast_push(c, $1);
          ast_push(c, leaf_sval(TT_QLIT, $3));
          ast_push(c, leaf_sval(TT_QLIT, "match"));
          $$ = c; }
    | add_expr OP_SMATCH LIT_MATCH_GLOBAL
        { tree_t *c = ast_node_new(TT_SMATCH);
          ast_push(c, $1);
          ast_push(c, leaf_sval(TT_QLIT, $3));
          ast_push(c, leaf_sval(TT_QLIT, "match_global"));
          $$ = c; }
    | add_expr OP_SMATCH LIT_SUBST
        { tree_t *c = ast_node_new(TT_SMATCH);
          ast_push(c, $1);
          ast_push(c, leaf_sval(TT_QLIT, $3));
          ast_push(c, leaf_sval(TT_QLIT, "subst"));
          $$ = c; }
    | range_expr               { $$=$1; }
    ;
range_expr
    : add_expr OP_RANGE    add_expr { $$=expr_binary(TT_TO,$1,$3); }
    | add_expr OP_RANGE_EX add_expr { $$=expr_binary(TT_TO,$1,$3); }
    | add_expr                      { $$=$1; }
    ;
add_expr
    : add_expr '+' mul_expr  { $$=expr_binary(TT_ADD,$1,$3); }
    | add_expr '-' mul_expr  { $$=expr_binary(TT_SUB,$1,$3); }
    | add_expr '~' mul_expr  { $$=expr_binary(TT_CAT,$1,$3); }
    | mul_expr               { $$=$1; }
    ;
mul_expr
    : mul_expr '*'    unary_expr  { $$=expr_binary(TT_MUL,$1,$3); }
    | mul_expr '/'    unary_expr  { $$=expr_binary(TT_DIV,$1,$3); }
    | mul_expr '%'    unary_expr  { $$=expr_binary(TT_MOD,$1,$3); }
    | mul_expr OP_DIV unary_expr  { $$=expr_binary(TT_DIV,$1,$3); }
    | unary_expr                  { $$=$1; }
    ;
unary_expr
    : '-' unary_expr %prec UMINUS  { $$=expr_unary(TT_MNS,$2); }
    | '!' unary_expr               { $$=expr_unary(TT_NOT,$2); }
    | postfix_expr                 { $$=$1; }
    ;
postfix_expr : call_expr { $$=$1; } ;
call_expr
    : IDENT '(' arg_list ')'
        { tree_t *e=make_call($1);
          ExprList *args=$3;
          if(args){ for(int i=0;i<args->count;i++) expr_add_child(e,args->items[i]); exprlist_free(args); }
          $$=e; }
    | IDENT '(' ')'  { $$=make_call($1); }
    | IDENT '.' KW_NEW '(' named_arg_list ')'
        { tree_t *c = ast_node_new(TT_NEW);
          ast_push(c, leaf_sval(TT_QLIT, $1)); free($1);
          ExprList *nargs = $5;
          if (nargs) { for (int i = 0; i < nargs->count; i++) ast_push(c, nargs->items[i]); exprlist_free(nargs); }
          $$ = c; }
    | IDENT '.' KW_NEW '(' ')'
        { tree_t *c = ast_node_new(TT_NEW);
          ast_push(c, leaf_sval(TT_QLIT, $1)); free($1);
          $$ = c; }
    | atom '.' IDENT '(' arg_list ')'
        { tree_t *c = ast_node_new(TT_METHCALL);
          ast_push(c, $1);
          ast_push(c, leaf_sval(TT_QLIT, $3)); free($3);
          ExprList *args = $5;
          if (args) { for (int i = 0; i < args->count; i++) ast_push(c, args->items[i]); exprlist_free(args); }
          $$ = c; }
    | atom '.' IDENT '(' ')'
        { tree_t *c = ast_node_new(TT_METHCALL);
          ast_push(c, $1);
          ast_push(c, leaf_sval(TT_QLIT, $3)); free($3);
          $$ = c; }
    | atom '.' IDENT
        { tree_t *fe=ast_node_new(TT_FIELD);
          fe->v.sval=(char*)intern($3); free($3);
          expr_add_child(fe,$1);
          $$=fe; }
    | KW_DIE expr
        { tree_t *d=ast_node_new(TT_DIE); expr_add_child(d,$2); $$=d; }
    | KW_MAP closure expr
        { tree_t *c = ast_node_new(TT_MAP);  ast_push(c, $2); ast_push(c, $3); $$ = c; }
    | KW_GREP closure expr
        { tree_t *c = ast_node_new(TT_GREP); ast_push(c, $2); ast_push(c, $3); $$ = c; }
    | KW_SORT expr
        { tree_t *c = ast_node_new(TT_SORT); ast_push(c, $2); $$ = c; }
    | KW_SORT closure expr
        { tree_t *c = ast_node_new(TT_SORT); ast_push(c, $2); ast_push(c, $3); $$ = c; }
    | atom           { $$=$1; }
    ;
arg_list
    : expr              { $$=exprlist_append(exprlist_new(),$1); }
    | arg_list ',' expr { $$=exprlist_append($1,$3); }
    ;
atom
    : LIT_INT         { tree_t *e=ast_node_new(TT_ILIT); e->v.ival=$1; $$=e; }
    | LIT_FLOAT       { tree_t *e=ast_node_new(TT_FLIT); e->v.dval=$1; $$=e; }
    | LIT_STR         { $$=leaf_sval(TT_QLIT,$1); }
    | LIT_INTERP_STR  { $$=lower_interp_str($1); }
    | VAR_SCALAR      { $$=var_node($1); }
    | VAR_ARRAY       { $$=var_node($1); }
    | VAR_HASH        { $$=var_node($1); }
    | VAR_CAPTURE
        {
          tree_t *c=make_call("raku_capture");
          tree_t *idx=ast_node_new(TT_ILIT); idx->v.ival=$1;
          expr_add_child(c,idx); $$=c; }
    | VAR_NAMED_CAPTURE
        {
          tree_t *c=make_call("raku_named_capture");
          expr_add_child(c,leaf_sval(TT_QLIT,$1)); $$=c; }
    | VAR_ARRAY '[' expr ']'
        { tree_t *c=ast_node_new(TT_ARR_GET); ast_push(c,var_node($1)); ast_push(c,$3); $$=c; }
    | VAR_HASH '<' IDENT '>'
        { tree_t *c=ast_node_new(TT_HASH_GET); ast_push(c,var_node($1)); ast_push(c,leaf_sval(TT_QLIT,$3)); $$=c; }
    | VAR_HASH '{' expr '}'
        { tree_t *c=ast_node_new(TT_HASH_GET); ast_push(c,var_node($1)); ast_push(c,$3); $$=c; }
    | KW_EXISTS VAR_HASH '<' IDENT '>'
        { tree_t *c=ast_node_new(TT_HASH_EXISTS); ast_push(c,var_node($2)); ast_push(c,leaf_sval(TT_QLIT,$4)); $$=c; }
    | KW_EXISTS VAR_HASH '{' expr '}'
        { tree_t *c=ast_node_new(TT_HASH_EXISTS); ast_push(c,var_node($2)); ast_push(c,$4); $$=c; }
    | IDENT           { $$=var_node($1); }
    | VAR_TWIGIL
        { tree_t *fe=ast_node_new(TT_FIELD);
          fe->v.sval=(char*)intern($1); free($1);
          expr_add_child(fe, leaf_sval(TT_VAR, "self"));
          $$=fe; }
    | '(' expr ')'    { $$=$2; }
    ;
%%
/*----------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------*/
extern void *raku_yy_scan_string(const char *);
/*----------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------*/
extern void  raku_yy_delete_buffer(void *);
/*----------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------*/
/* Gather hoist pass: walk tree recursively, replace TT_GATHER[body...] with TT_FNC call; collect defs. */
static int   g_gather_seq  = 0;
static tree_t *g_gather_defs[256];
static int    g_gather_ndef = 0;
/*----------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------*/
static void raku_hoist_gather_in_expr(tree_t *e) {
    if (!e) return;
    for (int i = 0; i < e->n; i++) raku_hoist_gather_in_expr(e->c[i]);
    if (e->t != TT_GATHER) return;
    char gname[32]; snprintf(gname, sizeof gname, "__gather_%d", g_gather_seq++);
    tree_t *def = ast_node_new(TT_SUB_DECL); def->v.sval = intern(gname);
    tree_t *dn  = ast_node_new(TT_VAR); dn->v.sval = intern(gname);
    expr_add_child(def, dn);
    for (int i = 0; i < e->n; i++) expr_add_child(def, e->c[i]);
    if (g_gather_ndef < 256) g_gather_defs[g_gather_ndef++] = def;
    e->t      = TT_FNC;
    e->v.sval = intern(gname);
    e->n      = 0;
    e->c      = NULL;
    tree_t *cn = ast_node_new(TT_VAR); cn->v.sval = intern(gname);
    expr_add_child(e, cn);
}
/*----------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------*/
static void raku_lower_hoist_gather_pass(tree_t *prog) {
    if (!prog) return;
    g_gather_seq  = 0;
    g_gather_ndef = 0;
    for (int i = 0; i < prog->n; i++) {
        tree_t *st = prog->c[i];
        if (!st || st->t != TT_STMT) continue;
        for (int j = 0; j < st->n; j++) {
            tree_t *ch = st->c[j];
            if (!ch) continue;
            if (ch->t == TT_ATTR) { for (int k = 0; k < ch->n; k++) raku_hoist_gather_in_expr(ch->c[k]); continue; }
            raku_hoist_gather_in_expr(ch);
        }
    }
    if (!g_gather_ndef) return;
    int old_n = prog->n;
    int new_n = old_n + g_gather_ndef;
    size_t new_cap = (size_t)new_n;
    char *block = (char *)malloc(sizeof(size_t) + new_cap * sizeof(tree_t *));
    tree_t **new_c = (tree_t **)(block + sizeof(size_t));
    *(size_t *)block = new_cap;
    for (int i = 0; i < g_gather_ndef; i++) {
        tree_t *st = ast_stmt_new(TT_STMT);
        expr_add_child(st, ast_attr_int(":lang",  LANG_RAKU));
        expr_add_child(st, ast_attr_int(":line",  0));
        expr_add_child(st, ast_attr_int(":stno",  0));
        expr_add_child(st, ast_attr_expr(":subj", g_gather_defs[i]));
        new_c[i] = st;
    }
    for (int i = 0; i < old_n; i++) new_c[g_gather_ndef + i] = prog->c[i];
    if (prog->c) free((char *)prog->c - sizeof(size_t));
    prog->c = new_c;
    prog->n = new_n;
}
/*----------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------*/
tree_t *raku_parse_string(const char *src) {
    raku_prog_result = NULL;
    void *buf = raku_yy_scan_string(src);
    raku_yyparse();
    raku_yy_delete_buffer(buf);
    raku_lower_hoist_gather_pass(raku_prog_result);
    return raku_prog_result;
}
