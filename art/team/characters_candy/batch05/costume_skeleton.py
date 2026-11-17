"""kid_skeleton v2: the Skeleton on kit v2 (design: batch04/ref/ref_skeleton.png; v1: batch04/costume_skeleton.py).

A black fleece onesie printed with puffy cream bones, a skull hood and two hood drawstrings:
  head      charcoal plush hood (taller dome) with a bone-coloured face rim (its inner side a step darker, the
            jaw), and the skull over the top: a padded cranium tucked into the hood round its edge, two BIG oval
            eye sockets (ink, each with a bone_shade bevel ring), an upside-down heart nose hole, a row of seven
            rounded teeth on the hood rim over the kid's fringe (gaps shaded, an ink gum line behind them), and
            sutures on the back (a zigzag sagittal line and the lambdoid V). The kid's face v2, brown fringe.
  body      ribs (4 pairs), sternum + manubrium, two lumbar vertebrae and the butterfly pelvis on the front;
            spine, shoulder blades, back ribs and pelvis on the back.
  arms      humerus on the upper arm, radius + ulna on the forearm, a dark rib cuff; black glove fists with a
            bone on every phalanx of every real finger, two on the thumb and metacarpals on the back of the hand.
  legs      femur front and back on the thigh, black / off-white high-tops with purple accents.
  soft_cord_l / soft_cord_r   the hood drawstrings (under body, origin at the top of each cord, inside the chin)
            hanging over the chest, each ending in a little bone toggle; the rig swings them (`cord` preset).

Every bone is a puffy band or knob in `bone` with its steep sides in `bone_shade`, so they have form and do not
clip to flat white; the black is charcoal on the plush layer (fleece, not vinyl). No wings, no tail.
"""
import math

import bmesh  # noqa: F401
from mathutils import Matrix  # noqa: F401

import kid_kit as KK
from kid_kit import D, K, V, PLUSH
from costume_common import dome

BLK, BLK_DK = "charcoal", "cat_black"          # fleece, seams / cuffs / shoe upper
BONE, BONE_SH = "skel_bone", "skel_shade"     # the print: top, steep sides (row 11, set_palette_skeleton.py)
INK = "ink"
SUTURE = "stone_dark"
SHOE = dict(upper=BLK_DK, panel="gourd_white", toe="gourd_white", sole="gourd_white", stripe="candy_grape", lace="gourd_white",
            collar=BLK, tongue=BLK_DK, heel="candy_grape", mat="vinyl")


# ================================================================== print helpers
def pband(name, pts, nrms, wid, hgt, segs=5, color=BONE, shade=BONE_SH, mat="flat", closed=False, sink=0.0035):
    """A puffy screen-print band lying on a surface: the upper arc of an elliptic cross-section (half
    width 'wid' across the surface, height 'hgt' out of it; edges tucked 'sink' into the surface) swept
    along pts with the surface normals nrms. Its two steep outer strips are painted 'shade' (form).
    wid / hgt are scalars or per-point lists; a 0 width end becomes a rounded pole."""
    n = len(pts)
    wl = wid if isinstance(wid, (list, tuple)) else [wid] * n
    hl = hgt if isinstance(hgt, (list, tuple)) else [hgt] * n
    bm = bmesh.new()
    rings = []
    for i in range(n):
        if closed:
            t = pts[(i + 1) % n] - pts[(i - 1) % n]
        else:
            t = pts[min(i + 1, n - 1)] - pts[max(i - 1, 0)]
        N = V(nrms[i]).normalized()
        t = (t - N * t.dot(N)).normalized()
        bv = N.cross(t)
        if wl[i] < 1e-6:
            rings.append([bm.verts.new(pts[i] + N * hl[i] * 0.35)])
            continue
        ring = []
        for j in range(segs + 1):
            a = math.pi * (j / segs - 0.5)
            ring.append(bm.verts.new(pts[i] + N * (math.cos(a) * hl[i] - sink * (1 - math.cos(a)))
                                     + bv * math.sin(a) * wl[i]))
        rings.append(ring)
    count = n if closed else n - 1
    side = []
    for i in range(count):
        A, Bq = rings[i], rings[(i + 1) % n]
        for j in range(segs):
            if len(A) == 1:
                f = bm.faces.new((A[0], Bq[j], Bq[j + 1]))
            elif len(Bq) == 1:
                f = bm.faces.new((A[j], Bq[0], A[j + 1]))
            else:
                f = bm.faces.new((A[j], Bq[j], Bq[j + 1], A[j + 1]))
            if shade and j in (0, segs - 1):
                side.append(f)
    return D.mesh(bm, name, color, mat, "box", smooth=True, extra=[(side, shade, mat)] if side else ())


