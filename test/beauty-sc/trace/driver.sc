&STLIMIT = 1000000;
strOfs = 0; t8Max = 0; t8MaxLine = 0; t8MaxLast = 0; doDebug = 0; t8Map = '';

procedure T8Pos(t8Ofs, _map, i) {
    if (IDENT(_map, '')) { T8Pos = LPAD(t8Ofs, 8); return; }
    i = t8Ofs;
    if (GT(t8Ofs, t8Max)) { t8Max = t8Ofs; }
    while (1) {
        if (~IDENT(_map[i], '')) { break; }
        i = i - 1;
        if (LT(i, 0)) { T8Pos = LPAD(t8Ofs, 8); return; }
    }
    t8Line = _map[i];
    t8Pos  = t8Ofs - i + 1;
    i = t8Max;
    while (1) {
        if (~IDENT(_map[i], '')) { break; }
        i = i - 1;
        if (LT(i, 0)) { T8Pos = LPAD(t8Ofs, 8); return; }
    }
    t8MaxLine = _map[i];
    t8MaxPos  = t8Max - i + 1;
    T8Pos = '(' && LPAD(t8MaxLine, 5) && ', ' && LPAD(t8MaxPos, 3) &&
            ', ' && LPAD(t8Line, 5)   && ', ' && LPAD(t8Pos, 3) && ')';
    return;
}

procedure T8Trace(lvl, str, ofs) {
    T8Trace = .dummy;
    if (~GT(doDebug, 0)) { nreturn; }
    if (~LE(lvl, doDebug)) { nreturn; }
    if (~GT(doDebug, 1)) {
        if (str ? (POS(0) && '?')) { nreturn; }
        nreturn;
    }
    if (str ? (POS(0) && '?')) {
        str = '? ' && SUBSTR(str, 2);
    } else {
        str = '  ' && str;
    }
    _t8p = T8Pos(strOfs + ofs, t8Map);
    if (~GE(t8MaxLine, 621)) { nreturn; }
    if (GE(t8Max, t8MaxLast)) { t8MaxLast = t8Max; }
    OUTPUT = _t8p && str;
    nreturn;
}

dSTRING = DATATYPE('');

r1 = T8Pos(5, '');
if (IDENT(r1, '       5')) { OUTPUT = 'PASS: 1 T8Pos nil map=LPAD'; } else { OUTPUT = 'FAIL: 1 [' && r1 && ']'; }

t8Map2 = TABLE(); t8Map2[0] = 1; t8Map2[5] = 2; t8Max = 0;
r2 = T8Pos(7, t8Map2);
if (IDENT(r2, '(    2,   3,     2,   3)')) { OUTPUT = 'PASS: 2 T8Pos map line/col'; } else { OUTPUT = 'FAIL: 2 [' && r2 && ']'; }

t8Map3 = TABLE(); t8Map3[0] = 1; t8Max = 0;
T8Pos(12, t8Map3);
if (EQ(t8Max, 12)) { OUTPUT = 'PASS: 3 T8Pos updates t8Max'; } else { OUTPUT = 'FAIL: 3 t8Max=' && t8Max; }

doDebug = 0;
r4 = T8Trace(1, 'hello', 0);
if (IDENT(DATATYPE(r4), dSTRING)) { OUTPUT = 'PASS: 4 T8Trace doDebug=0 returns STRING'; } else { OUTPUT = 'FAIL: 4'; }

doDebug = 1; t8Max = 0; t8MaxLine = 0; strOfs = 0; t8Map = '';
r5 = T8Trace(2, 'skip', 0);
if (IDENT(DATATYPE(r5), dSTRING)) { OUTPUT = 'PASS: 5 T8Trace lvl>doDebug NRETURN'; } else { OUTPUT = 'FAIL: 5'; }

doDebug = 1; t8Max = 0; t8MaxLine = 621; t8MaxLast = 0; strOfs = 0; t8Map = '';
r6 = T8Trace(1, '?x', 0);
if (IDENT(DATATYPE(r6), dSTRING)) { OUTPUT = 'PASS: 6 T8Trace ?-prefix doDebug=1 NRETURN'; } else { OUTPUT = 'FAIL: 6'; }

doDebug = 2; t8Max = 0; t8MaxLine = 0; t8MaxLast = 0; strOfs = 0; t8Map = '';
r7 = T8Trace(1, 'blocked', 0);
if (IDENT(DATATYPE(r7), dSTRING)) { OUTPUT = 'PASS: 7 T8Trace t8MaxLine<621 NRETURN'; } else { OUTPUT = 'FAIL: 7'; }

doDebug = 2; t8Max = 0; t8MaxLine = 621; t8MaxLast = 0; strOfs = 0; t8Map = '';
OUTPUT = '--- test 8 output follows ---';
T8Trace(1, '?node', 0);
OUTPUT = 'PASS: 8 T8Trace doDebug=2 ?-expand output';

doDebug = 2; t8Max = 10; t8MaxLine = 621; t8MaxLast = 5; strOfs = 0; t8Map = '';
T8Trace(1, 'upd', 0);
if (EQ(t8MaxLast, 10)) { OUTPUT = 'PASS: 9 t8MaxLast updated to t8Max'; } else { OUTPUT = 'FAIL: 9 t8MaxLast=' && t8MaxLast; }
