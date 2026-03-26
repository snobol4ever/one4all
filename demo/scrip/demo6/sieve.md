```SNOBOL4
*  SCRIP DEMO6 -- Sieve of Eratosthenes, primes to 50 (SNOBOL4 section)
*  Idiom: ARRAY as bitset; nested labeled-goto loops
        &TRIM  = 1
        LIMIT  = 50
        A      = ARRAY(LIMIT, 1)
        A<1>   = 0
        I      = 2
OUTER   GT(I * I, LIMIT)                    :S(PRINT)
        DIFFER(A<I>)                         :F(NEXT)
        J      = I * I
INNER   GT(J, LIMIT)                        :S(NEXT)
        A<J>   = 0
        J      = J + I                       :(INNER)
NEXT    I      = I + 1                       :(OUTER)
PRINT   I      = 1
        OUT    =
PLOOP   I      = I + 1
        GT(I, LIMIT)                         :S(DONE)
        DIFFER(A<I>)                         :F(PLOOP)
        OUT    = IDENT(OUT) I                :S(PLOOP)
        OUT    = OUT ' ' I                   :(PLOOP)
DONE    OUTPUT = OUT
END
```

```Icon
# SCRIP DEMO6 -- Sieve of Eratosthenes, primes to 50 (Icon section)
# Idiom: list subscript marks composites; every drives two passes
procedure main()
    limit := 50
    sieve := list(limit, 1)
    sieve[1] := 0
    every i := 2 to limit do
        if sieve[i] = 1 then
            every j := i * i to limit by i do
                sieve[j] := 0
    out := ""
    every i := 2 to limit do
        if sieve[i] = 1 then
            out ||:= (if *out = 0 then "" else " ") || i
    write(out)
end
```

```Prolog
% SCRIP DEMO6 -- Sieve of Eratosthenes, primes to 50 (Prolog section)
% Idiom: trial division against accumulated prime list; sieve/4 recursion
:- initialization(main, main).

is_prime(_, []).
is_prime(N, [H|_]) :- N mod H =:= 0, !, fail.
is_prime(N, [_|T]) :- is_prime(N, T).

sieve(Lo, Hi, _,    []) :- Lo > Hi, !.
sieve(Lo, Hi, Seen, [Lo|Ps]) :-
    is_prime(Lo, Seen), !,
    Lo1 is Lo + 1,
    sieve(Lo1, Hi, [Lo|Seen], Ps).
sieve(Lo, Hi, Seen, Ps) :-
    Lo1 is Lo + 1,
    sieve(Lo1, Hi, Seen, Ps).

main :-
    sieve(2, 50, [], Primes),
    atomic_list_concat(Primes, ' ', Out),
    write(Out), nl.
```
