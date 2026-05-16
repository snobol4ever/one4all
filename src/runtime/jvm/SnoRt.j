; SnoRt.j — SCRIP SNOBOL4 JVM runtime
; AUTHORS: Lon Jones Cherryholmes · Jeffrey Cooper M.D. · Claude Sonnet 4.6
;
; Provides scalar operations called by scrip --sm-emit --target=jvm output.
; All methods are static. The value vstack is a static Deque<Object>.
; SNOBOL4 values are boxed as: Long (integer), Double (real), String (string), null (null/unset).
; MatchState is an inner class holding sigma/delta/omega for pattern matching.
; OUTPUT trap: store_var("OUTPUT") prints TOS to System.out.
; INPUT source: push_var("INPUT") reads a line from System.in.

; ── SnoRt ────────────────────────────────────────────────────────────────────
.class public rt/SnoRt
.super java/lang/Object

; value stack
.field private static final vstack Ljava/util/ArrayDeque;

; last_ok flag (SNOBOL4 success/failure of last statement)
.field private static last_ok Z

; variable store (name → Object)
.field private static final vars Ljava/util/HashMap;

; call stack for user-function dispatch (stores return PCs as Integer)
.field private static final ret_stack Ljava/util/ArrayDeque;

; function parameter registry: fn name → param name array (from DEFINE)
.field private static final fn_params Ljava/util/HashMap;

; current function name for return value dispatch
.field private static _ret_fn Ljava/lang/String;

; stdin reader for INPUT
.field private static reader Ljava/io/BufferedReader;

; static initialiser
.method static <clinit>()V
    .limit stack 8
    .limit locals 0
    new java/util/ArrayDeque
    dup
    invokespecial java/util/ArrayDeque/<init>()V
    putstatic rt/SnoRt/vstack Ljava/util/ArrayDeque;
    new java/util/HashMap
    dup
    invokespecial java/util/HashMap/<init>()V
    putstatic rt/SnoRt/vars Ljava/util/HashMap;
    new java/util/ArrayDeque
    dup
    invokespecial java/util/ArrayDeque/<init>()V
    putstatic rt/SnoRt/ret_stack Ljava/util/ArrayDeque;
    new java/util/HashMap
    dup
    invokespecial java/util/HashMap/<init>()V
    putstatic rt/SnoRt/fn_params Ljava/util/HashMap;
    new java/io/BufferedReader
    dup
    new java/io/InputStreamReader
    dup
    getstatic java/lang/System/in Ljava/io/InputStream;
    invokespecial java/io/InputStreamReader/<init>(Ljava/io/InputStream;)V
    invokespecial java/io/BufferedReader/<init>(Ljava/io/Reader;)V
    putstatic rt/SnoRt/reader Ljava/io/BufferedReader;
    return
.end method

; ── init / finalize ──────────────────────────────────────────────────────────
.method public static init()V
    .limit stack 0
    .limit locals 0
    return
.end method

.method public static finalize_rt()I
    .limit stack 1
    .limit locals 0
    iconst_0
    ireturn
.end method

; ── vstack helpers ─────────────────────────────────────────────────────────────
; push an Object onto TOS
.method private static push_obj(Ljava/lang/Object;)V
    .limit stack 3
    .limit locals 1
    getstatic rt/SnoRt/vstack Ljava/util/ArrayDeque;
    aload_0
    invokevirtual java/util/ArrayDeque/push(Ljava/lang/Object;)V
    return
.end method

; pop an Object from TOS (returns null if empty)
.method private static pop_obj()Ljava/lang/Object;
    .limit stack 2
    .limit locals 1
    getstatic rt/SnoRt/vstack Ljava/util/ArrayDeque;
    invokevirtual java/util/ArrayDeque/isEmpty()Z
    ifeq pop_obj_not_empty
    aconst_null
    areturn
pop_obj_not_empty:
    getstatic rt/SnoRt/vstack Ljava/util/ArrayDeque;
    invokevirtual java/util/ArrayDeque/pop()Ljava/lang/Object;
    areturn
.end method

; peek TOS without removing
.method private static peek_obj()Ljava/lang/Object;
    .limit stack 2
    .limit locals 0
    getstatic rt/SnoRt/vstack Ljava/util/ArrayDeque;
    invokevirtual java/util/ArrayDeque/peek()Ljava/lang/Object;
    areturn
.end method

; ── push operations ───────────────────────────────────────────────────────────
.method public static push_int(J)V
    .limit stack 4
    .limit locals 2
    lload_0
    invokestatic java/lang/Long/valueOf(J)Ljava/lang/Long;
    invokestatic rt/SnoRt/push_obj(Ljava/lang/Object;)V
    return
.end method

.method public static push_str(Ljava/lang/String;)V
    .limit stack 2
    .limit locals 1
    aload_0
    invokestatic rt/SnoRt/push_obj(Ljava/lang/Object;)V
    return
.end method

.method public static push_real(D)V
    .limit stack 4
    .limit locals 2
    dload_0
    invokestatic java/lang/Double/valueOf(D)Ljava/lang/Double;
    invokestatic rt/SnoRt/push_obj(Ljava/lang/Object;)V
    return
.end method

.method public static push_null()V
    .limit stack 2
    .limit locals 0
    ldc ""
    invokestatic rt/SnoRt/push_obj(Ljava/lang/Object;)V
    return
.end method

; push value of a named variable (null if unset; INPUT reads a line)
.method public static push_var(Ljava/lang/String;)V
    .limit stack 3
    .limit locals 2
    aload_0
    ldc "INPUT"
    invokevirtual java/lang/String/equals(Ljava/lang/Object;)Z
    ifeq push_var_normal
    ; INPUT: read a line
    getstatic rt/SnoRt/reader Ljava/io/BufferedReader;
    invokevirtual java/io/BufferedReader/readLine()Ljava/lang/String;
    astore_1
    aload_1
    invokestatic rt/SnoRt/push_obj(Ljava/lang/Object;)V
    return
push_var_normal:
    getstatic rt/SnoRt/vars Ljava/util/HashMap;
    aload_0
    invokevirtual java/util/HashMap/get(Ljava/lang/Object;)Ljava/lang/Object;
    astore_1
    aload_1
    ifnonnull push_var_got
    ldc ""
    invokestatic rt/SnoRt/push_obj(Ljava/lang/Object;)V
    return
