"""Autumn maples (batch03 v3): tree_maple_red, tree_maple_orange, tree_maple_gold.

Crown FORM follows the batch02 v2 trees (user preference): a big, full, rounded crown made of ~20
overlapping lobes spread over a whole ellipsoid (directions down to z = -0.55), so lobes bulge out and
down and the crown bottom sits low on a short visible trunk. Same centres / radii / inset / cluster
sizes / low-branch rule as batch02's build_trees.py.

Crown LOOK is batch03's leaf cards:
  * outer skin: ~650 small (0.3 m) maple cards placed by area over the *exposed* surface of the lobe
    union (undersides and lower flanks included), serrated 3+2-lobe outline on the silhouette,
    cheaper 10-point outline face-on; single-sided unless poking out
  * inner shell: ~230 larger dark cards a little deeper, for the leafy dark volume
  * filler core with a downward "shadow only" normal so the sky never shows through
  * custom normals = lobe normal blended with crown normal; analytic AO per leaf (lobe tops light,
    undersides and crevices dark, crown base darker) and a 4-step tint ramp per tree.
Dark curvy bark trunk with root flare and a fork whose limbs disappear into the crown, plus the low
strong branch carrying socket_branch (1.0, -0.04, 2.6), kept clear of foliage for swings / lights.
"""
import pathlib
import random
import sys

sys.path.insert(0, str(pathlib.Path(__file__).resolve().parent))
from foliage_common import *  # noqa: E402,F401,F403
import foliage_common as F  # noqa: E402

V = Vector
SOCKET = (1.0, -0.04, 2.6)


def low_branch_ctrl(lean):
    x0 = 0.13 * lean * math.sin(2.0 * 1.05) - 0.035 * lean * 2.0 ** 1.6
    return [(x0, 0.0, 1.95), (0.45, -0.02, 2.42), (1.0, -0.04, 2.71), (1.5, -0.05, 2.98), (1.85, -0.04, 3.42)]


def trunk_line(z, lean, seed):
    x = lean * (0.13 * math.sin(z * 1.05) - 0.035 * z ** 1.6) + 0.05 * math.sin(z * 2.6 + seed)
    y = 0.07 * math.sin(z * 1.7 + seed)
    return V((x, y, z))


def build_trunk(fork_z, lean, seed):
    zs = [0.0, 0.05, 0.13, 0.24, 0.4, 0.62, 0.9, 1.2, 1.5, 1.8, 2.1]
    zs = [z for z in zs if z < fork_z - 0.15] + [fork_z, fork_z + 0.3]
    pts = [trunk_line(z, lean, seed) for z in zs]

    def r_of(z):
        return 0.26 * (1.0 - 0.28 * min(z / fork_z, 1.0)) + 0.12 * math.exp(-z / 0.3)

    radii = [r_of(z) for z in zs]
    ph = seed * 0.37

    def shape(i, a):
        z = zs[i]
        flare = math.exp(-z / 0.2)
        root = (0.5 + 0.5 * math.cos(5 * (a + ph) + 0.4 * math.sin(3 * a))) ** 2.2
        knob = 0.07 * nz(math.cos(a) * 1.3, math.sin(a) * 1.3, z * 1.4, seed)
        twist = 0.05 * math.cos(2 * a + z * 1.5)
        return 1.0 + flare * 0.62 * root + knob + twist

    tr = sweep("trunk", pts, radii, color="bark_dark", mat="bark", sides=11, caps=(False, True), shape=shape)
    for v in tr.data.vertices:
        v.co.z = max(v.co.z, 0.0)
    return tr, pts[-2]


def limb(name, ctrl, r0, r1, sides=7, per=2, color="bark_dark"):
    pts = catmull(ctrl, per=per)
    n = len(pts)
    radii = [r0 + (r1 - r0) * (i / (n - 1)) ** 0.7 for i in range(n)]
    return sweep(name, pts, radii, color=color, mat="bark", sides=sides, caps=(False, True))


