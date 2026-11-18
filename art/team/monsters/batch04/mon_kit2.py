"""Monster kit v2 (monsters / batch04): rig trees of rigid parts for the v2 monster contract.

Everything from batch03's mon_kit is re-exported (blob, lathe, band_lathe, tube, cone, bone, plate,
smile, cheek, put, bake, mirror_x, catmull, bvh / surf ...; D = dk_kit, F = farm_lib, K = lowpoly_kit),
so a build script needs only `import mon_kit2 as M`. Added here:

  Rig                  the part tree: named parts with joint-origin pivots, any nesting, rest rotation
                       where a part needs its own axes (eyes), ball joints, soft parts, sockets,
                       per-part AO, export, the spec the verifier checks, review and pose sheets
  eye()                an eye part (sclera dome + two glints) with its pupil child part (iris + pupil)
  socket_eye()         a skull's eye: a dark socket lining with a small soft light child part inside it
  limb()               a tapered bone / limb segment with optional flared, rounded ends
  shell() / patch()    a thick cloth slab / a thin decal from a parametric surface (soft parts, bandanas,
                       hems, mouths, markings)
  ribbon()             a tapered strap along a path (streamers, tails, bandage ends)
  ellipse() / heart()  2D outlines for F.carve and plates
  straw()              a chunky straw / leaf blade whose normals are set so a cloth_wave ripple flaps it
  button()             a sewn button (rim, dish, cross of thread): shirt buttons, button eyes

Coordinates: Blender, Z up, front = -Y. g2b((x, y_up, z_front)) converts game / glTF coordinates
(+X the monster's left, +Y up, +Z front) to Blender; every pivot below is a Blender world point.

The contract this kit builds for (see CHECKPOINT.md, "v2 glTF contract"):
  * one empty root `monster_<key>` at the origin; every other node is a part named after itself,
    parented to the nearest part above it; a part's origin is its pivot and its mesh is in its own space
  * names: letters, digits and underscores only, unique, no Godot import-hint suffix (_col, _rigid ...)
  * rest rotation is allowed (Rig.part(z_out=...)), rest scale never; limb chains meant for two-bone IK
    stay translation-only (do not give them z_out)
  * eyes: origin = eye centre, local +Z out of the face, +Y up, pupil a child of the eye
  * soft parts: ONE continuous mesh whose pivot is the pinned end, with enough loops along its run
  * socket_<name> empties are points, alt_<name> parts start hidden
  * at most 32 mesh parts and 8,000 triangles
"""
from __future__ import annotations

import json
import math
import pathlib
import re
import subprocess
import sys
import time

HERE2 = pathlib.Path(__file__).resolve().parent
sys.path.insert(0, str(HERE2.parent / "batch03"))

import bmesh  # noqa: E402
import bpy  # noqa: E402
from mathutils import Matrix, Vector  # noqa: E402
from mathutils.bvhtree import BVHTree  # noqa: E402

from mon_kit import *  # noqa: E402,F401,F403  (the batch03 helpers, unchanged)
from mon_kit import ARGS, FLAGS, IDS, D, F, K, TAU, V, b2g, g2b  # noqa: E402,F401

SPEC_DIR = HERE2 / "rig_spec"
SHEETS = K.REVIEW
MAX_PARTS, MAX_TRIS = 32, 8000
# Godot's scene importer treats these name endings ("-x" or "_x") as import hints and changes or
# removes the node.
IMPORT_HINTS = ("noimp", "col", "convcol", "colonly", "convcolonly", "navmesh", "vehicle", "wheel", "rigid",
                "loop", "cycle", "occ", "occonly")


def check_name(name):
    """Raises unless `name` survives the Godot importer unchanged."""
    if not re.fullmatch(r"[A-Za-z][A-Za-z0-9_]*", name):
        raise ValueError(f"part name '{name}': use letters, digits and underscores only (no dots, no spaces)")
    low = name.lower()
    for h in IMPORT_HINTS:
        if low.endswith("_" + h) or low == h:
            raise ValueError(f"part name '{name}' ends with the Godot import hint '_{h}'")
    return name


# ---------------------------------------------------------------- outlines
def ellipse(cx, cz, rx, rz, n=14, tilt=0.0):
    """CCW ellipse outline [(x, z)], tilted by `tilt` degrees (positive leans the top toward -x)."""
    c, s = math.cos(math.radians(tilt)), math.sin(math.radians(tilt))
    out = []
    for i in range(n):
        a = TAU * i / n
        x, z = math.cos(a) * rx, math.sin(a) * rz
        out.append((cx + x * c - z * s, cz + x * s + z * c))
    return out


def heart(cx, cz, w, h, n=18, point_up=False):
    """CCW heart outline [(x, z)] w wide and h tall centred on (cx, cz). point_up=True turns it over
    (the lobes at the bottom): a skull's nose hole."""
    pts = []
    for i in range(n):
        t = TAU * i / n
        x = 16 * math.sin(t) ** 3
        z = 13 * math.cos(t) - 5 * math.cos(2 * t) - 2 * math.cos(3 * t) - math.cos(4 * t)
        pts.append((x / 32.0, (z + 2.5) / 29.0))
    if point_up:
        pts = [(x, -z) for x, z in pts]
    return F.ccw([(cx + x * w, cz + z * h) for x, z in pts])


# ---------------------------------------------------------------- shapes
def limb(name, a, b, r, r_end=None, flare_a=1.0, flare_b=1.0, sides=8, color="bone", mat="flat", cap=0.5,
         waist=0.9):
    """Limb segment from a to b: radius r (to r_end), pinched to `waist` in the middle, with the ends
    flared by flare_a / flare_b (1 = plain) and rounded caps. A flared end reads as a bone knob."""
    a, b = V(a), V(b)
    r_end = r if r_end is None else r_end
    ts = (0.0, 0.07, 0.2, 0.5, 0.8, 0.93, 1.0)
    ks = (flare_a * 0.92, flare_a, 1.0 + (flare_a - 1.0) * 0.25, waist, 1.0 + (flare_b - 1.0) * 0.25, flare_b, flare_b * 0.92)
    pts = [a.lerp(b, t) for t in ts]
    rr = [(r + (r_end - r) * t) * k for t, k in zip(ts, ks)]
    return F.sweep(name, pts, rr, sides=sides, color=color, mat=mat, cap_offset=cap)


def patch(name, fn, nu, nv, color="ink", mat="flat", paint=None, smooth=True, flip=False):
    """Thin one-sided decal from a parametric surface: fn(u, v) -> point, u and v in 0..1, nu x nv
    quads. paint(i, j) -> (color, mat) or None recolours quad (i, j). Lay it a millimetre or two above
    the surface it sits on. flip=True reverses the facing."""
    bm = bmesh.new()
    grid = [[bm.verts.new(fn(i / nu, j / nv)) for j in range(nv + 1)] for i in range(nu + 1)]
    groups = {}
    for i in range(nu):
        for j in range(nv):
            q = (grid[i][j], grid[i + 1][j], grid[i + 1][j + 1], grid[i][j + 1])
            f = bm.faces.new(tuple(reversed(q)) if flip else q)
            cm = paint(i, j) if paint else None
            if cm:
                groups.setdefault(cm, []).append(f)
    extra = [(fs, c, m) for (c, m), fs in groups.items()]
    return D.mesh(bm, name, color, mat, "box", smooth=smooth, extra=extra)


