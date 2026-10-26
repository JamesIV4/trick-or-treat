"""Play props batch03: fire_wand, the dragon kid's storybook fire wand (held in the LEFT hand).

    blender --background --factory-startup --python build_wand.py

Contract (the game attaches it by these):
  * built along local +Z, then turned so the wand points along Blender -Y = glTF/Godot +Z (front)
  * origin = centre of the leather grip (where the hand closes)
  * socket_tip = centre of the glowing flame crystal, Godot (0, 0, 0.30)

Parts, butt to tip: gold pommel ball, wrapped leather grip (tilted wraps), gold collar, twisted
two-tone dark-wood shaft, scalloped gold cup, faceted flame crystal (glow_jack_hot core, glow_jack licks,
swirling three-lobed, tip curling up). Reuses yard_street/batch02/yard_common.py (lathe, sweep, clean).
"""
import pathlib
import sys

HERE = pathlib.Path(__file__).resolve().parent
sys.path.insert(0, str(HERE.parents[1] / "yard_street" / "batch02"))
from yard_common import *  # noqa: E402,F401,F403
from yard_common import clean  # noqa: E402

import bmesh  # noqa: E402

TIP_Z = 0.30   # socket_tip along the wand axis (Godot +Z)


def uv_scale(ob, s):
    """Material UVs are in metres; a 4 cm wand needs denser tiling for the grain to show."""
    uv = ob.data.uv_layers.get("UVMap")
    for d in uv.data:
        d.uv = (d.uv[0] * s, d.uv[1] * s)
    return ob


def polar_deform(ob, fn):
    """fn(r, a, z) -> (r, a, z) on every vertex (around local Z)."""
    def f(co):
        r = math.hypot(co.x, co.y)
        a = math.atan2(co.y, co.x)
        r, a, z = fn(r, a, co.z)
        return Vector((math.cos(a) * r, math.sin(a) * r, z))
    return K.deform(ob, f)


def smooth01(x):
    x = max(0.0, min(1.0, x))
    return x * x * (3 - 2 * x)


