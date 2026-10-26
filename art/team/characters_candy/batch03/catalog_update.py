"""Add / update this lane's batch03 entries in art/asset_catalog.json with a minimal text edit
(read-modify-write in one go; other workers edit the catalog concurrently).
Run: python art/team/characters_candy/batch03/catalog_update.py
"""
import json
from pathlib import Path

ROOT = Path(__file__).resolve().parents[4]
P = ROOT / "art" / "asset_catalog.json"

NEW = [
    {
        "id": "candy_wrapped_orange",
        "path": "candy/candy_wrapped_orange",
        "lane": "characters_candy",
        "tris": 400,
        "collision": "none",
        "brief": "Same shape as candy_wrapped_pink (v3 round ball, bow-tie fans) in candy_orange with candy_orange_light fans. The concept's most common wrapped candy. Origin at its centre (it floats).",
    },
    {
        "id": "candy_corn",
        "path": "candy/candy_corn",
        "lane": "characters_candy",
        "tris": 300,
        "collision": "none",
        "brief": "Chunky classic candy corn about 0.3 m tall standing tip up: candy_corn_yellow base, candy_orange middle, candy_corn_white tip, lighter highlight column. Origin at its centre (it floats).",
    },
]
BUDGET = {"dragon_kid": 10000}

text = P.read_text(encoding="utf-8")
data = json.loads(text)
assets = data["assets"]
ids = {a["id"]: a for a in assets}
for a in assets:
    if a["id"] in BUDGET:
        a["tris"] = BUDGET[a["id"]]
insert_at = max(i for i, a in enumerate(assets) if a["id"].startswith("candy_")) + 1
for e in NEW:
    if e["id"] in ids:
        ids[e["id"]].update(e)
    else:
        assets.insert(insert_at, e)
        insert_at += 1
indent = 1 if text.startswith("{\n \"") else 2
P.write_text(json.dumps(data, indent=indent, ensure_ascii=False) + ("\n" if text.endswith("\n") else ""), encoding="utf-8")
print("catalog updated:", [e["id"] for e in NEW], BUDGET)
