"""Hit / telegraph flash strength per monster, adapted from the reviewer's pulse.py / tele.py
(art/review/showcase/after/review/v2). Read only on the clips.

Per frame t, gain g = luminance(t) - luminance(t-3) inside the centre box (where the monster stands):
the rising edge of a flash (a hit, or each 1/8 s telegraph pulse). Motion moves light around (its gains
are both positive and negative and cancel); a flash only brightens. So the measures are NET:
  net   = mean of g over the box, levels x 10 (how much light the flash adds)
  area  = % of box pixels brightened by > 12 levels minus % darkened by > 12 (how much of the body flashes)
  p97   = 97th percentile of g (the reviewer's pulse: the brightest flashing pixels; motion adds to it)
Each is the peak over the window. Windows: wind-up of attack_1 and attack_2, first and second hit.

usage: python flash_metric.py <side>[,<side>...] [ids] [out.json]
  sides are folders under art/review/showcase (before, after, wip_m, ...)
"""
import sys, os, json, subprocess
import numpy as np

ROOT = r"S:\Repos\Games\trick-or-treat\art\review\showcase"
SW, SH = 320, 360
LAG = 3
IDS = ["monster_pumpkin", "monster_skeleton", "monster_bat", "monster_ghost", "monster_gremlin", "monster_mummy",
       "monster_slime", "monster_scarecrow", "monster_spider", "monster_toadstool"]


def load(path, a, b):
    cmd = ["ffmpeg", "-v", "error", "-i", path, "-vf", f"select='between(n\\,{a}\\,{b})',scale={SW}:{SH}",
           "-fps_mode", "passthrough", "-f", "rawvideo", "-pix_fmt", "rgb24", "-"]
    raw = subprocess.run(cmd, stdout=subprocess.PIPE).stdout
    return np.frombuffer(raw, np.uint8).reshape(-1, SH, SW, 3)


def windows(side, clip):
    beats = json.load(open(os.path.join(ROOT, side, clip + ".beats.json")))["beats"]
    b = {x["name"]: x for x in beats}
    a1, a2, h = b["attack_1"]["start_frame"], b["attack_2"]["start_frame"], b["hit"]["start_frame"]
    return [("tele1", a1 + 30, a1 + 130), ("tele2", a2 + 30, a2 + 130), ("hit1", h + 4, h + 40), ("hit2", h + 48, h + 84)]


def measure(side, clip):
    wins = windows(side, clip)
    lo, hi = min(w[1] for w in wins) - LAG, max(w[2] for w in wins)
    v = load(os.path.join(ROOT, side, clip + ".mp4"), lo, hi).astype(np.float32)
    lum = (v[..., 0] * 0.3 + v[..., 1] * 0.59 + v[..., 2] * 0.11)[:, int(SH * 0.25):int(SH * 0.92), int(SW * 0.18):int(SW * 0.82)]
    out = {}
    for name, a, b in wins:
        best = {"net": (0.0, 0), "area": (0.0, 0), "p97": (0.0, 0)}
        for f in range(a, b + 1):
            t = f - lo
            if t - LAG < 0 or t >= len(lum):
                continue
            g = lum[t] - lum[t - LAG]
            m = {"net": float(g.mean()) * 10, "area": float(((g > 12).mean() - (g < -12).mean()) * 100),
                 "p97": float(np.percentile(g, 97))}
            for k in m:
                if m[k] > best[k][0]:
                    best[k] = (round(m[k], 1), f)
        out[name] = best
    return out


def main():
    sides = sys.argv[1].split(",")
    ids = sys.argv[2].split(",") if len(sys.argv) > 2 and sys.argv[2] != "-" else IDS
    dst = sys.argv[3] if len(sys.argv) > 3 else None
    res = {}
    for clip in ids:
        for side in sides:
            if not os.path.exists(os.path.join(ROOT, side, clip + ".mp4")):
                continue
            r = measure(side, clip)
            res.setdefault(clip, {})[side] = r
            print(f"{clip:18s} {side:7s} " + "  ".join(
                f"{k}: net {r[k]['net'][0]:5.1f}@{r[k]['net'][1]} area {r[k]['area'][0]:4.1f}@{r[k]['area'][1]} p97 {r[k]['p97'][0]:5.1f}"
                for k in r), flush=True)
    if dst:
        json.dump(res, open(dst, "w"), indent=1)


main()
