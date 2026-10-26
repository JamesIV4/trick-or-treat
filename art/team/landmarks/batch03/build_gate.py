"""Travel gate: travel_gate_base (runed stone pad + wrought-iron crook post) and travel_gate_lantern
(hanging lantern + every glowing inlay on the pad), both in the same space: origin at the pad centre
on the ground, so the game drops them at the same transform. Blender coords (Z up, front -Y).

    blender -b --factory-startup --python build_gate.py -- [travel_gate_base travel_gate_lantern] [--combined]

--combined also writes base+lantern merged into props/travel_gate_base (preview only: the engine preview
renders one catalog asset). Rebuild without it before publishing.
"""
import pathlib
import random
import sys

sys.path.insert(0, str(pathlib.Path(__file__).resolve().parent))
from lm_common import *  # noqa: E402,F401,F403
from lm_common import ARGS, ellipse, stroke, rock, sector_slab, ribbed, export_many  # noqa: E402

IRON = "lamp_black"
TEAL = "glow_magic_teal"
GROOVE = "stone_dark"
TOP = 0.22          # walkable pad top
HANG = (-0.15, 0.0, 2.55)   # lantern body centre (Godot (-0.15, 2.55, 0))
POST_X = -1.08
COMBINED = "--combined" in ARGS


# ------------------------------------------------------------------ glyphs (pad runes)
def glyph_paths(kind, s):
    """Rune strokes in a local 2D frame (y = outward), size s. Returns [(points, width)]."""
    if kind == "crescent":
        pts = [(math.cos(math.radians(a)) * s * 0.45, math.sin(math.radians(a)) * s * 0.45)
               for a in range(60, 320, 37)]
        return [(pts, s * 0.2)]
    if kind == "sparkle":
        return [([(0, -s * 0.5), (0, 0), (0, s * 0.5)], s * 0.2), ([(-s * 0.36, 0), (0, 0), (s * 0.36, 0)], s * 0.17)]
    if kind == "spiral":
        pts = []
        for k in range(9):
            a = math.radians(90 + k * 66)
            r = s * (0.08 + 0.4 * k / 8)
            pts.append((math.cos(a) * r, math.sin(a) * r))
        return [(pts, s * 0.15)]
    if kind == "wave":
        pts = [(-s * 0.45 + s * 0.9 * k / 6, math.sin(k / 6 * math.pi * 2) * s * 0.18) for k in range(7)]
        return [(pts, s * 0.15)]
    raise KeyError(kind)


def place2d(pts, ang, r, rot_extra=0.0):
    """Local glyph frame (x right, y outward) placed at polar (r, ang deg) on the pad."""
    a = math.radians(ang)
    out = Vector((math.cos(a), math.sin(a)))
    right = Vector((math.sin(a), -math.cos(a)))
    c = out * r
    res = []
    for x, y in pts:
        p = c + right * x + out * y
        res.append((p.x, p.y))
    return res