def in_clearance(p, pad=0.0):
    """Region around the low branch and the swing / light string hanging from socket_branch."""
    return (p.x > 0.35 - pad and abs(p.y) < 0.75 + pad and p.z < 3.0 + pad) or \
           (0.55 - pad < p.x < 1.45 + pad and abs(p.y) < 0.6 + pad and p.z < 3.2 + pad)


def make_lobes(cfg, rnd):
    """batch02 layout: fibonacci directions over the crown ellipsoid (z > min_z), centre at
    c + d * R * inset, size = cluster * U(0.8, 1.15) * (1 + 0.15 d.z), z squashed 0.82; lobes over
    the low branch are lifted exactly like batch02 did."""
    c = V(cfg["center"])
    R = V(cfg["radii"])
    dirs = [d for d in F.fib_dirs(cfg["n"], random.Random(cfg["seed"]), jitter=0.25) if d.z > cfg.get("min_z", -0.5)]
    out = []
    for d in dirs:
        p = c + V((d.x * R.x, d.y * R.y, d.z * R.z)) * cfg.get("inset", 0.7)
        size = cfg["cluster"] * rnd.uniform(0.8, 1.15) * (1.0 + 0.15 * d.z) * cfg.get("lobe_k", 1.22)
        if p.x > 0.3 and abs(p.y) < 1.0:
            p.z = max(p.z, 3.02 + size * 0.82 * min(1.0, (p.x - 0.3) / 0.5))
        out.append((p, V((size, size, size * 0.85))))
    for p, r in cfg.get("extra", []):
        out.append((V(p), V(r)))
    return out


def ell_dist(p, mass):
    c, r = mass
    d = p - c
    return math.sqrt((d.x / r.x) ** 2 + (d.y / r.y) ** 2 + (d.z / r.z) ** 2)


def surface_pool(lobes, rnd, density, kmin, kmax, cull_k, clear_pad, min_dz=-1.0):
    """Candidate points on the exposed surface of the lobe union, about `density` per m^2."""
    pool = []
    for i, (mp, mr) in enumerate(lobes):
        area = 4 * math.pi * ((mr.x * mr.y) ** 1.6 + (mr.x * mr.z) ** 1.6 + (mr.y * mr.z) ** 1.6) ** (1 / 1.6) / 3 ** (1 / 1.6)
        n = max(4, int(area * density))
        others = [m for j, m in enumerate(lobes) if j != i]
        for d in F.fib_dirs(n, rnd, jitter=0.5):
            if d.z < min_dz:
                continue
            k = rnd.uniform(kmin, kmax)
            p = mp + V((d.x * mr.x, d.y * mr.y, d.z * mr.z)) * k
            if any(ell_dist(p, m) < cull_k for m in others) or in_clearance(p, clear_pad):
                continue
            mn = V((d.x / mr.x, d.y / mr.y, d.z / mr.z)).normalized()
            crev = min([ell_dist(p, m) for m in others] + [2.0])
            pool.append((p, mn, k, crev, i))
    rnd.shuffle(pool)
    return pool