push_var_got:
    aload_1
    invokestatic rt/SnoRt/push_obj(Ljava/lang/Object;)V
    return
.end method

; ── store_var — pop TOS, assign to named variable ────────────────────────────
; OUTPUT trap: if name=="OUTPUT", print to stdout instead.
.method public static store_var(Ljava/lang/String;)V
    .limit stack 3
    .limit locals 2
    invokestatic rt/SnoRt/pop_obj()Ljava/lang/Object;
    astore_1
    aload_0
    ldc "OUTPUT"
    invokevirtual java/lang/String/equals(Ljava/lang/Object;)Z
    ifeq store_var_normal
    ; OUTPUT: print value
    getstatic java/lang/System/out Ljava/io/PrintStream;
    aload_1
    ifnonnull store_var_not_null
    ldc ""
    goto store_var_print
store_var_not_null:
    aload_1
    invokevirtual java/lang/Object/toString()Ljava/lang/String;
store_var_print:
    invokevirtual java/io/PrintStream/println(Ljava/lang/String;)V
    return
store_var_normal:
    getstatic rt/SnoRt/vars Ljava/util/HashMap;
    aload_0
    aload_1
    invokevirtual java/util/HashMap/put(Ljava/lang/Object;Ljava/lang/Object;)Ljava/lang/Object;
    pop
    return
.end method

; ── pop_void — discard TOS ────────────────────────────────────────────────────
.method public static pop_void()V
    .limit stack 2
    .limit locals 0
    invokestatic rt/SnoRt/pop_obj()Ljava/lang/Object;
    pop
    return
.end method

; ── concat — pop two strings, push concatenation ─────────────────────────────
.method public static concat()V
    .limit stack 4
    .limit locals 2
    invokestatic rt/SnoRt/pop_obj()Ljava/lang/Object;
    astore_0
    invokestatic rt/SnoRt/pop_obj()Ljava/lang/Object;
    astore_1
    new java/lang/StringBuilder
    dup
    invokespecial java/lang/StringBuilder/<init>()V
    aload_1
    ifnonnull concat_left_nonnull
    ldc ""
    goto concat_left_append
concat_left_nonnull:
    aload_1
    invokevirtual java/lang/Object/toString()Ljava/lang/String;
concat_left_append:
    invokevirtual java/lang/StringBuilder/append(Ljava/lang/String;)Ljava/lang/StringBuilder;
    aload_0
    ifnonnull concat_right_nonnull
    ldc ""
    goto concat_right_append
concat_right_nonnull:
    aload_0
    invokevirtual java/lang/Object/toString()Ljava/lang/String;
concat_right_append:
    invokevirtual java/lang/StringBuilder/append(Ljava/lang/String;)Ljava/lang/StringBuilder;
    invokevirtual java/lang/StringBuilder/toString()Ljava/lang/String;
    invokestatic rt/SnoRt/push_obj(Ljava/lang/Object;)V
    return
.end method

; ── coerce_num — coerce TOS to numeric (Long or Double) ──────────────────────
; Mirrors C rt_coerce_num: string→atoll first; if 0 and not "0" prefix try atof; else 0.
; Non-numeric strings → 0 (no exception), matching C atoll/atof behavior.
.method public static coerce_num()V
    .limit stack 4
    .limit locals 2
    invokestatic rt/SnoRt/pop_obj()Ljava/lang/Object;
    astore_0
    aload_0
    instanceof java/lang/Long
    ifne coerce_num_done_push
    aload_0
    instanceof java/lang/Double
    ifne coerce_num_done_push
    aload_0
    ifnull coerce_num_zero
    aload_0
    invokevirtual java/lang/Object/toString()Ljava/lang/String;
    astore_1
    aload_1
    invokestatic rt/SnoRt/parse_long_safe(Ljava/lang/String;)Ljava/lang/Object;
    invokestatic rt/SnoRt/push_obj(Ljava/lang/Object;)V
    return
coerce_num_zero:
    lconst_0
    invokestatic java/lang/Long/valueOf(J)Ljava/lang/Long;
    invokestatic rt/SnoRt/push_obj(Ljava/lang/Object;)V
    return
coerce_num_done_push:
    aload_0
    invokestatic rt/SnoRt/push_obj(Ljava/lang/Object;)V
    return
.end method
; ── parse_long_safe — atoll-like: parse trimmed string as Long; if fails try Double; else Long(0) ──
.method private static parse_long_safe(Ljava/lang/String;)Ljava/lang/Object;
    .limit stack 4
    .limit locals 2
    aload_0
    invokevirtual java/lang/String/trim()Ljava/lang/String;
    astore_1
    ; try Long.parseLong
    aload_1
    invokestatic rt/SnoRt/try_parse_long(Ljava/lang/String;)Ljava/lang/Object;
    astore_0
    aload_0
    ifnull parse_long_try_double
    aload_0
    areturn
parse_long_try_double:
    aload_1
    invokestatic rt/SnoRt/try_parse_double(Ljava/lang/String;)Ljava/lang/Object;
    astore_0
    aload_0
    ifnull parse_long_zero
    aload_0
    areturn
parse_long_zero:
    lconst_0
    invokestatic java/lang/Long/valueOf(J)Ljava/lang/Long;
    areturn
.end method
; ── try_parse_long — returns Long or null on NumberFormatException ──
.method private static try_parse_long(Ljava/lang/String;)Ljava/lang/Object;
    .limit stack 3
    .limit locals 2
    .catch java/lang/NumberFormatException from L_tpl_start to L_tpl_end using L_tpl_catch
L_tpl_start:
    aload_0
    invokestatic java/lang/Long/parseLong(Ljava/lang/String;)J
    invokestatic java/lang/Long/valueOf(J)Ljava/lang/Long;
    areturn
L_tpl_end:
L_tpl_catch:
    astore_1
    aconst_null
    areturn
.end method
; ── try_parse_double — returns Double or null on NumberFormatException ──
.method private static try_parse_double(Ljava/lang/String;)Ljava/lang/Object;
    .limit stack 3
    .limit locals 2
    .catch java/lang/NumberFormatException from L_tpd_start to L_tpd_end using L_tpd_catch
L_tpd_start:
    aload_0
    invokestatic java/lang/Double/parseDouble(Ljava/lang/String;)D
    invokestatic java/lang/Double/valueOf(D)Ljava/lang/Double;
    areturn
