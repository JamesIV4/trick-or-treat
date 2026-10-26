"""Thorn vines (vines / batch03): three bramble knots and the thorn root mass (nest with a heart).

  blender --background --factory-startup --python art/team/vines/batch03/build_vines.py -- [keys] [--nosheet]
  keys: knot_a knot_b knot_c root_mass (all when none)
"""
from __future__ import annotations

import math
import pathlib
import sys

sys.path.insert(0, str(pathlib.Path(__file__).resolve().parent))
import vine_kit as VK  # noqa: E402
from vine_kit import D, F, K, M, TAU, V  # noqa: E402


def cyl(R, a, z):
    return V((math.cos(a) * R, math.sin(a) * R, z))


def add_roots(objs, rng, count, L=(0.8, 1.1), r0=(0.14, 0.18), phase=0.0):
    for k in range(count):
        a = phase + k * TAU / count + rng.uniform(-0.3, 0.3)
        ob, _, _ = VK.ground_root(f"root{k}", a, rng.uniform(*L), rng.uniform(*r0), rng)
        objs.append(ob)


def add_leaves(objs, stems, rng, count, size=(0.26, 0.34), frac=(0.25, 0.8)):
    for k in range(count):
        pts, radii = stems[k % len(stems)]
        tot = VK.arclen(pts)[-1]
        p, r, i = VK.sample(pts, radii, tot * rng.uniform(*frac))
        t = (pts[i + 1] - pts[i]).normalized()
        out = V((p.x, p.y, 0))
        out = out.normalized() if out.length > 1e-3 else t.orthogonal().normalized()
        side = (out - t * out.dot(t))
        side = side.normalized() if side.length > 1e-3 else t.orthogonal().normalized()
        d = (side * 0.8 + t * 0.45 + V((0, 0, 0.15))).normalized()
        objs.append(VK.leaf(f"leaf{k}", p + side * r * 0.7, d, V((0, 0, 1)), rng.uniform(*size)))


# ================================================================ knot A: corkscrew spire
def knot_a():
    K.reset()
    rng = M.rnd(11)
    objs, stems = [], []
    S = VK.Spines()
    exits = [2.15, 1.48, 1.0, 1.8]
    reach = [0.72, 0.84, 0.8, 0.82]
    spin = 1.0
    for k in range(4):
        a0 = k * TAU / 4 + rng.uniform(-0.15, 0.15)
        zs = [-0.25, 0.18, 0.55, 0.92, 1.28, 1.62, 1.95]
        Rs = [0.6, 0.38, 0.22, 0.17, 0.16, 0.15, 0.14]
        ctrl = []
        for i, z in enumerate(zs):
            if z > exits[k] - 0.1:
                break
            ctrl.append(cyl(Rs[i], a0 + spin * i, z))
        a = a0 + spin * len(ctrl) - 0.4
        hk = exits[k]
        ctrl.append(cyl(0.34, a, hk + 0.16))
        ctrl.append(cyl(reach[k], a + 0.15, hk + 0.24))
        R0 = 0.25 + 0.04 * (k % 2)
        pts = VK.with_curl(ctrl, V((0, 0, -1)), R0, turns=1.15, n=9, per=2)
        ob, pts, radii = VK.stem(f"stem{k}", pts, 0.16, 0.045, power=0.9)
        objs.append(ob)
        stems.append((pts, radii))
        VK.spines_on(S, pts, radii, rng, out_fn=VK.radial(0.55), start=0.35, size=(2.6, 3.6), min_len=0.15)
        if k == 1:
            t = (pts[-1] - pts[-2]).normalized()
            objs += VK.bud("bud0", pts[-1] - t * 0.02, t, r=0.1)
    # a short side shoot with a bud low on the trunk
    a = 2.6
    sh = VK.smooth([cyl(0.18, a, 0.62), cyl(0.42, a + 0.1, 0.78), cyl(0.62, a + 0.3, 0.98)], 3)
    ob, pts, radii = VK.stem("shoot", sh, 0.075, 0.05, sides=6, cap=0.0)
    objs.append(ob)
    t = (pts[-1] - pts[-2]).normalized()
    objs += VK.bud("bud1", pts[-1], t, r=0.11)
    add_roots(objs, rng, 5, L=(0.85, 1.05), r0=(0.15, 0.18))
    add_leaves(objs, stems, rng, 4)
    objs.append(S.build())
    print("spines", S.count)
    VK.finish_knot(objs, "thorn_knot_a")


