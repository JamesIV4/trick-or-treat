"""kid_cat v2: the Black Cat on kit v2 (design: batch04/ref/ref_cat.png; v1: batch04/costume_cat.py).

A black plush onesie with a fluffy hood: big pointed ears with pink insides and pale fluff (soft_ear_l /
soft_ear_r, sprung by the rig: they fold back in a fall and perk up on landing), a fur rim round the face
opening with tufts that flare out at the cheeks, the kid's own face v2 and brown fringe, cat face paint
(pink button nose, three whiskers a side, a gold star on the right cheek), a purple satin bow tie
(bob_bow) with a big gold jingle bell hanging from its knot (bob_bell, under bob_bow), a grey plush belly
oval with a fluffy chest, black 1.4x paw mitts with five pink pads each (four toe beans and a palm pad)
still gripping the wand and the bucket, fluffy cuffs, black high-tops (purple collar, white toe and sole,
an orange strap, a purple cat face on the instep) and a thick tail in an undulating S with a fluffy tip
(ONE mesh, `tail`, origin at the tail base, 55 rings along it: the rig bends it and runs tail_wave down it).

Black reads as plush, not vinyl: charcoal on the plush layer, fur tufts tipped a step lighter (the
shaded cell catches the street light at night), no glossy parts except the bell.
"""
import math

import bmesh  # noqa: F401
from mathutils import Matrix

import kid_kit as KK
from kid_kit import D, K, V, PLUSH
from costume_common import bow

BLK, BLK_HI, BLK_DK = "charcoal", "stone_dark", "cat_black"     # plush black, fur tips, seams
BELLY, BELLY_DK = "stone", "stone_dark"
PINK, FLUFF = "cheek_pink", "candy_rose_light"
BOW, BOW_DK = "wrapper_purple", "witch_purple"
GOLD, GOLD_DK = "foil_gold", "foil_gold_dark"
SHOE = dict(upper=BLK_DK, panel="candy_grape", toe="lolly_white", sole="lolly_white", stripe="witch_purple",
            lace="candy_orange", collar="candy_grape", tongue="candy_grape", heel="candy_grape", mat="vinyl")


def smooth01(t):
    t = min(max(t, 0.0), 1.0)
    return t * t * (3 - 2 * t)


# ------------------------------------------------------------------ fur and decals
def tuft(name, base, nrm, flow, size, color=BLK, n=3, spread=0.55, r=None, tip=BLK_HI, mat=PLUSH, per=2, sides=6, bend=1.0):
    """A fur tuft: n soft curved flames rising from 'base' along 'nrm', combed toward 'flow' and fanned out
    sideways, their tips a step lighter (tip colour) so the fur catches the light. About 70 triangles a
    flame (per=2, sides=6: 5 sides and one ring per span read as faceted crystals in the engine)."""
    nrm = V(nrm).normalized()
    flow = V(flow)
    flow = flow - nrm * flow.dot(nrm)
    flow = flow.normalized() if flow.length > 1e-6 else nrm.orthogonal().normalized()
    side = nrm.cross(flow).normalized()
    r = r or size * 0.42
    base = V(base)
    objs = []
    for k in range(n):
        u = (k - (n - 1) / 2) / ((n - 1) / 2) if n > 1 else 0.0
        ln = size * (1.0 - 0.2 * abs(u))
        sw = side * u * spread * size
        b = bend
        up = 1.0 + (1.0 - b) * 0.9                    # a straighter tuft stands further out
        ctrl = [base - nrm * r * 0.7, base + nrm * ln * 0.3 * up + flow * ln * 0.14 * b + sw * 0.35,
                base + nrm * ln * 0.45 * up + flow * ln * 0.52 * b + sw * 0.75, base + nrm * ln * 0.5 * up + flow * ln * 0.92 * b + sw]
        pts, par = KK.catmull(ctrl, per=per)
        # a soft flame: fat at the root, bellying out, then a short point (not a thin spike)
        rr = [r * (1.0 - 0.22 * abs(u)) * (1 - t) ** 0.6 * (1 + 0.35 * math.sin(math.pi * min(t * 1.6, 1.0))) for t in par]
        rr[-1] = 0.0
        last = len(pts) - 1
        objs.append(D.sweep(f"{name}_{k}", pts, rr, sides=sides, color=color, mat=mat, project="box",
                            paint=(lambda i, j, last=last: (tip, mat) if tip and i >= last - 1 - per else None)))
    return K.merge(objs, name)


