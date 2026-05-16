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
#define YYPURE 0

/* Push parsers.  */
#define YYPUSH 0

/* Pull parsers.  */
#define YYPULL 1

/* Substitute the type names.  */
#define YYSTYPE         RAKU_YYSTYPE
/* Substitute the variable and function names.  */
#define yyparse         raku_yyparse
#define yylex           raku_yylex
#define yyerror         raku_yyerror
#define yydebug         raku_yydebug
#define yynerrs         raku_yynerrs
#define yylval          raku_yylval
#define yychar          raku_yychar

/* First part of user prologue.  */
#line 11 "raku.y"

#include "../../ast/ast.h"
#include "../snobol4/scrip_cc.h"
#include "raku.tab.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
/*----------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------*/
extern int  raku_yylex(void);
/*----------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------*/
extern int  raku_get_lineno(void);
/*----------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------*/
void raku_yyerror(const char *msg) {
    fprintf(stderr, "raku parse error line %d: %s\n", raku_get_lineno(), msg);
}
/*----------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------*/
static ExprList *exprlist_new(void) {
    ExprList *l = calloc(1, sizeof *l);
    if (!l) { fprintf(stderr, "raku: OOM\n"); exit(1); }
    return l;
}
/*----------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------*/
static ExprList *exprlist_append(ExprList *l, tree_t *e) {
    if (l->count >= l->cap) {
        l->cap = l->cap ? l->cap * 2 : 8;
        l->items = realloc(l->items, l->cap * sizeof(tree_t *));
        if (!l->items) { fprintf(stderr, "raku: OOM\n"); exit(1); }
    }
    l->items[l->count++] = e;
    return l;
}
/*----------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------*/
static void exprlist_free(ExprList *l) { if (l) { free(l->items); free(l); } }
/*----------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------*/
static const char *strip_sigil(const char *s) {
    if (s && (s[0]=='$'||s[0]=='@'||s[0]=='%')) return s+1;
    return s;
}
/*----------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------*/
static tree_t *leaf_sval(tree_e k, const char *s) {
    tree_t *e = ast_node_new(k); e->v.sval = intern(s); return e;
}
/*----------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------*/
static tree_t *var_node(const char *name) {
    return leaf_sval(TT_VAR, strip_sigil(name));
}
/*----------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------*/
static tree_t *make_call(const char *name) {
    tree_t *e = leaf_sval(TT_FNC, name);
    tree_t *n = ast_node_new(TT_VAR); n->v.sval = intern(name);
    expr_add_child(e, n);
    return e;
}
/*----------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------*/
static tree_t *make_seq(ExprList *stmts) {
    tree_t *seq = ast_node_new(TT_SEQ_EXPR);
    if (stmts) {
        for (int i = 0; i < stmts->count; i++) expr_add_child(seq, stmts->items[i]);
        exprlist_free(stmts);
    }
    return seq;
}
/*----------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------*/
static tree_t *lower_interp_str(const char *s) {
    int len = s ? (int)strlen(s) : 0;
    tree_t *result = NULL;
    char litbuf[4096]; int litpos = 0, i = 0;
    while (i < len) {
        if (s[i]=='$' && i+1<len &&
            (s[i+1]=='_'||(s[i+1]>='A'&&s[i+1]<='Z')||(s[i+1]>='a'&&s[i+1]<='z'))) {
            if (litpos>0) { litbuf[litpos]='\0';
                tree_t *lit=leaf_sval(TT_QLIT,litbuf);
                result=result?expr_binary(TT_CAT,result,lit):lit; litpos=0; }
            i++;
            char vname[256]; int vlen=0;
            while (i<len&&(s[i]=='_'||(s[i]>='A'&&s[i]<='Z')||(s[i]>='a'&&s[i]<='z')||(s[i]>='0'&&s[i]<='9')))
                { if(vlen<255) vname[vlen++]=s[i]; i++; }
            vname[vlen]='\0';
            tree_t *var=leaf_sval(TT_VAR,vname);
            result=result?expr_binary(TT_CAT,result,var):var;
        } else { if(litpos<4095) litbuf[litpos++]=s[i]; i++; }
    }
    if (litpos>0) { litbuf[litpos]='\0';
        tree_t *lit=leaf_sval(TT_QLIT,litbuf);
        result=result?expr_binary(TT_CAT,result,lit):lit; }
    return result ? result : leaf_sval(TT_QLIT,"");
}
/*----------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------*/
static tree_t *make_for_range(tree_t *lo, tree_t *hi, const char *vname, tree_t *body) {
    tree_t *init = expr_binary(TT_ASSIGN, leaf_sval(TT_VAR,vname), lo);
    tree_t *cond = expr_binary(TT_LE, leaf_sval(TT_VAR,vname), hi);
    tree_t *one  = ast_node_new(TT_ILIT); one->v.ival = 1;
    tree_t *incr_rhs = expr_binary(TT_ADD, leaf_sval(TT_VAR,vname), one);
    tree_t *incr = expr_binary(TT_ASSIGN, leaf_sval(TT_VAR,vname), incr_rhs);
    tree_t *body2 = ast_node_new(TT_SEQ_EXPR);
    for (int i = 0; i < body->n; i++) expr_add_child(body2, body->c[i]);
    expr_add_child(body2, incr);
    tree_t *wloop = expr_binary(TT_WHILE, cond, body2);
    tree_t *seq   = ast_node_new(TT_SEQ_EXPR);
    expr_add_child(seq, init); expr_add_child(seq, wloop);
    return seq;
}
tree_t *raku_prog_result = NULL;
/*----------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------*/
static void add_proc(tree_t *e) {
    if (!e) return;
    if (!raku_prog_result) raku_prog_result = ast_stmt_new(TT_PROGRAM);
    tree_t *st = ast_stmt_new(TT_STMT);
    expr_add_child(st, ast_attr_int(":lang", LANG_RAKU));
    expr_add_child(st, ast_attr_int(":line", 0));
    expr_add_child(st, ast_attr_int(":stno", 0));
    expr_add_child(st, ast_attr_expr(":subj", e));
    expr_add_child(raku_prog_result, st);
}
#define SUB_TAG_ID 1
#define RAKU_METH_MAX 256
typedef struct { char key[128]; char procname[128]; } RakuMethEntry;
static RakuMethEntry raku_meth_table[RAKU_METH_MAX];
static int           raku_meth_ntypes = 0;
/*----------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------*/
static void raku_meth_register(const char *classname, const char *methname, const char *procname) {
    if (raku_meth_ntypes >= RAKU_METH_MAX) return;
    RakuMethEntry *e = &raku_meth_table[raku_meth_ntypes++];
    snprintf(e->key,      sizeof e->key,      "%s::%s", classname, methname);
    snprintf(e->procname, sizeof e->procname,  "%s",     procname);
}
/*----------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------*/
const char *raku_meth_lookup(const char *classname, const char *methname) {
    char key[128];
    snprintf(key, sizeof key, "%s::%s", classname, methname);
    for (int i = 0; i < raku_meth_ntypes; i++)
        if (strcmp(raku_meth_table[i].key, key) == 0)
            return raku_meth_table[i].procname;
    return NULL;
}

#line 216 "raku.tab.c"

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

#include "raku.tab.h"
/* Symbol kind.  */
enum yysymbol_kind_t
{
  YYSYMBOL_YYEMPTY = -2,
  YYSYMBOL_YYEOF = 0,                      /* "end of file"  */
  YYSYMBOL_YYerror = 1,                    /* error  */
  YYSYMBOL_YYUNDEF = 2,                    /* "invalid token"  */
  YYSYMBOL_LIT_INT = 3,                    /* LIT_INT  */
  YYSYMBOL_LIT_FLOAT = 4,                  /* LIT_FLOAT  */
  YYSYMBOL_LIT_STR = 5,                    /* LIT_STR  */
  YYSYMBOL_LIT_INTERP_STR = 6,             /* LIT_INTERP_STR  */
  YYSYMBOL_LIT_REGEX = 7,                  /* LIT_REGEX  */
  YYSYMBOL_LIT_MATCH_GLOBAL = 8,           /* LIT_MATCH_GLOBAL  */
  YYSYMBOL_LIT_SUBST = 9,                  /* LIT_SUBST  */
  YYSYMBOL_VAR_SCALAR = 10,                /* VAR_SCALAR  */
  YYSYMBOL_VAR_ARRAY = 11,                 /* VAR_ARRAY  */
  YYSYMBOL_VAR_HASH = 12,                  /* VAR_HASH  */
  YYSYMBOL_VAR_TWIGIL = 13,                /* VAR_TWIGIL  */
  YYSYMBOL_IDENT = 14,                     /* IDENT  */
  YYSYMBOL_VAR_CAPTURE = 15,               /* VAR_CAPTURE  */
  YYSYMBOL_VAR_NAMED_CAPTURE = 16,         /* VAR_NAMED_CAPTURE  */
  YYSYMBOL_KW_MY = 17,                     /* KW_MY  */
  YYSYMBOL_KW_SAY = 18,                    /* KW_SAY  */
  YYSYMBOL_KW_PRINT = 19,                  /* KW_PRINT  */
  YYSYMBOL_KW_IF = 20,                     /* KW_IF  */
  YYSYMBOL_KW_ELSE = 21,                   /* KW_ELSE  */
  YYSYMBOL_KW_ELSIF = 22,                  /* KW_ELSIF  */
  YYSYMBOL_KW_WHILE = 23,                  /* KW_WHILE  */
  YYSYMBOL_KW_FOR = 24,                    /* KW_FOR  */
  YYSYMBOL_KW_SUB = 25,                    /* KW_SUB  */
  YYSYMBOL_KW_GATHER = 26,                 /* KW_GATHER  */
  YYSYMBOL_KW_TAKE = 27,                   /* KW_TAKE  */
  YYSYMBOL_KW_RETURN = 28,                 /* KW_RETURN  */
  YYSYMBOL_KW_GIVEN = 29,                  /* KW_GIVEN  */
  YYSYMBOL_KW_WHEN = 30,                   /* KW_WHEN  */
  YYSYMBOL_KW_DEFAULT = 31,                /* KW_DEFAULT  */
  YYSYMBOL_KW_EXISTS = 32,                 /* KW_EXISTS  */
  YYSYMBOL_KW_DELETE = 33,                 /* KW_DELETE  */
  YYSYMBOL_KW_UNLESS = 34,                 /* KW_UNLESS  */
  YYSYMBOL_KW_UNTIL = 35,                  /* KW_UNTIL  */
  YYSYMBOL_KW_REPEAT = 36,                 /* KW_REPEAT  */
  YYSYMBOL_KW_MAP = 37,                    /* KW_MAP  */
  YYSYMBOL_KW_GREP = 38,                   /* KW_GREP  */
  YYSYMBOL_KW_SORT = 39,                   /* KW_SORT  */
  YYSYMBOL_KW_TRY = 40,                    /* KW_TRY  */
  YYSYMBOL_KW_CATCH = 41,                  /* KW_CATCH  */
  YYSYMBOL_KW_DIE = 42,                    /* KW_DIE  */
  YYSYMBOL_KW_CLASS = 43,                  /* KW_CLASS  */
  YYSYMBOL_KW_METHOD = 44,                 /* KW_METHOD  */
  YYSYMBOL_KW_HAS = 45,                    /* KW_HAS  */
  YYSYMBOL_KW_NEW = 46,                    /* KW_NEW  */
  YYSYMBOL_OP_FATARROW = 47,               /* OP_FATARROW  */
  YYSYMBOL_OP_RANGE = 48,                  /* OP_RANGE  */
  YYSYMBOL_OP_RANGE_EX = 49,               /* OP_RANGE_EX  */
  YYSYMBOL_OP_ARROW = 50,                  /* OP_ARROW  */
  YYSYMBOL_OP_EQ = 51,                     /* OP_EQ  */
  YYSYMBOL_OP_NE = 52,                     /* OP_NE  */
  YYSYMBOL_OP_LE = 53,                     /* OP_LE  */
  YYSYMBOL_OP_GE = 54,                     /* OP_GE  */
  YYSYMBOL_OP_SEQ = 55,                    /* OP_SEQ  */
  YYSYMBOL_OP_SNE = 56,                    /* OP_SNE  */
  YYSYMBOL_OP_AND = 57,                    /* OP_AND  */
  YYSYMBOL_OP_OR = 58,                     /* OP_OR  */
  YYSYMBOL_OP_BIND = 59,                   /* OP_BIND  */
  YYSYMBOL_OP_SMATCH = 60,                 /* OP_SMATCH  */
  YYSYMBOL_OP_DIV = 61,                    /* OP_DIV  */
  YYSYMBOL_62_ = 62,                       /* '='  */
  YYSYMBOL_63_ = 63,                       /* '!'  */
  YYSYMBOL_64_ = 64,                       /* '<'  */
  YYSYMBOL_65_ = 65,                       /* '>'  */
  YYSYMBOL_66_ = 66,                       /* '~'  */
  YYSYMBOL_67_ = 67,                       /* '+'  */
  YYSYMBOL_68_ = 68,                       /* '-'  */
  YYSYMBOL_69_ = 69,                       /* '*'  */
  YYSYMBOL_70_ = 70,                       /* '/'  */
  YYSYMBOL_71_ = 71,                       /* '%'  */
  YYSYMBOL_UMINUS = 72,                    /* UMINUS  */
  YYSYMBOL_73_ = 73,                       /* '.'  */
  YYSYMBOL_74_ = 74,                       /* ';'  */
  YYSYMBOL_75_ = 75,                       /* '('  */
  YYSYMBOL_76_ = 76,                       /* ','  */
  YYSYMBOL_77_ = 77,                       /* ')'  */
  YYSYMBOL_78_ = 78,                       /* '['  */
  YYSYMBOL_79_ = 79,                       /* ']'  */
  YYSYMBOL_80_ = 80,                       /* '{'  */
  YYSYMBOL_81_ = 81,                       /* '}'  */
  YYSYMBOL_YYACCEPT = 82,                  /* $accept  */
  YYSYMBOL_program = 83,                   /* program  */
  YYSYMBOL_stmt_list = 84,                 /* stmt_list  */
  YYSYMBOL_stmt = 85,                      /* stmt  */
  YYSYMBOL_if_stmt = 86,                   /* if_stmt  */
  YYSYMBOL_while_stmt = 87,                /* while_stmt  */
  YYSYMBOL_unless_stmt = 88,               /* unless_stmt  */
  YYSYMBOL_until_stmt = 89,                /* until_stmt  */
  YYSYMBOL_repeat_stmt = 90,               /* repeat_stmt  */
  YYSYMBOL_for_stmt = 91,                  /* for_stmt  */
  YYSYMBOL_given_stmt = 92,                /* given_stmt  */
  YYSYMBOL_when_list = 93,                 /* when_list  */
  YYSYMBOL_sub_decl = 94,                  /* sub_decl  */
  YYSYMBOL_class_decl = 95,                /* class_decl  */
  YYSYMBOL_class_body_list = 96,           /* class_body_list  */
  YYSYMBOL_named_arg_list = 97,            /* named_arg_list  */
  YYSYMBOL_param_list = 98,                /* param_list  */
  YYSYMBOL_block = 99,                     /* block  */
  YYSYMBOL_closure = 100,                  /* closure  */
  YYSYMBOL_expr = 101,                     /* expr  */
  YYSYMBOL_cmp_expr = 102,                 /* cmp_expr  */
  YYSYMBOL_range_expr = 103,               /* range_expr  */
  YYSYMBOL_add_expr = 104,                 /* add_expr  */
  YYSYMBOL_mul_expr = 105,                 /* mul_expr  */
  YYSYMBOL_unary_expr = 106,               /* unary_expr  */
  YYSYMBOL_postfix_expr = 107,             /* postfix_expr  */
  YYSYMBOL_call_expr = 108,                /* call_expr  */
  YYSYMBOL_arg_list = 109,                 /* arg_list  */
  YYSYMBOL_atom = 110                      /* atom  */
};
typedef enum yysymbol_kind_t yysymbol_kind_t;




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
typedef yytype_int16 yy_state_t;

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
         || (defined RAKU_YYSTYPE_IS_TRIVIAL && RAKU_YYSTYPE_IS_TRIVIAL)))

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
#define YYFINAL  3
/* YYLAST -- Last index in YYTABLE.  */
#define YYLAST   739

