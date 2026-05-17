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
#line 1 "snocone_parse.y"


#line 71 "snocone_parse.tab.c"
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
#line 30 "snocone_parse.y"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include "snocone_lex.h"
#include "../icon/icon_lex.h"
/* compat shims: old names → new canonical names */
#define expr_new(k)            ast_node_new(k)
#define expr_add_child(p,c)    ast_push((p),(c))
#define TT_ASSIGN  TT_ASSIGN
#define TT_ADD     TT_ADD
#define TT_SUB     TT_SUB
#define TT_MUL     TT_MUL
#define TT_DIV     TT_DIV
#define TT_POW     TT_POW
#define TT_SEQ     TT_SEQ
#define TT_ALT     TT_ALT
#define TT_SCAN    TT_SCAN
#define TT_FNC     TT_FNC
#define TT_VAR     TT_VAR
#define TT_KEYWORD TT_KEYWORD
#define TT_QLIT    TT_QLIT
#define TT_ILIT    TT_ILIT
#define TT_FLIT    TT_FLIT
#define TT_NUL     TT_NUL
#define TT_VLIST   TT_VLIST
#define TT_IDX     TT_IDX
#define TT_INDIRECT   TT_INDIRECT
#define TT_DEFER      TT_DEFER
#define TT_NAME       TT_NAME
#define TT_CAPT_CURSOR      TT_CAPT_CURSOR
#define TT_CAPT_IMMED_ASGN  TT_CAPT_IMMED_ASGN
#define TT_CAPT_COND_ASGN   TT_CAPT_COND_ASGN
#define TT_PLS        TT_PLS
#define TT_MNS        TT_MNS
#define TT_NOT        TT_NOT
#define TT_INTERROGATE TT_INTERROGATE
#define TT_OPSYN      TT_OPSYN
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
static void     sc_finalize_if_no_else_pst(ScParseState *st, struct IfHead *h);
/*----------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------*/
static void     sc_finalize_if_else_pst(ScParseState *st, struct IfHead *h, STMT_t *before_else);
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
struct WhileHead {
    tree_t *cond;
    STMT_t *before_body;   /* snapshot taken after loop push, before body stmts */
};
struct DoHead {
    STMT_t *before_body;   /* snapshot taken after loop push, before body stmts */
};
struct ForHead {
    tree_t *cond;
    tree_t *step;
    STMT_t *before_body;
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
static void     sc_finalize_while_pst  (ScParseState *st, struct WhileHead *h, tree_t *cond);
/*----------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------*/
static void     sc_finalize_do_while_pst(ScParseState *st, struct DoHead *h, tree_t *cond);
/*----------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------*/
static struct ForHead   *sc_for_head_new_pst(ScParseState *st, tree_t *cond, tree_t *step, STMT_t *before_body);
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
static void     sc_append_label_node  (ScParseState *st, const char *name);
/*----------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------*/
static void     sc_append_goto_label  (ScParseState *st, char *target);
/*----------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------*/
static void     sc_loop_push           (ScParseState *st, char *cont_label, char *end_label, int is_loop);
/*----------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------*/
static void     sc_loop_pop            (ScParseState *st);
/*----------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------*/
static LoopFrame *sc_loop_find_innermost    (ScParseState *st, int want_loop);
/*----------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------*/
static tree_t  *sc_flatten_arith       (tree_e op, tree_t *left, tree_t *right);
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

#line 402 "snocone_parse.tab.c"

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
       0,   262,   262,   263,   265,   266,   268,   269,   272,   273,
     274,   276,   278,   280,   282,   284,   286,   288,   290,   292,
     294,   297,   299,   301,   303,   305,   307,   310,   319,   326,
     327,   329,   331,   338,   341,   342,   344,   345,   348,   349,
     352,   353,   355,   359,   360,   361,   364,   368,   374,   376,
     382,   385,   387,   388,   389,   395,   396,   397,   398,   399,
     400,   401,   402,   404,   405,   407,   409,   413,   416,   419,
     422,   425,   428,   431,   433,   436,   438,   441,   443,   446,
     449,   452,   455,   458,   461,   464,   467,   470,   473,   476,
     479,   482,   485,   488,   491,   493,   495,   498,   500,   502,
     505,   507,   510,   512,   514,   517,   524,   527,   530,   532,
     534,   537,   544,   548,   552,   554,   556,   558,   560,   567,
     569,   571,   573,   574,   575,   576,   577,   578,   579,   581,
     583,   585,   587,   589,   591
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
#line 275 "snocone_parse.y"
                                        { sc_finalize_if_else_pst(st, (yyvsp[-3].ifhead), (yyvsp[-1].stmt_ptr)); }
#line 1670 "snocone_parse.tab.c"
    break;

  case 11: /* matched_stmt: while_head matched_stmt  */
#line 277 "snocone_parse.y"
                                        { sc_finalize_while_pst(st, (yyvsp[-1].whilehead), (yyvsp[-1].whilehead)->cond); }
#line 1676 "snocone_parse.tab.c"
    break;

  case 12: /* matched_stmt: do_head do_body T_WHILE T_LPAREN expr0 T_RPAREN T_SEMICOLON  */
#line 279 "snocone_parse.y"
                                        { sc_finalize_do_while_pst(st, (yyvsp[-6].dohead), (yyvsp[-2].expr)); }
#line 1682 "snocone_parse.tab.c"
    break;

  case 13: /* matched_stmt: for_head matched_stmt  */
#line 281 "snocone_parse.y"
                                        { sc_finalize_for_pst(st, (yyvsp[-1].forhead)); }
#line 1688 "snocone_parse.tab.c"
    break;

  case 14: /* matched_stmt: func_head T_LBRACE stmt_list T_RBRACE  */
#line 283 "snocone_parse.y"
                                        { sc_finalize_function_pst(st, (yyvsp[-3].funchead)); }
#line 1694 "snocone_parse.tab.c"
    break;

  case 15: /* matched_stmt: func_head T_LBRACE T_RBRACE  */
#line 285 "snocone_parse.y"
                                        { sc_finalize_function_pst(st, (yyvsp[-2].funchead)); }
#line 1700 "snocone_parse.tab.c"
    break;

  case 16: /* matched_stmt: switch_head T_LBRACE switch_body T_RBRACE  */
#line 287 "snocone_parse.y"
                                        { sc_finalize_switch_pst(st, (yyvsp[-3].switchhead)); }
#line 1706 "snocone_parse.tab.c"
    break;

