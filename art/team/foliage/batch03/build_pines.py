"""Woods conifers (batch03): tree_pine_a (~7.5 m), tree_pine_b (~9.5 m).

Short visible dark trunk under a stack of soft drooping tiers. Each tier is a closed skirt: a convex
drooping upper surface whose rim breaks into a jagged fringe of needle clumps (long drooping spikes
and shallow notches, with ridges running in from each spike), and a shallower underside. Upper
surfaces go pine_deep near the trunk -> pine_mid -> pine / pine_tip at the fringe. Every tier
vertex carries a custom normal that blends the tier's outward slope with the whole tree's cone, so
the tiers shade softly instead of as flat paper facets.
"""
import pathlib
import random
import sys

sys.path.insert(0, str(pathlib.Path(__file__).resolve().parent))
from foliage_common import *  # noqa: E402,F401,F403
import foliage_common as F  # noqa: E402

V = Vector
UP = V((0, 0, 1))


def tier(acc, cfg, center, zt, R, drop, spikes, rnd, apex_z=None, tip_cols=("pine_tip", "pine_mid"), lean=V((0, 0, 0))):
    bm = acc.bm
    proj, tint = acc.proj, acc.tint
    M = spikes
    N = 2 * M
    ph = rnd.uniform(0, math.tau)
    # low-frequency outline wobble so tiers are not perfect circles
    w1, w2 = rnd.uniform(0, math.tau), rnd.uniform(0, math.tau)

    def wob(a):
        return 1.0 + 0.09 * math.sin(2 * a + w1) + 0.06 * math.sin(3 * a + w2)

    spike_len = [rnd.uniform(0.82, 1.0) if m % 2 else rnd.uniform(0.95, 1.18) for m in range(M)]
    clump_n = [V((rnd.uniform(-1, 1), rnd.uniform(-1, 1), rnd.uniform(-0.6, 1.0))) * cfg.get("clump_jit", 0.3) for _ in range(M)]
    angles = []
    for j in range(N):
        a = ph + math.tau * j / N + (rnd.uniform(-0.06, 0.06) if j % 2 == 0 else 0.0)
        angles.append(a)
    # radial fractions of the upper rings; the last is the fringe
    fr = cfg.get("rings", [0.28, 0.55, 0.78, 0.92, 1.0])
    expo = cfg.get("droop_pow", 1.5)
    cols_top = cfg.get("cols", ["pine_deep", "pine_deep", "pine_mid", "pine_mid", None])
    rings = []
    shade_tree = []
    for k, f in enumerate(fr):
        ring = []
        for j in range(N):
            a = angles[j]
            spike = j % 2 == 0
            amp = (f - 0.55) / 0.45 if f > 0.55 else 0.0  # ridges grow toward the rim
            if spike:
                rr = f * (1.0 + amp * (spike_len[j // 2] - 1.0))
            else:
                rr = f * (1.0 - cfg.get("notch", 0.2) * amp ** 1.5)
            r = R * rr * wob(a)
            z = zt - drop * (min(rr, 1.15)) ** expo
            if spike and f >= 1.0:
                z -= drop * cfg.get("tip_droop", 0.12)
            if not spike and f >= 0.8:
                z += drop * 0.05 * amp
            d = V((math.cos(a), math.sin(a), 0))
            p = center + d * r + V((0, 0, z)) + lean * (z / 10.0)
            v = bm.verts.new(p)
            slope = (d * drop * expo / R + UP).normalized()  # outward-up normal of the drooping skirt
            tree_n = (d + UP * cfg.get("cone_up", 1.3)).normalized()
            jit = clump_n[j // 2] if spike else (clump_n[j // 2] + clump_n[(j // 2 + 1) % M]) * 0.5
            n = (slope * 0.45 + tree_n * 0.55 + jit * min(1.0, f / 0.6)).normalized()
            acc.shade[v] = n
            ring.append(v)
        rings.append(ring)
    apex = bm.verts.new(center + V((0, 0, apex_z if apex_z is not None else zt + drop * 0.04)) + lean * (zt / 10.0))
    acc.shade[apex] = UP.copy()
    # underside: one inner ring + hub
    u_in = []
    for j in range(N):
        a = angles[j]
        d = V((math.cos(a), math.sin(a), 0))
        r = R * 0.5 * wob(a)
        z = zt - drop * cfg.get("under_in", 0.78) - drop * 0.12
        v = bm.verts.new(center + d * r + V((0, 0, z)) + lean * (z / 10.0))
        acc.shade[v] = (d * 0.6 - UP * 0.4 + UP * 0.2).normalized()
        u_in.append(v)
    hub = bm.verts.new(center + V((0, 0, zt - drop * cfg.get("under_in", 0.78))) + lean * (zt / 10.0))
    acc.shade[hub] = -UP

    def face(vs, col, mat):
        f = bm.faces.new(vs)
        cell = acc._cell(col, mat)
        for loop in f.loops:
            loop[tint].uv = cell
            co = loop.vert.co
            loop[proj].uv = (co.x, co.y)
        f.smooth = True
        acc.new_faces.append(f)
        return f

    mat = cfg.get("mat", "hedge")
    # upper surface: apex fan + bands
    for j in range(N):
        j1 = (j + 1) % N
        face([apex, rings[0][j], rings[0][j1]], "pine_deep", mat)
    for k in range(len(rings) - 1):
        for j in range(N):
            j1 = (j + 1) % N
            if k == len(rings) - 2:  # fringe band: lighter tips on the spikes
                col = tip_cols[0] if (j % 2 == 0 and rnd.random() < cfg.get("tip_p", 0.7)) else tip_cols[1]
            else:
                col = cols_top[k + 1] if k + 1 < len(cols_top) - 1 else cols_top[k]
            face([rings[k][j], rings[k + 1][j], rings[k + 1][j1], rings[k][j1]], col, mat)
    # underside: fringe -> inner ring -> hub (faces point down/out)
    fringe = rings[-1]
    for j in range(N):
        j1 = (j + 1) % N
        face([fringe[j], u_in[j], u_in[j1], fringe[j1]], "pine_deep", mat)
        face([u_in[j], hub, u_in[j1]], "pine_deep", mat)


def build(asset_id, cfg):
    K.reset()
    rnd = random.Random(cfg["seed"])
    H = cfg["height"]
    lean = V(cfg.get("lean", (0, 0, 0)))
    # trunk: short visible base, hidden inside the tiers above
    tz = H * 0.8
    zs = [0.0, 0.06, 0.16, 0.3, 0.5, 0.8, 1.2, 2.0, 3.0, tz]
    pts = [V((0.03 * math.sin(z * 1.7 + cfg["seed"]), 0.03 * math.cos(z * 1.3), z)) + lean * (z / 10.0) for z in zs]
    radii = [cfg["trunk_r"] * (1.0 - 0.75 * z / tz) for z in zs]

    def shape(i, a):
        z = zs[i]
        return 1.0 + 0.5 * math.exp(-z / 0.18) * (0.5 + 0.5 * math.cos(5 * a + 1.0)) ** 2 + 0.04 * math.cos(3 * a + z)

    tr = sweep("trunk", pts, radii, color="bark_dark", mat="bark", sides=8, caps=(False, True), shape=shape)
    for v in tr.data.vertices:
        v.co.z = max(v.co.z, 0.0)
    base = F.merged_base([tr], asset_id)
    acc = F.Acc(base)
    n = cfg["tiers"]
    z0, z1 = cfg["z0"], H - cfg["top_gap"]
    for i in range(n):
        t = i / (n - 1)
        R = cfg["r0"] + (cfg["r1"] - cfg["r0"]) * t ** cfg.get("taper_pow", 0.95)
        R *= rnd.uniform(0.94, 1.06)
        zt = z0 + (z1 - z0) * t ** cfg.get("space_pow", 0.92)
        spacing = (z1 - z0) / (n - 1)
        drop = max(R * cfg.get("drop_k", 0.62), spacing * cfg.get("min_drop", 1.45))
        spikes = max(6, int(round(cfg["spikes0"] * (R / cfg["r0"]) ** 0.7)))
        off = V((rnd.uniform(-0.06, 0.06), rnd.uniform(-0.06, 0.06), 0))
        apex_z = H if i == n - 1 else None
        tier(acc, cfg, off, zt, R, drop, spikes, rnd, apex_z=apex_z, lean=lean, tip_cols=cfg.get("tips", ("pine_tip", "pine_mid")))
    tree, shade = acc.to_object()

    zmax = H

    def extra(p, ao, is_leaf):
        if not is_leaf:
            return ao
        return ao * (0.8 + 0.2 * max(0.0, min(1.0, p.z / zmax)))

    tris = F.finish_foliage(tree, shade, f"props/{asset_id}", asset_id, ao_radius=1.2, ao_strength=0.75,
                            extra=extra)
    print(f"INFO {asset_id} tiers={n} H={H}")
    return tris


PINE_A = dict(cols=["pine_deep", "pine", "hedge", "hedge", None], tips=("pine_tip", "hedge"), seed=5, height=7.5, tiers=7, z0=2.3, mat="grass", top_gap=0.75, r0=2.25, r1=0.42, spikes0=22, trunk_r=0.26,
              drop_k=0.6)
PINE_B = dict(cols=["pine_deep", "pine_mid", "pine", "hedge", None], tips=("pine_tip", "pine"), seed=9, height=9.5, tiers=9, z0=2.3, mat="grass", top_gap=0.8, r0=2.05, r1=0.38, spikes0=19, trunk_r=0.28,
              drop_k=0.58, lean=(0.18, 0.08, 0))

if want("tree_pine_a"):
    build("tree_pine_a", PINE_A)
if want("tree_pine_b"):
    build("tree_pine_b", PINE_B)
