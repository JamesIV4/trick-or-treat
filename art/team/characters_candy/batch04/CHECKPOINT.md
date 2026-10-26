# characters_candy / batch04 checkpoint: unlockable player costumes

## 2026-10-26 fidelity pass (current)

The user asked to match the fidelity of "Five Kids in Halloween Costumes.png" (crops in ref/) and,
asked whether it was a design target, chose to follow the image (designs, colours, accessories),
keeping the kid's face, the candy bucket and the wand. Later directions: no wings on the ghost,
witch or pumpkin (the game shows magic dust on double jumps and hovers instead); better witch hair;
no hair on top of the pumpkin cap; the cat's tail an S (not a C) that undulates smoothly.

| Asset | Tris / budget | Optional parts | Height | Engine preview |
| --- | --- | --- | --- | --- |
| kid_ghost | 11482 / 16000 | none | 1.100 m | art/review/engine/kid_ghost.png |
| kid_cat | 13614 / 16000 | tail (continuous S, swished in the toon shader) | 1.227 m | art/review/engine/kid_cat.png |
| kid_witch | 15240 / 16000 | none (the cape is part of the body) | 1.332 m (hat) | art/review/engine/kid_witch.png |
| kid_skeleton | 15456 / 16000 | none | 1.127 m | art/review/engine/kid_skeleton.png |
| kid_pumpkin | 15544 / 16000 | none | 1.213 m (stem) | art/review/engine/kid_pumpkin.png |

Budgets went from 10000 to 16000 (the player character; one on screen). Each costume's builder now
lives in its own costume_<name>.py (helpers in costume_common.py) so they can be worked on in
parallel. Shared additions in kid_kit.py: sneaker() (chunky high-top, ~1600 tris a foot, colour
dict), strip() (clean curved appliques), paint_grid() (grid-aligned paint on lathes), and
rm(hand_color=, hand_mat=) (mitts/gloves with the hand shape unchanged). Review:
uild_costumes.py -- <ids> --close --pose also renders <id>_close.png (1024 px) and
<id>_feet.png, before the glide pose.

In game (dragon_controller.cpp): costumes without wings get a magic-dust burst on the double jump and
a dust stream while hovering (no rings); the cat's tail sways at the root while the toon shader's
	ail_wave instance uniform sends a sideways wave from root to tip (set per frame on the tail's
meshes); the wand spin has no ring and the spin is 1.5x faster.

Weak / unresolved: the sneakers eat most of the budget (450-1700 tris left per costume); the
skeleton's sockets are shaped for the elevated gameplay camera and look a little squat straight on;
the pumpkin's bangs are pointed tufts; the witch's locks are slightly tube-like straight from behind;
the pumpkin's upper arms sit inside the body in the fixed arm pose.

## Earlier build (batch04 first pass, superseded)
## Built (all gates PASS, rig check PASS, catalog published)

| Asset | Tris / budget | Optional parts | Height | Engine preview |
| --- | --- | --- | --- | --- |
| kid_ghost | 8094 / 10000 | wing_l, wing_r (sheet flaps) | 1.158 m | art/review/engine/kid_ghost.png |
| kid_cat | 8912 / 10000 | tail | 1.184 m | art/review/engine/kid_cat.png |
| kid_witch | 8870 / 10000 | wing_l, wing_r (cape halves) | 1.364 m (hat) | art/review/engine/kid_witch.png |
| kid_skeleton | 9782 / 10000 | none | 1.121 m | art/review/engine/kid_skeleton.png |
| kid_pumpkin | 8200 / 10000 | wing_l, wing_r (pumpkin leaves) | 1.216 m (stem) | art/review/engine/kid_pumpkin.png |

dragon_kid for reference: 9372 tris, 1.163 m. Only the cat has a `tail` (coordinator: costumes
without one get a wand spin in game).

