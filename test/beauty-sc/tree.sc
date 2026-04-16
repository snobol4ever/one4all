// tree.sc — Snocone port of tree.sno

struct treeNode { t, v, n, c }
struct tree { t, v, n, c }

procedure MakeNode(nodeT, nodeV, nodeN, nodeC) {
    MakeNode = treeNode(nodeT, nodeV, nodeN, nodeC);
    return;
}
procedure MakeLeaf(nodeT, nodeV) {
    MakeLeaf = treeNode(nodeT, nodeV, 0, ARRAY('1:0'));
    return;
}
