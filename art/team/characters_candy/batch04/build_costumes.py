"""Unlockable player costumes: the same kid as dragon_kid (batch03) in five other Halloween costumes.
Everything shared with the dragon (joints, face, hands, bucket, candy heap, rig assembly) comes from
kid_kit.py, which is the dragon build script parametrised; this file only adds what each costume
changes. Rig contract identical to dragon_kid (node names, parents, origins, front -Y / glTF +Z).

  kid_ghost     bedsheet ghost: domed sheet hood with eye holes, the kid's face peeking out of its
                round 'boo' mouth, a long sheet with a scalloped hem; wing_l/wing_r = loose sheet
                flaps behind the arms
  (only the cat has a tail: costumes without one get a wand spin in game)
  kid_cat       blue-black cat onesie: pointed ears with pink insides, green cat eyes on the hood,
                grey belly patch, orange collar with a gold bell, paw boots; tail = long curling
                cat tail with an orange bow
  kid_witch     purple witch dress with a green-trimmed pointy hem, striped stockings, pointy curled
                boots, big floppy pointed hat with a green band and gold buckle, pigtails;
                wing_l/wing_r = the two halves of a dark cape with an orange lining
  kid_skeleton  black onesie with a puffy bone print (ribs, sternum, spine, shoulder blades, pelvis,
                arm and leg bones, toe bones) and a skull hood with eye sockets and a toothy mouth
                framing the kid's face
  kid_pumpkin   round ribbed pumpkin suit with a stitched jack-o'-lantern face, a leafy green collar,
                green sleeves and tights, brown boots, and a green beanie with a curly stem, two
                leaves and a tendril; wing_l/wing_r = two big pumpkin leaves on the back

Run: blender --background --factory-startup --python art/team/characters_candy/batch04/build_costumes.py
     [-- kid_ghost kid_cat ...] [--nosheet] [--pieces]
"""
import json
import math
import sys
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parent))
import bmesh  # noqa: E402
from mathutils import Matrix  # noqa: E402

import kid_kit as KK  # noqa: E402
from kid_kit import D, K, V, FELT  # noqa: E402

ALL = ["kid_ghost", "kid_cat", "kid_witch", "kid_skeleton", "kid_pumpkin"]


# ================================================================== small shared builders
def tube(name, ctrl, radii, per=2, sides=8, color="ink", mat=FELT, project="sphere", up=None, paint=None):
    pts, par = KK.catmull(ctrl, per=per)
    rr = [KK.lerp_list(radii, t) for t in par]
    return D.sweep(name, pts, rr, sides=sides, color=color, mat=mat, project=project, up=up, paint=paint), pts, rr


def panel(name, P, nu, nv, th, out_color, in_color, out_mat=FELT, in_mat=FELT, out_hint=(0, 1, 0), out_paint=None):
    """Thick cloth panel from a surface P(u, v) -> point (u, v in 0..1). The outer side faces roughly
    along out_hint (per-vertex hint function allowed), the inner side gets in_color. out_paint(i, j) ->
    colour or None recolours outer faces (trims)."""
    grid = [[V(P(i / nu, j / nv)) for j in range(nv + 1)] for i in range(nu + 1)]

    def normal(i, j):
        a = grid[min(i + 1, nu)][j] - grid[max(i - 1, 0)][j]
        b = grid[i][min(j + 1, nv)] - grid[i][max(j - 1, 0)]
        n = a.cross(b).normalized()
        h = V(out_hint(i / nu, j / nv) if callable(out_hint) else out_hint)
        return n if n.dot(h) >= 0 else -n
    bm = bmesh.new()
    top = [[bm.verts.new(grid[i][j] + normal(i, j) * th / 2) for j in range(nv + 1)] for i in range(nu + 1)]
    bot = [[bm.verts.new(grid[i][j] - normal(i, j) * th / 2) for j in range(nv + 1)] for i in range(nu + 1)]
    outer, inner = [], []
    for i in range(nu):
        for j in range(nv):
            outer.append(bm.faces.new((top[i][j], top[i + 1][j], top[i + 1][j + 1], top[i][j + 1])))
            inner.append(bm.faces.new((bot[i][j], bot[i][j + 1], bot[i + 1][j + 1], bot[i + 1][j])))
    border = [(i, 0) for i in range(nu)] + [(nu, j) for j in range(nv)] + \
             [(i, nv) for i in range(nu, 0, -1)] + [(0, j) for j in range(nv, 0, -1)]
    for k in range(len(border)):
        i0, j0 = border[k]
        i1, j1 = border[(k + 1) % len(border)]
        bm.faces.new((top[i0][j0], bot[i0][j0], bot[i1][j1], top[i1][j1]))
    bmesh.ops.recalc_face_normals(bm, faces=bm.faces)
    # recalc may flip the whole shell; make the outer faces face along the hint
    bm.normal_update()
    i_m, j_m = nu // 2, nv // 2
    f = outer[(i_m) * nv + j_m]
    h = V(out_hint(0.5, 0.5) if callable(out_hint) else out_hint)
    if f.normal.dot(h) < 0:
        bmesh.ops.reverse_faces(bm, faces=bm.faces)
    rest = [f for f in bm.faces if f not in outer and f not in inner]
    extra = [(inner, in_color, in_mat), (rest, in_color, in_mat)]
    if out_paint:
        groups = {}
        for i in range(nu):
            for j in range(nv):
                c = out_paint(i, j)
                if c:
                    groups.setdefault(c, []).append(outer[i * nv + j])
        extra += [(fs, c, out_mat) for c, fs in groups.items()]
    return D.mesh(bm, name, out_color, out_mat, "box", smooth=True, extra=extra)


def stitch_line(name, pts_nrm, color, length=0.017, width=0.006):
    items = []
    for k in range(len(pts_nrm) - 1):
        p, n = pts_nrm[k]
        q, _ = pts_nrm[k + 1]
        items.append(((p + q) / 2, q - p, n))
    return D.stitches(name, items, length=length, width=width, height=0.003, color=color)


def bone(name, a, b, nrm, r=0.011, color="bone", flat=0.6, knob=1.45):
    """A cartoon bone (shaft + two knobs at each end) lying on a surface between a and b."""
    a, b = V(a), V(b)
    L = (b - a).length
    shaft = K.cylinder(name + "_s", r, L, color=color, mat="flat", verts=6, rot=(90, 0, 0))
    ps = [shaft]
    for sy in (1, -1):
        for sx in (1, -1):
            ps.append(K.sphere(f"{name}_k{sy}{sx}", r * knob, color=color, mat="flat", segments=6, rings=4,
                               loc=(sx * r * 1.05, sy * L / 2, 0)))
    ob = K.merge(ps, name)
    ob.data.transform(Matrix.Diagonal((1, 1, flat, 1)))
    D.put(ob, (a + b) / 2 + V(nrm).normalized() * r * flat * 0.5, nrm, up=(b - a))
    return ob


