/*
 * ir.h — Unified Intermediate Representation
 *
 * THE single source of truth for all IR node kinds across all frontends
 * and all backends in scrip-cc. Every frontend lowers to AST_t nodes
 * using this AST_e enum. Every backend consumes it.
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

#ifndef IR_H
#define IR_H

#ifdef __cplusplus
extern "C" {
#endif

/* =========================================================================
 * AST_e — unified expression node kind enum
 * ========================================================================= */

typedef enum AST_e {

    /* --- Literals -------------------------------------------------------- */

    AST_QLIT,         /* Quoted string / pattern literal  (QLITYP=1 in SIL)   */
    AST_ILIT,         /* Integer literal                  (ILITYP=2 in SIL)   */
    AST_FLIT,         /* Float / real literal             (FLITYP=6 in SIL)   */
    AST_CSET,         /* Cset literal (Icon / Rebus)                          */
    AST_NUL,          /* Null / empty value               (was AST_NULV)         */

    /* --- References ------------------------------------------------------ */

    AST_VAR,          /* Variable reference               (VARTYP=3; was AST_VART) */
    AST_KEYWORD,           /* &IDENT keyword reference         (K=10 data type)     */
    AST_INDIRECT,         /* $expr  indirect / imm-assign target                  */
    AST_DEFER,        /* *expr  deferred / indirect pattern ref (XSTAR=32; was AST_STAR) */

    /* --- Arithmetic ------------------------------------------------------ */

    AST_INTERROGATE,  /* ?X  interrogation: null if X succeeds, fail if X fails  (o$int)  */
    AST_NAME,         /* .X  name reference: return name descriptor of X         (o$nam)  */
    AST_MNS,          /* Unary minus          (MNS proc in SIL; o$com in MINIMAL) */
    AST_PLS,          /* Unary plus / numeric coerce  (PLS proc in SIL; o$aff in MINIMAL) */
    AST_ADD,          /* Addition                                              */
    AST_SUB,          /* Subtraction                                           */
    AST_MUL,          /* Multiplication                                        */
    AST_DIV,          /* Division                                              */
    AST_MOD,          /* Modulo / remainder                                    */
    AST_POW,          /* Exponentiation       (EXR in SIL; was AST_EXPOP)        */

    /* --- Sequence and Alternation ---------------------------------------- */

    AST_SEQ,          /* Goal-directed sequence, n-ary — Byrd-box wiring       */
                    /* α→lα, lγ→rα, rω→lβ, rγ→γ. SNOBOL4 pattern CAT;      */
                    /* Icon ||/;/&/loop bodies. (CONCAT/CONCL in SIL)          */
    AST_CAT,       /* Pure value-context string concat, n-ary, cannot fail  */
                    /* SNOBOL4 value ctx; JVM StringBuilder; .NET Concat.     */
                    /* M-G4-SPLIT-SEQ-CONCAT (2026-03-28).                   */
    AST_ALT,          /* Pattern alternation, n-ary (ORPP in SIL; was AST_OR)   */
    AST_VLIST,        /* Goal-directed value-context disjunction.  N-ary.     */
                    /* Try children left-to-right; return first non-failing  */
                    /* value; fail if all fail.  SPITBOL `(a, b, c)`         */
                    /* paren-list and Snocone `||`.  Distinct from AST_ALT     */
                    /* (pattern alternation, lazy at match time).            */
    AST_OPSYN,        /* & operator: reduce(left, right)                      */

    /* --- Pattern Primitives ---------------------------------------------- */
    /*
     * Each primitive has distinct Byrd box wiring in emit_byrd_asm.c.
     * SIL X___ codes from equ.h confirm each is a separate dispatch case.
     * All 14 are required; none can be merged without emitter evidence.
     */

    AST_ARB,          /* Arbitrary match           (XFARB=17; p$arb)          */
    AST_ARBNO,        /* Zero-or-more              (XARBN=3;  p$arb)          */
    AST_POS,          /* Cursor assert POS(n)      (XPOSI=24)                 */
    AST_RPOS,         /* Right cursor RPOS(n)      (XRPSI=25)                 */
    AST_ANY,          /* ANY(S) — match one from S (XANYC=1;  p$any)          */
    AST_NOTANY,       /* NOTANY(S) — match one not in S  (XNNYC=21)           */
    AST_SPAN,         /* SPAN(S) — longest run from S    (XSPNC=31; p$spn)    */
    AST_BREAK,        /* BREAK(S) — up to char in S      (XBRKC=8;  p$brk)    */
    AST_BREAKX,       /* BREAKX(S) — BREAK + backtrack   (XBRKX=9;  p$bkx)    */
    AST_LEN,          /* LEN(N) — exactly N chars        (XLNTH=19; p$len)    */
    AST_TAB,          /* TAB(N) — to cursor pos N        (XTB=33;   p$tab)    */
    AST_RTAB,         /* RTAB(N) — to N from right       (XRTB=26;  p$rtb)    */
    AST_REM,          /* REM — remainder of subject      (p$rem)              */
    AST_FAIL,         /* FAIL — always fail              (XFAIL=27; p$fal)    */
    AST_SUCCEED,      /* SUCCEED — always succeed        (XSUCF=36; p$suc)    */
    AST_FENCE,        /* FENCE — succeed, seal β         (XFNCE=35)           */
    AST_ABORT,        /* ABORT — abort entire match                           */
    AST_BAL,          /* BAL — balanced parentheses      (XBAL=6;   p$bal)    */

    /* --- Captures -------------------------------------------------------- */

    AST_CAPT_COND_ASGN,    /* .var  conditional capture (on success) (was AST_NAM)   */
    AST_CAPT_IMMED_ASGN,     /* $var  immediate capture               (was AST_DOL)    */
    AST_CAPT_CURSOR,     /* @var  cursor position capture (XATP=4; was AST_ATP)    */

    /* --- Call, Access, Assignment, Scan, Swap ---------------------------- */

    AST_FNC,          /* Function call / goal / builtin, n-ary (FNCTYP=5)     */
    AST_IDX,          /* Array / table / record subscript (ARYTYP=7; absorbs AST_ARY) */
    AST_ASSIGN,       /* Assignment  (ASGN proc in SIL; was AST_ASGN)           */
    AST_SCAN,        /* E ? E  scanning  (XSCON=30/SCONCL; was AST_SCAN)       */
    AST_SWAP,         /* :=:  swap bindings  (SWAP proc in SIL)               */

    /* --- Icon Generators and Constructors -------------------------------- */

    AST_SUSPEND,      /* Generator suspend / yield                            */
    AST_TO,           /* i to j  generator                                    */
    AST_TO_BY,        /* i to j by k  generator                               */
    AST_LIMIT,        /* E \ N  limitation                                    */
    AST_ALTERNATE,       /* Icon / Rebus alt generator, left-then-right (was AST_ALT_GEN) */
    AST_ITERATE,         /* !E  iterate list or string elements (was AST_BANG)     */
    AST_MAKELIST,     /* [e1,e2,...]  list constructor                        */

    /* --- Prolog ---------------------------------------------------------- */

    AST_UNIFY,        /* =/2  unification with trail                          */
    AST_CLAUSE,       /* Horn clause: head + body + EnvLayout                 */
    AST_CHOICE,       /* Predicate choice point: α/β chain over clauses       */
    AST_CUT,          /* !  cut / FENCE — seals β of enclosing choice         */
    AST_TRAIL_MARK,   /* Save trail.top into env slot                         */
    AST_TRAIL_UNWIND, /* Restore trail to saved mark                          */

    /* --- Icon: Numeric Relational ----------------------------------------
     * Goal-directed: succeed and yield rhs if condition holds, else fail.
     * Six distinct Byrd-box wiring patterns (each distinct comparison jump).
     * No SNOBOL4/Prolog equivalent — those use AST_FNC("lt",...) dispatch.
     * M-G9-ICON-IR-WIRE (2026-03-30). */

    AST_LT,           /* E1 < E2   (numeric less-than)                        */
    AST_LE,           /* E1 <= E2  (numeric less-or-equal)                    */
    AST_GT,           /* E1 > E2   (numeric greater-than)                     */
    AST_GE,           /* E1 >= E2  (numeric greater-or-equal)                 */
    AST_EQ,           /* E1 = E2   (numeric equality)                         */
    AST_NE,           /* E1 ~= E2  (numeric not-equal)                        */

    /* --- Icon: Lexicographic (String) Relational -------------------------
     * L prefix = Lexicographic. Goal-directed semantics on string values.
     * AST_L{LT,LE,GT,GE,EQ,NE} — parallel to E_{LT,LE,GT,GE} numeric relops. */

    AST_LLT,          /* E1 << E2  (string less-than)                         */
    AST_LLE,          /* E1 <<= E2 (string less-or-equal)                     */
    AST_LGT,          /* E1 >> E2  (string greater-than)                      */
    AST_LGE,          /* E1 >>= E2 (string greater-or-equal)                  */
    AST_LEQ,         /* E1 == E2  (string equality;  ICN_SEQ)                */
    AST_LNE,          /* E1 ~== E2 (string not-equal)                         */

    /* --- Icon: Cset Operators -------------------------------------------- */

    AST_CSET_COMPL,   /* ~E       cset complement                             */
    AST_CSET_UNION,   /* E1 ++ E2 cset union                                  */
    AST_CSET_DIFF,    /* E1 -- E2 cset difference                             */
    AST_CSET_INTER,   /* E1 ** E2 cset intersection                           */
    AST_LCONCAT,      /* E1 ||| E2  list concatenation (distinct from || str) */

    /* --- Icon: Unary Operators ------------------------------------------- */

    AST_NONNULL,      /* \E   succeed if E non-null, yield E's value          */
    AST_NULL,         /* /E   succeed if E is null, yield &null               */
    AST_NOT,          /* not E  succeed iff E fails                           */
    AST_SIZE,         /* *E   size of string/list/table                       */
    AST_RANDOM,       /* ?E   random element or integer in [1,E]              */
    AST_IDENTICAL,    /* E1 === E2  object identity (same pointer)            */
    AST_AUGOP,        /* E1 op:= E2  augmented assignment; op subtype in ival */

    /* --- Icon: Expression Sequence / Control Flow ------------------------ */

    AST_SEQ_EXPR,     /* (E1; E2; ...; En) — evaluate all, result = last     */
    AST_EVERY,        /* every E [do body]  — drive generator to exhaustion  */
    AST_WHILE,        /* while E [do body]                                    */
    AST_UNTIL,        /* until E [do body]                                    */
    AST_REPEAT,       /* repeat body        — unconditional loop              */
    AST_IF,           /* if E then E2 [else E3]                               */
    AST_CASE,         /* case E of { clauses }                                */
    AST_RETURN,       /* return [E]         — return from procedure           */
    AST_PROC_FAIL,    /* fail               — fail-return from procedure (Icon/Raku)
                     * NOTE: distinct from AST_FAIL = SNOBOL4 FAIL pattern primitive */
    AST_LOOP_BREAK,   /* break [E]          — exit innermost loop
                     * NOTE: distinct from AST_BREAK = SNOBOL4 BREAK(S)      */
    AST_LOOP_NEXT,    /* next               — restart innermost loop          */
    AST_BANG_BINARY,  /* E1 ! E2            — invoke E1 with list E2         */

    /* --- Icon: Structure / Declarations ---------------------------------- */

    AST_SECTION,      /* E[i:j]   string section                              */
    AST_SECTION_PLUS, /* E[i+:n]  section by length (forward)                */
    AST_SECTION_MINUS,/* E[i-:n]  section by length (backward)               */
    AST_RECORD,       /* record declaration                                   */
    AST_FIELD,        /* E.name   field access                                */
    AST_GLOBAL,       /* global varname  declaration                          */
    AST_INITIAL,      /* initial { body }  once-on-first-call block          */
    AST_REVASSIGN,    /* E1 <- E2  reversible assignment (Icon)               */
    AST_REVSWAP,      /* E1 <-> E2 reversible value swap (Icon)               */

    /* --- Sentinel -------------------------------------------------------- */

    AST_KIND_COUNT    /* Total number of kinds — used for array sizing / asserts.
                     * NOT a valid node kind. Must remain last. */

} AST_e;