def dblob(name, rad, segs=8, rings=4, color=BONE, shade=BONE_SH, edge=0.5):
    """Upper half of an ellipsoid (a print knob showing above the cloth), its steep sides shaded."""
    b = KK.blob(name, (0, 0, 0), rad, segs=segs, rings=rings, color=color, mat="flat",
                cut=lambda c: c.z < -rad[2] * 0.45)
    if shade:
        K.paint(b, shade, where=lambda c, n: n.z < edge)
    return b


def pbone(name, a, b, nrm, r=0.011, knob=1.4, color=BONE):
    """Cartoon bone (shaft + two knobs at each end) lying on a surface between a and b; only the halves
    above the surface are built."""
    a, b, nrm = V(a), V(b), V(nrm).normalized()
    ax = (b - a).normalized()
    side = nrm.cross(ax).normalized()
    shaft = pband(name + "_s", [a + ax * r * 0.6, (a + b) / 2, b - ax * r * 0.6], [nrm, nrm, nrm], r * 0.8, r * 0.78, segs=5,
                  sink=0.002, color=color)
    parts = [shaft]
    for e, (p, dirn) in enumerate(((a, -ax), (b, ax))):
        for s in (1, -1):
            k = dblob(f"{name}_k{e}{s}", (r * knob, r * knob, r * 0.88), segs=10, rings=4, color=color)
            D.put(k, p + side * s * r * 0.95 + dirn * r * 0.2 + nrm * r * 0.12, nrm, up=ax)
            parts.append(k)
    return K.merge(parts, name)


def capped(curve, w):
    """Extend a 2D (x, z) polyline with round caps at both ends: returns [(x, z, width factor)]."""
    P = [V((x, 0, z)) for x, z in curve]
    t0 = (P[0] - P[1]).normalized()
    t1 = (P[-1] - P[-2]).normalized()
    cap = [(0.5, 0.87), (0.87, 0.5), (1.0, 0.0)]
    out = [(P[0] + t0 * w * d, f) for d, f in reversed(cap)]
    out += [(p, 1.0) for p in P]
    out += [(P[-1] + t1 * w * d, f) for d, f in cap]
    return [(p.x, p.z, f) for p, f in out]


def print_band(name, tree, curve, w, h, back=False, closed=False, segs=5, color=BONE):
    """A puffy print band along a 2D (x, z) curve projected onto a body surface from the front / back."""
    ray = V((0, -1, 0)) if back else V((0, 1, 0))
    samples = [(x, z, 1.0) for x, z in curve] if closed else capped(curve, w)
    pts, nrms, ws, hs = [], [], [], []
    for x, z, f in samples:
        loc, nrm = KK.on_surface(tree, (x, -0.3 * ray.y, z), ray)
        if loc is None:
            loc, nrm = V((x, -0.15 * ray.y, z)), -ray
        pts.append(loc)
        nrms.append(nrm)
        ws.append(w * f)
        hs.append(h * max(f, 0.35))
    return pband(name, pts, nrms, ws, hs, segs=segs, color=color, closed=closed)


def conf_disc(name, tree, outline, center, ray, up=(0, 0, 1), lift=0.0025, color=INK, mat="flat", rings=(0.5, 1.0), wall=0.006):
    """A flat applique filled with concentric rings, every vertex conformed to a curved surface."""
    ray = V(ray).normalized()
    rgt = ray.cross(V(up)).normalized()
    upv = rgt.cross(ray).normalized()
    c = V(center)
    bm = bmesh.new()

    nr = {}

    def vert(u, v):
        # lifted along the surface normal (a lift along the ray is tangential where the surface slopes away)
        q = c + rgt * u + upv * v
        loc, nrm = KK.on_surface(tree, q, ray)
        if loc is None:
            loc, nrm = q, -ray
        bv = bm.verts.new(loc + nrm * lift)
        nr[bv] = nrm
        return bv
    mid = vert(0.0, 0.0)
    rows = [[vert(u * f, v * f) for u, v in outline] for f in rings]      # outline is CCW seen along ray
    n = len(outline)
    faces = [(mid, rows[0][i], rows[0][(i + 1) % n]) for i in range(n)]
    for A, Bq in zip(rows[:-1], rows[1:]):
        faces += [(A[i], Bq[i], Bq[(i + 1) % n], A[(i + 1) % n]) for i in range(n)]
    edge = rows[-1]
    low = [bm.verts.new(v.co - nr[v] * wall) for v in edge]
    faces += [(edge[i], low[i], low[(i + 1) % n], edge[(i + 1) % n]) for i in range(n)]
    for fv in faces:
        bm.faces.new(fv)
    return D.mesh(bm, name, color, mat, "box", smooth=True)


