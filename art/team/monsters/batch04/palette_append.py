"""Append Lane M's new palette cells (FIDELITY_PASS decision 10): row 14 first (the line that starts
with "bat_fur:"), then row 13 (the line that starts with "thorn_bark:"). Append only: existing cells
are never changed, a name that is already in the palette is skipped, and only the one line that
gains cells is rewritten (the other lane edits other rows of the same file).

    python art/team/monsters/batch04/palette_append.py     then     python art/tools/make_palette.py

Add a cell by appending to CELLS below (name:#rrggbb, or name:#rrggbb:flicker for a glow cell) and
running both commands again. Order matters only for where a new cell lands.
"""
import json
import re
from pathlib import Path

CELLS = [
    # --- M2, Rattlebones
    "bone_warm:#f4dcb2",          # warm ivory bone (the lit tone of the toy skeleton)
    "bone_tan:#d7a877",           # its warm shade: creases, undersides, socket floors
    "glow_teal_dim:#1f7f7a:0.0",  # dim teal iris the bright teal pupil sits on (reads under bloom)
    # bone_warm proved too light: under the showcase key light it clips to flat white and the form is
    # lost. The skeleton now uses the three below (bone_warm / bone_tan are left for anyone who took them).
    "ivory:#d0b086",              # warm ivory, dark enough that the lit side keeps its shading
    "ivory_shade:#a67c5a",        # its shade cell: undersides, creases, socket rims (Rig.shade)
    "ivory_deep:#6c4837",         # sutures, the gaps between ribs, the deepest creases
    # --- Mummy: linen bandages in four tones (cream, kept darker than the mock so the key light and
    # the bloom do not clip them), the dark hollow the eye sits in, and a two-tone amber eye glow
    "linen_light:#d6c4a0",        # the top layer of wraps
    "linen:#c4ae88",              # the middle layer
    "linen_shade:#a08663",        # the bottom layer, strip edges, undersides
    "linen_deep:#6f5843",         # the under-wrap that shows in the cracks between strips
    "linen_dark:#2e201d",         # the hollow behind the eye gap
    "glow_amber_dim:#a8590f:0.0", # outer iris: a dim amber the bright ring and the ink pupil sit on
    "glow_amber:#ff9a26:0.0",     # inner iris ring
    # --- Scarecrow: the witch hat. witch_purple (#6d4aa8) reads lilac under the showcase key light and
    # the felt layer's sheen; this is the mock's violet, darker and more saturated.
    "hat_violet:#4f2a8e",
    # --- Toadstool: the cap. leaf_crimson (#c2302a) went salmon pink under the showcase key light and
    # sheen; a deeper red with very little green or blue stays red.
    "toad_red:#a5181b",
    # --- Goo Slime: the jelly. apple_green (#8fbf3f) went milky pale lime in the face close-up under the
    # key light, the rim and the sheen; a darker, more saturated lime keeps its colour and its form.
    "goo_lime:#6fac1e",
    # --- Spider: the head fur, a berry plum (the mock's magenta head, darker so the key light keeps it
    # berry and does not wash it to pink); the abdomen and legs stay hat_violet.
    "spider_berry:#6c2a62",
]
ROWS = [(14, "bat_fur:", 16), (13, "thorn_bark:", 16)]   # (row index, first cell of that row, capacity)

path = Path(__file__).resolve().parents[3] / "palette" / "palette.json"
text = path.read_text()
data = json.loads(text)
have = {e.split(":")[0] for row in data["rows"] for e in row}
todo = [c for c in CELLS if c.split(":")[0] not in have]
lines = text.split("\n")
added = []
for row_index, first, cap in ROWS:
    if not todo:
        break
    idx = [i for i, l in enumerate(lines) if re.match(r'\s*\["' + re.escape(first), l)]
    assert len(idx) == 1, f"row starting with {first} not found exactly once"
    i = idx[0]
    row = json.loads(lines[i].rstrip().rstrip(","))
    assert row == data["rows"][row_index], f"row {row_index} is not where it is expected"
    take = todo[:max(0, cap - len(row))]
    if not take:
        continue
    todo = todo[len(take):]
    indent = re.match(r"\s*", lines[i]).group(0)
    trail = "," if lines[i].rstrip().endswith(",") else ""
    lines[i] = indent + json.dumps(row + take) + trail
    added += [(row_index, c) for c in take]
assert not todo, f"no room left for {todo}"
new = "\n".join(lines)
check = json.loads(new)
for r, row in enumerate(data["rows"]):
    assert check["rows"][r][:len(row)] == row, f"row {r} changed"
if added:
    path.write_text(new)
for row_index, c in added:
    print(f"row {row_index} += {c}")
print("nothing to add" if not added else f"{len(added)} cells added; now run art/tools/make_palette.py")
