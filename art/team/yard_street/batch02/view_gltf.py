"""Render an exported gltf from custom angles (tint x AO, workbench).
blender -b --factory-startup --python view.py -- <gltf> <out.png> yaw:pitch[:zoom] ..."""
import math
import sys

import bpy
from mathutils import Vector

sys.path.insert(0, r"S:\Repos\trick-or-treat\art\tools")
import lowpoly_kit as K  # noqa: E402

args = sys.argv[sys.argv.index("--") + 1:]
src, out = args[0], args[1]
views = [tuple(float(x) for x in v.split(":")) for v in args[2:]] or [(-35, 20), (0, -40), (90, 0), (-20, 35)]
K.reset()
bpy.ops.import_scene.gltf(filepath=src)
meshes = [o for o in bpy.context.scene.objects if o.type == "MESH"]
pal = K._palette_rgb()
for o in meshes:
    me = o.data
    uvs = me.uv_layers
    tint = uvs[1] if len(uvs) > 1 else None
    ao = me.color_attributes[0] if len(me.color_attributes) else None
    prev = me.color_attributes.new("Preview", "FLOAT_COLOR", "CORNER")
    for poly in me.polygons:
        for li in poly.loop_indices:
            u, v = tint.data[li].uv if tint else (0.0, 0.0)
            col = int((u % 1.0) * 16)
            row = int((1.0 - v) * 16)
            rgb = pal.get((col, row), (1, 0, 1))
            a = 1.0
            if ao is not None:
                if ao.domain == "POINT":
                    a = ao.data[me.loops[li].vertex_index].color[0]
                else:
                    a = ao.data[li].color[0]
            prev.data[li].color = (rgb[0] * a, rgb[1] * a, rgb[2] * a, 1)
    me.color_attributes.active_color = prev
bpy.context.view_layer.update()
lo = Vector((1e9,) * 3)
hi = Vector((-1e9,) * 3)
for o in meshes:
    for c in o.bound_box:
        w = o.matrix_world @ Vector(c)
        lo = Vector(map(min, lo, w))
        hi = Vector(map(max, hi, w))
center = (lo + hi) / 2
radius = max((hi - lo).length / 2, 0.3)
scene = bpy.context.scene
scene.render.engine = "BLENDER_WORKBENCH"
sh = scene.display.shading
sh.light = "STUDIO"
sh.color_type = "VERTEX"
sh.show_shadows = False
sh.show_cavity = False
scene.render.resolution_x = 640
scene.render.resolution_y = 640
world = bpy.data.worlds.new("w")
scene.world = world
world.color = (0.16, 0.19, 0.32)
cd = bpy.data.cameras.new("cam")
cd.type = "ORTHO"
cam = bpy.data.objects.new("cam", cd)
scene.collection.objects.link(cam)
scene.camera = cam
tiles = []
for i, v in enumerate(views):
    yaw, pitch = v[0], v[1]
    zoom = v[2] if len(v) > 2 else 1.0
    cd.ortho_scale = radius * 2.3 / zoom
    d = Vector((math.sin(math.radians(yaw)) * math.cos(math.radians(pitch)),
                -math.cos(math.radians(yaw)) * math.cos(math.radians(pitch)), math.sin(math.radians(pitch))))
    cam.location = center + d * radius * 4
    cam.rotation_euler = (-d).to_track_quat("-Z", "Y").to_euler()
    p = out.replace(".png", f"_{i}.png")
    scene.render.filepath = p
    bpy.ops.render.render(write_still=True)
    tiles.append(p)
from pathlib import Path  # noqa: E402
K._compose([Path(t) for t in tiles[:4]], Path(out))
