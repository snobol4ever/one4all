/*
 * ir.h — Unified Intermediate Representation
 *
 * THE single source of truth for all IR node kinds across all frontends
 * and all backends in scrip-cc. Every frontend lowers to tree_t nodes
 * using this tree_e enum. Every backend consumes it.
 *
 * 59 canonical node kinds:
 *   5  Literals
 *   4  References
 *   7  Arithmetic
 *   3  Sequence / Alternation
 *   14 Pattern Primitives  (each has distinct Byrd box wiring)
 *   3  Captures
 *   5  Call / Access / Scan / Swap
 *   7  Icon Generators + Constructors
 *   6  Prolog
 *
 * Name heritage: E_ prefix = Expression node. Names derived from SIL
 * v311.sil xxxTYP token type codes (CSNOBOL4 2.3.3) where applicable.
 * See archive/doc/ARCH-sil-heritage.md for full lineage.
 * See archive/doc/IR_AUDIT.md for node-by-node Byrd box wiring notes.
 * See archive/doc/SIL_NAMES_AUDIT.md for broader naming law.
 *
 * DO NOT add new node kinds here without Lon's explicit approval.
 * Protocol: evidence from emitter code that a distinct node is needed,
 * then add to this enum, then update all four backends.
 *
 * Produced by: Claude Sonnet 4.6 (G-7 session, 2026-03-28)
 * Milestone: M-G1-IR-HEADER-DEF
 */

#ifndef TREE_H
#define TREE_H

