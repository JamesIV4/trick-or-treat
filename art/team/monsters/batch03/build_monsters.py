"""The ten monsters (batch03). Each is an empty root '<asset_id>' with 'body' and optional
'part_a' / 'part_b' children whose origins sit at the C++ pivots (monster_art.cpp).

blender --background --factory-startup --python art/team/monsters/batch03/build_monsters.py -- [keys] [--nosheet] [--pieces]
keys: pumpkin bat skeleton ghost gremlin mummy slime scarecrow spider toadstool
"""
import math
import pathlib
import sys

sys.path.insert(0, str(pathlib.Path(__file__).resolve().parent))
import bmesh  # noqa: E402
import mon_kit as M  # noqa: E402
from mon_kit import D, F, K, V, Matrix, g2b  # noqa: E402

GLOW = "glow_jack"
HOT = "glow_jack_hot"


# ====================================================================== PUMPKIN HOPPER
def pumpkin():
    K.reset()
    W, H, LIFT = 0.88, 0.74, 0.05
    body = F.pumpkin_body("pbody", width=W, height=H, lobes=8, spl=4, rings=12, groove=0.12, plump=0.45,
                          fullness=0.8, flatness=0.7, top_dimple=0.07, dimple_w=0.42, lumps=0.03, seed=3,
                          belly=1.1, rot_z=-90.0)
    top = V(body["top"])
    # flatten the face a little so both eyes face the camera from 3/4 views
    K.deform(body, lambda co: V((co.x, co.y * (0.86 + 0.14 * min(1.0, abs(co.x) / (W * 0.5)) ** 2)
                                 if co.y < 0 else co.y, co.z)))
    ex, ez = 0.165, 0.47
    feats = [{"shape": F.convex_shape(F.rounded_triangle(s * ex, ez, 0.22, 0.2, tilt=8 * s, round_=0.22, arc_pts=1)),
              "color": HOT, "wall": GLOW} for s in (-1, 1)]
    feats.append({"shape": F.shape_of(F.grin, cx=0.0, cz=0.3, w=0.56, top_sag=0.05, depth=0.2, n=15,
                                      teeth=(-0.1, 0.1), tooth_w=0.075, tooth_h=0.065, lift=(0.05, 0.05)),
                  "color": HOT, "wall": GLOW, "inset": 0.02, "rim_w": 0.012})
    F.carve(body, feats, depth=0.085, inset=0.024)
    M.xform(body, Matrix.Translation((0, 0, LIFT)))
    top = top + V((0, 0, LIFT))
    parts = [body]
    parts.append(F.stem("stem", tuple(top), height=0.2, radius=0.05, color="stem_green", mat="leaves", curl=0.11,
                        lean_dir=math.radians(110), rings=7, cut_color="sage_leaf"))
    leaf = F.pumpkin_leaf("leaf", size=0.17, n=24, color="vine", under="vine_dark", two_sided=True, mat="leaves")
    F.seat(leaf, body, top.x - 0.13, top.y + 0.06, 200, lift=0.02, tilt=-18)
    parts.append(leaf)
    parts.append(F.tendril("tendril", (top.x + 0.07, top.y + 0.02, top.z + 0.02), turns=1.6, r0=0.06, r1=0.02,
                           rise=0.1, n=11, radius=0.013, color="vine", lead=(top.x + 0.02, top.y, top.z - 0.01)))
    # stubby root feet peeking out under the front
    for s in (-1, 1):
        parts.append(M.blob(f"foot{s}", (s * 0.19, -0.2, 0.045), (0.12, 0.15, 0.06), segs=8, rings=5,
                            color="vine_dark", mat="leaves"))
    # arms: chunky curly vine with a three-fingered leafy mitt
    arms = {}
    for name, s in (("part_a", -1), ("part_b", 1)):
        piv = g2b((s * 0.44, 0.4, 0.05))
        pts = [piv + V((-s * 0.07, 0.0, 0.01)), piv + V((s * 0.06, -0.03, 0.0)), piv + V((s * 0.15, -0.08, -0.06)),
               piv + V((s * 0.19, -0.14, -0.14))]
        pts = M.catmull(pts, 2)
        arm = M.tube(name + "_vine", pts, lambda t: 0.05 * (1 - 0.3 * t), sides=7, color="vine", mat="leaves",
                     cap_offset=0.5)
        end = pts[-1]
        d = (pts[-1] - pts[-2]).normalized()
        palm = M.blob(name + "_palm", end + d * 0.03, (0.06, 0.05, 0.065), segs=8, rings=5, color="stem_green",
                      mat="leaves")
        hand = [palm]
        side = d.cross(V((0, 1, 0))).normalized()
        for k, a in enumerate((-35, 0, 35)):
            fd = (Matrix.Rotation(math.radians(a), 3, side.cross(d)) @ d).normalized()
            fg = M.blob(f"{name}_f{k}", (0, 0, 0), (0.026, 0.026, 0.05), segs=6, rings=4, color="stem_green",
                        mat="leaves")
            M.put(fg, end + d * 0.05 + fd * 0.06, fd)
            hand.append(fg)
        arms[name] = ([arm] + hand, (s * 0.44, 0.4, 0.05))
    root = M.rig("monster_pumpkin", parts, arms)
    return M.finish(root, "monster_pumpkin")


# ====================================================================== BAT
def scallop_outline(tips, lead, root_low, depth=0.34, steps=2):
    """Wing outline (x out, z up): trailing edge scalloped between finger tips, leading edge points."""
    pts = [V(root_low)]
    chain = [V(root_low)] + [V(t) for t in reversed(tips)]
    hub = V(lead[1]) if len(lead) > 1 else V(lead[0])
    for a, b in zip(chain[:-1], chain[1:]):
        mid = (a + b) / 2
        ctrl = mid + (hub - mid) * depth
        for k in range(1, steps + 1):
            t = k / (steps + 1)
            pts.append(a * (1 - t) ** 2 + ctrl * 2 * t * (1 - t) + b * t * t)
        pts.append(b)
    for q in reversed(lead):
        pts.append(V(q))
    return pts


def membrane(name, outline, th, color, mat="felt", bevel=0.01):
    bm = bmesh.new()
    front = [bm.verts.new((p.x, -th / 2, p.y)) for p in outline]
    back = [bm.verts.new((p.x, th / 2, p.y)) for p in outline]
    bm.faces.new(front)
    bm.faces.new(list(reversed(back)))
    k = len(outline)
    for i in range(k):
        bm.faces.new((front[i], back[i], back[(i + 1) % k], front[(i + 1) % k]))
    bmesh.ops.recalc_face_normals(bm, faces=bm.faces)
    rim = [e for e in bm.edges if abs(e.verts[0].co.y - e.verts[1].co.y) < 1e-6]
    bmesh.ops.bevel(bm, geom=rim, offset=bevel, segments=1, profile=0.5, affect="EDGES", clamp_overlap=True)
    bmesh.ops.triangulate(bm, faces=[f for f in bm.faces if len(f.verts) > 4], quad_method="BEAUTY",
                          ngon_method="EAR_CLIP")
    return D.mesh(bm, name, color, mat, "box", smooth=True, center=(0.3, 0, 0))


