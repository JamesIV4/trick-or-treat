"""Hanging decor (batch03): string_jack, bunting_flag_orange/purple/cream, door_wreath.

    blender --background --factory-startup --python build_decor.py -- [ids...]

Conventions: origin at the hang point (string_jack: top of its socket; bunting: top-centre of the
hem, where the cord runs; door_wreath: back centre, touching the door). Front faces Blender -Y.
"""
import pathlib
import random
import sys

sys.path.insert(0, str(pathlib.Path(__file__).resolve().parent))
from decor_common import *  # noqa: E402,F401,F403
from decor_common import B, K, Vector, bmesh, math, raw, report, uv_box  # noqa: E402

import bpy  # noqa: E402


# ------------------------------------------------------------------ string_jack
def ribbed_globe(name, R, squash, centre, seg, rings, rib, color, mat="flat", front_calm=0.75):
    """Pumpkin globe, poles on Z. rib = groove depth fraction; grooves calm down on the front
    (-Y) so the carved-face decal sits flush. A ridge faces -Y."""
    bm = bmesh.new()
    cx, cy, cz = centre
    rows = []

    def rad(a):
        groove = 0.5 - 0.5 * math.cos((a + math.pi / 2) * seg / 2)  # 0 on ridges, 1 in grooves
        front = max(0.0, -math.sin(a)) ** 2
        return 1.0 - rib * groove * (1 - front_calm * front)

    for i in range(rings + 1):
        lat = -math.pi / 2 + math.pi * i / rings
        if i in (0, rings):
            # dimpled poles (pumpkin top/bottom pulled in a little)
            rows.append([bm.verts.new((cx, cy, cz + math.sin(lat) * R * squash * 0.86))])
            continue
        row = []
        for j in range(seg):
            a = 2 * math.pi * j / seg - math.pi / 2
            r = R * math.cos(lat) * rad(a)
            row.append(bm.verts.new((cx + math.cos(a) * r, cy + math.sin(a) * r, cz + math.sin(lat) * R * squash)))
        rows.append(row)
    for i in range(rings):
        r0, r1 = rows[i], rows[i + 1]
        for j in range(seg):
            j1 = (j + 1) % seg
            if len(r0) == 1:
                bm.faces.new((r0[0], r1[j1], r1[j]))
            elif len(r1) == 1:
                bm.faces.new((r0[j], r0[j1], r1[0]))
            else:
                bm.faces.new((r0[j], r0[j1], r1[j1], r1[j]))
    bmesh.ops.recalc_face_normals(bm, faces=bm.faces)
    bm.normal_update()
    uv_box(bm, bm.faces, 4.0)
    K._paint_bm(bm, color, mat)
    for f in bm.faces:
        f.smooth = True
    me = bpy.data.meshes.new(name)
    bm.to_mesh(me)
    bm.free()
    ob = K._link(bpy.data.objects.new(name, me))
    return ob, rad


def on_globe(centre, R, squash, rad, x, z, lift):
    """Point on the globe's front (-Y) at screen offsets x (right) and z (up) from the centre."""
    cx, cy, cz = centre
    # solve direction: start from the sphere and correct for rib + squash
    zz = max(-0.98, min(0.98, z / (R * squash)))
    lat = math.asin(zz)
    ring = R * math.cos(lat)
    xx = max(-0.98, min(0.98, x / max(ring, 1e-4)))
    a = -math.pi / 2 + math.asin(xx)
    r = ring * rad(a) + lift
    return (cx + math.cos(a) * r, cy + math.sin(a) * r, cz + math.sin(lat) * R * squash + lift * zz * 0.3)


