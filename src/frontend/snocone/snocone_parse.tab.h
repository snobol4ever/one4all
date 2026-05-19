/* A Bison parser, made by GNU Bison 3.8.2.  */

/* Bison interface for Yacc-like parsers in C

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

/* DO NOT RELY ON FEATURES THAT ARE NOT DOCUMENTED in the manual,
   especially those whose name start with YY_ or yy_.  They are
   private implementation details that can be changed or removed.  */

#ifndef YY_SC_SNOCONE_PARSE_TAB_H_INCLUDED
# define YY_SC_SNOCONE_PARSE_TAB_H_INCLUDED
/* Debug traces.  */
#ifndef SC_DEBUG
# if defined YYDEBUG
#if YYDEBUG
#   define SC_DEBUG 1
#  else
#   define SC_DEBUG 0
#  endif
# else /* ! defined YYDEBUG */
#  define SC_DEBUG 0
# endif /* ! defined YYDEBUG */
#endif  /* ! defined SC_DEBUG */
#if SC_DEBUG
extern int sc_debug;
#endif
/* "%code requires" blocks.  */
#line 3 "snocone_parse.y"

#include "scrip_cc.h"
struct LexCtx;
struct IfHead;
struct WhileHead;
struct DoHead;
struct FuncHead;
typedef struct LoopFrame {
    char    *cont_label;
    char    *end_label;
    int      is_loop;
    int      cont_used;
    struct LoopFrame *outer;
} LoopFrame;
typedef struct ScParseState {
    struct LexCtx *ctx;
    CODE_t        *code;
    const char    *filename;
    int            nerrors;
    char          *cur_func_name;
    LoopFrame    *loop_top;
    struct SwitchHead *cur_switch;
} ScParseState;

#line 82 "snocone_parse.tab.h"

