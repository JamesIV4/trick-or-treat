"""Fidelity pass on pumpkins_farm assets against the concept (ids, sizes and sockets unchanged).

blender --background --factory-startup --python art/team/decor_ground/batch03/build_farm_fix.py [-- ids...]
ids: hay_bale, hay_bale_round, jackolantern_happy, jackolantern_wink, jackolantern_surprised, jackolantern_toothy,
     jackolantern_sleepy

Reuses the pumpkins_farm/batch02 builders unchanged and only overrides what the concept asks for:
  * hay_bale: shaggy straw fringe along every edge (the concept's bales bristle with strands, which
    is what makes them read as hay at 15 m), thin wicker-coloured twine instead of bold rust bands.
  * jack-o'-lanterns: carved pocket floors glow glow_jack_hot (yellow-white) with glow_jack walls, so
    the faces read hot-centred and bright like the concept, instead of a flat orange glow.
"""
import math
import pathlib
import random
import sys

HERE = pathlib.Path(__file__).resolve().parent
sys.path.insert(0, str(HERE))
sys.path.insert(0, str(HERE.parents[1] / "pumpkins_farm" / "batch02"))

import bmesh  # noqa: E402
from mathutils import Vector  # noqa: E402

import farm_lib as F  # noqa: E402
import build_farm_props as P  # noqa: E402
import build_jacks as J  # noqa: E402

K = F.K
ARGS = sys.argv[sys.argv.index("--") + 1:] if "--" in sys.argv else []


def want(i):
    return not ARGS or i in ARGS


# ----------------------------------------------------------------------------------- hay bale
def spikes(bm, base, direction, count, length, radius, rnd, colors=("straw", "hay", "straw_dark", "straw"),
           spread=0.45, along=None, jitter=0.03, droop=0.25, zmax=None):
    """Fan of 3-sided straw spikes (3 tris each, base buried in the bale)."""
    d0 = Vector(direction).normalized()
    side = d0.cross(Vector((0, 0, 1)))
    if side.length < 1e-3:
        side = d0.cross(Vector((1, 0, 0)))
    side.normalize()
    up = side.cross(d0).normalized()
    along = Vector(along).normalized() if along is not None else side
    out = []
    for i in range(count):
        d = (d0 + side * rnd.uniform(-spread, spread) + up * rnd.uniform(-spread, spread) * 0.7).normalized()
        b = Vector(base) + along * rnd.uniform(-jitter, jitter) - d * 0.02
        ln = length * rnd.uniform(0.7, 1.3)
        tip = b + d * ln + Vector((0, 0, -droop * ln * rnd.uniform(0.2, 1.0)))
        if zmax is not None:
            tip.z = min(tip.z, zmax - rnd.uniform(0.0, 0.012))
        s2 = d.cross(Vector((0.3, 0.2, 1))).normalized()
        u2 = s2.cross(d).normalized()
        r = radius * rnd.uniform(0.8, 1.2)
        ring = [bm.verts.new(b + (s2 * math.cos(a) + u2 * math.sin(a)) * r) for a in (0.0, 2.094, 4.189)]
        t = bm.verts.new(tip)
        fs = [bm.faces.new((ring[k], ring[(k + 1) % 3], t)) for k in range(3)]
        for f in fs:
            f.normal_update()
            c = f.calc_center_median()
            if f.normal.dot(c - (b + d * ln * 0.3)) < 0:
                f.normal_flip()
        proj = F.uvl(bm)
        for f in fs:
            for loop in f.loops:
                loop[proj].uv = ((loop.vert.co - b).dot(s2) * 3, (loop.vert.co - b).dot(d) * 3)
        F.paint(bm, fs, colors[i % len(colors)], "straw")
        out += fs
    return out


