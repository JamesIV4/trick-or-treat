"""Gameplay-distance board: the night quadrants (front 3/4 and back 3/4) of each engine preview, shrunk so
the kid is about --px pixels tall (camera ~9.5 m back and above on a 900 px screen ~ 100 px), then blown
back up with nearest-neighbour so the pixels can be judged. dragon_kid first for reference.

python art/team/characters_candy/batch04/costume_board.py [--suffix _p5] [--px 100] [--out <png>]
"""
import sys
from pathlib import Path
from PIL import Image, ImageDraw

ROOT = Path(__file__).resolve().parents[4]
ENG = ROOT / "art" / "review" / "engine"
args = sys.argv[1:]


def opt(name, default):
    return args[args.index(name) + 1] if name in args else default


suffix = opt("--suffix", "")
px = float(opt("--px", 100))
out = Path(opt("--out", str(ENG / "costumes_distance.png")))
ids = ["dragon_kid", "kid_ghost", "kid_cat", "kid_witch", "kid_skeleton", "kid_pumpkin"]
k = px / 300.0          # the kid is ~300 px tall in an 800x450 preview quadrant
cols = []
for aid in ids:
    p = ENG / f"{aid}{suffix if aid != 'dragon_kid' else ''}.png"
    if not p.exists():
        p = ENG / f"{aid}.png"
    im = Image.open(p).convert("RGB")
    tiles = []
    for qx in (0, 1):
        q = im.crop((qx * 800 + 200, 40, qx * 800 + 600, 440))
        s = q.resize((max(1, int(400 * k)), max(1, int(400 * k))), Image.LANCZOS)
        tiles.append(s.resize((s.width * 2, s.height * 2), Image.NEAREST))
    col = Image.new("RGB", (tiles[0].width, tiles[0].height * 2 + 18), (20, 20, 30))
    col.paste(tiles[0], (0, 18))
    col.paste(tiles[1], (0, 18 + tiles[0].height))
    ImageDraw.Draw(col).text((4, 3), aid, fill=(230, 230, 230))
    cols.append(col)
W = sum(c.width for c in cols)
board = Image.new("RGB", (W, cols[0].height), (20, 20, 30))
x = 0
for c in cols:
    board.paste(c, (x, 0))
    x += c.width
board.save(out)
print(out, board.size)
