"""Yard Halloween decor (v2): inflatable_ghost, inflatable_cat, cauldron.

Inflatables are gored vinyl balloons: the body is swept up the Z axis with a cross-section that
bulges between vertical seams, plus pinched horizontal seam rings, so they read as puffy
sewn-together vinyl panels.
"""
import pathlib
import sys

sys.path.insert(0, str(pathlib.Path(__file__).resolve().parent))
from yard_common import *  # noqa: E402,F401,F403


def gored_body(name, prof, color, gores=6, sides=24, seams=(), seam_depth=0.035, gore_amp=0.035, phase=0.0,
               mat="vinyl", hem=None):
    """prof: [(z, r)] bottom to top. seams: z heights of pinched horizontal seams.
    hem(i, a) -> extra factor for the bottom rings (scalloped skirt)."""
    pts, radii = [], []
    seam_set = []
    zs = [z for z, _ in prof]
    rs = [r for _, r in prof]

    def r_at(z):
        for k in range(len(zs) - 1):
            if zs[k] <= z <= zs[k + 1]:
                t = (z - zs[k]) / (zs[k + 1] - zs[k])
                return rs[k] + (rs[k + 1] - rs[k]) * t
        return rs[-1]

    levels = sorted(set(zs + [s + d for s in seams for d in (-0.035, 0.0, 0.035)]))
    for z in levels:
        r = r_at(z)
        if any(abs(z - s) < 1e-6 for s in seams):
            r *= 1 - seam_depth
            seam_set.append(len(pts))
        pts.append((0, 0, z))
        radii.append(max(r, 0.004))
    top_r = radii[-1]

    def shape(i, a):
        g = abs(math.sin(gores / 2 * (a + phase)))
        k = 1 + gore_amp * (g ** 0.6 - 0.75)
        # gores fade out toward the very top/bottom poles
        k = 1 + (k - 1) * min(1.0, radii[i] / 0.12)
        if hem is not None:
            k *= hem(i, a)
        return k

    ob = sweep(name, pts, radii, color=color, mat=mat, sides=sides, caps=(True, top_r > 0.01), shape=shape,
               up_hint=(1, 0, 0))
    return ob


def inflatable_ghost():
    K.reset()
    W = "ghost_white"
    parts = []
    prof = [(0.0, 0.6), (0.06, 0.66), (0.22, 0.6), (0.6, 0.52), (0.95, 0.47), (1.18, 0.5), (1.42, 0.56), (1.64, 0.55),
            (1.8, 0.47), (1.91, 0.34), (1.97, 0.18), (2.0, 0.03)]

    def hem(i, a):
        return 1 + (0.07 * math.cos(5 * a) if i < 2 else 0.0)

    body = gored_body("body", prof, W, gores=6, sides=24, seams=(0.6, 1.18), hem=hem)
    parts.append(body)
    for sx in (-1, 1):
        arm = catmull([(0.38 * sx, 0.0, 1.1), (0.62 * sx, -0.04, 1.28), (0.76 * sx, -0.08, 1.5), (0.8 * sx, -0.1, 1.62)],
                      per=2)
        parts.append(sweep(f"arm{sx}", arm, [0.15, 0.14, 0.13, 0.12, 0.115, 0.11, 0.11], color=W, mat="vinyl",
                           sides=8, caps=(False, False)))
        parts.append(blob(f"hand{sx}", 0.135, W, mat="vinyl", loc=(0.82 * sx, -0.1, 1.68), scale=(1.0, 0.9, 1.12),
                          segments=8, rings=6, noise_amp=0.0, axis="Z"))
        parts.append(blob(f"eye{sx}", 0.105, "ink", mat="flat", loc=(0.18 * sx, -0.49, 1.58), scale=(0.95, 0.42, 1.3),
                          segments=8, rings=5, noise_amp=0.0, axis="Y"))
        parts.append(blob(f"glint{sx}", 0.03, "eye_white", mat="flat", loc=(0.18 * sx + 0.035, -0.535, 1.64),
                          scale=(1, 0.5, 1), segments=6, rings=4, noise_amp=0.0, axis="Y"))
        parts.append(blob(f"cheek{sx}", 0.075, "cheek_pink", mat="flat", loc=(0.33 * sx, -0.43, 1.38),
                          scale=(1.0, 0.35, 0.65), segments=6, rings=4, noise_amp=0.0, axis="Y"))
        # tether rope to a little ground stake
        parts.append(sweep(f"rope{sx}", [(0.5 * sx, -0.2, 0.55), (0.75 * sx, -0.32, 0.25), (0.95 * sx, -0.42, 0.04)],
                           0.012, color="rope", mat="flat", sides=4))
        parts.append(rod(f"stake{sx}", (0.95 * sx, -0.42, 0.0), (0.95 * sx, -0.42, 0.07), 0.018, "silver", sides=5,
                         chamfer=0))
    mouth = blob("mouth", 0.17, "ink", mat="flat", loc=(0.0, -0.48, 1.36), scale=(1.0, 0.42, 0.82), segments=10, rings=6,
                 noise_amp=0.0, axis="Y")
    cut_below(mouth, 1.375, cap_color="ink", keep="below")
    paint_world(mouth, "cheek_pink", lambda c, n: c.z < 1.27 and n.y < -0.3)
    parts.append(mouth)
    ghost = K.merge(parts, "inflatable_ghost")

    def sag(co):  # a little deflated: the head droops sideways and nods forward
        if co.z > 0.9:
            t = (co.z - 0.9) / 1.1
            co.x += 0.1 * t * t
            co.y -= 0.05 * t
            co.z -= 0.05 * t * t
        return co
    K.deform(ghost, sag)
    ground(ghost)
    return finish([ghost], "props/inflatable_ghost", "inflatable_ghost", ao_radius=0.5, ao_strength=0.75)


