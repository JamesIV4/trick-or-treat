"""Shared helpers for the pumpkins_farm lane (batch01).

Import from a build script in this folder:
    import sys, pathlib
    HERE = pathlib.Path(__file__).resolve().parent
    sys.path.insert(0, str(HERE)); sys.path.insert(0, str(HERE.parents[2] / "tools"))
    import pumpkin_lib as P
    K = P.K

What lives here (things the kit does not provide):
  * pumpkin_body   - lobed pumpkin/gourd built directly in bmesh, with narrow painted groove strips,
                     top dimple, per-lobe lumpiness, twist, lopsidedness and optional flat top.
  * curly_stem / tendril - stems and curly vine tendrils (kit tube based).
  * carve          - jack-o'-lantern carving: Exact boolean of recessed, tapered pockets into a body.
                     Pocket walls are painted rim_color (pumpkin_dark), pocket floors glow_jack.
  * face shapes    - outlines for eyes / mouths in the front face plane (x right, z up, metres).
  * ribbon         - thin leaf / ribbon with a midrib crease (closed mesh, visible from both sides).
  * cleanup / finish - triangulate, kill degenerate triangles, export + sheet + report.
"""
from __future__ import annotations

import math
import pathlib
import random
import sys

HERE = pathlib.Path(__file__).resolve().parent
sys.path.insert(0, str(HERE.parents[2] / "tools"))

import bmesh  # noqa: E402
import bpy  # noqa: E402
from mathutils import Vector  # noqa: E402
from mathutils.bvhtree import BVHTree  # noqa: E402

import lowpoly_kit as K  # noqa: E402