def string_jack():
    K.reset()
    parts = []
    R, sq = 0.058, 0.86
    c = (0.0, 0.0, -0.077)
    globe, rad = ribbed_globe("globe", R, sq, c, 10, 6, 0.12, "glow_jack_shell")
    parts.append(globe)
    # carved face decal (glow_jack_face) just proud of the darker glow_jack_shell
    lift = 0.0032
    eye = [(-0.011, 0.0), (0.011, 0.0), (0.0, 0.019)]
    mouth = [(-0.034, -0.006), (-0.012, -0.016), (0.012, -0.016), (0.034, -0.006), (0.02, -0.027), (0.0, -0.032),
             (-0.02, -0.027)]
    verts, faces = [], []
    for ex in (-0.02, 0.02):
        b = len(verts)
        verts += [on_globe(c, R, sq, rad, ex + x, 0.006 + z, lift) for x, z in eye]
        faces.append(((b, b + 2, b + 1), "glow_jack_face", "flat"))
    b = len(verts)
    verts += [on_globe(c, R, sq, rad, x, z, lift) for x, z in mouth]
    # mouth outline (counter-clockwise seen from the front -> listed so the normal faces -Y)
    faces.append(((b, b + 6, b + 5, b + 4, b + 3, b + 2, b + 1), "glow_jack_face", "flat"))
    face = raw("face", verts, faces)
    _face_front(face)
    parts.append(face)
    # little green stem and the black lamp socket that clips onto the wire
    parts.append(lathe("stem", [(0.0, -0.032), (0.014, -0.03), (0.009, -0.016)], "stem_green", "wood",
                       segments=5, cap_bottom=False, cap_top=False))
    parts.append(lathe("socket", [(0.0, -0.02), (0.013, -0.019), (0.013, -0.004), (0.0, 0.0)], B,
                       "metal", segments=6, cap_bottom=False, cap_top=False, sharp_bands=(0,)))
    report(parts)
    ob = K.merge(parts, "string_jack")
    socket("light", c, ob)
    return finish([ob], "props/string_jack", "string_jack", ao_radius=0.03, ao_strength=0.35, ground_ao=False)


def _face_front(ob):
    """Make every face of a front decal point toward -Y."""
    bm = bmesh.new()
    bm.from_mesh(ob.data)
    bm.normal_update()
    flip = [f for f in bm.faces if f.normal.y > 0]
    if flip:
        bmesh.ops.reverse_faces(bm, faces=flip)
    bm.to_mesh(ob.data)
    bm.free()
    ob.data.update()


# ------------------------------------------------------------------ bunting
W, H, TOP = 0.14, 0.34, -0.009


def wave(x, z):
    t = (TOP - z) / (H + TOP)  # 0 at the hem, 1 at the tip
    t = max(0.0, min(1.0, t))
    return 0.015 * math.sin(x / W * math.pi * 1.15 + 0.5) * (0.3 + 0.7 * t) - 0.014 * t * t


def pennant(asset_id, color, bat=False):
    K.reset()
    rows, cols = (2, 4) if bat else (3, 4)
    verts, faces = [], []
    idx = {}
    for r in range(rows + 1):
        t = r / rows
        z = TOP - t * (H + TOP)
        if r == rows:
            idx[(r, 0)] = len(verts)
            verts.append((0.0, 0.0, z))
            continue
        half = W * (1 - t)
        for cc in range(cols + 1):
            x = (cc / cols * 2 - 1) * half
            idx[(r, cc)] = len(verts)
            verts.append((x, 0.0, z))
    quads = []
    for r in range(rows):
        for cc in range(cols):
            if r + 1 == rows:
                quads.append((idx[(r, cc)], idx[(r + 1, 0)], idx[(r, cc + 1)]))
            else:
                quads.append((idx[(r, cc)], idx[(r + 1, cc)], idx[(r + 1, cc + 1)], idx[(r, cc + 1)]))
    eps = 0.0012
    front = [(x, wave(x, z) - eps, z) for x, _, z in verts]
    back = [(x, wave(x, z) + eps, z) for x, _, z in verts]
    n = len(verts)
    allv = front + back
    for q in quads:
        faces.append((q, color, "felt"))
        faces.append((tuple(i + n for i in reversed(q)), color, "felt"))
    cloth = raw("cloth", allv, faces, smooth=True)
    _orient_two_sided(cloth, n)
    parts = [cloth]
    # rolled hem the cord runs through
    parts.append(sweep("hem", [(-W - 0.006, 0, -0.006), (W + 0.006, 0, -0.006)], 0.0075, color=color, mat="felt",
                       sides=4, caps=(False, False), phase=math.pi / 4))
    if bat:
        parts += bat_decal()
    report(parts)
    ob = K.merge(parts, asset_id)
    return finish([ob], f"props/{asset_id}", asset_id, ao_radius=0.03, ao_strength=0.3, ground_ao=False)


