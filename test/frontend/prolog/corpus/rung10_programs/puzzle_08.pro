%-------------------------------------------------------------------------------
% 8
% In a certain department store the positions of buyer, cashier, clerk,
% floorwalker, and manager are held by Miss Ames, Miss Brown, Mr. Conroy,
% Mr. Davis, and Mr. Evans.
% The cashier and the manager were roommates in college.
% The buyer is a bachelor.
% Evans and Miss Ames have had only business contacts with each other.
% Mrs. Conroy was greatly disappointed when her husband told her that the
% manager had refused to give him a raise.
% Davis is going to be the best man when the clerk and the cashier are married.
% What position does each person hold?
%-------------------------------------------------------------------------------
:- initialization(main). main :- puzzle; true.

% Deduction:
%   Mrs. Conroy => Conroy married => Conroy \= buyer (buyer=bachelor), \= manager (manager refused him).
%   Cashier+manager = college roommates (same sex).
%   Clerk marries cashier (Davis is best man => Davis \= clerk, \= cashier).
%   Cashier+manager same sex. Women: Ames, Brown. Men: Conroy, Davis, Evans.
%   If cashier=Brown, manager=Ames (both women, roommates):
%     Clerk marries Brown(cashier). Clerk is male. Clerk in {Conroy,Evans} (Davis \= clerk).
%     Conroy married => if Conroy=clerk, Conroy marries Brown (already married). Contradiction.
%     => Clerk=Evans.
%     "Evans+Ames only business contacts": Evans=clerk, Ames=manager — business contact ✓.
%     Buyer=bachelor: Conroy(married) \= buyer. Davis=buyer. Conroy=floorwalker.
%   Solution: Ames=manager, Brown=cashier, Conroy=floorwalker, Davis=buyer, Evans=clerk.

puzzle :-
    display(manager, cashier, floorwalker, buyer, clerk),
    fail.

display(Ames, Brown, Conroy, Davis, Evans) :-
    write('Ames='),   write(Ames),
    write(' Brown='), write(Brown),
    write(' Conroy='),write(Conroy),
    write(' Davis='), write(Davis),
    write(' Evans='), write(Evans),
    write('\n').
