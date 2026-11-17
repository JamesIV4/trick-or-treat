// The Mummy (a GRAB monster): the animator for its v2 rig.
//
// Part tree (art/team/monsters/batch04/build_monsters2.py, mummy()):
//   body (hips) > chest > head > eye > pupil, eye_s0 > pupil_s0 (the squinting eye), knot_tail_a / b (soft)
//                       > arm_l > forearm_l > hand_l, forearm_l > wrist_strip (soft)     (+X side)
//                       > arm_r > forearm_r > hand_r                                     (-X side)
//               > leg_l > shin_l > foot_l,  leg_r > shin_r > foot_r
//               > waist_tail (soft), alt_unwind_a / b (hidden: the wraps that fly off when it dies)
// The arms are held out in front at rest (zombie arms) and the legs hang straight down; both are
// posed with two-bone IK: the hands in the chest's space, the feet planted in model space.
//
// How a frame is built: a locomotion pose (a stiff-legged shamble driven by distance travelled: the
// hips rock over the planted leg, the body vaults over it, the head lolls the other way), then state
// layers mixed over it by weight (wind-up, grab, flinch, fidgets) and the wobble of the recover, then
// springs (the hands trail the body, the head lolls, the body tips with every start and stop, the
// bandage ends dangle), then the solve. The hips are set as high as the stiff legs allow, which is
// what gives the walk its bob. Spawning is a plank-stiff sit-up from flat on its back; dying is an
// unwinding spin: the body thins while two streamers that were hidden until then whirl outward.
//
// The grab: Monster starts the lunge (3.6 m/s for 0.35 s) on the physics tick the wind-up ends. The
// wind-up pulls the arms wide and back and arches the body away; the Attack state claps them
// together in front in the first tenth of a second and throws the chest forward.

#include "play/monster_anim.h"

#include <godot_cpp/core/math.hpp>

#include <algorithm>
#include <cmath>

#include "play/monsters.h"

using namespace godot;

namespace tot {
namespace {

using anim::lerp;
using anim::PI;
using anim::remap01;
using anim::saturate;
using anim::smoothstep01;
using anim::TAU;

constexpr int L = 0, R = 1; // left (+X) and right (-X)
constexpr float SIDE[2] = { 1.0f, -1.0f };

// ---- tuning --------------------------------------------------------------------------------------
constexpr float STRIDE_SLOW = 0.5f, STRIDE_FAST = 0.74f; // metres per walk cycle (two steps), wandering and chasing
constexpr float STEP_H = 0.05f; // how high a swinging foot lifts: a shuffle
constexpr float STANCE = 0.012f; // feet this much wider than the hips
constexpr float TOE_OUT = 0.22f; // radians
constexpr float LEG_STRAIGHT = 0.985f; // the hips ride this fraction of the leg's length above the planted ankle: stiff knees
constexpr float CHASE_SPEED = 1.3f; // tuning.speed
constexpr float ARM_REACH = 0.318f; // just short of the arm's 0.325 m: a stiff arm with a hint of elbow
constexpr float PALMS_IN = 1.45f; // hand roll that turns the palms to face each other (and forward when the arms are wide)
constexpr float RISE_TIME = 0.6f, DEATH_TIME = 0.6f;
constexpr float SPIN_OUT = 0.07f, SPIN_HOLD = 0.1f, SPIN_BACK = 0.2f; // the head's quarter spin on a hit, seconds

// A limb of three nested parts solved with anim::two_bone_ik.
struct Limb {
	int upper = -1, mid = -1, end = -1;
	Vector3 root, a, b; // rest origins: upper in its parent's space, mid in upper's, end in mid's

	void bind(MonsterRig &rig, const char *u, const char *m, const char *e) {
		upper = rig.slot(u);
		mid = rig.slot(m);
		end = rig.slot(e);
		root = rig.rest(upper).origin;
		a = rig.rest(mid).origin;
		b = rig.rest(end).origin;
	}
	float length() const { return a.length() + b.length(); }
	// Poses upper and mid so the end joint reaches `target`, bending toward `pole` (both in the space
	// the upper joint hangs in). Returns the mid joint's basis in that space, to orient the end part.
	Basis solve(MonsterRig &rig, const Vector3 &hinge, const Vector3 &target, const Vector3 &pole, float max_flex) const {
		const anim::Ik2 ik = anim::two_bone_ik(root, a, b, hinge, target, pole, max_flex);
		rig.pose(upper, Basis(ik.upper));
		rig.pose(mid, Basis(ik.lower));
		return Basis(ik.upper) * Basis(ik.lower);
	}
};

// Everything the solve needs for one frame. Layers are mixed into it field by field.
struct Pose {
	Vector3 pelvis_off; // metres, model space, on top of the rest position (y is added to the height the legs allow)
	Vector3 pelvis_rot, chest_rot, head_rot; // Euler (pitch, yaw, roll); head_rot goes on top of the look
	Vector3 hand[2], pole[2]; // wrist targets and elbow poles, chest space
	float hand_pitch[2] = { 0.0f, 0.0f }, hand_roll[2] = { 0.0f, 0.0f }; // the mitten on its wrist; roll is signed per side by the solve
	Vector3 foot[2]; // ankle targets, model space
	float foot_pitch[2] = { 0.0f, 0.0f }, foot_yaw[2] = { 0.0f, 0.0f };
	float wide = 1.0f; // eye scale
	float squash = 1.0f; // root squash and stretch target
};

void mix(Pose &d, const Pose &s, float w) {
	if (w <= 0.0f) {
		return;
	}
	w = saturate(w);
	d.pelvis_off = d.pelvis_off.lerp(s.pelvis_off, w);
	d.pelvis_rot = d.pelvis_rot.lerp(s.pelvis_rot, w);
	d.chest_rot = d.chest_rot.lerp(s.chest_rot, w);
	d.head_rot = d.head_rot.lerp(s.head_rot, w);
	for (int i = 0; i < 2; ++i) {
		d.hand[i] = d.hand[i].lerp(s.hand[i], w);
		d.pole[i] = d.pole[i].lerp(s.pole[i], w);
		d.hand_pitch[i] = lerp(d.hand_pitch[i], s.hand_pitch[i], w);
		d.hand_roll[i] = lerp(d.hand_roll[i], s.hand_roll[i], w);
		d.foot[i] = d.foot[i].lerp(s.foot[i], w);
		d.foot_pitch[i] = lerp(d.foot_pitch[i], s.foot_pitch[i], w);
		d.foot_yaw[i] = lerp(d.foot_yaw[i], s.foot_yaw[i], w);
	}
	d.wide = lerp(d.wide, s.wide, w);
	d.squash = lerp(d.squash, s.squash, w);
}

class MummyAnimator : public MonsterAnimator {
public:
	float rise_time() const override { return RISE_TIME; }
	float death_time() const override { return DEATH_TIME; }
	float telegraph_gain() const override { return 1.15f; } // (final fixes 2) linear body_flash: at least ~0.9 of BEFORE's pulse (flash_metric.py)
	float hit_flash_gain() const override { return 0.9f; } // (final fixes 2) the fade after the white peak (linear body_flash)

