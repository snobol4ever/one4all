%{
#include "raku.tab.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
static char raku_strbuf[65536];
static int  raku_strpos;
static int raku_after_smatch  = 0;
static int raku_match_global   = 0;
static char *raku_subst_pat    = NULL;
/*----------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------*/
void raku_lex_error(const char *msg) {
    fprintf(stderr, "raku lex error line %d: %s\n", raku_yylineno, msg);
}
/*----------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------*/
int raku_get_lineno(void) { return raku_yylineno; }
%}
%option noyywrap noinput nounput
%option prefix="raku_yy"
%option yylineno
%x STR_DQ STR_SQ STR_RE STR_SUBST_PAT STR_SUBST_REPL
DIGIT    [0-9]
ALPHA    [A-Za-z_]
ALNUM    [A-Za-z0-9_]
EXP      [eE][+-]?{DIGIT}+
WS       [ \t\r]
%%
{WS}+           { }
\n              { }
"#"[^\n]*       { }
"my"            { return KW_MY; }
"say"           { return KW_SAY; }
"print"         { return KW_PRINT; }
"if"            { return KW_IF; }
"else"          { return KW_ELSE; }
"elsif"         { return KW_ELSIF; }
"while"         { return KW_WHILE; }
"for"           { return KW_FOR; }
"given"         { return KW_GIVEN; }
"when"          { return KW_WHEN; }
"default"       { return KW_DEFAULT; }
"sub"           { return KW_SUB; }
"gather"        { return KW_GATHER; }
"take"          { return KW_TAKE; }
"return"        { return KW_RETURN; }
"exists"        { return KW_EXISTS; }
"delete"        { return KW_DELETE; }
"unless"        { return KW_UNLESS; }
"until"         { return KW_UNTIL; }
"repeat"        { return KW_REPEAT; }
"map"           { return KW_MAP; }
"grep"          { return KW_GREP; }
"sort"          { return KW_SORT; }
"try"           { return KW_TRY; }
"catch"         { return KW_CATCH; }
"CATCH"         { return KW_CATCH; }
"die"           { return KW_DIE; }
"class"         { return KW_CLASS; }
"method"        { return KW_METHOD; }
"has"           { return KW_HAS; }
"new"           { return KW_NEW; }
"self"          { raku_yylval.sval = strdup("self"); return VAR_SCALAR; }
"True"          { raku_yylval.ival = 1; return LIT_INT; }
"False"         { raku_yylval.ival = 0; return LIT_INT; }
"eq"            { return OP_SEQ; }
"ne"            { return OP_SNE; }
"div"           { return OP_DIV; }
"..^"           { return OP_RANGE_EX; }
".."            { return OP_RANGE; }
"->"            { return OP_ARROW; }
"=="            { return OP_EQ; }
"!="            { return OP_NE; }
"<="            { return OP_LE; }
">="            { return OP_GE; }
"&&"            { return OP_AND; }
"||"            { return OP_OR; }
":="            { return OP_BIND; }
"~~"            { raku_after_smatch = 1; return OP_SMATCH; }
"+"             { return '+'; }
"-"             { return '-'; }
"*"             { return '*'; }
"%"             { return '%'; }
"~"             { return '~'; }
"<"             { return '<'; }
">"             { return '>'; }
"!"             { return '!'; }
"="             { return '='; }
"{"             { return '{'; }
"}"             { return '}'; }
"("             { return '('; }
")"             { return ')'; }
"["             { return '['; }
"]"             { return ']'; }
";"             { return ';'; }
","             { return ','; }
"."             { return '.'; }
"=>"            { return OP_FATARROW; }
{DIGIT}+"."{DIGIT}+{EXP}?  { raku_yylval.dval = atof(yytext); return LIT_FLOAT; }
{DIGIT}+{EXP}               { raku_yylval.dval = atof(yytext); return LIT_FLOAT; }
{DIGIT}+                    { raku_yylval.ival = atol(yytext); return LIT_INT; }
"$_"            { raku_yylval.sval = strdup("$_"); return VAR_SCALAR; }
"$*STDIN"       { raku_yylval.ival = 0; return VAR_CAPTURE; }
"$*STDOUT"      { raku_yylval.ival = 1; return VAR_CAPTURE; }
"$*STDERR"      { raku_yylval.ival = 2; return VAR_CAPTURE; }
"$"[0-9]+       { raku_yylval.ival = atoi(yytext+1); return VAR_CAPTURE; }
"$<"[a-zA-Z][a-zA-Z0-9_]*">" {
    int len = strlen(yytext)-3;
    char *nm = malloc(len+1); memcpy(nm,yytext+2,len); nm[len]='\0';
    raku_yylval.sval = nm; return VAR_NAMED_CAPTURE; }
