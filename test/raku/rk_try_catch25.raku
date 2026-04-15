# rk_try_catch25.raku — RK-25: try/CATCH/die exception handling
sub might_die($x) {
    if ($x == 0) { die('zero error'); }
    say($x);
}

sub main() {
    # try with no CATCH — body succeeds
    try { might_die(42); }

    # try that swallows a die
    try { might_die(0); }
    say('after swallowed die');

    # try with CATCH block — handler fires
    try {
        die('test error');
    } CATCH {
        say('caught in handler');
    }

    # try with CATCH — success path (handler should NOT fire)
    try {
        might_die(7);
    } CATCH {
        say('WRONG');
    }

    # die inside sub, caught by outer try
    try { might_die(0); }
    say('outer catch ok');
}