def bat():
    K.reset()
    FUR, WING, BELLY = "bat_fur", "bat_wing", "bat_belly"
    C, R = V((0, 0, 0.03)), V((0.215, 0.2, 0.215))
    body = M.blob("bbody", C, R, segs=16, rings=10, color=FUR, mat="felt",
                  warp=lambda c: V((c.x, c.y, c.z * (1.0 + 0.08 * max(0.0, -c.y)))))
    parts = [body]
    # lilac tummy patch, slightly proud of the fur
    belly = M.blob("belly", (0, -0.085, -0.04), (0.14, 0.13, 0.135), segs=10, rings=6, color=BELLY, mat="felt")
    parts.append(belly)
    # fluffy head tuft
    for i, (x, a) in enumerate(((-0.035, -18), (0.0, 0), (0.04, 20))):
        t = M.cone(f"tuft{i}", 0.08 - abs(x) * 0.5, 0.032, segs=6, color=FUR, mat="felt", bend=0.25)
        p, n = M.ellipsoid_dir(C, R, (x * 4, 0.15, 1.0))
        M.put(t, p - n * 0.015, n + V((math.sin(math.radians(a)), 0.2, 0)), up=(0, 1, 0))
        parts.append(t)
    # big ears: fur cone with a lilac inner, tipped outward
    for s in (-1, 1):
        p, n = M.ellipsoid_dir(C, R, (s * 0.5, 0.05, 0.9))
        ear = M.cone(f"ear{s}", 0.27, 0.1, segs=8, color=FUR, mat="felt", bend=0.12)
        inner = M.cone(f"eari{s}", 0.2, 0.066, segs=6, color=BELLY, mat="felt", bend=0.1)
        ax = (n + V((s * 0.3, -0.05, 0.6))).normalized()
        M.put(ear, p - n * 0.03, ax, up=(0, -1, 0), scale=(1.0, 0.5, 1.0))
        M.put(inner, p - n * 0.03 + V((0, -0.03, 0.0)) + ax * 0.025, ax, up=(0, -1, 0), scale=(1.0, 0.3, 1.0))
        parts += [ear, inner]
    # glowing amber eyes with big pupils, pug nose, fangs, smile
    for s in (-1, 1):
        p, n = M.ellipsoid_pt(C, R, s * 0.085, 0.085)
        parts += M.eye_glow(f"eye{s}", p, n, 0.058, 0.066, glow=GLOW, pw=0.62, ph=0.7, roll=-12 * s,
                            look=(0.0, -0.08))
        p, n = M.ellipsoid_pt(C, R, s * 0.135, 0.005)
        parts.append(M.cheek(f"cheek{s}", p, n, 0.032, 0.02, roll=-15 * s))
        p, n = M.ellipsoid_pt(C, R, s * 0.035, -0.045)
        fang = M.cone(f"fang{s}", 0.045, 0.014, segs=5, color="eye_white")
        M.put(fang, p + V((0, -0.008, 0.015)), (0, -0.25, -1), up=(0, -1, 0))
        parts.append(fang)
    p, n = M.ellipsoid_pt(C, R, 0.0, 0.025)
    parts.append(M.blob("nose", p + n * 0.01, (0.03, 0.02, 0.022), segs=8, rings=4, color=BELLY, mat="felt"))
    parts.append(M.smile("mouth", p + V((0, 0.004, -0.032)), n, 0.08, 0.018, 0.012))
    # little hanging feet
    for s in (-1, 1):
        parts.append(M.blob(f"foot{s}", (s * 0.065, 0.0, -0.19), (0.035, 0.04, 0.03), segs=8, rings=4, color=FUR, mat="felt"))
    # wings (pivot at the shoulder, flap around Z)
    wings = {}
    for name, s in (("part_a", -1), ("part_b", 1)):
        tips = [V((0.66, 0.15)), V((0.55, -0.1)), V((0.33, -0.18))]
        lead = [V((0.0, 0.07)), V((0.14, 0.17)), V((0.3, 0.22)), V((0.48, 0.2))]
        outline = scallop_outline(tips, lead, (0.0, -0.1), depth=0.3)
        mem = membrane(name + "_mem", outline, 0.022, WING)
        bones = []
        wrist = V((0.3, 0.0, 0.215))
        bones.append(M.tube(name + "_arm", M.catmull([V((0.0, 0.0, 0.06)), V((0.14, 0.0, 0.175)), wrist], 2),
                            lambda t: 0.026 - 0.006 * t, sides=6, color=FUR, mat="felt", cap_offset=0.5))
        for k, tp in enumerate(tips):
            tip = V((tp.x * 0.97, 0.0, tp.y * 0.97))
            mid = wrist.lerp(tip, 0.5) + V((0, 0, 0.02))
            bones.append(M.tube(f"{name}_rib{k}", M.catmull([wrist, mid, tip], 2), lambda t: 0.018 - 0.01 * t,
                                sides=5, color=FUR, mat="felt", cap_offset=0.6))
        claw = M.cone(name + "_claw", 0.05, 0.016, segs=5, color=BELLY, bend=0.3)
        M.put(claw, wrist + V((0.0, 0.0, 0.01)), (0.3, 0, 1), up=(0, -1, 0))
        w = K.merge([mem] + bones + [claw], name + "_w")
        if s < 0:
            w = M.mirror_x(w, name + "_wm")
        M.xform(w, Matrix.Rotation(math.radians(-12 * s), 4, "Y"))   # tips swept slightly up
        M.xform(w, Matrix.Rotation(math.radians(14 * s), 4, "Z"))    # and a little back
        piv = g2b((s * 0.15, 0.04, 0.0))
        w.location = piv
        wings[name] = ([w], (s * 0.15, 0.04, 0.0))
    root = M.rig("monster_bat", parts, wings)
    return M.finish(root, "monster_bat", ground_ao=False, ao_radius=0.25, preview_lift=0.3)


# ====================================================================== SKELETON
def limb(name, pts, r, color="bone", mat="concrete", sides=6, flare=1.45, joints=()):
    """Bone limb through pts: flared knobbly ends (and flared joints at the given point indices)."""
    pts = [V(p) for p in pts]
    rr = [r * flare if (i in (0, len(pts) - 1) or i in joints) else r for i in range(len(pts))]
    return M.tube(name, pts, rr, sides=sides, color=color, mat=mat, cap_offset=0.6)


