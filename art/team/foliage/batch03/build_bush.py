"""bush_round (batch03): dense leafy shrub about 1.1 m wide and 0.85 m tall.

A few dark leafy core lumps (hedge_dark, hedge texture) sitting on the ground, covered by ~200 small
pointed leaf cards (hedge / vine / moss with lighter sage tips on top), whose custom normals blend
the lump and whole-bush normals so it shades as a soft mound while the silhouette breaks into leaves,
plus a sprinkle of tiny cream five-petal flowers.
"""
import pathlib
import random
import sys

sys.path.insert(0, str(pathlib.Path(__file__).resolve().parent))
from foliage_common import *  # noqa: E402,F401,F403
import foliage_common as F  # noqa: E402

V = Vector
UP = V((0, 0, 1))
LEAF = F._norm_outline([(0, -0.5), (0.25, -0.16), (0.21, 0.2), (0, 0.6), (-0.21, 0.2), (-0.25, -0.16)])
FLOWER = F._norm_outline([(math.cos(math.radians(90 + 36 * i)) * (0.5 if i % 2 == 0 else 0.22),
                           math.sin(math.radians(90 + 36 * i)) * (0.5 if i % 2 == 0 else 0.22)) for i in range(10)])

MASSES = [  # centre, radii
    ((0.0, 0.02, 0.36), (0.44, 0.42, 0.38)),
    ((0.3, -0.18, 0.26), (0.3, 0.3, 0.27)),
    ((-0.32, -0.08, 0.26), (0.3, 0.3, 0.27)),
    ((0.1, 0.3, 0.27), (0.28, 0.28, 0.26)),
    ((-0.22, 0.24, 0.27), (0.27, 0.27, 0.25)),
    ((0.04, 0.0, 0.6), (0.27, 0.27, 0.24)),
]


def inside(p, m, k=1.0):
    c, r = m
    d = p - V(c)
    return (d.x / r[0]) ** 2 + (d.y / r[1]) ** 2 + (d.z / r[2]) ** 2 < k * k


def bush_round(cfg):
    K.reset()
    rnd = random.Random(cfg["seed"])
    # no core volumes (smooth cores read as glossy balls or cobbles between the leaves): an inner
    # shell of dark leaves gives the depth, and one low squashed base hides the ground under the bush
    cores = [blob("base", 1.0, "pine_deep", mat="flat", loc=(0, 0.02, 0.1), scale=(0.48, 0.44, 0.2), segments=10,
                  rings=5, noise_amp=0.04, seed=cfg["seed"], uvmode="box"),
             blob("filler", 1.0, "pine_deep", mat="flat", loc=(0, 0.02, 0.34), scale=(0.42, 0.4, 0.3), segments=10,
                  rings=6, noise_amp=0.04, seed=cfg["seed"] + 1, uvmode="box")]
    for v in cores[0].data.vertices:
        v.co.z = max(v.co.z, 0.0)
    base = F.merged_base(cores, "bush_round")
    acc = F.Acc(base)
    acc.shade_existing(lambda co: True, (0.0, 0.0, -1.0), 0.3)
    C = V((0, 0, 0.3))
    RR = V((0.58, 0.55, 0.5))
    area = [r[0] * r[1] + r[0] * r[2] + r[1] * r[2] for _, r in MASSES]
    total = sum(area)
    n_leaves = cfg["leaves"]
    tints = cfg["tints"]
    placed = 0
    flowers = 0
    for i, (mc, mr) in enumerate(MASSES):
        mc, mr = V(mc), V(mr)
        want_n = int(round(n_leaves * area[i] / total))
        dirs = F.fib_dirs(int(want_n * 2.4) + 6, rnd, jitter=0.4)
        rnd.shuffle(dirs)
        others = [m for j, m in enumerate(MASSES) if j != i]
        got = 0
        for d in dirs:
            if got >= want_n:
                break
            if d.z < -0.65:
                continue
            inner = rnd.random() < cfg.get("inner_p", 0.33)
            if inner:
                k = rnd.uniform(0.66, 0.82)
            else:
                k = rnd.uniform(0.88, 1.02) if rnd.random() > 0.15 else rnd.uniform(1.02, 1.1)
            p = mc + V((d.x * mr.x, d.y * mr.y, d.z * mr.z)) * k
            if p.z < 0.05 or any(inside(p, m, 0.9) for m in others):
                continue
            mn = V((d.x / mr.x, d.y / mr.y, d.z / mr.z)).normalized()
            cd = p - C
            cn = V((cd.x / RR.x ** 2, cd.y / RR.y ** 2, cd.z / RR.z ** 2)).normalized()
            h01 = max(0.0, min(1.0, p.z / 0.85))
            e = 0.45 * cn.z + 0.5 * h01 + rnd.gauss(0, 0.2)
            if inner:
                col = rnd.choice(cfg.get("inner_cols", ["pine_deep", "hedge_dark"]))
            elif e < cfg["dark_t"]:
                col = rnd.choice(tints["dark"])
            elif e > cfg["light_t"]:
                col = rnd.choice(tints["light"])
            else:
                col = rnd.choice(tints["main"])
            poke = k > 1.02
            pn = (mn + F.rand_unit(rnd) * (0.9 if poke else 0.45)).normalized()
            tip = F.rand_unit(rnd) * 0.6 + mn * (1.0 if poke else 0.4) + UP * 0.2
            sh = (mn * 0.75 + cn * 0.25 + F.rand_unit(rnd) * 0.08).normalized()
            size = cfg["leaf"] * rnd.uniform(0.8, 1.2) * (1.7 if inner else 1.0)
            double = (poke or pn.dot(mn) < 0.78) and not inner
            ao = 0.4 if inner else 0.45 + 0.3 * (mn.z * 0.5 + 0.5) + 0.25 * h01
            acc.leaf(p, pn, tip, size, col, mat="flat", shade=sh, outline=LEAF, cup=0.1, double=double,
                     ao=max(0.3, min(1.0, ao)))
            got += 1
            placed += 1
            if not inner and flowers < cfg["flowers"] and d.z > 0.1 and rnd.random() < cfg["flowers"] / max(want_n, 1) * 0.6:
                fp = p + mn * 0.03
                acc.leaf(fp, (mn + UP * 0.4).normalized(), F.rand_unit(rnd), cfg["flower"] * rnd.uniform(0.85, 1.15),
                         rnd.choice(cfg["flower_cols"]), mat="flat", shade=(mn + UP).normalized(), outline=FLOWER,
                         cup=-0.15, double=False, ao=0.9)
                flowers += 1
    ob, shade = acc.to_object()

    def extra(p, ao, is_leaf):
        if not is_leaf:
            return ao * 0.55
        return ao * (0.8 + 0.2 * max(0.0, min(1.0, p.z / 0.8)))

    tris = F.finish_foliage(ob, shade, "props/bush_round", "bush_round", ao_radius=0.35, ao_strength=0.7,
                            extra=extra, fixed_ao=acc.fixed_ao)
    print(f"INFO bush_round leaves={placed} flowers={flowers}")
    return tris


BUSH = dict(seed=31, leaves=400, inner_p=0.25, leaf=0.15, dark_t=0.15, light_t=0.72, flowers=9, flower=0.07,
            flower_cols=["candy_corn_white", "cream"],
            tints=dict(dark=["hedge_dark"], main=["hedge", "hedge", "vine"], light=["moss", "vine"]))

if want("bush_round"):
    bush_round(BUSH)
