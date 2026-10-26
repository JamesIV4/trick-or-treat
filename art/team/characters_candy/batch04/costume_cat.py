"""kid_cat : one costume builder (see build_costumes.py for the run command and the rig contract).

Black plush cat onesie after the costume reference (ref/ref_cat.png): a puffy hood with a fluffy rim and big
pointed ears (pink inner ear, white fluff tufts), the kid's brown fringe under the rim, cat face paint
(pink nose, whiskers, a gold star on one cheek), a purple bow tie with a big gold jingle bell, a grey
plush belly oval, black paw mitts with pink toe beans and palm pads, fluffy cuffs, a long curling tail
with a little purple bow, and chunky black high-tops (purple padded collar, white sole, orange strap,
a little purple cat face on the instep)."""
import math

import bmesh  # noqa: F401
import bpy
from mathutils import Matrix  # noqa: F401

import kid_kit as KK
from kid_kit import D, K, V, FELT  # noqa: F401
from costume_common import bow

BLACK, FUR2, BELLY, BELLY_ST = "cat_black", "charcoal", "stone", "stone_dark"
PINK, FLUFF = "cheek_pink", "candy_rose_light"
BOW, BOW_DK = "wrapper_purple", "witch_purple"
GOLD, GOLD_DK = "foil_gold", "foil_gold_dark"


# ------------------------------------------------------------------ small helpers (cat only)
def _drop(objs):
    for ob in objs:
        bpy.data.objects.remove(ob, do_unlink=True)


def loft(name, sections, color, mat=FELT, project="sphere", center=None):
    """Closed skin through rings of equal length (bottom to top). A ring of one point is a pole;
    the first ring, when it is a real ring, gets a flat cap."""
    bm = bmesh.new()
    rings = [[bm.verts.new(V(p)) for p in ring] for ring in sections]
    for A, B in zip(rings[:-1], rings[1:]):
        n = max(len(A), len(B))
        for j in range(n):
            j2 = (j + 1) % n
            if len(A) == 1:
                bm.faces.new((A[0], B[j2], B[j]))
            elif len(B) == 1:
                bm.faces.new((A[j], A[j2], B[0]))
            else:
                bm.faces.new((A[j], A[j2], B[j2], B[j]))
    if len(rings[0]) > 1:
        bm.faces.new(list(reversed(rings[0])))
    if len(rings[-1]) > 1:
        bm.faces.new(rings[-1])
    bm.normal_update()
    cen = sum((v.co for v in bm.verts), V((0, 0, 0))) / len(bm.verts)
    out = sum((f.normal.dot(f.calc_center_median() - cen) for f in bm.faces))
    if out < 0:
        bmesh.ops.reverse_faces(bm, faces=bm.faces)
    return D.mesh(bm, name, color, mat, project, smooth=True, center=center)


def star(name, R, r, th, color, mat="flat"):
    """Five-point star prism in the local XY plane (front at +Z), tips rounded off a little."""
    bm = bmesh.new()
    pts = []
    for i in range(10):
        a = math.pi / 2 + math.pi * i / 5
        rr = R if i % 2 == 0 else r
        pts.append((math.cos(a) * rr, math.sin(a) * rr))
    top = [bm.verts.new((x, y, th)) for x, y in pts]
    bot = [bm.verts.new((x, y, 0.0)) for x, y in pts]
    ct, cb = bm.verts.new((0, 0, th * 1.4)), bm.verts.new((0, 0, 0))
    n = len(pts)
    for i in range(n):
        j = (i + 1) % n
        bm.faces.new((ct, top[i], top[j]))
        bm.faces.new((cb, bot[j], bot[i]))
        bm.faces.new((top[j], top[i], bot[i], bot[j]))
    bm.normal_update()
    for f in bm.faces:
        if f.normal.dot(f.calc_center_median() - V((0, 0, th * 0.5))) < 0:
            f.normal_flip()
    return D.mesh(bm, name, color, mat, "box", smooth=False)


