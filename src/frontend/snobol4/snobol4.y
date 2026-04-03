%code requires { #include "scrip_cc.h" #include "snobol4.h" }
%code {
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
typedef struct { Program *prog; EXPR_t **result; } PP;
static Lex     *g_lx;
static void     sno4_stmt_commit(void*,Token,EXPR_t*,EXPR_t*,int,EXPR_t*,Token);
static void     fixup_val(EXPR_t*);
static int      is_pat(EXPR_t*);
static char    *goto_label(Lex*);
static SnoGoto *goto_field(const char*,int);
static EXPR_t  *parse_expr(Lex*);
}
%define api.prefix {snobol4_}
%define api.pure full
%parse-param { void *yyparse_param }
%union { EXPR_t *expr; Token tok; }

/* Atoms */
%token <tok> TK_IDENT TK_FUNCTION TK_KEYWORD TK_END TK_INT TK_REAL TK_STR
/* Statement structure */
%token <tok> TK_LABEL TK_GOTO TK_STMT_END
/* Binary operators (WHITE op WHITE) */
%token TK_ASSIGNMENT TK_MATCH TK_ALTERNATION TK_ADDITION TK_SUBTRACTION
%token TK_MULTIPLICATION TK_DIVISION TK_EXPONENTIATION
%token TK_IMMEDIATE_ASSIGN TK_COND_ASSIGN
%token TK_AMPERSAND TK_AT_SIGN TK_POUND TK_PERCENT TK_TILDE
/* Unary operators (no leading space) — named per SPITBOL Chapter 15 */
%token TK_UN_AT_SIGN TK_UN_TILDE TK_UN_QUESTION_MARK TK_UN_AMPERSAND
%token TK_UN_PLUS TK_UN_MINUS TK_UN_ASTERISK TK_UN_DOLLAR_SIGN TK_UN_PERIOD
%token TK_UN_EXCLAMATION TK_UN_PERCENT TK_UN_SLASH TK_UN_POUND
%token TK_UN_EQUAL TK_UN_VERTICAL_BAR
/* Structural */
%token TK_CONCAT TK_COMMA TK_LPAREN TK_RPAREN TK_LBRACK TK_RBRACK TK_LANGLE TK_RANGLE

%type <expr> expr0 expr2 expr3 expr4 expr5 expr6 expr7 expr8
%type <expr> expr9 expr10 expr11 expr12 expr13 expr14 expr15 expr17
%type <expr> exprlist exprlist_ne opt_subject opt_pattern opt_repl
%type <tok>  opt_label opt_goto

%%
top        : program                                                                                { }
           ;
program    : program stmt | stmt                                                                    ;
stmt       : opt_label opt_subject opt_repl opt_goto TK_STMT_END                      { sno4_stmt_commit(yyparse_param,$1,$2,($3!=NULL),$3,$4); }
           ;
opt_label  : TK_LABEL                                                                              { $$=$1; }
           | /* empty */                                                                           { $$.sval=NULL;$$.ival=0;$$.lineno=0;$$.kind=0; }
           ;
opt_subject: expr3                                                                                { $$=$1; }
           | /* empty */                                                                           { $$=NULL; }
           ;
opt_pattern: expr3                                                                                 { $$=$1; }
           | /* empty */                                                                           { $$=NULL; }
           ;
opt_repl   : TK_ASSIGNMENT expr0                                                                   { $$=$2; }
           | /* empty */                                                                           { $$=NULL; }
           ;
opt_goto   : TK_GOTO                                                                               { $$=$1; }
           | /* empty */                                                                           { $$.sval=NULL;$$.ival=0;$$.lineno=0;$$.kind=0; }
           ;

/* Expression grammar — levels match beauty.sno Expr0–Expr17 and SPITBOL manual priorities */
expr0      : expr2 TK_ASSIGNMENT expr0                                                             { $$=expr_binary(E_ASSIGN,          $1,$3); }
           | expr2 TK_MATCH      expr0                                                             { $$=expr_binary(E_SCAN,            $1,$3); }
           | expr2                                                                                 { $$=$1; }
           ;
expr2      : expr2 TK_AMPERSAND  expr3                                                             { $$=expr_binary(E_OPSYN,           $1,$3); }
           | expr3                                                                                 { $$=$1; }
           ;
expr3      : expr3 TK_ALTERNATION expr4                                                            { if($1->kind==E_ALT){expr_add_child($1,$3);$$=$1;}else{EXPR_t*a=expr_new(E_ALT);expr_add_child(a,$1);expr_add_child(a,$3);$$=a;} }
           | expr4                                                                                 { $$=$1; }
           ;
expr4      : expr4 TK_CONCAT expr5                                                                           { if($1->kind==E_SEQ){expr_add_child($1,$3);$$=$1;}else{EXPR_t*s=expr_new(E_SEQ);expr_add_child(s,$1);expr_add_child(s,$3);$$=s;} }
           | expr5                                                                                 { $$=$1; }
           ;
expr5      : expr5 TK_AT_SIGN    expr6                                                             { $$=expr_binary(E_CAPT_CURSOR,     $1,$3); }
           | expr6                                                                                 { $$=$1; }
           ;
expr6      : expr6 TK_ADDITION   expr7                                                             { $$=expr_binary(E_ADD,             $1,$3); }
           | expr6 TK_SUBTRACTION expr7                                                            { $$=expr_binary(E_SUB,             $1,$3); }
           | expr7                                                                                 { $$=$1; }
           ;
expr7      : expr7 TK_POUND      expr8                                                             { $$=expr_binary(E_MUL,             $1,$3); }
           | expr8                                                                                 { $$=$1; }
           ;
expr8      : expr8 TK_DIVISION   expr9                                                             { $$=expr_binary(E_DIV,             $1,$3); }
           | expr9                                                                                 { $$=$1; }
           ;
expr9      : expr9 TK_MULTIPLICATION expr10                                                        { $$=expr_binary(E_MUL,             $1,$3); }
           | expr10                                                                                { $$=$1; }
           ;
expr10     : expr10 TK_PERCENT   expr11                                                            { $$=expr_binary(E_DIV,             $1,$3); }
           | expr11                                                                                { $$=$1; }
           ;
expr11     : expr12 TK_EXPONENTIATION expr11                                                       { $$=expr_binary(E_POW,             $1,$3); }
           | expr12                                                                                { $$=$1; }
           ;
expr12     : expr12 TK_IMMEDIATE_ASSIGN expr13                                                     { $$=expr_binary(E_CAPT_IMMED_ASGN,$1,$3); }
           | expr12 TK_COND_ASSIGN      expr13                                                     { $$=expr_binary(E_CAPT_COND_ASGN, $1,$3); }
           | expr13                                                                                { $$=$1; }
           ;
expr13     : expr14 TK_TILDE     expr13                                                            { $$=expr_binary(E_CAPT_COND_ASGN, $1,$3); }
           | expr14                                                                                { $$=$1; }
           ;
expr14     : TK_UN_AT_SIGN      expr14                                                             { $$=expr_unary(E_CAPT_CURSOR,     $2); }
           | TK_UN_TILDE        expr14                                                             { $$=expr_unary(E_INDIRECT,        $2); }
           | TK_UN_QUESTION_MARK expr14                                                            { $$=expr_unary(E_INTERROGATE,     $2); }
           | TK_UN_AMPERSAND    expr14                                                             { $$=expr_unary(E_OPSYN,           $2); }
           | TK_UN_PLUS         expr14                                                             { $$=expr_unary(E_PLS,             $2); }
           | TK_UN_MINUS        expr14                                                             { $$=expr_unary(E_MNS,             $2); }
           | TK_UN_ASTERISK     expr14                                                             { $$=expr_unary(E_DEFER,           $2); }
           | TK_UN_DOLLAR_SIGN  expr14                                                             { $$=expr_unary(E_INDIRECT,        $2); }
           | TK_UN_PERIOD       expr14                                                             { $$=expr_unary(E_NAME,            $2); }
           | TK_UN_EXCLAMATION  expr14                                                             { $$=expr_unary(E_POW,             $2); }  /* user-definable */
           | TK_UN_PERCENT      expr14                                                             { $$=expr_unary(E_DIV,             $2); }  /* user-definable */
           | TK_UN_SLASH        expr14                                                             { $$=expr_unary(E_DIV,             $2); }  /* user-definable */
           | TK_UN_POUND        expr14                                                             { $$=expr_unary(E_MUL,             $2); }  /* user-definable */
           | TK_UN_EQUAL        expr14                                                             { $$=expr_unary(E_ASSIGN,          $2); }  /* user-definable */
           | TK_UN_VERTICAL_BAR expr14                                                             { $$=expr_unary(E_ALT,             $2); }  /* user-definable */
           | expr15                                                                                { $$=$1; }
           ;
expr15     : expr15 TK_LBRACK exprlist TK_RBRACK                                                  { EXPR_t*i=expr_new(E_IDX);expr_add_child(i,$1);for(int j=0;j<$3->nchildren;j++)expr_add_child(i,$3->children[j]);free($3->children);free($3);$$=i; }
           | expr15 TK_LANGLE exprlist TK_RANGLE                                                  { EXPR_t*i=expr_new(E_IDX);expr_add_child(i,$1);for(int j=0;j<$3->nchildren;j++)expr_add_child(i,$3->children[j]);free($3->children);free($3);$$=i; }
           | expr17                                                                                { $$=$1; }
           ;
exprlist   : exprlist_ne                                                                           { $$=$1; }
           | /* empty */                                                                           { $$=expr_new(E_NUL); }
           ;
exprlist_ne: exprlist_ne TK_COMMA expr0                                                            { expr_add_child($1,$3);$$=$1; }
           | exprlist_ne TK_COMMA                                                                  { expr_add_child($1,expr_new(E_NUL));$$=$1; }
           | expr0                                                                                 { EXPR_t*l=expr_new(E_NUL);expr_add_child(l,$1);$$=l; }
           ;
expr17     : TK_LPAREN expr0 TK_RPAREN                                                            { $$=$2; }
           | TK_LPAREN expr0 TK_COMMA exprlist_ne TK_RPAREN                                       { EXPR_t*a=expr_new(E_ALT);expr_add_child(a,$2);for(int i=0;i<$4->nchildren;i++)expr_add_child(a,$4->children[i]);free($4->children);free($4);$$=a; }
           | TK_LPAREN TK_RPAREN                                                                  { $$=expr_new(E_NUL); }
           | TK_FUNCTION TK_LPAREN exprlist TK_RPAREN                                             { EXPR_t*e=expr_new(E_FNC);e->sval=(char*)$1.sval;for(int i=0;i<$3->nchildren;i++)expr_add_child(e,$3->children[i]);free($3->children);free($3);$$=e; }
           | TK_IDENT                                                                              { EXPR_t*e=expr_new(E_VAR);    e->sval=(char*)$1.sval;$$=e; }
           | TK_END                                                                                { EXPR_t*e=expr_new(E_VAR);    e->sval=(char*)$1.sval;$$=e; }
           | TK_KEYWORD                                                                            { EXPR_t*e=expr_new(E_KEYWORD);e->sval=(char*)$1.sval;$$=e; }
           | TK_STR                                                                                { EXPR_t*e=expr_new(E_QLIT);   e->sval=(char*)$1.sval;$$=e; }
           | TK_INT                                                                                { EXPR_t*e=expr_new(E_ILIT);   e->ival=$1.ival;$$=e; }
           | TK_REAL                                                                               { EXPR_t*e=expr_new(E_FLIT);   e->dval=$1.dval;$$=e; }
           ;
%%
int snobol4_lex(YYSTYPE *yylval_param, void *yyparse_param) {
    (void)yyparse_param; Token t=lex_next(g_lx); yylval_param->tok=t;
    switch(t.kind){
        case T_IDENT:            return TK_IDENT;           case T_FUNCTION:         return TK_FUNCTION;
        case T_KEYWORD:          return TK_KEYWORD;         case T_END:              return TK_END;
        case T_INT:              return TK_INT;             case T_REAL:             return TK_REAL;
        case T_STR:              return TK_STR;
        case T_LABEL:            return TK_LABEL;           case T_GOTO:             return TK_GOTO;
        case T_STMT_END:         return TK_STMT_END;
        case T_ASSIGNMENT:       return TK_ASSIGNMENT;      case T_MATCH:            return TK_MATCH;
        case T_ALTERNATION:      return TK_ALTERNATION;     case T_ADDITION:         return TK_ADDITION;
        case T_SUBTRACTION:      return TK_SUBTRACTION;     case T_MULTIPLICATION:   return TK_MULTIPLICATION;
        case T_DIVISION:         return TK_DIVISION;        case T_EXPONENTIATION:   return TK_EXPONENTIATION;
        case T_IMMEDIATE_ASSIGN: return TK_IMMEDIATE_ASSIGN;case T_COND_ASSIGN:      return TK_COND_ASSIGN;
        case T_AMPERSAND:        return TK_AMPERSAND;       case T_AT_SIGN:          return TK_AT_SIGN;
        case T_POUND:            return TK_POUND;           case T_PERCENT:          return TK_PERCENT;
        case T_TILDE:            return TK_TILDE;
        case T_UN_AT_SIGN:       return TK_UN_AT_SIGN;      case T_UN_TILDE:         return TK_UN_TILDE;
        case T_UN_QUESTION_MARK: return TK_UN_QUESTION_MARK;case T_UN_AMPERSAND:     return TK_UN_AMPERSAND;
        case T_UN_PLUS:          return TK_UN_PLUS;         case T_UN_MINUS:         return TK_UN_MINUS;
        case T_UN_ASTERISK:      return TK_UN_ASTERISK;     case T_UN_DOLLAR_SIGN:   return TK_UN_DOLLAR_SIGN;
        case T_UN_PERIOD:        return TK_UN_PERIOD;       case T_UN_EXCLAMATION:   return TK_UN_EXCLAMATION;
        case T_UN_PERCENT:       return TK_UN_PERCENT;      case T_UN_SLASH:         return TK_UN_SLASH;
        case T_UN_POUND:         return TK_UN_POUND;        case T_UN_EQUAL:         return TK_UN_EQUAL;
        case T_UN_VERTICAL_BAR:  return TK_UN_VERTICAL_BAR;
        case T_CONCAT:           return TK_CONCAT;  case T_COMMA:            return TK_COMMA;
        case T_LPAREN:           return TK_LPAREN;          case T_RPAREN:           return TK_RPAREN;
        case T_LBRACK:           return TK_LBRACK;          case T_RBRACK:           return TK_RBRACK;
        case T_LANGLE:           return TK_LANGLE;          case T_RANGLE:           return TK_RANGLE;
        default:                 return 0;
    }
}
void snobol4_error(void *p,const char *msg){(void)p;sno_error(g_lx?g_lx->lineno:0,"parse error: %s",msg);}
static void fixup_val(EXPR_t *e){
    if(!e) return; if(e->kind==E_SEQ) e->kind=E_CAT;
    for(int i=0;i<e->nchildren;i++) fixup_val(e->children[i]);
}
static int is_pat(EXPR_t *e){
    if(!e) return 0;
    switch(e->kind){case E_ARB:case E_ARBNO:case E_CAPT_COND_ASGN:case E_CAPT_IMMED_ASGN:case E_CAPT_CURSOR:case E_DEFER:return 1;default:break;}
    for(int i=0;i<e->nchildren;i++) if(is_pat(e->children[i])) return 1;
    return 0;
}
static char *goto_label(Lex *lx){
    Token t=lex_peek(lx); TokKind open=t.kind,close;
    if(open==T_LPAREN) close=T_RPAREN; else if(open==T_LANGLE) close=T_RANGLE; else return NULL;
    lex_next(lx); t=lex_peek(lx); char *label=NULL;
    if(t.kind==T_IDENT||t.kind==T_KEYWORD||t.kind==T_END){lex_next(lx);label=(char*)t.sval;}
    else if(t.kind==T_UN_DOLLAR_SIGN){
        lex_next(lx);
        if(lex_peek(lx).kind==T_LPAREN){
            int depth=1;lex_next(lx);char eb[512];int ep=0;
            while(!lex_at_end(lx)&&depth>0){Token tok=lex_next(lx);if(tok.kind==T_LPAREN)depth++;else if(tok.kind==T_RPAREN){depth--;if(!depth)break;}if(tok.sval&&ep<510){int l=strlen(tok.sval);memcpy(eb+ep,tok.sval,l);ep+=l;}}
            eb[ep]='\0';char*buf=malloc(12+ep+1);memcpy(buf,"$COMPUTED:",10);memcpy(buf+10,eb,ep);buf[10+ep]='\0';label=buf;
        } else if(lex_peek(lx).kind==T_STR){
            Token n=lex_next(lx);const char*lit=n.sval?n.sval:"";char*buf=malloc(16+strlen(lit));sprintf(buf,"$COMPUTED:'%s'",lit);label=buf;
        } else {Token n=lex_next(lx);char buf[512];snprintf(buf,sizeof buf,"$%s",n.sval?n.sval:"?");label=strdup(buf);}
    } else if(t.kind==T_LPAREN){
        int depth=1;lex_next(lx);
        while(!lex_at_end(lx)&&depth>0){Token tok=lex_next(lx);if(tok.kind==T_LPAREN)depth++;else if(tok.kind==T_RPAREN)depth--;}
        label=strdup("$COMPUTED");
    }
    if(lex_peek(lx).kind==close) lex_next(lx);
    return label;
}
static SnoGoto *goto_field(const char *gs,int lineno){
    if(!gs||!*gs) return NULL;
    Lex lx={0};lex_open_str(&lx,gs,(int)strlen(gs),lineno);SnoGoto *g=sgoto_new();
    while(!lex_at_end(&lx)){
        Token t=lex_peek(&lx);
        if(t.kind==T_IDENT&&t.sval){
            if(strcasecmp(t.sval,"S")==0){lex_next(&lx);g->onsuccess=goto_label(&lx);continue;}
            if(strcasecmp(t.sval,"F")==0){lex_next(&lx);g->onfailure=goto_label(&lx);continue;}
        }
        if(t.kind==T_LPAREN||t.kind==T_LANGLE){g->uncond=goto_label(&lx);continue;}
        sno_error(lineno,"unexpected token in goto field");lex_next(&lx);
    }
    if(!g->onsuccess&&!g->onfailure&&!g->uncond){free(g);return NULL;}
    return g;
}
static void sno4_stmt_commit(void *param,Token lbl,EXPR_t *subj,EXPR_t *pat,int has_eq,EXPR_t *repl,Token gt){
    PP *pp=(PP*)param;
    if(lbl.sval&&strcasecmp(lbl.sval,"EXPORT")==0){
        if(subj&&subj->kind==E_VAR&&subj->sval){
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
    if(lbl.sval){s->label=strdup(lbl.sval);s->is_end=lbl.ival||(strcasecmp(lbl.sval,"END")==0);}
    s->subject=subj; s->pattern=pat;
    if(s->subject) fixup_val(s->subject);
    if(has_eq){s->has_eq=1;s->replacement=repl;if(repl&&!is_pat(repl))fixup_val(repl);}
    s->go=goto_field(gt.sval,s->lineno);
    if(!pp->prog->head) pp->prog->head=pp->prog->tail=s; else{pp->prog->tail->next=s;pp->prog->tail=s;}
    pp->prog->nstmts++;
}
static EXPR_t *parse_expr(Lex *lx){
    Program *prog=calloc(1,sizeof*prog);PP p={prog,NULL};g_lx=lx;snobol4_parse(&p);
    return prog->head?prog->head->subject:NULL;
}
Program *parse_program_tokens(Lex *stream){
    Program *prog=calloc(1,sizeof*prog);PP p={prog,NULL};g_lx=stream;snobol4_parse(&p);return prog;
}
Program *parse_program(LineArray *lines){(void)lines;return calloc(1,sizeof(Program));}
EXPR_t *parse_expr_from_str(const char *src){
    if(!src||!*src) return NULL;Lex lx={0};lex_open_str(&lx,src,(int)strlen(src),0);return parse_expr(&lx);
}
