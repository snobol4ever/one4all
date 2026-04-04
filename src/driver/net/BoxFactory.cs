// BoxFactory.cs — Compile all Byrd box types at startup via System.Reflection.Emit
//
// Uses ILGenerator to transcribe opcodes from the verified .il oracle files.
// Called once at process start; resulting Type objects cached in static fields.
// PatternBuilder and Executor call BoxFactory.CreateLit(...) etc. instead of new bb_lit(...).
//
// Method names in the emitted types use the Unicode interface names α / β
// (matching IByrdBox) rather than the IL oracle's Alpha/Beta — the oracle is
// used for opcode logic only, not for method naming.
//
// AUTHORS: Lon Jones Cherryholmes · Jeffrey Cooper M.D. · Claude Sonnet 4.6
// SPRINT:  D-177

using System.Reflection;
using System.Reflection.Emit;
using Snobol4.Runtime.Boxes;

namespace ScripInterp;

public static class BoxFactory
{
    // ── Cached runtime Types ─────────────────────────────────────────────────
    private static Type _tLit      = null!;
    private static Type _tEps      = null!;
    private static Type _tPos      = null!;
    private static Type _tRpos     = null!;
    private static Type _tLen      = null!;
    private static Type _tTab      = null!;
    private static Type _tRtab     = null!;
    private static Type _tRem      = null!;
    private static Type _tAny      = null!;
    private static Type _tNotany   = null!;
    private static Type _tSpan     = null!;
    private static Type _tBrk      = null!;
    private static Type _tBreakx   = null!;
    private static Type _tArb      = null!;
    private static Type _tSeq      = null!;
    private static Type _tAlt      = null!;
    private static Type _tArbno    = null!;
    private static Type _tCapture  = null!;
    private static Type _tAtp      = null!;
    private static Type _tDvar     = null!;
    private static Type _tBal      = null!;
    private static Type _tFence    = null!;
    private static Type _tAbort    = null!;
    private static Type _tFail     = null!;
    private static Type _tSucceed  = null!;
    private static Type _tNot      = null!;
    private static Type _tInterr   = null!;

    // ── Well-known reflected members (used repeatedly in IL emission) ────────
    private static readonly Type   TSpec       = typeof(Spec);
    private static readonly Type   TMatchState = typeof(MatchState);
    private static readonly Type   TIByrdBox   = typeof(IByrdBox);
    private static readonly Type   TObject     = typeof(object);
    private static readonly Type   TString     = typeof(string);
    private static readonly Type   TInt32      = typeof(int);
    private static readonly Type   TBool       = typeof(bool);
    private static readonly Type   TVoid       = typeof(void);

    private static readonly FieldInfo FSpec_Start   = TSpec.GetField(nameof(Spec.Start))!;
    private static readonly FieldInfo FSpec_Length  = TSpec.GetField(nameof(Spec.Length))!;
    private static readonly FieldInfo FSpec_Fail    = TSpec.GetField(nameof(Spec.Fail))!;
    private static readonly MethodInfo MSpec_Of     = TSpec.GetMethod(nameof(Spec.Of))!;
    private static readonly MethodInfo MSpec_ZW     = TSpec.GetMethod(nameof(Spec.ZeroWidth))!;
    private static readonly MethodInfo MSpec_IsFail = TSpec.GetProperty(nameof(Spec.IsFail))!.GetGetMethod()!;

    private static readonly MethodInfo MMS_CursorGet = TMatchState.GetProperty(nameof(MatchState.Cursor))!.GetGetMethod()!;
    private static readonly MethodInfo MMS_CursorSet = TMatchState.GetProperty(nameof(MatchState.Cursor))!.GetSetMethod()!;
    private static readonly MethodInfo MMS_Length  = TMatchState.GetProperty(nameof(MatchState.Length))!.GetGetMethod()!;
    private static readonly MethodInfo MMS_Subject = TMatchState.GetProperty(nameof(MatchState.Subject))!.GetGetMethod()!;
    private static readonly MethodInfo MMS_MatchAt = TMatchState.GetMethod(nameof(MatchState.MatchesAt))!;
    private static readonly MethodInfo MMS_CharIn  = TMatchState.GetMethod(nameof(MatchState.CharInSet))!;

    private static readonly MethodInfo MIBox_Alpha = TIByrdBox.GetMethod("α")!;
    private static readonly MethodInfo MIBox_Beta  = TIByrdBox.GetMethod("β")!;

    private static readonly MethodInfo MStr_Equals   = TString.GetMethod("Equals", new[]{TString})!;
    private static readonly MethodInfo MStr_Substr   = TString.GetMethod("Substring", new[]{TInt32, TInt32})!;
    private static readonly MethodInfo MStr_Length   = TString.GetProperty("Length")!.GetGetMethod()!;
    private static readonly MethodInfo MStr_GetChars = TString.GetMethod("get_Chars")!;
    private static readonly MethodInfo MObjToString  = TObject.GetMethod("ToString")!;

    private static readonly ConstructorInfo CtorObject = TObject.GetConstructor(Type.EmptyTypes)!;

    private static readonly Type TActionSS = typeof(Action<string,string>);
    private static readonly Type TFuncSS   = typeof(Func<string,string>);
    private static readonly Type TFuncSIBox= typeof(Func<string,IByrdBox?>);
    private static readonly MethodInfo MActionInvoke = TActionSS.GetMethod("Invoke")!;
    private static readonly MethodInfo MFuncSSInvoke  = TFuncSS.GetMethod("Invoke")!;
    private static readonly MethodInfo MFuncSIBInvoke = TFuncSIBox.GetMethod("Invoke")!;

    // ── Module ───────────────────────────────────────────────────────────────
    private static ModuleBuilder _mod = null!;

    // ── Public initialiser — call once at startup ────────────────────────────
    public static void Init()
    {
        var asmName = new AssemblyName("Snobol4.Boxes.Dynamic");
        var asm     = AssemblyBuilder.DefineDynamicAssembly(asmName, AssemblyBuilderAccess.Run);
        _mod        = asm.DefineDynamicModule("BoxModule");

        _tLit     = BuildLit();
        _tEps     = BuildEps();
        _tPos     = BuildPos();
        _tRpos    = BuildRpos();
        _tLen     = BuildLen();
        _tTab     = BuildTab();
        _tRtab    = BuildRtab();
        _tRem     = BuildRem();
        _tAny     = BuildAny();
        _tNotany  = BuildNotany();
        _tSpan    = BuildSpan();
        _tBrk     = BuildBrk();
        _tBreakx  = BuildBreakx();
        _tArb     = BuildArb();
        _tSeq     = BuildSeq();
        _tAlt     = BuildAlt();
        _tArbno   = BuildArbno();
        _tCapture = BuildCapture();
        _tAtp     = BuildAtp();
        _tDvar    = BuildDvar();
        _tBal     = BuildBal();
        _tFence   = BuildFence();
        _tAbort   = BuildAbort();
        _tFail    = BuildFail();
        _tSucceed = BuildSucceed();
        _tNot     = BuildNot();
        _tInterr  = BuildInterr();
    }

    // ── Factory methods (called by PatternBuilder) ───────────────────────────
    public static IByrdBox CreateLit(string lit)
        => (IByrdBox)Activator.CreateInstance(_tLit, lit)!;
    public static IByrdBox CreateEps()
        => (IByrdBox)Activator.CreateInstance(_tEps)!;
    public static IByrdBox CreatePos(int n)
        => (IByrdBox)Activator.CreateInstance(_tPos, n)!;
    public static IByrdBox CreateRpos(int n)
        => (IByrdBox)Activator.CreateInstance(_tRpos, n)!;
    public static IByrdBox CreateLen(int n)
        => (IByrdBox)Activator.CreateInstance(_tLen, n)!;
    public static IByrdBox CreateTab(int n)
        => (IByrdBox)Activator.CreateInstance(_tTab, n)!;
    public static IByrdBox CreateRtab(int n)
        => (IByrdBox)Activator.CreateInstance(_tRtab, n)!;
    public static IByrdBox CreateRem()
        => (IByrdBox)Activator.CreateInstance(_tRem)!;
    public static IByrdBox CreateAny(string chars)
        => (IByrdBox)Activator.CreateInstance(_tAny, chars)!;
    public static IByrdBox CreateNotany(string chars)
        => (IByrdBox)Activator.CreateInstance(_tNotany, chars)!;
    public static IByrdBox CreateSpan(string chars)
        => (IByrdBox)Activator.CreateInstance(_tSpan, chars)!;
    public static IByrdBox CreateBrk(string chars)
        => (IByrdBox)Activator.CreateInstance(_tBrk, chars)!;
    public static IByrdBox CreateBreakx(string chars)
        => (IByrdBox)Activator.CreateInstance(_tBreakx, chars)!;
    public static IByrdBox CreateArb()
        => (IByrdBox)Activator.CreateInstance(_tArb)!;
    public static IByrdBox CreateSeq(IByrdBox left, IByrdBox right)
        => (IByrdBox)Activator.CreateInstance(_tSeq, left, right)!;
    public static IByrdBox CreateAlt(IByrdBox[] children)
        => (IByrdBox)Activator.CreateInstance(_tAlt, (object)children)!;
    public static IByrdBox CreateArbno(IByrdBox body)
        => (IByrdBox)Activator.CreateInstance(_tArbno, body)!;
    public static bb_capture CreateCapture(IByrdBox child, string varname, bool immediate)
        => (bb_capture)Activator.CreateInstance(_tCapture, child, varname, immediate)!;
    public static bb_atp CreateAtp(string varname)
        => (bb_atp)Activator.CreateInstance(_tAtp, varname)!;
    public static bb_dvar CreateDvar(string varname)
        => (bb_dvar)Activator.CreateInstance(_tDvar, varname)!;
    public static IByrdBox CreateBal()
        => (IByrdBox)Activator.CreateInstance(_tBal)!;
    public static IByrdBox CreateFence()
        => (IByrdBox)Activator.CreateInstance(_tFence)!;
    public static IByrdBox CreateAbort()
        => (IByrdBox)Activator.CreateInstance(_tAbort)!;
    public static IByrdBox CreateFail()
        => (IByrdBox)Activator.CreateInstance(_tFail)!;
    public static IByrdBox CreateSucceed()
        => (IByrdBox)Activator.CreateInstance(_tSucceed)!;
    public static IByrdBox CreateNot(IByrdBox child)
        => (IByrdBox)Activator.CreateInstance(_tNot, child)!;
    public static IByrdBox CreateInterr(IByrdBox child)
        => (IByrdBox)Activator.CreateInstance(_tInterr, child)!;

