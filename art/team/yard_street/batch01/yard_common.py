"""Shared helpers for the yard_street lane (batch01).

Import from a build script in this folder:
    import sys, pathlib; sys.path.insert(0, str(pathlib.Path(__file__).resolve().parent))
    from yard_common import K, lathe, blob, cut_below, finish, want

Adds what the kit lacks:
  * lathe()      revolve a (radius, z) profile with one palette colour per band; r=0 makes a clean pole
  * blob()       faceted lumpy icosphere, baked to world space (canopies, clouds, bushes, mums)
  * cut_below()  slice a mesh flat at a height and cap it (flat-bottomed clouds, bushes on the ground)
  * bake()       apply an object's transform into its mesh
  * mesh()       build an object from raw verts/faces with per-face colours
  * paint_where_world() repaint by world-space face centre/normal
  * finish()     export + review sheet + tri report in one call
  * want()       filter assets by ids passed after `--` on the Blender command line
"""
from __future__ import annotations

import math
import pathlib
import sys

ROOT = pathlib.Path(__file__).resolve().parents[4]
sys.path.insert(0, str(ROOT / "art" / "tools"))
import lowpoly_kit as K  # noqa: E402

import bmesh  # noqa: E402
import bpy  # noqa: E402
from mathutils import Euler, Matrix, Vector  # noqa: E402

ARGS = sys.argv[sys.argv.index("--") + 1:] if "--" in sys.argv else []


def want(asset_id):
    return not ARGS or asset_id in ARGS


# ---------------------------------------------------------------- object plumbing
def obj_from_bm(bm, name, loc=(0, 0, 0), rot=(0, 0, 0), scale=(1, 1, 1), smooth=False):
    """Like K._finish but keeps whatever per-face painting is already on the bmesh."""
    for f in bm.faces:
        f.smooth = smooth
    me = bpy.data.meshes.new(name)
    bm.to_mesh(me)
    bm.free()
    ob = K._link(bpy.data.objects.new(name, me))
    ob.location = Vector(loc)
    ob.rotation_euler = Euler([math.radians(a) for a in rot])
    ob.scale = Vector(scale)
    return ob


def bake(ob):
    """Applies the object's transform to its mesh (object ends at identity)."""
    bpy.context.view_layer.update()
    ob.data.transform(ob.matrix_world)
    ob.matrix_world = Matrix.Identity(4)
    ob.data.update()
    return ob


def mesh(name, verts, faces, colors, smooth=False):
    """Raw mesh. colors: one palette name or a list with one name per face."""
    bm = bmesh.new()
    vs = [bm.verts.new(v) for v in verts]
    fs = [bm.faces.new([vs[i] for i in f]) for f in faces]
    bmesh.ops.recalc_face_normals(bm, faces=bm.faces)
    if isinstance(colors, str):
        colors = [colors] * len(fs)
    for f, c in zip(fs, colors):
        K._paint_bm(bm, c, [f])
    return obj_from_bm(bm, name, smooth=smooth)


def paint_where_world(ob, color, where):
    """Repaint faces using world-space centre/normal: where(center, normal) -> bool."""
    bpy.context.view_layer.update()
    mw = ob.matrix_world
    nm = mw.to_3x3().inverted().transposed()
    bm = bmesh.new()
    bm.from_mesh(ob.data)
    faces = [f for f in bm.faces if where(mw @ f.calc_center_median(), (nm @ f.normal).normalized())]
    K._paint_bm(bm, color, faces)
    bm.to_mesh(ob.data)
    bm.free()
    return ob


def parent_keep(child, parent):
    """K.set_parent keeps the world matrix, but reads matrix_world before the depsgraph has
    refreshed it (a location set just before is lost and the child snaps to the origin).
    Refresh first, then parent."""
    bpy.context.view_layer.update()
    return K.set_parent(child, parent)


def socket(anchor, loc, parent):
    """Same contract as K.socket (empty named socket_<anchor>) but the position survives export."""
    ob = K.empty(f"socket_{anchor}")
    ob.location = Vector(loc)
    return parent_keep(ob, parent)


