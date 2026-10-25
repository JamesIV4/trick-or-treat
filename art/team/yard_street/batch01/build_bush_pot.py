"""bush_round and flower_pot."""
import pathlib
import sys

sys.path.insert(0, str(pathlib.Path(__file__).resolve().parent))
from yard_common import K, blob, cull_hidden, cut_below, finish, lathe, paint_where_world, want  # noqa: E402


def bush_round():
    K.reset()
    parts = [
        blob("main", 0.52, "hedge", loc=(0.0, 0.0, 0.26), scale=(1.0, 0.95, 0.9), lump=0.06, seed=3),
        blob("lump_a", 0.3, "hedge", loc=(0.3, -0.2, 0.3), lump=0.07, seed=9),
        blob("lump_b", 0.3, "hedge", loc=(-0.33, -0.02, 0.28), lump=0.07, seed=17),
        blob("lump_c", 0.27, "hedge", loc=(0.06, 0.1, 0.62), lump=0.07, seed=29),
    ]
    for p in parts:
        cut_below(p, 0.0, cap_color="hedge_dark")
        paint_where_world(p, "hedge_dark", lambda c, n: n.z < -0.25 or c.z < 0.12)
        paint_where_world(p, "moss", lambda c, n: n.z > 0.8)
    cull_hidden(parts)
    bush = K.merge(parts, "bush_round")
    return finish([bush], "props/bush_round", "bush_round")


def flower_pot():
    K.reset()
    pot = lathe("pot", [(0.15, 0.0), (0.19, 0.25), (0.235, 0.255), (0.245, 0.33)],
                ["terracotta", "clay", "terracotta"], segments=8, phase=0.39, cap_bottom=False,
                cap_colors=("clay", "choco_dark"))
    paint_where_world(pot, "clay", lambda c, n: n.z < -0.6)
    soil = 0.32
    mums = [
        blob("mum_a", 0.2, "maple_orange", loc=(-0.05, -0.06, 0.33), scale=(1.0, 1.0, 0.9), lump=0.12, seed=2),
        blob("mum_b", 0.17, "golden_leaf", loc=(0.12, 0.05, 0.33), scale=(1.0, 1.0, 0.9), lump=0.12, seed=5),
        blob("mum_c", 0.16, "maple_orange", loc=(-0.02, 0.12, 0.35), scale=(1.0, 1.0, 0.9), lump=0.12, seed=8),
    ]
    for m in mums:
        cut_below(m, soil, cap_color="hedge_dark")
        paint_where_world(m, "rust_leaf", lambda c, n: n.z < -0.55)
    cull_hidden(mums)
    # leaf tufts poking out over the rim
    leaves = [K.cylinder("leaf_a", 0.1, 0.025, color="hedge", verts=3, loc=(-0.2, -0.14, 0.35), rot=(25, -35, 20)),
              K.cylinder("leaf_b", 0.095, 0.025, color="hedge", verts=3, loc=(0.22, -0.08, 0.35), rot=(20, 35, -15)),
              K.cylinder("leaf_c", 0.09, 0.025, color="hedge", verts=3, loc=(-0.12, 0.2, 0.36), rot=(-25, -20, 40))]
    ob = K.merge([pot] + mums + leaves, "flower_pot")
    return finish([ob], "props/flower_pot", "flower_pot")


if want("bush_round"):
    bush_round()
if want("flower_pot"):
    flower_pot()
