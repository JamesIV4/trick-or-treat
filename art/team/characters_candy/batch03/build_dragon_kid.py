"""dragon_kid v3 (concept match): the player character. 1.1 m child in a plush GREEN felt dragon
onesie (pale yellow-cream belly, golden horns, yellow-green spikes, green bat wings with a light
membrane, chunky spiked tail, cartoon eyes on the hood) holding a glowing jack-o'-lantern bucket.
Every part is its own object with its origin at the joint, parented to an empty root 'dragon_kid'
at the feet. Character right = Blender -X, front = -Y. Rig contract identical to batch02.

Parts (identity rotation and scale, origin at the joint; joint positions unchanged from v1):
  body (pelvis)  head (neck)  wing_l / wing_r (wing roots)  tail (tail base)  leg_l / leg_r (hips)
  arm_l / arm_r (shoulders)  bucket (child of arm_r, origin at the handle grip)
  bucket_candy (child of bucket, origin on the bucket floor; the game scales it 0..1 in Y)
  socket_bucket_light (child of bucket, at the bucket centre)

Run: blender --background --factory-startup --python art/team/characters_candy/batch03/build_dragon_kid.py
     [-- --nosheet] [--pieces]
"""
import math
import sys
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parent))
import bmesh  # noqa: E402
import bpy  # noqa: E402
from mathutils import Matrix, Vector  # noqa: E402

import dk_kit as D  # noqa: E402
from dk_kit import K, V  # noqa: E402

K.reset()
T, TD, TL = "dragon_jade", "dragon_green_dark", "dragon_green_light"
SPK, GOLD, CREAM, CREAMD = "dragon_spike", "dragon_gold", "dragon_cream", "dragon_cream_dark"
FELT = "felt"
WING_SCALE = 1.4

# ------------------------------------------------------------------ joints (same as v1)
HIP_Z = 0.30
HIP_L, HIP_R = (0.085, 0.0, HIP_Z), (-0.085, 0.0, HIP_Z)
SHO_L, SHO_R = (0.165, 0.0, 0.585), (-0.165, 0.0, 0.585)
NECK = (0.0, 0.0, 0.64)
WING_L, WING_R = (0.075, 0.135, 0.56), (-0.075, 0.135, 0.56)
TAIL_BASE = (0.0, 0.14, 0.34)
HAND_L = V((0.245, -0.03, 0.405))
HAND_R = V((-0.26, -0.085, 0.415))

root = K.empty("dragon_kid", (0, 0, 0))


def catmull(pts, per=4):
    """Catmull-Rom resample through pts (endpoints kept). Returns (points, params 0..1)."""
    P = [V(p) for p in pts]
    P = [P[0] * 2 - P[1]] + P + [P[-1] * 2 - P[-2]]
    out, par = [], []
    nseg = len(pts) - 1
    for s in range(nseg):
        p0, p1, p2, p3 = P[s], P[s + 1], P[s + 2], P[s + 3]
        for k in range(per):
            t = k / per
            t2, t3 = t * t, t * t * t
            out.append(0.5 * ((2 * p1) + (-p0 + p2) * t + (2 * p0 - 5 * p1 + 4 * p2 - p3) * t2 + (-p0 + 3 * p1 - 3 * p2 + p3) * t3))
            par.append((s + t) / nseg)
    out.append(V(pts[-1]))
    par.append(1.0)
    return out, par


def lerp_list(vals, t):
    x = t * (len(vals) - 1)
    i = min(int(x), len(vals) - 2)
    f = x - i
    return vals[i] * (1 - f) + vals[i + 1] * f


def soft_spike(name, h, r, segs=7, color=SPK):
    """Rounded felt spike (pointing up local Z, base at z=0)."""
    prof = [(r, -0.008), (r * 0.9, h * 0.3), (r * 0.62, h * 0.62), (r * 0.28, h * 0.88), (0.0, h)]
    return D.lathe(name, prof, segs, color=color, mat=FELT, project="cyl")


def ellipsoid_pt(c, rad, x, z, front=True):
    q = 1 - ((x - c.x) / rad.x) ** 2 - ((z - c.z) / rad.z) ** 2
    y = c.y + (-1 if front else 1) * rad.y * math.sqrt(max(q, 0.0))
    p = V((x, y, z))
    n = V(((p.x - c.x) / rad.x ** 2, (p.y - c.y) / rad.y ** 2, (p.z - c.z) / rad.z ** 2)).normalized()
    return p, n


def ellipsoid_dir(c, rad, d):
    d = V(d).normalized()
    s = 1.0 / math.sqrt((d.x / rad.x) ** 2 + (d.y / rad.y) ** 2 + (d.z / rad.z) ** 2)
    p = c + d * s
    n = V(((p.x - c.x) / rad.x ** 2, (p.y - c.y) / rad.y ** 2, (p.z - c.z) / rad.z ** 2)).normalized()
    return p, n