def skeleton():
    K.reset()
    B, BS, MAT = "bone", "bone_shade", "concrete"
    parts = []
    # legs (one bent sweep each, knobby knee) and feet
    for s in (-1, 1):
        hip, knee, ankle = V((s * 0.085, 0.0, 0.5)), V((s * 0.095, -0.02, 0.29)), V((s * 0.1, 0.0, 0.08))
        parts.append(limb(f"leg{s}", [hip, hip.lerp(knee, 0.5), knee, knee.lerp(ankle, 0.5), ankle], 0.03,
                          joints=(2,), flare=1.4))
        parts.append(M.blob(f"foot{s}", (s * 0.105, -0.045, 0.045), (0.07, 0.11, 0.05), segs=8, rings=5,
                            color=B, mat=MAT, warp=lambda c: V((c.x, c.y, c.z if c.z > 0 else c.z * 0.5))))
    # pelvis: a soft bony bowl with two dark hollows
    parts.append(M.blob("pelvis", (0, 0.0, 0.52), (0.15, 0.09, 0.075), segs=10, rings=6, color=B, mat=MAT,
                        warp=lambda c: V((c.x, c.y, c.z - 0.25 * c.x * c.x))))
    for s in (-1, 1):
        p, n = M.ellipsoid_pt((0, 0, 0.52), (0.15, 0.09, 0.075), s * 0.07, 0.52)
        parts.append(M.cheek(f"hollow{s}", p, n, 0.034, 0.028, color="bone_shade", segs=7))
    # spine and a ribcage of three curved ribs a side
    parts.append(M.tube("spine", [V((0, 0.03, 0.52)), V((0, 0.045, 0.7)), V((0, 0.03, 0.9))], 0.028, sides=6,
                        color=BS, mat=MAT))
    parts.append(M.blob("vert0", (0, 0.04, 0.6), (0.04, 0.035, 0.02), segs=6, rings=4, color=B, mat=MAT))
    for k, (z, rx) in enumerate(((0.66, 0.135), (0.74, 0.155), (0.82, 0.16))):
        for s in (-1, 1):
            pts = []
            for i in range(5):
                t = i / 4
                a = math.radians(10 + 140 * t)   # from the spine (back) round the side to the sternum
                pts.append(V((s * rx * math.sin(a), -0.025 + 0.095 * math.cos(a), z + 0.012 - 0.045 * t)))
            parts.append(M.tube(f"rib{k}{s}", pts, 0.021 - 0.0015 * k, sides=5, color=B, mat=MAT, cap_offset=0.5))
    parts.append(M.blob("sternum", (0, -0.1, 0.75), (0.035, 0.025, 0.1), segs=6, rings=4, color=B, mat=MAT))
    # collar bone and a jaunty orange bandana
    parts.append(limb("collar", [(-0.2, 0.0, 0.86), (0.0, 0.0, 0.87), (0.2, 0.0, 0.86)], 0.026, flare=1.5))
    parts.append(K.torus("scarf", 0.085, 0.035, color="pumpkin", mat="fabric", major_seg=10, minor_seg=4,
                         loc=(0, -0.01, 0.885), scale=(1.0, 0.9, 0.8)))
    flap = M.lathe("flap", [(0.0, -0.09), (0.05, -0.05), (0.075, 0.0), (0.0, 0.01)], 6, color="pumpkin", mat="fabric")
    M.put(flap, (0.0, -0.085, 0.87), (0, 0.15, 1), up=(0, -1, 0), scale=(1.0, 0.35, 1.0))
    parts.append(flap)
    parts.append(M.blob("knot", (0.04, -0.085, 0.88), (0.035, 0.03, 0.03), segs=6, rings=4, color="pumpkin_dark",
                        mat="fabric"))
    # big round skull, cheekbones, a little jaw
    SC, SR = V((0, -0.01, 1.05)), V((0.205, 0.19, 0.185))
    parts.append(M.blob("skull", SC, SR, segs=16, rings=10, color=B, mat=MAT,
                        warp=lambda c: V((c.x * (1.0 - 0.12 * max(0.0, -c.z)), c.y, c.z))))
    parts.append(M.blob("jaw", (0, -0.065, 0.905), (0.125, 0.1, 0.07), segs=10, rings=5, color=B, mat=MAT))
    for s in (-1, 1):
        p, n = M.ellipsoid_pt(SC, SR, s * 0.082, 1.06)
        parts += M.eye_glow(f"sock{s}", p, n, 0.068, 0.078, glow="ink", pupil="glow_magic_teal", pw=0.32, ph=0.32,
                            roll=8 * s, look=(0.05 * -s, -0.05), segs=10)
        p, n = M.ellipsoid_pt(SC, SR, s * 0.135, 0.965)
        parts.append(M.cheek(f"cheek{s}", p, n, 0.03, 0.018, roll=-12 * s))
    p, n = M.ellipsoid_pt(SC, SR, 0.0, 0.985)
    nose = K.polygon_prism("nose", [(-0.022, 0.012), (0.022, 0.012), (0.0, -0.02)], 0.01, color="ink")
    M.put(nose, p - n * 0.005, n)
    parts.append(nose)
    # grin with stitched teeth across the jaw line
    p, n = M.ellipsoid_pt((0, -0.065, 0.905), (0.125, 0.1, 0.07), 0.0, 0.925)
    parts.append(M.smile("grin", p, n, 0.15, 0.022, 0.013, roll=0.0))
    for k in range(-2, 3):
        q, qn = M.ellipsoid_pt((0, -0.065, 0.905), (0.125, 0.1, 0.07), k * 0.026, 0.917 - 0.009 * (1 - (k / 2.5) ** 2) * 2)
        tick = K.box(f"tooth{k}", (0.006, 0.024, 0.006), color="ink")
        M.put(tick, q, qn, up=(0, 0, 1), scale=(1, 1, 1))
        parts.append(tick)
    # arms: shoulder pivots, hanging relaxed; the right (+x) hand holds a bone to throw
    arms = {}
    for name, s in (("part_a", -1), ("part_b", 1)):
        sh = g2b((s * 0.22, 0.84, 0.0))
        el = sh + V((s * 0.04, 0.01, -0.21))
        wr = el + V((s * 0.0, -0.1, -0.15)) if s > 0 else el + V((s * 0.01, -0.03, -0.18))
        objs = [M.blob(name + "_sh", sh, (0.042, 0.042, 0.042), segs=6, rings=4, color=B, mat=MAT),
                limb(name + "_arm", [sh, sh.lerp(el, 0.5), el, el.lerp(wr, 0.5), wr], 0.023, joints=(2,), flare=1.4)]
        hd = (wr - el).normalized()
        objs.append(M.blob(name + "_palm", wr + hd * 0.04, (0.05, 0.035, 0.045), segs=6, rings=4, color=B, mat=MAT))
        side = hd.cross(V((0, 1, 0))).normalized()
        for k, off in enumerate((-0.025, 0.0, 0.025)):
            fp = wr + hd * 0.09 + side * off * 1.3
            objs.append(M.blob(f"{name}_fi{k}", fp, (0.014, 0.014, 0.03), segs=6, rings=3, color=B, mat=MAT))
        if s > 0:
            objs.append(M.bone(name + "_held", wr + hd * 0.06 + V((0, 0.12, 0.0)), wr + hd * 0.06 + V((0, -0.2, 0.03)),
                               0.024, color=B, mat=MAT, sides=6, knob_segs=7, knob_rings=4))
        arms[name] = (objs, (s * 0.22, 0.84, 0.0))
    root = M.rig("monster_skeleton", parts, arms)
    return M.finish(root, "monster_skeleton", ao_radius=0.25)


# ====================================================================== GHOST
def ghost():
    K.reset()
    SHEET = "ghost_white"
    prof = [(0.0, 0.15), (0.3, 0.12), (0.445, 0.1), (0.44, 0.17), (0.405, 0.28), (0.37, 0.42), (0.345, 0.58),
            (0.335, 0.74), (0.333, 0.87), (0.318, 0.99), (0.275, 1.1), (0.195, 1.195), (0.1, 1.255), (0.0, 1.275)]

    def drape(c):
        a = math.atan2(c.y, c.x)
        low = max(0.0, 1.0 - (c.z - 0.1) / 0.5)
        r = math.hypot(c.x, c.y)
        wave = math.sin(5 * a + 1.2)
        k = 1.0 + 0.06 * wave * low
        z = c.z + 0.085 * wave * max(0.0, 1.0 - (c.z - 0.1) / 0.1) if r > 0.25 else c.z
        return V((c.x * k, c.y * 0.94 * k, z))
    body = M.lathe("gbody", prof, 30, color=SHEET, mat="cloud", project="sphere", warp=drape, center=(0, 0, 0.7))
    parts = [body]
    C, R = V((0, 0, 0.7)), V((0.35, 0.33, 0.58))
    for s in (-1, 1):
        p, n = M.ellipsoid_pt(C, R, s * 0.125, 0.93)
        parts += M.eye_cute(f"eye{s}", p, n, 0.068, 0.092, roll=-6 * s, glint="bat_belly")
        p, n = M.ellipsoid_pt(C, R, s * 0.2, 0.8)
        parts.append(M.cheek(f"cheek{s}", p, n, 0.05, 0.028, roll=-12 * s))
    p, n = M.ellipsoid_pt(C, R, 0.0, 0.79)
    mouth = M.blob("mouth", (0, 0, 0), (0.045, 0.055, 0.02), segs=10, rings=5, color="ink")
    M.put(mouth, p - n * 0.006, n)
    tongue = M.blob("tongue", (0, 0, 0), (0.028, 0.02, 0.012), segs=8, rings=4, color="cheek_pink")
    M.put(tongue, p + n * 0.004 + V((0, 0, -0.028)), n)
    parts += [mouth, tongue]
    arms = {}
    for name, s in (("part_a", -1), ("part_b", 1)):
        piv = g2b((s * 0.38, 0.72, 0.0))
        arm = M.blob(name + "_nub", (0, 0, 0), (0.07, 0.075, 0.16), segs=10, rings=7, color=SHEET, mat="cloud",
                     warp=lambda c: V((c.x * (1.0 + 0.35 * c.z), c.y, c.z)))
        M.put(arm, piv + V((s * 0.07, -0.04, -0.05)), (s * 0.85, -0.25, -0.45), up=(0, -1, 0))
        arms[name] = ([arm], (s * 0.38, 0.72, 0.0))
    root = M.rig("monster_ghost", parts, arms)
    return M.finish(root, "monster_ghost", ao_radius=0.35)


