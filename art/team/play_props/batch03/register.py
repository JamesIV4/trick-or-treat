"""Adds/updates the play_props batch03 entries in art/asset_catalog.json (re-reads the file right before
writing so concurrent edits by other lanes are kept).
    python art/team/play_props/batch03/register.py [ids...]     # no ids = all entries"""
import json
from pathlib import Path

ROOT = Path(__file__).resolve().parents[4]
CAT = ROOT / "art" / "asset_catalog.json"

ENTRIES = [
    {
        "id": "trampoline_large",
        "path": "props/trampoline_large",
        "lane": "play_props",
        "tris": 6000,
        "collision": "box",
        "walkable": True,
        "collision_top": 0.85,
        "bounce": 17.0,
        "brief": "Round backyard trampoline ~3.75 m across (pad outer radius 1.87 m), origin at ground centre. "
                 "Dark plum woven mat, top exactly 0.85 m, printed with a big jack-o'-lantern inside an orange "
                 "target ring and faint bats; bone-stitched purple border; ring of 40 steel springs; puffy "
                 "orange safety pad (alternating pumpkin panels, purple skirt) rising ~6 cm above the mat; "
                 "6 curved steel W-legs; small purple-step ladder on the front-right (-62 deg, reaches r 2.2 m). "
                 "No enclosure net. bounce = launch speed for gameplay.",
    },
    {
        "id": "trampoline_mini",
        "path": "props/trampoline_mini",
        "lane": "play_props",
        "tris": 1500,
        "collision": "box",
        "walkable": True,
        "collision_top": 0.28,
        "bounce": 14.0,
        "brief": "Mini fitness rebounder ~1.18 m across, origin at ground centre. Black woven mat, top exactly "
                 "0.28 m, with a bold lemon star inside a lemon ring; painted steel spring band; bright violet "
                 "padded rim (darker purple skirt) rising ~3 cm above the mat; six stubby splayed steel legs "
                 "with rubber feet. bounce = launch speed for gameplay.",
    },
    {
        "id": "fire_wand",
        "path": "props/fire_wand",
        "lane": "characters_candy",
        "tris": 600,
        "collision": "none",
        "floating": True,
        "sockets": ["tip"],
        "brief": "Chunky storybook fire wand (~0.43 m) the dragon kid holds in the LEFT hand. Points along Godot +Z; "
                 "origin at the centre of the wrapped leather grip (hand point). Gold pommel ball, leather grip "
                 "with tilted wraps, gold collar, twisted two-tone dark-wood shaft, scalloped gold cup holding a "
                 "faceted swirling flame crystal (glow_jack_hot core, glow_jack licks) whose tip curls up. "
                 "socket_tip at the crystal centre, Godot (0, 0, 0.30): spawn fireballs / muzzle flames there.",
    },
]


def main():
    import sys
    only = set(sys.argv[1:])
    data = json.loads(CAT.read_text(encoding="utf-8"))
    assets = data["assets"]
    for e in ENTRIES:
        if only and e["id"] not in only:
            continue
        for i, a in enumerate(assets):
            if a["id"] == e["id"]:
                assets[i] = e
                break
        else:
            assets.append(e)
    CAT.write_text(json.dumps(data, indent=1, ensure_ascii=False) + "\n", encoding="utf-8")
    print("registered", [e["id"] for e in ENTRIES if not only or e["id"] in only])


if __name__ == "__main__":
    main()
