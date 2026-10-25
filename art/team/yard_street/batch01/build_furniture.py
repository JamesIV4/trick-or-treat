"""Yard furniture: mailbox, bench, rocking_chair, swing_set."""
import math
import pathlib
import sys

sys.path.insert(0, str(pathlib.Path(__file__).resolve().parent))
from yard_common import K, arc, finish, paint_where_world, want  # noqa: E402


def rod(name, a, b, radius, color, verts=6, caps=True):
    """Cylinder from point a to point b."""
    from mathutils import Vector
    a, b = Vector(a), Vector(b)
    d = b - a
    ob = K.cylinder(name, radius, d.length, color=color, verts=verts, loc=tuple((a + b) / 2), caps=caps)
    ob.rotation_euler = d.to_track_quat("Z", "Y").to_euler()
    return ob


def slat(name, length, width, thick, color, loc, rot=(0, 0, 0)):
    """Rounded plank: a flattened hexagonal prism along X (20 tris, reads as a soft-edged slat)."""
    from mathutils import Matrix
    ob = K.cylinder(name, 0.5, length, color=color, verts=6, loc=loc, scale=(thick, width, 1.0))
    m = Matrix.Rotation(math.radians(rot[0]), 3, "X") @ Matrix.Rotation(math.radians(90), 3, "Y")
    ob.rotation_euler = m.to_euler()
    return ob


def d_outline(w, h_rect, n_arc=6):
    """Mailbox D-profile in (x, up): flat bottom, straight sides, half-round top. CCW."""
    r = w / 2
    pts = [(-r, 0.0), (r, 0.0)]
    for i in range(n_arc + 1):
        a = math.pi * i / n_arc
        pts.append((math.cos(a) * r, h_rect + math.sin(a) * r))
    return pts


def mailbox():
    K.reset()
    parts = []
    parts.append(K.box("post", (0.14, 0.14, 0.98), color="wood", loc=(0, 0.02, 0.49), bevel=0.018))
    parts.append(K.box("shelf", (0.24, 0.46, 0.05), color="wood_dark", loc=(0, 0.0, 0.955), bevel=0.01))
    body_len = 0.52
    # polygon_prism extrudes +Z; rot X +90 maps outline-y to world Z and extrusion to world -Y
    parts.append(K.polygon_prism("box", d_outline(0.33, 0.13), body_len, color="mailbox_black",
                                 loc=(0, body_len / 2, 0.98), rot=(90, 0, 0)))
    parts.append(K.polygon_prism("door", d_outline(0.355, 0.13), 0.03, color="mailbox_black",
                                 loc=(0, -body_len / 2 + 0.01, 0.97), rot=(90, 0, 0)))
    paint_where_world(parts[-1], "iron", lambda c, n: n.y < -0.9)
    parts.append(K.sphere("handle", 0.025, color="silver", loc=(0, -body_len / 2 - 0.03, 1.22), segments=6, rings=4))
    # red flag, raised, on the right side
    parts.append(K.box("flag_arm", (0.02, 0.03, 0.3), color="metal_red", loc=(0.185, 0.08, 1.15), rot=(-8, 0, 0)))
    parts.append(K.box("flag", (0.022, 0.14, 0.085), color="metal_red", loc=(0.185, 0.02, 1.27), rot=(-8, 0, 0)))
    parts.append(K.cylinder("pivot", 0.025, 0.02, color="silver", verts=6, loc=(0.178, 0.1, 1.04), rot=(0, 90, 0)))
    ob = K.merge(parts, "mailbox")
    ob.rotation_euler.y = math.radians(1.5)  # a little lean for personality
    from yard_common import bake
    bake(ob)
    # lean lifted one foot corner a hair: snap back so the lowest point is on the ground
    lo = min(v.co.z for v in ob.data.vertices)
    for v in ob.data.vertices:
        v.co.z -= lo
    return finish([ob], "props/mailbox", "mailbox")


def bench():
    K.reset()
    parts = []
    for i, y in enumerate((-0.17, -0.03, 0.11)):
        parts.append(slat(f"seat{i}", 1.5, 0.12, 0.045, "wood_light" if i != 1 else "plank", (0, y, 0.45)))
    for i, z in enumerate((0.62, 0.78)):
        parts.append(slat(f"back{i}", 1.5, 0.12, 0.045, "plank" if i == 0 else "wood_light",
                          (0, 0.255 + (z - 0.62) * 0.27, z), rot=(75, 0, 0)))
    for sx in (-0.62, 0.62):
        # front leg rising into the armrest with a curl at the front
        front = [(sx, -0.2, 0.0), (sx, -0.21, 0.3), (sx, -0.2, 0.55), (sx, -0.12, 0.64), (sx, 0.05, 0.66),
                 (sx, 0.2, 0.66)]
        parts.append(K.tube(f"front_{sx}", front, radius=0.032, color="iron", sides=4, smooth=False))
        curl = arc((sx, -0.27, 0.6), 0.06, 30, 300, 4)
        parts.append(K.tube(f"curl_{sx}", [(sx, -0.2, 0.62)] + curl, radius=0.026, color="iron", sides=4, smooth=False))
        back = [(sx, 0.22, 0.0), (sx, 0.2, 0.42), (sx, 0.26, 0.65), (sx, 0.31, 0.88)]
        parts.append(K.tube(f"back_{sx}", back, radius=0.034, color="iron", sides=4, smooth=False))
        parts.append(rod(f"rail_{sx}", (sx, -0.2, 0.42), (sx, 0.21, 0.42), 0.03, "iron", verts=4))
    ob = K.merge(parts, "bench")
    return finish([ob], "props/bench", "bench")


