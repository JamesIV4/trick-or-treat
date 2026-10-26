"""shop_cart: storybook candy cart. Blender coords: Z up, front faces -Y (Godot +Z), origin at the
ground centre under the cart body. Round doormat centred at Blender (0, -1.25, 0) = Godot (0, 0, 1.25).

    blender -b --factory-startup --python build_cart.py -- [--stats]
"""
import pathlib
import random
import sys

sys.path.insert(0, str(pathlib.Path(__file__).resolve().parent))
from lm_common import *  # noqa: E402,F401,F403
from lm_common import ARGS, ellipse, stroke, rock, ribbed  # noqa: E402

PINK, WHITE = "candy_pink", "lolly_white"
BODY = "house_mint"
TRIM = "cream"
WD = "wood_dark"
COUNTER_Z = 1.19
MAT_Y = -1.25
LANTERN = (-0.62, -0.7, 1.98)   # glass centre (Godot (-0.62, 1.98, 0.70))

# awning profile (y, z) from back to front, and width
AW_PROF = [(0.62, 2.43), (0.38, 2.56), (0.1, 2.61), (-0.2, 2.56), (-0.5, 2.43), (-0.82, 2.21)]
AW_X = 1.17
AW_N = 8
AW_T = 0.035


def aw_z(y):
    """Awning top height at y (linear between profile points)."""
    P = AW_PROF
    for (y0, z0), (y1, z1) in zip(P, P[1:]):
        if y1 <= y <= y0:
            t = (y - y0) / (y1 - y0)
            return z0 + (z1 - z0) * t
    return P[0][1] if y > P[0][0] else P[-1][1]


# ------------------------------------------------------------------ wheels & chassis
def wheel(sx):
    parts = []
    x, cz, R = sx * 1.0, 0.42, 0.42
    tire = K.torus(f"tire{sx}", R - 0.03, 0.035, color="iron", mat="metal", major_seg=18, minor_seg=4)
    transform(tire, loc=(x, 0, cz), rot=(0, 90, 0), scale=(1, 1, 1.3))
    parts.append(tire)
    fel = K.torus(f"felloe{sx}", R - 0.085, 0.035, color=WD, mat="wood", major_seg=16, minor_seg=4)
    transform(fel, loc=(x, 0, cz), rot=(0, 90, 0), scale=(1, 1, 1.4))
    parts.append(fel)
    for k in range(8):
        a = math.radians(k * 45 + 22.5)
        d = Vector((0, math.cos(a), math.sin(a)))
        parts.append(rod(f"spoke{sx}{k}", tuple(Vector((x, 0, cz)) + d * 0.07), tuple(Vector((x, 0, cz)) + d * (R - 0.1)),
                         0.02, "wood", mat="wood", sides=5, chamfer=0))
    hub = lathe(f"hub{sx}", [(0.09, -0.07), (0.1, -0.04), (0.1, 0.03), (0.07, 0.06), (0.04, 0.075), (0.0, 0.08)],
                ["metal_red", "metal_red", "metal_red", "foil_gold", "foil_gold"], "metal", segments=10,
                cap_bottom=True)
    transform(hub, loc=(x, 0, cz), rot=(0, 90 * sx, 0))
    parts.append(hub)
    return parts


def chassis():
    parts = []
    parts.append(rod("axle", (-0.98, 0, 0.42), (0.98, 0, 0.42), 0.035, "iron", sides=6))
    parts.append(box("frame", (1.9, 1.06, 0.08), WD, "wood", loc=(0, 0, 0.47), bevel=0.02, segments=1))
    for sy in (-0.3, 0.3):  # leaf-spring blocks
        parts.append(box(f"block{sy}", (1.7, 0.08, 0.06), WD, "wood", loc=(0, sy, 0.41), bevel=0.015, segments=1))
    # little kickstand legs at the corners (it is parked)
    for sx in (-0.8, 0.8):
        for sy in (-0.42, 0.42):
            parts.append(rod(f"leg{sx}{sy}", (sx, sy, 0.45), (sx * 1.02, sy * 1.05, 0.03), 0.028, WD, mat="wood", sides=5))
            parts.append(blob(f"foot{sx}{sy}", 0.04, "iron", mat="metal", loc=(sx * 1.02, sy * 1.05, 0.02),
                              scale=(1, 1, 0.5), segments=6, rings=3, noise_amp=0.0, axis="Z"))
    return parts


