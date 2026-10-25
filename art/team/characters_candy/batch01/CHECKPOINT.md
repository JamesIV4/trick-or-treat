# characters_candy / batch01 checkpoint

## Built (all 8 lane assets plus the player character, all gates PASS)

| Asset | Tris / budget | Script |
| --- | --- | --- |
| dragon_kid | 2976 / 3000 | build_dragon_kid.py |
| candy_wrapped_pink / mint / grape / lemon | 120 / 120 each | build_candy.py |
| candy_chocolate_bar | 116 / 120 | build_candy.py |
| candy_lollipop | 140 / 160 | build_candy.py |
| candy_golden_bar | 148 / 160 | build_candy.py |

Rebuild: `blender --background --factory-startup --python art/team/characters_candy/batch01/build_dragon_kid.py`
and `... --python art/team/characters_candy/batch01/build_candy.py [-- <asset ids>]` (no ids = all candy).

## dragon_kid rig (what the game can rely on)

- An empty root `dragon_kid` sits at the feet (z = 0). Its direct children are `body` (pelvis, z 0.30), `head` (neck,
  z 0.64), `wing_l` / `wing_r` (wing roots on the back), `tail` (tail base), `leg_l` / `leg_r` (hips) and `arm_l` /
  `arm_r` (shoulders, z 0.585).
- `bucket` is a child of `arm_r`, with its origin at the handle grip in the mitten. `bucket_candy` is a child of
  `bucket`, with its origin on the dark bucket floor, so scaling it from 0 to 1 grows the heap up out of the
  bucket. `socket_bucket_light` is a child of `bucket` at the bucket's centre.
- Every part has identity rotation and scale, and no parent-inverse matrix (I parent with a helper that writes clean
  local translations). The glTF nodes therefore carry only translations, which I checked in the exported file.
- The character's right is Blender -X. The front faces -Y. The head mesh has a 6 degree curious tilt baked in, so
  the rest pose stays identity.

## Helpers (in this batch folder, not in art/tools)

`cc_helpers.py` provides these helpers:
- `part()`: merges pieces and re-origins them to a joint.
- `attach()`: parents without a parent-inverse matrix. `K.set_parent` left `bucket_candy` offset by the bucket's
  world position in the export, because it chained a parent-inverse through an already-parented object.
- `slab()`: an X-Z outline extruded along Y, used for the wings.
- `aim()`
- `close_sheet()`: writes `<id>_close.png` (four close views, no scale post) and, for the kid, `dragon_kid_far.png`.
  That second sheet is a 96 px gameplay-distance check.

## Weak / unresolved

- **dragon_kid**
  - It is 24 triangles under budget. Any added detail must be paid for elsewhere. The heap candy, claws and
    highlights are already very low segment counts.
  - Seen from straight in front, the wings show only as small flaps beside the hood. From behind (the likely
    platformer camera) wings, spikes and tail all read well. If the AD wants wings in the front silhouette, make
    them larger and less swept back.
  - The face skin renders quite tan in the Workbench sheet. That is the preview's palette darkening (the kit's
    test pumpkin shows it too), not the `skin` cell.
  - Mittens and boots are `dragon_teal_dark`. They read fine against the orange bucket but are not a strong
    accent.
  - The bucket's carved face is a glow decal 4 mm proud of the shell, not a real inset. Its walls dip into the
    shell, and it has no inner face.
  - Arms hang in a neutral rest pose, so personality comes from the head tilt, asymmetric fringe and the tail curling
    to the left. The procedural animation has to supply the rest.
- **Wrapped candies:** the twisted lathe gives clean spiral stripes, but the low segment count (8) makes the body
  facets look a little crumpled up close. They sit exactly on the 120 budget. The white stripe on lemon is the
  weakest contrast of the four.
- **candy_golden_bar:** reads as a chunky gold bar with a band and a star. The flat crimp plates make it a little
  "belt buckle". The intended pinch toward the ends barely shows because the box has no interior loops. The
  `glow_candy_gold` band looks the same as `foil_gold` in Workbench; it only separates in game, where it is emissive.
- **candy_chocolate_bar:** the wrapper is plain red with no label. A label band would need geometry, and the bar has
  only 4 triangles of headroom.
- Nothing has been checked in Godot (not allowed). Workbench sheets only.
