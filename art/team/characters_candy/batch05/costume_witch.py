"""kid_witch v2: the Little Witch on kit v2 (design: batch04/ref/ref_witch.png; v1: batch04/costume_witch.py).

A big pointed purple felt hat (bob_hat, origin mid-seat: a wide brim with a rolled edge and a darker
underside, a wide orange band with piped edges and a puffy yellow star) whose cone flops over to the kid's
right and curls up at the tip (soft_tip, origin where the cone starts to flop: the rig whips it); warm
chestnut hair in layered FLAT locks with caramel highlight streaks (soft_hair_back, soft_hair_l,
soft_hair_r, origins at the roots: they hang and lag), a side-swept fringe, the kid's own face v2 (no
fang) and ears; a purple dress (plush bodice, dark belt with a gold buckle) with a layered skirt (soft_skirt,
origin at the waist on the body axis: a purple overskirt in soft folds with an orange hem band over a
gathered orange ruffle); a purple cape (soft_cape, origin at the clasp height on the body axis: a rolled
collar round the neck with the gold star clasp, the cape hanging behind the arms down over the back of
the skirt, darker lining); orange lapels under the clasp and a purple ribbon bow with long tails (bob_bow);
purple sleeves with big flared orange cuffs; orange-and-purple striped stockings; purple high-tops with
lilac soles, orange laces and a yellow star. No broom (the wand stays), no wings.

Cloth never drapes over the arms (user, 2026-11-15, for the ghost): the cape hangs from the back of the
shoulders behind the shoulder balls and stays narrower than the arms' swing; the collar sits on the
neck between the shoulder balls.
"""
import math

import bmesh
from mathutils import Matrix

import kid_kit as KK
from kid_kit import D, K, V, PLUSH

PURPLE, PURPLE_DK, PURPLE_LT, LILAC = "witch_purple", "hat_violet", "banner_purple", "candy_grape"
# the hat: a deep violet felt (witch_purple read pink-lilac under the showcase key light), the brim's underside a
# darker shade of it (banner_purple read pale pink); matte in the engine (kid_rig.cpp: KidTuning::hat_gloss)
HAT, HAT_UNDER = "hat_violet", "hat_violet_deep"   # (spider_plum read grey; hat_violet_deep: set_palette_witch.py)
ORANGE, ORANGE_LT, ORANGE_DK = "candy_orange", "candy_orange_light", "pumpkin_dark"
BELT, GOLD, GOLD_DK, STAR = "spider_plum", "foil_gold", "foil_gold_dark", "candy_lemon"
# chestnut (as BEFORE and the mock): cinnamon / caramel read ginger under the showcase's key light, and hair_brown with
# hair_light streaks still read light caramel there (polish round 1); a darker, more saturated chestnut, a choco streak
# (round 3: once the hair went matte (KidTuning::hair_gloss) sunflower_center read ginger in the face beat: the sky
# sheen had been cooling it; one step darker, sunflower_center becomes the streak)
HAIR, HAIR_LT, HAIR_DK = "choco_dark", "sunflower_center", "hair_dark"
SHOE = dict(upper=PURPLE, panel=LILAC, toe=PURPLE_LT, sole="house_lavender", stripe=PURPLE_DK, lace=ORANGE,
            collar=LILAC, tongue=PURPLE, heel=ORANGE, mat="vinyl")

# head
HY = 0.04                                   # the scalp's centre (Blender y)
SCALP_C, SCALP_R = V((0, HY, 0.80)), V((0.24, 0.235, 0.2))
# hat: everything is modelled in hat space (seat plane z = 0, axis z) and put on the head by HAT_M
SEAT = V((0.0, 0.022, 0.975))               # bob_hat's origin: the middle of its seat on the head
HAT_M = Matrix.Translation(SEAT) @ Matrix.Rotation(math.radians(-8), 4, "X") @ Matrix.Rotation(math.radians(-7), 4, "Y")
CONE = [(0.0, 0.0, -0.012), (0.0, 0.012, 0.10), (-0.01, 0.026, 0.19), (-0.032, 0.04, 0.262), (-0.078, 0.05, 0.312),
        (-0.14, 0.055, 0.334), (-0.2, 0.054, 0.322), (-0.243, 0.05, 0.296), (-0.27, 0.046, 0.288), (-0.288, 0.042, 0.302)]
CONE_R = [0.192, 0.158, 0.12, 0.09, 0.068, 0.05, 0.036, 0.024, 0.013, 0.0]
TIP_FROM = 9                                # soft_tip starts at this ring of the cone (per=3: ring 9 = CONE[3])
BAND = (-0.006, 0.072)                      # the band's height on the cone (hat space)
# body
WAIST, HEM = 0.465, 0.296
COLLAR_Z = 0.598                            # the collar's mean height (lower in front, higher behind)
CLASP = V((0.0, -0.146, 0.598))             # on the front of the flared collar (cape_collar), facing up and out
CAPE_PIN = V((0.0, 0.0, 0.625))
SKIRT_PIN = V((0.0, 0.0, WAIST))
BOW_C, BOW_N = V((0.0, -0.162, 0.548)), V((0.0, -1.0, 0.2))
SWAY = 0.0


def smooth01(t):
    t = min(max(t, 0.0), 1.0)
    return t * t * (3 - 2 * t)