# ----------------------------------------------------------------------------- pumpkin body
def pumpkin_body(name, width=0.6, height=0.45, lobes=8, groove=0.10, groove_w=0.10, peaks=2,
                 rings=7, color="pumpkin", groove_color="pumpkin_dark", top_dimple=0.06,
                 bottom_dimple=0.015, fullness=0.72, flatness=0.8, twist=0.0, lumps=0.04, seed=1,
                 lopsided=0.0, lop_dir=0.0, lean=(0.0, 0.0), flat_top=None, loc=(0, 0, 0),
                 smooth=True, top_color=None, bottom_color=None, belly=1.0):
    """Lobed pumpkin with its base at z=0 (before loc).

    peaks: vertices per lobe between groove strips (1-3). Segments = lobes * (2 + peaks - 1)...
    exactly lobes * (1 + peaks) + lobes (each lobe = 2 groove-strip verts + peaks verts).
    fullness < 1 makes the side profile boxier; flatness < 1 flattens top and bottom.
    flat_top: fraction of height above which the surface is squashed flat (walkable tops).
    """
    rnd = random.Random(seed)
    R = width / 2.0
    lobe_scale = [1.0 + lumps * rnd.uniform(-1, 1) for _ in range(lobes)]
    fr = [0.0, groove_w] + [groove_w + (1 - groove_w) * (k + 1) / (peaks + 1) for k in range(peaks)]
    per = len(fr)
    segs = lobes * per

    def lobe_mod(f):
        if f <= groove_w + 1e-6:
            return 1.0 - groove
        t = (f - groove_w) / (1.0 - groove_w)
        return 1.0 - groove * (1.0 - math.sin(math.pi * t)) ** 1.6

    bm = bmesh.new()
    ring_verts = []
    zs = []
    for k in range(1, rings):
        phi = math.pi * k / rings
        u = math.cos(phi)
        p = math.sin(phi) ** fullness
        z = 0.5 + 0.5 * math.copysign(abs(u) ** flatness, u)
        zs.append(z)
        ring = []
        for j in range(segs):
            L, f = divmod(j, per)
            fl = fr[f]
            theta = 2 * math.pi * (L + fl) / lobes + twist * (z - 0.5)
            s = lobe_scale[L]
            if fl == 0.0:  # shared groove edge: average with the previous lobe
                s = 0.5 * (s + lobe_scale[L - 1])
            r = R * p * lobe_mod(fl) * s
            r *= 1.0 + lopsided * math.cos(theta - lop_dir)
            ring.append((theta, r, z))
        ring_verts.append(ring)
    top_z = zs[0] - top_dimple / height
    bot_z = zs[-1] + bottom_dimple / height
    lo = min(min(zs), bot_z)
    hi = max(max(zs), top_z)

    def zmap(z):
        zn = min(max((z - lo) / (hi - lo), 0.0), 1.0)
        return zn ** belly * height

    bv_rings = []
    for ring in ring_verts:
        row = []
        for theta, r, z in ring:
            zz = zmap(z)
            if flat_top is not None and zz > flat_top * height:
                zz = flat_top * height + (zz - flat_top * height) * 0.25
            co = Vector((math.cos(theta) * r, math.sin(theta) * r, zz))
            co.x += lean[0] * zz
            co.y += lean[1] * zz
            row.append(bm.verts.new(co))
        bv_rings.append(row)
    tz = zmap(top_z)
    if flat_top is not None and tz > flat_top * height:
        tz = flat_top * height + (tz - flat_top * height) * 0.25
    top = bm.verts.new((lean[0] * tz, lean[1] * tz, tz))
    bot = bm.verts.new((0, 0, zmap(bot_z)))

    groove_faces = []
    top_faces = []
    bottom_faces = []
    for i in range(len(bv_rings) - 1):
        a, b = bv_rings[i], bv_rings[i + 1]
        for j in range(segs):
            j2 = (j + 1) % segs
            f = bm.faces.new((a[j], b[j], b[j2], a[j2]))
            if j % per == 0:
                groove_faces.append(f)
    for j in range(segs):
        j2 = (j + 1) % segs
        f = bm.faces.new((top, bv_rings[0][j], bv_rings[0][j2]))
        top_faces.append(f)
        if j % per == 0:
            groove_faces.append(f)
        f = bm.faces.new((bot, bv_rings[-1][j2], bv_rings[-1][j]))
        bottom_faces.append(f)
    bmesh.ops.recalc_face_normals(bm, faces=bm.faces)
    K._paint_bm(bm, color)
    if groove_color:
        K._paint_bm(bm, groove_color, groove_faces)
    if top_color:
        K._paint_bm(bm, top_color, top_faces)
    if bottom_color:
        K._paint_bm(bm, bottom_color, bottom_faces)
    for f in bm.faces:
        f.smooth = smooth
    for f in groove_faces:  # hard crease along the grooves
        f.smooth = False
    me = bpy.data.meshes.new(name)
    bm.to_mesh(me)
    bm.free()
    ob = K._link(bpy.data.objects.new(name, me))
    ob.location = Vector(loc)
    ob["top"] = (lean[0] * tz, lean[1] * tz, tz)
    return ob


def top_point(body):
    """The top dimple (stem socket) of a pumpkin_body, in world space."""
    bpy.context.view_layer.update()
    return body.matrix_world @ Vector(body["top"])


# ----------------------------------------------------------------------------- stems, vines
def curly_stem(name, base, height=0.14, radius=0.04, color="stem_brown", curl=0.06, lean_dir=0.0,
               sides=6, pts=5, flare=True, tip_ratio=0.55):
    """Chunky stem rising from base with a bend that curls over at the top."""
    bx, by, bz = base
    points = []
    for i in range(pts):
        t = i / (pts - 1)
        bend = curl * t * t
        points.append((bx + math.cos(lean_dir) * bend, by + math.sin(lean_dir) * bend,
                       bz - 0.01 + height * t - 0.5 * curl * t ** 3))
    parts = [K.tube(name, points, radius=radius, radius_end=radius * tip_ratio, color=color, sides=sides,
                    smooth=False)]
    if flare:
        parts.append(K.cylinder(name + "_flare", radius * 1.9, 0.035, color=color, verts=sides,
                                radius_top=radius * 1.05, loc=(bx, by, bz - 0.005)))
    return parts