#ifdef __cplusplus
extern "C" {
#endif

/* =========================================================================
 * tree_e — unified expression node kind enum
 * ========================================================================= */

typedef enum tree_e {

    /* --- Literals -------------------------------------------------------- */

    TT_QLIT,         /* Quoted string / pattern literal  (QLITYP=1 in SIL)   */
    TT_ILIT,         /* Integer literal                  (ILITYP=2 in SIL)   */
    TT_FLIT,         /* Float / real literal             (FLITYP=6 in SIL)   */
    TT_CSET,         /* Cset literal (Icon / Rebus)                          */
    TT_NUL,          /* Null / empty value               (was AST_NULV)         */

    /* --- References ------------------------------------------------------ */

    TT_VAR,          /* Variable reference               (VARTYP=3; was AST_VART) */
    TT_KEYWORD,           /* &IDENT keyword reference         (K=10 data type)     */
    TT_INDIRECT,         /* $expr  indirect / imm-assign target                  */
    TT_DEFER,        /* *expr  deferred / indirect pattern ref (XSTAR=32; was AST_STAR) */

    /* --- Arithmetic ------------------------------------------------------ */

    TT_INTERROGATE,  /* ?X  interrogation: null if X succeeds, fail if X fails  (o$int)  */
    TT_NAME,         /* .X  name reference: return name descriptor of X         (o$nam)  */
    TT_MNS,          /* Unary minus          (MNS proc in SIL; o$com in MINIMAL) */
    TT_PLS,          /* Unary plus / numeric coerce  (PLS proc in SIL; o$aff in MINIMAL) */
    TT_ADD,          /* Addition                                              */
    TT_SUB,          /* Subtraction                                           */
    TT_MUL,          /* Multiplication                                        */
    TT_DIV,          /* Division                                              */
    TT_MOD,          /* Modulo / remainder                                    */
    TT_POW,          /* Exponentiation       (EXR in SIL; was AST_EXPOP)        */

    /* --- Sequence and Alternation ---------------------------------------- */

    TT_SEQ,          /* Goal-directed sequence, n-ary — Byrd-box wiring       */
                    /* α→lα, lγ→rα, rω→lβ, rγ→γ. SNOBOL4 pattern CAT;      */
                    /* Icon ||/;/&/loop bodies. (CONCAT/CONCL in SIL)          */
    TT_CAT,       /* Pure value-context string concat, n-ary, cannot fail  */
                    /* SNOBOL4 value ctx; JVM StringBuilder; .NET Concat.     */
                    /* M-G4-SPLIT-SEQ-CONCAT (2026-03-28).                   */
    TT_ALT,          /* Pattern alternation, n-ary (ORPP in SIL; was AST_OR)   */
    TT_VLIST,        /* Goal-directed value-context disjunction.  N-ary.     */
                    /* Try children left-to-right; return first non-failing  */
                    /* value; fail if all fail.  SPITBOL `(a, b, c)`         */
                    /* paren-list and Snocone `||`.  Distinct from TT_ALT     */
                    /* (pattern alternation, lazy at match time).            */
    TT_OPSYN,        /* & operator: reduce(left, right)                      */

    /* --- Pattern Primitives ---------------------------------------------- */
    /*
     * Each primitive has distinct Byrd box wiring in emit_byrd_asm.c.
     * SIL X___ codes from equ.h confirm each is a separate dispatch case.
     * All 14 are required; none can be merged without emitter evidence.
     */

    TT_ARB,          /* Arbitrary match           (XFARB=17; p$arb)          */
    TT_ARBNO,        /* Zero-or-more              (XARBN=3;  p$arb)          */
    TT_POS,          /* Cursor assert POS(n)      (XPOSI=24)                 */
    TT_RPOS,         /* Right cursor RPOS(n)      (XRPSI=25)                 */
    TT_ANY,          /* ANY(S) — match one from S (XANYC=1;  p$any)          */
    TT_NOTANY,       /* NOTANY(S) — match one not in S  (XNNYC=21)           */
    TT_SPAN,         /* SPAN(S) — longest run from S    (XSPNC=31; p$spn)    */
    TT_BREAK,        /* BREAK(S) — up to char in S      (XBRKC=8;  p$brk)    */
    TT_BREAKX,       /* BREAKX(S) — BREAK + backtrack   (XBRKX=9;  p$bkx)    */
    TT_LEN,          /* LEN(N) — exactly N chars        (XLNTH=19; p$len)    */
    TT_TAB,          /* TAB(N) — to cursor pos N        (XTB=33;   p$tab)    */
    TT_RTAB,         /* RTAB(N) — to N from right       (XRTB=26;  p$rtb)    */
    TT_REM,          /* REM — remainder of subject      (p$rem)              */
    TT_FAIL,         /* FAIL — always fail              (XFAIL=27; p$fal)    */
    TT_SUCCEED,      /* SUCCEED — always succeed        (XSUCF=36; p$suc)    */
    TT_FENCE,        /* FENCE — succeed, seal β         (XFNCE=35)           */
    TT_ABORT,        /* ABORT — abort entire match                           */
    TT_BAL,          /* BAL — balanced parentheses      (XBAL=6;   p$bal)    */

    /* --- Captures -------------------------------------------------------- */

    TT_CAPT_COND_ASGN,    /* .var  conditional capture (on success) (was AST_NAM)   */
    TT_CAPT_IMMED_ASGN,     /* $var  immediate capture               (was AST_DOL)    */
    TT_CAPT_CURSOR,     /* @var  cursor position capture (XATP=4; was AST_ATP)    */

    /* --- Call, Access, Assignment, Scan, Swap ---------------------------- */

    TT_FNC,          /* Function call / goal / builtin, n-ary (FNCTYP=5)     */
    TT_IDX,          /* Array / table / record subscript (ARYTYP=7; absorbs AST_ARY) */
    TT_ASSIGN,       /* Assignment  (ASGN proc in SIL; was AST_ASGN)           */
    TT_SCAN,        /* E ? E  scanning  (XSCON=30/SCONCL; was TT_SCAN)       */
    TT_SWAP,         /* :=:  swap bindings  (SWAP proc in SIL)               */

    /* --- Icon Generators and Constructors -------------------------------- */

    TT_SUSPEND,      /* Generator suspend / yield                            */
    TT_TO,           /* i to j  generator                                    */
    TT_TO_BY,        /* i to j by k  generator                               */
    TT_LIMIT,        /* E \ N  limitation                                    */
    TT_ALTERNATE,       /* Icon / Rebus alt generator, left-then-right (was AST_ALT_GEN) */
    TT_ITERATE,         /* !E  iterate list or string elements (was AST_BANG)     */
    TT_MAKELIST,     /* [e1,e2,...]  list constructor                        */

    /* --- Prolog ---------------------------------------------------------- */

    TT_UNIFY,        /* =/2  unification with trail                          */
    TT_CLAUSE,       /* Horn clause: head + body + EnvLayout                 */
    TT_CHOICE,       /* Predicate choice point: α/β chain over clauses       */
    TT_CUT,          /* !  cut / FENCE — seals β of enclosing choice         */
    TT_TRAIL_MARK,   /* Save trail.top into env slot                         */
    TT_TRAIL_UNWIND, /* Restore trail to saved mark                          */

    /* --- Icon: Numeric Relational ----------------------------------------
     * Goal-directed: succeed and yield rhs if condition holds, else fail.
     * Six distinct Byrd-box wiring patterns (each distinct comparison jump).
     * No SNOBOL4/Prolog equivalent — those use TT_FNC("lt",...) dispatch.
     * M-G9-ICON-IR-WIRE (2026-03-30). */

    TT_LT,           /* E1 < E2   (numeric less-than)                        */
    TT_LE,           /* E1 <= E2  (numeric less-or-equal)                    */
    TT_GT,           /* E1 > E2   (numeric greater-than)                     */
    TT_GE,           /* E1 >= E2  (numeric greater-or-equal)                 */
    TT_EQ,           /* E1 = E2   (numeric equality)                         */
    TT_NE,           /* E1 ~= E2  (numeric not-equal)                        */

    /* --- Icon: Lexicographic (String) Relational -------------------------
     * L prefix = Lexicographic. Goal-directed semantics on string values.
     * TT_L{LT,LE,GT,GE,EQ,NE} — parallel to E_{LT,LE,GT,GE} numeric relops. */

    TT_LLT,          /* E1 << E2  (string less-than)                         */
    TT_LLE,          /* E1 <<= E2 (string less-or-equal)                     */
    TT_LGT,          /* E1 >> E2  (string greater-than)                      */
    TT_LGE,          /* E1 >>= E2 (string greater-or-equal)                  */
    TT_LEQ,         /* E1 == E2  (string equality;  ICN_SEQ)                */
    TT_LNE,          /* E1 ~== E2 (string not-equal)                         */

    /* --- Icon: Cset Operators -------------------------------------------- */

    TT_CSET_COMPL,   /* ~E       cset complement                             */
    TT_CSET_UNION,   /* E1 ++ E2 cset union                                  */
    TT_CSET_DIFF,    /* E1 -- E2 cset difference                             */
    TT_CSET_INTER,   /* E1 ** E2 cset intersection                           */
    TT_LCONCAT,      /* E1 ||| E2  list concatenation (distinct from || str) */

    /* --- Icon: Unary Operators ------------------------------------------- */

    TT_NONNULL,      /* \E   succeed if E non-null, yield E's value          */
    TT_NULL,         /* /E   succeed if E is null, yield &null               */
    TT_NOT,          /* not E  succeed iff E fails                           */
    TT_SIZE,         /* *E   size of string/list/table                       */
    TT_RANDOM,       /* ?E   random element or integer in [1,E]              */
    TT_IDENTICAL,    /* E1 === E2  object identity (same pointer)            */
    TT_AUGOP,        /* E1 op:= E2  augmented assignment; op subtype in ival */

    /* --- Icon: Expression Sequence / Control Flow ------------------------ */

    TT_SEQ_EXPR,     /* (E1; E2; ...; En) — evaluate all, result = last     */
    TT_EVERY,        /* every E [do body]  — drive generator to exhaustion  */
    TT_WHILE,        /* while E [do body]                                    */
    TT_UNTIL,        /* until E [do body]                                    */
    TT_REPEAT,       /* repeat body        — unconditional loop              */
    TT_IF,           /* if E then E2 [else E3]                               */
    TT_CASE,         /* case E of { clauses }                                */
    TT_RETURN,       /* return [E]         — return from procedure           */
    TT_PROC_FAIL,    /* fail               — fail-return from procedure (Icon/Raku)
                     * NOTE: distinct from TT_FAIL = SNOBOL4 FAIL pattern primitive */
    TT_LOOP_BREAK,   /* break [E]          — exit innermost loop
                     * NOTE: distinct from TT_BREAK = SNOBOL4 BREAK(S)      */
    TT_LOOP_NEXT,    /* next               — restart innermost loop          */
    TT_BANG_BINARY,  /* E1 ! E2            — invoke E1 with list E2         */

    /* --- Icon: Structure / Declarations ---------------------------------- */

    TT_SECTION,      /* E[i:j]   string section                              */
    TT_SECTION_PLUS, /* E[i+:n]  section by length (forward)                */
    TT_SECTION_MINUS,/* E[i-:n]  section by length (backward)               */
    TT_RECORD,       /* record declaration                                   */
    TT_FIELD,        /* E.name   field access                                */
    TT_GLOBAL,       /* global varname  declaration                          */
    TT_INITIAL,      /* initial { body }  once-on-first-call block          */
    TT_REVASSIGN,    /* E1 <- E2  reversible assignment (Icon)               */
    TT_REVSWAP,      /* E1 <-> E2 reversible value swap (Icon)               */

    /* --- Program structure (SI-1, Phase 5) --------------------------------
     * These replace CODE_t / STMT_t once all frontends emit them directly.
     * Until SI-6, the shim helpers stmt_to_ast / code_to_ast produce these
     * from the old structs; lower() and lower_stmt() consume them.
     *
     * Pure tree shape — four logical fields per node: t(kind) v(sval/ival/dval)
     * n(nchildren) c(children[]).  Matches Snocone `tree` datatype exactly.
     *
     * TT_PROGRAM  kind=TT_PROGRAM  v=""  children = TT_STMT/TT_END nodes
     *
     * TT_STMT     kind=TT_STMT  v=""  children = tagged attribute nodes:
     *   tree(':lbl',  label_str)          — label (omitted if none)
     *   tree(':lang', lang_int_as_str)    — lang code (omitted if LANG_SNO=0)
     *   tree(':line', lineno_str)         — source line number
     *   tree(':stno', stno_str)           — source statement number
     *   tree(':subj', subject_expr)       — subject (omitted if absent)
     *   tree(':pat',  pattern_expr)       — pattern (omitted if absent)
     *   tree(':eq',   '')                 — presence signals has_eq=true
     *   tree(':repl', repl_expr_or_NUL)   — replacement (omitted if !has_eq)
     *   tree(':goS',  label_or_expr)      — success goto (omitted if absent)
     *   tree(':goF',  label_or_expr)      — failure goto (omitted if absent)
     *   tree(':go',   label_or_expr)      — unconditional goto (omitted if absent)
     *
     * TT_END      kind=TT_END  v=""  children: [:lbl] [:line] [:stno]
     *
     * Attribute tag kinds (sval = the tag string, v = payload or child):
     *   ':lbl' ':lang' ':line' ':stno' ':subj' ':pat' ':eq' ':repl'
     *   ':goS' ':goF' ':go'
     * Each tag node: kind=TT_ATTR, sval=tag_name, nchildren=0 (leaf with
     * sval payload) or nchildren=1 (tree payload in children[0]).
     *
     * Matches parser_snobol4.sc STMT shape byte-for-byte.
     * ----------------------------------------------------------------------- */
    TT_PROGRAM,
    TT_STMT,
    TT_END,      /* END statement — structurally distinct from TT_STMT    */
    TT_ATTR,     /* attribute tag node: sval=":lbl"/":subj"/etc.           */
    TT_GOTO_S,   /* kept for lower_stmt goto arm compat during SI-3..SI-5 */
    TT_GOTO_F,
    TT_GOTO_U,

    /* --- Sentinel -------------------------------------------------------- */

    TT_KIND_COUNT    /* Total number of kinds — used for array sizing / asserts.
                     * NOT a valid node kind. Must remain last. */

} tree_e;

/* =========================================================================
 * AugOp_e — augmented-assignment operator codes (SR-9)
 *
 * Written into TT_AUGOP.v.ival by the Icon frontend (icon_parse.c).
 * lower.c / lower_icn_unary.c reads these values without including
 * the frontend's icon_lex.h — eliminating the mid-function #include.
 *
 * The numeric values are stable; never reorder them.
 * ========================================================================= */
typedef enum {
    AUGOP_ADD       = 1,  /* +:=   */
    AUGOP_SUB       = 2,  /* -:=   */
    AUGOP_MUL       = 3,  /* *:=   */
    AUGOP_DIV       = 4,  /* /:=   */
    AUGOP_MOD       = 5,  /* %:=   */
    AUGOP_POW       = 6,  /* ^:=   */
    AUGOP_CONCAT    = 7,  /* ||:=  */
    AUGOP_CSET_UNION  = 8,  /* ++:=  */
    AUGOP_CSET_DIFF   = 9,  /* --:=  */
    AUGOP_CSET_INTER  = 10, /* **:=  */
    AUGOP_SCAN      = 11, /* ?:=   */
    AUGOP_EQ        = 12, /* =:=   */
    AUGOP_SEQ       = 13, /* ==:=  */
    AUGOP_LT        = 14, /* <:=   */
    AUGOP_LE        = 15, /* <=:=  */
    AUGOP_GT        = 16, /* >:=   */
    AUGOP_GE        = 17, /* >=:=  */
    AUGOP_NE        = 18, /* ~=:=  */
    AUGOP_SLT       = 19, /* <<:=  */
    AUGOP_SLE       = 20, /* <<=:= */
    AUGOP_SGT       = 21, /* >>:=  */
    AUGOP_SGE       = 22, /* >>=:= */
    AUGOP_SNE       = 23, /* ~==:= */
} AugOp_e;

/* =========================================================================
 * tree_t — the canonical IR node type.
 *
 * Matches the Snocone `tree` datatype exactly: four logical fields t/v/n/c.
 *
 *   t  — kind       (tree_e)
 *   v  — value      (union: v.sval / v.ival / v.dval — active by kind)
 *   n  — nchildren  (int, number of valid children)
 *   c  — children[] (tree_t **, realloc array that grows and shrinks)
 *
 * v field by kind:
 *   v.sval — TT_QLIT (text), TT_VAR/TT_KEYWORD/TT_FNC/TT_IDX (name),
 *             TT_CSET (chars), TT_ATTR (tag string)
 *   v.ival — TT_ILIT (literal); TT_VAR (frame-slot index after Icon scope
 *             analysis; v.sval still holds name); TT_AUGOP (AugOp_e);
 *             TT_GLOBAL (declared-global flag)
 *   v.dval — TT_FLIT (float literal)
 *
 * C implementation details (not logical tree fields, underscore-prefixed):
 *   _nalloc — allocated capacity of c[] for realloc bookkeeping
 *   _id     — node identity for INITIAL block dedup (emit-time only)
 *
 * ast.h is the sole owner of this definition (FI-0A).
 * ========================================================================= */
typedef struct tree_t tree_t;

struct tree_t {
    tree_e    t;         /* kind                                              */
    union {
        char     *sval; /* string value (QLIT/VAR/FNC/KEYWORD/ATTR/CSET)    */
        long long ival; /* integer value (ILIT) or slot/flag (VAR etc.)     */
        double   dval;  /* float value (FLIT)                               */
    } v;
    int       n;        /* nchildren — number of valid children              */
    tree_t  **c;        /* children[] — realloc-grown/shrunk array           */
    /* C implementation details: */
    int      _nalloc;   /* allocated capacity of c[]                         */
    int      _id;       /* node id for INITIAL dedup (emit-time only)        */
};

/* =========================================================================
 * ast_push / ast_pop / ast_node_new
 *
 * ast_push: append child; c[] doubles when full.
 * ast_pop:  remove last child; c[] halves when n < _nalloc/4; frees when empty.
 * ast_node_new:  allocate a zeroed node with kind t.
 *
 * These match the Snocone push_child / pop_child / tree contract exactly.
 * ========================================================================= */
#include <stdlib.h>

static inline void ast_push(tree_t *p, tree_t *child) {
    if (p->n >= p->_nalloc) {
        p->_nalloc = p->_nalloc ? p->_nalloc * 2 : 4;
        p->c = (tree_t **)realloc(p->c, (size_t)p->_nalloc * sizeof(tree_t *));
    }
    p->c[p->n++] = child;
}

static inline tree_t *ast_pop(tree_t *p) {
    if (p->n == 0) return NULL;
    tree_t *child = p->c[--p->n];
    if (p->n == 0) {
        free(p->c); p->c = NULL; p->_nalloc = 0;
    } else if (p->n < p->_nalloc / 4) {
        p->_nalloc /= 2;
        p->c = (tree_t **)realloc(p->c, (size_t)p->_nalloc * sizeof(tree_t *));
    }
    return child;
}

static inline tree_t *ast_node_new(tree_e kind) {
    tree_t *e = (tree_t *)calloc(1, sizeof(tree_t));
    e->t = kind;
    return e;
}

/* =========================================================================
 * tree_e name table — for ast_print.c and debugging
 * ========================================================================= */

#ifdef IR_DEFINE_NAMES

static const char * const tt_e_name[TT_KIND_COUNT] = {
    [TT_QLIT]         = "TT_QLIT",
    [TT_ILIT]         = "TT_ILIT",
    [TT_FLIT]         = "TT_FLIT",
    [TT_CSET]         = "TT_CSET",
    [TT_NUL]          = "TT_NUL",
    [TT_VAR]          = "TT_VAR",
    [TT_KEYWORD]           = "TT_KEYWORD",
    [TT_INDIRECT]         = "TT_INDIRECT",
    [TT_DEFER]        = "TT_DEFER",
    [TT_INTERROGATE]  = "TT_INTERROGATE",
    [TT_NAME]         = "TT_NAME",
    [TT_MNS]          = "TT_MNS",
    [TT_PLS]          = "TT_PLS",
    [TT_ADD]          = "TT_ADD",
    [TT_SUB]          = "TT_SUB",
    [TT_MUL]          = "TT_MUL",
    [TT_DIV]          = "TT_DIV",
    [TT_MOD]          = "TT_MOD",
    [TT_POW]          = "TT_POW",
    [TT_SEQ]          = "TT_SEQ",
    [TT_CAT]       = "TT_CAT",
    [TT_ALT]          = "TT_ALT",
    [TT_VLIST]        = "TT_VLIST",
    [TT_OPSYN]        = "TT_OPSYN",
    [TT_ARB]          = "TT_ARB",
    [TT_ARBNO]        = "TT_ARBNO",
    [TT_POS]          = "TT_POS",
    [TT_RPOS]         = "TT_RPOS",
    [TT_ANY]          = "TT_ANY",
    [TT_NOTANY]       = "TT_NOTANY",
    [TT_SPAN]         = "TT_SPAN",
    [TT_BREAK]        = "TT_BREAK",
    [TT_BREAKX]       = "TT_BREAKX",
    [TT_LEN]          = "TT_LEN",
    [TT_TAB]          = "TT_TAB",
    [TT_RTAB]         = "TT_RTAB",
    [TT_REM]          = "TT_REM",
    [TT_FAIL]         = "TT_FAIL",
    [TT_SUCCEED]      = "TT_SUCCEED",
    [TT_FENCE]        = "TT_FENCE",
    [TT_ABORT]        = "TT_ABORT",
    [TT_BAL]          = "TT_BAL",
    [TT_CAPT_COND_ASGN]    = "TT_CAPT_COND_ASGN",
    [TT_CAPT_IMMED_ASGN]     = "TT_CAPT_IMMED_ASGN",
    [TT_CAPT_CURSOR]     = "TT_CAPT_CURSOR",
    [TT_FNC]          = "TT_FNC",
    [TT_IDX]          = "TT_IDX",
    [TT_ASSIGN]       = "TT_ASSIGN",
    [TT_SCAN]        = "TT_SCAN",
    [TT_SWAP]         = "TT_SWAP",
    [TT_SUSPEND]      = "TT_SUSPEND",
    [TT_TO]           = "TT_TO",
    [TT_TO_BY]        = "TT_TO_BY",
    [TT_LIMIT]        = "TT_LIMIT",
    [TT_ALTERNATE]       = "TT_ALTERNATE",
    [TT_ITERATE]         = "TT_ITERATE",
    [TT_MAKELIST]     = "TT_MAKELIST",
    [TT_UNIFY]        = "TT_UNIFY",
    [TT_CLAUSE]       = "TT_CLAUSE",
    [TT_CHOICE]       = "TT_CHOICE",
    [TT_CUT]          = "TT_CUT",
    [TT_TRAIL_MARK]   = "TT_TRAIL_MARK",
    [TT_TRAIL_UNWIND] = "TT_TRAIL_UNWIND",
    /* Icon numeric relational */
    [TT_LT]           = "TT_LT",
    [TT_LE]           = "TT_LE",
    [TT_GT]           = "TT_GT",
    [TT_GE]           = "TT_GE",
    [TT_EQ]           = "TT_EQ",
    [TT_NE]           = "TT_NE",
    /* Icon string relational */
    [TT_LLT]          = "TT_LLT",
    [TT_LLE]          = "TT_LLE",
    [TT_LGT]          = "TT_LGT",
    [TT_LGE]          = "TT_LGE",
    [TT_LEQ]         = "TT_LEQ",
    [TT_LNE]          = "TT_LNE",
    /* Icon cset ops */
    [TT_CSET_COMPL]   = "TT_CSET_COMPL",
    [TT_CSET_UNION]   = "TT_CSET_UNION",
    [TT_CSET_DIFF]    = "TT_CSET_DIFF",
    [TT_CSET_INTER]   = "TT_CSET_INTER",
    [TT_LCONCAT]      = "TT_LCONCAT",
    /* Icon unary */
    [TT_NONNULL]      = "TT_NONNULL",
    [TT_NULL]         = "TT_NULL",
    [TT_NOT]          = "TT_NOT",
    [TT_SIZE]         = "TT_SIZE",
    [TT_RANDOM]       = "TT_RANDOM",
    [TT_IDENTICAL]    = "TT_IDENTICAL",
    [TT_AUGOP]        = "TT_AUGOP",
    /* Icon control flow */
    [TT_SEQ_EXPR]     = "TT_SEQ_EXPR",
    [TT_EVERY]        = "TT_EVERY",
    [TT_WHILE]        = "TT_WHILE",
    [TT_UNTIL]        = "TT_UNTIL",
    [TT_REPEAT]       = "TT_REPEAT",
    [TT_IF]           = "TT_IF",
    [TT_CASE]         = "TT_CASE",
    [TT_RETURN]       = "TT_RETURN",
    [TT_PROC_FAIL]    = "TT_PROC_FAIL",
    [TT_LOOP_BREAK]   = "TT_LOOP_BREAK",
    [TT_LOOP_NEXT]    = "TT_LOOP_NEXT",
    [TT_BANG_BINARY]  = "TT_BANG_BINARY",
    /* Icon structure */
    [TT_SECTION]      = "TT_SECTION",
    [TT_SECTION_PLUS] = "TT_SECTION_PLUS",
    [TT_SECTION_MINUS]= "TT_SECTION_MINUS",
    [TT_RECORD]       = "TT_RECORD",
    [TT_FIELD]        = "TT_FIELD",
    [TT_GLOBAL]       = "TT_GLOBAL",
    [TT_INITIAL]      = "TT_INITIAL",
    [TT_REVASSIGN]    = "TT_REVASSIGN",
    [TT_REVSWAP]      = "TT_REVSWAP",
    [TT_PROGRAM]      = "TT_PROGRAM",
    [TT_STMT]         = "TT_STMT",
    [TT_END]          = "TT_END",
    [TT_ATTR]         = "TT_ATTR",
    [TT_GOTO_S]       = "TT_GOTO_S",
    [TT_GOTO_F]       = "TT_GOTO_F",
    [TT_GOTO_U]       = "TT_GOTO_U",
};

#endif /* IR_DEFINE_NAMES */



#ifdef __cplusplus
}
#endif

#endif /* AST_H */