# ====================================================================== CANDY-CORN GREMLIN
def gremlin():
    K.reset()
    Y, O, Wt = "candy_corn_yellow", "pumpkin", "candy_corn_white"
    prof = [(0.0, 0.1), (0.2, 0.095), (0.285, 0.13), (0.318, 0.2), (0.318, 0.27), (0.305, 0.33), (0.283, 0.4),
            (0.255, 0.47), (0.222, 0.53), (0.19, 0.58), (0.15, 0.635), (0.105, 0.69), (0.062, 0.74), (0.028, 0.77),
            (0.0, 0.78)]
    band = [0, 0, 0, 0, 0, 1, 1, 1, 1, 2, 2, 2, 2, 2, 2]
    prof3 = [(r, z, b) for (r, z), b in zip(prof, band)]
    body = M.band_lathe("cbody", prof3, 18, colors=(Y, O, Wt), mats=("vinyl",), project="sphere",
                        warp=lambda c: V((c.x, c.y * 0.88, c.z + 0.02 * math.sin(c.x * 6))), center=(0, 0, 0.42))
    parts = [body]
    tree = M.bvh(body)

    def on_front(x, z):
        p, n = M.surf(tree, (x, -1.0, z), (0, 1, 0))
        return p, n
    for s in (-1, 1):
        p, n = on_front(s * 0.1, 0.43)
        parts += M.eye_cute(f"eye{s}", p, n, 0.062, 0.078, roll=-10 * s, glint="pumpkin_light")
        # cheeky brows: little dark candy slivers tilted in
        p, n = on_front(s * 0.105, 0.535)
        brow = M.blob(f"brow{s}", (0, 0, 0), (0.05, 0.014, 0.012), segs=8, rings=3, color="choco_dark")
        M.put(brow, p + n * 0.004, n, roll=16 * s)
        parts.append(brow)
        p, n = on_front(s * 0.19, 0.33)
        parts.append(M.cheek(f"cheek{s}", p, n, 0.035, 0.022, roll=-15 * s))
        # pointy candy ears sticking out sideways
        p, n = M.surf(tree, (s * 1.0, 0.0, 0.45), (-s, 0, 0))
        ear = M.cone(f"ear{s}", 0.17, 0.065, segs=7, color=O, mat="vinyl", bend=0.15)
        inner = M.cone(f"eari{s}", 0.11, 0.038, segs=5, color=Wt, mat="vinyl", bend=0.1)
        ax = V((s * 1.0, 0.05, 0.55)).normalized()
        M.put(ear, p - n * 0.02, ax, up=(0, -1, 0), scale=(1.0, 0.5, 1.0))
        M.put(inner, p - n * 0.02 + V((0, -0.016, 0)) + ax * 0.02, ax, up=(0, -1, 0), scale=(1.0, 0.3, 1.0))
        parts += [ear, inner]
        # stubby licorice arms with round mitts
        sh, hand = V((s * 0.28, -0.03, 0.27)), V((s * 0.36, -0.08, 0.15))
        parts.append(M.tube(f"arm{s}", [sh, sh.lerp(hand, 0.5) + V((s * 0.02, 0, 0)), hand], 0.03, sides=6,
                            color="charcoal", mat="vinyl", cap_start=False))
        parts.append(M.blob(f"mitt{s}", hand + V((s * 0.01, -0.01, -0.025)), (0.045, 0.045, 0.05), segs=8, rings=5,
                            color="charcoal", mat="vinyl"))
    # toothy grin with two fangs
    p, n = on_front(0.0, 0.29)
    parts.append(M.smile("grin", p, n, 0.2, 0.04, 0.034))
    for s in (-1, 1):
        q, qn = on_front(s * 0.045, 0.295)
        fang = M.cone(f"fang{s}", 0.04, 0.016, segs=5, color="eye_white")
        M.put(fang, q + qn * 0.006, (0, -0.2, -1), up=(0, -1, 0))
        parts.append(fang)
    # a little candy sprout curl on the very tip
    tip = V((0, 0, 0.775))
    parts.append(M.tube("curl", [tip - V((0, 0, 0.03)), tip + V((0.0, 0.0, 0.04)), tip + V((0.035, 0.0, 0.08)),
                                 tip + V((0.075, 0.0, 0.07))], lambda t: 0.034 * (1 - 0.45 * t), sides=7, color=Wt,
                         mat="vinyl", cap_offset=0.9))
    legs = {}
    for name, s in (("part_a", -1), ("part_b", 1)):
        hip = g2b((s * 0.12, 0.13, 0.0))
        ankle = hip + V((s * 0.01, 0.0, -0.09))
        leg = M.tube(name + "_leg", [hip + V((0, 0, 0.03)), ankle], 0.032, sides=6, color="charcoal", mat="vinyl",
                     cap_start=False)
        shoe = M.blob(name + "_shoe", ankle + V((s * 0.01, -0.09, 0.005)), (0.085, 0.13, 0.055), segs=10, rings=6,
                      color="witch_purple", mat="vinyl", warp=lambda c: V((c.x, c.y, c.z if c.z > 0 else c.z * 0.4)))
        toe = M.blob(name + "_toe", ankle + V((s * 0.01, -0.2, 0.02)), (0.035, 0.035, 0.035), segs=6, rings=4,
                     color="candy_corn_white", mat="vinyl")
        legs[name] = ([leg, shoe, toe], (s * 0.12, 0.13, 0.0))
    root = M.rig("monster_gremlin", parts, legs)
    return M.finish(root, "monster_gremlin", ao_radius=0.25)


