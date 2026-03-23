/*
 * icon_lex_test.c — Unit tests for icon_lex.c
 *
 * M-ICON-LEX acceptance criteria:
 *  1. All keywords tokenized correctly
 *  2. All operators tokenized correctly
 *  3. Integer / real / string / cset literals parsed correctly
 *  4. Identifiers recognized
 *  5. EOF produced at end
 *  6. Every token in the 6 Rung 1 corpus programs tokenized
 *  7. No auto-semicolon insertion
 */

#include "icon_lex.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static int tests_run = 0;
static int tests_pass = 0;
static int tests_fail = 0;

#define PASS(name) do { tests_run++; tests_pass++; printf("  PASS  %s\n", name); } while(0)
#define FAIL(name, ...) do { tests_run++; tests_fail++; printf("  FAIL  %s: ", name); printf(__VA_ARGS__); printf("\n"); } while(0)

/* -------------------------------------------------------------------------
 * Helper: tokenize src, fill toks[max], return count
 * -------------------------------------------------------------------------*/
static int tokenize_all(const char *src, IcnToken *toks, int max) {
    IcnLexer lx;
    icn_lex_init(&lx, src);
    int n = 0;
    for (;;) {
        IcnToken t = icn_lex_next(&lx);
        if (n < max) toks[n++] = t;
        if (t.kind == TK_EOF || t.kind == TK_ERROR || n >= max) break;
    }
    return n;
}

/* Expect exactly one non-EOF token of given kind */
static int expect1(const char *src, IcnTkKind expected, const char *test_name) {
    IcnToken toks[4];
    int n = tokenize_all(src, toks, 4);
    if (n < 1) { FAIL(test_name, "no tokens"); return 0; }
    if (toks[0].kind != expected) {
        FAIL(test_name, "got %s, want %s", icn_tk_name(toks[0].kind), icn_tk_name(expected));
        return 0;
    }
    PASS(test_name);
    return 1;
}

/* =========================================================================
 * Test groups
 * ======================================================================= */

static void test_keywords(void) {
    printf("--- keywords ---\n");
    struct { const char *src; IcnTkKind kind; } cases[] = {
        {"to",        TK_TO},
        {"by",        TK_BY},
        {"every",     TK_EVERY},
        {"do",        TK_DO},
        {"if",        TK_IF},
        {"then",      TK_THEN},
        {"else",      TK_ELSE},
        {"while",     TK_WHILE},
        {"until",     TK_UNTIL},
        {"repeat",    TK_REPEAT},
        {"return",    TK_RETURN},
        {"suspend",   TK_SUSPEND},
        {"fail",      TK_FAIL},
        {"break",     TK_BREAK},
        {"next",      TK_NEXT},
        {"not",       TK_NOT},
        {"procedure", TK_PROCEDURE},
        {"end",       TK_END},
        {"global",    TK_GLOBAL},
        {"local",     TK_LOCAL},
        {"static",    TK_STATIC},
        {"record",    TK_RECORD},
        {"case",      TK_CASE},
        {"of",        TK_OF},
        {"default",   TK_DEFAULT},
        {NULL, 0}
    };
    for (int i = 0; cases[i].src; i++)
        expect1(cases[i].src, cases[i].kind, cases[i].src);
}

