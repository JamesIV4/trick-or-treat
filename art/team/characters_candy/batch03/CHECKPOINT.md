# characters_candy / batch03 checkpoint (v3: match the concept, art/review/concept/target.png)

## Built (all gates PASS, catalog published)

| Asset | Tris / budget | Engine preview | Script |
| --- | --- | --- | --- |
| dragon_kid | 9372 / 10000 (budget raised from 9000) | art/review/engine/dragon_kid.png | build_dragon_kid.py |
| candy_wrapped_pink / mint / grape / lemon | 396 / 400 each | art/review/engine/candy_wrapped_*.png | build_candy.py |
| candy_wrapped_orange (NEW) | 396 / 400 | art/review/engine/candy_wrapped_orange.png | build_candy.py |
| candy_corn (NEW) | 288 / 300 | art/review/engine/candy_corn.png | build_candy.py |

candy_chocolate_bar, candy_lollipop and candy_golden_bar are unchanged from batch02 (they are not
in the concept; the code is carried over so the script still builds the whole family).

Rebuild:
- `python art/team/characters_candy/batch03/set_palette_row.py` then `python art/tools/make_palette.py`
  (rewrites only the `dragon_green:` row, i.e. palette row 12, in place)
- `blender --background --factory-startup --python art/team/characters_candy/batch03/build_dragon_kid.py [-- --nosheet] [--pieces]`
- `... build_candy.py [-- <ids>] [--nosheet]`
- `python art/team/characters_candy/batch03/catalog_update.py` (adds the 2 new candy ids, sets dragon_kid budget 10000)
- Review helpers: `distance_check.py` (shrinks a preview sheet to gameplay size next to the concept kid),
  `candy_board.py` (candy row + concept crops + gameplay-size row) -> art/review/engine/candy_v3_board.png

## Palette row 12 (this lane)
`dragon_green` (lead's cell, value unchanged, not used by v3), `dragon_jade #30ad6b` (main onesie),
`dragon_green_dark`, `dragon_green_light` (wing membrane), `dragon_spike`, `dragon_gold` (horns,
claws, tail tip), `dragon_cream` (belly), `dragon_cream_dark` (belly ridges), `candy_orange`,
`candy_orange_light`, `candy_green`, `candy_green_light`, `candy_violet`, `candy_violet_light`,
`candy_magenta`. 15 of 16 cells. Note: dragon_jade was inserted second during iteration, so cells
after it shifted one column once; nothing outside this lane referenced them (grep of native/ found only
the generated header). Append new cells at the end from now on. dragon_teal* cells untouched.

## dragon_kid v3: what changed vs batch02
Rig contract unchanged (checked in the exported glTF: same node names, parents and translations,
translation-only nodes, bucket under arm_r, bucket_candy and socket_bucket_light under bucket).
- Colour: concept onesie measured at hue ~155-178 (jade/teal-green, not leaf green). A leaf green
  (dragon_green #4cb04a, pass 1) blended into the lawn and read yellower than the concept, so the main
  colour is dragon_jade #30ad6b; under the night light it lands on the concept's jade.
- Golden horns, bigger (0.15 m) and curving back, no collars; golden toe claws; gold spike on the tail tip.
- Yellow-green spike row: 4 hood crest + 4 back + 6 tail spikes (was 3 + 2 + 5), slightly larger.
- Wings 1.4x larger, spread outward past the hood (Y rot +6 deg) and leaned forward 18 deg so the back
  face catches the light from the high rear camera. Membrane dragon_green_light, smooth (flat), jade ribs thicker.
- Tail chunkier and longer (radius 0.132 at the base, reaches 0.8 m back), cream underside stops before the
  curl (it read as a white tip from behind).
- Belly: pale yellow-cream, smooth (felt read as burlap), 3 darker plate ridges, dark-green stitches.
- Hood: cartoon dragon eyes on the brow (concept), jade rim; brown hair fringe (4 tufts) + temple locks
  now poking out below the rim (pass 1 had it hidden under the rim).
- Bare skin hands instead of mittens (concept); wrist and boot cuffs removed (tris went to spikes/eyes/hair).
- Bucket heap recoloured with the new vivid candy cells.

Passes: v3a (leaf green, first shapes) -> v3b (jade, wings spread, hair lowered) -> v3c (smooth belly and
membrane, thicker ribs, tail stripe fix, bigger back spikes) -> v3d (wings leaned forward). Distance sheets:
art/review/engine/dragon_kid_v3{a,b,c,d}_dist.png (rightmost tile = concept kid at the same size).

## Candy v3: what changed
- Wrapped: new mesh (12-sided so the fan rim vertices land on lobe peaks): round ball, tight twisted
  necks, flat bow-tie fans with two rounded lobes, a centre pleat crease and a lighter tint, plus a white
  highlight streak and dot conformed to the upper front of the ball. Vivid colours: pink = candy_magenta /
  candy_pink fans, mint = candy_green / candy_green_light, grape = candy_violet / candy_violet_light,
  lemon = candy_lemon / sunflower_yellow fans, orange = candy_orange / candy_orange_light. Same size as
  batch02 (0.334 m tip to tip). The old spiral stripes are gone (the concept candies are solid with a highlight).
- candy_corn: 16-sided boxy-oval loft, 0.30 m tall x 0.23 wide, tip up, yellow / orange / white bands on
  ring boundaries, lighter highlight column on the front left. Origin at the centre.

## v3.1 (user feedback via lead: "Remove the modeled specular from candy and restore the candy stripes")
- Removed every modelled highlight: the white streak and dot decals on all five wrapped candies, and the
  lighter highlight column on candy_corn (now plain yellow / orange / white bands). The candy shader's
  gloss and rim glow do that job in game.
- Restored the batch02 stripes on all five wrapped candies: back to 18 sides, three spiral stripes
  (columns 0-1 of every 6, rings twisted by 8.5 x z along the axis) on the ball and necks. lolly_white on
  pink, mint, grape and orange. Lemon keeps the batch02 candy_cherry stripe, because white on the pale
  lemon body has no contrast (batch02 made that same call).
- Kept from v3: bold row-12 colours, round ball, twisted necks, bow-tie fans in a lighter tint, size
  (0.334 m tip to tip; corn 0.30 m tall). The fan lobes are now a piecewise-linear profile so they work with
  18 sides. The ball has one fewer ring per half to fit 396 tris, the same density as batch02.
- All gates PASS (wrapped 396/400, corn 288/300), catalog published. Previews: art/review/engine/candy_wrapped_{orange,pink,mint,grape,lemon}.png,
  candy_corn.png; board art/review/engine/candy_v3_board.png.
- The "Candy v3" section above describes the superseded v3 highlight version.

## Weak / unresolved
- Preview vs game shader: asset_preview renders candy with the toon material; in game candy.gdshader adds
  self-glow (0.55 x albedo) and a white rim. The pale lemon body may wash toward cream in game; not verified in game.
- Wrapped fans are low-poly (12 sides): the lobes read as two blunt bow lobes, not the concept's soft
  three-finger fans; the fan edge looks slightly blocky close up.
- dragon_kid: the felt layer still makes the hood/back look mottled close up (material-level). The face
  keeps the batch02 kawaii style (big ink eyes), not the concept's realistic profile. Four eyes read on the
  front (hood eyes + face eyes); it reads as a dragon hood but is busier than the concept's side view.
- Side view: wings are partly hidden behind the hood/body because of the forward lean.
- Old *_lifted.png previews in art/review/engine are stale batch02 renders (the preview tool lifts floating
  assets itself now).
