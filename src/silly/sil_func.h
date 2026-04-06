/*
 * sil_func.h — Other functions (v311.sil §19 lines 6322–7037)
 *
 * Faithful C translation of §19 procedures.
 * Complex procs (OPSYN, CONVERT, DMP/DUMP, ARG/LOCAL/FIELDS, CNVTA/CNVAT)
 * are stubbed pending deeper infrastructure.
 *
 * Authors: Lon Jones Cherryholmes · Claude Sonnet 4.6
 * Date:    2026-04-06
 * Milestone: M12
 */

#ifndef SIL_FUNC_H
#define SIL_FUNC_H

#include "sil_types.h"

Sil_result APPLY_fn(void);
Sil_result ARG_fn(void);
Sil_result LOCAL_fn(void);
Sil_result FIELDS_fn(void);
Sil_result CLEAR_fn(void);
Sil_result CMA_fn(void);
Sil_result COLECT_fn(void);
Sil_result COPY_fn(void);
Sil_result CNVRT_fn(void);
Sil_result CODER_fn(void);
Sil_result DATE_fn(void);
Sil_result DT_fn(void);
Sil_result DMP_fn(void);
Sil_result DUMP_fn(void);
Sil_result DUPL_fn(void);
Sil_result OPSYN_fn(void);
Sil_result RPLACE_fn(void);
Sil_result REVERS_fn(void);
Sil_result SIZE_fn(void);
Sil_result SUBSTR_fn(void);
Sil_result TIME_fn(void);
Sil_result TRIM_fn(void);
Sil_result VDIFFR_fn(void);

#endif /* SIL_FUNC_H */