L_tpd_end:
L_tpd_catch:
    astore_1
    aconst_null
    areturn
.end method

; ── neg — negate TOS ──────────────────────────────────────────────────────────
.method public static neg()V
    .limit stack 4
    .limit locals 1
    invokestatic rt/SnoRt/pop_obj()Ljava/lang/Object;
    astore_0
    aload_0
    instanceof java/lang/Double
    ifeq neg_long
    aload_0
    checkcast java/lang/Double
    invokevirtual java/lang/Double/doubleValue()D
    dneg
    invokestatic java/lang/Double/valueOf(D)Ljava/lang/Double;
    invokestatic rt/SnoRt/push_obj(Ljava/lang/Object;)V
    return
neg_long:
    aload_0
    checkcast java/lang/Long
    invokevirtual java/lang/Long/longValue()J
    lneg
    invokestatic java/lang/Long/valueOf(J)Ljava/lang/Long;
    invokestatic rt/SnoRt/push_obj(Ljava/lang/Object;)V
    return
.end method

; ── exp_op — exponentiation: pop exponent then base, push result ──────────────
.method public static exp_op()V
    .limit stack 6
    .limit locals 2
    invokestatic rt/SnoRt/pop_obj()Ljava/lang/Object;
    astore_0
    invokestatic rt/SnoRt/pop_obj()Ljava/lang/Object;
    astore_1
    aload_1
    checkcast java/lang/Number
    invokevirtual java/lang/Number/doubleValue()D
    aload_0
    checkcast java/lang/Number
    invokevirtual java/lang/Number/doubleValue()D
    invokestatic java/lang/Math/pow(DD)D
    invokestatic java/lang/Double/valueOf(D)Ljava/lang/Double;
    invokestatic rt/SnoRt/push_obj(Ljava/lang/Object;)V
    return
.end method
; ── mod — modulo (integer only; string/mixed → 0) ──────────────────────────────
.method public static mod()V
    .limit stack 4
    .limit locals 2
    invokestatic rt/SnoRt/pop_obj()Ljava/lang/Object;
    astore_0
    invokestatic rt/SnoRt/pop_obj()Ljava/lang/Object;
    astore_1
    aload_1
    checkcast java/lang/Long
    invokevirtual java/lang/Long/longValue()J
    aload_0
    checkcast java/lang/Long
    invokevirtual java/lang/Long/longValue()J
    lrem
    invokestatic java/lang/Long/valueOf(J)Ljava/lang/Long;
    invokestatic rt/SnoRt/push_obj(Ljava/lang/Object;)V
    return
.end method

; ── arith — arithmetic: op codes match SM_ARITH constants
; op: 0=add 1=sub 2=mul 3=div
; Defensive: if operands are not Long/Double, coerce them first.
.method public static arith(I)V
    .limit stack 8
    .limit locals 5
    invokestatic rt/SnoRt/pop_obj()Ljava/lang/Object;
    astore_1
    invokestatic rt/SnoRt/pop_obj()Ljava/lang/Object;
    astore_2
    ; coerce both operands to numeric
    aload_2
    instanceof java/lang/Long
    ifne arith_b_ok
    aload_2
    instanceof java/lang/Double
    ifne arith_b_ok
    aload_2
    invokestatic rt/SnoRt/push_obj(Ljava/lang/Object;)V
    invokestatic rt/SnoRt/coerce_num()V
    invokestatic rt/SnoRt/pop_obj()Ljava/lang/Object;
    astore_2
arith_b_ok:
    aload_1
    instanceof java/lang/Long
    ifne arith_a_ok
    aload_1
    instanceof java/lang/Double
    ifne arith_a_ok
    aload_1
    invokestatic rt/SnoRt/push_obj(Ljava/lang/Object;)V
    invokestatic rt/SnoRt/coerce_num()V
    invokestatic rt/SnoRt/pop_obj()Ljava/lang/Object;
    astore_1
arith_a_ok:
    ; if either is Double, use double arithmetic
    aload_1
    instanceof java/lang/Double
    ifne arith_double
    aload_2
    instanceof java/lang/Double
    ifne arith_double
    ; integer arithmetic — stack: [a_long, b_long, op]
    aload_2
    checkcast java/lang/Long
    invokevirtual java/lang/Long/longValue()J
    aload_1
    checkcast java/lang/Long
    invokevirtual java/lang/Long/longValue()J
    iload_0
    tableswitch 0 3
        arith_i_add
        arith_i_sub
        arith_i_mul
        arith_i_div
      default: arith_i_add
arith_i_add:
    ladd
    invokestatic java/lang/Long/valueOf(J)Ljava/lang/Long;
    invokestatic rt/SnoRt/push_obj(Ljava/lang/Object;)V
    return
arith_i_sub:
    lsub
    invokestatic java/lang/Long/valueOf(J)Ljava/lang/Long;
    invokestatic rt/SnoRt/push_obj(Ljava/lang/Object;)V
    return
arith_i_mul:
    lmul
    invokestatic java/lang/Long/valueOf(J)Ljava/lang/Long;
    invokestatic rt/SnoRt/push_obj(Ljava/lang/Object;)V
    return
arith_i_div:
    ldiv
    invokestatic java/lang/Long/valueOf(J)Ljava/lang/Long;
    invokestatic rt/SnoRt/push_obj(Ljava/lang/Object;)V
    return
arith_double:
    aload_2
    checkcast java/lang/Number
    invokevirtual java/lang/Number/doubleValue()D
    aload_1
    checkcast java/lang/Number
    invokevirtual java/lang/Number/doubleValue()D
    iload_0
    tableswitch 0 3
        arith_d_add
        arith_d_sub
        arith_d_mul
        arith_d_div
      default: arith_d_add
arith_d_add:
    dadd
    invokestatic java/lang/Double/valueOf(D)Ljava/lang/Double;
    invokestatic rt/SnoRt/push_obj(Ljava/lang/Object;)V
    return
arith_d_sub:
    dsub
    invokestatic java/lang/Double/valueOf(D)Ljava/lang/Double;
    invokestatic rt/SnoRt/push_obj(Ljava/lang/Object;)V
    return
