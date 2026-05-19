/* sm_ctx.h — per-instruction context passed to context-dependent SM template functions.
   Holds the fields that JUMP/RETURN/HALT need from their enclosing dispatch loops.
   Include this from sm_template_common.h and from each silo that calls sm_control.c fns. */
#pragma once
#include <stdio.h>
typedef struct {
    int            i;             /* current instruction index (PC) */
    int            n;             /* total instruction count */
    int            in_body;       /* JVM: 1 if emitting the function body method, 0 for main */
    const char *   in_my_method;  /* JVM: byte array [n], non-zero if PC i is in current method */
    const int *    pc_to_fn;      /* NET: maps PC→function index (-1 if not in a function) */
    const char **  fn_names;      /* NET/JVM: function name table indexed by fn index */
    int            fn_count;      /* NET/JVM: number of entries in fn_names */
} sm_ctx_t;
