"""kid_witch : one costume builder (see build_costumes.py for the run command and the rig contract).

Follows the costume reference (ref/ref_witch.png): a big purple felt hat with a wide wavy brim and a tall
cone whose tip flops over and curls, a wide orange satin band with a puffy yellow star; loose wavy
shoulder-length hair; a purple dress with orange cuffs, an orange hem ruffle and a dark belt with a gold
buckle; a lilac ribbon bow with a gold star clasp between orange cape lapels; orange-and-purple striped
stockings; chunky purple high-tops with a white sole, orange laces and a yellow star. The purple cape (orange
lining) is part of the static body (no wing_l / wing_r: it never flaps), narrow at the shoulders and above the
bucket so the arms, the bucket and the legs swing clear of it.
"""
import math

import bmesh  # noqa: F401
import bpy
from mathutils import Matrix  # noqa: F401

import kid_kit as KK
from kid_kit import D, K, V, FELT  # noqa: F401
from costume_common import tube, panel, stitch_line, bone, bow, hood_eyes, dome, back_y, leaf_outline  # noqa: F401

HAT, DR, CAPE = "witch_purple", "banner_purple", "witch_purple"
LINING, TRIM, BELT = "candy_orange", "candy_orange", "spider_plum"
RIBBON, STAR, GOLD = "candy_grape", "candy_lemon", "foil_gold"
HAIR, HAIR_IN = "hair_brown", "wood_dark"
SATIN = "vinyl"


# ================================================================== local helpers
def puffy_star(name, R, h, color, mat="flat", inner=0.48, n=5):
    """A chunky stuffed star (felt appliquÃ© / enamel clasp): flat back on local z=0, a straight side wall
    and a pillowy front rising to a soft point at z=h. Points up local +Y."""
    bm = bmesh.new()

    def ring(scale, z):
        out = []
        for i in range(2 * n):
            rad = (R if i % 2 == 0 else R * inner) * scale
            a = math.pi / 2 + i * math.pi / n
            out.append(bm.verts.new((math.cos(a) * rad, math.sin(a) * rad, z)))
        return out
    r0, r1, r2 = ring(1.0, 0.0), ring(1.0, h * 0.45), ring(0.62, h * 0.85)
    top = bm.verts.new((0, 0, h))
    m = 2 * n
    bm.faces.new(list(reversed(r0)))
    for A, B in ((r0, r1), (r1, r2)):
        for j in range(m):
            bm.faces.new((A[j], A[(j + 1) % m], B[(j + 1) % m], B[j]))
    for j in range(m):
        bm.faces.new((r2[j], r2[(j + 1) % m], top))
    bmesh.ops.recalc_face_normals(bm, faces=bm.faces)
    return D.mesh(bm, name, color, mat, "box", smooth=True, center=V((0, 0, h / 2)))


def loop_lathe(name, prof, segs, color, mat=FELT, warp=None, project="cyl", center=None):
    """Revolve a CLOSED (r, z) loop (a torus-like ring: brim, ruffle) with no end caps."""
    bm = bmesh.new()
    rings = [[bm.verts.new((math.cos(2 * math.pi * j / segs) * r, math.sin(2 * math.pi * j / segs) * r, z))
              for j in range(segs)] for r, z in prof]
    n = len(rings)
    for i in range(n):
        A, B = rings[i], rings[(i + 1) % n]
        for j in range(segs):
            j2 = (j + 1) % segs
            bm.faces.new((A[j], A[j2], B[j2], B[j]))
    if warp:
        for v in bm.verts:
            v.co = warp(v.co.copy())
    bmesh.ops.recalc_face_normals(bm, faces=bm.faces)
    return D.mesh(bm, name, color, mat, project, True, center=center)


