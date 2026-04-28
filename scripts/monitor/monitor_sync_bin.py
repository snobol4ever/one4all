#!/usr/bin/env python3
"""
monitor_sync_bin.py — binary-protocol sync-step monitor controller.

Reads fixed-size binary records from each participant's ready FIFO,
compares them as (kind, name_string, type, value_bytes) tuples, and
writes 'G' (go) or 'S' (stop) to each participant's go FIFO.

Wire format (matches monitor_wire.h):
    13-byte header LE: u32 kind | u32 name_id | u8 type | u32 value_len
    value_len bytes of value (varies by type)

SN-26-bridge-coverage-e — streaming intern on the wire.

Names are NOT loaded from a sidecar file.  Participants emit MWK_NAME_DEF
records inline before any record using a fresh name_id.  The controller
maintains a per-participant intern table populated from those NAME_DEF
records.  NAME_DEFs are acked with 'G' like any other record but are
not surfaced as semantic events for divergence comparison — different
participants may assign the same name different ids without diverging,
since comparison is on the resolved name string, not the id.

CLI shape (single, simple):

    monitor_sync_bin.py NAME:READY_FIFO:GO_FIFO ...

The first PARTICIPANT is the consensus oracle.  Divergences are reported
relative to it.

Exit codes:
    0   all participants reached END agreeing on every event
    1   divergence — first disagreement reported, all participants stopped
    2   timeout / bad CLI
    3   protocol error (bad header, short read, etc.)
"""

import os
import struct
import sys
import time
from collections import namedtuple, deque

# How many last-agreed records to keep in the circular buffer and print on
# DIVERGE — gives the "last-agree + first-disagree" context RULES.md
# "Sync-step monitor — read the divergence point, not the trace" calls for.
# Overridable via MONITOR_LAST_AGREE_TRAIL env var (integer >= 1).
# The circular buffer is a collections.deque(maxlen=N) — O(1) append/evict.
_default_history = 5
try:
    DIVERGE_HISTORY = max(1, int(os.environ.get('MONITOR_LAST_AGREE_TRAIL', '') or _default_history))
except ValueError:
    DIVERGE_HISTORY = _default_history

HDR_FMT  = '<IIBI'   # u32 kind, u32 name_id, u8 type, u32 value_len
HDR_SIZE = struct.calcsize(HDR_FMT)
assert HDR_SIZE == 13, "header must be 13 bytes"

# Event kinds — keep aligned with monitor_wire.h MWK_*
MWK_VALUE     = 1
MWK_CALL      = 2
MWK_RETURN    = 3
MWK_END       = 4
MWK_LABEL     = 5
MWK_NAME_DEF  = 6

KIND_NAMES = {
    1: 'VALUE', 2: 'CALL', 3: 'RETURN', 4: 'END',
    5: 'LABEL', 6: 'NAME_DEF',
}

# Type tags (must match monitor_wire.h MWT_*)
TYPE_NAMES = {
    0: 'NULL',  1: 'STRING', 2: 'INTEGER', 3: 'REAL',  4: 'NAME',
    5: 'PATTERN', 6: 'EXPRESSION', 7: 'ARRAY', 8: 'TABLE',
    9: 'CODE', 10: 'DATA', 11: 'FILE', 255: 'UNKNOWN',
}

NAME_ID_NONE = 0xffffffff

EVENT_TIMEOUT_S = 60.0    # generous; beauty self-host is slow

# Raw record off the wire — name_id is dialect-local until resolved.
Event = namedtuple('Event', 'kind name_id type value')


def name_for_id(names_table, name_id):
    """Resolve a name_id against a participant's intern table.

    names_table is a dict {id -> bytes}. NAME_ID_NONE -> '' (used for END/LABEL).
    Unknown ids surface as '(id=N)' so downstream comparison still has a stable
    string — should not happen with well-formed wire output.
    """
    if name_id == NAME_ID_NONE:
        return ''
    nm = names_table.get(name_id)
    if nm is None:
        return f'(id={name_id})'
    try:
        return nm.decode('utf-8', errors='backslashreplace')
    except Exception:
        return repr(nm)


