"""hay_bale, hay_bale_round, crate, barrel, wheelbarrow (batch02, textured).

blender --background --factory-startup --python art/team/pumpkins_farm/batch02/build_farm_props.py [-- ids...]
"""
import math
import pathlib
import random
import sys

sys.path.insert(0, str(pathlib.Path(__file__).resolve().parent))
import bmesh  # noqa: E402
import bpy  # noqa: E402
from mathutils import Vector  # noqa: E402

import farm_lib as F  # noqa: E402

K = F.K


# ----------------------------------------------------------------------------- hay bale
def bale_body(name, L, W, H, r=0.09, flakes=6, pinch=0.025, corner_pts=1, end_round=0.06, colors=("straw", "hay")):
    """Rectangular bale along X: rounded-rect section, pillowy flakes pinched at the seams, rounded
    ends. Straw strands (texture v) run along X; the cut ends are straw_dark. Base at z=0."""
    sec = F.rounded_rect(W, H, r, corner_pts)
    m = len(sec)
    bm = F.bm_new()
    proj = F.uvl(bm)
    # ring stations: rounded end lip, then seam/mid pairs per flake
    xs, scales, flake_of = [], [], []
    x0 = -L / 2
    fl = L / flakes
    xs.append(x0)
    scales.append(1.0 - end_round / (H / 2))
    flake_of.append(0)
    for k in range(flakes):
        a = x0 + k * fl
        if k == 0:
            xs.append(a + end_round * 0.45)
            scales.append(1.0 - pinch * 0.6)
            flake_of.append(0)
        else:
            xs.append(a)
            scales.append(1.0 - pinch)
            flake_of.append(k)
        xs.append(a + fl * 0.5)
        scales.append(1.0)
        flake_of.append(k)
    xs.append(L / 2 - end_round * 0.45)
    scales.append(1.0 - pinch * 0.6)
    flake_of.append(flakes - 1)
    xs.append(L / 2)
    scales.append(1.0 - end_round / (H / 2))
    flake_of.append(flakes - 1)
    rings = []
    for x, s in zip(xs, scales):
        rings.append([bm.verts.new((x, y * s, H / 2 + z * s)) for y, z in sec])
    per = [0.0]
    for i in range(1, m + 1):
        a, b = sec[i - 1], sec[i % m]
        per.append(per[-1] + math.hypot(b[0] - a[0], b[1] - a[1]))
    side_faces = {}
    for i in range(len(rings) - 1):
        for j in range(m):
            j2 = (j + 1) % m
            f = bm.faces.new((rings[i][j], rings[i][j2], rings[i + 1][j2], rings[i + 1][j]))
            F.set_uv(bm, f, ((per[j], xs[i]), (per[j + 1], xs[i]), (per[j + 1], xs[i + 1]), (per[j], xs[i + 1])))
            side_faces[f] = flake_of[i + 1] if i + 1 < len(flake_of) else flakes - 1
    ends = []
    for k, ring in ((0, rings[0]), (1, rings[-1])):
        x = xs[0] if k == 0 else xs[-1]
        c = bm.verts.new((x + (-1 if k == 0 else 1) * end_round * 0.25, 0, H / 2))
        for j in range(m):
            j2 = (j + 1) % m
            f = bm.faces.new((c, ring[j2], ring[j]) if k == 0 else (c, ring[j], ring[j2]))
            F.set_uv(bm, f, [(v.co.y, v.co.z) for v in f.verts])
            ends.append(f)
    bmesh.ops.recalc_face_normals(bm, faces=bm.faces)
    F.paint(bm, bm.faces, colors[0], "straw")
    F.paint(bm, [f for f, k in side_faces.items() if k % 2 == 1], colors[1], "straw")
    F.paint(bm, ends, "straw_dark", "straw")
    return F.to_object(bm, name, smooth=True, sharp=55)


def rope_loop(name, sec, x, scale, H, out=0.012, radius=0.014, sides=4, twist=0.9):
    twist = F.loop_twist(twist, len(sec), sides)
    """Twisted rope band around a bale section at station x (4 sided, twisting = rope strands)."""
    pts = []
    m = len(sec)
    for j in range(m + 1):
        y, z = sec[j % m]
        d = Vector((0, y, z))
        n = d.normalized()
        p = d * scale + n * out
        pts.append((x, p.y, H / 2 + p.z))
    return F.sweep(name, pts, radius, sides=sides, color="rust", mat="flat", cap_start=False, cap_end=False,
                   twist=twist, smooth=False)


