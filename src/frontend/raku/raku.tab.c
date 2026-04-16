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

#include "../../ir/ir.h"
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
static ExprList *exprlist_append(ExprList *l, EXPR_t *e) {
    if (l->count >= l->cap) {
        l->cap = l->cap ? l->cap * 2 : 8;
        l->items = realloc(l->items, l->cap * sizeof(EXPR_t *));
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
static EXPR_t *leaf_sval(EKind k, const char *s) {
    EXPR_t *e = expr_new(k); e->sval = intern(s); return e;
}
static EXPR_t *var_node(const char *name) {
    return leaf_sval(E_VAR, strip_sigil(name));
}
/* make_call: E_FNC + children[0]=E_VAR(name) for icn_interp_eval layout */
static EXPR_t *make_call(const char *name) {
    EXPR_t *e = leaf_sval(E_FNC, name);
    EXPR_t *n = expr_new(E_VAR); n->sval = intern(name);
    expr_add_child(e, n);
    return e;
}
/* make_seq: ExprList → E_SEQ_EXPR, frees list */
static EXPR_t *make_seq(ExprList *stmts) {
    EXPR_t *seq = expr_new(E_SEQ_EXPR);
    if (stmts) {
        for (int i = 0; i < stmts->count; i++) expr_add_child(seq, stmts->items[i]);
        exprlist_free(stmts);
    }
    return seq;
}
/* lower_interp_str: "hello $var" → left-associative E_CAT chain */
static EXPR_t *lower_interp_str(const char *s) {
    int len = s ? (int)strlen(s) : 0;
    EXPR_t *result = NULL;
    char litbuf[4096]; int litpos = 0, i = 0;
    while (i < len) {
        if (s[i]=='$' && i+1<len &&
            (s[i+1]=='_'||(s[i+1]>='A'&&s[i+1]<='Z')||(s[i+1]>='a'&&s[i+1]<='z'))) {
            if (litpos>0) { litbuf[litpos]='\0';
                EXPR_t *lit=leaf_sval(E_QLIT,litbuf);
                result=result?expr_binary(E_CAT,result,lit):lit; litpos=0; }
            i++;
            char vname[256]; int vlen=0;
            while (i<len&&(s[i]=='_'||(s[i]>='A'&&s[i]<='Z')||(s[i]>='a'&&s[i]<='z')||(s[i]>='0'&&s[i]<='9')))
                { if(vlen<255) vname[vlen++]=s[i]; i++; }
            vname[vlen]='\0';
            EXPR_t *var=leaf_sval(E_VAR,vname);
            result=result?expr_binary(E_CAT,result,var):var;
        } else { if(litpos<4095) litbuf[litpos++]=s[i]; i++; }
    }
    if (litpos>0) { litbuf[litpos]='\0';
        EXPR_t *lit=leaf_sval(E_QLIT,litbuf);
        result=result?expr_binary(E_CAT,result,lit):lit; }
    return result ? result : leaf_sval(E_QLIT,"");
}
/* make_for_range: for lo..hi -> $v body → explicit while-loop */
static EXPR_t *make_for_range(EXPR_t *lo, EXPR_t *hi, const char *vname, EXPR_t *body_seq) {
    EXPR_t *init = expr_binary(E_ASSIGN, leaf_sval(E_VAR,vname), lo);
    EXPR_t *cond = expr_binary(E_LE, leaf_sval(E_VAR,vname), hi);
    EXPR_t *one  = expr_new(E_ILIT); one->ival = 1;
    EXPR_t *incr = expr_binary(E_ADD, leaf_sval(E_VAR,vname), one);
    expr_add_child(body_seq, expr_binary(E_ASSIGN, leaf_sval(E_VAR,vname), incr));
    EXPR_t *wloop = expr_binary(E_WHILE, cond, body_seq);
    EXPR_t *seq   = expr_new(E_SEQ_EXPR);
    expr_add_child(seq, init); expr_add_child(seq, wloop);
    return seq;
}

/*--------------------------------------------------------------------
 * Program output
 *--------------------------------------------------------------------*/
Program *raku_prog_result = NULL;

static void add_proc(EXPR_t *e) {
    if (!e) return;
    if (!raku_prog_result) raku_prog_result = calloc(1, sizeof(Program));
    STMT_t *st = calloc(1, sizeof(STMT_t));
    st->subject = e; st->lineno = 0; st->lang = LANG_RAKU;
    if (!raku_prog_result->head) raku_prog_result->head = raku_prog_result->tail = st;
    else { raku_prog_result->tail->next = st; raku_prog_result->tail = st; }
    raku_prog_result->nstmts++;
}

/* SUB_TAG: sentinel bit to distinguish sub defs from body stmts in stmt_list */
#define SUB_TAG 0x40000000

/* RK-26: Raku method table — maps "ClassName::method" → E_FNC proc name */
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
  YYSYMBOL_75_ = 75,                       /* '['  */
  YYSYMBOL_76_ = 76,                       /* ']'  */
  YYSYMBOL_77_ = 77,                       /* '{'  */
  YYSYMBOL_78_ = 78,                       /* '}'  */
  YYSYMBOL_79_ = 79,                       /* '('  */
  YYSYMBOL_80_ = 80,                       /* ')'  */
  YYSYMBOL_81_ = 81,                       /* ','  */
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
#define YYLAST   583

/* YYNTOKENS -- Number of terminals.  */
#define YYNTOKENS  82
/* YYNNTS -- Number of nonterminals.  */
#define YYNNTS  29
/* YYNRULES -- Number of rules.  */
#define YYNRULES  130
/* YYNSTATES -- Number of states.  */
#define YYNSTATES  308

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
      79,    80,    69,    67,    81,    68,    73,    70,     2,     2,
       2,     2,     2,     2,     2,     2,     2,     2,     2,    74,
      64,    62,    65,     2,     2,     2,     2,     2,     2,     2,
       2,     2,     2,     2,     2,     2,     2,     2,     2,     2,
       2,     2,     2,     2,     2,     2,     2,     2,     2,     2,
       2,    75,     2,    76,     2,     2,     2,     2,     2,     2,
       2,     2,     2,     2,     2,     2,     2,     2,     2,     2,
       2,     2,     2,     2,     2,     2,     2,     2,     2,     2,
       2,     2,     2,    77,     2,    78,    66,     2,     2,     2,
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
     265,   267,   269,   271,   273,   275,   277,   279,   281,   283,
     286,   291,   294,   297,   300,   303,   306,   307,   308,   309,
     310,   312,   315,   318,   319,   320,   321,   322,   326,   328,
     330,   335,   341,   343,   349,   355,   361,   374,   380,   395,
     413,   414,   423,   431,   443,   476,   477,   480,   483,   494,
     506,   510,   517,   518,   522,   527,   531,   532,   555,   559,
     560,   561,   562,   563,   564,   565,   566,   567,   568,   569,
     575,   581,   587,   591,   592,   593,   597,   598,   599,   600,
     604,   605,   606,   607,   608,   612,   613,   614,   617,   620,
     625,   627,   633,   638,   645,   651,   658,   661,   664,   667,
     670,   673,   677,   678,   682,   683,   684,   685,   686,   687,
     688,   689,   694,   698,   700,   702,   704,   706,   708,   710,
     715
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
  "'%'", "UMINUS", "'.'", "';'", "'['", "']'", "'{'", "'}'", "'('", "')'",
  "','", "$accept", "program", "stmt_list", "stmt", "if_stmt",
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

#define YYPACT_NINF (-60)

#define yypact_value_is_default(Yyn) \
  ((Yyn) == YYPACT_NINF)

#define YYTABLE_NINF (-1)

#define yytable_value_is_error(Yyn) \
  0

/* YYPACT[STATE-NUM] -- Index in YYTABLE of the portion describing
   STATE-NUM.  */
static const yytype_int16 yypact[] =
{
     -60,    20,   375,   -60,   -60,   -60,   -60,   -60,   -47,   -59,
     -29,   -60,   -55,   -60,   -60,    81,   504,   504,   -54,   -24,
     504,    59,     3,   504,   418,   504,    95,   122,    74,    83,
       3,    61,    61,   461,     3,   504,   146,    98,    98,   504,
     -60,   -60,   -60,   -60,   -60,   -60,   -60,   -60,   -60,   -60,
     100,    -7,   -60,   103,    27,   -60,   -60,   -60,   105,   504,
     162,   504,   165,   504,   134,    26,   120,   121,   126,   117,
     128,   109,   -11,   123,   125,   504,   504,   -44,   107,   -60,
     -60,   127,   -60,   131,   119,   -10,     6,   504,   504,   -60,
     504,   504,   504,   504,   -60,   153,   -60,   129,   -60,   -60,
     -60,   118,   -60,    98,    98,    98,    98,    98,    98,    98,
      98,    98,    98,   124,    98,    98,    98,    98,    98,    98,
      98,    98,    98,   186,   133,   147,   132,   137,   135,   148,
     -60,   -60,    19,   504,   504,   504,   -28,   -18,    16,   504,
     504,   196,   504,   -60,   -60,   151,   152,   201,   -60,     1,
     305,   -60,   -60,   -60,   220,   504,   221,   504,   161,   163,
     164,   -60,   -60,   -60,     3,   -60,   -60,    79,    79,    79,
      79,    79,    79,    79,    79,    79,    79,   -60,   -60,   -60,
      79,    79,    27,    27,    27,   -60,   -60,   -60,   -60,   172,
     -60,   504,   190,   191,   193,    -1,   -60,   504,   183,   184,
     187,   504,   -60,   504,   -60,   504,   -60,   -60,   188,   197,
     189,     3,     3,     3,   -60,     3,    35,   -60,    -9,   200,
     192,   203,   199,     3,     3,   -60,   -60,   -17,   234,   195,
     504,   504,   504,   227,   -60,    40,   -60,   -60,   -60,   -60,
     204,   205,   206,   -60,   -60,   -60,   254,   -60,   -60,   -60,
       3,   271,   504,     3,   -60,   -60,   -60,   209,   210,   266,
     -60,   274,   112,   -60,   -60,    62,   -60,   215,   216,   217,
     504,   -60,   278,   -60,   -60,   -60,   -15,   -60,   -60,     3,
     218,   -60,   -60,     3,   214,   224,   225,   -60,   -60,   -60,
     -60,   -60,   247,   -60,   -60,   -60,   -60,   -60,     2,   -60,
     -60,   504,     3,    84,   -60,   -60,     3,   -60
};

/* YYDEFACT[STATE-NUM] -- Default reduction number in state STATE-NUM.
   Performed when YYTABLE does not specify something else to do.  Zero
   means the default is an error.  */
static const yytype_uint8 yydefact[] =
{
       3,     0,     2,     1,   114,   115,   116,   117,   118,   119,
     120,   129,   128,   121,   122,     0,     0,     0,     0,     0,
       0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
       0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
       4,    27,    28,    33,    34,    35,    29,    30,    36,    37,
       0,    68,    82,    85,    89,    94,    97,    98,   111,     0,
       0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
     118,   119,   120,     0,     0,     0,     0,     0,     0,     3,
      67,     0,    18,     0,     0,     0,     0,     0,     0,    45,
       0,     0,     0,     0,   109,    31,   106,     0,   118,    96,
      95,     0,    26,     0,     0,     0,     0,     0,     0,     0,
       0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
       0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
     100,   112,     0,     0,     0,     0,     0,     0,     0,     0,
       0,     0,     0,    14,    15,     0,     0,     0,    47,     0,
       0,    16,    17,    50,     0,     0,     0,     0,     0,     0,
       0,   107,   108,   110,     0,    55,   130,    69,    70,    83,
      84,    71,    72,    75,    76,    77,    78,    79,    80,    81,
      73,    74,    88,    86,    87,    93,    90,    91,    92,   105,
      19,     0,   123,   124,   125,     0,    99,     0,     0,     0,
       0,     0,    11,     0,    12,     0,    13,    66,     0,     0,
       0,     0,     0,     0,    62,     0,     0,    64,     0,     0,
       0,     0,     0,     0,     0,    65,    32,     0,     0,     0,
       0,     0,     0,     0,   102,     0,   113,     5,     6,     7,
       0,     0,     0,   123,   124,   125,    38,    41,    46,    53,
       0,     0,     0,     0,    48,   126,   127,     0,     0,    42,
      44,     0,     0,    54,   104,     0,    20,     0,     0,     0,
       0,   101,     0,     8,     9,    10,     0,    52,    63,     0,
       0,    24,    25,     0,     0,     0,     0,   103,    21,    22,
      23,    60,     0,    40,    39,    51,    49,    43,     0,    57,
      56,     0,     0,     0,    61,    59,     0,    58
};

/* YYPGOTO[NTERM-NUM].  */
static const yytype_int16 yypgoto[] =
{
     -60,   -60,   222,   -60,    24,   -60,   -60,   -60,   -60,   -60,
     -60,   -60,   -60,   -60,   -60,   -60,    -3,   -20,   140,   -16,
     -60,   -60,   114,    32,   -35,   -60,   -60,    75,   -60
};

/* YYDEFGOTO[NTERM-NUM].  */
static const yytype_uint8 yydefgoto[] =
{
       0,     1,     2,    40,    41,    42,    43,    44,    45,    46,
      47,   218,    48,    49,   227,   235,   216,    80,    91,    50,
      51,    52,    53,    54,    55,    56,    57,   132,    58
};

/* YYTABLE[YYPACT[STATE-NUM]] -- What to do in state STATE-NUM.  If
   positive, shift that token.  If negative, reduce the rule whose
   number is the opposite.  If YYTABLE_NINF, syntax error.  */
static const yytype_int16 yytable[] =
{
      73,    74,    99,   100,    77,    18,   147,    81,    83,    84,
      89,   214,   214,   233,    95,    59,    61,    94,    64,    96,
       3,   252,   253,   101,    65,    75,    60,   261,   262,     4,
       5,     6,     7,    79,   201,    62,    70,    71,    72,    11,
      12,    13,    14,   124,   203,   126,   202,   128,    63,   131,
     103,   104,    22,   141,   154,    76,   204,   148,    26,   145,
     146,   263,    79,    31,    32,    33,   142,   155,    35,   254,
     156,   158,   159,    78,   160,   161,   162,   163,   205,   234,
      79,   215,   302,   157,   185,   186,   187,   188,   119,    37,
     206,    66,    67,    68,    38,    69,   120,   121,   122,   196,
     197,     4,     5,     6,     7,    39,   130,    85,    98,    71,
      72,    11,    12,    13,    14,   250,   251,   198,   199,   200,
     271,   272,   285,   207,   208,   286,   210,   136,   137,   138,
      26,   177,   178,   179,    86,    31,    32,    33,    90,   220,
      35,   222,   287,   197,   226,   116,   117,   118,   182,   183,
     184,   105,   106,    87,   107,   108,   109,   110,   111,   112,
      97,    37,    88,   113,   306,   251,    38,   114,   115,   116,
     117,   118,    92,    93,   102,   229,   125,    39,   123,   127,
     129,   236,   133,   134,   140,   240,   149,   241,   135,   242,
     139,   246,   247,   248,   164,   249,   153,   143,   166,   144,
     189,   151,   193,   259,   260,   152,   165,   190,   192,   191,
     209,   213,   131,   194,   267,   268,   269,   167,   168,   169,
     170,   171,   172,   173,   174,   175,   176,   195,   180,   181,
     277,   211,   212,   280,   219,   221,   279,     4,     5,     6,
       7,   223,   225,   224,    70,    71,    72,    11,    12,    13,
      14,   228,   230,   231,   291,   232,   294,   237,   238,   295,
      22,   239,   244,   297,   243,   255,    26,   245,   257,   266,
     256,    31,    32,    33,   270,   276,    35,   258,   273,   274,
     275,   278,   305,   281,   282,   304,   307,   283,   284,   288,
     289,   290,   292,   298,   301,   303,   296,    37,   299,   300,
     293,   150,    38,   265,     0,     0,     0,     0,     4,     5,
       6,     7,     0,    39,   264,     8,     9,    10,    11,    12,
      13,    14,    15,    16,    17,    18,     0,     0,    19,    20,
      21,    22,    23,    24,    25,     0,     0,    26,    27,    28,
      29,    30,    31,    32,    33,    34,     0,    35,    36,     0,
       0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
       0,     0,     0,     0,     0,     0,     0,     0,    37,     0,
       0,     0,     0,    38,     0,     0,     0,     0,     4,     5,
       6,     7,     0,   217,    39,     8,     9,    10,    11,    12,
      13,    14,    15,    16,    17,    18,     0,     0,    19,    20,
      21,    22,    23,    24,    25,     0,     0,    26,    27,    28,
      29,    30,    31,    32,    33,    34,     0,    35,    36,     0,
       0,     4,     5,     6,     7,     0,     0,     0,    70,    71,
      72,    11,    12,    13,    14,     0,     0,     0,    37,     0,
       0,     0,     0,    38,    22,     0,     0,     0,     0,     0,
      26,     0,     0,     0,    39,    31,    32,    33,     0,     0,
      35,     0,     0,     0,     4,     5,     6,     7,     0,     0,
       0,    70,    71,    72,    11,    12,    13,    14,     0,     0,
       0,    37,     0,     0,     0,     0,    38,    22,     0,     0,
       0,     0,    82,    26,     0,     0,     0,    39,    31,    32,
      33,     0,     0,    35,     0,     0,     0,     4,     5,     6,
       7,     0,     0,     0,    70,    71,    72,    11,    12,    13,
      14,     0,     0,     0,    37,     0,     0,     0,     0,    38,
      22,     0,     0,     0,     0,     0,    26,     0,    90,     0,
      39,    31,    32,    33,     0,     0,    35,     0,     0,     0,
       0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
       0,     0,     0,     0,     0,     0,     0,    37,     0,     0,
       0,     0,    38,     0,     0,     0,     0,     0,     0,     0,
       0,     0,     0,    39
};

static const yytype_int16 yycheck[] =
{
      16,    17,    37,    38,    20,    20,    50,    23,    24,    25,
      30,    10,    10,    14,    34,    62,    75,    33,    73,    35,
       0,    30,    31,    39,    79,    79,    73,    44,    45,     3,
       4,     5,     6,    77,    62,    64,    10,    11,    12,    13,
      14,    15,    16,    59,    62,    61,    74,    63,    77,    65,
      57,    58,    26,    64,    64,    79,    74,    77,    32,    75,
      76,    78,    77,    37,    38,    39,    77,    77,    42,    78,
      64,    87,    88,    14,    90,    91,    92,    93,    62,    80,
      77,    80,    80,    77,   119,   120,   121,   122,    61,    63,
      74,    10,    11,    12,    68,    14,    69,    70,    71,    80,
      81,     3,     4,     5,     6,    79,    80,    12,    10,    11,
      12,    13,    14,    15,    16,    80,    81,   133,   134,   135,
      80,    81,    10,   139,   140,    13,   142,    10,    11,    12,
      32,     7,     8,     9,    12,    37,    38,    39,    77,   155,
      42,   157,    80,    81,   164,    66,    67,    68,   116,   117,
     118,    48,    49,    79,    51,    52,    53,    54,    55,    56,
      14,    63,    79,    60,    80,    81,    68,    64,    65,    66,
      67,    68,    32,    33,    74,   191,    14,    79,    73,    14,
      46,   197,    62,    62,    75,   201,    79,   203,    62,   205,
      62,   211,   212,   213,    41,   215,    77,    74,    80,    74,
      14,    74,    65,   223,   224,    74,    77,    74,    76,    62,
      14,    10,   228,    78,   230,   231,   232,   103,   104,   105,
     106,   107,   108,   109,   110,   111,   112,    79,   114,   115,
     250,    80,    80,   253,    14,    14,   252,     3,     4,     5,
       6,    80,    78,    80,    10,    11,    12,    13,    14,    15,
      16,    79,    62,    62,   270,    62,   276,    74,    74,   279,
      26,    74,    65,   283,    76,    65,    32,    78,    65,    74,
      78,    37,    38,    39,    47,    21,    42,    78,    74,    74,
      74,    10,   302,    74,    74,   301,   306,    21,    14,    74,
      74,    74,    14,    79,    47,   298,    78,    63,    74,    74,
     276,    79,    68,   228,    -1,    -1,    -1,    -1,     3,     4,
       5,     6,    -1,    79,    80,    10,    11,    12,    13,    14,
      15,    16,    17,    18,    19,    20,    -1,    -1,    23,    24,
      25,    26,    27,    28,    29,    -1,    -1,    32,    33,    34,
      35,    36,    37,    38,    39,    40,    -1,    42,    43,    -1,
      -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,
      -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    63,    -1,
      -1,    -1,    -1,    68,    -1,    -1,    -1,    -1,     3,     4,
       5,     6,    -1,    78,    79,    10,    11,    12,    13,    14,
      15,    16,    17,    18,    19,    20,    -1,    -1,    23,    24,
      25,    26,    27,    28,    29,    -1,    -1,    32,    33,    34,
      35,    36,    37,    38,    39,    40,    -1,    42,    43,    -1,
      -1,     3,     4,     5,     6,    -1,    -1,    -1,    10,    11,
      12,    13,    14,    15,    16,    -1,    -1,    -1,    63,    -1,
      -1,    -1,    -1,    68,    26,    -1,    -1,    -1,    -1,    -1,
      32,    -1,    -1,    -1,    79,    37,    38,    39,    -1,    -1,
      42,    -1,    -1,    -1,     3,     4,     5,     6,    -1,    -1,
      -1,    10,    11,    12,    13,    14,    15,    16,    -1,    -1,
      -1,    63,    -1,    -1,    -1,    -1,    68,    26,    -1,    -1,
      -1,    -1,    74,    32,    -1,    -1,    -1,    79,    37,    38,
      39,    -1,    -1,    42,    -1,    -1,    -1,     3,     4,     5,
       6,    -1,    -1,    -1,    10,    11,    12,    13,    14,    15,
      16,    -1,    -1,    -1,    63,    -1,    -1,    -1,    -1,    68,
      26,    -1,    -1,    -1,    -1,    -1,    32,    -1,    77,    -1,
      79,    37,    38,    39,    -1,    -1,    42,    -1,    -1,    -1,
      -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,
      -1,    -1,    -1,    -1,    -1,    -1,    -1,    63,    -1,    -1,
      -1,    -1,    68,    -1,    -1,    -1,    -1,    -1,    -1,    -1,
      -1,    -1,    -1,    79
};

/* YYSTOS[STATE-NUM] -- The symbol kind of the accessing symbol of
   state STATE-NUM.  */
static const yytype_int8 yystos[] =
{
       0,    83,    84,     0,     3,     4,     5,     6,    10,    11,
      12,    13,    14,    15,    16,    17,    18,    19,    20,    23,
      24,    25,    26,    27,    28,    29,    32,    33,    34,    35,
      36,    37,    38,    39,    40,    42,    43,    63,    68,    79,
      85,    86,    87,    88,    89,    90,    91,    92,    94,    95,
     101,   102,   103,   104,   105,   106,   107,   108,   110,    62,
      73,    75,    64,    77,    73,    79,    10,    11,    12,    14,
      10,    11,    12,   101,   101,    79,    79,   101,    14,    77,
      99,   101,    74,   101,   101,    12,    12,    79,    79,    99,
      77,   100,   100,   100,   101,    99,   101,    14,    10,   106,
     106,   101,    74,    57,    58,    48,    49,    51,    52,    53,
      54,    55,    56,    60,    64,    65,    66,    67,    68,    61,
      69,    70,    71,    73,   101,    14,   101,    14,   101,    46,
      80,   101,   109,    62,    62,    62,    10,    11,    12,    62,
      75,    64,    77,    74,    74,   101,   101,    50,    99,    79,
      84,    74,    74,    77,    64,    77,    64,    77,   101,   101,
     101,   101,   101,   101,    41,    77,    80,   104,   104,   104,
     104,   104,   104,   104,   104,   104,   104,     7,     8,     9,
     104,   104,   105,   105,   105,   106,   106,   106,   106,    14,
      74,    62,    76,    65,    78,    79,    80,    81,   101,   101,
     101,    62,    74,    62,    74,    62,    74,   101,   101,    14,
     101,    80,    80,    10,    10,    80,    98,    78,    93,    14,
     101,    14,   101,    80,    80,    78,    99,    96,    79,   101,
      62,    62,    62,    14,    80,    97,   101,    74,    74,    74,
     101,   101,   101,    76,    65,    78,    99,    99,    99,    99,
      80,    81,    30,    31,    78,    65,    78,    65,    78,    99,
      99,    44,    45,    78,    80,   109,    74,   101,   101,   101,
      47,    80,    81,    74,    74,    74,    21,    99,    10,   101,
      99,    74,    74,    21,    14,    10,    13,    80,    74,    74,
      74,   101,    14,    86,    99,    99,    78,    99,    79,    74,
      74,    47,    80,    98,   101,    99,    80,    99
};

/* YYR1[RULE-NUM] -- Symbol kind of the left-hand side of rule RULE-NUM.  */
static const yytype_int8 yyr1[] =
{
       0,    82,    83,    84,    84,    85,    85,    85,    85,    85,
      85,    85,    85,    85,    85,    85,    85,    85,    85,    85,
      85,    85,    85,    85,    85,    85,    85,    85,    85,    85,
      85,    85,    85,    85,    85,    85,    85,    85,    86,    86,
      86,    87,    88,    88,    89,    90,    91,    91,    92,    92,
      93,    93,    94,    94,    95,    96,    96,    96,    96,    96,
      97,    97,    98,    98,    99,   100,   101,   101,   101,   102,
     102,   102,   102,   102,   102,   102,   102,   102,   102,   102,
     102,   102,   102,   103,   103,   103,   104,   104,   104,   104,
     105,   105,   105,   105,   105,   106,   106,   106,   107,   108,
     108,   108,   108,   108,   108,   108,   108,   108,   108,   108,
     108,   108,   109,   109,   110,   110,   110,   110,   110,   110,
     110,   110,   110,   110,   110,   110,   110,   110,   110,   110,
     110
};

/* YYR2[RULE-NUM] -- Number of symbols on the right-hand side of rule RULE-NUM.  */
static const yytype_int8 yyr2[] =
{
       0,     2,     1,     0,     2,     5,     5,     5,     6,     6,
       6,     4,     4,     4,     3,     3,     3,     3,     2,     4,
       6,     7,     7,     7,     6,     6,     2,     1,     1,     1,
       1,     2,     4,     1,     1,     1,     1,     1,     5,     7,
       7,     5,     5,     7,     5,     2,     5,     3,     5,     7,
       0,     4,     6,     5,     5,     0,     4,     4,     7,     6,
       3,     5,     1,     3,     3,     3,     3,     2,     1,     3,
       3,     3,     3,     3,     3,     3,     3,     3,     3,     3,
       3,     3,     1,     3,     3,     1,     3,     3,     3,     1,
       3,     3,     3,     3,     1,     2,     2,     1,     1,     4,
       3,     6,     5,     6,     5,     3,     2,     3,     3,     2,
       3,     1,     1,     3,     1,     1,     1,     1,     1,     1,
       1,     1,     1,     4,     4,     4,     5,     5,     1,     1,
       3
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
                    EXPR_t *e = all->items[i];
                    if (!e || !(e->kind==E_FNC && (e->ival & SUB_TAG))) continue;
                    e->ival &= ~SUB_TAG;   /* restore real nparams */
                    add_proc(e);
                    all->items[i] = NULL;  /* mark consumed */
                }
                /* Pass 2: wrap remaining body stmts in synthetic "main" E_FNC */
                int has_body = 0;
                for (int i = 0; i < all->count; i++) if (all->items[i]) { has_body=1; break; }
                if (has_body) {
                    EXPR_t *mf = leaf_sval(E_FNC, "main"); mf->ival = 0;
                    EXPR_t *mn = expr_new(E_VAR); mn->sval = intern("main");
                    expr_add_child(mf, mn);
                    for (int i = 0; i < all->count; i++)
                        if (all->items[i]) expr_add_child(mf, all->items[i]);
                    add_proc(mf);
                }
                exprlist_free(all);
            }
        }
