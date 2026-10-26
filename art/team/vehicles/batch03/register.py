"""Insert/update this lane's catalog entries in art/asset_catalog.json. Re-reads the file right before
writing so concurrent edits by other workers are kept; existing entries are updated in place (fields not
listed here are kept). hood_aabb / hood_hinge (Godot asset space, m) come from hood.json, which
build_cars.py writes. Usage: python register.py [id ...] (default: all)."""
import json
import sys
from pathlib import Path

ROOT = Path(__file__).resolve().parents[4]
CAT = ROOT / "art" / "asset_catalog.json"
HOOD = Path(__file__).resolve().parent / "hood.json"

ENTRIES = [
    {
        "id": "parked_car",
        "path": "props/parked_car",
        "lane": "yard_street",
        "tris": 4500,
        "collision": "box",
        "walkable": True,
        "parts": ["hood"],
        "brief": "Decorative parked car: rounded retro bubble hatchback ~3.6 x 1.7 x 1.5 m, teal body (house_teal) with a cream "
                 "roof, recessed dark glass with chrome window reveals and highlight streaks, chunky whitewall "
                 "wheels in crisp arches with dark wells, bug-eye headlights, smiling oval grille, wrap-around "
                 "chrome bumpers, door seams/handles/mirrors, 'BOO' plates and a jack-o'-lantern antenna topper. "
                 "Front faces -Y (Godot +Z), origin at the ground centre, flat roof is a platform. Lamps unlit.",
    },
    {
        "id": "parked_wagon",
        "path": "props/parked_wagon",
        "lane": "yard_street",
        "tris": 4700,  # was 4500; +opening hood and engine bay
        "collision": "box",
        "walkable": True,
        "parts": ["hood"],
        "brief": "Decorative parked car: retro station wagon ~4.2 x 1.8 x 1.6 m, cherry-red body (metal_red) with faux-wood "
                 "side panels in cream frames, cream roof with a low roof rack carrying a suitcase and two pumpkins (one a glowing jack; cargo tops out ~0.22 m above the roof), "
                 "recessed dark glass with chrome reveals, whitewall wheels, round lamps, chrome bumpers. "
                 "Front faces -Y (Godot +Z), origin at the ground centre, roof is a platform. Lamps unlit.",
    },
    {
        "id": "parked_pickup",
        "path": "props/parked_pickup",
        "lane": "yard_street",
        "tris": 4800,  # was 4500; +opening hood and engine bay
        "collision": "box",
        "walkable": True,
        "parts": ["hood"],
        "brief": "Decorative parked car: small vintage pickup ~4.0 x 1.8 x 1.6 m, sunny yellow body (quilt_yellow) with a white "
                 "cab roof, long rounded bonnet with a chrome grille, open plank-floored bed holding a hay bale, "
                 "pumpkins and a carved jack-o'-lantern (glow face) sitting on the bale. Front faces -Y (Godot +Z), origin at the ground "
                 "centre, cab roof is the highest platform. Lamps unlit.",
    },
]


def main(ids):
    hood = json.loads(HOOD.read_text(encoding="utf-8")) if HOOD.exists() else {}
    data = json.loads(CAT.read_text(encoding="utf-8"))
    index = {a["id"]: i for i, a in enumerate(data["assets"])}
    done = []
    for e in ENTRIES:
        if ids and e["id"] not in ids:
            continue
        e = dict(e)
        if e["id"] in hood:
            e["hood_aabb"] = hood[e["id"]]["hood_aabb"]
            e["hood_hinge"] = hood[e["id"]]["hood_hinge"]
        if e["id"] in index:
            data["assets"][index[e["id"]]].update(e)
        else:
            data["assets"].append(e)
        done.append(e["id"])
    CAT.write_text(json.dumps(data, indent=1, ensure_ascii=False) + "\n", encoding="utf-8")
    print("registered", done)


if __name__ == "__main__":
    main(sys.argv[1:])
