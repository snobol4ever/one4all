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
#line 21 "raku.y"

#include "../../ir/ast.h"
#include "../snobol4/scrip_cc.h"
#include "raku.tab.h"   /* pulls in ExprList from %code requires */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

extern int  raku_yylex(void);
extern int  raku_get_lineno(void);
void raku_yyerror(const char *msg) {
    fprintf(stderr, "raku parse error line %d: %s\n", raku_get_lineno(), msg);
}

/*--------------------------------------------------------------------
 * ExprList helpers
 *--------------------------------------------------------------------*/
static ExprList *exprlist_new(void) {
    ExprList *l = calloc(1, sizeof *l);
    if (!l) { fprintf(stderr, "raku: OOM\n"); exit(1); }
    return l;
}
static ExprList *exprlist_append(ExprList *l, AST_t *e) {
    if (l->count >= l->cap) {
        l->cap = l->cap ? l->cap * 2 : 8;
        l->items = realloc(l->items, l->cap * sizeof(AST_t *));
        if (!l->items) { fprintf(stderr, "raku: OOM\n"); exit(1); }
    }
    l->items[l->count++] = e;
    return l;
}
static void exprlist_free(ExprList *l) { if (l) { free(l->items); free(l); } }

/*--------------------------------------------------------------------
 * Build helpers (logic from raku_lower.c, inlined for direct IR)
 *--------------------------------------------------------------------*/
static const char *strip_sigil(const char *s) {
    if (s && (s[0]=='$'||s[0]=='@'||s[0]=='%')) return s+1;
    return s;
}
static AST_t *leaf_sval(AST_e k, const char *s) {
    AST_t *e = expr_new(k); e->sval = intern(s); return e;
}
static AST_t *var_node(const char *name) {
    return leaf_sval(AST_VAR, strip_sigil(name));
}
/* make_call: AST_FNC + children[0]=AST_VAR(name) for icn_interp_eval layout */
static AST_t *make_call(const char *name) {
    AST_t *e = leaf_sval(AST_FNC, name);
    AST_t *n = expr_new(AST_VAR); n->sval = intern(name);
    expr_add_child(e, n);
    return e;
}
/* make_seq: ExprList → AST_SEQ_EXPR, frees list */
static AST_t *make_seq(ExprList *stmts) {
    AST_t *seq = expr_new(AST_SEQ_EXPR);
    if (stmts) {
        for (int i = 0; i < stmts->count; i++) expr_add_child(seq, stmts->items[i]);
        exprlist_free(stmts);
    }
    return seq;
}
/* lower_interp_str: "hello $var" → left-associative AST_CAT chain */
static AST_t *lower_interp_str(const char *s) {
    int len = s ? (int)strlen(s) : 0;
    AST_t *result = NULL;
    char litbuf[4096]; int litpos = 0, i = 0;
    while (i < len) {
        if (s[i]=='$' && i+1<len &&
            (s[i+1]=='_'||(s[i+1]>='A'&&s[i+1]<='Z')||(s[i+1]>='a'&&s[i+1]<='z'))) {
            if (litpos>0) { litbuf[litpos]='\0';
                AST_t *lit=leaf_sval(AST_QLIT,litbuf);
                result=result?expr_binary(AST_CAT,result,lit):lit; litpos=0; }
            i++;
            char vname[256]; int vlen=0;
            while (i<len&&(s[i]=='_'||(s[i]>='A'&&s[i]<='Z')||(s[i]>='a'&&s[i]<='z')||(s[i]>='0'&&s[i]<='9')))
                { if(vlen<255) vname[vlen++]=s[i]; i++; }
            vname[vlen]='\0';
            AST_t *var=leaf_sval(AST_VAR,vname);
            result=result?expr_binary(AST_CAT,result,var):var;
        } else { if(litpos<4095) litbuf[litpos++]=s[i]; i++; }
    }
    if (litpos>0) { litbuf[litpos]='\0';
        AST_t *lit=leaf_sval(AST_QLIT,litbuf);
        result=result?expr_binary(AST_CAT,result,lit):lit; }
    return result ? result : leaf_sval(AST_QLIT,"");
}
/* make_for_range: for lo..hi -> $v body → explicit while-loop */
static AST_t *make_for_range(AST_t *lo, AST_t *hi, const char *vname, AST_t *body_seq) {
    AST_t *init = expr_binary(AST_ASSIGN, leaf_sval(AST_VAR,vname), lo);
    AST_t *cond = expr_binary(AST_LE, leaf_sval(AST_VAR,vname), hi);
    AST_t *one  = expr_new(AST_ILIT); one->ival = 1;
    AST_t *incr = expr_binary(AST_ADD, leaf_sval(AST_VAR,vname), one);
    expr_add_child(body_seq, expr_binary(AST_ASSIGN, leaf_sval(AST_VAR,vname), incr));
    AST_t *wloop = expr_binary(AST_WHILE, cond, body_seq);
    AST_t *seq   = expr_new(AST_SEQ_EXPR);
    expr_add_child(seq, init); expr_add_child(seq, wloop);
    return seq;
}

/*--------------------------------------------------------------------
 * CODE_t output
 *--------------------------------------------------------------------*/
CODE_t *raku_prog_result = NULL;

static void add_proc(AST_t *e) {
    if (!e) return;
    if (!raku_prog_result) raku_prog_result = calloc(1, sizeof(CODE_t));
    STMT_t *st = calloc(1, sizeof(STMT_t));
    st->subject = e; st->lineno = 0; st->lang = LANG_RAKU;
    if (!raku_prog_result->head) raku_prog_result->head = raku_prog_result->tail = st;
    else { raku_prog_result->tail->next = st; raku_prog_result->tail = st; }
    raku_prog_result->nstmts++;
}

/* SUB_TAG: sentinel bit to distinguish sub defs from body stmts in stmt_list */
#define SUB_TAG 0x40000000

/* RK-26: Raku method table — maps "ClassName::method" → AST_FNC proc name */
#define RAKU_METH_MAX 256
typedef struct { char key[128]; char procname[128]; } RakuMethEntry;
static RakuMethEntry raku_meth_table[RAKU_METH_MAX];
static int           raku_meth_ntypes = 0;

static void raku_meth_register(const char *classname, const char *methname, const char *procname) {
    if (raku_meth_ntypes >= RAKU_METH_MAX) return;
    RakuMethEntry *e = &raku_meth_table[raku_meth_ntypes++];
    snprintf(e->key,      sizeof e->key,      "%s::%s", classname, methname);
    snprintf(e->procname, sizeof e->procname,  "%s",     procname);
}

/* Emit the extern declaration so interp.c can call raku_meth_lookup */
const char *raku_meth_lookup(const char *classname, const char *methname) {
    char key[128];
    snprintf(key, sizeof key, "%s::%s", classname, methname);
    for (int i = 0; i < raku_meth_ntypes; i++)
        if (strcmp(raku_meth_table[i].key, key) == 0)
            return raku_meth_table[i].procname;
    return NULL;
}



#line 223 "raku.tab.c"

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
#define YYLAST   687

/* YYNTOKENS -- Number of terminals.  */
#define YYNTOKENS  82
/* YYNNTS -- Number of nonterminals.  */
#define YYNNTS  29
/* YYNRULES -- Number of rules.  */
#define YYNRULES  132
/* YYNSTATES -- Number of states.  */
#define YYNSTATES  320

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
       0,   219,   219,   249,   250,   254,   256,   258,   261,   263,
     265,   267,   269,   271,   273,   275,   278,   280,   283,   285,
     287,   289,   292,   297,   300,   303,   306,   309,   312,   313,
     314,   315,   316,   318,   321,   324,   325,   326,   327,   328,
     332,   334,   336,   341,   347,   349,   355,   361,   367,   380,
     386,   401,   419,   420,   429,   437,   449,   482,   483,   486,
     489,   500,   512,   516,   523,   524,   528,   533,   537,   538,
     561,   565,   566,   567,   568,   569,   570,   571,   572,   573,
     574,   575,   581,   587,   593,   597,   598,   599,   603,   604,
     605,   606,   610,   611,   612,   613,   614,   618,   619,   620,
     623,   626,   631,   633,   639,   644,   651,   657,   664,   667,
     670,   673,   676,   679,   683,   684,   688,   689,   690,   691,
     692,   693,   694,   695,   700,   704,   706,   708,   710,   712,
     714,   716,   721
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

#define YYPACT_NINF (-50)

#define yypact_value_is_default(Yyn) \
  ((Yyn) == YYPACT_NINF)

#define YYTABLE_NINF (-1)

#define yytable_value_is_error(Yyn) \
  0

/* YYPACT[STATE-NUM] -- Index in YYTABLE of the portion describing
   STATE-NUM.  */