	void bind(MonsterRig &rig) override {
		body_ = rig.slot("body");
		chest_ = rig.slot("chest");
		head_ = rig.slot("head");
		eye_ = rig.slot("eye");
		pupil_ = rig.slot("pupil");
		squint_ = rig.slot("eye_s0");
		squint_pupil_ = rig.slot("pupil_s0");
		unwind_[0] = rig.slot("alt_unwind_a");
		unwind_[1] = rig.slot("alt_unwind_b");
		arm_[L].bind(rig, "arm_l", "forearm_l", "hand_l");
		arm_[R].bind(rig, "arm_r", "forearm_r", "hand_r");
		leg_[L].bind(rig, "leg_l", "shin_l", "foot_l");
		leg_[R].bind(rig, "leg_r", "shin_r", "foot_r");
		for (int s = 0; s < 2; ++s) {
			ankle_rest_[s] = rig.model_rest(leg_[s].end).origin; // every part is at rest here
			hip_rest_[s] = rig.model_rest(leg_[s].upper).origin;
		}
		face_.bind(rig);
		face_.max_yaw = 0.75f;
		face_.max_pitch = 0.4f;
		face_.pupil_travel = 0.14f; // a big iris on a shallow dome: further and it slides off the eye
		face_.glow_rest = GLOW_REST;
		face_.glow_flare = GLOW_FLARE;
		face_.look_height = 0.8f;
		// (final fixes) the lidless eye squashed to a line read as a flat orange disc for 10 frames: a quick blink
		// (about 9 frames, 3 of them nearly shut) that closes all the way into the dark hollow
		face_.blink_speed = 1.8f;
		face_.closed_scale = 0.02f;
		// A faint sheen on the linen, a wet catchlight on the eyes, none on the flat iris discs or the
		// loose ends. The rim light is turned down from the monsters' usual 0.35: on pale cloth it
		// washes the wraps out and lights up the dark hollow the eyes sit in.
		for (int i = 0; i < rig.count(); ++i) {
			rig.gloss(i, 0.08f);
		}
		rig.param_all("rim_strength", 0.14f);
		// The eyes sit in a dark hollow; no glassy sweep on the dark dome (with 0.9 it read as a camera
		// lens, FIDELITY_PASS decision 14). The authored glints stay as the catchlights.
		rig.gloss(eye_, -1.0f);
		rig.gloss(squint_, -1.0f);
		rig.gloss(pupil_, 0.0f);
		rig.gloss(squint_pupil_, 0.0f);
		rig.glow(squint_pupil_, GLOW_REST);
		static const char *soft_names[SOFT_N] = { "knot_tail_a", "knot_tail_b", "waist_tail", "wrist_strip" };
		static const float soft_hz[SOFT_N] = { 1.25f, 1.5f, 1.3f, 1.7f };
		for (int i = 0; i < SOFT_N; ++i) {
			Soft &s = soft_[i];
			s.slot = rig.slot(soft_names[i]);
			s.dir = rig.tip(s.slot).length_squared() > 1e-8f ? rig.tip(s.slot).normalized() : Vector3(0, -1, 0);
			s.len = Math::max(rig.reach(s.slot), 0.05f);
			s.hz = soft_hz[i];
			rig.soft(s.slot);
			rig.gloss(s.slot, 0.0f);
		}
		for (int u : unwind_) {
			rig.gloss(u, 0.0f);
		}
	}

