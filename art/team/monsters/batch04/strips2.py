"""Frame strip of one showcase beat, to judge motion (arcs, overlap, popping, joint gaps).

    python art/team/monsters/batch04/strips2.py <id> <beat> [--dir art/review/showcase/wip_m] [--n 24] [--cols 8]
           [--from F] [--to F] [--crop X0,Y0,X1,Y1] [--w 300] [--before] [--out file.png]

--from / --to are frames counted from the start of the beat (default: the whole beat). --crop is in
fractions of the frame (default 0.1,0.12,0.9,0.92: the monster stays near the middle). --before
puts the same frames of art/review/showcase/before in a second block underneath.
Writes <dir>/strips/<id>_<beat>.png and prints the path.
"""
import argparse
import json
import subprocess
import tempfile
from pathlib import Path

from PIL import Image, ImageDraw

ROOT = Path(__file__).resolve().parents[4]


def grab(mp4, frames, tmp, tag):
    expr = "+".join(f"eq(n\\,{n})" for n in frames)
    subprocess.run(["ffmpeg", "-hide_banner", "-loglevel", "error", "-y", "-i", str(mp4), "-vf", f"select='{expr}'",
                    "-vsync", "0", str(Path(tmp) / f"{tag}_%04d.png")], check=True)
    files = sorted(Path(tmp).glob(f"{tag}_*.png"))
    return [Image.open(f).convert("RGB") for f in files]


def block(imgs, frames, crop, w, cols, label):
    W, H = imgs[0].size
    box = (int(crop[0] * W), int(crop[1] * H), int(crop[2] * W), int(crop[3] * H))
    h = int(w * (box[3] - box[1]) / (box[2] - box[0]))
    rows = (len(imgs) + cols - 1) // cols
    out = Image.new("RGB", (cols * w, rows * h), (20, 22, 34))
    d = ImageDraw.Draw(out)
    for i, im in enumerate(imgs):
        t = im.crop(box).resize((w, h), Image.LANCZOS)
        x, y = (i % cols) * w, (i // cols) * h
        out.paste(t, (x, y))
        d.text((x + 4, y + 3), f"{label} f{frames[i]}", fill=(255, 235, 150))
    return out


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("id")
    ap.add_argument("beat")
    ap.add_argument("--dir", default="art/review/showcase/wip_m")
    ap.add_argument("--n", type=int, default=24)
    ap.add_argument("--cols", type=int, default=8)
    ap.add_argument("--from", dest="f0", type=int, default=None)
    ap.add_argument("--to", dest="f1", type=int, default=None)
    ap.add_argument("--crop", default="0.1,0.12,0.9,0.92")
    ap.add_argument("--w", type=int, default=300)
    ap.add_argument("--before", action="store_true")
    ap.add_argument("--out", default=None)
    a = ap.parse_args()
    d = ROOT / a.dir
    beats = {b["name"]: b for b in json.loads((d / f"{a.id}.beats.json").read_text())["beats"]}
    b = beats[a.beat]
    s = b["start_frame"] + (a.f0 or 0)
    e = b["start_frame"] + a.f1 if a.f1 is not None else b["end_frame"]
    n = min(a.n, e - s + 1)
    frames = sorted({s + round((e - s) * k / max(n - 1, 1)) for k in range(n)})
    crop = [float(x) for x in a.crop.split(",")]
    with tempfile.TemporaryDirectory() as tmp:
        blocks = [block(grab(d / f"{a.id}.mp4", frames, tmp, "a"), frames, crop, a.w, a.cols, "after" if a.before else a.beat)]
        if a.before:
            blocks.append(block(grab(ROOT / "art/review/showcase/before" / f"{a.id}.mp4", frames, tmp, "b"), frames, crop, a.w, a.cols, "BEFORE"))
    sheet = Image.new("RGB", (blocks[0].width, sum(x.height for x in blocks) + 6 * (len(blocks) - 1)), (255, 0, 255))
    y = 0
    for x in blocks:
        sheet.paste(x, (0, y))
        y += x.height + 6
    out = Path(a.out) if a.out else d / "strips" / f"{a.id}_{a.beat}.png"
    out.parent.mkdir(parents=True, exist_ok=True)
    sheet.save(out)
    print(out)


if __name__ == "__main__":
    main()
