"""Stage 3b summary of a sweep log: python terrain_stats.py <name> (reads <name>.log next to this file).

Hills under the neighbourhoods (SWEEP standalone hoods and SWEEP_EMB embedded hoods): road grade, terrain
relief, pad - road, yard vs pad, cut / fill against the natural relief, foundations, extra porch treads,
candy over the ground, floating / buried props, nav sanity."""
import json
import os
import statistics
import sys

here = os.path.dirname(os.path.abspath(__file__))
name = sys.argv[1]
rows = {"SWEEP": [], "SWEEP_EMB": []}
with open(os.path.join(here, name + ".log"), encoding="utf-8", errors="replace") as f:
    for line in f:
        for tag in rows:
            if line.startswith(tag + " {"):
                rows[tag].append(json.loads(line[len(tag) + 1:]))


def summ(vals):
    return "min %.2f mean %.2f max %.2f" % (min(vals), statistics.mean(vals), max(vals)) if vals else "-"


def report(label, rs):
    rs = [r for r in rs if "road_grade_max" in r]
    if not rs:
        return
    print("== %s (%d)" % (label, len(rs)))
    for k in ["lots", "road_grade_max", "road_grade4_max", "terrain_relief", "cut_max", "fill_max", "foundations_above_ground", "foundation_gap_max",
              "foundations_showing", "foundation_depth_max", "porch_steps_added", "retaining_walls", "retaining_step_max", "pad_above_road_max", "ground_slope_p99", "ground_slope_max", "walk_pokes", "walk_poke_max", "walk_gap_max", "candy_low", "candy_over_ground_min", "candy", "candy_dropped",
              "props_floating", "props_floating_max", "props_buried", "nav_spawn_walkable", "nav_nests_cut_off", "plan_ms", "build_ms"]:
        vals = [r[k] for r in rs if isinstance(r.get(k), (int, float))]
        if vals:
            print("  %-26s %s  (sum %.1f)" % (k, summ(vals), sum(vals)))
    for k in ["pad_minus_road", "yard_vs_pad"]:
        lo = [r[k][0] for r in rs if k in r]
        hi = [r[k][1] for r in rs if k in r]
        if lo:
            print("  %-26s low %s | high %s" % (k, summ(lo), summ(hi)))
    ids = [i for r in rs for i in r.get("props_floating_ids", [])]
    if ids:
        print("  off-ground props:", ids[:16])


report("hood standalone", [r for r in rows["SWEEP"] if r.get("kind") == "neighborhood"])
report("hood embedded", rows["SWEEP_EMB"])