arith_d_mul:
    dmul
    invokestatic java/lang/Double/valueOf(D)Ljava/lang/Double;
    invokestatic rt/SnoRt/push_obj(Ljava/lang/Object;)V
    return
arith_d_div:
    ddiv
    invokestatic java/lang/Double/valueOf(D)Ljava/lang/Double;
    invokestatic rt/SnoRt/push_obj(Ljava/lang/Object;)V
    return
.end method

; ── acomp — arithmetic comparison: pop b then a, push 1/0 (or set last_ok)
; op: 0=LT 1=LE 2=EQ 3=NE 4=GE 5=GT
; Defensive: coerce both operands to numeric first.
.method public static acomp(I)V
    .limit stack 8
    .limit locals 4
    invokestatic rt/SnoRt/pop_obj()Ljava/lang/Object;
    astore_1
    invokestatic rt/SnoRt/pop_obj()Ljava/lang/Object;
    astore_2
    ; coerce both operands
    aload_2
    instanceof java/lang/Number
    ifne acomp_b_ok
    aload_2
    invokestatic rt/SnoRt/push_obj(Ljava/lang/Object;)V
    invokestatic rt/SnoRt/coerce_num()V
    invokestatic rt/SnoRt/pop_obj()Ljava/lang/Object;
    astore_2
acomp_b_ok:
    aload_1
    instanceof java/lang/Number
    ifne acomp_a_ok
    aload_1
    invokestatic rt/SnoRt/push_obj(Ljava/lang/Object;)V
    invokestatic rt/SnoRt/coerce_num()V
    invokestatic rt/SnoRt/pop_obj()Ljava/lang/Object;
    astore_1
acomp_a_ok:
    aload_2
    checkcast java/lang/Number
    invokevirtual java/lang/Number/doubleValue()D
    aload_1
    checkcast java/lang/Number
    invokevirtual java/lang/Number/doubleValue()D
    dcmpl
    istore_3
    iload_0
    tableswitch 0 5
        acomp_lt
        acomp_le
        acomp_eq
        acomp_ne
        acomp_ge
        acomp_gt
      default: acomp_lt
acomp_lt:
    iload_3
    iflt acomp_true
    goto acomp_false
acomp_le:
    iload_3
    ifle acomp_true
    goto acomp_false
acomp_eq:
    iload_3
    ifeq acomp_true
    goto acomp_false
acomp_ne:
    iload_3
    ifne acomp_true
    goto acomp_false
acomp_ge:
    iload_3
    ifge acomp_true
    goto acomp_false
acomp_gt:
    iload_3
    ifgt acomp_true
    goto acomp_false
acomp_true:
    iconst_1
    putstatic rt/SnoRt/last_ok Z
    return
acomp_false:
    iconst_0
    putstatic rt/SnoRt/last_ok Z
    return
.end method

; ── lcomp — lexicographic (string) comparison
; op: 0=LLT 1=LLE 2=LEQ (IDENT) 3=LNE (DIFFER) 4=LGE 5=LGT
.method public static lcomp(I)V
    .limit stack 6
    .limit locals 4
    invokestatic rt/SnoRt/pop_obj()Ljava/lang/Object;
    astore_1
    invokestatic rt/SnoRt/pop_obj()Ljava/lang/Object;
    astore_2
    ; convert both to strings
    aload_2
    ifnonnull lcomp_a_nonnull
    ldc ""
    goto lcomp_a_done
lcomp_a_nonnull:
    aload_2
    invokevirtual java/lang/Object/toString()Ljava/lang/String;
lcomp_a_done:
    astore_2
    aload_1
    ifnonnull lcomp_b_nonnull
    ldc ""
    goto lcomp_b_done
lcomp_b_nonnull:
    aload_1
    invokevirtual java/lang/Object/toString()Ljava/lang/String;
lcomp_b_done:
    astore_1
    aload_2
    checkcast java/lang/String
    aload_1
    checkcast java/lang/String
    invokevirtual java/lang/String/compareTo(Ljava/lang/String;)I
    istore_3
    iload_0
    tableswitch 0 5
        lcomp_lt
        lcomp_le
        lcomp_eq
        lcomp_ne
        lcomp_ge
        lcomp_gt
      default: lcomp_lt
lcomp_lt:
    iload_3
    iflt lcomp_true
    goto lcomp_false
lcomp_le:
    iload_3
    ifle lcomp_true
    goto lcomp_false
lcomp_eq:
    iload_3
    ifeq lcomp_true
    goto lcomp_false
lcomp_ne:
    iload_3
    ifne lcomp_true
    goto lcomp_false
lcomp_ge:
    iload_3
    ifge lcomp_true
    goto lcomp_false
lcomp_gt:
    iload_3
    ifgt lcomp_true
    goto lcomp_false
lcomp_true:
    iconst_1
    putstatic rt/SnoRt/last_ok Z
    return
lcomp_false:
    iconst_0
    putstatic rt/SnoRt/last_ok Z
    return
.end method

; ── last_ok / set_last_ok ─────────────────────────────────────────────────────
.method public static last_ok()Z
    .limit stack 1
    .limit locals 0
    getstatic rt/SnoRt/last_ok Z
    ireturn
.end method

.method public static set_last_ok(Z)V
    .limit stack 1
    .limit locals 1
    iload_0
    putstatic rt/SnoRt/last_ok Z
    return
.end method

; ── set_stno — record statement number (for &STCOUNT) ────────────────────────
.method public static set_stno(J)V
    .limit stack 2
    .limit locals 2
    return
.end method

; ── halt_tos — print TOS as final value, exit 0 ──────────────────────────────
.method public static halt_tos()V
    .limit stack 3
    .limit locals 1
    invokestatic rt/SnoRt/pop_obj()Ljava/lang/Object;
    astore_0
    getstatic java/lang/System/out Ljava/io/PrintStream;
    aload_0
    ifnonnull halt_tos_nonnull
    ldc ""
    goto halt_tos_print
halt_tos_nonnull:
    aload_0
    invokevirtual java/lang/Object/toString()Ljava/lang/String;
halt_tos_print:
    invokevirtual java/io/PrintStream/println(Ljava/lang/String;)V
    return
.end method

