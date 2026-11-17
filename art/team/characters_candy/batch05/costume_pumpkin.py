"""kid_pumpkin v2: the Pumpkin costume on kit v2 (design: batch04/ref/ref_pumpkin.png; v1: batch04/costume_pumpkin.py).

A big round pumpkin body suit with deep glossy ribs (ten lobes, a dimpled top and bottom), a carved
jack-o'-lantern face on the belly (triangle eyes, triangle nose, toothy grin; dark pockets with lit
pumpkin-flesh walls), a collar of big veined leaves round the neck, two curly vines on the back; short
orange puff sleeves (ribbed like the pumpkin, darker rolled hem) over the kid's own bare arms with
working elbows; green-and-black striped tights; orange high-tops with white toes and soles and a green
strap. On the head the kid's own face v2 (no fang), brown hair (swept bangs and side locks) showing
UNDER a pumpkin-top cap (ribbed, scalloped lobe tips; no hair on top of it), with the spring parts the
rig drives:
  soft_stem     (under head, origin at its base): the thick curling green stem
  bob_leaf_a/_b (under head, origin at the stalk): two big leaves lying on the cap by the stem
  soft_vine_a/_b (under head, origin at the attached end, by the stem base): curly tendrils, corkscrews
                that run AWAY from their pins (the shader bends by distance from the pin)
No wings. The arm pivots are the kit's (v1 top-level joint origins kept); the round body is shaped so the
upper arms clear it with the costume's arm_out (ARM_OUT here = KidTuning kid_pumpkin.arm_out).
"""
import math
import os

import bmesh
import bpy
from mathutils import Matrix

import kid_kit as KK
from kid_kit import D, K, V, PLUSH
from costume_common import panel

P, PD, PL = "pumpkin", "pumpkin_dark", "pumpkin_light"
LEAF, LEAF2, LEAF_IN, VEIN = "lilypad", "banner_green", "lawn_dark", "apple_green"
STEM, STEM_DK = "apple_green", "gourd_green"
TEND = "gourd_green"
SOCK, SOCK_DARK = "lilypad", "charcoal"
HAIR, HAIR_LT, HAIR_DK = "hair_brown", "hair_light", "hair_dark"
GLOSS = "vinyl"
# The suit's shell (body, cap, puffs): a clean smooth surface; the shine is the rig's `gloss` (KidTuning::shine) and
# the creases are shading. The "pumpkin" texture layer (streaky ribs, speckles) read as translucent wax in the
# showcase (BEFORE and round 1).
SUIT = "flat"
SHOE = dict(upper=P, panel=LEAF, toe="lolly_white", sole="lolly_white", stripe=PD, lace=LEAF, collar=PD, tongue=PL,
            heel=LEAF, mat=GLOSS)

# = KidTuning kid_pumpkin.arm_out / elbow_rest in kid_rig.cpp (build_costumes ARM_OUT renders <id>_out.png with them)
ARM_OUT, ELBOW_REST = 0.47, 0.22

# body: a squat ribbed pumpkin
RX, RY, CZ, RZT, RZB = 0.214, 0.232, 0.43, 0.238, 0.19
NL, SPL = 10, 8
A0 = -math.pi / 2            # a lobe centred on the front (-Y)
RIB = 0.105                  # depth of the creases (share of the radius)
BODY_Y = 0.008
# head: the pumpkin-top cap
CC, CR, RYF = V((0.0, 0.018, 0.846)), V((0.262, 0.262, 0.238)), 0.286
CAP_NL = 10
HY = 0.035                   # the scalp's centre (Blender y)
SCALP_C, SCALP_R = V((0, HY, 0.80)), V((0.224, 0.214, 0.198))


def smooth01(t):
    t = min(max(t, 0.0), 1.0)
    return t * t * (3 - 2 * t)


# ================================================================== helpers
def lobe(a, a0, n):
    """Position across a pumpkin lobe: u in -1..1 (0 = lobe centre, +-1 = crease) and the round
    'semicircle' height of the lobe there (1 at the centre, 0 in the crease)."""
    t = (a - a0) * n / (2 * math.pi)
    u = 2 * (t - round(t))
    return u, math.sqrt(max(0.0, 1 - u * u))


def curl(name, start, d0, up, length, turns, r0, n=14, tight=1.7, drift=0.15, color=TEND, sides=6):
    """A curly vine lying on a surface (static): starts along d0 and winds up into a tightening spiral."""
    d0 = V(d0).normalized()
    up = V(up)
    up = (up - d0 * up.dot(d0)).normalized()
    side = d0.cross(up)
    p = V(start)
    pts = [p.copy()]
    ds = length / n
    for i in range(1, n + 1):
        t = i / n
        th = turns * 2 * math.pi * t ** tight
        h = d0 * math.cos(th) + up * math.sin(th) + side * drift
        p = p + h.normalized() * ds * (1 - 0.6 * t)
        pts.append(p.copy())
    radii = [r0 * (1 - 0.65 * i / n) for i in range(n + 1)]
    radii[-1] = 0.0
    return D.sweep(name, pts, radii, sides=sides, color=color, mat="flat", project="box")


