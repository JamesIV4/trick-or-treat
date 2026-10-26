"""The ten monster nests (batch03): destructible spawners, one merged mesh each, origin at the
ground centre, front -Y (glTF +Z). Each carries a glowing element in its monster's colour.

blender --background --factory-startup --python art/team/monsters/batch03/build_nests.py -- [keys] [--nosheet] [--pieces]
keys: pumpkin bat skeleton ghost gremlin mummy slime scarecrow spider toadstool
"""
import math
import pathlib
import sys

sys.path.insert(0, str(pathlib.Path(__file__).resolve().parent))
import bmesh  # noqa: E402
import mon_kit as M  # noqa: E402
from mon_kit import D, F, K, V, Matrix  # noqa: E402

TAU = 2 * math.pi
GLOW, HOT = "glow_jack", "glow_jack_hot"


def pieces(parts):
    if "--pieces" in M.FLAGS:
        print("   pieces", ", ".join(f"{o.name}:{D.tris(o)}" for o in parts))


def done(parts, aid, **kw):
    pieces(parts)
    return M.nest_finish(parts, aid, **kw)


def leaf_on_ground(name, x, y, yaw, size=0.24, z=0.01, color="stem_green"):
    lf = F.pumpkin_leaf(name, size=size, n=20, color=color, mat="leaves", cup=0.18)
    F.place(lf, (x, y, z), (0, 0, yaw))
    return lf


# ====================================================================== CRACKED PUMPKIN
def crack_poly(cx, z0, z1, steps=6, amp=0.11, w0=0.07, w1=0.17, seed=3):
    rr = M.rnd(seed)
    cl = []
    for i in range(steps + 1):
        t = i / steps
        x = cx + (amp if i % 2 else -amp) * (0.4 + 0.6 * math.sin(math.pi * t)) + rr.uniform(-0.02, 0.02)
        w = w0 + (w1 - w0) * math.sin(math.pi * t)
        cl.append((x, z0 + (z1 - z0) * t, w))
    right = [(x + w / 2, z) for x, z, w in cl[1:-1]]
    left = [(x - w / 2, z) for x, z, w in reversed(cl[1:-1])]
    poly = [(cl[0][0], cl[0][1] - 0.03)] + right + [(cl[-1][0], cl[-1][1] + 0.03)] + left
    return F.ccw(poly)


def nest_pumpkin():
    K.reset()
    body = F.pumpkin_body("nbody", width=2.3, height=1.5, lobes=10, spl=4, rings=13, groove=0.11, plump=0.5,
                          fullness=0.78, flatness=0.72, top_dimple=0.12, dimple_w=0.4, lumps=0.05, seed=11,
                          belly=1.08, lopsided=0.04, lop_dir=1.0, color="pumpkin_dark", rot_z=-90.0)
    top = V(body["top"])
    poly = crack_poly(0.05, 0.22, 1.3)
    poly2 = crack_poly(-0.62, 0.35, 0.95, steps=4, amp=0.07, w0=0.06, w1=0.12, seed=8)
    feats = [{"shape": (lambda g, p=poly: F.offset_poly(p, -g)), "color": HOT, "wall": GLOW, "inset": 0.012,
              "rim_w": 0.008},
             {"shape": (lambda g, p=poly2: F.offset_poly(p, -g)), "color": HOT, "wall": GLOW, "inset": 0.01,
              "rim_w": 0.006}]
    F.carve(body, feats, depth=0.12, inset=0.012)
    parts = [body]
    # broken stem stump and light leaking round the cracked lid
    parts.append(F.stem("stump", tuple(top), height=0.2, radius=0.11, color="stem_brown", mat="bark", curl=0.05,
                        lean_dir=math.radians(60), rings=5, ridges=5, flare=1.5, hook=0.0))
    parts.append(M.seam("lid", body, top, 0.42, n=26, zig=0.05, color=GLOW, tube_r=0.03, lift=0.006))
    # sprawling vines with leaves, curls and three baby pumpkins
    rr = M.rnd(5)
    for k, a in enumerate((-60, 30, 135, 215)):
        ar = math.radians(a)
        d = V((math.cos(ar), math.sin(ar), 0))
        sd = V((-d.y, d.x, 0))
        pts = [d * 1.0 + V((0, 0, 0.1)), d * 1.35 + sd * 0.12 + V((0, 0, 0.04)), d * 1.7 - sd * 0.08 + V((0, 0, 0.04)),
               d * 2.0 + sd * 0.1 + V((0, 0, 0.05))]
        parts.append(M.tube(f"vine{k}", M.catmull(pts, 2), lambda t: 0.075 * (1 - 0.45 * t), sides=6, color="vine",
                            mat="leaves", cap_offset=0.6))
        lp = d * 1.5 + sd * 0.22
        parts.append(leaf_on_ground(f"leaf{k}", lp.x, lp.y, a + 40, size=0.34, z=0.05))
        if k != 1:
            bp = d * 2.05 + sd * 0.12
            baby = F.pumpkin_body(f"baby{k}", width=0.36, height=0.27, lobes=6, spl=3, rings=7, groove=0.1, plump=0.5,
                                  seed=20 + k, color="pumpkin", rot_z=math.degrees(ar) + 90)
            baby.location = (bp.x, bp.y, 0.0)
            parts.append(baby)
            # two tiny glowing eyes so the babies read as hatchlings
            for s in (-1, 1):
                e = M.blob(f"be{k}{s}", (0, 0, 0), (0.028, 0.034, 0.012), segs=6, rings=3, color=HOT)
                out = (bp - V((0, 0, 0))).normalized()
                ex = bp - out * 0.0 + V((0, 0, 0.15))
                fwd = V((0, -1, 0)) if abs(out.y) < 0.3 else -out
                side = fwd.cross(V((0, 0, 1))).normalized()
                M.put(e, bp + V((0, 0, 0.15)) + fwd * 0.165 + side * s * 0.055, fwd)
                parts.append(e)
        if k == 1:
            parts.append(F.tendril(f"curl{k}", tuple(d * 2.0 + sd * 0.1 + V((0, 0, 0.06))), turns=1.5, r0=0.08, r1=0.03,
                                   rise=0.12, n=11, radius=0.016, color="vine"))
    return done(parts, "nest_pumpkin", ao_radius=0.9)


# ====================================================================== BAT BELFRY
def mini_bat(name, loc, scale=1.0, yaw=0.0):
    """A little bat hanging upside down, wings folded (cloak)."""
    s = scale
    body = M.blob(name + "_b", (0, 0, 0), (0.09 * s, 0.08 * s, 0.12 * s), segs=8, rings=5, color="bat_fur", mat="felt")
    cloak = M.blob(name + "_w", (0, 0.01 * s, 0.01 * s), (0.11 * s, 0.09 * s, 0.11 * s), segs=8, rings=5,
                   color="bat_wing", mat="felt", warp=lambda c: V((c.x, c.y if c.y > -0.2 else -0.2 + (c.y + 0.2) * 0.3, c.z)))
    parts = [body, cloak]
    for k in (-1, 1):
        ear = M.cone(name + f"_e{k}", 0.06 * s, 0.03 * s, segs=5, color="bat_fur", mat="felt", lo=True)
        M.put(ear, (k * 0.04 * s, 0, -0.1 * s), (k * 0.4, 0, -1), up=(0, -1, 0))
        eye = M.blob(name + f"_i{k}", (k * 0.032 * s, -0.075 * s, -0.04 * s), (0.017 * s, 0.01 * s, 0.012 * s),
                     segs=6, rings=3, color="bat_belly")
        parts += [ear, eye]
    feet = M.tube(name + "_f", [V((0, 0, 0.1 * s)), V((0, 0, 0.17 * s))], 0.012 * s, sides=4, color="bat_fur")
    parts.append(feet)
    ob = K.merge(parts, name)
    M.xform(ob, Matrix.Translation(V(loc)) @ Matrix.Rotation(math.radians(yaw), 4, "Z"))
    return ob


