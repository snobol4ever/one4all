/*
 * prolog_emit_net.c — Prolog IR → .NET CIL emitter (stub)
 *
 * M-LINK-NET-4: cross-assembly call proof-of-concept uses a hand-authored
 * ancestor.il rather than a generated one.  This stub exists so the driver
 * links; the full emitter is a future sprint (M-LINK-NET-5).
 *
 * Entry point: prolog_emit_net(Program *prog, FILE *out, const char *filename)
 * Called from driver/main.c when -pl -net flags are set.
 */

#include "sno2c.h"
#include <stdio.h>
#include <stdlib.h>

void prolog_emit_net(Program *prog, FILE *out, const char *filename) {
    (void)prog; (void)out;
    fprintf(stderr,
        "sno2c: -pl -net: Prolog CIL emitter not yet implemented.\n"
        "  For M-LINK-NET-4, use the hand-authored test/linker/net/ancestor/ancestor.il.\n"
        "  Full emitter: M-LINK-NET-5.\n");
    exit(1);
}
