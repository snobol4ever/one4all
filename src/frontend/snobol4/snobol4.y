%code requires {
#include "scrip_cc.h"
#include "snobol4.h"
}
%code {
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
typedef struct { CODE_t *prog; AST_t **result; } PP;
static void     sno4_stmt_commit_go(void*,Token,AST_t*,AST_t*,int,AST_t*,AST_t*,AST_t*,AST_t*);
static Lex     *g_lx;
static void     fixup_val(AST_t*);
static int      is_pat(AST_t*);
static AST_t  *parse_expr(Lex*);
/* pat_prim_kind: map pattern primitive name → typed IR kind; AST_VAR = not a prim */
static AST_e pat_prim_kind(const char *s) {
    if (!s) return AST_VAR;
    static const struct { const char *n; AST_e k; } m[] = {
        {"ANY",AST_ANY},{"NOTANY",AST_NOTANY},{"SPAN",AST_SPAN},{"BREAK",AST_BREAK},{"BREAKX",AST_BREAKX},
        {"LEN",AST_LEN},{"POS",AST_POS},{"RPOS",AST_RPOS},{"TAB",AST_TAB},{"RTAB",AST_RTAB},
        {"ARB",AST_ARB},{"ARBNO",AST_ARBNO},{"REM",AST_REM},{"FAIL",AST_FAIL},{"SUCCEED",AST_SUCCEED},
        {"FENCE",AST_FENCE},{"ABORT",AST_ABORT},{"BAL",AST_BAL},{NULL,AST_VAR}
    };
    for (int i = 0; m[i].n; i++) if (strcasecmp(s, m[i].n) == 0) return m[i].k;
    return AST_VAR;
}
}
%define api.prefix {snobol4_}
%define api.pure full
%parse-param { void *yyparse_param }
%union { AST_t *expr; Token tok; }

/* Atoms */
%token <tok> T_IDENT T_FUNCTION T_KEYWORD T_END T_INT T_REAL T_STR
/* Statement structure */
%token <tok> T_LABEL T_GOTO_S T_GOTO_F T_GOTO_LPAREN T_GOTO_RPAREN T_STMT_END
/* Binary operators (WHITE op WHITE) */
%token T_2EQUAL T_2QUEST T_2PIPE T_2PLUS T_2MINUS
%token T_2STAR T_2SLASH T_2CARET
%token T_2DOLLAR T_2DOT
%token T_2AMP T_2AT T_2POUND T_2PERCENT T_2TILDE
/* Unary operators (no leading space) — named per SPITBOL Chapter 15 */
%token T_1AT T_1TILDE T_1QUEST T_1AMP
%token T_1PLUS T_1MINUS T_1STAR T_1DOLLAR T_1DOT
%token T_1BANG T_1PERCENT T_1SLASH T_1POUND
%token T_1EQUAL T_1PIPE
/* Structural */
%token T_CONCAT T_COMMA T_LPAREN T_RPAREN T_LBRACK T_RBRACK T_LANGLE T_RANGLE

%type <expr> expr0 expr2 expr3 expr4 expr5 expr6 expr7 expr8
%type <expr> expr9 expr10 expr11 expr12 expr13 expr14 expr15 expr17
%type <expr> exprlist exprlist_ne opt_subject opt_pattern opt_repl
%type <expr> goto_label_expr
%type <expr> goto_expr goto_atom

%%
top        : program                                                                                { }
           | /* empty */                                                                            { }
           ;
