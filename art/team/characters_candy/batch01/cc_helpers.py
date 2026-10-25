"""Lane helpers for characters_candy (kept out of art/tools on purpose).

part()      merge pieces into one named object whose origin sits at a joint (rotation identity).
slab()      outline in the X-Z plane extruded along Y (wings, carved face shapes, flat chips).
aim()       Euler degrees that point local +Z along a direction (spikes, horns).
close_sheet() extra review renders: close front, close 3/4, back 3/4 and a gameplay-distance tile.
"""
import math
import sys
from pathlib import Path

import bmesh
import bpy
from mathutils import Matrix, Vector

sys.path.insert(0, str(Path(__file__).resolve().parents[3] / "tools"))
import lowpoly_kit as K  # noqa: E402


def part(objs, name, joint):
    ob = K.merge(objs, name)
    j = Vector(joint)
    ob.data.transform(Matrix.Translation(-j))
    ob.location = j
    return ob


def attach(child, parent):
    """Parent without a parent-inverse matrix (clean local transforms in glTF). Assumes the
    parent chain has no rotation/scale, which holds for our rigs (parts are identity-rotated)."""
    bpy.context.view_layer.update()
    world = child.matrix_world.translation.copy()
    pw = parent.matrix_world.translation.copy()
    child.parent = parent
    child.matrix_parent_inverse = Matrix.Identity(4)
    child.location = world - pw
    bpy.context.view_layer.update()
    return child


def slab(name, outline_xz, thickness, color, loc=(0, 0, 0), rot=(0, 0, 0), bevel=0.0, smooth=False,
         scale=(1, 1, 1)):
    bm = bmesh.new()
    t = thickness / 2
    front = [bm.verts.new((x, -t, z)) for x, z in outline_xz]
    back = [bm.verts.new((x, t, z)) for x, z in outline_xz]
    bm.faces.new(front)
    bm.faces.new(list(reversed(back)))
    k = len(outline_xz)
    for i in range(k):
        bm.faces.new((front[i], back[i], back[(i + 1) % k], front[(i + 1) % k]))
    bmesh.ops.recalc_face_normals(bm, faces=bm.faces)
    return K._finish(bm, name, color, loc, rot, scale, smooth, bevel, 1)


def aim(direction, roll=0.0):
    d = Vector(direction).normalized()
    e = d.to_track_quat("Z", "Y").to_euler()
    return (math.degrees(e.x), math.degrees(e.y), math.degrees(e.z) + roll)


def _bounds(objs):
    bpy.context.view_layer.update()
    lo = Vector((1e9, 1e9, 1e9))
    hi = Vector((-1e9, -1e9, -1e9))
    for o in objs:
        for c in o.bound_box:
            w = o.matrix_world @ Vector(c)
            lo = Vector((min(lo.x, w.x), min(lo.y, w.y), min(lo.z, w.z)))
            hi = Vector((max(hi.x, w.x), max(hi.y, w.y), max(hi.z, w.z)))
    return lo, hi


def close_sheet(objects, name, views=((0, 8), (-35, 15), (150, 20), (90, 5)), tiny=True):
    """2x2 close-up sheet (no scale post) at art/review/sheets/<name>_close.png, plus an optional
    gameplay-distance tile <name>_far.png (35 deg down, rendered at ~70 px tall then upscaled)."""
    mat = K._preview_material()
    meshes = [o for ob in objects for o in [ob] + list(ob.children_recursive) if o.type == "MESH"]
    for o in meshes:
        if not o.data.materials:
            o.data.materials.append(mat)
    lo, hi = _bounds(meshes)
    center = (lo + hi) / 2
    radius = max((hi - lo).length / 2, 0.15)
    scene = bpy.context.scene
    scene.render.engine = "BLENDER_WORKBENCH"
    sh = scene.display.shading
    sh.light = "STUDIO"
    sh.color_type = "TEXTURE"
    sh.show_shadows = True
    sh.show_cavity = True
    world = scene.world or bpy.data.worlds.new("w")
    scene.world = world
    world.color = (0.16, 0.19, 0.32)
    cam_data = bpy.data.cameras.new("close_cam")
    cam_data.type = "ORTHO"
    cam = K._link(bpy.data.objects.new("close_cam", cam_data))
    scene.camera = cam

    def shoot(yaw, pitch, path, res, ortho):
        scene.render.resolution_x = res
        scene.render.resolution_y = res
        cam_data.ortho_scale = ortho
        d = Vector((math.sin(math.radians(yaw)) * math.cos(math.radians(pitch)),
                    -math.cos(math.radians(yaw)) * math.cos(math.radians(pitch)),
                    math.sin(math.radians(pitch))))
        cam.location = center + d * radius * 4
        cam.rotation_euler = (-d).to_track_quat("-Z", "Y").to_euler()
        scene.render.filepath = str(path)
        bpy.ops.render.render(write_still=True)

    tiles = []
    for i, (yaw, pitch) in enumerate(views):
        p = K.REVIEW / f"_{name}_c{i}.png"
        shoot(yaw, pitch, p, 512, radius * 2.2)
        tiles.append(p)
    K._compose(tiles, K.REVIEW / f"{name}_close.png")
    for p in tiles:
        p.unlink(missing_ok=True)
    if tiny:
        # 9-15 m away with a ~50 deg FOV at 1080p: the 1.1 m kid is roughly 60-90 px tall.
        p = K.REVIEW / f"{name}_far.png"
        shoot(-25, 35, p, 96, 1.5)
    bpy.data.objects.remove(cam, do_unlink=True)
