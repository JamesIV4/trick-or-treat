"""Light props (v2): lamp_post, paper_lantern, porch_lantern, string_bulb."""
import pathlib
import sys

sys.path.insert(0, str(pathlib.Path(__file__).resolve().parent))
from yard_common import *  # noqa: E402,F401,F403

B = "lamp_black"


def panes(n, frame_deg, glass, frame=B, offset=0.0):
    """Column angles + colours for a lathe band: n glass panes split by thin frame mullions."""
    angles, cols = [], []
    for i in range(n):
        a = offset + i * 360.0 / n
        angles += [a - frame_deg / 2, a + frame_deg / 2]
        cols += [frame, glass]
    return angles, cols


def lantern_head(name, s, glass, sides=6, loc=(0, 0, 0)):
    """Hanging lantern: origin at the hook, body below. Returns parts and the glass centre z."""
    ang, cols = panes(sides, 7, glass, offset=180.0 / sides)
    parts = []
    # roof: flared hood with a rolled rim, finial knob and hook ring above
    parts.append(lathe(f"{name}_roof", [(0.0, -0.0), (0.03, -0.01), (0.035, -0.04), (0.06, -0.06), (0.17, -0.17),
                                         (0.21, -0.2), (0.215, -0.225), (0.19, -0.235), (0.0, -0.235)],
                       B, "metal", segments=sides * 2, cap_bottom=False, cap_top=False, phase=0.0,
                       loc=loc, scale=(s, s, s), sharp_bands=(4, 5, 6)))
    # glass panes with frame mullions, slightly tapering toward the bottom
    parts.append(lathe(f"{name}_glass", [(0.165, -0.235), (0.13, -0.56)][::-1], glass, "flat", angles=ang,
                       band_cols={0: cols}, cap_bottom=False, cap_top=False, loc=loc, scale=(s, s, s), smooth=False))
    # top + bottom frame rings
    parts.append(lathe(f"{name}_ring", [(0.13, -0.6), (0.15, -0.58), (0.15, -0.55), (0.12, -0.545)], B, "metal",
                       segments=sides * 2, cap_bottom=False, cap_top=True, cap_colors=(B, glass), cap_mats=("metal", "flat"),
                       loc=loc, scale=(s, s, s)))
    # bottom bowl and drip finial
    parts.append(lathe(f"{name}_bowl", [(0.0, -0.72), (0.025, -0.71), (0.03, -0.68), (0.07, -0.65), (0.13, -0.6)],
                       B, "metal", segments=sides * 2, cap_bottom=False, cap_top=False, loc=loc, scale=(s, s, s)))
    return parts, loc[2] - 0.4 * s


