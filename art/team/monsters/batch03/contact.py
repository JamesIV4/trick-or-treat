"""Contact sheet of engine previews: for each id, the chosen quadrants (0 night front, 1 night back,
2 studio front, 3 low side) cropped around the centre, tiled in rows.

python art/team/monsters/batch03/contact.py <out.png> <id> [<id> ...] [--q 0,1] [--crop 560x400] [--far 15]
--far D additionally shrinks each tile to the size the asset would have at D metres from the
gameplay camera (approximation from the preview framing) and blows it back up (nearest), so the
read at gameplay distance can be judged.
"""
import json
import math
import sys
from pathlib import Path

from PIL import Image, ImageDraw

ROOT = Path(__file__).resolve().parents[4]
args = sys.argv[1:]


def opt(name, default):
    if name in args:
        i = args.index(name)
        v = args[i + 1]
        del args[i:i + 2]
        return v
    return default


quads = [int(q) for q in opt("--q", "0,1").split(",")]
cw, ch = [int(v) for v in opt("--crop", "560x400").split("x")]
far = float(opt("--far", "0"))
out, ids = args[0], args[1:]
cat = {a["id"]: a for a in json.loads((ROOT / "art" / "asset_catalog.json").read_text(encoding="utf-8"))["assets"]}
tiles = []
for aid in ids:
    im = Image.open(ROOT / "art" / "review" / "engine" / f"{aid}.png").convert("RGB")
    row = []
    for q in quads:
        qx, qy = (q % 2) * 800, (q // 2) * 450
        t = im.crop((qx + 400 - cw // 2, qy + 225 - ch // 2, qx + 400 + cw // 2, qy + 225 + ch // 2))
        if far:
            # preview camera distance ~ radius / tan(fov/2) * 1.25 (fov 50); radius from the gltf bounds
            g = json.loads((ROOT / "game" / "assets" / "models" / f"{cat[aid]['path']}.gltf").read_text())
            lo, hi = [1e9] * 3, [-1e9] * 3
            for acc in g["accessors"]:
                if acc.get("type") == "VEC3" and "min" in acc and "max" in acc:
                    lo = [min(a, b) for a, b in zip(lo, acc["min"])]
                    hi = [max(a, b) for a, b in zip(hi, acc["max"])]
            radius = max(math.dist(lo, hi) * 0.5, 0.5)
            dist = radius / math.tan(math.radians(25)) * 1.25
            k = min(1.0, dist / far)
            small = t.resize((max(1, int(cw * k)), max(1, int(ch * k))), Image.LANCZOS)
            t = small.resize((cw, ch), Image.NEAREST)
        d = ImageDraw.Draw(t)
        d.text((6, 6), f"{aid} q{q}", fill=(255, 255, 255))
        row.append(t)
    tiles.append(row)
W = cw * len(quads)
cols = max(1, 2 if len(quads) <= 2 else 1)
rows = math.ceil(len(tiles) / cols)
sheet = Image.new("RGB", (W * cols, ch * rows), (16, 16, 24))
for i, row in enumerate(tiles):
    x0, y0 = (i % cols) * W, (i // cols) * ch
    for j, t in enumerate(row):
        sheet.paste(t, (x0 + j * cw, y0))
sheet.save(out)
print(out, sheet.size)
