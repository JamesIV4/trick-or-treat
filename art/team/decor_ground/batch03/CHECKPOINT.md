# decor_ground / batch03 checkpoint

Lane: ground decor (yard set dressing), matched against `art/review/concept/target.png`.
Concept crops used for reference are in `_ref/` (ghost, cat, planter, mums, bales, jacks).

## Scripts (headless Blender 5.1, run from repo root)

| Script | Assets |
| --- | --- |
| `decor_common.py` | Shared helpers. Imports yard_street/batch02 `yard_common` (Y) and pumpkins_farm/batch02 `farm_lib` (F). Adds `flower_head`, `leaf`, `decal`/`oval` (BVH-conforming face decals), `cull_inside`, `surface_points` and `finish` |
| `build_mums.py` | mums_orange, mums_purple, mums_white (`mum_cluster()` is reused by the planter) |
| `build_brick.py` | brick_planter, brick_pillar |
| `build_ghost.py` | ghost_figure |
| `build_cat.py` | black_cat_sitting |
| `build_farm_fix.py` | hay_bale, hay_bale_round, jackolantern_* (wraps the pumpkins_farm builders without changing them) |
| `go.ps1 -Script x.py [-Ids a,b] [-Preview]` | Build, gate, and run the engine preview |
| `far_check.ps1 -Ids a,b` | Shows how the night tile reads at 15 m (output in `_far/`) |

## Assets

| id | tris / budget | collision | notes |
| --- | --- | --- | --- |
| mums_orange / _purple / _white | 858 / 900 each | none | A 0.54 x 0.39 m mound. About 30 twelve-petal heads (24 tris each) are Poisson-placed crown-first on a lumpy hedge-material mound, with mixed petal tints, contrasting centres and pointed leaves round the base. Nine build passes. |
| brick_planter | 3549 / 3600 | box, walkable | 2.0 x 0.9 m. The cap top is at 0.55 m and the soil at 0.51 m. Brick walls use about 8 cm courses (world UVs x2.3). The cap is 10 bevelled stone slabs with hairline gaps. Planted with 3 mum clusters, 2 small pumpkins, leafy fillers, spill leaves over the cap and ivy at both ends. The AABB is 0.86 m tall (plants). Four passes. |
| brick_pillar | 346 / 700 | box, walkable | 0.45 x 0.45 x 1.1 m. Brick plinth and shaft, stepped stone cap, flat top at 1.10 m. `socket_top` at (0, 1.1, 0) in Godot coordinates. Ivy on the front-left corner. Four passes. |
| ghost_figure | 1180 / 1200 | cylinder | 0.9 m sheet ghost: 6 soft folds, pointed wavy hem, shadowed underside, flared arms. Ink eyes with glints, open smile, pink cheeks. Round wooden stand and dowel. Vinyl material. Three passes. |
| black_cat_sitting | 757 / 800 | none | 0.39 m to the ear tips. Chubby pear body, big head with cheek fluff, pink inner ears, glow_firefly eyes with ink slit pupils and glints, and a question-mark tail on the cat's right. Hidden faces are culled. Five passes. |
| hay_bale (rebuilt) | 774 / 800 | box, walkable | Same body and size. The bold rust bands are now thin wicker twine, and a straw-spike fringe runs along every edge. The fringe stays below 0.562 m so the walk height barely moves. Four passes. |
| hay_bale_round (rebuilt) | 888 / 1000 | unchanged | Only change: the twine is wicker instead of rust. |
| jackolantern_* x5 (rebuilt) | unchanged | unchanged | Carved pocket floors are now glow_jack_hot with glow_jack walls (toothy was already hot). Faces read yellow-hot like the concept. Geometry and sockets are unchanged. One pass plus a check of all five previews. |

## Weak / open

- Mums: the heads are faceted 6-point stars. They read as phlox or daisy blooms more than true pompom mums. The base leaves are spiky triangles, and the lower front flank shows a dark green band.
- Planter: the plants stand in a row. It is lush but less overgrown than the concept's beds (no big vine leaves). The leafy filler lumps read as dark blobs from behind.
- **Walkability**: box collision uses the mesh AABB, so on brick_planter the player would stand at about 0.86 m on top of the plants, not on the 0.55 m cap. Root needs a collision-height override (`height` is only read for `trunk`), or a plant-free variant. hay_bale's fringe widens its AABB to 1.29 x 0.76 m (body 1.1 x 0.6 m).
- Cat: glow bloom swallows the pupils at night. The legs are 5-sided.
- Ghost: the vinyl texture seam was moved down near the hem, but a faint line remains.
- Jacks: only the glow cells changed. The far-eye at 3/4 views is the same as in batch02.
