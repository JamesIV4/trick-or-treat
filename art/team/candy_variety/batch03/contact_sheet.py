"""Contact sheets from the Blender tiles (tiles/<id>_0.png front 3/4, _1.png turned 90 deg; camera 30 deg
above, orthographic, fixed 0.75 m frame so relative sizes are true; palette tint x AO, no shader gloss).

python art/team/candy_variety/batch03/contact_sheet.py [tag]
  -> art/review/candy_variety/<tag>_tier0.png, <tag>_tier123.png, <tag>_side.png, <tag>_gameplay.png
Gameplay sheet: each tile shrunk to the size a candy has ~10 m from the gameplay camera (~95 px/m on a
900 px tall screen; tile is 341 px/m -> x0.28), then blown up x2 nearest-neighbour.
"""
import sys
from pathlib import Path

from PIL import Image, ImageDraw, ImageFont

HERE = Path(__file__).resolve().parent
ROOT = HERE.parents[3]
sys.path.insert(0, str(HERE))
from specs import SPECS  # noqa: E402

OUT = ROOT / "art" / "review" / "candy_variety"
OUT.mkdir(parents=True, exist_ok=True)
tag = sys.argv[1] if len(sys.argv) > 1 else "v1"
TILES = HERE / "tiles"
BG = (16, 18, 34)
try:
    FONT = ImageFont.truetype("arial.ttf", 13)
except OSError:
    FONT = ImageFont.load_default()


def grid(items, view, cols, cell, name, label=True):
    rows = (len(items) + cols - 1) // cols
    lh = 18 if label else 0
    sheet = Image.new("RGB", (cols * cell, rows * (cell + lh)), BG)
    d = ImageDraw.Draw(sheet)
    for k, s in enumerate(items):
        p = TILES / f"{s['id']}_{view}.png"
        if not p.exists():
            continue
        im = Image.open(p).convert("RGB").resize((cell, cell), Image.LANCZOS)
        x, y = (k % cols) * cell, (k // cols) * (cell + lh)
        sheet.paste(im, (x, y))
        if label:
            d.text((x + 4, y + cell + 2), f"{k + 1}. {s['id'][6:]} (t{s['tier']})", fill=(230, 230, 240), font=FONT)
    out = OUT / f"{tag}_{name}.png"
    sheet.save(out)
    print(out, sheet.size)


t0 = [s for s in SPECS if s["tier"] == 0]
t123 = [s for s in SPECS if s["tier"] > 0]
grid(t0, 0, 9, 200, "tier0")
grid(t123, 0, 7, 240, "tier123")
grid(SPECS, 1, 12, 150, "side")

# gameplay-size sheet
small = 72
cols = 14
rows = (len(SPECS) + cols - 1) // cols
sheet = Image.new("RGB", (cols * small * 2, rows * small * 2), (24, 40, 30))
for k, s in enumerate(SPECS):
    p = TILES / f"{s['id']}_0.png"
    if not p.exists():
        continue
    im = Image.open(p).convert("RGB")
    # replace the navy studio background with a night-grass tone so silhouettes read like in game
    px = im.load()
    for yy in range(im.height):
        for xx in range(im.width):
            r, g, b = px[xx, yy]
            if abs(r - 48) < 4 and abs(g - 52) < 4 and abs(b - 84) < 4:
                px[xx, yy] = (24, 40, 30)
    im = im.resize((small, small), Image.LANCZOS).resize((small * 2, small * 2), Image.NEAREST)
    sheet.paste(im, ((k % cols) * small * 2, (k // cols) * small * 2))
out = OUT / f"{tag}_gameplay.png"
sheet.save(out)
print(out, sheet.size)