def clump(name, base, dirn, length, r, color=BLK, sag=(0, 0, -1), mat=PLUSH):
    """A soft fur clump: a fat smooth teardrop from 'base' out along 'dirn', its point sagging toward
    'sag'. Thin tufts read as claws or thorns at showcase distance; clumps read as fluff."""
    d = V(dirn).normalized()
    s = V(sag)
    s = (s - d * s.dot(d))
    s = s.normalized() if s.length > 1e-6 else V((0, 0, 0))
    base = V(base)
    ts = [0.0, 0.18, 0.4, 0.62, 0.82, 1.0]
    pts = [base + d * length * t + s * length * 0.35 * t * t for t in ts]
    rr = [r * 0.75, r, r * 0.95, r * 0.72, r * 0.38, 0.0]
    return D.sweep(name, pts, rr, sides=8, color=color, mat=mat, project="box",
                   warp=lambda a, i: 1.0 + 0.12 * math.cos(2 * a + i))


def poly_decal(S, name, outline, color, lift=0.0025, puff=0.001, sink=0.002, rings=3, mat="flat"):
    """A pillowy decal with any outline [(x, y)] on a kit Surf (fan from the outline's centroid, rings
    shrinking toward it, edge tucked 'sink' under the surface)."""
    n = len(outline)
    cx = sum(p[0] for p in outline) / n
    cy = sum(p[1] for p in outline) / n
    bm = bmesh.new()
    c = bm.verts.new(S.pt(cx, cy, lift + puff))
    prev = None
    for i in range(1, rings + 1):
        f = i / rings
        cur = [bm.verts.new(S.pt(cx + (x - cx) * f, cy + (y - cy) * f, lift + puff * (1 - f ** 2.5))) for x, y in outline]
        for j in range(n):
            j2 = (j + 1) % n
            if prev is None:
                bm.faces.new((c, cur[j], cur[j2]))
            else:
                bm.faces.new((prev[j], cur[j], cur[j2], prev[j2]))
        prev = cur
    skirt = [bm.verts.new(S.pt(x, y, -sink)) for x, y in outline]
    for j in range(n):
        j2 = (j + 1) % n
        bm.faces.new((prev[j], skirt[j], skirt[j2], prev[j2]))
    bm.normal_update()
    if sum((f.normal.dot(S.n) for f in bm.faces), 0.0) < 0:
        bmesh.ops.reverse_faces(bm, faces=bm.faces)
    return D.mesh(bm, name, color, mat, "box", smooth=True)


def star_outline(R, r, rot=0.0):
    pts = []
    for i in range(10):
        a = math.pi / 2 + math.pi * i / 5 + math.radians(rot)
        rr = R if i % 2 == 0 else r
        pts.append((math.cos(a) * rr, math.sin(a) * rr))
    return pts


def on_ell(c, M, R, u):
    """Point and normal on an ellipsoid (centre c, rotation M, radii R) in local direction u."""
    u = V(u).normalized()
    s = 1.0 / math.sqrt((u.x / R[0]) ** 2 + (u.y / R[1]) ** 2 + (u.z / R[2]) ** 2)
    p = u * s
    n = V((p.x / R[0] ** 2, p.y / R[1] ** 2, p.z / R[2] ** 2)).normalized()
    return V(c) + M @ p, (M @ n).normalized()


def loft(name, sections, color, mat=PLUSH, project="sphere", center=None):
    """Closed skin through rings of equal length (bottom to top). A one-point ring is a pole."""
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
    if sum((f.normal.dot(f.calc_center_median() - cen) for f in bm.faces)) < 0:
        bmesh.ops.reverse_faces(bm, faces=bm.faces)
    return D.mesh(bm, name, color, mat, project, smooth=True, center=center)


