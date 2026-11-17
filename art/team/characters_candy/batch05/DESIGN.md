# Player character rig and animation upgrade: design

Nothing was built or run. I read the code, kit, gates, catalog and checkpoints, and looked at the mock, the engine previews and the `_close` sheets. Joint offsets for new nodes are computed from `kid_kit.py` constants, not measured in a glTF. Three engine facts are from memory and need checking: the 16 instance-uniform cap, the importer's name-suffix rule, and glTF extras arriving as node meta.

## 1. Verdict

The architecture is right, with one correction: do not build tail, hair, cape or hat-tip chains from rigid segments. `S:\Repos\art-pipeline\techniques\character-costume-fidelity-pass.md` records that a segmented tail "read as segments and was rejected".

- **Rigid nested joints** only where anatomy is hinged: elbow, wrist, knee, plus face parts.
- **Everything floppy** stays one continuous mesh with its origin at the pin point, bent in the vertex shader by a spring-driven uniform (a generalisation of `tail_wave`). This also works on today's legacy `tail` and `wing_*` meshes with no art change.
- **Skinning stays rejected.** Weights could be assigned analytically along `D.sweep` limbs, but it forces a second pose path (Skeleton3D vs Node3D), bone attachments for bucket and wand, and armature support in `kid_kit.finish`, `K.merge` and `verify_rig.py`. The only gain is smoother elbows and knees, which ball-joint overlaps on puffy sleeves hide.
- **Bigger structural problem:** all parts are siblings under the root, so today's body lean leaves the head and arms behind. Fix this in code at bind time, not in the art.

## 2. Rig contract v2

Coordinates are glTF/Godot: Y up, +Z front, character-left +X. The v1 top level is unchanged (same names, same origins, flat under the root), so the catalog `parts` only gain names.

```
<id>
  body (0,.30,0)            + costume soft_*/bob_* children
  head (0,.64,0)
    eye_l, eye_r            origin at eye centre, local Z = eye normal, Y = eye up (rotation allowed)
      iris_l, iris_r        iris + pupil + amber crescent; slides ±8 mm in eye XY
    alt_lash_l, alt_lash_r  closed-eye arcs
    brow_l, brow_r
    mouth_smile, alt_mouth_open, alt_mouth_o, alt_mouth_grit   same origin
    costume soft_*/bob_* children
  leg_l/r (±.085,.30,0)
    shin_l/r  local (±.005,-.135,0)   knee at the sneaker collar; shin + whole shoe
  arm_l (.165,.585,0)
    forearm_l local (.058,-.090,.005)
      hand_l local (.022,-.090,.025)  -> socket_wand
  arm_r (-.165,.585,0)
    forearm_r local (-.066,-.085,.033)
      hand_r local (-.029,-.085,.052)
        bucket local (0,-.030,0) -> bucket_candy, socket_bucket_light
  wing_l/r (dragon), tail (dragon, cat)   unchanged origins, single meshes
```

- **Required in v2 for all six:** `shin_*`, `forearm_*`, `hand_*`, `eye_*`, `iris_*`, `brow_*`, `alt_lash_*`, the four mouths, `socket_wand`.
- **Costume optional:**
  - cat: `soft_ear_l/r`, `bob_bell`, `bob_bow`
  - witch: `bob_hat` with child `soft_tip`, `soft_hair_back/l/r`, `soft_cape`, `soft_skirt`, `bob_bow`
  - ghost: `soft_sheet` (origin at neck height, child of body), `soft_cowl` (child of head)
  - pumpkin: `soft_stem`, `bob_leaf_a/b`, `soft_vine_a/b`
  - skeleton: `soft_cord_l/r` (hood drawstrings)
  - dragon: none
- **Prefix meanings:**
  - `soft_` is one mesh with origin at the pin, bent in the shader by a spring.
  - `bob_` is a rigid part swung about its origin by a spring.
  - `alt_` is hidden by default. `AssetLibrary::instantiate` must hide these and `merge_into` must skip them, otherwise the preview scene, shop stage, costume pickups and the cine lineup show every mouth stacked.
