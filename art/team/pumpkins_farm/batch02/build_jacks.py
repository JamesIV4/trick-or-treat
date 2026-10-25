"""Five jack-o'-lanterns: happy, wink, surprised, toothy, sleepy (batch02, textured).

blender --background --factory-startup --python art/team/pumpkins_farm/batch02/build_jacks.py [-- ids...]

Faces are carved with F.carve: Exact-boolean pockets. From the surface inward each pocket has a
pumpkin_dark rind band, a glowing inner wall band and a glowing floor (glow cells are emissive and
flicker in game). Features are big and simple so they read at 15 m at night. A glowing zig-zag lid
seam rings the stem (light leaking out of the cut lid).
"""
import math
import pathlib
import sys

sys.path.insert(0, str(pathlib.Path(__file__).resolve().parent))
import bpy  # noqa: E402
from mathutils import Vector  # noqa: E402
from mathutils.bvhtree import BVHTree  # noqa: E402

import farm_lib as F  # noqa: E402

K = F.K
GLOW = "glow_jack"
HOT = "glow_jack_hot"
EX = 0.102  # eye x offset


def lid_seam(body, top, radius=0.1, n=22, zig=0.012, color=GLOW, tube=0.009):
    """Zig-zag lid cut ringing the stem, following the surface (rays straight down)."""
    bpy.context.view_layer.update()
    dg = bpy.context.evaluated_depsgraph_get()
    tree = BVHTree.FromObject(body, dg)
    pts = []
    for i in range(n + 1):
        a = 2 * math.pi * i / n
        r = radius + (zig if i % 2 else -zig)
        x, y = top.x + math.cos(a) * r, top.y + math.sin(a) * r
        loc, nrm, _, _ = tree.ray_cast(Vector((x, y, top.z + 1.0)), Vector((0, 0, -1)))
        pts.append(loc + nrm * 0.002)
    return F.sweep("lid", pts, tube, sides=3, color=color, mat="flat", cap_start=False, cap_end=False,
                   smooth=False)


def jack(name, features, width=0.55, height=0.46, stem_h=0.16, stem_lean=200, stem_color="stem_brown",
         seed=1, lean=(0.0, 0.0), body_kw=None, depth=0.065, inset=0.02, lid_r=None):
    K.reset()
    kw = dict(lobes=8, spl=4, rings=12, groove=0.12, plump=0.45, fullness=0.75, flatness=0.72,
              top_dimple=0.06, dimple_w=0.42, lumps=0.035, seed=seed, belly=1.15, lean=lean, rot_z=-90.0)
    kw.update(body_kw or {})
    body = F.pumpkin_body("body", width=width, height=height, **kw)
    top = Vector(body["top"])
    # flatten the face side a little so both eyes face the camera at 3/4 views
    K.deform(body, lambda co: Vector((co.x, co.y * (0.84 + 0.16 * min(1.0, abs(co.x) / (width * 0.5)) ** 2)
                                      if co.y < 0 else co.y, co.z)))
    F.carve(body, features, depth=depth, inset=inset)
    parts = [body]
    parts.append(lid_seam(body, top, radius=lid_r or width * 0.19))
    parts.append(F.stem("stem", (top.x, top.y, top.z), height=stem_h, radius=0.038, color=stem_color,
                        curl=0.09, lean_dir=math.radians(stem_lean), rings=7))
    ob = K.merge(parts, name)
    K.socket("light", (0, 0, height * 0.42), ob)
    return F.finish([ob], f"props/{name}", name)


def happy():
    feats = [{"shape": F.convex_shape(F.rounded_triangle(s * EX, 0.29, 0.15, 0.13, tilt=-6 * s, round_=0.2,
                                                         arc_pts=1)), "color": GLOW} for s in (-1, 1)]
    feats.append({"shape": F.shape_of(F.grin, cx=0.0, cz=0.185, w=0.36, top_sag=0.04, depth=0.14, n=13),
                  "color": GLOW, "inset": 0.016, "rim_w": 0.01})
    return jack("jackolantern_happy", feats, seed=2)


def wink():
    feats = [{"shape": F.convex_shape(F.rounded_triangle(-EX, 0.295, 0.145, 0.13, tilt=6, round_=0.2, arc_pts=1)),
              "color": GLOW},
             {"shape": F.shape_of(F.arc_band, cx=EX, cz=0.275, w=0.16, h=0.05, thick=0.06, n=9, up=True),
              "color": GLOW, "inset": 0.016, "rim_w": 0.008},
             {"shape": F.shape_of(F.grin, cx=0.015, cz=0.18, w=0.34, top_sag=0.02, depth=0.12, n=13,
                                  lift=(0.0, 0.05)), "color": GLOW, "inset": 0.016, "rim_w": 0.01}]
    return jack("jackolantern_wink", feats, seed=5, stem_lean=-20)


def surprised():
    feats = [{"shape": F.convex_shape(F.circle(s * 0.095, 0.36, 0.056, n=12, sz=1.15)), "color": GLOW}
             for s in (-1, 1)]
    feats.append({"shape": F.convex_shape(F.circle(0.0, 0.195, 0.068, n=14, sz=1.25)), "color": GLOW})
    return jack("jackolantern_surprised", feats, width=0.5, height=0.56, seed=9, stem_h=0.15,
                body_kw=dict(fullness=0.85, flatness=0.65, belly=1.08, rings=13, groove=0.11))


def toothy():
    feats = [{"shape": F.convex_shape(F.rounded_triangle(s * EX, 0.295, 0.14, 0.12, tilt=-4 * s, round_=0.2,
                                                         arc_pts=1)), "color": GLOW} for s in (-1, 1)]
    feats.append({"shape": F.shape_of(F.grin, cx=0.0, cz=0.205, w=0.37, top_sag=0.025, depth=0.16, n=13,
                                      teeth=(-0.06, 0.06), tooth_w=0.055, tooth_h=0.05), "color": HOT, "inset": 0.016, "rim_w": 0.01,
                  "wall": HOT})
    return jack("jackolantern_toothy", feats, seed=13, stem_lean=160)


def sleepy():
    feats = [{"shape": F.shape_of(F.arc_band, cx=s * EX, cz=0.245, w=0.16, h=0.04, thick=0.062, n=9, up=False),
              "color": GLOW, "inset": 0.016, "rim_w": 0.008} for s in (-1, 1)]
    feats.append({"shape": F.shape_of(F.grin, cx=0.0, cz=0.165, w=0.25, top_sag=0.015, depth=0.1, n=11),
                  "color": GLOW, "inset": 0.014, "rim_w": 0.008})
    return jack("jackolantern_sleepy", feats, width=0.58, height=0.38, seed=17, stem_h=0.14, stem_lean=110,
                body_kw=dict(fullness=0.68, flatness=0.7, belly=1.2, top_dimple=0.05, rings=11))


if __name__ == "__main__":
    F.run({"happy": happy, "wink": wink, "surprised": surprised, "toothy": toothy, "sleepy": sleepy})
