"""Before/after frame strips of one showcase beat, to judge motion (arcs, overlap, popping).

  python art/team/characters_candy/batch05/strips.py <id> <beat> [after_dir] [--n 16] [--cols 8]
                                                     [--from F] [--to F] [--crop] [--only after|before]

Reads art/review/showcase/before/<id>.mp4 and <after_dir>/<id>.mp4 (default art/review/showcase/wip_p)
with their beats.json, takes n evenly spaced frames of the beat (or of frames --from..--to counted
from the beat's start) and writes <after_dir>/strips/<id>_<beat>.png: the BEFORE rows on top, the
AFTER rows below, each tile labelled with its frame within the beat. --crop keeps the middle of the
picture (the character) at a larger scale. Needs ffmpeg on PATH and Pillow.
"""
import json
import subprocess
import sys
import tempfile
from pathlib import Path

from PIL import Image, ImageDraw, ImageFont

ROOT = Path(__file__).resolve().parents[4]


def font(size):
    for name in ("arialbd.ttf", "arial.ttf"):
        try:
            return ImageFont.truetype(name, size)
        except OSError:
            pass
    return ImageFont.load_default()


def grab(folder, cid, beat, n, lo, hi, crop, tile_w):
    beats = json.loads((folder / f"{cid}.beats.json").read_text())["beats"]
    b = next(x for x in beats if x["name"] == beat)
    length = b["end_frame"] - b["start_frame"] + 1
    lo = 0 if lo is None else lo
    hi = length - 1 if hi is None else min(hi, length - 1)
    rel = [round(lo + (hi - lo) * i / (n - 1)) for i in range(n)]
    rel = sorted(set(rel))
    frames = [b["start_frame"] + r for r in rel]
    tiles = []
    with tempfile.TemporaryDirectory() as tmp:
        expr = "+".join(f"eq(n\\,{f})" for f in frames)
        subprocess.run(["ffmpeg", "-hide_banner", "-loglevel", "error", "-y", "-i", str(folder / f"{cid}.mp4"), "-vf", f"select='{expr}'",
                        "-vsync", "0", str(Path(tmp) / "f_%03d.png")], check=True)
        files = sorted(Path(tmp).glob("f_*.png"))
        for r, path in zip(rel, files):
            im = Image.open(path).convert("RGB")
            if crop:
                w, h = im.size
                im = im.crop((int(w * 0.2), int(h * 0.22), int(w * 0.8), int(h * 0.82)))
            im = im.resize((tile_w, round(im.height * tile_w / im.width)), Image.LANCZOS)
            d = ImageDraw.Draw(im)
            d.rectangle((0, 0, 44, 20), fill=(0, 0, 0))
            d.text((4, 2), str(r), font=font(14), fill=(255, 235, 150))
            tiles.append(im)
    return tiles


def sheet(rows_of_tiles, labels, cols, pad=4, bg=(18, 18, 26)):
    w, h = rows_of_tiles[0][0].size
    total_rows = sum((len(t) + cols - 1) // cols for t in rows_of_tiles)
    out = Image.new("RGB", (cols * (w + pad) + pad, total_rows * (h + pad) + pad + 26 * len(rows_of_tiles)), bg)
    d = ImageDraw.Draw(out)
    y = pad
    for tiles, text in zip(rows_of_tiles, labels):
        d.text((pad + 2, y + 2), text, font=font(18), fill=(255, 235, 150))
        y += 26
        for i, im in enumerate(tiles):
            out.paste(im, (pad + (i % cols) * (w + pad), y + (i // cols) * (h + pad)))
        y += ((len(tiles) + cols - 1) // cols) * (h + pad)
    return out


def main():
    args = sys.argv[1:]
    opts = {"--n": 16, "--cols": 8, "--from": None, "--to": None}
    flags = {"--crop": False}
    only = None
    pos = []
    i = 0
    while i < len(args):
        a = args[i]
        if a in opts:
            opts[a] = int(args[i + 1])
            i += 2
        elif a == "--only":
            only = args[i + 1]
            i += 2
        elif a in flags:
            flags[a] = True
            i += 1
        else:
            pos.append(a)
            i += 1
    if len(pos) < 2:
        print(__doc__)
        return 2
    cid, beat = pos[0], pos[1]
    after = Path(pos[2]) if len(pos) > 2 else ROOT / "art/review/showcase/wip_p"
    before = ROOT / "art/review/showcase/before"
    tile_w = 1920 // opts["--cols"] - 4
    rows, labels = [], []
    if only != "after":
        rows.append(grab(before, cid, beat, opts["--n"], opts["--from"], opts["--to"], flags["--crop"], tile_w))
        labels.append(f"BEFORE  {cid}  {beat}")
    if only != "before":
        rows.append(grab(after, cid, beat, opts["--n"], opts["--from"], opts["--to"], flags["--crop"], tile_w))
        labels.append(f"AFTER   {cid}  {beat}   ({after.name})")
    out_dir = after / "strips"
    out_dir.mkdir(parents=True, exist_ok=True)
    out = out_dir / f"{cid}_{beat}.png"
    sheet(rows, labels, opts["--cols"]).save(out)
    print(out)
    return 0


if __name__ == "__main__":
    sys.exit(main())
