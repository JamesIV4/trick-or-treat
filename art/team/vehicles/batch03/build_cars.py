"""Decorative parked cars (vehicles / batch03), rebuilt from scratch.

    blender -b --factory-startup --python build_cars.py -- parked_car parked_wagon parked_pickup [--stats]

Every car: front faces -Y, origin at the ground centre, flat roof (box collision, walkable), <= 4500 tris.
"""
import pathlib
import sys

sys.path.insert(0, str(pathlib.Path(__file__).resolve().parent))
from car_kit import *  # noqa: E402,F401,F403
from car_kit import ARGS, Surf, Cabin, lower_body, fillet_outline, profile_corners, wheel, lamp, plate, jack, \
    hay_bale, ribbon, finish_car, orient, insert_outline_y, split_hood, engine_bay, CHROME, GLASS  # noqa: E402

STATS = "--stats" in ARGS
# Bonnet hinge line (Blender y; front is -Y): a few cm in front of where the windshield base meets the body.
HOOD_HINGE = {"parked_car": -0.90, "parked_wagon": -1.10, "parked_pickup": -0.75}
PAINT = "flat" if "--metal" not in ARGS else "metal"


def stats(parts, label):
    if STATS:
        t = K.tri_count(parts) if isinstance(parts, list) else K.tri_count([parts])
        print(f"STATS {label}: {t}")


# ------------------------------------------------------------------ common details
def bumper(name, S, sy, z, hw, color=CHROME, reach=66, c_off=0.5, out=0.035, rv=0.062, rh=0.036, steps=10):
    """Wrap-around bumper bar traced from the body at height z (sy=-1 front, +1 rear)."""
    ymax = max(v.co.y * sy for v in S.ob.data.vertices)
    c = Vector((0, sy * (ymax - c_off), z))
    pts = []
    for k in range(steps + 1):
        a = math.radians(-reach + 2 * reach * k / steps)
        d = Vector((math.sin(a), sy * math.cos(a), 0))
        hit, nrm = S.hit(c + d * 3.0, -d)
        if hit is None:
            continue
        pts.append(hit + d * out)
    return sweep(name, pts, rv, color=color, mat="metal", sides=6, shape=ellipse(1.0, rh / rv), up_hint=(0, 0, 1),
                 phase=math.pi / 6)


def door_seams(name, S, sx, path_yz, color, width=0.012):
    pts, outs = [], []
    for y, z in path_yz:
        p, n = S.side(sx, y, z)
        if p is None:
            continue
        pts.append(p)
        outs.append(n)
    return ribbon(name, pts, outs, width, color, lift=0.003)


def handle(name, S, sx, y, z, color=CHROME):
    p, n = S.side(sx, y, z)
    return _place_side(box(name, (0.035, 0.15, 0.035), color, "metal"), p, n)


def at(ob, loc):
    ob.location = Vector(loc)
    return bake(ob)


def _place_side(ob, p, n, lift=0.012):
    """Move an object built around the origin (long axis Y) onto a side surface point."""
    ob.location = p + n * lift
    yaw = math.atan2(n.y, n.x) - (0 if n.x > 0 else math.pi)
    ob.rotation_euler = (0, 0, yaw)
    return bake(ob)


def mirror(name, S, sx, y, z_body, color, reach=0.08):
    """Little bullet wing mirror on a chrome stalk; the mirror face (toward +Y) is chrome."""
    p, n = S.side(sx, y, z_body)
    base = p + Vector((sx * -0.02, 0, 0))
    head = Vector((p.x + sx * reach, y + 0.02, z_body + 0.1))
    parts = [rod(f"{name}_s", base, head - Vector((sx * 0.02, 0, 0.02)), 0.016, CHROME, sides=5, chamfer=0)]
    hd = box(f"{name}_h", (0.07, 0.05, 0.08), color, PAINT, loc=tuple(head), bevel=0.022, segments=1)
    bake(hd)
    paint_world(hd, CHROME, lambda c, nn: nn.y > 0.7)
    parts.append(hd)
    return parts


