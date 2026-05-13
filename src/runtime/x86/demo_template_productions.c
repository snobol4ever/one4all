/*
 * demo_template_productions.c — visual demonstration.
 *
 * Drives `emit_sm_halt` through ALL three backends and prints the
 * outputs side-by-side, for review by Lon when resolving the open
 * architectural question (see KNOWN OPEN ARCHITECTURAL QUESTION in
 * templates/sm_halt.c).  Not a test (no PASS/FAIL).
 *
 * Output:
 *   [binary]  hex bytes
 *   [text-invocation]  what mode-4's per-call site WOULD emit if it
 *                      were wired through this template
 *   [text-definition]  what sm_macros.s WOULD contain if it were
 *                      regenerated from this template
 *   [macro_def] same as text-definition (it's a thin wrapper)
 *
 * Authors: Lon Jones Cherryholmes · Claude Opus 4.7
 * Sprint:  EM-MODE4-IS-MODE3-DUMP-c / GOAL-MODE4-EMIT
 */

#include "emitter.h"
#include "templates.h"
#include "emit_bb_gen.h"
#include <stdio.h>
#include <stdlib.h>

int main(void)
{
    /* (1) Binary backend — emit into a buffer, print hex */
    printf("=== emit_sm_halt — BINARY backend ===\n");
    {
        unsigned char buf[16];
        bb_buf_t cap = buf;
        emitter_init_binary(cap, sizeof(buf));
        emit_sm_halt();
        int n = emitter_end();
        
        printf("  bytes (%d total):", n);
        for (int i = 0; i < n; i++) printf(" %02x", buf[i]);
        printf("\n  meaning: inc dword [r13+20] ; ret\n");
    }

    /* (2) Text-INVOCATION backend — emit into stdout */
    printf("\n=== emit_sm_halt — TEXT_INVOCATION backend ===\n");
    {
        emitter_init_text(stdout, TEXT_MODE_INVOCATION);
        emit_sm_halt();
        
    }

    /* (3) Text-DEFINITION backend — emit into stdout */
    printf("\n=== emit_sm_halt — TEXT_DEFINITION (== MACRO_DEF) backend ===\n");
    {
        emitter_init_macro_def(stdout);
        emit_sm_halt();
        
    }

    printf("\n");
    return 0;
}