def nest_bat():
    K.reset()
    parts = []
    parts.append(F.rbox("base", (1.1, 1.1, 0.26), loc=(0, 0, 0.13), bevel=0.06, segments=2, color="stone", mat="stone"))
    parts.append(F.rbox("base_top", (0.84, 0.84, 0.1), loc=(0, 0, 0.3), bevel=0.04, segments=2, color="stone_dark",
                        mat="stone", top_color="moss"))
    # crooked post with braces
    lean = Matrix.Rotation(math.radians(4), 4, "Y") @ Matrix.Rotation(math.radians(-3), 4, "X")
    tower = []
    tower.append(F.board("post", (0, 0, 0.32), (0, 0, 1.86), 0.24, 0.24, r=0.04, n=1, up=(0, 1, 0), color="wood_dark",
                         mat="wood"))
    for s in (-1, 1):
        tower.append(F.board(f"brace{s}", (s * 0.06, 0, 1.25), (s * 0.4, 0, 1.78), 0.08, 0.08, r=0.02, up=(0, 1, 0),
                             color="wood_dark", mat="wood"))
    # the belfry: plank box with glowing arched openings on every side and a bell inside
    tower.append(F.rbox("floor", (0.98, 0.98, 0.1), loc=(0, 0, 1.86), bevel=0.03, segments=1, color="wood", mat="planks"))
    tower.append(F.rbox("house", (0.84, 0.84, 0.62), loc=(0, 0, 2.22), bevel=0.03, segments=1, color="wood_dark",
                        mat="planks"))
    arch = [(-0.17, -0.2)] + [(math.cos(math.pi * i / 6) * 0.17 * -1, 0.06 + math.sin(math.pi * i / 6) * 0.17)
                              for i in range(7)] + [(0.17, -0.2)]
    arch = [(x, z) for x, z in arch]
    for k in range(4):
        rz = k * 90
        g = M.plate(f"win{k}", arch, 0.02, "glow_purple", bevel=0.0)
        trim = M.plate(f"wtrim{k}", [(x * 1.25, (z + 0.2) * 1.15 - 0.215) for x, z in arch], 0.016, "wood", mat="wood",
                       bevel=0.004)
        for ob, off in ((trim, 0.426), (g, 0.432)):
            M.xform(ob, Matrix.Rotation(math.radians(rz), 4, "Z") @ Matrix.Translation((0, -off, 2.2)))
        tower += [trim, g]
    bell = M.lathe("bell", [(0.0, -0.12), (0.12, -0.12), (0.13, -0.1), (0.1, -0.05), (0.075, 0.04), (0.06, 0.09),
                            (0.0, 0.11)], 12, color="foil_gold", mat="metal")
    bell.location = (0, -0.43, 2.2)
    tower.append(bell)
    # steep plum shingle roof with overhanging eaves, finial and a bat weathervane
    roof = M.lathe("roof", [(0.0, -0.02), (0.74, -0.02), (0.76, 0.03), (0.52, 0.28), (0.28, 0.6), (0.12, 0.86),
                            (0.0, 1.0)], 4, color="roof_plum", mat="shingles", project="box",
                   warp=lambda c: V((c.x, c.y, c.z + 0.04 * math.hypot(c.x, c.y))))
    M.xform(roof, Matrix.Translation((0, 0, 2.52)) @ Matrix.Rotation(math.radians(45), 4, "Z"))
    tower.append(roof)
    tower.append(K.box("trim_eave", (1.05, 1.05, 0.05), color="wood", loc=(0, 0, 2.54), bevel=0.015, mat="wood"))
    tower.append(M.tube("finial", [V((0, 0, 3.46)), V((0, 0, 3.66))], 0.025, sides=6, color="iron", mat="metal"))
    tower.append(M.blob("knob", (0, 0, 3.66), (0.05, 0.05, 0.05), segs=8, rings=5, color="foil_gold", mat="metal"))
    vane = [(-0.02, 0.0), (0.06, 0.04), (0.12, 0.0), (0.2, 0.06), (0.17, -0.04), (0.1, -0.06), (0.04, -0.02),
            (0.0, -0.05), (-0.04, -0.02), (-0.1, -0.06), (-0.17, -0.04), (-0.2, 0.06), (-0.12, 0.0), (-0.06, 0.04)]
    v = M.plate("vane", vane, 0.02, "lamp_black", mat="metal", bevel=0.0)
    M.xform(v, Matrix.Translation((0, 0, 3.6)) @ Matrix.Rotation(math.radians(20), 4, "Z"))
    tower.append(v)
    # sleeping bats hanging under the eaves
    for k, (x, y, yaw) in enumerate(((0.38, -0.5, 0), (-0.25, -0.5, 10), (0.5, 0.2, 90))):
        tower.append(mini_bat(f"mb{k}", (x, y, 2.33), 1.0 - 0.1 * k, yaw))
    for ob in tower:
        M.bake(ob)
        M.xform(ob, Matrix.Translation((0, 0, 0.32)) @ lean @ Matrix.Translation((0, 0, -0.32)))
    parts += tower
    return done(parts, "nest_bat", ao_radius=0.7, weighted=False)


# ====================================================================== CROOKED GRAVES
def headstone(name, w, h, th, top="arch"):
    if top == "arch":
        out = [(-w / 2, 0.0), (w / 2, 0.0), (w / 2, h - w / 2)]
        out += [(math.cos(math.pi * i / 8) * w / 2, h - w / 2 + math.sin(math.pi * i / 8) * w / 2) for i in range(1, 8)]
        out += [(-w / 2, h - w / 2)]
    else:  # cross
        a, b = w * 0.18, w * 0.5
        out = [(-a, 0.0), (a, 0.0), (a, h * 0.62), (b, h * 0.62), (b, h * 0.78), (a, h * 0.78), (a, h),
               (-a, h), (-a, h * 0.78), (-b, h * 0.78), (-b, h * 0.62), (-a, h * 0.62)]
    ob = M.plate(name, out, th, "stone_light", mat="stone", bevel=0.03, segments=2, center=(0, 0, h / 2))
    K.paint(ob, "moss", where=lambda c, n: n.z > 0.55 and c.z > h * 0.5, mat="grass")
    return ob


