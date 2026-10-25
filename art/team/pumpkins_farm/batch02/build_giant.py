"""giant_pumpkin: prize pumpkin platform with a blue first-prize rosette (batch02, textured).

blender --background --factory-startup --python art/team/pumpkins_farm/batch02/build_giant.py
"""
import math
import pathlib
import sys

sys.path.insert(0, str(pathlib.Path(__file__).resolve().parent))
import bmesh  # noqa: E402
import bpy  # noqa: E402
from mathutils import Matrix, Vector  # noqa: E402
from mathutils.bvhtree import BVHTree  # noqa: E402

import farm_lib as F  # noqa: E402

K = F.K
W, H = 1.8, 1.2


def surface(tree, x, z):
    loc, nrm, _, _ = tree.ray_cast(Vector((x, -5, z)), Vector((0, 1, 0)))
    return loc, nrm


def pleated_disc(name, r_out, r_in, pleats, depth, color, mat="felt", z=0.0):
    """Ring of radial pleats (zig-zag folds) in the XY plane facing +Z: rosette ruffle."""
    bm = F.bm_new()
    outer, inner = [], []
    n = pleats * 2
    for i in range(n):
        a = 2 * math.pi * i / n
        up = depth if i % 2 == 0 else -depth
        ro = r_out * (1.0 if i % 2 == 0 else 0.93)
        outer.append(bm.verts.new((math.cos(a) * ro, math.sin(a) * ro, z + up)))
        inner.append(bm.verts.new((math.cos(a) * r_in, math.sin(a) * r_in, z + depth * 0.3)))
    back = bm.verts.new((0, 0, z - depth * 2.5))
    for i in range(n):
        i2 = (i + 1) % n
        bm.faces.new((inner[i], outer[i], outer[i2], inner[i2]))
        bm.faces.new((outer[i2], outer[i], back))  # closed back so it never shows a hole
    cap = bm.verts.new((0, 0, z + depth * 0.3))
    for i in range(n):
        bm.faces.new((cap, inner[i], inner[(i + 1) % n]))
    bmesh.ops.recalc_face_normals(bm, faces=bm.faces)
    for f in bm.faces:
        F.set_uv(bm, f, [(v.co.x * 2, v.co.y * 2) for v in f.verts])
    F.paint(bm, bm.faces, color, mat)
    return F.to_object(bm, name, smooth=True, sharp=50)


def rosette(tree, x, z):
    """First-prize rosette built facing +Z, then seated on the surface at (x, z) facing outward."""
    parts = [pleated_disc("pleat", 0.29, 0.16, 14, 0.025, "quilt_blue")]
    parts.append(pleated_disc("pleat2", 0.2, 0.1, 10, 0.02, "trim_white", z=0.03))
    parts.append(K.cylinder("button", 0.115, 0.05, color="foil_gold", mat="metal", verts=16, bevel=0.015,
                            segments=2, loc=(0, 0, 0.07)))
    F.auto_smooth(parts[-1], 40)
    # star stamped on the button
    star = []
    for i in range(10):
        a = math.pi / 2 + math.pi * i / 5
        r = 0.07 if i % 2 == 0 else 0.03
        star.append((math.cos(a) * r, math.sin(a) * r))
    st = K.polygon_prism("star", star, 0.012, color="quilt_yellow", mat="flat", loc=(0, 0, 0.09))
    parts.append(st)
    ros = K.merge(parts, "rosette")
    loc, nrm = surface(tree, x, z)
    out_dir = Vector((nrm.x, nrm.y, nrm.z * 0.6)).normalized()
    rot = out_dir.to_track_quat("Z", "Y").to_matrix().to_4x4()
    ros.matrix_world = Matrix.Translation(loc + out_dir * 0.03) @ rot
    F.bake(ros)
    out = [ros]
    # two tails with a V-notched end draping down the belly, following the surface
    for side, sway in ((-1, -0.06), (1, 0.05)):
        pts, ups = [], []
        for i in range(6):
            t = i / 5
            px = x + side * 0.07 + sway * t * 2.5
            pz = z - 0.12 - 0.5 * t
            l2, n2 = surface(tree, px, pz)
            pts.append(l2 + n2 * 0.014)
            ups.append(n2)
        widths = [0.11, 0.12, 0.125, 0.125, 0.125, 0.125]
        tail = F.blade(f"tail{side}", pts, widths, thick=0.01, color="quilt_blue", mat="felt", up=ups, crease=1.0,
                       uv_scale=2.0)
        out.append(tail)
        # V notch: a little white-edged swallowtail cut
        e = pts[-1]
        d = (pts[-1] - pts[-2]).normalized()
        sidev = d.cross(ups[-1]).normalized()
        for k in (-1, 1):
            out.append(F.blade(f"notch{side}{k}", [e - d * 0.005 + sidev * k * 0.031, e + d * 0.055 + sidev * k * 0.04],
                               [0.062, 0.0], thick=0.009, color="quilt_blue", mat="felt", up=ups[-1]))
    return out


def build():
    K.reset()
    body = F.pumpkin_body("body", width=W, height=H, lobes=10, spl=6, rings=18, groove=0.085, plump=0.42,
                          color="pumpkin_light", fullness=0.75, flatness=0.68, top_dimple=0.14, dimple_w=0.36,
                          bottom_dimple=0.03, lumps=0.06, seed=21, twist=0.1, lopsided=0.05,
                          lop_dir=math.radians(30), lean=(0.04, 0.025), belly=1.28, flat_top=0.84, lobe_h=0.03,
                          rot_z=-90 + 18 + 5)
    top = Vector(body["top"])
    bpy.context.view_layer.update()
    dg = bpy.context.evaluated_depsgraph_get()
    tree = BVHTree.FromObject(body, dg)
    parts = [body]
    parts.append(F.stem("stem", (top.x - 0.02, top.y + 0.04, top.z), height=0.36, radius=0.085, color="stem_green",
                        curl=0.24, lean_dir=math.radians(160), rings=9, ridges=6, flare=1.9, tip_ratio=0.75,
                        uv_scale=1.6, sink=0.05))
    # pig-tail tendril springing off the stem base
    parts.append(F.tendril("curl", (top.x + 0.26, top.y - 0.12, top.z + 0.2), turns=1.8, r0=0.1, r1=0.03, rise=-0.12,
                           n=20, radius=0.02, sides=6, axis="x", phase=math.pi,
                           lead=(top.x + 0.06, top.y - 0.02, top.z + 0.08)))
    # two broad leaves lying on the flat top beside the stem, petioles toward the stem
    for k, (ang, size, tilt) in enumerate(((205, 0.3, 8), (-35, 0.26, 10))):
        a = math.radians(ang)
        cx, cy = top.x + math.cos(a) * (size + 0.08), top.y + math.sin(a) * (size + 0.08)
        leaf = F.pumpkin_leaf(f"leaf{k}", size=size, color="vine", lobes=5, n=30, cup=0.06, corrugate=0.1,
                              vein_color="vine_dark", sharp=85)
        F.drape(leaf, tree, cx, cy, ang, lift=0.012)
        parts.append(leaf)
    parts += rosette(tree, 0.06, 0.66)
    ob = K.merge(parts, "giant_pumpkin")
    return F.finish([ob], "props/giant_pumpkin", "giant_pumpkin", ao_radius=1.2)


if __name__ == "__main__":
    build()
