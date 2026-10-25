# Material textures checkpoint (2026-10-24)

Every layer in `materials.json` except `flat` (no texture) and `siding` (already done) now has a painted
source at `art/materials/source/<name>.png`, each with a `.provenance.json` sidecar. All were made with
`art/tools/codex_imagegen.ps1` using the `style` string plus the layer `subject`, except where a retry
prompt is noted below. `python art/tools/make_materials.py` was run afterwards: 28 layers, 27 painted,
0 placeholder. `materials.json` was not changed.

Rejected first attempts are kept in `source/rejected/`. Unmodified generator output for the two images
that were offset after generation is kept in `source/raw/`. Their final `.provenance.json` has a
`postprocess` field describing the change.

## Per layer

| Layer | Attempts | Verdict | Notes / weaknesses |
|---|---|---|---|
| shingles | 1 | Good | Exactly 7 staggered rows, chunky. The blend in make_materials leaves the seam rows slightly uneven, which is barely visible. |
| planks | 1 | Good | 5 planks, nail dots, gentle grain. |
| wood | 1 | OK | The source is very subtle (std 0.016), so contrast levelling boosts it about 8x and the grain turns streaky with dark knot "eyes". Strength 0.6 softens this in game. Busier than "very subtle". |
| brick | 1 | Good | 8 courses, running bond, light mortar. |
| stone | 1 | Excellent | Rounded fieldstones, reads well. |
| pavers | 2 | Good | v1 was a 3x3 grid. Because a half-tile offset is 1.5 cells, the make_materials seam blend turned it into a ghosted double grid. v2 prompt added "perfect 3 by 3 grid filling the tile ... half a gap along each outer edge". v2 was then offset by half a paver (np.roll 209,209) so its edges fall inside stones and no blend triggers (seam 4.16 -> 0.48). A small checker artifact at the original corner was patched by copying a neighbouring grout crossing. Tiles cleanly now. |
| concrete | 1 | OK | Faint joint across the middle, speckles. After levelling, the joint line and some speckles go quite dark and crisp. |
| asphalt | 1 | Good | Speckled grain with hairline cracks. |
| grass | 1 | Good | Painterly tufts. They are slightly side-on tuft shapes rather than strictly top-down, but read well as stylised lawn. |
| bark | 1 | Excellent | Chunky vertical plates and furrows. |
| leaves | 1 | Excellent | Clear maple shapes, value-only variation. |
| hedge | 1 | Good | Small rounded leaf clusters. Fairly busy at a 1 m repeat, but reads as foliage. |
| straw | 1 | Good | Vertical fibrous strokes. |
| pumpkin | 2 | Good | v1 had ~3.3 ribs per tile, so the seam blend ghosted the ribs. v2 prompt: "exactly 4 identical straight parallel ribs ... groove exactly at the left and right edges". It was offset half a rib (np.roll 157,0) so edges fall mid-rib and no blend triggers. There is a faint horizontal brightness step where the tile repeats vertically. |
| felt | 1 | Good | Fuzzy felt with a diamond stitch grid. The stitching is stronger than "faint" and the fibres are slightly realistic rather than painterly. |
| denim | 2 | OK | v1 had a hard horizontal seam across the middle of the image. v2 prompt added "one continuous uniform piece ... no seams, folds or lines". v2 is clean but a fine twill. It is the least chunky layer and will mostly read as diagonal hatch at distance. |
| burlap | 1 | Excellent | Chunky basket weave with stray fibres. |
| metal | 2 | Good | v1 was a very faint, mottled brushed look that levelling blew up into purple blotches. v2 prompt: "soft horizontal brushed streaks running evenly across the whole tile, gentle long light and dark brush bands". Streaky after levelling, but fine at strength 0.4. |
| paper | 1 | Good | Fibre/crinkle texture with horizontal rib lines. The crinkle is a touch realistic. |
| stucco | 1 | Good | Trowel strokes and small bumps. |
| scallop | 1 | Excellent | Exactly 7 rows of fish-scale shingles. |
| soil | 1 | Excellent | Soft clods and pebbles. |
| lattice | 1 | Excellent | Diagonal woven slats with dark gaps. High source contrast (levelling reduces it). |
| fabric | 1 | Good | Flannel plaid that tiles (2x4 repeats). The plaid is clearer than "faint". |
| vinyl | 2 | Good | v1 was a glossy tufted-cushion grid with baked specular highlights that did not tile vertically. v2 prompt: "smooth matte inflatable vinyl ... gentle soft wrinkles ... one faint straight welded seam ... no tufted cushion grid, no shiny specular highlights". Clean. |
| cloud | 1 | Good | Soft billowing puffs. The seam blend applies, but the result is invisible on clouds. |

## Notes for whoever touches this next
- `make_materials.py`'s seam check (`seam_ratio > 1.6`) fires on most structured textures, even ones that
  already tile, because flat interiors give a tiny denominator. Its half-offset blend is harmless when
  the pattern period divides half the tile (brick, shingles, scallop, 4-rib pumpkin), but it ghosts
  layouts that don't (3x3 grids, odd rib counts). The fix used here is to offset the source so its edges
  land in continuous content. The script was not changed.
- `siding` (not part of this pass): in the processed 2x2 tiling, the blend leaves short dark lip
  streaks in the middle of some boards. If that bothers anyone, offsetting the source by half a board
  horizontally (the same trick) should stop the blend firing.
- Low-contrast sources (wood, concrete, metal, stucco, asphalt) are boosted 4-8x by `TARGET_STD = 0.13`.
  That is why wood and metal look stronger in the preview than in the source.