; ── call — invoke a named SNOBOL4 built-in or user function ──────────────────
; Dispatches to built-in methods by name. Unknown names set last_ok=false.
; builtin_DEFINE: pop prototype string, store in fn_params keyed by fn name
.method private static builtin_DEFINE()V
    .limit stack 4
    .limit locals 3
    invokestatic rt/SnoRt/pop_obj()Ljava/lang/Object;
    astore_0
    aload_0
    ifnonnull define_notnull
    iconst_1
    putstatic rt/SnoRt/last_ok Z
    return
define_notnull:
    aload_0
    invokevirtual java/lang/Object/toString()Ljava/lang/String;
    astore_0
    bipush 40
    istore_1
    aload_0
    iload_1
    invokevirtual java/lang/String/indexOf(I)I
    istore_1
    iload_1
    ifge define_hasparen
    iconst_1
    putstatic rt/SnoRt/last_ok Z
    ldc ""
    invokestatic rt/SnoRt/push_obj(Ljava/lang/Object;)V
    return
define_hasparen:
    aload_0
    iconst_0
    iload_1
    invokevirtual java/lang/String/substring(II)Ljava/lang/String;
    astore_2
    getstatic rt/SnoRt/fn_params Ljava/util/HashMap;
    aload_2
    aload_0
    invokevirtual java/util/HashMap/put(Ljava/lang/Object;Ljava/lang/Object;)Ljava/lang/Object;
    pop
    iconst_1
    putstatic rt/SnoRt/last_ok Z
    ldc ""
    invokestatic rt/SnoRt/push_obj(Ljava/lang/Object;)V
    return
.end method

; bind_params: given fn name and nargs, bind stack values to param vars
; Reads prototype from fn_params, extracts param names, stores to vars.
.method public static bind_params(Ljava/lang/String;I)V
    .limit stack 8
    .limit locals 8
    getstatic rt/SnoRt/fn_params Ljava/util/HashMap;
    aload_0
    invokevirtual java/util/HashMap/get(Ljava/lang/Object;)Ljava/lang/Object;
    astore_2
    aload_2
    ifnonnull bind_have_proto
    iload_1
    istore_3
bind_pop_noreg:
    iload_3
    ifle bind_return
    invokestatic rt/SnoRt/pop_obj()Ljava/lang/Object;
    pop
    iinc 3 -1
    goto bind_pop_noreg
bind_have_proto:
    aload_2
    checkcast java/lang/String
    astore_2
    iload_1
    anewarray java/lang/Object
    astore_3
    iload_1
    iconst_1
    isub
    istore 4
bind_collect:
    iload 4
    iflt bind_bind
    aload_3
    iload 4
    invokestatic rt/SnoRt/pop_obj()Ljava/lang/Object;
    aastore
    iinc 4 -1
    goto bind_collect
bind_bind:
    aload_2
    bipush 40
    invokevirtual java/lang/String/indexOf(I)I
    iconst_1
    iadd
    istore 4
    iconst_0
    istore 5
bind_parse_loop:
    iload 5
    iload_1
    if_icmpge bind_return
    aload_2
    invokevirtual java/lang/String/length()I
    istore 6
    iload 4
    iload 6
    if_icmpge bind_return
    iload 4
    istore 7
bind_find_sep:
    iload 7
    iload 6
    if_icmpge bind_use_sep
    aload_2
    iload 7
    invokevirtual java/lang/String/charAt(I)C
    bipush 44
    if_icmpne bind_not_comma
    goto bind_use_sep
bind_not_comma:
    aload_2
    iload 7
    invokevirtual java/lang/String/charAt(I)C
    bipush 41
    if_icmpne bind_next_char
    goto bind_use_sep
bind_next_char:
    iinc 7 1
    goto bind_find_sep
bind_use_sep:
    getstatic rt/SnoRt/vars Ljava/util/HashMap;
    aload_2
    iload 4
    iload 7
    invokevirtual java/lang/String/substring(II)Ljava/lang/String;
    aload_3
    iload 5
    aaload
    invokevirtual java/util/HashMap/put(Ljava/lang/Object;Ljava/lang/Object;)Ljava/lang/Object;
    pop
    iload 7
    iconst_1
    iadd
    istore 4
    iinc 5 1
    goto bind_parse_loop
bind_return:
    aload_0
    putstatic rt/SnoRt/_ret_fn Ljava/lang/String;
    return
.end method

; fn_return_push: push the return value (vars[_ret_fn]) onto the value stack
.method public static fn_return_push()V
    .limit stack 3
    .limit locals 1
    getstatic rt/SnoRt/vars Ljava/util/HashMap;
    getstatic rt/SnoRt/_ret_fn Ljava/lang/String;
    astore_0
    aload_0
    ifnonnull fn_ret_have_name
    ldc ""
    invokestatic rt/SnoRt/push_obj(Ljava/lang/Object;)V
    return
fn_ret_have_name:
    getstatic rt/SnoRt/vars Ljava/util/HashMap;
    aload_0
    invokevirtual java/util/HashMap/get(Ljava/lang/Object;)Ljava/lang/Object;
    astore_0
    aload_0
    ifnonnull fn_ret_push_val
    ldc ""
    invokestatic rt/SnoRt/push_obj(Ljava/lang/Object;)V
    return
fn_ret_push_val:
    aload_0
    invokestatic rt/SnoRt/push_obj(Ljava/lang/Object;)V
    return
.end method

.method public static call(Ljava/lang/String;I)V
    .limit stack 4
    .limit locals 2
    aload_0
    ldc "DEFINE"
    invokevirtual java/lang/String/equals(Ljava/lang/Object;)Z
    ifeq call_not_DEFINE
    invokestatic rt/SnoRt/builtin_DEFINE()V
    return
call_not_DEFINE:
    aload_0
    ldc "SIZE"
    invokevirtual java/lang/String/equals(Ljava/lang/Object;)Z
    ifeq call_not_SIZE
    invokestatic rt/SnoRt/builtin_SIZE()V
    return
call_not_SIZE:
    aload_0
    ldc "TRIM"
    invokevirtual java/lang/String/equals(Ljava/lang/Object;)Z
    ifeq call_not_TRIM
    invokestatic rt/SnoRt/builtin_TRIM()V
    return
call_not_TRIM:
    aload_0
    ldc "DUPL"
    invokevirtual java/lang/String/equals(Ljava/lang/Object;)Z
    ifeq call_not_DUPL
    invokestatic rt/SnoRt/builtin_DUPL()V
    return
