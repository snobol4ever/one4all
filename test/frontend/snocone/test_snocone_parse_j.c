/*
 * test_snocone_parse_j.c — GOAL-SNOCONE-LANG-SPACE LS-4.l acceptance test
 *
 * Two LS-4.l fixes verified here:
 *
 *   1. Binary `.` and `$` (pattern-binding operators, priority 12,
 *      left-associative).  Mirrors snobol4.y:159-161.
 *
 *        pat . var   →  AST_CAPT_COND_ASGN(pat, var)
 *        pat $ var   →  AST_CAPT_IMMED_ASGN(pat, var)
 *
 *      Examples from the corpus:
 *        'ab' ? LEN(1) . X            (test_fence.sc inner)
 *        epsilon . *PushCounter()     (test_semantic.sc nPush)
 *
 *   2. AST_SCAN/AST_SEQ split when committing a stmt — for both bare-stmt
 *      `subj ? pat;` and conditional `if (subj ? pat) {…}` /
 *      `while (subj ? pat) {…}` / `do { … } while (subj ? pat);`.
 *      Mirrors snobol4.y:248-270.
 *
 *      After split, the stmt has separate `s->subject = subj` and
 *      `s->pattern = pat` slots so the runtime's pattern-match engine
 *      fires.  Without the split the whole AST_SCAN ends up in
 *      `s->subject`, the runtime evaluates it as a value (always
 *      succeeding), and the pattern-match-as-cond branches always
 *      take the success arm.
 *
 *      This was the runtime cause of the
 *      fence/match/semantic/trace beauty 3-mode FAILs in
 *      LS-4.l (12/42 → 0/42).
 *
 * Build:
 *   cc -Wall -o test_snocone_parse_j test_snocone_parse_j.c \
 *       ../../src/frontend/snocone/snocone_parse.tab.c \
 *       ../../src/frontend/snocone/snocone_lex.c \
 *       -I ../../src/frontend/snocone \
 *       -I ../../src/frontend/snobol4 \
 *       -I ../../src
 *
 * AUTHORS: Lon Jones Cherryholmes · Claude Sonnet
 * Commit identity: LCherryholmes / lcherryh@yahoo.com  (RULES.md)
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdarg.h>

#define IR_DEFINE_NAMES
#include "scrip_cc.h"

Program *snocone_parse_program(const char *src, const char *filename);

/* ---- Helpers (same shape as parse_c.c) ---- */

static int g_pass = 0, g_fail = 0;
static void check(const char *label, int cond, const char *fmt, ...) {
    if (cond) { printf("  PASS %s\n", label); g_pass++; }
    else {
        va_list ap; va_start(ap, fmt);
        printf("  FAIL %s: ", label); vprintf(fmt, ap); putchar('\n'); va_end(ap);
        g_fail++;
    }
}

static const char *kn(int k) {
    return ast_e_name[k] ? ast_e_name[k] : "?";
}

/* ============================================================ */
/* 1. Binary `.` (AST_CAPT_COND_ASGN) — fence/semantic prerequisite */
/* ============================================================ */

static void test_binary_dot_basic(void) {
    /* X = a . b   parses as  X = (a . b)
     * which is AST_ASSIGN(X, AST_CAPT_COND_ASGN(a, b))
     */
    const char *src = "X = a . b;";
    printf("=== test: %s ===\n", src);
    Program *prog = snocone_parse_program(src, "<test>");
    check("parses", prog != NULL, "NULL");
    if (!prog || !prog->head) return;
    AST_t *rhs = prog->head->replacement;
    check("rhs is AST_CAPT_COND_ASGN", rhs && rhs->kind == AST_CAPT_COND_ASGN,
          "got %s", rhs ? kn(rhs->kind) : "(nil)");
    if (!rhs || rhs->kind != AST_CAPT_COND_ASGN) return;
    check("rhs.nchildren == 2", rhs->nchildren == 2, "got %d", rhs->nchildren);
    check("rhs.child0 is AST_VAR(a)",
          rhs->children[0]->kind == AST_VAR &&
          strcmp(rhs->children[0]->sval, "a") == 0,
          "got %s", kn(rhs->children[0]->kind));
    check("rhs.child1 is AST_VAR(b)",
          rhs->children[1]->kind == AST_VAR &&
          strcmp(rhs->children[1]->sval, "b") == 0,
          "got %s", kn(rhs->children[1]->kind));
}