# ====================================================================== MUMMY
def mummy():
    K.reset()
    CR, BN, SH = "cream", "bone", "bone_shade"
    cols = (CR, BN, CR, SH, CR, BN, CR, CR, SH, BN, CR, BN, CR, SH)
    wob = lambda c: V((c.x, c.y * 0.82, c.z + 0.035 * math.sin(c.z * 23.0) * (c.x / 0.25)))
    # torso: chunky barrel of tilted bandage bands
    torso = M.band_lathe("torso", M.wrap_prof(0.36, 0.98, lambda z: 0.235 - 0.05 * ((z - 0.62) / 0.36) ** 2, 6,
                                              groove=0.16, pole_top=True, pole_bottom=True),
                         13, colors=cols, mats=("paper",), warp=wob)
    parts = [torso]
    # two loose straps wrapping diagonally across the bands
    for k, (tilt, ph, col) in enumerate(((0.2, 0.0, SH), (-0.17, 1.1, BN))):
        pts = []
        for i in range(12):
            a = TAU * i / 12
            z = 0.72 + tilt * math.sin(a + ph)
            r = (0.235 - 0.05 * ((z - 0.62) / 0.36) ** 2) + 0.002
            pts.append(V((math.cos(a) * r, math.sin(a) * r * 0.82, z)))
        parts.append(D.sweep(f"strap{k}", pts, 0.045, sides=4, color=col, mat="paper", project="cyl", closed=True,
                             up=(0, 0, 1), warp=lambda a, i: 0.28 + 0.72 * abs(math.cos(a))))
    # stubby bandaged legs
    for s in (-1, 1):
        leg = M.band_lathe(f"leg{s}", M.wrap_prof(0.0, 0.42, lambda z: 0.1 - 0.012 * z, 3, groove=0.2, pole_bottom=True,
                                                  start_band=4 + (s > 0)),
                           7, colors=cols, mats=("paper",), warp=lambda c: V((c.x, c.y, c.z)))
        leg.location = (s * 0.11, 0.0, 0.0)
        parts.append(leg)
    # head: big round bandaged head with a dark gap for the eyes
    HZ = 1.17
    head_bands = 6
    hprof = M.wrap_prof(HZ - 0.19, HZ + 0.19, lambda z: 0.2 * math.sqrt(max(0.0, 1 - ((z - HZ) / 0.205) ** 2)) + 0.02,
                        head_bands, groove=0.12, pole_bottom=True, pole_top=True, start_band=7)
    hcols = list(cols)
    hcols[7 + 3] = "ink"     # the eye band
    head = M.band_lathe("head", hprof, 14, colors=tuple(hcols), mats=("paper", "paper", "paper", "flat") * 4,
                        project="sphere", warp=lambda c: V((c.x, c.y * 0.92, c.z + 0.02 * (c.x / 0.2) * (1 if c.z > HZ + 0.05 else 0))))
    parts.append(head)
    htree = M.bvh(head)
    # the eye gap: one big glowing eye peeking out, one squinting under a bandage
    gz = HZ - 0.19 + 0.38 * 3.5 / head_bands
    p, n = M.surf(htree, (-0.075, -1.0, gz), (0, 1, 0))
    parts += M.eye_glow("eyeL", p + n * 0.004, n, 0.055, 0.05, glow=HOT, pw=0.5, ph=0.62, look=(0.1, 0.0))
    p, n = M.surf(htree, (0.08, -1.0, gz - 0.006), (0, 1, 0))
    parts += M.eye_glow("eyeR", p + n * 0.004, n, 0.04, 0.026, glow=GLOW, pw=0.45, ph=0.55, roll=-8)
    # loose bandage flaps: one hanging off the head, one trailing from the waist
    p, n = M.surf(htree, (0.12, -0.2, HZ + 0.12), (-0.5, 0.6, -0.2))
    parts.append(M.tube("flap_head", M.catmull([V((0.14, 0.05, HZ + 0.1)), V((0.22, 0.1, HZ + 0.02)),
                                                V((0.26, 0.14, HZ - 0.1)), V((0.24, 0.17, HZ - 0.2))], 2),
                        0.02, sides=4, color=CR, mat="paper", flatten=0.3, cap_offset=0.0))
    parts.append(M.tube("flap_waist", M.catmull([V((-0.15, 0.15, 0.55)), V((-0.2, 0.25, 0.48)), V((-0.19, 0.33, 0.36)),
                                                 V((-0.22, 0.38, 0.25))], 2),
                        0.026, sides=4, color=BN, mat="paper", flatten=0.3, cap_offset=0.0))
    # arms stretched forward, wrapped, with a loose strip dangling from one wrist
    arms = {}
    for name, s in (("part_a", -1), ("part_b", 1)):
        sh = g2b((s * 0.29, 0.98, 0.0))
        wr = sh + V((s * 0.02, -0.4, -0.03))
        pts = [sh + V((-s * 0.06, 0.03, 0.0)), sh + V((0, -0.08, -0.01)), sh.lerp(wr, 0.55), wr]
        arm = D.sweep(name + "_arm", M.catmull(pts, 2), [0.078, 0.076, 0.074, 0.072, 0.07, 0.068, 0.066], sides=8,
                      color=CR, mat="paper", project="cyl",
                      paint=lambda i, j: ((BN if i % 2 else CR), "paper"),
                      warp=lambda a, i: 1.0 + (0.12 if i % 2 == 0 else 0.0) * 0.5)
        hand = M.blob(name + "_hand", wr + V((0, -0.06, 0.0)), (0.07, 0.075, 0.06), segs=8, rings=5, color=BN,
                      mat="paper")
        objs = [arm, hand]
        for k, a in enumerate((-30, 0, 30)):
            fd = V((math.sin(math.radians(a)) * 0.5, -1.0, -0.15)).normalized()
            fg = M.blob(f"{name}_f{k}", (0, 0, 0), (0.022, 0.022, 0.04), segs=6, rings=3, color=CR, mat="paper")
            M.put(fg, wr + V((0, -0.1, -0.005)) + fd * 0.03 + V((math.sin(math.radians(a)) * 0.03, 0, 0)), fd)
            objs.append(fg)
        if s > 0:
            objs.append(M.tube(name + "_strip", M.catmull([wr + V((0.0, 0.0, -0.06)), wr + V((0.03, 0.05, -0.15)),
                                                           wr + V((0.0, 0.1, -0.24))], 2), 0.022, sides=4, color=CR,
                               mat="paper", flatten=0.3, cap_offset=0.0))
        arms[name] = (objs, (s * 0.29, 0.98, 0.0))
    root = M.rig("monster_mummy", parts, arms)
    return M.finish(root, "monster_mummy", ao_radius=0.3)


