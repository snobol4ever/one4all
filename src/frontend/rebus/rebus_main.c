#include "rebus.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
/*----------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------*/
void rebus_emit(tree_t *prog, FILE *out);
/*----------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------*/
int main(int argc, char *argv[]) {
    const char *outfile = NULL;
    const char *infile  = NULL;
    int do_print = 0;
    int do_emit  = 0;
    for (int i = 1; i < argc; i++) {
        if (!strcmp(argv[i], "-p")) {
            do_print = 1;
        } else if (!strcmp(argv[i], "-e")) {
            do_emit = 1;
        } else if (!strcmp(argv[i], "-o") && i + 1 < argc) {
            outfile = argv[++i];
        } else if (argv[i][0] != '-') {
            infile = argv[i];
        } else {
            fprintf(stderr, "rebus: unknown option %s\n", argv[i]);
            return 1;
        }
    }
    FILE *in = stdin;
    if (infile) {
        in = fopen(infile, "r");
        if (!in) { perror(infile); return 1; }
    }
    FILE *out = stdout;
    if (outfile) {
        out = fopen(outfile, "w");
        if (!out) { perror(outfile); return 1; }
    }
    tree_t *prog = rebus_parse(in, infile ? infile : "<stdin>");
    if (rebus_nerrors) {
        fprintf(stderr, "rebus: %d parse error(s)\n", rebus_nerrors);
        return 1;
    }
    if (!do_print && !do_emit) do_emit = 1;
    if (do_print)
        rebus_print(prog, out);
    if (do_emit)
        rebus_emit(prog, out);
    if (infile)  fclose(in);
    if (outfile) fclose(out);
    return 0;
}
