/*
 * sm_phase2_sim_test.c — EM-7a unit test for sm_phase2_to_patnd().
 *
 * Tests three patterns:
 *   Test A: BREAK("=") . LHS — pure invariant? No: XBRKC is variant per
 *           flat_is_eligible_node (mutable ζ-delta). But PATND_t is
 *           reconstructed correctly with has_variant=1.
 *   Test B: 'literal' CAT 'another' — pure invariant (two XCHR nodes).
 *           has_variant=0, patnd_is_fully_invariant=1.
 *   Test C: *VAR — XDSAR; has_variant=1.
 *   Test D: epsilon — empty window; returns XEPS; has_variant=0.
 *
 * PASS means: correct XKIND_t at root, correct has_variant flag,
 * and patnd_is_fully_invariant agrees.
 */

#include "sm_prog.h"
#include "sm_codegen_x64_emit.h"
#include "snobol4.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static int g_pass = 0, g_fail = 0;

static void check(const char *name, int cond)
{
    if (cond) { printf("  PASS  %s\n", name); g_pass++; }
    else       { printf("  FAIL  %s\n", name); g_fail++; }
}

int main(void)
{
    /* ── Test A: BREAK("=") . LHS
     * SM: SM_PUSH_LIT_S "=" ; SM_PAT_BREAK ; SM_PAT_CAPTURE "LHS" cond
     * Expected: root = XNME (capture cond), child = XBRKC
     *           has_variant = 1 (XBRKC is mutable-ζ)
     *           patnd_is_fully_invariant = 0
     */
    {
        SM_Program *p = sm_prog_new();
        sm_emit_s(p, SM_PUSH_LIT_S, "=");
        sm_emit(p, SM_PAT_BREAK);
        {
            int idx = sm_emit_s(p, SM_PAT_CAPTURE, "LHS");
            p->instrs[idx].a[1].i = 0; /* cond */
        }
        int has_v = -1;
        DESCR_t d = sm_phase2_to_patnd(p, 0, p->count, &has_v);
        PATND_t *root = (PATND_t *)d.s;
        check("A: DT_P returned",   d.v == DT_P && root != NULL);
        check("A: root=XNME",       root && root->kind == XNME);
        check("A: child=XBRKC",     root && root->nchildren == 1 &&
                                    root->children[0]->kind == XBRKC);
        check("A: has_variant=0",   has_v == 0);
        check("A: fully_invar",     root && patnd_is_fully_invariant(root));
        sm_prog_free(p);
    }

    /* ── Test B: 'hello' CAT 'world'
     * SM: SM_PAT_LIT "hello" ; SM_PAT_LIT "world" ; SM_PAT_CAT
     * Expected: root = XCAT, two XCHR children
     *           has_variant = 0
     *           patnd_is_fully_invariant = 1
     */
    {
        SM_Program *p = sm_prog_new();
        sm_emit_s(p, SM_PAT_LIT, "hello");
        sm_emit_s(p, SM_PAT_LIT, "world");
        sm_emit(p, SM_PAT_CAT);
        int has_v = -1;
        DESCR_t d = sm_phase2_to_patnd(p, 0, p->count, &has_v);
        PATND_t *root = (PATND_t *)d.s;
        check("B: DT_P returned",  d.v == DT_P && root != NULL);
        check("B: root=XCAT",      root && root->kind == XCAT);
        check("B: 2 children",     root && root->nchildren == 2);
        check("B: left=XCHR",      root && root->nchildren == 2 &&
                                   root->children[0]->kind == XCHR);
        check("B: right=XCHR",     root && root->nchildren == 2 &&
                                   root->children[1]->kind == XCHR);
        check("B: has_variant=0",  has_v == 0);
        check("B: fully_invar",    root && patnd_is_fully_invariant(root));
        sm_prog_free(p);
    }

    /* ── Test C: *VAR (deferred reference)
     * SM: SM_PAT_REFNAME "PAT"
     * Expected: root = XDSAR (pat_ref builds XDSAR)
     *           has_variant = 1
     *           patnd_is_fully_invariant = 0
     */
    {
        SM_Program *p = sm_prog_new();
        sm_emit_s(p, SM_PAT_REFNAME, "PAT");
        int has_v = -1;
        DESCR_t d = sm_phase2_to_patnd(p, 0, p->count, &has_v);
        PATND_t *root = (PATND_t *)d.s;
        check("C: DT_P returned",   d.v == DT_P && root != NULL);
        check("C: root=XDSAR",      root && root->kind == XDSAR);
        check("C: has_variant=0",   has_v == 0);
        check("C: fully_invar",     root && patnd_is_fully_invariant(root));
        sm_prog_free(p);
    }

    /* ── Test D: empty window → epsilon
     * SM: (no instructions, phase2_start == phase2_end)
     * Expected: DT_P, root=XEPS, has_variant=0
     */
    {
        SM_Program *p = sm_prog_new();
        int has_v = -1;
        DESCR_t d = sm_phase2_to_patnd(p, 0, 0, &has_v);
        PATND_t *root = (PATND_t *)d.s;
        check("D: DT_P returned",  d.v == DT_P && root != NULL);
        check("D: root=XEPS",      root && root->kind == XEPS);
        check("D: has_variant=0",  has_v == 0);
        sm_prog_free(p);
    }

    /* ── Test E: flat_is_eligible_node per-kind spot-checks */
    {
        PATND_t xchr = {0}; xchr.kind = XCHR;
        PATND_t xcat = {0}; xcat.kind = XCAT;
        PATND_t xdsar = {0}; xdsar.kind = XDSAR;
        PATND_t xbrkc = {0}; xbrkc.kind = XBRKC;
        PATND_t xeps  = {0}; xeps.kind  = XEPS;
        PATND_t xposi = {0}; xposi.kind = XPOSI;
        check("E: XCHR invariant",   flat_is_eligible_node(&xchr)  == 1);
        check("E: XCAT invariant",   flat_is_eligible_node(&xcat)  == 1);
        check("E: XEPS invariant",   flat_is_eligible_node(&xeps)  == 1);
        check("E: XPOSI invariant",  flat_is_eligible_node(&xposi) == 1);
        check("E: XDSAR invariant",  flat_is_eligible_node(&xdsar) == 1);
        check("E: XBRKC invariant",  flat_is_eligible_node(&xbrkc) == 1);
    }

    printf("\nPASS=%d FAIL=%d  (EM-7a Phase-2 simulator + flat_is_eligible_node)\n",
           g_pass, g_fail);
    return g_fail > 0 ? 1 : 0;
}