def tendril(name, start, turns=1.5, r0=0.05, r1=0.015, rise=0.03, n=10, radius=0.008, color="vine",
            sides=4, phase=0.0, axis_dir=None):
    """Little curly spiral tendril (vine curl) starting at start."""
    sx, sy, sz = start
    pts = []
    for i in range(n):
        t = i / (n - 1)
        a = phase + 2 * math.pi * turns * t
        r = r0 + (r1 - r0) * t
        pts.append((sx + math.cos(a) * r - math.cos(phase) * r0, sy + math.sin(a) * r - math.sin(phase) * r0,
                    sz + rise * t))
    return K.tube(name, pts, radius=radius, color=color, sides=sides, smooth=True)


def blob_tube(name, points, radii, sides=8, color="gourd_yellow", stripe_color=None, stripe_every=2,
              smooth=True, cap_offset=0.3):
    """Rounded lathe along a (possibly curved) spine: per-point radius, pole verts at both ends.
    stripe_color paints every stripe_every-th column (striped gourds)."""
    bm = bmesh.new()
    pts = [Vector(p) for p in points]
    n = len(pts)
    rings = []
    prev_side = None
    for i, p in enumerate(pts):
        t = (pts[min(i + 1, n - 1)] - pts[max(i - 1, 0)]).normalized()
        ref = Vector((1, 0, 0)) if prev_side is None else prev_side
        side = (ref - t * ref.dot(t))
        if side.length < 1e-6:
            side = t.cross(Vector((0, 1, 0)))
        side.normalize()
        prev_side = side
        up2 = t.cross(side).normalized()
        rings.append([bm.verts.new(p + (side * math.cos(2 * math.pi * j / sides) +
                                        up2 * math.sin(2 * math.pi * j / sides)) * radii[i])
                      for j in range(sides)])
    t0 = (pts[1] - pts[0]).normalized()
    t1 = (pts[-1] - pts[-2]).normalized()
    p0 = bm.verts.new(pts[0] - t0 * radii[0] * cap_offset)
    p1 = bm.verts.new(pts[-1] + t1 * radii[-1] * cap_offset)
    stripes = []
    for i in range(n - 1):
        for j in range(sides):
            f = bm.faces.new((rings[i][j], rings[i + 1][j], rings[i + 1][(j + 1) % sides], rings[i][(j + 1) % sides]))
            if j % stripe_every == 0:
                stripes.append(f)
    for j in range(sides):
        f = bm.faces.new((p0, rings[0][(j + 1) % sides], rings[0][j]))
        if j % stripe_every == 0:
            stripes.append(f)
        f = bm.faces.new((p1, rings[-1][j], rings[-1][(j + 1) % sides]))
        if j % stripe_every == 0:
            stripes.append(f)
    bmesh.ops.recalc_face_normals(bm, faces=bm.faces)
    K._paint_bm(bm, color)
    if stripe_color:
        K._paint_bm(bm, stripe_color, stripes)
    return _finish_keep_paint(bm, name, smooth)


def slab(name, size, loc=(0, 0, 0), color="plank", omit=(), rot=(0, 0, 0), colors=None):
    """Box that skips hidden faces. omit: any of '+x','-x','+y','-y','+z','-z'.
    colors: optional {side: palette name} overrides per side."""
    sx, sy, sz = (s / 2 for s in size)
    bm = bmesh.new()
    v = {}
    for ix in (-1, 1):
        for iy in (-1, 1):
            for iz in (-1, 1):
                v[(ix, iy, iz)] = bm.verts.new((ix * sx, iy * sy, iz * sz))
    sides = {
        "+x": [(1, -1, -1), (1, 1, -1), (1, 1, 1), (1, -1, 1)],
        "-x": [(-1, -1, -1), (-1, -1, 1), (-1, 1, 1), (-1, 1, -1)],
        "+y": [(-1, 1, -1), (-1, 1, 1), (1, 1, 1), (1, 1, -1)],
        "-y": [(-1, -1, -1), (1, -1, -1), (1, -1, 1), (-1, -1, 1)],
        "+z": [(-1, -1, 1), (1, -1, 1), (1, 1, 1), (-1, 1, 1)],
        "-z": [(-1, -1, -1), (-1, 1, -1), (1, 1, -1), (1, -1, -1)],
    }
    uv = bm.loops.layers.uv.verify()
    for key, quad in sides.items():
        if key in omit:
            continue
        f = bm.faces.new([v[c] for c in quad])
        u, w = K._uv_cell((colors or {}).get(key, color))
        for loop in f.loops:
            loop[uv].uv = (u, w)
    ob = _finish_keep_paint(bm, name, False)
    ob.location = Vector(loc)
    ob.rotation_euler = [math.radians(a) for a in rot]
    return ob