def hay_bale():
    K.reset()
    L, W, H = 1.1, 0.6, 0.55
    parts = [bale_body("bale", L, W, H, r=0.1, flakes=6, pinch=0.03, corner_pts=1)]
    sec = F.rounded_rect(W, H, 0.1, 1)
    fl = L / 6
    for k, x in enumerate((-L / 2 + fl * 1.5, -L / 2 + fl * 4.5)):
        parts.append(rope_loop(f"band{k}", sec, x, 0.985, H, out=0.008, radius=0.02))
    rnd = random.Random(4)
    spots = [((-0.56, -0.17, 0.42), (-1, -0.4, 0.5)), ((-0.57, 0.14, 0.16), (-1, 0.3, -0.2)),
             ((0.56, 0.16, 0.4), (1, 0.4, 0.4)), ((0.1, -0.31, 0.12), (0.2, -1, -0.3)),
             ((-0.25, -0.31, 0.47), (-0.3, -1, 0.5)), ((0.36, 0.31, 0.3), (0.2, 1, 0.1))]
    for i, (b, d) in enumerate(spots):
        parts += F.straw_tuft(f"t{i}", b, d, count=3, length=0.1 + rnd.uniform(0, 0.03), radius=0.013,
                              spread=0.6, seed=i + 3)
    ob = K.merge(parts, "hay_bale")
    return F.finish([ob], "props/hay_bale", "hay_bale")


def hay_bale_round():
    K.reset()
    R, H = 0.6, 1.0
    segs = 20
    prof = [(R - 0.09, 0.0), (R - 0.03, 0.02), (R - 0.004, 0.07), (R + 0.01, 0.28), (R + 0.014, 0.5),
            (R + 0.01, 0.72), (R - 0.004, 0.93), (R - 0.03, 0.98), (R - 0.09, H)]
    body = F.lathe("bale", prof, segs=segs, color="hay", mat="straw", band_colors=[None, "straw", None, None, None,
                                                                                    None, "straw", None],
                   cap_top=False, cap_bottom=False, uv="around", smooth=True, sharp=60)
    parts = [body]
    # top: polar disc with a raised hay spiral (3 turns), painted in two tones along the spiral
    bm = F.bm_new()
    rings_n = 6
    r_top = R - 0.09
    turns = 3.0
    grid = []
    for k in range(1, rings_n + 1):
        rr = r_top * k / rings_n
        row = []
        for s in range(segs):
            a = 2 * math.pi * s / segs
            phase = rr / (r_top / turns) - a / (2 * math.pi)
            z = H + 0.014 * math.sin(2 * math.pi * phase) * math.sin(math.pi * k / rings_n) + 0.02 * (1 - (k / rings_n) ** 2)
            row.append(bm.verts.new((math.cos(a) * rr, math.sin(a) * rr, z)))
        grid.append(row)
    c = bm.verts.new((0, 0, H + 0.022))
    faces = []
    for s in range(segs):
        faces.append(bm.faces.new((c, grid[0][s], grid[0][(s + 1) % segs])))
    for k in range(rings_n - 1):
        for s in range(segs):
            s2 = (s + 1) % segs
            faces.append(bm.faces.new((grid[k][s], grid[k + 1][s], grid[k + 1][s2], grid[k][s2])))
    bmesh.ops.recalc_face_normals(bm, faces=bm.faces)
    bm.normal_update()
    flip = [f for f in bm.faces if f.normal.z < 0]
    if flip:
        bmesh.ops.reverse_faces(bm, faces=flip)
    for f in bm.faces:
        # straw strands wound around the centre: texture v follows the circumference
        angs = [math.atan2(v.co.y, v.co.x) for v in f.verts]
        if max(angs) - min(angs) > math.pi:
            angs = [a + 2 * math.pi if a < 0 else a for a in angs]
        cc = f.calc_center_median()
        rf = max(math.hypot(cc.x, cc.y), 0.05)
        F.set_uv(bm, f, [(math.hypot(v.co.x, v.co.y), a * rf) for v, a in zip(f.verts, angs)])
    F.paint(bm, bm.faces, "straw", "straw")
    dark = []
    for f in faces:
        cc = f.calc_center_median()
        rr = math.hypot(cc.x, cc.y)
        a = math.atan2(cc.y, cc.x) % (2 * math.pi)
        phase = rr / (r_top / turns) - a / (2 * math.pi)
        if (phase % 1.0) < 0.5:
            dark.append(f)
    F.paint(bm, dark, "straw_dark", "straw")
    top = F.to_object(bm, "top", smooth=True, sharp=50)
    parts.append(top)
    # stitch the top disc to the lathe lip: lip ring is at r_top, z = H; already coincident
    # twine bands
    for k, z in enumerate((0.33, 0.67)):
        pts = []
        for s in range(segs + 1):
            a = 2 * math.pi * s / segs
            rr = R + 0.017
            pts.append((math.cos(a) * rr, math.sin(a) * rr, z))
        parts.append(F.sweep(f"twine{k}", pts, 0.014, sides=3, color="rust", mat="flat", cap_start=False,
                             cap_end=False, smooth=False, twist=F.loop_twist(0.8, segs, 3)))
    for i, (a, z, up) in enumerate(((-100, 0.6, 0.3), (-40, 0.2, -0.2), (150, 0.82, 0.4))):
        ar = math.radians(a)
        b = (math.cos(ar) * (R + 0.005), math.sin(ar) * (R + 0.005), z)
        parts += F.straw_tuft(f"t{i}", b, (math.cos(ar), math.sin(ar), up), count=3, length=0.11, radius=0.013,
                              spread=0.7, seed=20 + i)
    ob = K.merge(parts, "hay_bale_round")
    return F.finish([ob], "props/hay_bale_round", "hay_bale_round", ao_radius=1.0)


