# foliage / batch03 checkpoint

All 8 assets are built, exported, gated (PASS), and published to `game/assets/catalog.json`. In-engine previews are in `art/review/engine/<id>.png` and Workbench sheets in `art/review/sheets/<id>.png`.

## Scripts
| Script | Assets |
| --- | --- |
| `foliage_common.py` | Leaf outlines (`maple_serrated` 14 pt, `maple_simple` 10 pt, `maple_halves` midrib-split, `_norm_outline`). `Acc` appends leaves and polys to one bmesh with per-vertex shading normals and optional fixed AO. `shade_existing()` gives filler cores a down-facing "shadow only" normal. Also AO along the shading normals, custom split normals, and export. |
| `build_maples.py` | tree_maple_red, tree_maple_orange, tree_maple_gold |
| `build_pines.py` | tree_pine_a, tree_pine_b |
| `build_leaves.py` | leaf_scatter, leaf_single |
| `build_bush.py` | bush_round |
| `measure_crowns.py` | Crown top / bottom / width / widest-band height from glTF (old vs new comparison) |
| `catalog_update.py` | Read-modify-write of this batch's entries in `art/asset_catalog.json` (budgets: maples 10000, bush 2000, pines 3000, leaf_scatter 400, leaf_single 40) |
| `run.ps1` | `powershell -NoProfile -File art\team\foliage\batch03\run.ps1 build_maples.py id1,id2 -Preview` |

## Techniques that worked
- **Two-sided only where needed.** A leaf card lying on a mass surface only ever shows its outer face, so it is single-sided. Only cards that poke out are double-sided. This roughly doubles the leaf count for the same budget.
- **Custom split normals.** Each card carries a blend of its mass normal and the crown normal, so the crown shades as soft rounded masses (one toon terminator per mass) while its silhouette breaks into leaves.
- **Analytic per-leaf AO instead of ray AO.** Ray AO speckled the cards. The analytic AO makes mass tops light, undersides dark, crevices between masses dark, and the crown base darker.
- **Inner shell of large dark cards plus a small filler core** with a downward shading normal. Smooth per-mass cores read as shiny balloons, cobbles or "brains" whatever the texture. Shadow-only fillers read as depth.
- **Bug fixed:** newly created bmesh faces have no normal, so ear-clip triangulation filled the lobe notches. `normal_update()` is now called before triangulating.

## Log (passes)
- **Maples (13 passes):**
  1. Dark balloons with confetti.
  2. Single-sided cards.
  3. Notch triangulation fix.
  4. Broader lobes and deep core tints.
  5. Domed crown.
  6. Narrowed tints.
  7. Lead feedback: cards read as big paper stars. Moved to 0.3 m cards with a serrated 3+2-lobe outline.
  8. Mixed serrated and simple outlines for budget.
  9. Inner shell replaced the per-mass cores.
  10. Analytic AO.
  11. Darker tint ramps.
  12. Crevice darkening and per-mass normals.
  13. Shadow-only filler.
- **Pines (7 passes):** first tiers; more and irregular spikes, narrow tip band, minimum drop; darker tints with a material A/B (grass beat flat and hedge); per-clump normal jitter; upward normals; saturated green ramp; pine_b scheme.
- **Leaves (2 passes plus a fix):** two-tone midrib fold, petiole, clumps; sizes biased up, third clump, broader lower lobes.
- **Bush (8 passes):** cores plus small leaves (too sparse); bigger leaves; lower coverage; the hedge texture read as pebbles; the flat core read as glossy balls; no core at all was see-through; inner shell plus filler; shadow-only filler.

## Maples v3: batch02 crown form restored (user feedback)
The user preferred the old (batch02 v2) crown shape. The v2 build had become a thin umbrella cap: leaves mostly on top, a hollow underside, and a long bare trunk.

`build_maples.py` now rebuilds the old layout:
- the same centre, radii, inset, cluster size, `min_z` and low-branch lift rule as `yard_street/batch02/build_trees.py`;
- 20-21 lobes, each 1.22x the old size to stand in for the old blob bulges;
- leaves placed from one pool over the *exposed* surface of the lobe union (`surface_pool`), so undersides and lower flanks are covered;
- the leaf-card look is kept: ~720 cards of 0.34 m, ~210 inner-shell cards of 0.65 m, a shadow filler core, analytic AO and the 4-step tint ramp.

Budgets went to 10000 because the full crown has about twice the surface of the cap.

Measured with `measure_crowns.py` (crown = non-bark vertices; "bottom" is the 2nd percentile of crown heights):

| tree | top old / new | crown bottom old / new | width xÃ—z old / new | widest at h old / new | tris |
| --- | --- | --- | --- | --- | --- |
| red | 5.66 / 5.71 | 3.16 / 2.96 | 4.90Ã—4.47 / 4.85Ã—4.31 | 4.11 / 3.81 | 9533 |
| orange | 5.32 / 5.34 | 2.89 / 2.84 | 4.88Ã—4.77 / 4.96Ã—4.67 | 3.61 / 3.90 | 9341 |
| gold | 6.30 / 6.08 | 3.21 / 3.11 | 4.45Ã—4.14 / 4.48Ã—4.10 | 3.57 / 3.85 | 9699 |

All dimensions are within 10%, except the widest-band height: red is 7% low, orange 8% high, gold 8% high. That metric is noisy because the low-branch tip lobe and stray poke leaves set the extreme widths. `socket_branch` is unchanged at (1, 2.6, 0.04) in glTF Y-up, and the low branch and swing area are still excluded from foliage.

v3 passes:
1. Old layout at 17 lobes: leaf pool too sparse (411 leaves placed).
2. Pool density raised, lobes 1.22x: shape matched, but the crown read see-through with a dark-hole filler.
3. Cards at 0.34 m, larger inner shell, serrated outline only on the true silhouette, budget 10000: full crown.

## leaf_scatter v2 (user feedback: larger leaves, fewer per patch)
6 leaves of 0.22-0.32 m (about twice the old size) replace the 14 of 8-16 cm. They are spread loosely (centres at least 0.42 m apart within +/-0.58 m), and one leaf from each colour family (crimson, orange, gold, rust) is guaranteed. Gentler tilt and curl, stacked 4 mm apart so overlaps never z-fight. Footprint 1.22 x 1.25 m, height 0.002-0.053 m, 78 / 400 tris, single-sided. Preview: `art/review/engine/leaf_scatter.png`. The pass log: 1) placement code joined onto the next line by a script edit (syntax error, fixed); 2) 6 leaves over +/-0.52 m, footprint 1.15 x 1.05 m; 3) spread widened to +/-0.58 m.

## Weak / unresolved
- **Bright lit tops.** Under the preview's low, bright moon, the lit tops of red and orange leaves still clip toward pink or white. The toon ramp is nearly a step, so anything facing the moon gets full intensity, and bright saturated albedos clip. Darker tint ramps reduced this but did not remove it. The pines show the same thing as a frosty sheen. This needs a lighting or shader answer (moon energy, or a foliage spec/exposure mask).
- **Maple crowns (v3).** The form matches batch02. A few small dark gaps still show deep in the crown from some angles, and the lobes read as one full rounded mass more than as separate puffs.
- **Pines.** The silhouette is a stylised zig-zag. The lead says they look good in the woods.
- **leaf_single** uses one tint (maple_orange / leaf_tangerine). leaf_scatter's leaves are single-sided (seen from above only).
- **bush_round.** The lit sides wash pale under the preview's studio light. It always carries the same 9 cream flowers.