def _orient_two_sided(ob, n_front):
    """Front copy (verts < n_front) faces -Y, back copy faces +Y."""
    bm = bmesh.new()
    bm.from_mesh(ob.data)
    bm.normal_update()
    flip = []
    for f in bm.faces:
        is_front = all(v.index < n_front for v in f.verts)
        if (is_front and f.normal.y > 0) or (not is_front and f.normal.y < 0):
            flip.append(f)
    if flip:
        bmesh.ops.reverse_faces(bm, faces=flip)
    bm.to_mesh(ob.data)
    bm.free()
    ob.data.update()


def bat_decal(cz=-0.096, s=1.0):
    """Black bat silhouette on both faces of the cream pennant (two pointy ears, round head,
    scalloped wings)."""
    half = [(0.0, 0.018), (0.011, 0.036), (0.018, 0.016), (0.032, 0.014), (0.09, 0.04), (0.075, 0.0), (0.058, 0.01),
            (0.04, -0.02)]
    bottom = (0.0, -0.024)
    outline = half + [bottom] + [(-x, z) for x, z in reversed(half[1:])]
    out = []
    for side in (-1, 1):
        lift = 0.0035 * side
        vs = [(x * s, wave(x * s, cz + z * s) + lift, cz + z * s) for x, z in outline]
        order = list(range(len(vs)))
        ob = raw(f"bat{side}", vs, [(tuple(order), "cat_black", "felt")])
        bm = bmesh.new()
        bm.from_mesh(ob.data)
        bm.normal_update()
        bm.faces.ensure_lookup_table()
        if (side < 0 and bm.faces[0].normal.y > 0) or (side > 0 and bm.faces[0].normal.y < 0):
            bmesh.ops.reverse_faces(bm, faces=bm.faces[:])
        bm.to_mesh(ob.data)
        bm.free()
        out.append(ob)
    return out


# ------------------------------------------------------------------ door_wreath
RING_R, TUBE_R, DEPTH_K = 0.2, 0.058, 0.7
RING_Y = -TUBE_R * DEPTH_K  # ring centre in front of the door so the back touches y = 0


def ring_point(theta, phi, lift=0.0):
    """Surface point + normal on the squashed torus. theta round the ring (0 = +X, CCW seen from
    the front), phi round the tube (0 = outward, +90 = toward the front -Y)."""
    radial = Vector((math.cos(theta), 0, math.sin(theta)))
    fwd = Vector((0, -1, 0))
    n = (radial * math.cos(phi) + fwd * math.sin(phi) / DEPTH_K).normalized()
    p = radial * (RING_R + math.cos(phi) * (TUBE_R + lift)) + fwd * (math.sin(phi) * (TUBE_R + lift) * DEPTH_K)
    p.y += RING_Y
    return p, n


def leaf_verts(centre, normal, tip, size, shape, cup=0.1):
    """Fan leaf: shape = [(polar angle deg, radius fraction), ...] round the centre; 90 deg points
    along `tip` (projected into the leaf plane). Centre raised by `cup`, edges curl back a little."""
    v = (tip - normal * tip.dot(normal)).normalized()
    u = v.cross(normal)
    out = [centre + normal * size * cup]
    for a, rr in shape:
        ar = math.radians(a)
        d = (u * math.cos(ar) + v * math.sin(ar)) * size * rr
        out.append(centre + d - normal * size * 0.1 * rr * rr)
    return out


# maple: broad rounded lobes with shallow notches (deep notches read as stars), stem notch
MAPLE = [(90, 1.0), (68, 0.66), (38, 0.86), (12, 0.64), (-18, 0.72), (-60, 0.46), (-90, 0.26), (-120, 0.46),
         (198, 0.72), (168, 0.64), (142, 0.86), (112, 0.66)]
