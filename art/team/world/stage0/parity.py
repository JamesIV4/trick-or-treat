"""Stage 4c: are the settlements of one kind unchanged? town_parity.py generalised by kind (towns are no longer
meant to match the pre-4c layouts; this now checks the neighbourhoods). Compares every settlement of that kind
category by category between two layout dumps (several dump files per side may be joined with '+').

  python parity.py 4c_before_dump 4c_a_dump+4c_b_dump neighborhood
"""
import collections
import json
import os
import sys

here = os.path.dirname(os.path.abspath(__file__))
kind = sys.argv[3] if len(sys.argv) > 3 else "neighborhood"


def load(names):
    out = {}
    for name in names.split("+"):
        with open(os.path.join(here, name + ".jsonl"), encoding="utf-8-sig") as f:
            for line in f:
                if not line.startswith("{"):
                    continue
                r = json.loads(line)
                if r.get("kind") == kind:
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
print("%s settlements compared: %d (only in one: %d)" % (kind, len(set(a) & set(b)), len(set(a) ^ set(b))))
for cat in sorted(set(same) | set(diff)):
    print("  %-14s same %3d  differ %3d  %s" % (cat, same[cat], diff[cat], examples.get(cat, "")))