def runes(glow):
    """Glow strokes (lantern asset) or the carved grooves under them (base asset)."""
    parts = []
    kinds = ["crescent", "sparkle", "spiral", "wave"]
    col = TEAL if glow else GROOVE
    widen = 0.0 if glow else 0.035
    z = TOP + (0.006 if glow else 0.001)
    th = 0.012 if glow else 0.006
    sides = 6 if glow else 4
    for k in range(8):
        ang = 22.5 + 45 * k
        for j, (pts, w) in enumerate(glyph_paths(kinds[k % 4], 0.26)):
            p2 = place2d(pts, ang, 1.15)
            parts.append(stroke(f"rune{k}_{j}{glow}", [(x, y, z) for x, y in p2], w + widen, col, thick=th, z=z,
                                sides=sides))
    # centre sigil: 8-point star of tapered strokes inside a ring
    for k in range(4):
        a = math.radians(k * 45)
        L = 0.3 if k % 2 == 0 else 0.2
        pts = [(math.cos(a) * -L, math.sin(a) * -L), (0, 0), (math.cos(a) * L, math.sin(a) * L)]
        parts.append(stroke(f"star{k}{glow}", [(x, y, z) for x, y in pts], (0.07 if k % 2 == 0 else 0.05) + widen, col,
                            thick=th, taper=(0.15, 0.15), z=z, sides=sides))
    w = 0.018 + widen / 2
    zt = z + th / 2
    parts.append(lathe(f"sring{glow}", [(0.38 + w, TOP - 0.004), (0.38 + w, zt), (0.38 - w, zt), (0.38 - w, TOP - 0.004)],
                       col, "flat", segments=24, cap_bottom=False, cap_top=False, smooth=False))
    for k in range(4 if glow else 0):
        a = math.radians(45 + k * 90)
        parts.append(blob(f"dot{k}{glow}", 0.028 + widen / 2, col, mat="flat", loc=(math.cos(a) * 0.26, math.sin(a) * 0.26, z),
                          scale=(1, 1, 0.25), segments=6, rings=3, noise_amp=0.0, axis="Z"))
    if glow:
        # the big ring sitting in the channel between the inner and outer slabs
        parts.append(lathe("chring", [(0.985, 0.17), (0.985, TOP - 0.006), (0.945, TOP - 0.006), (0.945, 0.17)],
                           TEAL, "flat", segments=32, cap_bottom=False, cap_top=False, smooth=False))
    return parts


# ------------------------------------------------------------------ base
def pad():
    parts = []
    rnd = random.Random(7)
    # drum (grout shows between the slabs)
    parts.append(lathe("drum", [(1.27, 0.0), (1.3, 0.03), (1.31, 0.12), (1.28, 0.175), (0.0, 0.175)],
                       ["stone", "stone", "stone", GROOVE], ["stone", "stone", "stone", "flat"], segments=24,
                       cap_bottom=False))
    # outer ring of 8 slabs carrying the runes
    for k in range(8):
        a0 = 45 * k + 1.6
        parts.append(sector_slab(f"oslab{k}", 1.005, 1.3, a0, a0 + 45 - 3.2, 0.15, TOP,
                                 "stone_light" if k % 2 == 0 else "sidewalk", mat="stone", bevel=0.018, n=4))
    # inner ring of 6 slabs + medallion
    for k in range(6):
        a0 = 60 * k + 15 + 2.0
        parts.append(sector_slab(f"islab{k}", 0.47, 0.925, a0, a0 + 60 - 4.0, 0.15, TOP,
                                 "sidewalk" if k % 2 == 0 else "stone_light", mat="pavers", bevel=0.016, n=4))
    parts.append(lathe("medal", [(0.43, 0.15), (0.445, TOP - 0.012), (0.43, TOP), (0.0, TOP)], "stone_light",
                       ["stone", "stone", "pavers"], segments=24, cap_bottom=False, sharp_bands=(2,)))
    # skirt of chunky fieldstones
    n = 16
    for k in range(n):
        a = 2 * math.pi * k / n + rnd.uniform(-0.04, 0.04)
        r = 1.36 + rnd.uniform(-0.02, 0.03)
        h = rnd.uniform(0.09, 0.15)
        col = rnd.choice(["stone", "stone", "stone_light", "stone_dark"])
        parts.append(rock(f"skirt{k}", (0.2 + rnd.uniform(0, 0.05), 2 * math.pi * 1.36 / n * 0.92, h), col,
                          loc=(math.cos(a) * r, math.sin(a) * r, h / 2 - 0.01), rot=(0, 0, math.degrees(a)),
                          bevel=0.035, segments=1, wobble=0.012, seed=k))
    # carved grooves under the glowing runes and the ring channel lip
    parts += runes(False)
    return parts


