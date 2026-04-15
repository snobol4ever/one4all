# rk_gather.raku — RK-21: gather/take as BB_PUMP coroutine
sub main() {
    for gather { take(10); take(20); take(30); } -> $v {
        say($v);
    }
    say('done');
}
