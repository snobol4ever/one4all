#!/bin/bash
# run_monitor.sh <sno_file> [tracepoints_conf]
#
# IPC monitor: CSNOBOL4 (oracle) + snobol4x ASM backend via named FIFOs.
# Each participant writes trace events to its own FIFO via monitor_ipc.so
# (CSNOBOL4) or MONITOR_FIFO env var (ASM backend comm_var).
# No stderr/stdout blending. Parallel execution ready.
#
# Expands to 5-way (+SPITBOL+JVM+NET) in M-MONITOR-IPC-5WAY.
#
# Exit 0 = PASS. Exit 1 = FAIL.

set -euo pipefail

SNO=${1:?Usage: run_monitor.sh <sno_file> [tracepoints_conf]}
CONF=${2:-$(dirname "$0")/tracepoints.conf}

DIR=$(cd "$(dirname "$0")/../.." && pwd)   # snobol4x root
RT=$DIR/src/runtime
INC=/home/claude/snobol4corpus/programs/inc
SO=$(dirname "$0")/monitor_ipc.so
TMP=$(mktemp -d /tmp/monitor_XXXXXX)
trap 'rm -rf "$TMP"' EXIT

SNO_BASE=$(basename "$SNO" .sno)

# ------------------------------------------------------------------
# Step 1: Inject TRACE() callbacks → instrumented .sno for CSNOBOL4
# ------------------------------------------------------------------
python3 "$(dirname "$0")/inject_traces.py" "$SNO" "$CONF" > "$TMP/instr.sno"

# ------------------------------------------------------------------
# Step 2: Create named FIFOs
# ------------------------------------------------------------------
mkfifo "$TMP/csn.fifo"
mkfifo "$TMP/asm.fifo"

# ------------------------------------------------------------------
# Step 3: Start FIFO collectors (background cat → file)
# ------------------------------------------------------------------
cat "$TMP/csn.fifo" > "$TMP/csn.trace" &
CSN_CAT=$!
cat "$TMP/asm.fifo" > "$TMP/asm.trace" &
ASM_CAT=$!

# ------------------------------------------------------------------
# Step 4: Run CSNOBOL4 on instrumented .sno via IPC FIFO
#   stdout = program output (kept for functional check)
#   trace → csn.fifo via monitor_ipc.so
# ------------------------------------------------------------------
MONITOR_FIFO="$TMP/csn.fifo" MONITOR_SO="$SO" \
    snobol4 -f -P256k -I"$INC" "$TMP/instr.sno" \
    < /dev/null \
    > "$TMP/csn.out" 2>"$TMP/csn.stderr" || true

# ------------------------------------------------------------------
# Step 5: Build ASM binary and run with MONITOR_FIFO
# ------------------------------------------------------------------
gcc -O0 -g -c "$RT/asm/snobol4_stmt_rt.c" \
    -I"$RT/snobol4" -I"$RT" -I"$DIR/src/frontend/snobol4" -w \
    -o "$TMP/stmt_rt.o"
gcc -O0 -g -c "$RT/snobol4/snobol4.c" \
    -I"$RT/snobol4" -I"$RT" -I"$DIR/src/frontend/snobol4" -w \
    -o "$TMP/snobol4.o"
gcc -O0 -g -c "$RT/mock/mock_includes.c" \
    -I"$RT/snobol4" -I"$RT" -I"$DIR/src/frontend/snobol4" -w \
    -o "$TMP/mock.o"
gcc -O0 -g -c "$RT/snobol4/snobol4_pattern.c" \
    -I"$RT/snobol4" -I"$RT" -I"$DIR/src/frontend/snobol4" -w \
    -o "$TMP/pat.o"
gcc -O0 -g -c "$RT/engine/engine.c" \
    -I"$RT/snobol4" -I"$RT" -I"$DIR/src/frontend/snobol4" -w \
    -o "$TMP/eng.o"

"$DIR/sno2c" -asm "$SNO" > "$TMP/prog.s" 2>"$TMP/compile.err" || {
    echo "FAIL [ASM compile] $SNO_BASE"
    cat "$TMP/compile.err" | head -10
    wait $CSN_CAT $ASM_CAT 2>/dev/null || true; exit 1
}

nasm -f elf64 -I"$RT/asm/" "$TMP/prog.s" -o "$TMP/prog.o" 2>"$TMP/nasm.err" || {
    echo "FAIL [ASM assemble] $SNO_BASE"
    cat "$TMP/nasm.err" | head -10
    wait $CSN_CAT $ASM_CAT 2>/dev/null || true; exit 1
}

gcc -no-pie "$TMP/prog.o" \
    "$TMP/stmt_rt.o" "$TMP/snobol4.o" "$TMP/mock.o" "$TMP/pat.o" "$TMP/eng.o" \
    -lgc -lm -o "$TMP/prog_asm" 2>"$TMP/link.err" || {
    echo "FAIL [ASM link] $SNO_BASE"
    cat "$TMP/link.err" | head -10
    wait $CSN_CAT $ASM_CAT 2>/dev/null || true; exit 1
}

MONITOR_FIFO="$TMP/asm.fifo" \
    "$TMP/prog_asm" \
    < /dev/null \
    > "$TMP/asm.out" 2>"$TMP/asm.stderr" || true

# ------------------------------------------------------------------
# Step 6: Close FIFOs — EOF signals collectors to finish
# ------------------------------------------------------------------
# Both participants have exited; FIFOs now at EOF. Wait for collectors.
wait $CSN_CAT $ASM_CAT 2>/dev/null || true

# ------------------------------------------------------------------
# Step 7: Normalize both streams
# ------------------------------------------------------------------
python3 "$(dirname "$0")/normalize_trace.py" "$CONF" \
    "$TMP/csn.trace" "$TMP/asm.trace" \
    "$TMP/csn.norm"  "$TMP/asm.norm"

# ------------------------------------------------------------------
# Step 8: Sanity check — CSNOBOL4 stream must be non-empty
# ------------------------------------------------------------------
if [ ! -s "$TMP/csn.norm" ]; then
    echo "WARN [csnobol4] empty trace stream for $SNO_BASE"
fi

# ------------------------------------------------------------------
# Step 9: Diff
# ------------------------------------------------------------------
FAIL=0
DIFF=$(diff "$TMP/csn.norm" "$TMP/asm.norm" || true)
if [ -z "$DIFF" ]; then
    echo "PASS [ASM] $SNO_BASE"
else
    echo "FAIL [ASM] $SNO_BASE"
    echo "$DIFF" | head -10
    FAIL=1
fi

exit $FAIL
