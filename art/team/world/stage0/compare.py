"""Stage 0 parity check.

  python art/team/world/stage0/compare.py baseline_pristine after          # SWEEP lines (original keys)
  python art/team/world/stage0/compare.py baseline_dump after --dump       # plus the layout dump hashes

SWEEP lines: every key the pristine baseline has, except the timings (plan_ms, build_ms), must match.
Dump: every category's entry count and hash must match, per settlement / variant and per world plan.
"""
import json
import os
import sys

HERE = os.path.dirname(os.path.abspath(__file__))
TIMINGS = {"plan_ms", "build_ms", "stage_ms", "nav_build_ms"} | {"nav%dk_%s" % (c, k) for c in (9, 20, 40, 100) for k in ("route_ms", "worst_route_ms")}


def load_jsonl(path, prefix=None):
    out = []
    with open(path, encoding="utf-8-sig") as f:
        for line in f:
            line = line.strip()
            if not line or line.startswith("D "):
                continue
            out.append(json.loads(line))
    return out


def main():
    a, b = sys.argv[1], sys.argv[2]
    dump = "--dump" in sys.argv
    bad = 0
    sa = load_jsonl(os.path.join(HERE, a + ".jsonl"))
    sb = load_jsonl(os.path.join(HERE, b + ".jsonl"))
    if len(sa) != len(sb):
        print(f"SWEEP line count differs: {len(sa)} vs {len(sb)}")
        bad += 1
    for x, y in zip(sa, sb):
        for k in x:
            if k in TIMINGS:
                continue
            if x[k] != y.get(k):
                print(f"SWEEP seed {x.get('seed')} {k}: {x[k]} -> {y.get(k)}")
                bad += 1
    print(f"SWEEP: {len(sa)} lines compared on {len([k for k in sa[0] if k not in TIMINGS])} keys, {bad} differences")
    if dump:
        da = load_jsonl(os.path.join(HERE, a + "_dump.jsonl"))
        db = load_jsonl(os.path.join(HERE, b + "_dump.jsonl"))
        dbad = 0
        if len(da) != len(db):
            print(f"dump record count differs: {len(da)} vs {len(db)}")
            dbad += 1
        cats = 0
        for x, y in zip(da, db):
            tag = f"{x['label']} seed {x['seed']}"
            if (x["label"], x["seed"]) != (y["label"], y["seed"]):
                print(f"dump order differs at {tag}")
                dbad += 1
                continue
            for k, v in x["cats"].items():
                cats += 1
                if y["cats"].get(k) != v:
                    print(f"dump {tag} {k}: {v} -> {y['cats'].get(k)}")
                    dbad += 1
        print(f"dump: {len(da)} records, {cats} category hashes compared, {dbad} differences")
        bad += dbad
    sys.exit(1 if bad else 0)


if __name__ == "__main__":
    main()
