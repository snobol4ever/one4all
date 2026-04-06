/*
 * sil_io.h — I/O functions (v311.sil §15 lines 5268–5465)
 *
 * READ   — INPUT(V,U,O,N)
 * PRINT  — OUTPUT(V,U,O,N)
 * BKSPCE — BACKSPACE(U)
 * ENDFL  — ENDFILE(U)
 * REWIND — REWIND(U)
 * SET    — SET(U,O,W)
 * DETACH — DETACH(V)
 * PUTIN  — internal input procedure
 * PUTOUT — internal output procedure
 *
 * Authors: Lon Jones Cherryholmes · Claude Sonnet 4.6
 * Date:    2026-04-06
 * Milestone: M15
 */

#ifndef SIL_IO_H
#define SIL_IO_H

#include "sil_types.h"

Sil_result READ_fn(void);     /* INPUT(V,U,O,N)   */
Sil_result PRINT_fn(void);    /* OUTPUT(V,U,O,N)  */
Sil_result BKSPCE_fn(void);   /* BACKSPACE(U)     */
Sil_result ENDFL_fn(void);    /* ENDFILE(U)       */
Sil_result REWIND_fn(void);   /* REWIND(U)        */
Sil_result SET_fn(void);      /* SET(U,O,W)       */
Sil_result DETACH_fn(void);   /* DETACH(V)        */
Sil_result PUTIN_fn(DESCR_t blk, DESCR_t var);   /* internal input  */
void       PUTOUT_fn(DESCR_t blk, DESCR_t val);  /* internal output */

#endif /* SIL_IO_H */
