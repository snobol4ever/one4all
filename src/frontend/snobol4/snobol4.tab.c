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

/* Substitute the type names.  */
#define YYSTYPE         SNOBOL4_STYPE
/* Substitute the variable and function names.  */
#define yyparse         snobol4_parse
#define yylex           snobol4_lex
#define yyerror         snobol4_error
#define yydebug         snobol4_debug
#define yynerrs         snobol4_nerrs


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

#include "snobol4.tab.h"
/* Symbol kind.  */
enum yysymbol_kind_t
{
  YYSYMBOL_YYEMPTY = -2,
  YYSYMBOL_YYEOF = 0,                      /* "end of file"  */
  YYSYMBOL_YYerror = 1,                    /* error  */
  YYSYMBOL_YYUNDEF = 2,                    /* "invalid token"  */
  YYSYMBOL_T_IDENT = 3,                    /* T_IDENT  */
  YYSYMBOL_T_FUNCTION = 4,                 /* T_FUNCTION  */
  YYSYMBOL_T_KEYWORD = 5,                  /* T_KEYWORD  */
  YYSYMBOL_T_END = 6,                      /* T_END  */
  YYSYMBOL_T_INT = 7,                      /* T_INT  */
  YYSYMBOL_T_REAL = 8,                     /* T_REAL  */
  YYSYMBOL_T_STR = 9,                      /* T_STR  */
  YYSYMBOL_T_LABEL = 10,                   /* T_LABEL  */
  YYSYMBOL_T_GOTO_S = 11,                  /* T_GOTO_S  */
  YYSYMBOL_T_GOTO_F = 12,                  /* T_GOTO_F  */
  YYSYMBOL_T_GOTO_LPAREN = 13,             /* T_GOTO_LPAREN  */
  YYSYMBOL_T_GOTO_RPAREN = 14,             /* T_GOTO_RPAREN  */
  YYSYMBOL_T_STMT_END = 15,                /* T_STMT_END  */
  YYSYMBOL_T_2EQUAL = 16,                  /* T_2EQUAL  */
  YYSYMBOL_T_2QUEST = 17,                  /* T_2QUEST  */
  YYSYMBOL_T_2PIPE = 18,                   /* T_2PIPE  */
  YYSYMBOL_T_2PLUS = 19,                   /* T_2PLUS  */
  YYSYMBOL_T_2MINUS = 20,                  /* T_2MINUS  */
  YYSYMBOL_T_2STAR = 21,                   /* T_2STAR  */
  YYSYMBOL_T_2SLASH = 22,                  /* T_2SLASH  */
  YYSYMBOL_T_2CARET = 23,                  /* T_2CARET  */
  YYSYMBOL_T_2DOLLAR = 24,                 /* T_2DOLLAR  */
  YYSYMBOL_T_2DOT = 25,                    /* T_2DOT  */
  YYSYMBOL_T_2AMP = 26,                    /* T_2AMP  */
  YYSYMBOL_T_2AT = 27,                     /* T_2AT  */
  YYSYMBOL_T_2POUND = 28,                  /* T_2POUND  */
  YYSYMBOL_T_2PERCENT = 29,                /* T_2PERCENT  */
  YYSYMBOL_T_2TILDE = 30,                  /* T_2TILDE  */
  YYSYMBOL_T_1AT = 31,                     /* T_1AT  */
  YYSYMBOL_T_1TILDE = 32,                  /* T_1TILDE  */
  YYSYMBOL_T_1QUEST = 33,                  /* T_1QUEST  */
  YYSYMBOL_T_1AMP = 34,                    /* T_1AMP  */
  YYSYMBOL_T_1PLUS = 35,                   /* T_1PLUS  */
  YYSYMBOL_T_1MINUS = 36,                  /* T_1MINUS  */
  YYSYMBOL_T_1STAR = 37,                   /* T_1STAR  */
  YYSYMBOL_T_1DOLLAR = 38,                 /* T_1DOLLAR  */
  YYSYMBOL_T_1DOT = 39,                    /* T_1DOT  */
  YYSYMBOL_T_1BANG = 40,                   /* T_1BANG  */
  YYSYMBOL_T_1PERCENT = 41,                /* T_1PERCENT  */
  YYSYMBOL_T_1SLASH = 42,                  /* T_1SLASH  */
  YYSYMBOL_T_1POUND = 43,                  /* T_1POUND  */
  YYSYMBOL_T_1EQUAL = 44,                  /* T_1EQUAL  */
  YYSYMBOL_T_1PIPE = 45,                   /* T_1PIPE  */
  YYSYMBOL_T_CONCAT = 46,                  /* T_CONCAT  */
  YYSYMBOL_T_COMMA = 47,                   /* T_COMMA  */
  YYSYMBOL_T_LPAREN = 48,                  /* T_LPAREN  */
  YYSYMBOL_T_RPAREN = 49,                  /* T_RPAREN  */
  YYSYMBOL_T_LBRACK = 50,                  /* T_LBRACK  */
  YYSYMBOL_T_RBRACK = 51,                  /* T_RBRACK  */
  YYSYMBOL_T_LANGLE = 52,                  /* T_LANGLE  */
  YYSYMBOL_T_RANGLE = 53,                  /* T_RANGLE  */
  YYSYMBOL_YYACCEPT = 54,                  /* $accept  */
  YYSYMBOL_top = 55,                       /* top  */
  YYSYMBOL_program = 56,                   /* program  */
  YYSYMBOL_stmt = 57,                      /* stmt  */
  YYSYMBOL_unlabeled_stmt = 58,            /* unlabeled_stmt  */
  YYSYMBOL_opt_subject = 59,               /* opt_subject  */
  YYSYMBOL_opt_pattern = 60,               /* opt_pattern  */
  YYSYMBOL_opt_repl = 61,                  /* opt_repl  */
  YYSYMBOL_goto_atom = 62,                 /* goto_atom  */
  YYSYMBOL_goto_expr = 63,                 /* goto_expr  */
  YYSYMBOL_goto_label_expr = 64,           /* goto_label_expr  */
  YYSYMBOL_expr0 = 65,                     /* expr0  */
  YYSYMBOL_expr2 = 66,                     /* expr2  */
  YYSYMBOL_expr3 = 67,                     /* expr3  */
  YYSYMBOL_expr4 = 68,                     /* expr4  */
  YYSYMBOL_expr5 = 69,                     /* expr5  */
  YYSYMBOL_expr6 = 70,                     /* expr6  */
  YYSYMBOL_expr7 = 71,                     /* expr7  */
  YYSYMBOL_expr8 = 72,                     /* expr8  */
  YYSYMBOL_expr9 = 73,                     /* expr9  */
  YYSYMBOL_expr10 = 74,                    /* expr10  */
  YYSYMBOL_expr11 = 75,                    /* expr11  */
  YYSYMBOL_expr12 = 76,                    /* expr12  */
  YYSYMBOL_expr13 = 77,                    /* expr13  */
  YYSYMBOL_expr14 = 78,                    /* expr14  */
  YYSYMBOL_expr15 = 79,                    /* expr15  */
  YYSYMBOL_exprlist = 80,                  /* exprlist  */
  YYSYMBOL_exprlist_ne = 81,               /* exprlist_ne  */
  YYSYMBOL_expr17 = 82                     /* expr17  */
};
typedef enum yysymbol_kind_t yysymbol_kind_t;



/* Unqualified %code blocks.  */
#line 5 "snobol4.y"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
typedef struct { CODE_t *prog; tree_t **result; tree_t *ast_prog; } PP;
/*----------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------*/
static void     sno4_stmt_commit_go(void*,Token,tree_t*,tree_t*,int,tree_t*,tree_t*,tree_t*,tree_t*);
static Lex     *g_lx;
/*----------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------*/
static void     fixup_val(tree_t*);
/*----------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------*/
static int      is_pat(tree_t*);
/*----------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------*/
static tree_t  *parse_expr(Lex*);
/*----------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------*/
static tree_e pat_prim_kind(const char *s) {
    if (!s) return TT_VAR;
    static const struct { const char *n; tree_e k; } m[] = {
        {"ANY",TT_ANY},{"NOTANY",TT_NOTANY},{"SPAN",TT_SPAN},{"BREAK",TT_BREAK},{"BREAKX",TT_BREAKX},
        {"LEN",TT_LEN},{"POS",TT_POS},{"RPOS",TT_RPOS},{"TAB",TT_TAB},{"RTAB",TT_RTAB},
        {"ARB",TT_ARB},{"ARBNO",TT_ARBNO},{"REM",TT_REM},{"FAIL",TT_FAIL},{"SUCCEED",TT_SUCCEED},
        {"FENCE",TT_FENCE},{"ABORT",TT_ABORT},{"BAL",TT_BAL},{NULL,TT_VAR}
    };
    for (int i = 0; m[i].n; i++) if (strcmp(s, m[i].n) == 0) return m[i].k;
    return TT_VAR;
}

#line 221 "snobol4.tab.c"

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
         || (defined SNOBOL4_STYPE_IS_TRIVIAL && SNOBOL4_STYPE_IS_TRIVIAL)))

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
#define YYFINAL  67
/* YYLAST -- Last index in YYTABLE.  */
#define YYLAST   269

/* YYNTOKENS -- Number of terminals.  */
#define YYNTOKENS  54
/* YYNNTS -- Number of nonterminals.  */
#define YYNNTS  29
/* YYNRULES -- Number of rules.  */
#define YYNRULES  112
/* YYNSTATES -- Number of states.  */
#define YYNSTATES  211

/* YYMAXUTOK -- Last valid token kind.  */
#define YYMAXUTOK   308


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
      45,    46,    47,    48,    49,    50,    51,    52,    53
};