static const yytype_int16 yypact[] =
{
     -50,    27,   411,   -50,   -50,   -50,   -50,   -50,   -27,   -37,
     -49,   -50,    39,   -50,   -50,    42,   492,   532,   -39,   -20,
     572,    36,    35,   572,   452,   572,    48,    59,    38,    52,
      35,    73,    73,    92,    35,   572,   144,   612,   612,   572,
     -50,   -50,   -50,   -50,   -50,   -50,   -50,   -50,   -50,   -50,
      85,   -25,   -50,    84,    -3,   -50,   -50,   -50,    88,   572,
     148,   572,   149,   572,   118,   226,   103,   104,   106,    71,
     107,    93,   -46,   572,    96,   572,    99,   572,   572,   -36,
     100,   -50,   -50,   102,   -50,   108,    94,   -43,   -42,   572,
     572,   -50,   572,   572,   572,   572,   -50,   136,   -50,    98,
     -50,   -50,   -50,   109,   -50,   612,   612,   612,   612,   612,
     612,   612,   612,   612,   612,    83,   612,   612,   612,   612,
     612,   612,   612,   612,   612,   166,   111,   119,   113,   122,
     114,   115,   -50,   -50,   -13,   572,   572,   572,   -34,   -32,
     -23,   572,   572,   169,   572,     8,   -50,    17,   -50,   117,
     120,   178,   -50,    -5,   302,   -50,   -50,   -50,   182,   572,
     184,   572,   125,   129,   127,   -50,   -50,   -50,    35,   -50,
     -50,    33,    33,    33,    33,    33,    33,    33,    33,    33,
      33,   -50,   -50,   -50,    33,    33,    -3,    -3,    -3,   -50,
     -50,   -50,   -50,   126,   -50,   572,   147,   150,   151,    -8,
     572,   -50,   137,   140,   143,   572,   -50,   572,   -50,   572,
     -50,   -50,   131,   154,   142,   572,   572,    35,    35,    35,
     -50,    35,    40,   -50,    -6,   159,   145,   160,   146,    35,
      35,   -50,   -50,   -33,   343,   161,   572,   572,   572,   181,
     -50,    46,   -50,   -50,   -50,   -50,   172,   174,   175,   -50,
     -50,   -50,   156,   157,   222,   -50,   -50,   -50,   240,    35,
     572,    35,   -50,   -50,   -50,   177,   179,   233,   -50,   241,
      16,   -50,   -50,    65,   -50,   183,   185,   186,   572,   242,
     -50,   -50,   -50,   -50,   187,   192,   -10,   -50,   -50,    35,
     188,   -50,   -50,    35,   195,   193,   197,   -50,   -50,   -50,
     -50,   -50,   227,   -50,   -50,   -50,   -50,   -50,   -50,   -50,
       3,   -50,   -50,   572,    35,    70,   -50,   -50,    35,   -50
};

/* YYDEFACT[STATE-NUM] -- Default reduction number in state STATE-NUM.
   Performed when YYTABLE does not specify something else to do.  Zero
   means the default is an error.  */
static const yytype_uint8 yydefact[] =
{
       3,     0,     2,     1,   116,   117,   118,   119,   120,   121,
     122,   131,   130,   123,   124,     0,     0,     0,     0,     0,
       0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
       0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
       4,    29,    30,    35,    36,    37,    31,    32,    38,    39,
       0,    70,    84,    87,    91,    96,    99,   100,   113,     0,
       0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
     120,   121,   122,     0,     0,     0,     0,     0,     0,     0,
       0,     3,    69,     0,    20,     0,     0,     0,     0,     0,
       0,    47,     0,     0,     0,     0,   111,    33,   108,     0,
     120,    98,    97,     0,    28,     0,     0,     0,     0,     0,
       0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
       0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
       0,     0,   102,   114,     0,     0,     0,     0,     0,     0,
       0,     0,     0,     0,     0,     0,    14,     0,    16,     0,
       0,     0,    49,     0,     0,    18,    19,    52,     0,     0,
       0,     0,     0,     0,     0,   109,   110,   112,     0,    57,
     132,    71,    72,    85,    86,    73,    74,    77,    78,    79,
      80,    81,    82,    83,    75,    76,    90,    88,    89,    95,
      92,    93,    94,   107,    21,     0,   125,   126,   127,     0,
       0,   101,     0,     0,     0,     0,    11,     0,    12,     0,
      13,    68,     0,     0,     0,     0,     0,     0,     0,     0,
      64,     0,     0,    66,     0,     0,     0,     0,     0,     0,
       0,    67,    34,     0,     0,     0,     0,     0,     0,     0,
     104,     0,   115,     5,     6,     7,     0,     0,     0,   125,
     126,   127,     0,     0,    40,    43,    48,    55,     0,     0,
       0,     0,    50,   128,   129,     0,     0,    44,    46,     0,
       0,    56,   106,     0,    22,     0,     0,     0,     0,     0,
     103,     8,     9,    10,     0,     0,     0,    65,    54,     0,
       0,    26,    27,     0,     0,     0,     0,   105,    23,    24,
      25,    62,     0,    15,    17,    42,    41,    53,    51,    45,
       0,    59,    58,     0,     0,     0,    63,    61,     0,    60
};

/* YYPGOTO[NTERM-NUM].  */
static const yytype_int16 yypgoto[] =
{
     -50,   -50,   196,   -50,    -4,   -50,   -50,   -50,   -50,   -50,
     -50,   -50,   -50,   -50,   -50,   -50,   -30,   -14,   124,   -16,
     -50,   -50,   281,    -9,   -35,   -50,   -50,    44,   -50
};

/* YYDEFGOTO[NTERM-NUM].  */
static const yytype_uint8 yydefgoto[] =
{
       0,     1,     2,    40,    41,    42,    43,    44,    45,    46,
      47,   224,    48,    49,   233,   241,   222,    82,    93,    50,
      51,    52,    53,    54,    55,    56,    57,   134,    58
};

/* YYTABLE[YYPACT[STATE-NUM]] -- What to do in state STATE-NUM.  If
   positive, shift that token.  If negative, reduce the rule whose
   number is the opposite.  If YYTABLE_NINF, syntax error.  */
static const yytype_int16 yytable[] =
{
      74,    76,   101,   102,    79,   220,   239,    83,    85,    86,
      18,   269,   270,   220,   151,    62,    91,    96,   143,    98,
      97,   158,   160,   103,   260,   261,   295,     3,   205,   296,
     207,    63,   105,   106,   144,    59,    77,   159,   161,   209,
     206,    61,   208,   126,    81,   128,    60,   130,   271,   133,
      80,   210,    66,    67,    68,    78,    69,   145,   121,   147,
      87,   149,   150,   200,   201,   152,   122,   123,   124,   240,
      81,    88,   221,   162,   163,   262,   164,   165,   166,   167,
     314,   138,   139,   140,   215,   170,   189,   190,   191,   192,
     181,   182,   183,   216,   170,     4,     5,     6,     7,   118,
     119,   120,    70,    71,    72,    11,    12,    13,    14,   186,
     187,   188,    64,    89,    65,    81,   258,   259,    22,   202,
     203,   204,   279,   280,    26,   211,   212,    90,   214,    31,
      32,    33,   107,   108,    35,   109,   110,   111,   112,   113,
     114,   200,   297,   226,   115,   228,   258,   318,   116,   117,
     118,   119,   120,    92,   232,    37,    94,    95,    99,   104,
      38,   125,   127,   129,   131,   135,   136,    39,   137,   141,
     146,   142,    92,   148,   157,   153,   155,   168,   169,   235,
     193,   195,   156,   213,   242,   194,   170,   197,   219,   246,
     199,   247,   196,   248,   217,   198,   225,   218,   227,   252,
     253,   234,   229,   254,   255,   256,   230,   257,   231,   236,
     249,   243,   237,   238,   244,   267,   268,   245,   133,   250,
     275,   276,   277,   251,   263,   265,   264,   266,   278,     4,
       5,     6,     7,   284,   285,   274,    70,    71,    72,    11,
      12,    13,    14,   286,   289,   288,   281,   290,   282,   283,
     287,   291,    22,   292,   293,   294,   302,   298,    26,   299,
     300,   303,   301,    31,    32,    33,   304,   311,    35,   308,
     310,   312,   306,     0,   313,   307,     0,   154,   273,   309,
     315,     0,   305,     0,     0,     0,     0,     0,     0,    37,
       0,     0,     0,     0,    38,     0,     0,   316,     0,     0,
     317,    39,     0,   132,   319,     4,     5,     6,     7,     0,
       0,     0,     8,     9,    10,    11,    12,    13,    14,    15,
      16,    17,    18,     0,     0,    19,    20,    21,    22,    23,
      24,    25,     0,     0,    26,    27,    28,    29,    30,    31,
      32,    33,    34,     0,    35,    36,     4,     5,     6,     7,
       0,     0,     0,    70,    71,    72,    11,    12,    13,    14,
       0,     0,     0,     0,     0,    37,     0,     0,     0,    22,
      38,     0,     0,     0,     0,    26,     0,    39,     0,     0,
      31,    32,    33,   223,     0,    35,   171,   172,   173,   174,
     175,   176,   177,   178,   179,   180,     0,   184,   185,     0,
       0,     0,     0,     0,     0,     0,    37,     0,     0,     0,
       0,    38,     0,     0,     4,     5,     6,     7,    39,     0,
     272,     8,     9,    10,    11,    12,    13,    14,    15,    16,
      17,    18,     0,     0,    19,    20,    21,    22,    23,    24,
      25,     0,     0,    26,    27,    28,    29,    30,    31,    32,
      33,    34,     0,    35,    36,     4,     5,     6,     7,     0,
       0,     0,    70,    71,    72,    11,    12,    13,    14,     0,
       0,     0,     0,     0,    37,     0,     0,     0,    22,    38,
       0,     0,     0,     0,    26,     0,    39,     0,     0,    31,
      32,    33,     0,     0,    35,     4,     5,     6,     7,     0,
       0,     0,    70,    71,    72,    11,    12,    13,    14,     0,
       0,     0,     0,     0,     0,    37,     0,     0,    22,     0,
      38,     0,     0,     0,    26,     0,    84,    39,     0,    31,
      32,    33,     0,     0,    35,     4,     5,     6,     7,     0,
       0,     0,    70,    71,    72,    11,    12,    13,    14,     0,
       0,     0,     0,     0,     0,    37,     0,     0,    22,     0,
      38,     0,     0,     0,    26,     0,     0,    73,     0,    31,
      32,    33,     0,     0,    35,     4,     5,     6,     7,     0,
       0,     0,    70,    71,    72,    11,    12,    13,    14,     0,
       0,     0,     0,     0,     0,    37,     0,     0,    22,     0,
      38,     0,     0,     0,    26,     0,     0,    75,     0,    31,
      32,    33,     0,     0,    35,     4,     5,     6,     7,     0,
       0,     0,   100,    71,    72,    11,    12,    13,    14,     0,
       0,     0,     0,     0,     0,    37,     0,     0,     0,     0,
      38,     0,     0,     0,    26,     0,     0,    39,     0,    31,
      32,    33,     0,     0,    35,     0,     0,     0,     0,     0,
       0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
       0,     0,     0,     0,     0,    37,     0,     0,     0,     0,
      38,     0,     0,     0,     0,     0,     0,    39
};

