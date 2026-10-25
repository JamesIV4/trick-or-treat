"""dragon_kid: the player character. 1.1 m child in a teal dragon onesie holding a glowing
jack-o'-lantern bucket. Every part is its own object with its origin at the joint, parented to an
empty root 'dragon_kid' at the feet. Character right = Blender -X (front faces -Y).

Parts (all identity-rotated, origin at the joint):
  body (pelvis)  head (neck)  wing_l / wing_r (wing roots on the back)  tail (tail base)
  leg_l / leg_r (hips)  arm_l / arm_r (shoulders)  bucket (child of arm_r, origin at the grip)
  bucket_candy (child of bucket, origin on the bucket floor; scale 0..1)  socket_bucket_light
"""
import math
import sys
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parent))
import bmesh  # noqa: E402
from mathutils import Euler, Matrix, Vector  # noqa: E402

import cc_helpers as H  # noqa: E402
from cc_helpers import K  # noqa: E402

K.reset()
T, TD, TL = "dragon_teal", "dragon_teal_dark", "dragon_teal_light"

# ------------------------------------------------------------------ joints
HIP_Z = 0.30
HIP_L, HIP_R = (0.085, 0.0, HIP_Z), (-0.085, 0.0, HIP_Z)
SHO_L, SHO_R = (0.165, 0.0, 0.585), (-0.165, 0.0, 0.585)
NECK = (0.0, 0.0, 0.64)
WING_L, WING_R = (0.075, 0.135, 0.56), (-0.075, 0.135, 0.56)
TAIL_BASE = (0.0, 0.14, 0.34)
HAND_L = Vector((0.245, -0.03, 0.405))
HAND_R = Vector((-0.26, -0.085, 0.415))   # grips the bucket handle

root = K.empty("dragon_kid", (0, 0, 0))
report = {}

# ------------------------------------------------------------------ body (origin at pelvis)
torso = K.sphere("torso", 0.2, color=T, loc=(0, 0.01, 0.45), scale=(1.0, 0.88, 1.12), segments=12, rings=7)
K.deform(torso, lambda c: Vector((c.x * (1 + 0.16 * -c.z / 0.2), c.y * (1 + 0.12 * -c.z / 0.2), c.z)))
belly = K.sphere("belly", 0.15, color="dragon_belly", loc=(0, -0.112, 0.425), scale=(0.88, 0.42, 1.08),
                 segments=10, rings=6)
# soft belly plates: two slightly darker bands make it read as a dragon tummy, not a bib
K.paint(belly, "bone", lambda c, n: abs(c.z - 0.045) < 0.02 or abs(c.z + 0.045) < 0.02)
body_parts = [torso, belly]
for i, (y, z, s) in enumerate([(0.175, 0.56, 0.95), (0.19, 0.46, 1.0), (0.185, 0.36, 0.85)]):
    p = Vector((0, y, z))
    n = Vector((0, 1, 0.25 - i * 0.2))
    body_parts.append(K.cylinder(f"spine{i}", 0.032 * s, 0.065 * s, color=TL, verts=5, radius_top=0.006,
                                 loc=p + n.normalized() * 0.02, rot=H.aim(n), smooth=True))
body = H.part(body_parts, "body", (0, 0, HIP_Z))


# ------------------------------------------------------------------ legs (origin at hip)
def leg(name, hip):
    x = hip[0]
    thigh = K.cylinder(name + "_thigh", 0.082, 0.22, color=T, verts=8, loc=(x * 1.08, 0, 0.2),
                       radius_top=0.09, smooth=True)
    boot = K.sphere(name + "_boot", 0.09, color=TD, loc=(x * 1.12, -0.03, 0.062), scale=(1.0, 1.3, 0.78),
                    segments=10, rings=5)
    K.deform(boot, lambda c: Vector((c.x, c.y, max(c.z, -0.075))))   # flat sole, stands on z = 0
    K.paint(boot, "dragon_belly", lambda c, n: n.z < -0.8)          # cream sole
    claws = [K.sphere(f"{name}_claw{i}", 0.021, color="dragon_horn", segments=4, rings=3,
                      loc=(x * 1.12 + dx, -0.14 + abs(dx) * 0.6, 0.035), scale=(1, 1.25, 0.9))
             for i, dx in enumerate((-0.042, 0.0, 0.042))]
    return H.part([thigh, boot] + claws, name, hip)


