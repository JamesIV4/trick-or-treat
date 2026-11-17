// Candy-Corn Gremlin (a LUNGE monster): the animator for its v2 rig.
//
// Part tree (art/team/monsters/batch04/build_monsters2.py, gremlin()):
//   body (meshless hip joint) > head (the whole candy: yellow, orange and cream bands, the face)
//        > tip (soft: the cream curl), ear_l / ear_r, brow_l / brow_r, eye_l / eye_r > pupil_l / pupil_r,
//          nose, jaw (the chin plate) > tongue (soft), arm_l / arm_r > forearm_x > hand_x (IK)
//   body > leg_l / leg_r > shin_x > foot_x (IK)
// The candy turns, leans, waddles and squashes about its hips, so the arms (in its space) ride along
// and the feet stay planted (model space).
//
// The mouth: the dark mouth is painted on the candy under the chin plate. `jaw` turns the plate about
// the centre of the face's vertical curvature (its pivot, about 0.98 m behind the lip), so a small
// angle slides it straight down the face: 0 = shut, 0.03 rad = a grin with the fangs showing, 0.088 rad
// = wide open. The tongue grows out of the mouth with the opening (its scale) and licks by swinging.
//
// A frame: a locomotion pose (idle hand-rubbing to a fast pattering waddle), state layers mixed over
// it by weight (wind-up, bite, recover, flinch, sniffs), then springs (lean, squash, ears, tip,
// tongue), then one solve. Rise and death are poses of the whole root (out of the ground; a fall on
// its back) on top of the solved body.

#include "play/monster_anim.h"

#include <godot_cpp/core/math.hpp>

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
constexpr float CHASE_SPEED = 2.8f; // tuning.speed: the gait is fullest there (it wanders at half)
constexpr float STRIDE_WALK = 0.3f, STRIDE_RUN = 0.42f; // metres per full cycle (two steps): a fast patter
constexpr float STEP_H = 0.05f;
constexpr float STANCE = 0.012f;
constexpr float TOE_OUT = 0.28f;
constexpr float JAW_GRIN = 0.031f, JAW_WIDE = 0.088f; // radians of the chin plate: a grin, wide open (the mouth is 9 cm deep)
constexpr float BITE_AT = 0.6f; // fraction of the lunge (0.25 s) when the jaw snaps shut
constexpr float LUNGE_TIME = 0.25f;
constexpr float RISE_TIME = 0.6f, DEATH_TIME = 0.6f;

struct Limb {
	int upper = -1, mid = -1, end = -1;
	Vector3 root, a, b;

	void bind(MonsterRig &rig, const char *u, const char *m, const char *e) {
		upper = rig.slot(u);
		mid = rig.slot(m);
		end = rig.slot(e);
		root = rig.rest(upper).origin;
		a = rig.rest(mid).origin;
		b = rig.rest(end).origin;
	}
	// Returns the mid joint's accumulated basis (upper * lower) in the space the limb hangs in.
	Basis solve(MonsterRig &rig, const Vector3 &hinge, const Vector3 &target, const Vector3 &pole) const {
		const anim::Ik2 ik = anim::two_bone_ik(root, a, b, hinge, target, pole, 2.6f);
		rig.pose(upper, Basis(ik.upper));
		rig.pose(mid, Basis(ik.lower));
		return Basis(ik.upper) * Basis(ik.lower);
	}
	Vector3 wrist() const { return root + a + b; }
};

// An orthonormal frame whose +Z is `d` and +Y is as close to `u` as it can be.
Basis frame(const Vector3 &d, const Vector3 &u) {
	const Vector3 z = d.length_squared() > 1e-10f ? d.normalized() : Vector3(0, 0, 1);
	Vector3 x = u.cross(z);
	if (x.length_squared() < 1e-8f) {
		x = anim::any_perp(z);
	}
	x = x.normalized();
	const Vector3 y = z.cross(x);
	Basis b;
	b.set_column(0, x);
	b.set_column(1, y);
	b.set_column(2, z);
	return b;
}

// Everything the solve needs for one frame. Layers are mixed into it field by field.
struct Pose {
	Vector3 body_off; // hips, model space, on top of rest
	Vector3 head_rot; // the candy: Euler (pitch, yaw, roll) about the hips, on top of the look
	Vector3 lean; // root lean (rotation vector, facing frame) about the feet
	float stretch = 1.0f; // the candy's squash (< 1) / stretch (> 1) target along its own axis
	Vector3 hand[2], pole[2]; // wrist targets and elbow poles, candy space
	Vector3 hand_dir[2], hand_up[2]; // where the fingers point and the back of the hand faces, candy space
	Vector3 foot[2]; // ankle targets, model space
	float foot_pitch[2] = { 0.0f, 0.0f }, foot_yaw[2] = { 0.0f, 0.0f };
	float jaw = JAW_GRIN;
	float tongue = 1.0f; // 0 in .. 1 lolling .. 1.35 stuck right out (times how open the mouth is)
	float tongue_swing = 0.0f, tongue_curl = 0.0f; // radians: side to side about the face normal; tip lifted out
	float ear_back = 0.15f, ear_up = 0.0f; // radians: swept back toward the tail end, tips raised
	float frown = 0.12f, brow_raise = 0.0f, brow_asym = 0.06f; // radians / metres / (left raised, right lowered)
	float coil = 0.0f; // extra curl of the tip toward its left, radians
	float wide = 1.0f, shut = 0.0f;
	float sniff = 0.0f;
};

