"""Stage 2a: are towns unchanged? Compares every town settlement (the sweep's town seeds and the
town_open / town_closed variants of every seed) category by category between two layout dumps.

  python town_parity.py 1e_after2_dump 2a_v5_dump
"""
import collections
import json
import os
import sys

here = os.path.dirname(os.path.abspath(__file__))


def load(name):
    out = {}
    with open(os.path.join(here, name + ".jsonl"), encoding="utf-8-sig") as f:
        for line in f:
            if not line.startswith("{"):
                continue
            r = json.loads(line)
            if r.get("kind") == "town":
                out[(r["label"], r["seed"])] = r["cats"]
    return out


a, b = load(sys.argv[1]), load(sys.argv[2])
same = collections.Counter()
diff = collections.Counter()
examples = {}
for key in sorted(set(a) & set(b)):
    for cat, v in a[key].items():
        if b[key].get(cat) == v:
            same[cat] += 1
        else:
            diff[cat] += 1
            examples.setdefault(cat, key)
print("towns compared:", len(set(a) & set(b)), "(only in one:", len(set(a) ^ set(b)), ")")
for cat in sorted(set(same) | set(diff)):
    print("  %-14s same %3d  differ %3d  %s" % (cat, same[cat], diff[cat], examples.get(cat, "")))