def hay_bale():
    K.reset()
    L, W, H = 1.1, 0.6, 0.55
    parts = [P.bale_body("bale", L, W, H, r=0.1, flakes=6, pinch=0.03, corner_pts=1)]
    sec = F.rounded_rect(W, H, 0.1, 1)
    fl = L / 6
    for k, x in enumerate((-L / 2 + fl * 1.5, -L / 2 + fl * 4.5)):
        band = P.rope_loop(f"band{k}", sec, x, 0.985, H, out=0.004, radius=0.011, sides=3)
        F_paint_all(band, "wicker")
        parts.append(band)
    rnd = random.Random(12)
    bm = F.bm_new()
    c = 0.1 * (1 - math.sqrt(0.5))  # rounded-corner inset of the edge midline
    ex, ey, zt = L / 2 - c, W / 2 - c, H - c
    # top long edges (most visible from the gameplay camera): dense fringe
    for sy in (-1, 1):
        n = 8
        for i in range(n):
            x = -L / 2 + 0.06 + (L - 0.12) * (i + rnd.uniform(0.2, 0.8)) / n
            spikes(bm, (x, sy * ey, zt), (0, sy * 0.9, 0.5), 3, 0.115, 0.011, rnd, along=(1, 0, 0), jitter=0.04,
                   zmax=H + 0.012)
    # top end edges
    for sx in (-1, 1):
        for i in range(3):
            y = -W / 2 + 0.07 + (W - 0.14) * (i + rnd.uniform(0.2, 0.8)) / 3
            spikes(bm, (sx * ex, y, zt), (sx * 0.9, 0, 0.5), 3, 0.11, 0.011, rnd, along=(0, 1, 0), jitter=0.04,
                   zmax=H + 0.012)
    # vertical edges
    for sx in (-1, 1):
        for sy in (-1, 1):
            for i in range(2):
                z = 0.08 + (H - 0.18) * (i + rnd.uniform(0.2, 0.8)) / 2
                spikes(bm, (sx * ex, sy * ey, z), (sx, sy, 0.15), 2, 0.1, 0.011, rnd, along=(0, 0, 1),
                       jitter=0.04, droop=0.4)
    # a few long stragglers poking out of the cut ends, and a skirt of loose straw at the base
    for sx in (-1, 1):
        for (y, z) in ((-0.12, 0.38), (0.12, 0.2)):
            spikes(bm, (sx * (L / 2 - 0.005), y, z), (sx, 0, 0.1), 2, 0.1, 0.01, rnd, along=(0, 1, 0), droop=0.5)
    parts.append(F.to_object(bm, "fringe", smooth=False))
    ob = K.merge(parts, "hay_bale")
    for v in ob.data.vertices:
        v.co.z = max(v.co.z, 0.0)
    return F.finish([ob], "props/hay_bale", "hay_bale")


def F_paint_all(ob, color):
    bm = bmesh.new()
    bm.from_mesh(ob.data)
    F.paint(bm, bm.faces, color, "flat")
    bm.to_mesh(ob.data)
    bm.free()


# ----------------------------------------------------------------------------------- jacks
_carve = F.carve


def carve_hot(body, features, **kw):
    for f in features:
        if f.get("color", "glow_jack") == "glow_jack":
            f["color"] = "glow_jack_hot"
        f.setdefault("wall", "glow_jack")
    return _carve(body, features, **kw)


F.carve = carve_hot  # build_jacks calls F.carve at run time

def hay_bale_round():
    """Unchanged build, but the twine is wicker instead of bold rust (matches hay_bale)."""
    sweep = F.sweep

    def sweep_twine(*a, **kw):
        if kw.get("color") == "rust":
            kw["color"] = "wicker"
        return sweep(*a, **kw)

    F.sweep = sweep_twine
    try:
        return P.hay_bale_round()
    finally:
        F.sweep = sweep


JACKS = {"jackolantern_happy": J.happy, "jackolantern_wink": J.wink, "jackolantern_surprised": J.surprised,
         "jackolantern_toothy": J.toothy, "jackolantern_sleepy": J.sleepy}

if __name__ == "__main__":
    if want("hay_bale"):
        hay_bale()
    if want("hay_bale_round"):
        hay_bale_round()
    for jid, fn in JACKS.items():
        if want(jid):
            fn()