#line 1603 "raku.tab.c"
    break;

  case 3: /* stmt_list: %empty  */
#line 249 "raku.y"
                     { (yyval.list) = exprlist_new(); }
#line 1609 "raku.tab.c"
    break;

  case 4: /* stmt_list: stmt_list stmt  */
#line 250 "raku.y"
                     { (yyval.list) = exprlist_append((yyvsp[-1].list), (yyvsp[0].node)); }
#line 1615 "raku.tab.c"
    break;

  case 5: /* stmt: KW_MY VAR_SCALAR '=' expr ';'  */
#line 255 "raku.y"
        { (yyval.node) = expr_binary(E_ASSIGN, var_node((yyvsp[-3].sval)), (yyvsp[-1].node)); }
#line 1621 "raku.tab.c"
    break;

  case 6: /* stmt: KW_MY VAR_ARRAY '=' expr ';'  */
#line 257 "raku.y"
        { (yyval.node) = expr_binary(E_ASSIGN, var_node((yyvsp[-3].sval)), (yyvsp[-1].node)); }
#line 1627 "raku.tab.c"
    break;

  case 7: /* stmt: KW_MY VAR_HASH '=' expr ';'  */
#line 259 "raku.y"
        { (yyval.node) = expr_binary(E_ASSIGN, var_node((yyvsp[-3].sval)), (yyvsp[-1].node)); }
