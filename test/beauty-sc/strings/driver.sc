// driver.sc — test driver for strings.sc

procedure Reverse(s, i, n, out) {
    n = SIZE(s); out = ''; i = n + 1;
    while (GT(i, 1)) { i = i - 1; out = out && SUBSTR(s, i, 1); }
    Reverse = out; return;
}
procedure TrimLeft(s, i, n, ch, found) {
    n = SIZE(s); i = 0; found = 0;
    while (LT(i, n)) { i = i + 1; ch = SUBSTR(s, i, 1);
        if (DIFFER(ch, ' ') && DIFFER(ch, CHAR(9))) { found = 1; break; } }
    if (IDENT(found, 0)) { TrimLeft = ''; } else { TrimLeft = SUBSTR(s, i); }
    return;
}
procedure TrimRight(s, i, n, ch, found) {
    n = SIZE(s); i = n + 1; found = 0;
    while (GT(i, 1)) { i = i - 1; ch = SUBSTR(s, i, 1);
        if (DIFFER(ch, ' ') && DIFFER(ch, CHAR(9))) { found = 1; break; } }
    if (IDENT(found, 0)) { TrimRight = ''; } else { TrimRight = SUBSTR(s, 1, i); }
    return;
}
procedure Trim(s) { Trim = TrimLeft(TrimRight(s)); return; }
procedure StartsWith(s, prefix) {
    if (IDENT(SUBSTR(s, 1, SIZE(prefix)), prefix)) { return; } else { freturn; }
}
procedure EndsWith(s, suffix, n, sn) {
    n = SIZE(s); sn = SIZE(suffix); if (GT(sn, n)) { freturn; }
    if (IDENT(SUBSTR(s, n - sn + 1, sn), suffix)) { return; } else { freturn; }
}
procedure Split(s, sep, i, n, slen, out_n, start, arr) {
    n = SIZE(s); slen = SIZE(sep); out_n = 0;
    arr = TABLE(); i = 1; start = 1;
    while (LE(i, n)) {
        if (IDENT(SUBSTR(s, i, slen), sep)) {
            out_n = out_n + 1; arr[out_n] = SUBSTR(s, start, i - start);
            i = i + slen; start = i;
        } else { i = i + 1; }
    }
    out_n = out_n + 1; arr[out_n] = SUBSTR(s, start, n - start + 1);
    arr[0] = out_n; Split = arr; return;
}
procedure Join(arr, sep, i, n, out) {
    n = arr[0]; out = ''; i = 0;
    while (LT(i, n)) { i = i + 1;
        if (GT(i, 1)) { out = out && sep; }
        out = out && arr[i]; }
    Join = out; return;
}

&STLIMIT = 1000000;
if (IDENT(Reverse('hello'), 'olleh'))    { OUTPUT = 'PASS: 1 Reverse'; }        else { OUTPUT = 'FAIL: 1 Reverse'; }
if (IDENT(Reverse(''), ''))              { OUTPUT = 'PASS: 2 Reverse empty'; }   else { OUTPUT = 'FAIL: 2 Reverse empty'; }
if (IDENT(TrimLeft('   hi'), 'hi'))      { OUTPUT = 'PASS: 3 TrimLeft'; }        else { OUTPUT = 'FAIL: 3 TrimLeft'; }
if (IDENT(TrimLeft('hi'), 'hi'))         { OUTPUT = 'PASS: 4 TrimLeft noop'; }   else { OUTPUT = 'FAIL: 4 TrimLeft noop'; }
if (IDENT(TrimLeft('   '), ''))          { OUTPUT = 'PASS: 5 TrimLeft all'; }    else { OUTPUT = 'FAIL: 5 TrimLeft all'; }
if (IDENT(TrimRight('hi   '), 'hi'))     { OUTPUT = 'PASS: 6 TrimRight'; }       else { OUTPUT = 'FAIL: 6 TrimRight'; }
if (IDENT(Trim('  hi  '), 'hi'))         { OUTPUT = 'PASS: 7 Trim'; }            else { OUTPUT = 'FAIL: 7 Trim'; }
if (IDENT(Trim('   '), ''))              { OUTPUT = 'PASS: 8 Trim all'; }        else { OUTPUT = 'FAIL: 8 Trim all'; }
if (StartsWith('hello', 'hel'))          { OUTPUT = 'PASS: 9 StartsWith hit'; }  else { OUTPUT = 'FAIL: 9 StartsWith hit'; }
if (~StartsWith('hello', 'xyz'))         { OUTPUT = 'PASS: 10 StartsWith miss'; } else { OUTPUT = 'FAIL: 10 StartsWith miss'; }
if (EndsWith('hello', 'llo'))            { OUTPUT = 'PASS: 11 EndsWith hit'; }   else { OUTPUT = 'FAIL: 11 EndsWith hit'; }
if (~EndsWith('hello', 'xyz'))           { OUTPUT = 'PASS: 12 EndsWith miss'; }  else { OUTPUT = 'FAIL: 12 EndsWith miss'; }
t = Split('a,b,c', ',');
if (IDENT(t[1],'a') && IDENT(t[2],'b') && IDENT(t[3],'c') && EQ(t[0],3)) { OUTPUT = 'PASS: 13 Split'; } else { OUTPUT = 'FAIL: 13 Split'; }
if (IDENT(Join(t, '-'), 'a-b-c'))        { OUTPUT = 'PASS: 14 Join'; }           else { OUTPUT = 'FAIL: 14 Join'; }
t2 = Split('hello', ',');
if (EQ(t2[0], 1) && IDENT(t2[1], 'hello')) { OUTPUT = 'PASS: 15 Split no-sep'; } else { OUTPUT = 'FAIL: 15 Split no-sep'; }