program    : program stmt | stmt                                                                    ;
stmt
             : T_LABEL opt_subject opt_repl T_STMT_END                                     { sno4_stmt_commit_go(yyparse_param,$1,$2,NULL,($3!=NULL),$3,NULL,NULL,NULL); }
             | T_LABEL opt_subject opt_repl goto_label_expr T_STMT_END                     { sno4_stmt_commit_go(yyparse_param,$1,$2,NULL,($3!=NULL),$3,$4,NULL,NULL); }
             | T_LABEL opt_subject opt_repl T_GOTO_S goto_label_expr T_STMT_END            { sno4_stmt_commit_go(yyparse_param,$1,$2,NULL,($3!=NULL),$3,NULL,$5,NULL); }
             | T_LABEL opt_subject opt_repl T_GOTO_F goto_label_expr T_STMT_END            { sno4_stmt_commit_go(yyparse_param,$1,$2,NULL,($3!=NULL),$3,NULL,NULL,$5); }
             | T_LABEL opt_subject opt_repl T_GOTO_S goto_label_expr T_GOTO_F goto_label_expr T_STMT_END { sno4_stmt_commit_go(yyparse_param,$1,$2,NULL,($3!=NULL),$3,NULL,$5,$7); }
             | T_LABEL opt_subject opt_repl T_GOTO_F goto_label_expr T_GOTO_S goto_label_expr T_STMT_END { sno4_stmt_commit_go(yyparse_param,$1,$2,NULL,($3!=NULL),$3,NULL,$7,$5); }
           | T_LABEL expr2 T_2QUEST opt_pattern opt_repl T_STMT_END                      { sno4_stmt_commit_go(yyparse_param,$1,expr_binary(AST_SCAN,$2,$4),NULL,($5!=NULL),$5,NULL,NULL,NULL); }
           | T_LABEL expr2 T_2QUEST opt_pattern opt_repl goto_label_expr T_STMT_END      { sno4_stmt_commit_go(yyparse_param,$1,expr_binary(AST_SCAN,$2,$4),NULL,($5!=NULL),$5,$6,NULL,NULL); }
           | T_LABEL expr2 T_2QUEST opt_pattern opt_repl T_GOTO_S goto_label_expr T_STMT_END { sno4_stmt_commit_go(yyparse_param,$1,expr_binary(AST_SCAN,$2,$4),NULL,($5!=NULL),$5,NULL,$7,NULL); }
           | T_LABEL expr2 T_2QUEST opt_pattern opt_repl T_GOTO_F goto_label_expr T_STMT_END { sno4_stmt_commit_go(yyparse_param,$1,expr_binary(AST_SCAN,$2,$4),NULL,($5!=NULL),$5,NULL,NULL,$7); }
           | T_LABEL expr2 T_2QUEST opt_pattern opt_repl T_GOTO_S goto_label_expr T_GOTO_F goto_label_expr T_STMT_END { sno4_stmt_commit_go(yyparse_param,$1,expr_binary(AST_SCAN,$2,$4),NULL,($5!=NULL),$5,NULL,$7,$9); }
           | T_LABEL expr2 T_2QUEST opt_pattern opt_repl T_GOTO_F goto_label_expr T_GOTO_S goto_label_expr T_STMT_END { sno4_stmt_commit_go(yyparse_param,$1,expr_binary(AST_SCAN,$2,$4),NULL,($5!=NULL),$5,NULL,$9,$7); }
           | unlabeled_stmt
           ;
