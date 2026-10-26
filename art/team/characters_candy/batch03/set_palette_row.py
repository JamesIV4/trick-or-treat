"""Rewrite only this lane's palette row (row 12, the line that starts with "dragon_green:") in place,
leaving every other line byte-for-byte untouched (other workers edit other rows concurrently).
Run: python art/team/characters_candy/batch03/set_palette_row.py  then  python art/tools/make_palette.py
"""
import json
import re
from pathlib import Path

ROW = [
    # dragon_kid v3 (green onesie)
    "dragon_green:#4cb04a",        # main onesie (lead's cell, value kept)
    "dragon_jade:#30ad6b",         # v3 main onesie: emerald/jade like the concept (cooler than the lawn)
    "dragon_green_dark:#1f7a4c",   # soles, stitches
    "dragon_green_light:#a9d94f",  # wing membrane, cuffs, hood rim (yellow-green)
    "dragon_spike:#d4e04a",        # back / hood / tail spikes
    "dragon_gold:#ffbd2e",         # horns, claws, tail tip
    "dragon_cream:#fbe7a2",        # pale yellow-cream belly
    "dragon_cream_dark:#e9c86e",   # belly ridges
    # vivid wrapped candy (concept: orange, green, purple, pink)
    "candy_orange:#ff6f12",
    "candy_orange_light:#ffae3a",
    "candy_green:#2fd04a",
    "candy_green_light:#9bf26a",
    "candy_violet:#9a3cff",
    "candy_violet_light:#cf8cff",
    "candy_magenta:#ff3fa4",
]

path = Path(__file__).resolve().parents[3] / "palette" / "palette.json"
text = path.read_text()
lines = text.split("\n")
idx = [i for i, l in enumerate(lines) if re.match(r'\s*\["dragon_green:', l)]
assert len(idx) == 1, idx
i = idx[0]
indent = re.match(r"\s*", lines[i]).group(0)
trail = "," if lines[i].rstrip().endswith(",") else ""
lines[i] = indent + json.dumps(ROW) + trail
new = "\n".join(lines)
json.loads(new)  # still valid
assert len(ROW) <= 16
path.write_text(new)
print("row", i, "->", len(ROW), "cells")
