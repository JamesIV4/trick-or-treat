"""Reads a real_test.ps1 log (MTRACE lines from monsters.cpp) and prints, per test monster:
  - its state timeline (engine frame, movie frame = engine frame - movie start, state, seconds),
  - how evenly it moves on screen while it walks or flies (the drawn picture against the physics body):
    per frame, the on-screen step divided by the frame's dt (px/s), compared with the mean of its
    neighbours; a "stutter" frame is one whose speed is off by more than 25 % of the local mean.

python real_trace.py <log> [--min-speed 30]
"""
import re
import sys

STATES = ["Rise", "Roam", "Windup", "Attack", "Recover", "Hurt", "Dying"]


def main():
    path = sys.argv[1]
    min_speed = float(sys.argv[sys.argv.index("--min-speed") + 1]) if "--min-speed" in sys.argv else 30.0
    text = open(path, encoding="utf-8", errors="replace").read()
    ids = [int(m) for m in re.findall(r"MTEST spawn k=\d+ id=(\d+)", text)]
    m0 = re.search(r"MTEST start .* frame=(\d+)", text)
    f0 = int(m0.group(1)) - 2 if m0 else 0
    rows = {}
    pat = re.compile(r"MTRACE id=(\d+) f=(\d+) t=([\d.]+) dt=([\d.]+) st=(\d) stt=([\d.]+) far=(\d) floor=(\d) drawn=([-\d.]+),([-\d.]+),([-\d.]+) "
                     r"body=([-\d.]+),([-\d.]+),([-\d.]+) sd=([-\d.]+),([-\d.]+) sb=([-\d.]+),([-\d.]+) vy=([-\d.]+)")
    for line in text.splitlines():
        m = pat.search(line)
        if not m:
            continue
        i = int(m.group(1))
        if ids and i not in ids:
            continue
        g = m.groups()
        rows.setdefault(i, []).append(dict(f=int(g[1]), dt=float(g[3]), st=int(g[4]), stt=float(g[5]), far=int(g[6]), floor=int(g[7]),
                                           y=float(g[9]), sd=(float(g[14]), float(g[15])), sb=(float(g[16]), float(g[17])), vy=float(g[18])))
    for i, r in rows.items():
        print(f"== monster id {i}: {len(r)} frames (movie frame = engine frame - {f0})")
        line = []
        for k, a in enumerate(r):
            if k == 0 or a["st"] != r[k - 1]["st"] or a["stt"] < r[k - 1]["stt"] - 1e-4:
                line.append(f"{STATES[a['st']]}@{a['f']}(m{a['f'] - f0})")
        print("  states: " + " ".join(line))
        fars = sum(a["far"] for a in r)
        if fars:
            print(f"  far (LOD) frames: {fars}")
        for key in ("sd", "sb"):
            sp = []
            for k in range(1, len(r)):
                a, b = r[k - 1], r[k]
                if b["dt"] <= 0 or a["st"] not in (1,) or b["st"] not in (1,):
                    sp.append(None)
                    continue
                dx = b[key][0] - a[key][0]
                dy = b[key][1] - a[key][1]
                sp.append((dx * dx + dy * dy) ** 0.5 / b["dt"])
            bad = n = 0
            worst = 0.0
            for k in range(2, len(sp) - 2):
                win = [sp[j] for j in range(k - 2, k + 3) if j != k and sp[j] is not None]
                if sp[k] is None or len(win) < 4:
                    continue
                mean = sum(win) / len(win)
                if mean < min_speed:
                    continue
                n += 1
                off = abs(sp[k] - mean) / mean
                worst = max(worst, off)
                if off > 0.25:
                    bad += 1
            label = "drawn" if key == "sd" else "body "
            print(f"  {label}: {n} moving Roam frames, {bad} stutter frames (>25% off the local mean), worst {worst * 100:.0f}%")
        # Judder: how far each frame's on-screen position is off the straight line (in time) between its two
        # neighbours, in pixels. Smooth motion, even if speeding up or slowing down, gives about 0; a body drawn
        # where the physics ticks left it, with 0, 1 or 2 ticks per frame, gives a sawtooth.
        tt = [0.0]
        for k in range(1, len(r)):
            tt.append(tt[-1] + r[k]["dt"])
        for key in ("sd", "sb"):
            res = []
            for k in range(1, len(r) - 1):
                a, b, c = r[k - 1], r[k], r[k + 1]
                if not (a["st"] == b["st"] == c["st"] == 1) or a["floor"] + b["floor"] + c["floor"] not in (0, 3):
                    continue
                span = tt[k + 1] - tt[k - 1]
                if span <= 0:
                    continue
                f = (tt[k] - tt[k - 1]) / span
                ex = a[key][0] + (c[key][0] - a[key][0]) * f
                ey = a[key][1] + (c[key][1] - a[key][1]) * f
                step = ((c[key][0] - a[key][0]) ** 2 + (c[key][1] - a[key][1]) ** 2) ** 0.5
                if step < 0.5:
                    continue
                res.append(((b[key][0] - ex) ** 2 + (b[key][1] - ey) ** 2) ** 0.5)
            if res:
                res.sort()
                label = "drawn" if key == "sd" else "body "
                print(f"  {label} judder: {len(res)} moving frames, mean {sum(res) / len(res):.2f} px, 95% {res[int(len(res) * 0.95)]:.2f} px, max {res[-1]:.2f} px")
        dts = sorted(a["dt"] for a in r)
        if dts:
            print(f"  frame dt: median {dts[len(dts) // 2] * 1000:.2f} ms, min {dts[0] * 1000:.2f}, max {dts[-1] * 1000:.2f}")


if __name__ == "__main__":
    main()
