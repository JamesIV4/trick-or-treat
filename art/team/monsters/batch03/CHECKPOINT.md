# monsters / batch03 checkpoint

All 20 assets are built and exported to `game/assets/models/monsters/<id>.gltf`. All pass `art/tools/gates.py` and are published to `game/assets/catalog.json`.

## Files

- `mon_kit.py` holds the lane helpers. It imports the finished `farm_lib` (pumpkins_farm/batch02) and `dk_kit` (characters_candy/batch02) read-only. It adds:
  - shapes: `blob`, `band_lathe` + `wrap_prof` (bandages), `cone`, `bone`, `plate`, `seam`
  - faces: `eye_cute`, `eye_glow`, `smile`, `cheek`
  - rig and export: `rig` / `part`, `finish`, `nest_finish` (which clips nests at z = 0)
- `build_monsters.py [-- keys] [--nosheet] [--pieces] [--lift]` builds the monsters.
- `build_nests.py [-- keys] [--nosheet] [--pieces]` builds the nests.
- Keys for both scripts: pumpkin bat skeleton ghost gremlin mummy slime scarecrow spider toadstool.
- `go.ps1 -Script monsters|nests -Keys a,b [-Lift]` runs build, gates and engine preview. `-Lift` is for the bat: it previews a raised copy, then re-exports at the true height.
- `contact.py out.png ids... [--q 0,1] [--far 15]` makes contact sheets of engine previews. `--far` gives a rough gameplay-distance read.
- `set_palette_row.py` writes palette row 14. `catalog_update.py` writes the 20 catalog entries.

## Rig contract (verified in the exported glTF)

- Root empty `monster_<key>` sits at the origin.
- Its children are `body` (translation 0) and `part_a` / `part_b`. Each part's translation is exactly the C++ `pivot_a` / `pivot_b`, with no rotation and no scale.
- The front faces glTF +Z.
- The slime has `body` only.
- The bat's body is centred near the origin, like the C++ version: its min y is -0.22. Its catalog entry is marked `floating: true`.

## Palette row 14

bat_fur, bat_wing, bat_belly, slime_green, slime_light, slime_dark, glow_slime (emissive, flicker 0.25), bone_shade, spider_plum, crow_blue, grave_soil, sarc_blue.

## Review

- Every asset went through at least two engine-preview passes.
- The monsters were also checked with a simulated ~15 m contact sheet.

Main changes from those passes:
- ghost and mummy: dropped the `fabric` material, which read as plaid.
- skeleton: switched from `paper` (the skull looked striped) to `concrete`.
- slime: replaced the spike-like drips with base lumps.
- spider: chunkier legs.
- toadstool: tilted the cap back so the face shows from above.
- web nest: rebuilt as a twig tepee, because the dome read as a climbing frame.
- gremlin burrow: grass top. Replaced on 2026-10-25 (it read as a dirt mound) by the **Candy-Corn Hive**: a giant
  squat candy-corn `band_lathe` in the gremlin's own bands and its tip curl, with a jagged broken-shell opening cut by
  `F.carve` (white shell edge, dark burrow) and three hairline cracks. Glowing eyes and fangs float part-way into the
  burrow so the top lip never hides them, and kernel heaps sit either side of the opening. 2676 tris, 2.01 x 2.13 m,
  1.62 m tall, body ground radius 0.8 m. Gotcha: `carve` samples the hull only at outline points, so the long bottom
  edge needs in-between points or the lower middle stays uncut. A sill lower than z 0.06 brings the uncut lump back.
- sarcophagus: dark interior with a glow frame and peeking eyes.
- mummy: diagonal straps.

## Weak / open

- Glow eyes (bat, spider, skeleton teal pupils) bloom heavily in the preview, so the pupils mostly vanish. At distance the read is "glowing eyes", which is good, but the cute pupils are lost.
- mummy is still the plainest silhouette (pale column plus arms).
- nest_pumpkin: the ground vines and leaves are low-contrast against grass.
- nest_ghost: the ghost face on the lantern glass is faint under the bloom.
- monster_pumpkin's top leaf catches a pale specular from behind.
- ghost and toadstool caps show faint facets up close.