def tendril(name, start, axis, length, turns, coil, r0, n=34, color=TEND, rise=(0, 0, 1), arc=0.5):
    """A corkscrew vine tendril (a soft part): a short straight start, then a helix round an axis that bends
    toward 'rise' along its length, the coil widening and the turns tightening toward the tip. Every point is
    farther from the start than the one before (the rig bends a soft part by distance from its pin)."""
    ax = V(axis).normalized()
    rs = V(rise).normalized()
    pts, prev_axis_p = [], V(start)
    c = V(start)
    for i in range(n + 1):
        t = i / n
        d = (ax * (1 - arc * t) + rs * (arc * t)).normalized()
        if i:
            c = c + d * (length / n)
        u = d.orthogonal().normalized()
        w = d.cross(u)
        rc = coil * smooth01(t / 0.35) * (1.0 - 0.25 * t)
        th = turns * 2 * math.pi * t ** 1.25
        pts.append(c + (u * math.cos(th) + w * math.sin(th)) * rc)
    radii = [r0 * (1 - 0.55 * i / n) for i in range(n + 1)]
    radii[-1] = 0.0
    # a rounded end
    radii[-2] = max(radii[-2], r0 * 0.4)
    ob = D.sweep(name, pts, radii, sides=6, color=color, mat=GLOSS, project="box")
    return ob


def hug_leaf(name, surf, a, th0, span, W, hr, lift=0.004, tip_lift=0.02, cup=0.008, th=0.007, nu=8, nv=5,
             color=LEAF, under=LEAF_IN, veins=VEIN, pairs=3, out_mat=GLOSS):
    """A pointed oval leaf lying on a round surface. surf(direction) -> (point, normal) on the surface seen
    from its centre; the leaf runs down the meridian at azimuth a from polar angle th0 over 'span' (radians),
    W = half width (m), hr ~ horizontal radius of the surface. Tips lift off the surface, the edges cup up.
    Midrib + side veins on top. Returns (object, stalk point)."""
    def hw(u):
        return max(W * math.sin(math.pi * min(max(u, 0.0), 1.0) ** 0.75) ** 0.85, 0.002)

    def at(u, v, extra=0.0):
        tht = th0 + span * u
        s = max(math.sin(tht), 0.08)
        da = (2 * v - 1) * hw(u) / (hr * s)
        d = V((s * math.cos(a + da), s * math.sin(a + da), math.cos(tht)))
        p, n = surf(d)
        off = lift + tip_lift * u * u + cup * (2 * v - 1) ** 2 * hw(u) / W + extra
        return p + n * off, n

    lf = panel(name, lambda u, v: at(u, v)[0], nu, nv, th, color, under, out_mat=out_mat, in_mat=out_mat,
               out_hint=lambda u, v: at(u, v)[1])
    parts = [lf]
    top = th / 2 + 0.0015
    rib = [at(u, 0.5, top)[0] for u in (0.02, 0.25, 0.5, 0.72, 0.9)]
    parts.append(D.sweep(name + "_rib", rib, [0.0045, 0.004, 0.0034, 0.0024, 0.0], sides=5, color=veins, mat="flat",
                         project="box"))
    for k in range(pairs):
        u0 = 0.16 + 0.22 * k
        for sv in (0.12, 0.88):
            vp = [at(u0, 0.5, top)[0], at(u0 + 0.09, 0.5 + (sv - 0.5) * 0.55, top)[0], at(u0 + 0.15, sv, top)[0]]
            parts.append(D.sweep(f"{name}_v{k}{sv}", vp, [0.0026, 0.0019, 0.0], sides=4, color=veins, mat="flat",
                                 project="box"))
    stalk = at(0.0, 0.5)[0]
    return K.merge(parts, name), stalk


