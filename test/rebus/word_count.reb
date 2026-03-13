# word_count.reb — word counting example from Griswold TR 84-9, section 3

function main()
  letter := &lcase || &ucase
  wpat := break(letter) & span(letter) . word
  count := table()
  while text := input do
    while text ?- wpat do
      count[word] +:= 1
  if result := sort(count) then {
    output := "Word count:"
    output := ""
    i := 0
    repeat output := rpad(result[i +:= 1, 1], 15) || lpad(result[i, 2], 4)
  }
  else output := "There are no words"
end
