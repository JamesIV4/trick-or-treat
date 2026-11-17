// The Scarecrow (a CHARGE monster): the animator for its v2 rig.
//
// Part tree (art/team/monsters/batch04/build_monsters2.py, scarecrow()):
//   pole > tuft (the straw tied round its foot)
//        > body (the shirt) > skirt, collar (straw)
//                           > arm_l > cuff_l (+X side),  arm_r > cuff_r (-X side)
//                           > head > hair (straw), hat > hat_tip (the whole cone: one soft mesh)
//                                  > eye_l / eye_r (a dim glow disc) > pupil_l / pupil_r (the button)
//                                  > brow_l, brow_r, mouth, alt_mouth_o, alt_mouth_zig
// It has no legs: it pogo-hops on the pole. The pole's pivot is its foot, so the pole is posed as an
// inverted pendulum (foot planted while it is on the ground, swung forward in the air) and the body
// rides on top: it slides down the pole when it lands, lags the pole's tilt, and everything above it
// lags in turn (head, hat, hat tip).
//
// How a frame is built: the hop cycle (driven by time at a period that follows the speed, so the
// planted foot moves back under the body at exactly the body's speed), then a pose (idle and
// travelling) with state layers mixed over it by weight (wind-up, charge, dizzy, flinch, fidgets),
// then springs: the root's lean chases the pose's lean with overshoot (the stagger after a charge
// comes from that), the body chases the pole, the head bobbles, the arms flop on their shoulders,
// the hat rocks, and every straw part and the hat's cone is a soft mesh on a Dangle with a
// cloth_wave ripple running round it.
//
// The charge: Monster holds still for the 1.0 s wind-up (leaning back, the hat's tip coiling up,
// the button eyes' glow flaring), then runs at 6 m/s for up to 1.1 s; the animator leans it 25
// degrees into the run with its arms and straw streaming back. Afterwards it is dizzy for as long
// as Monster says (AnimInput::dizzy): the body goes round in a cone, the head leans the other way,
// the buttons spin, the hat slips down over one eye and is shaken back at the end.

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
constexpr float CHASE_SPEED = 1.4f; // tuning.speed
constexpr float CONTACT = 0.42f; // the part of a hop cycle the foot is on the ground
constexpr float HOP_SLOW = 0.56f, HOP_FAST = 0.43f, HOP_CHARGE = 0.27f; // seconds per hop: wandering, chasing, charging
constexpr float HOP_H_SLOW = 0.085f, HOP_H_FAST = 0.15f, HOP_H_CHARGE = 0.075f; // how high the foot leaves the ground
constexpr float PLANT_MAX = 0.19f, PLANT_CHARGE = 0.12f; // furthest the planted foot may be from under the body (it skids past that)
constexpr float SLIDE = 0.05f; // how far the body slides down the pole on a landing
constexpr float CHARGE_LEAN = 0.44f; // 25 degrees
constexpr float RISE_TIME = 0.6f, DEATH_TIME = 0.6f;
constexpr float BURIED = 1.46f; // how far under the ground it starts (the hat's tip just shows)
constexpr float HALO_REST = 0.3f, HALO_FLARE = 1.1f; // glow_boost of the discs behind the buttons
constexpr float THREAD_REST = 0.2f, THREAD_FLARE = 0.7f; // ... and of the buttons' thread
// The hat as the model builds it: tilted back and to its left, the cone curling over to its right
// and back (build_monsters2.py: HX and BEND). In model axes.
const Basis HAT_TILT = Basis(Vector3(1, 0, 0), Math::deg_to_rad(-16.0f)) * Basis(Vector3(0, 0, 1), Math::deg_to_rad(-7.0f));
const Vector3 HAT_UP = HAT_TILT.xform(Vector3(0, 1, 0));
const Vector3 HAT_CURL = HAT_UP.cross(HAT_TILT.xform(Vector3(-0.906f, 0.0f, -0.423f))).normalized(); // turning about this curls the tip tighter

// Everything one frame asks of the rig, before the springs. Layers are mixed into it field by field.
struct Pose {
	Vector3 lean; // the root: a rotation vector in the facing frame (x > 0 tips the top forward)
	float slide = 0.0f; // the body along the pole, metres (negative = sunk onto it)
	float squash = 1.0f; // root squash and stretch
	float pole = 0.0f; // extra tilt of the pole about X (its foot kicked back or forward)
	Vector3 body; // extra lean of the body, a rotation vector in model space
	Vector3 head; // Euler (pitch, yaw, roll), on top of the look
	Vector3 hat; // Euler, on the head
	Vector3 hat_off; // metres, head space
	float arm_back[2] = { 0.0f, 0.0f }, arm_up[2] = { 0.0f, 0.0f }; // radians: swept back, raised
	float frown = 0.0f; // brows: 1 = angry (inner ends down), -1 = worried
	float brow_up = 0.0f; // metres
	float wide = 1.0f; // eye scale
	float coil = 0.0f; // the hat's tip: radians curled tighter (negative: straightened)
	float ripple = 0.004f, ripple_rate = 2.2f; // straw: amplitude (m) and how fast the wave runs round (rad/s)
	float glow = 0.0f; // 0 rest .. 1 flare
};

void mix(Pose &d, const Pose &s, float w) {
	if (w <= 0.0f) {
		return;
	}
	w = saturate(w);
	d.lean = d.lean.lerp(s.lean, w);
	d.slide = lerp(d.slide, s.slide, w);
	d.squash = lerp(d.squash, s.squash, w);
	d.pole = lerp(d.pole, s.pole, w);
	d.body = d.body.lerp(s.body, w);
	d.head = d.head.lerp(s.head, w);
	d.hat = d.hat.lerp(s.hat, w);
	d.hat_off = d.hat_off.lerp(s.hat_off, w);
	for (int i = 0; i < 2; ++i) {
		d.arm_back[i] = lerp(d.arm_back[i], s.arm_back[i], w);
		d.arm_up[i] = lerp(d.arm_up[i], s.arm_up[i], w);
	}
	d.frown = lerp(d.frown, s.frown, w);
	d.brow_up = lerp(d.brow_up, s.brow_up, w);
	d.wide = lerp(d.wide, s.wide, w);
	d.coil = lerp(d.coil, s.coil, w);
	d.ripple = lerp(d.ripple, s.ripple, w);
	d.ripple_rate = lerp(d.ripple_rate, s.ripple_rate, w);
	d.glow = lerp(d.glow, s.glow, w);
}

