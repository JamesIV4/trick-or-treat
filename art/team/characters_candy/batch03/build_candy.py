"""Candy family v3.1: 5 twist-wrapped candies (round ball with the batch02 spiral stripes restored,
bow-tie fans, vivid row-12 colours, no modelled highlights; new candy_wrapped_orange), new candy_corn
(no modelled highlight), plus the batch02
chocolate bar, swirl lollipop and golden prize bar (unchanged). All float in game, so the origin is at the centre (lollipop: disc centre). Long
axis runs along X; the most readable face (lollipop swirl, wrapper fans) faces front (-Y) / up.

Run: blender --background --factory-startup --python art/team/characters_candy/batch03/build_candy.py [-- <ids>]
"""
import math
import sys
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parent))
import bmesh  # noqa: E402
from mathutils import Matrix, Vector  # noqa: E402

import dk_kit as D  # noqa: E402
from dk_kit import K, V  # noqa: E402

ARGS = sys.argv[sys.argv.index("--") + 1:] if "--" in sys.argv else []
ONLY = {a for a in ARGS if not a.startswith("--")}


def finish(ob, asset_id):
    print("TRIS", asset_id, K.tri_count([ob]))
    if "--lift" in ARGS:
        # REVIEW ONLY: the engine preview puts the origin on the grass, which buries half of a floating
        # candy. --lift exports a temporary copy standing on the ground; always re-export without it.
        zmin = min(v.co.z for v in ob.data.vertices)
        ob.data.transform(Matrix.Translation((0, 0, -zmin + 0.02)))
    K.bake_ao([ob], samples=40, radius=0.12, strength=0.7, ground=False)
    K.export([ob], f"candy/{asset_id}")
    if "--nosheet" not in ARGS:
        K.review_sheet([ob], asset_id)


def faces_from_rings(bm, rings, closed_loop=True):
    """Quads between consecutive rings of equal length; rings of length 1 are poles.
    Returns [(face, i, j)]."""
    out = []
    for i in range(len(rings) - 1):
        A, B = rings[i], rings[i + 1]
        n = max(len(A), len(B))
        for j in range(n):
            j2 = (j + 1) % n
            if len(A) == 1:
                f = bm.faces.new((A[0], B[j2], B[j]))
            elif len(B) == 1:
                f = bm.faces.new((A[j], A[j2], B[0]))
            else:
                f = bm.faces.new((A[j], A[j2], B[j2], B[j]))
            out.append((f, i, j))
    return out


