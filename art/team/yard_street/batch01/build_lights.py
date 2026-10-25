"""Light props: lamp_post, paper_lantern, porch_lantern, string_bulb."""
import pathlib
import sys

sys.path.insert(0, str(pathlib.Path(__file__).resolve().parent))
from yard_common import K, arc, finish, lathe, socket, want  # noqa: E402


def panes(n, frame_deg, glass, frame="lamp_black", offset=0.0):
    """Column angles + colours for a lathe band: n glass panes split by thin frame mullions."""
    angles, cols = [], []
    for i in range(n):
        a = offset + i * 360.0 / n
        angles += [a - frame_deg / 2, a + frame_deg / 2]
        cols += [frame, glass]
    return angles, cols


def lamp_post():
    K.reset()
    parts = []
    parts.append(lathe("base", [(0.28, 0.0), (0.28, 0.11), (0.2, 0.17), (0.13, 0.4), (0.095, 0.5)],
                       "lamp_black", segments=8, cap_bottom=False, phase=0.39))
    parts.append(K.cylinder("pole", 0.08, 2.45, color="lamp_black", verts=8, loc=(0, 0, 0.46 + 2.45 / 2),
                            radius_top=0.065, caps=False))
    parts.append(lathe("collar", [(0.075, 1.3), (0.11, 1.34), (0.11, 1.41), (0.07, 1.45)], "lamp_black",
                       segments=8, cap_bottom=False, cap_top=False, phase=0.39))
    # shepherd's crook over the top, then a short drop to the lantern hook
    lx, hz = 0.66, 2.98
    crook = [(0.0, 0, 2.85)] + arc((lx / 2, 0, 3.02), lx / 2, 180, 0, 8) + [(lx, 0, hz)]
    parts.append(K.tube("crook", crook, radius=0.06, radius_end=0.045, color="lamp_black", sides=6, smooth=False))
    # curly flourish inside the crook
    curl = arc((0.27, 0, 2.97), 0.22, 195, 520, 9, radius_end=0.06)
    parts.append(K.tube("curl", curl, radius=0.032, radius_end=0.024, color="lamp_black", sides=5, smooth=False))
    # lantern head hanging from the crook (profiles relative to the hook, scaled up for readability)
    s = 1.3
    head = dict(loc=(lx, 0, hz), scale=(s, s, s))
    ang, cols = panes(6, 9, "glow_lamp", offset=30)
    parts.append(lathe("roof", [(0.2, -0.21), (0.22, -0.18), (0.05, -0.05), (0.0, 0.01)], "lamp_black",
                       segments=6, cap_bottom=True, **head))
    parts.append(lathe("glass", [(0.12, -0.56), (0.165, -0.21)], "glow_lamp", angles=ang, band_cols={0: cols},
                       cap_bottom=False, cap_top=False, **head))
    parts.append(lathe("bottom", [(0.0, -0.68), (0.05, -0.65), (0.14, -0.58), (0.14, -0.55)], "lamp_black",
                       segments=6, cap_top=True, cap_colors=("lamp_black", "glow_lamp"), **head))
    post =K.merge(parts, "lamp_post")
    socket("light", (lx, 0, hz - 0.38 * s), post)
    return finish([post], "props/lamp_post", "lamp_post")


def paper_lantern():
    K.reset()
    G, R, D = "glow_lantern", "wood_dark", "lamp_black"
    prof = [(0.0, -0.44), (0.06, -0.425), (0.1, -0.395), (0.152, -0.335), (0.164, -0.318), (0.178, -0.245),
            (0.178, -0.22), (0.162, -0.14), (0.154, -0.125), (0.1, -0.075), (0.05, -0.055), (0.0, 0.0)]
    cols = [D, D, G, R, G, R, G, R, G, D, D]
    body = lathe("paper_lantern", prof, cols, segments=10, phase=0.1)
    socket("hang", (0, 0, 0), body)
    socket("light", (0, 0, -0.24), body)
    return finish([body], "props/paper_lantern", "paper_lantern")


def porch_lantern():
    K.reset()
    parts = []
    parts.append(K.box("plate", (0.13, 0.03, 0.24), color="lamp_black", loc=(0, -0.015, 0.0)))
    parts.append(K.box("arm", (0.045, 0.1, 0.045), color="lamp_black", loc=(0, -0.075, -0.03)))
    cy = -0.165
    ang, cols = panes(4, 10, "glow_porch", offset=45)
    parts.append(lathe("base", [(0.0, -0.2), (0.04, -0.18), (0.095, -0.15), (0.095, -0.12)], "lamp_black",
                       segments=4, phase=0.785, cap_top=True, cap_colors=("lamp_black", "glow_porch"), loc=(0, cy, 0)))
    parts.append(lathe("glass", [(0.08, -0.12), (0.095, 0.1)], "glow_porch", angles=ang, band_cols={0: cols},
                       cap_bottom=False, cap_top=False, loc=(0, cy, 0)))
    parts.append(lathe("roof", [(0.13, 0.095), (0.135, 0.12), (0.03, 0.19), (0.03, 0.205), (0.0, 0.22)],
                       "lamp_black", segments=4, phase=0.785, cap_bottom=True, loc=(0, cy, 0)))
    ob = K.merge(parts, "porch_lantern")
    socket("light", (0, cy, -0.01), ob)
    return finish([ob], "props/porch_lantern", "porch_lantern")


def string_bulb():
    K.reset()
    G = "glow_lantern"
    ob = lathe("string_bulb", [(0.0, -0.1), (0.04, -0.083), (0.043, -0.048), (0.02, -0.028), (0.0, 0.0)],
               [G, G, G, "lamp_black"], segments=6)
    return finish([ob], "props/string_bulb", "string_bulb")


if want("lamp_post"):
    lamp_post()
if want("paper_lantern"):
    paper_lantern()
if want("porch_lantern"):
    porch_lantern()
if want("string_bulb"):
    string_bulb()
