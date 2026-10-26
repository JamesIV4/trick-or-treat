"""Sets palette row 10 (decor lane) without touching other rows, then rebuilds the palette.

    python art/team/decor/batch03/set_palette_row.py

Edits only the row-10 line in place (other workers edit other rows concurrently)."""
import json
import pathlib
import subprocess
import sys

ROOT = pathlib.Path(__file__).resolve().parents[4]
P = ROOT / "art" / "palette" / "palette.json"
ROW = 10
ENTRIES = [
    "bunting_cream:#f6ead0",
    "glow_jack_shell:#a0400c:0.08",
    "glow_jack_face:#ffd864:0.05",
]

text = P.read_text(encoding="utf-8")
data = json.loads(text)
old = data["rows"][ROW]
assert old and old[0].startswith("bunting_cream"), f"row {ROW} is not ours: {old}"
old_line = json.dumps(old)
new_line = json.dumps(ENTRIES)
if old_line not in text:
    # fall back to compact match ignoring spacing
    raise SystemExit(f"could not find row text {old_line}")
assert text.count(old_line) == 1
text = text.replace(old_line, new_line)
json.loads(text)
P.write_text(text, encoding="utf-8", newline="\n")
print("row", ROW, "->", ENTRIES)
subprocess.check_call([sys.executable, str(ROOT / "art" / "tools" / "make_palette.py")])