def fire_wand():
    K.reset()
    parts = []

    # ---- handle: gold pommel ball, wrapped leather grip, gold collar (one lathe, no hidden caps)
    GOLD, LEA, LEA_D = "foil_gold", "leather", "bark_dark"
    prof = [
        (0.0, -0.066),     # pommel pole
        (0.0105, -0.0645),
        (0.0180, -0.0590),
        (0.0200, -0.0515),  # pommel widest
        (0.0170, -0.0445),
        (0.0122, -0.0410),  # neck
        (0.0150, -0.0375),  # grip start
        (0.0170, -0.0270),  # wrap
        (0.0152, -0.0150),  # seam
        (0.0172, -0.0030),  # wrap
        (0.0152, 0.0090),   # seam
        (0.0170, 0.0210),   # wrap
        (0.0145, 0.0330),   # grip end
        (0.0178, 0.0360),   # collar
        (0.0208, 0.0420),
        (0.0205, 0.0505),
        (0.0160, 0.0555),
    ]
    bands = len(prof) - 1
    cols = [GOLD] * 5 + [LEA] * 7 + [GOLD] * (bands - 12)
    mats = ["metal"] * 5 + ["vinyl"] * 7 + ["metal"] * (bands - 12)
    cols[5] = LEA_D  # dark binding at the grip start
    cols[11] = LEA_D  # and at the grip end
    cols[7] = cols[9] = "cinnamon"  # lighter strap turns between the darker ones (reads as a wound wrap)
    handle = lathe("handle", prof, cols, mats, segments=8, smooth=True, cap_bottom=False, cap_top=True,
                   cap_colors=(GOLD, GOLD), cap_mats=("metal", "metal"), phase=math.pi / 8)

    # tilt the inner grip rings so the wraps read as a strap wound around the handle
    def wrap_tilt(r, a, z):
        if -0.030 < z < 0.025:
            z += 0.0042 * math.sin(a)
        return r, a, z
    polar_deform(handle, wrap_tilt)
    uv_scale(handle, 4.0)
    parts.append(handle)

    # ---- shaft: twisted three-lobed dark wood, two-tone strands
    z0, z1 = 0.050, 0.262
    n = 9
    pts = [(0, 0, z0 + (z1 - z0) * i / (n - 1)) for i in range(n)]
    radii = [x * 1.12 for x in (0.0136, 0.0132, 0.0126, 0.0120, 0.0116, 0.0114, 0.0117, 0.0124, 0.0132)]
    step = 2 * math.pi / 6

    def lobes(i, a):
        j = int(round(a / step)) % 6
        return 1.24 if j % 2 == 0 else 0.80

    shaft = sweep("shaft", pts, radii, color="bark", mat="bark", sides=6, smooth=True, caps=(False, False),
                  shape=lobes, colors=lambda i, j: ("wood_dark", "bark") if j % 2 else None,
                  up_hint=(1, 0, 0))
    twist = math.radians(210.0)

    def twist_fn(r, a, z):
        return r, a + twist * (z - z0) / (z1 - z0), z
    polar_deform(shaft, twist_fn)
    uv_scale(shaft, 5.0)
    parts.append(shaft)

    # ---- gold cup with three scallops cradling the crystal
    cup_prof = [(0.0090, 0.2385), (0.0170, 0.2470), (0.0250, 0.2575), (0.0292, 0.2670), (0.0270, 0.2725),
                (0.0215, 0.2690)]
    # darker gold than the collars so the cup separates from the yellow core of the flame (pass 4)
    cup = lathe("cup", cup_prof, "foil_gold_dark", "metal", segments=9, smooth=True, cap_bottom=False, cap_top=False)

    def scallop(r, a, z):
        if z > 0.262:
            k = math.cos(3 * a)
            z += 0.0085 * k
            r *= 1.0 + 0.07 * k
        return r, a, z
    polar_deform(cup, scallop)
    uv_scale(cup, 3.0)
    parts.append(cup)

    # ---- flame crystal: faceted, swirling three lobes, tip curls up (local +Y -> Godot +Y)
    HOT, JACK = "glow_jack_hot", "glow_jack"
    fz0, fz1 = 0.252, 0.362
    fprof = [(0.0, fz0), (0.0165, 0.2575), (0.0275, 0.2690), (0.0340, 0.2840), (0.0345, 0.3010),
             (0.0295, 0.3170), (0.0210, 0.3310), (0.0110, 0.3450), (0.0, fz1)]
    RED = "glow_jack_shell"
    # Emission is cell x 2.6 then tonemapped: glow_jack reads lemon-yellow (passes 1-2) and
    # glow_lantern_red reads peach-white (pass 3). glow_jack_shell (the jack-o'-lantern shell cell)
    # is the one that glows a rich orange in engine.
    # Gradient: white-hot core -> yellow (glow_jack) -> orange tongues (glow_jack_shell) at the tip,
    # with orange licks running down the lobe crests into the yellow middle.
    fcols = [HOT, HOT, HOT, JACK, JACK, RED, RED, RED]
    licks = {3: [RED if j % 3 == 0 else JACK for j in range(9)],
             4: [RED if j % 3 == 0 else JACK for j in range(9)],
             5: [JACK if j % 3 == 1 else RED for j in range(9)]}
    flame = lathe("flame", fprof, fcols, "flat", segments=9, smooth=False, cap_bottom=False, cap_top=False,
                  band_cols=licks)

    def flame_shape(r, a, z):
        h = (z - fz0) / (fz1 - fz0)
        r *= 1.0 + 0.13 * math.cos(3 * a) * math.sin(math.pi * h)
        a += math.radians(70.0) * h
        return r, a, z
    polar_deform(flame, flame_shape)

    def curl(co):
        c = smooth01((co.z - 0.300) / 0.062)
        return Vector((co.x, co.y + 0.017 * c * c, co.z - 0.003 * c))
    K.deform(flame, curl)
    parts.append(flame)

    body = K.merge(parts, "fire_wand")
    # wand axis +Z -> Blender -Y (glTF / Godot +Z, the asset front); local +Y -> Blender +Z (up)
    body.data.transform(Matrix.Rotation(math.radians(90.0), 4, "X"))
    body.data.update()
    clean(body)

    socket("tip", (0.0, -TIP_Z, 0.0), body)
    K.bake_ao([body], samples=32, radius=0.05, strength=0.7, ground=False)
    out = K.export([body], "props/fire_wand")
    tris = K.tri_count([body])
    sh = K.review_sheet([body], "fire_wand")
    vs = [v.co for v in body.data.vertices]
    lo = Vector((min(v.x for v in vs), min(v.y for v in vs), min(v.z for v in vs)))
    hi = Vector((max(v.x for v in vs), max(v.y for v in vs), max(v.z for v in vs)))
    print(f"ASSET fire_wand tris={tris} blender_min={tuple(round(x, 4) for x in lo)} "
          f"blender_max={tuple(round(x, 4) for x in hi)} gltf={out} sheet={sh}")
    return tris


if want("fire_wand"):
    fire_wand()
