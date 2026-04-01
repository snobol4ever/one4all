// driver.sc — test driver for roman.sc
procedure Roman(n, s, i, len, d, place, ones, fives, tens, result) {
    s = CONVERT(n, 'STRING'); len = SIZE(s); result = ''; i = 0;
    while (LT(i, len)) {
        i = i + 1; d = INTEGER(SUBSTR(s, i, 1)); place = len - i;
        if (EQ(place, 0)) { ones = 'I'; fives = 'V'; tens = 'X'; }
        if (EQ(place, 1)) { ones = 'X'; fives = 'L'; tens = 'C'; }
        if (EQ(place, 2)) { ones = 'C'; fives = 'D'; tens = 'M'; }
        if (EQ(place, 3)) { ones = 'M'; fives = '';  tens = '';  }
        if (EQ(d, 1)) { result = result && ones; }
        else if (EQ(d, 2)) { result = result && ones && ones; }
        else if (EQ(d, 3)) { result = result && ones && ones && ones; }
        else if (EQ(d, 4)) { result = result && ones && fives; }
        else if (EQ(d, 5)) { result = result && fives; }
        else if (EQ(d, 6)) { result = result && fives && ones; }
        else if (EQ(d, 7)) { result = result && fives && ones && ones; }
        else if (EQ(d, 8)) { result = result && fives && ones && ones && ones; }
        else if (EQ(d, 9)) { result = result && ones && tens; }
    }
    Roman = result; return;
}
&STLIMIT = 1000000;
if (IDENT(Roman(1),    'I'))         { OUTPUT = 'PASS: 1 Roman(1)=I'; }         else { OUTPUT = 'FAIL: 1 ' && Roman(1); }
if (IDENT(Roman(4),    'IV'))        { OUTPUT = 'PASS: 2 Roman(4)=IV'; }        else { OUTPUT = 'FAIL: 2'; }
if (IDENT(Roman(9),    'IX'))        { OUTPUT = 'PASS: 3 Roman(9)=IX'; }        else { OUTPUT = 'FAIL: 3'; }
if (IDENT(Roman(14),   'XIV'))       { OUTPUT = 'PASS: 4 Roman(14)=XIV'; }      else { OUTPUT = 'FAIL: 4 ' && Roman(14); }
if (IDENT(Roman(42),   'XLII'))      { OUTPUT = 'PASS: 5 Roman(42)=XLII'; }     else { OUTPUT = 'FAIL: 5'; }
if (IDENT(Roman(400),  'CD'))        { OUTPUT = 'PASS: 6 Roman(400)=CD'; }      else { OUTPUT = 'FAIL: 6'; }
if (IDENT(Roman(1776), 'MDCCLXXVI')) { OUTPUT = 'PASS: 7 Roman(1776)=MDCCLXXVI'; } else { OUTPUT = 'FAIL: 7'; }
if (IDENT(Roman(1999), 'MCMXCIX'))   { OUTPUT = 'PASS: 8 Roman(1999)=MCMXCIX'; } else { OUTPUT = 'FAIL: 8'; }
if (IDENT(Roman(2024), 'MMXXIV'))    { OUTPUT = 'PASS: 9 Roman(2024)=MMXXIV'; }  else { OUTPUT = 'FAIL: 9'; }
if (IDENT(Roman(3999), 'MMMCMXCIX')){ OUTPUT = 'PASS: 10 Roman(3999)=MMMCMXCIX'; } else { OUTPUT = 'FAIL: 10'; }
