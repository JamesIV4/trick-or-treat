"""kid_skeleton : one costume builder (see build_costumes.py for the run command and the rig contract).

Follows the costume reference (ref/ref_skeleton.png): a black hooded onesie with a big puffy white
cartoon skull sitting on top of the hood (two big ink eye sockets, a little upside-down heart nose and
a row of teeth along its lower edge, just above the kid's fringe), a white jaw band framing the face,
a puffy screen-print skeleton on the suit (sternum, ribs, lumbar vertebrae, pelvis; spine, back ribs,
shoulder blades and pelvis on the back), arm and leg bones, black gloves with white finger bones on
the backs of the hands, and chunky black/white high-top sneakers."""
import math

import bmesh  # noqa: F401
from mathutils import Matrix  # noqa: F401

import kid_kit as KK
from kid_kit import D, K, V, FELT  # noqa: F401
from costume_common import tube, panel, stitch_line, bone, bow, hood_eyes, dome, back_y, leaf_outline  # noqa: F401

BLK = "charcoal"        # the onesie
GLOVE = "cat_black"
W = "trim_white"        # skull and bone print
TOOTH = "lolly_white"
INK = "ink"


# ================================================================== helpers (skeleton only)
def pband(name, pts, nrms, wid, hgt, segs=4, color=W, mat="flat", closed=False, sink=0.0035):
    """A puffy screen-print band lying on a surface: the upper arc of an elliptic cross-section (half
    width 'wid' across the surface, height 'hgt' out of it; its edges tucked 'sink' into the surface,
    the buried half is not built) swept along pts with the surface normals nrms. wid/hgt are scalars
    or per-point lists; a 0 width end becomes a rounded pole."""
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
            rings.append([bm.verts.new(pts[i] - N * sink * 0.5)])
            continue
        ring = []
        for j in range(segs + 1):
            a = math.pi * (j / segs - 0.5)
            ring.append(bm.verts.new(pts[i] + N * (math.cos(a) * hl[i] - sink * (1 - math.cos(a)))
                                     + bv * math.sin(a) * wl[i]))
        rings.append(ring)
    count = n if closed else n - 1
    for i in range(count):
        A, Bq = rings[i], rings[(i + 1) % n]
        for j in range(segs):
            if len(A) == 1:
                bm.faces.new((A[0], Bq[j], Bq[j + 1]))
            elif len(Bq) == 1:
                bm.faces.new((A[j], Bq[0], A[j + 1]))
            else:
                bm.faces.new((A[j], Bq[j], Bq[j + 1], A[j + 1]))
    return D.mesh(bm, name, color, mat, "box", smooth=True)


def dblob(name, rad, segs=8, rings=4, color=W):
    """Upper half of an ellipsoid (the part of a print blob that shows above the cloth)."""
    return KK.blob(name, (0, 0, 0), rad, segs=segs, rings=rings, color=color, mat="flat",
                   cut=lambda c: c.z < -rad[2] * 0.45)


def pbone(name, a, b, nrm, r=0.011, knob=1.4, color=W):
    """Cartoon bone (shaft + two knobs at each end) lying on a surface between a and b; only the
    halves above the surface are built."""
    a, b, nrm = V(a), V(b), V(nrm).normalized()
    L = (b - a).length
    ax = (b - a).normalized()
    side = nrm.cross(ax).normalized()
    shaft = pband(name + "_s", [a + ax * r * 0.6, b - ax * r * 0.6], [nrm, nrm], r * 0.8, r * 0.75, sink=0.002, color=color)
    parts = [shaft]
    for e, (p, dirn) in enumerate(((a, -ax), (b, ax))):
        for s in (1, -1):
            k = dblob(f"{name}_k{e}{s}", (r * knob, r * knob, r * 0.85), segs=7, rings=4, color=color)
            D.put(k, p + side * s * r * 0.95 + dirn * r * 0.2 + nrm * r * 0.15, nrm, up=ax)
            parts.append(k)
    ob = K.merge(parts, name)
    return ob


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


def print_band(name, tree, curve, w, h, back=False, closed=False, segs=4, color=W):
    """A puffy print band along a 2D (x, z) curve projected onto a body surface from the front/back."""
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