static void test_operators(void) {
    printf("--- operators ---\n");
    struct { const char *src; IcnTkKind kind; } cases[] = {
        {"+",    TK_PLUS},
        {"-",    TK_MINUS},
        {"*",    TK_STAR},
        {"/",    TK_SLASH},
        {"%",    TK_MOD},
        {"^",    TK_CARET},
        {"<",    TK_LT},
        {"<=",   TK_LE},
        {">",    TK_GT},
        {">=",   TK_GE},
        {"=",    TK_EQ},
        {"~=",   TK_NEQ},
        {"<<",   TK_SLT},
        {"<<=",  TK_SLE},
        {">>",   TK_SGT},
        {">>=",  TK_SGE},
        {"==",   TK_SEQ},
        {"~==",  TK_SNE},
        {"||",   TK_CONCAT},
        {"|||",  TK_LCONCAT},
        {":=",   TK_ASSIGN},
        {":=:",  TK_SWAP},
        {"<-",   TK_REVASSIGN},
        {"+:=",  TK_AUGPLUS},
        {"-:=",  TK_AUGMINUS},
        {"*:=",  TK_AUGSTAR},
        {"/:=",  TK_AUGSLASH},
        {"%:=",  TK_AUGMOD},
        {"||:=", TK_AUGCONCAT},
        {"&",    TK_AND},
        {"|",    TK_BAR},
        {"\\",   TK_BACKSLASH},
        {"!",    TK_BANG},
        {"?",    TK_QMARK},
        {"@",    TK_AT},
        {"~",    TK_TILDE},
        {".",    TK_DOT},
        {NULL, 0}
    };
    for (int i = 0; cases[i].src; i++)
        expect1(cases[i].src, cases[i].kind, cases[i].src);
}

static void test_punctuation(void) {
    printf("--- punctuation ---\n");
    struct { const char *src; IcnTkKind kind; } cases[] = {
        {"(", TK_LPAREN},
        {")", TK_RPAREN},
        {"{", TK_LBRACE},
        {"}", TK_RBRACE},
        {"[", TK_LBRACK},
        {"]", TK_RBRACK},
        {",", TK_COMMA},
        {";", TK_SEMICOL},
        {":", TK_COLON},
        {NULL, 0}
    };
    for (int i = 0; cases[i].src; i++)
        expect1(cases[i].src, cases[i].kind, cases[i].src);
}

static void test_integer_literals(void) {
    printf("--- integer literals ---\n");
    struct { const char *src; long expected; } cases[] = {
        {"0",     0},
        {"1",     1},
        {"42",    42},
        {"1000",  1000},
        {"0xff",  255},
        {"0xFF",  255},
        {NULL, 0}
    };
    for (int i = 0; cases[i].src; i++) {
        IcnToken toks[4];
        tokenize_all(cases[i].src, toks, 4);
        char name[64]; snprintf(name, sizeof(name), "int %s", cases[i].src);
        if (toks[0].kind != TK_INT) {
            FAIL(name, "not TK_INT, got %s", icn_tk_name(toks[0].kind)); continue;
        }
        if (toks[0].val.ival != cases[i].expected) {
            FAIL(name, "value %ld want %ld", toks[0].val.ival, cases[i].expected); continue;
        }
        PASS(name);
    }
}

static void test_real_literals(void) {
    printf("--- real literals ---\n");
    struct { const char *src; double expected; } cases[] = {
        {"3.14",  3.14},
        {"1.0",   1.0},
        {"1e5",   100000.0},
        {"1.5e2", 150.0},
        {"1e-3",  0.001},
        {NULL, 0}
    };
    for (int i = 0; cases[i].src; i++) {
        IcnToken toks[4];
        tokenize_all(cases[i].src, toks, 4);
        char name[64]; snprintf(name, sizeof(name), "real %s", cases[i].src);
        if (toks[0].kind != TK_REAL) {
            FAIL(name, "not TK_REAL, got %s", icn_tk_name(toks[0].kind)); continue;
        }
        double diff = toks[0].val.fval - cases[i].expected;
        if (diff < -1e-9 || diff > 1e-9) {
            FAIL(name, "value %g want %g", toks[0].val.fval, cases[i].expected); continue;
        }
        PASS(name);
    }
}