#line 1633 "raku.tab.c"
    break;

  case 8: /* stmt: KW_MY IDENT VAR_SCALAR '=' expr ';'  */
#line 262 "raku.y"
        { free((yyvsp[-4].sval)); (yyval.node) = expr_binary(E_ASSIGN, var_node((yyvsp[-3].sval)), (yyvsp[-1].node)); }
#line 1639 "raku.tab.c"
    break;

  case 9: /* stmt: KW_MY IDENT VAR_ARRAY '=' expr ';'  */
#line 264 "raku.y"
        { free((yyvsp[-4].sval)); (yyval.node) = expr_binary(E_ASSIGN, var_node((yyvsp[-3].sval)), (yyvsp[-1].node)); }
#line 1645 "raku.tab.c"
    break;

  case 10: /* stmt: KW_MY IDENT VAR_HASH '=' expr ';'  */
#line 266 "raku.y"
        { free((yyvsp[-4].sval)); (yyval.node) = expr_binary(E_ASSIGN, var_node((yyvsp[-3].sval)), (yyvsp[-1].node)); }
#line 1651 "raku.tab.c"
    break;

  case 11: /* stmt: KW_MY IDENT VAR_SCALAR ';'  */
#line 268 "raku.y"
        { free((yyvsp[-2].sval)); (yyval.node) = expr_binary(E_ASSIGN, var_node((yyvsp[-1].sval)), leaf_sval(E_QLIT, "")); }