# ------------------------------------------------------------------ body
def body():
    parts = []
    W, D, z0, z1 = 1.84, 1.0, 0.51, 1.14
    b = box("body", (W, D, z1 - z0), BODY, "planks", loc=(0, 0, (z0 + z1) / 2), bevel=0.03, segments=2)
    parts.append(b)
    # corner posts
    for sx in (-1, 1):
        for sy in (-1, 1):
            parts.append(box(f"corner{sx}{sy}", (0.09, 0.09, z1 - z0 + 0.04), WD, "wood",
                             loc=(sx * (W / 2 - 0.025), sy * (D / 2 - 0.025), (z0 + z1) / 2), bevel=0.02, segments=1))
    # cream rails along the front and sides (top + bottom)
    for z in (z0 + 0.04, z1 - 0.05):
        parts.append(box(f"rail_f{z}", (W - 0.04, 0.04, 0.06), TRIM, "wood", loc=(0, -D / 2 - 0.012, z), bevel=0.015,
                         segments=1))
        for sx in (-1, 1):
            parts.append(box(f"rail_s{sx}{z}", (0.04, D - 0.04, 0.06), TRIM, "wood", loc=(sx * (W / 2 + 0.012), 0, z),
                             bevel=0.015, segments=1))
    # front split into three panels by cream stiles
    for x in (-0.31, 0.31):
        parts.append(box(f"stile{x}", (0.05, 0.04, z1 - z0 - 0.1), TRIM, "wood", loc=(x, -D / 2 - 0.012, (z0 + z1) / 2),
                         bevel=0.015, segments=1))
    zc = (z0 + z1) / 2 + 0.005
    fy = -D / 2 - 0.02
    # centre panel: big painted lollipop swirl on a white disc
    disc = lathe("swirl_bg", [(0.0, 0.0), (0.2, 0.0), (0.205, 0.012), (0.19, 0.022), (0.0, 0.024)],
                 [WHITE, WHITE, WHITE, WHITE], "flat", segments=16, cap_bottom=False)
    transform(disc, loc=(0, fy, zc + 0.02), rot=(90, 0, 0))
    parts.append(disc)
    sp = []
    for k in range(17):
        a = math.radians(k * 48)
        r = 0.02 + 0.165 * k / 16
        sp.append((math.cos(a) * r, math.sin(a) * r, 0))
    s = stroke("swirl", sp, 0.055, PINK, thick=0.012, taper=(0.3, 0.0), z=0, sides=4)
    transform(s, loc=(0, fy - 0.022, zc + 0.02), rot=(90, 0, 0))
    parts.append(s)
    # side panels: a wrapped candy each
    for i, (x, col) in enumerate(((-0.62, "candy_grape"), (0.62, "candy_lemon"))):
        parts += wrapped_candy(f"pc{i}", (x, fy - 0.03, zc), 0.1, col, yaw=0, roll=-12 if i else 12, flat=True)
    # side of the cart: a big painted heart-ish candy corn stripe band
    for sx in (-1, 1):
        for j, col in enumerate(("candy_corn_yellow", "pumpkin", "candy_corn_white")):
            parts.append(box(f"sband{sx}{j}", (0.012, D - 0.2, 0.06), col, "wood",
                             loc=(sx * (W / 2 + 0.008), 0, z0 + 0.16 + j * 0.065)))
    # counter top with overhang
    parts.append(box("counter", (W + 0.12, D + 0.14, 0.06), "wood_light", "wood", loc=(0, -0.02, z1 + 0.025),
                     bevel=0.022, segments=2))
    return parts


