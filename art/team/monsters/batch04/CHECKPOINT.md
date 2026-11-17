# monsters / batch04 checkpoint

## Progress (final fixes 3; newest first, kept current in case of a cut-off)

- 06:15 (final) Done. Final run of the four changed ids (gremlin, bat, ghost, slime) from one build (DLL 06:00:35,
  built by that run) in `art\review\showcase\wip_m`, beat sheets rebuilt (all "frames aligned") and looked at. Proof
  strips (after above wip_m, same clip frames) `wip_m\strips\f4_*.png`. Measures `flash_metric_final3.json`. No model
  rebuilt (gates / verify_rig2 not needed). Scratch `wip_m\t3`, `wip_m\t3o` deleted. `game\override.cfg` absent.
  Nothing in flight.

### Hand-off (final fixes 3)

**Ids changed: monster_gremlin, monster_bat, monster_ghost, monster_slime.** No shader or shared code touched
(`monsters.*`, `monster_anim.*`, `monster_rig.*`, `toon.gdshader` unchanged), so the other six were not re-recorded
into `wip_m`; their `wip_m` clips are still the 04:29 ones.

| item | change (source) | result (strip) |
|---|---|---|
| 1 gremlin grin | `bite()`: the jaw closes on its own curve, u 0.42-0.78 of the lunge (~5 frames); the body's snap (stretch, head, claws) keeps 0.56-0.70 | the grin closes wide > half > grin > slit > shut over 1225-1231 / ~989-994, no swap (`f4_gr_atk1`, `f4_gr_atk2`, `f4_gr_atk2_face`) |
| 1 gremlin rise end | the rise's end pose (mouth shut, ears back) now fades out over 0.1 s (`rise_fade_`) instead of dropping in one frame | the grin opens and the ears rise over 1627-1632 (was one frame, 1626/1627) (`f4_gr_rise_end`) |
| 2a bat telegraph | `telegraph_gain` 2.2 -> 2.3 | flash_metric net 102.7 / 97.8 = 104 / 92% of BEFORE (was 102 / 90); body solid (`f4_ba_tele`) |
| 2b ghost face close | twirl: on the pulse's rise the eye closes over env 0..0.7, arcs grow 0.7..0.95 (opening keeps 0.3 / 0.6) | closes 380-384 (~4 frames, was 2) (`f4_gh_face`) |
| 2c ghost rise eyes | rise `shut` over k 0.78-0.97 (was 0.92); `open_time` still caps the rate | line at 1619, open by 1624 (~5 frames, was 4) (`f4_gh_rise`) |
| 2d ghost / slime death | ghost melt k 0.48-0.96 (was 0.64); slime root shrink 0.68-0.98 (was 0.86-1.0) | ghost sheet shrinks ~1543-1552 (7-8 visible frames), slime puddle ~1546-1551 (6) (`f4_gh_die`, `f4_sl_die`) |

Other gremlin mouth changes checked in AFTER: the wind-up opening is eased already (0.27 s); the hit flinch opens
the mouth on the white silhouette frames (1362-1363, covered); the death mix eases over 0.08 s; none changed.

**Regression proof** (`batch04\beatdiff.py <dirA> <dirB> <ids>`; `tele_cmp.py` runs the reviewer's tele5 on any folder: per beat, frame mean abs diff inside the reviewer's character box,
320x360): the noise floor between two runs of the same code is box median 0.9-3.5, p95 up to ~7 (die p95 28-43:
the random puff), from fireflies, sparkles and the puff. after vs final for the four ids: every beat in that range
except the ghost face beat (block max 152 only at 380-384, the item) and the bat die median 9.0 (the bat's pose is
identical in a strip at 1500-1591; the difference is the sparkles and fireflies). The other six were recorded once
with the final tree (`-NoBuild`, scratch `t3o`, deleted): after vs that run matches after vs the old 04:29 run beat
for beat (e.g. pumpkin attack_2 p95 4.9 / 6.4, mummy attack_2 5.7 / 1.9, skeleton attack_2 5.3 / 2.7); spot strips of
the highest beats (skeleton 1190-1215, mummy 1221-1260, gremlin chase 706-710, slime chase 824-828 and attack 990-994)
show the same poses with fireflies in different places.

**Weak points:** the reviewer's own pulse measure for the bat (tele5 P_med) moves only 6.07 -> 6.20 (83% of BEFORE)
with the gain: the tonemap compresses the added light, so reaching 90% there would need ~3x and push flash_metric's
first wind-up past 110%; left at 2.3. The gremlin rise's jaw still closes in about 3 frames before the landing
(1616-1618; not flagged). The diff tool cannot separate a small pose change from firefly noise at whole-beat level;
the claim of "unchanged" rests on the stats being inside the same-code range plus the spot strips.

- 2026-11-16 (start) Taken: gremlin grin snap (review v4 snap994) + nitpicks (bat telegraph gain, ghost face close,
  ghost rise eyes, ghost / slime death shrink). In source (built 05:49, recorded into scratch `t3`, then bat 2.3): gremlin `bite()` jaw closes on
  its own curve u 0.42-0.78 (was the body snap 0.56-0.70, ~2 frames); gremlin rise end pose fades out over 0.1 s
  (`rise_fade_`; the grin and ears popped at 1626/1627); bat `telegraph_gain` 2.2 -> 2.4; ghost twirl closes over
  env 0..0.7 on the rise (opening unchanged), rise eyes open over k 0.78-0.97 (was 0.92), death melt 0.48-0.96
  (was 0.64); slime death shrink 0.68-0.98 (was 0.86-1.0). Tools: `batch04\beatdiff.py`, `tele_cmp.py`.
  Noise floor (old wip_m 04:29 vs after 04:44, same monster code): frame mean 1.1-2.3 per beat, die p95 10-23
  (random puff), block max up to 70-120 (leaves / particles). Next: build, record gremlin, bat, ghost, slime.

## Progress (final fixes 2; newest first, kept current in case of a cut-off)

- 04:45 (final) Done. Final run of all ten from one build (DLL 04:29) in `art\review\showcase\wip_m`, beat sheets
  rebuilt (all "frames aligned"), proof strips `wip_m\strips\f3_*.png` (BEFORE / after / final, same clip frames),
  real play `wip_m\real\f2_spider_hitw.mp4`, `f2_mummy_hitw.mp4` (strips `real\strips\f2_*`). Measures in
  `flash_metric_final2.json`. No model rebuilt; gates + verify_rig2 PASS for all ten anyway. `game\override.cfg`
  absent. Scratch `wip_m\cal`, `wip_m\t1` deleted. Nothing in flight. Hand-off below.

### Hand-off (final fixes 2)

**Changed in source (all visual):** `toon.gdshader` `body_flash` is now a linear vec4 (rgb = flash, a = hit peak),
emission = flash x (surface brightness-weighted colour + a 0.35 floor) x AO x a soft top light + a strong rim, and
`a` whitens ALBEDO and adds near-white emission (the 2-frame white silhouette at the start of a hit). The old
`source_color` hint had squared the gains away (the main reason dark / saturated monsters barely flashed).
`monsters.cpp`: TELEGRAPH warmer (1.0, 0.68, 0.18); hit = peak for 2 frames then the fade; per-monster
`telegraph_gain()` / `hit_flash_gain()` in each animator (telegraph 1.0-2.2, hit 0.8-2.0). `MonsterFace::open_time`
(monster_anim.cpp): `shut` closes at once but opens over about 5 frames (bat, scarecrow, toadstool after the
rise; chevrons swap to a shut line that then opens). Ghost: happy face eased across the swap; death eyes go from
the squint to shut (no re-open); the flat sheet shrinks over the last 11 frames (0.64-0.96 of the death). Pumpkin
death shrink over 8 frames (0.74-0.97). Gremlin run: hands in toward the belly, bigger up-down pump, elbows down.

**Flash / telegraph, net light added (flash_metric.py; BEFORE / after clip / final):**

| monster | tele1 | tele2 | hit1 | hit2 |
|---|---|---|---|---|
| pumpkin | 145 / 45 / 126 | 152 / 56 / 135 | 286 / 168 / 338 | 310 / 169 / 348 |
| skeleton | 66 / 44 / 70 | 87 / 48 / 80 | 137 / 108 / 255 | 121 / 140 / 282 |
| bat | 98 / 12 / 102 | 106 / 29 / 96 | 136 / 37 / 187 | 144 / 22 / 127 |
| ghost | 63 / 36 / 67 | 72 / 32 / 64 | 110 / 71 / 188 | 92 / 66 / 188 |
| gremlin | 103 / 44 / 102 | 122 / 41 / 100 | 216 / 79 / 259 | 168 / 79 / 243 |
| mummy | 71 / 18 / 69 | 84 / 30 / 95 | 141 / 63 / 268 | 138 / 75 / 303 |
| slime | 120 / 22 / 111 | 123 / 36 / 112 | 227 / 102 / 229 | 216 / 90 / 203 |
| scarecrow | 73 / 10 / 91 | 94 / 12 / 103 | 159 / 40 / 252 | 162 / 10 / 258 |
| spider | 174 / 8 / 155 | 172 / 10 / 152 | 301 / 54 / 257 | 303 / 71 / 319 |
| toadstool | 157 / 54 / 140 | 165 / 64 / 149 | 268 / 77 / 322 | 275 / 78 / 345 |

Lowest: gremlin tele2 82%, spider hit1 85%, bat hit2 88%; everything else 87-200% of BEFORE. The reviewer's
tele.py area (scratch adaptation run on three sides): spider wind-up 20.6 / 5.6 / 19.7 %, every monster's wind-up
and hit area 73-130% of BEFORE.

**Known weak points:** the telegraph's fade passes through pink on purple monsters (warm flash over purple; BEFORE
did the same). Hits are brighter than BEFORE on most monsters (2 white frames, then a pale fade); solid in every
strip looked at (spider, gremlin, mummy, skeleton, scarecrow, ghost), but a lead may want them toned to ~100%.
Gremlin far arm: now bent and swinging (out, down, tucked behind the candy), not held straight; from the chase
camera it is often hidden behind the body, so the run still reads a little one-sided. Ghost puddle and pumpkin
are shrinking over 8-9 visible frames, inside the puff.

- 04:30 Run of all ten (DLL 04:08: TELEGRAPH warmer, spider gain) in `wip_m` measured: every monster's hit and both
  telegraphs at 81-128% of BEFORE (net, `flash_metric.py`) and the reviewer's area at 73-110%; strips `strips\f3_*`
  looked at: solid (spider 1403-1414 / 950-981, gremlin 1358-1369, mummy, skeleton, scarecrow both hits flash, ghost
  telegraph cream-warm). Ghost face, death eyes, bat / scarecrow / toadstool eyes ease open (4-6 frames). Then:
  ghost puddle shrink 0.76 -> 0.64 (9 visible frames, was 4) and gremlin run arms (hands in, bigger up-down pump,
  elbows down), checked in scratch `wip_m\t1` (DLL 04:23); gremlin hand peak lowered a little after it (not yet
  recorded). Next: real-play `-Hit windup:0.5` spider + mummy, final run of all ten, sheets, gates, hand-off.

- 03:50 Calibration run (`wip_m\cal`, all gains 1.0, scratch) measured and looked at: hit peak + fade solid on spider,
  mummy, scarecrow; telegraph at gain 1 reached 40-108% of BEFORE (dark ones lowest) and read pink on the spider ->
  shader now weights the surface's brightness over its hue (flash in its own colour), gains set per monster
  (telegraph 1.0-2.2, hit 0.8-2.0), hit peak trimmed to 2 frames. Ghost face / death eyes / puddle, toadstool,
  scarecrow, bat eyes, pumpkin death all ease as wanted in `cal`. Gremlin: near arm pumps, far arm still out ->
  elbow pole down-back, hands closer. TEMP calibration env removed. In flight: run of all ten into `wip_m`.

- 2026-11-16 (start) Taken: (1) flash/telegraph too faint, (2) ghost death eye pop + puddle vanish, pumpkin death shrink,
  (3) eyes snap open after the rise (bat, scarecrow, toadstool) and the ghost's happy face, (4) gremlin chase arms.
  Baseline metric `flash_metric.py before,after` (net light added by the flash; motion cancels): e.g. spider telegraph
  net 174 -> 8, scarecrow hit 2 162 -> 10. Cause found beyond the lead's: `body_flash` had a `source_color` hint, so the
  gains were squared away (0.3 * 1.6 sRGB = 0.2 linear). In source (built, not recorded): `body_flash` is vec4 linear
  (rgb flash, a = hit peak: whole body near white 2-3 frames), floor term shaded by AO and a top light, strong rim;
  `MonsterFace::open_time` (eyes never snap open); ghost happy face eased, death eyes stay shut, sheet shrinks over the
  last 7 frames; pumpkin death shrink 8 frames; gremlin run arms bent and pumping. TEMP `TOT_FLASH_CAL` env in
  monsters.cpp (calibration; must be removed). Next: calibration run of all ten into `wip_m\cal` (scratch).

## Progress (final fixes; newest first, kept current in case of a cut-off)

- 02:55 (final) Done; see "Final fixes (2026-11-16)" at the end of this file. All ten monsters recorded from one
  build (DLL 02:26) into `art\review\showcase\wip_m`, beat sheets rebuilt (all "frames aligned"), proof strips
  (after on top, final below, same clip frames) in `wip_m\strips\ff_*.png`, real-play hits at mid wind-up in
  `wip_m\real\ff_<spider|gremlin|mummy>_hitw.mp4` (strips in `real\strips\ff_*`). Rebuilt models spider + gremlin:
  gates PASS, verify_rig2 PASS, engine previews looked at. Native build OK. `game\override.cfg` absent. Nothing in
  flight. Scratch recordings (`wip_m\diag_*`, `r1`, `r2`) deleted.
