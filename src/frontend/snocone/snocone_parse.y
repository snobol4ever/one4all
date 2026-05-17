%code top {
}
%code requires {
#include "scrip_cc.h"
struct LexCtx;
struct IfHead;
struct FuncHead;
typedef struct LoopFrame {
    char    *cont_label;
    char    *end_label;
    char   **user_labels;
    int      user_labels_count;
    int      is_loop;
    int      cont_used;
    struct LoopFrame *outer;
} LoopFrame;
typedef struct ScParseState {
    struct LexCtx *ctx;
    CODE_t        *code;
    const char    *filename;
    int            nerrors;
    int            label_seq;
    char          *cur_func_name;
    LoopFrame    *loop_top;
    char        **pending_user_labels;
    int           pending_user_labels_count;
    int           pending_user_labels_cap;
    char        **stash_for_pending_labels;
    int           stash_for_pending_labels_count;
    struct SwitchHead *cur_switch;
    STMT_t            *if_before_body;    /* PST-SC-4b */
    STMT_t            *while_before_body; /* PST-SC-4c */
    STMT_t            *do_before_body;    /* PST-SC-4d */
    STMT_t            *for_before_body;   /* PST-SC-4e */
    STMT_t            *func_before_body;  /* PST-SC-4g: CODE_t tail snapshot taken at func_head */
} ScParseState;
}
%code {
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include "snocone_lex.h"
#include "../icon/icon_lex.h"
/* compat shims: old names → new canonical names */
#define expr_new(k)            ast_node_new(k)
#define expr_add_child(p,c)    ast_push((p),(c))
#define AST_ASSIGN  TT_ASSIGN
#define AST_ADD     TT_ADD
#define AST_SUB     TT_SUB
#define AST_MUL     TT_MUL
#define AST_DIV     TT_DIV
#define AST_POW     TT_POW
#define AST_SEQ     TT_SEQ
#define AST_ALT     TT_ALT
#define AST_SCAN    TT_SCAN
#define AST_FNC     TT_FNC
#define AST_VAR     TT_VAR
#define AST_KEYWORD TT_KEYWORD
#define AST_QLIT    TT_QLIT
#define AST_ILIT    TT_ILIT
#define AST_FLIT    TT_FLIT
#define AST_NUL     TT_NUL
#define AST_VLIST   TT_VLIST
#define AST_IDX     TT_IDX
#define AST_INDIRECT   TT_INDIRECT
#define AST_DEFER      TT_DEFER
#define AST_NAME       TT_NAME
#define AST_CAPT_CURSOR      TT_CAPT_CURSOR
#define AST_CAPT_IMMED_ASGN  TT_CAPT_IMMED_ASGN
#define AST_CAPT_COND_ASGN   TT_CAPT_COND_ASGN
#define AST_PLS        TT_PLS
#define AST_MNS        TT_MNS
#define AST_NOT        TT_NOT
#define AST_INTERROGATE TT_INTERROGATE
#define AST_OPSYN      TT_OPSYN
/* old field names → new field names */
#define kind       t
#define nchildren  n
#define children   c
#define sval       v.sval
#define ival       v.ival
#define dval       v.dval
/*----------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------*/
int  sc_lex  (SC_STYPE *yylval, ScParseState *st);
/*----------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------*/
void sc_error(ScParseState *st, const char *msg);
/*----------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------*/
static void     sc_append_stmt        (ScParseState *st, tree_t *top);
/*----------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------*/
static tree_t  *sc_collect_body       (ScParseState *st, STMT_t *snapshot);
/*----------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------*/
static void     sc_finalize_if_no_else_pst(ScParseState *st, tree_t *cond);
/*----------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------*/
static void     sc_finalize_if_else_pst(ScParseState *st, tree_t *cond, STMT_t *before_else);
/*----------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------*/
static void     sc_split_subject_pattern(tree_t **subj_io, tree_t **pat_io);
/*----------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------*/
static tree_t  *sc_int_literal        (const char *txt);
/*----------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------*/
static tree_t  *sc_real_literal       (const char *txt);
/*----------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------*/
static tree_t  *sc_str_literal        (const char *txt);
struct IfHead {
    tree_t *cond;
    STMT_t *before_body;   /* st->code->tail snapshot at end of if_head reduction
                              (NULL if list was empty); body starts at
                              before_body->next or st->code->head if NULL.     */
    int     lineno;
};
struct ForHead {
    tree_t *cond;
    tree_t *step;
};
struct FuncHead {
    char   *name;
    char   *argstr;
    char   *prev_func;
};
struct CaseEntry {
    char   *case_label;
    tree_t *value;
    STMT_t *before_body;  /* PST-SC-4f: CODE_t tail snapshot taken just before this arm's stmts */
};
struct SwitchHead {
    tree_t *disc;
    char   *tmp_name;
    char   *end_label;
    char   *default_label;
    int     has_default;
    STMT_t *after_tmp_assign;
    struct CaseEntry *cases;
    int     cases_count;
    int     cases_cap;
    STMT_t *last_case_label_tail;
    struct SwitchHead *prev_switch;
    int     lineno;
};
/*----------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------*/
static char    *sc_label_new          (ScParseState *st, const char *prefix);
/*----------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------*/
static struct IfHead    *sc_if_head_new    (ScParseState *st, tree_t *cond);
/*----------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------*/
static void     sc_finalize_while_pst  (ScParseState *st, tree_t *cond);
/*----------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------*/
static void     sc_finalize_do_while_pst(ScParseState *st, tree_t *cond);
/*----------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------*/
static struct ForHead   *sc_for_head_new_pst(ScParseState *st, tree_t *cond, tree_t *step);
/*----------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------*/
static void     sc_append_return      (ScParseState *st, tree_t *retval);
/*----------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------*/
static void     sc_append_freturn     (ScParseState *st);
/*----------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------*/
static void     sc_append_nreturn     (ScParseState *st);
/*----------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------*/
static STMT_t  *sc_make_label_stmt    (ScParseState *st, char *label);
/*----------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------*/
static STMT_t  *sc_make_goto_uncond_stmt(ScParseState *st, char *target);
/*----------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------*/
static void     sc_splice_after       (ScParseState *st, STMT_t *anchor, STMT_t *chain_head, STMT_t *chain_tail);
/*----------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------*/
static void     sc_append_chain       (ScParseState *st, STMT_t *chain_head, STMT_t *chain_tail);
/*----------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------*/
static void     sc_finalize_if_no_else(ScParseState *st, struct IfHead *h);
/*----------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------*/
static void     sc_finalize_if_else   (ScParseState *st, struct IfHead *h, STMT_t *before_else);
/*----------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------*/
static void     sc_finalize_for_pst   (ScParseState *st, struct ForHead *h);
/*----------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------*/
static struct FuncHead *sc_func_head_new_pst(ScParseState *st, char *name, char *argstr);
/*----------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------*/
static void     sc_finalize_function_pst(ScParseState *st, struct FuncHead *h);
/*----------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------*/
static void     sc_emit_label_pad     (ScParseState *st, char *label);
/*----------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------*/
static void     sc_append_goto_label  (ScParseState *st, char *target);
/*----------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------*/
static void     sc_pending_label_add   (ScParseState *st, const char *name);
/*----------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------*/
static void     sc_pending_label_clear (ScParseState *st);
/*----------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------*/
static void     sc_pending_to_stash    (ScParseState *st);
/*----------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------*/
static void     sc_loop_push           (ScParseState *st, char *cont_label, char *end_label, int is_loop, int from_stash);
/*----------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------*/
static void     sc_loop_pop            (ScParseState *st);
/*----------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------*/
static LoopFrame *sc_loop_find_by_user_label(ScParseState *st, const char *name, int want_loop);
/*----------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------*/
static void     sc_append_break        (ScParseState *st, char *user_label);
/*----------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------*/
static void     sc_append_continue     (ScParseState *st, char *user_label);
/*----------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------*/
static struct SwitchHead *sc_switch_head_new(ScParseState *st, tree_t *disc);
/*----------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------*/
static void     sc_switch_case_label   (ScParseState *st, tree_t *value);
/*----------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------*/
static void     sc_switch_default_label(ScParseState *st);
/*----------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------*/
static void     sc_finalize_switch_pst (ScParseState *st, struct SwitchHead *h);
/*----------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------*/
static void     sc_emit_struct         (ScParseState *st, char *name, char *fields);
}
%define api.prefix {sc_}
%define api.pure full
%parse-param { ScParseState *st }
%lex-param   { ScParseState *st }
%union {
    tree_t *expr;
    char   *str;
    long    ival;
    double  dval;
    struct IfHead    *ifhead;
    struct ForHead   *forhead;
    struct FuncHead  *funchead;
    struct SwitchHead *switchhead;
    STMT_t           *stmt_ptr;
}
%token <str> T_IDENT
%token <str> T_KEYWORD
%token <str> T_INT
%token <str> T_REAL
%token <str> T_STR
%token <str> T_CALL
%token T_2PLUS
%token T_2MINUS
%token T_2STAR
%token T_2SLASH
%token T_2CARET
%token T_EQ T_NE T_LT T_GT T_LE T_GE
%token T_LEQ T_LNE T_LLT T_LGT T_LLE T_LGE
%token T_IDENT_OP T_DIFFER
%token T_1PLUS
%token T_1MINUS
%token T_2EQUAL
%token T_PLUS_ASSIGN T_MINUS_ASSIGN T_STAR_ASSIGN T_SLASH_ASSIGN T_CARET_ASSIGN
%token T_2QUEST
%token T_2PIPE
%token T_CONCAT
%token T_LPAREN
%token T_RPAREN
%token T_SEMICOLON
%token T_COMMA
%token T_LBRACK T_RBRACK
%token T_2DOLLAR T_2DOT
%token T_2AMP T_2AT T_2POUND T_2PERCENT T_2TILDE
%token T_1STAR T_1SLASH T_1PERCENT
%token T_1AT T_1TILDE T_1DOLLAR T_1DOT T_1POUND
%token T_1PIPE T_1EQUAL T_1QUEST T_1AMP T_1BANG
%token T_COLON
%token T_DO T_FOR
%token T_SWITCH T_CASE T_DEFAULT
%token T_BREAK T_CONTINUE T_GOTO
%token T_DEFINE T_RETURN T_FRETURN T_NRETURN T_STRUCT
%token T_UNKNOWN
%token T_LBRACE T_RBRACE
%token T_IF T_ELSE T_WHILE
%type <expr> expr0 expr1 expr3 expr4 expr5 expr6 expr9 expr11 expr12 expr15 expr17 exprlist exprlist_ne
%type <expr>      if_head while_head
/* do_head has no semantic value (PST-SC-4d: snapshot taken into st->do_before_body) */
%type <stmt_ptr>  else_keyword
%type <forhead>   for_head
%type <funchead>  func_head
%type <str>       func_arglist func_arglist_ne
%type <switchhead> switch_head
%type <str>       struct_field_list
%%
program     : stmt_list
            |
            ;