def ell_prof(rings):
    out = []
    for i in range(rings + 1):
        phi = math.pi * i / rings
        out.append((0.0 if i in (0, rings) else math.sin(phi), -math.cos(phi)))
    return out


def blob(name, center, rad, segs=16, rings=10, color=T, mat=FELT, project="sphere", cut=None):
    """Smooth ellipsoid at center with radii rad; optional face cut(world face centre) -> delete."""
    rad = V(rad)
    ob = D.lathe(name, ell_prof(rings), segs, color=color, mat=mat, project=project,
                 warp=lambda c: V((c.x * rad.x, c.y * rad.y, c.z * rad.z)), center=(0, 0, 0))
    ob.location = V(center)
    if cut:
        bm = bmesh.new()
        bm.from_mesh(ob.data)
        bmesh.ops.delete(bm, geom=[f for f in bm.faces if cut(f.calc_center_median() + V(center))], context="FACES")
        bm.to_mesh(ob.data)
        bm.free()
    return ob


# ================================================================== BODY (origin at pelvis)
torso_prof = [(0.0, 0.215), (0.08, 0.222), (0.135, 0.245), (0.172, 0.28), (0.195, 0.33), (0.203, 0.385),
              (0.198, 0.44), (0.183, 0.5), (0.162, 0.56), (0.138, 0.61), (0.11, 0.655), (0.07, 0.685), (0.0, 0.695)]


def torso_warp(c):
    y = c.y * 0.88
    if y < 0:   # a soft round tummy pushing forward at belly height
        y *= 1.0 + 0.1 * math.exp(-((c.z - 0.42) / 0.12) ** 2)
    return V((c.x, y + 0.012, c.z))


torso = D.lathe("torso", torso_prof, 20, color=T, mat=FELT, project="sphere", warp=torso_warp, center=(0, 0, 0.45))
torso_bvh = D.bvh(torso)

# cream felt belly patch: a pillowy oval conforming to the tummy, sewn on with dark stitches
PC, PRU, PRV = V((0, 0, 0.415)), 0.118, 0.142
fr = [0.0, 0.45, 0.75, 0.92, 1.0]
hh = [0.010, 0.0095, 0.0075, 0.0038, -0.002]
SEG = 22
bm = bmesh.new()
rings = []
for f, h in zip(fr, hh):
    ring = []
    for j in range(1 if f == 0 else SEG):
        a = 2 * math.pi * j / SEG
        q = PC + V((math.cos(a) * PRU * f, -1.0, math.sin(a) * PRV * f))
        loc, nrm = D.hit(torso_bvh, q, (0, 1, 0))
        ring.append(bm.verts.new(loc + nrm * h))
    rings.append(ring)
for i in range(len(rings) - 1):
    A, B = rings[i], rings[i + 1]
    for j in range(SEG):
        j2 = (j + 1) % SEG
        if len(A) == 1:
            bm.faces.new((A[0], B[j], B[j2]))
        else:
            bm.faces.new((A[j], B[j], B[j2], A[j2]))
for fc in bm.faces:
    fc.normal_update()
    if fc.normal.y > 0:
        fc.normal_flip()
belly = D.mesh(bm, "belly", CREAM, "flat", "box", center=PC)   # smooth, like the concept (felt read as burlap)
items = D.ring_on(torso_bvh, PC, (1, 0, 0), (0, 0, 1), PRU * 0.84, PRV * 0.84, 16, (0, 1, 0), back=1.0, offset=0.0088)
belly_st = D.stitches("belly_st", items, length=0.019, width=0.006, height=0.003, color=TD)
body_parts = [torso, belly, belly_st]

# soft horizontal belly-plate ridges (darker cream), conforming to the pillowed patch
for i, (dz, w) in enumerate([(0.06, 0.78), (0.0, 0.86), (-0.06, 0.78)]):
    pts = []
    for k in range(7):
        u = -1 + 2 * k / 6
        q = PC + V((u * PRU * w * 0.82, -1.0, dz + 0.012 * u * u))
        loc, nrm = D.hit(torso_bvh, q, (0, 1, 0))
        pts.append(loc + nrm * 0.0095)
    rr = [0.0045 * (1 - 0.55 * abs(-1 + 2 * k / 6) ** 2) for k in range(7)]
    body_parts.append(D.sweep(f"ridge{i}", pts, rr, sides=5, color=CREAMD, mat=FELT, project="box"))

# a row of soft yellow-green spikes down the back, continuing the hood crest to the tail
for i, (z, s) in enumerate([(0.545, 0.95), (0.475, 1.0), (0.405, 0.92), (0.345, 0.8)]):
    loc, nrm = D.hit(torso_bvh, (0, 1.0, z), (0, -1, 0))
    sp = soft_spike(f"bspike{i}", 0.08 * s, 0.04 * s)
    D.put(sp, loc - nrm * 0.004, nrm + V((0, 0, -0.3)), up=(0, 0, 1), scale=(0.6, 1.0, 1.0))
    body_parts.append(sp)