def conf_ring(name, tree, outline, center, ray, rows, up=(0, 0, 1), color=BONE_SH, mat="flat"):
    """An annulus round an outline, conformed to a surface: rows [(scale, lift)] from the inside out. With the
    inner row sunk and the middle one raised it reads as the rounded lip of a hole (a socket's bevel)."""
    ray = V(ray).normalized()
    rgt = ray.cross(V(up)).normalized()
    upv = rgt.cross(ray).normalized()
    c = V(center)
    bm = bmesh.new()

    def vert(u, v, lift):
        q = c + rgt * u + upv * v
        loc, nrm = KK.on_surface(tree, q, ray)
        if loc is None:
            loc, nrm = q, -ray
        return bm.verts.new(loc + nrm * lift)
    R = [[vert(u * f, v * f, lift) for u, v in outline] for f, lift in rows]
    n = len(outline)
    for A, Bq in zip(R[:-1], R[1:]):
        for i in range(n):
            bm.faces.new((A[i], Bq[i], Bq[(i + 1) % n], A[(i + 1) % n]))
    return D.mesh(bm, name, color, mat, "box", smooth=True)


def blob_on(name, tree, x, z, rad, back=False, roll=0.0, color=BONE, segs=10):
    ray = V((0, -1, 0)) if back else V((0, 1, 0))
    loc, nrm = KK.on_surface(tree, (x, -0.3 * ray.y, z), ray)
    b = dblob(name, rad, segs=segs, rings=4, color=color)
    D.put(b, loc + nrm * rad[2] * 0.15, nrm, roll=roll)
    return b


# ================================================================== the skull hood
HOOD_WARP = dome(0.9, 1.9)                   # a taller hood with an upright forehead for the skull's face
TEETH_Z = 0.997                               # top of the teeth row (under the skull's lower edge)
# skull edge as (azimuth from the front, polar angle from the crown) in degrees: along the teeth line at the
# front, stepping round the face rim, down to ear level at the sides and well down the back
EDGE = [(0, 55.0), (14, 55.0), (26, 57.0), (36, 65.0), (45, 76.0), (56, 84.0), (90, 90.0), (135, 96.0), (180, 99.0)]
SOCK_C = (0.092, 1.052)                       # eye socket centre (x, z), seen along SOCK_RAY
SOCK_S = (1.4, 1.4)                         # socket outline scale (across, up) on v1's
SOCK_RAY = (0, 1, -0.15)
NOSE_Z = 1.012


def edge(ph):
    ph = abs(ph)
    for (p0, t0), (p1, t1) in zip(EDGE[:-1], EDGE[1:]):
        if p0 <= ph <= p1:
            u = (ph - p0) / (p1 - p0)
            return t0 + (t1 - t0) * u * u * (3 - 2 * u)
    return EDGE[-1][1]


def skull_pt(th, ph, o):
    """A point 'o' metres off the hood ellipsoid (warped like the hood). th: polar angle from the crown,
    ph: azimuth from the front (-Y) toward the character's left (+X), in degrees."""
    t, f = math.radians(th), math.radians(ph)
    d = V((math.sin(t) * math.sin(f), -math.sin(t) * math.cos(f), math.cos(t)))
    p = KK.HC + V(((KK.HR.x + o) * d.x, (KK.HR.y + o) * d.y, (KK.HR.z + o) * d.z))
    # an upright forehead for the sockets: the front of the cranium pushed forward above the teeth (fading
    # out with the padding toward the tucked edge, so the skull still meets the hood there)
    fwd = 0.045 * math.exp(-((th - 40) / 15) ** 2) * max(math.cos(f), 0.0) ** 1.2 * min(max(o + 0.004, 0.0) / 0.03, 1.0)
    return HOOD_WARP(p) + V((0, -fwd, 0))


def puff(th, ph):
    """Skull padding height: a pillow tucked into the hood all round its edge, a fuller brow and cheekbone
    bulges beside the sockets, and a rounder, fuller cranium at the back (no flat dome)."""
    a = abs(ph)
    w = 7.0 + 20.0 * min(a / 90.0, 1.0)
    t = min(max((edge(ph) - th) / w, 0.0), 1.0)
    fr = max(math.cos(math.radians(min(a, 90.0))), 0.0) ** 1.5 * min(th / 35.0, 1.0)
    top = 0.012 + 0.015 * fr
    top += 0.009 * math.exp(-((th - 36) / 16) ** 2) * math.exp(-(a / 30) ** 2)        # brow
    top += 0.008 * math.exp(-((th - 60) / 13) ** 2) * math.exp(-((a - 50) / 13) ** 2)  # cheekbones
    top += 0.012 * math.exp(-((th - 62) / 26) ** 2) * math.exp(-((a - 180) / 50) ** 2)  # occiput: the back bulges
    return -0.004 + (top + 0.004) * math.sqrt(1 - (1 - t) ** 2)


