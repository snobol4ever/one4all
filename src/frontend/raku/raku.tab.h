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

#ifndef YY_RAKU_YY_RAKU_TAB_H_INCLUDED
# define YY_RAKU_YY_RAKU_TAB_H_INCLUDED
/* Debug traces.  */
#ifndef RAKU_YYDEBUG
# if defined YYDEBUG
#if YYDEBUG
#   define RAKU_YYDEBUG 1
#  else
#   define RAKU_YYDEBUG 0
#  endif
# else /* ! defined YYDEBUG */
#  define RAKU_YYDEBUG 0
# endif /* ! defined YYDEBUG */
#endif  /* ! defined RAKU_YYDEBUG */
#if RAKU_YYDEBUG
extern int raku_yydebug;
#endif
/* "%code requires" blocks.  */
#line 3 "raku.y"

/*
 * raku.y — Tiny-Raku Bison grammar
 *
 * FI-3: builds EXPR_t/STMT_t directly — no intermediate RakuNode AST.
 *
 * AUTHORS: Lon Jones Cherryholmes · Claude Sonnet 4.6
 */
#include "../../ir/ir.h"
#include "../snobol4/scrip_cc.h"

typedef struct ExprList {
    EXPR_t **items;
    int      count;
    int      cap;
} ExprList;

#line 75 "raku.tab.h"

/* Token kinds.  */
#ifndef RAKU_YYTOKENTYPE
# define RAKU_YYTOKENTYPE
  enum raku_yytokentype
  {
    RAKU_YYEMPTY = -2,
    RAKU_YYEOF = 0,                /* "end of file"  */
    RAKU_YYerror = 256,            /* error  */
    RAKU_YYUNDEF = 257,            /* "invalid token"  */
    LIT_INT = 258,                 /* LIT_INT  */
    LIT_FLOAT = 259,               /* LIT_FLOAT  */
    LIT_STR = 260,                 /* LIT_STR  */
    LIT_INTERP_STR = 261,          /* LIT_INTERP_STR  */
    LIT_REGEX = 262,               /* LIT_REGEX  */
    VAR_SCALAR = 263,              /* VAR_SCALAR  */
    VAR_ARRAY = 264,               /* VAR_ARRAY  */
    VAR_HASH = 265,                /* VAR_HASH  */
    IDENT = 266,                   /* IDENT  */
    KW_MY = 267,                   /* KW_MY  */
    KW_SAY = 268,                  /* KW_SAY  */
    KW_PRINT = 269,                /* KW_PRINT  */
    KW_IF = 270,                   /* KW_IF  */
    KW_ELSE = 271,                 /* KW_ELSE  */
    KW_ELSIF = 272,                /* KW_ELSIF  */
    KW_WHILE = 273,                /* KW_WHILE  */
    KW_FOR = 274,                  /* KW_FOR  */
    KW_SUB = 275,                  /* KW_SUB  */
    KW_GATHER = 276,               /* KW_GATHER  */
    KW_TAKE = 277,                 /* KW_TAKE  */
    KW_RETURN = 278,               /* KW_RETURN  */
    KW_GIVEN = 279,                /* KW_GIVEN  */
    KW_WHEN = 280,                 /* KW_WHEN  */
    KW_DEFAULT = 281,              /* KW_DEFAULT  */
    KW_EXISTS = 282,               /* KW_EXISTS  */
    KW_DELETE = 283,               /* KW_DELETE  */
    KW_UNLESS = 284,               /* KW_UNLESS  */
    KW_UNTIL = 285,                /* KW_UNTIL  */
    KW_REPEAT = 286,               /* KW_REPEAT  */
    OP_RANGE = 287,                /* OP_RANGE  */
    OP_RANGE_EX = 288,             /* OP_RANGE_EX  */
    OP_ARROW = 289,                /* OP_ARROW  */
    OP_EQ = 290,                   /* OP_EQ  */
    OP_NE = 291,                   /* OP_NE  */
    OP_LE = 292,                   /* OP_LE  */
    OP_GE = 293,                   /* OP_GE  */
    OP_SEQ = 294,                  /* OP_SEQ  */
    OP_SNE = 295,                  /* OP_SNE  */
    OP_AND = 296,                  /* OP_AND  */
    OP_OR = 297,                   /* OP_OR  */
    OP_BIND = 298,                 /* OP_BIND  */
    OP_SMATCH = 299,               /* OP_SMATCH  */
    OP_DIV = 300,                  /* OP_DIV  */
    UMINUS = 301                   /* UMINUS  */
  };
  typedef enum raku_yytokentype raku_yytoken_kind_t;
#endif

/* Value type.  */
#if ! defined RAKU_YYSTYPE && ! defined RAKU_YYSTYPE_IS_DECLARED
union RAKU_YYSTYPE
{
#line 141 "raku.y"

    long      ival;
    double    dval;
    char     *sval;
    EXPR_t   *node;
    ExprList *list;

#line 146 "raku.tab.h"

};
typedef union RAKU_YYSTYPE RAKU_YYSTYPE;
# define RAKU_YYSTYPE_IS_TRIVIAL 1
# define RAKU_YYSTYPE_IS_DECLARED 1
#endif


extern RAKU_YYSTYPE raku_yylval;


int raku_yyparse (void);


#endif /* !YY_RAKU_YY_RAKU_TAB_H_INCLUDED  */