def bow(name, c, nrm, up, color, s=1.0):
    """Little ribbon bow: two lobes and a knot (local Z along nrm)."""
    ps = []
    for sx in (1, -1):
        lobe = KK.blob(f"{name}_l{sx}", (0, 0, 0), (0.034 * s, 0.024 * s, 0.014 * s), segs=8, rings=5, color=color, mat=FELT)
        lobe.location = (sx * 0.03 * s, 0, 0)
        lobe.rotation_euler = (0, 0, math.radians(-12 * sx))
        ps.append(lobe)
        tail = KK.blob(f"{name}_t{sx}", (0, 0, 0), (0.01 * s, 0.026 * s, 0.008 * s), segs=6, rings=4, color=color, mat=FELT)
        tail.location = (sx * 0.013 * s, -0.03 * s, -0.004 * s)
        tail.rotation_euler = (0, 0, math.radians(20 * sx))
        ps.append(tail)
    ps.append(KK.blob(f"{name}_k", (0, 0, 0), (0.014 * s, 0.014 * s, 0.013 * s), segs=7, rings=4, color=color, mat=FELT))
    ob = K.merge(ps, name)
    D.put(ob, c, nrm, up=up)
    return ob


def hood_eyes(kind, color, pupil="ink", d=(0.34, -0.52, 0.8), size=1.0, roll=12, at=None, tree=None, face_fwd=0.0):
    """Eyes on the hood brow like the dragon's. kind: 'cat' (almond, slit pupil), 'hole' (ink oval).
    at=(x, z) + tree (BVH of the hood shell) casts them onto the real (possibly warped) hood front;
    face_fwd tilts them toward the front so they read from the side/front cameras."""
    parts = []
    for sx in (1, -1):
        if at:
            p, n = KK.on_surface(tree, (at[0] * sx, -0.6, at[1]), (0, 1, 0))
        else:
            p, n = KK.ellipsoid_dir(KK.HC, KK.HR, (d[0] * sx, d[1], d[2]))
        ns = n
        n = (n + V((0, -face_fwd, 0))).normalized()
        if kind == "hole":
            W, H = 0.05 * size, 0.064 * size
            e = KK.blob(f"heye{sx}", (0, 0, 0), (W, H, 0.022), segs=12, rings=6, color=color, mat="flat")
            D.put(e, p - ns * 0.012, n, roll=-roll * sx)
            parts.append(e)
            continue
        W, H, Dp = 0.044 * size, 0.032 * size, 0.016
        e = KK.blob(f"heye{sx}", (0, 0, 0), (W, H, Dp), segs=12, rings=6, color=color, mat="flat")
        D.put(e, p - n * 0.006, n, roll=-roll * sx)
        M = D.look(n, roll=-roll * sx)
        pc = p - n * 0.006 + M @ V((0, 0, Dp * 0.95))
        pu = KK.blob(f"hpupil{sx}", (0, 0, 0), (0.0085 * size, 0.026 * size, 0.005), segs=8, rings=4, color=pupil, mat="flat")
        D.put(pu, pc, n, roll=-roll * sx)
        hl = KK.blob(f"hhl{sx}", (0, 0, 0), (0.0075, 0.0075, 0.003), segs=6, rings=3, color="eye_white", mat="flat")
        D.put(hl, pc + M @ V((-0.016 * size, 0.008 * size, -0.002)), n, roll=-roll * sx)
        parts += [e, pu, hl]
    return parts


def dome(k, fwd=0.0, z0=0.97):
    """Hood shell warp: stretch everything above the face opening upward (taller sheet/skull dome) and
    push the front of it forward into a brow that faces the camera (room for big eyes / sockets)."""
    def w(p):
        if p.z > z0:
            front = max(0.0, -(p.y - KK.HC.y) / KK.HR.y)
            dz = p.z - z0
            p = V((p.x, p.y - fwd * front * min(dz, 0.07) * (1.0 - min(dz, 0.098) / 0.2), p.z + dz * k))
        return p
    return w


def back_y(x, z, off):
    """A Y just behind the torso's back at (x, z), offset 'off' (cloth hanging behind the body)."""
    R = max(KK.torso_r(z), 0.17) + off
    if abs(x) >= R:
        return 0.012 + off * 0.5
    return 0.012 + 0.88 * math.sqrt(R * R - x * x) + off * 0.12


