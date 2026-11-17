#pragma once
// Monster animation: one MonsterAnimator per live monster turns its gameplay state (AnimInput, filled
// by Monster every rendered frame) into part poses on its MonsterRig and the transform of its visual
// root. Animation is visual only: nothing here may move the body, change a timing or read input.
//
// Which animator a monster gets (make_animator):
//   rig_version 1 (procedural model, or a glTF with just body / part_a / part_b) -> LegacyAnimator,
//       the original whole-body squash and two swinging parts, exactly as before the rig existed.
//   rig_version 2 (a glTF with more named parts) -> the animator registered for that MonsterType,
//       or LegacyAnimator (with a warning) when none is registered yet.
//
// Writing an animator: put it in its own file, native/src/play/monster_anim_<key>.cpp (the build
// globs src/*/*.cpp, so there is nothing to register in the build):
//
//   #include "play/monster_anim.h"
//   #include "play/monsters.h"                       // only if you read in.tuning
//   namespace tot { namespace {
//   class SkeletonAnimator : public MonsterAnimator {
//       int chest_ = -1, head_ = -1;  MonsterFace face_;  HitRecoil hit_;  Gait gait_;
//       void bind(MonsterRig &rig) override {            // once, after the rig is built
//           chest_ = rig.slot("chest");  head_ = rig.slot("head");  face_.bind(rig);
//       }
//       void update(const AnimInput &in, MonsterRig &rig, godot::Transform3D &root) override {
//           ... rig.pose(chest_, ...);                   // every slot may be -1: calls are no-ops
//           root = root_pose(in.yaw, lean, scale, offset);
//       }
//   };
//   } TOT_MONSTER_ANIMATOR(MonsterType::Skeleton, SkeletonAnimator) }
//
// Conventions (anim_kit.h): metres, seconds, radians; Y up, +Z the monster's front, +X its left.
// "local" vectors in AnimInput are in the facing frame: the monster's origin, turned by in.yaw.

#include <godot_cpp/variant/transform3d.hpp>
#include <godot_cpp/variant/vector2.hpp>
#include <godot_cpp/variant/vector3.hpp>

#include <cstdint>
#include <functional>

#include "play/anim_kit.h"
#include "play/monster_art.h"
#include "play/monster_rig.h"

namespace tot {

struct MonsterTuning;

// Mirrors Monster::State, in the same order.
enum class MonsterAnimState { Rise, Roam, Windup, Attack, Recover, Hurt, Dying };

// Today's timings; an animator may stretch either up to MONSTER_ANIM_TIME_MAX.
constexpr float MONSTER_RISE_TIME = 0.55f, MONSTER_DEATH_TIME = 0.28f, MONSTER_ANIM_TIME_MAX = 0.6f;
// Beyond this distance from the camera a monster is "far" (AnimInput::lod = 1).
constexpr float MONSTER_LOD_DISTANCE = 35.0f;

struct AnimInput {
	MonsterType type = MonsterType::PumpkinHopper;
	const MonsterTuning *tuning = nullptr; // read-only: attack kind, wind-up time, flying, hops ... (play/monsters.h)

	// ---- state
	MonsterAnimState state = MonsterAnimState::Rise;
	MonsterAnimState prev_state = MonsterAnimState::Rise; // the state before this one
	// True on the first update() after the state changed, on the very first update, and when the same
	// state restarts (hit again while already hurt).
	bool entered = false;
	float state_t = 0.0f; // seconds spent in `state` (the gameplay clock: it advances on physics ticks)
	// How long `state` lasts when that is fixed: Rise = rise_time(), Dying = death_time(), Windup =
	// tuning.windup, Hurt = 0.28. 0 for the open-ended ones (Roam, Attack, Recover).
	float state_len = 0.0f;
	float windup = 0.0f; // 0..1 through the wind-up; 0 in every other state
	float cooldown = 0.0f; // seconds until it may attack again (counts down through Recover and Roam)
	float dizzy = 0.0f; // seconds of dizziness left after a charge (0 = not dizzy; only CHARGE monsters)
	float since_hit = 1000.0f; // seconds since it last took a hit (1000 = never)

