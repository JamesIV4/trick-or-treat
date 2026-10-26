"""kid_ghost : one costume builder (see build_costumes.py for the run command and the rig contract).

Reference: ref/ref_ghost.png. A soft white bedsheet over the head and down to the knees: a round dome
with two big black oval eyes high above the face, the kid's face (brown fringe) framed by a rolled
opening, the sheet falling from the head over the shoulders in soft vertical folds to an uneven wavy
hem; the arms come out from under it with bare hands; black leggings; chunky purple high-tops with a
white toe cap and sole, orange laces and a silver padded collar. No optional parts (no wings, no tail):
the drape behind the arms and the folds down the back are all in the one body sheet."""
import math

import bmesh  # noqa: F401
from mathutils import Matrix  # noqa: F401

import kid_kit as KK
from kid_kit import D, K, V, FELT  # noqa: F401
from costume_common import tube, panel, stitch_line, bone, bow, hood_eyes, dome, back_y, leaf_outline  # noqa: F401

W = "ghost_white"
SH = "flat"           # a smooth sheet: felt read as fleece, the flannel check as pyjamas
SEG = 64              # around the sheet
BUCKET_A = math.atan2(KK.HAND_R.y, KK.HAND_R.x)


# ------------------------------------------------------------------ sheet shape (body sheet + the hood's skirt)
def _adiff(a, b):
    return math.atan2(math.sin(a - b), math.cos(a - b))


def hitch(a):
    """The hem lifts over the bucket arm so the bucket swings free in front of it."""
    return 0.125 * math.exp(-(_adiff(a, BUCKET_A) / 0.72) ** 2)


def fold(a):
    """Irregular soft vertical folds around the sheet (-1..1-ish)."""
    return 0.55 * math.sin(7 * a + 0.4) + 0.3 * math.sin(12 * a + 2.1) + 0.2 * math.sin(4 * a + 1.0)


def fold_amp(z):
    """Fold depth in metres: soft under the head (the hood continues them), deep toward the hem."""
    t = min(max((0.62 - z) / 0.38, 0.0), 1.0)
    return 0.005 + 0.032 * t ** 1.3


def drape(a, z):
    """Behind the arms the sheet falls out from the shoulders like it is hanging over the arms (the
    broad trapezoid of the reference), without touching the arms, the hands or the bucket."""
    k = min(max((0.575 - z) / 0.13, 0.0), 1.0)
    k = k * k * (3 - 2 * k)
    h = k * (0.052 - 0.034 * min(max((0.42 - z) / 0.17, 0.0), 1.0))
    return h * (math.exp(-(_adiff(a, 0.62) / 0.42) ** 2) + math.exp(-(_adiff(a, math.pi - 0.62) / 0.42) ** 2))


def hem_z(a):
    """Uneven, softly wavy hem: higher in front (the leggings show), lower at the sides."""
    return (0.248 + 0.016 * math.sin(5 * a + 0.7) + 0.009 * math.sin(9 * a + 1.9)
            - 0.03 * math.cos(a) ** 2 + 0.022 * max(0.0, -math.sin(a)) ** 2 + hitch(a))


# The head sheet (hood) keeps falling straight down past the cheeks to the shoulders; the body sheet
# comes out from under it a little above the hood's lower edge and crosses it at a shallow angle, so the
# two read as one sheet. HOOD_T: the hood skirt's radius (side, front = the bib under the chin, back).
HOOD_T = (0.252, 0.19, 0.254)
# body rings that follow the hood: (z, offset from the hood surface, least radius: in front the sheet
# falls from behind the rolled rim under the chin)
JOIN = [(0.695, -0.02, 0.15), (0.665, -0.01, 0.162), (0.635, -0.002, 0.17), (0.608, 0.006, 0.18)]
# (z, side, front, back) radii of the sheet below the hood: over the shoulders, close at the arms, loose front/back
UPPER = [(0.58, 0.256, 0.196, 0.258), (0.535, 0.248, 0.21, 0.258), (0.485, 0.229, 0.224, 0.253),
         (0.44, 0.215, 0.232, 0.248), (0.405, 0.209, 0.24, 0.245)]
SKIRT_TOP = 0.405
HEM_R = (0.322, 0.296, 0.306)      # side, front, back at the hem


