"""Sky: cloud_a, cloud_b, cloud_c (flat-bottomed storm puffs) and bat (flappable wings)."""
import math
import pathlib
import sys

sys.path.insert(0, str(pathlib.Path(__file__).resolve().parent))
from yard_common import K, cull_hidden, finish, lathe, mesh, paint_where_world, parent_keep, puff, want  # noqa: E402

TOP, BELLY = "roof_slate", "shadow_navy"


def cloud(asset_id, layout, depth, seed, seg_big=10):
    K.reset()
    blobs = []
    for i, (x, y, z, r) in enumerate(layout):
        b = puff(f"puff{i}", r, TOP, loc=(x, y, z), scale=(1.0, depth, 0.85),
                 segments=seg_big if r >= 2.0 else (8 if r >= 1.6 else 7), bands=3 if r >= 1.6 else 2,
                 lump=0.05, seed=seed + i * 5, base_color=BELLY)
        paint_where_world(b, BELLY, lambda c, n: n.z < -0.3 or c.z < 0.35)
        blobs.append(b)
    cull_hidden(blobs)
    ob = K.merge(blobs, asset_id)
    # origin at the centre of the cloud mass
    xs = [v.co.x for v in ob.data.vertices]
    zs = [v.co.z for v in ob.data.vertices]
    cx, cz = (min(xs) + max(xs)) / 2, (min(zs) + max(zs)) / 2
    for v in ob.data.vertices:
        v.co.x -= cx
        v.co.z -= cz
    return finish([ob], f"sky/{asset_id}", asset_id)


CLOUD_A = [(-5.2, 0.0, 0.5, 1.9), (-2.6, 0.3, 0.9, 2.7), (0.6, -0.2, 1.5, 3.2), (3.8, 0.2, 0.8, 2.5),
           (5.6, -0.1, 0.4, 1.6), (-0.6, 1.3, 0.5, 2.2)]
CLOUD_B = [(-3.3, 0.0, 0.5, 1.7), (-1.1, 0.0, 1.0, 2.6), (1.7, 0.1, 0.8, 2.4), (3.5, -0.1, 0.4, 1.5),
           (0.3, -0.9, 0.4, 1.9), (0.2, 0.3, 2.0, 1.6)]
CLOUD_C = [(-7.5, 0.1, 0.2, 1.65), (-4.8, -0.2, 0.9, 2.6), (-1.5, 0.2, 1.5, 3.1), (1.9, -0.25, 0.6, 2.4),
           (4.6, 0.15, 1.2, 2.8), (7.6, -0.1, 0.3, 1.7)]


def bat():
    K.reset()
    C = "charcoal"
    body = K.sphere("bat", 0.09, color=C, scale=(1.0, 0.9, 1.1), segments=6, rings=4)
    extra = []
    for sx in (-1, 1):
        extra.append(lathe(f"ear{sx}", [(0.03, 0.0), (0.0, 0.065)], C, segments=3, loc=(0.045 * sx, 0.0, 0.075),
                           rot=(0, 18 * sx, 0), cap_bottom=False))
        extra.append(lathe(f"eye{sx}", [(0.0, -0.012), (0.026, 0.0), (0.0, 0.006)], "eye_white", segments=4, phase=0.785,
                           loc=(0.035 * sx, -0.074, 0.02), rot=(90, 0, 0)))
        extra.append(lathe(f"pupil{sx}", [(0.0, -0.006), (0.012, 0.0), (0.0, 0.004)], "ink", segments=3,
                           loc=(0.035 * sx, -0.088, 0.018), rot=(90, 0, 0)))
    body = K.merge([body] + extra, "bat")
    # wing outline (x outward from the shoulder, z up), two scallops on the trailing edge
    outline = [(0.0, 0.04), (0.11, 0.1), (0.22, 0.04), (0.17, -0.04), (0.125, -0.005), (0.06, -0.055), (0.0, -0.03)]
    t = 0.008
    for side, sx in (("wing_l", -1), ("wing_r", 1)):
        verts = []
        for y in (-t, t):
            for x, z in outline:
                lift = x * 0.25  # slight dihedral
                verts.append((x * 0.82 * sx, y, z + lift))  # 0.82: ~0.5 m tip to tip
        n = len(outline)
        faces = [list(range(n)), list(range(2 * n - 1, n - 1, -1))]
        for i in range(n):
            j = (i + 1) % n
            faces.append([i, j, n + j, n + i])
        w = mesh(side, verts, faces, C)
        w.location = (0.07 * sx, 0.0, 0.01)
        parent_keep(w, body)
    return finish([body], "sky/bat", "bat")


if want("cloud_a"):
    cloud("cloud_a", CLOUD_A, 0.75, 101)
if want("cloud_b"):
    cloud("cloud_b", CLOUD_B, 0.85, 202)
if want("cloud_c"):
    cloud("cloud_c", CLOUD_C, 0.7, 303, seg_big=9)
if want("bat"):
    bat()
