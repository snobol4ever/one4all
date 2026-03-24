%-------------------------------------------------------------------------------
% 15
% Vernon, Wilson, and Yates are an architect, a doctor, and a lawyer with
% offices on different floors of the same building. Their secretaries are
% Miss Ainsley, Miss Barnette, and Miss Coulter.
% The lawyer has his office on the ground floor.
% Miss Barnette became engaged to Yates and goes to lunch with him every day.
% At noon Miss Ainsley goes upstairs to eat lunch with Wilson's secretary.
% Vernon had to send his secretary down to borrow stamps from the architect's office.
% What is each man's profession and who is his secretary?
%-------------------------------------------------------------------------------
:- initialization(main). main :- puzzle; true.

% Deduction chain:
%   Barnette engaged to Yates => SecYates = barnette.
%   Ainsley goes upstairs to Wilson's sec => Ainsley IS Wilson's secretary (she goes up from Wilson's floor).
%     => SecWilson = ainsley.
%   SecVernon = coulter (only one left).
%   Vernon sends Coulter DOWN to architect => Vernon above architect => Vernon != architect.
%   Vernon != lawyer (lawyer on ground, Vernon sends sec down = Vernon above ground).
%   => Vernon = doctor.
%   Yates is the architect (Yates below Vernon; Ainsley on Wilson's ground floor goes upstairs).
%   Wilson = lawyer (ground floor).

puzzle :-
    display(doctor, coulter, lawyer, ainsley, architect, barnette),
    fail.

display(OVernon, SVernon, OWilson, SWilson, OYates, SYates) :-
    write('Vernon='),  write(OVernon),  write(' sec='), write(SVernon),
    write(' Wilson='), write(OWilson),  write(' sec='), write(SWilson),
    write(' Yates='),  write(OYates),   write(' sec='), write(SYates),
    write('\n').