def build(asset_id, cfg):
    K.reset()
    rnd = random.Random(cfg["seed"] * 7 + 1)
    lean, seed = cfg["lean"], cfg["seed"]
    tr, fork = build_trunk(cfg["fork"], lean, seed)
    parts = [tr, limb("branch_low", low_branch_ctrl(lean), 0.14, 0.06, sides=8, per=3)]
    lobes = make_lobes(cfg, rnd)
    c = V(cfg["center"])
    R = V(cfg["radii"])
    # a few limbs from the fork up into the crown (ends hidden inside lobes)
    big = sorted(lobes, key=lambda m: -(m[1].x))[:cfg.get("limbs", 5)]
    for i, (mp, mr) in enumerate(big):
        d = mp - fork
        side = V((d.x, d.y, 0)).normalized() if V((d.x, d.y, 0)).length > 1e-3 else V((1, 0, 0))
        ctrl = [fork + V((0, 0, -0.25)), fork + side * 0.25 + V((0, 0, 0.35)), fork + d * 0.6 + V((0, 0, 0.1)), mp]
        parts.append(limb(f"limb{i}", ctrl, 0.12, 0.035, sides=6))
    # filler core: shadow only (downward normal, dark AO) so the sky never shows through
    fc, fr = c - V((0, 0, R.z * 0.12)), V((R.x * 0.6, R.y * 0.6, R.z * 0.62))
    parts.append(blob("filler", 1.0, cfg["core"], mat="leaves", loc=tuple(fc), scale=tuple(fr), segments=10, rings=6,
                      lobes=0, noise_amp=0.03, seed=seed + 99, uvmode="box"))
    tree = F.merged_base(parts, asset_id)
    base_tris = K.tri_count([tree])
    acc = F.Acc(tree)
    acc.shade_existing(lambda co: ell_dist(co, (fc, fr)) < 1.2, (0.0, 0.0, -1.0), 0.35)

    top = max(mp.z + mr.z for mp, mr in lobes)
    bot = min(mp.z - mr.z for mp, mr in lobes)
    tints = cfg["tints"]
    count = {}
    m_off = [rnd.gauss(0, 0.06) for _ in lobes]

    def crown_n(p):
        cd = p - c
        return V((cd.x / R.x ** 2, cd.y / R.y ** 2, cd.z / R.z ** 2)).normalized()

    # inner shell: larger dark cards a little below the skin
    for p, mn, k, crev, i in surface_pool(lobes, rnd, cfg.get("inner_density", 6.0), 0.74, 0.86, 0.8, 0.05)[:cfg["inner"]]:
        col = rnd.choice(tints["deep"]) if mn.z < 0.45 else rnd.choice(tints["dark"])
        acc.leaf(p, (mn + F.rand_unit(rnd) * 0.35).normalized(), F.rand_unit(rnd) + V((0, 0, -0.3)),
                 cfg["leaf"] * 1.9 * rnd.uniform(0.85, 1.15), col, shade=(mn * 0.7 + crown_n(p) * 0.3).normalized(),
                 outline=F.maple_simple(rnd, 1.0), cup=0.1, double=False, ao=0.36 + 0.14 * max(mn.z, 0.0))
        count[col] = count.get(col, 0) + 1

    # outer skin: small leaves over the whole exposed surface, undersides included
    placed = doubles = 0
    for p, mn, k, crev, i in surface_pool(lobes, rnd, cfg.get("density", 24.0), 0.92, 1.03, 0.92, 0.1)[:cfg["leaves"]]:
        cn = crown_n(p)
        h01 = (p.z - bot) / max(top - bot, 0.1)
        cv = max(0.0, min(1.0, (1.3 - crev) / 0.4))
        e = 0.62 * mn.z + 0.5 * (h01 - 0.5) + m_off[i] + rnd.gauss(0, 0.05) - 0.4 * cv
        if e < cfg.get("deep_t", -0.5):
            col = rnd.choice(tints["deep"])
        elif e < cfg.get("dark_t", 0.05):
            col = rnd.choice(tints["dark"])
        elif e > cfg.get("light_t", 0.6):
            col = rnd.choice(tints["light"])
        else:
            col = rnd.choice(tints["main"])
        if rnd.random() < cfg.get("accent_p", 0.03):
            col = rnd.choice(tints["accent"])
        count[col] = count.get(col, 0) + 1
        poke = rnd.random() < 0.06
        if poke:
            p = p + mn * cfg["leaf"] * 0.35
        pn = (mn + F.rand_unit(rnd) * (0.8 if poke else 0.28)).normalized()
        tip = F.rand_unit(rnd) * 0.8 + V((0, 0, -0.35)) + mn * (0.9 if poke else 0.2)
        sh = (mn * 0.82 + cn * 0.18 + F.rand_unit(rnd) * 0.05).normalized()
        # undersides stay visibly leafy: AO floor ~0.45 there, tops up to 1
        leaf_ao = (0.45 + 0.3 * (mn.z * 0.5 + 0.5) + 0.25 * h01) * (1.0 - 0.4 * cv)
        size = cfg["leaf"] * rnd.uniform(0.84, 1.16)
        double = poke or pn.dot(mn) < 0.78
        silhouette = poke or abs(mn.z) < 0.22
        acc.leaf(p, pn, tip, size, col, mat="flat", shade=sh,
                 outline=F.maple_serrated(rnd, 1.0) if silhouette else F.maple_simple(rnd, 1.0),
                 cup=0.14, double=double, ao=max(0.32, min(1.0, leaf_ao)))
        doubles += int(double)
        placed += 1
    tree, shade = acc.to_object()

    def extra(p, ao, is_leaf):
        return ao if is_leaf or p.z < cfg["fork"] + 0.6 else ao * 0.6

    socket("branch", SOCKET, tree)
    tris = F.finish_foliage(tree, shade, f"props/{asset_id}", asset_id, ao_radius=0.9, ao_strength=0.75,
                            extra=extra, fixed_ao=acc.fixed_ao)
    print(f"INFO {asset_id} base_tris={base_tris} lobes={len(lobes)} leaves={placed} doubles={doubles} "
          f"top={top:.2f} bot={bot:.2f} tints={count}")
    return tris


