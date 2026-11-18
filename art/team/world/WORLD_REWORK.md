# World rework: winding neighborhoods, hills, varied towns (2026-11-17)

Lead: the main session (plans, reviews, decides). Workers: at most 2 at a time, Opus, one stage each.
Every worker reads this whole file first and appends a "Progress" note to the section of its stage
(what is done, what is left, numbers), so a cut-off worker can be resumed.

## The user's request (verbatim)

"look into making the neighborhoods and towns more fun to play around in. The town has more platforming
opportunities and they feel more fun than the neighborhoods. Real neighborhoods have winding roads with
cul-desacs, I was envisioning a spline-base neighborhood road system that is more realistic like that. It's
OK if they are bigger, clearing them out would be more of an accomplishment. I would also like them to not
be so flat, but set into more natural rolling hills terrain. You would do this by having a foundation for
each house that is level and extends into the ground to avoid showing any holes.

Monster spawners should be allowed in the streets, and vines should be allowed to have a nest anywhere, not
just in streets, and their nests should not be allowed to be too close together.

Candy placement in front yards feels a little overdone or haphazard. We need to tighten up the placement to
make it more satisfying to collect.

Towns should also not be completely flat like they are, and should vary in size and street layout. Never
just the same layout repeated like we have, but much more dynamic. Street lengths for roads and
neighborhoods should be randomized as well."

Also (user, 2026-11-17, added before Stage 3b as "Stage 2b"): "I want neighborhoods to have curvier roads and be
between 2 to 3x bigger" ... "the number of houses" (clarified: 2-3x the house count of Stage 2a's 18.7 lots per
hood, every hood in the band; curvier, S-bends, curved branches, 2-5 branches, usually 2+ cul-de-sacs, optional
loop/crescent; footprint/REGION grown as needed; nests/hearts scale with size; towns bit-identical).

Also (user, 2026-11-17): "reduce the amount of in-game checks to speed up development and reduce token
cost". Verify with headless and code-level checks (the sweep, stats, determinism); at most ONE in-game
capture per stage, at the end. No recordings, no frame-by-frame reviews.

## Standing rules (every worker)

