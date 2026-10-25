"""Yard furniture (v2): mailbox, bench, rocking_chair, swing_set."""
import pathlib
import sys

sys.path.insert(0, str(pathlib.Path(__file__).resolve().parent))
from yard_common import *  # noqa: E402,F401,F403


def ellipse(ax=1.0, ay=1.0):
    """sweep() shape: elliptical cross-section (ax along the frame normal, ay along the binormal)."""
    def f(i, a):
        return 1.0 / math.sqrt((math.cos(a) / ax) ** 2 + (math.sin(a) / ay) ** 2)
    return f


def d_outline(w, h_rect, n_arc=8, corner=0.025):
    """Mailbox D-profile (x, up), CCW: flat bottom with rounded corners, straight sides, round top."""
    r = w / 2
    pts = []
    for k in range(3):  # bottom-right corner
        a = -math.pi / 2 + (math.pi / 2) * k / 2
        pts.append((r - corner + math.cos(a) * corner, corner + math.sin(a) * corner))
    for i in range(n_arc + 1):
        a = math.pi * i / n_arc
        pts.append((math.cos(a) * r, h_rect + math.sin(a) * r))
    for k in range(3):  # bottom-left corner
        a = math.pi + (math.pi / 2) * k / 2
        pts.append((-r + corner + math.cos(a) * corner, corner + math.sin(a) * corner))
    return pts


def mailbox():
    K.reset()
    parts = []
    parts.append(box("post", (0.12, 0.12, 1.0), "wood", "wood", loc=(0, 0.04, 0.5), bevel=0.018, segments=2))
    parts.append(box("cap_board", (0.22, 0.5, 0.045), "wood_dark", "wood", loc=(0, 0.0, 0.995), bevel=0.012, segments=1))
    # diagonal brace under the board
    parts.append(rod("brace", (0, 0.1, 0.72), (0, 0.2, 0.97), 0.025, "wood_dark", mat="wood", sides=6))
    W, H, Z = 0.3, 0.13, 1.018
    outline = d_outline(W, H)
    secs = []
    for y, k in [(-0.262, 0.97), (-0.258, 1.035), (-0.232, 1.035), (-0.228, 1.0), (0.235, 1.0), (0.245, 0.96)]:
        secs.append((y, [(x * k, Z + (z - 0.0) * k - (k - 1.0) * 0.004) for x, z in outline]))
    body = loft("box", secs, "mailbox_black", mat="metal")
    parts.append(body)
    # door seam band reads lighter, handle on top of the door, little hinge at the bottom
    paint_world(body, "iron", lambda c, n: -0.259 < c.y < -0.231)
    parts.append(blob("handle", 0.022, "silver", mat="metal", loc=(0, -0.272, Z + H + 0.1), scale=(1.4, 0.8, 0.8),
                      segments=8, rings=5, noise_amp=0.0, axis="X"))
    parts.append(rod("hinge", (-0.08, -0.262, Z + 0.012), (0.08, -0.262, Z + 0.012), 0.012, "iron", sides=6))
    # red flag, raised, on the right side with a pivot bolt
    fx = W / 2 + 0.018
    parts.append(box("flag_arm", (0.016, 0.03, 0.27), "metal_red", "metal", loc=(fx, 0.09, Z + 0.17), rot=(-6, 0, 0),
                     bevel=0.006, segments=1))
    parts.append(box("flag", (0.018, 0.13, 0.085), "metal_red", "metal", loc=(fx, 0.04, Z + 0.28), rot=(-6, 0, 0),
                     bevel=0.008, segments=1))
    parts.append(rod("pivot", (W / 2 - 0.005, 0.1, Z + 0.06), (W / 2 + 0.035, 0.1, Z + 0.06), 0.022, "silver", sides=8))
    # house number plate on the side
    parts.append(box("plate", (0.006, 0.12, 0.06), "cream", "flat", loc=(-W / 2 - 0.004, -0.06, Z + 0.075),
                     bevel=0.003, segments=1))
    ob = K.merge(parts, "mailbox")
    ob.rotation_euler.y = math.radians(1.5)
    bake(ob)
    ground(ob)
    return finish([ob], "props/mailbox", "mailbox", ao_radius=0.3)


def slat(name, length, width, thick, color, loc, rot=(0, 0, 0), mat="wood"):
    return box(name, (length, width, thick), color, mat, loc=loc, rot=rot, bevel=min(width, thick) * 0.3, segments=1)