# ================================================================== GHOST
def build_ghost():
    W, ST = "ghost_white", "house_lavender"
    SH = "flat"      # a smooth sheet: felt read as fleece, the flannel check as pyjamas
    DOME = dome(0.95, 1.6)

    # --- head: the sheet over the head, tall dome, big eye holes, the face peeks out of the 'boo' mouth
    head = KK.hood(W, rim_color=W, stitch_color=ST, mat=SH, rim_mat=SH, warp=DOME, rim_r=0.034)
    tree = D.bvh(head[0])
    head += KK.face(fang=False, mouth="oh")
    head += KK.fringe()
    head += hood_eyes("hole", "ink", at=(0.064, 1.05), size=1.1, roll=-6, tree=tree, face_fwd=0.4)
    head = D.part(head, "head", KK.NECK)
    KK.tilt_head(head)

    # --- body: the long sheet, a bell from the chin to a scalloped hem near the ankles. The profile runs
    # from the underside disc out to the hem and up the outside (outer faces point out, the inner wall
    # shows from below). The hem is hitched up over the bucket arm so the bucket swings free.
    prof = [(0.0, 0.36), (0.17, 0.27), (0.26, 0.19), (0.282, 0.165), (0.29, 0.172), (0.279, 0.21), (0.252, 0.26),
            (0.226, 0.31), (0.21, 0.36), (0.204, 0.42), (0.2, 0.48), (0.194, 0.54), (0.178, 0.59), (0.15, 0.635),
            (0.1, 0.672), (0.0, 0.69)]
    BUCKET_A = math.atan2(KK.HAND_R.y, KK.HAND_R.x)

    def hitch(a):
        da = math.atan2(math.sin(a - BUCKET_A), math.cos(a - BUCKET_A))
        return 0.13 * math.exp(-(da / 0.75) ** 2) + 0.035 * math.exp(-((a % (2 * math.pi)) / 0.6) ** 2)

    def sheet_warp(c):
        a = math.atan2(c.y, c.x)
        r = math.hypot(c.x, c.y)
        k = min(max((0.33 - c.z) / 0.16, 0.0), 1.0)
        fold = 1.0 + (0.05 * k + 0.01) * math.cos(9 * a + 0.6)       # loose folds, deeper toward the hem
        h = hitch(a) * k
        z = c.z + h - k * 0.022 * math.cos(8 * a)                     # soft scallops
        r = (r - h * 0.55) * fold
        return V((r * math.cos(a), r * math.sin(a) * 0.92 + 0.012, z))
    sheet = D.lathe("sheet", prof, 40, color=W, mat=SH, project="sphere", warp=sheet_warp, center=(0, 0, 0.45))
    tree = D.bvh(sheet)
    # lavender running stitches along the hem
    hem = []
    for i in range(34):
        a = 2 * math.pi * (i + 0.5) / 34
        z = 0.2 + hitch(a) - 0.022 * math.cos(8 * a)
        loc, nrm = KK.on_surface(tree, (math.cos(a) * 0.25, math.sin(a) * 0.23 + 0.012, z), (-math.cos(a), -math.sin(a), 0))
        if loc is not None:
            hem.append(((loc + nrm * 0.002), V((-math.sin(a), math.cos(a), 0)), nrm))
    body = [sheet, D.stitches("hem_st", hem, length=0.022, width=0.0065, height=0.003, color=ST)]
    body = D.part(body, "body", (0, 0, KK.HIP_Z))

    # --- arms: white bell sleeves of the sheet; legs: lavender leggings and purple sneakers
    arm_l = KK.arm("arm_l", KK.SHO_L, KK.HAND_L, W, mat=SH, r0=0.07, flare=0.024)
    arm_r = KK.arm("arm_r", KK.SHO_R, KK.HAND_R, W, mat=SH, r0=0.07, flare=0.024)
    leg_l = KK.leg("leg_l", KK.HIP_L, "house_lavender", "bat_wing", "lolly_white", cuff=("lolly_white", "flat"))
    leg_r = KK.leg("leg_r", KK.HIP_R, "house_lavender", "bat_wing", "lolly_white", cuff=("lolly_white", "flat"))

    # --- wings: loose sheet flaps hanging behind the arms (the game spreads them like ghost arms)
    def flap(name, root, sx):
        def P(u, v):
            x = 0.03 + u * (0.16 + 0.14 * v)              # widens toward the bottom
            z = 0.625 - v * (0.3 + 0.03 * u) - 0.012 * math.sin(u * math.pi * 2.0) * v
            if v > 0.999:                                  # scalloped bottom edge: soft points
                z -= 0.035 * abs(math.sin(u * math.pi * 1.5))
            y = max(back_y(x, z, 0.04 + 0.03 * v), 0.105)
            return (x * sx, y, z)
        ob = panel(name + "_m", P, 8, 6, 0.018, W, "house_lavender", out_mat=SH, in_mat=SH,
                   out_hint=lambda u, v: (0.4 * sx * u, 1.0, 0.0))
        return D.part([ob], name, root)
    wing_l = flap("wing_l", KK.WING_L, 1)
    wing_r = flap("wing_r", KK.WING_R, -1)
    return dict(body=body, head=head, wing_l=wing_l, wing_r=wing_r, leg_l=leg_l, leg_r=leg_r, arm_l=arm_l, arm_r=arm_r)


# ================================================================== CAT
def build_cat():
    C, CB, ST, PINK = "crow_blue", "stone_dark", "stone", "cheek_pink"

    head = KK.hood(C, rim_color=C, stitch_color=ST)
    head += KK.face(fang=True)
    head += KK.fringe()
    head += hood_eyes("cat", "dragon_spike", d=(0.34, -0.55, 0.78), size=1.22, roll=14)
    # pointed ears with pink insides
    for sx in (1, -1):
        p, n = KK.ellipsoid_dir(KK.HC, KK.HR, (0.52 * sx, 0.02, 0.85))
        h, r = 0.17, 0.085
        prof = [(r, -0.03), (r, 0.0), (r * 0.86, h * 0.3), (r * 0.62, h * 0.58), (r * 0.34, h * 0.82), (r * 0.12, h * 0.96), (0.0, h)]
        ear = D.lathe(f"ear{sx}", prof, 10, color=C, mat=FELT, project="cyl")
        inner = D.lathe(f"earin{sx}", [(r * 0.62, 0.0), (r * 0.5, h * 0.35), (r * 0.28, h * 0.65), (0.0, h * 0.84)],
                        8, color=PINK, mat=FELT, project="cyl")
        inner.data.transform(Matrix.Diagonal((1.0, 0.35, 1.0, 1.0)))
        inner.data.transform(Matrix.Translation(V((0, r * 0.42, 0.012))))
        ear.data.transform(Matrix.Diagonal((1.0, 0.5, 1.0, 1.0)))
        e = K.merge([ear, inner], f"ear{sx}")
        D.put(e, p - n * 0.02, n + V((0.25 * sx, -0.12, 0.35)), up=(0, -1, 0.2), roll=0)
        head.append(e)
    # cat face paint on the kid: a pink nose tip and three whiskers each side
    p, n = KK.ellipsoid_pt(KK.FC, KK.FR, 0.0, 0.775)
    nose = KK.blob("nose", (0, 0, 0), (0.013, 0.009, 0.006), segs=8, rings=4, color=PINK, mat="flat")
    D.put(nose, p + n * 0.001, n)
    head.append(nose)
    for sx in (1, -1):
        for k, (dz, ang) in enumerate([(0.012, 10), (0.0, 0), (-0.012, -10)]):
            pts = []
            for t in range(4):
                x = (0.105 + 0.019 * t) * sx
                z = 0.768 + dz + math.sin(math.radians(ang)) * 0.019 * t
                q, qn = KK.ellipsoid_pt(KK.FC, KK.FR, x, z)
                pts.append(q + qn * 0.0025)
            head.append(D.sweep(f"wh{sx}{k}", pts, [0.0034, 0.0032, 0.0028, 0.0], sides=4, color="ink", mat="flat",
                                project="box"))
    head = D.part(head, "head", KK.NECK)
    KK.tilt_head(head)

    tors = KK.torso(C)
    tree = D.bvh(tors)
    belly, st = KK.patch("belly", tree, V((0, 0, 0.415)), 0.112, 0.138, CB, mat=FELT)
    body = [tors, belly, D.stitches("belly_st", st, length=0.019, width=0.006, height=0.003, color=PINK)]
    # orange felt collar with a gold bell, where the hood meets the body
    cp = []
    for i in range(20):
        a = 2 * math.pi * i / 20
        cp.append(V((math.cos(a) * 0.142, math.sin(a) * 0.132 + 0.004, 0.592 - 0.012 * math.sin(a))))
    body.append(D.sweep("collar", cp, 0.024, sides=7, color="candy_orange", mat=FELT, closed=True, center=(0, 0, 0.59)))
    bell = KK.blob("bell", V((0, -0.165, 0.555)), (0.03, 0.028, 0.03), segs=10, rings=6, color="dragon_gold", mat="metal")
    slot = K.cylinder("bell_slot", 0.004, 0.032, color="foil_gold_dark", mat="flat", verts=4, loc=(0, -0.192, 0.548),
                      rot=(0, 90, 0))
    body += [bell, slot]
    body = D.part(body, "body", (0, 0, KK.HIP_Z))

    def paws(x):
        out = []
        for i, dx in enumerate((-0.042, 0.0, 0.042)):
            out.append(K.sphere(f"toe{i}", 0.03, color=C, mat=FELT, segments=8, rings=5,
                                loc=(x + dx, -0.142 + abs(dx) * 0.4, 0.06), scale=(0.95, 1.1, 0.95)))
        return out
    leg_l = KK.leg("leg_l", KK.HIP_L, C, C, CB, extras=paws)
    leg_r = KK.leg("leg_r", KK.HIP_R, C, C, CB, extras=paws)

    def cuff(i, j):
        return (CB, FELT) if i == 5 else None
    arm_l = KK.arm("arm_l", KK.SHO_L, KK.HAND_L, C, paint=cuff)
    arm_r = KK.arm("arm_r", KK.SHO_R, KK.HAND_R, C, paint=cuff)

    # long tail: out of the back, low swoop, then up into a curl
    ctrl = [(0, 0.1, 0.36), (0.0, 0.24, 0.3), (0.02, 0.37, 0.29), (0.05, 0.47, 0.37), (0.07, 0.52, 0.5),
            (0.07, 0.52, 0.65), (0.05, 0.46, 0.75), (0.02, 0.39, 0.75), (0.01, 0.37, 0.69)]
    rad = [0.06, 0.052, 0.046, 0.043, 0.041, 0.04, 0.039, 0.037, 0.0]
    tl, tp, tr = tube("tail_tube", ctrl, rad, per=3, sides=10, color=C, mat=FELT, up=(0, 0, 1))
    j = 4
    tan = (tp[j + 1] - tp[j - 1]).normalized()
    nrm = (V((0, -0.3, 1)) - tan * V((0, -0.3, 1)).dot(tan)).normalized()
    bw = bow("tail_bow", tp[j] + nrm * (tr[j] + 0.006), nrm, tan, "candy_orange", s=1.4)
    tail = D.part([tl, bw], "tail", KK.TAIL_BASE)
    return dict(body=body, head=head, tail=tail, leg_l=leg_l, leg_r=leg_r, arm_l=arm_l, arm_r=arm_r)


