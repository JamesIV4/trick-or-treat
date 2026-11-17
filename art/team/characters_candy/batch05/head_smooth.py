"""Measure how smoothly the kid's head moves, from a TOT_RIG_DEBUG trace.

    python head_smooth.py <game stdout log> [--plot out.png] [--label text] [--beats x.beats.json]

Reads the `RIGDBG ... HEAD dt=.. ev=.. pitch yaw roll y z W worldy g=.. calm=..` lines KidRig prints
when the environment variable TOT_RIG_DEBUG is set (head angles relative to the controller's node).
Reports, for walking (on the ground, 15-75% of run speed), running (over 75%), standing, and the
0.5 s after each candy pickup:
  - head turn speed (deg/s) and turn acceleration (deg/s^2): median, 95th, 99.5th percentile, max
  - head vertical acceleration (m/s^2)
  - snaps: frames where the head turns more than 2.5 deg in one 60 fps frame's worth of time
  - kinks: frames where the turn speed changes by more than 120 deg/s from one frame to the next
A smooth head has no snaps and few kinks; a jerky one has a long tail in the acceleration figures.
(--beats is accepted for the caller's convenience and ignored: the split is by speed, not by beat.)
"""
import argparse
import re
import sys

import numpy as np

EV_CELEBRATE = 1 << 9
LINE = re.compile(
    r"RIGDBG t=([\d.]+) state=(\S*) sf=([\d.-]+) .*? HEAD dt=([\d.]+) ev=(\d+) ([\d.-]+) ([\d.-]+) ([\d.-]+) ([\d.-]+) ([\d.-]+)"
    r"(?: W ([\d.-]+) g=([\d.]+) calm=([\d.]+))?")


def read(path):
    raw = open(path, "rb").read()
    text = raw.decode("utf-16") if raw[:2] in (b"\xff\xfe", b"\xfe\xff") else raw.decode("utf-8", "replace")
    rows = []
    for m in LINE.finditer(text):
        t, state, sf, dt, ev, p, y, r, hy, hz, wy, g, calm = m.groups()
        rows.append((float(t), state, float(sf), float(dt), int(ev), float(p), float(y), float(r),
                     float(wy if wy else hy), float(hz), float(g) if g else 1.0, float(calm) if calm else 1.0))
    return rows


def line(name, v):
    if len(v) == 0:
        return f"    {name}: no frames"
    q = np.percentile(v, [50, 95, 99.5])
    return f"    {name}: median {q[0]:9.2f}   p95 {q[1]:9.2f}   p99.5 {q[2]:9.2f}   max {np.max(v):9.2f}"


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("log")
    ap.add_argument("--plot")
    ap.add_argument("--label", default="")
    ap.add_argument("--beats")
    a = ap.parse_args()
    rows = read(a.log)
    if len(rows) < 100:
        sys.exit(f"only {len(rows)} trace lines in {a.log}")
    t = np.array([r[0] for r in rows])
    sf = np.array([r[2] for r in rows])
    dt = np.clip(np.array([r[3] for r in rows]), 1e-4, 0.1)
    ev = np.array([r[4] for r in rows])
    ang = np.stack([np.unwrap([r[5] for r in rows]), np.unwrap([r[6] for r in rows]), np.unwrap([r[7] for r in rows])], axis=1)
    hy = np.array([r[8] for r in rows])
    ground = np.array([r[10] > 0.98 and r[11] > 0.98 for r in rows])

    w = np.degrees(np.diff(ang, axis=0, prepend=ang[:1])) / dt[:, None]  # deg/s per axis
    speed = np.linalg.norm(w, axis=1)
    dw = np.linalg.norm(np.diff(w, axis=0, prepend=w[:1]), axis=1)  # change in turn speed, deg/s per frame
    acc = dw / dt
    vy = np.diff(hy, prepend=hy[:1]) / dt
    ay = np.abs(np.diff(vy, prepend=vy[:1])) / dt
    snap = speed * (1.0 / 60.0) > 2.5
    kink = dw > 120.0

    cele_t = t[(ev & EV_CELEBRATE) != 0]
    after_cele = np.zeros(len(t), bool)
    for c in cele_t:
        after_cele |= (t >= c) & (t < c + 0.5)
    sets = (
        ("standing", ground & (sf <= 0.15)),
        ("walking", ground & (sf > 0.15) & (sf <= 0.75)),
        ("running", ground & (sf > 0.75)),
        ("0.5 s after each candy pickup, on the ground", ground & after_cele),
    )
    fps = 1.0 / np.median(dt)
    print(f"{a.label or a.log}: {len(rows)} frames, {t[-1] - t[0]:.1f} s, median {fps:.0f} fps, {len(cele_t)} candy pickups, "
          f"{ground.mean() * 100:.0f}% of frames on the ground")
    for title, mask in sets:
        mask = mask.copy()
        mask[:3] = False
        n = int(mask.sum())
        secs = float(dt[mask].sum())
        print(f"  {title}: {n} frames, {secs:.1f} s")
        if n == 0:
            continue
        print(line("turn speed, deg/s     ", speed[mask]))
        print(line("turn accel, deg/s^2   ", acc[mask]))
        print(line("vertical accel, m/s^2 ", ay[mask]))
        print(f"    snaps: {int((snap & mask).sum())} ({(snap & mask).sum() / max(secs, 1e-3):.2f} per second)   "
              f"kinks: {int((kink & mask).sum())} ({(kink & mask).sum() / max(secs, 1e-3):.2f} per second)")

    if a.plot:
        import matplotlib
        matplotlib.use("Agg")
        import matplotlib.pyplot as plt

        # The 6 seconds with the most time on the ground moving, and the most candy.
        loco = ground & (sf > 0.15)
        best, best_score = t[0], -1.0
        for start in np.arange(t[0], max(t[0] + 0.1, t[-1] - 6.0), 0.25):
            m = (t >= start) & (t < start + 6.0)
            if not m.any():
                continue
            score = 6.0 * float(loco[m].mean()) + 0.2 * int(((cele_t >= start) & (cele_t < start + 6.0)).sum())
            if score > best_score:
                best, best_score = start, score
        m = (t >= best) & (t < best + 6.0)
        fig, ax = plt.subplots(4, 1, figsize=(13, 9), sharex=True)
        for i, (nm, col) in enumerate((("pitch", "tab:red"), ("yaw", "tab:green"), ("roll", "tab:blue"))):
            ax[0].plot(t[m], np.degrees(ang[m, i]), color=col, label=nm, lw=1.2)
        ax[0].set_ylabel("head angle, deg")
        ax[0].legend(loc="upper right", ncol=3)
        ax[1].plot(t[m], speed[m], color="k", lw=1.0)
        ax[1].set_ylabel("turn speed, deg/s")
        ax[2].plot(t[m], hy[m] * 100.0, color="tab:purple", lw=1.2)
        ax[2].set_ylabel("head height, cm")
        ax[3].plot(t[m], sf[m], color="tab:gray", lw=1.2)
        ax[3].set_ylabel("speed (1 = run)")
        ax[3].set_xlabel("time, s")
        for c in cele_t[(cele_t >= best) & (cele_t < best + 6.0)]:
            for x in ax:
                x.axvline(c, color="orange", lw=0.8, alpha=0.8)
        ax[0].set_title(f"{a.label}  head motion, 6 s of walking and running (orange lines: candy pickups)")
        for x in ax:
            x.grid(alpha=0.3)
        fig.tight_layout()
        fig.savefig(a.plot, dpi=90)
        print(f"plot: {a.plot}")


if __name__ == "__main__":
    main()
