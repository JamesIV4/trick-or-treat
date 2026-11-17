#pragma once
// The kid's animation rig: every costume (dragon_kid, kid_ghost, kid_cat, kid_witch, kid_skeleton,
// kid_pumpkin) is posed by this one class. DragonController keeps all physics and gameplay; each
// frame it fills a KidAnimInput and calls KidRig::update(), which
//   1. blends the pose layers (kid_poses.cpp): a locomotion set, full-body overrides, upper-body
//      overrides and additive layers, with eased weights;
//   2. adds secondary motion: lean and head-lag springs, look-at, landing crouch, a bucket that swings
//      from its grip, soft parts (tail, wings, soft_*) bent in the toon shader by a spring, rigid
//      bob_* parts on springs;
//   3. solves the limbs that have to touch something (the wand hand on a ledge lip, the feet on uneven
//      ground), writes the node transforms and shader parameters;
//   4. runs the face (kid_face.cpp): blinks, gaze, brows, mouth shapes.
// It works on the flat v1 models (body, head, arm_*, leg_*, tail, wing_*, bucket) and picks up the
// v2 parts by name when a model has them. Everything it does is visual: nothing here feeds back
// into movement. See art/team/characters_candy/batch05/CHECKPOINT.md for the part-name contract.

#include <godot_cpp/classes/geometry_instance3d.hpp>
#include <godot_cpp/classes/node3d.hpp>
#include <godot_cpp/variant/string.hpp>

#include <cstdint>
#include <vector>

#include "play/anim_kit.h"

