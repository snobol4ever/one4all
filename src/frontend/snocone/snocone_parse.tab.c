/* A Bison parser, made by GNU Bison 3.8.2.  */

/* Bison implementation for Yacc-like parsers in C

   Copyright (C) 1984, 1989-1990, 2000-2015, 2018-2021 Free Software Foundation,
   Inc.

   This program is free software: you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation, either version 3 of the License, or
   (at your option) any later version.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with this program.  If not, see <https://www.gnu.org/licenses/>.  */

/* As a special exception, you may create a larger work that contains
   part or all of the Bison parser skeleton and distribute that work
   under terms of your choice, so long as that work isn't itself a
   parser generator using the skeleton or a modified version thereof
   as a parser skeleton.  Alternatively, if you modify or redistribute
   the parser skeleton itself, you may (at your option) remove this
   special exception, which will cause the skeleton and the resulting
   Bison output files to be licensed under the GNU General Public
   License without this special exception.

   This special exception was added by the Free Software Foundation in
   version 2.2 of Bison.  */

/* C LALR(1) parser skeleton written by Richard Stallman, by
   simplifying the original so-called "semantic" parser.  */

/* DO NOT RELY ON FEATURES THAT ARE NOT DOCUMENTED in the manual,
   especially those whose name start with YY_ or yy_.  They are
   private implementation details that can be changed or removed.  */

/* All symbols defined below should begin with yy or YY, to avoid
   infringing on user name space.  This should be done even for local
   variables, as they might otherwise be expanded by user macros.
   There are some unavoidable exceptions within include files to
   define necessary library symbols; they are noted "INFRINGES ON
   USER NAME SPACE" below.  */

/* Identify Bison output, and Bison version.  */
#define YYBISON 30802

/* Bison version string.  */
#define YYBISON_VERSION "3.8.2"

/* Skeleton name.  */
#define YYSKELETON_NAME "yacc.c"

/* Pure parsers.  */
#define YYPURE 2

/* Push parsers.  */
#define YYPUSH 0

/* Pull parsers.  */
#define YYPULL 1

/* "%code top" blocks.  */
#line 59 "snocone_parse.y"

/* Token kinds (T_*) come from snocone_parse.tab.h — Bison's generated
 * enum sc_tokentype is the single source of truth.  snocone_lex.c
 * includes that header to resolve T_* names; snocone_lex.h (the
 * lexer's API) returns kinds as plain `int` so it does not pull in
 * tab.h.  No parallel `ScKind` enum, no #define alias dance, no
 * per-token translation table.  Post-cleanup design (session
 * 2026-05-01, GOAL-SNOCONE-BEAUTY SB-6.H). */

#line 78 "snocone_parse.tab.c"
/* Substitute the type names.  */
#define YYSTYPE         SC_STYPE
/* Substitute the variable and function names.  */
#define yyparse         sc_parse
#define yylex           sc_lex
#define yyerror         sc_error
#define yydebug         sc_debug
#define yynerrs         sc_nerrs


# ifndef YY_CAST
#  ifdef __cplusplus
#   define YY_CAST(Type, Val) static_cast<Type> (Val)
#   define YY_REINTERPRET_CAST(Type, Val) reinterpret_cast<Type> (Val)
#  else
#   define YY_CAST(Type, Val) ((Type) (Val))
#   define YY_REINTERPRET_CAST(Type, Val) ((Type) (Val))
#  endif
# endif
# ifndef YY_NULLPTR
#  if defined __cplusplus
#   if 201103L <= __cplusplus
#    define YY_NULLPTR nullptr
#   else
#    define YY_NULLPTR 0
#   endif
#  else
#   define YY_NULLPTR ((void*)0)
#  endif
# endif

#include "snocone_parse.tab.h"
/* Symbol kind.  */
enum yysymbol_kind_t
{
  YYSYMBOL_YYEMPTY = -2,
  YYSYMBOL_YYEOF = 0,                      /* "end of file"  */
  YYSYMBOL_YYerror = 1,                    /* error  */
  YYSYMBOL_YYUNDEF = 2,                    /* "invalid token"  */
  YYSYMBOL_T_IDENT = 3,                    /* T_IDENT  */
  YYSYMBOL_T_KEYWORD = 4,                  /* T_KEYWORD  */
  YYSYMBOL_T_INT = 5,                      /* T_INT  */
  YYSYMBOL_T_REAL = 6,                     /* T_REAL  */
  YYSYMBOL_T_STR = 7,                      /* T_STR  */
  YYSYMBOL_T_CALL = 8,                     /* T_CALL  */
  YYSYMBOL_T_2PLUS = 9,                    /* T_2PLUS  */
  YYSYMBOL_T_2MINUS = 10,                  /* T_2MINUS  */
  YYSYMBOL_T_2STAR = 11,                   /* T_2STAR  */
  YYSYMBOL_T_2SLASH = 12,                  /* T_2SLASH  */
  YYSYMBOL_T_2CARET = 13,                  /* T_2CARET  */
  YYSYMBOL_T_EQ = 14,                      /* T_EQ  */
  YYSYMBOL_T_NE = 15,                      /* T_NE  */
  YYSYMBOL_T_LT = 16,                      /* T_LT  */
  YYSYMBOL_T_GT = 17,                      /* T_GT  */
  YYSYMBOL_T_LE = 18,                      /* T_LE  */
  YYSYMBOL_T_GE = 19,                      /* T_GE  */
  YYSYMBOL_T_LEQ = 20,                     /* T_LEQ  */
  YYSYMBOL_T_LNE = 21,                     /* T_LNE  */
  YYSYMBOL_T_LLT = 22,                     /* T_LLT  */
  YYSYMBOL_T_LGT = 23,                     /* T_LGT  */
  YYSYMBOL_T_LLE = 24,                     /* T_LLE  */
  YYSYMBOL_T_LGE = 25,                     /* T_LGE  */
  YYSYMBOL_T_IDENT_OP = 26,                /* T_IDENT_OP  */
  YYSYMBOL_T_DIFFER = 27,                  /* T_DIFFER  */
  YYSYMBOL_T_1PLUS = 28,                   /* T_1PLUS  */
  YYSYMBOL_T_1MINUS = 29,                  /* T_1MINUS  */
  YYSYMBOL_T_2EQUAL = 30,                  /* T_2EQUAL  */
  YYSYMBOL_T_PLUS_ASSIGN = 31,             /* T_PLUS_ASSIGN  */
  YYSYMBOL_T_MINUS_ASSIGN = 32,            /* T_MINUS_ASSIGN  */
  YYSYMBOL_T_STAR_ASSIGN = 33,             /* T_STAR_ASSIGN  */
  YYSYMBOL_T_SLASH_ASSIGN = 34,            /* T_SLASH_ASSIGN  */
  YYSYMBOL_T_CARET_ASSIGN = 35,            /* T_CARET_ASSIGN  */
  YYSYMBOL_T_2QUEST = 36,                  /* T_2QUEST  */
  YYSYMBOL_T_2PIPE = 37,                   /* T_2PIPE  */
  YYSYMBOL_T_CONCAT = 38,                  /* T_CONCAT  */
  YYSYMBOL_T_LPAREN = 39,                  /* T_LPAREN  */
  YYSYMBOL_T_RPAREN = 40,                  /* T_RPAREN  */
  YYSYMBOL_T_SEMICOLON = 41,               /* T_SEMICOLON  */
  YYSYMBOL_T_COMMA = 42,                   /* T_COMMA  */
  YYSYMBOL_T_LBRACK = 43,                  /* T_LBRACK  */
  YYSYMBOL_T_RBRACK = 44,                  /* T_RBRACK  */
  YYSYMBOL_T_2DOLLAR = 45,                 /* T_2DOLLAR  */
  YYSYMBOL_T_2DOT = 46,                    /* T_2DOT  */
  YYSYMBOL_T_2AMP = 47,                    /* T_2AMP  */
  YYSYMBOL_T_2AT = 48,                     /* T_2AT  */
  YYSYMBOL_T_2POUND = 49,                  /* T_2POUND  */
  YYSYMBOL_T_2PERCENT = 50,                /* T_2PERCENT  */
  YYSYMBOL_T_2TILDE = 51,                  /* T_2TILDE  */
  YYSYMBOL_T_1STAR = 52,                   /* T_1STAR  */
  YYSYMBOL_T_1SLASH = 53,                  /* T_1SLASH  */
  YYSYMBOL_T_1PERCENT = 54,                /* T_1PERCENT  */
  YYSYMBOL_T_1AT = 55,                     /* T_1AT  */
  YYSYMBOL_T_1TILDE = 56,                  /* T_1TILDE  */
  YYSYMBOL_T_1DOLLAR = 57,                 /* T_1DOLLAR  */
  YYSYMBOL_T_1DOT = 58,                    /* T_1DOT  */
  YYSYMBOL_T_1POUND = 59,                  /* T_1POUND  */
  YYSYMBOL_T_1PIPE = 60,                   /* T_1PIPE  */
  YYSYMBOL_T_1EQUAL = 61,                  /* T_1EQUAL  */
  YYSYMBOL_T_1QUEST = 62,                  /* T_1QUEST  */
  YYSYMBOL_T_1AMP = 63,                    /* T_1AMP  */
  YYSYMBOL_T_1BANG = 64,                   /* T_1BANG  */
  YYSYMBOL_T_COLON = 65,                   /* T_COLON  */
  YYSYMBOL_T_DO = 66,                      /* T_DO  */
  YYSYMBOL_T_FOR = 67,                     /* T_FOR  */
  YYSYMBOL_T_SWITCH = 68,                  /* T_SWITCH  */
  YYSYMBOL_T_CASE = 69,                    /* T_CASE  */
  YYSYMBOL_T_DEFAULT = 70,                 /* T_DEFAULT  */
  YYSYMBOL_T_BREAK = 71,                   /* T_BREAK  */
  YYSYMBOL_T_CONTINUE = 72,                /* T_CONTINUE  */
  YYSYMBOL_T_GOTO = 73,                    /* T_GOTO  */
  YYSYMBOL_T_DEFINE = 74,                  /* T_DEFINE  */
  YYSYMBOL_T_RETURN = 75,                  /* T_RETURN  */
  YYSYMBOL_T_FRETURN = 76,                 /* T_FRETURN  */
  YYSYMBOL_T_NRETURN = 77,                 /* T_NRETURN  */
  YYSYMBOL_T_STRUCT = 78,                  /* T_STRUCT  */
  YYSYMBOL_T_UNKNOWN = 79,                 /* T_UNKNOWN  */
  YYSYMBOL_T_LBRACE = 80,                  /* T_LBRACE  */
  YYSYMBOL_T_RBRACE = 81,                  /* T_RBRACE  */
  YYSYMBOL_T_IF = 82,                      /* T_IF  */
  YYSYMBOL_T_ELSE = 83,                    /* T_ELSE  */
  YYSYMBOL_T_WHILE = 84,                   /* T_WHILE  */
  YYSYMBOL_YYACCEPT = 85,                  /* $accept  */
  YYSYMBOL_program = 86,                   /* program  */
  YYSYMBOL_stmt_list = 87,                 /* stmt_list  */
  YYSYMBOL_stmt = 88,                      /* stmt  */
  YYSYMBOL_matched_stmt = 89,              /* matched_stmt  */
  YYSYMBOL_unmatched_stmt = 90,            /* unmatched_stmt  */
  YYSYMBOL_if_head = 91,                   /* if_head  */
  YYSYMBOL_while_head = 92,                /* while_head  */
  YYSYMBOL_do_head = 93,                   /* do_head  */
  YYSYMBOL_do_body = 94,                   /* do_body  */
  YYSYMBOL_for_lead = 95,                  /* for_lead  */
  YYSYMBOL_for_head = 96,                  /* for_head  */
  YYSYMBOL_switch_head = 97,               /* switch_head  */
  YYSYMBOL_switch_body = 98,               /* switch_body  */
  YYSYMBOL_case_clause = 99,               /* case_clause  */
  YYSYMBOL_case_or_default_label = 100,    /* case_or_default_label  */
  YYSYMBOL_opt_head_sep = 101,             /* opt_head_sep  */
  YYSYMBOL_func_head = 102,                /* func_head  */
  YYSYMBOL_func_arglist = 103,             /* func_arglist  */
  YYSYMBOL_func_arglist_ne = 104,          /* func_arglist_ne  */
  YYSYMBOL_struct_field_list = 105,        /* struct_field_list  */
  YYSYMBOL_else_keyword = 106,             /* else_keyword  */
  YYSYMBOL_label_decl = 107,               /* label_decl  */
  YYSYMBOL_simple_stmt = 108,              /* simple_stmt  */
  YYSYMBOL_block_stmt = 109,               /* block_stmt  */
  YYSYMBOL_expr0 = 110,                    /* expr0  */
  YYSYMBOL_expr1 = 111,                    /* expr1  */
  YYSYMBOL_expr3 = 112,                    /* expr3  */
  YYSYMBOL_expr4 = 113,                    /* expr4  */
  YYSYMBOL_expr5 = 114,                    /* expr5  */
  YYSYMBOL_expr6 = 115,                    /* expr6  */
  YYSYMBOL_expr9 = 116,                    /* expr9  */
  YYSYMBOL_expr11 = 117,                   /* expr11  */
  YYSYMBOL_expr12 = 118,                   /* expr12  */
  YYSYMBOL_expr15 = 119,                   /* expr15  */
  YYSYMBOL_exprlist = 120,                 /* exprlist  */
  YYSYMBOL_exprlist_ne = 121,              /* exprlist_ne  */
  YYSYMBOL_expr17 = 122                    /* expr17  */
};
typedef enum yysymbol_kind_t yysymbol_kind_t;



/* Unqualified %code blocks.  */
#line 158 "snocone_parse.y"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include "snocone_lex.h"     /* full LexCtx definition + sc_lex_next API */

/* Forward declarations of yylex/yyerror (Bison's generated parser
 * references them via the api.prefix-mangled names sc_lex / sc_error
 * before their definitions appear after %% — without these forward
 * decls the compiler emits implicit-declaration warnings at the
 * yychar = yylex(...) call site inside sc_parse). */
int  sc_lex  (SC_STYPE *yylval, ScParseState *st);
void sc_error(ScParseState *st, const char *msg);

/* Helpers — defined after %% */
static void     sc_append_stmt        (ScParseState *st, AST_t *top);
static void     sc_split_subject_pattern(AST_t **subj_io, AST_t **pat_io); /* LS-4.l */
static AST_t  *sc_int_literal        (const char *txt);
static AST_t  *sc_real_literal       (const char *txt);
static AST_t  *sc_str_literal        (const char *txt);
static AST_t  *sc_clone_expr_simple  (AST_t *e);  /* LS-4.c — for compound-assigns */

/* LS-4.f — control-flow lowering helpers.
 * Architecture: control-flow head non-terminals (if_head, while_head)
 * snapshot the linked-list tail BEFORE the body is parsed and return
 * the snapshot via a heap-allocated handoff struct.  The body's stmts
 * append normally to st->code in their order.  The parent rule's
 * final action calls sc_finalize_* with the head struct (and, for
 * if-else, also the else_keyword snapshot of where the else-body
 * begins) to splice the cond-stmt and label-stmts into the correct
 * positions.  This emit-and-splice avoids needing MRAs in the middle
 * of the rule, which is what causes reduce/reduce conflicts in
 * Bison's balanced if-else grammars.
 */
struct IfHead {
    AST_t *cond;          /* condition expression                              */
    STMT_t *before_body;   /* st->code->tail snapshot at end of if_head reduction
                              (NULL if list was empty); body starts at
                              before_body->next or st->code->head if NULL.     */
    int     lineno;        /* lineno of if_head for the cond-stmt's lineno     */
};
struct WhileHead {
    AST_t *cond;
    STMT_t *before_body;
    int     lineno;
    /* LS-4.i.2 — synthetic labels owned by the head, allocated eagerly so
     * break/continue stmts emitted DURING body parsing target them by
     * name.  finalize_while uses these same names instead of allocating
     * fresh ones. */
    char   *cont_label;     /* loop top — continue target */
    char   *end_label;      /* loop end — break target */
};
/* LS-4.g — do/while: snapshot before body (at KW_DO), cond
 * provided when the trailing while clause is parsed. */
struct DoHead {
    STMT_t *before_body;   /* tail snapshot at KW_DO */
    int     lineno;
    /* LS-4.i.2 — eager labels.  cont_label is a fresh "_Lcont_NNNN" pad
     * that finalize_do_while splices in just before the trailing
     * cond stmt — `continue;` in a do/while re-evaluates the cond. */
    char   *cont_label;
    char   *end_label;
};
/* LS-4.g — for (init; cond; step): snapshot after init emits, before
 * the loop-top label and cond-stmt are spliced. */
struct ForHead {
    STMT_t *before_loop;   /* tail snapshot after init stmt appended */
    AST_t *cond;          /* loop condition */
    AST_t *step;          /* step expression (emitted at loop bottom) */
    int     lineno;
    /* LS-4.i.2 — eager labels.  cont_label is "_Lcont_NNNN" — `continue;`
     * in a for-loop must execute the step expression before re-testing
     * cond, so its target is just before the step stmt, not Ltop. */
    char   *cont_label;
    char   *end_label;
};
/* LS-4.h — function definition handoff.
 *
 * `function NAME(args) { body }` lowers to:
 *
 *      <existing stmts up to before_func>
 *      DEFINE('NAME(args)')              :(NAME_end)        <- emitted by sc_func_head_new
 *      NAME    <body-stmts>                                  <- spliced by sc_finalize_function
 *      NAME_end                                              <- appended by sc_finalize_function
 *
 * This matches the SPITBOL idiom: the DEFINE installs the function
 * descriptor at load time, the unconditional goto jumps over the body
 * so it does not execute as straight-line code, the entry-point label
 * `NAME` is where calls land, and `NAME_end` is the skip-target.
 *
 * Inside the body, return/freturn/nreturn use the cur_func_name field
 * (saved by sc_func_head_new and restored by sc_finalize_function) to
 * emit the SPITBOL `:(RETURN)`, `:(FRETURN)`, `:(NRETURN)` branches.
 *
 * Nested function definitions are not supported (Andrew's `.sc`
 * doesn't have them either) — the implementation assumes
 * cur_func_name is NULL when sc_func_head_new fires.
 */
struct FuncHead {
    char   *name;          /* function name — used as entry-point label,
                              and for the prefix of the end-skip label */
    char   *end_label;     /* "NAME_end" — the skip target */
    char   *prev_func;     /* saved cur_func_name (for nested-function safety) */
    STMT_t *after_goto;    /* tail snapshot AFTER the DEFINE+goto stmts,
                              before any body stmt is appended.  The
                              entry-point label `NAME` is spliced just
                              after this anchor. */
    int     lineno;
};

/* LS-4.i.3 — switch / case / default lowering handoff.
 *
 *   switch (e) { case v1: A; case v2: B; default: D; }
 *
 * lowers to:
 *
 *      tmp = e                                 <- emitted by sc_switch_head_new
 *      IDENT(tmp, v1)        :S(_Lcase_0001)   <-+
 *      IDENT(tmp, v2)        :S(_Lcase_0002)   <-+ dispatch chain — built
 *                            :(_Ldefault_0003) <-+ in head->dispatch_*, spliced
 *                                                   in finalize after after_tmp_assign
 *      _Lcase_0001  A      :(_Lend_0004)       <- A's body, then implicit break
 *      _Lcase_0002  B      :(_Lend_0004)       <- B's body, then implicit break
 *      _Ldefault_0003 D                          <- D's body — NO trailing goto
 *                                                   (last case naturally falls
 *                                                   through to _Lend below)
 *      _Lend_0004                                <- appended at finalize
 *
 * Each case's body falls through to _Lend implicitly at the start of the next
 * case label (or at finalize for the last one) — modern no-fall-through
 * semantics per Q6.  An empty body between two case labels (`case 1: case 2:
 * stmts;`) suppresses the implicit break: detected via last_case_label_tail
 * being the same as code->tail when the next case head fires (no body stmts
 * appended in between).  Both labels point at the same body via SNOBOL4
 * label-chaining — the grammar emits two label pads back-to-back; the second
 * is the resolved jump target for `IDENT(tmp, 1)`, the first is the resolved
 * target for `IDENT(tmp, 2)`, both label pads chain forward to the actual body.
 *
 * `default:` is a special case-head that allocates `_Ldefault_NNNN` instead
 * of `_Lcase_NNNN`, doesn't emit a dispatch IDENT() entry (the dispatch chain
 * just appends `:(_Ldefault_NNNN)` as the catch-all), and sets has_default=1.
 * Multiple `default:` clauses in one switch is a parse error.  If no default
 * appears, the dispatch's catch-all goto targets _Lend directly.
 *
 * `break;` inside a switch body resolves to the switch's end_label via the
 * existing LoopFrame stack — sc_switch_head_new pushes a frame with is_loop=0
 * (forward-compat groundwork from LS-4.i.2), so sc_append_break finds it but
 * sc_append_continue skips it (continue is a parse error in switch).
 */
struct CaseEntry {
    char   *case_label;    /* "_Lcase_NNNN" or "_Ldefault_NNNN" — owned */
    AST_t *value;         /* case value expression (still owned); NULL = default */
};
struct SwitchHead {
    AST_t *disc;                  /* discriminant expression (consumed by tmp=disc) */
    char   *tmp_name;              /* "_Lswitch_t_NNNN" — synthetic var name */
    char   *end_label;             /* "_Lend_NNNN" — break target */
    char   *default_label;         /* "_Ldefault_NNNN" or strdup(end_label) if no default */
    int     has_default;
    STMT_t *after_tmp_assign;      /* splice anchor for dispatch list */
    struct CaseEntry *cases;       /* dynamic array */
    int     cases_count;
    int     cases_cap;
    /* For implicit-break suppression on stacked case labels (`case 1: case 2:`). */
    STMT_t *last_case_label_tail;
    /* Saved outer-switch pointer for nested switches. */
    struct SwitchHead *prev_switch;
    int     lineno;
};

