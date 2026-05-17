%code requires {
#include "scrip_cc.h"
#include "snobol4.h"
}
%code {
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
typedef struct { CODE_t *prog; tree_t **result; tree_t *ast_prog; } PP;
/*----------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------*/
static void     sno4_stmt_commit_go(void*,Token,tree_t*,tree_t*,int,tree_t*,tree_t*,tree_t*,tree_t*);
static Lex     *g_lx;
/*----------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------*/
static void     fixup_val(tree_t*);
/*----------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------*/
static int      is_pat(tree_t*);
/*----------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------*/
static tree_t  *parse_expr(Lex*);
/*----------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------*/
static tree_e pat_prim_kind(const char *s) {
    if (!s) return TT_VAR;
    static const struct { const char *n; tree_e k; } m[] = {
        {"ANY",TT_ANY},{"NOTANY",TT_NOTANY},{"SPAN",TT_SPAN},{"BREAK",TT_BREAK},{"BREAKX",TT_BREAKX},
        {"LEN",TT_LEN},{"POS",TT_POS},{"RPOS",TT_RPOS},{"TAB",TT_TAB},{"RTAB",TT_RTAB},
        {"ARB",TT_ARB},{"ARBNO",TT_ARBNO},{"REM",TT_REM},{"FAIL",TT_FAIL},{"SUCCEED",TT_SUCCEED},
        {"FENCE",TT_FENCE},{"ABORT",TT_ABORT},{"BAL",TT_BAL},{NULL,TT_VAR}
    };
    for (int i = 0; m[i].n; i++) if (strcmp(s, m[i].n) == 0) return m[i].k;
    return TT_VAR;
}
}
%define api.prefix {snobol4_}
%define api.pure full
%parse-param { void *yyparse_param }
%union { tree_t *expr; Token tok; }
%token <tok> T_IDENT T_FUNCTION T_KEYWORD T_END T_INT T_REAL T_STR
%token <tok> T_LABEL T_GOTO_S T_GOTO_F T_GOTO_LPAREN T_GOTO_RPAREN T_STMT_END
%token T_2EQUAL T_2QUEST T_2PIPE T_2PLUS T_2MINUS
%token T_2STAR T_2SLASH T_2CARET
%token T_2DOLLAR T_2DOT
%token T_2AMP T_2AT T_2POUND T_2PERCENT T_2TILDE
%token T_1AT T_1TILDE T_1QUEST T_1AMP
%token T_1PLUS T_1MINUS T_1STAR T_1DOLLAR T_1DOT
%token T_1BANG T_1PERCENT T_1SLASH T_1POUND
%token T_1EQUAL T_1PIPE
%token T_CONCAT T_COMMA T_LPAREN T_RPAREN T_LBRACK T_RBRACK T_LANGLE T_RANGLE
%type <expr> expr0 expr2 expr3 expr4 expr5 expr6 expr7 expr8
%type <expr> expr9 expr10 expr11 expr12 expr13 expr14 expr15 expr17
%type <expr> exprlist exprlist_ne opt_subject opt_pattern opt_repl
%type <expr> goto_label_expr
%type <expr> goto_expr goto_atom
%%
top        : program                                                                                { }
           |                                                                            { }
           ;