#line 1657 "raku.tab.c"
    break;

  case 12: /* stmt: KW_MY IDENT VAR_ARRAY ';'  */
#line 270 "raku.y"
        { free((yyvsp[-2].sval)); (yyval.node) = expr_binary(E_ASSIGN, var_node((yyvsp[-1].sval)), leaf_sval(E_QLIT, "")); }
#line 1663 "raku.tab.c"
    break;

  case 13: /* stmt: KW_MY IDENT VAR_HASH ';'  */
#line 272 "raku.y"
        { free((yyvsp[-2].sval)); (yyval.node) = expr_binary(E_ASSIGN, var_node((yyvsp[-1].sval)), leaf_sval(E_QLIT, "")); }
#line 1669 "raku.tab.c"
    break;

  case 14: /* stmt: KW_SAY expr ';'  */
#line 274 "raku.y"
        { EXPR_t *c=make_call("write"); expr_add_child(c,(yyvsp[-1].node)); (yyval.node)=c; }
#line 1675 "raku.tab.c"
    break;

  case 15: /* stmt: KW_PRINT expr ';'  */
#line 276 "raku.y"
        { EXPR_t *c=make_call("writes"); expr_add_child(c,(yyvsp[-1].node)); (yyval.node)=c; }
#line 1681 "raku.tab.c"
    break;

  case 16: /* stmt: KW_TAKE expr ';'  */
#line 278 "raku.y"
        { (yyval.node)=expr_unary(E_SUSPEND,(yyvsp[-1].node)); }
#line 1687 "raku.tab.c"
    break;

  case 17: /* stmt: KW_RETURN expr ';'  */
#line 280 "raku.y"
        { EXPR_t *r=expr_new(E_RETURN); expr_add_child(r,(yyvsp[-1].node)); (yyval.node)=r; }
#line 1693 "raku.tab.c"
    break;

  case 18: /* stmt: KW_RETURN ';'  */
#line 282 "raku.y"
        { (yyval.node)=expr_new(E_RETURN); }
#line 1699 "raku.tab.c"
    break;

  case 19: /* stmt: VAR_SCALAR '=' expr ';'  */
#line 284 "raku.y"
        { (yyval.node)=expr_binary(E_ASSIGN,var_node((yyvsp[-3].sval)),(yyvsp[-1].node)); }
#line 1705 "raku.tab.c"
    break;

  case 20: /* stmt: VAR_SCALAR '.' IDENT '=' expr ';'  */
#line 287 "raku.y"
        { EXPR_t *fe=expr_new(E_FIELD);
          fe->sval=(char*)intern((yyvsp[-3].sval)); free((yyvsp[-3].sval));
          expr_add_child(fe,var_node((yyvsp[-5].sval)));
          (yyval.node)=expr_binary(E_ASSIGN,fe,(yyvsp[-1].node)); }
#line 1714 "raku.tab.c"
    break;

  case 21: /* stmt: VAR_ARRAY '[' expr ']' '=' expr ';'  */
#line 292 "raku.y"
        { EXPR_t *c=make_call("arr_set");
          expr_add_child(c,var_node((yyvsp[-6].sval))); expr_add_child(c,(yyvsp[-4].node)); expr_add_child(c,(yyvsp[-1].node)); (yyval.node)=c; }
#line 1721 "raku.tab.c"
    break;

  case 22: /* stmt: VAR_HASH '<' IDENT '>' '=' expr ';'  */
#line 295 "raku.y"
        { EXPR_t *c=make_call("hash_set");
          expr_add_child(c,var_node((yyvsp[-6].sval))); expr_add_child(c,leaf_sval(E_QLIT,(yyvsp[-4].sval))); expr_add_child(c,(yyvsp[-1].node)); (yyval.node)=c; }
#line 1728 "raku.tab.c"
    break;

  case 23: /* stmt: VAR_HASH '{' expr '}' '=' expr ';'  */
#line 298 "raku.y"
        { EXPR_t *c=make_call("hash_set");
          expr_add_child(c,var_node((yyvsp[-6].sval))); expr_add_child(c,(yyvsp[-4].node)); expr_add_child(c,(yyvsp[-1].node)); (yyval.node)=c; }
#line 1735 "raku.tab.c"
    break;

  case 24: /* stmt: KW_DELETE VAR_HASH '<' IDENT '>' ';'  */
#line 301 "raku.y"
        { EXPR_t *c=make_call("hash_delete");
          expr_add_child(c,var_node((yyvsp[-4].sval))); expr_add_child(c,leaf_sval(E_QLIT,(yyvsp[-2].sval))); (yyval.node)=c; }
#line 1742 "raku.tab.c"
    break;

  case 25: /* stmt: KW_DELETE VAR_HASH '{' expr '}' ';'  */
#line 304 "raku.y"
        { EXPR_t *c=make_call("hash_delete");
          expr_add_child(c,var_node((yyvsp[-4].sval))); expr_add_child(c,(yyvsp[-2].node)); (yyval.node)=c; }
#line 1749 "raku.tab.c"
    break;

  case 26: /* stmt: expr ';'  */
#line 306 "raku.y"
               { (yyval.node)=(yyvsp[-1].node); }
#line 1755 "raku.tab.c"
    break;

  case 27: /* stmt: if_stmt  */
#line 307 "raku.y"
                        { (yyval.node)=(yyvsp[0].node); }
#line 1761 "raku.tab.c"
    break;

  case 28: /* stmt: while_stmt  */
#line 308 "raku.y"
                        { (yyval.node)=(yyvsp[0].node); }
#line 1767 "raku.tab.c"
    break;

  case 29: /* stmt: for_stmt  */
#line 309 "raku.y"
                        { (yyval.node)=(yyvsp[0].node); }
#line 1773 "raku.tab.c"
    break;

  case 30: /* stmt: given_stmt  */
#line 310 "raku.y"
                        { (yyval.node)=(yyvsp[0].node); }
#line 1779 "raku.tab.c"
    break;

  case 31: /* stmt: KW_TRY block  */
