"""kid_ghost v2: the Bedsheet Ghost on kit v2 (design: batch04/ref/ref_ghost.png).

A kid under ONE white bedsheet (FIDELITY_PASS decision 15): a rounded hood with two high black oval
sheet-eyes and the face showing through a soft round opening, the kid's own face v2 and brown fringe,
the sheet falling in one continuous drape from the crown to ONE plain soft wavy hem (below the knees,
the same all the way round whatever the arms do).
ARM HOLES (user, 2026-11-15: "make arm holes for the ghost kid's arms, not try to drape over the kid's
arms"): the sheet has a round hole at each side just under the hood, with a rolled turned-over edge and
a short lining, and the kid's own arms come out through them. The arms' pivots (arm_l / arm_r origins)
sit IN the holes (SHOULDER_L / _R, moved out from the kit's shoulders by shift_arms), so an arm turns in
its hole; its shoulder ball fills the hole from every angle. The cloth never follows the arms (no tenting,
no lift): the sheet's rigid top (the "sheet" preset in kid_rig.cpp) reaches below the holes. No sleeves: the arms are the kid's plain dark plum long-sleeve top with a rolled cuff, bare
hands, dark plum leggings, chunky purple high-tops. No wings.

ONE drape, two parts. Both parts sample the same drape surface (drape_r), so their slopes, folds and
cells agree across the join:
  soft_cowl  (under head) the crown, the face opening and its rolled rim, the sheet-eyes, and the drape
             down to a thin edge at ZC (just under the shoulders). Origin at the neck (the head's own
             pivot). Its lower part is counter-rotated against the head: statically against the baked
             head tilt here (pre_counter), and every frame against the head's pose in kid_rig.cpp (the
             "cowl" preset, COWL_COUNTER_*), so the edge always lies on the body sheet.
  soft_sheet (under body) the same drape from SHEET_TOP (inside the cowl) to the hem, SHEET_OFF inside
             the cowl's surface where they overlap and easing back out onto it below the cowl's edge.
             Origin at neck height on the body axis; it bends below its rigid top and ripples at the hem.

Colour: the sheet is ghost_cloth, flat (ghost_sheet and then ghost_shade both clipped to
flat white under the showcase lights), the inside ghost_deep, with soft fold shading painted into the
baked AO (KK.AO_SHAPE).
"""
import math

import bmesh
import bpy
from mathutils import Matrix, Quaternion

import kid_kit as KK
from kid_kit import D, V
from costume_common import dome

SH, SHD, SHX, LEG = "ghost_cloth", "ghost_deep", "ghost_shade", "ghost_legging"
CLOTH = "flat"      # (the fabric layer is a plaid check: a blanket, not a bedsheet)
NU = 72                       # columns round the sheet and the cowl
YO = 0.012                    # the drape's axis: (0, YO) in Blender X/Y (the hood centre)
NECK = V(KK.NECK)
SHEET_PIN = NECK              # both cloth parts share one mesh space (origin at the neck)
# The game's default arm pose for this costume (= KidTuning arm_out / elbow_rest for kid_ghost in
# kid_rig.cpp; build_costumes.py --poses renders kid_ghost_out.png in it): change both together.
ARM_OUT, ELBOW_REST = 0.42, 0.3
# The arm holes: one at each side (angle 0 = the kid's left, pi = the right), centred HOLE_Z high, below
# the hood's join (ZC) with room for the rolled edge. HOLE_R = the shoulder ball (ARM_R) + clearance.
HOLE_A = {"l": 0.0, "r": math.pi}
HOLE_Z = 0.462
ARM_R = 0.052                 # the upper arm's radius at the shoulder (its ball fills the hole)
HOLE_R = 0.060
RIM_T = 0.0075                # the rolled edge's tube radius
# The arm's pivot sits this far OUTSIDE the sheet's outer surface on the hole's axis: an upper arm tilted
# about 35 degrees off the hole's axis (hanging, or raised) then still leaves the cloth inside the hole; the
# shoulder ball (ARM_R) still fills the opening but for a ring the lining covers.
PIVOT_OUT = 0.012
UPPER_ARM, FOREARM = 0.6, 0.85   # the upper arm's and the forearm's length, as a share of the kit's
COWL_PIN = NECK              # the head's pivot: the counter-rotation undoes the head's turn about it
# The cowl's counter-rotation ramp, below the neck: none above R0, all of it from R0 + LEN down. Must
# match COWL_COUNTER_R0 / COWL_COUNTER_LEN in native/src/play/kid_rig.cpp.
COUNTER_R0, COUNTER_LEN = 0.012, 0.048
ZC = 0.552                    # the cowl's lower edge (a gentle wave round it)
SHEET_TOP = 0.622             # the body sheet's top, hidden inside the cowl
COWL_T = 0.003                # cowl thickness (a thin edge lying on the sheet)
SHEET_OFF = 0.0025            # the sheet lies this far inside the drape where the cowl's tucked edge crosses it
SHOE = dict(upper="witch_purple", panel="candy_grape", toe="lolly_white", sole="lolly_white", stripe="candy_grape",
            lace="candy_orange", collar="silver", tongue="witch_purple", heel="candy_orange")