def shell(name, outer, nu, nv, thick=0.008, inner=None, color="pumpkin", mat="flat", paint=None, smooth=True,
          rim_color=None):
    """Closed slab between two parametric surfaces: outer(u, v) -> point (u, v in 0..1) and
    inner(u, v) (default: `thick` behind the outer surface along its normal). One continuous mesh
    with nv loops along v: what a soft part (bandana, cape, hem, ear) or a jaw is made of.
    The default inner surface lies on the side (d/du x d/dv) points away from, so run u and v so that
    cross product faces the viewer. paint(i, j) recolours the outer quad (i, j), as in patch()."""
    def normal(u, v, e=1e-3):
        du = V(outer(min(u + e, 1.0), v)) - V(outer(max(u - e, 0.0), v))
        dv = V(outer(u, min(v + e, 1.0))) - V(outer(u, max(v - e, 0.0)))
        n = du.cross(dv)
        return n.normalized() if n.length > 1e-12 else V((0, 0, 1))
    if inner is None:
        def inner(u, v):
            return V(outer(u, v)) - normal(u, v) * thick
    bm = bmesh.new()
    A = [[bm.verts.new(outer(i / nu, j / nv)) for j in range(nv + 1)] for i in range(nu + 1)]
    B = [[bm.verts.new(inner(i / nu, j / nv)) for j in range(nv + 1)] for i in range(nu + 1)]
    groups = {}
    rim = []
    for i in range(nu):
        for j in range(nv):
            f = bm.faces.new((A[i][j], A[i + 1][j], A[i + 1][j + 1], A[i][j + 1]))
            cm = paint(i, j) if paint else None
            if cm:
                groups.setdefault(cm, []).append(f)
            bm.faces.new((B[i][j], B[i][j + 1], B[i + 1][j + 1], B[i + 1][j]))
    for i in range(nu):
        rim.append(bm.faces.new((A[i][0], B[i][0], B[i + 1][0], A[i + 1][0])))
        rim.append(bm.faces.new((A[i][nv], A[i + 1][nv], B[i + 1][nv], B[i][nv])))
    for j in range(nv):
        rim.append(bm.faces.new((A[0][j], A[0][j + 1], B[0][j + 1], B[0][j])))
        rim.append(bm.faces.new((A[nu][j], B[nu][j], B[nu][j + 1], A[nu][j + 1])))
    extra = [(fs, c, m) for (c, m), fs in groups.items()]
    if rim_color:
        extra.append((rim, rim_color, mat))
    bmesh.ops.recalc_face_normals(bm, faces=bm.faces)   # a closed slab: outward whichever way u, v run
    return D.mesh(bm, name, color, mat, "box", smooth=smooth, extra=extra)


def ribbon(name, pts, width, thick=0.006, nv=8, nu=2, color="pumpkin", mat="flat", up=(0, -1, 0), cup=0.0,
           per=3):
    """Tapered strap along a path (Catmull-Rom through pts): width is a number or fn(t) -> width,
    flat side facing `up`, `cup` bows it across its width. nv loops along the run (8 bends smoothly)."""
    path = catmull(pts, per)  # noqa: F405
    lens = [0.0]
    for p, q in zip(path[:-1], path[1:]):
        lens.append(lens[-1] + (q - p).length)

    def at(t):
        s = t * lens[-1]
        for k in range(1, len(path)):
            if s <= lens[k] or k == len(path) - 1:
                f = (s - lens[k - 1]) / max(lens[k] - lens[k - 1], 1e-9)
                return path[k - 1].lerp(path[k], min(max(f, 0.0), 1.0)), (path[k] - path[k - 1]).normalized()
    upv = V(up)

    def outer(u, v):
        p, t = at(v)
        side = t.cross(upv)
        side = side.normalized() if side.length > 1e-6 else V((1, 0, 0))
        nrm = side.cross(t).normalized()
        w = width(v) if callable(width) else width
        x = (u - 0.5) * 2.0
        return p + side * (x * w * 0.5) + nrm * (cup * (1.0 - x * x) * w)
    return shell(name, outer, nu, nv, thick=thick, color=color, mat=mat)


# ---------------------------------------------------------------- bandages, belts, bands (added for the mummy)
def strap(name, on, n, half_w, height=0.008, edge=0.3, closed=False, color="linen", edge_color=None, mat="flat",
          uv_scale=1.0, fade=0, cap=True, lift=0.0):
    """A padded strip lying on a surface: bandage wraps, belts, collars, cuffs, hat bands.

    on(t, s) -> (point, normal): the surface point at t along the strip (0..1) and s metres across
    its width (band_fn and ring_fn below make these). The strip is three quads across: both edges
    sit on the surface (plus `lift`), the middle (1 - edge) of the width stands `height` proud, and
    everything is smooth shaded, so it reads as a soft band with a rounded shoulder and a real edge.
    Strips laid later with a larger height cover the ones under them where they cross.

    half_w   half the width, a number or fn(t); height may also be fn(t)
    closed   a full loop (t = 1 joins t = 0); otherwise the ends are capped
    fade     open strips: over this many segments at each end the height runs down to nothing, so the
             end tucks under its neighbours
    edge_color  colour of the two sloping edge quads (default: the same as the top)
    UVs run along the strip (u) and across it (v) in metres times uv_scale, so a weave follows it."""
    rows = n if closed else n + 1
    bm = bmesh.new()
    proj, _ = K._uv_layers(bm)
    grid, mids, nrm0 = [], [], None
    for i in range(rows):
        t = i / n
        w = half_w(t) if callable(half_w) else half_w
        h = height(t) if callable(height) else height
        if fade and not closed:
            k = min(i, n - i) / fade
            h *= min(1.0, max(k, 0.0)) ** 0.5
        row = []
        for s, up in ((-w, 0.0), (-w * (1.0 - edge), 1.0), (w * (1.0 - edge), 1.0), (w, 0.0)):
            p, nr = on(t, s)
            row.append(bm.verts.new(V(p) + V(nr) * (lift + h * up)))
        grid.append(row)
        pm, nm = on(t, 0.0)
        mids.append(V(pm))
        if nrm0 is None:
            nrm0 = V(nm)
    arc = [0.0]
    for i in range(1, rows):
        arc.append(arc[-1] + (mids[i] - mids[i - 1]).length)
    if closed:
        arc.append(arc[-1] + (mids[0] - mids[-1]).length)
    top, sides = [], []
    span = n if closed else n
    for i in range(span):
        a, b = grid[i], grid[(i + 1) % rows]
        for k in range(3):
            f = bm.faces.new((a[k], a[k + 1], b[k + 1], b[k]))
            w0 = half_w(i / n) if callable(half_w) else half_w
            vs = (-w0, -w0 * (1.0 - edge), w0 * (1.0 - edge), w0)
            for loop, (u, v) in zip(f.loops, ((arc[i], vs[k]), (arc[i], vs[k + 1]), (arc[i + 1], vs[k + 1]), (arc[i + 1], vs[k]))):
                loop[proj].uv = (u * uv_scale, v * uv_scale)
            (top if k == 1 else sides).append(f)
    caps = []
    if not closed and cap:
        for row, out in ((grid[0], mids[0] - mids[1]), (grid[-1], mids[-1] - mids[-2])):
            # (a faded end has no height left: nothing to cap)
            if ((row[1].co + row[2].co) * 0.5 - (row[0].co + row[3].co) * 0.5).length > 1e-4:
                caps.append((bm.faces.new((row[0], row[1], row[2], row[3])), out))
    bm.normal_update()
    if top and top[0].normal.dot(nrm0) < 0.0:
        bmesh.ops.reverse_faces(bm, faces=top + sides)
    for f, out in caps:
        if f.normal.dot(out) < 0.0:
            bmesh.ops.reverse_faces(bm, faces=[f])
    caps = [f for f, _ in caps]
    K._paint_bm(bm, color, mat)
    if edge_color:
        K._paint_bm(bm, edge_color, mat, sides + caps)
    for f in bm.faces:
        f.smooth = True
    me = bpy.data.meshes.new(name)
    bm.to_mesh(me)
    bm.free()
    return K._link(bpy.data.objects.new(name, me))


def proxy_tree(make):
    """BVH of a throwaway high-resolution copy of a shape, for laying strips on it: make() builds the
    object (e.g. the same blob() call with many more segments); it is removed again."""
    ob = make()
    bake(ob)  # noqa: F405
    tree = bvh(ob)  # noqa: F405
    bpy.data.objects.remove(ob, do_unlink=True)
    return tree


