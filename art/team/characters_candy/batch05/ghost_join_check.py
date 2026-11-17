"""Measures the kid_ghost cowl / sheet join: horizontal rays toward the drape axis at several angles and
heights; prints which part is hit first and its radius. Exports nothing.
Run: blender --background --factory-startup --python art/team/characters_candy/batch05/ghost_join_check.py
"""
import math
import sys
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parent))
import bpy  # noqa: E402
from mathutils import Vector as V  # noqa: E402
import kid_kit as KK  # noqa: E402
from kid_kit import K  # noqa: E402
import costume_ghost as G  # noqa: E402

K.reset()
parts, children = G.build()
root, total, counts = KK.finish2("kid_ghost", parts, children, sheet=False, export=False)
bpy.context.view_layer.update()
dg = bpy.context.evaluated_depsgraph_get()
names = {"soft_cowl", "soft_sheet"}
for deg in (0, 45, 90, 135, 180, 225, 270, 315):
    a = math.radians(deg) - math.pi / 2       # 0 deg = the front
    row = []
    for z in (0.62, 0.6, 0.58, 0.57, 0.56, 0.555, 0.55, 0.545, 0.54, 0.53, 0.5):
        o = V((math.cos(a) * 0.8, math.sin(a) * 0.8 + G.YO, z))
        d = V((-math.cos(a), -math.sin(a), 0))
        res = []
        for nm in names:
            ob = bpy.data.objects[nm]
            mi = ob.matrix_world.inverted()
            ok, loc, nrm, idx = ob.ray_cast(mi @ o, (mi.to_3x3() @ d).normalized())
            if ok:
                w = ob.matrix_world @ loc
                res.append((round(math.hypot(w.x, w.y - G.YO), 4), nm[5:9]))
        res.sort(reverse=True)
        row.append(f"z{z:.3f}:" + ("/".join(f"{n}{r:.4f}" for r, n in res) if res else "-"))
    print(f"ANG {deg:3d} " + "  ".join(row))
# baked AO (the vertex colour the game multiplies in) on the outside of each part, by height
for nm in ("soft_cowl", "soft_sheet"):
    ob = bpy.data.objects[nm]
    me = ob.data
    attr = me.color_attributes.get("AO")
    bands = {}
    for v in me.vertices:
        w = ob.matrix_world @ v.co
        n = (ob.matrix_world.to_3x3() @ v.normal)
        if V((w.x, w.y - G.YO, 0)).dot(V((n.x, n.y, 0))) <= 0:
            continue
        b = round(w.z * 50) / 50
        bands.setdefault(b, []).append(attr.data[v.index].color[0])
    print("AO", nm, " ".join(f"{b:.2f}:{sum(x) / len(x):.2f}" for b, x in sorted(bands.items()) if 0.4 < b < 0.8))