# ------------------------------------------------------------------ ears (soft_ear_l / soft_ear_r)
EAR_H, EAR_W, EAR_T = 0.228, 0.118, 0.046


def ear_local(sx):
    """A big soft pointed ear in local space: base on z = 0, tip up +Z, front (pink) toward +Y. A thick
    plush shell with a rounded-triangle outline (the outer edge a little fuller), a recessed pink inner
    ear and pale fluff tufts curling out of its base."""
    H, W, T = EAR_H, EAR_W, EAR_T
    N, NS = 20, 13
    secs = []
    for k in range(NS):
        t = k / (NS - 1)
        w = W * (1 - t) ** 0.8 * (1 + 0.12 * math.sin(math.pi * t))
        d = T * (1 - t) ** 0.7 + 0.004 * (1 - t)
        lean = -0.016 * t * t                       # the tip curls back a touch
        if k == NS - 1:
            secs.append([(0, lean, H)])
            continue
        ring = []
        for j in range(N):
            a = 2 * math.pi * j / N
            ca, sa = math.cos(a), math.sin(a)
            y = sa * d * (0.5 if sa > 0 else 1.0)    # flat front, round back
            ring.append((ca * w * (1 + 0.06 * ca * sx), y + lean, t * H - 0.014))
        secs.append(ring)
    shell = loft(f"ear{sx}", secs, BLK, PLUSH, center=(0, 0, H * 0.4))
    ins = []
    Hi, Wi = H * 0.8, W * 0.64
    for k in range(10):
        t = k / 9
        w = Wi * (1 - t) ** 0.85 * (1 + 0.08 * math.sin(math.pi * t))
        z = -0.004 + t * Hi
        ts = min(max((z + 0.014) / H, 0.0), 1.0)
        sf = 0.5 * (T * (1 - ts) ** 0.7 + 0.004 * (1 - ts)) - 0.016 * ts * ts     # the shell's flat front here
        if k == 9:
            ins.append([(0, sf + 0.001, z)])
            continue
        ring = []
        for j in range(14):
            a = 2 * math.pi * j / 14
            ring.append((math.cos(a) * w, sf + 0.0035 - 0.008 * (1 - math.sin(a)), z))
        ins.append(ring)
    inner = loft(f"earin{sx}", ins, PINK, PLUSH, center=(0, 0.1, H * 0.4))
    parts = [shell, inner]
    # pale fluff curling up out of the bottom of the inner ear
    for k, (x, z, s) in enumerate([(-0.022, 0.014, 0.052), (0.008, 0.01, 0.06), (0.03, 0.02, 0.046)]):
        base = V((x * sx, 0.006, z))
        parts.append(tuft(f"efl{sx}{k}", base, (0.25 * x * sx / 0.03, 0.55, 1.0), (x * sx * 4, 0.3, 1.0), s, color=FLUFF,
                          n=2, spread=0.5, r=0.0135, tip="trim_white"))
    return K.merge(parts, f"ear{sx}")


def ears():
    """[(name, object, 'head')] with each ear's origin (the pin) at the middle of its base on the hood."""
    out = []
    for sx, s in ((1, "l"), (-1, "r")):
        p, n = KK.ellipsoid_dir(KK.HC, KK.HR, (0.56 * sx, 0.02, 0.83))
        e = ear_local(sx)
        base = p - n * 0.016
        D.put(e, base, n + V((0.1 * sx, -0.12, 0.62)), up=(0, -1, 0.15))
        out.append(("soft_ear_" + s, KK.soft_part("soft_ear_" + s, [e], base), "head"))
    return out