def inflatable_cat():
    K.reset()
    C = "cat_black"
    parts = []
    prof = [(0.0, 0.5), (0.06, 0.56), (0.2, 0.58), (0.42, 0.54), (0.62, 0.45), (0.78, 0.34), (0.9, 0.24)]
    body = gored_body("body", prof, C, gores=6, sides=22, seams=(0.42,), phase=0.26)
    paint_world(body, "charcoal", lambda c, n: n.y < -0.4 and 0.1 < c.z < 0.75 and abs(c.x) < 0.34)
    parts.append(body)
    head = blob("head", 0.46, C, mat="vinyl", loc=(0, -0.04, 1.06), scale=(1.13, 1.0, 0.9), segments=16, rings=10,
                noise_amp=0.0, axis="Z")
    parts.append(head)
    for sx in (-1, 1):
        ear = lathe(f"ear{sx}", [(0.15, 0.0), (0.1, 0.12), (0.0, 0.27)], C, "vinyl", segments=6, loc=(0.29 * sx, 0.0, 1.27),
                    rot=(-6, 26 * sx, 0), scale=(1.0, 0.7, 1.0), cap_bottom=False)
        inner = lathe(f"inner{sx}", [(0.09, 0.0), (0.0, 0.18)], "cheek_pink", "vinyl", segments=4,
                      loc=(0.3 * sx, -0.07, 1.31), rot=(-6, 26 * sx, 0), scale=(1.0, 0.45, 1.0), cap_bottom=False)
        eye = blob(f"eye{sx}", 0.125, "cat_orange", mat="flat", loc=(0.18 * sx, -0.43, 1.09), scale=(1.0, 0.42, 1.18),
                   segments=8, rings=5, noise_amp=0.0, axis="Y")
        pupil = blob(f"pupil{sx}", 0.05, "ink", mat="flat", loc=(0.18 * sx, -0.48, 1.09), scale=(0.55, 0.4, 1.8),
                     segments=6, rings=4, noise_amp=0.0, axis="Y")
        glint = blob(f"glint{sx}", 0.022, "eye_white", mat="flat", loc=(0.18 * sx + 0.03, -0.5, 1.15),
                     segments=5, rings=3, noise_amp=0.0, axis="Y")
        paw = blob(f"paw{sx}", 0.15, C, mat="vinyl", loc=(0.21 * sx, -0.44, 0.08), scale=(1.0, 1.35, 0.6),
                   segments=8, rings=5, noise_amp=0.0, axis="Y")
        cheek = blob(f"cheek{sx}", 0.06, "cheek_pink", mat="flat", loc=(0.3 * sx, -0.4, 0.96), scale=(1, 0.35, 0.6),
                     segments=6, rings=4, noise_amp=0.0, axis="Y")
        parts += [ear, inner, eye, pupil, glint, paw, cheek]
        for k, dz in enumerate((0.0, -0.05)):
            parts.append(sweep(f"whisker{sx}{k}", [(0.12 * sx, -0.47, 0.98 + dz), (0.3 * sx, -0.46, 0.99 + dz * 1.6),
                                                   (0.46 * sx, -0.4, 1.0 + dz * 2.4)], 0.008, color="ghost_white",
                               mat="flat", sides=3))
    parts.append(blob("nose", 0.04, "cheek_pink", mat="flat", loc=(0, -0.475, 1.0), scale=(1.2, 0.6, 0.8), segments=6,
                      rings=4, noise_amp=0.0, axis="Y"))
    tail = catmull([(0.3, 0.42, 0.16), (0.62, 0.15, 0.12), (0.66, -0.25, 0.14), (0.5, -0.52, 0.22), (0.4, -0.58, 0.4)],
                   per=2)
    parts.append(sweep("tail", tail, [0.1 - 0.035 * k / (len(tail) - 1) for k in range(len(tail))], color=C, mat="vinyl",
                       sides=7, caps=(False, True)))
    # witch hat: brim, pumpkin band, cone bent over at the tip, gold buckle
    hat = [lathe("brim", [(0.0, 1.38), (0.36, 1.39), (0.38, 1.415), (0.35, 1.44), (0.0, 1.45)], "witch_purple", "felt",
                 segments=12),
           lathe("band", [(0.205, 1.43), (0.2, 1.52)], "pumpkin", "felt", segments=12, cap_bottom=False, cap_top=False),
           lathe("cone", [(0.19, 1.51), (0.15, 1.62), (0.1, 1.72), (0.055, 1.81), (0.0, 1.88)], "witch_purple", "felt",
                 segments=12, cap_bottom=False)]
    hat.append(box("buckle", (0.08, 0.02, 0.07), "foil_gold", "metal", loc=(0, -0.205, 1.475), bevel=0.008, segments=1))
    hat = K.merge(hat, "hat")

    def bend(co):
        if co.z > 1.62:
            t = co.z - 1.62
            co.x += 2.6 * t * t
            co.z -= 0.5 * t * t
        return co
    K.deform(hat, bend)
    hat.rotation_euler.y = math.radians(-9)
    bake(hat)
    hat.data.transform(Matrix.Translation((0.24, 0, 0.03)))
    parts.append(hat)
    cat = K.merge(parts, "inflatable_cat")
    ground(cat)
    return finish([cat], "props/inflatable_cat", "inflatable_cat", ao_radius=0.45, ao_strength=0.75)


