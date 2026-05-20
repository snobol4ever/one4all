#include "parser_output.h"
#include "snobol4.h"
#include "polyglot.h"
/* Forward decls: helpers declared in driver/interp.h, but interp.h pulls in DESCR_t which has a non-trivial include footprint.  These are stable, two-arg signatures over const tree_t*. */
extern void label_table_build(const tree_t *prog);
extern void prescan_defines(const tree_t *prog);
/*----------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------*/
/* Build the package that crosses the parser -> lower boundary.  The contract this function names is: after this returns, lower(po) has everything it needs.                                            */
/* Single source of truth for the pre-lower setup sequence.  Side-effects on globals (label_table, proc_table, g_pl_pred_table) are inherited from the helpers called here; Design A will fold them in. */
ParserOutput parser_output_build(const tree_t *prog) {
    ParserOutput po = { .prog = prog, .lang_mask = 0 };
    if (!prog) return po;
    label_table_build(prog);
    prescan_defines(prog);
    po.lang_mask = polyglot_lang_mask(prog);
    polyglot_init(prog, po.lang_mask);
    return po;
}