def on_ell(c, M, R, u):
    """Point and normal on an ellipsoid (centre c, rotation M, radii R) in local direction u."""
    u = V(u).normalized()
    s = 1.0 / math.sqrt((u.x / R[0]) ** 2 + (u.y / R[1]) ** 2 + (u.z / R[2]) ** 2)
    p = u * s
    n = V((p.x / R[0] ** 2, p.y / R[1] ** 2, p.z / R[2] ** 2)).normalized()
    return V(c) + M @ p, (M @ n).normalized()


# ------------------------------------------------------------------ head
def cat_ear(sx):
    """Big soft pointed ear in local space: base on z=0, tip up +Z, front (pink) toward +Y.
    A thick black shell with a rounded-triangle outline, a recessed pink inner ear and white fluff tufts."""
    H, W, T = 0.205, 0.104, 0.042
    N = 14
    secs = []
    for k in range(9):
        t = k / 8
        w = W * (1 - t) ** 0.78 * (1 + 0.1 * math.sin(math.pi * t))
        d = T * (1 - t) ** 0.7 + 0.004 * (1 - t)
        lean = -0.012 * t * t                       # the tip curls back a touch
        if k == 8:
            secs.append([(0, lean, H)])
            continue
        ring = []
        for j in range(N):
            a = 2 * math.pi * j / N
            ca, sa = math.cos(a), math.sin(a)
            y = sa * d * (0.55 if sa > 0 else 1.0)    # flatter front, rounder back
            ring.append((ca * w, y + lean, t * H - 0.012))
        secs.append(ring)
    shell = loft(f"ear{sx}", secs, BLACK, FELT, center=(0, 0, H * 0.4))
    # pink inner ear: a thinner rounded triangle standing proud of the flat front, inset from the edge
    ins = []
    Hi, Wi = H * 0.8, W * 0.66
    for k in range(9):
        t = k / 8
        w = Wi * (1 - t) ** 0.85 * (1 + 0.08 * math.sin(math.pi * t))
        z = -0.006 + t * Hi
        ts = min(max((z + 0.012) / H, 0.0), 1.0)             # the shell's own height parameter here
        sf = 0.55 * (T * (1 - ts) ** 0.7 + 0.004 * (1 - ts)) - 0.012 * ts * ts     # the shell's flat front
        if k == 8:
            ins.append([(0, sf + 0.001, z)])
            continue
        ring = []
        for j in range(10):
            a = 2 * math.pi * j / 10
            ring.append((math.cos(a) * w, sf + 0.004 - 0.0075 * (1 - math.sin(a)), z))
        ins.append(ring)
    inner = loft(f"earin{sx}", ins, PINK, FELT, center=(0, 0.1, H * 0.4))
    parts = [shell, inner]
    # the pale fluffy fur at the bottom of the inner ear
    K.paint(inner, FLUFF, where=lambda c, n: c.z < 0.042)
    return K.merge(parts, f"ear{sx}")


def fur_rim(color, oa=KK.OA, oc=KK.OC, oz=KK.OZ, r=0.036):
    """The hood's face rim as a soft fluffy roll: more points than the kit's rim, with gentle tufted lumps."""
    NR = 44
    pts = []
    for i in range(NR):
        a = 2 * math.pi * i / NR
        p, n = KK.ellipsoid_pt(KK.HC, KK.HR, math.cos(a) * oa, oz + math.sin(a) * oc)
        pts.append(p - n * 0.004)

    def lump(a, i):
        return 1.0 + 0.11 * math.cos(2 * math.pi * i * 15 / NR) + 0.05 * math.cos(3 * a + i)
    return D.sweep("hood_rim", pts, r, sides=8, color=color, mat=FELT, project="sphere", closed=True, center=KK.HC,
                   warp=lump)


