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


#line 199 "raku.tab.c"

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
  YYSYMBOL_VAR_SCALAR = 8,                 /* VAR_SCALAR  */
  YYSYMBOL_VAR_ARRAY = 9,                  /* VAR_ARRAY  */
  YYSYMBOL_VAR_HASH = 10,                  /* VAR_HASH  */
  YYSYMBOL_IDENT = 11,                     /* IDENT  */
  YYSYMBOL_KW_MY = 12,                     /* KW_MY  */
  YYSYMBOL_KW_SAY = 13,                    /* KW_SAY  */
  YYSYMBOL_KW_PRINT = 14,                  /* KW_PRINT  */
  YYSYMBOL_KW_IF = 15,                     /* KW_IF  */
  YYSYMBOL_KW_ELSE = 16,                   /* KW_ELSE  */
  YYSYMBOL_KW_ELSIF = 17,                  /* KW_ELSIF  */
  YYSYMBOL_KW_WHILE = 18,                  /* KW_WHILE  */
  YYSYMBOL_KW_FOR = 19,                    /* KW_FOR  */
  YYSYMBOL_KW_SUB = 20,                    /* KW_SUB  */
  YYSYMBOL_KW_GATHER = 21,                 /* KW_GATHER  */
  YYSYMBOL_KW_TAKE = 22,                   /* KW_TAKE  */
  YYSYMBOL_KW_RETURN = 23,                 /* KW_RETURN  */
  YYSYMBOL_KW_GIVEN = 24,                  /* KW_GIVEN  */
  YYSYMBOL_KW_WHEN = 25,                   /* KW_WHEN  */
  YYSYMBOL_KW_DEFAULT = 26,                /* KW_DEFAULT  */
  YYSYMBOL_KW_EXISTS = 27,                 /* KW_EXISTS  */
  YYSYMBOL_KW_DELETE = 28,                 /* KW_DELETE  */
  YYSYMBOL_KW_UNLESS = 29,                 /* KW_UNLESS  */
  YYSYMBOL_KW_UNTIL = 30,                  /* KW_UNTIL  */
  YYSYMBOL_KW_REPEAT = 31,                 /* KW_REPEAT  */
  YYSYMBOL_OP_RANGE = 32,                  /* OP_RANGE  */
  YYSYMBOL_OP_RANGE_EX = 33,               /* OP_RANGE_EX  */
  YYSYMBOL_OP_ARROW = 34,                  /* OP_ARROW  */
  YYSYMBOL_OP_EQ = 35,                     /* OP_EQ  */
  YYSYMBOL_OP_NE = 36,                     /* OP_NE  */
  YYSYMBOL_OP_LE = 37,                     /* OP_LE  */
  YYSYMBOL_OP_GE = 38,                     /* OP_GE  */
  YYSYMBOL_OP_SEQ = 39,                    /* OP_SEQ  */
  YYSYMBOL_OP_SNE = 40,                    /* OP_SNE  */
  YYSYMBOL_OP_AND = 41,                    /* OP_AND  */
  YYSYMBOL_OP_OR = 42,                     /* OP_OR  */
  YYSYMBOL_OP_BIND = 43,                   /* OP_BIND  */
  YYSYMBOL_OP_SMATCH = 44,                 /* OP_SMATCH  */
  YYSYMBOL_OP_DIV = 45,                    /* OP_DIV  */
  YYSYMBOL_46_ = 46,                       /* '='  */
  YYSYMBOL_47_ = 47,                       /* '!'  */
  YYSYMBOL_48_ = 48,                       /* '<'  */
  YYSYMBOL_49_ = 49,                       /* '>'  */
  YYSYMBOL_50_ = 50,                       /* '~'  */
  YYSYMBOL_51_ = 51,                       /* '+'  */
  YYSYMBOL_52_ = 52,                       /* '-'  */
  YYSYMBOL_53_ = 53,                       /* '*'  */
  YYSYMBOL_54_ = 54,                       /* '/'  */
  YYSYMBOL_55_ = 55,                       /* '%'  */
  YYSYMBOL_UMINUS = 56,                    /* UMINUS  */
  YYSYMBOL_57_ = 57,                       /* ';'  */
  YYSYMBOL_58_ = 58,                       /* '['  */
  YYSYMBOL_59_ = 59,                       /* ']'  */
  YYSYMBOL_60_ = 60,                       /* '{'  */
  YYSYMBOL_61_ = 61,                       /* '}'  */
  YYSYMBOL_62_ = 62,                       /* '('  */
  YYSYMBOL_63_ = 63,                       /* ')'  */
  YYSYMBOL_64_ = 64,                       /* ','  */
  YYSYMBOL_YYACCEPT = 65,                  /* $accept  */
  YYSYMBOL_program = 66,                   /* program  */
  YYSYMBOL_stmt_list = 67,                 /* stmt_list  */
  YYSYMBOL_stmt = 68,                      /* stmt  */
  YYSYMBOL_if_stmt = 69,                   /* if_stmt  */
  YYSYMBOL_while_stmt = 70,                /* while_stmt  */
  YYSYMBOL_unless_stmt = 71,               /* unless_stmt  */
  YYSYMBOL_until_stmt = 72,                /* until_stmt  */
  YYSYMBOL_repeat_stmt = 73,               /* repeat_stmt  */
  YYSYMBOL_for_stmt = 74,                  /* for_stmt  */
  YYSYMBOL_given_stmt = 75,                /* given_stmt  */
  YYSYMBOL_when_list = 76,                 /* when_list  */
  YYSYMBOL_sub_decl = 77,                  /* sub_decl  */
  YYSYMBOL_param_list = 78,                /* param_list  */
  YYSYMBOL_block = 79,                     /* block  */
  YYSYMBOL_expr = 80,                      /* expr  */
  YYSYMBOL_cmp_expr = 81,                  /* cmp_expr  */
  YYSYMBOL_range_expr = 82,                /* range_expr  */
  YYSYMBOL_add_expr = 83,                  /* add_expr  */
  YYSYMBOL_mul_expr = 84,                  /* mul_expr  */
  YYSYMBOL_unary_expr = 85,                /* unary_expr  */
  YYSYMBOL_postfix_expr = 86,              /* postfix_expr  */
  YYSYMBOL_call_expr = 87,                 /* call_expr  */
  YYSYMBOL_arg_list = 88,                  /* arg_list  */
  YYSYMBOL_atom = 89                       /* atom  */
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
#define YYLAST   341

/* YYNTOKENS -- Number of terminals.  */
#define YYNTOKENS  65
/* YYNNTS -- Number of nonterminals.  */
#define YYNNTS  25
/* YYNRULES -- Number of rules.  */
#define YYNRULES  102
/* YYNSTATES -- Number of states.  */
#define YYNSTATES  241

