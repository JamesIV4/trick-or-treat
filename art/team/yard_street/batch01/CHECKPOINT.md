# yard_street / batch01 checkpoint

All 21 yard_street catalog assets are built, exported and gated: 21 of 21 pass `python art/tools/gates.py --lane yard_street`.
Each has a four-angle sheet in `art/review/sheets/`, and each sheet went through at least one revision pass.

## Scripts

| Script | Assets |
| --- | --- |
| `yard_common.py` | Shared helpers (see below) |
| `build_trees.py` | tree_maple_red, tree_maple_orange, tree_maple_gold |
| `build_bush_pot.py` | bush_round, flower_pot |
| `build_lights.py` | lamp_post, paper_lantern, porch_lantern, string_bulb |
| `build_furniture.py` | mailbox, bench, rocking_chair, swing_set |
| `build_inflatables.py` | inflatable_ghost, inflatable_cat, cauldron |
| `build_car.py` | parked_car |
| `build_sky.py` | cloud_a, cloud_b, cloud_c, bat |

Run any script with `blender --background --factory-startup --python <script> -- <asset_id> ...`. If you pass ids after `--`, only those assets are rebuilt.

## Helpers added in yard_common.py (things the kit lacked)

- `lathe()`: revolves a profile and gives each band its own palette colour. r=0 makes a clean pole, so there are no degenerate triangles. Optional uneven `angles` plus `band_cols` give glass panes with thin dark mullions, used by the lamp post and porch lantern.
- `blob()`: lumpy faceted icosphere baked to world space. It records its ellipsoid for `cull_hidden()`.
- `puff()`: a faceted dome (the part of a sphere above a cut plane) with staggered rings, built directly so no triangles are spent below the flat bottom. Used for the clouds.
- `cut_below()`: bisects a mesh, keeps one side and caps the hole. Used for flat bush bottoms, mums at soil level and the ghost's D-shaped mouth.
- `cull_hidden()`: deletes faces buried inside other blobs of the same cluster.
- `clean()`: runs inside `finish()`. It triangulates and removes zero-area slivers that bisect cuts leave (the gate had flagged 2 degenerate triangles before this).
- `paint_where_world()`, `mesh()`, `bake()`, `arc()`, `bezier()`, `finish()`, `want()`.
- `socket()` / `parent_keep()`: refresh the view layer before parenting. While I was working, kit `set_parent()` read a stale `matrix_world`, so every socket and the bat wings exported at the origin. Root has since fixed the kit. I re-exported everything against the fixed kit and checked the node translations: socket_branch (1, 2.6, -0.04 Blender), lamp socket_light at the glass, porch socket_light at the glass, bat wings at the shoulders.

## Kit note

`K.ico(subdiv=1)` is the bare 20-triangle icosahedron in Blender 5.1, not 80 triangles as the kit docstring says. You need subdiv=2 for 80 triangles. Other lanes may be budgeting from the wrong number.

## Weak or unresolved

- The workbench sheets do not show glow, so glow_* parts look tan or orange on the sheets. Light-emitting parts use glow_lamp, glow_lantern, glow_porch and glow_magic_teal cells, but none of them has been checked in game.
- parked_car: clean and readable, but it reads as a boxy compact rather than a bubbly toy car. A rounder cabin and a shorter hood would help.
- rocking_chair and bench: the frames are fairly thin for the chunky-toy style and may shimmer at 15 m.
- inflatable_cat: the all-black body has almost no internal definition. A cat_orange or cheek_pink inner ear or a belly patch would read better at night, but it is at 590/600 triangles.
- flower_pot: the mums are smooth puffs without petal detail (293/300 triangles).
- tree_maple_orange: its canopy starts about 2.7 m up, so the socket_branch tip pokes into the lowest leaf blob. Swing ropes from it will come out of the foliage edge.
- tree_maple_gold: the oval canopy is a little narrow.
- cloud_c: slightly beads-on-a-string when seen side-on. The coplanar flat caps of overlapping puffs share one colour (shadow_navy), so z-fighting there should be invisible, but nobody has checked it in game.
- paper_lantern is exactly at budget (200/200).
- The mailbox door face is the same mailbox_black as the box, so it reads only as a rim.