leg_l = leg("leg_l", HIP_L)
leg_r = leg("leg_r", HIP_R)


# ------------------------------------------------------------------ arms (origin at shoulder)
def arm(name, sho, hand):
    s = Vector(sho)
    out = 1 if s.x > 0 else -1
    mid = s.lerp(hand, 0.5) + Vector((0.012 * out, 0, 0.0))
    sleeve = K.tube(name + "_sleeve", [s + Vector((-0.03 * out, 0, 0)), mid, hand],
                    radius=0.06, radius_end=0.052, color=T, sides=8)
    cuff = K.torus(name + "_cuff", 0.05, 0.016, color=TL, major_seg=8, minor_seg=3,
                   loc=s.lerp(hand, 0.86), rot=H.aim(hand - s))
    mitten = K.sphere(name + "_mitten", 0.06, color=TD, loc=hand + (hand - s).normalized() * 0.03,
                      scale=(1.0, 0.95, 1.05), segments=7, rings=5)
    thumb = K.sphere(name + "_thumb", 0.025, color=TD, segments=5, rings=3,
                     loc=hand + (hand - s).normalized() * 0.01 + Vector((0, -0.05, 0.01)))
    return H.part([sleeve, cuff, mitten, thumb], name, sho)


arm_l = arm("arm_l", SHO_L, HAND_L)
arm_r = arm("arm_r", SHO_R, HAND_R)

# ------------------------------------------------------------------ head (origin at neck)
HC = Vector((0, 0.01, 0.825))          # hood centre
HA, HB, HCZ = 0.26, 0.235, 0.225        # hood radii (x, y, z)
hood = K.sphere("hood", 0.25, color=T, loc=HC, scale=(HA / 0.25, HB / 0.25, HCZ / 0.25), segments=12, rings=8)
face = K.sphere("face", 0.175, color="skin", loc=(0, -0.125, 0.8), scale=(1.06, 0.62, 0.94), segments=10, rings=8)
RIM_Y = -0.195
rim = K.merge([K.torus("hood_rim_t", 0.17, 0.034, color=TL, major_seg=12, minor_seg=4, loc=(0, RIM_Y, 0.805),
                       rot=(90, 0, 0), scale=(1.06, 1.0, 0.96))], "hood_rim")


def wrap_on_hood(v):
    # slide the rim back so it hugs the hood surface instead of floating like a porthole
    q = 1 - ((v.x - HC.x) / HA) ** 2 - ((v.z - HC.z) / HCZ) ** 2
    ys = HC.y - HB * math.sqrt(max(q, 0.04))
    return Vector((v.x, ys + (v.y - RIM_Y) - 0.004, v.z))


K.deform(rim, wrap_on_hood)
head_parts = [hood, face, rim]
EYE_Y = -0.228
for sx in (1, -1):
    ex = 0.068 * sx
    head_parts.append(K.sphere(f"eye{sx}", 0.043, color="ink", loc=(ex, EYE_Y, 0.81), scale=(0.82, 0.42, 1.12),
                               segments=8, rings=5, rot=(0, 0, -14 * sx)))
    head_parts.append(K.sphere(f"hl{sx}", 0.0145, color="eye_white", loc=(ex - 0.012, EYE_Y - 0.016, 0.83),
                               segments=5, rings=3))
    head_parts.append(K.sphere(f"hl2{sx}", 0.0075, color="eye_white", loc=(ex + 0.012, EYE_Y - 0.016, 0.795),
                               segments=5, rings=3))
    head_parts.append(K.sphere(f"cheek{sx}", 0.03, color="cheek_pink", loc=(0.122 * sx, -0.208, 0.762),
                               scale=(1.25, 0.45, 0.75), segments=6, rings=3, rot=(0, 0, -30 * sx)))
    # tiny cream horns, leaning out and back
    hb = Vector((0.095 * sx, 0.035, 1.0))
    hd = Vector((0.45 * sx, 0.35, 1.0))
    head_parts.append(K.cylinder(f"horn{sx}", 0.037, 0.085, color="dragon_horn", verts=7, radius_top=0.013,
                                 loc=hb + hd.normalized() * 0.04, rot=H.aim(hd), smooth=True))
    head_parts.append(K.sphere(f"horntip{sx}", 0.013, color="dragon_horn", segments=4, rings=3,
                               loc=hb + hd.normalized() * 0.083))