    // ── Helpers ──────────────────────────────────────────────────────────────

    private static TypeBuilder NewBox(string name)
    {
        var tb = _mod.DefineType(
            "Snobol4.Runtime.Boxes." + name,
            TypeAttributes.Public | TypeAttributes.Sealed | TypeAttributes.BeforeFieldInit,
            TObject, new[] { TIByrdBox });
        // Default .ctor (may be overridden per-type)
        return tb;
    }

    private static void AddBaseCtor(TypeBuilder tb, Type[] paramTypes, Action<ILGenerator> bodyEmitter)
    {
        var ctor = tb.DefineConstructor(
            MethodAttributes.Public | MethodAttributes.SpecialName | MethodAttributes.RTSpecialName,
            CallingConventions.Standard, paramTypes);
        var il = ctor.GetILGenerator();
        bodyEmitter(il);
        il.Emit(OpCodes.Ret);
    }

    private static MethodBuilder AddAlpha(TypeBuilder tb, Action<ILGenerator> body)
        => AddPort(tb, "α", body);

    private static MethodBuilder AddBeta(TypeBuilder tb, Action<ILGenerator> body)
        => AddPort(tb, "β", body);

    private static MethodBuilder AddPort(TypeBuilder tb, string name, Action<ILGenerator> body)
    {
        var mb = tb.DefineMethod(name,
            MethodAttributes.Public | MethodAttributes.Virtual | MethodAttributes.Final,
            TSpec, new[] { TMatchState });
        var il = mb.GetILGenerator();
        body(il);
        tb.DefineMethodOverride(mb,
            name == "α" ? MIBox_Alpha : MIBox_Beta);
        return mb;
    }

    // Emit: ldsfld Spec::Fail; ret
    private static void EmitFail(ILGenerator il)
    {
        il.Emit(OpCodes.Ldsfld, FSpec_Fail);
        il.Emit(OpCodes.Ret);
    }

    // Emit: load ms.Cursor; call Spec.ZeroWidth; ret
    private static void EmitZeroWidthRet(ILGenerator il)
    {
        il.Emit(OpCodes.Ldarg_1);
        il.Emit(OpCodes.Callvirt, MMS_CursorGet);
        il.Emit(OpCodes.Call, MSpec_ZW);
        il.Emit(OpCodes.Ret);
    }

    // Emit call to IsByrdBox.IsFail property getter on a Spec local
    private static void EmitIsFail(ILGenerator il, LocalBuilder specLocal)
    {
        il.Emit(OpCodes.Ldloca_S, specLocal);
        il.Emit(OpCodes.Call, MSpec_IsFail);
    }

    // ── bb_lit ───────────────────────────────────────────────────────────────
    // α: cursor+len>Length→fail; !MatchesAt→fail; result=Of(cursor,len); cursor+=len; ret
    // β: cursor-=len; fail
    private static Type BuildLit()
    {
        var tb   = NewBox("bb_lit");
        var fLit = tb.DefineField("_lit", TString, FieldAttributes.Private);
        var fLen = tb.DefineField("_len", TInt32,  FieldAttributes.Private);

        AddBaseCtor(tb, new[]{TString}, il => {
            il.Emit(OpCodes.Ldarg_0);
            il.Emit(OpCodes.Call, CtorObject);
            il.Emit(OpCodes.Ldarg_0);
            il.Emit(OpCodes.Ldarg_1);
            var notNull = il.DefineLabel();
            il.Emit(OpCodes.Dup);
            il.Emit(OpCodes.Brtrue_S, notNull);
            il.Emit(OpCodes.Pop);
            il.Emit(OpCodes.Ldstr, "");
            il.MarkLabel(notNull);
            il.Emit(OpCodes.Stfld, fLit);
            il.Emit(OpCodes.Ldarg_0);
            il.Emit(OpCodes.Ldarg_0);
            il.Emit(OpCodes.Ldfld, fLit);
            il.Emit(OpCodes.Callvirt, MStr_Length);
            il.Emit(OpCodes.Stfld, fLen);
        });

        AddAlpha(tb, il => {
            var fail   = il.DefineLabel();
            var result = il.DeclareLocal(TSpec);
            // if cursor + len > Length → fail
            il.Emit(OpCodes.Ldarg_1); il.Emit(OpCodes.Callvirt, MMS_CursorGet);
            il.Emit(OpCodes.Ldarg_0); il.Emit(OpCodes.Ldfld, fLen);
            il.Emit(OpCodes.Add);
            il.Emit(OpCodes.Ldarg_1); il.Emit(OpCodes.Callvirt, MMS_Length);
            il.Emit(OpCodes.Bgt, fail);
            // if !MatchesAt(cursor, lit) → fail
            il.Emit(OpCodes.Ldarg_1);
            il.Emit(OpCodes.Ldarg_1); il.Emit(OpCodes.Callvirt, MMS_CursorGet);
            il.Emit(OpCodes.Ldarg_0); il.Emit(OpCodes.Ldfld, fLit);
            il.Emit(OpCodes.Callvirt, MMS_MatchAt);
            il.Emit(OpCodes.Brfalse, fail);
            // result = Spec.Of(cursor, len)
            il.Emit(OpCodes.Ldarg_1); il.Emit(OpCodes.Callvirt, MMS_CursorGet);
            il.Emit(OpCodes.Ldarg_0); il.Emit(OpCodes.Ldfld, fLen);
            il.Emit(OpCodes.Call, MSpec_Of);
            il.Emit(OpCodes.Stloc, result);
            // cursor += len
            il.Emit(OpCodes.Ldarg_1);
            il.Emit(OpCodes.Ldarg_1); il.Emit(OpCodes.Callvirt, MMS_CursorGet);
            il.Emit(OpCodes.Ldarg_0); il.Emit(OpCodes.Ldfld, fLen);
            il.Emit(OpCodes.Add);
            il.Emit(OpCodes.Callvirt, MMS_CursorSet);
            il.Emit(OpCodes.Ldloc, result);
            il.Emit(OpCodes.Ret);
            il.MarkLabel(fail);
            EmitFail(il);
        });

        AddBeta(tb, il => {
            // cursor -= len
            il.Emit(OpCodes.Ldarg_1);
            il.Emit(OpCodes.Ldarg_1); il.Emit(OpCodes.Callvirt, MMS_CursorGet);
            il.Emit(OpCodes.Ldarg_0); il.Emit(OpCodes.Ldfld, fLen);
            il.Emit(OpCodes.Sub);
            il.Emit(OpCodes.Callvirt, MMS_CursorSet);
            EmitFail(il);
        });

        return tb.CreateType()!;
    }

    // ── bb_eps ───────────────────────────────────────────────────────────────
    // α: if _done→fail; _done=true; ZeroWidth
    // β: fail
    private static Type BuildEps()
    {
        var tb    = NewBox("bb_eps");
        var fDone = tb.DefineField("_done", TBool, FieldAttributes.Private);

        AddBaseCtor(tb, Type.EmptyTypes, il => {
            il.Emit(OpCodes.Ldarg_0); il.Emit(OpCodes.Call, CtorObject);
        });

        AddAlpha(tb, il => {
            var fail = il.DefineLabel();
            il.Emit(OpCodes.Ldarg_0); il.Emit(OpCodes.Ldfld, fDone);
            il.Emit(OpCodes.Brtrue, fail);
            il.Emit(OpCodes.Ldarg_0); il.Emit(OpCodes.Ldc_I4_1); il.Emit(OpCodes.Stfld, fDone);
            EmitZeroWidthRet(il);
            il.MarkLabel(fail); EmitFail(il);
        });

        AddBeta(tb, il => EmitFail(il));
        return tb.CreateType()!;
    }

    // ── bb_pos ───────────────────────────────────────────────────────────────
    // α: cursor!=n→fail; ZeroWidth
    // β: fail
    private static Type BuildPos()
    {
        var tb = NewBox("bb_pos");
        var fN = tb.DefineField("_n", TInt32, FieldAttributes.Private);

        AddBaseCtor(tb, new[]{TInt32}, il => {
            il.Emit(OpCodes.Ldarg_0); il.Emit(OpCodes.Call, CtorObject);
            il.Emit(OpCodes.Ldarg_0); il.Emit(OpCodes.Ldarg_1); il.Emit(OpCodes.Stfld, fN);
        });

        AddAlpha(tb, il => {
            var fail = il.DefineLabel();
            il.Emit(OpCodes.Ldarg_1); il.Emit(OpCodes.Callvirt, MMS_CursorGet);
            il.Emit(OpCodes.Ldarg_0); il.Emit(OpCodes.Ldfld, fN);
            il.Emit(OpCodes.Bne_Un, fail);
            EmitZeroWidthRet(il);
            il.MarkLabel(fail); EmitFail(il);
        });

        AddBeta(tb, il => EmitFail(il));
        return tb.CreateType()!;
    }