body = D.part(body_parts, "body", (0, 0, HIP_Z))


# ================================================================== LEGS (origin at hip)
def boot_warp(c):
    y = c.y * 1.32 - 0.028
    if c.y < 0:
        y -= 0.012 * (1 - abs(c.x) / 0.1)
    return V((c.x, y, c.z))


def leg(name, hip):
    x = hip[0] * 1.06
    thigh = D.lathe(name + "_thigh", [(0.0, 0.12), (0.074, 0.125), (0.08, 0.17), (0.084, 0.24), (0.074, 0.305), (0.0, 0.335)],
                    10, color=T, mat=FELT, project="cyl", loc=(x, 0, 0))
    boot = D.lathe(name + "_boot", [(0.0, 0.03), (0.07, 0.03), (0.09, 0.05), (0.096, 0.083), (0.088, 0.115),
                                    (0.066, 0.142), (0.032, 0.158), (0.0, 0.162)],
                   14, color=T, mat=FELT, project="sphere", warp=boot_warp, loc=(x, 0, 0), center=(0, 0, 0.09))
    sole = D.lathe(name + "_sole", [(0.0, 0.0), (0.086, 0.0), (0.099, 0.008), (0.102, 0.021), (0.096, 0.033), (0.0, 0.036)],
                   12, color=TD, mat="flat", project="cyl",
                   warp=lambda c: V((c.x * 1.02, c.y * 1.36 - 0.03, c.z)), loc=(x, 0, 0))
    claws = []
    for i, dx in enumerate((-0.04, 0.0, 0.04)):
        claws.append(K.sphere(f"{name}_claw{i}", 0.025, color=GOLD, mat="flat", segments=6, rings=4,
                              loc=(x + dx, -0.15 + abs(dx) * 0.45, 0.047), scale=(1.0, 1.2, 0.85)))
    return D.part([thigh, boot, sole] + claws, name, hip)


leg_l = leg("leg_l", HIP_L)
leg_r = leg("leg_r", HIP_R)


# ================================================================== ARMS (origin at shoulder)
def arm(name, sho, hand):
    s = V(sho)
    out = 1 if s.x > 0 else -1
    d = (hand - s).normalized()
    elbow = s.lerp(hand, 0.5) + V((0.018 * out, 0.01, 0.0))
    pts, par = catmull([s + V((-0.045 * out, 0, 0.012)), elbow, hand], per=3)
    # sleeve flares slightly into a soft cuff at the wrist; the kid's own hand pokes out
    rr = [0.066 - 0.016 * t + 0.012 * max(0.0, (t - 0.8) / 0.2) for t in par]
    sleeve = D.sweep(name + "_sleeve", pts, rr, sides=10, color=T, mat=FELT, project="cyl")
    mc = hand + d * 0.04
    mitten = blob(name + "_hand", (0, 0, 0), (0.042, 0.04, 0.05), segs=10, rings=6, color="skin", mat="flat")
    D.put(mitten, mc, d, up=(0, -1, 0))
    th = blob(name + "_thumb", (0, 0, 0), (0.016, 0.016, 0.026), segs=7, rings=4, color="skin", mat="flat")
    D.put(th, mc + V((0.01 * out, -0.036, 0.018)), d + V((0, -0.6, 0)), up=(0, -1, 0))
    return D.part([sleeve, mitten, th], name, sho)


arm_l = arm("arm_l", SHO_L, HAND_L)
arm_r = arm("arm_r", SHO_R, HAND_R)

# ================================================================== HEAD (origin at neck)
HC, HR = V((0, 0.012, 0.83)), V((0.268, 0.248, 0.238))
FC, FR = V((0, -0.118, 0.80)), V((0.198, 0.128, 0.178))
OA, OC, OZ = 0.18, 0.165, 0.80       # face opening ellipse (x radius, z radius, centre z)


def opening(c):
    return c.y < HC.y - 0.1 and (c.x / (OA - 0.012)) ** 2 + ((c.z - OZ) / (OC - 0.012)) ** 2 < 1.0


hood = blob("hood", HC, HR, segs=28, rings=15, color=T, cut=opening)
face = blob("face", FC, FR, segs=18, rings=11, color="skin", mat="flat")

# puffy light-teal rim framing the face, with dark stitches along its crest
rim_pts = []
NR = 24
for i in range(NR):
    a = 2 * math.pi * i / NR
    p, n = ellipsoid_pt(HC, HR, math.cos(a) * OA, OZ + math.sin(a) * OC)
    rim_pts.append(p - n * 0.004)
rim = D.sweep("hood_rim", rim_pts, 0.032, sides=7, color=T, mat=FELT, project="sphere", closed=True, center=HC)
rim_st = []
for i in range(20):
    a = 2 * math.pi * (i + 0.5) / 20
    p, n = ellipsoid_pt(HC, HR, math.cos(a) * OA, OZ + math.sin(a) * OC)
    t = V((-math.sin(a) * OA, 0, math.cos(a) * OC))
    rim_st.append((p - n * 0.004 + n * 0.031, t, n))