def nest_skeleton():
    K.reset()
    parts = []
    mound = M.blob("mound", (0, 0, -0.05), (1.15, 0.85, 0.33), segs=20, rings=9, color="grave_soil", mat="soil",
                   warp=lambda c: V((c.x, c.y, c.z + 0.04 * math.sin(c.x * 9) * math.cos(c.y * 7))))
    parts.append(mound)
    tree = M.bvh(mound)
    # the open grave: a glowing teal pit in front of the tallest stone
    pit_c, pit_r = V((0.05, -0.18, 0.0)), V((0.38, 0.22, 0.0))
    p, n = M.surf(tree, (pit_c.x, pit_c.y, 2.0), (0, 0, -1))
    rim = K.torus("pit_rim", 1.0, 0.07, color="grave_soil", mat="soil", major_seg=16, minor_seg=4,
                  loc=(pit_c.x, pit_c.y, p.z + 0.02), scale=(pit_r.x, pit_r.y, 1.0))
    rim.scale = (pit_r.x, pit_r.y, 0.7)
    pit = M.blob("pit", (pit_c.x, pit_c.y, p.z - 0.02), (pit_r.x * 0.97, pit_r.y * 0.95, 0.05), segs=16, rings=4,
                 color="glow_magic_teal")
    parts += [rim, pit]
    # crooked headstones
    specs = [("hs0", -0.68, 0.18, 0.5, 0.8, "arch", -9, 6), ("hs1", 0.05, 0.28, 0.62, 1.15, "arch", 4, -4),
             ("hs2", 0.72, 0.12, 0.62, 0.95, "cross", 12, 8)]
    for name, x, y, w, h, top, tilt, yaw in specs:
        hs = headstone(name, w, h, 0.16, top)
        gz = M.surf(tree, (x, y, 2.0), (0, 0, -1))[0].z - 0.12
        M.xform(hs, Matrix.Translation((x, y, gz)) @ Matrix.Rotation(math.radians(yaw), 4, "Z") @
                Matrix.Rotation(math.radians(tilt), 4, "Y") @ Matrix.Rotation(math.radians(-5), 4, "X"))
        parts.append(hs)
        # glowing teal rune on the face: a little skull for the middle stone, a moon / star otherwise
        if name == "hs1":
            sk = M.blob("rune_skull", (0, 0, 0), (0.11, 0.025, 0.1), segs=10, rings=4, color="glow_magic_teal")
            jaw = M.blob("rune_jaw", (0, 0, -0.085), (0.065, 0.022, 0.05), segs=8, rings=3, color="glow_magic_teal")
            eyes = [M.blob(f"rune_e{s}", (s * 0.042, -0.02, 0.0), (0.03, 0.012, 0.032), segs=6, rings=3, color="ink")
                    for s in (-1, 1)]
            rune = K.merge([sk, jaw] + eyes, "rune1")
            M.xform(rune, Matrix.Translation((0, -0.08, h * 0.6)))
        elif top == "arch":
            pts = [(math.cos(a) * 0.1, math.sin(a) * 0.1) for a in [math.radians(60 + 240 * i / 10) for i in range(11)]]
            pts += [(math.cos(a) * 0.065 + 0.03, math.sin(a) * 0.065) for a in [math.radians(300 - 240 * i / 10) for i in range(1, 10)]]
            rune = M.plate("rune0", pts, 0.02, "glow_magic_teal", bevel=0.0)
            M.xform(rune, Matrix.Translation((0, -0.08, h * 0.58)))
        else:
            rune = M.plate("rune2", [(-0.035, 0), (0.035, 0), (0.035, 0.11), (0.09, 0.11), (0.09, 0.17), (0.035, 0.17),
                                     (0.035, 0.23), (-0.035, 0.23), (-0.035, 0.17), (-0.09, 0.17), (-0.09, 0.11),
                                     (-0.035, 0.11)], 0.02, "glow_magic_teal", bevel=0.0)
            M.xform(rune, Matrix.Translation((0, -0.08, h * 0.3)))
        M.xform(rune, Matrix.Translation((x, y, gz)) @ Matrix.Rotation(math.radians(yaw), 4, "Z") @
                Matrix.Rotation(math.radians(tilt), 4, "Y") @ Matrix.Rotation(math.radians(-5), 4, "X"))
        parts.append(rune)
    # a bony hand waving out of the pit, scattered bones and a skull
    wrist = V((0.3, -0.24, 0.12))
    parts.append(M.tube("forearm", [wrist - V((0.02, 0.0, 0.2)), wrist], 0.035, sides=6, color="bone", mat="concrete"))
    parts.append(M.blob("palm", wrist + V((0, 0, 0.05)), (0.06, 0.035, 0.06), segs=8, rings=5, color="bone", mat="concrete"))
    for k, a in enumerate((-35, -12, 10, 32)):
        d = V((math.sin(math.radians(a)), 0, math.cos(math.radians(a))))
        parts.append(M.tube(f"finger{k}", [wrist + V((0, 0, 0.07)) + d * 0.02, wrist + V((0, 0, 0.07)) + d * 0.11],
                            0.016, sides=5, color="bone", mat="concrete", cap_offset=0.8))
    parts.append(M.bone("bone0", (-0.5, -0.55, 0.15), (-0.18, -0.68, 0.1), 0.035, color="bone", mat="concrete",
                        sides=6, knob_segs=7, knob_rings=4))
    parts.append(M.bone("bone1", (0.55, -0.5, 0.12), (0.8, -0.3, 0.16), 0.03, color="bone", mat="concrete", sides=6,
                        knob_segs=7, knob_rings=4))
    skull = M.blob("skull", (-0.32, -0.5, 0.2), (0.13, 0.12, 0.11), segs=12, rings=7, color="bone", mat="concrete")
    parts.append(skull)
    for s in (-1, 1):
        parts.append(M.blob(f"sock{s}", (-0.32 + s * 0.045, -0.6, 0.215), (0.035, 0.02, 0.04), segs=6, rings=3,
                            color="ink"))
    # a little teal candle on the left stone
    parts.append(M.lathe("candle", [(0.0, 0.0), (0.04, 0.0), (0.04, 0.12), (0.0, 0.12)], 8, color="cream", mat="flat",
                         loc=(-0.95, -0.25, 0.12)))
    parts.append(M.blob("flame", (-0.95, -0.25, 0.28), (0.03, 0.03, 0.05), segs=6, rings=4, color="glow_magic_teal"))
    return done(parts, "nest_skeleton", ao_radius=0.7)


# ====================================================================== HAUNTED LANTERN
def nest_ghost():
    K.reset()
    parts = []
    parts.append(M.lathe("plinth", [(0.0, 0.0), (0.55, 0.0), (0.56, 0.1), (0.5, 0.18), (0.38, 0.22), (0.36, 0.3),
                                    (0.3, 0.34), (0.0, 0.34)], 8, color="stone", mat="stone", project="box"))
    K.paint(parts[-1], "moss", where=lambda c, n: n.z > 0.6 and c.z > 0.15, mat="grass")
    # iron post with a ring collar and decorative scrolls
    parts.append(M.lathe("post", [(0.0, 0.3), (0.13, 0.3), (0.11, 0.42), (0.075, 0.5), (0.065, 1.4), (0.06, 2.0),
                                  (0.1, 2.06), (0.1, 2.12), (0.0, 2.12)], 10, color="lamp_black", mat="metal"))
    for z in (0.62, 1.55):
        parts.append(K.torus(f"collar{z}", 0.085, 0.022, color="iron", mat="metal", major_seg=10, minor_seg=4,
                             loc=(0, 0, z)))
    for s in (-1, 1):
        pts = [V((s * 0.06, 0, 1.75)), V((s * 0.2, 0, 1.85)), V((s * 0.28, 0, 2.02)), V((s * 0.24, 0, 2.14)),
               V((s * 0.17, 0, 2.1)), V((s * 0.18, 0, 2.03))]
        parts.append(M.tube(f"scroll{s}", M.catmull(pts, 2), 0.022, sides=5, color="lamp_black", mat="metal",
                            cap_offset=0.5))
    # the lantern: iron cage with ghost-purple glass and a little ghost face in the flame
    Z0, Z1, HW = 2.14, 2.86, 0.3
    parts.append(F.rbox("lbase", (0.72, 0.72, 0.08), loc=(0, 0, Z0 + 0.04), bevel=0.02, segments=1, color="iron",
                        mat="metal"))
    glass = F.rbox("glass", (2 * HW - 0.02, 2 * HW - 0.02, Z1 - Z0 - 0.06), loc=(0, 0, (Z0 + Z1) / 2 + 0.01),
                   bevel=0.02, segments=1, color="glow_purple", mat="flat")
    parts.append(glass)
    for sx in (-1, 1):
        for sy in (-1, 1):
            parts.append(F.board(f"bar{sx}{sy}", (sx * HW, sy * HW, Z0 + 0.06), (sx * HW, sy * HW, Z1), 0.05, 0.05, r=0.012,
                                 up=(0, 1, 0), color="lamp_black", mat="metal"))
    for k in range(4):
        rz = math.radians(90 * k)
        d = V((math.sin(rz), -math.cos(rz), 0))
        sd = V((math.cos(rz), math.sin(rz), 0))
        parts.append(F.board(f"mid{k}", d * HW + V((0, 0, Z0 + 0.06)) , d * HW + V((0, 0, Z1)), 0.022, 0.022, r=0.006,
                             up=(0, 0, 1) if False else tuple(sd), color="lamp_black", mat="metal"))
    # ghost face on the front glass
    for s in (-1, 1):
        parts.append(M.blob(f"geye{s}", (s * 0.1, -HW - 0.01, Z0 + 0.45), (0.065, 0.015, 0.09), segs=8, rings=4,
                            color="ink"))
    parts.append(M.blob("gmouth", (0.0, -HW - 0.01, Z0 + 0.26), (0.055, 0.015, 0.07), segs=8, rings=4, color="ink"))
    for s in (-1, 1):
        parts.append(M.blob(f"gcheek{s}", (s * 0.15, -HW - 0.004, Z0 + 0.34), (0.035, 0.008, 0.02), segs=6, rings=3,
                            color="candy_pink"))
    # pointed cap and hanging ring
    parts.append(M.lathe("cap", [(0.0, -0.02), (0.44, -0.02), (0.45, 0.02), (0.28, 0.16), (0.12, 0.32), (0.0, 0.42)], 4,
                         color="lamp_black", mat="metal", project="box", loc=(0, 0, Z1)))
    M.xform(parts[-1], Matrix.Translation((0, 0, Z1)) @ Matrix.Rotation(math.radians(45), 4, "Z") @ Matrix.Translation((0, 0, -Z1)))
    parts.append(K.torus("ring", 0.07, 0.016, color="iron", mat="metal", major_seg=10, minor_seg=4,
                         loc=(0, 0, Z1 + 0.48), rot=(90, 0, 0)))
    # two little ghost wisps drifting round the lantern
    for k, (a, z, s) in enumerate(((210, 2.0, 1.7), (40, 2.85, 1.4))):
        ar = math.radians(a)
        c = V((math.cos(ar) * 0.7, math.sin(ar) * 0.7, z))
        wisp = M.lathe(f"wisp{k}", [(0.0, -0.18), (0.05, -0.12), (0.09, -0.02), (0.1, 0.06), (0.07, 0.13), (0.0, 0.16)],
                       10, color="ghost_white", mat="cloud", project="sphere",
                       warp=lambda q: V((q.x + 0.25 * max(0.0, -q.z) ** 1.5, q.y, q.z)))
        M.xform(wisp, Matrix.Translation(c) @ Matrix.Rotation(ar + math.pi / 2, 4, "Z") @ Matrix.Scale(s, 4))
        parts.append(wisp)
        tdir = V((-math.sin(ar), math.cos(ar), 0))
        out = V((math.cos(ar), math.sin(ar), 0))
        for e in (-1, 1):
            parts.append(M.blob(f"we{k}{e}", c + out * 0.085 * s + tdir * 0.0 + V((0, 0, 0.05 * s)) + V((-out.y, out.x, 0)) * e * 0.035 * s,
                                (0.014 * s, 0.014 * s, 0.022 * s), segs=6, rings=3, color="ink"))
    return done(parts, "nest_ghost", ao_radius=0.6)


