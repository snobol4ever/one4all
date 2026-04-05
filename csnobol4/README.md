# csnobol4-2.3.3 — STREAM trace instrumentation patches

Patches to CSNOBOL4 2.3.3 to enable two-way STREAM trace oracle
(identical format to sno4parse's `SNO_TRACE` output).

## What's here

| File | Change |
|------|--------|
| `stream.c` | Adds `g_csno_trace` / `g_csno_trace_fd` globals; emits trace line on every `stream()` call when `SNO_TRACE=1` |
| `main.c` | Opens `/tmp/sno_csno.trace` and sets `g_csno_trace=1` before `snobol4_init()` when `SNO_TRACE=1` |

## How to apply and build

```bash
# Get CSNOBOL4 2.3.3 source
wget https://www.regressive.org/snobol4/csnobol4/curr/snobol4-2.3.3.tar.gz
tar xzf snobol4-2.3.3.tar.gz
cd snobol4-2.3.3

# Apply patches
cp /path/to/x64/csnobol4-2.3.3/stream.c lib/stream.c
cp /path/to/x64/csnobol4-2.3.3/main.c main.c

# Build with trace enabled
./configure
make -j4 COPT="-DTRACE_STREAM -g -O0"
```

## How to use

```bash
# Run with trace
printf "    X = LT(N, 1000000)\nEND\n" > /tmp/lt_test.sno
SNO_TRACE=1 ./snobol4 /tmp/lt_test.sno
# Trace written to /tmp/sno_csno.trace

# Run sno4parse with trace
echo "    X = LT(N, 1000000)" | SNO_TRACE=1 ./sno4parse /dev/stdin 2>/tmp/sn.trace

# Diff — first divergence = root cause
diff /tmp/sno_csno.trace /tmp/sn.trace | head -30
```

## Trace format (identical in both tools)

```
STREAM <table>     [<input_prefix>      ] -> ret=<STOP|EOS|ERROR>  stype=<n>
```

## DYN-88 diagnosis result

Divergence on `LT(N, 1000000)`:
- CS uses `FRWDTB` to position before each arg token
- sno4parse uses `IBLKTB` (wrong) and calls FRWDTB twice on comma
- Fix: arg loop in `ELEFNC` must use FORWRD not FORBLK; single call per boundary