stmt_list   : stmt_list stmt
            | stmt
            ;
stmt        : matched_stmt
            | unmatched_stmt
            ;
matched_stmt
            : simple_stmt
            | block_stmt
            | if_head matched_stmt else_keyword matched_stmt
                                        { sc_finalize_if_else_pst(st, $1, $3); }
            | while_head matched_stmt
                                        { sc_finalize_while_pst(st, $1); }
            | do_head do_body T_WHILE T_LPAREN expr0 T_RPAREN T_SEMICOLON
                                        { sc_finalize_do_while_pst(st, $5); }
            | for_head matched_stmt
                                        { sc_finalize_for_pst(st, $1); }
            | func_head T_LBRACE stmt_list T_RBRACE
                                        { sc_finalize_function_pst(st, $1); }
            | func_head T_LBRACE T_RBRACE
                                        { sc_finalize_function_pst(st, $1); }
            | switch_head T_LBRACE switch_body T_RBRACE
                                        { sc_finalize_switch_pst(st, $1); }
            | switch_head T_LBRACE T_RBRACE
                                        { sc_finalize_switch_pst(st, $1); }
            | T_STRUCT T_IDENT T_LBRACE struct_field_list T_RBRACE
                                        { sc_emit_struct(st, $2, $4); free($2); free($4); }
            | T_STRUCT T_IDENT T_LBRACE T_RBRACE
                                        { sc_emit_struct(st, $2, strdup("")); free($2); }
            | label_decl matched_stmt
            ;
unmatched_stmt
            : if_head stmt
                                        { sc_finalize_if_no_else_pst(st, $1); }
            | if_head matched_stmt else_keyword unmatched_stmt
                                        { sc_finalize_if_else_pst(st, $1, $3); }
            | while_head unmatched_stmt
                                        { sc_finalize_while_pst(st, $1); }
            | for_head unmatched_stmt
                                        { sc_finalize_for_pst(st, $1); }
            | label_decl unmatched_stmt
            ;
if_head     : T_IF T_LPAREN expr0 T_RPAREN opt_head_sep
                                        { st->if_before_body = st->code->tail; $$ = $3; }
            ;
while_head  : T_WHILE T_LPAREN expr0 T_RPAREN opt_head_sep
                                        { char *lc = sc_label_new(st, "_Ltop");
                                          char *le = sc_label_new(st, "_Lend");
                                          sc_loop_push(st, lc, le, 1, 0);
                                          st->while_before_body = st->code->tail;
                                          $$ = $3; }
            ;
do_head     : T_DO                  { char *lc = sc_label_new(st, "_Lcont");
                                      char *le = sc_label_new(st, "_Lend");
                                      sc_loop_push(st, lc, le, 1, 0);
                                      st->do_before_body = st->code->tail; }
            ;
do_body     : T_LBRACE stmt_list T_RBRACE
            | T_LBRACE T_RBRACE
            ;
for_lead    : T_FOR                  { sc_pending_to_stash(st); }
            ;
for_head    : for_lead T_LPAREN expr0 T_SEMICOLON expr0 T_SEMICOLON expr0 T_RPAREN opt_head_sep
                                        { sc_append_stmt(st, $3);
                                          char *lc = sc_label_new(st, "_Lcont");
                                          char *le = sc_label_new(st, "_Lend");
                                          sc_loop_push(st, lc, le, 1, 1);
                                          st->for_before_body = st->code->tail;
                                          $$ = sc_for_head_new_pst(st, $5, $7); }
            ;
switch_head : T_SWITCH T_LPAREN expr0 T_RPAREN
                                        { $$ = sc_switch_head_new(st, $3); }
            ;