def moss_and_leaves():
    parts = []
    rnd = random.Random(3)
    # moss cushions spilling over the rim and between skirt stones
    for i, (ang, r, z, s) in enumerate([(205, 1.27, 0.2, 0.2), (140, 1.3, 0.16, 0.16), (255, 1.33, 0.12, 0.15),
                                        (320, 1.29, 0.19, 0.17), (40, 1.35, 0.1, 0.14), (95, 1.32, 0.15, 0.13),
                                        (172, 1.38, 0.06, 0.18), (290, 1.42, 0.05, 0.14)]):
        a = math.radians(ang)
        m = blob(f"moss{i}", s, "moss" if i % 3 else "hedge", mat="hedge", loc=(math.cos(a) * r, math.sin(a) * r, z),
                 scale=(1.0, 1.4, 0.4), rot=(0, 0, ang), segments=8, rings=4, lobes=5, lobe_amp=0.2, lobe_pow=3,
                 noise_amp=0.06, noise_freq=3.0, seed=i, axis="Z", uvmode="box")
        paint_world(m, "hedge_dark", lambda c, nn: nn.z < -0.2)
        parts.append(m)
    # little grass/fern tufts at the skirt
    for i, ang in enumerate(()):
        a = math.radians(ang)
        cx, cy = math.cos(a) * 1.48, math.sin(a) * 1.48
        for j in range(4):
            b = math.radians(ang + j * 90 + 20)
            tip = (cx + math.cos(b) * 0.08, cy + math.sin(b) * 0.08, 0.17 + 0.04 * (j % 2))
            parts.append(stroke(f"blade{i}_{j}", [(cx, cy, 0.0), (cx + math.cos(b) * 0.04, cy + math.sin(b) * 0.04, 0.1), tip],
                                0.035, "lawn" if j % 2 else "moss", thick=0.012, taper=(0.9, 0.2), sides=4, z=None))
    # fallen maple leaves on the pad and around it
    leaf_cols = ["maple_red", "maple_orange", "golden_leaf", "rust_leaf"]
    for i, (x, y, z) in enumerate([(0.62, -0.75, TOP), (-0.55, 0.62, TOP), (0.95, 0.35, TOP), (1.55, -0.6, 0.0),
                                   (-1.5, -0.95, 0.0), (0.3, 1.55, 0.0), (-0.2, -1.12, TOP)]):
        parts.append(maple_leaf(f"leaf{i}", (x, y, z + 0.004), 0.09, leaf_cols[i % 4], rnd.uniform(0, 360)))
    return parts


def maple_leaf(name, loc, s, color, yaw):
    """Flat 5-lobed maple leaf, slightly cupped."""
    outline = []
    NL = 12
    for k in range(NL):
        a = 2 * math.pi * k / NL
        lobe = 0.55 + 0.45 * abs(math.cos(a * 2.5)) ** 0.6
        if k == 9:  # stem notch at the bottom
            lobe *= 0.55
        outline.append((math.cos(a + math.pi / 2) * s * lobe, math.sin(a + math.pi / 2) * s * lobe))
    bm = bmesh.new()
    top = [bm.verts.new((x, y, 0.008)) for x, y in outline]
    bot = [bm.verts.new((x, y, 0.0)) for x, y in outline]
    c = bm.verts.new((0, 0, 0.014))
    for i in range(NL):
        j = (i + 1) % NL
        bm.faces.new((c, top[i], top[j]))
        bm.faces.new((bot[i], bot[j], top[j], top[i]))
    bmesh.ops.recalc_face_normals(bm, faces=bm.faces)
    uv_box(bm, bm.faces)
    K._paint_bm(bm, color, "leaves")
    ob = new_obj(bm, name, False)
    return transform(ob, loc=loc, rot=(0, 0, yaw))


CZ, RX, RZ = 3.25, 0.465, 0.16      # crook: ends over the hang point at x = -0.15
CAP_TOP = TOP + 0.8                  # stone pillar top


def face_glyph(name, kind, size, center, ang_deg, glow, lift=0.0):
    """A rune glyph standing on a vertical face whose outward normal points at ang_deg (0 = +X, -90 = front)."""
    parts = []
    for j, (pts, w) in enumerate(glyph_paths(kind, size)):
        ob = stroke(f"{name}{j}{glow}", [(x, y, 0) for x, y in pts], w + (0.0 if glow else 0.03),
                    TEAL if glow else GROOVE, thick=0.012 if glow else 0.006, z=0, sides=6 if glow else 4)
        d = Vector((math.cos(math.radians(ang_deg)), math.sin(math.radians(ang_deg)), 0))
        transform(ob, loc=tuple(Vector(center) + d * (lift + (0.006 if glow else 0.0))), rot=(90, 0, ang_deg + 90))
        parts.append(ob)
    return parts