  case 17: /* matched_stmt: switch_head T_LBRACE T_RBRACE  */
#line 289 "snocone_parse.y"
                                        { sc_finalize_switch_pst(st, (yyvsp[-2].switchhead)); }
#line 1712 "snocone_parse.tab.c"
    break;

  case 18: /* matched_stmt: T_STRUCT T_IDENT T_LBRACE struct_field_list T_RBRACE  */
#line 291 "snocone_parse.y"
                                        { sc_emit_struct(st, (yyvsp[-3].str), (yyvsp[-1].str)); free((yyvsp[-3].str)); free((yyvsp[-1].str)); }
#line 1718 "snocone_parse.tab.c"
    break;

  case 19: /* matched_stmt: T_STRUCT T_IDENT T_LBRACE T_RBRACE  */
#line 293 "snocone_parse.y"
                                        { sc_emit_struct(st, (yyvsp[-2].str), strdup("")); free((yyvsp[-2].str)); }
#line 1724 "snocone_parse.tab.c"
    break;

  case 21: /* unmatched_stmt: if_head stmt  */
#line 298 "snocone_parse.y"
                                        { sc_finalize_if_no_else_pst(st, (yyvsp[-1].ifhead)); }
#line 1730 "snocone_parse.tab.c"
    break;

  case 22: /* unmatched_stmt: if_head matched_stmt else_keyword unmatched_stmt  */
#line 300 "snocone_parse.y"
                                        { sc_finalize_if_else_pst(st, (yyvsp[-3].ifhead), (yyvsp[-1].stmt_ptr)); }
#line 1736 "snocone_parse.tab.c"
    break;

  case 23: /* unmatched_stmt: while_head unmatched_stmt  */
#line 302 "snocone_parse.y"
                                        { sc_finalize_while_pst(st, (yyvsp[-1].whilehead), (yyvsp[-1].whilehead)->cond); }
#line 1742 "snocone_parse.tab.c"
    break;

  case 24: /* unmatched_stmt: for_head unmatched_stmt  */
#line 304 "snocone_parse.y"
                                        { sc_finalize_for_pst(st, (yyvsp[-1].forhead)); }
#line 1748 "snocone_parse.tab.c"
    break;

  case 26: /* if_head: T_IF T_LPAREN expr0 T_RPAREN opt_head_sep  */
#line 308 "snocone_parse.y"
                                        { (yyval.ifhead) = sc_if_head_new(st, (yyvsp[-2].expr)); }
#line 1754 "snocone_parse.tab.c"
    break;

  case 27: /* while_head: T_WHILE T_LPAREN expr0 T_RPAREN opt_head_sep  */
#line 311 "snocone_parse.y"
                                        { char *lc = sc_label_new(st, "_Ltop");
                                          char *le = sc_label_new(st, "_Lend");
                                          sc_loop_push(st, lc, le, 1);
                                          struct WhileHead *wh = calloc(1, sizeof *wh);
                                          wh->cond        = (yyvsp[-2].expr);
                                          wh->before_body = st->code->tail;
                                          (yyval.whilehead) = wh; }
#line 1766 "snocone_parse.tab.c"
    break;

  case 28: /* do_head: T_DO  */
#line 319 "snocone_parse.y"
                                    { char *lc = sc_label_new(st, "_Lcont");
                                      char *le = sc_label_new(st, "_Lend");
                                      sc_loop_push(st, lc, le, 1);
                                      struct DoHead *dh = calloc(1, sizeof *dh);
                                      dh->before_body = st->code->tail;
                                      (yyval.dohead) = dh; }
#line 1777 "snocone_parse.tab.c"
    break;

  case 31: /* for_lead: T_FOR  */
#line 329 "snocone_parse.y"
                                     { }
#line 1783 "snocone_parse.tab.c"
    break;

  case 32: /* for_head: for_lead T_LPAREN expr0 T_SEMICOLON expr0 T_SEMICOLON expr0 T_RPAREN opt_head_sep  */
#line 332 "snocone_parse.y"
                                        { sc_append_stmt(st, (yyvsp[-6].expr));
                                          char *lc = sc_label_new(st, "_Lcont");
                                          char *le = sc_label_new(st, "_Lend");
                                          sc_loop_push(st, lc, le, 1);
                                          (yyval.forhead) = sc_for_head_new_pst(st, (yyvsp[-4].expr), (yyvsp[-2].expr), st->code->tail); }
#line 1793 "snocone_parse.tab.c"
    break;

  case 33: /* switch_head: T_SWITCH T_LPAREN expr0 T_RPAREN  */
#line 339 "snocone_parse.y"
                                        { (yyval.switchhead) = sc_switch_head_new(st, (yyvsp[-1].expr)); }
#line 1799 "snocone_parse.tab.c"
    break;

  case 38: /* case_or_default_label: T_CASE expr0 T_COLON  */
#line 348 "snocone_parse.y"
                                        { sc_switch_case_label(st, (yyvsp[-1].expr)); }
#line 1805 "snocone_parse.tab.c"
    break;

  case 39: /* case_or_default_label: T_DEFAULT T_COLON  */
#line 349 "snocone_parse.y"
                                        { sc_switch_default_label(st); }
#line 1811 "snocone_parse.tab.c"
    break;

  case 42: /* func_head: T_DEFINE T_IDENT T_LPAREN func_arglist opt_head_sep  */
#line 356 "snocone_parse.y"
                                        { (yyval.funchead) = sc_func_head_new_pst(st, (yyvsp[-3].str), (yyvsp[-1].str)); free((yyvsp[-3].str)); free((yyvsp[-1].str)); }
#line 1817 "snocone_parse.tab.c"
    break;

  case 43: /* func_arglist: T_RPAREN  */
#line 359 "snocone_parse.y"
                                       { (yyval.str) = strdup(""); }
#line 1823 "snocone_parse.tab.c"
    break;

  case 44: /* func_arglist: T_IDENT T_RPAREN  */
#line 360 "snocone_parse.y"
                                       { (yyval.str) = strdup((yyvsp[-1].str)); free((yyvsp[-1].str)); }
#line 1829 "snocone_parse.tab.c"
    break;

  case 45: /* func_arglist: func_arglist_ne T_RPAREN  */
#line 361 "snocone_parse.y"
                                       { (yyval.str) = (yyvsp[-1].str); }
#line 1835 "snocone_parse.tab.c"
    break;

