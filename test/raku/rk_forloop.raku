# rk_forloop.raku — for RANGE -> $var loop
sub main() {
    my $sum = 0;
    my $i = 1;
    while ($i <= 5) {
        say($i);
        $sum = $sum + $i;
        $i = $i + 1;
    }
    say($sum);
}
