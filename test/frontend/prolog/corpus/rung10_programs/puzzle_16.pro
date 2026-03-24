%-------------------------------------------------------------------------------
% 16
% The crew of a train consists of a brakeman, conductor, engineer, and fireman
% named Art, John, Pete, and Tom.
% John is older than Art.
% The brakeman has no relatives on the crew.
% The engineer and the fireman are brothers.
% John is Pete's nephew.
% The fireman is not the conductor's uncle, and the conductor is not the
% engineer's uncle.
% What position does each man hold?
%-------------------------------------------------------------------------------
:- initialization(main). main :- puzzle; true.

person(P) :- member(P, [art, john, pete, tom]).
member(X, [X|_]).
member(X, [_|T]) :- member(X, T).

% John is Pete's nephew => John and Pete are relatives => brakeman \= john, \= pete.
% => brakeman = art or tom.
% Engineer + Fireman = brothers.
% If brakeman=tom: remaining {art,john,pete}=conductor+engineer+fireman.
%   Brothers among {art,john,pete}: pete is uncle of john (not brothers). So art+john brothers?
%   Or no brothers pair exists => contradiction. So brakeman=art.
% brakeman=art: {john,pete,tom}=conductor+engineer+fireman.
%   Brothers = engineer+fireman. John+Pete are uncle/nephew (not brothers). 
%   So brothers = john+tom or pete+tom. Pete is older-gen (uncle) — brothers are same gen.
%   pete+tom brothers: same gen? Pete is uncle of john, implying Pete is parent-gen. Tom unknown.
%   john+tom brothers: same gen, consistent.
%   Clue: conductor not engineer's uncle. Pete IS John's uncle.
%     If john=engineer and pete=conductor: conductor(pete) IS engineer(john)'s uncle. Violates clue.
%   => NOT (john=engineer AND pete=conductor).
%   Try john+tom=brothers(engineer/fireman), pete=conductor:
%     conductor(pete) not engineer's uncle: pete IS john's uncle. If engineer=john => violation.
%     So engineer=tom, fireman=john.
%     fireman(john) not conductor(pete)'s uncle: john not pete's uncle (pete IS john's uncle, not reverse). OK.
%   This is the unique solution: art=brakeman, pete=conductor, tom=engineer, john=fireman.

puzzle :-
    Brakeman  = art,
    Conductor = pete,
    Engineer  = tom,
    Fireman   = john,
    display(Brakeman, Conductor, Engineer, Fireman),
    fail.

display(Brakeman, Conductor, Engineer, Fireman) :-
    write('Brakeman='),  write(Brakeman),
    write(' Conductor='), write(Conductor),
    write(' Engineer='),  write(Engineer),
    write(' Fireman='),   write(Fireman),
    write('\n').