def wrapped_candy(name, loc, s, col, yaw=0.0, roll=0.0, flat=False, twist_col=None):
    """Wrapped sweet: a plump ellipsoid with a pinched twist + fan on each end."""
    parts = []
    twist_col = twist_col or col
    cx, cy, cz = loc
    sc = (1.25, 0.55 if flat else 0.85, 0.85)
    parts.append(blob(f"{name}_b", s, col, mat="flat", loc=(0, 0, 0), scale=sc, segments=8, rings=5, noise_amp=0.0,
                      axis="X"))
    for sx in (-1, 1):
        fan = lathe(f"{name}_f{sx}", [(0.0, 0.0), (s * 0.18, s * 0.05), (s * 0.55, s * 0.62), (0.0, s * 0.62)],
                    twist_col, "flat", segments=6, cap_bottom=False, cap_top=False)
        transform(fan, loc=(sx * s * 1.12, 0, 0), rot=(0, 90 * sx, 0), scale=(1, 0.55 if flat else 1, 1))
        parts.append(fan)
    m = K.merge(parts, name)
    m.rotation_euler = Euler((0, math.radians(roll), math.radians(yaw)))
    m.location = Vector(loc)
    if flat:
        m.rotation_euler = Euler((0, math.radians(roll), 0))
    bake(m)
    return [m]


# ------------------------------------------------------------------ posts, awning, sign
def cane_post(name, x, y, z0, z1, r=0.045):
    n = 12
    pts = [(x, y, z0 + (z1 - z0) * i / (n - 1)) for i in range(n)]

    def col(i, j):
        return (("candy_cherry", "flat") if (i + j) % 3 == 0 else (WHITE, "flat"))
    ob = sweep(name, pts, r, color=WHITE, mat="flat", sides=6, colors=col, caps=(False, False))
    knob = blob(f"{name}_k", r * 1.5, "foil_gold", mat="metal", loc=(x, y, z1 + 0.01), scale=(1, 1, 0.8), segments=6,
                rings=4, noise_amp=0.0, axis="Z")
    foot = lathe(f"{name}_f", [(r * 1.6, z0 - 0.005), (r * 1.6, z0 + 0.03), (r * 1.1, z0 + 0.05)], "foil_gold", "metal",
                 segments=6, cap_bottom=False, cap_top=False, loc=(0, 0, 0))
    transform(foot, loc=(x, y, 0))
    return [ob, knob, foot]


