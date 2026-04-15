/* test_while.sc — while loop lowering test
 * Ref generated from equivalent SNOBOL4 under SPITBOL.
 */

/* Test 1: count 1..5 */
i = 1;
while (LE(i, 5)) {
    OUTPUT = i;
    i = i + 1;
}

/* Test 2: sum 1..10 */
s = 0;
j = 1;
while (LE(j, 10)) {
    s = s + j;
    j = j + 1;
}
OUTPUT = s;

/* Test 3: nested 3x3, print i*j */
i = 1;
while (LE(i, 3)) {
    j = 1;
    while (LE(j, 3)) {
        OUTPUT = i * j;
        j = j + 1;
    }
    i = i + 1;
}