class ScarecrowAnimator : public MonsterAnimator {
public:
	float rise_time() const override { return RISE_TIME; }
	float death_time() const override { return DEATH_TIME; }
	float telegraph_gain() const override { return 1.8f; } // (final fixes 2) linear body_flash: at least ~0.9 of BEFORE's pulse (flash_metric.py)
	float hit_flash_gain() const override { return 2.0f; } // (final fixes 2) the fade after the white peak (linear body_flash)

	void bind(MonsterRig &rig) override {
		pole_ = rig.slot("pole");
		tuft_ = rig.slot("tuft");
		body_ = rig.slot("body");
		head_ = rig.slot("head");
		hat_ = rig.slot("hat");
		hat_tip_ = rig.slot("hat_tip");
		mouth_ = rig.slot("mouth");
		mouth_o_ = rig.slot("alt_mouth_o");
		mouth_zig_ = rig.slot("alt_mouth_zig");
		static const char *arm_names[2] = { "arm_l", "arm_r" }, *eye_names[2] = { "eye_l", "eye_r" }, *pupil_names[2] = { "pupil_l", "pupil_r" },
						  *brow_names[2] = { "brow_l", "brow_r" };
		for (int s = 0; s < 2; ++s) {
			arm_[s] = rig.slot(arm_names[s]);
			eye_[s] = rig.slot(eye_names[s]);
			pupil_[s] = rig.slot(pupil_names[s]);
			brow_[s] = rig.slot(brow_names[s]);
		}
		pole_len_ = Math::max(rig.rest(body_).origin.y, 0.2f);
		face_.bind(rig);
		face_.max_yaw = 0.7f;
		face_.max_pitch = 0.35f;
		face_.pupil_travel = 0.1f; // the button slides a centimetre over its glow disc
		face_.closed_scale = 0.1f;
		face_.look_height = 0.8f;
		// Matte cloth, burlap and straw; shiny plastic buttons; a little sheen on the felt hat.
		for (int i = 0; i < rig.count(); ++i) {
			rig.gloss(i, 0.0f);
		}
		rig.param_all("rim_strength", 0.22f);
		rig.gloss(body_, 0.03f);
		// Felt: matte (gloss < 0 also drops the soft painterly highlight). With it the crown carried a
		// broad white sheen and read as shiny vinyl (FIDELITY_PASS decision 14).
		rig.gloss(hat_, -1.0f);
		rig.gloss(hat_tip_, -1.0f);
		for (int s = 0; s < 2; ++s) {
			rig.gloss(pupil_[s], 0.12f); // at 0.3 the whole button flashed white when it faced the light
			rig.gloss(eye_[s], 0.25f);
			rig.glow(eye_[s], HALO_REST);
			rig.glow(pupil_[s], THREAD_REST);
		}
		// The soft parts. Straw: bent as one piece about its pin and rippled round its own Y axis.
		struct Def {
			const char *name;
			Vector3 pin_dir; // soft_pin direction (zero: by distance from the pin)
			float pin_len, rigid;
			Vector3 rest_dir; // what the Dangle swings
			float len, hz, zeta, drag, limit, inertia;
			float lobes, below, gain; // ripple: lobes round the axis, the local height it starts under (0: by distance), amplitude scale
		};
		static const Def defs[SOFT_N] = {
			{ "skirt", Vector3(0, -1, 0), 0.2f, 0.0f, Vector3(0, -1, 0), 0.22f, 2.1f, 0.28f, 0.35f, 0.75f, 1.0f, 4.0f, 0.03f, 1.0f },
			{ "collar", Vector3(), 0.2f, 0.05f, Vector3(0, 1, 0), 0.14f, 3.0f, 0.3f, 0.2f, 0.4f, 0.8f, 3.0f, 0.0f, 0.8f },
			{ "cuff_l", Vector3(0, 1, 0), 0.18f, 0.0f, Vector3(0, 1, 0), 0.19f, 2.6f, 0.26f, 0.4f, 0.9f, 1.0f, 3.0f, 0.0f, 0.9f },
			{ "cuff_r", Vector3(0, 1, 0), 0.18f, 0.0f, Vector3(0, 1, 0), 0.19f, 2.6f, 0.26f, 0.4f, 0.9f, 1.0f, 3.0f, 0.0f, 0.9f },
			{ "hair", Vector3(0, -1, 0), 0.2f, 0.05f, Vector3(0, -1, 0), 0.26f, 2.4f, 0.28f, 0.3f, 0.6f, 1.0f, 5.0f, -0.04f, 0.9f },
			{ "hat_tip", Vector3(0, 1, 0), 0.26f, 0.07f, Vector3(0, 1, 0), 0.34f, 1.7f, 0.2f, 0.3f, 1.15f, 1.0f, 0.0f, 0.0f, 0.0f },
		};
		for (int i = 0; i < SOFT_N; ++i) {
			Soft &s = soft_[i];
			const Def &d = defs[i];
			s.slot = rig.slot(d.name);
			s.dir = d.rest_dir;
			s.params.len = d.len;
			s.params.hz = d.hz;
			s.params.zeta = d.zeta;
			s.params.drag = d.drag;
			s.params.limit = d.limit;
			s.params.inertia = d.inertia;
			s.lobes = d.lobes;
			s.below = d.below;
			s.gain = d.gain;
			rig.soft(s.slot, d.pin_dir, d.pin_len, d.rigid);
		}
		rig.soft(hat_tip_, HAT_UP, 0.26f, 0.03f); // the cone bends along the hat's own (tilted) axis, from just above its base ring
		soft_[TIP].dir = rig.tip(hat_tip_).length_squared() > 1e-6f ? rig.tip(hat_tip_).normalized() : HAT_UP;
		soft_[TIP].params.len = Math::max(rig.reach(hat_tip_), 0.1f);
	}