def stocking(name, x, period=0.042, z0=0.135, z1=0.31):
    """Striped stocking over the kit's thigh: a lathe with a ring at every stripe edge so the
    orange / purple bands are crisp (the kit's thigh only has 6 rings)."""
    def thigh_r(z):
        pr = [(0.074, 0.125), (0.08, 0.17), (0.084, 0.24), (0.074, 0.305)]
        for (ra, za), (rb, zb) in zip(pr[:-1], pr[1:]):
            if za <= z <= zb:
                return ra + (rb - ra) * (z - za) / (zb - za)
        return pr[0][0] if z < pr[0][1] else pr[-1][0]
    zs = []
    z = z0
    while z < z1:
        zs.append(z)
        z += period / 2
    zs.append(z1)
    prof = [(0.0, z0 - 0.004)] + [(thigh_r(z) + 0.0035, z) for z in zs] + [(0.0, z1 + 0.004)]
    SEGS = 12
    ob = D.lathe(name, prof, SEGS, color=TRIM, mat=FELT, project="cyl", loc=(x, 0, 0))
    # band 0 is the bottom fan; bands 1.. are the stripe bands (alternate purple)
    KK.paint_grid(ob, SEGS, lambda b, s: HAT if 1 <= b <= len(zs) - 1 and b % 2 == 0 else None, mat=FELT)
    return ob


# ================================================================== WITCH
def build():
    # --- head: the kid's own head + loose wavy shoulder-length hair + the big floppy hat
    head = head_hair()
    head += KK.face(fang=False)
    head.append(hat())
    head = D.part(head, "head", KK.NECK)
    KK.tilt_head(head)

    # --- body: dress bodice, flared skirt with an orange ruffle, wide belt + gold buckle, lapels, bow
    tors = KK.torso(DR)
    tree = D.bvh(tors)
    body = [tors] + skirt() + belt(tree) + neck_bow(tree) + cape_body()
    body = D.part(body, "body", (0, 0, KK.HIP_Z))

    # --- arms: purple sleeves with wide orange cuffs
    def cuff(i, j):
        return (TRIM, FELT) if i == 5 else None
    arm_l = KK.arm("arm_l", KK.SHO_L, KK.HAND_L, DR, r0=0.066, flare=0.026, paint=cuff)
    arm_r = KK.arm("arm_r", KK.SHO_R, KK.HAND_R, DR, r0=0.066, flare=0.026, paint=cuff)

    # --- legs: orange/purple striped stockings, chunky purple high-tops with a star
    shoe = dict(upper=HAT, panel=RIBBON, toe=HAT, sole="lolly_white", stripe="house_lavender", lace=TRIM,
                collar=RIBBON, tongue=HAT, heel=TRIM)

    def leg_extras(name):
        def ex(x):
            return [stocking(name + "_stock", x)] + shoe_star(name, x)
        return ex
    leg_l = KK.leg("leg_l", KK.HIP_L, TRIM, HAT, "lolly_white", shoe=shoe, extras=leg_extras("leg_l"))
    leg_r = KK.leg("leg_r", KK.HIP_R, TRIM, HAT, "lolly_white", shoe=shoe, extras=leg_extras("leg_r"))
    return dict(body=body, head=head, leg_l=leg_l, leg_r=leg_r, arm_l=arm_l, arm_r=arm_r)


# ------------------------------------------------------------------ head pieces
def flat_lock(name, pts, radii, outward, color, flat=0.55, sides=6):
    """A chunky hair lock: a tapered tube through pts, flattened against the head (thin along 'outward'),
    ending in a soft rounded tip (two short closing rings past the last point)."""
    pts, radii = [V(p) for p in pts], list(radii)
    tan = (pts[-1] - pts[-2]).normalized()
    r = radii[-1]
    pts += [pts[-1] + tan * r * 0.5, pts[-1] + tan * r * 0.85]
    radii += [r * 0.78, 0.0]

    def w(a, i):
        return math.sqrt((flat * math.cos(a)) ** 2 + math.sin(a) ** 2)
    return D.sweep(name, pts, radii, sides=sides, color=color, mat="flat", project="sphere", up=outward, warp=w)


