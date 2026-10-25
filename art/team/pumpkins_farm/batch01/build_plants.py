"""corn_stalks, sunflower, leaf_pile, vine_patch.

blender --background --factory-startup --python art/team/pumpkins_farm/batch01/build_plants.py [-- ids...]
"""
import math
import pathlib
import random
import sys

HERE = pathlib.Path(__file__).resolve().parent
sys.path.insert(0, str(HERE))
import bmesh  # noqa: E402
import bpy  # noqa: E402
from mathutils import Euler, Matrix, Vector  # noqa: E402

import pumpkin_lib as P  # noqa: E402

K = P.K


def transform(ob, mat):
    """Bake a matrix into an object's mesh (object stays at identity)."""
    ob.data.transform(mat)
    ob.data.update()
    return ob


def bake(ob):
    """Apply an object's own transform to its mesh."""
    ob.data.transform(ob.matrix_basis)
    ob.matrix_basis.identity()
    ob.data.update()
    return ob


def chip(name, length, width, thick, color):
    """Flat diamond leaf chip in the XY plane (8 tris), tip along +X."""
    bm = bmesh.new()
    vs = [bm.verts.new(p) for p in ((-length * 0.5, 0, 0), (0, -width / 2, 0), (length * 0.5, 0, 0), (0, width / 2, 0))]
    top = bm.verts.new((0, 0, thick))
    bot = bm.verts.new((0, 0, -thick * 0.6))
    for i in range(4):
        bm.faces.new((vs[i], vs[(i + 1) % 4], top))
        bm.faces.new((vs[(i + 1) % 4], vs[i], bot))
    bmesh.ops.recalc_face_normals(bm, faces=bm.faces)
    return K._finish(bm, name, color, (0, 0, 0), (0, 0, 0), (1, 1, 1), False)


def star_outline(n, r_out, r_in, rot=0.0, notch=None):
    pts = []
    for i in range(2 * n):
        a = math.pi * i / n + rot
        r = r_out if i % 2 == 0 else r_in
        pts.append((math.cos(a) * r, math.sin(a) * r))
    return pts


# ----------------------------------------------------------------------------- corn stalks
def corn_stalks():
    K.reset()
    rnd = random.Random(5)
    parts = []
    n = 7
    tie_z = 0.62
    for i in range(n):
        a = 2 * math.pi * i / n + rnd.uniform(-0.2, 0.2)
        d = Vector((math.cos(a), math.sin(a), 0))
        h = 1.62 + rnd.uniform(-0.08, 0.14)
        pts = [d * 0.17 + Vector((0, 0, 0.0)), d * 0.07 + Vector((0, 0, tie_z)),
               d * 0.16 + Vector((0, 0, 1.15)), d * (0.3 + rnd.uniform(0, 0.08)) + Vector((0, 0, h))]
        col = "straw_dark" if i % 2 else "straw"
        parts.append(K.tube(f"stalk{i}", pts, radius=0.042, radius_end=0.02, color=col, sides=4, smooth=False))
        # leaves: one droopy leaf low, one splayed high on most stalks
        for k, (t0, ln) in enumerate(((0.95 + rnd.uniform(-0.1, 0.1), 0.5), (1.45 + rnd.uniform(-0.1, 0.1), 0.45))):
            if k == 0 and i % 3 == 2:
                continue
            base = d * (0.12 + 0.08 * (t0 - 0.62)) + Vector((0, 0, t0))
            out = (d + Vector((rnd.uniform(-0.3, 0.3), rnd.uniform(-0.3, 0.3), 0))).normalized()
            lp = [base, base + out * ln * 0.35 + Vector((0, 0, 0.12)), base + out * ln * 0.7 + Vector((0, 0, 0.08)),
                  base + out * ln + Vector((0, 0, -0.12 - 0.1 * k))]
            up = out.cross(Vector((0, 0, 1))).cross(out).normalized()
            parts.append(P.ribbon(f"leaf{i}{k}", lp, [0.06, 0.15, 0.11, 0.0], thick=0.012,
                                  color="hay" if (i + k) % 2 else "straw", under_color="straw_dark", up=up,
                                  crease=1.5))
    # rope tie with a knot
    parts.append(K.torus("rope", 0.11, 0.028, color="rope", major_seg=10, minor_seg=4, loc=(0, 0, tie_z)))
    parts.append(K.sphere("knot", 0.04, color="rope", segments=6, rings=4, loc=(0.0, -0.13, tie_z)))
    parts.append(K.tube("rend", [(0.0, -0.14, tie_z - 0.01), (0.03, -0.16, tie_z - 0.12)], radius=0.016,
                        color="rope", sides=4, smooth=False))
    ob = K.merge(parts, "corn_stalks")
    lo = min(v.co.z for v in ob.data.vertices)
    for v in ob.data.vertices:
        v.co.z -= lo
    return P.finish([ob], "props/corn_stalks", "corn_stalks")


