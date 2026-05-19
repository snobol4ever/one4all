%{
#include "rebus.h"
#include "../../ast/ast.h"
#include "../../frontend/snobol4/scrip_cc.h"  /* expr_add_child, expr_binary, expr_unary */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static RProgram *prog;
extern RProgram *rebus_parsed_program;
extern int       rebus_nerrors;

/* PST-RB-5b: parser now builds tree_t directly for all exprs and stmts.
   RDecl still used for declaration structure (body/params now tree_t). */

typedef struct { char **a; int n, cap; } SAL;
/*----------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------*/
static SAL *sal_new(void) {
    SAL *s = calloc(1, sizeof *s);
    s->cap = 4; s->a = malloc(4 * sizeof(char *));
    return s;
}
/*----------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------*/
static void sal_push(SAL *s, char *v) {
    if (s->n >= s->cap) { s->cap *= 2; s->a = realloc(s->a, s->cap * sizeof(char *)); }
    s->a[s->n++] = v;
}

/* Dynamic tree_t child list used during parse for arg/stmt lists */
typedef struct { tree_t **a; int n, cap; } TAL;
/*----------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------*/
static TAL *tal_new(void) {
    TAL *t = calloc(1, sizeof *t);
    t->cap = 4; t->a = malloc(4 * sizeof(tree_t *));
    return t;
}
/*----------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------*/
static void tal_push(TAL *t, tree_t *v) {
    if (t->n >= t->cap) { t->cap *= 2; t->a = realloc(t->a, t->cap * sizeof(tree_t *)); }
    t->a[t->n++] = v;
}

/*----------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------*/
extern int  yylex(void);
/*----------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------*/
extern void yyerror(const char *);
extern int  rebus_yylineno;
#define yylineno rebus_yylineno
%}

%define api.prefix {rebus_yy}
%union {
    char       *sval;
    long        ival;
    double      dval;
    tree_t     *tree;   /* PST: exprs and stmts are tree_t* */
    RDecl      *decl;
    RCase      *rcase;  /* kept temporarily for case clause list */
    void       *sal;    /* SAL* — for string id lists (params/locals/fields) */
    void       *tal;    /* TAL* — for tree_t child lists (args, stmt lists) */
}

%token <sval>  T_IDENT T_STR T_KEYWORD
%token <ival>  T_INT
%token <dval>  T_REAL
%token T_CASE T_DEFAULT T_DO T_ELSE T_END T_EXIT T_FAIL
%token T_FOR T_FROM T_FUNCTION T_BY T_IF T_INITIAL T_LOCAL
%token T_NEXT T_OF T_RECORD T_REPEAT T_RETURN T_STOP
%token T_THEN T_TO T_UNLESS T_UNTIL T_WHILE
%token T_ASSIGN
%token T_EXCHANGE
%token T_ADDASSIGN
%token T_SUBASSIGN
%token T_CATASSIGN
%token T_QUESTMINUS
%token T_ARROW
%token T_STRCAT
%token T_STARSTAR    /* **   */
%token T_NE
%token T_GE
%token T_LE
%token T_SEQ
%token T_SNE
%token T_SGT
%token T_SGE
%token T_SLT
%token T_SLE
%token T_PLUSCOLON

%type <decl>   decl function_decl record_decl
%type <tree>   stmt stmt_body compound_stmt expr_as_stmt
%type <tree>   if_stmt unless_stmt while_stmt until_stmt
%type <tree>   repeat_stmt for_stmt case_stmt
%type <tree>   stmt_list stmt_list_ne
%type <tree>   expr opt_expr
%type <tree>   assign_expr alt_expr cat_expr cmp_expr
%type <tree>   add_expr mul_expr pow_expr unary_expr postfix_expr primary
%type <tree>   pat_expr
%type <sal>    idlist_ne opt_idlist
%type <sal>    opt_locals opt_params
%type <tal>    arglist arglist_ne
%type <tree>   opt_initial
/* case clause list reuses RCase for now — 5c will remove it */
%type <rcase>  caselist caseclause

%expect 1
%nonassoc LOWER_THAN_ELSE
%nonassoc T_ELSE
%right T_ASSIGN T_EXCHANGE T_ADDASSIGN T_SUBASSIGN T_CATASSIGN
%left  '|'
%left  T_STRCAT '&'
%left  '<' T_LE '>' T_GE '=' T_NE T_SEQ T_SNE T_SLT T_SLE T_SGT T_SGE
%left  '+' '-'
%left  '*' '/' '%'
%right T_STARSTAR '^'
%right UMINUS UPLUS UTILDE UBACK USLASH UBANG UAT UDOLLAR UDOT