HC2, HR2 = V((0, 0.04, 0.80)), V((0.245, 0.245, 0.2))


def head_hair():
    """Soft, full, wavy shoulder-length hair (replaces KK.bare_head's cap + fringe): a rounded volume
    under the brim, 12 chunky overlapping locks falling round the back and sides to the shoulders with
    their ends flipping outward, and four thick side-swept bangs across the forehead."""
    def cut(c):
        return c.y < -0.07 and (c.x / 0.176) ** 2 + ((c.z - 0.80) / 0.17) ** 2 < 1.0
    parts = [KK.blob("hair", HC2, HR2, segs=20, rings=10, color=HAIR_IN, mat="flat", cut=cut)]
    N = 12
    lens = [0.02, -0.01, 0.015, -0.02, 0.005, 0.02, -0.005, 0.02, -0.015, 0.01, -0.01, 0.02]
    for k in range(N):
        t = k / (N - 1)
        phi = math.radians(-32 + 244 * t)
        edge = min(t, 1 - t) * (N - 1)                 # 0 at the two face-framing locks
        sway = (0.1 if k % 2 else -0.1) * (1 if math.sin(phi) > 0 else 0.6)
        zb = 0.648 + lens[k] + (0.03 if edge < 0.5 else 0.0)
        r0 = 0.06 if edge >= 1 else 0.05

        def at(rad, z, dphi):
            a = phi + dphi
            return V((math.cos(a) * rad, HC2.y + math.sin(a) * rad * 0.97, z))
        ctrl = [at(0.19, 0.9, 0.0), at(0.262, 0.805, sway * 0.4), at(0.258, 0.745, -sway * 0.3),
                at(0.29, zb + 0.006, sway * 0.1), at(0.324, zb + 0.026, sway * 0.4)]
        pts, par = KK.catmull(ctrl, per=2)
        rr = [KK.lerp_list([r0 * 0.8, r0 * 1.1, r0 * 0.95, r0 * 0.9, r0 * 0.55], p) for p in par]
        col = HAIR
        parts.append(flat_lock(f"lock{k}", pts, rr, (math.cos(phi), math.sin(phi), 0.0), col, flat=0.48, sides=7))
    # side-swept bangs: thick clumps from the parting sweeping down to the temples
    for k, (x0, z0, x1, z1, bend, r0) in enumerate([(-0.095, 0.93, -0.168, 0.855, -0.01, 0.046),
                                                     (-0.075, 0.938, 0.045, 0.884, 0.0, 0.054),
                                                     (-0.04, 0.942, 0.125, 0.876, 0.0, 0.054),
                                                     (0.015, 0.94, 0.188, 0.845, 0.0, 0.05)]):
        pts = []
        for i in range(5):
            s = i / 4
            x = x0 + (x1 - x0) * s + bend * math.sin(math.pi * s)
            z = z0 + (z1 - z0) * (s ** 1.7) + 0.012 * math.sin(math.pi * s)
            p, n = KK.ellipsoid_pt(KK.FC, KK.FR, x, z)
            pts.append(p + n * (0.016 + 0.016 * math.sin(math.pi * s)))
        _, n0 = KK.ellipsoid_pt(KK.FC, KK.FR, (x0 + x1) / 2, (z0 + z1) / 2)
        rr = [r0 * 0.7, r0 * 1.1, r0, r0 * 0.8, r0 * 0.5]
        parts.append(flat_lock(f"bang{k}", pts, rr, n0, HAIR, flat=0.45, sides=7))
    return parts


