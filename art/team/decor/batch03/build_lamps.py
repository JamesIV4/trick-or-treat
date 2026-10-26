"""Victorian lights (batch03 upgrade): lamp_post, porch_lantern.

    blender --background --factory-startup --python build_lamps.py -- [lamp_post] [porch_lantern]
"""
import pathlib
import sys

sys.path.insert(0, str(pathlib.Path(__file__).resolve().parent))
from decor_common import *  # noqa: E402,F401,F403
from decor_common import B, K, lantern4, math, report  # noqa: E402

RIM, CORE = "glow_lantern", "glow_lamp"


def fluted(n, depth=0.14, phase=0.0):
    """shape(i, a) for sweep(): n raised ribs with soft flutes between them."""
    def f(i, a):
        return 1.0 - depth * (0.5 - 0.5 * math.cos((a - phase) * n))
    return f


def lamp_post():
    K.reset()
    parts = []
    ph8 = math.radians(22.5)
    # octagonal two-step plinth (crisp facets)
    parts.append(lathe("plinth", [(0.25, 0.0), (0.26, 0.015), (0.26, 0.085), (0.235, 0.105), (0.2, 0.11),
                                  (0.2, 0.17), (0.185, 0.185), (0.165, 0.19)], B, "metal", segments=8, phase=ph8,
                       smooth=False, cap_bottom=False, cap_top=True))
    # round bell base with moulding rings
    bell = [(0.17, 0.185), (0.178, 0.215), (0.16, 0.235), (0.146, 0.3), (0.12, 0.43),
            (0.096, 0.62), (0.088, 0.7), (0.108, 0.715), (0.11, 0.76), (0.078, 0.785)]
    parts.append(lathe("bell", bell, B, "metal", segments=12, cap_bottom=False, cap_top=False,
                       sharp_bands=(0, 1, 6, 7, 8)))
    # fluted shaft
    shaft = [(0, 0, 0.77), (0, 0, 1.6), (0, 0, 2.42)]
    parts.append(sweep("shaft", shaft, [0.074, 0.068, 0.062], color=B, mat="metal", sides=16,
                       shape=fluted(8, 0.16), caps=(False, False)))
    # small ring a third of the way up
    parts.append(lathe("ring", [(0.066, 1.12), (0.083, 1.135), (0.083, 1.165), (0.066, 1.18)], B, "metal",
                       segments=12, cap_bottom=False, cap_top=False))
    # decorative collar under the head: stacked rings + four scroll brackets
    collar = [(0.06, 2.4), (0.085, 2.415), (0.085, 2.45), (0.066, 2.465), (0.066, 2.5), (0.09, 2.53),
              (0.115, 2.6), (0.122, 2.635), (0.112, 2.655), (0.0, 2.66)]
    parts.append(lathe("collar", collar, B, "metal", segments=10, cap_bottom=False, cap_top=False,
                       sharp_bands=(0, 1, 2, 7, 8)))
    for k in range(4):
        a = math.radians(45 + 90 * k)
        c, s = math.cos(a), math.sin(a)
        pts = [(0.06, 2.43), (0.1, 2.46), (0.14, 2.52), (0.165, 2.6), (0.17, 2.66)]
        curl = [(r * c, r * s, z) for r, z in pts]
        parts.append(sweep(f"scroll{k}", curl, [0.016, 0.016, 0.015, 0.014, 0.013], color=B, mat="metal", sides=5,
                           caps=(False, True)))

    head, gc, top, bot = lantern4("head", (0, 0, 2.69), 0.46, 0.13, 0.185, rim=RIM, core=CORE, bottom="seat",
                                  frame_w=0.02, roof_w=0.255, roof_h=0.19)
    parts += head
    report(parts)
    post = K.merge(parts, "lamp_post")
    socket("light", gc, post)
    print("lamp_post top", top, "glass centre", gc)
    return finish([post], "props/lamp_post", "lamp_post", ao_radius=0.4)


def porch_lantern():
    K.reset()
    parts = []
    # wall plate with a rounded boss where the arm meets it
    parts.append(box("plate", (0.12, 0.025, 0.27), B, "metal", loc=(0, -0.0125, 0.0), bevel=0.008, segments=1))
    parts.append(blob("boss", 0.03, B, mat="metal", loc=(0, -0.03, 0.075), scale=(1, 0.8, 1), segments=6, rings=4,
                      noise_amp=0.0, axis="Y"))


    hy = -0.21
    # arm out from the plate ending in a hook
    arm = [(0, -0.03, 0.075), (0, -0.1, 0.085), (0, -0.17, 0.095), (0, hy + 0.005, 0.09), (0, hy, 0.07)]
    parts.append(sweep("arm", arm, [0.014, 0.014, 0.013, 0.012, 0.011], color=B, mat="metal", sides=5))
    # scroll brace under the arm
    sc = arc((0, -0.085, 0.02), 0.05, 200, -110, 8, plane="YZ", radius_end=0.02)
    parts.append(sweep("scroll", [(0, -0.03, -0.07)] + sc, 0.009, color=B, mat="metal", sides=4))
    head, gc, top, bot = lantern4("head", (0, hy, -0.27), 0.2, 0.062, 0.088, rim=RIM, core=CORE, bottom="pan",
                                  frame_w=0.011, roof_w=0.125, roof_h=0.085, hang_ring=True)
    parts += head
    print("porch_lantern top", top, "bottom", bot, "glass centre", gc)
    report(parts)
    ob = K.merge(parts, "porch_lantern")
    socket("light", gc, ob)
    return finish([ob], "props/porch_lantern", "porch_lantern", ao_radius=0.1, ao_strength=0.6, ground_ao=False)


if want("lamp_post"):
    lamp_post()
if want("porch_lantern"):
    porch_lantern()
