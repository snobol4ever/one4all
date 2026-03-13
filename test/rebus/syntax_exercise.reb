# syntax_exercise.reb — exercises every Rebus construct from TR 84-9 appendix grammar

record point(x, y)
record bnode(value, left, right, up)

function main()
  local i, j, s, t, result

  # Arithmetic and assignment
  i := 0
  j := 10
  i +:= 1
  j -:= 1
  s := "hello" || " " || "world"
  s ||:= "!"

  # Comparisons (all six numeric + all six string)
  if i = 0 then output := "i is zero"
  if i ~= j then output := "i ne j"
  if i < j then output := "i lt j"
  if i <= j then output := "i le j"
  if j > i then output := "j gt i"
  if j >= i then output := "j ge i"
  if s == "hello world!" then output := "strings equal"
  if s ~== "other" then output := "strings differ"
  if "abc" << "def" then output := "abc lt def"
  if "def" >> "abc" then output := "def gt abc"
  if "abc" <<= "abc" then output := "abc le abc"
  if "abc" >>= "abc" then output := "abc ge abc"

  # Exchange
  i :=: j

  # Unless
  unless i = 0 then output := "i is nonzero"

  # While loop
  i := 0
  while i < 5 do
    i +:= 1

  # Until loop
  until i = 0 do
    i -:= 1

  # Repeat with exit
  repeat {
    i +:= 1
    if i > 3 then exit
  }

  # For loop with by
  for i from 1 to 10 by 2 do
    output := i

  # For loop without by
  for i from 1 to 5 do
    output := i

  # Case statement
  case s of {
    "hello" : output := "got hello"
    "world" : output := "got world"
    default : output := "got other"
  }

  # Pattern match (no replacement)
  s ? span("abcdefghijklmnopqrstuvwxyz") . t

  # Pattern replace
  s ? "hello" <- "goodbye"

  # Pattern replace-with-empty (shorthand)
  s ?- span(" ")

  # Nested if/else
  if i = 0 then {
    output := "zero"
  }
  else {
    if i < 0 then output := "negative"
    else output := "positive"
  }

  # Function calls and record construction
  t := point(3, 4)
  output := x(t)

  # Keyword references
  output := &lcase
  output := &ucase
  output := size(s)

  # Subscript and substring
  output := s[1]
  output := s[2 +: 3]

  # Unary operators
  result := \i        # IDENT (value test)
  result := /i        # DIFFER (null test)

  # Pattern alternation and concatenation
  t := span("aeiou") | span("bcdfg")
  t := span("a") & span("b")

  # Capture operators in patterns
  s ? any("aeiou") . result
  s ? any("aeiou") $ result

  return result
end

function factorial(n)
  if n <= 1 then return 1
  else return n * factorial(n - 1)
end

function fib(n)
  local a, b, tmp
  initial {
    a := 0
    b := 1
  }
  if n <= 0 then return a
  if n = 1 then return b
  repeat {
    tmp := a + b
    a := b
    b := tmp
    n -:= 1
    if n = 1 then return b
  }
end