call_not_DUPL:
    aload_0
    ldc "SUBSTR"
    invokevirtual java/lang/String/equals(Ljava/lang/Object;)Z
    ifeq call_not_SUBSTR
    iload_1
    invokestatic rt/SnoRt/builtin_SUBSTR(I)V
    return
call_not_SUBSTR:
    aload_0
    ldc "IDENT"
    invokevirtual java/lang/String/equals(Ljava/lang/Object;)Z
    ifeq call_not_IDENT
    invokestatic rt/SnoRt/builtin_IDENT()V
    return
call_not_IDENT:
    aload_0
    ldc "DIFFER"
    invokevirtual java/lang/String/equals(Ljava/lang/Object;)Z
    ifeq call_not_DIFFER
    invokestatic rt/SnoRt/builtin_DIFFER()V
    return
call_not_DIFFER:
    aload_0
    ldc "INTEGER"
    invokevirtual java/lang/String/equals(Ljava/lang/Object;)Z
    ifeq call_not_INTEGER
    invokestatic rt/SnoRt/builtin_INTEGER()V
    return
call_not_INTEGER:
    aload_0
    ldc "DATATYPE"
    invokevirtual java/lang/String/equals(Ljava/lang/Object;)Z
    ifeq call_not_DATATYPE
    invokestatic rt/SnoRt/builtin_DATATYPE()V
    return
call_not_DATATYPE:
    aload_0
    ldc "LE"
    invokevirtual java/lang/String/equals(Ljava/lang/Object;)Z
    ifeq call_not_LE
    bipush 0
    invokestatic rt/SnoRt/builtin_numcmp(I)V
    return
call_not_LE:
    aload_0
    ldc "LT"
    invokevirtual java/lang/String/equals(Ljava/lang/Object;)Z
    ifeq call_not_LT
    bipush 1
    invokestatic rt/SnoRt/builtin_numcmp(I)V
    return
call_not_LT:
    aload_0
    ldc "GE"
    invokevirtual java/lang/String/equals(Ljava/lang/Object;)Z
    ifeq call_not_GE
    bipush 2
    invokestatic rt/SnoRt/builtin_numcmp(I)V
    return
call_not_GE:
    aload_0
    ldc "GT"
    invokevirtual java/lang/String/equals(Ljava/lang/Object;)Z
    ifeq call_not_GT
    bipush 3
    invokestatic rt/SnoRt/builtin_numcmp(I)V
    return
call_not_GT:
    aload_0
    ldc "EQ"
    invokevirtual java/lang/String/equals(Ljava/lang/Object;)Z
    ifeq call_not_EQ
    bipush 4
    invokestatic rt/SnoRt/builtin_numcmp(I)V
    return
call_not_EQ:
    aload_0
    ldc "NE"
    invokevirtual java/lang/String/equals(Ljava/lang/Object;)Z
    ifeq call_not_NE
    bipush 5
    invokestatic rt/SnoRt/builtin_numcmp(I)V
    return
call_not_NE:
    ; unknown function — pop nargs args, set last_ok=false, push ""
call_unknown_pop_loop:
    iload_1
    ifle call_unknown_done
    invokestatic rt/SnoRt/pop_obj()Ljava/lang/Object;
    pop
    iinc 1 -1
    goto call_unknown_pop_loop
call_unknown_done:
    iconst_0
    putstatic rt/SnoRt/last_ok Z
    ldc ""
    invokestatic rt/SnoRt/push_obj(Ljava/lang/Object;)V
    return
.end method

; ── numcmp — numeric comparison: 0=LE 1=LT 2=GE 3=GT 4=EQ 5=NE
; Pops B then A. Sets last_ok = (A cmp B). Pushes "" on success, FAILs on failure (last_ok=false).
.method private static builtin_numcmp(I)V
    .limit stack 6
    .limit locals 5
    ; param 0 = op
    ; Pop B → local 1, Pop A → local 2
    invokestatic rt/SnoRt/pop_obj()Ljava/lang/Object;
    astore_1
    invokestatic rt/SnoRt/pop_obj()Ljava/lang/Object;
    astore_2
    ; Compute A vs B comparison
    aload_2
    invokestatic rt/SnoRt/to_long(Ljava/lang/Object;)J
    aload_1
    invokestatic rt/SnoRt/to_long(Ljava/lang/Object;)J
    ; Stack: a (long), b (long) — lcmp gives a<b=-1, a==b=0, a>b=1
    lcmp
    ; Stack: [int cmp_result]
    istore_3
    ; Branch on op
    iload_0
    ifne numcmp_op_not_0
    iload_3
    ifgt numcmp_fail
    goto numcmp_ok
numcmp_op_not_0:
    iload_0
    iconst_1
    if_icmpne numcmp_op_not_1
    iload_3
    ifge numcmp_fail
    goto numcmp_ok
numcmp_op_not_1:
    iload_0
    iconst_2
    if_icmpne numcmp_op_not_2
    iload_3
    iflt numcmp_fail
    goto numcmp_ok
numcmp_op_not_2:
    iload_0
    iconst_3
    if_icmpne numcmp_op_not_3
    iload_3
    ifle numcmp_fail
    goto numcmp_ok
numcmp_op_not_3:
    iload_0
    iconst_4
    if_icmpne numcmp_op_not_4
    iload_3
    ifne numcmp_fail
    goto numcmp_ok
numcmp_op_not_4:
    iload_3
    ifeq numcmp_fail
    goto numcmp_ok
numcmp_ok:
    iconst_1
    putstatic rt/SnoRt/last_ok Z
    ldc ""
    invokestatic rt/SnoRt/push_obj(Ljava/lang/Object;)V
    return
numcmp_fail:
    iconst_0
    putstatic rt/SnoRt/last_ok Z
    ldc ""
    invokestatic rt/SnoRt/push_obj(Ljava/lang/Object;)V
    return
.end method

; ── to_long — coerce Object to long (Number → longValue, String → parse, null → 0)
.method private static to_long(Ljava/lang/Object;)J
    .limit stack 4
    .limit locals 3
    aload_0
    ifnonnull to_long_nonnull
    lconst_0
    lreturn
to_long_nonnull:
    aload_0
    instanceof java/lang/Number
    ifeq to_long_not_num
    aload_0
    checkcast java/lang/Number
    invokevirtual java/lang/Number/longValue()J
    lreturn