	void update(const AnimInput &in, MonsterRig &rig, Transform3D &root) override {
		using State = MonsterAnimState;
		const float dt = anim::safe_dt(in.dt);
		const bool far = in.lod > 0;
		if (!seeded_) {
			seeded_ = true;
			fidget_ = anim::RandTimer(in.seed ^ 0x5C4Eu);
			phase0_ = anim::hash01(in.seed) * TAU;
			turn_ = ((in.seed >> 4) & 1u) != 0 ? 1.0f : -1.0f;
			ripple_phase_ = phase0_;
		}
		const bool dying = in.state == State::Dying, rising = in.state == State::Rise;
		const bool alive = !dying && !rising;
		const bool charging = in.state == State::Attack;

		// ---- one-shots
		const Vector3 recoil = hit_.update(in); // sets hit_.fresh and hit_.ouch
		const float ouch = alive ? hit_.ouch : 0.0f;
		const bool landed = in.on_floor && !was_floor_ && alive;
		if (!in.on_floor) {
			fall_ = in.vel_local.y;
		}
		was_floor_ = in.on_floor;

		// ---- the hop cycle. It runs on time, at a period set by the speed, while it travels on the
		// ground; a hop that is under way is finished even if it stops.
		const bool moving = alive && in.on_floor && (charging || ((in.state == State::Roam || in.state == State::Recover) && in.speed > 0.15f));
		speed_ = far ? in.speed : anim::damp(speed_, in.speed, 0.06f, dt);
		const float run = saturate(speed_ / CHASE_SPEED);
		const float period = charging ? HOP_CHARGE : lerp(HOP_SLOW, HOP_FAST, run);
		amt_ = far ? (moving ? 1.0f : 0.0f) : anim::damp(amt_, moving ? 1.0f : 0.0f, 0.06f, dt);
		bool touchdown = false;
		if (in.state == State::Attack && in.entered) {
			phase_ = CONTACT; // the charge starts with a leap
		}
		if (moving || amt_ > 0.03f) {
			phase_ += dt / period;
			if (phase_ >= 1.0f) {
				phase_ -= std::floor(phase_);
				touchdown = amt_ > 0.3f;
			}
		} else {
			phase_ = 0.0f; // at rest: the next start begins by sinking onto the pole
		}
		// Where the foot is (model space), how far the body has sunk onto the pole, the squash.
		const float plant = Math::min(speed_ * period * CONTACT * 0.5f, charging ? PLANT_CHARGE : PLANT_MAX) * amt_;
		const float hop_h = (charging ? HOP_H_CHARGE : lerp(HOP_H_SLOW, HOP_H_FAST, run)) * amt_;
		float foot_z, foot_y = 0.0f, hop_slide, hop_squash;
		if (phase_ < CONTACT) {
			const float u = phase_ / CONTACT;
			foot_z = plant * (1.0f - 2.0f * u);
			const float sink = std::sin(PI * std::pow(u, 0.75f));
			hop_slide = -SLIDE * sink * amt_;
			hop_squash = 1.0f - 0.1f * sink * amt_;
		} else {
			const float u = (phase_ - CONTACT) / (1.0f - CONTACT);
			foot_z = plant * (2.0f * smoothstep01(u) - 1.0f);
			foot_y = hop_h * anim::arc(u);
			const float spring = std::sin(PI * saturate(u * 1.7f));
			hop_slide = SLIDE * 0.55f * spring * amt_;
			hop_squash = 1.0f + 0.08f * spring * amt_;
		}

		// Idle fidgets when it stands about with nobody to chase.
		const bool idle = in.state == State::Roam && !in.chasing && in.speed < 0.15f && in.on_floor;
		fidget_t_ += dt;
		if (!idle) {
			fidget_t_ = Math::max(fidget_t_, 10.0f); // drop a fidget the moment it has something to do
			fidget_.reset();
			idle_t_ = 0.0f;
		} else if ((idle_t_ += dt) > 5.0f && fidget_t_ > fidget_len() + 0.8f && fidget_.step(dt, 3.0f, 6.5f)) {
			fidget_kind_ = (fidget_.count + 2) % 3; // a curious tilt of the head first, then a bounce on the spot, then a look round
			fidget_t_ = 0.0f;
		}

		// ---- the pose: idle and travelling, then the state layers
		w_chase_.step(alive && (in.chasing || in.state == State::Windup || charging), 0.3f, 0.6f, dt);
		w_wind_.step(in.state == State::Windup, 0.1f, 0.14f, dt);
		w_charge_.step(charging, 0.06f, 0.28f, dt);
		w_dizzy_.step(alive && in.dizzy > 0.0f && in.state == State::Recover, 0.22f, 0.3f, dt);
		w_air_.step(alive && !in.on_floor, 0.05f, 0.12f, dt);
		Pose p = base_pose(in, run);
		if (in.state == State::Windup) {
			wind_ = in.windup; // kept, so the layer can fade from where it stopped (a hit in the middle of a wind-up)
		}
		if (w_wind_.w > 0.0f) {
			mix(p, windup(in, wind_), w_wind_.w);
		}
		if (charging) {
			charge_t_ = in.state_t;
		}
		if (w_charge_.w > 0.0f) {
			mix(p, charge(in, charge_t_), w_charge_.w);
		}
		// Dizzy: how far round the cone it has gone, and the cone itself (added after the springs).
		const float dizzy = w_dizzy_.w;
		if (dizzy > 0.0f) {
			dizzy_a_ += dt * TAU * 0.95f * turn_;
			mix(p, dizzy_pose(in), dizzy);
		}
		const bool was_dizzy = dizzy_on_;
		dizzy_on_ = alive && in.dizzy > 0.0f && in.state == State::Recover;
		if (was_dizzy && !dizzy_on_ && alive) {
			shake_t_ = 0.0f; // it shakes its head and the hat hops back into place
		}
		shake_t_ += dt;
		bounce_ = sink_ = 0.0f;
		fidgets(in, p);
		if (w_air_.w > 0.0f) { // knocked or dropped: arms up, the pole swings out in front
			Pose a = p;
			a.pole = -0.3f - 0.25f * ouch;
			a.squash = 1.06f;
			a.slide = 0.02f;
			for (int s = 0; s < 2; ++s) {
				a.arm_up[s] = 0.45f;
			}
			mix(p, a, w_air_.w);
		}
		if (ouch > 0.0f) { // a flinch: thrown back, arms flung up, brows up, mouth an O, the straw bristles
			Pose h = p;
			h.body += Vector3(-0.2f, 0.0f, 0.1f * turn_);
			h.head = Vector3(-0.32f, 0.0f, -0.16f * turn_);
			h.hat = Vector3(-0.08f, 0.0f, 0.14f * turn_);
			for (int s = 0; s < 2; ++s) {
				h.arm_up[s] = 0.72f - 0.2f * float(s) * turn_;
				h.arm_back[s] = -0.25f;
			}
			h.frown = -1.0f;
			h.brow_up = 0.016f;
			h.wide = 1.0f;
			h.coil = -0.5f;
			h.ripple = 0.03f;
			h.ripple_rate = 26.0f;
			h.glow = 0.0f;
			mix(p, h, ouch);
		}
		shut_ = 0.0f;
		float yaw = in.yaw;
		Vector3 offset, scale(1, 1, 1);
		float collapse = 0.0f; // dying: 0 standing .. 1 a heap
		float arm_hang = 0.0f; // 0 as posed .. 1 hanging straight down
		if (dying) {
			die_pose(in, p, collapse, arm_hang, yaw, scale);
		}
		if (rising) {
			rise_pose(in, p, arm_hang, offset, dt);
		}

		// ---- springs
		const Vector3 cone = Vector3(std::cos(dizzy_a_), 0.0f, std::sin(dizzy_a_)) * (0.34f * dizzy * (0.8f + 0.2f * std::sin(in.t * 1.3f)));
		if (far || !primed_) {
			primed_ = true;
			lean_.reset(p.lean);
			body_s_.reset();
			loll_.reset();
			tip_.reset();
			hat_hop_.reset();
			tuft_s_.reset();
			slide_s_.reset(p.slide + hop_slide);
		} else {
			// The root's lean chases the pose's with overshoot: a lunge into the charge, a stagger out of it.
			const float stiff = Math::max(w_wind_.w, w_charge_.w);
			lean_.step(p.lean, lerp(2.3f, 4.2f, stiff), lerp(0.3f, 0.5f, stiff), dt);
			// Starts, stops and knocks tip the whole stick (it nearly falls over) and throw the head about.
			tip_.kick(Vector3(-in.accel_local.z, 0.0f, in.accel_local.x) * (0.2f * dt));
			loll_.kick(Vector3(-in.accel_local.z, 0.0f, in.accel_local.x) * (0.7f * dt));
			if (touchdown) {
				loll_.kick(Vector3(2.6f, 0.0f, 0.5f * side_) * (0.5f + 0.5f * run));
				tuft_s_.kick(5.5f);
				side_ = -side_; // the head nods to the other side on the next landing
			}
			if (hit_.fresh) {
				loll_.kick(Vector3(in.hit_dir_local.z, 0.0f, -in.hit_dir_local.x) * 3.5f);
				hat_hop_.kick(4.2f); // the hat jumps off its head
			}
			if (landed) {
				loll_.kick(Vector3(3.0f, 0.0f, 0.5f * side_));
				tuft_s_.kick(8.0f);
				hat_hop_.kick(Math::clamp(-fall_, 0.0f, 6.0f) * 0.25f);
			}
			if (shake_t_ < 0.02f && alive) {
				hat_hop_.kick(1.3f);
			}
			tip_.step(Vector3(), 2.4f, 0.3f, dt);
			if (tip_.x.length() > 0.18f) {
				tip_.x = tip_.x.normalized() * 0.18f;
			}
			loll_.step(Vector3(), 2.9f, 0.27f, dt);
			if (loll_.x.length() > 0.42f) {
				loll_.x = loll_.x.normalized() * 0.42f;
			}
			// Nodding forward is held shorter: at the end of a charge the stop throws the head forward
			// and the brim covered the whole face for a fifth of a second.
			if (loll_.x.x > 0.26f) {
				loll_.x.x = 0.26f;
				loll_.v.x = Math::min(loll_.v.x, 0.0f);
			}
			hat_hop_.step(0.0f, 3.6f, 0.3f, dt);
			tuft_s_.step(0.0f, 5.0f, 0.3f, dt);
			slide_s_.step(p.slide + hop_slide, 7.0f, 0.55f, dt);
		}

		// ---- the pole: its foot where the hop cycle puts it, tilted so its top stays under the body
		// (the pose's extra tilt swings the foot under a top that stays put: kicked out in front in the air)
		const float air = w_air_.w;
		const float plant_tilt = std::atan2(-foot_z, pole_len_) * (1.0f - air);
		const float tilt_x = plant_tilt + p.pole;
		const Basis tilt(Vector3(1, 0, 0), tilt_x);
		const Vector3 foot(0.0f, foot_y + bounce_ - sink_ + pole_len_ * (std::cos(plant_tilt) - std::cos(tilt_x)) * air,
				foot_z * (1.0f - air) + pole_len_ * (std::sin(plant_tilt) - std::sin(tilt_x)));
		rig.pose(pole_, tilt, foot);
		// The straw round the foot splays flat when it lands.
		const float splay = Math::clamp(tuft_s_.x, -0.25f, 0.6f);
		rig.pose(tuft_, Basis(), Vector3(), Vector3(1.0f + 0.5f * splay, 1.0f - 0.6f * splay, 1.0f + 0.5f * splay));

		// ---- the body: it stays more upright than the pole, a beat late, and slides on it
		const Vector3 body_want = Vector3(tilt_x * 0.35f, 0.0f, 0.0f) + p.body + cone * 0.6f;
		if (far) {
			body_s_.reset(body_want);
		} else {
			body_s_.step(body_want, 3.3f, 0.4f, dt);
		}
		const float squashy = 1.0f + 0.5f * collapse, squat = 1.0f - 0.45f * collapse;
		rig.pose(body_, tilt.inverse() * Basis(anim::quat_from_rotvec(body_s_.x + (alive ? recoil * 0.4f : Vector3()))),
				Vector3(0.0f, Math::clamp(slide_s_.x, -0.5f, 0.09f), 0.0f), Vector3(squashy, squat, squashy));
		// The head (and the hat on it) must not inherit the heap's squash, or the hat lands as a huge flat disc;
		// it keeps a little of it.
		const Vector3 head_scale((1.0f + 0.08f * collapse) / squashy, (1.0f - 0.08f * collapse) / squat, (1.0f + 0.08f * collapse) / squashy);

		// ---- arms: posed, then flopping on their shoulders (the Dangle is thrown by every hop and stop)
		for (int s = 0; s < 2; ++s) {
			Vector3 flop;
			if (far) {
				arm_d_[s].reset();
			} else {
				anim::DangleParams dp;
				dp.len = 0.5f;
				dp.hz = 2.2f;
				dp.zeta = 0.3f;
				dp.drag = 0.3f;
				dp.inertia = 1.25f;
				dp.limit = 0.95f;
				if (hit_.fresh) {
					arm_d_[s].kick(Vector3(0, 3.0f, 0));
				}
				flop = arm_d_[s].step(rig.world_rest(arm_[s]), Vector3(SIDE[s], 0, 0), dp, dt);
			}
			// (the two angles of a swing: about Y = fore and aft, about Z = up and down)
			float back = p.arm_back[s] + SIDE[s] * flop.y;
			float up = Math::clamp(p.arm_up[s] + SIDE[s] * flop.z, -1.45f, 0.82f); // not through the head
			back = lerp(back, 0.0f, arm_hang);
			up = lerp(up, -1.38f, arm_hang);
			rig.pose(arm_[s], Basis(Vector3(0, 1, 0), SIDE[s] * back) * Basis(Vector3(0, 0, 1), SIDE[s] * up));
		}

		// ---- head and face
		const bool intent = in.state == State::Windup || charging;
		face_.look_weight = intent ? 1.0f : ((look_away_ || dizzy > 0.3f) ? 0.0f : -1.0f);
		face_.head_lag = anim::damp(face_.head_lag, intent ? 0.05f : 0.16f, 0.08f, dt);
		face_.wide = p.wide;
		face_.shut = shut_;
		face_.update(in, rig);
		const float shake = shake_t_ < 0.7f ? std::sin(shake_t_ * TAU * 4.5f) * 0.36f * (1.0f - smoothstep01(shake_t_ / 0.7f)) : 0.0f;
		// (it keeps its head a little more level than the body under it)
		const Vector3 level = -body_s_.x * 0.45f - Vector3(lean_.x.x, 0.0f, lean_.x.z) * 0.25f;
		rig.pose(head_, Basis(anim::quat_from_rotvec(level)) * Basis::from_euler(anim::look_euler(face_.head)) * Basis::from_euler(p.head + Vector3(0, shake, 0)) *
								Basis(anim::quat_from_rotvec(loll_.x)),
				Vector3(), head_scale);

		// Buttons: they slide to look (MonsterFace) and spin while it is dizzy, each its own way; when it
		// stops they settle on the nearest quarter turn (a cross of thread looks the same there).
		for (int s = 0; s < 2; ++s) {
			if (dizzy > 0.0f) {
				spin_[s] += dt * (s == L ? 10.5f : -8.0f) * dizzy;
			} else {
				spin_[s] = anim::damp(spin_[s], std::round(spin_[s] / (PI * 0.5f)) * (PI * 0.5f), 0.07f, dt);
			}
			if (pupil_[s] >= 0 && rig.node(pupil_[s]) && std::fabs(spin_[s]) > 1e-4f) {
				const Vector3 slide = rig.node(pupil_[s])->get_position() - rig.rest(pupil_[s]).origin;
				rig.pose(pupil_[s], Basis(Vector3(0, 0, 1), spin_[s]), slide);
			}
			// Brows: +Z is out of the face and +X the monster's left on both, so the inner end is -X on the left brow.
			const Basis rest = rig.rest(brow_[s]).basis;
			rig.pose(brow_[s], Basis(Vector3(0, 0, 1), SIDE[s] * 0.42f * p.frown), rest.xform(Vector3(0.0f, p.brow_up - 0.006f * Math::max(p.frown, 0.0f), 0.0f)));
		}
		// Eye glow: the discs behind the buttons and the thread flare through the wind-up and the charge.
		glow_ = far ? p.glow : anim::damp(glow_, p.glow, 0.05f, dt);
		if (std::fabs(glow_ - glow_sent_) > 0.01f) {
			glow_sent_ = glow_;
			for (int s = 0; s < 2; ++s) {
				rig.glow(eye_[s], lerp(HALO_REST, HALO_FLARE, glow_));
				rig.glow(pupil_[s], lerp(THREAD_REST, THREAD_FLARE, glow_));
			}
		}
		// Mouth: the stitched smirk, an O (the charge's yell, a yelp), or a zig-zag (gritted for the wind-up, queasy when dizzy).
		int mouth = 0;
		if (dying || ouch > 0.25f || (charging && charge_t_ > 0.05f)) {
			mouth = 1;
		} else if (in.state == State::Windup || dizzy > 0.4f) {
			mouth = 2;
		}
		rig.show(mouth_, mouth == 0);
		rig.show(mouth_o_, mouth == 1);
		rig.show(mouth_zig_, mouth == 2);
		if (mouth == 1) {
			const float o = 1.0f + 0.12f * std::sin(in.t * 23.0f);
			rig.pose(mouth_o_, Basis(), Vector3(), Vector3(o, 2.0f - o, 1.0f));
		} else if (mouth == 2) {
			rig.pose(mouth_zig_, Basis(Vector3(0, 0, 1), 0.12f * std::sin(in.t * (dizzy > 0.4f ? 5.0f : 31.0f))));
		}

		// ---- the hat rocks on the head, hops off it when hit, slips when dizzy
		Vector3 hat_rock;
		if (far) {
			hat_d_.reset();
		} else {
			anim::DangleParams dp;
			dp.len = 0.34f;
			dp.hz = 3.4f;
			dp.zeta = 0.33f;
			dp.inertia = 0.5f;
			dp.limit = 0.16f;
			hat_rock = hat_d_.step(rig.world_rest(hat_), Vector3(0, 1, 0), dp, dt);
		}
		const float hop = Math::clamp(hat_hop_.x, -0.02f, 0.16f);
		rig.pose(hat_, Basis(anim::quat_from_rotvec(hat_rock)) * Basis::from_euler(p.hat + Vector3(-0.6f * hop, 0.0f, 0.0f)), p.hat_off + Vector3(0.0f, hop, 0.0f));

		// ---- straw and the hat's cone: soft meshes on Dangles, the straw with a ripple running round it
		ripple_phase_ += dt * p.ripple_rate;
		if (ripple_phase_ > 1000.0f) {
			ripple_phase_ -= 160.0f * TAU;
		}
		const Vector3 world_hit = Basis(Vector3(0, 1, 0), in.yaw).xform(in.hit_dir_local);
		for (int i = 0; i < SOFT_N; ++i) {
			Soft &s = soft_[i];
			if (s.slot < 0) {
				continue;
			}
			Vector3 bend;
			if (far) {
				s.dangle.reset();
			} else {
				if (hit_.fresh) {
					s.dangle.kick(world_hit * 2.2f + Vector3(0, 2.0f, 0));
				}
				bend = s.dangle.step(rig.world_rest(s.slot), s.dir, s.params, dt);
			}
			if (i == TIP) {
				// Curled tighter or straightened by the pose, with a slow sway of its own.
				bend += HAT_CURL * (p.coil + 0.07f * std::sin(in.t * 1.7f + phase0_));
			} else if (s.gain > 0.0f) {
				rig.ripple(s.slot, far ? 0.0f : p.ripple * s.gain, ripple_phase_ + float(i) * 1.3f, s.lobes, s.below);
			}
			rig.bend(s.slot, bend);
		}

		// ---- the root
		Vector3 lean = lean_.x + cone;
		if (alive) {
			lean += recoil * 0.55f + tip_.x;
		}
		const Vector3 sq = squash_.update(in, alive ? p.squash * hop_squash : p.squash);
		root = root_pose(yaw, lean, Vector3(sq.x * scale.x, sq.y * scale.y, sq.z * scale.z), offset);
	}

private:
	enum { SKIRT, COLLAR, CUFF_L, CUFF_R, HAIR, TIP, SOFT_N };
	struct Soft {
		int slot = -1;
		Vector3 dir;
		anim::DangleParams params;
		anim::Dangle dangle;
		float lobes = 3.0f, below = 0.0f, gain = 1.0f;
	};

