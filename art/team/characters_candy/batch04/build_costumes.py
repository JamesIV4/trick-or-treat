"""Unlockable player costumes: the same kid as dragon_kid (batch03) in five other Halloween costumes.
Everything shared with the dragon (joints, face, hands, bucket, candy heap, rig assembly) comes from
kid_kit.py, which is the dragon build script parametrised; each costume's own parts live in
costume_<name>.py (shared helpers in costume_common.py), and this file runs them. Rig contract
identical to dragon_kid (node names, parents, origins, front -Y / glTF +Z).

  2026-10-26 fidelity pass: every costume now follows "Five Kids in Halloween Costumes.png"
  (the user's concept; crops in ref/), keeping the kid's face, the candy bucket and the wand; all wear the
  kit's chunky high-top sneaker in their own colours. Only the cat has an optional part (its tail);
  wingless costumes get magic dust on double jumps and hovers and a wand spin in game.

  kid_ghost     a white bedsheet over the head to the knees, big black oval eyes high on the sheet,
                a rolled face opening, soft vertical folds and a wavy hem, black leggings, purple
                high-tops with orange laces
  kid_cat       black plush cat onesie: fluffy hood roll, big ears with pink insides, a purple bow tie
                with a gold bell, grey belly patch, black paw mitts with pink pads, star face paint,
                black high-tops with a strap and a cat face; tail = long curling tail with a purple bow
  kid_witch     big purple hat (wavy brim, flopped tip, orange satin band, yellow star), loose wavy
                shoulder-length hair, purple dress with an orange ruffle, cuffs and lapels, belt with a
                gold buckle, ribbon bow with a star clasp, a purple cape with an orange lining (part
                of the body), orange-and-purple striped stockings, purple high-tops with a star
  kid_skeleton  black hooded onesie with a big white cartoon skull on top of the hood (sockets, nose,
                teeth above the fringe), puffy bone print front and back, black gloves with finger
                bones, black-and-white high-tops
  kid_pumpkin   a big ribbed pumpkin suit with a carved jack-o'-lantern face, a collar of big veined
                leaves and curly vines, puffy orange sleeves, a pumpkin-top cap with a curling stem,
                leaves and tendrils, green-and-dark striped stockings, orange high-tops
Run: blender --background --factory-startup --python art/team/characters_candy/batch04/build_costumes.py
     [-- kid_ghost kid_cat ...] [--nosheet] [--pieces] [--pose] [--close]
"""
import json
import math
import sys
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parent))
import bmesh  # noqa: E402
from mathutils import Matrix  # noqa: E402

import kid_kit as KK  # noqa: E402,F401
from kid_kit import D, K, V, FELT  # noqa: E402,F401
import costume_ghost, costume_cat, costume_witch, costume_skeleton, costume_pumpkin  # noqa: E402




ALL = ["kid_ghost", "kid_cat", "kid_witch", "kid_skeleton", "kid_pumpkin"]
BUILDERS = dict(kid_ghost=costume_ghost.build, kid_cat=costume_cat.build, kid_witch=costume_witch.build,
                kid_skeleton=costume_skeleton.build, kid_pumpkin=costume_pumpkin.build)


def main():
    argv = sys.argv[sys.argv.index("--") + 1:] if "--" in sys.argv else []
    ids = [a for a in argv if not a.startswith("--")] or ALL
    sheet = "--nosheet" not in argv
    out = Path(__file__).resolve().parent / "build_log.json"
    log = json.loads(out.read_text()) if out.exists() else {}
    for aid in ids:
        K.reset()
        parts = BUILDERS[aid]()
        total, counts = KK.finish(aid, parts, sheet=sheet)
        log[aid] = {"tris": total, "parts": counts}
        if "--close" in argv:     # detail review: four 1024 px close views, and the shoes up close
            K.review_sheet([parts["body"].parent], f"{aid}_close", scale_figure=False, size=1024,
                           views=[(-28, 10), (150, 14), (0, 4), (-15, 32)], zoom=1.75, focus=(-0.03, 0, 0.6))
            K.review_sheet([parts["body"].parent], f"{aid}_feet", scale_figure=False, size=768,
                           views=[(-35, 18), (145, 18), (-90, 6), (0, 10)], zoom=4.0, focus=(0, -0.02, 0.09))
        if "--pose" in argv:          # last: it leaves the rig posed
            glide_sheet(aid, parts)
    out.write_text(json.dumps(log, indent=1))


def glide_sheet(aid, parts):
    """After export: pose the rig like the game does while gliding (dragon_controller.cpp: wings
    straight out to the sides, (0, -0.1, -0.45) / (0, 0.1, 0.45), legs tucked -0.35, arm_l (-0.6, 0, 0.5), arm_r
    (0, 0, -0.2); Godot YXZ Euler, Godot (X, Y, Z) = Blender (X, Z, -Y)) and render
    art/review/sheets/<id>_glide.png. Not exported."""
    def godot_euler(ob, x, y, z):
        ob.matrix_basis = (Matrix.Translation(ob.location) @ Matrix.Rotation(y, 4, "Z") @
                           Matrix.Rotation(x, 4, "X") @ Matrix.Rotation(-z, 4, "Y"))
    w = 0.75
    if "wing_l" in parts:
        godot_euler(parts["wing_l"], 0, -0.1, -0.45)
        godot_euler(parts["wing_r"], 0, 0.1, 0.45)
    for leg in ("leg_l", "leg_r"):
        godot_euler(parts[leg], -0.35, 0, 0)
    godot_euler(parts["arm_l"], -0.6, 0, 0.5)
    godot_euler(parts["arm_r"], 0, 0, -0.2)
    if "tail" in parts:
        godot_euler(parts["tail"], 0.25, 0.3, 0)
    root = parts["body"].parent
    K.review_sheet([root], f"{aid}_glide")


main()