- 02:30 r1 (all ten) and r2 (spider, gremlin, mummy, skeleton, ghost) looked at: no see-through anywhere; gremlin
  swipe below the chin, both arms pump, dark mouth; skeleton stacks legs > ribcage > skull (skull from the +X side,
  over the top: r1 had it on the bone arm's side when mirrored and it brushed the raised arm, fixed); ghost eyes stay
  on the sheet and shut before it is flat; ghost attack_2 eyes stay black (the brown was the old flat telegraph glow);
  mummy blink 8 frames, eye closes fully; slime eyes ease open over 4 frames; bat no blink after waking. Flash
  formula final: `body_flash * ((ALBEDO*3 + luma(ALBEDO)*1.6) * AO + rim)`; spider gain 0.25 -> 0.4, bat 0.2 -> 0.32
  (both barely read once the veil was gone). In flight: final run of all ten into `wip_m` (one build), sheets, then
  real-play hits (`real\ff_<type>_hitw`).
- 01:58 Flash cause found by elimination (spider, same frames, `wip_m\diag_*`): glow off, SSIL off, SSR off, MSAA
  off and ALL post effects off (glow, SSIL, SSAO, SSR, fog, DOF, contact AO) still see-through; flash colour forced
  to zero = solid. So it is the flat added emission itself: a uniform veil that lifts dark stripes and parts in
  front toward the colour behind them (frosted glass). Fix: new `toon.gdshader` instance uniform `body_flash`
  (emission = flash * (ALBEDO * 2.2 + rim * 0.5)), `MonsterRig::hit_flash` now drives it (nests keep `hit_flash`).
  TEMP diag code removed. Recording r1 of all ten into `wip_m\r1`.
- 01:50 In source, not yet recorded: skeleton rise (skull lies beside the heap, hops on last over the ribcage:
  `apply_pile`, `SKULL_RISE_*`), ghost death eyes (ride the sheet, shut and shrink before it is flat:
  `linger_eyes`), gremlin (run arms pump, lunge claws below the chin, new `mouth` part gloss -2 rim 0; model rebuilt,
  PASS), spider abdomen (plush + gloss -0.3; model rebuilt, PASS), mummy quick blink (`MonsterFace::blink_speed`,
  closed 0.02), bat `MonsterFace::wake_hold` 1.2 s, slime eyes eased open. Flash (item 1): investigating with a
  TEMP env switch `TOT_FX_DIAG` in `Monster::_process` (must be removed): glow off and SSIL off do NOT remove the
  see-through; testing SSR / all effects next. `wip_m\diag_*` are scratch recordings.

## Progress (real-play review; newest first, kept current in case of a cut-off)

- 01:00 (final) Real-play review finished; see "Real-play review" at the end of this file. Showcase re-recorded for
  pumpkin + spider + bat from the final DLL (beat sheets rebuilt; `wip_m\strips\pk_real_attack2.png`,
  `sp_real_attack2.png`: the held hop / pounce lands with the puff). gates + verify_rig2 PASS for all ten (no model
  touched). Autopilot `--seed=101 --autopilot=45` 1600x900: 173 / 184 / 185 / 183 fps (M1 181-189, M2 169-196, M3
  163-198: within noise). `game\override.cfg` absent. Nothing in flight. Gameplay bug for the lead: HOP / POUNCE
  leap velocity lost (see the section).
- 00:40 All real-play scenarios recorded and looked at (see "Real-play review" at the end). Spider feet now use a
  surface probe (kerbs / sidewalks / steps), idle scene fixed (nobody chasing, faces the camera), far scene camera
  down the street. In flight: showcase re-record of pumpkin + spider + bat (`scratchpad\showcase.log`). Next:
  sheets, autopilot fps, gates/verifier, override.cfg check, final notes.
- 23:40 Fixed and re-recorded (strips `real\strips\pk_street_176_239`, `pk_leap_200_263`, `sp_street_186_225`,
  `sp_leap_192_239`, `ba_street_488_519`): pumpkin wind-up no longer squashes into a disc in mid air (it can start
  mid-hop), held attack = a 13 cm hop that lands on the game's landing frame (puff), real -Leap path looked at and
  fine; spider held pounce compressed to the 0.155 s Attack (lands with the puff); bat dive capped at 0.8 rad pitch and
  reaches for the kid's chest when close (visual offset, sprung back; it hit 0.7 m above the head before). Switch: the
  camera is now from the far side, slides along the street to a clear spot, `-Res 1920x1080`, `TOT_MONSTER_CAM` takes
  aim fraction and aim height; `real_batch.ps1` runs several. In flight: batch of 14 (hits, crowds, idle, slope,
  kerb, steps) for pumpkin / spider / bat. Next: review them, then the other seven.
- 23:15 Resumed after a cut-off. Found in the tree (not noted before): the switch block in festival_game.cpp,
  `review_state()` / `monster_review_label` in monsters.*, MTRACE lines, `real_test.ps1`, `real_trace.py`, one
  recording `real\pk_street` (21:37) and the DLL built 21:40. FOUND: HOP / POUNCE attacks never leave the ground in
  the game (pre-existing, in git HEAD: the Windup branch of `physics_step` writes back the pre-attack velocity after
  `begin_attack`); not fixed (decision 5), `TOT_MONSTER_LEAP` is a review-only switch that keeps the leap. Added
  `real_strip.py`. Next: re-record pumpkin street (real and -Leap), strips, judge.
- 2026-11-15 (start) Real-play review taken: a review switch `TOT_MONSTER_TEST` (festival_game.cpp sibling block
  + trace in monsters.cpp), recordings into `art\review\showcase\wip_m\real\`, then fixes in the animators.
  Nothing changed yet. Next: write the switch.

## Progress (polish; newest first, kept current in case of a cut-off)

- (final) Polish round finished; see "Polish round" under M10 (pumpkin), M12 (spider) and M6 (skeleton). Final
  recording of pumpkin + spider + skeleton + bat from one DLL in `wip_m` (ended 21:26:25), beat sheets rebuilt,
  `strips\pk_compare.png` and `sp_compare.png` rebuilt. Bat unchanged against its 20:21 recording (`ba_strips.py`:
  1.6 to 3.7, death puff 10.1 / 16.7; poses identical by eye). gates + verify_rig2 PASS for all ten. Catalog briefs
  (pumpkin, skeleton, spider) updated and published. `game\override.cfg`: written 21:26:41 by the other lane's
  recording (started after mine ended), absent again at the end of this round; none of my runs left one. Nothing in flight.
  Technique notes for art-pipeline: (1) paint tubes by ring index (face order of a sweep), never by face position;
  (2) a part laid "under" a curved surface must be offset along the surface normal on the target's OWN facets
  (hi-res proxies and y offsets poke through near shoulders and grooves); (3) an alternate carved shape = carve the
  largest shape into the shell and fill it back with a parametric band of the same surface (cheap, no boolean),
  copying the shell's UVs per face from one source triangle; (4) a jack-o'-lantern reads as lit from inside through
  a graded light (flesh rim, glowing walls, a plate behind with a pale core, a small hot flame), and the pupils'
  life (look, blink, flinch) can live in the flame; (5) cover a mesh swap with a 0.09 s dip of the light;
  (6) soft-floor squash plus a stiffer spring for a slam, never a hard clamp (a clamp holds a pancake).

- (all three done in source and models; catalog briefs updated and published) Spider: ring-painted legs, 3-ring fade,
  fuller ruff, legs out of the shade pass, shin rim 0.06 / gloss -1, eased launch. Skeleton: rounded uneven ivory
  teeth, mouth back wall + floor (throat blob gone), body bones on stucco + gloss -1.6 (7,986 tris). Next: final run
  of pumpkin + spider + skeleton + bat from one build, sheets, strips, ba_strips on the bat, gates/verifier for all
  ten, hand-off sections.

- (pumpkin done, pending the final run) Face rebuilt (`pumpkin()`: triangle eyes 0.28 x 0.238, roar carved in
  the head + `mouth` lower-lip band, `mouth_glow`, eye plates + flames as eye_l/pupil_l, brows; 22 parts, 7,792
  tris, PASS) and the animator re-targeted (plates/flames/brows/roar/gutter; slam on a 6 Hz spring with a soft
  floor). Recorded 5 rounds; strips `wip_m\strips\pk_p1_*`, `pk_p2_*`; compare rebuilt. Next: spider.

- 2026-11-15 (start) Polish round taken: (1) pumpkin face rebuilt as a carved jack-o'-lantern (triangle
  eyes, wide toothed grin, blazing light, ember cores instead of pupils), slam squash eased; (2) spider leg
  bands as clean rings + soft tip gradient, ruff presence, eased launch; (3) skeleton teeth (ivory, small,
  uneven, gum shadow), matte body bone, closed mouth back. Nothing changed yet. Next: pumpkin face.

## Progress (Spider, monster_spider; newest first, kept current in case of a cut-off)

- (final) Spider finished; see "M12 (Spider)" at the end of this file. Final recording of spider + skeleton +
  bat from one DLL in `wip_m` (strips `sp_final_*`, compare `strips\sp_compare.png`); skeleton and bat unchanged
  against their 19:45 recordings (ba_strips 0.8 to 2.5, death puff 10 / 18-20). Catalog brief set and published;
  gates + verifier PASS for all ten v2 monsters. `game\override.cfg` absent. Nothing in flight.
- (rounds 1-4, animator) `monster_anim_spider.cpp` written; recordings `sp_r1_*`, `sp_r2_*`. A probe showed the
  showcase holds the monster still through Attack (velocity 0, on the floor), so the pounce runs on its own
  clock with a visual leap in place when held (see M12).
- (model round 3) `spider()` in `build_monsters2.py` published (7,688 tris, 32 mesh parts = the cap; gates +
  verifier PASS). New palette cell `spider_berry` (row 13, its last free slot; head fur). Part tree in the
  docstring: body (thorax hub) > head > eyes, eye_s0..2 (mirrored PAIRS of small eyes), fang_l/r, jaw, tuft,
  alt_squint; body > abdomen (carved glowing jack face) > alt_silk; body > leg_XN > shin_XN (IK, end = foot
  contact). Engine preview looked at (both rows). Next step: write `native\src\play\monster_anim_spider.cpp`
  (world-planted feet in two groups, air poses for pounce/hit/rise/death), build, showcase.
- 2026-11-15 (start) Spider taken. Nothing built yet. Reading the pilot, bat and mock. Next step: plan the
  part tree, write `spider()` in `build_monsters2.py`, run `go2.ps1 -Keys spider`.

## Progress (Bat, monster_bat; newest first, kept current in case of a cut-off)

- 19:50 (final) Bat finished; see "M11 (Bat)" at the end of this file. Final recording of bat + skeleton +
  spider from one DLL in `wip_m` (strips `bat_r1_*` .. `bat_r4_*`, compare `strips\bat_compare.png`); skeleton
  (vs the 19:10 recording) and spider (v1, vs BEFORE) unchanged by ba_strips. Catalog brief set and published;
  gates + verifier PASS for all nine v2 monsters. `game\override.cfg` only from the other lane's recording that
  started 19:48:09, after mine ended. Nothing in flight.

- 19:33 (round 1) Model `bat()` in `build_monsters2.py` published (7,590 tris, 20 mesh parts, gates + verifier
  PASS; no new palette cells: hat_violet / spider_plum fur, bat_wing membrane, candy_pink / house_plum ears,
  linen_light / linen belly, witch_purple tuft, nose and feet). Kit: `M.pad(up, aspect, outline)`, and the pose
  sheet now frames a `floating` rig (ground under it, full span). Animator `monster_anim_bat.cpp` written,
  BUILD OK. Next: showcase round 1 (`tools\showcase.ps1 -Ids monster_bat -Out art\review\showcase\wip_m`),
  sheets, strips of every beat, iterate.

- 2026-11-15 19:30 (start) Bat taken. Kit: `M.pad` gained optional `up`, `aspect`, `outline` (oval, tufted
  edge; old calls unchanged). Plan: part tree body (torso at the origin: belly fluff, neck ruff, tail nub) >
  head (big round head, cheek fluff, lip line, fangs, nose, blush) > ear_l/r (soft shells, pink inside),
  tuft (soft), eye_l/r > pupil_l/r (M.eye: sclera, amber two-tone iris, pupil, glints, lash line),
  brow_l/r, jaw (open mouth scaled about the lip line), alt_squint_l/r; body > wing_l/r (arm + inner
  membrane + thumb, ball at the shoulder) > wingtip_l/r (three fingers + outer membrane; rest frame +Y
  along finger 1 = the fold hinge, so the fold never opens a gap); body > foot_l/r. Animator
  `native\src\play\monster_anim_bat.cpp`. Next step: write `bat()` in `build_monsters2.py`, run
  `go2.ps1 -Keys bat`.

## Progress (Pumpkin Hopper, monster_pumpkin; newest first, kept current in case of a cut-off)

- 19:20 (final) Pumpkin finished; see "M10 (Pumpkin Hopper)" at the end of this file. Final recording of
  pumpkin + skeleton + bat from one DLL in `wip_m` (strips `pk_final_*`, compare `pk_compare.png`);
  skeleton (vs the 18:40 recording) and bat (vs BEFORE) unchanged by ba_strips. Catalog brief set and
  published; gates + verifier PASS for all eight v2 monsters. `game\override.cfg` only from the other lane's
  running recordings. Nothing in flight.

- 19:20 (round 1) Model `pumpkin()` in `build_monsters2.py`, published (7,734 tris, 18 mesh parts, gates +
  verifier PASS, no new palette cells: candy_orange / pumpkin_dark / rust / amber / glow_jack /
  glow_jack_hot / glow_jack_shell / gourd_green / apple_green / dragon_green_light / vine / vine_dark).
  Face carved with `F.carve` (farm_lib) into `F.pumpkin_body`. Animator `monster_anim_pumpkin.cpp` written,
  BUILD OK. Round-1 showcase recording in flight. Next: review beat sheet + strips, iterate.

- 2026-11-15 18:50 (start) Pumpkin taken. Nothing built yet. Plan (FIDELITY_PASS decision 7: faithful to
  the PROCEDURAL pumpkin, ~1.0 m wide x 0.8 m, triangle eyes, slot grin with three upper teeth): part tree
  body (meshless joint on the ground) > head (the lobed shell, `F.pumpkin_body` + `F.carve` pockets for the
  eyes and grin, pivot at its bottom: squash, rock, look) > stem (soft, curled), leaf_a..d (soft, collar),
  tendril_l/r (soft curls), eye_l/r > pupil_l/r (`M.eye`: a glow dome inside each carved triangle, ink
  pupil), arm_l/r (vine, ball at a sunk shoulder) > hand_l/r (three leaves); body > foot_l/r (root feet).
  Animator `native\src\play\monster_anim_pumpkin.cpp`. Next step: write `pumpkin()` in `build_monsters2.py`,
  run `go2.ps1 -Keys pumpkin`.

## Progress (Goo Slime, monster_slime; newest first, kept current in case of a cut-off)

- (final) Slime finished; see "M9 (Goo Slime)" at the end of this file. Final recording of slime +
  skeleton + bat from one DLL in `wip_m` (strips `sl_*`, compare `sl_compare.png`); skeleton and bat
  unchanged (ba_strips). Catalog brief updated and published. Nothing in flight.
- (round 2, done) Model `slime()` published (7,414 tris, 21 mesh parts, gates + verifier PASS; new
  cell `goo_lime` row 13). Animator `monster_anim_slime.cpp` written, BUILD OK, recorded once (round 1
  strips `wip_m\strips\sl_r1_*`). Round 2 changes: darker jelly, rim 0.3, bigger open mouth, gentler
  wind-up flatten, smaller skirt kicks, thicker spawn puddle. Next: record round 2, review strips.
- (model round 1, done) Writing `slime()` in `build_monsters2.py`. Part tree as planned below,
  except: `skirt` is a child of `head` (it squashes and spreads with the jelly), the curl is a separate
  soft part `curl`, four sliding bubbles `bubble_a..d`, `alt_happy_l/r`, and a hidden `alt_goo` (the
  projectile's look via `thrown_part`). Colours from existing cells (apple_green body, dragon_green_light
  swirls/cheeks, slime_light bubbles). Next step: `go2.ps1 -Keys slime`, look at the sheets.
- 2026-11-15 (start) Slime taken. Nothing built yet. Plan: part tree body (meshless hip joint at the
  ground centre) > head (the whole gumdrop + curled tip as ONE soft mesh, pinned low; raised bubbles,
  inner highlight swirl, the lip line) > eye_l/r > pupil_l/r, brow_l/r, mouth (cavity + tongue, scaled
  open; O by X-narrowing), cheek_l/r, alt_squint_l/r; body > skirt (ring of rounded drips, one soft mesh,
  cloth_wave). Animator `native\src\play\monster_anim_slime.cpp`. Next step: write `slime()` in
  `build_monsters2.py`, run `go2.ps1 -Keys slime`.

## Progress (Ghost, monster_ghost; newest first, kept current in case of a cut-off)

- (final) Ghost finished; see "M8 (Ghost)" at the end of this file. Model + animator published and
  built; final recording of ghost in `wip_m` (strips `gh_final_*`, compare `gh_compare.png`); skeleton
  + slime recorded from the same DLL and unchanged (ba_strips). Catalog brief updated. Nothing in flight.
- (rounds 1-6) Model `ghost()` in `build_monsters2.py`, published (7,426 tris, 16 mesh parts, gates + verifier
  PASS; cells silver / ghost_cloth / ghost_deep, no new palette cells). Animator `monster_anim_ghost.cpp`.
  Six recordings (strips `wip_m\strips\gh_r1_*` .. `gh_r6_*`).
- 2026-11-15 (start) Ghost taken. Nothing built yet. Plan: part tree body (meshless hip joint at the
  sheet's centre of mass) > head (the whole sheet: smooth dome + deep-lobed hem as ONE soft mesh,
  bent by soft_bend opposite velocity, cloth_wave ripple below the waist) > eye_l/r > pupil_l/r,
  mouth (cavity + tongue, scaled open), cheek_l/r, alt_squint_l/r, arm_l/r (mitten nubs on a spring).
  Animator `native\src\play\monster_anim_ghost.cpp`. Next step: write `ghost()` in
  `build_monsters2.py`, run `go2.ps1 -Keys ghost`.

## Progress (Toadstool Imp, monster_toadstool; newest first, kept current in case of a cut-off)

- (final) Toadstool finished; see "M7 (Toadstool Imp)" at the end of this file. Last change: the feet
  sink with the stalk in the death. Final recording (toadstool + skeleton + slime from one DLL) is the
  run `ts_final2`; catalog brief updated and published; gates + verifier PASS.

- (round 3) Animator `monster_anim_toadstool.cpp` written and recorded twice (strips `wip_m\strips\ts_r1_*`,
  `ts_r2_*`). Changed since round 1: cap cell `toad_red` (row 13, appended; leaf_crimson went salmon),
  cap gloss -0.55, arm swing applied before the raise (+fwd stays forward), hit cap drop smaller and
  both hands pushing up in front, cap rides back on the run, heavier cap pendulum (2.2 Hz), rise shows
  the sprout first. Next step: record round 3, then the final run with monster_skeleton + monster_slime
  (regressions), ba_strips, override.cfg check, catalog brief, hand-off section.

- (model round 2) `toadstool()` is in `build_monsters2.py` and published (7,399 tris, 20 mesh parts, gates +
  verifier PASS, no new palette cells: ivory / leaf_crimson / bone / bone_tan / apple_green). Kit gained
  `M.pad` (conforming raised pad). Engine preview and pose sheet looked at (no gaps; the stalk runs up
  into the closed cap). Next step: write `native\src\play\monster_anim_toadstool.cpp`, build, showcase.
- 2026-11-15 (start) Toadstool taken. Nothing built yet. Plan: part tree body (meshless hip joint) >
  head (the cream stalk with the face, blush) > cap (red dome, raised spot pads, gills underneath; on a
  spring in the animator) > sprout (soft: stem + two leaves); head > eye_l/r > pupil_l/r, brow_l/r,
  mouth (scaled open), cheek_l/r (puff), alt_squint_l/r (shut-eye chevrons), arm_l/r > hand_l/r;
  body > foot_l/r. Palette: two new cells in row 13 (stalk cream and its shade). Animator
  `native\src\play\monster_anim_toadstool.cpp`. Next step: write `toadstool()` in `build_monsters2.py`,
  run `go2.ps1 -Keys toadstool`.

## Progress (Rattlebones head rework, monster_skeleton; newest first, kept current in case of a cut-off)

- 16:05 Head rework finished; see "M6 (Rattlebones head rework)" at the end of this file. State:
  skeleton model rebuilt (7,954 tris, 23 parts, gates + verifier PASS), mummy model rebuilt (eye ring
  colour only), native build OK; `wip_m` holds the final skeleton + slime recording (one DLL) and the
  round-2 mummy + scarecrow recordings (their code has not changed since). Catalog brief updated and
  published. `game\override.cfg` absent. Nothing is in flight.
- 15:40 Round 2 recording in flight (skeleton + mummy + scarecrow from one DLL). Round 1 (engine):
  reads as a skull at every distance; fixed since: shade band round the lower face (down -0.45 ->
  -0.8), teeth `linen_light` (unshaded), wider lower face (less of a ledge), lights bigger and dimmer
  (glow 0.45 rest, they clipped white), throat back painted `ivory_deep`. Audit fixes made: mummy eye
  dome gloss 0.9 -> -1 and inner iris ring `glow_amber` -> `glow_amber_dim` (model rebuilt), scarecrow
  hat / hat_tip gloss 0.03 -> -1. Shader: `gloss < 0` = matte (drops the painterly highlight; Lane P
  added: <= -2 also drops the environment sheen). Pending in source, not yet built: mandible inside
  `ivory_deep` instead of ink (black hole in the back view). Next: review round 2, then regressions.
- 2026-11-15 (start) User feedback: the v2 head reads as a robot (smooth glossy ball, glassy teal lens
  eyes with white rings). Task: rebuild the HEAD only as a cartoon bone skull (cranium, brow ridges,
  cheekbones, upper jaw with blocky teeth, separate mandible with chin and teeth, dark sockets with a
  soft teal light inside, matte chalky ivory), adapt the face code in `monster_anim_skeleton.cpp`,
  then audit the mummy / scarecrow / gremlin faces (FIDELITY_PASS decision 14). Pre-rework recordings
  copied to the session scratchpad `prev_head\`. Next step: rebuild `skeleton()`'s skull section.

## Progress (Gremlin, monster_gremlin; newest first, kept current in case of a cut-off)

- 14:30 Gremlin finished; see "M5 (Gremlin)" at the end of this file. State of the tree: model built
  and verified (7,858 tris, 25 mesh parts, gates + verifier PASS), native build OK, `wip_m` holds the
  final recording of monster_gremlin plus monster_skeleton and monster_slime from the same DLL
  (unchanged against the previous skeleton recording / BEFORE). Catalog brief updated and published.
  `game\override.cfg` is absent. Nothing is in flight.
- 14:20 Final recording in flight: monster_gremlin + monster_skeleton + monster_slime in one run
  (regressions). Rounds 3-5 reviewed (`wip_m\strips\gr_r3_*` .. `gr_r5_*`): the chin plate now takes
  the candy's normals (custom normals, invisible on the face), hugs the mouth as a U (no orange tab
  over the yellow at a grin), mouth 8.6 cm deep, grin 0.031 rad, tongue rooted inside the mouth,
  wind-up hands out and back, rise eye pop fixed (eyes doubled in size). Next step: sheets, compare
  with BEFORE / mock, `ba_strips.py` against `scratchpad\prev` (skeleton) and BEFORE (slime), check
  `game\override.cfg`, catalog brief, hand-off section.
- 14:10 Round 3 in flight. Rounds 1-2 reviewed (strips `wip_m\strips\gr_r1_*`, `gr_r2_*`). Changed:
  orange is `candy_orange` (pumpkin went peach), candy layer `plush` (vinyl read as crumpled
  cellophane), candy gloss 0.03, hit flash 0.35, idle look off (the face beat turned away), hands
  kept off the face (idle rub at the belly, chase claws out to the sides), the mouth cavity patch is
  flipped (it faced into the candy and was culled: no mouth showed) with a darker throat, shallower
  wind-up squash. Next step: review round 3 (face still, attack_2 / attack_1 strips), then regressions.
- (round 1) Model `gremlin()` built and published (7,858 tris, 25 parts, gates + verifier PASS; the
  "head" is the whole candy, "body" a meshless hip joint, the jaw a chin plate on a 0.98 m pivot:
  open 0.03 rad = grin, 0.1 = wide). Animator `monster_anim_gremlin.cpp` written, BUILD OK, not yet
  seen in motion. Next step: showcase round 1 (`tools\showcase.ps1 -Ids monster_gremlin -Out
  art\review\showcase\wip_m`), sheets, strips of every beat, iterate. Old recordings of skeleton /
  slime / gremlin copied to the session scratchpad `prev\` for ba_strips.
- 2026-11-15 (start) Gremlin taken. Nothing built yet. Plan: part tree body (yellow band, hips) >
  head (orange + white bands, face; its lower edge is a sphere about its pivot hidden under the
  yellow lip, so it can turn without a gap) > tip (soft white curl), ear_l/r, brow_l/r, eye_l/r >
  pupil_l/r, jaw > tongue (soft); body > arm_l/r > forearm_l/r > hand_l/r (IK), leg_l/r > shin_l/r >
  foot_l/r (IK). Animator `native\src\play\monster_anim_gremlin.cpp`. Next step: write `gremlin()` in
  `build_monsters2.py`, run `go2.ps1 -Keys gremlin`.

## Progress (Scarecrow, monster_scarecrow; newest first, kept current in case of a cut-off)

- 2026-11-15 13:50 Scarecrow finished; see "M4 (Scarecrow)" at the end of this file. State of the
  tree: model built and verified (7,746 tris, 22 parts, gates + verifier PASS), native build OK,
  `wip_m` holds the round-4 recording of monster_scarecrow plus monster_skeleton and monster_slime
  re-recorded in one run (unchanged against the pilot recording / BEFORE). `game\override.cfg` is
  absent. Nothing is in flight.

- 2026-11-15 13:35 Round 4 in flight: the hat is now the `plush` layer (the `felt` layer is a crumpled
  foil relief; finer UVs did not help), the forward head loll is capped at 0.26 rad. Model rebuilt
  (gates + verifier PASS). The showcase build failed inside Lane P's `kid_rig.cpp` (mid-edit); a
  background retry loop (6 x 2 min) then records. Next step: review round 4, then regressions
  (skeleton + slime with `ba_strips.py`), then the hand-off section.
- 2026-11-15 13:20 Round 2 in flight. Reviewed round 1 (strips of every beat in `wip_m\strips\sc_r1_*`).
  Changed: palette cell `hat_violet` (row 13) for the hat, straw recoloured to golden_leaf / straw_dark /
  caramel, hair roots lowered at the back (they poked up through the brim), gloss lowered (hat 0.03,
  buttons 0.12: they flashed white), hit flash 0.3, the head no longer inherits the death heap's squash
  (the hat landed as a huge flat disc), dizzy cone 0.34 rad with the head counter-leaning 0.55, deeper
  wind-up lean. Model rebuilt (7,746 tris, gates + verifier PASS), DLL built. Next step: showcase round 2.
- 2026-11-15 13:16 Resumed after a cut-off. A round-1 recording exists in `wip_m` (12:52, DLL 12:51,
  log `scarecrow=v2(22)`, beat sheet built); the glTF was re-exported at 12:48 after a kit edit.
  Next step: review that round's beat sheet and strips, then iterate.

- 2026-11-15 (round 1) Model `scarecrow()` is in `build_monsters2.py` and published (v2, 22 parts, 7,642
  tris, gates + `verify_rig2.py` PASS; kit gained `M.straw`, `M.button`, `eye(eye_extra=, pupil_extra=)`
  and wave normals in `Rig.finish`). Animator `native\src\play\monster_anim_scarecrow.cpp` is written
  and compiles (BUILD OK), not yet reviewed in motion. Next step: `tools\showcase.ps1 -Ids
  monster_scarecrow -Out art\review\showcase\wip_m`, sheets, a strip of every beat, iterate; then the
  catalog brief, the regression recordings (skeleton + one v1 monster) and the hand-off section.
- 2026-11-15 (start) Scarecrow taken. Nothing built yet. Plan: part tree `pole > body > head > hat >
  hat_tip` (see the header comment of `scarecrow()` in `build_monsters2.py` once it exists), animator
  `native\src\play\monster_anim_scarecrow.cpp`. Next step: write `scarecrow()` in `build_monsters2.py`,
  run `go2.ps1 -Keys scarecrow`.

## Progress (Mummy, monster_mummy; newest first, kept current in case of a cut-off)

- 2026-11-15 08:32 Mummy finished; see "M3 (Mummy)" near the end of this file. State of the tree:
  model built and verified (7,982 tris, 25 parts, gates and `verify_rig2.py` PASS for monster_mummy
  and monster_skeleton), native build OK, `art\review\showcase\wip_m` holds the final recording of
  monster_mummy plus monster_skeleton and monster_slime re-recorded from the same DLL (skeleton
  matches the pilot recording, slime matches BEFORE). `game\override.cfg` is absent. Nothing is in flight.
## Progress (M2, kit + Rattlebones pilot; newest first, kept current in case of a cut-off)

- 2026-11-15 08:35 M2 finished; see "M2 (done)" at the end of this file. State of the tree: model
  rebuilt and verified (7,870 tris, 23 parts, gates and `verify_rig2.py` PASS), native build OK,
  `art\review\showcase\wip_m` holds the final recording of monster_skeleton plus monster_slime and
  monster_bat from the same build (both still match BEFORE). Nothing is in flight.
- 2026-11-15 07:40 M2 resumed after a cut-off. The four lead review items (white bone, the white
  ellipse, the hit blob, per-beat strips) are all closed in the final recording.

Read `art/team/FIDELITY_PASS.md` first (binding), then `DESIGN.md` here. This file records what exists
after each chunk and the contracts the next worker must follow.

## M1 (done): rig and animator framework, no visual change

Every monster is now built from a part list and posed by an animator object. All ten still use the
old look and motion (their glTFs are v1, so they get `LegacyAnimator`). Nothing in Blender changed.

### Code map

| File | What it holds |
|---|---|
| `native/src/gen/rig_part.h` | `RigPart` (name, parent, rest, own mesh, reach, tip, flags `HIDDEN` / `TINY`) and `RigSocket`. |
| `native/src/gen/asset_library.*` | `AssetLibrary::part_tree(id, parts, &sockets)`: the glTF as a parent-first part list. Returns 0 (unpublished or no `body`), 1 (only body / part_a / part_b) or 2 (any other named part). |
| `native/src/play/monster_art.*` | `MonsterArt::parts`, `sockets`, `rig_version`; `set_legacy_parts()` fills them for the procedural models; `apply_assets` swaps in the glTF part tree. `PUMPKIN_HOPPER_USES_V2_MODEL` is in `monster_art.h`. Collision `height` / `radius` are still set only by the procedural `build_*` functions. |
| `native/src/play/monster_rig.*` | `MonsterRig`: nodes under the monster's visual root, slot lookup, posing, shader params, world / model space queries. |
| `native/src/play/monster_anim.h` | `AnimInput`, `MonsterAnimator`, `make_animator`, `TOT_MONSTER_ANIMATOR`, shared helpers (`root_pose`, `MonsterFace`, `HitRecoil`, `Gait`, `RootSquash`). |
| `native/src/play/monster_anim.cpp` | The registry, `LegacyAnimator` (the old `Monster::_process` body, moved verbatim) and the helpers. |
| `native/src/play/monsters.*` | `Monster::init` builds the rig and animator; `_process` fills `AnimInput`, calls `update`, fades the flash; `_physics_process` uses `rise_time_` / `death_time_`; `begin_attack` asks `muzzle()`. AI, tuning, `State` and the `showcase_*` hooks are untouched. |
| `native/src/play/monster_anim_<key>.cpp` | Does not exist yet: one per converted monster, written by the worker who converts it. |

### v2 glTF contract (what `part_tree` and `MonsterRig` expect)

- **Root.** Keep the empty `monster_<key>` as the single glTF root at the origin, as in batch03. Godot
  wraps it in one more scene root named after the file, so the imported tree is
  `monster_<key>2 > monster_<key> > body ...`. Neither is a part: **a meshless node that is above every
  part and is not called `body` is a container** (only its transform is carried down). So a meshless
  joint is fine below a part (a neck pivot) but not at the top of the tree. Front faces glTF +Z, +X is
  the monster's left, Y up, feet on y = 0 (the bat stays centred near the origin as today).
- **Parts are found by name, never by depth or order.** Every other node becomes a part named after
  the node, parented to the nearest part above it (or to the visual root). Names must be unique, with
  no dots (Godot renames duplicates and dotted names; a duplicate logs a warning and slot lookup finds
  the first).
- **Check what was loaded:** the game prints one line at start,
  `Trick or Treat: monster rigs: pumpkin=procedural v1(3) bat=v1(3) ...` (model source, rig version,
  part count). Look for it in `<out>\<id>.log` after a showcase run.
- **There must be a part called `body`** or the model is ignored (the procedural one is used).
  A model whose parts are only `body`, `part_a`, `part_b` is version 1 and gets `LegacyAnimator`.
  Any other part name makes it version 2 and it gets the animator registered for that monster type.
  In v2 nothing is special about `body` except that it must exist; hang the tree from it as DESIGN.md
  section 3 lists.
- **Pivot = the node's origin** (its glTF translation, in its parent's space). The mesh is in the
  node's own space, so vertices are relative to the pivot.
- **Own mesh only.** A part's mesh is just that node's mesh; children are separate parts. A node with
  no mesh is a plain joint (allowed, e.g. a neck pivot).
- **Rest rotation is allowed, rest scale is not** (a scale logs a warning and is dropped). Poses are
  applied after the rest rotation: `node = (rest.basis * rot * scale, rest.origin + offset)`.
  Limb chains that will use `anim::two_bone_ik` must be translation-only (no rest rotation).
- **Eyes** (for `MonsterFace`): an eye is a part whose origin is the eye's centre, local +Z pointing out
  of the face and +Y up; use a rest rotation to sit it on a curved head. Its pupil is a child part of
  the eye. Names: `eye_l` / `eye_r` with `pupil_l` / `pupil_r`, or a single `eye` with `pupil`; small
  extra eyes `eye_s0` .. `eye_s5` only blink. The head is `head`.
- **`socket_*` nodes are not joints.** They are stored as `RigSocket` (name without the prefix, the
  part they ride on, local transform) and read with `rig.socket_world("muzzle", xf)`. Nothing under a
  socket is rigged.
- **`alt_*` nodes** (alternate mouth shapes, closed-eye arcs) are parts flagged `HIDDEN`: built hidden,
  with everything under them, until the animator calls `rig.show(slot, true)`. The asset library's
  bakes and `instantiate()` leave them out (Lane P added that).
- **Tiny parts cast no shadow:** any v2 part whose `reach` (pivot to farthest vertex) is under 0.10 m
  is flagged `TINY`. Override per part with `rig.cast_shadow(slot, on)`.
- **Soft and ripple parts are not recognised from the file.** The animator declares them in `bind()`:
  `rig.soft(slot, dir, length, rigid)` sets `soft_pin` and widens the cull margin by 0.35 m, then
  `rig.bend(slot, rotvec)` drives `soft_bend` each frame and `rig.ripple(slot, amp, phase, lobes,
  below_height)` drives `cloth_wave`. For the art this means: a soft part is one continuous mesh whose
  **pivot is the pin** (the end that stays attached), with enough edge loops along its run to bend
  smoothly. `reach` and `tip` (the farthest vertex) come from the mesh, so `rig.soft(slot)` with no
  arguments bends over the whole reach measured by distance from the pivot, and `rig.tip(slot)` is the
  rest direction to give `anim::Dangle::step`. `cloth_wave` runs round the part's local Y axis.
- **Budgets:** at most 32 mesh parts and 8,000 triangles per monster (FIDELITY_PASS decision 9).
- **Collision** stays code-owned (`build_*` in `monster_art.cpp`); the art must fit it within about 10 %.

### Writing and registering an animator

One file per monster, `native/src/play/monster_anim_<key>.cpp` (the build globs `src/*/*.cpp`). The
header comment of `monster_anim.h` has the skeleton. In short:

```cpp
#include "play/monster_anim.h"
#include "play/monsters.h" // only for in.tuning
namespace tot {
namespace {
class SkeletonAnimator : public MonsterAnimator {
	int chest_ = -1; MonsterFace face_; HitRecoil hit_; Gait gait_; RootSquash squash_;
	void bind(MonsterRig &rig) override { chest_ = rig.slot("chest"); face_.bind(rig); }
	void update(const AnimInput &in, MonsterRig &rig, godot::Transform3D &root) override {
		rig.pose(chest_, ...);                      // slot -1 is a no-op everywhere
		root = root_pose(in.yaw, hit_.update(in), squash_.update(in, 1.0f));
	}
	float death_time() const override { return 0.6f; } // optional, 0.1 .. 0.6
};
} // namespace
TOT_MONSTER_ANIMATOR(MonsterType::Skeleton, SkeletonAnimator)
} // namespace tot
```

- The animator is used only when that monster's glTF is version 2. Until then `LegacyAnimator` runs.
  A v2 model with no registered animator also falls back to `LegacyAnimator` and logs one warning.
- `update` must write `root` (the visual root's local transform, which carries the facing). Rise,
  death and squash belong there so they work for any part count.
- `rise_time()` / `death_time()` are read once after `bind()` and clamped to 0.1 .. 0.6 s; they set how
  long `Rise` lasts before roaming and how long `Dying` lasts before the node is freed.
- `muzzle(rig, out_world)`: return true and fill the projectile's spawn point (THROW monsters); it is
  called on the physics tick the attack begins. Default keeps `origin + 0.9 * height`.
- `telegraph_gain()`: scale of the whole-body orange wind-up flash (1 = today).
- `distance_lod()`: true by default. A monster farther than 35 m from the camera is then updated every
  third frame with `in.lod = 1` and three frames' `dt`; the shared helpers skip their springs, blinks
  and look-at when `in.lod` is 1. `LegacyAnimator` returns false.
- Animation variety must come from `in.seed` (or `anim::Rand`), never from the monster's gameplay RNG.

### AnimInput fields

"Local" means the facing frame: the monster's origin turned by `yaw` (+Z front, +X left, Y up).

| Field | Meaning |
|---|---|
| `type`, `tuning` | Which monster, and its read-only `MonsterTuning` (attack kind, `windup`, `flying`, `hops`, `speed` ...). |
| `state`, `prev_state`, `entered` | `MonsterAnimState` (same order as `Monster::State`: Rise, Roam, Windup, Attack, Recover, Hurt, Dying), the state before it, and true on the first update after a change. |
| `state_t`, `state_len` | Seconds in the state; its fixed length (Rise, Dying, Windup, Hurt = 0.28) or 0 when open-ended. |
| `windup` | 0..1 through the wind-up, 0 otherwise. |
| `cooldown` | Seconds until it may attack again. |
| `dizzy` | Seconds of post-charge dizziness left (CHARGE monsters; 0 = not dizzy). |
| `since_hit` | Seconds since the last hit taken (1000 = never). |
| `dt`, `t` | Time since the last update; the monster's own animation clock. |
| `yaw` | Facing, radians about +Y. |
| `speed` | Horizontal speed, m/s (the body's velocity, as the old code used). |
| `vel_local`, `accel_local` | Velocity (y = vertical) and smoothed acceleration. |
| `player_local` | The player's feet relative to the monster's origin. |
| `attack_dir_local` | Unit, flat: where the wind-up or attack is aimed. |
| `hit_dir_local` | Unit, flat: the way the last hit pushed it. |
| `hop_in` | Hoppers: seconds until the next roaming hop may start (0 for the rest). |
| `on_floor`, `chasing` | `is_on_floor()`; the player is inside aggro range. |
| `origin_world`, `ground` | The monster's world origin, and a pointer to the terrain height function `(x, z) -> y` (may be null): for the spider's foot planting. |
| `height`, `radius` | The collision capsule. |
| `seed` | Per-monster variety seed. |
| `lod` | 0 near, 1 far. |

### Shared helpers (`monster_anim.h`)

- `root_pose(yaw, lean, scale, offset)`: the root transform. `lean` is a rotation vector in the facing
  frame (x > 0 tips the top forward); `scale` squashes along the upright axes.
- `MonsterFace`: `bind(rig)` finds the head, eyes and pupils; `update(in, rig)` poses eyes (blink to
  Y scale 0.08, hit squeeze, `shut`, `wide`) and pupils (slide up to 30 % of the eye's reach toward the
  player), sets `face.head` (yaw, pitch with 0.15 s lag, limits 35 / 20 degrees; apply with
  `anim::look_euler`) and drives the eye glow (`glow_rest` 0.35 to `glow_flare` 1.6 through the
  wind-up; off unless `glow_rest >= 0`).
- `HitRecoil`: `update(in)` returns a lean that tips 25 degrees away from the blow and springs back;
  `fresh` is true on the frame a hit lands, `ouch` is a 0.35 s envelope.
- `Gait`: `step(in, stride)` advances a phase by distance travelled; `swing()`, `bounce()`, `amount`.
- `RootSquash`: `update(in, target)` returns a volume-preserving springy scale and dips on landings.
- Everything else (springs, `Dangle`, IK, noise, `Blink`, pose blending) is `native/src/play/anim_kit.h`.
- `MonsterRig` extras worth knowing: `model_xf` / `model_rest` (visual-root space, cheap, safe while
  the root is scaled to nothing: do look-at maths there), `world_rest` (the frame `Dangle::step` wants),
  `glow`, `gloss`, `show`, `hit_flash`, `param`.

### Pumpkin switch

`PUMPKIN_HOPPER_USES_V2_MODEL` (`monster_art.h`, currently `true`). The hopper stays procedural while
`monster_pumpkin.gltf` is the v1 file. Publishing a v2 `monster_pumpkin` (any part beyond body / part_a
/ part_b) switches it in automatically; set the constant to `false` to go back to the procedural one.

### Gotchas found in M1

- Godot's importer adds a scene root above the glTF's own root empty. The first version of `part_tree`
  turned that empty into a part, which made every model "version 2" and switched the pumpkin to the
  Blender model. Hence the container rule above; the `monster rigs:` log line is the quick check.
- Part meshes are re-committed through a `SurfaceTool` (as `part_mesh` always did), so they carry no
  importer LODs or shadow meshes and render exactly like the old baked parts.
- `MonsterRig::pose` sets the whole local transform from rest every call; it is not additive. Compose
  everything for one part (look-at, recoil, pose) before calling it once.
- `MonsterFace::update` measures the look from where the head's ancestors are now and in model space,
  so call it after posing the chest / neck and before posing the head. World transforms are a frame
  late for the root (the root is applied after `update` returns) and are singular on the first Rise
  frame (root scale 0), so avoid `world_*` for look maths.
- `in.state_t` advances on physics ticks, `in.t` and `in.dt` on rendered frames.
- The legacy animation code must stay byte-for-byte in `LegacyAnimator::update`; it is the reference
  for every unconverted monster and it is what the clips in `art\review\showcase\before` show.

### What was verified in M1, and what was not

- All ten monsters recorded with `tools\showcase.ps1` into `art\review\showcase\wip_m` and compared
  with `art\review\showcase\before`: every beat sheet was looked at beside its BEFORE sheet, and
  `ba_strips.py` compared 8 frames per beat (72 per monster) at the same frame numbers. Poses, timing,
  flashes, the scarecrow's dizzy spin, the thrown bone and goo and the pumpkin's procedural model all
  match; the mean pixel difference is 0.6 to 3.3 everywhere except the frames with random particles
  (the death puff and sparkles, up to 24; the toadstool's spore puff, 10 to 12). For scale: the same measure gave 3 to
  20 when the pumpkin model was wrongly swapped, and 3 to 6 when a different animator was running.
- Frame time: Movie Maker's own numbers per clip are unchanged on the CPU (0.57 to 0.61 ms against 0.55
  to 0.67 before) and 0.02 to 0.08 ms higher on the GPU (about 3.96 against 3.90). The toon shader
  gained four instance uniforms from Lane P between the two recordings, so that small GPU difference
  cannot be pinned on the rig. One `--seed=101 --autopilot=45` run at 1600x900 reported 181 to 189 fps
  with fights in it; there is no earlier autopilot number to compare it with.
- The shared helpers were smoke-tested once with a throwaway animator on the v1 skeleton (not in the
  tree any more): `root_pose`, `HitRecoil`, `RootSquash`, `Gait`, `rig.soft` + `rig.bend` from an
  `anim::Dangle` (the arm bent as one soft mesh, so the shader plumbing works), `rig.gloss`, and the
  hit squeeze of `MonsterFace` on a stand-in part. It ran clean and looked sane in frame strips of the
  hit and chase beats. `muzzle()` was overridden in that test too, but the spawn point was not inspected.
- **Not verified in the engine:** a real v2 glTF (nested parts with rest rotations, `socket_*`,
  `alt_*`, the `TINY` rule); `MonsterFace` head look and pupil slide (no model has a head or pupils
  yet), its eye glow and the small-eye ripple; `rig.ripple`; `telegraph_gain`; the distance LOD (no
  animator that allows it exists yet, and the showcase camera is always near); `HitRecoil` after it was
  changed to keep its lean in world space (a small change made after the smoke test). The first v2
  worker should expect to shake these out on the skeleton pilot.

### Tools in this folder

- `ba_strips.py <before_dir> <after_dir> <out_dir> id...`: for each beat, a strip of 8 frames with
  BEFORE on top and AFTER below at the same frame numbers, plus the mean pixel difference per frame.

### Commands

```powershell
# build only (under the Godot lock); the showcase script also builds
. tools\godot_lock.ps1; $l = Enter-GodotLock; try { $env:PATH = "C:\msys64\mingw64\bin;$env:PATH"; Push-Location native
  scons platform=windows use_mingw=yes mingw_prefix= target=template_debug custom_api_file=extension_api.json -j8 *> build.log
  Pop-Location } finally { Exit-GodotLock $l }
# record and review (about 45 s per id; run detached with a log when recording more than two)
tools\showcase.ps1 -Ids monster_skeleton,monster_bat -Out art\review\showcase\wip_m
python tools\showcase_sheets.py art\review\showcase\wip_m monster_skeleton monster_bat
# compare with art\review\showcase\before\<id>_beats.png and <id>.mp4 (same frame numbers)
```

## M2 (done): monster kit v2 and the Rattlebones pilot

Rattlebones (`monster_skeleton`) is the first v2 monster: 23 parts, 7,870 triangles, its own animator.
The other nine are untouched (slime and bat were re-recorded from the final build and still match
BEFORE: mean pixel difference 1 to 3, death puff frames 9 to 21, as in M1).

### Files added in this folder

| File | What it is |
|---|---|
| `mon_kit2.py` | The kit. `import mon_kit2 as M` re-exports all of batch03's `mon_kit` (blob, tube, bone, smile, cheek, put, bvh, surf, catmull ...; `M.F` farm_lib, `M.K` lowpoly_kit, `M.D` dk_kit) and adds the rig tree and the shapes below. |
| `build_monsters2.py` | One function per monster, `BUILDS = {"skeleton": skeleton}`. Add yours here. |
| `go2.ps1 -Keys a,b [-NoPreview] [-NoSheet] [-Pieces]` | Blender build, catalog update, gates, rig verifier, catalog publish, engine preview. |
| `verify_rig2.py [ids]` | Checks the exported glTF against the v2 contract and `rig_spec\<id>.json`; writes `rig_check.json`. |
| `catalog_update2.py [ids]` | Sets `tris` (8000), `parts`, `sockets` of converted monsters from the specs (decision 9). |
| `palette_append.py` | Appends Lane M cells (row 14, then row 13). Add to `CELLS`, run it, then `python art\tools\make_palette.py`. |
| `build_native.ps1` | Debug DLL build under the Godot lock; prints `BUILD OK` or the first errors. |
| `strips2.py <id> <beat> [--n --cols --from --to --crop --w --before --out]` | Frame strip of one beat; `--before` stacks the same frames of the BEFORE clip underneath. |
| `preview_big.ps1`, `sheet_compose.py` | A larger engine preview cut into quadrants; the pose sheet's labeller (called by the kit). |
| `rig_spec\`, `rig_check.json` | Written by the build and the verifier. |

### Kit usage (`mon_kit2.py`)

All positions are Blender world points in the rest pose; write them in game coordinates through
`G = lambda x, y, z: M.g2b((x, y, z))` (+X the monster's left, +Y up, +Z front).

```python
R = M.Rig("monster_skeleton", height=1.2, radius=0.3)        # the C++ collision capsule, unchanged
R.part("body", [meshes...], pivot)                             # must exist; parents before children
R.part("chest", [...], pivot, parent="body")
R.part("arm_l", [...], shoulder, parent="chest", ball=0.051)   # ball joint: a sphere at the pivot
R.ik_chain("arm_l", "forearm_l", "hand_l")                     # verifier: translation-only chain
M.eye(R, "eye_l", "pupil_l", centre, normal, w, h, parent="head", ...)
R.part("bandana", [M.shell(...)], pin, parent="chest", soft=dict(dir=G(0, -1, 0.2), rigid=0.012, loops=6))
R.socket("muzzle", point, "hand_r")
R.shade({"ivory": "ivory_shade"}, parts=[...])                 # baked two-tone shading, see below
R.finish(ao_radius=0.2, ao_strength=0.9, poses=[...])          # AO, export, spec, review + pose sheets
```

- **`Rig.part`**: merges the objects into the node `name` with its origin at `pivot`. `z_out` gives a
  rest rotation (eyes only; never on IK chains). `ball=r` adds the ball joint; author the parent so
  its end overlaps the ball by 3 to 4 cm (the verifier fails a parent that does not reach in).
  `soft=` declares a shader-bent part (the verifier counts its vertex bands). `ground=False` for parts
  that never come near the floor. Names are checked for dots, duplicates and Godot import-hint
  endings (`_col`, `_rigid`, `_loop` ...).
- **`M.eye`**: an eye part (sclera dome + two authored glints) and its pupil child (iris disc + pupil),
  both with the eye's frame as rest rotation, so a blink is a Y scale and the pupil slides in XY.
  Living eye: white sclera, coloured iris, ink pupil. Skull socket (the skeleton): ink dome, dim glow
  iris, bright glow pupil; with `glow_rest = 0.35` in the animator the pupil survives the bloom.
- **`M.limb`** (flared bone segment), **`M.shell`** (thick slab between two parametric surfaces: soft
  parts, jaws), **`M.patch`** (thin decal), **`M.ribbon`** (tapered strap along a path),
  **`M.ellipse` / `M.heart`** (outlines for `F.carve`).
- **`Rig.bake_ao`** (called by `finish`): each part is occluded only by itself, the ball joints of its
  own children, parts named in `ao_from`, and the ground. No dark patch is left when a limb lifts.
- **`Rig.shade(pairs, ao_cut, down, parts)`**: after the AO bake, faces painted with a lit cell drop to
  its shade cell when they face the ground or sit in a crease. This is the "shaded cells" look within
  the one-cell-per-face palette rule. Leave big smooth surfaces (a skull, a jaw) out of `parts`: on
  large faces a shade cell is a hard-edged rectangle.
- **`Rig.pose_sheet`** (via `finish(poses=...)`): `art\review\sheets\<id>_poses.png`, one column per
  extreme pose, two views. Pose steps use game axes and compose like `Basis * Basis` in C++, so the
  numbers can be copied to and from the animator.

### The skeleton's animator (`native\src\play\monster_anim_skeleton.cpp`)

The pattern worth copying: a `Pose` struct of targets (pelvis, chest, head, hand and foot targets, jaw),
a locomotion pose, state layers mixed over it by `anim::Weight` (wind-up, follow-through, recover,
flinch, fidgets), springs on top (bobble head, landing dip, bandana `Dangle`s), then one solve: pelvis,
chest, `MonsterFace`, head, arms by two-bone IK in chest space, legs by IK with feet planted in model
space. Rise and death blend the solved pose with a hand-authored heap (`apply_pile`).

Three small framework additions made for it (all default to the old behaviour):
- `MonsterAnimator::hit_flash_gain()`: scales the hit flash (the skeleton uses 0.5; the fade time is
  unchanged). A pale monster under the full flash is a white blob.
- `MonsterAnimator::thrown_part(rig)`: the part a THROW monster's projectile should look like.
  `Monster::init` hands that mesh, recentred, to `Combat::set_projectile_look` once. The path, spin
  and hit test of the projectile are unchanged. The old bone projectile was a white ellipsoid 0.26 x
  0.5 m: that was the "floating ellipse" in the attack tiles (it is in BEFORE too).
- `face_.head_lag` is lowered to 0.03 s during wind-up and attack so the skull stays on its target
  while the chest whips.

### Technique notes (candidates for art-pipeline)

- **Pale characters need a darker base than the reference colour.** The showcase key light (1.5) and
  the game's bloom clip anything near `#f4dcb2` to flat white. `ivory #d0b086` with a shade cell
  `#a67c5a` and line cell `#6c4837` reads as the mock's warm ivory in the engine and keeps its form.
  Tune the cell, not the model: cells are addressed by position, so `make_palette.py` plus a
  re-record shows a colour change without a Blender rebuild.
- **Gloss on a large pale surface must be low** (0.1 to 0.15; eyes 0.9) and the monster rim light
  lower than the default 0.35 (0.12 here), or the highlight eats the shading.
- **A featureless dome gets its form from lines, not bumps.** Sutures (thin three-sided tubes half
  sunk in the surface, zig-zagged, in the darkest cell) read at every distance. Bumps under the skull
  were tried and read as ears; shade cells on the skull were hard-edged rectangles.
- **Per-part AO** plus **ball joints** is what makes rigid parts hold up in extreme poses.
- **The projectile must be the held prop's own mesh**, hidden in the hand on the frame it spawns, with
  the hand held at the release point for the last frames of the wind-up (the spawn reads the pose of
  the last rendered frame).
- **Read strips at full size before believing a small tile.** A 250 px tile of the throw looked like
  the skull had turned its back; at 400 px it was in profile, on target.

### Verified in M2, and not

- Looked at: both lighting rows of `art\review\engine\monster_skeleton.png`; the pose sheet (six
  extreme poses, no gaps at any joint); the beat sheet beside BEFORE and the mock; frame strips of
  turntable, wander, chase, attack_1 (plus the whip at full size), attack_2 (release), hit (with
  BEFORE underneath), die and rise_idle. `--seed=101 --autopilot=45` at 1600x900: 169 to 196 fps
  (M1: 181 to 189).
- The `monster rigs:` log line reads `skeleton=v2(23)`, so nested parts with rest rotations, the
  socket, `rig.soft` / `rig.bend` on a real v2 file and the `TINY` rule (eyes and pupils) all ran.
- Not verified: the fidgets (the showcase idle is too short to reach one); `alt_*` parts and
  `rig.ripple` (the skeleton has neither); the distance LOD path (camera always near); foot slide was
  judged by eye and from the maths (the planted foot moves back at body speed once speed is over
  0.35 m/s), not measured; a crowd of skeletons in play (mirrored piles, projectile look set once).

### Known weak points

- The wind-up is a wide "javelin" pose rather than an arm cocked behind the skull: the skull is 0.27 m
  in radius and the arm 0.33 m long, so the hand cannot get behind it with a bent elbow.
- The projectile is the true size of the held bone (0.33 m). It is much smaller than the old stand-in;
  it carries a rim and a faint glow, but its readability as a threat in play was not tested.
- At the moment of release the flying bone starts from a fixed orientation, not the held one (it spins
  at 14 rad/s, so this is one frame).
- During a spawn the chest pops up a few frames before the skull, so for about 0.08 s the ribcage is
  above the head.
- Scene fireflies drift through the showcase stage; they are in BEFORE as well.
- `bone_warm` and `bone_tan` (added earlier in M2, now unused by the skeleton) are still in row 14.

### Recipe: taking another monster end to end

1. **Read** this file's v2 contract and DESIGN.md section 3 / 4 for your monster; look at
   `ref\mock_<key>.png` and `art\review\showcase\before\<id>_beats.png`, `_front/_back/_face.png`.
2. **Palette**: add any new cells to `CELLS` in `palette_append.py`, run it, then
   `python art\tools\make_palette.py`. Start darker than the mock for anything pale.
3. **Model**: add `def <key>():` to `build_monsters2.py` and to `BUILDS`. `K.reset()`, build pieces in
   world space, `R.part(...)` parents first, eyes with `M.eye`, soft parts as one `M.shell` /
   `M.ribbon` pinned at the pivot, `R.ik_chain` for IK limbs (no `z_out` on them), `R.socket` for
   muzzles, a `poses` list of five or six extremes, `R.shade` for two-tone bone or cloth, `R.finish`.
4. **Build and check**: `powershell -NoProfile -File art\team\monsters\batch04\go2.ps1 -Keys <key>`.
   Wants `PASS` twice (gates, verifier), no `WARN`. Read `art\review\sheets\monster_<key>.png`,
   `..._poses.png` (no gaps) and `art\review\engine\monster_<key>.png` (both lighting rows).
5. **Animator**: copy the shape of `monster_anim_skeleton.cpp` into `monster_anim_<key>.cpp`, end it
   with `TOT_MONSTER_ANIMATOR(MonsterType::X, XAnimator)`. Declare soft parts and gloss in `bind()`.
   Then `powershell -NoProfile -File art\team\monsters\batch04\build_native.ps1`.
6. **Record and look**: `tools\showcase.ps1 -Ids monster_<key> -Out art\review\showcase\wip_m`,
   `python tools\showcase_sheets.py art\review\showcase\wip_m monster_<key>`, check
   `monster rigs: ... <key>=v2(n)` in `wip_m\monster_<key>.log`, then a strip per beat:
   `python art\team\monsters\batch04\strips2.py monster_<key> hit --n 16 --cols 8 --before`.
7. **Confirm nothing else moved**: record one or two other monsters and run `ba_strips.py` on them.

Pitfalls met on the pilot:
- The 8,000 triangle cap is easy to hit; `go2.ps1 -Pieces` prints each piece. Decals and tubes add up.
- A v2 file switches the monster to its animator the moment it is published. Until the animator
  exists it runs `LegacyAnimator` with a warning and most parts stay at rest.
- `rig.pose` is absolute: compose everything for a part and call it once per frame.
- Do look-at maths in model space (`rig.model_xf`), and call `face_.update` after posing the chest and
  before posing the head.
- Take the bone / goo release from `in.state == Attack && in.entered`; `state_t` runs on physics ticks.
- PowerShell `Replace` on these files needs LF anchors; the Python sources use LF line ends.
- `preview_big.ps1` gave the same pixel size as the normal preview on this machine; use the showcase
  face beat for close-ups.

## M3 (Mummy): monster_mummy, v2 model and animator

The Mummy is the second v2 monster: 25 parts, 7,982 triangles, `native\src\play\monster_anim_mummy.cpp`.
Collision is unchanged (height 1.4, radius 0.32; the art stands 1.47 m).

### What exists

- **Model** (`mummy()` in `build_monsters2.py`): every body part is an under-wrap blob (`linen_shade`)
  with padded bandage strips laid over it in three tones and three heights (`M.strap`), so the wraps
  really overlap and cross: an X over the chest front and back, two straps crossing on the crown, two
  "lid" bands that part round the big eye, two diagonals that cross beside it and leave the slit the
  second eye squints through. One big amber eye (`eye` > `pupil`, dark dome, dim amber iris, bright
  inner ring, ink pupil, two glints) and the squinting one (`eye_s0` > `pupil_s0`), both built with
  `M.eye`. Head knot with two tails, waist knot with a tail, a loose strip on the left wrist: four
  soft parts, each one ribbon with a ragged end, pinned at its pivot. Ball joints at shoulders,
  elbows, wrists, hips, knees and ankles; hips / chest / head are closed blobs that overlap, so no
  gap can open at the waist or the neck. `alt_unwind_a` / `alt_unwind_b` are two helix streamers that
  stay hidden until the death animation shows them (the first `alt_*` parts used in the game).
- **Part tree:** body (hips) > chest > head > eye > pupil, eye_s0 > pupil_s0, knot_tail_a, knot_tail_b;
  chest > arm_l/r > forearm_l/r > hand_l/r, forearm_l > wrist_strip; body > leg_l/r > shin_l/r >
  foot_l/r, waist_tail, alt_unwind_a, alt_unwind_b. The arms are IK chains that point FORWARD at rest
  (zombie arms), so the rest pose is the iconic one; the legs hang straight down.
- **Palette** (row 13): `linen_light`, `linen`, `linen_shade`, `linen_deep`, `linen_dark`,
  `glow_amber_dim`, `glow_amber`. Material layer: `burlap` with the UVs scaled 5.5 times (a gauze weave).
- **The face is authored with the big eye on the monster's right and mirrored by `FS` in the
  builder** (FS = 1: big eye on its left). Most showcase cameras stand on the monster's left front
  (azimuth +0.5 / +0.6 / +0.87), so that is the side the big eye is on.
- **Animator:** locomotion pose, state layers mixed by `anim::Weight`, springs, then the solve, as on
  the skeleton. Its own ideas, worth reusing:
  - *The hips ride as high as the stiff legs allow.* After the feet are placed, the pelvis height is
    the lowest of "ankle height + what a nearly straight leg can reach" over the two feet. That one
    line gives the stiff-legged vaulting bob, and it keeps both feet reachable in every pose (wide
    wind-up stance, lunge) without hand-tuned crouches.
  - *The hands trail the body.* Each wrist target (chest space) is chased by a `Spring3` in model
    space, so every bob, lurch and lunge reaches the hands late and overshoots; the spring is
    stiffened during the grab so the clap stays sharp.
  - *The head lolls* on a rotation-vector spring kicked by footfalls, acceleration, hits and landings;
    a much stiffer copy tips the whole root (a toy that nearly falls over when it starts and stops).
  - *Hit:* the head quarter-spins on a fixed 0.07 / 0.1 / 0.2 s envelope (out, hold, back with
    overshoot), the other way each hit; the arms are thrown up; the legs stick out stiff in the air.
  - *Death (0.6 s):* a short wind back, then 1.75 accelerating turns while the root thins to a third
    and sinks; the two `alt_unwind` helices are shown, counter-rotated so they stay behind in the
    world, and scaled wide and flat; the four soft ends are flung out by a centrifugal `accel`.
  - *Rise (0.6 s):* flat on its back for a beat, then up like a plank about its heels on a damped
    cosine (well past upright, back, to rest), arms straight out, eyes shut until it is nearly up.
    Monsters spawn 0.3 m above the ground and drop: until it lands the root is offset down by its
    height over `in.ground`, so the plank lies on the ground instead of in mid-air.
  - Eye glow: 0.14 at rest (amber, not white), 0.75 at the end of the wind-up (more and the bloom
    eats the pupil). Telegraph and hit flash gains are 0.3.
  - Loose ends use `Dangle` with `gravity = 26`, so they hang toward the ground whatever the part
    they are tied to does (lying on its back, a rolled head).
  - The squinting eye is not driven by `MonsterFace` beyond blinking: the animator copies the big
    pupil's slide to `pupil_s0` (scaled) and sets its glow itself.

### Added to the kit (`mon_kit2.py`), all backward compatible

- `M.strap(name, on, n, half_w, height, edge, closed, color, edge_color, mat, uv_scale, fade)`: a padded
  strip lying on a surface, three quads across (edge on the surface, top proud, edge), smooth shaded.
  `on(t, s)` gives the surface point and normal; three makers:
  `M.band_fn(tree, centre, axis, lat, ref_r, a0, a1, front, wave)` (a circle of latitude about any axis
  round a blob), `M.arc_fn(tree, centre, d0, d1, ref_r, bow)` (the short way between two directions),
  `M.ring_fn(a, b, radius, at, tilt, phase)` (a slanted ring round a straight limb).
  `M.axis_tilt(pitch, roll, yaw)` makes the axis; `M.proxy_tree(make)` gives the BVH of a throwaway
  high-resolution copy of a blob to lay the strips on (the real base can then be coarse).
- `M.eye(..., iris2=, iris2_r=)`: a second, inner iris ring. With it the decal layers are spread
  further apart (it z-fought at a distance); without it the eye builds exactly as before.
- `Rig.finish(closeups=[(suffix, zoom, focus)])`: extra review sheets (`<id>_face.png`); parts named
  `alt_*` are hidden in every Blender sheet.
- Nothing was added to `monster_anim.h` or `anim_kit.h`.

### Technique notes from the Mummy (candidates for art-pipeline)

- **Wraps are geometry, not texture.** A base blob in the bottom tone plus separate padded strips
  (three quads across, the two edge quads sloping down to the surface, all smooth shaded) reads as
  layered bandage at every distance; bulging lathe bands (batch03) read as stacked rings. Give the
  strips three heights (6 / 9 / 12 mm) and three tones, tilt their axes so they cross, and paint the
  sloping edges one tone darker: that is the outline. About 100 triangles per ring of 16 segments.
- **Make the base the bottom layer, not a hole.** With a dark base every crack between strips looked
  like a cage; with the base in the bottom wrap tone the same cracks look like more bandage.
- **Lay strips on a throwaway high-resolution proxy** (`M.proxy_tree`), keep the real base coarse:
  the strip edges then follow the true surface and the base hides under them.
- **Scale the weave, do not change the layer.** `burlap` at its own scale is a sack; with the strip
  UVs running along the strip and scaled 5.5 times it is gauze. Base blobs need `F.scale_uv` too.
- **An eye whose pupil part slides flat needs a shallow dome.** `MonsterFace` slides the pupil in the
  eye's XY plane; on a deep dome the trailing edge of a big iris sinks into the sclera. Depth about a
  quarter of the radius and `pupil_travel` 0.14 keep a 78 % iris clean. Stacked decals (iris, inner
  ring, pupil, glints) need about 2.5 mm between layers or they z-fight at a distance.
- **A glow iris at glow_boost 1 is a white disc.** `#ff9a26` is already amber at 0.14; the wind-up
  flare (1.5) is what is allowed to bloom. The preview scene does not run the animator, so the eye
  looks blown there and correct in the game.
- **Put the face's feature on the side the cameras stand.** The showcase and most gameplay views see
  a monster from its left front; a one-eyed face authored on the right hides its eye in every still.
- **Pale monsters: flash gains about 0.3.** At 0.55 the hit flash was still a white silhouette on linen.
- **Stiff legs: derive the hip height from the feet** (see the animator notes above) instead of
  authoring a bob.
- **A spawn or death "prop" can be an `alt_*` part**: built hidden, shown and scaled by the animator
  for half a second (the unwinding streamers). They cost triangles all the time, so keep them light.

### Verified for the Mummy, and not

- Looked at: the Blender review, face and torso sheets; the pose sheet (six extreme poses, two views,
  no gap at any joint, waist or neck); both lighting rows of `art\review\engine\monster_mummy.png`
  (enlarged); the beat sheet beside BEFORE and the mock; the front / back / face stills; frame
  strips at 400 px of every beat in three recording rounds: turntable, face, wander, chase, attack_1
  and attack_2 (wind-up, clap, recover wobble; plus an eye close-up of the flare), hit, die, rise_idle.
- `monster rigs:` log line reads `mummy=v2(25)`. The `alt_*` parts stay hidden until the death shows
  them (first use of `alt_*` and of `rig.show` on them in the game).
- From the same DLL: monster_skeleton against a copy of the pilot's recording (`ba_strips.py`: mean
  pixel difference 0.9 to 2.4, death puff frames 6 to 19) and monster_slime against BEFORE (0.9 to
  2.5, death puff 5.5 to 23): unchanged, as in M1 / M2. Both beat sheets looked at.
- `--seed=101 --autopilot=45` at 1600x900: 163 to 198 fps (M2: 169 to 196), no errors.
- Not verified: the scratch and wrist-strip fidgets in the final build (both were seen in the first
  recording, before the 5 s idle delay; the final clip only reaches the look-round); the distance LOD
  path; a crowd of mummies and a spawn at a real nest (the rise is held down on the terrain height
  `in.ground` gives while the body drops: on a floor that stands above the terrain it would sink
  into it for those few frames); a hit in the middle of a wind-up; foot slide was judged by eye and
  from the maths, not measured; the mirrored death (`mirror_` seed bit).

### Known weak points (Mummy)

- A blink is the eye part scaled flat, so the shut big eye is a thin bright amber line, not a lid.
- In the face close-up the weave is strong (the layer's strength is fixed; only its scale was tuned).
- The wraps have no frayed edges, only the ragged ends of the four loose tails; the budget is spent
  (7,982 of 8,000), 344 of it on the two death streamers.
- The death's middle is largely behind the game's own puff cloud and sparkles; the streamers are
  18 and 24 segments and look polygonal in a still.
- Flash gains are 0.3 (telegraph and hit) so the pose stays readable on pale cloth; whether the
  wind-up still telegraphs clearly enough in play (pose plus eye flare) was not playtested.
- A knocked monster turns to face the way it is thrown (gameplay), so in the showcase hit beat part
  of the head spin is seen from behind.
- The loose ends do not collide with the body; the waist tail can pass through the right thigh.
- Mitten hands with a thumb, no fingers. The squinting eye is small and reads only from the left front.

### Rebuild

```powershell
powershell -NoProfile -File art\team\monsters\batch04\go2.ps1 -Keys mummy        # model, gates, verifier, preview
powershell -NoProfile -File art\team\monsters\batch04\build_native.ps1            # animator
tools\showcase.ps1 -Ids monster_mummy -Out art\review\showcase\wip_m
python tools\showcase_sheets.py art\review\showcase\wip_m monster_mummy
python art\team\monsters\batch04\strips2.py monster_mummy attack_2 --n 16 --cols 4 --w 400 --from 52 --to 150
```

Strips: 4 columns at `--w 400` stay under the image viewer's width, so they are read at full size;
8 columns get scaled down to 250 px tiles.

## M4 (Scarecrow): monster_scarecrow, v2 model and animator

The Scarecrow is the third v2 monster: 22 parts, 7,746 triangles, `native\src\play\monster_anim_scarecrow.cpp`.
Collision is unchanged (height 1.6, radius 0.35; the art stands 1.745 m, the hat tip inside the 10 % margin).

### What exists

- **Model** (`scarecrow()` in `build_monsters2.py`; its docstring has the part tree). No legs: a wooden
  pole with a straw tuft tied round its foot; a buffalo-check flannel shirt painted face by face on a
  grid (three cells), placket, two gold buttons, yellow stitch runs at the hem and yoke, a padded hem
  strap and a blue denim patch with big cross stitches; stuffed sleeves on the crossbar with two rope
  ties each and the bar's end showing; a burlap sack head (UVs 2.2x) with a stitched seam down the back,
  cheeks, a small orange nose (kept from v1), stitched brows (one part each), a stitched smirk and two
  hidden mouth shapes (`alt_mouth_o`, `alt_mouth_zig`); button eyes built with `M.eye` (purple on its
  left, orange on its right; the eye part is a dim glow disc `glow_jack_hot` with two glints floating
  above, the pupil part is the button with a glowing thread cross); a crooked witch hat (brim, crown,
  orange band with gold buckle, yellow brim stitches) whose whole cone is ONE soft part `hat_tip`
  curling over to its right and back, with a seam and stitches. Straw everywhere as `M.straw` blades
  (custom normals so `cloth_wave` flaps them): hair (side, under and bangs; rooted lower at the back
  so no root pokes through the tipped-back brim), a two-layer collar, two-layer cuffs, a two-layer
  hem skirt. Ball joints at the shoulders; head, body and hat overlap so no gap opens in any pose.
- **Part tree:** pole > tuft, body > skirt, collar, arm_l/r > cuff_l/r, head > hair, hat > hat_tip,
  eye_l/r > pupil_l/r, brow_l/r, mouth, alt_mouth_o, alt_mouth_zig.
- **Palette:** `hat_violet` (row 13, appended; row 13 now has 3 free cells and row 14 none, so the
  remaining monsters should reuse existing cells wherever they can). Straw is `golden_leaf` / `straw_dark` / `caramel`
  (the pale `straw` / `hay` went olive under the showcase's purple light). Hat underside and cone seam
  `spider_plum`. Material layers: hat `plush` (NOT `felt`: see technique notes), shirt `fabric`,
  head `burlap`, patch `denim`, pole and bar `wood`.
- **Animator** (same shape as the skeleton's: a `Pose` of targets, state layers by `anim::Weight`,
  springs, one solve):
  - *Pogo hop.* A hop cycle on time at a period that follows the speed (0.56 s wandering, 0.43 s
    chasing, 0.27 s charging); the pole's pivot is its foot, posed as an inverted pendulum: planted
    and swept back under the body at exactly the body's speed during contact, swung forward in the
    air. The body slides down the pole on landing (squash) and lags the pole's tilt on a spring; head,
    hat and hat tip each lag the one below. The foot tuft splays on landings.
  - *Arms* hang off the crossbar on a `Dangle` each (flop late on every hop, start and stop), clamped
    so they never pass through the head.
  - *Soft parts:* straw (skirt, collar, cuffs, hair) on `Dangle`s plus a `cloth_wave` ripple whose
    amplitude and rate follow the state (idle breeze, gusts, hops, the charge's 34 rad/s flutter);
    `hat_tip` bent along the hat's own tilted axis: curled tighter (`coil`) or straightened.
  - *Wind-up (1.0 s):* sinks onto the pole, leans back with a 43 Hz tremble, arms drawn back and up,
    brows down, chin down (glares from under the brim), zig-zag gritted mouth, the hat tip coils, eye
    discs and thread flare (glow 0.3 -> 1.1 / 0.2 -> 0.7); in the last fifth it rocks forward, cocked.
  - *Charge:* starts with a leap, root lean spring to 25 degrees (overshoots), chin up, arms streaming
    back, tip straightened behind, straw rippling hard, O mouth.
  - *Dizzy (`in.dizzy`, same duration as before):* the whole stick precesses in a 0.34 rad cone round
    the planted foot, the head leans against the cone past level and rolls round on its neck, the two
    buttons spin opposite ways (settling on a quarter turn afterwards), the hat slips down over one
    eye, worried brows, queasy zig mouth; when it ends it shakes its head and the hat hops back.
  - *Hit:* thrown back, arms flung up, brows up, eyes squeezed, O mouth, the hat hops off the head,
    straw bristles. Flash gains: hit 0.3, telegraph 0.25.
  - *Death (0.6 s):* a start (arms up, hat jumps), then the stuffing goes out: the body slides down
    the pole and spreads (non-uniform scale; the head counter-scales so the hat keeps its shape), arms
    drop, head nods, the pole sinks, the hat left in the air drops onto the heap last, level.
  - *Rise (0.6 s):* only the hat tip shows, wagging; it shoots up out of the ground arms pinned, flings
    the arms out at the top, lands with a squash and pops its eyes open.
  - Idle: a slow breath (the body settles on the pole and puffs up), wind gusts that rock it and blow
    the hat tip straight, and fidgets after 5 s idle (head tilt, a bounce on the spot, a look round).
- **Catalog brief** describes the v2 part tree.

### Added to the kit (`mon_kit2.py`), all backward compatible

- `M.straw(...)`: a chunky lens-section blade along a drooping path, with per-vertex "wave" normals
  that `Rig.finish` turns into custom normals (so the shader's ripple flaps the flat side).
- `M.button(...)`: a sewn button (rim, domed middle, thread cross) as objects at the origin.
- `M.eye(..., eye_extra=, pupil_extra=)`: extra objects for the eye / pupil parts (button eyes,
  floating glints); `pupil=None` / `iris=None` leave the discs out.
- Nothing was added to `monster_anim.h` or `anim_kit.h`.

### Technique notes from the Scarecrow (candidates for art-pipeline)

- **The `felt` layer is a crumpled-foil relief.** On a large surface under a strong key light the base
  painterly highlight runs along every crumple and a dark violet reads as shiny lilac foil (it does in
  BEFORE too). Finer UVs (2.6x) did not fix it; the `plush` layer (fine, soft, low relief) did. Pick
  the layer by its relief, not its name; check it in the face close-up, not the studio preview.
- **Dark, saturated cells for big coloured shapes.** `witch_purple` #6d4aa8 read lilac in the street;
  `hat_violet` #4f2a8e reads as the mock's violet. Thin straw needs MORE saturation, not less: pale
  `straw` #e8c46a turned olive under the purple ambient; `golden_leaf` / `caramel` read golden.
- **Button eyes at night:** a dim glow disc behind the button (glow 0.3) and a glowing thread cross;
  keep the button's own gloss low (0.12): at 0.3 the whole button flashed white facing the light.
- **Hair under a tipped hat:** root the strands lower where the brim sits lower (here the back), or
  the roots poke up through the brim; it shows only in a top-back view.
- **A child of a non-uniformly scaled part inherits the squash.** Counter-scale the child (head) when
  the parent (body) collapses, or a rigid prop on top (the hat) lands as a huge flat disc.
- **An inverted-pendulum pole** (pivot at the foot, planted foot moving back at body speed, top kept
  under a lagging body) gives a pogo hop without any IK or legs.

### Verified for the Scarecrow, and not

- Looked at: the Blender review sheet (the back-top view showed straw roots through the brim; fixed),
  the pose sheet (six extremes: rest, landing, wind-up, charge, dizzy, hit; two views; no gap at
  shoulders, neck or hat), both lighting rows of `art\review\engine\monster_scarecrow.png`, the beat
  sheet beside BEFORE and the mock (`wip_m\strips\sc_compare.png`), the front / back / face stills, and
  400 px frame strips of every beat over four recording rounds (`wip_m\strips\sc_r1_*` .. `sc_r4_*`):
  turntable, face, wander, chase (plus an eye close-up), wind-up and charge from the side
  (attack_1) and the front (attack_2), dizzy from both, hit (with BEFORE underneath:
  `sc_r4_hit_ba.png`), die (dense, 3-frame steps) and rise (dense).
- Log line: `scarecrow=v2(22)`. The `alt_mouth_*` swaps, `rig.ripple` on the straw (first real use of
  `cloth_wave` in a monster) and the counter-scaled head all ran in the recording.
- Regression from the round-4 code (recorded together in one run): monster_skeleton against the
  12:53 pilot-state recording (`ba_strips.py`: 1.0 to 2.9, death puff frames 9.6 / 15.5) and
  monster_slime against BEFORE (1.0 to 3.4, death puff 10.9 / 19.0): unchanged, as in M1 to M3.
  Skeleton beat sheet looked at. `game\override.cfg` absent afterwards.
- Not verified: the idle fidgets (need 5 s of idle; the showcase never gets there), the idle breath
  added in round 4 beyond the rise_idle strip, the distance LOD path, a crowd of scarecrows,
  autopilot fps (not measured for this monster), a real playtest of how well the wind-up telegraphs
  with flash gains of 0.25 / 0.3.

### Known weak points (Scarecrow)

- In the key-lit face close-up the hat is still lighter and bluer than the mock's violet (the toon
  shader's base highlight on a large dark surface); it reads as violet felt, no longer as lilac foil.
- In the showcase's attack_2 a hit lands as the charge ends (attack_2 frames ~1242 to 1258): the head
  is thrown toward the camera and the brim hides the face for about 0.2 s. Capping the forward head
  loll did not change it, so the cause (recoil + body lean + hat hop together) is not pinned down.
- The wind-up's lean back reads well from the side but only modestly from the front; the coiling hat
  tip and the spinning buttons are small at the showcase's gameplay distance.
- Straw does not collide with the body; at extreme arm flops a cuff can pass through the collar straw.
- The death's middle is largely behind the game's own puff and sparkles (as for every monster).
- The small orange nose is kept from v1 (the mock has none).
- 7,746 of 8,000 triangles: room for little more detail.

### Rebuild

```powershell
powershell -NoProfile -File art\team\monsters\batch04\go2.ps1 -Keys scarecrow    # model, gates, verifier, preview
powershell -NoProfile -File art\team\monsters\batch04\build_native.ps1            # animator
tools\showcase.ps1 -Ids monster_scarecrow -Out art\review\showcase\wip_m
python tools\showcase_sheets.py art\review\showcase\wip_m monster_scarecrow
python art\team\monsters\batch04\strips2.py monster_scarecrow attack_2 --n 16 --cols 4 --w 400 --from 150 --to 239 --crop 0.25,0.3,0.75,0.75
```

## M5 (Gremlin): monster_gremlin, v2 model and animator

The Candy-Corn Gremlin is the fourth v2 monster: 25 mesh parts (26 nodes), 7,858 triangles,
`native\src\play\monster_anim_gremlin.cpp`. Collision is unchanged (height 0.8, radius 0.32; the art
stands 0.868 m with the curl, the ears reach 0.39 m from the axis).

### What exists

- **Model** (`gremlin()` in `build_monsters2.py`; its docstring has the part tree). The whole candy is
  ONE part, `head` (a band lathe: yellow below 0.30 m, `candy_orange` to 0.648 m, `linen_light` cream
  above), standing on a meshless hip joint `body`. A first plan split the candy at the yellow/orange
  boundary with a hidden spherical skirt; it cannot work on a candy whose belly narrows below the seam
  (a tilted head pokes out under the belly), so the candy turns, leans and waddles as a whole about
  the hips and only the face, limbs and soft parts move on it.
- **Face:** `M.eye` eyes (cream-white sclera, `iris_brown` iris, `glow_amber_dim` inner ring, ink pupil,
  glints) with an ink lash line + outer flick as `eye_extra` (it blinks with the eye); brows (one part
  each); a button nose with nostrils (`nose`, sniffs); blush with three `D.stitches` scratch marks; two
  fangs on the candy.
- **Mouth = a sliding chin plate.** The dark D-shaped mouth (`mouth_dark`, `licorice_black` throat) is a
  patch painted on the candy (flip=True!), 8.6 cm deep. `jaw` is a thin shell over it, 1.6-4 mm proud,
  whose top edge is the upper lip line at rest (mouth shut) and whose lower edge is a U just below the
  mouth. Its pivot is the centre of the face's vertical curvature (computed in the builder from three
  surface points: about 0.98 m behind the lip), so a small X rotation slides it down the face: 0.031
  rad = a grin showing the fangs, 0.088 = wide open. The plate's outer vertices carry the candy's own
  normals (custom normals through the kit's `M.WAVE_ATTR`), which makes it invisible on the face; with
  its own normals it read as a separate panel. `tongue` (soft, child of the jaw) is scaled by the
  animator with the opening, so it grows out of the mouth and vanishes when it shuts.
- **Limbs:** licorice arms (IK, bent rest pose: the hinge comes from the rest plane) with three curled
  clawed fingers and a thumb; shoulders on the candy at azimuth 56 degrees with sunk caps; licorice legs
  (IK) on big purple paws (three toes, pale claws, shade cell under the paw). Ball joints at every pivot.
- **Ears:** one rigid shell each (orange, `candy_peach` inside), base sunk 3 cm, on a spring in the animator.
- **Tip:** one soft tube (`tip`) that curls over to its left and back, sunk into the candy's top dome,
  with a tiny hidden blob at its pin (the verifier wants a vertex within 6 cm of a soft part's pin).
- **Materials:** candy, ears, nose, jaw and tip `plush` (a fine sugary grain; `vinyl` read as crumpled
  cellophane on the big bands); limbs and paws `vinyl`. No new palette cells (rows 13/14 untouched).
- **Catalog brief** describes the v2 part tree.

### The animator

Same shape as the skeleton's: a `Pose` of targets mixed by `anim::Weight` layers, springs, one solve.
- *Locomotion:* a pattering waddle by distance (stride 0.30-0.42 m per cycle, so about 7 Hz at the
  chase), the candy rolling +-10 degrees side to side and leaning into the run on a springy lean
  (kicked by acceleration: it tips back setting off, forward stopping), ears swept back by speed,
  claws out to the sides and pumping at the chase, panting with the tongue out.
- *Idle:* rubs its hands in front of the belly, hunched, one brow up; every 1.6-3.4 s a burst of three
  sniffs (nose wiggles up, head tips back, eyes half shut, mouth closes).
- *Wind-up (0.6 s):* crouch on bent knees, candy squashed and pulled back, jaw opens wide, brows down,
  claws up and spread, ears flat, tip coiled, trembling, eye ring flares (glow 0.12 -> 1.0); in the last
  fifth it tips forward.
- *Bite (the 0.25 s lunge):* the candy stretches to 1.3 along its own axis (the squash is on the `head`
  part's scale, not the root, so the legs do not stretch), lean 0.36-0.46 rad, claws reaching; the jaw
  snaps shut at 60 % and the stretch collapses to 0.88.
- *Recover:* a head shake with the ears flapping, then it licks its lips (tongue out, swinging,
  curled), then a smug grin.
- *Hit:* the root recoils, the candy squashes, arms flung up, mouth open, tongue out, brows up, ears
  and tip kicked, eyes squeezed; flash gains hit 0.35, telegraph 0.4.
- *Death (0.6 s):* a jolt (mouth and eyes wide, arms up, a hop), then it topples onto its back with the
  legs kicking, tongue lolling, eyes shut, and winks out at the end (mostly behind the game's puff).
- *Rise (0.6 s):* only the cream tip shows, wiggling out of the ground; it bursts up stretched with
  arms and ears flung out and eyes shut, lands squashed and pops its eyes open.

### Added to the kit / shared helpers

- Nothing added to `mon_kit2.py`, `monster_anim.h` or `anim_kit.h`. The custom-normal trick reuses the
  kit's `M.WAVE_ATTR` attribute (set it on any object before `R.part`; `Rig.finish` turns it into
  custom normals, local space handled).

### Technique notes from the Gremlin (candidates for art-pipeline)

- **A plate laid over a surface vanishes if it carries the surface's normals.** Toon bands flip on tiny
  normal differences, so a shell 2 mm proud with its own normals is outlined as a panel. Copy the
  base's normals (nearest point on a high-resolution proxy) onto the plate's outer vertices.
- **A mouth that opens on a one-piece body:** paint the open mouth on the body, cover it with a thin
  chin plate whose top edge is the lip line, and slide the plate down the face by turning it about the
  centre of the face's vertical curvature. Opening grows a thin smile into a D. Make the plate's lower
  edge hug the mouth (a U), not run down to the next colour band, or a grin drags a tab of the upper
  colour over the lower one.
- **Patches from `M.patch` face along du x dv.** With u running to the monster's left and v downward
  the patch faces into the body and is culled: nothing shows, no error. Use `flip=True` (the skeleton's
  mouth did); check the face close-up, not only the Blender sheet (Workbench draws back faces).
- **Saturated orange: pick by the green channel.** `pumpkin` (#f07b1f) went peach under the key light
  (red clips, green climbs toward yellow); `candy_orange` (#ff6f12) stays orange. Candy gloss 0.03.
- **Do not split a candy-corn (or any belly that narrows below the seam) into head and body** with a
  hidden skirt: a tilted head pokes out under the belly. Turn the whole body about the hips instead.
- **Hands that rub in front of a fat belly must stay below the mouth and to the sides,** and claws
  raised in a wind-up go out and back: the showcase's side and three-quarter cameras put a raised
  front hand straight across the face.
- **Showcase face beat and the idle look:** with `look_weight` on while idle the candy turned to the
  (offstage) player and showed the camera its cheek; idle look stays off.

### Verified for the Gremlin, and not

- Looked at: the Blender review, face and pose sheets (six extremes: rest, open + rub hands, wind-up
  crouch, run with ears back, hit, splits + twist; no gap at any joint, the plate a U chin when open);
  both lighting rows of `art\review\engine\monster_gremlin.png`; the beat sheet beside BEFORE and the
  mock (`wip_m\strips\gr_compare.png`); the face still; 400 px strips of every beat over five rounds
  (`wip_m\strips\gr_r1_*` .. `gr_r5_*`, dense die and rise strips `gr_r5_die_dense`, `gr_r5_rise_dense`,
  the wind-up close-up `gr_r4_a1_wind`, final face strip `gr_final_face`).
- Log line: `gremlin=v2(26)` (26 nodes incl. the meshless body).
- Regression from the final DLL (one run): monster_skeleton against the previous recording
  (`ba_strips.py`: 1.4 to 3.6, death puff frames 11.4 / 16.5; the strips show identical poses) and
  monster_slime against BEFORE (0.9 to 2.5, death puff 9.5 / 17.9): unchanged. Skeleton beat sheet
  looked at. `game\override.cfg` absent.
- Not verified: the recover's lip lick and head shake (in both attack beats the monster walks away
  from the camera after the lunge, so only its back is seen); the distance LOD path; a crowd of
  gremlins; autopilot fps; a mid-wind-up hit; the bite's jaw snap was judged at 22-frame spacing and
  in the wind-up close-up, not frame by frame.

### Known weak points (Gremlin)

- The yellow band is smaller than the mock's (the mock's lower body is fuller); the boundary is tied to
  the mouth depth (the chin plate must stay above it).
- The mouth interior reads brown-red under the key light, lighter than the mock's dark red.
- During the wind-up the telegraph flash plus the squash makes the candy look pale and squat from the
  high front camera; the arms raised read partly as "hands up" rather than claws forward.
- At a wide-open bite the U chin plate rides a few centimetres over the yellow band (reads as a dropped
  jaw).
- The tongue is a flat shell lying on the painted mouth; it has no real depth.
- The curl of the tip faces its left and back, so from its right side it reads as a plain cone.
- 7,858 of 8,000 triangles.

### Rebuild

```powershell
powershell -NoProfile -File art\team\monsters\batch04\go2.ps1 -Keys gremlin      # model, gates, verifier, preview
powershell -NoProfile -File art\team\monsters\batch04\build_native.ps1            # animator
tools\showcase.ps1 -Ids monster_gremlin -Out art\review\showcase\wip_m
python tools\showcase_sheets.py art\review\showcase\wip_m monster_gremlin
python art\team\monsters\batch04\strips2.py monster_gremlin attack_2 --n 12 --cols 4 --w 400 --crop 0.2,0.25,0.8,0.8
```

## M6 (Rattlebones head rework): monster_skeleton's head as a bone skull

User feedback (2026-11-15) on the M2 head: "reads more like a robot than a skeleton", too little bone
and skull shape, glowing blue eyes look robotic. The M2 head was a smooth round glossy ball with two
glassy teal lens eyes ringed in white. Only the head, jaw, eyes and their animator code changed; the
body, bandana, thrown bone, poses and timings are as in M2. Compare: `wip_m\strips\sk_head_compare.png`
(BEFORE v1 | rejected M2 | now).

### What changed

- **Skull** (`skeleton()` in `build_monsters2.py`, section "skull (head) and mandible (jaw)"). One blob
  whose warp `skull_shape` is a dome above the equator and, below it, a ring-by-ring table (`LOW_T`,
  `LOW_H`, `LOW_W`, `LOW_F`, `LOW_B`: height, half width, front, back) that narrows below the cheekbones
  to the upper jaw and closes in a flat palate at `T_UP = 0.926`; the front of every ring is fuller than
  an ellipse (a flat-ish face). Bumps by azimuth and height: brow ridges, cheekbones and their arch,
  temples pinched, a hollow under each cheekbone. Sockets carved deeper (0.042) with a friendly
  irregular outline (`socket_outline`: taller than wide, fuller on top, squared toward the nose, top
  leaning out), walls and floor `ink`, a `ivory_shade` rim; the heart nose is a real ink cavity. Sutures
  kept on the back, two hairline cracks. No blush, no painted brows.
- **Teeth**: eight blocky upper teeth hung from the palate edge (found by ray casts, `edge(a)`) and eight
  lower ones on the mandible, `linen_light` (never shaded), with dark gaps: an ink `throat` blob behind
  them (its back faces `ivory_deep`) and the palate's front underside painted ink.
- **Jaw**: a separate mandible (`M.shell` horseshoe under the palate edge, an overbite, a small chin
  jutting forward and down, rami rising into the skull) with its inside `ivory_deep`; pivot at the hinge
  `G(0, 0.935, 0.02)`. The animator's jaw values are unchanged.
- **Material**: skull and mandible on the `stucco` layer, UVs x3 (fine pits and strokes: chalky bone);
  shade cells only in AO creases (`R.shade(..., ao_cut=0.5, down=-1.5, parts=["head", "jaw"])`: a
  downward-facing rule gave hard brown rectangles under the cheekbones).
- **Eyes**: `M.socket_eye` (new, see below): an ink bowl lining each socket (`eye_l` / `eye_r`) and a small
  rounded `glow_magic_teal` light (`pupil_l` / `pupil_r`) floating a third of the way down inside it.
- **Animator** (`monster_anim_skeleton.cpp`): `pose_eyes()` runs right after `face_.update`. MonsterFace
  still turns the head and supplies `closed` (blinks, hit squeeze, `shut` for death and rise), but its
  glow is off (`glow_rest = -1`) and its eye poses are overwritten: the socket is posed at rest (it is
  bone; no blink squash), the light slides inside the socket toward the player (`pupil_offset`, 0.22
  of the socket's reach, damped), scales uniformly with `(1 - closed) * (1 + 0.45 flare + 0.12 laugh) *
  (1 - 0.5 ouch)` (so a blink or death winks it out; a Y-scale would leave a bright line in the dark),
  is knocked askew by a hit, and glows `0.35 + 1.15 flare` (times `1 - 0.55 ouch`). `flare` follows the
  wind-up and is held through the throw. Gloss: head and jaw -1.6, sockets -2, lights 0.
- **Shader** (`toon.gdshader`, additive): `gloss < 0` scales the soft painterly highlight by `1 + gloss`
  (-1 = none: matte bone, felt, sockets). Lane P then added: `gloss <= -2` also removes the environment
  sheen (`SPECULAR`). Positive gloss is unchanged.

### New kit helper: `M.socket_eye`

`M.socket_eye(R, "eye_l", "pupil_l", centre, normal, w, h, depth, parent="head", roll=..., back="ink",
light="glow_magic_teal", light_r=0.42, light_tall=1.2, light_back=0.35, look=(x, y))`: for any monster whose
eye is a light inside a dark hole (skulls, hollow pumpkins, sheet ghosts, a lantern face). The socket
part is a `back`-coloured bowl w x h, `depth` deep behind `centre` (put `centre` about a centimetre inside
the carved opening, the bowl a few mm inside the carve's tapered walls); the light part is its child.
It keeps the MonsterFace eye contract (origin = centre, +Z out, +Y up, light child of socket), so the
verifier and `MonsterFace` blinking work; pose the light yourself as `pose_eyes` does. Keep the light's
glow low (0.3 to 0.5): `glow_magic_teal` clips to white at 1, and the bloom gives the soft edge.
`M.eye` is unchanged.

### Audit of the other finished monsters (decision 14)

- **Mummy**: the big eye read as a camera lens (a glossy dark dome with a sweeping highlight, and a
  bright `glow_amber` inner ring inside a dim amber iris: a bullseye). Fixed with the smallest changes:
  eye and squint dome gloss 0.9 -> -1 (`monster_anim_mummy.cpp`), the inner iris ring recoloured
  `glow_amber` -> `glow_amber_dim` for both eyes (`mummy()`, geometry unchanged; model rebuilt, 7,982
  tris, PASS). Now: amber iris, ink pupil, the two authored glints.
- **Scarecrow**: button eyes read as buttons (kept). The violet hat read as shiny vinyl (a broad white
  sheen along the crown): `hat_` and `hat_tip_` gloss 0.03 -> -1 (`monster_anim_scarecrow.cpp`); it now
  reads as felt. Straw left as it is (a matter of taste; it is a little glossy in the close-up).
- **Gremlin**: no change. Living cartoon eyes (white sclera, brown iris, pupil, glints) and a sugary
  candy surface; candy may shine.

### Verified, and not

- Looked at: the Blender face sheet and a new straight front / side / back / low three-quarter skull
  sheet (`art\review\sheets\monster_skeleton_skull.png`, closeup with views) in every round; both
  lighting rows of `art\review\engine\monster_skeleton.png` (the preview runs no animator, so it shows
  glossy sockets and white lights there); the pose sheet (jaw open in two poses, no gap at the neck or
  hinge); three recording rounds of the face, front and back stills and beat sheets beside BEFORE and
  the mock; strips `wip_m\strips\sk_head_r2_attack2`, `sk_head_r2_hit`, `sk_head_r2_face`,
  `sk_head_r3_face`, `sk_head_r3_attack1` (wind-up flare, laugh with the jaw open, hit squeeze, blink).
- Regression from the final DLL: monster_slime against the pre-rework recording (`ba_strips.py`: 0.8 to
  3.0, death puff 8.3 / 17.1): unchanged. Mummy and scarecrow face stills after their audit fixes.
- `game\override.cfg` was present at the start of this session while another lane's recording was
  running; it was absent after every one of my runs.
- Not verified: the distance LOD path for the lights; a real night street at gameplay camera height
  beyond the showcase beats; a crowd of skeletons.

### Known weak points

- From directly behind at a low camera (the turntable's back still) you see under the cranium into the
  mouth: the brown throat and the backs of the teeth. The gameplay camera is higher and does not show it.
- The cranium still overhangs the lower face a little at the sides (a ledge under the cheekbones in
  the close-up three-quarter view).
- In the close-up the light's centre clips nearly white with a teal halo; at gameplay distance it reads
  teal. The look slide is small (0.22 of the socket reach) so the rim does not hide the light.
- The body bones keep their M2 smooth toy sheen (gloss 0.14, no texture); the skull is matte stucco.
  The user did not criticise the body, so it was left alone.
- 7,954 of 8,000 triangles.

### Polish round (2026-11-15): teeth, one bone material, the back of the mouth

The skull's shape, sockets and teal lights are unchanged (the user has seen them).
- **Teeth** (`rounded_tooth` in `skeleton()`): 8 upper, 8 lower, each a 4-sided lathe turned 45 degrees (a square
  section) with flattened pole tips: a rounded block in 16 triangles. Smaller (2.5 / 2.2 cm wide upper, 2.3 / 2.0
  lower, was 3.2 / 2.9), uneven (width, height, height offset and a 4 degree lean vary per tooth from fixed sines),
  `linen` on the `stucco` layer (ivory: `linen_light` / `bone_shade` on the flat layer read white), the root cap
  `ivory_shade` (the gum shadow at the lower row's base; the upper roots sit 7 mm inside the palate, where seen from
  above they read as brown cube tops: the AO under the palate is their shadow). The upper row hangs 4 mm in front of
  the lower row's tips.
- **Back of the mouth closed**: `mouthback` (head), a curved wall between the rami from the palate down to 0.85 m,
  ink toward the mouth, `ivory_deep` behind; `mouthfloor` (jaw), an ink floor inside the horseshoe meeting it. The
  throat blob is gone. From low behind you now see a dark closed surface, and an open jaw shows a dark mouth (with
  only the wall the laugh showed the bandana through the gap).
- **One bone material**: every body bone (all parts but head, jaw, eyes, bandana) moved from the flat layer to
  `stucco` at the skull's UV scale (x3) in the builder, and the animator gives all bone the skull's gloss -1.6 (was
  0.14 on the body). Geometry unchanged, bandana unchanged.
- Triangles: 7,986 of 8,000 (lights 7 x 4 instead of 8 x 5, the coronal suture 13 points, throat removed).
- Verified: skull sheet (front, side, back, low three-quarter), pose sheet (jaw open in two poses: dark mouth),
  face and back stills of the last two recordings, beat sheet. Weak points: the teeth are still small blocks up
  close (no triangles left for six-sided ones); the wall's back is a flat dark panel under the skull from directly
  behind.

### Rebuild

```powershell
powershell -NoProfile -File art\team\monsters\batch04\go2.ps1 -Keys skeleton      # model, gates, verifier, preview
powershell -NoProfile -File art\team\monsters\batch04\build_native.ps1            # animator
tools\showcase.ps1 -Ids monster_skeleton -Out art\review\showcase\wip_m
python tools\showcase_sheets.py art\review\showcase\wip_m monster_skeleton
python art\team\monsters\batch04\strips2.py monster_skeleton attack_2 --n 12 --cols 4 --w 300 --crop 0.25,0.2,0.75,0.7
```

## M7 (Toadstool Imp): monster_toadstool, v2 model and animator

The Toadstool Imp is the fifth v2 monster: 20 mesh parts (21 nodes), 7,399 triangles,
`native\src\play\monster_anim_toadstool.cpp`. Collision is unchanged (height 1.0, radius 0.36; the art
stands 1.079 m with the sprout; the cap reaches 0.46 m from the axis, as the v1 cap did).
Compare: `wip_m\strips\ts_compare.png` (BEFORE beats over AFTER beats, the mock, both face stills).

### What exists

- **Model** (`toadstool()` in `build_monsters2.py`; its docstring has the part tree). The stalk is ONE
  part, `head` (a lathe bean, `ivory` on the `plush` layer, `ivory_shade` in AO creases and undersides)
  standing on a meshless hip joint `body`; it runs up to 0.78 m, inside the closed cap, so the cap can
  tip, lift, drop and inflate without a gap. Face on the stalk: `M.eye` eyes (trim_white sclera,
  `iris_brown` + `glow_amber_dim` inner ring, ink pupil, glints, an ink lash line with an outer flick as
  `eye_extra`, as on the gremlin), thin `choco_dark` brows (one part each), a thin dark lip line with
  dimples (on the head), the open mouth as its own part, blush pads as their own parts, two hidden
  shut-eye chevrons (`alt_squint_l/r`). Stubby arms (`arm_x` > `hand_x`, mitten + thumb, ball joints,
  sunk shoulder caps). Round feet (`foot_x`, children of `body`, tucked under the belly, toes poking out).
- **Cap** (`cap`, pivot at the stalk top 0.625 m): a band lathe (rolled rim, `toad_red` top,
  `ivory_shade` underside) tipped back 12 degrees IN THE GEOMETRY (`MCAP`), 12 raised spot pads
  (`M.pad`, `bone` with a `bone_shade` shoulder) and 36 gill fins (`bone_tan`) hanging under it, each a
  wedge whose depth swells mid-run. `sprout` (soft, child of the cap): stem and both leaves (cupped
  `M.shell`s with a `vine` midrib) in ONE mesh pinned 2 cm inside the crown; its soft axis is the cap's
  tipped up axis (0, cos 12, -sin 12).
- **Mouth:** `mouth` is the open mouth (cavity `mouth_dark`, a row of teeth, the tongue), a patch laid
  on the stalk 1.2 mm proud, pivot on the lip line, rest rotation along the face normal. The animator
  scales it in Y about the lip line (0 = shut onto the painted smile, 1 = wide open; X narrows it to an
  O) and hides it below 0.03. The stalk is nearly straight vertically, so a Y scale keeps it on the surface.
- **Cheeks:** `cheek_x` is a pink blob sunk 2.4 cm into the face (a blush oval at rest); scaling it
  (1.7, 1.55, 2.25) puffs the cheek out.
- **Palette:** one new cell, `toad_red #a5181b` (row 13; `leaf_crimson` went salmon pink in the engine).
  Row 13 now has 2 free cells. Everything else reuses cells (ivory, bone, bone_tan, apple_green, vine,
  stem_green, cheek_pink, mouth_dark, tongue, iris_brown, glow_amber_dim, choco_dark).
- **Catalog brief** describes the v2 part tree (published).

### The animator

Same shape as the others: a `Pose` mixed from locomotion and state layers by `anim::Weight`, springs,
one solve. The body joint stays at rest (feet are posed in model space); the stalk carries look,
waddle, lean and squash (`squash_stretch` on the head part).
- *Cap:* authored pose (Euler about its pivot, offset, scale) times a heavy `Dangle` (1.7 Hz, zeta 0.42,
  limit 0.22: under the step rate, so the patter never pumps it) plus a vertical bounce spring kicked by
  footfalls, landings and the puff. On the run it rides back against the forward lean so the face stays
  in view.
- *Walk:* a patter by distance (0.24-0.32 m per cycle), the stalk rolling onto each planted foot and
  bobbing, the little arms swinging (walk) or pumping out and back (chase), open-mouthed with a frown
  on the chase. Arms are FK: `Basis(Z, side * raise) * Basis(X, -fwd)` (swing in the rest frame, then
  raise, so + fwd stays forward at any height), on a `Spring3` each; the hand bends by how far the arm
  still has to go (overlap).
- *Idle:* sway and breath; every 2.2-4.2 s a peek: the cap tips back 0.34 rad and lifts, one hand up at
  the brim, eyes wide, an O mouth. Idle look is off (it faces the camera in the face beat).
- *Wind-up (1.1 s):* crouch and glare with the eyes flaring (glow 0.12 -> 1.0, own driver; MonsterFace
  glow off), then it breathes in: cap inflates to 1.3 (0.3 / 0.21 / 0.3), cap lifts and tips back, cheeks
  puff, eyes squeeze shut into chevrons, arms lift out, a 44 Hz tremble; it rocks back at the end.
- *Puff (0.3 s):* the cap slams down and forward, deflating past rest (0.9), stalk squashes to 0.84, eyes
  pop wide and flaring, O mouth, arms flung back; kicks on the cap, sprout and squash springs.
- *Recover:* the cap wobbles round (3 Hz, decaying), the stalk jiggles, dazed half-lidded then pleased;
  fades out after 1.1-1.7 s while Recover continues.
- *Hit (visual 0.95 s, Hurt stays 0.28 s):* the cap drops 5 cm and tips 0.38 rad over its eyes (chevrons),
  arms flung out, then both hands come up in front pushing the brim back up, the cap pops up past rest
  and wobbles. Restarted by each hit. Flash gains: hit 0.35, telegraph 0.4.
- *Death (0.6 s):* a jolt (eyes wide, O mouth, arms up), then the stalk and feet duck into the ground
  (eyes shut) while the cap pops up, turns onto its rim and spins down like a coin, faster as it falls
  flat (an Euler disc), resting on the ground (`death_cap`: the pivot height is computed so the rim
  touches the floor at every tilt). The root shrinks in the last 14 %.
- *Rise (0.6 s):* only the sprout and crown show, wiggling; it bursts up stretched, arms flung up, eyes
  shut, the cap lagging back; lands squashed, the cap bonks down, eyes pop open.

### Added to the kit / shared helpers

- `M.pad(name, tree, centre, normal, r, h, segs, rings, heights, color, edge_color, mat, sink)`: a raised
  pad that hugs a curved surface (ray cast onto a hi-res proxy, flat top, rounded shoulder, edge sunk
  `sink` under the surface so faceting never shows a gap). Mushroom spots, patches, warts, scales.
- Nothing added to `monster_anim.h` or `anim_kit.h`.

### Technique notes from the Toadstool (candidates for art-pipeline)

- **A heavy hat or cap on a spring: keep its pendulum under the step rate.** At 2.8 Hz the cap
  resonated with a 3 Hz patter and buzzed; at 1.7 Hz with zeta 0.42 it lags and bobbles.
- **A part that must tilt and drop over a body without a gap: make the body run up inside a closed
  shell** (here the stalk 15 cm into the cap). No ball joint or skirt is needed.
- **Build a signature tilt into the geometry, not the rest rotation,** so soft children (the sprout) keep
  no rest rotation; give their soft axis and their `Dangle` rest direction the tilted axis.
- **A mouth that opens on a one-piece body:** a lip line painted on the body plus a separate cavity part
  scaled in Y about the lip line. Works when the surface is nearly straight in the vertical direction
  (a stalk, a sack); on a sphere use the gremlin's sliding chin plate.
- **Shut eyes as alt_ chevrons** (`> <`) shown while the eye part is scaled to 0.02: reads at gameplay
  distance far better than a Y-squashed eye.
- **Red under the showcase key light:** a red with green/blue above ~0x30 goes salmon pink; `#a5181b`
  stays red. Cap gloss -0.55 (the default painterly highlight clipped to white on the big dome).
- **FK arms: swing first, then raise** (`Rz(raise) * Rx(-fwd)`); the other order turns "forward"
  into "backward" once the arm is above shoulder height.

### Verified for the Toadstool, and not

- Looked at: the Blender review, face and pose sheets (six extremes: rest, peek + wave, wind-up, slam,
  hit with the cap over the eyes, waddle; two views; no gap at the cap, shoulders, wrists or feet); both
  lighting rows of `art\review\engine\monster_toadstool.png` (the preview runs no animator, so the cap
  shows the default highlight and the eye ring glows there); beat sheets beside BEFORE and the mock
  (`ts_compare.png`); face still; 400 px strips of every beat over four rounds (`wip_m\strips\ts_r1_*`,
  `ts_r2_*`, `ts_r3_*`, `ts_final_*`), dense strips of the puff (`ts_r2_a2_dense`), walk, death and rise.
- Log line: `toadstool=v2(21)`.
- Regression from the final DLL (toadstool + skeleton + slime recorded together in one run):
  monster_skeleton against the M6 recording (`ba_strips.py`: 1.0 to 2.8, death puff frames 11.4 / 17.0)
  and monster_slime against BEFORE (0.8 to 3.1, death puff 9.6 / 18.1): unchanged, as in M1 to M6.
  Skeleton beat sheet looked at. `verify_rig2.py` PASS for all five v2 monsters (the kit change is
  append-only; no other monster was rebuilt). `game\override.cfg` absent afterwards.
- Not verified: the distance LOD path; a crowd of toadstools; autopilot fps; a hit in the middle of a
  wind-up; the peek's hand never touches the brim (it is a wave beside it).

### Known weak points (Toadstool)

- From the showcase's high front camera, the hit's dropped cap hides the whole face and most of the
  stalk for about 0.3 s; the pushing hands read best from the side.
- The waddle's stalk roll swings the cap sideways; from a side camera that reads as the cap nodding
  forward over the eyes on some steps.
- The puff itself is mostly behind the game's own spore cloud (as BEFORE); the wind-up carries the read.
- The death's coin spin is partly behind the game's puff and sparkles.
- Blush is a flat pink oval (no gradient); the leaves are smooth (no veins beyond the midrib); the
  stalk has no fibre detail beyond the `plush` grain.
- 7,399 of 8,000 triangles.

### Rebuild

```powershell
powershell -NoProfile -File art\team\monsters\batch04\go2.ps1 -Keys toadstool      # model, gates, verifier, preview
powershell -NoProfile -File art\team\monsters\batch04\build_native.ps1             # animator
tools\showcase.ps1 -Ids monster_toadstool -Out art\review\showcase\wip_m
python tools\showcase_sheets.py art\review\showcase\wip_m monster_toadstool
python art\team\monsters\batch04\strips2.py monster_toadstool attack_2 --n 16 --cols 4 --w 400 --from 100 --to 175 --crop 0.2,0.3,0.8,0.8
```
## M8 (Ghost): monster_ghost, v2 model and animator

The Ghost is the sixth v2 monster: 16 mesh parts (17 nodes), 7,426 triangles,
`native\src\play\monster_anim_ghost.cpp`. Collision is unchanged (height 1.3, radius 0.4; the art stands
1.22 m; the dome is 0.375 m in radius, the scalloped hem flares to about 0.5 m and the arms reach 0.61 m).
It floats (gameplay hover 0.35 m +- 0.25); the model's hem is on y = 0 at rest. Compare:
`wip_m\strips\gh_compare.png` (BEFORE beats over AFTER beats, the mock, both face stills).

### What exists

- **Model** (`ghost()` in `build_monsters2.py`; its docstring has the part tree). The whole sheet is ONE
  part, `head`, built as one closed bmesh by `sheet()`: a dome (an ellipse, 9 rings), 6 middle rows, 9
  skirt rows down to a hem with eight deep round scallops (cusps between them), a rolled hem (3 rows of
  a half-roll, 2.4 cm thick), an inside wall up to 0.5 m and a cap. The columns are NOT uniform: ten per
  lobe, packed toward the cusps (`U_COLS`), so the scallops stay round and the cusps sharp at 80
  columns. Soft vertical folds swell toward the hem (`fold`). Hem stitches (`D.stitches`, 7 per lobe,
  spaced by arc length along each scallop, 3.4 cm up the sheet) and the ink lip line are merged into the
  sheet. A tiny hidden blob sits at the pin (the verifier wants a vertex within 6 cm of a soft pin).
- **Pin.** `body` is a meshless joint at 0.70 m and `head` (the sheet) pivots there: everything above
  it (face, shoulders) is rigid; the shader bends the sheet below it (`rig.soft(head, (0,-1,0), 0.70,
  0.04)`). The ripple (`cloth_wave`, lobes -3 = radial push, so the inside wall moves with the outside)
  applies below local y -0.3.
- **Face:** `M.eye` eyes with an `ink` dome, a `linen_dark` (very dark brown) iris, an ink pupil and
  `glow_white` glints (so the wind-up flare makes the glints blaze while the dark of the eye stays
  dark); short ink brows (one part each); the open D mouth (`mouth`, cavity `mouth_dark` + `tongue`,
  scaled in Y about the lip line, as on the toadstool); blush (`cheek_x`: a `cheek_pink` blob sunk so
  it is almost flat, with three `candy_berry` strokes); hidden `alt_squint_x` (> <) and `alt_happy_x`
  (^ ^) eyes.
- **Arms:** `arm_x` is ONE soft part each: a broad paddle (a tube flattened to half its depth) that
  tapers to a mitten tip with a thumb blob, pinned at a 6.6 cm ball sunk 3.4 cm into the sheet at azimuth
  62 degrees, 0.755 m. The mesh runs along local +-X, so the animator's X scale stretches the sleeve.
- **Colours:** `silver` (lit, a cool white), `ghost_cloth` (shade: folds, undersides, the hem roll),
  `ghost_deep` (inside cap) - existing cells, NO new palette cells (row 13 still has 2 free). Material
  layer `plush` everywhere on the sheet and arms. `R.shade({silver: ghost_cloth}, ao_cut 0.6, down -0.45)`.
- **Catalog brief** describes the v2 part tree (published).

### The animator

Same shape as the others (a `Pose` mixed from locomotion and state layers by `anim::Weight`, springs, one
solve), plus two ghost-specific pieces:
- *Hem.* The sheet's `soft_bend` = a trail opposite the smoothed velocity (`cross(down, -v) * 0.17`,
  capped at 0.32 rad) + a `Dangle` (1.5 Hz, zeta 0.32, limit 0.3) on a frame built from the facing and the
  head's model transform (not `world_xf`, so the spawn's zero root scale never reaches it) + the pose's
  own `hem`. Converted into the head's frame (it carries the look yaw). Ripple amplitude and rate follow
  the state (idle 0.011 m / 2.4 rad/s, chase, wind-up, BOO 0.03 / 12, hit 0.035 / 14).
- *Arms.* FK: `Basis(Y, -side*fwd) * Basis(Z, side*raise) * Basis(X, side*twist)` (elevation, then
  azimuth, then the roll about the arm), on a `Spring3`, with X-scale `stretch` and Y/Z `thick`. The
  paddle's soft bend = authored curl (`curl_up`, `curl_fwd`) + the spring's lag (overlap) + a `Dangle`.
  `aim(side, target)` gives (raise, fwd, stretch) that put the mitten at a head-space point: used for
  the hands over the eyes and the hand over the mouth.
- *Locomotion:* idle float (bob, the old 1.7 Hz sway, arms drifting out of step); wander: a lazy paddle
  with a little pitch; chase: arms out in front at 0.95 rad, mittens wiggling, 'oooo' mouth, sly frown.
  Moving up stretches it, sinking squashes it (the hover bob). Idle look is off.
- *Fidget (idle > 5 s):* a little wave with one mitten, happy eyes and a bounce (a full twirl was tried
  first: it turned the face away in the showcase's face beat).
- *Wind-up (0.75 s, peek-a-boo):* shrinks back and hunches, both mittens over the eyes (aimed 3 cm above
  and in front of each eye, curled in, 1.3x thick), cheeks puffed, shy wiggle; at 0.62-0.9 one mitten
  slides down and that eye peeks out, glints flaring (glow 0.35 -> 1.6); then the hands lift, cocked.
- *BOO (the 0.25 s lunge):* bursts tall (1.2), leans in 0.28, arms flung wide and high, mouth wide,
  eyes 1.32, glints blazing, the hem whipped back.
- *Recover (giggle):* three bounces with squash, happy arcs, laughing mouth, one mitten over its mouth,
  the other hugging its tummy, rocking; fades out by 1.8 s.
- *Hit (visual 0.75 s):* squash, a spin away from the blow, > < eyes, open mouth, arms flung up, hem
  kicked, ripple spike, wobble back. Flash gains: hit 0.3, telegraph 0.35.
- *Death (0.6 s):* a jolt (tall, eyes huge, O mouth, arms up), then it deflates into a flat puddle on the
  ground (body scale (1.5, 0.07, 1.5), the hem kept on the ground by `boff.y -= PIN_Y * (1 - sy)`, the
  root dropped by the hover height from `in.ground`); the eyes stay hanging in the air (`linger_eyes`),
  glance at each other, blink, then fall onto the puddle and shrink away.
- *Rise (0.6 s):* a flat sheet on the ground, rippling, eyes shut in happy arcs; it unfolds upward with
  an overshoot, flings its arms, lifts off to its hover height and pops its eyes open.

### Added to the kit / shared helpers

- Nothing added to `mon_kit2.py`, `monster_anim.h` or `anim_kit.h`.

### Technique notes from the Ghost (candidates for art-pipeline)

- **A flat paddle pointed at a target shows it its edge.** To cover an eye with a mitten, aim the arm
  at a point just above and in front of the eye and curl the soft paddle in, and thicken it a little;
  aiming straight at the eye left the eye visible from the front. Print the mitten's model position
  against the target once (a debug line in the animator) instead of guessing from strips.
- **White on white needs a silhouette or an absence to read.** A white mitten over a white face only
  reads because the dark eye disappears behind it; plan poses around hiding or showing a dark feature.
- **Ink eyes: gloss <= -2 on the dome and the pupil, glints in a glow cell.** With a positive gloss the
  dark dome took a chrome rim from the environment sheen (read as a camera lens); matte ink plus
  `glow_white` glints reads as the mock's glossy eye and gives the wind-up a flare that never eats the
  pupil.
- **Non-uniform columns for a scalloped hem.** Pack the columns toward the cusps (ten per lobe) instead
  of raising the count everywhere: round lobes and sharp cusps for the price of 80 columns.
- **Squash a floating body about its middle, and flatten it about the same pivot while sliding it down
  by `pin * (1 - sy)`,** so a deflate or an unfold keeps the hem on the ground.
- **Lingering parts during a deflate:** set the part's local transform to `parent_model^-1 * wanted`
  (a non-orthonormal Basis through `rig.pose(rot = rest^-1 * local)`); `Node3D` keeps the shear, so the
  eyes stay round in the air while their parent is squashed to 7 % under them.
- **Showcase idle beats are already past 5 s of idle** (turntable + face): an idle fidget that turns the
  body away will be in the face beat. Keep fidgets facing the camera.
- `silver` (#c9ced6) is the cool white that keeps its form under the key light; `ghost_sheet` (#e4e2ec)
  would clip.

### Verified for the Ghost, and not

- Looked at: the Blender review, face and pose sheets (six extremes, two views, no gap at the shoulders);
  both lighting rows of `art\review\engine\monster_ghost.png` (the preview runs no animator, so the eyes
  show a brown sheen there and are ink in the game); six recordings with 400 px strips of every beat
  (`gh_r1_*` .. `gh_r6_*`, `gh_final_*`), dense close strips of the wind-up from the front and the side
  over four rounds (`gh_r2_a2_wind` .. `gh_r5_a2_wind`, `gh_r*_a1_wind`), dense death and rise strips
  (`gh_final_die_dense`, `gh_final_rise_dense`), the beat sheet beside BEFORE and the mock.
- Log line: `ghost=v2(17)`.
- Regression from the final DLL (ghost + skeleton + slime recorded together): monster_skeleton against the
  previous (M7) recording (`ba_strips.py`: 0.9 to 2.5, death puff frames 11.1 / 20.4) and monster_slime
  against BEFORE (0.8 to 3.2, death puff 8.4 / 18.7): unchanged. The ghost was re-recorded once more
  after a model-only change (wider hem flare); the DLL did not change. `game\override.cfg` absent.
- Not verified: the distance LOD path; a crowd of ghosts; autopilot fps; a hit in the middle of a
  wind-up; the wave fidget beyond the face beat; the hem Dangle on sharp turns at speed (the showcase
  turns slowly); a spawn at a real nest (the rise is laid on `in.ground`, as for the others).

### Known weak points (Ghost)

- The pose sheet's peek-a-boo column is only rotations (the sheet cannot show the stretch and curl), so
  it does not match the game's wind-up.
- In the peek-a-boo the mittens cover the eyes for about 0.2 s before the peek; from the high front
  camera the arms cross the cheeks on the way up, and from the side the stretched arm reads a little like
  a reach across the face.
- The BOO's lean plus hem whip makes the side view lie almost diagonal for a few frames; tuned down once.
- The hit's open mouth keeps its smile shape (no true 'O' shape; the mouth part only scales).
- Blush is a flat pink oval with three small strokes, less soft than the mock's gradient blush.
- The sheet has no translucency or inner glow (opaque, as the lead asked); in the night street it is a
  bright white shape with the soft plush grain.
- 7,426 of 8,000 triangles.

### Rebuild

```powershell
powershell -NoProfile -File art\team\monsters\batch04\go2.ps1 -Keys ghost        # model, gates, verifier, preview
powershell -NoProfile -File art\team\monsters\batch04\build_native.ps1           # animator
tools\showcase.ps1 -Ids monster_ghost -Out art\review\showcase\wip_m
python tools\showcase_sheets.py art\review\showcase\wip_m monster_ghost
python art\team\monsters\batch04\strips2.py monster_ghost attack_2 --n 8 --cols 4 --w 400 --from 70 --to 130 --crop 0.3,0.3,0.7,0.65
```

## M9 (Goo Slime): monster_slime, v2 model and animator

The Goo Slime is the seventh v2 monster: 21 mesh parts (22 nodes), 7,414 triangles,
`native\src\play\monster_anim_slime.cpp`. Collision is unchanged (height 0.7, radius 0.42; the art stands
0.759 m with the curl, the drips reach 0.51 m from the axis, as the v1 lobes did). It hops (gameplay
`hops`), throws goo (THROW), and stays opaque. Compare: `wip_m\strips\sl_compare.png` (BEFORE beats over
AFTER beats, the mock, both face stills).

### What exists

- **Model** (`slime()` in `build_monsters2.py`; its docstring has the part tree). The jelly is ONE lathe,
  `head` (48 columns, 23 rows; profile table `PY` / `PR` through a monotone cubic: a wide rounded foot,
  concave flanks rising to a peak), pivot ON THE GROUND, so every squash, stretch, spread and flatten is
  about the foot and the skirt and face ride on it. `body` is a meshless joint on the ground.
  - `skirt` (child of head, pivot on the ground): a ring of nine rounded drips of different sizes, one
    closed tube whose cross-section at each azimuth leaves the body tangent to it (a cubic Bezier from the
    body's own slope), swells out to the drip's outermost point and rounds under onto the ground. Its top
    edge lies 1.6 mm PROUD of the body (`ra = rad(ya) + 0.0016`): starting it inside the body left a
    zig-zag seam where the two facetings cross. Columns packed toward the cusps (as the ghost's hem).
    Between drips it dips under the body, so the drips read as separate bulbs.
  - `curl` (soft, child of head, pinned at 0.58 m inside the peak): one tube that continues the peak and
    hooks over to its left (+X) into a ball, with a glint; a tiny blob at the pin for the verifier.
  - Jelly look (opaque): paler inner swirls and two highlight streaks are `M.patch` decals 1.2 mm proud
    (`swirl()`), raised bubbles are spheres sunk to 12 % with a glint (`bubble()`): four small ones in the
    head mesh, four big ones as parts `bubble_a..d` that the animator slides.
  - Face: `M.eye` eyes (trim_white sclera, iris_brown iris, big ink pupil, glow_white glints) with a
    closed `choco_dark` outline ring as `eye_extra` laid where the dome meets the jelly; thick short brows;
    raised paler cheeks (`cheek_x`, sunk blobs that puff); the open D mouth (`mouth`: cavity, upper teeth
    row, tongue; scaled in Y about the lip line) under a `choco_dark` lip line; hidden `alt_squint_x`
    (> <) and `alt_happy_x` (^ ^). `socket_muzzle` 5 cm in front of the mouth.
  - `alt_goo`: never shown; a lumpy `glow_slime` glob with `goo_lime` lumps and a glint, hidden inside the
    body. `thrown_part` hands it to Combat as the goo projectile's look (the old look was a 0.4 m flat
    glowing sphere: the "big green disc" in BEFORE's attack tiles).
- **Palette:** one new cell, `goo_lime #6fac1e` (row 13; row 13 now has 1 free cell). `apple_green` read
  milky pale lime in the face close-up under the key light, rim and sheen. Swirls, drip bands and cheeks
  `apple_green`, bubbles `slime_light`, skirt crease shade `gourd_green`. Material layer `flat` everywhere
  (smooth jelly; every textured layer would read as a skin).
- **Catalog brief** describes the v2 part tree (published).

### The animator

Same shape as the others: a `Pose` mixed from locomotion and state layers by `anim::Weight`, then springs,
then one solve. Specific to the slime:
- *The jelly is the head's transform:* `rig.pose(head, shear_basis(shear) * look, 0, scale)` with scale =
  `squash_stretch(sy) * size`, times the pose's `flat` (y) and `spread` (x, z). `sy` runs through a ringing
  spring (5.5 Hz, zeta 0.26: the "damped 6 Hz wobble"), `shear` (the top's slide per metre of height)
  through a 3.4 Hz, zeta 0.2 spring kicked by horizontal acceleration, take-offs, landings, hits and the
  lob. A shear is a non-orthonormal Basis (column Y = (shx, 1, shz)); `Node3D` keeps it and every child
  (face, bubbles, curl, skirt) follows the wobble exactly.
- *Hops* are read from the body: take-off = airborne for 0.04 s with vy > 1.5; landing = back on the floor
  with fall speed > 1 m/s. Crouch while `hop_in` runs down its last 0.16 s (only while `hop_in > 0`: at 0
  it is not going to hop), stretch rising (1 + 0.05 vy), rounder at the top, stretched falling, landing
  kick `-2.1 * fall speed` on the squash spring, a forward shear kick (the top carries on), a ripple
  pulse round the drips (`cloth_wave`, 5 lobes, radial, below local 0.2 m), drips spreading, face sliding
  down 1-2 cm and back on its own spring, the bubbles thrown down, the curl `Dangle` kicked.
- *Face:* MonsterFace for blinks, the pupils and the head look (max yaw 0.5, pitch 0.12; idle look off);
  the eyes are re-posed afterwards with the face-slide offset and the same blink scale. Glint glow 0.35
  rest, 1.6 at the end of the wind-up and the lob.
- *Wind-up (0.8 s):* flattens wide (sy 0.82 -> 0.77) and leans back (shear -0.09), cheeks puffed full of
  goo, mouth clamped shut, brows down, eyes narrowed and flaring, the curl cocked back, trembling in the
  last 30 %; the mouth opens into an O in the last 12 %.
- *Lob (0.3 s):* the squash spring is kicked up and the target is 1.3: it rebounds tall, leaning into the
  throw (shear +0.1), mouth an O, eyes wide and blazing, curl whipped forward; the goo leaves from
  `socket_muzzle` (`muzzle()`), so it comes out of the mouth.
- *Recover (the 3 s cooldown):* shrinks to 0.86 and grows back over the cooldown (`1 - cooldown / 3`),
  laughing with happy arcs and little bounces for the first second; it slides along (no hops in Recover)
  with a slug-like crawl ripple.
- *Hit (visual 0.7 s):* squash kick and target 0.8, shear away from the blow, > < eyes, O mouth, brows up,
  drips spread, ripple spike, bubbles scattered, curl kicked. Flash gains: hit 0.4, telegraph 0.3.
- *Death (0.6 s):* a jolt (tall, eyes huge, O mouth), then it splats into a puddle (flat 0.06, spread
  1.55 with an overshoot, drips spread, a big radial ripple that reads as a splash ring); the eyes are
  held round (`linger_eyes`, the ghost's trick: local = head_model^-1 * wanted, a non-orthonormal basis),
  sink onto the puddle, tip back, blink and close; the root shrinks away in the last 14 %. Dropped onto
  the terrain height from `in.ground` (a slime killed mid-air would otherwise splat in the air).
- *Rise (0.6 s):* a thick puddle (flat 0.13, spread 1.35) on the ground, rippling; it gathers itself up
  with an overshoot (ease_out_back 2.4), the curl flicks up, eyes shut until 76 % then pop open wide.
- *Idle fidget (after 5 s idle):* a happy 'boing' on the spot facing the camera (dip, pop, curl wag, ^ ^).

### Added to the kit / shared helpers

- Nothing added to `mon_kit2.py`, `monster_anim.h` or `anim_kit.h`. `palette_append.py` gained `goo_lime`.

### Technique notes from the Slime (candidates for art-pipeline)

- **A jelly is one part with a scale and a shear, not a soft bend.** Put the pivot on the ground and drive
  a volume-preserving squash and a shear (top offset per unit height) through two springs; pass the shear
  as a non-orthonormal Basis. Everything on the jelly (face, bubbles, curl, skirt) follows it exactly,
  which a shader bend cannot do for rigid children.
- **Lay a skirt's top edge just proud of the body, tangent to it,** never starting inside it: two
  different facetings crossing leave a visible zig-zag seam in the same colour.
- **Translucent without alpha:** a darker saturated lime (`goo_lime`), a warm lime rim (0.3), gloss 0.5 for
  a small sharp catchlight, paler swirl decals, raised bubbles with big glints. Rim 0.42 plus a paler cell
  went milky in the close-up.
- **A dark mouth cavity needs gloss -2;** with a positive gloss the environment sheen greys it.
- **Wind-up flatten: 0.8, not 0.7.** From the side camera a 0.67 squash plus a lean back read as deflated.
- **A hop's crouch from `hop_in` only while it is above 0:** it sits at 0 when the monster is not going to
  hop, and a crouch keyed to "nearly 0" would hold it crouched forever.

### Verified for the Slime, and not

- Looked at: the Blender review and face sheets in every model round (round 1: drips read as a tyre ring
  and the skirt's top edge zig-zagged into the body: fixed); both lighting rows of
  `art\review\engine\monster_slime.png` (zoomed); three recordings, each with the beat sheet and the face
  still; 300-400 px strips of wander, chase (the hop cycle), attack_1 (wind-up from the side), attack_2
  (wind-up and release from the front, dense at 400 px), hit, die (into the puddle under the puff) and
  rise (`wip_m\strips\sl_r1_*`, `sl_r2_*`, `sl_final_rise_eyes`); BEFORE beats over AFTER beats with the
  mock and both face stills (`sl_compare.png`).
- Log line: `slime=v2(22)`. gates + `verify_rig2.py` PASS (7,414 tris, 21 mesh parts).
- Regression from the final DLL (slime + skeleton + bat recorded together in one run): monster_skeleton
  against the M8 recording (`ba_strips.py`: 0.9 to 2.8, death puff frames 9.4 / 17.7) and monster_bat (v1,
  unconverted) against BEFORE (1.5 to 4.2, death puff 8.3 / 15.6; the face strip looked at: identical
  poses, only the lit houses behind differ): unchanged.
- `game\override.cfg`: my run ended at 18:41:30 and a recording of the other lane started at 18:41:31
  (Godot started then, override.cfg rewritten then), so the file present right afterwards was theirs; it
  was gone once that run ended (checked 18:43).
- Not verified: the idle fidget (needs 5 s of idle; the face beat shows a blink, not the boing); the distance
  LOD path; a crowd of slimes; autopilot fps; a hit in the middle of a wind-up; the goo projectile's
  tumbling at full size in play (seen only leaving the mouth in the attack beats); the recover's crawl
  ripple at speed (the showcase's recover is mostly still).

### Known weak points (Slime)

- The wind-up's puffed cheeks only read from the front; from the side the wind-up is the flatten, the
  lean back and the eye flare.
- The lob's pop is a spring kick: flat to tall in about four frames at 60 fps (a deliberate snap).
- The drips are rounded bulbs but slightly cushion-like (flat-topped) in the close-up.
- The small static bubbles read a little like pimples in the close-up; the four big sliding ones read as
  bubbles.
- The thrown goo is smaller than the old stand-in (about 0.3 m across against 0.4 m); its readability as a
  threat in play was not playtested.
- The hit beat in the showcase is mostly seen from behind (a knocked monster turns to face the way it is
  thrown; gameplay).
- 7,414 of 8,000 triangles.

### Rebuild

```powershell
powershell -NoProfile -File art\team\monsters\batch04\go2.ps1 -Keys slime        # model, gates, verifier, preview
powershell -NoProfile -File art\team\monsters\batch04\build_native.ps1           # animator
tools\showcase.ps1 -Ids monster_slime -Out art\review\showcase\wip_m
python tools\showcase_sheets.py art\review\showcase\wip_m monster_slime
python art\team\monsters\batch04\strips2.py monster_slime attack_2 --n 9 --cols 3 --w 400 --from 96 --to 128 --crop 0.1,0.35,0.9,0.8
```

## M10 (Pumpkin Hopper): monster_pumpkin, v2 model and animator

The Pumpkin Hopper is the eighth v2 monster: 18 mesh parts (19 nodes), 7,650 triangles,
`native\src\play\monster_anim_pumpkin.cpp`. Collision is unchanged (height 0.9, radius 0.42). Per
FIDELITY_PASS decision 7 it is faithful to the PROCEDURAL pumpkin the game used (`build_pumpkin` in
`monster_art.cpp`: about 1.0 m wide, 0.8 m tall, apex-up triangle eyes, a slot grin with three teeth hanging
from the top lip, vine arms), not to the mock's face; it takes the mock's polish (ribs, carved rim, leaf
collar, curled stem, tendrils, three-leaf hands, green root feet). The shell is 1.02 x 0.93 m, 0.065 to 0.815 m
high; the stem tops out at 0.965 m. It is switched in by `PUMPKIN_HOPPER_USES_V2_MODEL` (`monster_art.h`,
`true`); set it to `false` to go back to the procedural pumpkin. Compare: `wip_m\strips\pk_compare.png`
(BEFORE beats over AFTER beats, the mock, both face stills).

### What exists

- **Model** (`pumpkin()` in `build_monsters2.py`; its docstring has the part tree).
  - `head` is the shell: `F.pumpkin_body` (farm_lib, reached as `M.F`) with 10 lobes, groove 0.16, plump 0.45
    (fat cushions, crisp V grooves), a groove at the front centre (`rot_z=-90`) so each eye sits on a lobe,
    the `pumpkin` material layer with its rib-aligned UVs, scaled to 0.93 deep. Shade cell `pumpkin_dark` in
    AO creases (the grooves) and on faces pointing straight down (`down=-0.8`; at -0.45 the lower body showed
    hard-edged rectangles). Pivot at its bottom (0.065 m): squash, rock and look all happen about it, so the
    feet (children of `body`) keep their shape.
  - The face is CARVED with `F.carve` (farm_lib): each eye (a `F.rounded_triangle`, 0.2 x 0.172 m) and the grin
    (`F.grin`, 0.5 m, three teeth, corners lifted) is a pocket with an `amber` cut rim (the flesh thickness),
    `glow_jack` inner walls and a floor (`glow_jack_hot` behind the grin, the dim `glow_jack_shell` ember
    behind the eyes). The head has no other emissive cells, so `rig.glow(head)` is the grin's light.
  - Eyes: `M.eye` parts INSIDE the eye pockets: a shallow glowing dome (`glow_jack_hot`, 0.102 x 0.094, depth
    0.022, centre 4.4 cm behind the surface) that fills the triangle aperture (the dome is larger than the
    hole; the parts outside it sit inside the closed shell and are never seen), an ink pupil (`pupil_l/r`,
    `iris=None`) and `glow_white` glints. A blink scales the dome flat, which shows the dark ember floor: the
    light in the eye goes out. Pupils read at night at glow 0.55 and through the wind-up flare (1.35).
  - `stem` (soft, pinned 3 cm inside the top dimple): one ridged tube (`F.sweep` with an alternating profile)
    that rises and curls over to its left into a fiddlehead. `leaf_a..d` (soft): four collar leaves draped
    over the top (every vertex ray-cast down onto a hi-res proxy of the shell, lifted 1.1 cm, cupped, wavy
    edges, tips curling up), each with a midrib and four side veins as thin tubes; pivot at the stem.
    `tendril_l/r` (soft): a curl each side of the stem.
  - `arm_l/r`: a curving vine tube (0.041 -> 0.029 m) with a small tendril curl, ball joint 0.044 at a
    shoulder sunk 3 cm into a `rust` socket ring (plus a hidden core blob so the verifier sees the parent reach
    into the ball). `hand_l/r`: three cupped leaves fanned in the plane facing forward, ball 0.031 at the wrist.
  - `foot_l/r` (children of `body`): green rounded clogs with three toe lumps, tucked 6 cm up inside the shell.
  - Colours: candy_orange / pumpkin_dark / rust / amber / glow_jack / glow_jack_hot / glow_jack_shell /
    gourd_green (stem, arms) / apple_green (leaves) / dragon_green_light (veins) / vine / vine_dark. NO new
    palette cells (row 13 still has 1 free). Materials: `pumpkin` on the shell, `plush` on the greens.
- **Catalog brief** describes the v2 part tree (published).

### The animator

Same shape as the others: a `Pose` mixed from locomotion and state layers by `anim::Weight`, springs, one
solve. Specific to the pumpkin:
- *The shell is the head's transform:* `rotvec_basis(rock) * look` with `squash_stretch(sy) * size` times the
  pose's `flat` / `spread`, lifted by `0.22 |roll| + 0.12 |pitch|` so a rocking ball never sinks its edge in.
  `sy` rings on a 4.6 Hz, zeta 0.24 spring; `rock` on a 3.6 Hz spring kicked by take-off (top stays behind),
  landing (top carries on), acceleration and hits.
- *Arms counter-scaled:* the arm's local basis is `Basis::from_scale(1 / head_scale) * FK`, so head * arm is the
  undistorted FK pose however the shell squashes (rest basis is identity; the hands see an orthonormal parent).
  FK = `Basis(Y, -side*fwd) * Basis(Z, side*raise) * Basis(X, twist)` on a `Spring3` per arm; the hands flop
  by `-0.09 * raise velocity` (overlap) and flutter in the air.
- *Hops* read from the body as on the slime (take-off: airborne 0.04 s with vy > 1.5; landing: fall > 1 m/s):
  crouch in the last 0.17 s of `hop_in` (only while it is above 0), arms back; launch kicks the squash up and
  the arms down (left behind), then they fly up; stretched rising (max 1.16), rounder falling; landing kicks
  `-1.6 * fall` (about 0.7 for a roam hop) and slaps the arms down; feet tuck in the air; collar leaves
  flutter (21 rad/s) and lift as it falls. Because the next hop often starts right after a landing, the read
  is squash -> stretch -> squash, a bouncing ball.
- *Face:* MonsterFace for blinks, pupils and the head turn (max yaw 0.45, pitch 0.14; idle look off); the
  eyes are re-posed for the squint (wind-up, fidget, dizzy); the pupils' slide is read back from the node and
  re-posed with a size and the dizzy swim. Candle: `rig.glow(eye)` 0.55 and `rig.glow(head)` 1.0 at rest, a
  slow noise waver on top of the shader's own flicker.
- *Wind-up (0.45 s):* squash to 0.74 then 0.69, two rocks side to side (0.17 rad), arms cocked up and back,
  palms turned, leaves lifted, stem coiled back, eyes narrowed with smaller pupils, eyes 1.35 / grin 2.0
  glow, 46 Hz tremble and a tip back in the last quarter. Telegraph gain 0.35.
- *Leap (Attack):* stretched 1.22 tipped back with both arms over its head while rising, pitching forward
  over the top while falling, face blazing. *The slam* is a squash kick on Recover's first frame when it came
  from Attack (and a doubled landing kick when the leap really landed): in the showcase the attack is held in
  place (BEFORE shows the same), so the slam must not depend on a detected landing.
- *Dizzy recover (1.5 s of Recover):* flattened 0.68, re-inflating on `ease_out_elastic`, the shell rolling in
  a cone, pupils swimming in circles, eyes half shut, arms hanging limp then lifting, stem drooped and
  swaying, a shake at the end.
- *Hit (visual 0.62 s):* squashed, tipped away from the blow, eyes shut (the dark ember shows), the light
  knocked down to 0.18 / 0.3 and coming back, arms flung up, leaves and stem whipped. Hit flash gain 0.45.
- *Death (0.6 s):* a start (light flares, eyes wide), then the shell caves in wide and low (flat 0.5, spread
  1.22, ease_out_back), stem flops over, arms and leaves go limp, the candle sputters and goes out, eyes shut;
  root shrinks in the last 14 % (behind the game's puff).
- *Rise (0.6 s):* buried 0.47 m at 0.55 size (only the top, stem and tendrils show, wiggling), it bursts up
  and inflates with an overshoot (ease_out_back 2.2), arms flung out, eyes shut and dark; then the candle
  catches (a flicker pulse) and the eyes pop open.
- *Idle fidget (after 5 s idle):* a happy double bounce on the spot with a rock to one side, crinkled eyes.

### Added to the kit / shared helpers

- Nothing added to `mon_kit2.py`, `monster_anim.h` or `anim_kit.h`. The builder uses farm_lib's
  `F.pumpkin_body`, `F.carve`, `F.rounded_triangle`, `F.grin`, `F.shape_of` and `F.sweep` (already reachable
  as `M.F`).

### Technique notes from the Pumpkin (candidates for art-pipeline)

- **A carved face with rim thickness = a boolean pocket, not a hole.** `F.carve` cuts a closed pocket per
  feature (flesh rim band, glowing wall band, floor) into the closed shell: no inside of the pumpkin has to be
  modelled, and the cut edge reads as thickness from any angle. Keep the mouth's `inset` small (0.007): with
  0.02 the grin's floor outline self-intersected at the sharp corners and the pocket came out half cut.
- **Living eyes in a carved hole:** put an `M.eye` dome inside the pocket, larger than the aperture, a few cm
  behind the surface; the closed shell hides the rest. A blink scales it flat and reveals a dark ember floor.
- **Glow per part needs glow cells on that part only.** The shell's only emissive faces are the carved
  pockets, so `rig.glow(head)` dims, flares and snuffs the face without a separate face part.
- **Counter-scale children of a squashed body with `Basis::from_scale(1/s) * R`** (rest basis identity): the
  vine arms keep their length and shape through every squash and stretch.
- **Showcase attack beats hold HOP monsters in place:** key the slam to the Attack -> Recover transition, not
  to a detected landing.
- **Thin shell leaves:** a leaf tip whose width goes to 0 while an edge wave still offsets it along the normal
  gives a collinear (degenerate) triangle that fails the gate; scale the wave by the width.
- `candy_orange` held its orange under the key light and in the night street with gloss 0.12 and a warm rim
  0.1; the `pumpkin` layer's speckles read as skin.

### Verified for the Pumpkin, and not

- Looked at: the Blender review, face, front (four views) and pose sheets (six extremes: rest, hop arms up,
  slam arms flat, look, rock, hit; two views; no gap at shoulders, wrists or feet); both lighting rows of
  `art\review\engine\monster_pumpkin.png` (zoomed crops; the preview runs no animator, so the light domes are
  pale there); three recordings: beat sheets beside BEFORE and the mock (`pk_compare.png`), face / front
  stills, 400 px strips of every beat (`wip_m\strips\pk_r1_*`, `pk_r2_*`, `pk_final_*`), dense strips of
  attack_1 (side) and attack_2 (front: wind-up, stretch, slam, dizzy), a roam landing, the death and the rise.
- Log line: `pumpkin=v2(19)`. gates + `verify_rig2.py` PASS for all eight v2 monsters.
- Regression from the final DLL (pumpkin + skeleton + bat recorded together): monster_skeleton against the
  18:40 recording (`ba_strips.py`: 1.1 to 2.8, death puff frames 12.2 / 17.7) and monster_bat (v1) against
  BEFORE (1.4 to 4.6, death puff 8.0 / 16.3): unchanged; skeleton beat sheet looked at. `game\override.cfg`:
  another lane's recordings started right after mine (Godot started 19:11:22 and 19:13:26) and wrote it;
  it was absent between them (checked after my last preview run). None of my runs left one behind.
- Not verified: the real in-game leap and slam (the showcase holds the attack in place; the leap pose from
  vertical speed, the doubled landing kick and the arms over the head in flight were not seen in a recording);
  the idle fidget (the face beat shows a blink, not the bounce); the distance LOD path; a crowd of pumpkins;
  autopilot fps; a hit in the middle of a wind-up.

### Known weak points (Pumpkin)

- The slam squash reaches the 0.5 clamp: a very flat pancake for a few frames (reads as a slam; could be
  softened if it looks too rubbery for a pumpkin).
- The grin is a crescent with three teeth, smaller and less blazing than BEFORE's big white-hot slot.
- The eye triangles are rounded, so from some angles they read as rounded slanted eyes rather than sharp
  triangles; the pupils (new) change the face's character from the procedural one (the lead's eye rule).
- From the high front camera the collar leaves read as a flat green star; their veins only show close up.
- The hit beat is seen from behind in the showcase (gameplay turns a knocked monster).
- The mouth cannot change shape (it is carved); expression is eyes, light, squash and arms.
- 7,650 of 8,000 triangles.

### Polish round (2026-11-15): the carved face, the slam

The lead rejected the M10 face (lemon eyes with googly pupils and glints, a small lopsided crescent grin: a
cartoon face stuck on a pumpkin). It is now a CARVED JACK-O'-LANTERN again, as BEFORE and the mock; the shell,
ribs, stem, leaves, vine arms and feet are untouched. 22 mesh parts, 7,792 triangles. Compare:
`wip_m\strips\pk_compare.png` (BEFORE beats / AFTER beats / mock, BEFORE face, AFTER face).

- **Eyes**: two upright `F.rounded_triangle` holes, 0.28 x 0.238 m (round 0.14), centres x = +-0.175 at 0.568 m,
  symmetric, carved 7 cm deep (`amber` rim 40 % of the depth, `glow_jack` walls, a dim `glow_jack_shell` floor).
  No sclera, pupil, glint or dome (the M.eye domes are gone).
- **Mouth**: ONE wide upturned grin, 0.64 m, symmetric, two square teeth hanging from the top lip and one standing
  in the middle of the lower lip. The head is carved with the ROAR (same upper lip, teeth and corners, lower lip
  dropped to 0.245 below the centre); `mouth` is the lower-lip band between the two (the standing tooth included),
  a parametric tube of cross sections [skin on the surface, flesh wall tapered into the mouth, back, lower wall],
  132 triangles, laid on the head's own low-res facets 1.5 mm proud, with the SHELL'S OWN UVs copied onto its skin
  (one shell triangle per face, extrapolated: each lobe is its own UV island and a face that took corners from two
  islands smeared the texture into stripes). Hiding `mouth` drops the jaw. (A boolean INTERSECT plug cost 910
  triangles; EXACT intersect returns nothing in Blender 5.1, FLOAT works.)
- **Light, graded outside in**: amber flesh rim -> `glow_jack` walls (`rig.glow(head)`) -> a light plate behind each
  hole (`eye_l/r`: concentric rings of the triangle, `glow_jack` outer ring, `glow_jack_hot` inside; `mouth_glow`:
  a 12 x 3 grid, `glow_candle` core, `glow_jack_hot`, `glow_jack` ends) -> in each eye a small teardrop flame
  (`pupil_l/r`: `glow_white` core in a `glow_candle` body, 3 cm inside the hole). Gains in the animator:
  `EYE_GLOW 1.0`, `PLATE_K 0.9`, `MOUTH_K 1.0`, `FLAME_K 1.7`. Reads yellow-hot, not a white card, in the studio
  row and the night street.
- **Brows**: `brow_l/r`, shell wedges (`candy_orange` front, `amber` cut edges) hidden 8 mm under the shell along its
  NORMAL just above each triangle, their lower edge running down toward the nose at 38 degrees; pivot 0.42 m behind
  the face, local Z = the eye's normal. `rig.pose(brow, Basis(X, 0.11 * anger))` slides them about 4.6 cm down along
  the shell into the top of the hole: the triangles slant into angry eyes. They stop short of the shoulder groove
  (a wedge facet spanning a groove's V poked out of it; a y offset near the shoulder is almost no depth).
- **Animator** (`monster_anim_pumpkin.cpp`): every behaviour kept, re-targeted. Blink / hit / death (`face_.closed`)
  shrink the plates to 42 % toward their centre and dim them to 30 %, and snuff the flames. The flames slide toward
  the player (`anim::pupil_offset`, 12 % of the plate's reach, damped), flicker (noise size and height, a small
  lean), swim in circles when dizzy, are knocked askew on a hit, grow 35 % and burn 50 % brighter in the wind-up.
  `Pose::anger` (wind-up 0 -> 1, leap 0.8) drives the brows; `Pose::roar` (wind-up from 50 %, the leap, the first
  0.32 s after the slam) hides `mouth`, with hysteresis, and each switch is covered by the candle guttering (all
  glows dip 45 % for 0.09 s). The fidget's happy squint narrows the plates in Y.
- **Slam**: the hard 0.5 clamp is now a soft floor (`tanh` below 0.8, asymptote 0.56) and the first 0.45 s of the
  Recover rings on a stiffer spring (6 Hz, zeta 0.2, kick -12) with the dizzy base at 0.76 inflating from 0.05 s:
  the flattest frames are 2 to 3 (was 5 at the clamp), then an elastic stretch to about 1.18 and a settle. Timings
  unchanged. Strips: `wip_m\strips\pk_p2_slam.png` (consecutive) vs the old one.
- Catalog brief rewritten and published.

Verified (polish): face, front and pose sheets (new `angry` pose: brows down); engine preview; six recordings, the
last from the final four-monster build; 400 px strips `pk_final_wander/chase/attack_1/attack_2/hit/die/rise_idle`,
`pk_p2_windup_face` (angry eyes, roar), `pk_p2_roar`, `pk_p2_slam`, `pk_p1_hit/die/rise`; the face read at wander
and chase size is at least as strong as BEFORE. A facet-level check (`scratchpad t_poke.py`: brow facets 7 mm
inside the head). Not verified: the brows in a real in-game leap (the showcase holds the attack); the roar in play;
the plates at the far LOD (lights are set every frame there too, unthrottled by distance).

Weak points (polish): the roar's jaw drop is modest (lower lip 5.5 cm lower at the centre; the head cannot be
carved deeper there without reaching the bottom curve); a faint seam shows along the lower edge of the lip band in
the close-up; the flames are simple teardrops; the grade is cell-stepped (bloom smooths it).

### Rebuild

```powershell
powershell -NoProfile -File art\team\monsters\batch04\go2.ps1 -Keys pumpkin      # model, gates, verifier, preview
powershell -NoProfile -File art\team\monsters\batch04\build_native.ps1           # animator
tools\showcase.ps1 -Ids monster_pumpkin -Out art\review\showcase\wip_m
python tools\showcase_sheets.py art\review\showcase\wip_m monster_pumpkin
python art\team\monsters\batch04\strips2.py monster_pumpkin attack_2 --from 70 --to 190 --n 20 --cols 5 --w 320 --crop 0.15,0.25,0.85,0.75
```

## M11 (Bat): monster_bat, v2 model and animator

The Bat is the ninth v2 monster: 20 mesh parts, 7,858 triangles, `native\src\play\monster_anim_bat.cpp`.
Collision is unchanged (height 0.5, radius 0.3). It flies, so the model stays centred on the origin (the game's
flyer origin is its body centre) and is built with `M.Rig(..., floating=True)`; the catalog keeps
`"floating": true`. The art spans 1.38 m wing to wing, -0.28 to 0.37 m high. Compare: `wip_m\strips\bat_compare.png`
(BEFORE beats over AFTER beats, the mock, both face stills).

### What exists

- **Model** (`bat()` in `build_monsters2.py`; its docstring has the part tree).
  - `body`: the torso (an egg, fuller low), a cream belly patch (`M.pad` oval with a tufted edge), a tail nub with
    two tufts, and the **neck ruff**: ONE closed shell (`ruff`) laid 1.2 to 2.4 cm proud of the torso round the
    neck, its top running up inside the head, its lower edge 15 tuft points of uneven length. It hides the
    head/torso crease from every side, so the two balls read as one fluffy body.
  - `head` (pivot at the neck, sunk deep in the torso so it turns and pitches without a gap): cheek fluff
    (`M.straw` blades), a witch_purple button nose with nostrils, soft blush pads, the ink lip line (a wide smile)
    and two fangs. `jaw` is the open mouth (cavity + tongue patch, `mouth_dark` / `tongue`), scaled in Y about
    the lip line. `brow_l/r` thick ink arcs. `alt_squint_l/r` (> <) hidden until a hit or the death.
  - Eyes (`M.eye`): `trim_white` sclera, a thin `iris_brown` limbal ring (never glows) round a `glow_amber_dim`
    disc (the glow), an ink pupil, `glow_white` glints, an ink lash line with an outer flick (`eye_extra`).
  - `ear_l/r`: big cupped shells (fur back and rim, `candy_pink` inside, `house_plum` in the creases), soft
    (pinned at the sunk base, bent along the ear by the animator) and also turned as a whole.
  - `tuft`: one soft part of five curled locks (`witch_purple` / fur) on the crown.
  - `wing_l/r` (pivot at the shoulder, ball 0.038, identity rest): the furred arm (shoulder to wrist), a thumb with
    a cream claw, the inner membrane (`bat_wing`, `spider_plum` rim, billowed) and one vein.
    `wingtip_l/r` (pivot at the wrist, ball 0.025): three fingers with hooked tips, the outer membrane (a fan round
    the wrist, scalloped between the fingertips, billowed between the fingers) and two veins. **Its rest frame has
    +Z out of the membrane's front and +Y along the first (innermost) finger.** The inner membrane ends exactly on
    that finger, so a rotation about the wingtip's local Y (the fold) never opens a gap between the membranes; a
    rotation about any other wingtip axis would. Fold back = `Basis(Y, -SIDE * fold)`.
  - `foot_l/r` (ball at the hip, sunk in the torso): a short leg, a purple paw, three toes with cream claws.
  - Colours: hat_violet (fur) / spider_plum (fur shade, membrane rims, finger hooks), bat_wing (membranes),
    candy_pink / house_plum (ears), linen_light / linen (belly), witch_purple (tuft, nose, paws), cheek_pink,
    trim_white, iris_brown, glow_amber_dim, glow_white, ink, mouth_dark, tongue. **No new palette cells** (row 13
    still has one free). Material layer `plush` on fur and membranes, `flat` on face details.
- **Catalog brief** describes the v2 part tree (published).

### The animator

Same shape as the others (a `Pose` mixed from locomotion and state layers by `anim::Weight`, springs, one solve),
plus a flap generator:
- *The beat.* A phase `flap_` advances by the mixed pose's `flap_rate` (Hz); the inner wing's raise is
  `base + flap_amp * sin`, its sweep and twist follow, and the outer wing folds back about its hinge by
  `flap_fold * ((1 + cos) / 2)^1.5`: most while the inner wing rises, open on the downstroke (a quarter phase
  behind). The body bobs against the beat (`-bob * sin`) and rocks a little; the head rides a little behind.
  Amplitude, rate, fold and bob come from the mixed pose, so state changes never pop the cycle. Wing bases
  (raise, sweep, fold) are on a `Spring3` per wing (kicked by hits and the dive).
- *Locomotion:* hover 3.3 Hz, faster and swept back with speed, pitched into the flight (root lean), banks into
  turns (lateral acceleration kicks the lean spring), ears streaming back, feet trailing and paddling; idle roll
  and a curious head tilt; chasing: excited grin, brows down, head on the player.
- *Look:* MonsterFace with max pitch 0.45, but the head's upward pitch is clamped to 0.12 rad and the pupils'
  upward slide to 4 mm: it hovers above whoever it watches (and the showcase's chase lure is 40 m up, which
  rolled the face back and the eyes up).
- *Wind-up (0.7 s):* rears back and up (lean -0.62, body up 11 cm and back 9 cm), wings raised high and spread
  to the sides, the beat dropping to a 13 Hz shiver plus noise tremble, ears pricked then pinned, chin down glaring,
  a clamped fanged grin opening into a screech, brows down, eyes narrowed then wide; iris glow 0.2 -> 0.9, glints
  0.4 -> 1.5; in the last fifth it tips forward, wings sweeping back (anticipation). Telegraph gain 0.25.
- *Dive (Attack):* wings swept back into a V (raise 0.88, sweep 0.82, fold 0.62), body pitched along its velocity
  (at least 0.42 rad), stretched 1.16 (a squash kick on entry), ears flat back, feet trailing, mouth wide, eyes
  blazing.
- *Recover:* a fast shallow flutter (7.5 Hz, amp 0.3) settling back over 0.9 s, three quick head shakes with the
  eyes screwed shut and the ears flapping (their Dangles catch the head), then a smug grin.
- *Hit (visual 0.62 s):* a tumble (spin away from the blow), squash kick, wings flung up and open, ears and feet
  kicked, > < eyes, 'oof' mouth, then hard flapping to right itself. Hit flash gain 0.2 (0.45 and 0.3 still went
  white).
- *Death (0.6 s):* a jolt (eyes wide, wings up), then the wings wrap round the body (sweep forward, fold forward
  1.65) and it spirals (two turns, accelerating, tilted), falling 0.35 m and shrinking into the game's poof.
- *Rise (0.6 s):* hanging upside down wrapped in its wings (a non-springed root `tumble` of PI), it flips upright
  with an overshoot (ease_out_back), unfurls its wings with a fling, the beat starts, and the eyes pop open at
  62 %. The rise layer fades out over 0.22 s after Rise ends, so the hand-off to Roam is smooth.
- Soft parts: ears (`Dangle` 4.2 Hz on the pose-turned shell, an updraft from vertical speed), tuft (3.6 Hz),
  feet (rigid swing from a `Dangle`). Gloss: fur and membranes -0.45, sclera 0.55, iris -1, mouth 0.25; rim 0.14.

### Added to the kit / shared helpers

- `M.pad(..., up=, aspect=, outline=)`: an oval pad oriented by `up` on the surface, with a wavy or tufted edge
  (`outline(angle)` scales the outer two rings). Old calls build exactly as before.
- `Rig.pose_sheet`: for a `floating` rig only, the ground disc sits under the model and the camera frames its full
  span (the bat is the only floating rig; every other monster's sheet is unchanged).
- Nothing added to `monster_anim.h` or `anim_kit.h`.

### Technique notes from the Bat (candidates for art-pipeline)

- **A hinged membrane wing with no gap:** end the inner membrane exactly on the first finger, give the outer part
  a rest frame whose +Y runs along that finger, and fold only about that axis. Any in-plane bend at the wrist
  tears the membrane open; the fold about the finger never does. Flap the inner part in the frontal plane (about
  the body's Z) plus a little sweep and twist; fold the outer on the upstroke.
- **Fur ruff as a layer, not as blades.** Loose `straw` blades round a neck read as paper diamonds, and pale lilac
  ones at an ear base read as crystal shards. One closed shell laid 1-2 cm proud of the torso, its top inside the
  head, its lower edge a ring of uneven tuft points, reads as fluffy fur from every side and hides a head/torso
  crease.
- **Amber eyes that glow at night:** `glow_amber` (#ff9a26) reads lemon yellow under the showcase key light at
  any glow (it is the albedo, not the emission); `glow_amber_dim` (#a8590f) reads as the mock's amber in the
  studio and glows amber at night. A non-glowing dark limbal ring (`iris_brown`) round the glow disc keeps the
  eye from reading as a glowing bullseye; rest glow 0.2, flare 0.9, the pupil stays readable.
- **Flyers above their target:** clamp the head's upward pitch and the pupils' upward slide separately from the
  downward ones.
- **A flap as a phase generator** whose amplitude, rate and fold come from the mixed pose: every state change
  blends the beat instead of restarting it.
- **Flash gains on a mid-light lilac monster:** telegraph 0.25, hit 0.2.
- **Spawn for a flyer:** hang upside down wrapped in the wings and flip upright with an ease_out_back on a root
  rotation that bypasses the lean spring.

### Verified for the Bat, and not

- Looked at: the Blender review, face and pose sheets in every model round (round 1: snowman head/torso, tufts as
  diamonds, small mouth; fixed); the pose sheet (rest, upstroke fold, downstroke, dive V, death wrap, hit + look;
  two views; no gap at shoulders, wrists, hips or the membrane hinge); both lighting rows of
  `art\review\engine\monster_bat.png` (zoomed: night and daylight; the preview runs no animator, so eyes are at
  full glow there and the pupils still read); four recordings: beat sheets beside BEFORE and the mock, face
  stills and eye close-ups (`bat_r2_eyes` .. `bat_r4_eyes`), 400 px strips of every beat (`wip_m\strips\bat_r1_*`),
  dense strips of the wind-up and dive from both attack cameras, the recover, the hit, the rise, and
  consecutive-frame strips of wind-up -> dive, rise -> roam and the death (`bat_r4_transitions.png`: no pops).
- Log line: `bat=v2(20)`. gates + `verify_rig2.py` PASS for all nine v2 monsters.
- Regression from the final DLL (bat + skeleton + spider recorded together): monster_skeleton against the 19:10
  recording (`ba_strips.py`: 1.0 to 2.9, death puff frames 12.5 / 17.8) and monster_spider (v1, the last
  unconverted monster) against BEFORE (1.1 to 3.3, death puff 7.5 / 17.3): unchanged; both beat sheets looked at.
- `game\override.cfg`: absent after each of my runs; present afterwards only because the other lane's recording
  started at 19:48:09, 21 s after my last run ended (19:47:48); gone again at 19:50 when that run ended.
- Not verified: the distance LOD path; a crowd of bats; autopilot fps; a hit in the middle of a wind-up; the real
  in-game dive hitting the player (the showcase's lure is unhurt); the wing shiver and ear flutter frame by frame;
  idle fidgets (there are none: the bat's idle is the hover, roll and head tilt).

### Known weak points (Bat)

- The telegraph flash (gain 0.25) still makes the wind-up visibly paler than the rest of the clip; it is the
  gameplay telegraph, so it was not lowered further.
- The death's middle is under the game's puff and sparkles (as for every monster); the wrapped, spinning bat shows
  below the puff as it falls.
- The wings flap in the frontal plane, so from a pure side view a wing is a thin line moving up and down.
- When a wing is raised high the inner membrane's short body edge lifts off the flank; the shoulder ball and the
  ruff hide it in the views looked at, but it was not checked from below.
- The ruff is a regular, stylised zig-zag rather than soft fur; the rest of the body's silhouette is smooth (the
  mock's fur is fuzzier everywhere).
- The open mouth only scales (no true 'O' shape).
- 7,858 of 8,000 triangles.

### Rebuild

```powershell
powershell -NoProfile -File art\team\monsters\batch04\go2.ps1 -Keys bat          # model, gates, verifier, preview
powershell -NoProfile -File art\team\monsters\batch04\build_native.ps1           # animator
tools\showcase.ps1 -Ids monster_bat -Out art\review\showcase\wip_m
python tools\showcase_sheets.py art\review\showcase\wip_m monster_bat
python art\team\monsters\batch04\strips2.py monster_bat attack_2 --from 60 --to 135 --n 16 --cols 4 --w 400 --crop 0.25,0.3,0.75,0.72
```

## M12 (Spider): monster_spider, v2 model and animator

The Spider is the tenth and last v2 monster: 32 mesh parts (the cap), 7,680 triangles,
`native\src\play\monster_anim_spider.cpp`. Collision is unchanged (height 0.7, radius 0.45); the art stands 0.656 m,
legs reach 0.55 m from the axis. Compare: `wip_m\strips\sp_compare.png` (BEFORE beats over AFTER beats, both face
stills, the mock).

### What exists

- **Model** (`spider()` in `build_monsters2.py`; its docstring has the part tree).
  - `body`: a small dark thorax hub (`spider_plum`); every hip and the waist sit inside it.
  - `abdomen` (pivot at the pedicel, inside the hub): a plump egg (0.26 x 0.28 x 0.25, tilted back end up 12
    degrees), `hat_violet`, `flat` layer, gloss 0.22 in the animator. The jack-o'-lantern face is CARVED into its
    back with `F.carve` (two rounded triangle eyes and a toothed grin; `spider_plum` rim, `glow_jack` walls,
    `glow_jack` / `glow_jack_hot` floors). F.carve cuts along +Y from the front, so the abdomen is built at the
    origin, rotated so the marking's direction (42 degrees up from straight back) faces -Y with the face's up toward
    the spider's front, carved, rotated back and moved into place. Its only glow cells are the face, so
    `rig.glow(abdomen)` is the jack's light. Two spinnerets. `alt_silk` (child of the abdomen, pivot at the
    spinnerets): a 0.25 m thread authored along -Z (inside the height limit); the animator turns it up and stretches
    it 14 times for the spawn.
  - `head` (pivot at the neck, inside the hub): a fur ball (`spider_berry`, `plush`) fuller low, a cheek-and-chin
    ruff (ONE closed `M.shell` 2 to 5 cm proud with a tufted lower edge: low under the chin, up to the eye line at
    the cheeks), pink nose, blush, ink lip line. Big eyes with `M.eye`: `trim_white` sclera, `iris_brown` limbal ring,
    `glow_amber_dim` iris, ink pupil, `glow_white` glints, ink lash with an outer flick (as on the bat).
    `eye_s0..2`: the six small eyes as three mirrored PAIRS (part budget), inner to outer, so MonsterFace's ripple
    blinks them outward in three steps. Each small eye is an `iris_brown` dome + amber disc + pupil + glint built
    with `M._dome` / `M._decal` in its own frame; the pair's pivot is the midpoint of the two centres with z_out the
    mean normal, so a Y scale closes both in place. `fang_l/r` (parts, pivot at the lip), `jaw` (cavity + tongue,
    scaled in Y about the lip line), `tuft` (soft: one big curl tube rolling forward plus two small ones; tubes, not
    `M.straw` blades, which read as a crown of leaves), `alt_squint` (> < for both eyes in one part).
  - Legs: `leg_l0..3` / `leg_r0..3` (0 = front; hips at azimuth 38 / 72 / 108 / 142 degrees) > `shin_*`. Chunky
    tubes (0.058 m at the hip) with two narrow 20 % grooves per segment painted `spider_plum` (the rings), knees at
    0.39 m, the shin curving out then straight down to a slightly bulbous foot. Pale tips: `bat_wing` then
    `bat_belly`, with a fringe (every other face of the 7-sided tube reaches 3 % of the shin further). Ball joint
    0.056 at each knee (on the shin), hips sunk in the hub (no ball). Translation-only: the IK end is the foot
    contact point, which the animator takes from `rig.tip(shin)` dropped onto the rest ground plane.
  - Colours: hat_violet / spider_plum / bat_wing / bat_belly (legs, abdomen, hub), **new cell `spider_berry`
    #6c2a62** (row 13, its last free slot) / licorice_black (head fur and shade), trim_white, iris_brown,
    glow_amber_dim, glow_white, ink, cheek_pink, candy_pink, mouth_dark, tongue, glow_jack, glow_jack_hot,
    ghost_white (silk). **Lane M has no palette cells left** (rows 13 and 14 are full).
- **Catalog brief** describes the v2 part tree (published).

### The animator

Same shape as the others (a `Pose` mixed from locomotion and state layers by `anim::Weight`, springs, one solve),
plus world-planted feet:
- *Walking (the gait).* The eight feet are planted in the WORLD and step in two alternating groups of four
  (l0 r1 l2 r3 / r0 l1 r2 l3). One phase clock advances by distance travelled plus turning (`|dyaw| * 0.45 m`) over a
  stride of 0.36 to 0.56 m (with speed); group A swings in its first half, group B in the second. At each half
  boundary the landing group snaps to its target and the lifting group records where it lifts from; a swinging
  foot flies on an arc (0.05 to 0.085 m) to its home plus a quarter stride along the velocity, so the stance feet
  pass under the body and never slide. Standing still the clock runs on (2.4 cycles/s) only until every foot is
  within 2.8 cm of home, then stops on a boundary: settling steps after a stop, a turn on the spot or a stance
  change (the wind-up's splay steps out by itself). A foot already home only shuffles (lift scales with the step's
  length). Foot heights follow `in.ground` relative to the ground under the origin (clamped 25 cm); the hub tips
  with the terrain under it. Each group landing kicks the hub's height spring down a little. Every leg is solved
  with `two_bone_ik` in the hub's space (pole above and outside the hip), so the hub can dip, sway and tilt freely.
- *Air poses* (hub space, blended by a weight that goes in within 0.05 s and out over 0.1 s): tuck, star, reach,
  push (the launch), flail (hit), hang + paddle (spawn), curl + twitch (death). While fully in the air the planted
  feet follow the air pose projected onto the ground, and the clock is parked on a boundary with settling on, so
  touching down hands over to settling steps without a pop.
- *Locomotion:* low scuttle (hub pitched forward with speed), the hub yaws and rolls with the gait phase and banks
  with lateral acceleration, the abdomen sways behind it and breathes (scale), fang clicks at random, a curious
  head tilt standing about. Idle fidgets (after 2.2 s idle: tap a front foot and look at it; a slow look round
  with a ripple blink) were NOT seen in a recording (the showcase idle is too short).
- *Wind-up (0.65 s):* crouch and splay (hub down 10 cm, back 3.5 cm, head low), then the cat wiggle: the abdomen
  raised and swinging side to side at 4 rising to 8.5 Hz, fangs clicking at 9 Hz, a tremble, eyes narrowed; in the
  last fifth it loads (drops lower, nose up), the jaw opens into a hiss, eyes widen; iris 0.3 -> 1.0, glints
  0.45 -> 1.4, jack 0.8 -> 1.9 with the wiggle. Telegraph gain 0.2 (0.3 washed it pink).
- *The pounce:* **the showcase holds the monster still through Attack** (a probe printed velocity 0, on the floor,
  for the whole state; BEFORE shows the same). So the pounce runs on its own clock from the first frame of Attack:
  if the body really flies (in play), the phases follow its vertical speed and the real landing (Attack -> Recover)
  kicks the dip; if it is held, it leaps in place (`LEAP_TIME` 0.5 s, `LEAP_H` 0.34 m root lift) on the same
  timeline with a fake vertical speed, and the leap's own end kicks the dip. Either way: push-off, legs tucked
  rising (nose up), star-spread at the top, reaching nose down, landing dip to about 60 % and a rebound, abdomen
  thrown by its spring, mouth wide, fangs spread, eyes blazing. The game's landing puff appears at 0.15 s, while
  the held leap is still in the air.
- *Recover:* a nom (fangs and jaw) and an abdomen shake.
- *Hit (visual ~0.6 s):* the hub tips back and rolls (plus HitRecoil on the hub), abdomen squashed and kicked,
  > < eyes, mouth open, fangs out, iris and jack dimmed, legs flailing while the knock has it in the air. Hit flash
  gain 0.25.
- *Death (0.6 s):* a jolt (hops 14 cm, legs thrown out stiff, eyes wide), then it rolls onto its back (a root roll
  of PI about a point 0.33 m up, done by 36 % so the puff does not hide it), legs curling in with a decaying
  twitch, > < eyes, the jack goes out; it shrinks away in the last 14 %.
- *Rise (0.6 s):* it drops in head first on its thread (root pitched 1.35 rad about the spinnerets, turning, legs
  tucked and paddling), flips over as it lands (ease_out_back), legs flung into the star and down onto the ground,
  the thread is hidden at 50 %, the eyes pop open at 78 %.
- *Look:* MonsterFace (max yaw 0.55, pitch 0.35), upward head pitch clamped to 0.12 rad and the pupils' upward
  slide to 4 mm (the chase lure is 40 m up). Soft part: tuft (`Dangle` 3.6 Hz). Gloss: fur -0.45, abdomen 0.22,
  sclera 0.55, small eyes 0.4, iris -1, fangs 0.15, jaw 0.25; rim 0.14.

### Added to the kit / shared helpers

- Nothing added to `mon_kit2.py`, `monster_anim.h` or `anim_kit.h`. The builder uses the kit's private `M._dome` /
  `M._decal` for the small eyes (a candidate for a public `M.small_eye`).

### Technique notes from the Spider (candidates for art-pipeline)

- **World-planted multi-leg gait from one phase clock:** two groups, the clock advanced by distance plus turning,
  swing from the lift point to home + a quarter stride ahead; stop the clock only on a group boundary once every
  foot is home. Stance changes (a splay) and turns on the spot then produce settling steps for free. Feed the IK in
  the body's own space so the body can bob, sway and tilt over fixed feet.
- **Air poses as a separate target set blended by weight**, with the planted feet re-seeded from the projected air
  pose while airborne: the touchdown hands over to settling steps with no pop.
- **Check what the recorder really does before animating an attack:** a one-line probe of `vel_local` /
  `on_floor` per frame showed the showcase holds the body still through Attack. Drive the move from its own clock
  and fake the travel only when the body is held, so play and the showcase both read.
- **Carving a decal-like glowing marking onto a curved part:** build at the origin, rotate the marking direction to
  -Y, `F.carve`, rotate back. The marking then glows from inside dark-rimmed pockets (not a sticker).
- **Small eyes in mirrored pairs** halve their part cost and still give a symmetric outward blink ripple.
- **Pale leg tips:** a darker mid band (`bat_wing`) before the pale cell (`bat_belly`) and an alternating fringe
  read as a fur fade; under the key light `bat_belly` stays lilac, not white.
- **Death roll timing:** finish a roll in the first third of the death; the game's puff covers the middle.

### Verified for the Spider, and not

- Looked at: the Blender review, face and pose sheets each model round (round 1 spindly legs, weak rings, small
  dark head, leaf-crown tuft; fixed); the pose sheet (rest, group A stepping, crouch + wiggle, star, tuck, look +
  curl; two views; no gap at knees or hips); both lighting rows of `art\review\engine\monster_spider.png` (zoomed
  face crop); four recordings: beat sheets beside BEFORE and the mock, the face still, 400 px strips of every beat
  (`sp_r1_*`, `sp_r2_*`, `sp_final_*`), dense consecutive strips of the scuttle, the leap (wind-up -> leap ->
  landing -> recover, `sp_final_leap_consecutive`) and rise -> roam (`sp_final_rise_consecutive`): no pops.
- Log line: `spider=v2(32)`. gates + `verify_rig2.py` PASS for all ten v2 monsters.
- Regression from the final DLL (spider + skeleton + bat recorded together): monster_skeleton and monster_bat
  against their 19:45 recordings (`ba_strips.py` 0.8 to 2.5, death puff frames 9.8-17.9 / 10.0-19.5): unchanged;
  skeleton beat sheet looked at. There is no unconverted monster left to check against BEFORE.
- `game\override.cfg`: absent after every run.
- Not verified: **the real in-game pounce** (the showcase holds it; the real-flight path of the pounce code and
  its landing kick were never seen); foot planting on slopes, kerbs and steps (the showcase street is flat; the
  terrain offset is clamped to 25 cm); foot slide measured numerically (judged from strips and by construction);
  the idle fidgets; the distance LOD path; a crowd of spiders; autopilot fps; a hit in the middle of a wind-up or
  a leap.

### Known weak points (Spider)

- In the showcase the leap is in place (the recorder holds the body); the game's landing puff shows at 0.15 s while
  the spider is still in the air.
- The launch frame is abrupt (crouch to 14 cm up in two frames at 60 fps).
- The head is smaller and less furry than the mock; the ruff shows only as a tufted lower outline, and the head's
  silhouette is otherwise a smooth ball.
- The pale-tip fringe is stepped (one tube face per tooth) and reads as stairs in the close-up face beat.
- The scuttle at chase speed (about 4 steps a second per group) is quick; at 60 fps a swing is 6 to 7 frames.
- The death's middle is under the game's puff (as for every monster).
- 32 of 32 mesh parts: nothing can be added without merging parts (e.g. the fangs into the head).
- 7,680 of 8,000 triangles.

### Polish round (2026-11-15): leg bands, ruff, launch

- **Legs painted by ring, never by face position** (`ring_paint` in `spider()`): a `M.tube` / `F.sweep` creates its
  body faces segment by segment (`i * sides + j`) and then the caps, so each segment between two edge loops gets one
  cell and every band edge is a clean ring straight round the tube. (Painting by where a face's centre lies gave a
  stair of single faces on the bends; the alternating "fringe" made the pale tips stepped; the legs' per-face shade
  cells added scattered dark faces: the legs are out of `R.shade` now.) Dark rings: the two segments meeting at each
  groove (femur 0.4 / 0.7, shin 0.25 / 0.48). Tip fade over three rings of existing cells, no new palette cells:
  `hat_violet` -> `witch_purple` -> `bat_wing` -> `bat_belly`.
- **Tips no longer frosted jelly**: shins `rim_strength` 0.06 (was 0.14) and gloss -1 (`monster_anim_spider.cpp`).
- **Ruff**: 13 tufts (was 11), 21 + 14..25 degrees long, 2.4..4.4 cm proud; the chin and cheeks show tufts in the
  silhouette. No parts added (32 of 32), 7,760 triangles.
- **Launch**: the pounce pose holds the crouch (body 13.5 cm down and back, splayed) and lets it go over 0.14 s,
  and the held leap's height is `LEAP_H * arc(u) * smoothstep(u / 0.22)`: the lift spreads over three to four
  frames instead of two. Same apex and landing time. Strip: `wip_m\strips\sp_p1_leap.png` (vs `scratchpad`
  `sp_prev_leap.png`).
- Nothing else changed (jack marking, colours, eyes, gait). Compare: `wip_m\strips\sp_compare.png`. Brief updated.
- Weak points: the three-ring fade is still visible as three soft bands up close (one cell per face); the legs'
  7-sided section shows facets in the close-up silhouette.

### Rebuild

```powershell
powershell -NoProfile -File art\team\monsters\batch04\go2.ps1 -Keys spider       # model, gates, verifier, preview
powershell -NoProfile -File art\team\monsters\batch04\build_native.ps1           # animator
tools\showcase.ps1 -Ids monster_spider -Out art\review\showcase\wip_m
python tools\showcase_sheets.py art\review\showcase\wip_m monster_spider
python art\team\monsters\batch04\strips2.py monster_spider attack_2 --n 24 --cols 8 --w 250 --from 96 --to 143 --crop 0.15,0.3,0.85,0.75
```

## Real-play review (2026-11-15/16): the attacks, hits, crowds, idles and terrain the showcase cannot show

The showcase holds a monster still through its Attack state, so the pumpkin's leap, the spider's pounce, the bat's
dive reaching the kid, hits in the middle of a wind-up or leap, crowds, long idles and uneven ground had never been
seen. This round put real monsters on the kid in a normal game, recorded them side on at 60 fps, and fixed what was
wrong in the animators. No gameplay, AI, timing or tuning changed; the frozen recorder is untouched.

### FOUND: HOP and POUNCE attacks never leave the ground in the game (gameplay bug, NOT fixed: decision 5)

**Fixed 2026-11-16 at the user's request:** the `v = get_velocity();` line after `begin_attack()` is now unconditional,
and the `TOT_MONSTER_LEAP` switch is gone (`real_test.ps1 -Leap` does nothing). The text below is the original finding.

In `Monster::physics_step` (monsters.cpp, Windup case) `begin_attack()` sets the leap velocity with `set_velocity`,
but the local `v` still holds the pre-attack velocity, and the locomotion block below writes that back
(`set_velocity(v)`) on the same tick. So the pumpkin's and the spider's attack stays on the ground: Attack lasts
about 0.155 s (9 to 10 frames: `state_t_ > 0.15f && is_on_floor()`), then the game's landing puff and the hit come.
The same code is in git HEAD (pre-existing, not from this pass). The fix would be one line (`v = get_velocity();`
after `begin_attack()`, as the Attack case already does), but it changes gameplay (leap reach, hits), so it is left
to the lead. Review switch `TOT_MONSTER_LEAP` (with `TOT_MONSTER_TEST`; `real_test.ps1 -Leap`) applies exactly that
line so the intended leap can be reviewed; it is inert without both variables.

The animators now handle BOTH paths: the leap as the game plays it today (held on the ground) and a real leap.

### The review switch: `TOT_MONSTER_TEST`

Lane M code, inert without the variable: a sibling block after the lead's `TOT_JITTER_TEST` in
`festival_game.cpp::_process` (nothing else in that file changed), `Monster::review_state()` /
`review_state_time()` and `monster_review_label()` in monsters.*, and an `MTRACE` line per monster per frame in
`Monster::_process` (where the monster is drawn on screen and where its physics body is).

`TOT_MONSTER_TEST=<type>[,count][,seconds]` in a normal game start (seed 101): every other monster is cleared (nests
asleep, thorns hidden, wild monsters cleared each frame), the kid stands still (bot input, nothing hurts them: the
monsters' `hurt_player` and the combat's player hit are no-ops), `count` monsters of `type` rise at the engage distance
(9 m up the street) and fight with their own AI. A fixed side-on camera from the far side of the street (it slides
along the street to the nearest spot with nothing between it and the fight). The top-left label shows the test time,
the engine frame and monster #0's state and time in state. Options:
- `TOT_MONSTER_SCENE=street` (default) | `kerb` (kid on the sidewalk, monsters from the road) | `slope` (steepest open
  hillside 10-20 degrees, seen across it) | `steps` (kid at a front door up the porch steps; `TOT_MONSTER_LOT=k`) |
  `idle` (the "player" 4 km away: nobody chases, monsters face the camera; fidgets) | `far` (camera 42 m down the
  street: the distance LOD).
- `TOT_MONSTER_HIT=windup:<fraction>` | `attack:<seconds>`: a player hit (0.01 damage) at that point of every second
  attack.
- `TOT_MONSTER_LURE=high`: the player position the monsters see is 40 m up (they chase to under it, never attack:
  walking across kerbs and steps).
- `TOT_MONSTER_WALK=<0..1>`, `TOT_MONSTER_DIST=<m>`, `TOT_MONSTER_AT=<0..1>` (where along the home street),
  `TOT_MONSTER_CAM=<dist>,<height>,<fov>[,<aim toward spawn 0..1>[,<aim height>]]` (default -7,0.8,40,0.12,0.45).
- `TOT_MONSTER_LEAP=1`: the real leap (above). `TOT_NO_TICK_SMOOTH=1` (the lead's): monsters drawn where physics left them.

Tools in this folder:
- `real_test.ps1 -Name <n> -Type <t> [-Count] [-Seconds] [-Scene] [-Hit windup:0.5] [-Lure high] [-Cam '-6,0.8,40,0.45']
  [-Leap] [-NoMovie] [-NoSmooth] [-Res 1920x1080]`: one run under the Godot lock through `tools\godot_bg.ps1`, movie at
  60 fps into `art\review\showcase\wip_m\real\<n>.mp4` (starting 2 frames before the test; movie frame = engine frame
  minus the start), log `<n>.log`. `-NoMovie` runs uncapped (vsync off) for the evenness trace.
- `real_batch.ps1 -Runs "<args> / <args> / ..."`: several runs in a row, each followed by `real_trace.py`. Run long
  batches detached with a log.
- `real_trace.py <log>`: per monster, the state timeline in engine and movie frames (`Attack@386(m207)`), how many
  frames were in the far LOD, and the on-screen judder of a moving monster (each frame's distance from the straight
  line between its neighbours, px; 0 = even motion, a tick sawtooth shows as 0.7 to 1 px mean at 5 ms frames).
- `real_strip.py <n> --from F --to F [--step 1] [--crop x0,y0,x1,y1] [--w 400]`: frame strips (movie frames).
- `autopilot_fps.ps1 [-Seconds 45]`: the `--seed=101 --autopilot=45` fps run at 1600x900 the earlier rounds quoted.

Scenarios recorded (`art\review\showcase\wip_m\real\`, strips in `real\strips\`): `pk_street`, `pk_leap` (-Leap),
`pk_hitw`, `pk_hita`, `pk_crowd` (4), `pk_idle`, `pk_steps`, `pk_far` (3); `sp_street`, `sp_leap`, `sp_hitw`, `sp_hita`,
`sp_crowd` (4), `sp_idle`, `sp_slope`, `sp_kerb`, `sp_steps`, `sp_far` (3); `ba_street`, `ba_hitw`, `ba_hita`,
`ba_crowd` (4); `sk_street`, `sk_crowd` (4), `gh_street`, `gr_street`, `mu_street`, `sl_street`, `sc_street`,
`ts_street` (each of the seven with a hit at mid wind-up); uncapped traces `gh_unc`, `gh_unc_ns`, `sp_unc`, `sp_unc_ns`.

### What was seen, and fixed

- **Pumpkin.** Held leap (the game today): before, nothing left the ground and the "leap" pose (arms up, stretched)
  played on the spot for 9 frames, then the slam squash. A wind-up that started in the middle of a roaming hop
  flattened the shell into a disc IN THE AIR (the wind-up's 0.74 squash mixed over the air pose). Fixed in
  `monster_anim_pumpkin.cpp`: the wind-up keeps the air pose's body (squash, rock, lean, arms, feet) while it is in the
  air (`keep_air_body`, weight `air_w_` damped 35 ms), and the held Attack is a 13 cm hop on its own clock
  (`HELD_HOP_TIME` 0.155 s, the leap pose driven by a fake vertical speed: up, over the top, down) that lands on the
  game's landing frame, so the slam squash, the puff and the hit are one frame; if knocked out of it the hop comes down
  at 2 m/s, never snaps. Real leap (`-Leap`): stretched rising with arms over the head, pitching forward over the top,
  slam with the puff; the doubled landing kick reads; no change needed. Hit mid wind-up and mid hop: blends into the
  hit (squash, eyes dark, arms up), back to roam with no stuck pose. Crowd of 4: hops and blinks out of step (each has
  its own seed). Idle: the double bounce fidget plays after about 5 s (strip `pk_idle_632_695`). Strips:
  `pk_street_176_239`, `pk_leap_200_263`, `pk_hitw_358_421`, `pk_hitw_374_437`, `pk_hita_370_409`, `pk_crowd_60_300`,
  `pk_idle_380_1010`.
- **Spider.** Held pounce: before, the leap in place ran 0.5 s but Attack ended after 0.155 s, so the game's landing
  puff came while it was 25 cm up, and the leap went on through Recover. Fixed in `monster_anim_spider.cpp`: held, the
  pounce timeline plays in `LEAP_HELD` 0.155 s at 12 cm (`LEAP_H`), ends with Attack (Recover = the touchdown, the
  land kick), the air weight lets go at 85 % so the feet are planted as it lands; leaving it early comes down at 2 m/s.
  Real pounce (`-Leap`): push, tuck rising, star at the top, reach, landing dip; real landing kick with the puff; fine.
  Feet on terrain: the planting used the terrain height only (`in.ground`), which does not know kerbs, sidewalks or
  porch steps, so at a kerb the feet on the sidewalk sank into it. New `AnimInput::surface` (monster_anim.h; filled in
  monsters.cpp by a ray down through structures and ground, layers 4 | 8) and the spider caches it on a 5 cm grid
  (`surface_y`); it falls back to `in.ground`. After: feet stand on the kerb top and on the road (`sp_kerb_236_330`).
  Slope (about 15 degrees): feet planted, hub tilts with the hill, no leg through the ground (`sp_slope_236_299`).
  Hit mid wind-up / mid leap: flails, comes down, back to the gait, no pop (`sp_hitw_390_437`, `sp_hita_412_443`).
  Crowd of 4: stepping and wiggling out of step. Idle: the slow look round seen; the front-foot tap is small at that
  size (`sp_idle_100_1000`).
- **Bat.** The dive's hit is counted while the bat is still about 0.7 m above the kid's head (the game's test is 0.9 m
  across and 1.8 m up), so it never visibly reached the kid; and a steep dive pitched it past 1 rad (spring
  overshoot), reading as hanging upside down. Fixed in `monster_anim_bat.cpp`: close to its target (under 2.6 m) the
  dive reaches the visual root toward the kid's chest (`STRIKE_AT` 0.8 m above the feet, up to 0.8 m of offset, on a
  7 Hz spring, held 0.1 s into Recover when the dive ended early = the bite), and the dive pitch is capped at 0.8 rad.
  Strip `ba_street_488_519` (before: `ba_street_484_523` in the first round, overwritten). Hit mid wind-up and mid
  dive: tumble, flap upright, fine. Crowd of 4: flaps out of phase.
- **The other seven** (one pass, a hit at mid wind-up each): skeleton (throw arc, bone leaves the hand, hit knock and
  recover), ghost (lunge, hit squint and float back), gremlin (lunge into the kid, hit), mummy (grab lunge, hit,
  stagger), slime (goo spit, squash and stretch), scarecrow (charge with dust puffs, dizzy; no hit landed: the hit is
  skipped during a CHARGE attack by design), toadstool (puff, hit squash). No pops, no stuck poses, nothing under the
  ground seen; nothing changed. Skeleton crowd of 4: out of step. Strips `*_street_*` in `real\strips\`.
- **Drawn between physics ticks.** Uncapped (about 185 fps, 5.4 ms frames, physics 120 Hz): the drawn monster's judder
  is 0.00 px mean (ghost) / 0.02 px (spider) against the body's 0.71 / 1.05 px; with `TOT_NO_TICK_SMOOTH` the drawn
  judder is the body's (0.71 / 1.05 px, 95 % 1.6 / 2.5 px). At 60 fps both are 0.1 to 0.5 px (the spider's step bob).
  The tick smoothing works for monsters.
- **Distance LOD.** `sp_far` / `pk_far`: every monster spends 711 to 720 frames in the far LOD and the state timelines
  run normally; the picture from 42 m is too small to judge the poses (camera down the street; the yard hedges hid
  the first try).

### Not verified / weak

- The real leap (`-Leap`) is reviewed but it is NOT what the game plays until the lead fixes the leap bug above; the
  held hop is a stand-in (13 cm pumpkin, 12 cm spider, landing with the puff).
- The steps scene: the camera framing at the porch is poor (porch posts, candy); the spider walking up the steps was
  not seen clearly. The surface probe makes feet follow steps by construction (same as the kerb), not by eye.
- Foot slide was judged from strips, not measured.
- The pumpkin's and spider's fidgets were seen once each in one idle run; the spider's foot tap is small at that size.
- Far LOD: only that it runs and the timelines are right, not the look.
- The bat's reach is an offset of the visual from the body (up to 0.8 m); the body itself stays where the game has it.

### Final checks (real-play round)

- Showcase re-recorded for monster_pumpkin, monster_spider, monster_bat into `wip_m` from the final DLL, beat sheets
  rebuilt (`monster_*_beats.png`); attack strips `wip_m\strips\pk_real_attack2.png` (hop up, slam, puff on the landing
  frame) and `sp_real_attack2.png` (pounce lands with the puff). Log line unchanged (`pumpkin=v2(23)` ... `spider=v2(32)`).
- No model touched: gates + `verify_rig2.py` PASS for all ten.
- `autopilot_fps.ps1` (`--seed=101 --autopilot=45`, 1600x900): 173 / 184 / 185 / 183 fps; earlier rounds 181-189 (M1),
  169-196 (M2), 163-198 (M3): no slowdown beyond noise. The spider's surface ray is cached per 5 cm cell, so a planted
  foot costs no rays.
- `game\override.cfg` absent after every run.
- Files changed this round: `native\src\play\festival_game.cpp` (the `TOT_MONSTER_TEST` block only),
  `monsters.h` / `monsters.cpp` (review accessors, label, MTRACE, the `TOT_MONSTER_LEAP` review line, `surface_` probe),
  `monster_anim.h` (`AnimInput::surface`, additive), `monster_anim_pumpkin.cpp`, `monster_anim_spider.cpp`,
  `monster_anim_bat.cpp`; tools `real_test.ps1`, `real_batch.ps1`, `real_trace.py`, `real_strip.py`, `autopilot_fps.ps1`.

Technique notes for art-pipeline: (1) check an attack in real play, not only in a recorder that holds the body: here the
game itself lost the leap; (2) when the game's attack is shorter than the visual leap, compress the leap's timeline to
the state so the landing, the puff and the hit are one frame; (3) a state layer that squashes the body must wait for the
ground when the state can start in mid air; (4) world-planted feet need the walkable surface (a cached physics ray), not
the terrain height field, or they sink into kerbs and steps; (5) a strike that the game counts at a distance needs a
visual reach toward the target; (6) measure on-screen judder as the distance from the straight line between neighbour
frames (works at any speed, picks out a tick sawtooth).

## Final fixes (2026-11-16): the reviewers' last list

All ten ids changed (the flash is shared); the others changed only where listed. Proof strips: `art\review\showcase\wip_m\strips\ff_*.png`
(after on top, final below, the same clip frames).

1. **See-through flash (all monsters).** Cause found by elimination on the spider, same clip frames: with glow off, SSIL off,
   SSR off, MSAA off, and every post effect off at once (glow, SSIL, SSAO, SSR, volumetric fog, DOF, contact AO) the parts
   still looked like glass during the flash; with the flash colour forced to zero they were solid. So it was the flash
   itself: `EMISSION += hit_flash` adds the same light to every pixel, a uniform veil that lifts dark stripes, ink eyes and
   the parts in front toward the colour behind them; at partial strength that reads as frosted glass (at full white, the
   BEFORE look, it was a solid silhouette; it is in the fading frames of BEFORE too). It is also what turned the ghost's ink
   eyes brown in attack_2 (the telegraph pulses every 1/8 s: brown, black, brown).
   Fix: new instance uniform `body_flash` in `game\shaders\toon.gdshader` (13 of 16 slots now):
   `EMISSION += body_flash * ((ALBEDO*3 + luma(ALBEDO)*1.6) * AO + rim)`: the surface's own colour brightened and whitened
   in proportion to itself, creases kept by the baked AO, a rim for the silhouette. `MonsterRig::hit_flash` drives it
   (Monster::flash / fade / gains unchanged); nests, vines and hearts keep `hit_flash`. Gains: spider 0.25 -> 0.4, bat
   0.2 -> 0.32 (with the veil gone they barely read); the rest unchanged. Strips `ff_sp_hit_1405`, `ff_sp_windup_974`,
   `ff_gr_hit_1360`, `ff_mu_hit` (pale), `ff_ba_hit`, `ff_gh_att2_eyes_1216`; real play (`-Hit windup:0.5`): spider,
   gremlin, mummy flash pale and solid at gameplay distance (`real\strips\ff_spider_hitw_393_400.png`).
2. **Spider abdomen.** `abdomen_shell` is plush (the head's short pile) instead of flat, gloss 0.22 -> -0.3 (a broad soft
   sheen, no hotspot); plum-violet cell and the carved glowing jack unchanged; 32 parts, 7,760 tris. `ff_sp_abdomen`.
3. **Gremlin.** Lunge (`bite`): claws reach low and outside the face, swipe down and in below the chin (`ff_gr_att2_1224`).
   Run (`locomotion`): both arms pump forward and back against the legs, hands below the cheeks (`ff_gr_chase_780`). Mouth:
   the cavity patch is its own part `mouth` (under head; 26 mesh parts, same 7,858 tris), gloss -2 and rim 0 in the
   animator: dark and matte side on, the tongue lolls from inside it over the lip (`ff_gr_mouth_1440`).
4. **Rattlebones rise.** `apply_pile` (rising): the skull lies BESIDE the heap on the open hand's +X side
   (`SKULL_RISE_POS/ROT`, never mirrored: on the -X side it brushed the raised bone arm), legs and pelvis stack, then the
   ribcage and arms, and the skull hops last: up beside the body, over the top of the ribcage (0.17 m arc), in and down onto
   the neck with a small nod, from 0.5 to 0.86 of the 0.6 s rise. Death unchanged. `ff_sk_rise_1600`.
5. **Ghost death eyes.** `linger_eyes`: the eyes ride the deflating sheet (`head_m * rest`, kept round), stay open a beat
   after the collapse starts, glance, squeeze shut (k 0.27-0.36) and shrink away (gone by k 0.47) before the sheet is flat.
   Never detached. `ff_gh_die_1534`. Attack_2 eye colour: item 1.
6. **Nitpicks.** Mummy: `MonsterFace::blink_speed = 1.8` (blink about 8 frames) and `closed_scale` 0.02 (closes fully into
   the dark hollow; no orange disc) (`ff_mu_blink_456`). Slime: eyes ease open over k 0.72-0.86 (about 5 frames)
   (`ff_sl_rise_face_1604`). Bat: `MonsterFace::wake_hold = 1.2` s (no blink right after waking) (`ff_ba_rise_eyes`).

New shared bits (additive, default off): `MonsterFace::blink_speed`, `MonsterFace::wake_hold` (monster_anim.h/.cpp),
`body_flash` (toon.gdshader).

Files changed: `game\shaders\toon.gdshader` (body_flash), `native\src\play\monster_rig.cpp`, `monster_anim.h`,
`monster_anim.cpp`, `monster_anim_spider.cpp`, `monster_anim_gremlin.cpp`, `monster_anim_skeleton.cpp`,
`monster_anim_ghost.cpp`, `monster_anim_mummy.cpp`, `monster_anim_slime.cpp`, `monster_anim_bat.cpp`;
`build_monsters2.py` (spider abdomen mat, gremlin `mouth` part); models `monster_spider`, `monster_gremlin` (+ catalog via
go2). `monsters.cpp`: a temporary diagnostic switch was added and removed again (no net change this round).

Rebuild: `go2.ps1 -Keys spider,gremlin`, `build_native.ps1`, `tools\showcase.ps1 -Ids <ten monsters> -Out
art\review\showcase\wip_m`, `python tools\showcase_sheets.py art\review\showcase\wip_m <ids>`.

Verified: every item by strips of the same clip frames against `after`, beat sheets of all ten rebuilt and spider /
gremlin sheets looked at, real-play hits for spider, gremlin, mummy. Not verified / weak: the flash on the white ghost and
pale skeleton reads mostly through the pose (it brightens little: already near white); the telegraph pulse is now subtle
on dark monsters (it is a gentle warm brightening, the eye flare and pose carry it as before); the gremlin tongue still
sits slightly proud of the mouth in a three-quarter view (it is a soft part lying on the face); the skull's hop path was
checked in the showcase camera only (one seed, mirrored); pumpkin, scarecrow, toadstool flash looked at in r1/final only
at the hit frames.

Technique notes (art-pipeline): (1) a hit flash as a flat added emission is a veil: at partial strength it reads as
transparency (parts in front look behind glass) and tints dark details; brighten the surface in proportion to its own
albedo (and AO) instead; (2) find a render artefact by elimination on deterministic frames: switch off each post effect,
then the effect's input (here the flash colour) — glow, SSIL, SSR, MSAA were all innocent; (3) a reassembly must route
the last part around what is already stacked (beside, up, over, down), not along a straight line through it; (4) parts
that "linger" after a collapse must stay attached to the surface; let them close and shrink instead of floating.