def adiff(a, b):
    return math.atan2(math.sin(a - b), math.cos(a - b))


def mixr(a, side, front, back):
    """Value at angle a (a = -pi/2 is the front, Blender -Y) from side / front / back values."""
    sf = max(0.0, -math.sin(a)) ** 2
    sb = max(0.0, math.sin(a)) ** 2
    return side + (front - side) * sf + (back - side) * sb


def smooth(t):
    t = min(max(t, 0.0), 1.0)
    return t * t * (3 - 2 * t)


def fold(a):
    """Irregular soft vertical folds round the sheet (about -1..1)."""
    return 0.55 * math.sin(7 * a + 0.4) + 0.3 * math.sin(12 * a + 2.1) + 0.22 * math.sin(4 * a + 1.0)


def fold_amp(z):
    """Fold depth: a hint under the head, deep toward the hem."""
    t = min(max((0.62 - z) / 0.38, 0.0), 1.0)
    return 0.002 + 0.026 * t ** 1.4


def hem_z(a):
    """ONE plain hem all the way round: below the knees, a little higher in front (the leggings show),
    lower at the sides and back. The arms do not change it."""
    return (0.218 + 0.012 * math.sin(4 * a + 0.7) + 0.005 * math.sin(9 * a + 1.9) - 0.012 * math.cos(a) ** 2
            + 0.026 * max(0.0, -math.sin(a)) ** 2)


def hem_r(a):
    return mixr(a, 0.372, 0.325, 0.358)


def fold_z(a, z, zt):
    return z


def pt(a, r, z):
    return V((math.cos(a) * r, math.sin(a) * r + YO, z))


# ------------------------------------------------------------------ the one drape surface
HOOD_WARP = dome(0.55, 1.4)
N_UP = 15


def hood_col(a):
    """One column of the hood over the head, crown to where the drape takes over (at the sides and back
    the hood's widest line, in front just under the face opening): (points, zt, rt, front)."""
    x0 = math.cos(a) * 0.25
    front = 0.0
    zt = KK.HC.z
    if math.sin(a) < 0 and abs(x0) < KK.OA * 1.08:
        u = x0 / (KK.OA * 1.08)
        zt = min(KK.HC.z, KK.OZ - KK.OC * math.sqrt(1 - u * u) - 0.012)
        front = (1 - u * u) * min(1.0, -math.sin(a) * 1.5)
    rr = 1.0 / math.sqrt((math.cos(a) / KK.HR.x) ** 2 + (math.sin(a) / KK.HR.y) ** 2)
    ph_t = math.acos(max(-1.0, min(1.0, (zt - KK.HC.z) / KK.HR.z)))
    col = []
    for k in range(N_UP):
        ph = 0.09 + (ph_t - 0.09) * k / (N_UP - 1)
        p = V((math.cos(a) * rr * math.sin(ph), KK.HC.y + math.sin(a) * rr * math.sin(ph), KK.HC.z + KK.HR.z * math.cos(ph)))
        col.append(HOOD_WARP(p))
    rt = math.hypot(col[-1].x, col[-1].y - YO)
    return col, col[-1].z, rt, front


_COLS = {}


