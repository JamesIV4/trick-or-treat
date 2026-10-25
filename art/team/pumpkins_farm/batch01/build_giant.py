"""giant_pumpkin: prize pumpkin platform with a blue first-prize rosette.

blender --background --factory-startup --python art/team/pumpkins_farm/batch01/build_giant.py
"""
import math
import pathlib
import sys

HERE = pathlib.Path(__file__).resolve().parent
sys.path.insert(0, str(HERE))
import bpy  # noqa: E402
from mathutils import Matrix, Vector  # noqa: E402
from mathutils.bvhtree import BVHTree  # noqa: E402

import pumpkin_lib as P  # noqa: E402

K = P.K
W, H = 1.8, 1.2


def surface(tree, x, z):
    loc, nrm, _, _ = tree.ray_cast(Vector((x, -5, z)), Vector((0, 1, 0)))
    return loc, nrm


def rosette(tree, x, z):
    """Pleated blue rosette built facing -Y, then seated on the surface at (x, z)."""
    parts = []
    star = []
    for i in range(18):
        a = 2 * math.pi * i / 18
        r = 0.3 if i % 2 == 0 else 0.25
        star.append((math.cos(a) * r, math.sin(a) * r))
    parts.append(K.polygon_prism("pleat", star, 0.045, color="quilt_blue", rot=(90, 0, 0)))
    ring = [(math.cos(2 * math.pi * i / 10) * 0.19, math.sin(2 * math.pi * i / 10) * 0.19) for i in range(10)]
    parts.append(K.polygon_prism("ring", ring, 0.065, color="trim_white", rot=(90, 0, 0)))
    button = [(math.cos(2 * math.pi * i / 10 + 0.3) * 0.13, math.sin(2 * math.pi * i / 10 + 0.3) * 0.13)
              for i in range(10)]
    parts.append(K.polygon_prism("button", button, 0.085, color="foil_gold", rot=(90, 0, 0)))
    ros = K.merge(parts, "rosette")
    loc, nrm = surface(tree, x, z)
    rot = (-nrm).to_track_quat("Y", "Z").to_matrix().to_4x4()
    ros.matrix_world = Matrix.Translation(loc - nrm * 0.035) @ rot
    out = [ros]
    # two tails that drape down the belly, following the surface
    for side, sway in ((-1, -0.07), (1, 0.06)):
        pts, ups = [], []
        for i in range(5):
            t = i / 4
            px = x + side * 0.08 + sway * t * 2.5
            pz = z - 0.08 - 0.46 * t
            l2, n2 = surface(tree, px, pz)
            pts.append(l2 + n2 * 0.012)
            ups.append(n2)
        widths = [0.12, 0.13, 0.13, 0.13, 0.12]
        out.append(P.ribbon(f"tail{side}", pts, widths, thick=0.008, color="quilt_blue", up=ups, crease=1.2))
        # V-notch end: a small dark-blue chevron reads as the cut ribbon end
    return out


def build():
    K.reset()
    body = P.pumpkin_body("body", width=W, height=H, lobes=10, groove=0.09, groove_w=0.07, peaks=2, rings=10,
                          color="pumpkin_light", groove_color="pumpkin_dark", fullness=0.78, flatness=0.7,
                          top_dimple=0.12, bottom_dimple=0.03, lumps=0.07, seed=21, twist=0.1,
                          lopsided=0.05, lop_dir=math.radians(30), lean=(0.05, 0.03), belly=1.3, flat_top=0.82)
    body.rotation_euler.z = math.radians(-90 + 5)
    bpy.context.view_layer.update()
    top = P.top_point(body)
    body = K.merge([body], "body")
    dg = bpy.context.evaluated_depsgraph_get()
    tree = BVHTree.FromObject(body, dg)
    parts = [body]
    parts += P.curly_stem("stem", (top.x - 0.02, top.y + 0.05, top.z), height=0.4, radius=0.1,
                          color="stem_green", curl=0.22, lean_dir=math.radians(160), sides=7, pts=5,
                          tip_ratio=0.65)
    # pig-tail tendril
    pts = []
    for i in range(10):
        t = i / 9
        a = math.pi * 0.8 + 2 * math.pi * 1.5 * t
        r = 0.12 * (1 - 0.6 * t)
        pts.append((top.x + 0.22 + math.cos(a) * r, top.y - 0.1 - 0.12 * t, top.z + 0.2 + math.sin(a) * r))
    parts.append(K.tube("curl", [(top.x + 0.05, top.y, top.z + 0.06)] + pts, radius=0.022, color="vine",
                        sides=5, smooth=True))
    # two broad leaves lying on the flat top beside the stem
    for k, (ang, ln) in enumerate(((200, 0.42), (-30, 0.36))):
        a = math.radians(ang)
        base = Vector((top.x, top.y + 0.05, top.z + 0.04))
        d = Vector((math.cos(a), math.sin(a), 0))
        lpts = [base + d * ln * t + Vector((0, 0, 0.05 * math.sin(math.pi * t) + 0.06 * t)) for t in (0, 0.35, 0.7, 1.0)]
        parts.append(P.ribbon(f"leaf{k}", lpts, [0.06, 0.22, 0.2, 0.0], thick=0.02, color="vine",
                              under_color="vine_dark", crease=1.6))
    parts += rosette(tree, 0.08, 0.7)
    ob = K.merge(parts, "giant_pumpkin")
    return P.finish([ob], "props/giant_pumpkin", "giant_pumpkin", close=True)


if __name__ == "__main__":
    build()