static const yytype_int16 yycheck[] =
{
      16,    17,    37,    38,    20,    10,    14,    23,    24,    25,
      20,    44,    45,    10,    50,    64,    30,    33,    64,    35,
      34,    64,    64,    39,    30,    31,    10,     0,    62,    13,
      62,    80,    57,    58,    80,    62,    75,    80,    80,    62,
      74,    78,    74,    59,    80,    61,    73,    63,    81,    65,
      14,    74,    10,    11,    12,    75,    14,    73,    61,    75,
      12,    77,    78,    76,    77,    79,    69,    70,    71,    77,
      80,    12,    77,    89,    90,    81,    92,    93,    94,    95,
      77,    10,    11,    12,    76,    77,   121,   122,   123,   124,
       7,     8,     9,    76,    77,     3,     4,     5,     6,    66,
      67,    68,    10,    11,    12,    13,    14,    15,    16,   118,
     119,   120,    73,    75,    75,    80,    76,    77,    26,   135,
     136,   137,    76,    77,    32,   141,   142,    75,   144,    37,
      38,    39,    48,    49,    42,    51,    52,    53,    54,    55,
      56,    76,    77,   159,    60,   161,    76,    77,    64,    65,
      66,    67,    68,    80,   168,    63,    32,    33,    14,    74,
      68,    73,    14,    14,    46,    62,    62,    75,    62,    62,
      74,    78,    80,    74,    80,    75,    74,    41,    80,   195,
      14,    62,    74,    14,   200,    74,    77,    65,    10,   205,
      75,   207,    79,   209,    77,    81,    14,    77,    14,   215,
     216,    75,    77,   217,   218,   219,    77,   221,    81,    62,
      79,    74,    62,    62,    74,   229,   230,    74,   234,    65,
     236,   237,   238,    81,    65,    65,    81,    81,    47,     3,
       4,     5,     6,    77,    77,    74,    10,    11,    12,    13,
      14,    15,    16,    21,   260,   259,    74,   261,    74,    74,
      10,    74,    26,    74,    21,    14,    14,    74,    32,    74,
      74,    74,   278,    37,    38,    39,    74,    74,    42,    81,
      75,    74,   286,    -1,    47,   289,    -1,    81,   234,   293,
     310,    -1,   286,    -1,    -1,    -1,    -1,    -1,    -1,    63,
      -1,    -1,    -1,    -1,    68,    -1,    -1,   313,    -1,    -1,
     314,    75,    -1,    77,   318,     3,     4,     5,     6,    -1,
      -1,    -1,    10,    11,    12,    13,    14,    15,    16,    17,
      18,    19,    20,    -1,    -1,    23,    24,    25,    26,    27,
      28,    29,    -1,    -1,    32,    33,    34,    35,    36,    37,
      38,    39,    40,    -1,    42,    43,     3,     4,     5,     6,
      -1,    -1,    -1,    10,    11,    12,    13,    14,    15,    16,
      -1,    -1,    -1,    -1,    -1,    63,    -1,    -1,    -1,    26,
      68,    -1,    -1,    -1,    -1,    32,    -1,    75,    -1,    -1,
      37,    38,    39,    81,    -1,    42,   105,   106,   107,   108,
     109,   110,   111,   112,   113,   114,    -1,   116,   117,    -1,
      -1,    -1,    -1,    -1,    -1,    -1,    63,    -1,    -1,    -1,
      -1,    68,    -1,    -1,     3,     4,     5,     6,    75,    -1,
      77,    10,    11,    12,    13,    14,    15,    16,    17,    18,
      19,    20,    -1,    -1,    23,    24,    25,    26,    27,    28,
      29,    -1,    -1,    32,    33,    34,    35,    36,    37,    38,
      39,    40,    -1,    42,    43,     3,     4,     5,     6,    -1,
      -1,    -1,    10,    11,    12,    13,    14,    15,    16,    -1,
      -1,    -1,    -1,    -1,    63,    -1,    -1,    -1,    26,    68,
      -1,    -1,    -1,    -1,    32,    -1,    75,    -1,    -1,    37,
      38,    39,    -1,    -1,    42,     3,     4,     5,     6,    -1,
      -1,    -1,    10,    11,    12,    13,    14,    15,    16,    -1,
      -1,    -1,    -1,    -1,    -1,    63,    -1,    -1,    26,    -1,
      68,    -1,    -1,    -1,    32,    -1,    74,    75,    -1,    37,
      38,    39,    -1,    -1,    42,     3,     4,     5,     6,    -1,
      -1,    -1,    10,    11,    12,    13,    14,    15,    16,    -1,
      -1,    -1,    -1,    -1,    -1,    63,    -1,    -1,    26,    -1,
      68,    -1,    -1,    -1,    32,    -1,    -1,    75,    -1,    37,
      38,    39,    -1,    -1,    42,     3,     4,     5,     6,    -1,
      -1,    -1,    10,    11,    12,    13,    14,    15,    16,    -1,
      -1,    -1,    -1,    -1,    -1,    63,    -1,    -1,    26,    -1,
      68,    -1,    -1,    -1,    32,    -1,    -1,    75,    -1,    37,
      38,    39,    -1,    -1,    42,     3,     4,     5,     6,    -1,
      -1,    -1,    10,    11,    12,    13,    14,    15,    16,    -1,
      -1,    -1,    -1,    -1,    -1,    63,    -1,    -1,    -1,    -1,
      68,    -1,    -1,    -1,    32,    -1,    -1,    75,    -1,    37,
      38,    39,    -1,    -1,    42,    -1,    -1,    -1,    -1,    -1,
      -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,
      -1,    -1,    -1,    -1,    -1,    63,    -1,    -1,    -1,    -1,
      68,    -1,    -1,    -1,    -1,    -1,    -1,    75
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
      14,    80,    99,   101,    74,   101,   101,    12,    12,    75,
      75,    99,    80,   100,   100,   100,   101,    99,   101,    14,
      10,   106,   106,   101,    74,    57,    58,    48,    49,    51,
      52,    53,    54,    55,    56,    60,    64,    65,    66,    67,
      68,    61,    69,    70,    71,    73,   101,    14,   101,    14,
     101,    46,    77,   101,   109,    62,    62,    62,    10,    11,
      12,    62,    78,    64,    80,   101,    74,   101,    74,   101,
     101,    50,    99,    75,    84,    74,    74,    80,    64,    80,
      64,    80,   101,   101,   101,   101,   101,   101,    41,    80,
      77,   104,   104,   104,   104,   104,   104,   104,   104,   104,
     104,     7,     8,     9,   104,   104,   105,   105,   105,   106,
     106,   106,   106,    14,    74,    62,    79,    65,    81,    75,
      76,    77,   101,   101,   101,    62,    74,    62,    74,    62,
      74,   101,   101,    14,   101,    76,    76,    77,    77,    10,
      10,    77,    98,    81,    93,    14,   101,    14,   101,    77,
      77,    81,    99,    96,    75,   101,    62,    62,    62,    14,
      77,    97,   101,    74,    74,    74,   101,   101,   101,    79,
      65,    81,   101,   101,    99,    99,    99,    99,    76,    77,
      30,    31,    81,    65,    81,    65,    81,    99,    99,    44,
      45,    81,    77,   109,    74,   101,   101,   101,    47,    76,
      77,    74,    74,    74,    77,    77,    21,    10,    99,   101,
      99,    74,    74,    21,    14,    10,    13,    77,    74,    74,
      74,   101,    14,    74,    74,    86,    99,    99,    81,    99,
      75,    74,    74,    47,    77,    98,   101,    99,    77,    99
};

/* YYR1[RULE-NUM] -- Symbol kind of the left-hand side of rule RULE-NUM.  */
static const yytype_int8 yyr1[] =
{
       0,    82,    83,    84,    84,    85,    85,    85,    85,    85,
      85,    85,    85,    85,    85,    85,    85,    85,    85,    85,
      85,    85,    85,    85,    85,    85,    85,    85,    85,    85,
      85,    85,    85,    85,    85,    85,    85,    85,    85,    85,
      86,    86,    86,    87,    88,    88,    89,    90,    91,    91,
      92,    92,    93,    93,    94,    94,    95,    96,    96,    96,
      96,    96,    97,    97,    98,    98,    99,   100,   101,   101,
     101,   102,   102,   102,   102,   102,   102,   102,   102,   102,
     102,   102,   102,   102,   102,   103,   103,   103,   104,   104,
     104,   104,   105,   105,   105,   105,   105,   106,   106,   106,
     107,   108,   108,   108,   108,   108,   108,   108,   108,   108,
     108,   108,   108,   108,   109,   109,   110,   110,   110,   110,
     110,   110,   110,   110,   110,   110,   110,   110,   110,   110,
     110,   110,   110
};

