"""corn_stalks, sunflower, leaf_pile, vine_patch (batch02, textured).

blender --background --factory-startup --python art/team/pumpkins_farm/batch02/build_plants.py [-- ids...]
"""
import math
import pathlib
import random
import sys

sys.path.insert(0, str(pathlib.Path(__file__).resolve().parent))
import bmesh  # noqa: E402
import bpy  # noqa: E402
from mathutils import Euler, Matrix, Vector  # noqa: E402

import farm_lib as F  # noqa: E402

K = F.K


def curl_leaf(name, base, direction, length, width, droop=0.25, lift=0.15, twist=0.6, color="straw",
              under="straw_dark", mat="straw", n=6, thick=0.012):
    """Long strap leaf (corn / sunflower stalk leaf) arcing out and drooping, pointed tip, twisting."""
    d = Vector(direction).normalized()
    side = d.cross(Vector((0, 0, 1)))
    if side.length < 1e-4:
        side = Vector((1, 0, 0))
    side.normalize()
    pts, widths, ups = [], [], []
    for i in range(n):
        t = i / (n - 1)
        p = Vector(base) + d * length * t + Vector((0, 0, length * (lift * math.sin(math.pi * t * 0.8) - droop * t * t)))
        pts.append(p)
        widths.append(width * (0.45 + 0.9 * math.sin(math.pi * min(1.0, t * 1.15)) ** 0.7) * (1 - t ** 3) if i < n - 1
                      else 0.0)
        a = twist * t
        up = (Vector((0, 0, 1)) * math.cos(a) + side * math.sin(a)).normalized()
        ups.append(up)
    return F.blade(name, pts, widths, thick=thick, color=color, mat=mat, up=ups, under_color=under, crease=1.6,
                   uv_scale=1.0)


