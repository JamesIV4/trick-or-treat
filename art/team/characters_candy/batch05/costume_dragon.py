"""dragon_kid v2: the player's default costume on kit v2 (pilot for the other five).

The same design as batch03/build_dragon_kid.py at higher fidelity: plush jade onesie, cream belly
plates sewn on with dark stitches, gold horns, four crest spikes + four back spikes + six tail spikes
and a gold tail tip, the hood's own cartoon dragon eyes (the "four-eye" hood), bat wings with a
scalloped membrane, a fat upturned tail, round boots with three gold claws, rolled cuffs, the kid's
face v2 and gripping hands. Rig contract v2 (CHECKPOINT.md): wing_l / wing_r / tail stay single meshes
with their v1 origins so the shader bend (soft_pin / soft_bend) and the tail wave keep working.
"""
import math

import bmesh
from mathutils import Matrix

import kid_kit as KK
from kid_kit import D, K, V, PLUSH

T, TD, TL, TJL = "dragon_jade", "dragon_green_dark", "dragon_green_light", "dragon_jade_light"
SPK, GOLD, CREAM, CREAMD = "dragon_spike", "dragon_gold", "dragon_cream", "dragon_cream_dark"
WING_SCALE = 1.4


def spike(name, h, r, segs=10, color=SPK, mat=PLUSH):
    """Rounded plush spike (pointing up local Z, base at z = 0)."""
    prof = [(r, -0.008), (r * 0.97, h * 0.12), (r * 0.87, h * 0.33), (r * 0.68, h * 0.57), (r * 0.44, h * 0.77),
            (r * 0.2, h * 0.93), (0.0, h)]
    return D.lathe(name, prof, segs, color=color, mat=mat, project="cyl")


# ================================================================== body (origin at the pelvis)
def body():
    torso = KK.torso2(T)
    tree = D.bvh(torso)
    PC, PRU, PRV = V((0, 0, 0.415)), 0.118, 0.142
    base, _ = KK.patch("belly", tree, PC, PRU, PRV, CREAM, seg=28, heights=(0.0062, 0.006, 0.0052, 0.0032, -0.002))
    parts = [torso, base]
    # belly plates: four pillowy scutes, each lapping over the next like shingles
    NU, NV = 14, 5
    for k, (v0, v1) in enumerate([(0.83, 0.38), (0.42, -0.02), (0.02, -0.42), (-0.38, -0.83)]):
        bm = bmesh.new()
        grid = []
        for j in range(NV + 1):
            fv = j / NV
            v = v0 + (v1 - v0) * fv
            w = PRU * 0.9 * math.sqrt(max(1 - v * v, 0.03))
            row = []
            for i in range(NU + 1):
                u = -1 + 2 * i / NU
                edge = i in (0, NU) or j in (0, NV)
                h = 0.0012 if edge else 0.0045 + (0.0075 * math.sin(math.pi * fv) ** 0.7 + 0.0035 * fv) * (1 - abs(u) ** 3)
                loc, nrm = D.hit(tree, PC + V((u * w, -1.0, v * PRV)), (0, 1, 0))
                row.append(bm.verts.new(loc + nrm * h))
            grid.append(row)
        lips = []
        for j in range(NV):
            for i in range(NU):
                f = bm.faces.new((grid[j][i], grid[j + 1][i], grid[j + 1][i + 1], grid[j][i + 1]))
                f.normal_update()
                if f.normal.y > 0:
                    f.normal_flip()
                if j == NV - 1:
                    lips.append(f)
        parts.append(D.mesh(bm, f"plate{k}", CREAM, "flat", "box", center=PC, extra=[(lips, CREAMD, "flat")]))
    items = D.ring_on(tree, PC, (1, 0, 0), (0, 0, 1), PRU * 0.965, PRV * 0.965, 22, (0, 1, 0), back=1.0, offset=0.0022)
    parts.append(D.stitches("belly_st", items, length=0.016, width=0.0055, height=0.003, color=TD))
    # four soft spikes down the back, continuing the hood crest to the tail
    for i, (z, s) in enumerate([(0.545, 0.95), (0.475, 1.0), (0.405, 0.92), (0.345, 0.8)]):
        loc, nrm = D.hit(tree, (0, 1.0, z), (0, -1, 0))
        sp = spike(f"bspike{i}", 0.08 * s, 0.04 * s)
        D.put(sp, loc - nrm * 0.004, nrm + V((0, 0, -0.3)), up=(0, 0, 1), scale=(0.6, 1.0, 1.0))
        parts.append(sp)
    return D.part(parts, "body", (0, 0, KK.HIP_Z))