# ------------------------------------------------------------------ head (origin at the neck)
def fur_rim():
    """The hood's face rim as a fat fluffy roll (lumpy, more points than the kit's rim) with fur tufts
    sticking out of it all round, longer and flaring out at the cheeks (the mock's fluffy hood edge)."""
    NR = 52
    pts, nrms = [], []
    for i in range(NR):
        a = 2 * math.pi * i / NR
        p, n = KK.ellipsoid_pt(KK.HC, KK.HR, math.cos(a) * KK.OA, KK.OZ + math.sin(a) * KK.OC)
        pts.append(p - n * 0.004)
        nrms.append(n)

    def cheek_of(i):
        ang = 2 * math.pi * i / NR
        return abs(math.cos(ang)) ** 1.5 * (1.0 if math.sin(ang) < 0.35 else 0.4)

    def lump(a, i):
        # fat soft lumps of fur, each a little different; the roll is fuller at the cheeks (the mock's
        # fluffy hood edge flares out beside the face)
        return (1.0 + 0.16 * math.cos(2 * math.pi * i * 15 / NR) + 0.07 * math.cos(2 * math.pi * i * 7 / NR + 1.3)
                + 0.05 * math.cos(3 * a + i) + 0.42 * cheek_of(i))
    # (No tufts or clumps sticking out of the roll: tried three ways, every protrusion read as a claw or a
    # thorn at showcase distance. The roll's own lumps and its fuller cheeks are the fur.)
    return [D.sweep("hood_rim", pts, 0.04, sides=10, color=BLK, mat=PLUSH, project="sphere", closed=True, center=KK.HC,
                    warp=lump)]


def face_paint(skin):
    """The cat face paint on the face skin: pink button nose, three whiskers a side, the gold star on the
    kid's right cheek with two sparkles."""
    tree = D.bvh(skin)
    out = []
    p, n = KK.ellipsoid_pt(KK.FC, KK.FR, 0.0, 0.776)
    S = KK.Surf(tree, p, n)
    nose = [(x, y) for x, y in KK.rounded_poly([(0.0, -0.0105), (0.0155, 0.0065), (-0.0155, 0.0065)], 0.0055, 3)]
    out.append(poly_decal(S, "nose_paint", nose, PINK, lift=0.0056, puff=0.0024, sink=0.0035, rings=3))
    for sx in (1, -1):
        drop = 0.0 if sx > 0 else 0.022          # the starred cheek's whiskers sit a little lower, under the star
        for k, (z0, ang) in enumerate([(0.755, 10), (0.744, 0), (0.733, -10)]):
            pts = []
            for t in range(5):
                x = (0.1 + 0.0135 * t) * sx
                z = z0 - drop + math.sin(math.radians(ang)) * 0.016 * t
                pts.append((x, z))
            ref = KK.ellipsoid_pt(KK.FC, KK.FR, 0.0, 0.754)[0]
            W = KK.Surf(tree, ref, (0, -1, 0))
            # (Surf x runs to the kid's left for a board facing front; y is up)
            pts2 = [(x, z - 0.754) for x, z in pts]
            out.append(W.tube(f"wh{sx}{k}", pts2, [0.0028, 0.0027, 0.0025, 0.0018, 0.0], "ink", lift=0.0028, sides=5))
    # (inside the cheek's fuller fur roll, which covers the face beyond x ~0.125; below the eye's corner)
    p, n = KK.ellipsoid_pt(KK.FC, KK.FR, -0.117, 0.756)
    S = KK.Surf(tree, p, n, roll=14)
    out.append(poly_decal(S, "star", star_outline(0.0175, 0.008), GOLD, lift=0.0032, puff=0.0014, sink=0.002, rings=2))
    for i, (x, z, r) in enumerate([(-0.132, 0.782, 0.0036), (-0.097, 0.748, 0.003)]):
        q, qn = KK.ellipsoid_pt(KK.FC, KK.FR, x, z)
        S = KK.Surf(tree, q, qn)
        out.append(S.disc(f"sparkle{i}", 0, 0, r, r, GOLD, lift=0.0028, puff=0.0012, segs=8, rings=2, sink=0.0015))
    return out


