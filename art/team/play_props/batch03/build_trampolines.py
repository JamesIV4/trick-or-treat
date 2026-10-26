"""Play props batch03: trampoline_large (backyard, mat at 0.85 m) and trampoline_mini (rebounder, mat at 0.28 m).

    blender --background --factory-startup --python build_trampolines.py -- [trampoline_large] [trampoline_mini]

Reuses yard_street/batch02/yard_common.py (sweep, lathe, rod, box, finish helpers).
The mat prints (pumpkin, target ring, bats, star) are flat decal polygons a few millimetres above the
mat; they get their own AO pass so they don't darken the mat underneath them.
"""
import pathlib
import sys

HERE = pathlib.Path(__file__).resolve().parent
sys.path.insert(0, str(HERE.parents[1] / "yard_street" / "batch02"))
from yard_common import *  # noqa: E402,F401,F403
from yard_common import clean  # noqa: E402

import bmesh  # noqa: E402


# ---------------------------------------------------------------- helpers
def polar(r, deg, z):
    a = math.radians(deg)
    return (math.cos(a) * r, math.sin(a) * r, z)


def decal(name, pts, z, color, mat="flat"):
    """One flat upward-facing polygon at height z (concave outlines are fine; clean() triangulates)."""
    bm = bmesh.new()
    proj, _ = K._uv_layers(bm)
    vs = [bm.verts.new((x, y, z)) for x, y in pts]
    f = bm.faces.new(vs)
    f.normal_update()
    if f.normal.z < 0:
        f.normal_flip()
    for loop in f.loops:
        loop[proj].uv = (loop.vert.co.x, loop.vert.co.y)
    K._paint_bm(bm, color, mat)
    return new_obj(bm, name, False)


def xform2d(pts, scale=1.0, rot=0.0, off=(0.0, 0.0)):
    c, s = math.cos(math.radians(rot)), math.sin(math.radians(rot))
    return [(off[0] + (x * c - y * s) * scale, off[1] + (x * s + y * c) * scale) for x, y in pts]


def mirror_half(right):
    """right: points from top centre (x=0) clockwise down to bottom centre (x=0). Returns the full CCW-ish loop."""
    left = [(-x, y) for x, y in reversed(right[1:-1])]
    return right + left


def pumpkin_outline(R, n=36):
    pts = []
    for i in range(n):
        t = 2 * math.pi * i / n
        d = math.degrees(t)
        k = 1.0 + 0.045 * math.cos(6 * t)              # gentle lobes
        k *= 1.0 - 0.13 * math.exp(-((d - 90) / 16) ** 2)  # stem dimple
        pts.append((1.2 * R * math.cos(t) * k, R * math.sin(t) * k * (0.94 if math.sin(t) < 0 else 1.0)))
    return pts


def bat_outline():
    right = [(0.0, 0.03), (0.022, 0.062), (0.03, 0.028), (0.06, 0.04), (0.12, 0.068), (0.185, 0.05),
             (0.155, 0.005), (0.125, -0.002), (0.105, -0.034), (0.075, -0.014), (0.05, -0.04), (0.028, -0.026),
             (0.0, -0.058)]
    return mirror_half(right)


def star_outline(R, r, n=5, rot=90.0):
    pts = []
    for i in range(2 * n):
        a = math.radians(rot + 180.0 * i / n)
        rr = R if i % 2 == 0 else r
        pts.append((math.cos(a) * rr, math.sin(a) * rr))
    return pts


def spring(name, a, b, coil_r, wire_r, coils=2.5, per=4, color="silver"):
    a, b = Vector(a), Vector(b)
    d = b - a
    L = d.length
    t = d / L
    n1 = t.cross(Vector((0, 0, 1))).normalized()
    n2 = t.cross(n1).normalized()
    hook = min(0.022, L * 0.12)
    pts = [a]
    steps = int(coils * per)
    for k in range(steps + 1):
        s = hook + (L - 2 * hook) * k / steps
        ang = 2 * math.pi * k / per
        pts.append(a + t * s + (n1 * math.cos(ang) + n2 * math.sin(ang)) * coil_r)
    pts.append(b)
    return sweep(name, pts, wire_r, color=color, mat="metal", sides=3, caps=(False, False))


