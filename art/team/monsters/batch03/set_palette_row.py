"""Write this lane's palette row (row 14, the line that starts with "bat_fur:") in place, appending it
if it does not exist yet, leaving every other line byte-for-byte untouched (other workers edit other
rows concurrently).
Run: python art/team/monsters/batch03/set_palette_row.py  then  python art/tools/make_palette.py
"""
import json
import re
from pathlib import Path

ROW_INDEX = 14
ROW = [
    "bat_fur:#4e3d7c",        # dusky violet fur (bat)
    "bat_wing:#8d62b4",       # lighter plum membrane
    "bat_belly:#c3a0e6",      # lilac belly / inner ears
    "slime_green:#5fcf45",    # goo slime body
    "slime_light:#bdf76e",    # slime highlights / drips
    "slime_dark:#2d8f42",     # slime shade, puddle rim
    "glow_slime:#8dff52:0.25",  # glowing goo (cauldron, slime core)
    "bone_shade:#c4b190",     # bone joints / cracks, bandage shade
    "spider_plum:#3c2d5e",    # spider body (not pure black: reads at night)
    "crow_blue:#2e3150",      # crow feathers (blue-black)
    "grave_soil:#6e4a32",     # fresh grave dirt
    "sarc_blue:#2f63b0",      # sarcophagus lapis stripes
]

path = Path(__file__).resolve().parents[3] / "palette" / "palette.json"
text = path.read_text()
data = json.loads(text)
assert len(ROW) <= 16
lines = text.split("\n")
idx = [i for i, l in enumerate(lines) if re.match(r'\s*\["bat_fur:', l)]
if idx:
    i = idx[0]
    indent = re.match(r"\s*", lines[i]).group(0)
    trail = "," if lines[i].rstrip().endswith(",") else ""
    lines[i] = indent + json.dumps(ROW) + trail
else:
    assert len(data["rows"]) == ROW_INDEX, f"expected {ROW_INDEX} rows before appending, found {len(data['rows'])}"
    # the last row line is the one just before the closing "  ]" of rows
    close = max(i for i, l in enumerate(lines) if re.match(r"\s*\]\s*$", l))
    last = close - 1
    indent = re.match(r"\s*", lines[last]).group(0)
    lines[last] = lines[last].rstrip() + ","
    lines.insert(close, indent + json.dumps(ROW))
new = "\n".join(lines)
assert json.loads(new)["rows"][ROW_INDEX] == ROW
path.write_text(new)
print("row", ROW_INDEX, "->", len(ROW), "cells")