namespace tot {

// One-shot events, latched by DragonController where they happen (mostly in _physics_process) and
// handed to the rig once, in the next update().
enum KidEvent : uint32_t {
	KID_EV_JUMP = 1u << 0, // left the ground by jumping (also a climb or vault starting)
	KID_EV_FLAP = 1u << 1, // the double jump
	KID_EV_LAND = 1u << 2, // touched down; KidAnimInput::land_impact is the downward speed
	KID_EV_POUND = 1u << 3, // ground pound started (the stall)
	KID_EV_POUND_LAND = 1u << 4, // ground pound hit the floor
	KID_EV_SPIN = 1u << 5,
	KID_EV_ROLL = 1u << 6,
	KID_EV_CAST = 1u << 7, // wand flick
	KID_EV_HURT = 1u << 8, // knocked back; KidAnimInput::hurt_dir is the way the kid is thrown (world)
	KID_EV_CELEBRATE = 1u << 9, // candy landed in the bucket; KidAnimInput::celebrate_tier
	KID_EV_GRAB = 1u << 10, // caught a ledge
	KID_EV_BOUNCE = 1u << 11, // sprung off a trampoline
	KID_EV_TELEPORT = 1u << 12, // moved without travelling: forget all motion
	KID_EV_CLIMB_DONE = 1u << 13, // stood up on top of a ledge or landed a vault
};

struct KidAnimInput {
	float dt = 0.0f;
	float time = 0.0f; // the controller's clock, seconds
	// The controller's gait phase in radians (the "stepped" signal fires each time sin(phase) changes
	// sign). The rig runs its own stride clock locked to it: see KidRig::update().
	float phase = 0.0f;
	godot::Vector3 velocity; // world, m/s
	float run_speed = 7.9f;
	float yaw = 0.0f; // the visual yaw: already eased toward the gameplay facing
	float squash = 0.0f; // the controller's squash spring, clamped: -0.25 (stretched) .. 0.3 (squashed)
	float roar = 0.0f; // extra uniform scale wobble (the cast's little puff-up)
	float step_offset = 0.0f; // visual lift easing out after a stair step
	godot::Vector3 smooth_offset; // shift of the whole visual for drawing between physics ticks (play/tick_smooth.h)
	bool on_floor = true, mounted = false, hanging = false;
	bool pounding = false, pound_stall = false; // ground pound; the stall is its first moment, hanging in the air
	bool flap_used = false, gliding = false, knocked = false;
	// Progress of timed moves, 0..1; negative = not happening.
	float spin_k = -1.0f, roll_k = -1.0f, climb_k = -1.0f, cast_k = -1.0f, flap_k = -1.0f;
	bool vault = false; // the climb is a hop over a fence
	godot::Vector3 ledge, ledge_n; // the lip being hung from and the wall's normal (world)
	uint32_t events = 0; // KidEvent bits since the last update
	float land_impact = 0.0f;
	godot::Vector3 hurt_dir;
	int celebrate_tier = 0;
	bool has_camera = false;
	godot::Vector3 camera_pos;
	// The floor under each foot's rest spot (left, right), as a height relative to the kid's origin;
	// only read when ground_ok. Lets the feet settle on steps and kerbs while standing.
	bool ground_ok = false;
	float ground[2] = { 0.0f, 0.0f };
};

// Per-costume tuning, keyed by the visual's name (the asset id). Defaults suit every costume;
// kid_tuning() in kid_rig.cpp holds the table.
struct KidTuning {
	const char *id = "";
	int costume = 0; // KidCostume: picks the costume's own idle fidget
	float arm_out = 0.0f; // extra abduction of both upper arms, radians (arms that rest inside a wide body)
	float elbow_rest = 0.0f; // v2 forearms: the elbows rest at least about this bent (radians), easing out as a pose bends them more
	bool tail_swish = false; // the cat's travelling S wave (tail_wave) on top of the bend
	bool glide_spread = false; // glide with limbs spread (the cat) instead of the parachute float
	float bucket_hz = 1.7f, bucket_zeta = 0.36f; // the bucket's swing
	float bucket_fwd = 0.36f, bucket_inertia = 0.45f; // how far forward it may swing (radians); how hard the kid's movement throws it
	float tail_hz = 1.5f, tail_zeta = 0.38f, tail_limit = 0.95f; // the tail's soft bend
	float tail_inertia = 0.42f, tail_vertical = 0.45f; // how hard movement throws the tail; how much of that for up and down (takeoff, landing)
	float tail_wag = 1.0f; // scale on the idle and gait wag
	float tail_thump = 0.0f; // the tail slaps down and bounces on a landing (0 = off)
	float wing_hz = 2.8f, wing_zeta = 0.42f, wing_limit = 0.6f; // wing tip lag
	float head_look = 1.0f; // scale on how far the head turns to look
	float legacy_knee = 0.4f; // legs without a shin: how much of a knee bend swings the whole leg back
	float legacy_elbow = 0.35f; // arms without a forearm: how much of an elbow bend swings the whole arm forward
	float sway = 1.0f; // scale on the idle weight shift and breathing
	float bounce = 1.0f; // scale on the hop between strides
	float rattle = 0.0f; // a quick shiver of every limb after a landing or a hit (the skeleton)
	float hem_wave = 0.0f; // soft_sheet / soft_skirt / soft_cape: ripple amplitude in metres (the ghost's hem)
	float hem_lobes = 4.0f;
	float float_up = 1.0f; // scale on how much falling lifts the soft and bob parts (the ghost's sheet balloons)
	float spin_flare = 0.0f; // hems (skirt, cape) flare out in a spin by this share of their balloon (the witch)
	// A heavy sheet in a stop (the ghost, final fixes 2): while the kid skids to a halt and for a moment after
	// (KidRig::stop_calm_), the hanging hem's forward swing eases into this many radians (its spring state is held
	// there too, as cloth meeting the shins), and only stop_knee of the raised knee's push applies. 0 = off.
	float stop_fwd = 0.0f;
	float stop_knee = 1.0f;
	float stop_lean = 0.0f; // ... and this share of the body's lean back (the skid's brace) is taken out of the hem's tilt
	// A plush onesie (the cat, final fixes 2): the `gloss` instance uniform on the body, arms, forearms, hands, thighs,
	// tail and a static `hood` node under the head (the face, ears, bow and bell keep theirs). 0 = off.
	float plush_gloss = 0.0f;
	// Fleece, not vinyl (the skeleton): the `gloss` instance uniform for the body, arms, forearms, hands and thighs
	// (-2: no highlight and no sky sheen); the head and soft parts get no less than -1 (no highlight, the face
	// keeps its soft sheen). 0 = off (the shader's default). The shoes are left alone.
	float matte = 0.0f;
	// Glossy vinyl (the pumpkin suit): the `gloss` instance uniform on the body and the costume's soft_* / bob_*
	// parts (a tight catchlight on the ribs, the stem and the leaves); negative = a softer, smaller highlight than the
	// shader's default (-1 none). 0 = off.
	float shine = 0.0f;
	// Felt (the witch's hat): the `gloss` instance uniform on bob_hat and soft_tip (-1: no highlight, -2: no sky sheen
	// either). 0 = off.
	float hat_gloss = 0.0f;
	// Matte hair (the witch's locks, cap and fringe): the `gloss` instance uniform on soft_hair* and the static
	// hair_cap node (-1: no highlight; below -1 the sky sheen fades too). 0 = off.
	float hair_gloss = 0.0f;
};
const KidTuning &kid_tuning(const godot::String &id);

enum KidCostume { KID_PLAIN, KID_DRAGON, KID_GHOST, KID_CAT, KID_WITCH, KID_SKELETON, KID_PUMPKIN };

// What the face should be doing this frame. KidRig::update() fills it from the state; KidFace eases
// toward it.
struct KidFaceWant {
	enum Mouth { SMILE, OPEN, O, GRIT, MOUTH_COUNT };
	int mouth = SMILE;
	float brow_up = 0.0f; // -1 pressed down .. 1 raised
	float brow_tilt = 0.0f; // -1 worried (inner ends up) .. 1 fierce (inner ends down)
	float wide = 0.0f; // 0..1 eyes opened wide
	float squint = 0.0f; // 0..1
	godot::Vector2 gaze; // where the eyes point relative to the head: x yaw (+ = the kid's left), y pitch (- = up), radians
	bool lively = false; // standing about: the eyes dart around a little
	bool blink = false; // blink now
	float blink_hold = 0.0f; // ... and stay shut this long (squeezed eyes)
	bool hold_open = false; // no blinks of its own accord (the middle of an action, or a scripted face)
};

// The v2 face parts under `head`: eye_l/r (local Y is the blink axis) with iris_l/r children,
// alt_lash_l/r (the shut eye), brow_l/r, mouth_smile and alt_mouth_open / _o / _grit. Every part is
// optional; a model with none (the v1 costumes) leaves this unbound and step() does nothing.
class KidFace {
public:
	void bind(godot::Node *visual);
	void unbind();
	bool bound() const { return any_; }
	void reset();
	void step(const KidFaceWant &want, float dt);
	bool eyes_shut() const { return closed_ > 0.6f; }

private:
	void find(godot::Node *n);