static void test_binary_dollar_basic(void) {
    /* X = a $ b  →  AST_ASSIGN(X, AST_CAPT_IMMED_ASGN(a, b)) */
    const char *src = "X = a $ b;";
    printf("=== test: %s ===\n", src);
    Program *prog = snocone_parse_program(src, "<test>");
    check("parses", prog != NULL, "NULL");
    if (!prog || !prog->head) return;
    AST_t *rhs = prog->head->replacement;
    check("rhs is AST_CAPT_IMMED_ASGN", rhs && rhs->kind == AST_CAPT_IMMED_ASGN,
          "got %s", rhs ? kn(rhs->kind) : "(nil)");
}

static void test_binary_dot_in_pattern(void) {
    /* The fence-test.sc inner pattern:  'ab' ? LEN(1) . X
     * After stmt commit the AST_SCAN splits to subject=QLIT('ab'),
     * pattern=AST_CAPT_COND_ASGN(LEN(1), X). */
    const char *src = "'ab' ? LEN(1) . X;";
    printf("=== test: %s ===\n", src);
    Program *prog = snocone_parse_program(src, "<test>");
    check("parses", prog != NULL, "NULL");
    if (!prog || !prog->head) return;
    STMT_t *s = prog->head;
    check("subject is AST_QLIT('ab')",
          s->subject && s->subject->kind == AST_QLIT &&
          strcmp(s->subject->sval, "ab") == 0,
          "got %s", s->subject ? kn(s->subject->kind) : "(nil)");
    check("pattern slot populated (split happened)",
          s->pattern != NULL, "NULL");
    if (!s->pattern) return;
    check("pattern is AST_CAPT_COND_ASGN",
          s->pattern->kind == AST_CAPT_COND_ASGN,
          "got %s", kn(s->pattern->kind));
    check("pattern.child0 is AST_FNC(LEN)",
          s->pattern->children[0]->kind == AST_FNC &&
          strcmp(s->pattern->children[0]->sval, "LEN") == 0,
          "got %s", kn(s->pattern->children[0]->kind));
    check("pattern.child1 is AST_VAR(X)",
          s->pattern->children[1]->kind == AST_VAR &&
          strcmp(s->pattern->children[1]->sval, "X") == 0,
          "got %s", kn(s->pattern->children[1]->kind));
}

static void test_binary_dot_with_alternation(void) {
    /* fence test 1:  'ab' ? (LEN(1) . X | FENCE)
     * Inside the parens: LEN(1) . X  |  FENCE
     * Binary `.` (pri 12) binds TIGHTER than `|` (pri 3), so this
     * parses as `(LEN(1) . X) | FENCE`. */
    const char *src = "'ab' ? (LEN(1) . X | FENCE);";
    printf("=== test: %s ===\n", src);
    Program *prog = snocone_parse_program(src, "<test>");
    check("parses", prog != NULL, "NULL");
    if (!prog || !prog->head) return;
    STMT_t *s = prog->head;
    check("pattern slot populated", s->pattern != NULL, "NULL");
    if (!s->pattern) return;
    check("pattern is AST_ALT", s->pattern->kind == AST_ALT,
          "got %s", kn(s->pattern->kind));
    check("alt.nchildren == 2", s->pattern->nchildren == 2,
          "got %d", s->pattern->nchildren);
    if (s->pattern->nchildren < 2) return;
    check("alt.child0 is AST_CAPT_COND_ASGN",
          s->pattern->children[0]->kind == AST_CAPT_COND_ASGN,
          "got %s", kn(s->pattern->children[0]->kind));
    check("alt.child1 is AST_VAR(FENCE) (pattern primitive)",
          s->pattern->children[1]->kind == AST_VAR &&
          strcmp(s->pattern->children[1]->sval, "FENCE") == 0,
          "got %s", kn(s->pattern->children[1]->kind));
}