to_long_not_num:
    aload_0
    invokevirtual java/lang/Object/toString()Ljava/lang/String;
    invokestatic rt/SnoRt/try_parse_long(Ljava/lang/String;)Ljava/lang/Object;
    astore_1
    aload_1
    ifnonnull to_long_parsed
    lconst_0
    lreturn
to_long_parsed:
    aload_1
    checkcast java/lang/Long
    invokevirtual java/lang/Long/longValue()J
    lreturn
.end method

; ── do_return — function return (kind: 0=RETURN 1=FRETURN 2=NRETURN) ─────────
.method public static do_return(II)I
    .limit stack 2
    .limit locals 2
    ; kind==1 (FRETURN) → last_ok = false
    iload_0
    iconst_1
    if_icmpne do_return_ok
    iconst_0
    putstatic rt/SnoRt/last_ok Z
    iconst_0
    ireturn
do_return_ok:
    iconst_1
    putstatic rt/SnoRt/last_ok Z
    iconst_1
    ireturn
.end method

; ── push_ret_pc — push a return PC (int) onto the call stack ─────────────────
.method public static push_ret_pc(I)V
    .limit stack 4
    .limit locals 1
    getstatic rt/SnoRt/ret_stack Ljava/util/ArrayDeque;
    iload_0
    invokestatic java/lang/Integer/valueOf(I)Ljava/lang/Integer;
    invokevirtual java/util/ArrayDeque/push(Ljava/lang/Object;)V
    return
.end method

; ── pop_ret_pc — pop a return PC (int) from the call stack ───────────────────
.method public static pop_ret_pc()I
    .limit stack 2
    .limit locals 1
    getstatic rt/SnoRt/ret_stack Ljava/util/ArrayDeque;
    invokevirtual java/util/ArrayDeque/pop()Ljava/lang/Object;
    checkcast java/lang/Integer
    invokevirtual java/lang/Integer/intValue()I
    ireturn
.end method

; ── built-in functions ────────────────────────────────────────────────────────

; SIZE(s) → length of string
.method private static builtin_SIZE()V
    .limit stack 4
    .limit locals 1
    invokestatic rt/SnoRt/pop_obj()Ljava/lang/Object;
    astore_0
    aload_0
    ifnonnull size_nonnull
    lconst_0
    invokestatic java/lang/Long/valueOf(J)Ljava/lang/Long;
    invokestatic rt/SnoRt/push_obj(Ljava/lang/Object;)V
    return
size_nonnull:
    aload_0
    invokevirtual java/lang/Object/toString()Ljava/lang/String;
    invokevirtual java/lang/String/length()I
    i2l
    invokestatic java/lang/Long/valueOf(J)Ljava/lang/Long;
    invokestatic rt/SnoRt/push_obj(Ljava/lang/Object;)V
    return
.end method

; TRIM(s) → trailing-space trimmed string
.method private static builtin_TRIM()V
    .limit stack 3
    .limit locals 3
    invokestatic rt/SnoRt/pop_obj()Ljava/lang/Object;
    astore_0
    aload_0
    ifnonnull trim_nonnull
    ldc ""
    invokestatic rt/SnoRt/push_obj(Ljava/lang/Object;)V
    return
trim_nonnull:
    aload_0
    invokevirtual java/lang/Object/toString()Ljava/lang/String;
    astore_1
    ; trim trailing spaces manually
    aload_1
    invokevirtual java/lang/String/length()I
    istore 2
trim_loop:
    iload 2
    ifle trim_done
    aload_1
    iload 2
    iconst_1
    isub
    invokevirtual java/lang/String/charAt(I)C
    bipush 32
    if_icmpne trim_done
    iinc 2 -1
    goto trim_loop
trim_done:
    aload_1
    iconst_0
    iload 2
    invokevirtual java/lang/String/substring(II)Ljava/lang/String;
    invokestatic rt/SnoRt/push_obj(Ljava/lang/Object;)V
    return
.end method

; DUPL(s, n) → s repeated n times
.method private static builtin_DUPL()V
    .limit stack 6
    .limit locals 4
    invokestatic rt/SnoRt/pop_obj()Ljava/lang/Object;
    astore_0
    invokestatic rt/SnoRt/pop_obj()Ljava/lang/Object;
    astore_1
    aload_0
    checkcast java/lang/Number
    invokevirtual java/lang/Number/intValue()I
    istore_2
    new java/lang/StringBuilder
    dup
    invokespecial java/lang/StringBuilder/<init>()V
    astore_3
dupl_loop:
    iload_2
    ifle dupl_done
    aload_3
    aload_1
    ifnonnull dupl_str_nonnull
    ldc ""
    goto dupl_append
dupl_str_nonnull:
    aload_1
    invokevirtual java/lang/Object/toString()Ljava/lang/String;
dupl_append:
    invokevirtual java/lang/StringBuilder/append(Ljava/lang/String;)Ljava/lang/StringBuilder;
    pop
    iinc 2 -1
    goto dupl_loop
dupl_done:
    aload_3
    invokevirtual java/lang/StringBuilder/toString()Ljava/lang/String;
    invokestatic rt/SnoRt/push_obj(Ljava/lang/Object;)V
    return
.end method

; SUBSTR(s, i, n) → substring (1-based, SNOBOL4 convention)
.method private static builtin_SUBSTR(I)V
    .limit stack 6
    .limit locals 7
    ; Stack: if nargs=3: N (top), POS, STR  ;  if nargs=2: POS (top), STR — N defaults to remaining
    ; SNOBOL4 semantics: SUBSTR(STR, POS [, N]) returns substring or fails (last_ok=false)
    ; Param 0 is nargs (int). Locals: 1=nargs, 3=N, 4=POS, 5=STR, 6=str_len
    iload_0
    istore_1
    ; If nargs >= 3, pop N from top; else N = -1 (sentinel meaning "to end")
    iload_1
    iconst_3
    if_icmplt substr_2arg
    invokestatic rt/SnoRt/pop_obj()Ljava/lang/Object;
    astore_2
    aload_2
    instanceof java/lang/Number
    ifeq substr_fail
    aload_2
    checkcast java/lang/Number
    invokevirtual java/lang/Number/intValue()I
    istore_3
    goto substr_have_n
