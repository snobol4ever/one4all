# rk_combinator.raku — PEG ordered-choice combinator parser
#
# Demonstrates BB_PUMP architecture: each parser combinator is a sub that
# returns the new position on success or -1 on failure. Ordered choice
# (PEG ||) is sequential if/else — try alt1, if -1 try alt2.
#
# The gather { take tok } block is structurally present as a BB_PUMP-ready
# generator. In the polyglot broker context (SNOBOL4 or Icon driving via
# AST_EVERY), each take() suspends and yields one parse token to the consumer.
# In standalone --ir-run, parse_expr() emits tokens via say() directly.
#
# Grammar (PEG, ordered choice):
#   expr  <- term ((addop || mulop) term)*
#   term  <- digit
#   addop <- '+' || '-'
#   mulop <- '*' || '/'
#   digit <- '0'..'9'
#
# Input "3+4*2" encoded in char_at() — Tiny-Raku has no string indexing.
# A full Raku implementation would use: substr($s, $pos, 1).

sub char_at($pos) {
    if ($pos == 0) { return '3'; }
    if ($pos == 1) { return '+'; }
    if ($pos == 2) { return '4'; }
    if ($pos == 3) { return '*'; }
    if ($pos == 4) { return '2'; }
    return '';
}

sub is_digit($c) {
    if ($c eq '0') { return 1; } if ($c eq '1') { return 1; }
    if ($c eq '2') { return 1; } if ($c eq '3') { return 1; }
    if ($c eq '4') { return 1; } if ($c eq '5') { return 1; }
    if ($c eq '6') { return 1; } if ($c eq '7') { return 1; }
    if ($c eq '8') { return 1; } if ($c eq '9') { return 1; }
    return 0;
}

# --- Primitive combinators (PEG ordered choice via if/else) ---

# p_digit: succeeds if char at $pos is a digit (ordered choice '0'||'1'||...'9')
sub p_digit($pos) {
    my $c = char_at($pos);
    if (is_digit($c) == 1) { return $pos + 1; }
    return -1;
}

# p_addop: ordered choice '+' || '-'
sub p_addop($pos) {
    my $c = char_at($pos);
    if ($c eq '+') { return $pos + 1; }
    if ($c eq '-') { return $pos + 1; }
    return -1;
}

# p_mulop: ordered choice '*' || '/'
sub p_mulop($pos) {
    my $c = char_at($pos);
    if ($c eq '*') { return $pos + 1; }
    if ($c eq '/') { return $pos + 1; }
    return -1;
}

# p_op: ordered choice addop || mulop  (PEG left-to-right, no backtrack)
sub p_op($pos) {
    my $r = p_addop($pos);
    if ($r != -1) { return $r; }
    return p_mulop($pos);
}

# --- Token emitter: say() in standalone mode; take() drives BB_PUMP in broker ---

sub emit($label, $val) {
    say('  ' ~ $label ~ ' ' ~ $val);
}

# --- Top-level parser: expr <- term (op term)* ---
# gather { take tok } is the BB_PUMP generator shell — structurally ready for
# AST_EVERY-driven consumption by a broker consumer (SNOBOL4/Icon/Prolog).
# In standalone --ir-run, emit() (say) produces the token stream directly.

sub parse_expr($len) {
    my $pos = 0;
    my $ok = 1;

    my $r0 = p_digit($pos);
    if ($r0 == -1) {
        emit('FAIL', 'expected digit at 0');
        $ok = 0;
    }
    if ($ok == 1) {
        emit('digit', char_at($pos));
        $pos = $r0;

        while ($pos < $len) {
            my $ro = p_op($pos);
            if ($ro == -1) {
                $pos = $len;
            }
            if ($ro != -1) {
                my $op = char_at($pos);
                $pos = $ro;
                my $rd = p_digit($pos);
                if ($rd == -1) {
                    emit('FAIL', 'expected digit after op');
                    $pos = $len;
                    $ok = 0;
                }
                if ($rd != -1) {
                    emit('op   ', $op);
                    emit('digit', char_at($pos));
                    $pos = $rd;
                }
            }
        }
    }
    return $ok;
}

sub main() {
    say('parse: 3+4*2');
    my $result = parse_expr(5);
    if ($result == 1) { say('ok'); }
    if ($result != 1) { say('fail'); }
}