# ---------------------------------------------------------------------------
# read_record — read one full record (header + value bytes) from fd.
# ---------------------------------------------------------------------------

def read_exact(fd, n, timeout_s):
    """Read exactly n bytes from fd or return None on EOF."""
    deadline = time.monotonic() + timeout_s
    buf = b''
    while len(buf) < n:
        remaining = deadline - time.monotonic()
        if remaining <= 0:
            return None
        try:
            chunk = os.read(fd, n - len(buf))
        except BlockingIOError:
            time.sleep(0.001)
            continue
        if not chunk:
            return None  # EOF
        buf += chunk
    return buf


def read_record(fd, timeout_s):
    hdr = read_exact(fd, HDR_SIZE, timeout_s)
    if hdr is None or len(hdr) != HDR_SIZE:
        return None
    kind, name_id, type_tag, value_len = struct.unpack(HDR_FMT, hdr)
    if value_len > 0:
        val = read_exact(fd, value_len, timeout_s)
        if val is None or len(val) != value_len:
            raise ValueError(f'short value read: wanted {value_len} got {len(val) if val else 0}')
    else:
        val = b''
    return Event(kind, name_id, type_tag, val)


def read_semantic_record(f, timeout_s):
    """Read records from participant f until a non-NAME_DEF record is seen.

    ONLY NAME_DEF records are absorbed here — they are wire-protocol
    bookkeeping (binding name_id -> name bytes) and carry no semantics
    of their own.  Every other record kind (VALUE, CALL, RETURN, END,
    LABEL) is returned to the caller for sync-step comparison.

    LABEL records are EXPLICITLY comparison-eligible: a LABEL divergence
    means the runtimes entered different statements (different STNO),
    which is a structural-flow bug that must surface immediately.  Do
    not extend this absorption loop to LABEL or any future "informational"
    kind without an explicit goal-level decision — silently filtering
    LABEL would hide exactly the class of bug the monitor exists to
    catch (control-flow disagreement before any value disagreement).

    NAME_DEF records are acked with 'G' here so the participant can
    continue; the participant still cannot run ahead of the controller
    because each ack is one-record-at-a-time.

    Returns Event or None on EOF.  Raises ValueError on protocol error.
    """
    while True:
        ev = read_record(f['rd'], timeout_s)
        if ev is None:
            return None
        if ev.kind != MWK_NAME_DEF:
            return ev
        # Streaming intern: register binding, ack, loop for next record.
        f['names'][ev.name_id] = ev.value
        try:
            os.write(f['gw'], b'G')
        except OSError:
            return None  # participant closed early


# ---------------------------------------------------------------------------
# Resolve an Event to a comparable tuple using a per-participant names dict.
#
# All non-NAME_DEF kinds participate in this comparison — including LABEL.
# A LABEL divergence (same step, different STNO) means the runtimes are
# executing different statements; that is a real structural-flow bug,
# usually a control-flow disagreement upstream.  Do not be tempted to
# filter LABELs out of the comparison to "reach" a value divergence —
# the LABEL divergence IS the divergence.
# ---------------------------------------------------------------------------

def event_key(ev, names_table):
    """Return (kind, name_string, type, value_bytes) — the comparable tuple."""
    if ev is None:
        return None
    return (ev.kind, name_for_id(names_table, ev.name_id), ev.type, ev.value)


# MWT_UNKNOWN sentinel — wildcard on type field.  See keys_match below.
MWT_UNKNOWN = 255

# <lval> sentinel — wildcard on name field.  See keys_match below.
LVAL_SENTINEL = '<lval>'

