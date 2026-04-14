# rk_subs.raku — sub definitions, single/multi params, return values
sub double($n) {
    return $n * 2;
}
sub greet($name) {
    say('hello ' ~ $name);
}
sub add($a, $b) {
    return $a + $b;
}
sub classify($n) {
    if ($n < 0)  { return 'negative'; }
    if ($n == 0) { return 'zero';     }
    return 'positive';
}
sub main() {
    say(double(7));
    greet('raku');
    say(add(3, 4));
    say(classify(5));
    say(classify(0));
    say(classify(-1));
}
