// bb_eps.cs — EPS: zero-width success once; done flag prevents double-γ
// Mirrors src/runtime/boxes/bb_eps.c
//
// α: if done → ω;  done=true; γ zero-width
// β: ω

namespace Snobol4.Runtime.Boxes;

public sealed class bb_eps : IByrdBox
{
    private bool _done;

    public Spec α(MatchState ms)
    {
        if (_done) return Spec.Fail;
        _done = true;
        return Spec.ZeroWidth(ms.Cursor);
    }

    public Spec β(MatchState ms) => Spec.Fail;
}
