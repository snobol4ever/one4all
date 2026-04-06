/*
 * sil_asgn.h — Assignment and core operations (v311.sil §17 lines 5828–6101)
 *
 * Faithful C translation of:
 *   ASGN   — X = Y  (assignment)
 *   CONCAT — X Y    (concatenation)
 *   IND    — $X     (indirect reference)
 *   KEYWRD — &X     (keyword reference)
 *   LIT    — 'X'    (literal push)
 *   NAME   — .X     (unary name operator)
 *   STR    — *X     (unevaluated expression)
 *
 * Authors: Lon Jones Cherryholmes · Claude Sonnet 4.6
 * Date:    2026-04-06
 * Milestone: M10
 */

#ifndef SIL_ASGN_H
#define SIL_ASGN_H

#include "sil_types.h"

SIL_result ASGN_fn(void);    /* X = Y  — assignment                       */
SIL_result CONCAT_fn(void);  /* X Y    — concatenation                    */
SIL_result IND_fn(void);     /* $X     — indirect reference               */
SIL_result KEYWRD_fn(void);  /* &X     — keyword reference                */
SIL_result LIT_fn(void);     /* 'X'    — literal push                     */
SIL_result NAME_fn(void);    /* .X     — unary name                       */
SIL_result STR_fn(void);     /* *X     — unevaluated expression           */

#endif /* SIL_ASGN_H */