#line 313 "raku.y"
        { EXPR_t *c=make_call("raku_try");
          expr_add_child(c,(yyvsp[0].node)); (yyval.node)=c; }
#line 1786 "raku.tab.c"
    break;

  case 32: /* stmt: KW_TRY block KW_CATCH block  */
#line 316 "raku.y"
        { EXPR_t *c=make_call("raku_try");
          expr_add_child(c,(yyvsp[-2].node)); expr_add_child(c,(yyvsp[0].node)); (yyval.node)=c; }
#line 1793 "raku.tab.c"
    break;

  case 33: /* stmt: unless_stmt  */
#line 318 "raku.y"
                        { (yyval.node)=(yyvsp[0].node); }
#line 1799 "raku.tab.c"
    break;

  case 34: /* stmt: until_stmt  */
#line 319 "raku.y"
                        { (yyval.node)=(yyvsp[0].node); }
#line 1805 "raku.tab.c"
    break;

  case 35: /* stmt: repeat_stmt  */
#line 320 "raku.y"
                        { (yyval.node)=(yyvsp[0].node); }
#line 1811 "raku.tab.c"
    break;

  case 36: /* stmt: sub_decl  */
#line 321 "raku.y"
                        { (yyval.node)=(yyvsp[0].node); }
#line 1817 "raku.tab.c"
    break;

  case 37: /* stmt: class_decl  */
#line 322 "raku.y"
                        { (yyval.node)=(yyvsp[0].node); }
#line 1823 "raku.tab.c"
    break;

  case 38: /* if_stmt: KW_IF '(' expr ')' block  */
#line 327 "raku.y"
        { EXPR_t *e=expr_new(E_IF); expr_add_child(e,(yyvsp[-2].node)); expr_add_child(e,(yyvsp[0].node)); (yyval.node)=e; }
#line 1829 "raku.tab.c"
    break;

  case 39: /* if_stmt: KW_IF '(' expr ')' block KW_ELSE block  */
#line 329 "raku.y"
        { EXPR_t *e=expr_new(E_IF); expr_add_child(e,(yyvsp[-4].node)); expr_add_child(e,(yyvsp[-2].node)); expr_add_child(e,(yyvsp[0].node)); (yyval.node)=e; }
#line 1835 "raku.tab.c"
    break;

  case 40: /* if_stmt: KW_IF '(' expr ')' block KW_ELSE if_stmt  */
#line 331 "raku.y"
        { EXPR_t *e=expr_new(E_IF); expr_add_child(e,(yyvsp[-4].node)); expr_add_child(e,(yyvsp[-2].node)); expr_add_child(e,(yyvsp[0].node)); (yyval.node)=e; }
#line 1841 "raku.tab.c"
    break;

  case 41: /* while_stmt: KW_WHILE '(' expr ')' block  */
#line 336 "raku.y"
        { (yyval.node)=expr_binary(E_WHILE,(yyvsp[-2].node),(yyvsp[0].node)); }
#line 1847 "raku.tab.c"
    break;

  case 42: /* unless_stmt: KW_UNLESS '(' expr ')' block  */
#line 342 "raku.y"
        { EXPR_t *e=expr_new(E_IF); expr_add_child(e,expr_unary(E_NOT,(yyvsp[-2].node))); expr_add_child(e,(yyvsp[0].node)); (yyval.node)=e; }
#line 1853 "raku.tab.c"
    break;

  case 43: /* unless_stmt: KW_UNLESS '(' expr ')' block KW_ELSE block  */
#line 344 "raku.y"
        { EXPR_t *e=expr_new(E_IF); expr_add_child(e,expr_unary(E_NOT,(yyvsp[-4].node))); expr_add_child(e,(yyvsp[-2].node)); expr_add_child(e,(yyvsp[0].node)); (yyval.node)=e; }
#line 1859 "raku.tab.c"
    break;

  case 44: /* until_stmt: KW_UNTIL '(' expr ')' block  */
#line 350 "raku.y"
        { EXPR_t *e=expr_new(E_UNTIL); expr_add_child(e,(yyvsp[-2].node)); expr_add_child(e,(yyvsp[0].node)); (yyval.node)=e; }
#line 1865 "raku.tab.c"
    break;

  case 45: /* repeat_stmt: KW_REPEAT block  */
#line 356 "raku.y"
        { EXPR_t *e=expr_new(E_REPEAT); expr_add_child(e,(yyvsp[0].node)); (yyval.node)=e; }
#line 1871 "raku.tab.c"
    break;

  case 46: /* for_stmt: KW_FOR expr OP_ARROW VAR_SCALAR block  */
#line 362 "raku.y"
        { EXPR_t *iter=(yyvsp[-3].node); const char *vname=strip_sigil((yyvsp[-1].sval));
          if (iter->kind==E_TO) {
              /* range case: lo=children[0], hi=children[1] */
              (yyval.node) = make_for_range(iter->children[0], iter->children[1], vname, (yyvsp[0].node));
          } else {
              /* Always wrap in E_ITERATE so loopvar goes on the wrapper node.
               * RK-16/RK-21: gen->sval = loopvar name for icn_drive / E_EVERY binding. */
              const char *vn = intern(strip_sigil((yyvsp[-1].sval)));
              EXPR_t *gen = expr_unary(E_ITERATE, iter);
              gen->sval = (char *)vn;
              (yyval.node)=expr_binary(E_EVERY,gen,(yyvsp[0].node));
          } }
#line 1888 "raku.tab.c"
    break;

  case 47: /* for_stmt: KW_FOR expr block  */
#line 375 "raku.y"
        { EXPR_t *gen=((yyvsp[-1].node)->kind==E_VAR)?expr_unary(E_ITERATE,(yyvsp[-1].node)):(yyvsp[-1].node);
          (yyval.node)=expr_binary(E_EVERY,gen,(yyvsp[0].node)); }
#line 1895 "raku.tab.c"
    break;

  case 48: /* given_stmt: KW_GIVEN expr '{' when_list '}'  */
#line 381 "raku.y"
        { /* RK-18d: E_CASE[ topic, cmpnode0, val0, body0, cmpnode1, val1, body1, ... ]
           * cmp kind stored in separate E_ILIT node (ival=EKind) to avoid corrupting val->ival. */
          EXPR_t *ec=expr_new(E_CASE);
          expr_add_child(ec,(yyvsp[-3].node));
          ExprList *whens=(yyvsp[-1].list);
          for(int i=0;i<whens->count;i++){
              EXPR_t *pair=whens->items[i];
              EKind cmp=(EKind)(pair->ival);
              EXPR_t *val=pair->children[0], *body=pair->children[1];
              EXPR_t *cn=expr_new(E_ILIT); cn->ival=(long)cmp;
              expr_add_child(ec,cn); expr_add_child(ec,val); expr_add_child(ec,body);
          }
          exprlist_free(whens);
          (yyval.node)=ec; }
#line 1914 "raku.tab.c"
    break;

  case 49: /* given_stmt: KW_GIVEN expr '{' when_list KW_DEFAULT block '}'  */
#line 396 "raku.y"
        { /* RK-18d: E_CASE with default: E_NUL cmpnode + E_NUL val + body at end. */
          EXPR_t *ec=expr_new(E_CASE);
          expr_add_child(ec,(yyvsp[-5].node));
          ExprList *whens=(yyvsp[-3].list);
          for(int i=0;i<whens->count;i++){
              EXPR_t *pair=whens->items[i];
              EKind cmp=(EKind)(pair->ival);
              EXPR_t *val=pair->children[0], *body=pair->children[1];
              EXPR_t *cn=expr_new(E_ILIT); cn->ival=(long)cmp;
              expr_add_child(ec,cn); expr_add_child(ec,val); expr_add_child(ec,body);
          }
          exprlist_free(whens);
          expr_add_child(ec,expr_new(E_NUL)); expr_add_child(ec,expr_new(E_NUL)); expr_add_child(ec,(yyvsp[-1].node));
          (yyval.node)=ec; }
#line 1933 "raku.tab.c"
    break;

  case 50: /* when_list: %empty  */
#line 413 "raku.y"
                   { (yyval.list)=exprlist_new(); }
#line 1939 "raku.tab.c"
    break;

  case 51: /* when_list: when_list KW_WHEN expr block  */
#line 415 "raku.y"
        { EKind cmp=((yyvsp[-1].node)->kind==E_QLIT)?E_LEQ:E_EQ;
          EXPR_t *pair=expr_new(E_SEQ_EXPR);
          pair->ival=(long)cmp;
          expr_add_child(pair,(yyvsp[-1].node)); expr_add_child(pair,(yyvsp[0].node));
          (yyval.list)=exprlist_append((yyvsp[-3].list),pair); }
#line 1949 "raku.tab.c"
    break;

  case 52: /* sub_decl: KW_SUB IDENT '(' param_list ')' block  */
#line 424 "raku.y"
        { ExprList *params=(yyvsp[-2].list); int np=params?params->count:0;
          EXPR_t *e=leaf_sval(E_FNC,(yyvsp[-4].sval)); e->ival=(long)np|SUB_TAG;
          EXPR_t *nn=expr_new(E_VAR); nn->sval=intern((yyvsp[-4].sval)); expr_add_child(e,nn);
          if(params){ for(int i=0;i<np;i++) expr_add_child(e,params->items[i]); exprlist_free(params); }
          EXPR_t *body=(yyvsp[0].node);
          for(int i=0;i<body->nchildren;i++) expr_add_child(e,body->children[i]);
          (yyval.node)=e; }