# ================================================================== WITCH
def build_witch():
    DR, DRD, TRIM = "witch_purple", "spider_plum", "witch_green"
    HAT, LINING = "spider_plum", "candy_orange"

    # --- head: the kid's own head, pigtails, the big floppy hat
    head = KK.bare_head()
    head += KK.face(fang=False)
    for sx in (1, -1):
        pt = KK.blob(f"pig{sx}", (0, 0, 0), (0.05, 0.04, 0.075), segs=10, rings=6, color="hair_brown", mat="flat")
        D.put(pt, V((0.235 * sx, 0.04, 0.715)), V((0.9 * sx, 0.25, -0.5)), up=(0, 0, 1))
        tie = KK.blob(f"tie{sx}", (0, 0, 0), (0.026, 0.026, 0.02), segs=8, rings=4, color=TRIM, mat=FELT)
        D.put(tie, V((0.212 * sx, 0.035, 0.75)), V((0.9 * sx, 0.2, -0.4)))
        head += [pt, tie]
    hat = []
    BZ = 0.985
    brim_prof = [(0.17, BZ + 0.012), (0.24, BZ + 0.004), (0.3, BZ - 0.01), (0.335, BZ - 0.026), (0.342, BZ - 0.036),
                 (0.33, BZ - 0.042), (0.29, BZ - 0.032), (0.23, BZ - 0.016), (0.17, BZ - 0.01)]

    def brim_warp(c):
        a = math.atan2(c.y, c.x)
        r = math.hypot(c.x, c.y)
        k = max(0.0, (r - 0.2) / 0.14)
        return V((c.x, c.y, c.z + k * (0.022 * math.sin(3 * a + 0.8) - 0.02 * max(0.0, math.sin(a)))))
    # lathe needs bottom-to-top ordering for outward normals: the brim ring is a closed loop instead
    brim = D.lathe("brim", list(reversed(brim_prof)), 26, color=HAT, mat=FELT, project="cyl", warp=brim_warp,
                   center=(0, 0, BZ))
    hat.append(brim)
    # tall cone whose top third flops over sideways and back (reads from the front and from behind)
    cone_ctrl = [(0, 0, BZ - 0.01), (0, 0.012, BZ + 0.12), (0.006, 0.035, BZ + 0.23), (0.035, 0.065, BZ + 0.31),
                 (0.095, 0.085, BZ + 0.345), (0.16, 0.095, BZ + 0.325), (0.2, 0.1, BZ + 0.26)]
    cone, cp, cr = tube("cone", cone_ctrl, [0.172, 0.138, 0.102, 0.072, 0.052, 0.034, 0.0], per=3, sides=14, color=HAT,
                        mat=FELT, project="cyl")
    hat.append(cone)
    band = D.lathe("band", [(0.165, BZ - 0.004), (0.178, BZ + 0.0), (0.172, BZ + 0.045), (0.155, BZ + 0.05)], 16,
                   color=TRIM, mat=FELT, project="cyl")
    band.data.transform(Matrix.Translation(V((0, 0.0005, 0))))
    hat.append(band)
    buckle = K.box("buckle", (0.06, 0.016, 0.05), color="dragon_gold", mat="metal", loc=(0, -0.172, BZ + 0.024), bevel=0.006)
    hole = K.box("buckle_hole", (0.03, 0.012, 0.024), color=TRIM, mat="flat", loc=(0, -0.178, BZ + 0.024))
    hat += [buckle, hole]
    # a little felt moon patch on the cone
    star = K.polygon_prism("moon", [(math.cos(a) * 0.03, math.sin(a) * 0.03) for a in
                                    [math.radians(d) for d in range(-120, 121, 30)]] +
                           [(math.cos(a) * 0.02 + 0.012, math.sin(a) * 0.022) for a in
                            [math.radians(d) for d in range(110, -111, -30)]], 0.006, color="candy_lemon", mat="flat")
    hn = V((0, -1, 0.22)).normalized()
    D.put(star, cp[4] + V((0, -cr[4] + 0.004, 0)), hn, roll=10)
    hat.append(star)
    hatob = K.merge(hat, "hat")
    # jaunty: tipped back a little (brim lifts off the eyes) and to the left
    hatob.data.transform(Matrix.Translation(V((0, 0.015, BZ))) @ Matrix.Rotation(math.radians(-9), 4, "X") @
                         Matrix.Rotation(math.radians(-6), 4, "Y") @ Matrix.Translation(V((0, 0, -BZ))))
    head.append(hatob)
    head = D.part(head, "head", KK.NECK)
    KK.tilt_head(head)

    # --- body: dress bodice + a gently flared skirt with a pointy green-trimmed hem, belt and buckle
    tors = KK.torso(DR)
    skirt_prof = [(0.0, 0.29), (0.15, 0.255), (0.214, 0.232), (0.226, 0.226), (0.232, 0.24), (0.222, 0.29),
                  (0.208, 0.35), (0.2, 0.41), (0.196, 0.45), (0.17, 0.47)]

    def skirt_warp(c):
        a = math.atan2(c.y, c.x)
        r = math.hypot(c.x, c.y)
        k = min(max((0.3 - c.z) / 0.07, 0.0), 1.0)
        tri = abs(((5 * a / math.pi) % 2.0) - 1.0)          # 10 points around
        z = c.z - k * 0.04 * (1 - tri)
        r *= 1 + k * 0.02 * math.cos(10 * a)
        return V((r * math.cos(a), r * math.sin(a) * 0.9 + 0.012, z))
    skirt = D.lathe("skirt", skirt_prof, 30, color=DR, mat=FELT, project="sphere", warp=skirt_warp, center=(0, 0, 0.35))
    K.paint(skirt, TRIM, where=lambda c, n: c.z < 0.262, mat=FELT)
    belt = []
    for i in range(22):
        a = 2 * math.pi * i / 22
        belt.append(V((math.cos(a) * 0.2, math.sin(a) * 0.188 + 0.012 - 0.012 * max(0, -math.sin(a)) * 1.0, 0.46)))
    body = [tors, skirt, D.sweep("belt", belt, 0.017, sides=6, color=DRD, mat=FELT, closed=True, center=(0, 0, 0.46))]
    tree = D.bvh(tors)
    loc, nrm = KK.on_surface(tree, (0, -0.3, 0.46), (0, 1, 0))
    bb = K.box("belt_buckle", (0.05, 0.014, 0.04), color="dragon_gold", mat="metal", bevel=0.006)
    D.put(bb, loc + nrm * 0.02, nrm)
    body.append(bb)
    # pointed green collar points over the shoulders (front)
    for sx in (1, -1):
        pts = [(0.0, 0.0), (0.06 * sx, -0.02), (0.035 * sx, -0.07)]
        if sx < 0:
            pts = list(reversed(pts))
        col = K.polygon_prism(f"collar{sx}", KK.rounded_poly(pts, 0.008, 2), 0.01, color=TRIM, mat=FELT)
        loc, nrm = KK.on_surface(tree, (0.012 * sx, -0.3, 0.6), (0, 1, 0.3))
        D.put(col, loc + nrm * 0.004, nrm + V((0, 0, 0.0)), up=(0, 0, 1))
        body.append(col)
    body = D.part(body, "body", (0, 0, KK.HIP_Z))

    # --- arms: purple bell sleeves with green cuffs; legs: striped stockings and pointy curled boots
    def cuff(i, j):
        return (TRIM, FELT) if i == 5 else None
    arm_l = KK.arm("arm_l", KK.SHO_L, KK.HAND_L, DR, r0=0.066, flare=0.026, paint=cuff)
    arm_r = KK.arm("arm_r", KK.SHO_R, KK.HAND_R, DR, r0=0.066, flare=0.026, paint=cuff)

    def buckle_x(x):
        return [K.box("bt_buckle", (0.034, 0.012, 0.026), color="dragon_gold", mat="metal", loc=(x, -0.118, 0.105),
                      rot=(-38, 0, 0), bevel=0.004)]
    leg_l = KK.leg("leg_l", KK.HIP_L, "pumpkin_light", "licorice_black", "stem_brown", stripes=("licorice_black", 0.034),
                   toe=0.06, curl=0.05, extras=buckle_x)
    leg_r = KK.leg("leg_r", KK.HIP_R, "pumpkin_light", "licorice_black", "stem_brown", stripes=("licorice_black", 0.034),
                   toe=0.06, curl=0.05, extras=buckle_x)

    # --- wings: the two halves of the cape, hanging from the shoulders down the back
    def cape(name, root, sx):
        def P(u, v):
            x = 0.006 + u * (0.15 + 0.21 * v * v)          # flares out wide toward the hem, like a twirl
            z = 0.635 - v * 0.42 - 0.025 * u * u * (1 - v) + 0.07 * u ** 3 * v
            if v > 0.999:      # pointed hem: three points per half
                z -= 0.04 * abs(math.sin(u * math.pi * 1.5 + 0.0))
            y = back_y(x, z, 0.03 + 0.06 * v) + 0.018 * math.sin(u * math.pi * 3) * v + 0.04 * u * u * v
            return (x * sx, max(y, 0.105), z)
        ob = panel(name + "_m", P, 9, 8, 0.02, DRD, LINING, out_hint=lambda u, v: (0.6 * sx * u, 1.0, 0.0),
                   out_paint=lambda i, j: LINING if j == 7 else None)
        return D.part([ob], name, root)
    wing_l = cape("wing_l", KK.WING_L, 1)
    wing_r = cape("wing_r", KK.WING_R, -1)
    return dict(body=body, head=head, wing_l=wing_l, wing_r=wing_r, leg_l=leg_l, leg_r=leg_r, arm_l=arm_l, arm_r=arm_r)


