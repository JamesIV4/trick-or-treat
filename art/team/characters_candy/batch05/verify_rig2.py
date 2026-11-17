"""Numerical check of an exported v2 costume glTF against rig contract v2 (CHECKPOINT.md). Plain Python.

  python art/team/characters_candy/batch05/verify_rig2.py [ids...] [--tree]   -> table + rig_check2.json; exit 1 on failure

Per costume:
  * the v1 top level is unchanged: body, head, leg_*, arm_* (and wing_*, tail when present) under the
    root at exactly the v1 joint origins
  * the v2 chain exists with the contract's parents and pivots: shin_* under leg_*, forearm_* under
    arm_*, hand_* under forearm_*, bucket under hand_r (same place in the world as v1), bucket_candy
    and socket_bucket_light under bucket, socket_wand under hand_l (origin at the v1 wand grip, node
    +Z along the v1 wand direction)
  * every node is translation-only (no rotation, no scale) except eye_* and socket_* (iris_* is
    translation-only inside its eye)
  * face: eye_*, alt_lash_*, brow_*, mouth_smile and the three alt_mouth_* under head, iris_* under
    eye_*; each eye's +Z points out of the face and its +Y up; the four mouths share one origin
  * every part that must be a mesh is one; tail / wing_* / soft_* are a single mesh with no mesh below
    it and reach away from their origin; the shin carries the shoe (its mesh touches the ground)
  * hands: the bucket grip and the wand grip sit inside the fists
  * no node name ends in a Godot importer suffix; the bucket and candy heap are the shared ones;
    the model stands on the ground, faces +Z, and is inside the triangle cap
"""
import json
import math
import re
import sys
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parents[3] / "tools"))
import gates  # noqa: E402  (glTF accessor reader, node matrices)

ROOT = Path(__file__).resolve().parents[4]
MODELS = ROOT / "game" / "assets" / "models" / "characters"
R = "ROOT"
TOP = {"body": (0, 0.30, 0), "head": (0, 0.64, 0), "leg_l": (0.085, 0.30, 0), "leg_r": (-0.085, 0.30, 0),
       "arm_l": (0.165, 0.585, 0), "arm_r": (-0.165, 0.585, 0)}
TOP_OPT = {"wing_l": (0.075, 0.56, -0.135), "wing_r": (-0.075, 0.56, -0.135), "tail": (0, 0.34, -0.14)}
CHAIN = {"shin_l": ("leg_l", (0.0051, -0.135, 0)), "shin_r": ("leg_r", (-0.0051, -0.135, 0)),
         "forearm_l": ("arm_l", (0.058, -0.090, 0.005)), "forearm_r": ("arm_r", (-0.066, -0.085, 0.033)),
         "hand_l": ("forearm_l", (0.022, -0.090, 0.025)), "hand_r": ("forearm_r", (-0.029, -0.085, 0.052)),
         "bucket": ("hand_r", (0, -0.03, 0)), "bucket_candy": ("bucket", (0, -0.1577, 0)),
         "socket_bucket_light": ("bucket", (0, -0.2057, 0)), "socket_wand": ("hand_l", (0.016, -0.036, 0.006))}
FACE = {"eye_l": "head", "eye_r": "head", "iris_l": "eye_l", "iris_r": "eye_r", "alt_lash_l": "head", "alt_lash_r": "head",
        "brow_l": "head", "brow_r": "head", "mouth_smile": "head", "alt_mouth_open": "head", "alt_mouth_o": "head",
        "alt_mouth_grit": "head"}
MOUTHS = ["mouth_smile", "alt_mouth_open", "alt_mouth_o", "alt_mouth_grit"]
MAY_ROTATE = re.compile(r"^(eye_[lr]|socket_.*)$")
SUFFIX = re.compile(r"[-_](col|convcol|colonly|convcolonly|rigid|vehicle|wheel|navmesh|occ|occonly|loop|cycle|noimp)\d*$")
WAND_DIR = (0.05, -0.9, 0.45)
TRI_CAP = 30000
# Costumes whose arms are placed by the costume (kid_ghost: the shoulder pivots sit in the sheet's arm holes,
# the upper arm is shorter): arm_* / forearm_* / hand_* pivots and the bucket's place in the world are not held
# to the v1 numbers; instead the two arms must mirror each other and the parents must be right.
OWN_ARMS = {"kid_ghost"}
ARM_CHAIN = ("arm_l", "arm_r", "forearm_l", "forearm_r", "hand_l", "hand_r")