def head():
    import bpy
    hood = KK.hood2(BLK, rim_color=BLK, mat=PLUSH, stitches=0, segs=36, rings=22)
    bpy.data.objects.remove(hood.pop(1), do_unlink=True)
    hood += fur_rim()                             # the kit's smooth rim swapped for the fluffy one
    static, children = KK.face2(fang=True)
    parts = static + face_paint(static[0]) + KK.fringe2(clip=KK.hood_clip())
    # a soft seam down the back of the hood
    st = []
    for i in range(13):
        a = math.radians(-8 + 9.5 * i)
        d = V((0.0, math.sin(a), math.cos(a)))
        p, n = KK.ellipsoid_dir(KK.HC, KK.HR, d)
        st.append((p, V((0.0, math.cos(a), -math.sin(a))), n))
    hood.append(D.stitches("hood_seam", st, length=0.02, width=0.006, height=0.003, color=BLK_DK))
    # Final fixes 2 (2026-11-16): the hood (with its fur roll and seam) is a static node of its own under the head, so
    # the rig can make the plush matte (KidTuning::plush_gloss) without touching the face's sheen. Same geometry as
    # before; the parts that ignored the head in the AO bake ignore the hood too (AO_IGNORE in build()).
    children = children + [("hood", D.part(hood, "hood", KK.NECK), "head")]
    return D.part(parts, "head", KK.NECK), children + ears()


# ------------------------------------------------------------------ body (origin at the pelvis)
def body():
    torso = KK.torso2(BLK)
    tree = D.bvh(torso)
    PC, PRU, PRV = V((0, 0, 0.405)), 0.112, 0.15
    belly, st = KK.patch("belly", tree, PC, PRU, PRV, BELLY, mat=PLUSH, seg=30,
                         heights=(0.0085, 0.008, 0.0066, 0.0034, -0.002))
    del st                                      # (no stitching: the mock's belly is a plain fuzzy oval)
    parts = [torso, belly]
    btree = D.bvh(belly)
    # a fluffy chest: soft grey tufts along the top of the belly oval, under the bell
    for k, u in enumerate((-0.6, -0.2, 0.2, 0.6)):
        loc, nrm = D.hit(btree, PC + V((u * PRU * 0.9, -1.0, PRV * (0.8 - 0.25 * u * u))), (0, 1, 0))
        if loc is None:
            continue
        parts.append(clump(f"chest{k}", loc - nrm * 0.004, nrm * 0.6 + V((u * 0.2, 0, -1)), 0.034 - 0.005 * abs(u), 0.015,
                           color=BELLY))
    return D.part(parts, "body", (0, 0, KK.HIP_Z))


BOW_C, BOW_N, BOW_S = V((0, -0.162, 0.584)), V((0, -1, 0.3)), 1.5


def bow_tie():
    """bob_bow: chunky purple satin bow tie (two flared wings with pleats, a knot), origin at the knot."""
    parts = []
    for sx in (1, -1):
        def wing_warp(p, sx=sx):
            k = 0.42 + 0.58 * min(abs(p.x) / 0.066, 1.0) ** 0.8
            return V((p.x, p.y * k, p.z * (0.7 + 0.3 * k)))
        parts.append(KK.blob(f"bt_wing{sx}", (sx * 0.04, 0, 0), (0.042, 0.032, 0.017), segs=16, rings=8, color=BOW, mat="flat",
                             warp=wing_warp))
        for py in (0.011, -0.011):
            pts = []
            for t in (0.0, 0.33, 0.66, 1.0):
                x = sx * (0.02 + 0.045 * t)
                k = 0.42 + 0.58 * min(abs(x) / 0.066, 1.0) ** 0.8
                y = py * (0.6 + 0.9 * t)
                q = 1 - ((x - sx * 0.04) / 0.042) ** 2 - (y / (0.032 * k)) ** 2
                z = 0.017 * (0.7 + 0.3 * k) * math.sqrt(max(q, 0.0))
                pts.append(V((x, y, z + 0.0005)))
            parts.append(D.sweep(f"bt_pleat{sx}{py > 0}", pts, [0.0026, 0.0028, 0.0026, 0.0], sides=5, color=BOW_DK,
                                 mat="flat", project="box"))
    parts.append(KK.blob("bt_knot", (0, 0, 0.006), (0.019, 0.022, 0.016), segs=12, rings=6, color=BOW, mat="flat"))
    ob = K.merge(parts, "bowtie")
    D.put(ob, BOW_C, BOW_N, up=(0, 0, 1), scale=(BOW_S, BOW_S, BOW_S))
    return KK.bob_part("bob_bow", [ob], BOW_C)


