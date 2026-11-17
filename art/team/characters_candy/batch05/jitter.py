"""Measure how smoothly the kid moves ON SCREEN under the gameplay camera, from a TOT_JITTER_TEST run.

    python jitter.py <game stdout log> [--plot out.png] [--label text]

Reads the `JIT t=.. dt=.. mag=.. speed=.. body=x,y vis=x,y head=x,y along=.. cam_along=..` lines
(screen positions in pixels of the physics body, the drawn model's root and the head; `along` is the
kid's distance along its line of travel in metres, `cam_along` the camera's).

For the walk and the run (steady speed only) it reports, for each of body / model / head:
  - screen jitter: the frame-to-frame change in the on-screen step, in pixels (a smooth glide has the
    same step every frame, so this is ~0; a judder alternates big and small steps)
  - how many frames the point moves the wrong way or stands still while the kid is travelling
and for the world: how the kid's own step and the camera's step vary from frame to frame.
"""
import argparse
import re
import sys

import numpy as np

LINE = re.compile(
    r"JIT t=([\d.]+) dt=([\d.]+) mag=([\d.]+) speed=([\d.]+) body=([\d.-]+),([\d.-]+) vis=([\d.-]+),([\d.-]+) head=([\d.-]+),([\d.-]+)"
    r" along=([\d.-]+) cam_along=([\d.-]+)")


def read(path):
    raw = open(path, "rb").read()
    text = raw.decode("utf-16") if raw[:2] in (b"\xff\xfe", b"\xfe\xff") else raw.decode("utf-8", "replace")
    return np.array([[float(x) for x in m.groups()] for m in LINE.finditer(text)])


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("log")
    ap.add_argument("--plot")
    ap.add_argument("--label", default="")
    a = ap.parse_args()
    d = read(a.log)
    if len(d) < 100:
        sys.exit(f"only {len(d)} JIT lines in {a.log}")
    t, dt, mag, speed = d[:, 0], d[:, 1], d[:, 2], d[:, 3]
    pts = {"body": d[:, 4:6], "model": d[:, 6:8], "head": d[:, 8:10]}
    along, cam = d[:, 10], d[:, 11]
    print(f"{a.label or a.log}: {len(d)} frames, {t[-1]:.1f} s, median {1.0 / np.median(dt):.0f} fps (min {1.0 / dt.max():.0f}, max {1.0 / dt.min():.0f})")
    out = {}
    for name, m in (("walk", mag == 0.45), ("run", mag == 1.0)):
        if m.sum() < 30:
            continue
        # steady part only: the middle of the segment, speed within 3% of its median
        idx = np.where(m)[0]
        idx = idx[len(idx) // 3:]
        sp = np.median(speed[idx])
        idx = idx[np.abs(speed[idx] - sp) < 0.03 * max(sp, 0.1)]
        if len(idx) < 30:
            print(f"  {name}: no steady stretch (blocked?)")
            continue
        idx = idx[1:]
        print(f"  {name}: {len(idx)} steady frames at {sp:.2f} m/s")
        kid_step = (along[idx] - along[idx - 1]) * 100.0
        cam_step = (cam[idx] - cam[idx - 1]) * 100.0
        print(f"    world step per frame, cm: kid median {np.median(kid_step):.2f} (min {kid_step.min():.2f}, max {kid_step.max():.2f}, frames with no move {np.mean(np.abs(kid_step) < 1e-3) * 100:.0f}%);"
              f" camera median {np.median(cam_step):.2f} (min {cam_step.min():.2f}, max {cam_step.max():.2f})")
        for pn, p in pts.items():
            step = p[idx, 0] - p[idx - 1, 0]  # on-screen x step, px
            jit = np.abs(np.diff(step))
            vstep = p[idx, 1] - p[idx - 1, 1]
            vjit = np.abs(np.diff(vstep))
            print(f"    {pn:5s}: screen x step px median {np.median(step):6.2f}  jitter (change of step) median {np.median(jit):5.2f}  p95 {np.percentile(jit, 95):5.2f}  max {jit.max():6.2f}"
                  f"   | y jitter median {np.median(vjit):5.2f} p95 {np.percentile(vjit, 95):5.2f}")
            out[(name, pn)] = (t[idx], p[idx, 0], step)
    if a.plot and out:
        import matplotlib
        matplotlib.use("Agg")
        import matplotlib.pyplot as plt
        names = [n for n in ("walk", "run") if (n, "body") in out]
        fig, ax = plt.subplots(len(names), 2, figsize=(14, 4.2 * len(names)), squeeze=False)
        for r, n in enumerate(names):
            for pn, col in (("body", "tab:gray"), ("model", "tab:blue"), ("head", "tab:red")):
                tt, x, step = out[(n, pn)]
                k = slice(0, min(len(tt), 90))
                ax[r][0].plot(tt[k], x[k] - x[k][0], color=col, lw=1.0, marker=".", ms=3, label=pn)
                ax[r][1].plot(tt[k], step[k], color=col, lw=1.0, marker=".", ms=3, label=pn)
            ax[r][0].set_title(f"{a.label} {n}: on-screen x position, px (first 90 steady frames)")
            ax[r][1].set_title(f"{a.label} {n}: on-screen x step per frame, px (flat = smooth)")
            for c in (0, 1):
                ax[r][c].grid(alpha=0.3)
                ax[r][c].legend()
        fig.tight_layout()
        fig.savefig(a.plot, dpi=90)
        print(f"plot: {a.plot}")


if __name__ == "__main__":
    main()
