;; sno_runtime.wat — SNOBOL4 WASM scalar stack-machine runtime
;; Implements SM-level operations called from emitted $main functions.
;; Memory layout (32 pages = 2MB total):
;;   [0x000000..0x00FFFF]  value stack       (16-byte slots, STACK_BASE=0, 4096 slots)
;;   [0x010000..0x013FFF]  variable table    (512 slots × 32 bytes = 16KB, VAR_BASE=0x10000)
;;   [0x031000..0x031FFF]  runtime keywords  (INPUT @ 0x31000, OUTPUT @ 0x31010)
;;   [0x040000..0x04FFFF]  output buffer     (OUTPUT_BUF=0x40000, unused so far)
;;   [0x050000..0x05FFFF]  BB arena          (32-byte box slots, BOX_ARENA_BASE=0x50000)
;;   [0x070000..0x077FFF]  call stack        (32-byte frames, CALL_STACK_BASE=0x70000, 1024 frames)
;;   [0x078000..0x07FFFF]  saved bindings    (20-byte entries, SAVED_VARS_BASE=0x78000, bump)
;;   [0x080000..0x0FFFFF]  dynamic str heap  (bump alloc from STR_HEAP_BASE=0x80000, 512KB)
;;   [0x100000..0x1FFFFF]  emitter literals  (STR_DATA_BASE=0x100000 in emit_wasm.c, 1MB)
;; Stack slot layout (16 bytes): +0=tag(i32) +4=ival(i32) +8=len(i32) +12=pad(i32)
;; Type tags: 0=integer 1=string 2=real 3=null 4=fail
;; Authors: Claude Sonnet 4.6 (SN4-WASM-1), Claude Opus 4.7 (SN4-WASM-5 prep)