# ----------------------------------------------------------------------------- corn stalks
def corn_stalks():
    K.reset()
    rnd = random.Random(5)
    parts = []
    n = 8
    tie_z = 0.62
    for i in range(n):
        a = 2 * math.pi * i / n + rnd.uniform(-0.2, 0.2)
        d = Vector((math.cos(a), math.sin(a), 0))
        h = 1.62 + rnd.uniform(-0.1, 0.15)
        top_out = 0.3 + rnd.uniform(0, 0.1)
        pts = [d * 0.17, d * 0.075 + Vector((0, 0, tie_z)), d * 0.13 + Vector((0, 0, 1.05)),
               d * (top_out * 0.75) + Vector((0, 0, 1.45)), d * top_out + Vector((0, 0, h))]
        col = "straw_dark" if i % 2 else "straw"
        parts.append(F.sweep(f"stalk{i}", pts, lambda t: 0.04 * (1 - 0.5 * t), sides=5, color=col, mat="straw",
                             cap_start=False, cap_end=True, cap_offset=0.3, uv_scale=1.0, end_color="hay"))
        # nodes: little swollen rings along the stalk
        for k, t0 in enumerate((0.35, 0.62)):
            seg = pts[2] + (pts[3] - pts[2]) * (t0 - 0.35) / 0.27 if k else pts[2]
        # leaves: one droopy low leaf on most stalks, one splayed high leaf
        for k, (zt, ln, wd) in enumerate(((0.95 + rnd.uniform(-0.1, 0.1), 0.62, 0.11),
                                          (1.38 + rnd.uniform(-0.1, 0.1), 0.55, 0.1))):
            if k == 0 and i % 3 == 2:
                continue
            base = d * (0.1 + 0.08 * (zt - 0.62)) + Vector((0, 0, zt))
            out = (d + Vector((rnd.uniform(-0.4, 0.4), rnd.uniform(-0.4, 0.4), 0))).normalized()
            parts.append(curl_leaf(f"leaf{i}{k}", base, out, ln, wd, droop=0.45 + 0.15 * k, lift=0.25,
                                   twist=rnd.uniform(-0.9, 0.9), color="hay" if (i + k) % 2 else "straw"))
        # a little tassel at the very top of every other stalk
        if i % 2 == 0:
            tip = pts[-1]
            for j in range(3):
                aa = a + (j - 1) * 0.6
                dd = Vector((math.cos(aa), math.sin(aa), 0.9)).normalized()
                parts.append(F.sweep(f"tas{i}{j}", [tip, tip + dd * 0.09, tip + dd * 0.15 + Vector((0, 0, -0.05))],
                                     [0.008, 0.006, 0.002], sides=3, color="straw_dark", mat="flat",
                                     cap_start=False, cap_end=False, smooth=False))
    # rope tie: twisted loop with a knot and dangling ends
    loop = [(math.cos(2 * math.pi * k / 14) * 0.11, math.sin(2 * math.pi * k / 14) * 0.11, tie_z) for k in range(15)]
    parts.append(F.sweep("rope", loop, 0.026, sides=4, color="wicker", mat="flat", cap_start=False, cap_end=False,
                         twist=F.loop_twist(0.9, 14, 4), smooth=False))
    parts.append(K.sphere("knot", 0.04, color="wicker", mat="flat", segments=8, rings=5, loc=(0.0, -0.125, tie_z)))
    for s in (-1, 1):
        parts.append(F.sweep(f"rend{s}", [(0.0, -0.14, tie_z - 0.01), (0.03 * s, -0.17, tie_z - 0.1),
                                          (0.045 * s, -0.17, tie_z - 0.18)], 0.017, sides=4, color="wicker", mat="flat",
                             cap_offset=0.4, twist=0.9, smooth=False))
    # two little ornamental corn cobs tucked under the rope
    for s, col in ((-1, "burnt_amber"), (1, "candy_corn_yellow")):
        c0 = Vector((s * 0.09, -0.14, tie_z + 0.02))
        c1 = c0 + Vector((s * 0.05, -0.04, -0.28))
        cob = F.sweep(f"cob{s}", [c0, c0 + (c1 - c0) * 0.5, c1], [0.035, 0.04, 0.026], sides=8, color=col,
                      mat="pumpkin", rib=0.0625, cap_offset=0.5)
        parts.append(cob)
        for j in range(2):
            ang = (j - 0.5) * 1.6 + (0 if s < 0 else 3.14)
            dd = Vector((math.cos(ang) * 0.6, -0.4, 0.7)).normalized()
            parts.append(curl_leaf(f"husk{s}{j}", c0 + Vector((0, 0, 0.01)), dd, 0.16, 0.07, droop=0.05, lift=0.05,
                                   twist=0.3, color="cream", under="bone", mat="straw", n=4, thick=0.006))
    ob = K.merge(parts, "corn_stalks")
    F.drop_to_ground(ob)
    return F.finish([ob], "props/corn_stalks", "corn_stalks")