	float fidget_len() const { return fidget_kind_ == 0 ? 1.9f : (fidget_kind_ == 1 ? 1.3f : 2.3f); }

	// Standing (a slow sway in the wind, arms a little low) and travelling (leaning into the hops).
	Pose base_pose(const AnimInput &in, float run) const {
		Pose p;
		const float still = 1.0f - amt_;
		const float chase = w_chase_.w;
		const float sway = anim::fidget(in.t * 0.21f, in.seed + 3u), nod = anim::fidget(in.t * 0.17f, in.seed + 7u);
		p.lean = Vector3(0.022f * nod * still + (0.05f + 0.07f * run) * amt_, 0.0f, 0.03f * sway * still);
		p.body = Vector3(0.02f * nod, 0.0f, 0.03f * sway) * still;
		// Standing, it breathes: the stuffing settles on the pole and puffs up again.
		const float breathe = std::sin(in.t * 2.1f + phase0_);
		p.slide = -0.008f * (1.0f + breathe) * still;
		p.squash = 1.0f + 0.012f * breathe * still;
		p.head = Vector3(-0.04f * amt_ - 0.05f * chase, 0.0f, 0.06f * still * anim::fidget(in.t * 0.13f, in.seed + 11u));
		p.hat = Vector3(-0.03f * amt_, 0.0f, 0.0f);
		// Now and then a gust of wind: it rocks, the hat's tip is blown straight, the straw flutters.
		const float gust = smoothstep01(remap01(anim::fbm1(in.t * 0.33f, in.seed + 21u, 2), 0.05f, 0.55f)) * still;
		p.lean += Vector3(-0.025f, 0.0f, 0.04f) * gust;
		p.body += Vector3(-0.02f, 0.0f, 0.03f) * gust;
		p.hat.x -= 0.07f * gust;
		for (int s = 0; s < 2; ++s) {
			const float breath = std::sin(in.t * 1.4f + phase0_ + float(s) * 2.3f);
			p.arm_up[s] = lerp(-0.14f + 0.06f * breath, 0.16f, chase) - 0.06f * amt_ + 0.05f * gust * std::sin(in.t * 6.5f + float(s) * 1.7f);
			p.arm_back[s] = lerp(0.0f, -0.22f, chase) + 0.1f * amt_ + 0.12f * gust; // reaching forward once it has seen someone
		}
		p.frown = 0.45f * chase;
		p.wide = 1.0f + 0.05f * chase;
		p.coil = -0.35f * gust;
		p.ripple = 0.004f + 0.007f * amt_ + 0.004f * run + 0.014f * gust;
		p.ripple_rate = 2.2f + 5.0f * amt_ + 10.0f * gust;
		return p;
	}