"$."{ALPHA}{ALNUM}*  { raku_yylval.sval = strdup(yytext+2); return VAR_TWIGIL; }
"$!"{ALPHA}{ALNUM}*  { raku_yylval.sval = strdup(yytext+2); return VAR_TWIGIL; }
"$"{ALPHA}{ALNUM}*  { raku_yylval.sval = strdup(yytext); return VAR_SCALAR; }
"@"{ALPHA}{ALNUM}*  { raku_yylval.sval = strdup(yytext); return VAR_ARRAY; }
"%"{ALPHA}{ALNUM}*  { raku_yylval.sval = strdup(yytext); return VAR_HASH; }
{ALPHA}{ALNUM}* { raku_yylval.sval = strdup(yytext); return IDENT; }
"\""            { raku_strpos = 0; BEGIN(STR_DQ); }
<STR_DQ>"\"" {
    raku_strbuf[raku_strpos] = '\0';
    raku_yylval.sval = strdup(raku_strbuf);
    BEGIN(INITIAL);
    if (strchr(raku_strbuf, '$') != NULL)
        return LIT_INTERP_STR;
    return LIT_STR;
}
<STR_DQ>"\\n"  { raku_strbuf[raku_strpos++] = '\n'; }
<STR_DQ>"\\t"  { raku_strbuf[raku_strpos++] = '\t'; }
<STR_DQ>"\\\\" { raku_strbuf[raku_strpos++] = '\\'; }
<STR_DQ>"\\\"" { raku_strbuf[raku_strpos++] = '"'; }
<STR_DQ>\n     { raku_strbuf[raku_strpos++] = '\n'; }
<STR_DQ>.      { raku_strbuf[raku_strpos++] = yytext[0]; }
"'"             { raku_strpos = 0; BEGIN(STR_SQ); }
<STR_SQ>"'"    {
    raku_strbuf[raku_strpos] = '\0';
    raku_yylval.sval = strdup(raku_strbuf);
    BEGIN(INITIAL);
    return LIT_STR;
}
<STR_SQ>"\\'"  { raku_strbuf[raku_strpos++] = '\''; }
<STR_SQ>"\\\\" { raku_strbuf[raku_strpos++] = '\\'; }
<STR_SQ>\n     { raku_strbuf[raku_strpos++] = '\n'; }
<STR_SQ>.      { raku_strbuf[raku_strpos++] = yytext[0]; }
"s/"            { if (raku_after_smatch) { raku_after_smatch=0; raku_strpos=0; BEGIN(STR_SUBST_PAT); } else { yyless(1); return 's'; } }
<STR_SUBST_PAT>"/"  {
    raku_strbuf[raku_strpos]='\0';
    raku_subst_pat = strdup(raku_strbuf);
    raku_strpos=0; BEGIN(STR_SUBST_REPL);
}
<STR_SUBST_PAT>"\\/"  { raku_strbuf[raku_strpos++]='/'; }
<STR_SUBST_PAT>.       { raku_strbuf[raku_strpos++]=yytext[0]; }
<STR_SUBST_REPL>"/"[g]? {
    raku_strbuf[raku_strpos]='\0';
    int global = (yytext[1]=='g');
    size_t plen=strlen(raku_subst_pat), rlen=raku_strpos;
    char *tok = malloc(plen+rlen+4);
    memcpy(tok, raku_subst_pat, plen); tok[plen]='\x01';
    memcpy(tok+plen+1, raku_strbuf, rlen); tok[plen+1+rlen]='\x01';
    tok[plen+2+rlen] = global ? 'g' : '-'; tok[plen+3+rlen]='\0';
    free(raku_subst_pat); raku_subst_pat=NULL;
    raku_yylval.sval = tok;
    BEGIN(INITIAL); return LIT_SUBST;
}
<STR_SUBST_REPL>"\\/"  { raku_strbuf[raku_strpos++]='/'; }
<STR_SUBST_REPL>.       { raku_strbuf[raku_strpos++]=yytext[0]; }
"m:g/"          { if (raku_after_smatch) { raku_after_smatch=0; raku_strpos=0; BEGIN(STR_RE); raku_match_global=1; } else { yyless(3); return IDENT; } }
"/"             { if (raku_after_smatch) { raku_after_smatch=0; raku_strpos=0; BEGIN(STR_RE); } else { return '/'; } }
<STR_RE>"/"    {
    raku_strbuf[raku_strpos] = '\0';
    raku_yylval.sval = strdup(raku_strbuf);
    BEGIN(INITIAL);
    if (raku_match_global) { raku_match_global=0; return LIT_MATCH_GLOBAL; }
    return LIT_REGEX;
}
<STR_RE>"\\/"  { raku_strbuf[raku_strpos++] = '/'; }
<STR_RE>\n     { raku_strbuf[raku_strpos++] = '\n'; }
<STR_RE>.      { raku_strbuf[raku_strpos++] = yytext[0]; }
.               {
    char buf[64];
    snprintf(buf, sizeof buf, "unexpected char '%c'", yytext[0]);
    raku_lex_error(buf);
}
%%
