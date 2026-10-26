# candy_variety batch03 - checkpoint

83 new floating candy pickups (`candy_<name>`, exports in `game/assets/models/candy/`), registered in
`art/asset_catalog.json` with lane `candy_variety`, `floating: true`, `candy_tier`, `glow`, `brief`.
The original nine candies (characters_candy lane) were not touched.

| Tier | Count | Budget | Size (longest side) |
| --- | --- | --- | --- |
| 0 everyday | 63 | 500 tris | 0.24-0.34 m |
| 1 chocolate value | 10 | 500 tris | 0.36-0.44 m |
| 2 lollipop value | 7 | 700 tris | 0.52-0.60 m |
| 3 golden | 4 | 700 tris | 0.50-0.52 m |

Lead follow-up: `candy_golden_twist` (tier 3, glow #ffd040, 604 tris): the house twist-wrapped sweet made big
(0.5 m) and golden - foil_gold ball, two thin lolly_white spiral stripes (cream was invisible on gold under
the candy shader, so warm white, 2 of 18 columns each), crinkled foil_gold_dark bow-tie ends (pleats x1.35),
glow_candy_gold twist knots and a glow_candy_gold star on front and back. Engine preview:
`art/review/engine/candy_golden_twist.png`. Total now 84.

## Files

- `specs.py`: the spec table (id, tier, family, size, glow, brief, family parameters). Single source for
  the build and the catalog.
- `build_variety.py`: about 50 parametric family builders (twist wrapper with shapes/patterns/prints,
  lens discs, puffy outlines, lathes, sweeps, bars, lollipops...). Run:
  `blender --background --factory-startup --python art/team/candy_variety/batch03/build_variety.py -- [ids|tier0..3|family] [--sheet] [--notile] [--lift]`
- `cv_kit.py`: lane helpers on top of `art/tools/lowpoly_kit.py` and (read-only import)
  `characters_candy/batch02/dk_kit.py`: loft, painted mesh, puffy slab, conformed decals (with
  subdivision), twisted-column tube, finish (recentre, scale to size, triangulate + drop slivers, AO,
  export, tiles).
- `contact_sheet.py <tag>`: contact sheets in `art/review/candy_variety/` (tier0, tier123, side = turned
  90 deg, gameplay = shrunk to ~10 m size).
- `register.py`: adds/updates this batch's catalog entries (re-reads the catalog right before writing).
- `results/<id>.json`, `tiles/`: per-asset build results and Blender tiles.

Palette: added a new last row (row 15): candy_aqua, candy_cobalt, candy_lime, candy_berry, candy_peach,
candy_rose_light, licorice_red, licorice_black, peanut_tan, candy_sky_light, candy_mint_light,
foil_gold_dark (4 cells left free in that row). Grid is now full at 16 rows.

## Iterations

- v1: all 83 built; 9 over budget. Critique: conformed decals sank into curved bodies (eyeball iris,
  bat/ghost prints invisible), puffy inset self-intersected on concave outlines (heart, ghost lollipop
  broken), candy-cane stripes stair-stepped, caramel coats hid the apples, rock candy sparse, gummy worms
  thin, chocolate frog read as a brown blob, PB cups showed no wrapper.
- v2: puffy rim = outline scaled about its centroid; decals get edge subdivision; eyeball painted on
  a front-pole lathe; twisted-column tube for cane/coil/wand/fizz tube; coats moved up; chunkier rock
  candy; frog redesigned (white-chocolate eye domes, pink cheeks, smile); all in budget except frog.
- v3: prints raised 22 deg toward the 30 deg camera and enlarged (front and back), PB cups stood up so the
  orange fluted wrappers show, frog in budget, finish() triangulates and drops zero-area slivers
  (3 gate failures for degenerate triangles fixed). All 83 pass gates.
- Engine previews checked: twist_aqua_bats, gummy_bear_green, licorice_wheel, bar_bat, lolly_jack,
  golden_apple (`art/review/candy_variety/v3_engine_sample.png`).

## Weak / unresolved

- Twist family (12) shares one silhouette; distinction is colour and pattern only. Prints are on the
  front and back only, so side-on during the spin they are plain balls.
- Flat pieces (coins, peppermint, belts, candy buttons, bars, lollipop discs) go edge-on twice per spin.
- Same-shape colour variants: 3 gummy bears, 2 worms, 2 rings, 2 kisses, 2 coins, 2 rock candies,
  2 corn, 2 swirl lollipops.
- licorice_wheel is the only dark piece; in engine it reads purple-black, relying on its glow.
- golden_apple / lolly_jack bloom heavily in the studio light (glow cells); fine at night.
- Contact sheets are Workbench (palette tint x AO, no candy shader), only 6 assets seen in engine.
- The outline glow colour is catalog data only; candy_outline.gdshader still uses its 9-entry kind table
  (the lead wires gameplay).
