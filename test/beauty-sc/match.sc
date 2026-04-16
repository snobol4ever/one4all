// match.sc — Snocone port of match.sno

procedure match(subject, pattern) {
    match = .dummy;
    if (subject ? pattern) { nreturn; } else { freturn; }
}

procedure notmatch(subject, pattern) {
    notmatch = .dummy;
    if (subject ? pattern) { freturn; } else { nreturn; }
}