def squircle(q, hw, hh, n=4.0):
    """Blob warp: pushes an ellipsoid's outline (local x / y) out toward a rounded rectangle."""
    u, v = q.x / hw, q.y / hh
    r = math.hypot(u, v)
    if r < 1e-6:
        return q
    cu, cv = abs(u) / r, abs(v) / r
    s = (cu ** n + cv ** n) ** (-1.0 / n)
    return V((q.x * s, q.y * s, q.z))


def mirror_ccw(base_cw, sx):
    """A CW outline drawn for the socket on the viewer's right (+X): CCW outline for either side."""
    return list(reversed(base_cw)) if sx > 0 else [(-u, v) for u, v in base_cw]


def zigzag(name, path, amp, n, r=0.0034):
    """A suture: a zigzag stroke along path [(th, ph)] (skull angles), sunk half into the cranium."""
    pts = []
    for k in range(n):
        u = k / (n - 1)
        x = u * (len(path) - 1)
        i = min(int(x), len(path) - 2)
        f = x - i
        th = path[i][0] * (1 - f) + path[i + 1][0] * f
        ph = path[i][1] * (1 - f) + path[i + 1][1] * f
        p0 = skull_pt(th, ph, puff(th, ph) + 0.0004)
        # the zig: sideways across the line, every other point
        th2 = path[min(i + 1, len(path) - 1)][0] - path[i][0]
        ph2 = path[min(i + 1, len(path) - 1)][1] - path[i][1]
        L = math.hypot(th2, ph2) or 1.0
        s = (1 if k % 2 else -1) * (0.0 if k in (0, n - 1) else 1.0)
        th3, ph3 = th - ph2 / L * amp * s, ph + th2 / L * amp * s / max(math.sin(math.radians(th)), 0.3)
        pts.append(skull_pt(th3, ph3, puff(th3, ph3) + 0.0004) if s else p0)
    rr = [r * (0.4 if k in (0, n - 1) else 1.0) for k in range(n)]
    return D.sweep(name, pts, rr, sides=5, color=SUTURE, mat="flat", project="box")