def lathe(name, profile, segs=12, color="wood", band_colors=None, column_colors=None, cap_top=True,
          cap_bottom=True, smooth=False, top_color=None, bottom_color=None, phase=0.0):
    """Surface of revolution from profile [(r, z), ...] bottom to top.
    band_colors: per band (between profile points i and i+1) colour or None.
    column_colors: list cycled per column (staves)."""
    bm = bmesh.new()
    uv = bm.loops.layers.uv.verify()
    rings = []
    for r, z in profile:
        rings.append([bm.verts.new((math.cos(2 * math.pi * j / segs + phase) * r,
                                    math.sin(2 * math.pi * j / segs + phase) * r, z)) for j in range(segs)])

    def paint(f, c):
        u, w = K._uv_cell(c)
        for loop in f.loops:
            loop[uv].uv = (u, w)

    for i in range(len(rings) - 1):
        for j in range(segs):
            f = bm.faces.new((rings[i][j], rings[i][(j + 1) % segs], rings[i + 1][(j + 1) % segs], rings[i + 1][j]))
            c = color
            if column_colors:
                c = column_colors[j % len(column_colors)]
            if band_colors and band_colors[i]:
                c = band_colors[i]
            paint(f, c)
            f.smooth = smooth
    if cap_bottom:
        f = bm.faces.new(list(reversed(rings[0])))
        paint(f, bottom_color or color)
    if cap_top:
        f = bm.faces.new(rings[-1])
        paint(f, top_color or color)
    bmesh.ops.recalc_face_normals(bm, faces=bm.faces)
    return _finish_keep_paint(bm, name, smooth) if not smooth else _finish_keep_paint(bm, name, True)


# ----------------------------------------------------------------------------- ribbon leaf
def ribbon(name, points, widths, thick=0.01, color="vine", up=(0, 0, 1), under_color=None, smooth=False,
           crease=1.0):
    """Leaf/ribbon through points. Cross-section is a flat diamond (left, top rib, right, bottom rib),
    so it is closed and reads from both sides. A width of 0 at the end makes a pointed tip.
    up: the leaf surface normal hint (single vector or per-point list)."""
    bm = bmesh.new()
    pts = [Vector(p) for p in points]
    n = len(pts)
    ups = up if isinstance(up[0], (list, tuple, Vector)) else [up] * n
    rings = []
    tip = None
    for i, p in enumerate(pts):
        t = (pts[min(i + 1, n - 1)] - pts[max(i - 1, 0)]).normalized()
        u = Vector(ups[i]).normalized()
        side = t.cross(u)
        if side.length < 1e-6:
            side = t.cross(Vector((1, 0, 0)))
        side.normalize()
        u2 = side.cross(t).normalized()
        w = widths[i]
        if w <= 1e-6 and i == n - 1:
            tip = bm.verts.new(p)
            break
        th = thick * (crease if 0 < i < n - 1 else 1.0)
        rings.append([bm.verts.new(p - side * w / 2), bm.verts.new(p + u2 * th),
                      bm.verts.new(p + side * w / 2), bm.verts.new(p - u2 * th)])
    top_faces = []
    for i in range(len(rings) - 1):
        a, b = rings[i], rings[i + 1]
        for j in range(4):
            f = bm.faces.new((a[j], b[j], b[(j + 1) % 4], a[(j + 1) % 4]))
            if j in (0, 1):
                top_faces.append(f)
    bm.faces.new(list(reversed(rings[0])))
    if tip is not None:
        last = rings[-1]
        for j in range(4):
            f = bm.faces.new((last[j], tip, last[(j + 1) % 4]))
            if j in (0, 1):
                top_faces.append(f)
    else:
        bm.faces.new(rings[-1])
    bmesh.ops.recalc_face_normals(bm, faces=bm.faces)
    K._paint_bm(bm, color)
    if under_color:
        under = [f for f in bm.faces if f not in top_faces]
        K._paint_bm(bm, under_color, under)
    return _finish_keep_paint(bm, name, smooth)


