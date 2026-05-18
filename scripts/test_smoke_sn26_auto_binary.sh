#!/usr/bin/env bash
# Smoke probe for SN-26-auto-binary-scrip (SN-26-bridge-coverage-e revision).
#
# Validates: MONITOR_BIN=1 + SCRIP_TRACE=1 + SCRIP_FTRACE=1 produces a
# binary wire stream with streaming-intern NAME_DEF records inline, no
# source modification, no sidecar names file.
#
# Pipeline:
#   - Make a FIFO pair (ready=script→reader, go=reader→script).
#   - Reader (inline python) drains ready FIFO, builds a names table from
#     MWK_NAME_DEF records, dumps semantic records to disk, and acks 'G'
#     after each record.  EOF / final MWK_END terminates.
#   - Launch scrip on a tiny .sno with the relevant env set.
#   - Verify: semantic records decode to expected event sequence; names
#     resolve; no MONITOR_NAMES_OUT was written.
set -eu
HERE="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
SCRIP="${SCRIP:-$HERE/../scrip}"

if [ ! -x "$SCRIP" ]; then
    echo "FAIL scrip not built at $SCRIP"; exit 1
fi

T=$(mktemp -d)
trap 'rm -rf "$T"' EXIT
mkfifo "$T/ready" "$T/go"

cat > "$T/p.sno" <<'EOF'
                  DEFINE('SQR(x)f')                          :(SQR_END)
SQR               SQR     =  x * x                           :(RETURN)
SQR_END
                  a       =  'hello'
                  b       =  42
                  c       =  3.14
                  d       =  SQR(7)
END
EOF

# Reader: drain ready FIFO, ack 'G' after each record.  Absorb NAME_DEF
# records into the local names table; write semantic records (and the
# resolved names) to disk for assertion checking.
python3 - "$T/ready" "$T/go" "$T/probe_auto.bin" "$T/names.derived" <<'PY' &
import os, sys, struct
ready, go, outpath, namespath = sys.argv[1:5]
fout = open(outpath, "wb")
fr = open(ready, "rb", buffering=0)
fg = open(go, "wb", buffering=0)
names = {}  # id -> bytes
while True:
    hdr = fr.read(13)
    if len(hdr) == 0:
        break
    if len(hdr) < 13:
        sys.stderr.write(f"short hdr: {len(hdr)} bytes\n")
        break
    kind, name_id, t, vlen = struct.unpack("<IIBI", hdr)
    body = fr.read(vlen) if vlen > 0 else b""
    if kind == 6:  # MWK_NAME_DEF
        names[name_id] = body
        try:
            fg.write(b"G"); fg.flush()
        except BrokenPipeError:
            break
        continue
    fout.write(hdr)
    if vlen > 0:
        fout.write(body)
    fout.flush()
    try:
        fg.write(b"G"); fg.flush()
    except BrokenPipeError:
        break
    if kind == 4:  # MWK_END
        break
fout.close()
# Dump names sidecar derived from the wire (max-id+1 lines).
max_id = max(names) if names else -1
with open(namespath, "w") as nf:
    for i in range(max_id + 1):
        nf.write(names.get(i, b"").decode("utf-8", "backslashreplace") + "\n")
PY
READER_PID=$!

MONITOR_BIN=1 \
MONITOR_READY_PIPE="$T/ready" MONITOR_GO_PIPE="$T/go" \
SCRIP_TRACE=1 SCRIP_FTRACE=1 \
timeout 10 "$SCRIP" --interp "$T/p.sno" </dev/null > "$T/scrip.out" 2> "$T/scrip.err"
SCRIP_RC=$?

wait $READER_PID 2>/dev/null

if [ ! -s "$T/probe_auto.bin" ]; then
    echo "FAIL no binary records captured (scrip rc=$SCRIP_RC)"
    echo "--- scrip stderr ---"; cat "$T/scrip.err"
    exit 1
fi

# Decode: semantic records only (NAME_DEFs are already absorbed by reader).
python3 - "$T/probe_auto.bin" "$T/names.derived" <<'PY'
import sys, struct
binp, namesp = sys.argv[1], sys.argv[2]
with open(binp, "rb") as f:
    data = f.read()
with open(namesp) as f:
    nm = [ln.rstrip("\n") for ln in f]
print(f"names (from wire): {nm}")
i = 0
records = []
KINDS = {1:"VALUE", 2:"CALL", 3:"RETURN", 4:"END", 5:"LABEL"}
TYPES = {0:"NULL",1:"STRING",2:"INTEGER",3:"REAL",4:"NAME",5:"PATTERN",
         6:"EXPRESSION",7:"ARRAY",8:"TABLE",9:"CODE",10:"DATA",11:"FILE",255:"UNKNOWN"}
while i + 13 <= len(data):
    kind, name_id, t, vlen = struct.unpack("<IIBI", data[i:i+13])
    val = data[i+13:i+13+vlen]
    nm_str = nm[name_id] if 0 <= name_id < len(nm) else f"<id={name_id}>"
    if kind == 4:
        nm_str = "<END>"
    if t == 1 or t == 4:
        valdisp = repr(val.decode("utf-8", "replace"))
    elif t == 2:
        valdisp = struct.unpack("<q", val)[0] if vlen == 8 else "?"
    elif t == 3:
        valdisp = struct.unpack("<d", val)[0] if vlen == 8 else "?"
    else:
        valdisp = ""
    print(f"  {KINDS.get(kind,'?'):7s} {nm_str:20s} type={TYPES.get(t,'?')} {valdisp}")
    records.append((KINDS.get(kind,'?'), nm_str, TYPES.get(t,'?'), val))
    i += 13 + vlen

# Assertions: semantic records present, names resolve, NAME_DEFs not in semantic
# stream, MONITOR_NAMES_OUT was NOT consulted (we never set it).
kinds = [r[0] for r in records]
assert "VALUE"  in kinds, "no VALUE records"
assert "CALL"   in kinds, "no CALL record"
assert "RETURN" in kinds, "no RETURN record"
assert kinds[-1] == "END", "missing trailing END"
assert "NAME_DEF" not in kinds, "NAME_DEF leaked into semantic stream"
nameset = set(r[1] for r in records if r[0] != "END")
for required in ["a", "b", "c", "d", "SQR"]:
    assert required in nameset, f"missing trace for {required}"
print(f"OK  records={len(records)} names_on_wire={len(nm)}")
PY

# Verify no sidecar file was written by the runtime — streaming intern means
# the runtime never honors MONITOR_NAMES_OUT, but make extra sure here.
if [ -f "$T/names.out" ]; then
    echo "FAIL runtime wrote MONITOR_NAMES_OUT sidecar — should be on the wire instead"
    exit 1
fi

echo "PASS sn26_auto_binary smoke"
echo "PASS=1 FAIL=0"