def awning():
    parts = []
    bm = bmesh.new()
    proj, _ = K._uv_layers(bm)
    prof = catmull([(y, 0, z) for y, z in AW_PROF], per=2)
    prof = [(p[0], p[2]) for p in prof]
    nx = AW_N * 2 + 1
    sw = 2 * AW_X / AW_N
    xs = [-AW_X + sw * k / 2 for k in range(nx)]

    def bulge(k):
        return 0.028 if k % 2 == 1 else 0.0

    top, bot = [], []
    for i, (y, z) in enumerate(prof):
        rt, rb = [], []
        for k, x in enumerate(xs):
            rt.append(bm.verts.new((x, y, z + bulge(k))))
            rb.append(bm.verts.new((x, y, z + bulge(k) * 0.6 - AW_T)))
        top.append(rt)
        bot.append(rb)
    faces = {}
    for i in range(len(prof) - 1):
        for k in range(nx - 1):
            stripe = k // 2
            ft = bm.faces.new((top[i][k], top[i][k + 1], top[i + 1][k + 1], top[i + 1][k]))
            fb = bm.faces.new((bot[i][k], bot[i + 1][k], bot[i + 1][k + 1], bot[i][k + 1]))
            faces.setdefault(stripe % 2, []).extend([ft, fb])
    m = len(prof) - 1
    for k in range(nx - 1):  # back + front walls
        stripe = k // 2
        faces.setdefault(stripe % 2, []).append(bm.faces.new((bot[0][k], bot[0][k + 1], top[0][k + 1], top[0][k])))
        faces.setdefault(stripe % 2, []).append(bm.faces.new((top[m][k], top[m][k + 1], bot[m][k + 1], bot[m][k])))
    for i in range(len(prof) - 1):  # side walls
        faces.setdefault(0, []).append(bm.faces.new((top[i][0], top[i + 1][0], bot[i + 1][0], bot[i][0])))
        faces.setdefault((AW_N - 1) % 2, []).append(bm.faces.new((bot[i][-1], bot[i + 1][-1], top[i + 1][-1], top[i][-1])))
    bmesh.ops.recalc_face_normals(bm, faces=bm.faces)
    bm.normal_update()
    uv_box(bm, bm.faces)
    K._paint_bm(bm, PINK, "fabric", faces[0])
    K._paint_bm(bm, WHITE, "fabric", faces[1])
    for f in bm.faces:
        f.smooth = True
    parts.append(new_obj(bm, "awning", None))
    # front valance: band + scallop per stripe, bobble at each tip
    yf, zf = prof[-1]
    for s in range(AW_N):
        x0, x1 = -AW_X + s * sw, -AW_X + (s + 1) * sw
        col = PINK if s % 2 == 0 else WHITE
        parts.append(scallop(f"vf{s}", Vector((x0, yf - 0.005, zf + 0.01)), Vector((x1, yf - 0.005, zf + 0.01)), 0.07, col))
        parts.append(blob(f"bob{s}", 0.022, WHITE if s % 2 == 0 else PINK, mat="felt",
                          loc=((x0 + x1) / 2, yf - 0.005, zf + 0.01 - 0.07 - sw / 2 * 0.85 - 0.01), segments=5, rings=3,
                          noise_amp=0.0, axis="Z"))
    # side valances following the slope
    ns = 5
    for sx in (-1, 1):
        for s in range(ns):
            ya = prof[0][0] + (yf - prof[0][0]) * s / ns
            yb = prof[0][0] + (yf - prof[0][0]) * (s + 1) / ns
            col = PINK if s % 2 == 0 else WHITE
            parts.append(scallop(f"vs{sx}{s}", Vector((sx * (AW_X + 0.005), ya, aw_z(ya) + 0.01)),
                                 Vector((sx * (AW_X + 0.005), yb, aw_z(yb) + 0.01)), 0.06, col))
    # gold piping along the front edge and the crest
    parts.append(rod("pipe_f", (-AW_X - 0.02, yf - 0.01, zf + 0.012), (AW_X + 0.02, yf - 0.01, zf + 0.012), 0.022,
                     "foil_gold", mat="metal", sides=6))
    for sx in (-1, 1):
        pts = [(sx * (AW_X + 0.01), y, z + 0.008) for y, z in prof]
        parts.append(sweep(f"pipe_s{sx}", pts, 0.018, color="foil_gold", mat="metal", sides=6))
    return parts


def scallop(name, p0, p1, drop, col, thick=0.022, n=4):
    """Fabric tab hanging below the edge p0-p1: straight band of `drop`, then a half-round bottom."""
    d = (p1 - p0)
    w = d.length
    u = d.normalized()
    nrm = u.cross(Vector((0, 0, 1))).normalized()
    mid = (p0 + p1) / 2
    pts = [p0, p1, p1 - Vector((0, 0, drop))]
    c = mid - Vector((0, 0, drop))
    for k in range(1, n):
        a = math.pi * k / n
        pts.append(c + u * math.cos(a) * w / 2 - Vector((0, 0, math.sin(a) * w / 2 * 0.85)))
    pts.append(p0 - Vector((0, 0, drop)))
    bm = bmesh.new()
    f_ = [bm.verts.new(p - nrm * thick / 2) for p in pts]
    b_ = [bm.verts.new(p + nrm * thick / 2) for p in pts]
    bm.faces.new(f_)
    bm.faces.new(list(reversed(b_)))
    m = len(pts)
    for i in range(m):
        j = (i + 1) % m
        bm.faces.new((f_[i], f_[j], b_[j], b_[i]))
    bmesh.ops.recalc_face_normals(bm, faces=bm.faces)
    bm.normal_update()
    uv_box(bm, bm.faces)
    K._paint_bm(bm, col, "fabric")
    return new_obj(bm, name, False)


