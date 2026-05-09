# rk_interp.raku — RK-12: string interpolation in double-quoted strings
# $var inside "..." is expanded to the variable's value at runtime.
# The lexer emits LIT_INTERP_STR; the lowerer splits on $ident boundaries
# and builds an AST_CAT chain. Single-quoted strings remain literal.

sub titled($name, $title) {
    say("$title $name");
}

sub main() {
    my $lang = 'Raku';
    my $ver = 6;
    say("language: $lang");
    say("version: $ver");
    my $a = 'hello';
    my $b = 'world';
    say("$a $b");
    titled('Jones', 'Dr');
    say("no interp here");
    say("prefix $lang suffix");
}