	bool any_ = false;
	godot::Node3D *eye_[2] = {}, *iris_[2] = {}, *brow_[2] = {}, *lash_[2] = {};
	godot::Node3D *mouth_[KidFaceWant::MOUTH_COUNT] = {};
	godot::Transform3D eye_rest_[2], iris_rest_[2], brow_rest_[2], mouth_rest_[KidFaceWant::MOUTH_COUNT];
	// Blink: 0 waiting, 1 closing, 2 held, 3 opening.
	int blink_phase_ = 0;
	float blink_raw_ = 0.0f, blink_t_ = 0.0f, blink_next_ = 0.95f, blink_hold_ = 0.0f, closed_ = 0.0f;
	bool blink_second_ = false, was_held_ = false;
	uint32_t resets_ = 0; // varies the blink timing from one reset to the next
	anim::Rand rng_{ 0x51d0u };
	anim::Spring wide_, squint_, brow_up_, brow_tilt_, gaze_x_, gaze_y_, pop_;
	godot::Vector2 dart_;
	float dart_next_ = 0.6f;
	int mouth_now_ = KidFaceWant::SMILE;
	float mouth_age_ = 1.0f;
};

class KidRig {
public:
	// Pose joints. A model need not have them all: FOREARM, HAND and SHIN are v2 parts, TAIL and
	// WING only some costumes have.
	enum Joint {
		J_ROOT, // the whole model (the asset's root node): tilt, hop, shake
		J_BODY, // torso; its offset is the pelvis offset and also moves the legs
		J_HEAD,
		J_ARM_L,
		J_ARM_R,
		J_FOREARM_L,
		J_FOREARM_R,
		J_HAND_L,
		J_HAND_R,
		J_LEG_L,
		J_LEG_R,
		J_SHIN_L,
		J_SHIN_R,
		J_TAIL,
		J_WING_L,
		J_WING_R,
		// Not a node: pos.x = how much the rig should stand the kid on its lowest sole (1 for poses
		// that only bend the knees and leave the height to the rig; 0 for poses that set the pelvis
		// height themselves, such as the strides).
		J_AUX,
		J_COUNT
	};
	using Pose = anim::Pose<J_COUNT>;