static char    *sc_label_new          (ScParseState *st, const char *prefix);
static struct IfHead    *sc_if_head_new    (ScParseState *st, AST_t *cond);
static struct WhileHead *sc_while_head_new (ScParseState *st, AST_t *cond);
static struct DoHead    *sc_do_head_new    (ScParseState *st);
static struct ForHead   *sc_for_head_new   (ScParseState *st, AST_t *cond, AST_t *step);
/* LS-4.h */
static void     sc_append_return      (ScParseState *st, AST_t *retval);
static void     sc_append_freturn     (ScParseState *st);
static void     sc_append_nreturn     (ScParseState *st);
/* LS-4.h — forward decls for stmt-builder helpers used inside sc_func_head_new
 * (the helpers themselves are defined later in the epilogue). */
static STMT_t  *sc_make_label_stmt    (ScParseState *st, char *label);
static STMT_t  *sc_make_goto_uncond_stmt(ScParseState *st, char *target);
static void     sc_splice_after       (ScParseState *st, STMT_t *anchor, STMT_t *chain_head, STMT_t *chain_tail);
static void     sc_append_chain       (ScParseState *st, STMT_t *chain_head, STMT_t *chain_tail);
static void     sc_finalize_if_no_else(ScParseState *st, struct IfHead *h);
static void     sc_finalize_if_else   (ScParseState *st, struct IfHead *h, STMT_t *before_else);
static void     sc_finalize_while     (ScParseState *st, struct WhileHead *h);
static void     sc_finalize_do_while  (ScParseState *st, struct DoHead *h, AST_t *cond);
static void     sc_finalize_for       (ScParseState *st, struct ForHead *h);
static struct FuncHead *sc_func_head_new(ScParseState *st, char *name, char *argstr);
static void     sc_finalize_function  (ScParseState *st, struct FuncHead *h);
/* LS-4.i.1 — goto / label */
static void     sc_emit_label_pad     (ScParseState *st, char *label);
static void     sc_append_goto_label  (ScParseState *st, char *target);
/* LS-4.i.2 — break / continue */
static void     sc_pending_label_add   (ScParseState *st, const char *name);
static void     sc_pending_label_clear (ScParseState *st);
static void     sc_pending_to_stash    (ScParseState *st);     /* for_lead */
static void     sc_loop_push           (ScParseState *st, char *cont_label, char *end_label, int is_loop, int from_stash);
static void     sc_loop_pop            (ScParseState *st);
static LoopFrame *sc_loop_find_by_user_label(ScParseState *st, const char *name, int want_loop);
static void     sc_append_break        (ScParseState *st, char *user_label /* NULL = innermost */);
static void     sc_append_continue     (ScParseState *st, char *user_label /* NULL = innermost loop */);
/* LS-4.i.3 — switch / case / default */
static struct SwitchHead *sc_switch_head_new(ScParseState *st, AST_t *disc);
static void     sc_switch_case_label   (ScParseState *st, AST_t *value);
static void     sc_switch_default_label(ScParseState *st);
static void     sc_finalize_switch     (ScParseState *st, struct SwitchHead *h);
/* LS-4.i.5 — struct NAME { f1, f2, ... } — Andrew's .sc line 162 record-decl.
 * Lowers to a single SPITBOL DATA('NAME(f1,f2,...)') bare-expression statement.
 * SPITBOL's DATA primitive defines the constructor + per-field accessors in
 * one call; struct is sugar for that.  Empty `struct NAME { }` lowers to
 * DATA('NAME()') — legal SPITBOL (a zero-field record). */
static void     sc_emit_struct         (ScParseState *st, char *name, char *fields);

#line 462 "snocone_parse.tab.c"

#ifdef short
# undef short
#endif

/* On compilers that do not define __PTRDIFF_MAX__ etc., make sure
   <limits.h> and (if available) <stdint.h> are included
   so that the code can choose integer types of a good width.  */

#ifndef __PTRDIFF_MAX__
# include <limits.h> /* INFRINGES ON USER NAME SPACE */
# if defined __STDC_VERSION__ && 199901 <= __STDC_VERSION__
#  include <stdint.h> /* INFRINGES ON USER NAME SPACE */
#  define YY_STDINT_H
# endif
#endif

/* Narrow types that promote to a signed type and that can represent a
   signed or unsigned integer of at least N bits.  In tables they can
   save space and decrease cache pressure.  Promoting to a signed type
   helps avoid bugs in integer arithmetic.  */

#ifdef __INT_LEAST8_MAX__
typedef __INT_LEAST8_TYPE__ yytype_int8;
#elif defined YY_STDINT_H
typedef int_least8_t yytype_int8;
#else
typedef signed char yytype_int8;
#endif

#ifdef __INT_LEAST16_MAX__
typedef __INT_LEAST16_TYPE__ yytype_int16;
#elif defined YY_STDINT_H
typedef int_least16_t yytype_int16;
#else
typedef short yytype_int16;
#endif

/* Work around bug in HP-UX 11.23, which defines these macros
   incorrectly for preprocessor constants.  This workaround can likely
   be removed in 2023, as HPE has promised support for HP-UX 11.23
   (aka HP-UX 11i v2) only through the end of 2022; see Table 2 of
   <https://h20195.www2.hpe.com/V2/getpdf.aspx/4AA4-7673ENW.pdf>.  */
#ifdef __hpux
# undef UINT_LEAST8_MAX
# undef UINT_LEAST16_MAX
# define UINT_LEAST8_MAX 255
# define UINT_LEAST16_MAX 65535
#endif

#if defined __UINT_LEAST8_MAX__ && __UINT_LEAST8_MAX__ <= __INT_MAX__
typedef __UINT_LEAST8_TYPE__ yytype_uint8;
#elif (!defined __UINT_LEAST8_MAX__ && defined YY_STDINT_H \
       && UINT_LEAST8_MAX <= INT_MAX)
typedef uint_least8_t yytype_uint8;
#elif !defined __UINT_LEAST8_MAX__ && UCHAR_MAX <= INT_MAX
typedef unsigned char yytype_uint8;
#else
typedef short yytype_uint8;
#endif

#if defined __UINT_LEAST16_MAX__ && __UINT_LEAST16_MAX__ <= __INT_MAX__
typedef __UINT_LEAST16_TYPE__ yytype_uint16;
#elif (!defined __UINT_LEAST16_MAX__ && defined YY_STDINT_H \
       && UINT_LEAST16_MAX <= INT_MAX)
typedef uint_least16_t yytype_uint16;
#elif !defined __UINT_LEAST16_MAX__ && USHRT_MAX <= INT_MAX
typedef unsigned short yytype_uint16;
#else
typedef int yytype_uint16;
#endif

#ifndef YYPTRDIFF_T
# if defined __PTRDIFF_TYPE__ && defined __PTRDIFF_MAX__
#  define YYPTRDIFF_T __PTRDIFF_TYPE__
#  define YYPTRDIFF_MAXIMUM __PTRDIFF_MAX__
# elif defined PTRDIFF_MAX
#  ifndef ptrdiff_t
#   include <stddef.h> /* INFRINGES ON USER NAME SPACE */
#  endif
#  define YYPTRDIFF_T ptrdiff_t
#  define YYPTRDIFF_MAXIMUM PTRDIFF_MAX
# else
#  define YYPTRDIFF_T long
#  define YYPTRDIFF_MAXIMUM LONG_MAX
# endif
#endif

#ifndef YYSIZE_T
# ifdef __SIZE_TYPE__
#  define YYSIZE_T __SIZE_TYPE__
# elif defined size_t
#  define YYSIZE_T size_t
# elif defined __STDC_VERSION__ && 199901 <= __STDC_VERSION__
#  include <stddef.h> /* INFRINGES ON USER NAME SPACE */
#  define YYSIZE_T size_t
# else
#  define YYSIZE_T unsigned
# endif
#endif

#define YYSIZE_MAXIMUM                                  \
  YY_CAST (YYPTRDIFF_T,                                 \
           (YYPTRDIFF_MAXIMUM < YY_CAST (YYSIZE_T, -1)  \
            ? YYPTRDIFF_MAXIMUM                         \
            : YY_CAST (YYSIZE_T, -1)))

#define YYSIZEOF(X) YY_CAST (YYPTRDIFF_T, sizeof (X))


/* Stored state numbers (used for stacks). */
typedef yytype_uint8 yy_state_t;

/* State numbers in computations.  */
typedef int yy_state_fast_t;

#ifndef YY_
# if defined YYENABLE_NLS && YYENABLE_NLS
#  if ENABLE_NLS
#   include <libintl.h> /* INFRINGES ON USER NAME SPACE */
#   define YY_(Msgid) dgettext ("bison-runtime", Msgid)
#  endif
# endif
# ifndef YY_
#  define YY_(Msgid) Msgid
# endif
#endif


#ifndef YY_ATTRIBUTE_PURE
# if defined __GNUC__ && 2 < __GNUC__ + (96 <= __GNUC_MINOR__)
#  define YY_ATTRIBUTE_PURE __attribute__ ((__pure__))
# else
#  define YY_ATTRIBUTE_PURE
# endif
#endif

#ifndef YY_ATTRIBUTE_UNUSED
# if defined __GNUC__ && 2 < __GNUC__ + (7 <= __GNUC_MINOR__)
#  define YY_ATTRIBUTE_UNUSED __attribute__ ((__unused__))
# else
#  define YY_ATTRIBUTE_UNUSED
# endif
#endif

/* Suppress unused-variable warnings by "using" E.  */
#if ! defined lint || defined __GNUC__
# define YY_USE(E) ((void) (E))
#else
# define YY_USE(E) /* empty */
#endif

/* Suppress an incorrect diagnostic about yylval being uninitialized.  */
#if defined __GNUC__ && ! defined __ICC && 406 <= __GNUC__ * 100 + __GNUC_MINOR__
# if __GNUC__ * 100 + __GNUC_MINOR__ < 407
#  define YY_IGNORE_MAYBE_UNINITIALIZED_BEGIN                           \
    _Pragma ("GCC diagnostic push")                                     \
    _Pragma ("GCC diagnostic ignored \"-Wuninitialized\"")
# else
#  define YY_IGNORE_MAYBE_UNINITIALIZED_BEGIN                           \
    _Pragma ("GCC diagnostic push")                                     \
    _Pragma ("GCC diagnostic ignored \"-Wuninitialized\"")              \
    _Pragma ("GCC diagnostic ignored \"-Wmaybe-uninitialized\"")
# endif
# define YY_IGNORE_MAYBE_UNINITIALIZED_END      \
    _Pragma ("GCC diagnostic pop")
#else
# define YY_INITIAL_VALUE(Value) Value
#endif
#ifndef YY_IGNORE_MAYBE_UNINITIALIZED_BEGIN
# define YY_IGNORE_MAYBE_UNINITIALIZED_BEGIN
# define YY_IGNORE_MAYBE_UNINITIALIZED_END
#endif
#ifndef YY_INITIAL_VALUE
# define YY_INITIAL_VALUE(Value) /* Nothing. */
#endif

#if defined __cplusplus && defined __GNUC__ && ! defined __ICC && 6 <= __GNUC__
# define YY_IGNORE_USELESS_CAST_BEGIN                          \
    _Pragma ("GCC diagnostic push")                            \
    _Pragma ("GCC diagnostic ignored \"-Wuseless-cast\"")
# define YY_IGNORE_USELESS_CAST_END            \
    _Pragma ("GCC diagnostic pop")
#endif
#ifndef YY_IGNORE_USELESS_CAST_BEGIN
# define YY_IGNORE_USELESS_CAST_BEGIN
# define YY_IGNORE_USELESS_CAST_END
#endif


#define YY_ASSERT(E) ((void) (0 && (E)))

#if !defined yyoverflow

/* The parser invokes alloca or malloc; define the necessary symbols.  */

# ifdef YYSTACK_USE_ALLOCA
#  if YYSTACK_USE_ALLOCA
#   ifdef __GNUC__
#    define YYSTACK_ALLOC __builtin_alloca
#   elif defined __BUILTIN_VA_ARG_INCR
#    include <alloca.h> /* INFRINGES ON USER NAME SPACE */
#   elif defined _AIX
#    define YYSTACK_ALLOC __alloca
#   elif defined _MSC_VER
#    include <malloc.h> /* INFRINGES ON USER NAME SPACE */
#    define alloca _alloca
#   else
#    define YYSTACK_ALLOC alloca
#    if ! defined _ALLOCA_H && ! defined EXIT_SUCCESS
#     include <stdlib.h> /* INFRINGES ON USER NAME SPACE */
      /* Use EXIT_SUCCESS as a witness for stdlib.h.  */
#     ifndef EXIT_SUCCESS
#      define EXIT_SUCCESS 0
#     endif
#    endif
#   endif
#  endif
# endif

# ifdef YYSTACK_ALLOC
   /* Pacify GCC's 'empty if-body' warning.  */
#  define YYSTACK_FREE(Ptr) do { /* empty */; } while (0)
#  ifndef YYSTACK_ALLOC_MAXIMUM
    /* The OS might guarantee only one guard page at the bottom of the stack,
       and a page size can be as small as 4096 bytes.  So we cannot safely
       invoke alloca (N) if N exceeds 4096.  Use a slightly smaller number
       to allow for a few compiler-allocated temporary stack slots.  */
#   define YYSTACK_ALLOC_MAXIMUM 4032 /* reasonable circa 2006 */
#  endif
# else
#  define YYSTACK_ALLOC YYMALLOC
#  define YYSTACK_FREE YYFREE
#  ifndef YYSTACK_ALLOC_MAXIMUM
#   define YYSTACK_ALLOC_MAXIMUM YYSIZE_MAXIMUM
#  endif
#  if (defined __cplusplus && ! defined EXIT_SUCCESS \
       && ! ((defined YYMALLOC || defined malloc) \
             && (defined YYFREE || defined free)))
#   include <stdlib.h> /* INFRINGES ON USER NAME SPACE */
#   ifndef EXIT_SUCCESS
#    define EXIT_SUCCESS 0
#   endif
#  endif
#  ifndef YYMALLOC
#   define YYMALLOC malloc
#   if ! defined malloc && ! defined EXIT_SUCCESS
void *malloc (YYSIZE_T); /* INFRINGES ON USER NAME SPACE */
#   endif
#  endif
#  ifndef YYFREE
#   define YYFREE free
#   if ! defined free && ! defined EXIT_SUCCESS
void free (void *); /* INFRINGES ON USER NAME SPACE */
#   endif
#  endif
# endif
#endif /* !defined yyoverflow */

#if (! defined yyoverflow \
     && (! defined __cplusplus \
         || (defined SC_STYPE_IS_TRIVIAL && SC_STYPE_IS_TRIVIAL)))

/* A type that is properly aligned for any stack member.  */
union yyalloc
{
  yy_state_t yyss_alloc;
  YYSTYPE yyvs_alloc;
};

/* The size of the maximum gap between one aligned stack and the next.  */
# define YYSTACK_GAP_MAXIMUM (YYSIZEOF (union yyalloc) - 1)

/* The size of an array large to enough to hold all stacks, each with
   N elements.  */
# define YYSTACK_BYTES(N) \
     ((N) * (YYSIZEOF (yy_state_t) + YYSIZEOF (YYSTYPE)) \
      + YYSTACK_GAP_MAXIMUM)

# define YYCOPY_NEEDED 1

/* Relocate STACK from its old location to the new one.  The
   local variables YYSIZE and YYSTACKSIZE give the old and new number of
   elements in the stack, and YYPTR gives the new location of the
   stack.  Advance YYPTR to a properly aligned location for the next
   stack.  */
# define YYSTACK_RELOCATE(Stack_alloc, Stack)                           \
    do                                                                  \
      {                                                                 \
        YYPTRDIFF_T yynewbytes;                                         \
        YYCOPY (&yyptr->Stack_alloc, Stack, yysize);                    \
        Stack = &yyptr->Stack_alloc;                                    \
        yynewbytes = yystacksize * YYSIZEOF (*Stack) + YYSTACK_GAP_MAXIMUM; \
        yyptr += yynewbytes / YYSIZEOF (*yyptr);                        \
      }                                                                 \
    while (0)

#endif

#if defined YYCOPY_NEEDED && YYCOPY_NEEDED
/* Copy COUNT objects from SRC to DST.  The source and destination do
   not overlap.  */
# ifndef YYCOPY
#  if defined __GNUC__ && 1 < __GNUC__
#   define YYCOPY(Dst, Src, Count) \
      __builtin_memcpy (Dst, Src, YY_CAST (YYSIZE_T, (Count)) * sizeof (*(Src)))
#  else
#   define YYCOPY(Dst, Src, Count)              \
      do                                        \
        {                                       \
          YYPTRDIFF_T yyi;                      \
          for (yyi = 0; yyi < (Count); yyi++)   \
            (Dst)[yyi] = (Src)[yyi];            \
        }                                       \
      while (0)
#  endif
# endif
#endif /* !YYCOPY_NEEDED */

/* YYFINAL -- State number of the termination state.  */
#define YYFINAL  102
/* YYLAST -- Last index in YYTABLE.  */
#define YYLAST   863

/* YYNTOKENS -- Number of terminals.  */
#define YYNTOKENS  85
/* YYNNTS -- Number of nonterminals.  */
#define YYNNTS  38
/* YYNRULES -- Number of rules.  */
#define YYNRULES  134
/* YYNSTATES -- Number of states.  */
#define YYNSTATES  255

/* YYMAXUTOK -- Last valid token kind.  */
#define YYMAXUTOK   339


/* YYTRANSLATE(TOKEN-NUM) -- Symbol number corresponding to TOKEN-NUM
   as returned by yylex, with out-of-bounds checking.  */
#define YYTRANSLATE(YYX)                                \
  (0 <= (YYX) && (YYX) <= YYMAXUTOK                     \
   ? YY_CAST (yysymbol_kind_t, yytranslate[YYX])        \
   : YYSYMBOL_YYUNDEF)

/* YYTRANSLATE[TOKEN-NUM] -- Symbol number corresponding to TOKEN-NUM
   as returned by yylex.  */
static const yytype_int8 yytranslate[] =
{
       0,     2,     2,     2,     2,     2,     2,     2,     2,     2,
       2,     2,     2,     2,     2,     2,     2,     2,     2,     2,
       2,     2,     2,     2,     2,     2,     2,     2,     2,     2,
       2,     2,     2,     2,     2,     2,     2,     2,     2,     2,
       2,     2,     2,     2,     2,     2,     2,     2,     2,     2,
       2,     2,     2,     2,     2,     2,     2,     2,     2,     2,
       2,     2,     2,     2,     2,     2,     2,     2,     2,     2,
       2,     2,     2,     2,     2,     2,     2,     2,     2,     2,
       2,     2,     2,     2,     2,     2,     2,     2,     2,     2,
       2,     2,     2,     2,     2,     2,     2,     2,     2,     2,
       2,     2,     2,     2,     2,     2,     2,     2,     2,     2,
       2,     2,     2,     2,     2,     2,     2,     2,     2,     2,
       2,     2,     2,     2,     2,     2,     2,     2,     2,     2,
       2,     2,     2,     2,     2,     2,     2,     2,     2,     2,
       2,     2,     2,     2,     2,     2,     2,     2,     2,     2,
       2,     2,     2,     2,     2,     2,     2,     2,     2,     2,
       2,     2,     2,     2,     2,     2,     2,     2,     2,     2,
       2,     2,     2,     2,     2,     2,     2,     2,     2,     2,
       2,     2,     2,     2,     2,     2,     2,     2,     2,     2,
       2,     2,     2,     2,     2,     2,     2,     2,     2,     2,
       2,     2,     2,     2,     2,     2,     2,     2,     2,     2,
       2,     2,     2,     2,     2,     2,     2,     2,     2,     2,
       2,     2,     2,     2,     2,     2,     2,     2,     2,     2,
       2,     2,     2,     2,     2,     2,     2,     2,     2,     2,
       2,     2,     2,     2,     2,     2,     2,     2,     2,     2,
       2,     2,     2,     2,     2,     2,     1,     2,     3,     4,
       5,     6,     7,     8,     9,    10,    11,    12,    13,    14,
      15,    16,    17,    18,    19,    20,    21,    22,    23,    24,
      25,    26,    27,    28,    29,    30,    31,    32,    33,    34,
      35,    36,    37,    38,    39,    40,    41,    42,    43,    44,
      45,    46,    47,    48,    49,    50,    51,    52,    53,    54,
      55,    56,    57,    58,    59,    60,    61,    62,    63,    64,
      65,    66,    67,    68,    69,    70,    71,    72,    73,    74,
      75,    76,    77,    78,    79,    80,    81,    82,    83,    84
};

#if SC_DEBUG
/* YYRLINE[YYN] -- Source line where rule number YYN was defined.  */
static const yytype_int16 yyrline[] =
{
       0,   489,   489,   490,   493,   494,   520,   521,   525,   526,
     527,   529,   536,   539,   542,   544,   551,   553,   561,   563,
     566,   570,   572,   574,   577,   580,   583,   587,   594,   603,
     604,   620,   623,   640,   648,   649,   659,   660,   664,   665,
     676,   677,   701,   711,   712,   713,   718,   722,   735,   737,
     749,   767,   770,   771,   773,   774,   775,   776,   778,   780,
     781,   782,   783,   786,   787,   830,   832,   844,   848,   852,
     856,   860,   864,   877,   879,   893,   898,   913,   918,   932,
     935,   938,   941,   944,   947,   950,   953,   956,   959,   962,
     965,   968,   971,   974,   978,   980,   982,   986,   988,   990,
     998,  1000,  1033,  1035,  1037,  1065,  1072,  1083,  1086,  1089,
    1091,  1105,  1112,  1116,  1120,  1122,  1124,  1126,  1147,  1157,
    1159,  1161,  1165,  1167,  1169,  1171,  1173,  1175,  1177,  1180,
    1182,  1184,  1186,  1188,  1190
};
#endif

/** Accessing symbol of state STATE.  */
#define YY_ACCESSING_SYMBOL(State) YY_CAST (yysymbol_kind_t, yystos[State])

#if SC_DEBUG || 0
/* The user-facing name of the symbol whose (internal) number is
   YYSYMBOL.  No bounds checking.  */
static const char *yysymbol_name (yysymbol_kind_t yysymbol) YY_ATTRIBUTE_UNUSED;

/* YYTNAME[SYMBOL-NUM] -- String name of the symbol SYMBOL-NUM.
   First, the terminals, then, starting at YYNTOKENS, nonterminals.  */