substr_2arg:
    iconst_m1
    istore_3
substr_have_n:
    ; Pop POS
    invokestatic rt/SnoRt/pop_obj()Ljava/lang/Object;
    astore_2
    aload_2
    instanceof java/lang/Number
    ifeq substr_fail
    aload_2
    checkcast java/lang/Number
    invokevirtual java/lang/Number/intValue()I
    istore 4
    ; Pop STR
    invokestatic rt/SnoRt/pop_obj()Ljava/lang/Object;
    astore_2
    aload_2
    ifnonnull substr_str_nonnull
    ldc ""
    astore 5
    goto substr_str_done
substr_str_nonnull:
    aload_2
    invokevirtual java/lang/Object/toString()Ljava/lang/String;
    astore 5
substr_str_done:
    ; Get string length → local 6
    aload 5
    invokevirtual java/lang/String/length()I
    istore 6
    ; If N == -1 (2-arg case), N = length - (POS-1) = length - POS + 1
    iload_3
    iconst_m1
    if_icmpne substr_have_explicit_n
    iload 6
    iload 4
    isub
    iconst_1
    iadd
    istore_3
substr_have_explicit_n:
    ; Bounds check: POS <= 0 → fail
    iload 4
    ifle substr_fail
    ; Bounds check: N < 0 → fail
    iload_3
    iflt substr_fail
    ; Bounds check: POS-1 > size(STR) → fail
    iload 4
    iconst_1
    isub
    iload 6
    if_icmpgt substr_fail
    ; Bounds check: POS-1+N > size(STR) → fail
    iload 4
    iconst_1
    isub
    iload_3
    iadd
    iload 6
    if_icmpgt substr_fail
    ; Valid: extract substring(POS-1, POS-1+N)
    aload 5
    iload 4
    iconst_1
    isub
    iload 4
    iconst_1
    isub
    iload_3
    iadd
    invokevirtual java/lang/String/substring(II)Ljava/lang/String;
    invokestatic rt/SnoRt/push_obj(Ljava/lang/Object;)V
    iconst_1
    putstatic rt/SnoRt/last_ok Z
    return
substr_fail:
    ldc ""
    invokestatic rt/SnoRt/push_obj(Ljava/lang/Object;)V
    iconst_0
    putstatic rt/SnoRt/last_ok Z
    return
.end method

; IDENT(a, b) → set last_ok = (a equals b as strings)
.method private static builtin_IDENT()V
    .limit stack 4
    .limit locals 3
    invokestatic rt/SnoRt/pop_obj()Ljava/lang/Object;
    astore_0
    invokestatic rt/SnoRt/pop_obj()Ljava/lang/Object;
    astore_1
    aload_0
    ifnonnull ident_b_nonnull
    ldc ""
    astore_0
    goto ident_b_done
ident_b_nonnull:
    aload_0
    invokevirtual java/lang/Object/toString()Ljava/lang/String;
    astore_0
ident_b_done:
    aload_1
    ifnonnull ident_a_nonnull
    ldc ""
    astore_1
    goto ident_a_done
ident_a_nonnull:
    aload_1
    invokevirtual java/lang/Object/toString()Ljava/lang/String;
    astore_1
ident_a_done:
    aload_1
    checkcast java/lang/String
    aload_0
    checkcast java/lang/String
    invokevirtual java/lang/String/equals(Ljava/lang/Object;)Z
    ifeq ident_false
    iconst_1
    putstatic rt/SnoRt/last_ok Z
    aload_1
    invokestatic rt/SnoRt/push_obj(Ljava/lang/Object;)V
    return
ident_false:
    iconst_0
    putstatic rt/SnoRt/last_ok Z
    ldc ""
    invokestatic rt/SnoRt/push_obj(Ljava/lang/Object;)V
    return
.end method

; DIFFER(a, b) → set last_ok = (a not equal b)
.method private static builtin_DIFFER()V
    .limit stack 4
    .limit locals 3
    invokestatic rt/SnoRt/builtin_IDENT()V
    ; flip last_ok
    getstatic rt/SnoRt/last_ok Z
    ifeq differ_set_true
    iconst_0
    putstatic rt/SnoRt/last_ok Z
    return
differ_set_true:
    iconst_1
    putstatic rt/SnoRt/last_ok Z
    return
.end method

; INTEGER(x) → succeeds if x is an integer, fails otherwise
.method private static builtin_INTEGER()V
    .limit stack 4
    .limit locals 2
    invokestatic rt/SnoRt/pop_obj()Ljava/lang/Object;
    astore_0
    aload_0
    instanceof java/lang/Long
    ifne integer_ok
    aload_0
    ifnull integer_fail
    aload_0
    invokevirtual java/lang/Object/toString()Ljava/lang/String;
    astore_1
    aload_1
    invokestatic java/lang/Long/parseLong(Ljava/lang/String;)J
    pop2
    aload_0
    invokestatic rt/SnoRt/push_obj(Ljava/lang/Object;)V
    iconst_1
    putstatic rt/SnoRt/last_ok Z
    return
integer_ok:
    aload_0
    invokestatic rt/SnoRt/push_obj(Ljava/lang/Object;)V
    iconst_1
    putstatic rt/SnoRt/last_ok Z
    return
integer_fail:
    ldc ""
    invokestatic rt/SnoRt/push_obj(Ljava/lang/Object;)V
    iconst_0
    putstatic rt/SnoRt/last_ok Z
    return
.end method

; DATATYPE(x) → push type name string ("INTEGER", "REAL", "STRING", "PATTERN")
.method private static builtin_DATATYPE()V
    .limit stack 3
    .limit locals 1
    invokestatic rt/SnoRt/pop_obj()Ljava/lang/Object;
    astore_0
    aload_0
    instanceof java/lang/Long
    ifeq dt_not_long
    ldc "INTEGER"
    invokestatic rt/SnoRt/push_obj(Ljava/lang/Object;)V
    return
dt_not_long:
    aload_0
    instanceof java/lang/Double
    ifeq dt_not_double
    ldc "REAL"
    invokestatic rt/SnoRt/push_obj(Ljava/lang/Object;)V
    return
dt_not_double:
    ldc "STRING"
    invokestatic rt/SnoRt/push_obj(Ljava/lang/Object;)V
    return
.end method

; ── MatchState inner class ────────────────────────────────────────────────────

