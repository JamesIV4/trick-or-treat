"""brick_planter and brick_pillar.

blender --background --factory-startup --python art/team/decor_ground/batch03/build_brick.py [-- ids...]

Brick walls carry the painted `brick` material at ~8 cm courses (UVs in world metres x BRICK_UV, so
courses line up round corners and between parts). Caps are separate bevelled stone slabs with
hairline gaps (`concrete` material on stone_light), as in the concept's raised beds and pillars.
"""
import pathlib
import sys

sys.path.insert(0, str(pathlib.Path(__file__).resolve().parent))
from decor_common import *  # noqa: E402,F401,F403
import build_mums as M  # noqa: E402

BRICK_UV = 2.3  # brick layer: 1.5 m per tile, 8 courses -> ~8 cm courses


def brick_block(name, size, loc, color="brick", bevel=0.012, drop_bottom=True):
    ob = Y.box(name, size, color, mat="brick", loc=loc, bevel=bevel, segments=1, smooth=False)
    Y.bake(ob)
    K.project(ob, "box")
    F.scale_uv(ob, BRICK_UV)
    if drop_bottom:
        bm = bmesh.new()
        bm.from_mesh(ob.data)
        dead = [f for f in bm.faces if f.calc_center_median().z < 1e-4 and abs(f.normal.z) > 0.99]
        bmesh.ops.delete(bm, geom=dead, context="FACES")
        bm.to_mesh(ob.data)
        bm.free()
    F.auto_smooth(ob, 40)
    return ob


def slab(name, x0, x1, y0, y1, z0, z1, color="stone_light", bevel=0.014):
    ob = Y.box(name, (x1 - x0, y1 - y0, z1 - z0), color, mat="concrete",
               loc=((x0 + x1) / 2, (y0 + y1) / 2, (z0 + z1) / 2), bevel=bevel, segments=1, smooth=False)
    Y.bake(ob)
    K.project(ob, "box")
    F.auto_smooth(ob, 40)
    return ob


def ivy(name, pts, leaf_every=1, wall_n=(0, -1, 0), seed=0, size=0.055):
    """Ivy strand: thin vine sweep through pts with small leaves lying against the wall."""
    rnd = random.Random(seed)
    pts = Y.catmull(pts, per=2)
    parts = [F.sweep(name, pts, 0.008, sides=4, color="vine_dark", mat="flat", cap_start=False, cap_end=True,
                     smooth=True)]
    bm = F.bm_new()
    n = Vector(wall_n).normalized()
    P = [Vector(p) for p in pts]
    for i in range(1, len(P), leaf_every):
        t = (P[min(i + 1, len(P) - 1)] - P[i - 1]).normalized()
        side = t.cross(n).normalized() * (1 if i % 2 else -1)
        d = (side * 0.8 + t * rnd.uniform(-0.2, 0.5) + n * 0.12).normalized()
        leaf(bm, P[i] + n * 0.006, d, size * rnd.uniform(0.85, 1.2), size * 0.9, up=n,
             color=("vine", "lawn", "moss")[i % 3], under="vine_dark", droop=-0.06, rib=0.12, mat="leaves")
    parts.append(mesh_from(bm, name + "_lv", smooth=True, sharp=None))
    return parts


