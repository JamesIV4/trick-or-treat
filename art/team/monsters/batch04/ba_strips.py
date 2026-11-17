"""Before/after check for showcase clips: python cmp.py <before_dir> <after_dir> <out_dir> id [id...]
For each id: mean abs pixel difference at a set of frames (every beat's middle frame plus a spread
through each beat), and a side-by-side strip (before on top, after below) of 8 frames per chosen beat."""
import json
import subprocess
import sys
import tempfile
from pathlib import Path

from PIL import Image, ImageChops, ImageDraw, ImageStat


def grab(mp4, frames, tmp, tag, scale=320):
    expr = "+".join(f"eq(n\\,{n})" for n in frames)
    subprocess.run(["ffmpeg", "-hide_banner", "-loglevel", "error", "-y", "-i", str(mp4), "-vf", f"select='{expr}',scale={scale}:-1",
                    "-vsync", "0", str(Path(tmp) / f"{tag}_%04d.png")], check=True)
    files = sorted(Path(tmp).glob(f"{tag}_*.png"))
    assert len(files) == len(frames), (len(files), len(frames))
    return [Image.open(f).convert("RGB") for f in files]


def main():
    before, after, out = Path(sys.argv[1]), Path(sys.argv[2]), Path(sys.argv[3])
    out.mkdir(parents=True, exist_ok=True)
    for cid in sys.argv[4:]:
        beats = json.loads((before / f"{cid}.beats.json").read_text())["beats"]
        beats2 = json.loads((after / f"{cid}.beats.json").read_text())["beats"]
        assert beats == beats2, f"{cid}: beat lists differ"
        frames, owner = [], []
        for b in beats:
            s, e = b["start_frame"], b["end_frame"]
            for k in range(8):
                frames.append(s + (e - s) * (2 * k + 1) // 16)
                owner.append(b["name"])
        with tempfile.TemporaryDirectory() as tmp:
            A = grab(before / f"{cid}.mp4", frames, tmp, "a")
            B = grab(after / f"{cid}.mp4", frames, tmp, "b")
        print(cid)
        w, h = A[0].size
        for bi, b in enumerate(beats):
            ds = []
            row = Image.new("RGB", (8 * w, 2 * h + 4), (255, 0, 255))
            for k in range(8):
                i = bi * 8 + k
                d = ImageStat.Stat(ImageChops.difference(A[i], B[i])).mean
                ds.append(sum(d) / 3.0)
                row.paste(A[i], (k * w, 0))
                row.paste(B[i], (k * w, h + 4))
                dr = ImageDraw.Draw(row)
                dr.text((k * w + 4, 4), f"before f{frames[i]}", fill=(255, 235, 150))
                dr.text((k * w + 4, h + 8), f"after  d={ds[-1]:.1f}", fill=(150, 235, 255))
            row.save(out / f"{cid}_{b['name']}_ba.png")
            print(f"  {b['name']:10s} mean abs diff per frame: " + " ".join(f"{x:5.1f}" for x in ds))


if __name__ == "__main__":
    main()