- **Limb joints** are ball joints: both sides end in a sphere of the limb radius centred on the joint.
- **Backward compatibility in `find_parts`:**
  - Keep name-based recursive lookup, with every new part optional.
  - After lookup, reparent `head`, `arm_*`, `wing_*` and `tail` under `body` (keep global transform, then record rests). Legs stay on the root and receive only the pelvis translation.
  - No `forearm`/`shin` means FK only with IK off. No `eye_*` means the face controller is off. No `socket_wand` means the current hard-coded grip.
  - A legacy `tail`, `wing_*` or `bucket` still gets springs.
  - The costume id is `visual->get_name()` (set by `AssetLibrary::instantiate`), so `set_tail_style` can go.

## 3. Animation system

**Files**
- `native/src/core/anim_math.h`: header-only shared helpers.
- `native/src/play/soft_part.h/.cpp`: soft and bob springs, usable by monsters.
- `native/src/play/kid_rig.h/.cpp`: bind, blend, springs, IK, apply.
- `native/src/play/kid_poses.cpp`: one function per state.
- `native/src/play/kid_face.cpp`.
- `DragonController` keeps physics, fills a `KidAnimInput` (speed, vy, yaw rate, state flags, ledge, latched events) and calls `rig_.update()`.
- Today's `_process` body is kept as `animate_legacy()` behind `--anim-v1`, for the before video and as a kill switch.
- SCons already globs `src/*/*.cpp`.

**Blending.** Each state has a weight eased by a critically damped track (0.06–0.15 s). Poses are plain structs of joint eulers and offsets.
1. Locomotion, normalised: idle, walk, run, rise, apex, fall, glide.
2. Full-body overrides: roll, pound, hang, climb, hurt, mounted, spin.
3. Upper-body overrides: cast, flap.
4. Additive: breath, fidget, landing, celebrate, turn lean, look-at.
5. Springs, then IK, then face.

**Pose functions**
- **idle:** breathing at 0.28 Hz (body Y +1.5%, head +4 mm), a weight shift every 4–7 s, and a fidget every 6–12 s. Fidgets: peek into the bucket, wand twirl, look around, toe tap, plus one per costume (cat ear and tail flick, witch straightens hat, ghost "boo" arms, skeleton rattle shiver, pumpkin stem wobble, dragon wing stretch and tail thump).
- **walk:** thigh ±0.5, swing knee to 0.9, pelvis bob 1.5 cm, arms ±0.35 with elbows −0.4.
- **run:**
  - Body lean 0.28 (now carrying head and arms), head counter-pitched 60%.
  - Thigh ±1.0, recovery knee to 1.5.
  - Wand arm pumps ±0.9 with elbow −1.3; bucket arm ±0.3.
  - Torso counter-twist ±0.12; bank into turns `clamp(yaw_rate*0.06, ±0.3)`.
  - Pitch from acceleration, and a skid lean-back on stop.
- **turn:** head leads by 0.3 rad, soft parts lag.
- **jump:** takeoff is never delayed, so anticipation is visual only (a 2–3 frame pelvis lag of −4 cm with knees at 0.6, then a 1.12 stretch).
  - rise: arms up, legs trailing, mouth open.
  - apex: knees tucked to 0.9, arms wide, eyes wide.
  - fall: legs reach down, arms float up, cloth and hair bend upward with vy.
  - land: crouch scaled by impact (pelvis −2 to −7 cm, feet flat), head dip and overshoot, blink above impact 8.
- **flap:** dragon does two beats with tip lag and a back arch. Wingless costumes do a star-jump with a wand flourish.
- **glide:** dragon holds wings level with membrane flutter, body pitched 0.5, legs trailing. Wingless costumes parachute their cape, sheet or skirt (bend up and out by 0.6). The cat spreads its limbs.
- **spin:** counter-twist, arms out, tail and cape forced outward (springs frozen), 0.25 rad yaw overshoot afterwards.
- **roll:** tight tuck (knees 1.6, elbows −1.8), springs frozen, stretch on exit.
- **pound:** stall is tuck, eyes shut, grit. Dive is a 1.18 stretch with cloth streaming up. Impact is a squash to 0.72, pelvis −8 cm, then recovery with overshoot.
- **hang / climb:** hand IK to the lip, body pendulum, knee kicks. Climb is a push-up with one knee over between t 0.4 and 0.8. Vault is a hand plant with a leg swing.
- **cast:** release-first, because `cast_from()` must return the tip immediately, so there is no wind-up. Chain snaps to point, wrist flick, torso twist −0.2, recoil, 0.3 s settle.
- **hurt:** arch away from the hit, limbs flail, eyes shut, grit, 0.3 s shake.
- **celebrate:** additive glance at the bucket and open mouth. Tier 1 and above adds a hop, wand arm up and "^_^" lashes.
- **mounted:** seated (thighs −1.45, knees 1.3), body spring on car motion, hair and cape streaming.