    // ── bb_rpos ──────────────────────────────────────────────────────────────
    // α: cursor != Length-n → fail; ZeroWidth
    private static Type BuildRpos()
    {
        var tb = NewBox("bb_rpos");
        var fN = tb.DefineField("_n", TInt32, FieldAttributes.Private);

        AddBaseCtor(tb, new[]{TInt32}, il => {
            il.Emit(OpCodes.Ldarg_0); il.Emit(OpCodes.Call, CtorObject);
            il.Emit(OpCodes.Ldarg_0); il.Emit(OpCodes.Ldarg_1); il.Emit(OpCodes.Stfld, fN);
        });

        AddAlpha(tb, il => {
            var fail = il.DefineLabel();
            il.Emit(OpCodes.Ldarg_1); il.Emit(OpCodes.Callvirt, MMS_CursorGet);
            il.Emit(OpCodes.Ldarg_1); il.Emit(OpCodes.Callvirt, MMS_Length);
            il.Emit(OpCodes.Ldarg_0); il.Emit(OpCodes.Ldfld, fN);
            il.Emit(OpCodes.Sub);
            il.Emit(OpCodes.Bne_Un, fail);
            EmitZeroWidthRet(il);
            il.MarkLabel(fail); EmitFail(il);
        });

        AddBeta(tb, il => EmitFail(il));
        return tb.CreateType()!;
    }

    // ── bb_len ───────────────────────────────────────────────────────────────
    // α: cursor+n>Length→fail; result=Of(cursor,n); cursor+=n; ret
    // β: cursor-=n; fail
    private static Type BuildLen()
    {
        var tb = NewBox("bb_len");
        var fN = tb.DefineField("_n", TInt32, FieldAttributes.Private);

        AddBaseCtor(tb, new[]{TInt32}, il => {
            il.Emit(OpCodes.Ldarg_0); il.Emit(OpCodes.Call, CtorObject);
            il.Emit(OpCodes.Ldarg_0); il.Emit(OpCodes.Ldarg_1); il.Emit(OpCodes.Stfld, fN);
        });

        AddAlpha(tb, il => {
            var fail = il.DefineLabel();
            var r    = il.DeclareLocal(TSpec);
            il.Emit(OpCodes.Ldarg_1); il.Emit(OpCodes.Callvirt, MMS_CursorGet);
            il.Emit(OpCodes.Ldarg_0); il.Emit(OpCodes.Ldfld, fN);
            il.Emit(OpCodes.Add);
            il.Emit(OpCodes.Ldarg_1); il.Emit(OpCodes.Callvirt, MMS_Length);
            il.Emit(OpCodes.Bgt, fail);
            il.Emit(OpCodes.Ldarg_1); il.Emit(OpCodes.Callvirt, MMS_CursorGet);
            il.Emit(OpCodes.Ldarg_0); il.Emit(OpCodes.Ldfld, fN);
            il.Emit(OpCodes.Call, MSpec_Of); il.Emit(OpCodes.Stloc, r);
            il.Emit(OpCodes.Ldarg_1);
            il.Emit(OpCodes.Ldarg_1); il.Emit(OpCodes.Callvirt, MMS_CursorGet);
            il.Emit(OpCodes.Ldarg_0); il.Emit(OpCodes.Ldfld, fN);
            il.Emit(OpCodes.Add); il.Emit(OpCodes.Callvirt, MMS_CursorSet);
            il.Emit(OpCodes.Ldloc, r); il.Emit(OpCodes.Ret);
            il.MarkLabel(fail); EmitFail(il);
        });

        AddBeta(tb, il => {
            il.Emit(OpCodes.Ldarg_1);
            il.Emit(OpCodes.Ldarg_1); il.Emit(OpCodes.Callvirt, MMS_CursorGet);
            il.Emit(OpCodes.Ldarg_0); il.Emit(OpCodes.Ldfld, fN);
            il.Emit(OpCodes.Sub); il.Emit(OpCodes.Callvirt, MMS_CursorSet);
            EmitFail(il);
        });

        return tb.CreateType()!;
    }

    // ── bb_tab ───────────────────────────────────────────────────────────────
    // α: cursor>n→fail; advance=n-cursor; result=Of(cursor,advance); cursor=n; ret
    // β: cursor-=advance; fail
    private static Type BuildTab()
    {
        var tb  = NewBox("bb_tab");
        var fN  = tb.DefineField("_n",       TInt32, FieldAttributes.Private);
        var fAd = tb.DefineField("_advance", TInt32, FieldAttributes.Private);

        AddBaseCtor(tb, new[]{TInt32}, il => {
            il.Emit(OpCodes.Ldarg_0); il.Emit(OpCodes.Call, CtorObject);
            il.Emit(OpCodes.Ldarg_0); il.Emit(OpCodes.Ldarg_1); il.Emit(OpCodes.Stfld, fN);
        });

        AddAlpha(tb, il => {
            var fail = il.DefineLabel();
            var r    = il.DeclareLocal(TSpec);
            il.Emit(OpCodes.Ldarg_1); il.Emit(OpCodes.Callvirt, MMS_CursorGet);
            il.Emit(OpCodes.Ldarg_0); il.Emit(OpCodes.Ldfld, fN);
            il.Emit(OpCodes.Bgt, fail);
            // advance = n - cursor
            il.Emit(OpCodes.Ldarg_0);
            il.Emit(OpCodes.Ldarg_0); il.Emit(OpCodes.Ldfld, fN);
            il.Emit(OpCodes.Ldarg_1); il.Emit(OpCodes.Callvirt, MMS_CursorGet);
            il.Emit(OpCodes.Sub); il.Emit(OpCodes.Stfld, fAd);
            // result = Of(cursor, advance)
            il.Emit(OpCodes.Ldarg_1); il.Emit(OpCodes.Callvirt, MMS_CursorGet);
            il.Emit(OpCodes.Ldarg_0); il.Emit(OpCodes.Ldfld, fAd);
            il.Emit(OpCodes.Call, MSpec_Of); il.Emit(OpCodes.Stloc, r);
            // cursor = n
            il.Emit(OpCodes.Ldarg_1);
            il.Emit(OpCodes.Ldarg_0); il.Emit(OpCodes.Ldfld, fN);
            il.Emit(OpCodes.Callvirt, MMS_CursorSet);
            il.Emit(OpCodes.Ldloc, r); il.Emit(OpCodes.Ret);
            il.MarkLabel(fail); EmitFail(il);
        });

        AddBeta(tb, il => {
            il.Emit(OpCodes.Ldarg_1);
            il.Emit(OpCodes.Ldarg_1); il.Emit(OpCodes.Callvirt, MMS_CursorGet);
            il.Emit(OpCodes.Ldarg_0); il.Emit(OpCodes.Ldfld, fAd);
            il.Emit(OpCodes.Sub); il.Emit(OpCodes.Callvirt, MMS_CursorSet);
            EmitFail(il);
        });

        return tb.CreateType()!;
    }

    // ── bb_rtab ──────────────────────────────────────────────────────────────
    // α: target=Length-n; cursor>target→fail; advance=target-cursor; cursor=target; γ
    // β: cursor-=advance; fail
    private static Type BuildRtab()
    {
        var tb  = NewBox("bb_rtab");
        var fN  = tb.DefineField("_n",       TInt32, FieldAttributes.Private);
        var fAd = tb.DefineField("_advance", TInt32, FieldAttributes.Private);

        AddBaseCtor(tb, new[]{TInt32}, il => {
            il.Emit(OpCodes.Ldarg_0); il.Emit(OpCodes.Call, CtorObject);
            il.Emit(OpCodes.Ldarg_0); il.Emit(OpCodes.Ldarg_1); il.Emit(OpCodes.Stfld, fN);
        });

        AddAlpha(tb, il => {
            var fail   = il.DefineLabel();
            var r      = il.DeclareLocal(TSpec);
            var target = il.DeclareLocal(TInt32);
            // target = Length - n
            il.Emit(OpCodes.Ldarg_1); il.Emit(OpCodes.Callvirt, MMS_Length);
            il.Emit(OpCodes.Ldarg_0); il.Emit(OpCodes.Ldfld, fN);
            il.Emit(OpCodes.Sub); il.Emit(OpCodes.Stloc, target);
            // cursor > target → fail
            il.Emit(OpCodes.Ldarg_1); il.Emit(OpCodes.Callvirt, MMS_CursorGet);
            il.Emit(OpCodes.Ldloc, target);
            il.Emit(OpCodes.Bgt, fail);
            // advance = target - cursor
            il.Emit(OpCodes.Ldarg_0);
            il.Emit(OpCodes.Ldloc, target);
            il.Emit(OpCodes.Ldarg_1); il.Emit(OpCodes.Callvirt, MMS_CursorGet);
            il.Emit(OpCodes.Sub); il.Emit(OpCodes.Stfld, fAd);
            // result = Of(cursor, advance)
            il.Emit(OpCodes.Ldarg_1); il.Emit(OpCodes.Callvirt, MMS_CursorGet);
            il.Emit(OpCodes.Ldarg_0); il.Emit(OpCodes.Ldfld, fAd);
            il.Emit(OpCodes.Call, MSpec_Of); il.Emit(OpCodes.Stloc, r);
            // cursor = target
            il.Emit(OpCodes.Ldarg_1); il.Emit(OpCodes.Ldloc, target); il.Emit(OpCodes.Callvirt, MMS_CursorSet);
            il.Emit(OpCodes.Ldloc, r); il.Emit(OpCodes.Ret);
            il.MarkLabel(fail); EmitFail(il);
        });

        AddBeta(tb, il => {
            il.Emit(OpCodes.Ldarg_1);
            il.Emit(OpCodes.Ldarg_1); il.Emit(OpCodes.Callvirt, MMS_CursorGet);
            il.Emit(OpCodes.Ldarg_0); il.Emit(OpCodes.Ldfld, fAd);
            il.Emit(OpCodes.Sub); il.Emit(OpCodes.Callvirt, MMS_CursorSet);
            EmitFail(il);
        });

        return tb.CreateType()!;
    }