  case 46: /* func_arglist_ne: T_IDENT T_COMMA T_IDENT  */
#line 365 "snocone_parse.y"
                { int len = strlen((yyvsp[-2].str)) + 1 + strlen((yyvsp[0].str)) + 1;
                  char *s = malloc(len); snprintf(s, len, "%s,%s", (yyvsp[-2].str), (yyvsp[0].str));
                  free((yyvsp[-2].str)); free((yyvsp[0].str)); (yyval.str) = s; }
#line 1843 "snocone_parse.tab.c"
    break;

  case 47: /* func_arglist_ne: func_arglist_ne T_COMMA T_IDENT  */
#line 369 "snocone_parse.y"
                { int len = strlen((yyvsp[-2].str)) + 1 + strlen((yyvsp[0].str)) + 1;
                  char *s = malloc(len); snprintf(s, len, "%s,%s", (yyvsp[-2].str), (yyvsp[0].str));
                  free((yyvsp[-2].str)); free((yyvsp[0].str)); (yyval.str) = s; }
#line 1851 "snocone_parse.tab.c"
    break;

  case 48: /* struct_field_list: T_IDENT  */
#line 375 "snocone_parse.y"
                { (yyval.str) = strdup((yyvsp[0].str)); free((yyvsp[0].str)); }
#line 1857 "snocone_parse.tab.c"
    break;

  case 49: /* struct_field_list: struct_field_list T_COMMA T_IDENT  */
#line 377 "snocone_parse.y"
                { int len = strlen((yyvsp[-2].str)) + 1 + strlen((yyvsp[0].str)) + 1;
                  char *s = malloc(len); snprintf(s, len, "%s,%s", (yyvsp[-2].str), (yyvsp[0].str));
                  free((yyvsp[-2].str)); free((yyvsp[0].str)); (yyval.str) = s; }
#line 1865 "snocone_parse.tab.c"
    break;

  case 50: /* else_keyword: T_ELSE  */
#line 382 "snocone_parse.y"
                                     { (yyval.stmt_ptr) = st->code->tail; }
#line 1871 "snocone_parse.tab.c"
    break;

  case 51: /* label_decl: T_IDENT T_COLON  */
#line 385 "snocone_parse.y"
                                     { sc_append_label_node(st, (yyvsp[-1].str)); free((yyvsp[-1].str)); }
#line 1877 "snocone_parse.tab.c"
    break;

  case 52: /* simple_stmt: expr0 T_SEMICOLON  */
#line 387 "snocone_parse.y"
                                               { sc_append_stmt(st, (yyvsp[-1].expr)); }
#line 1883 "snocone_parse.tab.c"
    break;

  case 53: /* simple_stmt: T_SEMICOLON  */
#line 388 "snocone_parse.y"
                                               {         }
#line 1889 "snocone_parse.tab.c"
    break;

  case 54: /* simple_stmt: T_RETURN expr0 T_SEMICOLON  */
#line 389 "snocone_parse.y"
                                            { if (st->cur_func_name) {
                                               tree_t *lhs = ast_node_new(TT_VAR); lhs->sval = strdup(st->cur_func_name);
                                               tree_t *asgn = ast_node_new(TT_ASSIGN); ast_push(asgn, lhs); ast_push(asgn, (yyvsp[-1].expr));
                                               sc_append_stmt(st, asgn);
                                             } else { sc_append_stmt(st, (yyvsp[-1].expr)); }
                                             sc_append_stmt(st, ast_node_new(TT_RETURN)); }
#line 1900 "snocone_parse.tab.c"
    break;

  case 55: /* simple_stmt: T_RETURN T_SEMICOLON  */
#line 395 "snocone_parse.y"
                                            { sc_append_stmt(st, ast_node_new(TT_RETURN)); }
#line 1906 "snocone_parse.tab.c"
    break;

  case 56: /* simple_stmt: T_FRETURN T_SEMICOLON  */
#line 396 "snocone_parse.y"
                                            { sc_append_stmt(st, ast_node_new(TT_PROC_FAIL)); }
#line 1912 "snocone_parse.tab.c"
    break;

  case 57: /* simple_stmt: T_NRETURN T_SEMICOLON  */
#line 397 "snocone_parse.y"
                                            { sc_append_stmt(st, ast_node_new(TT_NRETURN)); }
#line 1918 "snocone_parse.tab.c"
    break;

  case 58: /* simple_stmt: T_GOTO T_IDENT T_SEMICOLON  */
#line 398 "snocone_parse.y"
                                            { sc_append_goto_label(st, (yyvsp[-1].str)); free((yyvsp[-1].str)); }
#line 1924 "snocone_parse.tab.c"
    break;

  case 59: /* simple_stmt: T_BREAK T_SEMICOLON  */
#line 399 "snocone_parse.y"
                                            { sc_append_break(st, NULL); }
#line 1930 "snocone_parse.tab.c"
    break;

  case 60: /* simple_stmt: T_BREAK T_IDENT T_SEMICOLON  */
#line 400 "snocone_parse.y"
                                            { sc_append_break(st, (yyvsp[-1].str)); free((yyvsp[-1].str)); }
#line 1936 "snocone_parse.tab.c"
    break;

  case 61: /* simple_stmt: T_CONTINUE T_SEMICOLON  */
#line 401 "snocone_parse.y"
                                            { sc_append_continue(st, NULL); }
#line 1942 "snocone_parse.tab.c"
    break;

  case 62: /* simple_stmt: T_CONTINUE T_IDENT T_SEMICOLON  */
#line 402 "snocone_parse.y"
                                             { sc_append_continue(st, (yyvsp[-1].str)); free((yyvsp[-1].str)); }
#line 1948 "snocone_parse.tab.c"
    break;

  case 63: /* block_stmt: T_LBRACE stmt_list T_RBRACE  */
#line 404 "snocone_parse.y"
                                               { }
#line 1954 "snocone_parse.tab.c"
    break;

  case 64: /* block_stmt: T_LBRACE T_RBRACE  */
#line 405 "snocone_parse.y"
                                               {                  }
#line 1960 "snocone_parse.tab.c"
    break;

  case 65: /* expr0: expr1 T_2EQUAL expr0  */
#line 408 "snocone_parse.y"
                                { (yyval.expr) = expr_binary(TT_ASSIGN, (yyvsp[-2].expr), (yyvsp[0].expr)); }
#line 1966 "snocone_parse.tab.c"
    break;

  case 66: /* expr0: expr1 T_2EQUAL  */
#line 410 "snocone_parse.y"
                                { tree_t *empty = expr_new(TT_QLIT);
                                  empty->sval = strdup("");
                                  (yyval.expr) = expr_binary(TT_ASSIGN, (yyvsp[-1].expr), empty); }
#line 1974 "snocone_parse.tab.c"
    break;