void mix(Pose &d, const Pose &s, float w) {
	if (w <= 0.0f) {
		return;
	}
	w = saturate(w);
	d.body_off = d.body_off.lerp(s.body_off, w);
	d.head_rot = d.head_rot.lerp(s.head_rot, w);
	d.lean = d.lean.lerp(s.lean, w);
	d.stretch = lerp(d.stretch, s.stretch, w);
	for (int i = 0; i < 2; ++i) {
		d.hand[i] = d.hand[i].lerp(s.hand[i], w);
		d.pole[i] = d.pole[i].lerp(s.pole[i], w);
		d.hand_dir[i] = d.hand_dir[i].lerp(s.hand_dir[i], w);
		d.hand_up[i] = d.hand_up[i].lerp(s.hand_up[i], w);
		d.foot[i] = d.foot[i].lerp(s.foot[i], w);
		d.foot_pitch[i] = lerp(d.foot_pitch[i], s.foot_pitch[i], w);
		d.foot_yaw[i] = lerp(d.foot_yaw[i], s.foot_yaw[i], w);
	}
	d.jaw = lerp(d.jaw, s.jaw, w);
	d.tongue = lerp(d.tongue, s.tongue, w);
	d.tongue_swing = lerp(d.tongue_swing, s.tongue_swing, w);
	d.tongue_curl = lerp(d.tongue_curl, s.tongue_curl, w);
	d.ear_back = lerp(d.ear_back, s.ear_back, w);
	d.ear_up = lerp(d.ear_up, s.ear_up, w);
	d.frown = lerp(d.frown, s.frown, w);
	d.brow_raise = lerp(d.brow_raise, s.brow_raise, w);
	d.brow_asym = lerp(d.brow_asym, s.brow_asym, w);
	d.coil = lerp(d.coil, s.coil, w);
	d.wide = lerp(d.wide, s.wide, w);
	d.shut = lerp(d.shut, s.shut, w);
	d.sniff = lerp(d.sniff, s.sniff, w);
}

class GremlinAnimator : public MonsterAnimator {
public:
	float rise_time() const override { return RISE_TIME; }
	float death_time() const override { return DEATH_TIME; }
	float telegraph_gain() const override { return 1.4f; } // (final fixes 2) linear body_flash: at least ~0.9 of BEFORE's pulse (flash_metric.py)
	float hit_flash_gain() const override { return 1.3f; } // (final fixes 2) the fade after the white peak (linear body_flash)

	void bind(MonsterRig &rig) override {
		body_ = rig.slot("body");
		head_ = rig.slot("head");
		tip_ = rig.slot("tip");
		jaw_ = rig.slot("jaw");
		tongue_ = rig.slot("tongue");
		nose_ = rig.slot("nose");
		ear_[L] = rig.slot("ear_l");
		ear_[R] = rig.slot("ear_r");
		brow_[L] = rig.slot("brow_l");
		brow_[R] = rig.slot("brow_r");
		arm_[L].bind(rig, "arm_l", "forearm_l", "hand_l");
		arm_[R].bind(rig, "arm_r", "forearm_r", "hand_r");
		leg_[L].bind(rig, "leg_l", "shin_l", "foot_l");
		leg_[R].bind(rig, "leg_r", "shin_r", "foot_r");
		for (int s = 0; s < 2; ++s) {
			ankle_rest_[s] = rig.model_rest(leg_[s].end).origin;
			wrist_rest_[s] = arm_[s].wrist();
			hand_rest_dir_[s] = arm_[s].b.normalized();
			hand_rest_up_[s] = Vector3(0, 1, 0);
			hand_rest_frame_[s] = frame(hand_rest_dir_[s], hand_rest_up_[s]);
			ear_dir_[s] = rig.tip(ear_[s]).length_squared() > 1e-8f ? rig.tip(ear_[s]).normalized() : Vector3(SIDE[s], 0.5f, 0.0f).normalized();
		}
		face_.bind(rig);
		face_.max_yaw = 0.5f;
		face_.max_pitch = 0.25f;
		face_.look_height = 0.55f;
		face_.pupil_travel = 0.16f;
		face_.closed_scale = 0.08f;
		face_.glow_rest = 0.12f; // the amber ring in the iris: warm at night, never over the pupil
		face_.glow_flare = 1.0f;
		// Candy: a soft sugary sheen, low on the big bands (key light and bloom clip a strong one);
		// glossy licorice limbs; wet eyes and tongue; flat discs and brows none.
		for (int i = 0; i < rig.count(); ++i) {
			rig.gloss(i, 0.04f);
		}
		rig.param_all("rim_strength", 0.2f);
		rig.gloss(head_, 0.03f);
		rig.gloss(tip_, 0.03f);
		for (int s = 0; s < 2; ++s) {
			rig.gloss(rig.slot(s == L ? "eye_l" : "eye_r"), 0.9f);
			rig.gloss(rig.slot(s == L ? "pupil_l" : "pupil_r"), 0.0f);
			rig.gloss(brow_[s], 0.0f);
			for (int k : { arm_[s].upper, arm_[s].mid, arm_[s].end, leg_[s].upper, leg_[s].mid }) {
				rig.gloss(k, 0.3f);
			}
			rig.gloss(leg_[s].end, 0.16f);
		}
		rig.gloss(tongue_, 0.45f);
		// (final fixes) the mouth cavity is its own part: dark and matte, no rim (it read as a grey decal side on)
		const int mouth = rig.slot("mouth");
		rig.gloss(mouth, -2.0f);
		rig.param(mouth, "rim_strength", 0.0f);
		// Soft parts: the tip bends from just above its base ring; the tongue from its root.
		rig.soft(tip_, Vector3(0, 1, 0), 0.24f, 0.03f);
		rig.soft(tongue_, Vector3(0.0f, -1.0f, 0.25f).normalized(), 0.07f, 0.006f);
	}