(module
  ;; imports MUST come before memory/globals in WASM
  (import "host" "write_line" (func $host_write_line (param i32 i32)))
  (import "host" "read_line"  (func $host_read_line  (param i32) (result i32)))

  (memory (export "memory") 32)

  ;; constants
  (global $TAG_INT   i32 (i32.const 0))
  (global $TAG_STR   i32 (i32.const 1))
  (global $TAG_REAL  i32 (i32.const 2))
  (global $TAG_NULL  i32 (i32.const 3))
  (global $TAG_FAIL  i32 (i32.const 4))
  (global $SLOT      i32 (i32.const 16))
  (global $VAR_SLOTS i32 (i32.const 512))
  (global $VAR_SZ    i32 (i32.const 32))

  ;; mutable globals
  (global $sp       (mut i32) (i32.const 0))        ;; stack pointer (byte offset, STACK_BASE=0)
  (global $str_ptr  (mut i32) (i32.const 0x80000))  ;; dynamic string heap bump ptr (above arena at 0x50000..0x5FFFF)
  (global $last_ok  (mut i32) (i32.const 1))        ;; last match result
  (global $stno     (mut i32) (i32.const 0))        ;; statement number
  (global $pop_tag  (mut i32) (i32.const 0))        ;; pop result staging
  (global $pop_ival (mut i32) (i32.const 0))
  (global $pop_len  (mut i32) (i32.const 0))
  (global $call_sp  (mut i32) (i32.const 0x70000))  ;; call-stack pointer (next free frame addr)
  (global $sv_top   (mut i32) (i32.const 0x78000))  ;; saved-vars region bump ptr

  ;; static keyword strings at known addresses
  (data (i32.const 0x31000) "INPUT")
  (data (i32.const 0x31010) "OUTPUT")
  ;; built-in names for $sno_call dispatch (each entry's address is the i32.const used in sno_call)
  (data (i32.const 0x31100) "LT")
  (data (i32.const 0x31103) "GT")
  (data (i32.const 0x31106) "EQ")
  (data (i32.const 0x31109) "NE")
  (data (i32.const 0x3110c) "GE")
  (data (i32.const 0x3110f) "LE")
  (data (i32.const 0x31112) "IDENT")
  (data (i32.const 0x31118) "DIFFER")
  ;; 1-arg builtins
  (data (i32.const 0x31130) "SIZE")
  (data (i32.const 0x31135) "INTEGER")
  (data (i32.const 0x3113d) "STRING")
  (data (i32.const 0x31144) "TRIM")
  (data (i32.const 0x31149) "REVERSE")
  (data (i32.const 0x31151) "DATATYPE")
  (data (i32.const 0x3115a) "CHAR")
  (data (i32.const 0x3115f) "ORD")
  (data (i32.const 0x31163) "CONVERT")
  ;; 2-arg builtins (in addition to LT/GT/EQ/NE/GE/LE/IDENT/DIFFER above)
  (data (i32.const 0x3116b) "DUPL")
  (data (i32.const 0x31170) "REPLACE")
  (data (i32.const 0x31178) "REMDR")
  ;; 3-arg builtins
  (data (i32.const 0x3117e) "SUBSTR")
  ;; keyword names (without leading &) at known addresses for $sno_push_var fast-path
  (data (i32.const 0x31200) "MAXINT")
  (data (i32.const 0x31208) "STCOUNT")
  (data (i32.const 0x31210) "STLIMIT")
  (data (i32.const 0x31218) "MAXLNGTH")
  (data (i32.const 0x31220) "TRIM")
  (data (i32.const 0x31228) "ERRLIMIT")
  (data (i32.const 0x31230) "ALPHABET")
  (data (i32.const 0x31238) "DIGITS")
  (data (i32.const 0x31240) "UCASE")
  (data (i32.const 0x31248) "LCASE")
  ;; Keyword VALUES (string contents) at separate addresses for push_var fast-path.
  ;; ALPHABET: 256 bytes of \\00..\\FF; emitted byte-by-byte via \\xx escapes.
  (data (i32.const 0x31300)
    "\00\01\02\03\04\05\06\07\08\09\0a\0b\0c\0d\0e\0f"
    "\10\11\12\13\14\15\16\17\18\19\1a\1b\1c\1d\1e\1f"
    " !\22#$%&'()*+,-./0123456789:;<=>?"
    "@ABCDEFGHIJKLMNOPQRSTUVWXYZ[\5c]^_"
    "`abcdefghijklmnopqrstuvwxyz{|}~\7f"
    "\80\81\82\83\84\85\86\87\88\89\8a\8b\8c\8d\8e\8f"
    "\90\91\92\93\94\95\96\97\98\99\9a\9b\9c\9d\9e\9f"
    "\a0\a1\a2\a3\a4\a5\a6\a7\a8\a9\aa\ab\ac\ad\ae\af"
    "\b0\b1\b2\b3\b4\b5\b6\b7\b8\b9\ba\bb\bc\bd\be\bf"
    "\c0\c1\c2\c3\c4\c5\c6\c7\c8\c9\ca\cb\cc\cd\ce\cf"
    "\d0\d1\d2\d3\d4\d5\d6\d7\d8\d9\da\db\dc\dd\de\df"
    "\e0\e1\e2\e3\e4\e5\e6\e7\e8\e9\ea\eb\ec\ed\ee\ef"
    "\f0\f1\f2\f3\f4\f5\f6\f7\f8\f9\fa\fb\fc\fd\fe\ff")
  ;; DIGITS, UCASE, LCASE values
  (data (i32.const 0x31400) "0123456789")
  (data (i32.const 0x31410) "ABCDEFGHIJKLMNOPQRSTUVWXYZ")
  (data (i32.const 0x31430) "abcdefghijklmnopqrstuvwxyz")

  ;; ── memory helpers ───────────────────────────────────────────────────────
  (func $memcpy (param $dst i32) (param $src i32) (param $len i32)
    (local $i i32)
    (local.set $i (i32.const 0))
    (block $B (loop $L
      (br_if $B (i32.ge_u (local.get $i) (local.get $len)))
      (i32.store8
        (i32.add (local.get $dst) (local.get $i))
        (i32.load8_u (i32.add (local.get $src) (local.get $i))))
      (local.set $i (i32.add (local.get $i) (i32.const 1)))
      (br $L)
    ))
  )

  (func $str_eq (param $p1 i32) (param $l1 i32) (param $p2 i32) (param $l2 i32) (result i32)
    (local $i i32)
    (if (i32.ne (local.get $l1) (local.get $l2)) (then (return (i32.const 0))))
    (local.set $i (i32.const 0))
    (block $B (loop $L
      (br_if $B (i32.ge_u (local.get $i) (local.get $l1)))
      (if (i32.ne
            (i32.load8_u (i32.add (local.get $p1) (local.get $i)))
            (i32.load8_u (i32.add (local.get $p2) (local.get $i))))
        (then (return (i32.const 0))))
      (local.set $i (i32.add (local.get $i) (i32.const 1)))
      (br $L)
    ))
    (i32.const 1)
  )

  (func $alloc_str (param $len i32) (result i32)
    (local $ptr i32)
    (local.set $ptr (global.get $str_ptr))
    (global.set $str_ptr (i32.add (local.get $ptr) (i32.add (local.get $len) (i32.const 1))))
    (local.get $ptr)
  )

  ;; int_to_str: write decimal of val (i32) into heap; return ptr,len
  (func $int_to_str (param $val i32) (result i32 i32)
    (local $ptr i32) (local $start i32) (local $end i32)
    (local $tmp i32) (local $neg i32) (local $v i32) (local $digit i32) (local $len i32)
    (local.set $ptr (global.get $str_ptr))
    (local.set $start (local.get $ptr))
    (local.set $neg (i32.const 0))
    (local.set $v (local.get $val))
    (if (i32.lt_s (local.get $v) (i32.const 0))
      (then
        (local.set $neg (i32.const 1))
        (local.set $v (i32.sub (i32.const 0) (local.get $v)))))
    (if (i32.eqz (local.get $v))
      (then
        (i32.store8 (local.get $ptr) (i32.const 48))
        (local.set $ptr (i32.add (local.get $ptr) (i32.const 1))))
      (else
        (block $B (loop $L
          (br_if $B (i32.eqz (local.get $v)))
          (local.set $digit (i32.rem_u (local.get $v) (i32.const 10)))
          (i32.store8 (local.get $ptr) (i32.add (local.get $digit) (i32.const 48)))
          (local.set $ptr (i32.add (local.get $ptr) (i32.const 1)))
          (local.set $v (i32.div_u (local.get $v) (i32.const 10)))
          (br $L)
        ))
        (if (local.get $neg)
          (then
            (i32.store8 (local.get $ptr) (i32.const 45))
            (local.set $ptr (i32.add (local.get $ptr) (i32.const 1)))))
        ;; reverse
        (local.set $end (i32.sub (local.get $ptr) (i32.const 1)))
        (local.set $tmp (local.get $start))
        (block $RB (loop $RL
          (br_if $RB (i32.ge_u (local.get $tmp) (local.get $end)))
          (local.set $digit (i32.load8_u (local.get $tmp)))
          (i32.store8 (local.get $tmp) (i32.load8_u (local.get $end)))
          (i32.store8 (local.get $end) (local.get $digit))
          (local.set $tmp (i32.add (local.get $tmp) (i32.const 1)))
          (local.set $end (i32.sub (local.get $end) (i32.const 1)))
          (br $RL)
        ))
      )
    )
    (local.set $len (i32.sub (local.get $ptr) (local.get $start)))
    (global.set $str_ptr (i32.add (local.get $ptr) (i32.const 1)))
    (local.get $start)
    (local.get $len)
  )

  ;; ── stack helpers ────────────────────────────────────────────────────────
  (func $push3 (param $tag i32) (param $ival i32) (param $len i32)
    (local $addr i32)
    (local.set $addr (global.get $sp))
    (i32.store                            (local.get $addr) (local.get $tag))
    (i32.store (i32.add (local.get $addr) (i32.const 4)) (local.get $ival))
    (i32.store (i32.add (local.get $addr) (i32.const 8)) (local.get $len))
    (i32.store (i32.add (local.get $addr) (i32.const 12)) (i32.const 0))
    (global.set $sp (i32.add (global.get $sp) (i32.const 16)))
  )

  (func $peek_tag  (result i32) (i32.load (i32.sub (global.get $sp) (i32.const 16))))
  (func $peek_ival (result i32) (i32.load (i32.add (i32.sub (global.get $sp) (i32.const 16)) (i32.const 4))))
  (func $peek_len  (result i32) (i32.load (i32.add (i32.sub (global.get $sp) (i32.const 16)) (i32.const 8))))

  (func $pop_slot
    (local $addr i32)
    (global.set $sp (i32.sub (global.get $sp) (i32.const 16)))
    (local.set $addr (global.get $sp))
    (global.set $pop_tag  (i32.load (local.get $addr)))
    (global.set $pop_ival (i32.load (i32.add (local.get $addr) (i32.const 4))))
    (global.set $pop_len  (i32.load (i32.add (local.get $addr) (i32.const 8))))
  )

  ;; coerce top-of-stack to string (ptr,len); does NOT pop
  (func $tos_to_str (result i32 i32)
    (local $tag i32) (local $ival i32) (local $len i32)
    (local.set $tag  (call $peek_tag))
    (local.set $ival (call $peek_ival))
    (local.set $len  (call $peek_len))
    (if (i32.eq (local.get $tag) (global.get $TAG_STR))
      (then (return (local.get $ival) (local.get $len))))
    (if (i32.eq (local.get $tag) (global.get $TAG_NULL))
      (then (return (i32.const 0) (i32.const 0))))
    (if (i32.eq (local.get $tag) (global.get $TAG_INT))
      (then (return (call $int_to_str (local.get $ival)))))
    (return (i32.const 0) (i32.const 0))
  )

  ;; ── variable table (open-address hash, 256 slots × 32 bytes) ─────────────
  ;; Slot layout: +0=name_ptr(4) +4=name_len(4) +8=tag(4) +12=ival(4) +16=len(4) +20=pad(12)
  (func $var_hash (param $ptr i32) (param $len i32) (result i32)
    (local $h i32) (local $i i32)
    (local.set $h (i32.const 5381))
    (local.set $i (i32.const 0))
    (block $B (loop $L
      (br_if $B (i32.ge_u (local.get $i) (local.get $len)))
      (local.set $h (i32.add
        (i32.add (i32.shl (local.get $h) (i32.const 5)) (local.get $h))
        (i32.load8_u (i32.add (local.get $ptr) (local.get $i)))))
      (local.set $i (i32.add (local.get $i) (i32.const 1)))
      (br $L)
    ))
    (i32.rem_u (i32.and (local.get $h) (i32.const 0x7fffffff)) (i32.const 512))
  )

  (func $var_slot_addr (param $name_ptr i32) (param $name_len i32) (result i32)
    (local $h i32) (local $idx i32) (local $saddr i32) (local $snl i32) (local $snp i32)
    (local.set $h (call $var_hash (local.get $name_ptr) (local.get $name_len)))
    (local.set $idx (local.get $h))
    (block $found (loop $probe
      (local.set $saddr (i32.add (i32.const 0x10000) (i32.mul (local.get $idx) (i32.const 32))))
      (local.set $snl (i32.load (i32.add (local.get $saddr) (i32.const 4))))
      (br_if $found (i32.eqz (local.get $snl)))
      (local.set $snp (i32.load (local.get $saddr)))
      (br_if $found (call $str_eq
        (local.get $name_ptr) (local.get $name_len) (local.get $snp) (local.get $snl)))
      (local.set $idx (i32.rem_u (i32.add (local.get $idx) (i32.const 1)) (i32.const 512)))
      (br_if $probe (i32.ne (local.get $idx) (local.get $h)))
    ))
    (i32.add (i32.const 0x10000) (i32.mul (local.get $idx) (i32.const 32)))
  )

  ;; ── exported runtime functions ────────────────────────────────────────────
  (func $sno_init (export "sno_init")
    (local $i i32)
    (global.set $sp (i32.const 0))
    (global.set $str_ptr (i32.const 0x80000))
    (global.set $last_ok (i32.const 1))
    (global.set $stno (i32.const 0))
    (local.set $i (i32.const 0))
    (block $B (loop $L
      (br_if $B (i32.ge_u (local.get $i) (i32.const 0x4000)))
      (i32.store (i32.add (i32.const 0x10000) (local.get $i)) (i32.const 0))
      (local.set $i (i32.add (local.get $i) (i32.const 4)))
      (br $L)
    ))
  )

  (func $sno_finalize (export "sno_finalize"))

  (func $sno_push_int (export "sno_push_int") (param $v i32)
    (call $push3 (global.get $TAG_INT) (local.get $v) (i32.const 0))
  )

  (func $sno_push_str (export "sno_push_str") (param $ptr i32) (param $len i32)
    (call $push3 (global.get $TAG_STR) (local.get $ptr) (local.get $len))
  )

  (func $sno_push_real (export "sno_push_real") (param $v f64)
    (local $bits i64)
    (local.set $bits (i64.reinterpret_f64 (local.get $v)))
    (call $push3
      (global.get $TAG_REAL)
      (i32.wrap_i64 (local.get $bits))
      (i32.wrap_i64 (i64.shr_u (local.get $bits) (i64.const 32))))
  )

  (func $sno_push_null (export "sno_push_null")
    (call $push3 (global.get $TAG_NULL) (i32.const 0) (i32.const 0))
  )

  (func $sno_push_var (export "sno_push_var") (param $name_ptr i32) (param $name_len i32)
    (local $saddr i32) (local $snl i32) (local $tag i32) (local $ival i32) (local $len i32)
    ;; Keyword fast-path: known &KW names push their well-known values directly.
    ;; (The emitter doesn't strip the leading &; it passes the name unchanged.  But
    ;; the lowering convention is that the SM only ever stores into the keyword
    ;; namespace via &KW assignments, so a plain push_var "MAXINT" comes ONLY from
    ;; &MAXINT.  This won't conflict with a user var named MAXINT in normal code.)
    (if (call $str_eq (local.get $name_ptr) (local.get $name_len) (i32.const 0x31200) (i32.const 6))
      (then (call $push3 (global.get $TAG_INT) (i32.const 2147483647) (i32.const 0)) (return)))
    (if (call $str_eq (local.get $name_ptr) (local.get $name_len) (i32.const 0x31218) (i32.const 8))
      (then (call $push3 (global.get $TAG_INT) (i32.const 5000) (i32.const 0)) (return)))
    ;; STCOUNT, STLIMIT, ERRLIMIT, TRIM: defaults of 0/-1/0/0; emit as int 0/-1
    (if (call $str_eq (local.get $name_ptr) (local.get $name_len) (i32.const 0x31208) (i32.const 7))
      (then (call $push3 (global.get $TAG_INT) (global.get $stno) (i32.const 0)) (return)))
    (if (call $str_eq (local.get $name_ptr) (local.get $name_len) (i32.const 0x31210) (i32.const 7))
      (then (call $push3 (global.get $TAG_INT) (i32.const -1) (i32.const 0)) (return)))
    (if (call $str_eq (local.get $name_ptr) (local.get $name_len) (i32.const 0x31228) (i32.const 8))
      (then (call $push3 (global.get $TAG_INT) (i32.const 0) (i32.const 0)) (return)))
    (if (call $str_eq (local.get $name_ptr) (local.get $name_len) (i32.const 0x31220) (i32.const 4))
      (then (call $push3 (global.get $TAG_INT) (i32.const 0) (i32.const 0)) (return)))
    ;; ALPHABET (8 chars): 256-byte string of \00..\FF
    (if (call $str_eq (local.get $name_ptr) (local.get $name_len) (i32.const 0x31230) (i32.const 8))
      (then (call $push3 (global.get $TAG_STR) (i32.const 0x31300) (i32.const 256)) (return)))
    ;; DIGITS (6 chars): "0123456789"
    (if (call $str_eq (local.get $name_ptr) (local.get $name_len) (i32.const 0x31238) (i32.const 6))
      (then (call $push3 (global.get $TAG_STR) (i32.const 0x31400) (i32.const 10)) (return)))
    ;; UCASE (5 chars): "A".."Z"
    (if (call $str_eq (local.get $name_ptr) (local.get $name_len) (i32.const 0x31240) (i32.const 5))
      (then (call $push3 (global.get $TAG_STR) (i32.const 0x31410) (i32.const 26)) (return)))
    ;; LCASE (5 chars): "a".."z"
    (if (call $str_eq (local.get $name_ptr) (local.get $name_len) (i32.const 0x31248) (i32.const 5))
      (then (call $push3 (global.get $TAG_STR) (i32.const 0x31430) (i32.const 26)) (return)))
    (local.set $saddr (call $var_slot_addr (local.get $name_ptr) (local.get $name_len)))
    (local.set $snl (i32.load (i32.add (local.get $saddr) (i32.const 4))))
    (if (i32.eqz (local.get $snl))
      (then
        (call $push3 (global.get $TAG_STR) (i32.const 0) (i32.const 0))
        (return)))
    (local.set $tag  (i32.load (i32.add (local.get $saddr) (i32.const 8))))
    (local.set $ival (i32.load (i32.add (local.get $saddr) (i32.const 12))))
    (local.set $len  (i32.load (i32.add (local.get $saddr) (i32.const 16))))
    (call $push3 (local.get $tag) (local.get $ival) (local.get $len))
  )

  (func $sno_store_var (export "sno_store_var") (param $name_ptr i32) (param $name_len i32)
    (local $saddr i32) (local $tag i32) (local $ival i32) (local $len i32)
    (local $p i32) (local $l i32)
    (call $pop_slot)
    (local.set $tag  (global.get $pop_tag))
    (local.set $ival (global.get $pop_ival))
    (local.set $len  (global.get $pop_len))
    ;; FAIL value: do not store, do not print; just set last_ok=0.
    (if (i32.eq (local.get $tag) (global.get $TAG_FAIL))
      (then (global.set $last_ok (i32.const 0)) (return)))
    ;; check for OUTPUT (6 chars)
    (if (call $str_eq (local.get $name_ptr) (local.get $name_len) (i32.const 0x31010) (i32.const 6))
      (then
        (call $push3 (local.get $tag) (local.get $ival) (local.get $len))
        (call $tos_to_str)
        (local.set $l)
        (local.set $p)
        (global.set $sp (i32.sub (global.get $sp) (i32.const 16)))
        (call $host_write_line (local.get $p) (local.get $l))
        (global.set $last_ok (i32.const 1))
        (return)))
    (local.set $saddr (call $var_slot_addr (local.get $name_ptr) (local.get $name_len)))
    (i32.store (local.get $saddr) (local.get $name_ptr))
    (i32.store (i32.add (local.get $saddr) (i32.const 4)) (local.get $name_len))
    (i32.store (i32.add (local.get $saddr) (i32.const 8)) (local.get $tag))
    (i32.store (i32.add (local.get $saddr) (i32.const 12)) (local.get $ival))
    (i32.store (i32.add (local.get $saddr) (i32.const 16)) (local.get $len))
    (global.set $last_ok (i32.const 1))
  )

  (func $sno_pop_void (export "sno_pop_void")
    (global.set $sp (i32.sub (global.get $sp) (i32.const 16)))
  )

  (func $sno_concat (export "sno_concat")
    (local $tag1 i32) (local $ival1 i32) (local $len1 i32)
    (local $tag2 i32) (local $ival2 i32) (local $len2 i32)
    (local $p1 i32) (local $l1 i32) (local $p2 i32) (local $l2 i32)
    (local $dst i32) (local $total i32)
    (call $pop_slot)
    (local.set $tag2  (global.get $pop_tag))
    (local.set $ival2 (global.get $pop_ival))
    (local.set $len2  (global.get $pop_len))
    (call $pop_slot)
    (local.set $tag1  (global.get $pop_tag))
    (local.set $ival1 (global.get $pop_ival))
    (local.set $len1  (global.get $pop_len))
    ;; If either operand is FAIL, the result is FAIL (propagates through).
    (if (i32.or (i32.eq (local.get $tag1) (global.get $TAG_FAIL))
                (i32.eq (local.get $tag2) (global.get $TAG_FAIL)))
      (then
        (call $push3 (global.get $TAG_FAIL) (i32.const 0) (i32.const 0))
        (global.set $last_ok (i32.const 0))
        (return)))
    (call $push3 (local.get $tag1) (local.get $ival1) (local.get $len1))
    (call $tos_to_str)
    (local.set $l1)
    (local.set $p1)
    (global.set $sp (i32.sub (global.get $sp) (i32.const 16)))
    (call $push3 (local.get $tag2) (local.get $ival2) (local.get $len2))
    (call $tos_to_str)
    (local.set $l2)
    (local.set $p2)
    (global.set $sp (i32.sub (global.get $sp) (i32.const 16)))
    (local.set $total (i32.add (local.get $l1) (local.get $l2)))
    (local.set $dst (call $alloc_str (local.get $total)))
    (call $memcpy (local.get $dst) (local.get $p1) (local.get $l1))
    (call $memcpy (i32.add (local.get $dst) (local.get $l1)) (local.get $p2) (local.get $l2))
    (call $push3 (global.get $TAG_STR) (local.get $dst) (local.get $total))
  )

  (func $sno_neg (export "sno_neg")
    (call $pop_slot)
    (if (i32.eq (global.get $pop_tag) (global.get $TAG_INT))
      (then
        (call $push3 (global.get $TAG_INT) (i32.sub (i32.const 0) (global.get $pop_ival)) (i32.const 0))
        (return)))
    (call $push3 (global.get $pop_tag) (global.get $pop_ival) (global.get $pop_len))
  )

  (func $sno_exp_op (export "sno_exp_op")
    (call $pop_slot) (call $pop_slot)
    (call $push3 (global.get $TAG_INT) (i32.const 0) (i32.const 0))
  )

  (func $sno_coerce_num (export "sno_coerce_num")
    (local $tag i32) (local $p i32) (local $l i32)
    (local $n i32) (local $neg i32) (local $i i32) (local $ch i32)
    (local.set $tag (call $peek_tag))
    (if (i32.eq (local.get $tag) (global.get $TAG_INT))  (then (return)))
    (if (i32.eq (local.get $tag) (global.get $TAG_REAL)) (then (return)))
    (local.set $p (call $peek_ival))
    (local.set $l (call $peek_len))
    (local.set $n (i32.const 0))
    (local.set $neg (i32.const 0))
    (local.set $i (i32.const 0))
    (if (i32.and (i32.gt_u (local.get $l) (i32.const 0))
                 (i32.eq (i32.load8_u (local.get $p)) (i32.const 45)))
      (then (local.set $neg (i32.const 1)) (local.set $i (i32.const 1))))
    (block $B (loop $L
      (br_if $B (i32.ge_u (local.get $i) (local.get $l)))
      (local.set $ch (i32.load8_u (i32.add (local.get $p) (local.get $i))))
      (br_if $B (i32.lt_u (local.get $ch) (i32.const 48)))
      (br_if $B (i32.gt_u (local.get $ch) (i32.const 57)))
      (local.set $n (i32.add (i32.mul (local.get $n) (i32.const 10)) (i32.sub (local.get $ch) (i32.const 48))))
      (local.set $i (i32.add (local.get $i) (i32.const 1)))
      (br $L)
    ))
    (if (i32.eq (local.get $i) (local.get $l))
      (then
        (if (local.get $neg) (then (local.set $n (i32.sub (i32.const 0) (local.get $n)))))
        (global.set $sp (i32.sub (global.get $sp) (i32.const 16)))
        (call $push3 (global.get $TAG_INT) (local.get $n) (i32.const 0))))
  )

  ;; arith: op 0=add 1=sub 2=mul 3=div 4=mod
  (func $sno_arith (export "sno_arith") (param $op i32)
    (local $v1 i32) (local $v2 i32) (local $r i32)
    (call $pop_slot) (local.set $v2 (global.get $pop_ival))
    (call $pop_slot) (local.set $v1 (global.get $pop_ival))
    (local.set $r (i32.const 0))
    (if (i32.eq (local.get $op) (i32.const 0))
      (then (local.set $r (i32.add (local.get $v1) (local.get $v2))))
    (else (if (i32.eq (local.get $op) (i32.const 1))
      (then (local.set $r (i32.sub (local.get $v1) (local.get $v2))))
    (else (if (i32.eq (local.get $op) (i32.const 2))
      (then (local.set $r (i32.mul (local.get $v1) (local.get $v2))))
    (else (if (i32.eq (local.get $op) (i32.const 3))
      (then
        (if (i32.eqz (local.get $v2))
          (then (local.set $r (i32.const 0)))
          (else (local.set $r (i32.div_s (local.get $v1) (local.get $v2))))))
    (else
      (if (i32.eqz (local.get $v2))
        (then (local.set $r (i32.const 0)))
        (else (local.set $r (i32.rem_s (local.get $v1) (local.get $v2)))))))))))))
    (call $push3 (global.get $TAG_INT) (local.get $r) (i32.const 0))
  )

  ;; acomp: op 0=eq 1=ne 2=lt 3=le 4=gt 5=ge
  (func $sno_acomp (export "sno_acomp") (param $op i32)
    (local $v1 i32) (local $v2 i32) (local $ok i32)
    (call $pop_slot) (local.set $v2 (global.get $pop_ival))
    (call $pop_slot) (local.set $v1 (global.get $pop_ival))
    (local.set $ok (i32.const 0))
    (if (i32.eq (local.get $op) (i32.const 0)) (then (local.set $ok (i32.eq  (local.get $v1) (local.get $v2))))
    (else (if (i32.eq (local.get $op) (i32.const 1)) (then (local.set $ok (i32.ne  (local.get $v1) (local.get $v2))))
    (else (if (i32.eq (local.get $op) (i32.const 2)) (then (local.set $ok (i32.lt_s (local.get $v1) (local.get $v2))))
    (else (if (i32.eq (local.get $op) (i32.const 3)) (then (local.set $ok (i32.le_s (local.get $v1) (local.get $v2))))
    (else (if (i32.eq (local.get $op) (i32.const 4)) (then (local.set $ok (i32.gt_s (local.get $v1) (local.get $v2))))
    (else (local.set $ok (i32.ge_s (local.get $v1) (local.get $v2)))))))))))))
    (global.set $last_ok (local.get $ok))
    (if (local.get $ok)
      (then (call $push3 (global.get $TAG_INT) (local.get $v1) (i32.const 0)))
      (else (call $push3 (global.get $TAG_FAIL) (i32.const 0) (i32.const 0))))
  )

  ;; lcomp: lexicographic string comparison; op same as acomp
  (func $sno_lcomp (export "sno_lcomp") (param $op i32)
    (local $p1 i32) (local $l1 i32) (local $p2 i32) (local $l2 i32)
    (local $i i32) (local $c1 i32) (local $c2 i32) (local $cmp i32) (local $ok i32)
    (call $pop_slot)
    (local.set $p2 (global.get $pop_ival)) (local.set $l2 (global.get $pop_len))
    (call $pop_slot)
    (local.set $p1 (global.get $pop_ival)) (local.set $l1 (global.get $pop_len))
    (local.set $i (i32.const 0)) (local.set $cmp (i32.const 0))
    (block $done (loop $lp
      (if (i32.ge_u (local.get $i) (local.get $l1))
        (then
          (if (i32.lt_u (local.get $i) (local.get $l2)) (then (local.set $cmp (i32.const -1))))
          (br $done)))
      (if (i32.ge_u (local.get $i) (local.get $l2))
        (then (local.set $cmp (i32.const 1)) (br $done)))
      (local.set $c1 (i32.load8_u (i32.add (local.get $p1) (local.get $i))))
      (local.set $c2 (i32.load8_u (i32.add (local.get $p2) (local.get $i))))
      (if (i32.ne (local.get $c1) (local.get $c2))
        (then
          (if (i32.lt_u (local.get $c1) (local.get $c2))
            (then (local.set $cmp (i32.const -1)))
            (else (local.set $cmp (i32.const 1))))
          (br $done)))
      (local.set $i (i32.add (local.get $i) (i32.const 1)))
      (br $lp)
    ))
    (local.set $ok (i32.const 0))
    (if (i32.eq (local.get $op) (i32.const 0))
      (then (local.set $ok (i32.eqz (local.get $cmp))))
      (else (if (i32.eq (local.get $op) (i32.const 1))
        (then (local.set $ok (i32.ne (local.get $cmp) (i32.const 0))))
        (else (if (i32.eq (local.get $op) (i32.const 2))
          (then (local.set $ok (i32.lt_s (local.get $cmp) (i32.const 0))))
          (else (if (i32.eq (local.get $op) (i32.const 3))
            (then (local.set $ok (i32.le_s (local.get $cmp) (i32.const 0))))
            (else (if (i32.eq (local.get $op) (i32.const 4))
              (then (local.set $ok (i32.gt_s (local.get $cmp) (i32.const 0))))
              (else (local.set $ok (i32.ge_s (local.get $cmp) (i32.const 0))))
            ))
          ))
        ))
      ))
    )
    (global.set $last_ok (local.get $ok))
    (if (local.get $ok)
      (then (call $push3 (global.get $TAG_STR) (local.get $p1) (local.get $l1)))
      (else (call $push3 (global.get $TAG_FAIL) (i32.const 0) (i32.const 0))))
  )

    (func $sno_last_ok     (export "sno_last_ok") (result i32) (global.get $last_ok))
  (func $sno_set_last_ok (export "sno_set_last_ok") (param $v i32) (global.set $last_ok (local.get $v)))
  (func $sno_set_stno    (export "sno_set_stno") (param $v i32) (global.set $stno (local.get $v)))

  (func $sno_halt_tos (export "sno_halt_tos")
    ;; SNOBOL4 halt: do nothing.  Earlier draft printed the TOS for smoke-test
    ;; debugging, but real SNOBOL4 halt is silent.  Programs that need to print
    ;; do so explicitly via OUTPUT = ... .  Printing here causes spurious lines
    ;; like "0" or "" to appear after pattern-match statements whose result is
    ;; left on the stack by the (still-stubbed) SM_PAT_* / SM_EXEC_GEN opcodes.
    (return)
  )

  ;; sno_call: dispatch by builtin name; for now handle the numeric comparators
  ;; LT/GT/EQ/NE/GE/LE (each takes 2 numeric args) and IDENT/DIFFER (2 args, any type).
  ;; Success → push "" (TAG_STR len=0) and set last_ok=1; Failure → push null and set last_ok=0.
  ;; Unknown builtin → pops nargs, pushes null (legacy stub behavior, last_ok unchanged).
  (func $sno_call (export "sno_call") (param $name_ptr i32) (param $name_len i32) (param $nargs i32)
    (local $i i32)
    (local $a_tag i32) (local $a_ival i32) (local $a_len i32) (local $a_off i32)
    (local $b_tag i32) (local $b_ival i32) (local $b_len i32) (local $b_off i32)
    (local $cmp i32) (local $ok i32) (local $matched i32)
    (local.set $matched (i32.const 0))
    ;; If exactly 2 args, peek both. b is TOS, a is TOS-1.
    (if (i32.eq (local.get $nargs) (i32.const 2))
      (then
        (local.set $b_off (i32.sub (global.get $sp) (i32.const 16)))
        (local.set $a_off (i32.sub (global.get $sp) (i32.const 32)))
        (local.set $b_tag  (i32.load (local.get $b_off)))
        (local.set $b_ival (i32.load (i32.add (local.get $b_off) (i32.const 4))))
        (local.set $b_len  (i32.load (i32.add (local.get $b_off) (i32.const 8))))
        (local.set $a_tag  (i32.load (local.get $a_off)))
        (local.set $a_ival (i32.load (i32.add (local.get $a_off) (i32.const 4))))
        (local.set $a_len  (i32.load (i32.add (local.get $a_off) (i32.const 8))))
        ;; LT(a,b) ─ succeed iff a<b
        (if (call $str_eq (local.get $name_ptr) (local.get $name_len) (i32.const 0x31100) (i32.const 2))
          (then (local.set $matched (i32.const 1))
                (local.set $ok (i32.lt_s (local.get $a_ival) (local.get $b_ival)))))
        (if (call $str_eq (local.get $name_ptr) (local.get $name_len) (i32.const 0x31103) (i32.const 2))
          (then (local.set $matched (i32.const 1))
                (local.set $ok (i32.gt_s (local.get $a_ival) (local.get $b_ival)))))
        (if (call $str_eq (local.get $name_ptr) (local.get $name_len) (i32.const 0x31106) (i32.const 2))
          (then (local.set $matched (i32.const 1))
                (local.set $ok (i32.eq (local.get $a_ival) (local.get $b_ival)))))
        (if (call $str_eq (local.get $name_ptr) (local.get $name_len) (i32.const 0x31109) (i32.const 2))
          (then (local.set $matched (i32.const 1))
                (local.set $ok (i32.ne (local.get $a_ival) (local.get $b_ival)))))
        (if (call $str_eq (local.get $name_ptr) (local.get $name_len) (i32.const 0x3110c) (i32.const 2))
          (then (local.set $matched (i32.const 1))
                (local.set $ok (i32.ge_s (local.get $a_ival) (local.get $b_ival)))))
        (if (call $str_eq (local.get $name_ptr) (local.get $name_len) (i32.const 0x3110f) (i32.const 2))
          (then (local.set $matched (i32.const 1))
                (local.set $ok (i32.le_s (local.get $a_ival) (local.get $b_ival)))))
        ;; IDENT(a,b) ─ succeed iff a===b (same type and value).  For ints/reals use ival; for strings use byte-eq.
        (if (call $str_eq (local.get $name_ptr) (local.get $name_len) (i32.const 0x31112) (i32.const 5))
          (then
            (local.set $matched (i32.const 1))
            (local.set $ok (i32.const 0))
            (if (i32.eq (local.get $a_tag) (local.get $b_tag))
              (then
                (if (i32.eq (local.get $a_tag) (global.get $TAG_STR))
                  (then (local.set $ok
                         (call $str_eq (local.get $a_ival) (local.get $a_len)
                                       (local.get $b_ival) (local.get $b_len))))
                  (else (local.set $ok (i32.eq (local.get $a_ival) (local.get $b_ival)))))))))
        ;; DIFFER(a,b) ─ succeed iff !IDENT(a,b)
        (if (call $str_eq (local.get $name_ptr) (local.get $name_len) (i32.const 0x31118) (i32.const 6))
          (then
            (local.set $matched (i32.const 1))
            (local.set $ok (i32.const 1))
            (if (i32.eq (local.get $a_tag) (local.get $b_tag))
              (then
                (if (i32.eq (local.get $a_tag) (global.get $TAG_STR))
                  (then (local.set $ok
                         (i32.eqz (call $str_eq (local.get $a_ival) (local.get $a_len)
                                                (local.get $b_ival) (local.get $b_len)))))
                  (else (local.set $ok (i32.ne (local.get $a_ival) (local.get $b_ival)))))))))
        (if (local.get $matched)
          (then
            ;; pop both args, push result, set last_ok
            (global.set $sp (i32.sub (global.get $sp) (i32.const 32)))
            (if (local.get $ok)
              (then
                (call $push3 (global.get $TAG_STR) (i32.const 0) (i32.const 0))
                (global.set $last_ok (i32.const 1)))
              (else
                (call $push3 (global.get $TAG_FAIL) (i32.const 0) (i32.const 0))
                (global.set $last_ok (i32.const 0))))
            (return)))))
    ;; ---- 1-arg builtins ----
    ;; If exactly 1 arg, peek it.
    (if (i32.eq (local.get $nargs) (i32.const 1))
      (then
        (local.set $a_off (i32.sub (global.get $sp) (i32.const 16)))
        (local.set $a_tag  (i32.load (local.get $a_off)))
        (local.set $a_ival (i32.load (i32.add (local.get $a_off) (i32.const 4))))
        (local.set $a_len  (i32.load (i32.add (local.get $a_off) (i32.const 8))))
        ;; SIZE(s): if string, len; if int, length of decimal representation.
        (if (call $str_eq (local.get $name_ptr) (local.get $name_len) (i32.const 0x31130) (i32.const 4))
          (then
            (local.set $matched (i32.const 1))
            (global.set $sp (i32.sub (global.get $sp) (i32.const 16)))
            (if (i32.eq (local.get $a_tag) (global.get $TAG_STR))
              (then (call $push3 (global.get $TAG_INT) (local.get $a_len) (i32.const 0)))
              (else
                (if (i32.eq (local.get $a_tag) (global.get $TAG_INT))
                  (then
                    ;; Count digits of |ival|; if negative add 1 for '-'.
                    (local.set $b_ival (local.get $a_ival))
                    (local.set $i (i32.const 0))
                    (if (i32.lt_s (local.get $b_ival) (i32.const 0))
                      (then (local.set $b_ival (i32.sub (i32.const 0) (local.get $b_ival)))
                            (local.set $i (i32.const 1))))
                    (if (i32.eqz (local.get $b_ival))
                      (then (local.set $i (i32.add (local.get $i) (i32.const 1)))))
                    (block $B (loop $L
                      (br_if $B (i32.eqz (local.get $b_ival)))
                      (local.set $i (i32.add (local.get $i) (i32.const 1)))
                      (local.set $b_ival (i32.div_u (local.get $b_ival) (i32.const 10)))
                      (br $L)
                    ))
                    (call $push3 (global.get $TAG_INT) (local.get $i) (i32.const 0)))
                  (else (call $push3 (global.get $TAG_INT) (i32.const 0) (i32.const 0))))))
            (global.set $last_ok (i32.const 1))
            (return)))
        ;; INTEGER(s): succeed iff s can be parsed as int; push 0 (it's a predicate that returns "")
        (if (call $str_eq (local.get $name_ptr) (local.get $name_len) (i32.const 0x31135) (i32.const 7))
          (then
            (local.set $matched (i32.const 1))
            (global.set $sp (i32.sub (global.get $sp) (i32.const 16)))
            (if (i32.eq (local.get $a_tag) (global.get $TAG_INT))
              (then (call $push3 (global.get $TAG_STR) (i32.const 0) (i32.const 0))
                    (global.set $last_ok (i32.const 1)))
              (else
                ;; Check string parses as int (digits only, optional leading -)
                (local.set $i (i32.const 0))
                (local.set $cmp (i32.const 1))
                (if (i32.and (i32.gt_u (local.get $a_len) (i32.const 0))
                             (i32.eq (i32.load8_u (local.get $a_ival)) (i32.const 45)))
                  (then (local.set $i (i32.const 1))))
                (if (i32.eq (local.get $i) (local.get $a_len)) (then (local.set $cmp (i32.const 0))))
                (block $B (loop $L
                  (br_if $B (i32.ge_u (local.get $i) (local.get $a_len)))
                  (local.set $b_tag (i32.load8_u (i32.add (local.get $a_ival) (local.get $i))))
                  (if (i32.or (i32.lt_u (local.get $b_tag) (i32.const 48)) (i32.gt_u (local.get $b_tag) (i32.const 57)))
                    (then (local.set $cmp (i32.const 0)) (br $B)))
                  (local.set $i (i32.add (local.get $i) (i32.const 1)))
                  (br $L)
                ))
                (if (local.get $cmp)
                  (then (call $push3 (global.get $TAG_STR) (i32.const 0) (i32.const 0))
                        (global.set $last_ok (i32.const 1)))
                  (else (call $push3 (global.get $TAG_NULL) (i32.const 0) (i32.const 0))
                        (global.set $last_ok (i32.const 0))))))
            (return)))
        ;; CHAR(n): single-byte string with code n. We need a heap byte.
        (if (call $str_eq (local.get $name_ptr) (local.get $name_len) (i32.const 0x3115a) (i32.const 4))
          (then
            (local.set $matched (i32.const 1))
            (local.set $i (global.get $str_ptr))
            (i32.store8 (local.get $i) (i32.and (local.get $a_ival) (i32.const 255)))
            (global.set $str_ptr (i32.add (local.get $i) (i32.const 1)))
            (global.set $sp (i32.sub (global.get $sp) (i32.const 16)))
            (call $push3 (global.get $TAG_STR) (local.get $i) (i32.const 1))
            (global.set $last_ok (i32.const 1))
            (return)))
        ;; ORD(s): first byte's code, or fail if empty.
        (if (call $str_eq (local.get $name_ptr) (local.get $name_len) (i32.const 0x3115f) (i32.const 3))
          (then
            (local.set $matched (i32.const 1))
            (global.set $sp (i32.sub (global.get $sp) (i32.const 16)))
            (if (i32.and (i32.eq (local.get $a_tag) (global.get $TAG_STR)) (i32.gt_u (local.get $a_len) (i32.const 0)))
              (then (call $push3 (global.get $TAG_INT) (i32.load8_u (local.get $a_ival)) (i32.const 0))
                    (global.set $last_ok (i32.const 1)))
              (else (call $push3 (global.get $TAG_NULL) (i32.const 0) (i32.const 0))
                    (global.set $last_ok (i32.const 0))))
            (return)))
        ;; TRIM(s): strip trailing spaces. Modifies length only.
        (if (call $str_eq (local.get $name_ptr) (local.get $name_len) (i32.const 0x31144) (i32.const 4))
          (then
            (local.set $matched (i32.const 1))
            (global.set $sp (i32.sub (global.get $sp) (i32.const 16)))
            (if (i32.eq (local.get $a_tag) (global.get $TAG_STR))
              (then
                ;; walk back while last byte is space/tab
                (local.set $i (local.get $a_len))
                (block $B (loop $L
                  (br_if $B (i32.eqz (local.get $i)))
                  (local.set $b_tag (i32.load8_u (i32.add (local.get $a_ival) (i32.sub (local.get $i) (i32.const 1)))))
                  (br_if $B (i32.and (i32.ne (local.get $b_tag) (i32.const 32))
                                     (i32.ne (local.get $b_tag) (i32.const 9))))
                  (local.set $i (i32.sub (local.get $i) (i32.const 1)))
                  (br $L)
                ))
                (call $push3 (global.get $TAG_STR) (local.get $a_ival) (local.get $i)))
              (else (call $push3 (local.get $a_tag) (local.get $a_ival) (local.get $a_len))))
            (global.set $last_ok (i32.const 1))
            (return)))
        ;; REVERSE(s): allocate new str, copy bytes in reverse.
        (if (call $str_eq (local.get $name_ptr) (local.get $name_len) (i32.const 0x31149) (i32.const 7))
          (then
            (local.set $matched (i32.const 1))
            (global.set $sp (i32.sub (global.get $sp) (i32.const 16)))
            (if (i32.eq (local.get $a_tag) (global.get $TAG_STR))
              (then
                (local.set $b_off (call $alloc_str (local.get $a_len)))
                (local.set $i (i32.const 0))
                (block $B (loop $L
                  (br_if $B (i32.ge_u (local.get $i) (local.get $a_len)))
                  (i32.store8
                    (i32.add (local.get $b_off) (i32.sub (i32.sub (local.get $a_len) (i32.const 1)) (local.get $i)))
                    (i32.load8_u (i32.add (local.get $a_ival) (local.get $i))))
                  (local.set $i (i32.add (local.get $i) (i32.const 1)))
                  (br $L)
                ))
                (call $push3 (global.get $TAG_STR) (local.get $b_off) (local.get $a_len)))
              (else (call $push3 (local.get $a_tag) (local.get $a_ival) (local.get $a_len))))
            (global.set $last_ok (i32.const 1))
            (return)))
      ))
    ;; ---- 3-arg builtins ----
    (if (i32.eq (local.get $nargs) (i32.const 3))
      (then
        (local.set $a_off (i32.sub (global.get $sp) (i32.const 48)))
        (local.set $a_tag  (i32.load (local.get $a_off)))
        (local.set $a_ival (i32.load (i32.add (local.get $a_off) (i32.const 4))))
        (local.set $a_len  (i32.load (i32.add (local.get $a_off) (i32.const 8))))
        (local.set $b_off (i32.sub (global.get $sp) (i32.const 32)))
        (local.set $b_ival (i32.load (i32.add (local.get $b_off) (i32.const 4))))
        ;; The 3rd arg's value is the length to copy.
        (local.set $cmp (i32.load (i32.add (i32.sub (global.get $sp) (i32.const 16)) (i32.const 4))))
        ;; SUBSTR(s, start, len): 1-based start.  Failure if out-of-bounds.
        (if (call $str_eq (local.get $name_ptr) (local.get $name_len) (i32.const 0x3117e) (i32.const 6))
          (then
            (local.set $matched (i32.const 1))
            (global.set $sp (i32.sub (global.get $sp) (i32.const 48)))
            (if (i32.and (i32.eq (local.get $a_tag) (global.get $TAG_STR))
                         (i32.and (i32.ge_s (local.get $b_ival) (i32.const 1))
                                  (i32.le_s (i32.add (i32.sub (local.get $b_ival) (i32.const 1)) (local.get $cmp))
                                            (local.get $a_len))))
              (then (call $push3 (global.get $TAG_STR)
                          (i32.add (local.get $a_ival) (i32.sub (local.get $b_ival) (i32.const 1)))
                          (local.get $cmp))
                    (global.set $last_ok (i32.const 1)))
              (else (call $push3 (global.get $TAG_NULL) (i32.const 0) (i32.const 0))
                    (global.set $last_ok (i32.const 0))))
            (return)))
      ))
    ;; Fallback stub: pop nargs, push FAIL, set last_ok=0.  Treating unknown builtins as failures
    ;; matches the SNOBOL4 semantics where an unimplemented function returns failure, which causes
    ;; the surrounding assignment to be a no-op (no print on OUTPUT = unknown_fn(...)).
    (local.set $i (i32.const 0))
    (block $B (loop $L
      (br_if $B (i32.ge_u (local.get $i) (local.get $nargs)))
      (global.set $sp (i32.sub (global.get $sp) (i32.const 16)))
      (local.set $i (i32.add (local.get $i) (i32.const 1)))
      (br $L)
    ))
    (call $push3 (global.get $TAG_FAIL) (i32.const 0) (i32.const 0))
    (global.set $last_ok (i32.const 0))
  )

  ;; ── call-frame helpers (user-defined function support, SN4-WASM-5f) ─────
  ;; Frame layout (32 bytes at $call_sp):
  ;;   +0  ret_pc          i32
  ;;   +4  retname_ptr     i32
  ;;   +8  retname_len     i32
  ;;   +12 saved_start_off i32   (offset into saved-vars region where this frame's saved entries start)
  ;;   +16 saved_count     i32   (number of saved entries)
  ;;   +20 caller_sp       i32   (value-stack pointer saved at call time)
  ;;   +24..+31 reserved
  ;; Saved-var entry layout (20 bytes at $sv_top):
  ;;   +0  name_ptr  i32
  ;;   +4  name_len  i32
  ;;   +8  tag       i32
  ;;   +12 ival      i32
  ;;   +16 len       i32

  ;; sno_call_frame_push: open a new frame, return its start address (for the emitter helper to populate
  ;; saved_count after recording bindings). Records ret_pc, retname, current sv_top as saved_start_off,
  ;; saved_count=0, caller_sp=current sp. Does NOT advance call_sp until sno_call_frame_close.
  (func $sno_call_frame_push (export "sno_call_frame_push")
        (param $ret_pc i32) (param $retname_ptr i32) (param $retname_len i32)
        (result i32)
    (local $fr i32)
    (local.set $fr (global.get $call_sp))
    (i32.store              (local.get $fr)                       (local.get $ret_pc))
    (i32.store (i32.add     (local.get $fr) (i32.const 4))        (local.get $retname_ptr))
    (i32.store (i32.add     (local.get $fr) (i32.const 8))        (local.get $retname_len))
    (i32.store (i32.add     (local.get $fr) (i32.const 12))       (global.get $sv_top))
    (i32.store (i32.add     (local.get $fr) (i32.const 16))       (i32.const 0))
    (i32.store (i32.add     (local.get $fr) (i32.const 20))       (global.get $sp))
    (local.get $fr)
  )

  ;; sno_call_frame_close: commit the frame (advance call_sp by 32).
  (func $sno_call_frame_close (export "sno_call_frame_close")
    (global.set $call_sp (i32.add (global.get $call_sp) (i32.const 32)))
  )

  ;; sno_save_var: record current binding of (name_ptr,name_len) into saved-vars region;
  ;; bump $sv_top; increment current frame's saved_count.  $fr is the open frame address.
  (func $sno_save_var (export "sno_save_var")
        (param $fr i32) (param $name_ptr i32) (param $name_len i32)
    (local $svaddr i32) (local $vslot i32) (local $cnt i32)
    (local.set $svaddr (global.get $sv_top))
    (local.set $vslot  (call $var_slot_addr (local.get $name_ptr) (local.get $name_len)))
    (i32.store              (local.get $svaddr)                     (local.get $name_ptr))
    (i32.store (i32.add     (local.get $svaddr) (i32.const 4))      (local.get $name_len))
    (i32.store (i32.add     (local.get $svaddr) (i32.const 8))      (i32.load (i32.add (local.get $vslot) (i32.const 8))))
    (i32.store (i32.add     (local.get $svaddr) (i32.const 12))     (i32.load (i32.add (local.get $vslot) (i32.const 12))))
    (i32.store (i32.add     (local.get $svaddr) (i32.const 16))     (i32.load (i32.add (local.get $vslot) (i32.const 16))))
    (global.set $sv_top (i32.add (local.get $svaddr) (i32.const 20)))
    (local.set $cnt (i32.load (i32.add (local.get $fr) (i32.const 16))))
    (i32.store (i32.add (local.get $fr) (i32.const 16)) (i32.add (local.get $cnt) (i32.const 1)))
  )

  ;; sno_clear_var: set var to empty string (tag=STR, ival=0, len=0).
  (func $sno_clear_var (export "sno_clear_var") (param $name_ptr i32) (param $name_len i32)
    (local $vslot i32)
    (local.set $vslot (call $var_slot_addr (local.get $name_ptr) (local.get $name_len)))
    (i32.store              (local.get $vslot)                     (local.get $name_ptr))
    (i32.store (i32.add     (local.get $vslot) (i32.const 4))      (local.get $name_len))
    (i32.store (i32.add     (local.get $vslot) (i32.const 8))      (global.get $TAG_STR))
    (i32.store (i32.add     (local.get $vslot) (i32.const 12))     (i32.const 0))
    (i32.store (i32.add     (local.get $vslot) (i32.const 16))     (i32.const 0))
  )

  ;; sno_set_var_from_tos: pop value stack, store into var (used to bind a formal parameter).
  (func $sno_set_var_from_tos (export "sno_set_var_from_tos") (param $name_ptr i32) (param $name_len i32)
    (local $vslot i32) (local $taddr i32)
    (call $pop_slot)
    (local.set $vslot (call $var_slot_addr (local.get $name_ptr) (local.get $name_len)))
    (i32.store              (local.get $vslot)                     (local.get $name_ptr))
    (i32.store (i32.add     (local.get $vslot) (i32.const 4))      (local.get $name_len))
    (i32.store (i32.add     (local.get $vslot) (i32.const 8))      (global.get $pop_tag))
    (i32.store (i32.add     (local.get $vslot) (i32.const 12))     (global.get $pop_ival))
    (i32.store (i32.add     (local.get $vslot) (i32.const 16))     (global.get $pop_len))
  )

  ;; sno_pop_to_null: pop value stack discarding the slot (used when there are extra args).
  (func $sno_pop_to_null (export "sno_pop_to_null")
    (global.set $sp (i32.sub (global.get $sp) (i32.const 16)))
  )

  ;; sno_fn_return: pop top frame, restore saved bindings, push retval to value stack, return ret_pc.
  ;; kind: 0=RETURN (use retname's value), 1=FRETURN (failure), 2=NRETURN (treat as RETURN for now).
  ;; cond: 0=plain, 1=only if last_ok, 2=only if NOT last_ok.
  ;; Returns -1 if cond not met (caller falls through), -2 if no frame (halt), else ret_pc.
  (func $sno_fn_return (export "sno_fn_return") (param $kind i32) (param $cond i32) (result i32)
    (local $fr i32) (local $ret_pc i32) (local $retname_ptr i32) (local $retname_len i32)
    (local $sv_start i32) (local $sv_cnt i32) (local $i i32) (local $svaddr i32)
    (local $vslot i32) (local $r_tag i32) (local $r_ival i32) (local $r_len i32)
    (if (i32.and (i32.eq (local.get $cond) (i32.const 1)) (i32.eqz (global.get $last_ok)))
      (then (return (i32.const -1))))
    (if (i32.and (i32.eq (local.get $cond) (i32.const 2)) (global.get $last_ok))
      (then (return (i32.const -1))))
    (if (i32.le_u (global.get $call_sp) (i32.const 0x70000))
      (then (return (i32.const -2))))
    (global.set $call_sp (i32.sub (global.get $call_sp) (i32.const 32)))
    (local.set $fr (global.get $call_sp))
    (local.set $ret_pc       (i32.load              (local.get $fr)))
    (local.set $retname_ptr  (i32.load (i32.add     (local.get $fr) (i32.const 4))))
    (local.set $retname_len  (i32.load (i32.add     (local.get $fr) (i32.const 8))))
    (local.set $sv_start     (i32.load (i32.add     (local.get $fr) (i32.const 12))))
    (local.set $sv_cnt       (i32.load (i32.add     (local.get $fr) (i32.const 16))))
    ;; Read retval BEFORE restoring saved bindings (so we get the callee's value).
    (if (i32.eq (local.get $kind) (i32.const 1))
      (then
        (local.set $r_tag  (global.get $TAG_FAIL))
        (local.set $r_ival (i32.const 0))
        (local.set $r_len  (i32.const 0)))
      (else
        (local.set $vslot (call $var_slot_addr (local.get $retname_ptr) (local.get $retname_len)))
        (local.set $r_tag  (i32.load (i32.add (local.get $vslot) (i32.const 8))))
        (local.set $r_ival (i32.load (i32.add (local.get $vslot) (i32.const 12))))
        (local.set $r_len  (i32.load (i32.add (local.get $vslot) (i32.const 16))))))
    ;; Restore saved bindings (reverse order to handle duplicates correctly).
    (local.set $i (local.get $sv_cnt))
    (block $B (loop $L
      (br_if $B (i32.eqz (local.get $i)))
      (local.set $i (i32.sub (local.get $i) (i32.const 1)))
      (local.set $svaddr (i32.add (local.get $sv_start) (i32.mul (local.get $i) (i32.const 20))))
      (local.set $vslot (call $var_slot_addr
        (i32.load              (local.get $svaddr))
        (i32.load (i32.add     (local.get $svaddr) (i32.const 4)))))
      (i32.store              (local.get $vslot)                 (i32.load              (local.get $svaddr)))
      (i32.store (i32.add     (local.get $vslot) (i32.const 4))  (i32.load (i32.add     (local.get $svaddr) (i32.const 4))))
      (i32.store (i32.add     (local.get $vslot) (i32.const 8))  (i32.load (i32.add     (local.get $svaddr) (i32.const 8))))
      (i32.store (i32.add     (local.get $vslot) (i32.const 12)) (i32.load (i32.add     (local.get $svaddr) (i32.const 12))))
      (i32.store (i32.add     (local.get $vslot) (i32.const 16)) (i32.load (i32.add     (local.get $svaddr) (i32.const 16))))
      (br $L)
    ))
    ;; Reclaim saved-vars space.
    (global.set $sv_top (local.get $sv_start))
    ;; Push retval onto value stack.
    (if (i32.eq (local.get $kind) (i32.const 1))
      (then (call $push3 (global.get $TAG_FAIL) (i32.const 0) (i32.const 0))
            (global.set $last_ok (i32.const 0)))
      (else (call $push3 (local.get $r_tag) (local.get $r_ival) (local.get $r_len))
            (global.set $last_ok (i32.const 1))))
    (local.get $ret_pc)
  )

  (func $sno_do_return (export "sno_do_return") (param $kind i32) (param $cond i32))
)