def bell():
    """bob_bell: the big gold jingle bell, hanging from the knot on a little loop (origin at the loop)."""
    bz, R = -0.062, 0.036
    parts = [KK.blob("bell", (0, bz, 0.012), (R, R * 0.95, R), segs=18, rings=10, color=GOLD, mat="metal"),
             K.torus("bell_band", R * 0.94, 0.0042, color=GOLD_DK, mat="metal", major_seg=18, minor_seg=4,
                     loc=(0, bz + 0.005, 0.012), rot=(90, 0, 0)),
             K.torus("bell_loop", 0.0075, 0.0028, color=GOLD_DK, mat="metal", major_seg=10, minor_seg=4,
                     loc=(0, bz + R + 0.004, 0.006), rot=(0, 90, 0)),
             K.cylinder("bell_slot", 0.0034, 0.026, color="ink", mat="flat", verts=6, loc=(0, bz - 0.024, 0.012 + R * 0.8),
                        rot=(90, 0, 0)),
             KK.blob("bell_hole", (0, bz - 0.013, 0.012 + R * 0.93), (0.0066, 0.0066, 0.003), segs=8, rings=3, color="ink",
                     mat="flat")]
    ob = K.merge(parts, "bell_m")
    D.put(ob, BOW_C, BOW_N, up=(0, 0, 1), scale=(BOW_S, BOW_S, BOW_S))
    M = D.look(BOW_N, up=(0, 0, 1))
    pivot = BOW_C + M @ (V((0, bz + R + 0.008, 0.012)) * BOW_S)
    return KK.bob_part("bob_bell", [ob], pivot)


# ------------------------------------------------------------------ legs: thigh, and the high-top as the shin
def shoe(name, x):
    """KK.sneaker with the laces swapped for an orange strap and a purple cat face on the instep (v1's)."""
    parts = KK.sneaker(name, x, SHOE)
    laces = [o for o in parts if "_lace" in o.name]
    parts = [o for o in parts if o not in laces]
    import bpy
    for o in laces:
        bpy.data.objects.remove(o, do_unlink=True)
    upper = next(o for o in parts if o.name.startswith(name + "_upper"))
    tree = D.bvh(upper)
    rows = []
    for i in range(11):
        u = -0.066 + 0.132 * i / 10
        v = 0.008 * (1 - (u / 0.066) ** 2)
        rows.append((u, 0.085 + v + 0.0125, 0.085 + v - 0.0125))
    parts.append(KK.strip(name + "_strap", tree, rows, (x, -0.1, 0.0), (0, 1, 0.12), lift=0.001, thick=0.007,
                          color="candy_orange", mat="vinyl"))
    loc, nrm = KK.on_surface(tree, (x, -0.1, 0.124), (0, 1, -0.2))
    if loc is None:
        loc, nrm = V((x, -0.1, 0.124)), V((0, -1, 0.3))
    nrm = (nrm + V((0, -0.4, 0.1))).normalized()
    c = loc + nrm * 0.008
    M = D.look(nrm, up=(0, 0, 1))
    S = 1.3
    face = KK.blob(name + "_catface", (0, 0, 0), (0.033 * S, 0.024 * S, 0.011), segs=12, rings=5, color="candy_grape", mat="vinyl")
    D.put(face, c, nrm, up=(0, 0, 1))
    parts.append(face)
    for sx in (1, -1):
        ear = K.cylinder(f"{name}_cfear{sx}", 0.012 * S, 0.022 * S, color="candy_grape", mat="vinyl", verts=4, radius_top=0.0)
        ear.data.transform(Matrix.Diagonal((1.0, 0.5, 1.0, 1.0)))
        D.put(ear, c + M @ V((sx * 0.022 * S, 0.018 * S, 0.002)), M @ V((sx * 0.55, 1, 0)), up=nrm)
        parts.append(ear)
        eye = KK.blob(f"{name}_cfeye{sx}", (0, 0, 0), (0.0062 * S, 0.0075 * S, 0.0025), segs=6, rings=3, color="candy_orange",
                      mat="flat")
        D.put(eye, c + M @ V((sx * 0.013 * S, 0.003 * S, 0.0105)), nrm)
        parts.append(eye)
    nose = KK.blob(name + "_cfnose", (0, 0, 0), (0.0045 * S, 0.0035 * S, 0.0022), segs=6, rings=3, color=PINK, mat="flat")
    D.put(nose, c + M @ V((0, -0.008 * S, 0.011)), nrm)
    parts.append(nose)
    return parts