def build_head():
    head = KK.hood(BLACK, rim_color=BLACK, stitches=False)
    _drop([head.pop(1)])                       # swap the kit's smooth rim for a fluffy one
    head.append(fur_rim(BLACK))
    head += KK.face(fang=True)
    head += KK.fringe()
    # big pointed ears on the top corners of the hood, facing forward and splaying out
    for sx in (1, -1):
        p, n = KK.ellipsoid_dir(KK.HC, KK.HR, (0.56 * sx, 0.0, 0.83))
        e = cat_ear(sx)
        D.put(e, p - n * 0.016, n + V((0.08 * sx, -0.14, 0.6)), up=(0, -1, 0.15), roll=0)
        head.append(e)
    # a soft seam down the back of the hood (the gameplay camera mostly sees the back)
    st = []
    for i in range(13):
        a = math.radians(-8 + 9.5 * i)
        d = V((0.0, math.sin(a), math.cos(a)))
        p, n = KK.ellipsoid_dir(KK.HC, KK.HR, d)
        t = V((0.0, math.cos(a), -math.sin(a)))
        st.append((p, t, n))
    head.append(D.stitches("hood_seam", st, length=0.02, width=0.006, height=0.003, color=FUR2))
    # cat face paint: a pink rounded-triangle nose, three whiskers a side, a gold star on the right cheek
    p, n = KK.ellipsoid_pt(KK.FC, KK.FR, 0.0, 0.776)
    nose = K.polygon_prism("nose", KK.rounded_poly([(0.0, -0.011), (0.016, 0.006), (-0.016, 0.006)], 0.005, 2), 0.006,
                           color=PINK, mat="flat")
    D.put(nose, p - n * 0.002, n)
    head.append(nose)
    for sx in (1, -1):
        for k, (dz, ang) in enumerate([(0.013, 12), (0.0, 0), (-0.013, -12)]):
            pts = []
            for t in range(4):
                x = (0.112 + 0.018 * t) * sx
                z = 0.766 + dz + math.sin(math.radians(ang)) * 0.018 * t
                q, qn = KK.ellipsoid_pt(KK.FC, KK.FR, x, z)
                pts.append(q + qn * 0.0025)
            head.append(D.sweep(f"wh{sx}{k}", pts, [0.0034, 0.0032, 0.0028, 0.0], sides=4, color="ink", mat="flat",
                                project="box"))
    p, n = KK.ellipsoid_pt(KK.FC, KK.FR, -0.15, 0.808)
    s = star("star", 0.022, 0.0098, 0.004, GOLD)
    D.put(s, p - n * 0.001, n, roll=-14)
    head.append(s)
    for i, (x, z, r) in enumerate([(-0.176, 0.83, 0.0042), (-0.128, 0.832, 0.0034)]):
        q, qn = KK.ellipsoid_pt(KK.FC, KK.FR, x, z)
        head.append(KK.blob(f"sparkle{i}", q + qn * 0.001, (r, r, r * 0.6), segs=6, rings=3, color=GOLD, mat="flat"))
    head = D.part(head, "head", KK.NECK)
    KK.tilt_head(head)
    return head


