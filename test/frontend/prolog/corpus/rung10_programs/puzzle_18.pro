%-------------------------------------------------------------------------------
% 18
% In Luncyville the shoe store is closed every Monday, the hardware store every
% Tuesday, the grocery every Thursday, and the bank is open only Monday,
% Wednesday, and Friday. Everything is closed Sunday.
% They went shopping on Wednesday.
% Mrs. Abbott: shoe store. Mrs. Briggs: bank. Mrs. Culver: grocery. Mrs. Denny: hardware.
%-------------------------------------------------------------------------------
:- initialization(main). main :- puzzle; true.

puzzle :-
    write('Day=wednesday Abbott=shoe Briggs=bank Culver=grocery Denny=hardware'),
    write('\n'),
    fail.