# ----------------------------------------------------------------------------- sunflower
def sunflower():
    K.reset()
    parts = []
    head_c = Vector((0.0, -0.06, 2.0))
    parts.append(K.tube("stalk", [(0, 0.04, 0), (0.035, 0.03, 0.7), (0.0, 0.03, 1.4), (-0.01, 0.0, 1.85),
                                  (head_c.x, head_c.y + 0.1, head_c.z - 0.03)],
                        radius=0.065, radius_end=0.045, color="stem_green", sides=6, smooth=False))
    # head frame: local +Z faces the viewer (-Y), tilted 15 degrees down
    head_m = Matrix.Translation(head_c) @ Euler((math.radians(105), 0, 0)).to_matrix().to_4x4()
    head = []
    head.append(P.lathe("disc", [(0.25, -0.02), (0.235, 0.05), (0.15, 0.1)], segs=12, color="sunflower_center",
                        top_color="sunflower_center", bottom_color="stem_green"))
    head.append(K.cylinder("calyx", 0.08, 0.12, color="stem_green", verts=10, radius_top=0.27, loc=(0, 0, -0.08)))
    bake(head[-1])
    for ring, (count, r0, r1, wd, z, col, off) in enumerate(((16, 0.2, 0.47, 0.17, 0.0, "sunflower_yellow", 0.0),
                                                              (12, 0.2, 0.42, 0.18, -0.035, "golden_leaf", 0.5))):
        for i in range(count):
            a = 2 * math.pi * (i + off) / count
            ln = r1 - r0
            c = chip(f"petal{ring}_{i}", ln, wd, 0.025, col)
            K.deform(c, lambda co: Vector((co.x, co.y, co.z + 0.06 * ((co.x / ln + 0.5) ** 2))))
            transform(c, Matrix.Rotation(a, 4, "Z") @ Matrix.Translation(((r0 + r1) / 2, 0, z)))
            head.append(c)
    for h in head:
        transform(h, head_m)
    parts += head
    # big leaves on the stalk
    for k, (zz, ang, ln) in enumerate(((0.55, -40, 0.55), (0.95, 150, 0.6), (1.35, 20, 0.5), (1.7, 200, 0.42))):
        a = math.radians(ang)
        d = Vector((math.cos(a), math.sin(a), 0))
        base = Vector((0.02, 0.03, zz))
        lp = [base, base + d * ln * 0.3 + Vector((0, 0, 0.1)), base + d * ln * 0.72 + Vector((0, 0, 0.06)),
              base + d * ln + Vector((0, 0, -0.1))]
        parts.append(P.ribbon(f"leaf{k}", lp, [0.05, 0.3, 0.24, 0.0], thick=0.02, color="vine",
                              under_color="vine_dark", crease=1.8))
    ob = K.merge(parts, "sunflower")
    return P.finish([ob], "props/sunflower", "sunflower", close=True)


# ----------------------------------------------------------------------------- leaf pile
def leaf_pile():
    K.reset()
    rnd = random.Random(12)
    parts = []
    R, Hh = 0.7, 0.4
    mound = P.lathe("mound", [(R, 0.0), (R * 0.86, 0.15), (R * 0.58, 0.3), (R * 0.2, Hh - 0.02)], segs=12,
                    color="rust_leaf", cap_bottom=False, top_color="maple_orange", smooth=True)

    def lumpy(co):
        a = math.atan2(co.y, co.x)
        f = 1.0 + 0.07 * math.sin(3 * a + 1.3) + 0.05 * math.cos(5 * a) + 0.04 * math.sin(11 * a + co.z * 9)
        return Vector((co.x * f, co.y * f, co.z * (1 + 0.12 * math.sin(2 * a) + 0.08 * math.cos(7 * a))))

    K.deform(mound, lumpy)
    cols = ["maple_red", "maple_orange", "golden_leaf", "rust_leaf"]
    K.paint(mound, "maple_red", where=lambda cc, n: rnd.random() < 0.3)
    K.paint(mound, "maple_orange", where=lambda cc, n: cc.z > 0.25 or rnd.random() < 0.15)
    parts.append(mound)

    def surf_z(r, a):
        t = min(r / R, 1.0)
        return Hh * (1 - t ** 2.2) * (1 + 0.1 * math.sin(2 * a))

    # diamond chips across the mound and spilling onto the ground
    for i in range(24):
        a = rnd.uniform(0, 2 * math.pi)
        r = R * math.sqrt(rnd.random()) * 1.15
        z = surf_z(r, a) if r < R else 0.012
        c = chip(f"chip{i}", rnd.uniform(0.2, 0.28), rnd.uniform(0.12, 0.16), 0.02, cols[i % 4])
        tilt = Euler((rnd.uniform(-0.35, 0.35), rnd.uniform(-0.35, 0.35), rnd.uniform(0, 6.28))).to_matrix().to_4x4()
        transform(c, Matrix.Translation((math.cos(a) * r, math.sin(a) * r, z + 0.01)) @ tilt)
        parts.append(c)
    # a few chunky maple stars on top so the silhouette reads as leaves
    for i, (r, a, col) in enumerate(((0.05, 0.3, "maple_red"), (0.35, 2.2, "golden_leaf"), (0.84, 5.4, "maple_red"))):
        st = K.polygon_prism(f"maple{i}", star_outline(5, 0.16, 0.085, rot=rnd.uniform(0, 1)), 0.025, color=col)
        z = surf_z(r, a) if r < R else 0.0
        tilt = Euler((rnd.uniform(-0.3, 0.3), rnd.uniform(-0.3, 0.3), 0)).to_matrix().to_4x4()
        transform(st, Matrix.Translation((math.cos(a) * r, math.sin(a) * r, z)) @ tilt)
        parts.append(st)
    ob = K.merge(parts, "leaf_pile")
    lo = min(v.co.z for v in ob.data.vertices)
    for v in ob.data.vertices:
        v.co.z -= lo
    return P.finish([ob], "props/leaf_pile", "leaf_pile")


