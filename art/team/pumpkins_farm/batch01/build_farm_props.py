"""hay_bale, hay_bale_round, crate, barrel, wheelbarrow.

blender --background --factory-startup --python art/team/pumpkins_farm/batch01/build_farm_props.py [-- ids...]
"""
import math
import pathlib
import random
import sys

HERE = pathlib.Path(__file__).resolve().parent
sys.path.insert(0, str(HERE))
import bpy  # noqa: E402
from mathutils import Vector  # noqa: E402

import pumpkin_lib as P  # noqa: E402

K = P.K


def tuft(name, base, direction, length=0.09, radius=0.035, color="straw"):
    """Little 4-sided straw spike poking out of a surface."""
    d = Vector(direction).normalized()
    pts = [Vector(base) - d * 0.03, Vector(base) + d * length]
    return K.tube(name, pts, radius=radius, radius_end=radius * 0.45, color=color, sides=4, smooth=False)


def axis_paint(ob, flat_color, bevel_color, end_color=None):
    """Flat faces -> flat_color, bevel faces -> bevel_color, +/-X faces -> end_color."""
    K.paint(ob, bevel_color)
    K.paint(ob, flat_color, where=lambda c, n: max(abs(n.x), abs(n.y), abs(n.z)) > 0.97)
    if end_color:
        K.paint(ob, end_color, where=lambda c, n: abs(n.x) > 0.97)


def hay_bale():
    K.reset()
    L, Wd, Hh = 1.1, 0.6, 0.55
    body = K.box("bale", (L, Wd, Hh), color="straw", loc=(0, 0, Hh / 2), bevel=0.09, segments=2)
    P.cleanup(body)
    axis_paint(body, "straw", "hay", end_color="straw_dark")
    parts = [body]
    for x in (-0.28, 0.3):
        band = K.box(f"band{x}", (0.075, Wd + 0.024, Hh + 0.018), color="rope", loc=(x, 0, Hh / 2 + 0.003),
                     bevel=0.035, segments=1)
        P.cleanup(band)
        parts.append(band)
    rnd = random.Random(4)
    spots = [((-0.55, -0.18, 0.4), (-1, -0.3, 0.6)), ((-0.56, 0.12, 0.18), (-1, 0.2, -0.2)),
             ((0.55, 0.15, 0.42), (1, 0.4, 0.5)), ((0.56, -0.2, 0.2), (1, -0.3, 0)),
             ((-0.1, -0.3, 0.52), (-0.2, -1, 0.7)), ((0.45, -0.3, 0.05), (0.3, -1, -0.2))]
    for i, (b, d) in enumerate(spots):
        parts.append(tuft(f"t{i}", b, d, length=0.06 + rnd.uniform(0, 0.03), radius=0.045,
                          color="straw" if i % 2 else "straw_dark"))
    ob = K.merge(parts, "hay_bale")
    return P.finish([ob], "props/hay_bale", "hay_bale")


def hay_bale_round():
    K.reset()
    R, Hh = 0.6, 1.0
    prof = [(R - 0.07, 0.0), (R - 0.01, 0.05), (R, 0.2), (R + 0.01, 0.5), (R, 0.8), (R - 0.01, 0.95), (R - 0.07, Hh)]
    body = P.lathe("bale", prof, segs=16, color="hay", band_colors=[None, "straw", None, None, "straw", None],
                   top_color="hay", bottom_color="straw_dark", smooth=True)
    parts = [body]
    # spiral on the flat top (3-sided strip, flat side down, barely proud so the top stays walkable)
    pts = []
    turns = 2.4
    n = 20
    for i in range(n):
        t = i / (n - 1)
        a = 2 * math.pi * turns * t
        r = (R - 0.1) * (1 - t) + 0.04
        pts.append((math.cos(a) * r, math.sin(a) * r, Hh + 0.004))
    parts.append(K.tube("spiral", pts, radius=0.022, color="straw_dark", sides=3, smooth=False))
    # a few straw wisps escaping the wrap
    for i, (a, z) in enumerate([(-100, 0.65), (-50, 0.25), (150, 0.8)]):
        ar = math.radians(a)
        b = (math.cos(ar) * R, math.sin(ar) * R, z)
        d = (math.cos(ar), math.sin(ar), 0.4 if i % 2 else -0.3)
        parts.append(tuft(f"t{i}", b, d, length=0.09, radius=0.03, color="straw" if i % 2 else "straw_dark"))
    ob = K.merge(parts, "hay_bale_round")
    return P.finish([ob], "props/hay_bale_round", "hay_bale_round")


