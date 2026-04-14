# rk_control.raku — if/else, while, nested conditions
sub main() {
    my $x = 3;
    if ($x == 3) { say('three'); } else { say('not three'); }
    if ($x == 5) { say('five');  } else { say('not five');  }
    my $i = 1;
    while ($i <= 4) {
        say($i);
        $i = $i + 1;
    }
    if ($x > 1) { say('gt one'); }
    if ($x < 10) { say('lt ten'); }
}
