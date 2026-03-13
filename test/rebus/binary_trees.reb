# binary_trees.reb — binary tree example from Griswold TR 84-9, section 3

record bnode(value, left, right, up)

function main()
  repeat output := bexp(btree(input))
end

function addl(n1, n2)
  left(n1) := n2
  up(n2) := n1
end

function addr(n1, n2)
  right(n1) := n2
  up(n2) := n1
end

function btree(s)
  local l, r, t
  initial {
    two  := "(" & bal . l & "," & bal . r & ")"
    rone := "(" & "," & bal . r & ")"
    lone := "(" & bal . l & ")"
    tform := break("(") . s & (two | rone | lone)
  }
  s ? tform
  t := bnode(s)
  if \l then addl(t, btree(l))
  if \r then addr(t, btree(r))
  return t
end

function bexp(t)
  local l, r, s1, s2
  s1 := value(t)
  if \left(t) then l := bexp(left(t))
  if \right(t) then r := "," || bexp(right(t))
  s2 := l || r
  if \s2 then return s1 || "(" || s2 || ")"
  else return s1
end