/* YYMAXUTOK -- Last valid token kind.  */
#define YYMAXUTOK   301


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
       2,     2,     2,    47,     2,     2,     2,    55,     2,     2,
      62,    63,    53,    51,    64,    52,     2,    54,     2,     2,
       2,     2,     2,     2,     2,     2,     2,     2,     2,    57,
      48,    46,    49,     2,     2,     2,     2,     2,     2,     2,
       2,     2,     2,     2,     2,     2,     2,     2,     2,     2,
       2,     2,     2,     2,     2,     2,     2,     2,     2,     2,
       2,    58,     2,    59,     2,     2,     2,     2,     2,     2,
       2,     2,     2,     2,     2,     2,     2,     2,     2,     2,
       2,     2,     2,     2,     2,     2,     2,     2,     2,     2,
       2,     2,     2,    60,     2,    61,    50,     2,     2,     2,
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
      45,    56
};

#if RAKU_YYDEBUG
/* YYRLINE[YYN] -- Source line where rule number YYN was defined.  */
static const yytype_int16 yyrline[] =
{
       0,   188,   188,   218,   219,   223,   225,   227,   230,   232,
     234,   236,   238,   240,   242,   244,   246,   248,   250,   252,
     254,   257,   260,   263,   266,   269,   270,   271,   272,   273,
     274,   275,   276,   277,   281,   283,   285,   290,   296,   298,
     304,   310,   316,   329,   335,   350,   368,   369,   378,   386,
     395,   396,   400,   404,   405,   428,   432,   433,   434,   435,
     436,   437,   438,   439,   440,   441,   442,   448,   452,   453,
     454,   458,   459,   460,   461,   465,   466,   467,   468,   469,
     473,   474,   475,   478,   481,   486,   487,   491,   492,   496,
     497,   498,   499,   500,   501,   502,   503,   505,   507,   509,
     511,   513,   514
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
  "LIT_STR", "LIT_INTERP_STR", "LIT_REGEX", "VAR_SCALAR", "VAR_ARRAY",
  "VAR_HASH", "IDENT", "KW_MY", "KW_SAY", "KW_PRINT", "KW_IF", "KW_ELSE",
  "KW_ELSIF", "KW_WHILE", "KW_FOR", "KW_SUB", "KW_GATHER", "KW_TAKE",
  "KW_RETURN", "KW_GIVEN", "KW_WHEN", "KW_DEFAULT", "KW_EXISTS",
  "KW_DELETE", "KW_UNLESS", "KW_UNTIL", "KW_REPEAT", "OP_RANGE",
  "OP_RANGE_EX", "OP_ARROW", "OP_EQ", "OP_NE", "OP_LE", "OP_GE", "OP_SEQ",
  "OP_SNE", "OP_AND", "OP_OR", "OP_BIND", "OP_SMATCH", "OP_DIV", "'='",
  "'!'", "'<'", "'>'", "'~'", "'+'", "'-'", "'*'", "'/'", "'%'", "UMINUS",
  "';'", "'['", "']'", "'{'", "'}'", "'('", "')'", "','", "$accept",
  "program", "stmt_list", "stmt", "if_stmt", "while_stmt", "unless_stmt",
  "until_stmt", "repeat_stmt", "for_stmt", "given_stmt", "when_list",
  "sub_decl", "param_list", "block", "expr", "cmp_expr", "range_expr",
  "add_expr", "mul_expr", "unary_expr", "postfix_expr", "call_expr",
  "arg_list", "atom", YY_NULLPTR
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
     -47,    23,   250,   -47,   -47,   -47,   -47,   -47,    -2,   -10,
     -39,    -5,    54,   279,   279,     9,    29,   279,    48,    40,
     279,    76,   279,    82,    91,    42,    43,    40,   131,   131,
     279,   -47,   -47,   -47,   -47,   -47,   -47,   -47,   -47,   -47,
      45,    28,   -47,   284,     0,   -47,   -47,   -47,   -47,   279,
     279,    95,   279,    31,    64,    65,    66,    21,    69,    59,
     -36,    61,    62,   279,   279,   -32,    58,   -47,   -47,    67,
     -47,    68,    70,   -35,   -28,   279,   279,   -47,   -47,   -47,
     -47,    63,   -47,   131,   131,   131,   131,   131,   131,   131,
     131,   131,   131,   114,   131,   131,   131,   131,   131,   131,
     131,   131,   131,    75,    84,    73,    83,   -47,   -47,    32,
     279,   279,   279,   -41,   -31,   -24,   279,   279,   116,   279,
     -47,   -47,    85,    86,   137,   -47,    -7,   205,   -47,   -47,
     -47,   135,   279,   136,   279,    90,    92,   -47,    16,    16,
      16,    16,    16,    16,    16,    16,    16,    16,   -47,    16,
      16,     0,     0,     0,   -47,   -47,   -47,   -47,   -47,   110,
     111,   113,   -47,   279,   104,   108,   112,   279,   -47,   279,
     -47,   279,   -47,   -47,   115,   118,   109,    40,    40,    40,
     -47,    40,    35,   -47,   -18,   122,   119,   123,   121,    40,
      40,   279,   279,   279,   -47,   -47,   -47,   -47,   120,   128,
     129,   -47,   -47,   -47,   157,   -47,   -47,   -47,    40,   167,
     279,    40,   -47,   -47,   -47,   130,   134,   160,   -47,   138,
     139,   140,   -47,   -47,   -47,    -9,   -47,   -47,    40,   133,
     -47,   -47,    40,   -47,   -47,   -47,   -47,   -47,   -47,   -47,
     -47
};

/* YYDEFACT[STATE-NUM] -- Default reduction number in state STATE-NUM.
   Performed when YYTABLE does not specify something else to do.  Zero
   means the default is an error.  */
static const yytype_int8 yydefact[] =
{
       3,     0,     2,     1,    89,    90,    91,    92,    93,    94,
      95,   101,     0,     0,     0,     0,     0,     0,     0,     0,
       0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
       0,     4,    26,    27,    30,    31,    32,    28,    29,    33,
       0,    55,    67,    70,    74,    79,    82,    83,    86,     0,
       0,     0,     0,     0,     0,     0,     0,     0,    93,    94,
      95,     0,     0,     0,     0,     0,     0,     3,    54,     0,
      18,     0,     0,     0,     0,     0,     0,    41,    93,    81,
      80,     0,    25,     0,     0,     0,     0,     0,     0,     0,
       0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
       0,     0,     0,     0,     0,     0,     0,    85,    87,     0,
       0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
      14,    15,     0,     0,     0,    43,     0,     0,    16,    17,
      46,     0,     0,     0,     0,     0,     0,   102,    56,    57,
      68,    69,    58,    59,    62,    63,    64,    65,    66,    60,
      61,    73,    71,    72,    78,    75,    76,    77,    19,    96,
      97,    98,    84,     0,     0,     0,     0,     0,    11,     0,
      12,     0,    13,    53,     0,     0,     0,     0,     0,     0,
      50,     0,     0,    52,     0,     0,     0,     0,     0,     0,
       0,     0,     0,     0,    88,     5,     6,     7,     0,     0,
       0,    96,    97,    98,    34,    37,    42,    49,     0,     0,
       0,     0,    44,    99,   100,     0,     0,    38,    40,     0,
       0,     0,     8,     9,    10,     0,    48,    51,     0,     0,
      23,    24,     0,    20,    21,    22,    36,    35,    47,    45,
      39
};