/* YYNTOKENS -- Number of terminals.  */
#define YYNTOKENS  82
/* YYNNTS -- Number of nonterminals.  */
#define YYNNTS  29
/* YYNRULES -- Number of rules.  */
#define YYNRULES  134
/* YYNSTATES -- Number of states.  */
#define YYNSTATES  331

/* YYMAXUTOK -- Last valid token kind.  */
#define YYMAXUTOK   317


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
       2,     2,     2,    63,     2,     2,     2,    71,     2,     2,
      75,    77,    69,    67,    76,    68,    73,    70,     2,     2,
       2,     2,     2,     2,     2,     2,     2,     2,     2,    74,
      64,    62,    65,     2,     2,     2,     2,     2,     2,     2,
       2,     2,     2,     2,     2,     2,     2,     2,     2,     2,
       2,     2,     2,     2,     2,     2,     2,     2,     2,     2,
       2,    78,     2,    79,     2,     2,     2,     2,     2,     2,
       2,     2,     2,     2,     2,     2,     2,     2,     2,     2,
       2,     2,     2,     2,     2,     2,     2,     2,     2,     2,
       2,     2,     2,    80,     2,    81,    66,     2,     2,     2,
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
      55,    56,    57,    58,    59,    60,    61,    72
};

#if RAKU_YYDEBUG
/* YYRLINE[YYN] -- Source line where rule number YYN was defined.  */
static const yytype_int16 yyrline[] =
{
       0,   194,   194,   220,   221,   224,   226,   228,   230,   232,
     234,   236,   238,   240,   242,   244,   247,   249,   252,   254,
     256,   258,   260,   265,   268,   271,   274,   277,   280,   281,
     282,   283,   284,   285,   288,   291,   292,   293,   294,   295,
     298,   300,   302,   306,   310,   312,   316,   320,   324,   326,
     328,   333,   338,   351,   366,   367,   375,   383,   391,   423,
     424,   427,   430,   441,   452,   456,   462,   463,   466,   469,
     472,   473,   488,   491,   492,   493,   494,   495,   496,   497,
     498,   499,   500,   501,   507,   513,   519,   522,   523,   524,
     527,   528,   529,   530,   533,   534,   535,   536,   537,   540,
     541,   542,   544,   546,   551,   552,   558,   562,   569,   574,
     579,   582,   585,   588,   591,   594,   597,   598,   601,   602,
     603,   604,   605,   606,   607,   608,   613,   617,   619,   621,
     623,   625,   627,   628,   633
};
#endif

/** Accessing symbol of state STATE.  */
#define YY_ACCESSING_SYMBOL(State) YY_CAST (yysymbol_kind_t, yystos[State])

#if RAKU_YYDEBUG || 0
/* The user-facing name of the symbol whose (internal) number is
   YYSYMBOL.  No bounds checking.  */
static const char *yysymbol_name (yysymbol_kind_t yysymbol) YY_ATTRIBUTE_UNUSED;

/* YYTNAME[SYMBOL-NUM] -- String name of the symbol SYMBOL-NUM.
   First, the terminals, then, starting at YYNTOKENS, nonterminals.  */
static const char *const yytname[] =
{
  "\"end of file\"", "error", "\"invalid token\"", "LIT_INT", "LIT_FLOAT",
  "LIT_STR", "LIT_INTERP_STR", "LIT_REGEX", "LIT_MATCH_GLOBAL",
  "LIT_SUBST", "VAR_SCALAR", "VAR_ARRAY", "VAR_HASH", "VAR_TWIGIL",
  "IDENT", "VAR_CAPTURE", "VAR_NAMED_CAPTURE", "KW_MY", "KW_SAY",
  "KW_PRINT", "KW_IF", "KW_ELSE", "KW_ELSIF", "KW_WHILE", "KW_FOR",
  "KW_SUB", "KW_GATHER", "KW_TAKE", "KW_RETURN", "KW_GIVEN", "KW_WHEN",
  "KW_DEFAULT", "KW_EXISTS", "KW_DELETE", "KW_UNLESS", "KW_UNTIL",
  "KW_REPEAT", "KW_MAP", "KW_GREP", "KW_SORT", "KW_TRY", "KW_CATCH",
  "KW_DIE", "KW_CLASS", "KW_METHOD", "KW_HAS", "KW_NEW", "OP_FATARROW",
  "OP_RANGE", "OP_RANGE_EX", "OP_ARROW", "OP_EQ", "OP_NE", "OP_LE",
  "OP_GE", "OP_SEQ", "OP_SNE", "OP_AND", "OP_OR", "OP_BIND", "OP_SMATCH",
  "OP_DIV", "'='", "'!'", "'<'", "'>'", "'~'", "'+'", "'-'", "'*'", "'/'",
  "'%'", "UMINUS", "'.'", "';'", "'('", "','", "')'", "'['", "']'", "'{'",
  "'}'", "$accept", "program", "stmt_list", "stmt", "if_stmt",
  "while_stmt", "unless_stmt", "until_stmt", "repeat_stmt", "for_stmt",
  "given_stmt", "when_list", "sub_decl", "class_decl", "class_body_list",
  "named_arg_list", "param_list", "block", "closure", "expr", "cmp_expr",
  "range_expr", "add_expr", "mul_expr", "unary_expr", "postfix_expr",
  "call_expr", "arg_list", "atom", YY_NULLPTR
};

static const char *
yysymbol_name (yysymbol_kind_t yysymbol)
{
  return yytname[yysymbol];
}
#endif

#define YYPACT_NINF (-47)

#define yypact_value_is_default(Yyn) \
  ((Yyn) == YYPACT_NINF)

#define YYTABLE_NINF (-1)

#define yytable_value_is_error(Yyn) \
  0

/* YYPACT[STATE-NUM] -- Index in YYTABLE of the portion describing
   STATE-NUM.  */
static const yytype_int16 yypact[] =
{
     -47,    27,   463,   -47,   -47,   -47,   -47,   -47,    11,   -39,
     -46,   -47,   -25,   -47,   -47,    71,   544,   584,   -15,   -12,
     624,    32,    -4,   624,   504,   624,    43,   116,    44,    69,
      -4,    65,    65,   229,    -4,   624,   133,   664,   664,   624,
     -47,   -47,   -47,   -47,   -47,   -47,   -47,   -47,   -47,   -47,
      75,     9,   -47,   107,    37,   -47,   -47,   -47,    77,   624,
     137,   624,   138,   624,   108,   354,   102,   103,   104,    91,
     114,    90,   -29,   624,    79,   624,    95,   624,   624,   -40,
     387,   105,   -47,   -47,    96,   -47,   109,    97,   -28,   -27,
     624,   624,   -47,   624,   624,   624,   624,   -47,   140,   -47,
      98,   -47,   -47,   -47,   111,   -47,   664,   664,   664,   664,
     664,   664,   664,   664,   664,   664,   106,   664,   664,   664,
     664,   664,   664,   664,   664,   664,   165,   112,   122,   118,
     120,   117,   115,   -47,   -47,    10,   624,   624,   624,   -36,
     -20,    -6,   624,   624,   175,   624,    28,   -47,    54,   -47,
     123,   124,   181,   -47,   664,   664,    -7,   313,   -47,   -47,
     -47,   179,   624,   185,   624,   127,   128,   129,   -47,   -47,
     -47,    -4,   -47,   -47,    50,    50,    50,    50,    50,    50,
      50,    50,    50,    50,   -47,   -47,   -47,    50,    50,    37,
      37,    37,   -47,   -47,   -47,   -47,   134,   -47,   624,   149,
     151,   152,    -8,   624,   -47,   121,   141,   142,   624,   -47,
     624,   -47,   624,   -47,   -47,   139,   154,   136,   624,   624,
      -4,    -4,    -4,   -38,   -35,   -47,    -4,    56,   -47,   -17,
     157,   143,   163,   148,    -4,    -4,   -47,   -47,   -23,   395,
     156,   624,   624,   624,   184,   -47,    58,   -47,   -47,   -47,
     -47,   162,   164,   172,   -47,   -47,   -47,   160,   170,   227,
     -47,   -47,   239,   240,   -47,   243,    -4,   624,    -4,   -47,
     -47,   -47,   182,   183,   237,   -47,   245,    31,   -47,   -47,
      62,   -47,   186,   188,   189,   624,   250,   -47,   -47,   -47,
     -47,   191,   196,    -9,    -4,    -4,   -47,   -47,    -4,   192,
     -47,   -47,    -4,   197,   200,   201,   -47,   -47,   -47,   -47,
     -47,   230,   -47,   -47,   -47,   -47,   -47,   -47,   -47,   -47,
     -47,    -5,   -47,   -47,   624,    -4,    64,   -47,   -47,    -4,
     -47
};

