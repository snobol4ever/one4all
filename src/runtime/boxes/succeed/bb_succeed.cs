// bb_succeed.cs — SUCCEED: always γ zero-width; outer scan loop retries
// Mirrors src/runtime/boxes/succeed/bb_succeed.c
//
// Always succeeds; the scan loop's retry-on-backtrack produces the
// "try-everything" effect SNOBOL4's SUCCEED provides.

namespace Snobol4.Runtime.Boxes;

public sealed class bb_succeed : IByrdBox
{
    public Spec α(MatchState ms) => Spec.ZeroWidth(ms.Cursor);
    public Spec β(MatchState ms)  => Spec.ZeroWidth(ms.Cursor);
}
