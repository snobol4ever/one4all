// PatternBuilder.cs — Walks IrNode pattern subtree → IByrdBox graph
//
// Dispatches on IrKind (not bespoke Node records).
// Mirrors ByrdBoxFactory.cs but takes IrNode trees from Snobol4Parser.
//
// AUTHORS: Lon Jones Cherryholmes · Claude Sonnet 4.6
// SPRINT:  M-NET-INTERP-A01b

using Snobol4.Runtime.Boxes;

namespace ScripInterp;

public sealed class PatternBuilder
{
    private readonly Action<string, string>   _setVar;
    private readonly Func<string, string>     _getStringVar;
    private readonly Func<string, IByrdBox?>  _getPatternVar;
    private readonly Func<IrNode, DESCR>  _evalNode;

    // Shared across outer + all inner builders so that captures from stored
    // pattern variables (getPatternVar) are visible to ByrdBoxExecutor.
    private readonly List<bb_capture> _captures;
    public IReadOnlyList<bb_capture> Captures => _captures;

    public PatternBuilder(
        Action<string, string>   setVar,
        Func<string, string>     getStringVar,
        Func<string, IByrdBox?>  getPatternVar,
        Func<IrNode, DESCR>  evalNode,
        List<bb_capture>?    sharedCaptures = null)
    {
        _captures      = sharedCaptures ?? new List<bb_capture>();
        _setVar        = setVar;
        _getStringVar  = getStringVar;
        _getPatternVar = getPatternVar;
        _evalNode      = evalNode;
    }

    // ── Entry point ──────────────────────────────────────────────────────────

    public IByrdBox Build(IrNode n)
    {
        _captures.Clear();
        return BuildNode(n);
    }

    // ── Recursive builder ────────────────────────────────────────────────────

    private IByrdBox BuildNode(IrNode n)
    {
        return n.Kind switch
        {
            // Structural
            IrKind.E_ALT  => BuildAlt(n),
            IrKind.E_SEQ  => BuildSeq(n),
            IrKind.E_CAT  => BuildSeq(n),   // CAT in pattern context = sequence

            // Literals
            IrKind.E_QLIT => BoxFactory.CreateLit(n.SVal ?? ""),
            IrKind.E_ILIT => BoxFactory.CreateLit(n.IVal.ToString()),
            IrKind.E_FLIT => BoxFactory.CreateLit(n.DVal.ToString()),

            // Captures
            IrKind.E_CAPT_COND_ASGN  => BuildCaptureCond(n),
            IrKind.E_CAPT_IMMED_ASGN => BuildCaptureImm(n),
            IrKind.E_CAPT_CURSOR      => BuildCaptCursor(n),

            // Deferred pattern
            IrKind.E_DEFER => BuildDeferred(n),

            // Variable — resolve at build time
            IrKind.E_VAR  => BuildVar(n.SVal!),

            // Indirect $expr
            IrKind.E_INDIRECT => BuildIndirect(n),

            // Nullary pattern primitives
            IrKind.E_ARB     => BoxFactory.CreateArb(),
            IrKind.E_REM     => BoxFactory.CreateRem(),
            IrKind.E_FAIL    => BoxFactory.CreateFail(),
            IrKind.E_SUCCEED => BoxFactory.CreateSucceed(),
            IrKind.E_FENCE   => BoxFactory.CreateFence(),
            IrKind.E_ABORT   => BoxFactory.CreateAbort(),
            IrKind.E_BAL     => BoxFactory.CreateBal(),

            // Unary pattern primitives — arg in Children[0]
            IrKind.E_ANY     => BoxFactory.CreateAny(StrArg(n, 0)),
            IrKind.E_NOTANY  => BoxFactory.CreateNotany(StrArg(n, 0)),
            IrKind.E_SPAN    => BoxFactory.CreateSpan(StrArg(n, 0)),
            IrKind.E_BREAK   => BoxFactory.CreateBrk(StrArg(n, 0)),
            IrKind.E_BREAKX  => BoxFactory.CreateBreakx(StrArg(n, 0)),
            IrKind.E_LEN     => BoxFactory.CreateLen(IntArg(n, 0)),
            IrKind.E_TAB     => BoxFactory.CreateTab(IntArg(n, 0)),
            IrKind.E_RTAB    => BoxFactory.CreateRtab(IntArg(n, 0)),
            IrKind.E_POS     => BoxFactory.CreatePos(IntArg(n, 0)),
            IrKind.E_RPOS    => BoxFactory.CreateRpos(IntArg(n, 0)),
            IrKind.E_ARBNO   => n.Children.Length >= 1
                                    ? BoxFactory.CreateArbno(BuildNode(n.Children[0]))
                                    : BoxFactory.CreateEps(),

            // Function call — may be a pattern builtin with dynamic args
            IrKind.E_FNC  => BuildFncPattern(n),

            _ => BoxFactory.CreateLit("")   // safe fallback
        };
    }