/* YYR2[RULE-NUM] -- Number of symbols on the right-hand side of rule RULE-NUM.  */
static const yytype_int8 yyr2[] =
{
       0,     2,     1,     0,     2,     5,     5,     5,     6,     6,
       6,     4,     4,     4,     3,     7,     3,     7,     3,     3,
       2,     4,     6,     7,     7,     7,     6,     6,     2,     1,
       1,     1,     1,     2,     4,     1,     1,     1,     1,     1,
       5,     7,     7,     5,     5,     7,     5,     2,     5,     3,
       5,     7,     0,     4,     6,     5,     5,     0,     4,     4,
       7,     6,     3,     5,     1,     3,     3,     3,     3,     2,
       1,     3,     3,     3,     3,     3,     3,     3,     3,     3,
       3,     3,     3,     3,     1,     3,     3,     1,     3,     3,
       3,     1,     3,     3,     3,     3,     1,     2,     2,     1,
       1,     4,     3,     6,     5,     6,     5,     3,     2,     3,
       3,     2,     3,     1,     1,     3,     1,     1,     1,     1,
       1,     1,     1,     1,     1,     4,     4,     4,     5,     5,
       1,     1,     3
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
#line 220 "raku.y"
        {
            ExprList *all = (yyvsp[0].list);
            /* Partition: subs (ival & SUB_TAG) vs body stmts */
            if (all) {
                /* Pass 1: emit sub defs */
                for (int i = 0; i < all->count; i++) {
                    AST_t *e = all->items[i];
                    if (!e || !(e->kind==AST_FNC && (e->ival & SUB_TAG))) continue;
                    e->ival &= ~SUB_TAG;   /* restore real nparams */
                    add_proc(e);
                    all->items[i] = NULL;  /* mark consumed */
                }
                /* Pass 2: wrap remaining body stmts in synthetic "main" AST_FNC */
                int has_body = 0;
                for (int i = 0; i < all->count; i++) if (all->items[i]) { has_body=1; break; }
                if (has_body) {
                    AST_t *mf = leaf_sval(AST_FNC, "main"); mf->ival = 0;
                    AST_t *mn = expr_new(AST_VAR); mn->sval = intern("main");
                    expr_add_child(mf, mn);
                    for (int i = 0; i < all->count; i++)
                        if (all->items[i]) expr_add_child(mf, all->items[i]);
                    add_proc(mf);
                }
                exprlist_free(all);
            }
        }
#line 1626 "raku.tab.c"
    break;

  case 3: /* stmt_list: %empty  */
#line 249 "raku.y"
                     { (yyval.list) = exprlist_new(); }
#line 1632 "raku.tab.c"
    break;

  case 4: /* stmt_list: stmt_list stmt  */
#line 250 "raku.y"
                     { (yyval.list) = exprlist_append((yyvsp[-1].list), (yyvsp[0].node)); }
#line 1638 "raku.tab.c"
    break;

  case 5: /* stmt: KW_MY VAR_SCALAR '=' expr ';'  */
#line 255 "raku.y"
        { (yyval.node) = expr_binary(AST_ASSIGN, var_node((yyvsp[-3].sval)), (yyvsp[-1].node)); }
#line 1644 "raku.tab.c"
    break;

  case 6: /* stmt: KW_MY VAR_ARRAY '=' expr ';'  */
#line 257 "raku.y"
        { (yyval.node) = expr_binary(AST_ASSIGN, var_node((yyvsp[-3].sval)), (yyvsp[-1].node)); }
#line 1650 "raku.tab.c"
    break;

  case 7: /* stmt: KW_MY VAR_HASH '=' expr ';'  */
#line 259 "raku.y"
        { (yyval.node) = expr_binary(AST_ASSIGN, var_node((yyvsp[-3].sval)), (yyvsp[-1].node)); }
#line 1656 "raku.tab.c"
    break;

  case 8: /* stmt: KW_MY IDENT VAR_SCALAR '=' expr ';'  */
#line 262 "raku.y"
        { free((yyvsp[-4].sval)); (yyval.node) = expr_binary(AST_ASSIGN, var_node((yyvsp[-3].sval)), (yyvsp[-1].node)); }
#line 1662 "raku.tab.c"
    break;

  case 9: /* stmt: KW_MY IDENT VAR_ARRAY '=' expr ';'  */
#line 264 "raku.y"
        { free((yyvsp[-4].sval)); (yyval.node) = expr_binary(AST_ASSIGN, var_node((yyvsp[-3].sval)), (yyvsp[-1].node)); }
#line 1668 "raku.tab.c"
    break;

  case 10: /* stmt: KW_MY IDENT VAR_HASH '=' expr ';'  */
#line 266 "raku.y"
        { free((yyvsp[-4].sval)); (yyval.node) = expr_binary(AST_ASSIGN, var_node((yyvsp[-3].sval)), (yyvsp[-1].node)); }
#line 1674 "raku.tab.c"
    break;

  case 11: /* stmt: KW_MY IDENT VAR_SCALAR ';'  */
#line 268 "raku.y"
        { free((yyvsp[-2].sval)); (yyval.node) = expr_binary(AST_ASSIGN, var_node((yyvsp[-1].sval)), leaf_sval(AST_QLIT, "")); }
#line 1680 "raku.tab.c"
    break;

  case 12: /* stmt: KW_MY IDENT VAR_ARRAY ';'  */
#line 270 "raku.y"
        { free((yyvsp[-2].sval)); (yyval.node) = expr_binary(AST_ASSIGN, var_node((yyvsp[-1].sval)), leaf_sval(AST_QLIT, "")); }
#line 1686 "raku.tab.c"
    break;

  case 13: /* stmt: KW_MY IDENT VAR_HASH ';'  */
#line 272 "raku.y"
        { free((yyvsp[-2].sval)); (yyval.node) = expr_binary(AST_ASSIGN, var_node((yyvsp[-1].sval)), leaf_sval(AST_QLIT, "")); }
#line 1692 "raku.tab.c"
    break;

  case 14: /* stmt: KW_SAY expr ';'  */
#line 274 "raku.y"
        { AST_t *c=make_call("write"); expr_add_child(c,(yyvsp[-1].node)); (yyval.node)=c; }
#line 1698 "raku.tab.c"
    break;

  case 15: /* stmt: KW_SAY '(' expr ',' expr ')' ';'  */
#line 276 "raku.y"
        { /* RK-39: say($fh, str) */
          AST_t *c=make_call("raku_say_fh"); expr_add_child(c,(yyvsp[-4].node)); expr_add_child(c,(yyvsp[-2].node)); (yyval.node)=c; }
#line 1705 "raku.tab.c"
    break;

  case 16: /* stmt: KW_PRINT expr ';'  */
#line 279 "raku.y"
        { AST_t *c=make_call("writes"); expr_add_child(c,(yyvsp[-1].node)); (yyval.node)=c; }
#line 1711 "raku.tab.c"
    break;

  case 17: /* stmt: KW_PRINT '(' expr ',' expr ')' ';'  */
#line 281 "raku.y"
        { /* RK-39: print($fh, str) */
          AST_t *c=make_call("raku_print_fh"); expr_add_child(c,(yyvsp[-4].node)); expr_add_child(c,(yyvsp[-2].node)); (yyval.node)=c; }
#line 1718 "raku.tab.c"
    break;

  case 18: /* stmt: KW_TAKE expr ';'  */
#line 284 "raku.y"
        { (yyval.node)=expr_unary(AST_SUSPEND,(yyvsp[-1].node)); }
#line 1724 "raku.tab.c"
    break;

  case 19: /* stmt: KW_RETURN expr ';'  */
#line 286 "raku.y"
        { AST_t *r=expr_new(AST_RETURN); expr_add_child(r,(yyvsp[-1].node)); (yyval.node)=r; }
#line 1730 "raku.tab.c"
    break;

  case 20: /* stmt: KW_RETURN ';'  */
#line 288 "raku.y"
        { (yyval.node)=expr_new(AST_RETURN); }
#line 1736 "raku.tab.c"
    break;

  case 21: /* stmt: VAR_SCALAR '=' expr ';'  */
#line 290 "raku.y"
        { (yyval.node)=expr_binary(AST_ASSIGN,var_node((yyvsp[-3].sval)),(yyvsp[-1].node)); }
#line 1742 "raku.tab.c"
    break;

  case 22: /* stmt: VAR_SCALAR '.' IDENT '=' expr ';'  */
#line 293 "raku.y"
        { AST_t *fe=expr_new(AST_FIELD);
          fe->sval=(char*)intern((yyvsp[-3].sval)); free((yyvsp[-3].sval));
          expr_add_child(fe,var_node((yyvsp[-5].sval)));
          (yyval.node)=expr_binary(AST_ASSIGN,fe,(yyvsp[-1].node)); }
#line 1751 "raku.tab.c"
    break;

  case 23: /* stmt: VAR_ARRAY '[' expr ']' '=' expr ';'  */
#line 298 "raku.y"
        { AST_t *c=make_call("arr_set");
          expr_add_child(c,var_node((yyvsp[-6].sval))); expr_add_child(c,(yyvsp[-4].node)); expr_add_child(c,(yyvsp[-1].node)); (yyval.node)=c; }
#line 1758 "raku.tab.c"
    break;

  case 24: /* stmt: VAR_HASH '<' IDENT '>' '=' expr ';'  */
#line 301 "raku.y"
        { AST_t *c=make_call("hash_set");
          expr_add_child(c,var_node((yyvsp[-6].sval))); expr_add_child(c,leaf_sval(AST_QLIT,(yyvsp[-4].sval))); expr_add_child(c,(yyvsp[-1].node)); (yyval.node)=c; }
#line 1765 "raku.tab.c"
    break;

  case 25: /* stmt: VAR_HASH '{' expr '}' '=' expr ';'  */
#line 304 "raku.y"
        { AST_t *c=make_call("hash_set");
          expr_add_child(c,var_node((yyvsp[-6].sval))); expr_add_child(c,(yyvsp[-4].node)); expr_add_child(c,(yyvsp[-1].node)); (yyval.node)=c; }
#line 1772 "raku.tab.c"
    break;

  case 26: /* stmt: KW_DELETE VAR_HASH '<' IDENT '>' ';'  */
#line 307 "raku.y"
        { AST_t *c=make_call("hash_delete");
          expr_add_child(c,var_node((yyvsp[-4].sval))); expr_add_child(c,leaf_sval(AST_QLIT,(yyvsp[-2].sval))); (yyval.node)=c; }
#line 1779 "raku.tab.c"
    break;

  case 27: /* stmt: KW_DELETE VAR_HASH '{' expr '}' ';'  */
#line 310 "raku.y"
        { AST_t *c=make_call("hash_delete");
          expr_add_child(c,var_node((yyvsp[-4].sval))); expr_add_child(c,(yyvsp[-2].node)); (yyval.node)=c; }
#line 1786 "raku.tab.c"
    break;

  case 28: /* stmt: expr ';'  */
#line 312 "raku.y"
               { (yyval.node)=(yyvsp[-1].node); }
#line 1792 "raku.tab.c"
    break;

  case 29: /* stmt: if_stmt  */
#line 313 "raku.y"
                        { (yyval.node)=(yyvsp[0].node); }
#line 1798 "raku.tab.c"
    break;

  case 30: /* stmt: while_stmt  */
#line 314 "raku.y"
                        { (yyval.node)=(yyvsp[0].node); }
#line 1804 "raku.tab.c"
    break;

  case 31: /* stmt: for_stmt  */
#line 315 "raku.y"
                        { (yyval.node)=(yyvsp[0].node); }
#line 1810 "raku.tab.c"
    break;

  case 32: /* stmt: given_stmt  */
#line 316 "raku.y"
                        { (yyval.node)=(yyvsp[0].node); }
#line 1816 "raku.tab.c"
    break;

  case 33: /* stmt: KW_TRY block  */
#line 319 "raku.y"
        { AST_t *c=make_call("raku_try");
          expr_add_child(c,(yyvsp[0].node)); (yyval.node)=c; }
#line 1823 "raku.tab.c"
    break;

  case 34: /* stmt: KW_TRY block KW_CATCH block  */
#line 322 "raku.y"
        { AST_t *c=make_call("raku_try");
          expr_add_child(c,(yyvsp[-2].node)); expr_add_child(c,(yyvsp[0].node)); (yyval.node)=c; }
#line 1830 "raku.tab.c"
    break;

  case 35: /* stmt: unless_stmt  */
#line 324 "raku.y"
                        { (yyval.node)=(yyvsp[0].node); }
#line 1836 "raku.tab.c"
    break;

  case 36: /* stmt: until_stmt  */
#line 325 "raku.y"
                        { (yyval.node)=(yyvsp[0].node); }
#line 1842 "raku.tab.c"
    break;

  case 37: /* stmt: repeat_stmt  */
#line 326 "raku.y"
                        { (yyval.node)=(yyvsp[0].node); }
#line 1848 "raku.tab.c"
    break;

  case 38: /* stmt: sub_decl  */
#line 327 "raku.y"
                        { (yyval.node)=(yyvsp[0].node); }
#line 1854 "raku.tab.c"
    break;

  case 39: /* stmt: class_decl  */
#line 328 "raku.y"
                        { (yyval.node)=(yyvsp[0].node); }
#line 1860 "raku.tab.c"
    break;

  case 40: /* if_stmt: KW_IF '(' expr ')' block  */
#line 333 "raku.y"
        { AST_t *e=expr_new(AST_IF); expr_add_child(e,(yyvsp[-2].node)); expr_add_child(e,(yyvsp[0].node)); (yyval.node)=e; }
#line 1866 "raku.tab.c"
    break;

  case 41: /* if_stmt: KW_IF '(' expr ')' block KW_ELSE block  */
#line 335 "raku.y"
        { AST_t *e=expr_new(AST_IF); expr_add_child(e,(yyvsp[-4].node)); expr_add_child(e,(yyvsp[-2].node)); expr_add_child(e,(yyvsp[0].node)); (yyval.node)=e; }
#line 1872 "raku.tab.c"
    break;

  case 42: /* if_stmt: KW_IF '(' expr ')' block KW_ELSE if_stmt  */
#line 337 "raku.y"
        { AST_t *e=expr_new(AST_IF); expr_add_child(e,(yyvsp[-4].node)); expr_add_child(e,(yyvsp[-2].node)); expr_add_child(e,(yyvsp[0].node)); (yyval.node)=e; }
#line 1878 "raku.tab.c"
    break;

  case 43: /* while_stmt: KW_WHILE '(' expr ')' block  */
#line 342 "raku.y"
        { (yyval.node)=expr_binary(AST_WHILE,(yyvsp[-2].node),(yyvsp[0].node)); }
#line 1884 "raku.tab.c"
    break;

  case 44: /* unless_stmt: KW_UNLESS '(' expr ')' block  */
#line 348 "raku.y"
        { AST_t *e=expr_new(AST_IF); expr_add_child(e,expr_unary(AST_NOT,(yyvsp[-2].node))); expr_add_child(e,(yyvsp[0].node)); (yyval.node)=e; }
#line 1890 "raku.tab.c"
    break;

  case 45: /* unless_stmt: KW_UNLESS '(' expr ')' block KW_ELSE block  */
#line 350 "raku.y"
        { AST_t *e=expr_new(AST_IF); expr_add_child(e,expr_unary(AST_NOT,(yyvsp[-4].node))); expr_add_child(e,(yyvsp[-2].node)); expr_add_child(e,(yyvsp[0].node)); (yyval.node)=e; }
#line 1896 "raku.tab.c"
    break;

  case 46: /* until_stmt: KW_UNTIL '(' expr ')' block  */
#line 356 "raku.y"
        { AST_t *e=expr_new(AST_UNTIL); expr_add_child(e,(yyvsp[-2].node)); expr_add_child(e,(yyvsp[0].node)); (yyval.node)=e; }
#line 1902 "raku.tab.c"
    break;

  case 47: /* repeat_stmt: KW_REPEAT block  */
#line 362 "raku.y"
        { AST_t *e=expr_new(AST_REPEAT); expr_add_child(e,(yyvsp[0].node)); (yyval.node)=e; }
#line 1908 "raku.tab.c"
    break;

  case 48: /* for_stmt: KW_FOR expr OP_ARROW VAR_SCALAR block  */
#line 368 "raku.y"
        { AST_t *iter=(yyvsp[-3].node); const char *vname=strip_sigil((yyvsp[-1].sval));
          if (iter->kind==AST_TO) {
              /* range case: lo=children[0], hi=children[1] */
              (yyval.node) = make_for_range(iter->children[0], iter->children[1], vname, (yyvsp[0].node));
          } else {
              /* Always wrap in AST_ITERATE so loopvar goes on the wrapper node.
               * RK-16/RK-21: gen->sval = loopvar name for coro_drive / AST_EVERY binding. */
              const char *vn = intern(strip_sigil((yyvsp[-1].sval)));
              AST_t *gen = expr_unary(AST_ITERATE, iter);
              gen->sval = (char *)vn;
              (yyval.node)=expr_binary(AST_EVERY,gen,(yyvsp[0].node));
          } }
#line 1925 "raku.tab.c"
    break;

  case 49: /* for_stmt: KW_FOR expr block  */
#line 381 "raku.y"
        { AST_t *gen=((yyvsp[-1].node)->kind==AST_VAR)?expr_unary(AST_ITERATE,(yyvsp[-1].node)):(yyvsp[-1].node);
          (yyval.node)=expr_binary(AST_EVERY,gen,(yyvsp[0].node)); }
#line 1932 "raku.tab.c"
    break;

  case 50: /* given_stmt: KW_GIVEN expr '{' when_list '}'  */
#line 387 "raku.y"
        { /* RK-18d: AST_CASE[ topic, cmpnode0, val0, body0, cmpnode1, val1, body1, ... ]
           * cmp kind stored in separate AST_ILIT node (ival=AST_e) to avoid corrupting val->ival. */
          AST_t *ec=expr_new(AST_CASE);
          expr_add_child(ec,(yyvsp[-3].node));
          ExprList *whens=(yyvsp[-1].list);
          for(int i=0;i<whens->count;i++){
              AST_t *pair=whens->items[i];
              AST_e cmp=(AST_e)(pair->ival);
              AST_t *val=pair->children[0], *body=pair->children[1];
              AST_t *cn=expr_new(AST_ILIT); cn->ival=(long)cmp;
              expr_add_child(ec,cn); expr_add_child(ec,val); expr_add_child(ec,body);
          }
          exprlist_free(whens);
          (yyval.node)=ec; }
#line 1951 "raku.tab.c"
    break;

  case 51: /* given_stmt: KW_GIVEN expr '{' when_list KW_DEFAULT block '}'  */
#line 402 "raku.y"
        { /* RK-18d: AST_CASE with default: AST_NUL cmpnode + AST_NUL val + body at end. */
          AST_t *ec=expr_new(AST_CASE);
          expr_add_child(ec,(yyvsp[-5].node));
          ExprList *whens=(yyvsp[-3].list);
          for(int i=0;i<whens->count;i++){
              AST_t *pair=whens->items[i];
              AST_e cmp=(AST_e)(pair->ival);
              AST_t *val=pair->children[0], *body=pair->children[1];
              AST_t *cn=expr_new(AST_ILIT); cn->ival=(long)cmp;
              expr_add_child(ec,cn); expr_add_child(ec,val); expr_add_child(ec,body);
          }
          exprlist_free(whens);
          expr_add_child(ec,expr_new(AST_NUL)); expr_add_child(ec,expr_new(AST_NUL)); expr_add_child(ec,(yyvsp[-1].node));
          (yyval.node)=ec; }
#line 1970 "raku.tab.c"
    break;

  case 52: /* when_list: %empty  */
#line 419 "raku.y"
                   { (yyval.list)=exprlist_new(); }
#line 1976 "raku.tab.c"
    break;

  case 53: /* when_list: when_list KW_WHEN expr block  */
#line 421 "raku.y"
        { AST_e cmp=((yyvsp[-1].node)->kind==AST_QLIT)?AST_LEQ:AST_EQ;
          AST_t *pair=expr_new(AST_SEQ_EXPR);
          pair->ival=(long)cmp;
          expr_add_child(pair,(yyvsp[-1].node)); expr_add_child(pair,(yyvsp[0].node));
          (yyval.list)=exprlist_append((yyvsp[-3].list),pair); }
#line 1986 "raku.tab.c"
    break;

  case 54: /* sub_decl: KW_SUB IDENT '(' param_list ')' block  */
#line 430 "raku.y"
        { ExprList *params=(yyvsp[-2].list); int np=params?params->count:0;
          AST_t *e=leaf_sval(AST_FNC,(yyvsp[-4].sval)); e->ival=(long)np|SUB_TAG;
          AST_t *nn=expr_new(AST_VAR); nn->sval=intern((yyvsp[-4].sval)); expr_add_child(e,nn);
          if(params){ for(int i=0;i<np;i++) expr_add_child(e,params->items[i]); exprlist_free(params); }
          AST_t *body=(yyvsp[0].node);
          for(int i=0;i<body->nchildren;i++) expr_add_child(e,body->children[i]);
          (yyval.node)=e; }
#line 1998 "raku.tab.c"
    break;

  case 55: /* sub_decl: KW_SUB IDENT '(' ')' block  */
#line 438 "raku.y"
        { AST_t *e=leaf_sval(AST_FNC,(yyvsp[-3].sval)); e->ival=(long)0|SUB_TAG;
          AST_t *nn=expr_new(AST_VAR); nn->sval=intern((yyvsp[-3].sval)); expr_add_child(e,nn);
          AST_t *body=(yyvsp[0].node);
          for(int i=0;i<body->nchildren;i++) expr_add_child(e,body->children[i]);
          (yyval.node)=e; }
#line 2008 "raku.tab.c"
    break;

  case 56: /* class_decl: KW_CLASS IDENT '{' class_body_list '}'  */
#line 450 "raku.y"
        {
            const char *cname = intern((yyvsp[-3].sval)); free((yyvsp[-3].sval));
            ExprList *body = (yyvsp[-1].list);
            AST_t *rec = expr_new(AST_RECORD);
            rec->sval = (char *)cname;
            if (body) {
                for (int i = 0; i < body->count; i++) {
                    AST_t *item = body->items[i];
                    if (!item) continue;
                    if (item->kind == AST_VAR) {
                        expr_add_child(rec, item);
                    } else if (item->kind == AST_FNC && (item->ival & SUB_TAG)) {
                        char fullname[256];
                        snprintf(fullname, sizeof fullname, "%s__%s", cname, item->sval);
                        const char *fname = intern(fullname);
                        raku_meth_register(cname, item->sval, fname);
                        item->sval = (char *)fname;
                        if (item->nchildren > 0 && item->children[0]->kind == AST_VAR)
                            item->children[0]->sval = (char *)fname;
                        item->ival &= ~SUB_TAG;
                        add_proc(item);
                        body->items[i] = NULL;
                    }
                }
                exprlist_free(body);
            }
            add_proc(rec);
            (yyval.node) = expr_new(AST_NUL);
        }
#line 2042 "raku.tab.c"
    break;

  case 57: /* class_body_list: %empty  */
#line 482 "raku.y"
                   { (yyval.list) = exprlist_new(); }
#line 2048 "raku.tab.c"
    break;

  case 58: /* class_body_list: class_body_list KW_HAS VAR_TWIGIL ';'  */
#line 484 "raku.y"
        { AST_t *fv = leaf_sval(AST_VAR, (yyvsp[-1].sval)); free((yyvsp[-1].sval));
          (yyval.list) = exprlist_append((yyvsp[-3].list), fv); }
#line 2055 "raku.tab.c"
    break;

  case 59: /* class_body_list: class_body_list KW_HAS VAR_SCALAR ';'  */
#line 487 "raku.y"
        { AST_t *fv = leaf_sval(AST_VAR, strip_sigil((yyvsp[-1].sval))); free((yyvsp[-1].sval));
          (yyval.list) = exprlist_append((yyvsp[-3].list), fv); }
#line 2062 "raku.tab.c"
    break;

  case 60: /* class_body_list: class_body_list KW_METHOD IDENT '(' param_list ')' block  */
#line 490 "raku.y"
        { ExprList *params = (yyvsp[-2].list); int np = params ? params->count : 0;
          AST_t *e = leaf_sval(AST_FNC, (yyvsp[-4].sval));
          e->ival = (long)(np + 1) | SUB_TAG;
          AST_t *nn = expr_new(AST_VAR); nn->sval = intern((yyvsp[-4].sval)); expr_add_child(e, nn);
          expr_add_child(e, leaf_sval(AST_VAR, "self"));
          if (params) { for (int i = 0; i < np; i++) expr_add_child(e, params->items[i]); exprlist_free(params); }
          AST_t *body = (yyvsp[0].node);
          for (int i = 0; i < body->nchildren; i++) expr_add_child(e, body->children[i]);
          free((yyvsp[-4].sval));
          (yyval.list) = exprlist_append((yyvsp[-6].list), e); }
#line 2077 "raku.tab.c"
    break;

  case 61: /* class_body_list: class_body_list KW_METHOD IDENT '(' ')' block  */
#line 501 "raku.y"
        { AST_t *e = leaf_sval(AST_FNC, (yyvsp[-3].sval));
          e->ival = (long)(1) | SUB_TAG;
          AST_t *nn = expr_new(AST_VAR); nn->sval = intern((yyvsp[-3].sval)); expr_add_child(e, nn);
          expr_add_child(e, leaf_sval(AST_VAR, "self"));
          AST_t *body = (yyvsp[0].node);
          for (int i = 0; i < body->nchildren; i++) expr_add_child(e, body->children[i]);
          free((yyvsp[-3].sval));
          (yyval.list) = exprlist_append((yyvsp[-5].list), e); }
#line 2090 "raku.tab.c"
    break;

  case 62: /* named_arg_list: IDENT OP_FATARROW expr  */
#line 513 "raku.y"
        { (yyval.list) = exprlist_new();
          exprlist_append((yyval.list), leaf_sval(AST_QLIT, (yyvsp[-2].sval))); free((yyvsp[-2].sval));
          exprlist_append((yyval.list), (yyvsp[0].node)); }
#line 2098 "raku.tab.c"
    break;

  case 63: /* named_arg_list: named_arg_list ',' IDENT OP_FATARROW expr  */
#line 517 "raku.y"
        { exprlist_append((yyvsp[-4].list), leaf_sval(AST_QLIT, (yyvsp[-2].sval))); free((yyvsp[-2].sval));
          exprlist_append((yyvsp[-4].list), (yyvsp[0].node));
          (yyval.list) = (yyvsp[-4].list); }
#line 2106 "raku.tab.c"
    break;

  case 64: /* param_list: VAR_SCALAR  */
#line 523 "raku.y"
                             { (yyval.list)=exprlist_append(exprlist_new(),var_node((yyvsp[0].sval))); }
#line 2112 "raku.tab.c"
    break;

  case 65: /* param_list: param_list ',' VAR_SCALAR  */
#line 524 "raku.y"
                                { (yyval.list)=exprlist_append((yyvsp[-2].list),var_node((yyvsp[0].sval))); }
#line 2118 "raku.tab.c"
    break;

  case 66: /* block: '{' stmt_list '}'  */
#line 528 "raku.y"
                         { (yyval.node)=make_seq((yyvsp[-1].list)); }
#line 2124 "raku.tab.c"
    break;

  case 67: /* closure: '{' expr '}'  */
#line 533 "raku.y"
                    { (yyval.node)=(yyvsp[-1].node); }
#line 2130 "raku.tab.c"
    break;

  case 68: /* expr: VAR_SCALAR '=' expr  */
#line 537 "raku.y"
                           { (yyval.node)=expr_binary(AST_ASSIGN,var_node((yyvsp[-2].sval)),(yyvsp[0].node)); }
#line 2136 "raku.tab.c"
    break;

  case 69: /* expr: KW_GATHER block  */
#line 538 "raku.y"
                           {
          /* RK-21: gather { block } → anonymous coroutine sub + call.
           * 1. Build AST_FNC def with SUB_TAG (like sub_decl) named __gather_N.
           * 2. add_proc() so it lands in the proc table.
           * 3. Return a call AST_FNC (no SUB_TAG) so coro_eval wraps it as
           *    coro_bb_suspend — a BB_PUMP coroutine collecting AST_SUSPEND (take) values. */
          static int gather_seq = 0;
          char gname[32]; snprintf(gname, sizeof gname, "__gather_%d", gather_seq++);
          /* Build the def node */
          AST_t *def = leaf_sval(AST_FNC, gname); def->ival = (long)0 | SUB_TAG;
          AST_t *dn  = expr_new(AST_VAR); dn->sval = intern(gname);
          expr_add_child(def, dn);
          /* Splice block children into def */
          AST_t *blk = (yyvsp[0].node);
          for (int i = 0; i < blk->nchildren; i++) expr_add_child(def, blk->children[i]);
          def->ival &= ~SUB_TAG;   /* strip sentinel — restore real nparams (0) for coro_call */
          add_proc(def);
          /* Build the call node (no SUB_TAG) */
          AST_t *call = leaf_sval(AST_FNC, gname);
          AST_t *cn   = expr_new(AST_VAR); cn->sval = intern(gname);
          expr_add_child(call, cn);
          (yyval.node) = call;
      }
#line 2164 "raku.tab.c"
    break;

  case 70: /* expr: cmp_expr  */
#line 561 "raku.y"
                           { (yyval.node)=(yyvsp[0].node); }
#line 2170 "raku.tab.c"
    break;

  case 71: /* cmp_expr: cmp_expr OP_AND add_expr  */
#line 565 "raku.y"
                                { (yyval.node)=expr_binary(AST_SEQ,(yyvsp[-2].node),(yyvsp[0].node)); }
#line 2176 "raku.tab.c"
    break;

  case 72: /* cmp_expr: cmp_expr OP_OR add_expr  */
#line 566 "raku.y"
                                { (yyval.node)=expr_binary(AST_ALT,(yyvsp[-2].node),(yyvsp[0].node)); }
#line 2182 "raku.tab.c"
    break;

  case 73: /* cmp_expr: add_expr OP_EQ add_expr  */
#line 567 "raku.y"
                                { (yyval.node)=expr_binary(AST_EQ,(yyvsp[-2].node),(yyvsp[0].node)); }
#line 2188 "raku.tab.c"
    break;

  case 74: /* cmp_expr: add_expr OP_NE add_expr  */
#line 568 "raku.y"
                                { (yyval.node)=expr_binary(AST_NE,(yyvsp[-2].node),(yyvsp[0].node)); }
#line 2194 "raku.tab.c"
    break;

  case 75: /* cmp_expr: add_expr '<' add_expr  */
#line 569 "raku.y"
                                { (yyval.node)=expr_binary(AST_LT,(yyvsp[-2].node),(yyvsp[0].node)); }
#line 2200 "raku.tab.c"
    break;

  case 76: /* cmp_expr: add_expr '>' add_expr  */
#line 570 "raku.y"
                                { (yyval.node)=expr_binary(AST_GT,(yyvsp[-2].node),(yyvsp[0].node)); }
#line 2206 "raku.tab.c"
    break;

  case 77: /* cmp_expr: add_expr OP_LE add_expr  */
#line 571 "raku.y"
                                { (yyval.node)=expr_binary(AST_LE,(yyvsp[-2].node),(yyvsp[0].node)); }
#line 2212 "raku.tab.c"
    break;

  case 78: /* cmp_expr: add_expr OP_GE add_expr  */
#line 572 "raku.y"
                                { (yyval.node)=expr_binary(AST_GE,(yyvsp[-2].node),(yyvsp[0].node)); }
#line 2218 "raku.tab.c"
    break;

  case 79: /* cmp_expr: add_expr OP_SEQ add_expr  */
#line 573 "raku.y"
                                { (yyval.node)=expr_binary(AST_LEQ,(yyvsp[-2].node),(yyvsp[0].node)); }
#line 2224 "raku.tab.c"
    break;

  case 80: /* cmp_expr: add_expr OP_SNE add_expr  */
#line 574 "raku.y"
                                { (yyval.node)=expr_binary(AST_LNE,(yyvsp[-2].node),(yyvsp[0].node)); }
#line 2230 "raku.tab.c"
    break;

  case 81: /* cmp_expr: add_expr OP_SMATCH LIT_REGEX  */
#line 576 "raku.y"
        { /* RK-23: $s ~~ /pattern/ */
          AST_t *c = make_call("raku_match");
          expr_add_child(c, (yyvsp[-2].node));
          expr_add_child(c, leaf_sval(AST_QLIT, (yyvsp[0].sval)));
          (yyval.node) = c; }
#line 2240 "raku.tab.c"
    break;

  case 82: /* cmp_expr: add_expr OP_SMATCH LIT_MATCH_GLOBAL  */
#line 582 "raku.y"
        { /* RK-37: $s ~~ m:g/pat/ — global match, returns SOH-sep match list */
          AST_t *c = make_call("raku_match_global");
          expr_add_child(c, (yyvsp[-2].node));
          expr_add_child(c, leaf_sval(AST_QLIT, (yyvsp[0].sval)));
          (yyval.node) = c; }
#line 2250 "raku.tab.c"
    break;

  case 83: /* cmp_expr: add_expr OP_SMATCH LIT_SUBST  */
#line 588 "raku.y"
        { /* RK-37: $s ~~ s/pat/repl/[g] — substitution */
          AST_t *c = make_call("raku_subst");
          expr_add_child(c, (yyvsp[-2].node));
          expr_add_child(c, leaf_sval(AST_QLIT, (yyvsp[0].sval)));
          (yyval.node) = c; }
#line 2260 "raku.tab.c"
    break;

  case 84: /* cmp_expr: range_expr  */
#line 593 "raku.y"
                               { (yyval.node)=(yyvsp[0].node); }
#line 2266 "raku.tab.c"
    break;

  case 85: /* range_expr: add_expr OP_RANGE add_expr  */
#line 597 "raku.y"
                                    { (yyval.node)=expr_binary(AST_TO,(yyvsp[-2].node),(yyvsp[0].node)); }
#line 2272 "raku.tab.c"
    break;

  case 86: /* range_expr: add_expr OP_RANGE_EX add_expr  */
#line 598 "raku.y"
                                    { (yyval.node)=expr_binary(AST_TO,(yyvsp[-2].node),(yyvsp[0].node)); }
#line 2278 "raku.tab.c"
    break;

  case 87: /* range_expr: add_expr  */
#line 599 "raku.y"
                                    { (yyval.node)=(yyvsp[0].node); }
#line 2284 "raku.tab.c"
    break;

  case 88: /* add_expr: add_expr '+' mul_expr  */
#line 603 "raku.y"
                             { (yyval.node)=expr_binary(AST_ADD,(yyvsp[-2].node),(yyvsp[0].node)); }
#line 2290 "raku.tab.c"
    break;

  case 89: /* add_expr: add_expr '-' mul_expr  */
#line 604 "raku.y"
                             { (yyval.node)=expr_binary(AST_SUB,(yyvsp[-2].node),(yyvsp[0].node)); }
#line 2296 "raku.tab.c"
    break;

  case 90: /* add_expr: add_expr '~' mul_expr  */
#line 605 "raku.y"
                             { (yyval.node)=expr_binary(AST_CAT,(yyvsp[-2].node),(yyvsp[0].node)); }
#line 2302 "raku.tab.c"
    break;

  case 91: /* add_expr: mul_expr  */
#line 606 "raku.y"
                             { (yyval.node)=(yyvsp[0].node); }
#line 2308 "raku.tab.c"
    break;

  case 92: /* mul_expr: mul_expr '*' unary_expr  */
#line 610 "raku.y"
                                  { (yyval.node)=expr_binary(AST_MUL,(yyvsp[-2].node),(yyvsp[0].node)); }
#line 2314 "raku.tab.c"
    break;

  case 93: /* mul_expr: mul_expr '/' unary_expr  */
#line 611 "raku.y"
                                  { (yyval.node)=expr_binary(AST_DIV,(yyvsp[-2].node),(yyvsp[0].node)); }
#line 2320 "raku.tab.c"
    break;

  case 94: /* mul_expr: mul_expr '%' unary_expr  */
#line 612 "raku.y"
                                  { (yyval.node)=expr_binary(AST_MOD,(yyvsp[-2].node),(yyvsp[0].node)); }
#line 2326 "raku.tab.c"
    break;

  case 95: /* mul_expr: mul_expr OP_DIV unary_expr  */
#line 613 "raku.y"
                                  { (yyval.node)=expr_binary(AST_DIV,(yyvsp[-2].node),(yyvsp[0].node)); }
#line 2332 "raku.tab.c"
    break;

  case 96: /* mul_expr: unary_expr  */
#line 614 "raku.y"
                                  { (yyval.node)=(yyvsp[0].node); }
#line 2338 "raku.tab.c"
    break;

  case 97: /* unary_expr: '-' unary_expr  */
#line 618 "raku.y"
                                   { (yyval.node)=expr_unary(AST_MNS,(yyvsp[0].node)); }
#line 2344 "raku.tab.c"
    break;

  case 98: /* unary_expr: '!' unary_expr  */
#line 619 "raku.y"
                                   { (yyval.node)=expr_unary(AST_NOT,(yyvsp[0].node)); }
#line 2350 "raku.tab.c"
    break;

  case 99: /* unary_expr: postfix_expr  */
#line 620 "raku.y"
                                   { (yyval.node)=(yyvsp[0].node); }
#line 2356 "raku.tab.c"
    break;

  case 100: /* postfix_expr: call_expr  */
#line 623 "raku.y"
                         { (yyval.node)=(yyvsp[0].node); }
#line 2362 "raku.tab.c"
    break;

  case 101: /* call_expr: IDENT '(' arg_list ')'  */
#line 627 "raku.y"
        { AST_t *e=make_call((yyvsp[-3].sval));
          ExprList *args=(yyvsp[-1].list);
          if(args){ for(int i=0;i<args->count;i++) expr_add_child(e,args->items[i]); exprlist_free(args); }
          (yyval.node)=e; }
#line 2371 "raku.tab.c"
    break;

  case 102: /* call_expr: IDENT '(' ')'  */
#line 631 "raku.y"
                     { (yyval.node)=make_call((yyvsp[-2].sval)); }
#line 2377 "raku.tab.c"
    break;

  case 103: /* call_expr: IDENT '.' KW_NEW '(' named_arg_list ')'  */
#line 634 "raku.y"
        { AST_t *c=make_call("raku_new");
          expr_add_child(c,leaf_sval(AST_QLIT,(yyvsp[-5].sval))); free((yyvsp[-5].sval));
          ExprList *nargs=(yyvsp[-1].list);
          if(nargs){ for(int i=0;i<nargs->count;i++) expr_add_child(c,nargs->items[i]); exprlist_free(nargs); }
          (yyval.node)=c; }
#line 2387 "raku.tab.c"
    break;

  case 104: /* call_expr: IDENT '.' KW_NEW '(' ')'  */
#line 640 "raku.y"
        { AST_t *c=make_call("raku_new");
          expr_add_child(c,leaf_sval(AST_QLIT,(yyvsp[-4].sval))); free((yyvsp[-4].sval));
          (yyval.node)=c; }
#line 2395 "raku.tab.c"
    break;

  case 105: /* call_expr: atom '.' IDENT '(' arg_list ')'  */
#line 645 "raku.y"
        { AST_t *c=make_call("raku_mcall");
          expr_add_child(c,(yyvsp[-5].node));
          expr_add_child(c,leaf_sval(AST_QLIT,(yyvsp[-3].sval))); free((yyvsp[-3].sval));
          ExprList *args=(yyvsp[-1].list);
          if(args){ for(int i=0;i<args->count;i++) expr_add_child(c,args->items[i]); exprlist_free(args); }
          (yyval.node)=c; }
#line 2406 "raku.tab.c"
    break;

  case 106: /* call_expr: atom '.' IDENT '(' ')'  */
#line 652 "raku.y"
        { AST_t *c=make_call("raku_mcall");
          expr_add_child(c,(yyvsp[-4].node));
          expr_add_child(c,leaf_sval(AST_QLIT,(yyvsp[-2].sval))); free((yyvsp[-2].sval));
          (yyval.node)=c; }
#line 2415 "raku.tab.c"
    break;

  case 107: /* call_expr: atom '.' IDENT  */
#line 658 "raku.y"
        { AST_t *fe=expr_new(AST_FIELD);
          fe->sval=(char*)intern((yyvsp[0].sval)); free((yyvsp[0].sval));
          expr_add_child(fe,(yyvsp[-2].node));
          (yyval.node)=fe; }
#line 2424 "raku.tab.c"
    break;

  case 108: /* call_expr: KW_DIE expr  */
#line 665 "raku.y"
        { AST_t *c=make_call("raku_die");
          expr_add_child(c,(yyvsp[0].node)); (yyval.node)=c; }
#line 2431 "raku.tab.c"
    break;

  case 109: /* call_expr: KW_MAP closure expr  */
#line 668 "raku.y"
        { AST_t *c=make_call("raku_map");
          expr_add_child(c,(yyvsp[-1].node)); expr_add_child(c,(yyvsp[0].node)); (yyval.node)=c; }
#line 2438 "raku.tab.c"
    break;

  case 110: /* call_expr: KW_GREP closure expr  */
#line 671 "raku.y"
        { AST_t *c=make_call("raku_grep");
          expr_add_child(c,(yyvsp[-1].node)); expr_add_child(c,(yyvsp[0].node)); (yyval.node)=c; }
#line 2445 "raku.tab.c"
    break;

  case 111: /* call_expr: KW_SORT expr  */
#line 674 "raku.y"
        { AST_t *c=make_call("raku_sort");
          expr_add_child(c,(yyvsp[0].node)); (yyval.node)=c; }
#line 2452 "raku.tab.c"
    break;

  case 112: /* call_expr: KW_SORT closure expr  */
#line 677 "raku.y"
        { AST_t *c=make_call("raku_sort");
          expr_add_child(c,(yyvsp[-1].node)); expr_add_child(c,(yyvsp[0].node)); (yyval.node)=c; }
#line 2459 "raku.tab.c"
    break;

  case 113: /* call_expr: atom  */
#line 679 "raku.y"
                     { (yyval.node)=(yyvsp[0].node); }
#line 2465 "raku.tab.c"
    break;

  case 114: /* arg_list: expr  */
#line 683 "raku.y"
                        { (yyval.list)=exprlist_append(exprlist_new(),(yyvsp[0].node)); }
#line 2471 "raku.tab.c"
    break;

  case 115: /* arg_list: arg_list ',' expr  */
#line 684 "raku.y"
                        { (yyval.list)=exprlist_append((yyvsp[-2].list),(yyvsp[0].node)); }
#line 2477 "raku.tab.c"
    break;

  case 116: /* atom: LIT_INT  */
#line 688 "raku.y"
                      { AST_t *e=expr_new(AST_ILIT); e->ival=(yyvsp[0].ival); (yyval.node)=e; }
#line 2483 "raku.tab.c"
    break;

  case 117: /* atom: LIT_FLOAT  */
#line 689 "raku.y"
                      { AST_t *e=expr_new(AST_FLIT); e->dval=(yyvsp[0].dval); (yyval.node)=e; }
#line 2489 "raku.tab.c"
    break;

  case 118: /* atom: LIT_STR  */
#line 690 "raku.y"
                      { (yyval.node)=leaf_sval(AST_QLIT,(yyvsp[0].sval)); }
#line 2495 "raku.tab.c"
    break;

  case 119: /* atom: LIT_INTERP_STR  */
#line 691 "raku.y"
                      { (yyval.node)=lower_interp_str((yyvsp[0].sval)); }
#line 2501 "raku.tab.c"
    break;

  case 120: /* atom: VAR_SCALAR  */
#line 692 "raku.y"
                      { (yyval.node)=var_node((yyvsp[0].sval)); }
#line 2507 "raku.tab.c"
    break;

  case 121: /* atom: VAR_ARRAY  */
#line 693 "raku.y"
                      { (yyval.node)=var_node((yyvsp[0].sval)); }
#line 2513 "raku.tab.c"
    break;

  case 122: /* atom: VAR_HASH  */
#line 694 "raku.y"
                      { (yyval.node)=var_node((yyvsp[0].sval)); }
#line 2519 "raku.tab.c"
    break;

  case 123: /* atom: VAR_CAPTURE  */
#line 696 "raku.y"
        { /* RK-34: $0/$1 positional capture */
          AST_t *c=make_call("raku_capture");
          AST_t *idx=expr_new(AST_ILIT); idx->ival=(yyvsp[0].ival);
          expr_add_child(c,idx); (yyval.node)=c; }
#line 2528 "raku.tab.c"
    break;

  case 124: /* atom: VAR_NAMED_CAPTURE  */
#line 701 "raku.y"
        { /* RK-35: $<name> named capture */
          AST_t *c=make_call("raku_named_capture");
          expr_add_child(c,leaf_sval(AST_QLIT,(yyvsp[0].sval))); (yyval.node)=c; }
#line 2536 "raku.tab.c"
    break;

  case 125: /* atom: VAR_ARRAY '[' expr ']'  */
#line 705 "raku.y"
        { AST_t *c=make_call("arr_get"); expr_add_child(c,var_node((yyvsp[-3].sval))); expr_add_child(c,(yyvsp[-1].node)); (yyval.node)=c; }
#line 2542 "raku.tab.c"
    break;

  case 126: /* atom: VAR_HASH '<' IDENT '>'  */
#line 707 "raku.y"
        { AST_t *c=make_call("hash_get"); expr_add_child(c,var_node((yyvsp[-3].sval))); expr_add_child(c,leaf_sval(AST_QLIT,(yyvsp[-1].sval))); (yyval.node)=c; }
#line 2548 "raku.tab.c"
    break;

  case 127: /* atom: VAR_HASH '{' expr '}'  */
#line 709 "raku.y"
        { AST_t *c=make_call("hash_get"); expr_add_child(c,var_node((yyvsp[-3].sval))); expr_add_child(c,(yyvsp[-1].node)); (yyval.node)=c; }
#line 2554 "raku.tab.c"
    break;

  case 128: /* atom: KW_EXISTS VAR_HASH '<' IDENT '>'  */
#line 711 "raku.y"
        { AST_t *c=make_call("hash_exists"); expr_add_child(c,var_node((yyvsp[-3].sval))); expr_add_child(c,leaf_sval(AST_QLIT,(yyvsp[-1].sval))); (yyval.node)=c; }
#line 2560 "raku.tab.c"
    break;

  case 129: /* atom: KW_EXISTS VAR_HASH '{' expr '}'  */
#line 713 "raku.y"
        { AST_t *c=make_call("hash_exists"); expr_add_child(c,var_node((yyvsp[-3].sval))); expr_add_child(c,(yyvsp[-1].node)); (yyval.node)=c; }
#line 2566 "raku.tab.c"
    break;

  case 130: /* atom: IDENT  */
#line 714 "raku.y"
                      { (yyval.node)=var_node((yyvsp[0].sval)); }
#line 2572 "raku.tab.c"
    break;

  case 131: /* atom: VAR_TWIGIL  */
#line 717 "raku.y"
        { AST_t *fe=expr_new(AST_FIELD);
          fe->sval=(char*)intern((yyvsp[0].sval)); free((yyvsp[0].sval));
          expr_add_child(fe, leaf_sval(AST_VAR, "self"));
          (yyval.node)=fe; }
#line 2581 "raku.tab.c"
    break;

  case 132: /* atom: '(' expr ')'  */
#line 721 "raku.y"
                      { (yyval.node)=(yyvsp[-1].node); }
#line 2587 "raku.tab.c"
    break;


#line 2591 "raku.tab.c"

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

#line 724 "raku.y"


/* ── Parse entry (sets up flex buffer and calls yyparse) ─────────────── */
extern void *raku_yy_scan_string(const char *);
extern void  raku_yy_delete_buffer(void *);

CODE_t *raku_parse_string(const char *src) {
    raku_prog_result = NULL;
    void *buf = raku_yy_scan_string(src);
    raku_yyparse();
    raku_yy_delete_buffer(buf);
    return raku_prog_result;
}