def carve(ob, tree, shapes, depth=0.016, floor="ink", wall="choco_dark"):
    """Carve jack-o'-lantern holes into the closed body mesh (v1's boolean): each shape is (outline [(u, v)],
    centre), seen along +Y; the cutter hugs the surface (top 3 cm out, floor 'depth' in along the normal), so
    the pocket has an even depth. Pocket floors -> floor colour, pocket walls -> wall colour (lit flesh)."""
    cutters = []
    for k, (outline, cen) in enumerate(shapes):
        bm = bmesh.new()
        top, bot = [], []
        for u, v in outline:
            q = V(cen) + V((u, 0, v))
            loc, nrm = KK.on_surface(tree, q, (0, 1, 0))
            top.append(bm.verts.new(loc + nrm * 0.03))
            bot.append(bm.verts.new(loc - nrm * depth))
        bm.faces.new(top)
        bm.faces.new(list(reversed(bot)))
        n = len(top)
        for i in range(n):
            j = (i + 1) % n
            bm.faces.new((top[i], bot[i], bot[j], top[j]))
        bmesh.ops.triangulate(bm, faces=list(bm.faces))
        bmesh.ops.recalc_face_normals(bm, faces=bm.faces)
        cutters.append(D.mesh(bm, f"cut{k}", "ink", "flat", "box", smooth=False))
    cut = K.merge(cutters, "cutter")
    mod = ob.modifiers.new("carve", "BOOLEAN")
    mod.operation = "DIFFERENCE"
    mod.solver = "EXACT"
    mod.object = cut
    dg = bpy.context.evaluated_depsgraph_get()
    carved = bpy.data.meshes.new_from_object(ob.evaluated_get(dg))
    ob.modifiers.clear()
    old = ob.data
    ob.data = carved
    bpy.data.meshes.remove(old)
    bpy.data.objects.remove(cut, do_unlink=True)

    bm = bmesh.new()
    bm.from_mesh(ob.data)
    bmesh.ops.dissolve_degenerate(bm, dist=1e-4, edges=bm.edges)
    bmesh.ops.triangulate(bm, faces=[f for f in bm.faces if len(f.verts) > 4])
    bm.normal_update()
    floors, walls = [], []

    def below(p):
        loc, nrm, _, dist = tree.find_nearest(p)
        return 0.0 if loc is None or (p - loc).dot(nrm) > 0 else dist
    for f in bm.faces:
        if below(f.calc_center_median()) < 0.0015:
            continue
        (floors if min(below(v.co) for v in f.verts) > depth * 0.6 else walls).append(f)
    K._project_bm(bm, "box", floors + walls)
    K._paint_bm(bm, floor, "flat", floors)
    K._paint_bm(bm, wall, "flat", walls)
    for f in floors + walls:
        f.smooth = False
    pocket = set(floors + walls)
    # the pockets go to their own mesh (matte_carve: the rig makes it fully matte, gloss -2; a dark floor on the
    # glossy shell caught the sky's sheen and read light grey in the showcase), the shell keeps the rest
    bm.faces.index_update()
    bm2 = bm.copy()
    bm2.faces.index_update()
    keep_idx = {f.index for f in pocket}
    bmesh.ops.delete(bm2, geom=[f for f in bm2.faces if f.index not in keep_idx], context="FACES")
    bmesh.ops.delete(bm, geom=list(pocket), context="FACES")
    bm.to_mesh(ob.data)
    bm.free()
    me = bpy.data.meshes.new("matte_carve")
    bm2.to_mesh(me)
    bm2.free()
    pk = K._link(bpy.data.objects.new("matte_carve", me))
    pk.matrix_world = ob.matrix_world.copy()
    return ob, pk