#if SNOBOL4_DEBUG
/* YYRLINE[YYN] -- Source line where rule number YYN was defined.  */
static const yytype_uint8 yyrline[] =
{
       0,    54,    54,    55,    57,    57,    59,    60,    61,    62,
      63,    64,    65,    66,    67,    68,    69,    70,    71,    74,
      75,    76,    77,    78,    79,    80,    81,    82,    83,    84,
      85,    87,    88,    90,    91,    93,    94,    95,    97,    98,
      99,   100,   102,   103,   106,   107,   108,   109,   110,   111,
     113,   114,   115,   117,   118,   120,   121,   123,   124,   126,
     127,   129,   130,   131,   133,   134,   136,   137,   139,   140,
     142,   143,   145,   146,   148,   149,   150,   152,   153,   155,
     156,   157,   158,   159,   160,   161,   162,   163,   164,   165,
     166,   167,   168,   169,   170,   172,   173,   174,   176,   177,
     179,   180,   181,   183,   184,   185,   186,   187,   188,   189,
     190,   191,   192
};
#endif

/** Accessing symbol of state STATE.  */
#define YY_ACCESSING_SYMBOL(State) YY_CAST (yysymbol_kind_t, yystos[State])

#if SNOBOL4_DEBUG || 0
/* The user-facing name of the symbol whose (internal) number is
   YYSYMBOL.  No bounds checking.  */
static const char *yysymbol_name (yysymbol_kind_t yysymbol) YY_ATTRIBUTE_UNUSED;

/* YYTNAME[SYMBOL-NUM] -- String name of the symbol SYMBOL-NUM.
   First, the terminals, then, starting at YYNTOKENS, nonterminals.  */
static const char *const yytname[] =
{
  "\"end of file\"", "error", "\"invalid token\"", "T_IDENT",
  "T_FUNCTION", "T_KEYWORD", "T_END", "T_INT", "T_REAL", "T_STR",
  "T_LABEL", "T_GOTO_S", "T_GOTO_F", "T_GOTO_LPAREN", "T_GOTO_RPAREN",
  "T_STMT_END", "T_2EQUAL", "T_2QUEST", "T_2PIPE", "T_2PLUS", "T_2MINUS",
  "T_2STAR", "T_2SLASH", "T_2CARET", "T_2DOLLAR", "T_2DOT", "T_2AMP",
  "T_2AT", "T_2POUND", "T_2PERCENT", "T_2TILDE", "T_1AT", "T_1TILDE",
  "T_1QUEST", "T_1AMP", "T_1PLUS", "T_1MINUS", "T_1STAR", "T_1DOLLAR",
  "T_1DOT", "T_1BANG", "T_1PERCENT", "T_1SLASH", "T_1POUND", "T_1EQUAL",
  "T_1PIPE", "T_CONCAT", "T_COMMA", "T_LPAREN", "T_RPAREN", "T_LBRACK",
  "T_RBRACK", "T_LANGLE", "T_RANGLE", "$accept", "top", "program", "stmt",
  "unlabeled_stmt", "opt_subject", "opt_pattern", "opt_repl", "goto_atom",
  "goto_expr", "goto_label_expr", "expr0", "expr2", "expr3", "expr4",
  "expr5", "expr6", "expr7", "expr8", "expr9", "expr10", "expr11",
  "expr12", "expr13", "expr14", "expr15", "exprlist", "exprlist_ne",
  "expr17", YY_NULLPTR
};

static const char *
yysymbol_name (yysymbol_kind_t yysymbol)
{
  return yytname[yysymbol];
}
#endif

#define YYPACT_NINF (-92)

#define yypact_value_is_default(Yyn) \
  ((Yyn) == YYPACT_NINF)

#define YYTABLE_NINF (-55)

#define yytable_value_is_error(Yyn) \
  0

/* YYPACT[STATE-NUM] -- Index in YYTABLE of the portion describing
   STATE-NUM.  */
static const yytype_int16 yypact[] =
{
      82,   -92,   -47,   -92,   -92,   -92,   -92,   -92,   193,   193,
     193,   193,   193,   193,   193,   193,   193,   193,   193,   193,
     193,   193,   193,   193,   146,    18,   128,   -92,   -92,     8,
       3,    10,   -12,    30,    59,    35,    17,    52,    55,   -92,
     133,   -92,    63,    19,   -92,   193,     8,     5,   -92,   -92,
     -92,   -92,   -92,   -92,   -92,   -92,   -92,   -92,   -92,   -92,
     -92,   -92,   -92,   -92,    50,    -3,    87,   -92,   -92,   193,
      40,   193,   193,   193,   193,   193,   193,   193,   193,   193,
     193,   193,   193,   193,   193,   193,   193,   193,   -92,    80,
     101,    49,   193,   193,   -92,   193,   193,   -92,   126,   126,
       0,   -92,   206,     8,    87,    87,   -12,    30,    59,    35,
      35,    17,    52,    55,   -92,   -92,   -92,   -92,   -92,   171,
     170,   -92,   193,   126,   126,   -92,   224,     8,    98,   -92,
     -92,    53,     1,   226,   228,   229,    12,   -92,    83,   -92,
     -92,   -92,    92,    15,   -92,   129,   -92,   126,   -92,   126,
     -92,   -92,   -92,   -92,   230,   231,    37,   126,   126,   -92,
     232,   126,   -92,   126,   -92,   126,   126,   -92,   233,   234,
     235,   -92,   -92,   -92,   -92,   -92,   -92,   -92,    -4,    97,
      33,   -92,   236,   237,   131,    34,   -92,   -92,   -92,   239,
      37,   126,   -92,   126,   -92,   -92,   -92,   126,   -92,   126,
     -92,   -92,   -92,   240,   241,   242,   243,   -92,   -92,   -92,
     -92
};

/* YYDEFACT[STATE-NUM] -- Default reduction number in state STATE-NUM.
   Performed when YYTABLE does not specify something else to do.  Zero
   means the default is an error.  */
static const yytype_int8 yydefact[] =
{
      32,   107,     0,   109,   108,   111,   112,   110,    32,     0,
       0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
       0,     0,     0,     0,     0,     0,    32,     5,    18,    37,
       0,    31,    56,    58,    60,    63,    65,    67,    69,    71,
      73,    76,    78,    94,    97,    99,    37,     0,    79,    80,
      81,    82,    83,    84,    85,    86,    87,    88,    89,    90,
      91,    92,    93,   105,     0,    52,    54,     1,     4,    36,
       0,    34,     0,     0,     0,     0,     0,     0,     0,     0,
       0,     0,     0,     0,     0,     0,    99,    99,   102,     0,
      98,     0,    34,     0,   103,     0,     0,    35,     0,     0,
       0,    19,     0,    37,    33,    53,    55,    57,    59,    61,
      62,    64,    66,    68,    70,    72,    74,    75,    77,     0,
       0,   106,   101,     0,     0,     6,     0,    37,     0,    50,
      51,     0,     0,     0,     0,     0,     0,    20,     0,    95,
      96,   100,     0,     0,     7,     0,   104,     0,    21,     0,
      22,    44,    46,    45,     0,     0,     0,     0,     0,    25,
       0,     0,     8,     0,     9,     0,     0,    12,     0,     0,
       0,    47,    49,    39,    40,    41,    38,    42,     0,     0,
       0,    26,     0,     0,     0,     0,    13,    23,    24,     0,
       0,     0,    27,     0,    28,    10,    11,     0,    14,     0,
      15,    48,    43,     0,     0,     0,     0,    29,    30,    16,
      17
};

/* YYPGOTO[NTERM-NUM].  */
static const yytype_int16 yypgoto[] =
{
     -92,   -92,   -92,   220,   -92,   246,   167,   -44,    70,   -92,
     -91,   -19,    11,     9,   188,   189,   187,   116,   186,   190,
     185,    29,   -92,   135,   194,   -92,    88,   173,   -92
};

/* YYDEFGOTO[NTERM-NUM].  */
static const yytype_uint8 yydefgoto[] =
{
       0,    25,    26,    27,    28,    29,   103,    70,   177,   178,
     102,    88,    65,    66,    32,    33,    34,    35,    36,    37,
      38,    39,    40,    41,    42,    43,    89,    90,    44
};

/* YYTABLE[YYPACT[STATE-NUM]] -- What to do in state STATE-NUM.  If
   positive, shift that token.  If negative, reduce the rule whose
   number is the opposite.  If YYTABLE_NINF, syntax error.  */
static const yytype_int16 yytable[] =
{
     126,    45,    91,   133,   134,    64,   135,   131,   132,    31,
     189,    30,   149,    95,    96,   154,   150,    31,    67,    47,
      71,   155,    92,    72,    69,   156,   163,   -54,    73,    72,
     164,    72,   142,   143,    74,    31,   -54,    30,   136,    79,
     173,   174,   190,   175,   193,   199,   176,   160,   194,   200,
      97,    98,    99,   100,   168,   101,   169,    75,   170,   138,
     123,   124,   100,    78,   125,   147,   179,   180,   148,    86,
     182,    87,   183,    80,   184,   185,   129,   130,    76,    77,
     104,   105,    -3,   145,    81,     1,     2,     3,     4,     5,
       6,     7,     8,    85,   157,   158,   100,    93,   159,    94,
     203,   104,   204,   141,   161,    73,   205,   162,   206,   191,
     114,   115,   192,     9,    10,    11,    12,    13,    14,    15,
      16,    17,    18,    19,    20,    21,    22,    23,    -2,   121,
      24,     1,     2,     3,     4,     5,     6,     7,     8,   100,
     165,   166,   100,   197,   167,   122,   198,   146,   122,     1,
       2,     3,     4,     5,     6,     7,    82,    83,    84,     9,
      10,    11,    12,    13,    14,    15,    16,    17,    18,    19,
      20,    21,    22,    23,   119,   120,    24,     9,    10,    11,
      12,    13,    14,    15,    16,    17,    18,    19,    20,    21,
      22,    23,   109,   110,    24,    63,     1,     2,     3,     4,
       5,     6,     7,    48,    49,    50,    51,    52,    53,    54,
      55,    56,    57,    58,    59,    60,    61,    62,   116,   117,
     118,   137,   139,   140,     9,    10,    11,    12,    13,    14,
      15,    16,    17,    18,    19,    20,    21,    22,    23,   144,
     151,    24,   152,   153,   171,   172,    68,   181,   186,   187,
     188,   195,   196,   201,    46,   207,   208,   209,   210,   127,
     202,   106,   108,   107,   111,   113,   128,     0,     0,   112
};