/* YYDEFACT[STATE-NUM] -- Default reduction number in state STATE-NUM.
   Performed when YYTABLE does not specify something else to do.  Zero
   means the default is an error.  */
static const yytype_uint8 yydefact[] =
{
       3,     0,     2,     1,   118,   119,   120,   121,   122,   123,
     124,   133,   132,   125,   126,     0,     0,     0,     0,     0,
       0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
       0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
       4,    29,    30,    35,    36,    37,    31,    32,    38,    39,
       0,    72,    86,    89,    93,    98,   101,   102,   115,     0,
       0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
     122,   123,   124,     0,     0,     0,     0,     0,     0,     0,
      89,     0,     3,    71,     0,    20,     0,     0,     0,     0,
       0,     0,    47,     0,     0,     0,     0,   113,    33,   110,
       0,   122,   100,    99,     0,    28,     0,     0,     0,     0,
       0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
       0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
       0,     0,     0,   104,   116,     0,     0,     0,     0,     0,
       0,     0,     0,     0,     0,     0,     0,    14,     0,    16,
       0,     0,     0,    51,     0,     0,     0,     0,    18,    19,
      54,     0,     0,     0,     0,     0,     0,     0,   111,   112,
     114,     0,    59,   134,    73,    74,    87,    88,    75,    76,
      79,    80,    81,    82,    83,    84,    85,    77,    78,    92,
      90,    91,    97,    94,    95,    96,   109,    21,     0,   127,
     128,   129,     0,     0,   103,     0,     0,     0,     0,    11,
       0,    12,     0,    13,    70,     0,     0,     0,     0,     0,
       0,     0,     0,    87,    88,    66,     0,     0,    68,     0,
       0,     0,     0,     0,     0,     0,    69,    34,     0,     0,
       0,     0,     0,     0,     0,   106,     0,   117,     5,     6,
       7,     0,     0,     0,   127,   128,   129,     0,     0,    40,
      43,    50,     0,     0,    57,     0,     0,     0,     0,    52,
     130,   131,     0,     0,    44,    46,     0,     0,    58,   108,
       0,    22,     0,     0,     0,     0,     0,   105,     8,     9,
      10,     0,     0,     0,     0,     0,    67,    56,     0,     0,
      26,    27,     0,     0,     0,     0,   107,    23,    24,    25,
      64,     0,    15,    17,    42,    41,    48,    49,    55,    53,
      45,     0,    61,    60,     0,     0,     0,    65,    63,     0,
      62
};

/* YYPGOTO[NTERM-NUM].  */
static const yytype_int16 yypgoto[] =
{
     -47,   -47,   194,   -47,   -11,   -47,   -47,   -47,   -47,   -47,
     -47,   -47,   -47,   -47,   -47,   -47,   -43,   -14,   110,   -16,
     -47,   -47,   -18,     4,   -13,   -47,   -47,    46,   -47
};

/* YYDEFGOTO[NTERM-NUM].  */
static const yytype_uint8 yydefgoto[] =
{
       0,     1,     2,    40,    41,    42,    43,    44,    45,    46,
      47,   229,    48,    49,   238,   246,   227,    83,    94,    50,
      51,    52,    53,    54,    55,    56,    57,   135,    58
};

/* YYTABLE[YYPACT[STATE-NUM]] -- What to do in state STATE-NUM.  If
   positive, shift that token.  If negative, reduce the rule whose
   number is the opposite.  If YYTABLE_NINF, syntax error.  */
static const yytype_int16 yytable[] =
{
      74,    76,    80,   225,    79,   225,   244,    84,    86,    87,
     152,    18,   262,   267,   268,   263,    92,    97,    62,    99,
      98,   276,   277,   104,   102,   103,   208,     3,   119,   120,
     121,   119,   120,   121,    63,   144,   161,   163,   209,    61,
      82,   304,   210,   127,   305,   129,    81,   131,    64,   134,
      65,   145,   162,   164,   211,    88,   212,   146,   278,   148,
      77,   150,   151,    78,   269,   153,   106,   107,   213,   245,
     226,    82,   325,    59,   165,   166,    82,   167,   168,   169,
     170,    66,    67,    68,    60,    69,   203,   204,   174,   175,
     176,   177,   178,   179,   180,   181,   182,   183,   122,   187,
     188,   139,   140,   141,   218,   173,   123,   124,   125,   192,
     193,   194,   195,   184,   185,   186,   119,   120,   121,    90,
     205,   206,   207,   189,   190,   191,   214,   215,    89,   217,
     219,   173,   265,   266,   286,   287,   223,   224,   203,   306,
     265,   329,    95,    96,    91,    93,   231,   100,   233,   105,
     126,   128,   130,   147,   132,   108,   109,   237,   110,   111,
     112,   113,   114,   115,   136,   137,   138,   116,   143,   149,
     158,   117,   118,   119,   120,   121,   142,   160,   172,   196,
     156,   171,   240,   159,   198,   200,   197,   247,   173,   216,
     202,   222,   251,   230,   252,   248,   253,   199,   201,   232,
     220,   221,   257,   258,   234,   235,   259,   260,   261,   239,
     236,   241,   264,   242,   243,   249,   250,   256,   254,   255,
     274,   275,   270,   134,   271,   282,   283,   284,   272,   273,
     281,   285,     4,     5,     6,     7,   288,   291,   289,    70,
      71,    72,    11,    12,    13,    14,   290,   292,   293,   294,
     295,   298,   297,   296,   299,    22,   300,   301,   302,   303,
     307,    26,   308,   309,   311,   312,    31,    32,    33,   310,
     313,    35,   321,   319,   322,   323,   157,   324,   326,   315,
     316,   317,   314,     0,   318,   280,     0,     0,   320,     0,
       0,     0,    37,     0,     0,     0,     0,    38,     0,     0,
       0,     0,     0,     0,    39,     0,     0,     0,   327,    93,
       0,   328,     0,     0,     0,   330,     4,     5,     6,     7,
       0,     0,     0,     8,     9,    10,    11,    12,    13,    14,
      15,    16,    17,    18,     0,     0,    19,    20,    21,    22,
      23,    24,    25,     0,     0,    26,    27,    28,    29,    30,
      31,    32,    33,    34,     0,    35,    36,     4,     5,     6,
       7,     0,     0,     0,    70,    71,    72,    11,    12,    13,
      14,     0,     0,     0,     0,     0,    37,     0,     0,     0,
      22,    38,     0,     0,     0,     0,    26,     0,    39,     0,
       0,    31,    32,    33,   228,     0,    35,     0,     4,     5,
       6,     7,     0,     0,     0,    70,    71,    72,    11,    12,
      13,    14,     0,     0,     0,     0,     0,    37,     0,     0,
       0,    22,    38,     0,     0,     0,     0,    26,     0,    39,
       0,   133,    31,    32,    33,   154,   155,    35,   110,   111,
     112,   113,   114,   115,     0,     0,     0,   116,     0,     0,
       0,   117,   118,   119,   120,   121,     0,     0,    37,     0,
       0,     0,     0,    38,     0,     0,     4,     5,     6,     7,
      39,     0,   279,     8,     9,    10,    11,    12,    13,    14,
      15,    16,    17,    18,     0,     0,    19,    20,    21,    22,
      23,    24,    25,     0,     0,    26,    27,    28,    29,    30,
      31,    32,    33,    34,     0,    35,    36,     4,     5,     6,
       7,     0,     0,     0,    70,    71,    72,    11,    12,    13,
      14,     0,     0,     0,     0,     0,    37,     0,     0,     0,
      22,    38,     0,     0,     0,     0,    26,     0,    39,     0,
       0,    31,    32,    33,     0,     0,    35,     4,     5,     6,
       7,     0,     0,     0,    70,    71,    72,    11,    12,    13,
      14,     0,     0,     0,     0,     0,     0,    37,     0,     0,
      22,     0,    38,     0,     0,     0,    26,     0,    85,    39,
       0,    31,    32,    33,     0,     0,    35,     4,     5,     6,
       7,     0,     0,     0,    70,    71,    72,    11,    12,    13,
      14,     0,     0,     0,     0,     0,     0,    37,     0,     0,
      22,     0,    38,     0,     0,     0,    26,     0,     0,    73,
       0,    31,    32,    33,     0,     0,    35,     4,     5,     6,
       7,     0,     0,     0,    70,    71,    72,    11,    12,    13,
      14,     0,     0,     0,     0,     0,     0,    37,     0,     0,
      22,     0,    38,     0,     0,     0,    26,     0,     0,    75,
       0,    31,    32,    33,     0,     0,    35,     4,     5,     6,
       7,     0,     0,     0,   101,    71,    72,    11,    12,    13,
      14,     0,     0,     0,     0,     0,     0,    37,     0,     0,
       0,     0,    38,     0,     0,     0,    26,     0,     0,    39,
       0,    31,    32,    33,     0,     0,    35,     0,     0,     0,
       0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
       0,     0,     0,     0,     0,     0,     0,    37,     0,     0,
       0,     0,    38,     0,     0,     0,     0,     0,     0,    39
};