# ------------------------------------------------------------------ helpers
def puffy_star(name, R, h, color, mat="flat", inner=0.48, n=5):
    """A chunky stuffed star: flat back on local z = 0, a straight side wall and a pillowy front rising to
    a soft point at z = h. Points up local +Y."""
    bm = bmesh.new()

    def ring(scale, z):
        out = []
        for i in range(2 * n):
            rad = (R if i % 2 == 0 else R * inner) * scale
            a = math.pi / 2 + i * math.pi / n
            out.append(bm.verts.new((math.cos(a) * rad, math.sin(a) * rad, z)))
        return out
    r0, r1, r2, r3 = ring(1.0, 0.0), ring(1.0, h * 0.4), ring(0.8, h * 0.72), ring(0.45, h * 0.93)
    top = bm.verts.new((0, 0, h))
    m = 2 * n
    bm.faces.new(list(reversed(r0)))
    for A, B in ((r0, r1), (r1, r2), (r2, r3)):
        for j in range(m):
            bm.faces.new((A[j], A[(j + 1) % m], B[(j + 1) % m], B[j]))
    for j in range(m):
        bm.faces.new((r3[j], r3[(j + 1) % m], top))
    bmesh.ops.recalc_face_normals(bm, faces=bm.faces)
    return D.mesh(bm, name, color, mat, "box", smooth=True, center=V((0, 0, h / 2)))


def loop_lathe(name, prof, segs, color, mat=PLUSH, warp=None, paint=None, project="cyl", center=None):
    """Revolve a CLOSED (r, z) loop (a thick cloth ring: brim, skirt, ruffle, cuff) about local z.
    paint(i) -> colour or None for the faces of profile segment i (prof[i] -> prof[i + 1])."""
    bm = bmesh.new()
    rings = [[bm.verts.new((math.cos(2 * math.pi * j / segs) * r, math.sin(2 * math.pi * j / segs) * r, z))
              for j in range(segs)] for r, z in prof]
    n = len(rings)
    groups = {}
    for i in range(n):
        A, B = rings[i], rings[(i + 1) % n]
        col = paint(i) if paint else None
        for j in range(segs):
            j2 = (j + 1) % segs
            f = bm.faces.new((A[j], A[j2], B[j2], B[j]))
            if col:
                groups.setdefault(col, []).append(f)
    if warp:
        for v in bm.verts:
            v.co = warp(v.co.copy())
    bmesh.ops.recalc_face_normals(bm, faces=bm.faces)
    extra = [(fs, c, mat) for c, fs in groups.items()]
    return D.mesh(bm, name, color, mat, project, True, center=center, extra=extra)


def resample(prof, per=2):
    """Catmull-Rom resample of an (r, z) polyline (ends kept)."""
    pts, _ = KK.catmull([(r, z, 0.0) for r, z in prof], per=per)
    return [(p.x, p.y) for p in pts]


def drop_caps(ob, sides):
    """Delete the flat end caps (n-gons) D.sweep puts on open tube ends: two tubes that meet on a
    shared ring then shade as one surface."""
    bm = bmesh.new()
    bm.from_mesh(ob.data)
    caps = [f for f in bm.faces if len(f.verts) == sides and sides > 4]
    bmesh.ops.delete(bm, geom=caps, context="FACES")
    bm.to_mesh(ob.data)
    bm.free()
    return ob