static const yytype_int16 yycheck[] =
{
      91,    48,    46,     3,     4,    24,     6,    98,    99,     0,
      14,     0,    11,    16,    17,     3,    15,     8,     0,     8,
      17,     9,    17,    26,    16,    13,    11,    17,    18,    26,
      15,    26,   123,   124,    46,    26,    26,    26,    38,    22,
       3,     4,    46,     6,    11,    11,     9,   138,    15,    15,
      69,    11,    12,    13,   145,    15,   147,    27,   149,   103,
      11,    12,    13,    28,    15,    12,   157,   158,    15,    50,
     161,    52,   163,    21,   165,   166,    95,    96,    19,    20,
      71,    72,     0,   127,    29,     3,     4,     5,     6,     7,
       8,     9,    10,    30,    11,    12,    13,    47,    15,    49,
     191,    92,   193,   122,    12,    18,   197,    15,   199,    12,
      81,    82,    15,    31,    32,    33,    34,    35,    36,    37,
      38,    39,    40,    41,    42,    43,    44,    45,     0,    49,
      48,     3,     4,     5,     6,     7,     8,     9,    10,    13,
      11,    12,    13,    12,    15,    47,    15,    49,    47,     3,
       4,     5,     6,     7,     8,     9,    23,    24,    25,    31,
      32,    33,    34,    35,    36,    37,    38,    39,    40,    41,
      42,    43,    44,    45,    86,    87,    48,    31,    32,    33,
      34,    35,    36,    37,    38,    39,    40,    41,    42,    43,
      44,    45,    76,    77,    48,    49,     3,     4,     5,     6,
       7,     8,     9,     9,    10,    11,    12,    13,    14,    15,
      16,    17,    18,    19,    20,    21,    22,    23,    83,    84,
      85,    15,    51,    53,    31,    32,    33,    34,    35,    36,
      37,    38,    39,    40,    41,    42,    43,    44,    45,    15,
      14,    48,    14,    14,    14,    14,    26,    15,    15,    15,
      15,    15,    15,    14,     8,    15,    15,    15,    15,    92,
     190,    73,    75,    74,    78,    80,    93,    -1,    -1,    79
};

/* YYSTOS[STATE-NUM] -- The symbol kind of the accessing symbol of
   state STATE-NUM.  */
static const yytype_int8 yystos[] =
{
       0,     3,     4,     5,     6,     7,     8,     9,    10,    31,
      32,    33,    34,    35,    36,    37,    38,    39,    40,    41,
      42,    43,    44,    45,    48,    55,    56,    57,    58,    59,
      66,    67,    68,    69,    70,    71,    72,    73,    74,    75,
      76,    77,    78,    79,    82,    48,    59,    66,    78,    78,
      78,    78,    78,    78,    78,    78,    78,    78,    78,    78,
      78,    78,    78,    49,    65,    66,    67,     0,    57,    16,
      61,    17,    26,    18,    46,    27,    19,    20,    28,    22,
      21,    29,    23,    24,    25,    30,    50,    52,    65,    80,
      81,    61,    17,    47,    49,    16,    17,    65,    11,    12,
      13,    15,    64,    60,    67,    67,    68,    69,    70,    71,
      71,    72,    73,    74,    75,    75,    77,    77,    77,    80,
      80,    49,    47,    11,    12,    15,    64,    60,    81,    65,
      65,    64,    64,     3,     4,     6,    38,    15,    61,    51,
      53,    65,    64,    64,    15,    61,    49,    12,    15,    11,
      15,    14,    14,    14,     3,     9,    13,    11,    12,    15,
      64,    12,    15,    11,    15,    11,    12,    15,    64,    64,
      64,    14,    14,     3,     4,     6,     9,    62,    63,    64,
      64,    15,    64,    64,    64,    64,    15,    15,    15,    14,
      46,    12,    15,    11,    15,    15,    15,    12,    15,    11,
      15,    14,    62,    64,    64,    64,    64,    15,    15,    15,
      15
};

/* YYR1[RULE-NUM] -- Symbol kind of the left-hand side of rule RULE-NUM.  */
static const yytype_int8 yyr1[] =
{
       0,    54,    55,    55,    56,    56,    57,    57,    57,    57,
      57,    57,    57,    57,    57,    57,    57,    57,    57,    58,
      58,    58,    58,    58,    58,    58,    58,    58,    58,    58,
      58,    59,    59,    60,    60,    61,    61,    61,    62,    62,
      62,    62,    63,    63,    64,    64,    64,    64,    64,    64,
      65,    65,    65,    66,    66,    67,    67,    68,    68,    69,
      69,    70,    70,    70,    71,    71,    72,    72,    73,    73,
      74,    74,    75,    75,    76,    76,    76,    77,    77,    78,
      78,    78,    78,    78,    78,    78,    78,    78,    78,    78,
      78,    78,    78,    78,    78,    79,    79,    79,    80,    80,
      81,    81,    81,    82,    82,    82,    82,    82,    82,    82,
      82,    82,    82
};

/* YYR2[RULE-NUM] -- Number of symbols on the right-hand side of rule RULE-NUM.  */
static const yytype_int8 yyr2[] =
{
       0,     2,     1,     0,     2,     1,     4,     5,     6,     6,
       8,     8,     6,     7,     8,     8,    10,    10,     1,     3,
       4,     5,     5,     7,     7,     5,     6,     7,     7,     9,
       9,     1,     0,     1,     0,     2,     1,     0,     1,     1,
       1,     1,     1,     3,     3,     3,     3,     4,     6,     4,
       3,     3,     1,     3,     1,     3,     1,     3,     1,     3,
       1,     3,     3,     1,     3,     1,     3,     1,     3,     1,
       3,     1,     3,     1,     3,     3,     1,     3,     1,     2,
       2,     2,     2,     2,     2,     2,     2,     2,     2,     2,
       2,     2,     2,     2,     1,     4,     4,     1,     1,     0,
       3,     2,     1,     3,     5,     2,     4,     1,     1,     1,
       1,     1,     1
};


enum { YYENOMEM = -2 };

#define yyerrok         (yyerrstatus = 0)
#define yyclearin       (yychar = SNOBOL4_EMPTY)

#define YYACCEPT        goto yyacceptlab
#define YYABORT         goto yyabortlab
#define YYERROR         goto yyerrorlab
#define YYNOMEM         goto yyexhaustedlab


#define YYRECOVERING()  (!!yyerrstatus)

#define YYBACKUP(Token, Value)                                    \
  do                                                              \
    if (yychar == SNOBOL4_EMPTY)                                        \
      {                                                           \
        yychar = (Token);                                         \
        yylval = (Value);                                         \
        YYPOPSTACK (yylen);                                       \
        yystate = *yyssp;                                         \
        goto yybackup;                                            \
      }                                                           \
    else                                                          \
      {                                                           \
        yyerror (yyparse_param, YY_("syntax error: cannot back up")); \
        YYERROR;                                                  \
      }                                                           \
  while (0)

/* Backward compatibility with an undocumented macro.
   Use SNOBOL4_error or SNOBOL4_UNDEF. */
#define YYERRCODE SNOBOL4_UNDEF


/* Enable debugging if requested.  */
#if SNOBOL4_DEBUG

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
                  Kind, Value, yyparse_param); \
      YYFPRINTF (stderr, "\n");                                           \
    }                                                                     \
} while (0)


/*-----------------------------------.
| Print this symbol's value on YYO.  |
`-----------------------------------*/

static void
yy_symbol_value_print (FILE *yyo,
                       yysymbol_kind_t yykind, YYSTYPE const * const yyvaluep, void *yyparse_param)
{
  FILE *yyoutput = yyo;
  YY_USE (yyoutput);
  YY_USE (yyparse_param);
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
                 yysymbol_kind_t yykind, YYSTYPE const * const yyvaluep, void *yyparse_param)
{
  YYFPRINTF (yyo, "%s %s (",
             yykind < YYNTOKENS ? "token" : "nterm", yysymbol_name (yykind));

  yy_symbol_value_print (yyo, yykind, yyvaluep, yyparse_param);
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
                 int yyrule, void *yyparse_param)
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
                       &yyvsp[(yyi + 1) - (yynrhs)], yyparse_param);
      YYFPRINTF (stderr, "\n");
    }
}

# define YY_REDUCE_PRINT(Rule)          \
do {                                    \
  if (yydebug)                          \
    yy_reduce_print (yyssp, yyvsp, Rule, yyparse_param); \
} while (0)

/* Nonzero means print parse trace.  It is left uninitialized so that
   multiple parsers can coexist.  */
