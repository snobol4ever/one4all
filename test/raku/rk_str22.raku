# rk_str22.raku — RK-22: substr/index/rindex/uc/lc/trim/chars
sub main() {
    my $s = 'Hello, World!';

    # substr: 0-based
    say(substr($s, 0, 5));
    say(substr($s, 7, 5));
    say(substr($s, 7));

    # index: 0-based, -1 if not found
    say(index($s, 'World'));
    say(index($s, 'xyz'));
    say(index($s, 'l'));

    # rindex: last occurrence, 0-based
    say(rindex($s, 'l'));
    say(rindex($s, 'xyz'));

    # uc / lc
    say(uc('hello'));
    say(lc('WORLD'));

    # trim: strip both ends
    my $padded = '  hello  ';
    say(trim($padded));

    # chars / length
    say(chars('hello'));
}
