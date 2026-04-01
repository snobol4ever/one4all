// driver.sc — test driver for stack.sc (Snocone)
struct link { next, value }
xTrace = 0;

procedure InitStack() { $'@S' = ''; return; }
procedure Push(x) {
    $'@S' = link($'@S', x);
    if (IDENT(x, '')) { Push = .value($'@S'); nreturn; }
    else { Push = .dummy; nreturn; }
}
procedure Pop(var) {
    if (~DIFFER($'@S')) { freturn; }
    if (IDENT(var, '')) { Pop = value($'@S'); $'@S' = next($'@S'); return; }
    else { $var = value($'@S'); $'@S' = next($'@S'); Pop = .dummy; nreturn; }
}
procedure Top() {
    if (~DIFFER($'@S')) { freturn; }
    Top = .value($'@S');
    nreturn;
}

&STLIMIT = 1000000;
InitStack();

Push(42);
if (IDENT(Top(), 42)) { OUTPUT = 'PASS: 1 push/top = 42'; } else { OUTPUT = 'FAIL: 1 push/top'; }

InitStack();
Push(10); Push(20); Push(30);
if (IDENT(Top(), 30)) { OUTPUT = 'PASS: 2 top of 3 = 30'; } else { OUTPUT = 'FAIL: 2 top of 3'; }

Pop('dummy');
if (IDENT(Top(), 20)) { OUTPUT = 'PASS: 3 pop restores 20'; } else { OUTPUT = 'FAIL: 3 pop restores'; }

InitStack();
Push(99);
Pop('result');
if (IDENT(result, 99)) { OUTPUT = 'PASS: 4 Pop(var) = 99'; } else { OUTPUT = 'FAIL: 4 Pop(var)'; }

InitStack();
if (~Pop('dummy')) { OUTPUT = 'PASS: 5 empty pop fails'; } else { OUTPUT = 'FAIL: 5 empty pop'; }

InitStack();
if (~Top()) { OUTPUT = 'PASS: 6 empty top fails'; } else { OUTPUT = 'FAIL: 6 empty top'; }

InitStack();
Push('a'); Push('b'); Push('c');
Pop('v1'); Pop('v2'); Pop('v3');
if (IDENT(v1,'c') && IDENT(v2,'b') && IDENT(v3,'a')) { OUTPUT = 'PASS: 7 nested pop order a/b/c'; } else { OUTPUT = 'FAIL: 7 pop order'; }
