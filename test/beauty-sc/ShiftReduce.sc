// ShiftReduce.sc — Snocone port of ShiftReduce.sno

procedure Shift(t, v,   s) {
    s = tree(t, v, 0, '');
    Push(s);
    OUTPUT = GT(xTrace, 3) && ('Shift(' && t && ', ' && v && ')');
    if (IDENT(v, '')) { Shift = .v(s); nreturn; }
    else { Shift = .dummy; nreturn; }
}

procedure Reduce(t, n,   c, i, r) {
    Reduce = .dummy;
    if (IDENT(REPLACE(DATATYPE(t), &LCASE, &UCASE), 'EXPRESSION')) {
        t = EVAL(t);
        if (~DIFFER(t)) { nreturn; }
    }
    if (IDENT(REPLACE(DATATYPE(n), &LCASE, &UCASE), 'EXPRESSION')) {
        n = EVAL(n);
        if (~DIFFER(n)) { nreturn; }
    }
    OUTPUT = GT(xTrace, 3) && ('Reduce(' && t && ', ' && n && ')');
    if (GE(n, 1)) { c = ARRAY('1:' && n); } else { c = ''; }
    i = n + 1;
    while (GT(i, 1)) { i = i - 1; c[i] = Pop(''); }
    r = tree(t, '', n, c);
    Push(r);
    nreturn;
}
