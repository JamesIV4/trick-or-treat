"""Packs the frames rendered by make_vfx.py into the game's particle textures.

Single sprites are downsampled to 128 px; flipbooks become 4x4 atlases of 128 px cells. Colour is
kept white where the art is meant to be tinted in game, and alpha is premultiplied-safe (colour is
set to white wherever alpha is zero, so filtering never pulls in dark fringes).
Run: python art/tools/assemble_vfx.py
"""
from pathlib import Path

from PIL import Image

ROOT = Path(__file__).resolve().parents[2]
FRAMES = ROOT / "art" / "vfx" / "_frames"
OUT = ROOT / "game" / "assets" / "vfx"
CELL = 128


def clean(img):
    """Unpremultiplied edges: give fully transparent pixels the nearest visible colour (white)."""
    img = img.convert("RGBA")
    px = img.load()
    for y in range(img.height):
        for x in range(img.width):
            r, g, b, a = px[x, y]
            if a == 0:
                px[x, y] = (255, 255, 255, 0)
    return img


def main():
    OUT.mkdir(parents=True, exist_ok=True)
    for name in ("sparkle", "glow", "ring"):
        img = clean(Image.open(FRAMES / f"{name}.png")).resize((CELL, CELL), Image.LANCZOS)
        img.save(OUT / f"{name}.png")
        print("wrote", name)
    for name in ("smoke", "flame", "swirl"):
        sheet = Image.new("RGBA", (CELL * 4, CELL * 4), (255, 255, 255, 0))
        for f in range(16):
            img = clean(Image.open(FRAMES / f"{name}_{f:02d}.png")).resize((CELL, CELL), Image.LANCZOS)
            sheet.paste(img, ((f % 4) * CELL, (f // 4) * CELL))
        sheet.save(OUT / f"{name}_4x4.png")
        print("wrote", name, "sheet")


if __name__ == "__main__":
    main()