    // ── Alt ──────────────────────────────────────────────────────────────────

    private IByrdBox BuildAlt(IrNode n)
    {
        var parts = new List<IrNode>();
        void Collect(IrNode x)
        {
            if (x.Kind == IrKind.E_ALT)
                foreach (var c in x.Children) Collect(c);
            else
                parts.Add(x);
        }
        Collect(n);
        return BoxFactory.CreateAlt(parts.Select(BuildNode).ToArray());
    }

    // ── Seq ──────────────────────────────────────────────────────────────────

    private IByrdBox BuildSeq(IrNode n)
    {
        var parts = new List<IrNode>();
        void Collect(IrNode x)
        {
            if (x.Kind == IrKind.E_SEQ || x.Kind == IrKind.E_CAT)
                foreach (var c in x.Children) Collect(c);
            else
                parts.Add(x);
        }
        Collect(n);
        if (parts.Count == 0) return BoxFactory.CreateEps();
        if (parts.Count == 1) return BuildNode(parts[0]);

        // Build left-to-right as a list of IByrdBox, handling capture wrapping:
        // When a part is E_CAPT_COND_ASGN or E_CAPT_IMMED_ASGN, it wraps the
        // immediately preceding box (its left sibling in the pattern sequence).
        var boxes = new List<IByrdBox>();
        foreach (var part in parts)
        {
            bool isCond  = part.Kind == IrKind.E_CAPT_COND_ASGN;
            bool isImmed = part.Kind == IrKind.E_CAPT_IMMED_ASGN;
            if ((isCond || isImmed) && boxes.Count > 0)
            {
                // Pop the last box — this is what the capture matches over
                var prev    = boxes[^1];
                boxes.RemoveAt(boxes.Count - 1);
                var varName = part.Children.Length > 0 && part.Children[0].Kind == IrKind.E_VAR
                            ? part.Children[0].SVal!
                            : (part.SVal ?? "");
                var cap = BoxFactory.CreateCapture(prev, varName, immediate: isImmed);
                cap.SetVar = _setVar;
                _captures.Add(cap);
                boxes.Add(cap);
            }
            else if (part.Kind == IrKind.E_CAPT_CURSOR)
            {
                // @var — cursor capture wraps Eps (records position, not span)
                var varName = part.Children.Length > 0 && part.Children[0].Kind == IrKind.E_VAR
                            ? part.Children[0].SVal!
                            : (part.SVal ?? "");
                { var _atp = BoxFactory.CreateAtp(varName); _atp.SetVar = _setVar; boxes.Add(_atp); }
            }
            else
            {
                boxes.Add(BuildNode(part));
            }
        }

        if (boxes.Count == 1) return boxes[0];
        // Right-fold into bb_seq chain
        IByrdBox right = boxes[^1];
        for (int i = boxes.Count - 2; i >= 0; i--)
            right = BoxFactory.CreateSeq(boxes[i], right);
        return right;
    }

    // ── Captures ─────────────────────────────────────────────────────────────

    private IByrdBox BuildCaptureCond(IrNode n)
    {
        // Children[0] = inner pattern node, SVal or Children[0] of inner = varname
        // For E_CAPT_COND_ASGN the child is E_VAR(varname) — the inner pattern
        // is the surrounding context; here we wrap the previous box.
        // When parser emits E_CAPT_COND_ASGN with one child E_VAR, the pattern
        // is the containing SEQ node's left sibling. For standalone .var, wrap Eps.
        var varName = n.Children.Length > 0 && n.Children[0].Kind == IrKind.E_VAR
                    ? n.Children[0].SVal!
                    : (n.SVal ?? "");
        var inner   = BoxFactory.CreateEps();
        var box     = BoxFactory.CreateCapture(inner, varName, immediate: false);
        box.SetVar = _setVar;
        _captures.Add(box);
        return box;
    }

