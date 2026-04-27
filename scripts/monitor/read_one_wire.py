#!/usr/bin/env python3
"""
read_one_wire.py — minimal single-participant wire reader for smoke testing
the SN-26-csn-bridge runtime.  Not intended for production sync-step work
(use scripts/monitor/monitor_sync_bin.py for that).

Spec:  NAME:READY:GO[:NAMES]
- creates the two FIFOs
- spawns nothing; the caller launches the participant separately
- reads records, prints each, sends 'G' ack after each non-END record
- exits when MWK_END is seen or pipe closes
"""
import os, sys, struct, time, errno

KIND_NAMES = {1: "VALUE", 2: "CALL", 3: "RETURN", 4: "END"}
TYPE_NAMES = {0: "NULL", 1: "STRING", 2: "INTEGER", 3: "REAL", 4: "NAME",
              5: "PATTERN", 6: "EXPRESSION", 7: "ARRAY", 8: "TABLE",
              9: "CODE", 10: "DATA", 11: "FILE", 255: "UNKNOWN"}

def main():
    if len(sys.argv) < 4:
        print("usage: read_one_wire.py READY_FIFO GO_FIFO [NAMES_FILE]", file=sys.stderr)
        sys.exit(2)
    ready_path = sys.argv[1]
    go_path    = sys.argv[2]
    names_path = sys.argv[3] if len(sys.argv) > 3 else None

    for p in (ready_path, go_path):
        if os.path.exists(p): os.unlink(p)
        os.mkfifo(p)

    # Open ready as reader, go as writer.  Reader-side open blocks until
    # the participant opens the write end.
    print(f"[ctrl] waiting for participant to open {ready_path}...", file=sys.stderr)
    ready_fd = os.open(ready_path, os.O_RDONLY)
    print(f"[ctrl] participant connected, opening go FIFO...", file=sys.stderr)
    go_fd    = os.open(go_path, os.O_WRONLY)
    print(f"[ctrl] go FIFO open, reading wire...", file=sys.stderr)

    n_records = 0
    while True:
        hdr = b""
        while len(hdr) < 13:
            chunk = os.read(ready_fd, 13 - len(hdr))
            if not chunk:
                print(f"[ctrl] EOF on ready fd after {n_records} records", file=sys.stderr)
                break
            hdr += chunk
        if len(hdr) < 13:
            break

        kind, name_id, t, vlen = struct.unpack("<IIBI", hdr)
        value = b""
        while len(value) < vlen:
            chunk = os.read(ready_fd, vlen - len(value))
            if not chunk: break
            value += chunk

        kn = KIND_NAMES.get(kind, f"?{kind}")
        tn = TYPE_NAMES.get(t,    f"?{t}")
        if t == 2 and len(value) == 8:    # INTEGER
            (ival,) = struct.unpack("<q", value)
            vrepr = f"INTEGER({ival})"
        elif t == 3 and len(value) == 8:  # REAL
            (rval,) = struct.unpack("<d", value)
            vrepr = f"REAL({rval})"
        elif t in (1, 4):                  # STRING / NAME
            vrepr = f"{tn}({vlen})={value!r}"
        else:
            vrepr = f"{tn}(empty)"

        print(f"[ctrl] #{n_records:03d} kind={kn} name_id={name_id} {vrepr}",
              file=sys.stderr)
        n_records += 1

        if kind == 4:   # END
            break
        # Send 'G' ack
        try:
            os.write(go_fd, b"G")
        except OSError as e:
            print(f"[ctrl] ack write failed: {e}", file=sys.stderr)
            break

    os.close(ready_fd)
    os.close(go_fd)
    print(f"[ctrl] read {n_records} records total", file=sys.stderr)

    if names_path and os.path.exists(names_path):
        print(f"[ctrl] names sidecar at {names_path}:", file=sys.stderr)
        with open(names_path) as f:
            for i, line in enumerate(f):
                print(f"[ctrl]   id={i}: {line.rstrip()}", file=sys.stderr)

if __name__ == "__main__":
    main()