  case 67: /* expr0: expr1 T_PLUS_ASSIGN expr0  */
#line 414 "snocone_parse.y"
                                { tree_t *a = ast_node_new(TT_AUGOP); a->ival = TK_AUGPLUS;
                                  ast_push(a, (yyvsp[-2].expr)); ast_push(a, (yyvsp[0].expr)); (yyval.expr) = a; }
#line 1981 "snocone_parse.tab.c"
    break;

  case 68: /* expr0: expr1 T_MINUS_ASSIGN expr0  */
#line 417 "snocone_parse.y"
                                { tree_t *a = ast_node_new(TT_AUGOP); a->ival = TK_AUGMINUS;
                                  ast_push(a, (yyvsp[-2].expr)); ast_push(a, (yyvsp[0].expr)); (yyval.expr) = a; }
#line 1988 "snocone_parse.tab.c"
    break;

  case 69: /* expr0: expr1 T_STAR_ASSIGN expr0  */
#line 420 "snocone_parse.y"
                                { tree_t *a = ast_node_new(TT_AUGOP); a->ival = TK_AUGSTAR;
                                  ast_push(a, (yyvsp[-2].expr)); ast_push(a, (yyvsp[0].expr)); (yyval.expr) = a; }
#line 1995 "snocone_parse.tab.c"
    break;

  case 70: /* expr0: expr1 T_SLASH_ASSIGN expr0  */
#line 423 "snocone_parse.y"
                                { tree_t *a = ast_node_new(TT_AUGOP); a->ival = TK_AUGSLASH;
                                  ast_push(a, (yyvsp[-2].expr)); ast_push(a, (yyvsp[0].expr)); (yyval.expr) = a; }
#line 2002 "snocone_parse.tab.c"
    break;

  case 71: /* expr0: expr1 T_CARET_ASSIGN expr0  */
#line 426 "snocone_parse.y"
                                { tree_t *a = ast_node_new(TT_AUGOP); a->ival = TK_AUGPOW;
                                  ast_push(a, (yyvsp[-2].expr)); ast_push(a, (yyvsp[0].expr)); (yyval.expr) = a; }
#line 2009 "snocone_parse.tab.c"
    break;

  case 72: /* expr0: expr1  */
#line 429 "snocone_parse.y"
                                { (yyval.expr) = (yyvsp[0].expr); }
#line 2015 "snocone_parse.tab.c"
    break;

  case 73: /* expr1: expr3 T_2QUEST expr1  */
#line 432 "snocone_parse.y"
                                { (yyval.expr) = expr_binary(TT_SCAN, (yyvsp[-2].expr), (yyvsp[0].expr)); }
#line 2021 "snocone_parse.tab.c"
    break;

  case 74: /* expr1: expr3  */
#line 434 "snocone_parse.y"
                                { (yyval.expr) = (yyvsp[0].expr); }
#line 2027 "snocone_parse.tab.c"
    break;

  case 75: /* expr3: expr3 T_2PIPE expr4  */
#line 437 "snocone_parse.y"
                                { (yyval.expr) = sc_flatten_arith(TT_ALT, (yyvsp[-2].expr), (yyvsp[0].expr)); }
#line 2033 "snocone_parse.tab.c"
    break;

  case 76: /* expr3: expr4  */
#line 439 "snocone_parse.y"
                                { (yyval.expr) = (yyvsp[0].expr); }
#line 2039 "snocone_parse.tab.c"
    break;

  case 77: /* expr4: expr4 T_CONCAT expr5  */
#line 442 "snocone_parse.y"
                                { (yyval.expr) = sc_flatten_arith(TT_SEQ, (yyvsp[-2].expr), (yyvsp[0].expr)); }
#line 2045 "snocone_parse.tab.c"
    break;

  case 78: /* expr4: expr5  */
#line 444 "snocone_parse.y"
                                { (yyval.expr) = (yyvsp[0].expr); }
#line 2051 "snocone_parse.tab.c"
    break;

  case 79: /* expr5: expr5 T_EQ expr6  */
#line 447 "snocone_parse.y"
                                { tree_t *e = expr_new(TT_FNC); e->sval = strdup("EQ");
                                  expr_add_child(e, (yyvsp[-2].expr)); expr_add_child(e, (yyvsp[0].expr)); (yyval.expr) = e; }
#line 2058 "snocone_parse.tab.c"
    break;

  case 80: /* expr5: expr5 T_NE expr6  */
#line 450 "snocone_parse.y"
                                { tree_t *e = expr_new(TT_FNC); e->sval = strdup("NE");
                                  expr_add_child(e, (yyvsp[-2].expr)); expr_add_child(e, (yyvsp[0].expr)); (yyval.expr) = e; }
#line 2065 "snocone_parse.tab.c"
    break;

  case 81: /* expr5: expr5 T_LT expr6  */
#line 453 "snocone_parse.y"
                                { tree_t *e = expr_new(TT_FNC); e->sval = strdup("LT");
                                  expr_add_child(e, (yyvsp[-2].expr)); expr_add_child(e, (yyvsp[0].expr)); (yyval.expr) = e; }
#line 2072 "snocone_parse.tab.c"
    break;

  case 82: /* expr5: expr5 T_GT expr6  */
#line 456 "snocone_parse.y"
                                { tree_t *e = expr_new(TT_FNC); e->sval = strdup("GT");
                                  expr_add_child(e, (yyvsp[-2].expr)); expr_add_child(e, (yyvsp[0].expr)); (yyval.expr) = e; }
#line 2079 "snocone_parse.tab.c"
    break;

  case 83: /* expr5: expr5 T_LE expr6  */
#line 459 "snocone_parse.y"
                                { tree_t *e = expr_new(TT_FNC); e->sval = strdup("LE");
                                  expr_add_child(e, (yyvsp[-2].expr)); expr_add_child(e, (yyvsp[0].expr)); (yyval.expr) = e; }
#line 2086 "snocone_parse.tab.c"
    break;

  case 84: /* expr5: expr5 T_GE expr6  */
#line 462 "snocone_parse.y"
                                { tree_t *e = expr_new(TT_FNC); e->sval = strdup("GE");
                                  expr_add_child(e, (yyvsp[-2].expr)); expr_add_child(e, (yyvsp[0].expr)); (yyval.expr) = e; }
#line 2093 "snocone_parse.tab.c"
    break;

