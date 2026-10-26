"""kid_pumpkin : one costume builder (see build_costumes.py for the run command and the rig contract).

Follows the costume reference (ref/ref_pumpkin.png): a big glossy, deeply ribbed pumpkin suit with a
carved jack-o'-lantern face, a collar of big veined leaves around the neck, short puffy orange sleeves
over bare arms, green-and-black striped stockings, chunky orange high-tops, and a hat that is the top
of a pumpkin (ribbed orange cap with scalloped lobes over the brown hair, a thick curling green stem,
two leaves and curly tendrils). No wings (the game uses a magic-dust effect instead).
"""
import math

import bmesh
import bpy

import kid_kit as KK
from kid_kit import D, K, V, FELT  # noqa: F401
from costume_common import panel

P, PD, PL = "pumpkin", "pumpkin_dark", "pumpkin_light"
LEAF, LEAF2, LEAF_IN, VEIN = "lilypad", "banner_green", "lawn_dark", "apple_green"
STEM = "apple_green"
TEND = "gourd_green"
SOCK, SOCK_DARK = "lilypad", "licorice_black"
GLOSS = "vinyl"


# ================================================================== helpers (pumpkin-only)
def lobe(a, a0, n):
    """Position across a pumpkin lobe: u in -1..1 (0 = lobe centre, +-1 = crease) and the round
    'semicircle' height of the lobe there (1 at the centre, 0 in the crease)."""
    t = (a - a0) * n / (2 * math.pi)
    u = 2 * (t - round(t))
    return u, math.sqrt(max(0.0, 1 - u * u))


def curl(name, start, d0, up, length, turns, r0, n=14, tight=1.7, drift=0.15, color=TEND):
    """A curly vine tendril: starts along d0 and winds up into a tightening spiral in the (d0, up) plane."""
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
    return D.sweep(name, pts, radii, sides=4, color=color, mat="flat", project="box")


def hug_leaf(name, surf, a, th0, span, W, hr, lift=0.004, tip_lift=0.02, cup=0.008, th=0.007, nu=6, nv=4,
             color=LEAF, under=LEAF_IN, veins=VEIN, pairs=2):
    """A pointed oval leaf lying on a round surface. surf(direction) -> (point, normal) on the surface
    seen from its centre; the leaf runs down the meridian at azimuth a from polar angle th0 over 'span'
    (radians), W = half width (m), hr ~ horizontal radius of the surface (to turn widths into angles).
    Tips lift off the surface, the edges cup up. Midrib + side veins on top."""
    def hw(u):
        # (a hair of width at the stem and the tip keeps the end rows from collapsing into zero-area faces)
        return max(W * math.sin(math.pi * min(max(u, 0.0), 1.0) ** 0.75) ** 0.85, 0.002)

    def at(u, v, extra=0.0):
        tht = th0 + span * u
        s = max(math.sin(tht), 0.08)
        da = (2 * v - 1) * hw(u) / (hr * s)
        d = V((s * math.cos(a + da), s * math.sin(a + da), math.cos(tht)))
        p, n = surf(d)
        off = lift + tip_lift * u * u + cup * (2 * v - 1) ** 2 * hw(u) / W + extra
        return p + n * off, n

    lf = panel(name, lambda u, v: at(u, v)[0], nu, nv, th, color, under, out_mat=GLOSS, in_mat=GLOSS,
               out_hint=lambda u, v: at(u, v)[1])
    parts = [lf]
    top = th / 2 + 0.0015
    rib = [at(u, 0.5, top)[0] for u in (0.04, 0.3, 0.6, 0.86)]
    parts.append(D.sweep(name + "_rib", rib, [0.0042, 0.0036, 0.0026, 0.0], sides=4, color=veins, mat="flat",
                         project="box"))
    for k in range(pairs):
        u0 = 0.22 + 0.26 * k
        for sv in (0.12, 0.88):
            vp = [at(u0, 0.5, top)[0], at(u0 + 0.1, 0.5 + (sv - 0.5) * 0.55, top)[0], at(u0 + 0.17, sv, top)[0]]
            parts.append(D.sweep(f"{name}_v{k}{sv}", vp, [0.0028, 0.002, 0.0], sides=4, color=veins, mat="flat",
                                 project="box"))
    return K.merge(parts, name)