/* =========================================================================
 * AST_t — unified n-ary expression node
 *
 * All structural children live in the `children` array (realloc-grown).
 * Leaf nodes (AST_QLIT / AST_ILIT / AST_FLIT / AST_CSET / AST_NUL / AST_VAR / AST_KEYWORD)
 * have nchildren == 0.
 *
 * The `id` field is assigned during the emit pass (unique per program).
 * It drives all generated label strings: P_<id>_α, L<id>_α, etc.
 *
 * sval / ival / fval union is not a C union here — all three exist as
 * separate fields to avoid aliasing hazards across frontends. The active
 * field is determined by kind:
 *   sval — AST_QLIT (text), AST_VAR/AST_KEYWORD/AST_FNC/AST_IDX (name), AST_CSET (chars)
 *   ival — AST_ILIT
 *   fval — AST_FLIT
 * ========================================================================= */

/* AST_t — the unified IR node struct.
 *
 * FI-0A: ir.h is the sole owner of this definition. scrip_cc.h no longer
 * carries a duplicate body. The EXPR_T_DEFINED guard has been removed —
 * this struct is defined exactly once, here.
 */
typedef struct AST_t AST_t;

struct AST_t {
    AST_e    kind;          /* node kind from AST_e enum above              */
    char    *sval;          /* string payload (see comment above)           */
    long long ival;         /* integer payload                              */
    double   dval;          /* float payload (named dval throughout codebase) */
    AST_t **children;      /* child nodes — realloc-grown array            */
    int      nchildren;     /* number of valid entries in children[]        */
    int      nalloc;        /* allocated capacity of children[]             */
    int      id;            /* unique node id — assigned at emit time       */
};