static void test_string_literals(void) {
    printf("--- string literals ---\n");

    /* Basic string */
    {
        IcnToken toks[4];
        tokenize_all("\"hello\"", toks, 4);
        if (toks[0].kind == TK_STRING && strcmp(toks[0].val.sval.data, "hello") == 0) PASS("\"hello\"");
        else FAIL("\"hello\"", "got kind=%s val=%s", icn_tk_name(toks[0].kind),
                  toks[0].kind == TK_STRING ? toks[0].val.sval.data : "?");
    }

    /* Escape sequences */
    {
        IcnToken toks[4];
        tokenize_all("\"a\\nb\"", toks, 4);
        if (toks[0].kind == TK_STRING && toks[0].val.sval.data[1] == '\n') PASS("escape \\n");
        else FAIL("escape \\n", "bad escape");
    }

    /* Empty string */
    {
        IcnToken toks[4];
        tokenize_all("\"\"", toks, 4);
        if (toks[0].kind == TK_STRING && toks[0].val.sval.len == 0) PASS("empty string");
        else FAIL("empty string", "kind=%s len=%zu", icn_tk_name(toks[0].kind),
                  toks[0].kind == TK_STRING ? toks[0].val.sval.len : 999);
    }

    /* "done" — appears in corpus */
    {
        IcnToken toks[4];
        tokenize_all("\"done\"", toks, 4);
        if (toks[0].kind == TK_STRING && strcmp(toks[0].val.sval.data, "done") == 0) PASS("\"done\"");
        else FAIL("\"done\"", "bad");
    }
}

static void test_cset_literals(void) {
    printf("--- cset literals ---\n");
    {
        IcnToken toks[4];
        tokenize_all("'abc'", toks, 4);
        if (toks[0].kind == TK_CSET && strcmp(toks[0].val.sval.data, "abc") == 0) PASS("'abc'");
        else FAIL("'abc'", "got kind=%s", icn_tk_name(toks[0].kind));
    }
    {
        IcnToken toks[4];
        tokenize_all("''", toks, 4);
        if (toks[0].kind == TK_CSET && toks[0].val.sval.len == 0) PASS("empty cset");
        else FAIL("empty cset", "bad");
    }
}

static void test_identifiers(void) {
    printf("--- identifiers ---\n");
    struct { const char *src; } cases[] = {
        {"main"}, {"write"}, {"x"}, {"foo_bar"}, {"_priv"}, {"i"}, {"n"},
        {NULL}
    };
    for (int i = 0; cases[i].src; i++) {
        IcnToken toks[4];
        tokenize_all(cases[i].src, toks, 4);
        char name[64]; snprintf(name, sizeof(name), "ident %s", cases[i].src);
        if (toks[0].kind == TK_IDENT && strcmp(toks[0].val.sval.data, cases[i].src) == 0)
            PASS(name);
        else
            FAIL(name, "got kind=%s", icn_tk_name(toks[0].kind));
    }
}

static void test_eof(void) {
    printf("--- EOF ---\n");
    {
        IcnToken toks[4];
        int n = tokenize_all("", toks, 4);
        if (n >= 1 && toks[0].kind == TK_EOF) PASS("empty source → EOF");
        else FAIL("empty source → EOF", "n=%d kind=%s", n, n>0 ? icn_tk_name(toks[0].kind) : "?");
    }
    /* Multiple calls after EOF still return EOF */
    {
        IcnLexer lx; icn_lex_init(&lx, "");
        IcnToken a = icn_lex_next(&lx);
        IcnToken b = icn_lex_next(&lx);
        if (a.kind == TK_EOF && b.kind == TK_EOF) PASS("repeated EOF");
        else FAIL("repeated EOF", "a=%s b=%s", icn_tk_name(a.kind), icn_tk_name(b.kind));
    }
}

static void test_comments(void) {
    printf("--- comments ---\n");
    {
        IcnToken toks[4];
        int n = tokenize_all("# this is a comment\n42", toks, 4);
        if (n >= 1 && toks[0].kind == TK_INT && toks[0].val.ival == 42) PASS("# comment skipped");
        else FAIL("# comment skipped", "n=%d kind=%s", n, n>0 ? icn_tk_name(toks[0].kind) : "?");
    }
}

/* =========================================================================
 * Rung 1 corpus tokenization tests
 * Verify every token in each corpus file tokenizes without error.
 * ======================================================================= */