# Per-participant blanket-name wildcard — set via env var.
# MONITOR_NAME_WILDCARD="spl"  (comma-separated participant names) treats
# the named participants' name field as a wildcard for ALL events, not
# just <lval>.  Used to advance the wire past known bridge-side bugs
# where one runtime emits stale-memory junk in the name slot for
# aggregate-element stores (SPITBOL fake-vrblk in spl_vrblk_name —
# see GOAL-NET-BEAUTY-SELF S-2-bridge-7 notes).  This lets bug-finding
# proceed on the trustworthy side (e.g. dot) without first having to
# patch the broken side's bridge.  Real value-byte / kind divergences
# are still reported.  Default: empty (no wildcard).
WILDCARD_NAMES_PARTICIPANTS = set(
    p.strip() for p in os.environ.get('MONITOR_NAME_WILDCARD', '').split(',')
    if p.strip()
)

# MONITOR_SKIP_EXTRA_KEYWORD_VALUES=1 — opt-in workaround for the spl bridge's
# missing VALUE emission on keyword assignments (& assignments such as
# &FULLSCAN = 1).  When dot or csn emits a VALUE for a keyword, spl emits
# nothing and advances directly to the next LABEL.  This produces a divergence
# where one participant has VALUE-on-keyword and the other has LABEL/anything-else.
#
# When this flag is set and a divergence matches that pattern, the controller
# acks only the VALUE-emitting side(s) with 'G', reads their next record, and
# retries comparison.  Bounded by SKIP_MAX_PER_STEP to prevent unbounded read-
# ahead.  Off by default — setting it is an explicit acknowledgment that one
# side's bridge is incomplete.
#
# Long-term fix: SN-26-bridge-coverage extends the spl bridge to emit VALUE
# for keyword stores (probably hooks zystt or the same fire-point as &x=v
# scalar stores).  When that lands, this skip becomes a no-op.
SKIP_EXTRA_KEYWORD_VALUES = os.environ.get('MONITOR_SKIP_EXTRA_KEYWORD_VALUES', '').strip() in ('1', 'true', 'yes', 'on')
SKIP_MAX_PER_STEP = 4  # at most this many extra reads per side per comparison step


def keys_match(a, b, a_name_wild=False, b_name_wild=False):
    """Compare two event_key tuples, with three principled wildcards:

      1. MWT_UNKNOWN on the type field — strictly less informative.
         A participant's bridge may not yet have full type-block
         discrimination (e.g. SPITBOL's spl_block_to_wire returns
         MWT_UNKNOWN for nmblk/ptblk/atblk/tbblk/cdblk/efblk because
         the type-word externs are not exported in osint.h).  Until
         SN-26-bridge-coverage extends the coverage, every aggregate
         creation in beauty would trip a spurious DIVERGE on the type
         byte alone while kind, name, and value bytes match.  Treating
         UNKNOWN as a wildcard preserves divergence detection on real
         disagreements; when two real typed tags disagree (STRING vs
         INTEGER), this still flags DIVERGE.

      2. '<lval>' on the name field — strictly less informative.
         The pure-observer protocol contract says aggregate-element
         stores (a<i>=v, d<'k'>=v) are anonymous lvalues with no
         meaningful single name.  csn and the original dot emitted
         '<lval>'.  S-2-bridge-7-lval (snobol4dotnet 2414a26) enriched
         dot to emit the collection name (e.g. 'UTF') as a debugging
         aid — strictly more informative.  Until SN-26-bridge-coverage
         backports the same enrichment to csn (and addresses spl's
         fake-vrblk bug), the wire shows '<lval>' on csn / '<lval>' or
         junk on spl / collection-name on dot.  Treating '<lval>' as
         a wildcard lets the run advance through aggregate stores when
         one side has the enriched name and the other has the sentinel.
         Real name disagreement (e.g. dot's 'S' vs spl's 'T' on a
         scalar store) still flags DIVERGE.

      3. Per-participant blanket name wildcard — opt-in via env var.
         When MONITOR_NAME_WILDCARD lists a participant, that
         participant's name field is wildcarded for ALL events.
         Used to drive the wire forward past known bridge bugs in a
         specific runtime (e.g. SPITBOL's fake-vrblk in
         spl_vrblk_name producing stale-memory names like 'ss' on
         table-element stores).  See SN-26-bridge-coverage in
         GOAL-LANG-SNOBOL4 for the long-term fix.  This wildcard is
         OFF by default; setting it is an explicit acknowledgment
         that one side's name-emission is untrusted.

    All wildcards apply ONLY when kind, value bytes, and the unmasked
    fields all match — they soften the comparison without ever masking
    a value-byte or kind divergence.  Those are the load-bearing fields
    of the protocol; the type and name fields are decorative metadata
    once a real value-byte agreement has been established.
    """
    if a is None or b is None:
        return a is b
    (ak, an, at, av) = a
    (bk, bn, bt, bv) = b
    if ak != bk or av != bv:
        return False
    type_ok = (at == bt) or (at == MWT_UNKNOWN) or (bt == MWT_UNKNOWN)
    if not type_ok:
        return False
    name_ok = (an == bn) or (an == LVAL_SENTINEL) or (bn == LVAL_SENTINEL) \
              or a_name_wild or b_name_wild
    return name_ok