def conf_disc(name, tree, outline, center, ray, up=(0, 0, 1), lift=0.0025, color=INK, mat="flat",
              rings=(0.5, 1.0)):
    """A flat appliqué filled with concentric rings, every vertex conformed to a curved surface (so a
    big shape follows the curvature instead of sinking into it like a single fan polygon)."""
    ray = V(ray).normalized()
    rgt = ray.cross(V(up)).normalized()
    upv = rgt.cross(ray).normalized()
    c = V(center)
    bm = bmesh.new()

    def vert(u, v):
        q = c + rgt * u + upv * v
        loc, nrm = KK.on_surface(tree, q, ray)
        if loc is None:
            loc = q
        return bm.verts.new(loc - ray * lift)
    mid = vert(0.0, 0.0)
    rows = [[vert(u * f, v * f) for u, v in outline] for f in rings]      # outline is CCW seen along ray
    n = len(outline)
    faces = [(mid, rows[0][i], rows[0][(i + 1) % n]) for i in range(n)]
    for A, Bq in zip(rows[:-1], rows[1:]):
        faces += [(A[i], Bq[i], Bq[(i + 1) % n], A[(i + 1) % n]) for i in range(n)]
    edge = rows[-1]                                     # a short wall down into the surface round the edge
    low = [bm.verts.new(v.co + ray * 0.006) for v in edge]
    faces += [(edge[i], low[i], low[(i + 1) % n], edge[(i + 1) % n]) for i in range(n)]
    for fv in faces:
        bm.faces.new(fv)
    return D.mesh(bm, name, color, mat, "box", smooth=True)


def blob_on(name, tree, x, z, rad, back=False, roll=0.0, color=W, segs=10):
    ray = V((0, -1, 0)) if back else V((0, 1, 0))
    loc, nrm = KK.on_surface(tree, (x, -0.3 * ray.y, z), ray)
    b = dblob(name, rad, segs=segs, rings=4, color=color)
    D.put(b, loc + nrm * rad[2] * 0.15, nrm, roll=roll)
    return b


# ------------------------------------------------------------------ skull on the hood
# The skull is the whole top of the head (the panda lesson: no black lumps above or beside it, sockets on
# its FRONT face): a padded white cranium over the crown, the temples and the upper sides, coming well
# down the back, with the black hood only framing the face from the teeth line down round the cheeks
# and jaw (and a black rim under it at the back).
HOOD_WARP = dome(0.42, 1.9)                   # a taller hood with an upright forehead for the skull's face
TEETH_Z = 0.995                              # top of the teeth row (under the skull's lower edge)
# skull edge as (azimuth from the front, polar angle from the crown) in degrees: along the teeth line at
# the front, stepping round the face rim, down to ear level at the sides and well down the back
EDGE = [(0, 55.0), (14, 55.0), (26, 57.0), (36, 65.0), (45, 77.0), (56, 90.0), (90, 100.0), (135, 110.0), (180, 116.0)]
SOCK_C = (0.082, 1.056)                      # eye socket centre (x, z), seen from the front
NOSE_Z = 1.008


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
    # an upright forehead for the sockets: push the front of the cranium forward above the teeth (fading
    # out with the padding toward the tucked edge, so the skull still meets the hood there)
    fwd = 0.02 * math.exp(-((th - 31) / 15) ** 2) * max(math.cos(f), 0.0) ** 1.2 * min(max(o + 0.004, 0.0) / 0.03, 1.0)
    return HOOD_WARP(p) + V((0, -fwd, 0))


def puff(th, ph):
    """Skull padding height: a pillow tucked into the hood all round its edge, with a fuller brow and
    slight cheekbone bulges beside the sockets."""
    a = abs(ph)
    w = 7.0 + 20.0 * min(a / 90.0, 1.0)      # tight behind the teeth, a long soft taper at the sides and back
    t = min(max((edge(ph) - th) / w, 0.0), 1.0)
    fr = max(math.cos(math.radians(min(a, 90.0))), 0.0) ** 1.5 * min(th / 35.0, 1.0)  # front, fading at the crown
    top = 0.011 + 0.015 * fr                     # thin at the sides and back: no brim
    top += 0.008 * math.exp(-((th - 36) / 16) ** 2) * math.exp(-(a / 30) ** 2)
    top += 0.007 * math.exp(-((th - 60) / 13) ** 2) * math.exp(-((a - 50) / 13) ** 2)
    return -0.004 + (top + 0.004) * math.sqrt(1 - (1 - t) ** 2)


def mirror_ccw(base_cw, sx):
    """A CW outline drawn for the socket on the viewer's right (+X): CCW outline for either side."""
    return list(reversed(base_cw)) if sx > 0 else [(-u, v) for u, v in base_cw]