# ================================================================ knot B: bramble dome
def knot_b():
    K.reset()
    rng = M.rnd(23)
    objs, stems = [], []
    S = VK.Spines()
    for k in range(3):
        a = k * TAU / 3 + 0.35
        b = a + math.radians(150)
        Pa, Pb = cyl(1.0, a, 0), cyl(0.95, b, 0)
        mid = (Pa + Pb) * 0.5
        perp = V((-(Pb - Pa).y, (Pb - Pa).x, 0)).normalized()
        H = (1.55, 1.35, 1.48)[k]
        ctrl = []
        for i in range(9):
            u = i / 8
            h = Pa.lerp(Pb, u) + perp * (0.22 * math.sin(math.pi * u) * (1 if k % 2 else -1))
            # pull the shoulders up and in a little: a rounder hoop than a parabola
            h = h.lerp(mid, 0.15 * math.sin(math.pi * u))
            ctrl.append(V((h.x, h.y, -0.25 + (H + 0.25) * math.sin(math.pi * u) ** 0.9)))
        pts = VK.wobble(VK.smooth(ctrl, 2), 0.05, wave=4.0, phase=k)
        ob, pts, radii = VK.stem(f"arch{k}", pts, 0.19, 0.12, power=1.0, swell=lambda u: 1 - 0.18 * math.sin(math.pi * u))
        objs.append(ob)
        stems.append((pts, radii))
        VK.spines_on(S, pts, radii, rng, out_fn=VK.radial(0.4), up_bias=0.45, start=0.3, end=0.92,
                     size=(2.5, 3.4), every=(0.3, 0.42))
        # a little curled side tendril from the shoulder
        tot = VK.arclen(pts)[-1]
        p, r, i = VK.sample(pts, radii, tot * (0.3 if k != 1 else 0.68))
        out = V((p.x, p.y, 0)).normalized()
        ctrl = [p, p + out * 0.3 + V((0, 0, 0.22)), p + out * 0.5 + V((0, 0, 0.42))]
        tp = VK.with_curl(ctrl, -out + V((0, 0, -0.3)), 0.12, turns=1.1, n=7)
        ob, tp, tr = VK.stem(f"tend{k}", tp, 0.075, 0.03, sides=6, cap=0.6)
        objs.append(ob)
        VK.spines_on(S, tp, tr, rng, out_fn=VK.radial(0.5), start=0.15, end=0.7, size=(2.4, 3.0), min_len=0.12,
                     every=(0.2, 0.26))
        if k == 2:
            t = (tp[-1] - tp[-2]).normalized()
            objs += VK.bud("bud0", tp[-1], t, r=0.1)
    # central spire: twisted, curling over at the top
    ctrl = [cyl(0.1, 0.0, -0.25), cyl(0.12, 1.3, 0.5), cyl(0.12, 2.6, 1.1), cyl(0.12, 3.9, 1.7),
            cyl(0.1, 5.0, 2.12), cyl(0.3, 5.4, 2.32)]
    pts = VK.with_curl(ctrl, V((0, 0, -1)), 0.22, turns=1.2, n=9, per=2)
    ob, pts, radii = VK.stem("spire", pts, 0.2, 0.045, power=0.85)
    objs.append(ob)
    stems.append((pts, radii))
    VK.spines_on(S, pts, radii, rng, out_fn=VK.radial(0.3), up_bias=0.2, start=0.9, size=(2.4, 3.4), min_len=0.14)
    p, r, i = VK.sample(pts, radii, 1.25)
    objs += VK.bud("bud1", p + V((-0.8, -0.6, 0)).normalized() * r * 0.85, V((-0.8, -0.6, 0.5)), r=0.1)
    add_roots(objs, rng, 4, L=(0.7, 0.95), r0=(0.13, 0.16), phase=0.9)
    add_leaves(objs, stems, rng, 4)
    objs.append(S.build())
    print("spines", S.count)
    VK.finish_knot(objs, "thorn_knot_b")