#line 1961 "raku.tab.c"
    break;

  case 53: /* sub_decl: KW_SUB IDENT '(' ')' block  */
#line 432 "raku.y"
        { EXPR_t *e=leaf_sval(E_FNC,(yyvsp[-3].sval)); e->ival=(long)0|SUB_TAG;
          EXPR_t *nn=expr_new(E_VAR); nn->sval=intern((yyvsp[-3].sval)); expr_add_child(e,nn);
          EXPR_t *body=(yyvsp[0].node);
          for(int i=0;i<body->nchildren;i++) expr_add_child(e,body->children[i]);
          (yyval.node)=e; }
#line 1971 "raku.tab.c"
    break;

  case 54: /* class_decl: KW_CLASS IDENT '{' class_body_list '}'  */
#line 444 "raku.y"
        {
            const char *cname = intern((yyvsp[-3].sval)); free((yyvsp[-3].sval));
            ExprList *body = (yyvsp[-1].list);
            EXPR_t *rec = expr_new(E_RECORD);
            rec->sval = (char *)cname;
            if (body) {
                for (int i = 0; i < body->count; i++) {
                    EXPR_t *item = body->items[i];
                    if (!item) continue;
                    if (item->kind == E_VAR) {
                        expr_add_child(rec, item);
                    } else if (item->kind == E_FNC && (item->ival & SUB_TAG)) {
                        char fullname[256];
                        snprintf(fullname, sizeof fullname, "%s__%s", cname, item->sval);
                        const char *fname = intern(fullname);
                        raku_meth_register(cname, item->sval, fname);
                        item->sval = (char *)fname;
                        if (item->nchildren > 0 && item->children[0]->kind == E_VAR)
                            item->children[0]->sval = (char *)fname;
                        item->ival &= ~SUB_TAG;
                        add_proc(item);
                        body->items[i] = NULL;
                    }
                }
                exprlist_free(body);
            }
            add_proc(rec);
            (yyval.node) = expr_new(E_NUL);
        }
#line 2005 "raku.tab.c"
    break;

  case 55: /* class_body_list: %empty  */
#line 476 "raku.y"
                   { (yyval.list) = exprlist_new(); }
#line 2011 "raku.tab.c"
    break;

  case 56: /* class_body_list: class_body_list KW_HAS VAR_TWIGIL ';'  */
#line 478 "raku.y"
        { EXPR_t *fv = leaf_sval(E_VAR, (yyvsp[-1].sval)); free((yyvsp[-1].sval));
          (yyval.list) = exprlist_append((yyvsp[-3].list), fv); }
#line 2018 "raku.tab.c"
    break;

  case 57: /* class_body_list: class_body_list KW_HAS VAR_SCALAR ';'  */
#line 481 "raku.y"
        { EXPR_t *fv = leaf_sval(E_VAR, strip_sigil((yyvsp[-1].sval))); free((yyvsp[-1].sval));
          (yyval.list) = exprlist_append((yyvsp[-3].list), fv); }
#line 2025 "raku.tab.c"
    break;

  case 58: /* class_body_list: class_body_list KW_METHOD IDENT '(' param_list ')' block  */
#line 484 "raku.y"
        { ExprList *params = (yyvsp[-2].list); int np = params ? params->count : 0;
          EXPR_t *e = leaf_sval(E_FNC, (yyvsp[-4].sval));
          e->ival = (long)(np + 1) | SUB_TAG;
          EXPR_t *nn = expr_new(E_VAR); nn->sval = intern((yyvsp[-4].sval)); expr_add_child(e, nn);
          expr_add_child(e, leaf_sval(E_VAR, "self"));
          if (params) { for (int i = 0; i < np; i++) expr_add_child(e, params->items[i]); exprlist_free(params); }
          EXPR_t *body = (yyvsp[0].node);
          for (int i = 0; i < body->nchildren; i++) expr_add_child(e, body->children[i]);
          free((yyvsp[-4].sval));
          (yyval.list) = exprlist_append((yyvsp[-6].list), e); }
#line 2040 "raku.tab.c"
    break;

  case 59: /* class_body_list: class_body_list KW_METHOD IDENT '(' ')' block  */
#line 495 "raku.y"
        { EXPR_t *e = leaf_sval(E_FNC, (yyvsp[-3].sval));
          e->ival = (long)(1) | SUB_TAG;
          EXPR_t *nn = expr_new(E_VAR); nn->sval = intern((yyvsp[-3].sval)); expr_add_child(e, nn);
          expr_add_child(e, leaf_sval(E_VAR, "self"));
          EXPR_t *body = (yyvsp[0].node);
          for (int i = 0; i < body->nchildren; i++) expr_add_child(e, body->children[i]);
          free((yyvsp[-3].sval));
          (yyval.list) = exprlist_append((yyvsp[-5].list), e); }
#line 2053 "raku.tab.c"
    break;

  case 60: /* named_arg_list: IDENT OP_FATARROW expr  */
#line 507 "raku.y"
        { (yyval.list) = exprlist_new();
          exprlist_append((yyval.list), leaf_sval(E_QLIT, (yyvsp[-2].sval))); free((yyvsp[-2].sval));
          exprlist_append((yyval.list), (yyvsp[0].node)); }
#line 2061 "raku.tab.c"
    break;

  case 61: /* named_arg_list: named_arg_list ',' IDENT OP_FATARROW expr  */
#line 511 "raku.y"
        { exprlist_append((yyvsp[-4].list), leaf_sval(E_QLIT, (yyvsp[-2].sval))); free((yyvsp[-2].sval));
          exprlist_append((yyvsp[-4].list), (yyvsp[0].node));
          (yyval.list) = (yyvsp[-4].list); }
#line 2069 "raku.tab.c"
    break;

  case 62: /* param_list: VAR_SCALAR  */
#line 517 "raku.y"
                             { (yyval.list)=exprlist_append(exprlist_new(),var_node((yyvsp[0].sval))); }
#line 2075 "raku.tab.c"
    break;

  case 63: /* param_list: param_list ',' VAR_SCALAR  */
#line 518 "raku.y"
                                { (yyval.list)=exprlist_append((yyvsp[-2].list),var_node((yyvsp[0].sval))); }
#line 2081 "raku.tab.c"
    break;

  case 64: /* block: '{' stmt_list '}'  */
#line 522 "raku.y"
                         { (yyval.node)=make_seq((yyvsp[-1].list)); }
#line 2087 "raku.tab.c"
    break;

  case 65: /* closure: '{' expr '}'  */
#line 527 "raku.y"
                    { (yyval.node)=(yyvsp[-1].node); }
#line 2093 "raku.tab.c"
    break;

  case 66: /* expr: VAR_SCALAR '=' expr  */
#line 531 "raku.y"
                           { (yyval.node)=expr_binary(E_ASSIGN,var_node((yyvsp[-2].sval)),(yyvsp[0].node)); }
#line 2099 "raku.tab.c"
    break;

  case 67: /* expr: KW_GATHER block  */
#line 532 "raku.y"
                           {
          /* RK-21: gather { block } → anonymous coroutine sub + call.
           * 1. Build E_FNC def with SUB_TAG (like sub_decl) named __gather_N.
           * 2. add_proc() so it lands in the proc table.
           * 3. Return a call E_FNC (no SUB_TAG) so icn_eval_gen wraps it as
           *    icn_bb_suspend — a BB_PUMP coroutine collecting E_SUSPEND (take) values. */
          static int gather_seq = 0;
          char gname[32]; snprintf(gname, sizeof gname, "__gather_%d", gather_seq++);
          /* Build the def node */
          EXPR_t *def = leaf_sval(E_FNC, gname); def->ival = (long)0 | SUB_TAG;
          EXPR_t *dn  = expr_new(E_VAR); dn->sval = intern(gname);
          expr_add_child(def, dn);
          /* Splice block children into def */
          EXPR_t *blk = (yyvsp[0].node);
          for (int i = 0; i < blk->nchildren; i++) expr_add_child(def, blk->children[i]);
          def->ival &= ~SUB_TAG;   /* strip sentinel — restore real nparams (0) for icn_call_proc */
          add_proc(def);
          /* Build the call node (no SUB_TAG) */
          EXPR_t *call = leaf_sval(E_FNC, gname);
          EXPR_t *cn   = expr_new(E_VAR); cn->sval = intern(gname);
          expr_add_child(call, cn);
          (yyval.node) = call;
      }
#line 2127 "raku.tab.c"
    break;

  case 68: /* expr: cmp_expr  */
#line 555 "raku.y"
                           { (yyval.node)=(yyvsp[0].node); }
#line 2133 "raku.tab.c"
    break;

  case 69: /* cmp_expr: cmp_expr OP_AND add_expr  */
#line 559 "raku.y"
                                { (yyval.node)=expr_binary(E_SEQ,(yyvsp[-2].node),(yyvsp[0].node)); }
#line 2139 "raku.tab.c"
    break;

  case 70: /* cmp_expr: cmp_expr OP_OR add_expr  */
#line 560 "raku.y"
                                { (yyval.node)=expr_binary(E_ALT,(yyvsp[-2].node),(yyvsp[0].node)); }