  case 85: /* expr5: expr5 T_LEQ expr6  */
#line 465 "snocone_parse.y"
                                { tree_t *e = expr_new(TT_FNC); e->sval = strdup("LEQ");
                                  expr_add_child(e, (yyvsp[-2].expr)); expr_add_child(e, (yyvsp[0].expr)); (yyval.expr) = e; }
#line 2100 "snocone_parse.tab.c"
    break;

  case 86: /* expr5: expr5 T_LNE expr6  */
#line 468 "snocone_parse.y"
                                { tree_t *e = expr_new(TT_FNC); e->sval = strdup("LNE");
                                  expr_add_child(e, (yyvsp[-2].expr)); expr_add_child(e, (yyvsp[0].expr)); (yyval.expr) = e; }
#line 2107 "snocone_parse.tab.c"
    break;

  case 87: /* expr5: expr5 T_LLT expr6  */
#line 471 "snocone_parse.y"
                                { tree_t *e = expr_new(TT_FNC); e->sval = strdup("LLT");
                                  expr_add_child(e, (yyvsp[-2].expr)); expr_add_child(e, (yyvsp[0].expr)); (yyval.expr) = e; }
#line 2114 "snocone_parse.tab.c"
    break;

  case 88: /* expr5: expr5 T_LGT expr6  */
#line 474 "snocone_parse.y"
                                { tree_t *e = expr_new(TT_FNC); e->sval = strdup("LGT");
                                  expr_add_child(e, (yyvsp[-2].expr)); expr_add_child(e, (yyvsp[0].expr)); (yyval.expr) = e; }
#line 2121 "snocone_parse.tab.c"
    break;

  case 89: /* expr5: expr5 T_LLE expr6  */
#line 477 "snocone_parse.y"
                                { tree_t *e = expr_new(TT_FNC); e->sval = strdup("LLE");
                                  expr_add_child(e, (yyvsp[-2].expr)); expr_add_child(e, (yyvsp[0].expr)); (yyval.expr) = e; }
#line 2128 "snocone_parse.tab.c"
    break;

  case 90: /* expr5: expr5 T_LGE expr6  */
#line 480 "snocone_parse.y"
                                { tree_t *e = expr_new(TT_FNC); e->sval = strdup("LGE");
                                  expr_add_child(e, (yyvsp[-2].expr)); expr_add_child(e, (yyvsp[0].expr)); (yyval.expr) = e; }
#line 2135 "snocone_parse.tab.c"
    break;

  case 91: /* expr5: expr5 T_IDENT_OP expr6  */
#line 483 "snocone_parse.y"
                                { tree_t *e = expr_new(TT_FNC); e->sval = strdup("IDENT");
                                  expr_add_child(e, (yyvsp[-2].expr)); expr_add_child(e, (yyvsp[0].expr)); (yyval.expr) = e; }
#line 2142 "snocone_parse.tab.c"
    break;

  case 92: /* expr5: expr5 T_DIFFER expr6  */
#line 486 "snocone_parse.y"
                                { tree_t *e = expr_new(TT_FNC); e->sval = strdup("DIFFER");
                                  expr_add_child(e, (yyvsp[-2].expr)); expr_add_child(e, (yyvsp[0].expr)); (yyval.expr) = e; }
#line 2149 "snocone_parse.tab.c"
    break;

  case 93: /* expr5: expr6  */
#line 489 "snocone_parse.y"
                                { (yyval.expr) = (yyvsp[0].expr); }
#line 2155 "snocone_parse.tab.c"
    break;

  case 94: /* expr6: expr6 T_2PLUS expr9  */
#line 492 "snocone_parse.y"
                                { (yyval.expr) = sc_flatten_arith(TT_ADD, (yyvsp[-2].expr), (yyvsp[0].expr)); }
#line 2161 "snocone_parse.tab.c"
    break;

  case 95: /* expr6: expr6 T_2MINUS expr9  */
#line 494 "snocone_parse.y"
                                { (yyval.expr) = sc_flatten_arith(TT_SUB, (yyvsp[-2].expr), (yyvsp[0].expr)); }
#line 2167 "snocone_parse.tab.c"
    break;

  case 96: /* expr6: expr9  */
#line 496 "snocone_parse.y"
                                { (yyval.expr) = (yyvsp[0].expr); }
#line 2173 "snocone_parse.tab.c"
    break;

  case 97: /* expr9: expr9 T_2STAR expr11  */
#line 499 "snocone_parse.y"
                                { (yyval.expr) = sc_flatten_arith(TT_MUL, (yyvsp[-2].expr), (yyvsp[0].expr)); }
#line 2179 "snocone_parse.tab.c"
    break;

  case 98: /* expr9: expr9 T_2SLASH expr11  */
#line 501 "snocone_parse.y"
                                { (yyval.expr) = sc_flatten_arith(TT_DIV, (yyvsp[-2].expr), (yyvsp[0].expr)); }
#line 2185 "snocone_parse.tab.c"
    break;

  case 99: /* expr9: expr11  */
#line 503 "snocone_parse.y"
                                { (yyval.expr) = (yyvsp[0].expr); }
#line 2191 "snocone_parse.tab.c"
    break;

  case 100: /* expr11: expr12 T_2CARET expr11  */
#line 506 "snocone_parse.y"
                                { (yyval.expr) = expr_binary(TT_POW, (yyvsp[-2].expr), (yyvsp[0].expr)); }
#line 2197 "snocone_parse.tab.c"
    break;

  case 101: /* expr11: expr12  */
#line 508 "snocone_parse.y"
                                { (yyval.expr) = (yyvsp[0].expr); }
#line 2203 "snocone_parse.tab.c"
    break;

  case 102: /* expr12: expr12 T_2DOLLAR expr15  */
#line 511 "snocone_parse.y"
                                { (yyval.expr) = expr_binary(TT_CAPT_IMMED_ASGN, (yyvsp[-2].expr), (yyvsp[0].expr)); }
#line 2209 "snocone_parse.tab.c"
    break;

  case 103: /* expr12: expr12 T_2DOT expr15  */
#line 513 "snocone_parse.y"
                                { (yyval.expr) = expr_binary(TT_CAPT_COND_ASGN,  (yyvsp[-2].expr), (yyvsp[0].expr)); }
#line 2215 "snocone_parse.tab.c"
    break;

  case 104: /* expr12: expr15  */
#line 515 "snocone_parse.y"
                                { (yyval.expr) = (yyvsp[0].expr); }
#line 2221 "snocone_parse.tab.c"
    break;

