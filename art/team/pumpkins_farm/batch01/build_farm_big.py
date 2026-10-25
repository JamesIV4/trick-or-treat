"""hay_wagon, scarecrow.

blender --background --factory-startup --python art/team/pumpkins_farm/batch01/build_farm_big.py [-- ids...]
"""
import math
import pathlib
import random
import sys

HERE = pathlib.Path(__file__).resolve().parent
sys.path.insert(0, str(HERE))
import bpy  # noqa: E402
from mathutils import Vector  # noqa: E402

import pumpkin_lib as P  # noqa: E402

K = P.K


def straw_fan(prefix, base, direction, count=3, length=0.12, radius=0.025, spread=0.5, seed=0,
              colors=("straw", "straw_dark")):
    """A little fan of blunt straw spikes poking out of a cuff."""
    rnd = random.Random(seed)
    d = Vector(direction).normalized()
    side = d.cross(Vector((0, 0, 1)))
    if side.length < 1e-3:
        side = d.cross(Vector((1, 0, 0)))
    side.normalize()
    up = side.cross(d).normalized()
    out = []
    for i in range(count):
        a = (i - (count - 1) / 2) * spread + rnd.uniform(-0.15, 0.15)
        b = rnd.uniform(-0.4, 0.4)
        dd = (d + side * math.tan(a) * 0.6 + up * b * 0.5).normalized()
        p0 = Vector(base) - dd * 0.03
        p1 = Vector(base) + dd * (length * rnd.uniform(0.8, 1.2))
        out.append(K.tube(f"{prefix}{i}", [p0, p1], radius=radius, radius_end=radius * 0.4,
                          color=colors[i % len(colors)], sides=4, smooth=False))
    return out


