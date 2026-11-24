#!/usr/bin/env python3
"""
parse_captures.py - Parse the Radica Cube World protocol-capture annotations
(*_Data.txt, produced by the sigrok-decoder in npjg/radica-cube-world-hack)
into captures.json plus a printed summary.

Usage:
  python3 tools/parse_captures.py <dir-with-3-subdirs> [out.json]

The *_Data.txt lines look like:
  7946829-7960841 Cube World: Transmissions: 0x1 0x7 0x1 0x5 0x1 0x1 0x9
"""
import json
import os
import re
import statistics
import sys

LINE = re.compile(r"(\d+)-(\d+) Cube World: Transmissions: (.+)")


def parse_file(path):
    rows = []
    with open(path) as f:
        for line in f:
            m = LINE.match(line.strip())
            if not m:
                continue
            t0, t1 = int(m.group(1)), int(m.group(2))
            nips = [int(x, 16) for x in re.findall(r"0x([0-9a-f])", m.group(3))]
            assert len(nips) == 7, (path, line)
            rows.append({"t0": t0, "t1": t1, "packet": nips})
    return rows


def summarize(name, rows):
    durs = [r["t1"] - r["t0"] for r in rows]
    gaps = [rows[i + 1]["t0"] - rows[i]["t1"] for i in range(len(rows) - 1)]
    kinds = {}
    for r in rows:
        k = tuple(r["packet"])
        kinds[k] = kinds.get(k, 0) + 1
    return {
        "packets": len(rows),
        "duration_ms": (rows[-1]["t1"] - rows[0]["t0"]) / 1000.0,
        "packet_dur_us": {"min": min(durs), "median": statistics.median(durs), "max": max(durs)},
        "inter_packet_ms": {"min": min(gaps) / 1000.0, "median": statistics.median(gaps) / 1000.0, "max": max(gaps) / 1000.0},
        "unique_packets": {"%x %x %x %x %x %x %x" % k: c for k, c in sorted(kinds.items())},
    }


def main():
    if len(sys.argv) < 2:
        print(__doc__)
        sys.exit(1)
    base = sys.argv[1]
    out = sys.argv[2] if len(sys.argv) > 2 else os.path.join(os.path.dirname(os.path.abspath(__file__)), "captures.json")
    data = {}
    summary = {}
    for name in sorted(os.listdir(base)):
        d = os.path.join(base, name)
        txt = os.path.join(d, name + "_Data.txt")
        if not os.path.isfile(txt):
            continue
        rows = parse_file(txt)
        data[name] = rows
        summary[name] = summarize(name, rows)
    with open(out, "w") as f:
        json.dump({"captures": data, "summary": summary}, f, indent=1)
    print("wrote", out)
    for name, s in summary.items():
        print("\n== %s ==" % name)
        print("  packets=%d  duration=%.0fms  median_gap=%.0fms" % (s["packets"], s["duration_ms"], s["inter_packet_ms"]["median"]))
        for p, c in s["unique_packets"].items():
            print("  %4dx  %s" % (c, p))


if __name__ == "__main__":
    main()