# ----------------------------------------------------------------------------- sunflower
def heart_leaf(name, size, color="vine", under="vine_dark", n=16):
    """Big heart-shaped leaf in local XY (petiole at -X), slightly cupped and folded on the midrib."""
    bm = F.bm_new()
    c = bm.verts.new((size * 0.05, 0, 0))
    top, bot = [], []
    ring = []
    for i in range(n):
        a = -math.pi + 2 * math.pi * (i + 0.5) / n
        # heart: cardioid-ish, tip at +X, notch at -X
        r = size * (0.62 + 0.38 * math.cos(a)) * (1 - 0.35 * math.exp(-((abs(a) - math.pi) / 0.5) ** 2))
        x, y = math.cos(a) * r + size * 0.35, math.sin(a) * r * 0.85
        z = 0.12 * size * (y / size) ** 2 * 4 + 0.06 * size * abs(y / size) - 0.35 * size * (max(0.0, x) / size) ** 2
        ring.append(bm.verts.new((x, y, z)))
    under_ring = [bm.verts.new(v.co - Vector((0, 0, 0.006))) for v in ring]
    cb = bm.verts.new(c.co - Vector((0, 0, 0.006)))
    tops, unders = [], []
    for i in range(n):
        i2 = (i + 1) % n
        tops.append(bm.faces.new((c, ring[i], ring[i2])))
        unders.append(bm.faces.new((cb, under_ring[i2], under_ring[i])))
        unders.append(bm.faces.new((ring[i], under_ring[i], under_ring[i2], ring[i2])))
    for f in bm.faces:
        F.set_uv(bm, f, [(v.co.x, v.co.y) for v in f.verts])
    bm.normal_update()
    for f in tops:
        if f.normal.z < 0:
            bmesh.ops.reverse_faces(bm, faces=[f])
    for f in unders[::2]:
        if f.normal.z > 0:
            bmesh.ops.reverse_faces(bm, faces=[f])
    F.paint(bm, bm.faces, under, "flat")
    F.paint(bm, tops, color, "flat")
    # midrib stripe (faces touching the tip direction)
    mid = [f for f in tops if abs(f.calc_center_median().y) < size * 0.07]
    F.paint(bm, mid, "sage_leaf", "flat")
    return F.to_object(bm, name, smooth=True, sharp=80)


def sunflower():
    K.reset()
    parts = []
    head_c = Vector((0.0, -0.08, 2.0))
    parts.append(F.sweep("stalk", [(0, 0.04, 0), (0.03, 0.035, 0.6), (0.0, 0.035, 1.2), (-0.02, 0.02, 1.65),
                                   (-0.005, -0.01, 1.88), (head_c.x, head_c.y + 0.11, head_c.z - 0.02)],
                         lambda t: 0.065 * (1 - 0.35 * t), sides=8, color="stem_green", mat="bark", cap_start=False,
                         cap_end=False, uv_scale=2.0))
    # head frame: local +Z faces the viewer (-Y), tilted ~15 degrees down
    head_m = Matrix.Translation(head_c) @ Euler((math.radians(105), 0, 0)).to_matrix().to_4x4()
    head = []
    disc = F.lathe("disc", [(0.27, -0.01), (0.265, 0.03), (0.22, 0.07), (0.14, 0.095), (0.0, 0.105)], segs=18,
                   color="sunflower_center", mat="lattice", cap_top=False, cap_bottom=False, smooth=True, sharp=60)
    K.project(disc, "box")
    F.scale_uv(disc, 5.0)
    K.paint(disc, "choco", where=lambda c, n: math.hypot(c.x, c.y) < 0.12)
    head.append(disc)
    calyx = F.lathe("calyx", [(0.27, -0.012), (0.24, -0.06), (0.14, -0.1), (0.07, -0.13), (0.0, -0.14)], segs=18,
                    color="stem_green", mat="flat", cap_top=False, cap_bottom=False, smooth=True)
    head.append(calyx)
    for ring, (count, r0, ln, wd, z, col, off, tilt) in enumerate(((16, 0.24, 0.27, 0.12, 0.0, "sunflower_yellow", 0.0, 0.18),
                                                                   (16, 0.22, 0.22, 0.11, -0.03, "golden_leaf", 0.5, 0.3))):
        for i in range(count):
            a = 2 * math.pi * (i + off) / count
            d = Vector((math.cos(a), math.sin(a), 0))
            p0 = d * r0 + Vector((0, 0, z))
            p1 = d * (r0 + ln * 0.55) + Vector((0, 0, z + ln * tilt * 0.4))
            p2 = d * (r0 + ln) + Vector((0, 0, z + ln * tilt * 0.6))
            head.append(F.blade(f"petal{ring}_{i}", [p0, p1, p2], [wd * 0.55, wd, 0.0], thick=0.012, color=col,
                                mat="flat", up=(0, 0, 1), under_color="amber" if ring == 0 else None, crease=1.3))
    # sepals behind the petals
    for i in range(10):
        a = 2 * math.pi * (i + 0.25) / 10
        d = Vector((math.cos(a), math.sin(a), 0))
        head.append(F.blade(f"sepal{i}", [d * 0.22 + Vector((0, 0, -0.05)), d * 0.32 + Vector((0, 0, -0.06)),
                                          d * 0.38 + Vector((0, 0, -0.03))], [0.07, 0.08, 0.0], thick=0.01,
                            color="vine", mat="flat", up=(0, 0, 1), under_color="stem_green"))
    for h in head:
        F.transform(h, head_m)
    parts += head
    # big heart leaves on the stalk
    for k, (zz, ang, size) in enumerate(((0.5, -40, 0.42), (0.92, 150, 0.44), (1.3, 30, 0.38), (1.62, 210, 0.3))):
        lf = heart_leaf(f"leaf{k}", size)
        a = math.radians(ang)
        F.transform(lf, Matrix.Translation((0.02 + math.cos(a) * 0.05, 0.03 + math.sin(a) * 0.05, zz)) @
                    Matrix.Rotation(a, 4, "Z") @ Matrix.Rotation(math.radians(-30), 4, "Y"))
        parts.append(lf)
        parts.append(F.sweep(f"petiole{k}", [(0.02, 0.03, zz - 0.06), (0.02 + math.cos(a) * 0.08, 0.03 + math.sin(a) * 0.08,
                                                                           zz + 0.02)], 0.014, sides=5,
                             color="stem_green", mat="flat", cap_start=False, cap_end=False))
    ob = K.merge(parts, "sunflower")
    return F.finish([ob], "props/sunflower", "sunflower", ao_radius=0.6)


