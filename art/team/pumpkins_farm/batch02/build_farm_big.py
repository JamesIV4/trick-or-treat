"""hay_wagon, scarecrow (batch02, textured).

blender --background --factory-startup --python art/team/pumpkins_farm/batch02/build_farm_big.py [-- ids...]
"""
import math
import pathlib
import random
import sys

sys.path.insert(0, str(pathlib.Path(__file__).resolve().parent))
import bmesh  # noqa: E402
import bpy  # noqa: E402
from mathutils import Matrix, Vector  # noqa: E402

import farm_lib as F  # noqa: E402
from build_farm_props import bale_body, rope_loop  # noqa: E402

K = F.K


# ----------------------------------------------------------------------------- shared bits
def cart_wheel(name, radius=0.32, width=0.13, segs=12, rim="metal_red", tyre="charcoal", hub="trim_white"):
    """Chunky cartoon wheel revolved around the local Y axis: rounded tyre, dished red disc, white hub."""
    w = width / 2
    prof = [(0.0, -w - 0.035), (0.075, -w - 0.02), (radius - 0.07, -w * 0.5), (radius - 0.03, -w * 1.02),
            (radius + 0.004, 0.0), (radius - 0.03, w * 1.02), (radius - 0.07, w * 0.5), (0.075, w + 0.02),
            (0.0, w + 0.035)]
    bands = []
    for i in range(len(prof) - 1):
        r0 = prof[i][0]
        r1 = prof[i + 1][0]
        rr = 0.5 * (r0 + r1)
        if rr < 0.08:
            bands.append(hub)
        elif rr < radius - 0.06:
            bands.append(rim)
        else:
            bands.append(tyre)
    # revolve: lathe builds around Z with (r, z); we rotate the result so its axis becomes Y
    ob = F.lathe(name, prof, segs=segs, color=rim, mat="metal", band_colors=bands, cap_top=False, cap_bottom=False,
                 smooth=True, sharp=50)
    K.paint(ob, tyre, where=lambda c, n: math.hypot(c.x, c.y) > radius - 0.06, mat="flat")
    K.paint(ob, hub, where=lambda c, n: math.hypot(c.x, c.y) < 0.085, mat="metal")
    F.transform(ob, Matrix.Rotation(math.radians(90), 4, "X"))
    return ob


