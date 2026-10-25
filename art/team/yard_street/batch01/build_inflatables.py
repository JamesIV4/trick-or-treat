"""Yard Halloween decor: inflatable_ghost, inflatable_cat, cauldron."""
import math
import pathlib
import sys

sys.path.insert(0, str(pathlib.Path(__file__).resolve().parent))
from yard_common import K, bake, cut_below, finish, lathe, paint_where_world, want  # noqa: E402


def scallop(ob, below_z, amp, waves):
    def f(co):
        if co.z < below_z:
            a = math.atan2(co.y, co.x)
            k = 1 + amp * math.cos(waves * a)
            co.x *= k
            co.y *= k
        return co
    return K.deform(ob, f)


def inflatable_ghost():
    K.reset()
    W = "ghost_white"
    parts = []
    body = lathe("body", [(0.64, 0.0), (0.68, 0.07), (0.62, 0.25), (0.53, 0.6), (0.47, 0.95), (0.5, 1.15),
                          (0.56, 1.4), (0.555, 1.62), (0.46, 1.82), (0.28, 1.95), (0.0, 2.0)],
                 W, segments=12, smooth=True, phase=math.pi / 12)
    scallop(body, 0.1, 0.07, 4)
    parts.append(body)
    for sx in (-1, 1):
        parts.append(K.tube(f"arm{sx}", [(0.4 * sx, 0, 1.12), (0.68 * sx, -0.05, 1.32), (0.8 * sx, -0.08, 1.56)],
                            radius=0.13, radius_end=0.1, color=W, sides=6, smooth=True))
        parts.append(K.sphere(f"hand{sx}", 0.13, color=W, loc=(0.82 * sx, -0.08, 1.62), segments=6, rings=4,
                              scale=(1.0, 0.9, 1.15)))
        parts.append(K.sphere(f"eye{sx}", 0.1, color="ink", loc=(0.18 * sx, -0.51, 1.57), scale=(1.0, 0.5, 1.3),
                              segments=7, rings=4))
        parts.append(K.sphere(f"glint{sx}", 0.025, color="eye_white", loc=(0.18 * sx + 0.03, -0.56, 1.62),
                              segments=4, rings=3))
        parts.append(K.sphere(f"cheek{sx}", 0.07, color="cheek_pink", loc=(0.31 * sx, -0.45, 1.38),
                              scale=(1.0, 0.4, 0.7), segments=5, rings=3))
    mouth = K.sphere("mouth", 0.18, color="ink", loc=(0.0, -0.49, 1.34), scale=(1.0, 0.4, 0.85), segments=8, rings=5)
    cut_below(mouth, 1.36, cap_color="ink", keep="below")
    paint_where_world(mouth, "cheek_pink", lambda c, n: c.z < 1.23 and n.y < -0.3)
    parts.append(mouth)
    ghost = K.merge(parts, "inflatable_ghost")

    # a little deflated: the head droops to one side and nods forward
    def sag(co):
        if co.z > 0.9:
            t = (co.z - 0.9) / 1.1
            co.x += 0.11 * t * t
            co.y -= 0.05 * t
            co.z -= 0.06 * t * t
        return co
    K.deform(ghost, sag)
    return finish([ghost], "props/inflatable_ghost", "inflatable_ghost")


