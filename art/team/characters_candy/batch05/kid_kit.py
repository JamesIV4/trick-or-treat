"""The trick-or-treater kid, parametrised from batch03/build_dragon_kid.py so every costume shares
one rig contract, one face, one pair of hands and one candy bucket.

What lives here (the dragon script's code, turned into functions with colour/shape parameters):
  joints           HIP_*, SHO_*, NECK, WING_*, TAIL_BASE, HAND_*  (identical to dragon_kid)
  shape helpers    catmull, lerp_list, blob, soft_spike, ellipsoid_pt/dir, rounded_poly, on_surface
  torso()          the dragon's tummy lathe (any colour/material)
  patch()          a pillowy sewn-on oval conforming to a surface (the dragon belly, generalised)
  leg()            thigh + boot + sole (+ optional toe bits, stripes, pointy toe)
  arm()            sleeve + the kid's own skin hand and thumb (sleeve radius/cuff flare/colour)
  hood()           the dragon hood with face opening, puffy rim and rim stitches (shape warp)
  face()           the kid's face: skin oval, ink eyes + glints + highlights, cheeks, smile (+ fang)
  fringe()         brown hair tufts and temple locks peeking out under a hood or hat
  bare_head()      face + hair cap + ears, for costumes without a hood
  bucket()         the glowing jack-o'-lantern bucket and candy heap, exactly as the dragon's
  finish()         hierarchy, AO, export, review sheet (same node layout as dragon_kid)

Character right = Blender -X, front = -Y. Every part is its own object with its origin at the joint
and identity rotation/scale, parented (no parent inverse) to an empty root named after the asset id.
"""
import math
import sys
from pathlib import Path

HERE = Path(__file__).resolve().parent
sys.path.insert(0, str(HERE.parent / "batch03"))   # reuse the lane's mesh helpers (dk_kit) as-is
import bmesh  # noqa: E402
import bpy  # noqa: E402
from mathutils import Matrix, Vector  # noqa: E402
from mathutils.bvhtree import BVHTree  # noqa: E402

import dk_kit as D  # noqa: E402
from dk_kit import K, V  # noqa: E402

FELT = "felt"

# ------------------------------------------------------------------ joints (identical to dragon_kid)
HIP_Z = 0.30
HIP_L, HIP_R = (0.085, 0.0, HIP_Z), (-0.085, 0.0, HIP_Z)
SHO_L, SHO_R = (0.165, 0.0, 0.585), (-0.165, 0.0, 0.585)
NECK = (0.0, 0.0, 0.64)
WING_L, WING_R = (0.075, 0.135, 0.56), (-0.075, 0.135, 0.56)
TAIL_BASE = (0.0, 0.14, 0.34)
HAND_L = V((0.245, -0.03, 0.405))
HAND_R = V((-0.26, -0.085, 0.415))

# head geometry (dragon hood and face)
HC, HR = V((0, 0.012, 0.83)), V((0.268, 0.248, 0.238))
FC, FR = V((0, -0.118, 0.80)), V((0.198, 0.128, 0.178))
OA, OC, OZ = 0.18, 0.165, 0.80       # face opening ellipse (x radius, z radius, centre z)


# ------------------------------------------------------------------ shape helpers (from the dragon script)
def catmull(pts, per=4):
    """Catmull-Rom resample through pts (endpoints kept). Returns (points, params 0..1)."""
    P = [V(p) for p in pts]
    P = [P[0] * 2 - P[1]] + P + [P[-1] * 2 - P[-2]]
    out, par = [], []
    nseg = len(pts) - 1
    for s in range(nseg):
        p0, p1, p2, p3 = P[s], P[s + 1], P[s + 2], P[s + 3]
        for k in range(per):
            t = k / per
            t2, t3 = t * t, t * t * t
            out.append(0.5 * ((2 * p1) + (-p0 + p2) * t + (2 * p0 - 5 * p1 + 4 * p2 - p3) * t2 + (-p0 + 3 * p1 - 3 * p2 + p3) * t3))
            par.append((s + t) / nseg)
    out.append(V(pts[-1]))
    par.append(1.0)
    return out, par


def lerp_list(vals, t):
    x = t * (len(vals) - 1)
    i = min(int(x), len(vals) - 2)
    f = x - i
    return vals[i] * (1 - f) + vals[i + 1] * f


def soft_spike(name, h, r, segs=7, color="dragon_spike", mat=FELT):
    """Rounded felt spike/cone (pointing up local Z, base at z=0)."""
    prof = [(r, -0.008), (r * 0.9, h * 0.3), (r * 0.62, h * 0.62), (r * 0.28, h * 0.88), (0.0, h)]
    return D.lathe(name, prof, segs, color=color, mat=mat, project="cyl")


def ellipsoid_pt(c, rad, x, z, front=True):
    q = 1 - ((x - c.x) / rad.x) ** 2 - ((z - c.z) / rad.z) ** 2
    y = c.y + (-1 if front else 1) * rad.y * math.sqrt(max(q, 0.0))
    p = V((x, y, z))
    n = V(((p.x - c.x) / rad.x ** 2, (p.y - c.y) / rad.y ** 2, (p.z - c.z) / rad.z ** 2)).normalized()
    return p, n


def ellipsoid_dir(c, rad, d):
    d = V(d).normalized()
    s = 1.0 / math.sqrt((d.x / rad.x) ** 2 + (d.y / rad.y) ** 2 + (d.z / rad.z) ** 2)
    p = c + d * s
    n = V(((p.x - c.x) / rad.x ** 2, (p.y - c.y) / rad.y ** 2, (p.z - c.z) / rad.z ** 2)).normalized()
    return p, n


def ell_prof(rings):
    out = []
    for i in range(rings + 1):
        phi = math.pi * i / rings
        out.append((0.0 if i in (0, rings) else math.sin(phi), -math.cos(phi)))
    return out


def blob(name, center, rad, segs=16, rings=10, color="dragon_jade", mat=FELT, project="sphere", cut=None, warp=None):
    """Smooth ellipsoid at center with radii rad; optional face cut(world face centre) -> delete,
    optional warp(world point) -> world point applied after scaling."""
    rad = V(rad)
    c0 = V(center)

    def w(c):
        p = V((c.x * rad.x, c.y * rad.y, c.z * rad.z))
        return warp(p + c0) - c0 if warp else p
    ob = D.lathe(name, ell_prof(rings), segs, color=color, mat=mat, project=project, warp=w, center=(0, 0, 0))
    ob.location = c0
    if cut:
        bm = bmesh.new()
        bm.from_mesh(ob.data)
        bmesh.ops.delete(bm, geom=[f for f in bm.faces if cut(f.calc_center_median() + c0)], context="FACES")
        bm.to_mesh(ob.data)
        bm.free()
    return ob


def rounded_poly(corners, rad=0.006, n=3):
    """Polygon with filleted corners (CCW list of (x, z))."""
    out = []
    k = len(corners)
    for i in range(k):
        p0, p1, p2 = V(corners[i - 1]), V(corners[i]), V(corners[(i + 1) % k])
        a = (p0 - p1).normalized()
        b = (p2 - p1).normalized()
        q0, q2 = p1 + a * rad, p1 + b * rad
        for j in range(n + 1):
            t = j / n
            out.append(tuple(q0 * (1 - t) ** 2 + p1 * 2 * t * (1 - t) + q2 * t * t))
    return out


def on_surface(tree, p, d):
    """Project world point p onto the surface along d (cast from 1 m back). Returns (loc, normal)."""
    d = V(d).normalized()
    return D.hit(tree, V(p) - d * 1.0, d, 3.0)


def decal(name, tree, outline, center, ray, up=(0, 0, 1), lift=0.004, thick=0.006, color="ink", mat="flat",
          scale=1.0):
    """A flat felt appliqué: a 2D outline [(u, v)] laid on a surface around 'center', seen along 'ray'
    (u to the right of the viewer, v along 'up'), each vertex conformed to the surface. Front cap +
    side walls (the back is hidden in the surface)."""
    ray = V(ray).normalized()
    rgt = ray.cross(V(up)).normalized()     # viewer's right when looking along ray
    upv = rgt.cross(ray).normalized()
    bm = bmesh.new()
    top, bot = [], []
    for u, v in outline:
        q = V(center) + rgt * u * scale + upv * v * scale
        loc, nrm = on_surface(tree, q, ray)
        if loc is None:
            loc, nrm = q, -ray
        top.append(bm.verts.new(loc + nrm * (lift + thick)))
        bot.append(bm.verts.new(loc - nrm * 0.002))
    f = bm.faces.new(top)
    f.normal_update()
    if f.normal.dot(ray) > 0:
        f.normal_flip()
    cen = sum((v.co for v in top), V((0, 0, 0))) / len(top)
    n = len(top)
    for i in range(n):
        j = (i + 1) % n
        w = bm.faces.new((top[i], bot[i], bot[j], top[j]))
        w.normal_update()
        if w.normal.dot(w.calc_center_median() - cen) < 0:
            w.normal_flip()
    bmesh.ops.triangulate(bm, faces=[f])
    return D.mesh(bm, name, color, mat, "box", smooth=False)


def strip(name, tree, rows, center, ray, up=(0, 0, 1), lift=0.001, thick=0.004, color="ink", mat="flat"):
    """A curved applique band (a swoosh, a stripe, a bone): rows [(u, v_top, v_bottom)] along the band in
    the decal() plane, each edge vertex conformed to the surface. Built as quads between the two edges,
    so concave and crescent shapes stay clean (decal() fans a single polygon)."""
    ray = V(ray).normalized()
    rgt = ray.cross(V(up)).normalized()
    upv = rgt.cross(ray).normalized()

    def onto(u, v):
        q = V(center) + rgt * u + upv * v
        loc, nrm = on_surface(tree, q, ray)
        return (q, -ray) if loc is None else (loc, nrm)
    bm = bmesh.new()
    T, B, Tb, Bb = [], [], [], []
    for u, vt, vb in rows:
        for v, front, back in ((vt, T, Tb), (vb, B, Bb)):
            loc, nrm = onto(u, v)
            front.append(bm.verts.new(loc + nrm * (lift + thick)))
            back.append(bm.verts.new(loc - nrm * 0.002))
    n = len(rows)
    fronts = [(T[i], T[i + 1], B[i + 1], B[i]) for i in range(n - 1)]
    walls = [(T[i + 1], T[i], Tb[i], Tb[i + 1]) for i in range(n - 1)]              # top edge
    walls += [(B[i], B[i + 1], Bb[i + 1], Bb[i]) for i in range(n - 1)]             # bottom edge
    walls += [(T[0], B[0], Bb[0], Tb[0]), (B[-1], T[-1], Tb[-1], Bb[-1])]           # ends
    for i, fv in enumerate(fronts + walls):
        f = bm.faces.new(fv)
        f.normal_update()
        if i < len(fronts):
            out = -ray                                          # the face of the band looks back at the viewer
        else:
            k = i - len(fronts)
            j = k % (n - 1) if k < 2 * (n - 1) else (0 if k == 2 * (n - 1) else n - 1)
            mid = (T[j].co + B[j].co) / 2
            out = f.calc_center_median() - mid
        if f.normal.dot(out) < 0:
            f.normal_flip()
    return D.mesh(bm, name, color, mat, "box", smooth=True)


def torso_r(z, prof=None):
    """Radius (X) of the dragon torso lathe at height z (0 outside it)."""
    prof = prof or TORSO_PROF
    for (r0, z0), (r1, z1) in zip(prof[:-1], prof[1:]):
        if z0 <= z <= z1:
            return r0 + (r1 - r0) * (z - z0) / max(z1 - z0, 1e-9)
    return 0.0


# ------------------------------------------------------------------ body
TORSO_PROF = [(0.0, 0.215), (0.08, 0.222), (0.135, 0.245), (0.172, 0.28), (0.195, 0.33), (0.203, 0.385),
              (0.198, 0.44), (0.183, 0.5), (0.162, 0.56), (0.138, 0.61), (0.11, 0.655), (0.07, 0.685), (0.0, 0.695)]


def torso_warp(c):
    y = c.y * 0.88
    if y < 0:   # a soft round tummy pushing forward at belly height
        y *= 1.0 + 0.1 * math.exp(-((c.z - 0.42) / 0.12) ** 2)
    return V((c.x, y + 0.012, c.z))


def torso(color, mat=FELT, name="torso", prof=None, warp=None, segs=20):
    return D.lathe(name, prof or TORSO_PROF, segs, color=color, mat=mat, project="sphere", warp=warp or torso_warp,
                   center=(0, 0, 0.45))