  case 105: /* expr15: expr15 T_LBRACK exprlist T_RBRACK  */
#line 518 "snocone_parse.y"
                                { tree_t *idx = expr_new(TT_IDX);
                                  expr_add_child(idx, (yyvsp[-3].expr));
                                  for (int i = 0; i < (yyvsp[-1].expr)->nchildren; i++)
                                      expr_add_child(idx, (yyvsp[-1].expr)->children[i]);
                                  free((yyvsp[-1].expr)->children); free((yyvsp[-1].expr));
                                  (yyval.expr) = idx; }
#line 2232 "snocone_parse.tab.c"
    break;

  case 106: /* expr15: expr17  */
#line 525 "snocone_parse.y"
                                { (yyval.expr) = (yyvsp[0].expr); }
#line 2238 "snocone_parse.tab.c"
    break;

  case 107: /* exprlist: exprlist_ne  */
#line 528 "snocone_parse.y"
                                { (yyval.expr) = (yyvsp[0].expr); }
#line 2244 "snocone_parse.tab.c"
    break;

  case 108: /* exprlist: %empty  */
#line 530 "snocone_parse.y"
                                { (yyval.expr) = expr_new(TT_NUL); }
#line 2250 "snocone_parse.tab.c"
    break;

  case 109: /* exprlist_ne: exprlist_ne T_COMMA expr0  */
#line 533 "snocone_parse.y"
                                { expr_add_child((yyvsp[-2].expr), (yyvsp[0].expr)); (yyval.expr) = (yyvsp[-2].expr); }
#line 2256 "snocone_parse.tab.c"
    break;

  case 110: /* exprlist_ne: expr0  */
#line 535 "snocone_parse.y"
                                { tree_t *l = expr_new(TT_NUL); expr_add_child(l, (yyvsp[0].expr)); (yyval.expr) = l; }
#line 2262 "snocone_parse.tab.c"
    break;

  case 111: /* expr17: T_CALL exprlist T_RPAREN  */
#line 538 "snocone_parse.y"
                                { tree_t *e = expr_new(TT_FNC);
                                  e->sval = (yyvsp[-2].str);
                                  for (int i = 0; i < (yyvsp[-1].expr)->nchildren; i++)
                                      expr_add_child(e, (yyvsp[-1].expr)->children[i]);
                                  free((yyvsp[-1].expr)->children); free((yyvsp[-1].expr));
                                  (yyval.expr) = e; }
#line 2273 "snocone_parse.tab.c"
    break;

  case 112: /* expr17: T_IDENT  */
#line 545 "snocone_parse.y"
                                { tree_t *e = expr_new(TT_VAR);
                                  e->sval = (yyvsp[0].str);
                                  (yyval.expr) = e; }
#line 2281 "snocone_parse.tab.c"
    break;

  case 113: /* expr17: T_KEYWORD  */
#line 549 "snocone_parse.y"
                                { tree_t *e = expr_new(TT_KEYWORD);
                                  e->sval = (yyvsp[0].str);
                                  (yyval.expr) = e; }
#line 2289 "snocone_parse.tab.c"
    break;

  case 114: /* expr17: T_INT  */
#line 553 "snocone_parse.y"
                                { (yyval.expr) = sc_int_literal((yyvsp[0].str)); free((yyvsp[0].str)); }
#line 2295 "snocone_parse.tab.c"
    break;

  case 115: /* expr17: T_REAL  */
#line 555 "snocone_parse.y"
                                { (yyval.expr) = sc_real_literal((yyvsp[0].str)); free((yyvsp[0].str)); }
#line 2301 "snocone_parse.tab.c"
    break;

  case 116: /* expr17: T_STR  */
#line 557 "snocone_parse.y"
                                { (yyval.expr) = sc_str_literal((yyvsp[0].str)); free((yyvsp[0].str)); }
#line 2307 "snocone_parse.tab.c"
    break;

  case 117: /* expr17: T_LPAREN expr0 T_RPAREN  */
#line 559 "snocone_parse.y"
                                { (yyval.expr) = (yyvsp[-1].expr); }
#line 2313 "snocone_parse.tab.c"
    break;

  case 118: /* expr17: T_LPAREN expr0 T_COMMA exprlist_ne T_RPAREN  */
#line 561 "snocone_parse.y"
                                { tree_t *a = expr_new(TT_VLIST);
                                  expr_add_child(a, (yyvsp[-3].expr));
                                  for (int i = 0; i < (yyvsp[-1].expr)->nchildren; i++)
                                      expr_add_child(a, (yyvsp[-1].expr)->children[i]);
                                  free((yyvsp[-1].expr)->children); free((yyvsp[-1].expr));
                                  (yyval.expr) = a; }
#line 2324 "snocone_parse.tab.c"
    break;

  case 119: /* expr17: T_LPAREN T_RPAREN  */
#line 568 "snocone_parse.y"
                                { (yyval.expr) = expr_new(TT_NUL); }
#line 2330 "snocone_parse.tab.c"
    break;

  case 120: /* expr17: T_1PLUS expr17  */
#line 570 "snocone_parse.y"
                                { (yyval.expr) = expr_unary(TT_PLS, (yyvsp[0].expr)); }
#line 2336 "snocone_parse.tab.c"
    break;

  case 121: /* expr17: T_1MINUS expr17  */
#line 572 "snocone_parse.y"
                                { (yyval.expr) = expr_unary(TT_MNS, (yyvsp[0].expr)); }
#line 2342 "snocone_parse.tab.c"
    break;

  case 122: /* expr17: T_1STAR expr17  */
#line 573 "snocone_parse.y"
                                { (yyval.expr) = expr_unary(TT_DEFER,       (yyvsp[0].expr)); }
#line 2348 "snocone_parse.tab.c"
    break;

  case 123: /* expr17: T_1DOT expr17  */
#line 574 "snocone_parse.y"
                                { (yyval.expr) = expr_unary(TT_NAME,        (yyvsp[0].expr)); }
#line 2354 "snocone_parse.tab.c"
    break;

  case 124: /* expr17: T_1DOLLAR expr17  */
#line 575 "snocone_parse.y"
                                { (yyval.expr) = expr_unary(TT_INDIRECT,    (yyvsp[0].expr)); }
#line 2360 "snocone_parse.tab.c"
    break;

  case 125: /* expr17: T_1AT expr17  */
#line 576 "snocone_parse.y"
                                { (yyval.expr) = expr_unary(TT_CAPT_CURSOR, (yyvsp[0].expr)); }
#line 2366 "snocone_parse.tab.c"
    break;