def build_skull():
    NPH, NTH = 44, 14
    bm = bmesh.new()
    pole = bm.verts.new(skull_pt(0.0, 0.0, puff(0.0, 0.0)))
    rings = []
    for j in range(1, NTH + 1):
        s = 0.5 * j / NTH + 0.5 * math.sin(0.5 * math.pi * j / NTH)          # denser toward the edge
        ring = []
        for i in range(NPH):
            ph = -180.0 + 360.0 * i / NPH
            th = s * (50.0 + (edge(ph) - 50.0) * s * s)
            ring.append(bm.verts.new(skull_pt(th, ph, puff(th, ph))))
        rings.append(ring)
    faces = [(pole, rings[0][i], rings[0][(i + 1) % NPH]) for i in range(NPH)]
    for A, Bq in zip(rings[:-1], rings[1:]):
        faces += [(A[i], Bq[i], Bq[(i + 1) % NPH], A[(i + 1) % NPH]) for i in range(NPH)]
    for fv in faces:
        f = bm.faces.new(fv)
        f.normal_update()
        if f.normal.dot(f.calc_center_median() - KK.HC) < 0:
            f.normal_flip()
    bmesh.ops.triangulate(bm, faces=bm.faces[:], quad_method="BEAUTY")
    sk = D.mesh(bm, "skull", BONE, "flat", "sphere", center=KK.HC)
    tree = D.bvh(sk)
    parts = [sk]
    ray = V(SOCK_RAY)
    # big oval eye sockets on the skull's front face: rounded, wider at the top-outer corner, tapering to a
    # rounded point at the bottom-inner corner; a bone_shade bevel ring round each (it reads as a hollow)
    base = [(-0.045, 0.032), (0.042, 0.04), (0.054, -0.008), (0.008, -0.048), (-0.047, -0.022)]       # CW
    sock = [(u * SOCK_S[0], v * SOCK_S[1]) for u, v in KK.rounded_poly(base, 0.027, 5)]
    for sx in (1, -1):
        c = (SOCK_C[0] * sx, -0.25, SOCK_C[1])
        o = mirror_ccw(sock, sx)
        # flush with the bone (a raised dark disc read as a black lump): the hollow is the shaded bevel round it
        parts.append(conf_disc(f"socket{sx}", tree, o, c, ray, lift=0.003, rings=(0.2, 0.4, 0.6, 0.8, 1.0), wall=0.004))
        parts.append(conf_ring(f"sockbev{sx}", tree, o, c, ray, ((0.96, 0.0), (1.07, 0.0034), (1.18, 0.0008))))
    # nose cavity: a dark upside-down heart between and below the sockets, with a shaded bevel
    heart = KK.rounded_poly([(0.0, -0.025), (0.028, 0.007), (0.017, 0.021), (0.0, 0.012), (-0.017, 0.021),
                             (-0.028, 0.007)], 0.006, 2)
    parts.append(conf_disc("nose", tree, heart, (0.0, -0.25, NOSE_Z), ray, lift=0.003, rings=(0.33, 0.66, 1.0), wall=0.004))
    parts.append(conf_ring("nosebev", tree, heart, (0.0, -0.25, NOSE_Z), ray, ((0.94, 0.0), (1.12, 0.0032), (1.26, 0.0008))))
    # upper jaw: a dark gum line under the skull's lower edge, then a row of six big rounded teeth (as the mock)
    line = []
    for k in range(13):
        x = -0.118 + 0.236 * k / 12
        p, n = KK.ellipsoid_pt(KK.HC, KK.HR, x, TEETH_Z + 0.002)
        fwd = V((n.x, n.y, 0)).normalized()
        line.append(HOOD_WARP(p) + fwd * 0.031)
    parts.append(D.sweep("jaw_gap", line, [0.0] + [0.0068] * 11 + [0.0], sides=6, color=INK, mat="flat", project="box"))
    for i, x in enumerate((-0.08, -0.048, -0.016, 0.016, 0.048, 0.08)):
        k = abs(x) / 0.08
        hw, hh = 0.0152 - 0.0012 * k, 0.0252 - 0.004 * k * k
        p, n = KK.ellipsoid_pt(KK.HC, KK.HR, x, TEETH_Z - hh)
        fwd = V((n.x, n.y, 0)).normalized()
        # a puffy rounded-square tooth (a superellipse pillow, domed front), its lower edge and sides shaded
        tooth = KK.blob(f"tooth{i}", (0, 0, 0), (hw, hh, 0.0095), segs=12, rings=6, color=BONE, mat="flat",
                        warp=lambda q, hw=hw, hh=hh: squircle(q, hw, hh), cut=lambda c: c.z < -0.004)
        K.paint(tooth, BONE_SH, where=lambda c, n: n.y < -0.45 or abs(n.x) > 0.62)
        D.put(tooth, HOOD_WARP(p) + fwd * 0.03, fwd + V((0, 0, 0.08)), up=(0, 0, 1))
        parts.append(tooth)
    # sutures on the back of the cranium: the sagittal line down the middle, the lambdoid V across the back
    parts.append(zigzag("sut_sag", [(26.0, 180.0), (50.0, 180.0), (74.0, 180.0)], 1.6, 17))
    for sx in (1, -1):
        parts.append(zigzag(f"sut_lam{sx}", [(74.0, 180.0), (83.0, 180.0 - 24.0 * sx), (90.0, 180.0 - 48.0 * sx)], 1.5, 11))
    # the coronal suture over the crown, behind the brow
    parts.append(zigzag("sut_cor", [(40.0, -95.0), (28.0, -140.0), (24.0, 180.0), (28.0, 140.0), (40.0, 95.0)], 1.4, 23))
    return parts


def cull_under_skull(hood, margin=12.0):
    """Delete the hood shell's faces that lie well under the skull (never seen: about half the hood)."""
    k = 0.9                                     # = HOOD_WARP's stretch above z0 0.97 (dome(0.9, ...))
    bm = bmesh.new()
    bm.from_mesh(hood.data)
    loc = V(hood.location)             # (matrix_world is not updated yet: the blob only set its location)
    dead = []
    for f in bm.faces:
        c = f.calc_center_median() + loc
        if c.z > 0.97:
            c = V((c.x, c.y, 0.97 + (c.z - 0.97) / (1.0 + k)))
        d = c - KK.HC
        d = V((d.x / KK.HR.x, d.y / KK.HR.y, d.z / KK.HR.z)).normalized()
        th = math.degrees(math.acos(max(-1.0, min(1.0, d.z))))
        ph = math.degrees(math.atan2(d.x, -d.y))
        if th < edge(ph) - margin:
            dead.append(f)
    bmesh.ops.delete(bm, geom=dead, context="FACES")
    bm.to_mesh(hood.data)
    bm.free()
    return hood


def hood_rim(rim):
    """The face rim in bone; its inner half (toward the face and into the hood) a step darker: the jaw."""
    def inner(c, n):
        to_mid = V((-c.x, 0.0, KK.OZ - c.z))
        if to_mid.length < 1e-6:
            return False
        return n.dot(to_mid.normalized()) > 0.62 or n.y > 0.75
    K.paint(rim, BONE_SH, where=inner)
    return rim


def head():
    parts = KK.hood2(BLK, rim_color=BONE, mat=PLUSH, rim_mat="flat", warp=HOOD_WARP, stitches=0, segs=36, rings=22, rim_r=0.034)
    hood_rim(parts[1])
    cull_under_skull(parts[0])
    parts += build_skull()
    static, children = KK.face2(fang=False)
    parts += static + KK.fringe2(clip=KK.hood_clip())
    return D.part(parts, "head", KK.NECK), children