# ====================================================================== CANDY-CORN HIVE
CC = ("candy_corn_yellow", "pumpkin", "candy_corn_white")  # the gremlin's own band colours


def kernel(name, size=0.18, segs=6):
    """A normal candy-corn kernel: tip up local Z, base at z=0, flattened along local Y."""
    h, r = size, size * 0.42
    prof = [(0.0, 0.0, 0), (r, h * 0.1, 0), (r * 0.88, h * 0.4, 1), (r * 0.6, h * 0.72, 2), (r * 0.26, h * 0.93, 2),
            (0.0, h, 2)]
    return M.band_lathe(name, prof, segs, colors=CC, mats=("vinyl",), project="cyl",
                        warp=lambda c: V((c.x, c.y * 0.62, c.z)))


def lay_kernel(ob, size, x, y, yaw, roll=0.0, z=0.0):
    """Kernel lying on its flat side, centred on (x, y), tip pointing along yaw."""
    M.xform(ob, Matrix.Translation((x, y, z + size * 0.42 * 0.62 * 0.8)) @ Matrix.Rotation(math.radians(yaw), 4, "Z") @
            Matrix.Rotation(math.radians(roll), 4, "Y") @ Matrix.Rotation(math.radians(90), 4, "X") @
            Matrix.Translation((0, 0, -size * 0.5)))
    return ob


def lean_kernel(ob, x, y, out_deg, tilt, z=-0.015):
    """Kernel standing at (x, y), flat face toward out_deg, tip tilted `tilt` degrees back toward the centre."""
    M.xform(ob, Matrix.Translation((x, y, z)) @ Matrix.Rotation(math.radians(out_deg - 90), 4, "Z") @
            Matrix.Rotation(math.radians(tilt), 4, "X"))
    return ob


def zig_crack(x0, z0, x1, z1, steps=5, amp=0.035, w0=0.05, w1=0.022):
    """Narrow zig-zag strip polygon from (x0, z0) to (x1, z1), tapering w0 -> w1 (x, z outline)."""
    d = V((x1 - x0, z1 - z0))
    t = d.normalized()
    nrm = V((-t.y, t.x))
    cl = []
    for i in range(steps + 1):
        s = i / steps
        off = (amp if i % 2 else -amp) * (0.0 if i in (0, steps) else 1.0)
        cl.append((V((x0, z0)) + d * s + nrm * off, w0 + (w1 - w0) * s))
    left = [tuple(p + nrm * w / 2) for p, w in cl]
    right = [tuple(p - nrm * w / 2) for p, w in reversed(cl)]
    return F.ccw(left + right)


def nest_gremlin():
    """Candy-Corn Hive: a giant squat glossy candy corn (yellow base, orange middle, white tip with the
    gremlin's curl) cracked open at the front into a dark burrow where glowing eyes and fangs peek out,
    sitting in a pile of normal candy-corn kernels and broken shell pieces."""
    K.reset()
    parts = []
    H, R, FLAT, LEAN = 1.42, 0.8, 0.86, 0.07
    # (r / R, z / H, band): yellow up to 0.40, orange to 0.70, white tip; crisp ring at each boundary
    prof = [(0.0, -0.03, 0), (0.8, -0.03, 0), (0.95, 0.035, 0), (1.0, 0.09, 0), (1.0, 0.16, 0), (0.97, 0.24, 0),
            (0.92, 0.32, 0), (0.86, 0.40, 1), (0.78, 0.48, 1), (0.69, 0.56, 1), (0.60, 0.63, 1), (0.50, 0.70, 2),
            (0.40, 0.77, 2), (0.30, 0.84, 2), (0.20, 0.91, 2), (0.11, 0.96, 2), (0.04, 0.99, 2), (0.0, 1.0, 2)]
    body = M.band_lathe("hive", [(r * R, z * H, b) for r, z, b in prof], 22, colors=CC, mats=("vinyl",),
                        project="cyl", warp=lambda c: V((c.x, c.y * FLAT + LEAN * max(c.z, 0.0) ** 2 / H, c.z)))
    # the cracked-open front: a jagged broken-shell opening (white shell edge, dark burrow), plus two cracks
    # (carve samples the hull only at outline points: the long bottom edge needs in-between points or the
    # cutter misses the hull's bulge and leaves the lower middle uncut)
    hole = [(-0.3, 0.07), (-0.15, 0.065), (0.0, 0.06), (0.15, 0.065), (0.3, 0.07), (0.38, 0.15), (0.42, 0.3), (0.42, 0.48), (0.37, 0.6), (0.31, 0.75),
            (0.23, 0.64), (0.15, 0.85), (0.06, 0.72), (-0.02, 0.92), (-0.1, 0.75), (-0.18, 0.84), (-0.25, 0.67),
            (-0.33, 0.73), (-0.38, 0.58), (-0.42, 0.44), (-0.41, 0.28), (-0.37, 0.15)]
    hole = F.ccw(hole)
    cracks = [zig_crack(-0.02, 0.97, 0.1, 1.17, steps=4), zig_crack(0.47, 0.36, 0.6, 0.46, steps=3),
              zig_crack(-0.46, 0.56, -0.54, 0.72, steps=3)]
    feats = [{"shape": (lambda g, p=hole: F.offset_poly(p, -g)), "rim": "candy_corn_white", "wall": "choco_dark",
              "color": "ink", "depth": 0.42, "inset": 0.03, "rim_w": 0.02, "rim_frac": 0.3}]
    for c in cracks:
        feats.append({"shape": (lambda g, p=c: F.offset_poly(p, -g)), "rim": "candy_corn_white", "wall": "choco_dark",
                      "color": "ink", "depth": 0.05, "inset": 0.006, "rim_w": 0.004, "rim_frac": 0.4})
    F.carve(body, feats)
    parts.append(body)
    tree = M.bvh(body)

    def floor(x, z):
        p, n = M.surf(tree, (x, -3.0, z), (0, 1, 0))
        return p

    # a gremlin face peeking out of the dark, hanging part-way in so the top lip never hides it: a big
    # mischievous pair of glowing eyes with two fangs, and a shy little pair low on the right
    for k, (x, z, s, col, fwd) in enumerate(((-0.15, 0.47, 1.0, HOT, 0.2), (0.15, 0.47, 1.0, HOT, 0.2),
                                             (0.21, 0.19, 0.6, GLOW, 0.12), (0.32, 0.21, 0.6, GLOW, 0.12))):
        side = -1 if k in (0, 2) else 1
        q = floor(x, z) - V((0, fwd, 0))
        parts.append(M.blob(f"peye{k}", q, (0.1 * s, 0.035, 0.11 * s), segs=8, rings=4, color=col,
                            warp=lambda c, sd=side: V((c.x, c.y, min(c.z, 0.55 + 0.35 * c.x * sd)))))
    for s in (-1, 1):
        q = floor(s * 0.065, 0.33) - V((0, 0.2, 0))
        fang = M.cone(f"fang{s}", 0.09, 0.032, segs=5, color="eye_white", lo=True)
        M.put(fang, q, (0, -0.15, -1), up=(0, -1, 0))
        parts.append(fang)
    # the gremlin's signature curl on the white tip
    tip = V((0, LEAN * H, H))
    parts.append(M.tube("curl", [tip - V((0, 0, 0.06)), tip + V((0, 0, 0.07)), tip + V((0.07, 0.0, 0.15)),
                                 tip + V((0.15, 0.0, 0.13))], lambda t: 0.07 * (1 - 0.45 * t), sides=8,
                        color="candy_corn_white", mat="vinyl", cap_offset=0.9))
    # the hoard: heaps of normal candy-corn kernels either side of the opening, a spill out of the burrow
    # and strays round the back; the front (238..302 degrees) stays clear for the opening
    rr = M.rnd(14)
    for k, a in enumerate((228, 312, 160, 20, 100, 65)):   # leaning on the hive
        ar = math.radians(a)
        rx, ry = R * 0.98 + 0.08, R * FLAT * 0.98 + 0.08
        ob = kernel(f"lk{k}", rr.uniform(0.21, 0.25))
        parts.append(lean_kernel(ob, math.cos(ar) * rx, math.sin(ar) * ry, a, rr.uniform(18, 30)))
    lying = []
    for sx, (cx, cy) in ((-1, (-0.64, -0.74)), (1, (0.66, -0.72))):   # the two heaps
        for dx, dy, yaw in ((-0.11, 0.06, 30), (0.08, -0.05, 120), (-0.03, -0.17, -40)):
            lying.append((cx + sx * dx, cy + dy, rr.uniform(0.2, 0.23), sx * yaw))
    lying += [(0.06, -0.95, 0.2, 100), (-0.2, -1.0, 0.19, -20), (0.24, -1.04, 0.19, 40),   # spill
              (-0.92, 0.06, 0.2, 10), (-0.68, 0.56, 0.19, 200), (0.02, 0.88, 0.19, 100), (0.6, 0.64, 0.2, -40),
              (0.93, 0.1, 0.2, 170), (-0.36, 0.84, 0.18, 60)]
    for k, (x, y, sz, yaw) in enumerate(lying):
        parts.append(lay_kernel(kernel(f"ck{k}", sz), sz, x, y, yaw, roll=rr.uniform(-15, 15)))
    # one stacked on top of each heap and one standing upright in the spill
    for k, (x, y, yaw) in enumerate(((-0.62, -0.75, 80), (0.64, -0.73, -70))):
        parts.append(lay_kernel(kernel(f"sk{k}", 0.21), 0.21, x, y, yaw, roll=25, z=0.08))
    parts.append(lean_kernel(kernel("up0", 0.22), -0.02, -1.12, -100, -8, z=-0.01))
    return done(parts, "nest_gremlin", ao_radius=0.7)


