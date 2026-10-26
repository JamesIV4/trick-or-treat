"""Aggregate results/<id>.json into results.json (run after build + gates)."""
import json
import sys
from collections import Counter
from pathlib import Path

HERE = Path(__file__).resolve().parent
sys.path.insert(0, str(HERE))
from specs import SPECS  # noqa: E402

out = []
for s in SPECS:
    r = json.loads((HERE / "results" / f"{s['id']}.json").read_text())
    out.append({"id": s["id"], "tier": s["tier"], "gltf": f"game/assets/models/candy/{s['id']}.gltf",
                "sheet": f"art/review/sheets/{s['id']}.png", "tris": r["tris"], "budget": r["budget"], "dims": r["dims"],
                "glow": s["glow"]})
(HERE / "results.json").write_text(json.dumps(out, indent=1))
print(len(out), dict(Counter(s["tier"] for s in SPECS)))