def rot(q, v):
    x, y, z, w = q
    m = gates.node_matrix({"rotation": [x, y, z, w]})
    return gates.apply(m, v)


def load(aid):
    path = MODELS / f"{aid}.gltf"
    g = json.loads(path.read_text())
    bins = [(path.parent / b["uri"]).read_bytes() for b in g["buffers"]]
    nodes = g["nodes"]
    root = g["scenes"][0]["nodes"][0]
    parent = {}
    for i, n in enumerate(nodes):
        for c in n.get("children", []):
            parent[c] = i
    world = {}

    def walk(i, pm):
        m = gates.mat_mul(pm, gates.node_matrix(nodes[i]))
        world[i] = m
        for c in nodes[i].get("children", []):
            walk(c, m)
    walk(root, [1, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1])
    info, tris = {}, 0
    for i, n in enumerate(nodes):
        verts = []
        if "mesh" in n:
            for prim in g["meshes"][n["mesh"]]["primitives"]:
                verts += gates.read_accessor(g, bins, prim["attributes"]["POSITION"])
                tris += g["accessors"][prim["indices"]]["count"] // 3
        p = parent.get(i)
        info[n.get("name", "")] = {"parent": None if p is None else (R if p == root else nodes[p]["name"]),
                                   "t": n.get("translation", [0, 0, 0]), "r": n.get("rotation"), "s": n.get("scale"),
                                   "m": n.get("matrix"), "verts": verts, "world": world[i],
                                   "children": [nodes[c]["name"] for c in n.get("children", [])]}
    return info, nodes[root].get("name", ""), tris


def bounds(vs):
    return [min(v[k] for v in vs) for k in range(3)], [max(v[k] for v in vs) for k in range(3)]


def mesh_below(info, nm):
    return any(info[c]["verts"] or mesh_below(info, c) for c in info[nm]["children"])


def tree(info, nm, depth=0, out=None):
    out = [] if out is None else out
    d = info[nm]
    kind = "mesh" if d["verts"] else "node"
    extra = "  rotated" if d["r"] is not None else ""
    out.append(f"{'  ' * depth}{nm:<{24 - 2 * depth}} {kind}  ({d['t'][0]:+.4f}, {d['t'][1]:+.4f}, {d['t'][2]:+.4f}){extra}")
    for c in d["children"]:
        tree(info, c, depth + 1, out)
    return out


