"""Sky (v2): cloud_a, cloud_b, cloud_c (soft billowy storm puffs) and bat (flappable wings).

Clouds: overlapping smooth lumpy puffs (cloud material, sphere projection with poles on the long
axis), every vertex below a shared base plane squashed toward it so the bottom is soft and flat
(no cut caps, no geodesic facets from below). Buried faces culled.
"""
import pathlib
import sys

sys.path.insert(0, str(pathlib.Path(__file__).resolve().parent))
from yard_common import *  # noqa: E402,F401,F403
import bmesh  # noqa: E402

TOP, MID, BELLY = "roof_slate", "roof_slate", "shadow_navy"


def cloud(asset_id, layout, depth, seed, base=0.0, res=1.0):
    K.reset()
    puffs = []
    for i, (x, y, z, r) in enumerate(layout):
        seg = int(round((7 + 3.2 * r) * res))
        rings = int(round(seg * 0.62))
        b = blob(f"puff{i}", r, TOP, mat="cloud", loc=(x, y, z), scale=(1.0, depth, 0.92), segments=seg, rings=rings,
                 lobes=4, lobe_amp=0.16, lobe_pow=4.0, noise_amp=0.035, noise_freq=1.3, seed=seed + i * 5, axis="X")
        puffs.append(b)
    cull_hidden(puffs, zmin=base + 0.05)  # never cull below the base: that part gets squashed
    for b in puffs:
        flatten_below(b, base, 0.22)

    ob = K.merge(puffs, asset_id)
    xs = [v.co.x for v in ob.data.vertices]
    ys = [v.co.y for v in ob.data.vertices]
    zs = [v.co.z for v in ob.data.vertices]
    cx, cy, cz = (min(xs) + max(xs)) / 2, (min(ys) + max(ys)) / 2, (min(zs) + max(zs)) / 2
    for v in ob.data.vertices:
        v.co -= Vector((cx, cy, cz))
    ob.data.update()
    return finish([ob], f"sky/{asset_id}", asset_id, ao_radius=2.5, ao_strength=0.7, ground_ao=False)


# (x, y, z, r): z is the puff centre above the shared flat base (z = 0)
# base row of big puffs sitting on the plane, smaller billows stacked on top, a couple of fillers
CLOUD_A = [(-5.3, 0.0, 0.75, 1.6), (-2.9, 0.1, 1.15, 2.2), (0.0, 0.0, 1.45, 2.6), (2.9, -0.1, 1.1, 2.15),
           (5.2, 0.0, 0.7, 1.55), (-1.5, 0.1, 2.75, 1.65), (1.3, -0.1, 2.85, 1.6),
           (-1.4, -1.3, 0.6, 1.7), (1.6, 1.3, 0.6, 1.75), (-3.8, 1.1, 0.5, 1.3), (3.9, -1.0, 0.5, 1.3)]
CLOUD_B = [(-3.1, 0.0, 0.7, 1.5), (-0.9, 0.1, 1.25, 2.25), (1.6, -0.1, 1.05, 2.0), (3.4, 0.1, 0.6, 1.3),
           (-0.2, 0.0, 2.75, 1.6), (1.9, 0.1, 2.3, 1.2), (0.0, -1.2, 0.55, 1.65), (0.6, 1.2, 0.55, 1.6)]
CLOUD_C = [(-7.4, 0.1, 0.6, 1.35), (-5.1, -0.1, 0.95, 1.95), (-2.4, 0.1, 1.25, 2.35), (0.6, -0.1, 1.0, 2.05),
           (3.5, 0.1, 1.2, 2.3), (6.2, -0.1, 0.85, 1.8), (8.2, 0.1, 0.5, 1.2), (-3.5, 0.0, 2.65, 1.4),
           (-0.8, 0.0, 2.5, 1.25), (4.2, 0.0, 2.7, 1.45), (-1.0, -1.2, 0.5, 1.55), (2.2, 1.15, 0.5, 1.55),
           (5.6, -1.0, 0.45, 1.3)]


