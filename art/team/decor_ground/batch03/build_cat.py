"""black_cat_sitting: cute black cat sitting upright, ~0.35 m tall, big glowing yellow-green eyes.

blender --background --factory-startup --python art/team/decor_ground/batch03/build_cat.py

Pear body leaning forward, round head (big, cute proportions), pointed ears with pink inners,
front legs and paws, haunches with peeking hind paws, and a tail rising in a question-mark curl
on the cat's right (viewer's left, -X). Eyes are glow_firefly decals with ink slit pupils and a
white glint. Front faces -Y.
"""
import pathlib
import sys

sys.path.insert(0, str(pathlib.Path(__file__).resolve().parent))
from decor_common import *  # noqa: E402,F401,F403

FUR = "cat_black"
FURMAT = "felt"


def ear(bm, base, out, fwd, size=0.07, width=0.038):
    """Three-sided pointed ear: base centre, outward (up/side) direction, forward direction."""
    out = Vector(out).normalized()
    fwd = Vector(fwd).normalized()
    side = out.cross(fwd).normalized()
    b = Vector(base)
    tip = bm.verts.new(b + out * size + fwd * 0.008)
    fl = bm.verts.new(b - side * width + fwd * width * 0.45)
    fr = bm.verts.new(b + side * width + fwd * width * 0.45)
    bk = bm.verts.new(b - fwd * width * 0.7)
    front = bm.faces.new((fl, fr, tip))
    s1 = bm.faces.new((fr, bk, tip))
    s2 = bm.faces.new((bk, fl, tip))
    for f in (front, s1, s2):
        f.normal_update()
    if front.normal.dot(fwd) < 0:
        for f in (front, s1, s2):
            f.normal_flip()
    # pink inner: a shrunken copy of the front face, lifted off it
    c = (fl.co + fr.co + tip.co) / 3
    n = front.normal.copy()
    pts = [c + (v.co - c) * 0.62 + n * 0.004 + (tip.co - c) * -0.06 for v in (fl, fr, tip)]
    inner = bm.faces.new([bm.verts.new(p) for p in pts])
    inner.normal_update()
    if inner.normal.dot(n) < 0:
        inner.normal_flip()
    proj = F.uvl(bm)
    for f in (front, s1, s2, inner):
        for loop in f.loops:
            loop[proj].uv = (loop.vert.co.x * 4, loop.vert.co.z * 4)
    F.paint(bm, (front, s1, s2), FUR, FURMAT)
    F.paint(bm, (inner,), "cheek_pink", "flat")


def black_cat_sitting():
    K.reset()
    parts = []
    body = Y.lathe("body", [(0.0, 0.0), (0.066, 0.0), (0.092, 0.022), (0.099, 0.058), (0.093, 0.098),
                            (0.079, 0.138), (0.063, 0.178), (0.05, 0.212), (0.0, 0.232)],
                   colors=FUR, mats=FURMAT, segments=10, cap_bottom=False, cap_top=False)
    Y.transform(body, loc=(0, 0.02, 0), rot=(12, 0, 0), scale=(1.1, 1.15, 1.0))
    parts.append(body)
    head = Y.blob("head", 0.086, FUR, mat=FURMAT, loc=(0, -0.035, 0.268), scale=(1.16, 0.96, 0.9), segments=12,
                  rings=7, noise_amp=0.0, axis="X", uvmode="box")
    parts.append(head)
    # cheek fluff
    for sx in (-1, 1):
        parts.append(Y.blob(f"cheek{sx}", 0.04, FUR, mat=FURMAT, loc=(sx * 0.06, -0.06, 0.238),
                            scale=(1.2, 0.9, 0.8), segments=6, rings=4, noise_amp=0.0, axis="X", uvmode="box"))
    # front legs + paws
    for sx in (-1, 1):
        parts.append(Y.rod(f"leg{sx}", (sx * 0.033, -0.058, 0.14), (sx * 0.036, -0.078, 0.02), 0.02, FUR,
                           mat=FURMAT, sides=5, chamfer=0.0))
        parts.append(Y.blob(f"paw{sx}", 0.025, FUR, mat=FURMAT, loc=(sx * 0.037, -0.088, 0.015),
                            scale=(1.0, 1.3, 0.62), segments=6, rings=4, noise_amp=0.0, axis="X", uvmode="box"))
        # haunch and the hind paw peeking out in front of it
        parts.append(Y.blob(f"haunch{sx}", 0.062, FUR, mat=FURMAT, loc=(sx * 0.064, 0.035, 0.058),
                            scale=(0.8, 1.25, 0.95), segments=9, rings=5, noise_amp=0.0, axis="Y", uvmode="box"))
        parts.append(Y.blob(f"hind{sx}", 0.023, FUR, mat=FURMAT, loc=(sx * 0.082, -0.03, 0.013),
                            scale=(0.95, 1.6, 0.6), segments=6, rings=4, noise_amp=0.0, axis="X", uvmode="box"))
    for ob in parts:
        for v in ob.data.vertices:
            v.co.z = max(v.co.z, 0.0)
    cull_inside(list(parts), list(parts))
    # tail: from the rump round the cat's right side, rising in a question-mark curl
    path = Y.catmull([(0.0, 0.09, 0.035), (-0.07, 0.12, 0.03), (-0.13, 0.08, 0.05), (-0.15, 0.05, 0.13),
                      (-0.14, 0.05, 0.22), (-0.155, 0.06, 0.29), (-0.2, 0.07, 0.31), (-0.215, 0.07, 0.28)], per=2)
    n = len(path)
    parts.append(Y.sweep("tail", path, [0.019 - 0.007 * (i / (n - 1)) for i in range(n)], color=FUR, mat=FURMAT,
                         sides=5, caps=(False, True)))
    # ears
    bm = F.bm_new()
    for sx in (-1, 1):
        a = math.radians(sx * 38)
        base = Vector((math.sin(a) * 0.086 * 1.05, -0.035, 0.268 + math.cos(a) * 0.086 * 0.82))
        ear(bm, base, (math.sin(a) * 0.8, 0.05, 1.0), (sx * 0.1, -1, 0.15))
    parts.append(mesh_from(bm, "ears", smooth=False))
    # face decals projected onto the head
    tree = bvh(head)
    bm = F.bm_new()

    def at(x, z):
        h = tree.ray_cast(Vector((x, -1.0, z)), Vector((0, 1, 0)))
        return h[0], h[1].normalized()

    for sx in (-1, 1):
        p, nrm = at(sx * 0.042, 0.284)
        decal(bm, tree, p, nrm, oval(0.028, 0.032, 10, rot=sx * -0.25), "glow_firefly", lift=0.003, dome=0.0015)
        p, nrm = at(sx * 0.042, 0.283)
        decal(bm, tree, p, nrm, oval(0.0095, 0.026, 8), "ink", lift=0.0075)
        p, nrm = at(sx * 0.042 + 0.009, 0.296)
        decal(bm, tree, p, nrm, oval(0.0055, 0.0065, 6), "eye_white", lift=0.0105)
    p, nrm = at(0.0, 0.256)
    decal(bm, tree, p, nrm, [(-0.011, 0.004), (0.011, 0.004), (0.0, -0.008)], "cheek_pink", lift=0.004)
    parts.append(mesh_from(bm, "face", smooth=False))
    ob = K.merge(parts, "black_cat_sitting")
    return finish([ob], "props/black_cat_sitting", "black_cat_sitting", ao_radius=0.15, ao_strength=0.7)


if __name__ == "__main__":
    black_cat_sitting()