# centre / radii / cluster / inset / min_z are batch02 v2's (build_trees.py); n lower so lobes stay
# distinct under small leaves (each lobe a touch bigger to keep the same envelope)
RED = dict(seed=11, lean=1.0, fork=2.25, center=(0.0, 0.0, 3.8), radii=(2.05, 1.85, 1.2), n=26, cluster=0.7,
           inset=0.7, leaves=725, inner=215, leaf=0.34, core="leaf_maroon",
           tints=dict(deep=["leaf_maroon"], dark=["leaf_maroon", "leaf_crimson"],
                      main=["leaf_crimson", "leaf_crimson", "maple_red"], light=["maple_red", "leaf_scarlet"],
                      accent=["maple_orange"]),
           extra=[((1.95, -0.05, 3.55), (0.47, 0.47, 0.4))])
ORANGE = dict(seed=23, lean=-0.8, fork=2.1, center=(0.0, 0.0, 3.6), radii=(2.0, 1.95, 1.08), n=26, cluster=0.7,
              inset=0.7, leaves=725, inner=215, leaf=0.34, core="leaf_umber",
              tints=dict(deep=["leaf_umber"], dark=["rust_leaf"], main=["burnt_amber", "maple_orange", "maple_orange"],
                         light=["maple_orange", "leaf_tangerine"], accent=["maple_red"]),
              extra=[((1.95, -0.05, 3.5), (0.45, 0.45, 0.38))])
GOLD = dict(seed=37, lean=0.6, fork=2.45, center=(0.0, 0.0, 4.1), radii=(1.8, 1.7, 1.5), n=26, cluster=0.68,
            inset=0.7, min_z=-0.55, leaves=715, inner=205, leaf=0.33, core="leaf_ochre",
            tints=dict(deep=["leaf_ochre"], dark=["burnt_amber"], main=["amber", "amber", "golden_leaf"],
                       light=["golden_leaf"], accent=["maple_orange", "rust_leaf"]), accent_p=0.04,
            extra=[((1.92, -0.05, 3.55), (0.45, 0.45, 0.38))])

if want("tree_maple_red"):
    build("tree_maple_red", RED)
if want("tree_maple_orange"):
    build("tree_maple_orange", ORANGE)
if want("tree_maple_gold"):
    build("tree_maple_gold", GOLD)
