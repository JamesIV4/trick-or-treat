// Toadstool Imp (a PUFF monster): the animator for its v2 rig.
//
// Part tree (art/team/monsters/batch04/build_monsters2.py, toadstool()):
//   body (meshless hip joint) > head (the cream stalk with the face)
//        > cap (red dome, spot pads, gills) > sprout (soft: stem and two leaves, one mesh)
//        > eye_l / eye_r > pupil_l / pupil_r, brow_l / brow_r, mouth (scaled open about the lip line),
//          cheek_l / cheek_r (blush pads that puff), alt_squint_l / alt_squint_r (shut-eye chevrons),
//          arm_l / arm_r > hand_l / hand_r
//   body > foot_l / foot_r
// The body joint stays at rest, so the feet are posed in model space; the stalk waddles, leans and
// squashes about the hips. The cap's built-in tilt back (12 degrees) is its rest; it rides on a
// Dangle (a heavy inverted pendulum: it lags and bobbles), a vertical bounce spring and authored
// poses on top.
//
// A frame: a locomotion pose (idle sway and peeks to a pattering waddle), state layers mixed over it
// by weight (wind-up, puff, recover, hit), then springs (stalk squash, cap, sprout, arms), then one
// solve. Rise and death are poses of the root and the cap (out of the ground; the cap spinning
// down like a coin while the stalk ducks into the ground).

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
constexpr float CHASE_SPEED = 1.5f; // tuning.speed: the gait is fullest there (it wanders at half)
constexpr float STRIDE_WALK = 0.24f, STRIDE_RUN = 0.32f; // metres per full cycle (two steps): a patter
constexpr float STEP_H = 0.035f;
constexpr float RISE_TIME = 0.6f, DEATH_TIME = 0.6f;
constexpr float PUFF_TIME = 0.3f; // the attack state's length (monsters.cpp: default attack_step)
constexpr float HIT_TIME = 0.95f; // the visual hit reaction (cap over the eyes, pushed back up); Hurt itself is 0.28 s
constexpr float CAP_TILT = 0.2094f; // the cap's built-in tilt back (radians)
constexpr float CAP_PIVOT_Y = 0.625f; // model height of the cap's pivot at rest
constexpr float HIP_Y = 0.1f; // model height of the stalk's pivot (the hips)
constexpr float CAP_RIM_R = 0.461f, CAP_UNDER = 0.076f; // rim radius, how far the rim hangs below the pivot

struct Pose {
	Vector3 head_off; // the stalk's pivot (hips), model space, on top of rest
	Vector3 head_rot; // Euler (pitch, yaw, roll) of the stalk about the hips, on top of the look
	float stretch = 1.0f; // stalk squash (< 1) / stretch (> 1), volume preserving
	Vector3 lean; // root lean (rotation vector, facing frame)
	Vector3 cap_rot; // Euler (pitch +forward, yaw, roll) of the cap about its pivot, on top of its rest tilt
	Vector3 cap_off; // cap pivot offset, stalk space
	Vector3 cap_scale = Vector3(1, 1, 1);
	float arm_raise[2] = { 0.1f, 0.1f }, arm_fwd[2] = { 0.0f, 0.0f }, hand_bend[2] = { 0.0f, 0.0f };
	Vector3 foot[2]; // offsets from rest, model space
	float foot_pitch[2] = { 0.0f, 0.0f }; // + tips the toe down
	float mouth = 0.0f, mouth_round = 0.0f; // 0 shut (just the lip line) .. 1 wide open; round narrows it to an O
	float puff = 0.0f; // cheeks blown out
	float frown = -0.05f, brow_raise = 0.0f;
	float squint = 0.0f; // > 0.5 shows the shut-eye chevrons
	float shut = 0.0f, wide = 1.0f;
};

void mix(Pose &d, const Pose &s, float w) {
	if (w <= 0.0f) {
		return;
	}
	w = saturate(w);
	d.head_off = d.head_off.lerp(s.head_off, w);
	d.head_rot = d.head_rot.lerp(s.head_rot, w);
	d.stretch = lerp(d.stretch, s.stretch, w);
	d.lean = d.lean.lerp(s.lean, w);
	d.cap_rot = d.cap_rot.lerp(s.cap_rot, w);
	d.cap_off = d.cap_off.lerp(s.cap_off, w);
	d.cap_scale = d.cap_scale.lerp(s.cap_scale, w);
	for (int i = 0; i < 2; ++i) {
		d.arm_raise[i] = lerp(d.arm_raise[i], s.arm_raise[i], w);
		d.arm_fwd[i] = lerp(d.arm_fwd[i], s.arm_fwd[i], w);
		d.hand_bend[i] = lerp(d.hand_bend[i], s.hand_bend[i], w);
		d.foot[i] = d.foot[i].lerp(s.foot[i], w);
		d.foot_pitch[i] = lerp(d.foot_pitch[i], s.foot_pitch[i], w);
	}
	d.mouth = lerp(d.mouth, s.mouth, w);
	d.mouth_round = lerp(d.mouth_round, s.mouth_round, w);
	d.puff = lerp(d.puff, s.puff, w);
	d.frown = lerp(d.frown, s.frown, w);
	d.brow_raise = lerp(d.brow_raise, s.brow_raise, w);
	d.squint = lerp(d.squint, s.squint, w);
	d.shut = lerp(d.shut, s.shut, w);
	d.wide = lerp(d.wide, s.wide, w);
}