**Springs**
- Root squash (existing), pelvis vertical, body pitch and roll, head lag (k 90, d 12).
- Bucket pendulum about its grip, in world space, clamped ±1.1 rad. It also works on legacy models.
- Soft parts: each has a 2-axis tip spring fed by pin acceleration, gravity and drag.

| Part | k | d |
|---|---|---|
| Hair | 70 | 7 |
| Cape | 30 | 4 |
| Sheet | 22 | 5 |
| Ear | 160 | 10 |
| Hat tip | 55 | 5 |
| Stem | 120 | 6 |
| Dragon tail | 45 | 6 |
| Cat tail (plus `tail_wave`) | 35 | 5 |
| Wing membrane | 140 | 12 |

- Bob parts: bell (k 80, d 3) and hat (k 200, d 14, lifting on falls).
- Reset all springs in `teleport()` and `set_visual()`.

**IK (two-bone)**
- Hands to the ledge lip while hanging or climbing.
- Feet when grounded under 1.5 m/s and on landing: two rays, pelvis drop up to 8 cm.
- No foot IK while running. Cadence is about 2.8 Hz at 7.9 m/s, so feet slide by design; keep `phase_` semantics for the `stepped` signal.

**Look-at.** New `set_look_target(pos, weight)`. Priority: cast aim, monster within 7 m, nearby treat or door, camera glance when idle, move direction. Eyes take the first ±0.35 rad; the head takes the rest (yaw ±1.0, pitch ±0.45, 6 rad/s).

**Face**
- Blink every 2.2–5.5 s: 0.11 s closing, 0.14 s opening, by scaling `eye_*` Y to 0.1 and showing the lash arc. 15% are double blinks. Forced on a hard landing, hurt and cast.
- Brows: up for jump, down-in for cast and pound, worried for hurt and hang.
- Mouths, swapped with a two-frame scale pop: smile (default), open (rise, glide, celebrate, long run), o (flap, spin, fast fall, hang), grit (hurt, pound stall, roll, climb).
- Eye scale 1.12 at apex, 0.6 squint.

**Costume behaviour** lives in a tuning table keyed by id.
- cat: `tail_wave` plus bend, ears flatten in a fall and perk on landing.
- witch: hat lifts and re-seats, tip whips, cape parachutes.
- ghost: hem ripples with 4 lobes, sheet balloons on a fall.
- pumpkin: arm abduction +0.35 (the checkpoint says its upper arms sit inside the body), stem and leaves wobble.
- skeleton: rattle jitter on landing and hurt.
- dragon: tail drag and thump, wing tip lag.

**Shader additions** (`game/shaders/toon.gdshader`), all defaulting to off:
- `instance uniform vec4 soft_pin`: xyz axis in mesh space (zero means radial), w length.
- `instance uniform vec4 soft_bend`: xyz rotation vector at the tip, w rigid start distance. Each vertex and normal rotates about the pin by `bend*k`, which preserves length.
- `instance uniform vec4 cloth_wave`: amplitude, phase, wavenumber, lobes; displaces along the normal.
- Optional `uniform float layer_gloss[32]` for glossy layers.

That makes 11 of what I recall is a cap of 16 instance uniforms.

## 4. Model fidelity