def lamp_post():
    K.reset()
    parts = []
    base_prof = [(0.27, 0.0), (0.285, 0.02), (0.285, 0.1), (0.26, 0.125), (0.215, 0.14), (0.205, 0.18), (0.17, 0.26),
                 (0.125, 0.4), (0.1, 0.52), (0.118, 0.55), (0.118, 0.6), (0.085, 0.63)]
    parts.append(lathe("base", base_prof, B, "metal", segments=12, cap_bottom=False, cap_top=False, phase=0.22,
                       sharp_bands=(1,)))
    parts.append(lathe("pole", [(0.078, 0.62), (0.07, 1.6), (0.062, 2.9)], B, "metal", segments=10,
                       cap_bottom=False, cap_top=False))
    parts.append(lathe("collar", [(0.066, 1.48), (0.1, 1.52), (0.1, 1.6), (0.066, 1.64)], B, "metal",
                       segments=10, cap_bottom=False, cap_top=False))
    # capital + ball finial on the post top
    parts.append(lathe("capital", [(0.062, 2.88), (0.1, 2.93), (0.1, 2.99), (0.075, 3.02), (0.0, 3.03)], B, "metal",
                       segments=10, cap_bottom=False, cap_top=False))
    parts.append(blob("finial", 0.06, B, mat="metal", loc=(0, 0, 3.08), segments=8, rings=5, noise_amp=0.0, axis="Z"))
    # shepherd's crook arm from the post to the hook, with a curl scroll inside
    lx, hz = 0.72, 3.0
    crook = [(0.0, 0, 2.72)] + arc((lx / 2 + 0.02, 0, 3.0), lx / 2 - 0.02, 180, 10, 9)[1:] + [(lx, 0, 3.05), (lx, 0, hz - 0.05)]
    parts.append(sweep("crook", crook, [0.048] * 3 + [0.042] * (len(crook) - 3), color=B, mat="metal", sides=7,
                       caps=(True, True)))
    curl = arc((0.3, 0, 2.98), 0.2, 200, 540, 10, radius_end=0.05)
    parts.append(sweep("curl", curl, [0.03 - 0.012 * i / len(curl) for i in range(len(curl))], color=B, mat="metal",
                       sides=6, caps=(True, True)))
    parts.append(sweep("brace", [(0.0, 0, 2.45), (0.12, 0, 2.62), (0.24, 0, 2.82)], [0.026, 0.024, 0.022], color=B,
                       mat="metal", sides=6))
    head, gz = lantern_head("head", 1.2, "glow_lamp", sides=6, loc=(lx, 0, hz - 0.03))
    parts += head
    post = K.merge(parts, "lamp_post")
    socket("light", (lx, 0, gz), post)
    return finish([post], "props/lamp_post", "lamp_post", ao_radius=0.4)


def paper_lantern():
    K.reset()
    G, R, D = "glow_lantern", "wood_dark", B
    zc, rz, rr = -0.235, 0.175, 0.178
    # 8 paper bands on a sphere, 3 thin raised dark ribs inserted between them
    ts = [i / 8 for i in range(9)]
    rib_t = [2.5 / 8, 4 / 8, 5.5 / 8]
    d = 0.022
    levels = [(t, "p") for t in ts] + [(t - d, "r0") for t in rib_t] + [(t + d, "r1") for t in rib_t]
    levels.sort()
    prof2, cols2, mats2 = [], [], []
    for k, (t, kind) in enumerate(levels):
        a = math.pi * t
        r = math.sin(a) * rr * (1.03 if kind != "p" else 1.0)
        z = zc - math.cos(a) * rz
        prof2.append((max(r, 0.07), z))
        if k > 0:
            dark = kind == "r1"
            cols2.append(R if dark else G)
            mats2.append("flat" if dark else "paper")
    body = lathe("body", prof2, cols2, mats2, segments=12, cap_bottom=False, cap_top=False, phase=0.1)
    top = lathe("cap_top", [(0.075, -0.068), (0.085, -0.055), (0.06, -0.036), (0.0, -0.03)], D, "metal",
                segments=10, cap_bottom=True, cap_top=False, cap_colors=(G, D), cap_mats=("flat", "metal"))
    bot = lathe("cap_bot", [(0.0, -0.44), (0.06, -0.432), (0.085, -0.41), (0.075, -0.4)], D, "metal",
                segments=10, cap_bottom=False, cap_top=True, cap_colors=(D, G), cap_mats=("metal", "flat"))
    loop = torus_ring("loop", 0.022, 0.006, (0, 0, -0.014))
    tassel = lathe("tassel", [(0.0, -0.53), (0.022, -0.52), (0.018, -0.47), (0.008, -0.45), (0.0, -0.44)],
                   "banner_orange", "felt", segments=7)
    cord = sweep("cord", [(0, 0, -0.44), (0, 0, -0.455)], 0.005, color=D, mat="flat", sides=4)
    ob = K.merge([body, top, bot, loop, tassel, cord], "paper_lantern")
    socket("hang", (0, 0, 0), ob)
    socket("light", (0, 0, zc), ob)
    return finish([ob], "props/paper_lantern", "paper_lantern", ao_radius=0.12, ao_strength=0.6, ground_ao=False)


