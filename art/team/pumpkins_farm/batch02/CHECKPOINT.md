# pumpkins_farm / batch02 checkpoint (v2: polished and textured)

All 21 catalog assets in the `pumpkins_farm` lane are rebuilt with kit v2 (palette tint plus painted
material on every face, AO baked). They are exported to `game/assets/models/props/`, with sheets in
`art/review/sheets/` and in-engine previews in `art/review/engine/`.
`python art/tools/gates.py --lane pumpkins_farm`: **21/21 PASS**, with no legacy warnings and no
degenerate triangles. Every asset went through at least two in-engine preview passes. The heroes
(jacks, giant pumpkin, bales, scarecrow, round pumpkin) went through three or four.

## Scripts (run from the repo root with headless Blender 5.1; optional ids after `--`)

| Script | Assets |
| --- | --- |
| `build_pumpkins.py` | pumpkin_round, pumpkin_tall, pumpkin_small, gourd_cluster |
| `build_jacks.py` | jackolantern_happy / wink / surprised / toothy / sleepy (ids: happy wink ...) |
| `build_giant.py` | giant_pumpkin |
| `build_farm_props.py` | hay_bale, hay_bale_round, crate, barrel, wheelbarrow |
| `build_farm_big.py` | hay_wagon, scarecrow (imports bale helpers from build_farm_props) |
| `build_plants.py` | corn_stalks, sunflower, leaf_pile, vine_patch |

Review helpers (PowerShell, in this folder):
- `go.ps1 -Script build_x.py [-Ids a,b]`: builds, runs the gates, then runs the engine preview for each built id.
- `far_check.ps1 -Ids a,b`: shrinks the night tile to its on-screen size at 15 m and enlarges it for checking.
- `crop.ps1 -Id x -X -Y -W -H`: enlarged crop of an engine preview.

Output from the last two goes to `_far/`. These are review aids only.

## farm_lib.py (upgraded from batch01/pumpkin_lib.py)
- Every helper writes both v2 UV maps, UVMap (material metres) first and then Tint. The v1 helpers
  wrote palette UVs into UVMap, which would break v2.
- `pumpkin_body`: plump sqrt-sine lobes, sharp groove edges, a real stem cavity and per-lobe height
  wobble. The UVs give each geometric lobe exactly one rib of the `pumpkin` texture (4 ribs per tile,
  grooves at u = 0.125 + k/4), so the painted ribs and grooves line up with the geometry.
- `stem`: ridged star section, flared base, hook curl, bark material at a denser UV scale, and a
  caramel cut end.
- `pumpkin_leaf`, `drape`: corrugated palmate leaf. `drape` drops each vertex onto the surface below,
  so leaves hug the shoulders instead of sticking out as flat planes.
- `carve`: the Exact-boolean pocket now has a pumpkin_dark rind band (`rim_w`, about 1 cm), then a
  glowing inner wall, then a glowing floor that follows the curvature. Convex shapes shrink by
  scaling about the centroid, because miter offsets self-intersected on chamfered triangles. If the
  boolean loses geometry, carve now raises an error instead of printing a warning.
- `sweep`: parallel-transport frames, cyclic tangents for closed loops, `rib=` texture alignment,
  per-side stripes, twist (twisted rope), and flattening (straps).
- `board`: rounded-rect beams with grain along the length and optional hidden end caps. `lathe`:
  pole handling, stave UVs (planks texture, one plank per stave), and wrapped-straw UVs.
  `project_along` handles grain direction. `bale_body` is in build_farm_props.
- `finish`: adds a WeightedNormal modifier (applied on export: crisp bevels, soft rounds), bakes AO,
  exports, renders the sheet, and stamps a build id into the node extras (see below).

## Pipeline finding (for root)
**Godot re-imports a glTF only when the .gltf JSON changes.** The geometry lives in the .bin, so a
rebuild that keeps the same vertex counts and bounds (for example, a moved leaf) is not re-imported.
The engine preview then silently shows the old model. `finish()` now writes a `build` timestamp
into the node extras so every export forces a re-import. Other lanes probably hit the same problem.

## What is weak (honest)
- **Jacks at 3/4 views**: even with the face side flattened 16%, the far eye sits near the silhouette
  at the preview's 34 degree yaw. At 15 m the far eye reads as a dark rind spot more than a glow.
  Head-on and at gameplay angles the face reads well. The rind band shows in studio light but at
  night bloom mostly hides it.
- **Leaves**, especially the giant pumpkin's top leaves: they are flat-shaded corrugated fans and
  still look a bit like stiff paper stars up close. The vine patch leaves were softened and look
  better.
- **Twine/rope** on the bales: these are 3/4-sided twisted tubes. Some facets catch a pale highlight
  that reads as pinkish streaks in studio light.
- **Gourd tubes** (pear, crookneck) have 10 sides, so their silhouette edges are slightly faceted.
- **Hay wagon heap**: a smooth deformed icosphere with straw texture and tufts. It reads as hay but
  is a bit egg-like. The wheels have 12 segments and the tyre silhouette is faceted.
- **Sunflower leaves** are plain flat-tinted heart blades. They read as leaves but are the least
  "painted" surfaces in the batch.
- **Crate** at 550 tris uses chamfered (not rounded) slats to stay under 600.
- The stem cut ends on pumpkins are caramel discs. They read fine but are slightly bright from the
  side.

## Unresolved / for root
- Walkable tops are flat or nearly flat on the hay bale (about 1 cm flake bulge), round bale (2 cm
  dome with 1.4 cm spiral relief), crate (flush boards), barrel (lid recessed 4 cm below the chime),
  wagon deck (the front half is free and the back half has the hay heap) and giant pumpkin (flat
  top). The giant pumpkin stem (0.36 m) is near the centre of its top, as in v1.
- The pumpkin bodies carry a small `top` custom property (stem point) and a `build` stamp in glTF
  extras. Both are harmless.
- The wagon's long axis is X with the tongue toward -X, unchanged from v1.