    // ── bb_rem ───────────────────────────────────────────────────────────────
    // α: len=Length-cursor; result=Of(cursor,len); cursor=Length; γ
    // β: fail
    private static Type BuildRem()
    {
        var tb = NewBox("bb_rem");
        AddBaseCtor(tb, Type.EmptyTypes, il => {
            il.Emit(OpCodes.Ldarg_0); il.Emit(OpCodes.Call, CtorObject);
        });
        AddAlpha(tb, il => {
            var r   = il.DeclareLocal(TSpec);
            var len = il.DeclareLocal(TInt32);
            // len = Length - cursor
            il.Emit(OpCodes.Ldarg_1); il.Emit(OpCodes.Callvirt, MMS_Length);
            il.Emit(OpCodes.Ldarg_1); il.Emit(OpCodes.Callvirt, MMS_CursorGet);
            il.Emit(OpCodes.Sub); il.Emit(OpCodes.Stloc, len);
            // result = Of(cursor, len)
            il.Emit(OpCodes.Ldarg_1); il.Emit(OpCodes.Callvirt, MMS_CursorGet);
            il.Emit(OpCodes.Ldloc, len);
            il.Emit(OpCodes.Call, MSpec_Of); il.Emit(OpCodes.Stloc, r);
            // cursor = Length
            il.Emit(OpCodes.Ldarg_1); il.Emit(OpCodes.Ldarg_1); il.Emit(OpCodes.Callvirt, MMS_Length);
            il.Emit(OpCodes.Callvirt, MMS_CursorSet);
            il.Emit(OpCodes.Ldloc, r); il.Emit(OpCodes.Ret);
        });
        AddBeta(tb, il => EmitFail(il));
        return tb.CreateType()!;
    }

    // ── bb_any ───────────────────────────────────────────────────────────────
    // α: !CharInSet(cursor,chars)→fail; result=Of(cursor,1); cursor++; γ
    // β: cursor--; fail
    private static Type BuildAny()
    {
        var tb  = NewBox("bb_any");
        var fCh = tb.DefineField("_chars", TString, FieldAttributes.Private);
        AddBaseCtor(tb, new[]{TString}, il => EmitStringFieldCtor(il, fCh));
        AddAlpha(tb, il => EmitCharSetAlpha(il, fCh, invert: false));
        AddBeta(tb,  il => EmitCharSetBeta(il));
        return tb.CreateType()!;
    }

    // ── bb_notany ────────────────────────────────────────────────────────────
    // α: CharInSet(cursor,chars)→fail; result=Of(cursor,1); cursor++; γ
    // β: cursor--; fail
    private static Type BuildNotany()
    {
        var tb  = NewBox("bb_notany");
        var fCh = tb.DefineField("_chars", TString, FieldAttributes.Private);
        AddBaseCtor(tb, new[]{TString}, il => EmitStringFieldCtor(il, fCh));
        AddAlpha(tb, il => EmitCharSetAlpha(il, fCh, invert: true));
        AddBeta(tb,  il => EmitCharSetBeta(il));
        return tb.CreateType()!;
    }

    private static void EmitStringFieldCtor(ILGenerator il, FieldInfo f)
    {
        il.Emit(OpCodes.Ldarg_0); il.Emit(OpCodes.Call, CtorObject);
        il.Emit(OpCodes.Ldarg_0); il.Emit(OpCodes.Ldarg_1);
        var notNull = il.DefineLabel();
        il.Emit(OpCodes.Dup); il.Emit(OpCodes.Brtrue_S, notNull);
        il.Emit(OpCodes.Pop); il.Emit(OpCodes.Ldstr, "");
        il.MarkLabel(notNull);
        il.Emit(OpCodes.Stfld, f);
    }

    private static void EmitCharSetAlpha(ILGenerator il, FieldInfo fCh, bool invert)
    {
        var fail = il.DefineLabel();
        var r    = il.DeclareLocal(TSpec);
        il.Emit(OpCodes.Ldarg_1);
        il.Emit(OpCodes.Ldarg_1); il.Emit(OpCodes.Callvirt, MMS_CursorGet);
        il.Emit(OpCodes.Ldarg_0); il.Emit(OpCodes.Ldfld, fCh);
        il.Emit(OpCodes.Callvirt, MMS_CharIn);
        // invert=false(ANY): brfalse→fail; invert=true(NOTANY): brtrue→fail
        if (invert) il.Emit(OpCodes.Brtrue,  fail);
        else        il.Emit(OpCodes.Brfalse, fail);
        // result = Of(cursor, 1); cursor++
        il.Emit(OpCodes.Ldarg_1); il.Emit(OpCodes.Callvirt, MMS_CursorGet);
        il.Emit(OpCodes.Ldc_I4_1); il.Emit(OpCodes.Call, MSpec_Of); il.Emit(OpCodes.Stloc, r);
        il.Emit(OpCodes.Ldarg_1);
        il.Emit(OpCodes.Ldarg_1); il.Emit(OpCodes.Callvirt, MMS_CursorGet);
        il.Emit(OpCodes.Ldc_I4_1); il.Emit(OpCodes.Add); il.Emit(OpCodes.Callvirt, MMS_CursorSet);
        il.Emit(OpCodes.Ldloc, r); il.Emit(OpCodes.Ret);
        il.MarkLabel(fail); EmitFail(il);
    }

    private static void EmitCharSetBeta(ILGenerator il)
    {
        il.Emit(OpCodes.Ldarg_1);
        il.Emit(OpCodes.Ldarg_1); il.Emit(OpCodes.Callvirt, MMS_CursorGet);
        il.Emit(OpCodes.Ldc_I4_1); il.Emit(OpCodes.Sub); il.Emit(OpCodes.Callvirt, MMS_CursorSet);
        EmitFail(il);
    }

    // ── bb_span ──────────────────────────────────────────────────────────────
    // α: count=0; loop while CharInSet(cursor+count): count++; if count<=0→fail; cursor+=count; γ
    // β: cursor-=count; fail
    private static Type BuildSpan()
    {
        var tb  = NewBox("bb_span");
        var fCh = tb.DefineField("_chars", TString, FieldAttributes.Private);
        var fCt = tb.DefineField("_count", TInt32,  FieldAttributes.Private);
        AddBaseCtor(tb, new[]{TString}, il => EmitStringFieldCtor(il, fCh));
        AddAlpha(tb, il => EmitScanAlpha(il, fCh, fCt, breakOnFound: false, requireNonZero: true));
        AddBeta(tb,  il => EmitScanBeta(il, fCt));
        return tb.CreateType()!;
    }

    // ── bb_brk ───────────────────────────────────────────────────────────────
    // α: count=0; loop while !CharInSet && !EOS: count++; if EOS→fail; cursor+=count; γ
    // β: cursor-=count; fail
    private static Type BuildBrk()
    {
        var tb  = NewBox("bb_brk");
        var fCh = tb.DefineField("_chars", TString, FieldAttributes.Private);
        var fCt = tb.DefineField("_count", TInt32,  FieldAttributes.Private);
        AddBaseCtor(tb, new[]{TString}, il => EmitStringFieldCtor(il, fCh));
        AddAlpha(tb, il => EmitScanAlpha(il, fCh, fCt, breakOnFound: true, requireNonZero: false));
        AddBeta(tb,  il => EmitScanBeta(il, fCt));
        return tb.CreateType()!;
    }

    // ── bb_breakx ────────────────────────────────────────────────────────────
    // Like brk but also fails if count==0
    private static Type BuildBreakx()
    {
        var tb  = NewBox("bb_breakx");
        var fCh = tb.DefineField("_chars", TString, FieldAttributes.Private);
        var fCt = tb.DefineField("_count", TInt32,  FieldAttributes.Private);
        AddBaseCtor(tb, new[]{TString}, il => EmitStringFieldCtor(il, fCh));
        AddAlpha(tb, il => EmitScanAlpha(il, fCh, fCt, breakOnFound: true, requireNonZero: true));
        AddBeta(tb,  il => EmitScanBeta(il, fCt));
        return tb.CreateType()!;
    }