program    : program stmt | stmt                                                                    ;
stmt
             : T_LABEL opt_subject opt_repl T_STMT_END                                     { sno4_stmt_commit_go(yyparse_param,$1,$2,NULL,($3!=NULL),$3,NULL,NULL,NULL); }
             | T_LABEL opt_subject opt_repl goto_label_expr T_STMT_END                     { sno4_stmt_commit_go(yyparse_param,$1,$2,NULL,($3!=NULL),$3,$4,NULL,NULL); }
             | T_LABEL opt_subject opt_repl T_GOTO_S goto_label_expr T_STMT_END            { sno4_stmt_commit_go(yyparse_param,$1,$2,NULL,($3!=NULL),$3,NULL,$5,NULL); }
             | T_LABEL opt_subject opt_repl T_GOTO_F goto_label_expr T_STMT_END            { sno4_stmt_commit_go(yyparse_param,$1,$2,NULL,($3!=NULL),$3,NULL,NULL,$5); }
             | T_LABEL opt_subject opt_repl T_GOTO_S goto_label_expr T_GOTO_F goto_label_expr T_STMT_END { sno4_stmt_commit_go(yyparse_param,$1,$2,NULL,($3!=NULL),$3,NULL,$5,$7); }
             | T_LABEL opt_subject opt_repl T_GOTO_F goto_label_expr T_GOTO_S goto_label_expr T_STMT_END { sno4_stmt_commit_go(yyparse_param,$1,$2,NULL,($3!=NULL),$3,NULL,$7,$5); }
           | T_LABEL expr2 T_2QUEST opt_pattern opt_repl T_STMT_END                      { sno4_stmt_commit_go(yyparse_param,$1,expr_binary(TT_SCAN,$2,$4),NULL,($5!=NULL),$5,NULL,NULL,NULL); }
           | T_LABEL expr2 T_2QUEST opt_pattern opt_repl goto_label_expr T_STMT_END      { sno4_stmt_commit_go(yyparse_param,$1,expr_binary(TT_SCAN,$2,$4),NULL,($5!=NULL),$5,$6,NULL,NULL); }
           | T_LABEL expr2 T_2QUEST opt_pattern opt_repl T_GOTO_S goto_label_expr T_STMT_END { sno4_stmt_commit_go(yyparse_param,$1,expr_binary(TT_SCAN,$2,$4),NULL,($5!=NULL),$5,NULL,$7,NULL); }
           | T_LABEL expr2 T_2QUEST opt_pattern opt_repl T_GOTO_F goto_label_expr T_STMT_END { sno4_stmt_commit_go(yyparse_param,$1,expr_binary(TT_SCAN,$2,$4),NULL,($5!=NULL),$5,NULL,NULL,$7); }
           | T_LABEL expr2 T_2QUEST opt_pattern opt_repl T_GOTO_S goto_label_expr T_GOTO_F goto_label_expr T_STMT_END { sno4_stmt_commit_go(yyparse_param,$1,expr_binary(TT_SCAN,$2,$4),NULL,($5!=NULL),$5,NULL,$7,$9); }
           | T_LABEL expr2 T_2QUEST opt_pattern opt_repl T_GOTO_F goto_label_expr T_GOTO_S goto_label_expr T_STMT_END { sno4_stmt_commit_go(yyparse_param,$1,expr_binary(TT_SCAN,$2,$4),NULL,($5!=NULL),$5,NULL,$9,$7); }
           | unlabeled_stmt
           ;
