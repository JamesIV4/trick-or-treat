"""Candy comparison board: the top-left (night) and bottom-left (studio) close crops of each candy's
engine preview, the concept candies, and the same row shrunk to gameplay size (~22 px per candy).
python art/team/characters_candy/batch03/candy_board.py <out.png> <id> [<id> ...]
"""
import sys
from pathlib import Path
from PIL import Image

ROOT = Path(__file__).resolve().parents[4]
out, ids = sys.argv[1], sys.argv[2:]
tiles = []
for i in ids:
    im = Image.open(ROOT / "art" / "review" / "engine" / f"{i}.png").convert("RGB")
    night = im.crop((280, 120, 520, 330))
    studio = im.crop((280, 570, 520, 780))
    t = Image.new("RGB", (240, 420))
    t.paste(night, (0, 0))
    t.paste(studio, (0, 210))
    tiles.append(t)
concept = Image.open(ROOT / "art" / "review" / "concept" / "target.png").convert("RGB")
for box in ((840, 500, 1010, 570), (650, 225, 900, 300)):
    c = concept.crop(box)
    c = c.resize((240, int(240 * c.height / c.width)), Image.LANCZOS)
    t = Image.new("RGB", (240, 420), (20, 20, 30))
    t.paste(c, (0, 0))
    tiles.append(t)
W = 240 * len(tiles)
board = Image.new("RGB", (W, 420 + 140), (20, 20, 30))
for k, t in enumerate(tiles):
    board.paste(t, (240 * k, 0))
    # gameplay size: candy ~0.33 m at ~15 m -> ~22 px; the crop shows the candy ~150 px wide
    small = t.crop((0, 0, 240, 210)).resize((36, 32), Image.LANCZOS).resize((144, 128), Image.NEAREST)
    board.paste(small, (240 * k + 48, 426))
board.save(out)
print(out, board.size)
