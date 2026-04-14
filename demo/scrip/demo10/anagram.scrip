```SNOBOL4
*  SCRIP DEMO10 -- Detect anagrams (SNOBOL4 section)
*  Idiom: b_sort char-ARRAY -> canonical key; TABLE groups words;
*         BREAK/SPAN word scan; CONVERT(T,'ARRAY') iterates entries
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
        DEFINE('sort_chars(w)a,i,n,key')    :(sort_chars_end)
sort_chars
        n      = SIZE(w)
        a      = ARRAY(n)
        i      = 0
sc1     i      = i + 1
        GT(i, n)                            :S(sc2)
        a<i>   = SUBSTR(w, i, 1)            :(sc1)
sc2     b_sort(a, 1, n)
        key    =
        i      = 0
sc3     i      = i + 1
        GT(i, n)                            :S(sc_ret)
        key    = key a<i>                   :(sc3)
sc_ret  sort_chars = key                    :(RETURN)
sort_chars_end
        t      = TABLE()
        word   = &LCASE &UCASE
        w_pat  = BREAK(word) SPAN(word) . w
        line   = 'eat tea tan ate nat bat'
w_loop  line   w_pat =                     :F(w_done)
        key    = sort_chars(w)
        t<key> = IDENT(t<key>) w            :S(w_loop)
        t<key> = t<key> ' ' w              :(w_loop)
w_done  rows   = CONVERT(t, 'ARRAY')
        proto  = PROTOTYPE(rows)
        proto  BREAK(',') . n_rows  =
        i      = 0
r_loop  i      = i + 1
        GT(i, +n_rows)                      :S(END)
        entry  = rows<i,2>
        entry  BREAK(' ')                   :S(do_out)
                                            :(r_loop)
do_out  OUTPUT = entry                      :(r_loop)
END
```

```Icon
# SCRIP DEMO10 -- Detect anagrams (Icon section)
# Idiom: sort string chars as list -> canonical key; table groups by key
procedure sort_chars(w)
    chars := []
    every put(chars, !w)
    chars := sort(chars)
    key := ""
    every key ||:= !chars
    return key
end

procedure main()
    words := ["eat", "tea", "tan", "ate", "nat", "bat"]
    t := table()
    every w := !words do {
        key := sort_chars(w)
        /t[key] := []
        put(t[key], w)
    }
    every pair := !sort(t) do {
        if *pair[2] > 1 then {
            out := ""
            every w2 := !pair[2] do {
                if *out > 0 then out ||:= " "
                out ||:= w2
            }
            write(out)
        }
    }
end
```

```Prolog
% SCRIP DEMO10 -- Detect anagrams (Prolog section)
% Idiom: msort on char list gives canonical key; group_pairs_by_key groups
:- initialization(main, main).

main :-
    Words = ["eat","tea","tan","ate","nat","bat"],
    maplist([W,K]>>(string_chars(W,Cs), msort(Cs,S), string_chars(K,S)),
            Words, Keys),
    pairs_keys_values(Pairs, Keys, Words),
    keysort(Pairs, Sorted),
    group_pairs_by_key(Sorted, Groups),
    forall(
        (member(_-Grp, Groups), length(Grp, L), L > 1),
        (atomic_list_concat(Grp, ' ', Line), write(Line), nl)
    ).
```