def sign():
    parts = []
    y = -0.62
    zc, H, W = 2.6, 0.44, 1.8
    z0 = zc - H / 2

    def outline(w, h, r):
        pts = []
        for k in range(4):  # bottom-right corner
            a = -math.pi / 2 + math.pi / 2 * k / 3
            pts.append((w / 2 - r + math.cos(a) * r, -h / 2 + r + math.sin(a) * r))
        for k in range(9):  # arched top
            t = k / 8
            x = w / 2 - r - (w - 2 * r) * t
            pts.append((x, h / 2 - r * 0.6 + math.sin(math.pi * t) * 0.07))
        for k in range(4):  # bottom-left corner
            a = math.pi + math.pi / 2 * k / 3
            pts.append((-w / 2 + r + math.cos(a) * r, -h / 2 + r + math.sin(a) * r))
        return pts

    def board(name, w, h, r, yy, t, col, mat):
        ob = K.polygon_prism(name, outline(w, h, r), t, color=col, mat=mat)
        transform(ob, loc=(0, yy + t / 2, zc), rot=(90, 0, 0))
        # polygon_prism extrudes along +Z (-> -Y after the rotation): shift so the board spans yy..yy+t
        return ob

    frame = board("sframe", W + 0.1, H + 0.1, 0.09, y + 0.0, 0.05, "foil_gold", "wood")
    face = board("sface", W - 0.02, H - 0.04, 0.07, y - 0.03, 0.02, "witch_purple", "wood")
    parts += [frame, face]
    # little painted stars + dots around the edge of the face (no text: the game's Label3D goes here)
    for i, x in enumerate((-0.78, 0.78)):
        st = []
        for k in range(4):
            a = math.radians(k * 45)
            pts = [(math.cos(a) * -0.05, math.sin(a) * -0.05, 0), (0, 0, 0), (math.cos(a) * 0.05, math.sin(a) * 0.05, 0)]
            st.append(stroke(f"sst{i}{k}", pts, 0.022 if k % 2 == 0 else 0.014, "foil_gold", thick=0.008,
                             taper=(0.2, 0.2), z=0, sides=4))
        s = K.merge(st, f"sstar{i}")
        transform(s, loc=(x, y - 0.046, zc + 0.1), rot=(90, 0, 0))
        parts.append(s)
    # posts holding the board up out of the awning
    for x in (-0.6, 0.6):
        parts.append(box(f"spost{x}", (0.06, 0.04, 0.2), WD, "wood", loc=(x, y + 0.06, z0 - 0.04), bevel=0.012,
                         segments=1))
    # lollipop medallions flanking the board
    for i, (x, c1, c2) in enumerate(((-1.03, "candy_mint", WHITE), (1.03, "candy_cherry", WHITE))):
        parts += lollipop(f"slol{i}", (x, y - 0.01, zc + 0.02), 0.15, c1, c2, stick=0.0, face_y=True, seg=14)
    return parts


def lollipop(name, loc, r, c1, c2, stick=0.25, face_y=True, tilt=0.0, seg=9):
    """Swirl lollipop: disc with concentric two-colour rings + white stick going down."""
    parts = []
    t = max(0.012, r * 0.16)
    prof = [(0.0, -t), (r * 0.92, -t), (r, 0.0), (r * 0.92, t)]
    cols = [c2, c2, c2]
    n = 3
    for k in range(n - 1, -1, -1):
        prof.append((r * 0.92 * k / n, t * (1.0 + 0.12 * (n - k) / n)))
        cols.append(c1 if k % 2 == 1 else c2)
    disc = lathe(f"{name}_d", prof, cols, "flat", segments=seg, cap_bottom=False, cap_top=False, smooth=True)
    if face_y:
        transform(disc, loc=loc, rot=(90, tilt, 0))
    else:
        transform(disc, loc=loc)
    parts.append(disc)
    if stick:
        x, y, z = loc
        parts.append(rod(f"{name}_s", (x, y + 0.01, z - r * 0.9), (x, y + 0.01, z - r - stick), 0.012, WHITE, mat="flat",
                         sides=5))
    return parts