	void update(const AnimInput &in, MonsterRig &rig, Transform3D &root) override {
		using State = MonsterAnimState;
		const float dt = anim::safe_dt(in.dt);
		const bool far = in.lod > 0;
		if (!seeded_) {
			seeded_ = true;
			sniff_timer_ = anim::RandTimer(in.seed ^ 0x5A1Fu);
			mirror_ = ((in.seed >> 4) & 1u) != 0;
			phase0_ = anim::hash01(in.seed) * TAU;
		}
		const bool dying = in.state == State::Dying, rising = in.state == State::Rise;
		const bool alive = !dying && !rising;

		// ---- clocks and one-shots
		AnimInput walk = in;
		if (!alive || in.state == State::Attack) {
			walk.speed = 0.0f; // the lunge is a leap pose, not a sprint; the dead do not walk
		}
		const float run = saturate(walk.speed / CHASE_SPEED);
		const float last_phase = gait_.phase;
		gait_.step(walk, lerp(STRIDE_WALK, STRIDE_RUN, run), 0.5f);
		const float amt = gait_.amount;
		const Vector3 recoil = hit_.update(in);
		const bool landed = in.on_floor && !was_floor_;
		was_floor_ = in.on_floor;
		const bool footfall = amt > 0.15f && int(std::floor(last_phase * 2.0f)) != int(std::floor(gait_.phase * 2.0f));

		const bool idle = in.state == State::Roam && !in.chasing && in.speed < 0.2f && in.on_floor;
		w_idle_.step(idle, 0.35f, 0.2f, dt);
		sniff_t_ += dt;
		if (!idle) {
			sniff_timer_.reset();
		} else if (sniff_t_ > 1.2f && sniff_timer_.step(dt, 1.6f, 3.4f)) {
			sniff_t_ = 0.0f;
		}

		// ---- the pose: locomotion, then the state layers
		Pose p = locomotion(in, amt, run);
		if (w_idle_.w > 0.0f) {
			mix(p, idle_pose(in, p), w_idle_.w);
		}
		w_wind_.step(in.state == State::Windup, 0.08f, 0.1f, dt);
		w_attack_.step(in.state == State::Attack, 0.03f, 0.22f, dt);
		w_recover_.step(in.state == State::Recover, 0.12f, 0.35f, dt);
		const bool rise_air = rising && in.state_t > 0.3f * RISE_TIME && in.state_t < 0.74f * RISE_TIME;
		w_air_.step((alive && !in.on_floor) || dying || rise_air, 0.07f, 0.12f, dt);
		if (in.state == State::Windup) {
			wind_ = in.windup;
		}
		if (w_wind_.w > 0.0f) {
			mix(p, windup(in, wind_), w_wind_.w);
		}
		if (in.state == State::Attack) {
			attack_t_ = in.state_t;
		}
		if (w_attack_.w > 0.0f) {
			mix(p, bite(in, attack_t_), w_attack_.w);
		}
		if (in.state == State::Recover) {
			recover_t_ = in.state_t;
		}
		if (w_recover_.w > 0.0f) {
			mix(p, recover(in, p, recover_t_), w_recover_.w);
		}
		// A flinch: thrown open (arms flung up, mouth open, tongue out, brows up, ears flung).
		const float ouch = alive ? hit_.ouch : 0.0f;
		if (ouch > 0.0f) {
			mix(p, flinch(in, p), ouch);
		}
		if (dying) {
			mix(p, death_pose(in, p), smoothstep01(in.state_t / 0.08f));
		}
		if (rising) {
			mix(p, rise_pose(in, p), 1.0f);
			rise_fade_ = 1.0f;
		} else if (rise_fade_ > 0.0f && alive) {
			// (final fixes 3) the spawn's last pose (mouth shut, ears back) eases into the living pose over
			// about 6 frames instead of dropping in one (the grin and the ears popped at the end of the rise)
			rise_fade_ = Math::max(0.0f, rise_fade_ - dt / 0.1f);
			AnimInput end = in;
			end.state_t = RISE_TIME;
			mix(p, rise_pose(end, p), smoothstep01(rise_fade_));
		}

		// ---- springs
		// Lean: forward with speed, back on a stop, with overshoot (a toy that nearly tips over).
		if (far) {
			lean_.reset(p.lean);
			bob_.reset();
		} else {
			lean_.kick(Vector3(-in.accel_local.z, 0.0f, in.accel_local.x) * (3.0f * dt) * (alive ? 1.0f : 0.0f)); // a toy: it tips back as it sets off, forward as it stops
			lean_.step(p.lean, 3.6f, 0.42f, dt);
			if (footfall) {
				const float side = gait_.phase < 0.5f ? 1.0f : -1.0f;
				bob_.kick(Vector3(0.5f, 0.0f, 0.9f * side) * amt);
				squash_.kick(-0.18f * amt);
			}
			if (hit_.fresh) {
				bob_.kick(Vector3(-in.hit_dir_local.z, 0.0f, in.hit_dir_local.x) * 4.0f);
				squash_.kick(-1.2f);
			}
			if (landed) {
				bob_.kick(Vector3(2.0f, 0.0f, 0.0f));
			}
			bob_.step(Vector3(), 4.0f, 0.25f, dt);
			if (bob_.x.length() > 0.3f) {
				bob_.x = bob_.x.normalized() * 0.3f;
			}
		}
		const Vector3 scale3 = squash_.update(in, p.stretch);
		const float tremble = w_wind_.w * smoothstep01(remap01(wind_, 0.1f, 0.4f)) * (1.0f - smoothstep01(remap01(wind_, 0.85f, 1.0f)));

		// ---- solve: hips, candy (with the look), face
		rig.pose(body_, Basis(), p.body_off);
		const bool intent = in.state == State::Windup || in.state == State::Attack;
		face_.look_weight = intent ? 1.0f : (alive ? -1.0f : 0.0f);
		if (idle) {
			face_.look_weight = -1.0f; // standing about: it faces where it is going (no one to watch)
		}
		face_.head_lag = anim::damp(face_.head_lag, intent ? 0.04f : 0.15f, 0.08f, dt);
		face_.wide = p.wide;
		face_.shut = p.shut;
		face_.update(in, rig);
		Vector3 hr = p.head_rot;
		hr.z += 0.03f * tremble * std::sin(in.t * 43.0f);
		const Basis head_basis = Basis::from_euler(anim::look_euler(face_.head)) * Basis::from_euler(hr) * Basis(anim::quat_from_rotvec(bob_.x));
		rig.pose(head_, head_basis, Vector3(), scale3);

		// jaw, tongue, nose, brows
		const float jaw = Math::clamp(p.jaw, -0.008f, 0.11f);
		rig.pose(jaw_, Basis(Vector3(1, 0, 0), jaw));
		const float open = saturate((jaw - 0.006f) / 0.03f);
		const float tsc = Math::max(0.02f, smoothstep01(open) * p.tongue);
		rig.pose(tongue_, Basis(Vector3(0, 0, 1), p.tongue_swing), Vector3(), Vector3(tsc, tsc, tsc));
		rig.pose(nose_, Basis(Vector3(1, 0, 0), -0.25f * p.sniff), Vector3(0.0f, 0.004f * p.sniff, 0.002f * p.sniff),
				Vector3(1.0f + 0.14f * p.sniff, 1.0f - 0.08f * p.sniff, 1.0f + 0.06f * p.sniff));
		for (int s = 0; s < 2; ++s) {
			// brow_l: + turns the outer end up (inner end down: a frown); brow_r the other way
			const float asym = SIDE[s] * p.brow_asym * (mirror_ ? -1.0f : 1.0f);
			rig.pose(brow_[s], Basis(Vector3(0, 0, 1), SIDE[s] * (p.frown - asym * 2.0f)), Vector3(0.0f, p.brow_raise + 0.012f * asym, 0.0f));
		}

		// ---- arms (candy space)
		for (int s = 0; s < 2; ++s) {
			const Basis fore = arm_[s].solve(rig, Vector3(), p.hand[s], p.pole[s]);
			const Basis want = frame(p.hand_dir[s], p.hand_up[s]) * hand_rest_frame_[s].transposed();
			rig.pose(arm_[s].end, fore.inverse() * want);
		}

		// ---- legs (feet planted in model space)
		const Transform3D body_model = rig.model_xf(body_);
		const Transform3D to_body = body_model.affine_inverse();
		const Vector3 knee_hinge(1, 0, 0);
		const float air = w_air_.w;
		for (int s = 0; s < 2; ++s) {
			Vector3 foot = p.foot[s];
			float pitch = p.foot_pitch[s];
			const Vector3 hip = body_model.xform(leg_[s].root);
			if (air > 0.0f) { // off the ground: the little legs pedal
				const float kick = std::sin(in.t * (dying ? 22.0f : 16.0f) + float(s) * PI);
				const Vector3 hang = hip + Vector3(SIDE[s] * (0.03f + 0.04f * ouch), -0.12f + 0.025f * kick, 0.03f + 0.035f * kick);
				foot = foot.lerp(hang, air);
				pitch = lerp(pitch, 0.5f + 0.3f * kick, air);
			}
			const Vector3 pole = hip + Vector3(SIDE[s] * 0.12f, -0.02f, 0.5f);
			const Basis shin = body_model.basis * leg_[s].solve(rig, knee_hinge, to_body.xform(foot), to_body.xform(pole));
			const Basis want = Basis(Vector3(0, 1, 0), p.foot_yaw[s]) * Basis(Vector3(1, 0, 0), pitch);
			rig.pose(leg_[s].end, shin.inverse() * want);
		}

		// ---- ears, tip and tongue: springs on top of their posed angles
		for (int s = 0; s < 2; ++s) {
			// ear_l: + about Y sweeps the tip back, + about Z lifts it; ear_r mirrored
			const Basis target = Basis(Vector3(0, 1, 0), SIDE[s] * p.ear_back) * Basis(Vector3(0, 0, 1), SIDE[s] * p.ear_up);
			Vector3 bend;
			if (!far) {
				if (hit_.fresh) {
					ears_[s].kick(Vector3(0.0f, 2.5f, 0.0f) + Basis(Vector3(0, 1, 0), in.yaw).xform(in.hit_dir_local) * 1.5f);
				}
				if (footfall) {
					ears_[s].kick(Vector3(0.0f, -0.35f * amt, 0.0f));
				}
				anim::DangleParams ep;
				ep.len = 0.2f;
				ep.hz = 4.2f;
				ep.zeta = 0.28f;
				ep.drag = 0.15f;
				ep.limit = 0.6f;
				bend = ears_[s].step(rig.world_rest(ear_[s]), ear_dir_[s], ep, dt);
			} else {
				ears_[s].reset();
			}
			rig.pose(ear_[s], Basis(anim::quat_from_rotvec(bend)) * target);
		}
		Vector3 tip_bend(0.0f, 0.0f, -p.coil);
		Vector3 tongue_bend(-p.tongue_curl, 0.0f, 0.0f);
		if (!far) {
			if (hit_.fresh) {
				tip_d_.kick(Basis(Vector3(0, 1, 0), in.yaw).xform(in.hit_dir_local) * 2.5f + Vector3(0, 1.0f, 0));
				tongue_d_.kick(Vector3(0, 1.5f, 0));
			}
			if (landed) {
				tip_d_.kick(Vector3(0, -1.5f, 0));
			}
			anim::DangleParams tp;
			tp.len = 0.22f;
			tp.hz = 2.6f;
			tp.zeta = 0.22f;
			tp.drag = 0.3f;
			tp.limit = 0.75f;
			tip_bend += tip_d_.step(rig.world_rest(tip_), Vector3(0, 1, 0), tp, dt);
			anim::DangleParams gp;
			gp.len = 0.06f;
			gp.hz = 4.5f;
			gp.zeta = 0.3f;
			gp.gravity = 6.0f;
			gp.limit = 0.6f;
			tongue_bend += tongue_d_.step(rig.world_rest(tongue_), Vector3(0.0f, -1.0f, 0.25f).normalized(), gp, dt);
			tip_bend.z += 0.06f * tremble * std::sin(in.t * 47.0f);
		} else {
			tip_d_.reset();
			tongue_d_.reset();
		}
		rig.bend(tip_, tip_bend);
		rig.bend(tongue_, tongue_bend);

		// ---- the root: facing, lean (springy), the flinch, rise and death
		Vector3 lean = lean_.x + recoil * 0.8f;
		Vector3 offset;
		float rscale = 1.0f;
		if (rising) {
			rise_root(in, lean, offset, rscale);
		} else if (dying) {
			death_root(in, lean, offset, rscale);
		}
		if (rising || dying) {
			// Spawned a little above the ground and dropping: keep the picture on the ground meanwhile.
			float drop = 0.0f;
			if (!in.on_floor && in.ground && *in.ground) {
				drop = Math::clamp(in.origin_world.y - (*in.ground)(in.origin_world.x, in.origin_world.z), 0.0f, 0.45f);
			}
			drop_ = in.on_floor ? anim::damp(drop_, 0.0f, 0.03f, dt) : drop;
			offset.y -= drop_;
		}
		root = root_pose(in.yaw, lean, Vector3(rscale, rscale, rscale), offset);
	}

private:
	// Idle to a fast patter: short quick steps by distance, a side-to-side waddle, the candy leaning
	// into the run, ears swept back by the speed, arms pumping with the claws up.
	Pose locomotion(const AnimInput &in, float amt, float run) const {
		Pose p;
		const float ph = gait_.phase * TAU;
		const float sn = std::sin(ph), cs = std::cos(ph);
		const float bounce = std::fabs(sn);
		const float breath = std::sin(in.t * 2.3f + phase0_);
		const float stride = lerp(STRIDE_WALK, STRIDE_RUN, run);
		p.body_off = Vector3(0.006f * amt * sn, -0.004f + 0.016f * amt * bounce + 0.003f * breath * (1.0f - amt), 0.0f);
		p.head_rot = Vector3(0.04f * amt + 0.012f * breath * (1.0f - amt), -0.08f * amt * cs, 0.17f * amt * sn * (1.0f - 0.35f * run));
		p.lean = Vector3(0.1f * amt + 0.17f * run, 0.0f, 0.0f);
		p.stretch = 1.0f + 0.012f * breath * (1.0f - amt);
		for (int s = 0; s < 2; ++s) {
			float f = gait_.phase + (s == L ? 0.0f : 0.5f);
			f -= std::floor(f);
			float z, lift = 0.0f, pitch;
			if (f < 0.5f) {
				z = stride * (0.25f - f);
				pitch = -0.25f * (1.0f - saturate(f / 0.1f));
			} else {
				const float u = (f - 0.5f) * 2.0f;
				z = stride * (-0.25f + 0.5f * smoothstep01(u));
				lift = STEP_H * std::sin(PI * u);
				pitch = u < 0.5f ? 0.6f * std::sin(TAU * u) : -0.25f * std::sin(PI * (u - 0.5f));
			}
			p.foot[s] = ankle_rest_[s] + Vector3(SIDE[s] * STANCE, lift * amt, z * amt);
			p.foot_pitch[s] = pitch * amt;
			p.foot_yaw[s] = SIDE[s] * TOE_OUT;
		}
		// Arms: walking, a loose swing against the legs; running, claws up and forward, pumping.
		for (int s = 0; s < 2; ++s) {
			const float swing = (s == L ? -cs : cs);
			const Vector3 hang = wrist_rest_[s] + Vector3(SIDE[s] * 0.01f, 0.01f + 0.004f * breath, -0.03f) + Vector3(0.0f, 0.012f * bounce, 0.05f * swing) * amt;
			// (final fixes) both arms pump, forward and back against the legs, the hands below the cheeks and out
			// to the sides (a small swing at cheek height read as one hand pressed to the face)
			// (final fixes 2) closer in (the arm is 0.235 m: at 0.15 out the target was 90% of the reach, so the
			// arms stayed straight out to the sides and the fore-aft swing hardly showed): elbows bent, the hand
			// rising as it swings forward and dropping back, so the pump reads from the front too
			// (final fixes 2, round 2) still read as the far arm held straight out: the hands come in toward the
			// belly (the far one tucks behind the candy at the back of its swing), the pump is bigger and more
			// up-and-down (it reads from any side), the elbows point down
			const Vector3 claws = arm_[s].root + Vector3(SIDE[s] * 0.0f, -0.075f + 0.06f * swing, 0.05f + 0.11f * swing); // (peak below the cheek)
			p.hand[s] = hang.lerp(claws, smoothstep01(run));
			// (the elbows down and back while running, not out to the side: the far arm read as held out straight)
			p.pole[s] = arm_[s].root + Vector3(SIDE[s] * 0.3f, -0.2f, -0.15f).lerp(Vector3(SIDE[s] * 0.1f, -0.4f, -0.12f), smoothstep01(run));
			const Vector3 droop = (hand_rest_dir_[s] + Vector3(0.0f, -0.5f, 0.0f)).normalized();
			const Vector3 up_claw = Vector3(SIDE[s] * 0.25f, 0.45f + 0.15f * swing, 0.85f).normalized();
			p.hand_dir[s] = droop.lerp(up_claw, smoothstep01(run));
			p.hand_up[s] = Vector3(0, 1, 0).lerp(Vector3(0.0f, -0.3f, 1.0f), smoothstep01(run));
		}
		// The jaw: a grin; panting with the tongue out on a run.
		p.jaw = JAW_GRIN + 0.022f * run + 0.012f * run * std::sin(in.t * TAU * 5.0f);
		p.tongue = 1.0f + 0.25f * run;
		p.tongue_curl = -0.25f * run; // flaps back over the chin
		p.ear_back = 0.15f + 0.85f * run + 0.25f * amt;
		p.ear_up = -0.05f - 0.15f * run;
		p.frown = 0.12f + 0.15f * run;
		return p;
	}

