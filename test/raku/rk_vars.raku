# rk_vars.raku — my declarations, assignment, re-assignment
sub main() {
    my $x = 10;
    say($x);
    $x = $x + 5;
    say($x);
    my $y = $x * 2;
    say($y);
    my $s = 'start';
    $s = $s ~ '!';
    say($s);
}
