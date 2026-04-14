```SNOBOL4
*  SCRIP DEMO6 -- Sieve of Eratosthenes, primes to 50 (SNOBOL4 section)
*  Idiom: ARRAY as bitset; nested labeled-goto loops
        &CASE  = 1
        &TRIM  = 1
        limit  = 50
        a      = ARRAY(limit, 1)
        a<1>   = 0
        i      = 2
outer   GT(i * i, limit)                    :S(print)
        EQ(a<i>, 1)                         :F(next)
        j      = i * i
inner   GT(j, limit)                        :S(next)
        a<j>   = 0
        j      = j + i                      :(inner)
next    i      = i + 1                      :(outer)
print   i      = 1
        out    =
p_loop  i      = i + 1
        GT(i, limit)                        :S(done)
        EQ(a<i>, 1)                         :F(p_loop)
        out    = IDENT(out) i               :S(p_loop)
        out    = out ' ' i                  :(p_loop)
done    OUTPUT = out
END
```

```Icon
# SCRIP DEMO6 -- Sieve of Eratosthenes, primes to 50 (Icon section)
# Idiom: list subscript marks composites; every drives two passes
procedure main()
    limit := 50;
    sieve := list(limit, 1);
    sieve[1] := 0;
    every i := 2 to limit do
        if sieve[i] = 1 then
            every j := i * i to limit by i do
                sieve[j] := 0;
    out := "";
    every i := 2 to limit do
        if sieve[i] = 1 then {
            if *out > 0 then out ||:= " ";
            out ||:= i;
        };
    write(out);
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
