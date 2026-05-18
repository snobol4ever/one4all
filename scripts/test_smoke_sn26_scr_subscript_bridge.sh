#!/usr/bin/env bash
# test_smoke_sn26_scr_subscript_bridge.sh — smoke test for SN-26-bridge-coverage-g.
#
# Validates that scrip's binary monitor wire emits VALUE records for
# subscript-set (array/table store) statements, with the real base variable
# name (e.g. "a", "d") — not an <lval> sentinel — on the wire.
#
# Probe: a<1>='x' / a<2>='y' / d<'k'>='z'  →  3 VALUE records, names a, a, d.
#
# Per RULES.md self-contained scripts: paths from $0; SKIP cleanly if deps missing.
set -eu
HERE="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
SCRIP="${SCRIP:-$HERE/../scrip}"
MONITOR_DIR="${MONITOR_DIR:-$HERE/monitor}"

if [ ! -x "$SCRIP" ]; then
    echo "SKIP scrip not built at $SCRIP"; exit 0
fi

T=$(mktemp -d)
trap 'rm -rf "$T"' EXIT

mkfifo "$T/ready" "$T/go"

cat > "$T/probe.sno" <<'EOF'
                  &ANCHOR   = 0
                  &FULLSCAN = 1
                  a         = ARRAY('1:4')
                  d         = TABLE()
                  a<1>      = 'x'
                  a<2>      = 'y'
                  d<'k'>    = 'z'
END
EOF

# Reader: absorb NAME_DEF records, ack all records with G, collect semantic records.
python3 - "$T/ready" "$T/go" "$T/out.bin" "$T/names.txt" <<'PY' &
import os, sys, struct
ready_path, go_path, outpath, namespath = sys.argv[1:5]
fout  = open(outpath, "wb")
fr    = open(ready_path, "rb", buffering=0)
fg    = open(go_path,    "wb", buffering=0)
names = {}
while True:
    hdr = fr.read(13)
    if len(hdr) == 0: break
    if len(hdr) < 13: break
    kind, name_id, t, vlen = struct.unpack("<IIBI", hdr)
    body = fr.read(vlen) if vlen > 0 else b""
    if kind == 6:  # MWK_NAME_DEF
        names[name_id] = body
        try: fg.write(b"G"); fg.flush()
        except BrokenPipeError: break
        continue
    fout.write(hdr)
    if vlen > 0: fout.write(body)
    fout.flush()
    try: fg.write(b"G"); fg.flush()
    except BrokenPipeError: break
    if kind == 4: break  # MWK_END
fout.close()
max_id = max(names) if names else -1
with open(namespath, "w") as nf:
    for i in range(max_id + 1):
        nf.write(names.get(i, b"").decode("utf-8", "backslashreplace") + "\n")
PY
READER_PID=$!

MONITOR_BIN=1 \
MONITOR_READY_PIPE="$T/ready" MONITOR_GO_PIPE="$T/go" \
SCRIP_TRACE=1 SCRIP_FTRACE=1 \
timeout 10 "$SCRIP" --interp "$T/probe.sno" </dev/null >"$T/scrip.out" 2>"$T/scrip.err"
SCRIP_RC=$?

wait $READER_PID 2>/dev/null

if [ ! -s "$T/out.bin" ]; then
    echo "FAIL no binary records captured (scrip rc=$SCRIP_RC)"
    echo "--- scrip stderr ---"; cat "$T/scrip.err"
    exit 1
fi

# Decode and assert.
python3 - "$T/out.bin" "$T/names.txt" <<'PY'
import sys, struct
binp, namesp = sys.argv[1], sys.argv[2]
with open(binp, "rb") as f: data = f.read()
with open(namesp) as f: nm = [ln.rstrip("\n") for ln in f]

KINDS = {1:"VALUE",2:"CALL",3:"RETURN",4:"END",5:"LABEL",6:"NAME_DEF"}
TYPES = {0:"NULL",1:"STRING",2:"INTEGER",3:"REAL",4:"NAME",5:"PATTERN",
         7:"ARRAY",8:"TABLE",255:"UNKNOWN"}

i = 0
records = []
while i + 13 <= len(data):
    kind, name_id, t, vlen = struct.unpack("<IIBI", data[i:i+13])
    val = data[i+13:i+13+vlen]
    nm_str = nm[name_id] if 0 <= name_id < len(nm) else f"<id={name_id}>"
    if kind == 4: nm_str = "<END>"
    if t in (1,4):
        vdisp = val.decode("utf-8","replace")
    elif t == 2 and vlen == 8:
        vdisp = str(struct.unpack("<q", val)[0])
    else:
        vdisp = TYPES.get(t, f"T{t}")
    print(f"  {KINDS.get(kind,'?'):7s} {nm_str:20s} type={TYPES.get(t,f'T{t}'):8s} {vdisp}")
    records.append((KINDS.get(kind,'?'), nm_str, TYPES.get(t,'?'), val))
    i += 13 + vlen

# Assertions:
# Expect 3 VALUE records for subscript stores on a, a, d.
value_recs = [(k,n,t,v) for k,n,t,v in records if k=="VALUE"]
subscript_values = [(n,t,v) for k,n,t,v in records if k=="VALUE" and n in ("a","d") and t=="STRING"]

assert len(subscript_values) >= 3, \
    f"Expected >=3 VALUE records for a/d subscript stores, got {subscript_values}"

names_seen = [n for n,t,v in subscript_values[:3]]
assert names_seen == ["a","a","d"], \
    f"Expected names [a,a,d] for first 3 subscript VALUE records, got {names_seen}"

assert "NAME_DEF" not in [k for k,n,t,v in records], \
    "NAME_DEF leaked into semantic stream"

print(f"OK  records={len(records)} subscript_values={len(subscript_values)} names_on_wire={len(nm)}")
PY

echo "PASS sn26_scr_subscript_bridge"
echo "PASS=1 FAIL=0"