unlabeled_stmt
             : opt_subject opt_repl T_STMT_END                                             { sno4_stmt_commit_go(yyparse_param,((Token){NULL,0,0,0}),$1,NULL,($2!=NULL),$2,NULL,NULL,NULL); }
             | opt_subject opt_repl goto_label_expr T_STMT_END                             { sno4_stmt_commit_go(yyparse_param,((Token){NULL,0,0,0}),$1,NULL,($2!=NULL),$2,$3,NULL,NULL); }
             | opt_subject opt_repl T_GOTO_S goto_label_expr T_STMT_END                    { sno4_stmt_commit_go(yyparse_param,((Token){NULL,0,0,0}),$1,NULL,($2!=NULL),$2,NULL,$4,NULL); }
             | opt_subject opt_repl T_GOTO_F goto_label_expr T_STMT_END                    { sno4_stmt_commit_go(yyparse_param,((Token){NULL,0,0,0}),$1,NULL,($2!=NULL),$2,NULL,NULL,$4); }
             | opt_subject opt_repl T_GOTO_S goto_label_expr T_GOTO_F goto_label_expr T_STMT_END { sno4_stmt_commit_go(yyparse_param,((Token){NULL,0,0,0}),$1,NULL,($2!=NULL),$2,NULL,$4,$6); }
             | opt_subject opt_repl T_GOTO_F goto_label_expr T_GOTO_S goto_label_expr T_STMT_END { sno4_stmt_commit_go(yyparse_param,((Token){NULL,0,0,0}),$1,NULL,($2!=NULL),$2,NULL,$6,$4); }
           | expr2 T_2QUEST opt_pattern opt_repl T_STMT_END                              { sno4_stmt_commit_go(yyparse_param,((Token){NULL,0,0,0}),expr_binary(TT_SCAN,$1,$3),NULL,($4!=NULL),$4,NULL,NULL,NULL); }
           | expr2 T_2QUEST opt_pattern opt_repl goto_label_expr T_STMT_END              { sno4_stmt_commit_go(yyparse_param,((Token){NULL,0,0,0}),expr_binary(TT_SCAN,$1,$3),NULL,($4!=NULL),$4,$5,NULL,NULL); }
           | expr2 T_2QUEST opt_pattern opt_repl T_GOTO_S goto_label_expr T_STMT_END     { sno4_stmt_commit_go(yyparse_param,((Token){NULL,0,0,0}),expr_binary(TT_SCAN,$1,$3),NULL,($4!=NULL),$4,NULL,$6,NULL); }
           | expr2 T_2QUEST opt_pattern opt_repl T_GOTO_F goto_label_expr T_STMT_END     { sno4_stmt_commit_go(yyparse_param,((Token){NULL,0,0,0}),expr_binary(TT_SCAN,$1,$3),NULL,($4!=NULL),$4,NULL,NULL,$6); }
           | expr2 T_2QUEST opt_pattern opt_repl T_GOTO_S goto_label_expr T_GOTO_F goto_label_expr T_STMT_END { sno4_stmt_commit_go(yyparse_param,((Token){NULL,0,0,0}),expr_binary(TT_SCAN,$1,$3),NULL,($4!=NULL),$4,NULL,$6,$8); }
           | expr2 T_2QUEST opt_pattern opt_repl T_GOTO_F goto_label_expr T_GOTO_S goto_label_expr T_STMT_END { sno4_stmt_commit_go(yyparse_param,((Token){NULL,0,0,0}),expr_binary(TT_SCAN,$1,$3),NULL,($4!=NULL),$4,NULL,$8,$6); }
           ;
opt_subject: expr3                                                                                { $$=$1; }
           |                                                                           { $$=NULL; }
           ;
opt_pattern: expr3                                                                                 { $$=$1; }
           |                                                                           { $$=NULL; }
           ;
opt_repl   : T_2EQUAL expr0                                                                   { $$=$2; }
           | T_2EQUAL                                                                          { tree_t*e=ast_node_new(TT_QLIT);e->v.sval=strdup("");$$=e; }
           |                                                                           { $$=NULL; }
           ;
goto_atom  : T_STR   { tree_t*e=ast_node_new(TT_QLIT);  e->v.sval=(char*)$1.sval; $$=e; }
           | T_IDENT  { tree_t*e=ast_node_new(TT_VAR);   e->v.sval=(char*)$1.sval; $$=e; }
           | T_FUNCTION { tree_t*e=ast_node_new(TT_VAR); e->v.sval=(char*)$1.sval; $$=e; }
           | T_END    { tree_t*e=ast_node_new(TT_VAR);   e->v.sval=(char*)$1.sval; $$=e; }
           ;
goto_expr  : goto_atom                          { $$=$1; }
           | goto_expr T_CONCAT goto_atom       { tree_t*s=ast_node_new(TT_SEQ);expr_add_child(s,$1);expr_add_child(s,$3);$$=s; }
           ;
