"""Stage 4c summary of a sweep log: python town_stats.py <name> [<name2> ...] (reads <name>.log next to this file).

Replaces town_parity.py (towns are no longer meant to match the pre-4c layouts). Towns: the SWEEP lines of the
odd seeds and the SWEEP_TOWN lines of the even ones (both embedded towns, the open world's), so every seed
has one. Template mix, size classes, the square, lot / corridor tests, grade, plinths and door treads, candy
and props against the ground, nests, nav, plan / build time; SWEEP_TPARITY (WorldPlan's ground vs the
generator's) and the world plan's ports (SWEEP_WORLD)."""
import collections
import json
import os
import statistics
import sys

here = os.path.dirname(os.path.abspath(__file__))
rows = {"SWEEP": [], "SWEEP_TOWN": [], "SWEEP_WORLD": [], "SWEEP_TPARITY": []}
for name in sys.argv[1:]:
    with open(os.path.join(here, name + ".log"), encoding="utf-8-sig", errors="replace") as f:
        for line in f:
            for tag in rows:
                if line.startswith(tag + " {"):
                    rows[tag].append(json.loads(line[len(tag) + 1:]))
towns = [r for r in rows["SWEEP"] if r.get("kind") == "town"] + rows["SWEEP_TOWN"]
towns.sort(key=lambda r: r["seed"])


def summ(vals):
    return "min %.2f mean %.2f max %.2f" % (min(vals), statistics.mean(vals), max(vals)) if vals else "-"


print("== towns (%d, seeds %s..%s)" % (len(towns), towns[0]["seed"] if towns else "-", towns[-1]["seed"] if towns else "-"))
for k in ["town_template", "town_size", "square_place", "town_ports", "town_port_roads", "town_dead_ends", "square_valid", "cine_straight"]:
    c = collections.Counter(str(r.get(k)) for r in towns)
    print("  %-22s %s" % (k, dict(sorted(c.items()))))
by_t = collections.defaultdict(list)
for r in towns:
    by_t[(r["town_template"], r["town_size"])].append(r)
print("  template x size: lots / built street m / footprint w x d")
for key in sorted(by_t):
    rs = by_t[key]
    print("    %-12s %-7s n=%-2d lots %s | street %s | fp %.0f x %.0f" % (key[0], key[1], len(rs), summ([r["lots"] for r in rs]), summ([r["town_built_len"] for r in rs]),
                                                                       statistics.mean(r["footprint"][0] for r in rs), statistics.mean(r["footprint"][1] for r in rs)))
keys = ["lots", "roads", "town_built_len", "road_attempts", "lot_overlaps", "lots_in_road", "lots_in_corridor", "lots_in_zone", "corridor_reach_max", "lot_candy_on_road",
        "square_lot_overlaps", "square_corridor_reach", "square_pad_minus_road", "plazas", "road_grade_max", "road_grade4_max", "terrain_relief", "cut_max", "fill_max",
        "foundations_above_ground", "foundation_gap_max", "foundation_depth_max", "shop_plinth_max", "shop_door_treads", "facade_step_max", "facades_stepped",
        "walk_pokes", "walk_poke_max", "walk_gap_max", "ground_slope_p99", "ground_slope_max", "candy", "candy_lot_mean", "candy_street", "candy_dropped", "candy_low",
        "candy_over_ground_min", "props_floating", "props_floating_max", "props_buried", "nests", "nests_wanted", "hearts", "hearts_wanted", "gap_ratio_min",
        "nav_spawn_walkable", "nav_nests_cut_off", "openness_failures", "plan_ms", "build_ms", "boxes"]
for k in keys:
    vals = [r[k] for r in towns if isinstance(r.get(k), (int, float))]
    if vals:
        print("  %-26s %s  (sum %.1f)" % (k, summ(vals), sum(vals)))
for k in ["square_size", "square_pad_ground", "footprint"]:
    a = [r[k][0] for r in towns if k in r]
    b = [r[k][1] for r in towns if k in r]
    if a:
        print("  %-26s [0] %s | [1] %s" % (k, summ(a), summ(b)))
ids = [i for r in towns for i in r.get("props_floating_ids", [])]
if ids:
    print("  off-ground props:", ids[:16])
fb = [r["seed"] for r in towns if r.get("road_attempts", 0) < 0]
print("  fallbacks:", fb)
notes = collections.Counter(n for r in towns for n in r.get("notes", []) if "landmarks" not in n)
if notes:
    print("  notes:", dict(notes))
bad = [(r["seed"], r.get("lot_overlaps"), r.get("lots_in_corridor"), r.get("lots_in_zone")) for r in towns if r.get("lot_overlaps") or r.get("lots_in_corridor") or r.get("lots_in_zone")]
if bad:
    print("  seeds with overlaps / corridor / zone:", bad)
tp = rows["SWEEP_TPARITY"]
if tp:
    print("== WorldPlan ground vs generator ground (one town per seed): %d towns, same grid %d, max |dh| %.4f" % (len(tp), sum(r["same_grid"] for r in tp), max(r["terrain_diff_max"] for r in tp)))
w = rows["SWEEP_WORLD"]
if w:
    print("== world (%d seeds x 25 regions)" % len(w))
    for k in ["world_towns", "world_ports", "world_ports_matched", "world_ports_roaded", "world_town_lanes", "world_town_lanes_open", "world_hood_ns_ports", "world_hood_ns_open",
              "world_rule_violations", "world_footprint_clashes", "world_roads_checked", "world_road_lot_hits", "world_road_lot_roads", "world_road_corridor_hits", "world_road_corridor_roads"]:
        print("  %-26s %d" % (k, sum(r[k] for r in w)))
    print("  world_reach_max %.1f (limit %.1f)" % (max(r["world_reach_max"] for r in w), w[0]["world_reach_limit"]))
    spots = [(r["seed"], s) for r in w for s in r.get("world_town_spots", [])]
    print("  town templates in the world plans:", dict(collections.Counter(s["template"] for _, s in spots)))
    if "-spots" in os.environ.get("TOWN_STATS", ""):
        for seed, s in spots[:40]:
            print("   ", seed, s)
