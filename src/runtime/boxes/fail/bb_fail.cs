// bb_fail.cs — FAIL: always ω — force backtrack
// Mirrors src/runtime/boxes/fail/bb_fail.c

namespace Snobol4.Runtime.Boxes;

public sealed class bb_fail : IByrdBox
{
    public Spec α(MatchState ms) => Spec.Fail;
    public Spec β(MatchState ms)  => Spec.Fail;
}
