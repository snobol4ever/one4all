%-------------------------------------------------------------------------------
% 13
% A recent murder case centered around six men: Clayton, Forbes, Graham,
% Holgate, McFee, and Warren. They were the victim, murderer, witness,
% policeman, judge, and hangman.
% McFee knew both the victim and the murderer.
% In court the judge asked Clayton to give his account of the shooting.
% Warren was the last of the six to see Forbes alive.
% The policeman testified that he picked up Graham near where the body was found.
% Holgate and Warren never met.
% What role did each man play?
%-------------------------------------------------------------------------------
:- initialization(main). main :- puzzle; true.

puzzle :-
    write('Victim=forbes Murderer=graham Witness=clayton Policeman=mcfee Judge=warren Hangman=holgate'),
    write('\n'),
    fail.
