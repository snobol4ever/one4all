// driver.sc — test driver for counter.sc (Snocone)
// Oracle: compare to beauty_counter_driver.ref

struct link_counter { next, value }
xTrace = 0;

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

&STLIMIT = 1000000;
InitCounter();

// 1: push and increment 3 times, top = 3
PushCounter();
IncCounter(); IncCounter(); IncCounter();
if (IDENT(TopCounter(), 3)) { OUTPUT = 'PASS: 1 push/inc/top = 3'; } else { OUTPUT = 'FAIL: 1 push/inc/top'; }

// 2: nested push, inc once, top = 1
PushCounter();
IncCounter();
if (IDENT(TopCounter(), 1)) { OUTPUT = 'PASS: 2 nested top = 1'; } else { OUTPUT = 'FAIL: 2 nested top'; }

// 3: pop restores outer (top = 3)
PopCounter();
if (IDENT(TopCounter(), 3)) { OUTPUT = 'PASS: 3 pop restore = 3'; } else { OUTPUT = 'FAIL: 3 pop restore'; }

// 4: pop outer, stack empty → PopCounter fails
PopCounter();
if (~PopCounter()) { OUTPUT = 'PASS: 4 empty pop fails'; } else { OUTPUT = 'FAIL: 4 empty pop'; }

// 5: TopCounter on empty stack fails
if (~TopCounter()) { OUTPUT = 'PASS: 5 empty top fails'; } else { OUTPUT = 'FAIL: 5 empty top'; }