/* YYPGOTO[NTERM-NUM].  */
static const yytype_int16 yypgoto[] =
{
     -47,   -47,   125,   -47,   -46,   -47,   -47,   -47,   -47,   -47,
     -47,   -47,   -47,   -47,   -27,    -3,   -47,   -47,   154,    -8,
     -25,   -47,   -47,   -47,   -47
};

/* YYDEFGOTO[NTERM-NUM].  */
static const yytype_uint8 yydefgoto[] =
{
       0,     1,     2,    31,    32,    33,    34,    35,    36,    37,
      38,   184,    39,   182,    68,    40,    41,    42,    43,    44,
      45,    46,    47,   109,    48
};

/* YYTABLE[YYPACT[STATE-NUM]] -- What to do in state STATE-NUM.  If
   positive, shift that token.  If negative, reduce the rule whose
   number is the opposite.  If YYTABLE_NINF, syntax error.  */
static const yytype_uint8 yytable[] =
{
      77,   180,   124,    79,    80,   167,    15,   210,   211,    51,
      61,    62,   118,   131,    65,   169,   168,    69,    71,    72,
     133,    52,   171,     3,   119,   132,   170,    81,    67,   113,
     114,   115,   134,   172,     4,     5,     6,     7,   125,    58,
      59,    60,    11,   212,    49,    99,   103,   104,    50,   106,
     108,    67,    19,   100,   101,   102,   181,    53,    23,    66,
     122,   123,    54,    55,    56,    57,    96,    97,    98,    83,
      84,    63,   135,   136,   154,   155,   156,   157,    28,     4,
       5,     6,     7,    29,    58,    59,    60,    11,   151,   152,
     153,    64,    73,    30,   107,   162,   163,    19,   208,   209,
      67,    74,    82,    23,    75,    76,   105,   164,   165,   166,
     110,   111,   112,   173,   174,   116,   176,   117,   120,   121,
     126,   148,   160,    28,   128,   129,   137,   175,    29,   186,
     130,   188,   158,    70,     4,     5,     6,     7,    30,    78,
      59,    60,    11,   159,   161,   179,   185,   187,   177,   178,
     204,   205,   206,   189,   207,   190,   191,   192,    23,   193,
     194,   195,   217,   218,   198,   196,   199,   202,   200,   197,
     203,   213,   215,   225,   201,   227,   232,   222,    28,   236,
     214,   226,   216,    29,   229,   223,   224,   230,   219,   220,
     221,   231,   127,    30,   239,   233,   234,   235,   237,     0,
       0,   238,     0,     0,     0,   240,     0,   228,     4,     5,
       6,     7,     0,     8,     9,    10,    11,    12,    13,    14,
      15,     0,     0,    16,    17,    18,    19,    20,    21,    22,
       0,     0,    23,    24,    25,    26,    27,   138,   139,   140,
     141,   142,   143,   144,   145,   146,   147,     0,   149,   150,
       0,     0,    28,     4,     5,     6,     7,    29,     8,     9,
      10,    11,    12,    13,    14,    15,   183,    30,    16,    17,
      18,    19,    20,    21,    22,     0,     0,    23,    24,    25,
      26,    27,     4,     5,     6,     7,     0,    58,    59,    60,
      11,     0,     0,     0,     0,     0,     0,    28,     0,     0,
      19,     0,    29,     0,     0,     0,    23,     0,     0,     0,
       0,     0,    30,     0,     0,     0,    85,    86,     0,    87,
      88,    89,    90,    91,    92,     0,    28,     0,    93,     0,
       0,    29,    94,    95,    96,    97,    98,     0,     0,     0,
       0,    30
};

static const yytype_int16 yycheck[] =
{
      27,     8,    34,    28,    29,    46,    15,    25,    26,    48,
      13,    14,    48,    48,    17,    46,    57,    20,    21,    22,
      48,    60,    46,     0,    60,    60,    57,    30,    60,     8,
       9,    10,    60,    57,     3,     4,     5,     6,    65,     8,
       9,    10,    11,    61,    46,    45,    49,    50,    58,    52,
      53,    60,    21,    53,    54,    55,    63,    62,    27,    11,
      63,    64,     8,     9,    10,    11,    50,    51,    52,    41,
      42,    62,    75,    76,    99,   100,   101,   102,    47,     3,
       4,     5,     6,    52,     8,     9,    10,    11,    96,    97,
      98,    62,    10,    62,    63,    63,    64,    21,    63,    64,
      60,    10,    57,    27,    62,    62,    11,   110,   111,   112,
      46,    46,    46,   116,   117,    46,   119,    58,    57,    57,
      62,     7,    49,    47,    57,    57,    63,    11,    52,   132,
      60,   134,    57,    57,     3,     4,     5,     6,    62,     8,
       9,    10,    11,    59,    61,     8,    11,    11,    63,    63,
     177,   178,   179,    63,   181,    63,    46,    46,    27,    46,
     163,    57,   189,   190,   167,    57,   169,    49,   171,    57,
      61,    49,    49,    16,    59,     8,    16,    57,    47,   225,
      61,   208,    61,    52,   211,    57,    57,    57,   191,   192,
     193,    57,    67,    62,    61,    57,    57,    57,   225,    -1,
      -1,   228,    -1,    -1,    -1,   232,    -1,   210,     3,     4,
       5,     6,    -1,     8,     9,    10,    11,    12,    13,    14,
      15,    -1,    -1,    18,    19,    20,    21,    22,    23,    24,
      -1,    -1,    27,    28,    29,    30,    31,    83,    84,    85,
      86,    87,    88,    89,    90,    91,    92,    -1,    94,    95,
      -1,    -1,    47,     3,     4,     5,     6,    52,     8,     9,
      10,    11,    12,    13,    14,    15,    61,    62,    18,    19,
      20,    21,    22,    23,    24,    -1,    -1,    27,    28,    29,
      30,    31,     3,     4,     5,     6,    -1,     8,     9,    10,
      11,    -1,    -1,    -1,    -1,    -1,    -1,    47,    -1,    -1,
      21,    -1,    52,    -1,    -1,    -1,    27,    -1,    -1,    -1,
      -1,    -1,    62,    -1,    -1,    -1,    32,    33,    -1,    35,
      36,    37,    38,    39,    40,    -1,    47,    -1,    44,    -1,
      -1,    52,    48,    49,    50,    51,    52,    -1,    -1,    -1,
      -1,    62
};

