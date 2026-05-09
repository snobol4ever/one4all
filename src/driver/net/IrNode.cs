// IrNode.cs — C# mirror of the unified IR
//
// Mirrors:
//   EKind     enum  → ir.h  (same names, same semantics)
//   IrNode    class → AST_t struct in ir.h
//   SnoGoto   class → removed RS-1; goto fields now flat in IrStmt / STMT_t
//   IrStmt    class → STMT_t struct in scrip_cc.h
//
// This is the "one IR, three consumers" invariant:
//   scrip-cc (C)        consumes AST_t / EKind from ir.h
//   scrip-interp.c (C)  consumes same
//   scrip-interp.cs     consumes IrNode / IrKind (this file)
//
// Node kind names are IDENTICAL to EKind names.
// SNOBOL4-frontend-relevant subset only: Icon/Prolog/Rebus kinds included
// as enum members for completeness but not dispatched by this interpreter.
//
// AUTHORS: Lon Jones Cherryholmes · Jeffrey Cooper M.D. · Claude Sonnet 4.6
// SPRINT:  D-169 / M-NET-INTERP-A01b

namespace ScripInterp;

// ── IrKind — mirrors EKind from ir.h exactly ───────────────────────────────

public enum IrKind
{
    // Literals
    AST_QLIT,             // quoted string / pattern literal
    AST_ILIT,             // integer literal
    AST_FLIT,             // float literal
    AST_CSET,             // cset literal (Icon/Rebus)
    AST_NUL,              // null / empty value

    // References
    AST_VAR,              // variable reference
    AST_KEYWORD,          // &IDENT keyword
    AST_INDIRECT,         // $expr indirect reference
    AST_DEFER,            // *expr deferred pattern ref

    // Arithmetic / operators
    AST_INTERROGATE,      // ?X interrogation
    AST_NAME,             // .X name reference
    AST_MNS,              // unary minus
    AST_PLS,              // unary plus / numeric coerce
    AST_ADD,
    AST_SUB,
    AST_MUL,
    AST_DIV,
    AST_MOD,
    AST_POW,

    // Sequence / alternation
    AST_SEQ,              // goal-directed concat (Byrd-box wiring)
    AST_CAT,              // pure value-context string concat
    AST_ALT,              // pattern alternation
    AST_OPSYN,            // & operator

    // Pattern primitives
    AST_ARB,
    AST_ARBNO,
    AST_POS,
    AST_RPOS,
    AST_ANY,
    AST_NOTANY,
    AST_SPAN,
    AST_BREAK,
    AST_BREAKX,
    AST_LEN,
    AST_TAB,
    AST_RTAB,
    AST_REM,
    AST_FAIL,
    AST_SUCCEED,
    AST_FENCE,
    AST_ABORT,
    AST_BAL,

    // Captures
    AST_CAPT_COND_ASGN,   // .var conditional capture
    AST_CAPT_IMMED_ASGN,  // $var immediate capture
    AST_CAPT_CURSOR,      // @var cursor capture

    // Call / access / assignment / scan
    AST_FNC,              // function call, n-ary
    AST_IDX,              // array/table subscript
    AST_ASSIGN,           // assignment
    AST_SCAN,             // E ? E scanning
    AST_SWAP,             // :=: swap

    // Icon generators (present for enum completeness; not dispatched by SNOBOL4 interpreter)
    AST_SUSPEND, AST_TO, AST_TO_BY, AST_LIMIT, AST_ALTERNATE, AST_ITERATE, AST_MAKELIST,

    // Prolog (present for completeness)
    AST_UNIFY, AST_CLAUSE, AST_CHOICE, AST_CUT, AST_TRAIL_MARK, AST_TRAIL_UNWIND,

    // Icon numeric relational
    AST_LT, AST_LE, AST_GT, AST_GE, AST_EQ, AST_NE,

    // Icon lexicographic relational
    AST_LLT, AST_LLE, AST_LGT, AST_LGE, AST_LEQ, AST_LNE,

    // Icon cset operators
    AST_CSET_COMPL, AST_CSET_UNION, AST_CSET_DIFF, AST_CSET_INTER, AST_LCONCAT,

    // Icon unary
    AST_NONNULL, AST_NULL, AST_NOT, AST_SIZE, AST_RANDOM, AST_IDENTICAL, AST_AUGOP,

    // Icon control flow
    AST_SEQ_EXPR, AST_EVERY, AST_WHILE, AST_UNTIL, AST_REPEAT,
    AST_IF, AST_CASE, AST_RETURN, AST_LOOP_BREAK, AST_LOOP_NEXT,
    AST_BANG_BINARY,
}

// ── IrNode — mirrors AST_t from ir.h ──────────────────────────────────────
//
// Fields:
//   Kind      ← kind
//   SVal      ← sval  (AST_QLIT text; AST_VAR/AST_KEYWORD/AST_FNC/AST_IDX name)
//   IVal      ← ival  (AST_ILIT value)
//   DVal      ← dval  (AST_FLIT value; note: ir.h uses dval, not fval)
//   Children  ← children[] / nchildren
//   Id        ← id    (assigned at emit time; not needed by interpreter)