def torus_ring(name, major, minor, loc, rot=(90, 0, 0)):
    ob = K.torus(name, major, minor, color=B, mat="metal", major_seg=8, minor_seg=4, loc=loc, rot=rot)
    return ob


def porch_lantern():
    K.reset()
    parts = []
    parts.append(box("plate", (0.15, 0.03, 0.27), B, "metal", loc=(0, -0.015, -0.02), bevel=0.012, segments=2))
    parts.append(blob("boss", 0.035, B, mat="metal", loc=(0, -0.04, -0.03), scale=(1, 0.8, 1), segments=8, rings=5,
                      noise_amp=0.0, axis="Y"))
    # arm from the plate with a little scroll underneath
    parts.append(sweep("arm", [(0, -0.03, -0.03), (0, -0.09, -0.03), (0, -0.15, 0.0), (0, -0.17, 0.05)],
                       [0.018, 0.017, 0.016, 0.016], color=B, mat="metal", sides=6))
    parts.append(sweep("scroll", arc((0, -0.075, -0.075), 0.045, 90, -200, 8, plane="YZ", radius_end=0.015),
                       [0.012] * 9, color=B, mat="metal", sides=5))
    cy = -0.17
    g = "glow_porch"
    ang, cols = panes(4, 12, g, offset=45)
    parts.append(lathe("bottom", [(0.0, -0.205), (0.03, -0.2), (0.06, -0.185), (0.105, -0.16), (0.105, -0.135),
                                  (0.09, -0.13)], B, "metal", segments=4, phase=math.radians(45), cap_top=True,
                       cap_colors=(B, g), cap_mats=("metal", "flat"), loc=(0, cy, 0.0), smooth=False))
    parts.append(lathe("glass", [(0.085, -0.13), (0.1, 0.09)], g, "flat", angles=ang, band_cols={0: cols},
                       cap_bottom=False, cap_top=False, loc=(0, cy, 0.0), smooth=False))
    for k in range(4):  # corner posts
        a = math.radians(45 + 90 * k)
        parts.append(rod(f"post{k}", (math.cos(a) * 0.098, cy + math.sin(a) * 0.098, -0.135),
                         (math.cos(a) * 0.112, cy + math.sin(a) * 0.112, 0.095), 0.009, B, sides=5, chamfer=0))
    parts.append(lathe("roof", [(0.13, 0.085), (0.145, 0.1), (0.14, 0.115), (0.05, 0.19), (0.035, 0.2),
                                (0.035, 0.215), (0.0, 0.22)], B, "metal", segments=4, phase=math.radians(45),
                       cap_bottom=True, cap_colors=(g, B), cap_mats=("flat", "metal"), loc=(0, cy, 0.0), smooth=False))
    parts.append(blob("knob", 0.025, B, mat="metal", loc=(0, cy, 0.24), segments=8, rings=5, noise_amp=0.0, axis="Z"))
    ob = K.merge(parts, "porch_lantern")
    socket("light", (0, cy, -0.02), ob)
    return finish([ob], "props/porch_lantern", "porch_lantern", ao_radius=0.1, ao_strength=0.6, ground_ao=False)


def string_bulb():
    K.reset()
    G = "glow_lantern"
    ob = lathe("string_bulb", [(0.0, -0.1), (0.03, -0.094), (0.043, -0.072), (0.039, -0.048), (0.02, -0.032),
                               (0.02, -0.006), (0.0, 0.0)], [G, G, G, G, B, B], ["flat"] * 4 + ["metal", "metal"],
               segments=8)
    return finish([ob], "props/string_bulb", "string_bulb", ao_radius=0.03, ao_strength=0.4, ground_ao=False)


if want("lamp_post"):
    lamp_post()
if want("paper_lantern"):
    paper_lantern()
if want("porch_lantern"):
    porch_lantern()
if want("string_bulb"):
    string_bulb()
