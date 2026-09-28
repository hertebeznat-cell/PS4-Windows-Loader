#!/usr/bin/env python3
"""Summarize CPU flags from a Stage 4.8 preflight log; never assert OS bootability."""

import argparse
import re
from pathlib import Path

ENTRY = re.compile(r"^CPU48: (leaf|subleaf|eax|ebx|ecx|edx)=0x([0-9a-fA-F]{16})$")
FIELDS = ("leaf", "subleaf", "eax", "ebx", "ecx", "edx")
FLAGS = (
    ("NX/DEP", 0x80000001, "edx", 20),
    ("CMPXCHG16B", 1, "ecx", 13),
    ("LAHF/SAHF", 0x80000001, "ecx", 0),
    ("PREFETCHW", 0x80000001, "ecx", 8),
    ("NPT (SLAT)", 0x8000000A, "edx", 0),
    ("SSE4.2", 1, "ecx", 20),
    ("POPCNT", 1, "ecx", 23),
)


def parse(text):
    if "MODE: CPU_ONLY; no EFI or Boot Manager entry" in text and (
        text.splitlines().count("CPU probe finished") != 1
    ):
        raise ValueError("standalone CPU probe did not finish")
    if text.count("CPU48: CPUID raw registers begin") != 1 or text.count(
        "CPU48: CPUID raw registers end"
    ) != 1:
        raise ValueError("expected exactly one complete CPU48 capture")
    lines = text.splitlines()
    start = lines.index("CPU48: CPUID raw registers begin")
    end = lines.index("CPU48: CPUID raw registers end")
    if end <= start:
        raise ValueError("CPU48 markers are out of order")
    values = []
    for line in lines[start + 1 : end]:
        match = ENTRY.fullmatch(line)
        if not match:
            raise ValueError(f"unexpected CPU48 line: {line!r}")
        values.append((match.group(1), int(match.group(2), 16)))
    if len(values) % len(FIELDS):
        raise ValueError("incomplete CPUID entry")
    leaves = {}
    for index in range(0, len(values), len(FIELDS)):
        group = values[index : index + len(FIELDS)]
        if tuple(field for field, _ in group) != FIELDS:
            raise ValueError("CPUID fields out of order")
        item = dict(group)
        key = item["leaf"], item["subleaf"]
        if key in leaves:
            raise ValueError("duplicate CPUID leaf")
        leaves[key] = item
    if (0, 0) not in leaves or (0x80000000, 0) not in leaves:
        raise ValueError("CPUID maximum-leaf records missing")
    basic_max = leaves[0, 0]["eax"]
    extended_max = leaves[0x80000000, 0]["eax"]
    for leaf in (1, 0x80000001, 0x8000000A):
        supported = leaf <= (basic_max if leaf < 0x80000000 else extended_max)
        if supported != ((leaf, 0) in leaves):
            raise ValueError(f"CPUID leaf {leaf:#x} inconsistent with maximum")
    return leaves


def summarize(leaves):
    result = []
    for name, leaf, register, bit in FLAGS:
        record = leaves.get((leaf, 0))
        state = "UNKNOWN" if record is None else (
            "YES" if record[register] & (1 << bit) else "NO"
        )
        result.append((name, state))
    return result


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("log", type=Path)
    args = parser.parse_args()
    try:
        leaves = parse(args.log.read_text(encoding="utf-8", errors="replace"))
    except ValueError as exc:
        parser.error(str(exc))
    for name, state in summarize(leaves):
        print(f"{name}: {state}")
    print("Clock, firmware, RAM map, drivers and OS compatibility: NOT VERIFIED")


if __name__ == "__main__":
    main()
