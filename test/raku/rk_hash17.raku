# RK-17: Hash full support — keys/values/pairs/exists/delete

my %h = 0;
%h<x> = 'hello';

# exists / not-exists
say(exists %h<x>);
say(exists %h<y>);

# keys/values/pairs — single entry, deterministic
say(hash_keys(%h));
say(hash_values(%h));
say(hash_pairs(%h));

# delete then re-check
delete %h<x>;
say(exists %h<x>);

# two keys, delete one, check both
%h<a> = '1';
%h<b> = '2';
delete %h<a>;
say(exists %h<a>);
say(exists %h<b>);
say(hash_get(%h, 'b'));