def cauldron():
    K.reset()
    I, G = "iron", "glow_magic_teal"
    parts = []
    prof = [(0.0, 0.12), (0.22, 0.13), (0.38, 0.2), (0.47, 0.33), (0.5, 0.48), (0.47, 0.64), (0.41, 0.74), (0.385, 0.78),
            (0.42, 0.8), (0.47, 0.825), (0.48, 0.86), (0.455, 0.89), (0.41, 0.895), (0.39, 0.87), (0.38, 0.82)]
    cols = [I] * 7 + ["stone_dark"] * 7
    pot = lathe("pot", prof, cols, "metal", segments=14, cap_bottom=False, cap_top=False, phase=0.1)
    parts.append(pot)
    potion = lathe("potion", [(0.385, 0.83), (0.2, 0.85), (0.0, 0.855)], G, "flat", segments=14, cap_bottom=False,
                   cap_top=False, phase=0.1)
    parts.append(potion)
    for i in range(3):
        a = math.radians(90 + i * 120)
        x, y = math.cos(a) * 0.3, math.sin(a) * 0.3
        leg = lathe(f"leg{i}", [(0.0, 0.0), (0.06, 0.005), (0.085, 0.04), (0.095, 0.12), (0.08, 0.22)], I, "metal",
                    segments=8, cap_top=False, loc=(x, y, 0.0),
                    rot=(math.degrees(-math.sin(a) * 0.22), math.degrees(math.cos(a) * 0.22), 0))
        parts.append(leg)
    for sx in (-1, 1):  # ring handles on the shoulders
        parts.append(K.torus(f"ear{sx}", 0.07, 0.018, color=I, mat="metal", major_seg=10, minor_seg=5,
                             loc=(0.5 * sx, 0, 0.66), rot=(90, 0, 0)))
        parts.append(blob(f"lug{sx}", 0.04, I, mat="metal", loc=(0.47 * sx, 0, 0.73), scale=(1, 0.8, 0.8), segments=6,
                          rings=4, noise_amp=0.0))
    for i, (r, loc) in enumerate([(0.12, (-0.1, -0.08, 0.89)), (0.09, (0.15, 0.07, 0.88)), (0.065, (0.03, 0.2, 0.87)),
                                  (0.05, (0.2, -0.17, 0.865)), (0.04, (-0.24, 0.1, 0.86))]):
        parts.append(blob(f"bubble{i}", r, G, mat="flat", loc=loc, scale=(1, 1, 0.85), segments=8, rings=5,
                          noise_amp=0.0, axis="Z"))
    # a glowing drip over the rim
    da = math.radians(-70)
    dx, dy = math.cos(da), math.sin(da)
    parts.append(sweep("drip", [(dx * 0.4, dy * 0.4, 0.885), (dx * 0.485, dy * 0.485, 0.86), (dx * 0.47, dy * 0.47, 0.8),
                                (dx * 0.47, dy * 0.47, 0.76)], [0.02, 0.022, 0.018, 0.015], color=G, mat="flat", sides=6))
    parts.append(blob("drop", 0.022, G, mat="flat", loc=(dx * 0.47, dy * 0.47, 0.745), scale=(1, 1, 1.25), segments=6,
                      rings=4, noise_amp=0.0))
    # wooden stirring spoon leaning on the rim
    parts.append(rod("spoon", (-0.05, 0.05, 0.75), (-0.42, 0.3, 1.35), 0.025, "wood", mat="wood", sides=6))
    parts.append(blob("spoon_end", 0.05, "wood", mat="wood", loc=(-0.43, 0.31, 1.37), scale=(1, 1, 0.8), segments=6,
                      rings=4, noise_amp=0.0))
    ob = K.merge(parts, "cauldron")
    ground(ob)
    return finish([ob], "props/cauldron", "cauldron", ao_radius=0.35)


if want("inflatable_ghost"):
    inflatable_ghost()
if want("inflatable_cat"):
    inflatable_cat()
if want("cauldron"):
    cauldron()