# ------------------------------------------------------------------ arms and paws
def fluffy_cuff(name, pts, r):
    """A fat fluffy cuff where the sleeve meets the paw, with a few tufts."""
    e, b = V(pts[-1]), V(pts[-2])
    d = (e - b).normalized()
    u = d.orthogonal().normalized()
    w = d.cross(u)
    cen = e - d * 0.006
    cp = [cen + (u * math.cos(a) + w * math.sin(a)) * r for a in (2 * math.pi * i / 18 for i in range(18))]
    out = [D.sweep(name + "_cuff", cp, 0.021, sides=8, color=BLK, mat=PLUSH, closed=True, center=cen,
                   warp=lambda a, i: 1.0 + 0.14 * math.cos(i * 2.4) + 0.05 * math.cos(3 * a))]
    return out


PAD_VIEW = {"hand_l": V((0.55, -1.0, 0.2)), "hand_r": V((-0.55, -1.0, 0.2))}


def paw_pads(name):
    """Five pink pads on the paw mitt: a toe bean on each of the four fingers, where the finger faces the
    viewer, and a big palm pad on the heel of the palm."""
    view = PAD_VIEW[name].normalized()

    def extras(F):
        G, a, s, t, fr, R0 = F["grip"], F["a"], F["s"], F["t"], F["fr"], F["R0"]
        fw = F["fw"]
        objs = []
        arcs = (200, 212, 204, 186)
        rads = (1.0, 1.04, 1.0, 0.88)
        for i in range(4):
            off = a * ((1.5 - i) * fw)
            best, th_b = -2.0, 0.0
            for k in range(41):
                th = math.radians(-32 + (arcs[i] + 32) * (0.15 + 0.7 * k / 40))
                dv = s * math.cos(th) + t * math.sin(th)
                if dv.dot(view) > best:
                    best, th_b = dv.dot(view), th
            dv = s * math.cos(th_b) + t * math.sin(th_b)
            p = G + off + dv * (R0 + fr * rads[i] * 0.86)
            bean = KK.blob(f"{name}_bean{i}", (0, 0, 0), (0.0122, 0.0136, 0.0058), segs=10, rings=4, color=PINK, mat="flat")
            D.put(bean, p, dv, up=a)
            objs.append(bean)
        pc = F["palm"]
        M = D.look(a, up=s)
        R = (0.029 * 1.4, 0.021 * 1.4, 0.041 * 1.4)
        loc, nrm = on_ell(pc, M, R, M.inverted() @ (view + s * 0.3))
        pad = KK.blob(f"{name}_pad", (0, 0, 0), (0.023, 0.019, 0.0062), segs=12, rings=4, color=PINK, mat="flat")
        D.put(pad, loc - nrm * 0.0015, nrm, up=a)
        objs.append(pad)
        return objs
    return extras


# ------------------------------------------------------------------ tail (ONE mesh, origin at the tail base)
TAIL_CTRL = [(0, 0.10, 0.36), (0, 0.2, 0.31), (-0.015, 0.33, 0.285), (-0.035, 0.45, 0.33), (-0.05, 0.53, 0.44),
             (-0.055, 0.55, 0.57), (-0.045, 0.52, 0.68), (-0.03, 0.52, 0.78), (-0.02, 0.57, 0.86), (-0.015, 0.64, 0.9)]
TAIL_RAD = [0.06, 0.066, 0.064, 0.06, 0.056, 0.053, 0.05, 0.049, 0.054, 0.056]


