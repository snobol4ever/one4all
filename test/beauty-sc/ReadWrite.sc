// ReadWrite.sc — Snocone port of ReadWrite.sno

procedure Read(fileName, rdMapName,   rdInput, rdIn, rdLine, rdLineNo, rdMap, rdOfs) {
    if (~input__(.rdInput, 8, '', fileName)) { freturn; }
    rdMap    = TABLE();
    rdOfs    = 0;
    rdLineNo = 1;
    Read     = '';
    Read_3:
    rdMap[rdOfs] = rdLineNo;
    rdLine = '';
    Read_5:
    rdIn = rdInput;
    if (~DIFFER(rdIn)) { goto Read_9; }
    rdLine = rdLine && rdIn;
    if (~LT(SIZE(rdIn), 131072)) { goto Read_5; }
    rdLine ? (RPOS(1) && cr && '') = ;
    rdOfs    = rdOfs + SIZE(rdLine) + 1;
    rdLineNo = rdLineNo + 1;
    Read     = Read && rdLine && nl;
    goto Read_3;
    Read_9:
    ENDFILE(8);
    if (~DIFFER(rdMapName)) { return; }
    $rdMapName = rdMap;
    return;
}

procedure Write(fileName, fileStr,   wrLine, wrOutput) {
    if (~output__(.wrOutput, 8, '', fileName)) { freturn; }
    Write_1:
    if (fileStr ? (POS(0) && RPOS(0))) { goto Write_9; }
    if (fileStr ? (POS(0) && BREAK(nl) . wrLine && nl && '')) { goto Write_3; }
    if (fileStr ? (POS(0) && RTAB(0) . wrLine && '')) { goto Write_3; }
    freturn;
    Write_3:
    wrOutput = wrLine;
    goto Write_1;
    Write_9:
    ENDFILE(8);
    return;
}

procedure LineMap(str, lmMapName,   lmLineNo, lmMap, lmOfs, xOfs) {
    lmMap    = TABLE();
    lmOfs    = 0;
    lmLineNo = 1;
    LineMap_3:
    lmMap[lmOfs] = lmLineNo;
    if (~(str ? (POS(0) && BREAK(nl) && nl && @xOfs && ''))) { goto LineMap_9; }
    lmOfs    = lmOfs + xOfs;
    lmLineNo = lmLineNo + 1;
    goto LineMap_3;
    LineMap_9:
    $lmMapName = lmMap;
    return;
}