rim_st = D.stitches("rim_st", rim_st, length=0.018, width=0.006, height=0.003, color=TD, sink=0.004)
head_parts = [hood, face, rim, rim_st]

# eyes: big glossy ink domes, a teal glint below, two white highlights (up-left as seen from the front)
for sx in (1, -1):
    ex, ez = 0.076 * sx, 0.806
    p, n = ellipsoid_pt(FC, FR, ex, ez)
    roll = -7 * sx
    W, H, Dp = 0.035, 0.047, 0.016
    eye = blob(f"eye{sx}", (0, 0, 0), (W, H, Dp), segs=14, rings=8, color="ink", mat="flat")
    D.put(eye, p - n * 0.003, n, roll=roll)
    M = D.look(n, roll=roll)

    def on_eye(lx, ly, lift=0.0015):
        lz = Dp * math.sqrt(max(1 - (lx / W) ** 2 - (ly / H) ** 2, 0.0))
        return p - n * 0.003 + M @ V((lx, ly, lz)) + n * lift, (M @ V((lx / W ** 2, ly / H ** 2, lz / Dp ** 2))).normalized()

    q, qn = on_eye(0.0, -0.027, 0.0)
    glint = blob(f"glint{sx}", (0, 0, 0), (0.02, 0.011, 0.004), segs=8, rings=4, color="burnt_amber", mat="flat")
    D.put(glint, q, qn, roll=roll)
    q, qn = on_eye(-0.011, 0.016)
    hl = blob(f"hl{sx}", (0, 0, 0), (0.0145, 0.0165, 0.004), segs=10, rings=4, color="eye_white", mat="flat")
    D.put(hl, q, qn, roll=roll)
    q, qn = on_eye(0.013, -0.008)
    hl2 = blob(f"hl2{sx}", (0, 0, 0), (0.0068, 0.0068, 0.003), segs=8, rings=4, color="eye_white", mat="flat")
    D.put(hl2, q, qn, roll=roll)
    head_parts += [eye, glint, hl, hl2]
    # rosy cheeks
    p, n = ellipsoid_pt(FC, FR, 0.132 * sx, 0.752)
    ck = blob(f"cheek{sx}", (0, 0, 0), (0.031, 0.019, 0.006), segs=8, rings=4, color="cheek_pink", mat="flat")
    D.put(ck, p - n * 0.002, n, roll=-10 * sx)
    head_parts.append(ck)

# happy open smile (crescent, corners up) with a pink tongue and one tiny fang
p, n = ellipsoid_pt(FC, FR, 0.0, 0.744)
MW = 0.028


def yb(x):
    u = x / MW
    return 0.007 * u * u - 0.018 * (1 - u * u)


def yt(x):
    u = x / MW
    return 0.007 * u * u - 0.003 * (1 - u * u)


smile = [(-MW + 2 * MW * i / 10, yb(-MW + 2 * MW * i / 10)) for i in range(11)]
smile += [(MW - 2 * MW * i / 8, yt(MW - 2 * MW * i / 8)) for i in range(1, 8)]
mouth = K.polygon_prism("mouth", smile, 0.008, color="ink", mat="flat")
D.put(mouth, p - n * 0.004, n)
ML = D.look(n)
tongue = blob("tongue", (0, 0, 0), (0.012, 0.0062, 0.004), segs=8, rings=4, color="cheek_pink", mat="flat")
D.put(tongue, p + n * 0.0025 + ML @ V((0.002, -0.0105, 0)), n)
fang = K.polygon_prism("fang", [(-0.0045, 0.0), (0.0045, 0.0), (0.0, -0.008)], 0.004, color="eye_white", mat="flat")
D.put(fang, p + n * 0.003 + ML @ V((0.011, yt(0.011) + 0.0005, 0)), n)
head_parts += [mouth, tongue, fang]

# brown hair swooping out from under the rim: a fringe across the forehead plus a lock at each temple
for i, (x, z, s, r) in enumerate([(-0.088, 0.9, 0.95, 34), (-0.028, 0.918, 1.2, 14), (0.038, 0.915, 1.1, -12),
                                  (0.1, 0.895, 0.9, -36)]):
    p, n = ellipsoid_pt(FC, FR, x, z)
    tuft = blob(f"fringe{i}", (0, 0, 0), (0.054 * s, 0.032 * s, 0.024), segs=8, rings=5, color="hair_brown", mat="flat")
    D.put(tuft, p + n * 0.006, n + V((0, 0, 0.35)), roll=r)
    head_parts.append(tuft)
for sx in (1, -1):
    p, n = ellipsoid_pt(FC, FR, 0.158 * sx, 0.872)
    lock = blob(f"lock{sx}", (0, 0, 0), (0.024, 0.044, 0.018), segs=7, rings=5, color="hair_brown", mat="flat")
    D.put(lock, p + n * 0.004, n, roll=18 * sx)
    head_parts.append(lock)

