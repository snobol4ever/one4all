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
#line 69 "snocone_parse.y"

#include "scrip_cc.h"

/* Forward-declare LexCtx — defined in snocone_lex.h.  We forward-declare
 * here so this %code requires block (which lands in tab.h) stays minimal
 * and avoids an include cycle: the lexer's IMPLEMENTATION (snocone_lex.c)
 * includes tab.h to resolve T_* names, but the lexer's API header
 * (snocone_lex.h) deliberately does not, so callers can pass `int kind`
 * around without needing the parser's enum. */
struct LexCtx;

/* LS-4.f — control-flow handoff structs.  Built by if_head / while_head
 * non-terminals; consumed by sc_finalize_* in the parent rule's final
 * action.  Forward-declared here so the %union can reference them; the
 * full layout is defined in the epilogue alongside the helpers. */
struct IfHead;
struct WhileHead;
struct FuncHead;

/* LS-4.i.2 — LoopFrame: tracks one enclosing loop (or switch, in LS-4.i.3)
 * for the purpose of resolving break/continue.  Pushed onto a stack rooted
 * at ScParseState.loop_top when a loop's head fires (sc_while_head_new,
 * sc_do_head_new, sc_for_head_new); popped when the matching finalize_*
 * runs.  Each frame carries the synthetic labels the corresponding
 * finalize_* will use, so break/continue stmts emitted DURING body parsing
 * can target them by name.
 *
 * user_labels[] holds any user labels that immediately preceded the loop
 * (i.e. were in pending_user_labels when the head fired).  Stacked labels
 * (`a: b: while(...) {...}`) all attach — break a; and break b; both work,
 * naming the same loop.  Java-style.
 *
 * is_loop = 1 for while/do/for; LS-4.i.3 will add 0 for switch (continue
 * skips switch frames when looking for its target). */
typedef struct LoopFrame {
    char    *cont_label;          /* continue target (loop top or "Lcont" for for/do) */
    char    *end_label;           /* break target (loop end / switch end) */
    char   **user_labels;         /* names that the user attached (own'd, strdup'd) */
    int      user_labels_count;
    int      is_loop;             /* 1 = loop (continue legal); 0 = switch (LS-4.i.3) */
    /* LS-4.i.2 — usage flags: tracks whether the body actually emitted any
     * break/continue stmts targeting this frame.  Used by finalize_* to
     * decide whether to emit the Lcont pad (for/do/while) — keeping the
     * emit lazy preserves the LS-4.f/g lowering shapes for code that
     * doesn't use continue, and avoids dead label pads in the IR.  break
     * targets don't need a flag — the Lend pad is always emitted (it's
     * the loop's exit target regardless). */
    int      cont_used;
    struct LoopFrame *outer;      /* link toward outer scope; NULL at outermost */
} LoopFrame;

/* Parser state — passed to sc_parse() via %parse-param.  Carries the
 * FSM lexer context (the single producer of tokens), the code under
 * construction, and a small error counter.  Uses CODE_t (typedef alias
 * of CODE_t) for symmetry with AST_t — Snocone's parser produces
 * code, not just an expression. */
typedef struct ScParseState {
    struct LexCtx *ctx;
    CODE_t        *code;
    const char    *filename;
    int            nerrors;
    int            label_seq;     /* LS-4.f: synthetic label counter */
    char          *cur_func_name; /* LS-4.h: enclosing function name (NULL at top level) */
    /* LS-4.i.2 — break/continue support.
     *
     * pending_user_labels accumulates label names emitted by recent
     * label_decl reductions that have not yet been "consumed" by either
     * a non-control stmt commit (which clears them) or a loop/switch
     * head (which captures them onto a LoopFrame).  This lets
     * `outer: while(...) { ... break outer; ... }` resolve the break to
     * the labeled loop frame.
     *
     * stash_for_pending_labels — a one-slot temporary that the for_lead
     * non-terminal moves pending into BEFORE the for-loop's init expr
     * gets emitted (which would otherwise clear pending via sc_append_stmt
     * before sc_for_head_new runs).  for_head's action consumes the stash. */
    LoopFrame    *loop_top;
    char        **pending_user_labels;
    int           pending_user_labels_count;
    int           pending_user_labels_cap;
    char        **stash_for_pending_labels;
    int           stash_for_pending_labels_count;
    /* LS-4.i.3 — innermost-switch pointer.  Used by case_or_default_label
     * actions to find the SwitchHead that owns them.  Saved/restored on
     * SwitchHead.prev_switch for nested switches. */
    struct SwitchHead *cur_switch;
} ScParseState;

#line 146 "snocone_parse.tab.h"

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
#line 384 "snocone_parse.y"

    AST_t *expr;
    char   *str;
    long    ival;
    double  dval;
    /* LS-4.f — control-flow handoff types.  IfHead/WhileHead are
     * returned by the head-prefix non-terminals (if_head, while_head)
     * and consumed by the finalize-* helpers.  stmt_ptr is returned
     * by `else_keyword` to snapshot the linked-list cursor before
     * the else-branch begins, enabling splice-in-the-middle. */
    struct IfHead    *ifhead;
    struct WhileHead *whilehead;
    struct DoHead    *dohead;
    struct ForHead   *forhead;
    /* LS-4.h — function definition handoff type. */
    struct FuncHead  *funchead;
    /* LS-4.i.3 — switch/case/default handoff type. */
    struct SwitchHead *switchhead;
    STMT_t           *stmt_ptr;

#line 268 "snocone_parse.tab.h"

};
typedef union SC_STYPE SC_STYPE;
# define SC_STYPE_IS_TRIVIAL 1
# define SC_STYPE_IS_DECLARED 1
#endif




int sc_parse (ScParseState *st);


#endif /* !YY_SC_SNOCONE_PARSE_TAB_H_INCLUDED  */