/* =========================================================================
 * AST_e name table — for ast_print.c and debugging
 * ========================================================================= */

#ifdef IR_DEFINE_NAMES

static const char * const ast_e_name[AST_KIND_COUNT] = {
    [AST_QLIT]         = "AST_QLIT",
    [AST_ILIT]         = "AST_ILIT",
    [AST_FLIT]         = "AST_FLIT",
    [AST_CSET]         = "AST_CSET",
    [AST_NUL]          = "AST_NUL",
    [AST_VAR]          = "AST_VAR",
    [AST_KEYWORD]           = "AST_KEYWORD",
    [AST_INDIRECT]         = "AST_INDIRECT",
    [AST_DEFER]        = "AST_DEFER",
    [AST_INTERROGATE]  = "AST_INTERROGATE",
    [AST_NAME]         = "AST_NAME",
    [AST_MNS]          = "AST_MNS",
    [AST_PLS]          = "AST_PLS",
    [AST_ADD]          = "AST_ADD",
    [AST_SUB]          = "AST_SUB",
    [AST_MUL]          = "AST_MUL",
    [AST_DIV]          = "AST_DIV",
    [AST_MOD]          = "AST_MOD",
    [AST_POW]          = "AST_POW",
    [AST_SEQ]          = "AST_SEQ",
    [AST_CAT]       = "AST_CAT",
    [AST_ALT]          = "AST_ALT",
    [AST_VLIST]        = "AST_VLIST",
    [AST_OPSYN]        = "AST_OPSYN",
    [AST_ARB]          = "AST_ARB",
    [AST_ARBNO]        = "AST_ARBNO",
    [AST_POS]          = "AST_POS",
    [AST_RPOS]         = "AST_RPOS",
    [AST_ANY]          = "AST_ANY",
    [AST_NOTANY]       = "AST_NOTANY",
    [AST_SPAN]         = "AST_SPAN",
    [AST_BREAK]        = "AST_BREAK",
    [AST_BREAKX]       = "AST_BREAKX",
    [AST_LEN]          = "AST_LEN",
    [AST_TAB]          = "AST_TAB",
    [AST_RTAB]         = "AST_RTAB",
    [AST_REM]          = "AST_REM",
    [AST_FAIL]         = "AST_FAIL",
    [AST_SUCCEED]      = "AST_SUCCEED",
    [AST_FENCE]        = "AST_FENCE",
    [AST_ABORT]        = "AST_ABORT",
    [AST_BAL]          = "AST_BAL",
    [AST_CAPT_COND_ASGN]    = "AST_CAPT_COND_ASGN",
    [AST_CAPT_IMMED_ASGN]     = "AST_CAPT_IMMED_ASGN",
    [AST_CAPT_CURSOR]     = "AST_CAPT_CURSOR",
    [AST_FNC]          = "AST_FNC",
    [AST_IDX]          = "AST_IDX",
    [AST_ASSIGN]       = "AST_ASSIGN",
    [AST_SCAN]        = "AST_SCAN",
    [AST_SWAP]         = "AST_SWAP",
    [AST_SUSPEND]      = "AST_SUSPEND",
    [AST_TO]           = "AST_TO",
    [AST_TO_BY]        = "AST_TO_BY",
    [AST_LIMIT]        = "AST_LIMIT",
    [AST_ALTERNATE]       = "AST_ALTERNATE",
    [AST_ITERATE]         = "AST_ITERATE",
    [AST_MAKELIST]     = "AST_MAKELIST",
    [AST_UNIFY]        = "AST_UNIFY",
    [AST_CLAUSE]       = "AST_CLAUSE",
    [AST_CHOICE]       = "AST_CHOICE",
    [AST_CUT]          = "AST_CUT",
    [AST_TRAIL_MARK]   = "AST_TRAIL_MARK",
    [AST_TRAIL_UNWIND] = "AST_TRAIL_UNWIND",
    /* Icon numeric relational */
    [AST_LT]           = "AST_LT",
    [AST_LE]           = "AST_LE",
    [AST_GT]           = "AST_GT",
    [AST_GE]           = "AST_GE",
    [AST_EQ]           = "AST_EQ",
    [AST_NE]           = "AST_NE",
    /* Icon string relational */
    [AST_LLT]          = "AST_LLT",
    [AST_LLE]          = "AST_LLE",
    [AST_LGT]          = "AST_LGT",
    [AST_LGE]          = "AST_LGE",
    [AST_LEQ]         = "AST_LEQ",
    [AST_LNE]          = "AST_LNE",
    /* Icon cset ops */
    [AST_CSET_COMPL]   = "AST_CSET_COMPL",
    [AST_CSET_UNION]   = "AST_CSET_UNION",
    [AST_CSET_DIFF]    = "AST_CSET_DIFF",
    [AST_CSET_INTER]   = "AST_CSET_INTER",
    [AST_LCONCAT]      = "AST_LCONCAT",
    /* Icon unary */
    [AST_NONNULL]      = "AST_NONNULL",
    [AST_NULL]         = "AST_NULL",
    [AST_NOT]          = "AST_NOT",
    [AST_SIZE]         = "AST_SIZE",
    [AST_RANDOM]       = "AST_RANDOM",
    [AST_IDENTICAL]    = "AST_IDENTICAL",
    [AST_AUGOP]        = "AST_AUGOP",
    /* Icon control flow */
    [AST_SEQ_EXPR]     = "AST_SEQ_EXPR",
    [AST_EVERY]        = "AST_EVERY",
    [AST_WHILE]        = "AST_WHILE",
    [AST_UNTIL]        = "AST_UNTIL",
    [AST_REPEAT]       = "AST_REPEAT",
    [AST_IF]           = "AST_IF",
    [AST_CASE]         = "AST_CASE",
    [AST_RETURN]       = "AST_RETURN",
    [AST_PROC_FAIL]    = "AST_PROC_FAIL",
    [AST_LOOP_BREAK]   = "AST_LOOP_BREAK",
    [AST_LOOP_NEXT]    = "AST_LOOP_NEXT",
    [AST_BANG_BINARY]  = "AST_BANG_BINARY",
    /* Icon structure */
    [AST_SECTION]      = "AST_SECTION",
    [AST_SECTION_PLUS] = "AST_SECTION_PLUS",
    [AST_SECTION_MINUS]= "AST_SECTION_MINUS",
    [AST_RECORD]       = "AST_RECORD",
    [AST_FIELD]        = "AST_FIELD",
    [AST_GLOBAL]       = "AST_GLOBAL",
    [AST_INITIAL]      = "AST_INITIAL",
    [AST_REVASSIGN]    = "AST_REVASSIGN",
    [AST_REVSWAP]      = "AST_REVSWAP",
};

#endif /* IR_DEFINE_NAMES */



#ifdef __cplusplus
}
#endif

#endif /* IR_H */
