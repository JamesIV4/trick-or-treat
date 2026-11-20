"""The reviewer's tele5 measure (review/v4/tele5.py, same functions from tele4.py), on any showcase folders.
usage: python tele_cmp.py <id> <side>[,<side>...]   (read only; prints, writes nothing)"""
import sys, os
import numpy as np
V4 = r"S:\Repos\Games\trick-or-treat\art\review\showcase\after\review\v4"
src = open(os.path.join(V4, "tele4.py")).read().split("clip = sys.argv[1]")[0]
exec(src)


def peaks(rows):
    P = np.array([r[1] for r in rows]); out = []
    for i in range(4, len(P) - 4):
        if P[i] == P[i - 2:i + 3].max() and P[i] - min(P[i - 4:i].min(), P[i + 1:i + 5].min()) > 3:
            out.append(rows[i])
    return out


clip = sys.argv[1]
for side in sys.argv[2].split(","):
    path = side if os.path.isabs(side) else os.path.join(ROOT, side)
    path = os.path.join(path, clip + ".mp4")
    allP, allG, allA = [], [], []
    for a, b in [(880, 1010), (1120, 1250)]:
        pk = peaks(measure(load(path, a, b), a))
        allP += [p[1] for p in pk]; allA += [p[2] for p in pk]; allG += [p[3] for p in pk]
    print(f"{clip:18s} {side:8s} n={len(allP):2d} P_med={np.median(allP) if allP else 0:5.2f} P_max={max(allP) if allP else 0:5.1f} "
          f"area_med={np.median(allA) if allA else 0:5.1f}% gain_med={np.median(allG) if allG else 0:4.0f}", flush=True)