def flat_lock(name, ctrl, w, out, thin=0.32, sides=8, per=2, color=HAIR, streak=True):
    """One flat hair lock (the witch's): lens section, wide almost to the end, a ROUNDED end; alternate locks
    carry a light streak down their outer face, the underside a step darker."""
    pts, par = KK.catmull(ctrl, per=per)
    rr = [w * (0.8 + 0.28 * math.sin(math.pi * min(t * 1.5, 1.0))) * max(1 - t ** 5, 0.0) ** 0.45 for t in par]
    d = (pts[-1] - pts[-2]).normalized()
    re = w * 0.7
    pts[-1:] = [pts[-1] - d * re * 0.2, pts[-1] + d * re * 0.25, pts[-1] + d * re * 0.45]
    rr[-1:] = [re, re * 0.62, 0.0]
    last = len(pts) - 2

    def flat(a, i):
        return 1.0 / math.sqrt((math.cos(a) / thin) ** 2 + math.sin(a) ** 2)

    def paint(i, j):
        if streak and 1 <= i < last - 1 and j in (0, sides - 1):
            return (HAIR_LT, "flat")
        if j in (sides // 2 - 1, sides // 2):
            return (HAIR_DK, "flat")
        return None
    return D.sweep(name, pts, rr, sides=sides, color=color, mat="flat", project="box", up=out, warp=flat, paint=paint)


# ================================================================== head: the pumpkin-top cap
def cap_rim(a, tip):
    """Height of the cap's lower edge at azimuth a (tip = lobe height there, 1 at a lobe tip): high over the
    forehead (the bangs come out from under it), above the ears at the sides (the hair and the ears show
    below it, as in the mock), low over the back of the head."""
    fr = max(0.0, -math.sin(a)) ** 1.8
    bk = max(0.0, math.sin(a))
    return 0.8 + 0.192 * fr - 0.125 * bk ** 1.5 - 0.036 * tip + 0.004


def pumpkin_cap(NL=CAP_NL, SPL=8, NR=12, rib=0.1, th=0.014):
    """The top of a pumpkin worn as a cap: a ribbed orange shell (NL lobes, one centred on the front) whose
    lower edge is scalloped into rounded lobe tips, with a thick turned lip and a darker inside."""
    C, R, ryf = CC, CR, RYF
    SEG = NL * SPL
    a0 = -math.pi / 2
    bm = bmesh.new()
    pole = bm.verts.new(C + V((0, 0, R.z)))
    cols = []
    for j in range(SEG):
        a = a0 + 2 * math.pi * j / SEG
        u, tip = lobe(a, a0, NL)
        m = 1 + rib * (tip ** 0.5 - 0.75)
        cphi = max(-0.95, min(0.95, (cap_rim(a, tip) - C.z) / R.z))
        phr = math.acos(cphi)
        ry = R.y if math.sin(a) >= 0 else ryf
        col = []
        for k in range(1, NR + 1):
            phi = phr * math.sin(0.5 * math.pi * k / NR)
            mm = 1 + (m - 1) * min(1.0, phi / 0.5)          # the ribs fade into the crown round the stem
            p = C + V((math.cos(a) * math.sin(phi) * R.x * mm, math.sin(a) * math.sin(phi) * ry * mm, math.cos(phi) * R.z))
            col.append(p)
        rimp = col[-1]
        n = V(((rimp.x - C.x) / R.x ** 2, (rimp.y - C.y) / ry ** 2, (rimp.z - C.z) / R.z ** 2)).normalized()
        col.append(rimp - n * th * 0.5 + V((0, 0, -0.006)))      # rolled lip
        col.append(rimp - n * th + V((0, 0, 0.003)))
        q = col[-4]
        col.append(q - n * th)                                    # inner skirt
        cols.append([bm.verts.new(p) for p in col])
    NB = len(cols[0])
    outer, lip, inner = [], [], []
    for j in range(SEG):
        A, B = cols[j], cols[(j + 1) % SEG]
        outer.append(bm.faces.new((pole, A[0], B[0])))
        for k in range(NB - 1):
            f = bm.faces.new((A[k], A[k + 1], B[k + 1], B[k]))
            (outer if k < NR - 1 else lip if k < NR + 1 else inner).append(f)
    bm.normal_update()
    f0 = outer[SEG // 2]
    if f0.normal.dot(f0.calc_center_median() - C) < 0:
        bmesh.ops.reverse_faces(bm, faces=bm.faces)
    return D.mesh(bm, "cap", P, SUIT, "sphere", smooth=True, center=C,
                  extra=[(lip, P, SUIT), (inner, PD, SUIT)])


def rim_at(p):
    a = math.atan2(p.y - CC.y, p.x)
    return cap_rim(a, lobe(a, -math.pi / 2, CAP_NL)[1])


def cap_crease_ao(p):
    """Soft darkening down the cap's creases (AO multiplier for the head mesh; only points on the cap shell)."""
    d = p - CC
    ry = RYF if d.y < 0 else CR.y
    e = math.sqrt((d.x / CR.x) ** 2 + (d.y / ry) ** 2 + (d.z / CR.z) ** 2)
    if not (0.86 < e < 1.15) or p.z < rim_at(p) - 0.03:
        return 1.0
    a = math.atan2(d.y, d.x)
    _, tip = lobe(a, -math.pi / 2, CAP_NL)
    fade = min(1.0, math.acos(max(-1.0, min(1.0, d.z / (CR.z * e)))) / 0.5)
    return 1.0 - 0.45 * fade * (1.0 - tip) ** 2.5


def cap_surf(d):
    p, n = KK.ellipsoid_dir(CC, V((CR.x, RYF if d[1] < 0 else CR.y, CR.z)), d)
    return p + n * 0.006, n


# ------------------------------------------------------------------ hair under the cap
def at_head(phi, r, z):
    a = math.radians(phi)
    return V((math.cos(a) * r, HY + math.sin(a) * r * 0.97, z))


def hang_ctrl(phi, z0, zend, r0=0.19, r_mid=0.238, r_flip=0.258, sway=0.0, r1=0.222):
    return [at_head(phi, r0, z0), at_head(phi + sway * 0.3, r1, z0 - 0.04), at_head(phi - sway * 0.3, r_mid, z0 - 0.09),
            at_head(phi + sway * 0.4, r_mid - 0.004, (z0 + zend) / 2 - 0.02), at_head(phi + sway, r_flip, zend)]


def hair():
    """The hair under the cap: a scalp volume (face cut out), small ears, flat locks round the sides and the
    back hanging out from under the cap's rim (flipping out at the ends), and the swept bangs (fringe2)."""
    def cut(c):
        return c.y < -0.07 and (c.x / 0.176) ** 2 + ((c.z - 0.80) / 0.17) ** 2 < 1.0
    parts = [KK.blob("scalp", SCALP_C, SCALP_R, segs=20, rings=11, color=HAIR_DK, mat="flat", cut=cut)]
    for sx in (1, -1):
        ear = KK.blob(f"ear{sx}", (0, 0, 0), (0.024, 0.033, 0.014), segs=10, rings=6, color="skin", mat="flat")
        D.put(ear, V((0.222 * sx, -0.05, 0.772)), V((sx, -0.2, 0)), roll=0)
        inner = KK.blob(f"earin{sx}", (0, 0, 0), (0.013, 0.02, 0.006), segs=8, rings=4, color="skin_shade", mat="flat")
        D.put(inner, V((0.234 * sx, -0.054, 0.772)), V((sx, -0.2, 0)), roll=0)
        parts += [ear, inner]
    locks = []
    for sx in (1, -1):
        def ph(p):
            return p if sx > 0 else 180 - p
        # a face-framing lock in front of the ear, curling in toward the cheek
        phi = ph(-30)
        ctrl = [at_head(phi, 0.19, 0.84), at_head(phi + 2 * sx, 0.222, 0.81), at_head(phi + 5 * sx, 0.232, 0.765),
                at_head(phi + 3 * sx, 0.236, 0.725), at_head(phi - 4 * sx, 0.252, 0.7)]
        locks.append(flat_lock(f"hf{sx}", ctrl, 0.034, at_head(phi, 1, 0) - at_head(phi, 0, 0), streak=True, sides=6))
        # behind the ear down to the jaw, flipping out
        for k, (p0, zend, w) in enumerate(((10, 0.69, 0.042), (34, 0.675, 0.046))):
            phi = ph(p0)
            z0 = min(0.83, rim_at(at_head(phi, 0.2, 0.8)) + 0.022)       # the roots just inside the rim
            locks.append(flat_lock(f"hs{sx}{k}", hang_ctrl(phi, z0, zend, sway=(-7 if k else 6) * sx), w,
                                   at_head(phi, 1, 0) - at_head(phi, 0, 0), streak=k == 0, sides=6))
    # (the back of the head: the cap comes down to the nape; separate locks hanging under it read as dreadlocks)
    parts += locks
    parts += KK.fringe2(color=HAIR, light=HAIR_LT, n=7, z_top=0.972, length=0.074, part=2.6, locks=False)
    return parts


# ------------------------------------------------------------------ cap parts the rig springs
TOP = CC + V((0, 0, CR.z - 0.016))


def stem():
    """soft_stem: a thick ribbed green stem growing up out of the flare and hooking over to the kid's left, a
    rounded end. Origin at its base."""
    base = TOP + V((0, 0, -0.008))
    ctrl = [base, TOP + V((0.0, 0.0, 0.045)), TOP + V((0.008, 0.003, 0.088)), TOP + V((0.034, 0.005, 0.118)),
            TOP + V((0.066, 0.004, 0.124)), TOP + V((0.088, 0.0, 0.106))]
    pts, par = KK.catmull(ctrl, per=3)
    rr = [KK.lerp_list([0.034, 0.029, 0.025, 0.022, 0.019, 0.017], t) for t in par]
    ob = KK.limb("stem_m", pts, rr, sides=10, color=STEM, mat=GLOSS, cap_a=False, cap_b=True,
                 warp=lambda a, i: 1.0 + 0.09 * math.cos(5 * a),
                 paint=lambda i, j: (STEM_DK, "flat") if j % 2 == 1 else None)
    return KK.soft_part("soft_stem", [ob], base)


def stem_flare():
    """The stem's flared foot on the cap (static, in the head)."""
    return D.lathe("stem_base", [(0.0, -0.014), (0.064, -0.014), (0.058, 0.0), (0.043, 0.012), (0.035, 0.028), (0.0, 0.03)],
                   20, color=STEM, mat=GLOSS, project="cyl", loc=TOP + V((0, 0, 0.004)),
                   warp=lambda c: V((c.x, c.y, c.z)) * (1.0 + 0.16 * math.cos(5 * math.atan2(c.y, c.x))))


def leaves():
    """bob_leaf_a (front, the kid's right) and bob_leaf_b (front-left): two big leaves lying on the cap by the
    stem, tips lifting off it; each origin at its stalk."""
    out = []
    for nm, (a, th0, span, W, col) in (("bob_leaf_a", (math.pi + 0.6, 0.15, 0.7, 0.062, LEAF)),
                                       ("bob_leaf_b", (-0.55, 0.17, 0.72, 0.066, LEAF2))):
        ob, stalk = hug_leaf(nm + "_m", cap_surf, a, th0, span, W, CR.x, lift=0.008, tip_lift=0.034, cup=0.012, color=col)
        out.append((nm, KK.bob_part(nm, [ob], stalk), "head"))
    return out


def vines():
    """soft_vine_a / _b: corkscrew tendrils springing from the stem's foot, out to either side and up."""
    out = []
    for nm, (d0, rise, L, turns, coil, r0) in (("soft_vine_a", ((-1.0, 0.45, 0.35), (-0.4, 0.2, 1.0), 0.16, 2.6, 0.016, 0.0055)),
                                               ("soft_vine_b", ((1.0, -0.3, 0.25), (0.5, -0.1, 1.0), 0.14, 2.3, 0.014, 0.0052))):
        d = V(d0).normalized()
        start = TOP + V((d.x * 0.04, d.y * 0.04, 0.008))
        ob = tendril(nm + "_m", start, d0, L, turns, coil, r0, rise=rise)
        out.append((nm, KK.soft_part(nm, [ob], start), "head"))
    return out


def head():
    static, children = KK.face2(fang=False)
    parts = static + hair()
    cap = pumpkin_cap()
    parts.append(cap)
    parts.append(stem_flare())
    # guard: no hair above the rim outside the cap shell (a ray from the cap centre to it must not cross the cap)
    bpy.context.view_layer.update()
    ctree = D.bvh(cap)
    out = {}
    for o in parts:
        if o is cap or o.type != "MESH" or not o.name.startswith(("scalp", "h", "bang")):
            continue
        for v in o.data.vertices:
            p = o.matrix_world @ v.co
            d = p - CC
            if p.z > rim_at(p) + 0.004 and d.length > 1e-4:
                loc, _ = D.hit(ctree, CC, d, d.length)
                if loc is not None:
                    out[o.name] = out.get(o.name, 0) + 1
    print("PUMPKIN hair vertices outside the cap:", out)
    children += [("soft_stem", stem(), "head")] + leaves() + vines()
    return D.part(parts, "head", KK.NECK), children


# ================================================================== body: the pumpkin suit
def body_profile():
    prof = [(0.0, -1.0)]
    for deg in range(9, 180, 9):
        phi = math.radians(deg)
        prof.append((math.sin(phi), -math.cos(phi)))
    prof.append((0.0, 1.0))
    return prof


def body_warp(c):
    a = math.atan2(c.y, c.x)
    r = math.hypot(c.x, c.y)
    u, tip = lobe(a, A0, NL)
    rib = 1.0 - RIB + RIB * tip ** 0.5
    zz = c.z
    if abs(zz) > 0.72:          # dimpled top (round the neck) and bottom (the legs come out there)
        zz -= math.copysign(0.3 * (1 - r) * (abs(zz) - 0.72) / 0.28, zz)
    rz = RZT if zz > 0 else RZB
    return V((r * rib * math.cos(a) * RX, r * rib * math.sin(a) * RY + BODY_Y, CZ + zz * rz))


def crease_ao(p):
    """Soft darkening down each crease of the body shell (AO multiplier)."""
    a = math.atan2(p.y - BODY_Y, p.x)
    _, tip = lobe(a, A0, NL)
    return 1.0 - 0.5 * (1.0 - tip) ** 2.5


def body():
    pump = D.lathe("pumpkin_body", body_profile(), NL * SPL, color=P, mat=SUIT, project="cyl", warp=body_warp,
                   center=(0, 0, CZ), phase=A0)
    tree = D.bvh(pump)
    PC = V((0, BODY_Y, CZ))

    def body_surf(d):
        loc, nrm = D.hit(tree, PC + V(d) * 0.6, -V(d), 1.2)
        if loc is None:
            return PC + V(d) * 0.2, V(d)
        return loc, nrm

    # the carved face: big triangle eyes, a triangle nose, a wide toothy grin (two teeth down, one up)
    eye = KK.rounded_poly([(-0.052, -0.033), (0.047, -0.033), (0.012, 0.045)], 0.007, 2)
    nose = KK.rounded_poly([(-0.022, -0.015), (0.022, -0.015), (0.0, 0.021)], 0.004, 2)
    Wg = 0.142

    def zb(x):
        u = x / Wg
        return -0.056 * (1 - u * u) + 0.026 * u * u

    def zt(x):
        u = x / Wg
        return -0.008 * (1 - u * u) + 0.032 * u * u

    def edge(xs, zf, teeth, drop):
        # (a grid point within 4 mm of a tooth's edge makes a sliver the exact boolean cannot cut: skipped)
        keys = [(x, None) for x in xs if not any(min(t) - 0.004 < x < max(t) + 0.004 for t in teeth)] + [(t[0], t) for t in teeth]
        keys.sort(key=lambda k: k[0], reverse=xs[0] > xs[-1])
        out = []
        for x, t in keys:
            if t is None:
                out.append((x, zf(x)))
            else:
                out += [(t[0], zf(t[0])), (t[0], zf(t[0]) + drop), (t[1], zf(t[1]) + drop), (t[1], zf(t[1]))]
        return out
    xs = [-Wg + 2 * Wg * i / 18 for i in range(19)]
    grin = edge(xs, zb, [(-0.014, 0.014)], 0.022) + edge(list(reversed(xs[1:-1])), zt, [(0.072, 0.036), (-0.036, -0.072)], -0.023)
    shapes = [(eye, (0.08, 0, 0.5)), ([(-x, z) for x, z in reversed(eye)], (-0.08, 0, 0.5)),
              (nose, (0.0, 0, 0.44)), (grin, (0.0, 0, 0.388))]
    n_before = len(pump.data.vertices)
    _, pocket = carve(pump, tree, shapes)
    n_shell = len(pump.data.vertices)
    print("PUMPKIN body shell verts", n_before, "->", n_shell, "pockets", len(pocket.data.polygons), "faces")
    assert n_shell > n_before * 0.8 and len(pocket.data.polygons) > 20, "the carve failed"
    parts = [pump]
    # collar of big veined leaves round the neck, flopping over the top of the pumpkin
    for i in range(6):
        a = A0 + 2 * math.pi * (i + 0.0) / 6
        lf, _ = hug_leaf(f"collar{i}", body_surf, a, math.radians(10), math.radians(52), 0.08, RX, lift=0.006,
                         tip_lift=0.05, cup=0.016, color=LEAF if i % 2 == 0 else LEAF2, nu=7, nv=4, pairs=2)
        parts.append(lf)
    parts.append(D.lathe("neck", [(0.0, 0.59), (0.118, 0.59), (0.112, 0.655), (0.0, 0.675)], 16, color=LEAF_IN, mat=PLUSH,
                         project="cyl"))
    # curly vines winding out from under the collar at the back of each side (v1's, lying on the shell)
    for sx in (1, -1):
        st, _ = body_surf(V((0.6 * sx, 0.5, 0.62)))
        parts.append(curl(f"vine{sx}", st + V((0, 0, 0.004)), (0.5 * sx, 0.4, -1), (sx, 0.3, 0.2), 0.27, 1.3, 0.0075,
                          n=18, tight=2.0, drift=0.05))
    ob = D.part(parts, "body", (0, 0, KK.HIP_Z))

    def post(o, vals):
        mw = o.matrix_world
        return [x * crease_ao(mw @ o.data.vertices[i].co) if i < n_shell else x for i, x in enumerate(vals)]
    KK.AO_POST["body"] = post
    return ob, D.part([pocket], "matte_carve", (0, 0, KK.HIP_Z))


# ================================================================== arms: bare arms in short puff sleeves
def puff_sleeve(name, s, e):
    """A short puffy orange sleeve round the shoulder: a lobed ball (six ribs, like the pumpkin) centred a little
    down the upper arm, so it turns about the shoulder pivot with the arm, and a darker rolled hem hugging the
    arm below it."""
    s, e = V(s), V(e)
    d = (e - s).normalized()
    out = V((1 if s.x > 0 else -1, 0, 0))
    c = s + d * 0.026 + out * 0.012

    def w(q):
        a = math.atan2(q.y, q.x)
        k = max(0.0, 1 - abs(q.z) / 0.066)
        return V((q.x, q.y, q.z)) * (1.0 + 0.085 * (abs(math.cos(3 * a)) ** 0.6 - 0.6) * k)
    pf = KK.blob(name + "_puff", (0, 0, 0), (0.072, 0.072, 0.066), segs=18, rings=10, color=P, mat=SUIT, warp=w)
    D.put(pf, c, d, up=(0, 0, 1))
    hem_c = c + d * 0.05
    ring = KK.ring(name + "_hem", hem_c, d, 0.044, 0.011, n=20, sides=8, color=PD, mat=SUIT, squash=0.9)
    return [pf, ring]


def bare_arm(s, sho, elb, wri):
    """arm_* (shoulder ball -> elbow ball, skin, the puff sleeve round the shoulder) and forearm_* (elbow ball
    -> a ball on the wrist that the hand's heel covers). Ball ends on both sides of the elbow: no gap at any
    bend."""
    S, E, W = V(sho), V(elb), V(wri)
    upts, par = KK.catmull([S, S.lerp(E, 0.5) + V((0.003 * (1 if S.x > 0 else -1), 0.003, 0)), E], per=3)
    upper = KK.limb(f"arm_{s}_upper", upts, [KK.lerp_list([0.05, 0.047, 0.043], t) for t in par], sides=12, color="skin",
                    mat="flat", project="sphere")
    fpts, par = KK.catmull([E, E.lerp(W, 0.5), W], per=3)
    fore = KK.limb(f"arm_{s}_fore", fpts, [KK.lerp_list([0.0415, 0.038, 0.031], t) for t in par], sides=12,
                   color="skin", mat="flat", project="sphere")
    return D.part([upper] + puff_sleeve(f"arm_{s}", S, E), "arm_" + s, S), D.part([fore], "forearm_" + s, E)


# ================================================================== legs: striped tights, orange high-tops
STRIPE = 0.021


def stocking(name, x):
    """The thigh as a green-and-black striped stocking (the witch's: a ring at every stripe edge, crisp bands)."""
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
    ob = D.lathe(name + "_thigh", prof, 16, color=SOCK, mat=PLUSH, project="cyl", loc=(x, 0, 0))
    K.paint(ob, SOCK_DARK, where=lambda c, n: KK.KNEE_Z <= c.z <= 0.316 and int((c.z - KK.KNEE_Z) / STRIPE) % 2 == 1)
    return ob


def shoe(name, x):
    """The kit's high-top in pumpkin colours, plus a green strap across the instep with a tab on the outside."""
    parts = KK.sneaker(name, x, SHOE)
    up = next(o for o in parts if o.name.startswith(name + "_upper"))
    tr = D.bvh(up)
    zs_, pts = 0.137, []
    for k in range(13):
        al = math.radians(-105 + 210 * k / 12)
        d = V((math.sin(al), -math.cos(al), 0.0))
        loc, nrm = D.hit(tr, V((x, -0.02, zs_)) + d * 0.3, -d, 0.6)
        if loc is not None:
            pts.append(loc + nrm * 0.005)
    if len(pts) > 3:
        parts.append(D.sweep(name + "_strap", pts, 0.017, sides=6, color=LEAF, mat=GLOSS, project="box", up=(0, 0, 1),
                             warp=lambda a, i: 1.0 / math.sqrt(math.cos(a) ** 2 + (math.sin(a) / 0.3) ** 2)))
        out = 1 if x > 0 else -1
        tab_p = pts[0] if out < 0 else pts[-1]
        tab = KK.blob(name + "_strap_tab", (0, 0, 0), (0.02, 0.018, 0.006), segs=8, rings=4, color="vine_dark", mat=GLOSS)
        D.put(tab, tab_p + V((0, 0.012, 0)) + V((out * 0.004, 0, 0)), V((out, 0, 0)), up=(0, 0, 1))
        parts.append(tab)
    return parts


# ================================================================== arm clearance (printed at build)
def arm_clearance(names):
    """How far the bare arm (below the puff) and the hand stay outside the pumpkin shell in the rest pose with
    the costume's arm_out and elbow_rest, in walk swings and in the review run pose. Printed; negative = inside."""
    # the shell's horizontal (unit) radius by height, sampled from the lathe profile through body_warp
    prof = []
    for k in range(361):
        th = math.pi * k / 360
        q = body_warp(V((math.sin(th), 0.0, -math.cos(th))))
        prof.append((q.z, math.sin(th)))

    def unit_r(z):
        best = 0.0
        for (za, ra), (zb, rb) in zip(prof[:-1], prof[1:]):
            if min(za, zb) <= z <= max(za, zb) and abs(zb - za) > 1e-9:
                best = max(best, ra + (rb - ra) * (z - za) / (zb - za))
        return best

    def inside(p):
        r = unit_r(p.z)
        if r <= 1e-4:
            return 1.0
        a = math.atan2(p.y - BODY_Y, p.x)
        _, tip = lobe(a, A0, NL)
        rib = 1.0 - RIB + RIB * tip ** 0.5
        e = math.hypot(p.x / (RX * rib * r), (p.y - BODY_Y) / (RY * rib * r))
        return (e - 1.0) * RX * rib * r

    def g_rot(x, z):
        # Godot E(0,0,arm_out) * E(x,0,0): Blender Rot(-z, Y) @ Rot(x, X)
        return Matrix.Rotation(-z, 4, "Y") @ Matrix.Rotation(x, 4, "X")
    rest = {nm: names[nm].matrix_basis.copy() for nm in ("arm_l", "arm_r", "forearm_l", "forearm_r", "hand_l", "hand_r")}
    cases = [("rest", 0.0, 0.0, 0.0, 0.0), ("walk_fwd", -0.55, 0.0, 0.45, 0.0), ("walk_back", 0.5, 0.0, -0.5, 0.0),
             ("run", -0.9, 1.3, 0.45, 0.5), ("cast_side", -0.3, 0.2, 0.2, 0.6)]
    for label, pl, fl, pr_, fr_ in cases:
        res = []
        for s, pitch, fold, sg in (("l", pl, fl, 1), ("r", pr_, fr_, -1)):
            arm, fore = names["arm_" + s], names["forearm_" + s]
            arm.matrix_basis = rest["arm_" + s] @ g_rot(pitch, sg * ARM_OUT)
            fore.matrix_basis = rest["forearm_" + s] @ Matrix.Rotation(-(fold + ELBOW_REST), 4, "X")
            bpy.context.view_layer.update()
            sho = arm.matrix_world.translation
            worst = 9.0
            for ob in (arm, fore, names["hand_" + s]):
                mw = ob.matrix_world
                for v in ob.data.vertices:
                    p = mw @ v.co
                    if ob is arm and (p - sho).length < 0.085:
                        continue                      # the puff sleeve round the shoulder sits on the pumpkin
                    worst = min(worst, inside(p))
            res.append(f"{s} {worst * 100:+.1f}cm")
        print("PUMPKIN arm clearance", label, ", ".join(res))
        for nm, m in rest.items():
            names[nm].matrix_basis = m
    bpy.context.view_layer.update()


# ================================================================== the costume
def build():
    """Returns (parts, children) for KK.finish2()."""
    KK.AO_SHAPE.clear()
    KK.AO_POST.clear()
    KK.POST_ATTACH[:] = [arm_clearance]
    KK.AO_IGNORE.clear()
    moving = {"soft_stem", "bob_leaf_a", "bob_leaf_b", "soft_vine_a", "soft_vine_b"}
    KK.AO_IGNORE["head"] = set(moving)
    for nm in moving:
        KK.AO_IGNORE[nm] = moving - {nm}
    body_ob, carve_ob = body()
    parts = {"body": body_ob}
    parts["head"], children = head()
    children.append(("matte_carve", carve_ob, "body"))
    KK.AO_SHAPE["head"] = cap_crease_ao
    KK.AO_IGNORE["matte_carve"] = {"arm_l", "arm_r", "forearm_l", "forearm_r", "hand_l", "hand_r", "bucket", "bucket_candy"}
    for s, hip in (("l", KK.HIP_L), ("r", KK.HIP_R)):
        x = hip[0] * 1.06                       # as KK.leg2
        parts["leg_" + s] = D.part([stocking("leg_" + s, x)], "leg_" + s, hip)
        parts["shin_" + s] = D.part(shoe("leg_" + s, x), "shin_" + s, (x, 0.0, KK.KNEE_Z))
    for s, sho, elb, wri in (("l", KK.SHO_L, KK.ELBOW_L, KK.WRIST_L), ("r", KK.SHO_R, KK.ELBOW_R, KK.WRIST_R)):
        parts["arm_" + s], parts["forearm_" + s] = bare_arm(s, sho, elb, wri)
    parts["hand_l"] = KK.grip_hand("hand_l", KK.WRIST_L, KK.WAND_GRIP, KK.WAND_DIR, (0.55, 0.83, 0.0), 0.0155, fat=1.08)
    parts["hand_r"] = KK.grip_hand("hand_r", KK.WRIST_R, KK.BUCKET_GRIP, (1, 0, 0), (0.0, -0.45, 0.9), 0.012, fat=1.08)
    return parts, children
