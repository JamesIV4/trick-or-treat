"""Composes the tiles of a pose sheet (written by mon_kit2.Rig.pose_sheet) into one labelled PNG.
Runs under the system Python (PIL); Blender's Python has no PIL.

    python sheet_compose.py <manifest.json>
"""
import json
import sys
from pathlib import Path

from PIL import Image, ImageDraw, ImageFont


def main(manifest):
    m = json.loads(Path(manifest).read_text())
    tiles = m["tiles"]
    with Image.open(tiles[0]["file"]) as first:
        w, h = first.size
    sheet = Image.new("RGB", (m["cols"] * w, m["rows"] * h), (20, 22, 34))
    try:
        font = ImageFont.truetype("arial.ttf", 20)
    except OSError:
        font = ImageFont.load_default()
    for t in tiles:
        with Image.open(t["file"]) as src:
            sheet.paste(src.convert("RGB"), (t["col"] * w, t["row"] * h))
        if t["label"]:
            d = ImageDraw.Draw(sheet)
            d.rectangle((t["col"] * w, t["row"] * h, t["col"] * w + 12 + 11 * len(t["label"]), t["row"] * h + 28), fill=(10, 10, 16))
            d.text((t["col"] * w + 6, t["row"] * h + 3), t["label"], fill=(255, 235, 150), font=font)
    sheet.save(m["out"])
    for t in tiles:
        Path(t["file"]).unlink(missing_ok=True)
    Path(manifest).unlink(missing_ok=True)
    print(f"pose sheet -> {m['out']}")


if __name__ == "__main__":
    main(sys.argv[1])
