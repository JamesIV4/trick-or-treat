"""Per-beat pixel diff of two showcase recordings of the same id (deterministic recorder, same frames).
usage: python beatdiff.py <dirA> <dirB> <id>[,<id>...]   (dirs under art/review/showcase or absolute)
Per frame: mean abs RGB diff (levels) at 320x360, plus the max over 20x20 blocks (local change).
Per beat: median / p95 / max of the frame mean, max block diff, and the frames where block diff > 40."""
import sys, os, json, subprocess
import numpy as np

ROOT = r"S:\Repos\trick-or-treat\art\review\showcase"
SW, SH, K = 320, 360, 20


def load(path):
    cmd = ["ffmpeg", "-v", "error", "-i", path, "-vf", f"scale={SW}:{SH}", "-f", "rawvideo", "-pix_fmt", "rgb24", "-"]
    raw = subprocess.run(cmd, stdout=subprocess.PIPE).stdout
    return np.frombuffer(raw, np.uint8).reshape(-1, SH, SW, 3)


def main():
    da, db = [d if os.path.isabs(d) else os.path.join(ROOT, d) for d in sys.argv[1:3]]
    for cid in sys.argv[3].split(","):
        beats = json.load(open(os.path.join(da, cid + ".beats.json")))["beats"]
        beats2 = json.load(open(os.path.join(db, cid + ".beats.json")))["beats"]
        assert beats == beats2, "beat lists differ"
        A, B = load(os.path.join(da, cid + ".mp4")), load(os.path.join(db, cid + ".mp4"))
        n = min(len(A), len(B))
        bbp = os.path.join(r"S:\Repos\trick-or-treat\art\review\showcase\after\review\v4", cid, "bbox.json")
        bb = json.load(open(bbp)) if os.path.exists(bbp) else {}
        mean = np.zeros(n); blk = np.zeros(n)
        for t in range(n):
            d = np.abs(A[t].astype(np.int16) - B[t].astype(np.int16)).mean(axis=2)
            b = bb.get(str(t))
            if b:  # the character box (reviewer bbox, 960x1080 px), padded: the frame mean there
                x0, y0, x1, y1 = [int(v / 3) for v in b]
                d = d[max(0, y0 - 6):y1 + 6, max(0, x0 - 6):x1 + 6]
            mean[t] = d.mean()
            d = np.abs(A[t].astype(np.int16) - B[t].astype(np.int16)).mean(axis=2)
            blk[t] = d.reshape(SH // K, K, SW // K, K).mean(axis=(1, 3)).max()
        print(f"{cid}  frames {len(A)}/{len(B)}")
        for b in beats:
            s, e = b["start_frame"], min(b["end_frame"], n - 1)
            m, k = mean[s:e + 1], blk[s:e + 1]
            hot = [s + i for i in np.nonzero(k > 40)[0]]
            hs = f"{hot[0]}-{hot[-1]} ({len(hot)})" if hot else "-"
            print(f"  {b['name']:10s} box med {np.median(m):4.2f} p95 {np.percentile(m, 95):4.2f} max {m.max():5.2f}@{s + int(m.argmax())}"
                  f"  block max {k.max():5.1f}@{s + int(k.argmax())}  block>40: {hs}", flush=True)


main()