static const char *const yytname[] =
{
  "\"end of file\"", "error", "\"invalid token\"", "T_IDENT", "T_KEYWORD",
  "T_INT", "T_REAL", "T_STR", "T_CALL", "T_2PLUS", "T_2MINUS", "T_2STAR",
  "T_2SLASH", "T_2CARET", "T_EQ", "T_NE", "T_LT", "T_GT", "T_LE", "T_GE",
  "T_LEQ", "T_LNE", "T_LLT", "T_LGT", "T_LLE", "T_LGE", "T_IDENT_OP",
  "T_DIFFER", "T_1PLUS", "T_1MINUS", "T_2EQUAL", "T_PLUS_ASSIGN",
  "T_MINUS_ASSIGN", "T_STAR_ASSIGN", "T_SLASH_ASSIGN", "T_CARET_ASSIGN",
  "T_2QUEST", "T_2PIPE", "T_CONCAT", "T_LPAREN", "T_RPAREN", "T_SEMICOLON",
  "T_COMMA", "T_LBRACK", "T_RBRACK", "T_2DOLLAR", "T_2DOT", "T_2AMP",
  "T_2AT", "T_2POUND", "T_2PERCENT", "T_2TILDE", "T_1STAR", "T_1SLASH",
  "T_1PERCENT", "T_1AT", "T_1TILDE", "T_1DOLLAR", "T_1DOT", "T_1POUND",
  "T_1PIPE", "T_1EQUAL", "T_1QUEST", "T_1AMP", "T_1BANG", "T_COLON",
  "T_DO", "T_FOR", "T_SWITCH", "T_CASE", "T_DEFAULT", "T_BREAK",
  "T_CONTINUE", "T_GOTO", "T_DEFINE", "T_RETURN", "T_FRETURN", "T_NRETURN",
  "T_STRUCT", "T_UNKNOWN", "T_LBRACE", "T_RBRACE", "T_IF", "T_ELSE",
  "T_WHILE", "$accept", "program", "stmt_list", "stmt", "matched_stmt",
  "unmatched_stmt", "if_head", "while_head", "do_head", "do_body",
  "for_lead", "for_head", "switch_head", "switch_body", "case_clause",
  "case_or_default_label", "opt_head_sep", "func_head", "func_arglist",
  "func_arglist_ne", "struct_field_list", "else_keyword", "label_decl",
  "simple_stmt", "block_stmt", "expr0", "expr1", "expr3", "expr4", "expr5",
  "expr6", "expr9", "expr11", "expr12", "expr15", "exprlist",
  "exprlist_ne", "expr17", YY_NULLPTR
};

static const char *
yysymbol_name (yysymbol_kind_t yysymbol)
{
  return yytname[yysymbol];
}
#endif

#define YYPACT_NINF (-200)

#define yypact_value_is_default(Yyn) \
  ((Yyn) == YYPACT_NINF)

#define YYTABLE_NINF (-1)

#define yytable_value_is_error(Yyn) \
  0

/* YYPACT[STATE-NUM] -- Index in YYTABLE of the portion describing
   STATE-NUM.  */
static const yytype_int16 yypact[] =
{
     640,   -48,  -200,  -200,  -200,  -200,   799,   799,   799,   722,
    -200,   799,   799,   799,   799,   799,   799,   799,   799,   799,
     799,   799,   799,   799,  -200,  -200,   -18,     8,    11,    21,
      39,   760,     7,    38,    84,   148,    49,    50,    90,   640,
    -200,  -200,  -200,   640,   640,    12,    52,   640,    17,    18,
     640,  -200,  -200,    55,    32,    23,    61,   307,    63,    66,
    -200,     5,    57,  -200,  -200,  -200,  -200,    53,    59,  -200,
    -200,  -200,    16,  -200,  -200,  -200,  -200,  -200,  -200,  -200,
    -200,  -200,  -200,  -200,  -200,  -200,   799,    62,  -200,    64,
    -200,    65,    68,  -200,    67,  -200,  -200,    22,  -200,   230,
     799,   799,  -200,  -200,  -200,    26,  -200,  -200,   312,    27,
     799,  -200,  -200,   -26,   394,  -200,  -200,  -200,   799,   799,
     799,   799,   799,   799,   799,   799,   799,   799,   799,   799,
     799,   799,   799,   799,   799,   799,   799,   799,   799,   799,
     799,   799,   799,   799,   799,   799,   799,   799,   799,  -200,
     799,  -200,   799,    70,  -200,  -200,  -200,    13,  -200,     1,
    -200,    78,    79,  -200,   640,  -200,   476,    81,    80,   799,
      58,  -200,   -24,   640,  -200,  -200,   558,  -200,  -200,  -200,
    -200,  -200,  -200,  -200,    61,   307,    63,    63,    63,    63,
      63,    63,    63,    63,    63,    63,    63,    63,    63,    63,
      66,    66,  -200,  -200,  -200,    57,    57,    82,  -200,    28,
    -200,    29,  -200,    86,    34,  -200,  -200,   -34,    86,    86,
    -200,  -200,  -200,   799,   799,    60,  -200,  -200,   640,  -200,
    -200,  -200,  -200,  -200,   119,  -200,  -200,  -200,   127,   128,
    -200,  -200,  -200,    92,    93,  -200,  -200,  -200,  -200,    95,
     799,  -200,    97,    86,  -200
};

/* YYDEFACT[STATE-NUM] -- Default reduction number in state STATE-NUM.
   Performed when YYTABLE does not specify something else to do.  Zero
   means the default is an error.  */
static const yytype_uint8 yydefact[] =
{
       3,   112,   113,   114,   115,   116,   108,     0,     0,     0,
      53,     0,     0,     0,     0,     0,     0,     0,     0,     0,
       0,     0,     0,     0,    28,    31,     0,     0,     0,     0,
       0,     0,     0,     0,     0,     0,     0,     0,     0,     2,
       5,     6,     7,     0,     0,     0,     0,     0,     0,     0,
       0,     8,     9,     0,    72,    74,    76,    78,    93,    96,
      99,   101,   104,   106,    51,   112,   110,     0,   107,   120,
     121,   119,     0,   122,   130,   129,   125,   126,   124,   123,
     131,   132,   133,   127,   128,   134,     0,     0,    59,     0,
      61,     0,     0,    55,     0,    56,    57,     0,    64,     0,
       0,     0,     1,     4,    21,     6,    11,    23,     0,     0,
       0,    13,    24,     0,     0,    20,    25,    52,    66,     0,
       0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
       0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
       0,     0,     0,     0,     0,     0,     0,     0,   108,   111,
       0,   117,     0,     0,    60,    62,    58,     0,    54,     0,
      63,     0,     0,    50,     0,    30,     0,     0,     0,     0,
       0,    17,     0,    34,    36,    15,     0,    65,    67,    68,
      69,    70,    71,    73,    75,    77,    79,    80,    81,    82,
      83,    84,    85,    86,    87,    88,    89,    90,    91,    92,
      94,    95,    97,    98,   100,   102,   103,     0,   109,     0,
      33,     0,    43,    40,     0,    48,    19,     0,    40,    40,
      10,    22,    29,     0,     0,     0,    39,    16,    35,    37,
      14,   105,   118,    44,     0,    41,    42,    45,     0,     0,
      18,    26,    27,     0,     0,    38,    46,    47,    49,     0,
       0,    12,     0,    40,    32
};

/* YYPGOTO[NTERM-NUM].  */
static const yytype_int16 yypgoto[] =
{
    -200,  -200,   -33,   -38,   -37,   -35,  -200,  -200,  -200,  -200,
    -200,  -200,  -200,  -200,   -39,  -200,  -199,  -200,  -200,  -200,
    -200,  -200,  -200,  -200,  -200,    -6,    19,  -200,    14,    24,
     276,   -58,  -104,  -200,   -61,    -8,   -11,    15
};

/* YYDEFGOTO[NTERM-NUM].  */
static const yytype_uint8 yydefgoto[] =
{
       0,    38,    39,    40,    41,    42,    43,    44,    45,   109,
      46,    47,    48,   172,   173,   174,   236,    49,   213,   214,
     217,   164,    50,    51,    52,    53,    54,    55,    56,    57,
      58,    59,    60,    61,    62,    67,    68,    63
};

/* YYTABLE[YYPACT[STATE-NUM]] -- What to do in state STATE-NUM.  If
   positive, shift that token.  If negative, reduce the rule whose
   number is the opposite.  If YYTABLE_NINF, syntax error.  */
static const yytype_uint8 yytable[] =
{
      66,   103,    99,    72,   215,   104,   105,   106,   239,   107,
     111,    87,   112,   115,    89,   116,   211,    64,   145,   241,
     242,    86,    69,    70,    91,    94,    73,    74,    75,    76,
      77,    78,    79,    80,    81,    82,    83,    84,    85,   202,
     203,   204,    92,   169,   170,   169,   170,   240,    95,    88,
     146,   147,    90,   212,   254,   171,   151,   227,   152,   124,
     125,   103,   118,   119,   120,   121,   122,   123,   232,   233,
     150,   234,   141,   142,   237,   166,   238,   143,   144,    96,
     153,   176,   216,   200,   201,   205,   206,    97,   100,   101,
     102,   110,   108,   149,   161,   162,   117,   113,   114,   126,
     148,   150,   159,   154,   168,   155,   156,   157,   158,   163,
     210,   167,   177,   178,   179,   180,   181,   182,   218,   219,
     223,   224,   246,   226,   235,   245,   231,   220,   103,   221,
     247,   248,   249,   228,   250,   229,   251,   253,   103,   184,
     207,   209,    66,   183,   208,     0,    66,     0,     0,     0,
     185,     1,     2,     3,     4,     5,     6,     0,     0,     0,
       0,     0,     0,   225,     0,     0,     0,     0,     0,     0,
       0,     0,     0,     0,     0,     0,     7,     8,     0,     0,
       0,     0,     0,     0,     0,     0,     0,     9,     0,    10,
     229,     0,     0,     0,     0,     0,     0,     0,     0,     0,
      11,    12,    13,    14,    15,    16,    17,    18,    19,    20,
      21,    22,    23,     0,    24,    25,    26,   243,   244,    27,
      28,    29,    30,    31,    32,    33,    34,     0,    35,    98,
      36,     0,    37,     1,     2,     3,     4,     5,     6,     0,
       0,     0,     0,     0,   252,     0,     0,     0,     0,     0,
       0,     0,     0,     0,     0,     0,     0,     0,     7,     8,
       0,     0,     0,     0,     0,     0,     0,     0,     0,     9,
       0,    10,     0,     0,     0,     0,     0,     0,     0,     0,
       0,     0,    11,    12,    13,    14,    15,    16,    17,    18,
      19,    20,    21,    22,    23,     0,    24,    25,    26,     0,
       0,    27,    28,    29,    30,    31,    32,    33,    34,     0,
      35,   160,    36,     0,    37,     1,     2,     3,     4,     5,
       6,   127,   128,   129,   130,   131,   132,   133,   134,   135,
     136,   137,   138,   139,   140,     0,     0,     0,     0,     0,
       7,     8,     0,     0,     0,     0,     0,     0,     0,     0,
       0,     9,     0,    10,     0,     0,     0,     0,     0,     0,
       0,     0,     0,     0,    11,    12,    13,    14,    15,    16,
      17,    18,    19,    20,    21,    22,    23,     0,    24,    25,
      26,     0,     0,    27,    28,    29,    30,    31,    32,    33,
      34,     0,    35,   165,    36,     0,    37,     1,     2,     3,
       4,     5,     6,   186,   187,   188,   189,   190,   191,   192,
     193,   194,   195,   196,   197,   198,   199,     0,     0,     0,
       0,     0,     7,     8,     0,     0,     0,     0,     0,     0,
       0,     0,     0,     9,     0,    10,     0,     0,     0,     0,
       0,     0,     0,     0,     0,     0,    11,    12,    13,    14,
      15,    16,    17,    18,    19,    20,    21,    22,    23,     0,
      24,    25,    26,     0,     0,    27,    28,    29,    30,    31,
      32,    33,    34,     0,    35,   175,    36,     0,    37,     1,
       2,     3,     4,     5,     6,     0,     0,     0,     0,     0,
       0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
       0,     0,     0,     0,     7,     8,     0,     0,     0,     0,
       0,     0,     0,     0,     0,     9,     0,    10,     0,     0,
       0,     0,     0,     0,     0,     0,     0,     0,    11,    12,
      13,    14,    15,    16,    17,    18,    19,    20,    21,    22,
      23,     0,    24,    25,    26,     0,     0,    27,    28,    29,
      30,    31,    32,    33,    34,     0,    35,   222,    36,     0,
      37,     1,     2,     3,     4,     5,     6,     0,     0,     0,
       0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
       0,     0,     0,     0,     0,     0,     7,     8,     0,     0,
       0,     0,     0,     0,     0,     0,     0,     9,     0,    10,
       0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
      11,    12,    13,    14,    15,    16,    17,    18,    19,    20,
      21,    22,    23,     0,    24,    25,    26,     0,     0,    27,
      28,    29,    30,    31,    32,    33,    34,     0,    35,   230,
      36,     0,    37,     1,     2,     3,     4,     5,     6,     0,
       0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
       0,     0,     0,     0,     0,     0,     0,     0,     7,     8,
       0,     0,     0,     0,     0,     0,     0,     0,     0,     9,
       0,    10,     0,     0,     0,     0,     0,     0,     0,     0,
       0,     0,    11,    12,    13,    14,    15,    16,    17,    18,
      19,    20,    21,    22,    23,     0,    24,    25,    26,     0,
       0,    27,    28,    29,    30,    31,    32,    33,    34,     0,
      35,     0,    36,     0,    37,    65,     2,     3,     4,     5,
       6,     0,     0,     0,     0,     0,     0,     0,     0,     0,
       0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
       7,     8,     0,     0,     0,     0,     0,     0,     0,     0,
       0,     9,    71,    65,     2,     3,     4,     5,     6,     0,
       0,     0,     0,     0,    11,    12,    13,    14,    15,    16,
      17,    18,    19,    20,    21,    22,    23,     0,     7,     8,
       0,     0,     0,     0,     0,     0,     0,     0,     0,     9,
       0,    93,    65,     2,     3,     4,     5,     6,     0,     0,
       0,     0,    11,    12,    13,    14,    15,    16,    17,    18,
      19,    20,    21,    22,    23,     0,     0,     7,     8,     0,
       0,     0,     0,     0,     0,     0,     0,     0,     9,     0,
       0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
       0,    11,    12,    13,    14,    15,    16,    17,    18,    19,
      20,    21,    22,    23
};

static const yytype_int16 yycheck[] =
{
       6,    39,    35,     9,     3,    43,    43,    44,    42,    44,
      47,     3,    47,    50,     3,    50,     3,    65,    13,   218,
     219,    39,     7,     8,     3,    31,    11,    12,    13,    14,
      15,    16,    17,    18,    19,    20,    21,    22,    23,   143,
     144,   145,     3,    69,    70,    69,    70,    81,    41,    41,
      45,    46,    41,    40,   253,    81,    40,    81,    42,    36,
      37,    99,    30,    31,    32,    33,    34,    35,    40,    40,
      42,    42,     9,    10,    40,   108,    42,    11,    12,    41,
      86,   114,    81,   141,   142,   146,   147,     3,    39,    39,
       0,    39,    80,    40,   100,   101,    41,    80,    80,    38,
      43,    42,    80,    41,   110,    41,    41,    39,    41,    83,
      40,    84,   118,   119,   120,   121,   122,   123,    40,    40,
      39,    41,     3,    65,    38,    65,    44,   164,   166,   164,
       3,     3,    40,   172,    41,   173,    41,    40,   176,   125,
     148,   152,   148,   124,   150,    -1,   152,    -1,    -1,    -1,
     126,     3,     4,     5,     6,     7,     8,    -1,    -1,    -1,
      -1,    -1,    -1,   169,    -1,    -1,    -1,    -1,    -1,    -1,
      -1,    -1,    -1,    -1,    -1,    -1,    28,    29,    -1,    -1,
      -1,    -1,    -1,    -1,    -1,    -1,    -1,    39,    -1,    41,
     228,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,
      52,    53,    54,    55,    56,    57,    58,    59,    60,    61,
      62,    63,    64,    -1,    66,    67,    68,   223,   224,    71,
      72,    73,    74,    75,    76,    77,    78,    -1,    80,    81,
      82,    -1,    84,     3,     4,     5,     6,     7,     8,    -1,
      -1,    -1,    -1,    -1,   250,    -1,    -1,    -1,    -1,    -1,
      -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    28,    29,
      -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    39,
      -1,    41,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,
      -1,    -1,    52,    53,    54,    55,    56,    57,    58,    59,
      60,    61,    62,    63,    64,    -1,    66,    67,    68,    -1,
      -1,    71,    72,    73,    74,    75,    76,    77,    78,    -1,
      80,    81,    82,    -1,    84,     3,     4,     5,     6,     7,
       8,    14,    15,    16,    17,    18,    19,    20,    21,    22,
      23,    24,    25,    26,    27,    -1,    -1,    -1,    -1,    -1,
      28,    29,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,
      -1,    39,    -1,    41,    -1,    -1,    -1,    -1,    -1,    -1,
      -1,    -1,    -1,    -1,    52,    53,    54,    55,    56,    57,
      58,    59,    60,    61,    62,    63,    64,    -1,    66,    67,
      68,    -1,    -1,    71,    72,    73,    74,    75,    76,    77,
      78,    -1,    80,    81,    82,    -1,    84,     3,     4,     5,
       6,     7,     8,   127,   128,   129,   130,   131,   132,   133,
     134,   135,   136,   137,   138,   139,   140,    -1,    -1,    -1,
      -1,    -1,    28,    29,    -1,    -1,    -1,    -1,    -1,    -1,
      -1,    -1,    -1,    39,    -1,    41,    -1,    -1,    -1,    -1,
      -1,    -1,    -1,    -1,    -1,    -1,    52,    53,    54,    55,
      56,    57,    58,    59,    60,    61,    62,    63,    64,    -1,
      66,    67,    68,    -1,    -1,    71,    72,    73,    74,    75,
      76,    77,    78,    -1,    80,    81,    82,    -1,    84,     3,
       4,     5,     6,     7,     8,    -1,    -1,    -1,    -1,    -1,
      -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,
      -1,    -1,    -1,    -1,    28,    29,    -1,    -1,    -1,    -1,
      -1,    -1,    -1,    -1,    -1,    39,    -1,    41,    -1,    -1,
      -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    52,    53,
      54,    55,    56,    57,    58,    59,    60,    61,    62,    63,
      64,    -1,    66,    67,    68,    -1,    -1,    71,    72,    73,
      74,    75,    76,    77,    78,    -1,    80,    81,    82,    -1,
      84,     3,     4,     5,     6,     7,     8,    -1,    -1,    -1,
      -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,
      -1,    -1,    -1,    -1,    -1,    -1,    28,    29,    -1,    -1,
      -1,    -1,    -1,    -1,    -1,    -1,    -1,    39,    -1,    41,
      -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,
      52,    53,    54,    55,    56,    57,    58,    59,    60,    61,
      62,    63,    64,    -1,    66,    67,    68,    -1,    -1,    71,
      72,    73,    74,    75,    76,    77,    78,    -1,    80,    81,
      82,    -1,    84,     3,     4,     5,     6,     7,     8,    -1,
      -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,
      -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    28,    29,
      -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    39,
      -1,    41,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,
      -1,    -1,    52,    53,    54,    55,    56,    57,    58,    59,
      60,    61,    62,    63,    64,    -1,    66,    67,    68,    -1,
      -1,    71,    72,    73,    74,    75,    76,    77,    78,    -1,
      80,    -1,    82,    -1,    84,     3,     4,     5,     6,     7,
       8,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,
      -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,
      28,    29,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,
      -1,    39,    40,     3,     4,     5,     6,     7,     8,    -1,
      -1,    -1,    -1,    -1,    52,    53,    54,    55,    56,    57,
      58,    59,    60,    61,    62,    63,    64,    -1,    28,    29,
      -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    39,
      -1,    41,     3,     4,     5,     6,     7,     8,    -1,    -1,
      -1,    -1,    52,    53,    54,    55,    56,    57,    58,    59,
      60,    61,    62,    63,    64,    -1,    -1,    28,    29,    -1,
      -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    39,    -1,
      -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,
      -1,    52,    53,    54,    55,    56,    57,    58,    59,    60,
      61,    62,    63,    64
};

/* YYSTOS[STATE-NUM] -- The symbol kind of the accessing symbol of
   state STATE-NUM.  */
static const yytype_int8 yystos[] =
{
       0,     3,     4,     5,     6,     7,     8,    28,    29,    39,
      41,    52,    53,    54,    55,    56,    57,    58,    59,    60,
      61,    62,    63,    64,    66,    67,    68,    71,    72,    73,
      74,    75,    76,    77,    78,    80,    82,    84,    86,    87,
      88,    89,    90,    91,    92,    93,    95,    96,    97,   102,
     107,   108,   109,   110,   111,   112,   113,   114,   115,   116,
     117,   118,   119,   122,    65,     3,   110,   120,   121,   122,
     122,    40,   110,   122,   122,   122,   122,   122,   122,   122,
     122,   122,   122,   122,   122,   122,    39,     3,    41,     3,
      41,     3,     3,    41,   110,    41,    41,     3,    81,    87,
      39,    39,     0,    88,    88,    89,    89,    90,    80,    94,
      39,    89,    90,    80,    80,    89,    90,    41,    30,    31,
      32,    33,    34,    35,    36,    37,    38,    14,    15,    16,
      17,    18,    19,    20,    21,    22,    23,    24,    25,    26,
      27,     9,    10,    11,    12,    13,    45,    46,    43,    40,
      42,    40,    42,   110,    41,    41,    41,    39,    41,    80,
      81,   110,   110,    83,   106,    81,    87,    84,   110,    69,
      70,    81,    98,    99,   100,    81,    87,   110,   110,   110,
     110,   110,   110,   111,   113,   114,   115,   115,   115,   115,
     115,   115,   115,   115,   115,   115,   115,   115,   115,   115,
     116,   116,   117,   117,   117,   119,   119,   120,   110,   121,
      40,     3,    40,   103,   104,     3,    81,   105,    40,    40,
      89,    90,    81,    39,    41,   110,    65,    81,    99,    88,
      81,    44,    40,    40,    42,    38,   101,    40,    42,    42,
      81,   101,   101,   110,   110,    65,     3,     3,     3,    40,
      41,    41,   110,    40,   101
};