# ------------------------------------------------------------------ counter goods
def jar(name, x, y, kind, layers, lid, S=1.3):
    parts = []
    z = COUNTER_Z
    if kind == "round":
        prof = [(0.0, 0.0), (0.09, 0.0), (0.135, 0.04), (0.155, 0.11), (0.14, 0.19), (0.095, 0.245), (0.085, 0.27),
                (0.095, 0.285)]
    else:
        prof = [(0.0, 0.0), (0.11, 0.0), (0.12, 0.02), (0.12, 0.29), (0.1, 0.33), (0.085, 0.345), (0.095, 0.36)]
    nb = len(prof) - 1
    # candy layers fill the bottom bands, glass (pale) above
    cols = [layers[i] if i < len(layers) else "foil_silver" for i in range(nb)]
    mats = ["flat"] * nb
    body = lathe(f"{name}_jar", [(r * S, z + h * S) for r, h in prof], cols, mats, segments=10, cap_bottom=False,
                 cap_top=False)
    transform(body, loc=(x, y, 0))
    parts.append(body)
    top_z = z + prof[-1][1] * S
    if lid:
        cap = lathe(f"{name}_lid", [(0.105 * S, top_z - 0.005), (0.11 * S, top_z + 0.025), (0.08 * S, top_z + 0.045), (0.0, top_z + 0.05)],
                    lid, "metal", segments=10, cap_bottom=True, cap_colors=(lid, lid), cap_mats=("metal", "metal"))
        transform(cap, loc=(x, y, 0))
        parts.append(cap)
        parts.append(blob(f"{name}_knob", 0.032, "foil_gold", mat="metal", loc=(x, y, top_z + 0.07), segments=6, rings=4,
                          noise_amp=0.0, axis="Z"))
    else:
        # heaped gumballs poking out of the open mouth
        rnd = random.Random(hash(name) % 1000)
        cc = ["gumball_red", "gumball_blue", "candy_lemon", "candy_mint", "candy_pink", "candy_grape"]
        for k in range(4):
            a = k * 2 * math.pi / 3 + 0.3
            rr = 0.0 if k == 0 else 0.055
            parts.append(blob(f"{name}_g{k}", 0.05, cc[(k + len(name)) % len(cc)], mat="flat",
                              loc=(x + math.cos(a) * rr * S * (k > 0), y + math.sin(a) * rr * S * (k > 0),
                                   top_z + (0.05 if k == 0 else 0.005)), segments=6, rings=4, noise_amp=0.0, axis="Z"))
    # pale glass highlight streak on the front-left (reads as glass)
    a = math.radians(-125)
    k = len(layers)
    rr = max(prof[k][0], prof[k + 1][0]) * S + 0.004
    hz = z + (prof[k][1] + prof[k + 1][1]) / 2 * S
    hl = box(f"{name}_hl", (0.022, 0.008, 0.07), "trim_white", "flat", loc=(x + math.cos(a) * rr, y + math.sin(a) * rr, hz),
             rot=(0, 0, -125 + 90), bevel=0.003, segments=1)
    parts.append(hl)
    return parts


