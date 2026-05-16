#include "lower_pl.h"
#include "IR.h"
#include "ast.h"
#include <stddef.h>
/*----------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------*/
/* lower_pl_predicate — placeholder: always returns NULL so caller falls back to legacy SM path.    */
/* Future rungs (PJ-4+) replace this with real IR_block_t construction per clause / choice.         */
IR_block_t *lower_pl_predicate(tree_t *choice) {
    (void)choice;
    return NULL;
}