# big golden horns sweeping up and back (no collar: they grow straight out of the hood)
for sx in (1, -1):
    p, n = ellipsoid_dir(HC, HR, (0.47 * sx, 0.12, 0.87))
    h, r = 0.15, 0.06
    prof = [(r, -0.025), (r, 0.0), (r * 0.95, h * 0.25), (r * 0.82, h * 0.5), (r * 0.63, h * 0.72),
            (r * 0.42, h * 0.87), (r * 0.18, h * 0.975), (0.0, h)]
    horn = D.lathe(f"horn{sx}", prof, 10, color=GOLD, mat="flat", project="cyl",
                   warp=lambda c: V((c.x, c.y + 2.6 * c.z * c.z, c.z)))
    hd = n + V((0.16 * sx, 0.1, 0.3))
    D.put(horn, p - n * 0.014, hd, up=(0, 1, 0))
    head_parts.append(horn)

# cartoon dragon eyes on the hood's brow, looking forward (the hood reads as a dragon head)
for sx in (1, -1):
    p, n = ellipsoid_dir(HC, HR, (0.34 * sx, -0.52, 0.8))
    W, H, Dp = 0.036, 0.04, 0.02
    ew = blob(f"heye{sx}", (0, 0, 0), (W, H, Dp), segs=10, rings=6, color="eye_white", mat="flat")
    D.put(ew, p - n * 0.006, n, roll=-12 * sx)
    M = D.look(n, roll=-12 * sx)
    lx, ly = -0.008 * sx, -0.004
    lz = Dp * math.sqrt(max(1 - (lx / W) ** 2 - (ly / H) ** 2, 0.0))
    pc = p - n * 0.006 + M @ V((lx, ly, lz))
    pupil = blob(f"hpupil{sx}", (0, 0, 0), (0.017, 0.021, 0.006), segs=8, rings=4, color="ink", mat="flat")
    D.put(pupil, pc, n, roll=-12 * sx)
    head_parts += [ew, pupil]

# soft crest spikes over the top and down the back of the hood
for i, (ang, s) in enumerate([(14, 0.9), (48, 1.05), (84, 1.05), (120, 0.9)]):
    a = math.radians(ang)
    p, n = ellipsoid_dir(HC, HR, (0, math.sin(a), math.cos(a)))
    sp = soft_spike(f"crest{i}", 0.08 * s, 0.04 * s)
    D.put(sp, p - n * 0.006, n + V((0, 0.3, 0)), up=(0, 1, 0), scale=(0.58, 1.0, 1.0))
    head_parts.append(sp)
head = D.part(head_parts, "head", NECK)
# a curious little head tilt, baked into the mesh so the rest pose stays identity
head.data.transform(Matrix.Rotation(math.radians(-5), 4, "Y") @ Matrix.Rotation(math.radians(-3), 4, "X"))


# ================================================================== WINGS (origin at wing root)
def wing_outline():
    tips = [V((0.3, 0.255)), V((0.255, 0.075)), V((0.14, -0.005))]
    lead = [V((0.0, 0.075)), V((0.09, 0.17)), V((0.2, 0.24))]
    pts = [V((0.0, -0.03))]
    chain = [V((0.0, -0.03)), tips[2], tips[1], tips[0]]
    for a, b in zip(chain[:-1], chain[1:]):   # trailing edge: scallops between finger tips
        mid = (a + b) / 2
        ctrl = mid + (V((0.02, 0.09)) - mid) * 0.34
        for t in (0.36, 0.64):
            pts.append(a * (1 - t) ** 2 + ctrl * 2 * t * (1 - t) + b * t * t)
        pts.append(b)
    for q in reversed(lead):
        pts.append(q)
    return pts, tips


def wing(name, root_pt, sx):
    pts, tips = wing_outline()
    th = 0.028
    bm = bmesh.new()
    front = [bm.verts.new((x, -th / 2, z)) for x, z in pts]
    back = [bm.verts.new((x, th / 2, z)) for x, z in pts]
    bm.faces.new(front)
    bm.faces.new(list(reversed(back)))
    k = len(pts)
    for i in range(k):
        bm.faces.new((front[i], back[i], back[(i + 1) % k], front[(i + 1) % k]))
    bmesh.ops.recalc_face_normals(bm, faces=bm.faces)
    rim_edges = [e for e in bm.edges if abs(e.verts[0].co.y - e.verts[1].co.y) < 1e-6]
    bmesh.ops.bevel(bm, geom=rim_edges, offset=0.012, segments=2, profile=0.5, affect="EDGES", clamp_overlap=True)
    mem = D.mesh(bm, name + "_m", TL, "flat", "box", smooth=True, center=(0.15, 0, 0.1))
    ribs = []
    for i, tp in enumerate(tips):
        a = V((0.01, 0.0, 0.06))
        b = V((tp.x * 0.97, 0.0, tp.y * 0.97))
        mid = a.lerp(b, 0.5) + V((0, 0, 0.025 if i < 2 else 0.0))
        rp, par = catmull([a, mid, b], per=2)
        ribs.append(D.sweep(f"{name}_rib{i}", rp, [0.025 - 0.011 * t for t in par], sides=6, color=T, mat=FELT,
                            project="box"))
    ob = K.merge([mem] + ribs, name + "_tmp")
    ob.data.transform(Matrix.Diagonal((WING_SCALE, 1.0, WING_SCALE, 1.0)))
    if sx < 0:
        ob.data.transform(Matrix.Scale(-1, 4, (1, 0, 0)))
        bm = bmesh.new()
        bm.from_mesh(ob.data)
        bmesh.ops.reverse_faces(bm, faces=bm.faces, flip_multires=False)
        bm.to_mesh(ob.data)
        bm.free()
    ob.data.transform(Matrix.Rotation(math.radians(6 * sx), 4, "Y"))   # spread out past the hood
    ob.data.transform(Matrix.Rotation(math.radians(18), 4, "X"))   # lean the tops forward: the back face catches the light
    ob.data.transform(Matrix.Rotation(math.radians(26 * sx), 4, "Z"))
    ob.location = V(root_pt)
    return D.part([ob], name, root_pt)


