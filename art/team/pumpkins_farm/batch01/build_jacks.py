"""Five jack-o'-lanterns: happy, wink, surprised, toothy, sleepy.

blender --background --factory-startup --python art/team/pumpkins_farm/batch01/build_jacks.py [-- ids...]

Faces are carved with P.carve: Exact-boolean pockets, tapered so a pumpkin_dark rim frames a
glow_jack floor (emissive in game). Everything is sized to read from 10-15 m.
"""
import math
import pathlib
import sys

HERE = pathlib.Path(__file__).resolve().parent
sys.path.insert(0, str(HERE))
import bpy  # noqa: E402

import pumpkin_lib as P  # noqa: E402

K = P.K
GLOW = "glow_jack"
HOT = "glow_jack_hot"


def jack(name, features, width=0.55, height=0.46, stem_h=0.17, stem_lean=200, stem_color="stem_brown",
         seed=1, lean=(0.0, 0.0), body_kw=None, depth=0.05, inset=0.014):
    K.reset()
    kw = dict(lobes=7, groove=0.15, groove_w=0.08, peaks=2, rings=6, fullness=0.8, flatness=0.72,
              top_dimple=0.07, lumps=0.035, seed=seed, belly=1.2, lean=lean)
    kw.update(body_kw or {})
    body = P.pumpkin_body("body", width=width, height=height, **kw)
    per = 2 + kw["peaks"]
    gw_deg = 360.0 / kw["lobes"] * kw["groove_w"]
    body.rotation_euler.z = math.radians(-90 - gw_deg / 2)  # a groove runs down the middle of the face
    bpy.context.view_layer.update()
    # bake rotation so the carve rays work in world space
    body = K.merge([body], "body")
    body["top"] = (lean[0] * height, lean[1] * height, 0)
    top = max((v.co for v in body.data.vertices if abs(v.co.x - lean[0] * v.co.z) < 1e-3 and
               abs(v.co.y - lean[1] * v.co.z) < 1e-3 and v.co.z > height * 0.5), key=lambda c: c.z)
    P.carve(body, features, depth=depth, inset=inset)
    parts = [body]
    parts += P.curly_stem("stem", (top.x, top.y, top.z), height=stem_h, radius=0.042, color=stem_color,
                          curl=0.08, lean_dir=math.radians(stem_lean), sides=5, pts=4, flare=False)
    ob = K.merge(parts, name)
    K.socket("light", (0, 0, height * 0.42), ob)
    return P.finish([ob], f"props/{name}", name, close=True)


def mirror(poly):
    return [(-x, z) for x, z in reversed(poly)]


def eyes(shape_fn):
    """Feature pair from a function x_sign -> carve shape."""
    return [shape_fn(-1), shape_fn(1)]


def happy():
    feats = [{"shape": P.convex_shape(P.rounded_triangle(s * 0.11, 0.285, 0.14, 0.125, tilt=-6 * s, round_=0.16)),
              "color": GLOW} for s in (-1, 1)]
    feats.append({"shape": P.shape_of(P.grin, cx=0.0, cz=0.19, w=0.36, top_sag=0.035, depth=0.13, n=9),
                  "color": GLOW})
    return jack("jackolantern_happy", feats, seed=2)


def wink():
    feats = [{"shape": P.convex_shape(P.rounded_triangle(-0.11, 0.29, 0.135, 0.125, tilt=6, round_=0.16)),
              "color": GLOW},
             {"shape": P.shape_of(P.arc_band, cx=0.11, cz=0.27, w=0.15, h=0.045, thick=0.052, n=7, up=True),
              "color": GLOW, "inset": 0.009},
             {"shape": P.shape_of(P.grin, cx=0.015, cz=0.18, w=0.33, top_sag=0.02, depth=0.11, n=9,
                                  lift=(0.0, 0.05)), "color": GLOW}]
    return jack("jackolantern_wink", feats, seed=5, stem_lean=-20)


def surprised():
    feats = [{"shape": P.convex_shape(P.circle(s * 0.1, 0.36, 0.052, n=10, sz=1.15)), "color": GLOW}
             for s in (-1, 1)]
    feats.append({"shape": P.convex_shape(P.circle(0.0, 0.2, 0.062, n=10, sz=1.25)), "color": GLOW})
    return jack("jackolantern_surprised", feats, width=0.5, height=0.56, seed=9, stem_h=0.16,
                body_kw=dict(fullness=0.85, flatness=0.65, belly=1.1, rings=7, peaks=1, groove=0.14,
                             groove_w=0.1))


def toothy():
    feats = [{"shape": P.convex_shape(P.rounded_triangle(s * 0.11, 0.29, 0.13, 0.11, tilt=-4 * s, round_=0.18)),
              "color": GLOW} for s in (-1, 1)]
    feats.append({"shape": P.shape_of(P.grin, cx=0.0, cz=0.2, w=0.37, top_sag=0.025, depth=0.15, n=9,
                                      teeth=(-0.055, 0.055), tooth_w=0.05, tooth_h=0.045), "color": HOT})
    return jack("jackolantern_toothy", feats, seed=13, stem_lean=160)


def sleepy():
    feats = [{"shape": P.shape_of(P.arc_band, cx=s * 0.11, cz=0.25, w=0.15, h=0.035, thick=0.055, n=7, up=False),
              "color": GLOW, "inset": 0.011} for s in (-1, 1)]
    feats.append({"shape": P.shape_of(P.grin, cx=0.0, cz=0.165, w=0.2, top_sag=0.015, depth=0.07, n=7),
                  "color": GLOW, "inset": 0.012})
    return jack("jackolantern_sleepy", feats, width=0.58, height=0.38, seed=17, stem_h=0.15, stem_lean=110,
                body_kw=dict(fullness=0.7, flatness=0.7, belly=1.25, top_dimple=0.06))


BUILDS = {"happy": happy, "wink": wink, "surprised": surprised, "toothy": toothy, "sleepy": sleepy}

if __name__ == "__main__":
    args = sys.argv[sys.argv.index("--") + 1:] if "--" in sys.argv else list(BUILDS)
    for a in args:
        BUILDS[a]()