def bench():
    K.reset()
    parts = []
    I = "iron"
    for i, y in enumerate((-0.19, -0.065, 0.06, 0.185)):
        parts.append(slat(f"seat{i}", 1.56, 0.105, 0.04, "wood_light" if i % 2 == 0 else "plank", (0, y, 0.45)))
    for i, z in enumerate((0.6, 0.73, 0.86)):
        y = 0.27 + (z - 0.6) * 0.3
        parts.append(slat(f"back{i}", 1.56, 0.1, 0.035, "plank" if i % 2 == 0 else "wood_light", (0, y, z), rot=(-73, 0, 0)))
    for sx in (-0.66, 0.66):
        # front leg rising into the armrest, ending in a curl
        front = catmull([(sx, -0.22, 0.0), (sx, -0.235, 0.22), (sx, -0.22, 0.43), (sx, -0.2, 0.6), (sx, -0.08, 0.67),
                         (sx, 0.12, 0.67), (sx, 0.3, 0.7)], per=1)
        front.insert(4, tuple((Vector(front[3]) + Vector(front[4])) / 2 + Vector((0, -0.012, 0.012))))
        parts.append(sweep(f"front_{sx}", front, 0.03, color=I, mat="metal", sides=6))
        curl = arc((sx, -0.27, 0.63), 0.055, 60, 330, 6, plane="YZ", radius_end=0.03)
        parts.append(sweep(f"curl_{sx}", [(sx, -0.2, 0.6)] + curl, [0.027] + [0.026 - 0.01 * k / 7 for k in range(7)],
                           color=I, mat="metal", sides=6))
        back = catmull([(sx, 0.24, 0.0), (sx, 0.22, 0.25), (sx, 0.24, 0.45), (sx, 0.31, 0.72), (sx, 0.36, 0.95)], per=2)
        parts.append(sweep(f"back_{sx}", back, 0.032, color=I, mat="metal", sides=6))
        parts.append(rod(f"rail_{sx}", (sx, -0.23, 0.42), (sx, 0.24, 0.42), 0.026, I, sides=6))
        parts.append(rod(f"low_{sx}", (sx, -0.23, 0.12), (sx, 0.23, 0.12), 0.02, I, sides=5))
        for fy in (-0.22, 0.24):
            parts.append(blob(f"foot_{sx}{fy}", 0.045, I, mat="metal", loc=(sx, fy, 0.012), scale=(1, 1, 0.45),
                              segments=6, rings=4, noise_amp=0.0, axis="Z"))
    ob = K.merge(parts, "bench")
    ground(ob)
    return finish([ob], "props/bench", "bench", ao_radius=0.35)


def rocking_chair():
    K.reset()
    parts = []
    W = 0.26
    WD, WM, WL = "wood_dark", "wood", "wood_light"
    for sx in (-W, W):
        rock = arc((sx, 0.05, 1.3), 1.3, 250, 292, 8, plane="YZ")
        parts.append(sweep(f"rocker_{sx}", rock, 0.04, color=WD, mat="wood", sides=6, shape=ellipse(1.0, 0.55),
                           up_hint=(0, 0, 1)))
        parts.append(rod(f"leg_f{sx}", (sx, -0.2, 0.03), (sx, -0.2, 0.66), 0.032, WM, mat="wood", sides=6))
        parts.append(rod(f"leg_b{sx}", (sx, 0.22, 0.03), (sx, 0.32, 1.06), 0.034, WM, mat="wood", sides=7))
        parts.append(box(f"arm_{sx}", (0.075, 0.52, 0.035), WL, "wood", loc=(sx * 1.05, 0.03, 0.675), bevel=0.012,
                         segments=1))
        parts.append(blob(f"knob_{sx}", 0.042, WL, mat="wood", loc=(sx, 0.322, 1.085), segments=7, rings=5,
                          noise_amp=0.0, axis="Z"))
        parts.append(rod(f"stretch_{sx}", (sx, -0.2, 0.2), (sx, 0.24, 0.2), 0.018, WM, mat="wood", sides=5))
    parts.append(box("seat", (0.6, 0.5, 0.055), WM, "wood", loc=(0, 0.01, 0.43), bevel=0.018, segments=1))
    top = [(-W, 0.315, 0.98), (-0.12, 0.33, 1.0), (0.0, 0.335, 1.01), (0.12, 0.33, 1.0), (W, 0.315, 0.98)]
    parts.append(sweep("top_rail", catmull(top, per=2), 0.042, color=WL, mat="wood", sides=6,
                       shape=ellipse(1.0, 0.6), up_hint=(0, 0, 1)))
    parts.append(rod("mid_rail", (-W, 0.27, 0.6), (W, 0.27, 0.6), 0.024, WM, mat="wood", sides=6))
    for i, x in enumerate((-0.15, -0.05, 0.05, 0.15)):
        parts.append(rod(f"spindle{i}", (x, 0.27, 0.6), (x, 0.315, 0.98), 0.02, WM, mat="wood", sides=5))
    # folded quilt on the seat, one end flopping over the front edge
    Q1, Q2 = "quilt_blue", "quilt_yellow"
    parts.append(box("quilt_a", (0.44, 0.36, 0.055), Q1, "fabric", loc=(0.02, -0.0, 0.485), rot=(0, 0, 6),
                     bevel=0.022, segments=2))
    parts.append(box("quilt_b", (0.4, 0.31, 0.05), Q2, "fabric", loc=(0.03, 0.01, 0.537), rot=(0, 0, 9), bevel=0.02,
                     segments=2))
    flap = sweep("quilt_flap", [(0.02, -0.17, 0.5), (0.02, -0.25, 0.48), (0.02, -0.285, 0.4), (0.03, -0.28, 0.3)],
                 0.17, color=Q1, mat="fabric", sides=8, shape=ellipse(0.13, 1.0), up_hint=(0, 1, 0))
    parts.append(flap)
    parts.append(box("quilt_top", (0.14, 0.12, 0.03), "cheek_pink", "fabric", loc=(0.06, 0.03, 0.565), rot=(0, 0, 14),
                     bevel=0.012, segments=2))
    ob = K.merge(parts, "rocking_chair")
    ground(ob)
    return finish([ob], "props/rocking_chair", "rocking_chair", ao_radius=0.35)