class ToadstoolAnimator : public MonsterAnimator {
public:
	float rise_time() const override { return RISE_TIME; }
	float death_time() const override { return DEATH_TIME; }
	float telegraph_gain() const override { return 1.3f; } // (final fixes 2) linear body_flash: at least ~0.9 of BEFORE's pulse (flash_metric.py)
	float hit_flash_gain() const override { return 1.1f; } // (final fixes 2) the fade after the white peak (linear body_flash)

	void bind(MonsterRig &rig) override {
		body_ = rig.slot("body");
		head_ = rig.slot("head");
		cap_ = rig.slot("cap");
		sprout_ = rig.slot("sprout");
		mouth_ = rig.slot("mouth");
		static const char *names[2][9] = {
			{ "eye_l", "pupil_l", "brow_l", "cheek_l", "alt_squint_l", "arm_l", "hand_l", "foot_l", "" },
			{ "eye_r", "pupil_r", "brow_r", "cheek_r", "alt_squint_r", "arm_r", "hand_r", "foot_r", "" },
		};
		for (int s = 0; s < 2; ++s) {
			eye_[s] = rig.slot(names[s][0]);
			pupil_[s] = rig.slot(names[s][1]);
			brow_[s] = rig.slot(names[s][2]);
			cheek_[s] = rig.slot(names[s][3]);
			squint_[s] = rig.slot(names[s][4]);
			arm_[s] = rig.slot(names[s][5]);
			hand_[s] = rig.slot(names[s][6]);
			foot_[s] = rig.slot(names[s][7]);
		}
		face_.bind(rig);
		face_.max_yaw = 0.5f;
		face_.max_pitch = 0.12f; // the stalk pitches about its hips: a little goes a long way
		face_.look_height = 0.5f;
		face_.pupil_travel = 0.16f;
		face_.closed_scale = 0.08f;
		face_.glow_rest = -1.0f; // the eye glow is driven here (dim amber ring, flaring through the wind-up)
		// Matte, organic surfaces; a soft sheen on the cap (key light and bloom clip a strong one on
		// the big red dome); wet eyes and mouth; glossy leaves.
		for (int i = 0; i < rig.count(); ++i) {
			rig.gloss(i, -0.45f);
		}
		rig.param_all("rim_strength", 0.15f);
		rig.gloss(cap_, -0.55f);
		rig.gloss(sprout_, 0.15f);
		rig.gloss(mouth_, 0.35f);
		for (int s = 0; s < 2; ++s) {
			rig.gloss(eye_[s], 0.9f);
			rig.gloss(pupil_[s], 0.0f);
			rig.gloss(cheek_[s], -0.6f);
		}
		// The sprout bends from just above where it is sunk into the cap, along the cap's up axis
		// (tipped back with the cap). Hidden until needed: the squint chevrons.
		sprout_up_ = Vector3(0.0f, std::cos(CAP_TILT), -std::sin(CAP_TILT));
		rig.soft(sprout_, sprout_up_, 0.17f, 0.012f);
		for (int s = 0; s < 2; ++s) {
			rig.show(squint_[s], false);
		}
	}

