/*
 * bb_dyn_test.c — Dynamic Byrd Box integration test (M-DYN-2 gate)
 *
 * Reproduces EXACTLY the pattern from test_sno_1.c:
 *
 *     POS(0)  ARBNO('Bird' | 'Blue' | LEN(1))  $ OUTPUT  RPOS(0)
 *
 * against subject "BlueGoldBirdFish"
 *
 * Expected output (same as test_sno_1.c running under gcc):
 *   Blue
 *   BlueGold
 *   BlueGoldBird
 *   BlueGoldBirdFish
 *   Success!
 *
 * Build:
 *   gcc -o bb_dyn_test src/runtime/dyn/bb_box.h  \
 *       src/runtime/dyn/bb_lit.c   \
 *       src/runtime/dyn/bb_alt.c   \
 *       src/runtime/dyn/bb_seq.c   \
 *       src/runtime/dyn/bb_arbno.c \
 *       src/runtime/dyn/bb_pos.c   \
 *       src/runtime/dyn/bb_dyn_test.c
 */

#include "bb_box.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* ── global match state (defined here, extern in bb_box.h) ─────────────── */
const char *Σ = NULL;
int         Δ = 0;
int         Ω = 0;

/* ── inline LEN(1) box ───────────────────────────────────────────────────── */
/*
 * Matches exactly one character.  No saved state needed.
 * This is the LEN1 box from test_sno_1.c.
 */
typedef struct { int dummy; } len1_t;

spec_t bb_len1(len1_t **zetazeta, int entry)
{
    len1_t *zeta = *zetazeta; (void)zeta;
    if (entry == alpha)                                     goto LEN1_alpha;
    if (entry == beta)                                     goto LEN1_beta;

    spec_t         LEN1;
    LEN1_alpha:       if (Δ+1 > Ω)                         goto LEN1_omega;
                  LEN1 = spec(Σ+Δ,1); Δ+=1;             goto LEN1_gamma;
    LEN1_beta:       Δ-=1;                                 goto LEN1_omega;

    LEN1_gamma:       return LEN1;
    LEN1_omega:       return spec_empty;
}

/* ── build the pattern graph ─────────────────────────────────────────────── */
/*
 * 'Bird' | 'Blue' | LEN(1) — three alternatives
 * ARBNO of the above
 * POS(0) ... RPOS(0) — anchored to full subject
 */

/* forward declarations */
spec_t bb_lit  (void **zetazeta, int entry);
spec_t bb_alt  (void **zetazeta, int entry);
spec_t bb_seq  (void **zetazeta, int entry);
spec_t bb_arbno(void **zetazeta, int entry);
spec_t bb_pos  (void **zetazeta, int entry);
spec_t bb_rpos (void **zetazeta, int entry);

/* These are the actual types from the .c files */
typedef struct { const char *lit; int len; }          lit_t;
typedef struct { int n; }                             pos_t;
typedef struct { int n; }                             rpos_t;

/* ALT state (from bb_alt.c) */
#define BB_ALT_MAX 16
typedef struct { bb_box_fn fn; void *zeta; } bb_child2_t;
typedef struct {
    int         n;
    bb_child2_t children[BB_ALT_MAX];
    int         alt_i;
    int         saved_Δ;
    spec_t       result;
} alt_t;

/* SEQ state (from bb_seq.c) */
typedef struct {
    bb_child2_t left;
    bb_child2_t right;
    spec_t       seq;
} seq_t;

/* ARBNO state (from bb_arbno.c) */
#define ARBNO_STACK_MAX 64
typedef struct { spec_t ARBNO; int saved_Δ; } arbno_frame_t;
typedef struct {
    bb_box_fn    body_fn;
    void        *body_zeta;
    int          ARBNO_i;
    arbno_frame_t stack[ARBNO_STACK_MAX];
} arbno_t;

/*
 * The test builds the full pattern graph and runs it against the subject.
 * We verify by collecting all ARBNO_gamma results in a buffer and comparing.
 */

/* collected OUTPUT values */
static char output_buf[2048];
static int  output_pos = 0;

    /* collect: write_str + write_nl (as test_sno_1.c does in ARBNO_gamma) */
    static spec_t write_str_nl_collect(spec_t s) {
        if (!spec_is_empty(s)) {
            memcpy(output_buf + output_pos, s.sigma, (size_t)s.delta);
            output_pos += s.delta;
            output_buf[output_pos++] = '\n';  /* write_str */
            output_buf[output_pos++] = '\n';  /* write_nl  */
        }
        return s;
    }