int yydebug;
#else /* !SNOBOL4_DEBUG */
# define YYDPRINTF(Args) ((void) 0)
# define YY_SYMBOL_PRINT(Title, Kind, Value, Location)
# define YY_STACK_PRINT(Bottom, Top)
# define YY_REDUCE_PRINT(Rule)
#endif /* !SNOBOL4_DEBUG */


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
            yysymbol_kind_t yykind, YYSTYPE *yyvaluep, void *yyparse_param)
{
  YY_USE (yyvaluep);
  YY_USE (yyparse_param);
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
yyparse (void *yyparse_param)
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

  yychar = SNOBOL4_EMPTY; /* Cause a token to be read.  */

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
  if (yychar == SNOBOL4_EMPTY)
    {
      YYDPRINTF ((stderr, "Reading a token\n"));
      yychar = yylex (&yylval);
    }

  if (yychar <= SNOBOL4_EOF)
    {
      yychar = SNOBOL4_EOF;
      yytoken = YYSYMBOL_YYEOF;
      YYDPRINTF ((stderr, "Now at end of input.\n"));
    }
  else if (yychar == SNOBOL4_error)
    {
      /* The scanner already issued an error message, process directly
         to error recovery.  But do not keep the error token as
         lookahead, it is too special and may lead us to an endless
         loop in error recovery. */
      yychar = SNOBOL4_UNDEF;
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
  yychar = SNOBOL4_EMPTY;
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
  case 2: /* top: program  */
#line 54 "snobol4.y"
                                                                                                    { }
#line 1339 "snobol4.tab.c"
    break;

  case 3: /* top: %empty  */
#line 55 "snobol4.y"
                                                                                        { }
#line 1345 "snobol4.tab.c"
    break;

  case 6: /* stmt: T_LABEL opt_subject opt_repl T_STMT_END  */
#line 59 "snobol4.y"
                                                                                           { sno4_stmt_commit_go(yyparse_param,(yyvsp[-3].tok),(yyvsp[-2].expr),NULL,((yyvsp[-1].expr)!=NULL),(yyvsp[-1].expr),NULL,NULL,NULL); }
#line 1351 "snobol4.tab.c"
    break;

  case 7: /* stmt: T_LABEL opt_subject opt_repl goto_label_expr T_STMT_END  */
#line 60 "snobol4.y"
                                                                                           { sno4_stmt_commit_go(yyparse_param,(yyvsp[-4].tok),(yyvsp[-3].expr),NULL,((yyvsp[-2].expr)!=NULL),(yyvsp[-2].expr),(yyvsp[-1].expr),NULL,NULL); }
#line 1357 "snobol4.tab.c"
    break;

  case 8: /* stmt: T_LABEL opt_subject opt_repl T_GOTO_S goto_label_expr T_STMT_END  */
#line 61 "snobol4.y"
                                                                                           { sno4_stmt_commit_go(yyparse_param,(yyvsp[-5].tok),(yyvsp[-4].expr),NULL,((yyvsp[-3].expr)!=NULL),(yyvsp[-3].expr),NULL,(yyvsp[-1].expr),NULL); }
#line 1363 "snobol4.tab.c"
    break;

  case 9: /* stmt: T_LABEL opt_subject opt_repl T_GOTO_F goto_label_expr T_STMT_END  */
#line 62 "snobol4.y"
                                                                                           { sno4_stmt_commit_go(yyparse_param,(yyvsp[-5].tok),(yyvsp[-4].expr),NULL,((yyvsp[-3].expr)!=NULL),(yyvsp[-3].expr),NULL,NULL,(yyvsp[-1].expr)); }
#line 1369 "snobol4.tab.c"
    break;

  case 10: /* stmt: T_LABEL opt_subject opt_repl T_GOTO_S goto_label_expr T_GOTO_F goto_label_expr T_STMT_END  */
#line 63 "snobol4.y"
                                                                                                         { sno4_stmt_commit_go(yyparse_param,(yyvsp[-7].tok),(yyvsp[-6].expr),NULL,((yyvsp[-5].expr)!=NULL),(yyvsp[-5].expr),NULL,(yyvsp[-3].expr),(yyvsp[-1].expr)); }
#line 1375 "snobol4.tab.c"
    break;

  case 11: /* stmt: T_LABEL opt_subject opt_repl T_GOTO_F goto_label_expr T_GOTO_S goto_label_expr T_STMT_END  */
#line 64 "snobol4.y"
                                                                                                         { sno4_stmt_commit_go(yyparse_param,(yyvsp[-7].tok),(yyvsp[-6].expr),NULL,((yyvsp[-5].expr)!=NULL),(yyvsp[-5].expr),NULL,(yyvsp[-1].expr),(yyvsp[-3].expr)); }
#line 1381 "snobol4.tab.c"
    break;

  case 12: /* stmt: T_LABEL expr2 T_2QUEST opt_pattern opt_repl T_STMT_END  */
#line 65 "snobol4.y"
                                                                                         { sno4_stmt_commit_go(yyparse_param,(yyvsp[-5].tok),expr_binary(TT_SCAN,(yyvsp[-4].expr),(yyvsp[-2].expr)),NULL,((yyvsp[-1].expr)!=NULL),(yyvsp[-1].expr),NULL,NULL,NULL); }
#line 1387 "snobol4.tab.c"
    break;

  case 13: /* stmt: T_LABEL expr2 T_2QUEST opt_pattern opt_repl goto_label_expr T_STMT_END  */
#line 66 "snobol4.y"
                                                                                         { sno4_stmt_commit_go(yyparse_param,(yyvsp[-6].tok),expr_binary(TT_SCAN,(yyvsp[-5].expr),(yyvsp[-3].expr)),NULL,((yyvsp[-2].expr)!=NULL),(yyvsp[-2].expr),(yyvsp[-1].expr),NULL,NULL); }
#line 1393 "snobol4.tab.c"
    break;

  case 14: /* stmt: T_LABEL expr2 T_2QUEST opt_pattern opt_repl T_GOTO_S goto_label_expr T_STMT_END  */
#line 67 "snobol4.y"
                                                                                             { sno4_stmt_commit_go(yyparse_param,(yyvsp[-7].tok),expr_binary(TT_SCAN,(yyvsp[-6].expr),(yyvsp[-4].expr)),NULL,((yyvsp[-3].expr)!=NULL),(yyvsp[-3].expr),NULL,(yyvsp[-1].expr),NULL); }
#line 1399 "snobol4.tab.c"
    break;

  case 15: /* stmt: T_LABEL expr2 T_2QUEST opt_pattern opt_repl T_GOTO_F goto_label_expr T_STMT_END  */
#line 68 "snobol4.y"
                                                                                             { sno4_stmt_commit_go(yyparse_param,(yyvsp[-7].tok),expr_binary(TT_SCAN,(yyvsp[-6].expr),(yyvsp[-4].expr)),NULL,((yyvsp[-3].expr)!=NULL),(yyvsp[-3].expr),NULL,NULL,(yyvsp[-1].expr)); }
#line 1405 "snobol4.tab.c"
    break;

  case 16: /* stmt: T_LABEL expr2 T_2QUEST opt_pattern opt_repl T_GOTO_S goto_label_expr T_GOTO_F goto_label_expr T_STMT_END  */
#line 69 "snobol4.y"
                                                                                                                      { sno4_stmt_commit_go(yyparse_param,(yyvsp[-9].tok),expr_binary(TT_SCAN,(yyvsp[-8].expr),(yyvsp[-6].expr)),NULL,((yyvsp[-5].expr)!=NULL),(yyvsp[-5].expr),NULL,(yyvsp[-3].expr),(yyvsp[-1].expr)); }
#line 1411 "snobol4.tab.c"
    break;

  case 17: /* stmt: T_LABEL expr2 T_2QUEST opt_pattern opt_repl T_GOTO_F goto_label_expr T_GOTO_S goto_label_expr T_STMT_END  */
#line 70 "snobol4.y"
                                                                                                                      { sno4_stmt_commit_go(yyparse_param,(yyvsp[-9].tok),expr_binary(TT_SCAN,(yyvsp[-8].expr),(yyvsp[-6].expr)),NULL,((yyvsp[-5].expr)!=NULL),(yyvsp[-5].expr),NULL,(yyvsp[-1].expr),(yyvsp[-3].expr)); }
#line 1417 "snobol4.tab.c"
    break;

  case 19: /* unlabeled_stmt: opt_subject opt_repl T_STMT_END  */
#line 74 "snobol4.y"
                                                                                           { sno4_stmt_commit_go(yyparse_param,((Token){NULL,0,0,0}),(yyvsp[-2].expr),NULL,((yyvsp[-1].expr)!=NULL),(yyvsp[-1].expr),NULL,NULL,NULL); }
#line 1423 "snobol4.tab.c"
    break;

  case 20: /* unlabeled_stmt: opt_subject opt_repl goto_label_expr T_STMT_END  */
#line 75 "snobol4.y"
                                                                                           { sno4_stmt_commit_go(yyparse_param,((Token){NULL,0,0,0}),(yyvsp[-3].expr),NULL,((yyvsp[-2].expr)!=NULL),(yyvsp[-2].expr),(yyvsp[-1].expr),NULL,NULL); }
#line 1429 "snobol4.tab.c"
    break;

  case 21: /* unlabeled_stmt: opt_subject opt_repl T_GOTO_S goto_label_expr T_STMT_END  */
#line 76 "snobol4.y"
                                                                                           { sno4_stmt_commit_go(yyparse_param,((Token){NULL,0,0,0}),(yyvsp[-4].expr),NULL,((yyvsp[-3].expr)!=NULL),(yyvsp[-3].expr),NULL,(yyvsp[-1].expr),NULL); }
#line 1435 "snobol4.tab.c"
    break;

  case 22: /* unlabeled_stmt: opt_subject opt_repl T_GOTO_F goto_label_expr T_STMT_END  */
#line 77 "snobol4.y"
                                                                                           { sno4_stmt_commit_go(yyparse_param,((Token){NULL,0,0,0}),(yyvsp[-4].expr),NULL,((yyvsp[-3].expr)!=NULL),(yyvsp[-3].expr),NULL,NULL,(yyvsp[-1].expr)); }
#line 1441 "snobol4.tab.c"
    break;

  case 23: /* unlabeled_stmt: opt_subject opt_repl T_GOTO_S goto_label_expr T_GOTO_F goto_label_expr T_STMT_END  */
#line 78 "snobol4.y"
                                                                                                 { sno4_stmt_commit_go(yyparse_param,((Token){NULL,0,0,0}),(yyvsp[-6].expr),NULL,((yyvsp[-5].expr)!=NULL),(yyvsp[-5].expr),NULL,(yyvsp[-3].expr),(yyvsp[-1].expr)); }
#line 1447 "snobol4.tab.c"
    break;

  case 24: /* unlabeled_stmt: opt_subject opt_repl T_GOTO_F goto_label_expr T_GOTO_S goto_label_expr T_STMT_END  */
#line 79 "snobol4.y"
                                                                                                 { sno4_stmt_commit_go(yyparse_param,((Token){NULL,0,0,0}),(yyvsp[-6].expr),NULL,((yyvsp[-5].expr)!=NULL),(yyvsp[-5].expr),NULL,(yyvsp[-1].expr),(yyvsp[-3].expr)); }
#line 1453 "snobol4.tab.c"
    break;

  case 25: /* unlabeled_stmt: expr2 T_2QUEST opt_pattern opt_repl T_STMT_END  */
#line 80 "snobol4.y"
                                                                                         { sno4_stmt_commit_go(yyparse_param,((Token){NULL,0,0,0}),expr_binary(TT_SCAN,(yyvsp[-4].expr),(yyvsp[-2].expr)),NULL,((yyvsp[-1].expr)!=NULL),(yyvsp[-1].expr),NULL,NULL,NULL); }
#line 1459 "snobol4.tab.c"
    break;

  case 26: /* unlabeled_stmt: expr2 T_2QUEST opt_pattern opt_repl goto_label_expr T_STMT_END  */
#line 81 "snobol4.y"
                                                                                         { sno4_stmt_commit_go(yyparse_param,((Token){NULL,0,0,0}),expr_binary(TT_SCAN,(yyvsp[-5].expr),(yyvsp[-3].expr)),NULL,((yyvsp[-2].expr)!=NULL),(yyvsp[-2].expr),(yyvsp[-1].expr),NULL,NULL); }
#line 1465 "snobol4.tab.c"
    break;

  case 27: /* unlabeled_stmt: expr2 T_2QUEST opt_pattern opt_repl T_GOTO_S goto_label_expr T_STMT_END  */
#line 82 "snobol4.y"
                                                                                         { sno4_stmt_commit_go(yyparse_param,((Token){NULL,0,0,0}),expr_binary(TT_SCAN,(yyvsp[-6].expr),(yyvsp[-4].expr)),NULL,((yyvsp[-3].expr)!=NULL),(yyvsp[-3].expr),NULL,(yyvsp[-1].expr),NULL); }
#line 1471 "snobol4.tab.c"
    break;

  case 28: /* unlabeled_stmt: expr2 T_2QUEST opt_pattern opt_repl T_GOTO_F goto_label_expr T_STMT_END  */
#line 83 "snobol4.y"
                                                                                         { sno4_stmt_commit_go(yyparse_param,((Token){NULL,0,0,0}),expr_binary(TT_SCAN,(yyvsp[-6].expr),(yyvsp[-4].expr)),NULL,((yyvsp[-3].expr)!=NULL),(yyvsp[-3].expr),NULL,NULL,(yyvsp[-1].expr)); }
#line 1477 "snobol4.tab.c"
    break;

  case 29: /* unlabeled_stmt: expr2 T_2QUEST opt_pattern opt_repl T_GOTO_S goto_label_expr T_GOTO_F goto_label_expr T_STMT_END  */
#line 84 "snobol4.y"
                                                                                                              { sno4_stmt_commit_go(yyparse_param,((Token){NULL,0,0,0}),expr_binary(TT_SCAN,(yyvsp[-8].expr),(yyvsp[-6].expr)),NULL,((yyvsp[-5].expr)!=NULL),(yyvsp[-5].expr),NULL,(yyvsp[-3].expr),(yyvsp[-1].expr)); }
#line 1483 "snobol4.tab.c"
    break;

  case 30: /* unlabeled_stmt: expr2 T_2QUEST opt_pattern opt_repl T_GOTO_F goto_label_expr T_GOTO_S goto_label_expr T_STMT_END  */
#line 85 "snobol4.y"
                                                                                                              { sno4_stmt_commit_go(yyparse_param,((Token){NULL,0,0,0}),expr_binary(TT_SCAN,(yyvsp[-8].expr),(yyvsp[-6].expr)),NULL,((yyvsp[-5].expr)!=NULL),(yyvsp[-5].expr),NULL,(yyvsp[-1].expr),(yyvsp[-3].expr)); }
#line 1489 "snobol4.tab.c"
    break;

  case 31: /* opt_subject: expr3  */
#line 87 "snobol4.y"
                                                                                                  { (yyval.expr)=(yyvsp[0].expr); }
#line 1495 "snobol4.tab.c"
    break;

  case 32: /* opt_subject: %empty  */
#line 88 "snobol4.y"
                                                                                       { (yyval.expr)=NULL; }
#line 1501 "snobol4.tab.c"
    break;

  case 33: /* opt_pattern: expr3  */
#line 90 "snobol4.y"
                                                                                                   { (yyval.expr)=(yyvsp[0].expr); }
#line 1507 "snobol4.tab.c"
    break;

  case 34: /* opt_pattern: %empty  */
#line 91 "snobol4.y"
                                                                                       { (yyval.expr)=NULL; }
#line 1513 "snobol4.tab.c"
    break;

  case 35: /* opt_repl: T_2EQUAL expr0  */
#line 93 "snobol4.y"
                                                                                              { (yyval.expr)=(yyvsp[0].expr); }
#line 1519 "snobol4.tab.c"
    break;

  case 36: /* opt_repl: T_2EQUAL  */
#line 94 "snobol4.y"
                                                                                               { tree_t*e=ast_node_new(TT_QLIT);e->v.sval=strdup("");(yyval.expr)=e; }
#line 1525 "snobol4.tab.c"
    break;

  case 37: /* opt_repl: %empty  */
#line 95 "snobol4.y"
                                                                                       { (yyval.expr)=NULL; }
#line 1531 "snobol4.tab.c"
    break;

  case 38: /* goto_atom: T_STR  */
#line 97 "snobol4.y"
                     { tree_t*e=ast_node_new(TT_QLIT);  e->v.sval=(char*)(yyvsp[0].tok).sval; (yyval.expr)=e; }
#line 1537 "snobol4.tab.c"
    break;

  case 39: /* goto_atom: T_IDENT  */
#line 98 "snobol4.y"
                      { tree_t*e=ast_node_new(TT_VAR);   e->v.sval=(char*)(yyvsp[0].tok).sval; (yyval.expr)=e; }
#line 1543 "snobol4.tab.c"
    break;

  case 40: /* goto_atom: T_FUNCTION  */
#line 99 "snobol4.y"
                        { tree_t*e=ast_node_new(TT_VAR); e->v.sval=(char*)(yyvsp[0].tok).sval; (yyval.expr)=e; }
#line 1549 "snobol4.tab.c"
    break;

  case 41: /* goto_atom: T_END  */
#line 100 "snobol4.y"
                      { tree_t*e=ast_node_new(TT_VAR);   e->v.sval=(char*)(yyvsp[0].tok).sval; (yyval.expr)=e; }
#line 1555 "snobol4.tab.c"
    break;

  case 42: /* goto_expr: goto_atom  */
#line 102 "snobol4.y"
                                                { (yyval.expr)=(yyvsp[0].expr); }
#line 1561 "snobol4.tab.c"
    break;

  case 43: /* goto_expr: goto_expr T_CONCAT goto_atom  */
#line 103 "snobol4.y"
                                                { tree_t*s=ast_node_new(TT_SEQ);expr_add_child(s,(yyvsp[-2].expr));expr_add_child(s,(yyvsp[0].expr));(yyval.expr)=s; }
#line 1567 "snobol4.tab.c"
    break;

  case 44: /* goto_label_expr: T_GOTO_LPAREN T_IDENT T_GOTO_RPAREN  */
#line 106 "snobol4.y"
                                                                                             { tree_t*e=ast_node_new(TT_QLIT);e->v.sval=strdup((yyvsp[-1].tok).sval);(yyval.expr)=e; }
#line 1573 "snobol4.tab.c"
    break;

  case 45: /* goto_label_expr: T_GOTO_LPAREN T_END T_GOTO_RPAREN  */
#line 107 "snobol4.y"
                                                                                             { tree_t*e=ast_node_new(TT_QLIT);e->v.sval=strdup((yyvsp[-1].tok).sval);(yyval.expr)=e; }
#line 1579 "snobol4.tab.c"
    break;

  case 46: /* goto_label_expr: T_GOTO_LPAREN T_FUNCTION T_GOTO_RPAREN  */
#line 108 "snobol4.y"
                                                                                             { tree_t*e=ast_node_new(TT_QLIT);e->v.sval=strdup((yyvsp[-1].tok).sval);(yyval.expr)=e; }
#line 1585 "snobol4.tab.c"
    break;

  case 47: /* goto_label_expr: T_GOTO_LPAREN T_1DOLLAR T_IDENT T_GOTO_RPAREN  */
#line 109 "snobol4.y"
                                                                                             { tree_t*e=ast_node_new(TT_QLIT);char buf[512];snprintf(buf,sizeof buf,"$%s",(yyvsp[-1].tok).sval);e->v.sval=strdup(buf);(yyval.expr)=e; }
#line 1591 "snobol4.tab.c"
    break;

  case 48: /* goto_label_expr: T_GOTO_LPAREN T_1DOLLAR T_GOTO_LPAREN goto_expr T_GOTO_RPAREN T_GOTO_RPAREN  */
#line 110 "snobol4.y"
                                                                                            { (yyval.expr)=(yyvsp[-2].expr); }
#line 1597 "snobol4.tab.c"
    break;

  case 49: /* goto_label_expr: T_GOTO_LPAREN T_1DOLLAR T_STR T_GOTO_RPAREN  */
#line 111 "snobol4.y"
                                                                                             { tree_t*e=ast_node_new(TT_QLIT);e->v.sval=strdup((yyvsp[-1].tok).sval);(yyval.expr)=e; }
#line 1603 "snobol4.tab.c"
    break;

  case 50: /* expr0: expr2 T_2EQUAL expr0  */
#line 113 "snobol4.y"
                                                                                              { (yyval.expr)=expr_binary(TT_ASSIGN,          (yyvsp[-2].expr),(yyvsp[0].expr)); }
#line 1609 "snobol4.tab.c"
    break;

  case 51: /* expr0: expr2 T_2QUEST expr0  */
#line 114 "snobol4.y"
                                                                                                   { (yyval.expr)=expr_binary(TT_SCAN,            (yyvsp[-2].expr),(yyvsp[0].expr)); }
#line 1615 "snobol4.tab.c"
    break;

  case 52: /* expr0: expr2  */
#line 115 "snobol4.y"
                                                                                                   { (yyval.expr)=(yyvsp[0].expr); }
#line 1621 "snobol4.tab.c"
    break;

  case 53: /* expr2: expr2 T_2AMP expr3  */
#line 117 "snobol4.y"
                                                                                             { tree_t*_e=expr_binary(TT_OPSYN,(yyvsp[-2].expr),(yyvsp[0].expr)); _e->v.sval=strdup("&"); (yyval.expr)=_e; }
#line 1627 "snobol4.tab.c"
    break;

  case 54: /* expr2: expr3  */
#line 118 "snobol4.y"
                                                                                                   { (yyval.expr)=(yyvsp[0].expr); }
#line 1633 "snobol4.tab.c"
    break;

  case 55: /* expr3: expr3 T_2PIPE expr4  */
#line 120 "snobol4.y"
                                                                                            { tree_t*a=ast_node_new(TT_ALT);expr_add_child(a,(yyvsp[-2].expr));expr_add_child(a,(yyvsp[0].expr));(yyval.expr)=a; }
#line 1639 "snobol4.tab.c"
    break;

  case 56: /* expr3: expr4  */
#line 121 "snobol4.y"
                                                                                                   { (yyval.expr)=(yyvsp[0].expr); }
#line 1645 "snobol4.tab.c"
    break;

  case 57: /* expr4: expr4 T_CONCAT expr5  */
#line 123 "snobol4.y"
                                                                                                            { tree_t*s=ast_node_new(TT_SEQ);expr_add_child(s,(yyvsp[-2].expr));expr_add_child(s,(yyvsp[0].expr));(yyval.expr)=s; }
#line 1651 "snobol4.tab.c"
    break;

  case 58: /* expr4: expr5  */
#line 124 "snobol4.y"
                                                                                                   { (yyval.expr)=(yyvsp[0].expr); }
#line 1657 "snobol4.tab.c"
    break;

  case 59: /* expr5: expr5 T_2AT expr6  */
#line 126 "snobol4.y"
                                                                                              { tree_t*_e=expr_binary(TT_OPSYN,(yyvsp[-2].expr),(yyvsp[0].expr)); _e->v.sval=strdup("@"); (yyval.expr)=_e; }
#line 1663 "snobol4.tab.c"
    break;

  case 60: /* expr5: expr6  */
#line 127 "snobol4.y"
                                                                                                   { (yyval.expr)=(yyvsp[0].expr); }
#line 1669 "snobol4.tab.c"
    break;

  case 61: /* expr6: expr6 T_2PLUS expr7  */
#line 129 "snobol4.y"
                                                                                               { (yyval.expr)=expr_binary(TT_ADD,             (yyvsp[-2].expr),(yyvsp[0].expr)); }
#line 1675 "snobol4.tab.c"
    break;

  case 62: /* expr6: expr6 T_2MINUS expr7  */
#line 130 "snobol4.y"
                                                                                             { (yyval.expr)=expr_binary(TT_SUB,             (yyvsp[-2].expr),(yyvsp[0].expr)); }
#line 1681 "snobol4.tab.c"
    break;

  case 63: /* expr6: expr7  */
#line 131 "snobol4.y"
                                                                                                   { (yyval.expr)=(yyvsp[0].expr); }
#line 1687 "snobol4.tab.c"
    break;

  case 64: /* expr7: expr7 T_2POUND expr8  */
#line 133 "snobol4.y"
                                                                                                   { (yyval.expr)=expr_binary(TT_MUL,             (yyvsp[-2].expr),(yyvsp[0].expr)); }
#line 1693 "snobol4.tab.c"
    break;

  case 65: /* expr7: expr8  */
#line 134 "snobol4.y"
                                                                                                   { (yyval.expr)=(yyvsp[0].expr); }
#line 1699 "snobol4.tab.c"
    break;

  case 66: /* expr8: expr8 T_2SLASH expr9  */
#line 136 "snobol4.y"
                                                                                                { (yyval.expr)=expr_binary(TT_DIV,             (yyvsp[-2].expr),(yyvsp[0].expr)); }
#line 1705 "snobol4.tab.c"
    break;

  case 67: /* expr8: expr9  */
#line 137 "snobol4.y"
                                                                                                   { (yyval.expr)=(yyvsp[0].expr); }
#line 1711 "snobol4.tab.c"
    break;

  case 68: /* expr9: expr9 T_2STAR expr10  */
#line 139 "snobol4.y"
                                                                                         { (yyval.expr)=expr_binary(TT_MUL,             (yyvsp[-2].expr),(yyvsp[0].expr)); }
#line 1717 "snobol4.tab.c"
    break;

  case 69: /* expr9: expr10  */
#line 140 "snobol4.y"
                                                                                                   { (yyval.expr)=(yyvsp[0].expr); }
#line 1723 "snobol4.tab.c"
    break;

  case 70: /* expr10: expr10 T_2PERCENT expr11  */
#line 142 "snobol4.y"
                                                                                                   { (yyval.expr)=expr_binary(TT_DIV,             (yyvsp[-2].expr),(yyvsp[0].expr)); }
#line 1729 "snobol4.tab.c"
    break;

  case 71: /* expr10: expr11  */
#line 143 "snobol4.y"
                                                                                                   { (yyval.expr)=(yyvsp[0].expr); }
#line 1735 "snobol4.tab.c"
    break;

  case 72: /* expr11: expr12 T_2CARET expr11  */
#line 145 "snobol4.y"
                                                                                          { (yyval.expr)=expr_binary(TT_POW,             (yyvsp[-2].expr),(yyvsp[0].expr)); }
#line 1741 "snobol4.tab.c"
    break;

  case 73: /* expr11: expr12  */
#line 146 "snobol4.y"
                                                                                                   { (yyval.expr)=(yyvsp[0].expr); }
#line 1747 "snobol4.tab.c"
    break;

  case 74: /* expr12: expr12 T_2DOLLAR expr13  */
#line 148 "snobol4.y"
                                                                                         { (yyval.expr)=expr_binary(TT_CAPT_IMMED_ASGN,(yyvsp[-2].expr),(yyvsp[0].expr)); }
#line 1753 "snobol4.tab.c"
    break;

  case 75: /* expr12: expr12 T_2DOT expr13  */
#line 149 "snobol4.y"
                                                                                           { (yyval.expr)=expr_binary(TT_CAPT_COND_ASGN, (yyvsp[-2].expr),(yyvsp[0].expr)); }
#line 1759 "snobol4.tab.c"
    break;

  case 76: /* expr12: expr13  */
#line 150 "snobol4.y"
                                                                                                   { (yyval.expr)=(yyvsp[0].expr); }
#line 1765 "snobol4.tab.c"
    break;

  case 77: /* expr13: expr14 T_2TILDE expr13  */
#line 152 "snobol4.y"
                                                                                                   { tree_t*_e=expr_binary(TT_OPSYN,(yyvsp[-2].expr),(yyvsp[0].expr)); _e->v.sval=strdup("~"); (yyval.expr)=_e; }
#line 1771 "snobol4.tab.c"
    break;

  case 78: /* expr13: expr14  */
#line 153 "snobol4.y"
                                                                                                   { (yyval.expr)=(yyvsp[0].expr); }
#line 1777 "snobol4.tab.c"
    break;

  case 79: /* expr14: T_1AT expr14  */
#line 155 "snobol4.y"
                                                                                           { (yyval.expr)=expr_unary(TT_CAPT_CURSOR,     (yyvsp[0].expr)); }
#line 1783 "snobol4.tab.c"
    break;

  case 80: /* expr14: T_1TILDE expr14  */
#line 156 "snobol4.y"
                                                                                                { (yyval.expr)=expr_unary(TT_NOT,             (yyvsp[0].expr)); }
#line 1789 "snobol4.tab.c"
    break;

  case 81: /* expr14: T_1QUEST expr14  */
#line 157 "snobol4.y"
                                                                                        { (yyval.expr)=expr_unary(TT_INTERROGATE,     (yyvsp[0].expr)); }
#line 1795 "snobol4.tab.c"
    break;

  case 82: /* expr14: T_1AMP expr14  */
#line 158 "snobol4.y"
                                                                                          { tree_t*_e=expr_unary(TT_OPSYN,(yyvsp[0].expr)); _e->v.sval=strdup("&"); (yyval.expr)=_e; }
#line 1801 "snobol4.tab.c"
    break;

  case 83: /* expr14: T_1PLUS expr14  */
#line 159 "snobol4.y"
                                                                                                { (yyval.expr)=expr_unary(TT_PLS,             (yyvsp[0].expr)); }
#line 1807 "snobol4.tab.c"
    break;

  case 84: /* expr14: T_1MINUS expr14  */
#line 160 "snobol4.y"
                                                                                                { (yyval.expr)=expr_unary(TT_MNS,             (yyvsp[0].expr)); }
#line 1813 "snobol4.tab.c"
    break;

  case 85: /* expr14: T_1STAR expr14  */
#line 161 "snobol4.y"
                                                                                            { (yyval.expr)=expr_unary(TT_DEFER,           (yyvsp[0].expr)); }
#line 1819 "snobol4.tab.c"
    break;

  case 86: /* expr14: T_1DOLLAR expr14  */
#line 162 "snobol4.y"
                                                                                           { (yyval.expr)=expr_unary(TT_INDIRECT,        (yyvsp[0].expr)); }
#line 1825 "snobol4.tab.c"
    break;

  case 87: /* expr14: T_1DOT expr14  */
#line 163 "snobol4.y"
                                                                                             { (yyval.expr)=expr_unary(TT_NAME,            (yyvsp[0].expr)); }
#line 1831 "snobol4.tab.c"
    break;

  case 88: /* expr14: T_1BANG expr14  */
#line 164 "snobol4.y"
                                                                                         { (yyval.expr)=expr_unary(TT_POW,             (yyvsp[0].expr)); }
#line 1837 "snobol4.tab.c"
    break;

  case 89: /* expr14: T_1PERCENT expr14  */
#line 165 "snobol4.y"
                                                                                                { (yyval.expr)=expr_unary(TT_DIV,             (yyvsp[0].expr)); }
#line 1843 "snobol4.tab.c"
    break;

  case 90: /* expr14: T_1SLASH expr14  */
#line 166 "snobol4.y"
                                                                                                { (yyval.expr)=expr_unary(TT_DIV,             (yyvsp[0].expr)); }
#line 1849 "snobol4.tab.c"
    break;

  case 91: /* expr14: T_1POUND expr14  */
#line 167 "snobol4.y"
                                                                                                { (yyval.expr)=expr_unary(TT_MUL,             (yyvsp[0].expr)); }
#line 1855 "snobol4.tab.c"
    break;

  case 92: /* expr14: T_1EQUAL expr14  */
#line 168 "snobol4.y"
                                                                                                { (yyval.expr)=expr_unary(TT_ASSIGN,          (yyvsp[0].expr)); }
#line 1861 "snobol4.tab.c"
    break;

  case 93: /* expr14: T_1PIPE expr14  */
#line 169 "snobol4.y"
                                                                                        { tree_t*_e=expr_unary(TT_OPSYN,(yyvsp[0].expr)); _e->v.sval=strdup("|"); (yyval.expr)=_e; }
#line 1867 "snobol4.tab.c"
    break;

  case 94: /* expr14: expr15  */
#line 170 "snobol4.y"
                                                                                                   { (yyval.expr)=(yyvsp[0].expr); }
#line 1873 "snobol4.tab.c"
    break;

  case 95: /* expr15: expr15 T_LBRACK exprlist T_RBRACK  */
#line 172 "snobol4.y"
                                                                                                { tree_t*i=ast_node_new(TT_IDX);expr_add_child(i,(yyvsp[-3].expr));for(int j=0;j<(yyvsp[-1].expr)->n;j++)expr_add_child(i,(yyvsp[-1].expr)->c[j]);free((yyvsp[-1].expr)->c);free((yyvsp[-1].expr));(yyval.expr)=i; }
#line 1879 "snobol4.tab.c"
    break;

  case 96: /* expr15: expr15 T_LANGLE exprlist T_RANGLE  */
#line 173 "snobol4.y"
                                                                                                { tree_t*i=ast_node_new(TT_IDX);expr_add_child(i,(yyvsp[-3].expr));for(int j=0;j<(yyvsp[-1].expr)->n;j++)expr_add_child(i,(yyvsp[-1].expr)->c[j]);free((yyvsp[-1].expr)->c);free((yyvsp[-1].expr));(yyval.expr)=i; }
#line 1885 "snobol4.tab.c"
    break;

  case 97: /* expr15: expr17  */
#line 174 "snobol4.y"
                                                                                                   { (yyval.expr)=(yyvsp[0].expr); }
#line 1891 "snobol4.tab.c"
    break;

  case 98: /* exprlist: exprlist_ne  */
#line 176 "snobol4.y"
                                                                                                   { (yyval.expr)=(yyvsp[0].expr); }
#line 1897 "snobol4.tab.c"
    break;

  case 99: /* exprlist: %empty  */
#line 177 "snobol4.y"
                                                                                       { (yyval.expr)=ast_node_new(TT_NUL); }
#line 1903 "snobol4.tab.c"
    break;

  case 100: /* exprlist_ne: exprlist_ne T_COMMA expr0  */
#line 179 "snobol4.y"
                                                                                                  { expr_add_child((yyvsp[-2].expr),(yyvsp[0].expr));(yyval.expr)=(yyvsp[-2].expr); }
#line 1909 "snobol4.tab.c"
    break;

  case 101: /* exprlist_ne: exprlist_ne T_COMMA  */
#line 180 "snobol4.y"
                                                                                                  { expr_add_child((yyvsp[-1].expr),ast_node_new(TT_NUL));(yyval.expr)=(yyvsp[-1].expr); }
#line 1915 "snobol4.tab.c"
    break;

  case 102: /* exprlist_ne: expr0  */
#line 181 "snobol4.y"
                                                                                                   { tree_t*l=ast_node_new(TT_NUL);expr_add_child(l,(yyvsp[0].expr));(yyval.expr)=l; }
#line 1921 "snobol4.tab.c"
    break;

  case 103: /* expr17: T_LPAREN expr0 T_RPAREN  */
#line 183 "snobol4.y"
                                                                                                { (yyval.expr)=(yyvsp[-1].expr); }
#line 1927 "snobol4.tab.c"
    break;

  case 104: /* expr17: T_LPAREN expr0 T_COMMA exprlist_ne T_RPAREN  */
#line 184 "snobol4.y"
                                                                                               { tree_t*a=ast_node_new(TT_VLIST);expr_add_child(a,(yyvsp[-3].expr));for(int i=0;i<(yyvsp[-1].expr)->n;i++)expr_add_child(a,(yyvsp[-1].expr)->c[i]);free((yyvsp[-1].expr)->c);free((yyvsp[-1].expr));(yyval.expr)=a; }
#line 1933 "snobol4.tab.c"
    break;

  case 105: /* expr17: T_LPAREN T_RPAREN  */
#line 185 "snobol4.y"
                                                                                                { (yyval.expr)=ast_node_new(TT_NUL); }
#line 1939 "snobol4.tab.c"
    break;

  case 106: /* expr17: T_FUNCTION T_LPAREN exprlist T_RPAREN  */
#line 186 "snobol4.y"
                                                                                               { tree_e _k=pat_prim_kind((yyvsp[-3].tok).sval);tree_t*e=ast_node_new(_k==TT_VAR?TT_FNC:_k);if(_k==TT_VAR)e->v.sval=(char*)(yyvsp[-3].tok).sval;for(int i=0;i<(yyvsp[-1].expr)->n;i++)expr_add_child(e,(yyvsp[-1].expr)->c[i]);free((yyvsp[-1].expr)->c);free((yyvsp[-1].expr));(yyval.expr)=e; }
#line 1945 "snobol4.tab.c"
    break;

  case 107: /* expr17: T_IDENT  */
#line 187 "snobol4.y"
                                                                                                  { tree_t*e=ast_node_new(TT_VAR);e->v.sval=(char*)(yyvsp[0].tok).sval;(yyval.expr)=e; }
#line 1951 "snobol4.tab.c"
    break;

  case 108: /* expr17: T_END  */
#line 188 "snobol4.y"
                                                                                                  { tree_t*e=ast_node_new(TT_VAR);    e->v.sval=(char*)(yyvsp[0].tok).sval;(yyval.expr)=e; }
#line 1957 "snobol4.tab.c"
    break;

  case 109: /* expr17: T_KEYWORD  */
#line 189 "snobol4.y"
                                                                                                  { tree_t*e=ast_node_new(TT_KEYWORD);e->v.sval=(char*)(yyvsp[0].tok).sval;(yyval.expr)=e; }
#line 1963 "snobol4.tab.c"
    break;

  case 110: /* expr17: T_STR  */
#line 190 "snobol4.y"
                                                                                                  { tree_t*e=ast_node_new(TT_QLIT);   e->v.sval=(char*)(yyvsp[0].tok).sval;(yyval.expr)=e; }
#line 1969 "snobol4.tab.c"
    break;

  case 111: /* expr17: T_INT  */
#line 191 "snobol4.y"
                                                                                                  { tree_t*e=ast_node_new(TT_ILIT);   e->v.ival=(yyvsp[0].tok).ival;(yyval.expr)=e; }
#line 1975 "snobol4.tab.c"
    break;

  case 112: /* expr17: T_REAL  */
#line 192 "snobol4.y"
                                                                                                  { tree_t*e=ast_node_new(TT_FLIT);   e->v.dval=(yyvsp[0].tok).dval;(yyval.expr)=e; }
#line 1981 "snobol4.tab.c"
    break;


#line 1985 "snobol4.tab.c"

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
  yytoken = yychar == SNOBOL4_EMPTY ? YYSYMBOL_YYEMPTY : YYTRANSLATE (yychar);
  /* If not already recovering from an error, report this error.  */
  if (!yyerrstatus)
    {
      ++yynerrs;
      yyerror (yyparse_param, YY_("syntax error"));
    }

  if (yyerrstatus == 3)
    {
      /* If just tried and failed to reuse lookahead token after an
         error, discard it.  */

      if (yychar <= SNOBOL4_EOF)
        {
          /* Return failure if at end of input.  */
          if (yychar == SNOBOL4_EOF)
            YYABORT;
        }
      else
        {
          yydestruct ("Error: discarding",
                      yytoken, &yylval, yyparse_param);
          yychar = SNOBOL4_EMPTY;
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
                  YY_ACCESSING_SYMBOL (yystate), yyvsp, yyparse_param);
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
  yyerror (yyparse_param, YY_("memory exhausted"));
  yyresult = 2;
  goto yyreturnlab;


/*----------------------------------------------------------.
| yyreturnlab -- parsing is finished, clean up and return.  |
`----------------------------------------------------------*/
yyreturnlab:
  if (yychar != SNOBOL4_EMPTY)
    {
      /* Make sure we have latest lookahead translation.  See comments at
         user semantic actions for why this is necessary.  */
      yytoken = YYTRANSLATE (yychar);
      yydestruct ("Cleanup: discarding lookahead",
                  yytoken, &yylval, yyparse_param);
    }
  /* Do not reclaim the symbols of the rule whose action triggered
     this YYABORT or YYACCEPT.  */
  YYPOPSTACK (yylen);
  YY_STACK_PRINT (yyss, yyssp);
  while (yyssp != yyss)
    {
      yydestruct ("Cleanup: popping",
                  YY_ACCESSING_SYMBOL (+*yyssp), yyvsp, yyparse_param);
      YYPOPSTACK (1);
    }
#ifndef yyoverflow
  if (yyss != yyssa)
    YYSTACK_FREE (yyss);
#endif

  return yyresult;
}

#line 194 "snobol4.y"

/*----------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------*/
int snobol4_lex(YYSTYPE *yylval_param, void *yyparse_param) {
    (void)yyparse_param; Token t=lex_next(g_lx); yylval_param->tok=t;
    if (getenv("SNO_TOK_TRACE"))
        fprintf(stderr,"[TOK %d sval=%s ival=%ld]\n",t.kind,t.sval?t.sval:"",t.ival);
    return t.kind;
}
/*----------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------*/
void snobol4_error(void *p,const char *msg){(void)p;sno_error(g_lx?g_lx->lineno:0,"parse error: %s",msg);}
/*----------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------*/
static void fixup_val(tree_t *e){ (void)e; }
/*----------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------*/
static int is_pat(tree_t *e){
    if(!e) return 0;
    switch(e->t){case TT_ARB:case TT_ARBNO:case TT_CAPT_COND_ASGN:case TT_CAPT_IMMED_ASGN:case TT_CAPT_CURSOR:case TT_DEFER:return 1;default:break;}
    for(int i=0;i<e->n;i++) if(is_pat(e->c[i])) return 1;
    return 0;
}
/*----------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------*/
static void sno4_stmt_commit_go(void *param,Token lbl,tree_t *subj,tree_t *pat,int has_eq,tree_t *repl,tree_t *gu,tree_t *gs,tree_t *gf){
    PP *pp=(PP*)param;
    /* PST-SN4-1a (2026-05-16): EXPORT/IMPORT special-case removed.  The parser
     * no longer interprets statements with label "EXPORT"/"IMPORT" specially.
     * They flow through as ordinary labeled statements with subject expressions,
     * exactly as the SNOBOL4 grammar describes them.  No downstream consumer
     * reads prog->exports / prog->imports today (only code_free walks them, to
     * free).  If a future feature needs cross-language link information, it
     * lives as a post-parse pass over the tree_t / TT_PROGRAM. */
    STMT_t *s=stmt_new();
    s->lineno = lbl.lineno ? lbl.lineno : snobol4_get_stmt_lineno();
    s->stno = ++pp->prog->nstmts;
    if(lbl.sval){s->label=strdup(lbl.sval);s->is_end=lbl.ival||(strcmp(lbl.sval,"END")==0);}
    /* PST-SN4-1b (2026-05-16): TT_SCAN-unpacking and TT_SEQ-splitting removed.
     * Parser emits pure syntax tree; lower.c performs the split. */
    s->subject=subj; s->pattern=pat;
    if(s->subject) fixup_val(s->subject);
    if(has_eq){s->has_eq=1;s->replacement=repl;if(repl&&!is_pat(repl))fixup_val(repl);}
    if(gu){ if(gu->t==TT_QLIT) s->goto_u=gu->v.sval; else s->goto_u_expr=gu; }
    if(gs){ if(gs->t==TT_QLIT) s->goto_s=gs->v.sval; else s->goto_s_expr=gs; }
    if(gf){ if(gf->t==TT_QLIT) s->goto_f=gf->v.sval; else s->goto_f_expr=gf; }
    if(!pp->prog->head) pp->prog->head=pp->prog->tail=s; else{pp->prog->tail->next=s;pp->prog->tail=s;}
    if (pp->ast_prog) {
        tree_t *anode = stmt_to_ast(s);
        if (pp->ast_prog->n >= pp->ast_prog->_nalloc) {
            pp->ast_prog->_nalloc = pp->ast_prog->_nalloc ? pp->ast_prog->_nalloc * 2 : 64;
            pp->ast_prog->c = realloc(pp->ast_prog->c,
                (size_t)pp->ast_prog->_nalloc * sizeof(tree_t*));
        }
        pp->ast_prog->c[pp->ast_prog->n++] = anode;
    }
}
/*----------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------*/
static tree_t *parse_expr(Lex *lx){
    CODE_t *prog=calloc(1,sizeof*prog);PP p={prog,NULL,NULL};g_lx=lx;snobol4_parse(&p);
    return prog->head?prog->head->subject:NULL;
}
/*----------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------*/
CODE_t *parse_program_tokens(Lex *stream){
    CODE_t *prog=calloc(1,sizeof*prog);PP p={prog,NULL,NULL};g_lx=stream;snobol4_parse(&p);return prog;
}
/*----------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------*/
CODE_t *parse_program_tokens_ast(Lex *stream, tree_t **ast_out){
    CODE_t *prog=calloc(1,sizeof*prog);
    tree_t *ast=calloc(1,sizeof*ast); ast->t=TT_PROGRAM;
    PP p={prog,NULL,ast};g_lx=stream;snobol4_parse(&p);
    *ast_out=ast;
    return prog;
}
/*----------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------*/
CODE_t *parse_program(LineArray *lines){(void)lines;return calloc(1,sizeof(CODE_t));}
/*----------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------*/
tree_t *parse_expr_from_str(const char *src){
    if(!src||!*src) return NULL;Lex lx={0};lex_open_str(&lx,src,(int)strlen(src),0);return parse_expr(&lx);
}
/*----------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------*/
tree_t *parse_expr_pat_from_str(const char *src) {
    if (!src || !*src) return NULL;
    int slen = (int)strlen(src);
    char *buf = malloc(slen + 2);
    if (!buf) return NULL;
    memcpy(buf, src, slen);
    buf[slen]   = '\n';
    buf[slen+1] = '\0';
    Lex lx = {0};
    lex_open_str(&lx, buf, slen + 1, 0);
    CODE_t *prog = calloc(1, sizeof(CODE_t));
    PP p = {prog, NULL, NULL};
    g_lx = &lx;
    snobol4_parse(&p);
    free(buf);
    if (p.ast_prog && p.ast_prog->n > 0) {
        const tree_t *s = p.ast_prog->c[0];
        if (s) {
            tree_t *pat = stmt_attr_expr(stmt_attr_find(s, ":pat"));
            if (pat) { free(prog); return pat; }
            return stmt_attr_expr(stmt_attr_find(s, ":subj"));
        }
    }
    if (!prog->head) { free(prog); return NULL; }
    STMT_t *s = prog->head;
    tree_t *res = s->pattern ? s->pattern : s->subject;
    free(prog);
    return res;
}
/*----------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------*/
CODE_t *sno_parse_string(const char *src) {
    if (!src) return calloc(1, sizeof(CODE_t));
    int slen = (int)strlen(src);
    char *buf = malloc(slen + 2);
    if (!buf) return calloc(1, sizeof(CODE_t));
    memcpy(buf, src, slen);
    buf[slen]   = '\n';
    buf[slen+1] = '\0';
    Lex lx = {0};
    lex_open_str_initial(&lx, buf, slen + 1, 0);
    CODE_t *prog = calloc(1, sizeof(CODE_t));
    PP p = {prog, NULL, NULL};
    g_lx = &lx;
    snobol4_parse(&p);
    free(buf);
    return prog;
}
/*----------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------*/
tree_t *sno_parse_string_ast(const char *src, CODE_t **code_out) {
    if (!src) { if (code_out) *code_out = calloc(1, sizeof(CODE_t)); return NULL; }
    int slen = (int)strlen(src);
    char *buf = malloc(slen + 2);
    if (!buf) { if (code_out) *code_out = calloc(1, sizeof(CODE_t)); return NULL; }
    memcpy(buf, src, slen);
    buf[slen]   = '\n';
    buf[slen+1] = '\0';
    Lex lx = {0};
    lex_open_str_initial(&lx, buf, slen + 1, 0);
    CODE_t *prog = calloc(1, sizeof(CODE_t));
    tree_t  *ast  = NULL;
    PP p = {prog, NULL, NULL};
    g_lx = &lx;
    snobol4_parse(&p);
    ast = p.ast_prog;
    free(buf);
    if (code_out) *code_out = prog; else free(prog);
    return ast;
}