# ====================================================================== SARCOPHAGUS
def coffin_outline(w_sh, w_ft, h, w_head):
    pts = [(-w_ft / 2, 0.0), (w_ft / 2, 0.0), (w_sh / 2, h * 0.68)]
    for i in range(1, 9):
        a = math.pi * i / 9
        pts.append((math.cos(a) * w_head / 2, h * 0.84 + math.sin(a) * (h * 0.16)))
    pts.append((-w_sh / 2, h * 0.68))
    return pts


def nest_mummy():
    K.reset()
    parts = []
    parts.append(F.rbox("plinth", (1.4, 0.95, 0.16), loc=(0, 0.05, 0.08), bevel=0.04, segments=2, color="stone",
                        mat="stone"))
    for s in (-1, 1):
        g = M.plate(f"glyph{s}", [(-0.05, 0), (0.05, 0), (0.07, 0.06), (0.0, 0.1), (-0.07, 0.06)], 0.01, GLOW, bevel=0.0)
        M.xform(g, Matrix.Translation((s * 0.5, -0.43, 0.04)))
        parts.append(g)
    H, WS, WF, WH = 2.0, 0.9, 0.62, 0.7
    case = M.plate("case", coffin_outline(WS, WF, H, WH), 0.5, "sarc_blue", mat="metal", bevel=0.05, segments=2,
                   center=(0, 0, H / 2))
    M.xform(case, Matrix.Translation((0, 0.12, 0.16)))
    parts.append(case)
    inner = M.plate("inner", coffin_outline(WS - 0.16, WF - 0.14, H - 0.12, WH - 0.14), 0.04, "ink", bevel=0.0)
    M.xform(inner, Matrix.Translation((0, -0.14, 0.22)))
    parts.append(inner)
    glow = M.plate("innerglow", coffin_outline(WS - 0.24, WF - 0.2, H - 0.22, WH - 0.24), 0.03, GLOW, bevel=0.0)
    M.xform(glow, Matrix.Translation((0, -0.15, 0.27)))
    parts.append(glow)
    dark = M.plate("innerdark", coffin_outline(WS - 0.36, WF - 0.32, H - 0.42, WH - 0.36), 0.03, "ink", bevel=0.0)
    M.xform(dark, Matrix.Translation((0, -0.16, 0.37)))
    parts.append(dark)
    for s in (-1, 1):
        parts.append(M.blob(f"peek{s}", (s * 0.075, -0.19, 1.68), (0.05, 0.02, 0.035), segs=8, rings=3, color=HOT))
    # the lid: gold with a painted pharaoh face, swung open on the right-hand hinge
    lid_parts = []
    lid = M.plate("lid", coffin_outline(WS + 0.02, WF + 0.02, H + 0.02, WH + 0.02), 0.09, "foil_gold", mat="metal",
                  bevel=0.03, segments=2, center=(0, 0, H / 2))
    lid_parts.append(lid)
    # striped headdress, face, glowing eyes, crossed arms
    hd = [(-0.33, H * 0.62), (0.33, H * 0.62), (0.31, H * 0.84)] + \
         [(math.cos(math.pi * i / 8) * 0.3, H * 0.84 + math.sin(math.pi * i / 8) * 0.28) for i in range(1, 8)] + [(-0.31, H * 0.84)]
    head = M.plate("hd", hd, 0.02, "sarc_blue", mat="metal", bevel=0.0)
    M.xform(head, Matrix.Translation((0, -0.055, 0)))
    lid_parts.append(head)
    for k, z in enumerate((H * 0.66, H * 0.72, H * 0.78)):
        st = K.box(f"stripe{k}", (0.62 - k * 0.02, 0.01, 0.03), color="foil_gold", mat="metal")
        st.location = (0, -0.067, z)
        lid_parts.append(st)
    face = M.blob("face", (0, -0.06, H * 0.8), (0.17, 0.05, 0.2), segs=12, rings=6, color="caramel", mat="flat")
    lid_parts.append(face)
    for s in (-1, 1):
        lid_parts.append(M.blob(f"leye{s}", (s * 0.065, -0.1, H * 0.83), (0.045, 0.02, 0.028), segs=8, rings=3,
                                color=HOT))
        lid_parts.append(M.blob(f"lbrow{s}", (s * 0.07, -0.1, H * 0.87), (0.05, 0.012, 0.01), segs=6, rings=3,
                                color="ink"))
    lid_parts.append(M.blob("beard", (0, -0.08, H * 0.66), (0.04, 0.03, 0.08), segs=6, rings=4, color="sarc_blue",
                            mat="metal"))
    for s in (-1, 1):
        arm = M.tube(f"carm{s}", [V((s * 0.28, -0.06, H * 0.55)), V((0.0, -0.09, H * 0.46)), V((-s * 0.2, -0.09, H * 0.5))],
                     0.035, sides=6, color="sarc_blue", mat="metal")
        lid_parts.append(arm)
    lidob = K.merge(lid_parts, "lidob")
    hinge = V((WS / 2 + 0.02, -0.16, 0.16))
    M.xform(lidob, Matrix.Translation(hinge) @ Matrix.Rotation(math.radians(38), 4, "Z") @
            Matrix.Translation(-hinge + V((0, -0.16, 0.16))))
    parts.append(lidob)
    # gold bands on the case front edge
    for z in (0.45, 1.05, 1.62):
        bw = WF + (WS - WF) * min(z / (H * 0.68), 1.0) + 0.03 if z < H * 0.7 else WH + 0.05
        b = F.rbox(f"band{z}", (bw, 0.52, 0.05), loc=(0, 0.12, 0.16 + z), bevel=0.015,
                   segments=1, color="foil_gold", mat="metal")
        parts.append(b)
    # bandages spilling out across the plinth onto the ground
    for k, pts in enumerate(([V((-0.15, -0.15, 0.6)), V((-0.2, -0.32, 0.3)), V((-0.35, -0.5, 0.17)), V((-0.6, -0.75, 0.04)),
                              V((-0.85, -0.85, 0.02))],
                             [V((0.12, -0.15, 0.95)), V((0.1, -0.33, 0.55)), V((0.25, -0.47, 0.18)), V((0.42, -0.7, 0.03))])):
        parts.append(M.tube(f"wrap{k}", M.catmull(pts, 2), 0.045, sides=4, color="cream" if k == 0 else "bone",
                            mat="paper", flatten=0.25, cap_offset=0.0))
    return done(parts, "nest_mummy", ao_radius=0.7)