# ================================================================== body print
def pelvis(tree, back):
    """Two iliac wings (thick bone rings round a black hole, tipped up at the outer side) meeting a rounded
    sacrum in the middle: the butterfly pelvis of the reference."""
    tag = "b" if back else ""
    parts = []
    for sx in (1, -1):
        cv = []
        for i in range(14):
            a = 2 * math.pi * i / 14
            u, v = 0.047 * math.cos(a), 0.028 * math.sin(a) * (1.0 + 0.25 * math.cos(a))
            r = math.radians(20)
            cv.append((sx * (0.072 + u * math.cos(r) - v * math.sin(r)), 0.335 + u * math.sin(r) + v * math.cos(r)))
        parts.append(print_band(f"{tag}ilium{sx}", tree, cv, 0.0165, 0.008, back=back, closed=True))
    parts.append(blob_on(f"{tag}sacrum", tree, 0.0, 0.322, (0.025, 0.036, 0.009), back=back))
    return parts


def body():
    torso = KK.torso2(BLK)
    tree = D.bvh(torso)
    parts = [torso]
    H = 0.0085
    # sternum: a rounded manubrium at the collar and the body of the sternum
    parts.append(print_band("manubrium", tree, [(0.0, 0.594), (0.0, 0.576)], 0.0185, H))
    parts.append(print_band("sternum", tree, [(0.0, 0.552), (0.0, 0.5), (0.0, 0.452)], 0.0135, H))
    # four pairs of ribs curving out from the sternum and down round the sides
    for k, (z0, w, hw) in enumerate([(0.566, 0.87, 0.0135), (0.523, 0.89, 0.0135), (0.48, 0.87, 0.013), (0.437, 0.79, 0.0125)]):
        for sx in (1, -1):
            cv = []
            for t in range(7):
                u = t / 6
                z = z0 + 0.012 * u - 0.054 * u * u
                x = (0.028 + u * (w * KK.torso_r(z) - 0.028)) * sx
                cv.append((x, z))
            parts.append(print_band(f"rib{k}{sx}", tree, cv, hw, H))
    for i, z in enumerate((0.412, 0.383)):
        parts.append(blob_on(f"lumbar{i}", tree, 0.0, z, (0.027, 0.012, 0.009)))
    parts.extend(pelvis(tree, False))
    # back print (the gameplay camera mostly sees the back): spine, shoulder blades, back ribs, pelvis
    for i, z in enumerate((0.592, 0.552, 0.512, 0.472, 0.432, 0.392, 0.352)):
        parts.append(blob_on(f"vert{i}", tree, 0.0, z, (0.028 - i * 0.001, 0.014, 0.009), back=True))
    for sx in (1, -1):
        parts.append(blob_on(f"scap{sx}", tree, 0.09 * sx, 0.556, (0.044, 0.031, 0.009), back=True, roll=-24 * sx))
    for k, (z0, w) in enumerate([(0.497, 0.87), (0.454, 0.85), (0.412, 0.77)]):
        for sx in (1, -1):
            cv = []
            for t in range(6):
                u = t / 5
                z = z0 + 0.01 * u - 0.046 * u * u
                x = (0.042 + u * (w * KK.torso_r(z) - 0.042)) * sx
                cv.append((x, z))
            parts.append(print_band(f"brib{k}{sx}", tree, cv, 0.0125, H, back=True, segs=4))
    parts.extend(pelvis(tree, True))
    return D.part(parts, "body", (0, 0, KK.HIP_Z)), tree


# ================================================================== hood drawstrings (soft_cord_l / soft_cord_r)
def cords(tree):
    """[(name, object, 'body')]: each cord from inside the chin (its origin, the pin) out under the rim and down
    over the chest, 13 mm off the print, ending in a little bone toggle. Distance from the pin grows along it."""
    out = []
    for sx, s in ((1, "l"), (-1, "r")):
        x0 = 0.058 * sx
        chest = []
        for z, dx in ((0.605, 0.002), (0.565, 0.005), (0.528, 0.008), (0.494, 0.01)):
            loc, nrm = KK.on_surface(tree, (x0 + dx * sx, -0.6, z), (0, 1, 0))
            chest.append(loc + nrm * 0.018)
        top = chest[0] + V((0.0, 0.035, 0.065))           # up and back inside the chin
        pts, par = KK.catmull([top] + chest, per=4)
        rr = [0.0064] * len(pts)
        cord = D.sweep(f"cord{s}", pts, rr, sides=7, color=BONE, mat="flat", project="cyl")
        K.paint(cord, BONE_SH, where=lambda c, n, sx=sx: n.y > 0.35 or n.x * sx > 0.75)
        # the toggle: a little bone hanging on, along the cord's end
        d = (pts[-1] - pts[-2]).normalized()
        nrm = (pts[-1] - V((0, 0.02, pts[-1].z))).normalized()
        nrm = (nrm - d * nrm.dot(d)).normalized()
        a = pts[-1] + d * 0.004
        b = a + d * 0.034
        side = d.cross(nrm).normalized()
        tog = [D.sweep(f"tog{s}", [a, b], [0.0056, 0.0056], sides=8, color=BONE, mat="flat", project="cyl")]
        for e, p in enumerate((a, b)):
            for k in (1, -1):
                kn = KK.blob(f"togk{s}{e}{k}", p + side * k * 0.0058 + d * (0.002 if e else -0.002), (0.0082, 0.0082, 0.0078),
                             segs=8, rings=5, color=BONE, mat="flat")
                tog.append(kn)
        togm = K.merge(tog, f"toggle{s}")
        K.paint(togm, BONE_SH, where=lambda c, n: n.dot(nrm) < -0.2)
        out.append(("soft_cord_" + s, KK.soft_part("soft_cord_" + s, [cord, togm], top), "body"))
    return out