#line 2145 "raku.tab.c"
    break;

  case 71: /* cmp_expr: add_expr OP_EQ add_expr  */
#line 561 "raku.y"
                                { (yyval.node)=expr_binary(E_EQ,(yyvsp[-2].node),(yyvsp[0].node)); }
#line 2151 "raku.tab.c"
    break;

  case 72: /* cmp_expr: add_expr OP_NE add_expr  */
#line 562 "raku.y"
                                { (yyval.node)=expr_binary(E_NE,(yyvsp[-2].node),(yyvsp[0].node)); }
#line 2157 "raku.tab.c"
    break;

  case 73: /* cmp_expr: add_expr '<' add_expr  */
#line 563 "raku.y"
                                { (yyval.node)=expr_binary(E_LT,(yyvsp[-2].node),(yyvsp[0].node)); }
#line 2163 "raku.tab.c"
    break;

  case 74: /* cmp_expr: add_expr '>' add_expr  */
#line 564 "raku.y"
                                { (yyval.node)=expr_binary(E_GT,(yyvsp[-2].node),(yyvsp[0].node)); }
#line 2169 "raku.tab.c"
    break;

  case 75: /* cmp_expr: add_expr OP_LE add_expr  */
#line 565 "raku.y"
                                { (yyval.node)=expr_binary(E_LE,(yyvsp[-2].node),(yyvsp[0].node)); }
#line 2175 "raku.tab.c"
    break;

  case 76: /* cmp_expr: add_expr OP_GE add_expr  */
#line 566 "raku.y"
                                { (yyval.node)=expr_binary(E_GE,(yyvsp[-2].node),(yyvsp[0].node)); }
#line 2181 "raku.tab.c"
    break;

  case 77: /* cmp_expr: add_expr OP_SEQ add_expr  */
#line 567 "raku.y"
                                { (yyval.node)=expr_binary(E_LEQ,(yyvsp[-2].node),(yyvsp[0].node)); }
#line 2187 "raku.tab.c"
    break;

  case 78: /* cmp_expr: add_expr OP_SNE add_expr  */
#line 568 "raku.y"
                                { (yyval.node)=expr_binary(E_LNE,(yyvsp[-2].node),(yyvsp[0].node)); }
#line 2193 "raku.tab.c"
    break;

  case 79: /* cmp_expr: add_expr OP_SMATCH LIT_REGEX  */
#line 570 "raku.y"
        { /* RK-23: $s ~~ /pattern/ */
          EXPR_t *c = make_call("raku_match");
          expr_add_child(c, (yyvsp[-2].node));
          expr_add_child(c, leaf_sval(E_QLIT, (yyvsp[0].sval)));
          (yyval.node) = c; }
#line 2203 "raku.tab.c"
    break;

  case 80: /* cmp_expr: add_expr OP_SMATCH LIT_MATCH_GLOBAL  */
#line 576 "raku.y"
        { /* RK-37: $s ~~ m:g/pat/ — global match, returns SOH-sep match list */
          EXPR_t *c = make_call("raku_match_global");
          expr_add_child(c, (yyvsp[-2].node));
          expr_add_child(c, leaf_sval(E_QLIT, (yyvsp[0].sval)));
          (yyval.node) = c; }
#line 2213 "raku.tab.c"
    break;

  case 81: /* cmp_expr: add_expr OP_SMATCH LIT_SUBST  */
#line 582 "raku.y"
        { /* RK-37: $s ~~ s/pat/repl/[g] — substitution */
          EXPR_t *c = make_call("raku_subst");
          expr_add_child(c, (yyvsp[-2].node));
          expr_add_child(c, leaf_sval(E_QLIT, (yyvsp[0].sval)));
          (yyval.node) = c; }
#line 2223 "raku.tab.c"
    break;

  case 82: /* cmp_expr: range_expr  */
#line 587 "raku.y"
                               { (yyval.node)=(yyvsp[0].node); }
#line 2229 "raku.tab.c"
    break;

  case 83: /* range_expr: add_expr OP_RANGE add_expr  */
#line 591 "raku.y"
                                    { (yyval.node)=expr_binary(E_TO,(yyvsp[-2].node),(yyvsp[0].node)); }
#line 2235 "raku.tab.c"
    break;

  case 84: /* range_expr: add_expr OP_RANGE_EX add_expr  */
#line 592 "raku.y"
                                    { (yyval.node)=expr_binary(E_TO,(yyvsp[-2].node),(yyvsp[0].node)); }
#line 2241 "raku.tab.c"
    break;

  case 85: /* range_expr: add_expr  */
#line 593 "raku.y"
                                    { (yyval.node)=(yyvsp[0].node); }
#line 2247 "raku.tab.c"
    break;

  case 86: /* add_expr: add_expr '+' mul_expr  */
#line 597 "raku.y"
                             { (yyval.node)=expr_binary(E_ADD,(yyvsp[-2].node),(yyvsp[0].node)); }
#line 2253 "raku.tab.c"
    break;

  case 87: /* add_expr: add_expr '-' mul_expr  */
#line 598 "raku.y"
                             { (yyval.node)=expr_binary(E_SUB,(yyvsp[-2].node),(yyvsp[0].node)); }
#line 2259 "raku.tab.c"
    break;

  case 88: /* add_expr: add_expr '~' mul_expr  */
#line 599 "raku.y"
                             { (yyval.node)=expr_binary(E_CAT,(yyvsp[-2].node),(yyvsp[0].node)); }
#line 2265 "raku.tab.c"
    break;

  case 89: /* add_expr: mul_expr  */
#line 600 "raku.y"
                             { (yyval.node)=(yyvsp[0].node); }
#line 2271 "raku.tab.c"
    break;

  case 90: /* mul_expr: mul_expr '*' unary_expr  */
#line 604 "raku.y"
                                  { (yyval.node)=expr_binary(E_MUL,(yyvsp[-2].node),(yyvsp[0].node)); }
#line 2277 "raku.tab.c"
    break;

  case 91: /* mul_expr: mul_expr '/' unary_expr  */
#line 605 "raku.y"
                                  { (yyval.node)=expr_binary(E_DIV,(yyvsp[-2].node),(yyvsp[0].node)); }
#line 2283 "raku.tab.c"
    break;

  case 92: /* mul_expr: mul_expr '%' unary_expr  */
#line 606 "raku.y"
                                  { (yyval.node)=expr_binary(E_MOD,(yyvsp[-2].node),(yyvsp[0].node)); }
#line 2289 "raku.tab.c"
    break;

  case 93: /* mul_expr: mul_expr OP_DIV unary_expr  */
#line 607 "raku.y"
                                  { (yyval.node)=expr_binary(E_DIV,(yyvsp[-2].node),(yyvsp[0].node)); }
#line 2295 "raku.tab.c"
    break;

  case 94: /* mul_expr: unary_expr  */
#line 608 "raku.y"
                                  { (yyval.node)=(yyvsp[0].node); }
#line 2301 "raku.tab.c"
    break;

  case 95: /* unary_expr: '-' unary_expr  */
#line 612 "raku.y"
                                   { (yyval.node)=expr_unary(E_MNS,(yyvsp[0].node)); }
#line 2307 "raku.tab.c"
    break;

  case 96: /* unary_expr: '!' unary_expr  */
#line 613 "raku.y"
                                   { (yyval.node)=expr_unary(E_NOT,(yyvsp[0].node)); }
#line 2313 "raku.tab.c"
    break;

  case 97: /* unary_expr: postfix_expr  */
#line 614 "raku.y"
                                   { (yyval.node)=(yyvsp[0].node); }
#line 2319 "raku.tab.c"
    break;

  case 98: /* postfix_expr: call_expr  */
#line 617 "raku.y"
                         { (yyval.node)=(yyvsp[0].node); }
#line 2325 "raku.tab.c"
    break;

  case 99: /* call_expr: IDENT '(' arg_list ')'  */
#line 621 "raku.y"
        { EXPR_t *e=make_call((yyvsp[-3].sval));
          ExprList *args=(yyvsp[-1].list);
          if(args){ for(int i=0;i<args->count;i++) expr_add_child(e,args->items[i]); exprlist_free(args); }
          (yyval.node)=e; }
#line 2334 "raku.tab.c"
    break;

  case 100: /* call_expr: IDENT '(' ')'  */
#line 625 "raku.y"
                     { (yyval.node)=make_call((yyvsp[-2].sval)); }
#line 2340 "raku.tab.c"
    break;

  case 101: /* call_expr: IDENT '.' KW_NEW '(' named_arg_list ')'  */
#line 628 "raku.y"
        { EXPR_t *c=make_call("raku_new");
          expr_add_child(c,leaf_sval(E_QLIT,(yyvsp[-5].sval))); free((yyvsp[-5].sval));
          ExprList *nargs=(yyvsp[-1].list);
          if(nargs){ for(int i=0;i<nargs->count;i++) expr_add_child(c,nargs->items[i]); exprlist_free(nargs); }
          (yyval.node)=c; }
#line 2350 "raku.tab.c"
    break;

  case 102: /* call_expr: IDENT '.' KW_NEW '(' ')'  */
#line 634 "raku.y"
        { EXPR_t *c=make_call("raku_new");
          expr_add_child(c,leaf_sval(E_QLIT,(yyvsp[-4].sval))); free((yyvsp[-4].sval));
          (yyval.node)=c; }
