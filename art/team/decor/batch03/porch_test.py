"""Porch mock-up for integration checks (Workbench, tint x AO): string_jack along a sagging wire,
bunting on a cord, door_wreath on a door, porch_lantern beside it, lamp_post on the kerb.

    blender -b --factory-startup --python porch_test.py -- <out.png> [yaw:pitch:dist ...]
"""
import math
import sys

import bpy
from mathutils import Vector

sys.path.insert(0, r"S:\Repos\trick-or-treat\art\tools")
import lowpoly_kit as K  # noqa: E402

M = r"S:\Repos\trick-or-treat\game\assets\models\props"
args = sys.argv[sys.argv.index("--") + 1:]
out = args[0]
views = [tuple(float(x) for x in v.split(":")) for v in args[1:]] or [(0, 12, 6.0), (-35, 35, 15.0)]
K.reset()
pal = K._palette_rgb()


def load(name):
    before = set(bpy.data.objects)
    bpy.ops.import_scene.gltf(filepath=f"{M}\\{name}.gltf")
    new = [o for o in bpy.data.objects if o not in before]
    root = [o for o in new if o.parent is None][0]
    meshes = [o for o in new if o.type == "MESH"]
    for o in meshes:
        me = o.data
        tint = me.uv_layers[1] if len(me.uv_layers) > 1 else None
        ao = me.color_attributes[0] if len(me.color_attributes) else None
        prev = me.color_attributes.new("Preview", "FLOAT_COLOR", "CORNER")
        for poly in me.polygons:
            for li in poly.loop_indices:
                u, v = tint.data[li].uv if tint else (0.0, 0.0)
                col, row = int((u % 1.0) * 16), int((1.0 - v) * 16)
                rgb = pal.get((col, row), (1, 0, 1))
                a = ao.data[me.loops[li].vertex_index].color[0] if ao is not None and ao.domain == "POINT" else 1.0
                prev.data[li].color = (rgb[0] * a, rgb[1] * a, rgb[2] * a, 1)
        me.color_attributes.active_color = prev
    return root, meshes


def place(name, loc, rotz=0.0):
    root, meshes = load(name)
    root.location = Vector(loc)
    root.rotation_euler.z += math.radians(rotz)
    return root



def flat(name, size, loc, color):
    ob = K.box(name, size, color=color, loc=loc)
    me = ob.data
    prev = me.color_attributes.new("Preview", "FLOAT_COLOR", "CORNER")
    rgb = pal[[(c, r) for r, row in enumerate(K.PALETTE_SPEC["rows"]) for c, e in enumerate(row)
               if e.split(":")[0] == color][0]]
    for li in range(len(me.loops)):
        prev.data[li].color = (*rgb, 1)
    me.color_attributes.active_color = prev
    return ob


# house front: wall, door, porch eave beam
flat("wall", (6, 0.1, 3.0), (0, 0.05, 1.5), "house_sage")
flat("door", (1.0, 0.04, 2.2), (0, -0.02, 1.1), "trim_white")
flat("beam", (6, 0.18, 0.2), (0, -1.6, 2.7), "trim_white")
for x in (-2.9, 2.9):
    flat(f"postc{x}", (0.18, 0.18, 2.7), (x, -1.6, 1.35), "trim_white")
flat("floor", (6, 2, 0.1), (0, -0.9, -0.05), "plank")

# wreath on the door, lantern beside it
place("door_wreath", (0, -0.04, 1.55))
place("porch_lantern", (0.85, 0.0, 1.95))
place("lamp_post", (3.8, -3.5, 0))

# string jacks on a catenary under the eave: posts every 1.45 m, sag 0.12 m, bulbs every 0.29 m
def sag(x, x0, x1, s):
    t = (x - x0) / (x1 - x0)
    return -4 * s * t * (1 - t)


spans = [(-2.9, -1.45), (-1.45, 0.0), (0.0, 1.45), (1.45, 2.9)]
for x0, x1 in spans:
    n = 5
    for k in range(1, n):
        x = x0 + (x1 - x0) * k / n
        z = 2.58 + sag(x, x0, x1, 0.12)
        place("string_jack", (x, -1.62, z))
    # the wire itself
    pts = [(x0 + (x1 - x0) * i / 12, -1.62, 2.58 + sag(x0 + (x1 - x0) * i / 12, x0, x1, 0.12)) for i in range(13)]
    cu = bpy.data.curves.new("wire", "CURVE")
    cu.dimensions = "3D"
    cu.bevel_depth = 0.005
    sp = cu.splines.new("POLY")
    sp.points.add(len(pts) - 1)
    for p, q in zip(sp.points, pts):
        p.co = (*q, 1)
    bpy.context.scene.collection.objects.link(bpy.data.objects.new("wire", cu))

# bunting on the wall above the door: flags every 0.31 m, sag 0.15 m over 2.8 m
cols = ["bunting_flag_orange", "bunting_flag_purple", "bunting_flag_cream"]
x0, x1 = -1.4, 1.4
for k in range(9):
    x = x0 + 0.04 + k * 0.34
    z = 2.62 + sag(x, x0, x1, 0.15)
    place(cols[k % 3], (x, -0.12, z))

sc = bpy.context.scene
sc.render.engine = "BLENDER_WORKBENCH"
sh = sc.display.shading
sh.light = "STUDIO"
sh.color_type = "VERTEX"
sc.render.resolution_x, sc.render.resolution_y = 1280, 720
w = bpy.data.worlds.new("w")
sc.world = w
w.color = (0.16, 0.19, 0.32)
cd = bpy.data.cameras.new("cam")
cd.lens = 35
cam = bpy.data.objects.new("cam", cd)
sc.collection.objects.link(cam)
sc.camera = cam
focus = Vector((0.5, -1.0, 1.6))
for i, (yaw, pitch, dist) in enumerate(views):
    d = Vector((math.sin(math.radians(yaw)) * math.cos(math.radians(pitch)),
                -math.cos(math.radians(yaw)) * math.cos(math.radians(pitch)), math.sin(math.radians(pitch))))
    cam.location = focus + d * dist
    cam.rotation_euler = (-d).to_track_quat("-Z", "Y").to_euler()
    sc.render.filepath = out.replace(".png", f"_{i}.png")
    bpy.ops.render.render(write_still=True)
