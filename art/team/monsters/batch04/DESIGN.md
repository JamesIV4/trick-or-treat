# Monster pipeline upgrade: design (read-only study)

**How this was checked:** I read the C++, kit, gates, catalog and shader, parsed the exported glTFs with Python for node trees and triangle counts, and viewed all 10 engine previews, all 10 mocks and `ref_all.png`. Nothing was built or run. Two Godot facts are from memory, not verified here: the cap of 16 instance uniforms per shader, and the per-instance uniform buffer size.

## 1. Verdict

Rigid nested parts plus vertex-shader soft deformation is the right call for monsters. The mocks are already toy-jointed (skeleton ball joints, ringed spider legs, wrap-hidden mummy seams, an elbowless scarecrow), and the things that must stay continuous map onto one-degree-of-freedom shader bends. The pipeline note `S:\Repos\art-pipeline\techniques\character-costume-fidelity-pass.md` records that a rigid segment chain for the cat tail was rejected by the user, so hems, streamers, curls, hat tips and jelly must never be chains. Skinning is not needed. If one limb seam ever cannot be hidden, the low-risk hybrid is analytic weights along a single `D.sweep` parameter, not automatic weights.

## 2. Rig contract v2

**What exists today**
- `S:\Repos\Games\trick-or-treat\native\src\play\monster_art.cpp` `apply_assets` (line 792) pulls `body`, `part_a`, `part_b` through `AssetLibrary::part_mesh`, which bakes every descendant into one mesh.
- The pumpkin is deliberately skipped there ("the user preferred it"), so the in-game hopper is the procedural one, not the model in `monster_pumpkin.png`.
- Collision height and radius come only from the C++ `build_*` functions; `apply_assets` never touches them.
- Current exports are 1,632 to 2,476 triangles against a 2,500 budget.

**Loader**
- Add `AssetLibrary::part_tree(id, std::vector<RigPart>&)`. It instantiates the scene once and walks it recursively by name, never by depth.
- Every `Node3D` that is not `socket_*` becomes a joint: `{name, parent index, rest transform, own mesh only, reach}`. `reach` is the farthest vertex from the origin, used as the bend length.
- Pivots are the glTF node translations. Rest rotation is allowed (eyes aligned to the surface); rest scale is not.
- `MonsterArt` gains `std::vector<RigPart> parts` and `int rig_version`. Version 2 means the glTF has a `body` with named children beyond `part_a`/`part_b`.
- The procedural fallback fills the same vector with `body`, `part_a`, `part_b`, so one code path builds every monster.

**`Monster::init`**
- Build nodes under `visual_` from `art.parts` into a `MonsterRig` (slot table, rest transforms).
- Per part: toon override, `rim_strength`, `rim_color`, `keep_visible`.
- No shadow casting for tiny parts (eyes, lids, pupils, fingers, teeth).
- `set_extra_cull_margin(0.35)` on parts that deform in the shader.

**Backward compatibility**
- `rig_version == 1` runs today's limb code moved verbatim into `LegacyAnimator`, so unconverted monsters and the procedural pumpkin look exactly as they do now.
- Each monster switches only when its v2 glTF lands. Animators null-check every slot.
- Keep the new showcase hooks in `monsters.h` (`showcase_goal`, `showcase_face`, `showcase_windup`) and the `State` enum untouched.

**Collision, flash, death**
- Height and radius stay code-owned and unchanged. Art must fit within about 10 percent.
- Rise, death and squash stay on `visual_`, so they work for any part count.
- `hit_flash` loops over `rig.meshes()` with a cached `StringName`, only while `flash_t_ > 0`.
- The 0.28 s death and 0.55 s rise are each hard-coded twice (`_physics_process` and `_process`). Move them to `rise_time()` and `death_time()` on the animator, 0.45 to 0.7 s. Tell the showcase worker, since their timeline may assume 0.28 s.
- Thrown projectiles spawn at `pos + height*0.9` in `begin_attack`. Add an animator `muzzle()` (hand or mouth world position) with that as the fallback.