goto_label_expr
           : T_GOTO_LPAREN T_IDENT T_GOTO_RPAREN                                             { tree_t*e=ast_node_new(TT_QLIT);e->v.sval=strdup($2.sval);$$=e; }
           | T_GOTO_LPAREN T_END T_GOTO_RPAREN                                               { tree_t*e=ast_node_new(TT_QLIT);e->v.sval=strdup($2.sval);$$=e; }
           | T_GOTO_LPAREN T_FUNCTION T_GOTO_RPAREN                                          { tree_t*e=ast_node_new(TT_QLIT);e->v.sval=strdup($2.sval);$$=e; }
           | T_GOTO_LPAREN T_1DOLLAR T_IDENT T_GOTO_RPAREN                                   { tree_t*e=ast_node_new(TT_QLIT);char buf[512];snprintf(buf,sizeof buf,"$%s",$3.sval);e->v.sval=strdup(buf);$$=e; }
           | T_GOTO_LPAREN T_1DOLLAR T_GOTO_LPAREN goto_expr T_GOTO_RPAREN T_GOTO_RPAREN    { $$=$4; }
           | T_GOTO_LPAREN T_1DOLLAR T_STR T_GOTO_RPAREN                                     { tree_t*e=ast_node_new(TT_QLIT);e->v.sval=strdup($3.sval);$$=e; }
           ;
expr0      : expr2 T_2EQUAL expr0                                                             { $$=expr_binary(TT_ASSIGN,          $1,$3); }
           | expr2 T_2QUEST      expr0                                                             { $$=expr_binary(TT_SCAN,            $1,$3); }
           | expr2                                                                                 { $$=$1; }
           ;
expr2      : expr2 T_2AMP  expr3                                                             { tree_t*_e=expr_binary(TT_OPSYN,$1,$3); _e->v.sval=strdup("&"); $$=_e; }
           | expr3                                                                                 { $$=$1; }
           ;
expr3      : expr3 T_2PIPE expr4                                                            { tree_t*a=ast_node_new(TT_ALT);expr_add_child(a,$1);expr_add_child(a,$3);$$=a; }
           | expr4                                                                                 { $$=$1; }
           ;
expr4      : expr4 T_CONCAT expr5                                                                           { tree_t*s=ast_node_new(TT_SEQ);expr_add_child(s,$1);expr_add_child(s,$3);$$=s; }
           | expr5                                                                                 { $$=$1; }
           ;
expr5      : expr5 T_2AT    expr6                                                             { tree_t*_e=expr_binary(TT_OPSYN,$1,$3); _e->v.sval=strdup("@"); $$=_e; }
           | expr6                                                                                 { $$=$1; }
           ;
expr6      : expr6 T_2PLUS   expr7                                                             { $$=expr_binary(TT_ADD,             $1,$3); }
           | expr6 T_2MINUS expr7                                                            { $$=expr_binary(TT_SUB,             $1,$3); }
           | expr7                                                                                 { $$=$1; }
           ;
expr7      : expr7 T_2POUND      expr8                                                             { $$=expr_binary(TT_MUL,             $1,$3); }
           | expr8                                                                                 { $$=$1; }
           ;
expr8      : expr8 T_2SLASH   expr9                                                             { $$=expr_binary(TT_DIV,             $1,$3); }
           | expr9                                                                                 { $$=$1; }
           ;
expr9      : expr9 T_2STAR expr10                                                        { $$=expr_binary(TT_MUL,             $1,$3); }
           | expr10                                                                                { $$=$1; }
           ;
expr10     : expr10 T_2PERCENT   expr11                                                            { $$=expr_binary(TT_DIV,             $1,$3); }
           | expr11                                                                                { $$=$1; }
           ;
expr11     : expr12 T_2CARET expr11                                                       { $$=expr_binary(TT_POW,             $1,$3); }
           | expr12                                                                                { $$=$1; }
           ;
expr12     : expr12 T_2DOLLAR expr13                                                     { $$=expr_binary(TT_CAPT_IMMED_ASGN,$1,$3); }
           | expr12 T_2DOT      expr13                                                     { $$=expr_binary(TT_CAPT_COND_ASGN, $1,$3); }
           | expr13                                                                                { $$=$1; }
           ;