/* Token kinds.  */
#ifndef SC_TOKENTYPE
# define SC_TOKENTYPE
  enum sc_tokentype
  {
    SC_EMPTY = -2,
    SC_EOF = 0,                    /* "end of file"  */
    SC_error = 256,                /* error  */
    SC_UNDEF = 257,                /* "invalid token"  */
    T_IDENT = 258,                 /* T_IDENT  */
    T_KEYWORD = 259,               /* T_KEYWORD  */
    T_INT = 260,                   /* T_INT  */
    T_REAL = 261,                  /* T_REAL  */
    T_STR = 262,                   /* T_STR  */
    T_CALL = 263,                  /* T_CALL  */
    T_2PLUS = 264,                 /* T_2PLUS  */
    T_2MINUS = 265,                /* T_2MINUS  */
    T_2STAR = 266,                 /* T_2STAR  */
    T_2SLASH = 267,                /* T_2SLASH  */
    T_2CARET = 268,                /* T_2CARET  */
    T_EQ = 269,                    /* T_EQ  */
    T_NE = 270,                    /* T_NE  */
    T_LT = 271,                    /* T_LT  */
    T_GT = 272,                    /* T_GT  */
    T_LE = 273,                    /* T_LE  */
    T_GE = 274,                    /* T_GE  */
    T_LEQ = 275,                   /* T_LEQ  */
    T_LNE = 276,                   /* T_LNE  */
    T_LLT = 277,                   /* T_LLT  */
    T_LGT = 278,                   /* T_LGT  */
    T_LLE = 279,                   /* T_LLE  */
    T_LGE = 280,                   /* T_LGE  */
    T_IDENT_OP = 281,              /* T_IDENT_OP  */
    T_DIFFER = 282,                /* T_DIFFER  */
    T_1PLUS = 283,                 /* T_1PLUS  */
    T_1MINUS = 284,                /* T_1MINUS  */
    T_2EQUAL = 285,                /* T_2EQUAL  */
    T_PLUS_ASSIGN = 286,           /* T_PLUS_ASSIGN  */
    T_MINUS_ASSIGN = 287,          /* T_MINUS_ASSIGN  */
    T_STAR_ASSIGN = 288,           /* T_STAR_ASSIGN  */
    T_SLASH_ASSIGN = 289,          /* T_SLASH_ASSIGN  */
    T_CARET_ASSIGN = 290,          /* T_CARET_ASSIGN  */
    T_2QUEST = 291,                /* T_2QUEST  */
    T_2PIPE = 292,                 /* T_2PIPE  */
    T_CONCAT = 293,                /* T_CONCAT  */
    T_LPAREN = 294,                /* T_LPAREN  */
    T_RPAREN = 295,                /* T_RPAREN  */
    T_SEMICOLON = 296,             /* T_SEMICOLON  */
    T_COMMA = 297,                 /* T_COMMA  */
    T_LBRACK = 298,                /* T_LBRACK  */
    T_RBRACK = 299,                /* T_RBRACK  */
    T_2DOLLAR = 300,               /* T_2DOLLAR  */
    T_2DOT = 301,                  /* T_2DOT  */
    T_2AMP = 302,                  /* T_2AMP  */
    T_2AT = 303,                   /* T_2AT  */
    T_2POUND = 304,                /* T_2POUND  */
    T_2PERCENT = 305,              /* T_2PERCENT  */
    T_2TILDE = 306,                /* T_2TILDE  */
    T_1STAR = 307,                 /* T_1STAR  */
    T_1SLASH = 308,                /* T_1SLASH  */
    T_1PERCENT = 309,              /* T_1PERCENT  */
    T_1AT = 310,                   /* T_1AT  */
    T_1TILDE = 311,                /* T_1TILDE  */
    T_1DOLLAR = 312,               /* T_1DOLLAR  */
    T_1DOT = 313,                  /* T_1DOT  */
    T_1POUND = 314,                /* T_1POUND  */
    T_1PIPE = 315,                 /* T_1PIPE  */
    T_1EQUAL = 316,                /* T_1EQUAL  */
    T_1QUEST = 317,                /* T_1QUEST  */
    T_1AMP = 318,                  /* T_1AMP  */
    T_1BANG = 319,                 /* T_1BANG  */
    T_COLON = 320,                 /* T_COLON  */
    T_DO = 321,                    /* T_DO  */
    T_FOR = 322,                   /* T_FOR  */
    T_SWITCH = 323,                /* T_SWITCH  */
    T_CASE = 324,                  /* T_CASE  */
    T_DEFAULT = 325,               /* T_DEFAULT  */
    T_BREAK = 326,                 /* T_BREAK  */
    T_CONTINUE = 327,              /* T_CONTINUE  */
    T_GOTO = 328,                  /* T_GOTO  */
    T_DEFINE = 329,                /* T_DEFINE  */
    T_RETURN = 330,                /* T_RETURN  */
    T_FRETURN = 331,               /* T_FRETURN  */
    T_NRETURN = 332,               /* T_NRETURN  */
    T_STRUCT = 333,                /* T_STRUCT  */
    T_UNKNOWN = 334,               /* T_UNKNOWN  */
    T_LBRACE = 335,                /* T_LBRACE  */
    T_RBRACE = 336,                /* T_RBRACE  */
    T_IF = 337,                    /* T_IF  */
    T_ELSE = 338,                  /* T_ELSE  */
    T_WHILE = 339                  /* T_WHILE  */
  };
  typedef enum sc_tokentype sc_token_kind_t;
#endif

/* Value type.  */
#if ! defined SC_STYPE && ! defined SC_STYPE_IS_DECLARED
union SC_STYPE
{
#line 185 "snocone_parse.y"

    tree_t *expr;
    char   *str;
    long    ival;
    double  dval;
    struct IfHead    *ifhead;
    struct WhileHead *whilehead;
    struct DoHead    *dohead;
    struct ForHead   *forhead;
    struct FuncHead  *funchead;
    struct SwitchHead *switchhead;
    STMT_t           *stmt_ptr;

#line 197 "snocone_parse.tab.h"

};
typedef union SC_STYPE SC_STYPE;
# define SC_STYPE_IS_TRIVIAL 1
# define SC_STYPE_IS_DECLARED 1
#endif




int sc_parse (ScParseState *st);


#endif /* !YY_SC_SNOCONE_PARSE_TAB_H_INCLUDED  */