	// A leg's measurements, for the stride (kid_poses.cpp) and for standing on the ground.
	struct LegGeom {
		bool shin = false; // a real knee; otherwise the knee is folded into the whole leg
		float hip_y = 0.3f; // the hip joint's height at rest
		float thigh = 0.135f; // hip to knee (the whole leg's length without a shin)
		godot::Vector3 heel = godot::Vector3(0, -0.165f, -0.04f), toe = godot::Vector3(0, -0.165f, 0.13f); // sole points, in the space of the node that carries the shoe
		float legacy_knee = 0.4f;
	};

	// visual_root: the controller's yaw + squash node. visual: the asset instance under it (its name
	// is the costume id). Reparents head, arm_*, wing_* and tail under body and records the rests.
	void bind(godot::Node3D *visual_root, godot::Node3D *visual);
	void unbind();
	bool bound() const { return visual_ != nullptr; }
	// Forget all motion (teleport, costume swap): springs to rest, blend weights snapped.
	void reset();
	void update(const KidAnimInput &in);

	// Where the head should look (world). Weight 0 hands the head back to the default behaviour (it
	// leads turns, and glances at the camera when standing about). Call it every frame while it
	// applies, or once with a weight to hold it.
	void set_look_target(const godot::Vector3 &pos, float weight);
	// Raises the wand arm to the cast pose at once (the shot must leave the tip where it is drawn).
	void cast_now();
	// 1: the cat's swishing S tail (also set by the costume's tuning).
	void set_tail_style(int s) { tail_style_ = s; }

