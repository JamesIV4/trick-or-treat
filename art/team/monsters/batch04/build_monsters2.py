"""The monsters as v2 rigs (batch04): a tree of rigid parts under the empty root 'monster_<key>',
built with mon_kit2.Rig. Collision stays code-owned (monster_art.cpp); the art fits it.

blender --background --factory-startup --python art/team/monsters/batch04/build_monsters2.py -- [keys] [--nosheet] [--pieces]
keys so far: skeleton
Or: art\\team\\monsters\\batch04\\go2.ps1 -Keys skeleton   (build, gates, verifier, engine preview)

All positions are written in game coordinates through G = g2b: (x, y_up, z_front), +X the monster's
left. Blob radii are Blender-ordered (x, depth, up).
"""
import math
import pathlib
import sys

sys.path.insert(0, str(pathlib.Path(__file__).resolve().parent))
import mon_kit2 as M  # noqa: E402
from mon_kit2 import D, F, K, V  # noqa: E402
import bmesh  # noqa: E402
import bpy  # noqa: E402


def G(x, y, z):
    """Game point (x, y up, z front) as a Blender vector."""
    return M.g2b((x, y, z))


# ====================================================================== RATTLEBONES
def skeleton():
    K.reset()
    BW, BT, BD, DEEP, INK = "ivory", "ivory_shade", "ivory_deep", "wood", "ink"
    OR, ORD = "pumpkin", "pumpkin_dark"
    R = M.Rig("monster_skeleton", height=1.2, radius=0.3)

    def ball(c, r, name, segs=8, rings=5, color=BW):
        return M.blob(name, c, (r, r, r), segs=segs, rings=rings, color=color)

    def flat(cz, rz):
        """Warp for a blob centred cz above the ground with up-radius rz: the sole sits on y = 0."""
        k = cz / rz
        return lambda c: V((c.x, c.y, c.z if c.z > 0 else c.z * k))

    # ------------------------------------------------------------ pelvis (the part called body)
    body = []
    for s in (-1, 1):
        ring = K.torus(f"ilium{s}", 0.058, 0.032, color=BW, major_seg=12, minor_seg=6)
        K.paint(ring, BT, where=lambda c, n: n.dot(V((c.x, c.y, 0)).normalized()) < -0.35)   # inside of the hole
        M.put(ring, G(s * 0.076, 0.507, -0.004), V((s * 0.38, -1.0, 0.28)), scale=(1.0, 1.12, 1.0))
        body.append(ring)
        body.append(M.blob(f"hipcup{s}", G(s * 0.09, 0.462, 0.0), (0.054, 0.046, 0.042), segs=8, rings=5, color=BW))
    body.append(M.blob("sacrum", G(0, 0.505, -0.03), (0.05, 0.036, 0.075), segs=8, rings=6, color=BW,
                       warp=lambda c: V((c.x * (1.0 - 0.55 * max(0.0, -c.z)), c.y, c.z))))
    body.append(M.blob("pubis", G(0, 0.452, 0.028), (0.045, 0.03, 0.03), segs=8, rings=5, color=BW))
    R.part("body", body, G(0, 0.47, 0))

    # ------------------------------------------------------------ chest: spine, ribcage, collar, neck
    chest = []
    for k, y in enumerate((0.546, 0.581, 0.615)):
        chest.append(M.blob(f"lumbar{k}", G(0, y, -0.016 - 0.01 * k), (0.036, 0.032, 0.023), segs=7, rings=4, color=BW))
    chest.append(M.tube("spine", [G(0, 0.617, -0.048), G(0, 0.694, -0.058), G(0, 0.772, -0.04)], 0.024, sides=7,
                        color=BT, mat="flat"))
    for k, (y, rx) in enumerate(((0.632, 0.108), (0.688, 0.128), (0.744, 0.118))):
        for s in (-1, 1):
            pts, rr = [], []
            for i in range(7):
                t = i / 6
                a = math.radians(8 + 158 * t)   # from the spine round the side to the sternum
                pts.append(G(s * rx * math.sin(a), y + 0.012 - 0.046 * t ** 1.3, 0.016 - 0.076 * math.cos(a)))
                rr.append(0.0215 - 0.003 * t)
            chest.append(M.tube(f"rib{k}{s}", pts, rr, sides=5, color=BW, mat="flat", cap_offset=0.5))
    chest.append(M.blob("sternum", G(0, 0.688, 0.094), (0.032, 0.02, 0.078), segs=8, rings=6, color=BW))
    for s in (-1, 1):
        chest.append(M.limb(f"collar{s}", G(s * 0.02, 0.768, 0.068), G(s * 0.175, 0.772, 0.0), 0.017, flare_a=1.2,
                            flare_b=1.5, sides=6, color=BW))
        chest.append(M.blob(f"shcup{s}", G(s * 0.172, 0.766, 0.0), (0.042, 0.042, 0.042), segs=7, rings=4, color=BW))
        chest.append(M.blob(f"scap{s}", G(s * 0.088, 0.711, -0.078), (0.048, 0.016, 0.056), segs=7, rings=4, color=BW))
    for k, y in enumerate((0.787, 0.816)):
        chest.append(M.blob(f"neck{k}", G(0, y, -0.012 + 0.006 * k), (0.032, 0.03, 0.022), segs=7, rings=4, color=BW))
    R.part("chest", chest, G(0, 0.535, 0), parent="body")

    # ------------------------------------------------------------ legs (straight down at rest: IK chains)
    for side, s in (("l", 1), ("r", -1)):
        x = s * 0.09
        hip, knee, ankle = G(x, 0.445, 0), G(x, 0.26, 0), G(x, 0.075, 0)
        thigh = [M.limb(f"thigh{s}", hip, knee, 0.036, flare_a=1.2, flare_b=1.25, sides=7, color=BW)]
        for k in (-1, 1):
            thigh.append(ball(knee + V((k * 0.031, 0, 0.004)), 0.04, f"cond{s}{k}", segs=7, rings=4))
        R.part(f"leg_{side}", thigh, hip, parent="body", ball=0.056, ball_color=BW)
        R.part(f"shin_{side}", [M.limb(f"tib{s}", knee, ankle, 0.033, flare_a=1.2, flare_b=1.35, sides=7, color=BW)], knee,
               parent=f"leg_{side}", ball=0.049, ball_color=BW)
        foot = [M.blob(f"sole{s}", G(x, 0.034, 0.045), (0.068, 0.092, 0.048), segs=10, rings=6, color=BW,
                       warp=flat(0.034, 0.048))]
        for k, (dx, dz, r) in enumerate(((-0.047, 0.107, 0.033), (0.0, 0.13, 0.036), (0.047, 0.107, 0.033))):
            foot.append(M.blob(f"toe{s}{k}", G(x + dx, 0.03, dz), (r, r * 1.25, 0.034), segs=7, rings=4, color=BW,
                               warp=flat(0.03, 0.034)))
        foot.append(M.blob(f"heel{s}", G(x, 0.04, -0.035), (0.047, 0.038, 0.04), segs=7, rings=4, color=BW,
                           warp=flat(0.04, 0.04)))
        R.part(f"foot_{side}", foot, ankle, parent=f"shin_{side}", ball=0.044, ball_color=BW, ball_segs=7)
        R.ik_chain(f"leg_{side}", f"shin_{side}", f"foot_{side}")

    # ------------------------------------------------------------ arms (straight down at rest: IK chains)
    for side, s in (("l", 1), ("r", -1)):
        x = s * 0.198
        sh, el, wr = G(x, 0.763, 0), G(x, 0.593, 0), G(x, 0.433, 0)
        upper = [M.limb(f"hum{s}", sh, el, 0.029, flare_a=1.2, flare_b=1.25, sides=7, color=BW)]
        for k in (-1, 1):
            upper.append(ball(el + V((k * 0.025, 0, 0.003)), 0.033, f"econd{s}{k}", segs=7, rings=4))
        R.part(f"arm_{side}", upper, sh, parent="chest", ball=0.051, ball_color=BW, ground=False)
        R.part(f"forearm_{side}", [M.limb(f"rad{s}", el, wr, 0.027, flare_a=1.2, flare_b=1.3, sides=7, color=BW)], el,
               parent=f"arm_{side}", ball=0.04, ball_color=BW, ball_segs=7, ground=False)
        if side == "l":   # open mitten hand, fingers spread: the one that points at the player
            hand = [M.blob("palm_l", G(x, 0.388, 0.004), (0.023, 0.042, 0.046), segs=8, rings=5, color=BW)]
            for k, (dz, ang, ln) in enumerate(((-0.03, -20, 0.046), (-0.01, -7, 0.055), (0.011, 6, 0.052), (0.031, 19, 0.042))):
                a = G(x, 0.365, dz)
                b = a + G(0, -math.cos(math.radians(ang)), math.sin(math.radians(ang))) * ln
                hand.append(M.tube(f"fing_l{k}", [a, a.lerp(b, 0.5), b], [0.014, 0.0145, 0.012], sides=6, color=BW,
                                   mat="flat", cap_offset=0.9))
            a = G(x, 0.395, 0.036)
            b = a + G(0, -0.55, 0.83).normalized() * 0.042
            hand.append(M.tube("thumb_l", [a, a.lerp(b, 0.5), b], [0.0155, 0.016, 0.0135], sides=6, color=BW, mat="flat",
                               cap_offset=0.9))
        else:             # fist round the bone
            gc = G(x, 0.376, 0.012)   # grip centre = the bone's pivot
            hand = [M.blob("palm_r", G(x, 0.400, 0.006), (0.027, 0.042, 0.035), segs=8, rings=5, color=BW)]
            for k, dz in enumerate((-0.026, 0.0, 0.026)):
                loop = K.torus(f"grip{k}", 0.033, 0.015, color=BW, major_seg=8, minor_seg=4)
                M.put(loop, gc + G(0, 0, dz), G(0, 0, 1))
                hand.append(loop)
            hand.append(M.blob("thumb_r", gc + G(0, 0.014, 0.046), (0.017, 0.022, 0.016), segs=7, rings=4, color=BW))
        R.part(f"hand_{side}", hand, wr, parent=f"forearm_{side}", ball=0.034, ball_color=BW, ball_segs=7, ground=False)
        R.ik_chain(f"arm_{side}", f"forearm_{side}", f"hand_{side}")
    held = M.bone("held", gc + G(0, 0, -0.105), gc + G(0, 0, 0.225), 0.026, color=BW, mat="flat", knob=1.7, sides=7,
                  knob_segs=7, knob_rings=5)
    R.part("bone", [held], gc, parent="hand_r", ground=False)
    R.socket("muzzle", gc + G(0, 0, 0.06), "hand_r")   # the middle of the held bone: where a throw leaves from

    # ------------------------------------------------------------ skull (head) and mandible (jaw)
    # A cute cartoon SKULL, not a ball (the user read the v2 ball head as a robot): a domed cranium with
    # brow ridges over big deep sockets, cheekbones, temples pinched in a little, the face narrowing
    # below the cheekbones to an upper jaw whose flat underside (the palate) carries a row of blocky
    # teeth; a separate mandible with a chin and its own teeth. Matte, chalky ivory (the `stucco`
    # layer, fine) with baked shade cells in the sockets, under the brows and cheekbones and along the
    # jaw; the eyes are a soft teal light inside each dark socket (M.socket_eye), not lenses.
    CY, CZ = 1.06, 0.012            # cranium centre (game y, z)
    SC = G(0, CY, CZ)
    T_UP = 0.926                     # game height of the palate edge: the upper teeth hang from it
    TOOTH_H, TOOTH_D = 0.027, 0.022  # tooth height, depth
    J_TOP, J_BOT = T_UP - 2 * TOOTH_H, 0.8     # mandible: alveolar rim at the front, chin bottom
    SKULL_MAT, SKULL_UV = "stucco", 3.0
    TEETH, GUM = "linen", "ivory_shade"   # (polish) ivory teeth (bone_shade on the flat layer still read white), a darker root: a hint of gum shadow

    def gauss(da, de, wa, we):
        return math.exp(-(da / wa) ** 2 - (de / we) ** 2)

    def smooth01(t):
        t = max(0.0, min(1.0, t))
        return t * t * (3.0 - 2.0 * t)

    def table(t, ts, vs):
        for i in range(1, len(ts)):
            if t <= ts[i] or i == len(ts) - 1:
                f = max(0.0, min(1.0, (t - ts[i - 1]) / (ts[i] - ts[i - 1])))
                return vs[i - 1] + (vs[i] - vs[i - 1]) * f
    # The lower half, by ring (t = 0 at the equator .. 1 at the bottom pole): height below the centre,
    # half width, front and back depth. The palate edge is at t = 0.62; below it the rings close flat.
    FLOOR = T_UP - CY
    LOW_T = (0.0, 0.15, 0.3, 0.45, 0.56, 0.64, 1.0)
    LOW_H = (0.0, -0.036, -0.074, -0.106, -0.126, FLOOR, FLOOR)
    LOW_W = (0.272, 0.264, 0.234, 0.2, 0.178, 0.164, 0.0)
    LOW_F = (0.262, 0.262, 0.256, 0.246, 0.234, 0.214, 0.0)
    LOW_B = (0.25, 0.24, 0.215, 0.175, 0.138, 0.11, 0.0)

    def skull_shape(c):
        """Unit direction (Blender) -> the skull point relative to SC (Blender, metres)."""
        X, Y, Z = c.x, c.z, -c.y   # game axes: X left, Y up, Z front
        ln = math.sqrt(X * X + Y * Y + Z * Z) or 1.0
        X, Y, Z = X / ln, Y / ln, Z / ln
        az = math.degrees(math.atan2(X, Z))
        el = math.degrees(math.asin(max(-1.0, min(1.0, Y))))
        ca = math.sqrt(X * X + Z * Z)
        sa, cz = (X / ca, Z / ca) if ca > 1e-9 else (0.0, 1.0)
        if Y >= 0.0:   # the cranium: a dome
            k = math.cos(math.radians(el))
            h, w, f, bk = 0.236 * math.sin(math.radians(el)), 0.272 * k, 0.262 * k, 0.25 * k
        else:          # the face: ring by ring down to the flat palate, narrowing below the cheekbones
            t = -el / 90.0
            h = table(t, LOW_T, LOW_H)
            w, f, bk = table(t, LOW_T, LOW_W), table(t, LOW_T, LOW_F), table(t, LOW_T, LOW_B)
        # a flat-ish face: the front half of each ring is fuller than an ellipse
        z = f * math.copysign(abs(cz) ** 0.8, cz) if cz > 0 else bk * cz
        x = w * sa
        yy = CY + h
        b = 0.0
        for sg in (-1.0, 1.0):
            b += 0.03 * gauss(az - sg * 30, yy - 1.188, 24, 0.022)    # brow ridge over each socket
            b += 0.036 * gauss(az - sg * 52, yy - 1.0, 13, 0.024)     # cheekbone
            b += 0.012 * gauss(az - sg * 74, yy - 1.0, 18, 0.012)     # its arch back toward the ear
            b -= 0.022 * gauss(az - sg * 86, yy - 1.11, 16, 0.045)    # temple
            b -= 0.012 * gauss(az - sg * 46, yy - 0.955, 13, 0.016)   # hollow under the cheekbone
        d = V((sa, 0.25 * Y, cz)).normalized()
        p = V((x, h, z)) + d * b
        return V((p.x, -p.z, p.y))

    def sdir(a, e):
        a, e = math.radians(a), math.radians(e)
        return V((math.sin(a) * math.cos(e), -math.cos(a) * math.cos(e), math.sin(e)))

    def spt(a, e):
        """Point on the skull at azimuth a (0 = front, + toward the monster's left) and elevation e, degrees."""
        return SC + skull_shape(sdir(a, e))

    def snrm(a, e, h=0.5):
        return (spt(a + h, e) - spt(a - h, e)).cross(spt(a, e + h) - spt(a, e - h)).normalized()

    def son(a, e, off):
        return spt(a, e) + snrm(a, e) * off

    skull = M.blob("skull", SC, (1.0, 1.0, 1.0), segs=26, rings=18, color=BW, mat=SKULL_MAT, warp=skull_shape)
    M.bake(skull)
    tree = M.bvh(skull)

    def socket_outline(cx, cz, w, h, tilt, s, n=18):
        """A friendly skull socket: taller than wide, fuller at the top, the inner-lower side squared off
        toward the nose, the top leaning outward."""
        pts = []
        inner_low = math.radians(225.0 if s > 0 else 315.0)
        for i in range(n):
            t = math.tau * i / n
            k = 1.0 - 0.1 * max(0.0, math.cos(t - inner_low)) ** 2
            x = math.cos(t) * w * (1.0 + 0.07 * math.sin(t)) * k
            z = math.sin(t) * h * k
            pts.append((x, z))
        c, sn = math.cos(math.radians(tilt)), math.sin(math.radians(tilt))
        return [(cx + x * c - z * sn, cz + x * sn + z * c) for x, z in pts]

    EX, EZ, SW, SH, TILT = 0.112, 1.09, 0.072, 0.092, 9.0
    DEPTH, INSET = 0.042, 0.012
    feats, sockets = [], []
    for s in (-1, 1):
        feats.append({"outline": socket_outline(s * EX, EZ, SW, SH, -s * TILT, s), "color": INK, "wall": INK,
                      "rim": BT, "depth": DEPTH, "inset": INSET, "rim_frac": 0.3, "rim_w": 0.005})
        loc, nrm = M.surf(tree, (s * EX, -3.0, EZ), (0, 1, 0))
        sockets.append((s, loc.copy(), nrm.copy()))
    feats.append({"outline": M.heart(0.0, 0.99, 0.07, 0.062, n=16, point_up=True), "color": INK, "wall": INK,
                  "rim": BT, "depth": 0.032, "inset": 0.0012, "rim_frac": 0.15, "rim_w": 0.0006})
    F.carve(skull, feats, depth=DEPTH, inset=INSET, rim_color=BT, wall_glow=INK, floor_color=INK)
    F.scale_uv(skull, SKULL_UV)
    head = [skull]

    # Skull sutures, so the back of the head is not a featureless dome: the coronal seam ear to ear
    # over the top, the sagittal seam back along the midline, the lambdoid seam as an arch across the
    # back, and two hairline cracks (brow and temple). Dark zig-zag hairlines half sunk in the bone.
    def sdir_pt(d, off=0.0):
        d = V(d).normalized()
        return son(math.degrees(math.atan2(d.x, -d.y)), math.degrees(math.asin(max(-1.0, min(1.0, d.z)))), off)

    def suture(name, dirs, r=0.0042, zig=0.011):
        pts = []
        for i, d in enumerate(dirs):
            d = V(d).normalized()
            if 0 < i < len(dirs) - 1:   # zig-zag across the run
                run = (V(dirs[i + 1]) - V(dirs[i - 1])).normalized()
                d = (d + run.cross(d).normalized() * (zig if i % 2 else -zig)).normalized()
            pts.append(sdir_pt(d, 0.0008))
        return M.tube(name, pts, r, sides=3, color=BD, mat="flat")

    def arc(fn, a0, a1, n):
        return [fn(math.radians(a0 + (a1 - a0) * i / (n - 1))) for i in range(n)]
    TILT_C = math.radians(10)   # the coronal seam leans a little forward of the crown
    head.append(suture("coronal", arc(lambda t: (math.sin(t), -math.sin(TILT_C) * math.cos(t), math.cos(TILT_C) * math.cos(t)),
                                       -80, 80, 13)))
    head.append(suture("sagittal", arc(lambda t: (0.0, math.sin(t), math.cos(t)), -10, 52, 8)))
    lam = [sdir(a, e) for a, e in ((116, -6), (124, 6), (134, 17), (146, 26), (160, 33), (180, 38), (-160, 33), (-146, 26),
                                   (-134, 17), (-124, 6), (-116, -6))]
    head.append(suture("lambdoid", lam))
    head.append(suture("crack", [sdir(a, e) for a, e in ((-50, 60), (-57, 52), (-52, 45), (-60, 39), (-56, 33))], r=0.0034,
                       zig=0.006))
    head.append(suture("crack2", [sdir(a, e) for a, e in ((74, 34), (80, 28), (77, 22), (84, 17))], r=0.003, zig=0.005))

    # The palate edge, measured on the skull: where a horizontal ray toward the axis meets it just above
    # the flat underside. The teeth hang from it and the mandible follows it.
    def edge(a, y=T_UP + 0.006):
        d = V((math.sin(math.radians(a)), -math.cos(math.radians(a)), 0.0))
        for k in range(12):   # the sides and back are rounded up off the floor: look a little higher there
            o = V((SC.x, SC.y, y + 0.006 * k)) + d * 1.0
            loc, _ = M.surf(tree, o, -d)
            if loc is not None:
                return V((loc.x, loc.y, y))
        raise RuntimeError(f"palate edge ray missed at azimuth {a}")

    # (polish) Teeth: smaller, rounded, a little uneven (size, height, lean), ivory, each with a darker ring at
    # its root where it is set into the bone (a hint of gum shadow); the upper row hangs 4 mm over the lower.
    # (They were plain white square blocks: piano keys in the face beat.) A tooth is a 4-sided lathe turned
    # 45 degrees (a square section) with its two pole tips flattened: a rounded block in 16 triangles.
    TEETH_A = (-49.0, -35.0, -21.0, -7.0, 7.0, 21.0, 35.0, 49.0)
    ROOT_IN = 0.003                  # how far a tooth's root sits inside the bone (the root cap shows below it: the gum shadow)

    def tooth_warp(c):
        x, y = (c.x - c.y) / math.sqrt(2.0), (c.x + c.y) / math.sqrt(2.0)
        k = math.sqrt(2.0) / 0.866
        return V((x * k, y * k, math.copysign(abs(c.z) ** 0.6, c.z)))

    def rounded_tooth(name, w, h, d, root_up):
        ob = M.blob(name, (0, 0, 0), (w / 2, d / 2, h / 2), segs=4, rings=3, color=TEETH, mat=SKULL_MAT, warp=tooth_warp,
                    rot=M.Matrix.Rotation(math.radians(-90), 3, "X"))   # poles (root, tip) along local Y
        M.bake(ob)
        K.paint(ob, GUM, where=lambda c, nr: (c.y if root_up else -c.y) > h * 0.3)
        return ob

    def jit(k, a, b):
        return math.sin(a * k + b)
    for k, a in enumerate(TEETH_A):
        p = edge(a)
        out = V((p.x - SC.x, p.y - SC.y, 0.0)).normalized()
        tw = (0.025 if abs(a) < 30 else 0.022) * (1.0 + 0.09 * jit(k, 2.3, 0.4))
        th = 0.034 * (1.0 + 0.07 * jit(k, 1.7, 1.1))
        top = T_UP + 0.007 + 0.0015 * jit(k, 3.1, 0.0)   # the root cap up inside the palate (seen from above it read as a brown cube top); the AO under the palate is the shadow
        tooth = rounded_tooth(f"tooth_u{k}", tw, th, 0.02, True)
        M.put(tooth, V((p.x, p.y, top - th / 2)) - out * 0.012, out, roll=4.0 * jit(k, 2.9, 0.3))   # root tucked under the edge
        head.append(tooth)
    # (polish) The back of the mouth: a dark wall from the palate down between the rami, so from low behind
    # you no longer see into the mouth (the backs of the teeth, the throat). Ink on the mouth side, the
    # skull base's dark brown behind. It also gives the gaps between the teeth their dark (the old throat
    # blob is gone). Plus the palate's front underside painted ink.
    def mouth_back(u, v):
        x = (1.0 - u * 2.0) * 0.14                 # run right to left so the painted (outer) side faces the mouth
        return G(x, 0.945 - 0.095 * v, 0.032 - 0.6 * x * x)   # between the rami, hidden behind them from the side
    head.append(M.shell("mouthback", mouth_back, 4, 2, thick=0.006, color=BD, mat="flat",
                        paint=lambda i, j: (INK, "flat")))
    K.paint(skull, INK, where=lambda c, nr: nr.z < -0.75 and c.z < T_UP + 0.004 and -(c.y - SC.y) > 0.02)
    R.part("head", head, G(0, 0.825, 0), parent="chest", ground=False)

    # Mandible: a horseshoe of bone under the palate edge (pulled in a little: an overbite), the chin
    # pushed forward and down, the rami rising at the back into the skull toward the hinge.
    AJ = 94.0

    def jaw_ring(a):
        e = edge(max(-AJ, min(AJ, a)))
        r = V((e.x - SC.x, e.y - SC.y, 0.0))
        return r.length, r.normalized()

    def jaw_outer(u, v):
        a = (u * 2 - 1) * AJ
        r, d = jaw_ring(a)
        fa = min(1.0, abs(a) / AJ)
        top = J_TOP + 0.09 * smooth01((fa - 0.55) / 0.45)                 # the ramus rises at the back
        bot = J_BOT + 0.05 * smooth01((fa - 0.25) / 0.6) + 0.012 * fa     # the jaw line climbs to the angle
        y = top + (bot - top) * v
        rr = r - 0.005 - 0.014 * v ** 1.4                                   # tucks in toward the bottom
        rr += 0.008 * math.sin(math.pi * v)                                 # a little fullness
        chin = math.exp(-(a / 22.0) ** 2)
        rr += chin * (0.009 * smooth01((v - 0.35) / 0.5))                  # the chin juts forward
        y -= chin * 0.008 * smooth01((v - 0.6) / 0.4)                       # ... and down
        p = V((SC.x, SC.y, y)) + d * rr
        return p

    def jaw_inner(u, v):
        a = (u * 2 - 1) * AJ
        p = jaw_outer(u, v)
        d = V((p.x - SC.x, p.y - SC.y, 0.0))
        th = 0.03 if abs(a) < 70 else 0.022
        return p - d.normalized() * th + V((0, 0, 0.006 * v))

    mand = M.shell("mandible", jaw_outer, 16, 5, inner=jaw_inner, color=BW, mat=SKULL_MAT)
    F.scale_uv(mand, SKULL_UV)
    # the inside of the horseshoe (seen when the jaw opens) is the dark of the mouth
    K.paint(mand, BD, where=lambda c, nr: nr.dot(V((c.x - SC.x, c.y - SC.y, 0.0)).normalized()) < -0.35 and c.z < J_TOP - 0.004 and c.y < SC.y - 0.04)
    jaw = [mand]
    # (polish) a dark floor inside the horseshoe (the old throat blob filled the mouth; with it gone an open
    # jaw showed the bandana through the gap): ink, facing up, from the inner rim back to the mouth's back wall
    def mouth_floor(u, v):
        a = (u * 2.0 - 1.0) * 80.0
        q = jaw_inner((a / AJ + 1.0) / 2.0, 0.2)
        back = G(q.x * 0.8, 0.866, 0.028)   # (Blender x is game x); meets the back wall (bottom 0.85)
        return q.lerp(back, v) + V((0, 0, -0.004))
    jaw.append(M.patch("mouthfloor", mouth_floor, 4, 2, color=INK, mat="flat"))
    for k, a in enumerate(TEETH_A):
        p = edge(a)
        out = V((p.x - SC.x, p.y - SC.y, 0.0)).normalized()
        tw = (0.023 if abs(a) < 30 else 0.02) * (1.0 + 0.1 * jit(k, 1.9, 2.0))
        th = 0.034 * (1.0 + 0.06 * jit(k, 2.6, 0.7))
        tip = T_UP - 0.023 + 0.0015 * jit(k, 2.2, 1.3)     # 4 mm up behind the upper row's tips
        tooth = rounded_tooth(f"tooth_l{k}", tw, th, 0.018, False)
        M.put(tooth, V((p.x, p.y, tip - th / 2)) - out * 0.0165, out, roll=-4.0 * jit(k, 2.1, 0.9))
        jaw.append(tooth)
    R.part("jaw", jaw, G(0, 0.935, 0.02), parent="head", ground=False)

    # Eyes: the dark of each socket with a soft teal light floating inside (M.socket_eye), no lens.
    for s, loc, nrm in sockets:
        side = "l" if s > 0 else "r"
        c = loc + V((0, 0.012, 0))
        M.socket_eye(R, f"eye_{side}", f"pupil_{side}", c, nrm, SW - INSET - 0.004, SH - INSET - 0.004, DEPTH - 0.014,
                     parent="head", roll=-s * TILT, back=INK, light="glow_magic_teal", light_r=0.42, light_tall=1.2,
                     light_back=0.35, look=(-s * 0.1, -0.08), light_segs=7, light_rings=4)

    # ------------------------------------------------------------ bandana: neck ring, knot and a soft front flap
    PIN = G(0, 0.808, 0.058)

    def flap(u, v):
        x = 1.0 - u * 2.0
        w = 0.105 * (1.0 - v) ** 0.8 + 0.003
        z = 0.075 + 0.05 * math.sin(min(1.0, v * 1.25) * math.pi / 2) - 0.072 * (x * w / 0.105) ** 2
        return G(x * w, 0.811 - 0.15 * v, z)
    cloth = [M.shell("flap", flap, 6, 7, thick=0.007, color=OR, rim_color=ORD),
             K.torus("bring", 0.058, 0.021, color=OR, major_seg=12, minor_seg=5, loc=G(0, 0.806, -0.004),
                     scale=(1.05, 1.0, 0.85)),
             M.blob("knot", G(0, 0.806, -0.082), (0.03, 0.024, 0.026), segs=7, rings=4, color=ORD)]
    for k, (u, v) in enumerate(((0.5, 0.2), (0.27, 0.12), (0.73, 0.12), (0.38, 0.42), (0.62, 0.42))):
        p = flap(u, v)
        n = (flap(u + 0.02, v) - flap(u - 0.02, v)).cross(flap(u, v + 0.02) - flap(u, v - 0.02)).normalized()
        dot = K.cylinder(f"dot{k}", 0.011, 0.003, color="cream", verts=6)
        M.put(dot, p + n * 0.0008, n)
        cloth.append(dot)
    R.part("bandana", cloth, PIN, parent="chest", soft=dict(dir=G(0, -1, 0.22), rigid=0.012, loops=6), ground=False)
    KN = G(0, 0.806, -0.088)
    tails = []
    for k, (sx, drop, out) in enumerate(((-1, 0.22, 0.115), (1, 0.17, 0.07))):
        pts = [KN, KN + G(sx * out * 0.25, -0.03, -0.038), KN + G(sx * out * 0.62, -drop * 0.5, -0.064),
               KN + G(sx * out, -drop, -0.078)]
        tails.append(M.ribbon(f"tail{k}", pts, lambda t: 0.052 - 0.014 * t, thick=0.006, nv=8, nu=2, color=OR,
                              up=G(0, 0.3, -1)))
    R.part("bandana_tail", tails, KN, parent="chest", soft=dict(dir=None, loops=6), ground=False)

    # ------------------------------------------------------------ extreme poses for the review sheet
    poses = [
        ("rest", {}),
        ("T, bent, look", {"arm_l": [("z", 90)], "forearm_l": [("x", -115)], "arm_r": [("z", -90)], "forearm_r": [("x", -115)],
                           "leg_l": [("x", -80)], "shin_l": [("x", 100)], "leg_r": [("x", 45)], "shin_r": [("x", 60)],
                           "head": [("y", 60), ("x", -25)], "chest": [("y", -35)], "jaw": [("x", 22)]}),
        ("wind-up", {"arm_r": [("x", 165)], "forearm_r": [("x", -95)], "hand_r": [("x", -40)], "arm_l": [("x", -85)],
                     "chest": [("y", -35), ("x", -12)], "head": [("y", 35)], "leg_l": [("z", 18)], "leg_r": [("z", -18)]}),
        ("crouch", {"body": [("move", (0, -0.13, 0))], "leg_l": [("z", 32), ("x", -75)], "shin_l": [("x", 125)],
                    "foot_l": [("x", -50)], "leg_r": [("z", -32), ("x", -75)], "shin_r": [("x", 125)],
                    "foot_r": [("x", -50)], "chest": [("x", 28)], "head": [("x", -30)],
                    "arm_l": [("z", 35), ("x", 40)], "arm_r": [("z", -35), ("x", 40)]}),
        ("hit, skull hop", {"chest": [("x", -30)], "head": [("move", (0, 0.08, 0)), ("x", -22), ("z", 14)], "jaw": [("x", 25)],
                            "arm_l": [("z", 125)], "arm_r": [("z", -125)], "forearm_l": [("x", -40)],
                            "foot_l": [("x", 40)], "leg_r": [("x", -35)], "shin_r": [("x", 70)]}),
        ("splits, twist", {"leg_l": [("z", 72)], "leg_r": [("z", -72)], "foot_l": [("z", -60)], "foot_r": [("z", 60)],
                           "body": [("move", (0, -0.3, 0))], "chest": [("y", 50)], "head": [("y", -60), ("z", 20)],
                           "arm_l": [("x", -150)], "arm_r": [("x", 60), ("z", -30)], "hand_l": [("x", 70)]}),
    ]
    # (polish) The body bones in the skull's matte chalky finish (the `stucco` layer at the skull's UV scale):
    # the body had kept the M2 smooth sheen, so head and body read as two materials. Finish only: the
    # geometry is unchanged. The animator gives them the skull's matte gloss as well.
    stucco, flat_ = K._layer(SKULL_MAT), K._layer("flat")
    bone_cells = [K._uv_cell(c) for c in (BW, BT, BD)]
    for n, prt in R.parts.items():
        if n in ("head", "jaw", "bandana", "bandana_tail") or n.startswith(("eye_", "pupil_")) or prt.ob.type != "MESH":
            continue
        tint = prt.ob.data.uv_layers["Tint"]
        for d in tint.data:
            u, v = d.uv
            layer = math.floor(u + 1e-6)
            if layer == flat_ and any(abs((u - layer) - cu) < 1e-3 and abs(v - cv) < 1e-3 for cu, cv in bone_cells):
                d.uv = (u - layer + stucco, v)
        F.scale_uv(prt.ob, SKULL_UV)
    # Baked two-tone shading on the bone: undersides and deep creases drop to the shade cell. Not on
    # the skull and jaw: their faces are large, and a shade cell there is a hard-edged rectangle
    # (bumps under the skull were tried too and read as ears; the sutures carry the back instead).
    R.shade({BW: BT}, ao_cut=0.5, down=-0.62, parts=[n for n in R.parts if n not in ("head", "jaw")])
    # The skull is no longer one big smooth ball: its faces are small enough now for shade cells to
    # follow the forms (under the brow ridges and cheekbones, round the sockets, along the jaw).
    R.shade({BW: BT}, ao_cut=0.5, down=-1.5, parts=["head", "jaw"])   # creases only: a downward-facing patch was a hard rectangle
    return R.finish(ao_radius=0.2, ao_strength=0.9, poses=poses, closeups=[("face", 2.4, tuple(G(0, 1.04, 0.05))),
                              ("skull", 2.4, tuple(G(0, 1.04, 0.0)), ((0, 0), (90, 0), (180, 0), (30, -20)))])


