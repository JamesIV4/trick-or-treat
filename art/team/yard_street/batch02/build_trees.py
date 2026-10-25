"""Storybook maples (v2): tree_maple_red, tree_maple_orange, tree_maple_gold.

Curved bark trunk with a five-root flare, a strong low branch carrying socket_branch, a few upper
branches into the crown, and a lush canopy of many soft leafy clusters (smooth lumpy blobs with
puffy sub-lobes, leaves material, mixed tints, darker undersides). Buried cluster faces are culled.
"""
import pathlib
import random
import sys

sys.path.insert(0, str(pathlib.Path(__file__).resolve().parent))
from yard_common import *  # noqa: E402,F401,F403

UNDER = {"maple_red": "roof_red", "maple_orange": "rust_leaf", "golden_leaf": "burnt_amber",
         "rust_leaf": "brick_dark", "amber": "burnt_amber", "pumpkin": "pumpkin_dark", "apple_red": "roof_red",
         "pumpkin_light": "burnt_amber", "burnt_amber": "rust", "candy_corn_yellow": "straw_dark"}


def trunk_line(z, height, lean, seed):
    s = height / 5.0
    x = lean * (0.13 * math.sin(z / s * 1.05) - 0.035 * (z / s) ** 1.6)
    y = 0.06 * math.sin(z / s * 1.7 + seed)
    return Vector((x, y, z))


def build_trunk(height, lean, seed, top_z):
    s = height / 5.0
    zs = [0.0, 0.05, 0.13, 0.24, 0.4, 0.62, 0.9, 1.25, 1.65, 2.05, 2.45, 2.85, 3.25, top_z]
    zs = [z if i < 6 else z * s for i, z in enumerate(zs)]
    zs[-1] = top_z
    pts = [trunk_line(z, height, lean, seed) for z in zs]

    def r_of(z):
        return 0.28 * (1.0 - 0.5 * min(z / top_z, 1.0)) + 0.13 * math.exp(-z / 0.3)

    radii = [r_of(z) for z in zs]
    ph = seed * 0.37

    def shape(i, a):
        z = zs[i]
        flare = math.exp(-z / 0.2)
        root = (0.5 + 0.5 * math.cos(5 * (a + ph) + 0.4 * math.sin(3 * a))) ** 2.2
        knob = 0.05 * nz(math.cos(a) * 1.3, math.sin(a) * 1.3, z * 1.4, seed)
        return 1.0 + flare * 0.62 * root + knob

    tr = sweep("trunk", pts, radii, color="bark", mat="bark", sides=14, caps=(False, True), shape=shape,
               v_scale=1.0)
    for v in tr.data.vertices:  # tilted base ring: keep everything on/above the lawn
        v.co.z = max(v.co.z, 0.0)
    return tr, pts


def branch(name, ctrl, r0, r1, sides=8, per=2):
    pts = catmull(ctrl, per=per)
    n = len(pts)
    radii = [r0 + (r1 - r0) * (i / (n - 1)) ** 0.8 for i in range(n)]
    return sweep(name, pts, radii, color="bark", mat="bark", sides=sides, caps=(False, True))


def fib_dirs(n, seed):
    rnd = random.Random(seed)
    out = []
    g = math.pi * (3 - math.sqrt(5))
    for i in range(n):
        z = 1 - 2 * (i + 0.5) / n
        r = math.sqrt(max(0.0, 1 - z * z))
        a = g * i + rnd.uniform(-0.25, 0.25)
        out.append(Vector((math.cos(a) * r, math.sin(a) * r, z)))
    return out


def canopy(cfg, seed):
    rnd = random.Random(seed)
    c = Vector(cfg["center"])
    rx, ry, rz = cfg["radii"]
    blobs = []
    tints = cfg["tints"]
    dirs = [d for d in fib_dirs(cfg["n"], seed) if d.z > cfg.get("min_z", -0.5)]
    for k, d in enumerate(dirs):
        # keep the low branch (socket side, +X low) clear of foliage
        p = c + Vector((d.x * rx, d.y * ry, d.z * rz)) * cfg.get("inset", 0.74)
        size = cfg["cluster"] * rnd.uniform(0.8, 1.15) * (1.0 + 0.15 * d.z)
        if p.x > 0.3 and abs(p.y) < 1.0:
            # keep the low branch (socket_branch at x=1, z=2.6) visible under the crown
            p.z = max(p.z, 3.02 + size * 0.82 * 1.15 * min(1.0, (p.x - 0.3) / 0.5))
        col = rnd.choices([t for t, _ in tints], [w for _, w in tints])[0]
        if d.z > 0.55 and rnd.random() < 0.4:
            col = cfg.get("top", col)
        if col in cfg.get("accent", ()):
            size *= 0.72
        b = blob(f"leaf{k}", size, col, mat="leaves", loc=tuple(p), scale=(1.0, 1.0, 0.82),
                 segments=cfg.get("seg", 14), rings=cfg.get("rings", 9), lobes=7, lobe_amp=0.3, lobe_pow=3.0,
                 noise_amp=0.06, noise_freq=1.4, seed=seed + k * 3, uvmode="box")
        blobs.append((b, col))
    # filler core so no gaps show between clusters
    core = blob("core", 1.0, tints[0][0], mat="leaves", loc=tuple(c), scale=(rx * 0.86, ry * 0.86, rz * 0.82),
                segments=12, rings=8, lobes=0, noise_amp=0.03, seed=seed + 99, uvmode="box")
    blobs.append((core, tints[0][0]))
    for extra in cfg.get("extra", []):
        r, loc, col = extra
        b = blob(f"tip{len(blobs)}", r, col, mat="leaves", loc=loc, scale=(1.0, 1.0, 0.85), segments=10, rings=7,
                 lobes=4, lobe_amp=0.2, noise_amp=0.06, seed=seed + len(blobs) * 5, uvmode="box")
        blobs.append((b, col))
    cull_hidden([b for b, _ in blobs])
    # only the filler core is darker (it shows in the gaps between clusters as depth); the clusters
    # themselves stay one tint each so no hard stair-step tint lines cross their smooth surfaces
    paint_world(core, UNDER.get(tints[0][0], "rust_leaf"))
    return [b for b, _ in blobs]


