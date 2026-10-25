"""pumpkin_round, pumpkin_tall, pumpkin_small, gourd_cluster.

blender --background --factory-startup --python art/team/pumpkins_farm/batch01/build_pumpkins.py [-- ids...]
"""
import math
import pathlib
import sys

HERE = pathlib.Path(__file__).resolve().parent
sys.path.insert(0, str(HERE))
import pumpkin_lib as P  # noqa: E402

K = P.K


def pumpkin_round():
    K.reset()
    body = P.pumpkin_body("body", width=0.62, height=0.45, lobes=8, groove=0.17, groove_w=0.08, peaks=2,
                          rings=6, fullness=0.8, flatness=0.72, top_dimple=0.08, lumps=0.05, seed=3,
                          twist=0.15, lopsided=0.03, lop_dir=1.0, belly=1.25)
    body.rotation_euler.z = math.radians(-90 + 22.5 / 2)
    bpy_update()
    top = P.top_point(body)
    parts = [body]
    parts += P.curly_stem("stem", top, height=0.2, radius=0.045, color="stem_brown", curl=0.1,
                          lean_dir=math.radians(200), sides=6, pts=4)
    # pig-tail tendril springing off the stem base, curling in a vertical plane
    pts = []
    for i in range(9):
        t = i / 8
        a = math.pi * 0.9 + 2 * math.pi * 1.4 * t
        r = 0.055 * (1 - 0.65 * t)
        pts.append((top.x + 0.09 + math.cos(a) * r, top.y - 0.03 - 0.06 * t, top.z + 0.085 + math.sin(a) * r))
    parts.append(K.tube("curl", [(top.x + 0.02, top.y, top.z + 0.03)] + pts, radius=0.011, color="vine",
                        sides=4, smooth=True))
    ob = K.merge(parts, "pumpkin_round")
    return P.finish([ob], "props/pumpkin_round", "pumpkin_round")


def pumpkin_tall():
    K.reset()
    body = P.pumpkin_body("body", width=0.5, height=0.6, lobes=8, groove=0.16, groove_w=0.08, peaks=2,
                          rings=7, color="pumpkin_light", groove_color="pumpkin_dark", fullness=0.8,
                          flatness=0.6, top_dimple=0.07, lumps=0.04, seed=7, twist=-0.2,
                          lean=(0.04, 0.0), belly=1.1)
    body.rotation_euler.z = math.radians(-90 + 22.5 / 2 + 8)
    bpy_update()
    top = P.top_point(body)
    parts = [body]
    parts += P.curly_stem("stem", top, height=0.2, radius=0.046, color="stem_green", curl=0.09,
                          lean_dir=math.radians(-30), sides=6, pts=4)
    ob = K.merge(parts, "pumpkin_tall")
    return P.finish([ob], "props/pumpkin_tall", "pumpkin_tall")


def pumpkin_small():
    K.reset()
    body = P.pumpkin_body("body", width=0.3, height=0.21, lobes=6, groove=0.17, groove_w=0.1, peaks=2,
                          rings=5, color="pumpkin_dark", groove_color="rust", fullness=0.75, flatness=0.7,
                          top_dimple=0.04, bottom_dimple=0.008, lumps=0.05, seed=11, belly=1.2)
    body.rotation_euler.z = math.radians(-90 + 30)
    bpy_update()
    top = P.top_point(body)
    parts = [body]
    parts += P.curly_stem("stem", top, height=0.12, radius=0.034, color="stem_brown", curl=0.04,
                          lean_dir=math.radians(150), sides=6, pts=3, tip_ratio=0.75)
    ob = K.merge(parts, "pumpkin_small")
    return P.finish([ob], "props/pumpkin_small", "pumpkin_small")


def gourd_cluster():
    K.reset()
    parts = []
    # mini white pumpkin, back left
    w = P.pumpkin_body("white", width=0.24, height=0.15, lobes=6, groove=0.12, groove_w=0.12, peaks=1,
                       rings=4, color="gourd_white", groove_color="bone", fullness=0.6, flatness=0.7,
                       top_dimple=0.025, bottom_dimple=0.006, seed=5, loc=(-0.11, 0.08, 0))
    bpy_update()
    t = P.top_point(w)
    parts.append(w)
    parts.append(K.cylinder("ws", 0.018, 0.06, color="stem_green", verts=5, radius_top=0.013,
                            loc=(t.x, t.y, t.z + 0.02), rot=(10, -12, 0)))
    # striped pear gourd, standing at back right
    spine = [(0.12, 0.09, 0.0), (0.12, 0.09, 0.07), (0.125, 0.092, 0.15), (0.135, 0.095, 0.22), (0.15, 0.1, 0.27)]
    radii = [0.07, 0.085, 0.06, 0.042, 0.03]
    parts.append(P.blob_tube("pear", spine, radii, sides=8, color="gourd_green", stripe_color="gourd_white",
                             stripe_every=2, cap_offset=0.25))
    parts[-1].location.z += 0.012
    parts.append(K.cylinder("ps", 0.013, 0.05, color="stem_brown", verts=5, loc=(0.155, 0.1, 0.305),
                            rot=(0, 18, 0)))
    # yellow crookneck lying in front
    spine = [(-0.17, -0.1, 0.08), (-0.1, -0.12, 0.085), (-0.03, -0.125, 0.08), (0.03, -0.12, 0.09),
             (0.065, -0.115, 0.13), (0.06, -0.12, 0.18), (0.035, -0.13, 0.205)]
    radii = [0.06, 0.085, 0.07, 0.045, 0.032, 0.026, 0.02]
    parts.append(P.blob_tube("crook", spine, radii, sides=8, color="gourd_yellow", cap_offset=0.5))
    parts.append(K.sphere("crook_tip", 0.022, color="stem_green", segments=5, rings=3, loc=(0.03, -0.132, 0.21)))
    # warty green gourd, front right
    wart = K.sphere("warty", 0.085, color="gourd_green", segments=8, rings=5, scale=(1.05, 1.0, 0.85),
                    loc=(0.17, -0.07, 0.072))
    parts.append(wart)
    for i, (ax, az) in enumerate([(-40, 25), (30, 40), (-100, 50)]):
        a, e = math.radians(ax - 90), math.radians(az)
        c = (0.17 + math.cos(a) * math.cos(e) * 0.085, -0.07 + math.sin(a) * math.cos(e) * 0.085,
             0.072 + math.sin(e) * 0.072)
        parts.append(K.sphere(f"wart{i}", 0.02, color="olive", segments=5, rings=3, loc=c))
    parts.append(K.cylinder("gs", 0.014, 0.05, color="stem_green", verts=5, loc=(0.17, -0.07, 0.155),
                            rot=(-15, 0, 0)))
    ob = K.merge(parts, "gourd_cluster")
    # base to ground
    lo = min(v.co.z for v in ob.data.vertices)
    for v in ob.data.vertices:
        v.co.z -= lo
    return P.finish([ob], "props/gourd_cluster", "gourd_cluster")


def bpy_update():
    import bpy
    bpy.context.view_layer.update()


BUILDS = {f.__name__: f for f in (pumpkin_round, pumpkin_tall, pumpkin_small, gourd_cluster)}

if __name__ == "__main__":
    args = sys.argv[sys.argv.index("--") + 1:] if "--" in sys.argv else list(BUILDS)
    for name in args:
        BUILDS[name]()
