/* ping_test.c — M-SS-MON-1: trivial participant to verify FIFO plumbing
 *
 * Build: gcc -std=c99 -g -O0 ping_test.c mon_hooks.c -o ping_test
 * Usage: MON_EVT=<path> MON_ACK=<path> ./ping_test [name]
 */
#include "mon_hooks.h"
#include <stdio.h>
#include <stdlib.h>

static void ping(void)
{
    MON_ENTER("ping");
    /* do nothing */
    MON_EXIT("ping", "OK");
}

int main(int argc, char *argv[])
{
    const char *name = argc >= 2 ? argv[1] : "anon";
    const char *evt = getenv("MON_EVT");
    const char *ack = getenv("MON_ACK");
    if (!evt || !ack) {
        fprintf(stderr, "[%s] MON_EVT/MON_ACK not set\n", name);
        return 1;
    }
    mon_open(evt, ack);
    ping();
    mon_close();
    return 0;
}