# ====================================================================== SLIME
def slime():
    K.reset()
    G, GL, GD = "slime_green", "slime_light", "slime_dark"
    prof = [(0.0, 0.0), (0.38, 0.0), (0.465, 0.018), (0.47, 0.05), (0.43, 0.085), (0.41, 0.14), (0.405, 0.22),
            (0.39, 0.31), (0.355, 0.41), (0.3, 0.5), (0.22, 0.575), (0.12, 0.625), (0.04, 0.645), (0.0, 0.648)]

    def goo(c):
        a = math.atan2(c.y, c.x)
        low = max(0.0, 1.0 - c.z / 0.12)
        k = 1.0 + 0.09 * math.sin(7 * a + 0.4) * low + 0.02 * math.sin(3 * a) * (1 - low)
        return V((c.x * k, c.y * k * 0.95, c.z))
    body = M.lathe("sbody", prof, 28, color=G, mat="vinyl", project="sphere", warp=goo, center=(0, 0, 0.3))
    K.paint(body, GD, where=lambda c, n: c.z < 0.06)
    parts = [body]
    tree = M.bvh(body)
    # soft-serve curl on top
    parts.append(M.tube("curl", [V((0, 0, 0.6)), V((0, 0, 0.68)), V((0.04, 0.02, 0.74)), V((0.09, 0.02, 0.73)),
                                 V((0.1, 0.0, 0.69))], lambda t: 0.06 * (1 - 0.7 * t), sides=8, color=G, mat="vinyl",
                         cap_offset=0.8))
    # glossy highlight blobs and glowing bubbles
    for k, (x, y, z, r, col) in enumerate(((-0.17, -0.2, 0.5, 0.06, GL), (-0.1, -0.27, 0.42, 0.03, GL),
                                           (0.24, 0.05, 0.47, 0.045, "glow_slime"), (0.3, -0.16, 0.22, 0.035, "glow_slime"),
                                           (-0.28, 0.12, 0.3, 0.04, "glow_slime"), (0.05, 0.3, 0.42, 0.05, GL))):
        p, n = M.surf(tree, V((x, y, z)) * 3.0, -V((x, y, z - 0.25)))
        if p is None:
            continue
        b = M.blob(f"bub{k}", (0, 0, 0), (r, r, r * 0.6), segs=8, rings=4, color=col, mat="vinyl")
        M.put(b, p + n * r * 0.1, n)
        parts.append(b)
    # melty goo lumps oozing out round the base
    for k, (a, r) in enumerate(((35, 0.075), (120, 0.06), (200, 0.08), (255, 0.055), (320, 0.07))):
        ar = math.radians(a)
        d = V((math.cos(ar), math.sin(ar) * 0.95, 0))
        p, n = M.surf(tree, d * 1.0 + V((0, 0, 0.07)), -d)
        if p is None:
            continue
        lump = M.blob(f"lump{k}", p + d * r * 0.25 + V((0, 0, r * 0.25)), (r * 1.2, r * 1.2, r * 0.75), segs=10, rings=5,
                      color=G, mat="vinyl", warp=lambda c: V((c.x, c.y, c.z if c.z > 0 else c.z * 0.3)))
        parts.append(lump)
    # face
    for s in (-1, 1):
        p, n = M.surf(tree, (s * 0.13, -1.0, 0.36), (0, 1, 0))
        parts += M.eye_cute(f"eye{s}", p, n, 0.08, 0.1, roll=-8 * s, glint=GL)
        p, n = M.surf(tree, (s * 0.24, -1.0, 0.25), (0, 1, 0))
        parts.append(M.cheek(f"cheek{s}", p, n, 0.045, 0.028, roll=-14 * s))
    p, n = M.surf(tree, (0.0, -1.0, 0.22), (0, 1, 0))
    mouth = M.blob("mouth", (0, 0, 0), (0.07, 0.045, 0.02), segs=10, rings=5, color="ink",
                   warp=lambda c: V((c.x, c.y if c.y < 0 else c.y * 0.25, c.z)))
    M.put(mouth, p - n * 0.006, n)
    tongue = M.blob("tongue", (0, 0, 0), (0.04, 0.022, 0.012), segs=8, rings=3, color="cheek_pink")
    M.put(tongue, p + n * 0.006 + V((0, 0, -0.022)), n)
    parts += [mouth, tongue]
    root = M.rig("monster_slime", parts)
    return M.finish(root, "monster_slime", ao_radius=0.3)


# ====================================================================== SCARECROW
def straw_fan(prefix, base, direction, count, length, radius, spread, seed, colors=("straw", "hay", "straw_dark")):
    rr = M.rnd(seed)
    d = V(direction).normalized()
    side = d.cross(V((0, 0, 1)))
    if side.length < 0.1:
        side = d.cross(V((1, 0, 0)))
    side.normalize()
    up = side.cross(d).normalized()
    out = []
    for k in range(count):
        a = TAU * k / count + rr.uniform(-0.3, 0.3)
        dd = (d + (side * math.cos(a) + up * math.sin(a)) * spread * rr.uniform(0.6, 1.1)).normalized()
        c = M.cone(f"{prefix}{k}", length * rr.uniform(0.75, 1.15), radius, segs=3, color=colors[k % len(colors)],
                   mat="straw", bend=rr.uniform(-0.15, 0.15), lo=True)
        M.put(c, V(base) + (side * math.cos(a) + up * math.sin(a)) * radius * 0.8, dd, up=up, roll=rr.uniform(0, 90))
        out.append(c)
    return out


TAU = 2 * math.pi