# ====================================================================== MUMMY
def mummy():
    """A chunky toy mummy: every body part is a dark under-wrap blob with padded bandage strips laid
    over it in three tones and three heights (M.strap), so the wraps really overlap and cross. One
    big amber eye in a dark gap between the wraps, a squinting second one in a slit beside it, a head
    knot with two tails, a waist knot with a tail, a loose strip on the left wrist."""
    K.reset()
    LL, LM, LS, LD, DK = "linen_light", "linen", "linen_shade", "linen_deep", "linen_dark"
    MAT, UVS = "burlap", 5.5                 # the weave, scaled down to gauze
    TONE = ((LS, LD, 0.006), (LM, LS, 0.009), (LL, LS, 0.012))   # layer -> (top, edge, height)
    UP, DOWN = V((0, 0, 1)), V((0, 0, -1))
    R = M.Rig("monster_mummy", height=1.4, radius=0.32)

    def hd(az, el):
        """Blender unit direction at azimuth az (0 = front, + toward the monster's left) and elevation el."""
        a, e = math.radians(az), math.radians(el)
        return V((math.sin(a) * math.cos(e), -math.cos(a) * math.cos(e), math.sin(e)))

    def on_surf(tree, centre, d):
        loc, nrm, _, _ = tree.ray_cast(V(centre) + V(d).normalized() * 4.0, -V(d).normalized(), 8.0)
        return loc, nrm

    def strips(prefix, tree, centre, ref_r, specs, n=16):
        """specs: (lat, width, pitch, roll, layer[, (a0, a1)[, yaw[, wander]]]) -> latitude bands."""
        out = []
        for k, sp in enumerate(specs):
            lat, w, pitch, roll, layer = sp[:5]
            a0, a1 = sp[5] if len(sp) > 5 and sp[5] else (0.0, 360.0)
            yaw = sp[6] if len(sp) > 6 else 0.0
            amp = sp[7] if len(sp) > 7 else 1.3
            col, edge, h = TONE[layer]
            closed = a1 - a0 >= 359.9
            seg = n if closed else max(4, round(n * (a1 - a0) / 360.0))
            on = M.band_fn(tree, centre, M.axis_tilt(pitch, roll, yaw), lat, ref_r, a0, a1,
                           wave=lambda th, k=k, amp=amp: amp * math.sin(2.0 * th + k * 1.9))
            out.append(M.strap(f"{prefix}{k}", on, seg, w / 2, height=h, closed=closed, color=col, edge_color=edge, mat=MAT,
                               uv_scale=UVS, fade=0 if closed else 2))
        return out

    def diag(name, tree, centre, ref_r, p0, p1, w, layer, n=8, bow=0.0, fade=2):
        col, edge, h = TONE[layer]
        return M.strap(name, M.arc_fn(tree, centre, hd(*p0), hd(*p1), ref_r, bow=bow), n, w / 2, height=h, color=col,
                       edge_color=edge, mat=MAT, uv_scale=UVS, fade=fade)

    def limb(prefix, a, b, r0, r1, specs, n=8, ball=0.0):
        """A wrapped limb segment from a to b: a thin core and slanted ring strips.
        specs: (at, width, tilt, layer, phase). ball = radius of the ball joint at a: strips near
        that end ride over the ball instead of cutting through it (at may be 0 or a little negative)."""
        length = (V(b) - V(a)).length

        def rf(u):
            r = r0 + (r1 - r0) * min(max(u, 0.0), 1.0)
            return max(r, math.sqrt(max(ball * ball - (u * length) ** 2, 0.0)) + 0.002) if ball else r
        out = [M.tube(prefix + "_core", [a, b], [r0 * 0.88, r1 * 0.88], sides=n, color=LS, mat="flat", cap_start=False,
                      cap_offset=0.45)]
        for k, (at, w, tilt, layer, ph) in enumerate(specs):
            col, edge, h = TONE[layer]
            out.append(M.strap(f"{prefix}_w{k}", M.ring_fn(a, b, rf, at, tilt=tilt, phase=ph, ref=(1, 0, 0)), n, w / 2, height=h,
                               closed=True, color=col, edge_color=edge, mat=MAT, uv_scale=UVS))
        return out

    def streamer(name, pts, w0, w1, face, nv=8, nu=3, thick=0.007, color=LL, rim=LS, fray=0.045):
        """A loose bandage end: one continuous ribbon along pts, widening from w0 to w1, its flat
        side facing `face`, the free end cut in a ragged zig-zag."""
        path = M.catmull(pts, 4)
        lens = [0.0]
        for p, q in zip(path[:-1], path[1:]):
            lens.append(lens[-1] + (q - p).length)
        total = lens[-1]
        zig = (0.0, -1.0, -0.2, -0.85, 0.0)

        def at(v):
            s = min(max(v, 0.0), 1.0) * total
            for k in range(1, len(path)):
                if s <= lens[k] or k == len(path) - 1:
                    f = (s - lens[k - 1]) / max(lens[k] - lens[k - 1], 1e-9)
                    return path[k - 1].lerp(path[k], min(max(f, 0.0), 1.0)), (path[k] - path[k - 1]).normalized()

        def outer(u, v):
            vv = v
            if v > 1.0 - 0.5 / nv:   # the end row
                vv = 1.0 + zig[round(u * nu) % len(zig)] * fray / total
            p, t = at(vv)
            side = t.cross(V(face))
            side = side.normalized() if side.length > 1e-6 else V((1, 0, 0))
            w = w0 + (w1 - w0) * v
            x = (u - 0.5) * 2.0
            nrm = side.cross(t).normalized()
            return p + side * (x * w * 0.5) + nrm * (0.012 * (1.0 - x * x) * math.sin(v * 5.0))   # a slight cup that flips
        return F.scale_uv(M.shell(name, outer, nu, nv, thick=thick, color=color, mat=MAT, rim_color=rim), UVS)

    # ------------------------------------------------------------ hips (the part called body)
    BC, BR = G(0, 0.455, 0), (0.235, 0.19, 0.15)
    hips = lambda segs, rings: F.scale_uv(M.blob("hips", BC, BR, segs=segs, rings=rings, color=LS, mat=MAT), UVS)
    btree = M.proxy_tree(lambda: hips(48, 32))
    body = [hips(12, 7)]
    body += strips("bw", btree, BC, 0.2, [
        (-42, 0.09, 0, 6, 1), (-15, 0.085, 0, 10, 0), (11, 0.085, 0, -9, 1), (40, 0.075, 0, 3, 2),
    ], n=14)
    WK, wn = on_surf(btree, BC, hd(-68, 40))
    WK = WK + wn * 0.02
    body.append(M.blob("wknot", WK, (0.042, 0.036, 0.036), segs=8, rings=5, color=LL, mat=MAT))
    R.part("body", body, G(0, 0.43, 0))
    wout = V((wn.x, wn.y, 0)).normalized()
    R.part("waist_tail", [streamer("wtail", [WK, WK + wout * 0.045 + DOWN * 0.04, WK + wout * 0.07 + DOWN * 0.16,
                                             WK + wout * 0.085 + DOWN * 0.27, WK + wout * 0.08 + DOWN * 0.36], 0.05, 0.085, wout,
                                   color=LM)],
           WK, parent="body", soft=dict(dir=None, loops=6))

    # ------------------------------------------------------------ chest: a barrel, crossed over front and back
    CC, CR = G(0, 0.745, 0), (0.228, 0.186, 0.272)

    def cwarp(c):
        z = max(-1.0, min(1.0, c.z))
        rho = math.sqrt(max(1.0 - z * z, 0.0))
        k = (max(1.0 - abs(z) ** 3.2, 0.0) ** (1.0 / 3.2)) / rho if rho > 1e-5 else 0.0
        x, y = c.x * k, c.y * k
        if y < 0.0:   # a little belly
            y *= 1.0 + 0.1 * math.exp(-((z + 0.3) / 0.45) ** 2)
        return V((x, y, z))
    chest_blob = lambda segs, rings: F.scale_uv(M.blob("chest", CC, CR, segs=segs, rings=rings, color=LS, mat=MAT, warp=cwarp), UVS)
    ctree = M.proxy_tree(lambda: chest_blob(48, 40))
    chest = [chest_blob(14, 9)]
    chest += strips("cw", ctree, CC, 0.22, [
        (-40, 0.085, 0, 6, 0), (-22, 0.085, 0, -8, 1), (-3, 0.085, 3, 7, 0), (15, 0.085, -3, -7, 1),
        (34, 0.085, 0, 6, 0), (51, 0.085, 0, -5, 1), (66, 0.085, 0, 4, 2),
        (2, 0.08, 0, 37, 2, None, 0, 0.0), (2, 0.08, 0, -37, 2, None, 0, 0.0),
    ], n=16)
    R.part("chest", chest, G(0, 0.56, 0), parent="body")

    # ------------------------------------------------------------ legs (straight down at rest: IK chains)
    for side, s in (("l", 1), ("r", -1)):
        x = s * 0.12
        hip, knee, ankle = G(x, 0.40, 0), G(x, 0.245, 0), G(x, 0.10, 0)
        R.part(f"leg_{side}", limb(f"th{side}", hip, knee, 0.1, 0.092,
                                   [(0.22, 0.075, 0.012 * s, 1, 0.5), (0.52, 0.07, -0.014 * s, 2, 2.0), (0.82, 0.07, 0.01 * s, 0, 4.0)]),
               hip, parent="body", ball=0.106, ball_color=LS, ball_segs=7)
        R.part(f"shin_{side}", limb(f"sh{side}", knee, ankle, 0.09, 0.1,
                                    [(0.2, 0.07, -0.012 * s, 2, 1.0), (0.52, 0.07, 0.014 * s, 1, 3.0), (0.84, 0.075, -0.008 * s, 2, 5.0)]),
               knee, parent=f"leg_{side}", ball=0.097, ball_color=LM, ball_segs=7)
        FC, FR = G(s * 0.126, 0.072, 0.05), (0.108, 0.155, 0.09)

        def fwarp(c, k=0.072 / 0.09):
            return V((c.x * (1.0 + 0.14 * max(0.0, -c.y)), c.y, c.z if c.z > 0 else c.z * k))
        foot_blob = lambda segs, rings: F.scale_uv(M.blob(f"foot{side}", FC, FR, segs=segs, rings=rings, color=LS, mat=MAT, warp=fwarp), UVS)
        ftree = M.proxy_tree(lambda: foot_blob(40, 28))
        foot = [foot_blob(10, 6)]
        for k, (lat, w, layer) in enumerate(((54, 0.085, 1), (24, 0.08, 2), (-6, 0.08, 1), (-36, 0.085, 2))):
            col, edge, h = TONE[layer]
            on = M.band_fn(ftree, FC, V((0, -1, 0.06)), lat, 0.12, -112, 112, front=(0, 0, 1),
                           wave=lambda th, k=k: 2.0 * math.sin(th + k * 2.2) * s)
            foot.append(M.strap(f"fw{side}{k}", on, 9, w / 2, height=h, color=col, edge_color=edge, mat=MAT, uv_scale=UVS, fade=1))
        R.part(f"foot_{side}", foot, ankle, parent=f"shin_{side}", ball=0.096, ball_color=LS, ball_segs=7)
        R.ik_chain(f"leg_{side}", f"shin_{side}", f"foot_{side}")

    # ------------------------------------------------------------ arms (held out in front at rest: IK chains)
    for side, s in (("l", 1), ("r", -1)):
        sh = G(s * 0.245, 0.865, 0.02)
        d = G(s * 0.10, -0.13, 1.0).normalized()
        el = sh + d * 0.17
        wr = el + d * 0.155
        R.part(f"arm_{side}", limb(f"ua{side}", sh, el, 0.08, 0.074,
                                   [(-0.3, 0.06, 0.012 * s, 2, 3.0), (0.0, 0.07, 0.02 * s, 1, 1.0), (0.34, 0.07, 0.012 * s, 2, 0.0), (0.64, 0.07, -0.014 * s, 1, 2.0),
                                    (0.9, 0.06, 0.008 * s, 2, 4.0)], ball=0.09),
               sh, parent="chest", ball=0.09, ball_color=LS, ground=False)
        R.part(f"forearm_{side}", limb(f"fa{side}", el, wr, 0.073, 0.069,
                                       [(0.2, 0.065, -0.012 * s, 1, 1.0), (0.52, 0.065, 0.014 * s, 2, 3.0), (0.84, 0.065, -0.01 * s, 0, 5.0)]),
               el, parent=f"arm_{side}", ball=0.078, ball_color=LS, ball_segs=7, ground=False)
        # mitten hand: palm down, fingers forward, the thumb on the inside
        HCn = wr + d * 0.085
        hand_blob = lambda segs, rings: M.put(F.scale_uv(M.blob(f"palm{side}", (0, 0, 0), (0.08, 0.06, 0.1), segs=segs,
                                                                  rings=rings, color=LM, mat=MAT), UVS), HCn, d)
        htree_ = M.proxy_tree(lambda: hand_blob(32, 24))
        hand = [hand_blob(10, 7)]
        thumb = M.blob(f"thumb{side}", (0, 0, 0), (0.03, 0.028, 0.05), segs=7, rings=5, color=LL, mat=MAT)
        M.put(thumb, wr + d * 0.07 + V((-s * 0.07, 0, -0.012)), (d + V((-s * 0.55, 0, 0))).normalized())
        hand.append(thumb)
        for k, (lat, w, layer, roll) in enumerate(((-34, 0.07, 2, 8), (4, 0.065, 0, -10))):
            col, edge, h = TONE[layer]
            on = M.band_fn(htree_, HCn, (d + V((0.01 * roll * s, 0, 0))).normalized(), lat, 0.085, front=(0, 0, 1))
            hand.append(M.strap(f"hw{side}{k}", on, 9, w / 2, height=h * 0.8, closed=True, color=col, edge_color=edge, mat=MAT,
                                uv_scale=UVS))
        R.part(f"hand_{side}", hand, wr, parent=f"forearm_{side}", ball=0.072, ball_color=LM, ball_segs=7, ground=False)
        R.ik_chain(f"arm_{side}", f"forearm_{side}", f"hand_{side}")
        if side == "l":   # a loose strip hanging from the left wrist
            pin = wr - d * 0.035 + DOWN * 0.064
            back = V((0, 1, 0))
            R.part("wrist_strip", [streamer("wstrip", [pin, pin + DOWN * 0.07 + back * 0.012, pin + DOWN * 0.16 + back * 0.02,
                                                      pin + DOWN * 0.25 + back * 0.012], 0.045, 0.07, V((s, 0.3, 0)), nv=7,
                                            color=LL)],
                   pin, parent="forearm_l", soft=dict(dir=None, loops=6), ground=False)

    # ------------------------------------------------------------ head
    HC, HR, HREF = G(0, 1.2, 0.005), (0.29, 0.265, 0.258), 0.272
    # The face is laid out with the big eye on the monster's right and mirrored by FS: +1 puts it on
    # its left, the side most game and showcase cameras see. mz() mirrors an azimuth.
    FS = 1.0
    mz = lambda az: -FS * az
    EYE, SQ = (mz(-18.0), 0.0), (mz(28.0), 4.5)   # (azimuth, elevation) of the big eye and the squinting one
    LID, LIDP, LIDR, LIDW = 14.5, 17.0, 14.0, 0.074   # the two lids: latitude, pitch, roll of their axes, width
    head_blob = lambda segs, rings: F.scale_uv(M.blob("skull", HC, HR, segs=segs, rings=rings, color=LS, mat=MAT), UVS)
    htree = M.proxy_tree(lambda: head_blob(64, 48))
    skull = head_blob(18, 11)

    def lid_el(a2):   # elevation of the upper lid's centre line at an (unmirrored) azimuth; the lower lid is its mirror
        a = math.radians(a2)
        return LID + LIDP * math.cos(a) + LIDR * math.sin(a)

    def in_gap(c, n):   # the hollow the eyes look out of: between the lids, from the eye's far side to the squint
        if c.y > 0.0:
            return False
        a2 = mz(math.degrees(math.atan2(c.x, -c.y)))
        el = math.degrees(math.asin(max(-1.0, min(1.0, c.z / max(c.length, 1e-6)))))
        if not -84.0 < a2 < 41.0:
            return False
        if a2 > 6.0:      # past the bridge: only the slit round the squinting eye
            return abs(el - SQ[1]) < 13.0
        return -lid_el(a2) - 3.0 < el < lid_el(a2) + 3.0
    K.paint(skull, DK, where=in_gap, mat="flat")
    head = [skull]
    head += strips("hw", htree, HC, HREF, [
        (-64, 0.09, 0, 0, 1), (-45, 0.085, 3, 6, 0), (-29, 0.085, 8, -7, 1),
        (-LID, LIDW, LIDP, -FS * LIDR, 2, None, 0, 0.0),    # the lower lid: dips under the eye, climbs on its far side
        (LID, LIDW, -LIDP, FS * LIDR, 2, None, 0, 0.0),     # the upper lid: arches over the eye
        (36, 0.085, -8, 9, 1), (55, 0.085, 6, -12, 0), (73, 0.09, -4, 6, 1),
        (17, 0.085, 0, 0, 1, (100, 258)),                   # fill the back of the head where the two lids' bands part
        (8, 0.085, 0, 0, 0, (186, 317) if FS > 0 else (43, 174), 0, 2.5),    # ... and the side of the head past the squint
        (-10, 0.09, 0, 0, 1, (182, 318) if FS > 0 else (42, 178), 0, 2.5),
    ], n=18)
    # over the top: two straps crossing on the crown; both run down the face and end under a lid
    for k, (az, a0, a1, layer) in enumerate(((46, -108, 128, 2), (-28, -112, 62, 2))):
        col, edge, h = TONE[layer]
        on = M.band_fn(htree, HC, hd(mz(az) + 90, 0), 0.0, HREF, a0, a1, front=(0, 0, 1))
        head.append(M.strap(f"crown{k}", on, 11, 0.039, height=h * (1.0 + 0.2 * k), color=col, edge_color=edge, mat=MAT,
                            uv_scale=UVS, fade=2))
    # two diagonals cross beside the big eye and leave the slit the second eye squints through
    head.append(diag("diag_b", htree, HC, HREF, (mz(-6), 27), (mz(60), -43), 0.072, 1, n=9))
    head.append(diag("diag_a", htree, HC, HREF, (mz(2), 12), (mz(50), 24), 0.072, 1, n=6))
    # the knot at the back right, on the upper lid's band
    KP, kn = on_surf(htree, HC, hd(-120, 17 if FS > 0 else 1))
    K0 = KP + kn * 0.022
    head.append(M.blob("knot", K0, (0.05, 0.042, 0.046), segs=8, rings=5, color=LL, mat=MAT))
    R.part("head", head, G(0, 0.965, 0), parent="chest", ground=False)

    ep, en = on_surf(htree, HC, hd(*EYE))
    # A shallow dome: the pupil part slides flat across it (MonsterFace), so a deep one would swallow its trailing edge.
    M.eye(R, "eye", "pupil", ep - en * 0.004, en, 0.084, 0.084, parent="head", depth=0.022, sclera=DK, iris="glow_amber_dim",
          iris_r=0.78, iris2="glow_amber_dim", iris2_r=0.56, pupil="ink", pupil_r=0.33, lift=0.0022)
    sp, sn = on_surf(htree, HC, hd(*SQ))
    M.eye(R, "eye_s0", "pupil_s0", sp - sn * 0.003, sn, 0.05, 0.025, parent="head", depth=0.011, sclera=DK, iris="glow_amber_dim",
          iris_r=0.95, iris2="glow_amber_dim", iris2_r=0.66, pupil="ink", pupil_r=0.36, roll=6 * FS, segs=10, lift=0.002)
    kout = V((kn.x, kn.y, 0)).normalized()
    ktan = UP.cross(kout).normalized()
    R.part("knot_tail_a", [streamer("ktail_a", [K0, K0 + kout * 0.05 + DOWN * 0.03 + ktan * 0.01, K0 + kout * 0.085 + DOWN * 0.14 + ktan * 0.03,
                                                K0 + kout * 0.105 + DOWN * 0.26 + ktan * 0.045, K0 + kout * 0.125 + DOWN * 0.37 + ktan * 0.04],
                                    0.05, 0.085, kout, color=LL)],
           K0, parent="head", soft=dict(dir=None, loops=6), ground=False)
    R.part("knot_tail_b", [streamer("ktail_b", [K0, K0 + kout * 0.04 + DOWN * 0.035 - ktan * 0.03, K0 + kout * 0.06 + DOWN * 0.13 - ktan * 0.06,
                                                K0 + kout * 0.075 + DOWN * 0.26 - ktan * 0.075], 0.05, 0.075, kout, nv=7, color=LM)],
           K0, parent="head", soft=dict(dir=None, loops=6), ground=False)

    # ------------------------------------------------------------ the wraps that fly off when it dies (alt_*: hidden until then)
    def helix(name, turns, y0, y1, r0, r1, phase, nv, w=0.07, color=LL):
        def outer(u, v):
            a = phase + math.tau * turns * v
            r = r0 + (r1 - r0) * v
            return G(math.sin(a) * r, y0 + (y1 - y0) * v + (u - 0.5) * w, math.cos(a) * r)
        return F.scale_uv(M.shell(name, outer, 1, nv, thick=0.006, color=color, mat=MAT, rim_color=LS), UVS)
    R.part("alt_unwind_a", [helix("unw_a", 1.5, 0.16, 1.2, 0.3, 0.36, 0.0, 24)], G(0, 0.43, 0), parent="body", ground=False)
    R.part("alt_unwind_b", [helix("unw_b", 1.2, 0.3, 1.3, 0.34, 0.3, 2.6, 18, color=LM)], G(0, 0.43, 0), parent="body",
           ground=False)

    poses = [
        ("rest", {}),
        ("shamble", {"body": [("z", 6)], "chest": [("z", -4), ("y", 12)], "head": [("z", -12), ("y", -20)], "arm_l": [("x", 15)],
                     "forearm_l": [("x", 20)], "arm_r": [("x", -12)], "leg_l": [("x", -30)], "shin_l": [("x", 25)],
                     "leg_r": [("x", 28)], "foot_r": [("x", -20)]}),
        ("wind-up", {"chest": [("x", -22)], "head": [("x", 10)], "arm_l": [("y", 85), ("x", -25)], "arm_r": [("y", -85), ("x", -25)],
                     "forearm_l": [("y", 30)], "forearm_r": [("y", -30)], "hand_l": [("z", -60)], "hand_r": [("z", 60)],
                     "leg_l": [("x", -15)], "leg_r": [("x", 20)]}),
        ("grab", {"body": [("x", 12)], "chest": [("x", 28)], "head": [("x", -25)], "arm_l": [("y", -28), ("x", -10)],
                  "arm_r": [("y", 28), ("x", -10)], "hand_l": [("z", 90)], "hand_r": [("z", -90)]}),
        ("hit, head spin", {"chest": [("x", -25), ("y", 20)], "head": [("y", 95), ("z", 15)], "arm_l": [("x", -110), ("y", 30)],
                            "arm_r": [("x", -80), ("y", -50)], "forearm_l": [("x", -60)], "leg_l": [("x", -50)],
                            "shin_l": [("x", 80)], "foot_l": [("x", 30)]}),
        ("arms down, crouch", {"arm_l": [("x", 80)], "arm_r": [("x", 80)], "forearm_l": [("x", -90)], "forearm_r": [("x", -30)],
                               "body": [("move", (0, -0.08, 0))], "leg_l": [("z", 25), ("x", -55)], "shin_l": [("x", 95)],
                               "foot_l": [("x", -40)], "leg_r": [("z", -25), ("x", -55)], "shin_r": [("x", 95)],
                               "foot_r": [("x", -40)], "chest": [("y", 40)], "head": [("y", -60), ("x", 20)]}),
    ]
    return R.finish(ao_radius=0.2, ao_strength=0.9, poses=poses,
                    closeups=[("face", 2.6, tuple(HC)), ("torso", 2.0, tuple(G(0, 0.6, 0)))])