    private IByrdBox BuildCaptureImm(IrNode n)
    {
        var varName = n.Children.Length > 0 && n.Children[0].Kind == IrKind.E_VAR
                    ? n.Children[0].SVal!
                    : (n.SVal ?? "");
        var inner   = BoxFactory.CreateEps();
        var _cap2 = BoxFactory.CreateCapture(inner, varName, immediate: true);
        _cap2.SetVar = _setVar;
        return _cap2;
    }

    private IByrdBox BuildCaptCursor(IrNode n)
    {
        var varName = n.Children.Length > 0 && n.Children[0].Kind == IrKind.E_VAR
                    ? n.Children[0].SVal!
                    : (n.SVal ?? "");
        var _atp2 = BoxFactory.CreateAtp(varName);
        _atp2.SetVar = _setVar;
        return _atp2;
    }

    // ── Deferred ─────────────────────────────────────────────────────────────

    private IByrdBox BuildDeferred(IrNode n)
    {
        if (n.Children.Length > 0 && n.Children[0].Kind == IrKind.E_VAR)
        {
            var name = n.Children[0].SVal!;
            var _dv = BoxFactory.CreateDvar(name);
            _dv.GetStringVar = _getStringVar;
            _dv.GetPatternVar = _getPatternVar;
            return _dv;
        }
        var val = _evalNode(n.Children.Length > 0 ? n.Children[0] : n).ToString();
        return BoxFactory.CreateLit(val);
    }

    // ── Variable ─────────────────────────────────────────────────────────────

    private IByrdBox BuildVar(string name)
    {
        var patBox = _getPatternVar(name);
        if (patBox != null) return patBox;
        var str = _getStringVar(name);
        return BoxFactory.CreateLit(str);
    }

    // ── Indirect ─────────────────────────────────────────────────────────────

    private IByrdBox BuildIndirect(IrNode n)
    {
        var val = n.Children.Length > 0 ? _evalNode(n.Children[0]).ToString() : "";
        return BuildVar(val);
    }

    // ── Function call — pattern builtins with dynamic args ───────────────────

    private IByrdBox BuildFncPattern(IrNode n)
    {
        var name = n.SVal?.ToUpperInvariant() ?? "";
        var args = n.Children;

        int    IntArg2(int i) => args.Length > i ? (int)_evalNode(args[i]).ToInt() : 0;
        string StrArg2(int i) => args.Length > i ? _evalNode(args[i]).ToString() : "";

        return name switch
        {
            "LEN"     => BoxFactory.CreateLen(IntArg2(0)),
            "POS"     => BoxFactory.CreatePos(IntArg2(0)),
            "RPOS"    => BoxFactory.CreateRpos(IntArg2(0)),
            "TAB"     => BoxFactory.CreateTab(IntArg2(0)),
            "RTAB"    => BoxFactory.CreateRtab(IntArg2(0)),
            "REM"     => BoxFactory.CreateRem(),
            "ANY"     => BoxFactory.CreateAny(StrArg2(0)),
            "NOTANY"  => BoxFactory.CreateNotany(StrArg2(0)),
            "SPAN"    => BoxFactory.CreateSpan(StrArg2(0)),
            "BREAK"   => BoxFactory.CreateBrk(StrArg2(0)),
            "BREAKX"  => BoxFactory.CreateBreakx(StrArg2(0)),
            "BAL"     => BoxFactory.CreateBal(),
            "FENCE"   => BoxFactory.CreateFence(),
            "ABORT"   => BoxFactory.CreateAbort(),
            "FAIL"    => BoxFactory.CreateFail(),
            "SUCCEED" => BoxFactory.CreateSucceed(),
            "ARB"     => BoxFactory.CreateArb(),
            "ARBNO"   => args.Length >= 1 ? BoxFactory.CreateArbno(BuildNode(args[0])) : BoxFactory.CreateEps(),
            _         => BoxFactory.CreateLit(StrArg2(0))
        };
    }

    // ── Arg helpers ──────────────────────────────────────────────────────────

    private int    IntArg(IrNode n, int i) =>
        n.Children.Length > i ? (int)_evalNode(n.Children[i]).ToInt() : 0;

    private string StrArg(IrNode n, int i) =>
        n.Children.Length > i ? _evalNode(n.Children[i]).ToString() : "";
}