# ================================================================ knot C: shepherd's crook
def knot_c():
    K.reset()
    rng = M.rnd(37)
    objs, stems = [], []
    S = VK.Spines()
    ctrl = [V((0.0, 0.0, -0.25)), V((0.1, 0.06, 0.4)), V((-0.06, 0.03, 0.95)), V((0.04, -0.05, 1.5)),
            V((0.22, 0.0, 1.98)), V((0.5, 0.02, 2.24)), V((0.78, 0.0, 2.27))]
    main = VK.with_curl(ctrl, V((0, 0, -1)), 0.32, turns=1.25, n=11, per=2)
    ob, main, mr = VK.stem("main", main, 0.24, 0.055, sides=8, power=0.85,
                           stripes=("vine", None, None, None, "hedge_dark", None, None, None))
    objs.append(ob)
    stems.append((main, mr))
    VK.spines_on(S, main, mr, rng, out_fn=VK.radial(0.3), up_bias=0.2, start=0.3, size=(2.3, 3.2),
                 every=(0.22, 0.32))
    # companions wound round the main stem, peeling off to the other side in curls
    mf = F._frames(main)
    marc = VK.arclen(main)
    for c, (s_end, ph, peel, R0) in enumerate(((1.45, 0.0, V((-0.7, -0.45, 0.25)), 0.2),
                                               (0.95, 2.5, V((-0.75, 0.5, 0.2)), 0.18))):
        pts = []
        for p, (t, side, up), s in zip(main, mf, marc):
            if s > s_end:
                break
            a = ph + s * 3.4
            pts.append(p + (side * math.cos(a) + up * math.sin(a)) * 0.21)
        pts[0] = pts[0] + V((0, 0, -0.1))
        q = pts[-1]
        ctrl = pts[-2:] + [q + peel * 0.5, q + peel * 1.0 + V((0, 0, 0.05))]
        tail = VK.with_curl(ctrl, V((0, 0, -1)), R0, turns=1.15, n=9)
        pts = pts[:-2] + tail
        ob, pts, radii = VK.stem(f"comp{c}", pts, 0.12, 0.04, sides=6, power=0.9)
        objs.append(ob)
        stems.append((pts, radii))
        VK.spines_on(S, pts, radii, rng, out_fn=VK.radial(0.6), start=0.6, size=(2.6, 3.6), min_len=0.13,
                     every=(0.24, 0.34))
        if c == 1:
            t = (pts[-1] - pts[-2]).normalized()
            objs += VK.bud("bud0", pts[-1], t, r=0.1)
    # a low arch on the crook side, diving back into the ground
    ctrl = [V((0.12, 0.1, 0.35)), V((0.5, 0.25, 0.75)), V((0.85, 0.35, 0.6)), V((1.0, 0.4, -0.25))]
    pts = VK.wobble(VK.smooth(ctrl, 3), 0.03, phase=1.0)
    ob, pts, radii = VK.stem("arch", pts, 0.12, 0.09, sides=6, power=1.0, cap=0.0)
    objs.append(ob)
    stems.append((pts, radii))
    VK.spines_on(S, pts, radii, rng, out_fn=VK.radial(0.3), up_bias=0.5, start=0.2, end=0.75, size=(2.6, 3.4),
                 min_len=0.15)
    p, r, i = VK.sample(pts, radii, 0.55)
    objs += VK.bud("bud1", p + V((0, 0, r * 0.8)), V((0.2, -0.3, 1)), r=0.1)
    add_roots(objs, rng, 4, L=(0.8, 1.0), r0=(0.15, 0.19), phase=2.0)
    add_leaves(objs, stems, rng, 4)
    objs.append(S.build())
    print("spines", S.count)
    VK.finish_knot(objs, "thorn_knot_c")


# ================================================================ thorn root mass
HEART = V((0, 0, 1.5))


