/*
 * icon_semicolon.c — Icon auto-semicolon inserter
 *
 * Reads standard Icon source (newline-terminated statements, no explicit
 * semicolons required) and writes semicolon-explicit Icon compatible with
 * our parser (which requires ';' after every statement/declaration).
 *
 * Rule (from the Icon Language Reference Manual §3.1):
 *   Insert ';' after a newline when the token ending the previous line
 *   is one that can end an expression or declaration:
 *
 *     IDENT  INT  REAL  STRING  CSET
 *     )  ]  }
 *     return  suspend  fail  break  next
 *     end
 *
 * Everything else: newline is just whitespace.
 *
 * Additionally: line continuations using trailing _ are NOT inserted.
 * Blank lines and comment-only lines are passed through unchanged.
 *
 * Usage:
 *   icon_semicolon [file.icn]       read file (or stdin), write to stdout
 *   icon_semicolon -o out.icn in.icn
 *
 * Build (standalone):
 *   gcc -O2 -o icon_semicolon src/frontend/icon/icon_semicolon.c
 *
 * Integrated into icon_driver via -semi flag:
 *   icon_driver -semi foo.icn -o foo_semi.icn
 */

#define _POSIX_C_SOURCE 200809L
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>

/* -------------------------------------------------------------------------
 * Minimal token kinds — only what we need for the semicolon decision
 * -------------------------------------------------------------------------*/
typedef enum {
    TK_EOF = 0,
    TK_NEWLINE,

    /* Terminals that CAN end a statement → may trigger semicolon */
    TK_IDENT,           /* identifier or unrecognised keyword */
    TK_INT,
    TK_REAL,
    TK_STRING,
    TK_CSET,
    TK_RPAREN,          /* ) */
    TK_RBRACK,          /* ] */
    TK_RBRACE,          /* } */
    /* keywords that end a statement */
    TK_KW_RETURN,       /* return */
    TK_KW_SUSPEND,      /* suspend */
    TK_KW_FAIL,         /* fail */
    TK_KW_BREAK,        /* break */
    TK_KW_NEXT,         /* next */
    TK_KW_END,          /* end — ends a procedure */

    /* Everything else — never triggers semicolon */
    TK_OTHER,
} SemiTkKind;

/* -------------------------------------------------------------------------
 * Lexer state
 * -------------------------------------------------------------------------*/
typedef struct {
    const char *src;
    size_t      pos;
    size_t      len;
} Lex;

static char lc(const Lex *lx)       { return lx->pos < lx->len ? lx->src[lx->pos] : 0; }
static char la(Lex *lx)             { return lx->pos < lx->len ? lx->src[lx->pos++] : 0; }
static char lp(const Lex *lx, int d){ size_t p = lx->pos+d; return p < lx->len ? lx->src[p] : 0; }

/* Skip to end of line (do NOT consume \n) */
static void skip_comment(Lex *lx) {
    while (lx->pos < lx->len && lc(lx) != '\n') la(lx);
}

/* Scan a quoted literal, return when past closing delimiter.
   We only care about its presence, not its value. */
static void scan_quoted(Lex *lx, char delim) {
    la(lx); /* consume opening delimiter */
    while (lx->pos < lx->len) {
        char c = la(lx);
        if (c == '\\') { la(lx); continue; } /* escape */
        if (c == delim) return;
        if (c == '\n') return; /* unterminated — let parser complain */
    }
}

/* Scan a radix integer like 16rFF or decimal/real.
   Returns TK_INT or TK_REAL. */
static SemiTkKind scan_number(Lex *lx) {
    int is_real = 0;
    /* digits before possible 'r' or '.' */
    while (isdigit((unsigned char)lc(lx))) la(lx);
    /* radix: NNr<digits> */
    if (lc(lx) == 'r' || lc(lx) == 'R') {
        la(lx);
        while (isalnum((unsigned char)lc(lx))) la(lx);
        return TK_INT;
    }
    /* decimal point */
    if (lc(lx) == '.' && isdigit((unsigned char)lp(lx,1))) {
        is_real = 1; la(lx);
        while (isdigit((unsigned char)lc(lx))) la(lx);
    }
    /* exponent */
    if (lc(lx) == 'e' || lc(lx) == 'E') {
        is_real = 1; la(lx);
        if (lc(lx) == '+' || lc(lx) == '-') la(lx);
        while (isdigit((unsigned char)lc(lx))) la(lx);
    }
    return is_real ? TK_REAL : TK_INT;
}