def column(i):
    if i not in _COLS:
        _COLS[i] = hood_col(2 * math.pi * i / NU)
    return _COLS[i]


def hermite(knots, z, t0):
    """Piecewise cubic through knots [(z, r)] (z falling): slope dr/d(-z) = t0 at the first knot,
    Catmull-Rom slopes inside, the last span's secant at the end."""
    n = len(knots)
    ts = [t0]
    for k in range(1, n):
        a, b = knots[k - 1], knots[min(k + 1, n - 1)]
        ts.append((b[1] - a[1]) / max(a[0] - b[0], 1e-4))
    if z >= knots[0][0]:
        return knots[0][1]
    for k in range(n - 1):
        (z0, r0), (z1, r1) = knots[k], knots[k + 1]
        if z >= z1 or k == n - 2:
            h = max(z0 - z1, 1e-4)
            s = min(max((z0 - z) / h, 0.0), 1.0)
            h00, h10, h01, h11 = 2 * s ** 3 - 3 * s * s + 1, s ** 3 - 2 * s * s + s, -2 * s ** 3 + 3 * s * s, s ** 3 - s * s
            return h00 * r0 + h10 * h * ts[k] + h01 * r1 + h11 * h * ts[k + 1]
    return knots[-1][1]


def drape_r(a, z, zt, rt, front):
    """Radius of the drape at angle a and height z (zt >= z >= hem): it leaves the hood straight down
    at the sides and back (vertical at the widest line, so there is no step), widens a little over the
    shoulders (the arm holes are there) and bells out to the hem; in front it falls from under the chin.
    Folds grow toward the hem."""
    zb, rb = hem_z(a), hem_r(a)
    r55 = max(rt + 0.006, mixr(a, 0.288, 0.215, 0.276))
    r43 = max(r55 + 0.008, mixr(a, 0.302, 0.262, 0.298))
    knots = [(zt, rt)]
    if zt > 0.6:
        knots.append((0.55, r55))
    knots += [(0.43, r43), (zb, rb)]
    v = min(max((zt - z) / max(zt - zb, 0.05), 0.0), 1.0)
    return hermite(knots, z, 0.9 * front) + fold_amp(fold_z(a, z, zt)) * fold(a) * min(1.0, v * 5)


def surf(p):
    """p moved radially (from the drape's axis) onto the drape surface."""
    a = math.atan2(p.y - YO, p.x)
    _, zt, rt, front = column(int(round(a / (2 * math.pi / NU))) % NU)
    return pt(a, drape_r(a, p.z, zt, rt, front), p.z)


class Hole:
    """An arm hole: centre c on the drape, n the drape's outward normal there, u round the sheet, v up it."""

    def __init__(self, side):
        a = HOLE_A[side]
        self.side, self.r = side, HOLE_R
        self.c = surf(pt(a, 0.3, HOLE_Z))
        du = surf(pt(a + 0.02, 0.3, HOLE_Z)) - surf(pt(a - 0.02, 0.3, HOLE_Z))
        dv = surf(pt(a, 0.3, HOLE_Z + 0.01)) - surf(pt(a, 0.3, HOLE_Z - 0.01))
        n = du.cross(dv).normalized()
        if n.dot(V((self.c.x, self.c.y - YO, 0.0))) < 0:
            n = -n
        self.n = n
        self.u = du.normalized()
        self.v = n.cross(self.u).normalized()
        if self.v.z < 0:
            self.v = -self.v

    def local(self, p):
        d = V(p) - self.c
        al = d.dot(self.n)
        return (d - self.n * al).length, al

    def on_edge(self, p, rr):
        d = V(p) - self.c
        d -= self.n * d.dot(self.n)
        if d.length < 1e-6:
            d = self.u.copy()
        return surf(self.c + d.normalized() * rr)

    def ring(self, rr, k=24):
        return [self.c + (self.u * math.cos(t) + self.v * math.sin(t)) * rr for t in (2 * math.pi * i / k for i in range(k))]