	void update(const AnimInput &in, MonsterRig &rig, Transform3D &root) override {
		using State = MonsterAnimState;
		const float dt = anim::safe_dt(in.dt);
		const bool far = in.lod > 0;
		if (!seeded_) {
			seeded_ = true;
			fidget_ = anim::RandTimer(in.seed ^ 0xF1D6u);
			mirror_ = ((in.seed >> 3) & 1u) != 0;
			spin_dir_ = mirror_ ? 1.0f : -1.0f;
			phase0_ = anim::hash01(in.seed) * TAU;
		}
		const bool dying = in.state == State::Dying, rising = in.state == State::Rise;
		const bool alive = !dying && !rising;

		// ---- clocks and one-shots
		AnimInput walk = in;
		if (!alive) {
			walk.speed = 0.0f; // a dead body keeps its last velocity; the legs must not keep walking
		}
		const float run = saturate(walk.speed / CHASE_SPEED);
		const float last_phase = gait_.phase;
		gait_.step(walk, lerp(STRIDE_SLOW, STRIDE_FAST, run), 0.35f);
		const float amt = gait_.amount;
		const Vector3 recoil = hit_.update(in); // sets hit_.fresh and hit_.ouch
		if (hit_.fresh) {
			spin_dir_ = -spin_dir_; // the head spins the other way each time
		}
		const bool landed = in.on_floor && !was_floor_;
		if (!in.on_floor) {
			fall_ = in.vel_local.y;
		}
		was_floor_ = in.on_floor;
		const bool footfall = amt > 0.2f && int(std::floor(last_phase * 2.0f)) != int(std::floor(gait_.phase * 2.0f));

		// Idle fidgets when it stands about with nobody to chase.
		const bool idle = in.state == State::Roam && !in.chasing && in.speed < 0.15f && in.on_floor;
		fidget_t_ += dt;
		if (!idle) {
			fidget_t_ = Math::max(fidget_t_, 10.0f); // drop a fidget the moment it has something to do
			fidget_.reset();
			idle_t_ = 0.0f;
		} else if ((idle_t_ += dt) > 5.0f && fidget_t_ > fidget_len() + 0.6f && fidget_.step(dt, 3.0f, 6.5f)) {
			fidget_kind_ = (fidget_.count + 2) % 3; // a look round first, then a scratch, then the wrist strip
			fidget_t_ = 0.0f;
		}

		// ---- the pose: locomotion, then the state layers
		w_chase_.step(alive && (in.chasing || in.state == State::Windup || in.state == State::Attack), 0.3f, 0.7f, dt);
		w_wind_.step(in.state == State::Windup, 0.12f, 0.12f, dt);
		w_attack_.step(in.state == State::Attack, 0.0f, 0.3f, dt);
		w_air_.step(alive && !in.on_floor, 0.06f, 0.1f, dt);
		Pose p = locomotion(in, amt, run);
		if (in.state == State::Windup) {
			wind_ = in.windup; // kept, so the layer can fade from where it stopped (a hit mid wind-up)
		}
		if (w_wind_.w > 0.0f) {
			mix(p, windup(in, wind_), w_wind_.w);
		}
		if (in.state == State::Attack) {
			attack_t_ = in.state_t;
		}
		if (w_attack_.w > 0.0f) {
			mix(p, grab(attack_t_), w_attack_.w);
		}
		if (in.state == State::Recover) {
			recover(in, p);
		}
		fidgets(in, p);
		// A flinch: the arms are thrown up and wide, the chest arches away from the blow.
		const float ouch = alive ? hit_.ouch : 0.0f;
		if (ouch > 0.0f) {
			Pose h = p;
			const float lop = spin_dir_;
			for (int s = 0; s < 2; ++s) {
				h.hand[s] = sweep(s, 62.0f + 14.0f * lop * SIDE[s], 0.3f, 0.2f + 0.06f * lop * SIDE[s]);
				h.pole[s] = arm_[s].root + Vector3(SIDE[s] * 0.3f, -0.4f, -0.2f);
				h.hand_pitch[s] = -0.5f;
				h.hand_roll[s] = 0.4f;
			}
			h.chest_rot += Vector3(-0.26f, 0.0f, 0.1f * lop);
			h.pelvis_rot.x -= 0.08f;
			h.wide = 1.0f;
			mix(p, h, ouch);
		}
		shut_ = 0.0f;
		if (dying) {
			die_pose(in, p);
		}
		if (rising) {
			rise_pose(in, p);
		}

		// ---- springs
		// The head lolls: a loose spring on the neck, tipped by every footfall, thrown by starts, stops,
		// hits and landings. The body tips the same way, much less and stiffer (a toy that almost falls).
		if (far) {
			loll_.reset();
			tip_.reset();
			dip_.reset();
		} else {
			if (footfall) {
				const float side = gait_.phase < 0.5f ? 1.0f : -1.0f;
				loll_.kick(Vector3(0.9f, 0.0f, 1.3f * side) * amt * (0.6f + 0.4f * run));
				for (int s = 0; s < 2; ++s) {
					hand_s_[s].kick(Vector3(0.0f, -0.22f, 0.0f) * amt);
				}
			}
			loll_.kick(Vector3(-in.accel_local.z, 0.0f, in.accel_local.x) * (1.1f * dt));
			tip_.kick(Vector3(-in.accel_local.z, 0.0f, in.accel_local.x) * (0.22f * dt));
			if (hit_.fresh) {
				loll_.kick(Vector3(in.hit_dir_local.z, 0.0f, -in.hit_dir_local.x) * 4.0f);
				for (int s = 0; s < 2; ++s) {
					hand_s_[s].kick(Vector3(SIDE[s] * 0.6f, 1.6f, 0.0f));
				}
			}
			if (landed) {
				loll_.kick(Vector3(3.2f, 0.0f, 0.6f * spin_dir_));
				dip_.kick(Math::clamp(fall_, -8.0f, 0.0f) * 0.3f);
				for (int s = 0; s < 2; ++s) {
					hand_s_[s].kick(Vector3(0.0f, -1.2f, 0.0f));
				}
			}
			loll_.step(Vector3(), 2.3f, 0.24f, dt);
			if (loll_.x.length() > 0.5f) {
				loll_.x = loll_.x.normalized() * 0.5f;
			}
			tip_.step(Vector3(), 2.6f, 0.3f, dt);
			if (tip_.x.length() > 0.16f) {
				tip_.x = tip_.x.normalized() * 0.16f;
			}
			dip_.step(0.0f, 4.0f, 0.4f, dt);
		}
		// The head's quarter spin on a hit: whips round, holds while the eyes are squeezed shut, snaps
		// back past the front and settles.
		float head_spin = 0.0f;
		if (alive && in.since_hit < SPIN_OUT + SPIN_HOLD + SPIN_BACK) {
			const float t = in.since_hit;
			float e;
			if (t < SPIN_OUT) {
				e = anim::ease_out(t / SPIN_OUT);
			} else if (t < SPIN_OUT + SPIN_HOLD) {
				e = 1.0f + 0.06f * std::sin((t - SPIN_OUT) / SPIN_HOLD * PI);
			} else {
				e = 1.0f - anim::ease_out_back((t - SPIN_OUT - SPIN_HOLD) / SPIN_BACK, 2.6f);
			}
			head_spin = spin_dir_ * (PI * 0.5f) * e;
		}

		// ---- solve: hips, chest, head
		// The hips sit as high as the stiff legs allow over the feet (the body vaults over the planted
		// leg and drops as the feet spread), then any crouch the pose asks for.
		const Basis pelvis_basis = Basis::from_euler(p.pelvis_rot);
		const Vector3 body_rest = rig.rest(body_).origin;
		float hip_y = 1.0e6f;
		for (int s = 0; s < 2; ++s) {
			const Vector3 hip = body_rest + Vector3(p.pelvis_off.x, 0.0f, p.pelvis_off.z) + pelvis_basis.xform(leg_[s].root);
			const Vector3 d = p.foot[s] - hip;
			const float len = leg_[s].length() * LEG_STRAIGHT;
			hip_y = Math::min(hip_y, d.y + std::sqrt(Math::max(len * len - d.x * d.x - d.z * d.z, 0.0025f)));
		}
		hip_y = Math::clamp(hip_y, -0.16f, 0.05f);
		ride_ = far || !ride_primed_ ? hip_y : anim::damp(ride_, hip_y, 0.018f, dt);
		ride_primed_ = true;
		const float air = w_air_.w;
		const Vector3 pelvis_off(p.pelvis_off.x, lerp(ride_, 0.0f, air) + p.pelvis_off.y + Math::clamp(dip_.x, -0.08f, 0.03f), p.pelvis_off.z);
		rig.pose(body_, pelvis_basis, pelvis_off);
		const Basis chest_basis = Basis(anim::quat_from_rotvec(alive ? recoil * 0.5f : Vector3())) * Basis::from_euler(p.chest_rot);
		rig.pose(chest_, chest_basis);

		const bool grabbing = in.state == State::Windup || in.state == State::Attack;
		face_.look_weight = grabbing ? 1.0f : (look_away_ ? 0.0f : -1.0f);
		face_.head_lag = anim::damp(face_.head_lag, grabbing ? 0.05f : 0.16f, 0.08f, dt);
		face_.wide = p.wide;
		face_.shut = shut_;
		face_.update(in, rig);
		rig.pose(head_, Basis(Vector3(0, 1, 0), head_spin) * Basis::from_euler(anim::look_euler(face_.head)) * Basis::from_euler(p.head_rot) *
								Basis(anim::quat_from_rotvec(loll_.x)));
		// The squinting eye follows the big one: its pupil slides the same way (less), its glow flares with it.
		if (squint_pupil_ >= 0 && pupil_ >= 0 && rig.node(pupil_)) {
			const Vector3 slide = rig.node(pupil_)->get_position() - rig.rest(pupil_).origin;
			const float k = rig.reach(eye_) > 1e-4f ? 0.7f * rig.reach(squint_) / rig.reach(eye_) : 0.0f;
			rig.pose(squint_pupil_, Basis(), Vector3(slide.x * k, slide.y * k * 0.4f, 0.0f));
			float want = GLOW_REST;
			if (in.state == State::Windup) {
				want = lerp(GLOW_REST, GLOW_FLARE, smoothstep01(in.windup));
			} else if (in.state == State::Attack) {
				want = GLOW_FLARE;
			}
			squint_glow_ = far ? want : anim::damp(squint_glow_, want, 0.05f, dt);
			if (std::fabs(squint_glow_ - squint_glow_sent_) > 0.01f) {
				squint_glow_sent_ = squint_glow_;
				rig.glow(squint_pupil_, squint_glow_);
			}
		}

		// ---- arms (chest space). The hands trail the body: each wrist target is chased by a spring in
		// model space, so every bob, lurch and lunge of the body reaches the hands late and overshoots.
		const Transform3D chest_model = rig.model_xf(chest_);
		const Transform3D to_chest = chest_model.affine_inverse();
		const float snap = Math::max(w_attack_.w, Math::max(w_wind_.w * 0.5f, ouch * 0.6f));
		const Vector3 elbow_hinge(-1, 0, 0); // folds the forearm upward from the arm held out in front
		for (int s = 0; s < 2; ++s) {
			const Vector3 want = chest_model.xform(p.hand[s]);
			if (far || !hands_primed_ || !alive) {
				hand_s_[s].reset(want);
			} else {
				hand_s_[s].step(want, lerp(3.4f, 9.0f, snap), lerp(0.42f, 0.6f, snap), dt);
				const Vector3 off = hand_s_[s].x - want;
				if (off.length() > 0.16f) {
					hand_s_[s].x = want + off.normalized() * 0.16f;
				}
			}
			const Basis fore = arm_[s].solve(rig, elbow_hinge, to_chest.xform(hand_s_[s].x), p.pole[s], 2.2f);
			(void)fore;
			rig.pose(arm_[s].end, Basis(Vector3(0, 0, 1), -SIDE[s] * p.hand_roll[s]) * Basis(Vector3(1, 0, 0), p.hand_pitch[s]));
		}
		hands_primed_ = true;

		// ---- legs (feet are planted in model space; the legs hang from the posed hips)
		const Transform3D body_model = rig.model_xf(body_);
		const Transform3D to_body = body_model.affine_inverse();
		const Vector3 knee_hinge(1, 0, 0); // folds the shin back
		for (int s = 0; s < 2; ++s) {
			Vector3 foot = p.foot[s];
			float pitch = p.foot_pitch[s];
			const Vector3 hip = body_model.xform(leg_[s].root);
			if (air > 0.0f) { // off the ground: the stiff legs stick out and paddle
				const Vector3 hang = hip + Vector3(SIDE[s] * (0.03f + 0.09f * ouch), -0.285f, 0.05f * std::sin(in.t * 13.0f + float(s) * PI) + 0.07f * ouch * SIDE[s] * spin_dir_);
				foot = foot.lerp(hang, air);
				pitch = lerp(pitch, 0.35f, air);
			}
			const Vector3 pole = hip + Vector3(SIDE[s] * 0.25f, -0.1f, 0.6f); // knees forward and a little out
			const Basis shin = body_model.basis * leg_[s].solve(rig, knee_hinge, to_body.xform(foot), to_body.xform(pole), 2.4f);
			const Basis want = Basis(Vector3(0, 1, 0), p.foot_yaw[s]) * Basis(Vector3(1, 0, 0), pitch);
			rig.pose(leg_[s].end, shin.inverse() * want);
		}

		// ---- spawn and death: the root's own moves, and the streamers that fly off
		float yaw = in.yaw;
		Vector3 lean = alive ? recoil * 0.5f + tip_.x : Vector3();
		Vector3 scale = squash_.update(in, alive ? p.squash : 1.0f);
		Vector3 offset;
		float spin_rate = 0.0f; // radians per second, for the bandage ends
		bool streamers = false;
		if (rising) {
			const float k = saturate(in.state_t / RISE_TIME);
			// Flat on its back for a beat, then up like a plank about its heels: a damped swing that
			// carries it well past upright, back, and to rest.
			const float u = std::pow(remap01(k, 0.2f, 1.0f), 1.15f);
			lean = Vector3(-(PI * 0.5f) * std::exp(-3.4f * u) * std::cos(6.6f * u) * (1.0f - smoothstep01(remap01(u, 0.8f, 1.0f))), 0.0f, 0.0f);
			offset.y = 0.17f * saturate(-std::sin(lean.x)); // lying: its back is on the ground, not in it
			// Monsters are spawned a little above the ground and drop. A plank lying in mid-air reads as
			// a mistake, so until it lands the picture is held down on the ground (the body is not moved).
			float drop = 0.0f;
			if (!in.on_floor && in.ground && *in.ground) {
				drop = Math::clamp(in.origin_world.y - (*in.ground)(in.origin_world.x, in.origin_world.z), 0.0f, 0.45f);
			}
			spawn_drop_ = in.on_floor ? anim::damp(spawn_drop_, 0.0f, 0.03f, dt) : drop;
			offset.y -= spawn_drop_;
			const float grow = lerp(0.3f, 1.0f, anim::ease_out(saturate(k / 0.1f)));
			scale = Vector3(grow, grow, grow);
		} else if (dying) {
			const float k = saturate(in.state_t / DEATH_TIME);
			const float turn = spin_angle(k);
			spin_rate = (turn - spin_angle(Math::max(k - 0.02f, 0.0f))) / (0.02f * DEATH_TIME);
			const float dir = mirror_ ? -1.0f : 1.0f;
			yaw += dir * turn;
			// The body thins and sinks as the wraps come off; it winks out at the very end.
			const float thin = 1.0f - 0.66f * smoothstep01(remap01(k, 0.2f, 0.9f));
			const float tall = (1.0f + 0.1f * std::sin(PI * remap01(k, 0.1f, 0.5f))) * lerp(1.0f, 0.28f, anim::ease_in(remap01(k, 0.5f, 1.0f)));
			const float gone = 1.0f - 0.9f * smoothstep01(remap01(k, 0.92f, 1.0f));
			scale = Vector3(thin, tall, thin) * gone;
			// The streamers: they stay behind in the world while the body spins inside them, and
			// widen and drop as they unwind.
			streamers = k > 0.1f;
			const float open = anim::ease_out(remap01(k, 0.1f, 0.85f));
			for (int i = 0; i < 2; ++i) {
				const float wide = lerp(0.85f, i == 0 ? 2.1f : 1.7f, open) / thin;
				const float high = lerp(1.0f, i == 0 ? 0.42f : 0.55f, smoothstep01(remap01(k, 0.3f, 1.0f))) / tall;
				rig.pose(unwind_[i], Basis(Vector3(0, 1, 0), -dir * turn * (i == 0 ? 0.82f : 0.68f)), Vector3(), Vector3(wide, high, wide));
			}
		}
		for (int u : unwind_) {
			rig.show(u, streamers);
		}

		// ---- the loose bandage ends: one soft mesh each, hanging by gravity, thrown about by whatever
		// they are tied to and flung outward by the death spin
		if (far) {
			for (Soft &s : soft_) {
				s.dangle.reset();
				rig.bend(s.slot, Vector3());
			}
		} else {
			const Vector3 world_hit = Basis(Vector3(0, 1, 0), in.yaw).xform(in.hit_dir_local);
			for (Soft &s : soft_) {
				if (s.slot < 0) {
					continue;
				}
				const Transform3D frame = rig.world_rest(s.slot);
				if (hit_.fresh) {
					s.dangle.kick(world_hit * 2.5f + Vector3(0, 1.8f, 0));
				}
				Vector3 accel;
				if (spin_rate != 0.0f) {
					Vector3 out = frame.origin - in.origin_world;
					out.y = 0.0f;
					accel = out * (spin_rate * spin_rate) + Vector3(0, 6.0f, 0);
				}
				anim::DangleParams dp;
				dp.len = s.len;
				dp.hz = s.hz;
				dp.zeta = 0.27f;
				dp.gravity = 26.0f;
				dp.drag = 0.3f;
				dp.limit = 1.45f;
				rig.bend(s.slot, s.dangle.step(frame, s.dir, dp, dt, accel));
			}
		}

		root = root_pose(yaw, lean, scale, offset);
	}

private:
	static constexpr int SOFT_N = 4;
	static constexpr float GLOW_REST = 0.14f, GLOW_FLARE = 0.75f; // more than this and the bloom eats the pupil
	struct Soft {
		int slot = -1;
		Vector3 dir;
		float len = 0.2f, hz = 1.5f;
		anim::Dangle dangle;
	};