/* Classify identifier/keyword */
static SemiTkKind classify_ident(const char *word) {
    /* Keywords that CAN end a statement — trigger semicolon insertion */
    if (strcmp(word, "return")    == 0) return TK_KW_RETURN;
    if (strcmp(word, "suspend")   == 0) return TK_KW_SUSPEND;
    if (strcmp(word, "fail")      == 0) return TK_KW_FAIL;
    if (strcmp(word, "break")     == 0) return TK_KW_BREAK;
    if (strcmp(word, "next")      == 0) return TK_KW_NEXT;
    if (strcmp(word, "end")       == 0) return TK_OTHER;

    /* Keywords that CANNOT end a statement — do NOT trigger semicolon.
       These are continuation keywords: the statement continues on the next line. */
    if (strcmp(word, "then")      == 0) return TK_OTHER;
    if (strcmp(word, "else")      == 0) return TK_OTHER;
    if (strcmp(word, "do")        == 0) return TK_OTHER;
    if (strcmp(word, "to")        == 0) return TK_OTHER;
    if (strcmp(word, "by")        == 0) return TK_OTHER;
    if (strcmp(word, "not")       == 0) return TK_OTHER;
    if (strcmp(word, "of")        == 0) return TK_OTHER;
    if (strcmp(word, "initial")   == 0) return TK_OTHER;
    if (strcmp(word, "if")        == 0) return TK_OTHER;
    if (strcmp(word, "while")     == 0) return TK_OTHER;
    if (strcmp(word, "until")     == 0) return TK_OTHER;
    if (strcmp(word, "repeat")    == 0) return TK_OTHER;
    if (strcmp(word, "every")     == 0) return TK_OTHER;
    if (strcmp(word, "local")     == 0) return TK_OTHER;
    if (strcmp(word, "static")    == 0) return TK_OTHER;
    if (strcmp(word, "global")    == 0) return TK_OTHER;
    if (strcmp(word, "procedure") == 0) return TK_OTHER;
    if (strcmp(word, "record")    == 0) return TK_OTHER;
    if (strcmp(word, "link")      == 0) return TK_OTHER;
    if (strcmp(word, "invocable") == 0) return TK_OTHER;
    if (strcmp(word, "create")    == 0) return TK_OTHER;
    if (strcmp(word, "case")      == 0) return TK_OTHER;
    if (strcmp(word, "default")   == 0) return TK_OTHER;
    if (strcmp(word, "of")        == 0) return TK_OTHER;

    /* Plain identifier — can end a statement */
    return TK_IDENT;
}

/* Return the next logical token kind for semicolon-insertion purposes.
   Advances lx past the token. */
static SemiTkKind next_semi_tok(Lex *lx) {
    for (;;) {
        if (lx->pos >= lx->len) return TK_EOF;
        char c = lc(lx);

        /* newline — caller handles */
        if (c == '\n') return TK_NEWLINE;

        /* horizontal whitespace */
        if (c == ' ' || c == '\t' || c == '\r') { la(lx); continue; }

        /* comment */
        if (c == '#') { skip_comment(lx); continue; }

        /* string */
        if (c == '"') { scan_quoted(lx, '"'); return TK_STRING; }

        /* cset */
        if (c == '\'') { scan_quoted(lx, '\''); return TK_CSET; }

        /* number */
        if (isdigit((unsigned char)c)) return scan_number(lx);

        /* identifier / keyword */
        if (isalpha((unsigned char)c) || c == '_') {
            char word[128]; int wlen = 0;
            while ((isalnum((unsigned char)lc(lx)) || lc(lx) == '_') && wlen < 127)
                word[wlen++] = la(lx);
            word[wlen] = '\0';
            return classify_ident(word);
        }

        /* punctuation */
        la(lx); /* consume */
        switch (c) {
            case ')': return TK_RPAREN;
            case ']': return TK_RBRACK;
            case '}': return TK_RBRACE;

            /* operators that cannot end a statement — TK_OTHER */
            case '+': case '-': case '*': case '/': case '%':
            case '^': case '<': case '>': case '=': case '~':
            case '|': case ':': case ',': case ';': case '&':
            case '\\': case '!': case '?': case '@': case '.':
            case '(': case '[': case '{':
                /* handle multi-char ops: consume second char if needed */
                /* We don't need the exact token, just that it's TK_OTHER */
                if (c == ':' && lc(lx) == '=') la(lx); /* := or :=: */
                return TK_OTHER;

            default: return TK_OTHER;
        }
    }
}

/* -------------------------------------------------------------------------
 * Semi-insertion decision
 * -------------------------------------------------------------------------*/