# elm/birch teardrop leaf
DROP = [(90, 1.0), (45, 0.56), (0, 0.5), (-50, 0.42), (-90, 0.24), (230, 0.42), (180, 0.5), (135, 0.56)]
OVAL = [(90, 1.0), (0, 0.38), (-90, 0.5), (180, 0.38)]


def door_wreath():
    K.reset()
    rnd = random.Random(31)
    parts = []
    base = K.torus("base", RING_R, TUBE_R, color="bark_dark", mat="leaves", major_seg=12, minor_seg=6,
                   rot=(90, 0, 0), smooth=True)
    base.scale = (1, 1, DEPTH_K)
    bake(base)
    base.location = (0, RING_Y, 0)
    bake(base)
    _drop_back(base)
    parts.append(base)
    verts, faces = [], []

    def add_leaf(theta, phi, size, shape, color, lift, cup=0.1, swirl=35):
        p, n = ring_point(theta, phi, lift)
        n = (n + Vector((0, -0.5, 0))).normalized()
        tangent = Vector((-math.sin(theta), 0, math.cos(theta)))
        radial = Vector((math.cos(theta), 0, math.sin(theta)))
        j = math.radians(rnd.uniform(-swirl, swirl))
        tip = tangent * math.cos(j) + radial * math.sin(j)
        vs = leaf_verts(p, n, tip, size, shape, cup)
        b = len(verts)
        verts.extend(tuple(v) for v in vs)
        m = len(vs) - 1
        for k in range(m):
            faces.append(((b, b + 1 + k, b + 1 + (k + 1) % m), color, "leaves"))

    # dark filler leaves first (hide the base on the outer, front and inner faces) ...
    nf = 30
    for i in range(nf):
        th = 2 * math.pi * (i + rnd.uniform(-0.3, 0.3)) / nf
        ph = rnd.uniform(-10, 55) if i % 2 == 0 else rnd.uniform(105, 165)
        add_leaf(th, math.radians(ph), rnd.uniform(0.05, 0.064), OVAL,
                 rnd.choice(["rust_leaf", "bark_dark", "wood_dark", "olive", "cinnamon"]), 0.004, cup=0.12)
    for i in range(16):  # extra mid-tone leaves on the front face where the bare ring showed through
        th = 2 * math.pi * (i + 0.5 + rnd.uniform(-0.3, 0.3)) / 16
        add_leaf(th, math.radians(rnd.uniform(70, 120)), rnd.uniform(0.052, 0.064), OVAL,
                 rnd.choice(["rust_leaf", "burnt_amber", "cinnamon"]), 0.009, cup=0.12)
    # ... then the bright autumn maples swirling round on top
    bright = ["maple_orange"] * 5 + ["golden_leaf"] * 4 + ["pumpkin"] * 2 + ["rust_leaf"] * 2 + ["maple_red"]
    nm = 40
    for i in range(nm):
        th = 2 * math.pi * (i + rnd.uniform(-0.3, 0.3)) / nm + 0.1
        ph = (rnd.uniform(-25, 25), rnd.uniform(45, 90), rnd.uniform(110, 160))[i % 3]
        maple = i % 2 == 0
        add_leaf(th, math.radians(ph), rnd.uniform(0.058, 0.074) if maple else rnd.uniform(0.05, 0.062),
                 MAPLE if maple else DROP, rnd.choice(bright), 0.014 + 0.005 * (i % 2), cup=0.06)
    leaves = raw("leaves", verts, faces)
    _face_out(leaves)
    parts.append(leaves)
    # berry clusters (little octahedra, smooth shaded so they read round)
    bv, bf = [], []
    for k, th_deg in enumerate((30, 110, 160, 215, 330)):
        th = math.radians(th_deg)
        for j in range(2 if k % 2 else 2 if k == 4 else 1 + 1):
            p, n = ring_point(th + math.radians(rnd.uniform(-8, 8)), math.radians(rnd.uniform(50, 90)), 0.032)
            r = rnd.uniform(0.012, 0.015)
            b = len(bv)
            for d in ((1, 0, 0), (-1, 0, 0), (0, 1, 0), (0, -1, 0), (0, 0, 1), (0, 0, -1)):
                bv.append(tuple(p + Vector(d) * r))
            col = "apple_red" if (k + j) % 3 else "wrapper_purple"
            for a, bb, cc in ((0, 2, 4), (2, 1, 4), (1, 3, 4), (3, 0, 4), (2, 0, 5), (1, 2, 5), (3, 1, 5), (0, 3, 5)):
                bf.append(((b + a, b + bb, b + cc), col, "flat", True))
    berries = raw("berries", bv, bf)
    _face_out(berries, per_cluster=True)
    parts.append(berries)
    parts += bow((0, RING_Y - TUBE_R * DEPTH_K - 0.035, -RING_R - 0.015))
    report(parts)
    ob = K.merge(parts, "door_wreath")
    return finish([ob], "props/door_wreath", "door_wreath", ao_radius=0.06, ao_strength=0.6, ground_ao=False)