	// Standing about: rubbing its hands together, hunched a little, grinning; now and then a burst of
	// sniffs (nose up, eyes half shut).
	Pose idle_pose(const AnimInput &in, const Pose &base) const {
		Pose p = base;
		const float rub = std::sin(in.t * TAU * 2.6f);
		const float rub2 = std::sin(in.t * TAU * 2.6f + 1.7f);
		for (int s = 0; s < 2; ++s) {
			const float k = s == L ? rub : -rub;
			p.hand[s] = Vector3(SIDE[s] * (0.1f + 0.006f * rub2), -0.005f + 0.014f * k, 0.305f + 0.012f * k); // in front of the belly, below the mouth
			p.pole[s] = arm_[s].root + Vector3(SIDE[s] * 0.35f, -0.25f, -0.1f);
			p.hand_dir[s] = Vector3(-SIDE[s] * 0.85f, 0.25f + 0.15f * k, 0.45f).normalized();
			p.hand_up[s] = Vector3(SIDE[s] * 0.2f, 0.4f, 1.0f).normalized();
		}
		p.head_rot.x += 0.08f;
		p.head_rot.z += 0.025f * rub2;
		p.jaw = JAW_GRIN + 0.012f;
		p.frown = 0.22f;
		p.brow_asym = 0.12f; // one brow up: scheming
		p.ear_back = 0.05f;
		p.ear_up = 0.05f;
		// sniffs: three quick ones in 0.6 s
		const float burst = anim::smooth_pulse(sniff_t_, 0.08f, 0.5f, 0.2f);
		if (burst > 0.0f) {
			const float sn = std::pow(std::fabs(std::sin(sniff_t_ * TAU * 4.6f)), 3.0f);
			p.sniff = burst * (0.4f + 0.6f * sn);
			p.head_rot.x -= burst * (0.16f + 0.04f * sn); // nose up into the air
			p.shut = 0.45f * burst;
			p.jaw = JAW_GRIN * (1.0f - 0.8f * burst);
			p.ear_up += 0.25f * burst;
		}
		return p;
	}