# ---------------------------------------------------------------- bat
def wing_mesh(name, sx):
    """Scalloped bat wing, x outward from the shoulder (origin), slight thickness and dihedral,
    three finger ribs as raised ridges."""
    S = 0.82
    outline = [(0.0, 0.035), (0.08, 0.1), (0.2, 0.088), (0.235, 0.03), (0.185, -0.032), (0.15, -0.008),
               (0.11, -0.05), (0.072, -0.022), (0.032, -0.06), (0.0, -0.03)]
    t = 0.007
    bm = bmesh.new()
    top, bot = [], []
    for x, z in outline:
        lift = 0.35 * x * x / 0.235 + 0.02 * x
        top.append(bm.verts.new((x * S * sx, -t, z + lift)))
        bot.append(bm.verts.new((x * S * sx, t, z + lift)))
    n = len(outline)
    faces = [bm.faces.new(top), bm.faces.new(list(reversed(bot)))]
    for i in range(n):
        j = (i + 1) % n
        faces.append(bm.faces.new((top[i], top[j], bot[j], bot[i])))
    bmesh.ops.recalc_face_normals(bm, faces=bm.faces)
    bm.normal_update()
    uv_box(bm, bm.faces)
    K._paint_bm(bm, "charcoal", "felt")
    ob = new_obj(bm, name, smooth=False)
    ribs = []
    for k, (ex, ez) in enumerate([]):  # finger ribs dropped: budget went to the eyes
        pts = []
        for s in range(3):
            u = s / 2
            x, z = ex * u, 0.04 * (1 - u) + ez * u + 0.02 * math.sin(u * math.pi)
            lift = 0.35 * x * x / 0.235 + 0.02 * x
            pts.append((x * S * sx, -t - 0.003, z + lift))
        ribs.append(sweep(f"rib{k}", pts, [0.008, 0.006, 0.003], color="ink", mat="flat", sides=3,
                          smooth=True, caps=(False, False)))
    return K.merge([ob] + ribs, name)


def bat():
    K.reset()
    C = "charcoal"
    body = blob("bat", 0.1, C, mat="felt", scale=(1.0, 0.92, 1.08), segments=12, rings=6, lobes=0, noise_amp=0.0,
                axis="Z")
    paint_world(body, "cat_black", lambda c, n: n.y < -0.4 and c.z < -0.02)
    parts = [body]
    for sx in (-1, 1):
        ear = lathe(f"ear{sx}", [(0.035, 0.0), (0.024, 0.04), (0.0, 0.075)], C, "felt", segments=5,
                    loc=(0.05 * sx, 0.0, 0.07), rot=(0, 20 * sx, 0), scale=(1.0, 0.55, 1.0), cap_bottom=False)
        inner = lathe(f"inner{sx}", [(0.022, 0.0), (0.0, 0.055)], "cheek_pink", "flat", segments=4,
                      loc=(0.05 * sx, -0.012, 0.075), rot=(0, 20 * sx, 0), scale=(1.0, 0.35, 1.0), cap_bottom=False)
        eye = blob(f"eye{sx}", 0.03, "eye_white", mat="flat", loc=(0.038 * sx, -0.083, 0.02), scale=(1.0, 0.55, 1.15),
                   segments=8, rings=4, noise_amp=0.0, axis="Y")
        pupil = blob(f"pupil{sx}", 0.018, "ink", mat="flat", loc=(0.036 * sx, -0.097, 0.018), scale=(1.0, 0.5, 1.2),
                     segments=6, rings=3, noise_amp=0.0, axis="Y")
        glint = blob(f"glint{sx}", 0.008, "eye_white", mat="flat", loc=(0.029 * sx, -0.109, 0.03),
                     segments=3, rings=2, noise_amp=0.0, axis="Y")
        fang = lathe(f"fang{sx}", [(0.007, 0.0), (0.0, -0.016)], "eye_white", "flat", segments=3,
                     loc=(0.014 * sx, -0.09, -0.035), cap_bottom=False)
        parts += [ear, inner, eye, pupil, glint, fang]
    body = K.merge(parts, "bat")
    for side, sx in (("wing_l", -1), ("wing_r", 1)):
        w = wing_mesh(side, sx)
        w.location = (0.075 * sx, 0.0, 0.01)
        parent_keep(w, body)
    return finish([body], "sky/bat", "bat", ao_radius=0.08, ao_strength=0.6, ground_ao=False)


if want("cloud_a"):
    cloud("cloud_a", CLOUD_A, 0.9, 101)
if want("cloud_b"):
    cloud("cloud_b", CLOUD_B, 0.95, 202, res=1.15)
if want("cloud_c"):
    cloud("cloud_c", CLOUD_C, 0.85, 303, res=0.93)
if want("bat"):
    bat()
