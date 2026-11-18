"""Stage 3c summary: walks, stairs, prop contact, bounce columns, roof decks, platform reach, nests.

    python art\\team\\world\\stage0\\stage3c_stats.py <name> [<name2> ...]

Reads SWEEP / SWEEP_EMB / SWEEP_TOWN lines of each <name>.log in this folder (several logs: one run split in two
processes). Hoods = SWEEP_EMB (the world plays the embedded variant), towns = SWEEP (odd seeds) + SWEEP_TOWN.
"""
import json
import os
import sys

HERE = os.path.dirname(os.path.abspath(__file__))


def load(names):
    rows = {"hood": [], "hood_sa": [], "town": []}
    for n in names:
        with open(os.path.join(HERE, n + ".log"), encoding="utf-8", errors="replace") as f:
            for line in f:
                for tag in ("SWEEP_EMB ", "SWEEP_TOWN ", "SWEEP "):
                    if line.startswith(tag + "{"):
                        d = json.loads(line[len(tag):])
                        if tag == "SWEEP_EMB ":
                            rows["hood"].append(d)
                        elif tag == "SWEEP_TOWN " or d.get("kind") == "town":
                            rows["town"].append(d)
                        else:
                            rows["hood_sa"].append(d)
                        break
    return rows


def tot(rs, k):
    return sum(r.get(k, 0) for r in rs)


def mx(rs, k):
    v = [r[k] for r in rs if k in r]
    return max(v) if v else None


def mn(rs, k):
    v = [r[k] for r in rs if k in r]
    return min(v) if v else None