def build_skull():
    NPH, NTH = 40, 12
    bm = bmesh.new()
    pole = bm.verts.new(skull_pt(0.0, 0.0, puff(0.0, 0.0)))
    rings = []
    for j in range(1, NTH + 1):
        s = 0.5 * j / NTH + 0.5 * math.sin(0.5 * math.pi * j / NTH)          # denser toward the edge
        ring = []
        for i in range(NPH):
            ph = -180.0 + 360.0 * i / NPH
            th = s * (50.0 + (edge(ph) - 50.0) * s * s)                      # even rings round the crown
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
    # triangulate here so the BVH (sockets, nose) and the exported mesh split the curved quads the same way
    bmesh.ops.triangulate(bm, faces=bm.faces[:], quad_method="BEAUTY")
    sk = D.mesh(bm, "skull", W, "flat", "sphere", center=KK.HC)
    tree = D.bvh(sk)
    parts = [sk]
    ray = V((0, 1, -0.42))           # seen from about the gameplay camera's elevation
    # big eye sockets on the skull's front face: rounded, wider at the top-outer corner, tapering to a
    # rounded point at the bottom-inner corner
    base = [(-0.045, 0.032), (0.042, 0.04), (0.054, -0.008), (0.008, -0.048), (-0.047, -0.022)]       # CW
    sock = [(u * 1.2, v * 1.2) for u, v in KK.rounded_poly(base, 0.027, 3)]
    for sx in (1, -1):
        parts.append(conf_disc(f"socket{sx}", tree, mirror_ccw(sock, sx), (SOCK_C[0] * sx, -0.25, SOCK_C[1]), ray,
                               lift=0.0045, rings=(0.25, 0.5, 0.75, 1.0)))
    # nose cavity: a dark upside-down heart between and below the sockets
    heart = KK.rounded_poly([(0.0, -0.025), (0.027, 0.008), (0.016, 0.021), (0.0, 0.012), (-0.016, 0.021),
                             (-0.027, 0.008)], 0.006, 2)
    parts.append(conf_disc("nose", tree, heart, (0.0, -0.25, NOSE_Z), ray, lift=0.004, rings=(0.5, 1.0)))
    # upper jaw: a thin dark gap line under the skull's lower edge, then a row of seven big teeth
    line = []
    for k in range(13):
        x = -0.112 + 0.224 * k / 12
        p, n = KK.ellipsoid_pt(KK.HC, KK.HR, x, TEETH_Z + 0.002)
        fwd = V((n.x, n.y, 0)).normalized()
        line.append(HOOD_WARP(p) + fwd * 0.03)
    parts.append(D.sweep("jaw_gap", line, [0.0] + [0.0062] * 11 + [0.0], sides=5, color=INK, mat="flat",
                         project="box"))
    for i, x in enumerate((-0.087, -0.058, -0.029, 0.0, 0.029, 0.058, 0.087)):
        k = abs(x) / 0.087
        hw, hh = 0.0119 - 0.001 * k, 0.0235 - 0.004 * k * k
        p, n = KK.ellipsoid_pt(KK.HC, KK.HR, x, TEETH_Z - hh)
        fwd = V((n.x, n.y, 0)).normalized()
        tooth = K.polygon_prism(f"tooth{i}", KK.rounded_poly([(-hw, -hh), (hw, -hh), (hw, hh), (-hw, hh)], 0.006, 2),
                                0.012, color=TOOTH, mat="flat")
        D.put(tooth, HOOD_WARP(p) + fwd * 0.031, fwd + V((0, 0, 0.08)), up=(0, 0, 1))
        parts.append(tooth)
    return parts


# ------------------------------------------------------------------ pelvis print
def pelvis(tree, back):
    """Two iliac wings (thick white rings round a small black hole, tipped up at the outer side) meeting
    a rounded sacrum in the middle: the butterfly pelvis of the reference."""
    tag = "b" if back else ""
    parts = []
    for sx in (1, -1):
        cv = []
        for i in range(14):
            a = 2 * math.pi * i / 14
            u, v = 0.044 * math.cos(a), 0.026 * math.sin(a) * (1.0 + 0.25 * math.cos(a))
            r = math.radians(20)
            cv.append((sx * (0.07 + u * math.cos(r) - v * math.sin(r)), 0.33 + u * math.sin(r) + v * math.cos(r)))
        parts.append(print_band(f"{tag}ilium{sx}", tree, cv, 0.0155, 0.0075, back=back, closed=True))
    parts.append(blob_on(f"{tag}sacrum", tree, 0.0, 0.318, (0.024, 0.034, 0.0085), back=back))
    return parts


