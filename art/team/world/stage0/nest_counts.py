"""Stage 4c: monster nests and thorn nests per settlement from sweep logs: python nest_counts.py <name> [<name2> ...]

Per line kind (SWEEP standalone hood, SWEEP_EMB embedded hood, towns from SWEEP / SWEEP_TOWN by size class):
nests and thorn nests placed / wanted, the spacing achieved (minimum gaps, worst pair against its rule, relax
steps) and how evenly the nests cover the streets (distance from every 5 m of street to the nearest nest)."""
import collections
import json
import os
import statistics
import sys

here = os.path.dirname(os.path.abspath(__file__))
groups = collections.defaultdict(list)
for name in sys.argv[1:]:
    with open(os.path.join(here, name + ".log"), encoding="utf-8-sig", errors="replace") as f:
        for line in f:
            tag = line.split(" ", 1)[0]
            if tag not in ("SWEEP", "SWEEP_EMB", "SWEEP_TOWN") or not line[len(tag) + 1:].startswith("{"):
                continue
            r = json.loads(line[len(tag) + 1:])
            if r.get("kind") == "town":
                groups["town " + r.get("town_size", "?")].append(r)
                groups["town (all)"].append(r)
            elif tag == "SWEEP_EMB":
                groups["hood embedded"].append(r)
            else:
                groups["hood standalone"].append(r)


def m(rs, k, f=statistics.mean):
    v = [r[k] for r in rs if isinstance(r.get(k), (int, float))]
    return f(v) if v else float("nan")


for g in sorted(groups):
    rs = groups[g]
    print("== %s (%d)" % (g, len(rs)))
    print("  nests  %.1f / wanted %.1f (min %d, max %d; short %d areas)" % (m(rs, "nests"), m(rs, "nests_wanted"), m(rs, "nests", min), m(rs, "nests", max),
                                                                      sum(1 for r in rs if r["nests"] < r["nests_wanted"])))
    print("  hearts %.1f / wanted %.1f (min %d, max %d)" % (m(rs, "hearts"), m(rs, "hearts_wanted"), m(rs, "hearts", min), m(rs, "hearts", max)))
    print("  min gaps: nest-nest %.1f, heart-heart %.1f, heart-nest %.1f (rules 14 / 18 / 12); gap ratio min %.2f; relax used in %d areas, retries %d" % (
        m(rs, "nest_gap_min", min), m(rs, "heart_gap_min", min), m(rs, "heart_nest_gap_min", min), m(rs, "gap_ratio_min", min),
        sum(1 for r in rs if r.get("relax", 0) or r.get("relax_hearts", 0)), sum(1 for r in rs if r.get("nest_tries", 1) > 1)))
    print("  street to nearest nest: mean %.1f m, worst %.1f m; share of street > 30 m from a nest %.3f" % (m(rs, "nest_street_gap_mean"), m(rs, "nest_street_gap_max", max),
                                                                                                      m(rs, "nest_street_far_share")))
    print("  nests on the road %.1f, candidates %.0f; built street %.0f m" % (m(rs, "nests_on_road"), m(rs, "nest_candidates"), m(rs, "town_built_len") if "town" in g else m(rs, "road_length")))