def band_fn(tree, centre, axis, lat, ref_r, a0=0.0, a1=360.0, front=(0, -1, 0), wave=None):
    """on(t, s) for strap(): a band round a blob. The band is the circle of latitude `lat` (degrees)
    about `axis` through `centre`, from angle a0 to a1 (degrees; 0 = the side facing `front`, 90 = the
    monster's left when the axis points up), found on the surface by ray casts against `tree`.
    ref_r turns the across offset s (metres) into latitude. wave(angle_radians) -> extra latitude in
    degrees makes it wander. Tilt the axis to cross bands over each other."""
    c, ax = V(centre), V(axis).normalized()
    f = V(front) - ax * V(front).dot(ax)
    e1 = f.normalized() if f.length > 1e-6 else ax.orthogonal().normalized()
    e2 = ax.cross(e1)

    def on(t, s):
        th = math.radians(a0 + (a1 - a0) * t)
        ph = math.radians(lat + (wave(th) if wave else 0.0)) + s / ref_r
        d = (e1 * math.cos(th) + e2 * math.sin(th)) * math.cos(ph) + ax * math.sin(ph)
        loc, nrm, _, _ = tree.ray_cast(c + d * 4.0, -d, 8.0)
        if loc is None:
            return c + d * ref_r, d
        return loc, nrm
    return on


def arc_fn(tree, centre, d0, d1, ref_r, bow=0.0):
    """on(t, s) for strap(): a strip running the short way over a blob from direction d0 to d1 (both
    from `centre`, less than half a turn apart), found on the surface by ray casts against `tree`.
    bow (degrees) pushes the middle of the run sideways (toward d0 x d1). For diagonal wraps."""
    c, d0, d1 = V(centre), V(d0).normalized(), V(d1).normalized()
    ax = d0.cross(d1).normalized()
    ang = d0.angle(d1)

    def on(t, s):
        d = Matrix.Rotation(ang * t, 3, ax) @ d0
        off = s / ref_r + math.radians(bow) * math.sin(math.pi * t)
        d = (d * math.cos(off) + ax * math.sin(off)).normalized()
        loc, nrm, _, _ = tree.ray_cast(c + d * 4.0, -d, 8.0)
        if loc is None:
            return c + d * ref_r, d
        return loc, nrm
    return on


def ring_fn(a, b, radius, at, tilt=0.0, phase=0.0, a0=0.0, a1=360.0, ref=(0, 0, 1)):
    """on(t, s) for strap(): a ring round a straight limb from a to b. radius is a number or fn(u)
    with u = 0 at a and 1 at b; the ring sits `at` (0..1) along the limb and rides up and down it by
    `tilt` metres as it goes round (a slanted wrap). Angle 0 faces `ref`."""
    a, b = V(a), V(b)
    ax = (b - a)
    length = ax.length
    ax = ax / length
    r0 = V(ref) - ax * V(ref).dot(ax)
    e1 = r0.normalized() if r0.length > 1e-6 else ax.orthogonal().normalized()
    e2 = ax.cross(e1)

    def on(t, s):
        th = math.radians(a0 + (a1 - a0) * t)
        along = at * length + tilt * math.sin(th + phase) + s
        r = radius(along / length) if callable(radius) else radius
        d = e1 * math.cos(th) + e2 * math.sin(th)
        return a + ax * along + d * r, d
    return on


def axis_tilt(pitch=0.0, roll=0.0, yaw=0.0):
    """Blender direction of an 'up' axis leaned `pitch` degrees toward the front (-Y) and `roll`
    degrees toward the monster's left (+X), then turned `yaw` degrees about up (toward its left).
    For band_fn: a band about an axis pitched forward dips at the front and rises at the back."""
    p, r, y = math.radians(pitch), math.radians(roll), math.radians(yaw)
    v = V((math.sin(r) * math.cos(p), -math.sin(p), math.cos(p) * math.cos(r)))
    return V((v.x * math.cos(y) - v.y * math.sin(y), v.x * math.sin(y) + v.y * math.cos(y), v.z)).normalized()


# ---------------------------------------------------------------- straw and buttons (added for the scarecrow)
WAVE_ATTR = "wave_n"   # per-vertex normal a straw() blade asks for; Rig.finish turns it into custom normals


def straw(name, root, direction, length, width, up, droop=0.0, thick=0.3, color="straw", mat="straw", n=4, wave=0.4,
          uv_scale=1.0, tip=0.0):
    """A chunky straw / leaf blade: from `root` along `direction`, `length` long and `width` wide at
    its widest (a third of the way out), lens section `thick` * width thick, its flat side facing
    `up`; `droop` (a fraction of the length) bows it away from `up` toward the tip. n points along it
    (3 = 14 triangles, 4 = 22). tip = width left at the end as a fraction (0 = a point).

    wave: the blade's normals are replaced by its flat side's direction plus `wave` of the real
    normal (None = leave them). The shader's cloth_wave pushes vertices along their normal, so with
    the real normals a ripple would only fatten and thin each blade; with these the whole blade
    swings square to its flat side, more toward the tip, and it is lit as one soft leaf."""
    root, d, u = V(root), V(direction).normalized(), V(up)
    u = (u - d * u.dot(d))
    u = u.normalized() if u.length > 1e-6 else d.orthogonal().normalized()
    ts = [i / (n - 1) for i in range(n)]
    prof = lambda t: (0.72 + 0.28 * math.sin(min(t / 0.36, 1.0) * math.pi / 2)) if t < 0.36 else \
        (tip + (1.0 - tip) * max(math.cos((t - 0.36) / 0.64 * math.pi / 2), 0.0) ** 0.8)
    pts = [root + d * (length * t) - u * (droop * length * t * t) for t in ts]
    ws = [width * prof(t) for t in ts]
    if tip <= 0.0:
        ws[-1] = 0.0
    ob = F.blade(name, pts, ws, thick=width * thick, color=color, mat=mat, up=tuple(u), uv_scale=uv_scale)
    if wave is not None:
        me = ob.data
        attr = me.attributes.new(WAVE_ATTR, "FLOAT_VECTOR", "POINT")
        for v in me.vertices:
            attr.data[v.index].vector = (u + v.normal * wave).normalized()
    return ob


def button(name, r, thick=0.014, color="banner_purple", rim=None, thread="straw", segs=14, cross=0.3, thread_w=0.15):
    """A sewn button lying in the local XY plane, its back on z = 0 and its face toward +Z: a raised
    rim (colour `rim`, default the same), a sunk, slightly domed middle and a cross of `thread` (None for none).
    Returns a list of objects at the origin: place each with put(), or hand the list to
    eye(pupil_extra=...) for a button eye."""
    t = thick
    # (the middle is a low dome, not flat: a flat face catches the gloss highlight all at once and flashes white)
    prof = [(r * 0.9, 0.0), (r, t * 0.35), (r, t * 0.72), (r * 0.91, t), (r * 0.76, t), (r * 0.66, t * 0.6), (r * 0.42, t * 0.74), (0.0, t * 0.8)]
    body = D.lathe(name, prof, segs, color=color, mat="flat", project="sphere", center=(0, 0, 0))
    if rim:
        K.paint(body, rim, where=lambda c, n: math.hypot(c.x, c.y) > r * 0.68)
    out = [body]
    if thread:
        h = r * cross
        z = t * 0.78 + r * thread_w * 0.3
        for k, (a, b) in enumerate((((-h, -h, z), (h, h, z)), ((-h, h, z), (h, -h, z)))):
            out.append(F.dash(f"{name}_x{k}", a, b, w=r * thread_w, t=r * thread_w * 0.55, color=thread))
    return out


# ---------------------------------------------------------------- the rig tree
class _Part:
    def __init__(self, name, ob, world, parent, soft, ground, ball, ao_from, tris):
        self.name, self.ob, self.world, self.parent = name, ob, world, parent
        self.soft, self.ground, self.ball, self.ao_from, self.tris = soft, ground, ball, tuple(ao_from), tris