def scarecrow():
    K.reset()
    parts = []
    # wooden post leg with a straw puff where it meets the ground
    parts.append(F.board("post", (0, 0, 0.0), (0, 0, 0.62), 0.085, 0.085, r=0.02, up=(0, 1, 0), color="wood_dark", mat="wood",
                         caps=(False, True)))
    parts += straw_fan("gstraw", (0, 0, 0.01), (0, 0, 1), 5, 0.13, 0.035, 1.6, 4)
    # flannel shirt torso
    shirt = M.lathe("shirt", [(0.0, 0.48), (0.2, 0.47), (0.235, 0.52), (0.245, 0.62), (0.24, 0.76), (0.225, 0.9),
                              (0.19, 0.99), (0.12, 1.04), (0.0, 1.05)], 14, color="flannel_red", mat="fabric",
                    project="cyl", warp=lambda c: V((c.x, c.y * 0.72, c.z)))
    parts.append(shirt)
    st = M.bvh(shirt)
    # straw skirt poking out under the hem, rope belt
    for k in range(9):
        a = TAU * k / 9 + 0.2
        d = V((math.cos(a), math.sin(a) * 0.72, 0))
        parts += straw_fan(f"skirt{k}_", V((0, 0, 0.5)) + d * 0.2, d * 0.6 + V((0, 0, -1)), 2, 0.15, 0.03, 0.4, 10 + k)
    parts.append(K.torus("belt", 0.245, 0.022, color="rope", mat="burlap", major_seg=14, minor_seg=4,
                         loc=(0, 0, 0.58), scale=(1.0, 0.74, 1.0)))
    # patch with stitches, buttons
    p, n = M.surf(st, (0.09, -1.0, 0.74), (0, 1, 0))
    patch = K.box("patch", (0.1, 0.1, 0.012), color="quilt_blue", mat="denim", bevel=0.004)
    M.put(patch, p, n, roll=10)
    parts.append(patch)
    items = []
    for k in range(8):
        a = TAU * k / 8 + math.pi / 8
        q, qn = M.surf(st, (0.09 + math.cos(a) * 0.058, -1.0, 0.74 + math.sin(a) * 0.058), (0, 1, 0))
        items.append((q + qn * 0.004, V((-math.sin(a), 0, math.cos(a))), qn))
    parts.append(D.stitches("patch_st", items, length=0.022, width=0.007, height=0.004, color="ink"))
    for k, z in enumerate((0.68, 0.84)):
        q, qn = M.surf(st, (-0.03, -1.0, z), (0, 1, 0))
        parts.append(M.cheek(f"btn{k}", q, qn, 0.018, 0.018, color="foil_gold"))
    # burlap sack head, cinched with rope, straw sprouting at the neck
    HC, HR = V((0, -0.01, 1.25)), V((0.2, 0.185, 0.2))
    head = M.blob("head", HC, HR, segs=15, rings=9, color="cardboard", mat="burlap",
                  warp=lambda c: V((c.x * (1 + 0.06 * c.z), c.y, c.z if c.z > -0.7 else -0.7 - (c.z + 0.7) * 0.5)))
    parts.append(head)
    parts.append(K.torus("cinch", 0.11, 0.02, color="rope", mat="burlap", major_seg=10, minor_seg=4, loc=(0, -0.01, 1.08)))
    parts += straw_fan("neck", (0, -0.01, 1.07), (0, 0, -1), 8, 0.1, 0.03, 2.2, 21)
    # glowing button eyes, triangle nose, stitched grin
    for s in (-1, 1):
        p, n = M.ellipsoid_pt(HC, HR, s * 0.078, 1.29)
        parts += M.eye_glow(f"eye{s}", p, n, 0.045, 0.045, glow=GLOW, pw=0.4, ph=0.4, look=(0.0, 0.0))
        rim = K.torus(f"eyer{s}", 0.047, 0.01, color="choco_dark", mat="flat", major_seg=10, minor_seg=3)
        M.put(rim, p, n)
        parts.append(rim)
        p, n = M.ellipsoid_pt(HC, HR, s * 0.135, 1.2)
        parts.append(M.cheek(f"cheek{s}", p, n, 0.03, 0.02, roll=-10 * s, color="candy_pink"))
    p, n = M.ellipsoid_pt(HC, HR, 0.0, 1.225)
    nose = K.polygon_prism("nose", [(-0.022, 0.014), (0.022, 0.014), (0.0, -0.02)], 0.012, color="pumpkin")
    M.put(nose, p - n * 0.004, n)
    parts.append(nose)
    gp = []
    for k in range(7):
        x = -0.085 + 0.17 * k / 6
        z = 1.17 + 0.03 * (x / 0.085) ** 2
        q, qn = M.ellipsoid_pt(HC, HR, x, z)
        gp.append(q + qn * 0.004)
    parts.append(M.tube("grin", gp, 0.007, sides=4, color="ink", mat="flat"))
    items = []
    for k in range(6):
        x = -0.07 + 0.14 * k / 5
        z = 1.17 + 0.03 * (x / 0.085) ** 2
        q, qn = M.ellipsoid_pt(HC, HR, x, z)
        items.append((q + qn * 0.004, V((0, 0, 1)), qn))
    parts.append(D.stitches("grin_st", items, length=0.04, width=0.008, height=0.004, color="ink"))
    # crooked witch hat with a pumpkin band and gold buckle
    hat_c = V((0.0, 0.0, 1.38))
    brim = M.lathe("brim", [(0.0, -0.012), (0.33, -0.01), (0.345, 0.0), (0.33, 0.012), (0.0, 0.02)], 18,
                   color="witch_purple", mat="felt", warp=lambda c: V((c.x, c.y, c.z + 0.05 * (c.x / 0.33) ** 2 - 0.03 * c.y / 0.33)))
    brim.location = hat_c
    crown = M.lathe("crown", [(0.17, 0.0), (0.15, 0.08), (0.12, 0.17), (0.085, 0.25), (0.05, 0.31), (0.02, 0.35), (0.0, 0.37)],
                    10, color="witch_purple", mat="felt",
                    warp=lambda c: V((c.x + 0.6 * (c.z / 0.37) ** 2 * 0.2, c.y + 0.25 * (c.z / 0.37) ** 3 * 0.25, c.z)))
    crown.location = hat_c
    band = M.lathe("hband", [(0.172, 0.0), (0.168, 0.05), (0.0, 0.05)], 10, color="pumpkin", mat="felt")
    band.location = hat_c + V((0, 0, 0.005))
    buckle = K.box("buckle", (0.06, 0.012, 0.045), color="foil_gold", mat="metal", bevel=0.005)
    buckle.location = hat_c + V((0.0, -0.17, 0.03))
    hat = K.merge([brim, crown, band, buckle], "hat")
    M.xform(hat, Matrix.Translation(hat_c) @ Matrix.Rotation(math.radians(-14), 4, "Y") @ Matrix.Rotation(math.radians(6), 4, "X") @ Matrix.Translation(-hat_c))
    parts.append(hat)
    # arms: outstretched flannel sleeves, straw hands
    arms = {}
    for name, s in (("part_a", -1), ("part_b", 1)):
        sh = g2b((s * 0.2, 0.98, 0.0))
        end = sh + V((s * 0.42, -0.02, -0.07))
        sleeve = M.tube(name + "_sleeve", M.catmull([sh + V((-s * 0.05, 0, 0)), sh + V((s * 0.18, 0, 0.0)), end], 2),
                        lambda t: 0.075 - 0.01 * t, sides=8, color="flannel_red", mat="fabric", cap_offset=0.0)
        cuff = K.torus(name + "_cuff", 0.07, 0.016, color="rope", mat="burlap", major_seg=8, minor_seg=3)
        M.put(cuff, end + V((-s * 0.03, 0, 0)), (s, 0, -0.15))
        objs = [sleeve, cuff]
        objs += straw_fan(name + "_hand", end, (s, -0.1, -0.25), 6, 0.15, 0.03, 0.9, 30 + s)
        if s > 0:
            p2 = K.box(name + "_patch", (0.012, 0.09, 0.08), color="house_butter", mat="fabric", bevel=0.004)
            p2.location = sh + V((s * 0.24, -0.04, 0.03))
            p2.rotation_euler = (0, math.radians(-20), math.radians(10))
            objs.append(p2)
        arms[name] = (objs, (s * 0.2, 0.98, 0.0))
    root = M.rig("monster_scarecrow", parts, arms)
    return M.finish(root, "monster_scarecrow", ao_radius=0.3)


# ====================================================================== SPIDER
def spider():
    K.reset()
    P, PU = "spider_plum", "witch_purple"
    AC, AR = V((0, 0.21, 0.43)), V((0.29, 0.31, 0.26))
    abdomen = M.blob("abdomen", AC, AR, segs=14, rings=9, color=P, mat="felt",
                     rot=Matrix.Rotation(math.radians(-18), 3, "X"))
    HC, HR = V((0, -0.16, 0.36)), V((0.215, 0.185, 0.185))
    head = M.blob("head", HC, HR, segs=14, rings=8, color=P, mat="felt")
    parts = [abdomen, head]
    at = M.bvh(abdomen)
    # glowing jack-o'-lantern face on the abdomen's back (reads from the high camera)
    def on_abd(x, y):
        return M.surf(at, (x, y, 2.0), (0, 0, -1))
    for s in (-1, 1):
        p, n = on_abd(s * 0.1, 0.12)
        eye = K.polygon_prism(f"jeye{s}", F.rounded_triangle(0, 0, 0.11, 0.09, round_=0.2), 0.012, color=GLOW)
        M.put(eye, p - n * 0.004, n, up=(0, -1, 0))
        parts.append(eye)
    p, n = on_abd(0.0, 0.28)
    grin = K.polygon_prism("jgrin", F.grin(0, 0, 0.26, 0.02, 0.09, n=11, teeth=(0.0,), tooth_w=0.04, tooth_h=0.035),
                           0.012, color=GLOW)
    M.put(grin, p - n * 0.004, n, up=(0, -1, 0))
    parts.append(grin)
    # fuzzy tuft and purple stripes ringing the abdomen
    for k, a in enumerate((-20, 0, 22)):
        t = M.cone(f"tuft{k}", 0.08, 0.03, segs=5, color=P, mat="felt", bend=0.2)
        p, n = M.ellipsoid_dir(HC, HR, (math.sin(math.radians(a)), -0.1, 1.0))
        M.put(t, p - n * 0.012, n + V((0, 0.3, 0)), up=(0, 1, 0))
        parts.append(t)
    ht = M.bvh(head)
    # eyes: two big glowing ones, four little ones above
    for s in (-1, 1):
        p, n = M.surf(ht, (s * 0.085, -1.0, 0.38), (0, 1, 0))
        parts += M.eye_glow(f"eye{s}", p, n, 0.066, 0.074, glow=HOT, pw=0.6, ph=0.68, roll=-10 * s, look=(0, -0.1))
        for k, (x, z, r) in enumerate(((0.065, 0.475, 0.026), (0.145, 0.445, 0.02))):
            p, n = M.surf(ht, (s * x, -1.0, z), (0, 1, 0))
            e = M.blob(f"eyes{s}{k}", (0, 0, 0), (r, r, r * 0.4), segs=7, rings=4, color=GLOW)
            M.put(e, p - n * 0.003, n)
            parts.append(e)
        p, n = M.surf(ht, (s * 0.045, -1.0, 0.27), (0, 1, 0))
        fang = M.cone(f"fang{s}", 0.06, 0.02, segs=5, color="bone", bend=-0.2)
        M.put(fang, p + n * 0.0, (s * 0.15, -0.3, -1), up=(0, -1, 0))
        parts.append(fang)
        p, n = M.surf(ht, (s * 0.15, -1.0, 0.31), (0, 1, 0))
        parts.append(M.cheek(f"cheek{s}", p, n, 0.025, 0.017, roll=-15 * s))
    legs = {}
    for name, s in (("part_a", -1), ("part_b", 1)):
        piv = g2b((s * 0.12, 0.35, 0.0))
        objs = []
        for k, (fy, spread) in enumerate(((-0.3, -0.55), (-0.1, -0.2), (0.1, 0.15), (0.28, 0.45))):
            root = piv + V((-s * 0.02, fy * 0.45, 0.0))
            knee = V((s * 0.33, fy + spread * 0.12, 0.56 - abs(fy) * 0.12))
            foot = V((s * 0.48, fy + spread * 0.45, 0.04))
            pts = M.catmull([root, root.lerp(knee, 0.55) + V((0, 0, 0.04)), knee, knee.lerp(foot, 0.5) + V((s * 0.03, 0, 0)), foot], 1)
            rad = [0.05, 0.048, 0.044, 0.04, 0.034]
            leg = D.sweep(f"{name}_leg{k}", pts, rad, sides=6, color=P, mat="felt", project="cyl",
                          paint=lambda i, j: ((PU, "felt") if i in (1, 3) else None))
            objs.append(leg)
            objs.append(M.blob(f"{name}_foot{k}", foot + V((0, 0, -0.005)), (0.045, 0.045, 0.04), segs=7, rings=4, color=PU, mat="felt"))
        legs[name] = (objs, (s * 0.12, 0.35, 0.0))
    root = M.rig("monster_spider", parts, legs)
    return M.finish(root, "monster_spider", ao_radius=0.3)


