# rk_re37.raku — RK-37: m:g/pat/ global match and s/pat/repl/ substitution

sub main() {
    my $s = 'one 1 two 22 three 333';

    # global match: collect all digit sequences
    my @nums = ($s ~~ m:g/\d+/);
    for @nums -> $n { say($n); }

    # substitution: single replace
    my $t = 'hello world';
    $t ~~ s/world/Raku/;
    say($t);

    # substitution: global replace
    my $u = 'aabbcc';
    $u ~~ s/b/X/g;
    say($u);

    # subst no match — string unchanged
    my $v = 'hello';
    $v ~~ s/xyz/WRONG/;
    say($v);

    say('rk_re37 ok');
}
