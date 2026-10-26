"""mums_orange, mums_purple, mums_white: mounded autumn chrysanthemum bushes (~0.5 x 0.4 m).

blender --background --factory-startup --python art/team/decor_ground/batch03/build_mums.py [-- ids...]

A lumpy leafy mound (hedge material; upper half tinted the flower's shade colour so gaps between
heads still read as a flower mass), dense star-petalled flower heads tucked into the mound, and
pointed leaves fanning out round the base.
"""
import pathlib
import sys

sys.path.insert(0, str(pathlib.Path(__file__).resolve().parent))
from decor_common import *  # noqa: E402,F401,F403

VARIANTS = {
    # petals, gaps/shade, centre, mound top tint
    "mums_orange": dict(colors=(("mum_orange", "mum_orange", "pumpkin_light", "amber"), "pumpkin_dark", "cinnamon"),
                        top="hedge_dark", seed=3),
    "mums_purple": dict(colors=(("wrapper_purple", "wrapper_purple", "candy_grape", "banner_purple"), "witch_purple",
                                "candy_corn_yellow"), top="hedge_dark", seed=7),
    "mums_white": dict(colors=(("ghost_white", "ghost_white", "candy_corn_white", "cream"), "bone", "candy_corn_yellow"),
                       top="hedge_dark", seed=11),
}


def fib_dirs(n, zmin=-0.1, seed=0):
    rnd = random.Random(seed)
    out = []
    golden = math.pi * (3 - math.sqrt(5))
    for i in range(n):
        z = 1 - (i + 0.5) / n * (1 - zmin)
        r = math.sqrt(max(0.0, 1 - z * z))
        a = golden * i + rnd.uniform(-0.25, 0.25)
        out.append(Vector((math.cos(a) * r, math.sin(a) * r, z + rnd.uniform(-0.04, 0.04))))
    return out


def mound(name, W=0.5, H=0.4, top="hedge_dark", seed=0, cx=0.0, cy=0.0, scale=1.0, segs=10, rings=6):
    r = W * 0.5 * scale * 0.86
    H = H * 0.84
    zc = H * scale * 0.4
    ob = Y.blob(name, r, "hedge", mat="hedge", loc=(cx, cy, zc), scale=(1.0, 0.96, (H * scale - zc) / r * 0.92),
                segments=segs, rings=rings, lobes=7, lobe_amp=0.14, lobe_pow=3.0, noise_amp=0.05, noise_freq=2.2,
                seed=seed, axis="Z", uvmode="box")
    sit_on_ground(ob)
    # flower-shade crown with a ragged lower edge (noise), dark leafy skirt below
    Y.paint_world(ob, "hedge_dark", lambda c, n: c.z < 0.05)
    Y.paint_world(ob, top, lambda c, n: c.z > zc * (0.85 + 0.6 * Y.nz(c.x * 9, c.y * 9, c.z * 9, seed)) and n.z > -0.2)
    return ob, Vector((cx, cy, zc))


def mum_cluster(prefix, colors, top, seed, n_heads=30, W=0.5, H=0.4, head_r=0.066, n_leaves=8, spacing=1.32, loc=(0, 0, 0),
                scale=1.0, petals=12, segs=10, rings=6, zmin=-0.15):
    """Returns a list of objects (mound + heads/leaves mesh) placed at loc."""
    rnd = random.Random(seed)
    base, c = mound(f"{prefix}_mound", W, H, top, seed, loc[0], loc[1], scale, segs, rings)
    if loc[2]:
        Y.transform(base, loc=(0, 0, loc[2]))
        c = c + Vector((0, 0, loc[2]))
    # even coverage: dense candidate points on the mound (rays from the centre), then greedy
    # Poisson-disk picking (spacing ~1.75 R), visiting upward-facing candidates first so the crown
    # (what the 35 degree gameplay camera sees) fills before the flanks
    cands = surface_points(base, fib_dirs(500, zmin=zmin, seed=seed), c, far=1.0)
    cands.sort(key=lambda h: -(h[0].z + rnd.uniform(0, 0.03)))  # crown first, then down the flanks
    sp = head_r * scale * spacing
    hits = []
    for p, nrm in cands:
        if p.z < 0.07 * scale + loc[2]:
            continue
        if all((p - q).length > sp for q, _ in hits):
            hits.append((p, nrm))
        if len(hits) >= n_heads:
            break
    bm = F.bm_new()
    for i, (p, nrm) in enumerate(hits):
        n = (nrm * 0.6 + (p - c).normalized() * 0.4).normalized()
        R = head_r * scale * rnd.uniform(0.85, 1.12)
        main = rnd.choice(colors[0]) if isinstance(colors[0], tuple) else colors[0]
        flower_head(bm, p + n * R * 0.2, n, R, petals=petals, colors=(main, main, colors[2]),
                    spin=rnd.uniform(0, TAU), dome=rnd.uniform(0.5, 0.6), tuck=0.2, cup=0.06, mat="paper",
                    inner=0.3, notch=0.7, ring_h=0.85)
    for i in range(n_leaves):
        a = TAU * (i + rnd.uniform(-0.2, 0.2)) / n_leaves
        d = Vector((math.cos(a), math.sin(a), rnd.uniform(0.15, 0.3)))
        r0 = W * 0.5 * scale * 0.6
        b = Vector((loc[0] + math.cos(a) * r0, loc[1] + math.sin(a) * r0, loc[2] + rnd.uniform(0.05, 0.08) * scale))
        leaf(bm, b, d, 0.12 * scale * rnd.uniform(0.85, 1.15), 0.1 * scale, color="vine" if i % 3 else "hedge",
             under="hedge_dark", droop=0.35, mat="hedge")
    flowers = mesh_from(bm, f"{prefix}_heads", smooth=True, sharp=None)
    return [base, flowers]


def build(asset_id):
    K.reset()
    v = VARIANTS[asset_id]
    parts = mum_cluster(asset_id, v["colors"], v["top"], v["seed"])
    ob = K.merge(parts, asset_id)
    return finish([ob], f"props/{asset_id}", asset_id, ao_radius=0.3)


if __name__ == "__main__":
    for aid in VARIANTS:
        if want(aid):
            build(aid)
