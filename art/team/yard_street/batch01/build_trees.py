"""Storybook maples: tree_maple_red, tree_maple_orange, tree_maple_gold."""
import pathlib
import sys

sys.path.insert(0, str(pathlib.Path(__file__).resolve().parent))
from yard_common import K, blob, finish, lathe, paint_where_world, socket, want  # noqa: E402


def trunk_parts(height, lean=1.0, branch_side=1.0):
    s = height / 5.0
    parts = []
    # root flare: a chunky skirt so the trunk sits heavy on the lawn
    parts.append(lathe("flare", [(0.5, 0.0), (0.38, 0.14), (0.28, 0.5)], "bark", segments=8,
                       cap_bottom=True, cap_top=False, phase=0.2))
    trunk = [(0, 0, 0.3), (0.04 * lean, 0, 1.1), (0.02 * lean, 0, 1.9), (-0.10 * lean, 0, 2.7 * s),
             (-0.08 * lean, 0, 3.5 * s), (0.0, 0, 3.9 * s)]
    parts.append(K.tube("trunk", trunk, radius=0.29, radius_end=0.15, color="bark", sides=8, smooth=False))
    # the low strong branch that carries swings / lantern strings (socket_branch at its middle)
    b = branch_side
    parts.append(K.tube("branch_low", [(0.0, 0, 2.05), (0.45 * b, -0.02, 2.42), (1.0 * b, -0.04, 2.6),
                                       (1.32 * b, -0.04, 2.86)],
                        radius=0.16, radius_end=0.08, color="bark", sides=6, smooth=False))
    # two upper branches disappearing into the canopy
    parts.append(K.tube("branch_up_a", [(-0.06 * lean, 0, 2.6 * s), (-0.6 * b, 0.15, 3.2 * s), (-0.95 * b, 0.25, 3.6 * s)],
                        radius=0.1, radius_end=0.05, color="bark", sides=5, smooth=False))
    parts.append(K.tube("branch_up_b", [(-0.04 * lean, 0, 3.0 * s), (0.35 * b, -0.3, 3.5 * s), (0.6 * b, -0.45, 3.9 * s)],
                        radius=0.09, radius_end=0.05, color="bark", sides=5, smooth=False))
    for p in parts:
        paint_where_world(p, "bark_dark", lambda c, n: n.z < -0.55)
    return parts


def canopy(layout, base, under, seed0):
    blobs = []
    for i, (r, loc, scl, col) in enumerate(layout):
        ob = blob(f"leaf{i}", r, col or base, loc=loc, scale=scl, lump=0.07, seed=seed0 + i * 7)
        paint_where_world(ob, under, lambda c, n: n.z < -0.5)
        blobs.append(ob)
    return blobs


def build(asset_id, height, layout, base, under, seed, lean=1.0):
    K.reset()
    parts = trunk_parts(height, lean)
    parts += canopy(layout, base, under, seed)
    tree = K.merge(parts, asset_id)
    socket("branch", (1.0, -0.04, 2.6), tree)
    return finish([tree], f"props/{asset_id}", asset_id)


# layouts: (radius, centre, scale, colour or None for base)
RED = [
    (1.3, (0.0, 0.0, 3.95), (1.2, 1.1, 0.85), None),
    (0.9, (1.35, -0.1, 3.6), (1.0, 1.0, 0.85), None),
    (0.9, (-1.3, 0.2, 3.7), (1.0, 1.0, 0.85), "maple_orange"),
    (0.85, (0.25, -1.15, 3.55), (1.0, 1.0, 0.85), None),
    (0.85, (-0.3, 1.15, 3.65), (1.0, 1.0, 0.85), None),
    (0.72, (0.5, 0.3, 4.45), (1.0, 1.0, 0.85), "maple_orange"),
    (0.7, (-0.8, -0.6, 4.35), (1.0, 1.0, 0.9), None),
    (0.75, (0.95, 0.75, 3.9), (1.0, 1.0, 0.85), None),
]
ORANGE = [
    (1.35, (0.0, 0.0, 3.65), (1.1, 1.1, 0.7), None),
    (0.8, (1.2, -0.3, 3.4), (1.0, 1.0, 0.85), "golden_leaf"),
    (0.8, (-1.15, 0.3, 3.45), (1.0, 1.0, 0.85), None),
    (0.78, (0.1, -1.2, 3.38), (1.0, 1.0, 0.85), None),
    (0.78, (-0.3, 1.15, 3.45), (1.0, 1.0, 0.85), "golden_leaf"),
    (0.7, (0.85, 0.85, 3.55), (1.0, 1.0, 0.85), None),
    (0.7, (-0.85, -0.85, 3.6), (1.0, 1.0, 0.85), None),
    (0.65, (-0.3, -0.3, 4.05), (1.0, 1.0, 0.85), "golden_leaf"),
]
GOLD = [
    (1.1, (0.0, 0.0, 3.8), (1.05, 1.05, 1.0), None),
    (0.9, (0.05, 0.05, 4.55), (1.0, 1.0, 1.0), None),
    (0.7, (0.95, -0.2, 3.55), (1.0, 1.0, 1.1), "rust_leaf"),
    (0.72, (-0.95, 0.15, 3.85), (1.0, 1.0, 1.1), None),
    (0.66, (0.1, -0.95, 4.2), (1.0, 1.0, 1.1), None),
    (0.6, (0.6, 0.5, 4.75), (1.0, 1.0, 1.1), "rust_leaf"),
    (0.52, (-0.5, -0.35, 4.95), (1.0, 1.0, 1.0), None),
    (0.65, (-0.3, 0.8, 3.5), (1.0, 1.0, 1.1), None),
]
if want("tree_maple_red"):
    build("tree_maple_red", 5.0, RED, "maple_red", "roof_red", 11, lean=1.0)
if want("tree_maple_orange"):
    build("tree_maple_orange", 4.5, ORANGE, "maple_orange", "rust_leaf", 23, lean=-0.8)
if want("tree_maple_gold"):
    build("tree_maple_gold", 5.5, GOLD, "golden_leaf", "burnt_amber", 37, lean=0.6)
