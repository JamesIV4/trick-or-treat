"""Area preloading parity: a --no-preload walk against a preloaded one (same binary, same seed).

  python art/team/world/preload/verify_compare.py verify_sync verify_pre

Reads <name>.log (VERIFY lines: node-tree fingerprints per settlement) and <name>_dump.jsonl (SweepDump
per-category hashes of each settlement's plan) from this folder; settlements are matched by region.
"""
import json
import os
import re
import sys

HERE = os.path.dirname(os.path.abspath(__file__))


def verify_lines(name):
    out = {}
    with open(os.path.join(HERE, name + ".log"), encoding="utf-8", errors="replace") as f:
        for line in f:
            m = re.match(r"VERIFY (.+) (-?\d+,-?\d+) nodes=(\d+) tree=(\w+)", line.strip())
            if m:
                out.setdefault(m.group(2), []).append((m.group(1), int(m.group(3)), m.group(4)))
    return out


def dumps(name):
    out = {}
    with open(os.path.join(HERE, name + "_dump.jsonl"), encoding="utf-8-sig") as f:
        for line in f:
            line = line.strip()
            if not line or line.startswith("D "):
                continue
            d = json.loads(line)
            out.setdefault(d["label"], []).append(d["cats"])
    return out


def main():
    a, b = sys.argv[1], sys.argv[2]
    bad = 0
    va, vb = verify_lines(a), verify_lines(b)
    common = sorted(set(va) & set(vb))
    for k in common:
        if va[k][0] != vb[k][0]:
            print(f"tree differs at {k}: {va[k][0]} vs {vb[k][0]}")
            bad += 1
    print(f"trees: {len(common)} settlements compared ({len(va)} / {len(vb)} attached), {bad} differ")
    da, db = dumps(a), dumps(b)
    cbad = 0
    cats = 0
    for k in sorted(set(da) & set(db)):
        x, y = da[k][0], db[k][0]
        for c, v in x.items():
            cats += 1
            if v != y.get(c):
                print(f"plan differs at {k} {c}: {v} vs {y.get(c)}")
                cbad += 1
    print(f"plans: {len(set(da) & set(db))} settlements, {cats} category hashes compared, {cbad} differ")
    sys.exit(1 if bad or cbad else 0)


if __name__ == "__main__":
    main()