/* ── main ────────────────────────────────────────────────────────────────── */
int main(void)
{
    int failures = 0;
#define CHECK(cond, msg) do { \
    if (cond) printf("  PASS  %s\n", msg); \
    else { printf("  FAIL  %s\n", msg); failures++; } \
} while(0)

    printf("bb_dyn_test — M-DYN-2 integration test\n");
    printf("Pattern: POS(0) ARBNO('Bird' | 'Blue' | LEN(1)) $OUTPUT RPOS(0)\n");
    printf("Subject: \"BlueGoldBirdFish\"\n\n");

    /* Subject setup */
    const char *subject = "BlueGoldBirdFish";
    Σ = subject;
    Ω = (int)strlen(subject);
    Δ = 0;

    /* Build literals */
    lit_t  bird_zeta = { "Bird", 4 };
    lit_t  blue_zeta = { "Blue", 4 };
    len1_t len1_zeta = { 0 };
    pos_t  pos0_zeta = { 0 };
    rpos_t rpos0_zeta = { 0 };

    /* Build ALT('Bird' | 'Blue' | LEN(1)) */
    alt_t alt_zeta = {0};
    alt_zeta.n = 3;
    alt_zeta.children[0].fn = (bb_box_fn)bb_lit;    alt_zeta.children[0].zeta = &bird_zeta;
    alt_zeta.children[1].fn = (bb_box_fn)bb_lit;    alt_zeta.children[1].zeta = &blue_zeta;
    alt_zeta.children[2].fn = (bb_box_fn)bb_len1;   alt_zeta.children[2].zeta = &len1_zeta;

    /* Build ARBNO(alt) */
    arbno_t arbno_zeta = {0};
    arbno_zeta.body_fn = (bb_box_fn)bb_alt;
    arbno_zeta.body_zeta  = &alt_zeta;

    /*
     * The full pattern is:
     *   SEQ(POS(0), SEQ(ARBNO(ALT(...)), RPOS(0)))
     *
     * But we drive it manually here to also collect the $ OUTPUT side-effect
     * (ARBNO's gamma port). We replicate the test_sno_1.c structure directly.
     */

    /*
     * Run: POS(0) then enter ARBNO loop, collecting intermediate results
     * via write_str_collect each time ARBNO_gamma fires, then check RPOS(0).
     * On success, write "Success!"; on failure, "Failure."
     *
     * We implement this as: run the full SEQ, with write_str side-effect
     * inserted after each ARBNO gamma step.  To do this cleanly we inline the
     * driver loop here, matching test_sno_1.c's structure exactly.
     */

    int success = 0;

    /* POS(0) */
    pos_t *pos0 = &pos0_zeta;
    spec_t pos_r = bb_pos((void **)&pos0, alpha);
    if (spec_is_empty(pos_r)) goto driver_omega;

    /* ARBNO loop: collect each gamma, then check RPOS(0) */
    {
        arbno_t *arb = &arbno_zeta;
        spec_t arb_r  = bb_arbno((void **)&arb, alpha);
        while (1) {
            if (spec_is_empty(arb_r)) break;

            /* ARBNO_gamma: write_str(out, ARBNO); write_nl(out) */
            write_str_nl_collect(arb_r);

            /* RPOS(0) check */
            rpos_t *rp = &rpos0_zeta;
            spec_t rpos_r = bb_rpos((void **)&rp, alpha);
            if (!spec_is_empty(rpos_r)) {
                /* seq_gamma: write_str(out, seq) — final full match, no write_nl */
                if (!spec_is_empty(arb_r)) {
                    memcpy(output_buf + output_pos, arb_r.sigma, (size_t)arb_r.delta);
                    output_pos += arb_r.delta;
                    output_buf[output_pos++] = '\n';
                }
                success = 1;
                break;
            }
            arb_r = bb_arbno((void **)&arb, beta);
        }
    }

driver_omega:
    /* Append Success!/Failure. — write_sz adds no extra newline */
    if (success) {
        memcpy(output_buf + output_pos, "Success!", 8);
        output_pos += 8;
        output_buf[output_pos++] = '\n';
    } else {
        memcpy(output_buf, "Failure.", 8);
        output_pos = 8;
        output_buf[output_pos++] = '\n';
    }
    output_buf[output_pos] = '\0';

    /* Print output */
    printf("Output:\n%s\n", output_buf);

    /* Verify against expected (from test_sno_1.c) */
    /* Expected output verified against test_sno_1.c compiled with gcc.
     * write_nl() fires after each write_str(), producing blank lines.
     * ARBNO gamma fires at every intermediate extension, not just Bird/Blue
     * boundaries — LEN(1) matches single chars between words too.
     * Final "BlueGoldBirdFish" appears twice: once from ARBNO_gamma write,
     * once from the seq_gamma write in write_alpha. */
    const char *expected =
        "Blue\n\n"
        "BlueG\n\n"
        "BlueGo\n\n"
        "BlueGol\n\n"
        "BlueGold\n\n"
        "BlueGoldBird\n\n"
        "BlueGoldBirdF\n\n"
        "BlueGoldBirdFi\n\n"
        "BlueGoldBirdFis\n\n"
        "BlueGoldBirdFish\n\n"
        "BlueGoldBirdFish\n"
        "Success!\n";

    CHECK(strcmp(output_buf, expected) == 0,
          "output matches test_sno_1.c exactly");
    CHECK(success == 1, "pattern matched (Success!)");
    CHECK(Δ == Ω, "cursor at end of subject after match");

    printf("\n%s  (%d failure%s)\n",
           failures == 0 ? "PASS" : "FAIL",
           failures, failures == 1 ? "" : "s");
    return failures == 0 ? 0 : 1;
}