def patch(name, tree, pc, pru, prv, color, mat="flat", seg=22, heights=(0.010, 0.0095, 0.0075, 0.0038, -0.002),
          ray=(0, 1, 0)):
    """Pillowy oval patch conforming to a surface (the dragon's belly patch), cast along 'ray'."""
    ray = V(ray).normalized()
    fr = [0.0, 0.45, 0.75, 0.92, 1.0]
    # in-plane axes: x across, the remaining one up-ish
    ax = V((1, 0, 0)) if abs(ray.x) < 0.9 else V((0, 1, 0))
    ay = ray.cross(ax).normalized()
    if ay.z < 0:
        ay = -ay
    bm = bmesh.new()
    rings = []
    for f, h in zip(fr, heights):
        ring = []
        for j in range(1 if f == 0 else seg):
            a = 2 * math.pi * j / seg
            q = V(pc) + ax * math.cos(a) * pru * f + ay * math.sin(a) * prv * f
            loc, nrm = on_surface(tree, q, ray)
            ring.append(bm.verts.new(loc + nrm * h))
        rings.append(ring)
    for i in range(len(rings) - 1):
        A, B = rings[i], rings[i + 1]
        for j in range(seg):
            j2 = (j + 1) % seg
            if len(A) == 1:
                bm.faces.new((A[0], B[j], B[j2]))
            else:
                bm.faces.new((A[j], B[j], B[j2], A[j2]))
    for fc in bm.faces:
        fc.normal_update()
        if fc.normal.dot(ray) > 0:
            fc.normal_flip()
    ob = D.mesh(bm, name, color, mat, "box", center=V(pc))
    stitch = D.ring_on(tree, pc, ax, ay, pru * 0.84, prv * 0.84, 16, ray, back=1.0, offset=heights[1] - 0.0007)
    return ob, stitch


# ------------------------------------------------------------------ legs (origin at hip)
def boot_warp_fn(toe=0.0, curl=0.0):
    def w(c):
        y = c.y * 1.32 - 0.028
        if c.y < 0:
            y -= 0.012 * (1 - abs(c.x) / 0.1)
            if toe:   # pointy toe: pull the front out and narrow it, optionally curling the tip up
                k = max(0.0, -c.y / 0.096) ** 2 * max(0.0, 1 - abs(c.z - 0.06) / 0.08)
                y -= toe * k
                c = V((c.x * (1 - 0.45 * k), c.y, c.z + curl * k * k))
        return V((c.x, y, c.z))
    return w


SNEAKER = dict(upper="lolly_white", panel="candy_grape", toe="lolly_white", sole="lolly_white", stripe="candy_grape",
               lace="candy_orange", collar="candy_grape", tongue="candy_grape", heel="candy_orange", mat="vinyl")


