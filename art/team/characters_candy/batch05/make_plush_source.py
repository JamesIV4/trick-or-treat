"""Procedural source painting for the `plush` material layer (short-pile velour for the costumes):
art/materials/source/plush.png. Fine fuzzy fibres with a faint nap direction and very soft mottling, so
a plush onesie reads as soft fabric up close and as a clean colour at gameplay distance (the `felt`
layer mottles large shapes and reads as crumpled foil on the dragon).
Run: python art/team/characters_candy/batch05/make_plush_source.py  then  python art/tools/make_materials.py
"""
from pathlib import Path

import numpy as np
from PIL import Image

ROOT = Path(__file__).resolve().parents[4]
OUT = ROOT / "art" / "materials" / "source" / "plush.png"
S = 512
rng = np.random.default_rng(505)


def value_noise(cells):
    g = rng.random((cells, cells)).astype(np.float32)
    c = np.arange(S) * cells / S
    i0 = np.floor(c).astype(int) % cells
    i1 = (i0 + 1) % cells
    f = c - np.floor(c)
    f = f * f * (3 - 2 * f)
    a = g[i0][:, i0] * (1 - f)[None, :] + g[i0][:, i1] * f[None, :]
    b = g[i1][:, i0] * (1 - f)[None, :] + g[i1][:, i1] * f[None, :]
    return a * (1 - f)[:, None] + b * f[:, None]


def blur(a, ry, rx):
    out = a.copy()
    for axis, r in ((0, ry), (1, rx)):
        if r:
            acc = np.zeros_like(out)
            for d in range(-r, r + 1):
                acc += np.roll(out, d, axis=axis)
            out = acc / (2 * r + 1)
    return out


fibre = blur(rng.random((S, S)).astype(np.float32), 3, 0)          # short fibres with a vertical nap
fibre = (fibre - fibre.mean()) / fibre.std()
tuft = value_noise(96)                                              # tiny tufts
tuft = (tuft - tuft.mean()) / tuft.std()
soft = value_noise(8)                                               # very soft pile mottling
soft = (soft - soft.mean()) / soft.std()
img = 0.5 + 0.075 * fibre + 0.05 * tuft + 0.035 * soft
img = np.clip(img, 0, 1)
OUT.parent.mkdir(parents=True, exist_ok=True)
Image.fromarray((np.repeat(img[..., None], 3, axis=2) * 255 + 0.5).astype(np.uint8)).save(OUT)
print("wrote", OUT)