def inflatable_cat():
    K.reset()
    C = "cat_black"
    parts = []
    parts.append(lathe("body", [(0.5, 0.0), (0.56, 0.12), (0.55, 0.38), (0.46, 0.62), (0.32, 0.8), (0.2, 0.9)],
                       C, segments=10, smooth=True, cap_bottom=False, cap_top=False))
    parts.append(K.sphere("head", 0.45, color=C, loc=(0, -0.03, 1.05), scale=(1.12, 1.0, 0.9), segments=10, rings=6))
    for sx in (-1, 1):
        parts.append(lathe(f"ear{sx}", [(0.13, 0.0), (0.0, 0.25)], C, segments=4, loc=(0.3 * sx, 0.0, 1.25),
                           rot=(0, 28 * sx, 0), cap_bottom=False))
        parts.append(K.sphere(f"eye{sx}", 0.12, color="cat_orange", loc=(0.17 * sx, -0.44, 1.08),
                              scale=(1.0, 0.45, 1.2), segments=7, rings=4))
        parts.append(K.sphere(f"pupil{sx}", 0.045, color="ink", loc=(0.17 * sx, -0.49, 1.08),
                              scale=(0.55, 0.5, 1.9), segments=5, rings=3))
        parts.append(K.sphere(f"paw{sx}", 0.14, color=C, loc=(0.2 * sx, -0.45, 0.07), scale=(1.0, 1.3, 0.6),
                              segments=6, rings=4))
    parts.append(K.sphere("nose", 0.04, color="cheek_pink", loc=(0, -0.475, 0.99), segments=5, rings=3))
    parts.append(K.tube("tail", [(0.3, 0.4, 0.15), (0.6, 0.1, 0.1), (0.62, -0.3, 0.12), (0.45, -0.55, 0.2),
                                 (0.36, -0.6, 0.36)], radius=0.09, radius_end=0.06, color=C, sides=6, smooth=True))
    hat = [lathe("brim", [(0.0, 1.4), (0.34, 1.41), (0.35, 1.44), (0.0, 1.46)], "witch_purple", segments=10),
           lathe("cone", [(0.19, 1.44), (0.18, 1.53), (0.11, 1.68), (0.04, 1.8), (0.0, 1.86)],
                 ["pumpkin", "witch_purple", "witch_purple", "witch_purple"], segments=10, cap_bottom=False)]
    hat = K.merge(hat, "hat")

    def bend(co):
        if co.z > 1.6:
            t = co.z - 1.6
            co.x += 3.0 * t * t
            co.z -= 0.6 * t * t
        return co
    K.deform(hat, bend)
    hat.rotation_euler.y = math.radians(-8)
    hat.location = (0.215, 0, 0.014)  # undo the sideways slide of tilting about the ground origin
    bake(hat)
    parts.append(hat)
    cat = K.merge(parts, "inflatable_cat")
    return finish([cat], "props/inflatable_cat", "inflatable_cat")


def cauldron():
    K.reset()
    I, G = "iron", "glow_magic_teal"
    parts = []
    prof = [(0.0, 0.1), (0.3, 0.12), (0.47, 0.3), (0.5, 0.5), (0.45, 0.7), (0.4, 0.78), (0.47, 0.82), (0.47, 0.88),
            (0.39, 0.89), (0.38, 0.82), (0.26, 0.85), (0.0, 0.87)]
    cols = [I, I, I, I, I, "stone_dark", "stone_dark", "stone_dark", I, G, G]
    parts.append(lathe("pot", prof, cols, segments=12, phase=0.13))
    for i in range(3):
        a = math.radians(90 + i * 120)
        x, y = math.cos(a) * 0.3, math.sin(a) * 0.3
        parts.append(K.cylinder(f"leg{i}", 0.075, 0.2, color=I, verts=6, loc=(x, y, 0.1), radius_top=0.09,
                                rot=(math.degrees(-math.sin(a) * 0.25), math.degrees(math.cos(a) * 0.25), 0)))
    for i, (r, loc) in enumerate([(0.13, (-0.1, -0.08, 0.9)), (0.095, (0.15, 0.06, 0.89)), (0.07, (0.02, 0.2, 0.88)),
                                  (0.05, (0.2, -0.17, 0.87))]):
        parts.append(K.sphere(f"bubble{i}", r, color=G, loc=loc, segments=7, rings=4))
    pot = K.merge(parts, "cauldron")
    lo = min(v.co.z for v in pot.data.vertices)
    for v in pot.data.vertices:
        v.co.z -= lo
    return finish([pot], "props/cauldron", "cauldron")


if want("inflatable_ghost"):
    inflatable_ghost()
if want("inflatable_cat"):
    inflatable_cat()
if want("cauldron"):
    cauldron()