	// The wind-up (1.0 s): it sinks onto the pole and leans back, further and further, trembling; the
	// arms are drawn back and up, the hat's tip coils, the brows come down. In the last fifth it rocks
	// forward onto its toe, cocked.
	Pose windup(const AnimInput &in, float wind) const {
		Pose p;
		const float c = anim::ease_out_back(remap01(wind, 0.0f, 0.45f), 1.5f);
		const float deep = smoothstep01(remap01(wind, 0.3f, 0.82f));
		const float e = smoothstep01(remap01(wind, 0.82f, 1.0f));
		const float tremble = saturate(c) * std::sin(in.t * 43.0f) * (0.4f + 0.6f * wind);
		p.lean = Vector3(-0.24f * c - 0.15f * deep + 0.1f * e, 0.0f, 0.012f * tremble);
		p.slide = -0.075f * c - 0.02f * deep + 0.05f * e;
		p.squash = 1.0f - 0.09f * c - 0.04f * deep + 0.1f * e;
		p.pole = 0.09f * c; // the pole stays the more upright: the body bows back over it
		p.body = Vector3(-0.14f * c - 0.08f * deep, 0.0f, 0.016f * tremble);
		p.head = Vector3(0.3f * c + 0.1f * deep, 0.0f, 0.02f * tremble); // chin down: it glares from under the brim
		p.hat = Vector3(-0.1f * c, 0.0f, 0.0f);
		for (int s = 0; s < 2; ++s) {
			p.arm_back[s] = 0.75f * c + 0.3f * deep + 0.012f * tremble;
			p.arm_up[s] = 0.24f * c + 0.2f * deep;
		}
		p.frown = saturate(c);
		p.brow_up = 0.0f;
		p.wide = 1.0f + 0.08f * saturate(c) + 0.1f * e;
		p.coil = 0.5f * c + 0.55f * deep + 0.03f * tremble;
		p.ripple = 0.006f + 0.01f * wind;
		p.ripple_rate = 9.0f + 22.0f * wind;
		p.glow = smoothstep01(wind);
		return p;
	}