%%

program
    : decl_list             { }
    ;

decl_list
    :
    | decl_list decl        {
            if ($2) {
                $2->next = NULL;
                if (!prog->decls) prog->decls = $2;
                else {
                    RDecl *d = prog->decls;
                    while (d->next) d = d->next;
                    d->next = $2;
                }
                prog->ndecls++;
            }
        }
    | decl_list ';'         { }
    | decl_list error ';'   { yyerrok; }
    ;

decl
    : function_decl         { $$ = $1; }
    | record_decl           { $$ = $1; }
    ;

opt_semi
    :
    | ';'
    ;

record_decl
    : T_RECORD T_IDENT '(' opt_idlist ')' opt_semi
        {
            RDecl *d   = rdecl_new(RD_RECORD, yylineno);
            d->name    = $2;
            SAL *sl    = $4;
            d->fields  = sl->a;
            d->nfields = sl->n;
            free(sl);
            $$ = d;
        }
    ;

function_decl
    : T_FUNCTION T_IDENT '(' opt_params ')' opt_semi
        opt_locals
        opt_initial
        stmt_list
      T_END
        {
            RDecl *d    = rdecl_new(RD_FUNCTION, yylineno);
            d->name     = $2;
            SAL *ps     = (SAL*)$4;
            d->params   = ps->a;
            d->nparams  = ps->n;
            free(ps);
            SAL *ls     = (SAL*)$7;
            d->locals   = ls->a;
            d->nlocals  = ls->n;
            free(ls);
            /* PST: initial and body are now tree_t* stored in decl_tree fields */
            d->initial_tree = $8;   /* tree_t* or NULL */
            d->body_tree    = $9;   /* tree_t* (TT_PROGRAM) */
            $$ = d;
        }
    ;

opt_params
    :   { $$ = (void*)sal_new(); }
    | idlist_ne     { $$ = $1; }
    ;

opt_locals
    :              { $$ = (void*)sal_new(); }
    | T_LOCAL idlist_ne ';'   { $$ = $2; }
    ;

opt_initial
    :              { $$ = NULL; }
    | T_INITIAL compound_stmt               { $$ = $2; }
    | T_INITIAL stmt ';'                    { $$ = $2; }
    ;

stmt_list
    :   {
            tree_t *p = ast_node_new(TT_PROGRAM);
            $$ = p;
        }
    | stmt_list_ne  { $$ = $1; }
    ;

stmt_list_ne
    : stmt ';'                  {
            tree_t *p = ast_node_new(TT_PROGRAM);
            if ($1) expr_add_child(p, $1);
            $$ = p;
        }
    | compound_stmt             { $$ = $1; }
    | stmt_list_ne stmt ';'     {
            if ($2) expr_add_child($1, $2);
            $$ = $1;
        }
    | stmt_list_ne compound_stmt {
            if ($2) for (int i = 0; i < $2->n; i++) expr_add_child($1, $2->c[i]);
            $$ = $1;
        }
    | stmt_list_ne error ';'    { yyerrok; $$ = $1; }
    ;

idlist_ne
    : T_IDENT               { SAL *s = sal_new(); sal_push(s, $1); $$ = s; }
    | idlist_ne ',' T_IDENT { sal_push($1, $3); $$ = $1; }
    ;

opt_idlist
    :   { $$ = sal_new(); }
    | idlist_ne     { $$ = $1; }
    ;

stmt
    : expr_as_stmt          { $$ = $1; }
    | if_stmt               { $$ = $1; }
    | unless_stmt           { $$ = $1; }
    | while_stmt            { $$ = $1; }
    | until_stmt            { $$ = $1; }
    | repeat_stmt           { $$ = $1; }
    | for_stmt              { $$ = $1; }
    | case_stmt             { $$ = $1; }
    | T_EXIT                { $$ = ast_node_new(TT_LOOP_BREAK); }
    | T_NEXT                { $$ = ast_node_new(TT_LOOP_NEXT); }
    | T_FAIL                { $$ = ast_node_new(TT_PROC_FAIL); }
    | T_STOP                { $$ = ast_node_new(TT_END); }
    | T_RETURN opt_expr     {
            tree_t *r = ast_node_new(TT_RETURN);
            if ($2) expr_add_child(r, $2);
            $$ = r;
        }
    | compound_stmt         { $$ = $1; }
    ;

