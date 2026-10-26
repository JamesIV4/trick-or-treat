"""Shared helpers for the decor lane, batch03 (hanging decor and lights).

Builds on art/team/yard_street/batch02/yard_common.py (sweep, lathe, blob, finish, ...):
    import sys, pathlib; sys.path.insert(0, str(pathlib.Path(__file__).resolve().parent))
    from decor_common import *

Adds:
  * raw()          mesh from explicit verts + faces with per-face colour/material/smoothing.
  * lantern4()     four-sided Victorian lantern head: tapered glass box with framed, two-tone
                   glowing panes (amber rim + pale hot core), corner posts, flared roof, finial.
"""
from __future__ import annotations

import pathlib
import sys

HERE = pathlib.Path(__file__).resolve().parent
sys.path.insert(0, str(HERE.parents[1] / "yard_street" / "batch02"))
from yard_common import *  # noqa: E402,F401,F403
from yard_common import K, Vector, bmesh, math, uv_box  # noqa: E402

import bpy  # noqa: E402

B = "lamp_black"


def raw(name, verts, faces, smooth=False, uv_scale=1.0):
    """faces: list of (index tuple, colour, material[, smooth]). UVs: per-face planar (metres)."""
    bm = bmesh.new()
    vs = [bm.verts.new(Vector(v)) for v in verts]
    groups = {}
    sm = []
    for spec in faces:
        idx, col, mat = spec[0], spec[1], spec[2]
        try:
            f = bm.faces.new([vs[i] for i in idx])
        except ValueError:
            continue
        groups.setdefault((col, mat), []).append(f)
        sm.append((f, spec[3] if len(spec) > 3 else smooth))
    bm.normal_update()
    uv_box(bm, bm.faces, uv_scale)
    for (c, m), fs in groups.items():
        K._paint_bm(bm, c, m, fs)
    for f, s in sm:
        f.smooth = s
    me = bpy.data.meshes.new(name)
    bm.to_mesh(me)
    bm.free()
    return K._link(bpy.data.objects.new(name, me))


def _lerp(a, b, t):
    return a + (b - a) * t