	// The charge: thrown forward to 25 degrees (the root's spring overshoots it), arms and hat tip
	// streaming back, chin up so the face still looks where it is going.
	Pose charge(const AnimInput &in, float t) const {
		Pose p;
		const float go = anim::ease_out(saturate(t / 0.12f));
		const float flutter = std::sin(in.t * 37.0f);
		p.lean = Vector3(CHARGE_LEAN, 0.0f, 0.0f);
		p.slide = 0.03f * go;
		p.squash = 1.0f + 0.1f * anim::pulse(t, 0.06f, 0.25f);
		p.body = Vector3(0.1f, 0.0f, 0.0f);
		p.head = Vector3(-0.46f * go, 0.0f, 0.0f);
		p.hat = Vector3(-0.08f * go, 0.0f, 0.0f);
		p.hat_off = Vector3(0.0f, 0.0f, -0.012f * go);
		for (int s = 0; s < 2; ++s) {
			p.arm_back[s] = 1.08f + 0.05f * flutter * SIDE[s];
			p.arm_up[s] = 0.22f + 0.07f * std::sin(in.t * 29.0f + float(s) * 2.0f);
		}
		p.frown = 1.0f;
		p.wide = 1.16f;
		p.coil = -1.05f; // straightened out behind it
		p.ripple = 0.022f;
		p.ripple_rate = 34.0f;
		p.glow = 1.0f;
		return p;
	}

