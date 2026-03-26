```SNOBOL4
*  SCRIP DEMO8 -- Sort 8 integers (SNOBOL4 section)
*  Idiom: Gimpel BSORT insertion sort on ARRAY; LGT drives shifts
        &CASE  = 1
        &TRIM  = 1
        DEFINE('b_sort(a,lo,hi)j,k,v')      :(b_sort_end)
b_sort  j      = lo
b_s1    j      = j + 1
        LE(j, hi)                           :F(RETURN)
        k      = j
        v      = a<j>
b_s2    GT(k, lo)                           :F(b_s_place)
        LGT(a<k - 1>, v)                    :F(b_s_place)
        a<k>   = a<k - 1>
        k      = k - 1                      :(b_s2)
b_s_place
        a<k>   = v                          :(b_s1)
b_sort_end
        a      = ARRAY(8)
        a<1>   = 5  ;  a<2>  = 3  ;  a<3>  = 8  ;  a<4>  = 1
        a<5>   = 9  ;  a<6>  = 2  ;  a<7>  = 7  ;  a<8>  = 4
        b_sort(a, 1, 8)
        out    =
        i      = 0
p_loop  i      = i + 1
        GT(i, 8)                            :S(done)
        out    = IDENT(out) a<i>            :S(p_loop)
        out    = out ' ' a<i>              :(p_loop)
done    OUTPUT = out
END
```

```Icon
# SCRIP DEMO8 -- Sort 8 integers (Icon section)
# Idiom: insertion sort via list subscript shifts
procedure isort(a)
    every i := 2 to *a do {
        v := a[i]
        j := i - 1
        while j >= 1 & a[j] > v do {
            a[j + 1] := a[j]
            j -:= 1
        }
        a[j + 1] := v
    }
    return a
end

procedure main()
    a := [5, 3, 8, 1, 9, 2, 7, 4]
    isort(a)
    out := ""
    every x := !a do {
        if *out > 0 then out ||:= " "
        out ||:= x
    }
    write(out)
end
```

```Prolog
% SCRIP DEMO8 -- Sort 8 integers (Prolog section)
% Idiom: msort/2 built-in; atomic_list_concat for output
:- initialization(main, main).

main :-
    msort([5, 3, 8, 1, 9, 2, 7, 4], Sorted),
    atomic_list_concat(Sorted, ' ', Out),
    write(Out), nl.
```
