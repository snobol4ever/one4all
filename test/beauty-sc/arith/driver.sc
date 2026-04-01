// driver.sc — test driver for arith.sc

procedure ISqrt(n, i) {
    i = 0; while (LE((i+1)*(i+1), n)) { i = i + 1; } ISqrt = i; return;
}
procedure Fibonacci(n, a, b, t, i) {
    if (LE(n, 0)) { Fibonacci = 0; return; }
    if (EQ(n, 1)) { Fibonacci = 1; return; }
    a = 0; b = 1; i = 1;
    while (LT(i, n)) { i = i + 1; t = b; b = a + b; a = t; }
    Fibonacci = b; return;
}
procedure GCD(a, b, t) {
    while (DIFFER(b, 0)) { t = b; b = REMDR(a, b); a = t; }
    GCD = a; return;
}
procedure Factorial(n, acc, i) {
    acc = 1; i = 0;
    while (LT(i, n)) { i = i + 1; acc = acc * i; }
    Factorial = acc; return;
}
procedure IsPrime(n, i, lim) {
    if (LE(n, 1)) { freturn; }
    if (EQ(n, 2)) { return; }
    if (IDENT(REMDR(n, 2), 0)) { freturn; }
    lim = ISqrt(n); i = 1;
    while (1) { i = i + 2; if (GT(i, lim)) { break; }
        if (IDENT(REMDR(n, i), 0)) { freturn; } }
    return;
}
procedure Sieve(n, arr, i, j) {
    arr = TABLE(); i = 2;
    while (LE(i, n)) { arr[i] = 1; i = i + 1; }
    i = 2;
    while (LE(i * i, n)) {
        if (IDENT(arr[i], 1)) { j = i * i;
            while (LE(j, n)) { arr[j] = 0; j = j + i; } }
        i = i + 1; }
    Sieve = arr; return;
}

&STLIMIT = 10000000;
if (EQ(Fibonacci(10), 55))    { OUTPUT = 'PASS: 1 Fibonacci(10)=55'; }  else { OUTPUT = 'FAIL: 1'; }
if (EQ(Fibonacci(0), 0))      { OUTPUT = 'PASS: 2 Fibonacci(0)=0'; }    else { OUTPUT = 'FAIL: 2'; }
if (EQ(Fibonacci(1), 1))      { OUTPUT = 'PASS: 3 Fibonacci(1)=1'; }    else { OUTPUT = 'FAIL: 3'; }
if (EQ(Fibonacci(20), 6765))  { OUTPUT = 'PASS: 4 Fibonacci(20)=6765'; } else { OUTPUT = 'FAIL: 4 ' && Fibonacci(20); }
if (EQ(GCD(48, 18), 6))       { OUTPUT = 'PASS: 5 GCD(48,18)=6'; }      else { OUTPUT = 'FAIL: 5'; }
if (EQ(GCD(100, 75), 25))     { OUTPUT = 'PASS: 6 GCD(100,75)=25'; }    else { OUTPUT = 'FAIL: 6'; }
if (EQ(GCD(7, 13), 1))        { OUTPUT = 'PASS: 7 GCD(7,13)=1'; }       else { OUTPUT = 'FAIL: 7'; }
if (EQ(Factorial(5), 120))    { OUTPUT = 'PASS: 8 Factorial(5)=120'; }   else { OUTPUT = 'FAIL: 8'; }
if (EQ(Factorial(0), 1))      { OUTPUT = 'PASS: 9 Factorial(0)=1'; }     else { OUTPUT = 'FAIL: 9'; }
if (EQ(Factorial(10), 3628800)) { OUTPUT = 'PASS: 10 Factorial(10)'; }  else { OUTPUT = 'FAIL: 10 ' && Factorial(10); }
if (IsPrime(2))               { OUTPUT = 'PASS: 11 IsPrime(2)'; }         else { OUTPUT = 'FAIL: 11'; }
if (IsPrime(17))              { OUTPUT = 'PASS: 12 IsPrime(17)'; }        else { OUTPUT = 'FAIL: 12'; }
if (~IsPrime(1))              { OUTPUT = 'PASS: 13 ~IsPrime(1)'; }        else { OUTPUT = 'FAIL: 13'; }
if (~IsPrime(15))             { OUTPUT = 'PASS: 14 ~IsPrime(15)'; }       else { OUTPUT = 'FAIL: 14'; }
if (~IsPrime(100))            { OUTPUT = 'PASS: 15 ~IsPrime(100)'; }      else { OUTPUT = 'FAIL: 15'; }
primes = Sieve(20);
if (IDENT(primes[2],1) && IDENT(primes[3],1) && IDENT(primes[4],0) && IDENT(primes[17],1) && IDENT(primes[15],0)) {
    OUTPUT = 'PASS: 16 Sieve(20)';
} else { OUTPUT = 'FAIL: 16 Sieve'; }
if (EQ(ISqrt(15), 3))         { OUTPUT = 'PASS: 17 ISqrt(15)=3'; }       else { OUTPUT = 'FAIL: 17'; }
if (EQ(ISqrt(16), 4))         { OUTPUT = 'PASS: 18 ISqrt(16)=4'; }       else { OUTPUT = 'FAIL: 18'; }