RUNESTONES = [(-35, 1.5, 0.85, 21, "spiral"), (62, 1.5, 0.7, 22, "sparkle")]


def runestones(glow):
    parts = []
    for ang, r, h, seed, kind in RUNESTONES:
        a = math.radians(ang)
        c = Vector((math.cos(a) * r, math.sin(a) * r, 0))
        if not glow:
            st = rock(f"rstone{seed}", (0.32, 0.2, h), "stone", loc=(c.x, c.y, h / 2 - 0.02),
                      rot=(0, 0, ang + 90), bevel=0.07, segments=2, wobble=0.02, seed=seed)
            # lean outward a touch, top a bit narrower
            deform(st, lambda v, c=c, h=h: Vector((c.x + (v.x - c.x) * (1 - 0.3 * (v.z / h) ** 2),
                                                   c.y + (v.y - c.y) * (1 - 0.3 * (v.z / h) ** 2), v.z)) + Vector((c.x, c.y, 0)).normalized() * 0.08 * (v.z / h) ** 2)
            parts.append(st)
            m = blob(f"rsmoss{seed}", 0.12, "moss", mat="hedge", loc=(c.x * 1.02, c.y * 1.02, h - 0.07), scale=(1.15, 0.95, 0.55),
                     rot=(0, 0, ang + 90), segments=8, rings=4, lobes=4, lobe_amp=0.2, noise_amp=0.05, noise_freq=3.0,
                     seed=seed, axis="Z", uvmode="box")
            paint_world(m, "hedge_dark", lambda cc, nn: nn.z < -0.2)
            parts.append(m)
        parts += face_glyph(f"rsg{seed}", kind, 0.2, (c.x, c.y, h * 0.48), ang, glow, lift=0.1)
    return parts


def footing_rune(glow):
    """Crescent moon carved into the front face of the pillar's middle stone."""
    return face_glyph("frune", "crescent", 0.24, (POST_X, 0.0, TOP + 0.5), -90, glow, lift=0.21)