# ====================================================================== SCARECROW
def scarecrow():
    """A pogo scarecrow: no legs, it hops on its one pole. A buffalo-check flannel shirt (the check is
    painted face by face on a grid) with a blue patch, a burlap sack head with stitched button eyes
    on dim glow discs, stitched brows and smile, a big crooked witch hat whose whole cone is one soft
    part, and straw everywhere: hair, collar, cuffs, shirt hem, a tuft round the foot of the pole.
    Every straw blade is an M.straw() leaf, so the shader's cloth_wave flaps it.

    Part tree: pole > tuft, body > skirt, collar, arm_l/r > cuff_l/r, head > hair, hat > hat_tip,
    eye_l/r > pupil_l/r, brow_l/r, mouth, alt_mouth_o, alt_mouth_zig."""
    K.reset()
    RED, RED2, RED3 = "flannel_red", "flannel_dark", "choco_dark"
    SACK, SEAM, THREAD = "peanut_tan", "leather", "candy_corn_yellow"
    PURPLE, PURPLE_D = "hat_violet", "spider_plum"
    # golden straw with orange shade (the pale "straw" cell went olive under the showcase's purple light)
    STRAW = ("golden_leaf", "straw_dark", "caramel")
    STRAW_IN = ("caramel", "straw_dark")
    UP = V((0, 0, 1))
    rnd = M.rnd(11)
    R = M.Rig("monster_scarecrow", height=1.6, radius=0.35)
    DY = -0.035   # everything above the pole sits this much lower than the numbers below say (the hat must stay under 1.76 m)

    def P(x, y, z):
        """A point (not a direction) above the pole, in game coordinates."""
        return G(x, y + DY, z)

    def interp(table, x):
        """Piecewise-linear lookup in [(x, y), ...] sorted by x."""
        if x <= table[0][0]:
            return table[0][1]
        for (x0, y0), (x1, y1) in zip(table[:-1], table[1:]):
            if x <= x1:
                return y0 + (y1 - y0) * (x - x0) / (x1 - x0)
        return table[-1][1]

    def blades(prefix, count, fn, colors=STRAW, shift=0, n=4):
        """count straw blades; fn(k) -> dict of M.straw arguments (root, direction, length, width, up, droop)."""
        out = []
        for k in range(count):
            out.append(M.straw(f"{prefix}{k}", color=colors[(k + shift) % len(colors)], n=n, **fn(k)))
        return out

    def stitch_run(name, pts_normals, length=0.03, width=0.008, height=0.004, color=THREAD, across=False):
        """Stitch dashes along a run of (point, normal): each lies along the run (or across it)."""
        items = []
        for i, (p, n) in enumerate(pts_normals):
            a, b = pts_normals[max(i - 1, 0)][0], pts_normals[min(i + 1, len(pts_normals) - 1)][0]
            t = (b - a).normalized()
            if across:
                t = n.cross(t).normalized()
            items.append((p, t, n))
        return M.D.stitches(name, items, length=length, width=width, height=height, color=color)

    # ------------------------------------------------------------ the pole, and the straw tied round its foot
    post = M.tube("post", [G(0, 0.022, 0), G(0.004, 0.2, 0.002), G(-0.003, 0.44, -0.002), G(0, 0.7, 0)], [0.044, 0.053, 0.049, 0.05],
                  sides=8, color="wood", mat="wood", cap_offset=0.5)
    R.part("pole", [post], G(0, 0, 0))

    def tuft_blade(k):
        a = math.tau * k / 7 + rnd.uniform(-0.25, 0.25)
        rad = G(math.sin(a), 0, math.cos(a))
        return dict(root=G(0, 0.082, 0) + rad * 0.03, direction=rad * 0.93 - UP * 0.36, length=rnd.uniform(0.11, 0.15), width=0.05,
                    up=UP, droop=0.1)
    tuft = blades("tuft", 7, tuft_blade, n=3)
    tuft.append(K.torus("tie", 0.056, 0.015, color="rope", mat="burlap", major_seg=10, minor_seg=4, loc=G(0, 0.09, 0)))
    R.part("tuft", tuft, G(0, 0.06, 0), parent="pole")

    # ------------------------------------------------------------ body: the flannel shirt
    ROWS = [(0.545, 0.268), (0.585, 0.258), (0.625, 0.249), (0.665, 0.242), (0.705, 0.236), (0.745, 0.231), (0.785, 0.226),
            (0.825, 0.220), (0.865, 0.210), (0.900, 0.194), (0.932, 0.166), (0.958, 0.130), (0.982, 0.096), (1.004, 0.076)]
    DEPTH = 0.77

    def tp(a, y, off=0.0):
        """Point on the shirt at azimuth a (degrees, 0 = front, + toward the monster's left) and height y."""
        r, ar = interp(ROWS, y), math.radians(a)
        p = P(r * math.sin(ar), y, r * DEPTH * math.cos(ar))
        return p + tn(a, y) * off if off else p

    def tn(a, y):
        return (tp(a + 1.5, y) - tp(a - 1.5, y)).cross(tp(a, y + 0.006) - tp(a, y - 0.006)).normalized()

    def check(i, j):
        return ((RED, RED2, RED3)[(i % 2) + (j % 2)], "fabric")
    NV = len(ROWS) - 1

    def torso(u, v):
        k = min(int(round(v * NV)), NV)
        return tp((u - 0.5) * 360.0, ROWS[k][0])
    body = [M.patch("shirt", torso, 24, NV, color=RED, mat="fabric", paint=check),
            M.blob("neck", P(0, 1.005, 0), (0.082, 0.066, 0.05), segs=8, rings=4, color=SACK, mat="burlap"),
            M.blob("under", P(0, 0.57, 0), (0.255, 0.255 * DEPTH, 0.028), segs=10, rings=3, color=RED3)]
    body.append(M.strap("hem", lambda t, s: (tp(t * 360.0 - 180.0, 0.556 + s), tn(t * 360.0 - 180.0, 0.556 + s)), 24, 0.017,
                        height=0.008, closed=True, color=RED2, edge_color=RED3, mat="fabric"))
    body.append(stitch_run("hem_st", [(tp(a, 0.603, 0.002), tn(a, 0.603)) for a in range(-173, 180, 15)]))

    def placket(t, s):
        y = 0.6 + 0.35 * t
        a = math.degrees(s / interp(ROWS, y))
        return tp(a, y), tn(a, y)
    body.append(M.strap("placket", placket, 8, 0.024, height=0.006, color=RED, edge_color=RED2, mat="fabric", fade=1))
    for k, y in enumerate((0.69, 0.83)):
        for o in M.button(f"btn{k}", 0.021, thick=0.011, color="foil_gold", rim="foil_gold_dark", thread="choco_dark", segs=10):
            M.put(o, tp(0, y, 0.005), tn(0, y))
            body.append(o)
    body.append(stitch_run("yoke_st", [(tp(a, 0.872 + 0.026 * abs(a - 180) / 60.0, 0.002), tn(a, 0.872 + 0.026 * abs(a - 180) / 60.0))
                                       for a in range(118, 243, 14)]))
    # the blue patch on its left hip, sewn on with big yellow stitches
    PA, PY, PW, PH, PROLL = 41.0, 0.705, 0.11, 0.1, math.radians(9)

    def patch_pt(u, v, off):
        dx, dy = (u - 0.5) * PW, (v - 0.5) * PH
        ex, ey = dx * math.cos(PROLL) - dy * math.sin(PROLL), dx * math.sin(PROLL) + dy * math.cos(PROLL)
        y = PY + ey
        return tp(PA + math.degrees(ex / interp(ROWS, y)), y, off)

    def patch_n(u, v, e=0.02):
        return (patch_pt(u + e, v, 0.0) - patch_pt(u - e, v, 0.0)).cross(patch_pt(u, v + e, 0.0) - patch_pt(u, v - e, 0.0)).normalized()
    body.append(M.shell("patch", lambda u, v: patch_pt(u, v, 0.009), 3, 3, inner=lambda u, v: patch_pt(u, v, -0.004), color="quilt_blue",
                        mat="denim", rim_color="denim"))
    edge = [(0.12 + 0.38 * k, 0.1) for k in range(3)] + [(0.9, 0.12 + 0.38 * k) for k in range(3)] + \
           [(0.88 - 0.38 * k, 0.9) for k in range(3)] + [(0.1, 0.88 - 0.38 * k) for k in range(3)]
    items = []
    for k, (u, v) in enumerate(edge):
        tang = (patch_pt(u + 0.05, v, 0.0) - patch_pt(u - 0.05, v, 0.0)) if (k // 3) % 2 == 0 else (patch_pt(u, v + 0.05, 0.0) - patch_pt(u, v - 0.05, 0.0))
        items.append((patch_pt(u, v, 0.0105), tang, patch_n(u, v)))
    body.append(M.D.stitches("patch_st", items, length=0.024, width=0.008, height=0.004, color=THREAD))
    R.part("body", body, P(0, 0.56, 0), parent="pole")

    # ------------------------------------------------------------ straw under the shirt hem (two layers)
    def hem_outer(k):
        a = 360.0 * k / 13 + rnd.uniform(-6, 6)
        rad = V((tn(a, 0.6).x, tn(a, 0.6).y, 0)).normalized()
        f = math.radians(rnd.uniform(27, 38))
        return dict(root=tp(a, 0.61, -0.04), direction=-UP * math.cos(f) + rad * math.sin(f), length=rnd.uniform(0.19, 0.235), width=0.082,
                    up=rad, droop=-0.14)

    def hem_inner(k):
        a = 360.0 * (k + 0.5) / 11 + rnd.uniform(-6, 6)
        rad = V((tn(a, 0.6).x, tn(a, 0.6).y, 0)).normalized()
        f = math.radians(rnd.uniform(12, 20))
        return dict(root=tp(a, 0.6, -0.075), direction=-UP * math.cos(f) + rad * math.sin(f), length=rnd.uniform(0.14, 0.17), width=0.07,
                    up=rad, droop=-0.08)
    skirt = blades("hemo", 13, hem_outer) + blades("hemi", 11, hem_inner, colors=STRAW_IN, n=3)
    R.part("skirt", skirt, P(0, 0.56, 0), parent="body", ao_from=("body",))

    # ------------------------------------------------------------ straw collar (two layers, lying on the shoulders)
    def col_lower(k):
        a = math.tau * k / 11 + rnd.uniform(-0.1, 0.1)
        rad = G(math.sin(a), 0, math.cos(a))
        g = math.radians(rnd.uniform(24, 32))
        return dict(root=P(0, 1.0, 0) + rad * 0.05, direction=rad * math.cos(g) - UP * math.sin(g), length=rnd.uniform(0.19, 0.22), width=0.085,
                    up=UP, droop=0.16)

    def col_upper(k):
        a = math.tau * (k + 0.5) / 9 + rnd.uniform(-0.1, 0.1)
        rad = G(math.sin(a), 0, math.cos(a))
        g = math.radians(rnd.uniform(6, 13))
        return dict(root=P(0, 1.012, 0) + rad * 0.05, direction=rad * math.cos(g) - UP * math.sin(g), length=rnd.uniform(0.17, 0.2), width=0.075,
                    up=UP, droop=0.12)
    collar = blades("coll", 11, col_lower, shift=1) + blades("colu", 9, col_upper)
    R.part("collar", collar, P(0, 1.0, 0), parent="body", ao_from=("body", "head"), ground=False)

    # ------------------------------------------------------------ arms: a stuffed sleeve on the crossbar, rope ties, the bar's end
    SLV = [(0.0, 0.088), (0.17, 0.095), (0.34, 0.097), (0.51, 0.095), (0.68, 0.088), (0.82, 0.076), (0.9, 0.067), (1.0, 0.084), (1.012, 0.04)]
    for side, s in (("l", 1), ("r", -1)):
        SH = P(s * 0.15, 0.905, 0)
        AX = V((s, 0, 0))

        def sl(t, ang, off=0.0, s=s):
            """Point on the sleeve: t along it (0 = shoulder, 1 = cuff edge), ang degrees round the arm (0 = top, 90 = front)."""
            r, a = interp(SLV, t) + off, math.radians(ang)
            return P(s * (0.15 + 0.36 * t), 0.905 - 0.014 * math.sin(math.pi * min(t, 1.0)) + r * math.cos(a), r * math.sin(a))

        def sleeve(u, v, sl=sl):
            return sl(SLV[min(int(round(v * 8)), 8)][0], u * 360.0)

        def sleeve_paint(i, j):
            if j >= 7:
                return (RED3, "fabric")
            if j == 6:
                return (RED2, "fabric")
            return check(i, j)
        arm = [M.patch(f"sleeve{side}", sleeve, 10, 8, color=RED, mat="fabric", paint=sleeve_paint, flip=s < 0)]
        for k, t in enumerate((0.875, 0.925)):
            coil = K.torus(f"rope{side}{k}", interp(SLV, t) + 0.003, 0.0135, color="rope", mat="burlap", major_seg=10, minor_seg=4)
            M.put(coil, P(s * (0.15 + 0.36 * t), 0.905 - 0.014 * math.sin(math.pi * t), 0), AX)
            arm.append(coil)
        arm.append(M.tube(f"bar{side}", [P(s * 0.4, 0.9, 0), P(s * 0.67, 0.9, 0)], [0.036, 0.039], sides=8, color="wood", mat="wood",
                          end_color="wood_light", cap_offset=0.12))
        ring = [(sl(0.745, a, 0.002), (sl(0.745, a, 0.01) - sl(0.745, a, 0.0)).normalized()) for a in range(0, 360, 45)]
        arm.append(M.D.stitches(f"cuffst{side}", [(p, AX.cross(n), n) for p, n in ring], length=0.026, width=0.008, height=0.004, color=THREAD))
        R.part(f"arm_{side}", arm, SH, parent="body", ball=0.09, ball_color=RED2, ground=False)

        WR = P(s * 0.51, 0.9, 0)

        def cuff_outer(k, s=s, WR=WR, AX=AX):
            a = math.tau * k / 9 + rnd.uniform(-0.15, 0.15)
            rad = G(0, math.cos(a), math.sin(a))
            f = math.radians(rnd.uniform(36, 48))
            return dict(root=WR + rad * 0.045 - AX * 0.03, direction=AX * math.cos(f) + rad * math.sin(f), length=rnd.uniform(0.16, 0.2),
                        width=0.066, up=rad, droop=-0.1)

        def cuff_inner(k, s=s, WR=WR, AX=AX):
            a = math.tau * (k + 0.5) / 7 + rnd.uniform(-0.15, 0.15)
            rad = G(0, math.cos(a), math.sin(a))
            f = math.radians(rnd.uniform(14, 22))
            return dict(root=WR + rad * 0.04 - AX * 0.03, direction=AX * math.cos(f) + rad * math.sin(f), length=rnd.uniform(0.17, 0.21),
                        width=0.056, up=rad, droop=-0.05)
        cuff = blades(f"cfo{side}", 9, cuff_outer, shift=s + 1) + blades(f"cfi{side}", 7, cuff_inner, colors=STRAW_IN, n=3)
        # Local +Y runs out along the arm: the shader's ripple goes round that axis.
        R.part(f"cuff_{side}", cuff, WR, parent=f"arm_{side}", z_out=G(0, 0, 1), up=tuple(AX), ground=False, ao_from=(f"arm_{side}",))

    # ------------------------------------------------------------ head: a stuffed burlap sack
    HC, HR = P(0, 1.238, 0.005), (0.268, 0.25, 0.236)

    def hwarp(c):
        k = 1.0 + 0.07 * max(0.0, -c.z) - 0.03 * max(0.0, c.z)   # fuller low down
        z = c.z if c.z > -0.8 else -0.8 - (c.z + 0.8) * 0.7      # a little flat underneath
        return V((c.x * k, c.y * k, z))
    head_blob = lambda segs, rings: F.scale_uv(M.blob("sack", HC, HR, segs=segs, rings=rings, color=SACK, mat="burlap", warp=hwarp), 2.2)
    htree = M.proxy_tree(lambda: head_blob(48, 32))

    def hd(az, el):
        a, e = math.radians(az), math.radians(el)
        return V((math.sin(a) * math.cos(e), -math.cos(a) * math.cos(e), math.sin(e)))

    def hs(az, el, off=0.0):
        """(point, normal) on the head at azimuth az (0 = front, + toward its left) and elevation el."""
        d = hd(az, el)
        loc, nrm, _, _ = htree.ray_cast(HC + d * 4.0, -d, 8.0)
        return loc + nrm * off, nrm
    head = [head_blob(22, 13)]
    for s in (-1, 1):
        p, n = hs(s * 45, -17)
        head.append(M.cheek(f"cheek{s}", p, n, 0.04, 0.027, color="cheek_pink", roll=-12 * s))
    p, n = hs(-1, -9)
    nose = K.polygon_prism("nose", F.rounded_triangle(0, 0, 0.062, 0.052, point_down=True, round_=0.25, arc_pts=1), 0.016, color="pumpkin",
                           smooth=False)
    M.put(nose, p - n * 0.005, n)
    head.append(nose)
    # the seam down the back of the sack
    back = [hs(180, e, 0.001) for e in range(64, -62, -14)]
    head.append(M.tube("seam", [p for p, _ in back], 0.005, sides=3, color=SEAM, mat="flat"))
    head.append(stitch_run("seam_st", [hs(180, e, 0.003) for e in range(57, -55, -14)], length=0.04, width=0.009, height=0.005, color="straw",
                           across=True))
    R.part("head", head, P(0, 1.02, 0), parent="body", ground=False, ao_from=("hat",))

    # brows: thick stitched arcs, one part each so they can frown and lift
    for side, az0, el0, hw in (("l", 22.0, 24.5, 13.0), ("r", -23.0, 23.5, 10.5)):
        pts = []
        for i in range(5):
            x = (i / 4 - 0.5) * 2.0
            pts.append(hs(az0 + x * hw, el0 + 3.4 * (1.0 - x * x), 0.007)[0])
        pc, nc = hs(az0, el0 + 2.0)
        R.part(f"brow_{side}", [M.tube(f"brow{side}", pts, [0.008, 0.0125, 0.014, 0.0125, 0.008], sides=5, color="hair_dark", mat="flat",
                                       cap_offset=0.8)], pc, parent="head", z_out=nc, ground=False)

    # mouth: a stitched smirk; two more shapes it can swap to (hidden until the animator shows them)
    def lip(az):
        return -37.0 + 11.5 * (abs(az) / 32.0) ** 1.8 + (2.5 if az > 0 else 0.0) * (abs(az) / 32.0)
    MP, MN = hs(0, -33.5)
    smile = [M.tube("smile", [hs(az, lip(az), 0.004)[0] for az in range(-31, 35, 8)], 0.012, sides=5, color="hair_dark", mat="flat",
                    cap_offset=0.8),
             stitch_run("smile_st", [hs(az, lip(az), 0.005) for az in (-23, -11.5, 0, 11.5, 23)], length=0.042, width=0.012, height=0.007,
                        color="hair_dark", across=True)]
    R.part("mouth", smile, MP, parent="head", z_out=MN, ground=False)
    RH = 0.26
    oval = [hs(math.degrees(math.cos(math.tau * k / 10) * 0.043 / RH), -33.5 + math.degrees(math.sin(math.tau * k / 10) * 0.052 / RH), 0.004)[0]
            for k in range(10)]
    hole = M.cheek("mouth_hole", MP, MN, 0.043, 0.052, color="mouth_dark", segs=10)
    R.part("alt_mouth_o", [hole, M.tube("mouth_rim", oval + oval[:1], 0.009, sides=4, color="hair_dark", mat="flat", cap_start=False,
                                        cap_end=False)], MP, parent="head", z_out=MN, ground=False)
    zig = [hs(az, -33.0 + (3.6 if k % 2 else -3.6) * (0.4 if k in (0, 6) else 1.0), 0.004)[0] for k, az in enumerate(range(-27, 28, 9))]
    R.part("alt_mouth_zig", [M.tube("mouth_zig", zig, 0.0105, sides=5, color="hair_dark", mat="flat", cap_offset=0.8)], MP, parent="head",
           z_out=MN, ground=False)

    # eyes: a button (the pupil part: it slides to look and spins when dizzy) on a dim glow disc (the
    # eye part), with a cross of glowing thread; the glints float above the button and stay put
    for side, az, el, w, col, rim in (("l", 21.5, -1.0, 0.084, "banner_purple", PURPLE), ("r", -22.5, 0.0, 0.07, "pumpkin", "pumpkin_dark")):
        ep, en = hs(az, el)
        rb, th = w * 0.82, 0.016
        glints = [M.blob(f"gl{side}a", (-rb * 0.36, rb * 0.42, th + 0.0035), (rb * 0.25, rb * 0.17, 0.002), segs=8, rings=3, color="eye_white"),
                  M.blob(f"gl{side}b", (rb * 0.4, -rb * 0.34, th + 0.0035), (rb * 0.1, rb * 0.09, 0.002), segs=6, rings=3, color="eye_white")]
        M.eye(R, f"eye_{side}", f"pupil_{side}", ep - en * 0.002, en, w, w, parent="head", depth=0.005, sclera="glow_jack_hot", iris=None,
              pupil=None, glints=False, segs=16, rings=3, eye_extra=glints,
              pupil_extra=M.button(f"button{side}", rb, thick=th, color=col, rim=rim, thread="glow_candy_gold", segs=16, cross=0.3, thread_w=0.17))

    # ------------------------------------------------------------ straw hair, out from under the hat
    def low(az):
        """0 at the sides .. 1 at the back: the hat is tipped back, so its brim sits lower there and the
        hair has to come out from under it lower down (else the roots poke up through the brim)."""
        return (1.0 - math.cos(math.radians(az))) * 0.5 - 0.25 if abs(az) > 60 else 0.0

    def hair_side(k):
        az = 62.0 + 236.0 * k / 12 + rnd.uniform(-5, 5)
        f = max(0.0, low(az)) / 0.75
        p, n = hs(az, 41 - 15 * f)
        q, _ = hs(az, 14 - 12 * f)
        return dict(root=p - n * 0.012, direction=(q - p).normalized() + n * 0.3, length=rnd.uniform(0.2, 0.25), width=0.078, up=n, droop=-0.3)

    def hair_under(k):
        az = 72.0 + 216.0 * (k + 0.5) / 9 + rnd.uniform(-5, 5)
        f = max(0.0, low(az)) / 0.75
        p, n = hs(az, 30 - 12 * f)
        q, _ = hs(az, 4 - 10 * f)
        return dict(root=p - n * 0.02, direction=(q - p).normalized() + n * 0.12, length=rnd.uniform(0.15, 0.18), width=0.07, up=n, droop=-0.2)

    def bang(k):
        az = (-52.0, -9.0, 7.0, 53.0)[k]
        p, n = hs(az, 44)
        q, _ = hs(az * 1.08, 20)
        return dict(root=p - n * 0.01, direction=(q - p).normalized() + n * 0.25, length=(0.13, 0.085, 0.1, 0.13)[k], width=0.062, up=n,
                    droop=-0.25)
    hair = blades("hair", 13, hair_side) + blades("hairu", 9, hair_under, colors=STRAW_IN, n=3) + blades("bang", 4, bang, shift=1)
    R.part("hair", hair, P(0, 1.44, 0), parent="head", ground=False, ao_from=("head", "hat"))

    # ------------------------------------------------------------ the hat: brim, band and buckle; the whole cone is the soft tip
    HX = M.Matrix.Translation(P(0, 1.392, -0.012)) @ (M.Matrix.Rotation(math.radians(-16), 4, "X") @ M.Matrix.Rotation(math.radians(7), 4, "Y"))
    HXR = HX.to_3x3()

    def brim_warp(c):
        r, a = math.hypot(c.x, c.y), math.atan2(c.y, c.x)
        f = min(max((r - 0.2) / 0.23, 0.0), 1.0) ** 1.4
        return V((c.x, c.y, c.z + f * (0.03 * math.sin(2 * a + 0.9) + 0.013 * math.sin(5 * a + 2.0) - 0.02)))
    brim = M.D.lathe("brim", [(0.12, -0.004), (0.3, -0.01), (0.405, -0.012), (0.432, 0.0), (0.405, 0.014), (0.3, 0.017), (0.185, 0.02)], 20,
                     color=PURPLE, mat="plush", warp=brim_warp)
    K.paint(brim, PURPLE_D, where=lambda c, n: n.z < -0.25)
    crown = M.D.lathe("crown0", [(0.207, -0.004), (0.203, 0.03), (0.194, 0.07), (0.187, 0.1)], 16, color=PURPLE, mat="plush")
    # 'plush', not 'felt': the felt layer is a crumpled-foil relief that catches the key light all over a
    # big hat (it read as shiny lilac foil in BEFORE, and still did with its UVs 2.6 times finer)
    brim.matrix_world = HX
    crown.matrix_world = HX
    hat = [brim, crown]

    def band_on(t, s):
        a, z = math.tau * t, 0.05 + s
        d = V((math.cos(a), math.sin(a), 0))
        return HX @ (d * (0.21 - 0.2 * z) + V((0, 0, z))), HXR @ (d + V((0, 0, 0.19))).normalized()
    hat.append(M.strap("band", band_on, 20, 0.035, height=0.008, closed=True, color="pumpkin", edge_color="pumpkin_dark"))
    for name, size, y, col, mat in (("buckle", (0.078, 0.016, 0.072), -0.208, "foil_gold", "metal"), ("buckle_in", (0.046, 0.016, 0.04), -0.213, "pumpkin_dark", "flat")):
        b = K.box(name, size, color=col, mat=mat, bevel=0.005)
        b.matrix_world = HX @ M.Matrix.Translation((0, y, 0.05))
        hat.append(b)
    items = []
    for k in range(18):
        a = math.tau * (k + 0.5) / 18
        pl = brim_warp(V((math.cos(a) * 0.385, math.sin(a) * 0.385, 0.0146)))
        items.append((HX @ pl, HXR @ V((-math.sin(a), math.cos(a), 0)), HXR @ UP))
    hat.append(M.D.stitches("brim_st", items, length=0.042, width=0.0095, height=0.0045, color=THREAD))
    R.part("hat", hat, P(0, 1.4, 0), parent="head", ground=False, ao_from=("head",))

    BEND = V((-0.9, 0.42, 0)).normalized()     # the cone curls over toward the monster's right and back
    SIDE = UP.cross(BEND)
    BASE = V((0, 0, 0.094))
    phi = lambda u: 3.0 * u ** 2.2
    rad = lambda u: 0.172 * (1.0 - u) ** 1.2 * (1.0 + 0.12 * math.sin(math.pi * u)) + 0.012 * (1.0 - u) + 0.006

    def axis(u, steps=40, length=None):
        p = BASE.copy()
        for k in range(steps):
            f = phi((k + 0.5) / steps * u)
            p += (UP * math.cos(f) + BEND * math.sin(f)) * ((LEN if length is None else length) * u / steps)
        return p
    # as tall as the height limit allows (10% over the collision height, less a centimetre)
    LEN = 0.56
    while LEN > 0.2 and max((HX @ axis(k / 30, length=LEN)).z + rad(k / 30) for k in range(31)) > R.height * 1.1 - 0.012:
        LEN -= 0.005

    def cone(u, off=0.0):
        """(point, normal, tangent) on the seam side of the cone (front, a little to its left; the outside of the curl higher up)."""
        f = phi(u)
        m = (SIDE * 0.8 + (UP * math.sin(f) - BEND * math.cos(f)) * 0.6).normalized()
        return HX @ (axis(u) + m * (rad(u) + off)), HXR @ m, HXR @ (UP * math.cos(f) + BEND * math.sin(f))
    us = (0.0, 0.1, 0.2, 0.3, 0.4, 0.5, 0.6, 0.7, 0.78, 0.86, 0.93, 1.0)
    tip = [M.tube("cone", [HX @ axis(u) for u in us], [rad(u) for u in us], sides=12, color=PURPLE, mat="plush", cap_offset=0.0),
           M.tube("cone_seam", [cone(u, 0.001)[0] for u in (0.03, 0.15, 0.3, 0.45, 0.6, 0.74)], 0.0045, sides=3, color=PURPLE_D, mat="flat")]
    items = []
    for u in (0.1, 0.2, 0.3, 0.4, 0.5, 0.6, 0.7):
        p, n, t = cone(u, 0.003)
        items.append((p, t.cross(n), n))
    tip.append(M.D.stitches("cone_st", items, length=0.036, width=0.009, height=0.0045, color=THREAD))
    R.part("hat_tip", tip, HX @ BASE, parent="hat", soft=dict(dir=HXR @ UP, rigid=0.02, loops=6), ground=False)

    poses = [
        ("rest", {}),
        ("hop: lands", {"pole": [("move", (0, 0, 0.1)), ("x", -12)], "body": [("move", (0, -0.06, 0)), ("x", 16)], "head": [("x", -12)],
                        "arm_l": [("z", -38)], "arm_r": [("z", 38)], "hat": [("x", 10)], "tuft": [("x", 10)]}),
        ("wind-up", {"pole": [("x", 6)], "body": [("move", (0, -0.07, 0)), ("x", -30)], "head": [("x", 16), ("y", 20)], "hat": [("x", -14)],
                     "arm_l": [("y", 55), ("z", 25)], "arm_r": [("y", -55), ("z", -25)], "brow_l": [("z", 22)], "brow_r": [("z", -22)],
                     "cuff_l": [("x", 25)], "cuff_r": [("x", 25)]}),
        ("charge", {"pole": [("x", 27)], "body": [("x", 8)], "head": [("x", -28)], "hat": [("x", -16)], "arm_l": [("y", 62), ("z", 12)],
                    "arm_r": [("y", -62), ("z", -12)]}),
        ("dizzy", {"pole": [("z", 10)], "body": [("x", 12), ("z", 12)], "head": [("z", -22), ("x", -10), ("y", 25)],
                   "hat": [("move", (0.03, -0.035, 0.05)), ("x", 22), ("z", 12)], "pupil_l": [("z", 100)], "pupil_r": [("z", -70)],
                   "arm_l": [("z", -30)], "arm_r": [("z", -30)]}),
        ("hit", {"pole": [("x", 22)], "body": [("x", -24)], "head": [("x", -28), ("z", 10)], "hat": [("move", (0, 0.1, -0.02)), ("x", -30)],
                 "arm_l": [("z", 55), ("y", -20)], "arm_r": [("z", -55), ("y", 20)], "brow_l": [("z", -20), ("move", (0, 0.02, 0))],
                 "brow_r": [("z", 20), ("move", (0, 0.02, 0))]}),
    ]
    return R.finish(ao_radius=0.2, ao_strength=0.9, poses=poses, closeups=[("face", 2.4, tuple(HC))])


# ====================================================================== CANDY-CORN GREMLIN
def gremlin():
    """A glossy candy-corn gremlin: the whole candy (yellow base, orange middle, cream top) is one
    rounded part, `head`, standing on two little licorice legs with clawed purple paws. It turns,
    leans and waddles about its hips, so no seam opens anywhere on the candy. Big ears with a peach
    inside, brows, big glossy brown eyes with a lash line, a button nose, blush with scratch marks,
    two fangs, a dark mouth that a sliding chin plate (`jaw`) opens and closes, a tongue that grows
    out of it, skinny licorice arms with three-clawed hands, and the cream tip that curls over: one
    soft part.

    Part tree: body (meshless hip joint) > head > tip, ear_l/r, brow_l/r, eye_l/r > pupil_l/r, nose,
    jaw > tongue, arm_l/r > forearm_l/r > hand_l/r; body > leg_l/r > shin_l/r > foot_l/r.
    The mouth: the dark cavity is painted on the candy under the jaw plate. The plate's top edge is
    the upper lip line at rest (mouth shut); the animator turns the plate about its pivot (the centre
    of the face's vertical curvature) so it slides down the face and uncovers the mouth: a thin smile
    first, the whole D-shaped mouth wide open."""
    K.reset()
    YEL, YEL_S = "candy_corn_yellow", "golden_leaf"
    ORA, ORA_S = "candy_orange", "pumpkin_dark"   # pumpkin (#f07b1f) went peach under the key light: its green clips first
    WHT, WHT_S = "bone", "bone_shade"
    LIC, CLAW = "licorice_black", "bat_fur"
    PUR, PUR_S, TOE_CLAW = "witch_purple", "hat_violet", "bat_belly"
    VIN = "vinyl"    # the licorice limbs and paws
    SUG = "plush"    # the candy: a fine sugary grain (vinyl read as crumpled cellophane on the big bands)
    UP = V((0, 0, 1))
    R = M.Rig("monster_gremlin", height=0.8, radius=0.32)

    # ------------------------------------------------------------ the candy (the part called head)
    BOUND, WHITE = 0.30, 0.648   # band boundaries: yellow below, orange between, cream above
    PROF = [(0.0, 0.145), (0.09, 0.148), (0.165, 0.158), (0.222, 0.173), (0.258, 0.192), (0.282, 0.215), (0.296, 0.24),
            (0.302, 0.265), (0.303, 0.285), (0.301, BOUND), (0.297, 0.33), (0.291, 0.36), (0.284, 0.39), (0.276, 0.42),
            (0.268, 0.45), (0.258, 0.48), (0.246, 0.51), (0.232, 0.54), (0.215, 0.57), (0.195, 0.60), (0.172, 0.63),
            (0.157, WHITE), (0.137, 0.668), (0.113, 0.69), (0.085, 0.71), (0.05, 0.725), (0.0, 0.732)]

    def band(z):
        return 0 if z < BOUND - 1e-6 else (1 if z < WHITE - 1e-6 else 2)

    def candy(segs, name="candy"):
        prof = [(r, z, band(z)) for r, z in PROF]
        ob = M.band_lathe(name, prof, segs, colors=(YEL, ORA, WHT), mats=(SUG,), project="sphere", center=(0, 0, 0.42))
        return ob
    ctree = M.proxy_tree(lambda: candy(96, "candy_hi"))

    def hd(az):
        a = math.radians(az)
        return V((math.sin(a), -math.cos(a), 0.0))

    def hs(az, y, off=0.0):
        """(point, normal) on the candy at azimuth az (degrees, 0 = front, + toward its left) and height y."""
        d = hd(az)
        loc, nrm, _, _ = ctree.ray_cast(V((0, 0, y)) + d * 2.0, -d, 4.0)
        return loc + nrm * off, nrm
    head = [candy(28)]

    # cheeks: blush with three scratch marks
    for s in (-1, 1):
        p, n = hs(s * 43, 0.442)
        head.append(M.cheek(f"cheek{s}", p, n, 0.043, 0.028, color="cheek_pink", roll=-14 * s))
        side = V((math.cos(math.radians(43)), math.sin(math.radians(43)), 0)) * s
        side = (side - n * side.dot(n)).normalized()
        upt = (UP - n * UP.dot(n)).normalized()
        items = []
        for k in (-1, 0, 1):
            q, qn = hs(s * (43 + k * 4.2), 0.442 + 0.002 * k, 0.0025)
            items.append((q, (upt * 0.75 - side * 0.55 * s).normalized(), qn))
        head.append(M.D.stitches(f"scratch{s}", items, length=0.022, width=0.0055, height=0.0028, color=ORA_S))

    # the mouth cavity, painted on the candy; the jaw plate covers it at rest
    MW, JW, CD = 22.0, 27.0, 0.086   # mouth half-width (degrees), chin plate half-width, deepest opening (m)

    def top(az):
        """The upper lip line: a smile, corners up."""
        return 0.43 + 0.022 * (az / MW) ** 2

    def cdepth(az):
        return CD * math.sqrt(max(0.0, 1.0 - (az / MW) ** 2)) + 0.004 if abs(az) <= MW else 0.0

    def cavity(u, v):
        a = (u * 2 - 1) * MW
        return hs(a, top(a) - v * cdepth(a), 0.0004)[0]   # (the jaw plate's thinnest part is 1.6 mm proud: stay well under it)
    # (flip: u runs to its left and v down, so the patch would face into the candy and be culled)
    # dark red, with a darker throat low in the middle so a wide-open mouth has depth
    # (final fixes) its own part `mouth` (under the head), so the animator can make it matte and rimless
    # (gloss -2): on the candy it took the candy's sheen and rim and read as a flat grey decal side on
    mouth_cav = M.patch("cavity", cavity, 12, 5, color="mouth_dark", flip=True,
                        paint=lambda i, j: ("licorice_black", "flat") if j >= 2 and 3 <= i <= 8 else None)
    # two fangs hanging from the upper lip, curving a little forward
    for s in (-1, 1):
        a = s * 8.5
        p, n = hs(a, top(a) + 0.007, -0.002)
        down = (-UP + n * 0.42).normalized()
        fang = M.cone(f"fang{s}", 0.036, 0.0105, segs=7, color="trim_white", bend=0.18)
        M.put(fang, p, down, up=tuple(n))
        head.append(fang)
    # shoulder sockets: sunk caps the arms' ball joints sit in
    shoulders = {}
    for side, s in (("l", 1), ("r", -1)):
        p, n = hs(s * 56, 0.335)
        sh = p - n * 0.014
        shoulders[side] = (sh, n)
        head.append(M.blob(f"socket{side}", sh, (0.021, 0.021, 0.021), segs=8, rings=5, color=LIC))
    R.part("body", [], G(0, 0.2, 0))
    R.part("head", head, G(0, 0.235, 0), parent="body")
    R.part("mouth", [mouth_cav], hs(0, top(0))[0], parent="head", ground=False)

    # ------------------------------------------------------------ the tip: one soft part that curls over to its left and back
    BEND = G(0.93, 0, -0.36).normalized()
    BASE = G(0, 0.632, -0.004)
    phi = lambda u: 4.1 * u ** 1.85
    rad = lambda u: 0.158 * (1.0 - u) ** 1.55 + 0.027 * u ** 2

    def axis(u, length, steps=48):
        p = BASE.copy()
        for k in range(steps):
            f = phi((k + 0.5) / steps * u)
            p += (UP * math.cos(f) + BEND * math.sin(f)) * (length * u / steps)
        return p
    LEN = 0.5
    while LEN > 0.15 and max(axis(k / 40, LEN).z + rad(k / 40) for k in range(41)) > R.height * 1.1 - 0.008:
        LEN -= 0.004
    us = (0.0, 0.07, 0.14, 0.22, 0.3, 0.38, 0.46, 0.54, 0.62, 0.7, 0.77, 0.84, 0.9, 0.95, 1.0)
    tip = M.tube("tipc", [axis(u, LEN) for u in us], [rad(u) for u in us], sides=14, color=WHT, mat=SUG, cap_start=False, cap_offset=0.9)
    pin = M.blob("tippin", BASE, (0.02, 0.02, 0.02), segs=6, rings=3, color=WHT)   # inside the candy: a vertex at the pin
    R.part("tip", [tip, pin], BASE, parent="head", soft=dict(dir=G(0, 1, 0), rigid=0.03, loops=6), ground=False)

    # ------------------------------------------------------------ ears: big, pointed, a peach inside
    for side, s in (("l", 1), ("r", -1)):
        p, n = hs(s * 80, 0.556)
        B = p - n * 0.032
        D = G(s * 0.8, 0.52, -0.16).normalized()
        N0 = G(s * 0.25, 0.0, 1.0)
        N = (N0 - D * N0.dot(D)).normalized()
        Wd = N.cross(D).normalized()
        L = 0.255

        def hw(t):
            return 0.068 * (1.0 - t) ** 0.85 * (1.0 + 1.25 * t * (1.0 - t))

        def ear_out(u, v, B=B, D=D, N=N, Wd=Wd, L=L):
            x = (u - 0.5) * 2.0
            w = hw(v)
            return B + D * (L * v) + N * (-0.045 * v * v + 0.4 * x * x * w) + Wd * (x * w) + UP * (0.02 * v * v)

        def ear_in(u, v, N=N, ear_out=ear_out):
            return ear_out(u, v) - N * (0.024 * (1.0 - v) + 0.005)

        def paint(i, j):
            return ("candy_peach", SUG) if 1 <= i <= 4 and 1 <= j <= 6 else None
        ear = M.shell(f"ear{side}", ear_out, 6, 9, inner=ear_in, color=ORA, mat=SUG, paint=paint, rim_color=ORA)
        R.part(f"ear_{side}", [ear], B, parent="head", ground=False, ao_from=("head",))

    # ------------------------------------------------------------ brows: thick dark arcs, one part each
    for side, s in (("l", 1), ("r", -1)):
        pts = []
        for i in range(5):
            x = i / 4                       # 0 at the inner end, 1 at the outer
            az = s * (12.0 + 24.0 * x)
            pts.append(hs(az, 0.592 + 0.018 * x + 0.012 * math.sin(math.pi * x), 0.006)[0])
        pc, nc = hs(s * 24.0, 0.608)
        brow = M.tube(f"brow{side}", pts, [0.0075, 0.0118, 0.0128, 0.011, 0.0065], sides=6, color="choco_dark", mat="flat", cap_offset=0.8)
        R.part(f"brow_{side}", [brow], pc, parent="head", z_out=nc, ground=False)

    # ------------------------------------------------------------ eyes: big glossy brown eyes with a lash line
    EW, EH = 0.05, 0.058
    for side, s in (("l", 1), ("r", -1)):
        p, n = hs(s * 24.5, 0.528)
        out = V((n.x, n.y, 0)).normalized()
        en = (n + out * 0.9).normalized()           # looks straight out, not up the slope
        lash = []
        for k in range(8):
            a = math.radians(14 + 152 * k / 7)
            lash.append(V((s * EW * 1.04 * math.cos(a), EH * 1.04 * math.sin(a), 0.003)))
        lash = [V((s * EW * 1.3, EH * 0.62, 0.002))] + lash   # the outer flick, then the line from outer to inner
        lash_ob = M.tube(f"lash{side}", lash, lambda t: 0.0045 + 0.0035 * math.sin(math.pi * min(1.0, t * 1.15)), sides=5, color="ink",
                         mat="flat", cap_offset=0.7)
        M.eye(R, f"eye_{side}", f"pupil_{side}", p - n * 0.006, en, EW, EH, parent="head", depth=EW * 0.3, sclera="trim_white",
              iris="iris_brown", iris_r=0.74, iris2="glow_amber_dim", iris2_r=0.52, pupil="ink", pupil_r=0.31, look=(-s * 0.07, -0.05),
              eye_extra=[lash_ob])

    # ------------------------------------------------------------ nose: a little button with nostrils
    p, n = hs(0, 0.47)
    upt = (UP - n * UP.dot(n)).normalized()
    nose = [M.blob("nosebud", (0, 0, 0), (0.026, 0.019, 0.017), segs=10, rings=6, color=ORA, mat=SUG)]
    M.put(nose[0], p + n * 0.003, n, up=tuple(upt))
    for s in (-1, 1):
        q = p + n * 0.013 - upt * 0.006 + V((s * 0.0105, 0, 0))
        nose.append(M.blob(f"nostril{s}", q, (0.0055, 0.004, 0.0045), segs=6, rings=4, color=ORA_S))
    R.part("nose", nose, p, parent="head", ground=False)

    # ------------------------------------------------------------ jaw: the chin plate that slides down to open the mouth
    def jtop(az):
        a = abs(az)
        if a <= MW:
            return top(az)
        return top(MW) - 0.006 * (a - MW) / (JW - MW)

    def jbot(az):
        """The plate's lower edge: a U just below the mouth, so a grin does not drag it over the yellow."""
        a = abs(az)
        if a <= MW:
            return max(top(az) - cdepth(az) - 0.014, BOUND + 0.008)
        return top(MW) - 0.018 + 0.006 * (a - MW) / (JW - MW)

    def jaw_pt(u, v, off):
        a = (u * 2 - 1) * JW
        t = jtop(a)
        return hs(a, t + (jbot(a) - t) * v, off)[0]

    def jaw_off(u, v):
        x = abs(u * 2 - 1)
        return 0.0016 + 0.0024 * (1.0 - v) ** 2.5 * (1.0 - x ** 4)
    chin = M.shell("chin", lambda u, v: jaw_pt(u, v, jaw_off(u, v)), 16, 6, inner=lambda u, v: jaw_pt(u, v, -0.012), color=ORA, mat=SUG)
    # Its outer face takes the candy's own normals (custom normals, via the kit's wave_n attribute), so
    # the toon bands run on across the plate's edge instead of outlining it as a separate panel.
    attr = chin.data.attributes.new(M.WAVE_ATTR, "FLOAT_VECTOR", "POINT")
    for v in chin.data.vertices:
        loc, nrm, _, dist = ctree.find_nearest(chin.matrix_world @ v.co)
        if nrm is not None and dist < 0.006 and v.normal.dot(nrm) > 0.3:
            attr.data[v.index].vector = nrm
    jaw = [chin]
    # the jaw's pivot: the centre of the face's vertical curvature, so turning it slides the plate down the face
    pa, pb, pc_ = (b2 for b2 in (hs(0, 0.30)[0], hs(0, 0.37)[0], hs(0, 0.45)[0]))
    ya, za, yb, zb, yc, zc = pa.z, -pa.y, pb.z, -pb.y, pc_.z, -pc_.y
    dd = 2 * (ya * (zb - zc) + yb * (zc - za) + yc * (za - zb))
    cy = ((ya * ya + za * za) * (zb - zc) + (yb * yb + zb * zb) * (zc - za) + (yc * yc + zc * zc) * (za - zb)) / dd
    cz = ((ya * ya + za * za) * (yc - yb) + (yb * yb + zb * zb) * (ya - yc) + (yc * yc + zc * zc) * (yb - ya)) / dd
    JP = G(0, cy, cz)
    print(f"GREMLIN jaw pivot (game) y={cy:.4f} z={cz:.4f} radius={math.hypot(ya - cy, za - cz):.4f}")
    R.part("jaw", jaw, JP, parent="head", ground=False, ao_from=("head",))

    # tongue: lolls over the lower lip; the animator grows it out of the mouth as the jaw opens
    TW = 9.5
    LIP = top(0)

    def tongue_pt(u, v, off0):
        x = (u * 2 - 1)
        w = TW * (0.62 + 0.38 * math.sin(math.pi * 0.5 * min(v / 0.55, 1.0)))
        if v > 0.7:
            w *= math.sqrt(max(0.0, 1.0 - ((v - 0.7) / 0.3) ** 2)) * 0.9 + 0.1
        a = x * w
        off = off0 + 0.004 + 0.019 * v ** 2.4 - 0.0022 * math.exp(-(x / 0.3) ** 2) * (1.0 - v * 0.5)
        return hs(a, LIP + 0.026 - 0.04 * v, off)[0]
    tongue = M.shell("tonguebody", lambda u, v: tongue_pt(u, v, 0.0), 6, 8, inner=lambda u, v: tongue_pt(u, v, -0.007), color="tongue",
                     mat="flat", rim_color="cheek_pink")
    TP = hs(0, LIP + 0.026, 0.004)[0]
    R.part("tongue", [tongue], TP, parent="jaw", soft=dict(dir=G(0, -1, 0.25), rigid=0.006, loops=6), ground=False)

    # ------------------------------------------------------------ arms: skinny licorice, three claws and a thumb (IK chains)
    for side, s in (("l", 1), ("r", -1)):
        S, n = shoulders[side]
        E = S + G(s * 0.075, -0.085, 0.052)
        W = E + G(-s * 0.012, -0.03, 0.105)
        upper = [M.limb(f"uarm{side}", S, E, 0.0235, r_end=0.021, flare_a=1.1, flare_b=1.12, sides=8, color=LIC, mat=VIN, waist=0.93)]
        R.part(f"arm_{side}", upper, S, parent="head", ball=0.03, ball_color=LIC, ground=False)
        fore = [M.limb(f"farm{side}", E, W, 0.0205, r_end=0.0185, flare_a=1.1, flare_b=1.15, sides=8, color=LIC, mat=VIN, waist=0.93)]
        R.part(f"forearm_{side}", fore, E, parent=f"arm_{side}", ball=0.025, ball_color=LIC, ball_segs=7, ground=False)
        d = (W - E).normalized()
        u = (UP - d * UP.dot(d)).normalized()
        sd = d.cross(u).normalized()                 # toward its right when the hand points forward
        inner = sd if s > 0 else -sd                 # toward the middle
        palm = M.blob(f"palm{side}", (0, 0, 0), (0.031, 0.019, 0.03), segs=9, rings=5, color=LIC, mat=VIN)
        M.put(palm, W + d * 0.024, d, up=tuple(u))
        hand = [palm]
        for k, spread in enumerate((-1.0, 0.0, 1.0)):
            b0 = W + d * 0.036 + sd * (spread * 0.019) + u * 0.004
            b1 = b0 + (d + sd * spread * 0.32).normalized() * 0.027 + u * 0.004
            b2 = b1 + (d * 0.5 - u * 0.86).normalized() * 0.021
            hand.append(M.tube(f"fing{side}{k}", [b0, b1, b2], [0.0108, 0.0098, 0.0086], sides=6, color=LIC, mat=VIN, cap_offset=0.6))
            cl = M.cone(f"claw{side}{k}", 0.022, 0.0082, segs=5, color=CLAW, mat=VIN, bend=0.3)
            M.put(cl, b2 - (d * 0.5 - u * 0.86).normalized() * 0.003, (d * 0.15 - u * 1.0).normalized(), up=tuple(-d))
            hand.append(cl)
        t0 = W + d * 0.016 + inner * 0.022 + u * 0.004
        t1 = t0 + (d * 0.75 + inner * 0.65).normalized() * 0.024
        hand.append(M.tube(f"thumb{side}", [t0, t1], [0.0105, 0.0092], sides=6, color=LIC, mat=VIN, cap_offset=0.6))
        cl = M.cone(f"tclaw{side}", 0.018, 0.0075, segs=5, color=CLAW, mat=VIN, bend=0.3)
        M.put(cl, t1, (d * 0.6 + inner * 0.3 - u * 0.75).normalized(), up=tuple(-inner))
        hand.append(cl)
        R.part(f"hand_{side}", hand, W, parent=f"forearm_{side}", ball=0.021, ball_color=LIC, ball_segs=7, ground=False)
        R.ik_chain(f"arm_{side}", f"forearm_{side}", f"hand_{side}")

    # ------------------------------------------------------------ legs and big clawed purple paws (IK chains)
    def flat(cz, rz):
        k = cz / rz
        return lambda c: V((c.x, c.y, c.z if c.z > 0 else c.z * k))
    for side, s in (("l", 1), ("r", -1)):
        x = s * 0.108
        H, Kn, A = G(x, 0.2, 0.0), G(x + s * 0.006, 0.13, 0.03), G(x + s * 0.01, 0.064, 0.0)
        R.part(f"leg_{side}", [M.limb(f"thigh{side}", H, Kn, 0.024, r_end=0.022, flare_a=1.0, flare_b=1.1, sides=7, color=LIC, mat=VIN)], H,
               parent="body", ball=0.03, ball_color=LIC, ball_segs=7)
        R.part(f"shin_{side}", [M.limb(f"shin{side}", Kn, A, 0.022, r_end=0.021, flare_a=1.1, flare_b=1.15, sides=7, color=LIC, mat=VIN)], Kn,
               parent=f"leg_{side}", ball=0.024, ball_color=LIC, ball_segs=7)
        fx = x + s * 0.012
        foot = [M.blob(f"pad{side}", G(fx, 0.036, 0.034), (0.07, 0.08, 0.042), segs=12, rings=7, color=PUR, mat=VIN, warp=flat(0.036, 0.042))]
        for k, dx in enumerate((-0.047, 0.0, 0.047)):
            tz = 0.098 if dx == 0 else 0.088
            foot.append(M.blob(f"toe{side}{k}", G(fx + dx, 0.03, tz), (0.03, 0.036, 0.031), segs=8, rings=5, color=PUR, mat=VIN,
                               warp=flat(0.03, 0.031)))
            cl = M.cone(f"tclaw{side}{k}", 0.026, 0.0115, segs=5, color=TOE_CLAW, mat=VIN, bend=0.35)
            M.put(cl, G(fx + dx, 0.026, tz + 0.026), G(0, -0.35, 1.0).normalized(), up=tuple(G(0, -1, 0)))
            foot.append(cl)
        R.part(f"foot_{side}", foot, A, parent=f"shin_{side}", ball=0.023, ball_color=LIC, ball_segs=7)
        R.ik_chain(f"leg_{side}", f"shin_{side}", f"foot_{side}")

    poses = [
        ("rest", {}),
        ("open, rub hands", {"jaw": [("x", 5)], "arm_l": [("y", -40), ("x", 20)], "forearm_l": [("x", -40)], "arm_r": [("y", 40), ("x", 20)],
                             "forearm_r": [("x", -40)], "tongue": [("move", (0, -0.004, 0.006))]}),
        ("wind-up crouch", {"body": [("move", (0, -0.05, 0))], "head": [("x", -14)], "jaw": [("x", 6)], "leg_l": [("x", -55), ("z", 12)],
                            "shin_l": [("x", 95)], "foot_l": [("x", -40)], "leg_r": [("x", -55), ("z", -12)], "shin_r": [("x", 95)],
                            "foot_r": [("x", -40)], "arm_l": [("z", 60), ("x", -30)], "arm_r": [("z", -60), ("x", -30)],
                            "brow_l": [("z", 20)], "brow_r": [("z", -20)], "ear_l": [("y", 35)], "ear_r": [("y", -35)]}),
        ("run, ears back", {"head": [("x", 16), ("z", 9)], "leg_l": [("x", -60)], "shin_l": [("x", 70)], "leg_r": [("x", 35)], "shin_r": [("x", 50)],
                            "ear_l": [("y", 50)], "ear_r": [("y", -50)], "arm_l": [("x", -50)], "arm_r": [("x", 50)]}),
        ("hit", {"head": [("x", -20), ("z", -14)], "arm_l": [("z", 100)], "arm_r": [("z", -100)], "forearm_l": [("x", -50)],
                 "ear_l": [("y", -30), ("z", 30)], "ear_r": [("y", 30), ("z", -30)], "jaw": [("x", 3.5)], "leg_r": [("x", -40)], "shin_r": [("x", 70)]}),
        ("splits, twist", {"body": [("move", (0, -0.08, 0))], "head": [("y", 35)], "leg_l": [("z", 65)], "leg_r": [("z", -65)],
                           "foot_l": [("z", -50)], "foot_r": [("z", 50)], "arm_l": [("x", -150)], "arm_r": [("x", 70), ("z", -40)]}),
    ]
    R.shade({YEL: YEL_S}, ao_cut=0.0, down=-0.55, parts=["head"])
    R.shade({PUR: PUR_S}, ao_cut=0.5, down=-0.6, parts=["foot_l", "foot_r"])
    return R.finish(ao_radius=0.18, ao_strength=0.85, poses=poses, closeups=[("face", 2.2, tuple(G(0, 0.47, 0.15)))])


# ====================================================================== TOADSTOOL IMP
def toadstool():
    """A toy toadstool imp: a fat cream stalk (the body, with the face) on two round feet, under a
    heavy red cap with raised cream spot pads, cream gills underneath and a two-leaf sprout on top.
    The cap sits tipped back a little (its identity) and is its own part on a spring: it lags, bobbles,
    tips back to peek, inflates, slams down, drops over the eyes and spins off like a coin.

    Part tree: body (meshless hip joint) > head (the stalk: belly, face, blush, lip line)
        > cap (dome, rolled rim, spot pads, gills) > sprout (soft: stem and both leaves, one mesh)
        > eye_l/r > pupil_l/r, brow_l/r, mouth (the open mouth; scaled shut onto the lip line),
          cheek_l/r (blush pads that puff out), alt_squint_l/r (shut-eye chevrons),
          arm_l/r > hand_l/r
      body > foot_l/r.
    The stalk runs up into the closed cap, so the cap can tip, lift and drop without a gap."""
    K.reset()
    CRM, CRM_S, CRM_D = "ivory", "ivory_shade", "ivory_deep"     # the stalk (a pale cream clips under the key light)
    RED, RED_S = "toad_red", "leaf_maroon"                        # the cap: leaf_crimson went salmon pink in the engine
    SPOT, SPOT_S = "bone", "bone_shade"
    GILL, GILL_BASE = "bone_tan", "ivory_shade"
    LEAF, LEAF_D, STEM = "apple_green", "vine", "stem_green"
    SOFT = "plush"
    UP = V((0, 0, 1))
    R = M.Rig("monster_toadstool", height=1.0, radius=0.36)

    # ------------------------------------------------------------ the stalk (the part called head)
    STALK = [(0.0, 0.045), (0.075, 0.048), (0.12, 0.058), (0.152, 0.075), (0.174, 0.1), (0.188, 0.13), (0.196, 0.165),
             (0.198, 0.2), (0.196, 0.24), (0.19, 0.28), (0.183, 0.32), (0.176, 0.36), (0.169, 0.4), (0.162, 0.44),
             (0.156, 0.48), (0.151, 0.52), (0.147, 0.56), (0.142, 0.6), (0.137, 0.65), (0.128, 0.7), (0.105, 0.745),
             (0.06, 0.772), (0.0, 0.778)]

    def stalk(segs, name="stalk"):
        ob = M.lathe(name, STALK, segs, color=CRM, mat=SOFT, project="cyl", center=(0, 0, 0.38))
        F.scale_uv(ob, 1.6)
        return ob
    stree = M.proxy_tree(lambda: stalk(96, "stalk_hi"))

    def hs(az, y, off=0.0):
        """(point, normal) on the stalk at azimuth az (degrees, 0 = front, + toward its left) and height y."""
        a = math.radians(az)
        d = V((math.sin(a), -math.cos(a), 0.0))
        loc, nrm, _, _ = stree.ray_cast(V((0, 0, y)) + d * 2.0, -d, 4.0)
        return loc + nrm * off, nrm
    head = [stalk(28)]

    # the lip line: a thin dark smile, corners up into little dimples; the open mouth hangs under it
    MW, MTOP, MCURVE, MDEEP = 15.0, 0.345, 0.006, 0.05

    def top(az):
        return MTOP + MCURVE * (az / MW) ** 2
    lip = [hs(a, top(a) + (0.004 * ((abs(a) - MW) / 4.0) if abs(a) > MW else 0.0), 0.0018)[0]
           for a in (-MW - 4, -MW, -MW * 0.6, -MW * 0.25, 0.0, MW * 0.25, MW * 0.6, MW, MW + 4)]
    head.append(M.tube("lipline", lip, lambda t: 0.0022 + 0.0016 * math.sin(math.pi * t), sides=5, color="choco_dark",
                       mat="flat", cap_offset=0.7))
    # shoulder sockets: sunk caps the arms' ball joints sit in
    shoulders = {}
    for side, s in (("l", 1), ("r", -1)):
        p, n = hs(s * 80, 0.33)
        sh = p - n * 0.016
        shoulders[side] = (sh, n)
        head.append(M.blob(f"socket{side}", sh, (0.024, 0.024, 0.024), segs=8, rings=5, color=CRM, mat=SOFT))
    R.part("body", [], G(0, 0.1, 0))
    R.part("head", head, G(0, 0.1, 0), parent="body", ao_from=("cap",))

    # ------------------------------------------------------------ the cap: a heavy dome tipped back, rolled rim
    P = G(0, 0.625, 0)
    TILT = math.radians(12.0)
    MCAP = M.Matrix.Translation(P) @ M.Matrix.Rotation(-TILT, 4, "X")   # tipped back (top toward +Y, the back)
    CAP = [(0.0, 0.06, 0), (0.08, 0.05, 0), (0.15, 0.025, 0), (0.21, 0.0, 0), (0.27, -0.026, 0), (0.32, -0.047, 0),
           (0.36, -0.062, 0), (0.395, -0.072, 0), (0.42, -0.076, 1), (0.437, -0.072, 1), (0.45, -0.06, 1), (0.458, -0.04, 1),
           (0.461, -0.015, 1), (0.458, 0.02, 1), (0.448, 0.06, 1), (0.43, 0.105, 1), (0.405, 0.15, 1), (0.37, 0.195, 1),
           (0.325, 0.235, 1), (0.27, 0.262, 1), (0.205, 0.286, 1), (0.135, 0.301, 1), (0.065, 0.31, 1), (0.0, 0.312, 1)]
    UNDER = [(r, z) for r, z, b in CAP[:9]]

    def under_z(r):
        for (r0, z0), (r1, z1) in zip(UNDER[:-1], UNDER[1:]):
            if r <= r1:
                return z0 + (z1 - z0) * (r - r0) / max(r1 - r0, 1e-9)
        return UNDER[-1][1]

    def cap_mesh(segs, name):
        ob = M.band_lathe(name, CAP, segs, colors=(GILL_BASE, RED), mats=(SOFT,), project="sphere", center=(0, 0, 0.1))
        ob.data.transform(MCAP)
        ob.data.update()
        return ob
    ctree = M.proxy_tree(lambda: cap_mesh(128, "cap_hi"))
    cap = [cap_mesh(32, "capdome")]
    CROT = MCAP.to_3x3()

    def cl(x, y, z):
        """A point in the cap's own (untipped) frame, Blender axes about the pivot, to world."""
        return MCAP @ V((x, y, z))

    # spot pads: one big on the crown, a ring of medium ones, small ones toward the rim (az, lat, radius)
    SPOTS = [(8, 57, 0.112), (172, 55, 0.09), (64, 33, 0.078), (-58, 37, 0.082), (120, 25, 0.068), (-122, 25, 0.074),
             (27, 14, 0.05), (-31, 12, 0.044), (90, 7, 0.04), (-88, 10, 0.042), (150, 7, 0.042), (-160, 13, 0.05)]
    for k, (az, lat, r) in enumerate(SPOTS):
        a, la = math.radians(az), math.radians(lat)
        d = CROT @ V((math.cos(la) * math.sin(a), -math.cos(la) * math.cos(a), math.sin(la)))
        c = cl(0, 0, 0.02)
        loc, nrm, _, _ = ctree.ray_cast(c + d * 2.0, -d, 4.0)
        cap.append(M.pad(f"spot{k}", ctree, loc, nrm, r, 0.0095 if r > 0.06 else 0.007, segs=14 if r > 0.06 else 10,
                         color=SPOT, edge_color=SPOT_S, mat=SOFT))
    # gills: radial fins hanging under the cap, deepest in the middle of their run
    NG = 36
    bm = M.bmesh.new()
    fin_faces = []
    for g in range(NG):
        phi = M.TAU * (g + 0.5) / NG
        rad_dir = V((math.sin(phi), -math.cos(phi), 0.0))
        tan = V((math.cos(phi), math.sin(phi), 0.0))
        rs = (0.15, 0.21, 0.27, 0.33, 0.38, 0.412)
        rows = []
        for i, r in enumerate(rs):
            t = i / (len(rs) - 1)
            depth = 0.004 + 0.014 * math.sin(math.pi * min(1.0, t * 1.05)) ** 0.7
            w = 0.0046 + 0.0016 * math.sin(math.pi * t)
            base = rad_dir * r + V((0, 0, under_z(r) + 0.003))
            rows.append((bm.verts.new(base + tan * w), bm.verts.new(base - tan * w), bm.verts.new(base - V((0, 0, depth)))))
        for (a0, b0, c0), (a1, b1, c1) in zip(rows[:-1], rows[1:]):
            fin_faces.append((bm.faces.new((a0, a1, c1, c0)), tan))
            fin_faces.append((bm.faces.new((c0, c1, b1, b0)), -tan))
        a0, b0, c0 = rows[0]
        fin_faces.append((bm.faces.new((a0, c0, b0)), -rad_dir))
        a1, b1, c1 = rows[-1]
        fin_faces.append((bm.faces.new((a1, b1, c1)), rad_dir))
    bm.normal_update()
    for f, want in fin_faces:
        if f.normal.dot(want) < 0.0:
            f.normal_flip()
    gills = M.D.mesh(bm, "gills", GILL, SOFT, "box", smooth=False)
    gills.data.transform(MCAP)
    gills.data.update()
    cap.append(gills)
    R.part("cap", cap, P, parent="head", ao_from=("head",))

    # ------------------------------------------------------------ the sprout: a stem and two leaves, ONE soft part
    B0 = cl(0, 0, 0.292)
    stem_pts = [cl(0, 0, 0.292), cl(0, -0.003, 0.325), cl(0, 0.0, 0.352), cl(0, 0.007, 0.376)]
    sprout = [M.tube("sstem", stem_pts, [0.017, 0.0155, 0.014, 0.0125], sides=7, color=STEM, mat=SOFT, cap_start=False, cap_offset=0.6)]
    TIP = stem_pts[-1]
    for s in (-1, 1):
        D_ = (CROT @ V((s * 0.8, 0.1, 0.6))).normalized()
        side = (CROT @ V((0, -1.0, 0.0)))
        N_ = D_.cross(side).normalized() * (1 if s > 0 else -1)
        if N_.dot(CROT @ UP) < 0:
            N_ = -N_
        W_ = N_.cross(D_).normalized()
        L_, WID = 0.17, 0.056

        def lw(v):
            return WID * (math.sin(math.pi * min(1.0, v ** 0.85)) ** 0.7) + 0.004 * (1.0 - v)

        def leaf_out(u, v, D_=D_, N_=N_, W_=W_, lw=lw, L_=L_):
            x = (u - 0.5) * 2.0
            w = lw(v)
            return TIP + D_ * (L_ * v) + W_ * (x * w) + N_ * (0.3 * x * x * w - 0.022 * v * v + 0.005)

        def leaf_in(u, v, N_=N_, leaf_out=leaf_out, lw=lw):
            x = (u - 0.5) * 2.0
            return leaf_out(u, v) - N_ * (0.004 + 0.008 * (1.0 - x * x) * min(1.0, lw(v) / WID))

        def paint(i, j):
            return (LEAF_D, SOFT) if i in (2, 3) and 1 <= j <= 5 else None
        sprout.append(M.shell(f"leaf{s}", leaf_out, 6, 7, inner=leaf_in, color=LEAF, mat=SOFT, paint=paint, rim_color=LEAF))
    R.part("sprout", sprout, B0, parent="cap", soft=dict(dir=CROT @ UP, rigid=0.012, loops=6), ground=False)

    # ------------------------------------------------------------ face: eyes, lashes, brows, blush, mouth
    EW, EH, EAZ, EY = 0.05, 0.058, 22.5, 0.43
    for side, s in (("l", 1), ("r", -1)):
        p, n = hs(s * EAZ, EY)
        out = V((n.x, n.y, 0)).normalized()
        en = (n + out * 0.9).normalized()
        lash = []
        for k in range(8):
            a = math.radians(16 + 150 * k / 7)
            lash.append(V((s * EW * 1.05 * math.cos(a), EH * 1.05 * math.sin(a), 0.003)))
        lash = [V((s * EW * 1.32, EH * 0.66, 0.002))] + lash
        lash_ob = M.tube(f"lash{side}", lash, lambda t: 0.0042 + 0.0034 * math.sin(math.pi * min(1.0, t * 1.15)), sides=5,
                         color="ink", mat="flat", cap_offset=0.7)
        M.eye(R, f"eye_{side}", f"pupil_{side}", p - n * 0.006, en, EW, EH, parent="head", depth=EW * 0.3, sclera="trim_white",
              iris="iris_brown", iris_r=0.76, iris2="glow_amber_dim", iris2_r=0.5, pupil="ink", pupil_r=0.32, look=(-s * 0.06, -0.05),
              eye_extra=[lash_ob])
        # brow: a thin brown arc well above the eye
        pts = [hs(s * (9.0 + 25.0 * (i / 4)), 0.513 + 0.008 * (i / 4) + 0.009 * math.sin(math.pi * i / 4), 0.004)[0] for i in range(5)]
        pc, nc = hs(s * 21.0, 0.52)
        brow = M.tube(f"brow{side}", pts, [0.0045, 0.0068, 0.0072, 0.0062, 0.004], sides=6, color="choco_dark", mat="flat", cap_offset=0.8)
        R.part(f"brow_{side}", [brow], pc, parent="head", z_out=nc, ground=False)
        # blush pad: a pink blob sunk into the face; the animator scales it up to puff the cheek
        q, qn = hs(s * 41.0, 0.36)
        ck = M.blob(f"cheekpad{side}", (0, 0, 0), (0.042, 0.028, 0.03), segs=12, rings=6, color="cheek_pink", mat="flat")
        M.put(ck, q - qn * 0.024, qn, up=tuple(UP))
        R.part(f"cheek_{side}", [ck], q - qn * 0.024, parent="head", z_out=qn, ground=False)
        # shut-eye chevron (> <), shown instead of the eye when it is squeezed shut
        base = EAZ - 1.0
        chev = [hs(s * (base + 11.0), EY + 0.024, 0.004)[0], hs(s * (base - 7.0), EY + 0.002, 0.004)[0],
                hs(s * (base + 11.0), EY - 0.018, 0.004)[0]]
        chev_ob = M.tube(f"chev{side}", chev, [0.0045, 0.0062, 0.0045], sides=5, color="ink", mat="flat", cap_offset=0.7)
        R.part(f"alt_squint_{side}", [chev_ob], p, parent="head", ground=False)

    # the open mouth: dark cavity, a row of upper teeth, the tongue; the animator scales it in Y about
    # the lip line (its pivot), so it shuts onto the painted smile
    def cavity(u, v):
        a = (u * 2 - 1) * MW
        depth = MDEEP * math.sqrt(max(0.0, 1.0 - (a / MW) ** 2)) ** 0.8 + 0.003
        return hs(a, top(a) - 0.002 - v * depth, 0.0012)[0]

    def mpaint(i, j):
        if j == 0 and 2 <= i <= 7:
            return ("trim_white", "flat")
        if j >= 3 and 2 <= i <= 7:
            return ("tongue", "flat")
        return None
    mouth = M.patch("mouthcav", cavity, 10, 5, color="mouth_dark", flip=True, paint=mpaint)
    MP, MN = hs(0, top(0), 0.0012)
    R.part("mouth", [mouth], MP, parent="head", z_out=MN, ground=False)

    # ------------------------------------------------------------ arms: stubby, mitten hands
    for side, s in (("l", 1), ("r", -1)):
        S, n = shoulders[side]
        E = S + G(s * 0.064, -0.05, 0.03)
        arm = [M.limb(f"uarm{side}", S, E, 0.039, r_end=0.034, flare_a=1.0, flare_b=1.0, sides=9, color=CRM, mat=SOFT, waist=0.95)]
        R.part(f"arm_{side}", arm, S, parent="head", ball=0.038, ball_color=CRM, ball_segs=9, ground=False)
        d = (E - S).normalized()
        u = (UP - d * UP.dot(d)).normalized()
        mit = M.blob(f"mitt{side}", (0, 0, 0), (0.043, 0.035, 0.046), segs=11, rings=6, color=CRM, mat=SOFT)
        M.put(mit, E + d * 0.03, d, up=tuple(u))
        thumb = M.blob(f"thumb{side}", (0, 0, 0), (0.016, 0.015, 0.024), segs=7, rings=4, color=CRM, mat=SOFT)
        fwd = G(0, 0, 1)
        M.put(thumb, E + d * 0.022 + (fwd - d * fwd.dot(d)).normalized() * 0.026 + u * 0.008, (d + u * 0.6).normalized(), up=tuple(fwd))
        R.part(f"hand_{side}", [mit, thumb], E, parent=f"arm_{side}", ball=0.032, ball_color=CRM, ball_segs=8, ground=False)

    # ------------------------------------------------------------ round feet tucked under the belly
    def flat(cz, rz):
        k = cz / rz
        return lambda c: V((c.x, c.y, c.z if c.z > 0 else c.z * k))
    for side, s in (("l", 1), ("r", -1)):
        x = s * 0.088
        foot = [M.blob(f"foot{side}", G(x, 0.043, 0.07), (0.068, 0.092, 0.045), segs=12, rings=7, color=CRM, mat=SOFT,
                       warp=flat(0.043, 0.045))]
        R.part(f"foot_{side}", foot, G(x, 0.07, 0.03), parent="body")

    poses = [
        ("rest", {}),
        ("peek, wave", {"cap": [("x", -16)], "head": [("x", -6)], "arm_l": [("z", 95), ("x", -20)], "hand_l": [("z", 30)]}),
        ("wind-up", {"head": [("x", -10)], "cap": [("x", -8), ("move", (0, 0.04, 0))], "arm_l": [("z", -25), ("x", 30)],
                     "arm_r": [("z", 25), ("x", 30)], "mouth": [("move", (0, 0, 0))]}),
        ("slam", {"head": [("x", 14)], "cap": [("x", 16), ("move", (0, -0.07, 0.02))], "arm_l": [("z", 40)], "arm_r": [("z", -40)],
                  "foot_l": [("move", (0, 0, 0.04))]}),
        ("hit: cap over eyes", {"head": [("x", -12), ("z", 8)], "cap": [("x", 24), ("move", (0, -0.08, 0.03))],
                                "arm_l": [("z", 110), ("x", -40)], "arm_r": [("z", -110), ("x", -40)]}),
        ("waddle", {"head": [("z", 12), ("y", 25)], "cap": [("z", -10)], "foot_l": [("move", (0, 0.04, 0.06)), ("x", -20)],
                    "foot_r": [("move", (0, 0, -0.05))], "arm_l": [("x", 40)], "arm_r": [("x", -40)]}),
    ]
    R.shade({CRM: CRM_S}, ao_cut=0.5, down=-0.55, parts=["head", "arm_l", "arm_r", "hand_l", "hand_r", "foot_l", "foot_r"])
    R.shade({RED: RED_S}, ao_cut=0.0, down=-0.35, parts=["cap"])
    return R.finish(ao_radius=0.2, ao_strength=0.85, poses=poses, closeups=[("face", 2.0, tuple(G(0, 0.47, 0.15)))])


# ====================================================================== GHOST
def ghost():
    """A soft toy sheet ghost: one smooth rounded dome that flares into a bell with deep scalloped
    lobes and a rolled, stitched hem; big glossy ink eyes, pink blush, an open mouth with a tongue;
    two floppy sleeve arms ending in mittens. Plain cool white (silver, darker than the mock so the key
    light and bloom keep its form), bluish shade in the folds and undersides, deeper inside the hem.

    Part tree: body (meshless joint at the pin) > head (the whole sheet: dome, folds, lobed hem, rolled
        edge, stitches; ONE soft mesh pinned at the waist: soft_bend trails the hem, cloth_wave ripples it)
        > eye_l/r > pupil_l/r, brow_l/r, mouth (cavity + tongue, scaled shut onto the lip line),
          cheek_l/r (blush with three strokes; puffs), alt_squint_l/r (> < shut eyes),
          alt_happy_l/r (^ ^ giggling eyes), arm_l/r (soft sleeves with mittens, pinned at the shoulder).
    Everything above the pin (face, shoulders) is rigid; the sheet below it bends."""
    K.reset()
    SH, SH_S, SH_D = "silver", "ghost_cloth", "ghost_deep"   # lit / shade / inside: cool white, faint blue
    SOFT = "plush"
    UP = V((0, 0, 1))
    R = M.Rig("monster_ghost", height=1.3, radius=0.4)
    NL = 8                                   # hem lobes (a lobe centred at the front)
    YC, DOME_A, R0, TOP = 0.84, 0.38, 0.375, 1.22
    PIN = 0.70                               # the bend pin: just under the mouth and the shoulders
    T_HEM = 0.024                            # sheet thickness at the rolled hem
    Y_LOBE, Y_ARCH = 0.012, 0.165            # hem height at a lobe's foot / at the cusp between lobes
    Y_SKIRT, Y_IN = 0.42, 0.5                # where the skirt rows start; how high the inside wall runs
    # columns: ten per lobe, packed toward the cusps so the scallops stay round and the cusps sharp
    U_COLS = (-0.94, -0.8, -0.58, -0.3, 0.0, 0.3, 0.58, 0.8, 0.94, 1.0)

    def rad(y):
        if y >= YC:
            t = min(1.0, (y - YC) / DOME_A)
            return R0 * max(0.0, 1.0 - t * t) ** 0.5
        d = (YC - y) / YC
        return R0 + 0.025 * d + 0.11 * d ** 2.2

    def lobe_u(th):
        """-1..1 across a lobe (0 at its middle, +-1 at the cusps)."""
        w = M.TAU / NL
        x = (th / w + 0.5) % 1.0
        return x * 2.0 - 1.0

    def fold(y):
        t = max(0.0, min(1.0, (0.66 - y) / 0.52))
        return 0.03 * t * t * (3 - 2 * t)

    def hem_y(th):
        u = lobe_u(th)
        return Y_LOBE + (Y_ARCH - Y_LOBE) * (1.0 - max(0.0, 1.0 - u * u) ** 0.55)

    def outer(th, y):
        """Game-space point on the outer sheet at azimuth th (0 = front, + toward its left) and height y."""
        u = lobe_u(th)
        r = rad(y) + fold(y) * (math.cos(math.pi * u) * 0.75 + 0.25 * (1.0 - u * u))
        return V((r * math.sin(th), y, r * math.cos(th)))

    def onormal(th, y, e=1e-3):
        dt = outer(th + e, y) - outer(th - e, y)
        dy = outer(th, y + e) - outer(th, y - e)
        n = dt.cross(dy).normalized()
        rho = V((math.sin(th), 0.0, math.cos(th)))
        return n if n.dot(rho) > 0 else -n

    def gb(p):
        return G(p.x, p.y, p.z)

    def column(th, n_dome, n_mid, n_skirt, n_rim, n_in):
        """The points of one meridian, top to inside: dome rings, middle rows, skirt rows to the hem,
        the rolled edge, the inside wall up to Y_IN. Game coordinates."""
        pts, kinds = [], []
        for k in range(1, n_dome + 1):
            pts.append(outer(th, YC + DOME_A * math.cos(math.pi / 2 * k / n_dome)))
            kinds.append("out")
        for k in range(1, n_mid + 1):
            pts.append(outer(th, YC - (YC - Y_SKIRT) * k / n_mid))
            kinds.append("out")
        hy = hem_y(th)
        for k in range(1, n_skirt + 1):
            pts.append(outer(th, Y_SKIRT + (hy - Y_SKIRT) * k / n_skirt))
            kinds.append("out" if k < n_skirt else "hem")
        H = pts[-1]
        e = 1e-3
        T = (outer(th + e, hem_y(th + e)) - outer(th - e, hem_y(th - e))).normalized()
        N = onormal(th, hy)
        B = T.cross(N).normalized()
        if B.y > 0:
            B = -B
        for k in range(1, n_rim + 1):
            a = math.pi * k / (n_rim + 1)
            pts.append(H - N * (T_HEM / 2) * (1 - math.cos(a)) + B * (T_HEM / 2) * math.sin(a))
            kinds.append("rim_lo" if k <= n_rim // 2 else "rim_hi")
        for k in range(n_in, -1, -1):
            y = Y_IN + (hy - Y_IN) * k / n_in
            pts.append(outer(th, y) - onormal(th, y) * T_HEM)
            kinds.append("in")
        return pts, kinds, (H, T, N, B)

    def sheet(name, ths, n_dome, n_mid, n_skirt, n_rim, n_in, paint=True):
        bm = M.bmesh.new()
        ns = len(ths)
        cols = [column(th, n_dome, n_mid, n_skirt, n_rim, n_in) for th in ths]
        nrow = len(cols[0][0])
        grid = [[bm.verts.new(gb(cols[i][0][k])) for i in range(ns)] for k in range(nrow)]
        pole = bm.verts.new(gb(V((0, TOP, 0))))
        cap = bm.verts.new(gb(V((0, Y_IN + 0.02, 0))))
        groups = {}
        for i in range(ns):
            i2 = (i + 1) % ns
            bm.faces.new((pole, grid[0][i], grid[0][i2]))
            for k in range(nrow - 1):
                f = bm.faces.new((grid[k][i], grid[k + 1][i], grid[k + 1][i2], grid[k][i2]))
                kind = cols[i][1][k + 1]
                if kind in ("rim_hi", "in"):
                    groups.setdefault(SH_S, []).append(f)
            f = bm.faces.new((grid[-1][i], cap, grid[-1][i2]))
            groups.setdefault(SH_D, []).append(f)
        bmesh_ops = M.bmesh.ops
        bmesh_ops.recalc_face_normals(bm, faces=bm.faces)
        extra = [(fs, c, SOFT) for c, fs in groups.items()] if paint else []
        ob = M.D.mesh(bm, name, SH, SOFT, "cyl", smooth=True, center=tuple(G(0, 0.62, 0)), extra=extra)
        return ob, cols

    stree = M.proxy_tree(lambda: sheet("sheet_hi", [M.TAU * i / 240 for i in range(240)], 30, 24, 24, 4, 4, paint=False)[0])

    def hs(az, y, off=0.0):
        """(point, normal) on the outer sheet at azimuth az (degrees, 0 = front, + toward its left), height y."""
        a = math.radians(az)
        d = V((math.sin(a), -math.cos(a), 0.0))
        loc, nrm, _, _ = stree.ray_cast(V((0, 0, y)) + d * 2.0, -d, 4.0)
        return loc + nrm * off, nrm

    ths = [M.TAU / NL * (lobe + u / 2.0) for lobe in range(NL) for u in U_COLS]
    body_ob, cols = sheet("sheet", ths, 9, 6, 9, 3, 3)
    head = [body_ob]

    # stitches along the rolled hem, 3.4 cm up the sheet, spaced evenly along each scallop
    def stitch_line(th):
        hy = hem_y(th)
        e = 1e-4
        T = (outer(th + e, hem_y(th + e)) - outer(th - e, hem_y(th - e))).normalized()
        N = onormal(th, hy)
        B = T.cross(N).normalized()
        if B.y > 0:
            B = -B
        return outer(th, hy) - B * 0.034 + N * 0.0005, N
    items = []
    per = 7
    for lobe in range(NL):
        us = [-0.9 + 1.8 * k / 80 for k in range(81)]
        line = [stitch_line(M.TAU / NL * (lobe + u / 2.0)) for u in us]
        acc = [0.0]
        for a, b in zip(line[:-1], line[1:]):
            acc.append(acc[-1] + (b[0] - a[0]).length)
        for m in range(per):
            want = acc[-1] * (m + 0.5) / per
            k = min(range(len(acc)), key=lambda i: abs(acc[i] - want))
            k = max(1, min(len(line) - 2, k))
            p, N = line[k]
            T = (line[k + 1][0] - line[k - 1][0]).normalized()
            items.append((gb(p), G(T.x, T.y, T.z), G(N.x, N.y, N.z)))
    head.append(M.D.stitches("hemstitch", items, length=0.024, width=0.0055, height=0.003, color=SH_S, sink=0.002))

    # the lip line: a thin smile across the top of the mouth, corners tucked up
    MW, MTOP, MCURVE, MDEEP = 7.4, 0.852, 0.007, 0.086

    def top(az):
        return MTOP + MCURVE * (az / MW) ** 2
    lip = [hs(a, top(a) + (0.004 * ((abs(a) - MW) / 3.0) if abs(a) > MW else 0.0), 0.0018)[0]
           for a in (-MW - 2.5, -MW, -MW * 0.6, -MW * 0.25, 0.0, MW * 0.25, MW * 0.6, MW, MW + 2.5)]
    head.append(M.tube("lipline", lip, lambda t: 0.0021 + 0.0012 * math.sin(math.pi * t), sides=5, color="ink",
                       mat="flat", cap_offset=0.7))
    # a tiny blob hidden inside the sheet at the pin (the verifier wants a vertex near a soft part's pin)
    head.append(M.blob("pinmark", G(0, PIN, 0), (0.02, 0.02, 0.02), segs=6, rings=4, color=SH, mat=SOFT))
    R.part("body", [], G(0, PIN, 0))
    R.part("head", head, G(0, PIN, 0), parent="body",
           soft=dict(dir=G(0, -1, 0), rigid=0.04, loops=6))

    # ------------------------------------------------------------ face: big glossy ink eyes
    EW, EH, EAZ, EY = 0.072, 0.088, 24.5, 0.965
    for side, s in (("l", 1), ("r", -1)):
        p, n = hs(s * EAZ, EY)
        out = V((n.x, n.y, 0)).normalized()
        en = (n + out * 0.6).normalized()
        # ink dome, a very dark brown iris, ink pupil; the glints are a glow cell so the wind-up flare
        # makes them blaze while the dark of the eye stays dark
        M.eye(R, f"eye_{side}", f"pupil_{side}", p - n * 0.007, en, EW, EH, parent="head", depth=EW * 0.3, sclera="ink",
              iris="linen_dark", iris_r=0.8, pupil="ink", pupil_r=0.46, glint="glow_white",
              look=(-s * 0.03, -0.1), segs=18)
        # a short soft brow stroke well above the eye
        pts = [hs(s * (17.0 + 13.0 * (i / 4)), EY + 0.1 + 0.006 * math.sin(math.pi * i / 4) + 0.004 * (i / 4), 0.0035)[0]
               for i in range(5)]
        pc, nc = hs(s * 23.5, EY + 0.105)
        brow = M.tube(f"brow{side}", pts, [0.0026, 0.0042, 0.0046, 0.0038, 0.0024], sides=5, color="ink", mat="flat",
                      cap_offset=0.8)
        R.part(f"brow_{side}", [brow], pc, parent="head", z_out=nc, ground=False)
        # blush: a pink oval sunk into the cheek, three little strokes on it; it puffs when scaled
        q, qn = hs(s * 38.0, 0.84)
        ck = M.blob(f"cheekpad{side}", (0, 0, 0), (0.058, 0.03, 0.046), segs=12, rings=6, color="cheek_pink", mat="flat")
        M.put(ck, q - qn * 0.0255, qn, up=tuple(UP))
        strokes = []
        for k in range(3):
            sp, sn = hs(s * (33.0 + 5.0 * k), 0.843 + 0.002 * k, 0.0058)
            tdir = (G(-s * 0.55, 1.0, 0.0)).normalized()
            strokes.append((sp, tdir, sn))
        st = M.D.stitches(f"blushstroke{side}", strokes, length=0.02, width=0.0045, height=0.0022, color="candy_berry",
                          sink=0.002)
        R.part(f"cheek_{side}", [ck, st], q - qn * 0.0255, parent="head", z_out=qn, ground=False)
        # shut eyes (> <) for a hit, happy arcs (^ ^) for a giggle; both hidden until shown
        base = EAZ
        chev = [hs(s * (base + 7.0), EY + 0.03, 0.004)[0], hs(s * (base - 6.5), EY + 0.001, 0.004)[0],
                hs(s * (base + 7.0), EY - 0.028, 0.004)[0]]
        R.part(f"alt_squint_{side}", [M.tube(f"chev{side}", chev, [0.0055, 0.0078, 0.0055], sides=5, color="ink", mat="flat",
                                              cap_offset=0.7)], p, parent="head", ground=False)
        arc = [hs(s * (base + 8.5 * math.cos(math.pi * i / 6)), EY - 0.012 + 0.03 * math.sin(math.pi * i / 6), 0.004)[0]
               for i in range(7)]
        R.part(f"alt_happy_{side}", [M.tube(f"happy{side}", arc, lambda t: 0.0052 + 0.0026 * math.sin(math.pi * t), sides=5,
                                            color="ink", mat="flat", cap_offset=0.7)], p, parent="head", ground=False)

    # the open mouth: dark cavity, tongue; scaled in Y about the lip line (its pivot) to open and shut
    def cavity(u, v):
        a = (u * 2 - 1) * MW
        depth = MDEEP * math.sqrt(max(0.0, 1.0 - (a / MW) ** 2)) ** 0.75 + 0.003
        return hs(a, top(a) - 0.0015 - v * depth, 0.0012)[0]

    def mpaint(i, j):
        if j >= 3 and 2 <= i <= 7:
            return ("tongue", "flat")
        if j >= 4 and i in (1, 8):
            return ("tongue", "flat")
        return None
    mouth = M.patch("mouthcav", cavity, 10, 6, color="mouth_dark", flip=True, paint=mpaint)
    MP, MN = hs(0, top(0), 0.0012)
    R.part("mouth", [mouth], MP, parent="head", z_out=MN, ground=False)

    # ------------------------------------------------------------ arms: floppy sleeves with mitten hands
    for side, s in (("l", 1), ("r", -1)):
        p, n = hs(s * 62.0, 0.755)
        S = p - n * 0.034
        X = G(s, 0, 0)
        # a broad flat paddle of sheet: wide where it leaves the body, tapering to a mitten with a thumb
        offs = [-0.045, 0.0, 0.045, 0.095, 0.145, 0.19, 0.228, 0.26, 0.284, 0.298]
        rr = [0.096, 0.1, 0.09, 0.076, 0.068, 0.07, 0.071, 0.064, 0.045, 0.015]
        paddle = M.tube(f"paddle{side}", [S + X * o for o in offs], rr, sides=12, color=SH, mat=SOFT, cap_offset=0.6)
        M.bake(paddle)
        flat = M.Matrix.Translation(S) @ M.Matrix.Diagonal((1.0, 0.5, 1.0, 1.0)) @ M.Matrix.Translation(-S)
        M.xform(paddle, flat)
        thumb = M.blob(f"thumb{side}", (0, 0, 0), (0.026, 0.022, 0.036), segs=8, rings=5, color=SH, mat=SOFT)
        tdir = (G(s * 0.55, 1.0, 0.15)).normalized()
        M.put(thumb, S + X * 0.21 + G(0, 0.064, 0.004), tdir, up=tuple(G(0, 0, 1)))
        R.part(f"arm_{side}", [paddle, thumb], S, parent="head", ball=0.066, ball_color=SH, ball_segs=10,
               soft=dict(dir=X, rigid=0.04, loops=6), ground=False)

    poses = [
        ("rest", {}),
        ("peek-a-boo", {"head": [("x", -6)], "arm_l": [("y", -128), ("z", 28)], "arm_r": [("y", 128), ("z", -28)]}),
        ("boo!", {"head": [("x", -8)], "arm_l": [("y", 15), ("z", 60)], "arm_r": [("y", -15), ("z", -60)]}),
        ("drift / lean", {"head": [("x", 12)], "arm_l": [("y", 45), ("z", -25)], "arm_r": [("y", -45), ("z", 25)]}),
        ("hit", {"head": [("z", 14), ("x", -10)], "arm_l": [("z", 85)], "arm_r": [("z", -85)]}),
        ("look, reach", {"head": [("y", 30)], "arm_l": [("y", -70), ("z", -10)], "arm_r": [("y", 70), ("z", 10)]}),
    ]
    R.shade({SH: SH_S}, ao_cut=0.6, down=-0.45, parts=["head", "arm_l", "arm_r"])
    return R.finish(ao_radius=0.2, ao_strength=0.8, poses=poses, closeups=[("face", 2.0, tuple(G(0, 0.93, 0.2)))])


# ====================================================================== GOO SLIME
def slime():
    """A glossy, toy-like goo slime: one smooth gumdrop of lime jelly (a wide base, concave flanks rising
    to a peak) with a curled tip ending in a little ball, a drippy skirt of rounded drips round its foot,
    big glossy eyes with a dark outline, brows, raised cheeks, an open D mouth with teeth and tongue,
    raised bubbles and paler inner swirls. Opaque: the jelly look is faked with gloss, a strong warm rim
    light (set in the animator), paler swirls and highlight streaks, and raised bubbles with glints.

    Part tree: body (meshless joint on the ground) > head (the whole gumdrop, pivot on the ground: squash,
        stretch and shear wobble about its foot) > skirt (the ring of drips, pivot on the ground; spreads
        and ripples by cloth_wave), curl (ONE soft tube pinned inside the peak), eye_l/r > pupil_l/r,
        brow_l/r, cheek_l/r, mouth (cavity + teeth + tongue, scaled open about the lip line),
        alt_squint_l/r (> <), alt_happy_l/r (^ ^), bubble_a..d (raised bubbles that slide on springs),
        alt_goo (hidden: the look of the thrown goo); socket_muzzle in front of the mouth."""
    K.reset()
    GOO, GOO_L, GOO_S, BUB = "goo_lime", "apple_green", "gourd_green", "slime_light"
    MAT = "flat"
    UP = V((0, 0, 1))
    R = M.Rig("monster_slime", height=0.7, radius=0.42)
    # the gumdrop's profile: radius by height (a wide rounded foot, concave flanks into the peak)
    PY = (0.0, 0.025, 0.07, 0.14, 0.23, 0.32, 0.40, 0.47, 0.53, 0.575, 0.6, 0.615, 0.627)
    PR = (0.36, 0.397, 0.408, 0.402, 0.376, 0.326, 0.264, 0.203, 0.152, 0.118, 0.098, 0.08, 0.0)
    TOP = PY[-1]

    def interp(ys, vs, y):
        """Monotone cubic (Hermite with averaged slopes) through (ys, vs)."""
        y = min(max(y, ys[0]), ys[-1])
        k = 0
        while k < len(ys) - 2 and y > ys[k + 1]:
            k += 1
        sl = [(vs[i + 1] - vs[i]) / (ys[i + 1] - ys[i]) for i in range(len(ys) - 1)]

        def tan(i):
            if i == 0:
                return sl[0]
            if i == len(ys) - 1:
                return sl[-1]
            return 0.0 if sl[i - 1] * sl[i] <= 0 else 0.5 * (sl[i - 1] + sl[i])
        h = ys[k + 1] - ys[k]
        t = (y - ys[k]) / h
        h00, h10, h01, h11 = 2 * t ** 3 - 3 * t ** 2 + 1, t ** 3 - 2 * t ** 2 + t, -2 * t ** 3 + 3 * t ** 2, t ** 3 - t ** 2
        return h00 * vs[k] + h10 * h * tan(k) + h01 * vs[k + 1] + h11 * h * tan(k + 1)

    def rad(y):
        return max(0.0, interp(PY, PR, y))

    def body_mesh(name, nc, ys):
        bm = M.bmesh.new()
        ths = [M.TAU * i / nc for i in range(nc)]
        rows = [[bm.verts.new(G(rad(y) * math.sin(th), y, rad(y) * math.cos(th))) for th in ths] for y in ys]
        bot = bm.verts.new(G(0, 0.004, 0))
        top = bm.verts.new(G(0, TOP, 0))
        for i in range(nc):
            i2 = (i + 1) % nc
            bm.faces.new((bot, rows[0][i2], rows[0][i]))
            for k in range(len(ys) - 1):
                bm.faces.new((rows[k][i], rows[k][i2], rows[k + 1][i2], rows[k + 1][i]))
            bm.faces.new((rows[-1][i], rows[-1][i2], top))
        M.bmesh.ops.recalc_face_normals(bm, faces=bm.faces)
        return M.D.mesh(bm, name, GOO, MAT, "cyl", smooth=True, center=tuple(G(0, 0.3, 0)))

    YS = (0.0, 0.012, 0.03, 0.055, 0.085, 0.12, 0.16, 0.2, 0.245, 0.29, 0.335, 0.38, 0.425, 0.465, 0.5, 0.53,
          0.553, 0.572, 0.588, 0.6, 0.609, 0.616, 0.622)
    btree = M.proxy_tree(lambda: body_mesh("jelly_hi", 160, [i * 0.625 / 120 for i in range(121)]))

    def hs(az, y, off=0.0):
        """(point, normal) on the jelly at azimuth az (degrees, 0 = front, + toward its left), height y."""
        a = math.radians(az)
        d = V((math.sin(a), -math.cos(a), 0.0))
        loc, nrm, _, _ = btree.ray_cast(V((0, 0, y)) + d * 2.0, -d, 4.0)
        return loc + nrm * off, nrm

    head = [body_mesh("jelly", 48, YS)]

    # paler inner swirls (decals a hair above the surface), as in the mock's back view, and two
    # highlight streaks high on its right where the key light's sheen would lie
    def swirl(name, az0, az1, y0, y1, w, bulge=0.0, color=GOO_L, nu=18):
        def width(u):
            return w * max(0.0, math.sin(math.pi * u)) ** 0.7

        def fn(u, v):
            a = az0 + (az1 - az0) * u
            y = y0 + (y1 - y0) * u + bulge * math.sin(math.pi * u)
            return hs(a, y + (v - 0.5) * width(u), 0.0012)[0]
        return M.patch(name, fn, nu, 2, color=color, flip=False)
    head.append(swirl("swirl_back", 105.0, 255.0, 0.16, 0.36, 0.075, bulge=0.06))
    head.append(swirl("swirl_back2", 150.0, 225.0, 0.4, 0.5, 0.04, bulge=0.025))
    head.append(swirl("swirl_side_l", 62.0, 108.0, 0.11, 0.2, 0.045, bulge=0.02))
    head.append(swirl("swirl_side_r", -66.0, -112.0, 0.12, 0.22, 0.045, bulge=0.02))
    head.append(swirl("sheen_a", -42.0, -22.0, 0.47, 0.55, 0.02, bulge=0.01, color="trim_white", nu=10))
    head.append(swirl("sheen_b", -58.0, -50.0, 0.4, 0.44, 0.013, bulge=0.004, color="trim_white", nu=6))

    # small raised bubbles set in the jelly (the big ones are parts and slide)
    def bubble(name, az, y, r, segs=8, rings=5):
        p, n = hs(az, y)
        c = p + n * (r * 0.12)
        b = M.blob(name, c, (r, r, r), segs=segs, rings=rings, color=BUB, mat=MAT)
        side = n.cross(UP)
        side = side.normalized() if side.length > 1e-6 else V((1, 0, 0))
        g = c + (n * 0.7 + UP * 0.5 + side * 0.35).normalized() * (r * 0.8)
        gl = M.blob(name + "_glint", g, (r * 0.34, r * 0.3, r * 0.22), segs=5, rings=3, color="trim_white", mat=MAT)
        return [b, gl], c, n
    for k, (az, y, r) in enumerate(((-84, 0.31, 0.016), (102, 0.22, 0.018), (-140, 0.27, 0.019), (140, 0.44, 0.013))):
        head += bubble(f"bub{k}", az, y, r, segs=7, rings=4)[0]

    # the lip line: a thin smile across the top of the mouth (the cavity part shuts onto it)
    MAZ, MW, MTOP, MCURVE, MDEEP = 3.0, 12.5, 0.248, 0.011, 0.082

    def top(az):
        return MTOP + MCURVE * ((az - MAZ) / MW) ** 2
    lip = [hs(MAZ + a, top(MAZ + a) + (0.005 * ((abs(a) - MW) / 3.0) if abs(a) > MW else 0.0), 0.0018)[0]
           for a in (-MW - 3.0, -MW, -MW * 0.6, -MW * 0.25, 0.0, MW * 0.25, MW * 0.6, MW, MW + 3.0)]
    head.append(M.tube("lipline", lip, lambda t: 0.0022 + 0.0014 * math.sin(math.pi * t), sides=5, color="choco_dark",
                       mat=MAT, cap_offset=0.7))
    R.part("body", [], G(0, 0, 0))
    R.part("head", head, G(0, 0, 0), parent="body")

    # ------------------------------------------------------------ skirt: a ring of rounded drips
    NL = 9
    LW = (1.1, 0.9, 1.2, 0.8, 1.0, 1.15, 0.85, 1.05, 0.95)          # lobe widths (front lobe first)
    LR = (0.505, 0.47, 0.52, 0.455, 0.49, 0.53, 0.465, 0.5, 0.48)    # how far each drip runs out
    LH = (0.088, 0.076, 0.092, 0.07, 0.082, 0.095, 0.074, 0.086, 0.08)
    tw = sum(LW)
    edges = [0.0]
    for w in LW:
        edges.append(edges[-1] + M.TAU * w / tw)
    shift = -M.TAU * LW[0] / tw / 2.0   # the first lobe centred at the front
    R_GAP, H_GAP = 0.39, 0.022
    U_COLS = (-1.0, -0.86, -0.62, -0.32, 0.0, 0.32, 0.62, 0.86)

    def drip(th):
        """(outer radius, height) of the skirt at azimuth th."""
        x = (th - shift) % M.TAU
        k = max(i for i in range(NL) if edges[i] <= x + 1e-9)
        k = min(k, NL - 1)
        u = ((x - edges[k]) / (edges[k + 1] - edges[k])) * 2.0 - 1.0
        f = max(0.0, 1.0 - u * u) ** 0.5
        return R_GAP + (LR[k] - R_GAP) * f, H_GAP + (LH[k] - H_GAP) * f

    def skirt_col(th):
        """The drip's cross-section, (r, y) points round a closed loop: it leaves the body tangent to it,
        swells out and down to its outermost point, rounds under onto the ground, runs back inside."""
        ro, h = drip(th)
        ya = h + 0.1
        ra = rad(ya) + 0.0016   # its top edge lies on the body, just proud of it: no seam line
        e = 1e-3
        dr = (rad(ya - e) - rad(ya + e)) / (2 * e)   # how much the body widens per metre down
        da = V((dr, -1.0, 0.0)).normalized()
        A, B = V((ra, ya, 0.0)), V((ro, h * 0.5, 0.0))
        L1, L2 = 0.4 * (B - A).length, 0.45 * h + 0.02
        C1, C2 = A + da * L1, B + V((0.0, L2, 0.0))
        pts = [(ra - 0.03, ya - 0.01), (ra, ya)]
        for s in (0.2, 0.4, 0.6, 0.8, 1.0):
            q = A * (1 - s) ** 3 + C1 * 3 * s * (1 - s) ** 2 + C2 * 3 * s * s * (1 - s) + B * s ** 3
            pts.append((q.x, q.y))
        for a in (-30, -60, -90):
            r = math.radians(a)
            pts.append((ro - h * 0.5 + h * 0.5 * math.cos(r), h * 0.5 + h * 0.5 * math.sin(r)))
        pts.append((0.31, 0.0))
        pts.append((0.31, 0.06))
        return pts

    ths = [shift + edges[k] + (edges[k + 1] - edges[k]) * (u + 1.0) / 2.0 for k in range(NL) for u in U_COLS]
    bm = M.bmesh.new()
    cols = [skirt_col(th) for th in ths]
    grid = [[bm.verts.new(G(r * math.sin(th), y, r * math.cos(th))) for (r, y) in col] for th, col in zip(ths, cols)]
    npt = len(cols[0])
    tops = []
    for i in range(len(ths)):
        i2 = (i + 1) % len(ths)
        for k in range(npt):
            k2 = (k + 1) % npt
            f = bm.faces.new((grid[i][k], grid[i2][k], grid[i2][k2], grid[i][k2]))
            if k == 6:   # the outermost swell of each drip: a paler band, lit from within
                tops.append(f)
    M.bmesh.ops.recalc_face_normals(bm, faces=bm.faces)
    skirt = [M.D.mesh(bm, "skirt_mesh", GOO, MAT, "cyl", smooth=True, center=tuple(G(0, 0.05, 0)), extra=[(tops, GOO_L, MAT)])]
    # a few small bubbles in the drips
    for k, th in enumerate((0.3, 2.2, 3.6, 5.1)):
        ro, h = drip(th)
        rr = ro - h * 0.55
        c = G(rr * math.sin(th), h * 0.82, rr * math.cos(th))
        skirt.append(M.blob(f"sbub{k}", c, (0.011, 0.011, 0.011), segs=6, rings=4, color=BUB, mat=MAT))
    R.part("skirt", skirt, G(0, 0, 0), parent="head")

    # ------------------------------------------------------------ the curled tip: one soft tube with a ball
    cpts = [G(0, 0.54, 0), G(0, 0.6, 0), G(0.004, 0.645, -0.004), G(0.028, 0.684, -0.008), G(0.068, 0.708, -0.01),
            G(0.11, 0.702, -0.01), G(0.135, 0.68, -0.008), G(0.132, 0.658, -0.006)]
    crad = [0.1, 0.093, 0.079, 0.064, 0.052, 0.046, 0.043, 0.042]
    curl = [M.tube("curltube", cpts, crad, sides=12, color=GOO, mat=MAT, cap_offset=0.6),
            M.blob("curlball", G(0.128, 0.652, -0.006), (0.05, 0.05, 0.048), segs=10, rings=6, color=GOO, mat=MAT),
            M.blob("curlpin", G(0, 0.58, 0), (0.02, 0.02, 0.02), segs=6, rings=4, color=GOO, mat=MAT),
            M.blob("curlglint", G(0.112, 0.676, 0.033), (0.01, 0.01, 0.008), segs=6, rings=4, color="trim_white", mat=MAT)]
    R.part("curl", curl, G(0, 0.58, 0), parent="head", soft=dict(dir=None, rigid=0.04, loops=6), ground=False)

    # ------------------------------------------------------------ face: big glossy eyes with a dark outline
    EW, EH, EAZ, EY = 0.082, 0.095, 22.5, 0.365
    for side, s in (("l", 1), ("r", -1)):
        p, n = hs(s * EAZ, EY)
        out = V((n.x, n.y, 0)).normalized()
        en = (n + out * 0.8).normalized()
        ring = []
        for k in range(17):
            a = math.radians(-90 + 360 * k / 16)
            ring.append(V((EW * 1.0 * math.cos(a), EH * 1.0 * math.sin(a), 0.0085)))
        line = M.tube(f"eyeline{side}", ring, lambda t: 0.0055 + 0.0035 * max(0.0, math.sin(math.pi * t)), sides=5,
                      color="choco_dark", mat=MAT, cap_offset=0.5)
        M.eye(R, f"eye_{side}", f"pupil_{side}", p - n * 0.008, en, EW, EH, parent="head", depth=EW * 0.36,
              sclera="trim_white", iris="iris_brown", iris_r=0.74, pupil="ink", pupil_r=0.42, glint="glow_white",
              look=(-s * 0.05, -0.04), segs=18, eye_extra=[line])
        # brow: a short thick dark arc above the outer half of the eye
        pts = [hs(s * (14.0 + 19.0 * (i / 4)), EY + 0.112 + 0.013 * math.sin(math.pi * i / 4) - 0.006 * (i / 4), 0.004)[0]
               for i in range(5)]
        pc, nc = hs(s * 23.5, EY + 0.12)
        brow = M.tube(f"brow{side}", pts, [0.006, 0.0098, 0.0105, 0.0088, 0.0052], sides=6, color="choco_dark", mat=MAT,
                      cap_offset=0.8)
        R.part(f"brow_{side}", [brow], pc, parent="head", z_out=nc, ground=False)
        # cheek: a paler raised oval of jelly sunk into the face; puffs when scaled
        q, qn = hs(s * 43.0, 0.25)
        ck = M.blob(f"cheekpad{side}", (0, 0, 0), (0.056, 0.038, 0.04), segs=12, rings=6, color=GOO_L, mat=MAT)
        M.put(ck, q - qn * 0.022, qn, up=tuple(UP))
        R.part(f"cheek_{side}", [ck], q - qn * 0.022, parent="head", z_out=qn, ground=False)
        # shut eyes (> <) for a hit, happy arcs (^ ^) for a laugh; hidden until shown
        chev = [hs(s * (EAZ + 8.0), EY + 0.03, 0.006)[0], hs(s * (EAZ - 7.0), EY + 0.002, 0.006)[0],
                hs(s * (EAZ + 8.0), EY - 0.026, 0.006)[0]]
        R.part(f"alt_squint_{side}", [M.tube(f"chev{side}", chev, [0.006, 0.0085, 0.006], sides=5, color="choco_dark", mat=MAT,
                                              cap_offset=0.7)], p, parent="head", ground=False)
        arc = [hs(s * (EAZ + 9.5 * math.cos(math.pi * i / 6)), EY - 0.014 + 0.032 * math.sin(math.pi * i / 6), 0.006)[0]
               for i in range(7)]
        R.part(f"alt_happy_{side}", [M.tube(f"happy{side}", arc, lambda t: 0.0058 + 0.0028 * math.sin(math.pi * t), sides=5,
                                            color="choco_dark", mat=MAT, cap_offset=0.7)], p, parent="head", ground=False)

    # the open mouth: dark cavity, a row of upper teeth, the tongue; scaled in Y about the lip line
    def cavity(u, v):
        a = (u * 2 - 1) * MW
        depth = MDEEP * math.sqrt(max(0.0, 1.0 - (a / MW) ** 2)) ** 0.75 + 0.003
        return hs(MAZ + a, top(MAZ + a) - 0.0015 - v * depth, 0.0012)[0]

    def mpaint(i, j):
        if j == 0 and 2 <= i <= 7:
            return ("trim_white", MAT)
        if j >= 3 and 2 <= i <= 7:
            return ("tongue", MAT)
        if j >= 4 and i in (1, 8):
            return ("tongue", MAT)
        return None
    mouth = M.patch("mouthcav", cavity, 10, 6, color="mouth_dark", flip=True, paint=mpaint)
    MP, MN = hs(MAZ, top(MAZ), 0.0012)
    R.part("mouth", [mouth], MP, parent="head", z_out=MN, ground=False)
    R.socket("muzzle", hs(MAZ, MTOP - 0.03, 0.05)[0], "head")

    # ------------------------------------------------------------ the big bubbles: parts that slide on springs
    for name, (az, y, r) in (("bubble_a", (-60.0, 0.15, 0.034)), ("bubble_b", (66.0, 0.36, 0.022)),
                             ("bubble_c", (168.0, 0.25, 0.045)), ("bubble_d", (-118.0, 0.4, 0.026))):
        objs, c, n = bubble(name + "_m", az, y, r, segs=10, rings=6)
        R.part(name, objs, c, parent="head", z_out=n, ground=False)

    # ------------------------------------------------------------ the thrown goo (hidden; its mesh is the projectile)
    goo = [M.blob("goo_core", G(0, 0.3, -0.04), (0.12, 0.11, 0.105), segs=10, rings=6, color="glow_slime", mat=MAT)]
    for k, (dx, dy, dz, r) in enumerate(((0.08, 0.05, 0.03, 0.05), (-0.07, -0.04, 0.05, 0.045), (0.02, -0.08, -0.05, 0.04),
                                         (-0.04, 0.07, -0.06, 0.042))):
        goo.append(M.blob(f"goo_lump{k}", G(dx, 0.3 + dy, dz - 0.04), (r, r, r), segs=7, rings=4, color=GOO, mat=MAT))
    goo.append(M.blob("goo_glint", G(-0.04, 0.37, 0.03), (0.018, 0.018, 0.014), segs=6, rings=4, color="trim_white", mat=MAT))
    R.part("alt_goo", goo, G(0, 0.3, -0.04), parent="head", ground=False)

    poses = [
        ("rest", {}),
        ("lean back", {"head": [("x", -12)], "curl": [("z", -25)], "brow_l": [("z", -15)], "brow_r": [("z", 15)]}),
        ("lob", {"head": [("x", 14)], "curl": [("x", -30)]}),
        ("look", {"head": [("y", 30)], "curl": [("z", 30)]}),
        ("hit", {"head": [("z", 16), ("x", -8)], "curl": [("z", -40)]}),
        ("tilt", {"head": [("z", -14)], "curl": [("x", 35)]}),
    ]
    R.shade({GOO: GOO_S}, ao_cut=0.42, down=-0.75, parts=["skirt"])
    return R.finish(ao_radius=0.18, ao_strength=0.55, poses=poses, closeups=[("face", 2.0, tuple(G(0, 0.33, 0.2)))])


# ====================================================================== PUMPKIN HOPPER
def pumpkin():
    """The Pumpkin Hopper, faithful to the PROCEDURAL pumpkin the game has used (FIDELITY_PASS decision 7:
    a squat lobed jack-o'-lantern about 1.0 m wide and 0.8 m tall, apex-up triangle eyes, a slot grin with
    three teeth hanging from the top lip, vine arms) at much higher fidelity, with the mock's polish:
    ten deep ribs, eyes and grin carved into the shell (a flesh-coloured cut rim, glowing inner walls, a
    glowing floor behind the grin), a curled green stem, a collar of four veined leaves, two curly
    tendrils, vine arms with three-leaf hands, stubby green root feet.

    (Polish round) The face is a CARVED JACK-O'-LANTERN: two big upright triangle eyes and a wide
    symmetric grin with square teeth (two hanging, one standing), every hole blazing with candle light.
    No pupils, no sclera: each eye hole holds a light plate (hot yellow, pale core) and a small white-hot
    flame that drifts toward the player, flickers, flares, shrinks on a hit and goes out on a blink or
    in death. The head is carved with the ROAR; `mouth` is a plug of the same shell surface that fills it
    back to the grin (hide it to roar). The brows are shell wedges hidden in the flesh above the eyes
    that swing down to slant them in anger.

    Part tree: body (meshless joint on the ground) > head (the shell with the carved face; pivot at its
        bottom: squash, rock and look about it) > mouth (the grin plug), mouth_glow (the mouth's light
        plate), stem (soft: one curled tube), leaf_a..d (soft collar leaves, pinned at the stem),
        tendril_l/r (soft curls), eye_l/r (the eye's light plate) > pupil_l/r (the flame), brow_l/r
        (anger wedges, pivot 0.42 m behind the face), arm_l/r (vine, ball at a sunk shoulder) > hand_l/r
        (three leaves, ball at the wrist);
      body > foot_l/r (root feet under the shell)."""
    K.reset()
    OR, OR_S = "candy_orange", "pumpkin_dark"
    SOCK = "rust"
    FLESH, WALL, HOT, EMBER = "amber", "glow_jack", "glow_jack_hot", "glow_jack_shell"
    STEM = "gourd_green"
    LEAF, LEAF_V, LEAF_U = "apple_green", "dragon_green_light", "vine"
    VINE, VINE_D = "vine", "vine_dark"
    SKIN, SOFT = "pumpkin", "plush"
    R = M.Rig("monster_pumpkin", height=0.9, radius=0.42)
    BOT, HGT, WID, DEP, DIMPLE = 0.065, 0.75, 1.02, 0.93, 0.075

    def shell_mesh(name, rings, spl):
        ob = F.pumpkin_body(name, width=WID, height=HGT, lobes=10, spl=spl, rings=rings, groove=0.16, plump=0.45,
                            fullness=0.74, flatness=0.78, top_dimple=DIMPLE, dimple_w=0.3, bottom_dimple=0.03,
                            lumps=0.025, lobe_h=0.03, seed=7, rot_z=-90.0, color=OR, mat=SKIN)
        F.transform(ob, M.Matrix.Translation((0, 0, BOT)) @ M.Matrix.Diagonal((1.0, DEP / WID, 1.0, 1.0)))
        return ob
    stree = M.proxy_tree(lambda: shell_mesh("shell_hi", 48, 16))

    def hs(az, y, off=0.0):
        """(point, normal) on the shell at azimuth az (degrees, 0 = front, + toward its left), height y."""
        a = math.radians(az)
        d = V((math.sin(a), -math.cos(a), 0.0))
        loc, nrm, _, _ = stree.ray_cast(V((0, 0, y)) + d * 2.0, -d, 4.0)
        return loc + nrm * off, nrm

    def down(bx, by):
        """(point, normal) on the shell straight below Blender (bx, by)."""
        loc, nrm, _, _ = stree.ray_cast(V((bx, by, 2.0)), V((0, 0, -1)), 4.0)
        return loc, nrm

    # ------------------------------------------------------------ the shell, with the face carved in
    # (polish round) A CARVED JACK-O'-LANTERN, as BEFORE and the mock: two big upright triangle eyes and a
    # wide symmetric toothed grin, all holes blazing with candle light. The head is carved with the ROAR
    # (the widest mouth); the `mouth` part is a plug of the same shell surface that fills the roar back in
    # down to the grin, so hiding it opens the mouth wide (wind-up, slam). Light is graded from the
    # outside in: amber flesh rim -> orange glowing walls (head) -> a yellow plate with a pale core behind
    # each hole (eye_l / eye_r, mouth_glow) -> a small white-hot flame in each eye (pupil_l / pupil_r).
    shell = shell_mesh("shell", 16, 6)
    EYE_X, EYE_Y, EYE_W, EYE_H, EYE_D = 0.175, 0.568, 0.28, 0.238, 0.07
    CORE, FLAME = "glow_candle", "glow_white"

    def eye_outline(s):
        return F.rounded_triangle(s * EYE_X, EYE_Y, EYE_W, EYE_H, tilt=0.0, round_=0.14, arc_pts=2)
    # The mouth: a wide grin (upper lip with two hanging teeth, lower lip with one standing tooth) and its
    # ROAR, the same upper lip and corners with the lower lip dropped. The head is carved with the roar; the
    # `mouth` part is the lower-lip band between the two (with the standing tooth), cut from the same shell
    # surface, so hiding it drops the jaw.
    MOUTH = dict(cx=0.0, cz=0.37, w=0.64, top_sag=0.055, lift=(0.05, 0.05), n=13, teeth=(-0.13, 0.13), tooth_w=0.074,
                 tooth_h=0.05)
    GRIN_D, ROAR_D, LTOOTH_W, LTOOTH_H = 0.19, 0.245, 0.074, 0.05
    MOUTH_D, PLUG_D = 0.09, 0.035
    feats = []
    for s in (1, -1):
        feats.append({"outline": eye_outline(s), "depth": EYE_D, "inset": 0.02, "rim_frac": 0.4, "rim_w": 0.01,
                      "rim": FLESH, "wall": WALL, "color": EMBER})
    feats.append({"shape": F.shape_of(F.grin, depth=ROAR_D, **MOUTH), "depth": MOUTH_D, "inset": 0.016, "rim_frac": 0.45,
                  "rim_w": 0.008, "rim": FLESH, "wall": WALL, "color": EMBER})
    F.carve(shell, feats)
    head = [shell]

    ltree = M.proxy_tree(lambda: shell_mesh("shell_lo", 16, 6))   # the head's own tessellation, uncarved

    def front_hit(x, z):
        """Blender y of the shell's front surface at (x, z), on the head's own (low-res) facets, so parts laid
        on it (the lip band, the brows) sit flush with or inside the head, never through it."""
        loc, _, _, _ = ltree.ray_cast(V((x, -3.0, z)), V((0, 1, 0)), 6.0)
        if loc is None:
            raise RuntimeError(f"pumpkin face: ray missed the shell at x={x:.3f} z={z:.3f}")
        return loc.y

    def lip(x, depth=None):
        """The upper lip (depth None) or a lower lip `depth` below the centre, as F.grin draws them."""
        hw_ = MOUTH["w"] / 2
        q = max(0.0, 1 - (x / hw_) ** 2)
        t = (x + hw_) / MOUTH["w"]
        c = MOUTH["lift"][0] * (1 - t) ** 3 + MOUTH["lift"][1] * t ** 3
        z = MOUTH["cz"] - MOUTH["top_sag"] * q + c
        return z if depth is None else z - (depth - MOUTH["top_sag"]) * q ** 0.75

    # the lower-lip band: a tube of cross sections [P (the grin's lower lip, on the surface), Q (the roar's
    # lower lip, 4 mm past it, on the surface), Q2 (Q at the back), P2 (P at the back, tapered into the mouth)]
    hw = MOUTH["w"] / 2
    NB = 17
    xs = [-hw + MOUTH["w"] * i / (NB - 1) for i in range(NB)]
    xs = [x for x in xs if abs(x) > LTOOTH_W / 2 + 0.012]
    tl, tr = -LTOOTH_W / 2, LTOOTH_W / 2
    top = ([(x, lip(x, GRIN_D), 0) for x in xs if x < 0] +
           [(tl, lip(tl, GRIN_D), 0), (tl, lip(tl, GRIN_D) + LTOOTH_H, -1),     # -1: shares Q with the column before
            (0.0, lip(0.0, GRIN_D) + LTOOTH_H, 0),                                # its middle: the lobe seam (front groove)
            (tr, lip(tr, GRIN_D) + LTOOTH_H, 1), (tr, lip(tr, GRIN_D), 0)] +   # 1: ... with the column after
           [(x, lip(x, GRIN_D), 0) for x in xs if x > 0])
    LIFT = 0.0015
    bm = F.bm_new()
    cols = []
    for k, (x, z, share) in enumerate(top):
        a = V((top[max(k - 1, 0)][0], top[max(k - 1, 0)][1], 0.0))
        b = V((top[min(k + 1, len(top) - 1)][0], top[min(k + 1, len(top) - 1)][1], 0.0))
        tg = (b - a).normalized()
        nx, nz = -tg.y, tg.x                     # left of the run (left to right): into the mouth
        P = bm.verts.new((x, front_hit(x, z) - LIFT, z))
        P2 = bm.verts.new((x + nx * 0.008, front_hit(x, z) + PLUG_D, z + nz * 0.008))
        Q = Q2 = None
        if share == 0:
            zq = lip(x, ROAR_D) - 0.004
            Q = bm.verts.new((x, front_hit(x, zq) - LIFT, zq))
            Q2 = bm.verts.new((x, front_hit(x, zq) + PLUG_D, zq))
        cols.append([P, Q, Q2, P2])
    for k, (_, _, share) in enumerate(top):
        if share:
            cols[k][1], cols[k][2] = cols[k + share][1], cols[k + share][2]
    front, upper, back, lower = [], [], [], []

    def face(vs, bucket):
        u = []
        for v in vs:
            if v not in u:
                u.append(v)
        if len(u) >= 3 and not (bucket is front and max(v.co.x for v in u) - min(v.co.x for v in u) < 1e-4):
            bucket.append(bm.faces.new(u))   # (the skin faces along a tooth's side are edge-on slivers: left out)
    for k in range(len(cols) - 1):
        (P, Q, Q2, P2), (Pn, Qn, Q2n, P2n) = cols[k], cols[k + 1]
        face((P, Pn, Qn, Q), front)
        face((P2, P2n, Pn, P), upper)
        face((Q2, Q2n, P2n, P2), back)
        face((Q, Qn, Q2n, Q2), lower)
    face(cols[0], lower)
    face(tuple(reversed(cols[-1])), lower)
    bmesh.ops.recalc_face_normals(bm, faces=bm.faces)
    if bm.calc_volume(signed=True) < 0:
        bmesh.ops.reverse_faces(bm, faces=bm.faces)
    plug = D.mesh(bm, "mouthplug", OR, SKIN, "box", smooth=True, extra=[(upper + lower, FLESH, "flat"), (back, WALL, "flat")])
    # the shell's own UVs on the lip's skin (the faces looking out), so the speckles carry across the seam
    src = shell_mesh("uvsrc", 16, 6)
    sbm = bmesh.new()
    sbm.from_mesh(src.data)
    bmesh.ops.triangulate(sbm, faces=sbm.faces)
    suv = sbm.loops.layers.uv[0]
    tris_ = [[lp.vert.co.copy() for lp in f.loops] for f in sbm.faces]
    tuv = [[lp[suv].uv.copy() for lp in f.loops] for f in sbm.faces]
    sbm.free()
    bpy.data.objects.remove(src, do_unlink=True)
    from mathutils.bvhtree import BVHTree as _BVH
    from mathutils.geometry import barycentric_transform as _bary
    ut = _BVH.FromPolygons([v for t in tris_ for v in t], [[3 * i, 3 * i + 1, 3 * i + 2] for i in range(len(tris_))])
    pme = plug.data
    uvl_ = pme.uv_layers[0]
    for poly in pme.polygons:
        if poly.normal.y > -0.3:   # the walls and the back face: hard-edged against the skin
            poly.use_smooth = False
            continue
        # one shell triangle per face (the one under its centre), extrapolated to its corners: each lobe is
        # its own UV island, and a face whose corners took UVs from two islands smears the texture into stripes
        _, _, idx, _ = ut.find_nearest(poly.center)
        if idx is None:
            continue
        a, b, c = tris_[idx]
        ua, ub, uc = (V((u.x, u.y, 0.0)) for u in tuv[idx])
        for li in poly.loop_indices:
            co = pme.vertices[pme.loops[li].vertex_index].co
            w = _bary(co, a, b, c, ua, ub, uc)
            uvl_.data[li].uv = (w.x, w.y)

    # the light behind the mouth: a plate at the roar's floor, graded hot yellow with a pale core
    NU_M, NV_M = 12, 3

    def mouth_plate(u, v):
        x = (u - 0.5) * MOUTH["w"] * 0.985
        lo, hi = lip(x, ROAR_D) - 0.006, lip(x) + 0.006
        z = lo + (hi - lo) * v
        return V((x, front_hit(x, z) + MOUTH_D - 0.012, z))

    def mouth_paint(i, j):
        du = abs((i + 0.5) / NU_M - 0.5) * 2.0
        if du < 0.5 and j == 1:
            return (CORE, "flat")
        if du > 0.84:
            return (WALL, "flat")
        return None
    mglow = M.patch("mouthglow", mouth_plate, NU_M, NV_M, color=HOT, mat="flat", paint=mouth_paint, smooth=False)   # u along +x, v up: faces -y (out)
    # shoulder sockets: a dark ring round each arm's root, the ball sunk in it
    shoulders = {}
    for side, s in (("l", 1), ("r", -1)):
        p, n = hs(s * 84.0, 0.41)
        shoulders[side] = (p - n * 0.03, n)
        head.append(M.blob(f"socket{side}", p - n * 0.036, (0.05, 0.05, 0.05), segs=8, rings=5, color=SOCK, mat=SOFT))
        head.append(M.blob(f"socketcore{side}", p - n * 0.03, (0.022, 0.022, 0.022), segs=6, rings=4, color=SOCK, mat=SOFT))
    R.part("body", [], G(0, 0, 0))
    R.part("head", head, G(0, BOT, 0), parent="body")

    mc = V((0.0, front_hit(0.0, 0.3), 0.3))
    R.part("mouth", [plug], mc, parent="head", ao_from=("head",), ground=False)
    R.part("mouth_glow", [mglow], mc + V((0, MOUTH_D, 0)), parent="head", ground=False)

    # ------------------------------------------------------------ eyes: a light plate and a flame in each hole
    for side, s in (("l", 1), ("r", -1)):
        outline = eye_outline(s)
        shape = F.convex_shape(outline)
        n_out = len(outline)
        cx = sum(p[0] for p in outline) / n_out
        cz = sum(p[1] for p in outline) / n_out
        nrm = V((s * 0.2, -1.0, 0.0)).normalized()
        centre = V((cx, front_hit(cx, cz), cz))
        # the plate: concentric rings of the triangle, hot yellow outside, the pale core inside
        bm = F.bm_new()
        rings = [[bm.verts.new((x, front_hit(x, z) + EYE_D - 0.011, z)) for x, z in shape(g)]
                 for g in (0.0, -0.022, -0.04, -0.056)]
        mid = bm.verts.new((cx, front_hit(cx, cz) + EYE_D - 0.011, cz))
        hot, core = [], []
        for k in range(len(rings) - 1):
            A, B = rings[k], rings[k + 1]
            for i in range(n_out):
                f = bm.faces.new((A[i], A[(i + 1) % n_out], B[(i + 1) % n_out], B[i]))
                (hot if k < 1 else core).append(f)   # outer ring orange (wall), the rest hot yellow
        for i in range(n_out):
            core.append(bm.faces.new((rings[-1][i], rings[-1][(i + 1) % n_out], mid)))
        bm.normal_update()
        if sum(f.normal.y for f in bm.faces) > 0:
            bmesh.ops.reverse_faces(bm, faces=bm.faces)
        plate = D.mesh(bm, f"eyeplate{side}", HOT, "flat", "box", smooth=False, extra=[(hot, WALL, "flat")])
        # the flame: a small teardrop, white-hot in front of a candle-yellow glow, 3 cm inside the hole
        fy = front_hit(cx, cz) + 0.03

        def tear(c):
            k = 1.0 - 0.55 * max(0.0, c.z) ** 1.2
            return V((c.x * k, c.y * k, c.z + 0.25 * max(0.0, c.z) ** 2))
        fl = [M.blob(f"flame{side}", V((cx, fy + 0.004, cz)), (0.03, 0.012, 0.042), segs=7, rings=4, color=CORE, warp=tear),
              M.blob(f"flamecore{side}", V((cx, fy - 0.004, cz - 0.004)), (0.017, 0.008, 0.026), segs=6, rings=3, color=FLAME,
                     warp=tear)]
        R.part(f"eye_{side}", [plate], centre, parent="head", z_out=nrm, ground=False)
        R.part(f"pupil_{side}", fl, centre, parent=f"eye_{side}", z_out=nrm, ground=False)
        R.eyes[f"eye_{side}"] = f"pupil_{side}"
        # the angry brow: a wedge of shell hidden in the flesh just above the triangle. Its lower edge runs
        # down toward the nose at 38 degrees; the animator swings it down about a pivot 0.42 m behind the
        # face (local X), so it slides along the shell into the top of the hole and slants the eye.
        ax, az = s * EYE_X, EYE_Y + EYE_H / 2 - 0.012 + 0.016
        dx, dz = -s * math.cos(math.radians(38)), -math.sin(math.radians(38))   # along the edge: down toward the nose
        nx, nz = -dz, dx                                                            # its normal toward the upper side
        if nz < 0:
            nx, nz = -nx, -nz

        def b2d(u, v):
            t = -0.03 + 0.185 * u     # the outer end stops short of the next groove up on the shoulder: a brow facet
            return ax + dx * t + nx * 0.04 * v, az + dz * t + nz * 0.04 * v   # spanning a groove's V pokes out of it

        def under(x, z, d):
            """`d` under the head's surface along its normal (not along y: near the shoulder the surface
            leans back, and a y offset there is almost no depth at all)."""
            loc, nrm, _, _ = ltree.ray_cast(V((x, -3.0, z)), V((0, 1, 0)), 6.0)
            return loc - nrm * d

        def brow_out(u, v):
            return under(*b2d(u, v), 0.008)

        def brow_in(u, v):
            return under(*b2d(u, v), 0.034)
        brow = M.shell(f"brow{side}", brow_out, 5, 2, inner=brow_in, color=OR, mat=SKIN, rim_color=FLESH)
        R.part(f"brow_{side}", [brow], centre - nrm * 0.42, parent="head", z_out=nrm, ground=False)

    # ------------------------------------------------------------ the stem: one curled tube (a fiddlehead)
    TOPY = BOT + HGT - DIMPLE
    PIN = G(0, TOPY - 0.03, 0)
    sp = [(0, TOPY - 0.03, 0), (0, 0.80, 0), (0.003, 0.85, -0.006), (0.014, 0.895, -0.012), (0.04, 0.93, -0.014),
          (0.075, 0.942, -0.01), (0.105, 0.93, -0.004), (0.118, 0.905, 0.0), (0.11, 0.882, 0.002), (0.09, 0.874, 0.002),
          (0.076, 0.886, 0.0), (0.08, 0.9, -0.002)]
    spts = M.catmull([G(*p) for p in sp], per=2)
    srad = [0.058, 0.043, 0.036, 0.031, 0.027, 0.024, 0.021, 0.019, 0.017, 0.015, 0.013, 0.012]

    def stem_r(t):
        f = t * (len(srad) - 1)
        i = min(int(f), len(srad) - 2)
        return srad[i] + (srad[i + 1] - srad[i]) * (f - i)
    stem = [F.sweep("stemtube", spts, stem_r, sides=10, color=STEM, mat=SOFT, profile=lambda j: 1.0 if j % 2 == 0 else 0.84,
                    cap_start=False, cap_offset=0.7)]
    R.part("stem", stem, PIN, parent="head", soft=dict(dir=G(0, 1, 0), rigid=0.03, loops=6), ground=False)

    # ------------------------------------------------------------ the leaf collar: four leaves draped over the top
    def leaf_width(v, wd):
        return wd * 0.5 * (math.sin(math.pi * min(1.0, v ** 0.85)) ** 0.7) + 0.006 * (1.0 - v)

    def collar_leaf(name, az, L, wd, r0=0.03, lift=0.011, cup=0.42, wave=0.006):
        a = math.radians(az)
        dr = V((math.sin(a), -math.cos(a), 0.0))
        tn = V((math.cos(a), math.sin(a), 0.0))

        def outer(u, v):
            x = (0.5 - u) * 2.0
            w = leaf_width(v, wd)
            q = dr * (r0 + L * v) + tn * (x * w)
            loc, nrm = down(q.x, q.y)
            edge = wave * math.sin(v * 9.0 + x * 2.0) * abs(x) * min(1.0, w / 0.03)
            return loc + nrm * (lift + cup * x * x * w + edge + 0.012 * v * (1.0 - v) + 0.03 * v ** 4)
        ob = M.shell(name, outer, 6, 8, thick=0.006, color=LEAF, mat=SOFT, rim_color=LEAF_U)
        rib = M.tube(name + "_rib", [outer(0.5, t) + V((0, 0, 0.004)) for t in (0.0, 0.3, 0.6, 0.85)],
                     [0.0055, 0.0045, 0.0035, 0.002], sides=5, color=LEAF_V, mat=SOFT, cap_offset=0.5)
        veins = []
        for k, (t0, sgn) in enumerate(((0.3, 1), (0.3, -1), (0.55, 1), (0.55, -1))):
            u1 = 0.5 - sgn * 0.33
            veins.append(M.tube(f"{name}_v{k}", [outer(0.5, t0) + V((0, 0, 0.0035)), outer(0.5 - sgn * 0.18, t0 + 0.1) + V((0, 0, 0.003)),
                                                  outer(u1, t0 + 0.17) + V((0, 0, 0.002))],
                                [0.003, 0.0024, 0.0014], sides=4, color=LEAF_V, mat=SOFT, cap_offset=0.4))
        pin = dr * r0
        loc, _ = down(pin.x, pin.y)
        return [ob, rib] + veins, loc, dr
    for name, az, L, wd in (("leaf_a", 36.0, 0.27, 0.21), ("leaf_b", 128.0, 0.25, 0.2), ("leaf_c", -140.0, 0.26, 0.21),
                            ("leaf_d", -48.0, 0.25, 0.19)):
        objs, pin, dr = collar_leaf(name + "_m", az, L, wd)
        R.part(name, objs, pin, parent="head", soft=dict(dir=dr, rigid=0.02, loops=6), ground=False)

    # ------------------------------------------------------------ tendrils: a curl each side of the stem
    for side, s in (("l", 1), ("r", -1)):
        c = (s * 0.14, 0.885, -0.05)
        r0, r1, turns = 0.036, 0.009, 1.55
        pts = [(s * 0.035, TOPY - 0.015, -0.03), (s * 0.06, 0.81, -0.042), (s * 0.085, 0.85, -0.05)]
        for i in range(16):
            t = i / 15
            ang = math.pi - t * turns * M.TAU
            r = r0 + (r1 - r0) * t
            pts.append((c[0] + s * math.cos(ang) * r, c[1] + math.sin(ang) * r, c[2] + 0.01 * t))
        tp = [G(*p) for p in pts]
        ten = [F.sweep(f"tendril{side}", tp, lambda t: 0.0085 * (1.0 - 0.5 * t), sides=5, color=STEM, mat=SOFT, cap_offset=0.6)]
        R.part(f"tendril_{side}", ten, tp[0], parent="head", soft=dict(dir=None, rigid=0.015, loops=6), ground=False)

    # ------------------------------------------------------------ vine arms with three-leaf hands
    def free_leaf(name, root, D, N, L, wd, cup=0.32, droop=0.1, nu=4, nv=5, thick=0.0055):
        D = V(D).normalized()
        N = (V(N) - D * V(N).dot(D)).normalized()
        Wv = N.cross(D).normalized()

        def outer(u, v):
            x = (u - 0.5) * 2.0
            w = leaf_width(v, wd)
            return root + D * (L * v) + Wv * (x * w) + N * (cup * x * x * w - droop * L * v * v)

        def inner(u, v):
            x = (u - 0.5) * 2.0
            return outer(u, v) - N * (thick * (0.4 + 0.6 * (1.0 - x * x)))
        ob = M.shell(name, outer, nu, nv, inner=inner, color=LEAF, mat=SOFT, rim_color=LEAF_U)
        rib = M.tube(name + "_rib", [outer(0.5, t) + N * 0.0025 for t in (0.05, 0.45, 0.85)], [0.0036, 0.003, 0.0016],
                     sides=4, color=LEAF_V, mat=SOFT, cap_offset=0.5)
        return [ob, rib]

    for side, s in (("l", 1), ("r", -1)):
        S, n = shoulders[side]
        ap = [S, S + G(s * 0.06, 0.006, 0.012), S + G(s * 0.12, -0.01, 0.03), S + G(s * 0.168, -0.038, 0.05),
              S + G(s * 0.2, -0.07, 0.064)]
        apts = M.catmull(ap, per=3)
        W = ap[-1]
        arm = [F.sweep(f"vine{side}", apts, lambda t: 0.041 - 0.012 * t, sides=9, color=STEM, mat=SOFT, cap_start=False,
                       cap_offset=0.5)]
        # a little curl of tendril springing from the vine
        base = S + G(s * 0.1, -0.006, 0.026)
        cc = base + G(s * 0.012, -0.045, 0.0)
        cpts = [base, base + G(s * 0.012, -0.018, 0.0)]
        for i in range(10):
            t = i / 9
            ang = math.pi * 0.5 + t * 1.4 * M.TAU
            r = 0.024 - 0.015 * t
            cpts.append(cc + G(s * math.cos(ang) * r, math.sin(ang) * r - 0.0, 0.004 * t))
        arm.append(F.sweep(f"armcurl{side}", cpts, lambda t: 0.0065 * (1.0 - 0.45 * t), sides=5, color=STEM, mat=SOFT, cap_offset=0.6))
        R.part(f"arm_{side}", arm, S, parent="head", ball=0.044, ball_color=STEM, ball_segs=9, ground=False)
        d = (W - ap[-2]).normalized()
        fwd = G(0, 0, 1)
        npalm = (fwd - d * fwd.dot(d)).normalized()
        u = npalm.cross(d)
        hand = []
        for k, (ang, L, wd) in enumerate(((-55.0, 0.1, 0.07), (0.0, 0.118, 0.078), (55.0, 0.102, 0.07))):
            a = math.radians(ang)
            dl = (d * math.cos(a) + u * math.sin(a)).normalized()
            hand += free_leaf(f"hleaf{side}{k}", W + dl * 0.014, dl, npalm, L, wd)
        R.part(f"hand_{side}", hand, W, parent=f"arm_{side}", ball=0.031, ball_color=STEM, ball_segs=8, ground=False)

    # ------------------------------------------------------------ stubby root feet
    def flatten(cz, rz):
        k = cz / rz
        return lambda c: V((c.x, c.y, c.z if c.z > 0 else c.z * k))
    for side, s in (("l", 1), ("r", -1)):
        x = s * 0.205
        foot = [M.blob(f"foot{side}", G(x, 0.058, 0.03), (0.1, 0.122, 0.07), segs=12, rings=7, color=VINE, mat=SOFT,
                       warp=flatten(0.058, 0.07))]
        for k, (dx, rr) in enumerate(((-0.055, 0.036), (0.0, 0.041), (0.055, 0.034))):
            foot.append(M.blob(f"toe{side}{k}", G(x + s * dx, 0.034, 0.118 - abs(dx) * 0.35), (rr, rr * 1.15, rr * 0.9), segs=9,
                               rings=5, color=LEAF, mat=SOFT, warp=flatten(0.034, rr * 0.9)))
        R.part(f"foot_{side}", foot, G(x, 0.09, 0.03), parent="body")

    poses = [
        ("rest", {}),
        ("hop: arms up", {"head": [("x", -8)], "arm_l": [("z", 70)], "arm_r": [("z", -70)], "hand_l": [("z", 30)],
                          "hand_r": [("z", -30)], "foot_l": [("x", 35)], "foot_r": [("x", 35)], "stem": [("x", -18)]}),
        ("slam: arms flat", {"head": [("x", 20)], "arm_l": [("z", -38), ("y", 25)], "arm_r": [("z", 38), ("y", -25)],
                             "leaf_a": [("x", 20)], "leaf_d": [("x", 20)]}),
        ("look", {"head": [("y", 32)], "arm_l": [("y", 30)], "arm_r": [("y", 30)]}),
        ("rock", {"head": [("z", 15)], "arm_l": [("z", 40)], "arm_r": [("z", 10)], "foot_r": [("move", (0, 0.03, 0))]}),
        ("hit", {"head": [("z", -12), ("x", -12)], "arm_l": [("z", 105)], "arm_r": [("z", -105)], "hand_l": [("x", 40)],
                 "leaf_b": [("x", -25)], "stem": [("z", 25)]}),
        ("angry", {"brow_l": [("x", 6.5)], "brow_r": [("x", 6.5)], "head": [("x", -6)]}),
    ]
    R.shade({OR: OR_S}, ao_cut=0.6, down=-0.8, parts=["head", "mouth"])
    R.shade({VINE: VINE_D}, ao_cut=0.5, down=-0.5, parts=["foot_l", "foot_r", "arm_l", "arm_r"])
    return R.finish(ao_radius=0.2, ao_strength=0.8, poses=poses,
                    closeups=[("face", 1.8, tuple(G(0, 0.5, 0.3))),
                              ("front", 2.2, tuple(G(0, 0.47, 0.4)), [(0, 4), (24, 6), (0, 32), (-60, 8)])])


# ====================================================================== BAT
def bat():
    """A chubby, fuzzy toy bat that hovers at tree-crown height (centred on the origin, as before: the game's
    flyer origin is its body centre). Violet plush fur with a cream belly patch, a fluffy neck ruff, cheek
    fluff and a head tuft; huge ears with a pink-lilac inside; big eyes (white, a two-tone amber iris that
    glows softly at night, an ink pupil, glints, a lash line) under dark brows; a little purple nose, blush,
    an open smiling mouth with a tongue and two fangs; tiny purple feet; and hinged wings: an arm with a
    thumb claw and the inner membrane, then three fingers with the scalloped outer membrane.

    Part tree: body (the torso, pivot at the origin) > head (pivot at the neck) > ear_l/r (soft shells
        pinned at the base), tuft (soft), eye_l/r > pupil_l/r, brow_l/r, jaw (the open mouth, scaled in Y
        about the lip line), alt_squint_l/r (> < eyes);
      body > wing_l/r (arm, thumb, inner membrane; ball at the shoulder) > wingtip_l/r (fingers and outer
        membrane; ball at the wrist), foot_l/r (ball at the hip).
    The wingtip's rest frame has +Z out of the membrane's front and +Y along the first (innermost)
    finger: the inner membrane ends exactly on that finger, so folding the outer wing about its local Y
    (the hinge) never opens a gap between the two membranes."""
    K.reset()
    FUR, FUR_S = "hat_violet", "spider_plum"
    FUR_L = "witch_purple"                   # lighter violet: tuft, feet, nose
    MEM = "bat_wing"
    EAR, EAR_S = "candy_pink", "house_plum"
    BEL, BEL_S = "linen_light", "linen"
    CLAW = "linen_light"
    SOFT = "plush"
    R = M.Rig("monster_bat", height=0.5, radius=0.3, floating=True)

    def gd(az, el):
        a, e = math.radians(az), math.radians(el)
        return G(math.sin(a) * math.cos(e), math.sin(e), math.cos(a) * math.cos(e))

    # ------------------------------------------------------------ shapes: torso (egg) and head
    TC, TRX, TRY, TRZ = (0.0, -0.08, -0.005), 0.165, 0.163, 0.152
    HC, HRX, HRY, HRZ = (0.0, 0.075, 0.014), 0.166, 0.146, 0.152

    def torso_mesh(name, segs, rings):
        return M.blob(name, G(*TC), (TRX, TRZ, TRY), segs=segs, rings=rings, color=FUR, mat=SOFT,
                      warp=lambda c: V((c.x * (1.0 + 0.07 * max(0.0, -c.z)), c.y * (1.0 + 0.05 * max(0.0, -c.z)), c.z)))

    def head_mesh(name, segs, rings):
        return M.blob(name, G(*HC), (HRX, HRZ, HRY), segs=segs, rings=rings, color=FUR, mat=SOFT,
                      warp=lambda c: V((c.x * (1.0 + 0.06 * max(0.0, -c.z)), c.y, c.z)))
    ttree = M.proxy_tree(lambda: torso_mesh("torso_hi", 48, 32))
    htree = M.proxy_tree(lambda: head_mesh("head_hi", 48, 32))

    def on(tree, c, az, el, off=0.0):
        d = gd(az, el)
        loc, nrm, _, _ = tree.ray_cast(G(*c) + d * 2.0, -d, 4.0)
        return loc + nrm * off, nrm

    def hs(az, el, off=0.0):
        return on(htree, HC, az, el, off)

    def ts(az, el, off=0.0):
        return on(ttree, TC, az, el, off)

    def fuzz(name, tree_fn, az, el, out, length, width, color=FUR, droop=0.25, sink=0.012):
        """A tuft of fur rooted at (az, el) on a surface, leaning `out` (0 = along the surface downward,
        1 = straight out)."""
        p, n = tree_fn(az, el)
        down = V((0, 0, -1))
        t = (down - n * down.dot(n))
        t = t.normalized() if t.length > 1e-6 else n.orthogonal().normalized()
        d = (t * (1.0 - out) + n * out).normalized()
        return M.straw(name, p - n * sink, d, length, width, up=tuple(n), droop=droop, thick=0.38, color=color, mat=SOFT, n=3,
                       wave=None)

    # ------------------------------------------------------------ body: torso, belly patch, neck ruff, tail nub
    body = [torso_mesh("torso", 20, 12)]
    bp, bn = ts(0, -33)
    body.append(M.pad("belly", ttree, bp, bn, 0.104, 0.007, segs=22, rings=(0.0, 0.45, 0.78, 0.93, 1.0), color=BEL, mat=SOFT,
                      up=G(0, 1, 0), aspect=1.15, outline=lambda a: 1.0 + 0.07 * math.sin(11 * a) + 0.03 * math.sin(5 * a + 1.0),
                      sink=0.004))
    # the neck ruff: ONE smooth layer of fur lying on the torso round the neck, its lower edge a ring of
    # soft tuft points; its top runs up inside the head, so it hides the crease from every side
    NT = 15                                   # tuft points round the collar

    def collar_at(u, v):
        az = -180.0 + 360.0 * u
        f = (u * NT) % 1.0
        tip = 1.0 - abs(f - 0.5) * 2.0       # 1 at a tuft point, 0 between
        front = math.exp(-(az / 38.0) ** 2)  # shorter over the chest, clear of the belly patch
        k = int(min(u, 0.9999) * NT)
        long_ = 1.0 + 0.32 * math.sin(k * 2.39 + 0.7)   # tufts of uneven length
        el_bot = 14.0 - 16.0 * long_ * tip ** 0.8 + 12.0 * front
        el = 42.0 + (el_bot - 42.0) * v
        return ts(az, el), tip

    def collar_out(u, v):
        (p, n), tip = collar_at(u, v)
        return p + n * (0.02 + 0.006 * math.sin(math.pi * v) - 0.008 * v * v * tip)

    def collar_in(u, v):
        (p, n), _ = collar_at(u, v)
        return p - n * 0.004
    body.append(M.shell("ruff", collar_out, NT * 2, 4, inner=collar_in, color=FUR, mat=SOFT, rim_color=FUR))
    # tail nub at the back bottom, with two tufts
    tp, tn = ts(180, -38)
    body.append(M.blob("tailnub", tp - tn * 0.004, (0.034, 0.03, 0.03), segs=10, rings=6, color=FUR, mat=SOFT))
    for k, dx in enumerate((-1, 1)):
        body.append(M.straw(f"tailtuft{k}", tp + G(dx * 0.012, 0.0, 0.0), (tn + G(dx * 0.4, -0.3, 0)).normalized(), 0.04, 0.026,
                            up=tuple(tn), droop=0.2, thick=0.36, color=FUR, mat=SOFT, n=3, wave=None))
    R.part("body", body, G(0, 0, 0), ground=False)

    # ------------------------------------------------------------ head: fur, cheek fluff, nose, blush, lip line, fangs
    NECK = G(0, 0.0, -0.005)
    head = [head_mesh("skull", 22, 14)]
    for s in (-1, 1):
        for k, (az, el, ln) in enumerate(((74, -18, 0.05), (86, -6, 0.045), (82, -30, 0.042), (98, -20, 0.04))):
            head.append(fuzz(f"cheekfluff{s}{k}", hs, s * az, el, 0.55, ln, 0.032, droop=0.15))
    # nose: a little rounded button with two nostrils
    NP, NN = hs(0, -5)
    upt = (V((0, 0, 1)) - NN * NN.z).normalized()
    nose = M.blob("nosebud", (0, 0, 0), (0.021, 0.014, 0.0135), segs=10, rings=6, color=FUR_L, mat=SOFT,
                  warp=lambda c: V((c.x * (1.0 + 0.25 * max(0.0, c.z)), c.y, c.z)))
    M.put(nose, NP + NN * 0.003, NN, up=tuple(upt))
    head.append(nose)
    for s in (-1, 1):
        head.append(M.blob(f"nostril{s}", NP + NN * 0.0125 - upt * 0.004 + V((s * 0.0075, 0, 0)), (0.0042, 0.0032, 0.003), segs=6, rings=4,
                           color=FUR_S))
    # blush
    for s in (-1, 1):
        p, n = hs(s * 40, -13)
        ck = M.blob(f"blush{s}", (0, 0, 0), (0.032, 0.021, 0.012), segs=14, rings=5, color="cheek_pink", mat="flat")
        M.put(ck, p - n * 0.0075, n, roll=-12 * s)   # a soft oval pad sunk into the fur, not a flat hexagon
        head.append(ck)
    # the lip line (a wide smile, corners up) and the two fangs hanging from it
    MW, MEL, MCURVE = 19.0, -18.0, 3.6

    def lip_el(az):
        return MEL + MCURVE * (az / MW) ** 2
    lip = [hs(a, lip_el(a) + (1.2 * (abs(a) - MW) / 3.0 if abs(a) > MW else 0.0), 0.0018)[0]
           for a in (-MW - 3.0, -MW, -MW * 0.6, -MW * 0.25, 0.0, MW * 0.25, MW * 0.6, MW, MW + 3.0)]
    head.append(M.tube("lipline", lip, lambda t: 0.0021 + 0.0013 * math.sin(math.pi * t), sides=5, color="ink", mat="flat",
                       cap_offset=0.7))
    for s in (-1, 1):
        a = s * 7.5
        p, n = hs(a, lip_el(a) + 0.4, 0.001)
        dn = (V((0, 0, -1)) + n * 0.35).normalized()
        fang = M.cone(f"fang{s}", 0.027, 0.0082, segs=6, color="trim_white", bend=0.12)
        M.put(fang, p, dn, up=tuple(n))
        head.append(fang)
    R.part("head", head, NECK, parent="body", ground=False, ao_from=("body",))

    # ------------------------------------------------------------ the open mouth (jaw): cavity + tongue, scaled about the lip line
    MDEEP = 0.052

    def cavity(u, v):
        a = (u * 2 - 1) * MW
        depth = MDEEP * math.sqrt(max(0.0, 1.0 - (a / MW) ** 2)) ** 0.8 + 0.003
        return hs(a, lip_el(a) - 0.3 - v * math.degrees(depth / 0.15), 0.0012)[0]

    def mpaint(i, j):
        if j >= 3 and 2 <= i <= 7:
            return ("tongue", "flat")
        if j >= 4 and i in (1, 8):
            return ("tongue", "flat")
        return None
    mouth = M.patch("mouthcav", cavity, 10, 6, color="mouth_dark", flip=True, paint=mpaint)
    MP, MN = hs(0, lip_el(0), 0.0012)
    R.part("jaw", [mouth], MP, parent="head", z_out=MN, ground=False)

    # ------------------------------------------------------------ eyes: white, two-tone amber iris, ink pupil, glints, lash line
    EW, EH, EAZ, EEL = 0.047, 0.054, 26.0, 5.0
    for side, s in (("l", 1), ("r", -1)):
        p, n = hs(s * EAZ, EEL)
        out = V((n.x, n.y, 0)).normalized()
        en = (n + out * 0.35).normalized()
        lash = []
        for k in range(9):
            a = math.radians(10 + 160 * k / 8)
            lash.append(V((s * EW * 1.05 * math.cos(a), EH * 1.05 * math.sin(a), 0.004)))
        lash = [V((s * EW * 1.32, EH * 0.68, 0.003))] + lash
        lash_ob = M.tube(f"lash{side}", lash, lambda t: 0.0042 + 0.0034 * math.sin(math.pi * min(1.0, t * 1.12)), sides=5, color="ink",
                         mat="flat", cap_offset=0.7)
        # a thin dark-brown limbal ring (never glows) round one amber disc (a glow cell), an ink pupil:
        # the iris glows softly at night and the ring keeps the eye from reading as a glowing bullseye.
        # glow_amber (#ff9a26) read lemon yellow under the key light at any glow; glow_amber_dim stays amber.
        M.eye(R, f"eye_{side}", f"pupil_{side}", p - n * 0.005, en, EW, EH, parent="head", depth=EW * 0.27, sclera="trim_white",
              iris="iris_brown", iris_r=0.82, iris2="glow_amber_dim", iris2_r=0.74, pupil="ink", pupil_r=0.38, glint="glow_white",
              look=(-s * 0.05, -0.06), segs=18, eye_extra=[lash_ob])
        # brow: a short dark arc well above the eye, thick in the middle
        pts = [hs(s * (13.0 + 22.0 * (i / 4)), EEL + 27.0 + 3.5 * math.sin(math.pi * i / 4) - 2.0 * (i / 4), 0.004)[0] for i in range(5)]
        pc, nc = hs(s * 24.0, EEL + 29.0)
        brow = M.tube(f"brow{side}", pts, [0.0045, 0.0072, 0.0078, 0.0066, 0.0042], sides=6, color="ink", mat="flat", cap_offset=0.8)
        R.part(f"brow_{side}", [brow], pc, parent="head", z_out=nc, ground=False)
        # shut eyes (> <) for a hit, hidden until shown
        chev = [hs(s * (EAZ + 8.0), EEL + 9.0, 0.005)[0], hs(s * (EAZ - 7.0), EEL + 0.5, 0.005)[0], hs(s * (EAZ + 8.0), EEL - 8.0, 0.005)[0]]
        R.part(f"alt_squint_{side}", [M.tube(f"chev{side}", chev, [0.005, 0.0072, 0.005], sides=5, color="ink", mat="flat", cap_offset=0.7)],
               p, parent="head", ground=False)

    # ------------------------------------------------------------ ears: big cupped shells, pink-lilac inside, lilac fluff at the base
    UP = V((0, 0, 1))
    for side, s in (("l", 1), ("r", -1)):
        p, n = hs(s * 40, 50)
        B = p - n * 0.03
        D_ = G(s * 0.58, 1.0, -0.1).normalized()
        N0 = G(s * 0.3, 0.05, 1.0)
        N = (N0 - D_ * N0.dot(D_)).normalized()
        Wd = N.cross(D_).normalized()
        L = 0.225

        def hw(t):
            return 0.08 * (1.0 - t) ** 0.62 * (1.0 + 1.0 * t * (1.0 - t)) + 0.004

        def ear_out(u, v, B=B, D_=D_, N=N, Wd=Wd, L=L, hw=hw):
            x = (u - 0.5) * 2.0
            w = hw(v)
            return B + D_ * (L * v) + N * (-0.03 * v * v + 0.42 * x * x * w) + Wd * (x * w) + UP * (0.01 * v * v)

        def ear_in(u, v, N=N, ear_out=ear_out):
            return ear_out(u, v) - N * (0.02 * (1.0 - v) + 0.006)

        def paint(i, j):
            return (EAR, SOFT) if 1 <= i <= 6 and 1 <= j <= 9 else None
        ear = M.shell(f"ear{side}", ear_out, 8, 11, inner=ear_in, color=FUR, mat=SOFT, paint=paint, rim_color=FUR)
        # (pale lilac fluff blades at the ear's base were tried: in the game they read as crystal shards)
        R.part(f"ear_{side}", [ear], B, parent="head", ground=False, ao_from=("head",),
               soft=dict(dir=D_, rigid=0.02, loops=6))

    # ------------------------------------------------------------ head tuft: one soft part of curled locks
    TP, TN = hs(0, 84)
    TPIN = TP - TN * 0.012
    tuft = [M.blob("tuftpin", TPIN, (0.016, 0.016, 0.012), segs=6, rings=4, color=FUR_L, mat=SOFT)]
    for k, (dx, dz, ln, w) in enumerate(((0.0, 0.3, 0.105, 0.042), (0.5, 0.12, 0.088, 0.036), (-0.5, 0.12, 0.088, 0.036),
                                          (0.28, -0.4, 0.075, 0.032), (-0.28, -0.4, 0.075, 0.032))):
        d = G(dx, 1.0, dz).normalized()
        tuft.append(M.straw(f"lock{k}", TPIN + G(dx * 0.012, 0, dz * 0.012), d, ln, w, up=tuple(G(-dx, 0.2, -dz - 0.6).normalized()),
                            droop=0.55, thick=0.38, color=FUR_L if k < 3 else FUR, mat=SOFT, n=4, wave=None))
    R.part("tuft", tuft, TPIN, parent="head", ground=False, soft=dict(dir=G(0, 1, 0), rigid=0.008, loops=4))

    # ------------------------------------------------------------ wings: wing (arm, thumb, inner membrane) > wingtip (fingers, outer membrane)
    SW = math.radians(14.0)   # the wings sweep back a little at rest
    for side, s in (("l", 1), ("r", -1)):
        S = G(s * 0.128, -0.028, -0.045)
        Xw = G(s * math.cos(SW), 0.0, -math.sin(SW))
        Yw = G(0.0, 1.0, 0.0)
        Nw = G(s * math.sin(SW), 0.0, math.cos(SW))

        def P(u, v, w=0.0, S=S, Xw=Xw, Yw=Yw, Nw=Nw):
            return S + Xw * u + Yw * v + Nw * w

        def lerp2(a, b, t):
            return (a[0] + (b[0] - a[0]) * t, a[1] + (b[1] - a[1]) * t)
        W2 = (0.25, 0.15)
        F1, F2, F3 = (0.30, -0.235), (0.46, -0.15), (0.565, 0.04)
        B2 = (-0.02, -0.08)
        A2 = (0.115, 0.125)   # control point of the arm's arc

        def arm2(t):
            a = lerp2((0.0, 0.0), A2, t)
            b = lerp2(A2, W2, t)
            return lerp2(a, b, t)

        def bow2(a, b, t, depth, toward):
            """Point t along a -> b, bowed by `depth` (sin profile) toward the 2D point `toward`."""
            q = lerp2(a, b, t)
            dx, dy = b[0] - a[0], b[1] - a[1]
            ln = math.hypot(dx, dy)
            nx, ny = -dy / ln, dx / ln
            if nx * (toward[0] - q[0]) + ny * (toward[1] - q[1]) < 0:
                nx, ny = -nx, -ny
            k = depth * math.sin(math.pi * t)
            return (q[0] + nx * k, q[1] + ny * k)

        # inner membrane: between the arm (top), finger 1 (outer edge), the scalloped trailing edge and the body
        def inner(u, v):
            top = arm2(u)
            bot = bow2(B2, F1, u, 0.05, W2)
            q = lerp2(bot, top, v)
            return P(q[0], q[1], -0.012 * math.sin(math.pi * u) * math.sin(math.pi * v))
        mem_in = M.shell(f"memin{side}", inner, 9, 5, thick=0.007, color=MEM, mat=SOFT, rim_color=FUR_S)
        arm_pts = [P(-0.03, -0.004)] + [P(*arm2(t)) for t in (0.0, 0.25, 0.5, 0.75, 1.0)]
        arm = M.tube(f"arm{side}", arm_pts, [0.03, 0.03, 0.029, 0.026, 0.023, 0.02], sides=9, color=FUR, mat=SOFT, cap_offset=0.6)
        # thumb: a stubby violet digit up off the wrist with a little cream claw
        T0 = P(W2[0] - 0.006, W2[1] + 0.012, 0.004)
        T1 = P(W2[0] - 0.02, W2[1] + 0.05, 0.006)
        thumb = M.tube(f"thumb{side}", [T0, T0.lerp(T1, 0.5), T1], [0.013, 0.012, 0.0095], sides=7, color=FUR, mat=SOFT, cap_offset=0.7)
        tcl = M.cone(f"thumbclaw{side}", 0.022, 0.0068, segs=6, color=CLAW, mat="flat", bend=0.35, lo=True)
        M.put(tcl, T1 - (T1 - T0).normalized() * 0.004, (T1 - T0).normalized(), up=tuple(-Xw))
        def vein(name, fn, a0, r0, a1, r1, bow):
            """A thin vein through a membrane (proud on both faces), following its billow."""
            pts = []
            for k in range(4):
                t = k / 3
                pts.append(fn(a0 + (a1 - a0) * t + bow * math.sin(math.pi * t), r0 + (r1 - r0) * t))
            return M.tube(name, pts, [0.0044, 0.004, 0.0033, 0.0024], sides=4, color=FUR, mat=SOFT, cap_offset=0.5)
        vein_in = vein(f"veinC{side}", inner, 0.62, 0.92, 0.72, 0.3, -0.05)
        R.part(f"wing_{side}", [mem_in, arm, thumb, tcl, vein_in], S, parent="body", ball=0.038, ball_color=FUR, ball_segs=10,
               ground=False)

        # outer membrane: a fan round the wrist between finger 1 and finger 3, scalloped between the fingertips
        def edge(a):
            if a <= 0.5:
                return bow2(F1, F2, a * 2.0, 0.045, W2)
            return bow2(F2, F3, a * 2.0 - 1.0, 0.04, W2)

        def outer(a, r):
            rr = 0.07 + 0.93 * r
            e = edge(a)
            q = (W2[0] + (e[0] - W2[0]) * rr, W2[1] + (e[1] - W2[1]) * rr)
            t = (a * 2.0) % 1.0 if a < 1.0 else 0.0
            return P(q[0], q[1], -0.012 * math.sin(math.pi * t) * math.sin(math.pi * r))
        mem_out = M.shell(f"memout{side}", outer, 12, 5, thick=0.007, color=MEM, mat=SOFT, rim_color=FUR_S)
        tip = [mem_out]
        tip.append(vein(f"veinA{side}", outer, 0.05, 0.42, 0.21, 0.86, 0.03))
        tip.append(vein(f"veinB{side}", outer, 0.55, 0.42, 0.73, 0.86, 0.03))
        for k, F_ in enumerate((F1, F2, F3)):
            pts = [P(*lerp2(W2, F_, t), 0.0) for t in (0.0, 0.3, 0.65, 1.0)]
            tip.append(M.tube(f"finger{side}{k}", pts, [0.012, 0.0105, 0.0085, 0.0062], sides=6, color=FUR, mat=SOFT, cap_offset=0.8))
            d = (P(*F_) - P(*W2)).normalized()
            hook = M.cone(f"fclaw{side}{k}", 0.016, 0.0055, segs=5, color=FUR_S, mat="flat", bend=0.4, lo=True)
            M.put(hook, P(*F_) - d * 0.004, d, up=tuple(Nw))
            tip.append(hook)
        f1dir = (P(*F1) - P(*W2)).normalized()
        R.part(f"wingtip_{side}", tip, P(*W2), parent=f"wing_{side}", z_out=Nw, up=f1dir, ball=0.025, ball_color=FUR, ball_segs=9,
               ground=False)

    # ------------------------------------------------------------ feet: tiny purple paws on short legs
    for side, s in (("l", 1), ("r", -1)):
        H = G(s * 0.062, -0.214, 0.032)
        A = H + G(s * 0.006, -0.042, 0.01)
        foot = [M.tube(f"leg{side}", [H, H.lerp(A, 0.5), A], [0.017, 0.016, 0.015], sides=7, color=FUR, mat=SOFT, cap_offset=0.5)]
        foot.append(M.blob(f"paw{side}", A + G(0, -0.008, 0.01), (0.026, 0.03, 0.019), segs=10, rings=6, color=FUR_L, mat=SOFT))
        for k, dx in enumerate((-0.014, 0.0, 0.014)):
            tq = A + G(dx, -0.012, 0.034 - abs(dx) * 0.3)
            foot.append(M.blob(f"toe{side}{k}", tq, (0.0095, 0.011, 0.009), segs=7, rings=4, color=FUR_L, mat=SOFT))
            cl = M.cone(f"toeclaw{side}{k}", 0.011, 0.004, segs=5, color=CLAW, mat="flat", bend=0.4, lo=True)
            M.put(cl, tq + G(0, -0.002, 0.008), G(0, -0.5, 1.0).normalized(), up=tuple(G(0, -1, 0)))
            foot.append(cl)
        R.part(f"foot_{side}", foot, H, parent="body", ball=0.02, ball_color=FUR, ball_segs=8, ground=False)

    poses = [
        ("rest", {}),
        ("upstroke: fold", {"wing_l": [("z", 50), ("y", 12)], "wing_r": [("z", -50), ("y", -12)],
                            "wingtip_l": [("y", -45)], "wingtip_r": [("y", 45)], "ear_l": [("x", -15)], "ear_r": [("x", -15)]}),
        ("downstroke", {"wing_l": [("z", -38), ("y", -14), ("x", 12)], "wing_r": [("z", 38), ("y", 14), ("x", 12)],
                        "wingtip_l": [("y", 18)], "wingtip_r": [("y", -18)], "foot_l": [("x", 30)], "foot_r": [("x", 30)]}),
        ("dive V", {"head": [("x", 12)], "wing_l": [("z", 62), ("y", 38)], "wing_r": [("z", -62), ("y", -38)],
                    "wingtip_l": [("y", -30)], "wingtip_r": [("y", 30)], "ear_l": [("x", -55)], "ear_r": [("x", -55)],
                    "foot_l": [("x", -50)], "foot_r": [("x", -50)]}),
        ("wrap (death)", {"wing_l": [("z", -15), ("y", -75)], "wing_r": [("z", 15), ("y", 75)],
                          "wingtip_l": [("y", 100)], "wingtip_r": [("y", -100)], "head": [("x", 15)]}),
        ("hit, look", {"head": [("y", 30), ("x", -14), ("z", 10)], "wing_l": [("z", 75)], "wing_r": [("z", -20)],
                       "ear_l": [("x", 35)], "ear_r": [("z", -30)], "jaw": [("move", (0, 0, 0))], "tuft": [("x", -30)]}),
    ]
    R.shade({FUR: FUR_S}, ao_cut=0.55, down=-0.62, parts=["body", "head"])
    R.shade({BEL: BEL_S}, ao_cut=0.5, down=-0.7, parts=["body"])
    R.shade({EAR: EAR_S}, ao_cut=0.62, down=-0.9, parts=["ear_l", "ear_r"])
    return R.finish(ao_radius=0.16, ao_strength=0.8, poses=poses, pose_views=((-25, 12), (155, 12)),
                    closeups=[("face", 2.1, tuple(G(0, 0.07, 0.12)))])


# ====================================================================== SPIDER
def spider():
    """A plump, fuzzy toy spider (POUNCE). A small berry-plum head (plush fur, a fluffy cheek and chin ruff, a
    curled tuft), big eyes (white, a two-tone amber iris that glows softly at night, an ink pupil, glints, a lash
    line), six small amber eyes in an arc above them, blush, a pink nose, a smiling lip line with two fangs and an
    open mouth for the hiss; a big glossy violet abdomen with the glowing jack-o'-lantern face CARVED into its back
    (F.carve pockets: dark rim, glowing walls and floor), spinnerets; eight chunky ringed legs that fade from violet
    to pale lilac tips.

    Part tree: body (the small dark thorax hub; every leg roots in it) > head (pivot at the neck, inside the hub)
        > eye_l/r > pupil_l/r, eye_s0..2 (each a mirrored PAIR of small eyes, inner to outer: the blink ripples
        outward), fang_l/r, jaw (the open mouth, scaled in Y about the lip line), tuft (soft), alt_squint (> <, both
        eyes, hidden);
      body > abdomen (pivot at the pedicel) > alt_silk (the drop thread, 0.25 m along -Z at rest, hidden: the
        animator turns it to point up and stretches it);
      body > leg_l0..3 / leg_r0..3 (pivot at the hip, sunk in the hub; 0 = front) > shin_l0..3 / shin_r0..3 (pivot
        at the knee, ball joint). Legs and shins are translation-only (two-bone IK); the shin's IK end is the foot
        contact point, which is on the ground at rest."""
    K.reset()
    VIO, VIO_S = "hat_violet", "spider_plum"   # abdomen and legs, and their shade / ring grooves
    MID, TIP = "bat_wing", "bat_belly"         # the fade to the pale tips
    BER, BER_S = "spider_berry", "licorice_black"   # head fur and its shade
    SOFT = "plush"
    Mx = M.Matrix
    R = M.Rig("monster_spider", height=0.7, radius=0.45)

    def gd(az, el):
        a, e = math.radians(az), math.radians(el)
        return G(math.sin(a) * math.cos(e), math.sin(e), math.cos(a) * math.cos(e))

    def smooth01(t):
        t = max(0.0, min(1.0, t))
        return t * t * (3.0 - 2.0 * t)

    # ------------------------------------------------------------ head shape (a fur ball, fuller low: chubby cheeks)
    HC, HRX, HRY, HRZ = (0.0, 0.345, 0.185), 0.205, 0.188, 0.178

    def head_mesh(name, segs, rings):
        return M.blob(name, G(*HC), (HRX, HRZ, HRY), segs=segs, rings=rings, color=BER, mat=SOFT,
                      warp=lambda c: V((c.x * (1.0 + 0.07 * max(0.0, -c.z)), c.y, c.z)))
    htree = M.proxy_tree(lambda: head_mesh("head_hi", 48, 32))

    def hs(az, el, off=0.0):
        d = gd(az, el)
        loc, nrm, _, _ = htree.ray_cast(G(*HC) + d * 2.0, -d, 4.0)
        return loc + nrm * off, nrm

    # ------------------------------------------------------------ body: the thorax hub the legs and the waist root in
    R.part("body", [M.blob("thorax", G(0, 0.285, -0.01), (0.13, 0.17, 0.112), segs=14, rings=8, color=VIO_S, mat=SOFT)],
           G(0, 0.285, 0.0))

    # ------------------------------------------------------------ abdomen: glossy, plump, the jack face carved into its back
    AC = (0.0, 0.405, -0.30)
    # (final fixes) plush, not flat: a short soft pile like the head (with a low sheen in the animator); the flat
    # shell under a tight highlight read as a plastic ball (decision 14)
    abd = M.blob("abdomen_shell", (0, 0, 0), (0.262, 0.282, 0.25), segs=22, rings=14, color=VIO, mat=SOFT,
                 warp=lambda c: V((c.x * (1.0 - 0.07 * max(0.0, -c.y)), c.y, c.z * (1.0 - 0.05 * max(0.0, -c.y)))),
                 rot=Mx.Rotation(math.radians(12), 3, "X"))
    M.bake(abd)
    # carve in a frame whose front (-Y) is the marking's direction (up 42 degrees from straight back) and whose
    # up is the face's up (toward the spider's front): F.carve cuts along +Y from the front
    dm = G(0, math.sin(math.radians(42)), -math.cos(math.radians(42))).normalized()
    fu = G(0, math.cos(math.radians(42)), math.sin(math.radians(42))).normalized()
    cy = -dm
    cx = cy.cross(fu).normalized()
    Rm = Mx((cx, cy, fu)).transposed()
    abd.data.transform(Rm.inverted().to_4x4())
    abd.data.update()
    feats = []
    for s in (-1, 1):
        feats.append({"outline": F.rounded_triangle(s * 0.088, 0.06, 0.11, 0.094, tilt=-s * 7, round_=0.22, arc_pts=2),
                      "depth": 0.03, "inset": 0.008, "rim_frac": 0.4, "rim_w": 0.005, "rim": VIO_S, "wall": "glow_jack",
                      "color": "glow_jack"})
    feats.append({"shape": F.shape_of(F.grin, cx=0.0, cz=-0.04, w=0.26, top_sag=0.015, depth=0.072, n=13, teeth=(-0.062, 0.062),
                                      tooth_w=0.036, tooth_h=0.025, lift=(0.022, 0.022)),
                  "depth": 0.032, "inset": 0.004, "rim_frac": 0.4, "rim_w": 0.003, "rim": VIO_S, "wall": "glow_jack",
                  "color": "glow_jack_hot"})
    F.carve(abd, feats)
    abd.data.transform(Mx.Translation(G(*AC)) @ Rm.to_4x4())
    abd.data.update()
    atree = M.bvh(abd)

    def on_abd(d):
        d = V(d).normalized()
        loc, nrm, _, _ = atree.ray_cast(G(*AC) + d * 2.0, -d, 4.0)
        return loc, nrm
    abdomen = [abd]
    spin = None
    for k, dx in enumerate((-1, 1)):
        p, n = on_abd(G(dx * 0.1, -0.42, -1.0))
        abdomen.append(M.blob(f"spinneret{k}", p - n * 0.006, (0.022, 0.026, 0.02), segs=6, rings=4, color=VIO_S, mat=SOFT))
    spin, spin_n = on_abd(G(0, -0.4, -1.0))
    R.part("abdomen", abdomen, G(0, 0.33, -0.075), parent="body")
    # the drop thread for the spawn (hidden): 0.25 m back along -Z from the spinnerets; posed up and stretched
    silk = M.tube("silk", [spin, spin + G(0, 0, -0.125), spin + G(0, 0, -0.25)], 0.0045, sides=4, color="ghost_white", mat="flat")
    R.part("alt_silk", [silk], spin, parent="abdomen", ground=False)

    # ------------------------------------------------------------ head: fur ball, cheek and chin ruff, nose, blush, lip line
    NECK = G(0, 0.30, 0.07)
    head = [head_mesh("skull", 22, 14)]
    NT = 13   # (polish) 11 -> 13 tufts, longer and fuller: more ruff in the head's silhouette

    def fluff_at(u, v):
        az = -155.0 + 310.0 * u
        f = (u * NT) % 1.0
        tip = 1.0 - abs(f - 0.5) * 2.0
        k = int(min(u, 0.9999) * NT)
        long_ = 1.0 + 0.3 * math.sin(k * 2.39 + 0.7)
        side = smooth01((abs(az) - 26.0) / 50.0)
        el_top = -56.0 + 52.0 * side      # low under the chin (clear of the smile), up to the eye line at the cheeks
        el_bot = el_top - 21.0 - (14.0 + 11.0 * side) * long_ * tip ** 0.8
        return hs(az, el_top + (el_bot - el_top) * v), tip

    def side_of(u):
        return smooth01((abs(-155.0 + 310.0 * u) - 26.0) / 50.0)

    def fluff_out(u, v):
        (p, n), tip = fluff_at(u, v)
        return p + n * (0.002 + (0.024 + 0.02 * side_of(u)) * v + (0.014 + 0.022 * side_of(u)) * v * v * tip)

    def fluff_in(u, v):
        (p, n), _ = fluff_at(u, v)
        return p - n * 0.005
    head.append(M.shell("ruff", fluff_out, NT * 2, 4, inner=fluff_in, color=BER, mat=SOFT, rim_color=BER))
    # nose: a little pink button between the eyes
    NP, NN = hs(0, -13)
    head.append(M.blob("nose", NP - NN * 0.002, (0.016, 0.012, 0.011), segs=8, rings=4, color="candy_pink", mat=SOFT))
    # blush
    for s in (-1, 1):
        p, n = hs(s * 46, -17)
        ck = M.blob(f"blush{s}", (0, 0, 0), (0.032, 0.021, 0.011), segs=10, rings=4, color="cheek_pink", mat="flat")
        M.put(ck, p - n * 0.007, n, roll=-14 * s)
        head.append(ck)
    # the lip line: a wide smile, corners up
    MW, MEL, MCURVE = 16.0, -29.0, 4.0

    def lip_el(az):
        return MEL + MCURVE * (az / MW) ** 2
    lip = [hs(a, lip_el(a) + (1.4 * (abs(a) - MW) / 3.0 if abs(a) > MW else 0.0), 0.0018)[0]
           for a in (-MW - 3.0, -MW, -MW * 0.6, -MW * 0.25, 0.0, MW * 0.25, MW * 0.6, MW, MW + 3.0)]
    head.append(M.tube("lipline", lip, lambda t: 0.0022 + 0.0014 * math.sin(math.pi * t), sides=5, color="ink", mat="flat",
                       cap_offset=0.7))
    R.part("head", head, NECK, parent="body", ground=False, ao_from=("body",))

    # ------------------------------------------------------------ fangs (parts: they click), the open mouth (jaw)
    for side, s in (("l", 1), ("r", -1)):
        a = s * 6.5
        p, n = hs(a, lip_el(a) + 0.5, 0.0015)
        dn = (V((0, 0, -1)) + n * 0.3).normalized()
        fang = M.cone(f"fangc{side}", 0.036, 0.01, segs=5, color="trim_white", bend=-0.15)
        M.put(fang, p - n * 0.002, dn, up=tuple(V((-s * 0.4, 0, 0)) + n))
        R.part(f"fang_{side}", [fang], p, parent="head", ground=False)
    MDEEP = 0.05

    def cavity(u, v):
        a = (u * 2 - 1) * MW
        depth = MDEEP * math.sqrt(max(0.0, 1.0 - (a / MW) ** 2)) ** 0.8 + 0.003
        return hs(a, lip_el(a) - 0.3 - v * math.degrees(depth / 0.15), 0.0012)[0]

    def mpaint(i, j):
        if j >= 3 and 2 <= i <= 7:
            return ("tongue", "flat")
        return None
    mouth = M.patch("mouthcav", cavity, 10, 6, color="mouth_dark", flip=True, paint=mpaint)
    MP, MN = hs(0, lip_el(0), 0.0012)
    R.part("jaw", [mouth], MP, parent="head", z_out=MN, ground=False)

    # ------------------------------------------------------------ big eyes: white, two-tone amber iris, ink pupil, glints, lash
    EW, EH, EAZ, EEL = 0.06, 0.066, 27.0, 1.0
    for side, s in (("l", 1), ("r", -1)):
        p, n = hs(s * EAZ, EEL)
        out = V((n.x, n.y, 0)).normalized()
        en = (n + out * 0.3).normalized()
        lash = []
        for k in range(9):
            a = math.radians(10 + 160 * k / 8)
            lash.append(V((s * EW * 1.05 * math.cos(a), EH * 1.05 * math.sin(a), 0.004)))
        lash = [V((s * EW * 1.3, EH * 0.72, 0.003))] + lash
        lash_ob = M.tube(f"lash{side}", lash, lambda t: 0.0042 + 0.0034 * math.sin(math.pi * min(1.0, t * 1.12)), sides=4,
                         color="ink", mat="flat", cap_offset=0.7)
        M.eye(R, f"eye_{side}", f"pupil_{side}", p - n * 0.006, en, EW, EH, parent="head", depth=EW * 0.3, sclera="trim_white",
              iris="iris_brown", iris_r=0.76, iris2="glow_amber_dim", iris2_r=0.68, pupil="ink", pupil_r=0.4, glint="glow_white",
              look=(-s * 0.05, -0.07), segs=16, eye_extra=[lash_ob])
        chev = [hs(s * (EAZ + 9.0), EEL + 10.0, 0.006)[0], hs(s * (EAZ - 8.0), EEL + 0.5, 0.006)[0],
                hs(s * (EAZ + 9.0), EEL - 9.0, 0.006)[0]]
        if side == "l":
            squint = []
        squint.append(M.tube(f"chev{side}", chev, [0.0055, 0.0078, 0.0055], sides=5, color="ink", mat="flat", cap_offset=0.7))
    R.part("alt_squint", squint, hs(0, EEL)[0], parent="head", ground=False)

    # ------------------------------------------------------------ six small eyes in an arc above, as three mirrored pairs
    def small_eye(name, az, el, r):
        p, n = hs(az, el)
        u = V((0, 0, 1))
        u = (u - n * u.dot(n)).normalized()
        rot = Mx(((-n).cross(u).normalized(), -n, u)).transposed()
        frame = Mx.Translation(p - n * 0.004) @ rot.to_4x4()
        d = r * 0.55
        objs = [M._dome(name + "_d", r, r, d, 8, 2, "iris_brown"),
                M._decal(name + "_i", 0.0, -r * 0.04, r * 0.78, r * 0.78, r, r, d, 0.0012, "glow_amber_dim", segs=8, rings=1),
                M._decal(name + "_p", 0.0, -r * 0.08, r * 0.4, r * 0.44, r, r, d, 0.0026, "ink", segs=6, rings=1),
                M._decal(name + "_g", -r * 0.3, r * 0.34, r * 0.22, r * 0.2, r, r, d, 0.0038, "glow_white", segs=5, rings=1)]
        for o in objs:
            o.matrix_world = frame
        return objs, p, n
    for k, (az, el, r) in enumerate(((8.0, 42.0, 0.022), (22.0, 38.5, 0.02), (35.0, 31.0, 0.017))):
        ol, pl, nl = small_eye(f"se{k}l", az, el, r)
        orr, pr, nr = small_eye(f"se{k}r", -az, el, r)
        nm = (nl + nr).normalized()
        R.part(f"eye_s{k}", ol + orr, (pl + pr) * 0.5 - nm * 0.004, parent="head", z_out=nm, ground=False)

    # ------------------------------------------------------------ head tuft: one soft part of curled locks
    TP, TN = hs(0, 76)
    TPIN = TP - TN * 0.012
    tuft = [M.blob("tuftpin", TPIN, (0.022, 0.022, 0.015), segs=7, rings=4, color=BER, mat=SOFT)]
    # one big curl rolling forward over the crown and two little ones to the sides (tubes, not blades:
    # flat blades read as a crown of leaves)
    for k, (sx, sc, lean) in enumerate(((0.0, 1.0, 0.0), (1.0, 0.55, 0.9), (-1.0, 0.55, 0.9))):
        rel = [(0.0, 0.0, 0.0), (0.0, 0.045, -0.006), (0.0, 0.082, 0.004), (0.0, 0.1, 0.03), (0.0, 0.094, 0.056),
               (0.0, 0.074, 0.064), (0.0, 0.06, 0.05), (0.0, 0.062, 0.037)]
        pts = []
        for (x, y, z) in rel:
            y, z = y * sc, z * sc
            pts.append(TPIN + G(sx * (0.012 + lean * y * 0.5), y * (1.0 - 0.4 * lean), z - 0.01 * abs(sx)))
        tuft.append(M.tube(f"curl{k}", M.catmull(pts, 2 if k == 0 else 1), lambda t, sc=sc: (0.026 - 0.019 * t) * (0.6 + 0.4 * sc), sides=6 if k == 0 else 5,
                           color=BER, mat=SOFT, cap_offset=0.8))
    R.part("tuft", tuft, TPIN, parent="head", ground=False, soft=dict(dir=G(0, 1, 0), rigid=0.008, loops=4))

    # ------------------------------------------------------------ legs: four a side, chunky, ringed, fading to pale tips
    LEG_AZ = (38.0, 72.0, 108.0, 142.0)
    UP_G = (0.0, 1.0, 0.0)

    def gv(x, y, z):
        return V((x, y, z))

    def to_b(p):
        return G(p.x, p.y, p.z)

    def groove(t, gs, w=0.045):
        return max([math.exp(-((t - g) / w) ** 2) for g in gs] + [0.0])

    def painter(path_fn, ts_dense):
        samples = [(to_b(path_fn(t)), t) for t in ts_dense]

        def t_of(c):
            best = min(samples, key=lambda st: (st[0] - c).length)
            return best[1]

        def ang_of(c, t):
            p0, p1 = to_b(path_fn(max(0.0, t - 0.01))), to_b(path_fn(min(1.0, t + 0.01)))
            tg = (p1 - p0).normalized()
            up = V((0, 0, 1))
            upr = (up - tg * up.dot(tg))
            upr = upr.normalized() if upr.length > 1e-6 else tg.orthogonal().normalized()
            sd = tg.cross(upr)
            r = c - to_b(path_fn(t))
            return math.atan2(r.dot(sd), r.dot(upr))
        return t_of, ang_of
    dense = [i / 80 for i in range(81)]
    # the fade to the pale tip over three rings of existing cells (no free palette cells in Lane M):
    # violet -> witch_purple -> bat_wing -> bat_belly
    FADE = ("witch_purple", MID, TIP)

    def shin_cell(a, b):
        """Cell of the shin's ring between t = a and b (None = the base violet)."""
        if any(a < g < b + 1e-6 or abs(a - g) < 1e-6 for g in (0.25, 0.48)):
            return VIO_S                       # the two dark rings (each the pair of segments meeting at a groove)
        if a >= 0.55 - 1e-6:
            return FADE[min(2, int(round((a - 0.55) / 0.055)))]
        return None

    def ring_paint(ob, ts, sides, cell_of):
        """Paints a M.tube by its rings: faces i * sides .. + sides - 1 lie between ts[i] and ts[i + 1]; the
        end cap (after the body faces) takes the last ring's cell."""
        bm = bmesh.new()
        bm.from_mesh(ob.data)
        bm.faces.ensure_lookup_table()
        nb = (len(ts) - 1) * sides
        groups = {}
        for i in range(len(ts) - 1):
            c = cell_of(ts[i], ts[i + 1])
            if c:
                groups.setdefault(c, []).extend(bm.faces[i * sides + j] for j in range(sides))
        last = cell_of(ts[-2], ts[-1])
        if last:
            groups.setdefault(last, []).extend(bm.faces[nb + sides:])   # the end cap (the start cap comes first)
        for c, fs in groups.items():
            K._paint_bm(bm, c, SOFT, fs)
        bm.to_mesh(ob.data)
        bm.free()
        ob.data.update()
    leg_names = []
    for side, s in (("l", 1), ("r", -1)):
        for k, az_deg in enumerate(LEG_AZ):
            az = math.radians(az_deg)
            out = gv(s * math.sin(az), 0.0, math.cos(az))
            hip = gv(s * 0.07, 0.275, 0.08 - 0.053 * k)
            C = gv(0.0, 0.0, -0.03)
            knee = C + out * 0.355 + gv(0, 0.39, 0)
            foot = C + out * 0.535                      # the contact point on the ground (the IK end)
            r_k = 0.355
            y_end = 0.034

            def upper_at(t, hip=hip, knee=knee):
                return hip.lerp(knee, t) + gv(0, 0.035 * math.sin(math.pi * t), 0)

            def shin_at(t, out=out, C=C, knee=knee):
                rr = r_k + (0.535 - r_k) * (1.0 - (1.0 - t) ** 1.6)
                return C + out * rr + gv(0, knee.y + (y_end - knee.y) * t, 0)
            UG = (0.4, 0.7)
            SG = (0.25, 0.48)
            ut = (0.0, 0.12, 0.24, 0.34, 0.4, 0.46, 0.57, 0.64, 0.7, 0.76, 0.88, 1.0)
            st = (0.0, 0.1, 0.19, 0.25, 0.31, 0.41, 0.48, 0.55, 0.61, 0.66, 0.71, 0.76, 0.82, 0.88, 0.94, 1.0)
            ur = [(0.058 - 0.006 * t) * (1.0 - 0.22 * groove(t, UG)) for t in ut]
            sr = [(0.053 - 0.012 * min(t / 0.8, 1.0) + 0.007 * math.exp(-((t - 0.93) / 0.06) ** 2)) * (1.0 - 0.2 * groove(t, SG))
                  for t in st]
            up_ob = M.tube(f"femur{side}{k}", [to_b(upper_at(t)) for t in ut], ur, sides=7, color=VIO, mat=SOFT, cap_offset=0.5)
            sh_ob = M.tube(f"tibia{side}{k}", [to_b(shin_at(t)) for t in st], sr, sides=7, color=VIO, mat=SOFT, cap_offset=0.85)
            # (polish) colours by ring, never by face position: every band edge is an edge loop, straight
            # round the tube (painting by where a face's centre lies gave a stair of single faces on the bends)
            ring_paint(up_ob, ut, 7, lambda a, b: VIO_S if any(a < g < b + 1e-6 or abs(a - g) < 1e-6 for g in UG) else None)
            ring_paint(sh_ob, st, 7, shin_cell)
            R.part(f"leg_{side}{k}", [up_ob], to_b(hip), parent="body", ground=True)
            R.part(f"shin_{side}{k}", [sh_ob], to_b(knee), parent=f"leg_{side}{k}", ball=0.056, ball_color=VIO, ball_segs=7)
            leg_names += [f"leg_{side}{k}", f"shin_{side}{k}"]

    # ------------------------------------------------------------ extreme poses for the review sheet
    lift = {}
    for k in range(4):
        for side, s in (("l", 1), ("r", -1)):
            grp_a = (k % 2 == 0) == (side == "l")
            if grp_a:
                lift[f"leg_{side}{k}"] = [("z", s * 28)]
                lift[f"shin_{side}{k}"] = [("z", -s * 30)]
    star = {}
    tuck = {}
    curl = {}
    for k in range(4):
        for side, s in (("l", 1), ("r", -1)):
            star[f"leg_{side}{k}"] = [("z", s * 8)]
            star[f"shin_{side}{k}"] = [("z", s * 55)]
            tuck[f"leg_{side}{k}"] = [("z", -s * 30)]
            tuck[f"shin_{side}{k}"] = [("z", -s * 70)]
            curl[f"leg_{side}{k}"] = [("z", s * 70)]
            curl[f"shin_{side}{k}"] = [("z", -s * 110)]
    star["body"] = [("move", (0, 0.25, 0))]
    tuck["body"] = [("move", (0, 0.2, 0))]
    curl["body"] = [("move", (0, 0.1, 0))]
    curl["abdomen"] = [("x", -25)]
    crouch = {"body": [("move", (0, -0.1, 0)), ("x", 10)], "abdomen": [("x", 18), ("y", 15)], "head": [("x", -15)]}
    for k in range(4):
        for side, s in (("l", 1), ("r", -1)):
            crouch[f"leg_{side}{k}"] = [("z", -s * 12)]
            crouch[f"shin_{side}{k}"] = [("z", s * 20)]
    poses = [
        ("rest", {}),
        ("step: group A up", lift),
        ("crouch, wiggle", crouch),
        ("star (apex)", star),
        ("tuck", tuck),
        ("look, curl", dict(curl, head=[("y", 35), ("x", -20)], jaw=[("move", (0, 0, 0))], fang_l=[("x", 25)], fang_r=[("x", 25)],
                            tuft=[("x", -30)])),
    ]
    R.shade({VIO: VIO_S}, ao_cut=0.5, down=-0.75, parts=["body"])   # (polish) not the legs: per-face shade cells stair-stepped them
    R.shade({BER: BER_S}, ao_cut=0.5, down=-0.8, parts=["head"])
    return R.finish(ao_radius=0.18, ao_strength=0.85, poses=poses, pose_views=((-35, 14), (145, 14)),
                    closeups=[("face", 2.2, tuple(G(0, 0.36, 0.2)))])


BUILDS = {"skeleton": skeleton, "mummy": mummy, "scarecrow": scarecrow, "gremlin": gremlin, "toadstool": toadstool,
          "ghost": ghost, "slime": slime, "pumpkin": pumpkin, "bat": bat, "spider": spider}

if __name__ == "__main__":
    M.run(BUILDS)