# smile
head_parts.append(K.torus("smile", 0.022, 0.0075, color="ink", major_seg=6, minor_seg=3, arc=180,
                          loc=(0, -0.232, 0.77), rot=(-90, 0, 0)))
# hair fringe peeking under the hood rim (asymmetric swoop)
for i, (x, s, r, dz) in enumerate([(-0.062, 0.95, 28, -0.012), (0.0, 1.1, 8, 0.0), (0.062, 0.8, -22, -0.004)]):
    head_parts.append(K.sphere(f"fringe{i}", 0.042 * s, color="hair_brown", loc=(x, -0.198, 0.93 + dz),
                               scale=(1.25, 0.55, 0.62), segments=6, rings=4, rot=(0, r, 0)))
# felt spikes along the hood crest
for i, ang in enumerate((70, 38, 8)):
    a = math.radians(ang)
    n = Vector((0, math.cos(a), math.sin(a)))
    p = HC + Vector((0, n.y * HB, n.z * HCZ))
    s = (0.8, 1.0, 1.0)[i]
    head_parts.append(K.cylinder(f"crest{i}", 0.04 * s, 0.075 * s, color=TL, verts=5, radius_top=0.008,
                                 loc=p + n * 0.025, rot=H.aim(n), smooth=True))
head = H.part(head_parts, "head", NECK)
# a little curious head tilt (baked into the mesh so the rest pose stays identity)
head.data.transform(Matrix.Rotation(math.radians(-6), 4, "Y") @ Matrix.Rotation(math.radians(-4), 4, "X"))

# ------------------------------------------------------------------ wings (origin at wing root)
WING = [(0.0, -0.05), (0.07, -0.07), (0.12, 0.01), (0.2, -0.01), (0.25, 0.06),
        (0.29, 0.2), (0.17, 0.16), (0.06, 0.09), (0.0, 0.06)]


def _rot_pts(pts, sx, yaw, roll):
    m = Euler((0, math.radians(roll), math.radians(yaw))).to_matrix()
    return [m @ Vector((x * sx, y, z)) for x, y, z in pts]


def wing(name, root_pt, sx):
    pts = [(x * sx, z) for x, z in WING]
    if sx < 0:
        pts = list(reversed(pts))
    yaw, roll = 32 * sx, -12 * sx
    r = Vector(root_pt)
    mem = H.slab(name + "_m", pts, 0.032, TL, loc=root_pt, rot=(0, roll, yaw), bevel=0.008)
    bone = K.tube(name + "_bone", [r + v for v in _rot_pts([(0.0, 0, 0.065), (0.11, 0, 0.125), (0.2, 0, 0.175),
                                                           (0.29, 0, 0.205)], sx, yaw, roll)],
                  radius=0.022, radius_end=0.014, color=TD, sides=5)
    return H.part([mem, bone], name, root_pt)


wing_l = wing("wing_l", WING_L, 1)
wing_r = wing("wing_r", WING_R, -1)

# ------------------------------------------------------------------ tail (origin at tail base)
tail_pts = [(0, 0.1, 0.36), (0.0, 0.22, 0.28), (0.02, 0.34, 0.17), (0.06, 0.45, 0.115), (0.11, 0.55, 0.12),
            (0.16, 0.62, 0.175), (0.19, 0.65, 0.25)]
tail_tube = K.tube("tail_tube", tail_pts, radius=0.12, radius_end=0.032, color=T, sides=8)
K.paint(tail_tube, "dragon_belly", lambda c, n: n.z < -0.55 and c.y > 0.18)
tail_parts = [tail_tube,
              K.sphere("tail_tip", 0.045, color=TL, loc=(0.192, 0.655, 0.262), scale=(1, 1, 1.15),
                       segments=6, rings=4)]