# ------------------------------------------------------------------ body
def bow_tie(c, nrm, up, s=1.0):
    """Chunky purple bow tie (two flared wings with pleats and a knot) and a big gold jingle bell."""
    parts = []
    for sx in (1, -1):
        def wing_warp(p, sx=sx):
            k = 0.42 + 0.58 * min(abs(p.x) / 0.066, 1.0) ** 0.8
            return V((p.x, p.y * k, p.z * (0.7 + 0.3 * k)))
        wing = KK.blob(f"bt_wing{sx}", (sx * 0.04, 0, 0), (0.042, 0.032, 0.017), segs=12, rings=7, color=BOW, mat=FELT,
                       warp=wing_warp)
        parts.append(wing)
        for py in (0.011, -0.011):        # two pleats fanning out from the knot
            pts = []
            for t in (0.0, 0.33, 0.66, 1.0):
                x = sx * (0.02 + 0.045 * t)
                k = 0.42 + 0.58 * min(abs(x) / 0.066, 1.0) ** 0.8
                y = py * (0.6 + 0.9 * t)
                q = 1 - ((x - sx * 0.04) / 0.042) ** 2 - (y / (0.032 * k)) ** 2
                z = 0.017 * (0.7 + 0.3 * k) * math.sqrt(max(q, 0.0))
                pts.append(V((x, y, z + 0.0005)))
            parts.append(D.sweep(f"bt_pleat{sx}{py > 0}", pts, [0.0026, 0.0028, 0.0026, 0.0], sides=4, color=BOW_DK,
                                 mat=FELT, project="box"))
    parts.append(KK.blob("bt_knot", (0, 0, 0.006), (0.019, 0.022, 0.016), segs=9, rings=5, color=BOW, mat=FELT))
    # the jingle bell hangs from the knot on a little gold loop
    bz = -0.05
    parts.append(KK.blob("bell", (0, bz, 0.012), (0.034, 0.032, 0.033), segs=12, rings=8, color=GOLD, mat="metal"))
    parts.append(K.torus("bell_band", 0.0315, 0.0042, color=GOLD_DK, mat="metal", major_seg=14, minor_seg=4,
                         loc=(0, bz + 0.004, 0.012), rot=(90, 0, 0)))
    parts.append(K.torus("bell_loop", 0.008, 0.0028, color=GOLD_DK, mat="metal", major_seg=8, minor_seg=4,
                         loc=(0, bz + 0.034, 0.012), rot=(0, 90, 0)))
    parts.append(K.cylinder("bell_slot", 0.0032, 0.022, color="ink", mat="flat", verts=4, loc=(0, bz - 0.022, 0.04),
                            rot=(90, 0, 0), scale=(1.0, 1.0, 1.0)))
    parts.append(KK.blob("bell_hole", (0, bz - 0.012, 0.043), (0.0062, 0.0062, 0.003), segs=7, rings=3, color="ink",
                         mat="flat"))
    parts.append(KK.blob("bell_glint", (-0.012, bz + 0.012, 0.04), (0.0065, 0.005, 0.002), segs=6, rings=3,
                         color="lolly_white", mat="flat"))
    ob = K.merge(parts, "bowtie")
    D.put(ob, c, nrm, up=up, scale=(s, s, s))
    return ob


def build_body():
    tors = KK.torso(BLACK)
    tree = D.bvh(tors)
    belly, st = KK.patch("belly", tree, V((0, 0, 0.405)), 0.108, 0.148, BELLY, mat=FELT)
    body = [tors, belly, D.stitches("belly_st", st, length=0.017, width=0.0055, height=0.003, color=BELLY_ST)]
    # a soft fur collar ring where the hood meets the body (mostly hidden, fills the neck from behind)
    cp = []
    for i in range(24):
        a = 2 * math.pi * i / 24
        cp.append(V((math.cos(a) * 0.138, math.sin(a) * 0.128 + 0.006, 0.6 - 0.01 * math.sin(a))))
    body.append(D.sweep("collar", cp, 0.026, sides=7, color=BLACK, mat=FELT, closed=True, center=(0, 0, 0.6),
                        warp=lambda a, i: 1.0 + 0.1 * math.cos(i * 2.6)))
    body.append(bow_tie(V((0, -0.16, 0.588)), V((0, -1, 0.3)), (0, 0, 1), s=1.4))
    return D.part(body, "body", (0, 0, KK.HIP_Z))


# ------------------------------------------------------------------ legs
SHOE = dict(upper=BLACK, panel="candy_grape", toe="licorice_black", sole="lolly_white", stripe="witch_purple",
            lace="candy_orange", collar="candy_grape", tongue="candy_grape", heel="candy_grape", mat="vinyl")


