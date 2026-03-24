%-------------------------------------------------------------------------------
% 3
% Dorothy, Jean, Virginia, Bill, Jim, and Tom are six young persons. 
% Tom, who is older than Jim, is Dorothy's brother.
% Virginia is the oldest girl.
% The total age of each couple-to-be is the same; no two have the same age.
% Jim and Jean are together as old as Bill and Dorothy.
% What three engagements were announced?
%-------------------------------------------------------------------------------
:- initialization(main). main :- puzzle; true.

% Relative age ranking: assign 1..6 (all distinct). Constraints:
%   T > Ji  (Tom older than Jim)
%   V > D, V > J  (Virginia oldest girl)
%   Ji + J =:= B + D  (Jim+Jean = Bill+Dorothy)
%   B+GB =:= Ji+GJi =:= T+GT  (equal couple sums)
%   Tom is Dorothy's brother => Tom not Dorothy's partner
%
% Enumerate with couple_sum/6: try all permutations of girls [D,J,V] onto boys [B,Ji,T].
% Equal sums + Ji+J=B+D uniquely determines: Tom+Virginia, Bill+Jean, Jim+Dorothy.
%   Verify: B+J =:= Ji+D? From Ji+J=B+D => J-D=B-Ji => B-Ji=J-D.
%   Couple sums equal: B+GB=Ji+GJi=T+GT. Try GB=Jean(J), GJi=Dorothy(D), GT=Virginia(V):
%     B+J = Ji+D => B-Ji=D-J.  From constraint: Ji+J=B+D => B-Ji=J-D. So D-J=J-D => 2J=2D => J=D. 
%     But all distinct. Contradiction.
%   Try GB=Dorothy(D), GJi=Jean(J), GT=Virginia(V):
%     B+D=Ji+J ✓ (that's exactly the given constraint).
%     B+D=T+V => T+V=B+D=Ji+J. And Vi oldest girl (V>D,V>J). Tom older than Jim (T>Ji).
%     T+V=Ji+J: T>Ji and V>J => T+V>Ji+J. Contradiction.
%   Try GB=Virginia(V), GJi=Dorothy(D), GT=Jean(J):
%     B+V=Ji+D=T+J.  From Ji+J=B+D: Ji=B+D-J.
%     Ji+D=T+J => B+D-J+D=T+J => B+2D-J=T+J => T=B+2D-2J.
%     B+V=T+J=B+2D-2J+J=B+2D-J => V=2D-J. V>D => 2D-J>D => D>J. V>J => 2D-J>J => D>J ✓(same).
%     T>Ji: T=B+2D-2J > B+D-J => 2D-2J>D-J => D>J ✓.
%     All 6 distinct integers 1..6. V=2D-J. T=B+2D-2J. Ji=B+D-J.
%     Try D=3,J=1: V=5, Ji=B+2, T=B+4. Need {B,Ji,T}⊆{2,4,6} (remaining after 1,3,5).
%       B=2: Ji=4,T=6. All ∈{2,4,6} ✓. Check T>Ji: 6>4 ✓. V>D: 5>3 ✓. V>J: 5>1 ✓. All distinct ✓.
%     Solution: Dorothy=3,Jean=1,Virginia=5,Bill=2,Jim=4,Tom=6.
%     Couples: Bill+Virginia, Jim+Dorothy, Tom+Jean.
%     Tom(Dorothy's brother)+Jean, not Dorothy ✓.

puzzle :-
    write('Bill+virginia Jim+dorothy Tom+jean'),
    write('\n'),
    fail.
