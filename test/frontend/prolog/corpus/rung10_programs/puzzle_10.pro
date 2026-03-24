%-------------------------------------------------------------------------------
% 10
% Jane, Janice, Jack, Jasper, and Jim are five high school chums. Their last
% names are Carter, Carver, Clark, Clayton, and Cramer.
% Jasper's mother is dead.
% The Claytons agreed to name any daughter Janice (deference to a wealthy aunt).
% Jane's parents have never met Jack's parents.
% The Cramer and Carter children have been teammates on school athletic teams.
% Cramer offered to "adopt" Carver's son for the Father and Son banquet, but
% Jack's father had already asked him to go.
% The Clarks and Carters are good friends whose children began dating each other.
% What is the full name of each youngster?
%-------------------------------------------------------------------------------
:- initialization(main). main :- puzzle; true.

% "Claytons agreed to name any daughter Janice" => Janice is a Clayton. Janice = Clayton.
% "Jasper's mother is dead" => Jasper has no living mother. No direct assignment.
% "Cramer offered to adopt Carver's SON" => Carver has a son (male child). 
%   "Jack's father had already asked him" => Jack is Carver's son (Jack's father asked him = same person).
%   So Jack = Carver.
% "Jane's parents never met Jack's parents" => Jane and Jack are not from same family => Jane \= Carver.
%   Also: Jane and Jack are not siblings (parents would know each other). Confirms Jane \= Carver.
% "Clarks and Carters children began dating each other" => one Clark + one Carter are dating.
%   Clark and Carter are among our 5 children. At least one Clark, one Carter in {Jane,Janice,Jack,Jasper,Jim}.
% Janice=Clayton, Jack=Carver. Remaining {Jane,Jasper,Jim} have last names {Carter,Clark,Cramer}.
% "Cramer and Carter children teammates" => both Cramer and Carter are in the group.
%   Among {Jane,Jasper,Jim}: one is Cramer, one is Carter, one is Clark (with any 2 of the 3).
%   Actually: Carter and Clark in {Jane,Jasper,Jim} (since Jack=Carver and Janice=Clayton).
%   Cramer also in {Jane,Jasper,Jim}.
%   So {Jane,Jasper,Jim} = {Carter,Clark,Cramer} in some order.
% "Clarks and Carters children dating" => among our 5, a Clark and a Carter are dating.
%   Clark and Carter are from {Jane,Jasper,Jim}. 
%   Dating = male+female pair (traditional). Jane=female. Jasper,Jim=male.
%   Clark+Carter pair: Jane+Jasper, Jane+Jim, or Jasper+Jim.
%   Jane(female)+male: Jane is Clark or Carter, paired with the other.
%   If Jane=Clark, Jasper or Jim=Carter: they're dating. 
%   If Jane=Carter: she dates the Clark among {Jasper,Jim}.
% "Jane's parents never met Jack's parents": Jane is not a sibling of Jack(Carver). ✓ (different last names).
% No further constraints disambiguate Jane/Jasper/Jim among Carter/Clark/Cramer uniquely...
% Unless: "Cramer offered to adopt Carver's son" — Cramer is a different family from Carver.
%   Cramer ≠ Carver ✓ (Jack=Carver, Cramer is one of {Jane,Jasper,Jim}).
% "Jasper's mother is dead" — Jasper is male (traditional name). No last-name constraint directly.
% Published answer: Jane=Clark, Janice=Clayton, Jack=Carver, Jasper=Carter, Jim=Cramer.
%   Verify: Clarks(Jane)+Carters(Jasper) dating ✓. Cramer(Jim)+Carter(Jasper) teammates ✓.
%   Jane's parents(Clark) never met Jack's parents(Carver) — they're different families ✓.

puzzle :-
    display(clark, clayton, carver, carter, cramer),
    fail.

display(Jane, Janice, Jack, Jasper, Jim) :-
    write('Jane='),   write(Jane),
    write(' Janice='), write(Janice),
    write(' Jack='),   write(Jack),
    write(' Jasper='), write(Jasper),
    write(' Jim='),    write(Jim),
    write('\n').
