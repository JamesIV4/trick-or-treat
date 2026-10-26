"""Numerical rig check of the exported costume glTFs against dragon_kid's export (plain Python).

  python art/team/characters_candy/batch04/verify_rig.py [ids...]   -> table + rig_check.json; exit 1 on failure

Per costume:
  * every required node exists, under the same parent as in dragon_kid
    (body, head, leg_l, leg_r, arm_l, arm_r under the root; bucket under arm_r; bucket_candy and
    socket_bucket_light under bucket); optional wing_l / wing_r / tail, when present, also under the root
  * every node's local translation equals dragon_kid's (joint origins), and every part node is
    translation-only (no rotation / scale), like dragon_kid
  * the root sits at the origin; min height ~0 (ground between the feet); overall height
  * hand reach: the farthest arm vertex from the shoulder (the kid's hand tip) matches the dragon's,
    so the wand (attached to arm_l at the left hand) and the bucket grip line up the same way
  * the bucket and candy heap meshes are identical to the dragon's (vertex count and bounds)
  * front = glTF +Z: the face's nose-most vertex (head mesh, max Z) is in front of the neck
"""
import json
import math
import sys
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parents[3] / "tools"))
import gates  # noqa: E402  (glTF accessor reader)

ROOT = Path(__file__).resolve().parents[4]
MODELS = ROOT / "game" / "assets" / "models" / "characters"
REQ = {"body": "ROOT", "head": "ROOT", "leg_l": "ROOT", "leg_r": "ROOT", "arm_l": "ROOT", "arm_r": "ROOT",
       "bucket": "arm_r", "bucket_candy": "bucket", "socket_bucket_light": "bucket"}
OPT = {"wing_l": "ROOT", "wing_r": "ROOT", "tail": "ROOT"}
TOL = 1e-5


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
    info = {}
    for i, n in enumerate(nodes):
        nm = n.get("name", "")
        p = parent.get(i)
        verts = []
        if "mesh" in n:
            for prim in g["meshes"][n["mesh"]]["primitives"]:
                verts += gates.read_accessor(g, bins, prim["attributes"]["POSITION"])
        info[nm] = {"parent": "ROOT" if p == root else (nodes[p]["name"] if p is not None else None),
                    "t": n.get("translation", [0, 0, 0]), "r": n.get("rotation"), "s": n.get("scale"),
                    "m": n.get("matrix"), "verts": verts}
    # world vertices (translation-only hierarchy is checked separately; compose translations)
    def world_t(nm):
        t = [0.0, 0.0, 0.0]
        while nm and nm != "ROOT":
            t = [a + b for a, b in zip(t, info[nm]["t"])]
            nm = info[nm]["parent"]
        return t
    lo, hi = [math.inf] * 3, [-math.inf] * 3
    for nm, d in info.items():
        if not d["verts"] or nm == nodes[root]["name"]:
            continue
        w = world_t(nm)
        for v in d["verts"]:
            for k in range(3):
                lo[k] = min(lo[k], v[k] + w[k])
                hi[k] = max(hi[k], v[k] + w[k])
    info["_root"] = {"name": nodes[root]["name"], "t": nodes[root].get("translation", [0, 0, 0]),
                     "r": nodes[root].get("rotation"), "s": nodes[root].get("scale")}
    info["_bounds"] = (lo, hi)
    return info


def reach(d):
    return max(math.sqrt(sum(c * c for c in v)) for v in d["verts"])


def far(d):
    return max(d["verts"], key=lambda v: sum(c * c for c in v))


def bounds(vs):
    return [min(v[k] for v in vs) for k in range(3)], [max(v[k] for v in vs) for k in range(3)]