def post():
    parts = []
    # stacked stone pillar: wide base stone, narrower middle stone, overhanging cap, moss
    parts.append(rock("pbase", (0.62, 0.58, 0.3), "stone", loc=(POST_X, 0.0, TOP + 0.14), bevel=0.06, segments=2,
                      wobble=0.018, seed=11))
    parts.append(rock("pmid", (0.48, 0.42, 0.42), "stone_light", loc=(POST_X, 0.0, TOP + 0.49), bevel=0.06, segments=2,
                      wobble=0.014, seed=13))
    parts.append(rock("pcap", (0.6, 0.56, 0.11), "stone", loc=(POST_X, 0.0, CAP_TOP - 0.05), bevel=0.04, segments=2,
                      wobble=0.01, seed=12))
    for nm, s, loc, sc, col, sd in [("capmoss", 0.17, (POST_X + 0.13, 0.12, CAP_TOP - 0.01), (1.3, 1.0, 0.35), "moss", 4),
                                    ("capmoss2", 0.1, (POST_X - 0.2, -0.2, CAP_TOP - 0.04), (1.0, 1.0, 0.6), "hedge", 6),
                                    ("footmoss", 0.15, (POST_X - 0.24, -0.26, TOP + 0.08), (1.2, 0.9, 0.55), "hedge", 5),
                                    ("footmoss2", 0.12, (POST_X + 0.27, 0.2, TOP + 0.05), (1.0, 1.2, 0.5), "moss", 7)]:
        m = blob(nm, s, col, mat="hedge", loc=loc, scale=sc, segments=8, rings=4, lobes=5, lobe_amp=0.2,
                 noise_amp=0.06, noise_freq=3.0, seed=sd, axis="Z", uvmode="box")
        paint_world(m, "hedge_dark", lambda c, n: n.z < -0.2)
        parts.append(m)
    parts += footing_rune(False)
    # iron post: bell base, tapering pole, collars
    ct = CAP_TOP
    parts.append(lathe("bell", [(0.17, ct - 0.01), (0.17, ct + 0.03), (0.13, ct + 0.06), (0.095, ct + 0.14),
                                (0.08, ct + 0.28), (0.1, ct + 0.31), (0.1, ct + 0.35), (0.075, ct + 0.38)],
                       IRON, "metal", segments=12, cap_bottom=False, cap_top=False, phase=0.26, loc=(POST_X, 0, 0)))
    z0 = ct + 0.36
    cx = POST_X + RX
    parts.append(sweep("pole", [(POST_X, 0, z0), (POST_X, 0, 2.2), (POST_X, 0, CZ)], [0.075, 0.068, 0.062], color=IRON,
                       mat="metal", sides=10, caps=(False, False)))
    for zc in (1.95, 2.7):
        parts.append(lathe(f"collar{zc}", [(0.055, zc - 0.05), (0.088, zc - 0.02), (0.088, zc + 0.02), (0.055, zc + 0.05)],
                           IRON, "metal", segments=10, cap_bottom=False, cap_top=False, loc=(POST_X, 0, 0)))
    # crook arm: half ellipse over to the hang point, ending in a knob the lantern ring hooks on
    crook = []
    for k in range(13):
        a = math.radians(180 - 180 * k / 12)
        crook.append((cx + math.cos(a) * RX, 0, CZ + math.sin(a) * RZ))
    crook.append((cx + RX, 0, CZ - 0.04))
    parts.append(sweep("crook", [(POST_X, 0, CZ - 0.02)] + crook, [0.062] * 4 + [0.056] * 6 + [0.05] * 5, color=IRON,
                       mat="metal", sides=10, caps=(False, True)))
    parts.append(blob("hookknob", 0.048, IRON, mat="metal", loc=(HANG[0], 0, CZ - 0.06), segments=10, rings=6,
                      noise_amp=0.0, axis="Z"))
    # finial on the crook crown: ball + little crescent moon
    parts.append(blob("finial", 0.06, IRON, mat="metal", loc=(cx, 0, CZ + RZ + 0.06), segments=10, rings=6,
                      noise_amp=0.0, axis="Z"))
    moon = [(cx + math.cos(math.radians(a)) * 0.09, 0, CZ + RZ + 0.18 + math.sin(math.radians(a)) * 0.09)
            for a in range(-120, 130, 30)]
    moon_r = [0.012 + 0.022 * math.sin(math.pi * k / (len(moon) - 1)) for k in range(len(moon))]
    parts.append(sweep("moon", moon, moon_r, color=IRON, mat="metal", sides=6, shape=ellipse(1.0, 0.45),
                       up_hint=(0, 1, 0)))
    parts.append(rod("moonstem", (cx, 0, CZ + RZ + 0.1), (cx - 0.02, 0, CZ + RZ + 0.13), 0.018, IRON, sides=6))
    # wrought-iron scrolls under the arm
    curl1 = [(POST_X + 0.03, 0, CZ - 0.33)] + arc((POST_X + 0.27, 0, CZ - 0.06), 0.18, 215, 560, 12, plane="XZ",
                                                   radius_end=0.04)
    parts.append(sweep("curl1", curl1, [0.026 - 0.012 * k / len(curl1) for k in range(len(curl1))], color=IRON,
                       mat="metal", sides=6))
    curl2 = arc((HANG[0] - 0.23, 0, CZ + 0.03), 0.1, 20, -300, 10, plane="XZ", radius_end=0.03)
    parts.append(sweep("curl2", curl2, [0.02 - 0.008 * k / len(curl2) for k in range(len(curl2))], color=IRON,
                       mat="metal", sides=5))
    # ivy climbing the post
    ivy = []
    for k in range(20):
        t = k / 19
        a = t * math.pi * 4.5 + 0.6
        ivy.append((POST_X + math.cos(a) * 0.08, math.sin(a) * 0.08, ct + 0.05 + t * 1.5))
    parts.append(sweep("ivy", ivy, [0.016 - 0.007 * k / 19 for k in range(20)], color="vine_dark", mat="flat", sides=5))
    for k in range(2, 20, 2):
        x, y, z = ivy[k]
        d = Vector((x - POST_X, y, 0)).normalized()
        lf = blob(f"ivyleaf{k}", 0.055 - 0.02 * k / 20, "vine" if k % 4 == 2 else "hedge", mat="leaves",
                  scale=(1.4, 1.0, 0.3), segments=6, rings=3, noise_amp=0.0, axis="X")
        transform(lf, loc=(x + d.x * 0.05, y + d.y * 0.05, z - 0.01),
                  rot=(0, -30, math.degrees(math.atan2(d.y, d.x))))
        paint_world(lf, "vine_dark", lambda c, n: n.z < 0)
        parts.append(lf)
    # pumpkins tucked beside the pillar
    for i, (px, py, r, h) in enumerate([(POST_X + 0.3, -0.5, 0.15, 0.19), (POST_X + 0.02, -0.55, 0.1, 0.12)]):
        parts.append(ribbed(f"pumpkin{i}", r, h, "pumpkin" if i == 0 else "pumpkin_light", ribs=8, depth=0.06, rings=5,
                            sides=12, loc=(px, py, TOP - 0.005)))
        parts.append(rod(f"pkstem{i}", (px, py, TOP + h - 0.03), (px + 0.02, py + 0.01, TOP + h + 0.04), 0.018,
                         "stem_green", mat="wood", sides=6))
    parts += runestones(False)
    return parts