# ----------------------------------------------------------------------------- hay wagon
def hay_wagon():
    K.reset()
    parts = []
    BL, BW, top = 2.1, 1.12, 0.7
    # deck: six rounded boards running along X (flat walkable bed)
    nb = 6
    bw = BW / nb
    for k in range(nb):
        y = -BW / 2 + bw * (k + 0.5)
        parts.append(F.board(f"deck{k}", (-BL / 2, y, top - 0.025), (BL / 2, y, top - 0.025), bw - 0.012, 0.05,
                             r=0.01, n=0, up=(0, 0, 1), color="plank" if k % 2 else "wood_light", mat="wood",
                             end_color="wood"))
    # red side boards (two per long side, one per end) with white top trim
    rh = 0.13
    for sy in (-1, 1):
        y = sy * (BW / 2 + 0.025)
        for k, z in enumerate((top + 0.08, top + 0.24)):
            parts.append(F.board(f"side{sy}{k}", (-BL / 2 + 0.04, y, z), (BL / 2 - 0.04, y, z), 0.045, rh, r=0.018,
                                 n=0, up=(0, 0, 1), color="metal_red", mat="wood"))
        parts.append(F.board(f"trim{sy}", (-BL / 2 - 0.02, y, top + 0.335), (BL / 2 + 0.02, y, top + 0.335), 0.07,
                             0.04, r=0.015, n=1, up=(0, 0, 1), color="trim_white", mat="wood"))
    for sx in (-1, 1):
        x = sx * (BL / 2 + 0.0)
        for k, z in enumerate((top + 0.08, top + 0.24)):
            parts.append(F.board(f"end{sx}{k}", (x, -BW / 2 + 0.02, z), (x, BW / 2 - 0.02, z), 0.045, rh, r=0.018,
                                 n=0, up=(0, 0, 1), color="metal_red", mat="wood"))
    # corner + middle stakes (dark wood) holding the sides
    for sx in (-1, 0, 1):
        for sy in (-1, 1):
            if sx == 0:
                continue
            x = sx * (BL / 2 - 0.03)
            y = sy * (BW / 2 + 0.03)
            parts.append(F.board(f"stake{sx}{sy}", (x, y, top - 0.06), (x, y, top + 0.37), 0.07, 0.07, r=0.02, n=0,
                                 up=(1, 0, 0), color="wood_dark", mat="wood", caps=(True, True)))
    # under-frame: two bolsters and iron axles
    wr, wx, wy = 0.32, 0.68, 0.64
    for sx in (-1, 1):
        parts.append(F.board(f"bolster{sx}", (sx * wx, -BW / 2 + 0.05, top - 0.12), (sx * wx, BW / 2 - 0.05, top - 0.12),
                             0.16, 0.14, r=0.025, n=0, up=(0, 0, 1), color="wood_dark", mat="wood"))
        parts.append(K.cylinder(f"axle{sx}", 0.045, 2 * wy + 0.1, color="iron", mat="metal", verts=8, rot=(90, 0, 0),
                                loc=(sx * wx, 0, wr)))
        parts.append(F.board(f"strut{sx}", (sx * wx, 0, wr), (sx * wx, 0, top - 0.18), 0.08, 0.08, r=0.02, n=0,
                             up=(1, 0, 0), color="wood_dark", mat="wood", caps=(False, False)))
        for sy in (-1, 1):
            wheel = cart_wheel(f"wheel{sx}{sy}", radius=wr, width=0.13)
            F.transform(wheel, Matrix.Translation((sx * wx, sy * wy, wr)))
            parts.append(wheel)
    # tongue with a T handle out the front (-X)
    parts.append(F.sweep("tongue", [(-wx, 0, wr + 0.04), (-BL / 2 - 0.05, 0, 0.3), (-BL / 2 - 0.32, 0, 0.24)],
                         0.045, sides=6, color="wood", mat="wood", cap_start=False, cap_end=True, cap_offset=0.3))
    parts.append(F.sweep("handle", [(-BL / 2 - 0.34, -0.22, 0.24), (-BL / 2 - 0.34, 0.22, 0.24)], 0.035, sides=6,
                         color="wood_dark", mat="wood", cap_offset=0.6))
    # hay heap in the back half: lumpy dome, flattened crown, straw texture
    heap = K.ico("heap", 0.5, color="hay", mat="straw", subdiv=3, scale=(1.18, 0.98, 0.95), loc=(0.42, 0.0, top + 0.04),
                 smooth=True)

    def lump(co):
        a = math.atan2(co.y, co.x)
        f = 1.0 + 0.07 * math.sin(3 * a + 1) + 0.05 * math.sin(co.z * 17 + a * 5) + 0.04 * math.cos(7 * a)
        out = Vector((co.x * f, co.y * f, co.z))
        if out.z < -0.08:
            out.z = -0.08
        if out.z > 0.36:
            out.z = 0.36 + (out.z - 0.36) * 0.35
        return out

    K.deform(heap, lump)
    F.bake(heap)
    K.project(heap, "sphere")
    rnd = random.Random(8)
    K.paint(heap, "straw", where=lambda c, n: rnd.random() < 0.35)
    K.paint(heap, "straw_dark", where=lambda c, n: rnd.random() < 0.12)
    parts.append(heap)
    for i, (b, d) in enumerate((((0.08, -0.3, 1.04), (-0.6, -0.6, 0.5)), ((0.72, 0.28, 1.1), (0.4, 0.5, 0.7)),
                                ((0.95, -0.15, 1.0), (1, -0.2, 0.3)), ((0.4, -0.45, 0.98), (0.1, -1, 0.3)),
                                )):
        parts += F.straw_tuft(f"hs{i}", b, d, count=3, length=0.14, radius=0.016, seed=i, spread=0.7)
    # a little bale and two pumpkins riding up front
    bale = bale_body("bale", 0.6, 0.38, 0.32, r=0.06, flakes=3, pinch=0.03, corner_pts=1)
    sec = F.rounded_rect(0.38, 0.32, 0.06, 1)
    bale = K.merge([bale, rope_loop("bband", sec, 0.0, 0.985, 0.32, out=0.006, radius=0.014)], "bale")
    F.transform(bale, Matrix.Translation((-0.45, 0.22, top)) @ Matrix.Rotation(math.radians(12), 4, "Z"))
    parts.append(bale)
    for k, (x, y, z, w, col, sc) in enumerate(((-0.8, -0.24, top, 0.36, "pumpkin", "stem_brown"),
                                                (-0.45, 0.2, top + 0.32, 0.28, "pumpkin_light", "stem_green"))):
        pb = F.pumpkin_body(f"pk{k}", width=w, height=w * 0.72, lobes=6, spl=3, rings=6, groove=0.13, plump=0.5,
                            color=col, fullness=0.72, flatness=0.7, top_dimple=0.03, seed=40 + k, loc=(x, y, z),
                            rot_z=k * 20)
        bpy.context.view_layer.update()
        tp = F.top_point(pb)
        parts.append(pb)
        parts.append(F.stem(f"pks{k}", tp, height=0.09, radius=0.024, curl=0.04, lean_dir=k * 2.5 + 1, color=sc,
                            rings=5, ridges=4, uv_scale=3.5, sink=0.015))
    ob = K.merge(parts, "hay_wagon")
    return F.finish([ob], "props/hay_wagon", "hay_wagon", ao_radius=1.0)