static void test_corpus_file(const char *path) {
    FILE *f = fopen(path, "r");
    if (!f) {
        char name[256]; snprintf(name, sizeof(name), "corpus %s", path);
        FAIL(name, "cannot open file");
        return;
    }
    fseek(f, 0, SEEK_END);
    long sz = ftell(f);
    rewind(f);
    char *src = malloc(sz + 1);
    fread(src, 1, sz, f);
    src[sz] = '\0';
    fclose(f);

    IcnLexer lx;
    icn_lex_init(&lx, src);
    int errors = 0;
    for (;;) {
        IcnToken t = icn_lex_next(&lx);
        if (t.kind == TK_ERROR) { errors++; printf("    error: %s\n", lx.errmsg); }
        if (t.kind == TK_EOF) break;
    }
    free(src);

    /* Extract short filename for label */
    const char *slash = strrchr(path, '/');
    const char *label = slash ? slash + 1 : path;
    char name[128]; snprintf(name, sizeof(name), "corpus %s", label);
    if (errors == 0) PASS(name);
    else FAIL(name, "%d lex error(s)", errors);
}

static void test_rung1_corpus(void) {
    printf("--- rung1 corpus lex ---\n");
    const char *corpus = "test/frontend/icon/corpus/rung01_paper";
    const char *files[] = {
        "t01_to5.icn", "t02_mult.icn", "t03_nested_to.icn",
        "t04_lt.icn",  "t05_compound.icn", "t06_paper_expr.icn",
        NULL
    };
    for (int i = 0; files[i]; i++) {
        char path[512];
        snprintf(path, sizeof(path), "%s/%s", corpus, files[i]);
        test_corpus_file(path);
    }
}

/* =========================================================================
 * Sequence test — tokenize full expression and verify token stream
 * ======================================================================= */

static void test_sequence(const char *src, IcnTkKind *expected, int n, const char *name) {
    IcnToken toks[64];
    int got = tokenize_all(src, toks, 64);
    /* Last expected is TK_EOF */
    int ok = 1;
    if (got < n) { FAIL(name, "only %d tokens, want %d", got, n); return; }
    for (int i = 0; i < n; i++) {
        if (toks[i].kind != expected[i]) {
            FAIL(name, "tok[%d]: got %s want %s", i,
                 icn_tk_name(toks[i].kind), icn_tk_name(expected[i]));
            ok = 0; break;
        }
    }
    if (ok) PASS(name);
}

static void test_sequences(void) {
    printf("--- multi-token sequences ---\n");

    /* every write(1 to 5); */
    {
        IcnTkKind exp[] = {TK_EVERY, TK_IDENT, TK_LPAREN, TK_INT, TK_TO, TK_INT, TK_RPAREN, TK_SEMICOL, TK_EOF};
        test_sequence("every write(1 to 5);", exp, 9, "every write(1 to 5);");
    }

    /* procedure main(); */
    {
        IcnTkKind exp[] = {TK_PROCEDURE, TK_IDENT, TK_LPAREN, TK_RPAREN, TK_SEMICOL, TK_EOF};
        test_sequence("procedure main();", exp, 6, "procedure main();");
    }

    /* 2 < (1 to 4) */
    {
        IcnTkKind exp[] = {TK_INT, TK_LT, TK_LPAREN, TK_INT, TK_TO, TK_INT, TK_RPAREN, TK_EOF};
        test_sequence("2 < (1 to 4)", exp, 8, "2 < (1 to 4)");
    }

    /* No auto-semicolon: newline does NOT generate TK_SEMICOL */
    {
        IcnToken toks[8];
        int n = tokenize_all("1\n2", toks, 8);
        int found_semi = 0;
        for (int i = 0; i < n; i++)
            if (toks[i].kind == TK_SEMICOL) found_semi = 1;
        if (!found_semi) PASS("no auto-semicolon on newline");
        else FAIL("no auto-semicolon on newline", "found spurious semicolon");
    }
}

/* =========================================================================
 * main
 * ======================================================================= */

int main(void) {
    printf("=== icon_lex_test ===\n");
    test_keywords();
    test_operators();
    test_punctuation();
    test_integer_literals();
    test_real_literals();
    test_string_literals();
    test_cset_literals();
    test_identifiers();
    test_eof();
    test_comments();
    test_sequences();
    test_rung1_corpus();

    printf("\n=== RESULTS: %d/%d PASS ===\n", tests_pass, tests_run);
    return tests_fail > 0 ? 1 : 0;
}