def hat():
    BZ = 0.985
    parts = []
    # wide brim with a rolled felt edge, gently waving up and down around the head
    brim_prof = [(0.166, BZ + 0.013), (0.24, BZ + 0.005), (0.31, BZ - 0.01), (0.352, BZ - 0.024), (0.372, BZ - 0.034),
                 (0.374, BZ - 0.045), (0.36, BZ - 0.05), (0.32, BZ - 0.042), (0.25, BZ - 0.026), (0.18, BZ - 0.012)]

    def brim_warp(c):
        a = math.atan2(c.y, c.x)
        r = math.hypot(c.x, c.y)
        k = max(0.0, (r - 0.2) / 0.17)
        dz = k * (0.026 * math.sin(3 * a + 0.8) + 0.012 * math.sin(5 * a + 2.0) - 0.016 * max(0.0, math.sin(a)))
        return V((c.x, c.y, c.z + dz))
    parts.append(loop_lathe("brim", list(reversed(brim_prof)), 36, HAT, warp=brim_warp, center=(0, 0, BZ)))
    # tall cone that bends over to the kid's right (viewer's left from the front) and droops, the very
    # tip hooking back up
    ctrl = [(0, 0, BZ - 0.01), (0, 0.012, BZ + 0.11), (-0.008, 0.028, BZ + 0.205), (-0.03, 0.045, BZ + 0.278),
            (-0.075, 0.056, BZ + 0.326), (-0.135, 0.06, BZ + 0.338), (-0.19, 0.058, BZ + 0.306),
            (-0.222, 0.054, BZ + 0.262), (-0.242, 0.05, BZ + 0.243), (-0.262, 0.046, BZ + 0.254)]
    radii = [0.174, 0.142, 0.11, 0.084, 0.064, 0.048, 0.034, 0.022, 0.012, 0.0]
    cone, cp, cr = tube("cone", ctrl, radii, per=3, sides=16, color=HAT, mat=FELT, project="cyl")
    parts.append(cone)
    fine, par = KK.catmull(ctrl, per=12)
    # wide orange satin band hugging the cone base (sampled from the same spline, so it follows it exactly)
    bp, brr = [], []
    for p, t in zip(fine, par):
        if BZ - 0.008 <= p.z <= BZ + 0.078:
            bp.append(p)
            h = (p.z - (BZ - 0.008)) / 0.086
            brr.append(KK.lerp_list(radii, t) + 0.006 + 0.004 * math.sin(math.pi * h))
    band = D.sweep("band", bp, brr, sides=24, color=TRIM, mat=SATIN, project="cyl")
    parts.append(band)
    # piping along both band edges (the satin ribbon's rolled edges)
    for k, (p, r) in enumerate(((bp[0], brr[0]), (bp[-1], brr[-1]))):
        ring = [p + V((math.cos(a) * r, math.sin(a) * r, 0)) for a in (2 * math.pi * i / 24 for i in range(24))]
        parts.append(D.sweep(f"band_edge{k}", ring, 0.0055, sides=4, color="pumpkin", mat=SATIN, closed=True,
                             center=p))
    # big puffy yellow star on the band, front and to the kid's right
    a = math.radians(-122)
    zc = BZ + 0.04
    rr = 0.172 + 0.006
    nrm = V((math.cos(a), math.sin(a), 0.16)).normalized()
    st = puffy_star("hat_star", 0.066, 0.024, STAR, mat=FELT)
    D.put(st, V((math.cos(a) * rr, math.sin(a) * rr + 0.004, zc)) - nrm * 0.004, nrm, up=(0, 0, 1), roll=-10)
    parts.append(st)
    ob = K.merge(parts, "hat")
    # jaunty: tipped back a little (brim lifts off the eyes) and down on the kid's right
    ob.data.transform(Matrix.Translation(V((0, 0.015, BZ))) @ Matrix.Rotation(math.radians(-9), 4, "X") @
                      Matrix.Rotation(math.radians(-6), 4, "Y") @ Matrix.Translation(V((0, 0, -BZ))))
    return ob


# ------------------------------------------------------------------ body pieces
HEM = 0.314