def tail():
    """A thick plush tail in an undulating S: out of the back low, a swoop down and back, rising, the
    top bending forward and the fluffy tip easing back out again (never a C, never doubling back toward
    the root: the rig bends it by distance from the root). 55 rings along it."""
    pts, par = KK.catmull(TAIL_CTRL, per=6)
    rr = [KK.lerp_list(TAIL_RAD, u) for u in par]
    d = (pts[-1] - pts[-2]).normalized()
    pts += [pts[-1] + d * rr[-1] * 0.45, pts[-1] + d * rr[-1] * 0.8, pts[-1] + d * rr[-1] * 0.98]
    rr += [rr[-1] * 0.86, rr[-1] * 0.55, 0.0]
    n = len(pts)

    def fur(a, i):
        k = smooth01((i - (n - 12)) / 8.0)         # the tip gets shaggy
        return 1.0 + 0.045 * math.cos(4 * a + i * 1.7) + k * (0.1 * math.cos(7 * a + i * 2.3) + 0.05 * math.cos(11 * a))
    parts = [D.sweep("tail_tube", pts, rr, sides=14, color=BLK, mat=PLUSH, project="sphere", up=(0, 0, 1), warp=fur)]
    # the fluffy tip: tufts round the end, combed along the tail
    for k in range(4):
        a = 2 * math.pi * (k + 0.5) / 4
        j = n - 4 - (k % 2) * 3
        tan = (pts[j + 1] - pts[j - 1]).normalized()
        u = tan.orthogonal().normalized()
        w = tan.cross(u)
        rad = u * math.cos(a) + w * math.sin(a)
        parts.append(tuft(f"ttip{k}", pts[j] + rad * rr[j] * 0.7, rad * 0.6 + tan, tan, 0.05, n=2))
    parts.append(tuft("ttip_end", pts[-3], d, d + V((0, 0, 0.3)), 0.05, n=2))
    # the little purple bow on the outside of the rise (v1's, reads from behind)
    j = int(len(par) * 0.5)
    tan = (pts[j + 1] - pts[j - 1]).normalized()
    nrm = (V((0, 1, 0)) - tan * V((0, 1, 0)).dot(tan)).normalized()
    parts.append(bow("tail_bow", pts[j] + nrm * (rr[j] + 0.004), nrm, tan, "candy_grape", s=1.05))
    return D.part(parts, "tail", KK.TAIL_BASE)


# ================================================================== the costume
def build():
    """Returns (parts, children) for KK.finish2()."""
    KK.AO_SHAPE.clear()
    KK.AO_IGNORE.clear()
    KK.POST_ATTACH[:] = []
    parts = {"body": body()}
    parts["head"], children = head()
    for s, hip in (("l", KK.HIP_L), ("r", KK.HIP_R)):
        parts["leg_" + s], parts["shin_" + s] = KK.leg2("leg_" + s, hip, BLK, lambda x, s=s: shoe("leg_" + s, x))
    for s, sho, elb, wri in (("l", KK.SHO_L, KK.ELBOW_L, KK.WRIST_L), ("r", KK.SHO_R, KK.ELBOW_R, KK.WRIST_R)):
        parts["arm_" + s], parts["forearm_" + s] = KK.arm2("arm_" + s, sho, elb, wri, BLK, mat=PLUSH,
                                                           fore_extras=lambda p, s=s: fluffy_cuff("arm_" + s, p, 0.05))
    parts["hand_l"] = KK.grip_hand("hand_l", KK.WRIST_L, KK.WAND_GRIP, KK.WAND_DIR, (0.55, 0.83, 0.0), 0.0155, fat=1.4,
                                   color=BLK, mat=PLUSH, extras=paw_pads("hand_l"))
    parts["hand_r"] = KK.grip_hand("hand_r", KK.WRIST_R, KK.BUCKET_GRIP, (1, 0, 0), (0.0, -0.45, 0.9), 0.012, fat=1.4,
                                   color=BLK, mat=PLUSH, extras=paw_pads("hand_r"))
    parts["tail"] = tail()
    bob_bow = bow_tie()
    children += [("bob_bow", bob_bow, "body"), ("bob_bell", bell(), "bob_bow")]
    # the hood was part of the head mesh: whatever ignored the head in the AO bake ignores it too (unchanged AO)
    for nm in ("arm_l", "arm_r", "soft_ear_l", "soft_ear_r"):
        KK.AO_IGNORE[nm] = ["hood"]
    return parts, children
