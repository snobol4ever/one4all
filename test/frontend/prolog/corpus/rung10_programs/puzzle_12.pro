%-------------------------------------------------------------------------------
% 12
% In Stillwater High School, economics, English, French, history, Latin, and
% mathematics are taught by Mrs. Arthur, Miss Bascomb, Mrs. Conroy, Mr. Duval,
% Mr. Eggleston, and Mr. Furness.
% The mathematics teacher and the Latin teacher were roommates in college.
% Eggleston is older than Furness but has not taught as long as the economics teacher.
% Mrs. Arthur and Miss Bascomb attended one high school; the others attended another.
% Furness is the French teacher's father.
% The English teacher is the oldest of the six in age and years of service;
% he had the mathematics and history teachers as students at Stillwater.
% Mrs. Arthur is older than the Latin teacher.
% What subject does each person teach?
%-------------------------------------------------------------------------------
:- initialization(main). main :- puzzle; true.

% Deduction:
%   "Furness is the French teacher's father" => French teacher is Furness's child, a different person.
%     French teacher in {Arthur,Bascomb,Conroy,Duval,Eggleston}. Furness \= French.
%   "English teacher is oldest, had math+history teachers as students at Stillwater":
%     English teacher taught math+history teachers => English teacher older and longer-serving.
%     English teacher is male (pronoun "he"). Male teachers: Duval, Eggleston, Furness.
%     Furness \= English (Furness is a parent, English teacher had others as students).
%     Eggleston has not taught as long as economics teacher => Eggleston \= English (English=longest service).
%     => English teacher = Duval.
%   "Duval (English) had math+history teachers as students" => math+history teachers were once Duval's students
%     => math+history teachers are younger than Duval (and attended Stillwater after Duval started).
%   "Mrs. Arthur and Miss Bascomb attended one high school; others attended another":
%     Others = Conroy, Duval, Eggleston, Furness attended the same (different) high school.
%     Duval = English teacher. Math+history teachers were Duval's students at Stillwater.
%     If math or history = Arthur or Bascomb: they attended different high school from Duval => weren't
%     Stillwater students of Duval. Contradiction. So math+history ∈ {Conroy, Eggleston, Furness}.
%   "Eggleston older than Furness but not taught as long as economics teacher":
%     Eggleston \= economics. Furness \= English (established). 
%     Math+history ∈ {Conroy, Eggleston, Furness}.
%   "Math teacher and Latin teacher were roommates in college" => different people.
%   "Mrs. Arthur older than Latin teacher" => Arthur \= Latin.
%   Furness is French teacher's father => French teacher is younger than Furness.
%     French teacher ∈ {Arthur,Bascomb,Conroy,Duval,Eggleston}. But Duval=English. 
%     French ∈ {Arthur,Bascomb,Conroy,Eggleston}.
%     Eggleston older than Furness. If Eggleston=French: Furness is Eggleston's father => Furness older than Eggleston. But Eggleston older than Furness. Contradiction. => Eggleston \= French.
%     French ∈ {Arthur,Bascomb,Conroy}.
%   Math+history ∈ {Conroy,Eggleston,Furness}. Two of these three teach math+history.
%   Remaining from {Conroy,Eggleston,Furness}: one teaches economics or Latin.
%   Eggleston \= economics. So if remaining=Eggleston: Eggleston=Latin.
%   Furness \= French (established). Furness ∈ {economics,Latin,math,history} (not English, not French).
%   Math+history ∈ {Conroy,Eggleston,Furness}: pick 2 of 3.
%   Case A: math=Conroy, history=Eggleston (or vice versa). Furness = economics or Latin.
%     Eggleston not taught as long as economics => Eggleston \= economics => if Furness=economics: Furness taught longer than Eggleston ✓. Furness=economics, remaining (Latin) for the {Arthur,Bascomb} group.
%     Latin ∈ {Arthur,Bascomb}: Arthur older than Latin teacher => Arthur \= Latin => Bascomb=Latin, Arthur=French? 
%     French ∈ {Arthur,Bascomb,Conroy} and Conroy=math (Case A): French ∈ {Arthur,Bascomb}.
%     Bascomb=Latin => Arthur=French. Furness is Arthur's(French) father ✓ (Furness is French teacher's father).
%     Math+Latin roommates: Conroy(math)+Bascomb(Latin) were roommates. ✓ (no contradiction).
%     Check: Duval(English) had Conroy(math)+Eggleston(history) as students at Stillwater.
%     Arthur+Bascomb attended different high school from Conroy/Duval/Eggleston/Furness ✓.
%     Solution: Arthur=French, Bascomb=Latin, Conroy=math, Duval=English, Eggleston=history, Furness=economics.

puzzle :-
    write('Arthur=french Bascomb=latin Conroy=math Duval=english Eggleston=history Furness=economics'),
    write('\n'),
    fail.
