"""Add / update this lane's 4 thorn-vine entries in art/asset_catalog.json with one read-modify-write
(other workers edit the catalog concurrently; run right after reading).
Run: python art/team/vines/batch03/catalog_update.py
"""
import json
from pathlib import Path

ROOT = Path(__file__).resolve().parents[4]
P = ROOT / "art" / "asset_catalog.json"

KNOT = ("Thorn vine bramble knot (infestation the player burns with fireballs; replaces C++ build_vine): "
        "thick dark-green bark vine stems corkscrewing up out of the ground and arching over into curled tips, "
        "roots knotting into the ground, studded with many bone-white spines (cream tips) pointing outward, "
        "a few small leaves and two small glowing purple buds. ~2.2 m across, ~2.5 m tall. "
        "Single merged mesh, origin at the ground centre. Silhouette: {sil}")

ENTRIES = [
    ("thorn_knot_a", 3500, KNOT.format(sil="corkscrew spire - four stems twisted into one rope trunk that "
                                           "splits into crozier hooks at staggered heights.")),
    ("thorn_knot_b", 3500, KNOT.format(sil="bramble dome - three arches diving back into the ground, crossing "
                                           "over a central curled spire.")),
    ("thorn_knot_c", 3500, KNOT.format(sil="shepherd's crook - one fat stem leaning up into a big top curl, "
                                           "companion stems wound round it curling the other way.")),
]
ROOT_MASS = ("thorn_root_mass", 9000,
             "Thorn nest (replaces the C++ ThornVine heart: the big knot + purple blob): a huge gnarled root mass "
             "~4.2 m across x ~3.6 m tall. Twisted rope-like bark roots rise from the ground and coil into a "
             "cage/crown of outward fiddlehead curls over a root cradle, big bone-white spines everywhere, "
             "smaller creeping vines spill out over the ground. The cage opens at the front (glTF +Z) on a "
             "glowing heart. Empty root 'thorn_root_mass' (origin at the ground centre) with two mesh children: "
             "'roots' (origin = root; everything except the heart) and 'heart' (glow_purple seed-heart ~1.3 m "
             "across, cupped by dark veined plum husk petals; its origin is the heart centre at (0, 1.5, 0) game "
             "coordinates, identity rotation) so the game can pulse its scale and glow on its own.")


def entries():
    out = []
    for aid, tris, brief in ENTRIES:
        out.append({"id": aid, "path": f"monsters/{aid}", "lane": "monsters", "tris": tris, "collision": "none",
                    "brief": brief})
    aid, tris, brief = ROOT_MASS
    out.append({"id": aid, "path": f"monsters/{aid}", "lane": "monsters", "tris": tris, "collision": "none",
                "parts": ["roots", "heart"], "brief": brief})
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