	// ---- clocks
	float dt = 0.0f; // seconds since the last update() (the frame time; three frames' worth when lod = 1)
	float t = 0.0f; // this monster's own animation clock, seconds since it spawned

	// ---- motion (facing frame unless noted)
	float yaw = 0.0f; // where it faces, radians about +Y (world): the root's turn
	float speed = 0.0f; // horizontal speed, m/s
	godot::Vector3 vel_local; // velocity; y is the vertical speed (hops, pounces, falls)
	godot::Vector3 accel_local; // smoothed acceleration, m/s^2 (starts, stops, knock-back)
	godot::Vector3 player_local; // the player's feet relative to the monster's origin
	godot::Vector3 attack_dir_local; // unit, horizontal: where the current wind-up / attack is aimed
	godot::Vector3 hit_dir_local; // unit, horizontal: the way the last hit pushed it (away from the blow)
	// Hoppers (tuning.hops): seconds until the next roaming hop may start; it leaves the ground as
	// soon as this reaches 0 while it is on the floor and has somewhere to go. 0 for everything else.
	float hop_in = 0.0f;
	bool on_floor = false; // CharacterBody3D::is_on_floor(); always false for flyers
	bool chasing = false; // the player is inside its aggro range
	// ---- world (for foot planting and the like)
	godot::Vector3 origin_world; // the monster's origin (its feet; a flyer's body centre)
	const std::function<float(float, float)> *ground = nullptr; // terrain height at world (x, z); may be null
	// The walkable surface under world (x, z) near height y: the terrain AND what is built on it (kerbs,
	// sidewalks, porch steps), which `ground` does not know about. A physics ray per call: cache it. May be null.
	const std::function<float(float, float, float)> *surface = nullptr;