static int triggers_semi(SemiTkKind k) {
    switch (k) {
        case TK_IDENT:
        case TK_INT:
        case TK_REAL:
        case TK_STRING:
        case TK_CSET:
        case TK_RPAREN:
        case TK_RBRACK:
        case TK_KW_RETURN:
        case TK_KW_SUSPEND:
        case TK_KW_FAIL:
        case TK_KW_BREAK:
        case TK_KW_NEXT:
            return 1;
        default:
            return 0;
    }
}

/* -------------------------------------------------------------------------
 * Process source: emit with auto-semicolons inserted
 *
 * Strategy: scan line-by-line.  For each line:
 *   1. Find the last non-whitespace, non-comment token on the line.
 *   2. If it triggers_semi(), append ';' before the newline in output.
 *   3. Emit the original line text verbatim (preserving formatting),
 *      then optionally ';', then '\n'.
 * -------------------------------------------------------------------------*/
static void process(const char *src, size_t src_len, FILE *out) {
    size_t pos = 0;

    while (pos <= src_len) {
        /* Find end of line */
        size_t line_start = pos;
        size_t line_end   = pos;
        while (line_end < src_len && src[line_end] != '\n') line_end++;
        /* line_end points at '\n' or end-of-file */

        /* Extract line as NUL-terminated string for scanning */
        size_t line_len = line_end - line_start;
        char *line = (char *)malloc(line_len + 1);
        memcpy(line, src + line_start, line_len);
        line[line_len] = '\0';

        /* Scan the line for the last meaningful token kind */
        Lex lx;
        lx.src = line;
        lx.pos = 0;
        lx.len = line_len;

        SemiTkKind last = TK_OTHER;
        SemiTkKind tok;
        /* Also track last non-whitespace position so we can insert ';'
           right after the last real token (before trailing whitespace/comment) */
        size_t last_tok_end = 0;
        for (;;) {
            size_t tok_start = lx.pos;
            tok = next_semi_tok(&lx);
            if (tok == TK_EOF || tok == TK_NEWLINE) break;
            last = tok;
            last_tok_end = lx.pos; /* position after consuming this token */
        }

        int insert = triggers_semi(last);

        if (insert) {
            /* Emit: everything up to last_tok_end, then ';', then rest of line */
            fwrite(line, 1, last_tok_end, out);
            fputc(';', out);
            /* rest of line (trailing whitespace / comments) */
            if (last_tok_end < line_len)
                fwrite(line + last_tok_end, 1, line_len - last_tok_end, out);
        } else {
            /* Emit line verbatim */
            fwrite(line, 1, line_len, out);
        }

        free(line);

        /* Emit newline (if not EOF) */
        if (line_end < src_len) {
            fputc('\n', out);
            pos = line_end + 1;
        } else {
            break;
        }
    }
}

/* -------------------------------------------------------------------------
 * Read entire file into memory
 * -------------------------------------------------------------------------*/
static char *read_file(const char *path, size_t *out_len) {
    FILE *f = (path && strcmp(path, "-") != 0) ? fopen(path, "r") : stdin;
    if (!f) { perror(path); return NULL; }
    size_t cap = 65536, len = 0;
    char *buf = malloc(cap);
    size_t n;
    while ((n = fread(buf + len, 1, cap - len, f)) > 0) {
        len += n;
        if (len == cap) { cap *= 2; buf = realloc(buf, cap); }
    }
    if (f != stdin) fclose(f);
    buf[len] = '\0';
    *out_len = len;
    return buf;
}

/* -------------------------------------------------------------------------
 * main
 * -------------------------------------------------------------------------*/
