// stack.sc — Snocone port of stack.sno

struct link { next, value }
xTrace = 0;

procedure InitStack() { $'@S' = ''; return; }
procedure Push(x) {
    OUTPUT = GT(xTrace, 4) && ('Push(' && t(x) && ')');
    $'@S' = link($'@S', x);
    if (IDENT(x, '')) { Push = .value($'@S'); nreturn; }
    else { Push = .dummy; nreturn; }
}
procedure Pop(var) {
    if (~DIFFER($'@S')) { freturn; }
    if (IDENT(var, '')) {
        Pop = value($'@S');
        OUTPUT = GT(xTrace, 4) && ('Pop() = ' && t(Pop));
        $'@S' = next($'@S');
        return;
    }
    Pop = .dummy;
    $var = value($'@S');
    OUTPUT = GT(xTrace, 4) && ('Pop() = ' && t($var));
    $'@S' = next($'@S');
    nreturn;
}
procedure Top() {
    if (~DIFFER($'@S')) { freturn; }
    Top = .value($'@S');
    OUTPUT = GT(xTrace, 4) && ('Top() = ' && t(Top));
    nreturn;
}
