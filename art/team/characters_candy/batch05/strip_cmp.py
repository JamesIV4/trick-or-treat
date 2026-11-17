"""Frame strips of the SAME clip frames from several showcase recordings, one row block per recording, every frame
labelled (A = the after clip, then W1, W2 ... for the others). The recorder is deterministic: frame n is the same
beat moment in every clip.
  python strip_cmp.py --id kid_ghost --frames 812-835 --out <png> [--step 1] [--cols 12] [--width 240]
      [--crop x0,y0,x1,y1 (fractions)] <clip dir> [<clip dir> ...]
"""
import argparse
import subprocess
import tempfile
from pathlib import Path

from PIL import Image, ImageDraw


def grab(mp4, frames, tmp, tag):
    sel = "+".join(f"eq(n\\,{f})" for f in frames)
    pat = str(Path(tmp) / f"{tag}_%04d.png")
    subprocess.run(["ffmpeg", "-hide_banner", "-loglevel", "error", "-y", "-i", str(mp4), "-vf", f"select='{sel}'",
                    "-vsync", "0", pat], check=True)
    return [Image.open(Path(tmp) / f"{tag}_{i + 1:04d}.png").convert("RGB") for i in range(len(frames))]


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--id", required=True)
    ap.add_argument("--frames", required=True)
    ap.add_argument("--step", type=int, default=1)
    ap.add_argument("--cols", type=int, default=12)
    ap.add_argument("--width", type=int, default=240)
    ap.add_argument("--crop", default="0,0,1,1")
    ap.add_argument("--out", required=True)
    ap.add_argument("--labels", default="")
    ap.add_argument("dirs", nargs="+")
    a = ap.parse_args()
    f0, f1 = (int(x) for x in a.frames.split("-"))
    frames = list(range(f0, f1 + 1, a.step))
    c = [float(x) for x in a.crop.split(",")]
    labels = a.labels.split(",") if a.labels else ["A"] + [f"W{i}" for i in range(1, len(a.dirs))]
    blocks = []
    with tempfile.TemporaryDirectory() as tmp:
        for di, d in enumerate(a.dirs):
            imgs = grab(Path(d) / f"{a.id}.mp4", frames, tmp, f"d{di}")
            tiles = []
            for f, im in zip(frames, imgs):
                w, h = im.size
                im = im.crop((int(c[0] * w), int(c[1] * h), int(c[2] * w), int(c[3] * h)))
                im = im.resize((a.width, int(im.size[1] * a.width / im.size[0])), Image.LANCZOS)
                dr = ImageDraw.Draw(im)
                txt = f"{labels[di]}{f}"
                dr.rectangle((0, 0, 9 * len(txt) + 6, 16), fill=(0, 0, 0))
                dr.text((3, 2), txt, fill=(255, 230, 120))
                tiles.append(im)
            blocks.append(tiles)
    tw, th = blocks[0][0].size
    cols = min(a.cols, len(frames))
    rows_per = -(-len(frames) // cols)
    sheet = Image.new("RGB", (cols * tw, rows_per * th * len(blocks) + 6 * (len(blocks) - 1)), (40, 40, 40))
    for bi, tiles in enumerate(blocks):
        for i, im in enumerate(tiles):
            sheet.paste(im, ((i % cols) * tw, bi * (rows_per * th + 6) + (i // cols) * th))
    Path(a.out).parent.mkdir(parents=True, exist_ok=True)
    sheet.save(a.out)
    print(a.out)


if __name__ == "__main__":
    main()