# ================================================================== SKELETON
def build_skeleton():
    C, B, BS = "charcoal", "bone", "bone_shade"

    # --- skull hood: bone white. Its face opening is lower than the dragon's (the skull's toothy mouth,
    # framing the kid's eyes, cheeks and smile), so the skull's own eye sockets and nose sit on a brow
    # that faces forward (sockets on the crown read as a panda).
    SA, SC, SZ = 0.18, 0.125, 0.77
    head = KK.hood(B, rim_color="ink", stitches=False, mat="flat", rim_mat="flat", rim_r=0.03,
                   warp=dome(0.3, 2.0, 0.88), oa=SA, oc=SC, oz=SZ)
    tree = D.bvh(head[0])
    head += KK.face(fang=False)
    head += hood_eyes("hole", "ink", at=(0.078, 0.995), size=1.12, roll=-14, tree=tree, face_fwd=0.15)
    # little upside-down heart nose socket between the eyes
    p, n = KK.on_surface(tree, (0.0, -0.6, 0.948), (0, 1, 0))
    nose = K.polygon_prism("nose", KK.rounded_poly([(0.0, -0.018), (0.02, 0.011), (-0.02, 0.011)], 0.006, 2), 0.012,
                           color="ink", mat="flat")
    D.put(nose, p - n * 0.006, n)
    head.append(nose)
    # teeth: rounded bone pegs standing on the dark mouth rim, top and bottom
    for i in range(14):
        a = 2 * math.pi * (i + 0.5) / 14
        if abs(math.sin(a)) < 0.45:
            continue           # leave the sides of the face clear
        p, n = KK.ellipsoid_pt(KK.HC, KK.HR, math.cos(a) * SA, SZ + math.sin(a) * SC)
        inward = V((-math.cos(a) * SA, 0, -math.sin(a) * SC)).normalized()
        s = 0.8 if math.sin(a) > 0 else 1.0       # the top row is a little smaller (above the kid's eyes)
        tooth = KK.blob(f"tooth{i}", (0, 0, 0), (0.017, 0.021 * s, 0.011), segs=8, rings=5, color="lolly_white",
                        mat="flat")
        D.put(tooth, p - n * 0.004 + n * 0.028 + inward * 0.012 * s, n, up=inward)
        head.append(tooth)
    head = D.part(head, "head", KK.NECK)
    KK.tilt_head(head)

    # --- body with the bone print
    tors = KK.torso(C)
    tree = D.bvh(tors)
    body = [tors]

    def surf(x, z, back=False):
        return KK.on_surface(tree, (x, 0.3 if back else -0.3, z), (0, -1 if back else 1, 0))
    pts = []
    for z in (0.57, 0.53, 0.49, 0.45, 0.41):
        loc, nrm = surf(0.0, z)
        pts.append(loc + nrm * 0.008)
    body.append(D.sweep("sternum", pts, [0.012, 0.016, 0.016, 0.016, 0.011], sides=6, color=B, mat="flat", project="box"))
    for k, (z0, w) in enumerate([(0.555, 0.84), (0.505, 0.86), (0.455, 0.82), (0.405, 0.72)]):
        for sx in (1, -1):
            rp = []
            for t in range(6):
                u = t / 5
                z = z0 + 0.018 * u - 0.05 * u * u
                x = (0.026 + u * (w * KK.torso_r(z) - 0.026)) * sx
                loc, nrm = surf(x, z)
                rp.append(loc + nrm * 0.007)
            body.append(D.sweep(f"rib{k}{sx}", rp, [0.012, 0.012, 0.0115, 0.011, 0.01, 0.0075], sides=6, color=B,
                                mat="flat", project="box"))
    # pelvis on the hips
    for sx in (1, -1):
        loc, nrm = surf(0.06 * sx, 0.3)
        pel = KK.blob(f"pelvis{sx}", (0, 0, 0), (0.055, 0.036, 0.008), segs=10, rings=5, color=B, mat="flat")
        D.put(pel, loc + nrm * 0.004, nrm, roll=-30 * sx)
        body.append(pel)
    # spine and back ribs (the gameplay camera mostly sees the back, so it carries a full print too)
    for i, z in enumerate((0.6, 0.555, 0.51, 0.465, 0.42, 0.375, 0.33)):
        loc, nrm = surf(0.0, z, back=True)
        vb = KK.blob(f"vert{i}", (0, 0, 0), (0.034 - i * 0.0012, 0.017, 0.009), segs=8, rings=4, color=B, mat="flat")
        D.put(vb, loc + nrm * 0.004, nrm)
        body.append(vb)
    for k, (z0, w) in enumerate([(0.54, 0.86), (0.48, 0.88), (0.42, 0.8)]):
        for sx in (1, -1):
            rp = []
            for t in range(6):
                u = t / 5
                z = z0 + 0.012 * u - 0.05 * u * u
                x = (0.04 + u * (w * KK.torso_r(z) - 0.04)) * sx
                loc, nrm = surf(x, z, back=True)
                rp.append(loc + nrm * 0.007)
            body.append(D.sweep(f"brib{k}{sx}", rp, [0.0125, 0.0125, 0.012, 0.011, 0.01, 0.0075], sides=6, color=B,
                                mat="flat", project="box"))
    body = D.part(body, "body", (0, 0, KK.HIP_Z))

    # --- arms: humerus and forearm bones along the outside of each sleeve
    def arm_bones(pts, rr, d):
        out = []
        sx = 1 if pts[0].x > 0 else -1
        side = V((sx, -0.55, 0.15)).normalized()
        for k, (t0, t1) in enumerate([(1, 3), (4, 6)]):
            def at(i):
                tg = (pts[min(i + 1, len(pts) - 1)] - pts[max(i - 1, 0)]).normalized()
                o = (side - tg * side.dot(tg)).normalized()
                return pts[i] + o * rr[i], o
            a, na = at(t0)
            b, nb = at(t1)
            if k == 1:
                b = b - (b - a).normalized() * 0.012
            out.append(bone(f"abone{k}", a, b, (na + nb).normalized(), r=0.0095))
        return out
    arm_l = KK.arm("arm_l", KK.SHO_L, KK.HAND_L, C, extras=arm_bones)
    arm_r = KK.arm("arm_r", KK.SHO_R, KK.HAND_R, C, extras=arm_bones)

    def leg_bones(x):
        out = [bone("femur", (x, -0.083, 0.29), (x, -0.081, 0.175), (0, -1, 0), r=0.011),
               bone("femur_b", (x, 0.083, 0.29), (x, 0.081, 0.175), (0, 1, 0), r=0.011)]
        for i, dx in enumerate((-0.03, 0.0, 0.03)):
            tb = KK.blob(f"toeb{i}", (0, 0, 0), (0.012, 0.022, 0.008), segs=7, rings=4, color=B, mat="flat")
            D.put(tb, V((x + dx, -0.135 + abs(dx) * 0.3, 0.098)), V((0, -0.55, 1)), up=(0, -1, 0))
            out.append(tb)
        return out
    leg_l = KK.leg("leg_l", KK.HIP_L, C, C, B, extras=leg_bones)
    leg_r = KK.leg("leg_r", KK.HIP_R, C, C, B, extras=leg_bones)
    return dict(body=body, head=head, leg_l=leg_l, leg_r=leg_r, arm_l=arm_l, arm_r=arm_r)


