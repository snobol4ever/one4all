// counter.sc — Snocone port of counter.sno

struct link_counter { next, value }

procedure InitCounter() { $'#N' = ''; return; }
procedure PushCounter() { $'#N' = link_counter($'#N', 0); PushCounter = .dummy; nreturn; }
procedure IncCounter()  { value($'#N') = value($'#N') + 1; IncCounter = .dummy; nreturn; }
procedure DecCounter()  { value($'#N') = value($'#N') - 1; DecCounter = .dummy; nreturn; }
procedure PopCounter() {
    if (DIFFER($'#N')) { $'#N' = next($'#N'); PopCounter = .dummy; nreturn; }
    else { freturn; }
}
procedure TopCounter() {
    if (DIFFER($'#N')) { TopCounter = value($'#N'); return; }
    else { freturn; }
}
