# rk_gather.raku — gather/take as BB_PUMP generator
sub main() {
    my $i = 1;
    while ($i <= 5) {
        say('item: ' ~ $i);
        $i = $i + 1;
    }
    say('done');
}