**Shared gaps against the mock, in priority order**
1. **Eyes.** Today they are faceted ink ellipsoids, 7 × 9.4 cm on a 54 cm head, with no sclera, lash line or brows. Build eyes about 30% larger (W 0.046, H 0.058) and 16–20 sided: a thin `eye_white` rim, a near-ink brown iris with the existing amber lower crescent, both white catchlights fixed on the eye, an upper lash stroke with an outer flick, and brows.
2. **Mouth and nose.** Wider smile (half-width 0.028 to 0.036), an open mouth with a teeth band and tongue, and a small nose bump.
3. **Hands.** Today a mitten blob and thumb (arms are 278–306 tris). Build four chubby fingers and a thumb modelled in grip poses around the bucket handle and the wand. Cat gets 1.4× paw mitts with five pads; skeleton gets finger bones on real fingers.
4. **Hair.** The fringe is a row of four lumps. Build 5–7 overlapping swept bangs with a highlight streak, and layered flat locks for the witch.
5. **Silhouette faceting.** Hood 28×15 to 40×24, torso 20 to 32 segments.
6. **Materials.** Felt mottles the dragon and black reads as vinyl. Wants a `plush` layer and a `gloss` layer (28 of 32 slots used; lead-owned tooling).
7. **Limb joints and cuffs** with thickness.

New palette cells are needed first (iris brown, hair light, mouth dark, tongue, cheek soft). Row 6 is full; row 11 has 15 free.

**Per character**
- **dragon_kid (9.4k to 22k).** Plush, wing ribs and a scalloped membrane, cuffs back, belly plates. Keep the jade colour, gold horns, spike counts, wing spread, tail length, bare hands and the four-eye hood.
- **kid_ghost (11.5k to 26k).** Remove the ragged neck seam (cowl overlaps a sheet that rises inside the dome), softer folds, drapey sleeves, hem as `soft_sheet`. Keep the high oval sheet eyes and the hitch over the bucket.
- **kid_cat (13.6k to 28k).** Fur tufts at hood rim, cheeks, chest and ear insides; big paws; thicker tail with a tip tuft. Keep the S tail as one mesh, plus bow, bell and star cheek.
- **kid_witch (15.2k to 30k).** Hair (the checkpoint calls it tube-like), separate cape with a star clasp, layered ruffle, lighter brim underside. No broom; the wand stays.
- **kid_skeleton (15.5k to 30k).** Skull still reads squat and panda-ish and the back is a plain dome: larger oval sockets, back sutures, bones shaded with `bone_shade`, fleece black.
- **kid_pumpkin (15.5k to 30k).** Deeper glossy ribs, proper puff sleeves and elbows, swept bangs. No hair on the cap.

For all six, keep the face identity, bucket, wand, sneaker designs and the v1 joint origins.

## 5. Shared helpers (`native/src/core/anim_math.h`, namespace `tot::anim`)

```cpp
float ease_in_out(float t); float ease_out_back(float t, float s = 1.7f);
float ease_out_elastic(float t); float smooth_pulse(float t, float in, float hold, float out);
float exp_decay(float cur, float target, float rate, float dt);
float angle_delta(float from, float to);
float noise1(float t, uint32_t seed); godot::Vector3 noise3(float t, uint32_t seed);
struct Spring1 { float x = 0, v = 0; float step(float target, float k, float d, float dt); void kick(float dv); void reset(float x0 = 0); };
struct Spring3 { godot::Vector3 x, v; godot::Vector3 step(const godot::Vector3 &target, float k, float d, float dt); void reset(const godot::Vector3 &x0 = {}); };
struct Damped { float x = 0, v = 0; float step(float target, float smooth_time, float dt); };
struct Pendulum { godot::Vector2 ang, vel; godot::Vector2 step(const godot::Vector3 &pivot_acc_local, const godot::Vector3 &gravity_local, float len, float damp, float limit, float dt); };
struct BlinkTimer { float step(float dt, RandomPcg &rng); void force(); void hold_open(float s); };
struct LookAt { godot::Vector2 step(const godot::Vector3 &target_local, godot::Vector2 max_yaw_pitch, float speed, float dt); };
bool two_bone_ik(const godot::Vector3 &root, const godot::Vector3 &target, const godot::Vector3 &pole, float l1, float l2, godot::Quaternion &upper, float &hinge);
struct Weight { float w = 0; float step(bool on, float t_in, float t_out, float dt); };
```

`soft_part.h`:
```cpp
struct SoftPart { void bind(godot::GeometryInstance3D *g, const SoftPreset &p); void step(float dt, const godot::Vector3 &bias = {}); void freeze(const godot::Vector3 &bend); void reset(); };
struct BobPart { void bind(godot::Node3D *n, const BobPreset &p); void step(float dt); void kick(const godot::Vector3 &v); void reset(); };
```