# ----------------------------------------------------------------------------- hay wagon
def hay_wagon():
    K.reset()
    parts = []
    BL, BW, top = 2.1, 1.1, 0.7
    bed = K.box("bed", (BL, BW, 0.12), color="plank", loc=(0, 0, top - 0.06), bevel=0.02, segments=1)
    P.cleanup(bed)
    K.paint(bed, "wood_light", where=lambda c, n: n.z > 0.9)
    parts.append(bed)
    # plank seams on the bed floor (flush, so the bed stays a flat landing)
    for y in (-0.275, 0.0, 0.275):
        parts.append(P.slab(f"seam{y}", (BL - 0.1, 0.02, 0.004), loc=(0, y, top + 0.002), color="wood",
                            omit=("-z", "+x", "-x")))
    # red sides with white top trim
    rh = 0.3
    for sy in (-1, 1):
        rail = K.box(f"rail{sy}", (BL, 0.06, rh), color="metal_red", loc=(0, sy * (BW / 2 - 0.03), top + rh / 2),
                     bevel=0.015, segments=1)
        P.cleanup(rail)
        parts.append(rail)
        parts.append(P.slab(f"trim{sy}", (BL + 0.02, 0.08, 0.04), loc=(0, sy * (BW / 2 - 0.03), top + rh + 0.02),
                            color="trim_white", omit=("-z",)))
        # white cross brace on the outside of each side panel
        for x0 in (-0.5, 0.5):
            for s in (-1, 1):
                ang = math.degrees(math.atan2(rh - 0.06, 0.8)) * s
                parts.append(P.slab(f"brace{sy}{x0}{s}", (0.86, 0.012, 0.05),
                                    loc=(x0, sy * (BW / 2 + 0.003), top + rh / 2), color="trim_white",
                                    omit=("+y" if sy < 0 else "-y",), rot=(0, ang, 0)))
    for sx in (-1, 1):
        end = K.box(f"end{sx}", (0.06, BW - 0.12, rh), color="metal_red", loc=(sx * (BL / 2 - 0.03), 0, top + rh / 2),
                    bevel=0.015, segments=1)
        P.cleanup(end)
        parts.append(end)
        parts.append(P.slab(f"etrim{sx}", (0.08, BW - 0.1, 0.04), loc=(sx * (BL / 2 - 0.03), 0, top + rh + 0.02),
                            color="trim_white", omit=("-z",)))
    # under-frame: bolsters and axles
    wr, wx, wy = 0.32, 0.68, 0.6
    for sx in (-1, 1):
        parts.append(P.slab(f"bolster{sx}", (0.16, BW - 0.1, 0.12), loc=(sx * wx, 0, top - 0.18), color="wood_dark",
                            omit=("+z",)))
        parts.append(K.cylinder(f"axle{sx}", 0.04, 2 * wy, color="iron", verts=6, rot=(90, 0, 0),
                                loc=(sx * wx, 0, wr)))
        for sy in (-1, 1):
            parts.append(K.cylinder(f"tyre{sx}{sy}", wr, 0.13, color="charcoal", verts=12, rot=(90, 0, 0),
                                    loc=(sx * wx, sy * wy, wr)))
            parts.append(K.cylinder(f"hub{sx}{sy}", 0.17, 0.15, color="metal_red", verts=8, rot=(90, 0, 0),
                                    loc=(sx * wx, sy * wy, wr)))
            parts.append(K.cylinder(f"cap{sx}{sy}", 0.06, 0.17, color="trim_white", verts=6, rot=(90, 0, 0),
                                    loc=(sx * wx, sy * wy, wr)))
    # tongue with a T handle out the front (-X)
    parts.append(K.tube("tongue", [(-wx, 0, wr + 0.02), (-BL / 2 - 0.05, 0, 0.28), (-BL / 2 - 0.25, 0, 0.22)],
                        radius=0.045, color="wood", sides=5, smooth=False))
    parts.append(K.cylinder("handle", 0.035, 0.42, color="wood_dark", verts=5, rot=(90, 0, 0),
                            loc=(-BL / 2 - 0.26, 0, 0.22)))
    # hay heap in the back half, plus a bale and two pumpkins riding up front
    heap = K.ico("heap", 0.5, color="hay", subdiv=1, scale=(1.25, 0.98, 0.8), loc=(0.42, 0.02, top + 0.06),
                 smooth=True)
    rnd = random.Random(8)

    def lump(co):
        f = 1.0 + 0.08 * math.sin(co.x * 13 + 1) * math.cos(co.y * 11)
        out = co * f
        if out.z < -0.1:
            out.z = -0.1
        if out.z > 0.25:  # flatten the crown into a landable top
            out.z = 0.25 + (out.z - 0.25) * 0.3
        return out

    K.deform(heap, lump)
    K.paint(heap, "straw", where=lambda c, n: rnd.random() < 0.35)
    K.paint(heap, "straw_dark", where=lambda c, n: rnd.random() < 0.15)
    parts.append(heap)
    for i, (b, d) in enumerate((((0.15, -0.3, 1.0), (-0.5, -0.6, 0.6)), ((0.7, 0.25, 1.06), (0.4, 0.5, 0.8)),
                                ((0.85, -0.2, 1.0), (1, -0.2, 0.5)))):
        parts += straw_fan(f"hs{i}", b, d, count=3, length=0.14, radius=0.03, seed=i)
    bale = K.box("bale", (0.62, 0.42, 0.34), color="straw", loc=(-0.42, 0.2, top + 0.17), bevel=0.05, segments=1,
                 rot=(0, 0, 12))
    P.cleanup(bale)
    K.paint(bale, "hay", where=lambda c, n: max(abs(n.x), abs(n.y), abs(n.z)) < 0.97)
    parts.append(bale)
    for k, (x, y, z, w, col) in enumerate(((-0.8, -0.2, top, 0.36, "pumpkin"), (-0.47, 0.18, top + 0.34, 0.28, "pumpkin_light"))):
        pb = P.pumpkin_body(f"pk{k}", width=w, height=w * 0.75, lobes=6, groove=0.13, groove_w=0.14, peaks=0,
                            rings=4, color=col, groove_color="pumpkin_dark", fullness=0.7, flatness=0.7,
                            top_dimple=0.025, seed=40 + k, loc=(x, y, z))
        bpy.context.view_layer.update()
        tp = P.top_point(pb)
        parts.append(pb)
        parts.append(K.cylinder(f"pks{k}", 0.025, 0.08, color="stem_brown", verts=4, radius_top=0.018,
                                loc=(tp.x, tp.y, tp.z + 0.03), rot=(10, -14, 0)))
    ob = K.merge(parts, "hay_wagon")
    return P.finish([ob], "props/hay_wagon", "hay_wagon", close=True)


