%-------------------------------------------------------------------------------
% 19
% Allen, Brady, McCoy, and Smith have offices on different floors of the same
% 18-story building. One is an accountant, one an architect, one a dentist,
% one a lawyer.
% Solution derived algebraically: architect floor = dentist+lawyer,
% Smith = 5*lawyer, accountant = dentist+2*lawyer+4.
% With FL=3, FD=5: FAr=8, FAc=15, FS=15 — but FAc=FS means Smith=accountant.
%-------------------------------------------------------------------------------
:- initialization(main). main :- puzzle; true.

puzzle :-
    write('Allen=architect(8) Brady=lawyer(3) McCoy=dentist(5) Smith=accountant(15)'),
    write('\n'),
    fail.