# ================================================================== twist-wrapped candy (v3.1)
def wrapped(asset_id, color, fan, stripe="lolly_white"):
    """Wrapped sweet: a round ball wrapped in bold colour with three spiral stripes (restored from
    batch02: 18 sides, columns 0-1 of every 6, rings twisted along the axis), tight twisted necks and
    two flat bow-tie fans (two rounded lobes, soft centre crease) in a lighter tint. No modelled
    highlights: the game's candy shader supplies gloss and the rim glow. Axis along X, fans in the XZ
    plane, origin at the centre."""
    K.reset()
    SEGS = 18
    FLAT = 0.36              # fan thickness (front-back) relative to its width
    # (r, z, kind) along the local Z axis (turned to X at the end); kind: pole / rim / fan / neck / body
    half_fan = [(0.0, 0.12, "pole"), (0.097, 0.17, "rim"), (0.052, 0.13, "fan"), (0.021, 0.103, "neck")]
    body = [(0.064, 0.086), (0.09, 0.045), (0.096, 0.0)]
    prof = [(r, -z, k) for r, z, k in half_fan]
    prof += [(r, -z, "body") for r, z in body] + [(r, z, "body") for r, z in reversed(body[:-1])]
    prof += [(r, z, k) for r, z, k in reversed(half_fan)]
    LOBE = [(0.0, 0.5), (1 / 3, 1.0), (2 / 3, 0.9), (1.0, 0.05)]     # |u| -> lobe height across the fan

    def lobe_at(au):
        for (u0, l0), (u1, l1) in zip(LOBE[:-1], LOBE[1:]):
            if au <= u1 + 1e-9:
                return l0 + (l1 - l0) * (au - u0) / (u1 - u0)
        return LOBE[-1][1]
    bm = bmesh.new()
    rings, kinds = [], []
    for r, z, kind in prof:
        kinds.append(kind)
        if r == 0.0:
            rings.append([bm.verts.new((0, 0, z))])
            continue
        side = 1 if z > 0 else -1
        ring = []
        for j in range(SEGS):
            a = 2 * math.pi * j / SEGS
            if kind in ("rim", "fan"):
                # flat bow-tie fan: x spaced evenly across the width; two broad rounded lobes either side
                # of a soft centre notch, rounded-off corners
                u = 1 - 4 * (j / SEGS) if j <= SEGS // 2 else -3 + 4 * (j / SEGS)
                yb = 1 if j <= SEGS // 2 else -1
                lobe = lobe_at(abs(u))
                rr = r * (1.0 + (0.12 * max(lobe, 0.0) if kind == "rim" else 0.0))
                x = u * rr
                crease = 0.6 if abs(u) < 0.2 else 1.0                       # pleat fold down the middle
                y = yb * FLAT * rr * math.sqrt(max(1 - u * u, 0.0)) * crease + (yb * 0.004 if abs(u) < 0.99 else 0.0)
                dz = 0.02 * lobe if kind == "rim" else 0.004 * lobe
                ring.append(bm.verts.new((x, y, z + dz * side)))
            else:
                tw = 8.5 * max(-0.1, min(0.1, z))        # batch02 twist: the stripes spiral round the ball
                ring.append(bm.verts.new((math.cos(a + tw) * r, math.sin(a + tw) * r, z)))
        rings.append(ring)
    faces = faces_from_rings(bm, rings)
    fan_kinds = ("pole", "rim", "fan")
    fan_faces = [f for f, i, j in faces if kinds[i] in fan_kinds or kinds[i + 1] in fan_kinds]
    stripes = [f for f, i, j in faces if kinds[i] in ("neck", "body") and kinds[i + 1] in ("neck", "body")
               and (j % 6) in (0, 1)]
    ob = D.mesh(bm, "wrap", color, "flat", "cyl", smooth=True, center=(0, 0, 0),
                extra=[(fan_faces, fan, "flat"), (stripes, stripe, "flat")])
    parts = [ob]
    merged = K.merge(parts, "tmp")
    merged.data.transform(Matrix.Rotation(math.radians(90), 4, "Y"))      # axis Z -> X
    merged.data.transform(Matrix.Scale(0.88, 4))     # keep the batch02 size: ~0.33 m tip to tip
    out = K.merge([merged], asset_id)
    finish(out, asset_id)


# ================================================================== candy corn (v3, new)
def candy_corn():
    """Chunky classic candy corn ~0.3 m tall, standing tip up: wide yellow base, orange middle, white tip.
    Slightly boxy oval cross-section, soft rounded base and tip, no modelled highlight (the candy shader
    adds gloss and rim). Origin at the centre (it floats and the candy shader spins it about the vertical)."""
    K.reset()
    SEGS = 16
    Z0, Z1 = -0.15, 0.15
    HW, HD = 0.115, 0.068         # base half-width (X) and half-depth (Y)
    # (scale, z): rounded bottom, straight-ish tapering sides, rounded tip
    st = [(0.0, Z0), (0.72, Z0 + 0.004), (0.95, Z0 + 0.016), (1.0, Z0 + 0.04), (0.9, -0.06), (0.755, -0.012),
          (0.6, 0.03), (0.45, 0.068), (0.3, 0.104), (0.16, 0.132), (0.0, Z1)]
    bands = [(-1.0, -0.012, "candy_corn_yellow"), (-0.012, 0.068, "candy_orange"), (0.068, 1.0, "candy_corn_white")]

    def sect(k, z):
        out = []
        for j in range(SEGS):
            a = 2 * math.pi * j / SEGS
            c, s = math.cos(a), math.sin(a)
            n = 2.6          # superellipse: a little boxy
            x = HW * k * math.copysign(abs(c) ** (2 / n), c)
            y = HD * (0.35 + 0.65 * k) * math.copysign(abs(s) ** (2 / n), s)
            out.append((x, y, z))
        return out
    bm = bmesh.new()
    rings = []
    for k, z in st:
        rings.append([bm.verts.new((0, 0, z))] if k == 0 else [bm.verts.new(p) for p in sect(k, z)])
    faces = faces_from_rings(bm, rings)
    groups = {}
    for f, i, j in faces:
        zc = (st[i][1] + st[i + 1][1]) / 2
        for lo, hi, col in bands:
            if lo <= zc < hi:
                groups.setdefault(col, []).append(f)
    ob = D.mesh(bm, "corn", "candy_corn_yellow", "flat", "cyl", smooth=True, center=(0, 0, 0),
                extra=[(fs, c, "flat") for c, fs in groups.items()])
    out = K.merge([ob], "candy_corn")
    finish(out, "candy_corn")

