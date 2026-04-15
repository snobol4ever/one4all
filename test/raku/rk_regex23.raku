# rk_regex23.raku — RK-23: $s ~~ /pattern/ basic regex match
sub main() {
    my $s = 'Hello, World!';

    # basic substring match
    if ($s ~~ /World/) { say('match World ok'); }
    if ($s ~~ /Hello/) { say('match Hello ok'); }

    # no match
    if ($s ~~ /xyz/) { say('WRONG'); } else { say('no match xyz ok'); }

    # match against variable content
    my $needle = 'World';
    if ($s ~~ /World/) { say('var content match ok'); }

    # empty string subject
    my $empty = '';
    if ($empty ~~ /hello/) { say('WRONG'); } else { say('empty no match ok'); }

    # match at start / end
    if ($s ~~ /Hello/) { say('start match ok'); }
    if ($s ~~ /World!/) { say('end match ok'); }

    # division still works after ~~
    my $x = 10;
    my $y = 2;
    say($x / $y);
}