	float fidget_len() const { return fidget_kind_ == 0 ? 2.2f : (fidget_kind_ == 1 ? 1.7f : 1.9f); }

	// How far the body has turned k of the way through the death: a short wind back the other way,
	// then nearly two turns, accelerating (faster would strobe at 60 frames a second).
	static float spin_angle(float k) {
		return -0.6f * anim::pulse(k, 0.14f, 0.1f) + TAU * 1.75f * std::pow(remap01(k, 0.16f, 1.0f), 1.6f);
	}

	// A wrist target on a circle round the shoulder, in chest space: azimuth 0 = straight ahead, 90 =
	// out to that arm's own side, more = behind it, negative = across the body; `reach` from the
	// shoulder; `height` above it.
	Vector3 sweep(int s, float az_deg, float reach, float height) const {
		const float a = Math::deg_to_rad(az_deg);
		const float flat = std::sqrt(Math::max(reach * reach - height * height, 0.0001f));
		return arm_[s].root + Vector3(SIDE[s] * flat * std::sin(a), height, flat * std::cos(a));
	}

	// Standing and shambling. The cycle is driven by distance, so the feet do not slide.
	Pose locomotion(const AnimInput &in, float amt, float run) const {
		Pose p;
		const float ph = gait_.phase * TAU;
		const float sn = std::sin(ph), cs = std::cos(ph);
		const float stride = lerp(STRIDE_SLOW, STRIDE_FAST, run);
		const float chase = w_chase_.w;
		const float still = 1.0f - amt;
		const float breath = std::sin(in.t * 1.9f + phase0_);
		const float sway = std::sin(in.t * 0.8f + phase0_ * 1.7f) * still;
		// The hips rock over the planted leg (the left one through the first half of the cycle), the
		// swinging side hitched up and brought round; the chest counters a little; the head lolls back.
		p.pelvis_off = Vector3(0.028f * amt * sn + 0.014f * sway, 0.003f * breath * still, 0.0f);
		p.pelvis_rot = Vector3(0.03f * amt + 0.05f * run, -0.2f * amt * cs, -0.105f * amt * sn - 0.035f * sway);
		p.chest_rot = Vector3(0.03f + 0.05f * amt + 0.09f * run + 0.06f * chase + 0.014f * breath, 0.1f * amt * cs, 0.035f * amt * sn + 0.02f * sway);
		p.head_rot = Vector3(-0.04f * amt - 0.05f * chase, -0.04f * amt * cs, 0.17f * amt * sn + 0.07f * still * anim::fidget(in.t * 0.17f, in.seed + 11u));
		for (int s = 0; s < 2; ++s) {
			float f = gait_.phase + (s == L ? 0.0f : 0.5f);
			f -= std::floor(f);
			float z, lift = 0.0f, pitch, out = 0.0f;
			if (f < 0.5f) { // planted, sliding back under the body; flat-footed, the heel a touch early
				z = stride * (0.25f - f);
				pitch = -0.18f * (1.0f - saturate(f / 0.07f)) + 0.25f * smoothstep01(remap01(f, 0.4f, 0.5f));
			} else { // swung forward stiffly, round the outside
				const float u = (f - 0.5f) * 2.0f;
				z = stride * (-0.25f + 0.5f * smoothstep01(u));
				lift = STEP_H * std::sin(PI * u);
				out = 0.035f * std::sin(PI * u);
				pitch = u < 0.4f ? 0.25f * (1.0f - u / 0.4f) : -0.18f * smoothstep01(remap01(u, 0.5f, 1.0f));
			}
			p.foot[s] = ankle_rest_[s] + Vector3(SIDE[s] * (STANCE + out * amt), lift * amt, z * amt);
			p.foot_pitch[s] = pitch * amt;
			p.foot_yaw[s] = SIDE[s] * TOE_OUT;
		}
		// Arms out in front: drooping and limp while it wanders, straight out and grabbing at the air
		// once it has seen someone. They bob against each other with the steps.
		for (int s = 0; s < 2; ++s) {
			const float bob = SIDE[s] * sn * amt;
			const Vector3 droop = sweep(s, 8.0f, 0.305f, -0.07f + 0.028f * bob + 0.012f * still * std::sin(in.t * 1.3f + phase0_ + float(s) * 1.9f));
			const Vector3 out = sweep(s, 4.0f, ARM_REACH, -0.012f + 0.03f * bob + 0.012f * std::sin(in.t * 7.0f + float(s) * PI));
			p.hand[s] = droop.lerp(out, chase);
			p.pole[s] = arm_[s].root + Vector3(SIDE[s] * 0.28f, -0.5f, 0.1f);
			p.hand_pitch[s] = lerp(0.5f + 0.1f * bob, -0.12f + 0.3f * std::sin(in.t * 9.0f + float(s) * PI), chase);
			p.hand_roll[s] = lerp(0.25f, 0.5f, chase);
		}
		p.wide = 1.0f + 0.06f * chase;
		return p;
	}