# ====================================================================== TOADSTOOL IMP
def toadstool():
    K.reset()
    ST, CAP = "cream", "metal_red"
    stem = M.lathe("stem", [(0.0, 0.0), (0.17, 0.0), (0.21, 0.04), (0.228, 0.14), (0.222, 0.27), (0.2, 0.4),
                            (0.18, 0.52), (0.16, 0.6), (0.0, 0.62)], 16, color=ST, mat="stucco", project="cyl",
                   warp=lambda c: V((c.x, c.y * 0.9, c.z)))
    parts = [stem]
    tree = M.bvh(stem)
    for s in (-1, 1):
        parts.append(M.blob(f"foot{s}", (s * 0.1, -0.1, 0.035), (0.075, 0.1, 0.045), segs=8, rings=5, color=ST,
                            mat="stucco"))
        p, n = M.surf(tree, (s * 0.083, -1.0, 0.335), (0, 1, 0))
        parts += M.eye_cute(f"eye{s}", p, n, 0.052, 0.068, roll=-8 * s, glint="cheek_pink")
        p, n = M.surf(tree, (s * 0.15, -1.0, 0.24), (0, 1, 0))
        parts.append(M.cheek(f"cheek{s}", p, n, 0.035, 0.022, roll=-15 * s))
    p, n = M.surf(tree, (0.0, -1.0, 0.225), (0, 1, 0))
    parts.append(M.smile("smile", p, n, 0.09, 0.02, 0.015))
    # cap: a fat glossy dome with a turned-under rim, cream gills underneath and white spots
    cap = M.lathe("cap", [(0.14, 0.54), (0.36, 0.53), (0.46, 0.55), (0.49, 0.6), (0.475, 0.66), (0.43, 0.74),
                          (0.35, 0.83), (0.24, 0.9), (0.12, 0.94), (0.0, 0.95)], 22, color=CAP, mat="vinyl",
                  project="sphere", warp=lambda c: V((c.x, c.y, c.z + 0.025 * math.sin(math.atan2(c.y, c.x) * 3) * (math.hypot(c.x, c.y) / 0.49) ** 3)))
    K.paint(cap, "cream", where=lambda c, n: c.z < 0.545 and math.hypot(c.x, c.y) < 0.43, mat="stucco")
    tilt = Matrix.Translation((0, 0, 0.55)) @ Matrix.Rotation(math.radians(-11), 4, "X") @ Matrix.Translation((0, 0, -0.55))
    M.xform(cap, tilt)
    parts.append(cap)
    ct = M.bvh(cap)
    rr = M.rnd(19)
    spots = [(0.0, 0.0, 0.075)] + [(math.radians(a + rr.uniform(-12, 12)), d, r) for a, d, r in
                                   ((10, 0.27, 0.06), (80, 0.3, 0.07), (150, 0.25, 0.05), (210, 0.31, 0.065),
                                    (280, 0.26, 0.055), (330, 0.38, 0.045), (120, 0.41, 0.04), (250, 0.42, 0.04),
                                    (45, 0.42, 0.04))]
    for k, (a, d, r) in enumerate(spots):
        x, y = math.cos(a) * d, math.sin(a) * d
        p, n = M.surf(ct, (x, y, 2.0), (0, 0, -1))
        if p is None:
            continue
        sp = M.blob(f"spot{k}", (0, 0, 0), (r, r, r * 0.3), segs=9, rings=3, color="lolly_white", mat="vinyl")
        M.put(sp, p - n * 0.004, n)
        parts.append(sp)
    # a tiny sprout on the cap
    p, n = M.surf(ct, (0.05, 0.08, 2.0), (0, 0, -1))
    parts.append(M.tube("sprout", [p - n * 0.01, p + V((0, 0, 0.06)), p + V((0.02, 0.0, 0.1))], 0.012, sides=5,
                        color="vine", mat="flat"))
    leaf = M.blob("sleaf", (0, 0, 0), (0.05, 0.025, 0.008), segs=8, rings=3, color="apple_green")
    M.put(leaf, p + V((0.05, 0.0, 0.105)), (0.3, 0, 1), up=(1, 0, 0))
    parts.append(leaf)
    arms = {}
    for name, s in (("part_a", -1), ("part_b", 1)):
        piv = g2b((s * 0.17, 0.4, 0.0))
        hand = piv + V((s * 0.14, -0.05, -0.13))
        arm = M.tube(name + "_arm", M.catmull([piv + V((-s * 0.04, 0, 0)), piv + V((s * 0.06, -0.02, -0.03)), hand], 2),
                     lambda t: 0.042 - 0.008 * t, sides=7, color=ST, mat="stucco", cap_start=False)
        mitt = M.blob(name + "_mitt", hand + V((s * 0.01, -0.01, -0.02)), (0.045, 0.045, 0.05), segs=8, rings=5,
                      color=ST, mat="stucco")
        arms[name] = ([arm, mitt], (s * 0.17, 0.4, 0.0))
    root = M.rig("monster_toadstool", parts, arms)
    return M.finish(root, "monster_toadstool", ao_radius=0.3)


if __name__ == "__main__":
    M.run({"pumpkin": pumpkin, "bat": bat, "skeleton": skeleton, "ghost": ghost, "gremlin": gremlin,
           "mummy": mummy, "slime": slime, "scarecrow": scarecrow, "spider": spider, "toadstool": toadstool})