def main(ids):
    ref = load("dragon_kid")
    ok_all = True
    report = {"dragon_kid": {n: ref[n]["t"] for n in list(REQ) + list(OPT)}}
    print("dragon_kid joints (glTF Y-up, +Z front):")
    for n in list(REQ) + list(OPT):
        print(f"  {n:<20} parent {ref[n]['parent']:<7} t = ({ref[n]['t'][0]:+.4f}, {ref[n]['t'][1]:+.4f}, {ref[n]['t'][2]:+.4f})")
    print(f"  hand tip L {tuple(round(c, 4) for c in far(ref['arm_l']))}  R {tuple(round(c, 4) for c in far(ref['arm_r']))}"
          f"  (arm-local, farthest vertex from the shoulder)")
    print(f"  height {ref['_bounds'][1][1]:.3f} m, min height {ref['_bounds'][0][1]:+.4f}")
    for aid in ids:
        c = load(aid)
        fails, notes = [], []
        if any(x is not None for x in (c["_root"]["r"], c["_root"]["s"])) or max(map(abs, c["_root"]["t"])) > TOL:
            fails.append("root not an identity transform at the origin")
        present_opt = [n for n in OPT if n in c]
        for n, par in list(REQ.items()) + [(n, OPT[n]) for n in present_opt]:
            if n not in c:
                fails.append(f"missing {n}")
                continue
            d = c[n]
            if d["parent"] != par:
                fails.append(f"{n} parent {d['parent']} != {par}")
            if d["r"] is not None or d["s"] is not None or d["m"] is not None:
                fails.append(f"{n} has rotation/scale/matrix")
            dt = max(abs(a - b) for a, b in zip(d["t"], ref[n]["t"]))
            if dt > TOL:
                fails.append(f"{n} origin off by {dt:.6f} m: {d['t']} vs {ref[n]['t']}")
            if n != "socket_bucket_light" and not d["verts"]:
                fails.append(f"{n} has no mesh")
        extra = [n for n in c if not n.startswith("_") and n not in REQ and n not in OPT and n != c["_root"]["name"]]
        if extra:
            notes.append(f"extra nodes {extra}")
        hand = {}
        for side in ("arm_l", "arm_r"):
            a, b = far(c[side]), far(ref[side])
            dh = math.dist(a, b)
            hand[side] = (round(dh, 4), [round(x, 4) for x in a])
            if dh > 0.01:
                fails.append(f"{side} hand tip {a} vs dragon {b} ({dh:.4f} m)")
        for n in ("bucket", "bucket_candy"):
            if len(c[n]["verts"]) != len(ref[n]["verts"]) or \
                    max(abs(x - y) for p, q in zip(bounds(c[n]["verts"]), bounds(ref[n]["verts"])) for x, y in zip(p, q)) > 1e-5:
                fails.append(f"{n} mesh differs from dragon_kid's")
        lo, hi = c["_bounds"]
        if abs(lo[1]) > 0.03:
            fails.append(f"min height {lo[1]:.3f}")
        hz = max(v[2] for v in c["head"]["verts"])
        if hz < 0.15:
            fails.append(f"head front max Z {hz:.3f}: face not toward +Z?")
        ok = not fails
        ok_all &= ok
        report[aid] = {"pass": ok, "fails": fails, "notes": notes, "optional": present_opt,
                       "height": round(hi[1], 4), "min_height": round(lo[1], 4), "hand_tip_delta": hand,
                       "head_front_z": round(hz, 4), "origins": {n: c[n]["t"] for n in list(REQ) + present_opt}}
        print(f"{'PASS' if ok else 'FAIL'} {aid:<13} optional {present_opt}  height {hi[1]:.3f} m  "
              f"min {lo[1]:+.4f}  hand tip delta L {hand['arm_l'][0]:.4f} R {hand['arm_r'][0]:.4f} m  head front +Z {hz:.3f}")
        for f in fails:
            print("     -", f)
        for nt in notes:
            print("     ~", nt)
    (Path(__file__).resolve().parent / "rig_check.json").write_text(json.dumps(report, indent=1))
    return 0 if ok_all else 1


if __name__ == "__main__":
    sys.exit(main(sys.argv[1:] or ["kid_ghost", "kid_cat", "kid_witch", "kid_skeleton", "kid_pumpkin"]))
