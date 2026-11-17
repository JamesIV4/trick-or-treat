"""Per-beat difference between two showcase recordings of the same id (the recorder is deterministic, so frame n is
the same moment in both). Prints, per beat, the mean and the largest per-frame mean absolute pixel difference
(grey, 240 x 270) and the frame of the largest. Background particles (fireflies, leaves) give a noise floor of
about 1-3; a changed pose shows as clearly more on the frames where it changes.
  python clip_diff.py <dir A> <dir B> <id> [<id> ...]
"""
import json
import subprocess
import sys
from pathlib import Path

import numpy as np

W, H = 240, 270


def frames(mp4):
    p = subprocess.Popen(["ffmpeg", "-hide_banner", "-loglevel", "error", "-i", str(mp4), "-vf", f"scale={W}:{H},format=gray",
                          "-f", "rawvideo", "-"], stdout=subprocess.PIPE)
    while True:
        b = p.stdout.read(W * H)
        if len(b) < W * H:
            break
        yield np.frombuffer(b, np.uint8).astype(np.int16)
    p.wait()


def main():
    a, b = Path(sys.argv[1]), Path(sys.argv[2])
    for cid in sys.argv[3:]:
        beats = json.loads((b / f"{cid}.beats.json").read_text())["beats"]
        d = [float(np.abs(x - y).mean()) for x, y in zip(frames(a / f"{cid}.mp4"), frames(b / f"{cid}.mp4"))]
        print(f"== {cid}: {len(d)} frames")
        for bt in beats:
            seg = d[bt["start_frame"]:bt["end_frame"] + 1]
            if seg:
                i = int(np.argmax(seg))
                print(f"  {bt['name']:18s} mean {np.mean(seg):5.2f}  max {seg[i]:5.2f} @{bt['start_frame'] + i}")


if __name__ == "__main__":
    main()