# ----------------------------------------------------------------------------- leaf pile
def maple_leaf(name, size, color, n_lobes=5, fold=0.25):
    """Maple leaf chip in local XY facing +Z: 5 pointed lobes, short stem toward -X, folded on the midrib."""
    bm = F.bm_new()
    lobe_a = [0.0, 0.85, -0.85, 1.75, -1.75]
    lobe_r = [1.0, 0.85, 0.85, 0.55, 0.55]
    outline = []
    angs = sorted(range(5), key=lambda k: lobe_a[k])
    seq = []
    for k in angs:
        seq.append((lobe_a[k], lobe_r[k]))
    pts = []
    for idx, (a, r) in enumerate(seq):
        if idx > 0:
            pa = (seq[idx - 1][0] + a) / 2
            pts.append((pa, 0.42))
        pts.append((a - 0.13, r * 0.8))
        pts.append((a, r))
        pts.append((a + 0.13, r * 0.8))
    pts = [(-2.6, 0.3)] + pts + [(2.6, 0.3)]
    c = bm.verts.new((0, 0, 0.0))
    ring = []
    for a, r in pts:
        x, y = math.cos(a) * r * size, math.sin(a) * r * size
        z = fold * size * abs(math.sin(a)) * r
        ring.append(bm.verts.new((x, y, z)))
    stem_v = bm.verts.new((-size * 0.55, 0, 0.0))
    faces = []
    m = len(ring)
    for i in range(m - 1):
        faces.append(bm.faces.new((c, ring[i], ring[i + 1])))
    faces.append(bm.faces.new((c, ring[-1], stem_v)))
    faces.append(bm.faces.new((c, stem_v, ring[0])))
    bm.normal_update()
    for f in faces:
        if f.normal.z < 0:
            bmesh.ops.reverse_faces(bm, faces=[f])
        F.set_uv(bm, f, [(v.co.x, v.co.y) for v in f.verts])
    F.paint(bm, bm.faces, color, "flat")
    return F.to_object(bm, name, smooth=False)