def _finish_keep_paint(bm, name, smooth):
    for f in bm.faces:
        f.smooth = smooth
    me = bpy.data.meshes.new(name)
    bm.to_mesh(me)
    bm.free()
    return K._link(bpy.data.objects.new(name, me))


def mesh_from_bm(bm, name, smooth=False):
    """Link a bmesh that is already painted (UVs set) as an object."""
    return _finish_keep_paint(bm, name, smooth)


# ----------------------------------------------------------------------------- 2D outline helpers
def _area(poly):
    return 0.5 * sum(poly[i][0] * poly[(i + 1) % len(poly)][1] - poly[(i + 1) % len(poly)][0] * poly[i][1]
                     for i in range(len(poly)))


def ccw(poly):
    return poly if _area(poly) > 0 else list(reversed(poly))


def offset_poly(poly, d, clamp=2.5):
    """Inward offset of a CCW polygon by d (miter, clamped)."""
    poly = ccw(poly)
    n = len(poly)
    out = []
    for i in range(n):
        p0, p1, p2 = Vector(poly[i - 1]), Vector(poly[i]), Vector(poly[(i + 1) % n])
        e0 = (p1 - p0).normalized()
        e1 = (p2 - p1).normalized()
        n0 = Vector((-e0.y, e0.x))
        n1 = Vector((-e1.y, e1.x))
        bis = n0 + n1
        if bis.length < 1e-6:
            bis = n0
        bis.normalize()
        cosh = max(bis.dot(n0), 1.0 / clamp)
        q = p1 + bis * (d / cosh)
        out.append((q.x, q.y))
    return out


def is_simple(poly):
    """True when no two non-adjacent edges of the polygon cross."""
    n = len(poly)

    def cross(o, a, b):
        return (a[0] - o[0]) * (b[1] - o[1]) - (a[1] - o[1]) * (b[0] - o[0])

    for i in range(n):
        a, b = poly[i], poly[(i + 1) % n]
        for j in range(i + 2, n):
            if i == 0 and j == n - 1:
                continue
            c, d = poly[j], poly[(j + 1) % n]
            if (cross(a, b, c) * cross(a, b, d) < 0) and (cross(c, d, a) * cross(c, d, b) < 0):
                return False
    return True


def circle(cx, cz, r, n=10, sx=1.0, sz=1.0, rot=0.0):
    return [(cx + math.cos(2 * math.pi * i / n + rot) * r * sx, cz + math.sin(2 * math.pi * i / n + rot) * r * sz)
            for i in range(n)]


def rounded_triangle(cx, cz, w, h, tilt=0.0, round_=0.18, point_down=False):
    """Cute triangle eye: apex up (or down), corners chamfered."""
    if point_down:
        corners = [(-w / 2, h / 2), (w / 2, h / 2), (0, -h / 2)]
    else:
        corners = [(-w / 2, -h / 2), (w / 2, -h / 2), (0, h / 2)]
    corners = ccw(corners)
    out = []
    for i in range(3):
        a, b, c = Vector(corners[i - 1]), Vector(corners[i]), Vector(corners[(i + 1) % 3])
        out.append(tuple(b + (a - b) * round_))
        out.append(tuple(b + (c - b) * round_))
    ct, st = math.cos(math.radians(tilt)), math.sin(math.radians(tilt))
    return [(cx + x * ct - z * st, cz + x * st + z * ct) for x, z in out]