# ------------------------------------------------------------------ gloves: finger bones on the backs
def finger_bones(pts, rr, d):
    hand = pts[-1]
    out = 1 if hand.x > 0 else -1
    mc = hand + d * 0.04
    R = V((0.042, 0.04, 0.05))
    M = D.look(d, up=(0, -1, 0))
    psi0 = math.atan2(0.42, out * 0.9)       # the back of the hand: outward, turned a little to the front
    parts = []

    def on_hand(psi, zeta, lift):
        s = math.sqrt(max(1 - zeta * zeta, 0.0))
        q = V((math.cos(psi) * s * R.x, math.sin(psi) * s * R.y, zeta * R.z))
        nl = V((q.x / R.x ** 2, q.y / R.y ** 2, q.z / R.z ** 2)).normalized()
        n = (M @ nl).normalized()
        return mc + M @ q + n * lift, n
    for k in range(4):
        psi = psi0 + (k - 1.5) * 0.36 * out
        for j, (zeta, ln) in enumerate(((-0.08, 0.0125), (0.38, 0.0105))):
            p, n = on_hand(psi, zeta, 0.0)
            tip, _ = on_hand(psi, zeta + 0.2, 0.0)
            b = dblob(f"fbone{k}{j}", (0.0078, ln * 1.12, 0.0052), segs=6, rings=4)
            D.put(b, p + n * 0.001, n, up=tip - p)
            parts.append(b)
    # back-of-hand bone (metacarpal pad)
    p, n = on_hand(psi0, -0.55, 0.0)
    tip, _ = on_hand(psi0, -0.35, 0.0)
    b = dblob("mcarp", (0.019, 0.012, 0.0055), segs=8, rings=4)
    D.put(b, p + n * 0.001, n, up=tip - p)
    parts.append(b)
    return parts


