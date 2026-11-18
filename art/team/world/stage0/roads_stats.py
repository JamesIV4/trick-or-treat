"""Stage 2a summary of a sweep log: python roads_stats.py <name> (reads <name>.log next to this file).

SWEEP lines (standalone hoods, embedded towns), SWEEP_EMB (embedded hoods, what the open world plays) and
SWEEP_WORLD (the world plan round each seed)."""
import json
import os
import statistics
import sys

here = os.path.dirname(os.path.abspath(__file__))
name = sys.argv[1]
rows = {"SWEEP": [], "SWEEP_EMB": [], "SWEEP_WORLD": []}
with open(os.path.join(here, name + ".log"), encoding="utf-8", errors="replace") as f:
    for line in f:
        for tag in rows:
            if line.startswith(tag + " {"):
                rows[tag].append(json.loads(line[len(tag) + 1:]))


def summ(vals):
    if not vals:
        return "-"
    return "min %.1f mean %.1f max %.1f" % (min(vals), statistics.mean(vals), max(vals))


def report(label, rs):
    if not rs:
        return
    print("== %s (%d)" % (label, len(rs)))
    keys = ["lots", "lot_overlaps", "lots_in_road", "lots_in_corridor", "lots_in_zone", "lot_candy_on_road", "corridor_reach_max", "road_intrusion_max",
            "branches", "bulbs", "bulb_lots", "bulb_ring_min", "road_length", "road_attempts", "footprint_reach", "plan_ms", "build_ms",
            "candy", "candy_lot_mean", "candy_street", "nests", "nests_wanted", "hearts", "lot_rejects", "nest_gap_min", "gap_ratio_min",
            "nav_build_ms"] + ["nav%dk_%s" % (c, k) for c in (9, 20, 40, 100) for k in ("chase_capped", "home_capped", "chase_capped_connected", "home_capped_connected", "expanded_max_connected", "route_ms", "worst_route_ms")] + [ "openness_failures"]
    for k in keys:
        vals = [r[k] for r in rs if k in r and isinstance(r[k], (int, float))]
        if vals:
            print("  %-22s %s  (sum %.1f)" % (k, summ(vals), sum(vals)))
    if "footprint" in rs[0]:
        w = [r["footprint"][0] for r in rs]
        d = [r["footprint"][1] for r in rs]
        print("  footprint w %s | d %s" % (summ(w), summ(d)))
    if "road_kinds" in rs[0]:
        lens = {}
        lots = {}
        for r in rs:
            for k, l, n in zip(r["road_kinds"], r["road_lengths"], r["lots_by_road"]):
                lens.setdefault(k, []).append(l)
                lots.setdefault(k, []).append(n)
        for k in lens:
            print("  road %-5s n=%-3d length %s | lots %s" % (k, len(lens[k]), summ(lens[k]), summ(lots[k])))
        ports = [r.get("port_branch", 0) for r in rs]
        print("  port branch: none %d, N %d, S %d" % (ports.count(0), ports.count(1), ports.count(-1)))
    bad = [(r["seed"], r.get("lot_overlaps"), r.get("lots_in_corridor"), r.get("lots_in_zone")) for r in rs
           if r.get("lot_overlaps", 0) or r.get("lots_in_corridor", 0) or r.get("lots_in_zone", 0)]
    if bad:
        print("  seeds with overlaps / corridor / zone:", bad[:12])
    notes = [(r["seed"], r["notes"]) for r in rs if r.get("notes")]
    if notes:
        print("  notes:", notes[:8])


hood = [r for r in rows["SWEEP"] if r.get("kind") == "neighborhood"]
town = [r for r in rows["SWEEP"] if r.get("kind") == "town"]
report("hood standalone", hood)
report("hood embedded", rows["SWEEP_EMB"])
report("town", town)
w = rows["SWEEP_WORLD"]
if w:
    print("== world (%d seeds x 25 regions)" % len(w))
    for k in ["world_hoods", "world_towns", "world_woods", "world_ports", "world_ports_matched", "world_ports_roaded", "world_hood_ns_ports",
              "world_hood_ns_open", "world_town_lanes", "world_town_lanes_open", "world_rule_violations", "world_footprint_clashes",
              "world_roads_checked", "world_road_lot_hits", "world_road_lot_roads", "world_road_corridor_hits", "world_road_corridor_roads"]:
        print("  %-26s %d" % (k, sum(r[k] for r in w)))
    print("  world_reach_max %.1f (limit %.1f)" % (max(r["world_reach_max"] for r in w), w[0]["world_reach_limit"]))
    print("  hood size max", max(r["world_hood_size_max"][0] for r in w), "x", max(r["world_hood_size_max"][1] for r in w))