def arc_band(cx, cz, w, h, thick, n=7, up=True, tip=0.4, grow=0.0):
    """Crescent band (closed-eye arc) with pointed-round ends. up=True draws '^' (happy squint),
    False draws a 'u' (sleepy). grow offsets the shape outward (negative shrinks) keeping topology."""
    s = 1 if up else -1
    g = grow
    outer, inner = [], []
    for i in range(1, n - 1):
        t = i / (n - 1)
        x = -w / 2 + w * t
        bump = math.sin(math.pi * t)
        taper = tip + (1 - tip) * math.sin(math.pi * t)
        half = thick * 0.5 * taper + g
        outer.append((cx + x, cz + s * (h * bump + half)))
        inner.append((cx + x, cz + s * (h * bump - half)))
    left = (cx - w / 2 - g, cz)
    right = (cx + w / 2 + g, cz)
    return ccw([left] + outer + [right] + list(reversed(inner)))


def grin(cx, cz, w, top_sag, depth, n=9, teeth=(), tooth_w=0.05, tooth_h=0.04, lift=(0.0, 0.0), grow=0.0,
         tongue=None):
    """Crescent smile. Upper edge sags by top_sag, lower edge by depth, pointed corners.
    teeth: x centres of square teeth hanging from the upper lip (uncut pumpkin).
    lift: (left, right) extra lift of the mouth corners for cheeky asymmetric grins.
    grow offsets the outline outward (negative shrinks) with identical topology."""
    g = grow
    hw = w / 2

    def corner(x):
        t = (x - (cx - hw)) / w
        return lift[0] * (1 - t) ** 3 + lift[1] * t ** 3

    def q_of(x):
        return max(0.0, 1 - ((x - cx) / hw) ** 2)

    def zu(x):
        return cz - top_sag * q_of(x) + corner(x)

    def zl(x):
        q = q_of(x)
        return cz - top_sag * q - (depth - top_sag) * q ** 0.75 + corner(x)

    xs = [cx - hw + w * i / (n - 1) for i in range(1, n - 1)]
    groups = []
    for x in xs:
        # skip lip points near a tooth; the margin exceeds any grow so topology never changes
        if any(abs(x - (cx + tc)) < tooth_w / 2 + 0.025 for tc in teeth):
            continue
        sq = min(1.0, 3 * q_of(x))
        groups.append((x, [(x, zu(x) + g * sq)]))
    for tc in teeth:
        x0 = cx + tc - tooth_w / 2 + g
        x1 = cx + tc + tooth_w / 2 - g
        groups.append((x0, [(x0, zu(x0) + g), (x0, zu(x0) - tooth_h + g), (x1, zu(x1) - tooth_h + g),
                            (x1, zu(x1) + g)]))
    groups.sort(key=lambda it: it[0])
    upper = [p for _, pts in groups for p in pts]
    lower = []
    for x in xs:
        sq = min(1.0, 3 * q_of(x))
        lower.append((cx + (x - cx) * 0.97, zl(x) - g * sq))
    left = (cx - hw - g, zu(cx - hw))
    right = (cx + hw + g, zu(cx + hw))
    return ccw([left] + upper + [right] + list(reversed(lower)))


def shape_of(fn, **kw):
    """Wrap a grow-aware shape function as a carve 'shape' callable."""
    return lambda g: fn(grow=g, **kw)


def convex_shape(poly):
    """Carve 'shape' callable for convex outlines (circles, triangles) using polygon offset."""
    return lambda g: offset_poly(poly, -g) if g else ccw(poly)