	// ---- model
	float height = 1.0f, radius = 0.4f; // the collision capsule the art has to fit
	uint32_t seed = 0; // per monster: seed Blink, RandTimer and noise with it so a crowd is out of step
	int lod = 0; // 0 near; 1 far: skip springs, blinks and look-at (the shared helpers do this themselves)
};

class MonsterAnimator {
public:
	virtual ~MonsterAnimator() = default;
	// Once, right after the rig is built: look up slots, mark soft parts (rig.soft), set gloss.
	virtual void bind(MonsterRig &rig) {}
	// Every rendered frame (every third when in.lod = 1 and distance_lod() is true). Pose the parts
	// and write the visual root's transform to `root` (it comes in holding the last one). The root
	// carries the facing: build it with root_pose(in.yaw, ...).
	virtual void update(const AnimInput &in, MonsterRig &rig, godot::Transform3D &root) = 0;
	// How long the spawn and the death play before the monster starts roaming / is removed.
	// Monster clamps both to 0.1 .. MONSTER_ANIM_TIME_MAX and reads them once, after bind().
	virtual float rise_time() const { return MONSTER_RISE_TIME; }
	virtual float death_time() const { return MONSTER_DEATH_TIME; }
	// Where a thrown projectile leaves the monster (hand, mouth), world space. Called on the physics
	// tick the attack starts. Return false to keep the default: 0.9 of the height above the origin.
	virtual bool muzzle(const MonsterRig &rig, godot::Vector3 &out_world) const { return false; }
	// Strength of the whole-body orange telegraph flash during the wind-up (1 = as always). An
	// animator that flares the eyes instead may halve it.
	virtual float telegraph_gain() const { return 1.0f; }
	// Strength of the whole-body flash when it takes a hit (1 = as always; the fade time never
	// changes). A pale monster that would turn into a white blob may lower it so its recoil reads.
	virtual float hit_flash_gain() const { return 1.0f; }
	// THROW monsters: the part the projectile should look like (the bone in the skeleton's hand).
	// Monster hands that part's mesh to Combat once, so what flies is what was held. -1 = keep
	// Combat's own projectile mesh. Visual only: the projectile's path and hit test do not change.
	virtual int thrown_part(const MonsterRig &rig) const { return -1; }
	// False: update() runs every frame at any distance.
	virtual bool distance_lod() const { return true; }
};

// ---- registry ------------------------------------------------------------------------------------
using MonsterAnimatorFactory = MonsterAnimator *(*)();
// Registers the animator for a monster's v2 rig. Use the macro, at namespace tot scope, in the
// animator's own .cpp.
bool register_monster_animator(MonsterType type, MonsterAnimatorFactory make);
#define TOT_MONSTER_ANIMATOR(m_type, m_class) \
	static const bool _tot_monster_animator_##m_class = ::tot::register_monster_animator(m_type, []() -> ::tot::MonsterAnimator * { return new m_class(); });
// A new animator for one monster (delete it with `delete`). Never null.
MonsterAnimator *make_animator(MonsterType type, int rig_version);

// ---- shared helpers ------------------------------------------------------------------------------

// The visual root's transform: facing `yaw`, leaning by the rotation vector `lean` (axis * radians
// in the facing frame), squashed by `scale` along the upright axes (not along the lean, so a squash
// always flattens toward the ground), lifted by `offset`.
//   lean.x > 0 tips the top toward +Z (forward), lean.z > 0 tips it toward -X (its right),
//   lean.y turns it (a spin on top of the facing). HitRecoil returns a lean.
godot::Transform3D root_pose(float yaw, const godot::Vector3 &lean = godot::Vector3(), const godot::Vector3 &scale = godot::Vector3(1, 1, 1),
		const godot::Vector3 &offset = godot::Vector3());

// Eyes and head: blinks, pupils that slide toward the player, a head turn with lag, the eye glow
// flare of a wind-up, eyes squeezed shut on a hit.
//
// Model contract: an eye is a part whose origin is the eye's centre, local +Z out of the face and +Y
// up (give it a rest rotation to sit it on a curved head). Its pupil is a child part of the eye. A
// blink scales the eye's Y to `closed_scale`, which closes the pupil with it. Extra eyes (the
// spider's eye_s0..5, the mummy's squinting second eye) only blink, in a ripple.
struct MonsterFace {
	static constexpr int MAX_EYES = 8;
	// ---- tuning (set after bind(), before the first update)
	float max_yaw = 0.61f, max_pitch = 0.35f; // head turn limits (35 and 20 degrees)
	float head_lag = 0.15f; // seconds the head trails the player
	float pupil_travel = 0.3f; // of the eye's radius (its reach)
	float pupil_lag = 0.05f; // half life, seconds
	float closed_scale = 0.08f; // eye Y scale when shut
	float look_height = 0.7f; // metres above the player's feet the eyes aim at
	float glow_rest = -1.0f; // glow_boost on the eyes at rest; negative = leave the eyes' glow alone
	float glow_flare = 1.6f; // ... and at the end of a wind-up
	float blink_speed = 1.0f; // > 1: quicker blinks (a lidless eye squashed flat should not stay a line for long)
	float wake_hold = 0.0f; // seconds after the Rise state the eyes hold open (no blink right after waking)
	// Seconds a drop of `shut` from 1 to 0 takes at least (it may close at once, it never snaps open: shut chevrons or
	// closed lines swap back to the eye at closed_scale, which then opens over about 5 frames). 0 = follow `shut` exactly.
	float open_time = 0.085f;
	// ---- per-frame controls (set before update(); they persist until changed)
	float shut = 0.0f; // 0..1 forced closing on top of the blinks (asleep, bracing, puffing)
	float wide = 1.0f; // eye scale (1.25 = startled)
	float look_weight = -1.0f; // 0..1 how much the head and pupils follow the player; negative = follow while chasing
	// ---- results
	godot::Vector2 head; // (yaw, pitch) for the head: compose with anim::look_euler(face.head)
	float closed = 0.0f; // 0 open .. 1 shut, after blinks, hits and `shut`