def build(asset_id, height, lean, seed, cfg, low_branch, uppers):
    K.reset()
    top_z = cfg["center"][2] + 0.1
    tr, pts = build_trunk(height, lean, seed, top_z)
    parts = [tr]
    parts.append(branch("branch_low", low_branch, 0.15, 0.065, sides=9, per=3))
    for i, ctrl in enumerate(uppers):
        parts.append(branch(f"branch_up{i}", ctrl, 0.14, 0.05, sides=8, per=2))
    for p in parts:
        paint_world(p, "bark_dark", lambda c, n: n.z < -0.6 and c.z > 0.5)
    parts += canopy(cfg, seed)
    tree = K.merge(parts, asset_id)
    socket("branch", (1.0, -0.04, 2.6), tree)
    return finish([tree], f"props/{asset_id}", asset_id, ao_radius=1.0, ao_strength=0.85)


def low_branch_ctrl(lean):
    x0 = 0.13 * lean * math.sin(2.0 * 1.05) - 0.035 * lean * 2.0 ** 1.6
    return [(x0, 0.0, 1.95), (0.45, -0.02, 2.42), (1.0, -0.04, 2.71), (1.5, -0.05, 2.98), (1.85, -0.04, 3.42)]


RED = dict(center=(0.0, 0.0, 3.8), radii=(2.05, 1.85, 1.2), n=32, cluster=0.7, inset=0.7, clear_z=3.3,
           tints=[("maple_red", 7), ("apple_red", 3), ("maple_orange", 1)], top="maple_orange", accent=("maple_orange",),
           extra=[(0.42, (1.95, -0.05, 3.55), "maple_orange")])
ORANGE = dict(center=(0.0, 0.0, 3.6), radii=(2.0, 1.95, 1.08), n=32, cluster=0.7, inset=0.7, clear_z=3.2,
              tints=[("maple_orange", 7), ("pumpkin", 3), ("golden_leaf", 1)], top="golden_leaf",
              extra=[(0.4, (1.95, -0.05, 3.5), "golden_leaf")])
GOLD = dict(center=(0.0, 0.0, 4.1), radii=(1.8, 1.7, 1.5), n=32, cluster=0.68, inset=0.7, clear_z=3.3,
            min_z=-0.55, tints=[("golden_leaf", 7), ("amber", 3), ("rust_leaf", 2)], top="candy_corn_yellow", accent=("rust_leaf",),
            extra=[(0.4, (1.92, -0.05, 3.55), "rust_leaf")])

if want("tree_maple_red"):
    build("tree_maple_red", 5.0, 1.0, 11, RED, low_branch_ctrl(1.0), [
        [(0.02, 0, 2.15), (-0.55, 0.15, 2.8), (-1.15, 0.3, 3.35)],
        [(-0.02, 0, 2.4), (0.15, -0.55, 3.05), (0.3, -1.05, 3.6)],
        [(-0.04, 0, 2.5), (0.25, 0.6, 3.15), (0.5, 1.05, 3.65)],
    ])
if want("tree_maple_orange"):
    build("tree_maple_orange", 4.5, -0.8, 23, ORANGE, low_branch_ctrl(-0.8), [
        [(0.05, 0, 2.05), (-0.6, 0.2, 2.65), (-1.15, 0.35, 3.15)],
        [(0.05, 0, 2.25), (0.15, -0.6, 2.85), (0.3, -1.05, 3.35)],
        [(0.05, 0, 2.35), (0.25, 0.6, 2.95), (0.45, 1.05, 3.4)],
    ])
if want("tree_maple_gold"):
    build("tree_maple_gold", 5.5, 0.6, 37, GOLD, low_branch_ctrl(0.6), [
        [(0.02, 0, 2.3), (-0.5, 0.15, 3.0), (-1.0, 0.3, 3.6)],
        [(0.0, 0, 2.6), (0.15, -0.5, 3.3), (0.3, -0.95, 3.95)],
        [(0.0, 0, 2.7), (0.25, 0.5, 3.4), (0.45, 0.95, 4.0)],
    ])