unlabeled_stmt
             : opt_subject opt_repl T_STMT_END                                             { sno4_stmt_commit_go(yyparse_param,((Token){NULL,0,0,0}),$1,NULL,($2!=NULL),$2,NULL,NULL,NULL); }
             | opt_subject opt_repl goto_label_expr T_STMT_END                             { sno4_stmt_commit_go(yyparse_param,((Token){NULL,0,0,0}),$1,NULL,($2!=NULL),$2,$3,NULL,NULL); }
             | opt_subject opt_repl T_GOTO_S goto_label_expr T_STMT_END                    { sno4_stmt_commit_go(yyparse_param,((Token){NULL,0,0,0}),$1,NULL,($2!=NULL),$2,NULL,$4,NULL); }
             | opt_subject opt_repl T_GOTO_F goto_label_expr T_STMT_END                    { sno4_stmt_commit_go(yyparse_param,((Token){NULL,0,0,0}),$1,NULL,($2!=NULL),$2,NULL,NULL,$4); }
             | opt_subject opt_repl T_GOTO_S goto_label_expr T_GOTO_F goto_label_expr T_STMT_END { sno4_stmt_commit_go(yyparse_param,((Token){NULL,0,0,0}),$1,NULL,($2!=NULL),$2,NULL,$4,$6); }
             | opt_subject opt_repl T_GOTO_F goto_label_expr T_GOTO_S goto_label_expr T_STMT_END { sno4_stmt_commit_go(yyparse_param,((Token){NULL,0,0,0}),$1,NULL,($2!=NULL),$2,NULL,$6,$4); }
           | expr2 T_2QUEST opt_pattern opt_repl T_STMT_END                              { sno4_stmt_commit_go(yyparse_param,((Token){NULL,0,0,0}),expr_binary(AST_SCAN,$1,$3),NULL,($4!=NULL),$4,NULL,NULL,NULL); }
           | expr2 T_2QUEST opt_pattern opt_repl goto_label_expr T_STMT_END              { sno4_stmt_commit_go(yyparse_param,((Token){NULL,0,0,0}),expr_binary(AST_SCAN,$1,$3),NULL,($4!=NULL),$4,$5,NULL,NULL); }
           | expr2 T_2QUEST opt_pattern opt_repl T_GOTO_S goto_label_expr T_STMT_END     { sno4_stmt_commit_go(yyparse_param,((Token){NULL,0,0,0}),expr_binary(AST_SCAN,$1,$3),NULL,($4!=NULL),$4,NULL,$6,NULL); }
           | expr2 T_2QUEST opt_pattern opt_repl T_GOTO_F goto_label_expr T_STMT_END     { sno4_stmt_commit_go(yyparse_param,((Token){NULL,0,0,0}),expr_binary(AST_SCAN,$1,$3),NULL,($4!=NULL),$4,NULL,NULL,$6); }
           | expr2 T_2QUEST opt_pattern opt_repl T_GOTO_S goto_label_expr T_GOTO_F goto_label_expr T_STMT_END { sno4_stmt_commit_go(yyparse_param,((Token){NULL,0,0,0}),expr_binary(AST_SCAN,$1,$3),NULL,($4!=NULL),$4,NULL,$6,$8); }
           | expr2 T_2QUEST opt_pattern opt_repl T_GOTO_F goto_label_expr T_GOTO_S goto_label_expr T_STMT_END { sno4_stmt_commit_go(yyparse_param,((Token){NULL,0,0,0}),expr_binary(AST_SCAN,$1,$3),NULL,($4!=NULL),$4,NULL,$8,$6); }
           ;
opt_subject: expr3                                                                                { $$=$1; }
           | /* empty */                                                                           { $$=NULL; }
           ;
opt_pattern: expr3                                                                                 { $$=$1; }
           | /* empty */                                                                           { $$=NULL; }
           ;
opt_repl   : T_2EQUAL expr0                                                                   { $$=$2; }
           | T_2EQUAL                                                                          { AST_t*e=expr_new(AST_QLIT);e->sval=strdup("");$$=e; }
           | /* empty */                                                                           { $$=NULL; }
           ;

/* goto_atom / goto_expr: mini expression inside $(…) in a goto field.
 * Only atoms that GT state can lex: T_STR, T_IDENT, T_FUNCTION, T_END.
 * Implicit concat via T_CONCAT (whitespace between atoms). */
goto_atom  : T_STR   { AST_t*e=expr_new(AST_QLIT);  e->sval=(char*)$1.sval; $$=e; }
           | T_IDENT  { AST_t*e=expr_new(AST_VAR);   e->sval=(char*)$1.sval; $$=e; }
           | T_FUNCTION { AST_t*e=expr_new(AST_VAR); e->sval=(char*)$1.sval; $$=e; }
           | T_END    { AST_t*e=expr_new(AST_VAR);   e->sval=(char*)$1.sval; $$=e; }
           ;
goto_expr  : goto_atom                          { $$=$1; }
           | goto_expr T_CONCAT goto_atom       { if($1->kind==AST_SEQ){expr_add_child($1,$3);$$=$1;}else{AST_t*s=expr_new(AST_SEQ);expr_add_child(s,$1);expr_add_child(s,$3);$$=s;} }
           ;

/* goto_label_expr: returns AST_t* — AST_QLIT(sval=label) for a plain label,
 * or the computed AST_t* directly for $(expr) targets. No struct. */
