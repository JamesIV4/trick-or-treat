# characters_candy / batch02 checkpoint (v2: polished and textured)

## Built (all 8 lane assets, all gates PASS)

| Asset | Tris / budget | Script |
| --- | --- | --- |
| dragon_kid | 8930 / 9000 | build_dragon_kid.py |
| candy_wrapped_pink / mint / grape / lemon | 396 / 400 each | build_candy.py |
| candy_chocolate_bar | 330 / 400 | build_candy.py |
| candy_lollipop | 488 / 500 | build_candy.py |
| candy_golden_bar | 472 / 500 | build_candy.py |

The catalog has 7 candy entries, not 8. The four wrapped colours share one mesh.

Rebuild commands:
- `blender --background --factory-startup --python art/team/characters_candy/batch02/build_dragon_kid.py [-- --nosheet] [--pieces]`
- `... build_candy.py [-- <ids>] [--nosheet] [--lift]`

`--pieces` prints the triangle count of each sub-piece. `--lift` is for review only: it exports candy standing on the ground. Always re-export without it afterwards, as I did.

Helpers live in `dk_kit.py`, in this folder, not in art/tools:
- `mesh()`
- `lathe()`, with poles, warp and cut
- `sweep()`: tubes with radius per point, parallel-transport frames, a profile warp and per-face column paint
- `stitches()`: tent-shaped dashes, 6 tris each
- `bvh()` / `hit()` / `ring_on()`: surface ray casting
- `look()` / `put()`: orientation
- `part()` / `attach()`: the rig, carried over from v1

## dragon_kid rig (unchanged contract from v1)

- The root empty `dragon_kid` sits at the feet. Its children are `body` (0, 0, 0.30), `head` (0, 0, 0.64), `wing_l` / `wing_r` (±0.075, 0.135, 0.56), `tail` (0, 0.14, 0.34), `leg_l` / `leg_r` (±0.085, 0, 0.30) and `arm_l` / `arm_r` (±0.165, 0, 0.585), in Blender coordinates.
- `bucket` is a child of `arm_r`, with its origin at the handle grip. `bucket_candy` is a child of `bucket`, with its origin on the candy floor.
- `socket_bucket_light` is a child of `bucket` at the pumpkin centre.
- I checked the glTF: every node carries a translation only. Character right is Blender -X, and the front is -Y.
- Change from v1: the candy floor (the `bucket_candy` origin) now sits 1.6 cm under the rim instead of near the bucket centre. Everything below it is the glowing lantern chamber that you see through the carved face. The game scales the heap in Y from its origin, so the empty-to-full behaviour is the same, but the heap is now a mound above the rim. That reads better from the high camera.

## What was done to reach the v2 bar

- **Forms.** Smooth lathes and ellipsoids everywhere: a pear torso, a 28x15 hood, puffy torus cuffs, boots with separate soles, mittens with thumbs, a swept tail and arms. Hard surfaces are bevelled.
- **Seams and stitching.** The belly patch is ray-cast onto the tummy and pillowed with a soft edge. Its stitch ring is dark teal. The hood rim is a closed sweep conforming to the hood ellipsoid, with stitches along its crest.
- **Materials.**
  - `felt` on all cloth, `felt_teal` for the main onesie, light and dark teal for the accents.
  - `pumpkin` on the bucket shell (cylindrical projection) and `flat` on the small accents: eyes, face, soles, claws, rope, candy.
  - `metal` on candy foils, `paper` on the chocolate sleeve and lollipop stick.
  - In game, candy uses `candy.gdshader`, which ignores the material layer. Only the palette colours, the gloss and the self-glow show there, so candy relies on silhouette and colour pattern.
- **Bucket face.** It is a real cut: an exact boolean of rounded-triangle eyes, a nose and a two-tooth grin through a 13 mm thick lobed shell. Afterwards I dissolve slivers and classify every face as outer, wall, inner or lip, then repaint it.
- **Previews.** Each asset was checked in engine over 2 to 4 passes. Changes to the dragon from those passes:
  - The mouth changed from a D shape that read as a pout to an upturned crescent.
  - The highlights got bigger, the face opening got bigger, and the horns got chunkier.
  - The onesie colour went from `dragon_teal` to brighter `felt_teal` for night readability.
  - The tail belly stripe is now painted by sweep column, which fixed jagged per-face paint.
  - The fully glowing interior washed out the candy, so it became a glowing chamber under a dark candy floor.
  - The candy heap was spread into a ring so it reads full.
  - I downscaled the preview to about 80 px tall to simulate the gameplay camera. The face (eyes, cheeks, smile), the bucket glow and the candy, and the back silhouette (horns, wings, spikes, tail) all still read at that size.

## Weak / unresolved

- **Engine preview vs floating candy.** `tools/asset_preview.ps1` puts the origin on the grass, so every candy's lower half is buried in the official `art/review/engine/<id>.png`. For review I also rendered `<id>_lifted.png` from a temporary lifted export; the shipped exports are centred. I could not fix this in the tool, because tools/ is off limits.
- **Felt texture.** The shared `felt` layer reads as fuzzy, slightly crumpled terrycloth more than smooth felt, especially in the studio row. The diamond quilting in the source is not visible at kid scale. It is a material-level issue, and I could not tune it from the lane.
- **Dragon budget.** Only 70 tris are left, so any addition must be paid for elsewhere. The hood (712), face (360) and eyes (196 each) are where the budget went. The silhouettes of the cuffs (10 and 12 segments), the rope (6 sides) and the candy heap pieces (7x5) are the first to look polygonal up close.
- **Dragon back.** Seen from behind, the back is plain teal apart from the spikes and wings, and it reads quite dark under the blue night light. The game's rim light (rim_strength 0.22, set by the controller) should help, but the preview does not show it.
- **Dragon arms.** The arms hang in a neutral rest pose, as in v1. The fang and tongue are tiny and only read close up.
- **Wrapped candy.** The fans are crinkled with spiky pleats. They read as wrapper twists in engine, but up close they look a little like crowns. Mint's lolly_white stripe has the least contrast of the four.
- **candy_lollipop.** The 18-sided disc shows slight polygon facets on the rim at close range. The swirl is a 6-arm pinwheel, not a single continuous spiral.
- **candy_golden_bar.** The glow band blooms heavily in the toon preview, but the red medallion and star survive it. In game the candy shader adds self-glow and rim, so the bar is the brightest candy by design.
- **candy_chocolate_bar.** From a low side angle the peeled foil corner can read as a small white fin.
