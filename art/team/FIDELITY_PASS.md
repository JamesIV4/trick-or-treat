# Character fidelity pass (2026-11): rules for every worker

The user asked for a massive improvement in the fidelity of the characters and their animation
(6 player costumes, 10 monsters), shown in a before/after video. The lead (main session) plans and
reviews; workers implement. Two lanes run at the same time in this one checkout:

- **Lane P (player):** `native/src/play/dragon_controller.*`, new `kid_*` files,
  `art/team/characters_candy/batch05/`, models `dragon_kid`, `kid_ghost`, `kid_cat`, `kid_witch`,
  `kid_skeleton`, `kid_pumpkin`. Design: `art/team/characters_candy/batch05/DESIGN.md`.
- **Lane M (monsters):** `native/src/play/monsters.*`, `monster_art.*`, new `monster_rig.*` /
  `monster_anim*.cpp`, `art/team/monsters/batch04/`, models `monster_<key>`. Design:
  `art/team/monsters/batch04/DESIGN.md`. Reference mocks: `art/team/monsters/batch04/ref/mock_<key>.png`.

Read your lane's DESIGN.md, `art/team/BRIEF.md`, and the previous batch's CHECKPOINT.md before you
start. Where this file and a DESIGN.md disagree, this file wins.

## Lead decisions (override the design documents)

1. **Shared animation kit** is `native/src/play/anim_kit.h` (namespace `tot::anim`, header-only,
   already written and unit-tested). Use it; do not create `core/anim_math.h`. Its API differs from
   the wish lists in the designs: springs take hz and zeta (`spring_coefs_kd` converts k/d),
   pendulums and soft/bob springs are `Dangle`, IK is `two_bone_ik(root, a, b, hinge_axis, target,
   pole)` returning `Ik2`. Read the header comments. You may add to it with small `Edit` calls
   (append, never rewrite or reformat; the other lane includes it too).
