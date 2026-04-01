// driver.sc — test driver for tree.sc (Snocone)
struct tree { t, v, n, c }

procedure MakeLeaf(type, val) { MakeLeaf = tree(type, val, 0, ''); return; }
procedure MakeNode(type, val, nc, kids) { MakeNode = tree(type, val, nc, kids); return; }

&STLIMIT = 1000000;

// 1: MakeLeaf creates node with correct t, v, n=0
leaf = MakeLeaf('Id', 'foo');
if (IDENT(t(leaf), 'Id') && IDENT(v(leaf), 'foo') && IDENT(n(leaf), 0)) {
    OUTPUT = 'PASS: 1 MakeLeaf t/v/n';
} else {
    OUTPUT = 'FAIL: 1 MakeLeaf t/v/n';
}

// 2: MakeNode with 2 children
ch1 = MakeLeaf('Id', 'x');
ch2 = MakeLeaf('Integer', '42');
kids = ARRAY('1:2');
kids[1] = ch1;
kids[2] = ch2;
nd = MakeNode('BinOp', '+', 2, kids);
if (IDENT(t(nd),'BinOp') && IDENT(n(nd),2) && IDENT(t(c(nd)[1]),'Id') && IDENT(t(c(nd)[2]),'Integer')) {
    OUTPUT = 'PASS: 2 MakeNode with children';
} else {
    OUTPUT = 'FAIL: 2 MakeNode with children';
}

// 3: DIFFER guard on real node
if (DIFFER(leaf)) { OUTPUT = 'PASS: 3 DIFFER guard'; } else { OUTPUT = 'FAIL: 3 DIFFER guard'; }

// 4: field update
leaf2 = MakeLeaf('X', 'y');
t(leaf2) = 'Label';
v(leaf2) = 'done';
if (IDENT(t(leaf2),'Label') && IDENT(v(leaf2),'done')) {
    OUTPUT = 'PASS: 4 field update';
} else {
    OUTPUT = 'FAIL: 4 field update';
}
