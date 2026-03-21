#!/usr/bin/env python3
"""
monitor_collect.py — parallel FIFO collector with per-participant watchdog.

Usage:
    python3 monitor_collect.py [--timeout T] [--pids name:pid,...] \\
        [--ready-fd N] \\
        fifo1 fifo2 ... fifoN outfile1 outfile2 ... outfileN

Handshake protocol (--ready-fd):
    Caller creates a pipe.  Passes write-end fd via --ready-fd.
    After ALL FIFOs are open for reading, collector writes one byte 'R'
    to that fd and closes it.  Caller blocks on read-end until 'R' arrives,
    then launches participants.  Zero-race: participants never try to open
    a FIFO write-end before a reader is present.

The first N args after options are FIFO paths; the next N are output files.
N must be equal halves of the positional args list.

Per-participant watchdog: if a FIFO is silent for more than T seconds between
events, that participant is declared hung (infinite loop / deadlock):
  - Its PID is killed (if --pids provided)
  - TIMEOUT line printed to stdout
  - Its output file is closed with EOF

Exit 0 = all participants exited cleanly.
Exit 1 = any participant timed out.
"""

import sys
import os
import select
import time
import signal
import argparse


INTER_EVENT_TIMEOUT = 10


def parse_args():
    p = argparse.ArgumentParser()
    p.add_argument('--timeout', type=float, default=INTER_EVENT_TIMEOUT)
    p.add_argument('--pids', type=str, default='')
    p.add_argument('--ready-fd-path', type=str, default=None,
                   help='Path to a named FIFO; collector opens it for writing,'
                        ' writes 1 byte R when all data FIFOs are open, then '
                        'closes it.')
    p.add_argument('positional', nargs='+')
    args = p.parse_args()

    pos = args.positional
    if len(pos) % 2 != 0:
        print('ERROR: must supply equal numbers of FIFOs and output files',
              file=sys.stderr)
        sys.exit(2)
    n = len(pos) // 2
    fifos    = pos[:n]
    outfiles = pos[n:]

    pid_map = {}
    if args.pids:
        for token in args.pids.split(','):
            token = token.strip()
            if ':' in token:
                name, pid = token.split(':', 1)
                try:
                    pid_map[name] = int(pid)
                except ValueError:
                    pass

    return args.timeout, pid_map, args.ready_fd_path, fifos, outfiles


class Participant:
    def __init__(self, name, fifo_path, outfile_path, pid=None):
        self.name       = name
        self.fifo_path  = fifo_path
        self.pid        = pid
        self.fd         = None
        self.fobj       = None
        self.outfile    = open(outfile_path, 'w')
        self.alive      = True
        self.last_event = ''
        self.last_time  = time.monotonic()
        self.timed_out  = False

    def open_fifo(self):
        """Open FIFO read-side non-blocking (succeeds immediately even with no
        writer yet), then switch back to blocking for readline() compatibility."""
        import fcntl
        fd = os.open(self.fifo_path, os.O_RDONLY | os.O_NONBLOCK)
        flags = fcntl.fcntl(fd, fcntl.F_GETFL)
        fcntl.fcntl(fd, fcntl.F_SETFL, flags & ~os.O_NONBLOCK)
        self.fd   = fd
        self.fobj = os.fdopen(fd, 'r', buffering=1)

    def kill_proc(self):
        if self.pid is not None:
            try:
                os.kill(self.pid, signal.SIGTERM)
            except ProcessLookupError:
                pass

    def close(self):
        self.alive = False
        if self.fobj:
            try:
                self.fobj.close()
            except Exception:
                pass
            self.fobj = None
            self.fd   = None
        self.outfile.close()


def run(timeout, pid_map, ready_fd, fifos, outfiles):  # ready_fd = path or None
    names = [os.path.basename(f).replace('.fifo', '') for f in fifos]

    participants = []
    for name, fifo, out in zip(names, fifos, outfiles):
        p = Participant(name, fifo, out, pid=pid_map.get(name))
        participants.append(p)

    # Open ALL FIFOs before signalling ready.
    # O_NONBLOCK on read-side succeeds even with no writer present.
    for p in participants:
        try:
            p.open_fifo()
        except Exception as e:
            print(f'WARN  [{p.name}] cannot open FIFO {p.fifo_path}: {e}',
                  flush=True)
            p.alive = False

    # Signal readiness: all FIFOs now have a reader.
    # Participants may now safely open their write-ends.
    if ready_fd is not None:
        try:
            # Open named FIFO for writing (blocks until shell reads)
            rfd = os.open(ready_fd, os.O_WRONLY)
            os.write(rfd, b'R')
            os.close(rfd)
        except OSError:
            pass

    any_timeout = False

    while True:
        alive = [p for p in participants if p.alive and p.fobj is not None]
        if not alive:
            break

        fds = [p.fobj for p in alive]

        try:
            readable, _, _ = select.select(fds, [], [], timeout)
        except ValueError:
            break

        now = time.monotonic()

        if not readable:
            # Global silence across all remaining FIFOs
            for p in alive:
                print(f'TIMEOUT [{p.name}] — last event: {p.last_event!r}',
                      flush=True)
                print(f'  → infinite loop or deadlock at this trace point',
                      flush=True)
                p.timed_out = True
                p.kill_proc()
                p.close()
                any_timeout = True
            break

        # Per-participant watchdog: readable returned but some fds were not in it
        for p in alive:
            if p.fobj not in readable:
                if (now - p.last_time) > timeout:
                    print(f'TIMEOUT [{p.name}] — last event: {p.last_event!r}',
                          flush=True)
                    print(f'  → infinite loop or deadlock at this trace point',
                          flush=True)
                    p.timed_out = True
                    p.kill_proc()
                    p.close()
                    any_timeout = True

        for fobj in readable:
            p = next(x for x in alive if x.fobj is fobj)
            try:
                line = fobj.readline()
            except Exception:
                line = ''
            if line == '':
                p.close()
            else:
                p.outfile.write(line)
                p.outfile.flush()
                p.last_event = line.rstrip('\n')
                p.last_time  = now

    for p in participants:
        if p.alive:
            p.close()

    return 1 if any_timeout else 0


def main():
    timeout, pid_map, ready_fd, fifos, outfiles = parse_args()  # ready_fd = path
    rc = run(timeout, pid_map, ready_fd, fifos, outfiles)
    sys.exit(rc)


if __name__ == '__main__':
    main()