2. **Shader instance uniforms** in `game/shaders/toon.gdshader` (all default off; the first Lane P
   worker adds them; after that either lane may make small additive `Edit`s, re-reading the file
   right before editing):
   - `instance uniform vec4 soft_pin`: xyz = bend axis direction in mesh space (zero vector = use
     distance from the mesh origin), w = length over which the bend ramps to full.
   - `instance uniform vec4 soft_bend`: xyz = rotation vector (axis * radians) reached at the tip,
     w = rigid start distance. Vertices and normals rotate about the pin by `bend * k`, `k` ramping
     0..1 along the part, so length is preserved.
   - `instance uniform vec4 cloth_wave`: x amplitude (m), y phase (radians), z lobes / wavenumber,
     w = local height below which the wave applies (0 = whole mesh, scaled by distance from origin).
   - `instance uniform float gloss`: 0 off; 1 = tight strong highlight (eyes, slime, vinyl).
   Monsters use the same four (the monster design's `bend` = `soft_bend`, `ripple` = `cloth_wave`).
3. **No motion-review mode of its own.** Do not build `--rig-gym`. The showcase recorder is the
   motion review tool and the source of the before/after video:
   `tools\showcase.ps1 -Ids <id[,id]> -Out art\review\showcase\wip_<lane>` then
   `python tools\showcase_sheets.py art\review\showcase\wip_<lane> <ids>`.
   It takes the Godot lock, builds the debug DLL, imports, records about 35 s per id and writes
   `<id>.mp4`, stills and `<id>_beats.png`. The matching BEFORE material is in
   `art\review\showcase\before\` (never write there).
4. **The recorder is frozen.** Do not edit `native/src/play/festival_showcase.cpp`,
   `tools/showcase.ps1` or `tools/showcase_sheets.py`, and keep everything they call working with the
   same meaning: `DragonController::bot_input`, `bot_roll`, `set_bot`, `teleport`, the game's
   `cycle_costume`, `hurt_player`, the pickup callback, and `Monster::showcase_leash/goal/face/windup`.
   If you believe a recorder change is unavoidable, stop and say so in your report instead.
5. **Gameplay does not change.** No change to physics, movement speeds, jump/roll/spin timings,
   collision sizes, monster AI, attack wind-up/recover times, damage, projectile behaviour or spawn
   rules. Animation is visual only. Monster death and rise visuals may run up to 0.6 s. The player's
   takeoff, cast and roll must stay instant (anticipation is visual lag only).
6. **Fidelity, not conformity.** Raise polish; keep each character's design, silhouette and colour
   identity. The five costumes (ghost, cat, witch, skeleton, pumpkin) follow the user's mock
   `art/team/characters_candy/batch04/ref/ref_all.png` in design, colours and accessories, but keep the
   kid's own face identity, the candy bucket and the wand; no wings on ghost/witch/pumpkin; the cat tail
   is one continuous undulating S (never a chain of segments); no hair on top of the pumpkin cap.
   Kids' faces: same identity, higher fidelity (larger glossy eyes with iris and catchlights, lash
   line, brows, blinks, mouth shapes).
   Monsters follow their mock for fidelity only: keep the skeleton's orange bandana, the spider's
   glowing jack marking on the abdomen and the scarecrow's glowing eyes. Monster nests are out of scope.
7. **Pumpkin Hopper.** The game currently skips the Blender pumpkin and uses the procedural one (the
   user preferred its look). The v2 pumpkin must be faithful to the procedural shape (squat lobed
   body, triangle eyes, toothed glowing grin) at higher fidelity, and is switched in with a single
   clearly named constant so it can be reverted.
8. **Floppy things are never rigid chains.** Tails, hair, capes, sheets, hat tips, streamers, hems and
   jelly are one continuous mesh bent in the shader (or a single rigid part on a spring). Rigid
   nested joints only where anatomy is hinged (elbow, wrist, knee, jaw, eyes, brows, wing fold).
9. **Budgets.** Costumes up to 30,000 triangles, monsters up to 8,000 (typical 4,500 to 7,000), at
   most 32 mesh parts per monster. For your own asset ids only, you may update `tris`, `parts` and
   `sockets` in `art/asset_catalog.json` with a small script that loads the JSON, changes only your
   ids and writes it back in one go (the other lane edits the same file), then run
   `python art/tools/publish_catalog.py`. Check your entries afterwards.
10. **Palette.** New cells: Lane P appends to row 11 (15 free). Lane M appends to row 14 (4 free), then
    row 13 from index 3 upward. Append only; never change an existing cell. Follow the existing
    `set_palette_row.py` pattern in a copy inside your batch folder, load-modify-write in one go.
    Material layers: do not change existing layers. Prefer the `gloss` instance uniform to a new
    layer; only the Lane P kit worker may add at most two layers, and only if clearly needed.
11. **Backward compatibility.** Models without the new parts must still animate (legacy parts get the
    new motion where possible). Player: today's `_process` animation stays reachable as
    `animate_legacy()` behind `--anim-v1`. Monsters: unconverted monsters run a `LegacyAnimator` that
    looks exactly like today.

## User feedback on the first results (2026-11-15) — binding

12. **The kid's head moves with the body. Never steady it against the body.** The user reported a
    jerky head when collecting candy and in the walk and run. The causes were snaps, fixed at source:
    every candy pickup restarted the "look into the bucket" pose from zero (candy lands about four
    times a second along a trail), the happy hop was cut short and restarted, the head's lead into a
    turn came straight from the turn rate, the close-camera act was dropped in one frame when the
    kid started moving, and the body's height had a sharp kink at every footfall. A follow-up attempt
    that damped the head against the body's bounce and turn was rejected outright ("like the body is
    hanging from their head"). So: fix a snap where it starts; never add head stabilisation, a head
    filter, or head damping. Rules that follow:
    - A pose triggered by an event that can repeat quickly (candy, hits, footsteps) is an eased level
      that the event renews; it is never a clock reset to zero. See `cele_look_`, `cele_lift_`,
      `hop_t_` in `kid_rig.cpp` and `kid_pose::celebrate(p, c, look, lift, hop_t, tier)`.
    - A layer's clock keeps running while its weight fades out (`mug_t_`).
    - Check with the trace, not by eye alone: `art\team\characters_candy\batch05\head_showcase.ps1
      -Name <x> -Candy 4` records the showcase with candy landing four times a second and prints head
      turn speed, snaps and kinks (`head_smooth.py`); `head_trace.ps1` does the same in the
      self-playing game. `TOT_RIG_CANDY=<per second>` is the review switch behind it. While it is on
      the bucket pulses constantly; that is the test, not the game.
13. **Kid's eyes: no white rim.** The user finds a white rim or outline around the kid's eyes strange.
    No sclera ring, sliver or light outline around the eye on any costume: dark lash line, warm brown
    iris, pupil, amber lower crescent, two white catchlights, as in the mock.
14. **Monsters must not read as robots or toys.** The user said Rattlebones' head now reads as a robot:
    too little bone and skull shape, and glossy glowing blue lens eyes. For every monster: organic
    forms and materials first (bone looks like bone, cloth like cloth, flesh like flesh); eyes are
    not glassy lenses with concentric rings and a glow; a glow, where the monster has one, is soft
    and sits inside a dark socket or behind the surface. Keep gloss for things that are wet or
    polished.

15. **Bedsheet Ghost costume reads as one sheet.** The user saw the first v2 ghost (a wide hood cape
    flaring out over a separate lower sheet, with a visible gap between the two) and said the two
    layers look too far apart; he prefers a single-sheet look, or at least a cohesive one. The
    silhouette is one continuous drape from the top of the head to the hem, as in the mock: no
    capelet step, no flared second hem at the shoulders. The hood (on the head) and the sheet (on
    the body) still have to be two parts because the head turns, so hide the join: the body sheet
    comes up over the shoulders to meet the hood, the surfaces lie within about a centimetre of each
    other with the same slope across the join, the same cells and fold lines carry across it, and the
    join sits in a fold or under the hood's lower edge. Check it from the front, the side and the
    back, and with the head turned and tilted.
    **Arms (user, same day): "make arm holes for the ghost kid's arms, not try to drape over the
    kid's arms".** The sheet has an arm hole at each shoulder and the kid's own arms come out through
    them. No sheet-coloured sleeves (in the game they lined up into a second hem and read as a cape
    over a skirt), no hem lifted or hitched over the arms, no cloth tenting over a raised arm. One
    plain hem all the way round.

16. **Game windows stay out of the user's way.** The user works and plays on this machine while reviews
    run and asked (2026-11-15) that recordings and test runs do not cover the screen or make sound.
    Every windowed Godot launch goes through `tools\godot_bg.ps1` (`Start-GodotBackground`): the
    window sits off-screen, audio is off, and keyboard focus is handed back at once. `tools\showcase.ps1`,
    `tools\asset_preview.ps1`, `tools\review.ps1` and batch05's `p1_verify.ps1`, `head_trace.ps1`,
    `jitter_test.ps1` already do. Never start a windowed Godot with a bare `& $godot ...` or
    `Start-Process`: if you use or write a script that does (batch05 `p3_hang.ps1`, `showcase_v1.ps1`,
    batch04 `preview_big.ps1` still do), convert that launch first. Headless runs (`--headless
    --import`) are fine as they are. Do not start a window minimized: Godot stops drawing and a
    recording comes out frozen.
17. **The kid, the car and the camera are drawn between physics ticks** (`native/src/play/tick_smooth.h`).
    Physics runs 120 times a second and frames are drawn at any rate, so anything drawn where physics
    left it judders on screen (measured: 3.3 px every frame at a run; 0.00 px with the fix). Aim
    cameras at `DragonController::smooth_position()`, never at the body's `get_global_position()`,
    and when you move a visual that follows a physics body, add its `TickSmooth` offset. Check with
    `art\team\characters_candy\batch05\jitter_test.ps1 -Name <x>` (add `-NoSmooth` for the old
    behaviour).

18. **Nothing soft sinks into the ground.** The user saw the dragon's tail go through the ground when
    the kid stops (measured: its underside 24 cm under the floor on a stop, 37 cm after a spin). Tails
    now rest on the ground (`keep_tail_above_ground` in `kid_rig.cpp`; the trace prints
    `TAIL raw=.. now=..`, the lowest point above the floor before and after the correction). Any new
    soft part that can reach the ground (a long cape, a sheet hem, a monster's tail or streamer)
    needs the same check: look at stops, landings, crouches and the death pose from a low side view.

## Working in a shared checkout

- **Git:** read-only. No commit, stash, checkout, restore, reset, clean or branch switch: the other
  lane's uncommitted work is in the same tree. The v1 models are recoverable from git HEAD
  (`git show HEAD:<path>`), so you do not need backups.
- **Other lane's files:** never edit them. Shared files (`anim_kit.h`, `toon.gdshader`,
  `native/src/gen/asset_library.*`, `art/asset_catalog.json`, `art/palette/palette.json`,
  `festival_game.*`) get only small targeted `Edit`s, re-read immediately before editing.
- **Builds:** one DLL for both lanes. Keep your own code compiling between edits. If a build fails
  inside a file that is not yours, the other lane is mid-edit: wait two minutes and retry (up to six
  times) rather than touching it. A failed link leaves the previous DLL in place.
- **Godot / Blender:** PowerShell only (the Bash tool is unreliable here). Godot console exe:
  `C:\GameDev\Godot_v4.7-stable_win64\Godot_v4.7-stable_win64_console.exe`. Blender:
  `"C:\Program Files\Blender Foundation\Blender 5.1\blender.exe" --background --factory-startup --python <script>`.
  Every Godot run, import and native build goes under `tools\godot_lock.ps1`
  (`Enter-GodotLock` / `Exit-GodotLock`; the showcase, review and asset_preview scripts already do).
  Never kill a Godot process you did not start (the user's editor may be open). If
  `game\native\totnative*.dll` is locked, rename it aside. Do not run `--import` while the user's
  editor is open on the project. Never cut a Godot process's output pipe (`Select-Object -First`).
  Anything over two minutes runs detached with a log file (`Start-Process ... -RedirectStandardOutput`)
  and is polled.
- **Leftovers:** if `game\override.cfg` exists and no showcase run is in progress, a recorder run was
  killed; say so in your report (the script refuses to overwrite one it did not write).
- **Do not touch** `.itchio-api-key`, the user's saves, or anything outside this repo except reading
  `S:\Repos\art-pipeline` for techniques (write-back to that repo is done later by the lead).

## Proving your work

Gates passing is not approval. For every model: `python art/tools/gates.py <id>` PASS, then
`tools\asset_preview.ps1 -Id <id>` and LOOK at `art/review/engine/<id>.png` (Read the image). For every
animation change: run the showcase for the affected ids, build the beat sheets, and LOOK at them next
to the BEFORE sheet and the reference (mock or `ref_all.png`). Also pull frame strips of the beats you
changed, e.g. `ffmpeg -ss <t> -t 2 -i <id>.mp4 -vf "fps=12,scale=320:-1,tile=8x3" strip.png`, to judge
the motion itself: arcs, overlap, no popping, no joint gaps, no parts detaching, nothing stacked or
left visible that should be hidden. Iterate until it is clearly better than before in both model and
motion. Report honestly what you looked at, what is still weak, and what you did not verify.

Hand-off: write or update `CHECKPOINT.md` in your batch folder (what exists, how to rebuild, contracts
the next worker must follow, known weak points, technique notes worth writing back to art-pipeline).
