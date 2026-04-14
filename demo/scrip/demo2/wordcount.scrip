```SNOBOL4
*  SCRIP DEMO2 -- Word Count (SNOBOL4 section)
*  Idiom: BREAK/SPAN word boundary pattern; subject replacement loop
*  Ref: Gimpel wordcount.sno idiom
        &CASE  = 1
        &TRIM  = 1
        word   = &UCASE &LCASE
        w_pat  = BREAK(word) SPAN(word)
        line   = 'the quick brown fox jumps over the lazy dog'
        n      = 0
next_w  line   w_pat =                     :F(done)
        n      = n + 1                     :(next_w)
done    OUTPUT = n
END
```

```Icon
# SCRIP DEMO2 -- Word Count (Icon section)
# Idiom: string scanning with tab(upto)/tab(many) generator
procedure main()
    s := "the quick brown fox jumps over the lazy dog";
    count := 0;
    s ? {
        while tab(upto(&letters)) do {
            tab(many(&letters));
            count +:= 1;
        }
    };
    write(count);
end
```

```Prolog
% SCRIP DEMO2 -- Word Count (Prolog section)
% Idiom: DCG rules tokenise char list; phrase/3 counts words
:- initialization(main, main).

whites --> [].
whites --> [C], { char_type(C, space) }, whites.

word([C|Cs]) --> [C], { char_type(C, alpha) }, word(Cs).
word([])     --> [].

words([])     --> whites.
words([W|Ws]) --> whites, word(W), { W \= [] }, words(Ws).

count_words(Str, N) :-
    string_chars(Str, Chars),
    phrase(words(Ws), Chars, []),
    length(Ws, N).

main :-
    count_words("the quick brown fox jumps over the lazy dog", N),
    write(N), nl.
```