int main(int argc, char **argv) {
    const char *inpath  = NULL;
    const char *outpath = NULL;

    for (int i = 1; i < argc; i++) {
        if (strcmp(argv[i], "-o") == 0 && i + 1 < argc) {
            outpath = argv[++i];
        } else if (argv[i][0] != '-') {
            inpath = argv[i];
        } else {
            fprintf(stderr, "usage: icon_semicolon [-o outfile] [infile]\n");
            return 1;
        }
    }

    size_t src_len;
    char *src = read_file(inpath, &src_len);
    if (!src) return 1;

    FILE *out = stdout;
    if (outpath) {
        out = fopen(outpath, "w");
        if (!out) { perror(outpath); free(src); return 1; }
    }

    /* JCON preprocessor pass 1: handle $define and expand macros.
       Also: $< -> [, $> -> ], $( -> {, $) -> } */
    {
        /* First pass: collect all $define NAME value lines into a table,
           blank them out, then do a second pass substituting. */
        #define MAX_MACROS 256
        #define MAX_MACRO_NAME 64
        #define MAX_MACRO_VAL  1024
        static char mname[MAX_MACROS][MAX_MACRO_NAME];
        static char mval [MAX_MACROS][MAX_MACRO_VAL];
        int nmacros = 0;

        /* Scan for $define lines */
        size_t i = 0;
        while (i < src_len) {
            /* skip whitespace at line start */
            size_t ls = i;
            while (i < src_len && (src[i] == ' ' || src[i] == '\t')) i++;
            if (i + 7 < src_len && src[i] == '$') {
                /* check for $define (with optional spaces) */
                size_t j = i + 1;
                while (j < src_len && (src[j] == ' ' || src[j] == '\t')) j++;
                if (j + 6 <= src_len && strncmp(src+j, "define", 6) == 0 &&
                    (src[j+6] == ' ' || src[j+6] == '\t')) {
                    /* found $define */
                    j += 6;
                    while (j < src_len && (src[j] == ' ' || src[j] == '\t')) j++;
                    /* read name */
                    size_t ns = j;
                    while (j < src_len && (isalnum((unsigned char)src[j]) || src[j]=='_')) j++;
                    size_t nlen = j - ns;
                    while (j < src_len && (src[j] == ' ' || src[j] == '\t')) j++;
                    /* read value (to end of line, strip trailing ; and whitespace) */
                    size_t vs = j;
                    while (j < src_len && src[j] != '\n') j++;
                    size_t vlen = j - vs;
                    /* strip trailing whitespace and ; */
                    while (vlen > 0 && (src[vs+vlen-1] == ' ' || src[vs+vlen-1] == '\t' ||
                                        src[vs+vlen-1] == ';' || src[vs+vlen-1] == '\r')) vlen--;
                    if (nlen > 0 && nlen < MAX_MACRO_NAME && vlen < MAX_MACRO_VAL && nmacros < MAX_MACROS) {
                        /* check for duplicate */
                        int dup = 0;
                        for (int m = 0; m < nmacros; m++)
                            if (strncmp(mname[m], src+ns, nlen)==0 && mname[m][nlen]=='\0') { dup=1; break; }
                        if (!dup) {
                            strncpy(mname[nmacros], src+ns, nlen); mname[nmacros][nlen]='\0';
                            strncpy(mval[nmacros], src+vs, vlen);  mval[nmacros][vlen]='\0';
                            nmacros++;
                        }
                    }
                    /* blank out the $define line */
                    for (size_t k = ls; k < j && k < src_len; k++) src[k] = ' ';
                    i = j;
                    continue;
                }
            }
            /* advance to next line */
            while (i < src_len && src[i] != '\n') i++;
            if (i < src_len) i++;
        }

        /* Pass 2: expand macros and handle $< $> $( $) */
        if (nmacros > 0 || 1) {
            /* Build expanded output */
            size_t cap2 = src_len * 2 + 1024;
            char *out2 = malloc(cap2);
            size_t olen = 0;
            #define ENSURE(n) do { if (olen+(n) >= cap2) { cap2=(cap2+(n))*2; out2=realloc(out2,cap2); } } while(0)
            for (size_t k = 0; k < src_len; ) {
                /* $< $> $( $) */
                if (src[k] == '$' && k+1 < src_len) {
                    char nc = src[k+1];
                    if (nc=='<') { ENSURE(1); out2[olen++]='['; k+=2; continue; }
                    if (nc=='>') { ENSURE(1); out2[olen++]=']'; k+=2; continue; }
                    if (nc=='(') { ENSURE(1); out2[olen++]='{'; k+=2; continue; }
                    if (nc==')') { ENSURE(1); out2[olen++]='}'; k+=2; continue; }
                }
                /* Try macro expansion: only at word boundaries */
                if (nmacros > 0 && (isalpha((unsigned char)src[k]) || src[k]=='_')) {
                    int matched = 0;
                    for (int m = 0; m < nmacros; m++) {
                        size_t nl = strlen(mname[m]);
                        if (k+nl <= src_len && strncmp(src+k, mname[m], nl)==0 &&
                            !isalnum((unsigned char)src[k+nl]) && src[k+nl]!='_') {
                            size_t vl = strlen(mval[m]);
                            ENSURE(vl); memcpy(out2+olen, mval[m], vl); olen+=vl;
                            k += nl; matched = 1; break;
                        }
                    }
                    if (matched) continue;
                }
                ENSURE(1); out2[olen++] = src[k++];
            }
            out2[olen] = '\0';
            free(src); src = out2; src_len = olen;
        }
    }

    process(src, src_len, out);

    if (outpath) fclose(out);
    free(src);
    return 0;
}