# ================================================================== legs: thigh, and the boot as the shin
def boot(name, x):
    """Round plush dragon boot with a rolled cuff and three gold claws (world space, round the foot at
    x). The ankle stays round and centred under the knee so it can turn about it."""
    def foot_k(z):
        return min(max((0.14 - z) / 0.08, 0.0), 1.0)

    def warp(c):
        k = foot_k(c.z)
        y = c.y * (1.0 + 0.32 * k) - 0.028 * k
        if c.y < 0:
            y -= 0.012 * k * (1 - min(abs(c.x) / 0.1, 1.0))
        return V((c.x, y, c.z))
    prof = [(0.0, 0.03), (0.072, 0.03), (0.092, 0.05), (0.098, 0.083), (0.095, 0.112), (0.089, 0.138), (0.08, 0.16),
            (0.062, 0.182), (0.034, 0.196), (0.0, 0.2)]
    parts = [D.lathe(name + "_boot", prof, 20, color=T, mat=PLUSH, project="sphere", warp=warp, loc=(x, 0, 0),
                     center=(0, 0, 0.09)),
             D.lathe(name + "_sole", [(0.0, 0.0), (0.086, 0.0), (0.099, 0.008), (0.102, 0.021), (0.096, 0.033), (0.0, 0.036)],
                     18, color=TD, mat="flat", project="cyl", warp=lambda c: V((c.x * 1.02, c.y * 1.36 - 0.03, c.z)),
                     loc=(x, 0, 0)),
             KK.ring(name + "_cuff", (x, 0, 0.15), (0, 0, 1), 0.079, 0.021, n=18, sides=8, color=TJL, mat=PLUSH, squash=1.3)]
    for i, dx in enumerate((-0.04, 0.0, 0.04)):
        parts.append(KK.blob(f"{name}_claw{i}", (x + dx, -0.152 + abs(dx) * 0.45, 0.046), (0.0245, 0.031, 0.0215), segs=10,
                             rings=6, color=GOLD, mat="flat"))
    return parts