# ---------------------------------------------------------------- lathe
def lathe(name, profile, colors="wood", segments=10, loc=(0, 0, 0), rot=(0, 0, 0), scale=(1, 1, 1),
          smooth=False, cap_bottom=True, cap_top=True, cap_colors=None, phase=0.0, angles=None,
          band_cols=None):
    """Revolves profile [(r, z), ...] (bottom to top) around Z.
    colors: one name or one per band (len(profile) - 1). r == 0 makes a pole (triangle fan).
    cap_colors: (bottom, top) colours for flat caps when the end radius is > 0.
    angles: optional explicit column angles in degrees (uneven columns, e.g. thin frame mullions).
    band_cols: {band_index: [colour per column]} overrides (glass panes with dark frames)."""
    if angles is not None:
        segments = len(angles)
    band_cols = band_cols or {}
    if isinstance(colors, str):
        colors = [colors] * (len(profile) - 1)
    if cap_colors is None:
        cap_colors = (colors[0], colors[-1])
    bm = bmesh.new()
    rings = []
    for r, z in profile:
        if r < 1e-6:
            rings.append([bm.verts.new((0, 0, z))])
        else:
            ring = []
            for j in range(segments):
                a = (math.radians(angles[j]) if angles is not None else 2 * math.pi * j / segments) + phase
                ring.append(bm.verts.new((math.cos(a) * r, math.sin(a) * r, z)))
            rings.append(ring)
    groups = {}
    for i in range(len(rings) - 1):
        r0, r1 = rings[i], rings[i + 1]
        for j in range(segments):
            j1 = (j + 1) % segments
            if len(r0) == 1 and len(r1) == 1:
                continue
            if len(r0) == 1:
                f = bm.faces.new((r0[0], r1[j], r1[j1]))
            elif len(r1) == 1:
                f = bm.faces.new((r0[j], r1[0], r0[j1]))
            else:
                f = bm.faces.new((r0[j], r1[j], r1[j1], r0[j1]))
            groups.setdefault(band_cols[i][j] if i in band_cols else colors[i], []).append(f)
    if cap_bottom and len(rings[0]) > 1:
        groups.setdefault(cap_colors[0], []).append(bm.faces.new(list(reversed(rings[0]))))
    if cap_top and len(rings[-1]) > 1:
        groups.setdefault(cap_colors[1], []).append(bm.faces.new(rings[-1]))
    bmesh.ops.recalc_face_normals(bm, faces=bm.faces)
    for c, fs in groups.items():
        K._paint_bm(bm, c, fs)
    return obj_from_bm(bm, name, loc, rot, scale, smooth)


# ---------------------------------------------------------------- blobs
def _hash(x, y, z, seed):
    h = math.sin(x * 12.9898 + y * 78.233 + z * 37.719 + seed * 4.1414) * 43758.5453
    return h - math.floor(h)


def blob(name, radius, color, loc=(0, 0, 0), scale=(1, 1, 1), rot=(0, 0, 0), subdiv=2, lump=0.08, seed=0):
    """Faceted lumpy icosphere baked to world space. lump = fraction of radius of random push.
    NOTE: in Blender 5.1 bmesh subdivisions=1 is the bare icosahedron (20 tris); 2 = 80 tris, 3 = 320."""
    ob = K.ico(name, radius, color=color, subdiv=subdiv, smooth=False)
    if lump > 0:
        def f(co):
            n = _hash(co.x, co.y, co.z, seed) * 2 - 1
            return co * (1 + n * lump)
        K.deform(ob, f)
    ob.location = Vector(loc)
    ob.rotation_euler = Euler([math.radians(a) for a in rot])
    ob.scale = Vector(scale)
    if not any(rot):
        inner = radius * (1 - lump) * 0.97
        ob["ell"] = [loc[0], loc[1], loc[2], inner * scale[0], inner * scale[1], inner * scale[2]]
    return bake(ob)