**Shader (`S:\Repos\Games\trick-or-treat\game\shaders\toon.gdshader`, 8 instance uniforms today)**
- `bend` (vec4: tip offset xyz, length): `VERTEX += bend.xyz * k*k`, `k = clamp(length(VERTEX)/bend.w, 0, 1)`. Spring-driven from C++.
- `ripple` (vec4: amplitude, phase, lobes, height): radial hem wave below local `height`.
- `gloss` (float): tighter, stronger highlight in `light()` for eyes and slime.
- `tail_wave` stays as is. Normals are not recomputed; that is acceptable below about 25 percent of length.

**Budget for 10 to 20 alive**
- At most 32 mesh parts and 8,000 triangles per monster; typical 12 to 24 parts and 4,500 to 7,000 triangles.
- Worst case is about 500 draw items and 140k triangles before shadow passes.
- Beyond 35 m from the camera, skip springs, blink and look-at and pose every third frame.
- Measure with `--autopilot=120` before and after.

## 3. Rig and animation sheets

All monsters share these:
- Eyes are parts: `eye_l/r` > `pupil_l/r`. Blink is eye scale-y to 0.08 for 0.1 s on a random 1.8 to 5 s gap.
- Pupils slide toward the player (30 percent of eye radius); the head yaws up to 35 degrees with 0.15 s lag.
- Eye `glow_boost` rests at 0.35 and flares to 1.6 during wind-up, alongside the existing orange flash at half strength.
- Hit: recoil 25 degrees away from the hit and spring back, eyes squeezed shut, limb springs kicked.
- Gait phase is driven by distance travelled, so feet do not slide.

**Pumpkin Hopper (HOP)**
- Tree: `body` > `stem`(bend) > `leaf_a/b/c`; `tendril_l/r`(bend); `arm_l/r` > `hand_l/r`; `foot_l/r`; `face` (all glow in one part).
- Rise: stem pops up, body inflates with overshoot.
- Roam: crouch in the last 0.15 s of `hop_t_`, stretch 1.12 in the air, squash 0.8 on landing; arms lag up and slap down; leaves flutter.
- Wind-up: deep squash 0.7, rocking, face flare. Slam: stretch 1.25, pitch +20 to -25 degrees, land at 0.6 by 1.3 with arms flat.
- Recover: dizzy re-inflate. Hit: face dims to 0.3. Death: caves in, glow snuffs.
- Alive: candle-flicker face, leaf follow-through.

**Bat (SWOOP)**
- Tree: `body` > `head` > `ear_l/r`(bend), eyes, `jaw`; `wing_l/r` > `wingtip_l/r`; `foot_l/r`.
- Flap: outer wing lags the inner by a quarter phase (fold on the upstroke); body bobs against the flap; ears and feet on springs.
- Wind-up: rears back, wings high and shivering. Dive: wings swept into a V, body pitched along velocity, ears flat.
- Recover: fast shallow flutter and a head shake. Death: wings fold, spiral, poof.

**Rattlebones (THROW)**
- Tree: `body`(pelvis) > `chest` > `head` > `jaw`, eyes; `scarf_tail`(bend); `arm_l/r` > `forearm_l/r` > `hand_l/r`; `bone` under `hand_r`; `leg_l/r` > `shin_l/r` > `foot_l/r`.
- Roam: bow-legged bounce, bobble-head spring, 12 Hz rattle jitter scaled by speed, jaw chatter bursts.
- Wind-up: right arm back behind the skull, left arm points at the player, chest twists -35 degrees, skull stays on target.
- Release: chest whips +30 degrees, `bone` hidden on the frame the projectile spawns. Recover: pulls a new bone (pop 0 to 1), laugh.
- Hit: skull hops 8 cm off the neck. Death: staggered collapse into a pile, skull last.

**Ghost (LUNGE)**
- Tree: `body`(bend + ripple) > eyes, `mouth`, `arm_l/r`.
- Roam: hem trails opposite velocity, continuous ripple, arms float with lag, the existing 1.7 Hz sway.
- Wind-up: shrinks back with hands over eyes (peek-a-boo). Boo: arms flung wide, mouth scaled open, eyes 1.25.
- Recover: giggle bounces. Rise: unfolds from a flat disc. Death: deflates flat, eyes linger a beat.

