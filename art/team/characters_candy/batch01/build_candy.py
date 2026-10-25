"""Candy family: 4 twist-wrapped candies, chocolate bar, swirl lollipop, golden prize bar.
All float in game, so the origin is at the centre. Long axis runs along X; the most readable face
(lollipop swirl, bar labels) faces front (-Y) / up. Run with Blender 5.1 headless."""
import math
import sys
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parent))
import bmesh  # noqa: E402
from mathutils import Vector  # noqa: E402

import cc_helpers as H  # noqa: E402
from cc_helpers import K  # noqa: E402

ONLY = set(sys.argv[sys.argv.index("--") + 1:]) if "--" in sys.argv else set()


def finish(ob, asset_id, rel):
    print("TRIS", asset_id, K.tri_count([ob]))
    K.export([ob], rel)
    K.review_sheet([ob], asset_id)
    H.close_sheet([ob], asset_id, views=((0, 25), (-35, 35), (150, 25), (90, 5)), tiny=False)


# ------------------------------------------------------------------ twist-wrapped candy
def wrapped(asset_id, color, stripe="lolly_white"):
    K.reset()
    R = 0.092
    # custom lathe along X whose rings are progressively twisted: the face columns themselves
    # spiral, so colouring whole columns gives two clean candy-cane stripes
    SEG = 8
    stations = [(-0.1, 0.024, -1.05), (-0.078, 0.064, -0.65), (-0.03, 0.092, -0.22), (0.03, 0.092, 0.22),
                (0.078, 0.064, 0.65), (0.1, 0.024, 1.05)]   # (x, radius, twist radians)
    stations = [(x, r, tw + math.pi * 0.6) for x, r, tw in stations]   # put a stripe across the top-front
    bm = bmesh.new()
    rings = [[bm.verts.new((x, math.cos(2 * math.pi * s / SEG + tw) * r, math.sin(2 * math.pi * s / SEG + tw) * r * 0.92))
              for s in range(SEG)] for x, r, tw in stations]
    stripe_faces = []
    for k in range(len(rings) - 1):
        for s in range(SEG):
            f = bm.faces.new((rings[k][s], rings[k][(s + 1) % SEG], rings[k + 1][(s + 1) % SEG], rings[k + 1][s]))
            if s % 4 == 0:
                stripe_faces.append(f)
    bmesh.ops.recalc_face_normals(bm, faces=bm.faces)
    K._paint_bm(bm, stripe, stripe_faces)
    uvl = bm.loops.layers.uv.verify()
    u, v = K._uv_cell(color)
    for f in bm.faces:
        if f not in stripe_faces:
            for loop in f.loops:
                loop[uvl].uv = (u, v)
        f.smooth = True
    me = K.bpy.data.meshes.new("body")
    bm.to_mesh(me)
    bm.free()
    body = K._link(K.bpy.data.objects.new("body", me))
    parts = [body]
    for sx in (1, -1):
        # pinched neck + fanned, twisted wrapper end
        end = K.cylinder(f"end{sx}", 0.024, 0.085, color=color, verts=6, radius_top=0.072, rot=(0, 90 * sx, 0),
                         loc=(sx * (R * 1.12 + 0.018), 0, 0))

        def fan(c, sx=sx):
            # local z runs outward along the end; flatten the far rim into a fan and twist it
            t = (c.z + 0.0425) / 0.085
            k = 1.0 - 0.62 * t
            v = Vector((c.x, c.y * k, c.z))
            a = math.radians(28 * t * sx)
            return Vector((v.x * math.cos(a) - v.y * math.sin(a), v.x * math.sin(a) + v.y * math.cos(a), v.z))

        K.deform(end, fan)
        parts.append(end)
    ob = K.merge(parts, asset_id)
    finish(ob, asset_id, f"candy/{asset_id}")


# ------------------------------------------------------------------ chocolate bar
def chocolate_bar():
    K.reset()
    L, W, Th = 0.42, 0.19, 0.06
    x0 = -L / 2
    sleeve_end = 0.03
    sleeve = K.box("sleeve", (sleeve_end - x0, W + 0.02, Th + 0.02), color="wrapper_red",
                   loc=((x0 + sleeve_end) / 2, 0, 0), bevel=0.012)
    foil = K.box("foil", (0.03, W + 0.03, Th + 0.026), color="foil_silver", loc=(sleeve_end + 0.012, 0, 0.001),
                 rot=(4, 0, 0))
    choco_base = K.box("choco_base", (L / 2 - sleeve_end + 0.02, W, Th * 0.55),
                       color="choco_dark", loc=((sleeve_end + L / 2) / 2, 0, -Th * 0.12))
    squares = []
    sx0 = sleeve_end + 0.05
    for i in range(2):
        for j in range(2):
            squares.append(K.cylinder(f"sq{i}{j}", 0.058, 0.04, color="choco", verts=4, radius_top=0.044,
                                      rot=(0, 0, 45),
                                      loc=(sx0 + i * 0.087, (j - 0.5) * 0.09, Th * 0.12)))
    ob = K.merge([sleeve, foil, choco_base] + squares, "candy_chocolate_bar")
    ob.rotation_euler = (0, 0, 0)
    finish(ob, "candy_chocolate_bar", "candy/candy_chocolate_bar")