# ================================================================== PUMPKIN
def leaf_outline(size, lobes=5):
    """Palmate pumpkin leaf outline (CCW), stem end at the origin, pointing +Y."""
    out = []
    n = 30
    for i in range(n):
        th = -math.pi * 0.92 + 1.84 * math.pi * i / (n - 1)
        lob = 0.68 + 0.32 * abs(math.cos(lobes * th / 2))
        r = size * lob
        out.append((math.sin(th) * r, 0.5 * size + math.cos(th) * r * 0.95))
    return out


def build_pumpkin():
    P, PD, PL = "pumpkin", "pumpkin_dark", "pumpkin_light"
    G, GD, STEM = "vine", "vine_dark", "stem_brown"

    # --- head: the kid's head with a green beanie: curly stem, two leaves and a tendril
    head = KK.bare_head()
    head += KK.face(fang=False)
    CAPC, CAPR = KK.HAIR_C + V((0, 0.0, 0.004)), KK.HAIR_R + V((0.012, 0.012, 0.012))

    def capcut(c):
        return c.z < 0.905 - 0.05 * max(0.0, (c.y - CAPC.y) / CAPR.y)
    cap = KK.blob("cap", CAPC, CAPR, segs=22, rings=12, color=G, mat=FELT, cut=capcut)
    head.append(cap)
    bandp = []
    for i in range(24):
        a = 2 * math.pi * i / 24
        z = 0.905 - 0.05 * max(0.0, math.sin(a)) + 0.004
        p, n = KK.ellipsoid_dir(CAPC, CAPR, (math.cos(a) * 0.9, math.sin(a) * 0.9, (z - CAPC.z) / CAPR.z * 0.9))
        q = CAPC + V((math.cos(a), math.sin(a), 0)) * 0.0
        r = math.sqrt(max(1 - ((z - CAPC.z) / CAPR.z) ** 2, 0.0))
        bandp.append(V((math.cos(a) * CAPR.x * r, CAPC.y + math.sin(a) * CAPR.y * r, z)))
    head.append(D.sweep("cap_band", bandp, 0.022, sides=7, color=GD, mat=FELT, closed=True, center=CAPC,
                        warp=lambda a, i: 1.0 + 0.12 * math.cos(2 * a) * (i % 2)))
    top = CAPC + V((0, 0, CAPR.z - 0.012))
    stem, sp, srr = tube("stem", [top, top + V((0, 0.0, 0.06)), top + V((0.012, 0.02, 0.11)), top + V((0.04, 0.045, 0.13))],
                         [0.04, 0.034, 0.03, 0.028], per=2, sides=7, color=STEM, mat="bark", project="cyl")
    head.append(stem)
    for sx, yaw, tilt, s in ((1, -40, 48, 0.1), (-1, 205, 52, 0.09)):
        lf = K.polygon_prism(f"leaf{sx}", leaf_outline(s), 0.012, color=G, mat=FELT)
        K.deform(lf, lambda c: V((c.x, c.y, c.z - 2.2 * c.x * c.x - 0.6 * c.y * c.y * 0.5)))
        lf.rotation_euler = (math.radians(tilt), 0, math.radians(yaw))
        lf.location = top + V((0.02 * sx, 0.0, -0.012))
        head.append(lf)
        # centre vein
        vn = K.cylinder(f"vein{sx}", 0.004, s * 1.2, color=GD, mat="flat", verts=4, rot=(90, 0, 0))
        vn.location = (0, s * 0.6, 0.012)
        vn.parent = lf
        head.append(vn)
    curl = []
    for i in range(16):
        t = i / 15
        a = t * math.pi * 3.2
        rr = 0.045 * (1 - 0.6 * t)
        curl.append(top + V((-0.03 - math.cos(a) * rr, 0.02 + math.sin(a) * rr * 0.6, 0.02 + t * 0.06 + math.sin(a) * rr * 0.6)))
    head.append(D.sweep("tendril", curl, [0.0065 * (1 - 0.5 * i / 15) for i in range(16)], sides=5, color=G, mat="flat",
                        project="box"))
    head = D.part(head, "head", KK.NECK)
    KK.tilt_head(head)

    # --- body: a round ribbed pumpkin (narrow enough in X that both hands stay outside it)
    RX, RY, RZ, CZ = 0.222, 0.248, 0.235, 0.43
    pprof = [(0.0, -1.0)]
    for deg in range(15, 180, 15):
        phi = math.radians(deg)
        pprof.append((math.sin(phi), -math.cos(phi)))
    pprof.append((0.0, 1.0))

    def pwarp(c):
        a = math.atan2(c.y, c.x)
        r = math.hypot(c.x, c.y)
        rib = 1.0 + 0.05 * math.cos(10 * a) - 0.02
        zz = c.z
        if abs(zz) > 0.8:     # dimpled top and bottom
            zz -= math.copysign(0.18 * (1 - r) * (abs(zz) - 0.8) / 0.2, zz)
        return V((r * rib * math.cos(a) * RX, r * rib * math.sin(a) * RY + 0.01, CZ + zz * RZ))
    pump = D.lathe("pumpkin_body", pprof, 40, color=P, mat="pumpkin", project="cyl", warp=pwarp, center=(0, 0, CZ))
    tree = D.bvh(pump)
    body = [pump]
    # stitched felt jack-o'-lantern face on the tummy
    eye = KK.rounded_poly([(-0.03, -0.018), (0.03, -0.018), (0.0, 0.026)], 0.007, 2)
    for sx in (1, -1):
        body.append(KK.decal(f"jeye{sx}", tree, eye, (0.055 * sx, -0.3, 0.47), (0, 1, 0), color="ink", mat=FELT))
    grin = []
    Wg = 0.085
    for i in range(11):
        x = -Wg + 2 * Wg * i / 10
        u = x / Wg
        grin.append((x, -0.038 * (1 - u * u) + 0.012 * u * u))
    for i in range(9, 0, -1):
        x = -Wg + 2 * Wg * i / 10
        u = x / Wg
        z = -0.006 * (1 - u * u) + 0.016 * u * u
        if abs(x - 0.03) < 0.012 or abs(x + 0.03) < 0.012:
            z -= 0.016          # two little teeth
        grin.append((x, z))
    body.append(KK.decal("jgrin", tree, grin, (0.0, -0.3, 0.385), (0, 1, 0), color="ink", mat=FELT))
    # leafy green collar around the neck
    for i in range(7):
        a = 2 * math.pi * (i + 0.5) / 7 - math.pi / 2
        d = V((math.cos(a), math.sin(a), 0))
        lf = KK.blob(f"collar{i}", (0, 0, 0), (0.043, 0.066, 0.012), segs=8, rings=6, color=G if i % 2 else "stem_green",
                     mat=FELT, warp=lambda p: V((p.x * (1 - 0.6 * max(0.0, p.y / 0.066)), p.y, p.z + 0.5 * p.y * p.y)))
        loc, nrm = KK.on_surface(tree, V((0, 0.01, 0.6)) + d * 0.15 + V((0, 0, 0.05)), -d * 0.6 + V((0, 0, -1)))
        D.put(lf, loc + nrm * 0.012, nrm, up=d + V((0, 0, -0.5)))
        body.append(lf)
    body.append(D.lathe("neck", [(0.0, 0.6), (0.13, 0.6), (0.13, 0.66), (0.0, 0.68)], 12, color=GD, mat=FELT, project="cyl"))
    body = D.part(body, "body", (0, 0, KK.HIP_Z))

    arm_l = KK.arm("arm_l", KK.SHO_L, KK.HAND_L, "stem_green")
    arm_r = KK.arm("arm_r", KK.SHO_R, KK.HAND_R, "stem_green")
    leg_l = KK.leg("leg_l", KK.HIP_L, "stem_green", STEM, "bark_dark")
    leg_r = KK.leg("leg_r", KK.HIP_R, "stem_green", STEM, "bark_dark")

    # --- wings: two big pumpkin leaves on the back (they flap on double jumps, spread to glide)
    def leafwing(name, root, sx):
        s = 0.125
        lf = K.polygon_prism(name + "_m", leaf_outline(s), 0.014, color="apple_green", mat=FELT)
        K.deform(lf, lambda c: V((c.x, c.y, c.z + 1.4 * c.x * c.x - 0.25 * c.y * c.y)))
        vein = []
        for k, ang in enumerate((0, 50, -50)):
            L = s * (1.05 if k == 0 else 0.75)
            t = math.radians(ang)
            p0 = V((0, 0.0, 0.016))
            p1 = V((math.sin(t) * L, 0.02 + math.cos(t) * L, 0.016 + 1.4 * (math.sin(t) * L) ** 2 - 0.25 * (math.cos(t) * L) ** 2))
            vein.append(D.sweep(f"{name}_v{k}", [p0, p0.lerp(p1, 0.5) + V((0, 0, 0.004)), p1], [0.008, 0.006, 0.0], sides=5,
                                color=GD, mat="flat", project="box"))
        ob = K.merge([lf] + vein, name + "_tmp")
        # stem at the shoulder blade, leaf pointing up and out, veined side facing back and a little up
        ob.data.transform(Matrix.Rotation(math.radians(180), 4, "Y"))      # veins to -Z
        ob.data.transform(Matrix.Rotation(math.radians(100), 4, "X"))      # leaf up, veins face back (+Y)
        # splayed out almost level, like the dragon's wings: the game's spread pose (+43 deg about the
        # front axis) then lifts them into a V instead of folding them up behind the head
        ob.data.transform(Matrix.Rotation(math.radians(74 * sx), 4, "Y"))
        ob.data.transform(Matrix.Rotation(math.radians(-14 * sx), 4, "Z"))  # tips swept back a little
        ob.data.transform(Matrix.Translation(V((0.03 * sx, 0.12, 0.08))))   # up on the shoulders, clear of the pumpkin
        ob.location = V(root)
        return D.part([ob], name, root)
    wing_l = leafwing("wing_l", KK.WING_L, 1)
    wing_r = leafwing("wing_r", KK.WING_R, -1)
    return dict(body=body, head=head, wing_l=wing_l, wing_r=wing_r, leg_l=leg_l, leg_r=leg_r, arm_l=arm_l, arm_r=arm_r)