/* YYR1[RULE-NUM] -- Symbol kind of the left-hand side of rule RULE-NUM.  */
static const yytype_int8 yyr1[] =
{
       0,    85,    86,    86,    87,    87,    88,    88,    89,    89,
      89,    89,    89,    89,    89,    89,    89,    89,    89,    89,
      89,    90,    90,    90,    90,    90,    91,    92,    93,    94,
      94,    95,    96,    97,    98,    98,    99,    99,   100,   100,
     101,   101,   102,   103,   103,   103,   104,   104,   105,   105,
     106,   107,   108,   108,   108,   108,   108,   108,   108,   108,
     108,   108,   108,   109,   109,   110,   110,   110,   110,   110,
     110,   110,   110,   111,   111,   112,   112,   113,   113,   114,
     114,   114,   114,   114,   114,   114,   114,   114,   114,   114,
     114,   114,   114,   114,   115,   115,   115,   116,   116,   116,
     117,   117,   118,   118,   118,   119,   119,   120,   120,   121,
     121,   122,   122,   122,   122,   122,   122,   122,   122,   122,
     122,   122,   122,   122,   122,   122,   122,   122,   122,   122,
     122,   122,   122,   122,   122
};

/* YYR2[RULE-NUM] -- Number of symbols on the right-hand side of rule RULE-NUM.  */
static const yytype_int8 yyr2[] =
{
       0,     2,     1,     0,     2,     1,     1,     1,     1,     1,
       4,     2,     7,     2,     4,     3,     4,     3,     5,     4,
       2,     2,     4,     2,     2,     2,     5,     5,     1,     3,
       2,     1,     9,     4,     1,     2,     1,     2,     3,     2,
       0,     1,     5,     1,     2,     2,     3,     3,     1,     3,
       1,     2,     2,     1,     3,     2,     2,     2,     3,     2,
       3,     2,     3,     3,     2,     3,     2,     3,     3,     3,
       3,     3,     1,     3,     1,     3,     1,     3,     1,     3,
       3,     3,     3,     3,     3,     3,     3,     3,     3,     3,
       3,     3,     3,     1,     3,     3,     1,     3,     3,     1,
       3,     1,     3,     3,     1,     4,     1,     1,     0,     3,
       1,     3,     1,     1,     1,     1,     1,     3,     5,     2,
       2,     2,     2,     2,     2,     2,     2,     2,     2,     2,
       2,     2,     2,     2,     2
};


enum { YYENOMEM = -2 };

#define yyerrok         (yyerrstatus = 0)
#define yyclearin       (yychar = SC_EMPTY)

#define YYACCEPT        goto yyacceptlab
#define YYABORT         goto yyabortlab
#define YYERROR         goto yyerrorlab
#define YYNOMEM         goto yyexhaustedlab


#define YYRECOVERING()  (!!yyerrstatus)

#define YYBACKUP(Token, Value)                                    \
  do                                                              \
    if (yychar == SC_EMPTY)                                        \
      {                                                           \
        yychar = (Token);                                         \
        yylval = (Value);                                         \
        YYPOPSTACK (yylen);                                       \
        yystate = *yyssp;                                         \
        goto yybackup;                                            \
      }                                                           \
    else                                                          \
      {                                                           \
        yyerror (st, YY_("syntax error: cannot back up")); \
        YYERROR;                                                  \
      }                                                           \
  while (0)

/* Backward compatibility with an undocumented macro.
   Use SC_error or SC_UNDEF. */
#define YYERRCODE SC_UNDEF


/* Enable debugging if requested.  */
#if SC_DEBUG

# ifndef YYFPRINTF
#  include <stdio.h> /* INFRINGES ON USER NAME SPACE */
#  define YYFPRINTF fprintf
# endif

# define YYDPRINTF(Args)                        \
do {                                            \
  if (yydebug)                                  \
    YYFPRINTF Args;                             \
} while (0)




# define YY_SYMBOL_PRINT(Title, Kind, Value, Location)                    \
do {                                                                      \
  if (yydebug)                                                            \
    {                                                                     \
      YYFPRINTF (stderr, "%s ", Title);                                   \
      yy_symbol_print (stderr,                                            \
                  Kind, Value, st); \
      YYFPRINTF (stderr, "\n");                                           \
    }                                                                     \
} while (0)


/*-----------------------------------.
| Print this symbol's value on YYO.  |
`-----------------------------------*/

static void
yy_symbol_value_print (FILE *yyo,
                       yysymbol_kind_t yykind, YYSTYPE const * const yyvaluep, ScParseState *st)
{
  FILE *yyoutput = yyo;
  YY_USE (yyoutput);
  YY_USE (st);
  if (!yyvaluep)
    return;
  YY_IGNORE_MAYBE_UNINITIALIZED_BEGIN
  YY_USE (yykind);
  YY_IGNORE_MAYBE_UNINITIALIZED_END
}


/*---------------------------.
| Print this symbol on YYO.  |
`---------------------------*/

static void
yy_symbol_print (FILE *yyo,
                 yysymbol_kind_t yykind, YYSTYPE const * const yyvaluep, ScParseState *st)
{
  YYFPRINTF (yyo, "%s %s (",
             yykind < YYNTOKENS ? "token" : "nterm", yysymbol_name (yykind));

  yy_symbol_value_print (yyo, yykind, yyvaluep, st);
  YYFPRINTF (yyo, ")");
}

/*------------------------------------------------------------------.
| yy_stack_print -- Print the state stack from its BOTTOM up to its |
| TOP (included).                                                   |
`------------------------------------------------------------------*/

static void
yy_stack_print (yy_state_t *yybottom, yy_state_t *yytop)
{
  YYFPRINTF (stderr, "Stack now");
  for (; yybottom <= yytop; yybottom++)
    {
      int yybot = *yybottom;
      YYFPRINTF (stderr, " %d", yybot);
    }
  YYFPRINTF (stderr, "\n");
}

# define YY_STACK_PRINT(Bottom, Top)                            \
do {                                                            \
  if (yydebug)                                                  \
    yy_stack_print ((Bottom), (Top));                           \
} while (0)


/*------------------------------------------------.
| Report that the YYRULE is going to be reduced.  |
`------------------------------------------------*/

static void
yy_reduce_print (yy_state_t *yyssp, YYSTYPE *yyvsp,
                 int yyrule, ScParseState *st)
{
  int yylno = yyrline[yyrule];
  int yynrhs = yyr2[yyrule];
  int yyi;
  YYFPRINTF (stderr, "Reducing stack by rule %d (line %d):\n",
             yyrule - 1, yylno);
  /* The symbols being reduced.  */
  for (yyi = 0; yyi < yynrhs; yyi++)
    {
      YYFPRINTF (stderr, "   $%d = ", yyi + 1);
      yy_symbol_print (stderr,
                       YY_ACCESSING_SYMBOL (+yyssp[yyi + 1 - yynrhs]),
                       &yyvsp[(yyi + 1) - (yynrhs)], st);
      YYFPRINTF (stderr, "\n");
    }
}

# define YY_REDUCE_PRINT(Rule)          \
do {                                    \
  if (yydebug)                          \
    yy_reduce_print (yyssp, yyvsp, Rule, st); \
} while (0)

/* Nonzero means print parse trace.  It is left uninitialized so that
   multiple parsers can coexist.  */
int yydebug;
#else /* !SC_DEBUG */
# define YYDPRINTF(Args) ((void) 0)
# define YY_SYMBOL_PRINT(Title, Kind, Value, Location)
# define YY_STACK_PRINT(Bottom, Top)
# define YY_REDUCE_PRINT(Rule)
#endif /* !SC_DEBUG */


/* YYINITDEPTH -- initial size of the parser's stacks.  */
#ifndef YYINITDEPTH
# define YYINITDEPTH 200
#endif

/* YYMAXDEPTH -- maximum size the stacks can grow to (effective only
   if the built-in stack extension method is used).

   Do not make this value too large; the results are undefined if
   YYSTACK_ALLOC_MAXIMUM < YYSTACK_BYTES (YYMAXDEPTH)
   evaluated with infinite-precision integer arithmetic.  */

#ifndef YYMAXDEPTH
# define YYMAXDEPTH 10000
#endif






/*-----------------------------------------------.
| Release the memory associated to this symbol.  |
`-----------------------------------------------*/

static void
yydestruct (const char *yymsg,
            yysymbol_kind_t yykind, YYSTYPE *yyvaluep, ScParseState *st)
{
  YY_USE (yyvaluep);
  YY_USE (st);
  if (!yymsg)
    yymsg = "Deleting";
  YY_SYMBOL_PRINT (yymsg, yykind, yyvaluep, yylocationp);

  YY_IGNORE_MAYBE_UNINITIALIZED_BEGIN
  YY_USE (yykind);
  YY_IGNORE_MAYBE_UNINITIALIZED_END
}






/*----------.
| yyparse.  |
`----------*/

