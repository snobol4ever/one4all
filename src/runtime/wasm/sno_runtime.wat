;; sno_runtime.wat — SNOBOL4 WASM runtime fragment
;; Inlined verbatim into every .wat produced by emit_wasm.c via emit_wasm_runtime_header().
;;
;; Memory layout (1 page = 64KB):
;;   [0  .. 32767]  output buffer  — written by sno_output_*; main() returns fill length
;;   [32768 .. 49151]  string heap — sno_str_alloc() bumps $str_ptr upward
;;   [49152 .. 65535]  variable table (reserved for M-SW-A04)
;;
;; Output protocol:
;;   SNOBOL4 OUTPUT = expr  →  sno_output_str / sno_output_int
;;   main() returns i32 byte-count written to [0..N-1]
;;   run_wasm.js writes exactly those bytes to stdout
;;
;; All string values are (offset: i32, len: i32) pairs in linear memory.
;;
;; Milestone: M-SW-1
;; Author: Claude Sonnet 4.6 (SW-1, 2026-03-30)

  ;; ── Memory ────────────────────────────────────────────────────────────────
  (memory (export "memory") 1)

  ;; output buffer write position
  (global $out_pos (mut i32) (i32.const 0))

  ;; string heap bump pointer
  (global $str_ptr (mut i32) (i32.const 32768))

  ;; ── sno_output_str (offset: i32, len: i32) ────────────────────────────────
  ;; Append string at [offset..offset+len) then '\n' to output buffer.
  (func $sno_output_str (param $offset i32) (param $len i32)
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
    ;; append newline
    (i32.store8
      (i32.add (local.get $dst) (local.get $len))
      (i32.const 10))
    (global.set $out_pos
      (i32.add (local.get $dst) (i32.add (local.get $len) (i32.const 1))))
  )

  ;; ── sno_output_int (val: i64) ─────────────────────────────────────────────
  ;; Format val as decimal string, append to output buffer with '\n'.
  (func $sno_output_int (param $val i64)
    (local $pos i32)
    (local $start i32)
    (local $end i32)
    (local $tmp i32)
    (local $neg i32)
    (local $v i64)
    (local $digit i32)
    (local.set $pos (global.get $out_pos))
    (local.set $start (local.get $pos))
    (local.set $neg (i32.const 0))
    (local.set $v (local.get $val))
    ;; handle negative
    (if (i64.lt_s (local.get $v) (i64.const 0))
      (then
        (local.set $neg (i32.const 1))
        (local.set $v (i64.sub (i64.const 0) (local.get $v)))))
    ;; handle zero
    (if (i64.eqz (local.get $v))
      (then
        (i32.store8 (local.get $pos) (i32.const 48))  ;; '0'
        (local.set $pos (i32.add (local.get $pos) (i32.const 1))))
      (else
        ;; write digits in reverse
        (block $dbreak
          (loop $digits
            (br_if $dbreak (i64.eqz (local.get $v)))
            (local.set $digit
              (i32.wrap_i64 (i64.rem_u (local.get $v) (i64.const 10))))
            (i32.store8 (local.get $pos)
              (i32.add (local.get $digit) (i32.const 48)))
            (local.set $pos (i32.add (local.get $pos) (i32.const 1)))
            (local.set $v (i64.div_u (local.get $v) (i64.const 10)))
            (br $digits)
          )
        )
        ;; write minus sign if negative
        (if (local.get $neg)
          (then
            (i32.store8 (local.get $pos) (i32.const 45))  ;; '-'
            (local.set $pos (i32.add (local.get $pos) (i32.const 1)))))
        ;; reverse the digit string in place
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
    ;; append newline
    (i32.store8 (local.get $pos) (i32.const 10))
    (global.set $out_pos (i32.add (local.get $pos) (i32.const 1)))
  )

  ;; ── sno_output_flush () → i32 ─────────────────────────────────────────────
  ;; Return number of bytes written to output buffer. Called by main().
  (func $sno_output_flush (result i32)
    (global.get $out_pos)
  )

  ;; ── sno_str_alloc (len: i32) → i32 ───────────────────────────────────────
  ;; Bump-allocate len bytes in string heap. Returns offset.
  (func $sno_str_alloc (param $len i32) (result i32)
    (local $ptr i32)
    (local.set $ptr (global.get $str_ptr))
    (global.set $str_ptr (i32.add (local.get $ptr) (local.get $len)))
    (local.get $ptr)
  )

  ;; ── sno_str_concat (a_off: i32, a_len: i32, b_off: i32, b_len: i32)
  ;;                  → (result_off: i32, result_len: i32) ──────────────────
  ;; Concatenate two strings into string heap. Returns (offset, len) as two i32.
  (func $sno_str_concat
    (param $a_off i32) (param $a_len i32)
    (param $b_off i32) (param $b_len i32)
    (result i32 i32)
    (local $total i32)
    (local $dst i32)
    (local $i i32)
    (local.set $total (i32.add (local.get $a_len) (local.get $b_len)))
    (local.set $dst (call $sno_str_alloc (local.get $total)))
    ;; copy a
    (local.set $i (i32.const 0))
    (block $da
      (loop $la
        (br_if $da (i32.ge_u (local.get $i) (local.get $a_len)))
        (i32.store8
          (i32.add (local.get $dst) (local.get $i))
          (i32.load8_u (i32.add (local.get $a_off) (local.get $i))))
        (local.set $i (i32.add (local.get $i) (i32.const 1)))
        (br $la)
      )
    )
    ;; copy b
    (local.set $i (i32.const 0))
    (block $db
      (loop $lb
        (br_if $db (i32.ge_u (local.get $i) (local.get $b_len)))
        (i32.store8
          (i32.add (local.get $dst) (i32.add (local.get $a_len) (local.get $i)))
          (i32.load8_u (i32.add (local.get $b_off) (local.get $i))))
        (local.set $i (i32.add (local.get $i) (i32.const 1)))
        (br $lb)
      )
    )
    (local.get $dst)
    (local.get $total)
  )