def puff(name, radius, color, loc, scale=(1, 1, 1), base_z=0.0, segments=9, bands=3, lump=0.06, seed=0,
         base_color=None):
    """Faceted dome: the part of a (scaled) sphere above base_z, built directly so no triangles are
    wasted below the cut. Rings are staggered half a step so facets read as triangles like an
    icosphere. Flat cap at base_z. Stores the same 'ell' data as blob() for cull_hidden()."""
    cx, cy, cz = loc
    sx, sy, sz = scale
    bottom = max(-1.0, min(1.0, (base_z - cz) / (radius * sz)))
    t0 = math.asin(bottom)  # elevation angle of the cut
    bm = bmesh.new()
    rings = []
    for i in range(bands + 1):
        t = t0 + (math.pi / 2 - t0) * i / (bands + 0.6)
        ring = []
        for j in range(segments):
            a = 2 * math.pi * (j + 0.5 * (i % 2)) / segments
            n = 1.0 if i == 0 else 1 + (_hash(math.cos(a) * 3.1, math.sin(a) * 3.1, t * 5.0, seed) * 2 - 1) * lump
            x = math.cos(a) * math.cos(t) * radius * n
            y = math.sin(a) * math.cos(t) * radius * n
            z = math.sin(t) * radius * n
            ring.append(bm.verts.new((cx + x * sx, cy + y * sy, base_z if i == 0 else cz + z * sz)))
        rings.append(ring)
    top = bm.verts.new((cx, cy, cz + radius * sz * (1 + (_hash(1, 2, 3, seed) * 2 - 1) * lump)))
    for i in range(bands):
        r0, r1 = rings[i], rings[i + 1]
        for j in range(segments):
            j1 = (j + 1) % segments
            if i % 2 == 0:
                bm.faces.new((r0[j], r0[j1], r1[j]))
                bm.faces.new((r0[j1], r1[j1], r1[j]))
            else:
                bm.faces.new((r0[j], r1[j1], r1[j]))
                bm.faces.new((r0[j], r0[j1], r1[j1]))
    for j in range(segments):
        bm.faces.new((rings[-1][j], rings[-1][(j + 1) % segments], top))
    cap = bm.faces.new(list(reversed(rings[0])))
    bmesh.ops.recalc_face_normals(bm, faces=bm.faces)
    K._paint_bm(bm, color)
    if base_color:
        K._paint_bm(bm, base_color, [cap])
    ob = obj_from_bm(bm, name)
    inner = radius * (1 - lump) * 0.97
    ob["ell"] = [cx, cy, cz, inner * sx, inner * sy, inner * sz]
    return ob


def cull_hidden(objs):
    """Deletes faces of each blob whose every vertex sits inside another blob's inner ellipsoid
    (fully buried in the cluster, never visible). Saves triangles on canopies, bushes, clouds."""
    ells = [(o, o.get("ell")) for o in objs]
    for ob in objs:
        others = [list(e) for o, e in ells if o is not ob and e is not None]
        if not others:
            continue
        bm = bmesh.new()
        bm.from_mesh(ob.data)

        def inside(co):
            for cx, cy, cz, rx, ry, rz in others:
                if ((co.x - cx) / rx) ** 2 + ((co.y - cy) / ry) ** 2 + ((co.z - cz) / rz) ** 2 < 1.0:
                    return True
            return False

        dead = [f for f in bm.faces if all(inside(v.co) for v in f.verts)]
        bmesh.ops.delete(bm, geom=dead, context="FACES")
        bm.to_mesh(ob.data)
        bm.free()
        ob.data.update()
    return objs


