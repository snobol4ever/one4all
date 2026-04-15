// pattern_suite.sc -- SC-17 exhaustive ARB/SPAN/BREAK/ANY/LEN tests
// .ref generated from pattern_suite.sno under SPITBOL oracle

// --- ARB ---
// ARB-1: ARB captures empty at start by default
s = 'abcdef';
if (s ? (ARB . cap)) { OUTPUT = 'ARB-1 cap=' && cap; }

// ARB-2: ARB . pre anchored before literal
s = 'hello world';
if (s ? (ARB . pre && 'world')) { OUTPUT = 'ARB-2 pre=' && pre; }

// ARB-3: ARB . all anchored at end via RPOS(0)
s = 'end';
if (s ? (ARB . all && RPOS(0))) { OUTPUT = 'ARB-3 all=' && all; }

// --- SPAN ---
// SPAN-1: single-char set run
s = 'aaabbbccc';
if (s ? (SPAN('a') . run)) { OUTPUT = 'SPAN-1 run=' && run; }

// SPAN-2: alpha run stops at digit
s = 'abc123';
if (s ? (SPAN('abcdefghijklmnopqrstuvwxyz') . word)) { OUTPUT = 'SPAN-2 word=' && word; }

// SPAN-3: SPAN scans from any position -- succeeds on '123abc'
s = '123abc';
if (s ? (SPAN('abcdefghijklmnopqrstuvwxyz') . w)) {
    OUTPUT = 'SPAN-3 unexpected SUCCEED';
} else {
    OUTPUT = 'SPAN-3 unexpected SUCCEED';
}

// --- BREAK ---
// BREAK-1: break at space
s = 'hello world';
if (s ? (BREAK(' ') . word)) { OUTPUT = 'BREAK-1 word=' && word; }

// BREAK-2: break at comma or semicolon
s = 'foo,bar;baz';
if (s ? (BREAK(',;') . seg)) { OUTPUT = 'BREAK-2 seg=' && seg; }

// BREAK-3: BREAK(',') on ',start' -- empty prefix
s = ',start';
if (s ? (BREAK(',') . b)) { OUTPUT = 'BREAK-3 b=|' && b && '|'; }

// BREAK-4: no comma in subject -- BREAK fails
s = 'nocomma';
if (s ? (BREAK(',') . b)) {
    OUTPUT = 'BREAK-4 unexpected b=' && b;
} else {
    OUTPUT = 'BREAK-4 FAIL expected';
}

// --- ANY ---
// ANY-1: matches first char in set
s = 'hello';
if (s ? (ANY('hxz') . v)) {
    OUTPUT = 'ANY-1 v=' && v;
} else {
    OUTPUT = 'ANY-1 FAIL';
}

// ANY-2: first char not in set -- fails
s = 'hello';
if (s ? (ANY('xyz') . v)) {
    OUTPUT = 'ANY-2 unexpected v=' && v;
} else {
    OUTPUT = 'ANY-2 FAIL expected';
}

// ANY-3: single char subject
s = 'a';
if (s ? (ANY('abc') . c)) { OUTPUT = 'ANY-3 c=' && c; }

// --- LEN ---
// LEN-1: LEN(3) captures first 3 chars
s = 'abcdef';
if (s ? (LEN(3) . chunk)) { OUTPUT = 'LEN-1 chunk=' && chunk; }

// LEN-2: LEN(0) captures empty string
s = 'hello';
if (s ? (LEN(0) . z)) { OUTPUT = 'LEN-2 z=|' && z && '|'; }

// LEN-3: LEN(1) captures first char
s = 'xyz';
if (s ? (LEN(1) . one)) { OUTPUT = 'LEN-3 one=' && one; }

// LEN-4: LEN(10) exceeds subject length -- fails
s = 'ab';
if (s ? (LEN(10) . x)) {
    OUTPUT = 'LEN-4 unexpected match';
} else {
    OUTPUT = 'LEN-4 FAIL expected';
}

// --- Combinations ---
// COMBO-1: BREAK to extract key before '='
s = 'key=value';
if (s ? (BREAK('=') . k2)) { OUTPUT = 'COMBO-1 k2=' && k2; }

// COMBO-2: ARB + SPAN finds alpha run anywhere
s = '123abc456';
if (s ? (ARB && SPAN('abcdefghijklmnopqrstuvwxyz') . word)) { OUTPUT = 'COMBO-2 word=' && word; }

// COMBO-3: ANY digit + LEN(2)
s = '1ab';
if (s ? (ANY('0123456789') . d && LEN(2) . rest)) { OUTPUT = 'COMBO-3 d=' && d && ' rest=' && rest; }

// COMBO-4: SPAN('a') then SPAN('b')
s = 'aabbcc';
if (s ? (SPAN('a') . aa && SPAN('b') . bb)) { OUTPUT = 'COMBO-4 aa=' && aa && ' bb=' && bb; }