expr_as_stmt
    : expr                          { $$ = $1; }
    | expr '?' pat_expr             {
            /* match: TT_SCAN c[0]=subject c[1]=pattern */
            tree_t *s = ast_node_new(TT_SCAN);
            expr_add_child(s, $1);
            expr_add_child(s, $3);
            $$ = s;
        }
    | expr '?' pat_expr T_ARROW expr {
            /* replace: TT_SCAN c[0]=subject c[1]=pattern c[2]=replacement */
            tree_t *s = ast_node_new(TT_SCAN);
            expr_add_child(s, $1);
            expr_add_child(s, $3);
            expr_add_child(s, $5);
            $$ = s;
        }
    | expr T_QUESTMINUS pat_expr    {
            /* replace-with-null: TT_SCAN c[0]=subject c[1]=pattern c[2]=TT_NUL */
            tree_t *s = ast_node_new(TT_SCAN);
            expr_add_child(s, $1);
            expr_add_child(s, $3);
            expr_add_child(s, ast_node_new(TT_NUL));
            $$ = s;
        }
    ;

compound_stmt
    : '{' stmt_list '}'     { $$ = $2; }
    ;

stmt_body
    : stmt          { $$ = $1; }
    ;

if_stmt
    : T_IF stmt T_THEN opt_semi stmt_body %prec LOWER_THAN_ELSE
        {
            /* TT_IF c[0]=cond c[1]=then */
            tree_t *n = ast_node_new(TT_IF);
            expr_add_child(n, $2);
            expr_add_child(n, $5);
            $$ = n;
        }
    | T_IF stmt T_THEN opt_semi stmt_body T_ELSE opt_semi stmt_body
        {
            /* TT_IF c[0]=cond c[1]=then c[2]=else */
            tree_t *n = ast_node_new(TT_IF);
            expr_add_child(n, $2);
            expr_add_child(n, $5);
            expr_add_child(n, $8);
            $$ = n;
        }
    ;

unless_stmt
    : T_UNLESS stmt T_THEN opt_semi stmt_body
        {
            tree_t *n = ast_node_new(TT_UNLESS);
            expr_add_child(n, $2);
            expr_add_child(n, $5);
            $$ = n;
        }
    ;

while_stmt
    : T_WHILE stmt T_DO opt_semi stmt_body
        {
            /* TT_WHILE c[0]=cond c[1]=body */
            tree_t *n = ast_node_new(TT_WHILE);
            expr_add_child(n, $2);
            expr_add_child(n, $5);
            $$ = n;
        }
    ;

until_stmt
    : T_UNTIL stmt T_DO opt_semi stmt_body
        {
            /* TT_UNTIL c[0]=cond c[1]=body */
            tree_t *n = ast_node_new(TT_UNTIL);
            expr_add_child(n, $2);
            expr_add_child(n, $5);
            $$ = n;
        }
    ;

repeat_stmt
    : T_REPEAT opt_semi stmt_body
        {
            /* TT_REPEAT c[0]=body */
            tree_t *n = ast_node_new(TT_REPEAT);
            expr_add_child(n, $3);
            $$ = n;
        }
    ;

for_stmt
    : T_FOR T_IDENT T_FROM expr T_TO expr T_DO opt_semi stmt_body
        {
            /* TT_FOR v.sval=var c[0]=from c[1]=to c[2]=TT_NUL c[3]=body */
            tree_t *n = ast_node_new(TT_FOR);
            n->v.sval = strdup($2);
            expr_add_child(n, $4);
            expr_add_child(n, $6);
            expr_add_child(n, ast_node_new(TT_NUL));  /* no 'by' */
            expr_add_child(n, $9);
            $$ = n;
        }
    | T_FOR T_IDENT T_FROM expr T_TO expr T_BY expr T_DO opt_semi stmt_body
        {
            /* TT_FOR v.sval=var c[0]=from c[1]=to c[2]=by c[3]=body */
            tree_t *n = ast_node_new(TT_FOR);
            n->v.sval = strdup($2);
            expr_add_child(n, $4);
            expr_add_child(n, $6);
            expr_add_child(n, $8);
            expr_add_child(n, $11);
            $$ = n;
        }
    ;

