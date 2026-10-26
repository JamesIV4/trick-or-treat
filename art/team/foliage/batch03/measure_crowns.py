"""Measures maple crowns (non-bark vertices) in glTF files: crown bottom, top, widths, height of the
widest band, plus the bark extent. python art/team/foliage/batch03/measure_crowns.py <gltf> ..."""
import json
import math
import struct
import sys
from pathlib import Path

COMP = {5126: ("f", 4)}
NC = {"VEC2": 2, "VEC3": 3}


def acc(g, bins, i):
    a = g["accessors"][i]
    v = g["bufferViews"][a["bufferView"]]
    n = NC[a["type"]]
    stride = v.get("byteStride", 4 * n)
    base = v.get("byteOffset", 0) + a.get("byteOffset", 0)
    d = bins[v["buffer"]]
    return [struct.unpack_from("<" + "f" * n, d, base + k * stride) for k in range(a["count"])]


def measure(path):
    path = Path(path)
    g = json.loads(path.read_text())
    bins = [(path.parent / b["uri"]).read_bytes() for b in g["buffers"]]
    crown, bark = [], []
    for m in g["meshes"]:
        for p in m["primitives"]:
            pos = acc(g, bins, p["attributes"]["POSITION"])
            t = acc(g, bins, p["attributes"]["TEXCOORD_1"])
            for (x, y, z), (u, _) in zip(pos, t):
                (bark if math.floor(u) == 11 else crown).append((x, y, z))
    ys = [c[1] for c in crown]
    lo, hi = min(ys), max(ys)
    # widest band: 0.2 m height bins, width = max x extent / z extent
    best = (0, 0, 0)
    bins_ = {}
    for x, y, z in crown:
        b = int((y - lo) / 0.2)
        e = bins_.setdefault(b, [1e9, -1e9, 1e9, -1e9])
        e[0], e[1], e[2], e[3] = min(e[0], x), max(e[1], x), min(e[2], z), max(e[3], z)
    for b, e in bins_.items():
        w = max(e[1] - e[0], e[3] - e[2])
        if w > best[0]:
            best = (w, lo + (b + 0.5) * 0.2, b)
    wx = max(c[0] for c in crown) - min(c[0] for c in crown)
    wz = max(c[2] for c in crown) - min(c[2] for c in crown)
    # crown "bottom" robust: 2nd percentile of crown heights (ignores a few stray low leaves)
    q = sorted(ys)[int(len(ys) * 0.02)]
    print(f"{path.parent.name}/{path.stem}: top {hi:.2f}  bottom(min) {lo:.2f}  bottom(p2) {q:.2f}  width x {wx:.2f} z {wz:.2f}"
          f"  widest {best[0]:.2f} at h {best[1]:.2f}  crown verts {len(crown)}")


for f in sys.argv[1:]:
    measure(f)