# ------------------------------------------------------------------ lantern
def panes(n, frame_deg, glass, frame=IRON, offset=0.0):
    angles, cols = [], []
    for i in range(n):
        a = offset + i * 360.0 / n
        angles += [a - frame_deg / 2, a + frame_deg / 2]
        cols += [frame, glass]
    return angles, cols


def lantern():
    x, y = HANG[0], HANG[1]
    L = (x, y, 0)
    parts = []
    zb, zt = 2.27, 2.83       # glass bottom/top (body centre 2.55)
    rb, rt = 0.22, 0.27
    ang, cols = panes(6, 10, TEAL, offset=0.0)   # hexagon corners (mullions) at 0, 60, ... degrees
    zm = 2.5
    rm = rb + (rt - rb) * (zm - zb) / (zt - zb)
    parts.append(lathe("glass_lo", [(rb, zb), (rm, zm - 0.012)], TEAL, "flat", angles=ang, band_cols={0: cols},
                       cap_bottom=False, cap_top=False, loc=L, smooth=False))
    parts.append(lathe("glass_hi", [(rm + 0.003, zm + 0.012), (rt, zt)], TEAL, "flat", angles=ang, band_cols={0: cols},
                       cap_bottom=False, cap_top=False, loc=L, smooth=False))
    parts.append(lathe("muntin", [(rm - 0.005, zm - 0.022), (rm + 0.014, zm - 0.014), (rm + 0.016, zm + 0.014),
                                  (rm - 0.002, zm + 0.022)], IRON, "metal", segments=6, cap_bottom=False, cap_top=False,
                       loc=L, smooth=False))
    for k in range(6):
        a = math.radians(k * 60)
        parts.append(rod(f"cpost{k}", (x + math.cos(a) * rb * 1.04, y + math.sin(a) * rb * 1.04, zb - 0.02),
                         (x + math.cos(a) * rt * 1.04, y + math.sin(a) * rt * 1.04, zt + 0.02), 0.018, IRON, sides=6,
                         chamfer=0))
    # base: frame ring, bowl, glowing drop crystal
    parts.append(lathe("bowl", [(0.0, 2.12), (0.04, 2.13), (0.08, 2.17), (0.2, 2.225), (0.25, 2.245), (0.25, 2.28),
                                (0.21, 2.285)], IRON, "metal", segments=12, cap_bottom=False, cap_top=True,
                       cap_colors=(IRON, TEAL), cap_mats=("metal", "flat"), loc=L, sharp_bands=(3, 4)))
    parts.append(lathe("drop", [(0.0, 1.97), (0.04, 2.03), (0.05, 2.07), (0.033, 2.11), (0.0, 2.13)], TEAL, "flat",
                       segments=6, loc=L, smooth=False))
    # top frame ring + flared roof with curled corner tips
    parts.append(lathe("topring", [(rt - 0.01, zt - 0.005), (rt + 0.03, zt), (rt + 0.035, zt + 0.032), (rt, zt + 0.037)],
                       IRON, "metal", segments=6, cap_bottom=False, cap_top=True, cap_colors=(IRON, TEAL),
                       cap_mats=("metal", "flat"), loc=L, smooth=False))
    roof = [(0.29, zt + 0.03), (0.37, zt + 0.03), (0.38, zt + 0.06), (0.28, zt + 0.11), (0.15, zt + 0.19),
            (0.08, zt + 0.235), (0.07, zt + 0.26), (0.085, zt + 0.275), (0.0, zt + 0.28)]
    parts.append(lathe("roof", roof, IRON, "metal", segments=6, cap_bottom=True, cap_top=False, loc=L, smooth=False,
                       cap_colors=(IRON, IRON)))
    for k in range(6):
        a = math.radians(k * 60)
        d = Vector((math.cos(a), math.sin(a), 0))
        base = Vector((x, y, 0)) + d * 0.37 + Vector((0, 0, zt + 0.06))
        pts = [base, base + d * 0.04 + Vector((0, 0, 0.035))]
        for j in range(4):
            b = math.radians(-90 + j * 80)
            c = base + d * 0.04 + Vector((0, 0, 0.08))
            pts.append(c + d * math.cos(b) * -0.035 * (1 - j / 9) + Vector((0, 0, math.sin(b) * -0.035 * (1 - j / 9))))
        parts.append(sweep(f"tip{k}", [tuple(p) for p in pts], [0.017, 0.016] + [0.014 - 0.002 * j for j in range(4)],
                           color=IRON, mat="metal", sides=4))
    parts.append(blob("rknob", 0.045, IRON, mat="metal", loc=(x, y, zt + 0.3), segments=8, rings=5, noise_amp=0.0,
                      axis="Z"))
    ring = K.torus("hanger", 0.05, 0.013, color=IRON, mat="metal", major_seg=10, minor_seg=5,
                   loc=(x, y, CZ - 0.085), rot=(90, 0, 0))
    parts.append(ring)
    parts.append(rod("hangrod", (x, y, zt + 0.3), (x, y, CZ - 0.13), 0.012, IRON, sides=5, chamfer=0))
    return parts