def rocking_chair():
    K.reset()
    parts = []
    W = 0.25  # half width
    for sx in (-W, W):
        rock = arc((sx, 0.05, 1.3), 1.3, 252, 290, 6, plane="YZ")
        parts.append(K.tube(f"rocker_{sx}", rock, radius=0.035, color="wood_dark", sides=4, smooth=False))
        parts.append(rod(f"leg_f{sx}", (sx, -0.2, 0.02), (sx, -0.2, 0.64), 0.03, "wood", verts=6))
        parts.append(rod(f"leg_b{sx}", (sx, 0.22, 0.03), (sx, 0.31, 1.05), 0.032, "wood", verts=6))
        parts.append(K.box(f"arm_{sx}", (0.07, 0.5, 0.035), color="wood_light", loc=(sx * 1.04, 0.03, 0.66)))
        parts.append(K.sphere(f"knob_{sx}", 0.04, color="wood_light", loc=(sx, 0.31, 1.07), segments=5, rings=3))
    parts.append(K.box("seat", (0.56, 0.48, 0.06), color="wood", loc=(0, 0.01, 0.42)))
    parts.append(rod("top_rail", (-W, 0.3, 0.98), (W, 0.3, 0.98), 0.045, "wood_light", verts=6))
    parts.append(rod("mid_rail", (-W, 0.26, 0.6), (W, 0.26, 0.6), 0.025, "wood", verts=5))
    for i, x in enumerate((-0.12, 0.0, 0.12)):
        parts.append(rod(f"spindle{i}", (x, 0.26, 0.6), (x, 0.3, 0.97), 0.022, "wood", verts=5))
    # folded quilt on the seat, one corner flopping over the front edge
    parts.append(K.box("quilt_a", (0.42, 0.34, 0.06), color="quilt_blue", loc=(0.02, -0.01, 0.48), rot=(0, 0, 6), bevel=0.02))
    parts.append(K.box("quilt_b", (0.38, 0.3, 0.05), color="quilt_yellow", loc=(0.04, 0.0, 0.535), rot=(0, 0, 9), bevel=0.018))
    parts.append(K.box("quilt_flap", (0.32, 0.04, 0.17), color="quilt_blue", loc=(-0.01, -0.255, 0.42), rot=(-10, 0, 5)))
    ob = K.merge(parts, "rocking_chair")
    lo = min(v.co.z for v in ob.data.vertices)
    for v in ob.data.vertices:
        v.co.z -= lo
    return finish([ob], "props/rocking_chair", "rocking_chair")


def swing_set():
    K.reset()
    parts = []
    H, X = 2.12, 1.25
    for sx in (-X, X):
        for sy in (-0.75, 0.75):
            parts.append(rod(f"leg{sx}{sy}", (sx * 1.04, sy, 0.0), (sx, 0, H), 0.085, "metal_blue", verts=8))
            parts.append(K.cylinder(f"foot{sx}{sy}", 0.12, 0.05, color="metal_blue", verts=8, loc=(sx * 1.04, sy, 0.02)))
        parts.append(rod(f"brace{sx}", (sx * 1.03, -0.5, 0.7), (sx * 1.03, 0.5, 0.7), 0.055, "metal_blue", verts=6))
    parts.append(rod("top_bar", (-X - 0.08, 0, H), (X + 0.08, 0, H), 0.095, "metal_blue", verts=8))
    for sx in (-X - 0.12, X + 0.12):
        parts.append(K.sphere(f"cap{sx}", 0.13, color="metal_red", loc=(sx, 0, H), segments=8, rings=5))
    # two swings, the right one caught mid-swing
    for i, (cx, tilt) in enumerate(((-0.55, 0.0), (0.55, 12.0))):
        swing = []
        seat_z = 0.48
        for side in (-0.2, 0.2):
            swing.append(K.box(f"clamp{i}{side}", (0.09, 0.22, 0.08), color="iron", loc=(cx + side, 0, H - 0.08)))
            swing.append(rod(f"chain{i}{side}", (cx + side, 0, H - 0.1), (cx + side, 0, seat_z + 0.03), 0.028,
                             "silver", verts=4, caps=False))
        swing.append(K.box(f"seat{i}", (0.52, 0.25, 0.06), color="metal_red", loc=(cx, 0, seat_z), bevel=0.025, segments=2))
        s = K.merge(swing, f"swing{i}")
        if tilt:
            import mathutils
            pivot = mathutils.Vector((cx, 0, H))
            rot = mathutils.Matrix.Translation(pivot) @ mathutils.Matrix.Rotation(math.radians(tilt), 4, "X") @ \
                mathutils.Matrix.Translation(-pivot)
            s.data.transform(rot)
        parts.append(s)
    ob = K.merge(parts, "swing_set")
    return finish([ob], "props/swing_set", "swing_set")


if want("mailbox"):
    mailbox()
if want("bench"):
    bench()
if want("rocking_chair"):
    rocking_chair()
if want("swing_set"):
    swing_set()