static void test_binary_dot_below_exponent(void) {
    /* `.` is at pri 12, `^` at pri 11 — `.` binds TIGHTER than `^`.
     * a ^ b . c   parses as   a ^ (b . c)
     * which is AST_POW(a, AST_CAPT_COND_ASGN(b, c)) */
    const char *src = "X = a ^ b . c;";
    printf("=== test: %s ===\n", src);
    Program *prog = snocone_parse_program(src, "<test>");
    check("parses", prog != NULL, "NULL");
    if (!prog || !prog->head) return;
    AST_t *rhs = prog->head->replacement;
    check("rhs is AST_POW", rhs && rhs->kind == AST_POW,
          "got %s", rhs ? kn(rhs->kind) : "(nil)");
    if (!rhs || rhs->kind != AST_POW || rhs->nchildren < 2) return;
    check("pow.child1 is AST_CAPT_COND_ASGN",
          rhs->children[1]->kind == AST_CAPT_COND_ASGN,
          "got %s", kn(rhs->children[1]->kind));
}

static void test_binary_dot_above_subscript(void) {
    /* `.` is at pri 12, subscript `[]` at pri 15 — subscript binds
     * TIGHTER than `.`.  a[0] . X  parses as  (a[0]) . X
     * which is AST_CAPT_COND_ASGN(AST_IDX(a, 0), X) */
    const char *src = "Y = a[0] . X;";
    printf("=== test: %s ===\n", src);
    Program *prog = snocone_parse_program(src, "<test>");
    check("parses", prog != NULL, "NULL");
    if (!prog || !prog->head) return;
    AST_t *rhs = prog->head->replacement;
    check("rhs is AST_CAPT_COND_ASGN",
          rhs && rhs->kind == AST_CAPT_COND_ASGN,
          "got %s", rhs ? kn(rhs->kind) : "(nil)");
    if (!rhs || rhs->kind != AST_CAPT_COND_ASGN || rhs->nchildren < 2) return;
    check("dot.child0 is AST_IDX", rhs->children[0]->kind == AST_IDX,
          "got %s", kn(rhs->children[0]->kind));
}

static void test_binary_dot_with_unary_star(void) {
    /* semantic.sc:  epsilon . *PushCounter()
     * Right operand is unary `*` applied to a call.
     * Parses as AST_CAPT_COND_ASGN(epsilon, AST_DEFER(PushCounter())) */
    const char *src = "Y = epsilon . *PushCounter();";
    printf("=== test: %s ===\n", src);
    Program *prog = snocone_parse_program(src, "<test>");
    check("parses", prog != NULL, "NULL");
    if (!prog || !prog->head) return;
    AST_t *rhs = prog->head->replacement;
    check("rhs is AST_CAPT_COND_ASGN",
          rhs && rhs->kind == AST_CAPT_COND_ASGN,
          "got %s", rhs ? kn(rhs->kind) : "(nil)");
    if (!rhs || rhs->kind != AST_CAPT_COND_ASGN || rhs->nchildren < 2) return;
    check("dot.child0 is AST_VAR(epsilon)",
          rhs->children[0]->kind == AST_VAR &&
          strcmp(rhs->children[0]->sval, "epsilon") == 0,
          "got %s", kn(rhs->children[0]->kind));
    check("dot.child1 is AST_DEFER", rhs->children[1]->kind == AST_DEFER,
          "got %s", kn(rhs->children[1]->kind));
}