# ====================================================================== BUBBLING CAULDRON
def nest_slime():
    K.reset()
    parts = []
    # fire: crossed logs and ember flames
    for k, a in enumerate((20, 80, 140)):
        ar = math.radians(a)
        d = V((math.cos(ar), math.sin(ar), 0))
        parts.append(F.board(f"log{k}", d * -0.62 + V((0, 0, 0.07)), d * 0.62 + V((0, 0, 0.07 + 0.03 * k)), 0.14, 0.14,
                             r=0.06, n=2, up=(0, 0, 1), color="bark", mat="bark", end_color="wood_light"))
    for k in range(5):
        a = TAU * k / 5 + 0.3
        fl = M.cone(f"flame{k}", 0.3 - 0.04 * (k % 2), 0.09, segs=6, color=GLOW if k % 2 else "glow_lantern_red",
                    bend=0.1)
        M.put(fl, (math.cos(a) * 0.3, math.sin(a) * 0.3, 0.1), (math.cos(a) * 0.3, math.sin(a) * 0.3, 1), up=(0, -1, 0))
        parts.append(fl)
    # the pot
    pot = M.lathe("pot", [(0.0, 0.18), (0.3, 0.18), (0.55, 0.24), (0.72, 0.38), (0.8, 0.58), (0.79, 0.78),
                          (0.72, 0.93), (0.66, 1.0), (0.64, 1.02), (0.0, 1.02)], 18, color="lamp_black", mat="metal",
                  project="sphere", center=(0, 0, 0.6))
    parts.append(pot)
    parts.append(K.torus("lip", 0.69, 0.06, color="iron", mat="metal", major_seg=18, minor_seg=5, loc=(0, 0, 1.03)))
    for k in range(3):
        a = TAU * k / 3 + 0.5
        d = V((math.cos(a), math.sin(a), 0))
        parts.append(M.tube(f"leg{k}", [d * 0.42 + V((0, 0, 0.3)), d * 0.55 + V((0, 0, 0.12)), d * 0.6 + V((0, 0, 0.0))],
                            lambda t: 0.08 * (1 - 0.3 * t), sides=6, color="lamp_black", mat="metal", cap_offset=0.4))
    for s in (-1, 1):
        parts.append(K.torus(f"handle{s}", 0.12, 0.025, color="iron", mat="metal", major_seg=10, minor_seg=4,
                             loc=(s * 0.82, 0, 0.8), rot=(90, 0, 90)))
    # the goo: domed glowing surface, bubbles, and overflow running down the sides
    goo = M.blob("goo", (0, 0, 1.0), (0.66, 0.66, 0.1), segs=18, rings=5, color="glow_slime",
                 warp=lambda c: V((c.x, c.y, c.z + 0.25 * math.sin(c.x * 5) * math.cos(c.y * 4) * max(0, c.z))))
    parts.append(goo)
    rr = M.rnd(15)
    for k, (x, y, r) in enumerate(((-0.2, -0.15, 0.13), (0.22, 0.1, 0.1), (0.05, 0.32, 0.08), (-0.32, 0.22, 0.07),
                                   (0.3, -0.3, 0.09), (-0.05, -0.42, 0.06))):
        parts.append(M.blob(f"bub{k}", (x, y, 1.08 + r * 0.3), (r, r, r * 0.8), segs=8, rings=5,
                            color="slime_light" if k % 3 == 0 else "glow_slime", mat="vinyl"))
    pt = M.bvh(pot)
    for k, a in enumerate((-80, -40, 30, 150, 220)):
        ar = math.radians(a)
        d = V((math.cos(ar), math.sin(ar), 0))
        z_end = 0.62 - 0.12 * (k % 3)
        pts = []
        for i in range(5):
            z = 1.04 - (1.04 - z_end) * i / 4
            q, nq = M.surf(pt, d * 2.0 + V((0, 0, z)), -d)
            if q is None:
                q, nq = d * 0.75 + V((0, 0, z)), d
            pts.append(q + nq * 0.02)
        pts[0] = pts[0] + V((0, 0, 0.04)) - d * 0.05
        parts.append(M.tube(f"drip{k}", pts, lambda t: 0.055 - 0.015 * t, sides=6, color="slime_green", mat="vinyl",
                            cap_start=False, cap_end=False))
        parts.append(M.blob(f"dropg{k}", pts[-1] - V((0, 0, 0.02)), (0.05, 0.05, 0.065), segs=8, rings=5,
                            color="glow_slime" if k % 2 else "slime_light", mat="vinyl"))
    # a puddle at the foot
    parts.append(M.blob("puddle", (0.62, -0.62, 0.0), (0.3, 0.22, 0.03), segs=12, rings=3, color="slime_green", mat="vinyl"))
    return done(parts, "nest_slime", ao_radius=0.7)


# ====================================================================== CROW POST
def crow(name, loc, yaw=0.0, s=1.0, look=0.0):
    C = "crow_blue"
    body = M.blob(name + "_b", (0, 0.02 * s, 0.16 * s), (0.13 * s, 0.16 * s, 0.14 * s), segs=10, rings=6, color=C, mat="felt")
    head = M.blob(name + "_h", (0, -0.1 * s, 0.32 * s), (0.1 * s, 0.1 * s, 0.1 * s), segs=10, rings=6, color=C, mat="felt")
    beak = M.cone(name + "_k", 0.1 * s, 0.035 * s, segs=5, color="candy_corn_yellow", mat="flat")
    M.put(beak, (0, -0.18 * s, 0.31 * s), (0, -1, -0.15), up=(0, 0, 1), scale=(1.0, 0.7, 1.0))
    tail = M.blob(name + "_t", (0, 0.2 * s, 0.13 * s), (0.08 * s, 0.13 * s, 0.03 * s), segs=6, rings=3, color=C,
                  mat="felt", rot=Matrix.Rotation(math.radians(25), 3, "X"))
    parts = [body, head, beak, tail]
    for e in (-1, 1):
        parts.append(M.blob(name + f"_e{e}", (e * 0.05 * s, -0.17 * s, 0.36 * s), (0.028 * s, 0.015 * s, 0.03 * s),
                            segs=6, rings=3, color=HOT))
        w = M.blob(name + f"_w{e}", (e * 0.12 * s, 0.03 * s, 0.17 * s), (0.04 * s, 0.13 * s, 0.09 * s), segs=6, rings=4,
                   color="bat_fur", mat="felt")
        parts.append(w)
    tuft = M.cone(name + "_tf", 0.08 * s, 0.025 * s, segs=4, color=C, mat="felt", lo=True)
    M.put(tuft, (0, -0.08 * s, 0.4 * s), (0, 0.4, 1), up=(0, -1, 0))
    parts.append(tuft)
    ob = K.merge(parts, name)
    M.xform(ob, Matrix.Translation(V(loc)) @ Matrix.Rotation(math.radians(yaw), 4, "Z"))
    return ob