  case 126: /* expr17: T_1TILDE expr17  */
#line 577 "snocone_parse.y"
                                { (yyval.expr) = expr_unary(TT_NOT,         (yyvsp[0].expr)); }
#line 2372 "snocone_parse.tab.c"
    break;

  case 127: /* expr17: T_1QUEST expr17  */
#line 578 "snocone_parse.y"
                                { (yyval.expr) = expr_unary(TT_INTERROGATE, (yyvsp[0].expr)); }
#line 2378 "snocone_parse.tab.c"
    break;

  case 128: /* expr17: T_1AMP expr17  */
#line 579 "snocone_parse.y"
                                { tree_t *_e = expr_unary(TT_OPSYN, (yyvsp[0].expr));
                                  _e->sval = strdup("&"); (yyval.expr) = _e; }
#line 2385 "snocone_parse.tab.c"
    break;

  case 129: /* expr17: T_1PERCENT expr17  */
#line 581 "snocone_parse.y"
                                { tree_t *_e = expr_unary(TT_OPSYN, (yyvsp[0].expr));
                                  _e->sval = strdup("%"); (yyval.expr) = _e; }
#line 2392 "snocone_parse.tab.c"
    break;

  case 130: /* expr17: T_1SLASH expr17  */
#line 583 "snocone_parse.y"
                                { tree_t *_e = expr_unary(TT_OPSYN, (yyvsp[0].expr));
                                  _e->sval = strdup("/"); (yyval.expr) = _e; }
#line 2399 "snocone_parse.tab.c"
    break;

  case 131: /* expr17: T_1POUND expr17  */
#line 585 "snocone_parse.y"
                                { tree_t *_e = expr_unary(TT_OPSYN, (yyvsp[0].expr));
                                  _e->sval = strdup("#"); (yyval.expr) = _e; }
#line 2406 "snocone_parse.tab.c"
    break;

  case 132: /* expr17: T_1PIPE expr17  */
#line 587 "snocone_parse.y"
                                { tree_t *_e = expr_unary(TT_OPSYN, (yyvsp[0].expr));
                                  _e->sval = strdup("|"); (yyval.expr) = _e; }
#line 2413 "snocone_parse.tab.c"
    break;

  case 133: /* expr17: T_1EQUAL expr17  */
#line 589 "snocone_parse.y"
                                { tree_t *_e = expr_unary(TT_OPSYN, (yyvsp[0].expr));
                                  _e->sval = strdup("="); (yyval.expr) = _e; }
#line 2420 "snocone_parse.tab.c"
    break;