switch_body : case_clause
            | switch_body case_clause
            ;
case_clause : case_or_default_label
            | case_clause stmt
            ;
case_or_default_label
            : T_CASE expr0 T_COLON      { sc_switch_case_label(st, $2); }
            | T_DEFAULT T_COLON         { sc_switch_default_label(st); }
            ;
opt_head_sep
            :
            | T_CONCAT
            ;
func_head   : T_DEFINE T_IDENT T_LPAREN func_arglist opt_head_sep
                                        { $$ = sc_func_head_new_pst(st, $2, $4); free($2); free($4); }
            ;
func_arglist
            : T_RPAREN                 { $$ = strdup(""); }
            | T_IDENT T_RPAREN         { $$ = strdup($1); free($1); }
            | func_arglist_ne T_RPAREN { $$ = $1; }
            ;
func_arglist_ne
            : T_IDENT T_COMMA T_IDENT
                { int len = strlen($1) + 1 + strlen($3) + 1;
                  char *s = malloc(len); snprintf(s, len, "%s,%s", $1, $3);
                  free($1); free($3); $$ = s; }
            | func_arglist_ne T_COMMA T_IDENT
                { int len = strlen($1) + 1 + strlen($3) + 1;
                  char *s = malloc(len); snprintf(s, len, "%s,%s", $1, $3);
                  free($1); free($3); $$ = s; }
            ;
struct_field_list
            : T_IDENT
                { $$ = strdup($1); free($1); }
            | struct_field_list T_COMMA T_IDENT
                { int len = strlen($1) + 1 + strlen($3) + 1;
                  char *s = malloc(len); snprintf(s, len, "%s,%s", $1, $3);
                  free($1); free($3); $$ = s; }
            ;
else_keyword
            : T_ELSE                 { $$ = st->code->tail; }
            ;
label_decl
            : T_IDENT T_COLON        { sc_emit_label_pad(st, $1); free($1); }
            ;
simple_stmt : expr0 T_SEMICOLON                { sc_append_stmt(st, $1); }
            | T_SEMICOLON                      {         }
            | T_RETURN expr0 T_SEMICOLON    { sc_append_return(st, $2); }
            | T_RETURN T_SEMICOLON          { sc_append_return(st, NULL); }
            | T_FRETURN T_SEMICOLON         { sc_append_freturn(st); }
            | T_NRETURN T_SEMICOLON         { sc_append_nreturn(st); }
            | T_GOTO T_IDENT T_SEMICOLON    { sc_append_goto_label(st, $2); free($2); }
            | T_BREAK T_SEMICOLON           { sc_append_break(st, NULL); }
            | T_BREAK T_IDENT T_SEMICOLON   { sc_append_break(st, $2); free($2); }
            | T_CONTINUE T_SEMICOLON        { sc_append_continue(st, NULL); }
            | T_CONTINUE T_IDENT T_SEMICOLON { sc_append_continue(st, $2); free($2); }
            ;
block_stmt  : T_LBRACE stmt_list T_RBRACE      { }
            | T_LBRACE T_RBRACE                {                  }
            ;
expr0       : expr1 T_2EQUAL    expr0
                                { $$ = expr_binary(AST_ASSIGN, $1, $3); }
            | expr1 T_2EQUAL
                                { tree_t *empty = expr_new(AST_QLIT);
                                  empty->sval = strdup("");
                                  $$ = expr_binary(AST_ASSIGN, $1, empty); }
            | expr1 T_PLUS_ASSIGN   expr0
                                { tree_t *a = ast_node_new(TT_AUGOP); a->ival = TK_AUGPLUS;
                                  ast_push(a, $1); ast_push(a, $3); $$ = a; }
            | expr1 T_MINUS_ASSIGN  expr0
                                { tree_t *a = ast_node_new(TT_AUGOP); a->ival = TK_AUGMINUS;
                                  ast_push(a, $1); ast_push(a, $3); $$ = a; }
            | expr1 T_STAR_ASSIGN   expr0
                                { tree_t *a = ast_node_new(TT_AUGOP); a->ival = TK_AUGSTAR;
                                  ast_push(a, $1); ast_push(a, $3); $$ = a; }
            | expr1 T_SLASH_ASSIGN  expr0
                                { tree_t *a = ast_node_new(TT_AUGOP); a->ival = TK_AUGSLASH;
                                  ast_push(a, $1); ast_push(a, $3); $$ = a; }
            | expr1 T_CARET_ASSIGN  expr0
                                { tree_t *a = ast_node_new(TT_AUGOP); a->ival = TK_AUGPOW;
                                  ast_push(a, $1); ast_push(a, $3); $$ = a; }
            | expr1
                                { $$ = $1; }
            ;
expr1       : expr3 T_2QUEST expr1
                                { $$ = expr_binary(AST_SCAN, $1, $3); }
            | expr3
                                { $$ = $1; }
            ;
expr3       : expr3 T_2PIPE expr4
                                { tree_t *a = ast_node_new(TT_ALT);
                                  expr_add_child(a, $1); expr_add_child(a, $3);
                                  $$ = a; }
            | expr4
                                { $$ = $1; }
            ;
expr4       : expr4 T_CONCAT expr5
                                { tree_t *s = ast_node_new(TT_SEQ);
                                  expr_add_child(s, $1); expr_add_child(s, $3);
                                  $$ = s; }
            | expr5
                                { $$ = $1; }
            ;
expr5       : expr5 T_EQ        expr6
                                { tree_t *e = expr_new(AST_FNC); e->sval = strdup("EQ");
                                  expr_add_child(e, $1); expr_add_child(e, $3); $$ = e; }
            | expr5 T_NE        expr6
                                { tree_t *e = expr_new(AST_FNC); e->sval = strdup("NE");
                                  expr_add_child(e, $1); expr_add_child(e, $3); $$ = e; }
            | expr5 T_LT        expr6
                                { tree_t *e = expr_new(AST_FNC); e->sval = strdup("LT");
                                  expr_add_child(e, $1); expr_add_child(e, $3); $$ = e; }
            | expr5 T_GT        expr6
                                { tree_t *e = expr_new(AST_FNC); e->sval = strdup("GT");
                                  expr_add_child(e, $1); expr_add_child(e, $3); $$ = e; }
            | expr5 T_LE        expr6
                                { tree_t *e = expr_new(AST_FNC); e->sval = strdup("LE");
                                  expr_add_child(e, $1); expr_add_child(e, $3); $$ = e; }
            | expr5 T_GE        expr6
                                { tree_t *e = expr_new(AST_FNC); e->sval = strdup("GE");
                                  expr_add_child(e, $1); expr_add_child(e, $3); $$ = e; }
            | expr5 T_LEQ       expr6
                                { tree_t *e = expr_new(AST_FNC); e->sval = strdup("LEQ");
                                  expr_add_child(e, $1); expr_add_child(e, $3); $$ = e; }
            | expr5 T_LNE       expr6
                                { tree_t *e = expr_new(AST_FNC); e->sval = strdup("LNE");
                                  expr_add_child(e, $1); expr_add_child(e, $3); $$ = e; }
            | expr5 T_LLT       expr6
                                { tree_t *e = expr_new(AST_FNC); e->sval = strdup("LLT");
                                  expr_add_child(e, $1); expr_add_child(e, $3); $$ = e; }
            | expr5 T_LGT       expr6
                                { tree_t *e = expr_new(AST_FNC); e->sval = strdup("LGT");
                                  expr_add_child(e, $1); expr_add_child(e, $3); $$ = e; }
            | expr5 T_LLE       expr6
                                { tree_t *e = expr_new(AST_FNC); e->sval = strdup("LLE");
                                  expr_add_child(e, $1); expr_add_child(e, $3); $$ = e; }
            | expr5 T_LGE       expr6
                                { tree_t *e = expr_new(AST_FNC); e->sval = strdup("LGE");
                                  expr_add_child(e, $1); expr_add_child(e, $3); $$ = e; }
            | expr5 T_IDENT_OP  expr6
                                { tree_t *e = expr_new(AST_FNC); e->sval = strdup("IDENT");
                                  expr_add_child(e, $1); expr_add_child(e, $3); $$ = e; }
            | expr5 T_DIFFER    expr6
                                { tree_t *e = expr_new(AST_FNC); e->sval = strdup("DIFFER");
                                  expr_add_child(e, $1); expr_add_child(e, $3); $$ = e; }
            | expr6
                                { $$ = $1; }
            ;