	// Dizzy: everything this layer holds still; the cone itself is added after the springs. The head
	// leans against the cone, the hat has slipped down over one eye.
	Pose dizzy_pose(const AnimInput &in) const {
		Pose p;
		const Vector3 dir(std::cos(dizzy_a_), 0.0f, std::sin(dizzy_a_));
		p.lean = Vector3();
		p.squash = 0.97f;
		p.slide = -0.015f;
		p.head = Vector3(-0.55f * dir.x, 0.5f * std::sin(dizzy_a_ * 0.5f + 1.0f), -0.55f * dir.z); // against the lean (past level), and rolling round on its neck
		p.hat = Vector3(0.24f, 0.0f, 0.2f);
		p.hat_off = Vector3(0.03f, -0.028f, 0.05f);
		for (int s = 0; s < 2; ++s) {
			p.arm_up[s] = -0.34f + 0.2f * std::sin(dizzy_a_ + float(s) * PI);
			p.arm_back[s] = 0.16f * std::cos(dizzy_a_ + float(s) * PI);
		}
		p.frown = -0.8f;
		p.brow_up = 0.006f;
		p.wide = 1.0f;
		p.coil = 0.25f * std::sin(dizzy_a_ * 1.0f + 0.7f);
		p.ripple = 0.007f;
		p.ripple_rate = 6.0f;
		p.glow = 0.12f + 0.12f * std::sin(in.t * 9.0f);
		return p;
	}

	// Standing about: a curious tilt of the head, a bounce on the spot, a look round.
	void fidgets(const AnimInput &in, Pose &p) {
		look_away_ = false;
		if (fidget_t_ >= fidget_len()) {
			return;
		}
		const float u = fidget_t_ / fidget_len();
		if (fidget_kind_ == 0) {
			const float env = anim::smooth_pulse(fidget_t_, 0.3f, fidget_len() - 0.75f, 0.45f);
			p.head += Vector3(0.03f, 0.1f * turn_, 0.26f * turn_) * env;
			p.body.z += 0.05f * turn_ * env;
			p.brow_up += 0.008f * env;
			p.hat += Vector3(0.0f, 0.0f, -0.06f * turn_) * env;
		} else if (fidget_kind_ == 1) { // sinks, pops up off the ground, lands
			const float sink = anim::smooth_pulse(fidget_t_, 0.22f, 0.05f, 0.1f);
			const float up = anim::arc(remap01(fidget_t_, 0.37f, 0.75f));
			const float land = anim::pulse(fidget_t_ - 0.75f, 0.05f, 0.3f);
			p.slide += -0.06f * sink - 0.04f * land + 0.03f * up;
			p.squash *= 1.0f - 0.1f * sink + 0.1f * up - 0.1f * land;
			bounce_ = 0.1f * up;
			for (int s = 0; s < 2; ++s) {
				p.arm_up[s] += -0.2f * sink + 0.45f * up;
			}
			p.wide += 0.1f * up;
		} else {
			const float env = anim::smooth_pulse(fidget_t_, 0.3f, fidget_len() - 0.7f, 0.4f);
			p.head += Vector3(0.0f, 0.55f * std::sin(TAU * u), 0.08f * std::sin(TAU * u)) * env;
			p.body.y += 0.12f * std::sin(TAU * u) * env;
			look_away_ = true;
		}
	}

	// Dying (0.6 s): a start (arms up, the hat jumps), then the stuffing goes out of it: the body slides
	// down the pole and spreads, the arms drop, the head nods onto its chest, the pole sinks, and the
	// hat, left behind in the air, drops onto the heap last. It winks out at the very end.
	void die_pose(const AnimInput &in, Pose &p, float &collapse, float &arm_hang, float &yaw, Vector3 &scale) {
		const float k = saturate(in.state_t / DEATH_TIME);
		const float start = anim::pulse(k, 0.1f, 0.14f);
		const float c = anim::ease_in(remap01(k, 0.2f, 0.72f));
		const float settle = smoothstep01(remap01(k, 0.62f, 0.86f));
		Pose d;
		d.lean = Vector3(-0.1f * start + 0.12f * c, 0.0f, 0.1f * c * turn_);
		d.slide = 0.03f * start - 0.3f * c;
		d.squash = 1.0f + 0.12f * start;
		d.pole = 0.0f;
		d.body = Vector3(0.25f * c, 0.0f, 0.0f);
		d.head = Vector3(-0.3f * start + 0.4f * c, 0.0f, 0.2f * c * turn_);
		// (the hat ends up lying level on top of the heap, whatever the head under it does)
		d.hat = Vector3(-0.2f * start - 0.35f * settle, 0.0f, 0.3f * c * turn_ * (1.0f - settle) - 0.2f * settle * turn_);
		// (the hat hangs in the air while the head drops away under it, then falls)
		d.hat_off = Vector3(0.0f, 0.12f * start + 0.42f * c * (1.0f - settle), 0.03f * settle);
		for (int s = 0; s < 2; ++s) {
			d.arm_up[s] = 0.7f * start;
			d.arm_back[s] = -0.2f * start;
		}
		d.frown = -1.0f;
		d.brow_up = 0.016f;
		d.wide = 1.3f;
		d.coil = -0.3f * start + 0.6f * c;
		d.ripple = 0.02f * (1.0f - c);
		d.ripple_rate = 20.0f;
		mix(p, d, smoothstep01(k / 0.08f));
		collapse = c;
		arm_hang = smoothstep01(remap01(k, 0.2f, 0.5f));
		shut_ = smoothstep01(remap01(k, 0.45f, 0.62f));
		yaw += 1.1f * c * turn_;
		const float gone = 1.0f - 0.94f * smoothstep01(remap01(k, 0.88f, 1.0f));
		// The pole sinks into the ground as it goes: the heap ends up on the floor.
		sink_ = 0.2f * c;
		scale = Vector3(gone, gone, gone);
	}