def carve(ob, tree, shapes, depth=0.014, floor="ink", wall="choco_dark"):
    """Carve jack-o'-lantern holes into the closed body mesh: each shape is (outline [(u, v)], centre),
    seen along +Y; the cutter hugs the surface (top 3 cm out, floor 'depth' in along the normal), so
    the pocket has an even depth. Pocket floors -> floor colour, pocket walls -> wall colour."""
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
    bm.to_mesh(ob.data)
    bm.free()
    return ob


# ================================================================== head: pumpkin-top hat
CAP_NL = 10


def cap_rim(a, tip):
    """Height of the cap's lower edge at azimuth a (tip = lobe height there, 1 at a lobe tip): high
    over the forehead (the bangs hang out from under it), down past the ears on the sides and low over
    the back of the head, so the hair cap is fully enclosed."""
    fr = max(0.0, -math.sin(a)) ** 2.2
    bk = max(0.0, math.sin(a))
    return 0.745 + 0.235 * fr - 0.035 * bk - 0.034 * tip + 0.004


def pumpkin_cap(C, R, ryf, NL=CAP_NL, SPL=6, NR=8, rib=0.085, th=0.013):
    """The top of a pumpkin worn as a hat: a ribbed orange shell (NL lobes, one centred on the front)
    whose lower edge is scalloped into rounded lobe tips, high over the forehead and low over the back
    and sides, with a thick turned lip and a darker inside."""
    SEG = NL * SPL
    a0 = -math.pi / 2
    rim_z = cap_rim
    bm = bmesh.new()
    pole = bm.verts.new(C + V((0, 0, R.z)))
    cols = []
    for j in range(SEG):
        a = a0 + 2 * math.pi * j / SEG
        u, tip = lobe(a, a0, NL)
        m = 1 + rib * (tip ** 0.6 - 0.75)
        cphi = max(-0.95, min(0.95, (rim_z(a, tip) - C.z) / R.z))
        phr = math.acos(cphi)
        ry = R.y if math.sin(a) >= 0 else ryf          # the front half bulges forward over the fringe
        col = []
        for k in range(1, NR + 1):
            phi = phr * math.sin(0.5 * math.pi * k / NR)
            p = C + V((math.cos(a) * math.sin(phi) * R.x * m, math.sin(a) * math.sin(phi) * ry * m, math.cos(phi) * R.z))
            col.append(p)
        rimp = col[-1]
        n = V(((rimp.x - C.x) / R.x ** 2, (rimp.y - C.y) / ry ** 2, (rimp.z - C.z) / R.z ** 2)).normalized()
        col.append(rimp - n * th * 0.55 + V((0, 0, -0.004)))      # rolled lip
        col.append(rimp - n * th + V((0, 0, 0.004)))
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
    return D.mesh(bm, "cap", P, "pumpkin", "sphere", smooth=True, center=C,
                  extra=[(lip, P, "pumpkin"), (inner, PD, "pumpkin")])