# ================================================================== arms and gloves
def arm_bones(sx, upper):
    """extras for arm2: the humerus along the outer front of the upper arm, or radius + ulna on the forearm."""
    side = V((0.75 * sx, -0.66, 0.06)).normalized()
    radii = [0.066, (0.066 + 0.058) * 0.5 + 0.003, 0.058] if upper else [0.056, (0.058 + 0.05) * 0.5 + 0.002, 0.05]

    def fn(pts):
        n = len(pts)

        def at(u, off=0.0):
            x = u * (n - 1)
            i = min(int(x), n - 2)
            f = x - i
            p = pts[i].lerp(pts[i + 1], f)
            tg = (pts[i + 1] - pts[i]).normalized()
            o = (side - tg * side.dot(tg)).normalized()
            bo = tg.cross(o)
            r = KK.lerp_list(radii, u)
            ang = off / r
            o2 = (o * math.cos(ang) + bo * math.sin(ang)).normalized()
            return p + o2 * r, o2
        out = []
        if upper:
            a, na = at(0.36)
            b, nb = at(0.9)
            out.append(pbone(f"humerus{sx}", a, b, (na + nb).normalized(), r=0.011))
        else:
            for k, off in enumerate((0.0115, -0.0115)):
                a, na = at(0.2, off)
                b, nb = at(0.8, off * 0.8)
                out.append(pbone(f"fore{sx}{k}", a, b, (na + nb).normalized(), r=0.0078, knob=1.5))
        return out
    return fn


def finger_bones(name):
    """grip_hand extras: a bone on every phalanx of every finger (on its back, the outer side of the fist), two on
    the thumb and three metacarpals on the back of the hand."""
    def extras(F):
        G, a, s, t, fw, fr, R0, W, pc = F["grip"], F["a"], F["s"], F["t"], F["fw"], F["fr"], F["R0"], F["wrist"], F["palm"]
        fat = fw / 0.0172
        objs = []
        arcs = (200, 212, 204, 186)
        rads = (1.0, 1.04, 1.0, 0.88)
        for i in range(4):
            off = a * ((1.5 - i) * fw)
            arc = arcs[i]
            segs = [(84.0, 128.0, 1.0), (136.0, 168.0, 0.9), (176.0, arc - 6.0, 0.8)]
            for k, (t0, t1, wk) in enumerate(segs):
                tm = 0.5 * (t0 + t1)
                u = (tm + 32) / (arc + 32)
                r_f = fr * rads[i] * (1.0 - 0.14 * u)
                th = math.radians(tm)
                rad = s * math.cos(th) + t * math.sin(th)
                knuckle = 0.0035 * fat * math.exp(-((tm - 78) / 34) ** 2)
                p = G + off + rad * (R0 + knuckle + r_f)
                tang = -s * math.sin(th) + t * math.cos(th)
                L = (R0 + r_f) * math.radians(t1 - t0)
                b = dblob(f"{name}_ph{i}{k}", (r_f * 0.62 * wk + 0.0012, L * 0.5, 0.0034), segs=10, rings=3, edge=0.55)
                D.put(b, p - rad * 0.0012, rad, up=tang)
                objs.append(b)
        # thumb: two bones on its outer side
        for k, (u0, u1) in enumerate(((0.22, 0.52), (0.6, 0.9))):
            um = 0.5 * (u0 + u1)
            th = math.radians(34 - 176 * um)
            offt = a * (2.2 * fw - 0.016 * fat * um)
            rad = s * math.cos(th) + t * math.sin(th)
            r_t = 0.0112 * fat * (1.0 - 0.12 * um)
            p = G + offt + rad * (F["R0"] - fr * 0.9 + 0.0108 * fat + r_t)
            tang = (s * -math.sin(th) + t * math.cos(th)) * -1.0
            L = (0.0108 * fat + 0.012) * math.radians(176 * (u1 - u0))
            b = dblob(f"{name}_th{k}", (r_t * 0.6, L * 0.5, 0.0034), segs=10, rings=3, edge=0.55)
            D.put(b, p - rad * 0.0012, rad, up=tang)
            objs.append(b)
        # metacarpals on the back of the hand (the palm blob's outer side, +s), running toward the knuckles
        M = D.look(a, up=s)
        R = (0.029 * fat, 0.021 * fat, 0.041 * fat)
        for k, f in enumerate((-0.42, 0.0, 0.42)):
            u_dir = M.inverted() @ (s * 1.0 + t * 0.22 + a * f)
            loc, nrm = on_ell(pc, M, R, u_dir)
            knuck = G + a * (f * 1.6 * fw) + (s * math.cos(math.radians(78)) + t * math.sin(math.radians(78))) * R0
            dirn = knuck - loc
            dirn = (dirn - nrm * dirn.dot(nrm)).normalized()
            b = dblob(f"{name}_mc{k}", (0.0078, 0.0175, 0.0042), segs=10, rings=3, edge=0.55)
            D.put(b, loc - nrm * 0.001, nrm, up=dirn)
            objs.append(b)
        return objs
    return extras