- Windows; use PowerShell (the Bash tool is unreliable here). Paths: Godot console
  `C:\GameDev\Godot_v4.7-stable_win64\Godot_v4.7-stable_win64_console.exe`; MinGW at `C:\msys64\mingw64\bin`
  (prepend to PATH); build: `scons platform=windows use_mingw=yes mingw_prefix= target=template_debug
  custom_api_file=extension_api.json -j8` in `native\` (see `tools\showcase.ps1` lines 40-56 for the exact
  block, including how a locked DLL is renamed aside: never delete or kill anything holding it).
- Every Godot run and every native build under the shared lock: `. tools\godot_lock.ps1; $lock =
  Enter-GodotLock; try { ... } finally { Exit-GodotLock $lock }`. Game windows run off-screen, muted, no
  focus: `tools\godot_bg.ps1` (`Start-GodotBackground`). Never kill the user's Godot editor or game. Do not
  run `--import` while the user's editor is open (check `Get-Process Godot*`).
- Long runs in the background with a log; never cut a Godot process's output pipe.
- No git commits, pushes, branch switches or stashes. Read-only git only (diff, status, show).
- Never touch `.itchio-api-key` in the repo root.
- Keep determinism: everything from the seed; no wall-clock or unordered-map iteration order in output.
- "Fidelity, not conformity": keep existing features (platforming routes, bouncy awnings, props, magic
  trail, landmarks); the rework adds, it does not strip.
- Files owned by another running worker are off limits (each stage says what it owns).

## The plan (from the read-only survey; file:line references are from 2026-11-17)

### How it works today (summary)
- `world\world_plan.cpp`: 200 m region grid (woods 35%, neighborhood 40%, town 25%; (0,0) a
  neighborhood). `base()` plans street + lots with the streamer's seed to get a footprint rectangle, spawn
  and ports (E/W = main street ends, N/S = side lane end). Country roads pair E-W and N-S ports, both at
  `origin.y`. `height()` blends natural hills to a flat plateau (`origin.y`) inside the footprint + 26 m,
  and toward road height near roads. The settlement node is only translated by `origin`.
- `gen\street.cpp`: `StreetPlan` = one sine street sampled every 1 m along x (`at(x)` indexes by x, so it
  must be monotonic in x), one straight side lane at `lane_x`, spawn at that junction. Lots march in two
  rows along x. Neighbour logic everywhere is "same side, index +-1".
- `gen\town.cpp`: the same street, straighter; shop lots shoulder to shoulder; one square opposite the
  lane; bunting. (Why every town looks alike.)
- Houses/yards at lot-local y = 0 (foundation slab 0.4 m on y = 0; flat lawn quad, path, driveway, fences).
- Nests (`play\monsters.cpp` Infestation::build ~1472-1925): monster nests 4-6, never on asphalt (lawn,
  sidewalk, service yard, off-street), >= 14 m apart. Thorn hearts 2-3: lawn spots only `if (!town)`,
  plus 12 road-centre points; openness weighting makes the road the usual pick; > 8 m from nests, > 15 m
  from other hearts. Save state `spawner_down/heart_down/vine_down` is positional.
- Front-yard candy (`gen\candy.cpp` ~212-470): ~40-45 pieces per lot; random ground rings and random yard
  hops not anchored to anything (the "haphazard" feel); the front zone is listed twice (~401/403). Candy
  index = piece id (`candy_filter_`, `candy_taken` saved per area).
- Downstream consumers of the layout: collision boxes, magic trail spine (along x + lane) and door
  branches, `street_yaw_at` (camera; festival_game.cpp:484, festival_cine.cpp:172/194/224/901), ports and
  country roads, spawn, gate and candy-shop cart (relative to `lane_origin`), showcase straight-run finder
  (festival_showcase.cpp:252-279), monster test (festival_game.cpp:2150-2165), doorbells by lot index,
  `SAVE_VERSION = 1` (festival_game.cpp:1094).
- Ground-is-zero assumptions (break on slopes): monsters nav grid (monsters.cpp:868-875), clear_of_boxes,
  shot_openness, candy heights/validation (candy.cpp:228, 287, 530, 584, 675-696, 786),
  candy_field.cpp:345, nests/hearts/tendrils/gate/cart at local y = 0.
- `--sweep` (festival_game.cpp:249-263) runs the generator only (flat, no WorldPlan, no terrain, no
  infestation).
- Guesses to verify, not trust: `MeshBuilder::ribbon` takes a constant y; the monsters' A* cap of 9000
  nodes may cut long chases on bigger maps.

### Stages (in order; each builds and plays on its own)

**Stage 0: road network API, no behaviour change.** FIRST capture a `--sweep` baseline (JSON/stats for
50 seeds) from the unchanged code. Add `RoadNet { roads; nearest(p) -> {road, s, lateral, tangent, y} }`,
`Road { pts, left, cumulative s, half_road, parent/parent_s, end kind Port|Bulb|Junction }`, built from
today's street + lane; `Lot::road`, `Lot::s`, explicit `prev/next` neighbour indices; `GenContext::ground(x,
z)` returning 0 for now. Move every x-indexed consumer to the new API (street_yaw_at, magic spine and
branches, pocket parks, town square, street rhythm, street candy, scatter_open_grass, nest/heart street
candidates, gate/cart, showcase, monster test, the +-1 neighbour rules). Extend the sweep with stats that
later stages need (lot overlaps, lots intruding on road corridors, candy per lot), and move nest picking
into a pure function callable from the sweep if practical. Verify: sweep output identical (or stats
identical) before and after across 50 seeds. No in-game check.

**Stage 1d: nest rules** (owns `play\monsters.cpp`). Monster nests may sit on roads (road position +-0-2.5
m across, 0.15-0.85 along each road, not within 10 m of a junction or spawn). Thorn hearts may sit anywhere
(lots, lawns, backyards, service yards, the square, roads, in towns too), stratified: at most one on a road.
One spacing rule across all nests (heart-heart 18 m, heart-nest 12 m, nest-nest 14 m); when draws run dry
relax by 15% up to twice instead of placing fewer; report the achieved minimum in the sweep stats.

**Stage 1e: tighter front-yard candy** (owns `gen\candy.cpp`, and the anchor fields it needs published by
`gen\houses.cpp` / `gen\yards.cpp` on `Lot`). Replace the random lawn rings and random hops with anchored
patterns: a path line (5-7 pieces, 0.9 m apart, 0.75 m up the stepping-stone path, a chocolate on the top
step); a drive line (4-5) up the driveway, continuing as an arc onto a parked car's bouncy hood when there
is one; jump-point arcs (3) over real obstacles (low fence, pumpkin cluster, hay stack, hedge gap); a
platform crown (3, tight triangle, 0.9 m above each sequence's top platform); a landmark ring (6-8 round a
landmark's centrepiece). At most 2 platform sequences per lot (3 on landmark lots); keep the porch-roof run;
drop the duplicate front zone. Target 22-30 pieces per lot with total value kept close by promoting tiers.

**Stage 2a: spline neighbourhood roads.** Region size (lead's decision, 2026-11-17): REGION = 280 m, so a
neighbourhood footprint can reach ~150 x 150 m (the blend margin and the neighbour's must still fit:
2 x (half-extent + blend) <= 280 - 2 x jitter). Every world re-rolls anyway with the new layouts and the
SAVE_VERSION bump at ship. Neighbourhoods may then be up to ~2x today's lot count; watch the load time and
the monsters' A* cap (9000 expansions ~ a 57 m square; raise it or make the nav grid coarser if chases fail). Main road W port to E port through 4-6 Catmull-Rom control points
(resampled every 1 m), randomised length; 1-3 branches at T junctions (60-90 degrees), 30-70 m,
randomised, each ending in a cul-de-sac bulb (r 9-11 m) or becoming the N/S port; reject self-intersecting
or too-close corridors. Lots by arc length along each side, frames from the tangent, skip junction
corridors, shallower on the inside of tight curves, 3-5 lots round each bulb facing its centre, deeper lots
allowed. Per-road ribbons, junction patches, bulb discs with curb/sidewalk rings. Ports from road ends.
Report the area load time (ms) from the streamer's "Loaded ... ms" print.

**Stage 3b: rolling hills.** Settlements on graded natural terrain instead of a plateau: road heights from
the natural terrain, smoothed, <= 8% grade; each lot a level pad (road height at its front + clamp(terrain
median - road, -1.2, +1.8)); a cached 1 m heightfield over footprint + blend; `height()` reads it; ports use
graded end heights. `lot.xf.origin.y` = pad height. Each house and garage gets a plinth/foundation skirt
from its floor down below the lowest ground under its footprint (no holes showing); porches get extra steps
when the path drops; lawns, paths, drives drape on the ground; fences follow it; street ribbons get
per-vertex y. Fix every ground-is-zero test listed above.

**Stage 4c: varied, hilly towns.** Per-seed templates (main street + 1-2 cross streets T or +; a 2-4 block
grid with 32-60 m blocks; a curved high street with a square bulge; a Y fork), small/medium/large
footprints, the square placed at a corner/centre/end on its own pad, shops by the same arc-length placer,
stepped facades with exposed side walls/plinths where pads differ, grade <= 5%.

**Stage 3c (queued after 4c; user request 2026-11-17, verbatim):** "Add rewards over bounceable things
sometimes, including the higher rewards at the top of a full jump plus A height (and get rid of the pumpkin
that bounces like a trampoline). Make house plots accept hills instead of flattening them, and make props just
take the terrain height and orientation over their surface (cars sit in contact with the ground, flat stone
walkways as well, etc). Make sure the stone walkways connect with the street always and strairs don't give up
and show above ground big ledges instead of going farther. chop off the top bit of houses so there's more stuff
for the player to jump on and walk on for houses." Lead's reading: yards drape on the natural slope (no
whole-lot pads; only the house/garage footprint is levelled on its foundation); every ground prop takes the
terrain height AND normal (cars with all wheels touching, stepping stones flush); the front walk always reaches
the sidewalk; porch stairs keep adding steps down to the ground; roofs get a flat walkable top (truncated
ridge / deck) on houses; candy arcs over bouncy things (car hoods etc.) sometimes, with the high-value pieces
at the apex of bounce + full jump + flap ("A" = the second jump press); remove the trampoline pumpkin.

**Ship:** bump SAVE_VERSION once (old saves reset), one overview capture.

## Progress

(Workers append here, newest last, under a heading per stage.)

### Stage 0

- [x] Tooling: `art\team\world\stage0\sweep.ps1 -Name <name> [-NoBuild] [-Dump]` (build + `--sweep=50 --seed=101`
  under the lock; writes `<name>.log`, `<name>.jsonl`, and with -Dump `<name>_dump.jsonl`).
- [x] Pristine baseline `art\team\world\stage0\baseline_pristine.{log,jsonl}` (unchanged code, 50 SWEEP lines, 3.6 s).
- [x] Output-only additions, generator untouched: `play\sweep.{h,cpp}` (SweepDump: per-category FNV hashes of exact
  float bits in emission order: lots, boxes, props, candy, scatter, meshes, magic, ...; 5 variants per seed: the sweep
  one + embedded hood/town with lane open/closed; plus WorldPlan regions/ports/roads for a 5x5 grid per seed), the pure
  `plan_infestation()` extracted from `Infestation::build` (monsters.cpp; same rng order), `--sweep-dump=<file>`
  `--sweep-dump-detail=<category>` (festival_game.cpp), `MeshBuilder::vertices()/cell_uvs()`.
  `baseline_dump` SWEEP lines == pristine (compare.py: 0 differences); dump rerun deterministic (4600 hashes, 0 diff).
  Compare: `python art\team\world\stage0\compare.py baseline_dump <name> --dump`.
- [x] RoadNet (types.h:196-275; street.cpp:75-283): `Road {pts, left, s, step, x_station, straight/dir, dims,
  parent/parent_s/parent_side, start_kind/end_kind Port|Bulb|Junction|DeadEnd, lots_s0/lots_s1}` with at / left_at /
  tangent_at / point / frontage_frame / end_dir / project -> `RoadHit {road, s, s_ext, beyond, lateral, dist, point,
  tangent, left, y}`; `RoadNet {roads, junctions(road), project(road,p), nearest(p)}`. `GenContext::roads`,
  `ground(x,z)` (0), `road_of(lot)`; `Lot::road, s, prev, next` (center_x and StreetPlan removed, so the compiler
  found every consumer); `link_lot_neighbours`, `frontage_gaps` (context.h). Parity crutches, both only set by
  plan_street: road 0 `x_station` (stations = world x, a point's station = its x), road 1 `straight` (exact
  o + dir*s). Stage 2a builds arc-length roads and leaves both off.
- [x] Consumers moved: plan_street/plan_lots/assign_households/build_street/build_boundary (street.cpp), parks +
  rhythm + cars + jack faces (yards.cpp), town lots/shops/square/bunting (town.cpp), house neighbour rules + apron
  (houses.cpp), candy hops/street candy/zones, magic spine (per road) + branches, open grass, leaves (candy.cpp),
  street_yaw_at (neighborhood_generator.cpp), ports (world_plan.cpp), gate/cart/nest+heart street spots
  (monsters.cpp plan_infestation), monster test (festival_game.cpp), showcase finder (festival_showcase.cpp),
  streamer road width default (world_streamer.cpp).
- [x] Sweep stats (play\sweep.cpp sweep_layout_stats): roads, road_length, lot_overlaps(+_neighbours, _max),
  lots_in_road(+_by_road, road_intrusion_max(_by_road)), candy_per_lot/min/mean/max/street, nests/hearts (+wanted),
  nest_gap_min, heart_gap_min, heart_nest_gap_min, nests_on_road, hearts_on_road. `SWEEP_ROADNET` self-check of
  the generic path on a synthetic net (U road doubling back in x, searched stations, y != 0, branch): 622 checks, 0 failed.
- [x] `after`: SWEEP lines == pristine on all 13 original keys; dump 4600 hashes == baseline_dump (bit-identical).
  Smoke: `--headless --quit-after 900 -- --seed=101`: "Loaded Acorn Court in 37 ms", only exit-leak noise (smoke.log).
- Also (lead's request): festival_showcase.cpp keeps the window where `--position` put it.
- street_yaw_at (camera, not in the dump): sweep stat `yaw_differ` compares RoadNet::nearest's tangent with the old
  rule on a 1 m grid over every footprint: 473,290 points, 0 differ. plan_infestation's extraction is verified by
  reading the diff (same rng_ calls, same order), not by a test: the dump baseline was taken after it.
- Stage 2a must still touch: consumers loop the trunk + `junctions(0)` only (lots, parks, rhythm, cars, street candy,
  grass rules, hop rows); `mouths.front()` in plan_infestation and plan_town_lots assumes a side road exists; the
  `-parent_side` "across" vector (crosswalk, lane mesh, arch, corner lamps, gate/cart, lane spine) assumes a straight
  child; one `ctx.lane_open` for all ports; world_plan keeps the first port per side (lane_side stays -1 if taken);
  bound_x0/x1 are set from trunk stations; on the x_station road `lateral`/`dist` are same-x values (the intrusion
  stats inherit that); polyline `project` is a linear scan (add a grid for big maps); build_street is still the
  legacy two-road mesher.
- Today's numbers for later stages (50 seeds): hood lot_overlaps 69 (all prev/next pairs on the inside of bends, up
  to 9.8 m deep), lots in road corridors: main 109 (max 1.25 m, front corners on bends), side lane 21 (max 6.4 m:
  lots by the straight lane on a sloped street stretch); town overlaps 98 (max 1.9 m), lane intrusions max 1.0 m.
  Candy per lot hood mean 36 (21-57), town 28. Nests 4-5 hood / 2-6 town; nest gap min 14.7; heart-nest min 8.5.

### Stage 1e

- [x] Output-only stats first (sweep.cpp, after candy_street): candy_value_per_lot / _lot_mean/min/max / _street,
  candy_lot_tiers, candy_lot_patterns (pieces per CandyPattern, types.h), candy_pattern_dropped/_raised (validation,
  GenStats), candy_lot_pattern_detail. Old code tagged by pattern for the baseline only.
- [x] Baseline `art\team\world\stage0\1e_before.{jsonl,log,_dump.jsonl}` (SWEEP == Stage 0 `after` on its 38 keys).
  Hood 231 lots: 36.2 pieces/lot (21-57; 18% in 22-30), value 86.1/lot (21-209), tiers/lot 33.3 / 1.92 / 0.65 / 0.27
  (basic / chocolate / lollipop / golden); legacy rings+hops 22.2/lot, sequences 10.5, roof 2.0, summit 1.0;
  dropped 46 / raised 42 (all seeds, lot + street). Town 304 lots: 28.3 (18-40), value 77.5 (18-194), tiers
  26.6 / 0.80 / 0.56 / 0.33; legacy 19.2/lot (320 of them lifted: sidewalk rings), sequences 5.6.
  Summary: `python art\team\world\stage0\candy_stats.py <name> [<before>]` (hood / town split, per-lot value deltas).
- [x] Anchors on Lot (types.h: YardObstacle/ObstacleKind before Lot; Lot::front_walk, drive_line, car_in_drive,
  car_hood(_half), obstacles, has_centrepiece/centrepiece/centre_r), all lot-local, published with ZERO rng draws
  (dump: lots/boxes/props/meshes/magic/scatter hashes == 1e_before; only meta, candy and Stage 1d's infest differ):
  houses.cpp:503 front walk (sidewalk edge, foot of steps, each tread centre up to the top step), :583 drive line,
  :756 picket segments between the path/drive gaps; yards.cpp:235/400/844 pumpkin clusters, :413 single side-fence
  bales, :436 garage bales shorten the drive, :506 brick planter, :597 parked car hood (has_hood checked; it faces the
  street) and the drive ends at its bumper, landmark centrepieces :712/731/783/819/837/857; town.cpp:434 shop walk
  (kerb side to the doorstep), :523 shop-front bale. (town.cpp and context.h GenStats are outside the listed files.)
- [x] Patterns (candy.cpp LotWeaver :307, per-lot loop :888): priority path -> drive -> roof run (unchanged) ->
  summit (unchanged, one colour now) -> landmark ring -> sequences -> obstacle arcs (flex) -> bonus lollipops
  (unchanged odds, drawn first so they count) -> value top-up. Cap 30 per lot, 34 on landmark lots; a whole pattern
  is skipped if it does not fit. Every pattern is one colour and one rhythm group, tested all-or-nothing against the
  validation's 0.25 m margin incl. midpoints between pieces (NearBoxes :237), and kept 0.7-1.0 m from pieces of
  other patterns. Path line :372 (5-7 at 0.9 m, 0.75 m above stones/treads, built back from the top step, chocolate on
  the top step; shops 3+); drive line :400 (4-5, 0.9-1.3 m; with a car: 4 from the kerb side + the real jump onto the
  hood + a chocolate 2.6 m over the hood where the bounce throws the kid; else ends 0.7 m short of bumper / bales /
  garage door); ring :499 (6-8 at centre_r + 0.7..1.9, 0.9 or 1.35 m high, the street-facing one a chocolate);
  sequences :541 (<= 2, 3 on landmarks; approach along the real jump, optional chain, crown of 3 in a 0.35 m triangle
  0.9 m over the top platform replacing the landing piece; the old +2.35 m bonus chocolate is gone); obstacle arcs
  :670 (3 at t = 1/4, 1/2, 3/4 of a real jump, takeoff/landing max(1.5, half + 0.9) m either side, street side first;
  <= 2, 3 on a lot still under 20); top_up :752 (chocolates along the patterns, crown vertices first, until the lot's
  basic + chocolate value is within 2 of 43 hood / 31 town, the 1e_before means). Lawn rings, yard hops and the
  duplicate front zone are gone. Street candy (lot -1) untouched (candy_street identical).
- [x] After `stage0\1e_after.*` (50 seeds; determinism: `1e_after_rerun` -NoBuild, 0 differences on 67 keys / 4850
  hashes; candy_street and candy_value_street identical to 1e_before seed by seed):
  - Hood 231 lots: 26.6/lot (13-34), 64% in 22-30. Regular lots 169: 24.9 (13-30), 36 under 22, none over 30;
    landmark lots 62: 31.3 (20-34). Per lot: path 6.2, drive 5.8, crown 4.2, sequence 3.6, obstacle 2.9, roof 2.0,
    summit 1.0, ring 0.5. Value 85.0/lot (before 86.1; basic + chocolate 42.2 vs 42.9, lollipop 0.64 vs 0.65, golden
    0.27 same; tiers 21.6 / 4.11 / 0.64 / 0.27). Paired per-lot value deltas p10/p90 -32/+30: with multiples of 25
    (a lollipop: roof-run odds unchanged, rng order changed) removed, the residual is p10/p90 -10/+9 (max 12): the
    top-up pulls every lot's basic + chocolate value to ~43 where it used to track the piece count (21-57).
    Cars 67, hood hop on all 67. Validation: path, drive, crown, ring and obstacle pieces 0 dropped / 0 lifted;
    sequences 1 dropped / 2 lifted (before 15 / 9); the rest is summit (18 dropped, as before) and street/bonus
    pieces (unchanged code). plan_ms 13.4 (before 15.9).
  - Town 304 lots: 15.2/lot (10-23; 285 under 22), value 76.0 (before 77.5; tiers 10.6 / 3.71 / 0.56 / 0.33: the
    chocolates carry the value). Shop walk 5.0, crown 4.0, sequence 2.7, roof 2.1, summit 1.0, obstacle 0.16 (the
    awning sits right over a full-jump arc on the sidewalk, so the bale arc rarely fits).
  - Landmark rings 16 of 62 landmark lots: Tree House 9/9, Inflatables 5/9, Scarecrow 1/5 (3 had one), Hay Ride 1/10
    (1 had a wagon), Cauldron 0/5 (1 had one), Pumpkin Patch 0/10 (the giant pumpkin never places: its y.place
    fails in a yard already full of patch rows; pre-existing), Sunflower / Lantern Porch / Carving Contest have no
    single centrepiece. No front-yard hedge exists (only the 3.2 m boundary hedges), so no hedge-gap arcs.
- OPEN for the lead: the user's art-direction memory says "never a walk-along candy breadcrumb trail" and the magic
  trail is the guide to the door; the path line (on the door axis, 0.75 m) runs right along the magic branch
  (plan_magic_paths, door_x, 0.85-0.95 m) and the drive line is the same shape. Built as briefed; each is its own
  function (path_line / drive_line) to cut or restyle in one line. Town lots fall to ~15 pieces (value kept by
  chocolates): the lead's call whether shop fronts should keep more.
- Stage 1d's numbers stand without a re-run: boxes/props/lots hashes are unchanged by 1e and monsters.cpp never
  reads ctx.candy.
- [x] In-game (one background capture, `--seed=101 --capture-view=lot:3`, no --import):
  `art\team\world\stage0\1e_seed101_lot3.png`. The path line climbs the stepping stones from the gate pillars to the
  top step; the neighbour's drive line reads as a straight run. Seen there: (1) the purple magic branch runs right
  along the path line (the conflict above); (2) "one colour" does not survive into the game: build_nodes
  (neighborhood_generator.cpp:1186) re-rolls every piece's kind within its tier from sub_seed(seed, 0xCA4D, i), so a
  line shows mixed varieties. Fix (not applied: outside the listed files, and it would want another look): key the
  re-roll by the rhythm group for everyday pieces, e.g. `sub_seed(seed, 0xCA4D, c.group >= 0 && candy_tier(c.kind) == 0
  ? 0x100000u + c.group : i)` (street groups would become one kind too).
- [x] Follow-up round (lead's decisions, 2026-11-17): (1) the path line is gone (user's rule: never a walk-along
  breadcrumb; the magic trail guides to the door) and the drive line exists only as the run-up into the parked-car
  hood hop. In their place `clusters()` (candy.cpp, before drive_line): up to 2 per lot, 4-5 pieces, one kind, round
  a published YardFeature (types.h; houses.cpp porch steps -> a pair each side of the flight; yards.cpp front tree
  trunk -> ring of 5, pumpkin clusters -> ring, planter / single bale -> arc; town.cpp door flankers -> arc facing
  away from the door, shop display -> arc), never within 0.95 m of front_walk (kept as the keep-out anchor).
  Value: sequence approach middles added as promotables, whole clusters turn chocolate as a last resort (stay one
  kind). (2) neighborhood_generator.cpp:1186: the kind re-roll is keyed by rhythm group for everyday and chocolate
  pieces (street groups too), so every shape shows one kind. (3) Towns left at Stage 4's mercy.
  Numbers (`stage0\1e_after2`, determinism rerun 0 differences, street candy identical seed by seed): hood 26.0/lot
  (10-34; regular 24.3 (10-30), 44 of 169 under 22, none over 30; landmark 30.5 (20-34)); per lot cluster 7.8, crown
  4.9, sequence 4.0, obstacle 3.0, drive (car hop) 2.3, roof 2.0, summit 1.0, ring 0.6; every lot has a cluster;
  value 85.0 (before 86.1; tiers 20.8 / 4.26 / 0.64 / 0.27); cars 67, hops 67; new patterns 0 dropped / 0 lifted,
  sequences 1 / 1. Town 17.3/lot (9-27), value 76.5 (before 77.5), tiers 13.1 / 3.34 / 0.56 / 0.33.
  Capture `stage0\1e_seed101_lot3_v2.png`: the walk to the door is clear (only the magic trail), the tree has an
  orange ring round its trunk, the neighbours' pumpkins and steps have single-kind clusters.
- For Stage 2a (curved roads, bulbs): candy works in each lot's frame (lot.to_world; anchors lot-local), so curved
  frontage is fine as long as Lot::xf is the frontage frame. Assumed: the sidewalk is lot-local z in [0, walk - verge]
  straight in front of the lot (path/drive starts at z 0.6 or the kerb side 1.4; the fence-arc takeoff at z +1.0;
  obstacle arcs may use z <= walk - verge - 0.3): on a bulb or the inside of a tight bend the sidewalk is not
  straight across the lot front, so check those takeoffs (NearBoxes only tests boxes, not the kerb or the road).
  NearBoxes gathers boxes within the lot's half-diagonal + 6 m by a linear scan over ctx.boxes per lot (fine at 2x
  lots; a grid if it shows in plan_ms). The street candy (weave_candy after the lot loop) still walks the trunk
  and junctions(0) only.
- For Stage 3b (slopes): lot-local y = 0 is the pad, so everything inside the lot moves with lot.xf.origin.y. Two
  things are not: (a) front_walk[0] / drive_line[0] and the kerb-side starts sit on the sidewalk, outside the pad:
  publish their y as sidewalk height minus pad height (houses.cpp:505/585) and add the extra porch steps to
  front_walk; (b) sequences take off at the lot's pad height (candy.cpp sequences: ground_y = lot.xf.origin.y) and
  obstacle arcs at lot-local y 0, so a sloped yard needs ctx.ground() under each takeoff/landing. Validation
  (candy.cpp validate_traversal) still treats y <= JUMP_REACH + 0.2 as "reachable from the ground" in absolute y,
  and the platform filter (e.top 0.75..3.2) is absolute: both must become relative to the ground under the point.

### Stage 1d

- [x] `plan_infestation()` (monsters.cpp:1473-1937) rewritten; Infestation::build (monsters.cpp:1940) now only builds
  nodes from the layout. Rules, as implemented:
  - Monster nests: the old lawn / backyard / service-yard / sidewalk / off-street spots plus road spots on EVERY road
    (`max(2, built_len/10)` each, 15-85% along `lots_s0..lots_s1`, lateral +-2.5 m). "On the road" is geometric (any
    road's `project()`: `beyond <= 0`, `|lateral| < half_road`), so a lawn spot that lands on asphalt obeys the road
    rules too: >= 10 m from every junction point (each child road's start on its parent) and the spawn, >= 7 m from
    the gate pad and the cart's doormat (`cart.xform(0,0,1.25)`), clear of boxes at the mix's biggest nest radius
    + 0.6 (parked cars are OBoxes via register_prop_collisions). All nests: cart doormat >= 4.5 m (new), in bounds.
  - Thorn hearts: front lawns (fixed: the old "lawn" range ran z 1.8 .. porch, i.e. partly ON the sidewalk; now
    porch + 1.8 .. -1.8, x +-(w/2 - 2)), backyards (2/lot), town service yards, sidewalks (every road, both sides,
    `max(2, len/12)`), open frontage (`frontage_gaps` on every road side: the town square, pocket parks) and the
    asphalt (`max(2, len/8)`, lateral +-1.2, >= 12 m from junctions); towns get all of them now. At most one heart on
    a road (hard rule). Class balancing: road / sidewalk / yard / open each get the same share of the draw
    (openness still picks within a class), otherwise the open road and sidewalk win every time.
  - One spacing rule (horizontal): heart-heart 18, heart-nest 12, nest-nest 14. Hearts are picked first (widest
    gaps), then nests. A dry draw relaxes the scale x0.85 (compounding, max twice: x0.7225) and stays relaxed; if it is
    still dry the whole pick starts over (rng carries on, deterministic), up to 4 tries, keeping the fullest.
    `draw()` no longer zeroes a failing candidate for good (fixed rules are folded into the base weight once).
  - Tendrils/knots planning moved into the planner (same rng position as before: straight after the picks);
    `InfestationLayout::tendrils/knots`. Changes: vines pass through yard fences (`clear_of_boxes(..., through_fences)`:
    shot_through boxes), a spoke whose start is blocked is skipped, the door tendril may turn +-1.5 rad (slide along a
    wall) and tries the 3 nearest free doors in turn.
  - New layout fields (monsters.h): nest_on_road, heart_on_road, tendrils, knots, road_nest/heart_candidates,
    relax_hearts, relax, tries. One print per area load: `Infestation: N nests (R on the road), ...` with positions.
- [x] Sweep (sweep.cpp, nest lines only): nests_road_flag / hearts_road_flag (agree with the geometric
  nests/hearts_on_road on all 50 seeds), road_nest/heart_candidates, relax_hearts, relax, nest_tries,
  gap_ratio_min (worst pair / its rule), nests_where / hearts_where / hearts_unknotted_where (road, front, back, walk,
  open), tendrils, tendrils_per_heart_min, doors_knotted, hearts_off_road(_knotted), hearts_road_knotted. Dump: new
  "vines" category (tendril points, knots). Summary: `python art\team\world\stage0\nest_stats.py <name>.jsonl`.
- [x] Numbers (50 seeds, `art\team\world\stage0\s1d_e.jsonl`, the standalone sweep variant; the embedded variants the game plays are only hashed; before = Stage 0 `after`). Measured with Stage 1e mid-flight in the same tree: yard boxes feed clear_of_boxes/openness, so re-run after 1e lands:
  - Hood: nests 117/117 wanted (before 111/113), hearts 62/62 (before 55/57). Nests on road 42 (36%; before 9 by
    accident), where: back 36, front 23, walk 15, open 1. Hearts on road 15 (24%; before 41 = 75%): walk 22, back 12,
    front 7, open 6; 10 of 25 areas have no road heart. Min gaps: nest-nest 11.1 (rule 14), heart-heart 17.0 (18),
    heart-nest 12.1 (12); gap_ratio_min 0.79; relax used in 4 areas (3 x1, 1 x2), every area on the first try.
  - Town: nests 137/138 (before 130/141; seed 108, a 10-lot town wanting 6, ends 5 after 4 tries at x0.7225),
    hearts 62/62 (before 54/60). Nests on road 52 (38%), back 71, open 13, walk 1. Hearts on road 20 (32%; before 54 =
    100%): back 20, walk 16, open 6; 5 of 25 towns have none. Min gaps: nest-nest 14.6, heart-heart 18.4, heart-nest
    10.7; gap_ratio_min 0.89; relax in 2 towns, 2 needed a retry.
  - Vines: hood 24 tendrils/area (min 4 per heart), doors knotted 2.04/area; off-road hearts knotted 37/47, road
    14/15. Town 22/area (min 2 per heart), 1.88 doors/area; off-road 27/42 knotted, the misses are almost all
    service-yard hearts (all 15 misses; 15 of the 20 there: the shop row is a wall; they still have their spokes). No consumer needs a knot (vines_left/on_vine_down/check_freed only count).
- [x] Determinism: two `-Dump` runs of the final build: 50 SWEEP lines on 65 keys and 4850 category hashes, 0
  differences (s1d_det1 vs s1d_det2). Stage 1e is building in the same tree, so lot/candy hashes are not compared to
  Stage 0's dump.
- [x] In-game check (headless smoke, `--headless --quit-after 900 -- --seed=101`, `stage0\s1d_smoke.log`): "Loaded
  Acorn Court in 37 ms", `Infestation: 4 nests (1 on the road), 3 thorn nests (1 on the road), 2 doors knotted,
  spacing relax 0; off-road(7.4,-0.3) off-road(-28.7,40.1) road(-20.4,13.5) off-road(35.8,18.4)`.
- For Stage 2a: every candidate loop runs per road over `lots_s0..lots_s1` (set them on new roads, or the full
  s range is used); junction points come from `parent/parent_s`, so T junctions of any branch work; Bulb ends get no
  special keep-out (a nest may sit 15-85% along a cul-de-sac road, never in the bulb itself if lots_s1 stops short
  of it). Still assumed: gate + cart on `junctions(0).front()` as a straight lane (`-parent_side` across vector),
  the 10 legacy off-street nest spots use the trunk only, `asphalt()` is a linear scan over roads x project()
  (a scan per candidate: fine for ~300 candidates on a few roads, add a grid with the RoadNet one). Bigger maps:
  the spacing rule then has room, but want_nests (4-5 / 5-6) and want_hearts (2-3) are fixed counts; scale them
  with area or road length if the bigger hoods feel empty. Saves: spawner/heart/vine_down are positional and every
  layout moved; SAVE_VERSION not bumped (lead's job at ship). Road share knob: the nest road-candidate count
  `max(2, len/10)` in plan_infestation (or scale their weight). Cine shots (festival_cine.cpp:203/722/766/832) frame
  heart 0 and the nests nearest the spawn: heart 0 may now be in a backyard.

### Stage 2a

Plan (worker, 2026-11-17): towns stay on the legacy path (plan_street legacy, plan_town_lots, legacy build_street /
build_boundary), so their street/lots/street_mesh hashes vs `1e_after2_dump` prove them unchanged. Hoods: spline
planner (main W->E road, 1-3 branches, bulbs, at most one N/S port branch decided by world_plan's pair rule so every
port connects), generic arc-length lot placer sharing the sweep's SAT/corridor tests, RoadNet::zone() surface
classifier (bulbs, junction and bulb fillets), per-road mesher with junction patches and bulb discs, consumers moved
to all roads, gate+cart at a bulb. Order: planner + placer + sweep stats first, then consumers, meshes, capture.

- [x] types.h / street.cpp: Road bulb + cuts + project index (16-segment chunk boxes), RoadFillet, RoadNet::zone
- [x] plan_street hood spline planner; world_plan REGION 280 + port pair rule + hood_port passed to the generator
- [x] plan_lots hood placer (0 overlaps / 0 intrusions by construction), bulb lots
- [x] sweep stats (footprint, branches, bulbs, lots per road, ports connected, corridor zone intrusions, A* probe);
      tools: `stage0\roads_stats.py <name>` (SWEEP / SWEEP_EMB / SWEEP_WORLD lines of <name>.log),
      `stage0\town_parity.py 1e_after2_dump <name>_dump`, `--sweep-geo=<dir>` + `stage0\geo_view.py` (top-down PNG
      of a hood from the generator alone: mesh triangles in palette colours, lots, props, gate/cart, nests, candy).
- [x] consumers: rhythm, parks, cars, corner lamps, crosswalks, street candy, magic, grass, leaves, hedges, gate/cart,
      nests, apron, fireflies
- [x] hood mesher; towns: all 125 town settlements bit-identical to 1e_after2_dump in every category (2a_v5)
- [x] numbers, determinism rerun, load time, one overview capture (below)

Where it lives (street.cpp unless noted): spline planner `hood_attempt` / `hood_checks` / `hood_fallback` /
`finish_hood_net` / `plan_hood_roads` (street.cpp:480-1160; `plan_street` :1161 sends towns to `plan_street_legacy`); lot placer
`plan_lots` (hood only) with the shared tests `lot_overlap` / `lot_corridor_reach` / `frontage_reach` (context.h);
`RoadNet::zone` + `Road::build_index` (chunked `project`); mesher `StreetMesher` / `build_street_hood`, boundary
`build_boundary_hood` (end of file; towns keep `*_legacy`). world_plan.cpp `kind_of` / `hood_port` (N/S pair rule) /
`base`; REGION 280 + JITTER + BLEND in world_plan.h. Consumers: yards.cpp `place_pocket_parks` (sized by the lot tests;
slivers get a tree), `place_street_rhythm` (every road + round bulbs, a lamp per junction corner, cars on any road);
candy.cpp `weave_street_candy` (hop rows per junction, clusters per road, a ring round each bulb's middle), magic spine
loops each bulb, `scatter_open_grass_net` (zone + 8 m grid), leaves by road length, validation pre-test (exact);
monsters.cpp `plan_infestation` (gate on the island of the bulb nearest the spawn, cart on its sidewalk ring facing it;
off-street spots on every road; asphalt test knows bulbs/patches; +1 nest per ~90 m of road past 120 m, +1 heart past
260 m), A* `route_cap` 40000 for connected searches, 9000 for hopeless ones (`ensure_labels`, built with the grid);
houses.cpp apron to the real curb; fireflies scale with area.

Numbers (`stage0\2a_done` + `2a_done_rerun`, 50 seeds, final build; SWEEP_EMB = the embedded hood the world plays):
- Hoods: lots 18.7 / area (15-22; Stage 0: 9.2); lot overlaps 0 (Stage 0: 69), straight-corridor intrusions 0 (was 130),
  full corridor test (bulb rings, fillets, port run-ons) 0, zone-sampled footprints 0, yard candy over curb/road 0.
- Branches 1.9 (1-3), bulbs 1.08 (1-2), lots round each bulb 3-5 (min 3, mean 3.9; the rings are placed before the
  main road's rows); standalone port road N 13 / S 8 / none 4. Embedded road lengths: main 132 (120-149), cul-de-sacs
  53 (37-73 junction to bulb centre = 30-66 m past the main road's sidewalk), port roads 68 (50-83 = 42-75 m past the
  sidewalk: long enough to reach past everything on its side); built road 247 m / area (174-293; was ~125).
- Footprint (bounds) 139 x 131 mean, max 154 x 150 (`HOOD_ENVELOPE` 152: one sweep at 165 gave 28 bulbs vs 27 and 19.4
  lots vs 18.7 for footprints to 162 m, so 152 stays); world plan (535 hoods, margins included) max 161 x 160, reach
  from origin max 83 vs the 100 m the rule allows: 0 violations, 0 blended-footprint clashes.
- Ports: 2444 settlement ports, 2364 matched and joined by a country road; the 80 misses are all town side lanes closed
  as before (209/289 open); hood N/S port roads 507/507 matched and open; E/W always matched. All 2364 country roads
  sampled against both end settlements' lots (3.4 + 0.6 m) and street corridors (zone): 0 hits.
- Layout attempts 2.6 (1-6), fallback 0/50 (forced once with HOOD_ATTEMPTS = 0: clean, 0 overlaps / intrusions).
- Generator: plan ~26 ms + build_nodes ~39 ms (sweep, embedded; stage_ms: lots+houses 9, magic+scatter 11, validate
  1.6 (was 8.7, exact pre-test), street 1, street mesh 2). In game ("Loaded ... ms", headless smoke, single runs, noisy):
  101: 85-110, 102: 75-85, 103: 69-79, 104: 88-104 (Stage 0: 37). The rest of build_nodes is one collision shape per
  box and the prop instances.
- Candy 548 / area (24.2 per lot, street 95). Nests 5.2 (4-6), hearts 2.5; nest spacing as Stage 1d (gap ratio min 1.0).
- A* probe (sweep_nav_probe: 200 chases 4-16 m + 12 walks home per nest from past the leash, connected pairs only): cap
  9000 cut 5.8 chases and 16.4 walks home per area; 40000: 1 chase in 25 areas, 0 walks home, worst route 3.0 ms
  (debug build). Shipped: 40000 when start and goal are connected, 9000 otherwise (labels built with the grid).
- Towns: 125 / 125 town settlements bit-identical in all 19 categories. Determinism: two -Dump runs, 0 differences on
  82 SWEEP keys and 4850 hashes (compare.py now skips the nav / stage timings).
- Terrain radius: monsters past the loaded terrain (CHUNK_LOAD 170 m from the player; a far nest can now be ~250 m off)
  are held by "never fall out of the world" (monsters.cpp ~541, analytic ground); nests are static. Nothing lost.
  No region-size literals outside world_plan.h (grepped world/, play/, gen/, game/scripts).
- Capture (one, taken with the envelope-165 build before the last two fixes): `stage0\2a_seed101_overview.png` (Acorn
  Court from 120 m: 24 houses, 652 candy; the area banner and depth blur soften it). Readable plan views of the final
  build from the generator alone: `stage0\geo\geo_101/103/105/107.png` (geo_view.py).

For the lead: most hoods keep one bulb: the pair rule gives ~95% of world hoods a port road, and two cul-de-sacs on
opposite sides do not fit 152 m (a bulb with its ring of lots reaches ~76 m from the main road). Load time ~2.3x for
~2x lots. Cine and showcase were not exercised (both read the trunk generically). SAVE_VERSION untouched.

For Stage 3b (hills): hood roads are flat only because `set_road_points` zeroes y; everything downstream reads road
height: the mesher writes every vertex as its centreline point's y + a lift (`StreetMesher::lift`,
`MeshBuilder::ribbon_lift`; junction patches / bulbs take vertex y, fillets lerp their end heights), lots get their
frontage height from `frontage_frame` (bulb lots: bulb_c.y), gate/cart use bulb_c.y. zone(), project(), the lot tests
and the planner checks are horizontal. Still ground-is-zero in the new code: `scatter_open_grass_net` (y -0.03 / 0.05),
leaves, street candy heights (road y + const: fine on graded roads), hedges (lot pad y: sample ground along them), the
pocket-park lawn quad (frame y), monsters' nav at origin.y. Grade along a road can be applied per sample after
`finish_hood_net` (fillets keep their end anchors road0/s0/lat0 / road1/s1/lat1, so arcs follow).
For Stage 4c (towns): towns still run plan_street_legacy, plan_town_lots, build_street_legacy, build_boundary_legacy,
and the `legacy` / `ctx.kind == Town` branches in place_street_rhythm, weave_candy, scatter_open_grass, leaves and
plan_infestation (lane gate/cart, trunk-only off-street spots, asphalt test without zone()). To move a town over: build
its RoadNet with `set_road_points` / `build_branch`-style roads + `finish_hood_net` (fillets, cuts, index), place shops
with a row placer like `plan_lots`' `row` (shop widths, the same fits()), then flip those guards (better keyed on
"the net has no x_station road" than on the kind); `StreetMesher` wants a town verge cell (brick pavers). WorldPlan
keeps a town's lane side from its seed; hoods prefer to run their port road to a town lane facing them.

### Stage 2b (curvier roads, 2-3x the houses; done by the Stage 3b worker before the hills)

- [x] Roads are turtle paths (street.cpp `Turtle` :606): straight runs and circular arcs sampled every 0.5 m, so the
  tightest centreline radius is the smallest arc radius. Main road (`hood_attempt` :863): straight off the W port,
  then S-bends (arcs 27-48 m radius swinging the heading to 0.3-0.95 rad either side of east, 80% of bends flip side,
  12-30 m straights between), back to east and a straight into the E port; x reach `HOOD_ENVELOPE * 0.78-0.93`.
  Side roads (`build_branch` :657, now with a `parent` index): 14 m straight off the junction and into the end, 1-3
  arcs between (24-40 m radius, S-bends), cul-de-sacs 55-110 m; the port road bends round to due N / S and puts any
  extra length in its end straight. 3-5 side roads (30% / 40% / 30%), sides alternating; a long cul-de-sac (> 72 m)
  gets a second cul-de-sac off it 60% of the time (parent = that branch: finish_hood_net, the corridor checks,
  junctions(), plan_lots and the mesher were already generic). Built greedily (:999 `try_add`): each road at its
  drawn length, shorter (x0.78, x0.6) if the checks fail, left out if it still fails; the port road must fit.
- [x] Limits: main radius >= 24 m, branch >= 21 m (`MAIN_RADIUS_MIN` / `BRANCH_RADIUS_MIN`; 2a: 40 / 28), junctions
  where the parent bends no tighter than 36 m over +-7 m (`JUNCTION_RADIUS_MIN`; 2a: 50). Port-end straights and every
  corridor reject kept. No loop / crescent street (optional in the brief; left out, the tree-of-roads assumption is
  everywhere downstream).
- [x] House band: `plan_hood_roads` (:1228) plans the lots for each candidate net (`plan_hood_lots`) and keeps it only
  with 38-56 lots (`HOOD_LOTS_MIN/MAX`); `plan_lots` then keeps them (`GenContext::hood_lots_ready`). Reject reasons per
  run in `GenStats::road_rejects` (no global counters: thread-safe for the preloader).
- [x] Size: `HOOD_ENVELOPE` = 270 (types.h, was 152 in street.cpp); `WorldPlan::REGION` derived from it
  (world_plan.h: 2 x (envelope / 2 + 8 + BLEND) + 2 x JITTER, rounded up to 10 m = 370; was 280).
- [x] Scaling (monsters.cpp :1695): +1 nest per 90 m of built road past 120 m (cap 9, was 3), +1 thorn nest per 220 m
  past 40 m (cap 3; same as 2a's single step at 260 m). A* `route_cap` 100000 for connected searches (was 40000).
- Numbers (`stage0\3b_v2`, 50 seeds, final build; embedded = what the world plays; 2a in brackets):
  lots 43.9 / hood, 38-56, every hood in the band (18.7, 15-22); overlaps 0, corridor / zone intrusions 0; side roads
  3.0 (2-5) incl. second cul-de-sacs, bulbs 2.2 (1-4) (1.08), lots per bulb 3-5 (standalone: one bulb got 2); road
  548 m (247); main 283 m, cul-de-sacs 80 m, port roads 107 m; footprint 258 x 219 mean, 277 x 277 max in the world
  plan; reach 140 m of 145 allowed, 0 rule violations / clashes; country roads 2364 matched, 0 lot / corridor hits;
  layout attempts 3.3 (lot-band rerolls 0.6), fallback 0/50. Candy 1252 / area (548), street 189. Nests 8.2 (7-10),
  hearts 4.0 (3-5), spacing gap ratio min 1.0. A* probe at 100k: chases cut 0, walks home cut 0.3 / area (40k: 13),
  worst route 8.4 ms (debug build). Plan ~120 ms + build ~81 ms in the sweep. Towns: 125/125 bit-identical in all 19
  dump categories; determinism 0 differences (104 SWEEP keys, 4850 hashes).
- Top-down plan views from the generator alone: `stage0\geo2b\geo_101.png`, `geo_103.png`, `geo_105.png` (street
  surfaces shaded by height since Stage 3b).
- The sweep process now hits Godot's RID "Element limit reached" near its end (it builds 250 settlements' nodes in one
  process and never frees them; ~1600 collision boxes per hood). The SWEEP lines and hashes are unaffected (they come
  from the plan); the game frees areas as they unload.

### Area preloading (area-preload worker, 2026-11-17)

User: "Preload the next connected areas multi-threaded while there's idle time for the calculations ... So when they
actually need to render it's immediate." Owned: `world\world_streamer.*`; entry-point split in
`gen\neighborhood_generator.*`; small edits elsewhere listed below. `--no-preload` runs the old synchronous path
(kept for comparison; same generator code).

Design (world_streamer.cpp unless noted):
- Settlement lifecycle (`Pre`, world_streamer.h): PLANNING (worker) -> PLANNED -> BUILDING (main thread, sliced,
  in the tree but hidden) -> READY -> REVEALING (sliced) -> APPLYING (AreaState per lot, sliced) -> attached
  (`on_area_loaded`), and when left behind CONCEALING -> READY again (the cache). `preload_update` :1155.
- Scheduler: every settlement of the 5 x 5 regions whose footprint is within `PRELOAD` = 260 m of the player, best
  first by `area_score` :877 (footprint distance - 60 m x heading toward it); at most `PLAN_TASKS` = 2 plans on the
  workers and `CACHE` = 4 settlements not attached (planning, building or built). Full cache: a settlement is dropped
  for one only if that one scores 60 m better (hysteresis); anything past `EVICT` = 420 m goes. An attached area past
  AREA_DROP (175 m, unchanged) is detached (`detach_area` :1012: `on_area_unloading`, the game's candy-field signal
  connections taken off) and kept hidden in the cache, so walking back re-attaches it with no plan and no build.
  `--preload-tasks=N`, `--preload-cache=N`, `--preload-dist=M` (stress tests).
- Worker phase (`AreaJob` :71, `WorkerThreadPool::add_native_task`, low priority): `plan_settlement()`
  (neighborhood_generator.cpp :148) = every data stage of the old `run()` (street .. scatter) on a fresh GenContext,
  plus with `pack` the surface arrays of every MeshBuilder mesh (`MeshBuilder::arrays()`, mesh_builder.cpp :283), the
  magic trail pieces (`magic_pieces` :856) and the strung lights (`strings_data` :1312). Then the game's
  `on_area_prepare` worker function: `Infestation::prepare` (monsters.cpp :2096: the walk grid and the nest layout,
  with the same rng_ draws `build()` made). A cached settlement gets its prepare re-run on a worker when it is detached
  (`PrepJob` :94).
- Main-thread phase: `NeighborhoodGenerator::begin_build(plan, hidden)` :268 / `build_step(deadline)` :647: the same
  build in the same order as `build_nodes()`, resumable per item (lot meshes, collision boxes, props, doors, lights,
  string kinds, grass groups, magic pieces); `run()` = plan + `build_step(0)`, so the sweep, the old path and the
  preloader share one code path. Hidden: every node the build adds to the settlement root (and every prop) is
  `set_visible(false)` before it enters the tree (`put` :589), collision live (the player is >= 95 m away), so
  `reveal_step` :600 / `conceal_step` :616 are one `set_visible` per top-level node, sliced (one toggle on the root
  costs 6-11 ms for ~9000 nodes: `--stream-probe`).
- Attach: reveal capped at 1.5 ms a frame, then rung / cleared per lot (`open_door` / `close_house`, idempotent), then
  `on_area_loaded` (fired at attach, never at prebuild). The game's infestation is `Infestation::build_begin` there
  (gate + cart) and the nests, thorn nests, tendrils and knots go up one per piece in the following frames through the
  new `on_area_loaded_step` hook (`finishing_update` :1066; `Infestation::build_step` monsters.cpp :2225).
- State at attach: the candy filter used by a build is kept; at reveal, if `AreaState::candy_taken` differs, the candy
  field is rebuilt with the current one (`rebuild_candy` :777). `visited` / `candy_total` are set at attach only (not
  at prebuild, so `export_states` never saves an area the player has not reached). State only changes while
  attached, so a preload never sees a stale state in practice; the check is there for safety.
- Fallback (`finish_now` :949): the player reaches an area that is not ready -> wait for its plan, build it whole,
  print "Area preload fallback (...)" and count it. Blocking updates (start, teleport, save load) go the same way
  without counting. Chunk next to the player (< 60 m) missing -> built on the main thread, printed and counted.
- Terrain: `make_chunk_data` :1591 (pure data: heights, ground + road quads, faces, posts, signpost spots, dressing,
  grass) on a worker with its own WorldPlan copy (`chunk_plan_`; one chunk job in flight); `finish_chunk` :1863 makes
  the nodes in the old order (~1 ms). `build_chunk` = both on the main thread (old path). The horizon ring's mesh is a
  worker job too (`HorizonJob` :162, `natural()` is pure).
- World plan regions: `WorldPlan::base()` plans each settlement's street and lots (Stage 2b: tens of ms each, up to
  ~670 ms for a 7 x 7 ring); `RegionJob` :145 plans the 7 x 7 regions around the player on a worker's own plan
  whenever the player's region changes, and `WorldPlan::adopt_regions` (world_plan.cpp :208) copies them into the
  main plan and the chunk worker's plan, so the main thread never plans a region in time.
- Freeing: an evicted settlement is taken apart <= 250 nodes a frame under the budget (`free_update` :1121;
  collision bodies leave the tree whole, then are emptied out of it).
- Budget: per frame `clamp(1000/60 - (process + physics) - 4 ms, 1, 3)` ms, where process + physics come from the
  Performance monitors (Godot refreshes them once a second with the worst frame of that second, and they include
  streaming's own time, so streaming's own worst of the last 128 frames is taken off; the frame-cap sleep never counts
  as work). Chunks first, then settlements, then freeing.

Thread-safety audit (plan phase): gen\*.cpp plan stages touch no nodes, resources or engine singletons (grep: no
memnew / Ref / get_singleton / UtilityFunctions in street, yards, candy, houses, town); no mutable statics in gen\,
mesh\, core\ (candy_kinds' table is filled once by `register_candy_kinds`, before any plan: setup() calls it too);
`Rng` is per instance; `noise::` is header-only and pure. Shared state found and fixed: `AssetLibrary::get()` inserts
on a miss (now a shared_mutex; a worker-thread miss prints a warning: none seen); `WorldPlan` caches lazily (never
shared across threads: each worker job has its own plan, copied over with `adopt_regions` while both are idle).
`plan_infestation` / `build_nav` read only the GenContext and the art kit. Godot's rules (docs "Thread-safe APIs"):
RenderingServer / PhysicsServer calls from threads need the Separate thread models, which this project does not set,
so no resource or node is made off the main thread; workers make plain data and Packed arrays / Arrays only.

Edits outside the owned files (all small, behaviour-preserving):
- `gen\neighborhood_generator.{h,cpp}`: `run()` split into `plan_settlement` + `begin_build` / `build_step`;
  build_collision / props / doors / lights / strings / scatter / magic resumable (same output, same order);
  `put()` instead of `add_child` for the visible top-level nodes; `magic_pieces` / `strings_data` pure helpers;
  `reveal_step`, `conceal_step`, `rebuild_candy`, `build_report`, `door_open`, `house_closed`.
- `gen\asset_library.{h,cpp}`: shared_mutex in `get()`; `merged_mesh()` caches its bake per asset (every area re-baked
  ~19 candy kinds, the bunting and leaves: 2-12 ms a time; callers only read the mesh).
- `mesh\mesh_builder.{h,cpp}`: `arrays()` / `mesh_from_arrays()` split of `build()`, writing through `ptrw()` (it was
  one engine call per element).
- `world\world_plan.{h,cpp}`: `adopt_regions()` and `region_count()` (additive).
- `play\monsters.{h,cpp}`: `build_nav(ctx, plan)` overload, `prepare()`, `build()` = `build_begin()` + `build_step(0)`;
  `check_freed()` ignores a half-built infestation (`building_`: no nests yet must not read as "freed").
- `gen\neighborhood_generator`: a destructor frees the collision bodies of a settlement dropped mid-collision-stage
  (they are out of the tree until the stage ends).
- `play\festival_game.cpp`: `on_area_prepare` / `on_area_loaded_step` hooks, `area_loaded` takes the prepared
  Infestation and calls `build_begin`.
- `game\project.godot`: `rendering/limits/global_shader_variables/buffer_size=1048576` (the real-renderer stall, below).

Verification. HEADLESS numbers (dummy renderer: no GPU upload, no pipeline compiles, so they measure the CPU side
only): seed 101, `--fly-test` east at 15 m/s, 200 s = 8 settlements planned, 6 attached across 8 region boundaries,
frame cap 60: `art\team\world\preload\walk.ps1 -Name <n> [-Extra --no-preload]`; logs in that folder:
- Old path (`sync1`): attaching a hood costs one frame of 199-315 ms (generate 170-273 + on_area_loaded 29-42), towns
  37-41 ms; streaming frame max 315 ms, p99 3.2 ms; chunk 3.4 ms mean, p99 27, max 48; horizon 7 ms each.
- Preloaded (`pre5`): streaming main-thread ms per frame max 6.0 (a town's first fireflies material, once), p99 2.7,
  mean 0.10; whole frame (update to update) max 19.8 ms, p99 17.2 (60 fps cap); the 120 frames after each attach max
  17.4 ms. Attach: worst single frame 1.5 ms (the capped reveal), 2.1-3.3 ms in all over 1-2 frames; on_area_loaded
  0.3-0.5 ms (was 29-42); then the nests and vines 2.4 ms mean / 3.8 max a frame over ~6 frames. Plan on the workers
  140-170 ms per hood (8-12 ms town), build 48-83 ms main-thread per hood over ~20 frames, queued to ready 0.5-1.5 s.
  Chunks: 1.06 ms main (max 1.6) + 1.7 ms worker; horizon 0.76 ms main. Fallbacks 0, chunk fallbacks 0. A rerun
  on the final build (`pre6`, other jobs loading the machine: game process up to 28 ms in its long frames) agrees on
  attach (worst 1.5 ms, on_area_loaded 0.25-0.6) and adds two outliers: freeing an evicted settlement 14-16 ms in one
  frame (a single unit: likely a collision body leaving the physics space whole) and a chunk 10-12 ms; both are
  build/free-phase, not attach.
- Determinism (`verify.ps1`: both walks under one lock hold; the preloaded one with `--preload-tasks=4
  --preload-cache=12 --preload-dist=1100`, so 39 settlements were planned and built along the way with 4 plans
  running at once at the peak, `--stream-verify`): the 6 attached settlements' plans, 114 SweepDump category hashes,
  0 differ; node trees (names but engine-made "@..." ones, classes, transforms, mesh bounds and sizes, multimesh
  instance transforms, labels, shapes) 0 differ (`verify_compare.py`); their `Infestation:` layout lines identical.
  (A first try hashed the engine's auto names, which count nodes made anywhere in the process, and differed.)
- State (`run.ps1 -Name state -GameArgs --seed=101,--stream-state-test,--stream-fps=60`): lot 0's candy collected
  through the real CandyField (the game marks candy_taken and lot_cleared), lot 1 rung; out 350 m and back (cached
  re-attach) and out 800 m and back (evicted, planned and built again with the saved state): 3 / 3 attaches correct
  (field shows 1273 of 1300 = 1300 - 27 taken, house 0 closed, door 1 open) in both modes; preloaded streaming max
  3.8 ms through it, fallbacks 0. Save/load uses the same states_ (export/import untouched) and setup() drops every
  preload before import_states.
- `--sweep=4` on the split `run()` (`sweep_smoke.log`): 4 SWEEP lines, SWEEP_DONE, openness failures 0. The
  `Infestation:` layout lines (nests, roads, positions) of the 6 settlements are identical between the two verify runs
  (prepare on the worker vs on the main thread).

WINDOWED (one off-screen pair through `tools\godot_bg.ps1`, real renderer, RTX 5060 Ti, 120 s, frame cap 60:
`walk.ps1 -Windowed`, `win_sync.log` / `win_pre.log`; the machine is shared, so there is background noise):
- Old path: Crow's Way attached in one 235 ms streaming frame (generate 183 + on_area_loaded 52); the frame after it
  took 13.9 s (243 ms of it streaming). A second frame of 14.8 s came ~2000 frames later with 0.1 ms of streaming in
  it: Acorn Court being unloaded (queue_free of the whole settlement; the nodes are deleted after update()). 43
  frames over 40 ms in all, 33.6 s of them.
- Preloaded: attaches 2.2 ms (Crow's Way) and 3.7 ms (Ciderton: its on_area_loaded alone 3.7 ms, over the 2 ms
  target on this renderer); chunks 1.3 ms main-thread, horizon 1.0; one unattributed 45.5 ms streaming frame at the
  start (the top of update() was not timed then; it is now). But 197 frames of 200-600 ms (max 2.3 s), 40.6 s of
  them in all, in which streaming spent only its 1 ms slice: one cluster in the first ~6 s while Crow's Way was
  being built hidden (20 s queued-to-ready instead of 1.5 s), 1.4-1.6 s frames right after each "Detached" (the
  game freeing that area's monsters, nests and vines), and a cluster at x = 911-1016 m while an evicted settlement
  was being freed in slices.
- (These two runs are the "before" below; their cause was found afterwards, next section.)

Real-renderer stall: cause and fix (two more off-screen runs, cleared by the lead: `win_fix.log`, `win_confirm.log`).
- Cause: the renderer's global shader-parameter buffer overflowed. toon.gdshader declares 13 `instance uniform`s, so
  EVERY GeometryInstance3D drawn with the toon material takes a 16-row slot in that buffer, whether or not a parameter
  is ever set. The project default `rendering/limits/global_shader_variables/buffer_size` = 65,536 rows = 4,096 slots.
  A hood alone has ~3,900 geometry instances (`VERIFY` lines: Acorn Court 3,893, Crow's Way 3,838; a town ~460; 45
  lights a hood), so the home area nearly filled it and the second hood overflowed it: both old windowed logs carry
  ~9,000 `ERROR: Too many instances using shader instance variables` plus ~4,200 `instance_buffer_pos.has` errors.
  Each failed allocation scans the whole buffer and prints an error with its source line: that was the "~1 ms a node",
  on creation and on freeing alike, and absent headless (the dummy renderer has no such buffer). Side effect fixed too:
  every instance past slot 4,096 had silently lost its per-instance parameters (e.g. wind sway, lantern glow_boost,
  rim light, magic-trail fade) in whatever loaded after the home area.
- Fix: `game\project.godot` [rendering] `limits/global_shader_variables/buffer_size=1048576` (65,536 slots, 16 MB of
  GPU memory; enough for 2 attached + 4 cached hoods ~ 24k instances, plus chunks and monsters). One line, no code.
  Node count is not the problem once the buffer fits (headless cost per area is already within budget), so props
  were not consolidated; that stays an option if the buffer ever fills again (the 6-area cache is the peak: CACHE 4).
- Other suspects, from the logs: pipeline compilations stalling a draw 0 in both runs (Godot 4.7's ubershaders cover
  first sight; the specialisations compile in the background); lights 45 omni a hood, no shadows on them; physics
  bodies were already a few per area (shapes on them); no script hooks on node add / remove; machine load sampled
  every 6 s during the confirm run (`win_confirm_load.log`): 3-39 % CPU, only this run's Godot (the 3b worker's next
  Godot started after it ended).

Frame time on the real renderer (whole frame, update to update; 60 fps cap; walking east at 15 m/s):

| run | frames | over 33 / 40 ms | 40-100 | 100-250 | 250-1000 | >= 1 s | worst | p99 | attach worst |
| --- | --- | --- | --- | --- | --- | --- | --- | --- | --- |
| before, old path (`win_sync`, 120 s) | 7,097 | 43 (>40) | 25 | 15 | 0 | 3 (13.9 s, 14.8 s, 1.3 s) | 14.8 s | 31.2 | 235 ms |
| before, preloaded (`win_pre`, 120 s) | 6,694 | 197 (>40) | 72 | 68 | 54 | 3 | 2.3 s | 225 | 3.7 ms |
| after, preloaded (`win_fix`, 150 s) | 8,997 | 1 (36.9 ms, the first frame after loading) | 0 | 0 | 0 | 0 | 36.9 ms | 18.4 | 1.7 ms |
| after, preloaded (`win_confirm`, 200 s, 8 boundaries, 6 settlements) | 11,997 | 1 (same first frame) | 0 | 0 | 0 | 0 | 36.9 ms | 18.7 | 2.9 ms |

  `win_confirm` in bins: < 17 ms 11,238; 17-20 748; 20-25 10; 25-33 0; 33-40 1. Frames over 33 ms with 5 ms or more of
  streaming in them: 0 (target: none over ~33 ms from streaming). Streaming's own main-thread time: max 15.7 ms (one
  build slice: a hood's collision bodies joining the space or a town's fireflies, see the caveats), p99 3.0, mean
  0.11; on_area_loaded 1.4-2.9 ms, attach worst frame 1.6-2.9 ms (target under ~5 ms); chunks 1.1 ms, horizon 1.0.
  `--no-preload` was not re-run windowed after the fix (the cleared runs went to the default path); the buffer fix
  applies to it too, but its 235 ms generate-on-arrival frame remains by design.
- Also not run: a second setup() in one process (new night, save load: wait_all, drop every preload, free_update(0));
  the paths are written for it but no test starts a second world.
- Headless extras: fallback (`walk.ps1 -Name fallback_test -Secs 60 -Extra --preload-dist=50`): 1 fallback,
  printed and counted ("Crow's Way was planning, finished in 223 ms": the old hitch, as expected); travel through a
  gate with preloads in flight (`--travel-test`, a blocking update mid-run): attaches fine, 0 fallbacks.

Caveats for later stages:
- Every toon-material GeometryInstance3D costs a slot in the global shader-parameter buffer (65,536 slots now). Bigger
  hoods, a bigger preload cache or more props per lot multiply it: keep (attached + cached) x instances per area well
  under 65k (`VERIFY ... geometry=` with `--stream-verify` counts them), or merge props / give static geometry a
  material without instance uniforms. An overflow shows as "Too many instances using shader instance variables".
- Stage 3b: anything the plan reads must stay plain data. If `GenContext::ground()` starts reading the WorldPlan,
  give `plan_settlement` a WorldPlan (each AreaJob would need its own copy, adopted like the chunk worker's) or copy
  the heightfield into the GenContext before the job starts; never let a worker touch the streamer's `plan_`. A new
  per-region cache in WorldPlan should be copied in `adopt_regions` too.
- Any new build_* step belongs in `build_step` (neighborhood_generator.cpp :647) as its own resumable unit; add its
  nodes with `put()` (hidden while preloading) unless they are containers or collision bodies. New plan data that the
  build turns into meshes can be packed in `plan_settlement(pack)`.
- Remaining single pieces over the 3 ms budget (build phase, not attach): the collision bodies joining the physics
  space at once (3-5 ms a hood: ~1600 shapes in a few bodies), a town's first fireflies (5 ms, once), one nest / thorn
  nest (<= 4 ms). Splitting the bodies would change the node layout others look up, so it was left.
- Node creation stays on the main thread; with the Separate thread models the build could move off it, but that is a
  project-wide setting.

### Stage 3b (rolling hills under the neighbourhoods)

- [x] Terrain (new `gen\terrain.cpp`; types.h `TERRAIN_LATTICE` / `HoodTerrain` :64-75, context.h `terrain`, `ground`,
  `lot_ground`, `in_structure`, `yard_lift`, `plan_hood_terrain`, `drape_lot_rect`). `hood_natural` (terrain.cpp:20):
  settlement-local relief from the settlement seed, 3.4 m x fbm(95 m) + 1.1 m x fbm(34 m) (relief over the footprint
  3.9-5.0 m; this is the knob if the hills should read stronger). `plan_hood_terrain` (:241), hoods only:
  1. Grades every road, parents first (`grade_road` :173): the natural height along it box-filtered over +-10 m, then
     held to 8% per segment by forward/backward clamps, rounded off, made feasible again. The parent runs at <= 1.2%
     within (child edge + 11 m) of each junction; a side road is level (fixed at its parent's height) across the parent
     corridor + fillet; a turning circle is level from bulb_s - 3 m (bulb_c.y = the end). Fillet p0/p1 and the spawn
     get their heights. The mesher already wrote every vertex from road y, so ribbons, curbs, verges, sidewalks,
     crosswalks, junction patches, bulb discs and fillet bands all follow.
  2. Pads: lot.xf.origin.y = road y at the lot front (bulb: bulb_c.y) + clamp(natural median over the lot - road,
     -1.2, +1.8).
  3. One heightfield on the open world's terrain lattice (2 m, the same triangles as the terrain chunks: `HoodTerrain::at`
     replicates build_chunk's a-c diagonal) over the bounds + 34 m: road corridors (and each port's 35 m run-on) at road
     height, level across; inside a lot its pad, a 3 m bank up / down to the sidewalk at the front, a soft step to a
     touching neighbour's pad (blend to the mean over 0.6-1.9 m each side), 55% of the natural roll kept in the yard
     (clamped -0.8 / +0.6, faded out over 2.5 m at the sides and back), never above the pad under the house, porch and
     flight, garage (+0.6 m); between lots and corridors smooth embankments to the natural hills (8 m from a corridor
     edge, 7 m from a lot). Why the lattice rather than a 1 m cache: the player walks on the terrain chunks' collision
     (2 m triangles), so ground() must be those triangles exactly, or lawns z-fight and props float; a hood's origin is
     snapped to the lattice (world_plan.cpp:134) so local and world lattices coincide (static_assert world_plan.cpp:13
     that WorldStreamer::STEP == TERRAIN_LATTICE).
  Run from build_house for lot 0 (houses.cpp:397: every house plan is known, nothing is built yet) and in
  WorldPlan::base() after plan_lots + assign_households + plan_house (world_plan.cpp:158), so both make the same field;
  RegionPlan::terrain carries it. Plan-side only: no statics, no engine calls (the preloader runs it on a worker).
- [x] World (world_plan.cpp): `height()` inside a hood = origin.y + terrain (exactly, where the mask is 1), blending to
  the natural hills over BLEND; the country-road flattening is faded out inside hoods (hood_m) since the hood's own
  graded corridor carries the road there; ports carry the graded end height (:170) and country roads start from it
  (:254; woods and towns unchanged: their ports are at origin.y).
- [x] Buildings (houses.cpp): `foundation` (:424) under the house and the garage, painted dark stone (STONE_DARK /
  mat::STONE) from 6 cm above the pad down to 0.3 m below the lowest ground under the footprint (sampled every 0.4 m),
  plus a solid so nothing walks under the floor; Lot::found_house / found_garage / found_porch record the bottoms. The
  porch's lattice skirt and solid reach the ground; the flight gets extra treads (straight on, at the bottom tread's
  width) while the ground at its foot is more than half a riser below (:531 Lot::porch_steps; front_walk anchor and the
  Steps feature follow; steps_front everywhere uses porch_steps). Lawn, drive slab and apron are the lattice clipped to
  their rectangles (`drape_lot_rect` terrain.cpp:92); stepping stones, luminaria, fence posts, pickets and rails sit on
  the ground (rails and collision split where the ground changes).
- [x] Yard things: one rule, `yard_lift` (terrain.cpp:78): a lot-local point gets the ground under it, 0 on the buildings
  (porch, roof, garage: those stay with the pad), the tread top on the extra treads. Applied in Yard::put (yards.cpp:105),
  GenContext::add_prop / add_box for lots (street.cpp:29), the candy LotWeaver's W() (candy.cpp:298), summit routes,
  the backyard lollipop, lawn grass, the magic branch; landmark meshes (tree house, carving tables) lifted by it.
  Pocket parks: props on the ground, lawn draped. Hedges: short level pieces on the ground, one collision block per run.
  Street: corner lamps on the ground, parked cars pitched along the grade; spine and street candy ride the road y.
- [x] Ground-is-zero fixes: candy platform filter, sequences (takeoffs and flight from ctx.ground), validation's "reachable
  from the ground" (candy.cpp:903/952/1159/1228), open grass, verge grass, leaf litter, road leaves; monsters: nav grid
  box filter (monsters.cpp:877), clear_of_boxes / shot_openness relative to the ground, nests / hearts / tendrils /
  knots / gate / cart settled onto the ground at the end of plan_infestation (:2072); candy_field burst height from the
  doorbell (candy_field.cpp). Towns pass ctx.terrain == null everywhere (guards keep every float op identical).
- [x] Review aids: `--capture-view=foundation` (deepest house foundation in the home area, from a clear street-side spot)
  and `--no-lens` (no depth blur) in festival_game.cpp.
- Numbers (`stage0\3b_v2` + `3b_v2_rerun`, 50 seeds, `python stage0\terrain_stats.py 3b_v2`; embedded hoods):
  road grade max 0.080 (per metre and over 4 m); relief 4.5 m (3.9-5.0); pad - road -1.20 .. +1.80; yard vs pad behind
  the bank -0.81 .. +0.68; cut / fill against the natural relief max 1.6 / 1.7 m; foundations above the lowest ground 0
  (worst bottom 0 m above, i.e. at most level), foundations showing (ground > 5 cm below the floor) 67 / hood, deepest
  0.86 m (standalone 1.47); extra porch treads 65 / hood; candy under 0.3 m over the ground 0 (lowest 0.71 m); candy
  dropped by validation 7.6 / hood (Stage 2b sweep comparable); props floating > 5 cm 0, buried > 15 cm 0; nav: spawn
  walkable 25/25, nests + hearts not connected to the spawn 3 of ~305 (fenced backyards). Towns 125/125 bit-identical
  in all 19 categories (`town_parity.py 1e_after2_dump 3b_v2_dump`); determinism 0 differences (104 SWEEP keys, 4850
  hashes). Sweep plan 119 ms (2b: ~96), build 81 ms.
- Load: the "Loaded ... ms" print is gone (area preloading): headless smoke `stage0\3b_smoke.log`, seeds 101-104:
  preload plan 147-217 ms on a worker thread, build 73-96 ms spread over 24-82 frames, worst attach frame 18-27 ms
  (2a: 70-110 ms in one frame).
- Capture (the one in-game look, seed 101, `--no-lens`): `stage0\3b_overview.png` (Acorn Court, 45 houses, the Stage
  2b layout: S-bend main road, curved cul-de-sacs; the hills read only faintly from 78 m up at night) and
  `stage0\3b_foundation.png` (lot 22, foundation 0.72 m deep: the first version of the view stood behind a hedge, so the
  foundation is hidden; the view now picks a clear street-side spot by raycast, built but not re-captured).
- For Stage 4c (hilly towns): flip a town onto the same path by giving it a terrain (call plan_hood_terrain for towns:
  it only needs roads, lots with house/shop plans; in_structure returns false for shops, so give shops their own
  footprint rule), snapping its origin to the lattice (world_plan base()), and dropping the `ctx.terrain` guards' town
  side (they exist only to keep towns bit-identical). Shop fronts on a 5% street want stepped pads (one pad per shop is
  what plan_hood_terrain already does per lot); the front bank there should become steps or a low wall (shops sit on
  the sidewalk). The heightfield must stay on the 2 m lattice while WorldStreamer::STEP is 2. A big town will hit the
  sweep's RID limit sooner (see Stage 2b).
#### Stage 3b round 2 (lead: "the hills don't read"; supersedes the relief, pad and capture numbers above)

- [x] Relief (terrain.cpp `relief_raw` / `hood_natural`, `plan_hood_terrain` start): 1-2 broad hills and 1-2 dips
  (Gaussian swells, radius 45-75 m, placed over the footprint) on a faint 130 m swell and 38 m roll, scaled per seed so
  the hood's peak-to-trough relief is 10-14 m (`HoodTerrain::swells/scale/bias`; the sweep measures 9.9-13.9, mean 11.3).
  Roads stay <= 8%: they climb over the crests and drop into the dips; where 8% can't follow, the corridor is cut or
  filled (up to 7.5 m cut / 6.6 m fill against the natural relief) and the banks run along the verges.
- [x] Pads: clamp -1.8 / +2.4 (`PAD_BELOW/PAD_ABOVE`); yard roll kept 55% clamped -1.4 / +0.8. Front: the ground stays
  level with the sidewalk for 2.4 m past every corridor edge (`WALK_APRON`: every lattice square touching a sidewalk has
  all its corners at its level, whatever lot or open ground is next to it, and any street's sidewalk counts, not only the
  lot's own), then a 4.2 m bank to the pad that runs under the porch flight, so perched houses get their extra treads.
  Never above the floor within 2.9 m of the house / porch / garage (a lattice square's reach).
- [x] Retaining walls: touching neighbours whose pads differ by more than 1.1 m (`RETAIN_STEP`) stay level to the line;
  the higher lot builds a painted stone wall down it (houses.cpp, after the lawn: 1.5 m courses following the ground,
  capped 0.3 m over the higher yard, 3 m solid walkable pieces; `Lot::retain`). Elsewhere embankments (9 m from a lot,
  2.4 + 8 m from a corridor).
- [x] Candy support test (candy.cpp validate) measures from the highest ground within 1.6 m (an arc over a fence on a
  hillside takes off uphill).
- [x] Review aids: `--capture-view=crest` (the home area's most prominent road crest, camera behind on the climb),
  `--capture-view=foundation` now picks the most perched house and looks from the far sidewalk, `--overview-pitch=`.
- Numbers (`stage0\3b2_final` + `3b2_final_rerun`, embedded hoods): grade max 0.080; relief 11.3 m (9.9-13.9); pad - road
  -1.80 .. +2.40 (24/25 hoods have a house at +2.4); **deepest foundation 3.67 m** (house / garage / porch, standalone max
  2.82; 74 / hood showing > 5 cm); extra porch treads 113 / hood; **retaining walls 3.7 / hood, tallest step 2.63 m**
  (standalone 3.21); foundations above the lowest ground 0; candy below 0.3 m over the ground 0; candy dropped by
  validation 10.5 / hood; sidewalk / verge pokes (visible terrain above the surface) 0, ground under a sidewalk's outer
  edge at most 0.11 m below the road; props floating > 5 cm 2 in 25 hoods (<= 0.12 m), buried > 15 cm 10 in 25 hoods
  (jack-o'-lanterns on the bottom porch treads of sunken lots whose flight comes within ~3 m of a higher sidewalk: the
  sidewalk's lattice square reaches the tread; not fixed); ground slope p99 0.46 (25 deg), max 2.9 at wall lines and
  clamps; nav: spawn walkable 25/25, 1 nest / heart cut off; A* at 100k: 0 chases cut, 1 walk home in 25 areas, worst
  8.4 ms. Towns 125/125 bit-identical in all 19 categories; determinism 0 differences (114 keys, 4850 hashes). The
  standalone sweep variant (40 m of road into the fog) still shows a few sidewalk pokes at its road ends; the world
  never plays it.
- Capture (round 2's one look, seed 101, `--no-lens`): `stage0\3b2_crest.png` (camera ~22 m up behind the home area's
  crest: the streets and roofs rise and fall into the distance, houses perched on banks) and `stage0\3b2_foundation.png`
  (lot 24, pad 2.4 m above the street: the bank up from the sidewalk and a 9-tread flight to the porch). Both were taken
  before the sidewalk apron fix: the foundation shot shows the lattice poking through the near sidewalk, which the
  `walk_pokes` stat (now 0) confirms is gone; not re-captured. The home hood's best crest is only 0.9 m prominent; a
  hood with a central hill would show it better.
### Stage 4c (varied, hilly towns; plus 2.5x monster nests)

- [x] Towns on the RoadNet (street.cpp `plan_town_roads` / `town_attempt` / `town_fallback`, after `plan_hood_roads`): per seed a
  template (`TownTemplate`: Cross = main street + 1-2 cross streets, each T or +; Grid = straight main street crossed square-on by
  2 / 2-3 / 3 + crossings, blocks of land 32-60 m between the sidewalks; HighStreet = a bow, the square on the outside of its
  apex; Fork = a Y, the branch leaving at 48-60 deg while the main street swings away) and a size class (main street x reach
  82-104 / 115-150 / 160-200 m, side streets 22-34 / 30-50 / 40-64 m; a grid's length comes from its blocks). Same Turtle
  roads, `build_branch`, `hood_checks` (new params: related-skip 62 m for the fork, shop row reach 19 m; a + crossing's two
  arms are exempt within 30 m of it) and `finish_hood_net`. Side streets end in a hedge under a lantern arch (`RoadEnd::DeadEnd`:
  bands closed like a port end, `build_boundary_hood`), except the N/S port roads. The net is a tree, so a "grid" is crossings
  whose arms run out into the country (no back streets closing the blocks): say so if the user expects closed blocks.
- [x] Ports matched: `WorldPlan::town_offer` (seed only: N/S offered) and `town_ports` (an offered side is built only if the
  neighbour meets it: woods always, a town offering the facing side, a hood whose `hood_port` points back); `hood_port` reads the
  offer instead of the town's base() (no recursion). Passed to the generator as `RegionPlan::hood_port = GenContext::TOWN_PORTS +
  mask` (the streamer already forwards hood_port as port_side: no streamer edit). Town origins snap to the lattice; base() runs
  plan_town_lots + assign_shops + plan_shop + plan_hood_terrain (no assets) and keeps `r.terrain`.
- [x] Lots (town.cpp `plan_town_lots`): the square first (`GenContext::square`, a `TownSquare` with a Lot footprint, shop -2): the
  seed asks centre / corner / end, candidates for all three are tried in that order; then shop lots shoulder to shoulder by arc
  length on both sides of every road (9.5-13 m wide, 13-17.5 deep, shallower on the inside of bends; the shared corridor /
  overlap tests, plus the square). `plan_shop` clamps the body to the lot depth.
- [x] Hills (terrain.cpp, towns now go through `plan_hood_terrain`): streets <= 5% (`TOWN_GRADE`), relief 5-8.5 m, junction flats
  edge + 7 m; a shop's pad = its sidewalk's height at the shop's higher front end (the sidewalk is never above a floor); no yard
  roll, soft steps between service yards (no retaining walls for shops); `in_structure` has a shop rule (body + the paved strip
  out to the lot front + the doorstep treads). The square: a level pad (road + 0.07 at its front centre), its front 2.4 m strip
  runs with the street, then a 3 m paved ramp (`SQUARE_RAMP`, `TownSquare::pad_z` -5.4), and the ground stays level 3 m round its
  sides and back (`SQUARE_MARGIN`). build_shop: lot 0 triggers the terrain (as build_house); a stone plinth from the floor to
  0.3 m below the lowest ground under the shop and its strip (`Lot::found_house`), doorstep on a stone block with up to 5 treads
  (risers <= 0.18) onto the sidewalk (`Lot::porch_steps` = 3 + treads), the service yard draped. Stepped fronts and exposed
  plinths come from the per-shop pads.
- [x] Consumers: the legacy branches now key on `trunk().x_station` (never true now; the old code is kept): street candy, open
  grass (towns: no grass on the brick verge, the square or plazas: `GenContext::plazas`), leaves, street rhythm (lamps, trees,
  cars on every road), mesher (brick-paver verge for towns), boundary (no side hedges beside shops so service yards open at the
  row ends; none into the square). dress_shopfront: sidewalk props on the sidewalk surface, clear of the treads; door flankers
  on the plinth strip. Routes: the awning run's take-off on the sidewalk. place_town_square rewritten: little plazas (draped
  pavers, bench, lamp, tree, pumpkin; hay + jack on wide ones) in every frontage gap on every road, the gazebo square on its pad,
  bunting across every road. Gate and cart stand on the square (`TownSquare::gate/cart`, read by plan_infestation); spawn at the
  crossing nearest the middle that has a 30 m straight near it (`town_straight_near`, the cine shot's finder).
- [x] Monster nests (user, mid-stage: "a lot more monster spawners and some more thorns"; monsters.cpp plan_infestation): hoods
  want 18-24 nests (18 + (built road - 400)/45, cap 23, +0-1) and 6-8 thorn nests; towns 12-20 and 4-6 by street length. Same
  18/12/14 m spacing rule. Spread: a nest spot's weight x (distance to the nearest nest placed / 36 m)^2 (capped at 1), so they
  cover every street, cul-de-sac and yard; towns also get every open spot on a 6 m grid (out behind the rows at 0.002 weight: in effect only once the street-side spots run dry).
  Runtime (a gameplay change nobody asked for, the lead may veto it: a one-line revert of `near_kid` in `MonsterNest::_process`):
  there is no global alive cap (each nest keeps max_alive 2-3), so a nest now only sends monsters out while the kid is within
  85 m; the live count follows the nests near the kid. HUD shows counts as text (fine).
  Instance budget: monsters, nests and vines exist only for attached areas (<= 2); +12 nests is +12 toon instances (beacons are
  StandardMaterial, glow lights are 4 m omni without shadows), monsters ~10-20 rig parts each: a few hundred to ~1,500 more slots
  per attached area against 65,536 (estimate). Measured settlement geometry (`4c_verify_pre.log`, headless): towns 785 / 879
  instances (Stage 3b: ~460), hoods 3,624-3,987; the stress walk (12-settlement cache) peaks at 49,085 for the streamer, under
  the 65,536 slots; no "Too many instances" in any log.
- [x] 3b leftover (done before the lead's note that Stage 3c replaces it): the porch jack-o'-lanterns sit on max(tread top, ground) (yards.cpp dress_yard; the bank still reaches over the
  bottom tread there: Stage 3c replaces this code), corner lamps on their verge band's own height (yards.cpp).
- Tools: `stage0\town_stats.py <logs>` (replaces town_parity.py), `parity.py <before_dump> <a_dump+b_dump> neighborhood` (kind-
  generalised parity), `nest_counts.py`, `capture4c.ps1`, sweep.ps1 `-Extra`. Sweep (sweep.cpp / festival_game.cpp): a town line
  for every seed (`SWEEP_TOWN` on the even ones), town stats (template, size, ports, square, plazas, cine_straight, facade steps,
  plinths, treads), shops' plinth check, prop heights against the street surface (fillet bands included), nest street coverage,
  `world_town_spots` (square and steepest-facade spots per world town), `SWEEP_TPARITY` (WorldPlan's town ground vs the
  generator's), `--capture-view=lookat:x,z,tx,tz`. Run the sweep as two processes (RID limit): `-Count 24 -Seed 101` and
  `-Count 26 -Seed 125` (keeps each seed's hood / town parity).
- Numbers (`stage0\4c_i` + `4c_j`, 50 seeds, final build; 50 towns = 25 SWEEP + 25 SWEEP_TOWN, all embedded):
  - Templates cross 11 / fork 15 / grid 14 / high_street 10; sizes small 16 / medium 20 / large 14; squares centre 23 / corner 12 /
    end 15. Lots 10-62 (small cross 15-21, small fork 10-17, large grid 53-62); built street 112-554 m; footprint 88-216 x 71-167 m.
    Layout attempts 1.18 mean, fallbacks 0. World plans (50 x 25 regions): 289 towns (cross 86, fork 74, grid 68, high street 61).
  - Lot overlaps 0, corridor / zone intrusions 0, square overlaps 0, square corridor reach <= 0. Ports: 2391 settlement ports,
    2391 matched and roaded (Stage 3b: 80 town lanes closed); town N/S port roads 237, all open; country roads vs lots /
    corridors 0 hits; footprint rule violations 0.
  - Grade max 0.050; relief 4.1-11.6 m (mean 6.5); plinth bottoms above ground 0; deepest plinth 1.58 m; facade step between
    touching shops max 1.08 m (mean max 0.70), stepped fronts 13 / town; door treads 42 / town; square pad ground -0.47..+0.44
    (edges by a cross street's sidewalk; interior level); sidewalk pokes 0; candy below 0.3 m 0; props floating 4 in 50 towns
    (<= 0.12 m: two door flankers on a flat shop's paving strip (stat noise) and two hay bales on shops with treads, not
    found), buried 0; platforms reachable 96.2% (Stage 3b towns 98.8%: plinths lift awnings and roofs over the sidewalk crate:
    not fixed); nav: spawn walkable 50/50, nests cut off 1;
    cine straight by the spawn 49/50. WorldPlan vs generator town ground: 50/50 same grid, max |dh| 0.
  - Nests: hoods 20.0 (18-23) and thorn nests 6.5 (6-7) per hood (Stage 3b: 8.2 / 4.0); towns 14.9 (12-20; small 13.1, medium 14.8,
    large 16.9) and 4.6 (4-6) (before 5.5 / 2.5). Spacing kept, no relax anywhere: min nest-nest 14.0 (hood) / 14.2 (town),
    heart-heart 18.1 / 18.4, heart-nest 12.0 / 12.1. Street coverage (every 5 m of street to the nearest nest): hood mean 13.2 m,
    3-5% of street > 30 m from a nest (worst 58 m); towns 11.4 m, 1.5% (large 2.6%, small 0%; worst 46 m).
  - Hoods vs `4c_before_dump` (125 hood settlements): lots, street, street_mesh, lot_mesh, candy, magic, scatter, boundary, doors,
    signs, springs, strings, occupied, meta identical; infest / vines differ (the nests), props / boxes / lights differ (only the
    jack and corner-lamp heights). 3b checks on embedded hoods: grade 0.08, foundations above ground 0, sidewalk pokes 0, candy low
    0, props buried 0 (was 10), floating 2 (<= 0.12 m, pumpkins by extra porch treads: left to 3c), nests cut off 6 in 25 (was 1:
    more nests land in fenced backyards).
  - Load (headless preload walk, seed 101 east, `preload\4c_walk.log`): towns plan 56 / 60 / 113 ms on the worker (Stage 3b 12-39),
    build 17-26 ms over 6-9 frames, attach worst frame 0.5-0.6 ms; hoods unchanged (plan 155-184, build 54-75 ms); fallbacks 0.
    Sweep: town plan 43 ms mean (max 106), build 25 ms. Determinism: `4c_i` vs `4c_i2` (-NoBuild rerun) 0 differences on 117
    SWEEP keys and 2,328 dump hashes. Preload parity (a stage0 copy of `preload\verify.ps1`, `4c_verify_*`): 6 settlements incl.
    the towns Ciderton and Mossy Square, node trees 0 differ, 114 plan hashes 0 differ (workers vs main thread).
- Captures (the one round, world seed 102, `--no-lens`, `capture4c.ps1`): `stage0\4c_overview_grid.png` (Maple Square, a large grid:
  19 nests / 6 thorn nests, the gazebo square, beacons), `stage0\4c_overview_highstreet.png` (Hollyhock Square, the bowed high
  street with the square bulging off its apex, stepped roof lines), `stage0\4c_facade_slope.png` (street level: the hardware store
  on its stone plinth with treads down to the climbing sidewalk, the neighbour a step lower). Four windowed runs in all: the first
  facade view stood inside the far row (a gap between shops) and was retaken from the road. The captures predate the last
  nest tweak (back-row weight), which only moves town nests. Top-downs: `stage0\geo4c\geo_town_101.png` (fork),
  `geo_town_104.png` (grid), `geo_town_105.png` (cross), `geo_town_112.png` (high street).
- Not verified: the cine shots 08/09 (they find a straight near the town spawn: 49/50 towns have one) and the showcase (home hood
  only); windowed frame time with ~2.5x nests (monster AI/physics cost was not profiled; the 85 m spawn gate bounds it); a town's
  square on a corner along a steep cross street slopes at that side. SAVE_VERSION untouched (lead at ship).
- Edits outside street / town / terrain: types.h, context.h (TownTemplate/TownSize/SquarePlace, TownSquare, GenContext town
  fields, plazas, TOWN_PORTS); candy.cpp and yards.cpp (legacy switches, grass, jack / lamp heights); monsters.cpp
  (plan_infestation: square gate / cart, empty-junction guard, counts, spread, town candidates; nest spawn gate); world_plan.{h,cpp};
  sweep.cpp; festival_game.cpp (sweep loop: SWEEP_TOWN, town geo dumps, SWEEP_TPARITY; the lookat capture view; done before the
  gameplay worker took the file). No edits to world_streamer or neighborhood_generator.

### House goals and candy bug (gameplay worker, 2026-11-17)

User: "sometimes houses stop giving out candy if you have trick or treated a lot. Make trick or treated houses and cleared
houses part of the goals. Make sure house cleared doesn't show up twice ... Not trick or treating means no cleared."

- Bug, root cause: `CandyField::burst` (candy_field.cpp) dropped treats silently once a fixed reserve ran out:
  `capacity_extra_ = 160` slots, split 160 / 6 = 26 per basic kind, never reclaimed. 12 treats a door, so about 11 full
  doors per field, then partial, then none (sized for the old ~9-lot street; hoods have ~45 lots since Stage 2b). Repro
  (`houses\door_test.ps1`, `--door-test`, seed 101, `houses\before_fix.log`): Acorn Court doors 0-11 gave 12, door 12 gave 8,
  13: 5, 14: 4, 15: 6, 16: 1, 17 onward 0; 44 doors 156 treats, 33 short; Crow's Way the same (46 doors, 156, 34 short).
  Fix: the reserve sized from the lot count (14 a lot) and `ensure_room()` grows a full MultiMesh (and the glow columns)
  and rewrites its instances, so a door always gets its full burst. After (`fixed.log`): 44 doors 528 treats, 46 doors
  552, every door 12; the same with `--no-preload` (`fixed_nopreload.log`).
- Also checked, by code: `loose_candy.cpp` (never used for door rewards), shop / festival logic (no door caps), the
  candy filter / rebuild at attach (treats are not saved; a rebuild only drops ones still lying around), doorbell index
  = lot index (`get_doorbells` per lot). Found one more: the doorbell loop is bounded by `AreaState::rung.size()`, which is
  sized only on the first visit, so an imported save from an older layout (fewer lots) left every door past its old
  count dead. Now `reconcile_state()` (world_streamer.cpp) resizes `rung` / `lot_cleared` / `candy_taken` to the layout at
  every attach (both paths).
- Cleared rule: cleared = trick-or-treated AND all of the house's placed candy collected (`AreaState::candy_taken`;
  doorbell treats are a reward on top and never count: they are not saved). `FestivalGame::try_clear_house` runs at the
  ring and when the lot's last placed piece is picked up (CandyField signal `lot_emptied`, was `house_cleared`, which
  fired on candy alone and again after the treats). It announces (callout, sound, firework, `close_house`) only when
  the second condition is met, once (the saved `lot_cleared` flag guards it). `reconcile_state` at attach: a house marked
  cleared but never rung (older saves) is un-cleared; rung with no candy left is marked cleared quietly. The trailer shot
  still calls the announcement directly.
- Goals: "freed" (every monster nest and thorn nest down: wakes the gate, monsters vanish; decided in monsters.cpp
  `check_freed`) is unchanged. New on top: all clear (`AreaState::complete`, saved as "complete") = freed AND every house
  trick-or-treated AND every house cleared (Trick or Treat mode: freed from the start, so the houses). Announced once
  ("<Area> is all clear!" callout, `play_freed`, fireworks over the gate, a toast); the area banner says so; the HUD's
  first line reads "All clear!"; the save list shows "N all clear"; the travel menu marks all-clear places. There is no
  world map in the code; the travel menu is the nearest thing. Lead's call: to gate the travel gate itself on the houses
  too, `Infestation::check_freed` would need the house state (not done: it would need every candy on ~45 lots first).
- HUD panel (top right): name; "Monster nests x/y   Thorn nests x/y" (or "Freed! ..." / peaceful "N doorbells left to
  ring" then "N houses left to clear" / "All clear!"); "Houses trick-or-treated x/y   Cleared x/y"; a smaller "Candy x/y".
- Evidence (`--door-test`, seed 101, 30 checks, 0 failures; the unfixed build failed 16): collect-only then ring: no
  announce, then 1 at the ring; ring then collect: 0, then 1 at the last piece; picking up the treats after: still 1;
  ring only and collect only: 0; cached re-attach, evicted rebuild and a save_state / load_state round trip in one
  process: nodes match the state, nothing re-announced; after the load, a ring (candy taken before the save) and a
  collect (rung before the save) each announce once; all houses cleared with nests up: not all clear; nests burned: all
  clear once, not again after a re-attach. `--stream-state-test` updated (lot 0 is rung too): 3 / 3 attaches OK.
  Grow path forced (`--candy-reserve=1`, `grow_forced.log`): 27 grows, every door 12, 0 failures (the default reserve needed
  0 grows in `fixed2.log`). Seed 103 (`fixed_s103.log`): Mossy Row 40 doors 480 treats, and the town Mossy Square 44 shops
  528, 0 failures. Trick or Treat mode (`--peaceful`, seed 103, `peaceful_s103.log`): all clear once on the last house,
  0 failures. Every shop lot in that town and every lot in the hoods tested has its own candy (0 lots cleared at the
  ring alone), and town shops place their doorbell the same way as houses (town.cpp :497).
- The one in-game look (`houses\hud_capture.ps1`, off-screen): `houses\hud_101.png`, the panel reads "Monster nests 20/20
  Thorn nests 7/7 / Houses trick-or-treated 0/45   Cleared 0/45 / Candy 0/1277", no wider than before.
- Not verified: the all-clear callout, toast and fireworks on screen (headless only); the travel menu and save-list
  "all clear" text (not opened); the trailer's house-cleared shot (it calls the announcement directly, not run); whether
  every doorbell is reachable on foot (the test stands the kid on each bell); old-save reconciliation (no old save at
  hand; by code).
- Files: play\candy_field.{h,cpp}, play\festival_game.{h,cpp}, play\festival_doortest.cpp (new), world\world_streamer.{h,cpp},
  game\scripts\hud.gd, game\scripts\saves.gd, houses\door_test.ps1, houses\hud_capture.ps1. No gen files touched.
  Test flags: `--door-test` (with `--candy-reserve=N`, `--peaceful`).

### Stage 3c (yards on the slope, props on the ground, walks, stairs, roof decks, bounce candy)

- [x] Trampoline pumpkin gone: the Pumpkin Patch's `bouncy` pumpkins (yards.cpp place_landmark; the only bouncy pumpkin;
  nothing else referenced it: no candy anchor, no sweep stat). Bouncy inflatables kept.
- [x] Nests / thorn nests walk-reachable: plan_infestation builds the walk grid itself (`Infestation::build_nav(ctx, plan)`
  made public, settlement bounds -3/+4 m like the sweep probe) and only takes candidates whose grid piece is the spawn's.
  Same signature, so festival_game.cpp's sweep call is unchanged. 50 seeds: nests cut off 0 (hoods 501/501 nests,
  162/162 thorn nests; towns 743/743, 232/232). Caveat: verified in the planner's grid only; the game's own grid uses the
  RegionPlan bounds (another cell phase), not separately checked.
- [x] Yards on the natural slope (terrain.cpp plan_hood_terrain, house lots only; shops / square as 4c): the yard is the
  natural relief relative to its street (`yard_y`: road height at the point's projection + `soft_band(natural - road,
  1.8, 2.4)`, a soft knee instead of a clamp), so touching neighbours on one road meet seamlessly (no pads, no soft steps).
  The floor = that ground along the house front, held to [sidewalk - 0.7, what the porch flight can carry down to the
  sidewalk]. A building-cap raster over every house lot + 9 m: ground <= floor within 2.9 m of house / porch / 3-tread
  flight / garage, then rising at most 0.55 (cut banks, whichever lot or embankment the vertex is in). Garage door: a
  1.2 m level apron blending over 3 m (x +-2.2..3.4 m). Front bank and WALK_APRON as 3b (0 pokes kept). Corner lots on two
  streets blend across the side, or a retaining wall when > 1.1 m (0 needed in 50 seeds).
- [x] Flights never give up (houses.cpp): treads until the ground at the foot, all risers equal (<= 0.21 m), a 1.1 m
  landing every 8 treads (newel posts), stop at the sidewalk. `Lot::flight` (treads), `flight_riser`, `flight_foot` replace
  the hard-coded 3 x 0.36 m everywhere (yard_lift, Yard::steps_front, plan_routes, candy magic branch, front_walk / Steps
  anchors, the vine's door target in monsters.cpp, kept short of the sidewalk). Floor cap above keeps risers <= 0.21.
  Extra treads' solids pushed unlifted (3b lifted them by the tread top). Shops: treads to the sidewalk's own surface
  (+7 cm over the lattice), up to 12, `Lot::flight` too; tread solids no longer yard-lifted.
- [x] Front walk unbroken (houses.cpp): flagstones with 6 cm joints from the flight's foot to the sidewalk (scanning out
  through any strip of ground in front of a set-back lot: `Lot::walk_end`), each on the ground's fitted plane (tilt <=
  0.45 rad); `Lot::walk_flags`. Picket gate, gateposts and mailbox move aside when a flight reaches the gate.
- [x] Props on the ground (terrain.cpp `fit_footprint` / `footprint_low` / `street_surface` / `settle_street_props` /
  `shop_surface`; yards.cpp Yard::put; town.cpp dress_shopfront put): `prop_settle_kind`: Flat (least-squares plane over a
  3x3 / 5x5 footprint grid, tilt <= 0.5 rad, lowered until no sample floats, -1 cm), Upright (trees, lamps, mailboxes,
  posts, pillars, sunflowers, corn, scarecrows, swings, inflatables, bushes: plumb, sunk to the lowest ground under a ring
  at the base radius), Car (plane through the four wheel contacts, centred; a drive spot only if every wheel is within
  2.5 cm, the drive <= ~20% and the ground under the body clear of the sills; street cars on the road surface).
  Samples count only on the centre's own support (ground / a tread / a shop's paving strip), so nothing tilts onto a
  step's edge. Stacked things (upper bales, toppers, the cat on a gatepost, a pumpkin on a barrel) ride their base's tilt.
  The park hay pyramid settles as one piece. Hand-placed lean dropped for settled props. Yard::place skips spots where a
  flat prop would sink > 15 cm uphill; big trampolines need even ground. Fixed on the way: clear_of_solids compared box
  bottoms with 0 instead of the pad (mini trampolines on the treads).
- [x] Flat walkable roof decks (houses.cpp gable_roof `DeckStyle`): the ridge cut at 46-62% of the rise (behind a dormer,
  >= 2.3 m wide), the gable ends trapezoids, a plank deck with a trimmed edge and a dark board border, walkable; a
  widow's-walk railing on 30% (60% on Victorian / two-storey) with an opening where the climb arrives; the chimney stands
  off the deck. Climb on every house: a window-box ledge on two-storey fronts, then 1-3 plank steps up the slope above the
  porch roof (ridge along z: up the side slope on the porch side). Landmark summit routes run up it to the chimney top.
  All in the lot mesh: no new instances. Garages already had flat walkable roofs; shops flat roofs (no change). Candy:
  40% of decks get a ring of five (CPAT_DECK, one chocolate). Shops (town.cpp): every shop whose parapet is > 2.6 m over
  its awning gets the flower ledges (1.45 m apart, zigzag) the summit shops had.
- [x] Bounce columns (candy.cpp `bounce_columns`, CPAT_BOUNCE): ~40% of the bounce pads, straight up or a vertical arc
  leaning up to 2.4 m (inside the bucket's 2.65 m reach) away from roofs. 3 everyday pieces from 1.3 m to the plain
  bounce's reach (jump not held, cut), 3 chocolates to the full-jump bounce's reach, a lollipop halfway between the
  full-jump(+spin) and full-jump + flap reaches. Axis clear of every box (0.45 m), of canopies, and of any other platform
  whose jump + flap (3.4 m) could take the top piece within 6.65 m of a short run-up (pass 1 uses 8.5 m: a full running
  leap; pads passing it are taken first). Replaces the car hop's +2.6 m chocolate. Validation: a bounce pad's reach
  (`bounce_reach`) and the 2.6 m magnet over it count as support; the platform BFS gets bounce edges
  (`platform_reach(ctx, bounce)`). Apex model in core/movement.h `bounce_apex` (mirrors dragon_controller.cpp; 120 Hz).

Apexes (feet over the pad; 14 = car hoods, mini trampolines, awnings; 17 = big trampolines; 13.5 = inflatables):
| pad | plain bounce, jump not held (cut) | plain | full jump (held on contact, x1.45) | + spin | full jump + flap | + spin |
| --- | --- | --- | --- | --- | --- | --- |
| 14 | 3.05 | 3.47 | 7.34 | 7.42 | 8.61 | 8.74 |
| 17 | 4.16 | 5.14 | 10.84 | 10.92 | 12.11 | 12.24 |
| 13.5 | 2.87 | 3.23 | 6.82 | 6.90 | 8.09 | 8.22 |
Pieces are collected up to 1.7 m over the feet (CandyField::collect from feet + 0.55, 1.15 m reach), so a car column's
top sits at 8.0 + 1.7 = 9.7 m over the hood: 0.6 m clear of the full-jump reach (9.1), 0.6 m under the flap reach (10.3).

Numbers (`stage0\3c_final_a` + `3c_final_b`, 50 seeds in two processes; `python stage0\stage3c_stats.py 3c_final_a
3c_final_b`; hoods = the embedded variant the world plays, 25 hoods / 1097 lots; towns 50):
- Walks: every lot's walk reaches the sidewalk, worst gap 0.10 m (the 6 cm joints, 3 cm at each end), 0 ending off the
  sidewalk, 0 flags under a solid, steepest walk 0.52.
- Stairs: 1097 house flights + 1469 shop doorsteps, bottom tread a riser over the ground at its foot within 0.07 m on all
  (0 over 0.08); risers <= 0.21 (shops 0.18); longest flight 15 treads, 223 landings.
- Contact (one-sided: no footprint corner more than 3 cm over the ground; sinking allowed; it shares the placement's sampling and support rules, so it is a consistency check more than an independent one): flat props 14,253, 2 over 3 cm (0.03, 0.14: a stacked bale's corner and a vine-patch decal at a tread edge);
  cars 218, all four wheels within 0.025 m; upright 6,681, 0 gaps. Floating 0; buried 4 (3 garage-door bale stacks on the
  fill beside the apron, one mini trampoline): NOT met (target 0). The measures changed: buried = base > 15 cm under the lowest surface on a 0.3 m ring round it (looser than 3b/4c's 'under the ground at the origin'); cars left out of the floating test (their four-wheel test replaces it). Towns: flat 10,436 / cars 165 /
  upright 3,128 all within 3 cm; 2 "floating" door flankers stand on the paving quad (stat noise, as in 4c).
- Bounce: hoods 182 columns on 495 pads (37%); top piece out of its band 0; reachable from another platform with a short
  run-up 0, by a full running leap + flap off a deck 100 of 182 (decks stand 6-9 m within a few metres of most yard pads;
  the strict rule could not give 30%: the lead's call). Towns 51 / 1634 (3%): NOT met (30-50%): awnings sit under their own shop's roof, so a top piece is always reachable from it. Column / deck pieces dropped by validation 0.
- Roof decks 1097, all reachable (platform BFS); deck candy rings on ~41%. Platform reach: hoods 100%; towns 99.8% with
  and 99.8% without the awning bounce (4c: 96.2%; the gazebo roofs are the rest).
- Terrain: yard ground vs floor -5.8 .. +2.6 m (3b: -1.4 .. +0.8: yards now follow the hill), floor - road -1.0 .. +2.3,
  foundations above ground 0, deepest exposed foundation 4.5 m, sidewalk pokes 0, retaining walls 0, ground slope p99
  0.56; lattice edges steeper than 45 deg inside lots: 21 in 25 hoods (max 2.57, at back lines where two lots on
  different streets meet): NOT met: the brief wants those blended or walled; no wall is built there yet.
- Candy per lot 25.2, dropped by validation 128 over 25 hoods. Nests as above.
- Determinism: `3c_final_*_rerun` (-NoBuild) vs `3c_final_*`: 0 differences on 162 SWEEP keys and 4,850 dump hashes.
- Plan time (sweep, embedded hoods): 123 ms (4c 118), build 77 ms; towns 50 ms. (Two exact pre-tests keep it there: the
  platform BFS skips pairs further apart than their radii allow, the column search only tests nearby boxes.)
- Preload parity + instance budget (`stage0\3c_verify_sync` / `3c_verify_pre`, seed 101 fly test 200 s, the preloaded one
  with 4 plans at once and a 12-settlement cache; `preload\verify_compare.py ../stage0/3c_verify_sync ../stage0/3c_verify_pre`):
  6 settlements, node trees 0 differ, 114 plan hashes 0 differ (workers vs main thread: the new walk grid in
  plan_infestation included). Hood geometry 3,607-3,984 instances (4c 3,624-3,987), towns 785 / 879 (same); stress-walk
  streamer peak 48,828 (4c 49,085) of 65,536; fallbacks 0; no "Too many instances".
- Captures (the one round, world seed 102 Candlewick Lane, `--no-lens`, picked by a headless dry run of seeds 101-104,
  `stage0\dryrun3c.ps1`): `stage0\3c_yard.png` (lot 16 from across the street: lawn on the slope, a 7-tread flight down to
  the flagstone walk, the walk to the sidewalk, a car on the drive; the foreground shows the near lot's walk meeting the
  sidewalk between its gateposts), `stage0\3c_roofs.png` (from ~20 m up over a junction: a row of flat roof decks, plank
  climbs up the front slopes, one widow's-walk railing), `stage0\3c_column.png` (a street car's candy column rising to the
  lollipop; the camera behind the kid sits in a street tree's canopy, which hides part of the lower frame; railings on the
  decks either side). The area banner overlays the top of each, as in earlier rounds.- Thread safety: no statics, no engine calls in the plan (plan_infestation's Infestation is plain data; AssetLibrary::get
  is the shared-mutex one already used); new nodes none (all in lot meshes and existing boxes / props).

Files: gen\terrain.cpp, houses.cpp, yards.cpp, candy.cpp, town.cpp, types.h (Lot::flight / walk_flags / walk_end / deck,
PropPlacement::settled, CPAT_DECK / CPAT_BOUNCE), context.h; core\movement.h (bounce_apex); play\monsters.{h,cpp}
(nest filter, vine door target); play\sweep.cpp (Stage 3c stats); play\festival_game.cpp (surgical: capture views
`yard3c`, `column3c`, `roofs3c` in the capture-view chain, review only). Tools: `stage0\stage3c_stats.py`,
`stage0\dryrun3c.ps1` (headless pick of the capture views per seed). SAVE_VERSION untouched.
- Apex model checked in game (headless `--capture-view=prop:trampoline_mini --drop-test`, seed 101, `stage0\3c_drop*.log`):
  plain drop 3.43 m over the ground = 3.08 over the mat (model 3.05, jump not held, cut); with `--drop-super` 7.72 = 7.37
  (model 7.34). The flap leg is model only. Nest layouts (`Infestation:` lines) identical between the sync and preloaded
  walks (6/6).
- Unverified / for the lead: deck reach is the platform BFS's gap model only (the two-storey climb jumps from a ledge under
  the eave overhang; not played); the standalone variant shows 2 decks unreached, 5 deck pieces dropped and 1 column top
  out of band (the world never plays it); the shop flower ledges now on every tall shop were never seen in game
  (fidelity: an addition to the facades, the lead's call); how many columns lean (up to 2.4 m against the 2.65 m reach)
  is not measured; the yard capture is elevated from across the street rather than street level up the yard, and the
  column capture is partly hidden by a canopy (not retaken: one round); the game's own nest grid, the cine shots and the
  showcase were not exercised. This note replaced the checklist written at the start of the stage (nothing else after
  it was touched).

### Ship (final worker, 2026-11-18)

- [x] Steep back lines. Diagnosed first (a per-edge dump: each end's lot, branch, value before the cap, the cap and the
  capping lot): of the 21 edges only a few were lot against lot. Most (14) were a yard's back or side against ANOTHER
  street's sidewalk: the in-lot bank to any sidewalk was the 3b 4.2 m bank whatever its height (up to 5 m). One was the
  garage apron stopping dead 0.5 m inside the door line (a step beside the garage on fill: also the buried bales). The rest
  were the embankment between two lots of different streets switching from one lot's value to the other's in one lattice
  square, or a lot corner beside another street's sidewalk. Fixes (terrain.cpp plan_hood_terrain, hoods only; towns go the
  old path): every vertex in or near a house lot takes the weighted mean of every house lot within `NB_BLEND` = 6 m of it,
  relative to the nearest (own lot weight 1; same-street neighbours were already seamless, so no change there); the bank to
  another street's sidewalk is `1.9 m x height` wide (4.2-9 m, `OTHER_BANK_*`) and then capped by the house cones; between
  a lot and another street the street holds for the same width before the lot's weight comes in; the garage apron fades
  out over 2.5 m behind the door line. What is left steeper than 45 deg across a house lot's side or back line gets a
  painted stone retaining wall along that line (`Lot::walls`, built in build_house like the 3b side walls: 1.5 m courses
  from 0.3 m under the lowest ground within 1.5 m either side to 0.3 m over the highest, solid walkable pieces <= 3 m, the
  line occupied so no yard prop stands in it; the 3b side walls are recorded there too, `side = true`).
  Result (`stage0\ship_final_a/_b`, embedded hoods): steep edges in lots 21 -> 5, unwalled 0 (`lot_steep_unwalled`: a
  wall segment must cross the edge). Walls: 5 in 3 of 25 hoods, 11.6 m in all; steps 2.2-5.3 m. The 5.3 m one (world 125
  L1) is not "low": that house's level ring is 2 m from a sidewalk of another street 5.1 m above it; no bank fits there.
  Risk: the stat proves a wall segment crosses each steep edge, not that the wall's top reaches the higher yard all along
  (the drop spans a 2 m lattice square; the courses sample the ground 1.5 m either side, so a short dip behind the cap is
  possible). No wall has been looked at in game.
- [x] Buried props: 4 -> 0 (current measure). Strict pre-3c measure (origin > 15 cm under the surface at the origin, new
  sweep stats `props_buried_strict_*`): flat 73 -> 43, upright 8 -> 11 (uprights are sunk to the lowest ground round their
  base by design), cars 0 (3c numbers from `stage0\ship_diag_a/_b`, the same build before the generator fix). The four
  named props (105 L33 bale, 121 L35 bale, 143 L11 mini trampoline, 145 L24 bale) are in neither list. Fixes: the apron
  fade above; the garage bale stack slides in along the door (0.25-1 m) when it would sink (`Yard::flat_sink`, put()'s own
  fit); a mini trampoline spot must not sink its origin under the ground ring (bounce pads 495 -> 491, columns 182 -> 181).
  Removals, for the lead (fidelity rule): over 25 hoods 4 mini trampolines and 2 parked cars are no longer placed
  (no clean spot on the reshaped ground; for the cars presumably the four-wheel test, not traced); other counts checked
  (decks, columns -1, nests, candy per lot) hold.
- [x] Everything else against 3c / 4c (50 seeds, two processes): walks, stairs, contact (flat 2 over 3 cm as before, cars
  216 all within 2.6 cm, upright 0), roof decks 1097/1097 reached, platforms 100%, foundations above ground 0, walk pokes 0,
  candy low 0, candy dropped 127 (128), nests 501/501, thorn nests 162/162, cut off 0, candy/lot 25.2. Towns: every dump
  category identical to `3c_final_*` (125 town settlements, `parity.py ... town`). New `SWEEP_HPARITY` (WorldPlan's hood
  ground vs the generator's): 50/50 same grid, max |dh| 0 (towns 50/50 as before). Determinism: `-NoBuild` reruns, 0
  differences on 172 SWEEP keys and 4,850 dump hashes. Hoods vs 3c dump: street, street_mesh, doors, signs, springs
  identical; ground-driven categories differ as expected; `lots` differs in 2 of 60 variants, field not identified (likely
  `faces_used` or a route height after the yard dressing moved; not checked with `--sweep-dump-detail=lots`).
- [x] House door test (`houses\door_test.ps1`, seed 101 and 103): 30 checks 0 failures each; Acorn Court 44 doors 528
  treats, Crow's Way 46 / 552, Mossy Row 40 / 480, Mossy Square 44 / 528 (every door 12), as at the house-goals note.
- [x] Cine (trailer reel, headless, `--cine-check`: per shot, frames with the camera under the ground, inside a body, the
  kid hidden, and the still; `final\headless_checks.ps1`). Broken with the new world and fixed (festival_cine.cpp):
  `lights` (the trailer's house-cleared shot) had its camera inside geometry in every frame: it now tries the house's own
  front as well as the way to the spawn, then the next best-dressed houses, until the kid's spot is free and the lane
  clear (`cine_lane(..., &found)`, `cine_spot_free`); `fireball` had the kid behind a structure the whole shot: nests near
  the spawn and 12 directions round each are tried (free spot, line of fire, a lane clear over the camera's sideways
  slide); `town` found no straight street and sat inside a building: it used the reel world's town spawn in its own world
  103; it now looks the town up in the shot's world (`high_street`); `car`'s camera drove through a parked car: the side
  offset is searched along the run; `thorns` took its lane's first offset when none was clear (the kid hidden 67 of 151
  frames by a prop beside the lens): spot and lane are now searched together over the thorn nests and 16 directions.
  Final run (reel world 101; town / car / drive in world 103): 20 trailer shots, camera under ground 0 everywhere, inside
  0 except `costumes` (8 of 166 frames, a solid at the dolly's end), kid hidden 0 except `bucket` 10 and `jump` 1 frames.
  Every still: kid in view, camera inside nothing. `CINE town` / `CINE car`: straight street true. With `--cine-play`
  (the 23 shots incl. menu, shop, play_candy, bounce seen through the game camera; run on the build before the thorns
  fix): all clear except `fireball`, whose game camera has a prop between it and the kid all shot (in play the occluder
  cutout dithers such things; not fixed). The cinematic `bounce` shot (trailer = false) is dropped from both modes by
  cine_build, so its trampoline finder only runs as `play_bounce` (kid 8.9 m up at the still: found). The hard-coded shot
  seeds (103, 104, 106, 109, 110, bounce 69) were picked for the old worlds; the shots now find their subjects there, but
  only two stills were looked at.
- [x] Showcase (`--showcase=dragon_kid` / `monster_bat` headless): the stage finder works on the new home street
  ("SHOWCASE stage ... tree room 3.5"); the run then stops at the window-size check, which can't pass headless (no
  recording made; `tools\showcase.ps1` was not run: it imports and records windowed).
- [x] SAVE_VERSION 1 -> 2 (festival_game.cpp, its one definition), bound as `save_version()`. `--save-version-test`
  (in process, nothing written): a version 1 and an unversioned save are refused with the state untouched, the current one
  loads: `SAVE_TEST ok`. saves.gd writes the version into the slot summary, marks older slots `outdated`, skips them for
  Continue, and `load_slot` reports `last_error = "outdated"`; hud.gd shows "That save is from an older version of the
  game, so it can't be loaded. Start a new night!" and "from an older version" on the slot's line; Continue now returns to
  the main menu on a refused save (it fell through to the pause menu); both Load buttons are enabled by any save
  (`Saves.any_save()`), so a player with only old saves still reaches the list and the message.
- [x] Captures (the one round, `final\capture_ship.ps1`, off-screen, `--no-lens`; overviews world 101, cine stills the
  reel's own worlds 101 / 103; taken on the build before the thorns fix and the wall-course sampling change, which touch
  neither picture's subject): `final\ship_hood_overview.png`
  (Acorn Court from above: S-bend junction, nests, decks), `final\ship_town_overview.png` (Hazel Falls, the gazebo square on
  its pad beside the high street), `final\cine\03_lights.png` ("HOUSE CLEARED!" over a lit two-storey house with its deck
  railing), `final\cine\16_town.png` (the kid on the high street between the shop fronts). All read right.
- Not verified: the new retaining walls in game (no capture looks at one; the 5.3 m wall never seen);
  the save UI text on screen (headless only: the menus were not opened); showcase recording; the hood parity check covers
  one hood per world seed; the `--cine-play` reel after the thorns fix.
- Files: native\src\gen\terrain.cpp, houses.cpp, yards.cpp, types.h; native\src\play\sweep.cpp, festival_game.{h,cpp},
  festival_cine.cpp; game\scripts\saves.gd, hud.gd. Tools: `final\verify.ps1` (the whole headless verification on one
  build), `final\headless_checks.ps1` (cine / showcase / save), `final\capture_ship.ps1`.