for i, t in enumerate((1.4, 2.4, 3.4, 4.4)):
    a, b = Vector(tail_pts[int(t)]), Vector(tail_pts[int(t) + 1])
    p = a.lerp(b, t - int(t))
    tan = (b - a).normalized()
    up = Vector((0, 0, 1))
    n = (up - tan * up.dot(tan)).normalized()
    rad = 0.12 + (0.032 - 0.12) * t / 6
    s = 1.0 - i * 0.16
    tail_parts.append(K.cylinder(f"tspike{i}", 0.034 * s, 0.062 * s, color=TL, verts=5, radius_top=0.006,
                                 loc=p + n * (rad + 0.012), rot=H.aim(n), smooth=True))
tail = H.part(tail_parts, "tail", TAIL_BASE)

# ------------------------------------------------------------------ bucket (child of arm_r, origin at grip)
GRIP = HAND_R + Vector((0, 0, -0.035))
BR = 0.14                        # bucket sphere radius
CUT = 0.06                       # rim height above the sphere centre
rim_r = math.sqrt(BR ** 2 - CUT ** 2)
BC = Vector((GRIP.x - 0.012, GRIP.y - 0.01, GRIP.z - rim_r - CUT))   # sphere centre


def lobes(c):
    ang = math.atan2(c.y, c.x)
    k = 1 + 0.05 * math.cos(6 * ang + math.pi)   # a lobe bulges at the front (-Y) under the face
    return Vector((c.x * k, c.y * k, max(c.z, -0.115)))   # 6 soft lobes, flat base


shell = K.sphere("bucket_shell", BR, color="pumpkin", loc=(0, 0, 0), segments=12, rings=8)
bm = bmesh.new()
bm.from_mesh(shell.data)
bmesh.ops.delete(bm, geom=[f for f in bm.faces if f.calc_center_median().z > CUT], context="FACES")
bm.to_mesh(shell.data)
bm.free()
K.deform(shell, lobes)
shell.location = BC
_bm = bmesh.new()   # single upward-facing floor disc (the bucket's dark inside)
_bm.faces.new([_bm.verts.new((math.cos(a) * rim_r * 0.98, math.sin(a) * rim_r * 0.98, 0))
               for a in (2 * math.pi * i / 12 for i in range(12))])
inner = K._finish(_bm, "bucket_inner", "pumpkin_dark", BC + Vector((0, 0, 0.04)), (0, 0, 0), (1, 1, 1), False)
brim = K.torus("bucket_rim", rim_r, 0.017, color="pumpkin_dark", major_seg=12, minor_seg=3,
               loc=BC + Vector((0, 0, CUT)))
handle = K.torus("bucket_handle", rim_r - 0.004, 0.011, color="rope", major_seg=8, minor_seg=4, arc=180,
                 loc=BC + Vector((0, 0, CUT)), rot=(90, 0, 0))
knots = [K.sphere(f"bucket_knot{s}", 0.02, color="rope", segments=5, rings=3,
                  loc=BC + Vector((s * rim_r, 0, CUT))) for s in (1, -1)]
face_parts = []


def shell_r(theta, z):
    """Radius of the lobed shell at height z (rel. centre), theta measured from -Y toward +X."""
    return math.sqrt(max(BR ** 2 - z ** 2, 1e-4)) * (1 + 0.05 * math.cos(6 * theta))


def carve(name, cols, cx, cz, color="glow_jack_hot"):
    """Glowing face cut-out: a thin plug wrapped onto the curved bucket surface. cols is a list of
    (arc u, v_top, v_bottom) columns around (cx, cz); narrow strips between columns keep every
    face hugging the shell (a single wide cap would sag under the surface)."""
    bm = bmesh.new()
    # build flat first (u, layer, v), then wrap onto the shell
    ot = [bm.verts.new((u, 0, t)) for u, t, b in cols]
    ob = [bm.verts.new((u, 0, b)) for u, t, b in cols]
    it = [bm.verts.new((u, 1, t)) for u, t, b in cols]
    ib = [bm.verts.new((u, 1, b)) for u, t, b in cols]
    for i in range(len(cols) - 1):
        j = i + 1
        bm.faces.new((ob[i], ob[j], ot[j], ot[i]))      # outer glow surface
        bm.faces.new((ot[i], ot[j], it[j], it[i]))      # top wall dipping into the shell
        bm.faces.new((ob[i], ib[i], ib[j], ob[j]))      # bottom wall
    # (no inner face: it is buried in the shell and would only cost triangles)
    for vt in bm.verts:
        u, layer, v = vt.co
        z = cz + v
        th = (cx + u) / math.sqrt(max(BR ** 2 - z ** 2, 1e-4))
        d = Vector((math.sin(th), -math.cos(th), 0))
        vt.co = BC + d * (shell_r(th, z) + (0.004 if layer < 0.5 else -0.016)) + Vector((0, 0, z))
    bmesh.ops.recalc_face_normals(bm, faces=bm.faces)
    face_parts.append(K._finish(bm, name, color, (0, 0, 0), (0, 0, 0), (1, 1, 1), False))


