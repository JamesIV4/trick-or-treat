"""Fidelity pass on street props seen constantly: mailbox, parked_car, flower_pot (bench reviewed, unchanged).
Ids, origins, sizes and sockets are kept from yard_street/batch02.

    blender -b --factory-startup --python build_street.py -- mailbox parked_car flower_pot
"""
import pathlib
import random
import sys

sys.path.insert(0, str(pathlib.Path(__file__).resolve().parent))
from lm_common import *  # noqa: E402,F401,F403
from lm_common import ARGS, ellipse, stroke, ribbed  # noqa: E402
import yard_common as YC  # noqa: E402

B02 = pathlib.Path(__file__).resolve().parents[1].parent / "yard_street" / "batch02"


def import_batch02(modname):
    """Import a batch02 builder without triggering its module-level builds."""
    saved = YC.ARGS
    YC.ARGS = ["__import_only__"]
    try:
        sys.path.insert(0, str(B02))
        mod = __import__(modname)
    finally:
        YC.ARGS = saved
    return mod


# ------------------------------------------------------------------ mailbox
def mailbox():
    F = import_batch02("build_furniture")
    K.reset()
    parts = []
    # white-painted post (matches the picket fences), cap block, chamfered foot
    parts.append(box("post", (0.12, 0.12, 0.98), "trim_white", "wood", loc=(0, 0.04, 0.49), bevel=0.02, segments=1))
    parts.append(box("cap_board", (0.22, 0.5, 0.045), "wood_dark", "wood", loc=(0, 0.0, 0.995), bevel=0.012, segments=1))
    parts.append(rod("brace", (0, 0.1, 0.72), (0, 0.2, 0.97), 0.025, "trim_white", mat="wood", sides=6))
    W, H, Z = 0.3, 0.13, 1.018
    outline = F.d_outline(W, H)
    secs = []
    for y, k in [(-0.262, 0.97), (-0.258, 1.035), (-0.232, 1.035), (-0.228, 1.0), (0.235, 1.0), (0.245, 0.96)]:
        secs.append((y, [(x * k, Z + z * k - (k - 1.0) * 0.004) for x, z in outline]))
    body = loft("box", secs, "mailbox_black", mat="metal")
    parts.append(body)
    paint_world(body, "iron", lambda c, n: -0.259 < c.y < -0.231)
    # (embossed ribs tried in pass 1: +180 tris, invisible at gameplay distance -> dropped)
    for yr in ():
        rs = [(yr - 0.012, 1.0), (yr - 0.006, 1.03), (yr + 0.006, 1.03), (yr + 0.012, 1.0)]
        rib = loft(f"rib{yr}", [(y, [(x * k, Z + z * k - (k - 1.0) * 0.004) for x, z in outline]) for y, k in rs],
                   "iron", mat="metal", caps=False)
        parts.append(rib)
    parts.append(blob("handle", 0.022, "foil_gold", mat="metal", loc=(0, -0.272, Z + H + 0.1), scale=(1.4, 0.8, 0.8),
                      segments=6, rings=4, noise_amp=0.0, axis="X"))
    parts.append(rod("hinge", (-0.08, -0.262, Z + 0.012), (0.08, -0.262, Z + 0.012), 0.012, "iron", sides=5))
    fx = W / 2 + 0.018
    parts.append(box("flag_arm", (0.016, 0.03, 0.27), "metal_red", "metal", loc=(fx, 0.09, Z + 0.17), rot=(-6, 0, 0),
                     bevel=0.006, segments=1))
    parts.append(box("flag", (0.018, 0.13, 0.085), "metal_red", "metal", loc=(fx, 0.04, Z + 0.28), rot=(-6, 0, 0),
                     bevel=0.008, segments=1))
    parts.append(rod("pivot", (W / 2 - 0.005, 0.1, Z + 0.06), (W / 2 + 0.035, 0.1, Z + 0.06), 0.022, "foil_gold",
                     sides=6))
    # brass house-number plate (cream glared at night) with three dark number dots
    parts.append(box("plate", (0.006, 0.12, 0.06), "caramel", "metal", loc=(-W / 2 - 0.004, -0.06, Z + 0.075),
                     bevel=0.003, segments=1))
    for i, yy in enumerate(()):
        parts.append(box(f"digit{i}", (0.004, 0.016, 0.034), "choco_dark", "flat", loc=(-W / 2 - 0.008, yy, Z + 0.075)))
    # grass tuft + little flowers + a tiny pumpkin at the foot of the post
    tuft = blob("tuft", 0.15, "hedge", mat="hedge", loc=(0.0, 0.03, 0.05), scale=(1.15, 1.0, 0.75), segments=7, rings=4,
                lobes=6, lobe_amp=0.22, lobe_pow=3, noise_amp=0.06, noise_freq=3.0, seed=3, axis="Z", uvmode="box")
    cut_below(tuft, 0.0)
    paint_world(tuft, "hedge_dark", lambda c, n: c.z < 0.03)
    parts.append(tuft)
    for i, (x, y, col) in enumerate([(-0.11, -0.07, "candy_corn_white"), (0.12, 0.1, "banner_purple")]):
        parts += flower(f"fl{i}", (x, y, 0.13 + 0.01 * i), 0.05, col)
    pk = ribbed("pk", 0.08, 0.11, "pumpkin", ribs=4, depth=0.08, rings=4, sides=8, loc=(0.1, -0.17, -0.004))
    parts.append(pk)
    parts.append(rod("pkst", (0.1, -0.17, 0.085), (0.11, -0.165, 0.12), 0.012, "stem_green", mat="wood", sides=4,
                     chamfer=0))
    ob = K.merge(parts, "mailbox")
    ob.rotation_euler.y = math.radians(1.5)
    bake(ob)
    ground(ob)
    return finish([ob], "props/mailbox", "mailbox", ao_radius=0.3)


