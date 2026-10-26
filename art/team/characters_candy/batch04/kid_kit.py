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