# ================================================================== SKELETON
def build():
    # --- black hood with a white jaw band around the face and the big skull on top
    head = KK.hood(BLK, rim_color=BLK, stitches=False, mat=FELT, rim_mat=FELT, rim_r=0.032, warp=HOOD_WARP)
    K.paint(head[1], W, where=lambda c, n: c.z < 0.93 or abs(c.x) > 0.1, mat="flat")
    head += build_skull()
    head += KK.face(fang=False)
    head += KK.fringe()
    head = D.part(head, "head", KK.NECK)
    KK.tilt_head(head)

    # --- body with the puffy bone print
    tors = KK.torso(BLK)
    tree = D.bvh(tors)
    body = [tors]
    H = 0.0075
    # sternum: a rounded manubrium at the collar and the body of the sternum
    body.append(print_band("manubrium", tree, [(0.0, 0.592), (0.0, 0.575)], 0.017, H))
    body.append(print_band("sternum", tree, [(0.0, 0.55), (0.0, 0.5), (0.0, 0.452)], 0.0125, H))
    # four pairs of ribs curving out from the sternum and down round the sides
    for k, (z0, w, hw) in enumerate([(0.565, 0.86, 0.0125), (0.522, 0.88, 0.0125), (0.479, 0.86, 0.012),
                                     (0.436, 0.78, 0.0115)]):
        for sx in (1, -1):
            cv = []
            for t in range(8):
                u = t / 7
                z = z0 + 0.012 * u - 0.052 * u * u
                x = (0.026 + u * (w * KK.torso_r(z) - 0.026)) * sx
                cv.append((x, z))
            body.append(print_band(f"rib{k}{sx}", tree, cv, hw, H))
    # lumbar vertebrae under the ribcage
    for i, z in enumerate((0.413, 0.385)):
        body.append(blob_on(f"lumbar{i}", tree, 0.0, z, (0.026, 0.0115, 0.0085)))
    # pelvis: two rings (white wings round a black hole) and the sacrum between them
    body.extend(pelvis(tree, False))

    # back print (the gameplay camera mostly sees the back): spine, shoulder blades, back ribs, pelvis
    for i, z in enumerate((0.59, 0.55, 0.51, 0.47, 0.43, 0.39, 0.35)):
        body.append(blob_on(f"vert{i}", tree, 0.0, z, (0.027 - i * 0.001, 0.0135, 0.0085), back=True))
    for sx in (1, -1):
        body.append(blob_on(f"scap{sx}", tree, 0.088 * sx, 0.556, (0.042, 0.03, 0.0085), back=True, roll=-24 * sx))
    for k, (z0, w) in enumerate([(0.495, 0.86), (0.452, 0.84), (0.41, 0.76)]):
        for sx in (1, -1):
            cv = []
            for t in range(7):
                u = t / 6
                z = z0 + 0.01 * u - 0.045 * u * u
                x = (0.04 + u * (w * KK.torso_r(z) - 0.04)) * sx
                cv.append((x, z))
            body.append(print_band(f"brib{k}{sx}", tree, cv, 0.0115, H, back=True))
    body.extend(pelvis(tree, True))
    body = D.part(body, "body", (0, 0, KK.HIP_Z))

    # --- arms: humerus and the two forearm bones along the outside of each sleeve, black gloves
    def arm_extras(pts, rr, d):
        out = []
        sx = 1 if pts[0].x > 0 else -1
        side = V((sx, -0.6, 0.12)).normalized()

        def at(i, off=0.0):
            tg = (pts[min(i + 1, len(pts) - 1)] - pts[max(i - 1, 0)]).normalized()
            o = (side - tg * side.dot(tg)).normalized()
            bo = tg.cross(o)
            return pts[i] + o * rr[i] + bo * off, o
        a, na = at(1)
        b, nb = at(3)
        out.append(pbone("humerus", a, b, (na + nb).normalized(), r=0.0115))
        for k, off in enumerate((0.011, -0.011)):
            a, na = at(4, off)
            b, nb = at(6, off * 0.7)
            b = b - (b - a).normalized() * 0.016
            out.append(pbone(f"forearm{k}", a, b, (na + nb).normalized(), r=0.0078, knob=1.5))
        return out + finger_bones(pts, rr, d)

    def cuff(i, j):
        return ("cat_black", FELT) if i == 5 else None
    arm_l = KK.arm("arm_l", KK.SHO_L, KK.HAND_L, BLK, paint=cuff, extras=arm_extras, hand_color=GLOVE, hand_mat=FELT)
    arm_r = KK.arm("arm_r", KK.SHO_R, KK.HAND_R, BLK, paint=cuff, extras=arm_extras, hand_color=GLOVE, hand_mat=FELT)

    # --- legs: femur and shin bones front and back, chunky black/white high-tops
    def thigh_r(z):
        prof = [(0.074, 0.125), (0.08, 0.17), (0.084, 0.24), (0.074, 0.305)]
        for (r0, z0), (r1, z1) in zip(prof[:-1], prof[1:]):
            if z0 <= z <= z1:
                return r0 + (r1 - r0) * (z - z0) / (z1 - z0)
        return 0.074

    def leg_bones(x):
        sx = 1 if x > 0 else -1
        out = []

        def tp(z, th, back=False):
            r = thigh_r(z)
            s = 1 if back else -1
            n = V((math.sin(th), s * math.cos(th), 0))
            return V((x, 0, z)) + n * r, n
        for nm, back in (("f", False), ("b", True)):
            a, na = tp(0.296, 0.2 * sx)
            b, nb = tp(0.24, -0.06 * sx)
            if back:
                a, na = tp(0.296, -0.2 * sx, True)
                b, nb = tp(0.24, 0.06 * sx, True)
            out.append(pbone(f"femur{nm}", a, b, (na + nb).normalized(), r=0.0115))
            if not back:
                a, na = tp(0.227, -0.08 * sx)
                b, nb = tp(0.192, -0.1 * sx)
                out.append(pbone(f"shin{nm}", a, b, (na + nb).normalized(), r=0.0095))
        return out
    shoe = dict(upper="cat_black", panel=W, toe=W, sole=W, stripe="candy_grape", lace=W, collar="charcoal",
                tongue="cat_black", heel="candy_grape")
    leg_l = KK.leg("leg_l", KK.HIP_L, BLK, BLK, W, extras=leg_bones, shoe=shoe)
    leg_r = KK.leg("leg_r", KK.HIP_R, BLK, BLK, W, extras=leg_bones, shoe=shoe)
    return dict(body=body, head=head, leg_l=leg_l, leg_r=leg_r, arm_l=arm_l, arm_r=arm_r)