def flower(name, loc, r, col, center="sunflower_center", petals=5):
    """Flat daisy-like head: petal star prism + domed centre."""
    x, y, z = loc
    n = petals * 2
    outline = []
    for k in range(n):
        a = 2 * math.pi * k / n
        rr = r if k % 2 == 0 else r * 0.55
        outline.append((math.cos(a) * rr, math.sin(a) * rr))
    pet = K.polygon_prism(f"{name}_p", outline, 0.012, color=col, mat="flat")
    transform(pet, loc=(x, y, z - 0.006), rot=(0, 0, (hash(name) % 60)))
    c = blob(f"{name}_c", r * 0.35, center, mat="flat", loc=(x, y, z + 0.008), scale=(1, 1, 0.6), segments=5, rings=3,
             noise_amp=0.0, axis="Z")
    return [pet, c]


# ------------------------------------------------------------------ parked car
def parked_car():
    C = import_batch02("build_car")

    def wheel(name, x, y, r=0.42, wdt=0.36):
        sx = 1 if x > 0 else -1
        parts = []
        prof = [(0.22, -wdt / 2), (r - 0.045, -wdt / 2), (r, -wdt / 2 + 0.075), (r, wdt / 2 - 0.075),
                (r - 0.045, wdt / 2), (0.22, wdt / 2)]
        # whitewall stripe on the outer sidewall (lathe +Z end ends up at +X after the rotation)
        cols = ["charcoal"] * 5
        tyre = lathe(f"{name}_tyre", prof, cols, "flat", segments=16, cap_bottom=True, cap_top=True,
                     cap_colors=("charcoal", "charcoal"), phase=0.1)
        transform(tyre, loc=(x, y, r), rot=(0, 90, 0))
        parts.append(tyre)
        # thin whitewall ring at the rim of the hubcap, silver dish, red centre button
        hub = lathe(f"{name}_hub", [(0.27, 0.0), (0.245, 0.018), (0.2, 0.04), (0.1, 0.07), (0.0, 0.082)],
                    ["lolly_white", "iron", "silver", "metal_red"], "metal", segments=14, cap_bottom=False)
        transform(hub, loc=(x + sx * (wdt / 2 - 0.005), y, r), rot=(0, 90 * sx, 0))
        parts.append(hub)
        return parts

    C.wheel = wheel
    # trim a few hidden-ish tris to pay for the rounder wheels
    orig_blob = C.blob

    def cheaper_blob(name, radius, color, **kw):
        if name.startswith("headlight"):
            kw.update(segments=10, rings=5)
        if name.startswith("mirror"):
            kw.update(segments=7, rings=4)
        return orig_blob(name, radius, color, **kw)

    C.blob = cheaper_blob
    return C.parked_car()


