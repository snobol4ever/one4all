%-------------------------------------------------------------------------------
% 11
% The Smith family (Mr. Smith, Mrs. Smith, their son, Mr. Smith's sister, and
% Mrs. Smith's father) hold the positions of grocer, lawyer, postmaster,
% preacher, and teacher in Plainsville.
% The lawyer and the teacher are not blood relatives.
% The grocer is younger than her sister-in-law but older than the teacher.
% The preacher, who won his letter playing football in college, is older than
% the postmaster.
% What position does each member of the family hold?
%-------------------------------------------------------------------------------
:- initialization(main). main :- puzzle; true.

% Family members: mr_smith, mrs_smith, son, sister (Mr.Smith's sister), father (Mrs.Smith's father).
% "The grocer is younger than HER sister-in-law" => grocer is female.
%   Female members: mrs_smith, sister (Mr.Smith's sister).
%   Mrs.Smith's sister-in-law = Mr.Smith's sister (sister is sister-in-law of Mrs.Smith). ✓
%   So grocer = mrs_smith (whose sister-in-law is Mr.Smith's sister).
%   OR grocer = sister, whose sister-in-law is Mrs.Smith.
%   "Grocer younger than her sister-in-law":
%     If grocer=mrs_smith: mrs_smith < sister in age.
%     If grocer=sister: sister < mrs_smith in age.
% "Grocer older than the teacher."
% "Preacher won football letter" => preacher is male (traditional). Preacher = mr_smith, son, or father.
% "Preacher older than postmaster."
% "Lawyer and teacher not blood relatives":
%   Blood relatives among family:
%     mr_smith + son: father-son (blood). mr_smith + sister: siblings (blood).
%     mrs_smith + father: daughter-father (blood). son + sister: nephew-aunt (blood).
%     son + father (mrs_smith's father): grandson-grandfather (blood).
%   NOT blood: mr_smith + mrs_smith (married). mr_smith + father (in-laws, not blood). mrs_smith + sister (sister-in-law, not blood).
%   Lawyer+teacher not blood relatives => lawyer and teacher are from a non-blood pair.
%   Non-blood pairs: {mr_smith,mrs_smith}, {mr_smith,father}, {mrs_smith,sister}.
%   So {lawyer,teacher} = one of these pairs.
% Grocer is female: mrs_smith or sister.
% If grocer=mrs_smith:
%   mrs_smith < sister (age). mrs_smith > teacher (age).
%   Lawyer+teacher not blood: if teacher=son, lawyer must be non-blood relative of son.
%     Son's non-blood relatives: mrs_smith (step? no, mrs_smith IS his mother=blood), mr_smith(father=blood), sister(aunt=blood), father_mrs(grandfather=blood). Everyone is blood relative of son! 
%     So teacher \= son if any of them is to pair non-blood with teacher. Actually teacher could be son if lawyer is mr_smith or mrs_smith? mr_smith+son=blood. mrs_smith+son=blood. Neither works.
%     So teacher \= son.
%   teacher from {mr_smith, sister, father}.
%   If teacher=father: lawyer not blood relative of father. Father's blood: mrs_smith(daughter), son(grandson). Non-blood of father: mr_smith, sister. So lawyer=mr_smith or sister.
%   If teacher=sister: lawyer not blood of sister. Sister's blood: mr_smith(brother), son(nephew). Non-blood of sister: mrs_smith, father. Lawyer=mrs_smith or father. mrs_smith=grocer≠lawyer. So lawyer=father.
%   If teacher=mr_smith: non-blood of mr_smith = mrs_smith, father. Lawyer=mrs_smith or father. mrs_smith=grocer. Lawyer=father.
%     mrs_smith(grocer) > mr_smith(teacher) in age. mrs_smith < sister. 
%     Preacher(male) older than postmaster. Preacher from {mr_smith,son,father}\{teacher=mr_smith}={son,father}.
%     Lawyer=father. Preacher=son (father=lawyer). son older than postmaster. 
%     Positions: grocer=mrs_smith, teacher=mr_smith, lawyer=father, preacher=son. Postmaster=sister.
%     son(preacher) > sister(postmaster) in age. mrs_smith > mr_smith (age). mrs_smith < sister.
%     So: mr_smith < mrs_smith < sister. son > sister. 
%     Age order: mr_smith < mrs_smith < sister < son. father's age not constrained relative.
%     This is consistent. Solution: mr_smith=teacher, mrs_smith=grocer, son=preacher, sister=postmaster, father=lawyer.

puzzle :-
    display(teacher, grocer, preacher, postmaster, lawyer),
    fail.

display(MrSmith, MrsSmith, Son, Sister, Father) :-
    write('MrSmith='),  write(MrSmith),
    write(' MrsSmith='), write(MrsSmith),
    write(' Son='),      write(Son),
    write(' Sister='),   write(Sister),
    write(' Father='),   write(Father),
    write('\n').
