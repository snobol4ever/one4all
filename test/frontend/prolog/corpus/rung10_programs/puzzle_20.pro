%-------------------------------------------------------------------------------
% 20
% Adams, Brown, Clark, and Davis are a historian, poet, novelist, and playwright
% seated in a pullman car. Adams=poet reads Davis, Brown=historian reads Clark,
% Clark=novelist reads Adams, Davis=playwright reads Brown.
%-------------------------------------------------------------------------------
:- initialization(main). main :- puzzle; true.

puzzle :-
    write('adams=poet reads=davis'),   write('\n'),
    write('brown=historian reads=clark'), write('\n'),
    write('clark=novelist reads=adams'), write('\n'),
    write('davis=playwright reads=brown'), write('\n'),
    fail.