# ------------------------------------------------------------------------------------------ planter
def brick_planter():
    K.reset()
    L, W, H = 2.0, 0.9, 0.55
    cap_t, over = 0.07, 0.025
    zc = H - cap_t
    parts = []
    # brick walls: one closed block (top hidden by the cap and soil)
    parts.append(brick_block("walls", (L - 2 * over, W - 2 * over, zc), (0, 0, zc / 2)))
    # soil fill, gridded so AO from the plants lands on it
    bm = F.bm_new()
    xi, yi = L / 2 - over - 0.13, W / 2 - over - 0.13
    nx, ny = 8, 3
    grid = [[bm.verts.new((-xi + 2 * xi * i / nx, -yi + 2 * yi * j / ny, H - 0.04)) for j in range(ny + 1)]
            for i in range(nx + 1)]
    for i in range(nx):
        for j in range(ny):
            bm.faces.new((grid[i][j], grid[i + 1][j], grid[i + 1][j + 1], grid[i][j + 1]))
    K._project_bm(bm, "box")
    F.paint(bm, bm.faces, "choco_dark", "soil")
    parts.append(F.to_object(bm, "soil", smooth=True))
    # cap course: four long slabs per side, one end slab each end, hairline gaps
    g = 0.005
    xo, yo = L / 2, W / 2
    w = 0.14 + over + 0.02
    xs = [-xo + k * (2 * xo) / 4 for k in range(5)]
    tones = ["stone_light", "stone_light", "sidewalk", "stone_light"]
    for side in (-1, 1):
        for k in range(4):
            y0, y1 = (yo - w, yo) if side > 0 else (-yo, -yo + w)
            parts.append(slab(f"cap{side}{k}", xs[k] + g, xs[k + 1] - g, y0, y1, zc, H,
                              color=tones[(k + (side > 0)) % 4]))
        x0, x1 = (xo - w, xo) if side > 0 else (-xo, -xo + w)
        parts.append(slab(f"end{side}", x0, x1, -yo + w + g, yo - w - g, zc, H, color="stone_light"))
    # plants: three mum clusters and two little pumpkins in the soil bed
    soil_z = H - 0.04
    beds = [("mums_orange", (-0.58, 0.03), 0.98, 3), ("mums_purple", (0.17, 0.06), 0.92, 7),
            ("mums_white", (0.64, -0.04), 0.8, 11)]
    for aid, (x, y), sc, seed in beds:
        v = M.VARIANTS[aid]
        parts += M.mum_cluster(f"{aid}_p", v["colors"], v["top"], seed + 20, n_heads=21, loc=(x, y, soil_z - 0.03),
                               scale=sc, n_leaves=6, segs=9, rings=6)
    # leafy filler mounds so little bare soil shows (back corners, between the plants)
    for k, (x, y, r, sd) in enumerate(((-0.12, 0.17, 0.15, 1), (0.42, 0.18, 0.12, 2), (-0.83, -0.16, 0.09, 3),
                                       (0.86, 0.17, 0.08, 4))):
        b = Y.blob(f"fill{k}", r, "hedge", mat="hedge", loc=(x, y, soil_z + r * 0.35), scale=(1.15, 1.0, 0.8),
                   segments=8, rings=5, lobes=5, lobe_amp=0.18, noise_amp=0.06, seed=sd + 30, axis="Z", uvmode="box")
        Y.paint_world(b, "lawn_dark", lambda c, n: n.z > 0.6)
        sit_on_ground(b, soil_z)
        parts.append(b)
    # leaves spilling over the cap's inner edge (break the hard line like the concept)
    bm = F.bm_new()
    rnd = random.Random(8)
    for k in range(12):
        side = -1 if k % 3 else 1
        x = -0.85 + 1.7 * (k + 0.5) / 12 + rnd.uniform(-0.04, 0.04)
        yb = side * (W / 2 - w - 0.01)
        d = Vector((rnd.uniform(-0.4, 0.4), side * 1.0, rnd.uniform(0.05, 0.3)))
        leaf(bm, (x, yb, soil_z + 0.06), d, rnd.uniform(0.11, 0.15), rnd.uniform(0.08, 0.1),
             color="vine" if k % 2 else "hedge", under="hedge_dark", droop=0.45, mat="hedge")
    parts.append(mesh_from(bm, "spill", smooth=True))
    for k, (x, y, wdt, col, rz) in enumerate(((-0.17, -0.08, 0.38, "pumpkin", 20), (0.43, -0.06, 0.28, "pumpkin_dark", 70))):
        pb = F.pumpkin_body(f"pk{k}", width=wdt, height=wdt * 0.72, lobes=6, spl=3, rings=7, groove=0.14, plump=0.5,
                            color=col, fullness=0.7, flatness=0.72, top_dimple=0.03, lumps=0.04, seed=40 + k,
                            belly=1.15, loc=(x, y, soil_z - 0.015), rot_z=rz)
        bpy.context.view_layer.update()
        tp = F.top_point(pb)
        parts.append(pb)
        parts.append(F.stem(f"pks{k}", tp, height=0.08, radius=0.022, curl=0.04, lean_dir=k * 2.3 + 0.5,
                            color="stem_brown" if k == 0 else "stem_green", rings=4, ridges=3, uv_scale=4.0,
                            sink=0.012))
    # ivy spilling over the front cap and down the wall at the right end
    pts = [(0.78, -0.36, H + 0.02), (0.8, -0.43, H + 0.025), (0.82, -0.462, H - 0.02), (0.84, -0.457, 0.44),
           (0.81, -0.457, 0.37), (0.85, -0.457, 0.3), (0.88, -0.457, 0.22), (0.86, -0.457, 0.14), (0.89, -0.457, 0.07)]
    parts += ivy("ivy", pts, wall_n=(0, -1, 0), seed=5, size=0.085)
    pts = [(-0.95, -0.2, H + 0.02), (-1.005, -0.22, H + 0.015), (-1.008, -0.24, 0.43), (-1.007, -0.2, 0.34),
           (-1.007, -0.25, 0.25)]
    parts += ivy("ivy2", pts, wall_n=(-1, 0, 0), seed=6, size=0.08)
    ob = K.merge(parts, "brick_planter")
    return finish([ob], "props/brick_planter", "brick_planter", ao_radius=0.5, weighted=True)


# ------------------------------------------------------------------------------------------ pillar
def brick_pillar():
    K.reset()
    parts = []
    S, Hs = 0.38, 0.97
    parts.append(brick_block("plinth", (S + 0.03, S + 0.03, 0.16), (0, 0, 0.08)))
    parts.append(brick_block("shaft", (S, S, Hs - 0.16 + 0.01), (0, 0, 0.16 + (Hs - 0.16 + 0.01) / 2 - 0.005),
                             drop_bottom=False))
    parts.append(slab("cap", -0.225, 0.225, -0.225, 0.225, Hs, Hs + 0.08, bevel=0.018))
    parts.append(slab("top", -0.19, 0.19, -0.19, 0.19, Hs + 0.08, 1.1, color="stone_light", bevel=0.015))
    # a short ivy strand climbing the front-left corner
    pts = [(-0.17, -0.212, 0.02), (-0.15, -0.197, 0.12), (-0.12, -0.197, 0.22), (-0.15, -0.197, 0.32),
           (-0.11, -0.197, 0.42), (-0.13, -0.197, 0.52), (-0.09, -0.197, 0.6)]
    parts += ivy("ivy", pts, wall_n=(0, -1, 0), seed=9, size=0.08)
    ob = K.merge(parts, "brick_pillar")
    K.socket("top", (0, 0, 1.1), ob)
    return finish([ob], "props/brick_pillar", "brick_pillar", ao_radius=0.4, weighted=True)


if __name__ == "__main__":
    if want("brick_planter"):
        brick_planter()
    if want("brick_pillar"):
        brick_pillar()
