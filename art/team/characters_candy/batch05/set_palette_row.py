"""Append Lane P's kit v2 cells to palette row 11 (the line that starts with "mum_orange:"), leaving every
other line byte-for-byte untouched (the monster lane edits rows 13/14 at the same time). Append only:
existing cells never change, and cells already present are skipped, so it is safe to re-run.
Run: python art/team/characters_candy/batch05/set_palette_row.py  then  python art/tools/make_palette.py
(run make_palette under tools\\godot_lock.ps1 so no Godot import reads a half-written texture).
"""
import json
import re
from pathlib import Path

APPEND = [
    # kid face v2 (shared by all six costumes)
    "iris_brown:#5c3018",        # iris ring round the ink pupil
    "hair_light:#9c6844",        # highlight streak on the bangs
    "hair_dark:#4a2d20",         # under-locks, brows
    "mouth_dark:#5b1f2e",        # inside of the mouth
    "tongue:#ff7a8a",
    "cheek_soft:#ffbcae",        # outer blush (between skin and cheek_pink)
    # dragon_kid v2
    "dragon_jade_light:#55c98a",  # hood rim, cuffs (lighter jade trim)
]

path = Path(__file__).resolve().parents[3] / "palette" / "palette.json"
text = path.read_text()
lines = text.split("\n")
idx = [i for i, l in enumerate(lines) if re.match(r'\s*\["mum_orange:', l)]
assert len(idx) == 1, idx
i = idx[0]
indent = re.match(r"\s*", lines[i]).group(0)
trail = "," if lines[i].rstrip().endswith(",") else ""
row = json.loads(lines[i].strip().rstrip(","))
have = {c.split(":")[0] for c in row}
row += [c for c in APPEND if c.split(":")[0] not in have]
assert len(row) <= 16, len(row)
lines[i] = indent + json.dumps(row) + trail
new = "\n".join(lines)
names = [c.split(":")[0] for r in json.loads(new)["rows"] for c in r]
assert len(names) == len(set(names)), "duplicate palette name"
path.write_text(new)
print("row 11 ->", len(row), "cells:", [c.split(":")[0] for c in row])