def chain(name, a, b, links=6, r=0.012):
    a, b = Vector(a), Vector(b)
    d = b - a
    L = d.length
    parts = []
    step = L / links
    for k in range(links):
        c = a + d * ((k + 0.5) / links)
        t = K.torus(f"{name}{k}", step * 0.42, r * 0.55, color="silver", mat="metal", major_seg=6, minor_seg=3,
                    loc=tuple(c), rot=(0, 90 if k % 2 else 0, 0) if False else (90, 0, 90 * (k % 2)),
                    scale=(0.55, 1.0, 1.0))
        parts.append(t)
    return parts


def swing_set():
    K.reset()
    parts = []
    H, X = 2.12, 1.25
    MB, MR = "metal_blue", "metal_red"
    for sx in (-X, X):
        for sy in (-0.8, 0.8):
            parts.append(rod(f"leg{sx}{sy}", (sx * 1.05, sy, 0.03), (sx, 0, H), 0.075, MB, sides=10))
            parts.append(lathe(f"foot{sx}{sy}", [(0.12, 0.0), (0.12, 0.03), (0.09, 0.07), (0.0, 0.075)], MB, "metal",
                               segments=10, cap_bottom=False, loc=(sx * 1.05, sy, 0.0)))
        parts.append(rod(f"brace{sx}", (sx * 1.035, -0.55, 0.72), (sx * 1.035, 0.55, 0.72), 0.05, MB, sides=8))
        parts.append(box(f"bracket{sx}", (0.2, 0.3, 0.24), MB, "metal", loc=(sx, 0, H - 0.04), bevel=0.05, segments=1))
    parts.append(rod("top_bar", (-X - 0.1, 0, H), (X + 0.1, 0, H), 0.085, MB, sides=12))
    for sx in (-X - 0.14, X + 0.14):
        parts.append(blob(f"cap{sx}", 0.12, MR, mat="metal", loc=(sx, 0, H), segments=9, rings=6, noise_amp=0.0,
                          axis="X"))
    seat_z = 0.48
    for i, (cx, tilt) in enumerate(((-0.58, 0.0), (0.58, 11.0))):
        sw = []
        for side in (-0.21, 0.21):
            sw.append(K.torus(f"hanger{i}{side}", 0.06, 0.02, color="silver", mat="metal", major_seg=8, minor_seg=4,
                              loc=(cx + side, 0, H - 0.04), rot=(0, 90, 0)))
            sw += chain(f"chain{i}{side}", (cx + side, 0, H - 0.1), (cx + side, 0, seat_z + 0.06), links=6)
        # sling seat: curved strap
        strap = [(cx - 0.27, 0, seat_z + 0.07), (cx - 0.15, 0, seat_z + 0.01), (cx, 0, seat_z - 0.01),
                 (cx + 0.15, 0, seat_z + 0.01), (cx + 0.27, 0, seat_z + 0.07)]
        sw.append(sweep(f"seat{i}", catmull(strap, per=2), 0.13, color=MR, mat="metal", sides=8,
                        shape=ellipse(0.22, 1.0), up_hint=(0, 0, 1)))
        s = K.merge(sw, f"swing{i}")
        if tilt:
            pivot = Vector((cx, 0, H))
            m = Matrix.Translation(pivot) @ Matrix.Rotation(math.radians(tilt), 4, "X") @ Matrix.Translation(-pivot)
            s.data.transform(m)
        parts.append(s)
    ob = K.merge(parts, "swing_set")
    ground(ob)
    return finish([ob], "props/swing_set", "swing_set", ao_radius=0.5)


if want("mailbox"):
    mailbox()
if want("bench"):
    bench()
if want("rocking_chair"):
    rocking_chair()
if want("swing_set"):
    swing_set()