# ---------------------------------------------------------------------------
# Pretty-print one event.
# ---------------------------------------------------------------------------

def fmt_value(type_tag, value):
    name = TYPE_NAMES.get(type_tag, f'T{type_tag}')
    if type_tag == 1 or type_tag == 4:  # STRING or NAME
        try:
            s = value.decode('utf-8', errors='backslashreplace')
        except Exception:
            s = repr(value)
        return f'{name}({len(value)})={s!r}'
    if type_tag == 2:  # INTEGER
        if len(value) == 8:
            iv = struct.unpack('<q', value)[0]
            return f'INT={iv}'
        return f'INT(?{len(value)}b)'
    if type_tag == 3:  # REAL
        if len(value) == 8:
            rv = struct.unpack('<d', value)[0]
            return f'REAL={rv!r}'
        return f'REAL(?{len(value)}b)'
    return f'{name}'


def fmt_event(ev, names_table, stno=None):
    kn = KIND_NAMES.get(ev.kind, f'K{ev.kind}')
    nm = name_for_id(names_table, ev.name_id) or '(none)'
    prefix = f'@{stno} ' if stno is not None else ''
    if ev.kind == MWK_END:
        return f'{prefix}{kn}'
    if ev.kind == MWK_CALL:
        return f'{prefix}{kn} {nm}'
    if ev.kind == MWK_RETURN:
        # Payload is the rtntype string ("RETURN"/"FRETURN"/"NRETURN"), not the
        # function result value.  Show as RETURN fname (KIND) to avoid confusion
        # with value-assignment display.  Result was already on the preceding VALUE.
        try:
            kind_str = ev.value.decode('utf-8', errors='replace') if ev.value else 'RETURN'
        except Exception:
            kind_str = 'RETURN'
        return f'{prefix}RETURN {nm} ({kind_str})'
    if ev.kind == MWK_LABEL:
        return f'{kn} stno={fmt_value(ev.type, ev.value)}'
    return f'{prefix}{kn} {nm} = {fmt_value(ev.type, ev.value)}'


import re as _re

def _scan_sno(path, inc_dirs, out, visited=None):
    if visited is None: visited = set()
    real = os.path.realpath(path)
    if real in visited: return
    visited.add(real)
    src_dir = os.path.dirname(os.path.abspath(path))
    try: lines = open(path, encoding='utf-8', errors='replace').readlines()
    except OSError: return
    fname = os.path.basename(path)
    for lineno, raw in enumerate(lines, 1):
        t = raw.rstrip('\n')
        if t.startswith('*') or t.startswith('+'): continue
        if t.startswith('-'):
            m = _re.search(r"""['"](.*?)['"]""", t)
            if m and 'INCLUDE' in t[:10].upper():
                for d in [src_dir] + list(inc_dirs):
                    cand = os.path.join(d, m.group(1))
                    if os.path.isfile(cand):
                        _scan_sno(cand, inc_dirs, out, visited); break
            continue
        out['_n'] = out.get('_n', 0) + 1
        out[out['_n']] = (fname, lineno, t.strip())