# ------------------------------------------------------------------ 1. parked_car: bubble hatchback
def parked_car():
    K.reset()
    BODY, ROOF, SEAM = "house_teal", "cream", "roof_teal"
    HW, ZB, BELT = 0.85, 0.22, 0.92
    WR, WY, AZ, AR = 0.32, 1.1, 0.32, 0.39
    spec = dict(zb=ZB,
                front_bottom=(-1.70, ZB, 0.15, 4),
                nose_top=(-1.73, 0.76, 0.30, 5),
                top=[(-0.76, BELT, 0.3, 3)],
                tail_top=(1.66, BELT + 0.02, 0.26, 5),
                tail_bottom=(1.70, ZB, 0.14, 4),
                arches=[(WY, AZ, AR, 8), (-WY, AZ, AR, 8)], mouth_r=0.05)
    P = fillet_outline(profile_corners(spec))
    P = insert_outline_y(P, HOOD_HINGE["parked_car"], BELT - 0.3)  # bonnet hinge line (collinear vertex)
    body, info = lower_body("body", P, HW, BODY, mat=PAINT, r_top=0.09, r_front=0.22, r_back=0.2, r_bot=0.03,
                            arches=[(WY, AZ, AR), (-WY, AZ, AR)], zb=ZB, belt=BELT, cowl_y=-0.76, crown=0.03,
                            bulge_f=0.05, bulge_b=0.03, rounding_steps=(90, 66, 44, 22, 0))
    S = Surf([body])
    S.ob = body
    parts = [body]
    stats(body, "body")
    cab = Cabin(z_base=BELT - 0.05, z_sill=BELT + 0.05, z_head=1.335, z_edge=1.37, z_top=1.48, ease_f=1.3, roof_steps=(30, 60, 90), body_mat=PAINT, roof_mat=PAINT,
                base=(0.74, -0.80, 1.60, 0.26), edge=(0.60, -0.12, 1.30, 0.34), n_arc=3,
                side_glass=((0.0, 0.47), (0.55, 0.86)), front_glass=((0, 1),), back_glass=((0, 1),),
                bulge_f=0.05, bulge_b=0.02, body=BODY, roof=ROOF)
    cabin = cab.build("cabin")
    parts.append(cabin)
    stats(cabin, "cabin")
    sk = []
    sk += cab.streaks("stF", "F", [(0.16, 0.30, 0.07, 0.0, 1.0), (0.38, 0.52, 0.025, 0.0, 1.0)])
    sk += cab.streaks("stR", "R", [(0.10, 0.20, 0.06, 0.0, 1.0)])
    sk += cab.streaks("stL", "L", [(0.10, 0.20, 0.06, 0.0, 1.0)])
    sk += cab.streaks("stB", "B", [(0.2, 0.32, 0.05, 0.0, 1.0)])
    parts += sk
    # wheels
    wl = []
    for sx in (-1, 1):
        for sy in (-1, 1):
            wl += wheel(f"w{sx}{sy}", sx * (HW - 0.18), sy * WY, r=WR, w=0.25, rim=BODY)
    parts += wl
    stats(wl, "wheels")
    # face: bug-eye headlights, smiling oval grille
    det = []
    for sx in (-1, 1):
        p, n = S.end(-1, sx * 0.5, 0.6)
        d = (n.normalized() + Vector((0, -1.0, 0.25))).normalized()
        det.append(lamp(f"hl{sx}", p + n * 0.01, d, 0.105, lens="candy_corn_white", pod=BODY, pod_len=0.08, segs=12))
        p, n = S.end(1, sx * 0.56, 0.66)
        det.append(lamp(f"tl{sx}", p, (n + Vector((0, 1, 0))).normalized(), 0.075, lens="candy_cherry", segs=8,
                        dome=0.02))
    p, n = S.end(-1, 0.0, 0.47)
    gr = lathe("grille", [(0.13, -0.03), (0.13, 0.008), (0.11, 0.02), (0.09, 0.008), (0.0, 0.0)],
               [CHROME, CHROME, CHROME, "charcoal"], ["metal", "metal", "metal", "flat"], segments=12, cap_bottom=False,
               scale=(1.7, 0.55, 1.0))
    det.append(orient(gr, p, (n + Vector((0, -1, 0))).normalized()))
    bar = box("gbar", (0.36, 0.02, 0.018), CHROME, "metal")
    det.append(at(bar, p + Vector((0, -0.012, 0))))
    det.append(bumper("bumper_f", S, -1, 0.3, HW))
    det.append(bumper("bumper_r", S, 1, 0.3, HW))
    # plates
    yf = min(v.co.y for v in det[-2].data.vertices)
    det.append(plate("plate_f", (0, yf - 0.006, 0.3), -1, text="BOO"))
    p, n = S.end(1, 0.0, 0.48)
    det.append(plate("plate_r", (0, p.y + 0.012, 0.48), 1, text="dash"))
    stats(det, "lamps+bumpers+plates")
    # door seams, handles, mirrors
    sd = []
    for sx in (-1, 1):
        sd.append(door_seams(f"seam{sx}", S, sx, [(-0.68, BELT - 0.08), (-0.68, 0.5), (-0.66, 0.3), (-0.62, 0.27),
                                                    (0.3, 0.27), (0.34, 0.3), (0.36, 0.5), (0.36, BELT - 0.08)], SEAM))
        sd.append(handle(f"hd{sx}", S, sx, 0.2, 0.76))
        sd.append(door_seams(f"rub{sx}", S, sx, [(-0.69, 0.56), (0.69, 0.56)], CHROME, width=0.03))
        sd += mirror(f"mir{sx}", S, sx, -0.58, BELT - 0.06, BODY)
    stats(sd, "seams+handles+mirrors")
    parts += det + sd
    # halloween: whip antenna with a tiny jack-o'-lantern on the rear left fender
    p, n = S.top(-0.72, 1.35)
    parts.append(rod("ant", p, p + Vector((0, 0.04, 0.42)), 0.008, CHROME, sides=4, chamfer=0))
    parts += jack("antjack", tuple(p + Vector((0, 0.04, 0.40))), 0.055, face_dir=(0, -1, 0), sides=10, rings=4)
    # opening bonnet + engine bay (the game flaps the hood up when the player bounces on it)
    hood, bay, bd = split_hood(body, info, HOOD_HINGE["parked_car"], (-1.548, 0.777), [(WY, AZ, AR), (-WY, AZ, AR)],
                               under=SEAM, side=BODY)
    eng = engine_bay("eng", hood, bd, block="pumpkin", hoses=("candy_cherry", "candy_blue"))
    stats([hood], "hood")
    stats([bay] + eng, "engine bay")
    return finish_car(parts, "parked_car", hood=dict(ob=hood, bay=[bay] + eng))