def build():
    K.reset()
    base_parts = pad() + moss_and_leaves() + post()
    glow_parts = lantern() + runes(True) + footing_rune(True) + runestones(True)
    if "--stats" in ARGS:
        stats = {}
        for o in base_parts + glow_parts:
            key = "".join(ch for ch in o.name if not ch.isdigit() and ch not in ".-_True False")[:10]
            stats[key] = stats.get(key, 0) + sum(len(p.vertices) - 2 for p in o.data.polygons)
        print("ASSET stats", sorted(stats.items(), key=lambda kv: -kv[1]))
    base = K.merge(base_parts, "travel_gate_base")
    lant = K.merge(glow_parts, "travel_gate_lantern")
    groups = [([base], "props/travel_gate_base", "travel_gate_base"),
              ([lant], "props/travel_gate_lantern", "travel_gate_lantern")]
    res = export_many(groups, ao_radius=0.6, ao_strength=0.85)
    K.review_sheet([base, lant], "travel_gate_combined")
    if COMBINED:
        # merge copies of both meshes into one object for the single-asset engine preview
        me = base.data.copy()
        me2 = lant.data.copy()
        ob1 = K._link(bpy.data.objects.new("pv1", me))
        ob2 = K._link(bpy.data.objects.new("pv2", me2))
        for o in (base, lant):
            o.hide_set(True)
        both = K.merge([ob1, ob2], "travel_gate_base")
        K.export([both], "props/travel_gate_base")
        print("COMBINED preview export written to props/travel_gate_base")
    return res


build()