# ----------------------------------------------------------------------------- vine patch
def vine_patch():
    K.reset()
    rnd = random.Random(21)
    parts = []
    vines = [
        [(-0.75, -0.2), (-0.5, -0.35), (-0.2, -0.2), (0.05, -0.3), (0.3, -0.15), (0.55, -0.3), (0.75, -0.15)],
        [(-0.6, 0.45), (-0.4, 0.25), (-0.15, 0.3), (0.05, 0.1), (0.25, 0.25), (0.5, 0.15)],
        [(0.1, 0.7), (0.15, 0.45), (0.05, 0.1), (-0.1, -0.15), (-0.05, -0.5), (0.15, -0.72)],
    ]
    for k, v in enumerate(vines):
        pts = [(x, y, 0.03 + 0.015 * math.sin(i * 1.7 + k)) for i, (x, y) in enumerate(v)]
        parts.append(K.tube(f"vine{k}", pts, radius=0.024, radius_end=0.014, color="vine_dark", sides=4,
                            smooth=True))
    for k, (x, y, ph) in enumerate(((0.78, -0.12, 0.0), (0.52, 0.2, 2.0), (-0.62, 0.5, 4.0))):
        parts.append(P.tendril(f"curl{k}", (x, y, 0.035), turns=1.5, r0=0.06, r1=0.018, rise=0.06, n=7,
                               radius=0.01, color="vine", sides=3, phase=ph))
    leaf = star_outline(5, 0.21, 0.17)
    for k, (x, y, s, ang, col) in enumerate(((-0.45, -0.05, 1.0, 20, "vine"), (0.2, -0.45, 0.9, 120, "hedge"),
                                              (0.4, 0.35, 1.05, 200, "vine"), (-0.25, 0.5, 0.85, 300, "hedge"),
                                              (0.6, -0.05, 0.8, 80, "vine"), (-0.65, -0.45, 0.8, 250, "hedge"))):
        lf = K.polygon_prism(f"leaf{k}", [(px * s, py * s) for px, py in leaf], 0.016, color="vine_dark")
        K.paint(lf, col, where=lambda c, n: n.z > 0.5)
        # cup the leaf (edges lift) and stand it up a little on its stem side
        K.deform(lf, lambda co: Vector((co.x, co.y, co.z + 0.5 * (co.x ** 2 + co.y ** 2))))
        m = Matrix.Translation((x, y, 0.035)) @ Euler((rnd.uniform(-0.25, 0.25), rnd.uniform(-0.25, 0.25),
                                                       math.radians(ang))).to_matrix().to_4x4()
        transform(lf, m)
        parts.append(lf)
    for k, (x, y) in enumerate(((-0.05, -0.05), (0.62, -0.3))):
        fl = K.polygon_prism(f"flower{k}", star_outline(5, 0.075, 0.035), 0.03, color="sunflower_yellow")
        transform(fl, Matrix.Translation((x, y, 0.05)))
        parts.append(fl)
        parts.append(K.cylinder(f"fc{k}", 0.022, 0.04, color="amber", verts=5, loc=(x, y, 0.08)))
    ob = K.merge(parts, "vine_patch")
    lo = min(v.co.z for v in ob.data.vertices)
    for v in ob.data.vertices:
        v.co.z -= lo
    return P.finish([ob], "props/vine_patch", "vine_patch")


BUILDS = {f.__name__: f for f in (corn_stalks, sunflower, leaf_pile, vine_patch)}

if __name__ == "__main__":
    args = sys.argv[sys.argv.index("--") + 1:] if "--" in sys.argv else list(BUILDS)
    for a in args:
        BUILDS[a]()