public sealed class IrNode
{
    public IrKind   Kind;
    public string?  SVal;
    public long     IVal;
    public double   DVal;
    public IrNode[] Children;
    public int      Id;

    public IrNode(IrKind kind)
    {
        Kind     = kind;
        Children = [];
    }

    // ── Leaf constructors ─────────────────────────────────────────────────

    public static IrNode QStr(string s)
        => new(IrKind.AST_QLIT)  { SVal = s };

    public static IrNode Int(long v)
        => new(IrKind.AST_ILIT)  { IVal = v };

    public static IrNode Float(double v)
        => new(IrKind.AST_FLIT)  { DVal = v };

    public static IrNode Nul()
        => new(IrKind.AST_NUL);

    public static IrNode Var(string name)
        => new(IrKind.AST_VAR)   { SVal = name };

    public static IrNode Keyword(string name)
        => new(IrKind.AST_KEYWORD) { SVal = name };

    // ── N-ary constructor ─────────────────────────────────────────────────

    public static IrNode Nary(IrKind kind, params IrNode[] children)
    {
        var n = new IrNode(kind) { Children = children };
        return n;
    }

    // ── Convenience predicates ────────────────────────────────────────────

    public bool IsLeaf     => Children.Length == 0;
    public bool IsPattern  => Kind is
        IrKind.AST_SEQ or IrKind.AST_ALT or IrKind.AST_ARB or IrKind.AST_ARBNO or
        IrKind.AST_ANY or IrKind.AST_NOTANY or IrKind.AST_SPAN or IrKind.AST_BREAK or
        IrKind.AST_BREAKX or IrKind.AST_LEN or IrKind.AST_TAB or IrKind.AST_RTAB or
        IrKind.AST_REM or IrKind.AST_FAIL or IrKind.AST_SUCCEED or IrKind.AST_FENCE or
        IrKind.AST_ABORT or IrKind.AST_BAL or IrKind.AST_POS or IrKind.AST_RPOS or
        IrKind.AST_CAPT_COND_ASGN or IrKind.AST_CAPT_IMMED_ASGN or
        IrKind.AST_CAPT_CURSOR or IrKind.AST_DEFER or IrKind.AST_QLIT;

    public override string ToString() =>
        SVal != null ? $"{Kind}({SVal})" :
        Kind == IrKind.AST_ILIT ? $"AST_ILIT({IVal})" :
        Kind == IrKind.AST_FLIT ? $"AST_FLIT({DVal})" :
        $"{Kind}[{Children.Length}]";
}

// ── IrStmt — mirrors STMT_t from scrip_cc.h ───────────────────────────────
//
// RS-1: SnoGoto class removed; goto fields flattened directly into IrStmt.
//
// Fields:
//   Label       ← label
//   Subject     ← subject   (AST_t*)
//   Pattern     ← pattern   (AST_t*; null if no pattern)
//   Replacement ← replacement (AST_t*; null if no replacement)
//   GotoS       ← goto_s    (on success)
//   GotoF       ← goto_f    (on failure)
//   GotoU       ← goto_u    (unconditional)
//   IsEnd       ← is_end
//   HasEq       ← has_eq    (explicit = replacement present)
//   LineNo      ← lineno

public sealed class IrStmt
{
    public string?  Label;
    public IrNode?  Subject;
    public IrNode?  Pattern;
    public IrNode?  Replacement;
    public string?  GotoS;          // ← goto_s  :S(label)
    public string?  GotoF;          // ← goto_f  :F(label)
    public string?  GotoU;          // ← goto_u  :(label)
    public bool     IsEnd;
    public bool     HasEq;
    public int      LineNo;

    // Convenience: effective goto targets (mirrors C interp logic)
    public string? GotoOnSuccess => GotoS ?? GotoU;
    public string? GotoOnFailure => GotoF ?? GotoU;
    public string? GotoUnconditional => GotoU;

    public bool HasPattern     => Pattern != null;
    public bool HasReplacement => Replacement != null || HasEq;
    public bool HasGoto        => GotoS != null || GotoF != null || GotoU != null;

    public override string ToString()
    {
        var sb = new System.Text.StringBuilder();
        if (Label != null) sb.Append(Label).Append(": ");
        if (Subject != null) sb.Append(Subject);
        if (Pattern != null) sb.Append(" ? ").Append(Pattern);
        if (Replacement != null) sb.Append(" = ").Append(Replacement);
        if (HasGoto) sb.Append(" :").Append(GotoOnSuccess ?? "").Append("/").Append(GotoOnFailure ?? "");
        if (IsEnd) sb.Append(" [END]");
        return sb.ToString();
    }
}
