```SNOBOL4
*  SCRIP DEMO5 -- Fibonacci first 10 (SNOBOL4 section)
*  Idiom: iterative labeled-goto loop; two accumulators
        &CASE  = 1
        &TRIM  = 1
        a      = 0
        b      = 1
        n      = 0
loop    OUTPUT = a
        n      = n + 1
        LT(n, 10)                           :F(END)
        t      = b
        b      = a + b
        a      = t                          :(loop)
END
```

```Icon
# SCRIP DEMO5 -- Fibonacci first 10 (Icon section)
# Idiom: suspend generator produces the sequence lazily
procedure fibs()
    a := 0
    b := 1
    repeat {
        suspend a
        a :=: b
        b +:= a
    }
end

procedure main()
    every write(fibs() \ 10)
end
```

```Prolog
% SCRIP DEMO5 -- Fibonacci first 10 (Prolog section)
% Idiom: fib/2 accumulator rule; forall drives output
:- initialization(main, main).

fib(N, F) :- fib(N, 0, 1, F).
fib(0, F, _, F) :- !.
fib(N, A, B, F) :- N > 0, N1 is N - 1, B1 is A + B, fib(N1, B, B1, F).

main :-
    forall(between(0, 9, N), (fib(N, F), write(F), nl)).
```