def build_heart(rng):
    c = HEART
    objs = []

    def seed(co):
        if co.z > 0:
            k = 1 - 0.2 * co.z ** 2
            co = V((co.x * k, co.y * k, co.z))
        return co
    core = M.blob("core", c, (0.6, 0.6, 0.66), segs=14, rings=9, color="glow_purple", warp=seed)
    objs.append(core)
    # husk petals: dark veined plum shell cupping the core from below, gaps between them glow
    n = 5
    for k in range(n):
        phi = math.radians(-90 + 36 + 72 * k)
        es = [-84, -60, -32, -4, 20, 40, 56]
        ws = [0.12, 0.42, 0.6, 0.62, 0.5, 0.26, 0.0]
        pts, ups = [], []
        for e in es:
            e = math.radians(e)
            dvec = V((math.cos(e) * math.cos(phi), math.cos(e) * math.sin(phi), math.sin(e)))
            co = seed(V((dvec.x * 0.6, dvec.y * 0.6, dvec.z * 0.66)))
            pts.append(c + co + dvec * (0.08 - 0.03 * max(e, 0)))
            ups.append(dvec)
        objs.append(F.blade(f"petal{k}", pts, ws, thick=0.05, color="spider_plum", mat="bark", up=ups,
                            under_color="bat_fur"))
        # glowing midrib vein on the petal
        vein = [p + u * 0.05 for p, u in zip(pts[1:6], ups[1:6])]
        objs.append(F.sweep(f"pvein{k}", vein, [0.02, 0.026, 0.024, 0.018, 0.01], sides=5, color="glow_purple",
                            mat="flat"))
        # dark vein over the bare upper core, from the petal tip to the crown
        phi2 = phi + math.radians(36)
        vp = []
        for e in (8, 30, 52, 72, 86):
            er = math.radians(e)
            dvec = V((math.cos(er) * math.cos(phi2 + math.radians(10) * math.sin(er * 3)),
                      math.cos(er) * math.sin(phi2 + math.radians(10) * math.sin(er * 3)), math.sin(er)))
            co = seed(V((dvec.x * 0.6, dvec.y * 0.6, dvec.z * 0.66)))
            vp.append(c + co * 1.03)
        objs.append(F.sweep(f"dvein{k}", vp, [0.03, 0.034, 0.032, 0.03, 0.03], sides=5, color="spider_plum",
                            mat="flat", cap_offset=0.5))
    # crown nub with a tiny curled sprout
    nub = M.blob("nub", c + V((0, 0, 0.66)), (0.1, 0.1, 0.07), segs=8, rings=4, color="spider_plum")
    objs.append(nub)
    sp = VK.with_curl([c + V((0, 0, 0.68)), c + V((0.03, 0, 0.8)), c + V((0.1, 0, 0.9))], V((0, 0, -1)), 0.06,
                      turns=1.1, n=6)
    ob, _, _ = VK.stem("sprout", sp, 0.035, 0.015, sides=5, color="vine", stripes=None, mat="flat", cap=0.6)
    objs.append(ob)
    return objs


