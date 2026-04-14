# rk_strings.raku — string concat, eq/ne, single/double quoted literals
sub main() {
    my $a = 'hello';
    my $b = 'world';
    say($a ~ ' ' ~ $b);
    if ($a eq 'hello') { say('eq literal ok'); }
    if ($a eq $b) { say('WRONG'); } else { say('ne vars ok'); }
    my $c = $a ~ $b;
    say($c);
    if ($c eq 'helloworld') { say('concat eq ok'); }
}
