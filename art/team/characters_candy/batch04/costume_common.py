"""Helpers shared by the costume builders (cloth panels, tubes, stitches, bones, bows, hood eyes,
hood warps, leaf outlines)."""
import math

import bmesh
from mathutils import Matrix

import kid_kit as KK
from kid_kit import D, K, V, FELT


# ================================================================== small shared builders
def tube(name, ctrl, radii, per=2, sides=8, color="ink", mat=FELT, project="sphere", up=None, paint=None):
    pts, par = KK.catmull(ctrl, per=per)
    rr = [KK.lerp_list(radii, t) for t in par]
    return D.sweep(name, pts, rr, sides=sides, color=color, mat=mat, project=project, up=up, paint=paint), pts, rr


def panel(name, P, nu, nv, th, out_color, in_color, out_mat=FELT, in_mat=FELT, out_hint=(0, 1, 0), out_paint=None):
    """Thick cloth panel from a surface P(u, v) -> point (u, v in 0..1). The outer side faces roughly
    along out_hint (per-vertex hint function allowed), the inner side gets in_color. out_paint(i, j) ->
    colour or None recolours outer faces (trims)."""
    grid = [[V(P(i / nu, j / nv)) for j in range(nv + 1)] for i in range(nu + 1)]

    def normal(i, j):
        a = grid[min(i + 1, nu)][j] - grid[max(i - 1, 0)][j]
        b = grid[i][min(j + 1, nv)] - grid[i][max(j - 1, 0)]
        n = a.cross(b).normalized()
        h = V(out_hint(i / nu, j / nv) if callable(out_hint) else out_hint)
        return n if n.dot(h) >= 0 else -n
    bm = bmesh.new()
    top = [[bm.verts.new(grid[i][j] + normal(i, j) * th / 2) for j in range(nv + 1)] for i in range(nu + 1)]
    bot = [[bm.verts.new(grid[i][j] - normal(i, j) * th / 2) for j in range(nv + 1)] for i in range(nu + 1)]
    outer, inner = [], []
    for i in range(nu):
        for j in range(nv):
            outer.append(bm.faces.new((top[i][j], top[i + 1][j], top[i + 1][j + 1], top[i][j + 1])))
            inner.append(bm.faces.new((bot[i][j], bot[i][j + 1], bot[i + 1][j + 1], bot[i + 1][j])))
    border = [(i, 0) for i in range(nu)] + [(nu, j) for j in range(nv)] + \
             [(i, nv) for i in range(nu, 0, -1)] + [(0, j) for j in range(nv, 0, -1)]
    for k in range(len(border)):
        i0, j0 = border[k]
        i1, j1 = border[(k + 1) % len(border)]
        bm.faces.new((top[i0][j0], bot[i0][j0], bot[i1][j1], top[i1][j1]))
    bmesh.ops.recalc_face_normals(bm, faces=bm.faces)
    # recalc may flip the whole shell; make the outer faces face along the hint
    bm.normal_update()
    i_m, j_m = nu // 2, nv // 2
    f = outer[(i_m) * nv + j_m]
    h = V(out_hint(0.5, 0.5) if callable(out_hint) else out_hint)
    if f.normal.dot(h) < 0:
        bmesh.ops.reverse_faces(bm, faces=bm.faces)
    rest = [f for f in bm.faces if f not in outer and f not in inner]
    extra = [(inner, in_color, in_mat), (rest, in_color, in_mat)]
    if out_paint:
        groups = {}
        for i in range(nu):
            for j in range(nv):
                c = out_paint(i, j)
                if c:
                    groups.setdefault(c, []).append(outer[i * nv + j])
        extra += [(fs, c, out_mat) for c, fs in groups.items()]
    return D.mesh(bm, name, out_color, out_mat, "box", smooth=True, extra=extra)


def stitch_line(name, pts_nrm, color, length=0.017, width=0.006):
    items = []
    for k in range(len(pts_nrm) - 1):
        p, n = pts_nrm[k]
        q, _ = pts_nrm[k + 1]
        items.append(((p + q) / 2, q - p, n))
    return D.stitches(name, items, length=length, width=width, height=0.003, color=color)


def bone(name, a, b, nrm, r=0.011, color="bone", flat=0.6, knob=1.45):
    """A cartoon bone (shaft + two knobs at each end) lying on a surface between a and b."""
    a, b = V(a), V(b)
    L = (b - a).length
    shaft = K.cylinder(name + "_s", r, L, color=color, mat="flat", verts=6, rot=(90, 0, 0))
    ps = [shaft]
    for sy in (1, -1):
        for sx in (1, -1):
            ps.append(K.sphere(f"{name}_k{sy}{sx}", r * knob, color=color, mat="flat", segments=6, rings=4,
                               loc=(sx * r * 1.05, sy * L / 2, 0)))
    ob = K.merge(ps, name)
    ob.data.transform(Matrix.Diagonal((1, 1, flat, 1)))
    D.put(ob, (a + b) / 2 + V(nrm).normalized() * r * flat * 0.5, nrm, up=(b - a))
    return ob