	// The wind-up (0.6 s): crouch on bent knees, the candy pulled back, jaw wide, brows down, claws
	// up and spread, ears flat, tip coiled, trembling; in the last fifth it tips forward, cocked.
	Pose windup(const AnimInput &in, float wind) const {
		Pose p;
		const float c = smoothstep01(remap01(wind, 0.0f, 0.35f));
		const float go = smoothstep01(remap01(wind, 0.8f, 1.0f));
		const float tr = std::sin(in.t * 41.0f) * c * (1.0f - go);
		p.body_off = Vector3(0.0f, -0.05f * c - 0.01f * go, -0.015f * c);
		p.head_rot = Vector3(-0.12f * c + 0.36f * go, 0.0f, 0.02f * tr);
		p.lean = Vector3(-0.03f * c + 0.22f * go, 0.0f, 0.0f);
		p.stretch = lerp(1.0f, 0.88f, c) + 0.08f * go;
		for (int s = 0; s < 2; ++s) {
			p.foot[s] = ankle_rest_[s] + Vector3(SIDE[s] * (STANCE + 0.03f * c), 0.0f, (s == L ? 0.04f : -0.05f) * c);
			p.foot_yaw[s] = SIDE[s] * (TOE_OUT + 0.15f * c);
			p.hand[s] = arm_[s].root + Vector3(SIDE[s] * 0.19f, 0.09f + 0.005f * tr, -0.09f + 0.15f * go); // out and back: the face stays clear from the side
			p.pole[s] = arm_[s].root + Vector3(SIDE[s] * 0.3f, -0.3f, -0.2f);
			p.hand_dir[s] = Vector3(SIDE[s] * 0.55f, 0.85f, 0.1f + 0.6f * go).normalized();
			p.hand_up[s] = Vector3(-SIDE[s] * 0.2f, 0.0f, -1.0f);
		}
		p.jaw = lerp(JAW_GRIN, JAW_WIDE, smoothstep01(remap01(wind, 0.1f, 0.55f))) + 0.004f * tr;
		p.tongue = 0.35f;
		p.ear_back = 0.95f;
		p.ear_up = -0.3f;
		p.frown = 0.42f;
		p.brow_raise = -0.008f * c;
		p.brow_asym = 0.0f;
		p.coil = 0.22f * c;
		p.wide = 1.0f + 0.12f * c;
		return p;
	}