expr6       : expr6 T_2PLUS    expr9
                                { $$ = expr_binary(AST_ADD, $1, $3); }
            | expr6 T_2MINUS expr9
                                { $$ = expr_binary(AST_SUB, $1, $3); }
            | expr9
                                { $$ = $1; }
            ;
expr9       : expr9 T_2STAR expr11
                                { $$ = expr_binary(AST_MUL, $1, $3); }
            | expr9 T_2SLASH       expr11
                                { $$ = expr_binary(AST_DIV, $1, $3); }
            | expr11
                                { $$ = $1; }
            ;
expr11      : expr12 T_2CARET expr11
                                { $$ = expr_binary(AST_POW, $1, $3); }
            | expr12
                                { $$ = $1; }
            ;
expr12      : expr12 T_2DOLLAR expr15
                                { $$ = expr_binary(AST_CAPT_IMMED_ASGN, $1, $3); }
            | expr12 T_2DOT    expr15
                                { $$ = expr_binary(AST_CAPT_COND_ASGN,  $1, $3); }
            | expr15
                                { $$ = $1; }
            ;
expr15      : expr15 T_LBRACK exprlist T_RBRACK
                                { tree_t *idx = expr_new(AST_IDX);
                                  expr_add_child(idx, $1);
                                  for (int i = 0; i < $3->nchildren; i++)
                                      expr_add_child(idx, $3->children[i]);
                                  free($3->children); free($3);
                                  $$ = idx; }
            | expr17
                                { $$ = $1; }
            ;
exprlist    : exprlist_ne
                                { $$ = $1; }
            |
                                { $$ = expr_new(AST_NUL); }
            ;
exprlist_ne : exprlist_ne T_COMMA expr0
                                { expr_add_child($1, $3); $$ = $1; }
            | expr0
                                { tree_t *l = expr_new(AST_NUL); expr_add_child(l, $1); $$ = l; }
            ;
expr17      : T_CALL exprlist T_RPAREN
                                { tree_t *e = expr_new(AST_FNC);
                                  e->sval = $1;
                                  for (int i = 0; i < $2->nchildren; i++)
                                      expr_add_child(e, $2->children[i]);
                                  free($2->children); free($2);
                                  $$ = e; }
            | T_IDENT
                                { tree_t *e = expr_new(AST_VAR);
                                  e->sval = $1;
                                  $$ = e; }
            | T_KEYWORD
                                { tree_t *e = expr_new(AST_KEYWORD);
                                  e->sval = $1;
                                  $$ = e; }
            | T_INT
                                { $$ = sc_int_literal($1); free($1); }
            | T_REAL
                                { $$ = sc_real_literal($1); free($1); }
            | T_STR
                                { $$ = sc_str_literal($1); free($1); }
            | T_LPAREN expr0 T_RPAREN
                                { $$ = $2; }
            | T_LPAREN expr0 T_COMMA exprlist_ne T_RPAREN
                                { tree_t *a = expr_new(AST_VLIST);
                                  expr_add_child(a, $2);
                                  for (int i = 0; i < $4->nchildren; i++)
                                      expr_add_child(a, $4->children[i]);
                                  free($4->children); free($4);
                                  $$ = a; }
            | T_LPAREN T_RPAREN
                                { $$ = expr_new(AST_NUL); }
            | T_1PLUS  expr17
                                { $$ = expr_unary(AST_PLS, $2); }
            | T_1MINUS expr17
                                { $$ = expr_unary(AST_MNS, $2); }
            | T_1STAR   expr17  { $$ = expr_unary(AST_DEFER,       $2); }
            | T_1DOT    expr17  { $$ = expr_unary(AST_NAME,        $2); }
            | T_1DOLLAR expr17  { $$ = expr_unary(AST_INDIRECT,    $2); }
            | T_1AT     expr17  { $$ = expr_unary(AST_CAPT_CURSOR, $2); }
            | T_1TILDE  expr17  { $$ = expr_unary(AST_NOT,         $2); }
            | T_1QUEST  expr17  { $$ = expr_unary(AST_INTERROGATE, $2); }
            | T_1AMP    expr17  { tree_t *_e = expr_unary(AST_OPSYN, $2);
                                  _e->sval = strdup("&"); $$ = _e; }
            | T_1PERCENT expr17 { tree_t *_e = expr_unary(AST_OPSYN, $2);
                                  _e->sval = strdup("%"); $$ = _e; }
            | T_1SLASH   expr17 { tree_t *_e = expr_unary(AST_OPSYN, $2);
                                  _e->sval = strdup("/"); $$ = _e; }
            | T_1POUND   expr17 { tree_t *_e = expr_unary(AST_OPSYN, $2);
                                  _e->sval = strdup("#"); $$ = _e; }
            | T_1PIPE    expr17 { tree_t *_e = expr_unary(AST_OPSYN, $2);
                                  _e->sval = strdup("|"); $$ = _e; }
            | T_1EQUAL   expr17 { tree_t *_e = expr_unary(AST_OPSYN, $2);
                                  _e->sval = strdup("="); $$ = _e; }
            | T_1BANG    expr17 { tree_t *_e = expr_unary(AST_OPSYN, $2);
                                  _e->sval = strdup("!"); $$ = _e; }
            ;
