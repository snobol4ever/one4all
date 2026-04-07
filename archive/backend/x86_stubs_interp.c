/* x86_stubs_interp.c — satisfy asm-side extern references
 * for non-x86 builds (scrip-interp). Never called by interpreter path. */
#include <stdint.h>
uint64_t cursor          = 0;
uint64_t subject_len_val = 0;
char     subject_data[65536] = {0};

/* stmt_init — asm-backend entry point not used by scrip-interp */
void stmt_init(void) {}