def on_ell(c, M, R, u):
    """Point and normal on an ellipsoid (centre c, rotation M, radii R) in local direction u."""
    u = V(u).normalized()
    s = 1.0 / math.sqrt((u.x / R[0]) ** 2 + (u.y / R[1]) ** 2 + (u.z / R[2]) ** 2)
    p = u * s
    n = V((p.x / R[0] ** 2, p.y / R[1] ** 2, p.z / R[2] ** 2)).normalized()
    return V(c) + M @ p, (M @ n).normalized()


# ================================================================== legs
def leg_bones(x):
    """thigh2 extras: a femur down the outer front and the outer back of the thigh (above the shoe's collar)."""
    sx = 1 if x > 0 else -1
    out = []

    def tp(z, th, back):
        r = KK.THIGH_R + 0.0005
        s = 1 if back else -1
        n = V((math.sin(th), s * math.cos(th), 0))
        return V((x, 0, z)) + n * r, n
    for nm, back in (("f", False), ("b", True)):
        a, na = tp(0.272, 0.62 * sx, back)
        b, nb = tp(0.198, 0.4 * sx, back)
        out.append(pbone(f"femur{nm}{sx}", a, b, (na + nb).normalized(), r=0.0115))
    return out


def shoe(name, x):
    return KK.sneaker(name, x, SHOE)


# ================================================================== the costume
def build():
    """Returns (parts, children) for KK.finish2()."""
    KK.AO_SHAPE.clear()
    KK.AO_POST.clear()
    KK.POST_ATTACH[:] = []
    KK.AO_IGNORE.clear()
    for nm in ("soft_cord_l", "soft_cord_r"):
        KK.AO_IGNORE[nm] = {"head", "soft_cord_l", "soft_cord_r"}
    KK.AO_IGNORE["body"] = {"soft_cord_l", "soft_cord_r"}
    parts = {}
    parts["body"], tree = body()
    parts["head"], children = head()
    for s, hip in (("l", KK.HIP_L), ("r", KK.HIP_R)):
        parts["leg_" + s], parts["shin_" + s] = KK.leg2("leg_" + s, hip, BLK, lambda x, s=s: shoe("leg_" + s, x),
                                                        thigh_extras=leg_bones)
    for s, sx, sho, elb, wri in (("l", 1, KK.SHO_L, KK.ELBOW_L, KK.WRIST_L), ("r", -1, KK.SHO_R, KK.ELBOW_R, KK.WRIST_R)):
        parts["arm_" + s], parts["forearm_" + s] = KK.arm2("arm_" + s, sho, elb, wri, BLK, mat=PLUSH, cuff=(BLK_DK, PLUSH),
                                                           upper_extras=arm_bones(sx, True), fore_extras=arm_bones(sx, False))
    parts["hand_l"] = KK.grip_hand("hand_l", KK.WRIST_L, KK.WAND_GRIP, KK.WAND_DIR, (0.55, 0.83, 0.0), 0.0155, fat=1.15,
                                   color=BLK, mat=PLUSH, extras=finger_bones("hand_l"))
    parts["hand_r"] = KK.grip_hand("hand_r", KK.WRIST_R, KK.BUCKET_GRIP, (1, 0, 0), (0.0, -0.45, 0.9), 0.012, fat=1.15,
                                   color=BLK, mat=PLUSH, extras=finger_bones("hand_r"))
    children += cords(tree)
    return parts, children