**Gremlin (LUNGE)**
- Tree: `body` > `tip`(bend), `ear_l/r`, `brow_l/r`, eyes, `jaw`, `tongue`; `arm_l/r` > `hand_l/r`; `leg_l/r`.
- Roam: leg blur, 15 degree forward lean, plus or minus 10 degree waddle, ears pinned by speed, claws wiggle. Idle: rubs hands, sniffs.
- Wind-up: crouch, jaw wide, brows down. Bite: stretch 1.25 with the jaw snapping shut at 60 percent.
- Recover: licks lips, shakes head.

**Mummy (GRAB)**
- Tree: `body` > `head` > `eye`, `pupil`, `knot_tail_a/b`(bend); `arm_l/r` > `hand_l/r`, `wrist_strip`(bend); `waist_tail`(bend); `leg_l/r`.
- Roam: stiff shamble, plus or minus 6 degree roll, head lolls opposite, arms out front bobbing with lag, streamers on springs.
- Wind-up: arms pulled wide and back, lean back, eye widens. Grab: arms clap together, 30 degree pitch, 0.35 s.
- Recover: over-balanced wobble. Hit: head quarter-spins and snaps back. Death: unwinding spin, streamers fly out.

**Goo Slime (THROW)**
- Tree: `body`(bend, gloss) > `curl`(bend), eyes, `brow_l/r`, `mouth`, `bubble_a..d`, `skirt`(ripple).
- Roam: volume-preserving squash and stretch on hops, landing ripple pulse, damped 6 Hz wobble, face and bubbles sliding on springs.
- Wind-up: flattens wide. Lob: rebounds to 1.35, mouth an O, goo leaves from the mouth.
- Recover: shrinks to 0.92 and regrows over the cooldown. Death: splats into a puddle.

