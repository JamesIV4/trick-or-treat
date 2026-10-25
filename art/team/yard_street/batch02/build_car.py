"""parked_car (v2): bubbly toy hatchback, front faces -Y, roof is a platform.

The body is one smooth loft of superellipse cross-sections split at the beltline (lower body /
upper cabin), so windows are clean grid-aligned panels painted on the cabin columns.
"""
import pathlib
import sys

sys.path.insert(0, str(pathlib.Path(__file__).resolve().parent))
from yard_common import *  # noqa: E402,F401,F403

BODY = "house_sky"
GLASS = "water_deep"
TRIM = "stone_light"
NU = 14  # faces per half ring


def smooth_keys(keys, y):
    """Smoothstep interpolation through [(y, value)]."""
    if y <= keys[0][0]:
        return keys[0][1]
    for (y0, v0), (y1, v1) in zip(keys, keys[1:]):
        if y0 <= y <= y1:
            t = (y - y0) / (y1 - y0)
            t = t * t * (3 - 2 * t)
            return v0 + (v1 - v0) * t
    return keys[-1][1]


TOP = [(-1.8, 0.95), (-1.5, 1.08), (-1.32, 1.14), (-1.2, 1.17), (-0.6, 1.56), (0.2, 1.62), (0.95, 1.56), (1.5, 1.32), (1.8, 1.05)]
BELT = [(-1.8, 0.86), (-1.0, 0.92), (1.8, 0.94)]
L, END0 = 1.8, 1.22


def ring(y):
    w = 0.83
    zb = 0.36
    zt = smooth_keys(TOP, y)
    belt = smooth_keys(BELT, y)
    cab = max(0.0, min(1.0, (zt - belt) / 0.55))
    tumble = 0.33 * cab
    pts = []
    nu, nl = 1.8, 2.8
    for k in range(NU + 1):  # upper half, right belt point -> left belt point
        th = math.pi * k / NU
        c, s = math.cos(th), math.sin(th)
        x = w * math.copysign(abs(c) ** (2 / nu), c) * (1 - tumble * s ** 1.4)
        z = belt + (zt - belt) * s ** (2 / nu)
        pts.append([x, z])
    for k in range(1, NU):  # lower half
        th = math.pi + math.pi * k / NU
        c, s = math.cos(th), math.sin(th)
        x = w * math.copysign(abs(c) ** (2 / nl), c)
        z = belt - (belt - zb) * abs(s) ** (2 / nl)
        pts.append([x, z])
    # round the nose and tail: shrink the section toward its centre
    a = abs(y)
    if a > END0:
        t = min(1.0, (a - END0) / (L - END0))
        f = math.sqrt(max(0.0, 1 - (t * 0.93) ** 2))
        zc = (zb + zt) / 2 * 0.5 + belt * 0.5
        for p in pts:
            p[0] *= f
            p[1] = zc + (p[1] - zc) * (0.55 + 0.45 * f)
    return [tuple(p) for p in pts]


YS = [-1.8, -1.77, -1.71, -1.62, -1.5, -1.36, -1.22, -1.02, -0.82, -0.62, -0.35, -0.05, 0.15, 0.27, 0.55,
      0.8, 1.05, 1.25, 1.4, 1.52, 1.63, 1.72, 1.78, 1.8]


def glass(i, j):
    """Face colour for loft band i (between YS[i], YS[i+1]) and ring face j."""
    y0, y1 = YS[i], YS[i + 1]
    ym = (y0 + y1) / 2
    upper = j < NU
    if not upper:
        return None
    side_r = j in (1, 2, 3)
    side_l = j in (NU - 2, NU - 3, NU - 4)
    top = 4 <= j <= NU - 5
    if -1.22 <= ym <= -0.62 and (top or j in (3, NU - 4)):  # windshield
        return (GLASS, "flat")
    if -0.62 <= ym <= 1.05 and (side_r or side_l) and not (0.15 <= ym <= 0.27):  # side windows, B pillar
        return (GLASS, "flat")
    if 1.05 <= ym <= 1.52 and (top or j in (3, NU - 4)):  # rear hatch window
        return (GLASS, "flat")
    return None


