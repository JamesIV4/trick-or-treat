"""candy_variety batch03: ~84 new candy pickups built from parametric families.

Run (Blender 5.1, headless, from the repo root):
  blender --background --factory-startup --python art/team/candy_variety/batch03/build_variety.py -- [ids|tier0..3] [--sheet] [--notile] [--lift]

Every candy floats and spins about the vertical axis in game: origin at the bounding-box centre, the most
readable face toward the front (-Y) and up. Scaled so the longest side equals the spec size. No modelled
specular highlights (the candy shader adds gloss and rim glow). Spec table: specs.py.
"""
import math
import random
import sys
from pathlib import Path

sys.dont_write_bytecode = True
sys.path.insert(0, str(Path(__file__).resolve().parent))
import bmesh  # noqa: E402
from mathutils import Matrix, Vector  # noqa: E402

import cv_kit as C  # noqa: E402
from cv_kit import D, K, TAU, V  # noqa: E402
from specs import SPECS  # noqa: E402

ARGS = sys.argv[sys.argv.index("--") + 1:] if "--" in sys.argv else []
SEL = {a for a in ARGS if not a.startswith("--")}


def clamp(x, a, b):
    return max(a, min(b, x))


# =========================================================================== twist-wrapped family
TWIST_HALF = {
    "ball": [(0.094, 0.0), (0.089, 0.03), (0.074, 0.058), (0.05, 0.082), (0.03, 0.094)],
    "log": [(0.064, 0.0), (0.065, 0.04), (0.064, 0.08), (0.05, 0.106), (0.03, 0.116)],
    "pillow": [(0.082, 0.0), (0.082, 0.035), (0.076, 0.066), (0.054, 0.088), (0.03, 0.098)],
}
FAN_S = {"neck": 0.0, "body": 0.0, "mid": 0.55, "rim": 1.0, "pole": 1.0}