/* YYSTOS[STATE-NUM] -- The symbol kind of the accessing symbol of
   state STATE-NUM.  */
static const yytype_int8 yystos[] =
{
       0,    66,    67,     0,     3,     4,     5,     6,     8,     9,
      10,    11,    12,    13,    14,    15,    18,    19,    20,    21,
      22,    23,    24,    27,    28,    29,    30,    31,    47,    52,
      62,    68,    69,    70,    71,    72,    73,    74,    75,    77,
      80,    81,    82,    83,    84,    85,    86,    87,    89,    46,
      58,    48,    60,    62,     8,     9,    10,    11,     8,     9,
      10,    80,    80,    62,    62,    80,    11,    60,    79,    80,
      57,    80,    80,    10,    10,    62,    62,    79,     8,    85,
      85,    80,    57,    41,    42,    32,    33,    35,    36,    37,
      38,    39,    40,    44,    48,    49,    50,    51,    52,    45,
      53,    54,    55,    80,    80,    11,    80,    63,    80,    88,
      46,    46,    46,     8,     9,    10,    46,    58,    48,    60,
      57,    57,    80,    80,    34,    79,    62,    67,    57,    57,
      60,    48,    60,    48,    60,    80,    80,    63,    83,    83,
      83,    83,    83,    83,    83,    83,    83,    83,     7,    83,
      83,    84,    84,    84,    85,    85,    85,    85,    57,    59,
      49,    61,    63,    64,    80,    80,    80,    46,    57,    46,
      57,    46,    57,    80,    80,    11,    80,    63,    63,     8,
       8,    63,    78,    61,    76,    11,    80,    11,    80,    63,
      63,    46,    46,    46,    80,    57,    57,    57,    80,    80,
      80,    59,    49,    61,    79,    79,    79,    79,    63,    64,
      25,    26,    61,    49,    61,    49,    61,    79,    79,    80,
      80,    80,    57,    57,    57,    16,    79,     8,    80,    79,
      57,    57,    16,    57,    57,    57,    69,    79,    79,    61,
      79
};

/* YYR1[RULE-NUM] -- Symbol kind of the left-hand side of rule RULE-NUM.  */
static const yytype_int8 yyr1[] =
{
       0,    65,    66,    67,    67,    68,    68,    68,    68,    68,
      68,    68,    68,    68,    68,    68,    68,    68,    68,    68,
      68,    68,    68,    68,    68,    68,    68,    68,    68,    68,
      68,    68,    68,    68,    69,    69,    69,    70,    71,    71,
      72,    73,    74,    74,    75,    75,    76,    76,    77,    77,
      78,    78,    79,    80,    80,    80,    81,    81,    81,    81,
      81,    81,    81,    81,    81,    81,    81,    81,    82,    82,
      82,    83,    83,    83,    83,    84,    84,    84,    84,    84,
      85,    85,    85,    86,    87,    87,    87,    88,    88,    89,
      89,    89,    89,    89,    89,    89,    89,    89,    89,    89,
      89,    89,    89
};

