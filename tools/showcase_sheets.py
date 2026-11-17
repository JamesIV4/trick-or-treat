"""Review sheets for the character showcase (tools/showcase.ps1).

  python tools/showcase_sheets.py art/review/showcase/before [id ...]

For every <id>.mp4 + <id>.beats.json in the folder (or just the ids named) it writes
<id>_beats.png: the middle frame of each beat, tiled and labelled with the beat's name and frame.
It also checks each clip against its beat list (the hard camera cut from the turntable to the face
close-up must fall exactly on the face beat's first frame) and exits 1 if any clip is out.
With no ids named it also writes all_front.png: every <id>_front.png tiled and labelled.
Needs ffmpeg on PATH and Pillow.
"""
import json
import subprocess
import sys
import tempfile
from pathlib import Path

from PIL import Image, ImageDraw, ImageFont

ORDER = [
    "dragon_kid", "kid_ghost", "kid_cat", "kid_witch", "kid_skeleton", "kid_pumpkin",
    "monster_pumpkin", "monster_bat", "monster_skeleton", "monster_ghost", "monster_gremlin",
    "monster_mummy", "monster_slime", "monster_scarecrow", "monster_spider", "monster_toadstool",
]


def font(size):
    for name in ("arialbd.ttf", "arial.ttf", "DejaVuSans-Bold.ttf"):
        try:
            return ImageFont.truetype(name, size)
        except OSError:
            pass
    return ImageFont.load_default()


def label(img, text, size=22):
    draw = ImageDraw.Draw(img)
    f = font(size)
    box = draw.textbbox((0, 0), text, font=f)
    draw.rectangle((0, 0, box[2] + 16, box[3] + 12), fill=(0, 0, 0))
    draw.text((8, 4), text, font=f, fill=(255, 235, 150))


def tile(images, cols, pad=6, bg=(18, 18, 26)):
    w, h = images[0].size
    rows = (len(images) + cols - 1) // cols
    sheet = Image.new("RGB", (cols * w + (cols + 1) * pad, rows * h + (rows + 1) * pad), bg)
    for i, im in enumerate(images):
        sheet.paste(im, (pad + (i % cols) * (w + pad), pad + (i // cols) * (h + pad)))
    return sheet


def beat_sheet(folder, cid, tile_w=400):
    beats = json.loads((folder / f"{cid}.beats.json").read_text())
    mp4 = folder / f"{cid}.mp4"
    frames = [(b["name"], (b["start_frame"] + b["end_frame"] + 1) // 2) for b in beats["beats"]]
    tiles = []
    with tempfile.TemporaryDirectory() as tmp:
        expr = "+".join(f"eq(n\\,{n})" for _, n in frames)
        subprocess.run(["ffmpeg", "-hide_banner", "-loglevel", "error", "-y", "-i", str(mp4), "-vf", f"select='{expr}'",
                        "-vsync", "0", str(Path(tmp) / "f_%03d.png")], check=True)
        files = sorted(Path(tmp).glob("f_*.png"))
        if len(files) != len(frames):
            raise RuntimeError(f"{cid}: expected {len(frames)} frames, got {len(files)}")
        for (name, n), path in zip(frames, files):
            im = Image.open(path).convert("RGB")
            im = im.resize((tile_w, round(im.height * tile_w / im.width)), Image.LANCZOS)
            label(im, f"{name}  f{n}")
            tiles.append(im)
    cols = 5 if len(tiles) > 5 else len(tiles)
    sheet = tile(tiles, cols)
    out = folder / f"{cid}_beats.png"
    sheet.save(out)
    return out


def check_cut(folder, cid):
    """The camera cuts hard from the turntable to the face close-up: that jump must sit exactly on
    the second beat's first frame, or the clip is a frame out against its beat list."""
    beats = json.loads((folder / f"{cid}.beats.json").read_text())["beats"]
    at = beats[1]["start_frame"]
    w, h = 96, 108
    raw = subprocess.run(["ffmpeg", "-hide_banner", "-loglevel", "error", "-i", str(folder / f"{cid}.mp4"), "-vf",
                          f"select='between(n\\,{at - 4}\\,{at + 3})',scale={w}:{h}", "-vsync", "0", "-f", "rawvideo", "-pix_fmt", "gray", "-"],
                         capture_output=True, check=True).stdout
    frames = [raw[i * w * h:(i + 1) * w * h] for i in range(len(raw) // (w * h))]
    diffs = [sum(abs(a - b) for a, b in zip(x, y)) for x, y in zip(frames, frames[1:])]
    jump = at - 4 + diffs.index(max(diffs)) + 1
    return jump == at, jump, at


def front_sheet(folder, tile_w=400):
    tiles = []
    for cid in ORDER:
        path = folder / f"{cid}_front.png"
        if not path.exists():
            continue
        im = Image.open(path).convert("RGB")
        im = im.resize((tile_w, round(im.height * tile_w / im.width)), Image.LANCZOS)
        label(im, cid)
        tiles.append(im)
    if not tiles:
        return None
    out = folder / "all_front.png"
    tile(tiles, 8 if len(tiles) > 8 else len(tiles)).save(out)
    return out


def main():
    if len(sys.argv) < 2:
        print(__doc__)
        return 2
    folder = Path(sys.argv[1])
    ids = sys.argv[2:] or [c for c in ORDER if (folder / f"{c}.mp4").exists() and (folder / f"{c}.beats.json").exists()]
    bad = 0
    for cid in ids:
        ok, jump, at = check_cut(folder, cid)
        bad += 0 if ok else 1
        print(beat_sheet(folder, cid), "frames aligned" if ok else f"NOT ALIGNED: the face cut is at frame {jump}, the beat list says {at}")
    if len(sys.argv) == 2:
        print(front_sheet(folder))
    return 1 if bad else 0


if __name__ == "__main__":
    sys.exit(main())