# ================================================================== head (origin at the neck)
def head():
    HC, HR = KK.HC, KK.HR
    parts = KK.hood2(T, rim_color=TJL, stitch_color=TD)
    static, children = KK.face2(fang=True)
    parts += static + KK.fringe2(clip=KK.hood_clip())
    # big golden horns sweeping up and back, growing straight out of the hood
    for sx in (1, -1):
        p, n = KK.ellipsoid_dir(HC, HR, (0.47 * sx, 0.12, 0.87))
        h, r = 0.15, 0.06
        prof = [(r, -0.025), (r, 0.0), (r * 0.97, h * 0.14), (r * 0.9, h * 0.3), (r * 0.8, h * 0.46), (r * 0.68, h * 0.62),
                (r * 0.53, h * 0.76), (r * 0.36, h * 0.88), (r * 0.18, h * 0.965), (0.0, h)]
        horn = D.lathe(f"horn{sx}", prof, 14, color=GOLD, mat="flat", project="cyl",
                       warp=lambda c: V((c.x, c.y + 2.6 * c.z * c.z, c.z)))
        D.put(horn, p - n * 0.014, n + V((0.16 * sx, 0.1, 0.3)), up=(0, 1, 0))
        parts.append(horn)
    # the hood's own cartoon dragon eyes on its brow (the hood reads as a dragon head)
    for sx in (1, -1):
        p, n = KK.ellipsoid_dir(HC, HR, (0.34 * sx, -0.52, 0.8))
        W, H, Dp = 0.036, 0.04, 0.02
        roll = -12 * sx
        ew = KK.blob(f"heye{sx}", (0, 0, 0), (W, H, Dp), segs=16, rings=8, color="eye_white", mat="flat")
        D.put(ew, p - n * 0.006, n, roll=roll)
        M = D.look(n, roll=roll)

        def on_eye(lx, ly, lift=0.0):
            lz = Dp * math.sqrt(max(1 - (lx / W) ** 2 - (ly / H) ** 2, 0.0))
            return p - n * 0.006 + M @ V((lx, ly, lz + lift))
        pupil = KK.blob(f"hpupil{sx}", (0, 0, 0), (0.017, 0.021, 0.006), segs=12, rings=4, color="ink", mat="flat")
        D.put(pupil, on_eye(-0.008 * sx, -0.004), n, roll=roll)
        hl = KK.blob(f"hhl{sx}", (0, 0, 0), (0.0058, 0.0066, 0.003), segs=8, rings=3, color="eye_white", mat="flat")
        D.put(hl, on_eye(-0.008 * sx - 0.006, 0.004, 0.0052), n, roll=roll)
        parts += [ew, pupil, hl]
    # soft crest spikes over the top and down the back of the hood
    for i, (ang, s) in enumerate([(14, 0.9), (48, 1.05), (84, 1.05), (120, 0.9)]):
        a = math.radians(ang)
        p, n = KK.ellipsoid_dir(HC, HR, (0, math.sin(a), math.cos(a)))
        sp = spike(f"crest{i}", 0.08 * s, 0.04 * s)
        D.put(sp, p - n * 0.006, n + V((0, 0.3, 0)), up=(0, 1, 0), scale=(0.58, 1.0, 1.0))
        parts.append(sp)
    return D.part(parts, "head", KK.NECK), children