%%
/*----------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------*/
void sc_error(ScParseState *st, const char *msg) {
    fprintf(stderr, "%s:%d: snocone parse error: %s\n",
            st->filename ? st->filename : "<stdin>",
            st->ctx ? st->ctx->line : 0,
            msg);
    st->nerrors++;
}
/*----------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------*/
static void sc_split_subject_pattern(tree_t **subj_io, tree_t **pat_io) {
    tree_t *subj = *subj_io;
    if (*pat_io || !subj) return;
    if (subj->kind == AST_SCAN && subj->nchildren == 2) {
        tree_t *new_subj = subj->children[0];
        tree_t *new_pat  = subj->children[1];
        free(subj->children);
        free(subj);
        *subj_io = new_subj;
        *pat_io  = new_pat;
        return;
    }
    if (subj->kind == AST_SEQ && subj->nchildren >= 2) {
        tree_t *first = subj->children[0];
        if (first->kind == AST_VAR || first->kind == AST_KEYWORD ||
            first->kind == AST_QLIT || first->kind == AST_INDIRECT) {
            int nc = subj->nchildren - 1;
            tree_t *rest;
            if (nc == 1) {
                rest = subj->children[1];
            } else {
                rest = expr_new(AST_SEQ);
                for (int i = 1; i < subj->nchildren; i++)
                    expr_add_child(rest, subj->children[i]);
            }
            free(subj->children);
            free(subj);
            *subj_io = first;
            *pat_io  = rest;
            return;
        }
    }
}
/*----------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------*/
static void sc_append_stmt(ScParseState *st, tree_t *top) {
    if (!top) return;
    sc_pending_label_clear(st);
    STMT_t *s = stmt_new();
    s->lineno = st->ctx ? st->ctx->line : 0;
    s->stno   = ++st->code->nstmts;
    if (top->kind == AST_ASSIGN && top->nchildren == 2) {
        s->subject     = top->children[0];
        s->replacement = top->children[1];
        s->has_eq      = 1;
        free(top->children);
        free(top);
    } else {
        s->subject = top;
    }
    sc_split_subject_pattern(&s->subject, &s->pattern);
    if (!st->code->head) st->code->head = st->code->tail = s;
    else { st->code->tail->next = s; st->code->tail = s; }
}
/*----------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------*/
static tree_t *sc_int_literal(const char *txt) {
    tree_t *e = expr_new(AST_ILIT);
    e->ival = strtol(txt, NULL, 10);
    return e;
}
/*----------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------*/
static tree_t *sc_real_literal(const char *txt) {
    tree_t *e = expr_new(AST_FLIT);
    e->dval = strtod(txt, NULL);
    return e;
}
/*----------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------*/
static tree_t *sc_str_literal(const char *txt) {
    tree_t *e = expr_new(AST_QLIT);
    e->sval = strdup(txt);
    return e;
}
/*----------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------*/
static char *sc_label_new(ScParseState *st, const char *prefix) {
    static int global_label_seq = 0;
    char buf[64];
    (void)st->label_seq;
    snprintf(buf, sizeof buf, "%s_%04d", prefix, ++global_label_seq);
    return strdup(buf);
}
/*----------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------*/
static struct IfHead *sc_if_head_new(ScParseState *st, tree_t *cond) {
    struct IfHead *h = calloc(1, sizeof *h);
    h->cond        = cond;
    h->before_body = st->code->tail;
    h->lineno      = st->ctx ? st->ctx->line : 0;
    return h;
}
/*----------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------*/
/*----------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------*/
/*----------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------*/
/* PST-SC-4e: slimmed ForHead carries only cond+step; snapshot/labels handled in grammar action */
static struct ForHead *sc_for_head_new_pst(ScParseState *st, tree_t *cond, tree_t *step) {
    (void)st;
    struct ForHead *h = calloc(1, sizeof *h);
    h->cond = cond;
    h->step = step;
    return h;
}
/*----------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------*/
/* PST-SC-4g (2026-05-16): func_head records name+argstr; no DEFINE call or goto emitted.
 * func_before_body snapshot taken so sc_finalize_function_pst can sc_collect_body. */
static struct FuncHead *sc_func_head_new_pst(ScParseState *st, char *name, char *argstr) {
    struct FuncHead *h  = calloc(1, sizeof *h);
    h->name             = strdup(name);
    h->argstr           = strdup(argstr);
    h->prev_func        = st->cur_func_name;
    st->cur_func_name   = h->name;
    st->func_before_body = st->code->tail;
    return h;
}
/*----------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------*/
/* PST-SC-4g: build TT_DEFINE(QLIT(name), QLIT(argstr), TT_PROGRAM(body)).
 * lower.c lower_define emits: DEFINE(name(args)) call, skip-goto, entry label, body, end. */