	// The lunge (0.25 s): the whole candy stretches out at the target, claws reaching, and the jaw
	// snaps shut at 60 %; then it lands squashed with the jaw clamped.
	Pose bite(const AnimInput &in, float t) const {
		Pose p;
		const float u = saturate(t / LUNGE_TIME);
		const float snap = smoothstep01(remap01(u, BITE_AT - 0.04f, BITE_AT + 0.1f));
		const float out = std::sin(PI * saturate(u * 1.15f));
		p.body_off = Vector3(0.0f, -0.035f + 0.02f * out, 0.02f);
		p.head_rot = Vector3(0.3f * out + 0.12f * snap, 0.0f, 0.0f);
		p.lean = Vector3(0.36f + 0.1f * out, 0.0f, 0.0f);
		p.stretch = lerp(1.3f, 0.88f, snap);
		for (int s = 0; s < 2; ++s) {
			const bool front = s == L;
			p.foot[s] = ankle_rest_[s] + Vector3(SIDE[s] * STANCE, front ? 0.03f * out : 0.0f, front ? 0.08f : -0.12f);
			p.foot_pitch[s] = front ? -0.2f : 0.5f;
			p.foot_yaw[s] = SIDE[s] * TOE_OUT;
			// (final fixes) the claws reach forward low and outside the face, then swipe down and in BELOW the chin
			// (reaching at mouth height, the front arm cut across the open mouth in the front cameras)
			p.hand[s] = arm_[s].root + Vector3(SIDE[s] * 0.05f, -0.05f, 0.2f).lerp(Vector3(SIDE[s] * 0.01f, -0.12f, 0.17f), snap);
			p.pole[s] = arm_[s].root + Vector3(SIDE[s] * 0.35f, -0.1f, -0.25f);
			p.hand_dir[s] = Vector3(-SIDE[s] * 0.1f, 0.2f, 1.0f).normalized().lerp(Vector3(-SIDE[s] * 0.45f, -0.55f, 0.7f).normalized(), snap);
			p.hand_up[s] = Vector3(0, 1, 0);
		}
		// (final fixes 3) the jaw closes over about 5 frames around the snap (it slid shut in one frame and
		// read as the open grin swapped for the closed mouth); the body's snap keeps its own timing
		const float shut = smoothstep01(remap01(u, BITE_AT - 0.18f, BITE_AT + 0.18f));
		p.jaw = lerp(JAW_WIDE, -0.006f, shut);
		p.tongue = 0.2f;
		p.ear_back = 1.2f;
		p.ear_up = -0.2f;
		p.frown = 0.45f;
		p.brow_asym = 0.0f;
		p.coil = -0.15f;
		p.wide = 1.12f;
		return p;
	}

