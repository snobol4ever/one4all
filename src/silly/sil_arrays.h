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

Sil_result ARRAY_fn(void);    /* ARRAY(P,V)  — create array              */
Sil_result ASSOC_fn(void);    /* TABLE(N,M)  — create table              */
Sil_result DATDEF_fn(void);   /* DATA(P)     — define data type          */
Sil_result PROTO_fn(void);    /* PROTOTYPE(A)— return prototype          */
Sil_result FREEZE_fn(void);   /* FREEZE(T)   — freeze table              */
Sil_result THAW_fn(void);     /* THAW(T)     — thaw table                */
Sil_result ITEM_fn(void);     /* array/table reference                   */
Sil_result DEFDAT_fn(void);   /* create defined data object              */
Sil_result FIELD_fn(void);    /* field accessor procedure                */
Sil_result RSORT_fn(void);    /* RSORT(T,C)  — reverse sort (stub)      */
Sil_result SORT_fn(void);     /* SORT(T,C)   — sort (stub)              */

/* Internal: ASSOCE — initialise a new table extent */
Sil_result ASSOCE_fn(DESCR_t size, DESCR_t ext);

#endif /* SIL_ARRAYS_H */