**Scarecrow (CHARGE)**
- Tree: `pole` > `body` > `head` > `hat` > `hat_tip`(bend), `hair`(ripple), `eye_l/r`; `collar`(ripple); `arm_l/r` > `cuff_l/r`(ripple); `skirt`(ripple).
- Roam: pogo-hops on its pole (pole squash), arms flopping late.
- Wind-up: leans back, hat tip coils, 1.0 s. Charge: 25 degree lean, arms streaming back, straw rippling hard.
- Dizzy (replaces today's whole-body spin): the body precesses in a cone, head counter-rotates, button eyes spin, hat slips.

**Spider (POUNCE)**
- Tree: `body` > `abdomen`, `head` > eyes, `eye_s0..5`, `fang_l/r`, `tuft`(bend); eight `leg_XN` > `shin_XN`.
- Roam: two-bone foot planting in alternating groups of four against `world_->ground`; body height springs from the foot average; abdomen breathes; small eyes blink in a ripple.
- Wind-up: crouch, legs splayed, abdomen wiggle. Pounce: legs tuck, then star-spread at the apex. Land: dips to 60 percent and rebounds.
- Death: rolls onto its back, legs curl.

**Toadstool (PUFF)**
- Tree: `body` > `cap`(spring) > `sprout`(bend) > `sprout_leaf_l/r`; eyes, `mouth`; `arm_l/r`; `foot_l/r`.
- Roam: patter-waddle, the heavy cap lags and bobbles. Idle: tips the cap back to peek.
- Wind-up (1.1 s): cap inflates to 1.3, cheeks puffed, eyes shut. Puff: cap slams down. Recover: cap wobble.
- Hit: the cap drops over its eyes and both arms push it back up. Death: the cap spins like a coin.

## 4. Fidelity upgrades (priority order; keep collision as listed)

| Monster | Gaps against the mock | Triangles | Must not change |
|---|---|---|---|
| Pumpkin | deep ribs, carved rim thickness on eyes and mouth, leaf collar, curled stem and tendrils, three-leaf hands, root feet | 5k | squat lobed shape, triangle eyes, toothed grin; 0.9 / 0.42 |
| Bat | sclera/iris/pupil eyes replacing all-glow, pink inner ears, cream belly fluff, wing fingers and thumb claw, head tuft, feet | 5k | violet colour, amber eyes, fangs; 0.5 / 0.3 |
| Skeleton | chunky ball joints, real pelvis and ribs, fingered hands, big toed feet, larger sockets with teal pupil on a dim glow, skull seam | 7k | orange bandana (the mock drops it), teal eyes |
| Ghost | deep-lobed stitched hem, mitten arms, open mouth with tongue, smoother cap (facets noted in CHECKPOINT) | 4.5k | plain white, pink cheeks, ink eyes; no `fabric` (read as plaid) |
| Gremlin | arms become parts with claws, bigger ears with inner colour, brows, open mouth with fangs and tongue, clawed purple feet, larger tip curl | 6k | band order yellow/orange/white |
| Mummy | overlapping crossed wraps with frayed edges, head knot and tails, mitten hands, chunky feet, one big glossy eye in the dark gap | 7k | cream/bone colours; keep the squinting second eye |
| Slime | gloss, raised bubbles, drippy skirt, brows, open mouth | 4.5k | green, curl; stays opaque |
| Scarecrow | hat seams, band and bent tip, straw hair, collar, cuffs and skirt, rope ties, crossbar ends, check shirt and patch, cross-stitched buttons | 8k | purple hat, red flannel, post |
| Spider | ringed legs with pale tips, big sclera eyes and six small ones, head tuft, cheeks | 7k | the glowing jack marking on the abdomen (the mock swaps it for a web) |
| Toadstool | gills under the cap, raised spot pads, two-leaf sprout, feet, open mouth | 5.5k | red cap, cream stem, cap tilt-back |

- **Face treatment:** white sclera, a coloured iris on a non-glow or dim cell, ink pupil, and two authored glints on `eye_white`. Per-part `glow_boost` of 0.35 fixes the "bloom eats the pupils" note without touching the palette.
- **Slime translucency:** fake it with `gloss`, a strong light rim and raised bubbles. Real alpha would fight the dither cutout and sorting.
- **Scarecrow eyes:** keep a dim glow disc behind the buttons so they still read at night.

## 5. Code location and helper API

Files: `native\src\play\anim_kit.h` (header-only, shared with `DragonController`), `monster_rig.h/.cpp`, `monster_anim.h/.cpp` (one animator class per type, `make_animator(type, rig_version)`). `SConstruct` globs `src/*/*.cpp`, so no build edits are needed.

```cpp
namespace tot::anim {
struct Spring  { float x, v; float step(float target, float hz, float zeta, float dt); void kick(float impulse); };
struct Spring3 { Vector3 x, v; Vector3 step(const Vector3 &target, float hz, float zeta, float dt); void kick(const Vector3 &); };
float damp(float cur, float target, float half_life, float dt);
float damp_angle(float cur, float target, float half_life, float dt);
float ease_out_back(float t, float s = 1.70158f); float ease_in_out(float t); float ease_out_elastic(float t);
float anticipate(float t, float back = 0.25f, float split = 0.3f);
float pulse(float t, float attack, float release);
float noise1(float t, uint32_t seed);
struct Blink { float step(float dt, Rng &rng, float min_gap = 1.8f, float max_gap = 5.0f); void force(float hold); }; // closedness 0..1
struct Weights { float w[8]; void drive(int active, float half_life, float dt); };
Vector2 look_angles(const Transform3D &head_world, const Vector3 &target, float max_yaw, float max_pitch);
Vector2 pupil_offset(const Transform3D &eye_world, const Vector3 &target, float max_r);
bool two_bone_ik(const Vector3 &root, const Vector3 &target, const Vector3 &pole, float l1, float l2, Basis &upper, Basis &lower);
struct FootPlant { bool step(const Vector3 &home_world, float stride, float dur, float lift, float dt, Vector3 &out_world); };
float gait_phase(float &phase, float speed, float stride, float dt);
}
struct RigPart { StringName name; int parent; Transform3D rest; Ref<ArrayMesh> mesh; float reach; uint8_t flags; };
class MonsterRig {
  void build(Node3D *visual, const MonsterArt &art, const Ref<Material> &toon);
  int slot(const StringName &name) const;                       // -1 if absent
  void pose(int slot, const Basis &rot, const Vector3 &offset = {}, const Vector3 &scale = {1,1,1});
  void param(int slot, const StringName &uniform, const Variant &v); void param_all(const StringName &, const Variant &);
  Vector3 world_pos(int slot) const;
};
struct AnimInput { MonsterAnimState state; float state_t, windup, dt, t, speed, hop_in, dizzy, since_hit;
  Vector3 vel_local, accel_local, player_local, hit_dir_local; bool on_floor, chasing; };
struct MonsterAnimator { virtual void bind(MonsterRig &); virtual void update(const AnimInput &, MonsterRig &, Transform3D &root) = 0;
  virtual float rise_time() const; virtual float death_time() const; virtual bool muzzle(const MonsterRig &, Vector3 &out) const; };
```

`Monster` needs three new members for `AnimInput`: `chasing_` (today a local in `_physics_process`), `hit_dir_`, and read access to `hop_t_`.

## 6. Risks and how to verify

- **The pumpkin skip.** Removing it reverses a stated user preference. Build the v2 pumpkin faithful to the procedural shape and get the user's sign-off, or leave it on the legacy path.
- **Mock versus identity.** Three calls need the lead: skeleton bandana, spider jack marking, scarecrow glow eyes. My recommendation is to keep all three.
- **Baked AO.** `K.bake_ao` occludes across the whole rest pose, so a raised arm shows a dark contact patch. Batch04 needs a lane helper that bakes moving parts with self and ground occlusion only.
- **Joint gaps.** Author a ball or embedded cap at every pivot (child end sunk 3 to 4 cm into the parent). Verify with an extreme-pose sheet per monster.
- **Gates.** The ground gate needs the rest pose to touch y = 0 within 3 cm. The palette gate forbids gradient UVs. `parts` and `tris` in `art\asset_catalog.json` must be updated by the lead; `tools\` and the catalog are off limits to lane workers.
- **Node names.** No dots and no duplicates: Godot renames them, and `find_part` matches exact names.
- **"Before" footage.** V2 exports overwrite `game\assets\models\monsters\monster_<key>.gltf`. Record the before clips and copy the v1 glTFs to `art\team\monsters\batch04\v1_backup\` first.
- **Shared files.** `monsters.h`, `dragon_controller.*` and `festival_game.*` are being edited right now by the other lane (uncommitted in git status). `anim_kit.h` and the shader uniforms need a single owner.
- **Instance uniforms.** 8 today plus 3 new is 11, under the 16 cap as I remember it.
- **Visual checks per stage.** Static: `tools\asset_preview.ps1 -Id monster_<key>`. Motion: the showcase recorder per monster, then a frame sheet with `ffmpeg -i clip.mp4 -vf "fps=4,scale=480:-1,tile=6x4" sheet.png`. In context: `--seed=101 --capture-view=nest:0` and `--autopilot=120` for fps. Run `--sweep` once after the framework lands.

## 7. Work breakdown (sequential, one worker each)

| # | Chunk | Size |
|---|---|---|
| 0 | Lead: record before clips, back up v1 glTFs, decide pumpkin and the three identity calls, raise catalog budgets and parts | small |
| 1 | C++ framework: `anim_kit.h`, `part_tree`, `MonsterRig`, `LegacyAnimator` (no visual change), timing and muzzle hooks, shader uniforms; build and confirm all ten look identical | large, about 700 to 900 lines |
| 2 | Kit and pilot: `mon_kit2.py` in batch04 (`rig_tree`, eye and lid, ball joint, per-part AO, a rig verifier modelled on `characters_candy\batch04\verify_rig.py`) plus the skeleton end to end, since it has the most joints and the throw sync | large |
| 3 | Mummy and scarecrow (streamers, ripple, pogo and dizzy) | large |
| 4 | Gremlin and toadstool | medium to large |
| 5 | Ghost, slime, pumpkin (soft bodies) | large |
| 6 | Bat and spider (wing fold, leg IK) | large; the spider is the single riskiest animator |
| 7 | Polish: fps, distance LOD, extreme-pose sheets, after-video, write-back to `S:\Repos\art-pipeline\techniques\` | medium |