    // Shared scan loop for span/brk/breakx
    // breakOnFound=false → span (loop while in-set, stop when not-in-set or EOS)
    // breakOnFound=true  → brk/breakx (loop while NOT in-set and not EOS, stop at set-char)
    // requireNonZero     → fail if count==0 (span and breakx)
    private static void EmitScanAlpha(ILGenerator il, FieldInfo fCh, FieldInfo fCt,
                                       bool breakOnFound, bool requireNonZero)
    {
        var loopTop = il.DefineLabel();
        var done    = il.DefineLabel();
        var fail    = il.DefineLabel();
        var r       = il.DeclareLocal(TSpec);

        // _count = 0
        il.Emit(OpCodes.Ldarg_0); il.Emit(OpCodes.Ldc_I4_0); il.Emit(OpCodes.Stfld, fCt);

        il.MarkLabel(loopTop);

        if (breakOnFound)
        {
            // brk/breakx: loop while cursor+count < Length && !CharInSet
            // if cursor+count >= Length → EOS path (fail for brk; fail for breakx)
            il.Emit(OpCodes.Ldarg_1); il.Emit(OpCodes.Callvirt, MMS_CursorGet);
            il.Emit(OpCodes.Ldarg_0); il.Emit(OpCodes.Ldfld, fCt);
            il.Emit(OpCodes.Add);
            il.Emit(OpCodes.Ldarg_1); il.Emit(OpCodes.Callvirt, MMS_Length);
            il.Emit(OpCodes.Bge, fail);  // EOS → fail
            // if CharInSet(cursor+count) → found
            il.Emit(OpCodes.Ldarg_1);
            il.Emit(OpCodes.Ldarg_1); il.Emit(OpCodes.Callvirt, MMS_CursorGet);
            il.Emit(OpCodes.Ldarg_0); il.Emit(OpCodes.Ldfld, fCt);
            il.Emit(OpCodes.Add);
            il.Emit(OpCodes.Ldarg_0); il.Emit(OpCodes.Ldfld, fCh);
            il.Emit(OpCodes.Callvirt, MMS_CharIn);
            il.Emit(OpCodes.Brtrue, done);  // found set-char → stop loop
        }
        else
        {
            // span: loop while CharInSet(cursor+count); brfalse → done
            il.Emit(OpCodes.Ldarg_1);
            il.Emit(OpCodes.Ldarg_1); il.Emit(OpCodes.Callvirt, MMS_CursorGet);
            il.Emit(OpCodes.Ldarg_0); il.Emit(OpCodes.Ldfld, fCt);
            il.Emit(OpCodes.Add);
            il.Emit(OpCodes.Ldarg_0); il.Emit(OpCodes.Ldfld, fCh);
            il.Emit(OpCodes.Callvirt, MMS_CharIn);
            il.Emit(OpCodes.Brfalse, done);
        }

        // count++
        il.Emit(OpCodes.Ldarg_0);
        il.Emit(OpCodes.Ldarg_0); il.Emit(OpCodes.Ldfld, fCt);
        il.Emit(OpCodes.Ldc_I4_1); il.Emit(OpCodes.Add); il.Emit(OpCodes.Stfld, fCt);
        il.Emit(OpCodes.Br, loopTop);

        il.MarkLabel(done);

        if (requireNonZero)
        {
            // if count <= 0 → fail
            il.Emit(OpCodes.Ldarg_0); il.Emit(OpCodes.Ldfld, fCt);
            il.Emit(OpCodes.Ldc_I4_0); il.Emit(OpCodes.Ble, fail);
        }

        // result = Of(cursor, count); cursor += count; ret
        il.Emit(OpCodes.Ldarg_1); il.Emit(OpCodes.Callvirt, MMS_CursorGet);
        il.Emit(OpCodes.Ldarg_0); il.Emit(OpCodes.Ldfld, fCt);
        il.Emit(OpCodes.Call, MSpec_Of); il.Emit(OpCodes.Stloc, r);
        il.Emit(OpCodes.Ldarg_1);
        il.Emit(OpCodes.Ldarg_1); il.Emit(OpCodes.Callvirt, MMS_CursorGet);
        il.Emit(OpCodes.Ldarg_0); il.Emit(OpCodes.Ldfld, fCt);
        il.Emit(OpCodes.Add); il.Emit(OpCodes.Callvirt, MMS_CursorSet);
        il.Emit(OpCodes.Ldloc, r); il.Emit(OpCodes.Ret);

        il.MarkLabel(fail); EmitFail(il);
    }

    private static void EmitScanBeta(ILGenerator il, FieldInfo fCt)
    {
        il.Emit(OpCodes.Ldarg_1);
        il.Emit(OpCodes.Ldarg_1); il.Emit(OpCodes.Callvirt, MMS_CursorGet);
        il.Emit(OpCodes.Ldarg_0); il.Emit(OpCodes.Ldfld, fCt);
        il.Emit(OpCodes.Sub); il.Emit(OpCodes.Callvirt, MMS_CursorSet);
        EmitFail(il);
    }

    // ── bb_arb ───────────────────────────────────────────────────────────────
    // α: count=0; start=cursor; ZeroWidth (lazy — zero first)
    // β: count++; if start+count>Length→fail; cursor=start+count; Of(start,count)
    private static Type BuildArb()
    {
        var tb  = NewBox("bb_arb");
        var fCt = tb.DefineField("_count", TInt32, FieldAttributes.Private);
        var fSt = tb.DefineField("_start", TInt32, FieldAttributes.Private);

        AddBaseCtor(tb, Type.EmptyTypes, il => {
            il.Emit(OpCodes.Ldarg_0); il.Emit(OpCodes.Call, CtorObject);
        });

        AddAlpha(tb, il => {
            il.Emit(OpCodes.Ldarg_0); il.Emit(OpCodes.Ldc_I4_0); il.Emit(OpCodes.Stfld, fCt);
            il.Emit(OpCodes.Ldarg_0);
            il.Emit(OpCodes.Ldarg_1); il.Emit(OpCodes.Callvirt, MMS_CursorGet);
            il.Emit(OpCodes.Stfld, fSt);
            EmitZeroWidthRet(il);
        });

        AddBeta(tb, il => {
            var fail = il.DefineLabel();
            // count++
            il.Emit(OpCodes.Ldarg_0);
            il.Emit(OpCodes.Ldarg_0); il.Emit(OpCodes.Ldfld, fCt);
            il.Emit(OpCodes.Ldc_I4_1); il.Emit(OpCodes.Add); il.Emit(OpCodes.Stfld, fCt);
            // if start+count > Length → fail
            il.Emit(OpCodes.Ldarg_0); il.Emit(OpCodes.Ldfld, fSt);
            il.Emit(OpCodes.Ldarg_0); il.Emit(OpCodes.Ldfld, fCt);
            il.Emit(OpCodes.Add);
            il.Emit(OpCodes.Ldarg_1); il.Emit(OpCodes.Callvirt, MMS_Length);
            il.Emit(OpCodes.Bgt, fail);
            // cursor = start + count
            il.Emit(OpCodes.Ldarg_1);
            il.Emit(OpCodes.Ldarg_0); il.Emit(OpCodes.Ldfld, fSt);
            il.Emit(OpCodes.Ldarg_0); il.Emit(OpCodes.Ldfld, fCt);
            il.Emit(OpCodes.Add); il.Emit(OpCodes.Callvirt, MMS_CursorSet);
            // return Of(start, count)
            il.Emit(OpCodes.Ldarg_0); il.Emit(OpCodes.Ldfld, fSt);
            il.Emit(OpCodes.Ldarg_0); il.Emit(OpCodes.Ldfld, fCt);
            il.Emit(OpCodes.Call, MSpec_Of); il.Emit(OpCodes.Ret);
            il.MarkLabel(fail); EmitFail(il);
        });

        return tb.CreateType()!;
    }

