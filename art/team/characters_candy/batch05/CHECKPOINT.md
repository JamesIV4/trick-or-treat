# batch05 checkpoint: player rig and animation (Lane P)

## Progress (final fixes 3) - keep this current

Task (2026-11-16, reviewers' strips in `after\review\v3`): 1 witch skirt flips up on the stop (+ check the other
costumes' soft parts on 810-840); 2 nitpick: witch pound slam crouch hides the face under the brim.
STATUS: FINISHED (2026-11-16 ~06:10). Nothing in flight. Only `native/src/play/kid_rig.cpp` changed (kid_poses.cpp was
edited for a trial and reverted to its previous text).
- DONE final run, one build (DLL 06:00): `tools\showcase.ps1` all six into `wip_p` (TOT_RIG_DEBUG on, trace only), 6 of
  6, every beat sheet "frames aligned" (`wip_p\ff3\final_run.log`, `final_sheets.log`, `final_guard.txt`).
- `head_showcase.ps1 -Name witch_head_ff3 -Id kid_witch -Candy 4`: 95 / 8 / 11 / 114 (= accepted).
- No model rebuilt (gates / verify_rig2 not needed). Scratch clips deleted (traces, logs, strips kept).
  `game\override.cfg` was present at the end, but it belongs to Lane M's monster showcase that was recording at
  06:05 (its powershell + Godot were running); not a leftover of this lane.

## Final fixes 3 (2026-11-16)

| Item | Cause (measured) | Fix | Result |
|---|---|---|---|
| 1 witch skirt flips on the stop | the ghost's three causes on the witch's `soft_skirt` (preset `skirt`, limit 0.32): knee push, the spring's forward swing into its hard limit (state -0.64, shown -0.31 plateau 826-830), the skid's lean back on a skirt rigid on the body. Shown -0.41 rad @822; front hem edge 26 cm at rest -> 59 cm (@822), `wip_p\guard\ff3w0.out` | `kid_tuning` witch row: `stop_fwd 0.07 / stop_knee 0.4 / stop_lean 1.0` (the ghost's stop hold, same values; no new code) | front edge peak 33 cm (lift 33 -> 6 cm), back 23-31 cm: a sway of a few cm, the underside no longer shows. The same hold also takes out a second flip at the end of the roll beat, where the kid runs out of the roll and stops (front edge 54 cm @1395 -> 28 cm). Walk, run, jump, glide, spin: the rig trace is identical frame for frame |
| 1b other costumes, 810-840 | looked at the `after` clip strips (`wip_p\ff3\other_stop_<id>.png`) and the tail trace | none needed | dragon tail rests on the road (raw -24 cm, held at +1 cm by `keep_tail_above_ground`, no whip; behind the body from this camera); cat tail 7-37 cm above the road, a smooth curl, no whip; pumpkin shell and leaf collar and skeleton hood: rigid on the body/head, no flip |
| 2 witch pound slam crouch (nitpick) | head pitch +0.43 rad @1483 (dive pose 0.22 + the land's body dip + the pound's head kick) under a high camera; the brim hid the face 1479-~1491 | hat only (`bob_hat`, only the witch has one): while the pound's landing plays (`land_s_ > 1.2`, only the pound lands at 1.35) the hat tips back with the crouch, `-0.3 x smooth_pulse(land_t_, 0.05, 0.1, 0.3)` rad. A chin lift (-0.12 x the land pose's late pulse) was tried too (`ff3w2`): no more face than the hat alone -> reverted (decision 12: head untouched) | PARTLY: the lower face and the (blinking) eyes show from ~1488 instead of ~1491-1492; 1480-1487 the brim still covers the face from that camera height. More would need a much bigger tilt or lifting the head against the crouch |

### Proof

- Strips (A = `after`, N = `wip_p` final, same clip frames): `wip_p\ff3\witch_stop_final.png` (814-837),
  `witch_stop_w1_zoom.png` (816-831, legs/skirt zoom), `witch_rollend_final.png` (1384-1407), `witch_gp_final.png`
  (1476-1499), `witch_gp_w2_zoom.png`. Witch beat sheet `wip_p\kid_witch_beats.png` looked at.
- Rig trace frame by frame (`wip_p\guard\ff3w0\kid_witch.log`, old code, vs `wip_p\kid_witch.log`): only the skirt's
  numbers (SHEET, FRONT/BACK) differ, in stop_idle 814-899, the end of the roll 1379-1409 (the run-out stop) and
  1767-1781 (hurt recovery, at most 1.5 mm); every other field and every other beat identical.
- `final_guard.txt` vs `ff2\final_guard.txt` (per-beat roll / pound / hurt / jump / stop summaries of all six): only
  the witch's stop lines differ.
- Per-beat pixel diff vs `after`: `wip_p\ff3\clip_diff.txt` (240x270) and `crop_diff.txt` (full-res crop on the kid):
  every id and beat at the particle noise floor (fireflies and the pound / candy effects are random per run).

### Which ids changed

kid_witch only: stop_idle (skirt), the end of the roll beat (skirt on the run-out stop), the ground-pound landing
~1478-1500 (hat tip). dragon_kid, kid_ghost, kid_cat, kid_skeleton, kid_pumpkin: unchanged (code paths unreachable for
them; guard summaries identical; pixel diff at noise).

### Weak points

- Witch pound: the face is hidden for 1480-1487 still (see table).
- Witch stop: the striped leggings show between the knees for 2-3 frames on the halting step (820-822); the hem stays down.

## Progress (final fixes 2) - keep this current

Task (2026-11-16, reviewers' strips in `after\review\v1`): 1 ghost hem swing on the stop, 2 cat onesie reads as glossy
vinyl, 3 re-check hang / climb / vault / car / swap / trampoline / roll off a kerb and on a slope / `--anim-v1` / jitter
after the first round's roll guard and sole floor; the wand in the roll.
STATUS: FINISHED (2026-11-16 ~04:58). Nothing in flight. Details and hand-off: "Final fixes 2 (2026-11-16)" below.
- Two workers: the first stopped ~04:09 mid-way (its code was kept and verified); the second finished.
- DONE final run, one build (04:44): `tools\showcase.ps1` all six into `wip_p` with TOT_RIG_DEBUG on (trace only), 6 of
  6, every beat sheet "frames aligned"; `wip_p\ff2\final_guard.txt`, `ff2\clip_diff.txt` (vs `after`).
  `head_showcase.ps1 -Name ghost_head_ff2 -Id kid_ghost -Candy 4`: 94 / 8 / 11 / 113 (= accepted).
- kid_cat rebuilt (first worker, 03:32): gates PASS 28,440 tris, verify_rig2 PASS, catalog has `hood`; asset_preview
  looked at. No other model rebuilt. Native build OK. `game\override.cfg` absent. Scratch clips deleted (logs, traces
  and strips kept).

## Final fixes 2 (2026-11-16)

### What changed

| Item | Cause (measured) | Fix | Result (measured) |
|---|---|---|---|
| 1 ghost hem on a stop | the halting step's knee push plus the spring's forward swing, AND the skid's 0.3 rad lean back: the sheet is rigid on the body, so the lean alone tipped its front hem up. Hem edge (trace `FRONT/BACK`) 25 cm at rest -> 51.7 cm at 822-824 (`wip_p\guard\f3n`, = the after clip) | `KidTuning::stop_fwd 0.07 / stop_knee 0.4 / stop_lean 1.0` (ghost only) with `KidRig::stop_calm_` (renewed by the skid, fades 0.5 s): during a stop the hanging hem's spring may not swing forward past 0.07 + (1 - hold) x 0.5 rad, only 0.4 of the knee push applies, the body's lean back is taken out of the sheet, what is shown eases into 1.25 x stop_fwd | hem edge peak 31.7 cm (lift 27 -> 7 cm), legs covered to about the ankle at the worst frame, a small sway 814-846 (edges 27-32 / 15-27 cm). Walk, run, jump, glide: identical (hold is 0 there; trace numbers equal, strips `ff2\ghost_walk/run/jump_glide.png`). The shown bend in the sheet's own space now peaks +0.48 rad at 821 (was -0.18): that is the counter-tilt against the lean, the hem itself hangs. Proof: `ff2\ghost_stop_zoom_final.png`, `ff2\ghost_stop_816_838.png` |
| 2 cat onesie | default highlight (pow 24 spec) on the black plush: hard streaks on the hood's back, body, tail | `KidTuning::plush_gloss -0.85` on body, arms, forearms, hands, legs, tail and the hood (split into its own static `hood` node under the head in costume_cat.py so the face keeps its sheen); ears, bow, bell, face, shoes untouched. -1.2 flattened the tail and fur roll, -0.7 left a brighter sheen | matte plush, a dim broad sky sheen keeps the form at night; no hard streaks. `ff2\cat_gloss_cmp_final.png` (turntable 0/120/180/264, face 420, idle 1990), `ff2\cat_zoom_180_420.png`, `art\review\engine\kid_cat.png` |
| 3a roll float | the roll guard's raise eased down (0.05 s) as soon as less was needed: as the ball came out of the head-down part it floated up to 10 cm for ~5 frames (real play kerb roll, and the showcase roll 1367-1394) | while rolling the raise follows the geometry both ways; it eases out only after the roll | the lowest point sits on the floor (+0.6 cm) while raised; raise frames 1355-1367 (was to 1394); roll strips `ff2\roll_all6.png` (no pop) |
| 3b hem flick on a landing | real play (autopilot, `real\ff2_runstop.log` t 26.18): landing into a run, the hem guard turned the sheet 0.47 rad for one frame to lift a point near its axis (small lever) out of the floor by 1.4 cm | hanging-hem guard capped at 0.15 rad per frame (never engages in the showcase: raw hem clearance >= 0.5 cm in every beat) | spike gone (`real\ff2b_runstop.log`: peak hem edge there 61 -> 48 cm, no one-frame spike) |

### Re-checked in real play (dragon + ghost; `wip_p\real\*ff2_*` logs and strips)

- hang + climb (`hang_`, `hangtop_`), vault, trampoline, car (mount, drive, hop, dismount), misc (pause, fall-out),
  costume change in play (`swap_ff2_all`, 12 changes incl. in the air / mid-spin): no lift, float or pop seen; the guard
  and the sole floor are off in hang / mounted / climb as designed.
- Rolls (`TOT_RIG_REVIEW=rolls`, `rolls_ff2b_*`, trace on): kerb + 8 deg slope down / up / across. With 3a the guard
  adds nothing once the ball is clear. Still: (1) the TRUE ground under the lowest point is up to 2.4-3.9 cm ABOVE it on
  the slope (the guard uses the controller's own floor; the leading part of the ball dips 3-4 cm into a rising slope
  for a frame or two); (2) mid-tumble the raw roll pose itself floats 12-26 cm while the kid lies flat (tumble about the
  fixed ROLL_CENTER, not the guard). Not fixed (needs a ground ray per probe / a new tumble centre).
- Wand in the roll, all six (final trace): lowest wand point >= +0.5 cm (dragon and cat are held at +0.5 by the guard,
  which now probes the wand; ghost +9, witch +10, skeleton +6.5, pumpkin +2.8).
- `--anim-v1` (v1check3 vs v1check2, dragon + ghost): noise floor. Jitter (`jitter\ff2_*`): model 0.00 px median, p95
  <= 0.04, identical to before.

### Which ids changed

- kid_ghost: the stop (814-846) and the end of the roll. kid_cat: every beat (material, clip_diff 2-7 over the noise),
  plus the end of the roll. dragon_kid, kid_witch, kid_skeleton, kid_pumpkin: only the end of the roll (1365-1394,
  less float); every other beat at the particle noise floor against `after` (`ff2\clip_diff.txt`).

### Review tools added

- Trace `FRONT= BACK=` (hanging hem's front / rear edge above the floor), `WAND=`, `TRUE=`; guard_trace.py prints hem
  edge heights per beat and per frame on the stop. `TOT_RIG_NOSTOP=1` turns the ghost's stop hold off (before trace).
- `TOT_RIG_REVIEW=rolls` (dragon_controller.cpp): roll off the nearest kerb, then down / up / across the nearest
  8-30 deg slope; `real_batch.ps1 -Scenes rolls` (trace on).

### Known weak points

- Ghost: the shoes still show under the hem for the halting step (818-822); the sheet tilts with the stepping leg there.
- Cat: the ear shells keep the default sheen (a soft highlight along the outer edge from behind); the onesie is matte
  but reads smooth velvet more than pile at gameplay distance.
- Rolls on slopes: see above. Older avi leftovers in `wip_p\hang\hang.avi`, `wip_p\lifthang\kid_ghost.avi` (not this round).

### Technique notes worth writing back to art-pipeline

- **A rigid-on-the-body sheet tilts with every body lean:** in a stop the brace leans the body back and the hem's front
  rides up regardless of the spring. Take the body's lean out of the hem's bend (counter-tilt) while a stop holds;
  measure the hem's edge height, not the bend in the part's own space.
- **A raise that eases down floats:** a ground guard for a tumble must follow the geometry both ways while it is
  active and ease only when it switches off.
- **Cap a hanging hem's ground correction:** a point near the axis has a tiny lever, so "lift by bending" asks for a
  huge turn; a one-frame cap avoids a flick.
## Progress (final fixes) - keep this current

Task (2026-11-16, last fix round from the two reviewers' strips in `after\review\g1`): 1 ghost hem flare on the stop,
2 head-top parts into the road in the roll (all six), 3 anything else under the road in roll / pound landing / hurt.
STATUS: FINISHED (2026-11-16 ~02:45). Nothing in flight. Details and hand-off: "Final fixes (2026-11-16)" below.
- DONE (C++, builds): sheet preset retuned; roll ground guard (`KidRig::ground_guard`); roll pose straightens while
  head-down (`kid_pose::roll_head_down`); hat squash + head-top fold in the roll; sole floor (2.5 cm) in `apply()`.
- DONE traces (`guard_trace.ps1` -> `wip_p\guard\g1..g4` + `g<n>.txt`), real play (`wip_p\real\ghost_runstop.*`).
- DONE final run, one build: `p4_review.ps1` all six into `wip_p` (`final_fixes.log`, 6 of 6, every beat sheet "frames
  aligned", strips cut). Proof strips (A = after, W = wip_p, same clip frames): `wip_p\final_fix\*.png`.
  `head_showcase.ps1 -Name ghost_head_final -Id kid_ghost -Candy 4`: snaps 94 / 8 / 11 / 113 (= accepted).
  `game\override.cfg` absent. No model rebuilt (gates / verify_rig2 not needed).
- Ghost roll lump / open hem (nitpick): wrap 0.14 and roll bend -0.7 tried (g3, g4), no visible change -> reverted.

## Final fixes (2026-11-16)

### What changed (all in `kid_rig.cpp` / `kid_rig.h` / `kid_poses.cpp`)

| Item | Cause (measured) | Fix | Result (measured) |
|---|---|---|---|
| 1 ghost hem flare on the stop | `sheet` preset 1.0 Hz / zeta 0.42 / drag 0.35 / inertia 0.85 / limit 0.42: the run stored a trail (spring +0.58) and the stop threw it forward into the spring's hard limit (-0.84); shown -0.40 rad held frames 826-840 (soft limit plateau), then +0.21 back swing to ~880 (`wip_p\guard\g1.txt`) | `sheet` 1.5 Hz, zeta 0.7, drag 0.18, limit 0.28, inertia 0.45 (only the ghost has `soft_sheet`) | stop: -0.18 rad forward peak at 830, back to 0 by ~846, no back swing, 0 frames over 0.2 rad (was 44). Real play (autopilot, `real\ghost_runstop.log`): stops peak -0.13 forward; landings: hem >= +1.9 cm |
| 2 head-top parts into the road (roll) | the tumble turns about a fixed point 0.5 m up the kid's axis; the tuck puts the neck in front of it, so the big head swings out: lowest point raw -16 (ghost) / -37 (witch hat) / -23 (cat ears) / -20 (dragon head) / -23 (pumpkin) / -22 (skeleton) cm | (a) `kid_pose::roll_head_down(k)`: while the head points down the back / neck straighten (body 0.42 x (1 - 0.6 d), head 0.5 x (1 - 0.7 d)); MORE tuck measured 3-6 cm WORSE. (b) `bob_hat` squashes (y 1 - 0.5 d, xz 1 + 0.12 d) and soft parts on the head that do not hang (`Soft::on_head`, reach 0: ears, hat tip, stem, tendrils) fold back 0.6 d. (c) `ground_guard`: points on every mesh (`Probe`, 1.5 cm cells, shader bends followed) and, while rolling, the controller's node is raised by exactly the lowest point's depth (+0.5 cm), easing back out (0.05 s) | raw lowest now -12.6 / -20.8 / -14.2 / -18.4 / -20.8 / -19.3 cm; after the raise +0.5 cm on every frame of every costume (TOP and BODY "now"). The ball rises up to 13-21 cm while head-down (it rolls over its head / hat) |
| 3 others | pound landing: the dive's stretch (scale 1.14) turned about the air pivot while the grounded weight eased in, plus the landing's pelvis dip: shoes -6 to -8 cm for ~5 frames (1475-1479), all six; after the roll -4 cm | on the floor (not rolling / mounted / hanging / climbing) the visual is raised so no sole is more than 2.5 cm under the lower floor under the feet (`apply()`, kerbs kept) | pound -2.0 cm, after roll -2.5 cm (the tolerance), hurt +0.1 cm; walking / stop unchanged (-1.8 cm, inside the tolerance) |

Head trace: ghost candy 4/s snaps 94 / 8 / 11 / 113, kinks equal; only the walking vertical-accel max rose (72 -> 179 m/s^2,
one frame, the roll's landing counted as walking).

### Review tools (new)

- `guard_trace.ps1 -Name <n> -Ids <...>` (TOT_RIG_DEBUG showcase into `wip_p\guard\<n>`) + `guard_trace.py <dir> <ids>
  [--rows a-b]`: per beat the lowest point of the head's parts (TOP) and of the whole kid (BODY, on-floor frames only
  outside the roll) before / after the guard, the part holding it, the LIFT, and the hanging hem's bend on the stop.
  Trace fields added to RIGDBG: `TOP raw= now= BODY raw= now= LIFT= OF= LOWPART= SHEET x y z d x z`.
- `strip_cmp.py --id <id> --frames a-b <dir> <dir> ...`: labelled strips of the same clip frames from several clips.
- `clip_diff.py <dirA> <dirB> <ids>`: per-beat mean pixel difference (particles give a floor of 1-3, up to ~20 in the
  candy / pound beats where the effects are random).

### Which ids changed

All six, in the roll (posture while head-down, raise; witch hat squash, cat ears / pumpkin stem / tendrils / witch hat
tip fold) and the first frames of the ground-pound landing (sole floor). kid_ghost also in every beat where the sheet
swings (stop, run trailing less, landings). Outside those beats the other five measure at the particle noise floor
against `wip_p\prefinal` (`wip_p\final_fix\clip_diff.txt`).

### Known weak points

- The roll now visibly lifts the ball 13-21 cm while head-down (rolls over its head / squashed hat for ~5 frames); the
  witch's roll beat still frame (f1365) reads as a headstand on the squashed hat. The root cause is the fixed tumble
  centre (`ROLL_CENTER` 0.5) for a big-headed kid; a tumble about the tucked ball's real centre would shift the
  visual ahead of the body mid-roll and was not tried.
- Ghost roll: the sheet still balloons into a lump at the start and the tucked shoes show at the open hem mid-tumble
  (wrap / bend tried, no visible change; the legs would have to tuck lower).
- Ghost stop: the leggings still show for a few frames under the front hem on the stopping step (knee push + forward
  swing), far less than before.
- The probes skip the wand (attached by the controller after bind).
- Not checked: hang / climb / mounted / `--anim-v1` again (the guard and the sole floor are off when hanging, mounted
  or climbing; the roll pose is v2 only).

### Technique notes worth writing back to art-pipeline

- **A soft limit on a spring hides a plateau:** when the spring's own state runs past the shown limit (tanh soft
  limit, hard limit 2x), the shown bend sits at the limit while the state comes back: a "stiff cone held" for 15
  frames. Trace the spring's state, not only what is shown; fix with damping and less stored drag, not a smaller limit.
- **A ground guard for a tumble:** sample every mesh (one vertex per 1.5 cm cell), follow the shader bends, and raise
  the tumbling node by the lowest point's depth: the ball rolls over whatever sticks out. Fold / squash the tall soft
  parts first so the raise stays small.
- **Tucking tighter is not always closer:** with the turning point behind the neck, pitching the head forward swings
  it OUT; measure before assuming.

## Progress (real-play verification) - keep this current

Task (2026-11-16, after the polish round): verify in REAL play what the showcase does not show, for all six costumes:
hang / climb, vault, mounted, trampoline, costume change in play, pause (photo mode), the fall-out respawn, head trace
(witch / skeleton / pumpkin), judder (one hooded, one soft-part costume), `--anim-v1`, autopilot frame rate.
STATUS: FINISHED (2026-11-16 ~01:45). Details: "Real-play review (2026-11-16)" below. Nothing in flight.
- DONE final: `real_batch.ps1 -Scenes hangtop -Tag fin_` (6 of 6) and `tools\showcase.ps1` all six into `wip_p`
  (`real_final.log`, 6 of 6, every beat sheet "frames aligned"), gates + verify_rig2 PASS for all six,
  `game\override.cfg` absent. Stills of dragon / cat / ghost / skeleton vs `polish_prev`: mean diff 1.9-3.2 (noise).
- DONE: `p3_hang.ps1` and `showcase_v1.ps1` launch through `tools\godot_bg.ps1`.
- DONE (C++, builds): review switches `TOT_RIG_COSTUME` / `TOT_RIG_REVIEW` / `TOT_RIG_CAM` (`dragon_controller.cpp`,
  documented at its top and in "Real-play review" below); `real_play.ps1` (one run), `real_batch.ps1` (all six x
  hang / hangside / vault / tramp / car / misc + swap), `real_checks.ps1` (head traces, jitter, v1, autopilot).
- DONE round 1 (`wip_p\real\real_batch.log`): 31 clips. FOUND: a costume change in the air / mid-spin showed the
  standing pose for ~4 frames (rig reset snaps weights to standing) -> FIXED `KidRig::bind_snap_`.
  Car route drove into thorns -> tight circle. Hang from behind hides the hands -> `hangside` camera.
- DONE round 2 (`real_batch_r2.log`, `real_checks.log`): swap fix seen; car circles on the street; head traces,
  jitter, `--anim-v1` (all six, `wip_p\v1check2`), autopilot per costume. FOUND: in the hang the wand lay across
  under the chin (its tip glow on the mouth) -> FIXED (fist along the lip, wand up off the roof; `hangtop_*` views).

## Real-play review (2026-11-16)

### Review switches (dragon_controller.cpp, inert without them)

| Variable | What it does |
|---|---|
| `TOT_RIG_COSTUME=<id>` | the game's first costume is `<id>` (the first `set_visual()` wears it): any review mode (`--ledge-test`, `--mantle-test`, `--drop-test`, `--give-car`, `--autopilot`, `TOT_JITTER_TEST`, `TOT_RIG_DEBUG` traces) then runs with any costume |
| `TOT_RIG_REVIEW=<scene>[,<s>]` | quits after `<s>` seconds (0: the game's own end). `cam`: only the camera; `car` (with `--give-car`): mounted idle, a slow tight circle, a hop (jump action), stop, dismount through the game's `summon_car` action, walk off; `swap`: 12 costume changes through all six (standing, walking, in the air, mid-spin; the same calls as `cycle_costume`), prints `RIGSWAP`; `misc`: walk, a 1 s pause of the whole tree (as photo mode pauses), walk on, fall out of the world (the controller's own respawn above the ground) |
| `TOT_RIG_CAM=<dist>,<height>,<azimuth deg from the kid's front, + = left>,<fov>,<follow 0/1>,<aim height>` | the review camera (environment lens without near blur); yaw from the kid's facing at start and after teleports in the first 5 s |

Scripts: `real_play.ps1` (one movie-writer run at 60 fps into `wip_p\real\<Name>.mp4` + log), `real_batch.ps1`
(scenes hang / hangside / hangtop / vault / tramp / car / misc per costume + swap, 400 px strips `<name>_strip.png`),
`real_checks.ps1` (head traces, jitter, v1 showcase, autopilot). All through `godot_bg.ps1` and the lock.

### Player states (from the code) and what was seen

KidRig layers: idle / walk / run, rise / apex / fall, glide, mounted, flap, hang, climb (+ vault), spin, roll, pound
(+ stall), hurt, breath, launch, land, celebrate (+ hop), skid, fidget 0-4, mug (camera act), rattle, lean, look, cast.
Events: bounce (trampoline), grab, climb done, teleport. Controller: step-up (stairs), knockback, invulnerability
blink, mounted, costume change (`set_visual`). The game has NO swimming, no death (a hit spills candy), no talk /
door / interact pose (doorbells have no player pose); the only respawn is the fall-out-of-world safety.

| State | Seen (wip_p\real\) | Result |
|---|---|---|
| hang + climb, all six | `hangtop_fin_*`, `hangtop_fin_all6.png`, `hangtop_fin_zoom4.png`, `hang_*_strip.png` | head over the lip, wand fist on the lip, legs dangling, climb ends standing. FIXED: wand lay across under the chin with the tip glow at the mouth |
| vault (fence, `--mantle-test=1`) | `vault_*_strip.png` | hops over cleanly; seen from behind only |
| trampoline (passive bounces, seed 69) | `tramp_*_strip.png`, `tramp_all5_tight.png` | clean; super bounce (jump held) not exercised |
| mounted / drive / hop / dismount | `car_r2_*_strip.png`, `car_r2_all6.png`, `car_r2_seated6.png` | seated reads for all six; tails hang over the side; nothing seen through the seat at this distance |
| pause (photo mode) and resume | `misc_pause_zoom.png` | frozen, resumes with no pop |
| fall-out respawn + landing | `misc_fallout_all6.png` | falls, lands with a crouch; springs not flung (Dangle 4 m guard) |
| costume change in play | `swap_r2_all_strip.png`, `swap_r2_zoom.png`, `swap_r2_all.log` | all parts present in both cycles; FIXED: a change in the air / mid-spin showed the standing pose for ~4 frames |

### Measurements

- Head trace, candy 4/s (`wip_p\head\<name>.txt`), standing / walking / running / after-candy snaps: witch 95 / 8 / 11
  / 114, pumpkin 94 / 8 / 11 / 113, dragon now 94 / 8 / 11 / 113 (accepted candy_fix5: 94 / 8 / 11 / 113). Skeleton
  120 / 8 / 11 / 139: the extra 43 are three continuous 0.2-0.4 s bursts, all in the skeleton's designed rattle
  (pound / jump landing, hurt) and its shimmy; walking and running equal the dragon. Not changed.
- Judder (`wip_p\jitter\kid_ghost_*`, `kid_witch_*`, `dragon_kid_*`): model and head jitter median 0.00 px, p95 <= 0.04,
  uncapped (~175 fps) and at 60 fps, same as the dragon. Measures the visual root and head, not the soft parts.
- `--anim-v1` with all six v2 models (`wip_p\v1check2`): 6 of 6 recorded, no errors, sheets aligned (old poses, parts
  static, as expected for the fallback).
- Autopilot 45 s per costume (`wip_p\real\auto_<id>.log`): fps 161-203 at the 10 s marks for every costume (dragon
  162 / 185 / 174 / 173; P1's 12:44 run 164 / 189 / 199 / 152). No script errors.

### Code changes

- `kid_rig.h/.cpp` `bind_snap_`: the first update after `bind()` snaps the state weights to the state the kid is in;
  a teleport in the same frame clears it (so the showcase, which teleports and changes costume in one frame, is
  unchanged).
- `kid_rig.cpp` hang hand: the fist lies along the lip (`lay` = side 0.75, in 0.45, down 0.35) and is turned so the
  wand points up off the roof.
- `dragon_controller.cpp/.h`: the review switches above (`review_step`, `review_camera`, `review_asset`).

### Not verified / weak

- A white sparkle sits at the chin in some hang frames: most likely the candy pickup effect (roof candy is in reach
  while hanging); not traced.
- Vault from behind only; trampoline super bounce, stairs step-up, the skid and uneven-ground feet not looked at.
- Photo mode emulated by pausing the tree with a fixed camera (not through `photo_mode.gd`); the costume change by the
  same calls as `cycle_costume` (not through the input action and found-flags).
- Soft parts (cape, sheet, tail) were not measured for on-screen judder at uncapped fps (the movie writer records at
  a fixed rate); judged only in 60 fps clips.
- Mounted pose seen at game distance only; a tail or cape inside the car body would be hidden by the body.

## Progress (polish) - keep this current

Lead's polish round (2026-11-15, after reviewing all six v2 costumes). Dragon, cat, ghost, skeleton approved as is.
Order: 1 witch hair (volume, chestnut, no bare temple), 2 witch hat felt + violet + collar, 3 witch motion (pound
hat lift <= 2 cm, run cape sideways swing), 4 pumpkin shine, 5 ghost join line (only if cheap).
STATUS: FINISHED 23:40 (started 2026-11-15 ~20:30). Details: "Polish round" sections in Chunk P8 and Chunk P10.
- DONE (Blender): kid_witch exported 29,962 tris, gates / verify_rig2 PASS, catalog + publish (hair, hat colours,
  collar; see Chunk P8 "Polish round"). New row-11 cell `hat_violet_deep` (`set_palette_witch.py`): row 11 is FULL.
- DONE (C++, builds): `KidTuning::hat_gloss` (witch -2), hat lift 0.015 + `HAT_LIFT_MAX` 0.02, cape trail streams
  back with speed, `KidTuning::shine` signed (pumpkin -0.35).
- DONE rounds 1-2 (`polish_r1.log`, `polish_r2.log`): r1 hair read light caramel, curls at the jaw read as buns,
  collar still a ring on a purple neck, brim underside pale, pumpkin back still streaked at 0.06 -> fixed; r2 looked at
  (face, front, back, turntable, run, spin, roll, pound strips; pumpkin face / back).
- 5 ghost join: tried a baked-AO match at the join (measured a 0.89 / 0.95 step at the front); the engine line did not
  change, so it is not the bake -> REVERTED (costume_ghost.py as before, re-exported, AO values measured equal).
- DONE: final run of all six from one build (`wip_p\polish_final2.log`, 21:29-21:41, 6 of 6, frames aligned).
  Previous stills of dragon / cat / skeleton / ghost saved in `wip_p\polish_prev` for the unchanged check.
- Round 3 (second worker, from 23:10): the final2 face beat still showed glossy toffee-stick bangs and drip-like
  locks (default hair gloss + sky sheen), and a dark violet neck column between the chin and the collar roll. Fixing:
  hair cap + fringe moved to a static `hair_cap` node under `head` and all witch hair made matte in the rig; wider,
  flatter, overlapping bangs; collar raised into a flared cape collar that meets the chin.
  - DONE: costume_witch.py (`cape_collar()`, `hair_cap` node, bangs thin 0.26 / taper 0.5), exported 29,994 tris,
    gates / verify_rig2 PASS, catalog_update (`hair_cap` added to the witch's parts) + publish, asset_preview looked at.
    C++: `KidTuning::hair_gloss` (witch -1.4) on soft_hair* / hair_cap*.
  - DONE r3 (`polish_r3.log`): matte at gloss -1.4 read GINGER (the sky sheen had been cooling it) -> cells one step
    darker (choco_dark main, sunflower_center streak, hair_dark under) and hair_gloss -1.0; collar inner edge raised
    8 mm in front (a 1.5 cm dark neck band still showed under the chin).
  - DONE r4 (`polish_r4.log`): face / front / back, turntable, spin, hurt, pound (zoomed `strips\kid_witch_pound_*_r4`),
    roll / jump (`*_r4`) looked at: chestnut matte hair, full fringe, head sits on the collar.
  - DONE final run of all six from one build (`wip_p\polish_final3.log`, 23:25-23:37): 6 of 6 recorded, every beat
    sheet "frames aligned", strips cut; `compare_sheets.py` for witch / pumpkin / ghost rebuilt. Witch beats, face,
    front, back looked at. Unchanged check: `wip_p\polish_unchanged_check.png` (dragon / cat / skeleton / ghost face
    and back, `polish_prev` vs final3): identical apart from fireflies / particles (mean pixel diff 1.4-3.0 over the
    whole frame, all of it background). `game\override.cfg` absent.
STATUS: polish round FINISHED (2026-11-15 23:40). Items 1-4 done; item 5 (ghost join line) tried and reverted, not
fixed. Nothing in flight.

## Progress (chunk P10: kid_pumpkin v2) - keep this current

STATUS: FINISHED (2026-11-15 20:45); details and hand-off in "Chunk P10" below. Nothing in flight.
- DONE final run (`pumpkin_final.log`, one build): kid_pumpkin, dragon_kid, kid_cat recorded (3 of 3, "frames
  aligned"), fid4 re-recorded; dragon_kid / kid_cat beat sheets look as in P8/P9. `game\override.cfg` absent.
  Review images: `wip_p\kid_pumpkin_beats_before_after.png`, `wip_p\kid_pumpkin_before_after_mock.png`,
  `wip_p\strips\kid_pumpkin_*.png`, `wip_p\fid4\strips\kid_pumpkin_fid_final.png`.
- DONE (Blender): `batch05/costume_pumpkin.py` (in `build_costumes.py` BUILDERS + ARM_OUT, `catalog_update.py`), exported
  29,282 tris, gates PASS, verify_rig2 PASS, catalog_update + publish_catalog. No new palette cells.
- DONE (C++, builds): `KidTuning::shine`, pumpkin row (arm_out 0.47, elbow_rest 0.22, shine 0.45), `Bob::surface`
  leaf clamp, landing kicks, pumpkin fidget 4 (`pumpkin_wobble`, 2.5 s) + face, roll / land / pound-stall leg splay,
  `matte_*` nodes get gloss -2.
- DONE round 1 (`pumpkin_r1.log`): beats, before/after (`compare_sheets.py`), fid zoom, roll / run / jump strips looked
  at. Fixes after it: suit material "pumpkin" layer -> flat (it read as translucent wax), carved pockets split into
  `matte_carve` (they caught the sky sheen, read grey), cap front rim raised + longer bangs, back locks removed (read as
  dreadlocks; cap lower at the back), fidget amplitudes up (head roll 0.38, hop 7.5 cm, stem bias 0.5).
- DONE round 2 (`pumpkin_r2.log`): carved face reads dark and bold, orange clean; fidget seen
  (`fid4\strips\kid_pumpkin_fid_head.png`): stem and tendrils whip with the wobble, but the leaves stood up off the cap
  like wings in the hop -> leaf preset limit 0.7 -> 0.42.
STATUS: chunk P10 FINISHED. All six costumes are now v2. Row 11 has one free palette cell.

## Chunk P10: kid_pumpkin v2 (2026-11-15)

### Polish round (2026-11-15) - supersedes `shine` 0.45 below

- `pumpkin.shine` -0.35 (was 0.45: glazed ceramic, hard white streaks down every lobe; 0.06 in round 1 still streaked
  the lobes in the back view). `KidTuning::shine` is now signed: > 0 adds the tight catchlight, < 0 scales the
  shader's soft highlight down (gloss -0.35: 65 %, sky sheen kept). It still goes on the body and every soft_* /
  bob_* part (stem, leaves, tendrils); `matte_*` stays -2. Model unchanged.
- Seen: face beat (smooth soft orange shell, gentle highlight, carved face dark) and back still (soft lobes, faint
  highlight). The body still glows warm-yellow in its middle under the face beat's key light (tone mapping of a
  saturated orange; not a sheen).

### What exists

- `costume_pumpkin.py` (29,282 tris, 35 nodes), in `build_costumes.py` (`BUILDERS`, `ARM_OUT` from the module) and
  `catalog_update.py`. Common v2 rig (face v2 without the fang, skin grip hands `fat=1.08`) plus:
  - body: a squat ten-lobed pumpkin (`body_warp`: RX 0.214, RY 0.232, centre z 0.43, top / bottom half-heights 0.238 /
    0.19, crease depth `RIB` 0.105, dimpled top round the neck and bottom where the legs come out), flat material (the
    rig's `shine` gives the gloss), crease shading baked into AO (`crease_ao`, AO_POST on the shell's vertices only).
    Carved face (v1's exact boolean, bigger shapes); the pockets are split off into `matte_carve` (child of body,
    origin at the pelvis) so the rig can make them fully matte. Collar of six veined leaves, a felt neck, two curly
    vines on the back (static).
  - arms: the kid's own bare arms (`bare_arm`: skin limbs with ball ends at shoulder, elbow and wrist; the hand's heel
    ball covers the wrist) and a lobed orange puff sleeve round each shoulder with a darker rolled hem (part of `arm_*`,
    centred 2.6 cm down the upper arm so it turns with the arm about the shoulder pivot).
  - legs: green / charcoal striped stockings (the witch's `stocking`), kit high-tops in pumpkin colours (white toe and
    sole, darker collar) with a green strap and tab (v1's).
  - head: face v2, a hair-dark scalp, small ears, side locks (a face-framing lock in front of each ear and two behind
    it, flipping out at the jaw; roots just inside the cap rim), swept bangs `fringe2` (z_top 0.972, length 0.074) and
    the pumpkin-top cap (`pumpkin_cap`: 10 lobes x 8, scalloped rim high over the forehead (`cap_rim`), above the ears
    at the sides, down to the nape at the back; NO hair on top of it), a flared stem foot. Cap crease shading:
    `AO_SHAPE["head"] = cap_crease_ao`.
  - `soft_stem` (under head, origin at its base inside the flare): thick ribbed stem hooking over to the kid's left.
  - `bob_leaf_a` / `bob_leaf_b` (under head, origin at the stalk by the stem): two big leaves lying on the cap.
  - `soft_vine_a` / `soft_vine_b` (under head, origin at the stem foot): corkscrew tendrils (`tendril`: a helix round
    an axis bending up, every point farther from the pin than the last).
- The build prints `PUMPKIN arm clearance` (analytic test against the shell, below the puff, with ARM_OUT / ELBOW_REST
  applied as the rig does): +0.2 .. +1.3 cm in the rest, walk, run and cast arm poses, and `PUMPKIN hair vertices
  outside the cap` (a few rim vertices only).
- Colours: no new palette cells (row 11 still has one free). `pumpkin` / `pumpkin_dark` suit, `lilypad` /
  `banner_green` leaves, `apple_green` / `gourd_green` stem and tendrils, `ink` / `choco_dark` carve.
- `compare_sheets.py <id>` (new): `wip_p\<id>_beats_before_after.png` and `<id>_before_after_mock.png`.

### Tuning (C++)

| What | Value | Where / why |
|---|---|---|
| pumpkin row | arm_out 0.47, elbow_rest 0.22 (= `ARM_OUT` / `ELBOW_REST` in the Python), bounce 1.2, bucket_fwd 0.35, `shine` 0.45 | the bare arms clear the round body in every pose the build checks |
| `KidTuning::shine` (new) | `gloss` on the body and on every `soft_*` / `bob_*` part | a glossy vinyl pumpkin, stem and leaves |
| `matte_*` nodes (new, generic) | `gloss` -2 | the carved face stays dark (round 1: the pockets caught the sky sheen and read grey) |
| `leaf` bob preset | 2.4 Hz, zeta 0.25, limit 0.42 (was 0.7) | 0.7 stood the leaves up off the cap like wings in the hop |
| `Bob::surface` (new) | bob_leaf_*: bend along `away` (dir x the cap normal, centre assumed 0.23 m under the stalk) >= -0.04, the rate bounces off | a leaf lifts off the cap, never sinks into it |
| landing (pumpkin) | soft parts kicked fwd (0.7 + 1.6 s) + left/right 0.25 by jump parity; leaves kicked up (1.0 + 2.2 s) | the stem nods, tendrils spring, leaves flap |
| fidget 4 (pumpkin, 2.5 s) | dip 0-0.3 (O face), hop 0.3-0.64 (7.5 cm, arms flung out, open grin), squash, head wobble 0.7-1.95 (`pumpkin_wobble`: roll 0.38 at 2.6 Hz, body counter-roll 0.11, giggle squint), pleased tilt (roll 0.18) with a smile; the stem / tendrils get bias z -0.5 x wobble 90 ms behind the head | `kid_poses.cpp fidget()`, `kid_rig.cpp step_secondary / face_want` |
| roll (pumpkin) | legs (-1.2, knee 1.75, out 0.62), arms out round the ball | tucked knees vanished inside the round body |
| land / pound stall (pumpkin) | land: thigh -0.5 (was -0.72), out 0.36 (was 0.14); stall: out 0.55, thigh -1.2 | knees out instead of up under the body |

Other costumes: `shine` 0 and no `matte_*` / `bob_leaf_*` parts, the roll / land / stall changes are behind
`c.costume == KID_PUMPKIN`; dragon_kid and kid_cat re-recorded from the same build (see Progress).

### Rebuild

```powershell
$b = "C:\Program Files\Blender Foundation\Blender 5.1\blender.exe"
& $b --background --factory-startup --python art\team\characters_candy\batch05\build_costumes.py -- kid_pumpkin --face --poses --close
python art\tools\gates.py kid_pumpkin ; python art\team\characters_candy\batch05\verify_rig2.py kid_pumpkin
python art\team\characters_candy\batch05\catalog_update.py ; python art\tools\publish_catalog.py ; tools\asset_preview.ps1 -Id kid_pumpkin
# from PowerShell (-Command, so the id list arrives as an array):
art\team\characters_candy\batch05\p4_review.ps1 -Ids kid_pumpkin,dragon_kid,kid_cat -Log pumpkin_final.log -Fidget 4
python art\team\characters_candy\batch05\compare_sheets.py kid_pumpkin
```

### Technique notes worth writing back to art-pipeline (P10)

- **A texture layer can make a toy read as wax.** The "pumpkin" detail layer (streaky ribs, speckles, strength 0.65)
  under the showcase's strong lights read as translucent; a flat shell with real geometric ribs, crease shading baked
  into the AO and a tight gloss catchlight reads as the mock's glossy vinyl.
- **Recesses on a glossy surface need their own matte mesh.** Dark pocket floors on a shiny shell reflect the sky and
  turn grey; split the pockets into their own node and switch its sheen off (`matte_*`, gloss -2).
- **An exact boolean fails silently on slivers.** A grin outline with a grid point 0.7 mm from a tooth edge returned an
  EMPTY mesh (no error). Drop grid points near the shape's corners, and assert the result is not empty.
- **Arms on a round body: measure, do not eyeball.** An analytic inside-test against the shell's own warp, run at
  build for the rest / walk / run arm poses with the rig's arm_out applied, gives the clearance in centimetres; a BVH
  nearest-point test lied (it found the leaves and the carve walls).
- **A soft tendril must run away from its pin.** A corkscrew (helix round an axis that bends up) keeps every point
  farther from the pin than the last, so the shader bend works; a flat spiral curls back toward the pin.
- **A leaf on a surface needs a one-sided limit** (bend component along dir x normal >= 0): a spring overshoot pushed
  it into the cap otherwise; and a lower limit, or two leaves flapping together read as wings.

### Known weak points (P10)

- The kid's skin (bare arms, face) is near white under the showcase's strong lights (every kid; the arms show more of
  it here than on the other costumes).
- The roll's mid-tumble still reads as a pumpkin with the shoes up; the legs are no longer swallowed but it is not a
  neat ball.
- From straight behind the cap reads as one big orange dome (the mock has no back view).
- The tendrils are thin at showcase distance; their springing is seen only in zoomed strips (jump landing, fidget).
- In the fidget's hop the two cap leaves still lift together to about 25 degrees (limit 0.42) and read briefly as
  little leaf "ears"; a lower limit or a stiffer leaf preset would calm them.
- The final run was not cut into a separate landing / leaf-clamp trace: that leaves never sink into the cap was judged
  from strips only.
- Not checked: hang / climb / mounted, `--anim-v1` with this model, `head_showcase.ps1` / `jitter_test.ps1` (no head or
  root motion code outside the pumpkin's own fidget was changed), kid_ghost / kid_witch / kid_skeleton re-recordings
  (their code paths are unchanged apart from the leaf preset, which only the pumpkin uses).

## Progress (chunk P9: kid_skeleton v2) - keep this current

STATUS: FINISHED (2026-11-15 20:10); details and hand-off in "Chunk P9" below. Nothing in flight.
- DONE final run (`skel_final.log`, one build): kid_skeleton, dragon_kid, kid_cat recorded (3 of 3, "frames aligned"),
  fid4 re-recorded; dragon_kid / kid_cat beat sheets look as in P8. `game\override.cfg` absent.
- DONE (Blender): `batch05/costume_skeleton.py` (in `build_costumes.py` and `catalog_update.py`), exported 29,784 tris,
  gates PASS, verify_rig2 PASS, catalog_update + publish_catalog. New row-11 cells `skel_bone` #cbc1ab / `skel_shade`
  #9a8e78 (`set_palette_skeleton.py`, make_palette run under the lock): `bone` clipped white in the engine.
- DONE (C++, builds): `Soft::front` (cord), `KidTuning::matte`, rattle v2 (`RATTLE_LENGTH` 0.6), skeleton fidget
  (`skeleton_shimmy`, 2.2 s) + face, cord landing kick / rattle jiggle / shimmy swing / spin fling.
- DONE rounds 1-3 (`skel_r1..3.log`): r1 skull + bones clipped white, black read as vinyl -> darker cells, matte;
  r2 shimmy too small -> amplitudes up; r3 reads (`fid4\strips\kid_skeleton_fid_idle3.png`). Model r2-r3: 6 bigger
  teeth, hood shell under the skull culled, smoother knobs, thicker sutures, skull ends higher at the back.
STATUS: chunk P9 FINISHED. Next worker: kid_pumpkin (row 11 has ONE free palette cell left).

## Chunk P9: kid_skeleton v2 (2026-11-15)

### What exists

- `costume_skeleton.py` (29,868 tris, 31 nodes), in `build_costumes.py` (`BUILDERS`) and `catalog_update.py`. Common v2
  rig (face v2 without the fang, `fringe2` under the hood, glove fists `grip_hand(fat=1.15)` in charcoal plush) plus:
  - head: `hood2` (36 x 22, `dome(0.9, 1.9)`: a taller hood with an upright forehead) in charcoal plush; the face rim in
    `skel_bone`, its inner side `skel_shade` (the jaw); the skull: a padded cranium (44 x 14 grid, `skull_pt` / `puff`,
    v1's code with a fuller brow, cheekbones and occiput, a stronger forehead push) tucked into the hood round `EDGE`
    (down to the teeth at the front, ~90 deg at the sides, 99 deg at the back so the lower back of the hood is black).
    Two BIG oval sockets (`SOCK_C` (0.092, 1.052), `SOCK_S` (1.4, 1.4), projected along `SOCK_RAY` (0, 1, -0.15)) and an
    upside-down heart nose, each an ink disc lifted 3 mm ALONG THE SURFACE NORMAL with a `conf_ring` bevel in
    `skel_shade` (raised lip, inner edge flush) so they read as hollows; six puffy rounded-square teeth (`squircle` blob
    warp, sides / lower edge shaded) on the rim with an ink gum line; zigzag sutures (`zigzag`, `stone_dark`): sagittal,
    lambdoid V, coronal. The hood shell under the skull is deleted (`cull_under_skull`, 310 of 728 faces).
  - body: `torso2` charcoal plush; print = `pband` bands / `dblob` knobs whose steep sides are painted `skel_shade`:
    manubrium, sternum, 4 rib pairs, 2 lumbar, butterfly pelvis (front); 7 vertebrae, scapulae, 3 back rib pairs, pelvis.
  - arms: `arm2` with a dark rib cuff, humerus (upper) and radius + ulna (forearm) via `arm_bones`; gloves: a bone on
    every phalanx of every finger (3 per finger, on its back, from `grip_hand`'s frame), two on the thumb, three
    metacarpals on the back of the hand (`finger_bones`).
  - legs: thigh2 + femur front and back (`leg_bones`); kit sneaker black upper, `gourd_white` toe / sole / laces, purple.
  - `soft_cord_l` / `soft_cord_r` (under body, origin INSIDE THE CHIN (top of each cord, about (+-0.058, -0.12, 0.67)
    Blender), the cord runs out under the rim and down the chest 18 mm off the torso, ending in a bone toggle).
- Colours: new row-11 cells `skel_bone` #bdb39d and `skel_shade` #8c826e (`set_palette_skeleton.py`). They were added
  this chunk as #cbc1ab / #9a8e78 and darkened once before anything else used them (`bone` #e3d5b8 and then #cbc1ab
  clipped to flat white on the skull in the showcase). Row 11 now 15 of 16.

### Tuning (C++)

| What | Value | Where / why |
|---|---|---|
| `cord` preset | 1.6 Hz, zeta 0.22, gravity 2, drag 0.2, limit 0.9, inertia 1 | `SOFT_PRESETS`; lively and light |
| `Soft::front` (new, `cord`) | bend.x <= 0.03 (never back into the chest); roll frozen at 0; spin frozen at (-0.35, 0, 0.75 side) (fly out); gloss -1 | `add_soft`, `step_secondary` |
| cord motion | landing kick fwd x (0.9 + 1.8 s) + left x 0.5 side; rattle jiggle bias (z 0.16 sin(41 t), x -0.1) x rattle decay; shimmy swing z 0.38 sin(2.25 Hz) x `skeleton_shimmy` | `update()` KID_EV_LAND, `step_secondary` |
| `KidTuning::matte` (new) | skeleton -2: gloss -2 on body / arms / forearms / hands / thighs, -1 on the head (face keeps its sheen) | fleece, not vinyl (the round-1 recording read as black vinyl) |
| rattle v2 | `kid_pose::RATTLE_LENGTH` 0.6 s (was 0.45), 40 ms onset, joints at 67 / 79 / 91 rad/s, arms 0.26 / 0.2, forearms 0.38, hands 0.45, knees knock 0.07, head 0.09-0.12 + 6 mm bob | `kid_poses.cpp rattle()`; landing (s > 0.2), pound, hurt, fidget start |
| fidget 4 (skeleton) | 2.2 s: hunch 0-0.3 (O face), shimmy 0.3-1.45 (`skeleton_shimmy`: body yaw 0.27 / roll 0.12 at 4.5 Hz, head counter-bobble 0.22, arms out 0.55 +- 0.28 flapping, forearms / hands flopping, knees knocking 0.16, teeth chattering = GRIT / OPEN swapped at 8 Hz), ta-da 1.5-2.2 (wand arm up and out 1.05, head cocked, grin) | `kid_poses.cpp fidget()`, `face_want()` |

Other costumes: the new fields default to off (`front` only for `soft_cord_*`, `matte` 0); dragon_kid and kid_cat
re-recorded from the same build and look as before.

### Rebuild

```powershell
$b = "C:\Program Files\Blender Foundation\Blender 5.1\blender.exe"
& $b --background --factory-startup --python art\team\characters_candy\batch05\build_costumes.py -- kid_skeleton --face --poses --close
python art\tools\gates.py kid_skeleton ; python art\team\characters_candy\batch05\verify_rig2.py kid_skeleton
python art\tools\publish_catalog.py ; tools\asset_preview.ps1 -Id kid_skeleton
# from PowerShell (-Command, so the id list arrives as an array):
art\team\characters_candy\batch05\p4_review.ps1 -Ids kid_skeleton,dragon_kid,kid_cat -Log skel_final.log -Fidget 4
```
Review images: `wip_p\kid_skeleton_beats_before_after.png`, `wip_p\kid_skeleton_before_after_mock.png`,
`wip_p\strips\kid_skeleton_*.png` (every beat), `wip_p\fid4\strips\kid_skeleton_fid_final.png` (the shimmy, idle beat).

### Technique notes worth writing back to art-pipeline (P9)

- **Decals on a curved, coarse shell: lift along the surface normal, not the projection ray.** Sockets projected along a
  near-horizontal ray onto a brow that slopes back got their "lift" almost tangentially and z-fought in streaks.
- **A dark appliquÃ© raised off a light surface reads as a black lump; flush plus a shaded bevel ring reads as a hole.**
- **A face on a skull hood needs an upright forehead** (push the front of the cranium forward where the sockets go and
  give the hood more height above the face); on a sloping brow the sockets show in the side silhouette as ears.
- **Cut the shell a padded overlay hides** (here half the hood under the skull) to pay for smoother small parts: low-
  segment knobs read as hexagons in a face close-up.
- **Cream under strong light:** the bone print needed a cell around #bdb39d (luminance ~0.7) to keep form in the
  showcase; #e3d5b8 and #cbc1ab clipped to flat white on the big skull dome.
- **A shimmy at 4.5 Hz needs big amplitudes to read at game distance** (body yaw ~0.27 rad, arms +-0.28); round 1's
  0.15 / 0.14 was invisible. Counter-bobble the head against the hips for a loose-necked read.

### Known weak points (P9)

- From a pure side view the sockets still show at the front-top corner of the silhouette (small ear-like bumps); from
  the front, the face beat and three-quarter views they read as a skull.
- The back of the hood is still a big pale dome with sutures (the "egg" from behind in the turntable); darker and
  less tall than BEFORE, but the mock's hood is more black at the back than ours.
- The cords are thin at showcase distance; their motion was seen only in zoomed strips (fidget shimmy, landing). Their
  spin fling and the front clamp were not checked frame by frame.
- `asset_preview` shows the old glossy black (the preview does not run the rig, so `matte` is not applied there).
- The rattle on landing / hurt is quick (0.6 s) and only readable in step-1 strips, not in a still.
- Not checked: hang / climb / mounted, `--anim-v1` with this model, `head_showcase.ps1` / `jitter_test.ps1` (no head or
  root motion code outside the skeleton's own fidget / rattle was changed), kid_ghost / kid_witch re-recordings (their
  code paths are unchanged: `front` and `matte` are off for them).

## Progress (chunk P8: kid_witch v2) - keep this current

STATUS: FINISHED (2026-11-15 19:25); details and hand-off in "Chunk P8" below.
- DONE (Blender): `batch05/costume_witch.py` (in `build_costumes.py`), exported 29,968 tris, gates PASS,
  verify_rig2 PASS, `catalog_update.py` (kid_witch added) + publish_catalog. No new palette cells.
- DONE (C++, builds): presets hair / skirt / cape `down` + rigid share, tip `rigid` (no down), `Soft::trail`
  (cape: never forward, streams back in roll / spin), `Soft::wrap`, `KidTuning::spin_flare`, `Bob::hat`
  (fidget askew via `kid_pose::witch_hat_askew`, landing nod), witch fidget pose + face, witch row
  (hem 12 mm x 6, arm_out 0.22, elbow_rest 0.15). asset_preview looked at (reads well both rows).
- DONE round 1 (`witch_r1.log`): beats, face, every strip, `fid4\strips\kid_witch_fid_zoom.png` looked at.
  Fidget: the raised hand came to the face and the wand crossed the eye -> arm up and OUT, straighter.
  Cape -> `hat_violet` outside / `spider_plum` lining (it read as the dress continuing into a coat).
- DONE round 2 (`witch_r2.log`): the hand rose late and stopped below the brim -> retimed (hat straight
  at 1.2-1.5 s, hand up by 0.7 s), arm out 2.05.
- DONE final run (19:22, one build): `p4_review.ps1 -Ids kid_witch,dragon_kid,kid_cat -Log witch_final.log
  -Fidget 4`: 3 of 3 recorded, sheets "frames aligned"; witch beats, `fid4\strips\kid_witch_fid_zoom3.png`
  (hand now at the left brim, wand up and away), `wip_p\kid_witch_before_after_mock.png`,
  `kid_witch_beats_before_after.png` looked at; dragon_kid and kid_cat beat sheets look as in P6/P7.
  `game\override.cfg` absent.
STATUS: chunk P8 FINISHED (2026-11-15 19:25). Nothing in flight. Next worker: kid_skeleton / kid_pumpkin.

## Chunk P8: kid_witch v2 (2026-11-15)

### Polish round (2026-11-15, after the lead's review) - supersedes the hair, hat colour and collar notes below

- **Hair** (the v2 hair was a regression against BEFORE: thin flat ginger strands, bare temple). Rebuilt in
  `costume_witch.py`:
  - `hair_cap()` (static, in `head`): a 24 x 14 ellipsoid (`CAP_C` / `CAP_R`) warped so it lies 9 mm proud of the
    face's ellipsoid wherever it passes over it (`face_exit`), cut round the face at a hairline ellipse (x 0.188, z
    0.79 +0.15 / -0.2) and `snap_hairline()` puts the cut's stepped edge onto that ellipse (a square notch of skin at
    the temple otherwise). Front band painted `HAIR`, the rest `HAIR_DK`. The ears are gone (under the hair).
  - `fringe()`: five thick swept bangs from a parting on the kid's right (one to the right temple, four lapping to
    the left temple), `wavy_lock(thin 0.36, taper 0.35)`.
  - `wavy_lock()` / `fall_ctrl()`: chunky lens-section locks (thin 0.42, widest down the middle, rounded end), a soft
    S wave (4 to 4.5 degrees; 7 kinked into zigzags), the end curling OUT and UP at the shoulder and turned sideways
    (`twist`: straight-out curls seen from behind read as a row of beads). `soft_hair_back`: 6 outer locks (10 sides,
    lengths / curls / twists varied, every other one streaked) over 3 darker inner locks; `soft_hair_l/_r`: a
    face-framing lock (small flip at the jaw: a full curl there read as a bun) + two locks over / behind the ear.
    Names, pins and the `hair` preset unchanged.
  - `under_brim()`: every hair vertex outside the cone that would show above the brim's underside (hat space, brim
    wave `brim_dz` included) is pushed under it (the tipped hat let roots poke up beside the band).
  - Colours (round 3): `choco_dark` main, `sunflower_center` streak, `hair_dark` under-layer / cap (r1's `hair_brown`
    / `hair_light` read light caramel; r2's `sunflower_center` main read ginger once the hair went matte). No new
    cell for the hair.
  - Round 3: the hair cap and the fringe are their own static node `hair_cap` (under `head`, origin at the neck,
    in `catalog_update.py`'s witch parts), so the rig can make all the hair matte: `KidTuning::hair_gloss` -1.0 on
    `soft_hair*` / `hair_cap*` (no highlight, sky sheen kept; -1.4 warmed the hair to ginger). With the default
    gloss the bangs read as glossy toffee sticks and the locks as shiny chocolate. Bangs flatter and wider (thin
    0.26, end 0.3, width x1.16, taper 0.5): they lie over each other as one swept fringe.
- **Hat**: `hat_violet` (cone, brim top, tip; `witch_purple` read pink-lilac), brim underside `hat_violet_deep`
  #3a1c6e (NEW row-11 cell, `set_palette_witch.py`; `banner_purple` read pale pink, `spider_plum` grey).
  `KidTuning::hat_gloss` -2 on `bob_hat` / `soft_tip` (fully matte felt; -1.6 left a pale sky sheen on the underside
  at grazing angles). Shape, band, star, sprung tip unchanged.
- **Collar** (round 3, `cape_collar()`): a flared felt cape collar, 28 x 8 section loop: inner edge hugging the
  torso's dome just behind the chin (z 0.633 front, 0.667 back: `zt = 0.65 + 0.017 sin a`), sloping out and down
  ~6 cm (7 in front) to a rounded lip; lip runs into the shoulder balls at the sides, the cape's top is tucked under
  it at the back; top `hat_violet`, underside `hat_violet_deep`. The clasp star is ray-placed on its front (the build
  prints `WITCH clasp on the collar at ...`). Round 2's flat roll at `COLLAR_Z` 0.598 sat at the base of a ~3 cm
  dark neck column (the torso dome) and still read as a turtleneck / neck brace. Lapels at z 0.55 as before.
- **Budget** (29,994 / 30,000): cape panel 16 x 10, overskirt 40 / ruffle 54 segments, brim 40, belt 32, no ears,
  3 inner hair locks; the new collar is 448 tris (the roll was 416).
- **Motion** (`kid_rig.cpp`): hat preset lift 0.015 (x float_up 1.3) and `HAT_LIFT_MAX` 0.02 clamp (was 0.045: 6 cm
  in the pound); cape (`Soft::trail`) off a roll / spin: bend.x += 0.22 x speed (streams back), sideways bend x (1 -
  0.6 sf) and clamped to +-(0.3 - 0.2 sf).
- **Rebuild** as in "Rebuild" below, then `python art\team\characters_candy\batch05\catalog_update.py` (hair_cap) and
  `p4_review.ps1 -Ids kid_witch,kid_pumpkin,kid_ghost,dragon_kid,kid_cat,kid_skeleton -Log polish_final3.log`.
- **Technique notes (art-pipeline)**: matte hair needs a darker cell than glossy hair: the sky sheen cools and darkens
  a saturated brown, and removing it all (gloss < -1) turns it ginger under a warm key; keep the sheen (-1), drop the
  highlight. A head sits on a costume when the collar's inner edge meets the chin from the face camera's height; a
  roll at the base of the neck leaves a neck column that reads as a brace.
- **Review aid**: `build_costumes.py --head` renders `<id>_head.png` (12 cameras round the head, high and low).
- Seen in the engine (`polish_r2`, `polish_final`): face / front / back stills, turntable (no skin between hair and
  brim from any angle), pound (`strips\kid_witch_pound_hat.png`: the hat stays on the hair), run
  (`strips\kid_witch_run_cape.png`: the cape trails as a dark wedge behind the bucket arm, no flap), spin, roll.
- Seen in round 4 / final3: face beat (chestnut matte hair, full fringe, head on the collar), front / back,
  turntable (no skin between hair and brim), spin, hurt, zoomed pound (`strips\kid_witch_pound_hat_r4.png`,
  `_pound_land_r4.png`: the hat stays on the hair, no scalp), roll / jump (`*_r4.png`).
- Weak points: the bucket's warm light still tints the locks nearest it orange in the face beat; the fringe's
  bang tips are soft clumps but still separate a little at the ends; in the roll the hair spreads on the floor
  mid-tumble (as before); the collar's inner rim shows as a thin dark line under the chin from the face camera; the
  run's cape was judged from strips only; glide not checked frame by frame for hair-through-cape (the hair hangs
  outside the cape at rest by ~10 cm); `asset_preview` shows the hair with its old sheen (the preview does not run
  the rig).

### What exists

- `costume_witch.py` (29,968 tris, 37 nodes), in `build_costumes.py` (`BUILDERS`, `ARM_OUT` (0.22, 0.15)) and
  `catalog_update.py`. Common v2 rig (face v2 without the fang, skin grip hands fat 1.12) plus:
  - `bob_hat` (under `head`, origin `SEAT` = mid-seat (0, .022, .975) Blender): 44-segment brim (closed loop
    lathe: rolled edge, top `witch_purple`, underside `banner_purple`), the lower cone, a wide orange band
    with `candy_orange_light` piping, a puffy `candy_lemon` star front-right. Everything is modelled in hat
    space and put on by `HAT_M` (tipped back 8 deg, down 7 deg on the kid's right).
  - `soft_tip` (under `bob_hat`, origin on the cone's ring 9 = `CONE[3]`, where it starts to flop): the rest
    of the cone, flopping to the kid's right with an upturned tip. Both cone parts are cut from ONE sweep
    (`split_rings`): same ring vertices and frames, no caps where they meet (two separate sweeps left a slit).
  - `soft_hair_back` (origin (0, .2, .9)): 5 outer locks (caramel streak down each) over 4 darker inner
    locks; `soft_hair_l` / `_r` (origin at the roots above the ear): two locks behind the ear, one
    face-framing lock in front. `flat_lock`: lens section (thin 0.32), wide almost to the end and a ROUNDED
    end (pointed ends read as claws from below), ends flipping out. Static: a `hair_brown` scalp under
    the hat, `fringe2` (n 6, cinnamon / caramel), small ears.
  - `soft_skirt` (under `body`, origin at the waist (0, 0, .465) on the axis): overskirt (8 soft folds, top
    under the belt hugging the torso's y-squash, orange band + rolled hem, darker inside) over an 18-wave
    gathered orange ruffle 2.5 cm below. One mesh.
  - `soft_cape` (under `body`, origin (0, 0, .625) on the axis at clasp height): a collar roll (low under
    the chin, higher at the back, hidden by the shoulder balls at the sides), the gold star clasp, and the
    cape (`panel`, `hat_violet` / `spider_plum` lining) hanging from the back of the shoulders (angles
    40..140 deg at the top, 16..164 at the hem), 0.32 m out at the hem so it clears the skirt's back.
    Never over the arms (decision 15's lesson): narrower than where the arms swing back to.
  - `bob_bow` (under `body`, origin at the knot): lilac satin bow, pleats, two tails. Static: orange lapels,
    dark plush belt with gold buckle and loop, purple sleeves with flared orange cuffs (`flared_cuff`),
    striped stockings (`stocking`: thigh2's shape with a ring at every 21 mm stripe edge), purple high-tops
    (lilac sole, orange laces, yellow star on the outer ankle).
- Colours: no new cells. `witch_purple` dress / hat, `hat_violet` cape (Lane M's row 13 cell, used, not
  changed), `banner_purple` brim underside, `candy_grape` bow, `cinnamon` / `caramel` / `hair_brown` hair
  (warm auburn: the mock's chestnut, the lead's "orange hair"), `spider_plum` belt / lining.

### Tuning (C++)

| What | Value | Where / why |
|---|---|---|
| `hair` preset | 1.3 Hz, zeta 0.4, drag 0.25, limit 0.42, inertia 0.85, `down`, rigid 0.22 | roots under the brim stay put, the locks swing and lag a turn; 0.7 rad let them reach the brim |
| `skirt` preset | 1.5 Hz, 0.4, limit 0.32, inertia 0.7, hem, `down`, rigid 0.3 | rigid under the belt; `Soft::wrap` 0.03 (tucks a little in a roll, bend -0.3) |
| `cape` preset | 1.05 Hz, 0.36, drag 0.35, limit 0.55, inertia 0.95, hem, `down`, rigid 0.22 | `Soft::trail`: bend.x >= -0.05 (never into the back), bend.z within +-0.3 (not round into the arms), roll freeze (+0.45, 0, 0), spin freeze (0.45, -0.22, 0), no knee push, no roll draw-in |
| `tip` preset | 1.25 Hz, 0.3, limit 0.85, inertia 1.0, rigid share 0.42 (no `down`) | new: a non-down preset `rigid` keeps that share of the length round the pin straight (`add_soft`), so the ring shared with the lower cone never opens |
| `hat` bob | 2.25 Hz, 0.45, limit 0.2 (was 0.3), lift 0.045 | the wide brim must not reach the hair / shoulders; lift may dip 6 mm under the seat (a press on landing); landing kicks it forward (nod, `Bob::hat`) |
| witch row | hem_wave 0.012 x 6 lobes, float_up 1.3, `spin_flare` 1 (new `KidTuning` field), arm_out 0.22, elbow_rest 0.15 | the flared cuffs rest clear of the skirt; skirt and cape fly out in the spin |
| fidget 4 (witch) | `fidget_length(4, KID_WITCH)` 2.3 s; `witch_hat_askew(t)`: slip 0.05-0.35, straight 1.2-1.5, overshoot wobble; hat rotvec (0.16, 0.06, -0.24) x askew x w_fidget; arm_l out 2.05 (up and OUT, nearly straight), face O -> GRIT -> OPEN | `kid_poses.cpp`, `kid_rig.cpp` (bob loop, face_want) |

Other costumes: the ghost's sheet keeps wrap 0.11 / its roll and knee push (only `trail` parts differ); the
new tuning field and preset fields default to off; dragon_kid and kid_cat re-recorded from the same build.

### Rebuild

```powershell
$b = "C:\Program Files\Blender Foundation\Blender 5.1\blender.exe"
& $b --background --factory-startup --python art\team\characters_candy\batch05\build_costumes.py -- kid_witch --face --poses --close
python art\tools\gates.py kid_witch ; python art\team\characters_candy\batch05\verify_rig2.py kid_witch
python art\tools\publish_catalog.py ; tools\asset_preview.ps1 -Id kid_witch
art\team\characters_candy\batch05\p4_review.ps1 -Ids kid_witch,dragon_kid,kid_cat -Log witch_final.log -Fidget 4
```

### Technique notes worth writing back to art-pipeline (P8)

- **Split one tube, never sweep two:** a hat cone that must be two parts (rigid base, sprung tip) is ONE
  sweep whose ring faces are divided by ring index; separate sweeps start with different parallel-transport
  frames and their shared ring does not line up (a slit). Drop the caps on the shared ring and keep a
  rigid zone (`soft_bend.w`) round the pin at least the ring's radius, so the seam never opens.
- **Hair as flat locks reads; tubes and points do not:** lens section (thin 0.3), width held to the end
  and a rounded end; alternate locks with a light streak on the outer face and a darker underside; an
  inner darker layer between them gives depth without more triangles.
- **Cloth behind the body needs a one-sided limit:** a cape on a spring swings forward through the back on
  every stop; clamp its forward bend and its sideways swing, and give it its own roll / spin poses (stream
  back) instead of the sheet's wrap.
- **A fidget that touches a prop:** check where the hand actually lands in a zoomed strip; a pitched-forward
  raised arm brings the fist (and the wand in it) across the face; up-and-out with a straight elbow reaches
  a wide brim. Time the prop's motion (the hat straightening) to the hand's arrival, not the other way round.

### Known weak points (P8)

- Under the showcase's strong lights the dress reads lavender and the hair a light ginger (the mock is a
  deeper violet and chestnut); the cells are right in the night and studio rows of `asset_preview`.
- The fidget's hat straightening is timed by hand to the arm (no IK to the brim); seen only in the
  `fid4` recording at showcase distance, the hand meets the brim's left edge but does not visibly grip it.
- At rest the bucket overlaps the front of the skirt's hem a little (the bucket is the shared one).
- In the run the cape's sideways swing reads as a flap from the three-quarter front camera.
- In a ground pound the hat floats about 6 cm off the head (lift x float_up), showing the scalp: an
  intended toon lift, possibly too much.
- Blender sheets show faint dark streaks on the upper sleeves by the shoulders (AO); not seen in engine.
- Not checked: `head_showcase.ps1` candy trace, `jitter_test.ps1`, hang / climb / mounted, `--anim-v1`
  with the v2 witch, kid_ghost re-recorded (its code path is unchanged: wrap 0.11, no trail, spin_flare 0).

## Progress (ghost arm holes) - keep this current

User direction (2026-11-15, decision 15): "make arm holes for the ghost kid's arms, not try to drape over
the kid's arms". The earlier "ghost single hem" attempt (hem lifted over the arms, `cloth_lift` tenting)
is DISCARDED.
STATUS: FINISHED (2026-11-15 18:45); details and hand-off in "Chunk P7" below. Nothing in flight.
- DONE (C++ / shader): removed `KidTuning::sheet_lift`, the `cloth_lift_l/_r` instance uniforms
  (toon.gdshader back to 12 of 16 slots), `TOT_RIG_LIFT`, `lift_test.ps1`. Sheet preset: `arms` 0 (raised
  arms no longer puff / lift it), rigid share 0.58 (rigid down to just below the holes). `elbow_rest`
  (generic) kept.
- FOUND: `costume_ghost.py` already rewritten for arm holes by the stopped worker (16:42, `--noexport`
  only): `Hole`, `hole_pieces` (rolled rim + lining), arm pivots moved into the holes (`shift_arms`
  POST_ATTACH hook), no sleeves (dark plum felt top + cuff, bare hands), one hem `hem_z`. 30,422 tris
  (over budget). Its docstring mentions a shader "press" (arm_hole / cloth_ball): NOT built, not planned.
- DONE (Blender): rim 24 x 6, lining 24, `N_LOW` 14 -> 29,760 tris. Arm pivots `PIVOT_OUT` 12 mm OUTSIDE
  the sheet on the hole axis (glTF (+-0.3118, 0.4641, -0.0106)); arms keep the kit's directions but
  `UPPER_ARM` 0.6 / `FOREARM` 0.85 of its lengths (the holes are 12 cm under the kit shoulders: the bucket
  was 7 cm into the ground); hands shifted onto the new wrists in `shift_arms`. `verify_rig2.py`:
  `OWN_ARMS` (kid_ghost: arm chain pivots / bucket place not held to v1; arms must mirror). Exported,
  gates PASS, verify PASS, published, DLL built, asset_preview looked at (one sheet, arms out of holes).
- DONE round 1 (`ghost_holes.log`): beats, run / jump / cast / celebrate / turntable / roll strips, zoomed
  roll / boo / celebrate looked at: one sheet with arms out of holes in every beat. Felt arms read as a
  crinkled leather jacket -> plush (plain top).
- DONE (C++): hem ground guard (decision 18): hanging hem sheets collect 24 hem points (`Soft::low_pts`,
  `Soft::reach`) and `keep_tail_above_ground(..., down=true)` lifts the bend when the hem would go under
  the floor (on the floor or rolling; floor = the controller's height). Trace `HEM raw= now=` in RIGDBG.
  Measured (`wip_p\hemtrace`): without it the hem sank 1.0 cm in the roll; with it >= +1.0 cm everywhere.
- DONE final run (one build, 18:30): `p4_review.ps1` for kid_ghost, dragon_kid, kid_cat with `-Fidget 4`
  (pass the ids from inside PowerShell: through `Start-Process -File` the comma list arrives as ONE string,
  showcase.ps1 copes but the sheets / strips do not; they were rebuilt by hand). Beat sheets, front / back /
  face stills, strips and `fid4\strips\kid_ghost_boo_big.png` / `_fid_idle.png` looked at. dragon_kid and
  kid_cat beat sheets look as in P6. `head_showcase.ps1 -Name ghost_head -Id kid_ghost -Candy 4`: snaps /
  kinks equal the accepted `candy_fix5` run (94 / 8 / 11 / 113 snaps). `game\override.cfg` absent.
- Lead's comparison: `art\review\showcase\wip_p\kid_ghost_arm_holes_compare.png` (BEFORE, P5 sleeves,
  the discarded hem lift, NOW, mock).

## Chunk P7: kid_ghost with ARM HOLES (2026-11-15)

### What exists

- `costume_ghost.py` (29,760 tris, 31 nodes): the P5 one-drape sheet (cowl / sheet crossing, `join_normals`,
  `pre_counter`, sheet-eyes, `ghost_cloth`) unchanged, plus:
  - `Hole` (centre on the drape at `HOLE_A` 0 / pi, `HOLE_Z` 0.462, radius `HOLE_R` 0.06) cut through both
    walls of `soft_sheet` by `shell(holes=...)`; `hole_pieces`: a rolled rim (tube `RIM_T` 7.5 mm, 24 x 6)
    and a 3 cm lining facing the arm, both in sheet colour, part of `soft_sheet`.
  - Arms: the kid's own plain dark plum top (`ghost_legging`, plush) with a rolled cuff, bare hands. No
    sleeves. Pivots `PIVOT_OUT` 12 mm outside the sheet on each hole's axis (glTF (+-0.3118, 0.4641,
    -0.0106)), mirrored; the arm keeps the kit's directions (poses mean the same as on every kid) with
    `UPPER_ARM` 0.6 and `FOREARM` 0.85 of the kit's lengths; `shift_arms` (POST_ATTACH) moves each arm into
    its hole and each hand (bucket, wand socket) onto the shorter arm's wrist. All nodes translation-only.
  - ONE plain hem `hem_z(a)` (scallops), the arms do not change it.
- `verify_rig2.py`: `OWN_ARMS = {"kid_ghost"}`: the arm chain pivots and the bucket's world place are not
  held to v1 (they are printed as notes); the arms must mirror. Every other check as before.

### Tuning (kid_rig.cpp)

| What | Value | Why |
|---|---|---|
| `sheet` preset | rigid share 0.58 (0.262 m of the 0.452 m depth), `arms` 0 | the holes' lower rims are 0.251 m under the pin: the holes never sway off the shoulders; raised arms no longer puff or lift the sheet |
| `ghost.arm_out` / `elbow_rest` | 0.42 / 0.3 (`ARM_OUT` / `ELBOW_REST` in the Python, keep equal) | the upper arm leaves the hole about 33 degrees below its axis, the hands and the bucket sit clear of the bell |
| hem ground guard | `Soft::low_pts` (24 hem points) + `Soft::reach`, `keep_tail_above_ground(..., down=true)`, on the floor or rolling | decision 18; trace `HEM raw= now=` (RIGDBG) |
| removed | `KidTuning::sheet_lift`, `cloth_lift_l/_r` (shader), `TOT_RIG_LIFT`, `lift_test.ps1` | the discarded "drape over the arms" attempt |

### Rebuild

```powershell
$b = "C:\Program Files\Blender Foundation\Blender 5.1\blender.exe"
& $b --background --factory-startup --python art\team\characters_candy\batch05\build_costumes.py -- kid_ghost --poses --close
python art\tools\gates.py kid_ghost ; python art\team\characters_candy\batch05\verify_rig2.py kid_ghost
python art\tools\publish_catalog.py ; tools\asset_preview.ps1 -Id kid_ghost
art\team\characters_candy\batch05\p4_review.ps1 -Ids kid_ghost,dragon_kid,kid_cat -Fidget 4   # from PowerShell
```

### Contracts for the next worker

- Hole height vs the bucket: the holes sit 12 cm under the kit's shoulders; anything that lengthens the
  arms or lowers the holes puts the bucket into the floor (gate: min height -0.023 at rest now).
- Keep the sheet's rigid depth below the holes' lower rims (`HOLE_Z - HOLE_R - 2 RIM_T` under the neck)
  if the drape or the holes change; the preset is a share of the mesh depth.
- `ARM_OUT` / `ELBOW_REST` (Python) = `ghost.arm_out` / `elbow_rest` (C++).

### Known weak points (P7)

- The roll: the frozen forward bend now ramps in over 0.19 m only (below the holes), so the wrapped sheet
  is a rounder bag with a soft crease at the bend start; the shoes show at the open hem mid-tumble (as
  before). Not tuned further.
- A faint shading line at the cowl / sheet join across the chest and back, and a small light notch at the
  kid's-left silhouette at the join in the face close-up: both also in P5's stills (not made by this chunk).
- Arms raised far past ~45 degrees above the hole's axis (the celebrate's high wand) can graze the rim's
  upper edge; at game distance it reads as the arm coming out of the hole.
- The bucket's pendulum is not kept out of the sheet's bell: on hard stops it can dip into the hem.
- Not checked: hang / climb / mounted, `--anim-v1` with this model, `jitter_test.ps1`.

### Technique notes worth writing back to art-pipeline (P7)

- **Arm holes in a hanging cloth:** put the shoulder pivot ON the hole's axis a little OUTSIDE the cloth;
  then a tilted upper arm leaves the cloth inside the hole, and the shoulder ball still fills the opening
  (a short lining covers the ring). Size the hole from the arm radius and the largest tilt off the hole's
  axis the poses use; keep the cloth rigid down past the holes and let only the skirt below sway.
- **Moving a joint costs reach:** when the holes sit lower than the rig's shoulders, shorten the limb
  segments (keep their directions so every authored pose keeps its meaning) instead of bending the rest
  pose; check the carried prop against the ground.
- **A hem ground guard is the tail guard with depth instead of distance:** sample the lowest hem point per
  angular sector at bind; measure with a per-frame trace before trusting it.

## Progress (chunk P6: kid_cat v2) - keep this current

STATUS: FINISHED (2026-11-15 15:45); details in "Chunk P6" below.
- DONE: `costume_cat.py` (v2 kit: soft_ear_l/r, bob_bow > bob_bell, one-mesh S tail, 1.4x paws with five
  pads, fur rim / cheek / chest tufts), in `build_costumes.py`; exported 29,756 tris; gates PASS,
  verify_rig2 PASS; `catalog_update.py` (kid_cat added) + publish_catalog; engine preview looked at (reads
  as soft charcoal plush, readable in the night row). No new palette cells (charcoal / stone_dark / stone).
- DONE (C++, builds): `kid_rig.h` `Soft::side`, `KidRig::ear_fold_`; `kid_rig.cpp` ear fold / perk /
  fidget flick, ear preset 2.2 Hz / 0.32 / flatten 0.75, cat tail kicked in fidget 4, tail_wave one S
  (0.95 TAU / len, amp 0.05 + 0.022 sf); `kid_poses.cpp` cat fidget = ear-and-tail flick.
- DONE round 1 recording (beats, strips, fid4, zoomed land / glide / fidget strips looked at): ears flick,
  fold in the fall, bounce after landing; tail undulates. Model fixes after it: rim / cheek tufts read as
  claws then thorns (three tries) -> removed, the roll itself is fuller at the cheeks; chest fur =
  smooth clumps; ear-edge and cuff tufts removed; tufts now 6 sides x 2 per span. Re-exported 28,440
  tris, gates / verify PASS, published. Glide: legs splayed wider (cat-only `c.spread` path).
- DONE round 2 (looked at: beats, face, glide / land / fidget zoom strips): rim clean, glide legs splay;
  the ear flick flopped 70 degrees and held -> crisper, smaller flick; star hidden under the fuller roll ->
  moved in, the starred side's whiskers lowered. Re-exported 28,440 tris, gates / verify PASS, published.
- DONE final run (15:41, one build): `p4_review.ps1 -Ids kid_cat,dragon_kid,kid_ghost -Log cat_final.log
  -Fidget 4`: 3 of 3 recorded, beat sheets / strips / fid4 written; dragon_kid and kid_ghost beat sheets
  look as before. `wip_p\kid_cat_before_after_mock.png`, `kid_cat_beats_before_after.png`.
  `game\override.cfg` existed at 15:42 but belonged to Lane M's showcase run started 15:41:54 (wip_m).
STATUS: chunk P6 FINISHED. Next worker: the next costume (witch / skeleton / pumpkin); see "Chunk P6".

## Chunk P6: kid_cat v2 (2026-11-15)

### What exists

- `costume_cat.py` (28,440 tris, 34 nodes): common v2 rig + `soft_ear_l` / `soft_ear_r` (under `head`, origin
  at the middle of each ear's base on the hood; thick plush shell, recessed pink inner ear, pale fluff
  flames at its base) + `bob_bow` (under `body`, origin at the knot: purple satin bow tie) > `bob_bell` (origin
  at its loop under the knot, so it can never leave the bow) + `tail` (ONE mesh, origin TAIL_BASE, 58 rings x
  14 sides: an S - low swoop back, rise, the top bending forward, the shaggy tip easing back out - every
  ring farther from the root than the last, so the shader bend works; fluffy tip flames and v1's little
  purple bow are part of the same mesh).
- Hood: `hood2` 36 x 22 in plush charcoal, its smooth rim swapped for a lumpy fur roll that is about 40%
  fuller at the cheeks (`fur_rim`). Face v2 (fang kept from v1) + `face_paint`: pink button nose decal over
  the nose bump, three whiskers a side (the starred side's sit 22 mm lower), gold star + two sparkles on the
  right cheek just inside the roll (`poly_decal`, a kit-style Surf decal with any outline).
- Body: torso2 charcoal plush, grey plush belly oval (no stitching: the mock has none), four smooth grey fur
  clumps along its top. Paws: `grip_hand(fat=1.4)` in charcoal plush, five pink pads each (`paw_pads`: one
  toe bean per finger where the finger faces a front-outward view, and a palm pad); fat lumpy cuffs.
  Shoes: kit sneaker, black upper, white toe / sole, purple collar, orange strap and purple cat-face badge
  (v1's, ported).
- Colours: no new palette cells. `charcoal` body (reads as soft dark plush in both lighting rows and at
  night; `cat_black` is kept for seams / the shoe upper), `stone_dark` fur tips, `stone` belly,
  `cheek_pink` ear / pads / nose, `wrapper_purple` / `witch_purple` bow, `foil_gold` bell / star.

### Tuning (C++)

| What | Value | Where / why |
|---|---|---|
| `ear` preset | 2.2 Hz, zeta 0.32, flatten 0.75 | `SOFT_PRESETS`; only the cat has `soft_ear_*` |
| ear fold | `ear_fold_` (new Spring, 2.4 Hz / zeta 0.3) towards max(falling, 0.6 x glide in the air) + 0.9 rolling; landing kicks it by -(2 + 6 s) | underdamped, so the ears spring forward past upright when a fall ends (the perk) |
| ear flick (fidget 4, cat) | left ear twice (t 0.30, 0.52), right ear once (1.0); 45 ms snap, bias (-0.42 back, -side x 0.22 out) x twitch, capped at 1 | `step_secondary`; first try (0.55 / 0.45, 60 ms) flopped the ear 70 degrees outward and held |
| `Soft::side` | +1 `*_l`, -1 `*_r` | set in `add_soft` from the node name |
| tail kick in fidget 4 | left x 1.6 + up 0.6 | `update()`; the cat only (the generic fidget kick skips ears now) |
| `tail_wave` (swish) | amp 0.05 + 0.022 sf, wavenumber 0.95 TAU / len | about one full S running down the tail (was 0.75 of one) |
| cat fidget pose (kind 4) | head cocks toward the flicking ear (yaw 0.14, roll 0.17), tail root whip 0.5, wand arm eased | `kid_poses.cpp` |
| glide (`c.spread`, cat only) | legs out 0.8, knees 0.42 +- 0.12 paddling | the old 0.5 hardly read |
| cat row | unchanged (tail 1.3 Hz / 0.4 / limit 0.7, vertical 0.6, bounce 1.15, arm_out 0) | the new thicker tail behaved in every beat |

### Technique notes worth writing back to art-pipeline (P6)

- **Fur on a toon plush costume is lumps, not strands.** Any thin protrusion from a silhouette (curled
  tufts, straight tufts, even smooth teardrop clumps) read as claws, thorns or a beard at game distance
  and in the face close-up. What read as fluffy: a lumpy roll whose radius varies (two cosines of
  different frequency) and swells where the fur should flare (the cheeks), plus fluff only where it sits
  INSIDE a silhouette (pale flames in the inner ear, grey clumps on the belly).
- **Black that stays readable at night:** charcoal (#36343a) on the plush layer, not a near-black; the
  plush texture's grain and the kid's rim light carry the form in the night row.
- **Decals near a fur roll:** check where the roll's inner edge lands on the face after making it fuller
  (the cheek star had to move 2 cm in).
- **An underdamped level is a free "perk":** drive the ear fold through a lightly damped spring toward the
  fall amount; when the fall ends it overshoots forward by itself, and a landing kick scales it.

### Known weak points (P6)

- The wand paw's four toe beans form a vertical row along the fist (the fingers wrap a vertical wand), so
  in the face beat they read as a column of pink buttons rather than a paw print.
- The black hood still shows a broad environment sheen in the bright face close-up (SPECULAR 0.3); it
  reads as soft rubber-plush there, plush elsewhere. Not changed (would need a per-part gloss).
- The perk on landing is small at showcase distance (seen in a zoomed strip, not obvious in the beat).
- Not checked: hang / climb / mounted / `--anim-v1` with the v2 cat, the head trace.

## Progress (chunk P5: kid_ghost ONE-sheet redo + eye rim removal) - keep this current

STATUS: chunk P5 is FINISHED (2026-11-15 15:05). kid_ghost v2 (one sheet) and the no-rim eye are
exported, recorded and looked at; dragon_kid and kid_cat re-recorded from the same build. Next worker:
start the next costume (cat / witch / skeleton / pumpkin) from "Chunk P5" below; nothing is in flight.

- DONE (kit): `eye2` has NO white: the eye's base disc is `choco_dark` (the iris's darker rim), iris
  IRX 0.0425, lid clip 0.975, parabolic dome, the lash line a flat ribbon; `kid_face.cpp` eye / iris
  `gloss` -2 (fully matte: new shader rule, see Chunk P5). Engine face close-ups of dragon_kid and
  kid_ghost: no rim, sliver or light outline (`wip_p\<id>_face.png`).
- DONE (Blender): `costume_ghost.py` rebuilt as ONE drape (decision 15): `drape_r()` is the one surface
  both parts sample; cowl and sheet cross at `ze(a)` (ZC 0.552) where both lie exactly on the drape
  (cowl turns in under the sheet below it, sheet turns in under the cowl above it); cowl origin at the
  NECK with a static pre-counter of the head tilt below the neck. `ghost_join_check.py` measures it.
  `build_costumes.py` renders `<id>_out.png` with the costume's `arm_out` (ARM_OUT dict). 29.4k tris.
- DONE (C++, builds): `kid_rig.cpp`: cowl preset -> `Soft::counter` (per-frame counter-rotation of the
  hood below the neck against the head's pose; COWL_COUNTER_R0 0.012 / LEN 0.048, same as the Python),
  ghost `arm_out` 0.32, sheet preset rigid share 0.28 + `arms` 1 (raised arms puff it), hem drawn in
  during a roll (`w_roll_`), the capelet arm tilt only for parts without a hem.
- DONE: dragon_kid + kid_ghost exported, gates / verify_rig2 PASS, catalog_update + publish_catalog,
  engine previews looked at (ghost: no join visible, studio row still near white).
- DONE rounds 1-3 of kid_ghost recordings: round 1 the sheet clipped white -> new cell `ghost_cloth`
  #aeaac4 (row 11, `set_palette_ghost.py`), flat material (the `fabric` layer is a plaid check: never
  for a bedsheet); taller hood (`dome(0.55, 1.4)`); roll wraps (hem cinch -0.11 + bend -0.5 forward);
  hem ripple 26 mm. The crossing showed as a hairline in the engine -> `join_normals()` gives both parts
  the drape's own normals round the crossing (custom normals, kit hook `KK.POST_ATTACH`): invisible in
  the face beat. Head turns (`TOT_RIG_FIDGET=2`, `wip_p\fid2`): the hood turns, the drape stays, no gap.
- DONE (kit): the lash line is a flat ribbon on the eye (a round tube showed a light line inside it in
  the engine); dragon_kid re-exported (27.9k), gates / verify PASS.
- DONE final rounds: `p4_review.ps1 -Ids kid_ghost,dragon_kid,kid_cat -Fidget 4`, then ghost + dragon
  again after the eye fixes (15:04). Beat sheets, strips, `fid4\strips\kid_ghost_boo_zoom.png`,
  `kid_ghost_before_after_mock.png` looked at. `game\override.cfg` absent.

## Progress (chunk P4: kit face polish + kid_ghost v2, superseded for the ghost by P5)

- DONE (kit face polish, Blender side): `eye2` iris is a big deep-brown disc clipped under the lash
  line (layers: iris_brown, cinnamon glow, amber crescent, choco_dark shade under the lid, smaller
  ink pupil), the lash line is a thick top stroke only (no outline under the eye, a faint skin_shade
  crease), the shut eye is a bold ^ arc with a flick and NO lid patch, the nose is a skin-coloured
  bump. `Surf.disc(clip=)` added. dragon_kid face sheets looked at (`--noexport`).
- DONE (C++): `kid_face.cpp` gloss eye 0.4, iris 0.08 (was 0.55 on both).
- DONE: dragon_kid re-exported with the new face (28,268 tris), gates / verify_rig2 PASS, published.
- DONE (C++/shader): sheet / cowl presets bend by depth below the pin (`down`, `rigid` preset fields,
  `soft_pin` axis (0,-1,0), `soft_bend.w` rigid start), hem ripple and puff pushed radially
  (`cloth_wave.z < 0`, normal tilted with the wave), the glide fills the sheet like a parachute.
  Palette row 11 + `ghost_sheet`, `ghost_shade`, `ghost_deep`, `ghost_legging` (`set_palette_ghost.py`).
- DONE: `costume_ghost.py` v2 (26.3k tris): soft_cowl = hood crown + face opening + sheet-eyes + cowl as
  ONE mesh (no neck seam), soft_sheet thick bell, sleeves, sneakers. Gates / verify_rig2 PASS, catalog +
  publish done, engine preview looked at.
- DONE: two recording rounds of kid_ghost. Round 1: the sheet blew out to white -> base cell is now
  ghost_shade (ghost_sheet kept for nothing / a spare highlight). Round 2: form reads; the parachute
  glide reads; the knee push (sheet rotated forward by raised knees) flipped the sheet up in the jump
  -> cut to 0.16 rad. Kit eye pass 2: iris clipped to the eye outline top and bottom (white only at the
  sides), no under-eye crease, smaller sclera skirt, choco glow + burnt_amber crescent; IRIS_Y 3 mm.
  Cowl: arms raised lift its side (Z tilt) and puff it open (radial cloth_wave). New "boo" fidget pose.
- IN FLIGHT: round 3 recording kid_ghost + dragon_kid + fidget 4 (`p4_review.ps1`).

## Chunk P5: kid_ghost as ONE sheet, eyes with no rim (2026-11-15)

### What exists

- `costume_ghost.py` (29,230 tris, 31 nodes): common v2 rig + `soft_cowl` (under `head`, origin at the
  NECK = the head's pivot: hood crown, face opening + rolled rim, two flat black sheet-eyes, and the
  drape down to just under the shoulders) + `soft_sheet` (under `body`, origin (0, .64, 0): the drape
  from inside the hood to the hem). Sleeves are `forearm_*` bells, bare hands, plum leggings, purple
  high-tops. Colours: `ghost_cloth` (new, #aeaac4) outside, `ghost_deep` inside, flat material.
- ONE surface: `drape_r(a, z)` (Hermite through the hood's widest line, 0.55, 0.43 and the hem; the
  hem is lifted over the bucket by `hitch`) is sampled by both parts, so slopes, folds and cells match.
  At the join `ze(a)` (ZC 0.552 +- 5 mm wave) both lie exactly ON the drape; above it the sheet turns
  in under the cowl (deeper higher up), below it the cowl turns in under the sheet (5.5 mm over 12 mm).
  `join_normals()` (via the new kit hook `KK.POST_ATTACH`) gives both the drape's own normals round
  the crossing (custom normals; glTF exports them; Godot uses them): no hairline, no step.
- The head moves; the drape must not: the cowl's part below the neck is counter-rotated against the
  head (`Soft::counter`, `COWL_COUNTER_R0` 0.012 / `_LEN` 0.048 in `kid_rig.cpp`; the same ramp is
  baked against the static head tilt by `pre_counter()` in the Python - change both together). The
  crown and face opening go with the head, the edge stays on the body sheet. Seen with
  `TOT_RIG_FIDGET=2` (look round, `wip_p\fid2\strips\kid_ghost_turnzoom.png`) and the face beat.
- `ghost_join_check.py`: rays at 8 angles x 11 heights print which part is outermost and its radius,
  plus the baked AO by height for both parts (use it after any change to the drape or the join).
- `build_costumes.py --poses` also writes `<id>_out.png` (rest pose with the costume's `arm_out`,
  `ARM_OUT` dict; keep it equal to the tuning row).

### Tuning (kid_rig.cpp)

| What | Value | Why |
|---|---|---|
| `ghost.arm_out` | 0.32 | the hands come out through the sides of the drape (mock: hands outside the sheet) |
| `ghost.hem_wave` | 0.026 | 12 and 19 mm hardly read at a walk |
| `sheet` preset | rigid 0.28, arms 1 | rigid top reaches below the join (z ~0.52); raised arms puff it |
| `cowl` preset | no spring, `counter` | see above; a spring on the hood would open the join |
| roll | sheet frozen at bend (-0.5, 0, 0) + `cloth_wave` -0.11 x `w_roll_` | the sheet wraps the tucked kid instead of standing open like a bag |
| arms puff | 0.55 x balloon when an arm is raised past 0.3 | the "boo" fidget (kind 4) pushes the sheet out |
| capelet side lift | only for soft parts without a hem | it used to tilt the ghost's cowl |

### Kit / shared changes

- `kid_kit.py`: `eye2` (no white; base disc `choco_dark`; IRX 0.0425; lid clip 0.975; parabolic dome;
  lash line = flat ribbon), `AO_IGNORE` (extra occluders a part skips), `POST_ATTACH` (hooks run by
  `finish2` after attaching, before AO / export). Both are module lists: a builder must set them
  (`costume_ghost.build()` does `[:] =` / `.clear()`).
- `toon.gdshader` (one line): `SPECULAR = 0.3 * clamp(2 + min(gloss, 0), 0, 1)`: `gloss` <= -2 is fully
  matte (no painted highlight AND no sky sheen). -1 (Lane M's matte) is unchanged.
- `kid_face.cpp`: eye and iris `gloss` -2. Any sheen on the dark eye parts turned the lash line, rim
  and pupil lavender in the bright face close-up and read as a light ring.
- Palette row 11: + `ghost_cloth` (13 of 16 used).

### Known weak points (P5)

- The sheet-eyes sit high on the hood: from the face beat's low close camera they read as dark beans
  on the top of the head; from the front stills they read as in the mock.
- The drape is a smooth bell with soft folds; it does not have the mock's deep diagonal folds from the
  arm or the fold over the bucket arm (the hem is only hitched up there).
- A raised arm (cast, boo) comes out through the drape as a sleeve tube; the drape does not tent
  over the arm.
- Under the face beat's strong lights the skin of every kid is close to white (not this chunk's).
- Head translation (6 mm in the camera act) is not counter-moved; it only shifts the crossing a few mm.
- Not checked: hang / climb / mounted with the new sheet, `--anim-v1` with the v2 ghost, the head
  trace (`head_showcase.ps1`; no head motion code was touched).

### Technique notes worth writing back to art-pipeline (P5)

- **Two parts, one cloth:** when a garment must be split at a moving joint, model ONE surface and
  sample it for both parts; let them CROSS on that surface (each turns in under the other on its own
  side) instead of overlapping with a lip; then set custom normals from the shared surface round the
  crossing. Measured: no step, no hairline, even in a close-up.
- **Counter-rotate the part of a head child that lies on the body** (rotation vector = log(B^-1 R) in
  the head's frame, ramped by depth below the neck in the shader), and bake the same ramp against any
  static head tilt in the model. A hood then turns with the head while its drape stays on the body.
- **A chord inside a thin shell pokes out:** an inside wall that skips rows of a curved 3 mm cloth
  shows through as dark blotches; follow every other row.
- **Dark glossy details wash out in bright close-ups** through the environment reflection, not just
  the highlight: give eyes a fully matte flag; get gloss from modelled catchlights.
- A "fabric" weave texture on a bedsheet reads as a plaid blanket; a plain cell a step darker than
  white (#aeaac4) keeps a white sheet from clipping under strong lights while still reading white.

## Progress (chunk P3: code for the v2 parts, tuning on dragon_kid) - keep this current

- WRITTEN, BUILDS (2026-11-15): `kid_face.cpp` (new: blink, gaze, brows, mouth pop, gloss on eyes),
  `kid_rig.h/.cpp` rewritten round it (stride clock, face wants, hand reach for the ledge lip, feet on
  uneven ground, soft limit on soft parts, hem ripple / balloon, hat lift, costume table with all six
  rows), `kid_poses.cpp` rewritten (solved stride, elbows / wrists / knees in every pose, cast
  follow-through, chin-over-the-lip hang, costume fidgets, camera act), `dragon_controller.cpp` (two
  ground rays under the feet in `animate_rig`, nothing else).
- TUNING ROUND 2 (second P3 worker, 2026-11-15 midday), looked at every beat's strip of the 08:37
  recording and changed: no blink of the kid's own in the middle of a move or a scripted face
  (`KidFaceWant::hold_open`), the first blink after a reset no longer at the same instant every time,
  the cast keeps the eyes open (blink when the wand comes down), an ordinary jump's landing only
  squints (the land strength scale was saturating on a plain jump), the celebrate / bucket-peek head
  turn no longer hides the face, the roll holds the bucket beside the knees, the idle sways more and
  glances at the camera sooner. Review aids: `TOT_RIG_DEBUG=1` (sole trace, `p3_slide.py`),
  `TOT_RIG_FIDGET=<kind>` (repeats one fidget). The stance foot does not slide (trace: a planted
  heel / toe stays within 1 mm).
- TUNING ROUND 3: the swinging foot was scraping the ground (trace: the toe within 8 mm of the floor
  for 3 to 4 frames of every stride at a walk and a run, and the leg thrown 40 degrees out sideways to
  manage even that). The swing now comes through earlier with the shoe level and the leg is drawn up
  into the body by up to 4 cm (`HIP_HIKE`, `Gait::clear`). Candy landing in the bucket no longer makes
  the kid stop looking at the camera. Hang / climb seen (`wip_p\hang\hang_zoom.png`): holds together.
  Fidgets seen: stretch, wand twirl, bucket peek, toe tap (small).
- DONE: engine test of `soft_*` / `bob_*` on a throwaway dragon (`p3_softtest.ps1`; it put the real
  model back, hashes equal; strips in `wip_p\soft\strips`): the parts are found, bend, lift on a
  fall and fling out in a spin.
- DONE (12:35 to 12:45): `p1_verify.ps1` with the final code: build OK, all six costumes recorded
  (6 of 6, sheets "frames aligned"), `--anim-v1` recording of the v2 dragon (old poses, one mouth,
  eyes open: `wip_p\v1check\dragon_kid_beats.png`), ledge grab and climb, 45 s of autopilot to
  `BOT_DONE` with no script errors. Final dragon strips are in `wip_p\strips`, the sole trace of the
  final code in `wip_p\trace`. Chunk P3 is finished; its section is below.
- NEXT (P4 / P5): port the five costumes with the kit; the names the code picks up are in "Costume
  table" below. Then per-costume spring tuning on the real cloth.

## Chunk P3: the code for the v2 parts (2026-11-15)

Where this section and the older P1 sections further down disagree, this one is current.

### Code map

| File | What is in it |
|---|---|
| `native/src/play/kid_rig.h` | `KidAnimInput` (what the controller tells the rig), `KidEvent`, `KidTuning` (per-costume row), `KidFaceWant` / `KidFace`, `KidRig`, `KidPoseCtx`, the `kid_pose::` declarations. |
| `native/src/play/kid_rig.cpp` | Spring presets (`SOFT_PRESETS`, `BOB_PRESETS`), the costume table `kid_tuning()`, `bind()` (name lookup, reparenting, soft / bob discovery, sole and hand-tip points), `step_gait()` (stride clock), `face_want()` (the face per state), `update()` (events, blend weights, fidget / glance / camera-act scheduling, the layer list), `apply()` (legacy knee / elbow folding, standing on the lowest sole, uneven ground, root transform, the hand on the ledge lip), `step_bucket()`, `step_secondary()` (soft bends, tail wave, hem ripple, bob parts, face), `cast_now()`. |
| `native/src/play/kid_poses.cpp` | The solved stride (`Gait`, `stance()`, `stride()`), then one function per state. Sign conventions at the top. |
| `native/src/play/kid_face.cpp` | Blink state machine, gaze (iris slide, darts), brows, mouth swap with a pop, eye widen / squint, `gloss` on the eyes. |
| `native/src/play/dragon_controller.cpp` | `animate_rig()` fills `KidAnimInput`; the only P3 addition is two ground rays under the feet (standing or creeping only). |
| `batch05/p3_review.ps1` | Build, record, sheets, every beat's strip, optional fidget recordings, hang strip, sole trace. Log `wip_p\review.log`. |
| `batch05/p3_strip.ps1` | A frame strip of any frame range with any crop. |
| `batch05/p3_hang.ps1` | Records the ledge test and cuts the hang / climb strip (`wip_p\hang`). |
| `batch05/p3_slide.py` | Reads the `TOT_RIG_DEBUG` sole trace from a showcase log: how far ground-contact points move per frame. |
| `batch05/p3_softtest.py` / `.ps1` | Throwaway dragon with `soft_*` / `bob_*` test parts, recorded, real model restored. |

Review aids (environment variables, read once at start; none changes a normal run):
`TOT_RIG_DEBUG=1` prints one `RIGDBG` line per frame (state, speed, stride clock, heel and toe of each
foot in the world); `TOT_RIG_FIDGET=<0..4>` starts that idle fidget half a second into standing still
and repeats it; `TOT_RIG_HEM=<metres>` gives any costume a hem ripple.

### What drives each v2 part

| Part | Driven by |
|---|---|
| `forearm_*` | Every pose's `arm_l / arm_r(pitch, out, elbow)`; breath, lean lag, cast, celebrate, fidgets. Hinge about local X. |
| `hand_*` | `hand_l / hand_r(pitch, roll)`: wand flicks in walk / run / cast / twirl / cheer; laid on the ledge lip while hanging. |
| `shin_*` | Every pose's `leg_l / leg_r(thigh, knee, out)`; the stride solves thigh and knee so the planted sole does not slide. |
| `socket_wand` | The controller parents the wand there (0.85 scale, identity). `cast_from()` -> `cast_now()` puts shoulder, elbow and wrist on the cast pose before the tip is read. |
| `eye_*` | Scaled along local Y: blink (to 0.1), squint, widen. Hidden while `alt_lash_*` shows. `gloss` 0.55. |
| `alt_lash_*` | Shown while the eye is more than 60% shut. |
| `iris_*` | Slides in the eye's XY (5.2 mm / 4.2 mm at the limit): the eyes' share of a look, quick darts while standing about. `gloss` 0.55. |
| `brow_*` | Raise / press (7.5 mm / 4.5 mm), tilt about head Z (0.34 rad), on a spring. |
| `mouth_smile`, `alt_mouth_open / _o / _grit` | Exactly one visible; a change pops the new one in (scale spring, mostly in height). |
| `soft_*` | Any MeshInstance3D so named, under any parent: a `Dangle` spring -> `soft_bend`; `extra_cull_margin` 0.5. Hems (`cape`, `skirt`, `sheet`) also get `cloth_wave`: a ripple (`KidTuning::hem_wave`, `hem_lobes`) and a puff-out on a fall. `ear` folds back in a fall. |
| `bob_*` | Any Node3D so named: rigid, swung about its origin. `hat` lifts off its seat in a fall (4.5 cm x `float_up`). |
| `tail`, `wing_*` | As P1 (FK + soft bend); the dragon's tail gets a downward kick on landing (`tail_thump`), the bend eases into its limit (`soft_limit`, tanh). |
| `bucket` | Pendulum from its grip, in world space; eases into its forward limit (`bucket_fwd`), flung out in a spin, rigid in a roll. |

### How to tune

- **Stride** (`kid_poses.cpp`, `Gait`): `duty` (share of a stride one foot is down), `reach` (ground a
  planted foot covers), `drive` (high knee), `fold` (heel kick), `hop`, the swing timing
  (`fold_at`, `level_at`, `level`, `lead_at`, `lead`, `high_at`) and `clear` (air under the swinging
  shoe). `walk()` and `run()` each fill one. Check with `TOT_RIG_DEBUG` + `p3_slide.py`: the walk and
  run rows should show a median slide of about 0.
- **A pose**: its function in `kid_poses.cpp`. Arms overhead go by `out`, not pitch. A pose that only
  bends the knees calls `stand()` and the rig puts the lowest sole on the floor.
- **The face for a state**: `KidRig::face_want()` (mouth, brows, wide, squint, gaze, `hold_open`).
  Forced blinks: `blink_req_ = true` (+ `blink_hold_req_`) where the event is handled in `update()`.
  Blink timing constants are at the top of `kid_face.cpp`.
- **Fidgets**: `kid_pose::fidget()` kinds 0..3 are shared, kind 4 is the costume's own (switch on
  `KidPoseCtx::costume`). Schedule in `update()`: first after 3 s standing, then every 7 to 10 s.
- **The act for a close camera** (`kid_pose::mug` + the `mugging` block in `face_want`): runs when a
  camera is within 2.2 m in front of the face. This is what plays in the showcase's face beat.
- **A costume**: its row in `kid_tuning()`. **A spring preset**: `SOFT_PRESETS` / `BOB_PRESETS`
  (hz, zeta, gravity, drag, limit, inertia, flatten, lift, hem).
- **Landing weight**: `KID_EV_LAND` in `update()`: strength 0 (a step) .. 0.55 (a jump) .. 1 (a roof).

### Costume table (`kid_tuning()`), and what each model must contain

Every v2 costume needs the common parts exactly as the dragon has them: `body`, `head`, `leg_*` >
`shin_*` (the shoe), `arm_*` > `forearm_*` > `hand_*`, `socket_wand` under `hand_l`, `bucket` under
`hand_r`, and under `head`: `eye_*` > `iris_*`, `alt_lash_*`, `brow_*`, `mouth_smile`,
`alt_mouth_open`, `alt_mouth_o`, `alt_mouth_grit`. All translation-only except the eyes and
`socket_wand`. A part that is missing is simply not animated (the costume still works).

| Costume | Tuning row today | Spring parts the code is ready for (build these names) |
|---|---|---|
| `dragon_kid` | tail 1.45 Hz / zeta 0.36 / limit 0.8, `tail_thump` 1, wings 2.6 Hz; fidget = wing stretch + tail thump | `tail`, `wing_l`, `wing_r` (one mesh each, origin at the root) |
| `kid_cat` | `tail_swish`, `glide_spread`, tail 1.3 Hz / limit 0.7, `bounce` 1.15; fidget = head shake + tail flick | `tail` (ONE mesh; it also gets `tail_wave`), `soft_ear_l`, `soft_ear_r` (under `head`, origin at the ear's base), `bob_bell` (under `body`, origin where it hangs from), `bob_bow` (origin at the knot) |
| `kid_witch` | P8: `hem_wave` 12 mm x 6 lobes, `float_up` 1.3, `spin_flare` 1, `arm_out` 0.22, `elbow_rest` 0.15; fidget = hat slips, hand pushes it straight (see Chunk P8) | `bob_hat` (under `head`, origin at the middle of its seat on the head) with child `soft_tip` (origin where the cone starts to flop), `soft_hair_back`, `soft_hair_l`, `soft_hair_r` (under `head`, origin at the roots), `soft_cape` (under `body`, origin at the clasp / neck), `soft_skirt` (under `body`, origin at the waist), `bob_bow` |
| `kid_ghost` | `hem_wave` 26 mm x 4 lobes, `float_up` 1.6, `bounce` 0.7, `arm_out` 0.42, `elbow_rest` 0.3 (P7: arm holes); fidget = "boo" arms (P5) | `soft_sheet` (under `body`, origin at neck height on the body's axis; the drape from inside the hood to the hem), `soft_cowl` (under `head`, origin AT THE NECK; counter-rotated below it, see Chunk P5) |
| `kid_pumpkin` | `arm_out` 0.18, `bounce` 1.2; fidget = two bounces | `soft_stem` (under `head`, origin at its base), `bob_leaf_a`, `bob_leaf_b` (origin at the stalk), `soft_vine_a`, `soft_vine_b` (origin at the attached end) |
| `kid_skeleton` | P9: `rattle` 1 (0.6 s clatter after a landing, a hit and at its fidget's start), `matte` -2 (fleece); fidget = brrr-shimmy-ta-da (see Chunk P9) | `soft_cord_l`, `soft_cord_r` (under `head` or `body`, origin at the top of the cord; the `cord` preset has gravity) |

Rules for a spring part (the code needs nothing else):

1. `soft_*` is ONE mesh node (not an empty with mesh children) whose origin is the pin. Its length is
   taken from its bounds (0.9 x the farthest corner) and it bends more the farther a vertex is from
   the origin, so model it running away from the pin; a part that curls back toward its pin bends
   wrongly. Its rest direction is toward the middle of its bounds.
2. `bob_*` is rigid; its origin is the pivot, its rest direction is toward the middle of its mesh.
3. The preset is the word after the prefix (`soft_hair_back` -> `hair`). An unknown word gets the
   default spring. Suffixes are free (`_l`, `_a`, `_back`), but avoid Godot importer suffixes.
4. Hems (`cape`, `skirt`, `sheet`) ripple round the mesh's local Y axis, growing with distance from
   the origin: put the origin on the body's axis at the top of the cloth.
5. A soft part may be the child of a bob part (the witch's hat tip) or of the head.
6. If `arm_out` leaves a v2 costume's arms too wide (the pumpkin's row was tuned on the v1 model
   whose upper arms sit inside the body), set it to 0 in the row.

Per-costume tuning that remains once the models exist: spring feel of each soft part (preset rows),
`hem_wave` on the real sheet / skirt, whether the hat's `lift` reads, `arm_out` for the pumpkin, the
cat's `tail_wave` amplitude on the new tail length, and a look at every beat sheet.

### Technique notes worth writing back to art-pipeline (P3)

- **Solve the stance, author the swing.** A planted foot that must not slide is found, not keyed: the
  sole's pivot (heel, then toe once the heel peels) moves back under the hip at exactly the ground
  speed and the thigh angle is bisected from the leg's measurements; the pelvis height follows. Only
  the airborne half of the stride is a hand-shaped curve. Measured with a per-frame sole trace, the
  planted point stays within a millimetre.
- **Short legs, big shoes:** the thigh (13 cm) cannot lift a 22 cm shoe clear while the other foot is
  planted. Draw the swinging leg up into the plush body by a few centimetres (a hidden hip hike)
  before spreading it sideways; a trace of sole heights finds the scrape that a strip at 30 fps hides.
- **Measure before tuning by eye.** One debug line per frame (state, clock, heel and toe in the world)
  answered "do the feet skate" in a minute and exposed the swing scrape nobody had seen.
- **Blink discipline.** Random blinks are held off during any move and any scripted face, and the
  first blink after a reset is randomised; otherwise the still at a fixed frame keeps catching closed
  eyes and action poses read as sleepy. Forced blinks belong to the settle after the action (the wand
  coming down), not to its key frame.
- **Eyes first, head second, and never turn the face away from the viewer for a reaction.** A look at
  the bucket is an iris slide plus a small head tip; a 25 degree head turn hides the face from a
  three-quarter camera.
- **A close camera gets an act.** When the camera is within about two metres of the face the kid runs
  a short loop (blink, grin with brows up, shy look away, double blink): that is what a face close-up
  shows, with no recorder change.
- **Ease soft parts into their limit** (limit x tanh(bend / limit)) and let the spring's own hard
  limit sit at twice that: a tail thrown by a takeoff arrives at the limit instead of slamming into it.
- **Landing strength needs a scale that does not saturate on the commonest case**: an ordinary jump
  should sit near the middle so a roof drop still has somewhere to go.
- **A bright VFX flash casts a hard shadow of the hood rim that looks like a one-frame head pop** in
  a strip. Check the frames before and after before chasing an animation bug.
- **Test spring parts on a throwaway export** of an existing model (back up, export over, record,
  restore byte for byte) rather than waiting for the first real costume.

### Verified in chunk P3

See the Progress list at the top for what was looked at in each round, and the hand-off report.

### Known weak points (P3)

- The run's planted foot is on the ground for only 2 frames in 9 (duty about 0.14 at full speed): it
  reads as a bouncy toon run, but the contact is brief and the body floats between steps.
- At a walk the swinging leg is still spread up to about 15 degrees for one or two frames in the
  slowest speeds (the hip hike covers the rest).
- The toe tap fidget is small at showcase distance.
- The hang was seen only from the ledge test's far camera: the pose holds together and the climb ends
  standing, but the hand on the lip was not seen up close.
- `mounted` (riding the car), the vault, the trampoline bounce and the uneven-ground foot placement
  were not seen at all. `set_look_target` still has no caller in the game.
- `soft_*` / `bob_*` were seen only on tube and ball test parts: discovery, bend, spin fling and the
  fall lift work; how a real sheet, cape or skirt looks with the hem ripple is untested.
- The dragon's shut-eye lids are plain (P2's note); held squeezes (hurt, pound stall) show them for
  up to 0.3 s.

## Progress (chunk P2: kit v2 + dragon_kid pilot) - keep this current

- DONE (first P2 worker, checked whole 2026-11-15): palette row 11 cells (`iris_brown`, `hair_light`,
  `hair_dark`, `mouth_dark`, `tongue`, `cheek_soft`, `dragon_jade_light`) via `set_palette_row.py`;
  `plush` material layer (index 28, source from `make_plush_source.py`); generated outputs agree with
  their JSON sources, old layers/cells byte-identical, DLL newer than the headers, kid_cat previews
  the same as before.
- DONE: `kid_kit.py` v2 section (below the v1 kit), `costume_dragon.py`, `build_costumes.py`
  (`--face --poses --close`), `verify_rig2.py`, `catalog_update.py`. dragon_kid v2 exported
  (27.2k tris), gates PASS, verify_rig2 PASS, catalog entry updated and published, first engine
  preview looked at. One C++ line in `kid_rig.cpp` (the eye is hidden while its lid shows), DLL built.
- DONE: dragon_kid recorded with `tools\showcase.ps1` into `art\review\showcase\wip_p` (stills, beat
  sheet, run / roll / glide strips looked at); two look passes (cuffs, hands, lids, fringe, brows,
  pupil, AO). The other five (v1 models) re-recorded: same as P1's stills (28 to 36 dB, the
  run-to-run noise level; `wip_p\p2\v1_costumes_p1_vs_now.png`). `kit_selftest.py` checks the
  sneaker knee. Chunk P2 is finished; details in the "Chunk P2" section below.
- NEXT (P3): drive the v2 parts (elbows, knees, wrists, face controller, `gloss` on the eyes).
  (P4/P5): port the other five costumes with the kit ("How the next costumes use the kit").

Status after chunk P1 (animation framework on the legacy models). Read `art/team/FIDELITY_PASS.md`
first; it overrides `DESIGN.md` in this folder.

## Chunk P2: kit v2 and the dragon_kid pilot (2026-11-15)

### Files (all in `art/team/characters_candy/batch05/`)

| File | What it is |
|---|---|
| `kid_kit.py` | The batch04 kit (v1 functions untouched, top of the file) plus the **KIT V2** section: `limb`, `ring`, `arm2`, `grip_hand`, `thigh2` / `leg2`, `Surf` (discs, bands, tubes that hug a surface), `eye2`, `mouths2`, `face2`, `hood_clip`, `fringe2`, `torso2`, `hood2`, `soft_part`, `bob_part`, `tilt_head2`, `bake_ao2`, `finish2`, `hide_alts`, `pose` / `unpose` / `sheet_grid`. Constants: `ELBOW_*`, `WRIST_*`, `KNEE_*`, `BUCKET_GRIP`, `WAND_GRIP`, `WAND_DIR`, `EYE`, `MOUTH_Z`, `PLUSH`. |
| `costume_dragon.py` | dragon_kid v2. `build()` returns `(parts, children)` for `KK.finish2()`. |
| `build_costumes.py` | Runs the builders (`BUILDERS`; only `dragon_kid` so far). `--face`, `--poses`, `--close`, `--noexport`, `--nosheet`. |
| `kit_selftest.py` | A plain hooded kid made only of kit calls, in the kit's sneakers, with one `soft_` and one `bob_` part. Exports nothing. Usage example for the next costumes and a check that the sneaker knee holds. |
| `verify_rig2.py` | Checks an exported glTF against contract v2 (see its docstring); writes `rig_check2.json`; `--tree` prints the part tree with pivots. |
| `catalog_update.py` | Sets `tris` 30000, `parts` and `sockets` (`bucket_light`, `wand`) for the ids in its `V2` dict only. Add a costume there when it is ported. |
| `set_palette_row.py`, `make_plush_source.py` | Row 11 cells and the `plush` layer's source (first P2 worker). Already applied; safe to re-run. |
| `costume_common.py` | Unmodified copy of batch04's helpers, for the costumes that get ported here. |

### Rebuild

```powershell
$b = "C:\Program Files\Blender Foundation\Blender 5.1\blender.exe"
& $b --background --factory-startup --python art\team\characters_candy\batch05\build_costumes.py -- dragon_kid --face --poses --close
python art\tools\gates.py dragon_kid
python art\team\characters_candy\batch05\verify_rig2.py dragon_kid --tree
python art\tools\publish_catalog.py          # ALWAYS after a re-export: see the gotcha below
tools\asset_preview.ps1 -Id dragon_kid       # art\review\engine\dragon_kid.png
tools\showcase.ps1 -Ids dragon_kid -Out art\review\showcase\wip_p ; python tools\showcase_sheets.py art\review\showcase\wip_p dragon_kid
```

The Blender build takes about 8 seconds including AO and every sheet, so iterate freely
(`--noexport` leaves the game's model alone). Sheets land in `art/review/sheets/`:
`dragon_kid.png`, `_close`, `_feet`, `_face` (zoom 4.2), `_hands`, `_poses` (16 posed views),
`_joints` (knees and elbows at +-1.2 and +-1.5 rad, up close), `_faces` (half blink, blink, the three
alt mouths, iris slides, raised brows).

**Gotcha: publish_catalog.** `art/tools/publish_catalog.py` runs the gates and marks a failing asset
unpublished; the game then shows a faceted green placeholder for it (two icospheres) while
`asset_preview.ps1` still shows the real model. Re-run it after every export and check it prints no
FAIL line.

### dragon_kid v2: part tree and pivots

27,460 triangles (cap 30,000), 32 nodes, 29 meshes. glTF / Godot coordinates (Y up, +Z front, +X
the kid's left), each node's translation relative to its parent:

```
dragon_kid                    (0, 0, 0)
  body                 mesh   (0, .30, 0)          torso, belly plates + stitches, 4 back spikes
  head                 mesh   (0, .64, 0)          hood, rim, stitches, face skin, nose, cheeks, fringe, horns, hood eyes, 4 crest spikes
    eye_l              mesh   (+.0632, .1843, .2263)  ROTATED: +Y eye up (0.02, 0.99, -0.11), +Z out of the face (0.26, 0.10, 0.96)
      iris_l           mesh   (0, 0, 0)            iris + pupil + amber crescent, same axes as the eye
    eye_r              mesh   (-.0942, .1705, .2263)  ROTATED: +Y (-0.20, 0.97, -0.11), +Z (-0.28, 0.05, 0.96)
      iris_r           mesh   (0, 0, 0)
    alt_lash_l / _r    mesh   same origins as the eyes; skin lid + lash arc (hidden)
    brow_l             mesh   (+.0647, .2610, .2014)   origin at the middle of the brow, on the skin
    brow_r             mesh   (-.1091, .2458, .2014)
    mouth_smile        mesh   (-.0095, .1089, .2323)
    alt_mouth_open / alt_mouth_o / alt_mouth_grit   mesh, same origin as mouth_smile (hidden)
  leg_l / leg_r        mesh   (+-.085, .30, 0)     thigh, ending in a ball (r 7.9 cm) on the knee
    shin_l / shin_r    mesh   (+-.0051, -.135, 0)  the whole boot: cuff, sole, 3 gold claws
  arm_l                mesh   (.165, .585, 0)      shoulder ball to elbow ball
    forearm_l          mesh   (.058, -.090, .005)  elbow ball to the cuff
      hand_l           mesh   (.022, -.090, .025)  fist round the wand
        socket_wand    node   (.016, -.036, .006)  ROTATED: +Z = (0.05, -0.9, 0.45) normalised, the v1 wand direction
  arm_r                mesh   (-.165, .585, 0)
    forearm_r          mesh   (-.066, -.085, .033)
      hand_r           mesh   (-.029, -.085, .052) fist round the bucket handle
        bucket         mesh   (0, -.030, 0)        same place in the world as v1
          bucket_candy mesh   (0, -.1577, 0)
          socket_bucket_light node (0, -.2057, 0)
  wing_l / wing_r      mesh   (+-.075, .56, -.135) ONE mesh each (v1 origin)
  tail                 mesh   (0, .34, -.14)       ONE mesh (v1 origin), 33 rings along its length
```

The face origins are not symmetric because the 5 / 3 degree head tilt is baked in (as in v1).
The dragon has no `soft_*` or `bob_*` parts; its tail and wings are the rig's built-in soft parts.
`alt_*`: `alt_lash_l/r`, `alt_mouth_open`, `alt_mouth_o`, `alt_mouth_grit`.
Everything is translation-only except `eye_l`, `eye_r` and `socket_wand`.

### What the animation code (P3) needs to know

- **Elbows.** The rest pose already has about 22 degrees of bend built in (upper arm 33 degrees out
  from straight down, forearm 14 degrees out and 15 degrees forward). The hinge is local X as the
  contract says; `_joints.png` shows it clean from -1.5 to +1.5 rad. P1 tuned upper-arm angles on
  folded straight arms, so the poses want a re-check (the run already reads well).
- **Knees.** The knee is at the top of the boot / sneaker (z 0.165), so the "shin" is the shoe and
  the thigh is the only visible leg. Clean from -1.5 to +1.5 rad.
- **Hands have no finger joints.** The fingers are modelled closed round the wand grip (left) and
  the bucket handle (right). `hand_*` can flick about the wrist; +-0.5 rad was rendered. Hand tip
  (farthest vertex from the wrist, hand-local): left (0.050, -0.088, 0.015), right (0.037, -0.058,
  0.005).
- **Wand.** `socket_wand` reproduces the v1 grip exactly (same point, same direction), so
  `cast_from()` returns the same place as before. The wand's pommel end sits inside the cuff.
- **Bucket.** Its origin is on the handle where the fist holds it; the fingers wrap the handle, so a
  swing about X (forward / back) turns inside the fist and a swing about Z tilts the handle through
  the fingers a little.
- **Eyes.** `eye_*` local +Y is the blink axis and +Z the eye normal. The eye is 9.2 cm wide and
  11.6 cm tall, a 6 mm dome on the face, with the rim, the lash line with its flicks and both white
  catchlights in the eye mesh. `iris_*` slides in the eye's local XY; about +-5 mm keeps it inside
  the white (rendered at +-5 mm, `_faces.png`). The catchlights stay put when the iris slides.
- **Blink.** `alt_lash_*` is a skin-coloured lid over the whole eye with the lash arc on it, only
  7 mm proud, so the eye must be hidden while it shows. `kid_rig.cpp` now does that (one line in
  `step_secondary`, next to the blink scale: `eye_[i]->set_visible(!(lash_[i] && closed > 0.6f))`).
  Keep it when the face controller replaces that block.
- **Brows.** Translation-only, origin mid-brow; raise by moving along head +Y (6 mm rendered) and
  tilt about head Z.
- **Mouths.** Four meshes on one origin; show exactly one. A scale pop about that origin works.
- **Gloss.** Nothing sets the `gloss` instance uniform yet; `eye_*` and `iris_*` are the meshes to
  give `gloss = 1`.
- **AO.** Baked per part with the parts it moves against left out (limb against limb, torso against
  limbs, face parts against each other), so no dark patch appears when a joint opens or an eye shuts.

### How the next costumes use the kit

1. Copy `batch04/costume_<name>.py` here. Make `build()` return `(parts, children)`:
   `parts` = `body`, `head`, `leg_*`, `shin_*`, `arm_*`, `forearm_*`, `hand_*` (+ `wing_*` / `tail`,
   each one mesh); `children` = `[(name, object, parent name)]` starting with the list from
   `KK.face2()`, plus the costume's `soft_*` / `bob_*` / extra `alt_*` parts.
2. Swap the v1 calls: `torso()` -> `torso2()`, `hood()` -> `hood2()`, `face()` -> `face2()` (static
   pieces go into the head mesh, the children list goes to `finish2`), `fringe()` -> `fringe2()`
   (pass `clip=KK.hood_clip()` under a hood so no lock pokes through it), `arm()` ->
   `arm2()` + `grip_hand()`, `leg()` -> `leg2(name, hip, thigh colour, lambda x: KK.sneaker(name, x, colours) + extras)`,
   `finish()` -> `finish2()`. `kit_selftest.py` is a complete example.
3. Build everything in world space on the UNTILTED head. `finish2` applies the head tilt to the
   head and to every child below it, attaches without parent-inverse matrices, bakes AO and exports.
4. `soft_part(name, objs, pin)` and `bob_part(name, objs, pivot)` make the spring parts: one mesh,
   origin at the pin / pivot, named `soft_<preset>[_x]` / `bob_<preset>[_x]`.
5. Hands: `grip_hand(..., fat=1.4, color=..., finger_color=..., extras=fn)` gives mitts, gloves and
   paws on the same grips (`extras(frame)` gets the grip frame for pads and bones).
6. Add the id to `BUILDERS` (`build_costumes.py`) and to `V2` (`catalog_update.py`), then run the
   rebuild commands above. `verify_rig2.py <id>` must pass.
7. Budget: the shared kit parts cost about 12k triangles (face parts 3.8k, hands 2.2k, arms 1.9k,
   bucket 1.8k, face skin and fringe 2.3k); a sneaker shin is 1.6k each.

### Known weak points (P2)

- 27.5k triangles, above the design's 22k target (under the 30k cap).
- The lids are flat skin ovals: fine for a blink, plain if held shut for long.
- The left fist is a "wand grip" (rod along the forearm), which no real hand does; it reads as a
  fist with the wand through it.
- The iris is fairly light under the bucket's glow; the pupil was enlarged once to keep the eyes
  dark like v1's.
- The plush layer was only judged on the dragon's jade.

## What exists (chunk P1)

| File | What it is |
|---|---|
| `native/src/play/kid_rig.h` / `kid_rig.cpp` | `KidRig`: bind (name lookup, reparent under body), layer blending, springs, look-at, bucket pendulum, soft parts, root transform. `KidAnimInput`, `KidEvent`, `KidTuning` + `kid_tuning()` table. |
| `native/src/play/kid_poses.cpp` | `kid_pose::*`: one function per state. The sign conventions are in the comment at the top. |
| `native/src/play/dragon_controller.*` | Physics and gameplay unchanged. `_process` now calls `animate_rig()` (fills `KidAnimInput`, calls `rig_.update()`, then footsteps / glide sparkles / wand-spin sparkles), or `animate_legacy()` (the old body, verbatim) when the game is started with `--anim-v1`. Events are latched into `anim_events_` where they happen. |
| `native/src/play/anim_kit.h` | Shared kit, not changed by this chunk. |
| `game/shaders/toon.gdshader` | New instance uniforms `soft_pin`, `soft_bend`, `cloth_wave`, `gloss` (12 of 16 instance-uniform slots used). |
| `native/src/gen/asset_library.*` | `is_alternate()` / `hide_alternates()`: `alt_*` nodes are hidden by `instantiate()` and skipped by `merged_mesh()` / `part_mesh()`. The `--preview` scene calls `hide_alternates()` too (one line in `festival_game.cpp`). |
| `art/team/characters_candy/batch05/build_native.ps1` | Builds the debug DLL under the Godot lock; log in `native/build_p.log`. |
| `art/team/characters_candy/batch05/strips.py` | Before/after frame strips of one showcase beat. |
| `art/team/characters_candy/batch05/showcase_v1.ps1` | Copy of `tools/showcase.ps1` that adds `--anim-v1`, to check the legacy path against the BEFORE clips. |

## Commands

```powershell
# build only
art\team\characters_candy\batch05\build_native.ps1
# record (builds first), then sheets and strips
tools\showcase.ps1 -Ids dragon_kid,kid_cat,kid_witch -Out art\review\showcase\wip_p
python tools\showcase_sheets.py art\review\showcase\wip_p dragon_kid kid_cat kid_witch
python art\team\characters_candy\batch05\strips.py dragon_kid run            # -> wip_p\strips\dragon_kid_run.png
python art\team\characters_candy\batch05\strips.py kid_cat jump --crop --n 24 --cols 8 --from 20 --to 90
# everything at once: build, all six costumes, sheets, the --anim-v1 check, a hang still, 45 s of
# autopilot. About 10 minutes; run it detached. Log: art\review\showcase\wip_p\verify.log
art\team\characters_candy\batch05\p1_verify.ps1
art\team\characters_candy\batch05\all_strips.ps1 -Ids dragon_kid            # every beat's strip
# the legacy animation, recorded the same way (should match art\review\showcase\before)
art\team\characters_candy\batch05\showcase_v1.ps1 -Ids dragon_kid -NoBuild
# hang pose still (ledge test)
& $godot --path game --resolution 1600x900 -- --seed=101 --ledge-test=0 --ledge-side --capture=<png>
```

## Rig part-name contract (what `KidRig::bind` expects)

Axes: Y up, +Z the kid's front, +X the kid's left. All joint nodes are translation-only at rest
(identity rotation and scale); the rig multiplies its rotation onto the rest basis, so a rest
rotation would change what "pitch" means for that joint.

Lookup is by node name at any depth; every part is optional and null-checked.

| Name | Parent in the asset | Pivot (node origin) | Driven as |
|---|---|---|---|
| `<id>` (root) | - | feet centre on the ground | whole-model tilt, hop, shake (`J_ROOT`). The costume id is this node's name. |
| `body` | root | hip height (0, 0.30, 0) | torso lean / twist; its offset is the pelvis offset |
| `head` | root (moved under `body` at bind) | neck (0, 0.64, 0) | head pitch / yaw / roll, look-at, lag spring |
| `arm_l`, `arm_r` | root (moved under `body`) | shoulder (Â±0.165, 0.585, 0) | shoulder. Left = wand arm, right = bucket arm |
| `forearm_l/r` (v2) | `arm_*` | elbow | hinge about local X, negative angle = flex (hand comes forward) |
| `hand_l/r` (v2) | `forearm_*` | wrist | small flicks (cast, wand twirl) |
| `leg_l`, `leg_r` | root (stay there) | hip (Â±0.085, 0.30, 0) | hip; also translated by the pelvis offset |
| `shin_l/r` (v2) | `leg_*` | knee | hinge about local X, positive angle = flex (foot goes back). Must carry the shoe: the sole points for grounding are read from this node's mesh bounds |
| `tail` | root (moved under `body`) | tail root | FK rotation + soft bend (single mesh) |
| `wing_l`, `wing_r` | root (moved under `body`) | wing root | FK rotation + soft bend for tip lag (single mesh) |
| `bucket` | `arm_r` today; `hand_r` in v2 | the grip (top of the handle) | world-space pendulum about the origin, punch scale on pickup |
| `bucket_candy`, `socket_bucket_light` | `bucket` | as today | unchanged (controller) |
| `socket_wand` (v2) | `hand_l` | grip point, local +Z along the wand toward its tip | the wand is parented here at 0.85 scale; without it the old hard-coded grip on `arm_l` is used |
| `eye_l/r`, `alt_lash_l/r` (v2) | `head` | eye centre | blink only so far: eye local Y scaled to 0.1, lash shown while shut |
| `iris_l/r`, `brow_l/r`, `mouth_smile`, `alt_mouth_open/o/grit` (v2) | `head` | - | bound, not driven yet (P3: face controller) |
| `soft_*` (v2) | anything | the pin | one mesh, bent in the shader (see below) |
| `bob_*` (v2) | anything | the pivot | rigid, swung about its origin by a spring |

Reparenting: at bind `head`, `arm_l`, `arm_r`, `wing_l`, `wing_r`, `tail` are moved under `body`
keeping their place (skipped if already below `body`), then every joint's rest transform is
recorded. Legs stay on the root.

Models without `shin_*` / `forearm_*` (all of today's): a knee bend is folded into the whole leg
(the leg swings back by `legacy_knee` x the knee angle and its Y scale drops to cos(knee/2), no
lower than 0.6); an elbow bend swings the whole arm forward by `legacy_elbow` x the angle. The same
pose functions therefore drive both kinds of model. P3 should re-check every pose once a model has
real knees and elbows: the thigh and upper-arm angles were tuned by eye on the folded limbs.

### Soft parts (`soft_pin` / `soft_bend`)

- A soft part is ONE `MeshInstance3D` whose origin is the pin (where it is attached). Today's
  `tail` and `wing_*` already are; `soft_*` nodes must be meshes themselves (not empties with
  mesh children).
- At bind: `len` = 0.9 x the farthest corner of the mesh AABB from the origin; `dir` (pin to tip, for
  the spring) = the direction of the AABB centre from the origin (straight down if the centre is at
  the origin). `soft_pin = (0, 0, 0, len)`: the zero axis means "bend by distance from the origin",
  which suits curled shapes such as the cat's S tail. `soft_bend = (rotation vector at the tip, 0)`.
  `extra_cull_margin` is set to 0.5 because the bend does not move the bounds.
- So: keep the pin at the mesh origin, and model the part so that distance from the origin grows
  along it (a part that doubles back toward its pin will bend wrongly). If a part needs an explicit
  axis or a rigid start, add fields to `Soft` and set `soft_pin.xyz` / `soft_bend.w` in `add_soft`.
- Spring presets are chosen by the word after `soft_` / `bob_` (`soft_hair_back` -> `hair`): hair,
  cape, skirt, sheet, cowl, ear, tip, stem, cord, vine; bell, hat, bow, leaf (`SOFT_PRESETS`,
  `BOB_PRESETS` in `kid_rig.cpp`). Tail and wings take theirs from `KidTuning`.
- `bob_*` rest direction: toward the centre of its own mesh bounds; if it has no mesh, up for
  hat / leaf / bow and down for everything else.
- The cat keeps `tail_wave` (same formula as before) on its `tail` mesh only, plus the bend.
- `cloth_wave` and `gloss` are in the shader but nothing in the kid rig sets them yet.

## How to add or change things

- **Tune a costume:** edit its row in `kid_tuning()` (`kid_rig.cpp`); fields are in `KidTuning`
  (`kid_rig.h`). Only the cat (swish, spread glide, softer tail) and the pumpkin (`arm_out`) differ
  from the defaults so far.
- **Change a pose:** edit its function in `kid_poses.cpp`. Use the `Set` helper
  (`s.arm_l(pitch, out, elbow)`, `s.leg_l(thigh, knee, out)`, `s.body(...)`, `s.wings(up, back)`,
  `s.tail(lift, yaw)`).
- **Add a state:** add a `kid_pose::` function, declare it in `kid_rig.h`, give it a `Weight` in
  `KidRig`, step the weight in `KidRig::update()` and add one `override_with` / `additive` call in
  the layer list there (order = priority). If it needs a new fact from gameplay, add a field to
  `KidAnimInput` and fill it in `DragonController::animate_rig()`; one-shots go through
  `anim_events_` (`KidEvent`).
- **Layer order in `update()`:** base (idle / walk / run by speed; rise / apex / fall by vertical
  speed; glide) -> overrides (mounted, flap, hang, climb, spin, roll, pound, hurt) -> additive
  (breath, launch, land, celebrate, skid, fidget, lean springs, look-at) -> the wand arm override
  for the cast (last, so the arm stays exactly on `kid_pose::CAST_ARM` at the moment of the shot).
- **Feet:** `apply()` finds the lowest sole point of both legs and moves the pelvis so it touches
  the ground (`plant`), and never lets a foot go below it while on the floor. Crouches are authored
  as knee bends, not as pelvis offsets.

## Contracts kept

- `bot_input`, `bot_roll`, `set_bot`, `teleport`, `set_tail_style`, `cast_from`, `celebrate`,
  `knockback`, `set_mounted`, `set_blink`, the signals (including `stepped` on the same phase
  crossings) and the glide / magic-dust / wand-spin effects behave as before. No physics, timing or
  collision value was touched; the only additions to `_physics_process` are `anim_events_ |= ...`
  lines.
- `cast_from()` calls `rig_.cast_now()`, which puts the wand arm chain on the cast pose before the
  tip position is read.
- Springs reset on `teleport()` (via `KID_EV_TELEPORT`) and on `set_visual()` (bind). Soft parts and
  the bucket are frozen / forced through roll and spin.
- The rig's random numbers are seeded with constants, so a showcase recording is repeatable.

## Shader uniforms (both lanes use these)

All default to off; an unmodified scene renders as before (kid_cat preview with the old and the new
shader: PSNR 63 dB, i.e. noise only).

- `soft_pin` (vec4): xyz = direction in mesh space along which the part runs away from the pin (the
  mesh origin); zero vector = use the distance from the origin. w = length over which the bend
  ramps to full; 0 = off.
- `soft_bend` (vec4): xyz = rotation vector (axis x radians, mesh space) reached at the tip; w =
  distance from the pin that stays rigid. Each vertex and its normal are rotated about the origin by
  `soft_bend.xyz * k`, k = smoothstep of `(along - soft_bend.w) / soft_pin.w`. Lengths are kept.
- `cloth_wave` (vec4): x amplitude in metres (0 = off), y phase, z lobes round the mesh's local Y
  axis, w = local height below which it applies, at full strength 0.2 below that. w = 0: the whole
  mesh, growing with distance from the origin (full at `soft_pin.w`, or at 0.25 if that is unset).
  Displaces along the normal; runs before the bend. Not used or looked at by Lane P yet.
- `gloss` (float): adds a tight highlight in `light()` (`pow(n.h, 160) * 1.6 * gloss`). Not used
  or looked at by Lane P yet.
- Order in `vertex()`: wind, `tail_wave`, `cloth_wave`, soft bend. The depth and shadow passes run
  the same `vertex()` (Godot does this for spatial shaders; the old `tail_wave` already relied on
  it), so cast shadows should follow the bend. Not checked by eye.

## Verified in chunk P1 (2026-11-14)

- Recorded all six costumes with `tools\showcase.ps1` into `art\review\showcase\wip_p` (6 of 6 OK,
  sheets "frames aligned"). Looked at every beat sheet, and at before/after frame strips of walk,
  run, stop_idle, jump, double_jump_glide, spin, roll, ground_pound, fire_cast, hurt,
  candy_celebrate, idle and turntable for dragon_kid, plus run / jump / glide / spin / roll for the
  cat and the witch, run for the ghost, jump for the pumpkin (`wip_p\strips`).
- `--anim-v1`: `showcase_v1.ps1` recording of dragon_kid shows the old poses on the same frames as
  the BEFORE clip (`wip_p\v1check\cmp_before_top_v1_bottom.png`). It is not pixel-identical (PSNR
  29 dB against BEFORE; the rig clip scores 23 dB): fireflies, sparkles and time-based sways differ
  from run to run.
- Idle and face beats: consecutive-frame differences are smooth (no jitter).
- 45 s of autopilot play (`--autopilot=45`) ran to `BOT_DONE` with no script errors.
- Ledge hang: one small still (`wip_p\hang_lot0.png`): the pose holds together. Nothing more.

## Known weak points and things not verified

- Hang, climb, vault and mounted poses are written but only the hang was seen, small and once.
  The bucket still hangs through the wall while hanging (DESIGN chunk 6).
- Fidgets: the stretch and the bucket peek were seen in recordings; the wand twirl, look-round and
  toe tap were not.
- The cast is hidden behind the fire effect in the showcase; the arm and recoil were not judged.
- On the legacy models a bent knee is a shortened leg and a bent elbow is a swung arm; the shoes
  squash a little in deep crouches and the roll. Real joints (P2 art) fix this.
- Feet slide at a run (as before: cadence does not match ground speed) and the run is planted by a
  penetration guard only.
- The bucket swings up to belly height in front of the kid on a hard stop and after a roll
  (`step_bucket`: forward clamp 0.65 rad, inertia 0.6). Lively, but it may want calming per costume.
- A full-height landing and the first frames of a jump throw the dragon's tail to its bend limit
  (`tail_limit` 0.95, inertia 0.42 in `add_soft`); it reads as drag but is strong.
- The ghost's sheet (`body`) is rigid: it leans with the torso and the legs swing under it.
- Head look-at only has the camera glance and an API (`set_look_target`); nothing in the game
  calls it yet (monsters, treats, doors).
- v2-only code paths are untested because no v2 model exists yet: `forearm_*` / `hand_*` /
  `shin_*`, `socket_wand`, `soft_*` / `bob_*` discovery and presets, the blink.
- `tail_wave` for the cat is now set on the tail mesh only (as the design asked); if a v2 cat tail
  has child meshes (tip tuft) they will not wave. Keep the tail one mesh.

## Technique notes worth writing back to art-pipeline

- Reparenting flat sibling parts under the torso at bind time (keep global transform, then record
  rests) gives a real upper-body hierarchy with no art change; lean and twist then read at once.
- A kneeless leg can fake a knee: swing it by a fraction of the knee angle and scale its length by
  cos(knee / 2). Crouches, a walk's foot lift and a tuck all read on a single-mesh leg.
- Ground the feet after blending instead of authoring pelvis heights: move the pelvis so the lowest
  sole point (heel and toe of both feet) touches the floor. Crouch poses become "bend the knees".
- One continuous mesh + a spring-driven rotation-vector bend in the vertex shader (pin at the mesh
  origin, bend by distance from it) gives tails and wings follow-through with no segments.
- A carried prop reads as heavy when it is a world-space pendulum from its grip, converted back
  into the hand's space each frame and clamped away from the body.
- Latch one-shot gameplay events where they happen (physics tick) and hand them to the animation
  once per rendered frame.
