"""Zoom crops of engine previews (the asset is small in the 4-up preview).
  python art/team/vines/batch03/crop.py out.png id [id ...] [--q 0,1,2,3] [--box x0,y0,x1,y1]
Each quadrant is 800x450; the default box crops around its centre and upscales 2x.
"""
import sys
from pathlib import Path

from PIL import Image

ROOT = Path(__file__).resolve().parents[4]
args = sys.argv[1:]
out = args.pop(0)
qs = [0, 1, 2, 3]
box = (200, 60, 600, 420)
if "--q" in args:
    i = args.index("--q")
    qs = [int(v) for v in args[i + 1].split(",")]
    del args[i:i + 2]
if "--box" in args:
    i = args.index("--box")
    box = tuple(int(v) for v in args[i + 1].split(","))
    del args[i:i + 2]
tiles = []
for aid in args:
    im = Image.open(ROOT / "art" / "review" / "engine" / f"{aid}.png").convert("RGB")
    for q in qs:
        ox, oy = (q % 2) * 800, (q // 2) * 450
        t = im.crop((ox + box[0], oy + box[1], ox + box[2], oy + box[3]))
        tiles.append(t.resize((t.width * 2, t.height * 2), Image.LANCZOS))
w, h = tiles[0].size
cols = len(qs)
rows = (len(tiles) + cols - 1) // cols
sheet = Image.new("RGB", (w * cols, h * rows))
for k, t in enumerate(tiles):
    sheet.paste(t, ((k % cols) * w, (k // cols) * h))
sheet.save(out)
print(out, sheet.size)