int
yyparse (ScParseState *st)
{
/* Lookahead token kind.  */
int yychar;


/* The semantic value of the lookahead symbol.  */
/* Default value used for initialization, for pacifying older GCCs
   or non-GCC compilers.  */
YY_INITIAL_VALUE (static YYSTYPE yyval_default;)
YYSTYPE yylval YY_INITIAL_VALUE (= yyval_default);

    /* Number of syntax errors so far.  */
    int yynerrs = 0;

    yy_state_fast_t yystate = 0;
    /* Number of tokens to shift before error messages enabled.  */
    int yyerrstatus = 0;

    /* Refer to the stacks through separate pointers, to allow yyoverflow
       to reallocate them elsewhere.  */

    /* Their size.  */
    YYPTRDIFF_T yystacksize = YYINITDEPTH;

    /* The state stack: array, bottom, top.  */
    yy_state_t yyssa[YYINITDEPTH];
    yy_state_t *yyss = yyssa;
    yy_state_t *yyssp = yyss;

    /* The semantic value stack: array, bottom, top.  */
    YYSTYPE yyvsa[YYINITDEPTH];
    YYSTYPE *yyvs = yyvsa;
    YYSTYPE *yyvsp = yyvs;

  int yyn;
  /* The return value of yyparse.  */
  int yyresult;
  /* Lookahead symbol kind.  */
  yysymbol_kind_t yytoken = YYSYMBOL_YYEMPTY;
  /* The variables used to return semantic value and location from the
     action routines.  */
  YYSTYPE yyval;



#define YYPOPSTACK(N)   (yyvsp -= (N), yyssp -= (N))

  /* The number of symbols on the RHS of the reduced rule.
     Keep to zero when no symbol should be popped.  */
  int yylen = 0;

  YYDPRINTF ((stderr, "Starting parse\n"));

  yychar = SC_EMPTY; /* Cause a token to be read.  */

  goto yysetstate;


/*------------------------------------------------------------.
| yynewstate -- push a new state, which is found in yystate.  |
`------------------------------------------------------------*/
yynewstate:
  /* In all cases, when you get here, the value and location stacks
     have just been pushed.  So pushing a state here evens the stacks.  */
  yyssp++;


/*--------------------------------------------------------------------.
| yysetstate -- set current state (the top of the stack) to yystate.  |
`--------------------------------------------------------------------*/
yysetstate:
  YYDPRINTF ((stderr, "Entering state %d\n", yystate));
  YY_ASSERT (0 <= yystate && yystate < YYNSTATES);
  YY_IGNORE_USELESS_CAST_BEGIN
  *yyssp = YY_CAST (yy_state_t, yystate);
  YY_IGNORE_USELESS_CAST_END
  YY_STACK_PRINT (yyss, yyssp);

  if (yyss + yystacksize - 1 <= yyssp)
#if !defined yyoverflow && !defined YYSTACK_RELOCATE
    YYNOMEM;
#else
    {
      /* Get the current used size of the three stacks, in elements.  */
      YYPTRDIFF_T yysize = yyssp - yyss + 1;

# if defined yyoverflow
      {
        /* Give user a chance to reallocate the stack.  Use copies of
           these so that the &'s don't force the real ones into
           memory.  */
        yy_state_t *yyss1 = yyss;
        YYSTYPE *yyvs1 = yyvs;

        /* Each stack pointer address is followed by the size of the
           data in use in that stack, in bytes.  This used to be a
           conditional around just the two extra args, but that might
           be undefined if yyoverflow is a macro.  */
        yyoverflow (YY_("memory exhausted"),
                    &yyss1, yysize * YYSIZEOF (*yyssp),
                    &yyvs1, yysize * YYSIZEOF (*yyvsp),
                    &yystacksize);
        yyss = yyss1;
        yyvs = yyvs1;
      }
# else /* defined YYSTACK_RELOCATE */
      /* Extend the stack our own way.  */
      if (YYMAXDEPTH <= yystacksize)
        YYNOMEM;
      yystacksize *= 2;
      if (YYMAXDEPTH < yystacksize)
        yystacksize = YYMAXDEPTH;

      {
        yy_state_t *yyss1 = yyss;
        union yyalloc *yyptr =
          YY_CAST (union yyalloc *,
                   YYSTACK_ALLOC (YY_CAST (YYSIZE_T, YYSTACK_BYTES (yystacksize))));
        if (! yyptr)
          YYNOMEM;
        YYSTACK_RELOCATE (yyss_alloc, yyss);
        YYSTACK_RELOCATE (yyvs_alloc, yyvs);
#  undef YYSTACK_RELOCATE
        if (yyss1 != yyssa)
          YYSTACK_FREE (yyss1);
      }
# endif

      yyssp = yyss + yysize - 1;
      yyvsp = yyvs + yysize - 1;

      YY_IGNORE_USELESS_CAST_BEGIN
      YYDPRINTF ((stderr, "Stack size increased to %ld\n",
                  YY_CAST (long, yystacksize)));
      YY_IGNORE_USELESS_CAST_END

      if (yyss + yystacksize - 1 <= yyssp)
        YYABORT;
    }
#endif /* !defined yyoverflow && !defined YYSTACK_RELOCATE */


  if (yystate == YYFINAL)
    YYACCEPT;

  goto yybackup;


/*-----------.
| yybackup.  |
`-----------*/
yybackup:
  /* Do appropriate processing given the current state.  Read a
     lookahead token if we need one and don't already have one.  */

  /* First try to decide what to do without reference to lookahead token.  */
  yyn = yypact[yystate];
  if (yypact_value_is_default (yyn))
    goto yydefault;

  /* Not known => get a lookahead token if don't already have one.  */

  /* YYCHAR is either empty, or end-of-input, or a valid lookahead.  */
  if (yychar == SC_EMPTY)
    {
      YYDPRINTF ((stderr, "Reading a token\n"));
      yychar = yylex (&yylval, st);
    }

  if (yychar <= SC_EOF)
    {
      yychar = SC_EOF;
      yytoken = YYSYMBOL_YYEOF;
      YYDPRINTF ((stderr, "Now at end of input.\n"));
    }
  else if (yychar == SC_error)
    {
      /* The scanner already issued an error message, process directly
         to error recovery.  But do not keep the error token as
         lookahead, it is too special and may lead us to an endless
         loop in error recovery. */
      yychar = SC_UNDEF;
      yytoken = YYSYMBOL_YYerror;
      goto yyerrlab1;
    }
  else
    {
      yytoken = YYTRANSLATE (yychar);
      YY_SYMBOL_PRINT ("Next token is", yytoken, &yylval, &yylloc);
    }

  /* If the proper action on seeing token YYTOKEN is to reduce or to
     detect an error, take that action.  */
  yyn += yytoken;
  if (yyn < 0 || YYLAST < yyn || yycheck[yyn] != yytoken)
    goto yydefault;
  yyn = yytable[yyn];
  if (yyn <= 0)
    {
      if (yytable_value_is_error (yyn))
        goto yyerrlab;
      yyn = -yyn;
      goto yyreduce;
    }

  /* Count tokens shifted since error; after three, turn off error
     status.  */
  if (yyerrstatus)
    yyerrstatus--;

  /* Shift the lookahead token.  */
  YY_SYMBOL_PRINT ("Shifting", yytoken, &yylval, &yylloc);
  yystate = yyn;
  YY_IGNORE_MAYBE_UNINITIALIZED_BEGIN
  *++yyvsp = yylval;
  YY_IGNORE_MAYBE_UNINITIALIZED_END

  /* Discard the shifted token.  */
  yychar = SC_EMPTY;
  goto yynewstate;


/*-----------------------------------------------------------.
| yydefault -- do the default action for the current state.  |
`-----------------------------------------------------------*/
yydefault:
  yyn = yydefact[yystate];
  if (yyn == 0)
    goto yyerrlab;
  goto yyreduce;


/*-----------------------------.
| yyreduce -- do a reduction.  |
`-----------------------------*/
yyreduce:
  /* yyn is the number of a rule to reduce with.  */
  yylen = yyr2[yyn];

  /* If YYLEN is nonzero, implement the default value of the action:
     '$$ = $1'.

     Otherwise, the following line sets YYVAL to garbage.
     This behavior is undocumented and Bison
     users should not rely upon it.  Assigning to YYVAL
     unconditionally makes the parser a bit smaller, and it avoids a
     GCC warning that YYVAL may be used uninitialized.  */
  yyval = yyvsp[1-yylen];


  YY_REDUCE_PRINT (yyn);
  switch (yyn)
    {
  case 10: /* matched_stmt: if_head matched_stmt else_keyword matched_stmt  */
#line 528 "snocone_parse.y"
                                        { sc_finalize_if_else(st, (yyvsp[-3].ifhead), (yyvsp[-1].stmt_ptr)); }
#line 1730 "snocone_parse.tab.c"
    break;

  case 11: /* matched_stmt: while_head matched_stmt  */
#line 530 "snocone_parse.y"
                                        { sc_finalize_while(st, (yyvsp[-1].whilehead)); }
#line 1736 "snocone_parse.tab.c"
    break;

  case 12: /* matched_stmt: do_head do_body T_WHILE T_LPAREN expr0 T_RPAREN T_SEMICOLON  */
#line 537 "snocone_parse.y"
                                        { sc_finalize_do_while(st, (yyvsp[-6].dohead), (yyvsp[-2].expr)); }
#line 1742 "snocone_parse.tab.c"
    break;

  case 13: /* matched_stmt: for_head matched_stmt  */
#line 540 "snocone_parse.y"
                                        { sc_finalize_for(st, (yyvsp[-1].forhead)); }
#line 1748 "snocone_parse.tab.c"
    break;

  case 14: /* matched_stmt: func_head T_LBRACE stmt_list T_RBRACE  */
#line 543 "snocone_parse.y"
                                        { sc_finalize_function(st, (yyvsp[-3].funchead)); }
#line 1754 "snocone_parse.tab.c"
    break;

  case 15: /* matched_stmt: func_head T_LBRACE T_RBRACE  */
#line 545 "snocone_parse.y"
                                        { sc_finalize_function(st, (yyvsp[-2].funchead)); }
#line 1760 "snocone_parse.tab.c"
    break;

  case 16: /* matched_stmt: switch_head T_LBRACE switch_body T_RBRACE  */
#line 552 "snocone_parse.y"
                                        { sc_finalize_switch(st, (yyvsp[-3].switchhead)); }
#line 1766 "snocone_parse.tab.c"
    break;

  case 17: /* matched_stmt: switch_head T_LBRACE T_RBRACE  */
#line 554 "snocone_parse.y"
                                        { sc_finalize_switch(st, (yyvsp[-2].switchhead)); }
#line 1772 "snocone_parse.tab.c"
    break;

  case 18: /* matched_stmt: T_STRUCT T_IDENT T_LBRACE struct_field_list T_RBRACE  */
#line 562 "snocone_parse.y"
                                        { sc_emit_struct(st, (yyvsp[-3].str), (yyvsp[-1].str)); free((yyvsp[-3].str)); free((yyvsp[-1].str)); }
#line 1778 "snocone_parse.tab.c"
    break;

  case 19: /* matched_stmt: T_STRUCT T_IDENT T_LBRACE T_RBRACE  */
#line 564 "snocone_parse.y"
                                        { sc_emit_struct(st, (yyvsp[-2].str), strdup("")); free((yyvsp[-2].str)); }
#line 1784 "snocone_parse.tab.c"
    break;

  case 21: /* unmatched_stmt: if_head stmt  */
#line 571 "snocone_parse.y"
                                        { sc_finalize_if_no_else(st, (yyvsp[-1].ifhead)); }
#line 1790 "snocone_parse.tab.c"
    break;

  case 22: /* unmatched_stmt: if_head matched_stmt else_keyword unmatched_stmt  */
#line 573 "snocone_parse.y"
                                        { sc_finalize_if_else(st, (yyvsp[-3].ifhead), (yyvsp[-1].stmt_ptr)); }
#line 1796 "snocone_parse.tab.c"
    break;

  case 23: /* unmatched_stmt: while_head unmatched_stmt  */
#line 575 "snocone_parse.y"
                                        { sc_finalize_while(st, (yyvsp[-1].whilehead)); }
#line 1802 "snocone_parse.tab.c"
    break;

  case 24: /* unmatched_stmt: for_head unmatched_stmt  */
#line 578 "snocone_parse.y"
                                        { sc_finalize_for(st, (yyvsp[-1].forhead)); }
#line 1808 "snocone_parse.tab.c"
    break;

  case 26: /* if_head: T_IF T_LPAREN expr0 T_RPAREN opt_head_sep  */
#line 584 "snocone_parse.y"
                                        { (yyval.ifhead) = sc_if_head_new(st, (yyvsp[-2].expr)); }
#line 1814 "snocone_parse.tab.c"
    break;

  case 27: /* while_head: T_WHILE T_LPAREN expr0 T_RPAREN opt_head_sep  */
#line 588 "snocone_parse.y"
                                        { (yyval.whilehead) = sc_while_head_new(st, (yyvsp[-2].expr)); }
#line 1820 "snocone_parse.tab.c"
    break;

  case 28: /* do_head: T_DO  */
#line 594 "snocone_parse.y"
                                    { (yyval.dohead) = sc_do_head_new(st); }
#line 1826 "snocone_parse.tab.c"
    break;

  case 31: /* for_lead: T_FOR  */
#line 620 "snocone_parse.y"
                                     { sc_pending_to_stash(st); }
#line 1832 "snocone_parse.tab.c"
    break;

  case 32: /* for_head: for_lead T_LPAREN expr0 T_SEMICOLON expr0 T_SEMICOLON expr0 T_RPAREN opt_head_sep  */
#line 624 "snocone_parse.y"
                                        { sc_append_stmt(st, (yyvsp[-6].expr));
                                          (yyval.forhead) = sc_for_head_new(st, (yyvsp[-4].expr), (yyvsp[-2].expr)); }
#line 1839 "snocone_parse.tab.c"
    break;

  case 33: /* switch_head: T_SWITCH T_LPAREN expr0 T_RPAREN  */
#line 641 "snocone_parse.y"
                                        { (yyval.switchhead) = sc_switch_head_new(st, (yyvsp[-1].expr)); }
#line 1845 "snocone_parse.tab.c"
    break;

  case 38: /* case_or_default_label: T_CASE expr0 T_COLON  */
#line 664 "snocone_parse.y"
                                        { sc_switch_case_label(st, (yyvsp[-1].expr)); }
#line 1851 "snocone_parse.tab.c"
    break;

  case 39: /* case_or_default_label: T_DEFAULT T_COLON  */
#line 665 "snocone_parse.y"
                                        { sc_switch_default_label(st); }
#line 1857 "snocone_parse.tab.c"
    break;

  case 42: /* func_head: T_DEFINE T_IDENT T_LPAREN func_arglist opt_head_sep  */
#line 702 "snocone_parse.y"
                                        { (yyval.funchead) = sc_func_head_new(st, (yyvsp[-3].str), (yyvsp[-1].str)); free((yyvsp[-3].str)); free((yyvsp[-1].str)); }
#line 1863 "snocone_parse.tab.c"
    break;

  case 43: /* func_arglist: T_RPAREN  */
#line 711 "snocone_parse.y"
                                       { (yyval.str) = strdup(""); }
#line 1869 "snocone_parse.tab.c"
    break;

  case 44: /* func_arglist: T_IDENT T_RPAREN  */
#line 712 "snocone_parse.y"
                                       { (yyval.str) = strdup((yyvsp[-1].str)); free((yyvsp[-1].str)); }
#line 1875 "snocone_parse.tab.c"
    break;

  case 45: /* func_arglist: func_arglist_ne T_RPAREN  */
#line 713 "snocone_parse.y"
                                       { (yyval.str) = (yyvsp[-1].str); }
#line 1881 "snocone_parse.tab.c"
    break;

  case 46: /* func_arglist_ne: T_IDENT T_COMMA T_IDENT  */
#line 719 "snocone_parse.y"
                { int len = strlen((yyvsp[-2].str)) + 1 + strlen((yyvsp[0].str)) + 1;
                  char *s = malloc(len); snprintf(s, len, "%s,%s", (yyvsp[-2].str), (yyvsp[0].str));
                  free((yyvsp[-2].str)); free((yyvsp[0].str)); (yyval.str) = s; }
#line 1889 "snocone_parse.tab.c"
    break;

  case 47: /* func_arglist_ne: func_arglist_ne T_COMMA T_IDENT  */
#line 723 "snocone_parse.y"
                { int len = strlen((yyvsp[-2].str)) + 1 + strlen((yyvsp[0].str)) + 1;
                  char *s = malloc(len); snprintf(s, len, "%s,%s", (yyvsp[-2].str), (yyvsp[0].str));
                  free((yyvsp[-2].str)); free((yyvsp[0].str)); (yyval.str) = s; }
#line 1897 "snocone_parse.tab.c"
    break;

  case 48: /* struct_field_list: T_IDENT  */
#line 736 "snocone_parse.y"
                { (yyval.str) = strdup((yyvsp[0].str)); free((yyvsp[0].str)); }
#line 1903 "snocone_parse.tab.c"
    break;

  case 49: /* struct_field_list: struct_field_list T_COMMA T_IDENT  */
#line 738 "snocone_parse.y"
                { int len = strlen((yyvsp[-2].str)) + 1 + strlen((yyvsp[0].str)) + 1;
                  char *s = malloc(len); snprintf(s, len, "%s,%s", (yyvsp[-2].str), (yyvsp[0].str));
                  free((yyvsp[-2].str)); free((yyvsp[0].str)); (yyval.str) = s; }
#line 1911 "snocone_parse.tab.c"
    break;

  case 50: /* else_keyword: T_ELSE  */
#line 749 "snocone_parse.y"
                                     { (yyval.stmt_ptr) = st->code->tail; }
#line 1917 "snocone_parse.tab.c"
    break;

  case 51: /* label_decl: T_IDENT T_COLON  */
#line 767 "snocone_parse.y"
                                     { sc_emit_label_pad(st, (yyvsp[-1].str)); free((yyvsp[-1].str)); }
#line 1923 "snocone_parse.tab.c"
    break;

  case 52: /* simple_stmt: expr0 T_SEMICOLON  */
#line 770 "snocone_parse.y"
                                               { sc_append_stmt(st, (yyvsp[-1].expr)); }
#line 1929 "snocone_parse.tab.c"
    break;

  case 53: /* simple_stmt: T_SEMICOLON  */
#line 771 "snocone_parse.y"
                                               { /* empty stmt */         }
#line 1935 "snocone_parse.tab.c"
    break;

  case 54: /* simple_stmt: T_RETURN expr0 T_SEMICOLON  */
#line 773 "snocone_parse.y"
                                            { sc_append_return(st, (yyvsp[-1].expr)); }
#line 1941 "snocone_parse.tab.c"
    break;

  case 55: /* simple_stmt: T_RETURN T_SEMICOLON  */
#line 774 "snocone_parse.y"
                                            { sc_append_return(st, NULL); }
#line 1947 "snocone_parse.tab.c"
    break;

  case 56: /* simple_stmt: T_FRETURN T_SEMICOLON  */
#line 775 "snocone_parse.y"
                                            { sc_append_freturn(st); }
#line 1953 "snocone_parse.tab.c"
    break;

  case 57: /* simple_stmt: T_NRETURN T_SEMICOLON  */
#line 776 "snocone_parse.y"
                                            { sc_append_nreturn(st); }
#line 1959 "snocone_parse.tab.c"
    break;

  case 58: /* simple_stmt: T_GOTO T_IDENT T_SEMICOLON  */
#line 778 "snocone_parse.y"
                                            { sc_append_goto_label(st, (yyvsp[-1].str)); free((yyvsp[-1].str)); }
#line 1965 "snocone_parse.tab.c"
    break;

  case 59: /* simple_stmt: T_BREAK T_SEMICOLON  */
#line 780 "snocone_parse.y"
                                            { sc_append_break(st, NULL); }
#line 1971 "snocone_parse.tab.c"
    break;

  case 60: /* simple_stmt: T_BREAK T_IDENT T_SEMICOLON  */
#line 781 "snocone_parse.y"
                                            { sc_append_break(st, (yyvsp[-1].str)); free((yyvsp[-1].str)); }
#line 1977 "snocone_parse.tab.c"
    break;

  case 61: /* simple_stmt: T_CONTINUE T_SEMICOLON  */
#line 782 "snocone_parse.y"
                                            { sc_append_continue(st, NULL); }
#line 1983 "snocone_parse.tab.c"
    break;

  case 62: /* simple_stmt: T_CONTINUE T_IDENT T_SEMICOLON  */
#line 783 "snocone_parse.y"
                                             { sc_append_continue(st, (yyvsp[-1].str)); free((yyvsp[-1].str)); }
#line 1989 "snocone_parse.tab.c"
    break;

  case 63: /* block_stmt: T_LBRACE stmt_list T_RBRACE  */
#line 786 "snocone_parse.y"
                                               { /* statements already appended */ }
#line 1995 "snocone_parse.tab.c"
    break;

  case 64: /* block_stmt: T_LBRACE T_RBRACE  */
#line 787 "snocone_parse.y"
                                               { /* empty block */                  }
#line 2001 "snocone_parse.tab.c"
    break;

  case 65: /* expr0: expr1 T_2EQUAL expr0  */
#line 831 "snocone_parse.y"
                                { (yyval.expr) = expr_binary(AST_ASSIGN, (yyvsp[-2].expr), (yyvsp[0].expr)); }
#line 2007 "snocone_parse.tab.c"
    break;

  case 66: /* expr0: expr1 T_2EQUAL  */
#line 841 "snocone_parse.y"
                                { AST_t *empty = expr_new(AST_QLIT);
                                  empty->sval = strdup("");
                                  (yyval.expr) = expr_binary(AST_ASSIGN, (yyvsp[-1].expr), empty); }
#line 2015 "snocone_parse.tab.c"
    break;

  case 67: /* expr0: expr1 T_PLUS_ASSIGN expr0  */
#line 845 "snocone_parse.y"
                                { AST_t *cl = sc_clone_expr_simple((yyvsp[-2].expr));
                                  AST_t *rhs = expr_binary(AST_ADD, cl, (yyvsp[0].expr));
                                  (yyval.expr) = expr_binary(AST_ASSIGN, (yyvsp[-2].expr), rhs); }
#line 2023 "snocone_parse.tab.c"
    break;

  case 68: /* expr0: expr1 T_MINUS_ASSIGN expr0  */
#line 849 "snocone_parse.y"
                                { AST_t *cl = sc_clone_expr_simple((yyvsp[-2].expr));
                                  AST_t *rhs = expr_binary(AST_SUB, cl, (yyvsp[0].expr));
                                  (yyval.expr) = expr_binary(AST_ASSIGN, (yyvsp[-2].expr), rhs); }
#line 2031 "snocone_parse.tab.c"
    break;

  case 69: /* expr0: expr1 T_STAR_ASSIGN expr0  */
#line 853 "snocone_parse.y"
                                { AST_t *cl = sc_clone_expr_simple((yyvsp[-2].expr));
                                  AST_t *rhs = expr_binary(AST_MUL, cl, (yyvsp[0].expr));
                                  (yyval.expr) = expr_binary(AST_ASSIGN, (yyvsp[-2].expr), rhs); }
#line 2039 "snocone_parse.tab.c"
    break;

  case 70: /* expr0: expr1 T_SLASH_ASSIGN expr0  */
#line 857 "snocone_parse.y"
                                { AST_t *cl = sc_clone_expr_simple((yyvsp[-2].expr));
                                  AST_t *rhs = expr_binary(AST_DIV, cl, (yyvsp[0].expr));
                                  (yyval.expr) = expr_binary(AST_ASSIGN, (yyvsp[-2].expr), rhs); }
#line 2047 "snocone_parse.tab.c"
    break;

  case 71: /* expr0: expr1 T_CARET_ASSIGN expr0  */
#line 861 "snocone_parse.y"
                                { AST_t *cl = sc_clone_expr_simple((yyvsp[-2].expr));
                                  AST_t *rhs = expr_binary(AST_POW, cl, (yyvsp[0].expr));
                                  (yyval.expr) = expr_binary(AST_ASSIGN, (yyvsp[-2].expr), rhs); }
#line 2055 "snocone_parse.tab.c"
    break;

  case 72: /* expr0: expr1  */
#line 865 "snocone_parse.y"
                                { (yyval.expr) = (yyvsp[0].expr); }
#line 2061 "snocone_parse.tab.c"
    break;

  case 73: /* expr1: expr3 T_2QUEST expr1  */
#line 878 "snocone_parse.y"
                                { (yyval.expr) = expr_binary(AST_SCAN, (yyvsp[-2].expr), (yyvsp[0].expr)); }
#line 2067 "snocone_parse.tab.c"
    break;

  case 74: /* expr1: expr3  */
#line 880 "snocone_parse.y"
                                { (yyval.expr) = (yyvsp[0].expr); }
#line 2073 "snocone_parse.tab.c"
    break;

  case 75: /* expr3: expr3 T_2PIPE expr4  */
#line 894 "snocone_parse.y"
                                { if ((yyvsp[-2].expr)->kind == AST_ALT) { expr_add_child((yyvsp[-2].expr), (yyvsp[0].expr)); (yyval.expr) = (yyvsp[-2].expr); }
                                  else { AST_t *a = expr_new(AST_ALT);
                                         expr_add_child(a, (yyvsp[-2].expr)); expr_add_child(a, (yyvsp[0].expr));
                                         (yyval.expr) = a; } }
#line 2082 "snocone_parse.tab.c"
    break;

  case 76: /* expr3: expr4  */
#line 899 "snocone_parse.y"
                                { (yyval.expr) = (yyvsp[0].expr); }
#line 2088 "snocone_parse.tab.c"
    break;

  case 77: /* expr4: expr4 T_CONCAT expr5  */
#line 914 "snocone_parse.y"
                                { if ((yyvsp[-2].expr)->kind == AST_SEQ) { expr_add_child((yyvsp[-2].expr), (yyvsp[0].expr)); (yyval.expr) = (yyvsp[-2].expr); }
                                  else { AST_t *s = expr_new(AST_SEQ);
                                         expr_add_child(s, (yyvsp[-2].expr)); expr_add_child(s, (yyvsp[0].expr));
                                         (yyval.expr) = s; } }
#line 2097 "snocone_parse.tab.c"
    break;

  case 78: /* expr4: expr5  */
#line 919 "snocone_parse.y"
                                { (yyval.expr) = (yyvsp[0].expr); }
#line 2103 "snocone_parse.tab.c"
    break;

  case 79: /* expr5: expr5 T_EQ expr6  */
#line 933 "snocone_parse.y"
                                { AST_t *e = expr_new(AST_FNC); e->sval = strdup("EQ");
                                  expr_add_child(e, (yyvsp[-2].expr)); expr_add_child(e, (yyvsp[0].expr)); (yyval.expr) = e; }
#line 2110 "snocone_parse.tab.c"
    break;

  case 80: /* expr5: expr5 T_NE expr6  */
#line 936 "snocone_parse.y"
                                { AST_t *e = expr_new(AST_FNC); e->sval = strdup("NE");
                                  expr_add_child(e, (yyvsp[-2].expr)); expr_add_child(e, (yyvsp[0].expr)); (yyval.expr) = e; }
#line 2117 "snocone_parse.tab.c"
    break;

  case 81: /* expr5: expr5 T_LT expr6  */
#line 939 "snocone_parse.y"
                                { AST_t *e = expr_new(AST_FNC); e->sval = strdup("LT");
                                  expr_add_child(e, (yyvsp[-2].expr)); expr_add_child(e, (yyvsp[0].expr)); (yyval.expr) = e; }
#line 2124 "snocone_parse.tab.c"
    break;

  case 82: /* expr5: expr5 T_GT expr6  */
#line 942 "snocone_parse.y"
                                { AST_t *e = expr_new(AST_FNC); e->sval = strdup("GT");
                                  expr_add_child(e, (yyvsp[-2].expr)); expr_add_child(e, (yyvsp[0].expr)); (yyval.expr) = e; }
#line 2131 "snocone_parse.tab.c"
    break;

  case 83: /* expr5: expr5 T_LE expr6  */
#line 945 "snocone_parse.y"
                                { AST_t *e = expr_new(AST_FNC); e->sval = strdup("LE");
                                  expr_add_child(e, (yyvsp[-2].expr)); expr_add_child(e, (yyvsp[0].expr)); (yyval.expr) = e; }
#line 2138 "snocone_parse.tab.c"
    break;

  case 84: /* expr5: expr5 T_GE expr6  */
#line 948 "snocone_parse.y"
                                { AST_t *e = expr_new(AST_FNC); e->sval = strdup("GE");
                                  expr_add_child(e, (yyvsp[-2].expr)); expr_add_child(e, (yyvsp[0].expr)); (yyval.expr) = e; }
#line 2145 "snocone_parse.tab.c"
    break;

  case 85: /* expr5: expr5 T_LEQ expr6  */
#line 951 "snocone_parse.y"
                                { AST_t *e = expr_new(AST_FNC); e->sval = strdup("LEQ");
                                  expr_add_child(e, (yyvsp[-2].expr)); expr_add_child(e, (yyvsp[0].expr)); (yyval.expr) = e; }
#line 2152 "snocone_parse.tab.c"
    break;

  case 86: /* expr5: expr5 T_LNE expr6  */
#line 954 "snocone_parse.y"
                                { AST_t *e = expr_new(AST_FNC); e->sval = strdup("LNE");
                                  expr_add_child(e, (yyvsp[-2].expr)); expr_add_child(e, (yyvsp[0].expr)); (yyval.expr) = e; }
#line 2159 "snocone_parse.tab.c"
    break;

  case 87: /* expr5: expr5 T_LLT expr6  */
#line 957 "snocone_parse.y"
                                { AST_t *e = expr_new(AST_FNC); e->sval = strdup("LLT");
                                  expr_add_child(e, (yyvsp[-2].expr)); expr_add_child(e, (yyvsp[0].expr)); (yyval.expr) = e; }
#line 2166 "snocone_parse.tab.c"
    break;

  case 88: /* expr5: expr5 T_LGT expr6  */
#line 960 "snocone_parse.y"
                                { AST_t *e = expr_new(AST_FNC); e->sval = strdup("LGT");
                                  expr_add_child(e, (yyvsp[-2].expr)); expr_add_child(e, (yyvsp[0].expr)); (yyval.expr) = e; }
#line 2173 "snocone_parse.tab.c"
    break;

  case 89: /* expr5: expr5 T_LLE expr6  */
#line 963 "snocone_parse.y"
                                { AST_t *e = expr_new(AST_FNC); e->sval = strdup("LLE");
                                  expr_add_child(e, (yyvsp[-2].expr)); expr_add_child(e, (yyvsp[0].expr)); (yyval.expr) = e; }
#line 2180 "snocone_parse.tab.c"
    break;

  case 90: /* expr5: expr5 T_LGE expr6  */
#line 966 "snocone_parse.y"
                                { AST_t *e = expr_new(AST_FNC); e->sval = strdup("LGE");
                                  expr_add_child(e, (yyvsp[-2].expr)); expr_add_child(e, (yyvsp[0].expr)); (yyval.expr) = e; }
#line 2187 "snocone_parse.tab.c"
    break;

  case 91: /* expr5: expr5 T_IDENT_OP expr6  */
#line 969 "snocone_parse.y"
                                { AST_t *e = expr_new(AST_FNC); e->sval = strdup("IDENT");
                                  expr_add_child(e, (yyvsp[-2].expr)); expr_add_child(e, (yyvsp[0].expr)); (yyval.expr) = e; }
#line 2194 "snocone_parse.tab.c"
    break;

  case 92: /* expr5: expr5 T_DIFFER expr6  */
#line 972 "snocone_parse.y"
                                { AST_t *e = expr_new(AST_FNC); e->sval = strdup("DIFFER");
                                  expr_add_child(e, (yyvsp[-2].expr)); expr_add_child(e, (yyvsp[0].expr)); (yyval.expr) = e; }
#line 2201 "snocone_parse.tab.c"
    break;

  case 93: /* expr5: expr6  */
#line 975 "snocone_parse.y"
                                { (yyval.expr) = (yyvsp[0].expr); }
#line 2207 "snocone_parse.tab.c"
    break;

  case 94: /* expr6: expr6 T_2PLUS expr9  */
#line 979 "snocone_parse.y"
                                { (yyval.expr) = expr_binary(AST_ADD, (yyvsp[-2].expr), (yyvsp[0].expr)); }
#line 2213 "snocone_parse.tab.c"
    break;

  case 95: /* expr6: expr6 T_2MINUS expr9  */
#line 981 "snocone_parse.y"
                                { (yyval.expr) = expr_binary(AST_SUB, (yyvsp[-2].expr), (yyvsp[0].expr)); }
#line 2219 "snocone_parse.tab.c"
    break;

  case 96: /* expr6: expr9  */
#line 983 "snocone_parse.y"
                                { (yyval.expr) = (yyvsp[0].expr); }
#line 2225 "snocone_parse.tab.c"
    break;

  case 97: /* expr9: expr9 T_2STAR expr11  */
#line 987 "snocone_parse.y"
                                { (yyval.expr) = expr_binary(AST_MUL, (yyvsp[-2].expr), (yyvsp[0].expr)); }
#line 2231 "snocone_parse.tab.c"
    break;

  case 98: /* expr9: expr9 T_2SLASH expr11  */
#line 989 "snocone_parse.y"
                                { (yyval.expr) = expr_binary(AST_DIV, (yyvsp[-2].expr), (yyvsp[0].expr)); }
#line 2237 "snocone_parse.tab.c"
    break;

  case 99: /* expr9: expr11  */
#line 991 "snocone_parse.y"
                                { (yyval.expr) = (yyvsp[0].expr); }
#line 2243 "snocone_parse.tab.c"
    break;

  case 100: /* expr11: expr12 T_2CARET expr11  */
#line 999 "snocone_parse.y"
                                { (yyval.expr) = expr_binary(AST_POW, (yyvsp[-2].expr), (yyvsp[0].expr)); }
#line 2249 "snocone_parse.tab.c"
    break;

  case 101: /* expr11: expr12  */
#line 1001 "snocone_parse.y"
                                { (yyval.expr) = (yyvsp[0].expr); }
#line 2255 "snocone_parse.tab.c"
    break;

  case 102: /* expr12: expr12 T_2DOLLAR expr15  */
#line 1034 "snocone_parse.y"
                                { (yyval.expr) = expr_binary(AST_CAPT_IMMED_ASGN, (yyvsp[-2].expr), (yyvsp[0].expr)); }
#line 2261 "snocone_parse.tab.c"
    break;

  case 103: /* expr12: expr12 T_2DOT expr15  */
#line 1036 "snocone_parse.y"
                                { (yyval.expr) = expr_binary(AST_CAPT_COND_ASGN,  (yyvsp[-2].expr), (yyvsp[0].expr)); }
#line 2267 "snocone_parse.tab.c"
    break;

  case 104: /* expr12: expr15  */
#line 1038 "snocone_parse.y"
                                { (yyval.expr) = (yyvsp[0].expr); }
#line 2273 "snocone_parse.tab.c"
    break;

  case 105: /* expr15: expr15 T_LBRACK exprlist T_RBRACK  */
#line 1066 "snocone_parse.y"
                                { AST_t *idx = expr_new(AST_IDX);
                                  expr_add_child(idx, (yyvsp[-3].expr));
                                  for (int i = 0; i < (yyvsp[-1].expr)->nchildren; i++)
                                      expr_add_child(idx, (yyvsp[-1].expr)->children[i]);
                                  free((yyvsp[-1].expr)->children); free((yyvsp[-1].expr));
                                  (yyval.expr) = idx; }
#line 2284 "snocone_parse.tab.c"
    break;

  case 106: /* expr15: expr17  */
#line 1073 "snocone_parse.y"
                                { (yyval.expr) = (yyvsp[0].expr); }
#line 2290 "snocone_parse.tab.c"
    break;

  case 107: /* exprlist: exprlist_ne  */
#line 1084 "snocone_parse.y"
                                { (yyval.expr) = (yyvsp[0].expr); }
#line 2296 "snocone_parse.tab.c"
    break;

  case 108: /* exprlist: %empty  */
#line 1086 "snocone_parse.y"
                                { (yyval.expr) = expr_new(AST_NUL); }
#line 2302 "snocone_parse.tab.c"
    break;

  case 109: /* exprlist_ne: exprlist_ne T_COMMA expr0  */
#line 1090 "snocone_parse.y"
                                { expr_add_child((yyvsp[-2].expr), (yyvsp[0].expr)); (yyval.expr) = (yyvsp[-2].expr); }
#line 2308 "snocone_parse.tab.c"
    break;

  case 110: /* exprlist_ne: expr0  */
#line 1092 "snocone_parse.y"
                                { AST_t *l = expr_new(AST_NUL); expr_add_child(l, (yyvsp[0].expr)); (yyval.expr) = l; }
#line 2314 "snocone_parse.tab.c"
    break;

  case 111: /* expr17: T_CALL exprlist T_RPAREN  */
#line 1106 "snocone_parse.y"
                                { AST_t *e = expr_new(AST_FNC);
                                  e->sval = (yyvsp[-2].str);             /* takes ownership */
                                  for (int i = 0; i < (yyvsp[-1].expr)->nchildren; i++)
                                      expr_add_child(e, (yyvsp[-1].expr)->children[i]);
                                  free((yyvsp[-1].expr)->children); free((yyvsp[-1].expr));
                                  (yyval.expr) = e; }
#line 2325 "snocone_parse.tab.c"
    break;

  case 112: /* expr17: T_IDENT  */
#line 1113 "snocone_parse.y"
                                { AST_t *e = expr_new(AST_VAR);
                                  e->sval = (yyvsp[0].str);             /* takes ownership */
                                  (yyval.expr) = e; }
#line 2333 "snocone_parse.tab.c"
    break;

  case 113: /* expr17: T_KEYWORD  */
#line 1117 "snocone_parse.y"
                                { AST_t *e = expr_new(AST_KEYWORD);
                                  e->sval = (yyvsp[0].str);
                                  (yyval.expr) = e; }
#line 2341 "snocone_parse.tab.c"
    break;

  case 114: /* expr17: T_INT  */
#line 1121 "snocone_parse.y"
                                { (yyval.expr) = sc_int_literal((yyvsp[0].str)); free((yyvsp[0].str)); }
#line 2347 "snocone_parse.tab.c"
    break;

  case 115: /* expr17: T_REAL  */
#line 1123 "snocone_parse.y"
                                { (yyval.expr) = sc_real_literal((yyvsp[0].str)); free((yyvsp[0].str)); }
#line 2353 "snocone_parse.tab.c"
    break;

  case 116: /* expr17: T_STR  */
#line 1125 "snocone_parse.y"
                                { (yyval.expr) = sc_str_literal((yyvsp[0].str)); free((yyvsp[0].str)); }
#line 2359 "snocone_parse.tab.c"
    break;

  case 117: /* expr17: T_LPAREN expr0 T_RPAREN  */
#line 1127 "snocone_parse.y"
                                { (yyval.expr) = (yyvsp[-1].expr); }
#line 2365 "snocone_parse.tab.c"
    break;

  case 118: /* expr17: T_LPAREN expr0 T_COMMA exprlist_ne T_RPAREN  */
#line 1148 "snocone_parse.y"
                                { AST_t *a = expr_new(AST_VLIST);
                                  expr_add_child(a, (yyvsp[-3].expr));
                                  for (int i = 0; i < (yyvsp[-1].expr)->nchildren; i++)
                                      expr_add_child(a, (yyvsp[-1].expr)->children[i]);
                                  free((yyvsp[-1].expr)->children); free((yyvsp[-1].expr));
                                  (yyval.expr) = a; }
#line 2376 "snocone_parse.tab.c"
    break;

  case 119: /* expr17: T_LPAREN T_RPAREN  */
#line 1158 "snocone_parse.y"
                                { (yyval.expr) = expr_new(AST_NUL); }
#line 2382 "snocone_parse.tab.c"
    break;

  case 120: /* expr17: T_1PLUS expr17  */
#line 1160 "snocone_parse.y"
                                { (yyval.expr) = expr_unary(AST_PLS, (yyvsp[0].expr)); }
#line 2388 "snocone_parse.tab.c"
    break;

  case 121: /* expr17: T_1MINUS expr17  */
#line 1162 "snocone_parse.y"
                                { (yyval.expr) = expr_unary(AST_MNS, (yyvsp[0].expr)); }
#line 2394 "snocone_parse.tab.c"
    break;

  case 122: /* expr17: T_1STAR expr17  */
#line 1165 "snocone_parse.y"
                                { (yyval.expr) = expr_unary(AST_DEFER,       (yyvsp[0].expr)); }
#line 2400 "snocone_parse.tab.c"
    break;

  case 123: /* expr17: T_1DOT expr17  */
#line 1167 "snocone_parse.y"
                                { (yyval.expr) = expr_unary(AST_NAME,        (yyvsp[0].expr)); }
#line 2406 "snocone_parse.tab.c"
    break;

  case 124: /* expr17: T_1DOLLAR expr17  */
#line 1169 "snocone_parse.y"
                                { (yyval.expr) = expr_unary(AST_INDIRECT,    (yyvsp[0].expr)); }
#line 2412 "snocone_parse.tab.c"
    break;

  case 125: /* expr17: T_1AT expr17  */
#line 1171 "snocone_parse.y"
                                { (yyval.expr) = expr_unary(AST_CAPT_CURSOR, (yyvsp[0].expr)); }
#line 2418 "snocone_parse.tab.c"
    break;

  case 126: /* expr17: T_1TILDE expr17  */
#line 1173 "snocone_parse.y"
                                { (yyval.expr) = expr_unary(AST_NOT,         (yyvsp[0].expr)); }
#line 2424 "snocone_parse.tab.c"
    break;

  case 127: /* expr17: T_1QUEST expr17  */
#line 1175 "snocone_parse.y"
                                { (yyval.expr) = expr_unary(AST_INTERROGATE, (yyvsp[0].expr)); }
#line 2430 "snocone_parse.tab.c"
    break;

  case 128: /* expr17: T_1AMP expr17  */
#line 1177 "snocone_parse.y"
                                { AST_t *_e = expr_unary(AST_OPSYN, (yyvsp[0].expr));
                                  _e->sval = strdup("&"); (yyval.expr) = _e; }
#line 2437 "snocone_parse.tab.c"
    break;

  case 129: /* expr17: T_1PERCENT expr17  */
#line 1180 "snocone_parse.y"
                                { AST_t *_e = expr_unary(AST_OPSYN, (yyvsp[0].expr));
                                  _e->sval = strdup("%"); (yyval.expr) = _e; }
#line 2444 "snocone_parse.tab.c"
    break;

  case 130: /* expr17: T_1SLASH expr17  */
#line 1182 "snocone_parse.y"
                                { AST_t *_e = expr_unary(AST_OPSYN, (yyvsp[0].expr));
                                  _e->sval = strdup("/"); (yyval.expr) = _e; }
#line 2451 "snocone_parse.tab.c"
    break;

  case 131: /* expr17: T_1POUND expr17  */
#line 1184 "snocone_parse.y"
                                { AST_t *_e = expr_unary(AST_OPSYN, (yyvsp[0].expr));
                                  _e->sval = strdup("#"); (yyval.expr) = _e; }
#line 2458 "snocone_parse.tab.c"
    break;

  case 132: /* expr17: T_1PIPE expr17  */
#line 1186 "snocone_parse.y"
                                { AST_t *_e = expr_unary(AST_OPSYN, (yyvsp[0].expr));
                                  _e->sval = strdup("|"); (yyval.expr) = _e; }
#line 2465 "snocone_parse.tab.c"
    break;

  case 133: /* expr17: T_1EQUAL expr17  */
#line 1188 "snocone_parse.y"
                                { AST_t *_e = expr_unary(AST_OPSYN, (yyvsp[0].expr));
                                  _e->sval = strdup("="); (yyval.expr) = _e; }
#line 2472 "snocone_parse.tab.c"
    break;

  case 134: /* expr17: T_1BANG expr17  */
#line 1190 "snocone_parse.y"
                                { AST_t *_e = expr_unary(AST_OPSYN, (yyvsp[0].expr));
                                  _e->sval = strdup("!"); (yyval.expr) = _e; }
#line 2479 "snocone_parse.tab.c"
    break;


#line 2483 "snocone_parse.tab.c"

      default: break;
    }
  /* User semantic actions sometimes alter yychar, and that requires
     that yytoken be updated with the new translation.  We take the
     approach of translating immediately before every use of yytoken.
     One alternative is translating here after every semantic action,
     but that translation would be missed if the semantic action invokes
     YYABORT, YYACCEPT, or YYERROR immediately after altering yychar or
     if it invokes YYBACKUP.  In the case of YYABORT or YYACCEPT, an
     incorrect destructor might then be invoked immediately.  In the
     case of YYERROR or YYBACKUP, subsequent parser actions might lead
     to an incorrect destructor call or verbose syntax error message
     before the lookahead is translated.  */
  YY_SYMBOL_PRINT ("-> $$ =", YY_CAST (yysymbol_kind_t, yyr1[yyn]), &yyval, &yyloc);

  YYPOPSTACK (yylen);
  yylen = 0;

  *++yyvsp = yyval;

  /* Now 'shift' the result of the reduction.  Determine what state
     that goes to, based on the state we popped back to and the rule
     number reduced by.  */
  {
    const int yylhs = yyr1[yyn] - YYNTOKENS;
    const int yyi = yypgoto[yylhs] + *yyssp;
    yystate = (0 <= yyi && yyi <= YYLAST && yycheck[yyi] == *yyssp
               ? yytable[yyi]
               : yydefgoto[yylhs]);
  }

  goto yynewstate;


/*--------------------------------------.
| yyerrlab -- here on detecting error.  |
`--------------------------------------*/
yyerrlab:
  /* Make sure we have latest lookahead translation.  See comments at
     user semantic actions for why this is necessary.  */
  yytoken = yychar == SC_EMPTY ? YYSYMBOL_YYEMPTY : YYTRANSLATE (yychar);
  /* If not already recovering from an error, report this error.  */
  if (!yyerrstatus)
    {
      ++yynerrs;
      yyerror (st, YY_("syntax error"));
    }

  if (yyerrstatus == 3)
    {
      /* If just tried and failed to reuse lookahead token after an
         error, discard it.  */

      if (yychar <= SC_EOF)
        {
          /* Return failure if at end of input.  */
          if (yychar == SC_EOF)
            YYABORT;
        }
      else
        {
          yydestruct ("Error: discarding",
                      yytoken, &yylval, st);
          yychar = SC_EMPTY;
        }
    }

  /* Else will try to reuse lookahead token after shifting the error
     token.  */
  goto yyerrlab1;


/*---------------------------------------------------.
| yyerrorlab -- error raised explicitly by YYERROR.  |
`---------------------------------------------------*/
yyerrorlab:
  /* Pacify compilers when the user code never invokes YYERROR and the
     label yyerrorlab therefore never appears in user code.  */
  if (0)
    YYERROR;
  ++yynerrs;

  /* Do not reclaim the symbols of the rule whose action triggered
     this YYERROR.  */
  YYPOPSTACK (yylen);
  yylen = 0;
  YY_STACK_PRINT (yyss, yyssp);
  yystate = *yyssp;
  goto yyerrlab1;


/*-------------------------------------------------------------.
| yyerrlab1 -- common code for both syntax error and YYERROR.  |
`-------------------------------------------------------------*/
yyerrlab1:
  yyerrstatus = 3;      /* Each real token shifted decrements this.  */

  /* Pop stack until we find a state that shifts the error token.  */
  for (;;)
    {
      yyn = yypact[yystate];
      if (!yypact_value_is_default (yyn))
        {
          yyn += YYSYMBOL_YYerror;
          if (0 <= yyn && yyn <= YYLAST && yycheck[yyn] == YYSYMBOL_YYerror)
            {
              yyn = yytable[yyn];
              if (0 < yyn)
                break;
            }
        }

      /* Pop the current state because it cannot handle the error token.  */
      if (yyssp == yyss)
        YYABORT;


      yydestruct ("Error: popping",
                  YY_ACCESSING_SYMBOL (yystate), yyvsp, st);
      YYPOPSTACK (1);
      yystate = *yyssp;
      YY_STACK_PRINT (yyss, yyssp);
    }

  YY_IGNORE_MAYBE_UNINITIALIZED_BEGIN
  *++yyvsp = yylval;
  YY_IGNORE_MAYBE_UNINITIALIZED_END


  /* Shift the error token.  */
  YY_SYMBOL_PRINT ("Shifting", YY_ACCESSING_SYMBOL (yyn), yyvsp, yylsp);

  yystate = yyn;
  goto yynewstate;


/*-------------------------------------.
| yyacceptlab -- YYACCEPT comes here.  |
`-------------------------------------*/
yyacceptlab:
  yyresult = 0;
  goto yyreturnlab;


/*-----------------------------------.
| yyabortlab -- YYABORT comes here.  |
`-----------------------------------*/
yyabortlab:
  yyresult = 1;
  goto yyreturnlab;


/*-----------------------------------------------------------.
| yyexhaustedlab -- YYNOMEM (memory exhaustion) comes here.  |
`-----------------------------------------------------------*/
yyexhaustedlab:
  yyerror (st, YY_("memory exhausted"));
  yyresult = 2;
  goto yyreturnlab;


/*----------------------------------------------------------.
| yyreturnlab -- parsing is finished, clean up and return.  |
`----------------------------------------------------------*/
yyreturnlab:
  if (yychar != SC_EMPTY)
    {
      /* Make sure we have latest lookahead translation.  See comments at
         user semantic actions for why this is necessary.  */
      yytoken = YYTRANSLATE (yychar);
      yydestruct ("Cleanup: discarding lookahead",
                  yytoken, &yylval, st);
    }
  /* Do not reclaim the symbols of the rule whose action triggered
     this YYABORT or YYACCEPT.  */
  YYPOPSTACK (yylen);
  YY_STACK_PRINT (yyss, yyssp);
  while (yyssp != yyss)
    {
      yydestruct ("Cleanup: popping",
                  YY_ACCESSING_SYMBOL (+*yyssp), yyvsp, st);
      YYPOPSTACK (1);
    }
#ifndef yyoverflow
  if (yyss != yyssa)
    YYSTACK_FREE (yyss);
#endif

  return yyresult;
}