def paint_grid(ob, segs, fn, mat="flat"):
    """Recolour a D.lathe mesh band by band: its first faces run ring-major (face k = band k // segs,
    segment k % segs, segment 0 starting at +X and turning toward +Y), so picking whole bands and
    segments paints clean edges along the grid. fn(band, seg) -> colour or None."""
    bm = bmesh.new()
    bm.from_mesh(ob.data)
    groups = {}
    for k, f in enumerate(bm.faces):
        col = fn(k // segs, k % segs)
        if col:
            groups.setdefault(col, []).append(f)
    for col, fs in groups.items():
        K._paint_bm(bm, col, mat, fs)
    bm.to_mesh(ob.data)
    bm.free()


def sneaker(name, x, colors=None):
    """A chunky cartoon high-top (the costume reference's shoes), in world space around the foot at x:
    a thick two-tone rubber sole (coloured outsole under a white midsole, a little toe spring), a full
    upper with a rubber toe cap and a swoosh panel appliqued on both sides, a padded ankle collar, a
    tall tongue, criss-cross laces over the instep and a heel pull tab. colors: SNEAKER keys (upper,
    panel, toe, sole, stripe, lace, collar, tongue, heel, mat) overriding the defaults. Same footprint
    as the old boot (y -0.17..0.11, height 0.18 m)."""
    c = dict(SNEAKER, **(colors or {}))
    mat = c["mat"]
    SEGS = 28

    def foot_k(z):          # 1 down in the foot, 0 up at the ankle (the ankle stays round around the leg)
        return min(max((0.135 - z) / 0.075, 0.0), 1.0)

    def upper_warp(p):
        k = foot_k(p.z)
        y = p.y * (1.0 + 0.36 * k) - 0.03 * k
        z = p.z
        if p.y < 0:
            y -= 0.012 * k * (1 - min(abs(p.x) / 0.1, 1.0))
            z -= 0.3 * k * max(p.z - 0.06, 0.0) * max(0.0, -p.y / 0.1)     # the toe box slopes down to the tip
        return V((p.x * (1.0 + 0.04 * k), y, z))
    up_prof = [(0.0, 0.034), (0.06, 0.034), (0.082, 0.039), (0.094, 0.05), (0.1, 0.064), (0.101, 0.078), (0.099, 0.094),
               (0.094, 0.11), (0.089, 0.126), (0.086, 0.142), (0.084, 0.158), (0.083, 0.17), (0.0, 0.172)]
    upper = D.lathe(name + "_upper", up_prof, SEGS, color=c["upper"], mat=mat, project="sphere", warp=upper_warp,
                    loc=(x, 0, 0), center=(0, 0, 0.09))
    front = 0.75 * SEGS                                     # the segment facing -Y

    def toe(band, seg):                                     # rubber toe cap: the low front bands
        da = min(abs(seg + 0.5 - front), SEGS - abs(seg + 0.5 - front))
        return c["toe"] if 2 <= band <= 6 and da < SEGS * 0.21 else None
    paint_grid(upper, SEGS, toe, mat="flat")
    tree = D.bvh(upper)
    parts = [upper]
    # swoosh panels on both sides, swept from the toe up toward the heel
    for side in (1, -1):
        ray = V((-side, 0, 0))
        rgt = ray.cross(V((0, 0, 1)))
        flip = 1 if rgt.y > 0 else -1                      # +u runs toward the heel on both sides
        rows = []
        for i in range(13):
            s = i / 12
            v = -0.018 + 0.05 * s ** 1.4
            w = 0.003 + 0.013 * math.sin(math.pi * min(s * 1.15, 1.0)) + 0.004 * s
            rows.append(((-0.088 + 0.17 * s) * flip, v + w, v - w * 0.7))
        parts.append(strip(f"{name}_swoosh{side}", tree, rows, (x + side * 0.15, -0.012, 0.086), ray, thick=0.004,
                           color=c["panel"], mat=mat))
    # two-tone sole: the coloured outsole under a white midsole (separate pieces: clean straight stripe)
    def sole_warp(p):
        y = p.y * 1.4 - 0.034
        return V((p.x * 1.05, y, p.z + 0.016 * max(0.0, (-y - 0.1) / 0.07) ** 2))     # toe spring
    out_prof = [(0.0, 0.0), (0.088, 0.0), (0.1, 0.006), (0.104, 0.016), (0.0, 0.017)]
    mid_prof = [(0.0, 0.016), (0.105, 0.016), (0.106, 0.03), (0.101, 0.044), (0.092, 0.051), (0.0, 0.053)]
    parts.append(D.lathe(name + "_outsole", out_prof, 20, color=c["stripe"], mat="flat", project="cyl", warp=sole_warp,
                         loc=(x, 0, 0)))
    parts.append(D.lathe(name + "_midsole", mid_prof, 20, color=c["sole"], mat="flat", project="cyl", warp=sole_warp,
                         loc=(x, 0, 0)))
    # padded ankle collar
    cp = [V((x + math.cos(a) * 0.087, math.sin(a) * 0.087 + 0.004, 0.166 - 0.008 * math.sin(a)))
          for a in (2 * math.pi * i / 18 for i in range(18))]
    parts.append(D.sweep(name + "_collar", cp, 0.018, sides=8, color=c["collar"], mat=mat, closed=True, center=(x, 0, 0.166)))
    # tongue: a soft flap standing up out of the front of the collar
    tongue = blob(name + "_tongue", (0, 0, 0), (0.044, 0.016, 0.052), segs=12, rings=6, color=c["tongue"], mat=mat)
    D.put(tongue, V((x, -0.084, 0.17)), V((0, -1, 0.35)), up=(0, 0, 1))
    parts.append(tongue)
    # criss-cross laces over the instep, sitting on the upper
    for k, z in enumerate((0.096, 0.122)):
        for s in (1, -1):
            pts = []
            for t in (0.0, 0.5, 1.0):
                px = x + s * (-0.034 + 0.068 * t)
                pz = z + 0.016 * t
                loc, nrm = on_surface(tree, (px, -0.4, pz), (0, 1, 0))
                if loc is None:
                    loc, nrm = V((px, -0.1, pz)), V((0, -1, 0))
                pts.append(loc + nrm * 0.006)
            parts.append(D.sweep(f"{name}_lace{k}{s}", pts, 0.0062, sides=5, color=c["lace"], mat="flat", project="box"))
    tab = blob(name + "_tab", (0, 0, 0), (0.022, 0.012, 0.03), segs=8, rings=4, color=c["heel"], mat=mat)
    D.put(tab, V((x, 0.093, 0.177)), V((0, 1, 0.2)), up=(0, 0, 1))
    parts.append(tab)
    return parts

def leg(name, hip, thigh_color, boot_color, sole_color, thigh_mat=FELT, boot_mat=FELT, toe=0.0, curl=0.0,
        stripes=None, extras=None, cuff=None, shoe=None):
    """stripes: (color, period) paints alternate bands on the thigh. extras(x) -> [objects] in world space.
    cuff: (color, mat) adds a turned-down cuff on the boot top. shoe: sneaker colours (dict, see
    SNEAKER) replaces the felt boot, sole and cuff with a high-top sneaker."""
    x = hip[0] * 1.06
    thigh = D.lathe(name + "_thigh", [(0.0, 0.12), (0.074, 0.125), (0.08, 0.17), (0.084, 0.24), (0.074, 0.305), (0.0, 0.335)],
                    10, color=thigh_color, mat=thigh_mat, project="cyl", loc=(x, 0, 0))
    if stripes:
        col, per = stripes
        K.paint(thigh, col, where=lambda c, n: 0.12 < c.z < 0.31 and int((c.z - 0.12) / per) % 2 == 1)
    if shoe is not None:
        parts = [thigh] + sneaker(name, x, shoe)
        if extras:
            parts += extras(x)
        return D.part(parts, name, hip)
    boot = D.lathe(name + "_boot", [(0.0, 0.03), (0.07, 0.03), (0.09, 0.05), (0.096, 0.083), (0.088, 0.115),
                                    (0.066, 0.142), (0.032, 0.158), (0.0, 0.162)],
                   14, color=boot_color, mat=boot_mat, project="sphere", warp=boot_warp_fn(toe, curl), loc=(x, 0, 0),
                   center=(0, 0, 0.09))
    sole = D.lathe(name + "_sole", [(0.0, 0.0), (0.086, 0.0), (0.099, 0.008), (0.102, 0.021), (0.096, 0.033), (0.0, 0.036)],
                   12, color=sole_color, mat="flat", project="cyl",
                   warp=lambda c: V((c.x * 1.02 * (1 - 0.35 * (toe > 0) * max(0.0, -c.y / 0.1) ** 2),
                                     c.y * 1.36 - 0.03 - toe * 0.8 * max(0.0, -c.y / 0.1) ** 2, c.z)), loc=(x, 0, 0))
    parts = [thigh, boot, sole]
    if cuff:
        parts.append(D.lathe(name + "_cuff", [(0.075, 0.118), (0.092, 0.122), (0.095, 0.142), (0.084, 0.162), (0.07, 0.16)],
                             12, color=cuff[0], mat=cuff[1], project="cyl", loc=(x, 0, 0)))
    if extras:
        parts += extras(x)
    return D.part(parts, name, hip)


# ------------------------------------------------------------------ arms (origin at shoulder)
def arm(name, sho, hand, color, mat=FELT, r0=0.066, taper=0.016, flare=0.012, sides=10, paint=None, extras=None,
        hand_color="skin", hand_mat="flat"):
    """Sleeve swept shoulder -> elbow -> wrist, flaring into a soft cuff; the kid's own skin hand pokes
    out (hand_color / hand_mat make it a mitten or glove with exactly the same shape, so the wand grip
    and the bucket grip don't move). paint(i, j) recolours sleeve faces; extras(pts, rr, d) -> [objects]."""
    s = V(sho)
    out = 1 if s.x > 0 else -1
    d = (hand - s).normalized()
    elbow = s.lerp(hand, 0.5) + V((0.018 * out, 0.01, 0.0))
    pts, par = catmull([s + V((-0.045 * out, 0, 0.012)), elbow, hand], per=3)
    rr = [r0 - taper * t + flare * max(0.0, (t - 0.8) / 0.2) for t in par]
    sleeve = D.sweep(name + "_sleeve", pts, rr, sides=sides, color=color, mat=mat, project="cyl", paint=paint)
    mc = hand + d * 0.04
    mitten = blob(name + "_hand", (0, 0, 0), (0.042, 0.04, 0.05), segs=10, rings=6, color=hand_color, mat=hand_mat)
    D.put(mitten, mc, d, up=(0, -1, 0))
    th = blob(name + "_thumb", (0, 0, 0), (0.016, 0.016, 0.026), segs=7, rings=4, color=hand_color, mat=hand_mat)
    D.put(th, mc + V((0.01 * out, -0.036, 0.018)), d + V((0, -0.6, 0)), up=(0, -1, 0))
    parts = [sleeve, mitten, th]
    if extras:
        parts += extras(pts, rr, d)
    return D.part(parts, name, sho)


# ------------------------------------------------------------------ head pieces
def opening(c, hc=HC, oa=OA, oc=OC, oz=OZ):
    return c.y < hc.y - 0.1 and (c.x / (oa - 0.012)) ** 2 + ((c.z - oz) / (oc - 0.012)) ** 2 < 1.0


def hood(color, rim_color=None, stitch_color="dragon_green_dark", mat=FELT, rim_mat=FELT, warp=None, segs=28,
         rings=15, rim_r=0.032, stitches=True, oa=OA, oc=OC, oz=OZ):
    """The dragon hood: an ellipsoid with the face opening cut out, a puffy rim and rim stitches.
    warp(world point) reshapes the shell away from the opening (ghost dome, etc.); oa/oc/oz resize the
    face opening (x radius, z radius, centre z)."""
    hd = blob("hood", HC, HR, segs=segs, rings=rings, color=color, mat=mat, warp=warp,
              cut=lambda c: opening(c, oa=oa, oc=oc, oz=oz))
    OA, OC, OZ = oa, oc, oz      # noqa: N806 (the rim below follows the chosen opening)
    rim_pts = []
    NR = 24
    for i in range(NR):
        a = 2 * math.pi * i / NR
        p, n = ellipsoid_pt(HC, HR, math.cos(a) * OA, OZ + math.sin(a) * OC)
        rim_pts.append(p - n * 0.004)
    rim = D.sweep("hood_rim", rim_pts, rim_r, sides=7, color=rim_color or color, mat=rim_mat, project="sphere",
                  closed=True, center=HC)
    parts = [hd, rim]
    if stitches:
        st = []
        for i in range(20):
            a = 2 * math.pi * (i + 0.5) / 20
            p, n = ellipsoid_pt(HC, HR, math.cos(a) * OA, OZ + math.sin(a) * OC)
            t = V((-math.sin(a) * OA, 0, math.cos(a) * OC))
            st.append((p - n * 0.004 + n * (rim_r - 0.001), t, n))
        parts.append(D.stitches("rim_st", st, length=0.018, width=0.006, height=0.003, color=stitch_color, sink=0.004))
    return parts


def face(fang=True, mouth="smile", fc=FC, fr=FR):
    """The kid's face: skin oval, big glossy ink eyes with a warm glint and two highlights, rosy cheeks
    and a happy open smile with a pink tongue (and optionally one tiny fang)."""
    parts = [blob("face", fc, fr, segs=18, rings=11, color="skin", mat="flat")]
    for sx in (1, -1):
        ex, ez = 0.076 * sx, 0.806
        p, n = ellipsoid_pt(fc, fr, ex, ez)
        roll = -7 * sx
        W, H, Dp = 0.035, 0.047, 0.016
        eye = blob(f"eye{sx}", (0, 0, 0), (W, H, Dp), segs=14, rings=8, color="ink", mat="flat")
        D.put(eye, p - n * 0.003, n, roll=roll)
        M = D.look(n, roll=roll)

        def on_eye(lx, ly, lift=0.0015):
            lz = Dp * math.sqrt(max(1 - (lx / W) ** 2 - (ly / H) ** 2, 0.0))
            return p - n * 0.003 + M @ V((lx, ly, lz)) + n * lift, (M @ V((lx / W ** 2, ly / H ** 2, lz / Dp ** 2))).normalized()

        q, qn = on_eye(0.0, -0.027, 0.0)
        glint = blob(f"glint{sx}", (0, 0, 0), (0.02, 0.011, 0.004), segs=8, rings=4, color="burnt_amber", mat="flat")
        D.put(glint, q, qn, roll=roll)
        q, qn = on_eye(-0.011, 0.016)
        hl = blob(f"hl{sx}", (0, 0, 0), (0.0145, 0.0165, 0.004), segs=10, rings=4, color="eye_white", mat="flat")
        D.put(hl, q, qn, roll=roll)
        q, qn = on_eye(0.013, -0.008)
        hl2 = blob(f"hl2{sx}", (0, 0, 0), (0.0068, 0.0068, 0.003), segs=8, rings=4, color="eye_white", mat="flat")
        D.put(hl2, q, qn, roll=roll)
        parts += [eye, glint, hl, hl2]
        p, n = ellipsoid_pt(fc, fr, 0.132 * sx, 0.752)
        ck = blob(f"cheek{sx}", (0, 0, 0), (0.031, 0.019, 0.006), segs=8, rings=4, color="cheek_pink", mat="flat")
        D.put(ck, p - n * 0.002, n, roll=-10 * sx)
        parts.append(ck)

    p, n = ellipsoid_pt(fc, fr, 0.0, 0.744)
    MW = 0.028

    def yb(x):
        u = x / MW
        return 0.007 * u * u - 0.018 * (1 - u * u)

    def yt(x):
        u = x / MW
        return 0.007 * u * u - 0.003 * (1 - u * u)

    if mouth == "oh":     # a round 'boo!' mouth
        ring = [(math.cos(2 * math.pi * i / 12) * 0.015, -0.009 + math.sin(2 * math.pi * i / 12) * 0.017) for i in range(12)]
        m = K.polygon_prism("mouth", ring, 0.008, color="ink", mat="flat")
        D.put(m, p - n * 0.004, n)
        ML = D.look(n)
        tg = blob("tongue", (0, 0, 0), (0.009, 0.006, 0.004), segs=8, rings=4, color="cheek_pink", mat="flat")
        D.put(tg, p + n * 0.0025 + ML @ V((0.0, -0.018, 0)), n)
        return parts + [m, tg]
    smile = [(-MW + 2 * MW * i / 10, yb(-MW + 2 * MW * i / 10)) for i in range(11)]
    smile += [(MW - 2 * MW * i / 8, yt(MW - 2 * MW * i / 8)) for i in range(1, 8)]
    m = K.polygon_prism("mouth", smile, 0.008, color="ink", mat="flat")
    D.put(m, p - n * 0.004, n)
    ML = D.look(n)
    tg = blob("tongue", (0, 0, 0), (0.012, 0.0062, 0.004), segs=8, rings=4, color="cheek_pink", mat="flat")
    D.put(tg, p + n * 0.0025 + ML @ V((0.002, -0.0105, 0)), n)
    parts += [m, tg]
    if fang:
        fg = K.polygon_prism("fang", [(-0.0045, 0.0), (0.0045, 0.0), (0.0, -0.008)], 0.004, color="eye_white", mat="flat")
        D.put(fg, p + n * 0.003 + ML @ V((0.011, yt(0.011) + 0.0005, 0)), n)
        parts.append(fg)
    return parts


def fringe(color="hair_brown", locks=True, z_off=0.0):
    """Brown hair swooping out from under a hood rim or hat brim: four forehead tufts + temple locks."""
    parts = []
    for i, (x, z, s, r) in enumerate([(-0.088, 0.9, 0.95, 34), (-0.028, 0.918, 1.2, 14), (0.038, 0.915, 1.1, -12),
                                      (0.1, 0.895, 0.9, -36)]):
        p, n = ellipsoid_pt(FC, FR, x, z + z_off)
        tuft = blob(f"fringe{i}", (0, 0, 0), (0.054 * s, 0.032 * s, 0.024), segs=8, rings=5, color=color, mat="flat")
        D.put(tuft, p + n * 0.006, n + V((0, 0, 0.35)), roll=r)
        parts.append(tuft)
    if locks:
        for sx in (1, -1):
            p, n = ellipsoid_pt(FC, FR, 0.158 * sx, 0.872 + z_off)
            lk = blob(f"lock{sx}", (0, 0, 0), (0.024, 0.044, 0.018), segs=7, rings=5, color=color, mat="flat")
            D.put(lk, p + n * 0.004, n, roll=18 * sx)
            parts.append(lk)
    return parts


HAIR_C, HAIR_R = V((0, 0.03, 0.835)), V((0.236, 0.226, 0.226))


def bare_head(hair="hair_brown", hair_warp=None):
    """No hood: the kid's own head. A rounded hair cap over the back and top (face cut out), small
    skin ears, plus the forehead fringe and temple locks."""
    def cut(c):
        return c.y < HAIR_C.y - 0.11 and (c.x / 0.168) ** 2 + ((c.z - 0.79) / 0.15) ** 2 < 1.0
    parts = [blob("hair", HAIR_C, HAIR_R, segs=22, rings=12, color=hair, mat="flat", cut=cut, warp=hair_warp)]
    for sx in (1, -1):
        ear = blob(f"ear{sx}", (0, 0, 0), (0.026, 0.036, 0.016), segs=8, rings=5, color="skin", mat="flat")
        D.put(ear, V((0.214 * sx, -0.035, 0.775)), V((sx, 0.25, 0)), roll=0)
        parts.append(ear)
    return parts + fringe(hair)


# ------------------------------------------------------------------ the candy bucket (exactly the dragon's)
def bucket():
    """Returns (bucket, bucket_candy, bucket_centre). bucket origin = grip (HAND_R - 3 cm), bucket_candy
    origin on the candy floor."""
    GRIP = HAND_R + V((0.0, 0.0, -0.03))
    BR, CUT, TH = 0.15, 0.064, 0.013
    rim_r = math.sqrt(BR ** 2 - CUT ** 2)
    BC = GRIP - V((0.0, 0.0, CUT + rim_r + 0.006))
    BOT = -0.128
    NLOBE = 5
    FLOOR_Z = CUT - 0.016

    def lobe_k(ang):
        return 1 + 0.055 * math.cos(NLOBE * (ang + math.pi / 2))

    prof = [(0.0, BOT)]
    for deg in (34, 60, 82, 100, 113.5):
        phi = math.radians(deg)
        prof.append((BR * math.sin(phi), max(-BR * math.cos(phi), BOT)))
    prof += [(rim_r - 0.002, CUT + 0.006), (rim_r - TH * 0.6, CUT + 0.009), (rim_r - TH, CUT + 0.002)]
    BRi = BR - TH
    for deg in (108, 85, 55):
        phi = math.radians(deg)
        prof.append((BRi * math.sin(phi), max(-BRi * math.cos(phi), BOT + TH)))
    prof.append((0.0, BOT + TH))

    def bucket_warp(c):
        k = lobe_k(math.atan2(c.y, c.x))
        return V((c.x * k, c.y * k, c.z))

    shell = D.lathe("bucket_shell", prof, 20, color="pumpkin", mat="pumpkin", project="cyl", warp=bucket_warp,
                    center=(0, 0, 0))

    def grin_outline():
        W = 0.084

        def zb(x):
            u = x / W
            return -0.044 - 0.042 * (1 - u * u) + 0.022 * u * u

        def zt(x):
            u = x / W
            return -0.044 - 0.008 * (1 - u * u) + 0.022 * u * u

        def tooth(xa, xb):
            return [(xa, zt(xa)), (xa, zt(xa) - 0.019), (xb, zt(xb) - 0.019), (xb, zt(xb))]

        out = [(-W + 2 * W * i / 10, zb(-W + 2 * W * i / 10)) for i in range(11)]
        out += [(0.065, zt(0.065)), (0.048, zt(0.048))]
        out += tooth(0.035, 0.016)
        out += [(0.0, zt(0.0))]
        out += tooth(-0.016, -0.035)
        out += [(-0.048, zt(-0.048)), (-0.065, zt(-0.065))]
        return out

    cutters = []

    def cutter(name, outline, cx, cz):
        ob = K.polygon_prism(name, [(x + cx, z + cz) for x, z in outline], 0.26, color="pumpkin")
        ob.rotation_euler = (math.radians(90), 0, 0)
        ob.location = (0, -0.04, 0)
        cutters.append(ob)

    eye = rounded_poly([(-0.034, -0.017), (0.034, -0.017), (0.004, 0.032)], 0.008)
    cutter("cut_eye_l", eye, 0.053, 0.016)
    cutter("cut_eye_r", [(-x, z) for x, z in reversed(eye)], -0.053, 0.016)
    cutter("cut_nose", rounded_poly([(-0.015, -0.009), (0.015, -0.009), (0.0, 0.014)], 0.004), 0.0, -0.018)
    cutter("cut_grin", grin_outline(), 0.0, 0.0)
    cut = K.merge(cutters, "cutter")
    mod = shell.modifiers.new("carve", "BOOLEAN")
    mod.operation = "DIFFERENCE"
    mod.solver = "EXACT"
    mod.object = cut
    dg = bpy.context.evaluated_depsgraph_get()
    carved = bpy.data.meshes.new_from_object(shell.evaluated_get(dg))
    shell.modifiers.clear()
    old = shell.data
    shell.data = carved
    bpy.data.meshes.remove(old)
    bpy.data.objects.remove(cut, do_unlink=True)

    bm = bmesh.new()
    bm.from_mesh(shell.data)
    bmesh.ops.dissolve_degenerate(bm, dist=2e-4, edges=bm.edges)
    bmesh.ops.triangulate(bm, faces=[f for f in bm.faces if len(f.verts) > 4])
    bm.normal_update()
    outer, walls, inner, lip = [], [], [], []
    for f in bm.faces:
        c = f.calc_center_median()
        nr = f.normal.dot(c.normalized()) if c.length > 1e-4 else -1.0
        if c.z > CUT - 0.002:
            lip.append(f)
        elif abs(nr) < 0.45 and c.y < -0.08 and -0.11 < c.z < 0.06:
            walls.append(f)
        elif (nr > 0 and c.length > BRi * 0.91) or (c.z < BOT + 0.004 and f.normal.z < -0.5):
            outer.append(f)
        else:
            inner.append(f)
    K._project_bm(bm, "cyl", outer + lip, center=V((0, 0, 0)))
    K._paint_bm(bm, "pumpkin", "pumpkin", outer)
    K._paint_bm(bm, "pumpkin_light", "pumpkin", lip)
    K._paint_bm(bm, "glow_jack", "flat", walls)
    K._paint_bm(bm, "glow_jack_hot", "flat", [f for f in inner if f.calc_center_median().z < FLOOR_Z])
    K._paint_bm(bm, "pumpkin_dark", "pumpkin", [f for f in inner if f.calc_center_median().z >= FLOOR_Z])
    for f in bm.faces:
        f.smooth = f not in walls
    bm.to_mesh(shell.data)
    bm.free()
    shell.location = BC

    bm = bmesh.new()
    fr_r = math.sqrt(BRi ** 2 - FLOOR_Z ** 2) * 0.985
    bm.faces.new([bm.verts.new((math.cos(a) * fr_r * lobe_k(a), math.sin(a) * fr_r * lobe_k(a), FLOOR_Z))
                  for a in (2 * math.pi * i / 20 for i in range(20))])
    floor = D.mesh(bm, "bucket_floor", "pumpkin_dark", "pumpkin", "box", smooth=False, loc=BC)

    hp = []
    HRAD = rim_r - 0.006
    for i in range(19):
        a = math.pi * i / 18
        hp.append(BC + V((math.cos(a) * HRAD, 0.0, CUT + 0.004 + math.sin(a) * (HRAD + 0.006))))
    handle = D.sweep("bucket_handle", hp, 0.0108, sides=6, color="rope", mat="flat", project="box",
                     warp=lambda a, i: 1.0 + 0.22 * math.cos(2 * a - i * 1.5))
    knots = []
    for s in (1, -1):
        kn = blob(f"bucket_knot{s}", (0, 0, 0), (0.021, 0.021, 0.017), segs=6, rings=4, color="rope", mat="flat")
        kn.location = BC + V((s * HRAD * lobe_k(0 if s > 0 else math.pi), 0, CUT + 0.006))
        knots.append(kn)
    bkt = D.part([shell, floor, handle] + knots, "bucket", GRIP)

    FL = BC + V((0, 0, FLOOR_Z))
    heap = []

    def mini_wrapped(name, c, col, yaw, tilt=0.0):
        ps = [blob(name, (0, 0, 0), (0.034, 0.024, 0.024), segs=7, rings=5, color=col, mat="flat")]
        for s in (1, -1):
            ps.append(K.cylinder(f"{name}_e{s}", 0.008, 0.026, color=col, mat="flat", verts=5, radius_top=0.021,
                                 loc=(s * 0.043, 0, 0), rot=(0, 90 * s, 0)))
        ob = K.merge(ps, name)
        ob.data.transform(Matrix.Rotation(math.radians(tilt), 4, "Y"))
        ob.data.transform(Matrix.Rotation(math.radians(yaw), 4, "Z"))
        ob.location = c
        heap.append(ob)

    ring_cols = ["candy_violet", "candy_cherry", "candy_lemon", "candy_green", "gumball_blue", "candy_orange"]
    for i, col in enumerate(ring_cols):
        a = math.radians(20 + 60 * i)
        r = 0.035 - 0.003 * (i % 2)
        heap.append(blob(f"gum{i}", FL + V((math.cos(a) * 0.078, math.sin(a) * 0.078, r * 0.75)), (r, r, r * 0.95),
                         segs=7, rings=5, color=col, mat="flat"))
    mini_wrapped("hw_pink", FL + V((0.0, -0.025, 0.045)), "candy_magenta", 15, -12)
    mini_wrapped("hw_mint", FL + V((-0.035, 0.03, 0.05)), "candy_orange", 70, 15)
    heap.append(K.cylinder("coin", 0.027, 0.009, color="foil_gold", mat="metal", verts=10,
                           loc=FL + V((0.04, -0.01, 0.06)), rot=(28, 12, 0)))
    heap.append(K.cylinder("heap_stick", 0.006, 0.1, color="lolly_white", mat="flat", verts=5,
                           loc=FL + V((0.02, 0.03, 0.06)), rot=(-18, 12, 0)))
    heap.append(K.cylinder("heap_lolly", 0.036, 0.016, color="candy_pink", mat="flat", verts=12,
                           loc=FL + V((0.031, 0.045, 0.112)), rot=(80, 0, 10)))
    heap.append(K.torus("heap_lolly_swirl", 0.02, 0.0065, color="lolly_white", mat="flat", major_seg=10, minor_seg=3,
                        loc=FL + V((0.031, 0.045 - 0.0085, 0.112)), rot=(80, 0, 10)))
    candy = D.part(heap, "bucket_candy", FL)
    return bkt, candy, BC


def tilt_head(head):
    """The dragon's curious little head tilt, baked into the mesh so the rest pose stays identity."""
    head.data.transform(Matrix.Rotation(math.radians(-5), 4, "Y") @ Matrix.Rotation(math.radians(-3), 4, "X"))


# ------------------------------------------------------------------ assembly
def finish(asset_id, parts, sheet=True):
    """parts: dict name -> object (body, head, leg_l, leg_r, arm_l, arm_r, optional wing_l/wing_r/tail).
    Builds the bucket under arm_r, parents everything like dragon_kid, bakes AO, exports and renders."""
    root = K.empty(asset_id, (0, 0, 0))
    bkt, candy, BC = bucket()
    order = ["body", "head", "wing_l", "wing_r", "tail", "leg_l", "leg_r", "arm_l", "arm_r"]
    for nm in order:
        if nm in parts:
            assert parts[nm].name == nm, (nm, parts[nm].name)
            D.attach(parts[nm], root)
    D.attach(bkt, parts["arm_r"])
    D.attach(candy, bkt)
    D.attach(K.empty("socket_bucket_light", tuple(BC)), bkt)
    counts = {}
    for nm in order + ["bucket", "bucket_candy"]:
        ob = parts.get(nm) or {"bucket": bkt, "bucket_candy": candy}.get(nm)
        if ob is not None:
            counts[nm] = D.tris(ob)
            print("PART", nm, counts[nm])
    total = K.tri_count([root])
    print("TRIS", asset_id, total)
    K.bake_ao([root], samples=40, radius=0.3, strength=0.8)
    K.export([root], f"characters/{asset_id}")
    if sheet:
        K.review_sheet([root], asset_id)
    return total, counts


# ====================================================================================================
#  KIT V2 (batch05, 2026-11 fidelity pass). Rig contract v2: art/team/characters_candy/batch05/
#  CHECKPOINT.md. Everything above is the v1 kit, kept so a costume can be ported piece by piece.
#
#    limb()        tube with ball ends (ball joints)
#    arm2()        arm_* (shoulder ball -> elbow ball) + forearm_* (elbow ball -> cuffed wrist)
#    grip_hand()   hand_*: palm, four chubby fingers and a thumb wrapped round a bar (handle / wand)
#    leg2()        leg_* (thigh -> knee ball) + shin_* (whatever shoe the costume passes in)
#    Surf          a drawing board on a surface: discs, bands and tubes that hug it (faces, decals)
#    face2()       face skin, nose, cheeks + the face parts: eye_*/iris_*/alt_lash_*/brow_*/mouths
#    fringe2()     swept bangs with a highlight streak + temple locks
#    torso2(), hood2()   smoother body and hood
#    soft_part(), bob_part()   costume parts the rig springs (one mesh, origin at the pin / pivot)
#    finish2()     bucket + sockets, head tilt for every head child, hierarchy, per-part AO, export
#    sheet_grid(), pose(), unpose()   posed review sheets (joint gaps, blink, mouths)
# ====================================================================================================
PLUSH = "plush"


def gd(x, y, z):
    """A Godot / glTF offset (x, y up, z front) as a Blender vector (x, -z, y)."""
    return V((x, -z, y))


ELBOW_L = V(SHO_L) + gd(0.058, -0.090, 0.005)
ELBOW_R = V(SHO_R) + gd(-0.066, -0.085, 0.033)
WRIST_L, WRIST_R = HAND_L.copy(), HAND_R.copy()        # = elbow + (.022,-.090,.025) / (-.029,-.085,.052)
KNEE_Z = 0.165
KNEE_L, KNEE_R = V((0.090, 0.0, KNEE_Z)), V((-0.090, 0.0, KNEE_Z))
BUCKET_GRIP = HAND_R + V((0.0, 0.0, -0.03))            # the bucket's origin: 3 cm under the right wrist
WAND_GRIP = V(SHO_L) + gd(0.096, -0.216, 0.036)        # where the v1 code held the wand (kept)
WAND_DIR = gd(0.05, -0.9, 0.45).normalized()           # the wand's +Z at rest: down and forward
HEAD_TILT = Matrix.Rotation(math.radians(-5), 4, "Y") @ Matrix.Rotation(math.radians(-3), 4, "X")

V2_PARENT = {"shin_l": "leg_l", "shin_r": "leg_r", "forearm_l": "arm_l", "forearm_r": "arm_r",
             "hand_l": "forearm_l", "hand_r": "forearm_r"}
V2_TOP = ["body", "head", "wing_l", "wing_r", "tail", "leg_l", "leg_r", "arm_l", "arm_r"]
FACE_NAMES = ["eye_l", "eye_r", "iris_l", "iris_r", "alt_lash_l", "alt_lash_r", "brow_l", "brow_r",
              "mouth_smile", "alt_mouth_open", "alt_mouth_o", "alt_mouth_grit"]


# ------------------------------------------------------------------ limbs with ball joints
def limb(name, pts, radii, sides=12, color="dragon_jade", mat=PLUSH, cap_a=True, cap_b=True, project="cyl",
         paint=None, up=None, warp=None, caps=(70, 44, 20)):
    """Tube through pts whose ends are half spheres of the end radius centred on the end points: put an
    end point on a joint and that end is the joint's ball. paint(i, j) counts rings from the first cap
    ring (a cap adds len(caps) + 1 rings)."""
    pts = [V(p) for p in pts]
    if isinstance(radii, (int, float)):
        radii = [radii] * len(pts)
    P, R = list(pts), list(radii)
    if cap_b:
        d = (pts[-1] - pts[-2]).normalized()
        for ang in reversed(caps):
            a = math.radians(ang)
            P.append(pts[-1] + d * radii[-1] * math.sin(a))
            R.append(radii[-1] * math.cos(a))
        P.append(pts[-1] + d * radii[-1])
        R.append(0.0)
    if cap_a:
        d = (pts[0] - pts[1]).normalized()
        hp, hr = [pts[0] + d * radii[0]], [0.0]
        for ang in caps:
            a = math.radians(ang)
            hp.append(pts[0] + d * radii[0] * math.sin(a))
            hr.append(radii[0] * math.cos(a))
        P, R = hp + P, hr + R
    return D.sweep(name, P, R, sides=sides, color=color, mat=mat, project=project, paint=paint, up=up, warp=warp)


def ring(name, center, axis, radius, tube, n=18, sides=8, color="dragon_jade", mat=PLUSH, squash=1.0):
    """A torus (rolled cuff, collar) round 'axis' at 'center'. squash < 1 flattens it along the axis."""
    c, a = V(center), V(axis).normalized()
    u = a.orthogonal().normalized()
    v = a.cross(u)
    pts = [c + (u * math.cos(t) + v * math.sin(t)) * radius for t in (2 * math.pi * i / n for i in range(n))]
    ob = D.sweep(name, pts, tube, sides=sides, color=color, mat=mat, project="cyl", closed=True, center=c)
    if squash != 1.0:
        for vert in ob.data.vertices:
            d = vert.co - c
            vert.co = c + d - a * d.dot(a) * (1.0 - squash)
    return ob


def arm2(name, sho, elbow, wrist, color, mat=PLUSH, r_sho=0.066, r_elbow=0.058, r_wrist=0.05, sides=14,
         cuff=None, cuff_tube=0.019, paint_upper=None, paint_fore=None, upper_extras=None, fore_extras=None):
    """Returns (arm, forearm): origins at the shoulder and the elbow. Both parts end in a ball centred
    on the joint they share (the forearm's a hair smaller, sunk into the upper arm's), so the elbow
    never opens. The sleeve stops 1.4 cm short of the wrist under a fat rolled cuff; the hand part
    (grip_hand) starts with a ball on the wrist inside it. cuff = (colour, material) or None. *_extras(points) -> [objects]."""
    s, e, w = V(sho), V(elbow), V(wrist)
    side = name[-2:]
    out = 1 if s.x > 0 else -1
    um = s.lerp(e, 0.5) + V((0.004 * out, 0.004, 0.0))
    upper_pts, par = catmull([s, um, e], per=3)
    upper = limb(name + "_upper", upper_pts, [lerp_list([r_sho, (r_sho + r_elbow) * 0.5 + 0.003, r_elbow], t) for t in par],
                 sides=sides, color=color, mat=mat, paint=paint_upper)
    d = (w - e).normalized()
    end = w - d * 0.014                    # the sleeve stops short of the wrist: the hand shows whole
    fore_pts, par = catmull([e, e.lerp(end, 0.5), end], per=3)
    fore = limb(name + "_fore", fore_pts, [lerp_list([r_elbow - 0.002, (r_elbow + r_wrist) * 0.5 + 0.002, r_wrist], t)
                                           for t in par], sides=sides, color=color, mat=mat, paint=paint_fore, cap_b=False)
    fparts = [fore]
    if cuff:
        fparts.append(ring(name + "_cuff", end - d * 0.012, d, r_wrist - 0.007, cuff_tube, n=18, sides=8, color=cuff[0],
                           mat=cuff[1], squash=1.4))
    uparts = [upper] + (upper_extras(upper_pts) if upper_extras else [])
    fparts += fore_extras(fore_pts) if fore_extras else []
    return D.part(uparts, name, s), D.part(fparts, "forearm" + side, e)


def grip_hand(name, wrist, grip, axis, palm_dir, bar_r, curl=1, thumb_end=1, color="skin", mat="flat", fat=1.0,
              finger_color=None, extras=None):
    """hand_*: a chubby fist closed round a bar (the bucket handle, the wand grip). Origin at the wrist.
      grip      a point on the bar's axis: the middle of the fist
      axis      the bar's direction; the four fingers sit side by side along it
      palm_dir  which side of the bar the palm is on (made perpendicular to the axis)
      curl      +1 / -1: which way round the bar the fingers go (seen along the axis)
      thumb_end +1 / -1: the index finger and thumb are at the +axis / -axis end of the fist
      fat       scales the fingers and palm (cat paws 1.4)
    extras(frame) -> [objects], frame = dict(grip, a, s, t, fw, fr, R0) for pads, bones, claws."""
    W, G = V(wrist), V(grip)
    a = V(axis).normalized()
    s = V(palm_dir)
    s = (s - a * s.dot(a)).normalized()
    t = a.cross(s) * curl
    fw, fr = 0.0172 * fat, 0.0102 * fat
    R0 = bar_r + fr * 0.9
    fc = finger_color or color
    parts = []
    pc = G + s * (bar_r + 0.018 * fat)
    palm = blob(name + "_palm", (0, 0, 0), (0.029 * fat, 0.021 * fat, 0.041 * fat), segs=12, rings=8, color=color, mat=mat)
    D.put(palm, pc, a, up=s)
    parts.append(palm)
    # the heel of the hand: from a ball on the wrist into the palm
    parts.append(limb(name + "_heel", [W, W.lerp(pc, 0.55), pc], [0.031 * fat ** 0.5, 0.03 * fat ** 0.5, 0.024 * fat],
                      sides=10, color=color, mat=mat, project="sphere"))
    arcs = (200, 212, 204, 186)
    rads = (1.0, 1.04, 1.0, 0.88)
    for i in range(4):
        off = a * (thumb_end * (1.5 - i) * fw)
        n = 8
        pts, rr = [], []
        for k in range(n + 1):
            u = k / n
            th = math.radians(-32 + (arcs[i] + 32) * u)
            knuckle = 0.0035 * fat * math.exp(-((math.degrees(th) - 78) / 34) ** 2)
            pts.append(G + off + (s * math.cos(th) + t * math.sin(th)) * (R0 + knuckle))
            rr.append(fr * rads[i] * (1.0 - 0.14 * u))
        parts.append(limb(f"{name}_f{i}", pts, rr, sides=7, color=fc, mat=mat, project="sphere", cap_a=False,
                          caps=(60, 28)))
    # thumb: the other way round, beside the index finger, slanting back over it
    n = 6
    pts, rr = [], []
    for k in range(n + 1):
        u = k / n
        th = math.radians(34 - 176 * u)
        off = a * (thumb_end * (2.2 * fw - 0.016 * fat * u))
        pts.append(G + off + (s * math.cos(th) + t * math.sin(th)) * (bar_r + 0.0108 * fat))
        rr.append(0.0112 * fat * (1.0 - 0.12 * u))
    parts.append(limb(name + "_thumb", pts, rr, sides=7, color=fc, mat=mat, project="sphere", cap_a=False, caps=(60, 28)))
    if extras:
        parts += extras(dict(grip=G, a=a, s=s, t=t, fw=fw, fr=fr, R0=R0, wrist=W, palm=pc))
    return D.part(parts, name, W)


THIGH_R, KNEE_R_BALL = 0.083, 0.079


def thigh2(name, x, color, mat=PLUSH, segs=16, stripes=None):
    """The thigh, from inside the torso down to a ball centred on the knee (world space, round x)."""
    prof = [(0.0, 0.338), (0.062, 0.326), (0.078, 0.292), (THIGH_R, 0.245), (THIGH_R - 0.001, 0.2)]
    for deg in (0, 22, 45, 66, 90):
        a = math.radians(deg)
        prof.append((KNEE_R_BALL * math.cos(a) if deg < 90 else 0.0, KNEE_Z - KNEE_R_BALL * math.sin(a)))
    prof.reverse()
    ob = D.lathe(name + "_thigh", prof, segs, color=color, mat=mat, project="cyl", loc=(x, 0, 0))
    if stripes:
        col, per = stripes
        K.paint(ob, col, where=lambda c, n: 0.12 < c.z < 0.31 and int((c.z - 0.12) / per) % 2 == 1)
    return ob


def leg2(name, hip, thigh_color, shin_parts, thigh_mat=PLUSH, stripes=None, thigh_extras=None):
    """Returns (leg, shin). leg_* is the thigh (origin at the hip) ending in a ball on the knee; shin_* is
    whatever the costume passes in shin_parts(x) -> [objects in world space] (a boot or the kit's
    sneaker, round the foot at x; its top must close over the knee at z = KNEE_Z) with its origin at
    the knee. The shin carries the whole shoe: the rig reads the sole from its bounds."""
    side = name[-2:]
    x = hip[0] * 1.06
    tparts = [thigh2(name, x, thigh_color, thigh_mat, stripes=stripes)]
    if thigh_extras:
        tparts += thigh_extras(x)
    return D.part(tparts, name, hip), D.part(shin_parts(x), "shin" + side, (x, 0.0, KNEE_Z))


# ------------------------------------------------------------------ drawing on a surface (faces, decals)
class Surf:
    """A drawing board on a surface. 2D coordinates (x to the kid's left for a board facing front, y up)
    round a centre point; heights are measured along the board's normal from the surface (plus an
    optional dome(x, y) -> height, e.g. an eyeball). Everything it makes is a world-space object."""

    def __init__(self, tree, origin, normal, up=(0, 0, 1), roll=0.0, dome=None):
        self.tree, self.o, self.n = tree, V(origin), V(normal).normalized()
        self.M = D.look(self.n, up, roll)
        self.dome = dome
        self._base = {}

    def axes(self):
        """(right, up, normal) as world vectors."""
        return self.M.col[0].copy(), self.M.col[1].copy(), self.M.col[2].copy()

    def base(self, x, y):
        key = (round(x, 5), round(y, 5))
        if key not in self._base:
            q = self.o + self.M @ V((x, y, 0.0))
            loc, _ = D.hit(self.tree, q + self.n * 0.5, -self.n, 1.5)
            self._base[key] = (loc - self.o).dot(self.n) if loc is not None else 0.0
        return self._base[key]

    def z(self, x, y):
        return self.base(x, y) + (self.dome(x, y) if self.dome else 0.0)

    def pt(self, x, y, lift=0.0):
        return self.o + self.M @ V((x, y, self.z(x, y) + lift))

    def disc(self, name, cx, cy, rx, ry, color, lift=0.0, puff=0.0, segs=18, rings=3, sink=0.002, mat="flat", rot=0.0,
             clip=None):
        """A pillowy elliptical decal: 'lift' above the surface, 'puff' higher in the middle, its edge
        tucked 'sink' under the surface. clip(x, y) -> (x, y) reshapes its outline (e.g. an iris cut
        off by the eye's upper lid)."""
        bm = bmesh.new()
        cr, sr = math.cos(math.radians(rot)), math.sin(math.radians(rot))

        def xy(f, j):
            a = 2 * math.pi * j / segs
            lx, ly = math.cos(a) * rx * f, math.sin(a) * ry * f
            p = cx + lx * cr - ly * sr, cy + lx * sr + ly * cr
            return clip(*p) if clip else p
        c = bm.verts.new(self.pt(cx, cy, lift + puff))
        prev = None
        for i in range(1, rings + 1):
            f = i / rings
            cur = [bm.verts.new(self.pt(*xy(f, j), lift + puff * (1 - f ** 2.5))) for j in range(segs)]
            for j in range(segs):
                j2 = (j + 1) % segs
                if prev is None:
                    bm.faces.new((c, cur[j], cur[j2]))
                else:
                    bm.faces.new((prev[j], cur[j], cur[j2], prev[j2]))
            prev = cur
        skirt = [bm.verts.new(self.pt(*xy(1.0, j), -sink)) for j in range(segs)]
        for j in range(segs):
            j2 = (j + 1) % segs
            bm.faces.new((prev[j], skirt[j], skirt[j2], prev[j2]))
        return D.mesh(bm, name, color, mat, "box", smooth=True)

    def band(self, name, rows, color, lift=0.0, puff=0.0, sink=0.002, mat="flat"):
        """A shape between a top and a bottom edge: rows [(x, y_top, y_bottom)] with x increasing
        (crescents, teeth, smiles). Pillowed by 'puff' along its middle, edges tucked under."""
        bm = bmesh.new()
        T, Mi, B, Ts, Bs = [], [], [], [], []
        for x, yt, yb in rows:
            yt = max(yt, yb + 0.0007)
            T.append(bm.verts.new(self.pt(x, yt, lift)))
            Mi.append(bm.verts.new(self.pt(x, (yt + yb) / 2, lift + puff)))
            B.append(bm.verts.new(self.pt(x, yb, lift)))
            Ts.append(bm.verts.new(self.pt(x, yt, -sink)))
            Bs.append(bm.verts.new(self.pt(x, yb, -sink)))
        for i in range(len(rows) - 1):
            bm.faces.new((Mi[i], Mi[i + 1], T[i + 1], T[i]))
            bm.faces.new((B[i], B[i + 1], Mi[i + 1], Mi[i]))
            bm.faces.new((T[i], T[i + 1], Ts[i + 1], Ts[i]))
            bm.faces.new((B[i + 1], B[i], Bs[i], Bs[i + 1]))
        bm.faces.new((T[0], Ts[0], Bs[0], B[0]))
        bm.faces.new((T[-1], B[-1], Bs[-1], Ts[-1]))
        if abs(puff) > 1e-5:            # (flat: the three end points are in line, no gap to close)
            bm.faces.new((T[0], B[0], Mi[0]))
            bm.faces.new((T[-1], Mi[-1], B[-1]))
        return D.mesh(bm, name, color, mat, "box", smooth=True)

    def tube(self, name, pts2, radii, color, lift=0.0, sides=6, mat="flat", closed=False):
        """A round stroke along 2D points, half sunk in the surface when lift = 0 (lashes, brows, lip
        lines). A radius of 0 at an end makes it a point."""
        return D.sweep(name, [self.pt(x, y, lift) for x, y in pts2], radii, sides=sides, color=color, mat=mat,
                       project="box", closed=closed)


def rebase(ob, origin, basis=None):
    """Re-express a world-space mesh object: origin at 'origin', axes 'basis' (3x3; default identity)."""
    M = Matrix.Translation(V(origin)) @ (basis.to_4x4() if basis is not None else Matrix.Identity(4))
    bpy.context.view_layer.update()
    ob.data.transform(M.inverted() @ ob.matrix_world)
    ob.matrix_world = M
    bpy.context.view_layer.update()
    return ob


def node_basis(right, up, fwd):
    """Blender axes for a node whose glTF / Godot local +X is 'right', +Y is 'up' and +Z is 'fwd'
    (Blender local X, Z and -Y)."""
    return Matrix((V(right), -V(fwd), V(up))).transposed()


# ------------------------------------------------------------------ face v2
EYE = dict(W=0.046, H=0.058, D=0.006, x=0.079, z=0.806, roll=7.0)
MOUTH_Z = 0.737


def eye2(sx, tree, fc=FC, fr=FR, p=None, skin="skin", lash="ink", brow="hair_dark"):
    """One eye's parts, in world space on the untilted head: [(name, object, parent)].
      eye_<s>       a dark-brown base (the iris's darker rim; NO white sclera anywhere, decision 13), a
                    thick lash line over the top only (two outer flicks, no outline underneath), two
                    white catchlights. Origin at the eye centre on the
                    face; node +Y = eye up, +Z = eye normal.
      iris_<s>      big deep-brown iris clipped under the lash line (iris_brown, a cinnamon glow low
                    down, the amber crescent, choco_dark shade under the lid), a moderate ink pupil.
                    Same origin and axes; the rig slides it (about +-5 mm stays under lash / in the white).
      alt_lash_<s>  the shut eye: only a bold happy ^ lash arc with a flick (no lid patch: the face skin is
                    whole under the eye). Translation-only; the rig hides eye_<s> while it shows.
      brow_<s>      origin at the brow's middle. Translation-only."""
    p = dict(EYE, **(p or {}))
    s = "l" if sx > 0 else "r"
    W, H, Dp = p["W"], p["H"], p["D"]
    c, n = ellipsoid_pt(fc, fr, p["x"] * sx, p["z"])
    roll = -p["roll"] * sx
    # a parabolic dome (not a half-ellipse): no steep wall at the edge, which caught the key light as a
    # pale arc just inside the lash line in the engine's face close-up
    S = Surf(tree, c, n, roll=roll, dome=lambda x, y: Dp * max(1 - (x / W) ** 2 - (y / H) ** 2, 0.0))
    F = Surf(tree, c, n, roll=roll)
    # No white anywhere round the eye (FIDELITY_PASS decision 13): the eye's base disc is the iris's own
    # darker rim colour, so the brown iris sits in a dark ring that meets the lash line and the skin.
    eye = [S.disc("sclera", 0, 0, W, H, "choco_dark", segs=24, rings=3, sink=0.0012)]
    # The lash line: a thick dark stroke over the top of the eye only (the mock's eyes have no drawn
    # outline underneath, so the white never reads as a goggle rim), tapering out at the corners.
    N = 22
    rim_pts, rim_r = [], []
    for k in range(N + 1):
        a = math.radians(-14 + 208 * k / N)
        rim_pts.append((math.cos(a) * W * 1.01, math.sin(a) * H * 1.01))
        top_k = max(0.0, math.sin(a) + 0.24) / 1.24
        rim_r.append(0.0046 * top_k ** 0.55 if 0 < k < N else 0.0)
    # A flat ribbon lying on the (domed) eye, not a round tube: a round stroke catches the light along
    # its top and reads as a light line inside the lash line (seen in the engine's face close-up).
    bm = bmesh.new()
    rows = []
    for k in range(N + 1):
        a = math.radians(-14 + 208 * k / N)
        r = rim_r[k]
        nx, ny = math.cos(a) / W, math.sin(a) / H
        nl = math.hypot(nx, ny)
        nx, ny = nx / nl, ny / nl
        x0, y0 = rim_pts[k]
        rows.append([bm.verts.new(S.pt(x0 + nx * r * dx, y0 + ny * r * dx, lift)) for dx, lift in
                     ((0.7, -0.0008), (0.7, 0.0007), (0.0, 0.0011), (-1.3, 0.0007), (-1.3, -0.0008))])
    for k in range(N):
        for j in range(4):
            bm.faces.new((rows[k][j], rows[k + 1][j], rows[k + 1][j + 1], rows[k][j + 1]))
    bm.normal_update()
    bm.faces.ensure_lookup_table()
    if bm.faces[(N // 2) * 4 + 2].normal.dot(n) < 0:      # a face on top of the ribbon, middle of the arc
        bmesh.ops.reverse_faces(bm, faces=bm.faces)
    eye.append(D.mesh(bm, "rim", lash, "flat", "box", smooth=True))
    for k, (deg, dx1, dy1, dx2, dy2, r0) in enumerate([(40, 0.008, 0.0045, 0.0155, 0.0125, 0.0034),
                                                         (18, 0.008, 0.001, 0.0135, 0.0048, 0.0026)]):
        a = math.radians(deg)
        x0, y0 = math.cos(a) * W, math.sin(a) * H
        eye.append(F.tube(f"flick{k}", [(sx * x0, y0), (sx * (x0 + dx1), y0 + dy1), (sx * (x0 + dx2), y0 + dy2)],
                          [r0, r0 * 0.8, 0.0], lash, lift=0.0004, sides=5))
    # catchlights: fixed on the eye, both eyes lit from the same side (up and to the viewer's left)
    eye.append(S.disc("hl", -0.0135, 0.0185, 0.0128, 0.0148, "eye_white", lift=0.0058, puff=0.0012, segs=16, rings=2,
                      sink=0.0006))
    eye.append(S.disc("hl2", 0.0165, -0.0155, 0.0066, 0.0066, "eye_white", lift=0.0058, puff=0.0008, segs=12, rings=2,
                      sink=0.0006))
    # The iris: big and warm brown, filling the eye so the white shows only as slivers at the sides; its
    # top and bottom follow the eye's own outline (clipped), so only the sides show white. Soft gradient
    # in four layers: dark under the lid, warm brown, a lighter choco glow low down and the burnt-amber
    # crescent at the bottom; a moderate pupil.
    def lid_clip(x, y, k=0.975):
        e = math.sqrt((x / (W * k)) ** 2 + (y / (H * k)) ** 2)
        return (x / e, y / e) if e > 1.0 else (x, y)
    IRX, IRY, ICY = 0.0425, 0.0515, -0.0015
    iris = [S.disc("iris", 0, ICY, IRX, H * 1.1, "iris_brown", lift=0.0022, puff=0.0005, segs=24, rings=3, sink=0.0006,
                   clip=lid_clip),
            S.disc("iris_glow", 0, ICY - 0.0165, IRX * 0.8, IRY * 0.6, "choco", lift=0.0026, puff=0.0005, segs=20, rings=2,
                   sink=0.0005)]
    rows = []
    for i in range(13):
        x = -IRX * 0.74 + IRX * 1.48 * i / 12
        yb = ICY - IRY * 0.93 * math.sqrt(max(1 - (x / (IRX * 0.93)) ** 2, 0.0))
        rows.append((x, yb + 0.003 + 0.0095 * (1 - (x / (IRX * 0.74)) ** 2), yb))
    iris.append(S.band("glint", rows, "burnt_amber", lift=0.0031, puff=0.0006, sink=0.0005))
    top = []
    for i in range(13):
        x = -IRX * 0.92 + IRX * 1.84 * i / 12
        yt = H * 0.955 * math.sqrt(max(1 - (x / (W * 0.955)) ** 2, 0.0))
        top.append((x, yt, yt - 0.0045 - 0.0085 * (1 - (x / (IRX * 0.92)) ** 2)))
    iris.append(S.band("shade", top, "choco_dark", lift=0.0029, puff=0.0004, sink=0.0005))
    iris.append(S.disc("pupil", 0, ICY + 0.003, 0.0195, 0.0245, "ink", lift=0.0036, puff=0.0004, segs=20, rings=2,
                       sink=0.0006))
    xr, up, nn = S.axes()
    basis = node_basis(xr, up, nn)
    eye_ob = rebase(K.merge(eye, "eye_" + s), c, basis)
    iris_ob = rebase(K.merge(iris, "iris_" + s), c, basis)
    # the shut eye
    # The shut eye: the eye is hidden while this shows, and the face skin under it is whole (and its AO
    # ignores the eyes), so there is no lid at all - no oval bulge - only a bold, happy upturned lash
    # arc (^ ^) with a flick at the outer corner, thick enough to read at showcase distance.
    L = Surf(tree, c, n, roll=roll)
    lid = []
    arc, arc_r = [], []
    for i in range(15):
        u = -1 + 2 * i / 14
        arc.append((u * 0.044, -0.008 + 0.019 * (1 - u * u) ** 0.85))
        arc_r.append(0.0056 * (1 - abs(u) ** 2.4) ** 0.6 if abs(u) < 1 else 0.0)
    lid.append(L.tube("lash", arc, arc_r, lash, lift=0.0006, sides=7))
    lid.append(L.tube("lash_f", [(sx * 0.037, -0.0015), (sx * 0.047, 0.0035), (sx * 0.056, 0.0045)], [0.0034, 0.0024, 0.0],
                      lash, lift=0.0006, sides=5))
    lash_ob = rebase(K.merge(lid, "alt_lash_" + s), c)
    # brow
    bx = [(-0.034, 0.0), (-0.02, 0.0042), (-0.004, 0.0054), (0.012, 0.0054), (0.026, 0.0044), (0.038, 0.0)]
    bpts = [(sx * x, H + 0.0105 + 0.0085 * (1 - ((x - 0.004) / 0.04) ** 2)) for x, _ in bx]
    brow_ob = K.merge([F.tube("brow", bpts, [r for _, r in bx], brow, lift=0.0008, sides=6)], "brow_" + s)
    rebase(brow_ob, F.pt(sx * 0.004, H + 0.019))
    return [("eye_" + s, eye_ob, "head"), ("iris_" + s, iris_ob, "eye_" + s), ("alt_lash_" + s, lash_ob, "head"),
            ("brow_" + s, brow_ob, "head")]


def mouths2(tree, fc=FC, fr=FR, z=MOUTH_Z, fang=True, w=0.036, line="ink"):
    """The four mouth shapes, sharing one origin (the mouth centre on the face), translation-only:
    mouth_smile (shown), alt_mouth_open (teeth band, tongue), alt_mouth_o, alt_mouth_grit."""
    pm, nm = ellipsoid_pt(fc, fr, 0.0, z)
    S = Surf(tree, pm, nm)

    def xs(n, half):
        return [-half + 2 * half * i / (n - 1) for i in range(n)]

    def lip(name, f, half, r=0.0017, ticks=0.0045, n=13):
        pts = [(-half - 0.0045, f(-1.0) + ticks)] + [(x, f(x / half)) for x in xs(n, half)] + [(half + 0.0045, f(1.0) + ticks)]
        rr = [0.0] + [r * (0.55 + 0.45 * (1 - abs(x / half) ** 2)) for x in xs(n, half)] + [0.0]
        return S.tube(name, pts, rr, line, lift=0.0006, sides=5)
    out = []
    # smile: an open crescent, corners up, tongue and (dragon) one tiny fang
    yb = lambda u: 0.009 * u * u - 0.0215 * (1 - u * u)     # noqa: E731
    yt = lambda u: 0.009 * u * u - 0.0042 * (1 - u * u)     # noqa: E731
    ps = [S.band("m", [(x, yt(x / w), yb(x / w)) for x in xs(13, w)], "mouth_dark", lift=0.0006, sink=0.002),
          S.disc("tongue", 0.002, -0.0142, 0.0155, 0.0066, "tongue", lift=0.0013, puff=0.0012, segs=12, rings=2, sink=0.0006),
          lip("lip", yt, w)]
    if fang:
        fy = yt(0.0125 / w) - 0.0004
        ps.append(S.band("fang", [(0.0085, fy, fy - 0.0012), (0.0125, fy, fy - 0.0088), (0.0165, fy, fy - 0.0012)], "eye_white",
                         lift=0.002, sink=0.0006))
    out.append(("mouth_smile", ps))
    # open: a big happy D, upper teeth, tongue
    wo = 0.038
    yto = lambda u: 0.0025 + 0.0065 * u * u                   # noqa: E731
    ybo = lambda u: yto(u) - 0.036 * max(1 - u * u, 0.0) ** 0.55   # noqa: E731
    ps = [S.band("m", [(x, yto(x / wo), ybo(x / wo)) for x in xs(15, wo)], "mouth_dark", lift=0.0006, sink=0.002),
          S.band("teeth", [(x, yto(x / wo) - 0.0006, yto(x / wo) - 0.0092 * (1 - (x / 0.0325) ** 4)) for x in xs(11, 0.031)],
                 "eye_white", lift=0.0016, puff=0.0008, sink=0.0006),
          S.disc("tongue", 0.0, ybo(0.0) + 0.0118, 0.0195, 0.0105, "tongue", lift=0.0014, puff=0.0016, segs=14, rings=2,
                 sink=0.0006),
          lip("lip", yto, wo)]
    out.append(("alt_mouth_open", ps))
    # o: a small round 'ooh'
    ring_pts = [(math.cos(a) * 0.0135, -0.008 + math.sin(a) * 0.0165) for a in (2 * math.pi * i / 16 for i in range(16))]
    ps = [S.disc("m", 0, -0.008, 0.0135, 0.0165, "mouth_dark", lift=0.0006, segs=16, rings=2, sink=0.002),
          S.disc("tongue", 0, -0.0175, 0.0078, 0.0045, "tongue", lift=0.0013, puff=0.0008, segs=10, rings=2, sink=0.0006),
          S.tube("lip", ring_pts, 0.0015, line, lift=0.0006, sides=5, closed=True)]
    out.append(("alt_mouth_o", ps))
    # grit: clenched teeth in a wide flat mouth, corners down
    wg = 0.034
    ytg = lambda u: 0.004 - 0.0045 * u * u                    # noqa: E731
    ybg = lambda u: ytg(u) - 0.0165 * max(1 - u ** 6, 0.0) ** 0.5 - 0.0012   # noqa: E731
    ps = [S.band("m", [(x, ytg(x / wg), ybg(x / wg)) for x in xs(15, wg)], "mouth_dark", lift=0.0006, sink=0.002),
          S.band("teeth", [(x, ytg(x / wg) - 0.0022, ybg(x / wg) + 0.0022) for x in xs(11, 0.029)], "eye_white", lift=0.0016,
                 puff=0.0006, sink=0.0006)]
    for x in (-0.0195, -0.0065, 0.0065, 0.0195):
        ps.append(S.tube(f"gap{x}", [(x, ytg(x / wg) - 0.0026), (x, ybg(x / wg) + 0.0026)], 0.0008, "mouth_dark", lift=0.0024,
                         sides=4))
    ps.append(S.tube("bite", [(x, (ytg(x / wg) + ybg(x / wg)) / 2) for x in xs(7, 0.028)], 0.0008, "mouth_dark", lift=0.0024,
                     sides=4))
    edge = [(x, ytg(x / wg)) for x in xs(11, wg)] + [(x, ybg(x / wg)) for x in reversed(xs(11, wg))][1:-1]
    ps.append(S.tube("lip", edge, 0.0015, line, lift=0.0006, sides=5, closed=True))
    out.append(("alt_mouth_grit", ps))
    return [(name, rebase(K.merge(ps, name), pm), "head") for name, ps in out]


def face2(fang=True, fc=FC, fr=FR, skin="skin", eye=None, segs=28, rings=16, nose="skin"):
    """The kid's face v2 (same identity as face(): big dark eyes with an amber glint and two white
    highlights, rosy cheeks, a happy open smile). Returns (static, children):
      static    [objects] for the head mesh: face skin, nose bump, soft two-tone cheeks
      children  [(name, object, parent)]: eye_*, iris_*, alt_lash_*, brow_*, mouth_smile, alt_mouth_*
    Everything is in world space on the UNTILTED head; finish2() tilts the head and its children."""
    skin_ob = blob("face", fc, fr, segs=segs, rings=rings, color=skin, mat="flat")
    tree = D.bvh(skin_ob)
    static = [skin_ob]
    p, n = ellipsoid_pt(fc, fr, 0.0, 0.774)
    # a small soft button nose in the skin's own colour: it reads by its shading, not as a coloured dot
    static.append(Surf(tree, p, n).disc("nose", 0, 0, 0.0125, 0.0092, nose, puff=0.0048, segs=14, rings=4, sink=0.001))
    for sx in (1, -1):
        p, n = ellipsoid_pt(fc, fr, 0.13 * sx, 0.742)
        C = Surf(tree, p, n, roll=-10 * sx)
        static.append(C.disc(f"cheek{sx}", 0, 0, 0.034, 0.0215, "cheek_soft", lift=0.0005, puff=0.0003, segs=16, rings=2,
                             sink=0.0012))
        static.append(C.disc(f"cheek_in{sx}", 0, 0, 0.022, 0.013, "cheek_pink", lift=0.0011, puff=0.0003, segs=14, rings=2,
                             sink=0.0006))
    children = []
    for sx in (1, -1):
        children += eye2(sx, tree, fc, fr, eye, skin=skin)
    children += mouths2(tree, fc, fr, fang=fang)
    return static, children


def hood_clip(margin=0.93, oa=OA, oc=OC, oz=OZ):
    """clip(point) for hair under the dragon-style hood: anything near or beyond the face opening's rim
    is pulled inside the hood shell, so no lock pokes out through the plush."""
    def f(p):
        if (p.x / oa) ** 2 + ((p.z - oz) / oc) ** 2 < 0.7 and p.y < HC.y:
            return p
        d = p - HC
        e = math.sqrt((d.x / HR.x) ** 2 + (d.y / HR.y) ** 2 + (d.z / HR.z) ** 2)
        return HC + d * (margin / e) if e > margin else p
    return f


def fringe2(color="hair_brown", light="hair_light", n=7, z_top=0.945, part=2.5, fc=FC, fr=FR, z_off=0.0, locks=True,
            length=0.05, clip=None):
    """Hair showing under a hood rim or hat brim: n flat leaf-shaped bangs lapping over each other,
    combed away from a side parting (between bang 'part' - 0.5 and 'part' + 0.5) with their tips
    flicking out, every other one longer, lifted over its neighbours and streaked lighter,
    and a curved lock at each temple. clip(point) -> point keeps it under the costume (hood_clip())."""
    parts = []
    flat = lambda a, i: 1.0 / math.sqrt((math.cos(a) / 0.3) ** 2 + math.sin(a) ** 2)   # noqa: E731
    for i in range(n):
        u = i / (n - 1)
        x0 = -0.086 + 0.172 * u
        side = -1 if i < part else 1
        away = abs(i - part)
        ln = length + 0.012 * math.sin(math.pi * u) + (0.009 if i % 2 else 0.0)
        dx = side * (0.026 + 0.012 * away)
        ctrl, n0 = [], None
        for t in (0.0, 0.3, 0.62, 0.86, 1.0):
            p, nn = ellipsoid_pt(fc, fr, x0 + dx * t ** 1.6, z_top - ln * t + z_off)
            n0 = n0 or nn
            ctrl.append(p + nn * (0.007 + 0.002 * t + 0.006 * max(t - 0.7, 0.0) + (0.004 if i % 2 else 0.0)))
        pts, par = catmull(ctrl, per=2)
        hw = 0.031 + 0.004 * math.sin(math.pi * u)
        rr = [hw * (0.72 + 0.28 * math.sin(math.pi * min(t * 1.6, 1.0) * 0.5)) * max(1 - t ** 2.2, 0.0) ** 0.62 for t in par]
        rr[-1] = 0.0
        last = len(pts) - 2
        streak = i % 2 == 1                 # the lifted bangs catch the light
        parts.append(D.sweep(f"bang{i}", pts, rr, sides=12, color=color, mat="flat", project="box", up=n0, warp=flat,
                             paint=lambda r, j, last=last, streak=streak: (light, "flat") if (streak and j == 0 and 2 <= r < last)
                             else None))
    if locks:
        for sx in (1, -1):
            ctrl = []
            for t, x, z in ((0.0, 0.132, 0.918), (0.4, 0.152, 0.884), (0.75, 0.158, 0.85), (1.0, 0.148, 0.822)):
                p, nn = ellipsoid_pt(fc, fr, x * sx, z + z_off)
                ctrl.append(p + nn * (0.007 - 0.002 * t))
            pts, par = catmull(ctrl, per=2)
            rr = [0.021 * max(1 - t ** 2.2, 0.0) ** 0.6 for t in par]
            rr[-1] = 0.0
            parts.append(D.sweep(f"lock{sx}", pts, rr, sides=8, color=color, mat="flat", project="box", up=(0, -1, 0),
                                 warp=flat, paint=lambda r, j: (light, "flat") if (j == 0 and 1 <= r < 5) else None))
    if clip:
        for ob in parts:
            for v in ob.data.vertices:
                v.co = clip(v.co.copy())
    return parts


# ------------------------------------------------------------------ smoother body and hood
def torso2(color, mat=PLUSH, name="torso", prof=None, warp=None, segs=32, per=2):
    """torso() with the profile resampled (per extra rings per span) and more segments."""
    prof = prof or TORSO_PROF
    pts, _ = catmull([(r, z, 0.0) for r, z in prof], per=per)
    fine = [(max(p.x, 0.0), p.y) for p in pts]
    fine[0], fine[-1] = prof[0], prof[-1]
    return D.lathe(name, fine, segs, color=color, mat=mat, project="sphere", warp=warp or torso_warp, center=(0, 0, 0.45))


def hood2(color, rim_color=None, stitch_color="dragon_green_dark", mat=PLUSH, rim_mat=PLUSH, warp=None, segs=40,
          rings=24, rim_r=0.032, stitches=24, oa=OA, oc=OC, oz=OZ, rim_n=36, rim_sides=9):
    """hood() at about 40 x 24 with a rounder rim. Returns [hood, rim(, stitches)]."""
    hd = blob("hood", HC, HR, segs=segs, rings=rings, color=color, mat=mat, warp=warp,
              cut=lambda c: opening(c, oa=oa, oc=oc, oz=oz))
    rim_pts = []
    for i in range(rim_n):
        a = 2 * math.pi * i / rim_n
        p, n = ellipsoid_pt(HC, HR, math.cos(a) * oa, oz + math.sin(a) * oc)
        rim_pts.append(p - n * 0.004)
    parts = [hd, D.sweep("hood_rim", rim_pts, rim_r, sides=rim_sides, color=rim_color or color, mat=rim_mat,
                         project="sphere", closed=True, center=HC)]
    if stitches:
        st = []
        for i in range(stitches):
            a = 2 * math.pi * (i + 0.5) / stitches
            p, n = ellipsoid_pt(HC, HR, math.cos(a) * oa, oz + math.sin(a) * oc)
            st.append((p - n * 0.004 + n * (rim_r - 0.001), V((-math.sin(a) * oa, 0, math.cos(a) * oc)), n))
        parts.append(D.stitches("rim_st", st, length=0.017, width=0.0055, height=0.003, color=stitch_color, sink=0.004))
    return parts


# ------------------------------------------------------------------ parts the rig springs
def soft_part(name, objs, pin):
    """soft_<word>[_x]: ONE continuous mesh bent in the shader, origin at the pin (where it is attached).
    Model it so the distance from the pin grows along the part. The word picks the spring preset
    (hair, cape, skirt, sheet, cowl, ear, tip, stem, cord, vine)."""
    assert name.startswith("soft_"), name
    return D.part(list(objs), name, pin)


def bob_part(name, objs, pivot):
    """bob_<word>[_x]: a rigid part swung about its origin by a spring (bell, hat, bow, leaf)."""
    assert name.startswith("bob_"), name
    return D.part(list(objs), name, pivot)


# ------------------------------------------------------------------ assembly v2
def attach2(child, parent):
    """Parent without a parent-inverse, keeping the child's world transform (rotated nodes too)."""
    bpy.context.view_layer.update()
    local = parent.matrix_world.inverted() @ child.matrix_world
    child.parent = parent
    child.matrix_parent_inverse = Matrix.Identity(4)
    if _is_rotated(local):
        child.matrix_basis = local
    else:
        child.matrix_basis = Matrix.Translation(local.translation)
    bpy.context.view_layer.update()
    return child


def _is_rotated(m):
    q = m.to_quaternion()
    return abs(q.w) < 1.0 - 1e-7


def tilt_head2(head, children):
    """The curious little head tilt, baked into the head mesh (rest pose stays identity) and carried
    by every head child: translation-only children get their mesh turned and their origin moved,
    nodes with their own axes (eyes, irises) are turned whole."""
    neck = head.matrix_world.translation.copy()
    T = Matrix.Translation(neck) @ HEAD_TILT @ Matrix.Translation(-neck)
    head.data.transform(HEAD_TILT)
    for ob in children:
        bpy.context.view_layer.update()
        mw = ob.matrix_world.copy()
        if _is_rotated(mw):
            ob.matrix_world = T @ mw
        else:
            if ob.type == "MESH":
                ob.data.transform(HEAD_TILT)
            ob.matrix_world = Matrix.Translation(T @ mw.translation)
    bpy.context.view_layer.update()


def _ao_ignore(name, parent_of):
    """Names of the parts that must not shade 'name' in the bake: the parts it moves against. A limb
    ignores the parts it is jointed to (the ball of one sits inside the other); the torso ignores the
    limbs; face parts ignore each other."""
    side = name[-2:] if name.endswith(("_l", "_r")) else ""
    base = name[:-2] if side else name
    if name in FACE_NAMES or name.startswith("alt_"):
        return set(FACE_NAMES) - {name}
    table = {"arm": ["body", "head", "forearm" + side], "forearm": ["arm" + side, "hand" + side], "hand": ["forearm" + side, "bucket"],
             "leg": ["body", "shin" + side], "shin": ["leg" + side], "bucket": ["hand_r", "forearm_r"],
             "body": ["arm_l", "arm_r", "forearm_l", "forearm_r", "hand_l", "hand_r", "leg_l", "leg_r", "shin_l", "shin_r",
                      "bucket", "bucket_candy"]}
    out = set(table.get(base, []))
    if name.startswith(("soft_", "bob_")) and name in parent_of:
        out.add(parent_of[name])
    return out | {n for n in FACE_NAMES} | set(AO_IGNORE.get(name, ()))


AO_IGNORE = {}    # part name -> extra part names it must not be shaded by (a costume's layered cloth; set per build)
POST_ATTACH = []  # fn(names: part name -> object), run by finish2 once everything is attached (set per build)


AO_SHAPE = {}     # part name -> fn(world point) -> multiplier on its baked AO (soft fold shading on cloth)
AO_POST = {}      # part name -> fn(object, values) -> values: a last pass over its baked AO (set per build)


def bake_ao2(root, parent_of, samples=64, radius=0.3, strength=0.8, blur=2):
    """K.bake_ao per part, each against its own set of occluders (see _ao_ignore), so a joint ball that
    sits inside its neighbour, a blinking eye or a stack of alternate mouths never bakes in shadows
    that show when the part moves."""
    bpy.context.view_layer.update()
    meshes = [o for o in root.children_recursive if o.type == "MESH"]
    geo = {}
    for o in meshes:
        mw = o.matrix_world
        geo[o.name] = ([mw @ v.co for v in o.data.vertices], [list(p.vertices) for p in o.data.polygons])
    dirs = K._hemisphere(samples)
    for o in meshes:
        skip = _ao_ignore(o.name, parent_of)
        verts, polys = [], []
        for nm, (vs, ps) in geo.items():
            if nm in skip and nm != o.name:
                continue
            base = len(verts)
            verts += vs
            polys += [[base + i for i in p] for p in ps]
        bvh = BVHTree.FromPolygons(verts, polys)
        mw = o.matrix_world
        nmat = mw.to_3x3().inverted().transposed()
        me = o.data
        attr = me.color_attributes.get("AO") or me.color_attributes.new("AO", "FLOAT_COLOR", "POINT")
        vals = []
        for v in me.vertices:
            p = mw @ v.co
            n = (nmat @ v.normal).normalized()
            t = n.orthogonal().normalized()
            b = n.cross(t)
            occ = 0.0
            for d in dirs:
                w = t * d.x + b * d.y + n * d.z
                if w.z < -1e-4 and (-p.z / w.z) <= radius and p.z >= -0.01:
                    occ += 1.0
                    continue
                hit = bvh.ray_cast(p + n * 0.006, w, radius)
                if hit[0] is not None:
                    occ += 1.0 - (hit[3] / radius) * 0.5
            vals.append(1.0 - strength * occ / len(dirs))
        # per-vertex AO on a coarse mesh is blotchy: average it with its neighbours a couple of times
        nbr = [[] for _ in vals]
        for e in me.edges:
            a, b2 = e.vertices
            nbr[a].append(b2)
            nbr[b2].append(a)
        for _ in range(blur):
            vals = [0.5 * x + 0.5 * (sum(vals[j] for j in nbr[i]) / len(nbr[i]) if nbr[i] else x) for i, x in enumerate(vals)]
        if o.name in AO_SHAPE:                              # painted-in soft shading (cloth folds)
            fn = AO_SHAPE[o.name]
            vals = [x * fn(mw @ v.co) for x, v in zip(vals, me.vertices)]
        if o.name in AO_POST:                               # a costume's own pass over the baked values
            vals = AO_POST[o.name](o, vals)
        if o.name.startswith(("eye_", "iris_")):
            vals = [0.88 + 0.12 * x for x in vals]          # glossy toy eyes stay bright
        elif o.name in FACE_NAMES:
            vals = [0.6 + 0.4 * x for x in vals]
        for i, ao in enumerate(vals):
            attr.data[i].color = (ao, ao, ao, 1.0)
        me.color_attributes.active_color = attr
        me.color_attributes.render_color_index = me.color_attributes.active_color_index


def finish2(asset_id, parts, children=(), sheet=True, export=True):
    """Assemble a v2 costume.
      parts     name -> object: body, head, leg_l/r, shin_l/r, arm_l/r, forearm_l/r, hand_l/r, optional
                wing_l / wing_r / tail (each ONE mesh, origin at its root)
      children  [(name, object, parent name)]: the face parts from face2() and the costume's soft_* /
                bob_* / alt_* parts, in world space on the untilted head. A child of the head (at any
                depth) gets the head tilt.
    Adds the bucket under hand_r, socket_bucket_light, and socket_wand under hand_l (origin at the wand
    grip, node +Z along the wand). Bakes AO per part, exports, renders the review sheet with the alt_
    parts hidden. Returns (root, total tris, per-part tris)."""
    root = K.empty(asset_id, (0, 0, 0))
    bkt, candy, BC = bucket()
    names = {nm: ob for nm, ob in parts.items()}
    for nm, ob, _ in children:
        assert nm not in names, nm
        names[nm] = ob
    names.update({"bucket": bkt, "bucket_candy": candy})
    for nm, ob in names.items():
        ob.name = nm
        assert ob.name == nm, (nm, ob.name)
    parent_of = {nm: ROOT_NAME for nm in V2_TOP if nm in parts}
    parent_of.update({nm: par for nm, par in V2_PARENT.items() if nm in parts})
    parent_of.update({nm: par for nm, _, par in children})
    parent_of.update({"bucket": "hand_r", "bucket_candy": "bucket"})
    for nm in V2_TOP[:2] + V2_TOP[5:] + list(V2_PARENT):
        assert nm in parts, f"missing part {nm}"

    def under_head(nm):
        while nm in parent_of:
            nm = parent_of[nm]
            if nm == "head":
                return True
        return False
    tilt_head2(parts["head"], [names[nm] for nm in names if under_head(nm)])
    light = K.empty("socket_bucket_light", tuple(BC))
    wand = K.empty("socket_wand", (0, 0, 0))
    front = V((0, -1, 0))
    wx = front.cross(WAND_DIR).normalized()
    wand.matrix_world = Matrix.Translation(WAND_GRIP) @ node_basis(wx, WAND_DIR.cross(wx), WAND_DIR).to_4x4()
    names.update({"socket_bucket_light": light, "socket_wand": wand})
    parent_of.update({"socket_bucket_light": "bucket", "socket_wand": "hand_l"})
    done = set()

    def attach(nm):
        if nm in done:
            return
        par = parent_of[nm]
        if par != ROOT_NAME:
            attach(par)
        attach2(names[nm], root if par == ROOT_NAME else names[par])
        done.add(nm)
    for nm in list(parent_of):
        attach(nm)
    for fn in POST_ATTACH:            # a costume's last touches on the assembled parts (custom normals ...)
        fn(names)
    counts = {nm: D.tris(ob) for nm, ob in names.items() if ob.type == "MESH"}
    total = K.tri_count([root])
    for nm, c in counts.items():
        print("PART", nm, c)
    print("TRIS", asset_id, total)
    bake_ao2(root, parent_of)
    if export:
        K.export([root], f"characters/{asset_id}")
    hide_alts(root, True)
    if sheet:
        K.review_sheet([root], asset_id)
    return root, total, counts


ROOT_NAME = "ROOT"


def hide_alts(root, hidden=True, show=()):
    """Hide (or show) every alt_ part in renders, as the game does. show: alt_ names to leave visible."""
    for o in root.children_recursive:
        if o.name.startswith("alt_"):
            o.hide_render = hidden and o.name not in show


# ------------------------------------------------------------------ posed review sheets
def pose(ob, x=0.0, y=0.0, z=0.0, scale=None):
    """Rotate a joint as the game does: a Godot YXZ Euler (x pitch, y yaw, z roll) on its rest pose."""
    if "rest" not in ob:
        ob["rest"] = [c for row in ob.matrix_basis for c in row]
    rest = Matrix([ob["rest"][i * 4:i * 4 + 4] for i in range(4)])
    m = rest @ Matrix.Rotation(y, 4, "Z") @ Matrix.Rotation(x, 4, "X") @ Matrix.Rotation(-z, 4, "Y")
    if scale is not None:      # Godot (x, y, z) scale = Blender local (x, z, y)
        m = m @ Matrix.Diagonal((scale[0], scale[2], scale[1], 1.0))
    ob.matrix_basis = m


def unpose(root):
    for o in root.children_recursive:
        if "rest" in o:
            o.matrix_basis = Matrix([o["rest"][i * 4:i * 4 + 4] for i in range(4)])
            del o["rest"]


def sheet_grid(root, name, tiles, cols=4, size=512):
    """One sheet of posed views: tiles [(setup(parts) or None, yaw, pitch, zoom, focus)], rendered like
    K.review_sheet (palette tint x AO, Workbench). setup poses the rig (see pose()); the rig is put
    back to rest after every tile. Saved to art/review/sheets/<name>.png."""
    import numpy as np
    meshes = [o for o in root.children_recursive if o.type == "MESH"]
    parts = {o.name: o for o in root.children_recursive}
    K._preview_colors(meshes)
    scene = bpy.context.scene
    scene.render.engine = "BLENDER_WORKBENCH"
    sh = scene.display.shading
    sh.light, sh.color_type, sh.show_shadows, sh.show_cavity = "STUDIO", "VERTEX", True, True
    scene.render.resolution_x = scene.render.resolution_y = size
    scene.render.film_transparent = False
    if not scene.world:
        scene.world = bpy.data.worlds.new("w")
    scene.world.color = (0.16, 0.19, 0.32)
    cam_data = bpy.data.cameras.new("cam")
    cam_data.type = "ORTHO"
    cam = K._link(bpy.data.objects.new("cam", cam_data))
    scene.camera = cam
    K.REVIEW.mkdir(parents=True, exist_ok=True)
    shots = []
    for i, (setup, yaw, pitch, zoom, focus) in enumerate(tiles):
        hide_alts(root, True)
        for o in root.children_recursive:
            if o.name.startswith(("mouth_", "eye_", "iris_")):
                o.hide_render = False
        if setup:
            setup(parts)
        cam_data.ortho_scale = 1.9 / zoom
        d = V((math.sin(math.radians(yaw)) * math.cos(math.radians(pitch)),
               -math.cos(math.radians(yaw)) * math.cos(math.radians(pitch)), math.sin(math.radians(pitch))))
        cam.location = V(focus) + d * 4.0
        cam.rotation_euler = (-d).to_track_quat("-Z", "Y").to_euler()
        path = K.REVIEW / f"_{name}_{i}.png"
        scene.render.filepath = str(path)
        bpy.ops.render.render(write_still=True)
        shots.append(path)
        unpose(root)
    rows = (len(shots) + cols - 1) // cols
    sheet = np.zeros((rows * size, cols * size, 4), np.float32)
    sheet[..., 3] = 1.0
    for i, pth in enumerate(shots):
        img = bpy.data.images.load(str(pth))
        px = np.empty(size * size * 4, np.float32)
        img.pixels.foreach_get(px)
        r, c = rows - 1 - i // cols, i % cols      # Blender images are bottom-up
        sheet[r * size:(r + 1) * size, c * size:(c + 1) * size] = px.reshape(size, size, 4)
        bpy.data.images.remove(img)
        pth.unlink(missing_ok=True)
    out = bpy.data.images.new("grid", cols * size, rows * size)
    out.pixels.foreach_set(sheet.ravel())
    out.filepath_raw = str(K.REVIEW / f"{name}.png")
    out.file_format = "PNG"
    out.save()
    bpy.data.images.remove(out)
    bpy.data.objects.remove(cam, do_unlink=True)
    hide_alts(root, True)
    for o in meshes:
        prev = o.data.color_attributes.get("Preview")
        if prev:
            o.data.color_attributes.remove(prev)
        ao = o.data.color_attributes.get("AO")
        if ao:
            o.data.color_attributes.active_color = ao
    return K.REVIEW / f"{name}.png"