def bow(name, c, nrm, up, color, s=1.0):
    """Little ribbon bow: two lobes and a knot (local Z along nrm)."""
    ps = []
    for sx in (1, -1):
        lobe = KK.blob(f"{name}_l{sx}", (0, 0, 0), (0.034 * s, 0.024 * s, 0.014 * s), segs=8, rings=5, color=color, mat=FELT)
        lobe.location = (sx * 0.03 * s, 0, 0)
        lobe.rotation_euler = (0, 0, math.radians(-12 * sx))
        ps.append(lobe)
        tail = KK.blob(f"{name}_t{sx}", (0, 0, 0), (0.01 * s, 0.026 * s, 0.008 * s), segs=6, rings=4, color=color, mat=FELT)
        tail.location = (sx * 0.013 * s, -0.03 * s, -0.004 * s)
        tail.rotation_euler = (0, 0, math.radians(20 * sx))
        ps.append(tail)
    ps.append(KK.blob(f"{name}_k", (0, 0, 0), (0.014 * s, 0.014 * s, 0.013 * s), segs=7, rings=4, color=color, mat=FELT))
    ob = K.merge(ps, name)
    D.put(ob, c, nrm, up=up)
    return ob


def hood_eyes(kind, color, pupil="ink", d=(0.34, -0.52, 0.8), size=1.0, roll=12, at=None, tree=None, face_fwd=0.0):
    """Eyes on the hood brow like the dragon's. kind: 'cat' (almond, slit pupil), 'hole' (ink oval).
    at=(x, z) + tree (BVH of the hood shell) casts them onto the real (possibly warped) hood front;
    face_fwd tilts them toward the front so they read from the side/front cameras."""
    parts = []
    for sx in (1, -1):
        if at:
            p, n = KK.on_surface(tree, (at[0] * sx, -0.6, at[1]), (0, 1, 0))
        else:
            p, n = KK.ellipsoid_dir(KK.HC, KK.HR, (d[0] * sx, d[1], d[2]))
        ns = n
        n = (n + V((0, -face_fwd, 0))).normalized()
        if kind == "hole":
            W, H = 0.05 * size, 0.064 * size
            e = KK.blob(f"heye{sx}", (0, 0, 0), (W, H, 0.022), segs=12, rings=6, color=color, mat="flat")
            D.put(e, p - ns * 0.012, n, roll=-roll * sx)
            parts.append(e)
            continue
        W, H, Dp = 0.044 * size, 0.032 * size, 0.016
        e = KK.blob(f"heye{sx}", (0, 0, 0), (W, H, Dp), segs=12, rings=6, color=color, mat="flat")
        D.put(e, p - n * 0.006, n, roll=-roll * sx)
        M = D.look(n, roll=-roll * sx)
        pc = p - n * 0.006 + M @ V((0, 0, Dp * 0.95))
        pu = KK.blob(f"hpupil{sx}", (0, 0, 0), (0.0085 * size, 0.026 * size, 0.005), segs=8, rings=4, color=pupil, mat="flat")
        D.put(pu, pc, n, roll=-roll * sx)
        hl = KK.blob(f"hhl{sx}", (0, 0, 0), (0.0075, 0.0075, 0.003), segs=6, rings=3, color="eye_white", mat="flat")
        D.put(hl, pc + M @ V((-0.016 * size, 0.008 * size, -0.002)), n, roll=-roll * sx)
        parts += [e, pu, hl]
    return parts


def dome(k, fwd=0.0, z0=0.97):
    """Hood shell warp: stretch everything above the face opening upward (taller sheet/skull dome) and
    push the front of it forward into a brow that faces the camera (room for big eyes / sockets)."""
    def w(p):
        if p.z > z0:
            front = max(0.0, -(p.y - KK.HC.y) / KK.HR.y)
            dz = p.z - z0
            p = V((p.x, p.y - fwd * front * min(dz, 0.07) * (1.0 - min(dz, 0.098) / 0.2), p.z + dz * k))
        return p
    return w


def back_y(x, z, off):
    """A Y just behind the torso's back at (x, z), offset 'off' (cloth hanging behind the body)."""
    R = max(KK.torso_r(z), 0.17) + off
    if abs(x) >= R:
        return 0.012 + off * 0.5
    return 0.012 + 0.88 * math.sqrt(R * R - x * x) + off * 0.12


# ================================================================== PUMPKIN
def leaf_outline(size, lobes=5):
    """Palmate pumpkin leaf outline (CCW), stem end at the origin, pointing +Y."""
    out = []
    n = 30
    for i in range(n):
        th = -math.pi * 0.92 + 1.84 * math.pi * i / (n - 1)
        lob = 0.68 + 0.32 * abs(math.cos(lobes * th / 2))
        r = size * lob
        out.append((math.sin(th) * r, 0.5 * size + math.cos(th) * r * 0.95))
    return out