def split_rings(ob, sides, i0, i1):
    """Keep only the faces of a D.sweep tube between rings i0 and i1 (its ring faces come first, ring-major,
    then the end caps; a cap is kept only on an end that is kept). Both halves of one sweep share exact
    ring vertices and frames, so the two parts meet with no slit and shade as one surface."""
    bm = bmesh.new()
    bm.from_mesh(ob.data)
    bm.faces.ensure_lookup_table()
    n_ring = sum(1 for f in bm.faces if len(f.verts) <= 4)
    rings = n_ring // sides
    gone = []
    for k, f in enumerate(bm.faces):
        if k < n_ring:
            if not (i0 <= k // sides < i1):
                gone.append(f)
        elif not ((i0 == 0 and f.calc_center_median().z < 0.0) or (i1 > rings and f.calc_center_median().z > 0.0)):
            gone.append(f)
    bmesh.ops.delete(bm, geom=gone, context="FACES")
    bm.to_mesh(ob.data)
    bm.free()
    return ob


def wavy_lock(name, ctrl, w, out, thin=0.42, end_thin=0.42, sides=8, per=2, color=HAIR, streak=True, under=HAIR_DK,
              root=0.62, end_from=0.7, taper=0.0):
    """One chunky lock of wavy hair: a lens-shaped section (thin along 'out', wide across) through ctrl, narrower at
    the root (it tucks into the hair cap), widest down its middle and a ROUNDED end (a point reads as a claw from
    below). end_thin > thin makes the end rounder in section: a soft curl, not a flat strip. A streaked lock carries
    a lighter band down its outer face; the inner face is the darker under-layer, so overlapping locks read."""
    pts, par = KK.catmull(ctrl, per=per)
    rr = [w * (root + (1 - root) * smooth01(t * 3.2)) * (1.0 - taper * smooth01((t - 0.45) / 0.55)) for t in par]
    d = (pts[-1] - pts[-2]).normalized()
    re = rr[-1]
    pts += [pts[-1] + d * re * 0.38, pts[-1] + d * re * 0.62]
    rr += [re * 0.72, 0.0]
    par += [1.0, 1.0]
    last = len(pts) - 2

    def warp(a, i):
        t = par[min(i, len(par) - 1)]
        th = thin + (end_thin - thin) * smooth01((t - end_from) / max(1.0 - end_from, 1e-3))
        return 1.0 / math.sqrt((math.cos(a) / th) ** 2 + math.sin(a) ** 2)

    def paint(i, j):
        if streak and 2 <= i < last - 1 and j in (0, sides - 1):
            return (HAIR_LT, "flat")
        if j in (sides // 2 - 1, sides // 2):
            return (under, "flat")
        return None
    return D.sweep(name, pts, rr, sides=sides, color=color, mat="flat", project="box", up=out, warp=warp, paint=paint)


# ------------------------------------------------------------------ hair (head children, origins at the roots)
def at_head(phi, r, z, dphi=0.0):
    a = math.radians(phi + dphi)
    return V((math.cos(a) * r, HY + math.sin(a) * r * 0.97, z))


def out_dir(phi):
    return at_head(phi, 1, 0) - at_head(phi, 0, 0)


def fall_ctrl(phi, zend, z0=0.905, r0=0.19, bulge=0.266, r_end=0.292, wave=6.0, curl=1.0, inset=0.0, twist=0.0):
    """A lock from under the brim at angle phi round the head (0 = the kid's left, 90 = the back): out over the hair
    cap to its widest at the temple line, a soft S wave on the way down (sideways and in and out), and the end curling
    OUT and UP at zend (a big soft curl at the shoulder, as BEFORE and the mock), turned sideways by twist degrees
    (straight-out curls seen end-on from behind read as a row of beads)."""
    zm = (z0 + zend) / 2
    return [at_head(phi, r0 - inset, z0),
            at_head(phi, 0.248 - inset, z0 - 0.045, wave * 0.2),
            at_head(phi, bulge - inset, z0 - 0.115, -wave * 0.5),
            at_head(phi, bulge - 0.012 - inset, zm - 0.01, wave * 0.6),
            at_head(phi, r_end - 0.03 - inset, zend + 0.048, -wave * 0.3),
            at_head(phi, r_end - inset, zend + 0.004, wave * 0.1 + twist * 0.2),
            at_head(phi, r_end + 0.03 * curl - inset, zend + 0.008 * curl, wave * 0.2 + twist * 0.6),
            at_head(phi, r_end + 0.04 * curl - inset, zend + 0.034 * curl, wave * 0.25 + twist)]


def hair_back():
    """soft_hair_back: six thick outer locks round the back under the brim, wavy, overlapping, each ending in a big
    curl out at the shoulders (lengths, curls and their sideways turn varied; every other one streaked), over three
    darker inner locks that fill between them."""
    locks = []
    for k, phi in enumerate((62, 90, 118)):
        locks.append(wavy_lock(f"hb_in{k}", fall_ctrl(phi, 0.69, inset=0.022, wave=0.0, curl=0.3), 0.066, out_dir(phi),
                               color=HAIR_DK, streak=False, sides=6))
    for k, phi in enumerate((40, 59, 78, 102, 121, 140)):
        zend = 0.636 + (0.016, -0.014, 0.008, -0.02, 0.02, -0.006)[k]
        curl = (0.9, 1.15, 0.75, 1.1, 0.8, 1.05)[k]
        twist = (-14.0, 10.0, -8.0, 12.0, -10.0, 14.0)[k]
        locks.append(wavy_lock(f"hb{k}", fall_ctrl(phi, zend, wave=4.5 if k % 2 else -4.5, curl=curl, twist=twist), 0.064,
                               out_dir(phi), streak=k % 2 == 0, end_thin=0.5, sides=10))
    ob = K.merge(locks, "hair_back_m")
    under_brim(ob)
    return KK.soft_part("soft_hair_back", [ob], V((0.0, 0.2, 0.9)))


def hair_side(s):
    """soft_hair_l / _r: a face-framing lock in front of the ear down beside the cheek, curling out at the jaw, and
    two thick locks over and behind the ear to the shoulder, curling out."""
    sx = 1 if s == "l" else -1

    def ph(p):
        return p if sx > 0 else 180 - p
    locks = [wavy_lock(f"hs{s}f", fall_ctrl(ph(-30), 0.69, z0=0.915, r0=0.2, bulge=0.258, r_end=0.264, wave=3.5 * sx,
                                            curl=0.45, twist=-8.0 * sx), 0.052, out_dir(ph(-30)), streak=True, end_thin=0.42,
                       sides=10)]
    for k, (phi, zend, w, tw) in enumerate(((-6, 0.66, 0.06, 10.0), (19, 0.634, 0.063, -12.0))):
        locks.append(wavy_lock(f"hs{s}{k}", fall_ctrl(ph(phi), zend, wave=(-4.0 if k else 4.0) * sx, curl=0.7 + 0.15 * k,
                                                       twist=tw * sx), w, out_dir(ph(phi)), streak=k == 1, end_thin=0.5,
                               sides=10))
    ob = K.merge(locks, f"hair_{s}_m")
    under_brim(ob)
    return KK.soft_part("soft_hair_" + s, [ob], at_head(ph(8), 0.21, 0.9))


BRIM_UNDER = [(0.165, -0.004), (0.176, -0.004), (0.235, -0.024), (0.3, -0.043), (0.355, -0.058), (0.381, -0.06), (0.4, -0.06)]


def brim_dz(x, y):
    """The brim's wave up and down round the head (hat space), as hat() bends it."""
    a = math.atan2(y, x)
    k = max(0.0, (math.hypot(x, y) - 0.2) / 0.19)
    return k * k * (0.024 * math.sin(3 * a + 0.8) + 0.011 * math.sin(5 * a + 2.0) - 0.014 * max(0.0, -math.sin(a))
                    + 0.02 * max(0.0, math.sin(a)))


def under_brim(ob, gap=0.005):
    """Push any hair vertex that would show above the brim (outside the cone, over the brim's underside) down under
    it, in hat space at the hat's rest seat: the hat tips back and down on the right, so the front roots and the
    right temple came up through the brim beside the band."""
    Mi = HAT_M.inverted()
    W = ob.matrix_world.copy()
    Wi = W.inverted()
    for v in ob.data.vertices:
        h = Mi @ (W @ v.co)
        r = math.hypot(h.x, h.y)
        if 0.165 <= r <= 0.4:
            zl = 0.0
            for (r0, z0), (r1, z1) in zip(BRIM_UNDER[:-1], BRIM_UNDER[1:]):
                if r0 <= r <= r1:
                    zl = z0 + (z1 - z0) * (r - r0) / (r1 - r0)
                    break
            zl += brim_dz(h.x, h.y) - gap
            if h.z > zl:
                h.z = zl
                v.co = Wi @ (HAT_M @ h)
    return ob


def face_exit(p0, d):
    """Distance along the unit vector d from p0 to where the ray leaves the face's ellipsoid (0 if it misses)."""
    o = V(((p0.x - KK.FC.x) / KK.FR.x, (p0.y - KK.FC.y) / KK.FR.y, (p0.z - KK.FC.z) / KK.FR.z))
    v = V((d.x / KK.FR.x, d.y / KK.FR.y, d.z / KK.FR.z))
    a, b, c = v.dot(v), 2 * o.dot(v), o.dot(o) - 1
    disc = b * b - 4 * a * c
    if disc < 0:
        return 0.0
    return max((-b + math.sqrt(disc)) / (2 * a), 0.0)


CAP_C, CAP_R = V((0.0, 0.03, 0.81)), V((0.25, 0.248, 0.205))


def hair_cap():
    """The hair's volume under the hat: a rounded cap over the skull that comes forward over the top and sides of the
    face's ellipsoid (9 mm proud of it) down to a hairline round the face, so no skin shows between the fringe, the
    side locks and the brim from any camera. The front band is the hair colour, the rest the darker under-layer
    (it shows only between locks). The ears are under it (not modelled)."""
    def warp(p):
        d = p - CAP_C
        L = d.length
        u = d / L
        return CAP_C + u * max(L, face_exit(CAP_C, u) + 0.009)

    def cut(c):
        if c.z < 0.655 or (c.y < 0.02 and c.z < 0.70):
            return True                         # the nape's bottom edge, and under the jaw
        if c.y > -0.03:
            return False
        rz = 0.15 if c.z > 0.79 else 0.2        # the hairline: down beside the eyes at the temples
        return (c.x / 0.188) ** 2 + ((c.z - 0.79) / rz) ** 2 < 1.0
    cap = KK.blob("hair_cap", CAP_C, CAP_R, segs=24, rings=14, color=HAIR_DK, mat="flat", cut=cut, warp=warp)
    snap_hairline(cap)
    K.paint(cap, HAIR, where=lambda c, n: (c + CAP_C).y < -0.04 and (c + CAP_C).z > 0.74)
    return [under_brim(cap)]


def snap_hairline(cap):
    """Move the cap's open-edge vertices round the face onto the hairline ellipse (the face cut removes whole faces,
    which left a stepped edge: a square notch of skin at the temple), each lying on the face 9 mm proud."""
    bm = bmesh.new()
    bm.from_mesh(cap.data)
    c0 = cap.location.copy()
    for v in bm.verts:
        if not any(e.is_boundary for e in v.link_edges):
            continue
        w = v.co + c0
        if w.y > -0.03 or w.z < 0.70:
            continue
        dz = w.z - 0.79
        rz = 0.15 if dz > 0 else 0.2
        a = math.atan2(dz / rz, w.x / 0.188)
        x, z = 0.188 * math.cos(a), 0.79 + rz * math.sin(a)
        d = V((x, CAP_C.y, z)) - CAP_C
        d.y = 0.0
        p, n = KK.ellipsoid_pt(KK.FC, KK.FR, x, z)
        q = 1 - (x / KK.FR.x) ** 2 - ((z - KK.FC.z) / KK.FR.z) ** 2
        if q > 0.0:
            v.co = p + n * 0.009 - c0
    bm.to_mesh(cap.data)
    bm.free()
    return cap


def fringe():
    """A full side-swept fringe of five thick bangs from a parting on the kid's right: one short sweep to the right
    temple and four lapping across the forehead to the left temple (each lifted over the one before), every other
    one streaked. On the face's ellipsoid, over the hair cap; the outer two end on the temples."""
    out = []
    spec = [(-0.072, 0.962, -0.19, 0.852, -0.012, 0.046),
            (-0.064, 0.968, -0.03, 0.889, 0.0, 0.05),
            (-0.04, 0.97, 0.062, 0.884, 0.0, 0.054),
            (-0.008, 0.97, 0.136, 0.873, 0.0, 0.054),
            (0.035, 0.966, 0.194, 0.846, 0.0, 0.05)]
    for k, (x0, z0, x1, z1, bend, r0) in enumerate(spec):
        ctrl = []
        for i in range(5):
            s = i / 4
            x = x0 + (x1 - x0) * s + bend * math.sin(math.pi * s)
            z = z0 + (z1 - z0) * s ** 1.6 + 0.01 * math.sin(math.pi * s)
            p, n = KK.ellipsoid_pt(KK.FC, KK.FR, x, z)
            ctrl.append(p + n * (0.014 + 0.016 * math.sin(math.pi * s) + 0.003 * k))
        _, n0 = KK.ellipsoid_pt(KK.FC, KK.FR, (x0 + x1) / 2, (z0 + z1) / 2)
        # (round 3: thin 0.36 / taper 0.35 read as a row of separate glossy drips in the face beat: each lens's round
        # edges shaded dark against its neighbour; a flatter section lies over the next bang and the tips taper to
        # soft clumps, so the fringe reads as one swept mass divided into a few bangs)
        out.append(under_brim(wavy_lock(f"bang{k}", ctrl, r0 * 1.16, n0, thin=0.26, end_thin=0.3, root=0.8, taper=0.5,
                                        streak=k % 2 == 1)))
    return out


# ------------------------------------------------------------------ the hat (bob_hat > soft_tip)
def hat():
    parts = []
    # wide brim: a thick felt ring with a rolled edge, the top violet, the underside a darker violet (it
    # is seen from below in the shade, and must not sink to black); gently waving up and down round the head
    prof = [(0.178, 0.014), (0.235, 0.009), (0.3, -0.003), (0.352, -0.02), (0.381, -0.035),
            (0.391, -0.05), (0.381, -0.06), (0.355, -0.058), (0.3, -0.043), (0.235, -0.024), (0.176, -0.004)]
    under = set(range(6, 10))

    def brim_warp(c):
        return V((c.x, c.y, c.z + brim_dz(c.x, c.y)))
    parts.append(loop_lathe("brim", prof, 40, HAT, mat=PLUSH, warp=brim_warp,
                            paint=lambda i: HAT_UNDER if i in under else None, center=(0, 0, 0)))
    # the cone, in two parts that meet on one shared ring: the lower cone (with the hat) and the flopping
    # tip (soft_tip); no caps where they meet, so they shade as one surface
    pts, par = KK.catmull(CONE, per=3)
    rr = [KK.lerp_list(CONE_R, t) for t in par]
    lower = split_rings(D.sweep("cone_lo", pts, rr, sides=18, color=HAT, mat=PLUSH, project="cyl", up=(0, 1, 0)),
                        18, 0, TIP_FROM)
    tip = split_rings(D.sweep("cone_tip", pts, rr, sides=18, color=HAT, mat=PLUSH, project="cyl", up=(0, 1, 0)),
                      18, TIP_FROM, len(pts))
    parts.append(lower)
    # the wide orange band hugging the cone base, piped at both edges
    fine, fpar = KK.catmull(CONE, per=7)
    bp, brr = [], []
    for p, t in zip(fine, fpar):
        if BAND[0] <= p.z <= BAND[1]:
            bp.append(p)
            h = (p.z - BAND[0]) / (BAND[1] - BAND[0])
            brr.append(KK.lerp_list(CONE_R, t) + 0.007 + 0.004 * math.sin(math.pi * h))
    parts.append(D.sweep("band", bp, brr, sides=32, color=ORANGE, mat="flat", project="cyl"))
    for k, (p, r) in enumerate(((bp[0], brr[0]), (bp[-1], brr[-1]))):
        ring = [p + V((math.cos(a) * r, math.sin(a) * r, 0)) for a in (2 * math.pi * i / 32 for i in range(32))]
        parts.append(D.sweep(f"band_edge{k}", ring, 0.0058, sides=4, color=ORANGE_LT, mat="flat", closed=True, center=p))
    # the big puffy yellow star on the band, front and to the kid's right (as the mock)
    a = math.radians(-118)
    zc = (BAND[0] + BAND[1]) / 2
    rr0 = KK.lerp_list(CONE_R, 0.02) + 0.011
    nrm = V((math.cos(a), math.sin(a), 0.12)).normalized()
    st = puffy_star("hat_star", 0.072, 0.03, STAR, mat="flat")
    D.put(st, V((math.cos(a) * rr0, math.sin(a) * rr0, zc)) - nrm * 0.002, nrm, up=(0, 0, 1), roll=-8)
    parts.append(st)
    hat_ob = K.merge(parts, "hat_m")
    hat_ob.data.transform(HAT_M)
    tip.data.transform(HAT_M)
    pin = HAT_M @ V(pts[TIP_FROM])
    tip_part = KK.soft_part("soft_tip", [tip], pin)
    return [("bob_hat", KK.bob_part("bob_hat", [hat_ob], SEAT), "head"), ("soft_tip", tip_part, "bob_hat")]


# ------------------------------------------------------------------ head (origin at the neck)
def head():
    static, children = KK.face2(fang=False)
    parts = static
    # the hair cap and the fringe are their own static node under the head (round 3), so the rig can make all the
    # hair matte (KidTuning::hair_gloss) without touching the face's sheen
    children += [("hair_cap", D.part(hair_cap() + fringe(), "hair_cap", KK.NECK), "head")]
    children += [("soft_hair_back", hair_back(), "head"), ("soft_hair_l", hair_side("l"), "head"),
                 ("soft_hair_r", hair_side("r"), "head")]
    children += hat()
    return D.part(parts, "head", KK.NECK), children


# ------------------------------------------------------------------ body (origin at the pelvis)
def torso_y(c):
    """The torso's own squash (KK.torso_warp) for cloth that must hug it."""
    return KK.torso_warp(c)


def belt(tree):
    """Wide dark belt at the waist with a chunky gold buckle and a belt loop."""
    prof = [(KK.torso_r(0.444) + 0.002, 0.441), (KK.torso_r(0.446) + 0.009, 0.447), (KK.torso_r(0.478) + 0.009, 0.477),
            (KK.torso_r(0.48) + 0.002, 0.483)]
    bl = D.lathe("belt", prof, 32, color=BELT, mat=PLUSH, project="cyl", warp=KK.torso_warp, center=(0, 0, 0.46))
    out = [bl]
    loc, nrm = KK.on_surface(D.bvh(bl), (0.0, -0.4, 0.462), (0, 1, 0))
    frame = K.box("buckle", (0.07, 0.052, 0.013), color=GOLD, mat="metal", bevel=0.005)
    D.put(frame, loc + nrm * 0.004, nrm)
    hole = K.box("buckle_hole", (0.04, 0.026, 0.012), color=BELT, mat="flat", bevel=0.003)
    D.put(hole, loc + nrm * 0.0065, nrm)
    prong = K.box("buckle_prong", (0.03, 0.0065, 0.006), color=GOLD_DK, mat="metal", bevel=0.002)
    D.put(prong, loc + nrm * 0.011 + V((0.006, 0, 0)), nrm)
    out += [frame, hole, prong]
    l2, n2 = KK.on_surface(D.bvh(bl), (0.07, -0.4, 0.462), (0, 1, 0))
    loop_ = K.box("belt_loop", (0.013, 0.044, 0.01), color=BELT, mat="flat", bevel=0.003)
    D.put(loop_, l2 + n2 * 0.004, n2)
    out.append(loop_)
    return out


def lapels(tree):
    """Two orange collar points folded down either side of the bow, under the clasp."""
    out = []
    for sx in (1, -1):
        pts = [(0.0, 0.012), (0.085 * sx, 0.018), (0.038 * sx, -0.058)]
        if sx < 0:
            pts = list(reversed(pts))
        lap = K.polygon_prism(f"lapel{sx}", KK.rounded_poly(pts, 0.012, 3), 0.008, color=ORANGE, mat=PLUSH)
        # (polish: lowered 2.5 cm so they come out from under the broad collar; at 0.575 their corners poked out as horns)
        loc, nrm = KK.on_surface(tree, (0.014 * sx, -0.4, 0.55), (0, 1, 0.0))
        D.put(lap, loc + nrm * 0.002, nrm + V((0, 0, 0.25)), up=(0, 0, 1))
        out.append(lap)
    return out


def body():
    torso = KK.torso2(PURPLE, segs=28)
    # the top of the torso above the collar (the 'neck' under the chin) in the cape's violet, so neck and collar read as
    # one soft cape collar (polish: a lit purple neck over the collar band read as a neck brace in the face beat)
    mw = torso.matrix_world.copy()
    K.paint(torso, PURPLE_DK, where=lambda c, n: (mw @ c).z > COLLAR_Z + 0.004)
    tree = D.bvh(torso)
    parts = [torso] + belt(tree) + lapels(tree)
    return D.part(parts, "body", (0, 0, KK.HIP_Z))


# ------------------------------------------------------------------ skirt (soft_skirt, origin at the waist)
def skirt_warp(lobes=8, amp=0.05):
    def w(c):
        a = math.atan2(c.y, c.x)
        r = math.hypot(c.x, c.y)
        k = smooth01((WAIST - 0.02 - c.z) / 0.14)
        r *= 1 + k * amp * math.cos(lobes * a + 0.4)
        z = c.z + k * k * 0.007 * math.cos(lobes * a + 0.4)
        sy = r * math.sin(a)
        # the waist hugs the torso's squash (y x 0.88), the hem is rounder
        top = 1.0 - smooth01((WAIST - c.z) / 0.12)
        fy = (0.88 + 0.08 * (1 - top)) if sy < 0 else (0.88 + 0.1 * (1 - top))
        return V((r * math.cos(a), sy * fy + 0.012, z))
    return w


def skirt():
    """A purple overskirt in eight soft folds from the waist (under the belt) to the hem, an orange band
    round the hem and its rolled edge, over a gathered orange ruffle that peeks out 2.5 cm below. One mesh
    (both layers), so the hem ripple and the puff move them together."""
    out = [(0.199, WAIST), (0.206, 0.445), (0.218, 0.415), (0.236, 0.375), (0.255, 0.34), (0.27, 0.312), (0.277, HEM + 0.006)]
    outer = resample(out, 1)
    lip = [(0.281, HEM - 0.001), (0.279, HEM - 0.008), (0.271, HEM - 0.009)]
    inner = [(r - 0.007, z) for r, z in reversed(outer[:-1])]
    prof = outer + lip + inner
    n_out = len(outer)
    band_from = n_out - 3                           # the last two outer segments + the lip are orange

    def paint(i):
        if band_from <= i < n_out - 1 + len(lip):
            return ORANGE
        if i >= n_out - 1 + len(lip):
            return PURPLE_DK
        return None
    over = loop_lathe("overskirt", prof, 40, PURPLE, mat=PLUSH, warp=skirt_warp(), paint=paint, center=(0, 0, 0.38))
    # the ruffle: gathered in 18 small waves round, its top hidden under the overskirt
    rprof = [(0.24, 0.34), (0.277, HEM - 0.004), (0.29, HEM - 0.019), (0.294, HEM - 0.028),
             (0.284, HEM - 0.03), (0.268, HEM - 0.012), (0.234, 0.34)]

    def rwarp(c):
        a = math.atan2(c.y, c.x)
        r = math.hypot(c.x, c.y)
        out_k = smooth01((r - 0.255) / 0.035)
        r *= (1 + 0.05 * math.cos(8 * a + 0.4) * smooth01((WAIST - 0.02 - c.z) / 0.14)) * (1 + out_k * 0.04 * math.cos(18 * a))
        z = c.z - out_k * 0.006 * math.cos(18 * a)
        sy = r * math.sin(a)
        return V((r * math.cos(a), sy * (0.96 if sy < 0 else 0.98) + 0.012, z))
    ruf = loop_lathe("ruffle", rprof, 54, ORANGE, mat=PLUSH, warp=rwarp,
                     paint=lambda i: ORANGE_DK if i >= 4 else None, center=(0, 0, HEM))
    ob = K.merge([over, ruf], "skirt_m")
    return KK.soft_part("soft_skirt", [ob], SKIRT_PIN)


# ------------------------------------------------------------------ cape (soft_cape, origin at the clasp height on the axis)
def cape_r(v):
    """The cape's distance from the body axis down its length (v 0 top .. 1 hem): clear of the back, the
    belt and the skirt's back (the skirt is 0.29 out at its hem)."""
    return KK.lerp_list([0.148, 0.185, 0.215, 0.24, 0.262, 0.285, 0.305, 0.322], v)


def cape_collar(nc=28):
    """The cape's collar: a soft flared felt collar whose inner edge hugs the neck right under the chin (higher at the
    back) and which slopes out and down over the shoulders to a rounded lip, so the head sits on it as on the other
    kids' collars. Round 2's flat roll lay at the base of a 3 cm neck column (the torso's dome showed between the chin
    and the roll: a turtleneck / neck-brace read in the face beat). At the sides its lip runs into the shoulder balls;
    at the back the cape's top edge is tucked under it. Top face violet, underside darker."""
    # section loop: (u along the chord inner edge -> lip, offset off the chord; None = the top's soft bulge)
    sec = [(-0.06, 0.002), (0.22, None), (0.5, None), (0.78, None), (0.98, 0.006), (1.05, 0.0), (0.62, -0.006),
           (0.12, -0.004)]
    bm = bmesh.new()
    rings = []
    for i in range(nc):
        a = 2 * math.pi * i / nc
        sn, cs = math.sin(a), math.cos(a)
        front = max(0.0, -sn)
        zt = 0.65 + 0.017 * sn                          # behind the chin in front (0.633), 0.667 at the back
        ri = KK.torso_r(zt) - 0.003
        ro = ri + 0.058 + 0.01 * front + 0.003 * math.cos(7 * a + 0.3)
        zb = zt - (0.052 - 0.008 * abs(cs))
        dr, dz = ro - ri, zb - zt
        L = math.hypot(dr, dz)
        ur, uz = dr / L, dz / L
        nr, nz = -uz, ur                                # off the chord, out and up
        ring = []
        for u, off in sec:
            if off is None:
                off = 0.004 + 0.011 * math.sin(math.pi * u) ** 0.8
            r = ri + u * dr + nr * off
            z = zt + u * dz + nz * off
            ring.append(bm.verts.new((cs * r, 0.012 + sn * r * 0.88, z)))
        rings.append(ring)
    m = len(sec)
    under = []
    for i in range(nc):
        A, B = rings[i], rings[(i + 1) % nc]
        for j in range(m):
            j2 = (j + 1) % m
            f = bm.faces.new((A[j], A[j2], B[j2], B[j]))
            if j >= 5:
                under.append(f)
    bmesh.ops.recalc_face_normals(bm, faces=bm.faces)
    return D.mesh(bm, "collar", PURPLE_DK, PLUSH, "cyl", True, center=CAPE_PIN, extra=[(under, HAT_UNDER, PLUSH)])


def cape():
    """The cape: a soft rolled collar round the neck (low in front under the chin, higher at the back,
    hidden by the shoulder balls at the sides) fastened by the gold star clasp, and the cape itself hanging
    from the back of the shoulders, behind the arms, down over the back of the skirt; soft vertical folds,
    a darker lining. Never over the arms: at every height it stays inside the angles the arms swing back to."""
    from costume_common import panel
    parts = []
    col = cape_collar()
    parts.append(col)
    loc, nrm = KK.on_surface(D.bvh(col), (0.0, -0.4, CLASP.z), (0, 1, 0))
    print("WITCH clasp on the collar at", tuple(round(c, 4) for c in loc), "normal", tuple(round(c, 2) for c in nrm))
    # the clasp: a gold star on the collar's front
    st = puffy_star("clasp", 0.034, 0.016, GOLD, mat="metal")
    D.put(st, loc - nrm * 0.002, nrm, up=(0, 0, 1))
    parts.append(st)
    knot = KK.blob("clasp_back", (0, 0, 0), (0.018, 0.012, 0.018), segs=10, rings=5, color=GOLD_DK, mat="metal")
    D.put(knot, loc - nrm * 0.004, nrm)
    parts.append(knot)

    # the cape: from the back of the shoulders (angles 40..140 deg round the axis) widening to 16..164 at the hem
    def P(u, v):
        half = math.radians(50 + 24 * smooth01(v * 1.3))
        a = math.pi / 2 + (2 * u - 1) * half
        r = cape_r(v) * (1 + 0.045 * math.cos((2 * u - 1) * 5.5 * math.pi) * v ** 0.8)
        z = 0.616 - v * 0.322 + 0.018 * abs(2 * u - 1) ** 2.5 * v
        z += 0.008 * math.cos((2 * u - 1) * 5.5 * math.pi) * v ** 3
        return (math.cos(a) * r * 1.02, 0.012 + math.sin(a) * r, z)
    parts.append(panel("cape", P, 16, 10, 0.009, PURPLE_DK, BELT, out_mat=PLUSH, in_mat=PLUSH,
                       out_hint=lambda u, v: (math.cos(math.pi / 2 + (2 * u - 1)), math.sin(math.pi / 2 + (2 * u - 1)), 0.0)))
    ob = K.merge(parts, "cape_m")
    return KK.soft_part("soft_cape", [ob], CAPE_PIN)


# ------------------------------------------------------------------ bow (bob_bow, origin at the knot)
def ribbon_bow():
    """bob_bow: a purple satin ribbon bow under the clasp, two loops with pleats, a knot and two long tails
    hanging down the chest."""
    parts = []
    for sx in (1, -1):
        def wing_warp(p, sx=sx):
            k = 0.4 + 0.6 * min(abs(p.x) / 0.064, 1.0) ** 0.8
            return V((p.x, p.y * k, p.z * (0.7 + 0.3 * k)))
        parts.append(KK.blob(f"bw_loop{sx}", (sx * 0.038, 0.004, 0), (0.04, 0.03, 0.016), segs=10, rings=5, color=LILAC,
                             mat="flat", warp=wing_warp))
        for py in (0.01, -0.01):
            pts = []
            for t in (0.0, 0.33, 0.66, 1.0):
                x = sx * (0.02 + 0.042 * t)
                k = 0.4 + 0.6 * min(abs(x) / 0.064, 1.0) ** 0.8
                y = 0.004 + py * (0.6 + 0.9 * t)
                q = 1 - ((x - sx * 0.038) / 0.04) ** 2 - ((y - 0.004) / (0.03 * k)) ** 2
                z = 0.016 * (0.7 + 0.3 * k) * math.sqrt(max(q, 0.0))
                pts.append(V((x, y, z + 0.0005)))
            parts.append(D.sweep(f"bw_pleat{sx}{py > 0}", pts, [0.0024, 0.0026, 0.0024, 0.0], sides=5, color=PURPLE,
                                 mat="flat", project="box"))
        # a long tail hanging down and out, a soft V cut at its end
        ctrl = [V((sx * 0.008, -0.004, 0.002)), V((sx * 0.02, -0.03, 0.006)), V((sx * 0.03, -0.062, 0.004)),
                V((sx * 0.036, -0.092, 0.0))]
        tp, par = KK.catmull(ctrl, per=2)
        rr = [0.012 + 0.004 * t for t in par]
        parts.append(D.sweep(f"bw_tail{sx}", tp, rr, sides=8, color=LILAC, mat="flat", project="box", up=(0, 0, 1),
                             warp=lambda a, i: 1.0 / math.sqrt((math.cos(a) / 0.3) ** 2 + math.sin(a) ** 2)))
    parts.append(KK.blob("bw_knot", (0, 0.002, 0.006), (0.017, 0.019, 0.014), segs=12, rings=6, color=LILAC, mat="flat"))
    ob = K.merge(parts, "bow_m")
    D.put(ob, BOW_C, BOW_N, up=(0, 0, 1))
    return KK.bob_part("bob_bow", [ob], BOW_C)


# ------------------------------------------------------------------ arms: purple sleeves, big flared orange cuffs
def flared_cuff(name, pts):
    e, b = V(pts[-1]), V(pts[-2])
    d = (e - b).normalized()
    M = D.look(d)
    # (r, s) loop along d: from inside the sleeve, flaring out past the sleeve's end, rolled lip, back in
    prof = [(0.046, -0.034), (0.056, -0.03), (0.06, -0.014), (0.066, 0.0), (0.073, 0.01), (0.077, 0.016), (0.075, 0.021),
            (0.069, 0.019), (0.062, 0.01), (0.054, -0.004), (0.047, -0.02), (0.043, -0.032)]
    ob = loop_lathe(name + "_cuff", prof, 18, ORANGE, mat=PLUSH, paint=lambda i: ORANGE_DK if i >= 8 else None,
                    center=(0, 0, 0))
    ob.data.transform(Matrix.Translation(e - d * 0.006) @ M.to_4x4())
    return [ob]


# ------------------------------------------------------------------ legs: striped stockings, purple high-tops
STRIPE = 0.021


def stocking(name, x):
    """The thigh as a striped stocking: thigh2's shape with a ring at every stripe edge (crisp bands)."""
    def thigh_r(z):
        pr = [(KK.KNEE_R_BALL, KK.KNEE_Z), (KK.THIGH_R - 0.001, 0.2), (KK.THIGH_R, 0.245), (0.078, 0.292), (0.062, 0.326), (0.0, 0.338)]
        for (ra, za), (rb, zb) in zip(pr[:-1], pr[1:]):
            if za <= z <= zb:
                return ra + (rb - ra) * (z - za) / (zb - za)
        return pr[0][0]
    prof = []
    for deg in (90, 50, 20):
        a = math.radians(deg)
        prof.append((KK.KNEE_R_BALL * math.cos(a), KK.KNEE_Z - KK.KNEE_R_BALL * math.sin(a)))
    z = KK.KNEE_Z
    while z < 0.312:
        prof.append((thigh_r(z), z))
        prof.append((thigh_r(z + STRIPE / 2), z + STRIPE / 2))
        z += STRIPE
    prof += [(0.074, 0.318), (0.062, 0.326), (0.0, 0.338)]
    prof[0] = (0.0, prof[0][1])
    ob = D.lathe(name + "_thigh", prof, 16, color=ORANGE, mat=PLUSH, project="cyl", loc=(x, 0, 0))
    K.paint(ob, PURPLE, where=lambda c, n: KK.KNEE_Z <= c.z <= 0.316 and int((c.z - KK.KNEE_Z) / STRIPE) % 2 == 1)
    return ob


def shoe(name, x):
    parts = KK.sneaker(name, x, SHOE)
    up = next(o for o in parts if o.name.startswith(name + "_upper"))
    side = 1 if x > 0 else -1
    d = V((-side, 0.45, -0.1)).normalized()
    loc, nrm = KK.on_surface(D.bvh(up), V((x + side * 0.02, -0.03, 0.112)), d)
    if loc is None:
        loc, nrm = V((x + side * 0.1, -0.04, 0.112)), -d
    st = puffy_star(name + "_star", 0.032, 0.013, STAR, mat="flat")
    D.put(st, loc - nrm * 0.002, nrm, up=(0, 0, 1), roll=8 * side)
    return parts + [st]


# ================================================================== the costume
def build():
    """Returns (parts, children) for KK.finish2()."""
    KK.AO_SHAPE.clear()
    KK.AO_POST.clear()
    KK.POST_ATTACH[:] = []
    KK.AO_IGNORE.clear()
    # the hat's brim must not bake a hard shadow on the face and hair (it lifts off and tilts in play)
    for nm in ("head", "hair_cap", "soft_hair_back", "soft_hair_l", "soft_hair_r"):
        KK.AO_IGNORE[nm] = {"bob_hat", "soft_tip"}
    KK.AO_IGNORE["bob_hat"] = {"soft_tip", "soft_hair_back", "soft_hair_l", "soft_hair_r"}
    KK.AO_IGNORE["soft_skirt"] = {"leg_l", "leg_r", "soft_cape"}
    KK.AO_IGNORE["soft_cape"] = {"soft_skirt", "soft_hair_back", "soft_hair_l", "soft_hair_r"}
    parts = {"body": body()}
    parts["head"], children = head()
    for s, hip in (("l", KK.HIP_L), ("r", KK.HIP_R)):
        x = hip[0] * 1.06                       # as KK.leg2
        parts["leg_" + s] = D.part([stocking("leg_" + s, x)], "leg_" + s, hip)
        parts["shin_" + s] = D.part(shoe("leg_" + s, x), "shin_" + s, (x, 0.0, KK.KNEE_Z))
    for s, sho, elb, wri in (("l", KK.SHO_L, KK.ELBOW_L, KK.WRIST_L), ("r", KK.SHO_R, KK.ELBOW_R, KK.WRIST_R)):
        parts["arm_" + s], parts["forearm_" + s] = KK.arm2("arm_" + s, sho, elb, wri, PURPLE, mat=PLUSH, sides=12,
                                                           fore_extras=lambda p, s=s: flared_cuff("arm_" + s, p))
    parts["hand_l"] = KK.grip_hand("hand_l", KK.WRIST_L, KK.WAND_GRIP, KK.WAND_DIR, (0.55, 0.83, 0.0), 0.0155, fat=1.12)
    parts["hand_r"] = KK.grip_hand("hand_r", KK.WRIST_R, KK.BUCKET_GRIP, (1, 0, 0), (0.0, -0.45, 0.9), 0.012, fat=1.12)
    children += [("soft_skirt", skirt(), "body"), ("soft_cape", cape(), "body"), ("bob_bow", ribbon_bow(), "body")]
    return parts, children