def lantern4(name, loc, h_glass, w_bot, w_top, rim="glow_lantern", core="glow_lamp", frame=B,
             roof_w=None, roof_h=None, finial=True, bottom="pan", frame_w=None, hang_ring=False):
    """Four-sided lantern head. loc = centre of the glass box bottom. Half-widths w_bot/w_top
    (wider on top, like the concept's street and porch lanterns). Faces point along +-X/+-Y so a
    flat pane faces the camera (-Y). Returns (parts, glass_centre, top_z, bottom_z)."""
    cx, cy, z0 = loc
    z1 = z0 + h_glass
    fw = frame_w if frame_w is not None else 0.11 * min(w_bot, w_top) * 2 * 0.5
    parts = []
    # ---- glass box: per side outer quad -> frame strips -> amber rim ring -> pale core
    corners = [(1, -1), (1, 1), (-1, 1), (-1, -1)]  # x, y signs going round (+X side first)
    verts, faces = [], []

    def P(sx, sy, z, w):
        return (cx + sx * w, cy + sy * w, z)

    sides = [((1, -1), (1, 1)), ((1, 1), (-1, 1)), ((-1, 1), (-1, -1)), ((-1, -1), (1, -1))]
    for (a, b) in sides:
        # outer quad corners (BL, BR, TR, TL) as seen from outside
        bl = Vector(P(a[0], a[1], z0, w_bot))
        br = Vector(P(b[0], b[1], z0, w_bot))
        tr = Vector(P(b[0], b[1], z1, w_top))
        tl = Vector(P(a[0], a[1], z1, w_top))
        n = ((br - bl).cross(tl - bl)).normalized()
        rec = -n * 0.004  # panes sit slightly recessed behind the frame

        def inner(u, v, d):
            bot = _lerp(bl, br, u)
            top = _lerp(tl, tr, u)
            return _lerp(bot, top, v) + d

        W = (br - bl).length
        H = (tl - bl).length
        fu, fv = fw / W, fw / H
        ring1 = [inner(fu, fv, rec), inner(1 - fu, fv, rec), inner(1 - fu, 1 - fv, rec), inner(fu, 1 - fv, rec)]
        cu, cv = 0.3, 0.26
        ring2 = [inner(cu, cv * 0.8, rec), inner(1 - cu, cv * 0.8, rec), inner(1 - cu, 1 - cv, rec), inner(cu, 1 - cv, rec)]
        outer = [bl, br, tr, tl]
        base = len(verts)
        verts += [tuple(v) for v in outer + ring1 + ring2]
        o = [base + i for i in range(4)]
        r1 = [base + 4 + i for i in range(4)]
        r2 = [base + 8 + i for i in range(4)]
        for k in range(4):
            k1 = (k + 1) % 4
            faces.append(((o[k], o[k1], r1[k1], r1[k]), frame, "metal"))
            faces.append(((r1[k], r1[k1], r2[k1], r2[k]), rim, "flat"))
        faces.append(((r2[0], r2[1], r2[2], r2[3]), core, "flat"))
    parts.append(raw(f"{name}_glass", verts, faces))
    # ---- corner posts (slightly proud of the glass corners)
    for sx, sy in corners:
        a = (cx + sx * (w_bot + 0.004), cy + sy * (w_bot + 0.004), z0 - 0.01)
        b = (cx + sx * (w_top + 0.004), cy + sy * (w_top + 0.004), z1 + 0.01)
        parts.append(rod(f"{name}_post{sx}{sy}", a, b, fw * 0.55, frame, sides=4, chamfer=0))
    s2 = math.sqrt(2)
    ph = math.radians(45)
    # ---- top band + flared roof (4-sided, flat shaded facets)
    rw = roof_w if roof_w is not None else w_top * 1.45
    rh = roof_h if roof_h is not None else h_glass * 0.42
    band = [(w_top * 1.06 * s2, z1 - 0.005), (w_top * 1.12 * s2, z1 + 0.012), (w_top * 1.12 * s2, z1 + rh * 0.12)]
    roof = [(rw * s2, z1 + rh * 0.16), (rw * 1.02 * s2, z1 + rh * 0.22), (rw * 0.86 * s2, z1 + rh * 0.3),
            (rw * 0.64 * s2, z1 + rh * 0.42), (rw * 0.42 * s2, z1 + rh * 0.6), (rw * 0.25 * s2, z1 + rh * 0.74),
            (rw * 0.2 * s2, z1 + rh * 0.77), (rw * 0.27 * s2, z1 + rh * 0.82), (rw * 0.27 * s2, z1 + rh * 0.9),
            (rw * 0.15 * s2, z1 + rh)]
    prof = [(0.0, z1 - 0.004)] + band + roof
    parts.append(lathe(f"{name}_roof", prof, [core] + [frame] * (len(prof) - 2), ["flat"] + ["metal"] * (len(prof) - 2),
                       segments=4, phase=ph, smooth=False, cap_bottom=False, cap_top=True, loc=(cx, cy, 0)))
    top_z = z1 + rh
    if finial:
        k = rw  # finial scales with the roof
        parts.append(lathe(f"{name}_finial", [(0.13 * k, top_z), (0.16 * k, top_z + 0.05 * k), (0.1 * k, top_z + 0.1 * k),
                                              (0.07 * k, top_z + 0.14 * k)], frame, "metal", segments=6,
                           cap_bottom=False, cap_top=False, loc=(cx, cy, 0)))
        parts.append(blob(f"{name}_ball", 0.15 * k, frame, mat="metal", loc=(cx, cy, top_z + 0.24 * k),
                          segments=8 if k > 0.2 else 6, rings=5 if k > 0.2 else 4, noise_amp=0.0, axis="Z"))
        if hang_ring:
            parts.append(K.torus(f"{name}_ring", 0.11 * k, 0.03 * k, color=frame, mat="metal", major_seg=6, minor_seg=4,
                                 loc=(cx, cy, top_z + 0.48 * k), rot=(90, 0, 0)))
            top_z += 0.59 * k
        else:
            parts.append(lathe(f"{name}_spike", [(0.06 * k, top_z + 0.36 * k), (0.0, top_z + 0.6 * k)], frame, "metal",
                               segments=6, cap_bottom=True, cap_top=False, loc=(cx, cy, 0)))
            top_z += 0.6 * k
    # ---- bottom: square pan tapering to a drop finial
    bw = w_bot * 1.12
    if bottom == "pan":
        bprof = [(0.0, z0 - 0.36 * h_glass), (0.12 * bw * s2, z0 - 0.33 * h_glass), (0.2 * bw * s2, z0 - 0.26 * h_glass),
                 (0.5 * bw * s2, z0 - 0.14 * h_glass), (bw * s2, z0 - 0.05 * h_glass), (bw * s2, z0 + 0.004),
                 (w_bot * 0.98 * s2, z0 + 0.006)]
        parts.append(lathe(f"{name}_pan", bprof, frame, "metal", segments=4, phase=ph, smooth=False,
                           cap_bottom=False, cap_top=True, cap_colors=(frame, core), cap_mats=("metal", "flat"),
                           loc=(cx, cy, 0)))
        bot_z = z0 - 0.36 * h_glass
    else:  # flat collar seat (post top)
        bprof = [(w_bot * 0.5 * s2, z0 - 0.07 * h_glass), (bw * s2, z0 - 0.05 * h_glass), (bw * s2, z0 + 0.004),
                 (w_bot * 0.98 * s2, z0 + 0.006)]
        parts.append(lathe(f"{name}_pan", bprof, frame, "metal", segments=4, phase=ph, smooth=False,
                           cap_bottom=True, cap_top=True, cap_colors=(frame, core), cap_mats=("metal", "flat"),
                           loc=(cx, cy, 0)))
        bot_z = z0 - 0.07 * h_glass
    return parts, (cx, cy, z0 + h_glass * 0.5), top_z, bot_z


def report(parts):
    """Prints triangle count per part (before merge) so budgets can be traded deliberately."""
    tot = 0
    for p in parts:
        n = sum(len(f.vertices) - 2 for f in p.data.polygons)
        tot += n
        print(f"  part {p.name:<18} {n}")
    print(f"  parts total {tot}")