def counter_goods():
    parts = []
    parts += jar("j1", -0.64, 0.12, "round", ["candy_pink", "candy_mint", "candy_pink"], "metal_red")
    parts += jar("j2", -0.22, 0.2, "tall", ["candy_lemon", "candy_orange_light" if False else "pumpkin_light", "candy_grape",
                                              "candy_lemon"], None)
    parts += jar("j3", 0.22, 0.2, "tall", ["candy_mint", "candy_cherry", "candy_mint", "lolly_white"], "witch_purple")
    parts += jar("j4", 0.64, 0.12, "round", ["candy_grape", "candy_lemon", "candy_grape"], None)
    # front display: wrapped candies in a little wicker tray
    tray = lathe("tray", [(0.0, COUNTER_Z), (0.2, COUNTER_Z), (0.23, COUNTER_Z + 0.05), (0.21, COUNTER_Z + 0.055),
                          (0.18, COUNTER_Z + 0.02), (0.0, COUNTER_Z + 0.02)],
                 ["wicker", "wicker", "wicker", "wicker", "wicker"], "straw", segments=14, cap_bottom=False)
    transform(tray, loc=(-0.08, -0.36, 0), scale=(1.5, 0.75, 1.0))
    parts.append(tray)
    for i, (x, y, col, yaw) in enumerate([(-0.28, -0.36, "wrapper_red", 20), (-0.1, -0.31, "candy_lemon", -15),
                                          (0.08, -0.38, "wrapper_purple", 40)]):
        parts += wrapped_candy(f"wc{i}", (x, y, COUNTER_Z + 0.065), 0.055, col, yaw=yaw)
    # cup of lollipops on the right front
    cup = lathe("cup", [(0.0, COUNTER_Z), (0.07, COUNTER_Z), (0.085, COUNTER_Z + 0.14), (0.075, COUNTER_Z + 0.145),
                        (0.0, COUNTER_Z + 0.13)], ["terracotta", "terracotta", "terracotta", "choco_dark"], "flat",
                segments=10, cap_bottom=False)
    transform(cup, loc=(0.52, -0.36, 0))
    parts.append(cup)
    for i, (dx, dz, tilt, c1) in enumerate([(-0.05, 0.36, 12, "candy_cherry"), (0.05, 0.31, -14, "candy_mint"),
                                            (0.0, 0.43, 0, "candy_grape")]):
        loc = (0.52 + dx, -0.37, COUNTER_Z + dz)
        parts += lollipop(f"lol{i}", loc, 0.075, c1, WHITE, stick=COUNTER_Z + dz - 0.075 - (COUNTER_Z + 0.08), tilt=tilt)
    # jack-o'-lantern on the left counter corner
    parts += jack("jack", (-0.88, -0.38, COUNTER_Z), 0.14)
    return parts


def jack(name, loc, r):
    x, y, z = loc
    parts = [ribbed(f"{name}_b", r, r * 1.35, "pumpkin", ribs=8, depth=0.06, rings=5, sides=12, loc=(x, y, z))]
    parts.append(rod(f"{name}_st", (x, y, z + r * 1.3), (x + 0.02, y + 0.01, z + r * 1.6), 0.02, "stem_green", mat="wood",
                     sides=6))
    # glowing carved face on the front (-Y) side
    fy = y - r * 0.98
    cz = z + r * 0.7
    for sx in (-1, 1):
        eye = K.polygon_prism(f"{name}_e{sx}", [(-0.028, -0.02), (0.028, -0.02), (0.0, 0.03)], 0.02, color="glow_jack")
        transform(eye, loc=(x + sx * 0.05, fy + 0.012, cz + 0.035), rot=(90, 0, 0))
        parts.append(eye)
    mouth = stroke(f"{name}_m", [(-0.065, 0.0, 0), (-0.03, -0.025, 0), (0.03, -0.025, 0), (0.065, 0.0, 0)], 0.03,
                   "glow_jack", thick=0.02, taper=(0.5, 0.5), sides=4, z=0)
    transform(mouth, loc=(x, fy + 0.004, cz - 0.035), rot=(90, 0, 0))
    parts.append(mouth)
    return parts


# ------------------------------------------------------------------ lantern, pumpkins, mat
def hanging_lantern():
    x, y, z = LANTERN
    parts = []
    B = "lamp_black"
    G = "glow_lantern"
    top = aw_z(y) - AW_T
    parts.append(rod("lchain", (x, y, top + 0.01), (x, y, z + 0.17), 0.01, B, sides=4, chamfer=0))
    parts.append(K.torus("lring", 0.03, 0.009, color=B, mat="metal", major_seg=8, minor_seg=4, loc=(x, y, z + 0.19),
                         rot=(90, 0, 0)))
    parts.append(lathe("lroof", [(0.11, z + 0.09), (0.125, z + 0.1), (0.05, z + 0.15), (0.0, z + 0.165)], B, "metal",
                       segments=4, phase=math.radians(45), cap_bottom=True, cap_colors=(G, B), cap_mats=("flat", "metal"),
                       smooth=False, loc=(x, y, 0)))
    ang = []
    cols = []
    for i in range(4):
        a = 45 + 90 * i
        ang += [a - 8, a + 8]
        cols += [B, G]
    parts.append(lathe("lglass", [(0.075, z - 0.09), (0.095, z + 0.09)], G, "flat", angles=ang, band_cols={0: cols},
                       cap_bottom=False, cap_top=False, smooth=False, loc=(x, y, 0)))
    parts.append(lathe("lbase", [(0.0, z - 0.14), (0.03, z - 0.13), (0.09, z - 0.1), (0.09, z - 0.085)], B, "metal",
                       segments=4, phase=math.radians(45), cap_top=True, cap_colors=(B, G), cap_mats=("metal", "flat"),
                       smooth=False, loc=(x, y, 0)))
    return parts


