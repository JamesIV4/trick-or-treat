# vines / batch03 checkpoint

All four thorn-vine assets are built, exported to `game/assets/models/monsters/<id>.gltf`, pass `art/tools/gates.py`, and are published to `game/assets/catalog.json`. The C++ (`build_vine`, `ThornVine::make_heart`) is not wired to them yet. That is the lead's job, because `native/` is out of this lane.

| id | tris / budget | size x * h * z (m) | parts |
| --- | --- | --- | --- |
| thorn_knot_a | 3322 / 3500 | 2.04 * 2.57 * 1.97 | one merged mesh |
| thorn_knot_b | 3395 / 3500 | 2.15 * 2.41 * 2.04 | one merged mesh |
| thorn_knot_c | 2550 / 3500 | 2.12 * 2.51 * 1.62 | one merged mesh |
| thorn_root_mass | 8903 / 9000 | 4.29 * 3.65 * 4.02 | empty `thorn_root_mass` with children `roots` (7771 tris, translation 0) and `heart` (1132 tris, translation [0, 1.5, 0], no rotation or scale; checked in the glTF) |

## Files

- `vine_kit.py` holds the lane helpers. It imports `mon_kit` (monsters/batch03) read-only. It provides:
  - paths: `smooth`, `curl` (fiddlehead spiral), `with_curl`, `wobble`
  - `stem`: a tapered bark tube. Its base is `hedge_dark` with a twisting `vine` stripe and a `pine_deep` stripe. `rope` gives a lobed cross-section.
  - `Spines`: every spine of an asset in one bmesh. A spine is a 6-sided cone (5-sided under 0.3 m) with an open buried base, a `trim_white` shaft and a `cream` tip.
  - `spines_on`: scatters spines along a stem in golden-angle steps, biased outward and upward.
  - `leaf`, `bud` (a `glow_purple` bud with three sepals), `ground_root`, `finish_knot`, `finish_parts` (the empty root plus `roots` and `heart` children; the heart's AO is baked on its own so the cage doesn't dim it)
- `build_vines.py -- [knot_a knot_b knot_c root_mass] [--nosheet]` builds the assets.
- `go.ps1 [-Keys a,b] [-NoPreview] [-Sheet]` runs build, gates and engine preview.
- `crop.py out.png ids... [--q 0,1,2,3] [--box ...]` makes 2x zoom crops of engine previews.
- `catalog_update.py` writes the four catalog entries in one read-modify-write.
- No palette changes. Cells used: hedge_dark, vine, vine_dark, pine_deep, trim_white, cream, glow_purple, spider_plum, bat_fur.

## Silhouettes

- **a, corkscrew spire:** four stems twist into a rope trunk and split into crozier hooks at staggered heights. One bud sits on a curl tip and one on a low side shoot.
- **b, bramble dome:** three low arches dive back into the ground across a central spire whose top curls over. It has small curled side tendrils.
- **c, shepherd's crook:** one fat stem leans up into a big top curl. Two companion stems wind round it and peel off the other way in curls. A low arch dives into the ground.
- **root_mass:**
  - A lobed twisted stump forms a cradle cup.
  - Seven rope-lobed roots hug the lawn, then rise and weave: neighbours twist opposite ways.
  - The roots wrap the heart and crown in outward fiddlehead curls, three of them with buds.
  - The front (glTF +Z) stays open under a lintel root.
  - Seven creeping vines spill out over the ground in curls.
  - It carries 100 spines.
- **heart:** a `glow_purple` seed with a narrowed top, cupped by five `spider_plum` bark husk petals that each carry a glowing midrib vein. Five dark veins run over the bare top to a crown nub with a tiny curled sprout.

## Review passes (engine preview, art/review/engine/<id>.png)

1. **First build:** the stems read pale and birch-like, and the spines were flat 5-sided shards. The root mass was a straight-trunk tepee, the husk was too thin to read, and the knots were over budget.
2. **Second pass:**
   - darker stem base (hedge_dark)
   - 6-sided fuller spines in trim_white rather than lolly_white, which read pink at night
   - woven twisting roots that hug the ground, with knobbly swell
   - bigger husk petals
   - wider croziers on knot a
   - lintel anchored to the front roots
3. **Third pass:**
   - tri trims (catmull density, small spines 5-sided, fewer spill spines)
   - thicker crown curl tips
   - footprint pulled in to ~4.2 m
   - knot b arches lowered
4. **Fourth pass:** crown curls angled partly tangential, so the back curl no longer reads as a bare stick edge-on from the front.

## Weak / open

- The bark material's brown furrows make the stems read a little like tree branches rather than vines up close. It is fine at gameplay distance.
- Under the warm preview porch light the white spines pick up a pink/lavender cast, and spines pointing straight at the camera show a darker tip centre.
- The knots' ground roots still read a bit like tripod legs.
- Knot b from the side is less distinctive than a and c.
- Leaves are very small and barely register. The brief asked for "a few small leaves", so this was kept.
- The heart's glow core blooms heavily in the preview. The husk petals and dark veins keep its shape readable, but the glowing midrib veins merge into the core glow.
- Root mass crown curls are a little crumpled close up.
- Game hookup: `ThornVine::make_heart` currently scales the knot by 1.7. The root mass is authored at final size, so use scale 1, and pulse only the `heart` part around its origin (0, 1.5, 0).