def front_face(S, body_col, hl_x, hl_z, hl_r, pod_len, gr_z, gr_r, gr_scale, bars, tl_x, tl_z, tl_r, segs=12):
    det = []
    for sx in (-1, 1):
        p, n = S.end(-1, sx * hl_x, hl_z)
        d = (n.normalized() + Vector((0, -1.0, 0.15))).normalized()
        det.append(lamp(f"hl{sx}", p + n * 0.01, d, hl_r, lens="candy_corn_white", pod=body_col, pod_len=pod_len,
                        segs=segs))
        p, n = S.end(1, sx * tl_x, tl_z)
        det.append(lamp(f"tl{sx}", p, (n + Vector((0, 1, 0))).normalized(), tl_r, lens="candy_cherry", segs=8,
                        dome=0.02))
    p, n = S.end(-1, 0.0, gr_z)
    gr = lathe("grille", [(gr_r, -0.03), (gr_r, 0.008), (gr_r * 0.85, 0.02), (gr_r * 0.7, 0.008), (0.0, 0.0)],
               [CHROME, CHROME, CHROME, "charcoal"], ["metal", "metal", "metal", "flat"], segments=12, cap_bottom=False,
               scale=gr_scale)
    det.append(orient(gr, p, (n + Vector((0, -1, 0))).normalized()))
    w = gr_r * gr_scale[0] * 1.6
    for k, dz in enumerate(bars):
        det.append(at(box(f"gbar{k}", (w, 0.02, 0.018), CHROME, "metal"),
                      p + Vector((0, -0.012, dz))))
    return det


