"""Add / update this lane's batch04 costume entries in art/asset_catalog.json (read-modify-write in one
go; other workers edit the catalog concurrently). Refuses to write if a plain json round-trip of the
current file would change anything outside these entries.
Run: python art/team/characters_candy/batch04/catalog_update.py
"""
import json
from pathlib import Path

ROOT = Path(__file__).resolve().parents[4]
P = ROOT / "art" / "asset_catalog.json"

RIG = ("Rig contract identical to dragon_kid (the game animates every costume with the same code): every part is "
       "its own object with its origin at its joint (hips for legs, shoulders for arms, neck for head, pelvis for "
       "body) at exactly dragon_kid's joint positions, parented with identity rotation/scale to an empty root at "
       "the feet; front = glTF +Z. bucket (the same glowing jack-o'-lantern candy bucket as dragon_kid, rope "
       "handle, origin at the grip) is a child of arm_r; bucket_candy (candy heap, origin on the bucket floor, "
       "scaled 0..1 in Y by the game) and socket_bucket_light are children of bucket. The kid's own face (big "
       "ink eyes, pink cheeks, smile) and bare skin hands show in every costume.")
BASE = ["body", "head", "leg_l", "leg_r", "arm_l", "arm_r", "bucket", "bucket_candy"]


def entry(aid, extra_parts, brief):
    return {
        "id": aid,
        "path": f"characters/{aid}",
        "lane": "characters_candy",
        "tris": 10000,
        "collision": "none",
        "parts": BASE[:2] + extra_parts + BASE[2:],
        "sockets": ["bucket_light"],
        "brief": brief + " " + RIG,
    }


NEW = [
    entry("kid_ghost", ["wing_l", "wing_r"],
          "Unlockable costume: the dragon kid's same 1.1 m kid as a cute bedsheet ghost. A soft white sheet hood "
          "with a tall round dome and two big black oval eye holes; the kid's face peeks out of the ghost's round "
          "'boo' mouth (sheet rim with lavender stitches, the kid makes an 'oh' mouth). The sheet falls to a "
          "scalloped hem with loose folds, lifted at the sides by the arms; white bell sleeves, lavender leggings "
          "and purple sneakers peek out below. wing_l / wing_r are loose sheet flaps hanging behind the arms (the "
          "game flaps and spreads them like ghost arms). No tail (a bedsheet has none; the game does a wand spin)."),
    entry("kid_cat", ["tail"],
          "Unlockable costume: the same kid in a plush black-cat onesie (blue-black felt so it still reads at "
          "night). Hood with tall pointed ears with pink insides and big yellow-green cat eyes with slit pupils on "
          "the brow; cat face paint on the kid (pink nose, ink whiskers). Grey belly patch with pink stitches, an "
          "orange felt collar with a gold bell, grey sleeve cuffs, round paw boots with three toe beans. tail is a "
          "long curling cat tail (swoops low, rises into a hook) with an orange bow near the base; the game sways it."),
    entry("kid_witch", ["wing_l", "wing_r"],
          "Unlockable costume: the same kid as a little witch. The kid's own brown hair with pigtails tied in "
          "green, under a big floppy pointed hat (dark plum felt, wavy brim, a crooked bent tip, green band with a "
          "gold buckle, a small yellow moon patch). Purple felt dress with a gently flared skirt and a pointy "
          "green-trimmed hem, dark belt with a gold buckle, green pointed collar, purple bell sleeves with green "
          "cuffs, orange-and-black striped stockings and pointy curled boots with gold buckles. wing_l / wing_r are "
          "the two halves of a dark plum cape with a bright orange lining, hanging from the shoulders down the "
          "back (the game spreads them on double jumps and glides)."),
    entry("kid_skeleton", [],
          "Unlockable costume: the same kid in a black skeleton onesie with a puffy bone-white print: sternum and "
          "four pairs of ribs on the chest, pelvis on the hips, spine and three pairs of ribs on the back (the "
          "gameplay camera mostly sees the back), bones down the outside of each sleeve, a thigh bone on the front "
          "and back of each leg and toe bones on the boots. Smooth bone-white skull hood whose face opening sits "
          "lower than the dragon's: it is the skull's dark mouth rimmed with little white teeth, framing the kid's "
          "eyes, cheeks and smile, so the skull's big black eye sockets and heart nose sit on a forward-facing brow "
          "above it (sockets on the crown read as a panda). No wings or tail."),
    entry("kid_pumpkin", ["wing_l", "wing_r"],
          "Unlockable costume: the same kid in a round ribbed pumpkin suit (pumpkin material, dimpled top) with a "
          "stitched black felt jack-o'-lantern face on the tummy and a leafy green collar. Green sleeves and "
          "tights, brown boots. On the kid's own brown hair, a green felt beanie with a curly brown stem, two "
          "pumpkin leaves and a curly tendril on top. wing_l / wing_r are two big pumpkin leaves on the back "
          "(the game flaps and spreads them on double jumps and glides)."),
]

text = P.read_text(encoding="utf-8")
data = json.loads(text)
indent = 1 if text.startswith("{\n \"") else 2
tail = "\n" if text.endswith("\n") else ""
if json.dumps(data, indent=indent, ensure_ascii=False) + tail != text:
    raise SystemExit("catalog does not round-trip byte-identically; refusing to rewrite it")
assets = data["assets"]
ids = {a["id"]: a for a in assets}
insert_at = next(i for i, a in enumerate(assets) if a["id"] == "dragon_kid") + 1
for e in NEW:
    if e["id"] in ids:
        ids[e["id"]].clear()
        ids[e["id"]].update(e)
    else:
        assets.insert(insert_at, e)
        insert_at += 1
P.write_text(json.dumps(data, indent=indent, ensure_ascii=False) + tail, encoding="utf-8")
print("catalog updated:", [e["id"] for e in NEW])