# ================================================================== wings (origin at the wing root, ONE mesh)
def wing(name, root_pt, sx):
    """Bat wing: a leading-edge bone and two finger ribs over a membrane with a scalloped trailing
    edge. The membrane is a fan grid from the root (8 rings out to the edge) and cups a little between
    the ribs, so the rig's bend along the wing curves it smoothly. Same outline, spread and root as v1."""
    tips = [V((0.3, 0.255)), V((0.255, 0.075)), V((0.14, -0.005))]
    lead = [V((0.0, 0.075)), V((0.09, 0.17)), V((0.2, 0.24))]
    foot = V((0.0, -0.03))
    R = V((0.012, 0.05))
    lp, _ = KK.catmull([(p.x, p.y, 0.0) for p in lead + [tips[0]]], per=5)
    bd = [V((p.x, p.y)) for p in lp]
    marks = [0, len(bd) - 1]
    chain = [tips[0], tips[1], tips[2], foot]
    for a, b in zip(chain[:-1], chain[1:]):
        mid = (a + b) / 2
        ctrl = mid + (V((0.02, 0.09)) - mid) * 0.34
        for k in range(1, 7):
            t = k / 6
            bd.append(a * (1 - t) ** 2 + ctrl * 2 * t * (1 - t) + b * t * t)
        marks.append(len(bd) - 1)
    NB, NT = len(bd), 8

    def cup(b):
        s = max(i for i in range(len(marks) - 1) if marks[i] <= b and i < len(marks) - 1)
        s = min(s, len(marks) - 2)
        f = (b - marks[s]) / (marks[s + 1] - marks[s])
        return math.sin(math.pi * min(f, 1.0)) * (0.3 if s == 0 else 1.0)
    bm = bmesh.new()
    layers = []
    for sgn in (-1, 1):
        cen = bm.verts.new((R.x, sgn * 0.005, R.y))
        grid = []
        for b in range(NB):
            row = []
            for k in range(1, NT + 1):
                t = k / NT
                q = R + (bd[b] - R) * t
                row.append(bm.verts.new((q.x, 0.016 * cup(b) * t ** 0.8 + sgn * 0.005 * (1 - 0.72 * t ** 3), q.y)))
            grid.append(row)
        for b in range(NB - 1):
            bm.faces.new((cen, grid[b][0], grid[b + 1][0]))
            for k in range(NT - 1):
                bm.faces.new((grid[b][k], grid[b][k + 1], grid[b + 1][k + 1], grid[b + 1][k]))
        layers.append((cen, grid))
    (cf, gf), (cb, gb) = layers
    for b in range(NB - 1):
        bm.faces.new((gf[b][-1], gb[b][-1], gb[b + 1][-1], gf[b + 1][-1]))
    for b in (0, NB - 1):
        bm.faces.new((cf, gf[b][0], gb[b][0], cb))
        for k in range(NT - 1):
            bm.faces.new((gf[b][k], gf[b][k + 1], gb[b][k + 1], gb[b][k]))
    bmesh.ops.recalc_face_normals(bm, faces=bm.faces)
    mem = D.mesh(bm, name + "_m", TL, "flat", "box", smooth=True, center=(0.15, 0, 0.1))
    parts = [mem]
    # the leading-edge bone follows the outline from the root to the first finger tip
    n0 = marks[1] + 1
    bone_pts = [V((bd[b].x * 0.985 + 0.004, 0.0, bd[b].y * 0.985)) for b in range(0, n0, 2)] + [V((tips[0].x * 0.985, 0, tips[0].y * 0.985))]
    bone_r = [0.027 - 0.0145 * i / (len(bone_pts) - 1) for i in range(len(bone_pts))]
    parts.append(KK.limb(name + "_bone", bone_pts, bone_r, sides=8, color=T, mat=PLUSH, project="box"))
    for i in (1, 2):
        a = V((0.012, 0.0, 0.055))
        b = V((tips[i].x * 0.975, 0.0, tips[i].y * 0.975))
        mid = a.lerp(b, 0.5) + V((0, 0, 0.022 if i < 2 else 0.004))
        rp, par = KK.catmull([a, mid, b], per=3)
        parts.append(KK.limb(f"{name}_rib{i}", rp, [0.023 - 0.012 * t for t in par], sides=8, color=T, mat=PLUSH,
                             project="box"))
    # a little thumb claw on the wrist of the wing
    j = marks[1] - 4
    claw = spike(name + "_claw", 0.042, 0.016, segs=8, color=SPK, mat="flat")
    D.put(claw, V((bd[j].x, 0.0, bd[j].y + 0.012)), V((-0.25, 0.0, 1.0)), up=(0, 1, 0))
    parts.append(claw)
    ob = K.merge(parts, name + "_tmp")
    ob.data.transform(Matrix.Diagonal((WING_SCALE, 1.0, WING_SCALE, 1.0)))
    if sx < 0:
        ob.data.transform(Matrix.Scale(-1, 4, (1, 0, 0)))
        bm = bmesh.new()
        bm.from_mesh(ob.data)
        bmesh.ops.reverse_faces(bm, faces=bm.faces, flip_multires=False)
        bm.to_mesh(ob.data)
        bm.free()
    ob.data.transform(Matrix.Rotation(math.radians(6 * sx), 4, "Y"))    # spread out past the hood
    ob.data.transform(Matrix.Rotation(math.radians(18), 4, "X"))        # tops lean forward: the back catches the light
    ob.data.transform(Matrix.Rotation(math.radians(26 * sx), 4, "Z"))
    ob.location = V(root_pt)
    return D.part([ob], name, root_pt)