def woody_panel(name, S, sx, y0, y1, z_top, z_bot, ncol=20, wood="wood_light", frame="cream", gaps=(), gap_col="wood"):
    """Faux-wood side panel that follows the body side, framed with cream trim and door gaps."""
    bm = bmesh.new()
    proj, _ = K._uv_layers(bm)
    top, bot, ntop, nbot = [], [], [], []
    for i in range(ncol + 1):
        y = y0 + (y1 - y0) * i / ncol
        zb_ = min(z_bot(y), z_top - 0.035)
        pt, nt = S.side(sx, y, z_top)
        pb, nb = S.side(sx, y, zb_)
        top.append(pt)
        bot.append(pb)
        ntop.append(nt)
        nbot.append(nb)
    vt = [bm.verts.new(p + n * 0.004) for p, n in zip(top, ntop)]
    vb = [bm.verts.new(p + n * 0.004) for p, n in zip(bot, nbot)]
    for i in range(ncol):
        f = bm.faces.new((vb[i], vb[i + 1], vt[i + 1], vt[i]))
        f.normal_update()
        if f.normal.x * sx < 0:
            f.normal_flip()
        for loop in f.loops:
            loop[proj].uv = (loop.vert.co.y, loop.vert.co.z)
        f.smooth = False
    K._paint_bm(bm, wood, "wood")
    parts = [mkobj(bm, name)]
    parts.append(ribbon(f"{name}_ft", top, ntop, 0.035, frame, mat="wood", lift=0.007))
    parts.append(ribbon(f"{name}_fb", bot, nbot, 0.035, frame, mat="wood", lift=0.007))
    for k, (pa, na, pb, nb) in enumerate(((top[0], ntop[0], bot[0], nbot[0]), (top[-1], ntop[-1], bot[-1], nbot[-1]))):
        parts.append(ribbon(f"{name}_fe{k}", [pa, pb], [na, nb], 0.035, frame, mat="wood", lift=0.007))
    for k, gy in enumerate(gaps):
        i = min(range(ncol + 1), key=lambda j: abs(y0 + (y1 - y0) * j / ncol - gy))
        pa, na = S.side(sx, gy, z_top - 0.02)
        pb, nb = S.side(sx, gy, max(z_bot(gy), 0.3) + 0.02)
        parts.append(ribbon(f"{name}_g{k}", [pa, pb], [na, nb], 0.014, gap_col, lift=0.008))
    return parts


def arch_clear(arches, base, margin=0.07):
    def f(y):
        z = base
        for ya, zc, R in arches:
            d = abs(y - ya)
            if d < R + margin:
                z = max(z, zc + math.sqrt((R + margin) ** 2 - d * d))
        return z
    return f