/* YYR2[RULE-NUM] -- Number of symbols on the right-hand side of rule RULE-NUM.  */
static const yytype_int8 yyr2[] =
{
       0,     2,     1,     0,     2,     5,     5,     5,     6,     6,
       6,     4,     4,     4,     3,     3,     3,     3,     2,     4,
       7,     7,     7,     6,     6,     2,     1,     1,     1,     1,
       1,     1,     1,     1,     5,     7,     7,     5,     5,     7,
       5,     2,     5,     3,     5,     7,     0,     4,     6,     5,
       1,     3,     3,     3,     2,     1,     3,     3,     3,     3,
       3,     3,     3,     3,     3,     3,     3,     1,     3,     3,
       1,     3,     3,     3,     1,     3,     3,     3,     3,     1,
       2,     2,     1,     1,     4,     3,     1,     1,     3,     1,
       1,     1,     1,     1,     1,     1,     4,     4,     4,     5,
       5,     1,     3
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
#line 189 "raku.y"
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
#line 1478 "raku.tab.c"
    break;

  case 3: /* stmt_list: %empty  */
#line 218 "raku.y"
                     { (yyval.list) = exprlist_new(); }
#line 1484 "raku.tab.c"
    break;

  case 4: /* stmt_list: stmt_list stmt  */
#line 219 "raku.y"
                     { (yyval.list) = exprlist_append((yyvsp[-1].list), (yyvsp[0].node)); }
#line 1490 "raku.tab.c"
    break;

  case 5: /* stmt: KW_MY VAR_SCALAR '=' expr ';'  */
#line 224 "raku.y"
        { (yyval.node) = expr_binary(E_ASSIGN, var_node((yyvsp[-3].sval)), (yyvsp[-1].node)); }
#line 1496 "raku.tab.c"
    break;

  case 6: /* stmt: KW_MY VAR_ARRAY '=' expr ';'  */
#line 226 "raku.y"
        { (yyval.node) = expr_binary(E_ASSIGN, var_node((yyvsp[-3].sval)), (yyvsp[-1].node)); }
#line 1502 "raku.tab.c"
    break;

  case 7: /* stmt: KW_MY VAR_HASH '=' expr ';'  */
#line 228 "raku.y"
        { (yyval.node) = expr_binary(E_ASSIGN, var_node((yyvsp[-3].sval)), (yyvsp[-1].node)); }
#line 1508 "raku.tab.c"
    break;

  case 8: /* stmt: KW_MY IDENT VAR_SCALAR '=' expr ';'  */
#line 231 "raku.y"
        { free((yyvsp[-4].sval)); (yyval.node) = expr_binary(E_ASSIGN, var_node((yyvsp[-3].sval)), (yyvsp[-1].node)); }
#line 1514 "raku.tab.c"
    break;

  case 9: /* stmt: KW_MY IDENT VAR_ARRAY '=' expr ';'  */
#line 233 "raku.y"
        { free((yyvsp[-4].sval)); (yyval.node) = expr_binary(E_ASSIGN, var_node((yyvsp[-3].sval)), (yyvsp[-1].node)); }
#line 1520 "raku.tab.c"
    break;

  case 10: /* stmt: KW_MY IDENT VAR_HASH '=' expr ';'  */
#line 235 "raku.y"
        { free((yyvsp[-4].sval)); (yyval.node) = expr_binary(E_ASSIGN, var_node((yyvsp[-3].sval)), (yyvsp[-1].node)); }
#line 1526 "raku.tab.c"
    break;

  case 11: /* stmt: KW_MY IDENT VAR_SCALAR ';'  */
#line 237 "raku.y"
        { free((yyvsp[-2].sval)); (yyval.node) = expr_binary(E_ASSIGN, var_node((yyvsp[-1].sval)), leaf_sval(E_QLIT, "")); }
#line 1532 "raku.tab.c"
    break;

  case 12: /* stmt: KW_MY IDENT VAR_ARRAY ';'  */
#line 239 "raku.y"
        { free((yyvsp[-2].sval)); (yyval.node) = expr_binary(E_ASSIGN, var_node((yyvsp[-1].sval)), leaf_sval(E_QLIT, "")); }
#line 1538 "raku.tab.c"
    break;

  case 13: /* stmt: KW_MY IDENT VAR_HASH ';'  */
#line 241 "raku.y"
        { free((yyvsp[-2].sval)); (yyval.node) = expr_binary(E_ASSIGN, var_node((yyvsp[-1].sval)), leaf_sval(E_QLIT, "")); }
#line 1544 "raku.tab.c"
    break;

  case 14: /* stmt: KW_SAY expr ';'  */
#line 243 "raku.y"
        { EXPR_t *c=make_call("write"); expr_add_child(c,(yyvsp[-1].node)); (yyval.node)=c; }
#line 1550 "raku.tab.c"
    break;

  case 15: /* stmt: KW_PRINT expr ';'  */
#line 245 "raku.y"
        { EXPR_t *c=make_call("writes"); expr_add_child(c,(yyvsp[-1].node)); (yyval.node)=c; }
#line 1556 "raku.tab.c"
    break;

  case 16: /* stmt: KW_TAKE expr ';'  */
#line 247 "raku.y"
        { (yyval.node)=expr_unary(E_SUSPEND,(yyvsp[-1].node)); }
#line 1562 "raku.tab.c"
    break;

  case 17: /* stmt: KW_RETURN expr ';'  */
#line 249 "raku.y"
        { EXPR_t *r=expr_new(E_RETURN); expr_add_child(r,(yyvsp[-1].node)); (yyval.node)=r; }
#line 1568 "raku.tab.c"
    break;

  case 18: /* stmt: KW_RETURN ';'  */
#line 251 "raku.y"
        { (yyval.node)=expr_new(E_RETURN); }
#line 1574 "raku.tab.c"
    break;

  case 19: /* stmt: VAR_SCALAR '=' expr ';'  */
#line 253 "raku.y"
        { (yyval.node)=expr_binary(E_ASSIGN,var_node((yyvsp[-3].sval)),(yyvsp[-1].node)); }
#line 1580 "raku.tab.c"
    break;

  case 20: /* stmt: VAR_ARRAY '[' expr ']' '=' expr ';'  */
#line 255 "raku.y"
        { EXPR_t *c=make_call("arr_set");
          expr_add_child(c,var_node((yyvsp[-6].sval))); expr_add_child(c,(yyvsp[-4].node)); expr_add_child(c,(yyvsp[-1].node)); (yyval.node)=c; }
#line 1587 "raku.tab.c"
    break;

  case 21: /* stmt: VAR_HASH '<' IDENT '>' '=' expr ';'  */
#line 258 "raku.y"
        { EXPR_t *c=make_call("hash_set");
          expr_add_child(c,var_node((yyvsp[-6].sval))); expr_add_child(c,leaf_sval(E_QLIT,(yyvsp[-4].sval))); expr_add_child(c,(yyvsp[-1].node)); (yyval.node)=c; }
#line 1594 "raku.tab.c"
    break;

  case 22: /* stmt: VAR_HASH '{' expr '}' '=' expr ';'  */
#line 261 "raku.y"
        { EXPR_t *c=make_call("hash_set");
          expr_add_child(c,var_node((yyvsp[-6].sval))); expr_add_child(c,(yyvsp[-4].node)); expr_add_child(c,(yyvsp[-1].node)); (yyval.node)=c; }
#line 1601 "raku.tab.c"
    break;

  case 23: /* stmt: KW_DELETE VAR_HASH '<' IDENT '>' ';'  */
#line 264 "raku.y"
        { EXPR_t *c=make_call("hash_delete");
          expr_add_child(c,var_node((yyvsp[-4].sval))); expr_add_child(c,leaf_sval(E_QLIT,(yyvsp[-2].sval))); (yyval.node)=c; }
#line 1608 "raku.tab.c"
    break;

  case 24: /* stmt: KW_DELETE VAR_HASH '{' expr '}' ';'  */
#line 267 "raku.y"
        { EXPR_t *c=make_call("hash_delete");
          expr_add_child(c,var_node((yyvsp[-4].sval))); expr_add_child(c,(yyvsp[-2].node)); (yyval.node)=c; }
#line 1615 "raku.tab.c"
    break;

  case 25: /* stmt: expr ';'  */
#line 269 "raku.y"
                        { (yyval.node)=(yyvsp[-1].node); }
#line 1621 "raku.tab.c"
    break;

  case 26: /* stmt: if_stmt  */
#line 270 "raku.y"
                        { (yyval.node)=(yyvsp[0].node); }
#line 1627 "raku.tab.c"
    break;

  case 27: /* stmt: while_stmt  */
#line 271 "raku.y"
                        { (yyval.node)=(yyvsp[0].node); }
#line 1633 "raku.tab.c"
    break;

  case 28: /* stmt: for_stmt  */
#line 272 "raku.y"
                        { (yyval.node)=(yyvsp[0].node); }
#line 1639 "raku.tab.c"
    break;

  case 29: /* stmt: given_stmt  */
#line 273 "raku.y"
                        { (yyval.node)=(yyvsp[0].node); }
#line 1645 "raku.tab.c"
    break;

  case 30: /* stmt: unless_stmt  */
#line 274 "raku.y"
                        { (yyval.node)=(yyvsp[0].node); }
#line 1651 "raku.tab.c"
    break;

  case 31: /* stmt: until_stmt  */
#line 275 "raku.y"
                        { (yyval.node)=(yyvsp[0].node); }
#line 1657 "raku.tab.c"
    break;

  case 32: /* stmt: repeat_stmt  */
#line 276 "raku.y"
                        { (yyval.node)=(yyvsp[0].node); }
#line 1663 "raku.tab.c"
    break;

  case 33: /* stmt: sub_decl  */
#line 277 "raku.y"
                        { (yyval.node)=(yyvsp[0].node); }
#line 1669 "raku.tab.c"
    break;

  case 34: /* if_stmt: KW_IF '(' expr ')' block  */
#line 282 "raku.y"
        { EXPR_t *e=expr_new(E_IF); expr_add_child(e,(yyvsp[-2].node)); expr_add_child(e,(yyvsp[0].node)); (yyval.node)=e; }
#line 1675 "raku.tab.c"
    break;

  case 35: /* if_stmt: KW_IF '(' expr ')' block KW_ELSE block  */
#line 284 "raku.y"
        { EXPR_t *e=expr_new(E_IF); expr_add_child(e,(yyvsp[-4].node)); expr_add_child(e,(yyvsp[-2].node)); expr_add_child(e,(yyvsp[0].node)); (yyval.node)=e; }
#line 1681 "raku.tab.c"
    break;

  case 36: /* if_stmt: KW_IF '(' expr ')' block KW_ELSE if_stmt  */
#line 286 "raku.y"
        { EXPR_t *e=expr_new(E_IF); expr_add_child(e,(yyvsp[-4].node)); expr_add_child(e,(yyvsp[-2].node)); expr_add_child(e,(yyvsp[0].node)); (yyval.node)=e; }
#line 1687 "raku.tab.c"
    break;

  case 37: /* while_stmt: KW_WHILE '(' expr ')' block  */
#line 291 "raku.y"
        { (yyval.node)=expr_binary(E_WHILE,(yyvsp[-2].node),(yyvsp[0].node)); }
#line 1693 "raku.tab.c"
    break;

  case 38: /* unless_stmt: KW_UNLESS '(' expr ')' block  */
#line 297 "raku.y"
        { EXPR_t *e=expr_new(E_IF); expr_add_child(e,expr_unary(E_NOT,(yyvsp[-2].node))); expr_add_child(e,(yyvsp[0].node)); (yyval.node)=e; }
#line 1699 "raku.tab.c"
    break;

  case 39: /* unless_stmt: KW_UNLESS '(' expr ')' block KW_ELSE block  */
#line 299 "raku.y"
        { EXPR_t *e=expr_new(E_IF); expr_add_child(e,expr_unary(E_NOT,(yyvsp[-4].node))); expr_add_child(e,(yyvsp[-2].node)); expr_add_child(e,(yyvsp[0].node)); (yyval.node)=e; }
#line 1705 "raku.tab.c"
    break;

  case 40: /* until_stmt: KW_UNTIL '(' expr ')' block  */
#line 305 "raku.y"
        { EXPR_t *e=expr_new(E_UNTIL); expr_add_child(e,(yyvsp[-2].node)); expr_add_child(e,(yyvsp[0].node)); (yyval.node)=e; }
#line 1711 "raku.tab.c"
    break;

  case 41: /* repeat_stmt: KW_REPEAT block  */
#line 311 "raku.y"
        { EXPR_t *e=expr_new(E_REPEAT); expr_add_child(e,(yyvsp[0].node)); (yyval.node)=e; }
#line 1717 "raku.tab.c"
    break;

  case 42: /* for_stmt: KW_FOR expr OP_ARROW VAR_SCALAR block  */
#line 317 "raku.y"
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
#line 1734 "raku.tab.c"
    break;

  case 43: /* for_stmt: KW_FOR expr block  */
#line 330 "raku.y"
        { EXPR_t *gen=((yyvsp[-1].node)->kind==E_VAR)?expr_unary(E_ITERATE,(yyvsp[-1].node)):(yyvsp[-1].node);
          (yyval.node)=expr_binary(E_EVERY,gen,(yyvsp[0].node)); }
#line 1741 "raku.tab.c"
    break;

  case 44: /* given_stmt: KW_GIVEN expr '{' when_list '}'  */
#line 336 "raku.y"
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
#line 1760 "raku.tab.c"
    break;

  case 45: /* given_stmt: KW_GIVEN expr '{' when_list KW_DEFAULT block '}'  */
#line 351 "raku.y"
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
#line 1779 "raku.tab.c"
    break;

  case 46: /* when_list: %empty  */
#line 368 "raku.y"
                   { (yyval.list)=exprlist_new(); }
#line 1785 "raku.tab.c"
    break;

  case 47: /* when_list: when_list KW_WHEN expr block  */
#line 370 "raku.y"
        { EKind cmp=((yyvsp[-1].node)->kind==E_QLIT)?E_LEQ:E_EQ;
          EXPR_t *pair=expr_new(E_SEQ_EXPR);
          pair->ival=(long)cmp;
          expr_add_child(pair,(yyvsp[-1].node)); expr_add_child(pair,(yyvsp[0].node));
          (yyval.list)=exprlist_append((yyvsp[-3].list),pair); }
#line 1795 "raku.tab.c"
    break;

  case 48: /* sub_decl: KW_SUB IDENT '(' param_list ')' block  */
#line 379 "raku.y"
        { ExprList *params=(yyvsp[-2].list); int np=params?params->count:0;
          EXPR_t *e=leaf_sval(E_FNC,(yyvsp[-4].sval)); e->ival=(long)np|SUB_TAG;
          EXPR_t *nn=expr_new(E_VAR); nn->sval=intern((yyvsp[-4].sval)); expr_add_child(e,nn);
          if(params){ for(int i=0;i<np;i++) expr_add_child(e,params->items[i]); exprlist_free(params); }
          EXPR_t *body=(yyvsp[0].node);
          for(int i=0;i<body->nchildren;i++) expr_add_child(e,body->children[i]);
          (yyval.node)=e; }
#line 1807 "raku.tab.c"
    break;

  case 49: /* sub_decl: KW_SUB IDENT '(' ')' block  */
#line 387 "raku.y"
        { EXPR_t *e=leaf_sval(E_FNC,(yyvsp[-3].sval)); e->ival=(long)0|SUB_TAG;
          EXPR_t *nn=expr_new(E_VAR); nn->sval=intern((yyvsp[-3].sval)); expr_add_child(e,nn);
          EXPR_t *body=(yyvsp[0].node);
          for(int i=0;i<body->nchildren;i++) expr_add_child(e,body->children[i]);
          (yyval.node)=e; }
#line 1817 "raku.tab.c"
    break;

  case 50: /* param_list: VAR_SCALAR  */
#line 395 "raku.y"
                             { (yyval.list)=exprlist_append(exprlist_new(),var_node((yyvsp[0].sval))); }
#line 1823 "raku.tab.c"
    break;

  case 51: /* param_list: param_list ',' VAR_SCALAR  */
#line 396 "raku.y"
                                { (yyval.list)=exprlist_append((yyvsp[-2].list),var_node((yyvsp[0].sval))); }
#line 1829 "raku.tab.c"
    break;

  case 52: /* block: '{' stmt_list '}'  */
#line 400 "raku.y"
                         { (yyval.node)=make_seq((yyvsp[-1].list)); }
#line 1835 "raku.tab.c"
    break;

  case 53: /* expr: VAR_SCALAR '=' expr  */
#line 404 "raku.y"
                           { (yyval.node)=expr_binary(E_ASSIGN,var_node((yyvsp[-2].sval)),(yyvsp[0].node)); }
#line 1841 "raku.tab.c"
    break;

  case 54: /* expr: KW_GATHER block  */
#line 405 "raku.y"
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
#line 1869 "raku.tab.c"
    break;

  case 55: /* expr: cmp_expr  */
#line 428 "raku.y"
                           { (yyval.node)=(yyvsp[0].node); }
#line 1875 "raku.tab.c"
    break;

  case 56: /* cmp_expr: cmp_expr OP_AND add_expr  */
#line 432 "raku.y"
                                { (yyval.node)=expr_binary(E_SEQ,(yyvsp[-2].node),(yyvsp[0].node)); }
#line 1881 "raku.tab.c"
    break;

  case 57: /* cmp_expr: cmp_expr OP_OR add_expr  */
#line 433 "raku.y"
                                { (yyval.node)=expr_binary(E_ALT,(yyvsp[-2].node),(yyvsp[0].node)); }
#line 1887 "raku.tab.c"
    break;

  case 58: /* cmp_expr: add_expr OP_EQ add_expr  */
#line 434 "raku.y"
                                { (yyval.node)=expr_binary(E_EQ,(yyvsp[-2].node),(yyvsp[0].node)); }
#line 1893 "raku.tab.c"
    break;

  case 59: /* cmp_expr: add_expr OP_NE add_expr  */
#line 435 "raku.y"
                                { (yyval.node)=expr_binary(E_NE,(yyvsp[-2].node),(yyvsp[0].node)); }
#line 1899 "raku.tab.c"
    break;

  case 60: /* cmp_expr: add_expr '<' add_expr  */
#line 436 "raku.y"
                                { (yyval.node)=expr_binary(E_LT,(yyvsp[-2].node),(yyvsp[0].node)); }
#line 1905 "raku.tab.c"
    break;

  case 61: /* cmp_expr: add_expr '>' add_expr  */
#line 437 "raku.y"
                                { (yyval.node)=expr_binary(E_GT,(yyvsp[-2].node),(yyvsp[0].node)); }
#line 1911 "raku.tab.c"
    break;

  case 62: /* cmp_expr: add_expr OP_LE add_expr  */
#line 438 "raku.y"
                                { (yyval.node)=expr_binary(E_LE,(yyvsp[-2].node),(yyvsp[0].node)); }
#line 1917 "raku.tab.c"
    break;

  case 63: /* cmp_expr: add_expr OP_GE add_expr  */
#line 439 "raku.y"
                                { (yyval.node)=expr_binary(E_GE,(yyvsp[-2].node),(yyvsp[0].node)); }
#line 1923 "raku.tab.c"
    break;

  case 64: /* cmp_expr: add_expr OP_SEQ add_expr  */
#line 440 "raku.y"
                                { (yyval.node)=expr_binary(E_LEQ,(yyvsp[-2].node),(yyvsp[0].node)); }
#line 1929 "raku.tab.c"
    break;

  case 65: /* cmp_expr: add_expr OP_SNE add_expr  */
#line 441 "raku.y"
                                { (yyval.node)=expr_binary(E_LNE,(yyvsp[-2].node),(yyvsp[0].node)); }
#line 1935 "raku.tab.c"
    break;

  case 66: /* cmp_expr: add_expr OP_SMATCH LIT_REGEX  */
#line 443 "raku.y"
        { /* RK-23: $s ~~ /pattern/ — emit make_call("raku_match", subj, pat_str) */
          EXPR_t *c = make_call("raku_match");
          expr_add_child(c, (yyvsp[-2].node));
          expr_add_child(c, leaf_sval(E_QLIT, (yyvsp[0].sval)));
          (yyval.node) = c; }
#line 1945 "raku.tab.c"
    break;

  case 67: /* cmp_expr: range_expr  */
#line 448 "raku.y"
                               { (yyval.node)=(yyvsp[0].node); }
#line 1951 "raku.tab.c"
    break;

  case 68: /* range_expr: add_expr OP_RANGE add_expr  */
#line 452 "raku.y"
                                    { (yyval.node)=expr_binary(E_TO,(yyvsp[-2].node),(yyvsp[0].node)); }
#line 1957 "raku.tab.c"
    break;

  case 69: /* range_expr: add_expr OP_RANGE_EX add_expr  */
#line 453 "raku.y"
                                    { (yyval.node)=expr_binary(E_TO,(yyvsp[-2].node),(yyvsp[0].node)); }
#line 1963 "raku.tab.c"
    break;

  case 70: /* range_expr: add_expr  */
#line 454 "raku.y"
                                    { (yyval.node)=(yyvsp[0].node); }
#line 1969 "raku.tab.c"
    break;

  case 71: /* add_expr: add_expr '+' mul_expr  */
#line 458 "raku.y"
                             { (yyval.node)=expr_binary(E_ADD,(yyvsp[-2].node),(yyvsp[0].node)); }
#line 1975 "raku.tab.c"
    break;

  case 72: /* add_expr: add_expr '-' mul_expr  */
#line 459 "raku.y"
                             { (yyval.node)=expr_binary(E_SUB,(yyvsp[-2].node),(yyvsp[0].node)); }
#line 1981 "raku.tab.c"
    break;

  case 73: /* add_expr: add_expr '~' mul_expr  */
#line 460 "raku.y"
                             { (yyval.node)=expr_binary(E_CAT,(yyvsp[-2].node),(yyvsp[0].node)); }
#line 1987 "raku.tab.c"
    break;

  case 74: /* add_expr: mul_expr  */
#line 461 "raku.y"
                             { (yyval.node)=(yyvsp[0].node); }
#line 1993 "raku.tab.c"
    break;

  case 75: /* mul_expr: mul_expr '*' unary_expr  */
#line 465 "raku.y"
                                  { (yyval.node)=expr_binary(E_MUL,(yyvsp[-2].node),(yyvsp[0].node)); }
#line 1999 "raku.tab.c"
    break;

  case 76: /* mul_expr: mul_expr '/' unary_expr  */
#line 466 "raku.y"
                                  { (yyval.node)=expr_binary(E_DIV,(yyvsp[-2].node),(yyvsp[0].node)); }
#line 2005 "raku.tab.c"
    break;

  case 77: /* mul_expr: mul_expr '%' unary_expr  */
#line 467 "raku.y"
                                  { (yyval.node)=expr_binary(E_MOD,(yyvsp[-2].node),(yyvsp[0].node)); }
#line 2011 "raku.tab.c"
    break;

  case 78: /* mul_expr: mul_expr OP_DIV unary_expr  */
#line 468 "raku.y"
                                  { (yyval.node)=expr_binary(E_DIV,(yyvsp[-2].node),(yyvsp[0].node)); }
#line 2017 "raku.tab.c"
    break;

  case 79: /* mul_expr: unary_expr  */
#line 469 "raku.y"
                                  { (yyval.node)=(yyvsp[0].node); }
#line 2023 "raku.tab.c"
    break;

  case 80: /* unary_expr: '-' unary_expr  */
#line 473 "raku.y"
                                   { (yyval.node)=expr_unary(E_MNS,(yyvsp[0].node)); }
#line 2029 "raku.tab.c"
    break;

  case 81: /* unary_expr: '!' unary_expr  */
#line 474 "raku.y"
                                   { (yyval.node)=expr_unary(E_NOT,(yyvsp[0].node)); }
#line 2035 "raku.tab.c"
    break;

  case 82: /* unary_expr: postfix_expr  */
#line 475 "raku.y"
                                   { (yyval.node)=(yyvsp[0].node); }
#line 2041 "raku.tab.c"
    break;

  case 83: /* postfix_expr: call_expr  */
#line 478 "raku.y"
                         { (yyval.node)=(yyvsp[0].node); }
#line 2047 "raku.tab.c"
    break;

  case 84: /* call_expr: IDENT '(' arg_list ')'  */
#line 482 "raku.y"
        { EXPR_t *e=make_call((yyvsp[-3].sval));
          ExprList *args=(yyvsp[-1].list);
          if(args){ for(int i=0;i<args->count;i++) expr_add_child(e,args->items[i]); exprlist_free(args); }
          (yyval.node)=e; }
#line 2056 "raku.tab.c"
    break;

  case 85: /* call_expr: IDENT '(' ')'  */
#line 486 "raku.y"
                     { (yyval.node)=make_call((yyvsp[-2].sval)); }
#line 2062 "raku.tab.c"
    break;

  case 86: /* call_expr: atom  */
#line 487 "raku.y"
                     { (yyval.node)=(yyvsp[0].node); }
#line 2068 "raku.tab.c"
    break;

  case 87: /* arg_list: expr  */
#line 491 "raku.y"
                        { (yyval.list)=exprlist_append(exprlist_new(),(yyvsp[0].node)); }
#line 2074 "raku.tab.c"
    break;

  case 88: /* arg_list: arg_list ',' expr  */
#line 492 "raku.y"
                        { (yyval.list)=exprlist_append((yyvsp[-2].list),(yyvsp[0].node)); }
#line 2080 "raku.tab.c"
    break;

  case 89: /* atom: LIT_INT  */
#line 496 "raku.y"
                      { EXPR_t *e=expr_new(E_ILIT); e->ival=(yyvsp[0].ival); (yyval.node)=e; }
#line 2086 "raku.tab.c"
    break;

  case 90: /* atom: LIT_FLOAT  */
#line 497 "raku.y"
                      { EXPR_t *e=expr_new(E_FLIT); e->dval=(yyvsp[0].dval); (yyval.node)=e; }
#line 2092 "raku.tab.c"
    break;

  case 91: /* atom: LIT_STR  */
#line 498 "raku.y"
                      { (yyval.node)=leaf_sval(E_QLIT,(yyvsp[0].sval)); }
#line 2098 "raku.tab.c"
    break;

  case 92: /* atom: LIT_INTERP_STR  */
#line 499 "raku.y"
                      { (yyval.node)=lower_interp_str((yyvsp[0].sval)); }
#line 2104 "raku.tab.c"
    break;

  case 93: /* atom: VAR_SCALAR  */
#line 500 "raku.y"
                      { (yyval.node)=var_node((yyvsp[0].sval)); }
#line 2110 "raku.tab.c"
    break;

  case 94: /* atom: VAR_ARRAY  */
#line 501 "raku.y"
                      { (yyval.node)=var_node((yyvsp[0].sval)); }
#line 2116 "raku.tab.c"
    break;

  case 95: /* atom: VAR_HASH  */
#line 502 "raku.y"
                      { (yyval.node)=var_node((yyvsp[0].sval)); }
#line 2122 "raku.tab.c"
    break;

  case 96: /* atom: VAR_ARRAY '[' expr ']'  */
#line 504 "raku.y"
        { EXPR_t *c=make_call("arr_get"); expr_add_child(c,var_node((yyvsp[-3].sval))); expr_add_child(c,(yyvsp[-1].node)); (yyval.node)=c; }
#line 2128 "raku.tab.c"
    break;

  case 97: /* atom: VAR_HASH '<' IDENT '>'  */
#line 506 "raku.y"
        { EXPR_t *c=make_call("hash_get"); expr_add_child(c,var_node((yyvsp[-3].sval))); expr_add_child(c,leaf_sval(E_QLIT,(yyvsp[-1].sval))); (yyval.node)=c; }
#line 2134 "raku.tab.c"
    break;

  case 98: /* atom: VAR_HASH '{' expr '}'  */
#line 508 "raku.y"
        { EXPR_t *c=make_call("hash_get"); expr_add_child(c,var_node((yyvsp[-3].sval))); expr_add_child(c,(yyvsp[-1].node)); (yyval.node)=c; }
#line 2140 "raku.tab.c"
    break;

  case 99: /* atom: KW_EXISTS VAR_HASH '<' IDENT '>'  */
#line 510 "raku.y"
        { EXPR_t *c=make_call("hash_exists"); expr_add_child(c,var_node((yyvsp[-3].sval))); expr_add_child(c,leaf_sval(E_QLIT,(yyvsp[-1].sval))); (yyval.node)=c; }
#line 2146 "raku.tab.c"
    break;

  case 100: /* atom: KW_EXISTS VAR_HASH '{' expr '}'  */
#line 512 "raku.y"
        { EXPR_t *c=make_call("hash_exists"); expr_add_child(c,var_node((yyvsp[-3].sval))); expr_add_child(c,(yyvsp[-1].node)); (yyval.node)=c; }
#line 2152 "raku.tab.c"
    break;

  case 101: /* atom: IDENT  */
#line 513 "raku.y"
                      { (yyval.node)=var_node((yyvsp[0].sval)); }
#line 2158 "raku.tab.c"
    break;

  case 102: /* atom: '(' expr ')'  */
#line 514 "raku.y"
                      { (yyval.node)=(yyvsp[-1].node); }
#line 2164 "raku.tab.c"
    break;


#line 2168 "raku.tab.c"

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

#line 517 "raku.y"


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