def wheel(name, x, y, r=0.42, wdt=0.36):
    sx = 1 if x > 0 else -1
    parts = []
    prof = [(0.2, -wdt / 2), (0.31, -wdt / 2 - 0.005), (r - 0.03, -wdt / 2 + 0.01), (r, -wdt / 2 + 0.07),
            (r, wdt / 2 - 0.07), (r - 0.03, wdt / 2 - 0.01), (0.31, wdt / 2 + 0.005), (0.2, wdt / 2)]
    tyre = lathe(f"{name}_tyre", prof, "charcoal", "flat", segments=14, cap_bottom=True, cap_top=True,
                 cap_colors=("charcoal", "charcoal"))
    transform(tyre, loc=(x, y, r), rot=(0, 90, 0))
    parts.append(tyre)
    hub = lathe(f"{name}_hub", [(0.22, 0.0), (0.21, 0.03), (0.14, 0.06), (0.0, 0.075)], "silver", "metal",
                segments=10, cap_bottom=False)
    transform(hub, loc=(x + sx * (wdt / 2 - 0.005), y, r), rot=(0, 90 * sx, 0))
    parts.append(hub)
    return parts


def arch(name, x, y, r=0.53):
    """Body-coloured fender flare over a wheel (half torus in the YZ plane)."""
    ob = K.torus(name, r, 0.07, color=BODY, mat="metal", major_seg=9, minor_seg=5, arc=180.0)
    transform(ob, loc=(x, y, 0.42), rot=(90, 0, 90), scale=(1.0, 1.0, 1.25))
    return ob


def surface(body, origin, direction):
    from mathutils.bvhtree import BVHTree
    me = body.data
    bvh = BVHTree.FromPolygons([v.co for v in me.vertices], [list(p.vertices) for p in me.polygons])
    hit = bvh.ray_cast(Vector(origin), Vector(direction).normalized(), 10.0)
    return hit[0] if hit[0] is not None else Vector(origin)


def parked_car():
    K.reset()
    parts = []
    body = loft("body", [(y, ring(y)) for y in YS], BODY, mat="metal", colors=glass)
    parts.append(body)
    front = {sx: surface(body, (sx * 0.47, -3, 0.74), (0, 1, 0)) for sx in (-1, 1)}
    back = {sx: surface(body, (sx * 0.55, 3, 0.8), (0, -1, 0)) for sx in (-1, 1)}
    grille_p = surface(body, (0, -3, 0.58), (0, 1, 0))
    for sx in (-1, 1):
        for sy in (-1, 1):
            parts += wheel(f"w{sx}{sy}", sx * 0.72, sy * 1.12)
            parts.append(arch(f"arch{sx}{sy}", sx * 0.79, sy * 1.12))
    # bumpers: soft capsules wrapping the nose and tail
    for sy in (-1, 1):
        pts = [(-0.7, sy * 1.7, 0.42), (-0.4, sy * 1.84, 0.42), (0.0, sy * 1.87, 0.42), (0.4, sy * 1.84, 0.42),
               (0.7, sy * 1.7, 0.42)]
        parts.append(sweep(f"bumper{sy}", catmull(pts, per=2), 0.085, color=TRIM, mat="metal", sides=8,
                           up_hint=(0, 0, 1)))
    for sx in (-1, 1):
        hl = blob(f"headlight{sx}", 0.15, "silver", mat="metal", loc=tuple(front[sx] + Vector((0, 0.02, 0))),
                  scale=(1.0, 0.55, 0.92), segments=10, rings=6, noise_amp=0.0, axis="Y")
        paint_world(hl, "charcoal", lambda c, n: n.y > -0.55)  # dark bezel ring around the lens
        parts.append(hl)
        parts.append(box(f"taillight{sx}", (0.22, 0.08, 0.15), "metal_red", "flat", loc=tuple(back[sx] + Vector((0, -0.02, 0))),
                         bevel=0.03, segments=2))
        mirror = blob(f"mirror{sx}", 0.09, BODY, mat="metal", loc=(sx * 0.8, -0.82, 1.06), scale=(0.9, 0.55, 0.65),
                      segments=8, rings=5, noise_amp=0.0)
        parts.append(mirror)
        parts.append(box(f"handle{sx}", (0.03, 0.14, 0.035), TRIM, "metal", loc=(sx * 0.84, 0.15, 0.86), bevel=0.012,
                         segments=1))
    parts.append(box("grille", (0.5, 0.08, 0.12), "charcoal", "metal", loc=tuple(grille_p + Vector((0, 0.02, 0))), bevel=0.035, segments=2))
    parts.append(box("plate_f", (0.36, 0.03, 0.12), "cream", "flat", loc=(0, -1.88, 0.42), bevel=0.012, segments=1))
    parts.append(box("plate_r", (0.36, 0.03, 0.12), "cream", "flat", loc=(0, 1.88, 0.42), bevel=0.012, segments=1))
    parts.append(rod("exhaust", (0.45, 1.6, 0.32), (0.45, 1.86, 0.32), 0.04, "silver", sides=8))
    car = K.merge(parts, "parked_car")
    ground(car)
    return finish([car], "props/parked_car", "parked_car", ao_radius=0.6)


if want("parked_car"):
    parked_car()
