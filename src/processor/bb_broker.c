#include "bb_broker.h"
extern int Δ;
extern int g_scan_pre_delta;
/*================================================================================================================================================================================*/
int bb_broker(bb_node_t root, BrokerMode mode, void (*body_fn)(DESCR_t val, void * arg), void * arg) {
    univ_box_fn fn;
    int ticks = 0;
    DESCR_t val;
    int scan;
    if (!root.fn) return 0;
    fn = (univ_box_fn)root.fn;
    switch (mode) {
    case bb_scan:
        for (scan = 0; scan <= Ω; scan++) {
            Δ   = scan;
            val = fn(root.ζ, α);
            if (!IS_FAIL_fn(val)) {
                g_scan_pre_delta = scan;
                if (body_fn) body_fn(val, arg);
                ticks++;
                return ticks;
            }
        }
        return 0;
    case bb_pump:
        val = fn(root.ζ, α);
        if (IS_FAIL_fn(val)) return 0;
        if (body_fn) body_fn(val, arg);
        ticks++;
        for (;;) {
            val = fn(root.ζ, β);
            if (IS_FAIL_fn(val)) break;
            if (body_fn) body_fn(val, arg);
            ticks++;
        }
        return ticks;
    case bb_once:
        val = fn(root.ζ, α);
        if (IS_FAIL_fn(val)) return 0;
        if (body_fn) body_fn(val, arg);
        return 1;
    }
    return 0;
}