	void update(const AnimInput &in, MonsterRig &rig, Transform3D &root) override {
		using State = MonsterAnimState;
		const float dt = anim::safe_dt(in.dt);
		const bool far = in.lod > 0;
		if (!seeded_) {
			seeded_ = true;
			peek_timer_ = anim::RandTimer(in.seed ^ 0x70AD5u);
			mirror_ = ((in.seed >> 5) & 1u) != 0;
			phase0_ = anim::hash01(in.seed) * TAU;
		}
		const bool dying = in.state == State::Dying, rising = in.state == State::Rise;
		const bool alive = !dying && !rising;

		// ---- clocks and one-shots
		AnimInput walk = in;
		if (!alive || in.state == State::Attack || in.state == State::Windup) {
			walk.speed = 0.0f;
		}
		const float run = saturate(walk.speed / CHASE_SPEED);
		const float last_phase = gait_.phase;
		gait_.step(walk, lerp(STRIDE_WALK, STRIDE_RUN, run), 0.6f);
		const float amt = gait_.amount;
		const Vector3 recoil = hit_.update(in);
		const bool landed = in.on_floor && !was_floor_;
		was_floor_ = in.on_floor;
		const bool footfall = amt > 0.12f && int(std::floor(last_phase * 2.0f)) != int(std::floor(gait_.phase * 2.0f));
		if (hit_.fresh && alive) {
			hit_t_ = 0.0f;
		}
		hit_t_ += dt;

		const bool idle = in.state == State::Roam && !in.chasing && in.speed < 0.2f && in.on_floor;
		w_idle_.step(idle, 0.35f, 0.2f, dt);
		peek_t_ += dt;
		if (!idle) {
			peek_timer_.reset();
		} else if (peek_t_ > 2.0f && peek_timer_.step(dt, 2.2f, 4.2f)) {
			peek_t_ = 0.0f;
			peek_side_ = 1 - peek_side_;
		}

		// ---- the pose: locomotion, then the state layers
		Pose p = locomotion(in, amt, run);
		if (w_idle_.w > 0.0f) {
			mix(p, idle_pose(in, p), w_idle_.w);
		}
		w_wind_.step(in.state == State::Windup, 0.1f, 0.06f, dt);
		w_attack_.step(in.state == State::Attack, 0.03f, 0.3f, dt);
		w_recover_.step(in.state == State::Recover, 0.06f, 0.4f, dt);
		if (in.state == State::Windup) {
			wind_ = in.windup;
		}
		if (w_wind_.w > 0.0f) {
			mix(p, windup(in, wind_), w_wind_.w);
		}
		if (in.state == State::Attack) {
			if (in.entered) {
				attack_kick_ = true;
			}
			attack_t_ = in.state_t;
		}
		if (w_attack_.w > 0.0f) {
			mix(p, puff(in, attack_t_), w_attack_.w);
		}
		if (in.state == State::Recover) {
			recover_t_ = in.state_t;
		}
		if (w_recover_.w > 0.0f) {
			const float fade = 1.0f - smoothstep01(remap01(recover_t_, 1.1f, 1.7f));
			mix(p, recover(in, p, recover_t_), w_recover_.w * fade);
		}
		// A hit: the cap drops over its eyes and both arms push it back up.
		const float hit_w = alive ? smoothstep01(hit_t_ / 0.04f) * (1.0f - smoothstep01(remap01(hit_t_, HIT_TIME - 0.22f, HIT_TIME))) : 0.0f;
		if (hit_w > 0.0f) {
			mix(p, hit_pose(in, p, hit_t_), hit_w);
		}
		if (dying) {
			mix(p, death_pose(in, p), smoothstep01(in.state_t / 0.08f));
		}
		if (rising) {
			mix(p, rise_pose(in, p), 1.0f);
		}

		// ---- springs
		if (far) {
			lean_.reset(p.lean);
			stretch_.reset(p.stretch - 1.0f);
			cap_y_.reset();
		} else {
			lean_.kick(Vector3(-in.accel_local.z, 0.0f, in.accel_local.x) * (2.2f * dt) * (alive ? 1.0f : 0.0f));
			lean_.step(p.lean, 3.2f, 0.45f, dt);
			if (footfall) {
				stretch_.kick(-0.35f * amt);
				cap_y_.kick(-0.3f * amt);
			}
			if (hit_.fresh) {
				stretch_.kick(-1.4f);
			}
			if (landed) {
				stretch_.kick(-0.8f);
				cap_y_.kick(-0.5f);
			}
			if (attack_kick_) {
				stretch_.kick(-1.2f);
				cap_y_.kick(-0.9f);
			}
			stretch_.step(p.stretch - 1.0f, 5.0f, 0.32f, dt);
			cap_y_.step(0.0f, 4.5f, 0.28f, dt);
			cap_y_.x = Math::clamp(cap_y_.x, -0.05f, 0.05f);
		}
		const float stretch = Math::clamp(1.0f + stretch_.x, 0.6f, 1.4f);

		// ---- solve: stalk (with the look), face
		rig.pose(body_, Basis());
		const bool intent = in.state == State::Windup || in.state == State::Attack;
		face_.look_weight = intent ? 1.0f : (alive && !idle ? -1.0f : 0.0f);
		face_.head_lag = anim::damp(face_.head_lag, intent ? 0.05f : 0.15f, 0.08f, dt);
		face_.wide = p.wide;
		face_.shut = p.shut;
		face_.update(in, rig);
		const Basis head_basis = Basis::from_euler(anim::look_euler(face_.head)) * Basis::from_euler(p.head_rot);
		rig.pose(head_, head_basis, p.head_off, anim::squash_stretch(stretch));

		// eyes: chevrons instead of the eye when squeezed shut
		const bool squint = p.squint > 0.5f;
		for (int s = 0; s < 2; ++s) {
			rig.show(squint_[s], squint);
			if (squint) {
				rig.pose(eye_[s], Basis(), Vector3(), Vector3(1.0f, 0.02f, 1.0f));
			}
		}
		// eye glow: a dim amber ring at rest, flaring through the wind-up and the puff
		float glow_want = 0.12f;
		if (in.state == State::Windup) {
			glow_want = lerp(0.12f, 1.0f, smoothstep01(remap01(in.windup, 0.0f, 0.3f)));
		} else if (in.state == State::Attack) {
			glow_want = 1.0f;
		}
		glow_ = glow_ < 0.0f || far ? glow_want : anim::damp(glow_, glow_want, in.state == State::Attack ? 0.02f : 0.12f, dt);
		if (std::fabs(glow_ - glow_sent_) > 0.01f) {
			glow_sent_ = glow_;
			for (int s = 0; s < 2; ++s) {
				rig.glow(eye_[s], glow_);
				rig.glow(pupil_[s], glow_);
			}
		}
		// mouth (scaled in Y about the lip line; an O narrows it), cheeks, brows
		const float mo = Math::clamp(p.mouth, 0.0f, 1.2f);
		rig.show(mouth_, mo > 0.03f);
		rig.pose(mouth_, Basis(), Vector3(), Vector3(1.0f - 0.42f * saturate(p.mouth_round), Math::max(mo, 0.02f), 1.0f));
		const float pf = Math::max(0.0f, p.puff);
		for (int s = 0; s < 2; ++s) {
			rig.pose(cheek_[s], Basis(), Vector3(), Vector3(1.0f + 0.7f * pf, 1.0f + 0.55f * pf, 1.0f + 1.25f * pf));
			// brow_l: + turns the outer end up (inner end down: a frown); brow_r the other way
			rig.pose(brow_[s], Basis(Vector3(0, 0, 1), SIDE[s] * p.frown), Vector3(0.0f, p.brow_raise, 0.0f));
		}

		// ---- arms and hands: on springs, the hands trailing the arms
		for (int s = 0; s < 2; ++s) {
			const Vector3 want(p.arm_raise[s], p.arm_fwd[s], 0.0f);
			if (far) {
				arm_s_[s].reset(want);
			} else {
				if (hit_.fresh) {
					arm_s_[s].kick(Vector3(5.0f, -2.0f, 0.0f));
				}
				arm_s_[s].step(want, 6.5f, 0.42f, dt);
			}
			const Vector3 a = arm_s_[s].x;
			// swing forward first (in the arm's rest frame), then raise: + fwd stays toward the front however high the arm is
			rig.pose(arm_[s], Basis(Vector3(0, 0, 1), SIDE[s] * a.x) * Basis(Vector3(1, 0, 0), -a.y));
			const Vector3 trail = want - a; // where the arm is still going: the hand lags behind it
			const float bend = p.hand_bend[s] + Math::clamp(0.9f * trail.x, -0.6f, 0.6f);
			rig.pose(hand_[s], Basis(Vector3(1, 0, 0), Math::clamp(-0.7f * trail.y, -0.5f, 0.5f)) * Basis(Vector3(0, 0, 1), SIDE[s] * bend));
		}

		// ---- feet (model space: body stays at rest)
		for (int s = 0; s < 2; ++s) {
			rig.pose(foot_[s], Basis(Vector3(1, 0, 0), p.foot_pitch[s]), p.foot[s]);
		}

		// ---- the cap: authored pose, a heavy pendulum on top, the vertical bounce
		Basis cap_basis = Basis::from_euler(p.cap_rot);
		Vector3 cap_off = p.cap_off + Vector3(0.0f, cap_y_.x, 0.0f);
		if (!far) {
			if (hit_.fresh) {
				cap_d_.kick(Basis(Vector3(0, 1, 0), in.yaw).xform(in.hit_dir_local) * 1.2f);
			}
			if (attack_kick_) {
				cap_d_.kick(Basis(Vector3(0, 1, 0), in.yaw).xform(Vector3(0.0f, 0.0f, 0.6f)));
			}
			anim::DangleParams cp;
			cp.len = 0.28f; // heavy: a slow, wide lag, never a buzz at step rate
			cp.hz = 1.7f; // under the step rate (about 3 Hz walking), so the patter never pumps it up
			cp.zeta = 0.42f;
			cp.inertia = 0.5f;
			cp.limit = 0.22f;
			const Vector3 lag = cap_d_.step(rig.world_rest(cap_), Vector3(0, 1, 0), cp, dt);
			cap_basis = Basis(anim::quat_from_rotvec(lag * (alive ? 1.0f : 0.0f))) * cap_basis;
		} else {
			cap_d_.reset();
		}
		Vector3 cap_scale = p.cap_scale;
		if (dying) {
			death_cap(in, p, stretch, cap_basis, cap_off, cap_scale);
		}
		rig.pose(cap_, cap_basis, cap_off, cap_scale);

		// ---- the sprout: whipped about by the cap
		Vector3 sbend;
		if (!far) {
			if (hit_.fresh) {
				sprout_d_.kick(Basis(Vector3(0, 1, 0), in.yaw).xform(in.hit_dir_local) * 2.0f);
			}
			if (attack_kick_) {
				sprout_d_.kick(Basis(Vector3(0, 1, 0), in.yaw).xform(Vector3(0.0f, 0.0f, 2.2f)));
			}
			anim::DangleParams sp;
			sp.len = 0.14f;
			sp.hz = 3.4f;
			sp.zeta = 0.2f;
			sp.drag = 0.2f;
			sp.inertia = 1.2f;
			sp.limit = 0.8f;
			sbend = sprout_d_.step(rig.world_rest(sprout_), sprout_up_, sp, dt);
			const float tremble = w_wind_.w * smoothstep01(remap01(wind_, 0.25f, 0.5f));
			sbend.z += 0.08f * tremble * std::sin(in.t * 47.0f);
		} else {
			sprout_d_.reset();
		}
		rig.bend(sprout_, sbend);
		attack_kick_ = false;

		// ---- the root: facing, lean (springy), the flinch, rise and death
		Vector3 lean = lean_.x + recoil * 0.7f;
		Vector3 offset;
		float rscale = 1.0f;
		if (rising) {
			rise_root(in, lean, offset, rscale);
		} else if (dying) {
			lean = Vector3();
			rscale = 1.0f - 0.95f * smoothstep01(remap01(in.state_t / DEATH_TIME, 0.86f, 1.0f));
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
	// A pattering waddle: short quick steps by distance, the stalk rolling onto each planted foot and
	// bobbing, leaning into the run; little arms swinging (walk) or pumping out to the sides (chase).
	Pose locomotion(const AnimInput &in, float amt, float run) const {
		Pose p;
		const float ph = gait_.phase * TAU;
		const float sn = std::sin(ph), cs = std::cos(ph);
		const float bounce = std::fabs(sn);
		const float breath = std::sin(in.t * 2.4f + phase0_);
		const float stride = lerp(STRIDE_WALK, STRIDE_RUN, run);
		p.head_off = Vector3(0.008f * amt * sn, 0.016f * amt * bounce + 0.003f * breath * (1.0f - amt), 0.0f);
		p.head_rot = Vector3(0.04f * amt + 0.05f * run, 0.07f * amt * cs, -0.085f * amt * sn * (1.0f - 0.3f * run));
		p.lean = Vector3(0.04f * amt + 0.09f * run, 0.0f, 0.0f);
		p.stretch = 1.0f + 0.014f * breath * (1.0f - amt) - 0.05f * amt * (1.0f - bounce);
		for (int s = 0; s < 2; ++s) {
			float f = gait_.phase + (s == L ? 0.0f : 0.5f);
			f -= std::floor(f);
			float z, lift = 0.0f, pitch;
			if (f < 0.5f) {
				z = stride * (0.25f - f);
				pitch = -0.2f * (1.0f - saturate(f / 0.1f));
			} else {
				const float u = (f - 0.5f) * 2.0f;
				z = stride * (-0.25f + 0.5f * smoothstep01(u));
				lift = STEP_H * std::sin(PI * u);
				pitch = u < 0.5f ? 0.45f * std::sin(TAU * u) : -0.2f * std::sin(PI * (u - 0.5f));
			}
			p.foot[s] = Vector3(0.0f, lift * amt, z * amt);
			p.foot_pitch[s] = pitch * amt;
		}
		for (int s = 0; s < 2; ++s) {
			const float swing = s == L ? -cs : cs;
			const float pump = std::sin(2.0f * ph + float(s) * PI);
			p.arm_raise[s] = 0.12f + 0.03f * breath * (1.0f - amt) + run * (0.55f + 0.22f * pump);
			p.arm_fwd[s] = 0.38f * swing * amt * (1.0f - run) - 0.3f * run;
			p.hand_bend[s] = 0.15f * run;
		}
		// the cap rides back against the forward lean, so the face stays in view on the run
		p.cap_rot = Vector3(-0.07f * amt - 0.1f * run, 0.0f, 0.04f * amt * sn);
		p.mouth = 0.45f * run + 0.06f * amt * (1.0f - run);
		p.frown = -0.05f + 0.25f * run;
		return p;
	}

	// Standing about: a slow sway and breath; now and then it tips its cap back to peek out, one hand
	// up at the brim, eyes wide and an 'oo'.
	Pose idle_pose(const AnimInput &in, const Pose &base) const {
		Pose p = base;
		const float sway = std::sin(in.t * 1.3f + phase0_);
		p.head_rot.z += 0.035f * sway;
		p.head_rot.x += 0.015f * std::sin(in.t * 0.9f + 1.0f);
		p.cap_rot.z += -0.02f * sway;
		for (int s = 0; s < 2; ++s) {
			p.arm_raise[s] = 0.12f + 0.05f * std::sin(in.t * 2.4f + phase0_ + float(s));
			p.arm_fwd[s] = 0.08f;
		}
		const float pk = anim::smooth_pulse(peek_t_, 0.28f, 0.85f, 0.4f);
		if (pk > 0.0f) {
			const int ps = peek_side_;
			p.cap_rot.x -= 0.34f * pk;
			p.cap_rot.z += SIDE[ps] * 0.06f * pk;
			p.cap_off += Vector3(0.0f, 0.03f * pk, -0.01f * pk);
			p.head_rot.x -= 0.07f * pk;
			p.head_rot.z -= SIDE[ps] * 0.05f * pk;
			p.arm_raise[ps] = lerp(p.arm_raise[ps], 2.05f, pk);
			p.arm_fwd[ps] = lerp(p.arm_fwd[ps], 0.45f, pk);
			p.hand_bend[ps] = 0.5f * pk;
			p.wide = 1.0f + 0.12f * pk;
			p.mouth = lerp(p.mouth, 0.35f, pk);
			p.mouth_round = 0.75f * pk;
			p.brow_raise = 0.008f * pk;
			p.frown = lerp(p.frown, -0.25f, pk);
		}
		return p;
	}

	// The wind-up (1.1 s): a crouch and a glare (eyes flaring), then it breathes in: the cap inflates
	// to 1.3, cheeks blow out, eyes squeeze shut, arms lift out, trembling; at the end it rocks back,
	// cocked.
	Pose windup(const AnimInput &in, float w) const {
		Pose p;
		const float a = smoothstep01(remap01(w, 0.0f, 0.22f));
		const float inf = smoothstep01(remap01(w, 0.22f, 0.82f));
		const float hold = smoothstep01(remap01(w, 0.84f, 1.0f));
		const float tr = std::sin(in.t * 44.0f) * inf;
		p.head_off = Vector3(0.0f, -0.025f * a * (1.0f - inf) + 0.018f * inf, -0.01f * a);
		p.head_rot = Vector3(0.09f * a * (1.0f - inf) - 0.12f * inf - 0.06f * hold, 0.0f, 0.012f * tr);
		p.lean = Vector3(-0.04f * inf - 0.03f * hold, 0.0f, 0.0f);
		p.stretch = lerp(lerp(1.0f, 0.9f, a), 1.08f, inf);
		const float g = 0.3f * inf + 0.03f * hold;
		p.cap_scale = Vector3(1.0f + g, 1.0f + 0.7f * g, 1.0f + g) + Vector3(1, 1, 1) * (0.012f * tr);
		p.cap_off = Vector3(0.0f, 0.035f * inf, -0.012f * inf);
		p.cap_rot = Vector3(-0.1f * inf - 0.05f * hold + 0.015f * tr, 0.0f, 0.02f * tr);
		for (int s = 0; s < 2; ++s) {
			p.foot[s] = Vector3(SIDE[s] * 0.012f * a, 0.0f, (s == L ? 0.03f : -0.03f) * a);
			p.arm_raise[s] = lerp(0.35f, 1.1f, inf) - 0.2f * a * (1.0f - inf);
			p.arm_fwd[s] = 0.35f * a * (1.0f - inf) - 0.3f * inf;
			p.hand_bend[s] = 0.35f * inf;
		}
		p.puff = inf;
		p.squint = smoothstep01(remap01(w, 0.3f, 0.4f));
		p.frown = 0.38f * a;
		p.brow_raise = -0.006f * a;
		p.mouth = 0.0f;
		p.wide = 1.0f + 0.1f * a * (1.0f - inf);
		return p;
	}

	// The puff (0.3 s): the cap slams down and deflates past rest, the stalk squashes, cheeks empty,
	// eyes pop open wide and flaring, an O mouth blowing, arms flung back.
	Pose puff(const AnimInput &in, float t) const {
		Pose p;
		const float u = saturate(t / PUFF_TIME);
		const float slam = smoothstep01(remap01(u, 0.0f, 0.22f));
		const float back = smoothstep01(remap01(u, 0.45f, 1.0f));
		const float g = lerp(0.33f, -0.1f, slam) + 0.1f * back;
		p.cap_scale = Vector3(1.0f + g, 1.0f + 0.6f * g, 1.0f + g);
		p.cap_off = Vector3(0.0f, -0.06f * slam * (1.0f - 0.5f * back), 0.02f * slam);
		p.cap_rot = Vector3(0.22f * slam * (1.0f - 0.4f * back), 0.0f, 0.0f);
		p.head_off = Vector3(0.0f, -0.03f * slam, 0.015f * slam);
		p.head_rot = Vector3(0.18f * slam * (1.0f - 0.5f * back), 0.0f, 0.0f);
		p.stretch = lerp(1.08f, 0.84f, slam) + 0.1f * back;
		p.lean = Vector3(0.1f * slam, 0.0f, 0.0f);
		for (int s = 0; s < 2; ++s) {
			p.foot[s] = Vector3(SIDE[s] * 0.012f, 0.0f, s == L ? 0.03f : -0.03f);
			p.arm_raise[s] = lerp(1.1f, 0.75f, slam);
			p.arm_fwd[s] = lerp(-0.3f, -0.75f, slam);
			p.hand_bend[s] = -0.3f * slam;
		}
		p.puff = 1.0f - smoothstep01(remap01(u, 0.0f, 0.15f));
		p.squint = 0.0f;
		p.mouth = smoothstep01(remap01(u, 0.0f, 0.1f));
		p.mouth_round = 0.85f;
		p.wide = 1.22f;
		p.frown = 0.15f;
		p.brow_raise = 0.006f;
		return p;
	}

	// Recover: the cap wobbles round and settles, the stalk jiggles, a dazed, pleased look.
	Pose recover(const AnimInput &in, const Pose &base, float t) const {
		Pose p = base;
		const float mir = mirror_ ? -1.0f : 1.0f;
		const float k = std::exp(-3.0f * t);
		const float wob = std::sin(t * TAU * 3.0f);
		p.cap_rot.z += 0.2f * k * wob * mir;
		p.cap_rot.x += 0.12f * k * std::sin(t * TAU * 3.0f + 1.4f);
		p.cap_off.y += 0.012f * k * std::cos(t * TAU * 6.0f);
		p.stretch += 0.07f * std::exp(-4.0f * t) * std::sin(t * TAU * 4.0f);
		p.head_rot.z -= 0.07f * k * wob * mir;
		p.shut = Math::max(p.shut, 0.5f * (1.0f - smoothstep01(remap01(t, 0.3f, 0.6f))));
		p.mouth = lerp(p.mouth, 0.3f, 1.0f - smoothstep01(remap01(t, 0.6f, 1.1f)));
		p.frown = -0.18f;
		for (int s = 0; s < 2; ++s) {
			p.arm_raise[s] += 0.35f * k;
		}
		return p;
	}

	// Hit (0.95 s, longer than the 0.28 s Hurt state): the cap drops over its eyes; both arms go up
	// and push it back, it pops up past rest and wobbles.
	Pose hit_pose(const AnimInput &in, const Pose &base, float t) const {
		Pose p = base;
		const float lop = mirror_ ? -1.0f : 1.0f;
		const float drop = smoothstep01(remap01(t, 0.0f, 0.07f)) * (1.0f - smoothstep01(remap01(t, 0.3f, 0.5f)));
		const float push = anim::smooth_pulse(t - 0.1f, 0.14f, 0.3f, 0.3f);
		const float pop = anim::smooth_pulse(t - 0.42f, 0.07f, 0.04f, 0.3f);
		p.cap_off = Vector3(0.0f, -0.05f * drop + 0.025f * pop, 0.03f * drop);
		p.cap_rot = Vector3(0.38f * drop - 0.14f * pop, 0.0f, 0.1f * drop * lop - 0.05f * pop * lop);
		p.cap_scale = Vector3(1, 1, 1);
		p.head_rot = Vector3(-0.08f * drop, 0.0f, 0.06f * lop * drop);
		p.stretch = 1.0f - 0.12f * drop + 0.04f * pop;
		const float fling = drop * (1.0f - push);
		for (int s = 0; s < 2; ++s) {
			// flung out, then both hands up in front pushing the brim back up
			p.arm_raise[s] = lerp(lerp(base.arm_raise[s], 1.3f, fling), 1.7f, push);
			p.arm_fwd[s] = lerp(lerp(base.arm_fwd[s], -0.2f, fling), 1.0f, push);
			p.hand_bend[s] = 0.6f * push;
		}
		p.squint = drop > 0.3f || push > 0.5f ? 1.0f : 0.0f;
		p.mouth = 0.6f * Math::max(drop, push);
		p.mouth_round = 0.7f;
		p.frown = -0.3f;
		p.brow_raise = 0.008f;
		p.puff = 0.0f;
		return p;
	}

	// Death (0.6 s): a jolt, then the stalk ducks into the ground, eyes squeezed shut, while the cap
	// spins off and down like a coin on its rim and falls flat (death_cap). The game's puff ends it.
	Pose death_pose(const AnimInput &in, const Pose &base) const {
		Pose p = base;
		const float k = saturate(in.state_t / DEATH_TIME);
		const float jolt = 1.0f - smoothstep01(remap01(k, 0.12f, 0.3f));
		const float sink = anim::ease_in(remap01(k, 0.12f, 0.6f));
		p.head_off = Vector3(0.0f, 0.04f * (1.0f - sink) * jolt - 0.85f * sink, 0.0f);
		p.head_rot = Vector3(0.0f, 0.0f, 0.06f * std::sin(in.state_t * 40.0f) * (1.0f - jolt) * (1.0f - sink));
		p.stretch = lerp(1.0f, 1.12f, jolt);
		p.lean = Vector3();
		for (int s = 0; s < 2; ++s) {
			p.arm_raise[s] = lerp(2.1f, 1.6f, sink);
			p.arm_fwd[s] = 0.1f;
			p.hand_bend[s] = 0.4f * std::sin(in.state_t * 30.0f + float(s));
			p.foot[s] = Vector3(0.0f, 0.04f * jolt - 0.85f * sink, 0.0f); // the feet go down with the stalk
		}
		p.mouth = 0.85f;
		p.mouth_round = 0.75f;
		p.wide = 1.0f + 0.3f * jolt;
		p.squint = k > 0.22f ? 1.0f : 0.0f;
		p.frown = -0.3f;
		p.brow_raise = 0.01f;
		p.puff = 0.0f;
		return p;
	}

	// The coin: the cap pops up, turns onto its rim and spins down, rolling round on its edge faster
	// and faster as it settles flat (a spinning coin), resting on the ground at the end.
	void death_cap(const AnimInput &in, const Pose &p, float stretch, Basis &basis, Vector3 &off, Vector3 &scale) const {
		const float k = saturate(in.state_t / DEATH_TIME);
		const float dir = mirror_ ? -1.0f : 1.0f;
		const float pop = remap01(k, 0.0f, 0.22f);
		const float edge = smoothstep01(remap01(k, 0.05f, 0.32f));
		const float fall = anim::ease_in(remap01(k, 0.5f, 0.92f));
		const float tilt = 1.35f * edge * (1.0f - fall) + 0.1f * fall;
		// the spin: speeds up as it falls flat (an Euler disc)
		const float spin = dir * (10.0f * in.state_t + 26.0f * Math::max(0.0f, in.state_t - 0.3f) * Math::max(0.0f, in.state_t - 0.3f) * 4.0f);
		const float grounded = CAP_RIM_R * std::sin(tilt) + CAP_UNDER * std::cos(tilt) + 0.01f;
		float h = lerp(CAP_PIVOT_Y, CAP_PIVOT_Y + 0.18f, std::sin(PI * 0.5f * Math::min(pop, 1.0f)));
		if (k > 0.22f) {
			const float u = remap01(k, 0.22f, 0.45f);
			h = lerp(CAP_PIVOT_Y + 0.18f, grounded, u * u);
		}
		h = Math::max(h, grounded);
		const float w = smoothstep01(in.state_t / 0.06f);
		const Basis coin = Basis(Vector3(0, 1, 0), spin) * Basis(Vector3(0, 0, 1), tilt) * Basis(Vector3(1, 0, 0), CAP_TILT);
		basis = Basis(basis.get_rotation_quaternion().slerp(coin.get_rotation_quaternion(), w));
		// the cap's pivot in the stalk's space: the stalk is not turned while dying, only moved and
		// (briefly, in the jolt) stretched. Model height of the pivot = hips + head_off + sy * (rest + off).
		const float sy = anim::squash_stretch(stretch).y;
		const float rest_y = CAP_PIVOT_Y - HIP_Y;
		const float want_y = (h - HIP_Y - p.head_off.y) / Math::max(sy, 0.2f) - rest_y;
		off = off.lerp(Vector3(0.0f, want_y, 0.0f), w);
		scale = scale.lerp(Vector3(1, 1, 1), w);
	}

	// Spawn (0.6 s): only the sprout shows, wiggling out of the ground; the toadstool bursts up into
	// the air stretched, arms flung out, eyes shut, the cap lagging; it lands squashed, the cap bonks
	// down and the eyes pop open.
	Pose rise_pose(const AnimInput &in, const Pose &base) const {
		Pose p = base;
		const float k = saturate(in.state_t / RISE_TIME);
		const float up = smoothstep01(remap01(k, 0.3f, 0.55f)) * (1.0f - smoothstep01(remap01(k, 0.68f, 0.82f)));
		for (int s = 0; s < 2; ++s) {
			p.arm_raise[s] = lerp(k < 0.3f ? -0.2f : 0.12f, 1.9f, up);
			p.arm_fwd[s] = 0.0f;
		}
		p.stretch = k < 0.3f ? 0.9f : lerp(1.0f, 1.18f, up);
		p.cap_rot.x = -0.25f * up;
		p.cap_off.y = -0.04f * up;
		p.mouth = 0.7f * up;
		p.mouth_round = 0.5f;
		p.squint = k < 0.78f ? 1.0f : 0.0f;
		// (final fixes 2) the chevrons swap to the eye shut (a line), which then opens over about 5 frames
		p.shut = 1.0f - smoothstep01(remap01(k, 0.78f, 0.92f));
		p.wide = 1.0f + 0.22f * anim::pulse(k - 0.8f, 0.03f, 0.12f);
		p.frown = -0.25f * up;
		return p;
	}

	void rise_root(const AnimInput &in, Vector3 &lean, Vector3 &offset, float &rscale) {
		const float k = saturate(in.state_t / RISE_TIME);
		float y;
		if (k < 0.3f) {
			// only the sprout shows, wiggling up out of the ground (the cap's crown stays under it)
			y = -0.92f + 0.07f * smoothstep01(k / 0.3f) + 0.012f * std::sin(in.t * 29.0f);
		} else if (k < 0.55f) {
			y = lerp(-0.85f, 0.16f, anim::ease_out(remap01(k, 0.3f, 0.55f)));
		} else if (k < 0.72f) {
			const float u = remap01(k, 0.55f, 0.72f);
			y = 0.16f * (1.0f - u * u);
		} else {
			y = 0.0f;
			if (!rise_landed_) {
				rise_landed_ = true;
				stretch_.kick(-2.4f);
				cap_y_.kick(-0.9f);
			}
		}
		lean = Vector3(0.0f, 0.0f, k < 0.3f ? 0.14f * std::sin(in.t * 21.0f) : 0.0f);
		offset = Vector3(0.0f, y, 0.0f);
		rscale = 1.0f;
	}

	int body_ = -1, head_ = -1, cap_ = -1, sprout_ = -1, mouth_ = -1;
	int eye_[2] = { -1, -1 }, pupil_[2] = { -1, -1 }, brow_[2] = { -1, -1 }, cheek_[2] = { -1, -1 }, squint_[2] = { -1, -1 };
	int arm_[2] = { -1, -1 }, hand_[2] = { -1, -1 }, foot_[2] = { -1, -1 };
	Vector3 sprout_up_;
	MonsterFace face_;
	HitRecoil hit_;
	Gait gait_;
	anim::Weight w_idle_, w_wind_, w_attack_, w_recover_;
	anim::Spring3 lean_, arm_s_[2];
	anim::Spring stretch_, cap_y_;
	anim::Dangle cap_d_, sprout_d_;
	anim::RandTimer peek_timer_;
	float wind_ = 0.0f, attack_t_ = 0.0f, recover_t_ = 0.0f, peek_t_ = 10.0f, hit_t_ = 10.0f, drop_ = 0.0f, phase0_ = 0.0f;
	float glow_ = -1.0f, glow_sent_ = -1000.0f;
	int peek_side_ = 0;
	bool was_floor_ = true, seeded_ = false, mirror_ = false, rise_landed_ = false, attack_kick_ = false;
};

} // namespace

TOT_MONSTER_ANIMATOR(MonsterType::Toadstool, ToadstoolAnimator)

} // namespace tot