# ----------------------------------------------------------------------------- crate
def crate():
    K.reset()
    S = 0.7
    parts = []
    post = 0.08
    c = S / 2 - post / 2
    # dark interior core (seen through the slat gaps)
    parts.append(F.slab("core", (S - 0.07, S - 0.07, S - 0.06), loc=(0, 0, (S - 0.06) / 2 + 0.005),
                        color="wood_dark", mat="flat", omit=("-z",)))
    for sx in (-1, 1):
        for sy in (-1, 1):
            parts.append(F.board(f"post{sx}{sy}", (sx * c, sy * c, 0), (sx * c, sy * c, S - 0.035), post, post,
                                 r=0.016, n=1, up=(1, 0, 0), color="wood", mat="wood", caps=(False, True)))
    # side slats: 3 per side, rounded edges, ends tucked into the posts
    slat_h, slat_t = 0.15, 0.03
    zs = (0.11, 0.33, 0.55)
    for k, z in enumerate(zs):
        tone = "plank" if k != 1 else "wood_light"
        for sy in (-1, 1):
            y = sy * (S / 2 - slat_t / 2 - 0.004)
            parts.append(F.board(f"sy{k}{sy}", (-c + 0.02, y, z), (c - 0.02, y, z), slat_t, slat_h, r=0.01, n=0,
                                 up=(0, 0, 1), color=tone, mat="wood", caps=(False, False)))
        for sx in (-1, 1):
            x = sx * (S / 2 - slat_t / 2 - 0.004)
            parts.append(F.board(f"sx{k}{sx}", (x, -c + 0.02, z), (x, c - 0.02, z), slat_t, slat_h, r=0.01, n=0,
                                 up=(0, 0, 1), color=tone, mat="wood", caps=(False, False)))
    # top deck: 4 boards across, flush (walkable)
    bw = (S - 0.02) / 4
    for k in range(4):
        x = -S / 2 + 0.01 + bw * (k + 0.5)
        parts.append(F.board(f"top{k}", (x, -S / 2 + 0.005, S - 0.0175), (x, S / 2 - 0.005, S - 0.0175), bw - 0.012,
                             0.035, r=0.008, n=0, up=(0, 0, 1), color="wood_light" if k % 2 else "plank", mat="wood"))
    # painted apple stamp on the front middle slat, and a little leaf
    apple = K.cylinder("apple", 0.07, 0.008, color="apple_red", mat="flat", verts=14, rot=(90, 0, 0),
                       loc=(0.0, -(S / 2 + 0.002), 0.33))
    K.deform(apple, lambda co: Vector((co.x * (1 + 0.12 * (co.z > 0.03)), co.y,
                                       co.z - 0.018 * math.exp(-(co.x / 0.02) ** 2) * (co.z > 0))))
    parts.append(apple)
    parts.append(K.cylinder("leafst", 0.025, 0.008, color="apple_green", mat="flat", verts=6, rot=(90, 0, 0),
                            scale=(1.6, 1.0, 0.8), loc=(0.04, -(S / 2 + 0.004), 0.41)))
    parts.append(F.board("stalk", (0.0, -(S / 2 + 0.004), 0.385), (0.012, -(S / 2 + 0.004), 0.43), 0.01, 0.006,
                         r=0.002, n=0, up=(0, -1, 0), color="stem_brown", mat="flat"))
    ob = K.merge(parts, "crate")
    return F.finish([ob], "props/crate", "crate")