def leaf_pile():
    K.reset()
    rnd = random.Random(12)
    parts = []
    R, Hh = 0.7, 0.4
    mound = F.lathe("mound", [(R, 0.0), (R * 0.95, 0.07), (R * 0.82, 0.18), (R * 0.6, 0.29), (R * 0.32, 0.37),
                              (R * 0.1, Hh - 0.003), (0.0, Hh)], segs=18, color="rust_leaf", mat="leaves", cap_bottom=False, cap_top=False,
                    smooth=True)

    def lumpy(co):
        a = math.atan2(co.y, co.x)
        rr = math.hypot(co.x, co.y) / R
        f = 1.0 + 0.08 * math.sin(3 * a + 1.3) + 0.05 * math.cos(5 * a) + 0.03 * math.sin(11 * a + co.z * 9)
        zf = 1 + (0.08 * math.sin(2 * a + 0.5) + 0.05 * math.cos(5 * a)) * min(1.0, rr * 1.5)
        return Vector((co.x * f, co.y * f, co.z * zf))

    K.deform(mound, lumpy)
    K.project(mound, "sphere")
    F.scale_uv(mound, 1.4)
    cols = ["maple_red", "maple_orange", "golden_leaf", "rust_leaf"]
    K.paint(mound, "maple_orange")
    K.paint(mound, "rust_leaf", where=lambda cc, n: cc.z < 0.06)
    parts.append(mound)

    def surf_z(r, a):
        t = min(r / R, 1.0)
        return Hh * (1 - t ** 2.6) ** 0.8 * (1 + 0.1 * math.sin(2 * a))

    # maple leaf chips across the mound and spilling onto the ground
    count = 34
    for i in range(count):
        a = rnd.uniform(0, 2 * math.pi)
        r = R * math.sqrt(rnd.random()) * (1.25 if i % 3 == 0 else 0.95)
        z = surf_z(r, a) + 0.012 if r < R else 0.012
        lf = maple_leaf(f"chip{i}", rnd.uniform(0.1, 0.15), cols[i % 4], fold=rnd.uniform(0.15, 0.35))
        slope = math.atan(2.0 * Hh * min(r / R, 1.0) / R) if r < R else 0.0
        tilt = Euler((rnd.uniform(-0.25, 0.25), slope + rnd.uniform(-0.2, 0.2), 0)).to_matrix().to_4x4()
        F.transform(lf, Matrix.Translation((math.cos(a) * r, math.sin(a) * r, z)) @ Matrix.Rotation(a + math.pi, 4, "Z")
                    @ tilt @ Matrix.Rotation(rnd.uniform(0, 6.28), 4, "Z"))
        parts.append(lf)
    ob = K.merge(parts, "leaf_pile")
    F.drop_to_ground(ob)
    return F.finish([ob], "props/leaf_pile", "leaf_pile")