# ------------------------------------------------------------------ 2. parked_wagon: woodie station wagon
def parked_wagon():
    K.reset()
    BODY, ROOF, SEAM = "metal_red", "cream", "flannel_dark"
    HW, ZB, BELT = 0.88, 0.24, 0.95
    WR, WYF, WYR, AZ, AR = 0.33, -1.32, 1.30, 0.33, 0.40
    arches = [(WYR, AZ, AR), (WYF, AZ, AR)]
    spec = dict(zb=ZB,
                front_bottom=(-1.98, ZB, 0.15, 4),
                nose_top=(-2.02, 0.84, 0.28, 5),
                top=[(-0.98, BELT, 0.3, 3)],
                tail_top=(1.99, BELT + 0.01, 0.16, 4),
                tail_bottom=(2.02, ZB, 0.14, 4),
                arches=[(a[0], a[1], a[2], 7) for a in arches], mouth_r=0.05)
    P = fillet_outline(profile_corners(spec))
    P = insert_outline_y(P, HOOD_HINGE["parked_wagon"], BELT - 0.3)
    body, info = lower_body("body", P, HW, BODY, mat=PAINT, r_top=0.09, r_front=0.22, r_back=0.14, r_bot=0.03,
                            arches=arches, zb=ZB, belt=BELT, cowl_y=-0.98, crown=0.03, bulge_f=0.05, bulge_b=0.02)
    S = Surf([body])
    S.ob = body
    parts = [body]
    stats(body, "body")
    cab = Cabin(z_base=BELT - 0.05, z_sill=BELT + 0.04, z_head=1.30, z_edge=1.335, z_top=1.42, ease_f=1.2,
                roof_steps=(40, 90), body_mat=PAINT, roof_mat=PAINT,
                base=(0.77, -0.98, 1.97, 0.22), edge=(0.66, -0.36, 1.90, 0.26), n_arc=3,
                side_glass=((0.0, 0.29), (0.35, 0.62), (0.68, 0.95)), front_glass=((0, 1),),
                back_glass=((0.0, 1.0),), bulge_f=0.05, bulge_b=0.01, body=BODY, roof=ROOF)
    cabin = cab.build("cabin")
    parts.append(cabin)
    stats(cabin, "cabin")
    sk = []
    sk += cab.streaks("stF", "F", [(0.16, 0.28, 0.07, 0.0, 1.0), (0.36, 0.48, 0.025, 0.0, 1.0)])
    for s in ("R", "L"):
        sk += cab.streaks(f"st{s}", s, [(0.06, 0.12, 0.04, 0.0, 1.0), (0.72, 0.78, 0.04, 0.0, 1.0)])
    sk += cab.streaks("stB", "B", [(0.2, 0.3, 0.05, 0.0, 1.0)])
    parts += sk
    wl = []
    for sx in (-1, 1):
        for wy in (WYF, WYR):
            wl += wheel(f"w{sx}{wy}", sx * (HW - 0.18), wy, r=WR, w=0.26, rim=BODY, segs=12)
    parts += wl
    stats(wl, "wheels")
    det = front_face(S, BODY, 0.6, 0.64, 0.1, 0.04, 0.45, 0.13, (2.3, 0.6, 1.0), (0.0,), 0.62, 0.62, 0.08)
    det.append(bumper("bumper_f", S, -1, 0.32, HW))
    det.append(bumper("bumper_r", S, 1, 0.32, HW))
    yf = min(v.co.y for v in det[-2].data.vertices)
    det.append(plate("plate_f", (0, yf - 0.006, 0.32), -1, text="dash"))
    p, n = S.end(1, 0.0, 0.52)
    det.append(plate("plate_r", (0, p.y + 0.012, 0.52), 1, text="BOO"))
    stats(det, "lamps+bumpers+plates")
    sd = []
    zbot = arch_clear(arches, 0.40)
    for sx in (-1, 1):
        sd += woody_panel(f"wood{sx}", S, sx, -0.92, 1.9, BELT - 0.12, zbot, ncol=18, gaps=(0.02, 0.86))
        sd.append(handle(f"hd{sx}a", S, sx, -0.12, BELT - 0.2))
        sd.append(handle(f"hd{sx}b", S, sx, 0.72, BELT - 0.2))
        sd += mirror(f"mir{sx}", S, sx, -0.8, BELT - 0.06, BODY)
    stats(sd, "woody+handles+mirrors")
    parts += det + sd
    # roof rack with a suitcase and two pumpkins (one carved)
    rk = []
    zt = 1.42
    zr = zt + 0.05
    for sx in (-1, 1):
        for y in (-0.12, 0.75, 1.6):
            rk.append(box(f"foot{sx}{y}", (0.045, 0.07, 0.06), "iron", "metal", loc=(sx * 0.52, y, zt + 0.02)))
        rk.append(rod(f"rail{sx}", (sx * 0.52, -0.18, zr), (sx * 0.52, 1.66, zr), 0.02, CHROME, sides=6))
    for y in (-0.15, 1.63):
        rk.append(rod(f"xbar{y}", (-0.52, y, zr), (0.52, y, zr), 0.016, CHROME, sides=6, chamfer=0))
    zc = zt
    rk.append(box("case", (0.62, 0.42, 0.17), "leather", "metal", loc=(0.0, 1.12, zc + 0.085), rot=(0, 0, 4),
                  bevel=0.03, segments=1))
    for k, dx in enumerate((-0.16, 0.16)):
        rk.append(box(f"strap{k}", (0.04, 0.44, 0.185), "caramel", "flat", loc=(dx, 1.12, zc + 0.085), rot=(0, 0, 4)))
    rk.append(rod("case_h", (-0.07, 0.9, zc + 0.12), (0.07, 0.9, zc + 0.12), 0.012, "caramel", sides=4, chamfer=0))
    rk += jack("rjack", (0.2, 0.22, zc - 0.005), 0.15, face_dir=(0.3, -1, 0), sides=10, rings=4)
    rk.append(ribbed("rpk", 0.12, 0.14, "pumpkin_light", ribs=8, depth=0.07, rings=4, sides=10, loc=(-0.22, 0.34, zc - 0.005)))
    rk.append(rod("rpk_st", (-0.22, 0.34, zc + 0.125), (-0.2, 0.35, zc + 0.17), 0.016, "stem_green", mat="wood", sides=5))
    stats(rk, "rack")
    parts += rk
    hood, bay, bd = split_hood(body, info, HOOD_HINGE["parked_wagon"], (-1.835, 0.846), arches, under="roof_red",
                               side=BODY)
    eng = engine_bay("eng", hood, bd, block="metal_blue", hoses=("candy_lime", "candy_cherry"),
                     caps=("candy_corn_yellow", "candy_lime"))
    stats([hood], "hood")
    stats([bay] + eng, "engine bay")
    return finish_car(parts, "parked_wagon", hood=dict(ob=hood, bay=[bay] + eng))