static void test_binary_dot_left_assoc(void) {
    /* a . b . c  parses left-associatively as  (a . b) . c
     * which is AST_CAPT_COND_ASGN(AST_CAPT_COND_ASGN(a, b), c) */
    const char *src = "X = a . b . c;";
    printf("=== test: %s ===\n", src);
    Program *prog = snocone_parse_program(src, "<test>");
    check("parses", prog != NULL, "NULL");
    if (!prog || !prog->head) return;
    AST_t *rhs = prog->head->replacement;
    check("rhs is AST_CAPT_COND_ASGN",
          rhs && rhs->kind == AST_CAPT_COND_ASGN,
          "got %s", rhs ? kn(rhs->kind) : "(nil)");
    if (!rhs || rhs->kind != AST_CAPT_COND_ASGN || rhs->nchildren < 2) return;
    check("outer.child0 is AST_CAPT_COND_ASGN (left-assoc)",
          rhs->children[0]->kind == AST_CAPT_COND_ASGN,
          "got %s — should chain LEFT not RIGHT", kn(rhs->children[0]->kind));
    check("outer.child1 is AST_VAR(c)",
          rhs->children[1]->kind == AST_VAR &&
          strcmp(rhs->children[1]->sval, "c") == 0,
          "got %s", kn(rhs->children[1]->kind));
}

static void test_unary_dot_unchanged(void) {
    /* Regression guard: unary .X (with leading whitespace gap before
     * `.`) still parses as AST_NAME(X), not as a binary `.`.  Without
     * leading expression, `.` is unary by FSM rule. */
    const char *src = "X = .Y;";
    printf("=== test: %s ===\n", src);
    Program *prog = snocone_parse_program(src, "<test>");
    check("parses", prog != NULL, "NULL");
    if (!prog || !prog->head) return;
    AST_t *rhs = prog->head->replacement;
    check("rhs is AST_NAME (unary .)",
          rhs && rhs->kind == AST_NAME,
          "got %s", rhs ? kn(rhs->kind) : "(nil)");
}

static void test_unary_dollar_unchanged(void) {
    /* Regression guard: $'#N' (semantic.sc convention) is unary $
     * applied to a string literal, NOT a binary $. */
    const char *src = "X = $'#N';";
    printf("=== test: %s ===\n", src);
    Program *prog = snocone_parse_program(src, "<test>");
    check("parses", prog != NULL, "NULL");
    if (!prog || !prog->head) return;
    AST_t *rhs = prog->head->replacement;
    check("rhs is AST_INDIRECT (unary $)",
          rhs && rhs->kind == AST_INDIRECT,
          "got %s", rhs ? kn(rhs->kind) : "(nil)");
}

/* ============================================================ */
/* 2. AST_SCAN split — bare stmt                                   */
/* ============================================================ */

static void test_scan_bare_stmt_split(void) {
    /* `subj ? pat;` — top-level is AST_SCAN(subj, pat).
     * After commit, must split into:
     *   s->subject = subj
     *   s->pattern = pat
     * (without the split, runtime evaluates AST_SCAN as value and
     *  always succeeds, breaking match() / notmatch() helpers). */
    const char *src = "subject ? pattern;";
    printf("=== test: %s ===\n", src);
    Program *prog = snocone_parse_program(src, "<test>");
    check("parses", prog != NULL, "NULL");
    if (!prog || !prog->head) return;
    STMT_t *s = prog->head;
    check("subject is AST_VAR(subject) (split happened)",
          s->subject && s->subject->kind == AST_VAR &&
          strcmp(s->subject->sval, "subject") == 0,
          "got %s", s->subject ? kn(s->subject->kind) : "(nil)");
    check("pattern slot populated",
          s->pattern != NULL, "NULL — split did not happen");
    if (!s->pattern) return;
    check("pattern is AST_VAR(pattern)",
          s->pattern->kind == AST_VAR &&
          strcmp(s->pattern->sval, "pattern") == 0,
          "got %s", kn(s->pattern->kind));
    check("has_eq is 0 (bare match, no replace)",
          s->has_eq == 0, "got %d", s->has_eq);
}

static void test_scan_replace_form_split(void) {
    /* `subj ? pat = repl;` — at expression level this is
     *   AST_ASSIGN(AST_SCAN(subj, pat), repl)
     * sc_append_stmt first applies AST_ASSIGN-split:
     *   s->subject = AST_SCAN(subj, pat)
     *   s->replacement = repl
     *   s->has_eq = 1
     * Then the SCAN-split fires:
     *   s->subject = subj
     *   s->pattern = pat
     *   s->replacement = repl  (unchanged)
     *   s->has_eq = 1          (unchanged) */
    const char *src = "subj ? pat = repl;";
    printf("=== test: %s ===\n", src);
    Program *prog = snocone_parse_program(src, "<test>");
    check("parses", prog != NULL, "NULL");
    if (!prog || !prog->head) return;
    STMT_t *s = prog->head;
    check("subject is AST_VAR(subj)",
          s->subject && s->subject->kind == AST_VAR &&
          strcmp(s->subject->sval, "subj") == 0,
          "got %s", s->subject ? kn(s->subject->kind) : "(nil)");
    check("pattern is AST_VAR(pat)",
          s->pattern && s->pattern->kind == AST_VAR &&
          strcmp(s->pattern->sval, "pat") == 0,
          "got %s", s->pattern ? kn(s->pattern->kind) : "(nil)");
    check("replacement is AST_VAR(repl)",
          s->replacement && s->replacement->kind == AST_VAR &&
          strcmp(s->replacement->sval, "repl") == 0,
          "got %s", s->replacement ? kn(s->replacement->kind) : "(nil)");
    check("has_eq == 1", s->has_eq == 1, "got %d", s->has_eq);
}

static void test_assign_match_rhs_no_split(void) {
    /* `result = subj ? pat;` — at expression level this is
     *   AST_ASSIGN(result, AST_SCAN(subj, pat))
     * sc_append_stmt does AST_ASSIGN-split:
     *   s->subject = result
     *   s->replacement = AST_SCAN(subj, pat)
     *   s->has_eq = 1
     * The SCAN node sits inside replacement — split must NOT fire
     * there.  s->pattern stays NULL. */
    const char *src = "result = subj ? pat;";
    printf("=== test: %s ===\n", src);
    Program *prog = snocone_parse_program(src, "<test>");
    check("parses", prog != NULL, "NULL");
    if (!prog || !prog->head) return;
    STMT_t *s = prog->head;
    check("subject is AST_VAR(result)",
          s->subject && s->subject->kind == AST_VAR &&
          strcmp(s->subject->sval, "result") == 0,
          "got %s", s->subject ? kn(s->subject->kind) : "(nil)");
    check("pattern slot is NULL (AST_SCAN inside repl, not split)",
          s->pattern == NULL,
          "got %s — replacement-side SCAN should not split",
          s->pattern ? kn(s->pattern->kind) : "(nil)");
    check("replacement is AST_SCAN (unchanged)",
          s->replacement && s->replacement->kind == AST_SCAN,
          "got %s", s->replacement ? kn(s->replacement->kind) : "(nil)");
    check("has_eq == 1", s->has_eq == 1, "got %d", s->has_eq);
}

/* ============================================================ */
/* 3. AST_SEQ split — bare juxtaposition `s pat;`                  */
/* ============================================================ */

static void test_seq_split_var_var(void) {
    /* Bare juxtaposition `s pat;` — Snocone space-as-concat lexes
     * the gap as T_CONCAT, lowers to AST_SEQ(s, pat).  The first
     * child is a name (AST_VAR), so the split fires:
     *   s->subject = s
     *   s->pattern = pat */
    const char *src = "s pat;";
    printf("=== test: %s ===\n", src);
    Program *prog = snocone_parse_program(src, "<test>");
    check("parses", prog != NULL, "NULL");
    if (!prog || !prog->head) return;
    STMT_t *st_ = prog->head;
    check("subject is AST_VAR(s)",
          st_->subject && st_->subject->kind == AST_VAR &&
          strcmp(st_->subject->sval, "s") == 0,
          "got %s", st_->subject ? kn(st_->subject->kind) : "(nil)");
    check("pattern slot populated",
          st_->pattern != NULL, "NULL — AST_SEQ split did not fire");
    if (!st_->pattern) return;
    check("pattern is AST_VAR(pat)",
          st_->pattern->kind == AST_VAR &&
          strcmp(st_->pattern->sval, "pat") == 0,
          "got %s", kn(st_->pattern->kind));
}

static void test_seq_split_three_pieces(void) {
    /* `s a b;` — AST_SEQ(s, a, b).  First is name, rest is two-piece
     * → split into subject=s, pattern=AST_SEQ(a, b). */
    const char *src = "s a b;";
    printf("=== test: %s ===\n", src);
    Program *prog = snocone_parse_program(src, "<test>");
    check("parses", prog != NULL, "NULL");
    if (!prog || !prog->head) return;
    STMT_t *s = prog->head;
    check("subject is AST_VAR(s)",
          s->subject && s->subject->kind == AST_VAR &&
          strcmp(s->subject->sval, "s") == 0,
          "got %s", s->subject ? kn(s->subject->kind) : "(nil)");
    check("pattern is AST_SEQ (rest aggregated)",
          s->pattern && s->pattern->kind == AST_SEQ,
          "got %s", s->pattern ? kn(s->pattern->kind) : "(nil)");
    if (!s->pattern || s->pattern->kind != AST_SEQ) return;
    check("pattern.nchildren == 2", s->pattern->nchildren == 2,
          "got %d", s->pattern->nchildren);
}

static void test_seq_split_string_first(void) {
    /* `'hello' SUFFIX;` — first child is AST_QLIT, qualifies for
     * split per snobol4.y:258.  subject='hello', pattern=SUFFIX. */
    const char *src = "'hello' SUFFIX;";
    printf("=== test: %s ===\n", src);
    Program *prog = snocone_parse_program(src, "<test>");
    check("parses", prog != NULL, "NULL");
    if (!prog || !prog->head) return;
    STMT_t *s = prog->head;
    check("subject is AST_QLIT('hello')",
          s->subject && s->subject->kind == AST_QLIT &&
          strcmp(s->subject->sval, "hello") == 0,
          "got %s", s->subject ? kn(s->subject->kind) : "(nil)");
    check("pattern is AST_VAR(SUFFIX)",
          s->pattern && s->pattern->kind == AST_VAR,
          "got %s", s->pattern ? kn(s->pattern->kind) : "(nil)");
}

static void test_seq_no_split_when_first_not_name(void) {
    /* If the first child of AST_SEQ is NOT a name-yielding atom, no
     * split happens — the whole AST_SEQ stays in subject.  Example:
     * `(a + b) c;` — first child is AST_ADD, not a name. */
    const char *src = "(a + b) c;";
    printf("=== test: %s ===\n", src);
    Program *prog = snocone_parse_program(src, "<test>");
    check("parses", prog != NULL, "NULL");
    if (!prog || !prog->head) return;
    STMT_t *s = prog->head;
    check("subject is AST_SEQ (no split)",
          s->subject && s->subject->kind == AST_SEQ,
          "got %s — should keep whole AST_SEQ when first is non-name",
          s->subject ? kn(s->subject->kind) : "(nil)");
    check("pattern is NULL",
          s->pattern == NULL,
          "got %s — split should NOT fire",
          s->pattern ? kn(s->pattern->kind) : "(nil)");
}

/* ============================================================ */
/* 4. Cond stmts — split must fire for if/while/do/while         */
/* ============================================================ */

/* Walk the program looking for the cond stmt — i.e. the stmt whose
 * `go->onfailure` (for if/while) or `go->onsuccess` (for do/while)
 * is set.  Returns NULL if none found. */
static STMT_t *find_cond_stmt(Program *prog) {
    for (STMT_t *s = prog->head; s; s = s->next) {
        if (s->go && (s->go->onfailure || s->go->onsuccess)) return s;
    }
    return NULL;
}

static void test_if_cond_scan_split(void) {
    /* if (subj ? pat) S — the cond stmt should have split subject
     * and pattern, with go->onfailure pointing at the synthesized
     * Lend label.  Without the split the runtime evaluates AST_SCAN
     * as a value, the cond never "fails", and the if always takes
     * the success arm.  This was the runtime cause of the
     * match-test FAILs. */
    const char *src = "if (subj ? pat) { x = 1; }";
    printf("=== test: %s ===\n", src);
    Program *prog = snocone_parse_program(src, "<test>");
    check("parses", prog != NULL, "NULL");
    if (!prog) return;
    STMT_t *cs = find_cond_stmt(prog);
    check("cond stmt found", cs != NULL, "no stmt with go set");
    if (!cs) return;
    check("cond.subject is AST_VAR(subj) (split happened)",
          cs->subject && cs->subject->kind == AST_VAR &&
          strcmp(cs->subject->sval, "subj") == 0,
          "got %s", cs->subject ? kn(cs->subject->kind) : "(nil)");
    check("cond.pattern is AST_VAR(pat)",
          cs->pattern && cs->pattern->kind == AST_VAR &&
          strcmp(cs->pattern->sval, "pat") == 0,
          "got %s", cs->pattern ? kn(cs->pattern->kind) : "(nil)");
    check("cond.go.onfailure points at a label",
          cs->go && cs->go->onfailure != NULL,
          "no onfailure target");
}

static void test_while_cond_scan_split(void) {
    /* while (subj ? pat) S — same shape as if. */
    const char *src = "while (subj ? pat) { x = 1; }";
    printf("=== test: %s ===\n", src);
    Program *prog = snocone_parse_program(src, "<test>");
    check("parses", prog != NULL, "NULL");
    if (!prog) return;
    STMT_t *cs = find_cond_stmt(prog);
    check("cond stmt found", cs != NULL, "no stmt with go set");
    if (!cs) return;
    check("cond.subject is AST_VAR(subj)",
          cs->subject && cs->subject->kind == AST_VAR &&
          strcmp(cs->subject->sval, "subj") == 0,
          "got %s", cs->subject ? kn(cs->subject->kind) : "(nil)");
    check("cond.pattern is AST_VAR(pat)",
          cs->pattern && cs->pattern->kind == AST_VAR &&
          strcmp(cs->pattern->sval, "pat") == 0,
          "got %s", cs->pattern ? kn(cs->pattern->kind) : "(nil)");
}

static void test_dowhile_cond_scan_split(void) {
    /* do { S } while (subj ? pat); — different finalizer
     * (sc_make_cond_succ_stmt vs sc_make_cond_fail_stmt) but the
     * split must fire there too.  go->onsuccess loops back to top. */
    const char *src = "do { x = 1; } while (subj ? pat);";
    printf("=== test: %s ===\n", src);
    Program *prog = snocone_parse_program(src, "<test>");
    check("parses", prog != NULL, "NULL");
    if (!prog) return;
    STMT_t *cs = find_cond_stmt(prog);
    check("cond stmt found", cs != NULL, "no stmt with go set");
    if (!cs) return;
    check("cond.subject is AST_VAR(subj)",
          cs->subject && cs->subject->kind == AST_VAR &&
          strcmp(cs->subject->sval, "subj") == 0,
          "got %s", cs->subject ? kn(cs->subject->kind) : "(nil)");
    check("cond.pattern is AST_VAR(pat)",
          cs->pattern && cs->pattern->kind == AST_VAR &&
          strcmp(cs->pattern->sval, "pat") == 0,
          "got %s", cs->pattern ? kn(cs->pattern->kind) : "(nil)");
    check("cond.go.onsuccess points at a label",
          cs->go && cs->go->onsuccess != NULL,
          "no onsuccess target");
}

static void test_if_cond_with_dot_pattern(void) {
    /* The fence headline: if ('ab' ? (LEN(1) . X | FENCE)) {…}
     * combines BOTH fixes — binary `.` in the pattern AND
     * the SCAN split for the cond. */
    const char *src = "if ('ab' ? (LEN(1) . X | FENCE)) { x = 1; }";
    printf("=== test: %s ===\n", src);
    Program *prog = snocone_parse_program(src, "<test>");
    check("parses", prog != NULL, "NULL");
    if (!prog) return;
    STMT_t *cs = find_cond_stmt(prog);
    check("cond stmt found", cs != NULL, "no stmt with go set");
    if (!cs) return;
    check("cond.subject is AST_QLIT('ab')",
          cs->subject && cs->subject->kind == AST_QLIT &&
          strcmp(cs->subject->sval, "ab") == 0,
          "got %s", cs->subject ? kn(cs->subject->kind) : "(nil)");
    check("cond.pattern is AST_ALT",
          cs->pattern && cs->pattern->kind == AST_ALT,
          "got %s", cs->pattern ? kn(cs->pattern->kind) : "(nil)");
    if (!cs->pattern || cs->pattern->kind != AST_ALT) return;
    check("alt.child0 is AST_CAPT_COND_ASGN",
          cs->pattern->children[0]->kind == AST_CAPT_COND_ASGN,
          "got %s", kn(cs->pattern->children[0]->kind));
    check("alt.child1 is AST_VAR(FENCE)",
          cs->pattern->children[1]->kind == AST_VAR &&
          strcmp(cs->pattern->children[1]->sval, "FENCE") == 0,
          "got %s", kn(cs->pattern->children[1]->kind));
}

static void test_if_cond_non_scan_no_split(void) {
    /* Regression guard: `if (DIFFER(x))` — cond is AST_FNC, not AST_SCAN
     * or AST_SEQ.  Split must NOT fire; pattern slot stays NULL. */
    const char *src = "if (DIFFER(x)) { y = 1; }";
    printf("=== test: %s ===\n", src);
    Program *prog = snocone_parse_program(src, "<test>");
    check("parses", prog != NULL, "NULL");
    if (!prog) return;
    STMT_t *cs = find_cond_stmt(prog);
    check("cond stmt found", cs != NULL, "no stmt with go set");
    if (!cs) return;
    check("cond.subject is AST_FNC", cs->subject && cs->subject->kind == AST_FNC,
          "got %s", cs->subject ? kn(cs->subject->kind) : "(nil)");
    check("cond.pattern is NULL (no split)",
          cs->pattern == NULL,
          "got %s — non-SCAN/non-SEQ should not split",
          cs->pattern ? kn(cs->pattern->kind) : "(nil)");
}

/* ============================================================ */
/* main                                                          */
/* ============================================================ */

int main(void) {
    /* 1. Binary . and $ */
    test_binary_dot_basic();
    test_binary_dollar_basic();
    test_binary_dot_in_pattern();
    test_binary_dot_with_alternation();
    test_binary_dot_below_exponent();
    test_binary_dot_above_subscript();
    test_binary_dot_with_unary_star();
    test_binary_dot_left_assoc();
    test_unary_dot_unchanged();
    test_unary_dollar_unchanged();
    /* 2. AST_SCAN split */
    test_scan_bare_stmt_split();
    test_scan_replace_form_split();
    test_assign_match_rhs_no_split();
    /* 3. AST_SEQ split */
    test_seq_split_var_var();
    test_seq_split_three_pieces();
    test_seq_split_string_first();
    test_seq_no_split_when_first_not_name();
    /* 4. Cond stmts */
    test_if_cond_scan_split();
    test_while_cond_scan_split();
    test_dowhile_cond_scan_split();
    test_if_cond_with_dot_pattern();
    test_if_cond_non_scan_no_split();

    printf("\n=== Results: %d passed, %d failed ===\n", g_pass, g_fail);
    return g_fail == 0 ? 0 : 1;
}
