# rk_re34.raku — RK-34: positional captures $0, $1

sub main() {
    my $s = 'John Smith, age 42';

    # single group — capture a word
    if ($s ~~ /([A-Za-z]+)/) { say($0); } else { say('FAIL no match'); }

    # two groups — name and number
    if ($s ~~ /([A-Za-z]+) ([A-Za-z]+)/) {
        say($0);
        say($1);
    } else { say('FAIL two groups'); }

    # digit capture
    my $t = 'score: 99 points';
    if ($t ~~ /([0-9]+)/) { say($0); } else { say('FAIL digit cap'); }

    # no match — $0 stays empty
    my $u = 'hello';
    if ($u ~~ /([0-9]+)/) { say('FAIL should not match'); } else { say('no match ok'); }

    say('rk_re34 ok');
}
