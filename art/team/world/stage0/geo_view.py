"""Stage 2a: a top-down picture of a --sweep-geo dump (no game run): python geo_view.py <geo_*.json> [out.png]

Street and boundary mesh triangles in their palette colour (lowest first), lot outlines (bulb lots blue,
landmark lots gold), street props (trees green, lamps yellow, cars red), gate (cyan), cart (magenta),
spawn (white), nests (orange), thorn hearts (purple), street candy (pink dots), magic spine (violet)."""
import json
import sys

from PIL import Image, ImageDraw

src = sys.argv[1]
dst = sys.argv[2] if len(sys.argv) > 2 else src.replace(".json", ".png")
g = json.load(open(src))
x0, z0, x1, z1 = g["bounds"]
pad = 12
x0, z0, x1, z1 = x0 - pad, z0 - pad, x1 + pad, z1 + pad
S = 7.0  # px per m
W, H = int((x1 - x0) * S), int((z1 - z0) * S)
img = Image.new("RGB", (W, H), (38, 58, 34))
d = ImageDraw.Draw(img)


def P(x, z):
    return ((x - x0) * S, (z1 - z) * S)  # +z up (north)


ys = [t[7] for t in g["tris"]] or [0.0]
y_lo, y_hi = min(ys), max(ys)
for t in sorted(g["tris"], key=lambda t: t[7]):
    rgb = t[6]
    # Stage 3b: street surfaces shaded by height (darker low, lighter high) so the grading reads from above.
    k = 0.65 + 0.6 * ((t[7] - y_lo) / (y_hi - y_lo) if y_hi > y_lo + 0.05 else 0.5)
    col = tuple(min(255, int(((rgb >> s) & 255) * k)) for s in (16, 8, 0))
    d.polygon([P(t[0], t[1]), P(t[2], t[3]), P(t[4], t[5])], fill=col)
for l in g["lots"]:
    pts = [P(l[i], l[i + 1]) for i in range(0, 8, 2)]
    col = (90, 140, 255) if l[8] else ((255, 200, 60) if l[9] else (240, 240, 240))
    d.polygon(pts, outline=col)
    d.line([pts[0], pts[1]], fill=(255, 80, 80), width=2)  # frontage edge
for x, z, k in g["props"]:
    c = (60, 200, 60) if k.startswith("tree") else ((255, 240, 90) if k == "lamp_post" else ((230, 50, 50) if k.startswith("parked") else (150, 150, 150)))
    px, pz = P(x, z)
    r = 4 if k.startswith("parked") else 2.5
    d.ellipse([px - r, pz - r, px + r, pz + r], fill=c)
cols = {"gate": (0, 255, 255), "cart": (255, 0, 255), "spawn": (255, 255, 255), "nest": (255, 140, 0), "heart": (170, 60, 255), "candy": (255, 120, 200), "spine": (160, 110, 255)}
for x, z, k in g["marks"]:
    px, pz = P(x, z)
    r = {"candy": 1.5, "spine": 1.2}.get(k, 6)
    d.ellipse([px - r, pz - r, px + r, pz + r], fill=cols[k])
img.save(dst)
print(dst, W, H)