goto_label_expr
           : T_GOTO_LPAREN T_IDENT T_GOTO_RPAREN                                             { AST_t*e=expr_new(AST_QLIT);e->sval=strdup($2.sval);$$=e; }
           | T_GOTO_LPAREN T_END T_GOTO_RPAREN                                               { AST_t*e=expr_new(AST_QLIT);e->sval=strdup($2.sval);$$=e; }
           | T_GOTO_LPAREN T_FUNCTION T_GOTO_RPAREN                                          { AST_t*e=expr_new(AST_QLIT);e->sval=strdup($2.sval);$$=e; }
           | T_GOTO_LPAREN T_1DOLLAR T_IDENT T_GOTO_RPAREN                                   { AST_t*e=expr_new(AST_QLIT);char buf[512];snprintf(buf,sizeof buf,"$%s",$3.sval);e->sval=strdup(buf);$$=e; }
           | T_GOTO_LPAREN T_1DOLLAR T_GOTO_LPAREN goto_expr T_GOTO_RPAREN T_GOTO_RPAREN    { $$=$4; }
           | T_GOTO_LPAREN T_1DOLLAR T_STR T_GOTO_RPAREN                                     { AST_t*e=expr_new(AST_QLIT);e->sval=strdup($3.sval);$$=e; }
           ;

/* Expression grammar — levels match beauty.sno Expr0–Expr17 and SPITBOL manual priorities */
expr0      : expr2 T_2EQUAL expr0                                                             { $$=expr_binary(AST_ASSIGN,          $1,$3); }
           | expr2 T_2QUEST      expr0                                                             { $$=expr_binary(AST_SCAN,            $1,$3); }
           | expr2                                                                                 { $$=$1; }
           ;
expr2      : expr2 T_2AMP  expr3                                                             { AST_t*_e=expr_binary(AST_OPSYN,$1,$3); _e->sval=strdup("&"); $$=_e; }
           | expr3                                                                                 { $$=$1; }
           ;
expr3      : expr3 T_2PIPE expr4                                                            { if($1->kind==AST_ALT){expr_add_child($1,$3);$$=$1;}else{AST_t*a=expr_new(AST_ALT);expr_add_child(a,$1);expr_add_child(a,$3);$$=a;} }
           | expr4                                                                                 { $$=$1; }
           ;
expr4      : expr4 T_CONCAT expr5                                                                           { if($1->kind==AST_SEQ){expr_add_child($1,$3);$$=$1;}else{AST_t*s=expr_new(AST_SEQ);expr_add_child(s,$1);expr_add_child(s,$3);$$=s;} }
           | expr5                                                                                 { $$=$1; }
           ;
expr5      : expr5 T_2AT    expr6                                                             { AST_t*_e=expr_binary(AST_OPSYN,$1,$3); _e->sval=strdup("@"); $$=_e; }
           | expr6                                                                                 { $$=$1; }
           ;
expr6      : expr6 T_2PLUS   expr7                                                             { $$=expr_binary(AST_ADD,             $1,$3); }
           | expr6 T_2MINUS expr7                                                            { $$=expr_binary(AST_SUB,             $1,$3); }
           | expr7                                                                                 { $$=$1; }
           ;
expr7      : expr7 T_2POUND      expr8                                                             { $$=expr_binary(AST_MUL,             $1,$3); }
           | expr8                                                                                 { $$=$1; }
           ;
expr8      : expr8 T_2SLASH   expr9                                                             { $$=expr_binary(AST_DIV,             $1,$3); }
           | expr9                                                                                 { $$=$1; }
           ;
expr9      : expr9 T_2STAR expr10                                                        { $$=expr_binary(AST_MUL,             $1,$3); }
           | expr10                                                                                { $$=$1; }
           ;
expr10     : expr10 T_2PERCENT   expr11                                                            { $$=expr_binary(AST_DIV,             $1,$3); }
           | expr11                                                                                { $$=$1; }
           ;
expr11     : expr12 T_2CARET expr11                                                       { $$=expr_binary(AST_POW,             $1,$3); }
           | expr12                                                                                { $$=$1; }
           ;
expr12     : expr12 T_2DOLLAR expr13                                                     { $$=expr_binary(AST_CAPT_IMMED_ASGN,$1,$3); }
           | expr12 T_2DOT      expr13                                                     { $$=expr_binary(AST_CAPT_COND_ASGN, $1,$3); }
           | expr13                                                                                { $$=$1; }
           ;
expr13     : expr14 T_2TILDE     expr13                                                            { AST_t*_e=expr_binary(AST_OPSYN,$1,$3); _e->sval=strdup("~"); $$=_e; }
           | expr14                                                                                { $$=$1; }
           ;
