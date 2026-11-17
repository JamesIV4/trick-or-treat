"""Measures foot slide from a rig debug trace.

Record with the environment variable TOT_RIG_DEBUG set (PowerShell: $env:TOT_RIG_DEBUG = 1, then
tools\\showcase.ps1 ...; remove it afterwards). KidRig then prints one RIGDBG line per frame to the
showcase log with the world position of each foot's heel and toe.

  python art/team/characters_candy/batch05/p3_slide.py art/review/showcase/wip_p/dragon_kid.log

For each state (walk, run, idle ...) it reports, over the frames in which a sole point is on the
ground (within 8 mm of the floor) in two consecutive frames: how far that point moved along the
ground per frame (mean and worst), next to how far the kid itself travelled. A planted foot that
does not slide moves 0.
"""
import math
import re
import sys
from collections import defaultdict

path = sys.argv[1]
rows = []
raw = open(path, "rb").read()
text = raw.decode("utf-16") if raw[:2] in (b"\xff\xfe", b"\xfe\xff") else raw.decode("utf-8", errors="ignore")  # PowerShell 5 redirects write UTF-16
for line in text.splitlines():
    if "RIGDBG" not in line:
        continue
    m = re.search(r"t=([\d.]+) state=(\w*) sf=([\d.]+) gait=([\d.]+) floor=(-?[\d.]+)((?: -?[\d.]+)+)", line)
    if not m:
        continue
    vals = [float(v) for v in m.group(6).split()]
    pts = [vals[i:i + 3] for i in range(0, len(vals), 3)]
    rows.append((float(m.group(1)), m.group(2), float(m.group(3)), float(m.group(5)), pts))

stats = defaultdict(lambda: {"d": [], "frames": 0, "contact": 0, "body": 0.0})
for a, b in zip(rows, rows[1:]):
    if b[0] - a[0] > 0.05 or a[1] != b[1]:
        continue
    if any(math.hypot(pb[0] - pa[0], pb[2] - pa[2]) > 0.4 for pa, pb in zip(a[4], b[4])):
        continue  # a teleport
    s = stats[a[1]]
    s["frames"] += 1
    touched = False
    for pa, pb in zip(a[4], b[4]):
        if abs(pa[1] - a[3]) < 0.003 and abs(pb[1] - b[3]) < 0.003:  # (abs: in a roll or a tumble the points go far "below" the root)
            s["d"].append(1000 * math.hypot(pb[0] - pa[0], pb[2] - pa[2]))
            touched = True
    s["contact"] += touched
    s["body"] += a[2] * 7.9 * (b[0] - a[0])

print(f"{len(rows)} frames. A sole point counts as on the ground when it is within 3 mm of the floor in two frames running;")
print("slide = how far it moved along the ground between them (a planted foot: 0).")
print("state      frames  with-contact  slide median (mm)  90th pct  share > 5 mm  body travel/frame (mm)")
for name, s in sorted(stats.items(), key=lambda kv: -kv[1]["frames"]):
    if s["frames"] < 5 or not s["d"]:
        continue
    d = sorted(s["d"])
    med, p90 = d[len(d) // 2], d[int(len(d) * 0.9)]
    share = sum(1 for x in d if x > 5.0) / len(d)
    print(f"{name:10s} {s['frames']:6d}  {s['contact']:12d}  {med:17.1f}  {p90:8.1f}  {100 * share:11.0f}%  {1000 * s['body'] / s['frames']:22.1f}")