#line 2358 "raku.tab.c"
    break;

  case 103: /* call_expr: atom '.' IDENT '(' arg_list ')'  */
#line 639 "raku.y"
        { EXPR_t *c=make_call("raku_mcall");
          expr_add_child(c,(yyvsp[-5].node));
          expr_add_child(c,leaf_sval(E_QLIT,(yyvsp[-3].sval))); free((yyvsp[-3].sval));
          ExprList *args=(yyvsp[-1].list);
          if(args){ for(int i=0;i<args->count;i++) expr_add_child(c,args->items[i]); exprlist_free(args); }
          (yyval.node)=c; }
#line 2369 "raku.tab.c"
    break;

  case 104: /* call_expr: atom '.' IDENT '(' ')'  */
#line 646 "raku.y"
        { EXPR_t *c=make_call("raku_mcall");
          expr_add_child(c,(yyvsp[-4].node));
          expr_add_child(c,leaf_sval(E_QLIT,(yyvsp[-2].sval))); free((yyvsp[-2].sval));
          (yyval.node)=c; }
#line 2378 "raku.tab.c"
    break;

  case 105: /* call_expr: atom '.' IDENT  */
#line 652 "raku.y"
        { EXPR_t *fe=expr_new(E_FIELD);
          fe->sval=(char*)intern((yyvsp[0].sval)); free((yyvsp[0].sval));
          expr_add_child(fe,(yyvsp[-2].node));
          (yyval.node)=fe; }
#line 2387 "raku.tab.c"
    break;

  case 106: /* call_expr: KW_DIE expr  */
#line 659 "raku.y"
        { EXPR_t *c=make_call("raku_die");
          expr_add_child(c,(yyvsp[0].node)); (yyval.node)=c; }
#line 2394 "raku.tab.c"
    break;

  case 107: /* call_expr: KW_MAP closure expr  */
#line 662 "raku.y"
        { EXPR_t *c=make_call("raku_map");
          expr_add_child(c,(yyvsp[-1].node)); expr_add_child(c,(yyvsp[0].node)); (yyval.node)=c; }
#line 2401 "raku.tab.c"
    break;

  case 108: /* call_expr: KW_GREP closure expr  */
#line 665 "raku.y"
        { EXPR_t *c=make_call("raku_grep");
          expr_add_child(c,(yyvsp[-1].node)); expr_add_child(c,(yyvsp[0].node)); (yyval.node)=c; }
#line 2408 "raku.tab.c"
    break;

  case 109: /* call_expr: KW_SORT expr  */
#line 668 "raku.y"
        { EXPR_t *c=make_call("raku_sort");
          expr_add_child(c,(yyvsp[0].node)); (yyval.node)=c; }
#line 2415 "raku.tab.c"
    break;

  case 110: /* call_expr: KW_SORT closure expr  */
#line 671 "raku.y"
        { EXPR_t *c=make_call("raku_sort");
          expr_add_child(c,(yyvsp[-1].node)); expr_add_child(c,(yyvsp[0].node)); (yyval.node)=c; }
#line 2422 "raku.tab.c"
    break;

  case 111: /* call_expr: atom  */
#line 673 "raku.y"
                     { (yyval.node)=(yyvsp[0].node); }
#line 2428 "raku.tab.c"
    break;

  case 112: /* arg_list: expr  */
#line 677 "raku.y"
                        { (yyval.list)=exprlist_append(exprlist_new(),(yyvsp[0].node)); }
#line 2434 "raku.tab.c"
    break;

  case 113: /* arg_list: arg_list ',' expr  */
#line 678 "raku.y"
                        { (yyval.list)=exprlist_append((yyvsp[-2].list),(yyvsp[0].node)); }
#line 2440 "raku.tab.c"
    break;

  case 114: /* atom: LIT_INT  */
#line 682 "raku.y"
                      { EXPR_t *e=expr_new(E_ILIT); e->ival=(yyvsp[0].ival); (yyval.node)=e; }
#line 2446 "raku.tab.c"
    break;

  case 115: /* atom: LIT_FLOAT  */
#line 683 "raku.y"
                      { EXPR_t *e=expr_new(E_FLIT); e->dval=(yyvsp[0].dval); (yyval.node)=e; }
#line 2452 "raku.tab.c"
    break;

  case 116: /* atom: LIT_STR  */
#line 684 "raku.y"
                      { (yyval.node)=leaf_sval(E_QLIT,(yyvsp[0].sval)); }
#line 2458 "raku.tab.c"
    break;

  case 117: /* atom: LIT_INTERP_STR  */
#line 685 "raku.y"
                      { (yyval.node)=lower_interp_str((yyvsp[0].sval)); }
#line 2464 "raku.tab.c"
    break;

  case 118: /* atom: VAR_SCALAR  */
#line 686 "raku.y"
                      { (yyval.node)=var_node((yyvsp[0].sval)); }
#line 2470 "raku.tab.c"
    break;

  case 119: /* atom: VAR_ARRAY  */
#line 687 "raku.y"
                      { (yyval.node)=var_node((yyvsp[0].sval)); }
#line 2476 "raku.tab.c"
    break;

  case 120: /* atom: VAR_HASH  */
#line 688 "raku.y"
                      { (yyval.node)=var_node((yyvsp[0].sval)); }
#line 2482 "raku.tab.c"
    break;

  case 121: /* atom: VAR_CAPTURE  */
#line 690 "raku.y"
        { /* RK-34: $0/$1 positional capture */
          EXPR_t *c=make_call("raku_capture");
          EXPR_t *idx=expr_new(E_ILIT); idx->ival=(yyvsp[0].ival);
          expr_add_child(c,idx); (yyval.node)=c; }
#line 2491 "raku.tab.c"
    break;

  case 122: /* atom: VAR_NAMED_CAPTURE  */
#line 695 "raku.y"
        { /* RK-35: $<name> named capture */
          EXPR_t *c=make_call("raku_named_capture");
          expr_add_child(c,leaf_sval(E_QLIT,(yyvsp[0].sval))); (yyval.node)=c; }
#line 2499 "raku.tab.c"
    break;

  case 123: /* atom: VAR_ARRAY '[' expr ']'  */
#line 699 "raku.y"
        { EXPR_t *c=make_call("arr_get"); expr_add_child(c,var_node((yyvsp[-3].sval))); expr_add_child(c,(yyvsp[-1].node)); (yyval.node)=c; }
#line 2505 "raku.tab.c"
    break;

  case 124: /* atom: VAR_HASH '<' IDENT '>'  */
#line 701 "raku.y"
        { EXPR_t *c=make_call("hash_get"); expr_add_child(c,var_node((yyvsp[-3].sval))); expr_add_child(c,leaf_sval(E_QLIT,(yyvsp[-1].sval))); (yyval.node)=c; }
#line 2511 "raku.tab.c"
    break;

  case 125: /* atom: VAR_HASH '{' expr '}'  */
#line 703 "raku.y"
        { EXPR_t *c=make_call("hash_get"); expr_add_child(c,var_node((yyvsp[-3].sval))); expr_add_child(c,(yyvsp[-1].node)); (yyval.node)=c; }
#line 2517 "raku.tab.c"
    break;

  case 126: /* atom: KW_EXISTS VAR_HASH '<' IDENT '>'  */
#line 705 "raku.y"
        { EXPR_t *c=make_call("hash_exists"); expr_add_child(c,var_node((yyvsp[-3].sval))); expr_add_child(c,leaf_sval(E_QLIT,(yyvsp[-1].sval))); (yyval.node)=c; }
#line 2523 "raku.tab.c"
    break;

  case 127: /* atom: KW_EXISTS VAR_HASH '{' expr '}'  */
#line 707 "raku.y"
        { EXPR_t *c=make_call("hash_exists"); expr_add_child(c,var_node((yyvsp[-3].sval))); expr_add_child(c,(yyvsp[-1].node)); (yyval.node)=c; }
#line 2529 "raku.tab.c"
    break;

  case 128: /* atom: IDENT  */
#line 708 "raku.y"
                      { (yyval.node)=var_node((yyvsp[0].sval)); }
#line 2535 "raku.tab.c"
    break;

  case 129: /* atom: VAR_TWIGIL  */
#line 711 "raku.y"
        { EXPR_t *fe=expr_new(E_FIELD);
          fe->sval=(char*)intern((yyvsp[0].sval)); free((yyvsp[0].sval));
          expr_add_child(fe, leaf_sval(E_VAR, "self"));
          (yyval.node)=fe; }
#line 2544 "raku.tab.c"
    break;

  case 130: /* atom: '(' expr ')'  */
#line 715 "raku.y"
                      { (yyval.node)=(yyvsp[-1].node); }
#line 2550 "raku.tab.c"
    break;


#line 2554 "raku.tab.c"

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

#line 718 "raku.y"


/* ── Parse entry (sets up flex buffer and calls yyparse) ─────────────── */
extern void *raku_yy_scan_string(const char *);
extern void  raku_yy_delete_buffer(void *);

Program *raku_parse_string(const char *src) {
    raku_prog_result = NULL;
    void *buf = raku_yy_scan_string(src);
    raku_yyparse();
    raku_yy_delete_buffer(buf);
    return raku_prog_result;
}