	// Finds head, eye_l / eye_r (or a single "eye") with pupil_l / pupil_r (or "pupil"), and any
	// eye_s0 .. eye_s5. Missing parts are skipped. Seed it with in.seed-like variety.
	void bind(MonsterRig &rig, uint32_t seed = 0);
	// Adds another eye (and its pupil, or "" for none) to the set. `follows` = its pupil tracks.
	void add_eye(MonsterRig &rig, const char *eye, const char *pupil = "", bool follows = true);
	// Poses the eyes and pupils and updates `head` and `closed`. Call after posing the head's
	// ancestors (the look is measured from where they are now); pose the head yourself afterwards.
	void update(const AnimInput &in, MonsterRig &rig);
	void blink_now(float hold = 0.0f) { blink_.force(hold); }

private:
	int head_ = -1, n_ = 0;
	int eye_[MAX_EYES], pupil_[MAX_EYES];
	bool follows_[MAX_EYES];
	godot::Vector2 slide_[MAX_EYES]; // each pupil's smoothed offset, in units of its travel
	anim::Blink blink_;
	anim::LookAt look_;
	float last_since_hit_ = 1000.0f, glow_ = -1.0f, glow_sent_ = -1000.0f, ripple_t_ = 10.0f, shut_eased_ = 0.0f;
	uint32_t seed_ = 0;
	bool seeded_ = false, was_blinking_ = false;
};

// A flinch: the body tips away from a blow and springs back. Returns a rotation vector in the facing
// frame; feed it to root_pose's lean (or turn it into a chest rotation with anim::quat_from_rotvec).
struct HitRecoil {
	float angle = 0.44f; // peak tip, radians (25 degrees)
	float hz = 3.5f, zeta = 0.35f; // the spring back
	bool fresh = false; // true on the update a new hit arrived: kick limb springs, hop the skull
	float ouch = 0.0f; // 1 at the hit, easing to 0 over about 0.35 s: squeeze eyes, dim a face
	godot::Vector3 update(const AnimInput &in);

private:
	anim::Spring3 lean_; // world space
	float last_since_hit_ = 1000.0f;
};

// Walk cycle driven by distance travelled, so feet do not slide: one full cycle (a left and a right
// step) per `stride` metres. Holds still in the air.
struct Gait {
	float phase = 0.0f; // 0..1
	float amount = 0.0f; // 0 standing .. 1 at or above `full_speed`, smoothed: scale limb swings by it
	float step(const AnimInput &in, float stride, float full_speed = 1.0f);
	float swing() const { return std::sin(phase * anim::TAU); } // -1..1, one cycle per stride
	float bounce() const { return std::fabs(std::sin(phase * anim::TAU)); } // 0..1, twice per stride (a footfall each)
};

// Squash and stretch for the root, volume preserving and springy: follows `target` (1 = rest, 0.7 a
// deep crouch, 1.15 a stretch) with overshoot, and dips by itself on a landing in proportion to the
// fall speed. Returns the scale for root_pose.
struct RootSquash {
	float hz = 5.0f, zeta = 0.4f;
	float land_gain = 0.035f; // squash per m/s of landing speed
	float min = 0.45f, max = 1.6f;
	bool landed = false; // true on the update it touched down
	godot::Vector3 update(const AnimInput &in, float target = 1.0f);
	void kick(float dv) { s_.kick(dv); } // negative squashes
	float value() const { return 1.0f + s_.x; }

private:
	anim::Spring s_;
	bool was_floor_ = true, primed_ = false;
	float fall_ = 0.0f;
};

} // namespace tot