case_stmt
    : T_CASE expr T_OF '{' caselist '}'
        {
            /* TT_CASE c[0]=expr, then alternating guard/body pairs:
               c[1]=guard0 c[2]=body0 c[3]=guard1 c[4]=body1 ...
               default clause: guard is TT_NUL. No synthesized TT_IF wrappers. */
            tree_t *cs = ast_node_new(TT_CASE);
            expr_add_child(cs, $2);
            for (RCase *c = $5; c; c = c->next) {
                if (c->is_default) {
                    expr_add_child(cs, ast_node_new(TT_NUL));
                } else {
                    expr_add_child(cs, c->guard_tree);
                }
                expr_add_child(cs, c->body_tree);
            }
            $$ = cs;
        }
    ;

caselist
    : caseclause            { $$ = $1; }
    | caselist ';' caseclause {
            RCase *c = $1; while (c->next) c = c->next;
            c->next = $3; $$ = $1;
        }
    | caselist ';'          { $$ = $1; }
    ;

caseclause
    : expr ':' stmt_body
        {
            RCase *c      = rcase_new(yylineno);
            c->guard_tree = $1;
            c->body_tree  = $3;
            $$ = c;
        }
    | T_DEFAULT ':' stmt_body
        {
            RCase *c      = rcase_new(yylineno);
            c->is_default = 1;
            c->body_tree  = $3;
            $$ = c;
        }
    ;

expr
    : assign_expr           { $$ = $1; }
    ;

assign_expr
    : alt_expr                              { $$ = $1; }
    | alt_expr T_ASSIGN    assign_expr      {
            tree_t *n = ast_node_new(TT_ASSIGN);
            expr_add_child(n, $1); expr_add_child(n, $3); $$ = n;
        }
    | alt_expr T_EXCHANGE  assign_expr      {
            tree_t *n = ast_node_new(TT_SWAP);
            expr_add_child(n, $1); expr_add_child(n, $3); $$ = n;
        }
    | alt_expr T_ADDASSIGN assign_expr      {
            /* lhs +:= rhs => TT_ASSIGN(lhs, TT_ADD(lhs, rhs)) */
            tree_t *lhs2 = ast_node_new(TT_VAR); lhs2->v.sval = strdup($1->v.sval ? $1->v.sval : "");
            tree_t *add  = ast_node_new(TT_ADD);
            expr_add_child(add, $1); expr_add_child(add, $3);
            tree_t *n = ast_node_new(TT_ASSIGN);
            expr_add_child(n, lhs2); expr_add_child(n, add); $$ = n;
        }
    | alt_expr T_SUBASSIGN assign_expr      {
            tree_t *lhs2 = ast_node_new(TT_VAR); lhs2->v.sval = strdup($1->v.sval ? $1->v.sval : "");
            tree_t *sub  = ast_node_new(TT_SUB);
            expr_add_child(sub, $1); expr_add_child(sub, $3);
            tree_t *n = ast_node_new(TT_ASSIGN);
            expr_add_child(n, lhs2); expr_add_child(n, sub); $$ = n;
        }
    | alt_expr T_CATASSIGN assign_expr      {
            tree_t *lhs2 = ast_node_new(TT_VAR); lhs2->v.sval = strdup($1->v.sval ? $1->v.sval : "");
            tree_t *cat  = ast_node_new(TT_CAT);
            expr_add_child(cat, $1); expr_add_child(cat, $3);
            tree_t *n = ast_node_new(TT_ASSIGN);
            expr_add_child(n, lhs2); expr_add_child(n, cat); $$ = n;
        }
    ;

alt_expr
    : cat_expr                              { $$ = $1; }
    | alt_expr '|' cat_expr                 {
            tree_t *n = ast_node_new(TT_ALT);
            expr_add_child(n, $1); expr_add_child(n, $3); $$ = n;
        }
    ;

cat_expr
    : cmp_expr                              { $$ = $1; }
    | cat_expr T_STRCAT cmp_expr            {
            tree_t *n = ast_node_new(TT_CAT);
            expr_add_child(n, $1); expr_add_child(n, $3); $$ = n;
        }
    | cat_expr '&' cmp_expr                 {
            tree_t *n = ast_node_new(TT_CAT);
            expr_add_child(n, $1); expr_add_child(n, $3); $$ = n;
        }
    ;