# ------------------------------------------------------------------ 3. parked_pickup: vintage pickup with a bed
def parked_pickup():
    K.reset()
    BODY, ROOF, SEAM = "quilt_yellow", "trim_white", "burnt_amber"
    HW, ZB, BELT = 0.88, 0.27, 1.0
    WR, WYF, WYR, AZ, AR = 0.35, -1.18, 1.12, 0.35, 0.42
    Y0, Y1, FLOOR, WALL = 0.42, 1.80, 0.66, 0.10
    arches = [(WYR, AZ, AR), (WYF, AZ, AR)]
    spec = dict(zb=ZB,
                front_bottom=(-1.92, ZB, 0.15, 4),
                nose_top=(-1.96, 0.9, 0.28, 5),
                top=[(-0.62, BELT, 0.3, 3), (Y0, BELT, 0, 0), (Y0 + 0.025, BELT, 0, 0), (Y1 - 0.025, BELT, 0, 0),
                     (Y1, BELT, 0, 0)],
                tail_top=(1.96, BELT, 0.07, 3),
                tail_bottom=(1.96, ZB, 0.12, 3),
                arches=[(a[0], a[1], a[2], 7) for a in arches], mouth_r=0.05)
    P = fillet_outline(profile_corners(spec))
    P = insert_outline_y(P, HOOD_HINGE["parked_pickup"], BELT - 0.3)
    bed = dict(y0=Y0, y1=Y1, floor=FLOOR, wall=WALL, floor_color="wood_light", floor_mat="planks", inner_color=BODY)
    body, info = lower_body("body", P, HW, BODY, mat=PAINT, r_top=0.08, r_front=0.22, r_back=0.07, r_bot=0.03,
                            arches=arches, zb=ZB, belt=BELT, cowl_y=-0.62, crown=0.035, bulge_f=0.05, bulge_b=0.0,
                            bed=bed)
    # world-space planks on the bed floor (run along the bed)
    K.project(body, "box", lambda c, n: n.z > 0.95 and c.z < FLOOR + 0.01)
    S = Surf([body])
    S.ob = body
    parts = [body]
    stats(body, "body")
    cab = Cabin(z_base=BELT - 0.05, z_sill=BELT + 0.04, z_head=1.47, z_edge=1.515, z_top=1.61, ease_f=1.1,
                roof_steps=(40, 90), body_mat=PAINT, roof_mat=PAINT,
                base=(0.76, -0.62, Y0 - 0.005, 0.2), edge=(0.67, -0.30, Y0 - 0.06, 0.24), n_arc=3,
                side_glass=((0.0, 0.72),), front_glass=((0, 1),), back_glass=((0.16, 0.84),),
                bulge_f=0.05, bulge_b=0.0, body=BODY, roof=ROOF)
    cabin = cab.build("cabin")
    parts.append(cabin)
    stats(cabin, "cabin")
    sk = []
    sk += cab.streaks("stF", "F", [(0.16, 0.28, 0.07, 0.0, 1.0), (0.36, 0.48, 0.025, 0.0, 1.0)])
    for s in ("R", "L"):
        sk += cab.streaks(f"st{s}", s, [(0.1, 0.24, 0.08, 0.0, 1.0)])
    parts += sk
    wl = []
    for sx in (-1, 1):
        for wy in (WYF, WYR):
            wl += wheel(f"w{sx}{wy}", sx * (HW - 0.19), wy, r=WR, w=0.27, rim=BODY, segs=12)
    parts += wl
    stats(wl, "wheels")
    det = front_face(S, BODY, 0.6, 0.7, 0.115, 0.07, 0.46, 0.14, (1.9, 0.95, 1.0), (-0.05, 0.0, 0.05), 0.66, 0.7,
                     0.07)
    det.append(bumper("bumper_f", S, -1, 0.34, HW, rv=0.07))
    det.append(bumper("bumper_r", S, 1, 0.34, HW, rv=0.065, reach=60))
    yf = min(v.co.y for v in det[-2].data.vertices)
    det.append(plate("plate_f", (0, yf - 0.006, 0.34), -1, text="dash"))
    p, n = S.end(1, 0.0, 0.56)
    det.append(plate("plate_r", (0, p.y + 0.012, 0.56), 1, text="BOO"))
    stats(det, "lamps+bumpers+plates")
    sd = []
    for sx in (-1, 1):
        sd.append(door_seams(f"seam{sx}", S, sx, [(-0.6, BELT - 0.09), (-0.6, 0.45), (-0.56, 0.37), (0.30, 0.37),
                                                    (0.34, 0.45), (0.34, BELT - 0.09)], SEAM))
        sd.append(handle(f"hd{sx}", S, sx, 0.2, BELT - 0.16))
        sd += mirror(f"mir{sx}", S, sx, -0.5, BELT - 0.06, BODY)
        # chrome bed-rail caps and stake pockets for a vintage touch
        sd.append(door_seams(f"rail{sx}", S, sx, [(Y0 + 0.06, BELT - 0.05), (Y1 - 0.04, BELT - 0.05)], CHROME, width=0.025))
    # tailgate latch handle
    p, n = S.end(1, 0.0, BELT - 0.12)
    sd.append(at(box("tg_h", (0.22, 0.03, 0.04), CHROME, "metal", bevel=0.012, segments=1), p + Vector((0, 0.012, 0))))
    stats(sd, "seams+handles+mirrors")
    parts += det + sd
    # plank floor grid just above the lofted floor: interior vertices so the AO bake does not darken it all
    bm = bmesh.new()
    proj, _ = K._uv_layers(bm)
    xs = [-(HW - WALL - 0.004), -0.26, 0.26, HW - WALL - 0.004]
    ys = [Y0 + 0.03, 0.8, 1.15, 1.48, Y1 - 0.03]
    grid = [[bm.verts.new((x, y, FLOOR + 0.006)) for x in xs] for y in ys]
    for i in range(len(ys) - 1):
        for j in range(len(xs) - 1):
            f = bm.faces.new((grid[i][j], grid[i][j + 1], grid[i + 1][j + 1], grid[i + 1][j]))
            if f.normal.z < 0:
                f.normal_flip()
            f.smooth = False
            for loop in f.loops:
                loop[proj].uv = (loop.vert.co.x, loop.vert.co.y)
    K._paint_bm(bm, "wood_light", "planks")
    parts.append(mkobj(bm, "bedfloor"))
    # bed load: hay bale, a carved jack-o'-lantern and two pumpkins
    ld = []
    # bale across the front of the bed; the carved jack sits on it so its face clears the rails
    ld += hay_bale("bale", (-0.12, 0.78, FLOOR - 0.005), (1.05, 0.5, 0.42), rot_z=-3)
    ld += jack("bjack", (0.2, 0.8, FLOOR + 0.405), 0.19, face_dir=(0.5, 0.87, 0), sides=12, rings=4)
    ld.append(ribbed("bpk1", 0.16, 0.19, "pumpkin_dark", ribs=8, depth=0.07, rings=4, sides=10,
                     loc=(0.42, 1.45, FLOOR - 0.005)))
    ld.append(rod("bpk1_st", (0.42, 1.45, FLOOR + 0.17), (0.44, 1.47, FLOOR + 0.22), 0.018, "stem_green", mat="wood",
                  sides=5))
    ld.append(ribbed("bpk2", 0.13, 0.15, "pumpkin_light", ribs=8, depth=0.07, rings=4, sides=10,
                     loc=(-0.42, 1.45, FLOOR - 0.005)))
    ld.append(rod("bpk2_st", (-0.42, 1.45, FLOOR + 0.13), (-0.41, 1.43, FLOOR + 0.18), 0.016, "stem_green", mat="wood",
                  sides=5))
    stats(ld, "bed load")
    parts += ld
    def bed_lift(co):
        inside = abs(co.x) < HW - WALL + 0.01 and Y0 - 0.01 < co.y < Y1 + 0.01 and FLOOR - 0.02 < co.z < BELT - 0.005
        return 0.5 if inside else 0.0

    hood, bay, bd = split_hood(body, info, HOOD_HINGE["parked_pickup"], (-1.769, 0.901), arches,
                               under="straw_dark", side=BODY)
    eng = engine_bay("eng", hood, bd, block="witch_purple", hoses=("candy_lime", "candy_orange"),
                     caps=("candy_cherry", "candy_corn_yellow"))
    stats([hood], "hood")
    stats([bay] + eng, "engine bay")
    return finish_car(parts, "parked_pickup", ao_lift=bed_lift, hood=dict(ob=hood, bay=[bay] + eng))


if want("parked_car"):
    parked_car()
if want("parked_wagon"):
    parked_wagon()
if want("parked_pickup"):
    parked_pickup()