static const yytype_int16 yycheck[] =
{
      16,    17,    20,    10,    20,    10,    14,    23,    24,    25,
      50,    20,    50,    30,    31,    50,    30,    33,    64,    35,
      34,    44,    45,    39,    37,    38,    62,     0,    66,    67,
      68,    66,    67,    68,    80,    64,    64,    64,    74,    78,
      80,    10,    62,    59,    13,    61,    14,    63,    73,    65,
      75,    80,    80,    80,    74,    12,    62,    73,    81,    75,
      75,    77,    78,    75,    81,    79,    57,    58,    74,    77,
      77,    80,    77,    62,    90,    91,    80,    93,    94,    95,
      96,    10,    11,    12,    73,    14,    76,    77,   106,   107,
     108,   109,   110,   111,   112,   113,   114,   115,    61,   117,
     118,    10,    11,    12,    76,    77,    69,    70,    71,   122,
     123,   124,   125,     7,     8,     9,    66,    67,    68,    75,
     136,   137,   138,   119,   120,   121,   142,   143,    12,   145,
      76,    77,    76,    77,    76,    77,   154,   155,    76,    77,
      76,    77,    32,    33,    75,    80,   162,    14,   164,    74,
      73,    14,    14,    74,    46,    48,    49,   171,    51,    52,
      53,    54,    55,    56,    62,    62,    62,    60,    78,    74,
      74,    64,    65,    66,    67,    68,    62,    80,    80,    14,
      75,    41,   198,    74,    62,    65,    74,   203,    77,    14,
      75,    10,   208,    14,   210,    74,   212,    79,    81,    14,
      77,    77,   218,   219,    77,    77,   220,   221,   222,    75,
      81,    62,   226,    62,    62,    74,    74,    81,    79,    65,
     234,   235,    65,   239,    81,   241,   242,   243,    65,    81,
      74,    47,     3,     4,     5,     6,    74,    77,    74,    10,
      11,    12,    13,    14,    15,    16,    74,    77,    21,    10,
      10,   267,   266,    10,   268,    26,    74,    74,    21,    14,
      74,    32,    74,    74,    14,    74,    37,    38,    39,   285,
      74,    42,    75,    81,    74,    74,    82,    47,   321,   293,
     294,   295,   293,    -1,   298,   239,    -1,    -1,   302,    -1,
      -1,    -1,    63,    -1,    -1,    -1,    -1,    68,    -1,    -1,
      -1,    -1,    -1,    -1,    75,    -1,    -1,    -1,   324,    80,
      -1,   325,    -1,    -1,    -1,   329,     3,     4,     5,     6,
      -1,    -1,    -1,    10,    11,    12,    13,    14,    15,    16,
      17,    18,    19,    20,    -1,    -1,    23,    24,    25,    26,
      27,    28,    29,    -1,    -1,    32,    33,    34,    35,    36,
      37,    38,    39,    40,    -1,    42,    43,     3,     4,     5,
       6,    -1,    -1,    -1,    10,    11,    12,    13,    14,    15,
      16,    -1,    -1,    -1,    -1,    -1,    63,    -1,    -1,    -1,
      26,    68,    -1,    -1,    -1,    -1,    32,    -1,    75,    -1,
      -1,    37,    38,    39,    81,    -1,    42,    -1,     3,     4,
       5,     6,    -1,    -1,    -1,    10,    11,    12,    13,    14,
      15,    16,    -1,    -1,    -1,    -1,    -1,    63,    -1,    -1,
      -1,    26,    68,    -1,    -1,    -1,    -1,    32,    -1,    75,
      -1,    77,    37,    38,    39,    48,    49,    42,    51,    52,
      53,    54,    55,    56,    -1,    -1,    -1,    60,    -1,    -1,
      -1,    64,    65,    66,    67,    68,    -1,    -1,    63,    -1,
      -1,    -1,    -1,    68,    -1,    -1,     3,     4,     5,     6,
      75,    -1,    77,    10,    11,    12,    13,    14,    15,    16,
      17,    18,    19,    20,    -1,    -1,    23,    24,    25,    26,
      27,    28,    29,    -1,    -1,    32,    33,    34,    35,    36,
      37,    38,    39,    40,    -1,    42,    43,     3,     4,     5,
       6,    -1,    -1,    -1,    10,    11,    12,    13,    14,    15,
      16,    -1,    -1,    -1,    -1,    -1,    63,    -1,    -1,    -1,
      26,    68,    -1,    -1,    -1,    -1,    32,    -1,    75,    -1,
      -1,    37,    38,    39,    -1,    -1,    42,     3,     4,     5,
       6,    -1,    -1,    -1,    10,    11,    12,    13,    14,    15,
      16,    -1,    -1,    -1,    -1,    -1,    -1,    63,    -1,    -1,
      26,    -1,    68,    -1,    -1,    -1,    32,    -1,    74,    75,
      -1,    37,    38,    39,    -1,    -1,    42,     3,     4,     5,
       6,    -1,    -1,    -1,    10,    11,    12,    13,    14,    15,
      16,    -1,    -1,    -1,    -1,    -1,    -1,    63,    -1,    -1,
      26,    -1,    68,    -1,    -1,    -1,    32,    -1,    -1,    75,
      -1,    37,    38,    39,    -1,    -1,    42,     3,     4,     5,
       6,    -1,    -1,    -1,    10,    11,    12,    13,    14,    15,
      16,    -1,    -1,    -1,    -1,    -1,    -1,    63,    -1,    -1,
      26,    -1,    68,    -1,    -1,    -1,    32,    -1,    -1,    75,
      -1,    37,    38,    39,    -1,    -1,    42,     3,     4,     5,
       6,    -1,    -1,    -1,    10,    11,    12,    13,    14,    15,
      16,    -1,    -1,    -1,    -1,    -1,    -1,    63,    -1,    -1,
      -1,    -1,    68,    -1,    -1,    -1,    32,    -1,    -1,    75,
      -1,    37,    38,    39,    -1,    -1,    42,    -1,    -1,    -1,
      -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,
      -1,    -1,    -1,    -1,    -1,    -1,    -1,    63,    -1,    -1,
      -1,    -1,    68,    -1,    -1,    -1,    -1,    -1,    -1,    75
};

/* YYSTOS[STATE-NUM] -- The symbol kind of the accessing symbol of
   state STATE-NUM.  */
static const yytype_int8 yystos[] =
{
       0,    83,    84,     0,     3,     4,     5,     6,    10,    11,
      12,    13,    14,    15,    16,    17,    18,    19,    20,    23,
      24,    25,    26,    27,    28,    29,    32,    33,    34,    35,
      36,    37,    38,    39,    40,    42,    43,    63,    68,    75,
      85,    86,    87,    88,    89,    90,    91,    92,    94,    95,
     101,   102,   103,   104,   105,   106,   107,   108,   110,    62,
      73,    78,    64,    80,    73,    75,    10,    11,    12,    14,
      10,    11,    12,    75,   101,    75,   101,    75,    75,   101,
     104,    14,    80,    99,   101,    74,   101,   101,    12,    12,
      75,    75,    99,    80,   100,   100,   100,   101,    99,   101,
      14,    10,   106,   106,   101,    74,    57,    58,    48,    49,
      51,    52,    53,    54,    55,    56,    60,    64,    65,    66,
      67,    68,    61,    69,    70,    71,    73,   101,    14,   101,
      14,   101,    46,    77,   101,   109,    62,    62,    62,    10,
      11,    12,    62,    78,    64,    80,   101,    74,   101,    74,
     101,   101,    50,    99,    48,    49,    75,    84,    74,    74,
      80,    64,    80,    64,    80,   101,   101,   101,   101,   101,
     101,    41,    80,    77,   104,   104,   104,   104,   104,   104,
     104,   104,   104,   104,     7,     8,     9,   104,   104,   105,
     105,   105,   106,   106,   106,   106,    14,    74,    62,    79,
      65,    81,    75,    76,    77,   101,   101,   101,    62,    74,
      62,    74,    62,    74,   101,   101,    14,   101,    76,    76,
      77,    77,    10,   104,   104,    10,    77,    98,    81,    93,
      14,   101,    14,   101,    77,    77,    81,    99,    96,    75,
     101,    62,    62,    62,    14,    77,    97,   101,    74,    74,
      74,   101,   101,   101,    79,    65,    81,   101,   101,    99,
      99,    99,    50,    50,    99,    76,    77,    30,    31,    81,
      65,    81,    65,    81,    99,    99,    44,    45,    81,    77,
     109,    74,   101,   101,   101,    47,    76,    77,    74,    74,
      74,    77,    77,    21,    10,    10,    10,    99,   101,    99,
      74,    74,    21,    14,    10,    13,    77,    74,    74,    74,
     101,    14,    74,    74,    86,    99,    99,    99,    99,    81,
      99,    75,    74,    74,    47,    77,    98,   101,    99,    77,
      99
};

/* YYR1[RULE-NUM] -- Symbol kind of the left-hand side of rule RULE-NUM.  */
static const yytype_int8 yyr1[] =
{
       0,    82,    83,    84,    84,    85,    85,    85,    85,    85,
      85,    85,    85,    85,    85,    85,    85,    85,    85,    85,
      85,    85,    85,    85,    85,    85,    85,    85,    85,    85,
      85,    85,    85,    85,    85,    85,    85,    85,    85,    85,
      86,    86,    86,    87,    88,    88,    89,    90,    91,    91,
      91,    91,    92,    92,    93,    93,    94,    94,    95,    96,
      96,    96,    96,    96,    97,    97,    98,    98,    99,   100,
     101,   101,   101,   102,   102,   102,   102,   102,   102,   102,
     102,   102,   102,   102,   102,   102,   102,   103,   103,   103,
     104,   104,   104,   104,   105,   105,   105,   105,   105,   106,
     106,   106,   107,   108,   108,   108,   108,   108,   108,   108,
     108,   108,   108,   108,   108,   108,   109,   109,   110,   110,
     110,   110,   110,   110,   110,   110,   110,   110,   110,   110,
     110,   110,   110,   110,   110
};

/* YYR2[RULE-NUM] -- Number of symbols on the right-hand side of rule RULE-NUM.  */
static const yytype_int8 yyr2[] =
{
       0,     2,     1,     0,     2,     5,     5,     5,     6,     6,
       6,     4,     4,     4,     3,     7,     3,     7,     3,     3,
       2,     4,     6,     7,     7,     7,     6,     6,     2,     1,
       1,     1,     1,     2,     4,     1,     1,     1,     1,     1,
       5,     7,     7,     5,     5,     7,     5,     2,     7,     7,
       5,     3,     5,     7,     0,     4,     6,     5,     5,     0,
       4,     4,     7,     6,     3,     5,     1,     3,     3,     3,
       3,     2,     1,     3,     3,     3,     3,     3,     3,     3,
       3,     3,     3,     3,     3,     3,     1,     3,     3,     1,
       3,     3,     3,     1,     3,     3,     3,     3,     1,     2,
       2,     1,     1,     4,     3,     6,     5,     6,     5,     3,
       2,     3,     3,     2,     3,     1,     1,     3,     1,     1,
       1,     1,     1,     1,     1,     1,     1,     4,     4,     4,
       5,     5,     1,     1,     3
};


enum { YYENOMEM = -2 };

#define yyerrok         (yyerrstatus = 0)
#define yyclearin       (yychar = RAKU_YYEMPTY)

#define YYACCEPT        goto yyacceptlab
#define YYABORT         goto yyabortlab
#define YYERROR         goto yyerrorlab
#define YYNOMEM         goto yyexhaustedlab


#define YYRECOVERING()  (!!yyerrstatus)

#define YYBACKUP(Token, Value)                                    \
  do                                                              \
    if (yychar == RAKU_YYEMPTY)                                        \
      {                                                           \
        yychar = (Token);                                         \
        yylval = (Value);                                         \
        YYPOPSTACK (yylen);                                       \
        yystate = *yyssp;                                         \
        goto yybackup;                                            \
      }                                                           \
    else                                                          \
      {                                                           \
        yyerror (YY_("syntax error: cannot back up")); \
        YYERROR;                                                  \
      }                                                           \
  while (0)

/* Backward compatibility with an undocumented macro.
   Use RAKU_YYerror or RAKU_YYUNDEF. */
#define YYERRCODE RAKU_YYUNDEF


/* Enable debugging if requested.  */
#if RAKU_YYDEBUG

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
                  Kind, Value); \
      YYFPRINTF (stderr, "\n");                                           \
    }                                                                     \
} while (0)


/*-----------------------------------.
| Print this symbol's value on YYO.  |
`-----------------------------------*/

static void
yy_symbol_value_print (FILE *yyo,
                       yysymbol_kind_t yykind, YYSTYPE const * const yyvaluep)
{
  FILE *yyoutput = yyo;
  YY_USE (yyoutput);
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
                 yysymbol_kind_t yykind, YYSTYPE const * const yyvaluep)
{
  YYFPRINTF (yyo, "%s %s (",
             yykind < YYNTOKENS ? "token" : "nterm", yysymbol_name (yykind));

  yy_symbol_value_print (yyo, yykind, yyvaluep);
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
                 int yyrule)
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
                       &yyvsp[(yyi + 1) - (yynrhs)]);
      YYFPRINTF (stderr, "\n");
    }
}

# define YY_REDUCE_PRINT(Rule)          \
do {                                    \
  if (yydebug)                          \
    yy_reduce_print (yyssp, yyvsp, Rule); \
} while (0)

/* Nonzero means print parse trace.  It is left uninitialized so that
   multiple parsers can coexist.  */
int yydebug;
#else /* !RAKU_YYDEBUG */
# define YYDPRINTF(Args) ((void) 0)
# define YY_SYMBOL_PRINT(Title, Kind, Value, Location)
# define YY_STACK_PRINT(Bottom, Top)
# define YY_REDUCE_PRINT(Rule)
#endif /* !RAKU_YYDEBUG */


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
            yysymbol_kind_t yykind, YYSTYPE *yyvaluep)
{
  YY_USE (yyvaluep);
  if (!yymsg)
    yymsg = "Deleting";
  YY_SYMBOL_PRINT (yymsg, yykind, yyvaluep, yylocationp);

  YY_IGNORE_MAYBE_UNINITIALIZED_BEGIN
  YY_USE (yykind);
  YY_IGNORE_MAYBE_UNINITIALIZED_END
}


/* Lookahead token kind.  */
int yychar;

/* The semantic value of the lookahead symbol.  */
YYSTYPE yylval;
/* Number of syntax errors so far.  */
int yynerrs;




/*----------.
| yyparse.  |
`----------*/