expr13     : expr14 T_2TILDE     expr13                                                            { tree_t*_e=expr_binary(TT_OPSYN,$1,$3); _e->v.sval=strdup("~"); $$=_e; }
           | expr14                                                                                { $$=$1; }
           ;
expr14     : T_1AT      expr14                                                             { $$=expr_unary(TT_CAPT_CURSOR,     $2); }
           | T_1TILDE        expr14                                                             { $$=expr_unary(TT_NOT,             $2); }
           | T_1QUEST expr14                                                            { $$=expr_unary(TT_INTERROGATE,     $2); }
           | T_1AMP    expr14                                                             { tree_t*_e=expr_unary(TT_OPSYN,$2); _e->v.sval=strdup("&"); $$=_e; }
           | T_1PLUS         expr14                                                             { $$=expr_unary(TT_PLS,             $2); }
           | T_1MINUS        expr14                                                             { $$=expr_unary(TT_MNS,             $2); }
           | T_1STAR     expr14                                                             { $$=expr_unary(TT_DEFER,           $2); }
           | T_1DOLLAR  expr14                                                             { $$=expr_unary(TT_INDIRECT,        $2); }
           | T_1DOT       expr14                                                             { $$=expr_unary(TT_NAME,            $2); }
           | T_1BANG  expr14                                                             { $$=expr_unary(TT_POW,             $2); }
           | T_1PERCENT      expr14                                                             { $$=expr_unary(TT_DIV,             $2); }
           | T_1SLASH        expr14                                                             { $$=expr_unary(TT_DIV,             $2); }
           | T_1POUND        expr14                                                             { $$=expr_unary(TT_MUL,             $2); }
           | T_1EQUAL        expr14                                                             { $$=expr_unary(TT_ASSIGN,          $2); }
           | T_1PIPE expr14                                                             { tree_t*_e=expr_unary(TT_OPSYN,$2); _e->v.sval=strdup("|"); $$=_e; }
           | expr15                                                                                { $$=$1; }
           ;
expr15     : expr15 T_LBRACK exprlist T_RBRACK                                                  { tree_t*i=ast_node_new(TT_IDX);expr_add_child(i,$1);for(int j=0;j<$3->n;j++)expr_add_child(i,$3->c[j]);free($3->c);free($3);$$=i; }
           | expr15 T_LANGLE exprlist T_RANGLE                                                  { tree_t*i=ast_node_new(TT_IDX);expr_add_child(i,$1);for(int j=0;j<$3->n;j++)expr_add_child(i,$3->c[j]);free($3->c);free($3);$$=i; }
           | expr17                                                                                { $$=$1; }
           ;
exprlist   : exprlist_ne                                                                           { $$=$1; }
           |                                                                           { $$=ast_node_new(TT_NUL); }
           ;
exprlist_ne: exprlist_ne T_COMMA expr0                                                            { expr_add_child($1,$3);$$=$1; }
           | exprlist_ne T_COMMA                                                                  { expr_add_child($1,ast_node_new(TT_NUL));$$=$1; }
           | expr0                                                                                 { tree_t*l=ast_node_new(TT_NUL);expr_add_child(l,$1);$$=l; }
           ;