#line 1194 "snocone_parse.y"


void sc_error(ScParseState *st, const char *msg) {
    fprintf(stderr, "%s:%d: snocone parse error: %s\n",
            st->filename ? st->filename : "<stdin>",
            st->ctx ? st->ctx->line : 0,
            msg);
    st->nerrors++;
}


/* ---- Statement assembly ----
 *
 * For LS-4.a there are exactly two shapes:
 *   AST_ASSIGN(lhs, rhs)         -> subject = lhs, replacement = rhs, has_eq = 1
 *   anything else              -> subject = expr (bare expression statement)
 *
 * Mirrors sno4_stmt_commit_go's split logic for the top-level AST_ASSIGN
 * case (snobol4.y line ~250).  Pattern-match split (AST_SCAN), label
 * handling, and goto-field assembly arrive in later LS-4.* steps.
 *
 * LS-4.l fence/match/semantic/trace fix: also split AST_SCAN(subj, pat)
 * out of `s->subject` into separate `s->subject = subj` /
 * `s->pattern = pat` slots so the runtime's pattern-match engine
 * fires.  Without this split, the runtime would evaluate AST_SCAN as a
 * value (always succeeding) instead of doing the SNOBOL4 pattern
 * match.  Mirrors snobol4.y:248-270 byte-for-byte.
 *
 * Two split forms (mirror snobol4.y):
 *
 *   1. AST_SCAN(subj, pat)
 *        -> s->subject = subj, s->pattern = pat
 *      Comes from the binary `?` operator: `subj ? pat` and the
 *      replace form `subj ? pat = repl` (where AST_ASSIGN's lhs is
 *      the AST_SCAN).
 *
 *   2. AST_SEQ(name, rest...) where first child is a name-yielding
 *      atom (AST_VAR / AST_KEYWORD / AST_QLIT / AST_INDIRECT)
 *        -> s->subject = name, s->pattern = rest
 *      Comes from bare juxtaposition: in Snocone with space-as-
 *      concat, `s pat;` lexes as `IDENT(s) T_CONCAT IDENT(pat) ;`
 *      which lowers to AST_SEQ(s, pat).  This is the SNOBOL4
 *      stmt-level "subject pattern" idiom and the runtime expects
 *      the same split.
 *
 * The split applies ONLY to the subject slot.  Replacement is left
 * unchanged: `result = subj ? pat` is AST_ASSIGN(result, AST_SCAN(subj,
 * pat)) and the replacement AST_SCAN evaluates as a value (the
 * matched substring) — that path is correct as is, no split needed.
 */
static void sc_split_subject_pattern(AST_t **subj_io, AST_t **pat_io) {
    AST_t *subj = *subj_io;
    if (*pat_io || !subj) return;

    /* Form 1: AST_SCAN(subj, pat) */
    if (subj->kind == AST_SCAN && subj->nchildren == 2) {
        AST_t *new_subj = subj->children[0];
        AST_t *new_pat  = subj->children[1];
        free(subj->children);
        free(subj);
        *subj_io = new_subj;
        *pat_io  = new_pat;
        return;
    }

    /* Form 2: AST_SEQ(name, rest...) where first child is name-like */
    if (subj->kind == AST_SEQ && subj->nchildren >= 2) {
        AST_t *first = subj->children[0];
        if (first->kind == AST_VAR || first->kind == AST_KEYWORD ||
            first->kind == AST_QLIT || first->kind == AST_INDIRECT) {
            int nc = subj->nchildren - 1;
            AST_t *rest;
            if (nc == 1) {
                rest = subj->children[1];
            } else {
                rest = expr_new(AST_SEQ);
                for (int i = 1; i < subj->nchildren; i++)
                    expr_add_child(rest, subj->children[i]);
            }
            /* Detach the children we kept; free the now-empty AST_SEQ shell. */
            free(subj->children);
            free(subj);
            *subj_io = first;
            *pat_io  = rest;
            return;
        }
    }
}

static void sc_append_stmt(ScParseState *st, AST_t *top) {
    if (!top) return;
    /* LS-4.i.2 — a "real" stmt commit consumes any pending user labels:
     * label_decl already emitted a label-pad which chains forward by
     * SNOBOL4 semantics, so the labels semantically attach to this
     * stmt; subsequent loops should not re-capture them. */
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
    /* LS-4.l: split AST_SCAN/AST_SEQ out of subject into subject+pattern.
     * Applies after AST_ASSIGN-split so `subj ? pat = repl` (which
     * lowers to AST_ASSIGN(AST_SCAN(subj,pat), repl)) gets correctly
     * split into s->subject=subj, s->pattern=pat, s->replacement=repl. */
    sc_split_subject_pattern(&s->subject, &s->pattern);
    if (!st->code->head) st->code->head = st->code->tail = s;
    else { st->code->tail->next = s; st->code->tail = s; }
}

/* ---- Literal builders ---- */
static AST_t *sc_int_literal(const char *txt) {
    AST_t *e = expr_new(AST_ILIT);
    e->ival = strtol(txt, NULL, 10);
    return e;
}

static AST_t *sc_real_literal(const char *txt) {
    AST_t *e = expr_new(AST_FLIT);
    e->dval = strtod(txt, NULL);
    return e;
}

static AST_t *sc_str_literal(const char *txt) {
    AST_t *e = expr_new(AST_QLIT);
    e->sval = strdup(txt);
    return e;
}

/* sc_clone_expr_simple — shallow-recursive clone for compound-assign LHS.
 *
 * Compound-assigns (`a += b`, `a *= b`, etc.) lower to
 *   AST_ASSIGN(a, AST_BINOP(clone(a), b))
 * which means the LHS expression is referenced in two distinct subtrees
 * of the same IR.  The IR's tree representation requires distinct nodes
 * (children pointer arrays would otherwise alias and double-free at
 * cleanup), so we clone the LHS.
 *
 * Coverage: the kinds Snocone source can produce as a compound-assign
 * LHS at this point in the grammar — atomic AST_VAR / AST_KEYWORD / AST_ILIT
 * / AST_FLIT / AST_QLIT, plus n-ary AST_IDX / AST_FNC for `a[i] += 1` and the
 * (rare) `f(x) += 1`.  Anything else is a parse-time bug; we return NULL
 * to surface it loudly via the parse error path rather than silently
 * producing a malformed IR.  When LS-4.d adds subscripts and unaries,
 * extend the switch as needed.
 */
static AST_t *sc_clone_expr_simple(AST_t *e) {
    if (!e) return NULL;
    AST_t *c = expr_new(e->kind);
    /* Scalar fields — copied verbatim. */
    c->ival = e->ival;
    c->dval = e->dval;
    if (e->sval) c->sval = strdup(e->sval);
    /* Children — recursive clone, preserving order. */
    for (int i = 0; i < e->nchildren; i++) {
        expr_add_child(c, sc_clone_expr_simple(e->children[i]));
    }
    return c;
}