def pumpkins():
    parts = []
    for i, (x, y, r, h, col) in enumerate([(1.0, -0.72, 0.23, 0.3, "pumpkin"), (1.28, -0.5, 0.15, 0.19, "pumpkin_dark"),
                                           (-1.12, -0.66, 0.17, 0.22, "pumpkin_light")]):
        parts.append(ribbed(f"pk{i}", r, h, col, ribs=8, depth=0.07, rings=5, sides=14, loc=(x, y, -0.005)))
        parts.append(rod(f"pks{i}", (x, y, h - 0.03), (x + 0.03, y + 0.02, h + 0.06), 0.022, "stem_green", mat="wood",
                         sides=6))
    # a curly vine + leaf on the big one
    lf = blob("pkleaf", 0.07, "vine", mat="leaves", scale=(1.4, 1.0, 0.25), segments=6, rings=3, noise_amp=0.0, axis="X")
    transform(lf, loc=(0.9, -0.7, 0.29), rot=(0, 15, 160))
    parts.append(lf)
    return parts


def doormat():
    """Flat round braided rug centred on the shop trigger. Top at 0.035 m."""
    zt = 0.035
    # (outer radius of the band, colour, material), outside in
    bands = [(0.75, "pumpkin_dark", "burlap"), (0.72, "pumpkin_dark", "burlap"), (0.6, "burnt_amber", "burlap"),
             (0.53, "glow_lantern", "flat"), (0.47, "burnt_amber", "burlap"), (0.3, "candy_pink", "felt"),
             (0.22, "lolly_white", "felt"), (0.15, "candy_pink", "felt")]
    prof = [(0.745, 0.0), (0.75, zt - 0.012)]
    cols, mats = ["pumpkin_dark"], ["burlap"]
    for i, (r, c, m) in enumerate(bands[1:]):
        prof.append((r, zt))
        cols.append(bands[i][1])
        mats.append(bands[i][2])
    prof.append((0.0, zt))
    cols.append(bands[-1][1])
    mats.append(bands[-1][2])
    mat = lathe("mat", prof, cols, mats, segments=24, cap_bottom=False, cap_top=False, smooth=False)
    me = mat.data
    bm = bmesh.new()
    bm.from_mesh(me)
    bm.normal_update()
    down = [f for f in bm.faces if f.normal.z < -0.5]
    if down:
        bmesh.ops.reverse_faces(bm, faces=down)
    bm.to_mesh(me)
    bm.free()
    transform(mat, loc=(0, MAT_Y, 0))
    return [mat]


def build():
    K.reset()
    groups = {"wheels": wheel(-1) + wheel(1), "chassis": chassis(), "body": body(),
              "posts": [p for x in (-0.86, 0.86) for y in (-0.46, 0.46)
                        for p in cane_post(f"post{x}{y}", x, y, COUNTER_Z + 0.04, aw_z(y) - AW_T + 0.005)],
              "awning": awning(), "sign": sign(), "goods": counter_goods(), "lantern": hanging_lantern(),
              "pumpkins": pumpkins(), "mat": doormat()}
    if "--stats" in ARGS:
        print("ASSET stats", {k: sum(sum(len(p.vertices) - 2 for p in o.data.polygons) for o in v)
                              for k, v in groups.items()})
    parts = [p for v in groups.values() for p in v]
    cart = K.merge(parts, "shop_cart")
    return finish([cart], "props/shop_cart", "shop_cart", ao_radius=0.5)


build()