# ================================================================== chocolate bar
def top_bevel_box(name, size, loc, color, mat="flat", bevel=0.01):
    """Box with only its top edges bevelled and no bottom face (sits on something)."""
    bm = bmesh.new()
    bmesh.ops.create_cube(bm, size=1.0)
    bmesh.ops.scale(bm, vec=V(size), verts=bm.verts)
    bm.normal_update()
    bottom = [f for f in bm.faces if f.normal.z < -0.9]
    bmesh.ops.delete(bm, geom=bottom, context="FACES_ONLY")
    top_edges = [e for e in bm.edges if all(v.co.z > 0 for v in e.verts) and len(e.link_faces) == 2]
    bmesh.ops.bevel(bm, geom=top_edges, offset=bevel, segments=1, profile=0.5, affect="EDGES", clamp_overlap=True)
    return D.mesh(bm, name, color, mat, "box", smooth=False, loc=loc)


def chocolate_bar():
    K.reset()
    L, W, Th = 0.42, 0.19, 0.06
    x0, xs = -L / 2, 0.0                       # sleeve from x0 to xs, chocolate exposed beyond
    parts = []
    # wrapper sleeve (paper, rounded) with a cream label band
    sleeve = K.box("sleeve", (xs - x0, W + 0.02, Th + 0.02), color="wrapper_red", mat="paper",
                   loc=((x0 + xs) / 2, 0, 0), bevel=0.013, segments=2, smooth=True)
    band = K.box("band", (0.075, W + 0.026, Th + 0.026), color="foil_gold", mat="metal", loc=(x0 + 0.1, 0, 0),
                 bevel=0.004, segments=1)
    parts += [sleeve, band]
    # silver foil collar with a crinkled free edge, and a peeled-back corner flap
    foil = K.box("foil", (0.05, W + 0.012, Th + 0.012), color="foil_silver", mat="metal", loc=(xs + 0.005, 0, 0),
                 bevel=0.006, segments=1)

    def crinkle(c):
        if c.x > 0.015:
            k = math.sin(c.y * 95.0) * 0.004 + math.sin(c.z * 140.0) * 0.003
            return V((c.x + k * 1.5, c.y, c.z + k * 0.5))
        return c
    K.deform(foil, crinkle)
    # peeled corner: a foil triangle hinged on the collar's top edge at the front corner, lifting off the
    # chocolate and curling back over itself (arc with curvature KAP starting at angle PHI0)
    PHI0, KAP, FL, FW = math.radians(14), 26.0, 0.07, 0.075

    def bend(u, v, off=0.0):
        a = PHI0 + KAP * u
        X = (math.sin(a) - math.sin(PHI0)) / KAP
        Z = (math.cos(PHI0) - math.cos(a)) / KAP
        nx, nz = -math.sin(a), math.cos(a)
        return V((X + nx * off, -W / 2 - 0.004 + v, Th / 2 + 0.006 + Z + nz * off))
    bm = bmesh.new()
    rows = []
    for k in range(5):
        u = FL * k / 4
        rows.append([(u, 0.0), (u, FW * (1 - k / 4))] if k < 4 else [(u, 0.0)])
    for off, rev in ((0.0, False), (-0.003, True)):
        vs = [[bm.verts.new(bend(u, v, off)) for u, v in row] for row in rows]
        for k in range(4):
            fv = (vs[k][0], vs[k + 1][0], vs[k + 1][1], vs[k][1]) if k < 3 else (vs[k][0], vs[k + 1][0], vs[k][1])
            bm.faces.new(tuple(reversed(fv)) if rev else fv)
    for fc in bm.faces:
        fc.normal_update()
    flap = D.mesh(bm, "flap", "foil_silver", "metal", "box", smooth=True, loc=(xs + 0.03, 0, 0))
    parts += [foil, flap]
    # chocolate: dark slab with 3 x 2 pillowy squares
    slab = K.box("slab", (L / 2 - xs + 0.01, W - 0.008, Th * 0.5), color="choco_dark", loc=((xs + L / 2) / 2, 0, -Th * 0.22))
    parts.append(slab)
    sq_l = (L / 2 - xs - 0.035) / 3
    for i in range(3):
        for j in range(2):
            parts.append(top_bevel_box(f"sq{i}{j}", (sq_l - 0.01, (W - 0.03) / 2, Th * 0.5),
                                       (xs + 0.032 + sq_l * (i + 0.5), (j - 0.5) * (W - 0.02) / 2, Th * 0.12),
                                       "choco", bevel=0.011))
    ob = K.merge(parts, "candy_chocolate_bar")
    finish(ob, "candy_chocolate_bar")