	const godot::String &id() const { return id_; }
	bool has_wings() const { return n_[J_WING_L] || n_[J_WING_R]; }
	bool has_tail() const { return n_[J_TAIL] != nullptr; }
	godot::Node3D *joint(int j) const { return n_[j]; }
	godot::Node3D *wand_socket() const { return socket_wand_; } // v2: where the wand is held, or null
	const char *debug_state() const { return debug_; }

private:
	struct Soft { // one continuous mesh bent in the shader about its origin
		godot::GeometryInstance3D *g = nullptr;
		godot::Node3D *node = nullptr;
		int joint = -1; // the pose joint that also rotates this node, or -1
		godot::Transform3D rest;
		godot::Vector3 dir; // pin-to-tip direction, local
		float len = 0.2f;
		float limit = 0.7f; // the bend eases into this (the spring's own hard limit is wider)
		float vertical = 1.0f; // how much of the parent's up and down movement throws it
		float flatten = 0.0f; // falling folds it back by this much (radians at full fall speed): ears
		float wave = 0.0f, lobes = 4.0f; // hem ripple (cloth_wave), metres
		anim::DangleParams p;
		anim::Dangle d;
		godot::Vector3 frame_origin, pin_prev; // the pin's world position with the vertical scaling applied, and where it really was
		bool frame_primed = false;
		float balloon = 0.0f; // falling puffs the whole part out by this much (metres): sheets, skirts
		float rigid = 0.0f; // soft_bend.w: how far (along the bend axis) from the pin nothing bends
		float wave_top = 0.0f; // cloth_wave.w: local height below which a hem ripples (0 = by distance)
		float arms = 0.0f; // a sheet over the shoulders: raised arms puff it out
		bool counter = false; // a hood (soft_cowl): its part below the neck is turned back against the head's pose, so it stays on the body's sheet
		int kind = 0; // 0 other, 1 tail, 2 left wing, 3 right wing
		int side = 0; // +1 a part named *_l (the kid's left), -1 *_r, 0 neither (the cat's ear flick)
		// A tail only: points along its underside (mesh space), to keep it from sinking into the
		// ground (see keep_tail_above_ground in kid_rig.cpp).
		std::vector<godot::Vector3> low_pts;
		float reach = 0.0f; // a hanging part (down): soft_pin.w, the depth over which its bend ramps in
		bool trail = false; // a cape hanging behind the body: it never swings forward into the back, trails over a roll
		float wrap = 0.11f; // a hanging hem in a roll: drawn in round the tucked knees by this much (metres)
		bool front = false; // a cord lying on the chest (the skeleton's drawstrings): it never swings back into the body
		// The shader bend as last set (soft_pin xyz / w and soft_bend.w with `shown` as soft_bend.xyz), so the ground
		// guard (KidRig::ground_guard) can follow the bent mesh.
		godot::Vector3 pin_axis, shown;
		float pin_w = 0.0f;
		bool on_head = false; // under the head (ears, the hat's tip, the stem, the tendrils, hair): folded down in a head-down roll
	};
	// Points on every mesh of the kid (about 2 cm apart, mesh space), for the ground guard in the roll: nothing (the
	// witch's hat, the cat's ears, a hood, the bucket) may go under the road while the kid tumbles (decision 18).
	struct Probe {
		godot::Node3D *node = nullptr;
		int soft = -1; // index into soft_ when the mesh is bent in the shader
		bool top = false; // the head and everything on it (the trace reports it apart: TOP)
		bool wand = false; // the wand (attached under socket_wand by the controller after bind; the trace reports it: WAND)
		std::vector<godot::Vector3> pts;
	};
	std::vector<Probe> probes_;
	float roll_lift_ = 0.0f; // how far the guard raises the tumbling kid (metres, eased out after the roll)
	float top_clear_raw_ = 9.99f, top_clear_ = 9.99f, body_clear_raw_ = 9.99f, body_clear_ = 9.99f; // trace only
	void collect_probes();
	int probe_wand_n_ = -1; // socket_wand's child count when the probes were collected (the wand comes later: collect again)
	float wand_clear_ = 9.99f; // trace only: the wand's lowest point above the floor (with the raise)
	float lowest_point(bool top_only, int *which = nullptr, bool wand_only = false, godot::Vector3 *at = nullptr) const;
	float true_clear_ = 9.99f; // trace only, rolling: the lowest point above the ground right under it (a ray), with the raise // the lowest world height of the kid's meshes (or of the head's / the wand's), bends included; *which = its probe
	int body_low_part_ = -1; // trace only: the probe holding the whole kid's lowest point
	void ground_guard(const KidAnimInput &in, float dt);
	float tail_clear_raw_ = 0.0f, tail_clear_ = 0.0f; // the tail's lowest point above the floor before and after that (trace only)
	float hem_clear_raw_ = 0.0f, hem_clear_ = 0.0f; // the same for a hanging sheet's hem (the ghost; trace only)
	struct Bob { // a rigid part swinging about its origin
		godot::Node3D *node = nullptr;
		godot::Transform3D rest;
		godot::Vector3 dir;
		float lift = 0.0f; // falling lifts it off its seat by up to this (metres): the witch's hat
		bool hat = false; // bob_hat: knocked askew and straightened in the witch's fidget, nods on a landing
		// bob_leaf_*: a leaf lying on a round surface below it (the pumpkin cap). Its swing may lift the tip off the
		// surface but not push it in: the bend's component along `away` (dir x the surface normal) stays >= about 0.
		bool surface = false;
		godot::Vector3 away;
		anim::DangleParams p;
		anim::Dangle d;
		anim::Spring up;
	};
	void find(godot::Node *n);
	void adopt(godot::Node3D *n);
	godot::Transform3D model_xf(godot::Node *n) const; // transform relative to visual_ (from the nodes' current transforms)
	void add_soft(godot::Node3D *n, int joint, int kind);
	void apply(const KidAnimInput &in, Pose &out, float grounded, float foot_ik, float hand_ik);
	void step_secondary(const KidAnimInput &in, const Pose &out);
	void step_bucket(const KidAnimInput &in, float dt);
	void step_gait(const KidAnimInput &in, float sf, float speed, float dt);
	KidFaceWant face_want(const KidAnimInput &in, float sf, bool busy, bool mugging, float dt);

	godot::Node3D *root_ = nullptr, *visual_ = nullptr;
	godot::String id_;
	KidTuning tune_;
	godot::Node3D *n_[J_COUNT] = {};
	godot::Transform3D rest_[J_COUNT];
	godot::Node3D *bucket_ = nullptr, *socket_wand_ = nullptr;
	godot::Transform3D bucket_rest_;
	godot::Basis bucket_rest_model_; // the bucket's rest orientation in model space
	bool bucket_left_ = false;
	KidFace face_;
	KidFaceWant want_;
	std::vector<Soft> soft_;
	std::vector<Bob> bob_;
	godot::Vector3 sole_[2][2]; // heel and toe of each foot, in the local space of the node that carries it
	LegGeom leg_;
	godot::Vector3 hand_tip_[2]; // the fist's far end, in the hand's space
	int tail_style_ = 0;

