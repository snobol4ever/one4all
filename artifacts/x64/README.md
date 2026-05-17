# artifacts/x64/ -- Mode-4 x86-64 Generated Assembly (one4all mirror)

## Canonical location

The tracked .s artifacts live side-by-side with their .sno sources in
the corpus repo:

    corpus/programs/snobol4/demo/roman.s
    corpus/programs/snobol4/demo/wordcount.s

Those two are the inspection targets. Git history there is the emitter
evolution record.

## This directory (one4all/artifacts/x64/)

    beauty_prog.s    Generated from beauty.sno (4700+ SM instructions).
                     Not for line-by-line inspection -- used for:
                       (a) assembly check (gcc -c must succeed), and
                       (b) EM-7 gate (zero UNHANDLED_OP lines).
    samples/         Benchmark variants of demo programs (benchmark
                     harness versions, not the canonical demo versions).
                     Secondary -- corpus/demo/ is authoritative.

## Protocol

See GOAL-MODE4-EMIT.md section "Tracked Artifacts -- Protocol" for
the definitive regen + commit procedure.

Short form: at end of every session touching the emitter --

    cd /home/claude/one4all
    DEMO=/home/claude/corpus/programs/snobol4/demo
    ./scrip --compile $DEMO/roman.sno    > $DEMO/roman.s    2>/dev/null
    ./scrip --compile $DEMO/wordcount.sno > $DEMO/wordcount.s 2>/dev/null
    # verify both assemble, then commit corpus if changed

## EM-7 beauty check

    ./scrip --compile /home/claude/corpus/programs/snobol4/demo/beauty.sno \
        > artifacts/x64/beauty_prog.s 2>/dev/null
    gcc -c artifacts/x64/beauty_prog.s -o /dev/null   # must be clean
    grep -c "UNHANDLED_OP" artifacts/x64/beauty_prog.s  # must be 0 at EM-7
