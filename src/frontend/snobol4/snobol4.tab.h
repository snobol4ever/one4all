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

#ifndef YY_SNOBOL4_HOME_CLAUDE_ONE4ALL_SRC_FRONTEND_SNOBOL4_SNOBOL4_TAB_H_INCLUDED
# define YY_SNOBOL4_HOME_CLAUDE_ONE4ALL_SRC_FRONTEND_SNOBOL4_SNOBOL4_TAB_H_INCLUDED
/* Debug traces.  */
#ifndef SNOBOL4_DEBUG
# if defined YYDEBUG
#if YYDEBUG
#   define SNOBOL4_DEBUG 1
#  else
#   define SNOBOL4_DEBUG 0
#  endif
# else /* ! defined YYDEBUG */
#  define SNOBOL4_DEBUG 0
# endif /* ! defined YYDEBUG */
#endif  /* ! defined SNOBOL4_DEBUG */
#if SNOBOL4_DEBUG
extern int snobol4_debug;
#endif
/* "%code requires" blocks.  */
#line 1 "/home/claude/one4all/src/frontend/snobol4/snobol4.y"
 #include "scrip_cc.h" #include "snobol4.h" 

#line 60 "/home/claude/one4all/src/frontend/snobol4/snobol4.tab.h"

/* Token kinds.  */
#ifndef SNOBOL4_TOKENTYPE
# define SNOBOL4_TOKENTYPE
  enum snobol4_tokentype
  {
    SNOBOL4_EMPTY = -2,
    SNOBOL4_EOF = 0,               /* "end of file"  */
    SNOBOL4_error = 256,           /* error  */
    SNOBOL4_UNDEF = 257,           /* "invalid token"  */
    TK_IDENT = 258,                /* TK_IDENT  */
    TK_FUNCTION = 259,             /* TK_FUNCTION  */
    TK_KEYWORD = 260,              /* TK_KEYWORD  */
    TK_END = 261,                  /* TK_END  */
    TK_INT = 262,                  /* TK_INT  */
    TK_REAL = 263,                 /* TK_REAL  */
    TK_STR = 264,                  /* TK_STR  */
    TK_LABEL = 265,                /* TK_LABEL  */
    TK_GOTO = 266,                 /* TK_GOTO  */
    TK_STMT_END = 267,             /* TK_STMT_END  */
    TK_ASSIGNMENT = 268,           /* TK_ASSIGNMENT  */
    TK_MATCH = 269,                /* TK_MATCH  */
    TK_ALTERNATION = 270,          /* TK_ALTERNATION  */
    TK_ADDITION = 271,             /* TK_ADDITION  */
    TK_SUBTRACTION = 272,          /* TK_SUBTRACTION  */
    TK_MULTIPLICATION = 273,       /* TK_MULTIPLICATION  */
    TK_DIVISION = 274,             /* TK_DIVISION  */
    TK_EXPONENTIATION = 275,       /* TK_EXPONENTIATION  */
    TK_IMMEDIATE_ASSIGN = 276,     /* TK_IMMEDIATE_ASSIGN  */
    TK_COND_ASSIGN = 277,          /* TK_COND_ASSIGN  */
    TK_AMPERSAND = 278,            /* TK_AMPERSAND  */
    TK_AT_SIGN = 279,              /* TK_AT_SIGN  */
    TK_POUND = 280,                /* TK_POUND  */
    TK_PERCENT = 281,              /* TK_PERCENT  */
    TK_TILDE = 282,                /* TK_TILDE  */
    TK_UN_AT_SIGN = 283,           /* TK_UN_AT_SIGN  */
    TK_UN_TILDE = 284,             /* TK_UN_TILDE  */
    TK_UN_QUESTION_MARK = 285,     /* TK_UN_QUESTION_MARK  */
    TK_UN_AMPERSAND = 286,         /* TK_UN_AMPERSAND  */
    TK_UN_PLUS = 287,              /* TK_UN_PLUS  */
    TK_UN_MINUS = 288,             /* TK_UN_MINUS  */
    TK_UN_ASTERISK = 289,          /* TK_UN_ASTERISK  */
    TK_UN_DOLLAR_SIGN = 290,       /* TK_UN_DOLLAR_SIGN  */
    TK_UN_PERIOD = 291,            /* TK_UN_PERIOD  */
    TK_UN_EXCLAMATION = 292,       /* TK_UN_EXCLAMATION  */
    TK_UN_PERCENT = 293,           /* TK_UN_PERCENT  */
    TK_UN_SLASH = 294,             /* TK_UN_SLASH  */
    TK_UN_POUND = 295,             /* TK_UN_POUND  */
    TK_UN_EQUAL = 296,             /* TK_UN_EQUAL  */
    TK_UN_VERTICAL_BAR = 297,      /* TK_UN_VERTICAL_BAR  */
    TK_CONCAT = 298,               /* TK_CONCAT  */
    TK_COMMA = 299,                /* TK_COMMA  */
    TK_LPAREN = 300,               /* TK_LPAREN  */
    TK_RPAREN = 301,               /* TK_RPAREN  */
    TK_LBRACK = 302,               /* TK_LBRACK  */
    TK_RBRACK = 303,               /* TK_RBRACK  */
    TK_LANGLE = 304,               /* TK_LANGLE  */
    TK_RANGLE = 305                /* TK_RANGLE  */
  };
  typedef enum snobol4_tokentype snobol4_token_kind_t;
#endif

/* Value type.  */
#if ! defined SNOBOL4_STYPE && ! defined SNOBOL4_STYPE_IS_DECLARED
union SNOBOL4_STYPE
{
#line 19 "/home/claude/one4all/src/frontend/snobol4/snobol4.y"
 EXPR_t *expr; Token tok; 

#line 130 "/home/claude/one4all/src/frontend/snobol4/snobol4.tab.h"

};
typedef union SNOBOL4_STYPE SNOBOL4_STYPE;
# define SNOBOL4_STYPE_IS_TRIVIAL 1
# define SNOBOL4_STYPE_IS_DECLARED 1
#endif




int snobol4_parse (void *yyparse_param);


#endif /* !YY_SNOBOL4_HOME_CLAUDE_ONE4ALL_SRC_FRONTEND_SNOBOL4_SNOBOL4_TAB_H_INCLUDED  */