	// Blend state.
	anim::Weight w_ground_, w_glide_, w_roll_, w_pound_, w_stall_, w_hang_, w_climb_, w_hurt_, w_mount_, w_spin_, w_cast_, w_flap_, w_skid_, w_fidget_, w_glance_, w_mug_;
	anim::Spring sf_; // smoothed speed fraction
	// The stride clock: radians, one stride (a left and a right step) per TAU, the left foot landing at 0.
	float gait_ = 0.0f, gait_rate_ = 8.0f, phase_prev_ = 0.0f;
	// Secondary motion.
	anim::Spring lean_pitch_, lean_roll_, head_pitch_, head_roll_, yaw_kick_, pelvis_, hang_sway_;
	// Eased levels that keep the head smooth: the look into the bucket and the bucket arm's lift after
	// candy lands (pickups in a row hold them, they never restart), and the head leading a turn.
	anim::Spring cele_look_, cele_lift_, head_lead_;
	// Ears (soft parts with `flatten`): folded back by a fall or a glide, an underdamped level, so they
	// spring forward past upright (perk) when the fall ends; a landing kicks it further.
	anim::Spring ear_fold_;
	float hop_t_ = -1.0f, hop_cool_ = 0.0f; // the happy hop's own clock (never cut short) and the wait before the next one
	// The body's height over the feet with the footfall kinks rounded off (see KidRig::apply). The
	// head moves with the body; it is not steadied against it.
	anim::Spring ride_s_;
	bool ride_primed_ = false;
	anim::LookAt look_;
	anim::Dangle bucket_d_;
	anim::Weight bucket_force_;
	anim::Rand rng_{ 0x7e11u };
	godot::Vector3 vel_prev_, accel_;
	float yaw_prev_ = 0.0f, yaw_rate_ = 0.0f, speed_prev_ = 0.0f, decel_ = 0.0f;
	float stop_calm_ = 0.0f; // 1 while skidding to a halt, fading over about half a second after (KidTuning::stop_fwd)
	float ground_[2] = { 0.0f, 0.0f }; // smoothed floor heights under the feet
	bool primed_ = false;
	// Set by bind(): the first update after a costume change in play takes the state weights straight from the
	// state the kid is in (mid-air, mid-spin, hanging, riding) instead of easing in from standing. A teleport in
	// the same frame clears it (a teleport is a fresh start on the ground, as before).
	bool bind_snap_ = false;
	// One-shot timers: seconds since the event, negative = idle.
	float land_t_ = -1.0f, land_s_ = 0.0f, launch_t_ = -1.0f, cele_t_ = -1.0f, hurt_t_ = -1.0f, cast_t_ = -1.0f, rattle_t_ = -1.0f;
	int bucket_forced_ = 0; // 1 flung outward (spin), 2 rigid with the arm (roll, mounted)
	int cele_tier_ = 0, jump_parity_ = 0;
	godot::Vector3 hurt_local_;
	float bucket_punch_ = 0.0f;
	float idle_t_ = 0.0f, fidget_t_ = -1.0f, fidget_next_ = 3.0f;
	float run_t_ = 0.0f, air_t_ = 0.0f, mug_t_ = -1.0f;
	int fidget_kind_ = 0, fidget_n_ = 0;
	bool was_spinning_ = false, was_rolling_ = false;
	godot::Vector3 look_pos_;
	float look_w_ = 0.0f;
	godot::Vector2 gaze_; // the eyes' share of the look, for the face
	bool blink_req_ = false; // the face should blink now ...
	float blink_hold_req_ = 0.0f; // ... and hold the eyes shut this long
	float hand_ik_ = 0.0f; // how much the wand hand is held on the ledge lip
	Pose last_; // the last pose written (cast_now() edits the wand arm in it)
	const char *debug_ = "";
};

// Pose library (kid_poses.cpp). Every function fills a pose relative to the rest pose; the ones
// marked additive are meant for PoseBlender::additive.
struct KidPoseCtx {
	float time = 0.0f; // seconds
	float gait = 0.0f; // stride clock, radians: the left foot lands at 0, the right at PI
	float gait_rate = 8.0f; // radians per second
	float speed = 0.0f; // ground speed, m/s (smoothed)
	float sf = 0.0f; // speed / run speed, 0..1.3
	float bounce = 1.0f; // scale on the hop between strides
	bool wings = false, tail = false, swish = false; // what the costume has
	bool spread = false; // glide with limbs spread
	int costume = KID_PLAIN;
	int parity = 0; // flips left and right in the jump poses from one jump to the next
	const KidRig::LegGeom *leg = nullptr;
};
namespace kid_pose {
using Pose = KidRig::Pose;
void idle(Pose &p, const KidPoseCtx &c);
void walk(Pose &p, const KidPoseCtx &c);
void run(Pose &p, const KidPoseCtx &c);
void rise(Pose &p, const KidPoseCtx &c);
void apex(Pose &p, const KidPoseCtx &c);
void fall(Pose &p, const KidPoseCtx &c);
void glide(Pose &p, const KidPoseCtx &c);
void roll(Pose &p, const KidPoseCtx &c, float k); // k: 0..1 through the roll
// 0..1: how far the tumbling kid's head points down at roll progress k (1 = straight down, 0 for the first and last
// part of the roll). The roll tucks the head in by it and KidRig squashes the head-top parts with it.
float roll_head_down(float k);
void pound(Pose &p, const KidPoseCtx &c, bool stall);
void hang(Pose &p, const KidPoseCtx &c, float sway);
void climb(Pose &p, const KidPoseCtx &c, float k, bool vault);
void hurt(Pose &p, const KidPoseCtx &c, float t, const godot::Vector3 &away_local); // t: seconds since the hit
void mounted(Pose &p, const KidPoseCtx &c);
void spin(Pose &p, const KidPoseCtx &c, float k);
void cast(Pose &p, const KidPoseCtx &c, float t); // t: seconds since the flick; upper body
void flap(Pose &p, const KidPoseCtx &c, float k); // the double jump
void breath(Pose &p, const KidPoseCtx &c, float amount); // additive
void launch(Pose &p, const KidPoseCtx &c, float t); // additive: the push-off just after a jump
void land(Pose &p, const KidPoseCtx &c, float t, float strength); // additive: landing crouch
void celebrate(Pose &p, const KidPoseCtx &c, float look, float lift, float hop_t, int tier); // additive
constexpr float CELEBRATE_HOP_LENGTH = 0.62f;
void skid(Pose &p, const KidPoseCtx &c); // additive: braking from a run
void rattle(Pose &p, const KidPoseCtx &c, float t, float amount); // additive: a shiver of every limb
constexpr float RATTLE_LENGTH = 0.6f; // seconds a rattle lasts (it dies away over this)
float skeleton_shimmy(float t); // the skeleton's fidget: 0..1 how hard the shimmy shakes, `t` seconds in (KidRig swings the cords with it)
void mug(Pose &p, const KidPoseCtx &c, float t); // additive: playing to a camera that has come close
void fidget(Pose &p, const KidPoseCtx &c, int kind, float t); // additive; see fidget_length
float fidget_length(int kind);
float fidget_length(int kind, int costume); // the costume's own fidget (kind 4) may run longer
float witch_hat_askew(float t); // the witch's fidget: 0..1 how far the hat has slipped, `t` seconds in (KidRig tips the hat)
float pumpkin_wobble(float t); // the pumpkin's fidget: the head's side-to-side wobble, -1..1, `t` seconds in (KidRig whips the stem with it)
constexpr int FIDGET_KINDS = 5;
constexpr float CAST_LENGTH = 0.8f; // seconds the flick's follow-through lasts
constexpr float MUG_LENGTH = 5.5f; // the camera act loops after this long
// How far the hang raises the model above the controller's hang point and draws it in to the wall:
// the kid hangs with the chin over the lip (the arms are far too short for a dead hang).
extern const godot::Vector3 HANG_LIFT;
// The wand arm at the instant of the cast (also what cast() starts from).
extern const godot::Vector3 CAST_ARM;
} // namespace kid_pose

} // namespace tot