def _mix(a, side, front, back):
    sf = max(0.0, -math.sin(a)) ** 2
    sb = max(0.0, math.sin(a)) ** 2
    return side + (front - side) * sf + (back - side) * sb


def sheet_at(a, t):
    """Point on the outer sheet at angle a, t = 0 at the top of the skirt .. 1 at the hem, or a
    negative index -k for the k-th UPPER ring from the bottom. Returns (radius, z)."""
    if t < 0:
        z, s, f, b = UPPER[len(UPPER) + int(t)]
        r = _mix(a, s, f, b)
    else:
        hz = hem_z(a)
        z = SKIRT_TOP + (hz - SKIRT_TOP) * t
        r0 = _mix(a, UPPER[-1][1], UPPER[-1][2], UPPER[-1][3])
        r1 = _mix(a, *HEM_R)
        e = t ** 1.25
        r = r0 + (r1 - r0) * e - hitch(a) * 0.5 * e
    return r + drape(a, z) + fold_amp(z) * fold(a), z


def ring_mesh(name, rings, color, mat, center):
    """Faces between consecutive rings (bottom-to-top profile order -> outward normals, like D.lathe);
    a single-point ring is a pole."""
    bm = bmesh.new()
    vs = [[bm.verts.new(p) for p in ring] for ring in rings]
    n = max(len(r) for r in vs)
    for A, B in zip(vs[:-1], vs[1:]):
        for j in range(n):
            j2 = (j + 1) % n
            if len(A) == 1:
                bm.faces.new((A[0], B[j2], B[j]))
            elif len(B) == 1:
                bm.faces.new((A[j], A[j2], B[0]))
            else:
                bm.faces.new((A[j], A[j2], B[j2], B[j]))
    return D.mesh(bm, name, color, mat, "sphere", True, center=V(center))


HEAD_TILT = Matrix.Rotation(math.radians(-5), 3, "Y") @ Matrix.Rotation(math.radians(-3), 3, "X")   # = KK.tilt_head


def _pt(a, r, z):
    """Sheet point; up where it meets the head sheet it follows the head's baked tilt, so the hood's
    skirt and the body sheet stay concentric all the way round (no ledge at the shoulders)."""
    p = V((math.cos(a) * r, math.sin(a) * r + 0.012, z))
    s = min(max((z - 0.5) / 0.09, 0.0), 1.0)
    if s > 0:
        n = V(KK.NECK)
        p = p + ((n + HEAD_TILT @ (p - n)) - p) * s
    return p


def body_sheet():
    angs = [2 * math.pi * j / SEG for j in range(SEG)]
    rings = []
    # underside: inner pole, the inner wall just above the hem, the rolled hem lip
    rings.append([V((0, 0.012, 0.37))])
    rings.append([_pt(a, sheet_at(a, 1.0)[0] - 0.03, sheet_at(a, 1.0)[1] + 0.06) for a in angs])
    rings.append([_pt(a, sheet_at(a, 1.0)[0] - 0.017, sheet_at(a, 1.0)[1] + 0.012) for a in angs])
    rings.append([_pt(a, sheet_at(a, 1.0)[0] - 0.006, sheet_at(a, 1.0)[1] - 0.002) for a in angs])
    # outer: hem up the skirt, then the upper sheet up under the hood
    for t in (1.0, 0.86, 0.7, 0.52, 0.34, 0.16):
        rings.append([_pt(a, *sheet_at(a, t)) for a in angs])
    for k in range(1, len(UPPER) + 1):
        rings.append([_pt(a, *sheet_at(a, -k)) for a in angs])
    for z, off, rmin in reversed(JOIN):
        rings.append([_pt(a, max(hood_r(a, z, hood_r0(a, z)) + off, rmin), z) for a in angs])
    rings.append([_pt(0.0, 0.0, 0.73)])
    return ring_mesh("sheet", rings, W, SH, (0, 0, 0.45))


# ------------------------------------------------------------------ head
def hood_r0(a, z):
    """Radius of the plain hood ellipsoid at angle a (about HC) and height z."""
    q = max(1 - ((z - KK.HC.z) / KK.HR.z) ** 2, 0.0)
    return math.sqrt(q) / math.sqrt((math.cos(a) / KK.HR.x) ** 2 + (math.sin(a) / KK.HR.y) ** 2)