	// Spawning (0.6 s): it comes up out of the ground hat first. For a beat only the hat's tip shows,
	// wagging; then it shoots up, arms pinned to its sides, clears the ground, flings its arms out at
	// the top and lands with a squash; its eyes pop open as it lands.
	void rise_pose(const AnimInput &in, Pose &p, float &arm_hang, Vector3 &offset, float dt) {
		const float k = saturate(in.state_t / RISE_TIME);
		const float up = anim::ease_out_cubic(remap01(k, 0.2f, 0.52f)); // out of the ground
		const float hop = anim::arc(remap01(k, 0.38f, 0.78f)); // ... and into the air
		const float land = anim::pulse(k - 0.78f, 0.04f, 0.2f);
		Pose r;
		r.lean = Vector3(0.05f * std::sin(k * 60.0f) * (1.0f - up), 0.0f, 0.07f * std::sin(k * 47.0f) * (1.0f - up));
		r.slide = 0.04f * hop - 0.07f * land;
		r.squash = 1.0f + 0.16f * anim::pulse(k - 0.2f, 0.12f, 0.3f) - 0.16f * land;
		r.pole = -0.2f * hop;
		r.head = Vector3(0.3f * (1.0f - smoothstep01(remap01(k, 0.5f, 0.75f))) - 0.2f * land, 0.0f, 0.0f);
		r.hat = Vector3(0.12f * (1.0f - up), 0.0f, 0.0f);
		for (int s = 0; s < 2; ++s) {
			r.arm_up[s] = 0.6f * anim::pulse(k - 0.5f, 0.12f, 0.3f);
		}
		r.brow_up = 0.014f * anim::pulse(k - 0.72f, 0.06f, 0.25f);
		r.wide = 1.0f + 0.3f * anim::pulse(k - 0.72f, 0.05f, 0.25f);
		r.coil = 0.5f * (1.0f - up) + 0.35f * std::sin(k * 40.0f) * (1.0f - up);
		r.ripple = 0.012f;
		r.ripple_rate = 18.0f;
		mix(p, r, 1.0f - smoothstep01(remap01(k, 0.86f, 1.0f)));
		arm_hang = 1.0f - smoothstep01(remap01(k, 0.44f, 0.6f));
		shut_ = 1.0f - smoothstep01(remap01(k, 0.7f, 0.86f)); // (final fixes 2) about 6 frames, never a 1-frame snap
		// Monsters are spawned a little above the ground and drop: the picture is held down on the
		// ground until the body has landed (the body itself is not moved).
		float drop = 0.0f;
		if (!in.on_floor && in.ground && *in.ground) {
			drop = Math::clamp(in.origin_world.y - (*in.ground)(in.origin_world.x, in.origin_world.z), 0.0f, 0.45f);
		}
		spawn_drop_ = in.on_floor ? anim::damp(spawn_drop_, 0.0f, 0.03f, dt) : drop;
		offset.y = -BURIED * (1.0f - up) + 0.17f * hop - spawn_drop_;
	}

	int pole_ = -1, tuft_ = -1, body_ = -1, head_ = -1, hat_ = -1, hat_tip_ = -1, mouth_ = -1, mouth_o_ = -1, mouth_zig_ = -1;
	int arm_[2] = { -1, -1 }, eye_[2] = { -1, -1 }, pupil_[2] = { -1, -1 }, brow_[2] = { -1, -1 };
	float pole_len_ = 0.52f;
	MonsterFace face_;
	HitRecoil hit_;
	RootSquash squash_;
	anim::Weight w_chase_, w_wind_, w_charge_, w_dizzy_, w_air_;
	anim::Spring3 lean_; // the root's lean, chasing the pose's
	anim::Spring3 body_s_; // the body's lean on top of it (a rotation vector, model space)
	anim::Spring3 loll_; // the head's wobble
	anim::Spring3 tip_; // starts and stops tip the whole stick
	anim::Spring hat_hop_, tuft_s_, slide_s_;
	anim::Dangle arm_d_[2], hat_d_;
	Soft soft_[SOFT_N];
	anim::RandTimer fidget_;
	float phase_ = 0.0f, amt_ = 0.0f, speed_ = 0.0f;
	float wind_ = 0.0f, charge_t_ = 0.0f, dizzy_a_ = 0.0f, shake_t_ = 10.0f, fidget_t_ = 10.0f, idle_t_ = 0.0f;
	float fall_ = 0.0f, phase0_ = 0.0f, turn_ = 1.0f, side_ = 1.0f, shut_ = 0.0f, ripple_phase_ = 0.0f, bounce_ = 0.0f, sink_ = 0.0f;
	float spin_[2] = { 0.0f, 0.0f };
	float glow_ = 0.0f, glow_sent_ = -1000.0f, spawn_drop_ = 0.0f;
	int fidget_kind_ = 0;
	bool was_floor_ = true, seeded_ = false, primed_ = false, look_away_ = false, dizzy_on_ = false;
};

} // namespace

TOT_MONSTER_ANIMATOR(MonsterType::Scarecrow, ScarecrowAnimator)

} // namespace tot
