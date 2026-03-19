procedure count_down(n) {
    total = 0
    i = n
    while (i > 0) do {
        total = total + i
        i = i - 1
    }
    return total
}
OUTPUT = count_down(10)
OUTPUT = count_down(5)