# ----------------------------------------------------------------------------- carving
def carve(body, features, depth=0.045, inset=0.012, rim_color="pumpkin_dark", extend=1.6):
    """Cut recessed pockets into body (a closed mesh at identity transform) from the front (-Y).

    features: list of dicts {"outline": [(x, z), ...], "color": "glow_jack", "inset": float?,
    "depth": float?}. Outline coordinates are world x/z, projected along +Y onto the surface.
    Each pocket wall tapers inward from the surface outline to an inset floor, so from the front a
    pumpkin_dark rim frames the glowing floor."""
    bpy.context.view_layer.update()
    dg = bpy.context.evaluated_depsgraph_get()
    tree = BVHTree.FromObject(body, dg)

    def hit(x, z):
        loc, nrm, idx, dist = tree.ray_cast(Vector((x, -5.0, z)), Vector((0, 1, 0)))
        if loc is None:
            raise RuntimeError(f"carve ray missed body at x={x:.3f} z={z:.3f}")
        return loc

    me = bpy.data.meshes.new("_cutter")
    acc = bmesh.new()
    for k, feat in enumerate(features):
        bm = bmesh.new()
        shape = feat["shape"] if "shape" in feat else convex_shape(feat["outline"])
        outline = shape(0.0)
        d = feat.get("depth", depth)
        ins = feat.get("inset", inset)
        # Expand the outline by ~half the inset so the surface opening lands close to the drawn shape
        # (the tapered wall crosses the surface part-way between front ring and floor).
        outer2d = shape(ins * 0.5)
        floor2d = shape(-ins)
        assert len(outer2d) == len(floor2d) == len(outline), "shape topology changed with grow"
        for label, poly in (("outline", outline), ("outer", outer2d), ("floor", floor2d)):
            if not is_simple(poly):
                print(f"WARN carve feature {k}: {label} polygon self-intersects")
        S = [hit(x, z) for x, z in outer2d]
        y_front = min(s.y for s in S) - 0.03
        F = [Vector((x, y_front, z)) for x, z in outer2d]
        B = []
        for (x, z), s in zip(floor2d, S):
            h = hit(x, z)
            B.append(Vector((x, max(h.y, s.y) + d, z)))
        fv = [bm.verts.new(p) for p in F]
        bv = [bm.verts.new(p) for p in B]
        n = len(outline)
        walls = [bm.faces.new((fv[i], fv[(i + 1) % n], bv[(i + 1) % n], bv[i])) for i in range(n)]
        front = bm.faces.new(fv)
        floor = bm.faces.new(list(reversed(bv)))
        K._paint_bm(bm, rim_color, walls + [front])
        K._paint_bm(bm, feat.get("color", "glow_jack"), [floor])
        # concave, non-planar caps confuse the solver: ear-clip them first
        caps = [f for f in bm.faces if len(f.verts) > 4]
        bmesh.ops.triangulate(bm, faces=caps, quad_method="BEAUTY", ngon_method="EAR_CLIP")
        bmesh.ops.recalc_face_normals(bm, faces=bm.faces)
        if bm.calc_volume(signed=True) < 0:
            bmesh.ops.reverse_faces(bm, faces=bm.faces)
        bm.to_mesh(me)
        bm.free()
        acc.from_mesh(me)
    for f in acc.faces:
        f.smooth = False
    acc.to_mesh(me)
    acc.free()
    cutter = K._link(bpy.data.objects.new("_cutter", me))

    mod = body.modifiers.new("carve", "BOOLEAN")
    mod.operation = "DIFFERENCE"
    mod.object = cutter
    mod.solver = "EXACT"
    bpy.context.view_layer.update()
    dg = bpy.context.evaluated_depsgraph_get()
    new_me = bpy.data.meshes.new_from_object(body.evaluated_get(dg))
    if len(new_me.polygons) < len(body.data.polygons):
        print(f"WARN carve: boolean lost geometry ({len(body.data.polygons)} -> {len(new_me.polygons)} faces)")
    body.modifiers.remove(mod)
    old = body.data
    body.data = new_me
    bpy.data.meshes.remove(old)
    bpy.data.objects.remove(cutter, do_unlink=True)
    bpy.data.meshes.remove(me)
    cleanup(body)
    return body


# ----------------------------------------------------------------------------- cleanup / finish
def _tri_area(f):
    return f.calc_area()


def cleanup(ob, min_area=2e-7):
    """Triangulate ngons (beauty) and remove degenerate/sliver triangles so gates stay clean."""
    bm = bmesh.new()
    bm.from_mesh(ob.data)
    bmesh.ops.remove_doubles(bm, verts=bm.verts, dist=1e-5)
    ngons = [f for f in bm.faces if len(f.verts) > 4]
    if ngons:
        bmesh.ops.triangulate(bm, faces=ngons, quad_method="BEAUTY", ngon_method="BEAUTY")
    for _ in range(4):
        bad = [f for f in bm.faces if f.calc_area() < min_area]
        if not bad:
            break
        edges = set()
        for f in bad:
            # collapse the shortest edge of each sliver
            e = min(f.edges, key=lambda e: e.calc_length())
            edges.add(e)
        bmesh.ops.collapse(bm, edges=[e for e in edges if e.is_valid], uvs=False)
        bmesh.ops.dissolve_degenerate(bm, dist=1e-5, edges=bm.edges)
    bm.to_mesh(ob.data)
    bm.free()
    ob.data.update()
    return ob