cmp_expr
    : add_expr                              { $$ = $1; }
    | cmp_expr '='    add_expr              { tree_t *n = ast_node_new(TT_EQ);  expr_add_child(n,$1); expr_add_child(n,$3); $$ = n; }
    | cmp_expr T_NE   add_expr              { tree_t *n = ast_node_new(TT_NE);  expr_add_child(n,$1); expr_add_child(n,$3); $$ = n; }
    | cmp_expr '<'    add_expr              { tree_t *n = ast_node_new(TT_LT);  expr_add_child(n,$1); expr_add_child(n,$3); $$ = n; }
    | cmp_expr T_LE   add_expr              { tree_t *n = ast_node_new(TT_LE);  expr_add_child(n,$1); expr_add_child(n,$3); $$ = n; }
    | cmp_expr '>'    add_expr              { tree_t *n = ast_node_new(TT_GT);  expr_add_child(n,$1); expr_add_child(n,$3); $$ = n; }
    | cmp_expr T_GE   add_expr              { tree_t *n = ast_node_new(TT_GE);  expr_add_child(n,$1); expr_add_child(n,$3); $$ = n; }
    | cmp_expr T_SEQ  add_expr              { tree_t *n = ast_node_new(TT_LEQ); expr_add_child(n,$1); expr_add_child(n,$3); $$ = n; }
    | cmp_expr T_SNE  add_expr              { tree_t *n = ast_node_new(TT_LNE); expr_add_child(n,$1); expr_add_child(n,$3); $$ = n; }
    | cmp_expr T_SLT  add_expr              { tree_t *n = ast_node_new(TT_LLT); expr_add_child(n,$1); expr_add_child(n,$3); $$ = n; }
    | cmp_expr T_SLE  add_expr              { tree_t *n = ast_node_new(TT_LLE); expr_add_child(n,$1); expr_add_child(n,$3); $$ = n; }
    | cmp_expr T_SGT  add_expr              { tree_t *n = ast_node_new(TT_LGT); expr_add_child(n,$1); expr_add_child(n,$3); $$ = n; }
    | cmp_expr T_SGE  add_expr              { tree_t *n = ast_node_new(TT_LGE); expr_add_child(n,$1); expr_add_child(n,$3); $$ = n; }
    ;

add_expr
    : mul_expr                              { $$ = $1; }
    | add_expr '+' mul_expr                 { tree_t *n = ast_node_new(TT_ADD); expr_add_child(n,$1); expr_add_child(n,$3); $$ = n; }
    | add_expr '-' mul_expr                 { tree_t *n = ast_node_new(TT_SUB); expr_add_child(n,$1); expr_add_child(n,$3); $$ = n; }
    ;

mul_expr
    : pow_expr                              { $$ = $1; }
    | mul_expr '*' pow_expr                 { tree_t *n = ast_node_new(TT_MUL); expr_add_child(n,$1); expr_add_child(n,$3); $$ = n; }
    | mul_expr '/' pow_expr                 { tree_t *n = ast_node_new(TT_DIV); expr_add_child(n,$1); expr_add_child(n,$3); $$ = n; }
    | mul_expr '%' pow_expr                 { tree_t *n = ast_node_new(TT_MOD); expr_add_child(n,$1); expr_add_child(n,$3); $$ = n; }
    ;

pow_expr
    : unary_expr                            { $$ = $1; }
    | unary_expr '^'       pow_expr         { tree_t *n = ast_node_new(TT_POW); expr_add_child(n,$1); expr_add_child(n,$3); $$ = n; }
    | unary_expr T_STARSTAR pow_expr        { tree_t *n = ast_node_new(TT_POW); expr_add_child(n,$1); expr_add_child(n,$3); $$ = n; }
    ;

