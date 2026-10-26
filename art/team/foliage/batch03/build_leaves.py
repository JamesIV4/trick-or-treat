"""Fallen maple leaves (batch03): leaf_scatter, leaf_single.

Each leaf is a five-lobe maple outline split along the midrib into two halves, folded into a
shallow V with the edges and tip curling up, the two halves in neighbouring tints (reads as a lit and
a shaded half), plus a thin petiole. leaf_scatter lies ~14 of them (8-16 cm, single-sided, seen from
above) loosely over a 1.6 m patch with a few small clumps [v2: 6 big 0.22-0.32 m leaves spread loosely]; leaf_single is one 14 cm two-sided leaf with
its origin at its centre.
"""
import pathlib
import random
import sys

sys.path.insert(0, str(pathlib.Path(__file__).resolve().parent))
from foliage_common import *  # noqa: E402,F401,F403
import foliage_common as F  # noqa: E402

V = Vector
UP = V((0, 0, 1))

PAIRS = [("leaf_crimson", "maple_red"), ("maple_orange", "leaf_tangerine"), ("golden_leaf", "amber"),
         ("rust_leaf", "burnt_amber"), ("maple_red", "leaf_scarlet"), ("leaf_tangerine", "golden_leaf")]


def folded_leaf(acc, center, yaw, size, cols, rnd, tilt=(0.0, 0.0), fold=0.12, curl=0.1, double=False,
                petiole=True, lift=0.0):
    right, left = F.maple_halves(rnd, jitter=1.0)
    ca, sa = math.cos(yaw), math.sin(yaw)
    tx, ty = tilt  # small rotations (radians) about local x and y

    def place(x, y):
        z = fold * size * abs(x) * 2 + curl * size * (max(y, 0.0) * 2) ** 2 + curl * 0.5 * size * (2 * x) ** 2
        # tilt: z += slope along local axes
        z += math.sin(ty) * x * size + math.sin(tx) * y * size
        lx, ly = x * size, y * size
        return V((center[0] + lx * ca - ly * sa, center[1] + lx * sa + ly * ca, center[2] + z + lift))

    for half, col in ((right, cols[0]), (left, cols[1])):
        pts = [place(x, y) for x, y in half]
        nrm = (pts[1] - pts[0]).cross(pts[-1] - pts[0])
        if nrm.z < 0:
            nrm = -nrm
        sh = (UP * 0.75 + nrm.normalized() * 0.25).normalized()
        uv = [(x * size, y * size) for x, y in half]
        acc.poly(pts, col, "flat", shade=sh, uv=uv)
        if double:
            acc.poly(list(reversed(pts)), col, "flat", shade=-sh, uv=list(reversed(uv)))
    if petiole:
        bx, by = right[0]
        b = place(bx, by)
        d = V((-sa, ca, 0)) * -1  # local -y in world
        side = V((ca, sa, 0))
        w = size * 0.035
        L = size * 0.28
        tri = [b + side * w, b - side * w, b + d * L + V((0, 0, size * 0.04))]
        if (tri[1] - tri[0]).cross(tri[2] - tri[0]).z < 0:
            tri = [tri[1], tri[0], tri[2]]
        acc.poly(tri, "stem_brown", "flat", shade=UP)
        if double:
            acc.poly(list(reversed(tri)), "stem_brown", "flat", shade=-UP)


def leaf_scatter():
    K.reset()
    rnd = random.Random(4242)
    acc = F.Acc()
    acc.ob = None
    # a few big leaves spread loosely (user feedback: larger, fewer per patch, not a pile)
    n = 6
    pts = []
    tries = 0
    while len(pts) < n and tries < 5000:
        tries += 1
        p = V((rnd.uniform(-0.58, 0.58), rnd.uniform(-0.58, 0.58), 0))
        if any((p - q).length < 0.42 for q in pts):
            continue
        pts.append(p)
    fams = [PAIRS[0], PAIRS[1], PAIRS[2], PAIRS[3]] + [rnd.choice(PAIRS) for _ in range(n - 4)]
    rnd.shuffle(fams)
    for i, p in enumerate(pts):
        size = rnd.uniform(0.22, 0.32)
        cols = fams[i] if rnd.random() < 0.5 else (fams[i][1], fams[i][0])
        # stacking order lifts later leaves a few mm so any overlap never z-fights
        folded_leaf(acc, (p.x, p.y, 0.004 + 0.004 * i), rnd.uniform(0, math.tau), size, cols, rnd,
                    tilt=(rnd.uniform(-0.06, 0.06), rnd.uniform(-0.06, 0.06)), fold=rnd.uniform(0.05, 0.1),
                    curl=rnd.uniform(0.04, 0.09))
    ob, shade = acc.to_object("leaf_scatter")
    lo = min(v.co.z for v in ob.data.vertices)
    for v in ob.data.vertices:  # nothing below the ground plane
        v.co.z += max(0.0, 0.002 - lo)
    ob.data.update()
    return F.finish_foliage(ob, shade, "props/leaf_scatter", "leaf_scatter", ao_radius=0.08, ao_strength=0.5)


def leaf_single():
    K.reset()
    rnd = random.Random(77)
    acc = F.Acc()
    folded_leaf(acc, (0.0, 0.0, 0.0), 0.0, 0.14, ("maple_orange", "leaf_tangerine"), rnd, fold=0.1, curl=0.1,
                double=True)
    ob, shade = acc.to_object("leaf_single")
    # origin at the leaf's centre (bounding-box centre)
    me = ob.data
    lo = V((min(v.co.x for v in me.vertices), min(v.co.y for v in me.vertices), min(v.co.z for v in me.vertices)))
    hi = V((max(v.co.x for v in me.vertices), max(v.co.y for v in me.vertices), max(v.co.z for v in me.vertices)))
    c = (lo + hi) / 2
    for v in me.vertices:
        v.co -= c
    me.update()
    return F.finish_foliage(ob, shade, "props/leaf_single", "leaf_single", ao_radius=0.05, ao_strength=0.3,
                            ground=False)


if want("leaf_scatter"):
    leaf_scatter()
if want("leaf_single"):
    leaf_single()