# ------------------------------------------------------------------ flower pot
def flower_pot():
    K.reset()
    T, CL = "terracotta", "clay"
    prof = [(0.0, 0.0), (0.15, 0.0), (0.162, 0.012), (0.2, 0.24), (0.206, 0.255), (0.24, 0.262), (0.252, 0.285),
            (0.252, 0.325), (0.24, 0.345), (0.222, 0.342), (0.214, 0.31)]
    cols = [CL, CL, T, CL, T, T, T, T, T, CL]
    pot = lathe("pot", prof, cols, ["stucco"] * 10, segments=12, cap_bottom=False, cap_top=False, phase=0.2)
    soil = lathe("soil", [(0.214, 0.31), (0.12, 0.318), (0.0, 0.322)], "choco_dark", "soil", segments=12,
                 cap_bottom=False, cap_top=False, phase=0.2)
    parts = [pot, soil]
    # low leafy cushion under the heads (fills the gaps, darker underneath)
    mound = blob("mound", 0.2, "hedge", mat="leaves", loc=(0, 0, 0.35), scale=(1.08, 1.08, 0.42), segments=10, rings=5,
                 lobes=7, lobe_amp=0.2, lobe_pow=4, noise_amp=0.05, noise_freq=3.0, seed=5, axis="Z", uvmode="box")
    cut_below(mound, 0.318)
    paint_world(mound, "hedge_dark", lambda c, n: n.z < 0.2)
    parts.append(mound)
    # mum pompoms: one big centre head and four around it, petal bumps, darker undersides
    heads = [(0.0, 0.0, 0.47, 0.125, "maple_orange"), (0.13, 0.06, 0.425, 0.105, "golden_leaf"),
             (-0.1, 0.11, 0.425, 0.1, "mum_orange"), (-0.1, -0.1, 0.42, 0.1, "golden_leaf"),
             (0.09, -0.12, 0.415, 0.095, "banner_purple")]
    dark = {"maple_orange": "rust_leaf", "golden_leaf": "burnt_amber", "mum_orange": "rust_leaf",
            "banner_purple": "witch_purple"}
    for i, (x, y, z, r, col) in enumerate(heads):
        h = blob(f"mum{i}", r, col, mat="felt", loc=(x, y, z), scale=(1.0, 1.0, 0.78), segments=10, rings=5, lobes=10,
                 lobe_amp=0.11, lobe_pow=6, noise_amp=0.03, noise_freq=5.0, seed=20 + i, axis="Z", uvmode="box")
        paint_world(h, dark[col], lambda c, n: n.z < -0.25)
        parts.append(h)
    cull_hidden(parts[3:])
    # a few pointed leaves spilling over the rim
    for i, a in enumerate((20, 140, 250, 320)):
        ar = math.radians(a)
        pts = [(math.cos(ar) * 0.17, math.sin(ar) * 0.17, 0.36), (math.cos(ar) * 0.26, math.sin(ar) * 0.26, 0.35),
               (math.cos(ar) * 0.31, math.sin(ar) * 0.31, 0.3)]
        parts.append(stroke(f"leaf{i}", pts, 0.08, "vine", mat="leaves", thick=0.014, taper=(0.5, 0.05), sides=4, z=None))
    ob = K.merge(parts, "flower_pot")
    return finish([ob], "props/flower_pot", "flower_pot", ao_radius=0.25)

if want("mailbox"):
    mailbox()
if want("parked_car"):
    parked_car()
if want("flower_pot"):
    flower_pot()