/* =========================================================================
 *  LS-4.f — control-flow lowering helpers
 *
 *  Architecture: emit-and-splice.  Control-flow head non-terminals
 *  (if_head, while_head) reduce on T_RPAREN of the head, snapshot
 *  st->code->tail at that moment, and emit no statements.  The body
 *  parses normally and appends its statements to st->code.  At the
 *  parent rule's final action, sc_finalize_* is called with the head
 *  struct (and, for if-else, the else_keyword snapshot).  These
 *  finalizers splice the cond-stmt and label-stmts into the correct
 *  positions in the linked list.
 *
 *  Why splice?  Because emitting in mid-rule MRAs causes Bison
 *  reduce/reduce conflicts when multiple matched/unmatched alternatives
 *  share a prefix (the four if-using rules with their per-rule MRAs at
 *  the same parser state).  Snapshot + final-action splice keeps
 *  emissions out of the contended region.
 *
 *  Naming: synthetic labels are "_Ltop_NNNN", "_Lelse_NNNN",
 *  "_Lend_NNNN" with NNNN a zero-padded 4-digit per-parse counter
 *  (st->label_seq).  The leading underscore guarantees no collision
 *  with user labels (which start with a letter).
 *
 *  Lowering shapes (using L1, L2 for clarity):
 *
 *    if (C) S
 *        →   subj=C  go.onfailure=L1
 *            <stmts of S>
 *            label=L1     (empty landing pad)
 *
 *    if (C) S1 else S2
 *        →   subj=C  go.onfailure=L1
 *            <stmts of S1>
 *            label=NULL  go.uncond=L2
 *            label=L1     (empty landing pad)
 *            <stmts of S2>
 *            label=L2     (empty landing pad)
 *
 *    while (C) S
 *        →   label=L1     (loop top — empty pad)
 *            subj=C  go.onfailure=L2
 *            <stmts of S>
 *            label=NULL  go.uncond=L1
 *            label=L2     (loop end — empty pad)
 *
 *  All landing-pad statements are STMT_t with subject=NULL, has_eq=0,
 *  replacement=NULL.  The downstream IR-walk treats them as no-ops
 *  with a label, exactly as SPITBOL `L1  ` would.
 * ========================================================================= */

static char *sc_label_new(ScParseState *st, const char *prefix) {
    char buf[64];
    snprintf(buf, sizeof buf, "%s_%04d", prefix, ++st->label_seq);
    return strdup(buf);
}

/* Snapshot helper: build an IfHead capturing cond and the current tail. */
static struct IfHead *sc_if_head_new(ScParseState *st, AST_t *cond) {
    struct IfHead *h = calloc(1, sizeof *h);
    h->cond        = cond;
    h->before_body = st->code->tail;       /* may be NULL */
    h->lineno      = st->ctx ? st->ctx->line : 0;
    return h;
}

static struct WhileHead *sc_while_head_new(ScParseState *st, AST_t *cond) {
    struct WhileHead *h = calloc(1, sizeof *h);
    h->cond        = cond;
    h->before_body = st->code->tail;
    h->lineno      = st->ctx ? st->ctx->line : 0;
    /* LS-4.i.2 — eager labels.  finalize_while will reuse these instead of
     * allocating fresh ones, so the body's break/continue stmts target
     * the same names finalize_while emits. */
    h->cont_label  = sc_label_new(st, "_Ltop");
    h->end_label   = sc_label_new(st, "_Lend");
    /* Push a loop frame; consumes pending_user_labels. */
    sc_loop_push(st, strdup(h->cont_label), strdup(h->end_label), 1, 0);
    return h;
}

/* LS-4.g — do_head: snapshot before the do-body is parsed. */
static struct DoHead *sc_do_head_new(ScParseState *st) {
    struct DoHead *h = calloc(1, sizeof *h);
    h->before_body = st->code->tail;
    h->lineno      = st->ctx ? st->ctx->line : 0;
    /* LS-4.i.2 — eager labels.  do/while's `continue` re-evaluates the
     * cond, so cont_label sits before the cond stmt (a fresh "_Lcont_NNNN"
     * that finalize_do_while will splice into the right spot). */
    h->cont_label  = sc_label_new(st, "_Lcont");
    h->end_label   = sc_label_new(st, "_Lend");
    sc_loop_push(st, strdup(h->cont_label), strdup(h->end_label), 1, 0);
    return h;
}

/* LS-4.g — for_head: called AFTER the init expr is emitted, so
 * before_loop snaps the tail that now includes the init stmt.
 *
 * LS-4.i.2 — pending user labels were already moved to st->stash_*
 * by for_lead's action (which fired on T_FOR before init parsed).
 * We pass `from_stash=1` to sc_loop_push so it picks them up there
 * instead of from the (now-cleared) pending list. */
static struct ForHead *sc_for_head_new(ScParseState *st, AST_t *cond, AST_t *step) {
    struct ForHead *h = calloc(1, sizeof *h);
    h->before_loop = st->code->tail;
    h->cond        = cond;
    h->step        = step;
    h->lineno      = st->ctx ? st->ctx->line : 0;
    /* LS-4.i.2 — eager labels.  for-loop's `continue` runs the step,
     * so cont_label sits just before the step stmt. */
    h->cont_label  = sc_label_new(st, "_Lcont");
    h->end_label   = sc_label_new(st, "_Lend");
    sc_loop_push(st, strdup(h->cont_label), strdup(h->end_label), 1, 1);
    return h;
}

/* LS-4.h — function-definition head.  Called when the parser reduces
 * `function NAME ( arglist )`.  Emits the DEFINE call statement and
 * the unconditional skip-the-body goto, snapshots the tail, and saves
 * cur_func_name on the head struct so sc_finalize_function can restore.
 *
 * Emits two STMT_t's:
 *   1. subject = AST_FNC("DEFINE", AST_QLIT("NAME(args)")), no goto
 *   2. subject = NULL, go.uncond = "NAME_end"  (a bare goto stmt)
 *
 * The body's stmts will then be appended via sc_append_stmt() in the
 * usual way.  sc_finalize_function patches in:
 *   - The entry-point label "NAME" — spliced as a label-only pad
 *     immediately AFTER the goto stmt (i.e. at the body's first
 *     position).  Because labels in SNOBOL4 attach to the next
 *     non-empty stmt, this is equivalent to labelling the body's
 *     first stmt with "NAME".
 *   - The end label "NAME_end" — appended at the end as a label pad.
 */
static struct FuncHead *sc_func_head_new(ScParseState *st, char *name, char *argstr) {
    struct FuncHead *h = calloc(1, sizeof *h);
    h->name      = strdup(name);
    /* end_label: "NAME_end" */
    int elen = strlen(name) + 5;
    h->end_label = malloc(elen);
    snprintf(h->end_label, elen, "%s_end", name);
    h->prev_func = st->cur_func_name;     /* save (handles "no nested fn" defensively) */
    h->lineno    = st->ctx ? st->ctx->line : 0;

    /* ---- 1. emit DEFINE('NAME(args)') ---- */
    int slen = strlen(name) + 1 + strlen(argstr) + 2;     /* NAME(args) + NUL */
    char *defarg = malloc(slen);
    snprintf(defarg, slen, "%s(%s)", name, argstr);
    AST_t *qarg = expr_new(AST_QLIT);
    qarg->sval   = defarg;
    AST_t *def_call = expr_new(AST_FNC);
    def_call->sval   = strdup("DEFINE");
    expr_add_child(def_call, qarg);
    sc_append_stmt(st, def_call);   /* appends as bare-expr stmt */

    /* ---- 2. emit :(NAME_end) skip-the-body goto ---- */
    STMT_t *skip = sc_make_goto_uncond_stmt(st, strdup(h->end_label));
    sc_append_chain(st, skip, skip);

    /* Snapshot tail AFTER the goto, so the body splices cleanly. */
    h->after_goto = st->code->tail;

    /* Mark the function context for return/freturn/nreturn */
    st->cur_func_name = h->name;
    return h;
}

/* LS-4.h — sc_finalize_function: splice entry-point label and append end label.
 *
 * Body has already been parsed and appended.  st->code looks like:
 *
 *   ... pre-func stmts ... | DEFINE(...) | goto :(NAME_end) | body0 | body1 | ... | tail
 *                                          ^ h->after_goto                                  ^
 *
 * We need:
 *   1. A label-pad "NAME" between h->after_goto and body0 (so body0 is
 *      the entry point).  If the body is empty (h->after_goto == tail),
 *      append the label pad at the end (it still serves as a label).
 *   2. A label-pad "NAME_end" appended at the very end.
 *
 * Restore cur_func_name to the saved prev_func.
 */
static void sc_finalize_function(ScParseState *st, struct FuncHead *h) {
    /* Entry-point label */
    STMT_t *entry = sc_make_label_stmt(st, strdup(h->name));
    sc_splice_after(st, h->after_goto, entry, entry);

    /* End label (skip target) */
    STMT_t *endpad = sc_make_label_stmt(st, strdup(h->end_label));
    sc_append_chain(st, endpad, endpad);

    /* Restore enclosing function context */
    st->cur_func_name = h->prev_func;

    free(h->name);
    free(h->end_label);
    free(h);
}

/* LS-4.h — sc_append_return: emit `cur_func_name = retval :(RETURN)`
 * (or, with no retval, just `:(RETURN)` as a bare goto-only stmt).
 *
 * SPITBOL/SNOBOL4 functions return their value by assigning to a
 * variable named after the function and then branching to the
 * pseudo-label RETURN.  Snocone's `return E;` lowers to that idiom;
 * `return;` lowers to a bare `:(RETURN)` (the function's slot retains
 * whatever previous value it was set to in the body, or null if
 * never assigned).
 *
 * Outside a function (cur_func_name == NULL), `return` is still
 * legal — it just lowers to `:(RETURN)` and behaves as in SPITBOL
 * (a top-level `:(RETURN)` is a runtime error or fallthrough,
 * depending on dialect).  We do not enforce structural validity here.
 */
static void sc_append_return(ScParseState *st, AST_t *retval) {
    sc_pending_label_clear(st);
    STMT_t *s = stmt_new();
    s->lineno = st->ctx ? st->ctx->line : 0;
    s->stno   = ++st->code->nstmts;
    if (retval && st->cur_func_name) {
        /* fname = retval :(RETURN) */
        AST_t *lhs = expr_new(AST_VAR);
        lhs->sval   = strdup(st->cur_func_name);
        s->subject     = lhs;
        s->replacement = retval;
        s->has_eq      = 1;
    } else if (retval) {
        /* `return E;` outside a function — keep E as bare-expr subject;
         * still emit the RETURN goto so it's syntactically a return.   */
        s->subject = retval;
    }
    s->goto_u = strdup("RETURN");
    if (!st->code->head) st->code->head = st->code->tail = s;
    else { st->code->tail->next = s; st->code->tail = s; }
}

static void sc_append_freturn(ScParseState *st) {
    sc_pending_label_clear(st);
    STMT_t *s = stmt_new();
    s->lineno = st->ctx ? st->ctx->line : 0;
    s->stno   = ++st->code->nstmts;
    s->goto_u = strdup("FRETURN");
    if (!st->code->head) st->code->head = st->code->tail = s;
    else { st->code->tail->next = s; st->code->tail = s; }
}

static void sc_append_nreturn(ScParseState *st) {
    sc_pending_label_clear(st);
    STMT_t *s = stmt_new();
    s->lineno = st->ctx ? st->ctx->line : 0;
    s->stno   = ++st->code->nstmts;
    s->goto_u = strdup("NRETURN");
    if (!st->code->head) st->code->head = st->code->tail = s;
    else { st->code->tail->next = s; st->code->tail = s; }
}

/* Build a label-only landing-pad STMT (subject=NULL).  Takes ownership
 * of `label`.  Does NOT link it into st->code — caller does that. */
static STMT_t *sc_make_label_stmt(ScParseState *st, char *label) {
    STMT_t *s = stmt_new();
    s->lineno = st->ctx ? st->ctx->line : 0;
    s->stno   = ++st->code->nstmts;
    s->label  = label;
    return s;
}

/* Build a STMT whose subject is `cond` and whose go.onfailure points at
 * `fail_target`.  Takes ownership of both.
 *
 * LS-4.l fix — applies the same AST_SCAN/AST_SEQ split used by
 * sc_append_stmt so `if (subj ? pat) {…}`, `while (subj ? pat) {…}`,
 * etc. drive the runtime's pattern-match engine instead of evaluating
 * AST_SCAN as a value (which always succeeds and breaks the if/while
 * failure-driven branch). */
static STMT_t *sc_make_cond_fail_stmt(ScParseState *st, AST_t *cond, char *fail_target, int lineno) {
    STMT_t *s = stmt_new();
    s->lineno = lineno;
    s->stno   = ++st->code->nstmts;
    s->subject = cond;
    sc_split_subject_pattern(&s->subject, &s->pattern);
    s->goto_f = fail_target;
    return s;
}

/* Build a STMT carrying only an unconditional goto.  Takes ownership. */
static STMT_t *sc_make_goto_uncond_stmt(ScParseState *st, char *target) {
    STMT_t *s = stmt_new();
    s->lineno = st->ctx ? st->ctx->line : 0;
    s->stno   = ++st->code->nstmts;
    s->goto_u = target;
    return s;
}

/* LS-4.i.1 — sc_emit_label_pad: append a label-only no-op stmt.
 *
 * Used by the `label_decl : T_IDENT T_COLON` rule.  Produces a STMT_t
 * with `label = name`, no subject, no replacement, no goto.  In
 * SNOBOL4, such a stmt is a valid branch target and chains the
 * label to the next non-empty stmt — exactly matching what the user
 * wrote: `top: x = 1;` should make `top` reach `x = 1` semantically.
 *
 * LS-4.i.2 — also tracks `name` in pending_user_labels so that if the
 * next thing parsed is a loop head, the loop frame can record this
 * label as a `break/continue LABEL;` target.  Stacked labels accumulate
 * (a: b: while(...) — both a and b name the same loop). */
static void sc_emit_label_pad(ScParseState *st, char *label) {
    STMT_t *pad = sc_make_label_stmt(st, strdup(label));
    sc_append_chain(st, pad, pad);
    sc_pending_label_add(st, label);
}

/* LS-4.i.1 — sc_append_goto_label: emit `:(target)` unconditional goto.
 *
 * Used by the `T_GOTO T_IDENT T_SEMICOLON` rule in simple_stmt.  The
 * target is a user-written label name; we duplicate it (since the
 * caller still owns the original IDENT string for free()).
 *
 * LS-4.i.2 — clears pending user labels (a stmt commit, like sc_append_stmt). */
static void sc_append_goto_label(ScParseState *st, char *target) {
    sc_pending_label_clear(st);
    STMT_t *g = sc_make_goto_uncond_stmt(st, strdup(target));
    sc_append_chain(st, g, g);
}

/* Splice helpers — operate on st->code's linked list.
 *
 * sc_splice_after(st, anchor, chain_head, chain_tail)
 *   Inserts the chain [chain_head..chain_tail] (linked via next pointers)
 *   right after `anchor`.  If `anchor` is NULL, prepends to st->code->head.
 *   Updates st->code->tail if appending at end.
 */
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

/* Append a chain to the end of st->code. */
static void sc_append_chain(ScParseState *st, STMT_t *chain_head, STMT_t *chain_tail) {
    if (!chain_head) return;
    if (!chain_tail) chain_tail = chain_head;
    if (!st->code->head) st->code->head = chain_head;
    else                 st->code->tail->next = chain_head;
    st->code->tail = chain_tail;
}

/* Finalize `if (cond) body` (no else).
 *
 *   <existing stmts up to before_body>
 *   cond  :F(Lend)            <- spliced after before_body
 *   <body stmts>
 *   Lend                      <- appended at end
 */
static void sc_finalize_if_no_else(ScParseState *st, struct IfHead *h) {
    char   *Lend       = sc_label_new(st, "_Lend");
    STMT_t *cond_stmt  = sc_make_cond_fail_stmt(st, h->cond, strdup(Lend), h->lineno);
    STMT_t *end_label  = sc_make_label_stmt(st, Lend);
    sc_splice_after(st, h->before_body, cond_stmt, cond_stmt);
    sc_append_chain(st, end_label, end_label);
    free(h);
}

/* Finalize `if (cond) S1 else S2`.
 *
 *   <existing stmts up to before_body>
 *   cond  :F(Lelse)           <- spliced after before_body
 *   <S1 stmts up to before_else>
 *   :(Lend)                   <- spliced after before_else, before S2 begins
 *   Lelse                     <- spliced after the goto
 *   <S2 stmts>
 *   Lend                      <- appended at end
 *
 * before_else snapshots st->code->tail at the moment T_ELSE is
 * recognised (after S1 was fully parsed).  The else-body is whatever
 * was appended after that point.  If S1 was empty (e.g. `if (c) ; else ...`)
 * before_else == h->before_body (or NULL); the splice math still works.
 */
static void sc_finalize_if_else(ScParseState *st, struct IfHead *h, STMT_t *before_else) {
    char   *Lelse     = sc_label_new(st, "_Lelse");
    char   *Lend      = sc_label_new(st, "_Lend");
    STMT_t *cond_stmt = sc_make_cond_fail_stmt(st, h->cond, strdup(Lelse), h->lineno);
    STMT_t *goto_end  = sc_make_goto_uncond_stmt(st, strdup(Lend));
    STMT_t *else_pad  = sc_make_label_stmt(st, Lelse);
    STMT_t *end_pad   = sc_make_label_stmt(st, Lend);
    /* (1) Splice cond_stmt right after before_body (i.e. before S1). */
    sc_splice_after(st, h->before_body, cond_stmt, cond_stmt);
    /* (2) Build chain [goto_end -> else_pad].  Splice anchor: the last
     *     stmt of S1.  When S1 was empty, before_else == h->before_body,
     *     so after step (1) the correct anchor is cond_stmt itself
     *     (otherwise we'd insert before cond, not after).  Otherwise
     *     before_else is the last S1 stmt and remains valid (splicing
     *     cond before S1 only edited h->before_body's next link). */
    STMT_t *anchor = (before_else == h->before_body) ? cond_stmt : before_else;
    goto_end->next = else_pad;
    sc_splice_after(st, anchor, goto_end, else_pad);
    /* (3) Append end_pad at the very end. */
    sc_append_chain(st, end_pad, end_pad);
    free(h);
}

/* Finalize `while (cond) body`.
 *
 *   <existing stmts up to before_body>
 *   Ltop                      <- spliced after before_body
 *   cond  :F(Lend)            <- next in chain
 *   <body stmts>
 *   :(Ltop)                   <- appended at end
 *   Lend                      <- appended at end
 */
static void sc_finalize_while(ScParseState *st, struct WhileHead *h) {
    /* LS-4.i.2 — reuse the eager labels allocated by sc_while_head_new
     * so any `break`/`continue` emitted inside the body targets the
     * same names we lay down here. */
    char   *Ltop      = h->cont_label;       /* take ownership */
    char   *Lend      = h->end_label;
    STMT_t *top_pad   = sc_make_label_stmt(st, Ltop);
    STMT_t *cond_stmt = sc_make_cond_fail_stmt(st, h->cond, strdup(Lend), h->lineno);
    STMT_t *goto_top  = sc_make_goto_uncond_stmt(st, strdup(Ltop));
    STMT_t *end_pad   = sc_make_label_stmt(st, Lend);
    /* (1) Splice [top_pad -> cond_stmt] after before_body. */
    top_pad->next = cond_stmt;
    sc_splice_after(st, h->before_body, top_pad, cond_stmt);
    /* (2) Append [goto_top -> end_pad] at the end. */
    goto_top->next = end_pad;
    sc_append_chain(st, goto_top, end_pad);
    sc_loop_pop(st);
    free(h);
}

/* =========================================================================
 *  LS-4.g — do/while and for lowering helpers
 *
 *  do { S } while (C);
 *      →   Ltop                      <- spliced after before_body
 *          <S stmts>
 *          C  :S(Ltop)               <- appended (success loops back)
 *          Lend                      <- appended
 *
 *  for (init; C; step) S
 *      →   <init stmt>               <- already in list (emitted by for_head rule)
 *          Ltop                      <- spliced after before_loop (= after init)
 *          C  :F(Lend)               <- next in chain
 *          <S stmts>
 *          <step stmt>               <- appended
 *          :(Ltop)                   <- appended
 *          Lend                      <- appended
 *
 *  Note: do/until removed — Snocone follows C's loop forms exactly
 *  (while and do/while only).  Lon directive session 2026-04-30 #12.
 * ========================================================================= */

/* Build a STMT whose subject is `cond` and whose go.onsuccess points at
 * `succ_target`.  Takes ownership of both.
 *
 * LS-4.l fix — same AST_SCAN/AST_SEQ split as sc_make_cond_fail_stmt so
 * `do { ... } while (subj ? pat);` drives the pattern-match engine
 * (without the split, the cond evaluates AST_SCAN as a value, always
 * succeeding, turning do/while into an infinite loop). */
static STMT_t *sc_make_cond_succ_stmt(ScParseState *st, AST_t *cond, char *succ_target, int lineno) {
    STMT_t *s = stmt_new();
    s->lineno  = lineno;
    s->stno    = ++st->code->nstmts;
    s->subject = cond;
    sc_split_subject_pattern(&s->subject, &s->pattern);
    s->goto_s = succ_target;
    return s;
}

static void sc_finalize_do_while(ScParseState *st, struct DoHead *h, AST_t *cond) {
    /* LS-4.i.2 — eager labels.  Ltop is a fresh "_Ltop_NNNN"; cont_label
     * was allocated eagerly by sc_do_head_new for use by `continue;` stmts
     * inside the body.  For do/while, `continue` must re-evaluate cond,
     * so the cont_label sits BEFORE the cond stmt.  Lazy emit: only
     * splice the cont pad if the body actually referenced it (frame's
     * cont_used flag set by sc_append_continue) — keeps the IR shape
     * unchanged for do/while bodies that don't use continue. */
    char   *Ltop      = sc_label_new(st, "_Ltop");
    char   *Lcont     = h->cont_label;       /* take ownership */
    char   *Lend      = h->end_label;
    int     cont_used = st->loop_top ? st->loop_top->cont_used : 0;
    STMT_t *top_pad   = sc_make_label_stmt(st, Ltop);
    STMT_t *cond_stmt = sc_make_cond_succ_stmt(st, cond, strdup(Ltop), h->lineno);
    STMT_t *end_pad   = sc_make_label_stmt(st, Lend);
    /* Splice Ltop-pad right before the do-body. */
    sc_splice_after(st, h->before_body, top_pad, top_pad);
    if (cont_used) {
        STMT_t *cont_pad = sc_make_label_stmt(st, Lcont);
        cont_pad->next  = cond_stmt;
        cond_stmt->next = end_pad;
        sc_append_chain(st, cont_pad, end_pad);
    } else {
        free(Lcont);   /* no continue used; pad and label both unused */
        cond_stmt->next = end_pad;
        sc_append_chain(st, cond_stmt, end_pad);
    }
    sc_loop_pop(st);
    free(h);
}