    // ── bb_seq ───────────────────────────────────────────────────────────────
    // Delegate entirely to the existing C# bb_seq (complex state machine —
    // reuse rather than re-emit, since bb_seq.cs is the verified reference)
    // NOTE: bb_seq.cs was deleted; we use the IL-verified logic inline here.
    private static Type BuildSeq()
    {
        var tb   = NewBox("bb_seq");
        var fL   = tb.DefineField("_left",   TIByrdBox, FieldAttributes.Private);
        var fR   = tb.DefineField("_right",  TIByrdBox, FieldAttributes.Private);
        var fMS  = tb.DefineField("_mStart", TInt32,    FieldAttributes.Private);
        var fML  = tb.DefineField("_mLen",   TInt32,    FieldAttributes.Private);

        AddBaseCtor(tb, new[]{TIByrdBox, TIByrdBox}, il => {
            il.Emit(OpCodes.Ldarg_0); il.Emit(OpCodes.Call, CtorObject);
            il.Emit(OpCodes.Ldarg_0); il.Emit(OpCodes.Ldarg_1); il.Emit(OpCodes.Stfld, fL);
            il.Emit(OpCodes.Ldarg_0); il.Emit(OpCodes.Ldarg_2); il.Emit(OpCodes.Stfld, fR);
        });

        AddAlpha(tb, il => {
            var lrLoc  = il.DeclareLocal(TSpec);
            var rrLoc  = il.DeclareLocal(TSpec);
            var omega  = il.DefineLabel();
            var tryR   = il.DefineLabel();
            var gamma  = il.DefineLabel();
            var lbeta  = il.DefineLabel();

            // _mStart = cursor
            il.Emit(OpCodes.Ldarg_0);
            il.Emit(OpCodes.Ldarg_1); il.Emit(OpCodes.Callvirt, MMS_CursorGet);
            il.Emit(OpCodes.Stfld, fMS);
            // lr = _left.α(ms)
            il.Emit(OpCodes.Ldarg_0); il.Emit(OpCodes.Ldfld, fL);
            il.Emit(OpCodes.Ldarg_1); il.Emit(OpCodes.Callvirt, MIBox_Alpha);
            il.Emit(OpCodes.Stloc, lrLoc);
            EmitIsFail(il, lrLoc); il.Emit(OpCodes.Brtrue, omega);
            // _mLen = lr.Length
            il.Emit(OpCodes.Ldarg_0); il.Emit(OpCodes.Ldloca_S, lrLoc);
            il.Emit(OpCodes.Ldfld, FSpec_Length); il.Emit(OpCodes.Stfld, fML);

            il.MarkLabel(tryR);
            // rr = _right.α(ms)
            il.Emit(OpCodes.Ldarg_0); il.Emit(OpCodes.Ldfld, fR);
            il.Emit(OpCodes.Ldarg_1); il.Emit(OpCodes.Callvirt, MIBox_Alpha);
            il.Emit(OpCodes.Stloc, rrLoc);
            EmitIsFail(il, rrLoc); il.Emit(OpCodes.Brfalse, gamma);

            il.MarkLabel(lbeta);
            // lr = _left.β(ms)
            il.Emit(OpCodes.Ldarg_0); il.Emit(OpCodes.Ldfld, fL);
            il.Emit(OpCodes.Ldarg_1); il.Emit(OpCodes.Callvirt, MIBox_Beta);
            il.Emit(OpCodes.Stloc, lrLoc);
            EmitIsFail(il, lrLoc); il.Emit(OpCodes.Brtrue, omega);
            // _mLen = lr.Length (reset, not accumulate)
            il.Emit(OpCodes.Ldarg_0); il.Emit(OpCodes.Ldloca_S, lrLoc);
            il.Emit(OpCodes.Ldfld, FSpec_Length); il.Emit(OpCodes.Stfld, fML);
            il.Emit(OpCodes.Br, tryR);

            il.MarkLabel(gamma);
            // return Of(_mStart, _mLen + rr.Length)
            il.Emit(OpCodes.Ldarg_0); il.Emit(OpCodes.Ldfld, fMS);
            il.Emit(OpCodes.Ldarg_0); il.Emit(OpCodes.Ldfld, fML);
            il.Emit(OpCodes.Ldloca_S, rrLoc); il.Emit(OpCodes.Ldfld, FSpec_Length);
            il.Emit(OpCodes.Add); il.Emit(OpCodes.Call, MSpec_Of); il.Emit(OpCodes.Ret);

            il.MarkLabel(omega); EmitFail(il);
        });

        AddBeta(tb, il => {
            var rrLoc  = il.DeclareLocal(TSpec);
            var lrLoc  = il.DeclareLocal(TSpec);
            var gamma  = il.DefineLabel();
            var lbeta  = il.DefineLabel();
            var omega  = il.DefineLabel();

            // rr = _right.β(ms)
            il.Emit(OpCodes.Ldarg_0); il.Emit(OpCodes.Ldfld, fR);
            il.Emit(OpCodes.Ldarg_1); il.Emit(OpCodes.Callvirt, MIBox_Beta);
            il.Emit(OpCodes.Stloc, rrLoc);
            EmitIsFail(il, rrLoc); il.Emit(OpCodes.Brfalse, gamma);

            il.MarkLabel(lbeta);
            // lr = _left.β(ms)
            il.Emit(OpCodes.Ldarg_0); il.Emit(OpCodes.Ldfld, fL);
            il.Emit(OpCodes.Ldarg_1); il.Emit(OpCodes.Callvirt, MIBox_Beta);
            il.Emit(OpCodes.Stloc, lrLoc);
            EmitIsFail(il, lrLoc); il.Emit(OpCodes.Brtrue, omega);
            // _mLen = lr.Length
            il.Emit(OpCodes.Ldarg_0); il.Emit(OpCodes.Ldloca_S, lrLoc);
            il.Emit(OpCodes.Ldfld, FSpec_Length); il.Emit(OpCodes.Stfld, fML);
            // rr = _right.α(ms)
            il.Emit(OpCodes.Ldarg_0); il.Emit(OpCodes.Ldfld, fR);
            il.Emit(OpCodes.Ldarg_1); il.Emit(OpCodes.Callvirt, MIBox_Alpha);
            il.Emit(OpCodes.Stloc, rrLoc);
            EmitIsFail(il, rrLoc); il.Emit(OpCodes.Brtrue, lbeta);

            il.MarkLabel(gamma);
            il.Emit(OpCodes.Ldarg_0); il.Emit(OpCodes.Ldfld, fMS);
            il.Emit(OpCodes.Ldarg_0); il.Emit(OpCodes.Ldfld, fML);
            il.Emit(OpCodes.Ldloca_S, rrLoc); il.Emit(OpCodes.Ldfld, FSpec_Length);
            il.Emit(OpCodes.Add); il.Emit(OpCodes.Call, MSpec_Of); il.Emit(OpCodes.Ret);

            il.MarkLabel(omega); EmitFail(il);
        });

        return tb.CreateType()!;
    }

    // ── bb_alt ───────────────────────────────────────────────────────────────
    // α: save cursor; try children[i].α in order; on γ store _current=i; ret
    // β: retry children[_current].β only
    private static Type BuildAlt()
    {
        var tb  = NewBox("bb_alt");
        var fCh = tb.DefineField("_children",  typeof(IByrdBox[]), FieldAttributes.Private);
        var fCu = tb.DefineField("_current",   TInt32,             FieldAttributes.Private);
        var fSP = tb.DefineField("_savedPos",  TInt32,             FieldAttributes.Private);

        AddBaseCtor(tb, new[]{typeof(IByrdBox[])}, il => {
            il.Emit(OpCodes.Ldarg_0); il.Emit(OpCodes.Call, CtorObject);
            il.Emit(OpCodes.Ldarg_0); il.Emit(OpCodes.Ldarg_1); il.Emit(OpCodes.Stfld, fCh);
        });

        AddAlpha(tb, il => {
            var i    = il.DeclareLocal(TInt32);
            var cr   = il.DeclareLocal(TSpec);
            var loop = il.DefineLabel();
            var next = il.DefineLabel();
            var fail = il.DefineLabel();

            // _savedPos = cursor
            il.Emit(OpCodes.Ldarg_0);
            il.Emit(OpCodes.Ldarg_1); il.Emit(OpCodes.Callvirt, MMS_CursorGet);
            il.Emit(OpCodes.Stfld, fSP);
            il.Emit(OpCodes.Ldc_I4_0); il.Emit(OpCodes.Stloc, i);

            il.MarkLabel(loop);
            // if i >= children.Length → fail
            il.Emit(OpCodes.Ldloc, i);
            il.Emit(OpCodes.Ldarg_0); il.Emit(OpCodes.Ldfld, fCh);
            il.Emit(OpCodes.Ldlen); il.Emit(OpCodes.Conv_I4);
            il.Emit(OpCodes.Bge, fail);
            // cursor = _savedPos
            il.Emit(OpCodes.Ldarg_1);
            il.Emit(OpCodes.Ldarg_0); il.Emit(OpCodes.Ldfld, fSP);
            il.Emit(OpCodes.Callvirt, MMS_CursorSet);
            // cr = children[i].α(ms)
            il.Emit(OpCodes.Ldarg_0); il.Emit(OpCodes.Ldfld, fCh);
            il.Emit(OpCodes.Ldloc, i); il.Emit(OpCodes.Ldelem_Ref);
            il.Emit(OpCodes.Ldarg_1); il.Emit(OpCodes.Callvirt, MIBox_Alpha);
            il.Emit(OpCodes.Stloc, cr);
            EmitIsFail(il, cr); il.Emit(OpCodes.Brtrue, next);
            // success: _current = i; return cr
            il.Emit(OpCodes.Ldarg_0); il.Emit(OpCodes.Ldloc, i); il.Emit(OpCodes.Stfld, fCu);
            il.Emit(OpCodes.Ldloc, cr); il.Emit(OpCodes.Ret);

            il.MarkLabel(next);
            il.Emit(OpCodes.Ldloc, i); il.Emit(OpCodes.Ldc_I4_1); il.Emit(OpCodes.Add);
            il.Emit(OpCodes.Stloc, i); il.Emit(OpCodes.Br, loop);

            il.MarkLabel(fail);
            il.Emit(OpCodes.Ldarg_1);
            il.Emit(OpCodes.Ldarg_0); il.Emit(OpCodes.Ldfld, fSP);
            il.Emit(OpCodes.Callvirt, MMS_CursorSet);
            EmitFail(il);
        });

        AddBeta(tb, il => {
            var cr   = il.DeclareLocal(TSpec);
            var fail = il.DefineLabel();
            il.Emit(OpCodes.Ldarg_0); il.Emit(OpCodes.Ldfld, fCh);
            il.Emit(OpCodes.Ldarg_0); il.Emit(OpCodes.Ldfld, fCu);
            il.Emit(OpCodes.Ldelem_Ref);
            il.Emit(OpCodes.Ldarg_1); il.Emit(OpCodes.Callvirt, MIBox_Beta);
            il.Emit(OpCodes.Stloc, cr);
            EmitIsFail(il, cr); il.Emit(OpCodes.Brtrue, fail);
            il.Emit(OpCodes.Ldloc, cr); il.Emit(OpCodes.Ret);
            il.MarkLabel(fail); EmitFail(il);
        });

        return tb.CreateType()!;
    }

