"""Register / refresh the candy_variety batch03 entries in art/asset_catalog.json (re-read right before
writing; other lanes edit it concurrently). Only this batch's ids are touched; existing entries with those
ids are updated in place, new ones are appended.

python art/team/candy_variety/batch03/register.py
"""
import json
import sys
from pathlib import Path

HERE = Path(__file__).resolve().parent
ROOT = HERE.parents[3]
sys.path.insert(0, str(HERE))
from specs import SPECS  # noqa: E402

BUDGET = {0: 500, 1: 500, 2: 700, 3: 700}
CAT = ROOT / "art" / "asset_catalog.json"

text = CAT.read_text(encoding="utf-8")
cat = json.loads(text)
index = {a["id"]: k for k, a in enumerate(cat["assets"])}
added = updated = 0
for s in SPECS:
    entry = {"id": s["id"], "path": f"candy/{s['id']}", "lane": "candy_variety", "tris": BUDGET[s["tier"]],
             "collision": "none", "floating": True, "candy_tier": s["tier"], "glow": s["glow"],
             "brief": s["brief"] + f" About {s['size']:.2f} m across; origin at its centre (it floats and spins)."}
    if s["id"] in index:
        cat["assets"][index[s["id"]]] = entry
        updated += 1
    else:
        cat["assets"].append(entry)
        added += 1
ascii_only = all(ord(ch) < 128 for ch in text)
out = json.dumps(cat, indent=1, ensure_ascii=ascii_only)
if text.endswith("\n"):
    out += "\n"
CAT.write_text(out, encoding="utf-8")
print(f"catalog: {added} added, {updated} updated, {len(cat['assets'])} assets total")