def report(label, rs):
    if not rs:
        return
    print(f"== {label}: {len(rs)} settlements")
    pr = tot(rs, "platforms_reached_3c"), tot(rs, "platforms_3c")
    print(f"  platforms reached {pr[0]}/{pr[1]} = {100.0 * pr[0] / max(1, pr[1]):.1f}%  (per-settlement min {mn(rs, 'platform_reach_pct')}%); without bounce throws {tot(rs, 'platforms_reached_nobounce')} = {100.0 * tot(rs, 'platforms_reached_nobounce') / max(1, pr[1]):.1f}%")
    print(f"  roof decks {tot(rs, 'roof_decks')}, reached {tot(rs, 'roof_decks_reached')}")
    print(f"  walks: lots {tot(rs, 'walk_lots')}, gap>=0.1 {tot(rs, 'walk_gap_bad')}, gap max {mx(rs, 'walk_gap_max')}, end not on sidewalk "
          f"{tot(rs, 'walk_end_not_sidewalk')}, flags blocked {tot(rs, 'walk_blocked_flags')}, flag over ground max {mx(rs, 'walk_flag_over_ground_max')}, walk slope max {mx(rs, 'walk_slope_max')}")
    print(f"  stairs: flights {tot(rs, 'stair_flights')}, bottom off >0.08 {tot(rs, 'stair_bottom_bad')}, err max {mx(rs, 'stair_bottom_err_max')}, "
          f"riser max {mx(rs, 'stair_riser_max')}, treads max {mx(rs, 'stair_treads_max')}, landings {tot(rs, 'stair_landings')}, steep risers {tot(rs, 'stair_steep_risers')}")
    print(f"  contact: flat {tot(rs, 'contact_flat')} bad {tot(rs, 'contact_flat_bad')} (max {mx(rs, 'contact_flat_max')}); cars {tot(rs, 'contact_cars')} bad "
          f"{tot(rs, 'contact_cars_bad')} (max {mx(rs, 'contact_car_max')}); upright {tot(rs, 'contact_upright')} bad {tot(rs, 'contact_upright_bad')} "
          f"(max {mx(rs, 'contact_upright_max')})")
    print(f"  props floating {tot(rs, 'props_floating')} (max {mx(rs, 'props_floating_max')}), buried {tot(rs, 'props_buried')}")
    bp, bc = tot(rs, "bounce_pads"), tot(rs, "bounce_columns")
    print(f"  bounce: pads {bp}, columns {bc} ({100.0 * bc / max(1, bp):.0f}%), top out of band {tot(rs, 'bounce_top_out_of_band')}, "
          f"top reachable otherwise {tot(rs, 'bounce_top_reachable_otherwise')} (by a long leap {tot(rs, 'bounce_top_long_leap')}), "
          f"bounce pieces dropped {sum(r.get('candy_pattern_dropped', {}).get('bounce', 0) for r in rs)}, deck pieces "
          f"{sum(r.get('candy_lot_patterns', {}).get('deck', 0) for r in rs)} (dropped {sum(r.get('candy_pattern_dropped', {}).get('deck', 0) for r in rs)})")
    print(f"  foundations above ground {tot(rs, 'foundations_above_ground')}, depth max {mx(rs, 'foundation_depth_max')}, walk pokes {tot(rs, 'walk_pokes')}, "
          f"retaining walls {tot(rs, 'retaining_walls')} (step max {mx(rs, 'retaining_step_max')}), candy low {tot(rs, 'candy_low')}, "
          f"candy dropped {tot(rs, 'candy_dropped')}")
    print(f"  yard vs pad {mn([{'v': r['yard_vs_pad'][0]} for r in rs if 'yard_vs_pad' in r], 'v')} .. "
          f"{mx([{'v': r['yard_vs_pad'][1]} for r in rs if 'yard_vs_pad' in r], 'v')}, pad-road "
          f"{mn([{'v': r['pad_minus_road'][0]} for r in rs if 'pad_minus_road' in r], 'v')} .. "
          f"{mx([{'v': r['pad_minus_road'][1]} for r in rs if 'pad_minus_road' in r], 'v')}, slope p99 max {mx(rs, 'ground_slope_p99')}, slope max {mx(rs, 'ground_slope_max')}")
    print(f"  steep (>45 deg) lattice edges in lots {tot(rs, 'lot_steep_edges')}, max {mx(rs, 'lot_steep_max')}; worst at", [r.get('lot_steep_at') for r in rs if r.get('lot_steep_max') == mx(rs, 'lot_steep_max')][:1])
    print(f"  nests {tot(rs, 'nests')}/{tot(rs, 'nests_wanted')}, hearts {tot(rs, 'hearts')}/{tot(rs, 'hearts_wanted')}, cut off {tot(rs, 'nav_nests_cut_off')}, "
          f"candy/lot {sum(r.get('candy_lot_mean', 0) for r in rs) / len(rs):.1f}")
    ex = [x for r in rs for x in r.get("contact_bad_at", [])][:12]
    if ex:
        print("  contact examples:", ex)
    ex = [x for r in rs for x in r.get("stair_bad_at", [])][:8]
    if ex:
        print("  stair examples:", ex)
    ex = [x for r in rs for x in r.get("walk_gap_at", [])][:8]
    if ex:
        print("  walk examples:", ex)
    ex = [x for r in rs for x in r.get("platforms_unreached", [])][:12]
    if ex:
        print("  unreached examples:", ex)
    ex = [x for r in rs for x in r.get("platforms_unreached_nobounce", [])][:12]
    if ex:
        print("  unreached without bounce:", ex)
    rj = [sum(r.get("bounce_rejects", [0, 0, 0])[i] for r in rs) for i in range(3)]
    print("  bounce rejects (axis blocked, canopy, other platform):", rj)
    ex = [x for r in rs for x in r.get("props_floating_ids", [])][:10]
    if ex:
        print("  floating/buried examples:", ex)


def main():
    rows = load(sys.argv[1:])
    report("hoods (embedded)", rows["hood"])
    report("hoods (standalone)", rows["hood_sa"])
    report("towns", rows["town"])
    if rows["hood"]:
        print("  bounce apex (13.5 / 14 / 17):", rows["hood"][0].get("bounce_apex_13.5"), rows["hood"][0].get("bounce_apex_14.0"), rows["hood"][0].get("bounce_apex_17.0"))


if __name__ == "__main__":
    main()