	// Recover: a shake of the head (ears flapping), then it licks its lips, pleased with itself.
	Pose recover(const AnimInput &in, const Pose &base, float t) const {
		Pose p = base;
		const float shake = anim::smooth_pulse(t - 0.05f, 0.06f, 0.25f, 0.15f);
		if (shake > 0.0f) {
			const float k = std::exp(-3.0f * Math::max(0.0f, t - 0.05f));
			p.head_rot.y += 0.32f * shake * k * std::sin((t - 0.05f) * TAU * 6.5f);
			p.head_rot.z += 0.08f * shake * k * std::sin((t - 0.05f) * TAU * 6.5f + 1.0f);
			p.shut = Math::max(p.shut, 0.75f * shake);
			p.jaw = lerp(p.jaw, 0.012f, shake);
			p.ear_up += 0.2f * shake;
		}
		const float lick = anim::smooth_pulse(t - 0.55f, 0.12f, 0.75f, 0.2f);
		if (lick > 0.0f) {
			const float l = t - 0.55f;
			p.jaw = lerp(p.jaw, 0.052f, lick);
			p.tongue = lerp(p.tongue, 1.35f, lick);
			p.tongue_swing = 0.5f * lick * std::sin(l * TAU * 1.6f);
			p.tongue_curl = 0.35f * lick;
			p.head_rot.x += -0.06f * lick;
			p.head_rot.z += -0.1f * lick * std::sin(l * TAU * 1.6f);
			p.frown = lerp(p.frown, 0.0f, lick);
			p.brow_raise = 0.006f * lick;
			p.brow_asym = 0.0f;
			p.shut = Math::max(p.shut, 0.35f * lick);
		}
		const float gloat = anim::smooth_pulse(t - 1.45f, 0.2f, 1.0f, 0.4f);
		p.brow_asym = lerp(p.brow_asym, 0.16f, gloat);
		p.jaw = lerp(p.jaw, JAW_GRIN + 0.012f, gloat);
		return p;
	}

	Pose flinch(const AnimInput &in, const Pose &base) const {
		Pose h = base;
		const float lop = mirror_ ? -1.0f : 1.0f;
		for (int s = 0; s < 2; ++s) {
			h.hand[s] = arm_[s].root + Vector3(SIDE[s] * 0.15f, 0.17f + 0.03f * lop * SIDE[s], -0.02f);
			h.pole[s] = arm_[s].root + Vector3(SIDE[s] * 0.3f, -0.2f, -0.2f);
			h.hand_dir[s] = Vector3(SIDE[s] * 0.5f, 1.0f, -0.1f).normalized();
			h.hand_up[s] = Vector3(0, 0, -1);
		}
		h.head_rot += Vector3(-0.18f, 0.0f, 0.12f * lop);
		h.stretch = 0.85f;
		h.jaw = 0.075f;
		h.tongue = 1.3f;
		h.tongue_curl = 0.3f;
		h.ear_back = -0.2f;
		h.ear_up = 0.45f;
		h.frown = -0.35f;
		h.brow_raise = 0.012f;
		h.brow_asym = 0.0f;
		h.wide = 1.0f;
		return h;
	}

	// Death (0.6 s): a jolt (mouth and eyes wide, arms up, a little hop), then it topples over onto its
	// back with its legs kicking, tongue lolling, eyes shut, tip drooping; at the end it winks out.
	Pose death_pose(const AnimInput &in, const Pose &base) const {
		Pose p = base;
		const float k = saturate(in.state_t / DEATH_TIME);
		const float jolt = 1.0f - smoothstep01(remap01(k, 0.12f, 0.35f));
		for (int s = 0; s < 2; ++s) {
			p.hand[s] = arm_[s].root + Vector3(SIDE[s] * 0.14f, 0.16f * jolt + 0.04f, 0.04f);
			p.pole[s] = arm_[s].root + Vector3(SIDE[s] * 0.3f, -0.2f, -0.2f);
			p.hand_dir[s] = Vector3(SIDE[s] * 0.5f, 1.0f, 0.1f).normalized();
			p.hand_up[s] = Vector3(0, 0, -1);
		}
		p.stretch = lerp(0.9f, 1.18f, jolt);
		p.jaw = lerp(0.06f, 0.1f, jolt);
		p.tongue = 1.35f;
		p.tongue_curl = -0.4f;
		p.ear_back = -0.1f;
		p.ear_up = lerp(-0.5f, 0.5f, jolt);
		p.frown = lerp(-0.2f, -0.4f, jolt);
		p.brow_raise = 0.012f * jolt;
		p.brow_asym = 0.0f;
		p.coil = -0.5f * (1.0f - jolt);
		p.wide = 1.0f + 0.3f * jolt;
		p.shut = smoothstep01(remap01(k, 0.3f, 0.45f));
		return p;
	}

