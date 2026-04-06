/*
 * sil_arrays.h — Arrays, Tables, and Defined Data Objects (v311.sil §14)
 *
 * Authors: Lon Jones Cherryholmes · Claude Sonnet 4.6
 * Date:    2026-04-06
 * Milestone: M13
 */

#ifndef SIL_ARRAYS_H
#define SIL_ARRAYS_H

#include "sil_types.h"

SIL_result ARRAY_fn(void);    /* ARRAY(P,V)  — create array              */
SIL_result ASSOC_fn(void);    /* TABLE(N,M)  — create table              */
SIL_result DATDEF_fn(void);   /* DATA(P)     — define data type          */
SIL_result PROTO_fn(void);    /* PROTOTYPE(A)— return prototype          */
SIL_result FREEZE_fn(void);   /* FREEZE(T)   — freeze table              */
SIL_result THAW_fn(void);     /* THAW(T)     — thaw table                */
SIL_result ITEM_fn(void);     /* array/table reference                   */
SIL_result DEFDAT_fn(void);   /* create defined data object              */
SIL_result FIELD_fn(void);    /* field accessor procedure                */
SIL_result RSORT_fn(void);    /* RSORT(T,C)  — reverse sort (stub)      */
SIL_result SORT_fn(void);     /* SORT(T,C)   — sort (stub)              */

/* Internal: ASSOCE — initialise a new table extent */
SIL_result ASSOCE_fn(DESCR_t size, DESCR_t ext);

#endif /* SIL_ARRAYS_H */
