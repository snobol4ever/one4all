;; sno_runtime.wat — SNOBOL4 WASM runtime MODULE
;; Standalone module: exports memory + all runtime functions.
;; Programs import from this module rather than inlining it.
;;
;; Memory layout (1 page = 64KB):
;;   [0  .. 32767]  output buffer  — written by sno_output_*; main() returns fill length
;;   [32768 .. 49151]  string heap — sno_str_alloc() bumps $str_ptr upward
;;   [49152 .. 65535]  variable table (reserved for M-SW-A04)
;;
;; Compile once per session:
;;   wat2wasm --enable-tail-call sno_runtime.wat -o sno_runtime.wasm
;;
;; Milestone: M-SW-1 (fragment); M-SW-A02 (standalone module)
;; Author: Claude Sonnet 4.6

(module
  ;; ── Memory (exported so programs can read output buffer) ──────────────────
  (memory (export "memory") 1)

  (global $out_pos (mut i32) (i32.const 0))
  (global $str_ptr (mut i32) (i32.const 32768))

  ;; ── sno_output_str (offset: i32, len: i32) ────────────────────────────────
  (func $sno_output_str (export "sno_output_str") (param $offset i32) (param $len i32)
    (local $i i32)
    (local $dst i32)
    (local.set $dst (global.get $out_pos))
    (local.set $i (i32.const 0))
    (block $done
      (loop $copy
        (br_if $done (i32.ge_u (local.get $i) (local.get $len)))
        (i32.store8
          (i32.add (local.get $dst) (local.get $i))
          (i32.load8_u (i32.add (local.get $offset) (local.get $i))))
        (local.set $i (i32.add (local.get $i) (i32.const 1)))
        (br $copy)
      )
    )
    (i32.store8 (i32.add (local.get $dst) (local.get $len)) (i32.const 10))
    (global.set $out_pos
      (i32.add (local.get $dst) (i32.add (local.get $len) (i32.const 1))))
  )

  ;; ── sno_output_int (val: i64) ─────────────────────────────────────────────
  (func $sno_output_int (export "sno_output_int") (param $val i64)
    (local $pos i32) (local $start i32) (local $end i32)
    (local $tmp i32) (local $neg i32) (local $v i64) (local $digit i32)
    (local.set $pos (global.get $out_pos))
    (local.set $start (local.get $pos))
    (local.set $neg (i32.const 0))
    (local.set $v (local.get $val))
    (if (i64.lt_s (local.get $v) (i64.const 0))
      (then
        (local.set $neg (i32.const 1))
        (local.set $v (i64.sub (i64.const 0) (local.get $v)))))
    (if (i64.eqz (local.get $v))
      (then
        (i32.store8 (local.get $pos) (i32.const 48))
        (local.set $pos (i32.add (local.get $pos) (i32.const 1))))
      (else
        (block $dbreak
          (loop $digits
            (br_if $dbreak (i64.eqz (local.get $v)))
            (local.set $digit (i32.wrap_i64 (i64.rem_u (local.get $v) (i64.const 10))))
            (i32.store8 (local.get $pos) (i32.add (local.get $digit) (i32.const 48)))
            (local.set $pos (i32.add (local.get $pos) (i32.const 1)))
            (local.set $v (i64.div_u (local.get $v) (i64.const 10)))
            (br $digits)
          )
        )
        (if (local.get $neg)
          (then
            (i32.store8 (local.get $pos) (i32.const 45))
            (local.set $pos (i32.add (local.get $pos) (i32.const 1)))))
        (local.set $end (i32.sub (local.get $pos) (i32.const 1)))
        (local.set $tmp (local.get $start))
        (block $rbreak
          (loop $rev
            (br_if $rbreak (i32.ge_u (local.get $tmp) (local.get $end)))
            (local.set $digit (i32.load8_u (local.get $tmp)))
            (i32.store8 (local.get $tmp) (i32.load8_u (local.get $end)))
            (i32.store8 (local.get $end) (local.get $digit))
            (local.set $tmp (i32.add (local.get $tmp) (i32.const 1)))
            (local.set $end (i32.sub (local.get $end) (i32.const 1)))
            (br $rev)
          )
        )
      )
    )
    (i32.store8 (local.get $pos) (i32.const 10))
    (global.set $out_pos (i32.add (local.get $pos) (i32.const 1)))
  )

  ;; ── sno_output_flush () → i32 ─────────────────────────────────────────────
  (func $sno_output_flush (export "sno_output_flush") (result i32)
    (global.get $out_pos)
  )

  ;; ── sno_str_alloc (len: i32) → i32 ───────────────────────────────────────
  (func $sno_str_alloc (export "sno_str_alloc") (param $len i32) (result i32)
    (local $ptr i32)
    (local.set $ptr (global.get $str_ptr))
    (global.set $str_ptr (i32.add (local.get $ptr) (local.get $len)))
    (local.get $ptr)
  )

  ;; ── sno_str_concat ────────────────────────────────────────────────────────
  (func $sno_str_concat (export "sno_str_concat")
    (param $a_off i32) (param $a_len i32)
    (param $b_off i32) (param $b_len i32)
    (result i32 i32)
    (local $total i32) (local $dst i32) (local $i i32)
    (local.set $total (i32.add (local.get $a_len) (local.get $b_len)))
    (local.set $dst (call $sno_str_alloc (local.get $total)))
    (local.set $i (i32.const 0))
    (block $da (loop $la
      (br_if $da (i32.ge_u (local.get $i) (local.get $a_len)))
      (i32.store8 (i32.add (local.get $dst) (local.get $i))
        (i32.load8_u (i32.add (local.get $a_off) (local.get $i))))
      (local.set $i (i32.add (local.get $i) (i32.const 1)))
      (br $la)))
    (local.set $i (i32.const 0))
    (block $db (loop $lb
      (br_if $db (i32.ge_u (local.get $i) (local.get $b_len)))
      (i32.store8 (i32.add (local.get $dst) (i32.add (local.get $a_len) (local.get $i)))
        (i32.load8_u (i32.add (local.get $b_off) (local.get $i))))
      (local.set $i (i32.add (local.get $i) (i32.const 1)))
      (br $lb)))
    (local.get $dst)
    (local.get $total)
  )

  ;; ── sno_str_eq ────────────────────────────────────────────────────────────
  (func $sno_str_eq (export "sno_str_eq")
    (param $a_off i32) (param $a_len i32)
    (param $b_off i32) (param $b_len i32)
    (result i32)
    (local $i i32)
    (if (i32.ne (local.get $a_len) (local.get $b_len))
      (then (return (i32.const 0))))
    (local.set $i (i32.const 0))
    (block $done (loop $cmp
      (br_if $done (i32.ge_u (local.get $i) (local.get $a_len)))
      (if (i32.ne
            (i32.load8_u (i32.add (local.get $a_off) (local.get $i)))
            (i32.load8_u (i32.add (local.get $b_off) (local.get $i))))
        (then (return (i32.const 0))))
      (local.set $i (i32.add (local.get $i) (i32.const 1)))
      (br $cmp)))
    (i32.const 1)
  )

  ;; ── sno_pow (base f64, exp f64) → f64 ─────────────────────────────────────
  (func $sno_pow (export "sno_pow") (param $base f64) (param $exp f64) (result f64)
    (local $result f64) (local $n i64) (local $neg_exp i32)
    (local.set $result (f64.const 1))
    (local.set $n (i64.trunc_f64_s (local.get $exp)))
    (local.set $neg_exp (i64.lt_s (local.get $n) (i64.const 0)))
    (if (local.get $neg_exp)
      (then (local.set $n (i64.sub (i64.const 0) (local.get $n)))))
    (block $done (loop $pow
      (br_if $done (i64.eqz (local.get $n)))
      (local.set $result (f64.mul (local.get $result) (local.get $base)))
      (local.set $n (i64.sub (local.get $n) (i64.const 1)))
      (br $pow)))
    (if (local.get $neg_exp)
      (then (local.set $result (f64.div (f64.const 1) (local.get $result)))))
    (local.get $result)
  )

  ;; ── sno_str_to_int (off i32, len i32) → i64 ───────────────────────────────
  (func $sno_str_to_int (export "sno_str_to_int")
    (param $off i32) (param $len i32) (result i64)
    (local $i i32) (local $neg i32) (local $result i64) (local $c i32)
    (local.set $result (i64.const 0))
    (local.set $neg (i32.const 0))
    (local.set $i (i32.const 0))
    ;; skip leading spaces
    (block $sp_done (loop $sp
      (br_if $sp_done (i32.ge_u (local.get $i) (local.get $len)))
      (local.set $c (i32.load8_u (i32.add (local.get $off) (local.get $i))))
      (br_if $sp_done (i32.ne (local.get $c) (i32.const 32)))
      (local.set $i (i32.add (local.get $i) (i32.const 1)))
      (br $sp)))
    ;; optional sign
    (if (i32.lt_u (local.get $i) (local.get $len))
      (then
        (local.set $c (i32.load8_u (i32.add (local.get $off) (local.get $i))))
        (if (i32.eq (local.get $c) (i32.const 45))
          (then (local.set $neg (i32.const 1))
                (local.set $i (i32.add (local.get $i) (i32.const 1)))))
        (if (i32.eq (local.get $c) (i32.const 43))
          (then (local.set $i (i32.add (local.get $i) (i32.const 1)))))))
    ;; digits
    (block $dbreak (loop $dlp
      (br_if $dbreak (i32.ge_u (local.get $i) (local.get $len)))
      (local.set $c (i32.load8_u (i32.add (local.get $off) (local.get $i))))
      (br_if $dbreak (i32.lt_u (local.get $c) (i32.const 48)))
      (br_if $dbreak (i32.gt_u (local.get $c) (i32.const 57)))
      (local.set $result (i64.add
        (i64.mul (local.get $result) (i64.const 10))
        (i64.extend_i32_u (i32.sub (local.get $c) (i32.const 48)))))
      (local.set $i (i32.add (local.get $i) (i32.const 1)))
      (br $dlp)))
    (if (local.get $neg)
      (then (local.set $result (i64.sub (i64.const 0) (local.get $result)))))
    (local.get $result)
  )

  ;; ── sno_int_to_str (val i64) → (off i32, len i32) ─────────────────────────
  (func $sno_int_to_str (export "sno_int_to_str") (param $val i64) (result i32 i32)
    (local $pos i32) (local $start i32) (local $end i32)
    (local $tmp i32) (local $dig i32) (local $neg i32) (local $v i64)
    (local.set $start (global.get $str_ptr))
    (local.set $pos (local.get $start))
    (local.set $neg (i32.wrap_i64 (i64.and
      (i64.shr_u (local.get $val) (i64.const 63)) (i64.const 1))))
    (local.set $v (local.get $val))
    (if (local.get $neg)
      (then (local.set $v (i64.sub (i64.const 0) (local.get $v)))))
    (if (i64.eqz (local.get $v))
      (then
        (i32.store8 (local.get $pos) (i32.const 48))
        (local.set $pos (i32.add (local.get $pos) (i32.const 1))))
      (else
        (block $dbreak (loop $dlp
          (br_if $dbreak (i64.eqz (local.get $v)))
          (local.set $dig (i32.wrap_i64 (i64.rem_u (local.get $v) (i64.const 10))))
          (i32.store8 (local.get $pos) (i32.add (local.get $dig) (i32.const 48)))
          (local.set $pos (i32.add (local.get $pos) (i32.const 1)))
          (local.set $v (i64.div_u (local.get $v) (i64.const 10)))
          (br $dlp)))
        (if (local.get $neg)
          (then
            (i32.store8 (local.get $pos) (i32.const 45))
            (local.set $pos (i32.add (local.get $pos) (i32.const 1)))))
        (local.set $end (i32.sub (local.get $pos) (i32.const 1)))
        (local.set $tmp (local.get $start))
        (block $rbreak (loop $rlp
          (br_if $rbreak (i32.ge_u (local.get $tmp) (local.get $end)))
          (local.set $dig (i32.load8_u (local.get $tmp)))
          (i32.store8 (local.get $tmp) (i32.load8_u (local.get $end)))
          (i32.store8 (local.get $end) (local.get $dig))
          (local.set $tmp (i32.add (local.get $tmp) (i32.const 1)))
          (local.set $end (i32.sub (local.get $end) (i32.const 1)))
          (br $rlp)))))
    (global.set $str_ptr (local.get $pos))
    (local.get $start)
    (i32.sub (local.get $pos) (local.get $start))
  )

  ;; ── sno_float_to_str (val f64) → (off i32, len i32) ──────────────────────
  ;; SNOBOL4 format: 1.0 → "1.", 1.5 → "1.5", strips trailing fractional zeros.
  (func $sno_float_to_str (export "sno_float_to_str") (param $val f64) (result i32 i32)
    (local $start i32) (local $pos i32) (local $neg i32)
    (local $ipart i64) (local $fpart f64) (local $fscale f64)
    (local $fint i64) (local $fdig i32) (local $fstart i32) (local $fend i32)
    (local $tmp i32) (local $dig i32)
    (local.set $start (global.get $str_ptr))
    (local.set $pos (local.get $start))
    ;; negative?
    (local.set $neg (f64.lt (local.get $val) (f64.const 0)))
    (if (local.get $neg)
      (then
        (local.set $val (f64.neg (local.get $val)))
        (i32.store8 (local.get $pos) (i32.const 45))
        (local.set $pos (i32.add (local.get $pos) (i32.const 1)))))
    ;; integer part
    (local.set $ipart (i64.trunc_f64_s (local.get $val)))
    (if (i64.eqz (local.get $ipart))
      (then
        (i32.store8 (local.get $pos) (i32.const 48))
        (local.set $pos (i32.add (local.get $pos) (i32.const 1))))
      (else
        (local.set $tmp (local.get $pos))
        (block $ib (loop $il
          (br_if $ib (i64.eqz (local.get $ipart)))
          (local.set $dig (i32.wrap_i64 (i64.rem_u (local.get $ipart) (i64.const 10))))
          (i32.store8 (local.get $pos) (i32.add (local.get $dig) (i32.const 48)))
          (local.set $pos (i32.add (local.get $pos) (i32.const 1)))
          (local.set $ipart (i64.div_u (local.get $ipart) (i64.const 10)))
          (br $il)))
        (local.set $fend (i32.sub (local.get $pos) (i32.const 1)))
        (block $rb (loop $rl
          (br_if $rb (i32.ge_u (local.get $tmp) (local.get $fend)))
          (local.set $dig (i32.load8_u (local.get $tmp)))
          (i32.store8 (local.get $tmp) (i32.load8_u (local.get $fend)))
          (i32.store8 (local.get $fend) (local.get $dig))
          (local.set $tmp (i32.add (local.get $tmp) (i32.const 1)))
          (local.set $fend (i32.sub (local.get $fend) (i32.const 1)))
          (br $rl)))))
    ;; decimal point always present
    (i32.store8 (local.get $pos) (i32.const 46))
    (local.set $pos (i32.add (local.get $pos) (i32.const 1)))
    ;; fractional part (up to 6 significant digits, strip trailing zeros)
    (local.set $fpart (f64.sub (local.get $val) (f64.convert_i64_s
      (i64.trunc_f64_s (local.get $val)))))
    (local.set $fscale (f64.const 1000000))
    (local.set $fint (i64.trunc_f64_s (f64.add
      (f64.mul (local.get $fpart) (local.get $fscale)) (f64.const 0.5))))
    (if (i64.eqz (local.get $fint))
      (then) ;; no fractional digits — trailing dot only (SNOBOL4: 1. format)
      (else
        (local.set $fstart (local.get $pos))
        ;; write 6 digits (with leading zeros)
        (local.set $tmp (i32.const 0))
        (block $sb (loop $sl
          (br_if $sb (i32.ge_u (local.get $tmp) (i32.const 6)))
          (local.set $fdig (i32.wrap_i64 (i64.rem_u (local.get $fint) (i64.const 10))))
          (i32.store8 (i32.add (local.get $pos) (local.get $tmp)) (i32.add (local.get $fdig) (i32.const 48)))
          (local.set $fint (i64.div_u (local.get $fint) (i64.const 10)))
          (local.set $tmp (i32.add (local.get $tmp) (i32.const 1)))
          (br $sl)))
        ;; these 6 digits are reversed — fix them
        (local.set $fend (i32.add (local.get $pos) (i32.const 5)))
        (local.set $tmp (local.get $pos))
        (block $fb (loop $fl
          (br_if $fb (i32.ge_u (local.get $tmp) (local.get $fend)))
          (local.set $fdig (i32.load8_u (local.get $tmp)))
          (i32.store8 (local.get $tmp) (i32.load8_u (local.get $fend)))
          (i32.store8 (local.get $fend) (local.get $fdig))
          (local.set $tmp (i32.add (local.get $tmp) (i32.const 1)))
          (local.set $fend (i32.sub (local.get $fend) (i32.const 1)))
          (br $fl)))
        ;; strip trailing zeros
        (local.set $pos (i32.add (local.get $pos) (i32.const 6)))
        (block $tz (loop $tzl
          (br_if $tz (i32.le_u (local.get $pos) (local.get $fstart)))
          (br_if $tz (i32.ne (i32.load8_u (i32.sub (local.get $pos) (i32.const 1))) (i32.const 48)))
          (local.set $pos (i32.sub (local.get $pos) (i32.const 1)))
          (br $tzl)))))
    (global.set $str_ptr (local.get $pos))
    (local.get $start)
    (i32.sub (local.get $pos) (local.get $start))
  )
)
