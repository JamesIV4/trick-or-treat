"""ghost_figure: friendly sheet-ghost porch decoration (~0.9 m) on a little wooden stand.

blender --background --factory-startup --python art/team/decor_ground/batch03/build_ghost.py

The sheet is one smooth surface of revolution with 7 soft radial folds that grow toward a wavy
hem, a shadowed underside, stubby flared arms, and a conforming decal face (ink eyes with glints,
open smile, pink cheeks). Front faces -Y.
"""
import pathlib
import sys

sys.path.insert(0, str(pathlib.Path(__file__).resolve().parent))
from decor_common import *  # noqa: E402,F401,F403

FOLDS = 6
SEG = 24
FRONT = -math.pi / 2  # -Y


def sheet():
    prof = [(0.885, 0.06), (0.862, 0.112), (0.825, 0.155), (0.775, 0.186), (0.715, 0.202), (0.645, 0.207),
            (0.575, 0.205), (0.505, 0.21), (0.44, 0.224), (0.375, 0.244), (0.31, 0.268), (0.25, 0.288),
            (0.2, 0.312), (0.16, 0.332)]
    top_z = 0.9
    bm = F.bm_new()
    proj = F.uvl(bm)

    def fold(z):
        return max(0.0, min(1.0, (0.56 - z) / 0.4)) ** 1.15

    rings = []
    for i, (z, r) in enumerate(prof):
        ring = []
        last = i == len(prof) - 1
        for j in range(SEG):
            a = TAU * j / SEG + FRONT
            crest = math.cos(FOLDS * (a - FRONT) + 0.6)  # a fold crest just off the centre front
            rr = r * (1 + 0.11 * fold(z) * crest)
            zz = z
            if last:
                zz = z - 0.045 * crest - 0.01
            elif i == len(prof) - 2:
                zz = z - 0.018 * crest
            ring.append(bm.verts.new((math.cos(a) * rr, math.sin(a) * rr, zz)))
        rings.append(ring)
    pole = bm.verts.new((0, 0, top_z))
    # arc length for v
    s = [0.0]
    for i in range(1, len(prof)):
        s.append(s[-1] + math.hypot(prof[i][0] - prof[i - 1][0], prof[i][1] - prof[i - 1][1]))
    outer = []
    for j in range(SEG):
        j2 = (j + 1) % SEG
        f = bm.faces.new((pole, rings[0][j], rings[0][j2]))
        outer.append(f)
    for i in range(len(rings) - 1):
        for j in range(SEG):
            j2 = (j + 1) % SEG
            outer.append(bm.faces.new((rings[i][j], rings[i + 1][j], rings[i + 1][j2], rings[i][j2])))
    # underside: hem turns in and up into a shadowed hollow
    hem = rings[-1]
    inner = [bm.verts.new((v.co.x * 0.86, v.co.y * 0.86, v.co.z + 0.035)) for v in hem]
    hollow = bm.verts.new((0, 0, 0.32))
    under, dark = [], []
    for j in range(SEG):
        j2 = (j + 1) % SEG
        under.append(bm.faces.new((hem[j], inner[j], inner[j2], hem[j2])))
        dark.append(bm.faces.new((inner[j], hollow, inner[j2])))
    bmesh.ops.recalc_face_normals(bm, faces=bm.faces)
    # outward normals for the shell; the hollow must face down
    bm.normal_update()
    for f in dark + under:
        if f.normal.z > 0:
            f.normal_flip()
    for f in outer:
        c = f.calc_center_median()
        if f.normal.dot(Vector((c.x, c.y, c.z - 0.5))) < 0:
            f.normal_flip()
    for f in bm.faces:
        for loop in f.loops:
            co = loop.vert.co
            a = math.atan2(co.y, co.x)
            loop[proj].uv = (a * 0.22, co.z * 0.8 + 0.33)
        angs = [math.atan2(l.vert.co.y, l.vert.co.x) for l in f.loops]
        if max(angs) - min(angs) > math.pi:
            for l, a in zip(f.loops, angs):
                l[proj].uv = ((a + TAU if a < 0 else a) * 0.22, l.vert.co.z * 0.8 + 0.33)
    F.paint(bm, bm.faces, "ghost_white", "vinyl")
    F.paint(bm, under, "silver", "vinyl")
    F.paint(bm, dark, "stone_dark", "flat")
    return F.to_object(bm, "sheet", smooth=True, sharp=None)


def ghost_figure():
    K.reset()
    parts = []
    body = sheet()
    parts.append(body)
    # stubby arms: flared sheet corners at the sides, drooping a little
    for sx in (-1, 1):
        arm = Y.blob(f"arm{sx}", 0.07, "ghost_white", mat="vinyl", loc=(sx * 0.24, 0.0, 0.5),
                     scale=(1.9, 0.62, 0.55), rot=(0, sx * 18, sx * -10), segments=10, rings=6, noise_amp=0.0,
                     axis="X", uvmode="box")
        parts.append(arm)
    # stand: round wooden base and a dowel up into the sheet
    base = Y.lathe("base", [(0.0, 0.0), (0.155, 0.0), (0.168, 0.012), (0.168, 0.032), (0.155, 0.046), (0.0, 0.046)],
                   colors=["wood_dark", "wood_dark", "wood", "wood", "wood"], mats="wood", segments=16,
                   cap_bottom=False, cap_top=False)
    parts.append(base)
    parts.append(Y.rod("dowel", (0, 0, 0.04), (0, 0, 0.36), 0.022, "wood", mat="wood", sides=8))
    # face decals, projected onto the head
    tree = bvh(body)
    bm = F.bm_new()

    def at(x, z):
        h = tree.ray_cast(Vector((x, -1.0, z)), Vector((0, 1, 0)))
        return h[0], h[1].normalized()

    for sx in (-1, 1):
        p, n = at(sx * 0.068, 0.69)
        decal(bm, tree, p, n, oval(0.03, 0.043, 12), "ink", lift=0.003, dome=0.004)
        p2, n2 = at(sx * 0.068 + 0.011, 0.708)
        decal(bm, tree, p2, n2, oval(0.009, 0.012, 6), "eye_white", lift=0.0065)
        p3, n3 = at(sx * 0.118, 0.622)
        decal(bm, tree, p3, n3, oval(0.026, 0.015, 8), "cheek_pink", lift=0.003)
    p, n = at(0.0, 0.612)
    decal(bm, tree, p, n, oval(0.034, 0.03, 12, cut="top"), "ink", lift=0.003)
    p, n = at(0.0, 0.597)
    decal(bm, tree, p, n, oval(0.017, 0.009, 8), "cheek_pink", lift=0.0055)
    parts.append(mesh_from(bm, "face", smooth=False))
    ob = K.merge(parts, "ghost_figure")
    return finish([ob], "props/ghost_figure", "ghost_figure", ao_radius=0.35)


if __name__ == "__main__":
    ghost_figure()