def _drop_back(ob):
    """Delete the base ring's faces that face the door (never seen)."""
    bm = bmesh.new()
    bm.from_mesh(ob.data)
    bm.normal_update()
    dead = [f for f in bm.faces if f.normal.y > 0.6]
    bmesh.ops.delete(bm, geom=dead, context="FACES")
    bm.to_mesh(ob.data)
    bm.free()
    ob.data.update()

def _face_out(ob, per_cluster=False):
    """Leaves face the viewer (-Y); berries face away from their own centre."""
    bm = bmesh.new()
    bm.from_mesh(ob.data)
    bm.normal_update()
    flip = []
    if per_cluster:
        for f in bm.faces:
            # octahedron centre = mean of the cluster's 6 verts: approximate with the face's far vertex set
            c = sum((v.co for v in f.verts), Vector()) / 3
            ce = _oct_centre(f)
            if f.normal.dot(c - ce) < 0:
                flip.append(f)
    else:
        for f in bm.faces:
            if f.normal.y > 0.05:
                flip.append(f)
    if flip:
        bmesh.ops.reverse_faces(bm, faces=flip)
    bm.to_mesh(ob.data)
    bm.free()
    ob.data.update()


def _oct_centre(f):
    vs = set()
    for v in f.verts:
        for e in v.link_edges:
            vs.add(e.other_vert(v))
        vs.add(v)
    return sum((v.co for v in vs), Vector()) / len(vs)


def bow(c, col="banner_purple"):
    cx, cy, cz = c
    parts = []
    for s in (-1, 1):
        # puffy filled loops read as a bow from across the street
        parts.append(blob(f"loop{s}", 0.05, col, mat="felt", loc=(cx + s * 0.052, cy + 0.004, cz + 0.012),
                          scale=(1.0, 0.42, 0.66), rot=(0, s * 18, 0), segments=7, rings=4, noise_amp=0.0, axis="X"))
        tail = [(cx + s * 0.008, cy + 0.002, cz - 0.01), (cx + s * 0.035, cy - 0.002, cz - 0.07),
                (cx + s * 0.06, cy - 0.008, cz - 0.115)]
        parts.append(sweep(f"tail{s}", tail, [0.012, 0.014, 0.017], color=col, mat="felt", sides=4,
                           caps=(False, True), shape=lambda i, a: 1.0 + 0.6 * math.cos(2 * a), up_hint=(1, 0, 0)))
    parts.append(blob("knot", 0.022, col, mat="felt", loc=(cx, cy - 0.006, cz), scale=(1, 0.8, 1.1), segments=6, rings=4,
                      noise_amp=0.0, axis="Z"))
    return parts


if want("string_jack"):
    string_jack()
if want("bunting_flag_orange"):
    pennant("bunting_flag_orange", "banner_orange")
if want("bunting_flag_purple"):
    pennant("bunting_flag_purple", "banner_purple")
if want("bunting_flag_cream"):
    pennant("bunting_flag_cream", "bunting_cream", bat=True)
if want("door_wreath"):
    door_wreath()
