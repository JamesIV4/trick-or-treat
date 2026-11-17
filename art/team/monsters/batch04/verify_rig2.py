"""Checks exported v2 monster glTFs against the rig contract (CHECKPOINT.md, "v2 glTF contract") and
against the spec the build wrote (rig_spec/<id>.json). Plain Python, no Blender.

    python art/team/monsters/batch04/verify_rig2.py [monster_skeleton ...]     (default: every spec)

Prints PASS / FAIL per id with the failures, and notes that are worth a look but do not fail.
Writes rig_check.json. Exit code 1 if anything failed.

What it checks:
  root       one scene root, an empty named <id>, identity transform
  names      letters / digits / underscores only, unique, no Godot import-hint ending (_col, _rigid ...)
  tree       every part of the spec exists under the parent the spec names; `body` exists; the model
             is version 2 (a part other than body / part_a / part_b)
  transforms no node has a scale or a matrix; rotation only where the spec says so; IK chains
             (leg, shin, foot ...) are translation-only
  budgets    at most 32 mesh parts and 8,000 triangles
  eyes       head exists; each eye's pupil is its child; the eye's local +Z points out of the head and
             its +Y up; the eye mesh is centred on its origin
  sockets    exist under the part the spec names; nothing hangs below a socket
  soft       a mesh pinned at its origin, with at least `loops` bands of vertices along its run
  ball       the child has a ball of that radius round its pivot and the parent's mesh reaches into
             it (so no gap can open at the joint)
  ground     the rest pose touches y = 0 within 3 cm (unless floating); height within 10% of the
             collision height
Notes: parts whose reach is under 0.10 m (the game casts no shadow from them), how far the art
sticks out of the collision radius.
"""
import json
import math
import re
import sys
from pathlib import Path

HERE = Path(__file__).resolve().parent
sys.path.insert(0, str(HERE.parents[2] / "tools"))
import gates  # noqa: E402  (glTF accessor reader, node matrices)

MODELS = HERE.parents[3] / "game" / "assets" / "models" / "monsters"
HINTS = ("noimp", "col", "convcol", "colonly", "convcolonly", "navmesh", "vehicle", "wheel", "rigid", "loop", "cycle",
         "occ", "occonly")
IDENT = [1, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1]


def axis(m, k):
    return (m[4 * k], m[4 * k + 1], m[4 * k + 2])


def sub(a, b):
    return tuple(x - y for x, y in zip(a, b))


def dot(a, b):
    return sum(x * y for x, y in zip(a, b))


def norm(a):
    return math.sqrt(dot(a, a))


def load(aid):
    path = MODELS / f"{aid}.gltf"
    g = json.loads(path.read_text())
    bins = [(path.parent / b["uri"]).read_bytes() for b in g["buffers"]]
    nodes = g["nodes"]
    parent = {}
    for i, n in enumerate(nodes):
        for c in n.get("children", []):
            parent[c] = i
    world = {}

    def walk(i, pm):
        world[i] = gates.mat_mul(pm, gates.node_matrix(nodes[i]))
        for c in nodes[i].get("children", []):
            walk(c, world[i])
    roots = g["scenes"][0]["nodes"]
    for r in roots:
        walk(r, IDENT)
    verts, tris = {}, {}
    for i, n in enumerate(nodes):
        vs, t = [], 0
        if "mesh" in n:
            for prim in g["meshes"][n["mesh"]]["primitives"]:
                pos = gates.read_accessor(g, bins, prim["attributes"]["POSITION"])
                vs += pos
                t += (g["accessors"][prim["indices"]]["count"] if "indices" in prim else len(pos)) // 3
        verts[i], tris[i] = vs, t
    return g, nodes, parent, world, roots, verts, tris