def crate():
    K.reset()
    S = 0.7
    parts = [K.box("core", (S - 0.06, S - 0.06, S - 0.04), color="wood_dark", loc=(0, 0, (S - 0.04) / 2))]
    post = 0.085
    # corner posts: chamfered (octagon) prisms, no bottom cap
    c = post / 2
    ch = post * 0.28
    octo = [(-c + ch, -c), (c - ch, -c), (c, -c + ch), (c, c - ch), (c - ch, c), (-c + ch, c), (-c, c - ch), (-c, -c + ch)]
    for sx in (-1, 1):
        for sy in (-1, 1):
            pent = [(-c, -c), (c, -c), (c, c - 2 * ch), (c - 2 * ch, c), (-c, c)]
            pent = [(x * sx, y * sy) for x, y in pent]
            p = K.polygon_prism(f"post{sx}{sy}", P.ccw(pent), S, color="wood", loc=(sx * (S / 2 - c), sy * (S / 2 - c), 0))
            K.paint(p, "wood_light", where=lambda cc, n: n.z > 0.9)
            parts.append(p)
    # side slats (only outer, top, bottom faces)
    inner = S - 2 * post + 0.01
    for k, (z, hgt) in enumerate(((0.14, 0.2), (0.43, 0.2))):
        for side in ("-y", "+y"):
            sgn = -1 if side == "-y" else 1
            parts.append(P.slab(f"sy{k}{side}", (inner, 0.03, hgt), loc=(0, sgn * (S / 2 - 0.022), z + hgt / 2 - 0.1 + 0.1),
                                color="plank", omit=("+x", "-x", "+y" if sgn < 0 else "-y")))
        for side in ("-x", "+x"):
            sgn = -1 if side == "-x" else 1
            parts.append(P.slab(f"sx{k}{side}", (0.03, inner, hgt), loc=(sgn * (S / 2 - 0.022), 0, z + hgt / 2 - 0.1 + 0.1),
                                color="plank", omit=("+y", "-y", "+x" if sgn < 0 else "-x")))
    # top boards: flat, flush with the posts so the player lands on a level deck
    for k, x in enumerate((-0.2, 0.0, 0.2)):
        parts.append(P.slab(f"top{k}", (0.17, S - 0.03, 0.035), loc=(x, 0, S - 0.0175), color="wood_light",
                            omit=("-z",)))
    # painted apple stamp on the front slat
    parts.append(K.cylinder("apple", 0.075, 0.012, color="apple_red", verts=6, rot=(90, 0, 0),
                            loc=(0.0, -(S / 2 - 0.004), 0.53), scale=(1.0, 1.0, 1.0)))
    parts.append(K.cylinder("leafst", 0.03, 0.012, color="apple_green", verts=4, rot=(90, 0, 30),
                            loc=(0.05, -(S / 2 - 0.006), 0.6)))
    ob = K.merge(parts, "crate")
    return P.finish([ob], "props/crate", "crate")


def barrel():
    K.reset()
    prof = [(0.25, 0.0), (0.283, 0.12), (0.3, 0.425), (0.283, 0.73), (0.25, 0.85), (0.215, 0.85), (0.215, 0.815)]
    body = P.lathe("barrel", prof, segs=12, color="wood", column_colors=["wood", "plank"],
                   band_colors=[None, None, None, None, "wood_light", "wood_dark"], top_color="plank",
                   bottom_color="wood_dark", smooth=False)
    parts = [body]
    for z0, z1, r0, r1 in ((0.07, 0.15, 0.278, 0.293), (0.70, 0.78, 0.293, 0.274), (0.39, 0.46, 0.307, 0.307)):
        parts.append(P.lathe(f"hoop{z0}", [(r0 + 0.006, z0), (r1 + 0.006, z1)], segs=12, color="iron",
                             cap_top=False, cap_bottom=False))
    # lid plank seams: two thin dark strips across the lid
    for x in (-0.07, 0.07):
        parts.append(P.slab(f"seam{x}", (0.012, 0.39, 0.004), loc=(x, 0, 0.817), color="wood_dark",
                            omit=("-z",)))
    ob = K.merge(parts, "barrel")
    return P.finish([ob], "props/barrel", "barrel")