static void sc_finalize_function_pst(ScParseState *st, struct FuncHead *h)
{
    tree_t *body  = sc_collect_body(st, st->func_before_body);
    int slen = strlen(h->name) + 1 + strlen(h->argstr) + 2;
    char *sig = malloc((size_t)slen);
    snprintf(sig, (size_t)slen, "%s(%s)", h->name, h->argstr);
    tree_t *qname = ast_node_new(TT_QLIT); qname->sval = strdup(h->name);
    tree_t *qsig  = ast_node_new(TT_QLIT); qsig->sval  = sig;
    tree_t *def   = ast_node_new(TT_DEFINE);
    ast_push(def, qname);
    ast_push(def, qsig);
    ast_push(def, body);
    st->cur_func_name = h->prev_func;
    free(h->name); free(h->argstr); free(h);
    sc_append_stmt(st, def);
}
/*----------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------*/
static void sc_append_return(ScParseState *st, tree_t *retval) {
    sc_pending_label_clear(st);
    STMT_t *s = stmt_new();
    s->lineno = st->ctx ? st->ctx->line : 0;
    s->stno   = ++st->code->nstmts;
    if (retval && st->cur_func_name) {
        tree_t *lhs = expr_new(AST_VAR);
        lhs->sval   = strdup(st->cur_func_name);
        s->subject     = lhs;
        s->replacement = retval;
        s->has_eq      = 1;
    } else if (retval) {
        s->subject = retval;
    }
    s->goto_u = strdup("RETURN");
    if (!st->code->head) st->code->head = st->code->tail = s;
    else { st->code->tail->next = s; st->code->tail = s; }
}
/*----------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------*/
static void sc_append_freturn(ScParseState *st) {
    sc_pending_label_clear(st);
    STMT_t *s = stmt_new();
    s->lineno = st->ctx ? st->ctx->line : 0;
    s->stno   = ++st->code->nstmts;
    s->goto_u = strdup("FRETURN");
    if (!st->code->head) st->code->head = st->code->tail = s;
    else { st->code->tail->next = s; st->code->tail = s; }
}
/*----------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------*/
static void sc_append_nreturn(ScParseState *st) {
    sc_pending_label_clear(st);
    STMT_t *s = stmt_new();
    s->lineno = st->ctx ? st->ctx->line : 0;
    s->stno   = ++st->code->nstmts;
    s->goto_u = strdup("NRETURN");
    if (!st->code->head) st->code->head = st->code->tail = s;
    else { st->code->tail->next = s; st->code->tail = s; }
}
/*----------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------*/
static STMT_t *sc_make_label_stmt(ScParseState *st, char *label) {
    STMT_t *s = stmt_new();
    s->lineno = st->ctx ? st->ctx->line : 0;
    s->stno   = ++st->code->nstmts;
    s->label  = label;
    return s;
}
/*----------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------*/
static STMT_t *sc_make_cond_fail_stmt(ScParseState *st, tree_t *cond, char *fail_target, int lineno) {
    STMT_t *s = stmt_new();
    s->lineno = lineno;
    s->stno   = ++st->code->nstmts;
    if (cond && cond->kind == AST_ASSIGN && cond->nchildren == 2) {
        s->subject     = cond->children[0];
        s->replacement = cond->children[1];
        s->has_eq      = 1;
        free(cond->children);
        free(cond);
    } else {
        s->subject = cond;
    }
    sc_split_subject_pattern(&s->subject, &s->pattern);
    s->goto_f = fail_target;
    return s;
}
/*----------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------*/
static STMT_t *sc_make_goto_uncond_stmt(ScParseState *st, char *target) {
    STMT_t *s = stmt_new();
    s->lineno = st->ctx ? st->ctx->line : 0;
    s->stno   = ++st->code->nstmts;
    s->goto_u = target;
    return s;
}
/*----------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------*/
static void sc_emit_label_pad(ScParseState *st, char *label) {
    STMT_t *pad = sc_make_label_stmt(st, strdup(label));
    sc_append_chain(st, pad, pad);
    sc_pending_label_add(st, label);
}
/*----------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------*/
static void sc_append_goto_label(ScParseState *st, char *target) {
    sc_pending_label_clear(st);
    STMT_t *g = sc_make_goto_uncond_stmt(st, strdup(target));
    sc_append_chain(st, g, g);
}
/*----------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------*/
static void sc_splice_after(ScParseState *st, STMT_t *anchor,
                            STMT_t *chain_head, STMT_t *chain_tail) {
    if (!chain_head) return;
    if (!chain_tail) chain_tail = chain_head;
    if (anchor) {
        chain_tail->next = anchor->next;
        anchor->next     = chain_head;
        if (st->code->tail == anchor) st->code->tail = chain_tail;
    } else {
        chain_tail->next = st->code->head;
        st->code->head   = chain_head;
        if (!st->code->tail) st->code->tail = chain_tail;
    }
}
/*----------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------*/
static void sc_append_chain(ScParseState *st, STMT_t *chain_head, STMT_t *chain_tail) {
    if (!chain_head) return;
    if (!chain_tail) chain_tail = chain_head;
    if (!st->code->head) st->code->head = chain_head;
    else                 st->code->tail->next = chain_head;
    st->code->tail = chain_tail;
}
/*----------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------*/
static void sc_finalize_if_no_else(ScParseState *st, struct IfHead *h) {
    char   *Lend       = sc_label_new(st, "_Lend");
    STMT_t *cond_stmt  = sc_make_cond_fail_stmt(st, h->cond, strdup(Lend), h->lineno);
    STMT_t *end_label  = sc_make_label_stmt(st, Lend);
    sc_splice_after(st, h->before_body, cond_stmt, cond_stmt);
    sc_append_chain(st, end_label, end_label);
    free(h);
}
/*----------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------*/
static void sc_finalize_if_else(ScParseState *st, struct IfHead *h, STMT_t *before_else) {
    char   *Lelse     = sc_label_new(st, "_Lelse");
    char   *Lend      = sc_label_new(st, "_Lend");
    STMT_t *cond_stmt = sc_make_cond_fail_stmt(st, h->cond, strdup(Lelse), h->lineno);
    STMT_t *goto_end  = sc_make_goto_uncond_stmt(st, strdup(Lend));
    STMT_t *else_pad  = sc_make_label_stmt(st, Lelse);
    STMT_t *end_pad   = sc_make_label_stmt(st, Lend);
    sc_splice_after(st, h->before_body, cond_stmt, cond_stmt);
    STMT_t *anchor = (before_else == h->before_body) ? cond_stmt : before_else;
    goto_end->next = else_pad;
    sc_splice_after(st, anchor, goto_end, else_pad);
    sc_append_chain(st, end_pad, end_pad);
    free(h);
}
/*----------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------*/
/* PST-SC-4b (2026-05-16): collect stmts appended to CODE_t after `snapshot` into a TT_PROGRAM
 * tree node, removing them from CODE_t.  Returns a TT_PROGRAM whose children are
 * stmt_to_ast(s) for each collected STMT_t.  The collected STMT_t chain is freed.
 * If no stmts were added after snapshot, returns an empty TT_PROGRAM. */
static tree_t *sc_collect_body(ScParseState *st, STMT_t *snapshot)
{
    tree_t *block = ast_node_new(TT_PROGRAM);
    /* body starts at snapshot->next (or head if snapshot==NULL) */
    STMT_t *first = snapshot ? snapshot->next : st->code->head;
    if (!first) return block;                   /* nothing to collect */
    /* detach body from CODE_t */
    if (snapshot) snapshot->next = NULL;
    else          st->code->head = NULL;
    st->code->tail = snapshot;                  /* restore tail to snapshot */
    /* convert each STMT_t to a tree node and push into block */
    for (STMT_t *s = first; s; ) {
        STMT_t *nxt = s->next;
        ast_push(block, stmt_to_ast(s));        /* stmt_to_ast allocates; we keep result */
        free(s);                                /* free the STMT_t shell (not its tree fields) */
        s = nxt;
    }
    return block;
}
/*----------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------*/
/* PST-SC-4b: Emit TT_IF(cond, then_block) as a single statement in CODE_t.
 * `cond` is the raw expression from if_head.
 * `before_then` is the CODE_t tail snapshot taken BEFORE the then-body was parsed. */
static void sc_finalize_if_no_else_pst(ScParseState *st, tree_t *cond)
{
    /* before_then == NULL means nothing was in CODE_t before; then-body is everything */
    /* We need the snapshot that was taken when if_head fired.  Since if_head now just
     * returns the cond expr and no longer captures a snapshot, we capture it here using
     * the current CODE_t tail (the body was already appended by the sub-rules). */
    /* NOTE: this approach requires that the snapshot be taken BEFORE the body rules run.
     * We do this by storing it in a local we compute at the call site.
     * For now, we re-implement by snapshotting at if_head time via sc_if_before_body
     * stored in ScParseState.  See header changes below. */
    tree_t *then_block = sc_collect_body(st, st->if_before_body);
    tree_t *if_node    = ast_node_new(TT_IF);
    ast_push(if_node, cond);
    ast_push(if_node, then_block);
    /* wrap TT_IF in a STMT_t so it reaches lower() via the normal CODE_t → TT_PROGRAM path */
    sc_append_stmt(st, if_node);
}
/*----------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------*/
/* PST-SC-4b: Emit TT_IF(cond, then_block, else_block) as a single statement.
 * `before_else` is the CODE_t tail snapshot taken at the T_ELSE token. */
static void sc_finalize_if_else_pst(ScParseState *st, tree_t *cond, STMT_t *before_else)
{
    tree_t *else_block = sc_collect_body(st, before_else);
    tree_t *then_block = sc_collect_body(st, st->if_before_body);
    tree_t *if_node    = ast_node_new(TT_IF);
    ast_push(if_node, cond);
    ast_push(if_node, then_block);
    ast_push(if_node, else_block);
    sc_append_stmt(st, if_node);
}
/*----------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------*/
/* PST-SC-4c (2026-05-16): pure-syntax while finalizer.
 * Builds TT_WHILE(cond, TT_PROGRAM(body), TT_QLIT(cont_lbl), TT_QLIT(end_lbl)).
 * Label strings are stored as QLIT children so lower.c can call labtab_define
 * at the right instruction positions without emitting SM_LABEL instructions.
 * sc_loop_push was called in while_head; sc_loop_pop resolves break/continue here. */