# ----------------------------------------------------------------------------- scarecrow
def stitches(prefix, pts, size=0.016, color="charcoal", every=1, normal_out=None):
    """Little cross stitches at points (each a tiny flat X of two thin boards)."""
    out = []
    for i, p in enumerate(pts[::every]):
        p = Vector(p)
        n = Vector(normal_out(p)) if normal_out else Vector((0, -1, 0))
        t = n.cross(Vector((0, 0, 1)))
        if t.length < 1e-4:
            t = Vector((1, 0, 0))
        t.normalize()
        b = n.cross(t).normalized()
        for k, d in enumerate(((t + b).normalized(), (t - b).normalized())):
            out.append(F.dash(f"{prefix}{i}{k}", p - d * size, p + d * size, 0.005, 0.003, color=color))
    return out


def scarecrow():
    K.reset()
    parts = []
    # post and crossbar (bevelled wood)
    parts.append(F.board("post", (0, 0.08, 0), (0, 0.08, 1.62), 0.1, 0.1, r=0.02, n=1, up=(1, 0, 0), color="wood",
                         mat="wood", caps=(True, True), end_color="wood_light"))
    parts.append(F.board("bar", (-0.72, 0.08, 1.33), (0.72, 0.08, 1.33), 0.08, 0.08, r=0.018, n=1, up=(0, 0, 1),
                         color="wood", mat="wood", end_color="wood_light"))
    # straw skirt at the foot of the post
    for i, a in enumerate((-110, -40, 60, 160)):
        ar = math.radians(a)
        parts += F.straw_tuft(f"foot{i}", (math.cos(ar) * 0.05, 0.08 + math.sin(ar) * 0.05, 0.03),
                              (math.cos(ar), math.sin(ar), 0.25), count=3, length=0.13, radius=0.014, seed=i)
    # --- flannel shirt torso (plaid fabric), slightly chubby and bottom-heavy
    torso = F.lathe("torso", [(0.2, 0.84), (0.235, 0.9), (0.245, 1.02), (0.235, 1.18), (0.2, 1.3), (0.13, 1.38),
                              (0.08, 1.42)], segs=14, color="flannel_red", mat="fabric", cap_bottom=False,
                    cap_top=False, smooth=True, sharp=70)
    F.transform(torso, Matrix.Scale(0.85, 4, Vector((0, 1, 0))))
    parts.append(torso)
    # collar points
    for sx in (-1, 1):
        parts.append(F.blade(f"collar{sx}", [(sx * 0.03, -0.1, 1.405), (sx * 0.09, -0.135, 1.36)], [0.07, 0.0],
                             thick=0.008, color="flannel_dark", mat="fabric", up=(0, -1, 0.6)))
    # sleeves along the crossbar, flaring at the cuffs, with a quilted patch
    for sx in (-1, 1):
        parts.append(F.sweep(f"sleeve{sx}", [(sx * 0.13, 0.04, 1.3), (sx * 0.28, 0.05, 1.33), (sx * 0.44, 0.06, 1.335),
                                             (sx * 0.58, 0.06, 1.33)], [0.105, 0.09, 0.088, 0.1], sides=10,
                             color="flannel_red", mat="fabric", cap_start=False, cap_end=False, smooth=True))
        parts.append(F.sweep(f"cuff{sx}", [(sx * 0.565, 0.06, 1.33), (sx * 0.62, 0.06, 1.33)], [0.105, 0.11],
                             sides=10, color="flannel_dark", mat="fabric", cap_start=False, cap_end=True,
                             cap_offset=-0.3, end_color="straw_dark", end_mat="straw"))
        parts += F.straw_tuft(f"hand{sx}", (sx * 0.62, 0.06, 1.33), (sx, 0, -0.1), count=4, length=0.17, radius=0.017,
                              spread=0.9, seed=10 + sx, droop=0.5)
    patch = F.board("patch", (0.36, -0.035, 1.35), (0.36, -0.035, 1.43), 0.1, 0.012, r=0.01, n=1, up=(0, -1, 0.3),
                    color="quilt_blue", mat="felt")
    parts.append(patch)
    parts += stitches("pst", [(0.31, -0.045, 1.35), (0.41, -0.045, 1.35), (0.31, -0.06, 1.43), (0.41, -0.06, 1.43)],
                      size=0.012, color="trim_white")
    # --- denim overalls: hip barrel, bib with pocket, straps, gold buttons
    hips = F.lathe("hips", [(0.215, 0.72), (0.25, 0.78), (0.26, 0.9), (0.252, 1.0), (0.24, 1.04)], segs=14,
                   color="denim", mat="denim", cap_top=False, cap_bottom=True, bottom_color="denim", smooth=True,
                   sharp=60)
    F.transform(hips, Matrix.Scale(0.88, 4, Vector((0, 1, 0))))
    parts.append(hips)
    parts.append(F.board("bib", (0, -0.2, 0.98), (0, -0.2, 1.22), 0.27, 0.05, r=0.02, n=1, up=(0, -1, 0),
                         color="denim", mat="denim", caps=(False, True)))
    parts.append(F.board("pocket", (0, -0.229, 1.06), (0, -0.229, 1.14), 0.13, 0.012, r=0.008, n=1, up=(0, -1, 0),
                         color="denim", mat="denim"))
    parts += stitches("pk", [(-0.05, -0.237, 1.14), (0.0, -0.237, 1.14), (0.05, -0.237, 1.14)], size=0.008,
                      color="quilt_yellow")
    for sx in (-1, 1):
        parts.append(F.sweep(f"strap{sx}", [(sx * 0.1, -0.215, 1.21), (sx * 0.11, -0.19, 1.33), (sx * 0.1, -0.06, 1.41),
                                            (sx * 0.1, 0.1, 1.38), (sx * 0.1, 0.17, 1.2)], 0.026, sides=6, flatten=0.4,
                             color="denim", mat="denim", cap_start=False, cap_end=False))
        parts.append(K.cylinder(f"btn{sx}", 0.026, 0.02, color="foil_gold", mat="metal", verts=10, rot=(90, 0, 0),
                                loc=(sx * 0.1, -0.233, 1.2), bevel=0.006, segments=1))
        F.auto_smooth(parts[-1], 40)
    # --- dangling legs with a knee patch and straw ankles
    for sx in (-1, 1):
        a = (sx * 0.11, -0.03, 0.82)
        b = (sx * 0.15, -0.1, 0.55)
        c = (sx * 0.16, -0.07, 0.33)
        parts.append(F.sweep(f"leg{sx}", [a, b, c], [0.095, 0.085, 0.098], sides=10, color="denim", mat="denim",
                             cap_start=False, cap_end=True, cap_offset=-0.3, end_color="straw_dark", end_mat="straw"))
        parts.append(F.sweep(f"turnup{sx}", [(sx * 0.158, -0.074, 0.37), (sx * 0.161, -0.07, 0.33)], [0.104, 0.106],
                             sides=10, color="denim", mat="denim", cap_start=False, cap_end=False))
        parts += F.straw_tuft(f"ankle{sx}", (sx * 0.162, -0.07, 0.32), (sx * 0.15, -0.15, -1), count=4, length=0.13,
                              radius=0.016, spread=0.9, seed=20 + sx, droop=0.1)
    knee = F.board("knee", (-0.155, -0.18, 0.5), (-0.155, -0.195, 0.6), 0.1, 0.012, r=0.01, n=1, up=(0, -1, 0.15),
                   color="quilt_yellow", mat="felt")
    parts.append(knee)
    parts += stitches("kst", [(-0.2, -0.193, 0.5), (-0.11, -0.193, 0.5), (-0.2, -0.205, 0.6), (-0.11, -0.205, 0.6)],
                      size=0.01, color="charcoal")
    # --- burlap head on a gathered neck with a rope tie
    parts.append(F.lathe("gather", [(0.1, 1.4), (0.065, 1.47), (0.085, 1.5)], segs=12, color="cardboard",
                         mat="burlap", cap_top=False, cap_bottom=False, smooth=True))
    tie_pts = [(math.cos(2 * math.pi * i / 14) * 0.075, math.sin(2 * math.pi * i / 14) * 0.075, 1.47)
               for i in range(15)]
    parts.append(F.sweep("tie", tie_pts, 0.02, sides=4, color="rope", mat="flat", cap_start=False, cap_end=False,
                         twist=F.loop_twist(0.9, 14, 4), smooth=False))
    parts.append(F.sweep("tieend", [(0.02, -0.08, 1.46), (0.05, -0.1, 1.4), (0.045, -0.11, 1.35)], 0.014, sides=4,
                         color="rope", mat="flat", cap_offset=0.5, twist=0.8, smooth=False))
    head = K.sphere("head", 0.235, color="cardboard", mat="burlap", segments=16, rings=11, scale=(1.05, 0.95, 1.0),
                    loc=(0, 0, 1.68), project="sphere")
    # sack shape: slightly pear-shaped, gathered at the bottom, soft lumps
    K.deform(head, lambda co: Vector((co.x * (1.0 - 0.18 * max(0.0, -co.z / 0.235) ** 2),
                                      co.y * (1.0 - 0.18 * max(0.0, -co.z / 0.235) ** 2),
                                      co.z * (1.0 if co.z > 0 else 0.92))))
    F.bake(head)
    K.project(head, "sphere")
    F.scale_uv(head, 1.05)
    parts.append(head)
    fy = -0.222  # face plane

    def on_face(x, z, push=0.004):
        # point on the sack surface in front (sphere approx, scale y 0.95)
        r = 0.235
        dz = z - 1.68
        yy = -math.sqrt(max(0.0, r * r - (x / 1.05) ** 2 - dz * dz)) * 0.95
        return Vector((x, yy - push, z))

    for sx in (-1, 1):
        e = on_face(sx * 0.078, 1.7)
        parts.append(K.cylinder(f"eye{sx}", 0.042, 0.03, color="ink", mat="flat", verts=12, rot=(90, 0, sx * 12),
                                loc=e, bevel=0.01, segments=1))
        F.auto_smooth(parts[-1], 40)
        for hx, hz in ((-0.011, 0.0), (0.011, 0.0)):
            parts.append(K.cylinder(f"hole{sx}{hx}{hz}", 0.006, 0.01, color="charcoal", mat="flat", verts=5,
                                    rot=(90, 0, sx * 12), loc=e + Vector((hx, -0.016, hz))))
        g = on_face(sx * 0.078 + 0.016, 1.716, push=0.02)
        parts.append(K.sphere(f"glint{sx}", 0.009, color="eye_white", mat="flat", segments=6, rings=4, loc=g))
        ch = on_face(sx * 0.13, 1.61, push=0.0)
        parts.append(K.cylinder(f"cheek{sx}", 0.035, 0.012, color="cheek_pink", mat="felt", verts=10,
                                rot=(90, 0, sx * 28), loc=ch, scale=(1.0, 1.0, 0.8)))
    # stitched smile: thread arc with cross stitches
    smile = [on_face(-0.095 + 0.19 * i / 8, 1.6 - 0.04 * math.sin(math.pi * i / 8), push=0.006) for i in range(9)]
    parts.append(F.sweep("smile", smile, 0.009, sides=5, color="charcoal", mat="flat", cap_offset=0.6))
    parts += stitches("ss", [smile[i] for i in (1, 3, 5, 7)], size=0.016, color="charcoal",
                      normal_out=lambda p: (p - Vector((0, 0, 1.68))).normalized())
    # little patched-on nose (pumpkin felt triangle)
    nose = F.sweep("nose", [on_face(0, 1.65, push=-0.01), on_face(0, 1.65, push=0.05)], [0.03, 0.006], sides=6,
                   color="pumpkin", mat="felt", cap_start=False, cap_end=True, cap_offset=0.5)
    parts.append(nose)
    # --- floppy straw hat: wavy brim, soft crown, purple band with a sunflower
    hat_c = Vector((0, 0.02, 1.84))
    brim = F.lathe("brim", [(0.13, 0.0), (0.24, -0.005), (0.33, -0.03), (0.37, -0.05), (0.375, -0.065),
                            (0.35, -0.055), (0.24, -0.03), (0.13, -0.02)], segs=20, color="straw", mat="straw",
                   cap_top=False, cap_bottom=False, smooth=True, sharp=70, uv="around")

    def flop(co):
        a = math.atan2(co.y, co.x)
        r = math.hypot(co.x, co.y)
        t = max(0.0, (r - 0.14) / 0.23)
        return Vector((co.x, co.y, co.z + 0.045 * math.sin(3 * a + 0.6) * t - 0.03 * t * t))

    K.deform(brim, flop)
    crown = F.lathe("crown", [(0.155, -0.01), (0.165, 0.06), (0.15, 0.13), (0.11, 0.17), (0.04, 0.185), (0.0, 0.18)],
                    segs=16, color="straw", mat="straw", cap_top=False, cap_bottom=False, smooth=True, sharp=70,
                    uv="around")
    K.deform(crown, lambda co: Vector((co.x, co.y, co.z - 0.025 * math.exp(-((co.x - 0.0) ** 2 + co.y ** 2) / 0.004))))
    band = F.lathe("band", [(0.163, 0.0), (0.169, 0.03), (0.166, 0.055)], segs=16, color="banner_purple", mat="fabric",
                   cap_top=False, cap_bottom=False, smooth=True)
    hat = K.merge([brim, crown, band], "hat")
    F.transform(hat, Matrix.Translation(hat_c) @ Matrix.Rotation(math.radians(-12), 4, "X") @
                Matrix.Rotation(math.radians(5), 4, "Y"))
    parts.append(hat)
    # sunflower tucked in the band
    fl_c = hat_c + Vector((0.12, -0.12, 0.05))
    petals = []
    for i in range(10):
        a = 2 * math.pi * i / 10
        d = Vector((math.cos(a), 0, math.sin(a)))
        petals.append(F.blade(f"pet{i}", [fl_c + d * 0.015, fl_c + d * 0.05 + Vector((0, -0.006, 0)),
                                          fl_c + d * 0.072 + Vector((0, -0.004, 0))], [0.02, 0.028, 0.0], thick=0.005,
                              color="sunflower_yellow", mat="flat", up=(0, -1, 0)))
    flower = K.merge(petals + [K.sphere("fc", 0.026, color="sunflower_center", mat="flat", segments=8, rings=5,
                                        loc=fl_c + Vector((0, -0.01, 0)), scale=(1, 0.6, 1))], "flower")
    F.transform(flower, Matrix.Translation(fl_c) @ Matrix.Rotation(math.radians(-30), 4, "Z") @ Matrix.Translation(-fl_c))
    parts.append(flower)
    # straw hair poking out under the brim
    for i, (x, y, d) in enumerate(((-0.17, -0.1, (-0.7, -0.6, -0.5)), (0.18, -0.08, (0.7, -0.5, -0.5)),
                                   (-0.2, 0.08, (-1, 0.2, -0.4)), (0.2, 0.1, (1, 0.3, -0.4)))):
        parts += F.straw_tuft(f"hair{i}", (x, y, 1.8), d, count=3, length=0.12, radius=0.015, seed=40 + i,
                              spread=0.8, droop=0.4)
    ob = K.merge(parts, "scarecrow")
    return F.finish([ob], "props/scarecrow", "scarecrow")


if __name__ == "__main__":
    F.run({f.__name__: f for f in (hay_wagon, scarecrow)})