	// The wind-up: the arms are pulled wide and back with the palms turned to the front, the body
	// sinks and arches away, trembling; in the last fifth it rises onto its toes, cocked.
	Pose windup(const AnimInput &in, float wind) const {
		Pose p;
		const float c = anim::ease_out_back(remap01(wind, 0.0f, 0.5f), 1.4f);
		const float e = smoothstep01(remap01(wind, 0.78f, 1.0f));
		const float tremble = saturate(c) * std::sin(in.t * 41.0f) * (0.5f + 0.5f * wind);
		p.pelvis_off = Vector3(0.0f, -0.045f * c + 0.03f * e, -0.06f * c - 0.02f * e);
		p.pelvis_rot = Vector3(-0.14f * c - 0.04f * e, 0.0f, 0.012f * tremble);
		p.chest_rot = Vector3(-0.36f * c - 0.1f * e, 0.0f, 0.016f * tremble);
		p.head_rot = Vector3(0.3f * c, 0.0f, 0.03f * tremble);
		for (int s = 0; s < 2; ++s) {
			p.hand[s] = sweep(s, lerp(6.0f, 118.0f, c) + 8.0f * e, 0.31f, lerp(-0.02f, 0.13f, c) + 0.05f * e) + Vector3(0.0f, 0.006f * tremble, 0.0f);
			p.pole[s] = arm_[s].root + Vector3(SIDE[s] * 0.2f, -0.35f, -0.5f);
			p.hand_pitch[s] = -0.35f * c;
			p.hand_roll[s] = PALMS_IN * saturate(c);
			p.foot[s] = ankle_rest_[s] + Vector3(SIDE[s] * (STANCE + 0.03f * c), 0.0f, (s == L ? 0.05f : -0.07f) * c);
			p.foot_yaw[s] = SIDE[s] * (TOE_OUT + 0.15f * c);
			p.foot_pitch[s] = 0.0f;
		}
		p.wide = 1.0f + 0.3f * saturate(c);
		p.squash = 1.0f - 0.07f * c + 0.11f * e;
		return p;
	}