def wheelbarrow():
    K.reset()
    parts = []
    # tray: tapered open box, outer plank, inner wood_dark, rim wood_light
    import bmesh
    bm = bmesh.new()
    uv = bm.loops.layers.uv.verify()

    def rect(wx, ly, z, cy=0.0):
        return [bm.verts.new((x, y + cy, z)) for x, y in ((-wx, -ly), (wx, -ly), (wx, ly), (-wx, ly))]

    def face(vs, color):
        f = bm.faces.new(vs)
        u, w = K._uv_cell(color)
        for loop in f.loops:
            loop[uv].uv = (u, w)
        return f

    t = 0.035
    ob_ = rect(0.2, 0.24, 0.36, cy=0.02)   # outer bottom
    ot = rect(0.33, 0.42, 0.64, cy=-0.04)  # outer top rim
    it = rect(0.33 - t, 0.42 - t, 0.64, cy=-0.04)
    ib = rect(0.2 - t, 0.24 - t, 0.36 + t, cy=0.02)
    face(list(reversed(ob_)), "wood_dark")
    for i in range(4):
        j = (i + 1) % 4
        face((ob_[i], ob_[j], ot[j], ot[i]), "plank")
        face((ot[i], ot[j], it[j], it[i]), "wood_light")
        face((it[i], it[j], ib[j], ib[i]), "wood_dark")
    face(ib, "wood_dark")
    bmesh.ops.recalc_face_normals(bm, faces=bm.faces)
    parts.append(P.mesh_from_bm(bm, "tray"))
    # handles run from the wheel fork, under the tray, out the back
    for sx in (-1, 1):
        x = sx * 0.17
        parts.append(K.tube(f"h{sx}", [(x * 0.75, -0.52, 0.2), (x, -0.1, 0.33), (x * 1.25, 0.45, 0.5),
                                       (x * 1.4, 0.72, 0.6)], radius=0.036, color="wood", sides=5, smooth=False))
        parts.append(K.cylinder(f"g{sx}", 0.042, 0.17, color="wood_dark", verts=5, rot=(-70, 0, 0),
                                loc=(x * 1.45, 0.8, 0.63)))
        parts.append(P.slab(f"leg{sx}", (0.065, 0.065, 0.47), loc=(x * 1.17, 0.3, 0.235), color="wood_dark",
                            omit=("-z",)))
    # front wheel: chunky charcoal tyre with a metal_red hub
    parts.append(K.cylinder("tyre", 0.2, 0.11, color="charcoal", verts=12, rot=(0, 90, 0), loc=(0, -0.56, 0.19)))
    parts.append(K.cylinder("hub", 0.095, 0.135, color="metal_red", verts=6, rot=(0, 90, 0), loc=(0, -0.56, 0.19)))
    # three little pumpkins heaped in the tray
    for k, (x, y, z, w, rz, col) in enumerate(((-0.12, 0.1, 0.44, 0.32, 10, "pumpkin"),
                                                (0.13, 0.02, 0.45, 0.3, 50, "pumpkin_light"),
                                                (-0.01, -0.2, 0.5, 0.28, 80, "pumpkin_dark"))):
        pb = P.pumpkin_body(f"p{k}", width=w, height=w * 0.75, lobes=6, groove=0.12, groove_w=0.14, peaks=0,
                            rings=4, color=col, groove_color="rust" if col == "pumpkin_dark" else "pumpkin_dark",
                            fullness=0.7, flatness=0.7, top_dimple=0.025, seed=30 + k, loc=(x, y, z))
        pb.rotation_euler.z = math.radians(rz)
        bpy.context.view_layer.update()
        tp = P.top_point(pb)
        parts.append(pb)
        parts.append(K.cylinder(f"ps{k}", 0.022, 0.07, color="stem_brown", verts=4, radius_top=0.016,
                                loc=(tp.x, tp.y, tp.z + 0.025), rot=(12 * (k - 1), 10, 0)))
    ob = K.merge(parts, "wheelbarrow")
    lo = min(v.co.z for v in ob.data.vertices)
    for v in ob.data.vertices:
        v.co.z -= lo
    return P.finish([ob], "props/wheelbarrow", "wheelbarrow")


BUILDS = {f.__name__: f for f in (hay_bale, hay_bale_round, crate, barrel, wheelbarrow)}

if __name__ == "__main__":
    args = sys.argv[sys.argv.index("--") + 1:] if "--" in sys.argv else list(BUILDS)
    for a in args:
        BUILDS[a]()