unary_expr
    : postfix_expr                          { $$ = $1; }
    | '-' unary_expr %prec UMINUS           { tree_t *n = ast_node_new(TT_MNS);      expr_add_child(n, $2); $$ = n; }
    | '+' unary_expr %prec UPLUS            { $$ = $2; /* unary plus is identity */ }
    | '~' unary_expr %prec UTILDE           { tree_t *n = ast_node_new(TT_NOT);      expr_add_child(n, $2); $$ = n; }
    | '\\' unary_expr %prec UBACK           { tree_t *n = ast_node_new(TT_NOT);      expr_add_child(n, $2); $$ = n; }
    | '/' unary_expr %prec USLASH           { tree_t *n = ast_node_new(TT_NONNULL);  expr_add_child(n, $2); $$ = n; }
    | '!' unary_expr %prec UBANG            { tree_t *n = ast_node_new(TT_ITERATE);  expr_add_child(n, $2); $$ = n; }
    | '@' T_IDENT %prec UAT                 {
            tree_t *n = ast_node_new(TT_CAPT_CURSOR);
            n->v.sval = strdup($2); $$ = n;
        }
    | '$' unary_expr %prec UDOLLAR          { tree_t *n = ast_node_new(TT_INDIRECT); expr_add_child(n, $2); $$ = n; }
    | '.' unary_expr %prec UDOT             {
            /* prefix dot = conditional capture with implicit subject */
            tree_t *n = ast_node_new(TT_CAPT_COND_ASGN);
            expr_add_child(n, ast_node_new(TT_NUL));
            expr_add_child(n, $2); $$ = n;
        }
    ;

postfix_expr
    : primary                               { $$ = $1; }
    | postfix_expr '(' arglist ')'
        {
            TAL *al = $3;
            tree_t *f;
            /* if callee is a plain TT_VAR, promote to TT_FNC with name */
            if ($1->t == TT_VAR) {
                f = ast_node_new(TT_FNC);
                f->v.sval = $1->v.sval;  /* steal sval */
                $1->v.sval = NULL;
            } else {
                /* indirect call: TT_FNC with no name, first child = callee */
                f = ast_node_new(TT_FNC);
                expr_add_child(f, $1);
            }
            for (int i = 0; i < al->n; i++)
                expr_add_child(f, al->a[i] ? al->a[i] : ast_node_new(TT_NUL));
            free(al->a); free(al);
            $$ = f;
        }
    | postfix_expr '[' arglist ']'
        {
            TAL *al = $3;
            tree_t *idx = ast_node_new(TT_IDX);
            expr_add_child(idx, $1);
            for (int i = 0; i < al->n; i++)
                expr_add_child(idx, al->a[i] ? al->a[i] : ast_node_new(TT_NUL));
            free(al->a); free(al);
            $$ = idx;
        }
    | postfix_expr '[' expr T_PLUSCOLON expr ']'
        {
            /* section: TT_IDX c[0]=base c[1]=start c[2]=len */
            tree_t *idx = ast_node_new(TT_IDX);
            expr_add_child(idx, $1);
            expr_add_child(idx, $3);
            expr_add_child(idx, $5);
            $$ = idx;
        }
    | postfix_expr '.' primary
        {
            tree_t *n = ast_node_new(TT_CAPT_COND_ASGN);
            expr_add_child(n, $1); expr_add_child(n, $3); $$ = n;
        }
    | postfix_expr '$' primary
        {
            tree_t *n = ast_node_new(TT_CAPT_IMMED_ASGN);
            expr_add_child(n, $1); expr_add_child(n, $3); $$ = n;
        }
    ;

primary
    : T_STR         { tree_t *n = ast_node_new(TT_QLIT); n->v.sval = $1; $$ = n; }
    | T_INT         { tree_t *n = ast_node_new(TT_ILIT); n->v.ival = $1; $$ = n; }
    | T_REAL        { tree_t *n = ast_node_new(TT_FLIT); n->v.dval = $1; $$ = n; }
    | T_KEYWORD     { tree_t *n = ast_node_new(TT_KEYWORD); n->v.sval = $1; $$ = n; }
    | T_IDENT       { tree_t *n = ast_node_new(TT_VAR); n->v.sval = $1; $$ = n; }
    | '(' expr ')'  { $$ = $2; }
    ;

pat_expr
    : expr      { $$ = $1; }
    ;

opt_expr
    :   { $$ = NULL; }
    | expr          { $$ = $1; }
    ;

arglist
    :   { $$ = tal_new(); }
    | arglist_ne    { $$ = $1; }
    ;

arglist_ne
    : expr                      { TAL *al = tal_new(); tal_push(al, $1); $$ = al; }
    | arglist_ne ',' expr       { tal_push($1, $3); $$ = $1; }
    | arglist_ne ','            { tal_push($1, NULL); $$ = $1; }
    ;

%%
/*----------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------*/
void rebus_parse_init(void) {
    prog = calloc(1, sizeof *prog);
    rebus_parsed_program = prog;
}
