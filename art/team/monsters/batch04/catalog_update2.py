"""Update `tris`, `parts` and `sockets` of converted (v2) monsters in art/asset_catalog.json, from the
rig specs the build wrote (rig_spec/<id>.json). FIDELITY_PASS decision 9: only this lane's ids, only
those three fields, one load-modify-write (the other lane edits the same file).

    python art/team/monsters/batch04/catalog_update2.py [monster_skeleton ...]    (default: every spec)
    python art/tools/publish_catalog.py
"""
import json
import sys
from pathlib import Path

HERE = Path(__file__).resolve().parent
P = HERE.parents[3] / "art" / "asset_catalog.json"
TRIS = 8000

ids = sys.argv[1:] or sorted(p.stem for p in (HERE / "rig_spec").glob("monster_*.json"))
specs = {i: json.loads((HERE / "rig_spec" / f"{i}.json").read_text()) for i in ids}
text = P.read_text(encoding="utf-8")
data = json.loads(text)
if json.dumps(data, indent=1) + "\n" != text:
    raise SystemExit("catalog does not round-trip byte-identically; refusing to rewrite it")
by_id = {a["id"]: a for a in data["assets"]}
for i, spec in specs.items():
    a = by_id[i]
    assert a["lane"] == "monsters"
    a["tris"] = TRIS
    a["parts"] = list(spec["tree"])
    if spec["sockets"]:
        a["sockets"] = sorted(spec["sockets"])
    else:
        a.pop("sockets", None)
    print(f"{i}: tris {TRIS}, {len(a['parts'])} parts, sockets {sorted(spec['sockets'])}")
P.write_text(json.dumps(data, indent=1) + "\n", encoding="utf-8")