expr14     : T_1AT      expr14                                                             { $$=expr_unary(AST_CAPT_CURSOR,     $2); }
           | T_1TILDE        expr14                                                             { $$=expr_unary(AST_NOT,             $2); }
           | T_1QUEST expr14                                                            { $$=expr_unary(AST_INTERROGATE,     $2); }
           | T_1AMP    expr14                                                             { AST_t*_e=expr_unary(AST_OPSYN,$2); _e->sval=strdup("&"); $$=_e; }
           | T_1PLUS         expr14                                                             { $$=expr_unary(AST_PLS,             $2); }
           | T_1MINUS        expr14                                                             { $$=expr_unary(AST_MNS,             $2); }
           | T_1STAR     expr14                                                             { $$=expr_unary(AST_DEFER,           $2); }
           | T_1DOLLAR  expr14                                                             { $$=expr_unary(AST_INDIRECT,        $2); }
           | T_1DOT       expr14                                                             { $$=expr_unary(AST_NAME,            $2); }
           | T_1BANG  expr14                                                             { $$=expr_unary(AST_POW,             $2); }  /* user-definable */
           | T_1PERCENT      expr14                                                             { $$=expr_unary(AST_DIV,             $2); }  /* user-definable */
           | T_1SLASH        expr14                                                             { $$=expr_unary(AST_DIV,             $2); }  /* user-definable */
           | T_1POUND        expr14                                                             { $$=expr_unary(AST_MUL,             $2); }  /* user-definable */
           | T_1EQUAL        expr14                                                             { $$=expr_unary(AST_ASSIGN,          $2); }  /* user-definable */
           | T_1PIPE expr14                                                             { AST_t*_e=expr_unary(AST_OPSYN,$2); _e->sval=strdup("|"); $$=_e; }  /* user-definable */
           | expr15                                                                                { $$=$1; }
           ;
expr15     : expr15 T_LBRACK exprlist T_RBRACK                                                  { AST_t*i=expr_new(AST_IDX);expr_add_child(i,$1);for(int j=0;j<$3->nchildren;j++)expr_add_child(i,$3->children[j]);free($3->children);free($3);$$=i; }
           | expr15 T_LANGLE exprlist T_RANGLE                                                  { AST_t*i=expr_new(AST_IDX);expr_add_child(i,$1);for(int j=0;j<$3->nchildren;j++)expr_add_child(i,$3->children[j]);free($3->children);free($3);$$=i; }
           | expr17                                                                                { $$=$1; }
           ;
exprlist   : exprlist_ne                                                                           { $$=$1; }
           | /* empty */                                                                           { $$=expr_new(AST_NUL); }
           ;
exprlist_ne: exprlist_ne T_COMMA expr0                                                            { expr_add_child($1,$3);$$=$1; }
           | exprlist_ne T_COMMA                                                                  { expr_add_child($1,expr_new(AST_NUL));$$=$1; }
           | expr0                                                                                 { AST_t*l=expr_new(AST_NUL);expr_add_child(l,$1);$$=l; }
           ;
expr17     : T_LPAREN expr0 T_RPAREN                                                            { $$=$2; }
           | T_LPAREN expr0 T_COMMA exprlist_ne T_RPAREN                                       { AST_t*a=expr_new(AST_VLIST);expr_add_child(a,$2);for(int i=0;i<$4->nchildren;i++)expr_add_child(a,$4->children[i]);free($4->children);free($4);$$=a; }
           | T_LPAREN T_RPAREN                                                                  { $$=expr_new(AST_NUL); }
           | T_FUNCTION T_LPAREN exprlist T_RPAREN                                             { AST_e _k=pat_prim_kind($1.sval);AST_t*e=expr_new(_k==AST_VAR?AST_FNC:_k);if(_k==AST_VAR)e->sval=(char*)$1.sval;for(int i=0;i<$3->nchildren;i++)expr_add_child(e,$3->children[i]);free($3->children);free($3);$$=e; }
           | T_IDENT                                                                              { AST_t*e=expr_new(AST_VAR);e->sval=(char*)$1.sval;$$=e; }
           | T_END                                                                                { AST_t*e=expr_new(AST_VAR);    e->sval=(char*)$1.sval;$$=e; }
           | T_KEYWORD                                                                            { AST_t*e=expr_new(AST_KEYWORD);e->sval=(char*)$1.sval;$$=e; }
           | T_STR                                                                                { AST_t*e=expr_new(AST_QLIT);   e->sval=(char*)$1.sval;$$=e; }
           | T_INT                                                                                { AST_t*e=expr_new(AST_ILIT);   e->ival=$1.ival;$$=e; }
           | T_REAL                                                                               { AST_t*e=expr_new(AST_FLIT);   e->dval=$1.dval;$$=e; }
           ;