def twist_body(body, fan, pat="plain", acc="lolly_white", shape="ball", fan2=None, SEG=15, stripe=(5, (0, 1)),
               neck=None, crinkle=1.0):
    half = TWIST_HALF[shape]
    zb = half[-1][1]
    fanp = [(0.019, zb + 0.012, "neck"), (0.047, zb + 0.04, "mid"), (0.084, zb + 0.074, "rim"), (0.0, zb + 0.03, "pole")]
    prof = [(r, -z, k) for r, z, k in reversed(fanp)]
    prof += [(r, -z, "body") for r, z in reversed(half)] + [(r, z, "body") for r, z in half[1:]]
    prof += fanp
    kinds = [k for _, _, k in prof]
    rings = []
    for r, z, kind in prof:
        if r == 0.0:
            rings.append([V((0, 0, z))])
            continue
        s = FAN_S[kind]
        side = 1 if z > 0 else -1
        tw = (7.0 * clamp(z, -zb, zb) if pat == "spiral" else 0.0) + 0.35 * s * side
        ring = []
        for j in range(SEG):
            a = TAU * j / SEG + tw
            rr = r
            if shape == "pillow" and kind == "body":
                rr = r / (abs(math.cos(a)) ** 4 + abs(math.sin(a)) ** 4) ** 0.25 * 0.92
            pleat = math.cos((SEG // 3) * (a - tw)) if kind in ("rim", "mid") else 0.0
            rr *= 1 + (0.17 if kind == "rim" else 0.07) * s * pleat * crinkle
            x, y = math.cos(a) * rr, math.sin(a) * rr
            y *= 1 - 0.5 * s
            ring.append(V((x, y, z + 0.009 * s * pleat * side * crinkle)))
        rings.append(ring)
    bm, info, _ = C.loft(rings)
    nb = [i for i, k in enumerate(kinds) if k == "body"]
    b0, b1 = nb[0], nb[-1]

    def is_body(i):
        return kinds[i] in ("body", "neck") and kinds[i + 1] in ("body", "neck")

    def pick(i, j, f):
        zc = f.calc_center_median().z
        if neck and (kinds[i] == "neck") != (kinds[i + 1] == "neck"):
            return neck
        if not is_body(i):
            return (fan2 or fan) if (zc > 0 and fan2) else fan
        if pat == "spiral":
            return acc if j % stripe[0] in stripe[1] else None
        if pat == "bands":
            return acc if (i - b0) % 2 == 1 and b0 <= i < b1 else None
        if pat == "lstripes":
            return acc if j % 3 == 0 else None
        if pat == "split":
            return acc if zc > 0 else None
        if pat == "check":
            return acc if (i + j) % 2 == 0 and b0 <= i < b1 else None
        return None
    g = C.group(info, pick)
    C.rot_bm(bm, "Y", 90)            # axis Z -> X; fans flat in the XZ plane (face the camera)
    return C.mk(bm, "wrap", body, g, smooth=True, project="cyl")


def twist(spec):
    p = spec["p"]
    shape = p.get("shape", "ball")
    pr = p.get("print")
    ob = twist_body(p["body"], p["fan"], p.get("pat", "plain"), p.get("acc", "lolly_white"), shape, p.get("fan2"),
                    SEG=12 if pr else 15)
    parts = [ob]
    tree = C.tree_of([ob])
    if pr:
        # one big print on the front and one on the back (subdivided so it hugs the round body); the
        # sweet spins, so both get seen
        outline, sz = {"bat": (C.BAT_SIMPLE, 0.15), "pumpkin": (C.pumpkin_pts(14), 0.105), "star": (C.star_pts(), 0.12),
                       "ghost": (C.ghost_pts(6, 2), 0.11)}[pr]
        for k, ang in enumerate((22, 158)):          # raised toward the camera, which looks down 30 degrees
            o, d = C.toward_axis_x(0.0, ang)
            dc = C.decal(tree, o, d, outline, sz, p["acc"], up=(0, 0, 1), off=0.004, name=f"pr{k}", sub=1)
            if dc:
                parts.append(dc)
            if pr == "pumpkin" and dc:
                st = C.decal(tree, o + V((0, 0, 0.05)), d, C.ngon(4, 1.0, math.pi / 4, 0.5, 1.0), 0.028, "candy_green",
                             up=(0, 0, 1), off=0.006, name=f"st{k}")
                if st:
                    parts.append(st)
            if pr == "ghost" and dc:
                for s in (1, -1):
                    e = C.decal(tree, o + V((s * 0.015, 0, 0.016)), d, C.ngon(5), 0.015, "ink", up=(0, 0, 1), off=0.007,
                                name=f"ge{k}{s}")
                    if e:
                        parts.append(e)
    if p.get("pat") == "dots":
        r = 0.017 if shape != "log" else 0.015
        pos = [(-0.04, 0), (0.04, 45), (-0.04, 90), (0.04, 135), (-0.04, 180), (0.04, 225), (-0.04, 270), (0.04, 315),
               (0.0, 22), (0.0, 112), (0.0, 202), (0.0, 292)]
        if shape == "log":
            pos += [(-0.085, 45), (0.085, 0), (-0.085, 225), (0.085, 180)]
        for k, (x, ang) in enumerate(pos):
            o, d = C.toward_axis_x(x, ang)
            dc = C.decal(tree, o, d, C.ngon(6), r * 2, p["acc"], up=(1, 0, 0), off=0.004, name=f"dot{k}")
            if dc:
                parts.append(dc)
    return parts


# =========================================================================== round things
def lathe_obj(name, prof, color, n=14, fn=None, twist=None, pick=None, smooth=True, flat_caps=False, phase=0.0):
    rings = C.lathe_rings(prof, n=n, fn=fn, twist=twist, phase=phase)
    bm, info, caps = C.loft(rings)
    g = C.group(info, pick) if pick else {}
    return C.mk(bm, name, color, g, smooth=smooth, flat=caps if flat_caps else (), project="cyl")


def sphere_rings(R, n_lat, z0=None):
    """(r, z) profile of a sphere from the bottom pole to the top pole."""
    return [(R * math.sin(math.pi * k / n_lat), -R * math.cos(math.pi * k / n_lat)) for k in range(n_lat + 1)]


def gumball_trio(spec):
    cols = spec["p"]["cols"]
    pos = [(-0.07, 0.0, -0.045), (0.07, 0.012, -0.045), (0.0, -0.012, 0.072)]
    return [C.sphere(f"g{k}", 0.076, c, loc=p, seg=12, rings=7) for k, (p, c) in enumerate(zip(pos, cols))]


def gumball_rainbow(spec):
    cols = spec["p"]["cols"]
    prof = sphere_rings(0.11, 9)
    ob = lathe_obj("ball", prof, cols[0], n=14, pick=lambda i, j, f: cols[(j // 2) % len(cols)])
    return [C.xf(ob, rot=(0, 18, 0))]


def caramel_cube(spec):
    p = spec["p"]
    cube = K.box("cube", (0.16, 0.16, 0.14), color=p["color"], bevel=0.024, segments=2, smooth=True)
    pts = [(-0.062 + 0.124 * k / 8, 0.052 * (1 if k % 2 else -1), 0.074) for k in range(9)]
    dr = C.sweep("dr", pts, 0.0085, sides=5, color=p["drizzle"])
    return [C.xf(K.merge([cube, dr], "cc"), rot=(20, 0, 30))]


def jawbreaker(spec):
    cols = spec["p"]["cols"]                       # centre -> outer shell
    R = 0.11
    zc = 0.035
    rc = math.sqrt(R * R - zc * zc)
    prof = [(0.0, -R)] + [(R * math.sin(math.radians(t)), -R * math.cos(math.radians(t))) for t in (30, 60, 90)]
    prof += [(rc, zc)]
    radii = [rc * k for k in (0.86, 0.7, 0.54, 0.38, 0.2)]
    prof += [(r, zc) for r in radii] + [(0.0, zc)]
    nshell = 5                                       # rings in the shell part (pole, 30, 60, 90, rim)

    def pick(i, j, f):
        if i < nshell - 1 + 0:
            return cols[-1]
        k = i - (nshell - 1)                         # 0 = band between rim and first inner ring
        return cols[max(0, len(cols) - 1 - k)] if k < len(cols) else cols[0]
    ob = lathe_obj("jaw", prof, cols[-1], n=16, pick=pick)
    return [C.xf(ob, rot=(90, 0, 0))]


def eyeball(spec):
    p = spec["p"]
    R = 0.11
    # latitudes measured from the front pole: pupil, iris, iris rim, then white eyeball (painted bands)
    lats = [180, 155, 130, 105, 80, 58, 44, 38, 17, 0]
    prof = [(R * math.sin(math.radians(t)), R * math.cos(math.radians(t))) for t in lats]
    nb = len(prof) - 1
    cols = {nb - 1: "ink", nb - 2: p["iris"], nb - 3: p["ring"]}
    ob = lathe_obj("eye", prof, "eye_white", n=16, pick=lambda i, j, f: cols.get(i))
    return [C.xf(ob, rot=(72, 0, 0))]


def popcorn_ball(spec):
    rnd = random.Random(7)
    parts = [C.sphere("core", 0.085, "candy_orange", seg=8, rings=5)]
    cols = ["candy_orange", "candy_orange", "candy_orange_light", "candy_corn_white", "candy_orange"]
    for k, d in enumerate(C.fib_dirs(20, seed=3)):
        s = rnd.uniform(0.85, 1.15)
        ob = K.ico(f"k{k}", 0.036 * s, color=cols[k % len(cols)], subdiv=1, smooth=True)
        D.put(ob, d * 0.088, d, roll=rnd.uniform(0, 360), scale=(1.0, 0.8, 0.85))
        parts.append(ob)
    return parts


# =========================================================================== beans, bears, worms
def bean(name, color, L=0.26, R=0.062, n=9, sides=8, arch=0.03):
    pts, radii = [], []
    for k in range(n):
        u = k / (n - 1)
        t = (1 - math.cos(math.pi * u)) / 2
        pts.append((((t - 0.5) * L), 0.0, arch * (1 - (2 * t - 1) ** 2)))
        radii.append(R * max(math.sin(math.pi * u), 0.0) ** 0.55)
    radii[0] = radii[-1] = 0.0
    return C.sweep(name, pts, radii, sides=sides, color=color, warp=lambda a, i: 1 - 0.1 * math.sin(a) ** 2)


def jellybean(spec):
    p = spec["p"]
    ob = bean("bean", p["color"], sides=10, n=11)
    parts = [ob]
    if p.get("speck"):
        tree = C.tree_of([ob])
        rnd = random.Random(5)
        for k in range(18):
            x = rnd.uniform(-0.09, 0.09)
            ang = rnd.uniform(-60, 240)
            o, d = C.toward_axis_x(x, ang)
            o.z += 0.025
            dc = C.decal(tree, o, V((x, 0, 0.025)) - o, C.ngon(5, phase=rnd.uniform(0, 3)), rnd.uniform(0.012, 0.018),
                         p["speck"][k % len(p["speck"])], up=(1, 0, 0), off=0.003, name=f"sp{k}")
            if dc:
                parts.append(dc)
    return parts


def jelly_trio(spec):
    cols = spec["p"]["cols"]
    b1 = C.xf(bean("b1", cols[0], L=0.21, R=0.05, n=9, sides=8), loc=(-0.035, 0.0, -0.03), rot=(0, 28, 0))
    b2 = C.xf(bean("b2", cols[1], L=0.21, R=0.05, n=9, sides=8), loc=(0.04, 0.015, -0.025), rot=(0, -32, 0))
    b3 = C.xf(bean("b3", cols[2], L=0.21, R=0.05, n=9, sides=8), loc=(0.0, -0.035, 0.035), rot=(10, 4, 0))
    return [b1, b2, b3]


def gummy_bear(spec):
    c = spec["p"]["color"]
    parts = [
        C.sphere("body", 0.085, c, loc=(0, 0, -0.045), scale=(1, 0.82, 1.12), seg=8, rings=6),
        C.sphere("head", 0.075, c, loc=(0, -0.005, 0.085), scale=(1.08, 0.88, 0.94), seg=10, rings=6),
        C.sphere("snout", 0.03, c, loc=(0, -0.07, 0.068), scale=(1.15, 0.75, 0.8), seg=6, rings=4),
    ]
    for s in (1, -1):
        parts.append(C.sphere(f"ear{s}", 0.03, c, loc=(s * 0.058, 0, 0.145), scale=(1, 0.6, 1), seg=6, rings=4))
        parts.append(C.sphere(f"arm{s}", 0.033, c, loc=(s * 0.082, -0.02, -0.005), scale=(0.85, 0.85, 1.25),
                              rot=(0, s * 30, 0), seg=6, rings=4))
        parts.append(C.sphere(f"leg{s}", 0.042, c, loc=(s * 0.05, -0.035, -0.135), scale=(0.95, 1.1, 0.85), seg=8, rings=4))
    tree = C.tree_of(parts)
    for s in (1, -1):
        o = V((s * 0.032, -0.4, 0.1))
        dc = C.decal(tree, o, V((0, 1, 0)), C.ngon(6), 0.02, "ink", off=0.004, name=f"eye{s}")
        if dc:
            parts.append(dc)
    dc = C.decal(tree, V((0, -0.4, 0.074)), V((0, 1, 0)), C.ngon(6, sx=1.3), 0.018, "ink", off=0.004, name="nose")
    if dc:
        parts.append(dc)
    return parts


def gummy_worm(spec):
    a, b = spec["p"]["cols"]
    n = 17
    pts, radii = [], []
    R = 0.038
    for k in range(n):
        t = k / (n - 1)
        pts.append(((t - 0.5) * 0.3, 0.02 * math.sin(math.pi * t), 0.045 * math.sin(TAU * t * 1.05)))
        radii.append(R * (1.0 if k % 2 == 0 else 0.87))
    radii[0], radii[1], radii[-2], radii[-1] = 0.0, R * 0.72, R * 0.72, 0.0
    ob = C.sweep("worm", pts, radii, sides=8, color=a, paint=lambda i, j: b if i >= (n - 1) // 2 else None)
    return [ob]


def gummy_bat(spec):
    p = spec["p"]
    ob = C.puffy("bat", C.BAT, 0.3, 0.06, p["color"])
    tree = C.tree_of([ob])
    parts = [ob]
    for s in (1, -1):
        e = C.decal(tree, V((s * 0.032, -0.3, 0.02)), V((0, 1, 0)), C.ngon(8), 0.034, "lolly_white", off=0.003, name=f"e{s}")
        pu = C.decal(tree, V((s * 0.028, -0.3, 0.016)), V((0, 1, 0)), C.ngon(6), 0.017, "ink", off=0.005, name=f"p{s}")
        f = C.decal(tree, V((s * 0.013, -0.3, -0.018)), V((0, 1, 0)), [(-1, 1), (1, 1), (0, -1)], 0.014, "lolly_white",
                    off=0.003, name=f"f{s}")
        parts += [x for x in (e, pu, f) if x]
    return parts


def fish_outline():
    """Swedish fish silhouette (unit coords, nose at -x): a plump oval body with a little dorsal and
    belly fin, and a forked tail."""
    out = []
    for deg in range(32, 329, 9):
        a = math.radians(deg)
        x, y = -0.2 + 0.75 * math.cos(a), 0.46 * math.sin(a)
        y += 0.08 * math.exp(-((deg - 92) / 28.0) ** 2)      # a soft hump of a dorsal fin
        y -= 0.05 * math.exp(-((deg - 262) / 18.0) ** 2)     # belly fin
        out.append((x, y))
    out += [(0.6, -0.17), (0.8, -0.38), (0.97, -0.55), (1.03, -0.45), (0.86, -0.12), (0.82, 0.0), (0.86, 0.12), (1.03, 0.45),
            (0.97, 0.55), (0.8, 0.38), (0.6, 0.17)]
    return out


def swedish_fish(spec):
    p = spec["p"]
    c, acc = p["color"], p["accent"]
    ob = C.puffy("fish", fish_outline(), 0.34, 0.09, c, edge=0.6)
    tree = C.tree_of([ob])
    parts = [ob]
    for side, ray in ((-1, V((0, 1, 0))), (1, V((0, -1, 0)))):
        y0 = 0.3 * side                                          # cast from outside this face
        # an embossed eye with a dark pupil, scale ovals in rows behind the gills, a gill curve
        e = C.blob(f"eye{side}", tree, V((-0.1, y0, 0.022)), ray, 0.018, c, flat=0.5)
        pu = C.decal(tree, V((-0.102, y0, 0.022)), ray, C.ngon(7), 0.016, "ink", off=0.01, name=f"pupil{side}")
        parts += [x for x in (e, pu) if x]
        for i, (x, z) in enumerate([(-0.02, 0.022), (-0.02, -0.02), (0.012, 0.0), (0.012, 0.042), (0.012, -0.04),
                                    (0.044, 0.022), (0.044, -0.02)]):
            sc = C.decal(tree, V((x, y0, z)), ray, C.ngon(6, sx=0.8), 0.022, acc, off=0.003, name=f"sc{side}{i}")
            if sc:
                parts.append(sc)
        for i, z in enumerate((0.05, 0.0, -0.05)):
            g = C.decal(tree, V((-0.06 + 0.006 * abs(i - 1), y0, z * 0.6)), ray, [(-0.25, -1), (0.25, -1), (0.25, 1), (-0.25, 1)],
                        0.03, acc, off=0.003, name=f"gill{side}{i}")
            if g:
                parts.append(g)
    return parts


def gummy_ring(spec):
    a, b = spec["p"]["cols"]
    ob = K.torus("ring", 0.09, 0.043, color=b, major_seg=16, minor_seg=8, smooth=True)
    K.paint(ob, a, where=lambda c, n: c.z > 0.0)
    return [C.xf(ob, rot=(90 - 22, 0, 0))]


# =========================================================================== discs and wrapped discs
def lens(name, R, T, cols, n=16, twist_k=6.0, width=1, radii=None, rim_fn=None, pick=None):
    """Thick lens disc facing front (-Y). cols cycle per 'width' columns. rim_fn(a) radius multiplier."""
    radii = radii or [0.0, R * 0.28, R * 0.52, R * 0.73, R * 0.88, R * 0.97]

    def yf(r):
        return T / 2 * math.sqrt(max(1 - (r / R) ** 2, 0.0)) + 0.003

    def ring(r, z):
        if r < 1e-7:
            return [V((0, 0, z))]
        out = []
        for j in range(n):
            a = TAU * j / n + twist_k * r
            m = rim_fn(TAU * j / n) if rim_fn else 1.0
            out.append(V((math.cos(a) * r * m, math.sin(a) * r * m, z)))
        return out
    rings = [ring(r, -yf(r)) for r in radii] + [ring(R, 0.0)] + [ring(r, yf(r)) for r in reversed(radii)]
    bm, info, _ = C.loft(rings)
    g = C.group(info, pick or (lambda i, j, f: cols[(j // width) % len(cols)]))
    C.rot_bm(bm, "X", 90)
    return C.mk(bm, name, cols[0], g, smooth=True, project="sphere")


def side_fans(color, x0, scale=1.0, n=12):
    """Two pleated cellophane fans along X starting at +-x0 (for wrapped discs)."""
    out = []
    for s in (1, -1):
        prof = [(0.018, 0.0), (0.045, 0.03), (0.078, 0.064), (0.0, 0.024)]
        rings = []
        for k, (r, z) in enumerate(prof):
            if r == 0.0:
                rings.append([V((0, 0, z * scale))])
                continue
            sf = (0.0, 0.55, 1.0)[k]
            ring = []
            for j in range(n):
                a = TAU * j / n
                pleat = math.cos(5 * a) if k else 0.0
                rr = r * (1 + 0.15 * sf * pleat) * scale
                ring.append(V((math.cos(a) * rr, math.sin(a) * rr * (1 - 0.5 * sf), (z + 0.008 * sf * pleat) * scale)))
            rings.append(ring)
        bm, info, _ = C.loft(rings)
        C.rot_bm(bm, "Y", 90 * s)
        bmesh.ops.translate(bm, verts=bm.verts, vec=(s * x0, 0, 0))
        out.append(C.mk(bm, f"fan{s}", color, project="cyl"))
    return out


def peppermint(spec):
    p = spec["p"]
    parts = [lens("disc", 0.12, 0.06, p["cols"], n=16, twist_k=7.0)]
    if p.get("fans"):
        parts += side_fans(p["fans"], 0.112)
    return parts


def butterscotch(spec):
    p = spec["p"]
    R, T = 0.1, 0.03
    prof = [(0.0, -T), (R * 0.88, -T), (R, -T * 0.45), (R, T * 0.45), (R * 0.88, T), (R * 0.62, T * 1.08), (0.0, T * 0.95)]
    disc = lathe_obj("disc", prof, p["color"], n=16)
    C.xf(disc, loc=(0, -0.02, 0), rot=(90, 0, 0))
    wrap = K.box("wrap", (0.22, 0.01, 0.22), color=p["wrap"], bevel=0.003, segments=1)
    K.deform(wrap, lambda c: V((c.x, c.y + 0.012 * math.sin(c.x * 40) * math.sin(c.z * 40), c.z)))
    C.xf(wrap, loc=(0, 0.018, 0), rot=(0, 45, 0))
    tw = []
    for s in (1, -1):        # two twisted corners
        c = K.cylinder(f"tw{s}", 0.014, 0.05, color=p["wrap"], verts=6, radius_top=0.004, smooth=True)
        D.put(c, (s * 0.165, 0.018, 0), (s, 0, 0))
        tw.append(c)
    return [disc, wrap] + tw


def coin(spec):
    p = spec["p"]
    R = 0.12
    prof = [(0.0, -0.012), (R * 0.82, -0.012), (R * 0.88, -0.018), (R, -0.012), (R, 0.012), (R * 0.88, 0.018),
            (R * 0.82, 0.012), (0.0, 0.012)]
    ob = lathe_obj("coin", prof, p["color"], n=20,
                   pick=lambda i, j, f: p["rim"] if 1 <= i <= 5 else None)
    for f in ob.data.polygons:
        f.use_smooth = abs(f.normal.z) < 0.9
    C.xf(ob, rot=(90, 0, 0))
    tree = C.tree_of([ob])
    parts = [ob]
    outline = {"pumpkin": C.pumpkin_pts(), "bat": C.BAT, "star": C.star_pts()}[p["motif"]]
    for side in (-1, 1):
        sz = 0.13 if p["motif"] != "bat" else 0.17
        dc = C.decal(tree, V((0, side * 0.3, 0)), V((0, -side, 0)), outline, sz, p["motif_col"], off=0.003, name=f"m{side}",
                     flatten=True)
        if dc:
            parts.append(dc)
        if p["motif"] == "pumpkin":
            st = C.decal(tree, V((0, side * 0.3, 0.06)), V((0, -side, 0)), C.ngon(4, 1, math.pi / 4, 0.5, 1.0), 0.03,
                         "candy_green", off=0.004, name=f"s{side}", flatten=True)
            if st:
                parts.append(st)
    return parts


# =========================================================================== corn, drops, kisses
def corn(spec):
    cols = spec["p"]["cols"]          # base, middle, tip
    SEGS = 16
    Z0, Z1 = -0.15, 0.15
    HW, HD = 0.115, 0.068
    st = [(0.0, Z0), (0.72, Z0 + 0.004), (0.95, Z0 + 0.016), (1.0, Z0 + 0.04), (0.9, -0.06), (0.755, -0.012),
          (0.6, 0.03), (0.45, 0.068), (0.3, 0.104), (0.16, 0.132), (0.0, Z1)]
    bands = [(-1.0, -0.012, cols[0]), (-0.012, 0.068, cols[1]), (0.068, 1.0, cols[2])]
    rings = []
    for k, z in st:
        if k == 0:
            rings.append([V((0, 0, z))])
            continue
        ring = []
        for j in range(SEGS):
            a = TAU * j / SEGS
            c, s = math.cos(a), math.sin(a)
            x = HW * k * math.copysign(abs(c) ** (2 / 2.6), c)
            y = HD * (0.35 + 0.65 * k) * math.copysign(abs(s) ** (2 / 2.6), s)
            ring.append(V((x, y, z)))
        rings.append(ring)
    bm, info, _ = C.loft(rings)

    def pick(i, j, f):
        zc = (st[i][1] + st[i + 1][1]) / 2
        for lo, hi, col in bands:
            if lo <= zc < hi:
                return col
    return [C.mk(bm, "corn", cols[0], C.group(info, pick), project="cyl")]


GUMDROP = [(0.0, -0.075), (0.085, -0.075), (0.093, -0.064), (0.089, -0.03), (0.073, 0.018), (0.046, 0.054), (0.0, 0.068)]


def gumdrop(spec):
    p = spec["p"]
    ob = lathe_obj("drop", GUMDROP, p["color"], n=20, fn=lambda a, z, k: (1 + 0.05 * math.cos(10 * a) * (0 < k < 6), 0.0))
    tree = C.tree_of([ob])
    parts = [ob]
    rnd = random.Random(11)
    for k, d in enumerate(C.fib_dirs(26, seed=2)):
        if d.z < -0.25:
            continue
        dc = C.decal(tree, d * 0.4, -d, [(-1, -0.8), (1, -0.6), (0.1, 1)], rnd.uniform(0.012, 0.017), "lolly_white",
                     up=(1, 0, 0), off=0.003, name=f"s{k}", spin=rnd.uniform(0, 360))
        if dc:
            parts.append(dc)
    return parts


def gumdrop_pile(spec):
    cols = spec["p"]["cols"]
    f = lambda a, z, k: (1 + 0.05 * math.cos(6 * a) * (0 < k < 6), 0.0)  # noqa: E731
    a = C.xf(lathe_obj("d1", GUMDROP, cols[0], n=12, fn=f), loc=(-0.075, 0.0, -0.035), rot=(0, 0, 10))
    b = C.xf(lathe_obj("d2", GUMDROP, cols[1], n=12, fn=f), loc=(0.075, 0.01, -0.035), rot=(0, 0, -15))
    c = C.xf(lathe_obj("d3", GUMDROP, cols[2], n=12, fn=f), loc=(0.0, -0.02, 0.06), rot=(-12, 8, 0), scale=(0.95, 0.95, 0.95))
    return [a, b, c]


def kiss(spec):
    p = spec["p"]
    prof = [(0.0, -0.075), (0.088, -0.075), (0.095, -0.066), (0.089, -0.036), (0.066, 0.0), (0.04, 0.034), (0.019, 0.066),
            (0.008, 0.084), (0.0, 0.088)]
    rnd = random.Random(3)
    ob = lathe_obj("kiss", prof, p["color"], n=16,
                   fn=lambda a, z, k: (1 + (0.035 * math.sin(7 * a + k * 1.3) if 1 < k < 7 else 0.0), 0.0))
    pts = [(0.0, 0.0, 0.08), (0.005, 0.0, 0.11), (0.02, 0.0, 0.14), (0.045, 0.0, 0.16), (0.07, 0.0, 0.165)]
    plume = C.sweep("plume", pts, [0.012] * 5, sides=4, color=p["plume"],
                    warp=lambda a, i: 0.25 if abs(math.sin(a)) > 0.7 else 1.0, up=(0, 1, 0))
    return [ob, plume]


# =========================================================================== licorice, belts, buttons
def licorice(spec):
    p = spec["p"]
    n, L, R = 15, 0.32, 0.042
    rings = []
    for k in range(n):
        t = k / (n - 1)
        z = (t - 0.5) * L
        bend = -0.035 * (1 - (2 * t - 1) ** 2)
        ring = []
        for j in range(12):
            a = TAU * j / 12 + k * math.radians(24)
            r = R * (1.0 if j % 2 == 0 else 0.74)
            ring.append(V((math.cos(a) * r + bend, math.sin(a) * r, z)))
        rings.append(ring)
    bm, info, caps = C.loft(rings)
    cols = p.get("bands")
    g = C.group(info, (lambda i, j, f: cols[(i * len(cols)) // (n - 1)]) if cols else (lambda i, j, f: None))
    if p.get("cap"):
        g.setdefault(p["cap"], []).extend(caps)
    C.rot_bm(bm, "Y", 90)
    return [C.mk(bm, "lic", p["color"], g, flat=caps, project="cyl")]


def licorice_wheel(spec):
    p = spec["p"]
    pts = []
    N = 30
    for k in range(N):
        th = TAU * 2.2 * k / (N - 1)
        r = 0.03 + 0.042 * th / TAU
        pts.append((r * math.cos(th), 0.0, r * math.sin(th)))
    ob = C.sweep("wheel", pts, 0.0195, sides=6, color=p["color"])
    bead = C.sphere("bead", 0.04, p["bead"], loc=(0, -0.008, 0), seg=8, rings=5)
    return [ob, bead]


def ribbon(name, L, W, T, cols, nx=14, amp=0.03):
    bm = bmesh.new()
    ns = len(cols)
    F, B = [], []
    for k in range(nx):
        t = k / (nx - 1)
        x = (t - 0.5) * L
        y = amp * math.sin(TAU * t)
        z = 0.02 * math.sin(math.pi * t) - 0.01
        dy = amp * TAU / L * math.cos(TAU * t)
        nrm = V((-dy, 1.0, 0.0)).normalized()
        zs = [(-0.5 + s / ns) * W for s in range(ns + 1)]
        F.append([bm.verts.new(V((x, y, z + zz)) - nrm * T / 2) for zz in zs])
        B.append([bm.verts.new(V((x, y, z + zz)) + nrm * T / 2) for zz in zs])
    groups = {}
    for k in range(nx - 1):
        for s in range(ns):
            f1 = bm.faces.new((F[k][s], F[k + 1][s], F[k + 1][s + 1], F[k][s + 1]))
            f2 = bm.faces.new((B[k][s], B[k][s + 1], B[k + 1][s + 1], B[k + 1][s]))
            groups.setdefault(cols[s], []).extend([f1, f2])
        groups.setdefault(cols[0], []).append(bm.faces.new((F[k][0], B[k][0], B[k + 1][0], F[k + 1][0])))
        groups.setdefault(cols[-1], []).append(bm.faces.new((F[k][ns], F[k + 1][ns], B[k + 1][ns], B[k][ns])))
    for k in (0, nx - 1):
        for s in range(ns):
            groups.setdefault(cols[s], []).append(bm.faces.new((F[k][s], F[k][s + 1], B[k][s + 1], B[k][s])))
    bmesh.ops.recalc_face_normals(bm, faces=bm.faces)
    return C.mk(bm, name, cols[0], groups, smooth=False, project="box")


def sour_belt(spec):
    return [ribbon("belt", 0.32, 0.085, 0.014, spec["p"]["cols"], amp=0.032)]


def candy_buttons(spec):
    cols = spec["p"]["cols"]
    strip = K.box("strip", (0.3, 0.012, 0.125), color="lolly_white", bevel=0.004, segments=1)
    parts = [strip]
    for r, z in enumerate((0.037, 0.0, -0.037)):
        for c, x in enumerate((-0.111, -0.037, 0.037, 0.111)):
            parts.append(C.sphere(f"b{r}{c}", 0.0195, cols[r], loc=(x, 0, z), scale=(1, 0.95, 1), seg=6, rings=4))
    ob = K.merge(parts, "buttons")
    K.deform(ob, lambda v: V((v.x, v.y + 0.045 * (v.x / 0.15) ** 2, v.z)))
    return [C.xf(ob, rot=(-12, 0, 0))]


def octa(name, length, width, color):
    bm = bmesh.new()
    t0 = bm.verts.new((0, 0, -length / 2))
    t1 = bm.verts.new((0, 0, length / 2))
    ring = [bm.verts.new((math.cos(a) * width, math.sin(a) * width, 0.0)) for a in (0, math.pi / 2, math.pi, 1.5 * math.pi)]
    for j in range(4):
        bm.faces.new((ring[j], ring[(j + 1) % 4], t1))
        bm.faces.new((ring[(j + 1) % 4], ring[j], t0))
    return C.mk(bm, name, color, smooth=False, project="box")


def rock_candy(spec):
    cols = spec["p"]["cols"]
    rnd = random.Random(21)
    parts = [C.stick("stick", 0.36, r=0.013, color="wood_light", top=(0, 0, 0.18))]
    parts.append(C.sphere("core", 0.05, cols[1], loc=(0, 0, -0.05), scale=(1, 1, 2.0), seg=8, rings=5))
    for k in range(40):
        z = -0.15 + 0.2 * (k + rnd.uniform(0, 1)) / 40
        a = k * 2.39996 + rnd.uniform(-0.3, 0.3)
        radial = V((math.cos(a), math.sin(a), 0))
        ax = (radial + V((0, 0, rnd.uniform(-0.3, 0.5)))).normalized()
        o = octa(f"c{k}", rnd.uniform(0.07, 0.1), rnd.uniform(0.024, 0.034), cols[k % len(cols)])
        D.put(o, radial * rnd.uniform(0.045, 0.06) + V((0, 0, z)), ax, roll=rnd.uniform(0, 90))
        parts.append(o)
    return parts


# =========================================================================== marshmallow, pumpkins, apples
def marsh_ghost(spec):
    p = spec["p"]
    prof = [(0.0, -0.112), (0.085, -0.11), (0.1, -0.09), (0.097, -0.045), (0.088, 0.01), (0.077, 0.055), (0.057, 0.093),
            (0.03, 0.116), (0.0, 0.122)]
    ob = lathe_obj("ghost", prof, p["color"], n=16,
                   fn=lambda a, z, k: (1 + (0.07 * math.cos(5 * a) if k in (1, 2) else 0.0),
                                       0.012 * math.cos(5 * a) if k == 1 else 0.0))
    arms = [C.sphere(f"arm{s}", 0.03, p["color"], loc=(s * 0.095, -0.01, -0.01), scale=(1.2, 0.8, 0.85),
                     rot=(0, s * -25, 0), seg=6, rings=4) for s in (1, -1)]
    tree = C.tree_of([ob])
    parts = [ob] + arms
    for s in (1, -1):
        e = C.decal(tree, V((s * 0.032, -0.4, 0.045)), V((0, 1, 0)), C.ngon(8, sy=1.35), 0.026, "ink", off=0.003, name=f"e{s}")
        ch = C.decal(tree, V((s * 0.058, -0.4, 0.014)), V((0, 1, 0)), C.ngon(8, sy=0.7), 0.026, "candy_pink", off=0.003,
                     name=f"c{s}")
        parts += [x for x in (e, ch) if x]
    m = C.decal(tree, V((0, -0.4, 0.012)), V((0, 1, 0)), C.ngon(8, sy=1.2), 0.02, "ink", off=0.003, name="mouth")
    if m:
        parts.append(m)
    return parts


PUMPKIN = [(0.0, -0.072), (0.06, -0.074), (0.1, -0.056), (0.118, -0.016), (0.115, 0.025), (0.092, 0.058), (0.045, 0.074),
           (0.0, 0.064)]


def pumpkin_fn(nrib):
    return lambda a, z, k: (1 - 0.075 * (math.cos(nrib * a) + 1) / 2 * (0 < k < len(PUMPKIN) - 1), 0.0)


def mellow_pumpkin(spec):
    p = spec["p"]
    ob = lathe_obj("pump", PUMPKIN, p["color"], n=18, fn=pumpkin_fn(6), phase=math.pi / 18)
    stem = K.cylinder("stem", 0.02, 0.06, color=p["stem"], verts=6, radius_top=0.014, smooth=True)
    D.put(stem, (0.004, 0, 0.088), (0.15, 0, 1))
    vine = C.sweep("vine", [(0.01, 0, 0.075), (0.04, -0.01, 0.09), (0.06, -0.03, 0.085), (0.055, -0.045, 0.07)], 0.007,
                   sides=5, color=p["stem"])
    leaf = C.puffy("leaf", C.ngon(8, sx=1.0, sy=0.55), 0.06, 0.01, p["stem"])
    C.xf(leaf, loc=(-0.035, 0.0, 0.078), rot=(10, 0, 25))
    return [ob, stem, vine, leaf]


APPLE = [(0.0, -0.085), (0.04, -0.088), (0.08, -0.066), (0.095, -0.02), (0.093, 0.03), (0.076, 0.066), (0.036, 0.081),
         (0.0, 0.069)]


def candy_apple_mini(spec):
    p = spec["p"]
    ob = lathe_obj("apple", APPLE, p["color"], n=14)
    foot = lathe_obj("foot", [(0.0, -0.092), (0.112, -0.092), (0.114, -0.084), (0.08, -0.074)], p["color"], n=14)
    st = C.stick("stick", 0.15, r=0.011, color="wood_light", top=(0, 0, 0.055), direction=(0.05, 0, 1))
    leaf = C.puffy("leaf", C.ngon(8, sx=1.0, sy=0.5), 0.06, 0.01, "candy_green")
    C.xf(leaf, loc=(0.03, -0.005, 0.085), rot=(0, -30, 0))
    return [ob, foot, st, leaf]


def cakepop(spec):
    p = spec["p"]
    ball = C.sphere("ball", 0.068, p["color"], seg=10, rings=7)
    st = C.stick("stick", 0.1, r=0.012, color="lolly_white", top=(0, 0, -0.05))
    parts = [ball, st]
    rnd = random.Random(4)
    if p.get("sprinkles"):
        sc = p["sprinkles"]
        for k, d in enumerate(C.fib_dirs(22, seed=9, jitter=0.3)):
            if d.z < -0.35:
                continue
            b = K.box(f"s{k}", (0.022, 0.0065, 0.0065), color=sc[k % len(sc)])
            D.put(b, d * 0.069, d, roll=rnd.uniform(0, 180))
            parts.append(b)
    if p.get("drizzle"):
        pts = []
        N = 30
        for k in range(N):
            th = TAU * k / N
            lat = 0.45 + 0.32 * math.sin(5 * th)
            pts.append((math.cos(th) * math.cos(lat) * 0.07, math.sin(th) * math.cos(lat) * 0.07, math.sin(lat) * 0.07))
        parts.append(C.sweep("dr", pts, 0.0075, sides=5, color=p["drizzle"], closed=True))
        parts.append(lathe_obj("cap", [(0.0, 0.052), (0.035, 0.058), (0.045, 0.05)], p["drizzle"], n=10))
    return parts


def bonbon(spec):
    p = spec["p"]
    cup = lathe_obj("cup", [(0.0, -0.06), (0.064, -0.06), (0.09, 0.006), (0.094, 0.012)], p["cup"], n=20,
                    fn=lambda a, z, k: (1 + 0.06 * math.cos(10 * a) * (k > 0), 0.0))
    dome = lathe_obj("dome", [(0.088, 0.0), (0.09, 0.026), (0.079, 0.052), (0.05, 0.072), (0.0, 0.08)], p["choc"], n=20)
    parts = [cup, dome]
    tree = C.tree_of([dome])
    pts = []
    for k in range(16):
        x = -0.06 + 0.12 * k / 15
        y = 0.035 * (1 if k % 2 else -1) * (1 - abs(x) / 0.1)
        loc, nrm = C.surf(tree, (x, y, 0.3), (0, 0, -1))
        if loc is not None:
            pts.append(loc + nrm * 0.004)
    parts.append(C.sweep("dr", pts, 0.0065, sides=5, color=p["drizzle"]))
    parts.append(C.sphere("top", 0.017, p["drizzle"], loc=(0, 0, 0.088), seg=8, rings=5))
    return [C.xf(K.merge(parts, "bb"), rot=(-15, 0, 0))]


# =========================================================================== tubes and rolls
def fizz_roll(spec):
    p = spec["p"]
    wrap = lathe_obj("wrap", [(0.052, -0.15), (0.052, -0.11), (0.052, -0.06), (0.052, -0.02), (0.052, 0.03)], "lolly_white",
                     n=12, pick=lambda i, j, f: p["band"] if i in (1, 3) else None)
    fan_r, fan_l = side_fans("lolly_white", 0.0, scale=0.9, n=10)
    C.bpy.data.objects.remove(fan_r, do_unlink=True)
    discs = []
    for k, c in enumerate(p["discs"]):
        h = 0.0105
        r = 0.048
        d = lathe_obj(f"d{k}", [(0.0, -h), (r * 0.82, -h), (r, 0.0), (r * 0.82, h), (0.0, h)], c, n=10)
        discs.append(C.xf(d, loc=(0, 0, 0.042 + k * 0.023)))
    roll = K.merge([wrap] + discs, "roll")
    C.xf(roll, rot=(0, 90, 0))
    C.xf(fan_l, loc=(-0.15, 0, 0))
    return [roll, fan_l]


def fizz_tube(spec):
    p = spec["p"]
    n = 14
    pts = [((k / (n - 1) - 0.5) * 0.33, 0.0, 0.0) for k in range(n)]
    radii = [0.034] * n
    radii[0] = radii[-1] = 0.04

    def warp(a, i):
        if i in (0, n - 1):
            return max(0.12, abs(math.cos(a)))
        if i in (1, n - 2):
            return max(0.6, abs(math.cos(a)))
        return 1.0
    ob = C.tube("tube", pts, radii, sides=8, color=p["cols"][0], warp=warp, up=(0, 0, 1), twist=0.55,
                pick=lambda i, j, f: p["cols"][2] if i in (0, n - 2) else (p["cols"][1] if j % 4 < 2 else None))
    return [C.xf(ob, rot=(0, -28, 0))]


# =========================================================================== hard candies
def puffy_candy(spec):
    p = spec["p"]
    outline = {"star": C.star_pts(5, 1.0, 0.5), "heart": C.heart_pts(), "bat": C.BAT}[p["shape"]]
    ob = C.puffy("hc", outline, 0.27, p.get("thick", 0.075), p["color"], edge=0.45)
    parts = [ob]
    if p.get("inner"):
        tree = C.tree_of([ob])
        for side in (-1, 1):
            dc = C.decal(tree, V((0, side * 0.3, 0.0)), V((0, -side, 0)), outline, 0.15, p["inner"], off=0.003,
                         name=f"in{side}", flatten=True)
            if dc:
                parts.append(dc)
    return [C.xf(K.merge(parts, "hc2"), rot=(-12, 0, 0))]


def strawberry(spec):
    p = spec["p"]
    prof = [(0.0, -0.1), (0.04, -0.088), (0.074, -0.05), (0.09, -0.003), (0.086, 0.04), (0.062, 0.07), (0.0, 0.078)]
    ob = lathe_obj("berry", prof, p["color"], n=14)
    tree = C.tree_of([ob])
    parts = [ob]
    for k, d in enumerate(C.fib_dirs(30, seed=1)):
        if d.z > 0.55:
            continue
        dc = C.decal(tree, d * 0.4, -d, C.ngon(3), 0.013, "candy_lemon", up=(0, 0, 1), off=0.003, name=f"s{k}")
        if dc:
            parts.append(dc)
    crown = C.puffy("crown", C.star_pts(6, 1.0, 0.45), 0.14, 0.014, "candy_green")
    C.xf(crown, loc=(0, 0, 0.072), rot=(90, 0, 0))
    K.deform(crown, lambda v: V((v.x, v.y, v.z - 0.25 * (v.x * v.x + v.z * v.z) ** 0.5 * 0)))
    stem = K.cylinder("stem", 0.01, 0.04, color="candy_green", verts=6, smooth=True, loc=(0, 0, 0.09))
    return [ob, crown, stem]


# =========================================================================== small composites
def ring_pop(spec):
    p = spec["p"]
    band = K.torus("band", 0.058, 0.016, color=p["band"], major_seg=14, minor_seg=6, smooth=True)
    C.xf(band, loc=(0, 0, -0.075), rot=(90, 0, 0))
    base = K.cylinder("base", 0.045, 0.03, color=p["band"], verts=12, smooth=False, loc=(0, 0, -0.005), bevel=0.005)
    gem = lathe_obj("gem", [(0.0, 0.006), (0.064, 0.042), (0.066, 0.052), (0.044, 0.085), (0.0, 0.09)], p["gem"], n=8,
                    smooth=False)
    return [band, base, gem]


def cotton_candy(spec):
    p = spec["p"]
    cone = lathe_obj("cone", [(0.0, -0.17), (0.016, -0.13), (0.03, -0.075), (0.045, -0.02)], "lolly_white", n=10,
                     twist=lambda z: z * 18, pick=lambda i, j, f: p["stripe"] if j % 5 in (0, 1) else None, flat_caps=True)
    cols = p["fluff"]
    spots = [((0, 0, 0.03), 0.075), ((-0.055, -0.01, 0.06), 0.06), ((0.055, 0.0, 0.065), 0.06), ((0.0, -0.035, 0.1), 0.06),
             ((0.0, 0.035, 0.1), 0.058), ((0.0, 0.0, 0.14), 0.05)]
    parts = [cone]
    for k, (loc, r) in enumerate(spots):
        parts.append(C.sphere(f"f{k}", r, cols[k % len(cols)], loc=loc, seg=8, rings=5))
    return parts


# =========================================================================== tier 1 (chocolate value)
def loft_bar(name, stations, sides, n_exp, color, caps=True, pick=None):
    rings = []
    for x, w, h in stations:
        ring = []
        for j in range(sides):
            a = TAU * j / sides + math.pi / sides
            c, s = math.cos(a), math.sin(a)
            ring.append(V((x, w * math.copysign(abs(c) ** (2 / n_exp), c), h * math.copysign(abs(s) ** (2 / n_exp), s))))
        rings.append(ring)
    bm, info, cp = C.loft(rings, caps=(caps, caps))
    bmesh.ops.recalc_face_normals(bm, faces=bm.faces)
    g = C.group(info, pick) if pick else {}
    return C.mk(bm, name, color, g, smooth=True, flat=cp, project="box")


def fins(name, hw, x, s, color, teeth=6, depth=0.045):
    outline = [(0.0, -hw)]
    for t in range(teeth * 2 + 1):
        y = -hw + 2 * hw * t / (teeth * 2)
        outline.append((depth if t % 2 == 0 else depth * 0.72, y))
    outline.append((0.0, hw))
    if s < 0:
        outline = [(-a, b) for a, b in reversed(outline)]
    return K.polygon_prism(name, outline, 0.014, color=color, loc=(x, 0, -0.007), smooth=False)


def wrapped_bar(spec):
    p = spec["p"]
    L, W, Th = 0.38, 0.16, 0.06
    hw, hh = W / 2, Th / 2
    ex = L / 2 - 0.04
    st = [(-ex, hw * 0.95, 0.012), (-ex + 0.03, hw * 0.99, hh * 0.75), (-ex + 0.07, hw, hh), (ex - 0.07, hw, hh),
          (ex - 0.03, hw * 0.99, hh * 0.75), (ex, hw * 0.95, 0.012)]
    body = loft_bar("body", st, 12, 3.4, p["wrap"])
    band = loft_bar("band", [(-0.055, hw + 0.005, hh + 0.005), (0.055, hw + 0.005, hh + 0.005)], 12, 3.4, p["band"], caps=False)
    parts = [body, band, fins("f1", hw * 0.94, ex - 0.005, 1, p["wrap"]), fins("f2", hw * 0.94, -ex + 0.005, -1, p["wrap"])]
    tree = C.tree_of([band])
    outline, sz = {"bat": (C.BAT, 0.1), "pumpkin": (C.pumpkin_pts(), 0.075), "star": (C.star_pts(), 0.085)}[p["motif"]]
    for side in (1, -1):
        dc = C.decal(tree, V((0, 0, side * 0.3)), V((0, 0, -side)), outline, sz, p["motif_col"], up=(0, side, 0), off=0.003,
                     name=f"m{side}")
        if dc:
            parts.append(dc)
        if p["motif"] == "pumpkin" and dc:
            s2 = C.decal(tree, V((0, side * 0.03, side * 0.3)), V((0, 0, -side)), C.ngon(4, 1, math.pi / 4, 0.5, 1.0), 0.02,
                         "candy_green", up=(0, side, 0), off=0.004, name=f"s{side}")
            if s2:
                parts.append(s2)
    if p.get("dots"):
        tb = C.tree_of([body])
        for k, x in enumerate((-0.12, -0.09, 0.09, 0.12)):
            for side in (1, -1):
                y = 0.035 * (1 if k % 2 else -1)
                dc = C.decal(tb, V((x, y, side * 0.3)), V((0, 0, -side)), C.star_pts(), 0.03, p["dots"], up=(0, side, 0),
                             off=0.003, name=f"d{k}{side}")
                if dc:
                    parts.append(dc)
    return [C.xf(K.merge(parts, "bar"), rot=(62, 0, 0))]


def top_bevel_box(name, size, loc, color, bevel=0.01):
    bm = bmesh.new()
    bmesh.ops.create_cube(bm, size=1.0)
    bmesh.ops.scale(bm, vec=V(size), verts=bm.verts)
    bm.normal_update()
    bmesh.ops.delete(bm, geom=[f for f in bm.faces if f.normal.z < -0.9], context="FACES_ONLY")
    top_edges = [e for e in bm.edges if all(v.co.z > 0 for v in e.verts) and len(e.link_faces) == 2]
    bmesh.ops.bevel(bm, geom=top_edges, offset=bevel, segments=1, profile=0.5, affect="EDGES", clamp_overlap=True)
    ob = C.mk(bm, name, color, smooth=False, project="box")
    ob.location = V(loc)
    return ob


def open_bar(spec):
    p = spec["p"]
    L, W, Th = 0.4, 0.17, 0.06
    hw, hh = W / 2, Th / 2
    xs = 0.0
    st = [(-L / 2, hw * 0.95, 0.012), (-L / 2 + 0.03, hw * 0.99, hh * 0.78), (-L / 2 + 0.07, hw, hh), (xs, hw, hh)]
    sleeve = loft_bar("sleeve", st, 12, 3.4, p["wrap"])
    band = loft_bar("band", [(-0.11, hw + 0.005, hh + 0.005), (-0.05, hw + 0.005, hh + 0.005)], 12, 3.4, p["band"], caps=False)
    parts = [sleeve, band, fins("f2", hw * 0.94, -L / 2 + 0.005, -1, p["wrap"])]
    # torn wrapper edge: a zigzag collar
    zz = []
    for j in range(14):
        a = TAU * j / 14
        zz.append(V((xs + (0.018 if j % 2 else 0.0), math.cos(a) * (hw + 0.002), math.sin(a) * (hh + 0.002))))
    parts.append(K.box("collar", (0.012, W + 0.006, Th + 0.006), color=p["wrap"], loc=(xs + 0.004, 0, 0), bevel=0.003))
    slab = K.box("slab", (L / 2 - xs + 0.01, W - 0.012, Th * 0.45), color="choco_dark", loc=((xs + L / 2) / 2, 0, -Th * 0.22))
    car = K.box("car", (L / 2 - xs - 0.005, W - 0.02, Th * 0.16), color="caramel", loc=((xs + L / 2) / 2, 0, -Th * 0.02))
    parts += [slab, car]
    sq = (L / 2 - xs - 0.02) / 2
    for i in range(2):
        for j in range(2):
            parts.append(top_bevel_box(f"sq{i}{j}", (sq - 0.01, (W - 0.03) / 2, Th * 0.42),
                                       (xs + 0.012 + sq * (i + 0.5), (j - 0.5) * (W - 0.02) / 2, Th * 0.12), "choco",
                                       bevel=0.012))
    tree = C.tree_of([band])
    for side in (1, -1):
        dc = C.decal(tree, V((-0.08, 0, side * 0.3)), V((0, 0, -side)), C.star_pts(), 0.05, p["motif_col"], up=(0, side, 0),
                     off=0.003, name=f"m{side}")
        if dc:
            parts.append(dc)
    return [C.xf(K.merge(parts, "ob"), rot=(62, 0, 0))]


def pb_cups(spec):
    p = spec["p"]
    out = []
    for k, (loc, rot) in enumerate((((-0.078, -0.02, -0.02), (-6, 0, -8)), ((0.078, 0.03, 0.03), (-14, 10, 12)))):
        cup = lathe_obj(f"cup{k}", [(0.0, -0.07), (0.066, -0.07), (0.095, 0.012), (0.097, 0.016)], p["cup"], n=14,
                        fn=lambda a, z, kk: (1 + 0.07 * math.cos(7 * a) * (kk > 0), 0.0))
        top = lathe_obj(f"top{k}", [(0.091, 0.006), (0.096, 0.022), (0.084, 0.032), (0.04, 0.036), (0.0, 0.034)], "choco",
                        n=14, fn=lambda a, z, kk: (1 + 0.035 * math.cos(7 * a + 0.4) * (kk in (1, 2)), 0.0))
        parts = [cup, top]
        if k == 0 and p.get("swirl"):
            pts = []
            for q in range(14):
                th = TAU * 1.6 * q / 13
                r = 0.012 + 0.045 * q / 13
                pts.append((math.cos(th) * r, math.sin(th) * r, 0.037 - 0.006 * q / 13))
            parts.append(C.sweep("sw", pts, 0.006, sides=4, color=p["swirl"]))
        ob = K.merge(parts, f"pb{k}")
        out.append(C.xf(ob, loc=loc, rot=rot))
    return out


def caramel_apple(spec):
    p = spec["p"]
    prof = [(0.0, -0.1), (0.05, -0.105), (0.1, -0.08), (0.12, -0.03), (0.117, 0.03), (0.095, 0.075), (0.045, 0.095), (0.0, 0.082)]
    apple = lathe_obj("apple", prof, p["apple"], n=14)
    coat_prof = [(0.0, 0.088), (0.048, 0.1), (0.1, 0.079), (0.12, 0.042)]
    coat = lathe_obj("coat", [(r * 1.04, z) for r, z in coat_prof], p["coat"], n=14,
                     fn=lambda a, z, k: (1.0 + 0.02 * (k == 3), -0.06 * max(0.0, math.cos(5 * a)) ** 4 if k == 3 else 0.0))
    st = C.stick("stick", 0.17, r=0.014, color="wood_light", top=(0, 0, 0.27), direction=(0, 0, -1))
    parts = [apple, coat, st]
    rnd = random.Random(8)
    if p.get("bits"):
        tree = C.tree_of([coat])
        for k in range(14):
            a = TAU * k / 14 + rnd.uniform(-0.15, 0.15)
            d = V((math.cos(a), math.sin(a), rnd.uniform(0.45, 1.1))).normalized()
            loc, nrm = C.surf(tree, d * 0.5, -d)
            if loc is None:
                continue
            b = K.box(f"n{k}", (0.016, 0.014, 0.01), color=p["bits"], rot=(rnd.uniform(0, 60), rnd.uniform(0, 60), 0))
            D.put(b, loc, nrm, roll=rnd.uniform(0, 90))
            parts.append(b)
    if p.get("drizzle"):
        tree = C.tree_of([coat])
        pts = []
        for q in range(20):
            th = TAU * q / 19 * 1.0
            lat = 0.95 if q % 2 else 0.55
            d = V((math.cos(th) * math.cos(lat), math.sin(th) * math.cos(lat), math.sin(lat)))
            loc, nrm = C.surf(tree, d * 0.5, -d)
            if loc is not None:
                pts.append(loc + nrm * 0.005)
        parts.append(C.sweep("dz", pts, 0.0075, sides=5, color=p["drizzle"]))
    return parts


def truffle_box(spec):
    p = spec["p"]
    W, D_, H = 0.28, 0.22, 0.05
    parts = [K.box("floor", (W, D_, 0.016), color=p["box"], loc=(0, 0, -H / 2)),
             K.box("liner", (W - 0.03, D_ - 0.03, 0.006), color=p["liner"], loc=(0, 0, -H / 2 + 0.01))]
    for s in (1, -1):
        parts.append(K.box(f"wx{s}", (0.016, D_, H), color=p["box"], loc=(s * (W / 2 - 0.008), 0, 0)))
        parts.append(K.box(f"wy{s}", (W, 0.016, H), color=p["box"], loc=(0, s * (D_ / 2 - 0.008), 0)))
    lid = K.box("lid", (W + 0.012, D_ + 0.012, 0.016), color=p["box"], bevel=0.004)
    rib = K.box("rib", (0.04, D_ + 0.02, 0.02), color=p["ribbon"])
    lid = K.merge([lid, rib], "lidm")
    C.xf(lid, loc=(0, D_ / 2 + 0.02, D_ / 2 + 0.01), rot=(-100, 0, 0))
    parts.append(lid)
    rib2 = K.box("rib2", (0.04, D_ + 0.02, H + 0.008), color=p["ribbon"], loc=(0, 0, 0))
    parts.append(rib2)
    cols = p["truffles"]
    k = 0
    for ix in (-1, 1):
        for iy in (-1, 1):
            x, y = ix * 0.07, iy * 0.052
            parts.append(C.sphere(f"t{k}", 0.043, cols[k], loc=(x, y, -0.002), scale=(1, 1, 0.85), seg=8, rings=5))
            parts.append(C.sphere(f"tt{k}", 0.012, p["tops"][k], loc=(x, y, 0.036), scale=(1, 1, 0.6), seg=6, rings=3))
            k += 1
    return [C.xf(K.merge(parts, "box"), rot=(48, 0, 0))]


def choco_frog(spec):
    p = spec["p"]
    c = p["color"]
    # sitting frog, head up: round body, wide flat head, big eye domes with cream eyes, folded back legs,
    # little front feet, a smile and pink cheeks
    parts = [
        C.sphere("body", 0.1, c, loc=(0, 0.02, -0.03), scale=(1.1, 0.95, 0.8), seg=8, rings=6),
        C.sphere("head", 0.085, c, loc=(0, -0.045, 0.05), scale=(1.25, 0.95, 0.75), seg=10, rings=5),
    ]
    for s in (1, -1):    # white-chocolate eye domes
        parts.append(C.sphere(f"eye{s}", 0.04, p["eye"], loc=(s * 0.056, -0.055, 0.105), seg=8, rings=5))
        parts.append(C.sphere(f"leg{s}", 0.05, c, loc=(s * 0.1, 0.03, -0.07), scale=(0.65, 1.25, 0.5), seg=6, rings=4))
        parts.append(C.sphere(f"ft{s}", 0.03, c, loc=(s * 0.05, -0.095, -0.075), scale=(1.25, 1.0, 0.5), seg=6, rings=4))
    tree = C.tree_of(parts)
    for s in (1, -1):
        pu = C.decal(tree, V((s * 0.052, -0.4, 0.108)), V((0, 1, 0)), C.ngon(8), 0.03, "ink", off=0.004, name=f"pu{s}")
        ch = C.decal(tree, V((s * 0.075, -0.4, 0.04)), V((0, 1, 0)), C.ngon(8, sy=0.7), 0.03, "candy_pink", off=0.003,
                     name=f"ch{s}")
        parts += [x for x in (pu, ch) if x]
    smile = [(-1, 0.25), (-0.5, -0.25), (0, -0.4), (0.5, -0.25), (1, 0.25), (0.5, -0.05), (0, -0.15), (-0.5, -0.05)]
    sm = C.decal(tree, V((0, -0.4, 0.04)), V((0, 1, 0)), smile, 0.08, p["mouth"], off=0.003, name="smile", sub=1)
    if sm:
        parts.append(sm)
    return [C.xf(K.merge(parts, "frog"), rot=(-10, 0, 0))]


def choco_bat(spec):
    p = spec["p"]
    wing = C.puffy("wing", C.BAT, 0.42, 0.05, p["color"], edge=0.35)
    body = C.sphere("body", 0.065, p["color"], loc=(0, -0.012, 0.0), scale=(0.95, 0.75, 1.1), seg=10, rings=6)
    parts = [wing, body]
    tree = C.tree_of([body, wing])
    for s in (1, -1):
        e = C.decal(tree, V((s * 0.025, -0.4, 0.03)), V((0, 1, 0)), C.ngon(8), 0.03, p["eye"], off=0.003, name=f"e{s}")
        pu = C.decal(tree, V((s * 0.022, -0.4, 0.027)), V((0, 1, 0)), C.ngon(6), 0.015, "ink", off=0.006, name=f"p{s}")
        parts += [x for x in (e, pu) if x]
        for k, (x, z) in enumerate(((0.13, 0.03), (0.18, -0.02))):
            r = C.decal(tree, V((s * x, -0.4, z)), V((0, 1, 0)), C.ngon(4, sx=2.2, sy=0.35, phase=0), 0.05, p["accent"],
                        up=(s * 0.4, 0, 1), off=0.003, name=f"r{s}{k}")
            if r:
                parts.append(r)
    b = C.decal(tree, V((0, -0.4, -0.03)), V((0, 1, 0)), C.pumpkin_pts(), 0.04, p["badge"], off=0.004, name="badge")
    if b:
        parts.append(b)
    return [C.xf(K.merge(parts, "cb"), rot=(-15, 0, 0))]


def bar_trio(spec):
    p = spec["p"]
    out = []
    L, W, Th = 0.26, 0.1, 0.045
    hw, hh = W / 2, Th / 2
    ex = L / 2
    st = [(-ex, hw * 0.92, 0.01), (-ex + 0.03, hw, hh * 0.85), (ex - 0.03, hw, hh * 0.85), (ex, hw * 0.92, 0.01)]
    place = [((0, 0, -0.04), (0, 0, 0)), ((0.0, 0.0, 0.006), (0, 0, 16)), ((0, 0, 0.052), (0, 0, -12))]
    for k, (col, (loc, rot)) in enumerate(zip(p["wraps"], place)):
        b = loft_bar(f"bar{k}", st, 8, 3.4, col, pick=lambda i, j, f: "lolly_white" if i == 1 and j in (1, 2) else None)
        out.append(C.xf(b, loc=loc, rot=rot))
    rib_pts = [(0.0, -0.065, -0.07), (0.0, 0.065, -0.07), (0.0, 0.065, 0.08), (0.0, -0.065, 0.08)]
    ring = C.sweep("rib", rib_pts, 0.012, sides=4, color=p["ribbon"], closed=True, warp=lambda a, i: 1.0)
    out.append(ring)
    for s in (1, -1):
        lp = K.torus(f"bow{s}", 0.03, 0.01, color=p["ribbon"], major_seg=8, minor_seg=4)
        C.xf(lp, loc=(s * 0.03, 0.0, 0.1), rot=(90, s * 30, 0), scale=(1.0, 0.6, 1.0))
        out.append(lp)
    out.append(C.sphere("knot", 0.014, p["ribbon"], loc=(0, 0, 0.09), seg=6, rings=4))
    m = K.merge(out, "trio")
    return [C.xf(m, rot=(58, 0, 0))]


# =========================================================================== tier 2 (lollipop value)
def bow(color, loc, size=1.0):
    parts = []
    for s in (1, -1):
        lp = K.torus(f"loop{s}", 0.038 * size, 0.012 * size, color=color, major_seg=8, minor_seg=4)
        C.xf(lp, loc=(loc[0] + s * 0.036 * size, loc[1] - 0.004, loc[2]), rot=(90, s * 25, 0), scale=(1.0, 0.55, 1.0))
        parts.append(lp)
        tail = C.sweep(f"tail{s}", [(loc[0] + s * 0.005, loc[1] - 0.005, loc[2]), (loc[0] + s * 0.025 * size, loc[1] - 0.01, loc[2] - 0.04 * size),
                                    (loc[0] + s * 0.035 * size, loc[1] - 0.012, loc[2] - 0.075 * size)],
                       0.011 * size, sides=4, color=color, warp=lambda a, i: 0.3 if abs(math.sin(a)) > 0.7 else 1.0, up=(0, 1, 0))
        parts.append(tail)
    parts.append(C.sphere("knot", 0.016 * size, color, loc=(loc[0], loc[1] - 0.006, loc[2]), seg=6, rings=4))
    return parts


def lolly_swirl(spec):
    p = spec["p"]
    disc = lens("disc", 0.24, 0.05, p["cols"], n=18, twist_k=9.0, width=p.get("width", 3),
                radii=[0.0, 0.06, 0.12, 0.17, 0.21, 0.232])
    st = C.stick("stick", 0.3, r=0.018, color="lolly_white", top=(0, 0, -0.2))
    return [disc, st] + bow(p["bow"], (0, -0.02, -0.25))


def lolly_coil(spec):
    p = spec["p"]
    cols = p["cols"]
    N = 40
    pts = []
    for k in range(N):
        th = TAU * 2.45 * k / (N - 1)
        r = 0.03 + 0.06 * th / TAU
        pts.append((r * math.cos(th), 0.0, r * math.sin(th)))
    coil = C.tube("coil", pts, 0.029, sides=6, color=cols[0], twist=0.55, pick=lambda i, j, f: cols[(j // 2) % 3])
    cap = C.sphere("cap", 0.03, cols[1], loc=(0, 0, 0), scale=(1, 0.9, 1), seg=8, rings=5)
    st = C.stick("stick", 0.28, r=0.018, color="lolly_white", top=(0, 0.0, -0.16))
    return [coil, cap, st]


def candy_cane(spec):
    p = spec["p"]
    path = [V((0.0, 0.0, -0.3)), V((0.0, 0.0, 0.14))]
    Rh = 0.1
    for k in range(1, 11):
        a = math.pi * k / 10
        path.append(V((Rh - Rh * math.cos(a), 0.0, 0.14 + Rh * math.sin(a))))
    path.append(V((2 * Rh + 0.005, 0.0, 0.1)))
    # resample by arc length
    seg = [(path[i + 1] - path[i]).length for i in range(len(path) - 1)]
    tot = sum(seg)
    N = 34
    pts = []
    for q in range(N):
        d = tot * q / (N - 1)
        acc = 0.0
        for i, s in enumerate(seg):
            if acc + s >= d - 1e-9:
                t = (d - acc) / s if s else 0
                pts.append(path[i].lerp(path[i + 1], t))
                break
            acc += s
    cols = p["cols"]
    ob = C.tube("cane", pts, 0.042, sides=8, color=cols[0], twist=0.62, up=(0, -1, 0),
                pick=lambda i, j, f: cols[1] if j % 4 in (0, 1) else (cols[2] if j % 4 == 3 and len(cols) > 2 and i % 2 == 0 else None))
    return [C.xf(ob, loc=(-Rh, 0, 0), rot=(0, 0, 0))]


def lolly_jack(spec):
    p = spec["p"]
    R = 0.2

    def rim(a):
        return 1 - 0.11 * max(0.0, math.sin(a)) ** 10 - 0.06 * max(0.0, -math.sin(a)) ** 10

    def pick(i, j, f):
        c = f.calc_center_median()
        band = int((c.x + R * 1.2) / (R * 2.4 / 5))
        return p["rib"] if band % 2 == 1 else None
    disc = lens("disc", R, 0.07, [p["color"]], n=20, twist_k=0.0, rim_fn=rim, pick=pick,
                radii=[0.0, 0.06, 0.11, 0.155, 0.185])
    K.deform(disc, lambda v: V((v.x * 1.12, v.y, v.z)))
    tree = C.tree_of([disc])
    parts = [disc]
    face = p["face"]
    for s in (1, -1):
        e = C.decal(tree, V((s * 0.075, -0.4, 0.05)), V((0, 1, 0)), [(-1, -0.6), (1, -0.6), (0.1 * s, 0.9)], 0.085, face,
                    off=0.004, name=f"e{s}")
        if e:
            parts.append(e)
    nose = C.decal(tree, V((0, -0.4, -0.005)), V((0, 1, 0)), [(-1, -0.7), (1, -0.7), (0, 0.8)], 0.04, face, off=0.004, name="n")
    mouth = [(-0.85, 0.15), (-0.6, -0.3), (-0.3, -0.5), (0.0, -0.55), (0.3, -0.5), (0.6, -0.3), (0.85, 0.15), (0.5, -0.02),
             (0.38, 0.16), (0.24, -0.06), (-0.24, -0.06), (-0.38, 0.16), (-0.5, -0.02)]
    m = C.decal(tree, V((0, -0.4, -0.065)), V((0, 1, 0)), mouth, 0.2, face, off=0.004, name="m")
    parts += [x for x in (nose, m) if x]
    stem = K.cylinder("stem", 0.024, 0.06, color="candy_green", verts=6, radius_top=0.016, smooth=True)
    D.put(stem, (0.005, 0, R * 0.92 + 0.02), (0.2, 0, 1))
    leaf = C.puffy("leaf", C.ngon(8, sx=1.0, sy=0.5), 0.08, 0.012, "candy_green")
    C.xf(leaf, loc=(-0.05, -0.005, R * 0.93), rot=(0, 0, 0))
    st = C.stick("stick", 0.26, r=0.018, color="lolly_white", top=(0, 0, -0.16))
    return parts + [stem, leaf, st]


def wand(spec):
    p = spec["p"]
    n = 12
    pts = [(0.0, 0.0, -0.36 + 0.4 * k / (n - 1)) for k in range(n)]
    sw = C.tube("stick", pts, 0.019, sides=6, color="lolly_white", twist=0.7, pick=lambda i, j, f: p["stripe"] if j % 3 == 0 else None)
    star = C.puffy("star", C.star_pts(5, 1.0, 0.5), 0.25, 0.06, p["star"], edge=0.45)
    C.xf(star, loc=(0, 0, 0.13))
    parts = [sw, star]
    for s, col in ((1, p["ribbons"][0]), (-1, p["ribbons"][1])):
        rp = [(0.0, 0.0, 0.03), (s * 0.04, -0.01, -0.02), (s * 0.05, -0.015, -0.08), (s * 0.085, -0.01, -0.13),
              (s * 0.09, -0.005, -0.18)]
        parts.append(C.sweep(f"r{s}", rp, 0.016, sides=4, color=col, warp=lambda a, i: 0.25 if abs(math.sin(a)) > 0.7 else 1.0,
                             up=(0, 1, 0)))
    for k, (x, z, sz) in enumerate(((-0.16, 0.22, 0.055), (0.15, 0.03, 0.045), (0.14, 0.25, 0.04))):
        sp = C.puffy(f"sp{k}", C.star_pts(4, 1.0, 0.38), sz, 0.014, p["star"], edge=0.4)
        parts.append(C.xf(sp, loc=(x, 0, z)))
    return parts


def lolly_ghost(spec):
    p = spec["p"]
    g = C.puffy("ghost", C.ghost_pts(), 0.32, 0.075, p["color"], edge=0.45)
    tree = C.tree_of([g])
    parts = [g]
    for s in (1, -1):
        e = C.decal(tree, V((s * 0.045, -0.4, 0.06)), V((0, 1, 0)), C.ngon(8, sy=1.35), 0.04, "ink", off=0.003, name=f"e{s}")
        ch = C.decal(tree, V((s * 0.085, -0.4, 0.015)), V((0, 1, 0)), C.ngon(8, sy=0.65), 0.04, p["cheek"], off=0.003,
                     name=f"c{s}")
        parts += [x for x in (e, ch) if x]
    m = C.decal(tree, V((0, -0.4, 0.015)), V((0, 1, 0)), C.ngon(8, sy=1.2), 0.03, "ink", off=0.003, name="m")
    if m:
        parts.append(m)
    st = C.stick("stick", 0.24, r=0.018, color="lolly_white", top=(0, 0.0, -0.06))
    return parts + [st] + bow(p["bow"], (0, -0.03, -0.12))


# =========================================================================== tier 3 (golden)
def golden_twist(spec):
    """The house twist-wrapped sweet (round ball, pleated crinkled bow-tie ends, spiral stripes) made big and
    golden: foil gold body with two thin cream spirals, darker gold crinkled fans, glowing gold twist knots
    and a glowing gold star on the front and back."""
    p = spec["p"]
    ob = twist_body("foil_gold", "foil_gold_dark", "spiral", p["stripe"], "ball", SEG=18, stripe=(9, (0, 1)),
                    neck="glow_candy_gold", crinkle=1.35)
    tree = C.tree_of([ob])
    parts = [ob]
    for k, ang in enumerate((22, 158)):
        o, d = C.toward_axis_x(0.0, ang)
        dc = C.decal(tree, o, d, C.star_pts(5, 1.0, 0.46), 0.085, "glow_candy_gold", up=(0, 0, 1), off=0.004,
                     name=f"star{k}", sub=1)
        if dc:
            parts.append(dc)
    return parts



def golden_apple(spec):
    prof = [(0.0, -0.1), (0.05, -0.105), (0.1, -0.08), (0.12, -0.03), (0.117, 0.03), (0.095, 0.075), (0.045, 0.095), (0.0, 0.082)]
    apple = lathe_obj("apple", prof, "foil_gold", n=18)
    coat_prof = [(0.0, 0.088), (0.048, 0.1), (0.1, 0.079), (0.122, 0.034), (0.126, -0.006)]
    coat = lathe_obj("coat", [(r * 1.035, z) for r, z in coat_prof], "glow_candy_gold", n=18,
                     fn=lambda a, z, k: (1.0, -0.05 * max(0.0, math.cos(6 * a)) ** 3 if k == 4 else 0.0))
    stem = K.cylinder("stem", 0.012, 0.07, color="foil_gold_dark", verts=6, radius_top=0.009, smooth=True)
    D.put(stem, (0.004, 0, 0.125), (0.15, 0, 1))
    leaf = C.puffy("leaf", C.ngon(10, sx=1.0, sy=0.5), 0.11, 0.014, "foil_gold_dark")
    C.xf(leaf, loc=(0.06, -0.005, 0.135), rot=(0, -25, 0))
    parts = [apple, coat, stem, leaf]
    for k, (x, z, sz) in enumerate(((-0.17, 0.12, 0.06), (0.17, -0.05, 0.05))):
        sp = C.puffy(f"sp{k}", C.star_pts(4, 1.0, 0.38), sz, 0.014, "glow_candy_gold", edge=0.4)
        parts.append(C.xf(sp, loc=(x, -0.02, z)))
    return parts


def golden_pumpkin(spec):
    prof = [(0.0, -0.09), (0.07, -0.093), (0.13, -0.07), (0.155, -0.02), (0.15, 0.03), (0.12, 0.072), (0.06, 0.092), (0.0, 0.08)]
    n = 24

    def pick(i, j, f):
        return "foil_gold_dark" if j % 3 == 2 else None
    ob = lathe_obj("pump", prof, "foil_gold", n=n, phase=0.0,
                   fn=lambda a, z, k: (1 - 0.09 * (math.cos(8 * a) + 1) / 2 * (0 < k < len(prof) - 1), 0.0), pick=pick)
    tree = C.tree_of([ob])
    parts = [ob]
    face = "glow_candy_gold"
    for s in (1, -1):
        e = C.decal(tree, V((s * 0.055, -0.5, 0.025)), V((0, 1, 0)), [(-1, -0.6), (1, -0.6), (0.1 * s, 0.9)], 0.055, face,
                    off=0.005, name=f"e{s}")
        if e:
            parts.append(e)
    mouth = [(-0.85, 0.15), (-0.6, -0.3), (-0.3, -0.5), (0.0, -0.55), (0.3, -0.5), (0.6, -0.3), (0.85, 0.15), (0.5, -0.02),
             (0.38, 0.16), (0.24, -0.06), (-0.24, -0.06), (-0.38, 0.16), (-0.5, -0.02)]
    m = C.decal(tree, V((0, -0.5, -0.028)), V((0, 1, 0)), mouth, 0.13, face, off=0.005, name="m")
    if m:
        parts.append(m)
    stem = K.cylinder("stem", 0.025, 0.07, color="foil_gold_dark", verts=6, radius_top=0.017, smooth=True)
    D.put(stem, (0.005, 0, 0.11), (0.2, 0, 1))
    vine = C.sweep("vine", [(0.01, 0, 0.09), (0.05, -0.01, 0.11), (0.08, -0.04, 0.105), (0.075, -0.065, 0.085),
                            (0.055, -0.07, 0.075)], 0.008, sides=5, color="glow_candy_gold")
    leaf = C.puffy("leaf", C.ngon(10, sx=1.0, sy=0.55), 0.1, 0.014, "glow_candy_gold")
    C.xf(leaf, loc=(-0.06, 0.0, 0.1), rot=(15, 0, 22))
    return parts + [stem, vine, leaf]


def golden_crown(spec):
    n = 18
    pts = 6
    R, Ri, H = 0.15, 0.135, 0.07

    def topz(j):
        ph = (j % (n // pts)) / (n // pts)          # 0 at a point, rises then valley
        return H + (0.075 if j % (n // pts) == 0 else 0.0)
    outer_b = [V((math.cos(TAU * j / n) * R, math.sin(TAU * j / n) * R, -H)) for j in range(n)]
    outer_t = [V((math.cos(TAU * j / n) * R * 1.05, math.sin(TAU * j / n) * R * 1.05, topz(j))) for j in range(n)]
    inner_t = [V((math.cos(TAU * j / n) * Ri * 1.05, math.sin(TAU * j / n) * Ri * 1.05, topz(j) - 0.004)) for j in range(n)]
    inner_b = [V((math.cos(TAU * j / n) * Ri, math.sin(TAU * j / n) * Ri, -H)) for j in range(n)]
    bm, info, _ = C.loft([outer_b, outer_t, inner_t, inner_b], closed=True)
    bmesh.ops.recalc_face_normals(bm, faces=bm.faces)
    g = C.group(info, lambda i, j, f: "glow_candy_gold" if i == 0 and abs(f.calc_center_median().z + H * 0.55) < 0.02 else None)
    band = C.mk(bm, "band", "foil_gold", g, smooth=False, project="cyl")
    rim = K.torus("rim", (R + Ri) / 2 + 0.008, 0.014, color="glow_candy_gold", major_seg=n, minor_seg=4)
    C.xf(rim, loc=(0, 0, -H + 0.004))
    parts = [band, rim]
    for k in range(pts):
        a = TAU * k * (n // pts) / n
        parts.append(C.sphere(f"tip{k}", 0.02, "glow_candy_gold", loc=(math.cos(a) * R * 1.05, math.sin(a) * R * 1.05, H + 0.09),
                              seg=6, rings=4))
    gems = ["candy_cherry", "candy_cobalt", "candy_green"]
    for k in range(6):
        a = TAU * (k + 0.5) / 6
        d = V((math.cos(a), math.sin(a), 0))
        o = octa(f"gem{k}", 0.05, 0.022, gems[k % 3])
        D.put(o, d * (R + 0.01) + V((0, 0, 0.0)), d, roll=45)
        parts.append(o)
    cushion = lathe_obj("cush", [(Ri * 0.98, -0.02), (Ri * 0.85, 0.05), (Ri * 0.5, 0.085), (0.0, 0.095)], "candy_berry", n=12)
    parts.append(cushion)
    return [C.xf(K.merge(parts, "crown"), rot=(-22, 0, 0))]


FAMILIES = {
    "twist": twist, "gumball_trio": gumball_trio, "gumball_rainbow": gumball_rainbow, "jawbreaker": jawbreaker,
    "eyeball": eyeball, "popcorn_ball": popcorn_ball, "caramel_cube": caramel_cube, "jellybean": jellybean, "jelly_trio": jelly_trio,
    "gummy_bear": gummy_bear, "gummy_worm": gummy_worm, "gummy_bat": gummy_bat, "gummy_ring": gummy_ring,
    "swedish_fish": swedish_fish,
    "peppermint": peppermint, "butterscotch": butterscotch, "coin": coin, "corn": corn, "gumdrop": gumdrop,
    "gumdrop_pile": gumdrop_pile, "kiss": kiss, "licorice": licorice, "licorice_wheel": licorice_wheel,
    "sour_belt": sour_belt, "candy_buttons": candy_buttons, "rock_candy": rock_candy, "marsh_ghost": marsh_ghost,
    "mellow_pumpkin": mellow_pumpkin, "candy_apple_mini": candy_apple_mini, "cakepop": cakepop, "bonbon": bonbon,
    "fizz_roll": fizz_roll, "fizz_tube": fizz_tube, "puffy_candy": puffy_candy, "strawberry": strawberry,
    "ring_pop": ring_pop, "cotton_candy": cotton_candy,
    "wrapped_bar": wrapped_bar, "open_bar": open_bar, "pb_cups": pb_cups, "caramel_apple": caramel_apple,
    "truffle_box": truffle_box, "choco_frog": choco_frog, "choco_bat": choco_bat, "bar_trio": bar_trio,
    "lolly_swirl": lolly_swirl, "lolly_coil": lolly_coil, "candy_cane": candy_cane, "lolly_jack": lolly_jack,
    "wand": wand, "lolly_ghost": lolly_ghost,
    "golden_apple": golden_apple, "golden_twist": golden_twist,"golden_pumpkin": golden_pumpkin, "golden_crown": golden_crown,
}
BUDGET = {0: 500, 1: 500, 2: 700, 3: 700}


def selected(spec):
    if not SEL:
        return True
    return spec["id"] in SEL or f"tier{spec['tier']}" in SEL or spec["fam"] in SEL


fails = []
for spec in SPECS:
    if not selected(spec):
        continue
    K.reset()
    try:
        parts = FAMILIES[spec["fam"]](spec)
        C.finish(parts, spec["id"], spec["size"], spec["tier"], ARGS, BUDGET[spec["tier"]])
    except Exception as e:  # keep going; report at the end
        import traceback
        traceback.print_exc()
        fails.append((spec["id"], repr(e)))
print("FAILED:", fails if fails else "none")