class Rig:
    """The part tree of one monster. Build pieces in world space in the rest pose, then hand each
    group to part() with the world position of its joint:

        R = M.Rig("monster_skeleton", height=1.2, radius=0.3)     # the C++ collision capsule
        R.part("body", [pelvis...], pivot=g2b((0, 0.47, 0)))
        R.part("chest", [ribs...], pivot=g2b((0, 0.535, 0)), parent="body")
        R.part("arm_l", [upper...], pivot=shoulder, parent="chest", ball=0.046)   # ball joint at the pivot
        M.eye(R, "eye_l", "pupil_l", centre, normal, 0.06, 0.07, parent="head")
        R.part("bandana", [flap], pivot=pin, parent="chest", soft=dict(dir=g2b((0, -1, 0.2))))
        R.socket("muzzle", point, parent="hand_r")
        R.finish(poses=[...])

    Parents must be added before their children. `body` must exist."""

    def __init__(self, asset_id, height, radius, floating=False):
        if not re.fullmatch(r"monster_[a-z]+", asset_id):
            raise ValueError("asset id must be monster_<key>")
        self.id = asset_id
        self.height, self.radius, self.floating = height, radius, floating
        self.root = K.empty(asset_id, (0, 0, 0))
        self.parts = {}
        self.sockets = {}
        self.ik = []
        self.eyes = {}
        self._shade = []

    # -- parts
    def part(self, name, objs, pivot, parent=None, z_out=None, up=(0, 0, 1), roll=0.0, soft=None, ground=True,
             ball=None, ball_color=None, ball_segs=8, ao_from=()):
        """One rigid part: `objs` (mesh objects in world space, rest pose) merged into the node `name`
        whose origin is `pivot`. No objs = a meshless joint.

        z_out   rest rotation: the part's local +Z (glTF) points along this world direction and its +Y
                toward `up`, turned by `roll` degrees about +Z. For eyes and anything the animator
                slides or scales along its own axes. Leave None for limbs (translation-only).
        ball    radius of a ball joint: a sphere is added at the pivot, so no gap opens however the
                part turns; the parent's AO sees it and the verifier checks the parent reaches into it.
                Author the parent so its end overlaps the ball by 3 to 4 cm.
        soft    dict for a shader-bent part (one continuous mesh pinned at the pivot): dir = world
                direction of its run from the pin (None: by distance from the pin), rigid = stiff
                distance from the pin, loops = fewest bands of vertices along the run (default 6).
        ground  False: its AO ignores the ground plane (parts that never come near it).
        ao_from names of other parts that occlude this one and never move against it."""
        check_name(name)
        if name in self.parts:
            raise ValueError(f"part '{name}' defined twice")
        if parent is not None and parent not in self.parts:
            raise ValueError(f"part '{name}': parent '{parent}' is not defined yet")
        pivot = V(pivot)
        objs = list(objs)
        if ball:
            col = ball_color or "bone"
            objs.append(blob(f"{name}_ball", pivot, (ball, ball, ball), segs=ball_segs, rings=max(4, ball_segs // 2 + 1),  # noqa: F405
                             color=col))
        if "--pieces" in FLAGS:
            print("  ", name, ", ".join(f"{o.name}:{D.tris(o)}" for o in objs))
        world = Matrix.Translation(pivot)
        if z_out is not None:
            n = V(z_out).normalized()
            u = V(up) - n * V(up).dot(n)
            u = u.normalized() if u.length > 1e-6 else n.orthogonal().normalized()
            x = (-n).cross(u).normalized()
            rot = Matrix((x, -n, u)).transposed()
            if roll:
                rot = rot @ Matrix.Rotation(math.radians(-roll), 3, "Y")
            world = world @ rot.to_4x4()
        if objs:
            ob = K.merge(objs, name)
            ob.data.transform(world.inverted())
            ob.data.update()
            tris = D.tris(ob)
        else:
            ob = K.empty(name)
            tris = 0
        if ob.name != name:
            raise RuntimeError(f"Blender renamed '{name}' to '{ob.name}': another object already uses the name")
        par_ob = self.parts[parent].ob if parent else self.root
        par_world = self.parts[parent].world if parent else Matrix.Identity(4)
        ob.parent = par_ob
        ob.matrix_parent_inverse = Matrix.Identity(4)
        ob.matrix_basis = par_world.inverted() @ world
        if soft is not None:
            soft = dict(soft)
            if soft.get("dir") is not None:
                soft["dir"] = tuple(V(soft["dir"]).normalized())
        self.parts[name] = _Part(name, ob, world, parent, soft, ground, ball, ao_from, tris)
        return ob

    def socket(self, name, loc, parent, z_out=None, up=(0, 0, 1)):
        """A named point riding on `parent` (read in C++ with rig.socket_world("name", xf))."""
        check_name(name)
        node = "socket_" + name
        world = Matrix.Translation(V(loc))
        if z_out is not None:
            n = V(z_out).normalized()
            u = V(up) - n * V(up).dot(n)
            u = u.normalized() if u.length > 1e-6 else n.orthogonal().normalized()
            world = world @ Matrix(((-n).cross(u).normalized(), -n, u)).transposed().to_4x4()
        ob = K.empty(node)
        ob.parent = self.parts[parent].ob
        ob.matrix_parent_inverse = Matrix.Identity(4)
        ob.matrix_basis = self.parts[parent].world.inverted() @ world
        self.sockets[name] = parent
        return ob

    def ik_chain(self, upper, mid, end):
        """Declares a two-bone IK chain (anim::two_bone_ik): the verifier checks it is translation-only."""
        self.ik.append([upper, mid, end])

    def meshes(self):
        return [p.ob for p in self.parts.values() if p.ob.type == "MESH"]

    # -- ambient occlusion, per part
    def bake_ao(self, samples=32, radius=0.25, strength=0.85):
        """Per-vertex AO for a rig of moving parts: each part is occluded by itself, by the ball joints
        of its own children (a sphere at a pivot looks the same in every pose), by the parts named in
        its ao_from, and by the ground. So no dark contact patch is left behind when a limb lifts."""
        bpy.context.view_layer.update()
        dirs = K._hemisphere(samples)
        ball_cache = {}

        def sphere(c, r):
            key = round(r, 4)
            if key not in ball_cache:
                bm = bmesh.new()
                bmesh.ops.create_uvsphere(bm, u_segments=12, v_segments=8, radius=r)
                ball_cache[key] = ([v.co.copy() for v in bm.verts], [[v.index for v in f.verts] for f in bm.faces])
                bm.free()
            vs, fs = ball_cache[key]
            return [c + v for v in vs], fs

        def geometry(part):
            mw = part.ob.matrix_world
            return [mw @ v.co for v in part.ob.data.vertices], [list(p.vertices) for p in part.ob.data.polygons]

        for part in self.parts.values():
            if part.ob.type != "MESH":
                continue
            verts, polys = geometry(part)
            extra = [sphere(q.world.translation, q.ball) for q in self.parts.values() if q.parent == part.name and q.ball]
            extra += [geometry(self.parts[n]) for n in part.ao_from if self.parts[n].ob.type == "MESH"]
            for vs, fs in extra:
                base = len(verts)
                verts += vs
                polys += [[base + i for i in f] for f in fs]
            tree = BVHTree.FromPolygons(verts, polys)
            me = part.ob.data
            mw = part.ob.matrix_world
            nm = mw.to_3x3().inverted().transposed()
            attr = me.color_attributes.get("AO") or me.color_attributes.new("AO", "FLOAT_COLOR", "POINT")
            for v in me.vertices:
                p = mw @ v.co
                n = (nm @ v.normal).normalized()
                t = n.orthogonal().normalized()
                b = n.cross(t)
                occ = 0.0
                for d in dirs:
                    w = t * d.x + b * d.y + n * d.z
                    if part.ground and not self.floating and w.z < -1e-4 and (-p.z / w.z) <= radius and p.z >= -0.01:
                        occ += 1.0
                        continue
                    hit = tree.ray_cast(p + n * 0.006, w, radius)
                    if hit[0] is not None:
                        occ += 1.0 - (hit[3] / radius) * 0.5
                ao = 1.0 - strength * occ / len(dirs)
                attr.data[v.index].color = (ao, ao, ao, 1.0)
            me.color_attributes.active_color = attr
            me.color_attributes.render_color_index = me.color_attributes.active_color_index

    # -- baked two-tone shading
    def shade(self, pairs, ao_cut=0.5, down=-0.62, parts=None):
        """Baked cel shading, applied by finish() after the AO bake: every face painted with a colour
        in `pairs` ({lit cell: shade cell}) drops to its shade cell when it faces the ground in the
        rest pose (world normal below `down`, -1 = straight down) or sits in a crease (the mean AO of
        its corners under `ao_cut`). The palette gate allows one cell per face, so this is how a toy's
        painted shadows are done: undersides and creases keep their form under a flat key light.
        parts = names to limit it to (default: every part). Call it any time before finish()."""
        self._shade.append((dict(pairs), ao_cut, down, set(parts) if parts else None))

    def _apply_shade(self):
        n_done = 0
        for pairs, ao_cut, down, only in self._shade:
            cells = {K._uv_cell(a): K._uv_cell(b) for a, b in pairs.items()}
            for part in self.parts.values():
                if part.ob.type != "MESH" or (only is not None and part.name not in only):
                    continue
                me = part.ob.data
                tint = me.uv_layers.get("Tint")
                ao = me.color_attributes.get("AO")
                if tint is None or ao is None:
                    continue
                nm = part.ob.matrix_world.to_3x3().inverted().transposed()
                for poly in me.polygons:
                    u, v = tint.data[poly.loop_start].uv
                    layer = math.floor(u + 1e-6)
                    hit = None
                    for (cu, cv), to in cells.items():
                        if abs((u - layer) - cu) < 1e-3 and abs(v - cv) < 1e-3:
                            hit = to
                            break
                    if hit is None:
                        continue
                    occ = sum(ao.data[i].color[0] for i in poly.vertices) / len(poly.vertices)
                    if (nm @ poly.normal).normalized().z < down or occ < ao_cut:
                        for li in poly.loop_indices:
                            tint.data[li].uv = (hit[0] + layer, hit[1])
                        n_done += 1
        return n_done

    # -- custom normals of straw() blades (see straw): applied after the last bmesh round trip
    def _apply_wave_normals(self):
        for p in self.parts.values():
            if p.ob.type != "MESH":
                continue
            me = p.ob.data
            attr = me.attributes.get(WAVE_ATTR)
            if attr is None:
                continue
            inv = p.world.to_3x3().inverted()
            want = [V(d.vector) for d in attr.data]
            corner = me.corner_normals
            normals = []
            for loop in me.loops:
                w = want[loop.vertex_index]
                normals.append(tuple((inv @ w).normalized()) if w.length > 0.5 else tuple(corner[loop.index].vector))
            me.attributes.remove(me.attributes[WAVE_ATTR])
            me.normals_split_custom_set(normals)

    # -- checks done at build time (the exported file is checked again by verify_rig2.py)
    def _soft_bands(self, part, bins=10):
        me = part.ob.data
        d = part.soft.get("dir")
        if d is not None:
            dl = (part.world.to_3x3().inverted() @ V(d)).normalized()
            along = [v.co.dot(dl) for v in me.vertices]
        else:
            along = [v.co.length for v in me.vertices]
        hi = max(along)
        used = {min(bins - 1, int(a / hi * bins)) for a in along if a > 0.0} if hi > 1e-6 else set()
        return len(used), hi

    def spec(self):
        return {
            "id": self.id, "height": self.height, "radius": self.radius, "floating": self.floating,
            "tree": {n: p.parent for n, p in self.parts.items()},
            "meshless": [n for n, p in self.parts.items() if p.ob.type != "MESH"],
            "balls": {n: p.ball for n, p in self.parts.items() if p.ball},
            "soft": {n: {"dir": (list(b2g(V(p.soft["dir"]))) if p.soft.get("dir") is not None else None),
                         "rigid": p.soft.get("rigid", 0.0), "loops": p.soft.get("loops", 6)}
                     for n, p in self.parts.items() if p.soft is not None},
            "ik": self.ik, "eyes": self.eyes, "sockets": self.sockets,
            "rotated": [n for n, p in self.parts.items()
                        if any(abs(p.world.to_3x3()[i][j] - (1.0 if i == j else 0.0)) > 1e-6 for i in range(3) for j in range(3))],
        }

    # -- finish: clean, AO, export, spec, sheets
    def finish(self, ao_radius=0.25, ao_strength=0.85, sheet=True, poses=None, pose_views=((-30, 10), (150, 10)),
               weighted=False, closeups=None):
        """closeups: [(suffix, zoom, focus world point)] extra review sheets <id>_<suffix>.png (a face).
        Parts named alt_* (hidden in the game until the animator shows them) are left out of every sheet."""
        if "body" not in self.parts:
            raise RuntimeError("a v2 monster needs a part called 'body'")
        meshes = self.meshes()
        for o in meshes:
            F.cleanup(o)
            if weighted:
                F.weighted_normals(o)
        for p in self.parts.values():
            p.tris = D.tris(p.ob) if p.ob.type == "MESH" else 0
        self._apply_wave_normals()
        deg = F.degenerate_count([self.root])
        bpy.context.view_layer.update()
        lo = V((1e9, 1e9, 1e9))
        hi = V((-1e9, -1e9, -1e9))
        for o in meshes:
            for v in o.data.vertices:
                w = o.matrix_world @ v.co
                lo = V((min(lo.x, w.x), min(lo.y, w.y), min(lo.z, w.z)))
                hi = V((max(hi.x, w.x), max(hi.y, w.y), max(hi.z, w.z)))
        warn = []
        tris = sum(p.tris for p in self.parts.values())
        if len(meshes) > MAX_PARTS:
            warn.append(f"{len(meshes)} mesh parts > {MAX_PARTS}")
        if tris > MAX_TRIS:
            warn.append(f"{tris} triangles > {MAX_TRIS}")
        if not self.floating and abs(lo.z) > 0.03:
            warn.append(f"rest pose does not touch the ground (min height {lo.z:.3f})")
        if hi.z > self.height * 1.1 + 1e-3:
            warn.append(f"height {hi.z:.3f} is more than 10% over the collision height {self.height}")
        for p in self.parts.values():
            if p.soft is not None and p.ob.type == "MESH":
                bands, run = self._soft_bands(p)
                if bands < p.soft.get("loops", 6):
                    warn.append(f"soft part {p.name}: only {bands} bands of vertices along its {run:.2f} m run")
        self.bake_ao(radius=ao_radius, strength=ao_strength)
        shaded = self._apply_shade()
        if shaded:
            print(f"SHADE {self.id}: {shaded} faces dropped to their shade cell")
        self.root["build"] = time.strftime("%Y%m%d-%H%M%S") + f"-{time.time() % 1:.4f}"
        out = K.export([self.root], f"monsters/{self.id}")
        SPEC_DIR.mkdir(exist_ok=True)
        spec = self.spec()
        spec["tris"] = tris
        (SPEC_DIR / f"{self.id}.json").write_text(json.dumps(spec, indent=1))
        sh = None
        if sheet and "--nosheet" not in FLAGS:
            alts = [p.ob for p in self.parts.values() if p.name.startswith("alt_")]
            for o in alts:
                o.hide_render = True
            sh = K.review_sheet([self.root], self.id)
            for cl in (closeups or ()):   # (suffix, zoom, focus[, four (yaw, pitch) views])
                K.review_sheet([self.root], f"{self.id}_{cl[0]}", scale_figure=False, zoom=cl[1], focus=cl[2],
                               views=cl[3] if len(cl) > 3 else None)
            if poses:
                self.pose_sheet(poses, views=pose_views)
            for o in alts:
                o.hide_render = False
        size = hi - lo
        print("PARTS " + "; ".join(f"{p.name}:{p.tris}" + (f"<{p.parent}" if p.parent else "") for p in self.parts.values()))
        for w in warn:
            print(f"WARN {self.id}: {w}")
        print(f"RESULT {self.id} tris={tris} parts={len(meshes)} nodes={len(self.parts)} degenerate={deg} "
              f"size(x,y_up,z)=({size.x:.2f},{size.z:.2f},{size.y:.2f}) minz={lo.z:.3f} maxz={hi.z:.3f} gltf={out} sheet={sh}")
        return tris

    # -- extreme-pose review sheet
    def pose_sheet(self, poses, views=((-30, 10), (150, 10)), size=440, name=None):
        """Renders the rig in each pose from each view into art/review/sheets/<id>_poses.png (one
        column per pose, one row per view, labelled) to look for joint gaps and parts that detach.

        poses: [(label, {part: steps})]. steps is a list of ("x" | "y" | "z", degrees) rotations and
        ("move", (x, y, z)) offsets, in game axes (+X left, +Y up, +Z front), composed left to right
        exactly like  Basis(axis, a) * Basis(axis, b)  in the animator: node = rest * R, plus the offset
        in the parent's space. The numbers can therefore be copied to and from the C++ animator."""
        name = name or f"{self.id}_poses"
        meshes = self.meshes()
        rest = {n: p.ob.matrix_basis.copy() for n, p in self.parts.items()}
        gz, span = -0.005, max(self.height, self.radius * 2.0)
        if self.floating:   # (added for the bat) the ground under the rig, and the frame wide enough for its span
            bpy.context.view_layer.update()
            pts = [o.matrix_world @ v.co for o in meshes for v in o.data.vertices]
            gz = min(p.z for p in pts) - 0.03
            span = max(max(abs(p.x) for p in pts) * 2.0, max(p.z for p in pts) - gz) * 0.8
        ground = K.cylinder("_pose_ground", self.radius * 2.2, 0.01, color="asphalt", loc=(0, 0, gz), verts=24)
        K._preview_colors(meshes + [ground])
        scene = bpy.context.scene
        scene.render.engine = "BLENDER_WORKBENCH"
        shading = scene.display.shading
        shading.light = "STUDIO"
        shading.color_type = "VERTEX"
        shading.show_shadows = True
        shading.show_cavity = True
        scene.render.resolution_x = size
        scene.render.resolution_y = size
        scene.render.film_transparent = False
        world = scene.world or bpy.data.worlds.new("w")
        scene.world = world
        world.color = (0.16, 0.19, 0.32)
        cam_data = bpy.data.cameras.new("pose_cam")
        cam_data.type = "ORTHO"
        cam_data.ortho_scale = span * 1.42
        cam = K._link(bpy.data.objects.new("pose_cam", cam_data))
        scene.camera = cam
        centre = V((0, 0, self.height * 0.5 if not self.floating else 0.0))
        SHEETS.mkdir(parents=True, exist_ok=True)
        tiles = []
        axes = {"x": ("X", 1.0), "y": ("Z", 1.0), "z": ("Y", -1.0)}
        for pi, (label, pose) in enumerate(poses):
            for n, m in rest.items():
                self.parts[n].ob.matrix_basis = m.copy()
            for n, steps in pose.items():
                if n not in self.parts:
                    raise KeyError(f"pose '{label}': no part '{n}'")
                rot = Matrix.Identity(4)
                off = V((0, 0, 0))
                for kind, val in steps:
                    if kind == "move":
                        off += g2b(val)
                    else:
                        ax, sign = axes[kind]
                        rot = rot @ Matrix.Rotation(math.radians(val) * sign, 4, ax)
                self.parts[n].ob.matrix_basis = Matrix.Translation(off) @ rest[n] @ rot
            bpy.context.view_layer.update()
            for vi, (yaw, pitch) in enumerate(views):
                d = V((math.sin(math.radians(yaw)) * math.cos(math.radians(pitch)),
                       -math.cos(math.radians(yaw)) * math.cos(math.radians(pitch)), math.sin(math.radians(pitch))))
                cam.location = centre + d * 6.0
                cam.rotation_euler = (-d).to_track_quat("-Z", "Y").to_euler()
                path = SHEETS / f"_{name}_{pi}_{vi}.png"
                scene.render.filepath = str(path)
                bpy.ops.render.render(write_still=True)
                tiles.append({"file": str(path), "col": pi, "row": vi, "label": label if vi == 0 else ""})
        for n, m in rest.items():
            self.parts[n].ob.matrix_basis = m
        bpy.data.objects.remove(cam, do_unlink=True)
        bpy.data.objects.remove(ground, do_unlink=True)
        for o in meshes:
            prev = o.data.color_attributes.get("Preview")
            if prev:
                o.data.color_attributes.remove(prev)
            ao = o.data.color_attributes.get("AO")
            if ao:
                o.data.color_attributes.active_color = ao
        out = SHEETS / f"{name}.png"
        manifest = SHEETS / f"_{name}.json"
        manifest.write_text(json.dumps({"out": str(out), "tiles": tiles, "cols": len(poses), "rows": len(views)}))
        # Composed (with labels) by the system Python, which has PIL; Blender's does not.
        r = subprocess.run(["python", str(HERE2 / "sheet_compose.py"), str(manifest)], capture_output=True, text=True)
        print((r.stdout + r.stderr).strip() or f"pose sheet -> {out}")
        return out


# ---------------------------------------------------------------- eyes
def _dome(name, w, h, d, segs, rings, color, mat="flat"):
    """Half ellipsoid in the eye frame (x right, z up, bulging toward -y): w, h half sizes, d depth."""
    prof = [(math.cos(math.pi / 2 * k / rings), d * math.sin(math.pi / 2 * k / rings)) for k in range(rings)] + [(0.0, d)]
    ob = D.lathe(name, prof, segs, color=color, mat=mat, project="sphere", center=(0, 0, 0))
    # lathe axis z -> eye out axis (-y); radial x, y -> eye x, z
    ob.data.transform(Matrix(((w, 0, 0, 0), (0, 0, -1, 0), (0, h, 0, 0), (0, 0, 0, 1))))
    bm = bmesh.new()
    bm.from_mesh(ob.data)
    bmesh.ops.recalc_face_normals(bm, faces=bm.faces)
    bm.to_mesh(ob.data)
    bm.free()
    ob.data.update()
    return ob


def _decal(name, cx, cz, rx, rz, w, h, d, lift, color, segs=10, rings=2, mat="flat", rot=0.0):
    """Disc riding `lift` above the dome's surface, centred (cx, cz) in the eye frame (rot: degrees,
    turning the oval counter-clockwise as seen from the front)."""
    def surf(x, z):
        q = 1.0 - (x / w) ** 2 - (z / h) ** 2
        return -(d * math.sqrt(max(q, 0.0)) + lift)
    cr, sr = math.cos(math.radians(rot)), math.sin(math.radians(rot))
    bm = bmesh.new()
    c = bm.verts.new((cx, surf(cx, cz) - lift * 0.5, cz))
    prev = None
    for k in range(1, rings + 1):
        f = k / rings
        ring = []
        for j in range(segs):
            lx, lz = math.cos(TAU * j / segs) * rx * f, math.sin(TAU * j / segs) * rz * f
            ring.append(bm.verts.new((cx + lx * cr - lz * sr, 0.0, cz + lx * sr + lz * cr)))
        for v in ring:
            v.co.y = surf(v.co.x, v.co.z) - (lift * 0.5 * (1.0 - f))
        for j in range(segs):
            j2 = (j + 1) % segs
            if prev is None:
                bm.faces.new((c, ring[j2], ring[j]))
            else:
                bm.faces.new((prev[j], prev[j2], ring[j2], ring[j]))
        prev = ring
    bmesh.ops.recalc_face_normals(bm, faces=bm.faces)
    # recalc may pick either side for an open disc: make it face out of the eye (-y)
    bm.normal_update()
    if sum(f.normal.y for f in bm.faces) > 0:
        bmesh.ops.reverse_faces(bm, faces=bm.faces)
    return D.mesh(bm, name, color, mat, "box", smooth=True)


def _crescent(name, cx, cz, rx, rz, k, w, h, d, lift, color, n=14, puff=0.0, mat="flat"):
    """A smile-shaped band riding `lift` above the dome: between the lower half of the oval (rx, rz) round
    (cx, cz) and the same arc squashed to k * rz, so it is thickest in the middle and runs to points at
    the sides (the glow in the bottom of an anime eye)."""
    def surf(x, z):
        q = 1.0 - (x / w) ** 2 - (z / h) ** 2
        return -(d * math.sqrt(max(q, 0.0)) + lift)
    bm = bmesh.new()
    rows = []
    for i in range(n + 1):
        u = -0.985 + 1.97 * i / n
        s = math.sqrt(max(1.0 - u * u, 0.0))
        x = cx + rx * u
        zb, zt = cz - rz * s, cz - rz * k * s
        zm = (zb + zt) * 0.5
        rows.append([bm.verts.new((x, surf(x, z) - (puff if z == zm else 0.0), z)) for z in (zt, zm, zb)])
    for i in range(n):
        a, b = rows[i], rows[i + 1]
        bm.faces.new((a[0], b[0], b[1], a[1]))
        bm.faces.new((a[1], b[1], b[2], a[2]))
    bm.normal_update()
    if sum(f.normal.y for f in bm.faces) > 0:
        bmesh.ops.reverse_faces(bm, faces=bm.faces)
    return D.mesh(bm, name, color, mat, "box", smooth=True)


def anime_layers(name, w, h, d, dome="ink", tint=None, glint=("burnt_amber", "amber"), hl="eye_white", look=(0.0, 0.0),
                 segs=20, rings=5, lift=0.0016, glint_y=-0.3, glint_w=0.54, glint_h=0.4, sparkle=True, hl_scale=1.0):
    """The cute anime eye (2026-11-17, user: the old anime eyes "read cuter ... and not so uncanny", a little
    higher fidelity): a glossy dark `dome`, a soft `tint` (a deep colour) in its lower part so it has depth
    without the shader's sheen, a big tilted highlight up and to the viewer's left, a small one right of
    centre and a tiny sparkle beside the big one (all `hl`, fixed on the eye), and a two-tone crescent glow
    in the bottom of the eye (glint = (outer, inner) colours, or None) that the pupil part carries, so a
    look slides it. No white sclera, no iris disc, no outline ring.
    Returns (eye objects, pupil objects) in the Blender eye frame (x right as seen from the front, -y out,
    z up), the dome's base at the origin."""
    m = min(w, h)
    eye = [_dome(name + "_dome", w, h, d, segs, rings, dome)]
    if tint:   # a low lens of deep colour: the eye darkens toward the top like a lit glass bead
        eye.append(_crescent(name + "_tint", 0.0, -h * 0.14, w * 0.78, h * 0.7, -0.2, w, h, d, lift, tint, n=12))
    s = hl_scale
    eye.append(_decal(name + "_hl1", -w * 0.28, h * 0.34, m * 0.41 * s, m * 0.31 * s, w, h, d, lift * 4.2, hl, segs=14, rings=2, rot=-30.0))
    eye.append(_decal(name + "_hl2", w * 0.4, -h * 0.06, m * 0.14 * s, m * 0.125 * s, w, h, d, lift * 4.2, hl, segs=10, rings=1))
    if sparkle:   # a pin-prick under the small one
        eye.append(_decal(name + "_hl3", w * 0.27, -h * 0.25, m * 0.05 * s, m * 0.05 * s, w, h, d, lift * 4.2, hl, segs=8, rings=1))
    pupil = []
    if glint:
        px, pz = look[0] * w, look[1] * h + glint_y * h
        outer, inner = glint if isinstance(glint, (tuple, list)) else (glint, None)
        # The pupil part slides flat (pupil_travel x reach, 0.3 of the eye by default) with these heights baked at
        # rest: the glint is kept small and high, and rides well above the dome, so a full look down only just
        # tips its points past the rim and a look up does not sink it (checked numerically, 2026-11-17). Keep the
        # dome about 0.32 of the eye's width deep for the same reason.
        pupil.append(_crescent(name + "_gl", px, pz, w * glint_w, h * glint_h, -0.22, w, h, d, lift * 2.4, outer, n=10))
        if inner:
            pupil.append(_crescent(name + "_gl2", px, pz - h * glint_h * 0.2, w * glint_w * 0.62, h * glint_h * 0.68, -0.28, w, h, d,
                                   lift * 3.6, inner, n=8))
    return eye, pupil


def _eye_frame(centre, n, up, roll):
    frame = Matrix.Translation(centre)
    u = V(up) - n * V(up).dot(n)
    u = u.normalized() if u.length > 1e-6 else n.orthogonal().normalized()
    rot = Matrix(((-n).cross(u).normalized(), -n, u)).transposed()
    if roll:
        rot = rot @ Matrix.Rotation(math.radians(-roll), 3, "Y")
    return frame @ rot.to_4x4()


def anime_eye(rig, name, pupil_name, centre, normal, w, h, parent="head", up=(0, 0, 1), roll=0.0, depth=None,
              eye_extra=(), **kw):
    """anime_layers() as the two MonsterFace parts (same contract as eye()): `name` = the dome, its tint and
    the highlights (a blink scales its Y), `pupil_name` = the crescent glow (child of the eye, same origin;
    the animator slides it to look). eye_extra: more objects for the eye part, authored in the game's eye
    frame (x, y up, +z out), e.g. a lash line. Returns (eye object, pupil object)."""
    centre, n = V(centre), V(normal).normalized()
    d = depth if depth is not None else min(w, h) * 0.32
    objs, inner = anime_layers(name, w, h, d, **kw)
    frame = _eye_frame(centre, n, up, roll)
    for o in objs + inner:
        o.matrix_world = frame
    conv = Matrix(((1, 0, 0, 0), (0, 0, -1, 0), (0, 1, 0, 0), (0, 0, 0, 1)))
    for o in eye_extra:
        o.matrix_world = frame @ conv @ o.matrix_basis.copy()
        objs.append(o)
    e = rig.part(name, objs, centre, parent=parent, z_out=n, up=up, roll=roll, ground=False)
    p = rig.part(pupil_name, inner, centre, parent=name, z_out=n, up=up, roll=roll, ground=False)
    rig.eyes[name] = pupil_name
    return e, p


def eye(rig, name, pupil_name, centre, normal, w, h, parent="head", up=(0, 0, 1), roll=0.0, depth=None,
        sclera="eye_white", iris="dragon_teal", iris_r=0.62, pupil="ink", pupil_r=0.34, pupil_tall=1.0,
        glint="eye_white", glints=True, look=(0.0, 0.0), segs=16, rings=4, lift=0.0016, iris2=None, iris2_r=0.45,
        eye_extra=(), pupil_extra=()):
    """An eye as two parts for MonsterFace.

      `name`        the eye: a `sclera` dome w x h (half sizes) and `depth` deep, plus two authored
                    glints that stay put (big one up and to the viewer's left, small one low right).
                    Origin = the eye centre (on the surface it sits on), local +Z out along `normal`,
                    +Y up. A blink scales its Y, which closes everything on it.
      `pupil_name`  child of the eye, same origin: the `iris` disc (None for none) with the `pupil`
                    on it. The animator slides it in the eye's XY plane (30% of the eye's radius).

    iris2 (with iris2_r) adds an inner iris ring in a second colour.
    eye_extra / pupil_extra: more objects for the eye / the pupil part, authored at the origin in the
    eye's own frame as the game sees it (x to the eye's right as seen from the front, y up, +z out of
    the face): a button() for a button eye, glints that must float above it. pupil=None leaves out the
    pupil disc (when pupil_extra is the whole pupil).
    iris_r and pupil_r are fractions of min(w, h); look = rest offset of the pupil as fractions of
    (w, h). Colours: a white sclera with a coloured iris for a living eye; an ink dome with a dim
    glow iris and a bright glow pupil for a skull's socket. Returns (eye object, pupil object)."""
    centre, n = V(centre), V(normal).normalized()
    d = depth if depth is not None else min(w, h) * 0.32
    m = min(w, h)
    objs = [_dome(name + "_sclera", w, h, d, segs, rings, sclera)]
    # Layers above the dome, in units of `lift`: iris 1, (inner iris 2.2), pupil 2 (3.4), glints 3.2 (4.6).
    # With an inner iris the layers are spread further apart, or it z-fights with the iris at a distance.
    k_pupil, k_glint = (3.4, 4.6) if iris2 else (2.0, 3.2)
    if glints:
        objs.append(_decal(name + "_g1", -w * 0.3, h * 0.34, m * 0.25, m * 0.2, w, h, d, lift * k_glint, glint, segs=8))
        objs.append(_decal(name + "_g2", w * 0.34, -h * 0.3, m * 0.11, m * 0.1, w, h, d, lift * k_glint, glint, segs=6, rings=1))
    px, pz = look[0] * w, look[1] * h
    inner = []
    if iris:
        inner.append(_decal(name + "_iris", px, pz, m * iris_r, m * iris_r, w, h, d, lift, iris, segs=14))
    if iris2:   # a second, inner iris ring between the iris and the pupil (a two-tone iris)
        inner.append(_decal(name + "_iris2", px, pz, m * iris2_r, m * iris2_r, w, h, d, lift * 2.2, iris2, segs=12))
    if pupil:
        inner.append(_decal(name + "_pupil", px, pz, m * pupil_r, m * pupil_r * pupil_tall, w, h, d, lift * k_pupil, pupil, segs=12))
    frame = Matrix.Translation(centre)
    u = V(up) - n * V(up).dot(n)
    u = u.normalized() if u.length > 1e-6 else n.orthogonal().normalized()
    rot = Matrix(((-n).cross(u).normalized(), -n, u)).transposed()
    if roll:
        rot = rot @ Matrix.Rotation(math.radians(-roll), 3, "Y")
    frame = frame @ rot.to_4x4()
    for o in objs + inner:
        o.matrix_world = frame
    # extras come in the game's eye frame (x, y up, z out); the Blender eye frame is (x, -out, up)
    conv = Matrix(((1, 0, 0, 0), (0, 0, -1, 0), (0, 1, 0, 0), (0, 0, 0, 1)))
    for group, extra in ((objs, eye_extra), (inner, pupil_extra)):
        for o in extra:
            o.matrix_world = frame @ conv @ o.matrix_basis.copy()
            group.append(o)
    e = rig.part(name, objs, centre, parent=parent, z_out=n, up=up, roll=roll, ground=False)
    p = rig.part(pupil_name, inner, centre, parent=name, z_out=n, up=up, roll=roll, ground=False)
    rig.eyes[name] = pupil_name
    return e, p


def socket_eye(rig, name, light_name, centre, normal, w, h, depth, parent="head", up=(0, 0, 1), roll=0.0,
               back="ink", light="glow_magic_teal", light_r=0.3, light_tall=1.2, light_back=0.4, look=(0.0, 0.0),
               segs=12, rings=3, light_segs=8, light_rings=5):
    """A skull's eye (no lens, no rings): a dark socket with a small soft light floating inside it.

      `name`        the socket lining: a `back` coloured bowl w x h (half sizes) sinking `depth` behind
                    `centre` (put `centre` a little inside the socket's opening, `normal` out of the
                    face). It is what the dark of the socket is made of, and it gives the eye its reach.
                    Origin = `centre`, local +Z out along `normal`, +Y up (the MonsterFace contract).
      `light_name`  child of the socket: the light, a small rounded `light` blob (light_r of min(w, h)
                    across, light_tall times as tall) floating `light_back` of the way down into the bowl,
                    at rest offset `look` (fractions of w, h). The animator slides it in the socket's XY
                    plane to look, and scales it uniformly to flare, flinch and wink out (a Y-scale blink
                    would leave a bright line in the dark). The bloom gives it its soft edge, so keep its
                    glow low at rest.
    Returns (socket object, light object)."""
    centre, n = V(centre), V(normal).normalized()
    m = min(w, h)
    bowl = _dome(name + "_bowl", w, h, depth, segs, rings, back)
    # turn the dome into a bowl: mirror it behind the origin plane and keep its faces toward the viewer
    bowl.data.transform(Matrix.Scale(-1.0, 4, (0, 1, 0)))
    bm = bmesh.new()
    bm.from_mesh(bowl.data)
    bmesh.ops.reverse_faces(bm, faces=bm.faces)
    bm.to_mesh(bowl.data)
    bm.free()
    bowl.data.update()
    lr = m * light_r
    orb = blob(name + "_light", (look[0] * w, depth * light_back, look[1] * h), (lr, lr * 0.7, lr * light_tall),  # noqa: F405
               segs=light_segs, rings=light_rings, color=light)
    bpy.context.view_layer.update()
    orb.data.transform(orb.matrix_world)
    orb.matrix_world = Matrix.Identity(4)
    frame = Matrix.Translation(centre)
    u = V(up) - n * V(up).dot(n)
    u = u.normalized() if u.length > 1e-6 else n.orthogonal().normalized()
    rot = Matrix(((-n).cross(u).normalized(), -n, u)).transposed()
    if roll:
        rot = rot @ Matrix.Rotation(math.radians(-roll), 3, "Y")
    frame = frame @ rot.to_4x4()
    bowl.matrix_world = frame
    orb.matrix_world = frame
    e = rig.part(name, [bowl], centre, parent=parent, z_out=n, up=up, roll=roll, ground=False)
    p = rig.part(light_name, [orb], centre, parent=name, z_out=n, up=up, roll=roll, ground=False)
    rig.eyes[name] = light_name
    return e, p


# ---------------------------------------------------------------- raised pads (added for the toadstool)
def pad(name, tree, centre, normal, r, h, segs=14, rings=(0.0, 0.5, 0.8, 0.94, 1.0), heights=None, color="bone",
        edge_color=None, mat="flat", sink=0.004, up=None, aspect=1.0, outline=None):
    """A raised pad that hugs a curved surface: mushroom spots, patches, warts, scales.

    Every vertex is found on the surface by a ray cast (against `tree`, a BVH: use M.proxy_tree of a
    high-resolution copy) along -normal from a disc of radius r round `centre`, then lifted along the
    surface normal by its ring's height: a flat top `h` proud, a rounded shoulder, and the outer ring
    `sink` under the surface, so no gap shows under the edge however the low-resolution surface is
    faceted. heights: one per ring (default flat top, shoulder, sunk edge). edge_color paints the
    shoulder (the outer two rings of quads). About segs * (2 * len(rings) - 3) triangles.
    (Added for the bat, all optional:) up = the direction of the pad's second axis on the surface,
    aspect = its length along `up` as a multiple of r (an oval), outline(angle) -> radius multiplier for
    a wavy or tufted edge (applied to the outer two rings only, so the top stays a smooth oval)."""
    n = V(normal).normalized()
    if up is not None:
        t2 = V(up) - n * V(up).dot(n)
        t2 = t2.normalized() if t2.length > 1e-6 else n.orthogonal().normalized()
        t1 = t2.cross(n).normalized()
    else:
        t1 = n.orthogonal().normalized()
        t2 = n.cross(t1)
    hs_ = list(heights) if heights else [h, h, h * 0.9, h * 0.5, -sink]
    c0 = V(centre)

    def onsurf(off):
        q = c0 + off
        loc, nrm, _, _ = tree.ray_cast(q + n * 0.25, -n, 0.6)
        if loc is None:
            return q, n
        return V(loc), V(nrm)
    bm = bmesh.new()
    rows = []
    for ri, (f, hh) in enumerate(zip(rings, hs_)):
        if f <= 0.0:
            loc, nrm = onsurf(V((0, 0, 0)))
            rows.append([bm.verts.new(loc + nrm * hh)])
            continue
        row = []
        for j in range(segs):
            a = TAU * j / segs
            k = outline(a) if (outline is not None and ri >= len(rings) - 2) else 1.0
            loc, nrm = onsurf((t1 * math.cos(a) + t2 * (math.sin(a) * aspect)) * (r * f * k))
            row.append(bm.verts.new(loc + nrm * hh))
        rows.append(row)
    edge = []
    for k in range(1, len(rows)):
        a, b = rows[k - 1], rows[k]
        for j in range(segs):
            j2 = (j + 1) % segs
            if len(a) == 1:
                f = bm.faces.new((a[0], b[j], b[j2]))
            else:
                f = bm.faces.new((a[j], b[j], b[j2], a[j2]))
            if k >= len(rows) - 2:
                edge.append(f)
    bm.normal_update()
    if sum(f.normal.dot(n) for f in bm.faces) < 0.0:
        bmesh.ops.reverse_faces(bm, faces=list(bm.faces))
    extra = [(edge, edge_color, mat)] if edge_color else []
    return D.mesh(bm, name, color, mat, "box", smooth=True, extra=extra)
