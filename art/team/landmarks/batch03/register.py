"""Insert/update this lane's catalog entries in art/asset_catalog.json (re-reads the file right before
writing so concurrent edits by other workers are kept). Usage: python register.py"""
import json
from pathlib import Path

ROOT = Path(__file__).resolve().parents[4]
CAT = ROOT / "art" / "asset_catalog.json"

ENTRIES = [
    {
        "id": "travel_gate_base",
        "path": "props/travel_gate_base",
        "lane": "landmarks",
        "tris": 7000,
        "collision": "cylinder",
        "radius": 1.4,
        "height": 0.22,
        "walkable": True,
        "brief": "Travel gate pad + post (teleport point). Round runed flagstone pad 2.9 m across, 0.22 m tall, "
                 "walkable top, mossy fieldstone skirt, carved (unlit) rune grooves; stone pillar with a wrought-iron "
                 "crook post (~3.4 m) whose hook sits over (-0.15, 0, 0) at about 3.1 m. No glow cells: every glowing "
                 "part lives in travel_gate_lantern. Origin at the pad centre on the ground.",
    },
    {
        "id": "travel_gate_lantern",
        "path": "props/travel_gate_lantern",
        "lane": "landmarks",
        "tris": 2500,
        "collision": "none",
        "floating": True,
        "brief": "Glowing half of the travel gate, in the same space as travel_gate_base (drop both at the same "
                 "transform): big hexagonal hanging lantern centred at Godot (-0.15, 2.55, 0) with glow_magic_teal "
                 "panes, plus the teal rune inlays on the pad top (channel ring, 8 runes, centre star) and on the "
                 "footing stones. The game dims it while the area is locked (glow_boost).",
    },
    {
        "id": "shop_cart",
        "path": "props/shop_cart",
        "lane": "landmarks",
        "tris": 9500,
        "collision": "box",
        "brief": "Candy shop cart, front faces -Y (Godot +Z): painted wooden cart ~2.3 m wide with wagon wheels, "
                 "candy-cane posts, pink/white striped awning with scalloped valance (top ~2.6 m), a blank sign board "
                 "for the game's Label3D, candy jars, lollipops, pumpkins, a hanging lantern, and a flat round "
                 "doormat centred at Godot (0, 0, 1.25). Origin at the ground centre under the cart body.",
    },
]


def main():
    data = json.loads(CAT.read_text(encoding="utf-8"))
    ids = {a["id"]: i for i, a in enumerate(data["assets"])}
    for e in ENTRIES:
        if e["id"] in ids:
            data["assets"][ids[e["id"]]] = e
        else:
            data["assets"].append(e)
    CAT.write_text(json.dumps(data, indent=1, ensure_ascii=False) + "\n", encoding="utf-8")
    print("registered", [e["id"] for e in ENTRIES])


if __name__ == "__main__":
    main()