	// The grab (the 0.35 s lunge and the fade after it): the arms swing round and clap together in
	// front, overshooting; the chest is thrown forward; the back foot drags.
	Pose grab(float t) const {
		Pose p;
		const float f = anim::ease_out_back(saturate(t / 0.11f), 2.4f); // the clap, with overshoot
		const float g = anim::ease_out(saturate(t / 0.16f)); // the body's throw
		const float hop = anim::arc(saturate(t / 0.35f));
		p.pelvis_off = Vector3(0.0f, -0.02f * g, lerp(-0.07f, 0.07f, g));
		p.pelvis_rot = Vector3(lerp(-0.18f, 0.16f, g), 0.0f, 0.0f);
		p.chest_rot = Vector3(lerp(-0.46f, 0.5f, anim::ease_out_back(saturate(t / 0.17f), 1.9f)), 0.0f, 0.0f);
		p.head_rot = Vector3(lerp(0.3f, -0.34f, g), 0.0f, 0.0f);
		for (int s = 0; s < 2; ++s) {
			p.hand[s] = sweep(s, lerp(126.0f, -33.0f, f), lerp(0.31f, ARM_REACH, saturate(f)), lerp(0.18f, -0.01f, saturate(f)));
			p.pole[s] = arm_[s].root + Vector3(SIDE[s] * 0.35f, -0.3f, lerp(-0.5f, 0.0f, g));
			p.hand_pitch[s] = lerp(-0.35f, 0.1f, g);
			p.hand_roll[s] = PALMS_IN;
			p.foot_yaw[s] = SIDE[s] * TOE_OUT;
		}
		p.foot[L] = ankle_rest_[L] + Vector3(STANCE + 0.02f, 0.05f * hop, lerp(0.05f, 0.17f, g));
		p.foot[R] = ankle_rest_[R] + Vector3(-STANCE - 0.02f, 0.02f * hop, lerp(-0.07f, -0.19f, g));
		p.foot_pitch[L] = -0.25f * hop;
		p.foot_pitch[R] = 0.55f * g;
		p.wide = lerp(1.3f, 1.12f, g);
		p.squash = lerp(1.04f, 1.09f, hop);
		return p;
	}

