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
    else       { printf("  FAIL: %s\n", desc); failed++; } \
} while(0)

#define STREQ(a,b) (strcmp((a),(b))==0)

int main(void) {
    sno_runtime_init();
    printf("snobol4.c runtime smoke test\n");
    printf("============================================================\n");

    /* --- SnoVal conversions --- */
    printf("\n[SnoVal conversions]\n");
    CHECK("int to str",       STREQ(sno_to_str(SNO_INT_VAL(42)), "42"));
    CHECK("int negative",     STREQ(sno_to_str(SNO_INT_VAL(-7)), "-7"));
    CHECK("str to int",       sno_to_int(SNO_STR_VAL("123")) == 123);
    CHECK("null to str empty",STREQ(sno_to_str(SNO_NULL_VAL), ""));
    CHECK("null to int 0",    sno_to_int(SNO_NULL_VAL) == 0);
    CHECK("datatype INT",     STREQ(sno_datatype(SNO_INT_VAL(1)), "INTEGER"));
    CHECK("datatype STR",     STREQ(sno_datatype(SNO_STR_VAL("x")), "STRING"));
    CHECK("datatype NULL",    STREQ(sno_datatype(SNO_NULL_VAL), "STRING"));

    /* --- String ops --- */
    printf("\n[String operations]\n");
    CHECK("concat",    STREQ(sno_concat("hello", " world"), "hello world"));
    CHECK("concat empty", STREQ(sno_concat("", "x"), "x"));
    CHECK("size",      sno_size("hello") == 5);
    CHECK("size empty",sno_size("") == 0);
    CHECK("size null", sno_size(NULL) == 0);

    /* --- Builtin functions --- */
    printf("\n[Builtin functions]\n");
    SnoVal sz = sno_size_fn(SNO_STR_VAL("hello"));
    CHECK("SIZE('hello')=5", sz.type==SNO_INT && sz.i==5);

    SnoVal dupl = sno_dupl_fn(SNO_STR_VAL("ab"), SNO_INT_VAL(3));
    CHECK("DUPL('ab',3)='ababab'", STREQ(sno_to_str(dupl), "ababab"));

    SnoVal trim = sno_trim_fn(SNO_STR_VAL("hello   "));
    CHECK("TRIM('hello   ')='hello'", STREQ(sno_to_str(trim), "hello"));

    SnoVal lpad = sno_lpad_fn(SNO_STR_VAL("hi"), SNO_INT_VAL(5), SNO_STR_VAL(" "));
    CHECK("LPAD('hi',5)='   hi'", STREQ(sno_to_str(lpad), "   hi"));

    SnoVal rpad = sno_rpad_fn(SNO_STR_VAL("hi"), SNO_INT_VAL(5), SNO_STR_VAL(" "));
    CHECK("RPAD('hi',5)='hi   '", STREQ(sno_to_str(rpad), "hi   "));

    SnoVal sub = sno_substr_fn(SNO_STR_VAL("hello"), SNO_INT_VAL(2), SNO_INT_VAL(3));
    CHECK("SUBSTR('hello',2,3)='ell'", STREQ(sno_to_str(sub), "ell"));

    SnoVal ch = sno_char_fn(SNO_INT_VAL(65));
    CHECK("CHAR(65)='A'", STREQ(sno_to_str(ch), "A"));

    SnoVal iv = sno_integer_fn(SNO_STR_VAL("42"));
    CHECK("INTEGER('42')=42", iv.type==SNO_INT && iv.i==42);

    SnoVal rv = sno_real_fn(SNO_INT_VAL(3));
    CHECK("REAL(3)=3.0", rv.type==SNO_REAL && rv.r==3.0);

    /* REPLACE: tr-style */
    SnoVal rep = sno_replace_fn(SNO_STR_VAL("hello"),
                                SNO_STR_VAL("aeiou"),
                                SNO_STR_VAL("AEIOU"));
    CHECK("REPLACE('hello',vowels,VOWELS)='hEllO'", STREQ(sno_to_str(rep), "hEllO"));

    /* --- Arithmetic --- */
    printf("\n[Arithmetic]\n");
    SnoVal a = SNO_INT_VAL(10), b = SNO_INT_VAL(3);
    CHECK("10+3=13",  sno_add(a,b).i == 13);
    CHECK("10-3=7",   sno_sub(a,b).i == 7);
    CHECK("10*3=30",  sno_mul(a,b).i == 30);
    CHECK("10/3=3",   sno_div(a,b).i == 3);   /* integer division */
    CHECK("10 EQ 10", sno_eq(a, SNO_INT_VAL(10)));
    CHECK("10 NE 3",  sno_ne(a, b));
    CHECK("3 LT 10",  sno_lt(b, a));
    CHECK("10 GT 3",  sno_gt(a, b));
    CHECK("10 GE 10", sno_ge(a, SNO_INT_VAL(10)));
    CHECK("3 LE 10",  sno_le(b, a));

    /* --- IDENT / DIFFER --- */
    printf("\n[IDENT / DIFFER]\n");
    CHECK("IDENT('x','x')=1", sno_ident(SNO_STR_VAL("x"), SNO_STR_VAL("x")));
    CHECK("IDENT('x','y')=0", !sno_ident(SNO_STR_VAL("x"), SNO_STR_VAL("y")));
    CHECK("IDENT(1,1)=1",     sno_ident(SNO_INT_VAL(1), SNO_INT_VAL(1)));
    CHECK("DIFFER('x','y')=1",sno_differ(SNO_STR_VAL("x"), SNO_STR_VAL("y")));
    /* null == empty string */
    CHECK("IDENT(null,'')=1", sno_ident(SNO_NULL_VAL, SNO_STR_VAL("")));

    /* --- Variable table --- */
    printf("\n[Variable table]\n");
    sno_var_set("FOO", SNO_INT_VAL(99));
    CHECK("var set/get", sno_var_get("FOO").i == 99);
    sno_var_set("BAR", SNO_STR_VAL("hello"));
    CHECK("var str", STREQ(sno_to_str(sno_var_get("BAR")), "hello"));
    SnoVal miss = sno_var_get("MISSING");
    CHECK("missing var = null", miss.type == SNO_NULL);

    /* $name indirect: FOO contains "BAR", $FOO should give BAR's value */
    sno_var_set("PTR", SNO_STR_VAL("BAR"));
    SnoVal indirect = sno_indirect_get("PTR");
    CHECK("indirect $PTR=BAR='hello'", STREQ(sno_to_str(indirect), "hello"));

    /* --- Counter stack --- */
    printf("\n[Counter stack]\n");
    sno_npush();
    CHECK("nTop after push=0",  sno_ntop() == 0);
    sno_ninc();
    sno_ninc();
    sno_ninc();
    CHECK("nTop after 3 inc=3", sno_ntop() == 3);
    sno_ndec();
    CHECK("nTop after dec=2",   sno_ntop() == 2);
    sno_npop();
    /* stack should be empty now — ntop returns 0 */
    sno_npush();
    CHECK("fresh push=0",       sno_ntop() == 0);
    sno_npop();

    /* --- Value stack --- */
    printf("\n[Value stack]\n");
    sno_push(SNO_INT_VAL(1));
    sno_push(SNO_INT_VAL(2));
    sno_push(SNO_INT_VAL(3));
    CHECK("stack depth=3",   sno_stack_depth() == 3);
    CHECK("top=3",           sno_top().i == 3);
    CHECK("pop=3",           sno_pop().i == 3);
    CHECK("pop=2",           sno_pop().i == 2);
    CHECK("stack depth=1",   sno_stack_depth() == 1);
    sno_pop();
    CHECK("stack empty=0",   sno_stack_depth() == 0);

    /* --- Tree --- */
    printf("\n[Tree]\n");
    Tree *root = sno_tree_new0("expr");
    Tree *c1   = sno_tree_new("num", SNO_INT_VAL(5));
    Tree *c2   = sno_tree_new("num", SNO_INT_VAL(7));
    sno_tree_append(root, c1);
    sno_tree_append(root, c2);
    CHECK("tree tag",      STREQ(sno_t(root), "expr"));
    CHECK("tree n=2",      sno_n(root) == 2);
    CHECK("tree c[1]=5",   sno_c_i(root, 1)->val.i == 5);
    CHECK("tree c[2]=7",   sno_c_i(root, 2)->val.i == 7);
    sno_tree_prepend(root, sno_tree_new0("dummy"));
    CHECK("prepend n=3",   sno_n(root) == 3);
    CHECK("prepend c[1]=dummy", STREQ(sno_c_i(root,1)->tag, "dummy"));
    Tree *removed = sno_tree_remove(root, 1);
    CHECK("remove dummy",  STREQ(removed->tag, "dummy"));
    CHECK("after remove n=2", sno_n(root) == 2);

    /* --- Array --- */
    printf("\n[Array]\n");
    SnoArray *arr = sno_array_new(1, 4);
    sno_array_set(arr, 1, SNO_INT_VAL(10));
    sno_array_set(arr, 4, SNO_INT_VAL(40));
    CHECK("arr[1]=10", sno_array_get(arr, 1).i == 10);
    CHECK("arr[4]=40", sno_array_get(arr, 4).i == 40);
    CHECK("arr[2]=null", sno_array_get(arr, 2).type == SNO_NULL);
    CHECK("arr OOB=null", sno_array_get(arr, 99).type == SNO_NULL);

    /* --- Table --- */
    printf("\n[Table]\n");
    SnoTable *tbl = sno_table_new();
    sno_table_set(tbl, "key1", SNO_INT_VAL(100));
    sno_table_set(tbl, "key2", SNO_STR_VAL("value"));
    CHECK("tbl key1=100", sno_table_get(tbl, "key1").i == 100);
    CHECK("tbl key2=value", STREQ(sno_to_str(sno_table_get(tbl, "key2")), "value"));
    CHECK("tbl has key1",  sno_table_has(tbl, "key1"));
    CHECK("tbl no key3",  !sno_table_has(tbl, "key3"));
    sno_table_set(tbl, "key1", SNO_INT_VAL(999));
    CHECK("tbl update",   sno_table_get(tbl, "key1").i == 999);

    /* --- DATA() / UDef --- */
    printf("\n[DATA / UDef]\n");
    sno_data_define("node(left,right,value)");
    SnoVal n = sno_udef_new("node",
                            SNO_NULL_VAL,  /* left  */
                            SNO_NULL_VAL,  /* right */
                            SNO_INT_VAL(42), /* value */
                            (SnoVal){0}    /* sentinel */);
    CHECK("udef type=node", STREQ(sno_datatype(n), "node"));
    SnoVal val = sno_field_get(n, "value");
    CHECK("udef field value=42", val.i == 42);
    sno_field_set(n, "value", SNO_INT_VAL(99));
    CHECK("udef field set", sno_field_get(n, "value").i == 99);

    /* --- Function table --- */
    printf("\n[Function table]\n");
    /* Define a trivial add function */
    SnoFunc add_fn = NULL;
    (void)add_fn;
    sno_define("double(x)", NULL);  /* no C body — just register */
    CHECK("func exists", sno_func_exists("double"));
    CHECK("func not exists", !sno_func_exists("nothing"));

    /* --- Summary --- */
    printf("\n============================================================\n");
    printf("Results: %d passed, %d failed\n", passed, failed);
    if (failed == 0)
        printf("ALL PASS — snobol4.c runtime is correct.\n");
    return failed ? 1 : 0;
}