def hole_pieces(h):
    """The rolled, turned-over edge round a hole (a tube lying on the drape round the opening) and a short
    lining inside it (sheet colour, facing the arm), so nothing dark or see-through shows round the arm."""
    pts = [surf(q) + h.n * (RIM_T * 0.25) for q in h.ring(h.r + RIM_T * 0.45, 24)]
    rim = D.sweep(f"hole_rim_{h.side}", pts, RIM_T, sides=6, color=SH, mat=CLOTH, project="cyl", closed=True, center=h.c)
    bm = bmesh.new()
    k = 24
    rows = []
    for depth, rr in ((-RIM_T * 0.2, h.r - 0.001), (0.008, h.r - 0.002), (0.02, h.r - 0.004), (0.03, h.r - 0.006)):
        rows.append([bm.verts.new(surf(q) - h.n * depth) for q in h.ring(rr, k)])
    faces = []
    for j in range(len(rows) - 1):
        for i in range(k):
            i2 = (i + 1) % k
            faces.append(bm.faces.new((rows[j][i], rows[j][i2], rows[j + 1][i2], rows[j + 1][i])))
    bm.normal_update()
    f = faces[0]
    if f.normal.dot(h.c - f.calc_center_median()) < 0:     # face the arm (the hole's axis)
        bmesh.ops.reverse_faces(bm, faces=bm.faces)
    lining = D.mesh(bm, f"hole_lining_{h.side}", SH, CLOTH, "cyl", smooth=True, center=h.c)
    return [rim, lining]