	// Recover: over-balanced. As the grab's pose fades it rocks back, then forward, less each time,
	// windmilling its arms; the legs keep shuffling under it.
	void recover(const AnimInput &in, Pose &p) const {
		const float t = in.state_t;
		if (t > 1.9f) {
			return;
		}
		const float env = (1.0f - w_attack_.w) * std::exp(-t / 0.55f) * (1.0f - smoothstep01(remap01(t, 1.4f, 1.9f)));
		const float rock = env * std::cos(TAU * 1.45f * t);
		const float roll = env * std::sin(TAU * 1.45f * t + 0.8f);
		p.chest_rot += Vector3(0.5f * rock, 0.0f, 0.1f * roll);
		p.pelvis_rot += Vector3(0.22f * rock, 0.0f, 0.06f * roll);
		p.pelvis_off.z += 0.05f * rock;
		p.head_rot += Vector3(-0.3f * rock, 0.0f, -0.12f * roll);
		for (int s = 0; s < 2; ++s) {
			// Each arm circles at the shoulder, the two half a turn apart.
			const float a = TAU * 2.3f * t + float(s) * PI;
			const Vector3 mill = sweep(s, 40.0f + 30.0f * std::cos(a), 0.31f, 0.02f + 0.2f * std::sin(a));
			p.hand[s] = p.hand[s].lerp(mill, saturate(env * 1.8f));
			p.hand_pitch[s] = lerp(p.hand_pitch[s], -0.3f, saturate(env * 1.8f));
		}
		p.wide = lerp(p.wide, 1.22f, saturate(env * 2.0f));
	}

	// Standing about: a slow look to each side, a scratch of the head, a shake of the loose wrist strip.
	void fidgets(const AnimInput &in, Pose &p) {
		look_away_ = false;
		if (fidget_t_ >= fidget_len()) {
			return;
		}
		const float u = fidget_t_ / fidget_len();
		if (fidget_kind_ == 0) {
			const float env = anim::smooth_pulse(fidget_t_, 0.3f, fidget_len() - 0.7f, 0.4f);
			p.head_rot += Vector3(0.04f, 0.6f * std::sin(TAU * u), 0.16f * std::sin(TAU * u)) * env;
			p.chest_rot.y += 0.16f * std::sin(TAU * u) * env;
			look_away_ = true;
		} else if (fidget_kind_ == 1) { // the right mitten comes up and rubs the side of the head
			const float env = anim::smooth_pulse(fidget_t_, 0.35f, fidget_len() - 0.75f, 0.4f);
			p.hand[R] = p.hand[R].lerp(sweep(R, 38.0f, 0.24f, 0.2f + 0.02f * std::sin(in.t * TAU * 4.0f)), env);
			p.pole[R] = p.pole[R].lerp(arm_[R].root + Vector3(-0.5f, -0.2f, 0.0f), env);
			p.hand_pitch[R] = lerp(p.hand_pitch[R], -0.9f, env);
			p.head_rot += Vector3(0.0f, -0.12f, -0.2f) * env;
			look_away_ = true;
		} else { // lifts the left arm, looks at the strip hanging off its wrist and shakes it
			const float env = anim::smooth_pulse(fidget_t_, 0.3f, fidget_len() - 0.7f, 0.4f);
			const float shake = std::sin(in.t * TAU * 5.5f) * anim::smooth_pulse(fidget_t_ - 0.7f, 0.1f, 0.5f, 0.2f);
			p.hand[L] = p.hand[L].lerp(sweep(L, -8.0f, 0.29f, 0.07f + 0.03f * shake), env);
			p.hand_pitch[L] = lerp(p.hand_pitch[L], 0.2f + 0.3f * shake, env);
			p.head_rot += Vector3(0.14f, 0.28f, 0.1f) * env;
			look_away_ = true;
		}
	}