def check(aid):
    c, root_name, tris = load(aid)
    fails, notes = [], []

    def expect(nm, par, t, tol):
        if nm not in c:
            fails.append(f"missing {nm}")
            return
        d = c[nm]
        if d["parent"] != par:
            fails.append(f"{nm} parent {d['parent']} != {par}")
        dt = max(abs(a - b) for a, b in zip(d["t"], t))
        if dt > tol:
            fails.append(f"{nm} pivot off by {dt:.5f} m: {[round(x, 4) for x in d['t']]} vs {t}")
    if root_name != aid:
        fails.append(f"root is named {root_name}")
    own_arms = aid in OWN_ARMS
    for nm, t in TOP.items():
        expect(nm, R, c[nm]["t"] if own_arms and nm in ARM_CHAIN and nm in c else t, 1e-5)
    opt = [nm for nm in TOP_OPT if nm in c]
    for nm in opt:
        expect(nm, R, TOP_OPT[nm], 1e-5)
    for nm, (par, t) in CHAIN.items():
        expect(nm, par, c[nm]["t"] if own_arms and nm in ARM_CHAIN and nm in c else t, 6e-4)
    if own_arms:
        la, ra = c.get("arm_l"), c.get("arm_r")
        if la and ra and (abs(la["t"][0] + ra["t"][0]) > 1e-4 or max(abs(la["t"][k] - ra["t"][k]) for k in (1, 2)) > 1e-4):
            fails.append(f"arm_l / arm_r pivots do not mirror each other: {la['t']} vs {ra['t']}")
        notes.append("own arms: " + ", ".join(f"{nm} {[round(x, 4) for x in c[nm]['t']]}" for nm in ARM_CHAIN if nm in c))
    for nm, par in FACE.items():
        if nm not in c:
            fails.append(f"missing {nm}")
        elif c[nm]["parent"] != par:
            fails.append(f"{nm} parent {c[nm]['parent']} != {par}")
    # transforms
    for nm, d in c.items():
        if nm == root_name:
            if d["r"] is not None or d["s"] is not None or max(map(abs, d["t"])) > 1e-6:
                fails.append("root is not an identity transform at the origin")
            continue
        if d["s"] is not None or d["m"] is not None:
            fails.append(f"{nm} has a scale or matrix")
        if d["r"] is not None and not MAY_ROTATE.match(nm):
            fails.append(f"{nm} has a rotation (only eye_* and socket_* may)")
        if SUFFIX.search(nm):
            fails.append(f"{nm}: name ends in a Godot importer suffix")
    # meshes
    need_mesh = list(TOP) + opt + [n for n in CHAIN if not n.startswith("socket_")] + list(FACE)
    for nm in need_mesh:
        if nm in c and not c[nm]["verts"]:
            fails.append(f"{nm} has no mesh")
    for nm in c:
        if nm in opt or nm.startswith("soft_"):
            if mesh_below(c, nm):
                fails.append(f"{nm} must be ONE mesh (a mesh hangs below it)")
            if c[nm]["verts"]:
                far = max(math.sqrt(sum(x * x for x in v)) for v in c[nm]["verts"])
                lo, hi = bounds(c[nm]["verts"])
                mid = math.sqrt(sum(((a + b) / 2) ** 2 for a, b in zip(lo, hi)))
                if far < 0.05 or mid < 0.1 * far:
                    fails.append(f"{nm}: the mesh does not reach away from its origin (far {far:.3f}, centre {mid:.3f})")
        if nm.startswith("soft_") and not c[nm]["verts"]:
            fails.append(f"{nm} must be a mesh itself")
    # face
    eye_axes = {}
    for s in "lr":
        e = c.get("eye_" + s)
        if not e:
            continue
        q = e["r"] or [0, 0, 0, 1]
        z, y = rot(q, (0, 0, 1)), rot(q, (0, 1, 0))
        eye_axes[s] = ([round(v, 3) for v in z], [round(v, 3) for v in y])
        if z[2] < 0.85 or y[1] < 0.9:
            fails.append(f"eye_{s}: +Z must point out of the face and +Y up (Z {eye_axes[s][0]}, Y {eye_axes[s][1]})")
        i = c.get("iris_" + s)
        if i and (i["r"] is not None or max(map(abs, i["t"])) > 1e-4):
            fails.append(f"iris_{s} must rest at its eye's origin with no rotation")
    ms = [c[m]["t"] for m in MOUTHS if m in c]
    if ms and max(abs(a[k] - ms[0][k]) for a in ms for k in range(3)) > 1e-5:
        fails.append("the mouths do not share one origin")
    # hands: the grips sit inside the fists
    grips = {}
    for hand, child in (("hand_r", "bucket"), ("hand_l", "socket_wand")):
        if hand in c and child in c and c[hand]["verts"]:
            lo, hi = bounds(c[hand]["verts"])
            g = c[child]["t"]
            mid = [(a + b) / 2 for a, b in zip(lo, hi)]
            dist = math.dist(g, mid)
            far = max(c[hand]["verts"], key=lambda v: sum(x * x for x in v))
            grips[hand] = {"grip": [round(x, 4) for x in g], "fist_centre": [round(x, 4) for x in mid], "off": round(dist, 4),
                           "tip": [round(x, 4) for x in far]}
            if dist > 0.03 or any(not (a - 0.004 <= x <= b + 0.004) for x, a, b in zip(g, lo, hi)):
                fails.append(f"{hand}: the {child} grip is not inside the fist (off {dist:.3f} m)")
    w = c.get("socket_wand")
    if w:
        z = rot(w["r"] or [0, 0, 0, 1], (0, 0, 1))
        n = math.sqrt(sum(x * x for x in WAND_DIR))
        if sum(a * b / n for a, b in zip(z, WAND_DIR)) < 0.999:
            fails.append(f"socket_wand +Z {[round(v, 3) for v in z]} is not the v1 wand direction")
    # bucket in the v1 place; shared bucket meshes; ground; front; budget
    if "bucket" in c:
        bw = c["bucket"]["world"][12:15]
        if own_arms:
            notes.append(f"bucket origin in the world {[round(x, 4) for x in bw]} (v1: -0.26, 0.385, 0.085)")
        elif math.dist(bw, (-0.26, 0.385, 0.085)) > 6e-4:
            fails.append(f"bucket origin moved in the world: {[round(x, 4) for x in bw]}")
    try:
        ref, _, _ = load("kid_cat")
        for nm in ("bucket", "bucket_candy"):
            if nm in c and (len(c[nm]["verts"]) != len(ref[nm]["verts"]) or max(
                    abs(x - y) for p, q in zip(bounds(c[nm]["verts"]), bounds(ref[nm]["verts"])) for x, y in zip(p, q)) > 1e-5):
                fails.append(f"{nm} mesh differs from the shared one (kid_cat's)")
    except (FileNotFoundError, KeyError):
        notes.append("no kid_cat export to compare the bucket with")
    lo_y, hi_y = math.inf, -math.inf
    for nm, d in c.items():
        for v in d["verts"]:
            y = gates.apply(d["world"], v)[1]
            lo_y, hi_y = min(lo_y, y), max(hi_y, y)
    if abs(lo_y) > 0.03:
        fails.append(f"min height {lo_y:.3f}")
    soles = {}
    for s in "lr":
        d = c.get("shin_" + s)
        if d and d["verts"]:
            y = min(gates.apply(d["world"], v)[1] for v in d["verts"])
            soles[s] = round(y, 4)
            if abs(y) > 0.01:
                fails.append(f"shin_{s} does not carry the shoe (its lowest point is at {y:.3f} m)")
    if "head" in c and c["head"]["verts"] and max(v[2] for v in c["head"]["verts"]) < 0.15:
        fails.append("head: face not toward +Z?")
    if tris > TRI_CAP:
        fails.append(f"triangles {tris} > {TRI_CAP}")
    extra = sorted(n for n in c if n != root_name and n not in TOP and n not in TOP_OPT and n not in CHAIN and n not in FACE)
    if extra:
        notes.append(f"costume parts: {extra}")
    return {"pass": not fails, "fails": fails, "notes": notes, "tris": tris, "optional": opt, "height": round(hi_y, 4),
            "min_height": round(lo_y, 4), "eye_axes": eye_axes, "grips": grips, "shin_floor": soles,
            "mesh_parts": sum(1 for d in c.values() if d["verts"]), "nodes": len(c),
            "tree": tree(c, root_name)}


def main(argv):
    ids = [a for a in argv if not a.startswith("--")] or ["dragon_kid"]
    out = Path(__file__).resolve().parent / "rig_check2.json"
    report = json.loads(out.read_text()) if out.exists() else {}
    ok = True
    for aid in ids:
        r = check(aid)
        ok &= r["pass"]
        report[aid] = r
        print(f"{'PASS' if r['pass'] else 'FAIL'} {aid:<13} tris {r['tris']}  nodes {r['nodes']} ({r['mesh_parts']} meshes)  "
              f"optional {r['optional']}  height {r['height']:.3f} m  min {r['min_height']:+.4f}")
        for h, gq in r["grips"].items():
            print(f"     {h}: grip {gq['grip']} is {gq['off'] * 1000:.0f} mm from the fist centre; farthest point {gq['tip']}")
        for f in r["fails"]:
            print("     -", f)
        for nt in r["notes"]:
            print("     ~", nt)
        if "--tree" in argv:
            print("\n".join("     " + ln for ln in r["tree"]))
    out.write_text(json.dumps(report, indent=1))
    return 0 if ok else 1


if __name__ == "__main__":
    sys.exit(main(sys.argv[1:]))