# ----------------------------------------------------------------------------- vine patch
def vine_patch():
    K.reset()
    rnd = random.Random(21)
    parts = []
    vines = [
        [(-0.78, -0.2), (-0.5, -0.36), (-0.2, -0.2), (0.05, -0.3), (0.3, -0.14), (0.55, -0.3), (0.76, -0.14)],
        [(-0.62, 0.46), (-0.4, 0.26), (-0.15, 0.32), (0.05, 0.12), (0.25, 0.26), (0.52, 0.16)],
        [(0.1, 0.72), (0.15, 0.46), (0.05, 0.12), (-0.1, -0.15), (-0.05, -0.5), (0.15, -0.72)],
    ]
    for k, v in enumerate(vines):
        pts = []
        for i in range(len(v) - 1):
            for s in range(2):
                t = s / 2
                x = v[i][0] + (v[i + 1][0] - v[i][0]) * t
                y = v[i][1] + (v[i + 1][1] - v[i][1]) * t
                pts.append((x, y, 0.028 + 0.012 * math.sin((i + t) * 1.7 + k)))
        pts.append((v[-1][0], v[-1][1], 0.025))
        parts.append(F.sweep(f"vine{k}", pts, lambda t: 0.024 * (1 - 0.45 * t), sides=4, color="vine_dark",
                             mat="flat", cap_offset=0.5, smooth=True))
    for k, (x, y, ph) in enumerate(((0.8, -0.1, 0.0), (-0.64, 0.5, 4.0), (0.18, -0.76, 3.0))):
        parts.append(F.tendril(f"curl{k}", (x, y, 0.04), turns=1.7, r0=0.06, r1=0.016, rise=0.07, n=11,
                               radius=0.009, sides=4, phase=ph))
    leaves = ((-0.45, -0.05, 0.25, 20), (0.22, -0.46, 0.23, 120), (0.42, 0.36, 0.26, 200), (-0.25, 0.52, 0.22, 300),
              (0.62, -0.06, 0.2, 80), (-0.66, -0.46, 0.2, 250), (-0.05, 0.25, 0.19, 160), (0.35, 0.02, 0.17, 340))
    for k, (x, y, s, ang) in enumerate(leaves):
        lf = F.pumpkin_leaf(f"leaf{k}", size=s, color="vine" if k % 2 == 0 else "vine_dark", lobes=5, n=18, cup=0.1,
                            corrugate=0.09, vein_color=None, sharp=88)
        F.transform(lf, Matrix.Translation((x, y, 0.05 + 0.025 * (k % 3))) @
                    Euler((rnd.uniform(-0.15, 0.15), rnd.uniform(-0.15, 0.15), math.radians(ang))).to_matrix().to_4x4())
        parts.append(lf)
        # leaf stem (petiole) back to the ground vine
        a = math.radians(ang)
        parts.append(F.sweep(f"pet{k}", [(x - math.cos(a) * s * 0.95, y - math.sin(a) * s * 0.95, 0.03),
                                         (x - math.cos(a) * s * 0.45, y - math.sin(a) * s * 0.45, 0.075),
                                         (x - math.cos(a) * s * 0.05, y - math.sin(a) * s * 0.05, 0.065)], 0.011,
                             sides=4, color="vine_dark", mat="flat", cap_start=False, cap_end=False))
    # two yellow pumpkin blossoms and one baby pumpkin
    for k, (x, y) in enumerate(((0.62, -0.32),)):
        petals = []
        for i in range(5):
            a = 2 * math.pi * i / 5
            d = Vector((math.cos(a), math.sin(a), 0))
            petals.append(F.blade(f"fp{k}{i}", [Vector((x, y, 0.07)) + d * 0.01, Vector((x, y, 0.1)) + d * 0.05,
                                                Vector((x, y, 0.12)) + d * 0.075], [0.03, 0.05, 0.0], thick=0.006,
                                  color="sunflower_yellow", mat="flat", up=(0, 0, 1), under_color="amber"))
        parts += petals
        parts.append(K.sphere(f"fc{k}", 0.02, color="amber", mat="flat", segments=6, rings=4, loc=(x, y, 0.09)))
    baby = F.pumpkin_body("baby", width=0.16, height=0.12, lobes=6, spl=3, rings=6, groove=0.14, plump=0.5,
                          color="pumpkin", fullness=0.7, top_dimple=0.015, seed=3, loc=(-0.2, 0.06, 0.0))
    bpy.context.view_layer.update()
    tp = F.top_point(baby)
    parts.append(baby)
    parts.append(F.stem("bs", tp, height=0.05, radius=0.012, curl=0.03, lean_dir=2.0, color="stem_green", rings=4,
                        ridges=3, uv_scale=5.0, sink=0.008))
    ob = K.merge(parts, "vine_patch")
    F.drop_to_ground(ob)
    return F.finish([ob], "props/vine_patch", "vine_patch", ao_radius=0.4)


if __name__ == "__main__":
    F.run({f.__name__: f for f in (corn_stalks, sunflower, leaf_pile, vine_patch)})