def nest_scarecrow():
    K.reset()
    parts = []
    parts.append(M.blob("mound", (0, 0, -0.04), (0.6, 0.55, 0.18), segs=14, rings=5, color="grave_soil", mat="soil"))
    rr = M.rnd(3)
    for k in range(9):
        a = TAU * k / 9 + rr.uniform(-0.2, 0.2)
        d = V((math.cos(a), math.sin(a), 0))
        st = M.cone(f"hay{k}", rr.uniform(0.18, 0.26), 0.035, segs=3, color=("straw", "hay", "straw_dark")[k % 3],
                    mat="straw", lo=True)
        M.put(st, d * 0.14 + V((0, 0, 0.08)), d * 0.8 + V((0, 0, 1)), up=(0, 0, 1))
        parts.append(st)
    lean = Matrix.Rotation(math.radians(-3), 4, "Y")
    tall = []
    tall.append(F.board("post", (0, 0, 0.0), (0, 0, 3.05), 0.16, 0.16, r=0.035, up=(0, 1, 0), color="wood_dark", mat="wood"))
    tall.append(F.board("bar", (-1.0, 0, 2.5), (1.0, 0, 2.62), 0.13, 0.11, r=0.03, up=(0, 1, 0), color="wood_dark",
                        mat="wood"))
    tall.append(K.torus("rope1", 0.1, 0.022, color="rope", mat="burlap", major_seg=8, minor_seg=4, loc=(0, 0, 2.56),
                        rot=(0, 0, 45)))
    tall.append(K.torus("rope2", 0.1, 0.022, color="rope", mat="burlap", major_seg=8, minor_seg=4, loc=(0, 0, 1.2)))
    for k in range(7):
        a = TAU * k / 7
        d = V((math.cos(a), math.sin(a), 0))
        st = M.cone(f"tie{k}", 0.2, 0.03, segs=3, color=("straw", "hay")[k % 2], mat="straw", lo=True)
        M.put(st, d * 0.08 + V((0, 0, 1.18)), d * 0.6 + V((0, 0, -1)), up=(0, 0, 1))
        tall.append(st)
    # three plump crows with glowing eyes
    tall.append(crow("crow0", (-0.72, 0.0, 2.57), yaw=15, s=1.0))
    tall.append(crow("crow1", (0.55, 0.0, 2.6), yaw=-25, s=1.1))
    tall.append(crow("crow2", (0.0, 0.0, 3.05), yaw=-5, s=0.95))
    # tattered flannel flag
    flag_pts = [(0.0, 0.0), (0.0, 0.42), (0.55, 0.36), (0.48, 0.27), (0.6, 0.18), (0.47, 0.12), (0.55, 0.03)]
    fl = M.plate("flag", [(x, z) for x, z in flag_pts], 0.015, "flannel_red", mat="fabric", bevel=0.0)
    M.xform(fl, Matrix.Translation((-0.08, 0.04, 1.85)) @ Matrix.Rotation(math.radians(180), 4, "Z") @
            Matrix.Rotation(math.radians(8), 4, "X"))
    for v in fl.data.vertices:
        v.co.y += 0.06 * math.sin(v.co.x * 9)
    tall.append(fl)
    # a jack-o'-lantern hanging from the bar's end
    tall.append(M.tube("hang", [V((-0.92, 0, 2.48)), V((-0.92, 0, 2.08))], 0.012, sides=4, color="rope", mat="burlap"))
    jack = F.pumpkin_body("jack", width=0.36, height=0.3, lobes=6, spl=3, rings=7, seed=4, color="pumpkin", rot_z=-90)
    M.xform(jack, Matrix.Translation((-0.92, 0, 1.8)))
    tall.append(jack)
    for s in (-1, 1):
        e = K.polygon_prism(f"je{s}", F.rounded_triangle(0, 0, 0.08, 0.065, round_=0.2), 0.01, color=HOT)
        M.put(e, (-0.92 + s * 0.065, -0.17, 1.98), (0, -1, 0.15), up=(0, 0, 1))
        tall.append(e)
    m = K.polygon_prism("jm", F.grin(0, 0, 0.18, 0.012, 0.05, n=9), 0.01, color=HOT)
    M.put(m, (-0.92, -0.175, 1.9), (0, -1, 0.0), up=(0, 0, 1))
    tall.append(m)
    for ob in tall:
        M.bake(ob)
        M.xform(ob, lean)
    parts += tall
    return done(parts, "nest_scarecrow", ao_radius=0.6)


# ====================================================================== WEB NEST
def nest_spider():
    K.reset()
    parts = []
    parts.append(M.blob("ground", (0, 0, -0.04), (1.2, 1.15, 0.12), segs=18, rings=5, color="grave_soil", mat="soil"))
    parts.append(M.blob("hole", (0, 0, 0.065), (0.62, 0.6, 0.03), segs=16, rings=3, color="ink"))
    parts.append(K.torus("holerim", 0.64, 0.07, color="cinnamon", mat="soil", major_seg=16, minor_seg=4, loc=(0, 0, 0.07),
                         scale=(1, 0.97, 0.6)))
    # glowing egg sacs nestled in the hole, each with a silk cap
    for k, (x, y, r) in enumerate(((-0.18, 0.05, 0.16), (0.15, -0.12, 0.14), (0.12, 0.22, 0.12), (-0.08, -0.25, 0.1))):
        parts.append(M.blob(f"egg{k}", (x, y, 0.06 + r * 0.8), (r, r, r * 1.15), segs=10, rings=6, color="glow_purple"))
        parts.append(M.blob(f"eggcap{k}", (x, y, 0.06 + r * 1.75), (r * 0.7, r * 0.7, r * 0.35), segs=8, rings=4,
                            color="ghost_white", mat="cloud"))
    # a tepee of four crooked dead twigs, crossing near the top
    rr = M.rnd(31)
    TOP = V((0.02, 0.0, 1.32))
    spokes = []
    for k, a in enumerate((35, 125, 205, 300)):
        ar = math.radians(a + rr.uniform(-8, 8))
        d = V((math.cos(ar), math.sin(ar), 0))
        sd = V((-d.y, d.x, 0))
        base = d * rr.uniform(0.95, 1.08)
        tip = TOP - d * 0.12 + sd * 0.1 + V((0, 0, rr.uniform(0.1, 0.22)))
        mid = base.lerp(tip, 0.5) + d * 0.12 + sd * rr.uniform(-0.08, 0.08)
        pts = M.catmull([base - V((0, 0, 0.05)), mid, tip], 3)
        parts.append(M.tube(f"twig{k}", pts, lambda t: 0.055 * (1 - 0.55 * t), sides=6, color="bark_dark", mat="bark",
                            cap_offset=0.5))
        fork = pts[3]
        parts.append(M.tube(f"fork{k}", [fork, fork + d * 0.16 + V((0, 0, 0.14)) + sd * 0.05], lambda t: 0.025 * (1 - 0.6 * t),
                            sides=4, color="bark_dark", mat="bark", cap_offset=0.5))
        spokes.append(lambda t, pts=pts: pts[min(int(t * (len(pts) - 1)), len(pts) - 2)].lerp(
            pts[min(int(t * (len(pts) - 1)), len(pts) - 2) + 1], t * (len(pts) - 1) - min(int(t * (len(pts) - 1)), len(pts) - 2)))
        # a silk guy-line between this twig and the next, reaching down to the ground
        a2 = ar + math.radians(45)
        g = V((math.cos(a2), math.sin(a2), 0)) * 1.12
        spokes.append(lambda t, g=g, a2=a2: g.lerp(TOP, t) + V((math.cos(a2), math.sin(a2), 0)) * 0.12 * math.sin(math.pi * t))
    hub = TOP - V((0, 0, 0.12))
    # radial threads from the hub to the twigs, and sagging spiral threads between neighbours
    for k, sp in enumerate(spokes):
        if k % 2:
            pts = [sp(t) for t in (0.0, 0.33, 0.66, 1.0)]
            parts.append(M.tube(f"guy{k}", pts, 0.014, sides=4, color="ghost_white", mat="flat", cap_offset=0.0))
    for ring, t in enumerate((0.3, 0.55, 0.78)):
        for k in range(len(spokes)):
            ta = t + rr.uniform(-0.05, 0.05)
            a, b = spokes[k](ta), spokes[(k + 1) % len(spokes)](ta + rr.uniform(-0.04, 0.04))
            mid = (a + b) / 2
            mid = V((mid.x * 0.9, mid.y * 0.9, mid.z - 0.07 - 0.04 * rr.random()))
            parts.append(M.tube(f"sp{ring}_{k}", M.catmull([a, mid, b], 2), 0.012, sides=4, color="ghost_white",
                                mat="flat", cap_offset=0.0))
    for k in range(0, len(spokes), 2):
        parts.append(M.tube(f"rad{k}", [hub, spokes[k](0.85)], 0.012, sides=4, color="ghost_white", mat="flat"))
    parts.append(M.blob("hubwad", hub, (0.12, 0.12, 0.09), segs=8, rings=5, color="ghost_white", mat="cloud"))
    TOP = hub
    # a spiderling dangling from the hub
    sp = V((0.15, -0.1, 0.92))
    parts.append(M.tube("thread", [TOP - V((0, 0, 0.03)), sp + V((0, 0, 0.1))], 0.008, sides=3, color="ghost_white"))
    parts.append(M.blob("sling", sp, (0.1, 0.09, 0.1), segs=10, rings=6, color="spider_plum", mat="felt"))
    for s in (-1, 1):
        parts.append(M.blob(f"sle{s}", sp + V((s * 0.035, -0.085, 0.01)), (0.022, 0.012, 0.026), segs=6, rings=3,
                            color=HOT))
        for k in range(2):
            ln = M.tube(f"sll{s}{k}", [sp + V((s * 0.06, -0.02 + 0.05 * k, 0)), sp + V((s * 0.14, -0.04 + 0.08 * k, 0.05)),
                                       sp + V((s * 0.17, -0.05 + 0.1 * k, -0.06))], 0.012, sides=4, color="witch_purple",
                        mat="felt")
            parts.append(ln)
    return done(parts, "nest_spider", ao_radius=0.6)


