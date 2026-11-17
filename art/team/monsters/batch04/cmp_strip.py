"""Same clip frames from several showcase folders, one block of rows per folder (old on top, new below).

    python cmp_strip.py <id> <frames> <out.png> [--dirs after,wip_m] [--crop x,y,w,h] [--tile 300] [--cols 8]

<frames>: comma list of a-b[:step] ranges or single frames. --dirs are folders under art/review/showcase
(or absolute paths). --crop is in pixels of the 960x1080 clip (default the whole frame). Read only on the clips.
"""
import argparse, os, subprocess
import numpy as np
from PIL import Image, ImageDraw, ImageFont

ROOT = r"S:\Repos\trick-or-treat\art\review\showcase"
W, H = 960, 1080


def parse(fs):
    out = []
    for item in fs.split(","):
        if "-" in item:
            rng, _, st = item.partition(":")
            a, b = map(int, rng.split("-"))
            out += list(range(a, b + 1, int(st) if st else 1))
        else:
            out.append(int(item))
    return out


def grab(path, frames):
    want = sorted(set(frames))
    lo, hi = want[0], want[-1]
    cmd = ["ffmpeg", "-v", "error", "-i", path, "-vf", f"select='between(n\\,{lo}\\,{hi})'", "-fps_mode", "passthrough",
           "-f", "rawvideo", "-pix_fmt", "rgb24", "-"]
    p = subprocess.Popen(cmd, stdout=subprocess.PIPE)
    out, n = {}, W * H * 3
    for f in range(lo, hi + 1):
        buf = p.stdout.read(n)
        if len(buf) < n:
            break
        if f in want:
            out[f] = np.frombuffer(buf, np.uint8).reshape(H, W, 3)
    p.stdout.close(); p.wait()
    return out


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("id"); ap.add_argument("frames"); ap.add_argument("out")
    ap.add_argument("--dirs", default="after,wip_m")
    ap.add_argument("--crop", default=f"0,0,{W},{H}")
    ap.add_argument("--tile", type=int, default=300)
    ap.add_argument("--cols", type=int, default=8)
    a = ap.parse_args()
    frames = parse(a.frames)
    x, y, w, h = map(int, a.crop.split(","))
    tw = a.tile; th = int(round(tw * h / w)); cols = min(a.cols, len(frames))
    rows = (len(frames) + cols - 1) // cols
    dirs = a.dirs.split(",")
    img = Image.new("RGB", (cols * tw, rows * len(dirs) * th + 4 * (len(dirs) - 1)), (255, 40, 40))
    d = ImageDraw.Draw(img)
    try:
        fnt = ImageFont.truetype("arialbd.ttf", 18)
    except Exception:
        fnt = ImageFont.load_default()
    for di, dname in enumerate(dirs):
        path = dname if os.path.isabs(dname) else os.path.join(ROOT, dname)
        src = grab(os.path.join(path, a.id + ".mp4"), frames)
        tag = os.path.basename(path.rstrip("\\/"))
        for i, f in enumerate(frames):
            if f not in src:
                continue
            t = Image.fromarray(src[f][y:y + h, x:x + w]).resize((tw, th), Image.LANCZOS)
            cx = (i % cols) * tw; cy = (di * rows + i // cols) * th + 4 * di
            img.paste(t, (cx, cy))
            lab = f"{tag} {f}"
            d.rectangle([cx, cy, cx + 9 * len(lab) + 6, cy + 22], fill=(0, 0, 0))
            d.text((cx + 3, cy + 1), lab, fill=(255, 230, 120) if di == 0 else (140, 220, 255), font=fnt)
    os.makedirs(os.path.dirname(os.path.abspath(a.out)), exist_ok=True)
    img.save(a.out)
    print("wrote", a.out)


main()