static void sc_finalize_for(ScParseState *st, struct ForHead *h) {
    /* LS-4.i.2 — eager labels.  Ltop is fresh; cont_label was allocated
     * eagerly by sc_for_head_new for use by `continue;` stmts inside the
     * body.  For for-loop, `continue` must run the step before re-testing
     * cond, so the cont_label sits just before the step stmt.  Lazy emit:
     * only splice the cont pad if the body actually referenced it
     * (frame's cont_used flag set by sc_append_continue) — keeps the IR
     * shape unchanged for bodies that don't use continue. */
    char   *Ltop      = sc_label_new(st, "_Ltop");
    char   *Lcont     = h->cont_label;       /* take ownership */
    char   *Lend      = h->end_label;
    int     cont_used = st->loop_top ? st->loop_top->cont_used : 0;
    STMT_t *top_pad   = sc_make_label_stmt(st, Ltop);
    STMT_t *cond_stmt = sc_make_cond_fail_stmt(st, h->cond, strdup(Lend), h->lineno);
    STMT_t *step_stmt = stmt_new();
    step_stmt->lineno  = h->lineno;
    step_stmt->stno    = ++st->code->nstmts;
    step_stmt->subject = h->step;
    STMT_t *goto_top  = sc_make_goto_uncond_stmt(st, strdup(Ltop));
    STMT_t *end_pad   = sc_make_label_stmt(st, Lend);
    /* Splice [top_pad -> cond_stmt] right after before_loop (after init). */
    top_pad->next = cond_stmt;
    sc_splice_after(st, h->before_loop, top_pad, cond_stmt);
    if (cont_used) {
        STMT_t *cont_pad = sc_make_label_stmt(st, Lcont);
        /* Append cont_pad, step, goto Ltop, Lend. */
        cont_pad->next  = step_stmt;
        step_stmt->next = goto_top;
        goto_top->next  = end_pad;
        sc_append_chain(st, cont_pad, end_pad);
    } else {
        free(Lcont);   /* no continue used; label allocated but unused */
        /* Append step, goto Ltop, Lend (original LS-4.g shape). */
        step_stmt->next = goto_top;
        goto_top->next  = end_pad;
        sc_append_chain(st, step_stmt, end_pad);
    }
    sc_loop_pop(st);
    free(h);
}

/* =========================================================================
 *  LS-4.i.2 — break / continue support
 *
 *  Lowering shapes:
 *
 *    break ;            ->   :(end_label_of_innermost_frame)
 *    break LABEL ;      ->   :(end_label_of_frame_with_user_label_LABEL)
 *    continue ;         ->   :(cont_label_of_innermost_loop_frame)
 *    continue LABEL ;   ->   :(cont_label_of_loop_frame_with_user_label_LABEL)
 *
 *  The cont/end labels are allocated eagerly by sc_*_head_new (so that
 *  break/continue stmts emitted DURING body parsing target them by name)
 *  and reused in sc_finalize_*.  user_labels[] on each frame holds any
 *  labels the user attached just before the loop (`a: while(...)`); both
 *  break a; and break LABEL; (named) and break; (innermost) work.
 *
 *  Out-of-context break/continue and unresolved-label cases call sc_error.
 * ========================================================================= */

static void sc_pending_label_add(ScParseState *st, const char *name) {
    if (st->pending_user_labels_count >= st->pending_user_labels_cap) {
        int newcap = st->pending_user_labels_cap ? st->pending_user_labels_cap * 2 : 4;
        st->pending_user_labels = realloc(st->pending_user_labels, newcap * sizeof(char *));
        st->pending_user_labels_cap = newcap;
    }
    st->pending_user_labels[st->pending_user_labels_count++] = strdup(name);
}

static void sc_pending_label_clear(ScParseState *st) {
    for (int i = 0; i < st->pending_user_labels_count; i++) free(st->pending_user_labels[i]);
    st->pending_user_labels_count = 0;
}

/* Move pending list to the one-slot stash (used by for_lead, since for's
 * init expr emission would otherwise clear pending before sc_for_head_new
 * fires).  Releases any prior stash contents (defensive — should be empty
 * unless someone wrote a top-level `for (for(...; ...; ...);  ...; ...)`
 * which is grammatically valid but unusual). */
static void sc_pending_to_stash(ScParseState *st) {
    /* Free any leftover stash from a prior for_lead that didn't get
     * consumed (shouldn't happen in well-formed input — defensive). */
    for (int i = 0; i < st->stash_for_pending_labels_count; i++) free(st->stash_for_pending_labels[i]);
    free(st->stash_for_pending_labels);
    st->stash_for_pending_labels       = st->pending_user_labels;
    st->stash_for_pending_labels_count = st->pending_user_labels_count;
    st->pending_user_labels       = NULL;
    st->pending_user_labels_count = 0;
    st->pending_user_labels_cap   = 0;
}

/* Push a new LoopFrame onto the loop stack.  Takes ownership of cont_label
 * and end_label.  user_labels are sourced from either pending_user_labels
 * (from_stash=0) or stash_for_pending_labels (from_stash=1, used by for-loop
 * since for_lead moved them to the stash before init parsed). */
static void sc_loop_push(ScParseState *st, char *cont_label, char *end_label, int is_loop, int from_stash) {
    LoopFrame *f = calloc(1, sizeof *f);
    f->cont_label = cont_label;
    f->end_label  = end_label;
    f->is_loop    = is_loop;
    f->outer      = st->loop_top;
    /* Capture user labels: take them by ownership from pending or stash. */
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

/* Search the loop stack innermost-first for a frame whose user_labels
 * contains `name`.  If want_loop is set, switch frames are skipped (LS-4.i.3
 * will make is_loop=0 frames possible; for LS-4.i.2 every frame is a loop). */
static LoopFrame *sc_loop_find_by_user_label(ScParseState *st, const char *name, int want_loop) {
    for (LoopFrame *f = st->loop_top; f; f = f->outer) {
        if (want_loop && !f->is_loop) continue;
        for (int i = 0; i < f->user_labels_count; i++) {
            if (strcmp(f->user_labels[i], name) == 0) return f;
        }
    }
    return NULL;
}

/* Find the innermost frame matching want_loop (1 = loop only, 0 = any). */
static LoopFrame *sc_loop_find_innermost(ScParseState *st, int want_loop) {
    for (LoopFrame *f = st->loop_top; f; f = f->outer) {
        if (want_loop && !f->is_loop) continue;
        return f;
    }
    return NULL;
}

/* `break;` (NULL label) jumps to the innermost loop or switch frame's end.
 * `break LABEL;` jumps to the named frame's end.
 * Out-of-context or unresolved labels emit a parse error and emit no stmt. */
static void sc_append_break(ScParseState *st, char *user_label) {
    LoopFrame *f = user_label
        ? sc_loop_find_by_user_label(st, user_label, 0)   /* break can target switch too */
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

/* `continue;` (NULL label) jumps to the innermost LOOP frame's cont target.
 * `continue LABEL;` jumps to the named loop frame's cont target.
 * Switch frames are not valid `continue` targets. */
static void sc_append_continue(ScParseState *st, char *user_label) {
    LoopFrame *f = user_label
        ? sc_loop_find_by_user_label(st, user_label, 1)   /* continue requires loop */
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

/* =========================================================================
 *  LS-4.i.3 — switch / case / default support
 *
 *  Lowering shape (per Q6 — modern no-fall-through):
 *
 *    switch (e) { case v1: A; case v2: B; default: D; }
 *
 *      tmp = e
 *      IDENT(tmp, v1)        :S(_Lcase_0001)
 *      IDENT(tmp, v2)        :S(_Lcase_0002)
 *                            :(_Ldefault_0003)        <- fallthrough = default jump
 *      _Lcase_0001  A        :(_Lend_0004)
 *      _Lcase_0002  B        :(_Lend_0004)
 *      _Ldefault_0003 D                                <- last clause: no goto
 *      _Lend_0004
 *
 *  If no `default:` clause is present, the dispatch's catch-all goto
 *  targets `_Lend` directly.  Empty `case 1: case 2: stmts;` (stacked
 *  labels with no body between) emits two label pads but no implicit
 *  break between them — both labels chain forward to the body via
 *  SNOBOL4 label-pad chaining semantics.  Detection: when a new
 *  case-or-default label fires, if `code->tail == last_case_label_tail`
 *  no body has been appended since the previous label, suppress the
 *  implicit `:(_Lend)` goto.
 *
 *  `break;` inside the body finds the switch's LoopFrame (is_loop=0)
 *  via sc_loop_find_innermost(want_loop=0) — already-implemented in
 *  LS-4.i.2.  `continue;` skips switch frames (want_loop=1) — also
 *  already-implemented in LS-4.i.2.  All forward-compat groundwork
 *  is now consumed.
 * ========================================================================= */

/* Architecture note re: head/finalize symmetry with while/for/dowhile:
 *
 *   sc_switch_head_new   — eager: emits `tmp = e;` immediately, allocates
 *                           all labels, snapshots after_tmp_assign, pushes
 *                           the frame.  Returns SwitchHead for the parent
 *                           rule to thread through to finalize.
 *
 *   sc_switch_case_label — fires DURING body parsing on each `case v:` head.
 *                           Emits implicit-break-to-end if previous case
 *                           had body, then emits the case-label pad, records
 *                           the (label, value) pair on the SwitchHead's
 *                           cases[] array (consumed by finalize for dispatch
 *                           construction), and snapshots the new last-case-
 *                           label-tail for the next case's break-suppression
 *                           check.
 *
 *   sc_switch_default_label — same as case but with no value (NULL marker)
 *                              and the _Ldefault_NNNN spelling.  Sets
 *                              has_default=1 — multiple defaults are a
 *                              parse error.
 *
 *   sc_finalize_switch   — builds the dispatch chain from cases[] (n
 *                           IDENT(tmp, vN) :S(case_label) stmts plus a
 *                           trailing :(default-or-end) goto), splices it
 *                           after after_tmp_assign, appends the _Lend
 *                           pad at the very end, pops the loop frame,
 *                           frees the SwitchHead and its cases[] array.
 */

static void sc_switch_cases_grow(struct SwitchHead *h) {
    if (h->cases_count >= h->cases_cap) {
        int newcap = h->cases_cap ? h->cases_cap * 2 : 4;
        h->cases = realloc(h->cases, newcap * sizeof *h->cases);
        h->cases_cap = newcap;
    }
}

static struct SwitchHead *sc_switch_head_new(ScParseState *st, AST_t *disc) {
    struct SwitchHead *h = calloc(1, sizeof *h);
    h->disc          = disc;
    h->lineno        = st->ctx ? st->ctx->line : 0;
    h->prev_switch   = st->cur_switch;
    /* Allocate synthetic tmp variable name + the three switch-control labels.
     * Order matters for predictable label numbering: tmp-var, end, default
     * are all allocated up front; per-case labels come later as bodies parse. */
    char buf[64];
    snprintf(buf, sizeof buf, "_Lswitch_t_%04d", ++st->label_seq);
    h->tmp_name      = strdup(buf);
    h->end_label     = sc_label_new(st, "_Lend");
    h->default_label = sc_label_new(st, "_Ldefault");
    h->has_default   = 0;
    /* Emit `tmp = disc;` as a regular assignment statement — sc_append_stmt
     * handles the AST_ASSIGN-to-subject-and-replacement split. */
    AST_t *lhs = expr_new(AST_VAR);
    lhs->sval   = strdup(h->tmp_name);
    AST_t *assign = expr_new(AST_ASSIGN);
    expr_add_child(assign, lhs);
    expr_add_child(assign, disc);   /* takes ownership of disc */
    sc_append_stmt(st, assign);
    /* Snapshot tail AFTER the tmp-assign — this is the splice anchor for
     * the dispatch chain that finalize will build. */
    h->after_tmp_assign     = st->code->tail;
    h->last_case_label_tail = NULL;   /* no case label emitted yet */
    /* Push loop frame with is_loop=0 — break finds it, continue skips. */
    sc_loop_push(st, strdup(h->end_label) /* unused for switch */,
                 strdup(h->end_label), 0 /* is_loop */, 0 /* from_stash */);
    /* Activate this switch as innermost. */
    st->cur_switch = h;
    return h;
}

/* Find the current switch's SwitchHead.  We don't carry it on the LoopFrame
 * (that would couple the LS-4.i.2 frame layout to switch internals), so the
 * grammar instead routes each case_or_default_label action through st via
 * a small per-state pointer.  Set in sc_switch_head_new, cleared in finalize.
 *
 * For nested switches (allowed: `switch (a) { case 1: switch (b) { ... } }`)
 * the inner switch's head saves the outer pointer onto its struct and
 * restores at finalize.  Both switches' cases[] arrays remain independent.
 */
/* Implemented via st->cur_switch field added below. */

/* Emit the implicit-break goto (if needed) right before a case-or-default
 * label is laid down.  "Needed" means: we are not the very first case
 * (last_case_label_tail != NULL) AND a body stmt has been appended since
 * the previous case label (code->tail != last_case_label_tail). */
static void sc_switch_emit_implicit_break(ScParseState *st, struct SwitchHead *h) {
    if (!h->last_case_label_tail) return;            /* first case in the switch */
    if (st->code->tail == h->last_case_label_tail) return;  /* empty body */
    STMT_t *g = sc_make_goto_uncond_stmt(st, strdup(h->end_label));
    sc_append_chain(st, g, g);
}

static void sc_switch_case_label(ScParseState *st, AST_t *value) {
    struct SwitchHead *h = st->cur_switch;
    if (!h) {
        sc_error(st, "case label outside of switch");
        /* `value` leaks on this error path — matches codebase convention
         * (no general expr_free helper exists; cleanup is end-of-parse). */
        (void)value;
        return;
    }
    sc_switch_emit_implicit_break(st, h);
    /* Allocate fresh case label and emit the label pad. */
    char *case_label = sc_label_new(st, "_Lcase");
    STMT_t *pad      = sc_make_label_stmt(st, strdup(case_label));
    sc_append_chain(st, pad, pad);
    /* Record dispatch entry (consumed by finalize). */
    sc_switch_cases_grow(h);
    h->cases[h->cases_count].case_label = case_label;
    h->cases[h->cases_count].value      = value;
    h->cases_count++;
    h->last_case_label_tail = st->code->tail;
    /* A case label is a real "stmt commit" — clear pending user labels so
     * stacked `a: case 1:` (if anyone writes that) doesn't mis-attach. */
    sc_pending_label_clear(st);
}

static void sc_switch_default_label(ScParseState *st) {
    struct SwitchHead *h = st->cur_switch;
    if (!h) {
        sc_error(st, "default label outside of switch");
        return;
    }
    if (h->has_default) {
        sc_error(st, "duplicate default label in switch");
        return;
    }
    h->has_default = 1;
    sc_switch_emit_implicit_break(st, h);
    /* Use the pre-allocated _Ldefault label from sc_switch_head_new. */
    STMT_t *pad = sc_make_label_stmt(st, strdup(h->default_label));
    sc_append_chain(st, pad, pad);
    /* Record default as a case-entry with NULL value.  Dispatch chain
     * skips NULL-value entries (they carry no IDENT() probe — default is
     * the trailing catch-all goto). */
    sc_switch_cases_grow(h);
    h->cases[h->cases_count].case_label = strdup(h->default_label);
    h->cases[h->cases_count].value      = NULL;
    h->cases_count++;
    h->last_case_label_tail = st->code->tail;
    sc_pending_label_clear(st);
}

static void sc_finalize_switch(ScParseState *st, struct SwitchHead *h) {
    /* Build the dispatch chain.  For each case entry with a non-NULL value:
     *   IDENT(tmp, value)   :S(case_label)
     * Followed by a single uncond goto to default_label (if has_default)
     * or end_label (if no default). */
    STMT_t *chain_head = NULL;
    STMT_t *chain_tail = NULL;
    for (int i = 0; i < h->cases_count; i++) {
        if (!h->cases[i].value) continue;   /* default entry — no probe */
        AST_t *probe = expr_new(AST_FNC);
        probe->sval   = strdup("IDENT");
        AST_t *tmp_ref = expr_new(AST_VAR);
        tmp_ref->sval   = strdup(h->tmp_name);
        expr_add_child(probe, tmp_ref);
        expr_add_child(probe, h->cases[i].value);   /* takes ownership */
        h->cases[i].value = NULL;                   /* prevent double-free */
        STMT_t *s = sc_make_cond_succ_stmt(st, probe,
                                           strdup(h->cases[i].case_label),
                                           h->lineno);
        if (!chain_head) chain_head = chain_tail = s;
        else { chain_tail->next = s; chain_tail = s; }
    }
    /* Catch-all goto: targets default_label if a default existed, else end_label. */
    char *catchall = h->has_default ? strdup(h->default_label) : strdup(h->end_label);
    STMT_t *catchgo = sc_make_goto_uncond_stmt(st, catchall);
    if (!chain_head) chain_head = chain_tail = catchgo;
    else { chain_tail->next = catchgo; chain_tail = catchgo; }
    /* Splice dispatch chain after the tmp-assign anchor. */
    sc_splice_after(st, h->after_tmp_assign, chain_head, chain_tail);
    /* Append _Lend pad at the very end. */
    STMT_t *end_pad = sc_make_label_stmt(st, strdup(h->end_label));
    sc_append_chain(st, end_pad, end_pad);
    /* Clean up: pop loop frame, free case-label strings + tmp_name + labels.
     * Note: case[].value is NULL here (transferred to probe nodes above);
     * any unconsumed case-label strings (the case_label allocated by
     * sc_switch_case_label) are owned by the cases[] array and freed here. */
    sc_loop_pop(st);
    /* Restore cur_switch — supports nested switches.  prev_switch is saved
     * in the SwitchHead struct itself by the parent grammar rule (see the
     * { sc_switch_head_new } action, which saves/restores via a temp). */
    st->cur_switch = h->prev_switch;
    for (int i = 0; i < h->cases_count; i++) {
        free(h->cases[i].case_label);
        /* h->cases[i].value should be NULL here (transferred to probe nodes
         * above).  Any non-NULL is a defensive defect — leaked, matches
         * codebase convention (no expr_free helper exists). */
        (void)h->cases[i].value;
    }
    free(h->cases);
    free(h->tmp_name);
    free(h->end_label);
    free(h->default_label);
    free(h);
}

/* =========================================================================
 *  LS-4.i.5 — sc_emit_struct: emit the SPITBOL DATA() call for a struct decl.
 *
 *  Snocone:        struct NAME { f1, f2, f3 }
 *  Lowers to:      DATA('NAME(f1,f2,f3)')         (one bare-expr stmt)
 *
 *  Andrew's `.sc` line 162 introduces `struct` as a record-declaration
 *  keyword; the SNOBOL4/SPITBOL underlying primitive is DATA('NAME(...)'),
 *  which simultaneously installs:
 *    • a constructor function NAME(arg1, arg2, ..., argN), creating a
 *      record value with N fields
 *    • per-field accessor functions f1(), f2(), ..., each acting as both
 *      a getter (`fk(x)` returns the k-th field) and an L-value
 *      (`fk(x) = newval` updates it)
 *
 *  This rung is the bare minimum for Andrew's stack.sc / tree.sc /
 *  counter.sc shapes — `struct link { next, value }` etc. lower to a
 *  single `DATA('link(next,value)')` statement that sits exactly where
 *  the source `struct` did.
 *
 *  IR shape (mirrors sc_func_head_new's DEFINE-emission idiom):
 *
 *    AST_t  qarg     = AST_QLIT  sval = "NAME(f1,f2,f3)"
 *    AST_t  data_call= AST_FNC   sval = "DATA"   children = [ qarg ]
 *    STMT_t  bare-expr stmt with subject = data_call,  no goto, no label.
 *
 *  Empty-fields case: `struct NAME { }` lowers to `DATA('NAME()')`, which
 *  SPITBOL accepts as a zero-field record (rare but legal).  Caller
 *  passes `fields = strdup("")`.
 *
 *  ⛔ This emits in the same place sc_func_head_new emits its DEFINE
 *  call — at top level (or within whatever block the struct decl sits
 *  in).  It does NOT push a new context, has no body to splice, and
 *  needs no finalize companion — struct is a single statement.
 * ========================================================================= */
static void sc_emit_struct(ScParseState *st, char *name, char *fields) {
    /* Build "NAME(fields)" — the QLIT argument to DATA(). */
    int slen = strlen(name) + 1 + strlen(fields) + 2;     /* NAME(fields) + NUL */
    char *spec = malloc(slen);
    snprintf(spec, slen, "%s(%s)", name, fields);

    AST_t *qarg = expr_new(AST_QLIT);
    qarg->sval   = spec;                                   /* takes ownership */

    AST_t *data_call = expr_new(AST_FNC);
    data_call->sval   = strdup("DATA");
    expr_add_child(data_call, qarg);

    sc_append_stmt(st, data_call);                         /* bare-expr stmt */
}

/* =========================================================================
 *  Public entry — snocone_parse_program
 *
 *  Parses a complete Snocone source string into a CODE_t*.  On parse
 *  failure returns NULL (and st.nerrors > 0); on success returns a
 *  freshly-allocated CODE_t ready for the IR/SM pipeline.
 *
 *  CODE_t is the typedef alias of CODE_t (added in LS-4.cn — session
 *  2026-04-30 #7 — for symmetry with AST_t).  Existing callers that
 *  declared the result as `CODE_t *` continue to work; the two names
 *  refer to the same type.
 *
 *  This is the LS-4.a entry point.  When LS-4.j wires it into scrip's
 *  driver, snocone_compile() will collapse to a thin wrapper around
 *  this function (~5 lines), mirroring sno_parse_string() at
 *  snobol4.y:316.
 * ========================================================================= */
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

    /* LS-4.i.2 — clean up any pending label state.  Should be empty at
     * end of a well-formed parse, but errors can leave residue. */
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
