#!/usr/bin/env python3
"""monitor_sync.py — 2-participant sync-step barrier controller

Usage:
    monitor_sync.py <csn_evt> <csn_ack> <sly_evt> <sly_ack> [--timeout N]

Reads one event from each participant per step. Compares. Sends G (go) or
S (stop). Prints MATCH, DIVERGE, TIMEOUT, or EOF lines to stdout.

Exit codes:
    0 — both participants reached EOF cleanly (PASS)
    1 — divergence detected
    2 — timeout (infinite loop in one or both participants)
    3 — error (FIFO open failed, etc.)
"""

import sys
import os
import select
import time
import signal

TIMEOUT_DEFAULT = 10   # seconds of FIFO silence → infinite loop declared

def open_fifos(csn_evt, csn_ack, sly_evt, sly_ack):
    """Open all four FIFOs. Order matters: open evt read-side before ack write-side
    so participant open(O_WRONLY) on evt unblocks, then we open ack write-side
    so participant open(O_RDONLY) on ack unblocks."""
    # evt FIFOs: we read, participants write (open O_WRONLY blocks until we open O_RDONLY)
    csn_ef = open(csn_evt, 'r')
    sly_ef = open(sly_evt, 'r')
    # ack FIFOs: we write, participants read (open O_RDONLY blocks until we open O_WRONLY)
    csn_af = open(csn_ack, 'w')
    sly_af = open(sly_ack, 'w')
    return csn_ef, csn_af, sly_ef, sly_af


def send_ack(f, code):
    f.write(code)
    f.flush()


def run(csn_evt, csn_ack, sly_evt, sly_ack, timeout):
    try:
        csn_ef, csn_af, sly_ef, sly_af = open_fifos(csn_evt, csn_ack, sly_evt, sly_ack)
    except Exception as e:
        print(f"ERROR opening FIFOs: {e}", flush=True)
        return 3

    alive = {'csn': True, 'sly': True}
    last_event = {'csn': '<none>', 'sly': '<none>'}
    last_time = {'csn': time.monotonic(), 'sly': time.monotonic()}
    step = 0
    pending = {'csn': None, 'sly': None}
    fds = {csn_ef.fileno(): ('csn', csn_ef), sly_ef.fileno(): ('sly', sly_ef)}

    while alive['csn'] or alive['sly']:
        # Build list of FDs still open
        open_fds = [fd for fd, (name, _) in fds.items() if alive[name]]
        if not open_fds:
            break

        # Wait with timeout
        try:
            ready, _, _ = select.select(open_fds, [], [], timeout)
        except Exception as e:
            print(f"ERROR select: {e}", flush=True)
            return 3

        now = time.monotonic()

        if not ready:
            # Global timeout — check per-participant
            for name in ('csn', 'sly'):
                if alive[name] and (now - last_time[name]) > timeout:
                    print(f"TIMEOUT [{name}] after step {step}", flush=True)
                    print(f"  last event: {last_event[name]!r}", flush=True)
                    print(f"  → infinite loop between this event and the next", flush=True)
                    # Send stop to the other participant if still alive
                    for n2, af in (('csn', csn_af), ('sly', sly_af)):
                        if alive[n2] and pending[n2] is not None:
                            send_ack(af, 'S')
                    return 2

        for fd in ready:
            name, ef = fds[fd]
            line = ef.readline()
            if line == '':
                # EOF — participant exited cleanly
                alive[name] = False
                print(f"EOF [{name}]", flush=True)
                continue
            line = line.rstrip('\n')
            last_event[name] = line
            last_time[name] = now
            pending[name] = line

        # If both have a pending event, compare and ack
        if pending['csn'] is not None and pending['sly'] is not None:
            step += 1
            csn_line = pending['csn']
            sly_line = pending['sly']
            pending['csn'] = None
            pending['sly'] = None

            if csn_line == sly_line:
                print(f"MATCH  {csn_line}", flush=True)
                send_ack(csn_af, 'G')
                send_ack(sly_af, 'G')
            else:
                print(f"DIVERGE at step {step}:", flush=True)
                print(f"  CSN: {csn_line!r}", flush=True)
                print(f"  SLY: {sly_line!r}", flush=True)
                send_ack(csn_af, 'S')
                send_ack(sly_af, 'S')
                return 1

        # If one exited cleanly but the other still has events — drain and report
        for name, af, other in (('csn', csn_af, 'sly'), ('sly', sly_af, 'csn')):
            if not alive[name] and alive[other] and pending[other] is not None:
                print(f"DIVERGE at step {step}: [{name}] exited, [{other}] still running", flush=True)
                print(f"  {other} pending: {pending[other]!r}", flush=True)
                other_af = csn_af if other == 'csn' else sly_af
                send_ack(other_af, 'S')
                return 1

    print(f"PASS — both participants completed {step} steps", flush=True)
    return 0


if __name__ == '__main__':
    import argparse
    ap = argparse.ArgumentParser()
    ap.add_argument('csn_evt')
    ap.add_argument('csn_ack')
    ap.add_argument('sly_evt')
    ap.add_argument('sly_ack')
    ap.add_argument('--timeout', type=float, default=TIMEOUT_DEFAULT)
    args = ap.parse_args()
    sys.exit(run(args.csn_evt, args.csn_ack, args.sly_evt, args.sly_ack, args.timeout))
