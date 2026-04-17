// test_capture_call.sc — SC-26 / CL-1 isolation
//
// Probes the (PAT . var) . *fn(var) pattern from claws5.sc and the treebank
// programs.  Both oracles (SPITBOL x64 -b, CSNOBOL4 -bf) invoke fn('foo')
// at match time.  scrip does not — fn is not called at all.
//
// Tests build from the simplest indirect-call case to the full blocker.
// Each test prints exactly one PASS/FAIL line for diff comparison.

// 1. Plain indirect call, no capture chained.
//    (epsilon . *hit()) — zero-width, call-on-entry.
procedure hit() {
    hit_seen = 1;
    hit = .dummy;
    nreturn;
}
hit_seen = 0;
p1 = epsilon . *hit();
if (('x' ? p1) && hit_seen) { OUTPUT = 'PASS: 1 epsilon-star call'; } else { OUTPUT = 'FAIL: 1 hit_seen=' hit_seen; }

// 2. Capture alone, no indirect call.
//    (LEN(3) . w) should leave w = 'foo' after a successful match.
w = '';
p2 = LEN(3) . w;
if (('foobar' ? p2) && (w == 'foo')) { OUTPUT = 'PASS: 2 plain capture'; } else { OUTPUT = 'FAIL: 2 w=<' w '>'; }

// 3. THE BLOCKER: capture + indirect call chained.
//    (LEN(3) . w) . *show(w)  — should call show('foo') with the captured w.
procedure show(x) {
    show_arg = x;
    show = .dummy;
    nreturn;
}
show_arg = '';
w = '';
p3 = (LEN(3) . w) . *show(w);
if (('foobar' ? p3) && (show_arg == 'foo')) { OUTPUT = 'PASS: 3 capture-call'; } else { OUTPUT = 'FAIL: 3 show_arg=<' show_arg '> w=<' w '>'; }

// 4. Same shape as claws5.sc uses: capture . *fn(captured_name)
//    with alternation — (A . v1) . *f(v1) | (B . v2) . *g(v2)
procedure saw_num(n) {
    last_kind = 'num';
    last_val = n;
    saw_num = .dummy;
    nreturn;
}
procedure saw_wrd(w) {
    last_kind = 'wrd';
    last_val = w;
    saw_wrd = .dummy;
    nreturn;
}
last_kind = '';
last_val = '';
p4 =
    ( (SPAN('0123456789') . n) . *saw_num(n)
    | (SPAN('abcdefghijklmnopqrstuvwxyz') . s) . *saw_wrd(s)
    );
if (('abc' ? p4) && (last_kind == 'wrd') && (last_val == 'abc')) { OUTPUT = 'PASS: 4a alt-wrd'; } else { OUTPUT = 'FAIL: 4a kind=' last_kind ' val=<' last_val '>'; }

last_kind = '';
last_val = '';
if (('123' ? p4) && (last_kind == 'num') && (last_val == 123)) { OUTPUT = 'PASS: 4b alt-num'; } else { OUTPUT = 'FAIL: 4b kind=' last_kind ' val=<' last_val '>'; }