expr17     : T_LPAREN expr0 T_RPAREN                                                            { $$=$2; }
           | T_LPAREN expr0 T_COMMA exprlist_ne T_RPAREN                                       { tree_t*a=ast_node_new(TT_VLIST);expr_add_child(a,$2);for(int i=0;i<$4->n;i++)expr_add_child(a,$4->c[i]);free($4->c);free($4);$$=a; }
           | T_LPAREN T_RPAREN                                                                  { $$=ast_node_new(TT_NUL); }
           | T_FUNCTION T_LPAREN exprlist T_RPAREN                                             { tree_e _k=pat_prim_kind($1.sval);tree_t*e=ast_node_new(_k==TT_VAR?TT_FNC:_k);if(_k==TT_VAR)e->v.sval=(char*)$1.sval;for(int i=0;i<$3->n;i++)expr_add_child(e,$3->c[i]);free($3->c);free($3);$$=e; }
           | T_IDENT                                                                              { tree_t*e=ast_node_new(TT_VAR);e->v.sval=(char*)$1.sval;$$=e; }
           | T_END                                                                                { tree_t*e=ast_node_new(TT_VAR);    e->v.sval=(char*)$1.sval;$$=e; }
           | T_KEYWORD                                                                            { tree_t*e=ast_node_new(TT_KEYWORD);e->v.sval=(char*)$1.sval;$$=e; }
           | T_STR                                                                                { tree_t*e=ast_node_new(TT_QLIT);   e->v.sval=(char*)$1.sval;$$=e; }
           | T_INT                                                                                { tree_t*e=ast_node_new(TT_ILIT);   e->v.ival=$1.ival;$$=e; }
           | T_REAL                                                                               { tree_t*e=ast_node_new(TT_FLIT);   e->v.dval=$1.dval;$$=e; }
           ;
