procedure max(a, b) {
    if (a >= b) then return a
    return b
}
procedure min(a, b) {
    if (a <= b) then return a
    return b
}
procedure abs_val(n) {
    if (n >= 0) then return n
    return 0 - n
}
OUTPUT = max(3, 7)
OUTPUT = min(3, 7)
OUTPUT = abs_val(0 - 5)
OUTPUT = max(abs_val(0 - 3), abs_val(0 - 8))