	void death_root(const AnimInput &in, Vector3 &lean, Vector3 &offset, float &rscale) {
		const float k = saturate(in.state_t / DEATH_TIME);
		const float hop = 0.07f * anim::arc(remap01(k, 0.0f, 0.3f));
		const float fall = remap01(k, 0.18f, 0.62f);
		const float tip = (PI * 0.5f) * (fall < 1.0f ? fall * fall : 1.0f) - 0.12f * std::sin(PI * remap01(k, 0.62f, 0.78f)); // a bounce on landing
		const float side = mirror_ ? -0.25f : 0.25f;
		lean = Vector3(-tip, 0.0f, side * tip);
		offset = Vector3(0.0f, hop + 0.26f * std::sin(Math::min(tip, PI * 0.5f)), 0.0f);
		offset = Basis(Vector3(0, 1, 0), in.yaw).xform(Vector3(0.0f, offset.y, -0.08f * std::sin(Math::min(tip, PI * 0.5f))));
		rscale = 1.0f - 0.9f * smoothstep01(remap01(k, 0.88f, 1.0f));
	}

	// Spawn (0.6 s): only the cream tip shows, wiggling out of the ground; it bursts up into the air
	// stretched, arms and ears flung out, lands squashed and pops its eyes open.
	Pose rise_pose(const AnimInput &in, const Pose &base) const {
		Pose p = base;
		const float k = saturate(in.state_t / RISE_TIME);
		const float up = smoothstep01(remap01(k, 0.32f, 0.6f)) * (1.0f - smoothstep01(remap01(k, 0.7f, 0.85f)));
		for (int s = 0; s < 2; ++s) {
			p.hand[s] = p.hand[s].lerp(arm_[s].root + Vector3(SIDE[s] * 0.16f, 0.12f, 0.02f), up);
			p.hand_dir[s] = p.hand_dir[s].lerp(Vector3(SIDE[s] * 0.6f, 0.8f, 0.2f).normalized(), up);
		}
		p.stretch = k < 0.32f ? 0.85f : lerp(1.0f, 1.25f, up);
		p.ear_back = lerp(0.9f, -0.2f, up);
		p.ear_up = lerp(-0.5f, 0.4f, up);
		p.jaw = lerp(0.0f, 0.08f, up);
		p.tongue = 0.6f;
		p.coil = k < 0.32f ? 0.25f * std::sin(in.t * 26.0f) : 0.0f;
		p.shut = 1.0f - smoothstep01(remap01(k, 0.75f, 0.86f));
		p.wide = 1.0f + 0.22f * anim::pulse(k - 0.84f, 0.03f, 0.12f); // the eyes pop open
		p.frown = -0.2f * up;
		return p;
	}

	void rise_root(const AnimInput &in, Vector3 &lean, Vector3 &offset, float &rscale) {
		const float k = saturate(in.state_t / RISE_TIME);
		float y;
		if (k < 0.32f) {
			y = -0.75f + 0.12f * smoothstep01(k / 0.32f) + 0.015f * std::sin(in.t * 31.0f);
		} else if (k < 0.58f) {
			y = lerp(-0.63f, 0.14f, anim::ease_out(remap01(k, 0.32f, 0.58f)));
		} else if (k < 0.74f) {
			const float u = remap01(k, 0.58f, 0.74f);
			y = 0.14f * (1.0f - u * u);
		} else {
			y = 0.0f;
			if (!rise_landed_) {
				rise_landed_ = true;
				squash_.kick(-2.2f);
			}
		}
		lean = Vector3(0.0f, 0.0f, k < 0.32f ? 0.12f * std::sin(in.t * 19.0f) : 0.0f);
		offset = Vector3(0.0f, y, 0.0f);
		rscale = 1.0f;
	}

	float rise_fade_ = 0.0f; // (final fixes 3) weight of the spawn's end pose after the rise (eases to 0)
	int body_ = -1, head_ = -1, tip_ = -1, jaw_ = -1, tongue_ = -1, nose_ = -1;
	int ear_[2] = { -1, -1 }, brow_[2] = { -1, -1 };
	Limb arm_[2], leg_[2];
	Vector3 ankle_rest_[2], wrist_rest_[2], hand_rest_dir_[2], hand_rest_up_[2], ear_dir_[2];
	Basis hand_rest_frame_[2];
	MonsterFace face_;
	HitRecoil hit_;
	Gait gait_;
	RootSquash squash_;
	anim::Weight w_idle_, w_wind_, w_attack_, w_recover_, w_air_;
	anim::Spring3 lean_, bob_;
	anim::Dangle ears_[2], tip_d_, tongue_d_;
	anim::RandTimer sniff_timer_;
	float wind_ = 0.0f, attack_t_ = 0.0f, recover_t_ = 0.0f, sniff_t_ = 10.0f, drop_ = 0.0f, phase0_ = 0.0f;
	bool was_floor_ = true, seeded_ = false, mirror_ = false, rise_landed_ = false;
};

} // namespace

TOT_MONSTER_ANIMATOR(MonsterType::Gremlin, GremlinAnimator)

} // namespace tot