%%
int snobol4_lex(YYSTYPE *yylval_param, void *yyparse_param) {
    (void)yyparse_param; Token t=lex_next(g_lx); yylval_param->tok=t;
    if (getenv("SNO_TOK_TRACE"))
        fprintf(stderr,"[TOK %d sval=%s ival=%ld]\n",t.kind,t.sval?t.sval:"",t.ival);
    return t.kind;
}
void snobol4_error(void *p,const char *msg){(void)p;sno_error(g_lx?g_lx->lineno:0,"parse error: %s",msg);}
static void fixup_val(AST_t *e){ (void)e; /* SNOBOL4: no-op — AST_SEQ never converted to AST_CAT; runtime handles both */ }
static int is_pat(AST_t *e){
    if(!e) return 0;
    switch(e->kind){case AST_ARB:case AST_ARBNO:case AST_CAPT_COND_ASGN:case AST_CAPT_IMMED_ASGN:case AST_CAPT_CURSOR:case AST_DEFER:return 1;default:break;}
    for(int i=0;i<e->nchildren;i++) if(is_pat(e->children[i])) return 1;
    return 0;
}
/* sno4_stmt_commit_go: go is AST_t* AST_SEQ encoding S/F/U goto parts (RS-1) */
/* sno4_stmt_commit_go: gu/gs/gf are AST_t* from goto_label_expr —
 * AST_QLIT(sval=label) for a plain label, any other kind for a computed target. */
