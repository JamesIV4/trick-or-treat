"""Before / after review images for one costume (P10, written for kid_pumpkin; works for any id):
  python art/team/characters_candy/batch05/compare_sheets.py kid_pumpkin [mock_name]
writes art/review/showcase/wip_p/<id>_beats_before_after.png (BEFORE beat sheet over NOW) and
<id>_before_after_mock.png (BEFORE front / NOW front / the mock crop, then BEFORE face / NOW face / NOW back).
"""
import sys
from pathlib import Path

from PIL import Image, ImageDraw

ROOT = Path(__file__).resolve().parents[4]
SHOW = ROOT / "art" / "review" / "showcase"
REF = ROOT / "art" / "team" / "characters_candy" / "batch04" / "ref"


def fit(im, h):
    return im.resize((max(1, round(im.width * h / im.height)), h), Image.LANCZOS)


def label(im, text):
    d = ImageDraw.Draw(im)
    d.rectangle((0, 0, 9 * len(text) + 16, 26), fill=(20, 18, 28))
    d.text((8, 6), text, fill=(255, 230, 160))
    return im


def row(images, h):
    ims = [fit(i, h) for i in images]
    out = Image.new("RGB", (sum(i.width for i in ims) + 8 * (len(ims) - 1), h), (20, 18, 28))
    x = 0
    for i in ims:
        out.paste(i, (x, 0))
        x += i.width + 8
    return out


def main():
    aid = sys.argv[1]
    mock = sys.argv[2] if len(sys.argv) > 2 else aid.replace("kid_", "")
    before, now = SHOW / "before", SHOW / "wip_p"
    b = Image.open(before / f"{aid}_beats.png").convert("RGB")
    n = Image.open(now / f"{aid}_beats.png").convert("RGB")
    w = max(b.width, n.width)
    out = Image.new("RGB", (w, b.height + n.height + 40), (20, 18, 28))
    out.paste(label(b.copy(), "BEFORE"), (0, 0))
    out.paste(label(n.copy(), "NOW"), (0, b.height + 40))
    out.save(now / f"{aid}_beats_before_after.png")
    H = 720
    ref = Image.open(REF / f"ref_{mock}.png").convert("RGB")
    top = row([label(Image.open(before / f"{aid}_front.png").convert("RGB"), "BEFORE front"),
               label(Image.open(now / f"{aid}_front.png").convert("RGB"), "NOW front"), label(ref, "mock")], H)
    bot = row([label(Image.open(before / f"{aid}_face.png").convert("RGB"), "BEFORE face"),
               label(Image.open(now / f"{aid}_face.png").convert("RGB"), "NOW face"),
               label(Image.open(now / f"{aid}_back.png").convert("RGB"), "NOW back")], H)
    w = max(top.width, bot.width)
    out = Image.new("RGB", (w, 2 * H + 8), (20, 18, 28))
    out.paste(top, (0, 0))
    out.paste(bot, (0, H + 8))
    out.save(now / f"{aid}_before_after_mock.png")
    print("wrote", now / f"{aid}_beats_before_after.png", now / f"{aid}_before_after_mock.png")


main()
