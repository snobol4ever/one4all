#include "icon_lex.h"
#include "icon_parse.h"
#include "icon_emit.h"
#include "scrip_cc.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
/*----------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------*/
ImportEntry *icn_prescan_imports(const char *src) {
    ImportEntry *head = NULL;
    const char *p = src;
    while (*p) {
        const char *line = p;
        while (*p && *p != '\n') p++;
        const char *lp = line;
        while (*lp == ' ' || *lp == '\t') lp++;
        int is_import = 0;
        if (*lp == '$' && strncmp(lp+1, "import", 6) == 0 &&
            (lp[7] == ' ' || lp[7] == '\t' || lp[7] == '\0' || lp[7] == '\n'))
            { is_import = 1; lp += 7; }
        else if (*lp == '-' && strncmp(lp+1, "IMPORT", 6) == 0 &&
            (lp[7] == ' ' || lp[7] == '\t' || lp[7] == '\0' || lp[7] == '\n'))
            { is_import = 1; lp += 7; }
        if (is_import) {
            while (*lp == ' ' || *lp == '\t') lp++;
            char tok[256]; int ti = 0;
            while (*lp && *lp != ' ' && *lp != '\t' && *lp != '\n' && ti < 255)
                tok[ti++] = *lp++;
            tok[ti] = '\0';
            if (ti > 0) {
                ImportEntry *e = calloc(1, sizeof *e);
                char *dot = strchr(tok, '.');
                if (dot) {
                    int alen = (int)(dot - tok);
                    char asmname[256] = {0};
                    strncpy(asmname, tok, alen < 255 ? alen : 255);
                    e->name   = strdup(asmname);
                    e->method = strdup(dot + 1);
                } else {
                    e->name   = strdup(tok);
                    e->method = strdup(tok);
                }
                e->lang = strdup("ICON");
                e->next = head;
                head = e;
            }
        }
        if (*p == '\n') p++;
    }
    return head;
}
/*----------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------*/
static char *read_file(const char *path) {
    FILE *f = fopen(path, "r");
    if (!f) { perror(path); return NULL; }
    fseek(f, 0, SEEK_END);
    long sz = ftell(f);
    rewind(f);
    char *buf = malloc(sz + 1);
    fread(buf, 1, sz, f);
    buf[sz] = '\0';
    fclose(f);
    return buf;
}
/*----------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------*/
int icn_main(int argc, char **argv) {
    const char *input  = NULL;
    const char *output = NULL;
    int do_run = 0;
    int do_jvm = 0;
    for (int i = 1; i < argc; i++) {
        if (strcmp(argv[i], "-o") == 0 && i+1 < argc) output = argv[++i];
        else if (strcmp(argv[i], "-run") == 0) do_run = 1;
        else if (strcmp(argv[i], "-jvm") == 0) do_jvm = 1;
        else input = argv[i];
    }
    if (!input) { fprintf(stderr, "usage: scrip-cc [-jvm] [-o out.j/.asm] [-run] file.icn\n"); return 1; }
    char *src = read_file(input);
    if (!src) return 1;
    IcnLexer lx;
    icn_lex_init(&lx, src);
    IcnParser parser;
    icn_parse_init(&parser, &lx);
    CODE_t *prog = icn_parse_file(&parser, NULL);
    if (parser.had_error) {
        fprintf(stderr, "parse error: %s\n", parser.errmsg);
        free(src); return 1;
    }
    (void)prog;
    if (do_jvm) {
        fprintf(stderr, "scrip: --jvm emit archived; use --sm-run or --jit-run\n");
        return 1;
    } else {
        fprintf(stderr, "scrip: icn emit archived; use --sm-run or --jit-run\n");
        return 1;
    }
    free(src);
    if (do_run && output) {
        char obj[256], bin[256], cmd[1024];
        snprintf(obj, sizeof obj, "%s.o", output);
        snprintf(bin, sizeof bin, "%s.bin", output);
        snprintf(cmd, sizeof cmd,
            "nasm -f elf64 %s -o %s && "
            "gcc -nostdlib -no-pie -Wl,--no-warn-execstack %s "
            "src/frontend/icon/icon_runtime.c "
            "-o %s",
            output, obj, obj, bin);
        int r = system(cmd);
        if (r != 0) { fprintf(stderr, "assemble/link failed\n"); return 1; }
        return system(bin);
    }
    return 0;
}