static void sno4_stmt_commit_go(void *param,Token lbl,AST_t *subj,AST_t *pat,int has_eq,AST_t *repl,AST_t *gu,AST_t *gs,AST_t *gf){
    PP *pp=(PP*)param;
    if(lbl.sval&&strcasecmp(lbl.sval,"EXPORT")==0){
        if(subj&&subj->kind==AST_VAR&&subj->sval){
            ExportEntry*e=calloc(1,sizeof*e);e->name=strdup(subj->sval);
            for(char*p=e->name;*p;p++)*p=(char)toupper((unsigned char)*p);
            e->next=pp->prog->exports;pp->prog->exports=e;
        } return;
    }
    if(lbl.sval&&strcasecmp(lbl.sval,"IMPORT")==0){
        ImportEntry*e=calloc(1,sizeof*e);const char*n=subj&&subj->sval?subj->sval:"";
        char*dot1=strchr(n,'.');
        if(dot1){char*dot2=strchr(dot1+1,'.');
            if(dot2){e->lang=strndup(n,(size_t)(dot1-n));e->name=strndup(dot1+1,(size_t)(dot2-dot1-1));e->method=strdup(dot2+1);}
            else{e->lang=strdup("");e->name=strndup(n,(size_t)(dot1-n));e->method=strdup(dot1+1);}}
        else{e->lang=strdup("");e->name=strdup(n);e->method=strdup(n);}
        e->next=pp->prog->imports;pp->prog->imports=e;return;
    }
    STMT_t *s=stmt_new();s->lineno=lbl.lineno;
    /* SN-26-bridge-coverage-j: assign source stno at parse time so backward
     * gotos report the correct stno (not a linear execution counter). 1-based.
     * Increment nstmts FIRST, then read it — counts blank statements too
     * (they go through this same commit_go path with empty subj/pat/repl). */
    s->stno = ++pp->prog->nstmts;
    if(lbl.sval){s->label=strdup(lbl.sval);s->is_end=lbl.ival||(strcmp(lbl.sval,"END")==0);}
    /* S=PR split: AST_SCAN(subj, pat) from "X ? PAT" binary match operator */
    if(!pat && subj && subj->kind==AST_SCAN && subj->nchildren==2) {
        AST_t *orig = subj;
        subj = orig->children[0];
        pat  = orig->children[1];
    }
    /* S=PR split: if subj is AST_SEQ with first child a bare name, split into
     * subject=first_child, pattern=rest. Grammar puts everything in opt_subject. */
    if(!pat && subj && (subj->kind==AST_SEQ) && subj->nchildren>=2) {
        AST_t *first = subj->children[0];
        if(first->kind==AST_VAR || first->kind==AST_KEYWORD || first->kind==AST_QLIT || first->kind==AST_INDIRECT) {
            int nc = subj->nchildren - 1;
            AST_t *rest;
            if(nc == 1) {
                rest = subj->children[1];
            } else {
                rest = expr_new(AST_SEQ);
                for(int i=1;i<subj->nchildren;i++) expr_add_child(rest,subj->children[i]);
            }
            subj = first;
            pat  = rest;
        }
    }
    s->subject=subj; s->pattern=pat;
    if(s->subject) fixup_val(s->subject);
    if(has_eq){s->has_eq=1;s->replacement=repl;if(repl&&!is_pat(repl))fixup_val(repl);}
    /* goto fields: gu/gs/gf are AST_QLIT(sval=label) for plain labels, else computed. */
    if(gu){ if(gu->kind==AST_QLIT) s->goto_u=gu->sval; else s->goto_u_expr=gu; }
    if(gs){ if(gs->kind==AST_QLIT) s->goto_s=gs->sval; else s->goto_s_expr=gs; }
    if(gf){ if(gf->kind==AST_QLIT) s->goto_f=gf->sval; else s->goto_f_expr=gf; }
    if(!pp->prog->head) pp->prog->head=pp->prog->tail=s; else{pp->prog->tail->next=s;pp->prog->tail=s;}
    /* nstmts already incremented above when assigning s->stno (SN-26-bridge-coverage-j) */
}
static AST_t *parse_expr(Lex *lx){
    CODE_t *prog=calloc(1,sizeof*prog);PP p={prog,NULL};g_lx=lx;snobol4_parse(&p);
    return prog->head?prog->head->subject:NULL;
}
CODE_t *parse_program_tokens(Lex *stream){
    CODE_t *prog=calloc(1,sizeof*prog);PP p={prog,NULL};g_lx=stream;snobol4_parse(&p);return prog;
}
CODE_t *parse_program(LineArray *lines){(void)lines;return calloc(1,sizeof(CODE_t));}
AST_t *parse_expr_from_str(const char *src){
    if(!src||!*src) return NULL;Lex lx={0};lex_open_str(&lx,src,(int)strlen(src),0);return parse_expr(&lx);
}
/* parse_expr_pat_from_str — parse a bare expression string using the bison
 * parser in BODY start state (lex_open_str). Returns s->pattern if the
 * scan-split fired, else s->subject. Used by _eval_str_impl_fn and snobol4_pattern.c. */
AST_t *parse_expr_pat_from_str(const char *src) {
    if (!src || !*src) return NULL;
    int slen = (int)strlen(src);
    char *buf = malloc(slen + 2);
    if (!buf) return NULL;
    memcpy(buf, src, slen);
    buf[slen]   = '\n';
    buf[slen+1] = '\0';
    Lex lx = {0};
    lex_open_str(&lx, buf, slen + 1, 0);
    CODE_t *prog = calloc(1, sizeof(CODE_t));
    PP p = {prog, NULL};
    g_lx = &lx;
    snobol4_parse(&p);
    free(buf);
    if (!prog->head) return NULL;
    STMT_t *s = prog->head;
    if (s->pattern) return s->pattern;
    return s->subject;
}
/* sno_parse_string — parse a multi-statement SNOBOL4 string via bison.
 * Uses lex_open_str_initial (INITIAL/col-1 start) so indented and labelled
 * statements are handled correctly.  lex_open_str pushes BODY — correct for
 * single-expression parsing only. */
CODE_t *sno_parse_string(const char *src) {
    if (!src) return calloc(1, sizeof(CODE_t));
    int slen = (int)strlen(src);
    char *buf = malloc(slen + 2);
    if (!buf) return calloc(1, sizeof(CODE_t));
    memcpy(buf, src, slen);
    buf[slen]   = '\n';
    buf[slen+1] = '\0';
    Lex lx = {0};
    lex_open_str_initial(&lx, buf, slen + 1, 0);
    CODE_t *prog = calloc(1, sizeof(CODE_t));
    PP p = {prog, NULL};
    g_lx = &lx;
    snobol4_parse(&p);
    free(buf);
    return prog;
}