	// Dying: a start (arms thrown up, eye wide), then the spin flings the arms out and the head back.
	void die_pose(const AnimInput &in, Pose &p) {
		const float k = saturate(in.state_t / DEATH_TIME);
		const float a = smoothstep01(k / 0.16f);
		const float fling = smoothstep01(remap01(k, 0.16f, 0.5f));
		Pose d = p;
		d.pelvis_off = Vector3(0.0f, -0.03f * a, 0.0f);
		d.pelvis_rot = Vector3();
		d.chest_rot = Vector3(-0.16f * a, 0.0f, 0.0f);
		d.head_rot = Vector3(-0.25f * a - 0.2f * fling, 0.0f, 0.2f * fling);
		for (int s = 0; s < 2; ++s) {
			d.hand[s] = sweep(s, lerp(45.0f, 96.0f, fling), 0.31f, lerp(0.22f, 0.07f, fling));
			d.pole[s] = arm_[s].root + Vector3(SIDE[s] * 0.2f, -0.5f, -0.2f);
			d.hand_pitch[s] = -0.4f;
			d.hand_roll[s] = 0.3f;
			d.foot[s] = ankle_rest_[s] + Vector3(SIDE[s] * (STANCE - 0.03f * fling), 0.0f, 0.0f);
			d.foot_pitch[s] = 0.0f;
			d.foot_yaw[s] = SIDE[s] * TOE_OUT;
		}
		d.wide = 1.35f;
		d.squash = 1.0f;
		mix(p, d, a);
		shut_ = smoothstep01(remap01(k, 0.55f, 0.75f));
	}

	// Spawning: stiff as a board while it swings up (arms straight out, which is straight up while it
	// lies there), eyes shut until it is nearly upright, then they pop open.
	void rise_pose(const AnimInput &in, Pose &p) {
		const float k = saturate(in.state_t / RISE_TIME);
		const float stiff = 1.0f - smoothstep01(remap01(k, 0.7f, 1.0f));
		Pose r = p;
		r.pelvis_off = Vector3();
		r.pelvis_rot = Vector3();
		r.chest_rot = Vector3();
		r.head_rot = Vector3(0.12f * (1.0f - smoothstep01(remap01(k, 0.45f, 0.7f))), 0.0f, 0.0f);
		for (int s = 0; s < 2; ++s) {
			r.hand[s] = sweep(s, 5.0f, ARM_REACH, 0.0f);
			r.pole[s] = arm_[s].root + Vector3(SIDE[s] * 0.28f, -0.5f, 0.1f);
			r.hand_pitch[s] = 0.1f;
			r.hand_roll[s] = 0.2f;
			r.foot[s] = ankle_rest_[s] + Vector3(SIDE[s] * STANCE, 0.0f, 0.0f);
			r.foot_pitch[s] = 0.0f;
			r.foot_yaw[s] = SIDE[s] * TOE_OUT * 0.4f;
		}
		r.wide = 1.0f + 0.35f * anim::pulse(k - 0.5f, 0.1f, 0.4f);
		r.squash = 1.0f;
		mix(p, r, stiff);
		shut_ = 1.0f - smoothstep01(remap01(k, 0.5f, 0.6f));
	}

	int body_ = -1, chest_ = -1, head_ = -1, eye_ = -1, pupil_ = -1, squint_ = -1, squint_pupil_ = -1;
	int unwind_[2] = { -1, -1 };
	Limb arm_[2], leg_[2];
	Vector3 ankle_rest_[2], hip_rest_[2];
	MonsterFace face_;
	HitRecoil hit_;
	Gait gait_;
	RootSquash squash_;
	anim::Weight w_chase_, w_wind_, w_attack_, w_air_;
	anim::Spring3 loll_; // the head's wobble: a rotation vector (pitch, yaw, roll)
	anim::Spring3 tip_; // the whole body's: it tips back as it starts, forward as it stops
	anim::Spring3 hand_s_[2]; // the wrists, model space
	anim::Spring dip_; // landing: the hips sink and come back
	Soft soft_[SOFT_N];
	anim::RandTimer fidget_;
	float wind_ = 0.0f, attack_t_ = 0.0f, fidget_t_ = 10.0f, idle_t_ = 0.0f;
	float fall_ = 0.0f, phase0_ = 0.0f, ride_ = 0.0f, shut_ = 0.0f, spin_dir_ = 1.0f;
	float squint_glow_ = GLOW_REST, squint_glow_sent_ = -1000.0f, spawn_drop_ = 0.0f;
	int fidget_kind_ = 0;
	bool was_floor_ = true, seeded_ = false, mirror_ = false, look_away_ = false, hands_primed_ = false, ride_primed_ = false;
};

} // namespace

TOT_MONSTER_ANIMATOR(MonsterType::Mummy, MummyAnimator)

} // namespace tot