def check(aid, spec):
    fails, notes = [], []
    g, nodes, parent, world, roots, verts, tris = load(aid)
    name = [n.get("name", "") for n in nodes]
    idx = {}
    for i, nm in enumerate(name):
        if nm in idx:
            fails.append(f"node name '{nm}' is used twice")
        idx[nm] = i
    # root
    if len(roots) != 1 or name[roots[0]] != aid:
        fails.append(f"scene roots are {[name[r] for r in roots]}, expected the single empty '{aid}'")
    root = roots[0]
    rn = nodes[root]
    if "mesh" in rn or any(k in rn for k in ("rotation", "scale", "matrix")) or max(map(abs, rn.get("translation", [0, 0, 0]))) > 1e-6:
        fails.append("the root must be an empty with an identity transform")
    # names and transforms
    for i, n in enumerate(nodes):
        nm = name[i]
        if i == root:
            continue
        if not re.fullmatch(r"[A-Za-z][A-Za-z0-9_]*", nm):
            fails.append(f"node name '{nm}': letters, digits and underscores only")
        if any(nm.lower().endswith("_" + h) for h in HINTS):
            fails.append(f"node name '{nm}' ends with a Godot import hint")
        if "matrix" in n:
            fails.append(f"{nm} has a matrix transform")
        s = n.get("scale")
        if s and max(abs(c - 1.0) for c in s) > 1e-4:
            fails.append(f"{nm} has a rest scale {s}")

    def is_part(i):
        return i != root and not name[i].startswith("socket_")

    def part_parent(i):
        p = parent.get(i)
        while p is not None and not is_part(p):
            p = parent.get(p)
        return name[p] if p is not None else None

    def under_socket(i):
        p = parent.get(i)
        while p is not None:
            if name[p].startswith("socket_"):
                return True
            p = parent.get(p)
        return False
    parts = [i for i in range(len(nodes)) if is_part(i) and not under_socket(i)]
    for i in range(len(nodes)):
        if under_socket(i):
            fails.append(f"{name[i]} hangs below a socket")
    if "body" not in idx:
        fails.append("no part called 'body'")
    if all(name[i] in ("body", "part_a", "part_b") for i in parts):
        fails.append("only body / part_a / part_b: this is a version 1 model")
    # tree against the spec
    for nm, par in spec["tree"].items():
        if nm not in idx:
            fails.append(f"missing part '{nm}'")
        elif part_parent(idx[nm]) != par:
            fails.append(f"{nm} is under {part_parent(idx[nm])}, the spec says {par}")
    extra = [name[i] for i in parts if name[i] not in spec["tree"]]
    if extra:
        fails.append(f"parts not in the spec: {extra}")
    rotated = set(spec.get("rotated", []))
    for i in parts:
        r = nodes[i].get("rotation")
        has_rot = bool(r) and max(abs(r[0]), abs(r[1]), abs(r[2])) > 1e-5
        if has_rot and name[i] not in rotated:
            fails.append(f"{name[i]} has a rest rotation the spec does not list")
    for chain in spec.get("ik", []):
        for nm in chain:
            r = nodes[idx[nm]].get("rotation") if nm in idx else None
            if r and max(abs(r[0]), abs(r[1]), abs(r[2])) > 1e-5:
                fails.append(f"IK chain part {nm} has a rest rotation")
        for a, b in zip(chain[:-1], chain[1:]):
            if a in idx and b in idx and part_parent(idx[b]) != a:
                fails.append(f"IK chain: {b} is not a child of {a}")
    # budgets
    mesh_parts = [i for i in parts if verts[i]]
    total = sum(tris[i] for i in range(len(nodes)))
    if len(mesh_parts) > 32:
        fails.append(f"{len(mesh_parts)} mesh parts > 32")
    if total > 8000:
        fails.append(f"{total} triangles > 8000")
    for nm in spec.get("meshless", []):
        if nm in idx and verts[idx[nm]]:
            fails.append(f"{nm} should be a meshless joint")
    # bounds
    lo, hi = [math.inf] * 3, [-math.inf] * 3
    for i in mesh_parts:
        for v in verts[i]:
            w = gates.apply(world[i], v)
            for k in range(3):
                lo[k] = min(lo[k], w[k])
                hi[k] = max(hi[k], w[k])
    if not spec.get("floating") and abs(lo[1]) > 0.03:
        fails.append(f"rest pose does not touch the ground: min height {lo[1]:.3f}")
    if hi[1] > spec["height"] * 1.1 + 1e-3:
        fails.append(f"height {hi[1]:.3f} is more than 10% over the collision height {spec['height']}")
    reach_xz = max(max(abs(lo[0]), abs(hi[0])), max(abs(lo[2]), abs(hi[2])))
    if reach_xz > spec["radius"] * 1.1:
        notes.append(f"the rest pose reaches {reach_xz:.2f} m from the axis (collision radius {spec['radius']})")
    # eyes
    if spec.get("eyes"):
        if "head" not in idx:
            fails.append("eyes without a part called 'head'")
        else:
            hv = [gates.apply(world[idx["head"]], v) for v in verts[idx["head"]]]
            hc = tuple(sum(v[k] for v in hv) / len(hv) for k in range(3))
            for e, p in spec["eyes"].items():
                if e not in idx or p not in idx:
                    continue
                if part_parent(idx[p]) != e:
                    fails.append(f"{p} must be a child of {e}")
                m = world[idx[e]]
                pos = (m[12], m[13], m[14])
                out = sub(pos, hc)
                z, y = axis(m, 2), axis(m, 1)
                if dot(z, out) / max(norm(out), 1e-9) < 0.3:
                    fails.append(f"{e}: local +Z does not point out of the head")
                if y[1] < 0.5:
                    fails.append(f"{e}: local +Y is not up (world y of the axis = {y[1]:.2f})")
                ev = verts[idx[e]]
                reach = max(norm(v) for v in ev)
                c = tuple(sum(v[k] for v in ev) / len(ev) for k in range(3))
                if norm((c[0], c[1], 0.0)) > 0.3 * reach:
                    fails.append(f"{e}: the mesh is not centred on its origin")
                if max(norm(v) for v in verts[idx[p]]) > reach * 1.05:
                    fails.append(f"{p} is larger than its eye")
    # sockets
    for s, par in spec.get("sockets", {}).items():
        nm = "socket_" + s
        if nm not in idx:
            fails.append(f"missing {nm}")
        elif part_parent(idx[nm]) != par:
            fails.append(f"{nm} is under {part_parent(idx[nm])}, the spec says {par}")
    # soft parts
    for nm, sp in spec.get("soft", {}).items():
        if nm not in idx or not verts[idx[nm]]:
            fails.append(f"soft part {nm} has no mesh")
            continue
        vs = verts[idx[nm]]
        if sp.get("dir"):
            # the spec's direction is in model space; soft parts have no rest rotation
            d = sp["dir"]
            dl = norm(d)
            along = [dot(v, d) / dl for v in vs]
        else:
            along = [norm(v) for v in vs]
        run = max(along)
        if min(norm(v) for v in vs) > 0.06:
            fails.append(f"soft part {nm}: no vertex within 6 cm of its pin (the origin)")
        bands = len({min(9, int(a / run * 10)) for a in along if a > 0.0}) if run > 1e-6 else 0
        if bands < sp.get("loops", 6):
            fails.append(f"soft part {nm}: {bands} bands of vertices along its {run:.2f} m run, needs {sp.get('loops', 6)}")
    # ball joints
    for nm, r in spec.get("balls", {}).items():
        if nm not in idx:
            continue
        i = idx[nm]
        on_ball = sum(1 for v in verts[i] if 0.9 * r <= norm(v) <= 1.03 * r)
        if on_ball < 12:
            fails.append(f"{nm}: no ball of radius {r} round its pivot")
        par = part_parent(i)
        if par is None or not verts[idx[par]]:
            notes.append(f"{nm}: ball joint under a meshless parent")
            continue
        piv = (world[i][12], world[i][13], world[i][14])
        near = min(norm(sub(gates.apply(world[idx[par]], v), piv)) for v in verts[idx[par]])
        if near > r * 0.8:
            fails.append(f"{nm}: its parent {par} does not reach into the ball (nearest vertex {near:.3f} m from the pivot, ball {r})")
    tiny = [name[i] for i in mesh_parts if max(norm(v) for v in verts[i]) < 0.10]
    if tiny:
        notes.append("cast no shadow (reach under 0.10 m): " + ", ".join(tiny))
    return {"pass": not fails, "fails": fails, "notes": notes, "tris": total, "mesh_parts": len(mesh_parts), "nodes": len(parts),
            "height": round(hi[1], 4), "min_height": round(lo[1], 4)}


def main(ids):
    ids = ids or sorted(p.stem for p in (HERE / "rig_spec").glob("monster_*.json"))
    report, ok = {}, True
    for aid in ids:
        spec = json.loads((HERE / "rig_spec" / f"{aid}.json").read_text())
        r = check(aid, spec)
        report[aid] = r
        ok &= r["pass"]
        print(f"{'PASS' if r['pass'] else 'FAIL'} {aid:<20} {r['mesh_parts']} mesh parts, {r['nodes']} nodes, {r['tris']} tris, "
              f"height {r['height']:.3f}, min {r['min_height']:+.3f}")
        for f in r["fails"]:
            print("     -", f)
        for n in r["notes"]:
            print("     ~", n)
    (HERE / "rig_check.json").write_text(json.dumps(report, indent=1))
    return 0 if ok else 1


if __name__ == "__main__":
    sys.exit(main(sys.argv[1:]))