static void sc_finalize_while_pst(ScParseState *st, tree_t *cond)
{
    LoopFrame *lf     = st->loop_top;
    char      *Ltop   = lf ? strdup(lf->cont_label) : sc_label_new(st, "_Ltop");
    char      *Lend   = lf ? strdup(lf->end_label)  : sc_label_new(st, "_Lend");
    tree_t    *body   = sc_collect_body(st, st->while_before_body);
    tree_t    *qlit_c = ast_node_new(TT_QLIT); qlit_c->sval = Ltop;
    tree_t    *qlit_e = ast_node_new(TT_QLIT); qlit_e->sval = Lend;
    tree_t    *w      = ast_node_new(TT_WHILE);
    ast_push(w, cond);
    ast_push(w, body);
    ast_push(w, qlit_c);
    ast_push(w, qlit_e);
    sc_loop_pop(st);
    sc_append_stmt(st, w);
}
/*----------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------*/
static STMT_t *sc_make_cond_succ_stmt(ScParseState *st, tree_t *cond, char *succ_target, int lineno) {
    STMT_t *s = stmt_new();
    s->lineno  = lineno;
    s->stno    = ++st->code->nstmts;
    s->subject = cond;
    sc_split_subject_pattern(&s->subject, &s->pattern);
    s->goto_s = succ_target;
    return s;
}
/*----------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------*/
/* PST-SC-4d (2026-05-16): pure-syntax do-while finalizer.
 * Builds TT_DO_WHILE(TT_PROGRAM(body), cond, TT_QLIT(cont_lbl), TT_QLIT(end_lbl)).
 * Body is child 0 (executed first); cond is child 1 (tested at loop bottom).
 * Label QLITs in c[2]/c[3] let lower.c call labtab_define without emitting SM_LABEL.
 * sc_loop_push was called in do_head; sc_loop_pop resolves break/continue here. */
static void sc_finalize_do_while_pst(ScParseState *st, tree_t *cond)
{
    LoopFrame *lf     = st->loop_top;
    char      *Lcont  = lf ? strdup(lf->cont_label) : sc_label_new(st, "_Lcont");
    char      *Lend   = lf ? strdup(lf->end_label)  : sc_label_new(st, "_Lend");
    tree_t    *body   = sc_collect_body(st, st->do_before_body);
    tree_t    *qlit_c = ast_node_new(TT_QLIT); qlit_c->sval = Lcont;
    tree_t    *qlit_e = ast_node_new(TT_QLIT); qlit_e->sval = Lend;
    tree_t    *dw     = ast_node_new(TT_DO_WHILE);
    ast_push(dw, body);
    ast_push(dw, cond);
    ast_push(dw, qlit_c);
    ast_push(dw, qlit_e);
    sc_loop_pop(st);
    sc_append_stmt(st, dw);
}
/*----------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------*/
/* PST-SC-4e (2026-05-16): pure-syntax for-loop finalizer.
 * Builds TT_FOR(cond, step, TT_PROGRAM(body), TT_QLIT(cont_lbl), TT_QLIT(end_lbl)).
 * Init was already appended as a preceding statement (via sc_append_stmt in for_head action).
 * Body collected from for_before_body snapshot. Labels from sc_loop_push in grammar action. */
static void sc_finalize_for_pst(ScParseState *st, struct ForHead *h)
{
    LoopFrame *lf     = st->loop_top;
    char      *Lcont  = lf ? strdup(lf->cont_label) : sc_label_new(st, "_Lcont");
    char      *Lend   = lf ? strdup(lf->end_label)  : sc_label_new(st, "_Lend");
    tree_t    *body   = sc_collect_body(st, st->for_before_body);
    tree_t    *qlit_c = ast_node_new(TT_QLIT); qlit_c->sval = Lcont;
    tree_t    *qlit_e = ast_node_new(TT_QLIT); qlit_e->sval = Lend;
    tree_t    *f      = ast_node_new(TT_FOR);
    ast_push(f, h->cond);
    ast_push(f, h->step);
    ast_push(f, body);
    ast_push(f, qlit_c);
    ast_push(f, qlit_e);
    sc_loop_pop(st);
    sc_append_stmt(st, f);
    free(h);
}
/*----------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------*/
static void sc_pending_label_add(ScParseState *st, const char *name) {
    if (st->pending_user_labels_count >= st->pending_user_labels_cap) {
        int newcap = st->pending_user_labels_cap ? st->pending_user_labels_cap * 2 : 4;
        st->pending_user_labels = realloc(st->pending_user_labels, newcap * sizeof(char *));
        st->pending_user_labels_cap = newcap;
    }
    st->pending_user_labels[st->pending_user_labels_count++] = strdup(name);
}
/*----------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------*/
static void sc_pending_label_clear(ScParseState *st) {
    for (int i = 0; i < st->pending_user_labels_count; i++) free(st->pending_user_labels[i]);
    st->pending_user_labels_count = 0;
}
/*----------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------*/
static void sc_pending_to_stash(ScParseState *st) {
    for (int i = 0; i < st->stash_for_pending_labels_count; i++) free(st->stash_for_pending_labels[i]);
    free(st->stash_for_pending_labels);
    st->stash_for_pending_labels       = st->pending_user_labels;
    st->stash_for_pending_labels_count = st->pending_user_labels_count;
    st->pending_user_labels       = NULL;
    st->pending_user_labels_count = 0;
    st->pending_user_labels_cap   = 0;
}
/*----------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------*/
static void sc_loop_push(ScParseState *st, char *cont_label, char *end_label, int is_loop, int from_stash) {
    LoopFrame *f = calloc(1, sizeof *f);
    f->cont_label = cont_label;
    f->end_label  = end_label;
    f->is_loop    = is_loop;
    f->outer      = st->loop_top;
    if (from_stash) {
        f->user_labels       = st->stash_for_pending_labels;
        f->user_labels_count = st->stash_for_pending_labels_count;
        st->stash_for_pending_labels       = NULL;
        st->stash_for_pending_labels_count = 0;
    } else {
        f->user_labels       = st->pending_user_labels;
        f->user_labels_count = st->pending_user_labels_count;
        st->pending_user_labels       = NULL;
        st->pending_user_labels_count = 0;
        st->pending_user_labels_cap   = 0;
    }
    st->loop_top = f;
}
/*----------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------*/
static void sc_loop_pop(ScParseState *st) {
    LoopFrame *f = st->loop_top;
    if (!f) return;
    st->loop_top = f->outer;
    free(f->cont_label);
    free(f->end_label);
    for (int i = 0; i < f->user_labels_count; i++) free(f->user_labels[i]);
    free(f->user_labels);
    free(f);
}
/*----------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------*/
static LoopFrame *sc_loop_find_by_user_label(ScParseState *st, const char *name, int want_loop) {
    for (LoopFrame *f = st->loop_top; f; f = f->outer) {
        if (want_loop && !f->is_loop) continue;
        for (int i = 0; i < f->user_labels_count; i++) {
            if (strcmp(f->user_labels[i], name) == 0) return f;
        }
    }
    return NULL;
}
/*----------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------*/
static LoopFrame *sc_loop_find_innermost(ScParseState *st, int want_loop) {
    for (LoopFrame *f = st->loop_top; f; f = f->outer) {
        if (want_loop && !f->is_loop) continue;
        return f;
    }
    return NULL;
}
/*----------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------*/
static void sc_append_break(ScParseState *st, char *user_label) {
    LoopFrame *f = user_label
        ? sc_loop_find_by_user_label(st, user_label, 0)
        : sc_loop_find_innermost(st, 0);
    if (!f) {
        if (user_label) {
            char buf[256];
            snprintf(buf, sizeof buf, "break: no enclosing loop or switch labeled '%s'", user_label);
            sc_error(st, buf);
        } else {
            sc_error(st, "break outside of loop or switch");
        }
        sc_pending_label_clear(st);
        return;
    }
    sc_pending_label_clear(st);
    STMT_t *g = sc_make_goto_uncond_stmt(st, strdup(f->end_label));
    sc_append_chain(st, g, g);
}
/*----------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------*/
static void sc_append_continue(ScParseState *st, char *user_label) {
    LoopFrame *f = user_label
        ? sc_loop_find_by_user_label(st, user_label, 1)
        : sc_loop_find_innermost(st, 1);
    if (!f) {
        if (user_label) {
            char buf[256];
            snprintf(buf, sizeof buf, "continue: no enclosing loop labeled '%s'", user_label);
            sc_error(st, buf);
        } else {
            sc_error(st, "continue outside of loop");
        }
        sc_pending_label_clear(st);
        return;
    }
    f->cont_used = 1;          /* tells finalize_* to emit the Lcont pad (do/for) */
    sc_pending_label_clear(st);
    STMT_t *g = sc_make_goto_uncond_stmt(st, strdup(f->cont_label));
    sc_append_chain(st, g, g);
}
/*----------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------*/
static void sc_switch_cases_grow(struct SwitchHead *h) {
    if (h->cases_count >= h->cases_cap) {
        int newcap = h->cases_cap ? h->cases_cap * 2 : 4;
        h->cases = realloc(h->cases, newcap * sizeof *h->cases);
        h->cases_cap = newcap;
    }
}
/*----------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------*/
/* PST-SC-4f (2026-05-16): switch head records disc and labels; no tmp-assign emitted.
 * lower.c will handle the IDENT comparisons directly from TT_CASE children. */