def skirt():
    """A-line skirt flaring into soft folds, with a gathered orange ruffle round the hem."""
    prof = [(0.0, HEM + 0.06), (0.17, HEM + 0.03), (0.236, HEM + 0.007), (0.25, HEM), (0.255, HEM + 0.012),
            (0.245, HEM + 0.042), (0.229, 0.39), (0.215, 0.425), (0.206, 0.452), (0.17, 0.47)]

    def fold_k(z):
        return min(max((0.44 - z) / 0.12, 0.0), 1.0)

    def warp(c):
        a = math.atan2(c.y, c.x)
        r = math.hypot(c.x, c.y)
        k = fold_k(c.z)
        r *= 1 + k * 0.035 * math.cos(8 * a)
        z = c.z + k * k * 0.008 * math.cos(8 * a)
        sy = r * math.sin(a)
        return V((r * math.cos(a), sy * (0.96 if sy < 0 else 0.9) + 0.012, z))
    sk = D.lathe("skirt", prof, 32, color=DR, mat=FELT, project="sphere", warp=warp, center=(0, 0, 0.35))
    # ruffle: a gathered frill tucked under the hem edge, rippling 14 times round
    rh = 0.25
    rprof = [(rh - 0.016, HEM + 0.014), (rh + 0.006, HEM + 0.004), (rh + 0.02, HEM - 0.016), (rh + 0.024, HEM - 0.03),
             (rh + 0.012, HEM - 0.026), (rh - 0.008, HEM - 0.006)]

    def rwarp(c):
        a = math.atan2(c.y, c.x)
        r = math.hypot(c.x, c.y)
        out = max(0.0, (r - (rh - 0.016)) / 0.04)
        r *= (1 + 0.035 * math.cos(8 * a)) * (1 + out * 0.035 * math.cos(12 * a))
        z = c.z + 0.008 * math.cos(8 * a) - out * 0.007 * math.cos(12 * a)
        sy = r * math.sin(a)
        return V((r * math.cos(a), sy * (0.96 if sy < 0 else 0.9) + 0.012, z))
    ruf = loop_lathe("ruffle", list(reversed(rprof)), 48, TRIM, mat=FELT, warp=rwarp, center=(0, 0, HEM))
    return [sk, ruf]


def belt(tree):
    """Wide dark belt at the waist with a chunky gold buckle and a belt loop."""
    def warp(c):
        y = c.y * 0.88
        if y < 0:
            y *= 1.0 + 0.1 * math.exp(-((c.z - 0.42) / 0.12) ** 2)
        return V((c.x, y + 0.012, c.z))
    prof = [(KK.torso_r(0.444) + 0.002, 0.441), (KK.torso_r(0.446) + 0.009, 0.447), (KK.torso_r(0.478) + 0.009, 0.477),
            (KK.torso_r(0.48) + 0.002, 0.483)]
    bl = D.lathe("belt", prof, 28, color=BELT, mat=FELT, project="cyl", warp=warp, center=(0, 0, 0.46))
    out = [bl]
    loc, nrm = KK.on_surface(tree, (0.0, -0.4, 0.462), (0, 1, 0))
    frame = K.box("buckle", (0.066, 0.05, 0.014), color=GOLD, mat="metal", bevel=0.006)
    D.put(frame, loc + nrm * 0.013, nrm)
    hole = K.box("buckle_hole", (0.036, 0.024, 0.012), color=BELT, mat=FELT)
    D.put(hole, loc + nrm * 0.016, nrm)
    prong = K.box("buckle_prong", (0.026, 0.006, 0.006), color=GOLD, mat="metal")
    D.put(prong, loc + nrm * 0.022 + V((0.006, 0, 0)), nrm)
    loop_ = K.box("belt_loop", (0.012, 0.046, 0.012), color=BELT, mat=FELT, bevel=0.003)
    l2, n2 = KK.on_surface(tree, (0.066, -0.4, 0.462), (0, 1, 0))
    D.put(loop_, l2 + n2 * 0.012, n2)
    return out + [frame, hole, prong, loop_]