def build():
    # --- head: the kid's own head + hair, wearing the top of a pumpkin as a hat
    head = KK.bare_head()
    hair = head[0]
    assert hair.name.startswith("hair")
    # the kit's fringe and temple locks sit up on the forehead (they'd lie on the cap): replace them with
    # bangs and locks that start inside the cap and hang out from under its rim
    for o in [o for o in head if o.name.startswith(("fringe", "lock"))]:
        head.remove(o)
        bpy.data.objects.remove(o, do_unlink=True)
    head += KK.face(fang=False)
    CC, CR, RYF = V((0.0, 0.018, 0.846)), V((0.262, 0.262, 0.238)), 0.286
    cap = pumpkin_cap(CC, CR, RYF)
    head.append(cap)
    top = CC + V((0, 0, CR.z - 0.014))

    def rim_at(p):
        a = math.atan2(p.y - CC.y, p.x)
        return cap_rim(a, lobe(a, -math.pi / 2, CAP_NL)[1])

    bm = bmesh.new()                 # the cap encloses the hair: drop every hair face that reaches near the rim
    bm.from_mesh(hair.data)
    bmesh.ops.delete(bm, geom=[f for f in bm.faces if any((v.co + hair.location).z < rim_at(v.co + hair.location) + 0.03
                                                          for v in f.verts)], context="FACES")
    bm.to_mesh(hair.data)
    bm.free()

    def skull(x, z, lift=0.0, ell=(KK.FC, KK.FR)):
        p, n = KK.ellipsoid_pt(ell[0], ell[1], x, z)
        return p + n * lift
    hairs = []
    HC_ = (KK.HAIR_C, KK.HAIR_R)
    # bangs: flat swooping tufts tucked behind the rim's lip, hanging over the upper forehead
    for i, (x0, dx, drop, w) in enumerate(((-0.125, -0.026, 0.05, 0.024), (-0.088, -0.03, 0.066, 0.031),
                                           (-0.044, -0.03, 0.074, 0.033), (0.0, -0.026, 0.078, 0.034),
                                           (0.044, 0.022, 0.074, 0.033), (0.088, 0.028, 0.066, 0.031),
                                           (0.125, 0.026, 0.05, 0.024))):
        zr = rim_at(skull(x0, 0.93))
        ctrl = [skull(x0 * 0.9, zr + 0.03, 0.0, HC_), skull(x0 + dx * 0.25, zr - 0.008, 0.009),
                skull(x0 + dx * 0.7, zr - drop * 0.6, 0.014), skull(x0 + dx, zr - drop, 0.007)]
        pts, par = KK.catmull(ctrl, per=3)
        rr = [KK.lerp_list([w * 0.75, w, w * 0.92, w * 0.55, 0.0], t) for t in par]
        hairs.append(D.sweep(f"bang{i}", pts, rr, sides=5, color="hair_brown", mat="flat", project="box", up=(0, -1, 0.3),
                             warp=lambda a, k: 1.0 / math.sqrt((math.cos(a) / 0.42) ** 2 + math.sin(a) ** 2)))
    # temple locks hanging from under the side lobes, curling toward the cheeks
    for sx in (1, -1):
        p0 = skull(0.17 * sx, 0.86, 0.0, HC_)
        zr = rim_at(p0)
        ctrl = [skull(0.165 * sx, zr + 0.03, 0.0, HC_), skull(0.178 * sx, zr - 0.012, 0.012),
                skull(0.18 * sx, zr - 0.05, 0.012), skull(0.165 * sx, zr - 0.08, 0.007)]
        pts, par = KK.catmull(ctrl, per=3)
        rr = [KK.lerp_list([0.02, 0.027, 0.022, 0.0], t) for t in par]
        hairs.append(D.sweep(f"tlock{sx}", pts, rr, sides=5, color="hair_brown", mat="flat", project="box",
                             up=(sx, -0.6, 0),
                             warp=lambda a, k: 1.0 / math.sqrt((math.cos(a) / 0.45) ** 2 + math.sin(a) ** 2)))
    # guard: no hair vertex above the rim may sit outside the cap shell (a ray from the cap centre to it
    # must not cross the cap); below the rim the bangs and locks hang out from under the lip on purpose
    bpy.context.view_layer.update()
    ctree = D.bvh(cap)
    out = 0
    for o in [hair] + hairs:
        for v in o.data.vertices:
            p = o.matrix_world @ v.co
            d = p - CC
            if p.z > rim_at(p):
                loc, _ = D.hit(ctree, CC, d, d.length)
                out += loc is not None
    assert out == 0, f"{out} hair vertices outside the pumpkin cap"
    head += hairs

    def cap_surf(d):
        p, n = KK.ellipsoid_dir(CC, V((CR.x, RYF if d[1] < 0 else CR.y, CR.z)), d)
        return p + n * 0.006, n
    # thick curling green stem with a ribbed flare at the base
    ctrl = [top + V((0, 0, -0.01)), top + V((0.0, 0.0, 0.05)), top + V((0.006, 0.004, 0.094)), top + V((0.036, 0.006, 0.124)),
            top + V((0.07, 0.004, 0.112)), top + V((0.08, 0.0, 0.082))]
    pts, par = KK.catmull(ctrl, per=3)
    rr = [KK.lerp_list([0.036, 0.03, 0.026, 0.022, 0.018, 0.0], t) for t in par]
    rr[-2] = max(rr[-2], 0.012)
    head.append(D.sweep("stem", pts, rr, sides=8, color=STEM, mat=GLOSS, project="cyl",
                        warp=lambda a, i: 1.0 + 0.1 * math.cos(5 * a)))
    flare = D.lathe("stem_base", [(0.0, -0.012), (0.06, -0.012), (0.055, 0.0), (0.04, 0.012), (0.032, 0.03), (0.0, 0.032)], 15,
                    color=STEM, mat=GLOSS, project="cyl", loc=top + V((0, 0, 0.004)),
                    warp=lambda c: V((c.x, c.y, c.z)) * (1.0 + 0.16 * math.cos(5 * math.atan2(c.y, c.x))))
    head.append(flare)
    # two leaves lying on the cap, near the stem
    for i, (a, th0, span, W) in enumerate(((math.pi + 0.55, 0.12, 0.62, 0.05), (-0.5, 0.16, 0.66, 0.055))):
        head.append(hug_leaf(f"cap_leaf{i}", cap_surf, a, th0, span, W, CR.x, lift=0.006, tip_lift=0.03, cup=0.01))
    # curly vine tendrils springing from the stem base
    for i, (d0, up, L, tn, r0) in enumerate((((-1, 0.25, 0.7), (0, 0, 1), 0.2, 1.25, 0.0062),
                                             ((1, 0.5, 0.45), (0, 0, 1), 0.17, 1.15, 0.0058))):
        start = top + V((d0[0] * 0.03, d0[1] * 0.03, 0.012))
        head.append(curl(f"tendril{i}", start, d0, up, L, tn, r0))
    head = D.part(head, "head", KK.NECK)
    KK.tilt_head(head)

    # --- body: a big round, deeply ribbed pumpkin (narrow enough in X that both hands stay outside it)
    RX, RY, RZ, CZ = 0.222, 0.258, 0.232, 0.445
    NL, SEG = 10, 60
    A0 = -math.pi / 2
    pprof = [(0.0, -1.0)]
    for deg in range(12, 180, 12):
        phi = math.radians(deg)
        pprof.append((math.sin(phi), -math.cos(phi)))
    pprof.append((0.0, 1.0))

    def pwarp(c):
        a = math.atan2(c.y, c.x)
        r = math.hypot(c.x, c.y)
        u, tip = lobe(a, A0, NL)
        rib = 1.0 - 0.075 + 0.075 * tip ** 0.55
        zz = c.z
        if abs(zz) > 0.78:     # dimpled top and bottom
            zz -= math.copysign(0.2 * (1 - r) * (abs(zz) - 0.78) / 0.22, zz)
        return V((r * rib * math.cos(a) * RX, r * rib * math.sin(a) * RY + 0.01, CZ + zz * RZ))
    pump = D.lathe("pumpkin_body", pprof, SEG, color=P, mat="pumpkin", project="cyl", warp=pwarp, center=(0, 0, CZ),
                   phase=A0)
    tree = D.bvh(pump)
    PC = V((0, 0.01, CZ))

    def body_surf(d):
        loc, nrm = D.hit(tree, PC + V(d) * 0.6, -V(d), 1.2)
        if loc is None:
            return PC + V(d) * 0.2, V(d)
        return loc, nrm

    # carved jack-o'-lantern face: big triangle eyes, triangle nose, wide toothy grin
    eye = KK.rounded_poly([(-0.04, -0.026), (0.036, -0.026), (0.008, 0.034)], 0.006, 2)
    nose = KK.rounded_poly([(-0.019, -0.013), (0.019, -0.013), (0.0, 0.017)], 0.004, 2)
    Wg = 0.122

    def zb(x):
        u = x / Wg
        return -0.052 * (1 - u * u) + 0.024 * u * u

    def zt(x):
        u = x / Wg
        return -0.008 * (1 - u * u) + 0.03 * u * u
    def edge(xs, zf, teeth, drop):
        """Grin edge through xs (in order) with square teeth (x0, x1) cut in, 'drop' toward the inside."""
        keys = [(x, None) for x in xs if not any(min(t) < x < max(t) for t in teeth)] + [(t[0], t) for t in teeth]
        keys.sort(key=lambda k: k[0], reverse=xs[0] > xs[-1])
        out = []
        for x, t in keys:
            if t is None:
                out.append((x, zf(x)))
            else:
                out += [(t[0], zf(t[0])), (t[0], zf(t[0]) + drop), (t[1], zf(t[1]) + drop), (t[1], zf(t[1]))]
        return out
    xs = [-Wg + 2 * Wg * i / 16 for i in range(17)]
    grin = edge(xs, zb, [(-0.013, 0.013)], 0.02) + edge(list(reversed(xs[1:-1])), zt, [(0.066, 0.033), (-0.033, -0.066)], -0.021)
    shapes = [(eye, (0.072, 0, 0.515)), ([(-x, z) for x, z in reversed(eye)], (-0.072, 0, 0.515)),
              (nose, (0.0, 0, 0.458)), (grin, (0.0, 0, 0.408))]
    carve(pump, tree, shapes)
    body = [pump]
    # collar of big veined leaves around the neck, flopping over the top of the pumpkin
    for i in range(6):
        a = A0 + 2 * math.pi * i / 6
        body.append(hug_leaf(f"collar{i}", body_surf, a, math.radians(14), math.radians(50), 0.074, RX,
                             lift=0.006, tip_lift=0.05, cup=0.014, color=LEAF if i % 2 == 0 else LEAF2))
    body.append(D.lathe("neck", [(0.0, 0.6), (0.12, 0.6), (0.115, 0.66), (0.0, 0.68)], 12, color=LEAF_IN, mat=FELT,
                        project="cyl"))
    # curly vines winding out from under the collar at the back of each side
    for sx in (1, -1):
        st, _ = body_surf(V((0.62 * sx, 0.45, 0.65)))
        body.append(curl(f"vine{sx}", st + V((0, 0, 0.004)), (0.55 * sx, 0.35, -1), (sx, 0.3, 0.2), 0.3, 1.3, 0.0075,
                         n=16, tight=2.0, drift=0.05))
    body = D.part(body, "body", (0, 0, KK.HIP_Z))

    # --- arms: short puffy orange pumpkin sleeves over the kid's bare arms
    def puff(pts, rr, d):
        s = pts[0]
        sx = 1 if s.x > 0 else -1
        c = s + V((0.045 * sx, 0, -0.012)) + d * 0.035 + V((0.018 * sx, 0, 0.0))    # on the shoulder, out of the pumpkin
        pf = KK.blob("puff", (0, 0, 0), (0.074, 0.074, 0.07), segs=12, rings=7, color=P, mat="pumpkin",
                     warp=lambda q: V((q.x, q.y, q.z)) * (1.0 + 0.07 * math.cos(6 * math.atan2(q.y, q.x)) *
                                                          max(0.0, 1 - abs(q.z) / 0.07)))
        D.put(pf, c, d, up=(0, 0, 1))
        hem = []
        for k in range(16):
            t = 2 * math.pi * k / 16
            hem.append(V((math.cos(t) * 0.058, math.sin(t) * 0.058, 0.05)))
        band = D.sweep("puff_hem", hem, 0.009, sides=4, color=PD, mat="pumpkin", closed=True, center=(0, 0, 0.05))
        D.put(band, c, d, up=(0, 0, 1))
        return [pf, band]

    def sleeve_paint(i, j):
        return (P, "pumpkin") if i < 2 else None
    arm_l = KK.arm("arm_l", KK.SHO_L, KK.HAND_L, "skin", mat="flat", r0=0.054, taper=0.008, flare=0.0,
                   paint=sleeve_paint, extras=puff)
    arm_r = KK.arm("arm_r", KK.SHO_R, KK.HAND_R, "skin", mat="flat", r0=0.054, taper=0.008, flare=0.0,
                   paint=sleeve_paint, extras=puff)

    # --- legs: green-and-black striped stockings, chunky orange high-tops with white soles, green accents
    tprof = [(0.074, 0.125), (0.08, 0.17), (0.084, 0.24), (0.074, 0.305)]

    def thigh_r(z):
        for (r0, z0), (r1, z1) in zip(tprof[:-1], tprof[1:]):
            if z0 <= z <= z1:
                return r0 + (r1 - r0) * (z - z0) / (z1 - z0)
        return tprof[0][0] if z < tprof[0][1] else tprof[-1][0]

    def stocking(x):
        zs = [0.13 + 0.0175 * k for k in range(11)]
        prof = [(thigh_r(z) + 0.003, z) for z in zs]
        st = D.lathe("stocking", prof, 10, color=SOCK, mat=FELT, project="cyl", loc=(x, 0, 0))
        KK.paint_grid(st, 10, lambda band, seg: (SOCK_DARK if band % 2 == 1 else None) if band < len(zs) - 1 else None,
                      mat=FELT)
        # green velcro strap across the instep, hugging the sneaker upper built just before (leg() calls
        # extras after sneaker()), with a little square tab on the outer side
        up_ob = bpy.data.objects.get(("leg_l" if x > 0 else "leg_r") + "_upper")
        parts = [st]
        if up_ob is not None:
            tr = D.bvh(up_ob)
            zs_, pts = 0.137, []
            for k in range(13):
                al = math.radians(-105 + 210 * k / 12)          # around the foot, through the front (-Y)
                d = V((math.sin(al), -math.cos(al), 0.0))
                loc, nrm = D.hit(tr, V((x, -0.02, zs_)) + d * 0.3, -d, 0.6)
                if loc is not None:
                    pts.append(loc + nrm * 0.005)
            if len(pts) > 3:
                parts.append(D.sweep("strap", pts, 0.017, sides=6, color=LEAF, mat=GLOSS, project="box", up=(0, 0, 1),
                                     warp=lambda a, i: 1.0 / math.sqrt(math.cos(a) ** 2 + (math.sin(a) / 0.3) ** 2)))
                out = 1 if x > 0 else -1
                tab_p = pts[0] if out < 0 else pts[-1]
                tab = KK.blob("strap_tab", (0, 0, 0), (0.02, 0.018, 0.006), segs=8, rings=4, color="vine_dark", mat=GLOSS)
                D.put(tab, tab_p + V((0, 0.012, 0)) + V((out * 0.004, 0, 0)), V((out, 0, 0)), up=(0, 0, 1))
                parts.append(tab)
        return parts
    shoe = dict(upper=P, panel=LEAF, toe="lolly_white", sole="lolly_white", stripe=PD, lace=LEAF, collar=PL, tongue=PL,
                heel=LEAF, mat=GLOSS)
    leg_l = KK.leg("leg_l", KK.HIP_L, SOCK, P, "lolly_white", shoe=shoe, extras=stocking)
    leg_r = KK.leg("leg_r", KK.HIP_R, SOCK, P, "lolly_white", shoe=shoe, extras=stocking)

    # no wings: the game plays a magic-dust effect for double jumps and hovering on wingless costumes
    return dict(body=body, head=head, leg_l=leg_l, leg_r=leg_r, arm_l=arm_l, arm_r=arm_r)
