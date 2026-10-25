# pumpkins_farm / batch01 checkpoint

All 21 catalog assets in the `pumpkins_farm` lane are built, exported to `game/assets/models/props/`,
have sheets in `art/review/sheets/`, and pass `python art/tools/gates.py --lane pumpkins_farm`.

## Scripts (run from repo root with headless Blender 5.1)

| Script | Assets |
| --- | --- |
| `build_pumpkins.py` | pumpkin_round, pumpkin_tall, pumpkin_small, gourd_cluster |
| `build_jacks.py` | jackolantern_happy / wink / surprised / toothy / sleepy |
| `build_giant.py` | giant_pumpkin |
| `build_farm_props.py` | hay_bale, hay_bale_round, crate, barrel, wheelbarrow |
| `build_farm_big.py` | hay_wagon, scarecrow |
| `build_plants.py` | corn_stalks, sunflower, leaf_pile, vine_patch |

Each script takes optional ids after `--` (for example `build_jacks.py -- toothy sleepy`).

## Shared helper module: `pumpkin_lib.py` (the kit was missing these)

- `pumpkin_body`: lobed pumpkin built straight in bmesh. It has narrow painted groove strips with hard
  creases, a top dimple, per-lobe lumps, twist, lean, lopsidedness, a `belly` setting for bottom-heavy
  shapes and a `flat_top` setting for walkable tops. It stores the stem point in `ob["top"]`. That ends
  up in glTF extras, which is harmless.
- `carve`: jack-o'-lantern carving with the Exact boolean. Each feature is a tapered pocket: the
  outline is projected along +Y onto the body, the walls (pumpkin_dark) slope in to an inset floor
  (glow_jack or glow_jack_hot), and the floor follows the surface curvature. So from the front a dark
  rim frames the glowing hole. Feature shapes are "grow-aware" callables (`grin`, `arc_band`,
  `convex_shape`), which keeps thin crescents and toothed mouths simple polygons at every offset.
  Miter offsets self-intersected on thin shapes and the solver then deleted the whole body. The
  function prints a WARN if a polygon self-intersects or if the boolean loses geometry.
- `curly_stem`, `tendril`, `blob_tube` (lathe along a curved spine, for gourds, sleeves and legs),
  `ribbon` (closed, two-sided leaf with a midrib crease), `slab` (box that skips hidden faces),
  `lathe` (with band and stave colouring), and `cleanup`, which triangulates and removes slivers. Beveled
  kit boxes with 2 segments produced 32 degenerate triangles on the hay bale until cleanup was run.
- `finish` exports, renders the official sheet, and also writes close-up renders without the scale
  post to `batch01/_zoom/` (`*_c0` is straight front, `*_c1` is the gameplay angle, and `*_night` is
  flat unlit colour for judging the glow cells). These are review aids only.

## Kit set_parent fix
The jacks are the only assets with parented children (`socket_light`). They were re-exported after
root's fix. The sockets are at 0.42 x height inside each pumpkin (for example 0.193 m on happy),
checked in the glTF.

## What is weak (honest list)

- **Pumpkin lobes** show flat panels under studio light (2 peaks per lobe, 6-7 rings). The silhouettes
  and grooves read well; the shading is faceted rather than soft.
- **Jacks:** these were judged in workbench, where glow_jack (#ff8a1f) is close to pumpkin (#f07b1f).
  The read in the sheets comes from the dark rims. In game the floors are emissive, so contrast should
  be much stronger, but nobody has checked this in the real toon shader at night. The sleepy and wink
  crescents are the thinnest carved features (about 5 cm) and are the most at risk at 15 m. Jack bodies use
  7 lobes, not 8, to fit the carving under 600 tris.
- **pumpkin_small:** grooves (rust on pumpkin_dark) are low contrast.
- **gourd_cluster:** the crookneck reads as a generic yellow squash.
- **hay_wagon:** the hay heap is an 80-tri ico. It is smooth-shaded but still looks a bit rocky. There was
  no budget left for a subdivided heap.
- **corn_stalks** and **leaf_pile:** leaf tips and chips are pointed. They are stylised but the most
  "spiky" things in the batch.
- **wheelbarrow:** handles and legs are thinner than the chunky toy style.
- **crate:** no bevels on the slats (the 200 tri budget). Corner posts are chamfered instead.
- **scarecrow:** 1474/1500 tris, so there is little room for additions. The hat band mostly hides inside
  the tilted crown.
- **vine_patch:** leaves are 5-point stars, not true lobed pumpkin leaves.

## Unresolved / for root

- Walkable tops: hay bales, crate, barrel lid, wagon bed, round bale and giant pumpkin are flat or
  nearly flat. The round bale spiral is 2 cm proud. The giant pumpkin stem (0.4 m) sits near the centre
  of its top, so a player landing dead centre hits the stem.
- The wagon's long axis is X (tongue toward -X), so its red side faces the -Y front. Check this
  matches level placement expectations.
- Not checked in Godot (not allowed to open it).