int
yyparse (void)
{
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

  yychar = RAKU_YYEMPTY; /* Cause a token to be read.  */

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
  if (yychar == RAKU_YYEMPTY)
    {
      YYDPRINTF ((stderr, "Reading a token\n"));
      yychar = yylex ();
    }

  if (yychar <= RAKU_YYEOF)
    {
      yychar = RAKU_YYEOF;
      yytoken = YYSYMBOL_YYEOF;
      YYDPRINTF ((stderr, "Now at end of input.\n"));
    }
  else if (yychar == RAKU_YYerror)
    {
      /* The scanner already issued an error message, process directly
         to error recovery.  But do not keep the error token as
         lookahead, it is too special and may lead us to an endless
         loop in error recovery. */
      yychar = RAKU_YYUNDEF;
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
  yychar = RAKU_YYEMPTY;
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
  case 2: /* program: stmt_list  */
#line 195 "raku.y"
        {
            ExprList *all = (yyvsp[0].list);
            if (all) {
                for (int i = 0; i < all->count; i++) {
                    tree_t *e = all->items[i];
                    if (!e || !(e->t==TT_FNC && e->_id == SUB_TAG_ID)) continue;
                    e->_id = 0;
                    add_proc(e);
                    all->items[i] = NULL;
                }
                int has_body = 0;
                for (int i = 0; i < all->count; i++) if (all->items[i]) { has_body=1; break; }
                if (has_body) {
                    tree_t *mf = leaf_sval(TT_FNC, "main"); mf->v.ival = 0;
                    tree_t *mn = ast_node_new(TT_VAR); mn->v.sval = intern("main");
                    expr_add_child(mf, mn);
                    for (int i = 0; i < all->count; i++)
                        if (all->items[i]) expr_add_child(mf, all->items[i]);
                    add_proc(mf);
                }
                exprlist_free(all);
            }
        }
#line 1632 "raku.tab.c"
    break;

  case 3: /* stmt_list: %empty  */
#line 220 "raku.y"
         { (yyval.list) = exprlist_new(); }
#line 1638 "raku.tab.c"
    break;

  case 4: /* stmt_list: stmt_list stmt  */
#line 221 "raku.y"
                     { (yyval.list) = exprlist_append((yyvsp[-1].list), (yyvsp[0].node)); }
#line 1644 "raku.tab.c"
    break;

  case 5: /* stmt: KW_MY VAR_SCALAR '=' expr ';'  */
#line 225 "raku.y"
        { (yyval.node) = expr_binary(TT_ASSIGN, var_node((yyvsp[-3].sval)), (yyvsp[-1].node)); }
#line 1650 "raku.tab.c"
    break;

  case 6: /* stmt: KW_MY VAR_ARRAY '=' expr ';'  */
#line 227 "raku.y"
        { (yyval.node) = expr_binary(TT_ASSIGN, var_node((yyvsp[-3].sval)), (yyvsp[-1].node)); }
#line 1656 "raku.tab.c"
    break;

  case 7: /* stmt: KW_MY VAR_HASH '=' expr ';'  */
#line 229 "raku.y"
        { (yyval.node) = expr_binary(TT_ASSIGN, var_node((yyvsp[-3].sval)), (yyvsp[-1].node)); }
#line 1662 "raku.tab.c"
    break;

  case 8: /* stmt: KW_MY IDENT VAR_SCALAR '=' expr ';'  */
#line 231 "raku.y"
        { free((yyvsp[-4].sval)); (yyval.node) = expr_binary(TT_ASSIGN, var_node((yyvsp[-3].sval)), (yyvsp[-1].node)); }
#line 1668 "raku.tab.c"
    break;

  case 9: /* stmt: KW_MY IDENT VAR_ARRAY '=' expr ';'  */
#line 233 "raku.y"
        { free((yyvsp[-4].sval)); (yyval.node) = expr_binary(TT_ASSIGN, var_node((yyvsp[-3].sval)), (yyvsp[-1].node)); }
#line 1674 "raku.tab.c"
    break;

  case 10: /* stmt: KW_MY IDENT VAR_HASH '=' expr ';'  */
#line 235 "raku.y"
        { free((yyvsp[-4].sval)); (yyval.node) = expr_binary(TT_ASSIGN, var_node((yyvsp[-3].sval)), (yyvsp[-1].node)); }
#line 1680 "raku.tab.c"
    break;

  case 11: /* stmt: KW_MY IDENT VAR_SCALAR ';'  */
#line 237 "raku.y"
        { free((yyvsp[-2].sval)); (yyval.node) = expr_binary(TT_ASSIGN, var_node((yyvsp[-1].sval)), leaf_sval(TT_QLIT, "")); }
#line 1686 "raku.tab.c"
    break;

  case 12: /* stmt: KW_MY IDENT VAR_ARRAY ';'  */
#line 239 "raku.y"
        { free((yyvsp[-2].sval)); (yyval.node) = expr_binary(TT_ASSIGN, var_node((yyvsp[-1].sval)), leaf_sval(TT_QLIT, "")); }
#line 1692 "raku.tab.c"
    break;

  case 13: /* stmt: KW_MY IDENT VAR_HASH ';'  */
#line 241 "raku.y"
        { free((yyvsp[-2].sval)); (yyval.node) = expr_binary(TT_ASSIGN, var_node((yyvsp[-1].sval)), leaf_sval(TT_QLIT, "")); }
#line 1698 "raku.tab.c"
    break;

  case 14: /* stmt: KW_SAY expr ';'  */
#line 243 "raku.y"
        { tree_t *c=make_call("write"); expr_add_child(c,(yyvsp[-1].node)); (yyval.node)=c; }
#line 1704 "raku.tab.c"
    break;

  case 15: /* stmt: KW_SAY '(' expr ',' expr ')' ';'  */
#line 245 "raku.y"
        {
          tree_t *c=make_call("raku_say_fh"); expr_add_child(c,(yyvsp[-4].node)); expr_add_child(c,(yyvsp[-2].node)); (yyval.node)=c; }
#line 1711 "raku.tab.c"
    break;

  case 16: /* stmt: KW_PRINT expr ';'  */
#line 248 "raku.y"
        { tree_t *c=make_call("writes"); expr_add_child(c,(yyvsp[-1].node)); (yyval.node)=c; }
#line 1717 "raku.tab.c"
    break;

  case 17: /* stmt: KW_PRINT '(' expr ',' expr ')' ';'  */
#line 250 "raku.y"
        {
          tree_t *c=make_call("raku_print_fh"); expr_add_child(c,(yyvsp[-4].node)); expr_add_child(c,(yyvsp[-2].node)); (yyval.node)=c; }
#line 1724 "raku.tab.c"
    break;

  case 18: /* stmt: KW_TAKE expr ';'  */
#line 253 "raku.y"
        { (yyval.node)=expr_unary(TT_SUSPEND,(yyvsp[-1].node)); }
#line 1730 "raku.tab.c"
    break;

  case 19: /* stmt: KW_RETURN expr ';'  */
#line 255 "raku.y"
        { tree_t *r=ast_node_new(TT_RETURN); expr_add_child(r,(yyvsp[-1].node)); (yyval.node)=r; }
#line 1736 "raku.tab.c"
    break;

  case 20: /* stmt: KW_RETURN ';'  */
#line 257 "raku.y"
        { (yyval.node)=ast_node_new(TT_RETURN); }
#line 1742 "raku.tab.c"
    break;

  case 21: /* stmt: VAR_SCALAR '=' expr ';'  */
#line 259 "raku.y"
        { (yyval.node)=expr_binary(TT_ASSIGN,var_node((yyvsp[-3].sval)),(yyvsp[-1].node)); }
#line 1748 "raku.tab.c"
    break;

  case 22: /* stmt: VAR_SCALAR '.' IDENT '=' expr ';'  */
#line 261 "raku.y"
        { tree_t *fe=ast_node_new(TT_FIELD);
          fe->v.sval=(char*)intern((yyvsp[-3].sval)); free((yyvsp[-3].sval));
          expr_add_child(fe,var_node((yyvsp[-5].sval)));
          (yyval.node)=expr_binary(TT_ASSIGN,fe,(yyvsp[-1].node)); }
#line 1757 "raku.tab.c"
    break;

  case 23: /* stmt: VAR_ARRAY '[' expr ']' '=' expr ';'  */
#line 266 "raku.y"
        { tree_t *c=make_call("arr_set");
          expr_add_child(c,var_node((yyvsp[-6].sval))); expr_add_child(c,(yyvsp[-4].node)); expr_add_child(c,(yyvsp[-1].node)); (yyval.node)=c; }
#line 1764 "raku.tab.c"
    break;

  case 24: /* stmt: VAR_HASH '<' IDENT '>' '=' expr ';'  */
#line 269 "raku.y"
        { tree_t *c=make_call("hash_set");
          expr_add_child(c,var_node((yyvsp[-6].sval))); expr_add_child(c,leaf_sval(TT_QLIT,(yyvsp[-4].sval))); expr_add_child(c,(yyvsp[-1].node)); (yyval.node)=c; }
#line 1771 "raku.tab.c"
    break;

  case 25: /* stmt: VAR_HASH '{' expr '}' '=' expr ';'  */
#line 272 "raku.y"
        { tree_t *c=make_call("hash_set");
          expr_add_child(c,var_node((yyvsp[-6].sval))); expr_add_child(c,(yyvsp[-4].node)); expr_add_child(c,(yyvsp[-1].node)); (yyval.node)=c; }
#line 1778 "raku.tab.c"
    break;

  case 26: /* stmt: KW_DELETE VAR_HASH '<' IDENT '>' ';'  */
#line 275 "raku.y"
        { tree_t *c=make_call("hash_delete");
          expr_add_child(c,var_node((yyvsp[-4].sval))); expr_add_child(c,leaf_sval(TT_QLIT,(yyvsp[-2].sval))); (yyval.node)=c; }
#line 1785 "raku.tab.c"
    break;

  case 27: /* stmt: KW_DELETE VAR_HASH '{' expr '}' ';'  */
#line 278 "raku.y"
        { tree_t *c=make_call("hash_delete");
          expr_add_child(c,var_node((yyvsp[-4].sval))); expr_add_child(c,(yyvsp[-2].node)); (yyval.node)=c; }
#line 1792 "raku.tab.c"
    break;

  case 28: /* stmt: expr ';'  */
#line 280 "raku.y"
               { (yyval.node)=(yyvsp[-1].node); }
#line 1798 "raku.tab.c"
    break;

  case 29: /* stmt: if_stmt  */
#line 281 "raku.y"
                        { (yyval.node)=(yyvsp[0].node); }
#line 1804 "raku.tab.c"
    break;

  case 30: /* stmt: while_stmt  */
#line 282 "raku.y"
                        { (yyval.node)=(yyvsp[0].node); }
#line 1810 "raku.tab.c"
    break;

  case 31: /* stmt: for_stmt  */
#line 283 "raku.y"
                        { (yyval.node)=(yyvsp[0].node); }
#line 1816 "raku.tab.c"
    break;

  case 32: /* stmt: given_stmt  */
#line 284 "raku.y"
                        { (yyval.node)=(yyvsp[0].node); }
#line 1822 "raku.tab.c"
    break;

  case 33: /* stmt: KW_TRY block  */
#line 286 "raku.y"
        { tree_t *c=make_call("raku_try");
          expr_add_child(c,(yyvsp[0].node)); (yyval.node)=c; }
#line 1829 "raku.tab.c"
    break;

  case 34: /* stmt: KW_TRY block KW_CATCH block  */
#line 289 "raku.y"
        { tree_t *c=make_call("raku_try");
          expr_add_child(c,(yyvsp[-2].node)); expr_add_child(c,(yyvsp[0].node)); (yyval.node)=c; }
#line 1836 "raku.tab.c"
    break;

  case 35: /* stmt: unless_stmt  */
#line 291 "raku.y"
                        { (yyval.node)=(yyvsp[0].node); }
#line 1842 "raku.tab.c"
    break;

  case 36: /* stmt: until_stmt  */
#line 292 "raku.y"
                        { (yyval.node)=(yyvsp[0].node); }
#line 1848 "raku.tab.c"
    break;

  case 37: /* stmt: repeat_stmt  */
#line 293 "raku.y"
                        { (yyval.node)=(yyvsp[0].node); }
#line 1854 "raku.tab.c"
    break;

  case 38: /* stmt: sub_decl  */
#line 294 "raku.y"
                        { (yyval.node)=(yyvsp[0].node); }
#line 1860 "raku.tab.c"
    break;

  case 39: /* stmt: class_decl  */
#line 295 "raku.y"
                        { (yyval.node)=(yyvsp[0].node); }
#line 1866 "raku.tab.c"
    break;

  case 40: /* if_stmt: KW_IF '(' expr ')' block  */
#line 299 "raku.y"
        { tree_t *e=ast_node_new(TT_IF); expr_add_child(e,(yyvsp[-2].node)); expr_add_child(e,(yyvsp[0].node)); (yyval.node)=e; }
#line 1872 "raku.tab.c"
    break;

  case 41: /* if_stmt: KW_IF '(' expr ')' block KW_ELSE block  */
#line 301 "raku.y"
        { tree_t *e=ast_node_new(TT_IF); expr_add_child(e,(yyvsp[-4].node)); expr_add_child(e,(yyvsp[-2].node)); expr_add_child(e,(yyvsp[0].node)); (yyval.node)=e; }
#line 1878 "raku.tab.c"
    break;

  case 42: /* if_stmt: KW_IF '(' expr ')' block KW_ELSE if_stmt  */
#line 303 "raku.y"
        { tree_t *e=ast_node_new(TT_IF); expr_add_child(e,(yyvsp[-4].node)); expr_add_child(e,(yyvsp[-2].node)); expr_add_child(e,(yyvsp[0].node)); (yyval.node)=e; }
#line 1884 "raku.tab.c"
    break;

  case 43: /* while_stmt: KW_WHILE '(' expr ')' block  */
#line 307 "raku.y"
        { (yyval.node)=expr_binary(TT_WHILE,(yyvsp[-2].node),(yyvsp[0].node)); }
#line 1890 "raku.tab.c"
    break;

  case 44: /* unless_stmt: KW_UNLESS '(' expr ')' block  */
#line 311 "raku.y"
        { tree_t *e=ast_node_new(TT_IF); expr_add_child(e,expr_unary(TT_NOT,(yyvsp[-2].node))); expr_add_child(e,(yyvsp[0].node)); (yyval.node)=e; }
#line 1896 "raku.tab.c"
    break;

  case 45: /* unless_stmt: KW_UNLESS '(' expr ')' block KW_ELSE block  */
#line 313 "raku.y"
        { tree_t *e=ast_node_new(TT_IF); expr_add_child(e,expr_unary(TT_NOT,(yyvsp[-4].node))); expr_add_child(e,(yyvsp[-2].node)); expr_add_child(e,(yyvsp[0].node)); (yyval.node)=e; }
#line 1902 "raku.tab.c"
    break;

  case 46: /* until_stmt: KW_UNTIL '(' expr ')' block  */
#line 317 "raku.y"
        { tree_t *e=ast_node_new(TT_UNTIL); expr_add_child(e,(yyvsp[-2].node)); expr_add_child(e,(yyvsp[0].node)); (yyval.node)=e; }
#line 1908 "raku.tab.c"
    break;

  case 47: /* repeat_stmt: KW_REPEAT block  */
#line 321 "raku.y"
        { tree_t *e=ast_node_new(TT_REPEAT); expr_add_child(e,(yyvsp[0].node)); (yyval.node)=e; }
#line 1914 "raku.tab.c"
    break;

  case 48: /* for_stmt: KW_FOR add_expr OP_RANGE add_expr OP_ARROW VAR_SCALAR block  */
#line 325 "raku.y"
        { (yyval.node) = make_for_range((yyvsp[-5].node), (yyvsp[-3].node), strip_sigil((yyvsp[-1].sval)), (yyvsp[0].node)); }
#line 1920 "raku.tab.c"
    break;

  case 49: /* for_stmt: KW_FOR add_expr OP_RANGE_EX add_expr OP_ARROW VAR_SCALAR block  */
#line 327 "raku.y"
        { (yyval.node) = make_for_range((yyvsp[-5].node), (yyvsp[-3].node), strip_sigil((yyvsp[-1].sval)), (yyvsp[0].node)); }
#line 1926 "raku.tab.c"
    break;

  case 50: /* for_stmt: KW_FOR expr OP_ARROW VAR_SCALAR block  */
#line 329 "raku.y"
        { const char *vn = intern(strip_sigil((yyvsp[-1].sval)));
          tree_t *gen = expr_unary(TT_ITERATE, (yyvsp[-3].node));
          gen->v.sval = (char *)vn;
          (yyval.node)=expr_binary(TT_EVERY,gen,(yyvsp[0].node)); }
#line 1935 "raku.tab.c"
    break;

  case 51: /* for_stmt: KW_FOR expr block  */
#line 334 "raku.y"
        { tree_t *gen=((yyvsp[-1].node)->t==TT_VAR)?expr_unary(TT_ITERATE,(yyvsp[-1].node)):(yyvsp[-1].node);
          (yyval.node)=expr_binary(TT_EVERY,gen,(yyvsp[0].node)); }
#line 1942 "raku.tab.c"
    break;

  case 52: /* given_stmt: KW_GIVEN expr '{' when_list '}'  */
#line 339 "raku.y"
        { /* RK-18d: TT_CASE[ topic, cmpnode0, val0, body0, ... ]
           * cmp kind stored in TT_ILIT child to avoid corrupting val->v.ival. */
          tree_t *ec=ast_node_new(TT_CASE);
          expr_add_child(ec,(yyvsp[-3].node));
          ExprList *whens=(yyvsp[-1].list);
          for(int i=0;i<whens->count;i++){
              tree_t *pair=whens->items[i];
              tree_t *cn=pair->c[0], *val=pair->c[1], *body=pair->c[2];
              expr_add_child(ec,cn); expr_add_child(ec,val); expr_add_child(ec,body);
          }
          exprlist_free(whens);
          (yyval.node)=ec; }
#line 1959 "raku.tab.c"
    break;

  case 53: /* given_stmt: KW_GIVEN expr '{' when_list KW_DEFAULT block '}'  */
#line 352 "raku.y"
        {
          tree_t *ec=ast_node_new(TT_CASE);
          expr_add_child(ec,(yyvsp[-5].node));
          ExprList *whens=(yyvsp[-3].list);
          for(int i=0;i<whens->count;i++){
              tree_t *pair=whens->items[i];
              tree_t *cn=pair->c[0], *val=pair->c[1], *body=pair->c[2];
              expr_add_child(ec,cn); expr_add_child(ec,val); expr_add_child(ec,body);
          }
          exprlist_free(whens);
          expr_add_child(ec,ast_node_new(TT_NUL)); expr_add_child(ec,ast_node_new(TT_NUL)); expr_add_child(ec,(yyvsp[-1].node));
          (yyval.node)=ec; }
#line 1976 "raku.tab.c"
    break;

  case 54: /* when_list: %empty  */
#line 366 "raku.y"
       { (yyval.list)=exprlist_new(); }
#line 1982 "raku.tab.c"
    break;

  case 55: /* when_list: when_list KW_WHEN expr block  */
#line 368 "raku.y"
        { tree_e cmpkind=((yyvsp[-1].node)->t==TT_QLIT)?TT_LEQ:TT_EQ;
          tree_t *cn=ast_node_new(TT_ILIT); cn->v.ival=(long long)cmpkind;
          tree_t *pair=ast_node_new(TT_SEQ_EXPR);
          expr_add_child(pair,cn); expr_add_child(pair,(yyvsp[-1].node)); expr_add_child(pair,(yyvsp[0].node));
          (yyval.list)=exprlist_append((yyvsp[-3].list),pair); }
#line 1992 "raku.tab.c"
    break;

  case 56: /* sub_decl: KW_SUB IDENT '(' param_list ')' block  */
#line 376 "raku.y"
        { ExprList *params=(yyvsp[-2].list); int np=params?params->count:0;
          tree_t *e=leaf_sval(TT_FNC,(yyvsp[-4].sval)); e->v.ival=(long long)np; e->_id=SUB_TAG_ID;
          tree_t *nn=ast_node_new(TT_VAR); nn->v.sval=intern((yyvsp[-4].sval)); expr_add_child(e,nn);
          if(params){ for(int i=0;i<np;i++) expr_add_child(e,params->items[i]); exprlist_free(params); }
          tree_t *body=(yyvsp[0].node);
          for(int i=0;i<body->n;i++) expr_add_child(e,body->c[i]);
          (yyval.node)=e; }
#line 2004 "raku.tab.c"
    break;

  case 57: /* sub_decl: KW_SUB IDENT '(' ')' block  */
#line 384 "raku.y"
        { tree_t *e=leaf_sval(TT_FNC,(yyvsp[-3].sval)); e->v.ival=(long long)0; e->_id=SUB_TAG_ID;
          tree_t *nn=ast_node_new(TT_VAR); nn->v.sval=intern((yyvsp[-3].sval)); expr_add_child(e,nn);
          tree_t *body=(yyvsp[0].node);
          for(int i=0;i<body->n;i++) expr_add_child(e,body->c[i]);
          (yyval.node)=e; }
#line 2014 "raku.tab.c"
    break;

  case 58: /* class_decl: KW_CLASS IDENT '{' class_body_list '}'  */
#line 392 "raku.y"
        {
            const char *cname = intern((yyvsp[-3].sval)); free((yyvsp[-3].sval));
            ExprList *body = (yyvsp[-1].list);
            tree_t *rec = ast_node_new(TT_RECORD);
            rec->v.sval = (char *)cname;
            if (body) {
                for (int i = 0; i < body->count; i++) {
                    tree_t *item = body->items[i];
                    if (!item) continue;
                    if (item->t == TT_VAR) {
                        expr_add_child(rec, item);
                    } else if (item->t == TT_FNC && item->_id == SUB_TAG_ID) {
                        char fullname[256];
                        snprintf(fullname, sizeof fullname, "%s__%s", cname, item->v.sval);
                        const char *fname = intern(fullname);
                        raku_meth_register(cname, item->v.sval, fname);
                        item->v.sval = (char *)fname;
                        if (item->n > 0 && item->c[0]->t == TT_VAR)
                            item->c[0]->v.sval = (char *)fname;
                        item->_id = 0;
                        add_proc(item);
                        body->items[i] = NULL;
                    }
                }
                exprlist_free(body);
            }
            add_proc(rec);
            (yyval.node) = ast_node_new(TT_NUL);
        }
#line 2048 "raku.tab.c"
    break;

  case 59: /* class_body_list: %empty  */
#line 423 "raku.y"
       { (yyval.list) = exprlist_new(); }
#line 2054 "raku.tab.c"
    break;

  case 60: /* class_body_list: class_body_list KW_HAS VAR_TWIGIL ';'  */
#line 425 "raku.y"
        { tree_t *fv = leaf_sval(TT_VAR, (yyvsp[-1].sval)); free((yyvsp[-1].sval));
          (yyval.list) = exprlist_append((yyvsp[-3].list), fv); }
#line 2061 "raku.tab.c"
    break;

  case 61: /* class_body_list: class_body_list KW_HAS VAR_SCALAR ';'  */
#line 428 "raku.y"
        { tree_t *fv = leaf_sval(TT_VAR, strip_sigil((yyvsp[-1].sval))); free((yyvsp[-1].sval));
          (yyval.list) = exprlist_append((yyvsp[-3].list), fv); }
#line 2068 "raku.tab.c"
    break;

  case 62: /* class_body_list: class_body_list KW_METHOD IDENT '(' param_list ')' block  */
#line 431 "raku.y"
        { ExprList *params = (yyvsp[-2].list); int np = params ? params->count : 0;
          tree_t *e = leaf_sval(TT_FNC, (yyvsp[-4].sval));
          e->v.ival = (long long)(np + 1); e->_id = SUB_TAG_ID;
          tree_t *nn = ast_node_new(TT_VAR); nn->v.sval = intern((yyvsp[-4].sval)); expr_add_child(e, nn);
          expr_add_child(e, leaf_sval(TT_VAR, "self"));
          if (params) { for (int i = 0; i < np; i++) expr_add_child(e, params->items[i]); exprlist_free(params); }
          tree_t *body = (yyvsp[0].node);
          for (int i = 0; i < body->n; i++) expr_add_child(e, body->c[i]);
          free((yyvsp[-4].sval));
          (yyval.list) = exprlist_append((yyvsp[-6].list), e); }
#line 2083 "raku.tab.c"
    break;

  case 63: /* class_body_list: class_body_list KW_METHOD IDENT '(' ')' block  */
#line 442 "raku.y"
        { tree_t *e = leaf_sval(TT_FNC, (yyvsp[-3].sval));
          e->v.ival = (long long)(1); e->_id = SUB_TAG_ID;
          tree_t *nn = ast_node_new(TT_VAR); nn->v.sval = intern((yyvsp[-3].sval)); expr_add_child(e, nn);
          expr_add_child(e, leaf_sval(TT_VAR, "self"));
          tree_t *body = (yyvsp[0].node);
          for (int i = 0; i < body->n; i++) expr_add_child(e, body->c[i]);
          free((yyvsp[-3].sval));
          (yyval.list) = exprlist_append((yyvsp[-5].list), e); }
#line 2096 "raku.tab.c"
    break;

  case 64: /* named_arg_list: IDENT OP_FATARROW expr  */
#line 453 "raku.y"
        { (yyval.list) = exprlist_new();
          exprlist_append((yyval.list), leaf_sval(TT_QLIT, (yyvsp[-2].sval))); free((yyvsp[-2].sval));
          exprlist_append((yyval.list), (yyvsp[0].node)); }
#line 2104 "raku.tab.c"
    break;

  case 65: /* named_arg_list: named_arg_list ',' IDENT OP_FATARROW expr  */
#line 457 "raku.y"
        { exprlist_append((yyvsp[-4].list), leaf_sval(TT_QLIT, (yyvsp[-2].sval))); free((yyvsp[-2].sval));
          exprlist_append((yyvsp[-4].list), (yyvsp[0].node));
          (yyval.list) = (yyvsp[-4].list); }
#line 2112 "raku.tab.c"
    break;

  case 66: /* param_list: VAR_SCALAR  */
#line 462 "raku.y"
                             { (yyval.list)=exprlist_append(exprlist_new(),var_node((yyvsp[0].sval))); }
#line 2118 "raku.tab.c"
    break;

  case 67: /* param_list: param_list ',' VAR_SCALAR  */
#line 463 "raku.y"
                                { (yyval.list)=exprlist_append((yyvsp[-2].list),var_node((yyvsp[0].sval))); }
#line 2124 "raku.tab.c"
    break;

  case 68: /* block: '{' stmt_list '}'  */
#line 466 "raku.y"
                         { (yyval.node)=make_seq((yyvsp[-1].list)); }
#line 2130 "raku.tab.c"
    break;

  case 69: /* closure: '{' expr '}'  */
#line 469 "raku.y"
                    { (yyval.node)=(yyvsp[-1].node); }
#line 2136 "raku.tab.c"
    break;

  case 70: /* expr: VAR_SCALAR '=' expr  */
#line 472 "raku.y"
                           { (yyval.node)=expr_binary(TT_ASSIGN,var_node((yyvsp[-2].sval)),(yyvsp[0].node)); }
#line 2142 "raku.tab.c"
    break;

  case 71: /* expr: KW_GATHER block  */
#line 473 "raku.y"
                           {
          static int gather_seq = 0;
          char gname[32]; snprintf(gname, sizeof gname, "__gather_%d", gather_seq++);
          tree_t *def = leaf_sval(TT_FNC, gname); def->v.ival = (long long)0; def->_id = SUB_TAG_ID;
          tree_t *dn  = ast_node_new(TT_VAR); dn->v.sval = intern(gname);
          expr_add_child(def, dn);
          tree_t *blk = (yyvsp[0].node);
          for (int i = 0; i < blk->n; i++) expr_add_child(def, blk->c[i]);
          def->_id = 0;
          add_proc(def);
          tree_t *call = leaf_sval(TT_FNC, gname);
          tree_t *cn   = ast_node_new(TT_VAR); cn->v.sval = intern(gname);
          expr_add_child(call, cn);
          (yyval.node) = call;
      }
#line 2162 "raku.tab.c"
    break;

  case 72: /* expr: cmp_expr  */
#line 488 "raku.y"
                           { (yyval.node)=(yyvsp[0].node); }
#line 2168 "raku.tab.c"
    break;

  case 73: /* cmp_expr: cmp_expr OP_AND add_expr  */
#line 491 "raku.y"
                                { (yyval.node)=expr_binary(TT_SEQ,(yyvsp[-2].node),(yyvsp[0].node)); }
#line 2174 "raku.tab.c"
    break;

  case 74: /* cmp_expr: cmp_expr OP_OR add_expr  */
#line 492 "raku.y"
                                { (yyval.node)=expr_binary(TT_ALT,(yyvsp[-2].node),(yyvsp[0].node)); }
#line 2180 "raku.tab.c"
    break;

  case 75: /* cmp_expr: add_expr OP_EQ add_expr  */
#line 493 "raku.y"
                                { (yyval.node)=expr_binary(TT_EQ,(yyvsp[-2].node),(yyvsp[0].node)); }
#line 2186 "raku.tab.c"
    break;

  case 76: /* cmp_expr: add_expr OP_NE add_expr  */
#line 494 "raku.y"
                                { (yyval.node)=expr_binary(TT_NE,(yyvsp[-2].node),(yyvsp[0].node)); }
#line 2192 "raku.tab.c"
    break;

  case 77: /* cmp_expr: add_expr '<' add_expr  */
#line 495 "raku.y"
                                { (yyval.node)=expr_binary(TT_LT,(yyvsp[-2].node),(yyvsp[0].node)); }
#line 2198 "raku.tab.c"
    break;

  case 78: /* cmp_expr: add_expr '>' add_expr  */
#line 496 "raku.y"
                                { (yyval.node)=expr_binary(TT_GT,(yyvsp[-2].node),(yyvsp[0].node)); }
#line 2204 "raku.tab.c"
    break;

  case 79: /* cmp_expr: add_expr OP_LE add_expr  */
#line 497 "raku.y"
                                { (yyval.node)=expr_binary(TT_LE,(yyvsp[-2].node),(yyvsp[0].node)); }
#line 2210 "raku.tab.c"
    break;

  case 80: /* cmp_expr: add_expr OP_GE add_expr  */
#line 498 "raku.y"
                                { (yyval.node)=expr_binary(TT_GE,(yyvsp[-2].node),(yyvsp[0].node)); }
#line 2216 "raku.tab.c"
    break;

  case 81: /* cmp_expr: add_expr OP_SEQ add_expr  */
#line 499 "raku.y"
                                { (yyval.node)=expr_binary(TT_LEQ,(yyvsp[-2].node),(yyvsp[0].node)); }
#line 2222 "raku.tab.c"
    break;

  case 82: /* cmp_expr: add_expr OP_SNE add_expr  */
#line 500 "raku.y"
                                { (yyval.node)=expr_binary(TT_LNE,(yyvsp[-2].node),(yyvsp[0].node)); }
#line 2228 "raku.tab.c"
    break;

  case 83: /* cmp_expr: add_expr OP_SMATCH LIT_REGEX  */
#line 502 "raku.y"
        {
          tree_t *c = make_call("raku_match");
          expr_add_child(c, (yyvsp[-2].node));
          expr_add_child(c, leaf_sval(TT_QLIT, (yyvsp[0].sval)));
          (yyval.node) = c; }
#line 2238 "raku.tab.c"
    break;

  case 84: /* cmp_expr: add_expr OP_SMATCH LIT_MATCH_GLOBAL  */
#line 508 "raku.y"
        {
          tree_t *c = make_call("raku_match_global");
          expr_add_child(c, (yyvsp[-2].node));
          expr_add_child(c, leaf_sval(TT_QLIT, (yyvsp[0].sval)));
          (yyval.node) = c; }
#line 2248 "raku.tab.c"
    break;

  case 85: /* cmp_expr: add_expr OP_SMATCH LIT_SUBST  */
#line 514 "raku.y"
        {
          tree_t *c = make_call("raku_subst");
          expr_add_child(c, (yyvsp[-2].node));
          expr_add_child(c, leaf_sval(TT_QLIT, (yyvsp[0].sval)));
          (yyval.node) = c; }
#line 2258 "raku.tab.c"
    break;

  case 86: /* cmp_expr: range_expr  */
#line 519 "raku.y"
                               { (yyval.node)=(yyvsp[0].node); }
#line 2264 "raku.tab.c"
    break;

  case 87: /* range_expr: add_expr OP_RANGE add_expr  */
#line 522 "raku.y"
                                    { (yyval.node)=expr_binary(TT_TO,(yyvsp[-2].node),(yyvsp[0].node)); }
#line 2270 "raku.tab.c"
    break;

  case 88: /* range_expr: add_expr OP_RANGE_EX add_expr  */
#line 523 "raku.y"
                                    { (yyval.node)=expr_binary(TT_TO,(yyvsp[-2].node),(yyvsp[0].node)); }
#line 2276 "raku.tab.c"
    break;

  case 89: /* range_expr: add_expr  */
#line 524 "raku.y"
                                    { (yyval.node)=(yyvsp[0].node); }
#line 2282 "raku.tab.c"
    break;

  case 90: /* add_expr: add_expr '+' mul_expr  */
#line 527 "raku.y"
                             { (yyval.node)=expr_binary(TT_ADD,(yyvsp[-2].node),(yyvsp[0].node)); }
#line 2288 "raku.tab.c"
    break;

  case 91: /* add_expr: add_expr '-' mul_expr  */
#line 528 "raku.y"
                             { (yyval.node)=expr_binary(TT_SUB,(yyvsp[-2].node),(yyvsp[0].node)); }
#line 2294 "raku.tab.c"
    break;

  case 92: /* add_expr: add_expr '~' mul_expr  */
#line 529 "raku.y"
                             { (yyval.node)=expr_binary(TT_CAT,(yyvsp[-2].node),(yyvsp[0].node)); }
#line 2300 "raku.tab.c"
    break;

  case 93: /* add_expr: mul_expr  */
#line 530 "raku.y"
                             { (yyval.node)=(yyvsp[0].node); }
#line 2306 "raku.tab.c"
    break;

  case 94: /* mul_expr: mul_expr '*' unary_expr  */
#line 533 "raku.y"
                                  { (yyval.node)=expr_binary(TT_MUL,(yyvsp[-2].node),(yyvsp[0].node)); }
#line 2312 "raku.tab.c"
    break;

  case 95: /* mul_expr: mul_expr '/' unary_expr  */
#line 534 "raku.y"
                                  { (yyval.node)=expr_binary(TT_DIV,(yyvsp[-2].node),(yyvsp[0].node)); }
#line 2318 "raku.tab.c"
    break;

  case 96: /* mul_expr: mul_expr '%' unary_expr  */
#line 535 "raku.y"
                                  { (yyval.node)=expr_binary(TT_MOD,(yyvsp[-2].node),(yyvsp[0].node)); }
#line 2324 "raku.tab.c"
    break;

  case 97: /* mul_expr: mul_expr OP_DIV unary_expr  */
#line 536 "raku.y"
                                  { (yyval.node)=expr_binary(TT_DIV,(yyvsp[-2].node),(yyvsp[0].node)); }
#line 2330 "raku.tab.c"
    break;

  case 98: /* mul_expr: unary_expr  */
#line 537 "raku.y"
                                  { (yyval.node)=(yyvsp[0].node); }
#line 2336 "raku.tab.c"
    break;

  case 99: /* unary_expr: '-' unary_expr  */
#line 540 "raku.y"
                                   { (yyval.node)=expr_unary(TT_MNS,(yyvsp[0].node)); }
#line 2342 "raku.tab.c"
    break;

  case 100: /* unary_expr: '!' unary_expr  */
#line 541 "raku.y"
                                   { (yyval.node)=expr_unary(TT_NOT,(yyvsp[0].node)); }
#line 2348 "raku.tab.c"
    break;

  case 101: /* unary_expr: postfix_expr  */
#line 542 "raku.y"
                                   { (yyval.node)=(yyvsp[0].node); }
#line 2354 "raku.tab.c"
    break;

  case 102: /* postfix_expr: call_expr  */
#line 544 "raku.y"
                         { (yyval.node)=(yyvsp[0].node); }
#line 2360 "raku.tab.c"
    break;

  case 103: /* call_expr: IDENT '(' arg_list ')'  */
#line 547 "raku.y"
        { tree_t *e=make_call((yyvsp[-3].sval));
          ExprList *args=(yyvsp[-1].list);
          if(args){ for(int i=0;i<args->count;i++) expr_add_child(e,args->items[i]); exprlist_free(args); }
          (yyval.node)=e; }
#line 2369 "raku.tab.c"
    break;

  case 104: /* call_expr: IDENT '(' ')'  */
#line 551 "raku.y"
                     { (yyval.node)=make_call((yyvsp[-2].sval)); }
#line 2375 "raku.tab.c"
    break;

  case 105: /* call_expr: IDENT '.' KW_NEW '(' named_arg_list ')'  */
#line 553 "raku.y"
        { tree_t *c=make_call("raku_new");
          expr_add_child(c,leaf_sval(TT_QLIT,(yyvsp[-5].sval))); free((yyvsp[-5].sval));
          ExprList *nargs=(yyvsp[-1].list);
          if(nargs){ for(int i=0;i<nargs->count;i++) expr_add_child(c,nargs->items[i]); exprlist_free(nargs); }
          (yyval.node)=c; }
#line 2385 "raku.tab.c"
    break;

  case 106: /* call_expr: IDENT '.' KW_NEW '(' ')'  */
#line 559 "raku.y"
        { tree_t *c=make_call("raku_new");
          expr_add_child(c,leaf_sval(TT_QLIT,(yyvsp[-4].sval))); free((yyvsp[-4].sval));
          (yyval.node)=c; }
#line 2393 "raku.tab.c"
    break;

  case 107: /* call_expr: atom '.' IDENT '(' arg_list ')'  */
#line 563 "raku.y"
        { tree_t *c=make_call("raku_mcall");
          expr_add_child(c,(yyvsp[-5].node));
          expr_add_child(c,leaf_sval(TT_QLIT,(yyvsp[-3].sval))); free((yyvsp[-3].sval));
          ExprList *args=(yyvsp[-1].list);
          if(args){ for(int i=0;i<args->count;i++) expr_add_child(c,args->items[i]); exprlist_free(args); }
          (yyval.node)=c; }
#line 2404 "raku.tab.c"
    break;

  case 108: /* call_expr: atom '.' IDENT '(' ')'  */
#line 570 "raku.y"
        { tree_t *c=make_call("raku_mcall");
          expr_add_child(c,(yyvsp[-4].node));
          expr_add_child(c,leaf_sval(TT_QLIT,(yyvsp[-2].sval))); free((yyvsp[-2].sval));
          (yyval.node)=c; }
#line 2413 "raku.tab.c"
    break;

  case 109: /* call_expr: atom '.' IDENT  */
#line 575 "raku.y"
        { tree_t *fe=ast_node_new(TT_FIELD);
          fe->v.sval=(char*)intern((yyvsp[0].sval)); free((yyvsp[0].sval));
          expr_add_child(fe,(yyvsp[-2].node));
          (yyval.node)=fe; }
#line 2422 "raku.tab.c"
    break;

  case 110: /* call_expr: KW_DIE expr  */
#line 580 "raku.y"
        { tree_t *c=make_call("raku_die");
          expr_add_child(c,(yyvsp[0].node)); (yyval.node)=c; }
#line 2429 "raku.tab.c"
    break;

  case 111: /* call_expr: KW_MAP closure expr  */
#line 583 "raku.y"
        { tree_t *c=make_call("raku_map");
          expr_add_child(c,(yyvsp[-1].node)); expr_add_child(c,(yyvsp[0].node)); (yyval.node)=c; }
#line 2436 "raku.tab.c"
    break;

  case 112: /* call_expr: KW_GREP closure expr  */
#line 586 "raku.y"
        { tree_t *c=make_call("raku_grep");
          expr_add_child(c,(yyvsp[-1].node)); expr_add_child(c,(yyvsp[0].node)); (yyval.node)=c; }
#line 2443 "raku.tab.c"
    break;

  case 113: /* call_expr: KW_SORT expr  */
#line 589 "raku.y"
        { tree_t *c=make_call("raku_sort");
          expr_add_child(c,(yyvsp[0].node)); (yyval.node)=c; }
#line 2450 "raku.tab.c"
    break;

  case 114: /* call_expr: KW_SORT closure expr  */
#line 592 "raku.y"
        { tree_t *c=make_call("raku_sort");
          expr_add_child(c,(yyvsp[-1].node)); expr_add_child(c,(yyvsp[0].node)); (yyval.node)=c; }
#line 2457 "raku.tab.c"
    break;

  case 115: /* call_expr: atom  */
#line 594 "raku.y"
                     { (yyval.node)=(yyvsp[0].node); }
#line 2463 "raku.tab.c"
    break;

  case 116: /* arg_list: expr  */
#line 597 "raku.y"
                        { (yyval.list)=exprlist_append(exprlist_new(),(yyvsp[0].node)); }
#line 2469 "raku.tab.c"
    break;

  case 117: /* arg_list: arg_list ',' expr  */
#line 598 "raku.y"
                        { (yyval.list)=exprlist_append((yyvsp[-2].list),(yyvsp[0].node)); }
#line 2475 "raku.tab.c"
    break;

  case 118: /* atom: LIT_INT  */
#line 601 "raku.y"
                      { tree_t *e=ast_node_new(TT_ILIT); e->v.ival=(yyvsp[0].ival); (yyval.node)=e; }
#line 2481 "raku.tab.c"
    break;

  case 119: /* atom: LIT_FLOAT  */
#line 602 "raku.y"
                      { tree_t *e=ast_node_new(TT_FLIT); e->v.dval=(yyvsp[0].dval); (yyval.node)=e; }
#line 2487 "raku.tab.c"
    break;

  case 120: /* atom: LIT_STR  */
#line 603 "raku.y"
                      { (yyval.node)=leaf_sval(TT_QLIT,(yyvsp[0].sval)); }
#line 2493 "raku.tab.c"
    break;

  case 121: /* atom: LIT_INTERP_STR  */
#line 604 "raku.y"
                      { (yyval.node)=lower_interp_str((yyvsp[0].sval)); }
#line 2499 "raku.tab.c"
    break;

  case 122: /* atom: VAR_SCALAR  */
#line 605 "raku.y"
                      { (yyval.node)=var_node((yyvsp[0].sval)); }
#line 2505 "raku.tab.c"
    break;

  case 123: /* atom: VAR_ARRAY  */
#line 606 "raku.y"
                      { (yyval.node)=var_node((yyvsp[0].sval)); }
#line 2511 "raku.tab.c"
    break;

  case 124: /* atom: VAR_HASH  */
#line 607 "raku.y"
                      { (yyval.node)=var_node((yyvsp[0].sval)); }
#line 2517 "raku.tab.c"
    break;

  case 125: /* atom: VAR_CAPTURE  */
#line 609 "raku.y"
        {
          tree_t *c=make_call("raku_capture");
          tree_t *idx=ast_node_new(TT_ILIT); idx->v.ival=(yyvsp[0].ival);
          expr_add_child(c,idx); (yyval.node)=c; }
#line 2526 "raku.tab.c"
    break;

  case 126: /* atom: VAR_NAMED_CAPTURE  */
#line 614 "raku.y"
        {
          tree_t *c=make_call("raku_named_capture");
          expr_add_child(c,leaf_sval(TT_QLIT,(yyvsp[0].sval))); (yyval.node)=c; }
#line 2534 "raku.tab.c"
    break;

  case 127: /* atom: VAR_ARRAY '[' expr ']'  */
#line 618 "raku.y"
        { tree_t *c=make_call("arr_get"); expr_add_child(c,var_node((yyvsp[-3].sval))); expr_add_child(c,(yyvsp[-1].node)); (yyval.node)=c; }
#line 2540 "raku.tab.c"
    break;

  case 128: /* atom: VAR_HASH '<' IDENT '>'  */
#line 620 "raku.y"
        { tree_t *c=make_call("hash_get"); expr_add_child(c,var_node((yyvsp[-3].sval))); expr_add_child(c,leaf_sval(TT_QLIT,(yyvsp[-1].sval))); (yyval.node)=c; }
#line 2546 "raku.tab.c"
    break;

  case 129: /* atom: VAR_HASH '{' expr '}'  */
#line 622 "raku.y"
        { tree_t *c=make_call("hash_get"); expr_add_child(c,var_node((yyvsp[-3].sval))); expr_add_child(c,(yyvsp[-1].node)); (yyval.node)=c; }
#line 2552 "raku.tab.c"
    break;

  case 130: /* atom: KW_EXISTS VAR_HASH '<' IDENT '>'  */
#line 624 "raku.y"
        { tree_t *c=make_call("hash_exists"); expr_add_child(c,var_node((yyvsp[-3].sval))); expr_add_child(c,leaf_sval(TT_QLIT,(yyvsp[-1].sval))); (yyval.node)=c; }
#line 2558 "raku.tab.c"
    break;

  case 131: /* atom: KW_EXISTS VAR_HASH '{' expr '}'  */
#line 626 "raku.y"
        { tree_t *c=make_call("hash_exists"); expr_add_child(c,var_node((yyvsp[-3].sval))); expr_add_child(c,(yyvsp[-1].node)); (yyval.node)=c; }
#line 2564 "raku.tab.c"
    break;

  case 132: /* atom: IDENT  */
#line 627 "raku.y"
                      { (yyval.node)=var_node((yyvsp[0].sval)); }
#line 2570 "raku.tab.c"
    break;

  case 133: /* atom: VAR_TWIGIL  */
#line 629 "raku.y"
        { tree_t *fe=ast_node_new(TT_FIELD);
          fe->v.sval=(char*)intern((yyvsp[0].sval)); free((yyvsp[0].sval));
          expr_add_child(fe, leaf_sval(TT_VAR, "self"));
          (yyval.node)=fe; }
#line 2579 "raku.tab.c"
    break;

  case 134: /* atom: '(' expr ')'  */
#line 633 "raku.y"
                      { (yyval.node)=(yyvsp[-1].node); }
#line 2585 "raku.tab.c"
    break;


#line 2589 "raku.tab.c"

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
  yytoken = yychar == RAKU_YYEMPTY ? YYSYMBOL_YYEMPTY : YYTRANSLATE (yychar);
  /* If not already recovering from an error, report this error.  */
  if (!yyerrstatus)
    {
      ++yynerrs;
      yyerror (YY_("syntax error"));
    }

  if (yyerrstatus == 3)
    {
      /* If just tried and failed to reuse lookahead token after an
         error, discard it.  */

      if (yychar <= RAKU_YYEOF)
        {
          /* Return failure if at end of input.  */
          if (yychar == RAKU_YYEOF)
            YYABORT;
        }
      else
        {
          yydestruct ("Error: discarding",
                      yytoken, &yylval);
          yychar = RAKU_YYEMPTY;
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
                  YY_ACCESSING_SYMBOL (yystate), yyvsp);
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
  yyerror (YY_("memory exhausted"));
  yyresult = 2;
  goto yyreturnlab;


/*----------------------------------------------------------.
| yyreturnlab -- parsing is finished, clean up and return.  |
`----------------------------------------------------------*/
yyreturnlab:
  if (yychar != RAKU_YYEMPTY)
    {
      /* Make sure we have latest lookahead translation.  See comments at
         user semantic actions for why this is necessary.  */
      yytoken = YYTRANSLATE (yychar);
      yydestruct ("Cleanup: discarding lookahead",
                  yytoken, &yylval);
    }
  /* Do not reclaim the symbols of the rule whose action triggered
     this YYABORT or YYACCEPT.  */
  YYPOPSTACK (yylen);
  YY_STACK_PRINT (yyss, yyssp);
  while (yyssp != yyss)
    {
      yydestruct ("Cleanup: popping",
                  YY_ACCESSING_SYMBOL (+*yyssp), yyvsp);
      YYPOPSTACK (1);
    }
#ifndef yyoverflow
  if (yyss != yyssa)
    YYSTACK_FREE (yyss);
#endif

  return yyresult;
}

#line 635 "raku.y"

/*----------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------*/
extern void *raku_yy_scan_string(const char *);
/*----------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------*/
extern void  raku_yy_delete_buffer(void *);
/*----------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------*/
tree_t *raku_parse_string(const char *src) {
    raku_prog_result = NULL;
    void *buf = raku_yy_scan_string(src);
    raku_yyparse();
    raku_yy_delete_buffer(buf);
    return raku_prog_result;
}
