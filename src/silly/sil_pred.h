/*
 * sil_pred.h — Predicate functions (v311.sil §18 lines 6102–6321)
 *
 * Faithful C translation of:
 *   DIFFER FUNCTN IDENT LABEL LABELC
 *   LEQ LGE LGT LLE LLT LNE
 *   NEG QUES
 *   CHAR LPAD RPAD
 *
 * Authors: Lon Jones Cherryholmes · Claude Sonnet 4.6
 * Date:    2026-04-06
 * Milestone: M11
 */

#ifndef SIL_PRED_H
#define SIL_PRED_H

#include "sil_types.h"

Sil_result DIFFER_fn(void);
Sil_result FUNCTN_fn(void);
Sil_result IDENT_fn(void);
Sil_result LABEL_fn(void);
Sil_result LABELC_fn(void);
Sil_result LEQ_fn(void);
Sil_result LGE_fn(void);
Sil_result LGT_fn(void);
Sil_result LLE_fn(void);
Sil_result LLT_fn(void);
Sil_result LNE_fn(void);
Sil_result NEG_fn(void);
Sil_result QUES_fn(void);
Sil_result CHAR_fn(void);
Sil_result LPAD_fn(void);
Sil_result RPAD_fn(void);

#endif /* SIL_PRED_H */