def degenerate_count(objects):
    bpy.context.view_layer.update()
    n = 0
    for ob in objects:
        for o in [ob] + list(ob.children_recursive):
            if o.type != "MESH":
                continue
            me = o.data
            me.calc_loop_triangles()
            for t in me.loop_triangles:
                a, b, c = (o.matrix_world @ me.vertices[i].co for i in t.vertices)
                if (b - a).cross(c - a).length < 1e-8:
                    n += 1
    return n


def finish(objects, rel_path, sheet_name, close=False):
    tris = K.tri_count(objects)
    if close:
        closeup(objects, sheet_name)
        closeup(objects, sheet_name + "_night", night=True)
    deg = degenerate_count(objects)
    out = K.export(objects, rel_path)
    sheet = K.review_sheet(objects, sheet_name)
    # close-up sheet without the scale post, for detail review (kept in the batch folder)
    real = K.REVIEW
    K.REVIEW = HERE / "_zoom"
    try:
        K.review_sheet(objects, sheet_name, scale_figure=False)
    finally:
        K.REVIEW = real
    print(f"RESULT {sheet_name} tris={tris} degenerate={deg} gltf={out} sheet={sheet}")
    return tris


def closeup(objects, name, views=((0, 8), (-25, 35)), res=512, night=False):
    """Tight orthographic renders (default: straight front, gameplay pitch) into _zoom/<name>_cN.png.
    night=True renders flat colour (no lighting) so glow cells can be judged against the body."""
    from math import cos, radians, sin
    mat = K._preview_material()
    meshes = [o for ob in objects for o in [ob] + list(ob.children_recursive) if o.type == "MESH"]
    for o in meshes:
        if not o.data.materials:
            o.data.materials.append(mat)
    bpy.context.view_layer.update()
    lo = Vector((1e9, 1e9, 1e9))
    hi = Vector((-1e9, -1e9, -1e9))
    for o in meshes:
        for c in o.bound_box:
            w = o.matrix_world @ Vector(c)
            lo = Vector((min(lo.x, w.x), min(lo.y, w.y), min(lo.z, w.z)))
            hi = Vector((max(hi.x, w.x), max(hi.y, w.y), max(hi.z, w.z)))
    center = (lo + hi) / 2
    radius = (hi - lo).length / 2
    scene = bpy.context.scene
    scene.render.engine = "BLENDER_WORKBENCH"
    sh = scene.display.shading
    sh.light = "FLAT" if night else "STUDIO"
    sh.color_type = "TEXTURE"
    sh.show_shadows = not night
    sh.show_cavity = not night
    scene.render.resolution_x = res
    scene.render.resolution_y = res
    if not scene.world:
        scene.world = bpy.data.worlds.new("w")
    scene.world.color = (0.16, 0.19, 0.32)
    cd = bpy.data.cameras.new("_cc")
    cd.type = "ORTHO"
    cd.ortho_scale = radius * 2.1
    cam = K._link(bpy.data.objects.new("_cc", cd))
    scene.camera = cam
    out_dir = HERE / "_zoom"
    out_dir.mkdir(exist_ok=True)
    for i, (yaw, pitch) in enumerate(views):
        d = Vector((sin(radians(yaw)) * cos(radians(pitch)), -cos(radians(yaw)) * cos(radians(pitch)),
                    sin(radians(pitch))))
        cam.location = center + d * radius * 4
        cam.rotation_euler = (-d).to_track_quat("-Z", "Y").to_euler()
        scene.render.filepath = str(out_dir / f"{name}_c{i}.png")
        bpy.ops.render.render(write_still=True)
    bpy.data.objects.remove(cam, do_unlink=True)


def seeded(seed):
    return random.Random(seed)