# ====================================================================== FAIRY RING
def toadstool(name, loc, h, cap_r, cap_col="witch_purple", stem_r=None, spots=4, glow_spots=True, seed=0, tilt=0.0):
    stem_r = stem_r or cap_r * 0.32
    st = M.lathe(name + "_s", [(0.0, 0.0), (stem_r * 1.15, 0.0), (stem_r, h * 0.4), (stem_r * 0.85, h), (0.0, h)], 6,
                 color="cream", mat="stucco")
    cap = M.lathe(name + "_c", [(stem_r * 0.8, h - 0.02), (cap_r, h - 0.01), (cap_r * 1.02, h + cap_r * 0.18),
                                (cap_r * 0.75, h + cap_r * 0.5), (cap_r * 0.35, h + cap_r * 0.66), (0.0, h + cap_r * 0.7)],
                  10, color=cap_col, mat="vinyl", project="sphere")
    parts = [st, cap]
    rr = M.rnd(seed)
    for k in range(spots):
        a = TAU * k / spots + rr.uniform(-0.4, 0.4)
        rho = cap_r * rr.uniform(0.35, 0.7)
        z = h + cap_r * 0.7 * math.sqrt(max(0.05, 1 - (rho / (cap_r * 1.02)) ** 2)) * 0.95
        n = V((math.cos(a) * rho / cap_r, math.sin(a) * rho / cap_r, 0.8)).normalized()
        sp = M.blob(name + f"_p{k}", (0, 0, 0), (cap_r * 0.16, cap_r * 0.16, cap_r * 0.05), segs=6, rings=3,
                    color="glow_purple" if glow_spots else "lolly_white")
        M.put(sp, (math.cos(a) * rho, math.sin(a) * rho, z), n)
        parts.append(sp)
    ob = K.merge(parts, name)
    M.xform(ob, Matrix.Translation(V(loc)) @ Matrix.Rotation(math.radians(tilt), 4, "X"))
    return ob


def nest_toadstool():
    K.reset()
    parts = []
    parts.append(M.blob("grass", (0, 0, -0.03), (1.45, 1.45, 0.07), segs=20, rings=4, color="moss", mat="grass"))
    parts.append(K.torus("ring", 1.12, 0.05, color="glow_purple", mat="flat", major_seg=24, minor_seg=3,
                         loc=(0, 0, 0.03), scale=(1, 1, 0.5)))
    # the cottage: a fat toadstool with a round door and window glowing warm
    st = M.lathe("hstem", [(0.0, 0.0), (0.42, 0.0), (0.43, 0.08), (0.4, 0.35), (0.36, 0.6), (0.34, 0.72), (0.0, 0.72)], 14,
                 color="cream", mat="stucco")
    parts.append(st)
    cap = M.lathe("hcap", [(0.3, 0.66), (0.66, 0.66), (0.72, 0.72), (0.7, 0.84), (0.58, 0.98), (0.4, 1.1),
                           (0.2, 1.17), (0.0, 1.19)], 16, color="witch_purple", mat="vinyl", project="sphere",
                  warp=lambda c: V((c.x, c.y, c.z + 0.03 * math.sin(math.atan2(c.y, c.x) * 3))))
    K.paint(cap, "cream", where=lambda c, n: n.z < -0.4, mat="stucco")
    parts.append(cap)
    ct = M.bvh(cap)
    for k, (a, d, r) in enumerate(((0, 0.0, 0.12), (30, 0.4, 0.09), (110, 0.45, 0.1), (190, 0.38, 0.08), (260, 0.5, 0.08),
                                   (330, 0.55, 0.07), (75, 0.6, 0.06), (150, 0.62, 0.06))):
        ar = math.radians(a)
        p, n = M.surf(ct, (math.cos(ar) * d, math.sin(ar) * d, 3.0), (0, 0, -1))
        if p is None:
            continue
        sp = M.blob(f"spot{k}", (0, 0, 0), (r, r, r * 0.3), segs=8, rings=3, color="lolly_white", mat="vinyl")
        M.put(sp, p - n * 0.005, n)
        parts.append(sp)
    stt = M.bvh(st)
    p, n = M.surf(stt, (0.0, -2.0, 0.26), (0, 1, 0))
    door_out = [(-0.13, 0.0)] + [(math.cos(math.pi * i / 8) * -0.13, 0.2 + math.sin(math.pi * i / 8) * 0.13) for i in range(9)] + [(0.13, 0.0)]
    door_out = [(x, z) for x, z in door_out]
    frame = M.plate("doorglow", [(x * 1.18, z * 1.1 - 0.0) for x, z in door_out], 0.03, "glow_window", bevel=0.0)
    door = M.plate("door", door_out, 0.04, "wood", mat="planks", bevel=0.01)
    for ob, off in ((frame, 0.0), (door, 0.02)):
        M.xform(ob, Matrix.Translation((p.x, p.y - off, 0.0)))
    parts += [frame, door]
    parts.append(M.blob("knob", (0.08, p.y - 0.05, 0.16), (0.018, 0.018, 0.018), segs=6, rings=3, color="foil_gold"))
    p, n = M.surf(stt, (-2.0, -0.9, 0.45), (1.0, 0.45, 0))
    win = M.blob("win", (0, 0, 0), (0.085, 0.085, 0.02), segs=10, rings=3, color="glow_window")
    M.put(win, p, n)
    wfr = K.torus("winframe", 0.09, 0.018, color="wood", mat="wood", major_seg=10, minor_seg=4)
    M.put(wfr, p + n * 0.005, n)
    parts += [win, wfr]
    # step stones to the door
    for k, (x, y) in enumerate(((0.05, -0.62), (-0.08, -0.85))):
        parts.append(M.blob(f"step{k}", (x, y, 0.0), (0.12, 0.1, 0.04), segs=8, rings=3, color="stone_light", mat="stone"))
    # the ring of small glowing purple toadstools
    rr = M.rnd(9)
    for k in range(9):
        a = TAU * k / 9 + 0.17
        if abs(math.degrees(a) % 360 - 270) < 15:
            a += 0.3
        h = 0.18 + 0.12 * ((k * 5) % 3) / 2
        parts.append(toadstool(f"ts{k}", (math.cos(a) * 1.12, math.sin(a) * 1.12, 0.0), h, 0.12 + 0.05 * ((k * 7) % 3) / 2,
                               spots=3, seed=k, tilt=rr.uniform(-8, 8)))
    return done(parts, "nest_toadstool", ao_radius=0.7)


if __name__ == "__main__":
    M.run({"pumpkin": nest_pumpkin, "bat": nest_bat, "skeleton": nest_skeleton, "ghost": nest_ghost,
           "gremlin": nest_gremlin, "mummy": nest_mummy, "slime": nest_slime, "scarecrow": nest_scarecrow,
           "spider": nest_spider, "toadstool": nest_toadstool})
