"""Stage 1e candy summary: python candy_stats.py <name> [<before-name>]  (names in art/team/world/stage0)."""
import json
import os
import sys

HERE = r"S:\Repos\trick-or-treat\art\team\world\stage0"


def load(name):
    with open(os.path.join(HERE, name + ".jsonl"), encoding="utf-8-sig") as f:
        return [json.loads(l) for l in f if l.strip()]


def summ(rows, kind):
    rows = [r for r in rows if r["kind"] == kind]
    per = [n for r in rows for n in r["candy_per_lot"]]
    val = [v for r in rows for v in r.get("candy_value_per_lot", [])]
    out = {
        "seeds": len(rows),
        "lots": len(per),
        "candy_mean": round(sum(per) / len(per), 1),
        "candy_min": min(per),
        "candy_max": max(per),
        "in_22_30": round(sum(1 for n in per if 22 <= n <= 30) / len(per), 2),
        "under_22": sum(1 for n in per if n < 22),
        "over_30": sum(1 for n in per if n > 30),
        "dropped": sum(r["candy_dropped"] for r in rows),
        "raised": sum(r["candy_raised"] for r in rows),
        "street": round(sum(r["candy_street"] for r in rows) / len(rows), 1),
    }
    if val:
        out.update({"value_mean": round(sum(val) / len(val), 1), "value_min": min(val), "value_max": max(val)})
        tiers = [0, 0, 0, 0]
        for r in rows:
            for i, t in enumerate(r["candy_lot_tiers"]):
                tiers[i] += t
        out["tiers_per_lot"] = [round(t / len(per), 2) for t in tiers]
        pats = {}
        drop = {}
        raise_ = {}
        for r in rows:
            for k, v in r["candy_lot_patterns"].items():
                pats[k] = pats.get(k, 0) + v
            for k, v in r["candy_pattern_dropped"].items():
                drop[k] = drop.get(k, 0) + v
            for k, v in r["candy_pattern_raised"].items():
                raise_[k] = raise_.get(k, 0) + v
        out["pattern_per_lot"] = {k: round(v / len(per), 2) for k, v in pats.items() if v}
        out["pattern_dropped"] = {k: v for k, v in drop.items() if v}
        out["pattern_raised"] = {k: v for k, v in raise_.items() if v}
    return out, rows


def main():
    a = load(sys.argv[1])
    b = load(sys.argv[2]) if len(sys.argv) > 2 else None
    for kind in ("neighborhood", "town"):
        s, ra = summ(a, kind)
        print(kind, json.dumps(s))
        if b:
            sb, rb = summ(b, kind)
            # per-lot value delta (same seeds, same lots)
            d = []
            for x, y in zip(ra, rb):
                for va, vb in zip(x["candy_value_per_lot"], y["candy_value_per_lot"]):
                    d.append(va - vb)
            if d:
                d.sort()
                print(f"  value delta vs {sys.argv[2]}: mean {sum(d)/len(d):+.1f}, median {d[len(d)//2]:+d}, p10 {d[len(d)//10]:+d}, p90 {d[len(d)*9//10]:+d}")


main()