def cat_leg(name, hip):
    """KK.leg's thigh + KK.sneaker, with the laces swapped for an orange strap and a purple cat-face badge."""
    x = hip[0] * 1.06
    thigh = D.lathe(name + "_thigh", [(0.0, 0.12), (0.074, 0.125), (0.08, 0.17), (0.084, 0.24), (0.074, 0.305), (0.0, 0.335)],
                    10, color=BLACK, mat=FELT, project="cyl", loc=(x, 0, 0))
    shoe = KK.sneaker(name, x, SHOE)
    laces = [o for o in shoe if "_lace" in o.name]
    shoe = [o for o in shoe if o not in laces]
    _drop(laces)
    upper = next(o for o in shoe if o.name.startswith(name + "_upper"))
    tree = D.bvh(upper)
    parts = [thigh] + shoe
    # orange velcro strap over the instep, wrapping a little down both sides
    rows = []
    for i in range(9):
        u = -0.064 + 0.128 * i / 8
        v = 0.008 * (1 - (u / 0.064) ** 2)
        rows.append((u, 0.085 + v + 0.0125, 0.085 + v - 0.0125))
    parts.append(KK.strip(name + "_strap", tree, rows, (x, -0.1, 0.0), (0, 1, 0.12), lift=0.001, thick=0.007,
                          color="candy_orange", mat="vinyl"))
    # little purple cat face on the instep above the strap: head, two ears, orange eyes, pink nose
    loc, nrm = KK.on_surface(tree, (x, -0.1, 0.124), (0, 1, -0.2))
    if loc is None:
        loc, nrm = V((x, -0.1, 0.124)), V((0, -1, 0.3))
    nrm = (nrm + V((0, -0.4, 0.1))).normalized()
    c = loc + nrm * 0.008
    M = D.look(nrm, up=(0, 0, 1))
    S = 1.3
    face = KK.blob(name + "_catface", (0, 0, 0), (0.033 * S, 0.024 * S, 0.011), segs=10, rings=5, color="candy_grape",
                   mat="vinyl")
    D.put(face, c, nrm, up=(0, 0, 1))
    parts.append(face)
    for sx in (1, -1):
        ear = K.cylinder(f"{name}_cfear{sx}", 0.012 * S, 0.022 * S, color="candy_grape", mat="vinyl", verts=4,
                         radius_top=0.0)
        ear.data.transform(Matrix.Diagonal((1.0, 0.5, 1.0, 1.0)))
        D.put(ear, c + M @ V((sx * 0.022 * S, 0.018 * S, 0.002)), M @ V((sx * 0.55, 1, 0)), up=nrm)
        parts.append(ear)
        eye = KK.blob(f"{name}_cfeye{sx}", (0, 0, 0), (0.0062 * S, 0.0075 * S, 0.0025), segs=6, rings=3,
                      color="candy_orange", mat="flat")
        D.put(eye, c + M @ V((sx * 0.013 * S, 0.003 * S, 0.0105)), nrm)
        parts.append(eye)
    nose = KK.blob(name + "_cfnose", (0, 0, 0), (0.0045 * S, 0.0035 * S, 0.0022), segs=6, rings=3, color=PINK, mat="flat")
    D.put(nose, c + M @ V((0, -0.008 * S, 0.011)), nrm)
    parts.append(nose)
    return D.part(parts, name, hip)


