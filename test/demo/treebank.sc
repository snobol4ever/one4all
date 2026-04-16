// treebank.sc — Penn Treebank S-expression parser (Snocone port)
// ENG 685, Lon Cherryholmes Sr.
//
// Structured like beauty.sno: -INCLUDE libraries, statement-level main loop.


&TRIM     = 1;
&ANCHOR   = 0;
&FULLSCAN = 1;
nl        = CHAR(10);
spc       = ' ';
spcnl     = spc && nl;

struct cell { hd, tl }
stk = '';

//--- Side-effect procedures -------------------------------------------------

procedure do_push_list(v) {
    stk = cell(cell(v, ''), stk);
    do_push_list = .dummy;
    nreturn;
}

procedure do_push_item(v) {
    hd(stk) = cell(v, hd(stk));
    do_push_item = .dummy;
    nreturn;
}

procedure do_pop_list(  p, n, i, a) {
    p = hd(stk);
    n = 0;
    while (DIFFER(p)) { n = n + 1; p = tl(p); }
    a = ARRAY('1:' && n);
    p = hd(stk);
    i = n + 1;
    while (GT(i, 1)) {
        i = i - 1;
        a[i] = hd(p);
        p = tl(p);
    }
    stk = tl(stk);
    hd(stk) = cell(a, hd(stk));
    do_pop_list = .dummy;
    nreturn;
}

procedure do_pop_final(v,   p, n, i, a) {
    p = hd(stk);
    n = 0;
    while (DIFFER(p)) { n = n + 1; p = tl(p); }
    a = ARRAY('1:' && n);
    p = hd(stk);
    i = n + 1;
    while (GT(i, 1)) {
        i = i - 1;
        a[i] = hd(p);
        p = tl(p);
    }
    stk = tl(stk);
    $v = a;
    do_pop_final = .dummy;
    nreturn;
}

//--- group() — recursive descent parser ------------------------------------

procedure group(  tag, wrd) {
    if (~(buf ? (POS(0) && '(' && ''))) { freturn; }
    if (~(buf ? (POS(0) && BREAK(spcnl && '()') . tag && ''))) { freturn; }
    dummy = do_push_list(tag);
    group_loop:
    if (~(buf ? (POS(0) && SPAN(spcnl) && ''))) { goto group_close; }
    if (buf ? (POS(0) && '(')) { goto group_rec; }
    if (~(buf ? (POS(0) && BREAK(spcnl && '()') . wrd && ''))) { goto group_close; }
    dummy = do_push_item(wrd);
    goto group_loop;
    group_rec:
    if (group()) { goto group_loop; } else { freturn; }
    group_close:
    if (~(buf ? (POS(0) && ')' && ''))) { freturn; }
    dummy = do_pop_list();
    group = '';
    return;
}

//--- print_node — recursive tree printer -----------------------------------

procedure print_node(a, depth,   i, ch, dta) {
    dta = DATATYPE(a);
    if (IDENT(REPLACE(dta, &LCASE, &UCASE), 'STRING')) {
        OUTPUT = DUPL('  ', depth) && a;
        return;
    }
    OUTPUT = DUPL('  ', depth) && '(' && a[1];
    i = 1;
    while (1) {
        i = i + 1;
        ch = a[i];
        if (~DIFFER(ch)) { break; }
        dummy = print_node(ch, depth + 1);
    }
    OUTPUT = DUPL('  ', depth) && ')';
    return;
}

//--- word pattern -----------------------------------------------------------

wbrks = '( )' && nl;
word  = NOTANY(wbrks) && BREAK(wbrks);

//--- Main -------------------------------------------------------------------

buf = '';
while (DIFFER(line = INPUT)) {
    buf = buf && spc && line;
}

buf   = buf && spc;
dummy = do_push_list('BANK');

sentloop:
buf ? (POS(0) && SPAN(spcnl) && '');
if (~(buf ? (POS(0) && '('))) { goto parse_done; }
dummy = do_push_list('ROOT');
if (~group()) { goto parse_err; }
dummy = do_pop_list();
goto sentloop;

parse_done:
dummy = do_pop_final('bank');
dta   = REPLACE(DATATYPE(bank), &LCASE, &UCASE);
if (~IDENT(dta, 'ARRAY')) { goto END; }
i = 1;
print_lp:
i = i + 1;
r = bank[i];
if (~DIFFER(r)) { goto END; }
dummy = print_node(r, 0);
goto print_lp;

parse_err:
OUTPUT = '*** parse error near: ' && SUBSTR(buf, 1, 40);
END:
