/* test_snobol4_runtime.c — smoke test for Sprint 20 snobol4.c runtime
 *
 * Build: cc -o test_runtime test_snobol4_runtime.c snobol4.c -I. -lgc -lm
 * Run:   ./test_runtime
 */

#include "snobol4.h"
#include <stdio.h>
#include <assert.h>
#include <string.h>

static int passed = 0, failed = 0;

#define CHECK(desc, cond) do { \
    if (cond) { printf("  PASS: %s\n", desc); passed++; } \
    else       { printf("  DT_FAIL: %s\n", desc); failed++; } \
} while(0)

#define STREQ(a,b) (strcmp((a),(b))==0)

int main(void) {
    SNO_INIT_fn();
    printf("snobol4.c runtime smoke test\n");
    printf("============================================================\n");

    /* --- DESCR_t conversions --- */
    printf("\n[DESCR_t conversions]\n");
    CHECK("int to STRVAL_fn",       STREQ(VARVAL_fn(INTVAL(42)), "42"));
    CHECK("int negative",     STREQ(VARVAL_fn(INTVAL(-7)), "-7"));
    CHECK("STRVAL_fn to int",       to_int(STRVAL("123")) == 123);
    CHECK("null to STRVAL_fn empty",STREQ(VARVAL_fn(NULVCL), ""));
    CHECK("null to int 0",    to_int(NULVCL) == 0);
    CHECK("datatype INT",     STREQ(datatype(INTVAL(1)), "INTEGER"));
    CHECK("datatype STR",     STREQ(datatype(STRVAL("x")), "STRING"));
    CHECK("datatype NULL",    STREQ(datatype(NULVCL), "STRING"));

    /* --- String ops --- */
    printf("\n[String operations]\n");
    CHECK("CONCAT_fn",    STREQ(CONCAT_fn("hello", " world"), "hello world"));
    CHECK("CONCAT_fn empty", STREQ(CONCAT_fn("", "x"), "x"));
    CHECK("size",      size("hello") == 5);
    CHECK("size empty",size("") == 0);
    CHECK("size null", size(NULL) == 0);

    /* --- Builtin functions --- */
    printf("\n[Builtin functions]\n");
    DESCR_t sz = SIZE_fn(STRVAL("hello"));
    CHECK("SIZE_fn('hello')=5", sz.v==DT_I && sz.i==5);

    DESCR_t dupl_result = DUPL_fn(STRVAL("ab"), INTVAL(3));
    CHECK("DUPL_fn('ab',3)='ababab'", STREQ(VARVAL_fn(dupl_result), "ababab"));

    DESCR_t trim = TRIM_fn(STRVAL("hello   "));
    CHECK("TRIM_fn('hello   ')='hello'", STREQ(VARVAL_fn(trim), "hello"));

    DESCR_t lpad = lpad_fn(STRVAL("hi"), INTVAL(5), STRVAL(" "));
    CHECK("LPAD('hi',5)='   hi'", STREQ(VARVAL_fn(lpad), "   hi"));

    DESCR_t rpad = rpad_fn(STRVAL("hi"), INTVAL(5), STRVAL(" "));
    CHECK("RPAD('hi',5)='hi   '", STREQ(VARVAL_fn(rpad), "hi   "));

    DESCR_t sub = SUBSTR_fn(STRVAL("hello"), INTVAL(2), INTVAL(3));
    CHECK("SUBSTR_fn('hello',2,3)='ell'", STREQ(VARVAL_fn(sub), "ell"));

    DESCR_t ch = BCHAR_fn(INTVAL(65));
    CHECK("CHAR(65)='A'", STREQ(VARVAL_fn(ch), "A"));

    DESCR_t iv = INTGER_fn(STRVAL("42"));
    CHECK("INTEGER('42')=42", iv.v==DT_I && iv.i==42);

    DESCR_t rv = real_fn(INTVAL(3));
    CHECK("REAL(3)=3.0", rv.v==DT_R && rv.r==3.0);

    /* REPLACE: tr-style */
    DESCR_t rep = REPLACE_fn(STRVAL("hello"),
                                STRVAL("aeiou"),
                                STRVAL("AEIOU"));
    CHECK("REPLACE('hello',vowels,VOWELS)='hEllO'", STREQ(VARVAL_fn(rep), "hEllO"));

    /* --- Arithmetic --- */
    printf("\n[Arithmetic]\n");
    DESCR_t a = INTVAL(10), b = INTVAL(3);
    CHECK("10+3=13",  add(a,b).i == 13);
    CHECK("10-3=7",   sub(a,b).i == 7);
    CHECK("10*3=30",  mul(a,b).i == 30);
    CHECK("10/3=3",   DIVIDE_fn(a,b).i == 3);   /* integer division */
    CHECK("10 EQ 10", eq(a, INTVAL(10)));
    CHECK("10 NE 3",  ne(a, b));
    CHECK("3 LT 10",  lt(b, a));
    CHECK("10 GT 3",  gt(a, b));
    CHECK("10 GE 10", ge(a, INTVAL(10)));
    CHECK("3 LE 10",  le(b, a));

    /* --- IDENT / DIFFER --- */
    printf("\n[IDENT / DIFFER]\n");
    CHECK("IDENT('x','x')=1", ident(STRVAL("x"), STRVAL("x")));
    CHECK("IDENT('x','y')=0", !ident(STRVAL("x"), STRVAL("y")));
    CHECK("IDENT(1,1)=1",     ident(INTVAL(1), INTVAL(1)));
    CHECK("DIFFER('x','y')=1",differ(STRVAL("x"), STRVAL("y")));
    /* null == empty string */
    CHECK("IDENT(null,'')=1", ident(NULVCL, STRVAL("")));

    /* --- Variable table --- */
    printf("\n[Variable table]\n");
    NV_SET_fn("FOO", INTVAL(99));
    CHECK("var set/get", NV_GET_fn("FOO").i == 99);
    NV_SET_fn("BAR", STRVAL("hello"));
    CHECK("var STRVAL_fn", STREQ(VARVAL_fn(NV_GET_fn("BAR")), "hello"));
    DESCR_t miss = NV_GET_fn("MISSING");
    CHECK("missing var = null", miss.v == DT_SNUL);

    /* $name indirect: FOO contains "BAR", $FOO should give BAR's value */
    NV_SET_fn("PTR", STRVAL("BAR"));
    DESCR_t indirect = INDR_GET_fn("PTR");
    CHECK("indirect $PTR=BAR='hello'", STREQ(VARVAL_fn(indirect), "hello"));

    /* --- Counter stack --- */
    printf("\n[Counter stack]\n");
    NPUSH_fn();
    CHECK("nTop after PUSH_fn=0",  ntop() == 0);
    NINC_fn();
    NINC_fn();
    NINC_fn();
    CHECK("nTop after 3 inc=3", ntop() == 3);
    NDEC_fn();
    CHECK("nTop after dec=2",   ntop() == 2);
    NPOP_fn();
    /* stack should be empty now — ntop returns 0 */
    NPUSH_fn();
    CHECK("fresh PUSH_fn=0",       ntop() == 0);
    NPOP_fn();

    /* --- Value stack --- */
    printf("\n[Value stack]\n");
    PUSH_fn(INTVAL(1));
    PUSH_fn(INTVAL(2));
    PUSH_fn(INTVAL(3));
    CHECK("stack depth=3",   STACK_DEPTH_fn() == 3);
    CHECK("TOP_fn=3",           TOP_fn().i == 3);
    CHECK("POP_fn=3",           POP_fn().i == 3);
    CHECK("POP_fn=2",           POP_fn().i == 2);
    CHECK("stack depth=1",   STACK_DEPTH_fn() == 1);
    POP_fn();
    CHECK("stack empty=0",   STACK_DEPTH_fn() == 0);

    /* --- TREEBLK_t --- */
    printf("\n[TREEBLK_t]\n");
    TREEBLK_t *root = tree_new0("expr");
    TREEBLK_t *c1   = tree_new("num", INTVAL(5));
    TREEBLK_t *c2   = tree_new("num", INTVAL(7));
    tree_append(root, c1);
    tree_append(root, c2);
    CHECK("tree tag",      STREQ(t(root), "expr"));
    CHECK("tree n=2",      n(root) == 2);
    CHECK("tree c[1]=5",   c_i(root, 1)->val.i == 5);
    CHECK("tree c[2]=7",   c_i(root, 2)->val.i == 7);
    tree_prepend(root, tree_new0("dummy"));
    CHECK("prepend n=3",   n(root) == 3);
    CHECK("prepend c[1]=dummy", STREQ(c_i(root,1)->tag, "dummy"));
    TREEBLK_t *removed = tree_remove(root, 1);
    CHECK("remove dummy",  STREQ(removed->tag, "dummy"));
    CHECK("after remove n=2", n(root) == 2);

    /* --- Array --- */
    printf("\n[Array]\n");
    ARBLK_t *arr = array_new(1, 4);
    array_set(arr, 1, INTVAL(10));
    array_set(arr, 4, INTVAL(40));
    CHECK("arr[1]=10", array_get(arr, 1).i == 10);
    CHECK("arr[4]=40", array_get(arr, 4).i == 40);
    CHECK("arr[2]=null", array_get(arr, 2).v == DT_SNUL);
    CHECK("arr OOB=null", array_get(arr, 99).v == DT_SNUL);

    /* --- Table --- */
    printf("\n[Table]\n");
    TBBLK_t *tbl = table_new();
    table_set(tbl, "key1", INTVAL(100));
    table_set(tbl, "key2", STRVAL("value"));
    CHECK("tbl key1=100", table_get(tbl, "key1").i == 100);
    CHECK("tbl key2=value", STREQ(VARVAL_fn(table_get(tbl, "key2")), "value"));
    CHECK("tbl has key1",  table_has(tbl, "key1"));
    CHECK("tbl no key3",  !table_has(tbl, "key3"));
    table_set(tbl, "key1", INTVAL(999));
    CHECK("tbl update",   table_get(tbl, "key1").i == 999);

    /* --- DT_DATA() / DATINST_t --- */
    printf("\n[DT_DATA / DATINST_t]\n");
    DEFDAT_fn("node(left,right,value)");
    DESCR_t n = DATCON_fn("node",
                            NULVCL,  /* left  */
                            NULVCL,  /* right */
                            INTVAL(42), /* value */
                            (DESCR_t){0}    /* sentinel */);
    CHECK("udef type=node", STREQ(datatype(n), "node"));
    DESCR_t val = FIELD_GET_fn(n, "value");
    CHECK("udef field value=42", val.i == 42);
    FIELD_SET_fn(n, "value", INTVAL(99));
    CHECK("udef field set", FIELD_GET_fn(n, "value").i == 99);

    /* --- Function table --- */
    printf("\n[Function table]\n");
    /* Define a trivial add function */
    FNCPTR_t add_fn = NULL;
    (void)add_fn;
    DEFINE_fn("double(x)", NULL);  /* no C body — just register */
    CHECK("func exists", FNCEX_fn("double"));
    CHECK("func not exists", !FNCEX_fn("nothing"));

    /* --- Summary --- */
    printf("\n============================================================\n");
    printf("Results: %d passed, %d failed\n", passed, failed);
    if (failed == 0)
        printf("ALL PASS — snobol4.c runtime is correct.\n");
    return failed ? 1 : 0;
}