# ------------------------------------------------------------------ arms
def paw_extras(out):
    """Pink toe beans and a palm pad on the front/inner face of the black paw mitt, plus a fluffy cuff."""
    def extras(pts, rr, d):
        hand = pts[-1]
        mc = hand + d * 0.04
        M = D.look(d, up=(0, -1, 0))
        R = (0.042, 0.04, 0.05)
        f = V((-out * 0.62, 0.78, 0.0)).normalized()        # palm: front and inward
        s = V((0.78, out * 0.62, 0.0)).normalized()
        t = V((0, 0, 1))
        objs = []
        p, n = on_ell(mc, M, R, f + t * 0.12)
        pad = KK.blob("palm_pad", (0, 0, 0), (0.026, 0.021, 0.007), segs=10, rings=4, color=PINK, mat="flat")
        D.put(pad, p - n * 0.001, n, up=M @ t)
        objs.append(pad)
        for k, (o, tz) in enumerate([(-0.66, 0.66), (-0.22, 0.86), (0.22, 0.86), (0.66, 0.66)]):
            p, n = on_ell(mc, M, R, f + t * tz + s * o)
            bean = KK.blob(f"bean{k}", (0, 0, 0), (0.0102, 0.0114, 0.005), segs=7, rings=3, color=PINK, mat="flat")
            D.put(bean, p - n * 0.0005, n, up=M @ t)
            objs.append(bean)
        # fluffy cuff where the sleeve meets the paw
        cpts = []
        b0 = d.cross(V((0, 0, 1))).normalized()
        b1 = d.cross(b0).normalized()
        cen = hand - d * 0.01
        rad = rr[-1] * 0.84
        for i in range(16):
            a = 2 * math.pi * i / 16
            cpts.append(cen + (b0 * math.cos(a) + b1 * math.sin(a)) * rad)
        objs.append(D.sweep("cuff", cpts, 0.017, sides=6, color=BLACK, mat=FELT, closed=True, center=cen,
                            warp=lambda a, i: 1.0 + 0.12 * math.cos(i * 2.4)))
        return objs
    return extras


# ------------------------------------------------------------------ tail
def build_tail():
    """Long plush tail in a soft S: out of the back with a low swoop, rising, the tip easing back out
    again (not a C curl), with a soft rounded tip. One continuous mesh: the game bends it with a
    sideways wave in the toon shader (tail_wave), travelling from the root to the tip."""
    ctrl = [(0, 0.1, 0.36), (0.0, 0.23, 0.3), (0.0, 0.36, 0.3), (0.0, 0.46, 0.38), (0.0, 0.51, 0.5),
            (0.0, 0.51, 0.62), (0.0, 0.53, 0.72), (0.0, 0.6, 0.8), (0.0, 0.66, 0.84)]
    rad = [0.062, 0.056, 0.051, 0.048, 0.046, 0.044, 0.042, 0.038, 0.03]
    pts, par = KK.catmull(ctrl, per=4)
    rr = [KK.lerp_list(rad, u) for u in par]
    d = (pts[-1] - pts[-2]).normalized()                     # a soft rounded tip: one more ring, then a pole
    pts += [pts[-1] + d * rr[-1] * 0.6, pts[-1] + d * rr[-1] * 0.95]
    rr += [rr[-1] * 0.7, 0.0]
    tube = D.sweep("tail_tube", pts, rr, sides=10, color=BLACK, mat=FELT, project="sphere", up=(0, 0, 1),
                   warp=lambda a, i: 1.0 + 0.045 * math.cos(4 * a + i * 1.7))
    j = int(len(pts) * 0.55)                                 # the purple bow, on the outside of the rise
    tan = (pts[j + 1] - pts[j - 1]).normalized()
    nrm = (V((0, 1, 0)) - tan * V((0, 1, 0)).dot(tan)).normalized()
    bw = bow("tail_bow", pts[j] + nrm * (rr[j] + 0.004), nrm, tan, "candy_grape", s=1.1)
    return D.part([tube, bw], "tail", KK.TAIL_BASE)

# ================================================================== CAT
def build():
    head = build_head()
    body = build_body()
    leg_l = cat_leg("leg_l", KK.HIP_L)
    leg_r = cat_leg("leg_r", KK.HIP_R)
    arm_l = KK.arm("arm_l", KK.SHO_L, KK.HAND_L, BLACK, hand_color=BLACK, hand_mat=FELT, extras=paw_extras(1))
    arm_r = KK.arm("arm_r", KK.SHO_R, KK.HAND_R, BLACK, hand_color=BLACK, hand_mat=FELT, extras=paw_extras(-1))
    tail = build_tail()
    return dict(body=body, head=head, tail=tail, leg_l=leg_l, leg_r=leg_r, arm_l=arm_l, arm_r=arm_r)