def neck_bow(tree):
    """Orange cape lapels folded down either side of a lilac ribbon bow with long tails, fastened by a
    gold star clasp, just under the chin."""
    out = []
    for sx in (1, -1):
        pts = [(0.0, 0.012), (0.105 * sx, 0.02), (0.045 * sx, -0.07)]
        if sx < 0:
            pts = list(reversed(pts))
        lap = K.polygon_prism(f"lapel{sx}", KK.rounded_poly(pts, 0.01, 2), 0.01, color=LINING, mat=FELT)
        loc, nrm = KK.on_surface(tree, (0.012 * sx, -0.3, 0.592), (0, 1, 0.0))
        D.put(lap, loc + nrm * 0.003, nrm, up=(0, 0, 1))
        out.append(lap)
        # ribbon tails hanging down the chest, conforming to the bodice
        rows = []
        for i in range(7):
            t = i / 6
            u = -0.012 - 0.092 * t                          # down the chest
            c = sx * (0.012 + 0.026 * t + 0.008 * math.sin(t * math.pi))
            w = 0.011 + 0.004 * t
            rows.append((u, -(c + w), -(c - w)))
        out.append(KK.strip(f"bow_tail{sx}", tree, rows, (0.0, -0.4, 0.58), (0, 1, 0.12), up=(-1, 0, 0),
                            lift=0.006, thick=0.005, color=RIBBON, mat=SATIN))
    loc, nrm = KK.on_surface(tree, (0.0, -0.4, 0.592), (0, 1, 0.0))
    nrm = (nrm + V((0, -0.3, 0.15))).normalized()
    out.append(bow("neck_bow", loc + nrm * 0.016, nrm, (0, 0, 1), RIBBON, s=1.05))
    st = puffy_star("clasp_star", 0.03, 0.014, GOLD, mat="metal")
    D.put(st, loc + nrm * 0.026, nrm, up=(0, 0, 1))
    out.append(st)
    return out


# ------------------------------------------------------------------ legs
def shoe_star(name, x):
    """A puffy yellow star stuck on the outer side of the high-top's ankle, angled a little to the front."""
    up = bpy.data.objects.get(name + "_upper")
    side = 1 if x > 0 else -1
    d = V((-side, 0.45, -0.1)).normalized()
    p0 = V((x + side * 0.02, -0.03, 0.115))
    loc, nrm = (None, None)
    if up is not None:
        loc, nrm = KK.on_surface(D.bvh(up), p0, d)
    if loc is None:
        loc, nrm = V((x + side * 0.1, -0.04, 0.115)), -d
    st = puffy_star(name + "_star", 0.03, 0.012, STAR, mat=FELT)
    D.put(st, loc - nrm * 0.002, nrm, up=(0, 0, 1), roll=8 * side)
    return [st]


# ------------------------------------------------------------------ cape (static, part of the body)
def cape_body():
    """The purple cape (orange lining) hanging from the shoulders down the back, built into the body so
    it never flaps. Narrow at the shoulders (stays behind the sleeves), it flares toward the hem below
    the hands and the bucket, in soft vertical folds, the hem rolled back a little to show the lining."""
    def W(v):
        return 0.12 + 0.02 * v + 0.075 * v * v          # hem half-width 0.215: clear of the swinging bucket

    def P(u, v):
        s = 2 * u - 1
        x = s * W(v)
        z = 0.632 - v * 0.41 + 0.05 * abs(s) ** 3 * v
        z += 0.011 * math.cos(s * math.pi * 2.5) * v ** 3                  # soft wavy hem
        y = back_y(x, z, 0.03 + 0.06 * v) + 0.024 * math.cos(s * math.pi * 2.5) * v ** 0.8
        if v > 0.8:                                                         # hem rolls back, showing the lining
            t = (v - 0.8) / 0.2
            y += 0.02 * t * t
            z += 0.01 * t * t
        return (x, max(y, 0.108), z)
    return [panel("cape", P, 16, 8, 0.02, CAPE, LINING, out_hint=lambda u, v: (0.8 * (2 * u - 1), 1.0, 0.0),
                  out_paint=lambda i, j: LINING if j == 7 else None)]
