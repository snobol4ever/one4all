// DESCR.cs — SCRIP universal value type for the .NET backend.
//
// This is the .NET parallel of DESCR_t (snobol4x/src/runtime/snobol4/snobol4.h)
// and struct descr (snobol4-2.3.3/include/snotypes.h).
//
// The SIL descriptor is a two-field cell:
//   v  — type tag   (DTYPE_t / DT_* in the C runtime)
//   a  — value      (union of string ptr, integer, real, or GC pointer)
//
// Sprint LP-4 stub: S/I/R fields only.  Pattern, Array, Table, Code
// pointer fields are added in LP-5 when the full GC layer is wired.
//
// Ref: ARCH-scrip-abi.md §1, §4.
// Compile into snobol4lib.dll alongside Snobol4Lib and DescrRT.

public class DESCR {
    // Type tags — match DTYPE_t numeric values in snobol4.h
    public const int DT_SNUL = 0;   // SNOBOL4 null / unassigned
    public const int DT_S    = 1;   // STRING
    public const int DT_P    = 3;   // PATTERN  (pointer — LP-5)
    public const int DT_I    = 6;   // INTEGER
    public const int DT_R    = 7;   // REAL
    public const int DT_C    = 8;   // CODE      (pointer — LP-5)
    public const int DT_E    = 11;  // EXPRESSION (pointer — LP-5)

    // v field — type tag
    public int    V;

    // a field — value union (C# has no true union; fields are exclusive by convention)
    public string S;    // DT_S  : string value
    public long   I;    // DT_I  : integer value
    public double R;    // DT_R  : real value
    // DT_P / DT_C / DT_E: object pointer added LP-5

    // Constructors — one per concrete type, mirrors STRVAL/INTVAL/REALVAL macros
    public DESCR(string s) { V = DT_S; S = s; }
    public DESCR(long   i) { V = DT_I; I = i; }
    public DESCR(double r) { V = DT_R; R = r; }

    // Null descriptor
    public static readonly DESCR Null = new DESCR("");

    public override string ToString() {
        switch (V) {
            case DT_S:    return S ?? "";
            case DT_I:    return I.ToString();
            case DT_R:    return R.ToString();
            default:      return "";
        }
    }
}