## Files
- `kid_kit.py`: the dragon build script (batch03) parametrised: joints, shape helpers, torso, belly patch,
  leg, arm (+ the kid's skin hands), hood (opening size + shell warp), face, fringe, bare_head, the bucket +
  candy heap (verbatim), and `finish()` (hierarchy, AO, export, sheet). Imports batch03/dk_kit.py as-is.
- `build_costumes.py`: one builder per costume on top of the kit. `-- [ids] [--nosheet] [--pose]`;
  `--pose` also renders `art/review/sheets/<id>_glide.png` with the game's glide pose (not exported).
- `catalog_update.py`: adds/updates the five entries next to dragon_kid (same lane, tris 10000, parts,
  sockets, brief). Refuses to write unless the catalog round-trips byte-identically.
- `verify_rig.py`: numeric glTF check against dragon_kid -> `rig_check.json`.
- `costume_board.py`: gameplay-distance board -> art/review/engine/costumes_distance.png.
- `build_log.json`: per-part triangle counts.

Rebuild: `blender --background --factory-startup --python art/team/characters_candy/batch04/build_costumes.py -- --pose`,
then `python art/team/characters_candy/batch04/verify_rig.py`, `python art/tools/gates.py <ids>`,
`python art/tools/publish_catalog.py`.

Engine previews were rendered from a scratch copy of game/ (outside the repo) so no `--import` ran on
the user's open project; tools/asset_preview.ps1 itself imports the real project.

## Rig contract (verify_rig.py, glTF numbers)
Same nodes, parents and local translations as dragon_kid for every costume (delta 0.0 m), all
translation-only, root at the origin, min height 0.000:
body (0, 0.30, 0), head (0, 0.64, 0), leg_l/r (+-0.085, 0.30, 0), arm_l/r (+-0.165, 0.585, 0),
bucket under arm_r (-0.095, -0.20, 0.085), bucket_candy under bucket (0, -0.1577, 0),
socket_bucket_light under bucket (0, -0.2057, 0), wing_l/r (+-0.075, 0.56, -0.135), tail (0, 0.34, -0.14).
Hand tips (farthest arm vertex from the shoulder) identical to the dragon's (L (0.116, -0.261, 0.044),
R (-0.135, -0.242, 0.121), delta 0.0000), so the wand grip on arm_l and the bucket grip line up.
Bucket and candy-heap meshes identical to the dragon's. Faces toward glTF +Z.

## Iteration log
- Ghost: p1 felt (fleece look), tiny eyes on top, short poncho, lizard-like wisp tail -> p2 flannel
  (read as pyjamas), longer bell sheet hitched over the bucket, no tail -> p3 smooth sheet, eye holes
  via BVH on the real dome -> p4 forward brow, eyes closer together and seated.
- Cat: p1 good read -> p2 bigger hood eyes, taller tail curl -> p6 bigger tail bow (reads from behind).
- Witch: p1 hat/dress read, cape lost at night -> p2/p3 strong floppy tip, wider flared cape with an
  orange hem band and lining.
- Skeleton: p1-p4 sockets on the crown read as a PANDA (round white head, black spots, black body)
  -> p5 lower face opening, sockets + heart nose on a forward brow (reads as a skull) -> p6 back ribs and
  back thigh bones so the gameplay camera (behind) reads the skeleton.
- Pumpkin: p1 leaf wings merged with the collar into a green shawl, cap leaves invisible -> p2 lighter
  leaves splayed, cap leaves raised -> p6 smaller collar, leaves higher -> p7/p8 leaves near level (the
  game's spread rotation folded upright leaves behind the head) and moved off the pumpkin's back.

## Weak / unresolved
- Previews are the preview scene (no running/flap animation in engine). The glide pose was checked with
  Blender sheets only; flap extremes (+-0.9 rad) were not rendered. Cape/flap/leaf parts hang 4+ cm
  behind the bucket in Y, so they should not hit it, but the Y-axis part of the wing pose can push a
  cape edge slightly into the back (same as the dragon's wings).
- Legs swing inside the ghost sheet and witch skirt; the boots can poke through the hem at full stride.
- The bucket handle grazes the ghost sheet / pumpkin side a little, as it grazes the dragon's tummy.
- From behind, the ghost and skeleton heads are plain white domes (the skeleton reads from the back
  print, the ghost from the bell silhouette). Witch hat makes her ~20 cm taller than the dragon.
- No game code wires the costumes up yet (only the assets + catalog entries).