static struct SwitchHead *sc_switch_head_new(ScParseState *st, tree_t *disc) {
    struct SwitchHead *h = calloc(1, sizeof *h);
    h->disc          = disc;
    h->lineno        = st->ctx ? st->ctx->line : 0;
    h->prev_switch   = st->cur_switch;
    h->end_label     = sc_label_new(st, "_Lend");
    h->default_label = sc_label_new(st, "_Ldefault");
    h->has_default   = 0;
    h->tmp_name      = NULL;
    h->after_tmp_assign     = NULL;
    h->last_case_label_tail = NULL;
    sc_loop_push(st, strdup(h->end_label), strdup(h->end_label), 0, 0);
    st->cur_switch = h;
    return h;
}
/*----------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------*/
/* PST-SC-4f: no implicit break gotos in the pure syntax tree — lower handles fallthrough */
static void sc_switch_emit_implicit_break(ScParseState *st, struct SwitchHead *h) {
    (void)st; (void)h;
}
/*----------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------*/
/* PST-SC-4f: record (value, before_body snapshot) — no label STMT_t emitted */
static void sc_switch_case_label(ScParseState *st, tree_t *value) {
    struct SwitchHead *h = st->cur_switch;
    if (!h) { sc_error(st, "case label outside of switch"); (void)value; return; }
    sc_switch_cases_grow(h);
    h->cases[h->cases_count].value       = value;
    h->cases[h->cases_count].case_label  = NULL;
    h->cases[h->cases_count].before_body = st->code->tail;
    h->cases_count++;
    sc_pending_label_clear(st);
}
/*----------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------*/
/* PST-SC-4f: record default arm with NULL value and before_body snapshot */
static void sc_switch_default_label(ScParseState *st) {
    struct SwitchHead *h = st->cur_switch;
    if (!h) { sc_error(st, "default label outside of switch"); return; }
    if (h->has_default) { sc_error(st, "duplicate default label in switch"); return; }
    h->has_default = 1;
    sc_switch_cases_grow(h);
    h->cases[h->cases_count].value       = NULL;
    h->cases[h->cases_count].case_label  = NULL;
    h->cases[h->cases_count].before_body = st->code->tail;
    h->cases_count++;
    sc_pending_label_clear(st);
}
/*----------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------*/
/* PST-SC-4f (2026-05-16): pure-syntax switch finalizer.
 * Builds TT_CASE(disc, val1, TT_PROGRAM(body1), val2, TT_PROGRAM(body2), ..., [TT_PROGRAM(default)]).
 * Bodies collected in reverse order (last arm first) so sc_collect_body's forward-scan works.
 * NULL value child marks the default arm. QLIT(end_lbl) is last child for lower.c.
 * lower_case already handles TT_CASE n-ary flat structure; Snocone bodies are TT_PROGRAM. */
static void sc_finalize_switch_pst(ScParseState *st, struct SwitchHead *h)
{
    int nc = h->cases_count;
    /* Collect bodies in reverse — each sc_collect_body shortens CODE_t tail */
    tree_t **bodies = calloc((size_t)(nc > 0 ? nc : 1), sizeof *bodies);
    for (int i = nc - 1; i >= 0; i--)
        bodies[i] = sc_collect_body(st, h->cases[i].before_body);
    /* Build TT_CASE node */
    tree_t *node   = ast_node_new(TT_CASE);
    tree_t *qlit_e = ast_node_new(TT_QLIT); qlit_e->sval = strdup(h->end_label);
    ast_push(node, h->disc);
    for (int i = 0; i < nc; i++) {
        /* value: NULL for default → push TT_NUL placeholder */
        if (h->cases[i].value)
            ast_push(node, h->cases[i].value);
        else {
            tree_t *nul = ast_node_new(TT_NUL); ast_push(node, nul);
        }
        ast_push(node, bodies[i]);
    }
    ast_push(node, qlit_e);
    free(bodies);
    sc_loop_pop(st);
    st->cur_switch = h->prev_switch;
    for (int i = 0; i < nc; i++) free(h->cases[i].case_label);
    free(h->cases);
    free(h->end_label);
    free(h->default_label);
    free(h->tmp_name);
    free(h);
    sc_append_stmt(st, node);
}
/*----------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------*/
static void sc_emit_struct(ScParseState *st, char *name, char *fields) {
    int slen = strlen(name) + 1 + strlen(fields) + 2;
    char *spec = malloc(slen);
    snprintf(spec, slen, "%s(%s)", name, fields);
    tree_t *qarg = expr_new(AST_QLIT);
    qarg->sval   = spec;
    tree_t *data_call = expr_new(AST_FNC);
    data_call->sval   = strdup("DATA");
    expr_add_child(data_call, qarg);
    sc_append_stmt(st, data_call);
}
/*----------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------*/
CODE_t *snocone_parse_program(const char *src, const char *filename) {
    LexCtx          ctx = {0};
    ctx.p           = src ? src : "";
    ctx.line        = 1;
    ScParseState    state = {0};
    state.ctx       = (struct LexCtx *)&ctx;
    state.code      = calloc(1, sizeof *state.code);
    state.filename  = filename;
    state.nerrors   = 0;
    int rc = sc_parse(&state);
    sc_pending_label_clear(&state);
    free(state.pending_user_labels);
    for (int i = 0; i < state.stash_for_pending_labels_count; i++)
        free(state.stash_for_pending_labels[i]);
    free(state.stash_for_pending_labels);
    while (state.loop_top) sc_loop_pop(&state);
    if (rc != 0 || state.nerrors > 0) {
        free(state.code);
        return NULL;
    }
    return state.code;
}