    // ── bb_arbno ─────────────────────────────────────────────────────────────
    // Greedy stack: push frames; β unwinds one frame at a time
    private static Type BuildArbno()
    {
        var tb  = NewBox("bb_arbno");
        var fBd = tb.DefineField("_body",       TIByrdBox,      FieldAttributes.Private);
        var fMS = tb.DefineField("_matchStart", typeof(int[]),  FieldAttributes.Private);
        var fML = tb.DefineField("_matchLen",   typeof(int[]),  FieldAttributes.Private);
        var fSS = tb.DefineField("_startStack", typeof(int[]),  FieldAttributes.Private);
        var fDp = tb.DefineField("_depth",      TInt32,         FieldAttributes.Private);

        AddBaseCtor(tb, new[]{TIByrdBox}, il => {
            il.Emit(OpCodes.Ldarg_0); il.Emit(OpCodes.Call, CtorObject);
            il.Emit(OpCodes.Ldarg_0); il.Emit(OpCodes.Ldarg_1); il.Emit(OpCodes.Stfld, fBd);
            foreach (var f in new[]{fMS, fML, fSS})
            {
                il.Emit(OpCodes.Ldarg_0); il.Emit(OpCodes.Ldc_I4, 64);
                il.Emit(OpCodes.Newarr, TInt32); il.Emit(OpCodes.Stfld, f);
            }
        });

        AddAlpha(tb, il => {
            var startHere = il.DeclareLocal(TInt32);
            var br        = il.DeclareLocal(TSpec);
            var loop      = il.DefineLabel();
            var stop      = il.DefineLabel();

            // _depth=0; _matchStart[0]=cursor; _matchLen[0]=0; _startStack[0]=cursor
            il.Emit(OpCodes.Ldarg_0); il.Emit(OpCodes.Ldc_I4_0); il.Emit(OpCodes.Stfld, fDp);
            foreach (var (f, val) in new[]{(fMS,0),(fSS,0)})  // set matchStart[0] and startStack[0]
            {
                il.Emit(OpCodes.Ldarg_0); il.Emit(OpCodes.Ldfld, f);
                il.Emit(OpCodes.Ldc_I4_0);
                il.Emit(OpCodes.Ldarg_1); il.Emit(OpCodes.Callvirt, MMS_CursorGet);
                il.Emit(OpCodes.Stelem_I4);
            }
            il.Emit(OpCodes.Ldarg_0); il.Emit(OpCodes.Ldfld, fML);
            il.Emit(OpCodes.Ldc_I4_0); il.Emit(OpCodes.Ldc_I4_0); il.Emit(OpCodes.Stelem_I4);

            il.MarkLabel(loop);
            // startHere = cursor
            il.Emit(OpCodes.Ldarg_1); il.Emit(OpCodes.Callvirt, MMS_CursorGet); il.Emit(OpCodes.Stloc, startHere);
            // br = _body.α(ms)
            il.Emit(OpCodes.Ldarg_0); il.Emit(OpCodes.Ldfld, fBd);
            il.Emit(OpCodes.Ldarg_1); il.Emit(OpCodes.Callvirt, MIBox_Alpha);
            il.Emit(OpCodes.Stloc, br);
            // if fail → stop
            EmitIsFail(il, br); il.Emit(OpCodes.Brtrue, stop);
            // zero-advance guard: cursor == startHere → stop
            il.Emit(OpCodes.Ldarg_1); il.Emit(OpCodes.Callvirt, MMS_CursorGet);
            il.Emit(OpCodes.Ldloc, startHere); il.Emit(OpCodes.Beq, stop);
            // if depth >= 63 → stop
            il.Emit(OpCodes.Ldarg_0); il.Emit(OpCodes.Ldfld, fDp);
            il.Emit(OpCodes.Ldc_I4, 63); il.Emit(OpCodes.Bge, stop);
            // depth++
            il.Emit(OpCodes.Ldarg_0);
            il.Emit(OpCodes.Ldarg_0); il.Emit(OpCodes.Ldfld, fDp);
            il.Emit(OpCodes.Ldc_I4_1); il.Emit(OpCodes.Add); il.Emit(OpCodes.Stfld, fDp);
            // _matchStart[depth] = _matchStart[0]
            il.Emit(OpCodes.Ldarg_0); il.Emit(OpCodes.Ldfld, fMS);
            il.Emit(OpCodes.Ldarg_0); il.Emit(OpCodes.Ldfld, fDp);
            il.Emit(OpCodes.Ldarg_0); il.Emit(OpCodes.Ldfld, fMS);
            il.Emit(OpCodes.Ldc_I4_0); il.Emit(OpCodes.Ldelem_I4);
            il.Emit(OpCodes.Stelem_I4);
            // _matchLen[depth] = _matchLen[depth-1] + br.Length
            il.Emit(OpCodes.Ldarg_0); il.Emit(OpCodes.Ldfld, fML);
            il.Emit(OpCodes.Ldarg_0); il.Emit(OpCodes.Ldfld, fDp);
            il.Emit(OpCodes.Ldarg_0); il.Emit(OpCodes.Ldfld, fML);
            il.Emit(OpCodes.Ldarg_0); il.Emit(OpCodes.Ldfld, fDp);
            il.Emit(OpCodes.Ldc_I4_1); il.Emit(OpCodes.Sub); il.Emit(OpCodes.Ldelem_I4);
            il.Emit(OpCodes.Ldloca_S, br); il.Emit(OpCodes.Ldfld, FSpec_Length);
            il.Emit(OpCodes.Add); il.Emit(OpCodes.Stelem_I4);
            // _startStack[depth] = cursor
            il.Emit(OpCodes.Ldarg_0); il.Emit(OpCodes.Ldfld, fSS);
            il.Emit(OpCodes.Ldarg_0); il.Emit(OpCodes.Ldfld, fDp);
            il.Emit(OpCodes.Ldarg_1); il.Emit(OpCodes.Callvirt, MMS_CursorGet);
            il.Emit(OpCodes.Stelem_I4);
            il.Emit(OpCodes.Br, loop);

            il.MarkLabel(stop);
            // return Of(_matchStart[_depth], _matchLen[_depth])
            il.Emit(OpCodes.Ldarg_0); il.Emit(OpCodes.Ldfld, fMS);
            il.Emit(OpCodes.Ldarg_0); il.Emit(OpCodes.Ldfld, fDp); il.Emit(OpCodes.Ldelem_I4);
            il.Emit(OpCodes.Ldarg_0); il.Emit(OpCodes.Ldfld, fML);
            il.Emit(OpCodes.Ldarg_0); il.Emit(OpCodes.Ldfld, fDp); il.Emit(OpCodes.Ldelem_I4);
            il.Emit(OpCodes.Call, MSpec_Of); il.Emit(OpCodes.Ret);
        });

        AddBeta(tb, il => {
            var fail = il.DefineLabel();
            // if depth <= 0 → fail
            il.Emit(OpCodes.Ldarg_0); il.Emit(OpCodes.Ldfld, fDp);
            il.Emit(OpCodes.Ldc_I4_0); il.Emit(OpCodes.Ble, fail);
            // depth--
            il.Emit(OpCodes.Ldarg_0);
            il.Emit(OpCodes.Ldarg_0); il.Emit(OpCodes.Ldfld, fDp);
            il.Emit(OpCodes.Ldc_I4_1); il.Emit(OpCodes.Sub); il.Emit(OpCodes.Stfld, fDp);
            // cursor = _startStack[depth]
            il.Emit(OpCodes.Ldarg_1);
            il.Emit(OpCodes.Ldarg_0); il.Emit(OpCodes.Ldfld, fSS);
            il.Emit(OpCodes.Ldarg_0); il.Emit(OpCodes.Ldfld, fDp); il.Emit(OpCodes.Ldelem_I4);
            il.Emit(OpCodes.Callvirt, MMS_CursorSet);
            // return Of(_matchStart[depth], _matchLen[depth])
            il.Emit(OpCodes.Ldarg_0); il.Emit(OpCodes.Ldfld, fMS);
            il.Emit(OpCodes.Ldarg_0); il.Emit(OpCodes.Ldfld, fDp); il.Emit(OpCodes.Ldelem_I4);
            il.Emit(OpCodes.Ldarg_0); il.Emit(OpCodes.Ldfld, fML);
            il.Emit(OpCodes.Ldarg_0); il.Emit(OpCodes.Ldfld, fDp); il.Emit(OpCodes.Ldelem_I4);
            il.Emit(OpCodes.Call, MSpec_Of); il.Emit(OpCodes.Ret);
            il.MarkLabel(fail); EmitFail(il);
        });

        return tb.CreateType()!;
    }

    // ── bb_capture ───────────────────────────────────────────────────────────
    // Wraps existing bb_capture type — too delegate-heavy for clean re-emit.
    // Return the existing C# type directly (bb_box.cs infrastructure, kept).
    private static Type BuildCapture() => typeof(bb_capture);

    // ── bb_atp ───────────────────────────────────────────────────────────────
    private static Type BuildAtp() => typeof(bb_atp);

    // ── bb_dvar ──────────────────────────────────────────────────────────────
    private static Type BuildDvar() => typeof(bb_dvar);