  case 134: /* expr17: T_1BANG expr17  */
#line 591 "snocone_parse.y"
                                { tree_t *_e = expr_unary(TT_OPSYN, (yyvsp[0].expr));
                                  _e->sval = strdup("!"); (yyval.expr) = _e; }
#line 2427 "snocone_parse.tab.c"
    break;


#line 2431 "snocone_parse.tab.c"

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

#line 594 "snocone_parse.y"

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
    if (subj->kind == TT_SCAN && subj->nchildren == 2) {
        tree_t *new_subj = subj->children[0];
        tree_t *new_pat  = subj->children[1];
        free(subj->children);
        free(subj);
        *subj_io = new_subj;
        *pat_io  = new_pat;
        return;
    }
    if (subj->kind == TT_SEQ && subj->nchildren >= 2) {
        tree_t *first = subj->children[0];
        if (first->kind == TT_VAR || first->kind == TT_KEYWORD ||
            first->kind == TT_QLIT || first->kind == TT_INDIRECT) {
            int nc = subj->nchildren - 1;
            tree_t *rest;
            if (nc == 1) {
                rest = subj->children[1];
            } else {
                rest = expr_new(TT_SEQ);
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
    STMT_t *s = stmt_new();
    s->lineno = st->ctx ? st->ctx->line : 0;
    s->stno   = ++st->code->nstmts;
    if (top->kind == TT_ASSIGN && top->nchildren == 2) {
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
    tree_t *e = expr_new(TT_ILIT);
    e->ival = strtol(txt, NULL, 10);
    return e;
}
/*----------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------*/
static tree_t *sc_real_literal(const char *txt) {
    tree_t *e = expr_new(TT_FLIT);
    e->dval = strtod(txt, NULL);
    return e;
}
/*----------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------*/
static tree_t *sc_str_literal(const char *txt) {
    tree_t *e = expr_new(TT_QLIT);
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
static struct ForHead *sc_for_head_new_pst(ScParseState *st, tree_t *cond, tree_t *step, STMT_t *before_body) {
    (void)st;
    struct ForHead *h = calloc(1, sizeof *h);
    h->cond        = cond;
    h->step        = step;
    h->before_body = before_body;
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
    if (cond && cond->kind == TT_ASSIGN && cond->nchildren == 2) {
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
/* PST-SC-4i (2026-05-16): label: → STMT_t with label field only; stmt_to_ast produces TT_STMT(:lbl).
 * No sc_pending_label_clear; no subject/pattern split. Direct append to CODE chain. */
static void sc_append_label_node(ScParseState *st, const char *name) {
    STMT_t *s = stmt_new();
    s->lineno = st->ctx ? st->ctx->line : 0;
    s->stno   = ++st->code->nstmts;
    s->label  = strdup(name);
    sc_append_chain(st, s, s);
}
/*----------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------*/
static void sc_append_goto_label(ScParseState *st, char *target) {
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
 * `h` carries the per-if cond + before_body snapshot (heap-allocated by sc_if_head_new),
 * so nested ifs each carry their own snapshot — fixes use-after-free that occurred when
 * a shared ScParseState field was overwritten by an inner if's reduction. */
static void sc_finalize_if_no_else_pst(ScParseState *st, struct IfHead *h)
{
    tree_t *then_block = sc_collect_body(st, h->before_body);
    tree_t *if_node    = ast_node_new(TT_IF);
    ast_push(if_node, h->cond);
    ast_push(if_node, then_block);
    /* wrap TT_IF in a STMT_t so it reaches lower() via the normal CODE_t → TT_PROGRAM path */
    sc_append_stmt(st, if_node);
    free(h);
}
/*----------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------*/
/* PST-SC-4b: Emit TT_IF(cond, then_block, else_block) as a single statement.
 * `h` carries the per-if cond + before_body snapshot (heap-allocated by sc_if_head_new).
 * `before_else` is the CODE_t tail snapshot taken at the T_ELSE token.
 *
 * SL-2 FIX (2026-05-17): previously `sc_finalize_if_else_pst` read `st->if_before_body`
 * here.  That field is a single slot on the parse state and is overwritten by every
 * nested `if_head` reduction, so with three or more chained `else if` clauses the
 * outer finalization read a stale snapshot whose STMT_t had been freed by the inner
 * `sc_collect_body` call — a heap-use-after-free in `sc_collect_body` line 840.
 * The per-if snapshot now lives in `IfHead`, so nesting cannot clobber it. */
static void sc_finalize_if_else_pst(ScParseState *st, struct IfHead *h, STMT_t *before_else)
{
    tree_t *else_block = sc_collect_body(st, before_else);
    tree_t *then_block = sc_collect_body(st, h->before_body);
    tree_t *if_node    = ast_node_new(TT_IF);
    ast_push(if_node, h->cond);
    ast_push(if_node, then_block);
    ast_push(if_node, else_block);
    sc_append_stmt(st, if_node);
    free(h);
}
/*----------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------*/
/* PST-SC-4c (2026-05-16): pure-syntax while finalizer.
 * Builds TT_WHILE(cond, TT_PROGRAM(body), TT_QLIT(cont_lbl), TT_QLIT(end_lbl)).
 * Label strings are stored as QLIT children so lower.c can call labtab_define
 * at the right instruction positions without emitting SM_LABEL instructions.
 * sc_loop_push was called in while_head; sc_loop_pop resolves break/continue here. */
static void sc_finalize_while_pst(ScParseState *st, struct WhileHead *h, tree_t *cond)
{
    LoopFrame *lf     = st->loop_top;
    char      *Ltop   = lf ? strdup(lf->cont_label) : sc_label_new(st, "_Ltop");
    char      *Lend   = lf ? strdup(lf->end_label)  : sc_label_new(st, "_Lend");
    tree_t    *body   = sc_collect_body(st, h->before_body);
    tree_t    *qlit_c = ast_node_new(TT_QLIT); qlit_c->sval = Ltop;
    tree_t    *qlit_e = ast_node_new(TT_QLIT); qlit_e->sval = Lend;
    tree_t    *w      = ast_node_new(TT_WHILE);
    ast_push(w, cond);
    ast_push(w, body);
    ast_push(w, qlit_c);
    ast_push(w, qlit_e);
    sc_loop_pop(st);
    free(h);
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
static void sc_finalize_do_while_pst(ScParseState *st, struct DoHead *h, tree_t *cond)
{
    LoopFrame *lf     = st->loop_top;
    char      *Lcont  = lf ? strdup(lf->cont_label) : sc_label_new(st, "_Lcont");
    char      *Lend   = lf ? strdup(lf->end_label)  : sc_label_new(st, "_Lend");
    tree_t    *body   = sc_collect_body(st, h->before_body);
    tree_t    *qlit_c = ast_node_new(TT_QLIT); qlit_c->sval = Lcont;
    tree_t    *qlit_e = ast_node_new(TT_QLIT); qlit_e->sval = Lend;
    tree_t    *dw     = ast_node_new(TT_DO_WHILE);
    ast_push(dw, body);
    ast_push(dw, cond);
    ast_push(dw, qlit_c);
    ast_push(dw, qlit_e);
    sc_loop_pop(st);
    free(h);
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
    tree_t    *body   = sc_collect_body(st, h->before_body);
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
/*----------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------
 * sc_flatten_arith — mirror of parser_snocone.sc's flatten_arith().
 * When building (op left right) and left is already the same op, absorb its
 * children rather than building a binary node.  E.g. a+b+c+d becomes
 * (TT_ADD a b c d) not (TT_ADD (TT_ADD (TT_ADD a b) c) d).
 * Only TT_ADD/TT_SUB/TT_MUL/TT_DIV are flattened (matches sc_flatten_ops).
 *----------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------*/
static tree_t *sc_flatten_arith(tree_e op, tree_t *left, tree_t *right) {
    if (left && left->t == op && left->n >= 2) {
        /* Absorb right into left in-place */
        ast_push(left, right);
        return left;
    }
    return expr_binary(op, left, right);
}
/*----------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------*/
static void sc_loop_push(ScParseState *st, char *cont_label, char *end_label, int is_loop) {
    LoopFrame *f = calloc(1, sizeof *f);
    f->cont_label = cont_label;
    f->end_label  = end_label;
    f->is_loop    = is_loop;
    f->outer      = st->loop_top;
    st->loop_top  = f;
}
/*----------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------*/
static void sc_loop_pop(ScParseState *st) {
    LoopFrame *f = st->loop_top;
    if (!f) return;
    st->loop_top = f->outer;
    free(f->cont_label);
    free(f->end_label);
    free(f);
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
/* PST-SC-4h (2026-05-16): emit TT_LOOP_BREAK([QLIT(user_label)]) — no goto STMT_t.
 * lower.c resolves the target via g_loop_stack at lower time. */
static void sc_append_break(ScParseState *st, char *user_label) {
    if (!st->loop_top) {
        sc_error(st, user_label ? "break: no enclosing loop or switch" : "break outside of loop or switch");
        return;
    }
    tree_t *brk = ast_node_new(TT_LOOP_BREAK);
    if (user_label) {
        tree_t *q = ast_node_new(TT_QLIT); q->sval = strdup(user_label);
        ast_push(brk, q);
    }
    sc_append_stmt(st, brk);
}
/*----------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------*/
/* PST-SC-4h (2026-05-16): emit TT_LOOP_NEXT([QLIT(user_label)]) — no goto STMT_t. */
static void sc_append_continue(ScParseState *st, char *user_label) {
    if (!st->loop_top) {
        sc_error(st, user_label ? "continue: no enclosing loop" : "continue outside of loop");
        return;
    }
    tree_t *nxt = ast_node_new(TT_LOOP_NEXT);
    if (user_label) {
        tree_t *q = ast_node_new(TT_QLIT); q->sval = strdup(user_label);
        ast_push(nxt, q);
    }
    sc_append_stmt(st, nxt);
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
    sc_loop_push(st, strdup(h->end_label), strdup(h->end_label), 0);
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
    tree_t *qarg = expr_new(TT_QLIT);
    qarg->sval   = spec;
    tree_t *data_call = expr_new(TT_FNC);
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
    while (state.loop_top) sc_loop_pop(&state);
    if (rc != 0 || state.nerrors > 0) {
        free(state.code);
        return NULL;
    }
    return state.code;
}
