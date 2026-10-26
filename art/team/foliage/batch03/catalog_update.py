"""Registers / updates the foliage batch03 entries in art/asset_catalog.json (read-modify-write in one go,
because other workers edit the same file). Run: python art/team/foliage/batch03/catalog_update.py"""
import json
from pathlib import Path

ROOT = Path(__file__).resolve().parents[4]
PATH = ROOT / "art" / "asset_catalog.json"

UPDATE = {
    "bush_round": {"tris": 2000, "brief": "Round leafy shrub about 1.1 m wide, 0.85 m tall: dark core lumps covered by ~380 small pointed leaf cards (14 cm) in hedge_dark / hedge / vine with moss tips on top, a sprinkle of tiny cream flowers."},
    "tree_maple_red": {"tris": 10000, "brief": "Autumn maple about 5.6 m tall: dark curvy bark trunk with root flare forking into limbs hidden in a full, rounded crown (batch02 v2 crown form: ~20 lobes bulging out and down to ~3 m) skinned with ~725 small serrated maple leaf cards over a dark inner shell of larger cards and a shadow filler core; maroon / crimson / red with scarlet tips. socket_branch on a low strong branch (about 2.6 m up, offset 1 m) for swings and lantern strings."},
    "tree_maple_orange": {"tris": 10000, "brief": "Variant: about 5.2 m, wide full round crown of maple leaf cards in umber / rust / burnt amber / orange with tangerine tips."},
    "tree_maple_gold": {"tris": 10000, "brief": "Variant: about 6 m, taller oval full crown of maple leaf cards in ochre / burnt amber / amber with golden tips and a few orange and rust accents."},
}
NEW = [
    {"id": "tree_pine_a", "path": "props/tree_pine_a", "lane": "foliage", "tris": 3000, "collision": "trunk",
     "radius": 0.3, "height": 3.0,
     "brief": "Woods conifer about 7.5 m tall: short visible dark trunk, soft drooping layered tiers with jagged needle-clump fringes, deep pine greens with lighter tips."},
    {"id": "tree_pine_b", "path": "props/tree_pine_b", "lane": "foliage", "tris": 3000, "collision": "trunk",
     "radius": 0.32, "height": 3.0,
     "brief": "Variant: taller (about 9.5 m), slimmer conifer with more tiers, slightly leaning tip."},
    {"id": "leaf_scatter", "path": "props/leaf_scatter", "lane": "foliage", "tris": 400, "collision": "none",
     "brief": "About 1.25 x 1.25 m patch of 6 big flat maple leaves (22-32 cm) spread loosely on the ground with slight curl, mixed crimson / orange / gold / rust. Scatter at random yaw over streets, sidewalks and lawns."},
    {"id": "leaf_single", "path": "props/leaf_single", "lane": "foliage", "tris": 40, "collision": "none",
     "brief": "One 14 cm maple leaf with a slight curl, origin at its centre, for scattering with random rotation."},
]


def main():
    data = json.loads(PATH.read_text(encoding="utf-8"))
    ids = {a["id"]: a for a in data["assets"]}
    for aid, upd in UPDATE.items():
        ids[aid].update(upd)
    for a in NEW:
        if a["id"] in ids:
            ids[a["id"]].update(a)
        else:
            data["assets"].append(a)
    PATH.write_text(json.dumps(data, indent=1, ensure_ascii=False) + "\n", encoding="utf-8")
    print("catalog updated")


if __name__ == "__main__":
    main()