    // ── bb_bal ───────────────────────────────────────────────────────────────
    private static Type BuildBal()
    {
        var tb  = NewBox("bb_bal");
        var fML = tb.DefineField("_matchedLen", TInt32, FieldAttributes.Private);

        AddBaseCtor(tb, Type.EmptyTypes, il => {
            il.Emit(OpCodes.Ldarg_0); il.Emit(OpCodes.Call, CtorObject);
        });

        AddAlpha(tb, il => {
            var pos   = il.DeclareLocal(TInt32);
            var depth = il.DeclareLocal(TInt32);
            var len   = il.DeclareLocal(TInt32);
            var c     = il.DeclareLocal(typeof(char));
            var start = il.DeclareLocal(TInt32);
            var loop  = il.DefineLabel();
            var chkCl = il.DefineLabel();
            var next  = il.DefineLabel();
            var done  = il.DefineLabel();
            var fail  = il.DefineLabel();

            // pos=cursor; depth=0; len=Length; start=pos; if pos>=len→fail
            il.Emit(OpCodes.Ldarg_1); il.Emit(OpCodes.Callvirt, MMS_CursorGet); il.Emit(OpCodes.Stloc, pos);
            il.Emit(OpCodes.Ldc_I4_0); il.Emit(OpCodes.Stloc, depth);
            il.Emit(OpCodes.Ldarg_1); il.Emit(OpCodes.Callvirt, MMS_Length); il.Emit(OpCodes.Stloc, len);
            il.Emit(OpCodes.Ldloc, pos); il.Emit(OpCodes.Stloc, start);
            il.Emit(OpCodes.Ldloc, pos); il.Emit(OpCodes.Ldloc, len); il.Emit(OpCodes.Bge, fail);

            il.MarkLabel(loop);
            // c = subject[pos]
            il.Emit(OpCodes.Ldarg_1); il.Emit(OpCodes.Callvirt, MMS_Subject);
            il.Emit(OpCodes.Ldloc, pos); il.Emit(OpCodes.Callvirt, MStr_GetChars);
            il.Emit(OpCodes.Stloc, c);
            // if c=='(' depth++
            il.Emit(OpCodes.Ldloc, c); il.Emit(OpCodes.Ldc_I4, (int)'(');
            il.Emit(OpCodes.Bne_Un, chkCl);
            il.Emit(OpCodes.Ldloc, depth); il.Emit(OpCodes.Ldc_I4_1); il.Emit(OpCodes.Add); il.Emit(OpCodes.Stloc, depth);
            il.Emit(OpCodes.Br, next);
            // if c==')': if depth==0→fail; depth--
            il.MarkLabel(chkCl);
            il.Emit(OpCodes.Ldloc, c); il.Emit(OpCodes.Ldc_I4, (int)')');
            il.Emit(OpCodes.Bne_Un, next);
            il.Emit(OpCodes.Ldloc, depth); il.Emit(OpCodes.Ldc_I4_0); il.Emit(OpCodes.Beq, fail);
            il.Emit(OpCodes.Ldloc, depth); il.Emit(OpCodes.Ldc_I4_1); il.Emit(OpCodes.Sub); il.Emit(OpCodes.Stloc, depth);
            il.MarkLabel(next);
            // pos++
            il.Emit(OpCodes.Ldloc, pos); il.Emit(OpCodes.Ldc_I4_1); il.Emit(OpCodes.Add); il.Emit(OpCodes.Stloc, pos);
            // while pos<len && depth>0
            il.Emit(OpCodes.Ldloc, pos); il.Emit(OpCodes.Ldloc, len); il.Emit(OpCodes.Bge, done);
            il.Emit(OpCodes.Ldloc, depth); il.Emit(OpCodes.Ldc_I4_0); il.Emit(OpCodes.Bgt, loop);

            il.MarkLabel(done);
            // if depth != 0 → fail
            il.Emit(OpCodes.Ldloc, depth); il.Emit(OpCodes.Ldc_I4_0); il.Emit(OpCodes.Bne_Un, fail);
            // _matchedLen = pos - start
            il.Emit(OpCodes.Ldarg_0);
            il.Emit(OpCodes.Ldloc, pos); il.Emit(OpCodes.Ldloc, start); il.Emit(OpCodes.Sub);
            il.Emit(OpCodes.Stfld, fML);
            // cursor = pos
            il.Emit(OpCodes.Ldarg_1); il.Emit(OpCodes.Ldloc, pos); il.Emit(OpCodes.Callvirt, MMS_CursorSet);
            // return Of(start, _matchedLen)
            il.Emit(OpCodes.Ldloc, start);
            il.Emit(OpCodes.Ldarg_0); il.Emit(OpCodes.Ldfld, fML);
            il.Emit(OpCodes.Call, MSpec_Of); il.Emit(OpCodes.Ret);

            il.MarkLabel(fail); EmitFail(il);
        });

        AddBeta(tb, il => {
            il.Emit(OpCodes.Ldarg_1);
            il.Emit(OpCodes.Ldarg_1); il.Emit(OpCodes.Callvirt, MMS_CursorGet);
            il.Emit(OpCodes.Ldarg_0); il.Emit(OpCodes.Ldfld, fML);
            il.Emit(OpCodes.Sub); il.Emit(OpCodes.Callvirt, MMS_CursorSet);
            EmitFail(il);
        });

        return tb.CreateType()!;
    }

    // ── Terminals (trivial) ──────────────────────────────────────────────────

    private static Type BuildFence()
    {
        var tb = NewBox("bb_fence");
        AddBaseCtor(tb, Type.EmptyTypes, il => { il.Emit(OpCodes.Ldarg_0); il.Emit(OpCodes.Call, CtorObject); });
        AddAlpha(tb, il => EmitZeroWidthRet(il));
        AddBeta(tb,  il => EmitFail(il));
        return tb.CreateType()!;
    }

    private static Type BuildAbort()
    {
        var tb = NewBox("bb_abort");
        AddBaseCtor(tb, Type.EmptyTypes, il => { il.Emit(OpCodes.Ldarg_0); il.Emit(OpCodes.Call, CtorObject); });
        AddAlpha(tb, il => EmitFail(il));
        AddBeta(tb,  il => EmitFail(il));
        return tb.CreateType()!;
    }

    private static Type BuildFail()
    {
        var tb = NewBox("bb_fail");
        AddBaseCtor(tb, Type.EmptyTypes, il => { il.Emit(OpCodes.Ldarg_0); il.Emit(OpCodes.Call, CtorObject); });
        AddAlpha(tb, il => EmitFail(il));
        AddBeta(tb,  il => EmitFail(il));
        return tb.CreateType()!;
    }

    private static Type BuildSucceed()
    {
        var tb = NewBox("bb_succeed");
        AddBaseCtor(tb, Type.EmptyTypes, il => { il.Emit(OpCodes.Ldarg_0); il.Emit(OpCodes.Call, CtorObject); });
        AddAlpha(tb, il => EmitZeroWidthRet(il));
        AddBeta(tb,  il => EmitZeroWidthRet(il));
        return tb.CreateType()!;
    }

    // ── bb_not ───────────────────────────────────────────────────────────────
    // α: save; child.α; restore; if child_γ→fail; ZeroWidth
    // β: fail
    private static Type BuildNot()
    {
        var tb = NewBox("bb_not");
        var fC = tb.DefineField("_child", TIByrdBox, FieldAttributes.Private);
        AddBaseCtor(tb, new[]{TIByrdBox}, il => {
            il.Emit(OpCodes.Ldarg_0); il.Emit(OpCodes.Call, CtorObject);
            il.Emit(OpCodes.Ldarg_0); il.Emit(OpCodes.Ldarg_1); il.Emit(OpCodes.Stfld, fC);
        });
        AddAlpha(tb, il => EmitLookahead(il, fC, succeedIfFail: true));
        AddBeta(tb,  il => EmitFail(il));
        return tb.CreateType()!;
    }

    // ── bb_interr ────────────────────────────────────────────────────────────
    // α: save; child.α; restore; if child_ω→fail; ZeroWidth
    // β: fail
    private static Type BuildInterr()
    {
        var tb = NewBox("bb_interr");
        var fC = tb.DefineField("_child", TIByrdBox, FieldAttributes.Private);
        AddBaseCtor(tb, new[]{TIByrdBox}, il => {
            il.Emit(OpCodes.Ldarg_0); il.Emit(OpCodes.Call, CtorObject);
            il.Emit(OpCodes.Ldarg_0); il.Emit(OpCodes.Ldarg_1); il.Emit(OpCodes.Stfld, fC);
        });
        AddAlpha(tb, il => EmitLookahead(il, fC, succeedIfFail: false));
        AddBeta(tb,  il => EmitFail(il));
        return tb.CreateType()!;
    }

    // succeedIfFail=true → NOT (succeed iff child fails)
    // succeedIfFail=false → INTERR (succeed iff child succeeds)
    private static void EmitLookahead(ILGenerator il, FieldInfo fC, bool succeedIfFail)
    {
        var saved = il.DeclareLocal(TInt32);
        var cr    = il.DeclareLocal(TSpec);
        var fail  = il.DefineLabel();

        il.Emit(OpCodes.Ldarg_1); il.Emit(OpCodes.Callvirt, MMS_CursorGet); il.Emit(OpCodes.Stloc, saved);
        il.Emit(OpCodes.Ldarg_0); il.Emit(OpCodes.Ldfld, fC);
        il.Emit(OpCodes.Ldarg_1); il.Emit(OpCodes.Callvirt, MIBox_Alpha);
        il.Emit(OpCodes.Stloc, cr);
        // always restore
        il.Emit(OpCodes.Ldarg_1); il.Emit(OpCodes.Ldloc, saved); il.Emit(OpCodes.Callvirt, MMS_CursorSet);
        EmitIsFail(il, cr);
        // NOT: brfalse→fail (child succeeded → we fail)
        // INTERR: brtrue→fail (child failed → we fail)
        if (succeedIfFail) il.Emit(OpCodes.Brfalse, fail);
        else               il.Emit(OpCodes.Brtrue,  fail);
        EmitZeroWidthRet(il);
        il.MarkLabel(fail); EmitFail(il);
    }
}