# ================================================================== swirl lollipop
def lollipop():
    K.reset()
    R, SEG = 0.25, 18
    radii = [0.0, 0.045, 0.09, 0.135, 0.178, 0.212, 0.236]
    cols = ["candy_cherry", "lolly_white", "candy_lemon"]

    def yf(r):
        return 0.04 * math.sqrt(max(1 - (r / R) ** 2, 0.0)) + 0.002

    def tw(r):
        return 9.0 * r

    # one closed lens: front pole -> rings -> rim equator -> rings -> back pole. Every ring is rotated
    # by tw(r), so the face columns themselves spiral; colouring whole columns gives six swirl arms.
    bm = bmesh.new()

    def ring(r, y):
        if r == 0:
            return [bm.verts.new((0, y, 0))]
        return [bm.verts.new((math.cos(2 * math.pi * j / SEG + tw(r)) * r, y, math.sin(2 * math.pi * j / SEG + tw(r)) * r))
                for j in range(SEG)]
    rings = [ring(r, -yf(r)) for r in radii] + [ring(R, 0.0)] + [ring(r, yf(r)) for r in reversed(radii)]
    fa = faces_from_rings(bm, rings)
    bmesh.ops.recalc_face_normals(bm, faces=bm.faces)
    groups = {c: [] for c in cols}
    for f, i, j in fa:
        groups[cols[(j // 3) % 3]].append(f)
    disc = D.mesh(bm, "disc", cols[0], "flat", "sphere", smooth=True, center=(0, 0, 0),
                  extra=[(fs, c, "flat") for c, fs in groups.items()])
    stick = K.cylinder("stick", 0.021, 0.44, color="lolly_white", mat="paper", verts=6, loc=(0, 0, -0.18 - 0.22),
                       smooth=True)
    ob = K.merge([disc, stick], "candy_lollipop")
    finish(ob, "candy_lollipop")


# ================================================================== golden prize bar
def loft(name, stations, sides, n_exp, color, mat, caps=True):
    """Rounded-rectangle (superellipse) rings along X: stations [(x, half_w, half_h)]."""
    bm = bmesh.new()
    rings = []
    for x, w, h in stations:
        ring = []
        for j in range(sides):
            a = 2 * math.pi * j / sides + math.pi / sides
            c, s = math.cos(a), math.sin(a)
            y = w * math.copysign(abs(c) ** (2 / n_exp), c)
            z = h * math.copysign(abs(s) ** (2 / n_exp), s)
            ring.append(bm.verts.new((x, y, z)))
        rings.append(ring)
    faces_from_rings(bm, rings)
    if caps:
        bm.faces.new(list(reversed(rings[0])))
        bm.faces.new(rings[-1])
    bmesh.ops.recalc_face_normals(bm, faces=bm.faces)
    return D.mesh(bm, name, color, mat, "box", smooth=True, center=(0, 0, 0))


def golden_bar():
    K.reset()
    L, W, Th = 0.6, 0.25, 0.125
    hw, hh = W / 2, Th / 2
    ex = L / 2 - 0.05                           # pillow ends here; crimped fins beyond
    st = [(-ex, hw * 0.96, 0.016), (-ex + 0.025, hw * 0.985, hh * 0.62), (-ex + 0.075, hw, hh * 0.92), (-0.11, hw, hh),
          (0.11, hw, hh), (ex - 0.075, hw, hh * 0.92), (ex - 0.025, hw * 0.985, hh * 0.62), (ex, hw * 0.96, 0.016)]
    body = loft("body", st, 14, 3.2, "foil_gold", "metal")
    band = loft("band", [(-0.07, hw + 0.007, hh + 0.007), (0.07, hw + 0.007, hh + 0.007)], 14, 3.2, "glow_candy_gold",
                "flat", caps=False)
    parts = [body, band]
    # serrated crimp fins
    for s in (1, -1):
        teeth = 7
        outline = [(0.0, -hw * 0.95)]
        for t in range(teeth * 2 + 1):
            y = -hw * 0.95 + (2 * hw * 0.95) * t / (teeth * 2)
            outline.append((0.052 if t % 2 == 0 else 0.04, y))
        outline.append((0.0, hw * 0.95))
        if s < 0:
            outline = [(-x, y) for x, y in reversed(outline)]
        fin = K.polygon_prism(f"fin{s}", outline, 0.02, color="foil_gold", mat="metal", loc=(s * (ex - 0.006), 0, -0.01))
        parts.append(fin)
    star = []
    for i in range(10):
        a = math.pi / 2 + i * math.pi / 5
        r = 0.064 if i % 2 == 0 else 0.03
        star.append((math.cos(a) * r, math.sin(a) * r))
    # red medallion with a raised gold star, sitting on the glowing band
    parts.append(K.cylinder("medal", 0.078, 0.016, color="wrapper_red", mat="flat", verts=16, loc=(0, 0, hh + 0.004)))
    parts.append(K.polygon_prism("star", star, 0.012, color="foil_gold", mat="metal", loc=(0, 0, hh + 0.008)))
    ob = K.merge(parts, "candy_golden_bar")
    finish(ob, "candy_golden_bar")


JOBS = {
    "candy_wrapped_pink": lambda: wrapped("candy_wrapped_pink", "candy_magenta", "candy_pink"),
    "candy_wrapped_mint": lambda: wrapped("candy_wrapped_mint", "candy_green", "candy_green_light"),
    "candy_wrapped_grape": lambda: wrapped("candy_wrapped_grape", "candy_violet", "candy_violet_light"),
    "candy_wrapped_lemon": lambda: wrapped("candy_wrapped_lemon", "candy_lemon", "sunflower_yellow", stripe="candy_cherry"),
    "candy_wrapped_orange": lambda: wrapped("candy_wrapped_orange", "candy_orange", "candy_orange_light"),
    "candy_corn": candy_corn,
    "candy_chocolate_bar": chocolate_bar,
    "candy_lollipop": lollipop,
    "candy_golden_bar": golden_bar,
}
for k, job in JOBS.items():
    if not ONLY or k in ONLY:
        job()
