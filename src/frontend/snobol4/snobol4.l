%{
#include "scrip_cc.h"
#include "snobol4.h"
#include "snobol4.tab.h"
#define T_EOF SNOBOL4_EOF
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdarg.h>
#include <ctype.h>
/*----------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------*/
void sno_error(int, const char *, ...);
char *inc_dirs[64];
int   n_inc = 0;
int   sno_nerrors = 0;
static int   lineno = 1;
static int   g_stmt_lineno = 1;
static char  strbuf[65536];
static int   strpos;
/*----------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------*/
int snobol4_get_stmt_lineno(void) { return g_stmt_lineno; }
static int sno_fold_on = 0;  /* SN-31: case-sensitive is the one4all default.
                              * Classic SNOBOL4 folds to upper; our .sno/.inc
                              * corpus uses mixed-case identifiers (bSlash,
                              * semicolon, snoLine, Push_list).  Ingress-at-lex
                              * preserves source spelling.  Flip to 1 for
                              * classic behavior via `scrip` (no flag is wired
                              * back the other way today). */
/*----------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------*/
void sno_set_case_sensitive(int on) { sno_fold_on = on ? 0 : 1; }
/*----------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------*/
int  sno_get_case_sensitive(void)   { return sno_fold_on ? 0 : 1; }
/*----------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------*/
void sno_fold_name(char *name) {
    if (!sno_fold_on || !name) return;
    for (char *p = name; *p; p++) *p = (char)toupper((unsigned char)*p);
}
/*----------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------*/
static void fold_strbuf(void) {
    if (!sno_fold_on) return;
    for (char *p = strbuf; *p; p++) *p = (char)toupper((unsigned char)*p);
}
/*----------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------*/
static Token mktok(int k, const char *sv, long iv, double dv) {
    Token t; t.kind=k; t.sval=sv; t.ival=iv; t.dval=dv; t.lineno=lineno;
    return t;
}
%}
%option reentrant noyywrap never-interactive stack
%option extra-type="void *"
%x LABEL LABEL_DONE BODY BODY_START GT STR1 STR2 SKIP INCL
DIGIT   [0-9]
ALPHA   [A-Za-z\x80-\xFF]
IDCONT  [A-Za-z0-9_.\x80-\xFF]
ALNUM   [A-Za-z0-9_]
EXP     [eE][+-]?{DIGIT}+
WS      [ \t]
CONT    \n[+.][ \t]*
W       ({WS}|{CONT})+
G       ({WS}|{CONT})*
%%
<INITIAL>\n             { lineno++; return T_STMT_END; }
<INITIAL>[*!|;]         { BEGIN(SKIP); }
<INITIAL>[ \t]          { g_stmt_lineno = lineno; BEGIN(BODY_START); }
<INITIAL>"-"[Ii][Nn][Cc][Ll][Uu][Dd][Ee][ \t]+'[^'\n]+'[^\n]*\n  {
    char *q1 = strchr(yytext,'\''); char *q2 = strrchr(yytext,'\'');
    lineno++;
    if(q1 && q2 && q2>q1){
        char iname[4096]; int fi=(int)(q2-q1-1);
        if(fi>=(int)sizeof iname)fi=(int)sizeof iname-1;
        memcpy(iname,q1+1,fi);iname[fi]='\0';
        char rpath[4096]; rpath[0]='\0';
        FILE *inc=fopen(iname,"r");
        if(inc){snprintf(rpath,sizeof rpath,"%s",iname);}
        if(!inc){char p[4096];for(int i=0;i<n_inc&&!inc;i++){snprintf(p,sizeof p,"%s/%s",inc_dirs[i],iname);if((inc=fopen(p,"r")))snprintf(rpath,sizeof rpath,"%s",p);}}
        if(!inc)sno_error(lineno,"cannot open include '%s'",iname);
        else {
            char *sl=strrchr(rpath,'/');
            if(sl){char *d=strndup(rpath,(size_t)(sl-rpath));sno_add_include_dir(d);}
            yypush_buffer_state(yy_create_buffer(inc,YY_BUF_SIZE,yyscanner),yyscanner);
        }
    }
}
<INITIAL>"-"[Ii][Nn][Cc][Ll][Uu][Dd][Ee][ \t]+\"[^\"\n]+\"[^\n]*\n  {
    char *q1 = strchr(yytext,'"'); char *q2 = strrchr(yytext,'"');
    lineno++;
    if(q1 && q2 && q2>q1){
        char iname[4096]; int fi=(int)(q2-q1-1);
        if(fi>=(int)sizeof iname)fi=(int)sizeof iname-1;
        memcpy(iname,q1+1,fi);iname[fi]='\0';
        char rpath[4096]; rpath[0]='\0';
        FILE *inc=fopen(iname,"r");
        if(inc){snprintf(rpath,sizeof rpath,"%s",iname);}
        if(!inc){char p[4096];for(int i=0;i<n_inc&&!inc;i++){snprintf(p,sizeof p,"%s/%s",inc_dirs[i],iname);if((inc=fopen(p,"r")))snprintf(rpath,sizeof rpath,"%s",p);}}
        if(!inc)sno_error(lineno,"cannot open include '%s'",iname);
        else {
            char *sl=strrchr(rpath,'/');
            if(sl){char *d=strndup(rpath,(size_t)(sl-rpath));sno_add_include_dir(d);}
            yypush_buffer_state(yy_create_buffer(inc,YY_BUF_SIZE,yyscanner),yyscanner);
        }
    }
}
<INITIAL>"-"[Cc][Oo][Pp][Yy][ \t]+'[^'\n]+'[^\n]*\n  {
    char *q1 = strchr(yytext,'\''); char *q2 = strrchr(yytext,'\'');
    lineno++;
    if(q1 && q2 && q2>q1){
        char iname[4096]; int fi=(int)(q2-q1-1);
        if(fi>=(int)sizeof iname)fi=(int)sizeof iname-1;
        memcpy(iname,q1+1,fi);iname[fi]='\0';
        char rpath[4096]; rpath[0]='\0';
        FILE *inc=fopen(iname,"r");
        if(inc){snprintf(rpath,sizeof rpath,"%s",iname);}
        if(!inc){char p[4096];for(int i=0;i<n_inc&&!inc;i++){snprintf(p,sizeof p,"%s/%s",inc_dirs[i],iname);if((inc=fopen(p,"r")))snprintf(rpath,sizeof rpath,"%s",p);}}
        if(!inc)sno_error(lineno,"cannot open include '%s'",iname);
        else {
            char *sl=strrchr(rpath,'/');
            if(sl){char *d=strndup(rpath,(size_t)(sl-rpath));sno_add_include_dir(d);}
            yypush_buffer_state(yy_create_buffer(inc,YY_BUF_SIZE,yyscanner),yyscanner);
        }
    }
}
<INITIAL>"-"[^\n]*\n    {
    lineno++;
}
<INITIAL>[+.]           {
    BEGIN(BODY_START);
}
<INITIAL>[^ \t\n*!|;+.\-] {
    strbuf[0] = yytext[0]; strpos = 1;
    BEGIN(LABEL);
}
<LABEL>[^ \t\n]+  {
    int take = yyleng < (int)sizeof(strbuf)-strpos-1
               ? yyleng : (int)sizeof(strbuf)-strpos-1;
    memcpy(strbuf+strpos, yytext, take); strpos += take;
}
<LABEL>[ \t]            {
    strbuf[strpos] = '\0';
    BEGIN(BODY_START);
    return T_LABEL;
}
<LABEL>\n               {
    strbuf[strpos] = '\0';
    lineno++;
    BEGIN(LABEL_DONE);
    return T_LABEL;
}
<LABEL_DONE>[ \t]          { BEGIN(INITIAL); return T_STMT_END; }
<LABEL_DONE>\n             { lineno++; BEGIN(INITIAL); return T_STMT_END; }
<LABEL_DONE>[*!|;]         { yyless(0); BEGIN(INITIAL); return T_STMT_END; }
<LABEL_DONE>[^ \t\n*!|;]  { yyless(0); BEGIN(INITIAL); return T_STMT_END; }
<LABEL_DONE><<EOF>>        { BEGIN(INITIAL); return T_STMT_END; }
<LABEL>.                {
    yyless(0);
    strbuf[strpos] = '\0';
    BEGIN(BODY_START);
    return T_LABEL;
}
<BODY_START>[ \t]+      { }
<BODY_START>\n          { lineno++; BEGIN(INITIAL); return T_STMT_END; }
<BODY_START>";"         { BEGIN(INITIAL); return T_STMT_END; }
<BODY_START>.           { yyless(0); BEGIN(BODY); }
<BODY>\n               { lineno++; BEGIN(INITIAL); return T_STMT_END; }
<BODY>";"              { BEGIN(INITIAL); return T_STMT_END; }
<BODY>{W}";"           { BEGIN(INITIAL); return T_STMT_END; }
<BODY>{G}":"  { for(int _i=0;_i<yyleng;_i++) if(yytext[_i]=='\n') lineno++; BEGIN(GT); }
<BODY>&{ALPHA}{ALNUM}* { strbuf[0]='\0'; strncat(strbuf,yytext+1,sizeof(strbuf)-1); return T_KEYWORD; }
<BODY>{W}  { for(int _i=0;_i<yyleng;_i++) if(yytext[_i]=='\n') lineno++; return T_CONCAT; }
<BODY>{W}"**"{W}  { for(int _i=0;_i<yyleng;_i++) if(yytext[_i]=='\n') lineno++; return T_2CARET;  }
<BODY>{W}"^"{W}   { for(int _i=0;_i<yyleng;_i++) if(yytext[_i]=='\n') lineno++; return T_2CARET;  }
<BODY>{W}"!"{W}   { for(int _i=0;_i<yyleng;_i++) if(yytext[_i]=='\n') lineno++; return T_2CARET;  }
<BODY>{W}"+"{W}   { for(int _i=0;_i<yyleng;_i++) if(yytext[_i]=='\n') lineno++; return T_2PLUS;        }
<BODY>{W}"-"{W}   { for(int _i=0;_i<yyleng;_i++) if(yytext[_i]=='\n') lineno++; return T_2MINUS;     }
<BODY>{W}"*"{W}   { for(int _i=0;_i<yyleng;_i++) if(yytext[_i]=='\n') lineno++; return T_2STAR;  }
<BODY>{W}"/"{W}   { for(int _i=0;_i<yyleng;_i++) if(yytext[_i]=='\n') lineno++; return T_2SLASH;        }
<BODY>{W}"$"{W}   { for(int _i=0;_i<yyleng;_i++) if(yytext[_i]=='\n') lineno++; return T_2DOLLAR;}
<BODY>{W}"."{W}   { for(int _i=0;_i<yyleng;_i++) if(yytext[_i]=='\n') lineno++; return T_2DOT;     }
<BODY>{W}"|"{W}   { for(int _i=0;_i<yyleng;_i++) if(yytext[_i]=='\n') lineno++; return T_2PIPE;     }
<BODY>{W}"="{W}   { for(int _i=0;_i<yyleng;_i++) if(yytext[_i]=='\n') lineno++; return T_2EQUAL;      }
<BODY>{W}"="       { return T_2EQUAL; }
<BODY>{W}"?"{W}   { for(int _i=0;_i<yyleng;_i++) if(yytext[_i]=='\n') lineno++; return T_2QUEST;           }
<BODY>{W}"&"{W}   { for(int _i=0;_i<yyleng;_i++) if(yytext[_i]=='\n') lineno++; return T_2AMP;       }
<BODY>{W}"@"{W}   { for(int _i=0;_i<yyleng;_i++) if(yytext[_i]=='\n') lineno++; return T_2AT;         }
<BODY>{W}"#"{W}   { for(int _i=0;_i<yyleng;_i++) if(yytext[_i]=='\n') lineno++; return T_2POUND;           }
<BODY>{W}"%"{W}   { for(int _i=0;_i<yyleng;_i++) if(yytext[_i]=='\n') lineno++; return T_2PERCENT;         }
<BODY>{W}"~"{W}   { for(int _i=0;_i<yyleng;_i++) if(yytext[_i]=='\n') lineno++; return T_2TILDE;           }
<BODY>"("{G}  { for(int _i=0;_i<yyleng;_i++) if(yytext[_i]=='\n') lineno++; return T_LPAREN; }
<BODY>"["{G}  { for(int _i=0;_i<yyleng;_i++) if(yytext[_i]=='\n') lineno++; return T_LBRACK; }
<BODY>"<"{G}  { for(int _i=0;_i<yyleng;_i++) if(yytext[_i]=='\n') lineno++; return T_LANGLE; }
<BODY>{G}")"  { for(int _i=0;_i<yyleng;_i++) if(yytext[_i]=='\n') lineno++; return T_RPAREN; }
<BODY>{G}"]"  { for(int _i=0;_i<yyleng;_i++) if(yytext[_i]=='\n') lineno++; return T_RBRACK; }
<BODY>{G}">"  { for(int _i=0;_i<yyleng;_i++) if(yytext[_i]=='\n') lineno++; return T_RANGLE; }
<BODY>{G}","{G}  { for(int _i=0;_i<yyleng;_i++) if(yytext[_i]=="\n") lineno++; return T_COMMA; }
<BODY>{ALPHA}{IDCONT}*/"("  { strbuf[0]='\0'; strncat(strbuf,yytext,sizeof(strbuf)-1); fold_strbuf(); return T_FUNCTION; }
<BODY>{ALPHA}{IDCONT}*      { strbuf[0]='\0'; strncat(strbuf,yytext,sizeof(strbuf)-1); fold_strbuf(); return strcmp(strbuf,"END")==0?T_END:T_IDENT; }
<BODY>{DIGIT}+"."{DIGIT}+{EXP}? |
<BODY>{DIGIT}+{EXP}         { strbuf[0]='\0'; strncat(strbuf,yytext,sizeof(strbuf)-1); return T_REAL; }
<BODY>{DIGIT}+                { strbuf[0]='\0'; strncat(strbuf,yytext,sizeof(strbuf)-1); return T_INT;  }
<BODY>\'                       { strpos=0; BEGIN(STR1); }
<BODY>\"                       { strpos=0; BEGIN(STR2); }
<BODY>"**"   { return T_2CARET;  }
<BODY>"^"    { return T_2CARET;  }
<BODY>"+"    { return T_1PLUS;         }
<BODY>"-"    { return T_1MINUS;        }
<BODY>"*"    { return T_1STAR;     }
<BODY>"/"    { return T_1SLASH;        }
<BODY>"%"    { return T_1PERCENT;      }
<BODY>"@"    { return T_1AT;      }
<BODY>"~"    { return T_1TILDE;        }
<BODY>"$"    { return T_1DOLLAR;  }
<BODY>"."    { return T_1DOT;       }
<BODY>"!"    { return T_1BANG;  }
<BODY>"#"    { return T_1POUND;        }
<BODY>"|"    { return T_1PIPE; }
<BODY>"="    { return T_1EQUAL;        }
<BODY>"?"    { return T_1QUEST;}
<BODY>"&"    { return T_1AMP;    }
<BODY>.      { sno_error(lineno,"unexpected char '%s'",yytext); }
<STR1>[^'\n]*           {
    int take = yyleng < (int)sizeof(strbuf)-strpos-1
               ? yyleng : (int)sizeof(strbuf)-strpos-1;
    memcpy(strbuf+strpos, yytext, take); strpos += take;
}
<STR1>\'\'              {
    if (strpos < (int)sizeof(strbuf)-1) strbuf[strpos++] = '\'';
}
<STR1>\'                {
    strbuf[strpos] = '\0';
    BEGIN(BODY);
    return T_STR;
}
<STR1>\n                {
    sno_error(lineno,"unterminated string"); lineno++; BEGIN(BODY);
}
<STR2>[^\"\n]*          {
    int take = yyleng < (int)sizeof(strbuf)-strpos-1
               ? yyleng : (int)sizeof(strbuf)-strpos-1;
    memcpy(strbuf+strpos, yytext, take); strpos += take;
}
<STR2>\"\"              {
    if (strpos < (int)sizeof(strbuf)-1) strbuf[strpos++] = '"';
}
<STR2>\"                {
    strbuf[strpos] = '\0';
    BEGIN(BODY);
    return T_STR;
}
<STR2>\n                {
    sno_error(lineno,"unterminated string"); lineno++; BEGIN(BODY);
}
<GT>[ \t]+              { return T_CONCAT; }
<GT>[Ss]/"("            { return T_GOTO_S; }
<GT>[Ss]/"<"            { return T_GOTO_S; }
<GT>[Ff]/"("            { return T_GOTO_F; }
<GT>[Ff]/"<"            { return T_GOTO_F; }
<GT>"("                 { return T_GOTO_LPAREN; }
<GT>")"                 { return T_GOTO_RPAREN; }
<GT>"<"                 { return T_GOTO_LPAREN; }
<GT>">"                 { return T_GOTO_RPAREN; }
<GT>"$"                 { return T_1DOLLAR; }
<GT>[A-Za-z][A-Za-z0-9_.]* {
    strbuf[0]='\0'; strncat(strbuf, yytext, sizeof(strbuf)-1);
    return T_IDENT;
}
<GT>\'[^\'\n]*\'    {
    int len = yyleng - 2;
    if (len < 0) len = 0;
    if (len >= (int)sizeof(strbuf)) len = sizeof(strbuf)-1;
    memcpy(strbuf, yytext+1, len); strbuf[len]='\0';
    return T_STR;
}
<GT>";"                 { BEGIN(INITIAL); return T_STMT_END; }
<GT>\n                  { lineno++; BEGIN(INITIAL); return T_STMT_END; }
<GT>.                   { }
<SKIP>[^\n]*            { }
<SKIP>\n                { lineno++; BEGIN(INITIAL); }
<<EOF>>             {
    if (YY_CURRENT_BUFFER) yypop_buffer_state(yyscanner);
    if (!YY_CURRENT_BUFFER) return T_EOF;
}
%%
/*----------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------*/
void sno_error(int ln, const char *fmt, ...) {
    va_list ap;
    fprintf(stderr,"snobol4:%d: error: ",ln);
    va_start(ap,fmt); vfprintf(stderr,fmt,ap); va_end(ap);
    fputc('\n',stderr);
    sno_nerrors++;
}
/*----------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------*/
void sno_add_include_dir(const char *d) {
    if (n_inc < 64) inc_dirs[n_inc++] = (char *)d;
}
/*----------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------*/
void flex_lex_open(Lex *lx, FILE *f, const char *fname) {
    yyscan_t sc;
    yylex_init(&sc);
    yy_switch_to_buffer(yy_create_buffer(f, YY_BUF_SIZE, sc), sc);
    lineno = 1;
    lx->_scanner = sc;
    lx->_extra   = NULL;
    (void)fname;
}
/*----------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------*/
Token flex_lex_next(Lex *lx) {
    yyscan_t sc = (yyscan_t)lx->_scanner;
    for (;;) {
        int r = yylex(sc);
        Token t;
        t.lineno = lineno;
        t.ival   = 0;
        t.dval   = 0.0;
        t.sval   = NULL;
        t.kind   = r;
        switch (r) {
        case T_EOF:
            t.kind = T_EOF; return t;
        case T_LABEL:
            fold_strbuf();
            t.sval = intern(strbuf);
            t.ival = strcmp(strbuf,"END")==0 ? 1 : 0;
            return t;
        case T_IDENT: case T_END: case T_KEYWORD:
        case T_FUNCTION:
        case T_GOTO_S: case T_GOTO_F:
            fold_strbuf();
            t.sval = intern(strbuf);
            return t;
        case T_STR:
            t.sval = intern(strbuf);
            return t;
        case T_INT:
            t.ival = atol(strbuf);
            return t;
        case T_REAL:
            t.sval = intern(strbuf);
            t.dval = atof(strbuf);
            return t;
        default:
            return t;
        }
    }
}
/*----------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------*/
void flex_lex_destroy(Lex *lx) {
    if (lx->_scanner) {
        yylex_destroy((yyscan_t)lx->_scanner);
        lx->_scanner = NULL;
    }
}
/*----------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------*/
void lex_open_str(Lex *lx, const char *s, int len, int ln) {
    yyscan_t sc;
    yylex_init(&sc);
    yy_scan_bytes(s, len, sc);
    lineno = ln;
    yy_push_state(BODY, sc);
    lx->_scanner = sc;
    lx->_extra   = NULL;
}
/*----------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------*/
void lex_open_str_initial(Lex *lx, const char *s, int len, int ln) {
    yyscan_t sc;
    yylex_init(&sc);
    yy_scan_bytes(s, len, sc);
    lineno = ln;
    lx->_scanner = sc;
    lx->_extra   = NULL;
}
/*----------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------*/
Token lex_next(Lex *lx) {
    if (lx->peeked) { lx->peeked=0; return lx->peek; }
    return flex_lex_next(lx);
}
/*----------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------*/
Token lex_peek(Lex *lx) {
    if (!lx->peeked) { lx->peek=lex_next(lx); lx->peeked=1; }
    return lx->peek;
}
/*----------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------*/
int  lex_at_end(Lex *lx) { return lex_peek(lx).kind==T_EOF; }
/*----------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------*/
void lex_destroy(Lex *lx) { flex_lex_destroy(lx); }
/*----------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------*/
extern CODE_t *parse_program_tokens(Lex *stream);
/*----------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------*/
extern CODE_t *parse_program_tokens_ast(Lex *stream, tree_t **ast_out);
/*----------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------*/
CODE_t *sno_parse(FILE *f, const char *fname) {
    Lex lx; memset(&lx,0,sizeof lx);
    flex_lex_open(&lx,f,fname);
    CODE_t *prog = parse_program_tokens(&lx);
    flex_lex_destroy(&lx);
    return prog;
}
/*----------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------*/
tree_t *sno_parse_ast(FILE *f, const char *fname, CODE_t **code_out) {
    Lex lx; memset(&lx,0,sizeof lx);
    flex_lex_open(&lx,f,fname);
    tree_t *ast = NULL;
    CODE_t *prog = parse_program_tokens_ast(&lx, &ast);
    flex_lex_destroy(&lx);
    if (code_out) *code_out = prog;
    return ast;
}
/*----------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------*/
void sno_reset(void) { sno_nerrors = 0; n_inc = 0; }