tri_eye = [(-0.031, -0.015, -0.018), (0.0, 0.03, -0.018), (0.031, -0.015, -0.018)]
carve("eye_l", tri_eye, 0.05, 0.02)
carve("eye_r", tri_eye, -0.05, 0.02)
carve("nose", [(-0.014, -0.007, -0.009), (0.0, 0.012, -0.009), (0.014, -0.007, -0.009)], 0.0, -0.012)
grin = [(-0.08, 0.017, 0.007), (-0.056, 0.004, -0.026), (-0.032, -0.004, -0.04), (-0.013, -0.008, -0.046),
        (-0.011, -0.024, -0.047), (0.011, -0.024, -0.047), (0.013, -0.008, -0.046), (0.032, -0.004, -0.04),
        (0.056, 0.004, -0.026), (0.08, 0.017, 0.007)]   # crescent grin, one square tooth hanging down
carve("grin", grin, 0.0, -0.05)
bucket = H.part([shell, inner, brim, handle] + knots + face_parts, "bucket", GRIP)

# heap of candy inside (origin on the bucket floor so 0..1 scaling grows it out of the bucket)
floor = BC + Vector((0, 0, 0.041))
heap = []
for i, (dx, dy, dz, col) in enumerate([(0.0, 0.0, 0.05, "candy_pink"), (0.052, 0.022, 0.026, "candy_mint"),
                                       (-0.052, -0.01, 0.028, "candy_lemon"), (0.02, -0.052, 0.03, "wrapper_purple"),
                                       (-0.02, 0.052, 0.03, "gumball_red")]):
    heap.append(K.sphere(f"heap{i}", 0.04 if i == 0 else 0.037, color=col, segments=5, rings=4,
                         loc=floor + Vector((dx, dy, dz)), scale=(1.1, 1.0, 0.85)))
heap.append(K.sphere("heap_wrap", 0.03, color="foil_gold", segments=6, rings=4,
                     loc=floor + Vector((-0.015, -0.02, 0.088)), scale=(1.5, 1.0, 0.9), rot=(0, 20, 30)))
heap.append(K.cylinder("heap_stick", 0.007, 0.09, color="lolly_white", verts=5,
                       loc=floor + Vector((0.035, 0.02, 0.095)), rot=(0, 25, 0)))
heap.append(K.cylinder("heap_lolly", 0.036, 0.016, color="candy_cherry", verts=7,
                       loc=floor + Vector((0.055, 0.02, 0.14)), rot=(90, 0, 15)))
bucket_candy = H.part(heap, "bucket_candy", floor)

# ------------------------------------------------------------------ hierarchy
for p in (body, head, wing_l, wing_r, tail, leg_l, leg_r, arm_l, arm_r):
    H.attach(p, root)
H.attach(bucket, arm_r)
H.attach(bucket_candy, bucket)
H.attach(K.empty("socket_bucket_light", tuple(BC)), bucket)

for p in (body, head, wing_l, wing_r, tail, leg_l, leg_r, arm_l, arm_r, bucket, bucket_candy):
    print("PART", p.name, sum(len(f.vertices) - 2 for f in p.data.polygons))
print("TRIS", K.tri_count([root]))
K.export([root], "characters/dragon_kid")
K.review_sheet([root], "dragon_kid")
H.close_sheet([root], "dragon_kid")