# ------------------------------------------------------------------ a thick cloth shell
def shell(name, grid, thick, out_col, in_col, lip_col=None, inner_rows=None, lip=4, mat=CLOTH, pole=None, cut=None,
          holes=()):
    """A closed cloth shell from an outer grid[j][i] (j = 0 top .. last = hem, i round the axis, closed):
    the outside, a rolled lip at the hem, an inside wall 'thick' in from the outside (only the outer
    rows listed in inner_rows, bottom first), and a strip closing the top. Outside faces get out_col,
    the inside in_col, the lip lip_col. pole: a point above row 0 that closes the top of the outside
    (a hood's crown); cut(face centre) -> True deletes that outside face (the face opening).
    holes: [Hole]: round holes through both walls (the outer edge is laid on the circle; the rolled
    edge and the lining are separate pieces, see hole_pieces)."""
    nv, nu = len(grid) - 1, len(grid[0])
    nrm = []
    for j in range(nv + 1):
        row = []
        for i in range(nu):
            du = grid[j][(i + 1) % nu] - grid[j][(i - 1) % nu]
            dv = grid[min(j + 1, nv)][i] - grid[max(j - 1, 0)][i]
            n = du.cross(dv).normalized()
            radial = V((grid[j][i].x, grid[j][i].y - YO, 0.0))
            if n.dot(radial) < 0:
                n = -n
            row.append(n)
        nrm.append(row)
    rows, kinds = [], []
    for j in range(nv + 1):
        rows.append(list(grid[j]))
        kinds.append("out")
    for k in range(1, lip):
        th = math.pi * k / lip
        r = []
        for i in range(nu):
            d = (grid[nv][i] - grid[nv - 1][i]).normalized()
            r.append(grid[nv][i] - nrm[nv][i] * (thick / 2) * (1 - math.cos(th)) + d * (thick / 2) * math.sin(th) * 1.3)
        rows.append(r)
        kinds.append("lip")
    for j in (inner_rows or list(range(nv, -1, -1))):
        rows.append([grid[j][i] - nrm[j][i] * thick for i in range(nu)])
        kinds.append("in")
    bm = bmesh.new()
    vs = [[bm.verts.new(p) for p in r] for r in rows]
    groups = {"out": [], "lip": [], "in": []}
    for k in range(len(vs) if pole is None else len(vs) - 1):
        A, B = vs[k], vs[(k + 1) % len(vs)]
        kind = kinds[k] if k + 1 < len(vs) else "in"
        if kinds[k] == "out" and k + 1 < len(vs) and kinds[k + 1] == "lip":
            kind = "lip"
        for i in range(nu):
            i2 = (i + 1) % nu
            groups[kind].append(bm.faces.new((A[i], A[i2], B[i2], B[i])))
    if pole is not None:
        pv = bm.verts.new(V(pole))
        pin = bm.verts.new(V(pole) - V((0, 0, 0.03)))
        for i in range(nu):
            groups["out"].append(bm.faces.new((pv, vs[0][(i + 1) % nu], vs[0][i])))
            groups["in"].append(bm.faces.new((pin, vs[-1][i], vs[-1][(i + 1) % nu])))
    bmesh.ops.recalc_face_normals(bm, faces=bm.faces)
    bm.normal_update()
    f = groups["out"][len(groups["out"]) // 2]
    c = f.calc_center_median()
    if f.normal.dot(V((c.x, c.y - YO, 0.0))) < 0:
        bmesh.ops.reverse_faces(bm, faces=bm.faces)
    for h in holes:
        outer = set(groups["out"]) | set(groups["lip"])
        gone = []
        for f in bm.faces:
            rad, al = h.local(f.calc_center_median())
            if f in outer:
                if rad < h.r and abs(al) < 0.03:
                    gone.append(f)
            elif rad < h.r + 0.014 and -0.07 < al < 0.02:
                gone.append(f)          # the inside wall where the lining passes through it
        gs = set(gone)
        for k in groups:
            groups[k] = [f for f in groups[k] if f not in gs]
        bmesh.ops.delete(bm, geom=gone, context="FACES")
        outer = set(groups["out"])
        for v in bm.verts:
            rad, al = h.local(v.co)
            if v.is_boundary and rad < h.r * 1.8 and abs(al) < 0.03 and any(f in outer for f in v.link_faces):
                v.co = h.on_edge(v.co, h.r)     # a round opening
    if cut:
        gone = [f for f in groups["out"] if cut(f.calc_center_median())]
        groups["out"] = [f for f in groups["out"] if f not in set(gone)]
        bmesh.ops.delete(bm, geom=gone, context="FACES")
        # a clean opening: the cut edge is laid on the opening's own curve (under the rolled rim)
        for v in bm.verts:
            if v.is_boundary and v.co.y < KK.HC.y - 0.05 and v.co.z > 0.58:
                t = math.atan2((v.co.z - KK.OZ) / KK.OC, v.co.x / KK.OA)
                q, _ = KK.ellipsoid_pt(KK.HC, KK.HR, math.cos(t) * (KK.OA - 0.004), KK.OZ + math.sin(t) * (KK.OC - 0.004))
                v.co = q
    extra = [(groups["in"], in_col, mat), (groups["lip"], lip_col or out_col, mat)]
    return D.mesh(bm, name, out_col, mat, "cyl", smooth=True, center=(0, 0, 0.45), extra=extra)


def fold_shade(p, deep=0.32):
    """Soft fold shading for the baked AO: the hollows of the folds darker, growing toward the hem."""
    a = math.atan2(p.y - YO, p.x)
    zt = column(int(round(a / (2 * math.pi / NU))) % NU)[1]
    k = fold_amp(fold_z(a, min(p.z, zt), zt)) / 0.028
    return 1.0 - deep * k * max(0.0, -fold(a)) ** 1.2


# ------------------------------------------------------------------ the body sheet (soft_sheet)
N_LOW = 14
# The join: at ze(a) (ZC with a gentle wave) the cowl and the sheet both lie exactly on the drape and
# cross there: above it the sheet turns in under the cowl, below it the cowl turns in under the sheet.
# The visible surface is the drape itself everywhere; rows are 3 to 4 mm apart round the crossing so
# the turned-in rows tilt the shading of only a hairline.
NEAR_SHEET = [0.03, 0.015, 0.007, 0.003, 0.0, -0.003, -0.008, -0.016]
NEAR_COWL = [0.003, 0.0, -0.004, -0.012]


def ze_of(a):
    return ZC + 0.005 * math.sin(5 * a + 1.3)


def sheet_grid():
    cols = []
    for i in range(NU):
        a = 2 * math.pi * i / NU
        _, zt, rt, front = column(i)
        top = min(SHEET_TOP, zt - 0.008)
        ze, zb = ze_of(a), hem_z(a)
        zs = [top, (top + ze + 0.03) / 2] + [ze + d for d in NEAR_SHEET]
        zs += [ze - 0.016 + (zb - ze + 0.016) * j / N_LOW for j in range(1, N_LOW + 1)]
        col = []
        for z in zs:
            off = 0.016 * (1.0 - math.exp(-(z - ze) / 0.027)) if z > ze else 0.0
            col.append(pt(a, drape_r(a, z, zt, rt, front) - off, z))
        cols.append(col)
    return [[cols[i][j] for i in range(NU)] for j in range(len(cols[0]))]


def sheet():
    g = sheet_grid()
    nv = len(g) - 1
    holes = [Hole("l"), Hole("r")]
    ob = shell("sheet", g, 0.011, SH, SHD, inner_rows=[nv, nv - 1, nv - 2, nv - 4, nv - 7, nv - 10, nv - 13, 6, 0],
               holes=holes)
    pieces = [p for h in holes for p in hole_pieces(h)]

    def shade(p):
        # the cowl's lower drape bakes a little darker (about 0.95) than the open sheet: match it at the join
        return fold_shade(p) * (1.0 - 0.045 * smooth((p.z - (ZC - 0.08)) / 0.08))
    KK.AO_SHAPE["soft_sheet"] = shade
    return KK.soft_part("soft_sheet", [ob] + pieces, SHEET_PIN), holes


def pivots(holes):
    """The arm pivots, mirror images of each other: on each hole's axis PIVOT_OUT outside the outer surface
    (so a tilted upper arm leaves the cloth inside the hole), averaged across the two sides."""
    pl = holes[0].c + holes[0].n * PIVOT_OUT
    pr = holes[1].c + holes[1].n * PIVOT_OUT
    x, y, z = (pl.x - pr.x) / 2, (pl.y + pr.y) / 2, (pl.z + pr.z) / 2
    return V((x, y, z)), V((-x, y, z))


# ------------------------------------------------------------------ the hood (soft_cowl)
N_DROP = 12


def cowl_grid():
    """The hood (crown to the drape line) and the drape below it, on the drape exactly down to the join
    ze(a), then turned in under the sheet to a thin edge: ONE surface. The face opening is cut out of
    it afterwards."""
    cols = []
    for i in range(NU):
        a = 2 * math.pi * i / NU
        col, zt, rt, front = column(i)
        col = list(col)
        ze = ze_of(a)
        zs = [zt + (ze + 0.008 - zt) * j / N_DROP for j in range(1, N_DROP + 1)] + [ze + d for d in NEAR_COWL]
        for z in zs:
            tuck = 0.0055 * min(1.0, (ze - z) / 0.012) if z < ze else 0.0
            col.append(pt(a, drape_r(a, z, zt, rt, front) - tuck, z))
        cols.append(col)
    return [[cols[i][j] for i in range(NU)] for j in range(len(cols[0]))]


def pre_counter(ob):
    """Undo the baked head tilt (finish2 tilts every head child) on the cowl's lower part, on the same
    ramp the rig uses for its per-frame counter-rotation, so after the tilt the cowl's edge lies on the
    (untilted) body sheet."""
    qi = KK.HEAD_TILT.to_quaternion().inverted()
    mw = ob.matrix_world
    inv = mw.inverted()
    for v in ob.data.vertices:
        p = mw @ v.co
        k = smooth((NECK.z - p.z - COUNTER_R0) / COUNTER_LEN)
        if k > 0.0:
            q = Quaternion().slerp(qi, k)
            v.co = inv @ (NECK + q @ (p - NECK))
    ob.data.update()


def cowl():
    """soft_cowl: the hood sheet (crown, face opening with a soft rolled edge, the two black sheet-eyes)
    and the drape below it to a thin edge lying on the body sheet, one piece of cloth."""
    g = cowl_grid()
    nv = len(g) - 1
    top = HOOD_WARP(V((0, KK.HC.y, KK.HC.z + KK.HR.z)))
    # the thin cowl's inside wall follows every other row of the drape (a chord across a longer span
    # of the curving drape would poke out through a 3 mm cloth)
    inner = [nv, nv - 1, nv - 2, nv - 3] + list(range(nv - 5, N_UP - 1, -2)) + [N_UP - 1]
    ob = shell("hood_sheet", g, COWL_T, SH, SHD, inner_rows=inner, pole=top,
               cut=lambda c: KK.opening(c))
    tree = D.bvh(ob)
    rim_pts = []
    for i in range(36):
        a = 2 * math.pi * i / 36
        p, n = KK.ellipsoid_pt(KK.HC, KK.HR, math.cos(a) * KK.OA, KK.OZ + math.sin(a) * KK.OC)
        rim_pts.append(p - n * 0.004)
    rim = D.sweep("hood_rim", rim_pts, 0.025, sides=8, color=SH, mat=CLOTH, project="sphere", closed=True, center=KK.HC)
    eyes = sheet_eyes(tree)
    pre_counter(ob)

    def shade(p):
        # the folds carry on from the sheet; a soft shade under the chin where the cloth turns in
        return fold_shade(p) * (1.0 - 0.1 * smooth((0.66 - p.z) / 0.06) * max(0.0, -math.sin(math.atan2(p.y - YO, p.x))))
    KK.AO_SHAPE["soft_cowl"] = shade
    return KK.soft_part("soft_cowl", [ob, rim] + eyes, COWL_PIN)


def join_normals(names):
    """KK.POST_ATTACH hook: round the crossing, the outside of both parts gets the drape's own normal
    (custom normals), so the turned-in rows do not shade a hairline where the cowl meets the sheet."""
    if "soft_cowl" not in names or "soft_sheet" not in names:
        return
    da = 2 * math.pi / NU

    def drape_pt(i, z):
        i %= NU
        _, zt, rt, front = column(i)
        a = i * da
        return pt(a, drape_r(a, min(z, zt), zt, rt, front), z)
    for nm in ("soft_cowl", "soft_sheet"):
        ob = names[nm]
        me = ob.data
        m3 = ob.matrix_world.to_3x3()
        inv = m3.inverted()
        normals = [v.normal.copy() for v in me.vertices]
        changed = 0
        for v in me.vertices:
            p = ob.matrix_world @ v.co
            a = math.atan2(p.y - YO, p.x) % (2 * math.pi)
            i = int(round(a / da))
            ze = ze_of(i * da)
            if not (ze - 0.02 < p.z < ze + 0.04):
                continue
            if (m3 @ v.normal).dot(V((p.x, p.y - YO, 0.0))) <= 0:
                continue                            # the inside wall
            du = drape_pt(i + 1, p.z) - drape_pt(i - 1, p.z)
            dv = drape_pt(i, p.z - 0.002) - drape_pt(i, p.z + 0.002)
            n = du.cross(dv).normalized()
            if n.dot(V((p.x, p.y - YO, 0.0))) < 0:
                n = -n
            normals[v.index] = (inv @ n).normalized()
            changed += 1
        me.normals_split_custom_set_from_vertices(normals)
        print("JOIN_NORMALS", nm, changed)


# ------------------------------------------------------------------ head
def sheet_eyes(tree):
    """Two big black ovals high on the hood, painted flat on the cloth, tops leaning out a little."""
    parts = []
    for sx in (1, -1):
        p, n = KK.on_surface(tree, (0.092 * sx, -0.6, 1.035), (0, 1, 0))
        S = KK.Surf(tree, p, n, roll=-11 * sx)
        parts.append(S.disc(f"heye{sx}", 0, 0, 0.047, 0.06, "ink", lift=0.0028, puff=0.0006, segs=24, rings=3, sink=0.004))
    return parts


def head():
    """The head mesh is the kid: face skin, nose, cheeks and fringe (the hood sheet is soft_cowl)."""
    static, children = KK.face2(fang=False)
    parts = static + KK.fringe2(clip=KK.hood_clip())
    return D.part(parts, "head", KK.NECK), children


# ------------------------------------------------------------------ body core
def body():
    """The kid's body under the sheet: dark clothes, never seen except from below the hem."""
    prof = [(0.0, 0.25), (0.12, 0.255), (0.16, 0.3), (0.17, 0.36), (0.16, 0.45), (0.135, 0.53), (0.1, 0.59), (0.0, 0.62)]
    core = D.lathe("core", prof, 16, color=LEG, mat="felt", project="cyl")
    return D.part([core], "body", (0, 0, KK.HIP_Z))



# ================================================================== the costume
def build():
    """Returns (parts, children) for KK.finish2()."""
    KK.AO_SHAPE.clear()
    KK.AO_POST.clear()
    KK.AO_IGNORE.clear()
    _COLS.clear()
    moving = ["arm_l", "arm_r", "forearm_l", "forearm_r", "hand_l", "hand_r", "bucket", "bucket_candy", "leg_l", "leg_r",
              "shin_l", "shin_r"]
    # layered cloth: no baked shadow line where the cowl's edge lies on the sheet, none from the arms that move
    KK.AO_IGNORE["soft_sheet"] = ["soft_cowl", "head"] + moving
    KK.AO_IGNORE["soft_cowl"] = ["soft_sheet", "body"] + moving
    for nm in moving[:8]:
        KK.AO_IGNORE[nm] = ["soft_sheet", "soft_cowl"]     # an arm turns in its hole: no baked ring on it
    parts = {"body": body()}
    parts["head"], children = head()
    for s, hip in (("l", KK.HIP_L), ("r", KK.HIP_R)):
        parts["leg_" + s], parts["shin_" + s] = KK.leg2("leg_" + s, hip, LEG, lambda x, s=s: KK.sneaker("leg_" + s, x, SHOE),
                                                        thigh_mat="felt")
    # No sheet sleeves: the kid's own plain dark long-sleeve top, coming out through the arm holes. Built on the
    # kit's shoulders (so hands, wand socket and bucket are the kit's), then moved out into the holes.
    # The arm keeps the kit's directions (so every pose means the same as on the other kids) but the upper arm
    # is UPPER_ARM and the forearm FOREARM of the kit's length: all of the upper arm shows outside the hole,
    # and the hand (with the bucket) sits higher, since the holes are lower than the kit's shoulders.
    for s, sho, elb, wri in (("l", KK.SHO_L, KK.ELBOW_L, KK.WRIST_L), ("r", KK.SHO_R, KK.ELBOW_R, KK.WRIST_R)):
        e2 = V(sho) + (V(elb) - V(sho)) * UPPER_ARM
        w2 = e2 + (V(wri) - V(elb)) * FOREARM
        HAND_SHIFT[s] = w2 - V(wri)
        parts["arm_" + s], parts["forearm_" + s] = KK.arm2("arm_" + s, sho, e2, w2, LEG,
                                                           mat=KK.PLUSH, r_sho=ARM_R, r_elbow=0.046, r_wrist=0.043,
                                                           cuff=(LEG, KK.PLUSH), cuff_tube=0.012)
    parts["hand_l"] = KK.grip_hand("hand_l", KK.WRIST_L, KK.WAND_GRIP, KK.WAND_DIR, (0.55, 0.83, 0.0), 0.0155, fat=1.12)
    parts["hand_r"] = KK.grip_hand("hand_r", KK.WRIST_R, KK.BUCKET_GRIP, (1, 0, 0), (0.0, -0.45, 0.9), 0.012, fat=1.12)
    sheet_ob, holes = sheet()
    SHOULDER["l"], SHOULDER["r"] = pivots(holes)
    print("ARM_PIVOTS (glTF x, y, z)", [(round(p.x, 4), round(p.z, 4), round(-p.y, 4)) for p in SHOULDER.values()])
    KK.POST_ATTACH[:] = [join_normals, shift_arms]
    children += [("soft_cowl", cowl(), "head"), ("soft_sheet", sheet_ob, "body")]
    return parts, children


SHOULDER = {}                 # the arm pivots in the holes (Blender coordinates), set by build()
HAND_SHIFT = {}               # how far the shorter upper arm moves the elbow, forearm and hand (set by build())


def shift_arms(names):
    """KK.POST_ATTACH hook: move each arm (and everything it carries: forearm, hand, wand socket, bucket) so
    its pivot sits in its hole, and each hand (with the bucket / the wand socket) onto the shorter arm's wrist.
    Every node stays translation-only."""
    for s, sho in (("l", KK.SHO_L), ("r", KK.SHO_R)):
        ob = names["arm_" + s]
        ob.matrix_basis = Matrix.Translation(ob.matrix_basis.translation + (SHOULDER[s] - V(sho)))
        hand = names["hand_" + s]
        hand.matrix_basis = Matrix.Translation(hand.matrix_basis.translation + HAND_SHIFT[s])
    bpy.context.view_layer.update()
