"""Frame strip of a real_test.ps1 recording (art/review/showcase/wip_m/real/<name>.mp4), consecutive or stepped
frames, to judge motion in real play.

    python art/team/monsters/batch04/real_strip.py <name> --from F --to F [--step 1] [--cols 8] [--w 400]
           [--crop X0,Y0,X1,Y1] [--out file.png] [--label text]

Frames are movie frames (60 fps; real_trace.py prints states as m<frame>). --crop is in fractions of the frame.
Writes art/review/showcase/wip_m/real/strips/<name>_<from>_<to>.png and prints the path.
"""
import argparse
import subprocess
import tempfile
from pathlib import Path

from PIL import Image, ImageDraw

ROOT = Path(__file__).resolve().parents[4]
REAL = ROOT / "art/review/showcase/wip_m/real"


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("name")
    ap.add_argument("--from", dest="f0", type=int, required=True)
    ap.add_argument("--to", dest="f1", type=int, required=True)
    ap.add_argument("--step", type=int, default=1)
    ap.add_argument("--cols", type=int, default=8)
    ap.add_argument("--w", type=int, default=400)
    ap.add_argument("--crop", default="0,0,1,1")
    ap.add_argument("--out", default=None)
    ap.add_argument("--label", default="")
    a = ap.parse_args()
    frames = list(range(a.f0, a.f1 + 1, a.step))
    crop = [float(x) for x in a.crop.split(",")]
    with tempfile.TemporaryDirectory() as tmp:
        expr = "+".join(f"eq(n\\,{n})" for n in frames)
        subprocess.run(["ffmpeg", "-hide_banner", "-loglevel", "error", "-y", "-i", str(REAL / f"{a.name}.mp4"), "-vf",
                        f"select='{expr}'", "-vsync", "0", str(Path(tmp) / "f_%04d.png")], check=True)
        imgs = [Image.open(f).convert("RGB") for f in sorted(Path(tmp).glob("f_*.png"))]
    W, H = imgs[0].size
    box = (int(crop[0] * W), int(crop[1] * H), int(crop[2] * W), int(crop[3] * H))
    h = int(a.w * (box[3] - box[1]) / (box[2] - box[0]))
    rows = (len(imgs) + a.cols - 1) // a.cols
    out = Image.new("RGB", (a.cols * a.w, rows * h), (20, 22, 34))
    d = ImageDraw.Draw(out)
    for i, im in enumerate(imgs):
        x, y = (i % a.cols) * a.w, (i // a.cols) * h
        out.paste(im.crop(box).resize((a.w, h), Image.LANCZOS), (x, y))
        d.text((x + 4, y + h - 14), f"{a.label} m{frames[i]}", fill=(255, 235, 150))
    p = Path(a.out) if a.out else REAL / "strips" / f"{a.name}_{a.f0}_{a.f1}.png"
    p.parent.mkdir(parents=True, exist_ok=True)
    out.save(p)
    print(p, len(imgs), "frames")


if __name__ == "__main__":
    main()