%%
/*----------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------*/
int snobol4_lex(YYSTYPE *yylval_param, void *yyparse_param) {
    (void)yyparse_param; Token t=lex_next(g_lx); yylval_param->tok=t;
    if (getenv("SNO_TOK_TRACE"))
        fprintf(stderr,"[TOK %d sval=%s ival=%ld]\n",t.kind,t.sval?t.sval:"",t.ival);
    return t.kind;
}
/*----------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------*/
void snobol4_error(void *p,const char *msg){(void)p;sno_error(g_lx?g_lx->lineno:0,"parse error: %s",msg);}
/*----------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------*/
static void fixup_val(tree_t *e){ (void)e; }
/*----------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------*/
static int is_pat(tree_t *e){
    if(!e) return 0;
    switch(e->t){case TT_ARB:case TT_ARBNO:case TT_CAPT_COND_ASGN:case TT_CAPT_IMMED_ASGN:case TT_CAPT_CURSOR:case TT_DEFER:return 1;default:break;}
    for(int i=0;i<e->n;i++) if(is_pat(e->c[i])) return 1;
    return 0;
}
/*----------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------*/
static void sno4_stmt_commit_go(void *param,Token lbl,tree_t *subj,tree_t *pat,int has_eq,tree_t *repl,tree_t *gu,tree_t *gs,tree_t *gf){
    PP *pp=(PP*)param;
    /* PST-SN4-1a (2026-05-16): EXPORT/IMPORT special-case removed.  The parser
     * no longer interprets statements with label "EXPORT"/"IMPORT" specially.
     * They flow through as ordinary labeled statements with subject expressions,
     * exactly as the SNOBOL4 grammar describes them.  No downstream consumer
     * reads prog->exports / prog->imports today (only code_free walks them, to
     * free).  If a future feature needs cross-language link information, it
     * lives as a post-parse pass over the tree_t / TT_PROGRAM. */
    STMT_t *s=stmt_new();
    s->lineno = lbl.lineno ? lbl.lineno : snobol4_get_stmt_lineno();
    s->stno = ++pp->prog->nstmts;
    if(lbl.sval){s->label=strdup(lbl.sval);s->is_end=lbl.ival||(strcmp(lbl.sval,"END")==0);}
    /* PST-SN4-1b (2026-05-16): TT_SCAN-unpacking and TT_SEQ-splitting removed.
     * Parser emits pure syntax tree; lower.c performs the split. */
    s->subject=subj; s->pattern=pat;
    if(s->subject) fixup_val(s->subject);
    if(has_eq){s->has_eq=1;s->replacement=repl;if(repl&&!is_pat(repl))fixup_val(repl);}
    if(gu){ if(gu->t==TT_QLIT) s->goto_u=gu->v.sval; else s->goto_u_expr=gu; }
    if(gs){ if(gs->t==TT_QLIT) s->goto_s=gs->v.sval; else s->goto_s_expr=gs; }
    if(gf){ if(gf->t==TT_QLIT) s->goto_f=gf->v.sval; else s->goto_f_expr=gf; }
    if(!pp->prog->head) pp->prog->head=pp->prog->tail=s; else{pp->prog->tail->next=s;pp->prog->tail=s;}
    if (pp->ast_prog) {
        tree_t *anode = stmt_to_ast(s);
        if (pp->ast_prog->n >= pp->ast_prog->_nalloc) {
            pp->ast_prog->_nalloc = pp->ast_prog->_nalloc ? pp->ast_prog->_nalloc * 2 : 64;
            pp->ast_prog->c = realloc(pp->ast_prog->c,
                (size_t)pp->ast_prog->_nalloc * sizeof(tree_t*));
        }
        pp->ast_prog->c[pp->ast_prog->n++] = anode;
    }
}
/*----------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------*/
static tree_t *parse_expr(Lex *lx){
    CODE_t *prog=calloc(1,sizeof*prog);PP p={prog,NULL,NULL};g_lx=lx;snobol4_parse(&p);
    return prog->head?prog->head->subject:NULL;
}
/*----------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------*/
CODE_t *parse_program_tokens(Lex *stream){
    CODE_t *prog=calloc(1,sizeof*prog);PP p={prog,NULL,NULL};g_lx=stream;snobol4_parse(&p);return prog;
}
/*----------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------*/
CODE_t *parse_program_tokens_ast(Lex *stream, tree_t **ast_out){
    CODE_t *prog=calloc(1,sizeof*prog);
    tree_t *ast=calloc(1,sizeof*ast); ast->t=TT_PROGRAM;
    PP p={prog,NULL,ast};g_lx=stream;snobol4_parse(&p);
    *ast_out=ast;
    return prog;
}
/*----------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------*/
CODE_t *parse_program(LineArray *lines){(void)lines;return calloc(1,sizeof(CODE_t));}
/*----------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------*/
tree_t *parse_expr_from_str(const char *src){
    if(!src||!*src) return NULL;Lex lx={0};lex_open_str(&lx,src,(int)strlen(src),0);return parse_expr(&lx);
}
/*----------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------*/
tree_t *parse_expr_pat_from_str(const char *src) {
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
    PP p = {prog, NULL, NULL};
    g_lx = &lx;
    snobol4_parse(&p);
    free(buf);
    if (p.ast_prog && p.ast_prog->n > 0) {
        const tree_t *s = p.ast_prog->c[0];
        if (s) {
            tree_t *pat = stmt_attr_expr(stmt_attr_find(s, ":pat"));
            if (pat) { free(prog); return pat; }
            return stmt_attr_expr(stmt_attr_find(s, ":subj"));
        }
    }
    if (!prog->head) { free(prog); return NULL; }
    STMT_t *s = prog->head;
    tree_t *res = s->pattern ? s->pattern : s->subject;
    free(prog);
    return res;
}
/*----------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------*/
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
    PP p = {prog, NULL, NULL};
    g_lx = &lx;
    snobol4_parse(&p);
    free(buf);
    return prog;
}
/*----------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------*/
tree_t *sno_parse_string_ast(const char *src, CODE_t **code_out) {
    if (!src) { if (code_out) *code_out = calloc(1, sizeof(CODE_t)); return NULL; }
    int slen = (int)strlen(src);
    char *buf = malloc(slen + 2);
    if (!buf) { if (code_out) *code_out = calloc(1, sizeof(CODE_t)); return NULL; }
    memcpy(buf, src, slen);
    buf[slen]   = '\n';
    buf[slen+1] = '\0';
    Lex lx = {0};
    lex_open_str_initial(&lx, buf, slen + 1, 0);
    CODE_t *prog = calloc(1, sizeof(CODE_t));
    tree_t  *ast  = NULL;
    PP p = {prog, NULL, NULL};
    g_lx = &lx;
    snobol4_parse(&p);
    ast = p.ast_prog;
    free(buf);
    if (code_out) *code_out = prog; else free(prog);
    return ast;
}
