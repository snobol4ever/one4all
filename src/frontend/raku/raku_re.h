/*============================================================= raku_re.h ====
 * Raku regex engine — table-driven NFA (Thompson construction)
 *
 * AUTHORS: Lon Jones Cherryholmes · Claude Sonnet 4.6
 *==========================================================================*/
#ifndef RAKU_RE_H
#define RAKU_RE_H

/*------------------------------------------------------------------------
 * Character class bitmap (256 bits)
 *----------------------------------------------------------------------*/
typedef struct { unsigned char bits[32]; } Raku_cc;

int raku_cc_test(const Raku_cc *cc, unsigned char c);

/*------------------------------------------------------------------------
 * NFA state kinds
 *----------------------------------------------------------------------*/
typedef enum {
    NK_EPS,         /* epsilon — no input consumed, follow out1 */
    NK_SPLIT,       /* fork — follow out1 AND out2 (alternation/quantifier) */
    NK_CHAR,        /* match literal character ch */
    NK_ANY,         /* match any char except newline */
    NK_CLASS,       /* match char class bitmap */
    NK_ANCHOR_BOL,  /* ^ zero-width assertion */
    NK_ANCHOR_EOL,  /* $ zero-width assertion */
    NK_CAP_OPEN,    /* ( — zero-width: record group start position */
    NK_CAP_CLOSE,   /* ) — zero-width: record group end position */
    NK_ACCEPT       /* accepting state */
} Nfa_kind;

#define NFA_NULL   (-1)
#define MAX_GROUPS  16   /* max capture groups per pattern */

/*------------------------------------------------------------------------
 * NFA state — element of the flat state table
 *
 * BB lifter note (Phase 3): each Nfa_state maps 1-to-1 to a BB box.
 *   NK_SPLIT     → E_ALTERNATE (two successor boxes)
 *   NK_EPS       → epsilon edge (no BB yield, just follow out1)
 *   NK_ACCEPT    → BB_PUMP return with match position
 *   NK_CHAR/ANY/CLASS → consume one char, advance pos or FAILDESCR
 *   NK_ANCHOR_*  → zero-width test, FAILDESCR if assertion fails
 *   NK_CAP_OPEN/CLOSE → zero-width, record position in capture array
 * The bb_id field is reserved for the Phase-3 lifter.
 *----------------------------------------------------------------------*/
typedef struct {
    int       id;        /* index in state table */
    Nfa_kind  kind;
    unsigned char ch;    /* NK_CHAR: literal character */
    Raku_cc   cc;        /* NK_CLASS: character class bitmap */
    int       out1;      /* primary successor  (NFA_NULL = none) */
    int       out2;      /* secondary successor (NK_SPLIT only) */
    int       cap_idx;   /* NK_CAP_OPEN/CLOSE: group index (0-based) */
    int       bb_id;     /* reserved: BB box id for Phase-3 lifter */
} Nfa_state;

/*------------------------------------------------------------------------
 * Match result — populated by raku_nfa_exec
 *----------------------------------------------------------------------*/
typedef struct {
    int matched;                    /* 1 = match found, 0 = no match */
    int full_start;                 /* byte offset of full match start */
    int full_end;                   /* byte offset past full match end */
    int group_start[MAX_GROUPS];    /* capture group start offsets (-1 = unset) */
    int group_end[MAX_GROUPS];      /* capture group end offsets   (-1 = unset) */
    int ngroups;                    /* number of groups in the pattern */
} Raku_match;

/*------------------------------------------------------------------------
 * NFA graph (opaque handle)
 *----------------------------------------------------------------------*/
typedef struct Raku_nfa Raku_nfa;

/* Compile a pattern string to an NFA state table.
 * Returns NULL on syntax error (message printed to stderr).
 * Caller owns the result; free with raku_nfa_free(). */
Raku_nfa *raku_nfa_build(const char *pattern);

/* Number of states in the compiled NFA. */
int raku_nfa_state_count(const Raku_nfa *nfa);

/* Number of capture groups in the compiled NFA. */
int raku_nfa_ngroups(const Raku_nfa *nfa);

/* Table-driven simulation (no captures): returns 1 on match, 0 on no match.
 * Unanchored unless pattern starts with ^. */
int raku_nfa_match(const Raku_nfa *nfa, const char *subject);

/* Full execution with capture tracking.
 * Fills *result. result->matched == 1 on success.
 * group_start[i]/group_end[i] give byte offsets into subject. */
void raku_nfa_exec(const Raku_nfa *nfa, const char *subject, Raku_match *result);

/* Release NFA memory. */
void raku_nfa_free(Raku_nfa *nfa);

/* Expose raw state array for BB lifter (Phase 3). */
Nfa_state *raku_nfa_states(Raku_nfa *nfa);

#endif /* RAKU_RE_H */