def root_mass():
    K.reset()
    rng = M.rnd(77)
    objs = []
    S = VK.Spines()
    stems = []

    def lobes(co):
        a = math.atan2(co.y, co.x)
        k = 1 + 0.1 * math.cos(5 * (a + co.z * 1.4))
        return V((co.x * k, co.y * k, co.z))
    # cradle stump: twisted root base with a cup the heart sits in
    prof = [(1.15, -0.12, 0), (1.0, 0.12, 0), (0.78, 0.4, 0), (0.62, 0.66, 0), (0.62, 0.86, 1), (0.66, 0.98, 1),
            (0.56, 1.04, 1), (0.42, 0.96, 1), (0.0, 0.8, 1)]
    stump = M.band_lathe("stump", prof, segs=16, colors=("vine_dark", "hedge_dark"), mats=("bark",), warp=lobes,
                         project="cyl")
    objs.append(stump)

    # cage roots: rope-twisted, rising from a wide flare, wrapping the heart, crowning in outward curls
    angs = [-45, 5, 55, 92, 128, 175, -135]
    tops = [3.15, 3.45, 3.6, 3.75, 3.55, 3.35, 3.2]
    spin = [1, -1, 1, -1, 1, -1, -1]          # woven: neighbours twist opposite ways; the front pair part
    front = [True, False, False, False, False, False, True]
    for k, (deg, top) in enumerate(zip(angs, tops)):
        a0 = math.radians(deg)
        s = spin[k]
        # hug the lawn first (knotting into the ground), then rise and wrap the heart
        zs = [-0.2, 0.08, 0.34, 0.78, 1.36, 1.95, 2.45, top - 0.5]
        Rs = [1.92, 1.74, 1.46, 1.22, 1.12 if not front[k] else 1.2, 1.02 if not front[k] else 1.1, 0.8, 0.6]
        tw = 0.15 if front[k] else 0.3
        ctrl = [cyl(R, a0 + s * tw * max(z - 0.3, 0) + rng.uniform(-0.04, 0.04), z) for R, z in zip(Rs, zs)]
        pts = VK.wobble(VK.smooth(ctrl, 2), 0.05, wave=3.0, phase=k * 1.7, fade_in=0.8)
        t = (pts[-1] - pts[-2]).normalized()
        a1 = math.atan2(pts[-1].y, pts[-1].x)
        out = V((math.cos(a1), math.sin(a1), 0))
        # turn up and outward, then curl out and down over the rim (the crown)
        p1 = pts[-1] + (t * 0.45 + out * 0.7).normalized() * 0.34
        pts.append(p1)
        t = (pts[-1] - pts[-2]).normalized()
        tang = V((-math.sin(a1), math.cos(a1), 0)) * (1 if k % 2 else -1)
        pts += VK.curl(pts[-1], t, out * 0.75 + tang * 0.6 - V((0, 0, 0.3)), rng.uniform(0.27, 0.33), turns=1.15, n=7)
        knobs = (lambda ph: (lambda u: 1 + 0.1 * math.sin(u * 23 + ph) * (1 - u)))(k * 2.1)
        ob, pts, radii = VK.stem(f"cage{k}", pts, 0.34, 0.085, sides=9, power=0.75, profile=VK.rope, swell=knobs,
                                 stripes=("vine", None, None, None, None, None, "pine_deep", None, None), twist=0.32)
        objs.append(ob)
        stems.append((pts, radii))

        def skip(p, d, L):
            tip = p + d * L
            return (tip - HEART).length < 0.95 or (p - HEART).length < 1.3 and d.dot((HEART - p).normalized()) > 0.1
        VK.spines_on(S, pts, radii, rng, out_fn=VK.radial(0.8), up_bias=0.15, start=0.55, size=(2.2, 3.0),
                     min_len=0.16, max_len=0.85, every=(0.38, 0.5), skip=skip)
    # lintel over the front opening
    def at_z(pts, z):
        return min(pts[:len(pts) // 2 + 4], key=lambda p: abs(p.z - z))
    pl, pr = at_z(stems[6][0], 2.15), at_z(stems[0][0], 2.25)
    al, ar = math.atan2(pl.y, pl.x), math.atan2(pr.y, pr.x)
    if al > ar:
        al -= TAU
    ctrl = [pl, cyl(1.02, al + (ar - al) * 0.3, 2.5), cyl(1.02, al + (ar - al) * 0.68, 2.55), pr]
    pts = VK.wobble(VK.smooth(ctrl, 3), 0.04, wave=5.0)
    ob, pts, radii = VK.stem("lintel", pts, 0.13, 0.11, sides=7, power=1.0, swell=lambda u: 1 - 0.2 * math.sin(math.pi * u))
    objs.append(ob)
    VK.spines_on(S, pts, radii, rng, out_fn=lambda p: V((0, -0.6, 0.3)), up_bias=0.3, start=0.25, end=0.85,
                 size=(2.4, 3.0), min_len=0.18, every=(0.28, 0.36),
                 skip=lambda p, d, L: (p + d * L - HEART).length < 0.95)
    # stump spines
    for k in range(9):
        a = TAU * k / 9 + 0.2
        z = rng.uniform(0.25, 0.6)
        R = 1.0 - (z - 0.12) * 0.75
        d = V((math.cos(a), math.sin(a), 0.55)).normalized()
        S.add(cyl(R * 0.9, a, z), d, rng.uniform(0.45, 0.6), hook=V((0, 0, 1)))
    # ground spill: creeping vines out over the lawn, ending in little curls
    for k in range(7):
        a = TAU * k / 7 + 0.25 + rng.uniform(-0.15, 0.15)
        L = rng.uniform(1.7, 1.86)
        w = rng.uniform(-0.35, 0.35)
        ctrl = []
        for i in range(5):
            u = i / 4
            R = 0.95 + (L - 0.95) * u
            ctrl.append(cyl(R, a + w * math.sin(u * 2.4), 0.12 if i == 0 else 0.07 + 0.03 * math.sin(u * 7)))
        pts = VK.smooth(ctrl, 2)
        t = (pts[-1] - pts[-2]).normalized()
        nrm = V((0, 0, 1)) if k % 2 == 0 else V((-t.y, t.x, 0)) * (1 if k % 3 else -1)
        pts += VK.curl(pts[-1], t, nrm, 0.13, turns=1.05, n=6)
        ob, pts, radii = VK.stem(f"spill{k}", pts, 0.11, 0.035, sides=6, power=0.9, twist=0.4)
        objs.append(ob)
        VK.spines_on(S, pts, radii, rng, out_fn=None, up_bias=1.0, start=0.35, end=0.85, size=(2.6, 3.4),
                     min_len=0.14, max_len=0.34, every=(0.38, 0.5), embed=0.2)
        stems.append((pts, radii))
    # crown buds
    for k, ci in enumerate((1, 3, 5)):
        pts, radii = stems[ci]
        t = (pts[-1] - pts[-2]).normalized()
        objs += VK.bud(f"bud{k}", pts[-1], t, r=0.12)
    add_leaves(objs, stems[:7], rng, 5, size=(0.34, 0.42), frac=(0.15, 0.55))
    objs.append(S.build())
    print("spines", S.count)
    heart = build_heart(rng)
    VK.finish_parts("thorn_root_mass", objs, heart, (0, 1.5, 0))


M.run({"knot_a": knot_a, "knot_b": knot_b, "knot_c": knot_c, "root_mass": root_mass})
