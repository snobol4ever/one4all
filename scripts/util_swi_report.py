#!/usr/bin/env python3
"""util_swi_report.py — print MISS/HIT report for one SWI test file.
Usage: util_swi_report.py <ref_file> <actual_text_file>
"""
import sys

ref_path = sys.argv[1]
actual_path = sys.argv[2]

expected = open(ref_path).read().strip().splitlines()
actual_raw = open(actual_path).read().strip().splitlines()

seen = set()
actual_set = set()
for line in actual_raw:
    line = line.strip()
    if line.startswith(('PASS ', 'FAIL ')) and line not in seen:
        seen.add(line)
        actual_set.add(line)

for e in expected:
    marker = "    " if e in actual_set else "MISS"
    print(f"  {marker}  {e}")