wing_l = wing("wing_l", WING_L, 1)
wing_r = wing("wing_r", WING_R, -1)

# ================================================================== TAIL (origin at tail base)
ctrl = [(0, 0.08, 0.38), (0.0, 0.23, 0.29), (0.01, 0.37, 0.19), (0.04, 0.5, 0.118), (0.09, 0.615, 0.098),
        (0.15, 0.705, 0.118), (0.2, 0.765, 0.17), (0.228, 0.795, 0.24), (0.236, 0.8, 0.29)]
crad = [0.132, 0.126, 0.112, 0.096, 0.08, 0.065, 0.052, 0.041, 0.032]
tp, par = catmull(ctrl, per=2)
trad = [lerp_list(crad, t) for t in par]
TS = 12
tail_tube = D.sweep("tail_tube", tp, trad, sides=TS, color=T, mat=FELT, project="sphere", up=(0, 0, 1),
                    paint=lambda i, j: (CREAM, FELT) if (2 <= i <= len(tp) - 5 and TS // 2 - 2 <= j <= TS // 2 + 1) else None)
tail_parts = [tail_tube]
for i, t in enumerate((0.2, 0.32, 0.45, 0.57, 0.69, 0.8)):
    j = round(t * (len(tp) - 1))
    p = tp[j]
    tan = (tp[j + 1] - tp[j - 1]).normalized()
    upv = V((0, 0, 1))
    n = (upv - tan * upv.dot(tan)).normalized()
    r = trad[j]
    s = 1.05 - i * 0.1
    sp = soft_spike(f"tspike{i}", 0.08 * s, 0.04 * s)
    D.put(sp, p + n * (r - 0.008), n - tan * 0.3, up=tan, scale=(0.58, 1.0, 1.0))
    tail_parts.append(sp)
# golden spike capping the upturned tip
tan = (tp[-1] - tp[-2]).normalized()
tip = soft_spike("ttip", 0.1, 0.038, segs=8, color=GOLD)
D.put(tip, tp[-1] - tan * 0.012, tan, up=(0, 1, 0), scale=(0.75, 1.0, 1.0))
tail_parts.append(tip)
tail = D.part(tail_parts, "tail", TAIL_BASE)

# ================================================================== BUCKET (child of arm_r, origin at grip)
GRIP = HAND_R + V((0.0, 0.0, -0.03))
BR, CUT, TH = 0.15, 0.064, 0.013
rim_r = math.sqrt(BR ** 2 - CUT ** 2)
BC = GRIP - V((0.0, 0.0, CUT + rim_r + 0.006))
BOT = -0.128
NLOBE = 5
FLOOR_Z = CUT - 0.016      # the candy floor sits just under the rim; below it is the glowing lantern chamber


def lobe_k(ang):
    """Six soft lobes, one bulging straight out the front (-Y) under the carved face."""
    return 1 + 0.055 * math.cos(NLOBE * (ang + math.pi / 2))


prof = [(0.0, BOT)]
for deg in (34, 60, 82, 100, 113.5):
    phi = math.radians(deg)
    prof.append((BR * math.sin(phi), max(-BR * math.cos(phi), BOT)))
prof += [(rim_r - 0.002, CUT + 0.006), (rim_r - TH * 0.6, CUT + 0.009), (rim_r - TH, CUT + 0.002)]
BRi = BR - TH
for deg in (108, 85, 55):
    phi = math.radians(deg)
    prof.append((BRi * math.sin(phi), max(-BRi * math.cos(phi), BOT + TH)))
prof.append((0.0, BOT + TH))


def bucket_warp(c):
    k = lobe_k(math.atan2(c.y, c.x))
    return V((c.x * k, c.y * k, c.z))


shell = D.lathe("bucket_shell", prof, 20, color="pumpkin", mat="pumpkin", project="cyl", warp=bucket_warp,
                center=(0, 0, 0))


def rounded_poly(corners, rad=0.006, n=3):
    """Polygon with filleted corners (CCW list of (x, z))."""
    out = []
    k = len(corners)
    for i in range(k):
        p0, p1, p2 = V(corners[i - 1]), V(corners[i]), V(corners[(i + 1) % k])
        a = (p0 - p1).normalized()
        b = (p2 - p1).normalized()
        q0, q2 = p1 + a * rad, p1 + b * rad
        for j in range(n + 1):
            t = j / n
            out.append(tuple(q0 * (1 - t) ** 2 + p1 * 2 * t * (1 - t) + q2 * t * t))
    return out


def grin_outline():
    """Crescent grin (CCW as seen from the front) with two square teeth hanging from the top edge."""
    W = 0.084

    def zb(x):
        u = x / W
        return -0.044 - 0.042 * (1 - u * u) + 0.022 * u * u

    def zt(x):
        u = x / W
        return -0.044 - 0.008 * (1 - u * u) + 0.022 * u * u

    def tooth(xa, xb):
        return [(xa, zt(xa)), (xa, zt(xa) - 0.019), (xb, zt(xb) - 0.019), (xb, zt(xb))]

    out = [(-W + 2 * W * i / 10, zb(-W + 2 * W * i / 10)) for i in range(11)]
    out += [(0.065, zt(0.065)), (0.048, zt(0.048))]
    out += tooth(0.035, 0.016)
    out += [(0.0, zt(0.0))]
    out += tooth(-0.016, -0.035)
    out += [(-0.048, zt(-0.048)), (-0.065, zt(-0.065))]
    return out


cutters = []


def cutter(name, outline, cx, cz):
    ob = K.polygon_prism(name, [(x + cx, z + cz) for x, z in outline], 0.26, color="pumpkin")
    ob.rotation_euler = (math.radians(90), 0, 0)
    ob.location = (0, -0.04, 0)
    cutters.append(ob)


eye = rounded_poly([(-0.034, -0.017), (0.034, -0.017), (0.004, 0.032)], 0.008)
cutter("cut_eye_l", eye, 0.053, 0.016)
cutter("cut_eye_r", [(-x, z) for x, z in reversed(eye)], -0.053, 0.016)
cutter("cut_nose", rounded_poly([(-0.015, -0.009), (0.015, -0.009), (0.0, 0.014)], 0.004), 0.0, -0.018)
cutter("cut_grin", grin_outline(), 0.0, 0.0)
cut = K.merge(cutters, "cutter")
mod = shell.modifiers.new("carve", "BOOLEAN")
mod.operation = "DIFFERENCE"
mod.solver = "EXACT"
mod.object = cut
dg = bpy.context.evaluated_depsgraph_get()
carved = bpy.data.meshes.new_from_object(shell.evaluated_get(dg))
shell.modifiers.clear()
old = shell.data
shell.data = carved
bpy.data.meshes.remove(old)
bpy.data.objects.remove(cut, do_unlink=True)

# classify faces: outer skin, carved walls (lit flesh), glowing chamber, dark rim inside, lip
bm = bmesh.new()
bm.from_mesh(shell.data)
bmesh.ops.dissolve_degenerate(bm, dist=2e-4, edges=bm.edges)   # boolean slivers
bmesh.ops.triangulate(bm, faces=[f for f in bm.faces if len(f.verts) > 4])
bm.normal_update()
outer, walls, inner, lip = [], [], [], []
for f in bm.faces:
    c = f.calc_center_median()
    nr = f.normal.dot(c.normalized()) if c.length > 1e-4 else -1.0
    if c.z > CUT - 0.002:
        lip.append(f)
    elif abs(nr) < 0.45 and c.y < -0.08 and -0.11 < c.z < 0.06:
        walls.append(f)
    elif (nr > 0 and c.length > BRi * 0.91) or (c.z < BOT + 0.004 and f.normal.z < -0.5):
        outer.append(f)
    else:
        inner.append(f)
K._project_bm(bm, "cyl", outer + lip, center=V((0, 0, 0)))
K._paint_bm(bm, "pumpkin", "pumpkin", outer)
K._paint_bm(bm, "pumpkin_light", "pumpkin", lip)
K._paint_bm(bm, "glow_jack", "flat", walls)
K._paint_bm(bm, "glow_jack_hot", "flat", [f for f in inner if f.calc_center_median().z < FLOOR_Z])
K._paint_bm(bm, "pumpkin_dark", "pumpkin", [f for f in inner if f.calc_center_median().z >= FLOOR_Z])
for f in bm.faces:
    f.smooth = f not in walls
bm.to_mesh(shell.data)
bm.free()
shell.location = BC
print("bucket faces outer/walls/inner/lip", len(outer), len(walls), len(inner), len(lip))

# candy floor: one upward face (dark pumpkin); from below it is culled so the chamber glow shows through the cuts
bm = bmesh.new()
fr_r = math.sqrt(BRi ** 2 - FLOOR_Z ** 2) * 0.985
bm.faces.new([bm.verts.new((math.cos(a) * fr_r * lobe_k(a), math.sin(a) * fr_r * lobe_k(a), FLOOR_Z))
              for a in (2 * math.pi * i / 20 for i in range(20))])
floor = D.mesh(bm, "bucket_floor", "pumpkin_dark", "pumpkin", "box", smooth=False, loc=BC)

# twisted rope handle and knots
hp = []
HRAD = rim_r - 0.006
for i in range(19):
    a = math.pi * i / 18
    hp.append(BC + V((math.cos(a) * HRAD, 0.0, CUT + 0.004 + math.sin(a) * (HRAD + 0.006))))
handle = D.sweep("bucket_handle", hp, 0.0108, sides=6, color="rope", mat="flat", project="box",
                 warp=lambda a, i: 1.0 + 0.22 * math.cos(2 * a - i * 1.5))
knots = []
for s in (1, -1):
    kn = blob(f"bucket_knot{s}", (0, 0, 0), (0.021, 0.021, 0.017), segs=6, rings=4, color="rope", mat="flat")
    kn.location = BC + V((s * HRAD * lobe_k(0 if s > 0 else math.pi), 0, CUT + 0.006))
    knots.append(kn)
bucket = D.part([shell, floor, handle] + knots, "bucket", GRIP)

# ------------------------------------------------------------------ candy heap (origin on the floor)
FL = BC + V((0, 0, FLOOR_Z))
heap = []


def mini_wrapped(name, c, col, yaw, tilt=0.0):
    parts = [blob(name, (0, 0, 0), (0.034, 0.024, 0.024), segs=7, rings=5, color=col, mat="flat")]
    for s in (1, -1):
        parts.append(K.cylinder(f"{name}_e{s}", 0.008, 0.026, color=col, mat="flat", verts=5, radius_top=0.021,
                                loc=(s * 0.043, 0, 0), rot=(0, 90 * s, 0)))
    ob = K.merge(parts, name)
    ob.data.transform(Matrix.Rotation(math.radians(tilt), 4, "Y"))
    ob.data.transform(Matrix.Rotation(math.radians(yaw), 4, "Z"))
    ob.location = c
    heap.append(ob)


# a ring of gumballs filling the floor, wrapped candies and a gold coin piled on top
ring_cols = ["candy_violet", "candy_cherry", "candy_lemon", "candy_green", "gumball_blue", "candy_orange"]
for i, col in enumerate(ring_cols):
    a = math.radians(20 + 60 * i)
    r = 0.035 - 0.003 * (i % 2)
    heap.append(blob(f"gum{i}", FL + V((math.cos(a) * 0.078, math.sin(a) * 0.078, r * 0.75)), (r, r, r * 0.95),
                     segs=7, rings=5, color=col, mat="flat"))
mini_wrapped("hw_pink", FL + V((0.0, -0.025, 0.045)), "candy_magenta", 15, -12)
mini_wrapped("hw_mint", FL + V((-0.035, 0.03, 0.05)), "candy_orange", 70, 15)
heap.append(K.cylinder("coin", 0.027, 0.009, color="foil_gold", mat="metal", verts=10,
                       loc=FL + V((0.04, -0.01, 0.06)), rot=(28, 12, 0)))
heap.append(K.cylinder("heap_stick", 0.006, 0.1, color="lolly_white", mat="flat", verts=5,
                       loc=FL + V((0.02, 0.03, 0.06)), rot=(-18, 12, 0)))
heap.append(K.cylinder("heap_lolly", 0.036, 0.016, color="candy_pink", mat="flat", verts=12,
                       loc=FL + V((0.031, 0.045, 0.112)), rot=(80, 0, 10)))
heap.append(K.torus("heap_lolly_swirl", 0.02, 0.0065, color="lolly_white", mat="flat", major_seg=10, minor_seg=3,
                    loc=FL + V((0.031, 0.045 - 0.0085, 0.112)), rot=(80, 0, 10)))
bucket_candy = D.part(heap, "bucket_candy", FL)

# ================================================================== hierarchy
for p in (body, head, wing_l, wing_r, tail, leg_l, leg_r, arm_l, arm_r):
    D.attach(p, root)
D.attach(bucket, arm_r)
D.attach(bucket_candy, bucket)
D.attach(K.empty("socket_bucket_light", tuple(BC)), bucket)

for p in (body, head, wing_l, wing_r, tail, leg_l, leg_r, arm_l, arm_r, bucket, bucket_candy):
    print("PART", p.name, D.tris(p))
print("TRIS", K.tri_count([root]))
K.bake_ao([root], samples=40, radius=0.3, strength=0.8)
K.export([root], "characters/dragon_kid")
if "--nosheet" not in sys.argv:
    K.review_sheet([root], "dragon_kid")