# ----------------------------------------------------------------------------- barrel
def barrel():
    K.reset()
    segs = 16
    Hb = 0.85
    prof = [(0.255, 0.0), (0.278, 0.08), (0.295, 0.22), (0.302, 0.425), (0.295, 0.63), (0.278, 0.77),
            (0.255, Hb)]
    staves = F.lathe("staves", prof, segs=segs, color="wood", mat="planks", uv="staves", stave=0.32,
                     column_colors=["wood", "plank", "wood", "wood_light"], cap_top=False, cap_bottom=False,
                     smooth=True, flat_cols=True)
    parts = [staves]
    # chime: stave ends roll over into a recessed lid
    rim = F.lathe("rim", [(0.255, Hb), (0.24, Hb + 0.006), (0.225, Hb), (0.222, Hb - 0.03)], segs=segs,
                  color="wood_light", mat="wood", cap_top=False, cap_bottom=False, smooth=True, sharp=50)
    parts.append(rim)
    lid = K.cylinder("lid", 0.222, 0.02, color="plank", mat="planks", verts=segs, loc=(0, 0, Hb - 0.04))
    K.project(lid, "box")
    parts.append(lid)
    parts.append(K.cylinder("bottom", 0.256, 0.01, color="wood_dark", mat="wood", verts=segs, loc=(0, 0, 0.005)))
    # iron hoops: proud bands with a rounded section
    for k, (z, r) in enumerate(((0.1, 0.283), (0.33, 0.302), (0.52, 0.302), (0.75, 0.283))):
        if k in (1, 2):
            r = 0.301
        hw = 0.032
        prof_h = [(r - 0.002, z - hw), (r + 0.012, z), (r - 0.002, z + hw)]
        parts.append(F.lathe(f"hoop{k}", prof_h, segs=segs, color="iron", mat="metal", cap_top=False,
                             cap_bottom=False, smooth=True, sharp=60))
        # rivets on the front
        if k in (0, 3):
            for a in (-100, -80):
                ar = math.radians(a)
                parts.append(K.sphere(f"rv{k}{a}", 0.009, color="silver", mat="flat", segments=5, rings=3,
                                      loc=(math.cos(ar) * (r + 0.012), math.sin(ar) * (r + 0.012), z)))
    # wooden tap (bung) on the front for charm
    parts.append(F.board("tap", (0, -0.29, 0.25), (0, -0.36, 0.25), 0.04, 0.04, r=0.015, n=1, up=(0, 0, 1),
                         color="wood_dark", mat="wood", caps=(False, True)))
    parts.append(F.board("spout", (0, -0.35, 0.25), (0, -0.35, 0.2), 0.026, 0.026, r=0.01, n=1, up=(1, 0, 0),
                         color="wood_dark", mat="wood", caps=(False, True)))
    parts.append(K.cylinder("handle", 0.012, 0.07, color="iron", mat="metal", verts=6, rot=(0, 90, 0),
                            loc=(0, -0.36, 0.275)))
    ob = K.merge(parts, "barrel")
    return F.finish([ob], "props/barrel", "barrel")


