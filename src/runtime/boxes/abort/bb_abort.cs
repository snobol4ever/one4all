// bb_abort.cs — ABORT: always ω regardless of entry
// Mirrors src/runtime/boxes/abort/bb_abort.c

namespace Snobol4.Runtime.Boxes;

public sealed class bb_abort : IByrdBox
{
    public Spec α(MatchState ms) => Spec.Fail;
    public Spec β(MatchState ms)  => Spec.Fail;
}
