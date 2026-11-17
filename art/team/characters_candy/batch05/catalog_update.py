"""Update this lane's v2 costume entries in art/asset_catalog.json: only `tris`, `parts` and `sockets`
of the ids listed in V2 (FIDELITY_PASS.md decision 9). Load, change, write back in one go (the monster
lane edits the same file); refuses to write if a plain JSON round-trip would change anything else.
Add a costume to V2 when it has been rebuilt on kit v2, then run:
  python art/team/characters_candy/batch05/catalog_update.py && python art/tools/publish_catalog.py
"""
import json
from pathlib import Path

ROOT = Path(__file__).resolve().parents[4]
P = ROOT / "art" / "asset_catalog.json"

TRIS = 30000
BASE = ["body", "head", "leg_l", "leg_r", "arm_l", "arm_r", "bucket", "bucket_candy"]
V2_PARTS = ["shin_l", "shin_r", "forearm_l", "forearm_r", "hand_l", "hand_r", "eye_l", "eye_r", "iris_l", "iris_r",
            "alt_lash_l", "alt_lash_r", "brow_l", "brow_r", "mouth_smile", "alt_mouth_open", "alt_mouth_o", "alt_mouth_grit"]
SOCKETS = ["bucket_light", "wand"]
# id -> the costume's own optional / soft / bob parts
V2 = {
    "dragon_kid": ["wing_l", "wing_r", "tail"],
    "kid_ghost": ["soft_sheet", "soft_cowl"],
    "kid_cat": ["hood", "tail", "soft_ear_l", "soft_ear_r", "bob_bow", "bob_bell"],
    "kid_witch": ["hair_cap", "bob_hat", "soft_tip", "soft_hair_back", "soft_hair_l", "soft_hair_r", "soft_cape", "soft_skirt", "bob_bow"],
    "kid_skeleton": ["soft_cord_l", "soft_cord_r"],
    "kid_pumpkin": ["matte_carve", "soft_stem", "bob_leaf_a", "bob_leaf_b", "soft_vine_a", "soft_vine_b"],
}

text = P.read_text(encoding="utf-8")
data = json.loads(text)
indent = 1 if text.startswith("{\n \"") else 2
tail = "\n" if text.endswith("\n") else ""
if json.dumps(data, indent=indent, ensure_ascii=False) + tail != text:
    raise SystemExit("catalog does not round-trip byte-identically; refusing to rewrite it")
ids = {a["id"]: a for a in data["assets"]}
for aid, own in V2.items():
    a = ids[aid]
    a["tris"] = TRIS
    a["parts"] = BASE[:2] + own + BASE[2:] + V2_PARTS
    a["sockets"] = SOCKETS
    print(aid, "tris", a["tris"], "parts", len(a["parts"]), "sockets", a["sockets"])
P.write_text(json.dumps(data, indent=indent, ensure_ascii=False) + tail, encoding="utf-8")
