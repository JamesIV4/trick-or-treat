"""Append the kid_ghost v2 cells (copy of set_palette_row.py) to palette row 11 (the line that starts with "mum_orange:"), leaving every
other line byte-for-byte untouched (the monster lane edits rows 13/14 at the same time). Append only:
existing cells never change, and cells already present are skipped, so it is safe to re-run.
Run: python art/team/characters_candy/batch05/set_palette_row.py  then  python art/tools/make_palette.py
(run make_palette under tools\\godot_lock.ps1 so no Godot import reads a half-written texture).
"""
import json
import re
from pathlib import Path

APPEND = [
    # kid_ghost v2: a cool off-white sheet that does not clip under the street lights, fold / inside
    # shades, and leggings that are dark plum rather than flat black
    "ghost_sheet:#e4e2ec",
    "ghost_shade:#c3c0d6",
    "ghost_deep:#9894b6",
    "ghost_legging:#3d3749",
    # kid_ghost one-sheet (P5): ghost_shade still clipped to white under the showcase lights
    "ghost_cloth:#aeaac4",
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