def finish_split(main, prints, rel_path, asset_id, ao_radius=0.6):
    """clean -> AO on the body (without prints) -> AO on prints alone -> parent -> export + sheet."""
    clean(main)
    for p in prints:
        clean(p)
    K.bake_ao([main], samples=32, radius=ao_radius, strength=0.9, ground=True)
    if prints:
        K.bake_ao(prints, samples=8, radius=0.2, strength=0.0, ground=False)
        for p in prints:
            parent_keep(p, main)
    out = K.export([main], rel_path)
    tris = K.tri_count([main])
    sh = K.review_sheet([main], asset_id)
    zmin = min(v.co.z for v in main.data.vertices)
    print(f"ASSET {asset_id} tris={tris} zmin={zmin:.4f} gltf={out} sheet={sh}")
    return tris


# ---------------------------------------------------------------- trampoline_large
def trampoline_large():
    K.reset()
    SEG = 40
    MAT_Z = 0.85
    RM = 1.42          # mat radius
    RF = 1.74          # frame ring radius
    FZ = 0.80          # frame tube centre height
    MATC, BORDER, STITCH = "cat_black", "witch_purple", "bone"
    parts, prints = [], []

    # mat: thin disc, flat-shaded, bands paint the stitched border and the target ring
    prof = [(0.0, MAT_Z - 0.012), (RM - 0.01, MAT_Z - 0.012), (RM, MAT_Z - 0.005), (RM - 0.012, MAT_Z),
            (1.385, MAT_Z), (1.37, MAT_Z), (1.32, MAT_Z), (1.04, MAT_Z), (0.9, MAT_Z), (0.0, MAT_Z)]
    cols = [MATC, BORDER, BORDER, BORDER, STITCH, BORDER, MATC, "pumpkin", MATC]
    stitch = [STITCH if j % 2 == 0 else BORDER for j in range(SEG)]
    mat = lathe("mat", prof, cols, ["denim"] * 9, segments=SEG, smooth=False, cap_bottom=False, cap_top=False,
                band_cols={4: stitch})
    reproject(mat, "box")
    parts.append(mat)

    # frame ring (sits under the pad) and springs from the mat edge to the frame
    parts.append(K.torus("frame", RF, 0.032, color="silver", mat="metal", major_seg=32, minor_seg=5,
                         loc=(0, 0, FZ)))
    for i in range(SEG):
        deg = 360.0 * (i + 0.5) / SEG
        parts.append(spring(f"spring{i}", polar(RM - 0.005, deg, MAT_Z - 0.007), polar(RF - 0.02, deg, FZ + 0.025),
                            0.017, 0.0055, coils=2.0))

    # padded safety cover: puffy ring, orange top panels, purple skirt
    pad = [(1.535, 0.846), (1.528, 0.868), (1.548, 0.892), (1.60, 0.906), (1.72, 0.91), (1.81, 0.902),
           (1.855, 0.882), (1.868, 0.85), (1.868, 0.78), (1.85, 0.758), (1.70, 0.79), (1.535, 0.846)]
    nb = len(pad) - 1
    top_cols = ["pumpkin" if (j // 5) % 2 == 0 else "pumpkin_light" for j in range(SEG)]
    pcols = ["pumpkin"] * 7 + [BORDER] * (nb - 7)
    parts.append(lathe("pad", pad, pcols, "vinyl", segments=SEG, smooth=True, cap_bottom=False, cap_top=False,
                       band_cols={b: top_cols for b in range(1, 7)}, phase=0.0))

    # six curved W-legs: two splayed uprights joined by a ground bar that follows the circle
    rt = 0.03
    for k in range(6):
        c = 60.0 * k + 30.0
        dlt = 11.0
        pts = []
        for side, sgn in ((0, -1), (1, 1)):
            th = c + sgn * dlt
            up = [polar(RF, th, FZ), polar(RF + 0.02, th, 0.45), polar(RF + 0.05, th, 0.16)]
            # bend into the ground bar along r = RF + 0.06
            bend = [polar(RF + 0.058, th - sgn * 1.2, 0.07), polar(RF + 0.06, th - sgn * 3.2, rt + 0.004)]
            seg = up + bend
            pts += seg if side == 0 else list(reversed(seg))
            if side == 0:
                for m in range(1, 4):
                    pts.append(polar(RF + 0.06, th + (2 * dlt - 6.4) * m / 4 + 3.2, rt))
        parts.append(sweep(f"leg{k}", pts, 0.03, color="silver", mat="metal", sides=6, caps=(False, False)))
        for sgn in (-1, 1):  # T-joint sleeves where the legs meet the frame
            p = polar(RF, c + sgn * dlt, FZ - 0.05)
            parts.append(rod(f"sleeve{k}{sgn}", (p[0], p[1], p[2] - 0.06), (p[0], p[1], p[2] + 0.04), 0.04, "iron",
                             sides=6, chamfer=0.0))

    # little ladder on the front-right
    th = -62.0
    lp = []
    for s in (-0.2, 0.2):
        base = Vector(polar(2.2, th, 0.0))
        tan = Vector((-math.sin(math.radians(th)), math.cos(math.radians(th)), 0)) * s
        rail = [base + tan + Vector((0, 0, rt)), Vector(polar(2.06, th, 0.5)) + tan, Vector(polar(1.93, th, 0.78)) + tan,
                Vector(polar(1.885, th, 0.84)) + tan, Vector(polar(1.85, th, 0.80)) + tan]
        lp.append(sweep(f"rail{s}", [tuple(p) for p in rail], 0.022, color="iron", mat="metal", sides=5))
    for i, z in enumerate((0.24, 0.52)):
        r = 2.2 - (2.2 - 1.93) * z / 0.78
        p = polar(r - 0.015, th, z)
        lp.append(box(f"step{i}", (0.1, 0.46, 0.035), "witch_purple", "vinyl", loc=p, rot=(0, 0, th), bevel=0.012,
                      segments=1))
    parts += lp

    body = K.merge(parts, "trampoline_large")

    # prints: pumpkin + face in the centre, faint bats between the ring and the border
    pz = MAT_Z + 0.004
    prints.append(decal("pumpkin", pumpkin_outline(0.54), pz, "pumpkin", "denim"))
    F = 1.08  # face/stem scale relative to the R=0.5 layout
    stem = [(-0.06, 0.44), (0.05, 0.44), (0.075, 0.6), (0.03, 0.63), (-0.035, 0.55)]
    prints.append(decal("stem", xform2d(stem, F), pz + 0.004, "stem_green", "denim"))
    fz = pz + 0.006  # well clear of the pumpkin print so it can't z-fight at grazing angles
    INK = "ink"
    prints.append(decal("eye_l", xform2d([(-0.36, 0.07), (-0.1, 0.07), (-0.22, 0.27)], F), fz, INK))
    prints.append(decal("eye_r", xform2d([(0.1, 0.07), (0.36, 0.07), (0.22, 0.27)], F), fz, INK))
    prints.append(decal("nose", xform2d([(-0.06, -0.04), (0.06, -0.04), (0.0, 0.05)], F), fz, INK))
    grin = [(-0.42, -0.07), (-0.3, -0.12), (-0.2, -0.12), (-0.17, -0.07), (-0.12, -0.13), (0.12, -0.13),
            (0.17, -0.07), (0.2, -0.12), (0.3, -0.12), (0.42, -0.07), (0.3, -0.28), (0.12, -0.34), (0.06, -0.26),
            (-0.06, -0.26), (-0.12, -0.34), (-0.3, -0.28)]
    prints.append(decal("grin", xform2d(grin, F), fz, INK))
    for i in range(6):
        deg = 60.0 * i
        ang = math.radians(deg)
        off = (math.cos(ang) * 1.16, math.sin(ang) * 1.16)
        prints.append(decal(f"bat{i}", xform2d(bat_outline(), 1.25, deg - 90.0, off), pz, "bat_fur", "denim"))
    pr = K.merge(prints, "mat_print")
    return finish_split(body, [pr], "props/trampoline_large", "trampoline_large", ao_radius=0.6)


# ---------------------------------------------------------------- trampoline_mini
def trampoline_mini():
    K.reset()
    SEG = 32
    MAT_Z = 0.28
    RM = 0.40
    MATC = "ink"
    parts, prints = [], []
    prof = [(0.0, MAT_Z - 0.008), (RM - 0.008, MAT_Z - 0.008), (RM, MAT_Z - 0.003), (RM - 0.008, MAT_Z),
            (0.335, MAT_Z), (0.29, MAT_Z), (0.0, MAT_Z)]
    cols = [MATC, MATC, MATC, MATC, "candy_lemon", MATC]
    mat = lathe("mat", prof, cols, "denim", segments=SEG, smooth=False, cap_bottom=False, cap_top=False)
    reproject(mat, "box")
    parts.append(mat)
    # spring zone: short sloped band, painted as alternating steel springs
    sb = [(RM - 0.002, MAT_Z - 0.004), (0.465, MAT_Z - 0.012)]
    springs = ["silver" if j % 2 == 0 else "charcoal" for j in range(SEG * 2)]
    parts.append(lathe("springs", sb, "silver", "metal", segments=SEG * 2, smooth=False, cap_bottom=False,
                       cap_top=False, band_cols={0: springs}))
    # bright purple pad
    pad = [(0.452, 0.27), (0.447, 0.288), (0.462, 0.306), (0.52, 0.314), (0.565, 0.306), (0.585, 0.286),
           (0.588, 0.235), (0.572, 0.222), (0.452, 0.27)]
    pcols = ["candy_violet"] * 5 + ["witch_purple"] * 3
    parts.append(lathe("pad", pad, pcols, "vinyl", segments=SEG, smooth=True, cap_bottom=False, cap_top=False))
    # six stubby splayed legs with rubber feet (the pad skirt hides the frame ring, so it is left out)
    for k in range(6):
        deg = 60.0 * k + 30.0
        # bowed screw-in leg: tucks under the pad, curves outward, ends in a lime rubber foot
        leg = [polar(0.515, deg, 0.255), polar(0.535, deg, 0.19), polar(0.56, deg, 0.12), polar(0.576, deg, 0.05)]
        parts.append(sweep(f"leg{k}", leg, [0.03, 0.029, 0.027, 0.025], color="iron", mat="metal", sides=6,
                           caps=(False, False)))
        b = leg[-1]
        parts.append(lathe(f"foot{k}", [(0.04, 0.0), (0.044, 0.02), (0.034, 0.058), (0.0, 0.064)], "candy_green",
                           "vinyl", segments=6, cap_bottom=True, loc=(b[0], b[1], 0.0)))
    body = K.merge(parts, "trampoline_mini")
    pz = MAT_Z + 0.002
    prints.append(decal("star", star_outline(0.25, 0.11), pz, "candy_lemon", "denim"))
    pr = K.merge(prints, "mat_print")
    return finish_split(body, [pr], "props/trampoline_mini", "trampoline_mini", ao_radius=0.3)


if want("trampoline_large"):
    trampoline_large()
if want("trampoline_mini"):
    trampoline_mini()
