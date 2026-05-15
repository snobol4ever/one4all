#include "emit.h"
#include "emit_templates.h"
#include "emit.h"
#include <stdio.h>
#include <stdlib.h>
/*----------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------*/
int main(void)
{
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
    printf("\n=== emit_sm_halt — TEXT_INVOCATION backend ===\n");
    {
        emitter_init_text(stdout, TEXT_MODE_INVOCATION);
        emit_sm_halt();
    }
    printf("\n=== emit_sm_halt — TEXT_DEFINITION (== MACRO_DEF) backend ===\n");
    {
        emitter_init_macro_def(stdout);
        emit_sm_halt();
    }
    printf("\n");
    return 0;
}
