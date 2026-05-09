% coverage_pl_nodes.pl — exercises every Prolog IR node kind
% Covers: AST_CLAUSE AST_CHOICE AST_UNIFY AST_CUT AST_FNC AST_QLIT AST_ILIT AST_FLIT
%         AST_VART AST_ADD AST_SUB AST_MPY AST_DIV AST_TRAIL_MARK AST_TRAIL_UNWIND

% AST_CLAUSE + AST_CHOICE — predicate with multiple clauses (choice point)
color(red).
color(green).
color(blue).

% AST_UNIFY — unification
unify_test(X, X).

% AST_CUT — cut
first_color(X) :- color(X), !.

% AST_FNC — builtin call (write/1, nl/0, is/2)
% AST_ILIT — integer literal
% AST_ADD AST_SUB AST_MPY AST_DIV — arithmetic
arith_test :-
    X is 3 + 4,
    Y is 10 - 3,
    Z is 3 * 4,
    W is 10 / 2,
    write(X), nl,
    write(Y), nl,
    write(Z), nl,
    write(W), nl.

% AST_QLIT — atom literal
atom_test :-
    X = hello,
    write(X), nl.

% AST_FLIT — float literal
float_test :-
    X is 1.5 + 0.5,
    write(X), nl.

% AST_VART — variable
var_test(X) :-
    write(X), nl.

% AST_TRAIL_MARK + AST_TRAIL_UNWIND — backtracking exercises the trail
trail_test :-
    color(X),
    write(X), nl,
    fail.
trail_test.

:- write(start), nl.
:- arith_test.
:- atom_test.
:- float_test.
:- var_test(world).
:- first_color(C), write(C), nl.
:- unify_test(hello, hello), write(unified), nl.
:- trail_test.
:- write(done), nl.