def hood_r(a, z, r):
    """The hood's shell radius after shaping (r = plain ellipsoid radius there): below the cheeks the
    sheet keeps falling straight to the shoulders (beside and behind the face; the face opening and the chin
    are left alone, the body sheet comes out from behind the rim there), and soft folds run down the back into the body sheet."""
    off = abs(_adiff(a, -math.pi / 2))                      # 0 = straight ahead
    thr = 0.62 + 0.33 * min(max((z - 0.66) / 0.14, 0.0), 1.0)  # the opening is narrow low down
    wa = min(max((off - thr) / 0.45, 0.0), 1.0)
    r2 = r
    if z < 0.82:
        k = min((0.82 - z) / 0.16, 1.0)
        k = k * k * (3 - 2 * k)
        m = k * wa
        r2 += max(0.0, _mix(a, *HOOD_T) - r) * m + m * 0.004 * fold(a)
    kb = min(max((0.98 - z) / 0.22, 0.0), 1.0)
    return r2 + wa * kb * kb * 0.005 * fold(a)


def head_warp_fn():
    DOME = dome(0.36, 1.4)

    def w(p):
        p = DOME(p)
        dx, dy = p.x, p.y - KK.HC.y
        r = math.hypot(dx, dy)
        if p.z < 0.98 and r > 1e-6:
            r2 = hood_r(math.atan2(dy, dx), p.z, r)
            p = V((dx * r2 / r, KK.HC.y + dy * r2 / r, p.z))
        return p
    return w


def sheet_eyes(tree):
    """Two big black ovals high on the sheet, set flush like painted-on holes, tops leaning in."""
    parts = []
    for sx in (1, -1):
        p, n = KK.on_surface(tree, (0.09 * sx, -0.6, 1.03), (0, 1, 0))
        nf = (n + V((0, -0.3, 0))).normalized()
        e = KK.blob(f"heye{sx}", (0, 0, 0), (0.047, 0.058, 0.011), segs=16, rings=6, color="ink", mat="flat")
        D.put(e, p - n * 0.003, nf, roll=12 * sx)
        parts.append(e)
    return parts


# ================================================================== GHOST
def build():
    # --- head: the sheet over the head, a round dome, big black eyes, the face framed by a rolled opening
    head = KK.hood(W, rim_color=W, mat=SH, rim_mat=SH, warp=head_warp_fn(), rim_r=0.04, stitches=False,
                   segs=36, rings=18)
    tree = D.bvh(head[0])
    head += KK.face(fang=False, mouth="smile")
    head += KK.fringe()
    head += sheet_eyes(tree)
    head = D.part(head, "head", KK.NECK)
    KK.tilt_head(head)

    # --- body: the long sheet from under the head to a wavy hem near the knees, hitched over the bucket
    body = D.part([body_sheet()], "body", (0, 0, KK.HIP_Z))

    # --- arms: the sheet over the arms, bare hands; legs: black leggings and purple high-tops
    arm_l = KK.arm("arm_l", KK.SHO_L, KK.HAND_L, W, mat=SH, r0=0.072, flare=0.026, sides=12)
    arm_r = KK.arm("arm_r", KK.SHO_R, KK.HAND_R, W, mat=SH, r0=0.072, flare=0.026, sides=12)
    shoe = dict(upper="witch_purple", panel="candy_grape", toe="lolly_white", sole="lolly_white",
                stripe="witch_purple", lace="candy_orange", collar="silver", tongue="witch_purple",
                heel="candy_orange")
    leg_l = KK.leg("leg_l", KK.HIP_L, "cat_black", "witch_purple", "lolly_white", thigh_mat=FELT, shoe=shoe)
    leg_r = KK.leg("leg_r", KK.HIP_R, "cat_black", "witch_purple", "lolly_white", thigh_mat=FELT, shoe=shoe)

    # no wings / tail: the sheet's drape behind the arms is part of the body sheet
    return dict(body=body, head=head, leg_l=leg_l, leg_r=leg_r, arm_l=arm_l, arm_r=arm_r)