# ================================================================== tail (origin at the tail base, ONE mesh)
def tail():
    ctrl = [(0, 0.08, 0.38), (0.0, 0.23, 0.29), (0.01, 0.37, 0.19), (0.04, 0.5, 0.118), (0.09, 0.615, 0.098),
            (0.15, 0.705, 0.118), (0.2, 0.765, 0.17), (0.228, 0.795, 0.24), (0.236, 0.8, 0.29)]
    crad = [0.132, 0.126, 0.112, 0.096, 0.08, 0.065, 0.052, 0.041, 0.032]
    tp, par = KK.catmull(ctrl, per=4)          # 33 rings: the shader bend and the wag stay smooth
    trad = [KK.lerp_list(crad, t) for t in par]
    TS, n = 16, len(tp)

    def paint(i, j):                           # cream underside in plates (side 0 faces up)
        if 4 <= i <= n - 9 and 5 <= j <= 10:
            return (CREAMD, "flat") if i % 3 == 0 else (CREAM, "flat")
        return None
    parts = [D.sweep("tail_tube", tp, trad, sides=TS, color=T, mat=PLUSH, project="sphere", up=(0, 0, 1), paint=paint)]
    for i, t in enumerate((0.2, 0.32, 0.45, 0.57, 0.69, 0.8)):
        j = round(t * (n - 1))
        p = tp[j]
        tan = (tp[j + 1] - tp[j - 1]).normalized()
        upv = V((0, 0, 1))
        nrm = (upv - tan * upv.dot(tan)).normalized()
        s = 1.05 - i * 0.1
        sp = spike(f"tspike{i}", 0.08 * s, 0.04 * s)
        D.put(sp, p + nrm * (trad[j] - 0.008), nrm - tan * 0.3, up=tan, scale=(0.58, 1.0, 1.0))
        parts.append(sp)
    tan = (tp[-1] - tp[-2]).normalized()
    tip = spike("ttip", 0.1, 0.038, segs=12, color=GOLD, mat="flat")
    D.put(tip, tp[-1] - tan * 0.012, tan, up=(0, 1, 0), scale=(0.75, 1.0, 1.0))
    parts.append(tip)
    return D.part(parts, "tail", KK.TAIL_BASE)


# ================================================================== the costume
def build():
    """Returns (parts, children) for KK.finish2()."""
    parts = {"body": body()}
    parts["head"], children = head()
    for s, hip in (("l", KK.HIP_L), ("r", KK.HIP_R)):
        parts["leg_" + s], parts["shin_" + s] = KK.leg2("leg_" + s, hip, T, lambda x, s=s: boot("leg_" + s, x))
    cuff = (TJL, PLUSH)
    parts["arm_l"], parts["forearm_l"] = KK.arm2("arm_l", KK.SHO_L, KK.ELBOW_L, KK.WRIST_L, T, cuff=cuff)
    parts["arm_r"], parts["forearm_r"] = KK.arm2("arm_r", KK.SHO_R, KK.ELBOW_R, KK.WRIST_R, T, cuff=cuff)
    # left: the wand lies along the forearm; the palm is behind and outside it, fingers come round the front
    parts["hand_l"] = KK.grip_hand("hand_l", KK.WRIST_L, KK.WAND_GRIP, KK.WAND_DIR, (0.55, 0.83, 0.0), 0.0155, fat=1.12)
    # right: the bucket handle runs side to side; palm over the front of it, fingers hooked under
    parts["hand_r"] = KK.grip_hand("hand_r", KK.WRIST_R, KK.BUCKET_GRIP, (1, 0, 0), (0.0, -0.45, 0.9), 0.012, fat=1.12)
    parts["wing_l"] = wing("wing_l", KK.WING_L, 1)
    parts["wing_r"] = wing("wing_r", KK.WING_R, -1)
    parts["tail"] = tail()
    return parts, children