# ----------------------------------------------------------------------------- scarecrow
def scarecrow():
    K.reset()
    parts = []
    # post and crossbar
    parts.append(K.box("post", (0.1, 0.1, 1.55), color="wood", loc=(0, 0.06, 0.775), bevel=0.012, segments=1))
    parts.append(K.box("bar", (1.36, 0.08, 0.08), color="wood", loc=(0, 0.06, 1.34), bevel=0.012, segments=1))
    for p in parts:
        P.cleanup(p)
    # little straw skirt at the foot of the post
    parts += straw_fan("foot", (0, 0.06, 0.04), (0, -1, 0.15), count=3, length=0.12, radius=0.03, spread=0.9, seed=3)
    parts += straw_fan("footb", (0, 0.06, 0.04), (0, 1, 0.15), count=2, length=0.1, radius=0.03, spread=0.9, seed=4)
    # shirt: plaid torso (alternating columns + a dark band)
    torso = P.lathe("torso", [(0.2, 0.86), (0.235, 0.98), (0.235, 1.2), (0.215, 1.34), (0.12, 1.41)], segs=10,
                    color="flannel_red", column_colors=["flannel_red", "flannel_red", "flannel_dark"],
                    band_colors=[None, None, "flannel_dark", None], cap_bottom=False, smooth=False)
    parts.append(torso)
    # sleeves along the crossbar, flaring at the cuffs, with a patch
    for sx in (-1, 1):
        parts.append(P.blob_tube(f"sleeve{sx}", [(sx * 0.14, 0.03, 1.3), (sx * 0.34, 0.04, 1.33), (sx * 0.55, 0.04, 1.33)],
                                 [0.1, 0.085, 0.1], sides=8, color="flannel_red", stripe_color="flannel_dark",
                                 stripe_every=3, smooth=False, cap_offset=0.05))
        parts.append(K.cylinder(f"cuff{sx}", 0.095, 0.06, color="flannel_dark", verts=8, rot=(0, 90, 0),
                                loc=(sx * 0.57, 0.04, 1.33)))
        parts += straw_fan(f"hand{sx}", (sx * 0.6, 0.04, 1.33), (sx, 0, 0), count=3, length=0.18, radius=0.036,
                           spread=0.45, seed=10 + sx)
    parts.append(P.slab("patch", (0.11, 0.02, 0.1), loc=(0.36, -0.04, 1.36), color="quilt_blue", rot=(0, 8, 0)))
    # overalls: hip barrel, bib, straps, gold buttons
    parts.append(P.lathe("hips", [(0.215, 0.74), (0.245, 0.8), (0.25, 0.96), (0.243, 1.02)], segs=10, color="denim",
                         cap_top=False, smooth=False))
    parts.append(P.slab("bib", (0.26, 0.04, 0.24), loc=(0, -0.215, 1.1), color="denim", omit=("+y",)))
    parts.append(P.slab("pocket", (0.12, 0.012, 0.08), loc=(0, -0.24, 1.12), color="witch_purple", omit=("+y",)))
    for sx in (-1, 1):
        parts.append(K.tube(f"strap{sx}", [(sx * 0.1, -0.22, 1.2), (sx * 0.11, -0.2, 1.34), (sx * 0.1, 0.0, 1.42),
                                           (sx * 0.1, 0.2, 1.3)], radius=0.022, color="denim", sides=4, smooth=False))
        parts.append(K.cylinder(f"btn{sx}", 0.025, 0.02, color="foil_gold", verts=6, rot=(90, 0, 0),
                                loc=(sx * 0.1, -0.243, 1.2)))
    # dangling legs with knee patch and straw cuffs
    for sx in (-1, 1):
        a = (sx * 0.11, -0.02, 0.8)
        b = (sx * 0.15, -0.1, 0.5)
        c = (sx * 0.16, -0.06, 0.32)
        parts.append(P.blob_tube(f"leg{sx}", [a, b, c], [0.09, 0.08, 0.095], sides=8, color="denim", smooth=False,
                                 cap_offset=0.05))
        parts += straw_fan(f"ankle{sx}", (sx * 0.165, -0.06, 0.28), (sx * 0.2, -0.2, -1), count=3, length=0.11,
                           radius=0.026, spread=0.5, seed=20 + sx)
    parts.append(P.slab("knee", (0.1, 0.02, 0.1), loc=(-0.15, -0.19, 0.52), color="quilt_yellow", rot=(-15, 0, 0)))
    # neck tie and burlap head
    parts.append(K.cylinder("gather", 0.1, 0.08, color="cardboard", verts=8, radius_top=0.07, loc=(0, 0, 1.44)))
    parts.append(K.torus("tie", 0.085, 0.022, color="rope", major_seg=8, minor_seg=4, loc=(0, 0, 1.45)))
    head = K.sphere("head", 0.23, color="cardboard", segments=10, rings=7, scale=(1.05, 0.95, 1.0), loc=(0, 0, 1.65))
    K.deform(head, lambda co: Vector((co.x, co.y, co.z * (1.0 if co.z > 0 else 0.92))))
    parts.append(head)
    fy = -0.215  # face plane
    for sx in (-1, 1):
        parts.append(K.cylinder(f"eye{sx}", 0.04, 0.03, color="ink", verts=8, rot=(90, 0, 0),
                                loc=(sx * 0.075, fy + 0.005, 1.68)))
        parts.append(K.cylinder(f"glint{sx}", 0.012, 0.01, color="eye_white", verts=4, rot=(90, 0, 0),
                                loc=(sx * 0.075 + 0.014, fy - 0.012, 1.695)))
        parts.append(K.cylinder(f"cheek{sx}", 0.032, 0.01, color="cheek_pink", verts=6, rot=(90, 0, sx * 25),
                                loc=(sx * 0.125, fy + 0.035, 1.6)))
    # stitched smile: an arc of thread with little cross stitches
    smile = []
    for i in range(7):
        t = i / 6
        x = -0.09 + 0.18 * t
        z = 1.59 - 0.035 * math.sin(math.pi * t)
        y = -math.sqrt(max(0.0, 0.215 ** 2 - x * x)) - 0.004
        smile.append((x, y, z))
    parts.append(K.tube("smile", smile, radius=0.009, color="charcoal", sides=4, smooth=False))
    for i in (1, 3, 5):
        x, y, z = smile[i]
        parts.append(P.slab(f"st{i}", (0.008, 0.01, 0.04), loc=(x, y - 0.002, z), color="charcoal", omit=("+y",)))
    parts.append(K.cylinder("nose", 0.028, 0.06, color="pumpkin", verts=5, radius_top=0.008, rot=(90, 0, 0),
                            loc=(0, -0.24, 1.645)))
    # floppy straw hat with a band and a tucked sunflower
    brim = K.cylinder("brim", 0.34, 0.03, color="straw", verts=14, loc=(0, 0.03, 1.83), rot=(-15, 0, 0))

    def flop(co):
        a = math.atan2(co.y, co.x)
        r = math.hypot(co.x, co.y)
        droop = 0.05 * math.sin(3 * a + 0.5) * (r / 0.33) - 0.04 * (r / 0.33) ** 2
        return Vector((co.x, co.y, co.z + droop))

    K.deform(brim, flop)
    parts.append(brim)
    crown = K.cylinder("crown", 0.17, 0.17, color="straw", verts=10, radius_top=0.14, loc=(0, 0.05, 1.93),
                       rot=(-15, 4, 0))
    K.paint(crown, "straw_dark", where=lambda c, n: n.z > 0.9)
    parts.append(crown)
    parts.append(K.cylinder("band", 0.172, 0.05, color="banner_purple", verts=10, radius_top=0.165, loc=(0, 0.045, 1.875),
                            rot=(-15, 4, 0)))
    parts.append(K.cylinder("flower", 0.06, 0.02, color="sunflower_yellow", verts=8, rot=(70, 0, 30),
                            loc=(0.115, -0.1, 1.92)))
    parts.append(K.cylinder("fcentre", 0.025, 0.03, color="sunflower_center", verts=6, rot=(70, 0, 30),
                            loc=(0.117, -0.106, 1.922)))
    parts += straw_fan("hair", (0, -0.13, 1.82), (0, -1, -0.5), count=3, length=0.1, radius=0.025, spread=0.6, seed=7)
    ob = K.merge(parts, "scarecrow")
    return P.finish([ob], "props/scarecrow", "scarecrow", close=True)


BUILDS = {f.__name__: f for f in (hay_wagon, scarecrow)}

if __name__ == "__main__":
    args = sys.argv[sys.argv.index("--") + 1:] if "--" in sys.argv else list(BUILDS)
    for a in args:
        BUILDS[a]()