def cut_below(ob, z=0.0, cap_color=None, keep="above"):
    """Bakes, slices the mesh at height z, deletes everything below (keep="below": above) and caps the hole."""
    bake(ob)
    bm = bmesh.new()
    bm.from_mesh(ob.data)
    geom = list(bm.verts) + list(bm.edges) + list(bm.faces)
    bmesh.ops.bisect_plane(bm, geom=geom, plane_co=(0, 0, z),
                        plane_no=(0, 0, 1 if keep == "above" else -1), clear_inner=True)
    boundary = [e for e in bm.edges if e.is_boundary]
    if boundary:
        res = bmesh.ops.holes_fill(bm, edges=boundary, sides=0)
        if cap_color:
            K._paint_bm(bm, cap_color, res["faces"])
    bmesh.ops.recalc_face_normals(bm, faces=bm.faces)
    # snap tiny float error so the cap is exactly at z
    for v in bm.verts:
        if abs(v.co.z - z) < 1e-5:
            v.co.z = z
    bm.to_mesh(ob.data)
    bm.free()
    ob.data.update()
    return ob


def dissolve_degenerate(ob, dist=1e-4):
    bm = bmesh.new()
    bm.from_mesh(ob.data)
    bmesh.ops.remove_doubles(bm, verts=bm.verts, dist=dist)
    bmesh.ops.dissolve_degenerate(bm, edges=bm.edges, dist=dist)
    bm.to_mesh(ob.data)
    bm.free()
    return ob


# ---------------------------------------------------------------- curves
def arc(center, radius, a0, a1, n, plane="XZ", radius_end=None):
    """Points along an arc (degrees). plane XZ: angle 0 = +X, 90 = +Z."""
    pts = []
    for i in range(n + 1):
        t = i / n
        a = math.radians(a0 + (a1 - a0) * t)
        r = radius if radius_end is None else radius + (radius_end - radius) * t
        if plane == "XZ":
            pts.append((center[0] + math.cos(a) * r, center[1], center[2] + math.sin(a) * r))
        elif plane == "YZ":
            pts.append((center[0], center[1] + math.cos(a) * r, center[2] + math.sin(a) * r))
        else:
            pts.append((center[0] + math.cos(a) * r, center[1] + math.sin(a) * r, center[2]))
    return pts


def bezier(p0, p1, p2, n=6, p3=None):
    """Quadratic (or cubic with p3) bezier points."""
    p0, p1, p2 = Vector(p0), Vector(p1), Vector(p2)
    out = []
    for i in range(n + 1):
        t = i / n
        if p3 is None:
            p = (1 - t) ** 2 * p0 + 2 * (1 - t) * t * p1 + t * t * p2
        else:
            q3 = Vector(p3)
            p = (1 - t) ** 3 * p0 + 3 * (1 - t) ** 2 * t * p1 + 3 * (1 - t) * t * t * p2 + t ** 3 * q3
        out.append(tuple(p))
    return out


# ---------------------------------------------------------------- hand-off
def clean(ob, dist=1e-5):
    """Triangulates and removes zero-area slivers (bisect cuts and ngon caps can make them)."""
    if ob.type != "MESH":
        return ob
    bm = bmesh.new()
    bm.from_mesh(ob.data)
    bmesh.ops.triangulate(bm, faces=bm.faces, quad_method="BEAUTY", ngon_method="BEAUTY")
    bmesh.ops.dissolve_degenerate(bm, edges=bm.edges, dist=dist)
    bmesh.ops.triangulate(bm, faces=bm.faces, quad_method="BEAUTY", ngon_method="BEAUTY")
    dead = [f for f in bm.faces if f.calc_area() < 1e-8]
    if dead:
        bmesh.ops.delete(bm, geom=dead, context="FACES")
    loose = [v for v in bm.verts if not v.link_faces]
    if loose:
        bmesh.ops.delete(bm, geom=loose, context="VERTS")
    bm.to_mesh(ob.data)
    bm.free()
    ob.data.update()
    return ob


def finish(objects, rel_path, asset_id):
    for ob in objects:
        for o in [ob] + list(ob.children_recursive):
            clean(o)
    out = K.export(objects, rel_path)
    tris = K.tri_count(objects)
    sheet = K.review_sheet(objects, asset_id)
    print(f"ASSET {asset_id} tris={tris} gltf={out} sheet={sheet}")
    return tris