## 6. Risks and gotchas

- **Stacked `alt_` nodes.** `K.bake_ao([root])` in `kid_kit.finish` would let stacked alternates occlude each other, so exclude `alt_` meshes or bake them separately. Hide them for Blender sheets too.
- **Head tilt.** `tilt_head()` bakes a −5°/−3° tilt into the head mesh. Face child nodes must go through the same matrix or they float off the face.
- **Importer name suffixes.** The `.import` files have `nodes/use_node_type_suffixes=true`. Avoid names ending `_col`, `_rigid`, `_occ`, `_wheel`, `_loop`, `_noimp`, `_navmesh` or `_vehicle` (trailing digits are stripped first, as I recall).
- **Tail wave.** `set_visual` applies `tail_wave` to every mesh under `tail`, and the wave uses each mesh's own origin. Restrict it to the tail's own mesh.
- **Wand and cast.** `attach_wand` hard-codes a grip relative to `arm_l`. `cast_from()` must set the whole arm chain before reading `socket_tip`.
- **Verifier and gates.** `verify_rig.py` (batch04) asserts bucket under `arm_r`, translation-only nodes and a hand-tip delta under 1 cm, so fork it for v2. `gates.py` only checks names.
- **Vertex colour channels.** `COLOR.g` and `COLOR.b` hold AO copies, not zero, so they cannot be used as soft weights without changing the kit.
- **Culling.** Shader bends do not move the AABB; set `extra_cull_margin` of about 0.5 on soft meshes.
- **Event timing.** Events fire in `_physics_process` but animation runs in `_process`, so latch them. Roll, spin and teleport would explode springs, so freeze or reset.
- **LODs.** `meshes/generate_lods=true` may collapse tiny face meshes at distance (unverified); raise `lod_bias` on face nodes if eyes shimmer.
- **Parenting.** Use `D.attach`, not `K.set_parent` (known parent-inverse bug).
- **Shadow pass.** Whether it runs the same `vertex()` is unverified.
- **Extras as meta.** glTF extras arriving as node meta in 4.7 is unverified, so presets stay in a C++ table.

**How to verify each stage**
- **Code:** add a `--rig-gym=<id>` mode: studio and night turntable that steps through every state with a label, written with `--write-movie`. Review as ffmpeg tile sheets. This doubles as the before/after source with `--anim-v1`.
- **Art:** add `--face` (zoom 5) and `--poses` sheets to `build_costumes.py` (elbows −1.2, knees +1.2, hang, blink, each mouth) to catch joint gaps before the engine. Then `tools\asset_preview.ps1`, and gameplay-distance crops.

## 7. Work breakdown (sequential, one worker each)

1. **Framework on legacy models (large, about 1,300 lines C++ and 40 shader).**
   - `anim_math.h`, `soft_part`, `KidRig` extraction, runtime reparent.
   - Pose blending for all states in FK, bucket pendulum, soft bend on the legacy tail and wings, head look-at.
   - Event latching, `--anim-v1`, rig gym.
   - Gives a visible jump before any art.
2. **Kit v2 and dragon pilot (large).**
   - Arm and leg chains with ball joints, grip hands, face v2, lock helper.
   - `alt_`/`soft_`/`bob_` helpers, AO exclusions, `verify_rig2`.
   - Rebuild `dragon_kid`.
   - Lead does palette cells, catalog `parts` and budgets, and the optional `plush`/`gloss` layers.
   - Can run alongside chunk 1.
3. **Code for v2 parts (medium-large, about 700 lines).**
   - Elbows and knees in every pose, the IK set, face controller.
   - `soft_`/`bob_` discovery, `alt_` hiding in `AssetLibrary`, `socket_wand`, costume tuning table.
4. **Ghost and cat (medium-large).**
5. **Witch, skeleton, pumpkin (large).**
6. **Tuning pass (medium).**
   - Per-costume spring and pose tuning in the gym and in game.
   - Hang-bucket wall clamp, mounted pose.
   - Technique notes written back to `S:\Repos\art-pipeline`.