BUILDERS = dict(kid_ghost=build_ghost, kid_cat=build_cat, kid_witch=build_witch, kid_skeleton=build_skeleton,
                kid_pumpkin=build_pumpkin)


def main():
    argv = sys.argv[sys.argv.index("--") + 1:] if "--" in sys.argv else []
    ids = [a for a in argv if not a.startswith("--")] or ALL
    sheet = "--nosheet" not in argv
    out = Path(__file__).resolve().parent / "build_log.json"
    log = json.loads(out.read_text()) if out.exists() else {}
    for aid in ids:
        K.reset()
        parts = BUILDERS[aid]()
        total, counts = KK.finish(aid, parts, sheet=sheet)
        log[aid] = {"tris": total, "parts": counts}
        if "--pose" in argv:
            glide_sheet(aid, parts)
    out.write_text(json.dumps(log, indent=1))


def glide_sheet(aid, parts):
    """After export: pose the rig like the game does while gliding (dragon_controller.cpp: wings
    pose (0, -w/2, w) / (0, w/2, -w) with w = 0.75, legs tucked -0.35, arm_l (-0.6, 0, 0.5), arm_r
    (0, 0, -0.2); Godot YXZ Euler, Godot (X, Y, Z) = Blender (X, Z, -Y)) and render
    art/review/sheets/<id>_glide.png. Not exported."""
    def godot_euler(ob, x, y, z):
        ob.matrix_basis = (Matrix.Translation(ob.location) @ Matrix.Rotation(y, 4, "Z") @
                           Matrix.Rotation(x, 4, "X") @ Matrix.Rotation(-z, 4, "Y"))
    w = 0.75
    if "wing_l" in parts:
        godot_euler(parts["wing_l"], 0, -w * 0.5, w)
        godot_euler(parts["wing_r"], 0, w * 0.5, -w)
    for leg in ("leg_l", "leg_r"):
        godot_euler(parts[leg], -0.35, 0, 0)
    godot_euler(parts["arm_l"], -0.6, 0, 0.5)
    godot_euler(parts["arm_r"], 0, 0, -0.2)
    if "tail" in parts:
        godot_euler(parts["tail"], 0.25, 0.3, 0)
    root = parts["body"].parent
    K.review_sheet([root], f"{aid}_glide")


main()