# ----------------------------------------------------------------------------- wheelbarrow
def wheelbarrow():
    K.reset()
    parts = []
    # tray: tapered open wooden box with thick rounded rim, planks outside, darker inside
    bm = F.bm_new()

    def rect(wx, ly, z, cy=0.0):
        return [bm.verts.new((x, y + cy, z)) for x, y in ((-wx, -ly), (wx, -ly), (wx, ly), (-wx, ly))]

    t = 0.035
    ob_ = rect(0.2, 0.24, 0.36, cy=0.02)
    ot = rect(0.34, 0.43, 0.64, cy=-0.04)
    rt = rect(0.34 - t * 0.5, 0.43 - t * 0.5, 0.655, cy=-0.04)
    it = rect(0.34 - t, 0.43 - t, 0.64, cy=-0.04)
    ib = rect(0.2 - t, 0.24 - t, 0.36 + t, cy=0.02)
    out_f, rim_f, in_f = [], [], []
    f = bm.faces.new(list(reversed(ob_)))
    out_f.append(f)
    for i in range(4):
        j = (i + 1) % 4
        out_f.append(bm.faces.new((ob_[i], ob_[j], ot[j], ot[i])))
        rim_f.append(bm.faces.new((ot[i], ot[j], rt[j], rt[i])))
        rim_f.append(bm.faces.new((rt[i], rt[j], it[j], it[i])))
        in_f.append(bm.faces.new((it[i], it[j], ib[j], ib[i])))
    in_f.append(bm.faces.new(ib))
    bmesh.ops.recalc_face_normals(bm, faces=bm.faces)
    bm.normal_update()
    K._project_bm(bm, "box")
    F.paint(bm, out_f, "plank", "planks")
    F.paint(bm, in_f, "wood_dark", "planks")
    F.paint(bm, rim_f, "wood_light", "wood")
    tray = F.to_object(bm, "tray", smooth=True, sharp=40)
    # boards run horizontally along the tray sides, two boards per side
    F.project_along(tray, "x", where=lambda c, n: abs(n.y) > abs(n.x) and abs(n.z) < 0.9)
    F.project_along(tray, "y", where=lambda c, n: abs(n.x) >= abs(n.y) and abs(n.z) < 0.9)
    F.scale_uv(tray, 1.8, where=lambda c, n: abs(n.z) < 0.9)
    parts.append(tray)
    # metal corner straps on the tray front
    for sx in (-1, 1):
        parts.append(F.board(f"strap{sx}", (sx * 0.2, -0.22, 0.37), (sx * 0.335, -0.465, 0.64), 0.03, 0.01, r=0.004,
                             n=0, up=(0, -1, 0.4), color="iron", mat="metal"))
    # handles run from the wheel fork, under the tray, out the back with rounded grips
    for sx in (-1, 1):
        x = sx * 0.17
        parts.append(F.sweep(f"h{sx}", [(x * 0.62, -0.56, 0.2), (x * 0.85, -0.3, 0.29), (x, -0.05, 0.34),
                                        (x * 1.22, 0.4, 0.49), (x * 1.38, 0.7, 0.58)], 0.034, sides=6,
                             color="wood", mat="wood", cap_start=False, cap_end=False, uv_scale=1.0))
        parts.append(F.sweep(f"g{sx}", [(x * 1.38, 0.68, 0.574), (x * 1.44, 0.8, 0.614), (x * 1.46, 0.86, 0.632)],
                             [0.042, 0.044, 0.04], sides=6, color="wood_dark", mat="wood", cap_start=True,
                             cap_end=True, cap_offset=0.5))
        parts.append(F.board(f"leg{sx}", (x * 1.15, 0.3, 0.0), (x * 1.15, 0.3, 0.44), 0.06, 0.06, r=0.014, n=1,
                             up=(1, 0, 0), color="wood_dark", mat="wood", caps=(True, False)))
        # fork plates holding the axle
        parts.append(F.board(f"fork{sx}", (sx * 0.075, -0.56, 0.19), (sx * 0.1, -0.42, 0.28), 0.012, 0.05, r=0.004,
                             n=0, up=(0, 0, 1), color="iron", mat="metal"))
    # front wheel: chunky rounded tyre, red hub with spokes-lumps, axle
    tyre = K.torus("tyre", major=0.165, minor=0.05, color="charcoal", mat="flat", major_seg=16, minor_seg=5,
                   rot=(0, 90, 0), loc=(0, -0.56, 0.215))
    parts.append(tyre)
    parts.append(K.cylinder("hub", 0.13, 0.07, color="metal_red", mat="metal", verts=12, rot=(0, 90, 0),
                            loc=(0, -0.56, 0.215), bevel=0.012, segments=1))
    F.auto_smooth(parts[-1], 40)
    parts.append(K.cylinder("cap", 0.035, 0.16, color="silver", mat="metal", verts=8, rot=(0, 90, 0),
                            loc=(0, -0.56, 0.215)))
    # three little pumpkins heaped in the tray
    for k, (x, y, z, w, rz, col, sc) in enumerate(((-0.12, 0.1, 0.42, 0.3, 10, "pumpkin", "stem_brown"),
                                                    (0.13, 0.02, 0.43, 0.28, 50, "pumpkin_light", "stem_green"),
                                                    (-0.01, -0.2, 0.47, 0.26, 80, "pumpkin_dark", "stem_brown"))):
        pb = F.pumpkin_body(f"p{k}", width=w, height=w * 0.72, lobes=6, spl=3, rings=6, groove=0.14, plump=0.5,
                            color=col, fullness=0.7, flatness=0.7, top_dimple=0.025, seed=30 + k, loc=(x, y, z),
                            rot_z=rz)
        bpy.context.view_layer.update()
        tp = F.top_point(pb)
        parts.append(pb)
        parts.append(F.stem(f"ps{k}", tp, height=0.07, radius=0.02, curl=0.03, lean_dir=k * 2.0, color=sc,
                            rings=4, ridges=3, uv_scale=4.0, sink=0.012))
    ob = K.merge(parts, "wheelbarrow")
    F.drop_to_ground(ob)
    return F.finish([ob], "props/wheelbarrow", "wheelbarrow")


if __name__ == "__main__":
    F.run({f.__name__: f for f in (hay_bale, hay_bale_round, crate, barrel, wheelbarrow)})
