"""Add / update this lane's 20 entries (10 monsters + 10 nests) in art/asset_catalog.json with one
read-modify-write (other workers edit the catalog concurrently; run right after reading).
Run: python art/team/monsters/batch03/catalog_update.py
"""
import json
from pathlib import Path

ROOT = Path(__file__).resolve().parents[4]
P = ROOT / "art" / "asset_catalog.json"

RIG = ("Empty root '{id}' at the ground under the monster's centre, children 'body' (origin = root){parts}. "
       "Faces glTF +Z. Parts have identity rotation; the game swings them around their origin.")

MONSTERS = [
    ("pumpkin", ["part_a", "part_b"], "Pumpkin Hopper: plump 0.9 m jack-o'-lantern imp with a carved glowing grin, curly stem and leaf, "
     "stubby root feet; part_a/part_b are vine arms with leaf hands (pivots at its sides, swing around X)."),
    ("bat", ["part_a", "part_b"], "Bat: round violet felt body (centre at the origin: it flies, so it floats), big ears, glowing amber eyes, "
     "tiny fangs; part_a/part_b are scalloped membrane wings (pivots at the shoulders, flap around Z)."),
    ("skeleton", ["part_a", "part_b"], "Rattlebones: 1.2 m chibi skeleton, big round skull with teal-glowing eye sockets, ribcage, "
     "pelvis and bony legs; part_a/part_b are arms (shoulder pivots, swing around X), the right one holding a bone to throw."),
    ("ghost", ["part_a", "part_b"], "Ghost: 1.3 m sheet ghost with a wavy hem, big glossy eyes, pink cheeks, little O mouth; "
     "part_a/part_b are nub arms (pivots at the sides, swing around X)."),
    ("gremlin", ["part_a", "part_b"], "Candy-Corn Gremlin: 0.8 m rounded candy-corn body (yellow/orange/white), big eyes, toothy grin, "
     "little horns and ears; part_a/part_b are stubby legs with shoes (hip pivots, swing around X)."),
    ("mummy", ["part_a", "part_b"], "Mummy: 1.4 m chunky bandaged mummy, one glowing eye peeking out, loose bandage ends; "
     "part_a/part_b are arms stretched forward (shoulder pivots, swing around X)."),
    ("slime", [], "Goo Slime: 0.7 m glossy green gumdrop blob with drips, a puddle skirt, big eyes and a smile. Body only."),
    ("scarecrow", ["part_a", "part_b"], "Scarecrow: 1.6 m sack-headed scarecrow on a post, glowing button eyes, stitched grin, "
     "crooked purple witch hat, flannel shirt, straw tufts; part_a/part_b are outstretched arms with straw hands (shoulder pivots, swing around X)."),
    ("spider", ["part_a", "part_b"], "Spider: plump plum-black spider with a glowing jack-o'-lantern marking on its abdomen, "
     "big glowing eyes and fangs; part_a/part_b are the left/right four striped legs (pivots at the body sides, swing around Y)."),
    ("toadstool", ["part_a", "part_b"], "Toadstool Imp: 1.0 m red-capped toadstool with white spots, cream stem body with a cute face; "
     "part_a/part_b are stubby arms (pivots at the sides, swing around X)."),
]

NESTS = [
    ("pumpkin", "Cracked Pumpkin: giant dark pumpkin split by a glowing crack, vines and baby pumpkins sprawling out. ~2.4 m wide, 1.7 m tall."),
    ("bat", "Bat Belfry: crooked little bell tower on a post, plum shingled roof, purple-glowing windows, sleeping bats under the eaves. ~3.3 m tall."),
    ("skeleton", "Crooked Graves: fresh dirt mound with three crooked mossy headstones carrying teal-glowing runes, a bony hand and bones. ~2 m wide, 1.4 m tall."),
    ("ghost", "Haunted Lantern: tall curly iron lantern post burning ghost-purple with a little ghost face in the flame. ~3.3 m tall."),
    ("gremlin", "Candy-Corn Hive: giant squat glossy candy corn in the gremlin's bands (yellow base, orange middle, white tip with its curl), "
     "cracked open at the front (+Z) into a dark burrow where glowing eyes and fangs peek out, sitting in heaps of normal candy-corn kernels. "
     "~2 m wide, 1.6 m tall; main body 0.8 m ground radius."),
    ("mummy", "Sarcophagus: upright painted sarcophagus with its gold lid ajar, warm glow inside, bandages trailing out. ~2.2 m tall."),
    ("slime", "Bubbling Cauldron: fat black cauldron on embers, glowing green goo bubbling over and dripping down. ~1.9 m wide, 1.2 m tall."),
    ("scarecrow", "Crow Post: tall crooked crossbar post with three plump glowing-eyed crows, a tattered flag and a hanging jack-o'-lantern. ~3.4 m tall."),
    ("spider", "Web Nest: dome of web strands over a dark hole with glowing purple egg sacs and a dangling spiderling. ~2.2 m wide, 1.5 m tall."),
    ("toadstool", "Fairy Ring: a toadstool cottage with a glowing door and window inside a ring of purple glowing toadstools. ~2.6 m wide, 1.2 m tall."),
]


def entries():
    out = []
    for key, parts, brief in MONSTERS:
        aid = f"monster_{key}"
        pr = "" if not parts else ", " + ", ".join(f"'{p}'" for p in parts) + " (origin at the joint)"
        e = {"id": aid, "path": f"monsters/{aid}", "lane": "monsters", "tris": 2500, "collision": "none",
             "parts": ["body"] + parts, "brief": brief + " " + RIG.format(id=aid, parts=pr)}
        if key == "bat":
            e["floating"] = True
        out.append(e)
    for key, brief in NESTS:
        aid = f"nest_{key}"
        out.append({"id": aid, "path": f"monsters/{aid}", "lane": "monsters", "tris": 3000, "collision": "none",
                    "brief": brief + " Monster spawner (destructible target). Origin at the ground centre."})
    return out


text = P.read_text(encoding="utf-8")
data = json.loads(text)
assets = data["assets"]
ids = {a["id"]: a for a in assets}
for e in entries():
    if e["id"] in ids:
        ids[e["id"]].clear()
        ids[e["id"]].update(e)
    else:
        assets.append(e)
indent = 1 if text.startswith("{\n \"") else 2
P.write_text(json.dumps(data, indent=indent, ensure_ascii=False) + ("\n" if text.endswith("\n") else ""), encoding="utf-8")
print("catalog updated:", len(entries()), "entries")