def build_stno_map():
    sno = os.environ.get('MONITOR_SNO_FILE', '').strip()
    if not sno or not os.path.isfile(sno): return {}
    dirs = [d for d in os.environ.get('MONITOR_INC_DIR', '').split(':') if d]
    raw = {}; _scan_sno(sno, dirs, raw); raw.pop('_n', None)
    return raw  # {int stno: (fname, lineno, stripped_text)}


# ---------------------------------------------------------------------------
# Open one FIFO pair (we read ready, write go).
# ---------------------------------------------------------------------------

def open_pair(ready_path, go_path):
    """Open ready FIFO for read, go FIFO for write."""
    rd = os.open(ready_path, os.O_RDONLY)
    gw = os.open(go_path,    os.O_WRONLY)
    return rd, gw


# ---------------------------------------------------------------------------
# Run the controller.
# ---------------------------------------------------------------------------

def run(participants):
    """participants: list of (name, ready_path, go_path)."""
    # Build stno->source map from MONITOR_SNO_FILE + MONITOR_INC_DIR env vars.
    # Pure Python inline read; no sidecar files written; no subprocesses.
    stno_map = build_stno_map()  # {int: (fname, lineno, text)} or {}

    # Optional per-participant wire log — one line per record.  Set
    # MONITOR_TRACE_LOG=/path/prefix and the controller will write
    # /path/prefix.<participant>.log with every record received from each
    # participant in order.  Useful for post-DIVERGE forensic grep without
    # spamming chat.  Empty / unset → no log written.
    trace_prefix = os.environ.get('MONITOR_TRACE_LOG', '').strip()

    fds = []
    for nm, rp, gp in participants:
        rd, gw = open_pair(rp, gp)
        log_fp = None
        if trace_prefix:
            log_fp = open(f'{trace_prefix}.{nm}.log', 'w')
        fds.append({'name': nm, 'rd': rd, 'gw': gw, 'names': {},
                    'log_fp': log_fp})
        print(f'[ctrl] opened {nm}: ready={rp} go={gp}', file=sys.stderr)

    # Interleaved agreed-event trail (circular buffer, always on).
    # Each entry: (step, stno, {pname: event_str}) — one dict per agreed step.
    # On DIVERGE prints as a grid: step | stno | col-per-participant.
    # Buffer is deque(maxlen=N), O(1) append/evict.
    pnames = [nm for nm, rp, gp in participants]
    trail = deque(maxlen=DIVERGE_HISTORY)

    # Track last agreed stno from LABEL records so VALUE/CALL/RETURN rows
    # can show which statement they belong to.  The stno is already on the
    # wire — no source file scanning required.
    last_agreed_stno = None

    diverged = False
    step = 0

    while True:
        step += 1
        # Read one semantic record from each participant.  read_semantic_record
        # absorbs NAME_DEFs internally (acks them, registers bindings).
        events = []
        eof_set = []
        protocol_err = False
        for f in fds:
            try:
                ev = read_semantic_record(f, EVENT_TIMEOUT_S)
            except ValueError as e:
                print(f'[ctrl] PROTOCOL ERR step {step} on {f["name"]}: {e}', file=sys.stderr)
                protocol_err = True
                events.append((f, None))
                eof_set.append(f['name'])
                continue
            if ev is None:
                events.append((f, None))
                eof_set.append(f['name'])
            else:
                events.append((f, ev))
                if f['log_fp']:
                    f['log_fp'].write(f'#{step} {fmt_event(ev, f["names"], stno=last_agreed_stno)}\n')
                    f['log_fp'].flush()

        if protocol_err:
            for ff in fds:
                try: os.write(ff['gw'], b'S')
                except OSError: pass
            return 3

        # All EOF: clean termination (legacy path, when a runtime exits without
        # emitting MWK_END).
        if len(eof_set) == len(fds):
            print(f'[ctrl] all reached EOF at step {step} (clean termination)',
                  file=sys.stderr)
            return 0

        # Mixed EOF: divergence in event count.
        if eof_set:
            print(f'[ctrl] PARTIAL EOF step {step}: {eof_set} done, others still running',
                  file=sys.stderr)
            for f, ev in events:
                if ev is not None:
                    print(f'  {f["name"]}: still emitting {fmt_event(ev, f["names"], stno=last_agreed_stno)}',
                          file=sys.stderr)
                else:
                    print(f'  {f["name"]}: EOF', file=sys.stderr)
            for ff in fds:
                try: os.write(ff['gw'], b'S')
                except OSError: pass
            return 1

        # Compare against oracle (events[0]) using per-participant name resolution.
        # keys_match treats MWT_UNKNOWN as a wildcard on the type field — see
        # the keys_match docstring for the rationale.
        oracle_f, oracle_ev = events[0]
        oracle_key = event_key(oracle_ev, oracle_f['names'])
        oracle_namewild = oracle_f['name'] in WILDCARD_NAMES_PARTICIPANTS
        agree = True
        for f, ev in events[1:]:
            other_namewild = f['name'] in WILDCARD_NAMES_PARTICIPANTS
            if not keys_match(event_key(ev, f['names']), oracle_key,
                              a_name_wild=other_namewild,
                              b_name_wild=oracle_namewild):
                agree = False
                break


        # Opt-in skip — bounded read-ahead on the side(s) that emitted an
        # "extra" VALUE for a keyword (& assignment).  See
        # SKIP_EXTRA_KEYWORD_VALUES comment near the top of this file.
        # Logic: while disagree AND any participant's current event is
        # "VALUE on a name beginning with &", ack that participant only,
        # read its next record, retry comparison.  Bounded to keep us
        # from running ahead unboundedly if the spl gap turns out to be
        # multi-record.
        if not agree and SKIP_EXTRA_KEYWORD_VALUES:
            skip_budget = {f['name']: SKIP_MAX_PER_STEP for f in fds}
            while not agree:
                # Identify participants whose current event is an extra
                # keyword VALUE — i.e., kind == MWK_VALUE and the resolved
                # name starts with '&'.  Only these get advanced.
                advanced_any = False
                for i, (f, ev) in enumerate(events):
                    if ev is None or ev.kind != MWK_VALUE:
                        continue
                    nm = name_for_id(f['names'], ev.name_id)
                    if not nm.startswith('&'):
                        continue
                    if skip_budget[f['name']] <= 0:
                        continue
                    # Ack just this participant, read its next record.
                    try:
                        os.write(f['gw'], b'G')
                    except OSError:
                        return 2
                    try:
                        new_ev = read_semantic_record(f, EVENT_TIMEOUT_S)
                    except ValueError as e:
                        print(f'[ctrl] PROTOCOL ERR while skipping kw-VALUE on {f["name"]}: {e}',
                              file=sys.stderr)
                        return 3
                    if new_ev is None:
                        # EOF on the skipping side mid-skip — let normal
                        # divergence reporting handle it.
                        events[i] = (f, None)
                        break
                    events[i] = (f, new_ev)
                    if f['log_fp']:
                        f['log_fp'].write(f'#{step}+ (kw-VALUE skipped) -> {fmt_event(new_ev, f["names"], stno=last_agreed_stno)}\n')
                        f['log_fp'].flush()
                    skip_budget[f['name']] -= 1
                    advanced_any = True
                if not advanced_any:
                    break  # nothing further to absorb; surface the divergence
                # Recompare with refreshed events.
                oracle_f, oracle_ev = events[0]
                oracle_key = event_key(oracle_ev, oracle_f['names'])
                oracle_namewild = oracle_f['name'] in WILDCARD_NAMES_PARTICIPANTS
                if oracle_ev is None:
                    break
                agree = True
                for f, ev in events[1:]:
                    if ev is None:
                        agree = False
                        break
                    other_namewild = f['name'] in WILDCARD_NAMES_PARTICIPANTS
                    if not keys_match(event_key(ev, f['names']), oracle_key,
                                      a_name_wild=other_namewild,
                                      b_name_wild=oracle_namewild):
                        agree = False
                        break

        if not agree:
            div_cols = {f['name']: fmt_event(ev, f['names'], stno=last_agreed_stno)
                        for f, ev in events}
            pnames = [f['name'] for f in fds]
            all_rows = list(trail) + [(step, last_agreed_stno, div_cols)]
            def src(n):
                if n is None or n not in stno_map: return ''
                fn, ln, txt = stno_map[n]
                return f'{fn}:{ln}  {txt}'
            # Markdown table: | step | stno | p1 | p2 | ... | source |
            def md_row(*cells):
                return '| ' + ' | '.join(str(c) for c in cells) + ' |'
            def md_sep(*widths):
                return '| ' + ' | '.join('-'*max(w,3) for w in widths) + ' |'
            col_w = {p: max(len(p), max((len(r[2].get(p,'')) for r in all_rows), default=0))
                     for p in pnames}
            src_w  = max(6, max((len(src(r[1])) for r in all_rows), default=6))
            step_w = max(4, len(str(step)))
            stno_w = max(4, max((len(str(r[1])) for r in all_rows if r[1] is not None), default=4))
            def row_line(s, n, cols, divrow=False):
                marker = '**>**' if divrow else ''
                cells = [marker + str(s), str(n) if n is not None else '']
                cells += [cols.get(p, '') for p in pnames]
                cells += [src(n)]
                return md_row(*cells)
            hdrs = ['step', 'stno'] + pnames + ['source']
            widths = [step_w, stno_w] + [col_w[p] for p in pnames] + [src_w]
            out = [f'\n[ctrl] DIVERGE step {step} — last {len(trail)} agreed rows + diverge (>):\n']
            out.append(md_row(*hdrs))
            out.append(md_sep(*widths))
            for i, (s, n, cols) in enumerate(all_rows):
                divrow = (i == len(all_rows) - 1)
                out.append(row_line(s, n, cols, divrow))
            print('\n'.join(out), file=sys.stderr)
            for f, ev in events:
                try: os.write(f['gw'], b'S')
                except OSError: pass
            diverged = True
            break
        # Update last-agreed stno from LABEL records (stno is on the wire).
        if oracle_ev.kind == MWK_LABEL:
            if len(oracle_ev.value) == 8:
                last_agreed_stno = int.from_bytes(oracle_ev.value, 'little')

        # Store per-participant event strings in the circular trail.
        trail.append((step, last_agreed_stno,
                      {f['name']: fmt_event(ev, f['names'], stno=last_agreed_stno)
                       for f, ev in events}))

        # If everyone sent END, we're done.
        if oracle_ev.kind == MWK_END:
            for f, ev in events:
                try: os.write(f['gw'], b'G')
                except OSError: pass
            print(f'[ctrl] all reached END after {step} steps', file=sys.stderr)
            break

        # Otherwise GO to all.
        for f, ev in events:
            try:
                os.write(f['gw'], b'G')
            except OSError:
                print(f'[ctrl] write failed to {f["name"]}', file=sys.stderr)
                diverged = True
                break
        if diverged:
            break

    # Close FDs.
    for f in fds:
        try: os.close(f['rd'])
        except OSError: pass
        try: os.close(f['gw'])
        except OSError: pass

    return 1 if diverged else 0


def parse_argv(argv):
    """Return participants or exit(2).

    Spec: NAME:READY:GO  (one per participant).  No sidecar/names paths —
    streaming intern means names live on the wire.
    """
    if len(argv) < 2:
        print('Usage: monitor_sync_bin.py NAME:READY:GO ...', file=sys.stderr)
        sys.exit(2)

    participants = []
    for spec in argv[1:]:
        parts = spec.split(':')
        if len(parts) != 3:
            print(f'bad participant spec (expect NAME:READY:GO): {spec}',
                  file=sys.stderr)
            sys.exit(2)
        participants.append(tuple(parts))
    return participants


def main():
    participants = parse_argv(sys.argv)
    rc = run(participants)
    sys.exit(rc)


if __name__ == '__main__':
    main()