# ------------------------------------------------------------------ swirl lollipop
def lollipop():
    K.reset()
    R = 0.25
    seg = 12
    # (radius, half thickness, twist): rings are rotated progressively so the face columns
    # themselves spiral; colouring whole columns then gives a real swirl
    rings = [(0.0, 0.058, 0.0), (0.1, 0.053, 1.25), (0.19, 0.036, 0.6), (R, 0.0, 0.0)]
    cols = ["candy_cherry", "lolly_white", "candy_lemon"]
    bm = bmesh.new()
    eq = [bm.verts.new((math.cos(2 * math.pi * s / seg) * R, 0, math.sin(2 * math.pi * s / seg) * R))
          for s in range(seg)]
    uv = bm.loops.layers.uv.verify()
    for side in (-1, 1):   # -1 = front (-Y)
        ring_v = []
        for r, h, tw in rings[1:-1]:
            ring_v.append([bm.verts.new((math.cos(2 * math.pi * s / seg + tw) * r, side * h,
                                         math.sin(2 * math.pi * s / seg + tw) * r)) for s in range(seg)])
        ring_v.append(eq)
        centre = bm.verts.new((0, side * rings[0][1], 0))
        faces = []
        for s in range(seg):
            s2 = (s + 1) % seg
            f = bm.faces.new((centre, ring_v[0][s], ring_v[0][s2]))
            faces.append((f, 0, s))
            for k in range(len(ring_v) - 1):
                f = bm.faces.new((ring_v[k][s], ring_v[k + 1][s], ring_v[k + 1][s2], ring_v[k][s2]))
                faces.append((f, k + 1, s))
        for f, k, s in faces:
            idx = (s // 2) % 3   # six spiral arms, two of each colour
            u, v = K._uv_cell(cols[idx])
            for loop in f.loops:
                loop[uv].uv = (u, v)
    bmesh.ops.recalc_face_normals(bm, faces=bm.faces)
    for f in bm.faces:
        f.smooth = False
    me = K.bpy.data.meshes.new("disc")
    bm.to_mesh(me)
    bm.free()
    disc = K._link(K.bpy.data.objects.new("disc", me))
    stick = K.cylinder("stick", 0.026, 0.46, color="lolly_white", verts=6, loc=(0, 0, -0.27), smooth=True)
    ob = K.merge([disc, stick], "candy_lollipop")
    finish(ob, "candy_lollipop", "candy/candy_lollipop")


# ------------------------------------------------------------------ golden prize bar
def golden_bar():
    K.reset()
    L, W, Th = 0.5, 0.25, 0.125
    body = K.box("body", (L, W, Th), color="foil_gold", bevel=0.018)
    # wrapper pinches toward the crimped ends (pillow-pack look instead of a brick)
    K.deform(body, lambda c: Vector((c.x, c.y, c.z * (0.55 if abs(c.x) > L / 2 - 0.02 else 1.0))))
    band = K.box("band", (0.17, W + 0.016, Th + 0.016), color="glow_candy_gold", bevel=0.012)
    crimps = []
    for s in (1, -1):
        cr = K.box(f"crimp{s}", (0.05, W * 0.97, 0.038), color="foil_gold", loc=(s * (L / 2 + 0.018), 0, 0))
        # fan the crimp slightly wider at its outer edge
        K.deform(cr, lambda c, s=s: Vector((c.x, c.y * (1.0 + 0.08 * (c.x * s > 0)), c.z * (0.7 if c.x * s > 0 else 1.0))))
        crimps.append(cr)
    # star medallion on top of the glowing band
    star = []
    for i in range(10):
        a = math.pi / 2 + i * math.pi / 5
        r = 0.058 if i % 2 == 0 else 0.026
        star.append((math.cos(a) * r, math.sin(a) * r))
    st = K.polygon_prism("star", star, 0.016, color="foil_gold", loc=(0, 0, Th / 2 + 0.004))
    ob = K.merge([body, band, st] + crimps, "candy_golden_bar")
    finish(ob, "candy_golden_bar", "candy/candy_golden_bar")


JOBS = {
    "candy_wrapped_pink": lambda: wrapped("candy_wrapped_pink", "candy_pink"),
    "candy_wrapped_mint": lambda: wrapped("candy_wrapped_mint", "candy_mint"),
    "candy_wrapped_grape": lambda: wrapped("candy_wrapped_grape", "candy_grape"),
    "candy_wrapped_lemon": lambda: wrapped("candy_wrapped_lemon", "candy_lemon"),
    "candy_chocolate_bar": chocolate_bar,
    "candy_lollipop": lollipop,
    "candy_golden_bar": golden_bar,
}
for k, job in JOBS.items():
    if not ONLY or k in ONLY:
        job()
