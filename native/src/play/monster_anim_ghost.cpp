// Ghost (a LUNGE monster): the animator for its v2 rig.
//
// Part tree (art/team/monsters/batch04/build_monsters2.py, ghost()):
//   body (meshless joint at the pin, 0.70 m) > head (the whole sheet: ONE soft mesh pinned at the waist;
//        its hem trails by soft_bend and ripples by cloth_wave)
//        > eye_l / eye_r > pupil_l / pupil_r, brow_l / brow_r, mouth (scaled open about the lip line),
//          cheek_l / cheek_r (blush, puffs), alt_squint_l / alt_squint_r (> <), alt_happy_l / alt_happy_r (^ ^),
//          arm_l / arm_r (soft sleeve paddles with mittens, pinned at the shoulder; FK + a Dangle curl)
// The body joint carries squash, stretch, the float bob, the twirl and the flattening of the spawn and
// the death (it scales about the pin and slides down so the hem stays on the ground). The head carries
// the look. The root carries facing, lean and the drop to the ground while it is a flat sheet.
//
// A frame: a locomotion pose (idle float, wander paddle, spooky chase arms), state layers mixed over it
// by weight (peek-a-boo wind-up, BOO, giggle recover, hit), springs (stretch, arms, hem), one solve.
// Rise unfolds from a flat disc on the ground; death deflates flat, the eyes riding the sheet down,
// open a beat longer than the collapse, then squeezed shut and gone before it is flat.

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
constexpr float CHASE_SPEED = 1.6f; // tuning.speed (it wanders at half)
constexpr float RISE_TIME = 0.6f, DEATH_TIME = 0.6f;
constexpr float LUNGE_TIME = 0.25f; // the attack state (monsters.cpp attack_step, LUNGE)
constexpr float HIT_TIME = 0.75f; // the visual hit reaction; Hurt itself is 0.28 s
constexpr float PIN_Y = 0.70f; // model height of the pin (body / head pivot)
constexpr float ARM_REACH = 0.245f; // shoulder pivot to the middle of the mitten
constexpr float FLAT_Y = 0.07f; // y scale of the body when it is a flat sheet on the ground
constexpr float TWIRL_TIME = 1.0f;

struct ArmPose {
	float raise = 0.35f; // elevation of the arm above horizontal
	float fwd = 0.3f; // swing toward the front (0 = straight out to the side, PI/2 = straight ahead)
	float twist = 0.0f; // roll about the arm's own axis
	float stretch = 1.0f; // the sleeve's length (it is cloth)
	float curl_up = 0.0f, curl_fwd = 0.0f; // the soft bend of the paddle (radians at the mitten)
	float thick = 1.0f; // the paddle's width and depth (a mitten spread over the eyes)
};

struct Pose {
	Vector3 body_off; // the pin, model space, on top of rest
	Vector3 body_rot; // Euler (pitch +forward, yaw, roll) about the pin
	float stretch = 1.0f; // volume preserving squash / stretch about the pin
	Vector3 flat = Vector3(1, 1, 1); // extra non-uniform body scale (spawn / death sheet)
	Vector3 lean; // root lean (rotation vector, facing frame)
	Vector3 hem; // extra hem bend (rotation vector, facing frame; +x = hem back)
	float ripple = 0.012f, ripple_rate = 2.6f;
	ArmPose arm[2];
	float mouth = 0.6f, mouth_round = 0.0f, puff = 0.0f;
	float frown = 0.0f, brow_raise = 0.0f;
	float squint = 0.0f, happy = 0.0f, shut = 0.0f, wide = 1.0f;
	float glow = 0.35f; // glint glow
	float look = -1.0f; // MonsterFace look weight (-1 = while chasing)
	float ground = 0.0f; // 1 = drop the picture onto the ground (flat sheet)
};

void mix(ArmPose &d, const ArmPose &s, float w) {
	d.raise = lerp(d.raise, s.raise, w);
	d.fwd = lerp(d.fwd, s.fwd, w);
	d.twist = lerp(d.twist, s.twist, w);
	d.stretch = lerp(d.stretch, s.stretch, w);
	d.curl_up = lerp(d.curl_up, s.curl_up, w);
	d.curl_fwd = lerp(d.curl_fwd, s.curl_fwd, w);
	d.thick = lerp(d.thick, s.thick, w);
}

void mix(Pose &d, const Pose &s, float w) {
	if (w <= 0.0f) {
		return;
	}
	w = saturate(w);
	d.body_off = d.body_off.lerp(s.body_off, w);
	d.body_rot = d.body_rot.lerp(s.body_rot, w);
	d.stretch = lerp(d.stretch, s.stretch, w);
	d.flat = d.flat.lerp(s.flat, w);
	d.lean = d.lean.lerp(s.lean, w);
	d.hem = d.hem.lerp(s.hem, w);
	d.ripple = lerp(d.ripple, s.ripple, w);
	d.ripple_rate = lerp(d.ripple_rate, s.ripple_rate, w);
	for (int i = 0; i < 2; ++i) {
		mix(d.arm[i], s.arm[i], w);
	}
	d.mouth = lerp(d.mouth, s.mouth, w);
	d.mouth_round = lerp(d.mouth_round, s.mouth_round, w);
	d.puff = lerp(d.puff, s.puff, w);
	d.frown = lerp(d.frown, s.frown, w);
	d.brow_raise = lerp(d.brow_raise, s.brow_raise, w);
	d.squint = lerp(d.squint, s.squint, w);
	d.happy = lerp(d.happy, s.happy, w);
	d.shut = lerp(d.shut, s.shut, w);
	d.wide = lerp(d.wide, s.wide, w);
	d.glow = lerp(d.glow, s.glow, w);
	d.look = lerp(d.look, s.look, w);
	d.ground = lerp(d.ground, s.ground, w);
}

class GhostAnimator : public MonsterAnimator {
public:
	float rise_time() const override { return RISE_TIME; }
	float death_time() const override { return DEATH_TIME; }
	float telegraph_gain() const override { return 1.0f; } // (final fixes 2) linear body_flash: at least ~0.9 of BEFORE's pulse (flash_metric.py)
	float hit_flash_gain() const override { return 0.8f; } // (final fixes 2) the fade after the white peak (linear body_flash)

	void bind(MonsterRig &rig) override {
		body_ = rig.slot("body");
		head_ = rig.slot("head");
		mouth_ = rig.slot("mouth");
		static const char *names[2][8] = {
			{ "eye_l", "pupil_l", "brow_l", "cheek_l", "alt_squint_l", "alt_happy_l", "arm_l", "" },
			{ "eye_r", "pupil_r", "brow_r", "cheek_r", "alt_squint_r", "alt_happy_r", "arm_r", "" },
		};
		for (int s = 0; s < 2; ++s) {
			eye_[s] = rig.slot(names[s][0]);
			pupil_[s] = rig.slot(names[s][1]);
			brow_[s] = rig.slot(names[s][2]);
			cheek_[s] = rig.slot(names[s][3]);
			squint_[s] = rig.slot(names[s][4]);
			happy_[s] = rig.slot(names[s][5]);
			arm_[s] = rig.slot(names[s][6]);
			// head-space points the hands aim at: just in front of each eye, and of the mouth
			const Transform3D er = rig.rest(eye_[s]);
			eye_front_[s] = er.origin + er.basis.get_column(2).normalized() * 0.075 + Vector3(0.0f, -0.01f, 0.0f);
			shoulder_[s] = rig.rest(arm_[s]).origin;
		}
		const Transform3D mr = rig.rest(mouth_);
		mouth_front_ = mr.origin + mr.basis.get_column(2).normalized() * 0.07 + Vector3(0.0f, -0.03f, 0.0f);
		face_.bind(rig);
		face_.max_yaw = 0.55f;
		face_.max_pitch = 0.12f; // the whole sheet pitches about the pin: a little goes a long way
		face_.look_height = 0.5f;
		face_.pupil_travel = 0.18f;
		face_.closed_scale = 0.08f;
		face_.glow_rest = -1.0f; // the glints' glow is driven here
		// A soft cloth sheen, never a wet shine on the big white sheet; glossy ink eyes, wet mouth.
		for (int i = 0; i < rig.count(); ++i) {
			rig.gloss(i, -0.35f);
		}
		rig.param_all("rim_strength", 0.12f);
		rig.gloss(mouth_, 0.3f);
		for (int s = 0; s < 2; ++s) {
			rig.gloss(eye_[s], -2.0f); // ink dome: no sheen (it read as a chrome lens); the glints carry the wet look
			rig.gloss(pupil_[s], -2.0f);
			rig.gloss(cheek_[s], -2.0f); // a flat blush, not a shiny pill
			rig.show(squint_[s], false);
			rig.show(happy_[s], false);
			// the sleeve bends from just outside the shoulder ball along the arm
			rig.soft(arm_[s], Vector3(SIDE[s], 0.0f, 0.0f), 0.26f, 0.04f);
		}
		// the sheet bends below the pin (everything above it, face and shoulders, stays rigid)
		rig.soft(head_, Vector3(0.0f, -1.0f, 0.0f), PIN_Y, 0.04f);
	}

	void update(const AnimInput &in, MonsterRig &rig, Transform3D &root) override {
		using State = MonsterAnimState;
		const float dt = anim::safe_dt(in.dt);
		const bool far = in.lod > 0;
		if (!seeded_) {
			seeded_ = true;
			fidget_timer_ = anim::RandTimer(in.seed ^ 0x6057u);
			mirror_ = ((in.seed >> 5) & 1u) != 0;
			phase0_ = anim::hash01(in.seed) * TAU;
		}
		const bool dying = in.state == State::Dying, rising = in.state == State::Rise;
		const bool alive = !dying && !rising;
		if (in.state == State::Dying && in.entered) {
			linger_cap_ = true;
		}

		// ---- clocks and one-shots
		const float hspeed = alive && in.state != State::Windup ? in.speed : 0.0f;
		amt_ = far ? saturate(hspeed / CHASE_SPEED) : anim::damp(amt_, saturate(hspeed / CHASE_SPEED), 0.15f, dt);
		const Vector3 recoil = hit_.update(in);
		if (hit_.fresh && alive) {
			hit_t_ = 0.0f;
			hit_spin_ = (mirror_ ? -1.0f : 1.0f) * (in.hit_dir_local.x >= 0.0f ? 1.0f : -1.0f);
		}
		hit_t_ += dt;
		ripple_phase_ += dt * ripple_rate_;
		stroke_ += dt * (1.6f + 1.8f * amt_);

		const bool idle = in.state == State::Roam && !in.chasing && in.speed < 0.25f;
		idle_for_ = idle ? idle_for_ + dt : 0.0f;
		w_idle_.step(idle, 0.4f, 0.2f, dt);
		twirl_t_ += dt;
		if (!idle) {
			fidget_timer_.reset();
		} else if (idle_for_ > 5.0f && twirl_t_ > TWIRL_TIME + 1.5f && fidget_timer_.step(dt, 2.4f, 4.4f)) {
			twirl_t_ = 0.0f;
		}

		// ---- the pose: locomotion, then the state layers
		Pose p = locomotion(in);
		if (w_idle_.w > 0.0f) {
			mix(p, idle_pose(in, p), w_idle_.w);
		}
		w_wind_.step(in.state == State::Windup, 0.08f, 0.05f, dt);
		w_attack_.step(in.state == State::Attack, 0.03f, 0.25f, dt);
		w_recover_.step(in.state == State::Recover, 0.05f, 0.4f, dt);
		if (in.state == State::Windup) {
			wind_ = in.windup;
		}
		if (w_wind_.w > 0.0f) {
			mix(p, windup(in, p, wind_), w_wind_.w);
		}
		if (in.state == State::Attack) {
			if (in.entered) {
				attack_kick_ = true;
			}
			attack_t_ = in.state_t;
		}
		if (w_attack_.w > 0.0f) {
			mix(p, boo(in, p, attack_t_), w_attack_.w);
		}
		if (in.state == State::Recover) {
			recover_t_ = in.state_t;
		}
		if (w_recover_.w > 0.0f) {
			const float fade = 1.0f - smoothstep01(remap01(recover_t_, 1.3f, 1.8f));
			mix(p, giggle(in, p, recover_t_), w_recover_.w * fade);
		}
		const float hit_w = alive ? smoothstep01(hit_t_ / 0.04f) * (1.0f - smoothstep01(remap01(hit_t_, HIT_TIME - 0.25f, HIT_TIME))) : 0.0f;
		if (hit_w > 0.0f) {
			mix(p, hit_pose(in, p, hit_t_), hit_w);
		}
		if (dying) {
			mix(p, death_pose(in, p), smoothstep01(in.state_t / 0.06f));
		}
		if (rising) {
			mix(p, rise_pose(in, p), 1.0f);
		}

		// ---- springs
		if (far) {
			lean_.reset(p.lean);
			stretch_.reset(p.stretch - 1.0f);
		} else {
			lean_.kick(Vector3(in.accel_local.z, 0.0f, -in.accel_local.x) * (0.9f * dt) * (alive ? 1.0f : 0.0f));
			lean_.step(p.lean, 2.4f, 0.4f, dt);
			if (hit_.fresh) {
				stretch_.kick(-1.8f);
			}
			if (attack_kick_) {
				stretch_.kick(1.6f);
			}
			// moving up stretches it a little, sinking squashes it (the hover bob)
			stretch_.step(p.stretch - 1.0f + (alive ? Math::clamp(0.05f * in.vel_local.y, -0.04f, 0.04f) : 0.0f), 4.0f, 0.32f, dt);
		}
		const float stretch = Math::clamp(1.0f + stretch_.x, 0.55f, 1.5f);
		ripple_rate_ = p.ripple_rate;

		// ---- solve: body joint (squash, flat sheet), head (look), face
		const Vector3 sq = anim::squash_stretch(stretch);
		const Vector3 bscale(sq.x * p.flat.x, sq.y * p.flat.y, sq.z * p.flat.z);
		Vector3 boff = p.body_off;
		// scaling about the pin lifts the hem: keep the hem where the pose put it
		boff.y -= PIN_Y * (1.0f - bscale.y) * p.ground;
		rig.pose(body_, Basis::from_euler(p.body_rot), boff, bscale);
		const float lw = p.look < 0.0f ? (in.chasing ? 1.0f : 0.0f) : saturate(p.look);
		look_w_ = far ? lw : anim::damp(look_w_, lw, 0.1f, dt);
		face_.look_weight = look_w_;
		const bool intent = in.state == State::Windup || in.state == State::Attack;
		face_.head_lag = anim::damp(face_.head_lag, intent ? 0.05f : 0.18f, 0.08f, dt);
		face_.wide = p.wide;
		face_.shut = p.shut;
		face_.update(in, rig);
		const Basis head_basis = Basis::from_euler(anim::look_euler(face_.head));
		rig.pose(head_, head_basis);

		// eyes: chevrons (hit) or happy arcs (giggle) instead of the eye
		const bool squint = p.squint > 0.5f, happy = !squint && p.happy > 0.5f;
		for (int s = 0; s < 2; ++s) {
			rig.show(squint_[s], squint);
			rig.show(happy_[s], happy);
			if (happy) {
				// (final fixes 2) the arcs grow out of the shut eye's line (p.happy 0.51 .. 1)
				rig.pose(happy_[s], Basis(), Vector3(), Vector3(1.0f, lerp(0.3f, 1.0f, saturate(remap01(p.happy, 0.51f, 1.0f))), 1.0f));
			}
			if (squint || happy) {
				rig.pose(eye_[s], Basis(), Vector3(), Vector3(1.0f, 0.02f, 1.0f));
			}
		}
		if (dying) {
			linger_eyes(in, rig);
		}
		// glint glow: a sparkle at rest, blazing at the peek and the BOO
		glow_ = glow_ < 0.0f || far ? p.glow : anim::damp(glow_, p.glow, 0.04f, dt);
		if (std::fabs(glow_ - glow_sent_) > 0.01f) {
			glow_sent_ = glow_;
			for (int s = 0; s < 2; ++s) {
				rig.glow(eye_[s], glow_);
			}
		}
		// mouth (scaled in Y about the lip line; round narrows it to an O), cheeks, brows
		const float mo = Math::clamp(p.mouth, 0.0f, 1.2f);
		rig.show(mouth_, mo > 0.03f);
		rig.pose(mouth_, Basis(), Vector3(), Vector3(1.0f - 0.38f * saturate(p.mouth_round), Math::max(mo, 0.02f), 1.0f));
		const float pf = Math::max(0.0f, p.puff);
		for (int s = 0; s < 2; ++s) {
			rig.pose(cheek_[s], Basis(), Vector3(), Vector3(1.0f + 0.35f * pf, 1.0f + 0.3f * pf, 1.0f + 1.1f * pf));
			rig.pose(brow_[s], Basis(Vector3(0, 0, 1), SIDE[s] * p.frown), Vector3(0.0f, p.brow_raise, 0.0f));
		}

		// ---- arms: FK on springs, the paddle curling behind the motion (a Dangle) plus the pose's curl
		for (int s = 0; s < 2; ++s) {
			const ArmPose &a = p.arm[s];
			const Vector3 want(a.raise, a.fwd, a.twist);
			if (far || !alive) {
				arm_s_[s].reset(want);
				arm_len_[s].reset(a.stretch);
			} else {
				if (hit_.fresh) {
					arm_s_[s].kick(Vector3(6.0f, -2.5f, 0.0f));
				}
				if (attack_kick_) {
					arm_s_[s].kick(Vector3(5.0f, -3.0f, 0.0f));
				}
				const float hz = in.state == State::Attack || in.state == State::Windup ? 7.0f : 3.6f;
				arm_s_[s].step(want, hz, 0.42f, dt);
				arm_len_[s].step(a.stretch, 5.0f, 0.5f, dt);
			}
			const Vector3 v = arm_s_[s].x;
			const float sd = SIDE[s];
			// raise in the frontal plane, then swing about the vertical, then twist along the arm
			const Basis rot = Basis(Vector3(0, 1, 0), -sd * v.y) * Basis(Vector3(0, 0, 1), sd * v.x) * Basis(Vector3(1, 0, 0), sd * v.z);
			rig.pose(arm_[s], rot, Vector3(), Vector3(Math::clamp(arm_len_[s].x, 0.5f, 1.8f), a.thick, a.thick));
			Vector3 bend(0.0f, -sd * a.curl_fwd, sd * a.curl_up);
			const Vector3 trail = want - v; // where the arm is still going: the paddle lags behind it
			bend += Vector3(0.0f, 0.0f, sd * Math::clamp(-0.8f * trail.x, -0.7f, 0.7f));
			bend += Vector3(0.0f, -sd * Math::clamp(-0.8f * trail.y, -0.7f, 0.7f), 0.0f);
			if (!far && alive) {
				anim::DangleParams ap;
				ap.len = 0.24f;
				ap.hz = 2.4f;
				ap.zeta = 0.3f;
				ap.drag = 0.15f;
				ap.gravity = 1.5f;
				ap.inertia = 0.9f;
				ap.limit = 0.6f;
				bend += arm_d_[s].step(rig.world_xf(arm_[s]), Vector3(sd, 0.0f, 0.0f), ap, dt);
			} else {
				arm_d_[s].reset();
			}
			rig.bend(arm_[s], bend);
		}

		// ---- the hem: trails opposite the velocity, thrown about by starts, stops and turns, ripples
		Vector3 hem_bend;
		if (!far) {
			const Vector3 hv(in.vel_local.x, 0.0f, in.vel_local.z);
			vel_s_.follow(alive ? hv : Vector3(), 0.18f, dt);
			// cross((0,-1,0), -v): the tip swings back from the way it goes
			Vector3 trail_m(vel_s_.x.z, 0.0f, -vel_s_.x.x);
			trail_m *= 0.17f;
			if (trail_m.length() > 0.32f) {
				trail_m = trail_m.normalized() * 0.32f;
			}
			const Basis head_model = rig.model_xf(head_).basis.orthonormalized();
			if (alive) {
				const Basis facing(Vector3(0, 1, 0), in.yaw);
				if (hit_.fresh) {
					hem_d_.kick(facing.xform(in.hit_dir_local) * 1.0f);
				}
				if (attack_kick_) {
					hem_d_.kick(facing.xform(Vector3(0.0f, 0.0f, -0.9f)));
				}
				const Transform3D frame(facing * head_model, in.origin_world + facing.xform(rig.model_xf(head_).origin));
				anim::DangleParams hp;
				hp.len = 0.55f;
				hp.hz = 1.5f;
				hp.zeta = 0.32f;
				hp.inertia = 0.8f;
				hp.limit = 0.3f;
				hem_bend = hem_d_.step(frame, Vector3(0.0f, -1.0f, 0.0f), hp, dt);
			} else {
				hem_d_.reset();
			}
			hem_bend += head_model.xform_inv(trail_m + p.hem);
		} else {
			hem_d_.reset();
			vel_s_.reset();
		}
		rig.bend(head_, hem_bend);
		const float rip = far ? 0.0f : p.ripple + (alive ? 0.004f * amt_ : 0.0f);
		rig.ripple(head_, rip, ripple_phase_ + phase0_, -3.0f, -0.3f);
		attack_kick_ = false;

		// ---- the root: facing, lean (springy), the flinch, the drop to the ground while flat
		Vector3 lean = lean_.x + recoil * 0.8f;
		Vector3 offset;
		if (!alive) {
			lean = rising ? Vector3() : lean * (1.0f - smoothstep01(in.state_t / 0.15f));
		}
		if (p.ground > 0.0f) {
			float gh = 0.35f;
			if (in.ground && *in.ground) {
				gh = Math::clamp(in.origin_world.y - (*in.ground)(in.origin_world.x, in.origin_world.z), 0.0f, 1.2f);
			}
			gh_ = gh;
			offset.y -= gh * p.ground;
		}
		root = root_pose(in.yaw, lean, Vector3(1, 1, 1), offset);
	}

private:
	// Floating about: a slow bob and the old 1.7 Hz sway; wandering it paddles along with its arms,
	// leaning in; chasing, it reaches out in front with wiggling mittens, an 'oooo' mouth and a sly frown.
	Pose locomotion(const AnimInput &in) const {
		Pose p;
		const float amt = amt_;
		const float run = saturate((amt - 0.55f) / 0.45f);
		const float walk = amt * (1.0f - run);
		const float bob = std::sin(in.t * 2.6f + phase0_);
		const float sway = std::sin(in.t * 1.7f + phase0_ * 0.5f);
		const float st = stroke_ * TAU;
		p.body_off = Vector3(0.0f, 0.022f * bob, 0.0f);
		p.body_rot = Vector3(0.025f * std::sin(st) * walk, 0.0f, 0.06f * sway * (1.0f - 0.5f * amt));
		p.lean = Vector3(0.1f * walk + 0.2f * run, 0.0f, 0.0f);
		p.stretch = 1.0f + 0.018f * std::sin(in.t * 2.6f + phase0_ + 0.8f);
		p.ripple = 0.011f + 0.01f * amt;
		p.ripple_rate = 2.4f + 3.0f * amt;
		for (int s = 0; s < 2; ++s) {
			ArmPose &a = p.arm[s];
			const float off = float(s) * PI;
			// idle: floating out and up a little, drifting up and down out of step
			a.raise = 0.32f + 0.12f * std::sin(in.t * 2.1f + phase0_ + off * 0.7f);
			a.fwd = 0.28f;
			a.curl_up = 0.12f + 0.08f * std::sin(in.t * 2.1f + phase0_ + off * 0.7f - 0.9f);
			// wander: a lazy paddle, trailing back
			const float pad = std::sin(st + off);
			a.raise = lerp(a.raise, 0.05f + 0.22f * pad, walk);
			a.fwd = lerp(a.fwd, -0.15f + 0.45f * std::cos(st + off), walk);
			a.curl_up = lerp(a.curl_up, -0.25f * std::cos(st + off), walk);
			// chase: both arms out in front, reaching, the mittens wiggling
			const float wig = std::sin(in.t * 9.0f + off);
			a.raise = lerp(a.raise, 0.36f + 0.1f * wig, run);
			a.fwd = lerp(a.fwd, 0.95f + 0.06f * wig, run);
			a.curl_up = lerp(a.curl_up, -0.35f + 0.3f * std::sin(in.t * 9.0f + off + 1.2f), run);
			a.twist = lerp(0.0f, -0.4f, run);
		}
		p.mouth = lerp(0.6f, 0.75f, run);
		p.mouth_round = 0.65f * run;
		p.frown = 0.22f * run;
		p.brow_raise = -0.004f * run;
		p.look = -1.0f;
		return p;
	}

	// Floating on the spot; now and then a happy twirl (arms out, eyes shut in a smile).
	Pose idle_pose(const AnimInput &in, const Pose &base) const {
		Pose p = base;
		p.look = 0.0f; // faces the camera in the face beat; the head does not hunt for the player
		const float tw = remap01(twirl_t_, 0.0f, TWIRL_TIME);
		if (twirl_t_ < TWIRL_TIME) {
			// a happy little wave: one mitten up and waving, a bounce, head tilted, eyes smiling (the
			// face stays toward whoever is looking; a full twirl hid it)
			const int ws = mirror_ ? R : L;
			const float env = anim::smooth_pulse(twirl_t_, 0.15f, TWIRL_TIME - 0.35f, 0.2f);
			const float wave = std::sin(twirl_t_ * TAU * 3.2f);
			p.body_off.y += 0.035f * std::sin(PI * tw);
			p.stretch += 0.04f * std::sin(PI * tw);
			p.body_rot.z += SIDE[ws] * -0.09f * env;
			ArmPose &a = p.arm[ws];
			a.raise = lerp(a.raise, 1.25f, env);
			a.fwd = lerp(a.fwd, 0.35f, env);
			a.curl_up = lerp(a.curl_up, 0.45f * wave, env);
			a.twist = lerp(a.twist, 0.25f * wave, env);
			p.ripple += 0.008f * env;
			// (final fixes 2) eased across the swap: the eyes close to a line, the ^ ^ arcs replace the line and
			// grow to full height; going back, the arcs give way to the shut eye, which opens (MonsterFace::open_time)
			// (final fixes 3) closing (the pulse's rise) is slower: about 4 frames to the line, as the opening
			const float cut = twirl_t_ < 0.15f ? 0.7f : 0.3f, top = twirl_t_ < 0.15f ? 0.95f : 0.6f;
			p.shut = smoothstep01(remap01(env, 0.0f, cut));
			p.happy = env > cut ? 0.51f + 0.49f * smoothstep01(remap01(env, cut, top)) : 0.0f;
			p.mouth = lerp(p.mouth, 0.85f, env);
		}
		return p;
	}

	// Arm angles (raise, fwd) and stretch that put the mitten at `target` (head space).
	ArmPose aim(int s, const Vector3 &target, float twist) const {
		ArmPose a;
		const Vector3 d = target - shoulder_[s];
		const float len = d.length();
		const Vector3 n = len > 1e-4f ? d / len : Vector3(SIDE[s], 0, 0);
		a.raise = std::asin(Math::clamp(n.y, -1.0f, 1.0f));
		a.fwd = std::atan2(n.z, SIDE[s] * n.x);
		a.stretch = Math::clamp(len / ARM_REACH, 0.85f, 1.65f);
		a.twist = twist;
		return a;
	}

	// The wind-up (0.75 s): it shrinks back and hides its eyes behind both mittens (peek-a-boo),
	// shy and wiggling, cheeks puffed holding in a giggle; near the end one mitten slides down and an
	// eye peeks out, glinting; then both hands lift, cocked for the BOO.
	Pose windup(const AnimInput &in, const Pose &base, float w) const {
		Pose p = base;
		const float a = smoothstep01(remap01(w, 0.0f, 0.18f));
		const float peek = smoothstep01(remap01(w, 0.62f, 0.76f)) * (1.0f - smoothstep01(remap01(w, 0.9f, 1.0f)));
		const float cock = smoothstep01(remap01(w, 0.88f, 1.0f));
		const float wig = std::sin(in.t * 13.0f) * smoothstep01(remap01(w, 0.15f, 0.3f)) * (1.0f - cock);
		const int ps = mirror_ ? R : L; // the peeking side
		// shrinks back and hunches over its hidden face (the sheet bows forward about the pin while the
		// whole ghost drifts and leans back)
		p.body_off = Vector3(0.0f, -0.015f * a, -0.08f * a);
		p.body_rot = Vector3(0.16f * a * (1.0f - cock) - 0.1f * cock, 0.1f * wig, 0.05f * wig + SIDE[ps] * 0.07f * peek);
		p.stretch = lerp(lerp(1.0f, 0.92f, a), 0.82f, cock);
		p.lean = Vector3(-0.07f * a - 0.08f * cock, 0.0f, 0.0f);
		p.hem = Vector3(-0.12f * a, 0.0f, 0.0f); // the hem tucks forward under it
		p.ripple = 0.014f + 0.008f * a;
		p.ripple_rate = 5.0f;
		for (int s = 0; s < 2; ++s) {
			// The paddle is broad and flat: pointed straight at the eye it would show the face its edge.
			// So the arm reaches up beside the eye and the mitten curls in over it, palm to the face.
			ArmPose cover = aim(s, eye_front_[s] + Vector3(-SIDE[s] * 0.03f, 0.022f, -0.012f), SIDE[s] * 0.35f);
			cover.curl_fwd = 0.35f;
			cover.curl_up = 0.1f;
			cover.thick = 1.35f;
			if (s == ps) {
				// slide the mitten down and out: the eye peeks over it
				ArmPose low = aim(s, eye_front_[s] + Vector3(SIDE[s] * 0.11f, -0.1f, -0.01f), 0.0f);
				low.curl_fwd = 0.8f;
				low.thick = 1.2f;
				mix(cover, low, peek);
			}
			ArmPose up;
			up.raise = 0.9f;
			up.fwd = 0.55f;
			up.curl_up = 0.5f;
			mix(cover, up, cock);
			ArmPose &ar = p.arm[s];
			ar = base.arm[s];
			mix(ar, cover, a);
		}
		p.puff = 0.8f * a * (1.0f - cock);
		p.mouth = lerp(base.mouth, 0.05f, a) + 0.3f * peek;
		p.mouth_round = 0.0f;
		p.frown = 0.3f * peek + 0.2f * cock;
		p.brow_raise = 0.006f * a * (1.0f - peek);
		p.wide = 1.0f + 0.15f * peek;
		p.glow = lerp(0.35f, 1.6f, smoothstep01(remap01(w, 0.55f, 0.75f)));
		p.look = 1.0f;
		return p;
	}

	// BOO (the 0.25 s lunge): it bursts up tall, arms flung wide and high, mouth wide open, eyes big
	// and blazing, leaning into the player; the hem whips out behind.
	Pose boo(const AnimInput &in, const Pose &base, float t) const {
		Pose p = base;
		const float u = saturate(t / LUNGE_TIME);
		const float burst = smoothstep01(remap01(u, 0.0f, 0.25f));
		const float shake = std::sin(in.t * 38.0f) * (1.0f - u);
		p.body_off = Vector3(0.0f, 0.06f * burst, 0.05f * burst);
		p.body_rot = Vector3(-0.08f * burst, 0.0f, 0.03f * shake);
		p.stretch = lerp(0.8f, 1.2f, burst);
		p.lean = Vector3(0.28f * burst, 0.0f, 0.0f);
		p.hem = Vector3(0.05f * burst, 0.0f, 0.0f);
		p.ripple = 0.03f;
		p.ripple_rate = 12.0f;
		for (int s = 0; s < 2; ++s) {
			ArmPose &a = p.arm[s];
			a.raise = lerp(0.9f, 1.05f, burst) + 0.05f * shake;
			a.fwd = lerp(0.55f, 0.25f, burst);
			a.twist = 0.0f;
			a.stretch = 1.12f;
			a.curl_up = -0.2f + 0.15f * shake;
			a.curl_fwd = 0.3f;
		}
		p.mouth = lerp(0.3f, 1.15f, burst);
		p.mouth_round = 0.35f;
		p.puff = 0.0f;
		p.frown = -0.15f;
		p.brow_raise = 0.01f * burst;
		p.wide = lerp(1.15f, 1.32f, burst);
		p.glow = 1.6f;
		p.look = 1.0f;
		return p;
	}

	// Recover: it giggles at its own joke: three bounces, eyes squeezed into happy arcs, laughing,
	// one mitten over its mouth, the other hugging its tummy, rocking.
	Pose giggle(const AnimInput &in, const Pose &base, float t) const {
		Pose p = base;
		const float k = std::exp(-1.5f * t);
		const float b = std::fabs(std::sin(PI * t / 0.27f));
		const float gs = mirror_ ? -1.0f : 1.0f;
		const float in_w = smoothstep01(t / 0.12f);
		p.body_off.y += 0.06f * b * k * in_w;
		p.stretch = lerp(p.stretch, 0.9f + 0.14f * b, k * in_w);
		p.body_rot.z += 0.1f * gs * std::sin(t * TAU * 1.9f) * k;
		p.body_rot.x += -0.06f * k;
		p.ripple += 0.01f * k;
		const int hs = mirror_ ? R : L; // the hand over the mouth
		ArmPose cover = aim(hs, mouth_front_ + Vector3(SIDE[hs] * 0.02f, 0.0f, 0.0f), -SIDE[hs] * 0.2f);
		cover.curl_up = 0.3f;
		mix(p.arm[hs], cover, in_w);
		ArmPose hug;
		hug.raise = -0.55f;
		hug.fwd = 0.95f;
		hug.curl_fwd = 0.5f;
		hug.curl_up = 0.2f;
		mix(p.arm[1 - hs], hug, in_w);
		p.happy = t < 1.25f ? 1.0f : 0.0f;
		p.mouth = 0.55f + 0.35f * std::fabs(std::sin(t * 11.0f));
		p.mouth_round = 0.0f;
		p.puff = 0.35f;
		p.frown = -0.2f;
		p.brow_raise = 0.006f;
		p.glow = 0.5f;
		p.look = 0.5f;
		return p;
	}

	// Hit (visual 0.75 s): squashed and spun by the blow, eyes squeezed shut, an 'oof' O mouth, arms
	// flung up, the hem kicked; then a wobble back.
	Pose hit_pose(const AnimInput &in, const Pose &base, float t) const {
		Pose p = base;
		const float snap = smoothstep01(remap01(t, 0.0f, 0.05f)) * (1.0f - smoothstep01(remap01(t, 0.25f, 0.55f)));
		const float wob = std::sin(t * TAU * 3.0f) * std::exp(-4.0f * t);
		p.body_rot = Vector3(-0.1f * snap, hit_spin_ * (0.55f * snap + 0.12f * wob), 0.1f * wob);
		p.stretch = 1.0f - 0.12f * snap + 0.06f * wob;
		p.ripple = 0.035f * (snap + 0.3f);
		p.ripple_rate = 14.0f;
		for (int s = 0; s < 2; ++s) {
			ArmPose fl;
			fl.raise = 1.35f;
			fl.fwd = -0.25f;
			fl.curl_up = -0.4f;
			fl.stretch = 1.05f;
			mix(p.arm[s], fl, snap);
		}
		p.squint = snap > 0.35f ? 1.0f : 0.0f;
		p.mouth = lerp(base.mouth, 0.85f, snap);
		p.mouth_round = 0.8f * snap;
		p.puff = 0.0f;
		p.frown = -0.3f * snap;
		p.brow_raise = 0.01f * snap;
		p.wide = 1.0f + 0.1f * Math::max(0.0f, -wob);
		p.glow = 0.2f;
		return p;
	}

	// Death (0.6 s): a startled jolt (tall, eyes huge, O mouth, arms up), then the sheet deflates flat
	// onto the ground; the eyes stay on it, open a beat longer, then shut and shrink (linger_eyes).
	Pose death_pose(const AnimInput &in, const Pose &base) const {
		Pose p = base;
		const float k = saturate(in.state_t / DEATH_TIME);
		const float jolt = smoothstep01(remap01(k, 0.0f, 0.1f)) * (1.0f - smoothstep01(remap01(k, 0.16f, 0.3f)));
		const float de = anim::ease_in(remap01(k, 0.14f, 0.5f));
		const float sy = lerp(1.0f, FLAT_Y, de);
		const float sxz = lerp(1.0f, 1.5f, anim::ease_out(remap01(k, 0.14f, 0.55f)));
		// (final fixes 2) the flat sheet shrinks away into the puff over the last 11 frames (7 read as 4 visible
		// steps), gone before the state ends
		// (final fixes 3) 0.64 -> 0.48: the shrink read as about 4 frames; it starts as the sheet lands flat
		const float melt = Math::max(0.001f, 1.0f - smoothstep01(remap01(k, 0.48f, 0.96f)));
		p.flat = Vector3(sxz * melt, sy * melt, sxz * melt);
		p.stretch = lerp(1.0f, 1.16f, jolt);
		p.body_off = Vector3(0.0f, 0.03f * jolt, 0.0f);
		p.body_rot = Vector3(0.0f, 0.0f, 0.04f * std::sin(in.state_t * 50.0f) * jolt);
		p.ground = smoothstep01(remap01(k, 0.12f, 0.5f));
		p.lean = Vector3();
		p.hem = Vector3();
		p.ripple = 0.03f * (1.0f - de) + 0.012f;
		p.ripple_rate = 10.0f;
		for (int s = 0; s < 2; ++s) {
			ArmPose &a = p.arm[s];
			a.raise = lerp(1.45f, 0.1f, de);
			a.fwd = lerp(0.2f, 0.0f, de);
			a.curl_up = lerp(-0.3f, 0.0f, de);
			a.stretch = 1.0f;
		}
		p.mouth = lerp(1.0f, 0.4f, de);
		p.mouth_round = 0.85f;
		p.wide = 1.0f + 0.35f * jolt;
		p.frown = -0.3f;
		p.brow_raise = 0.012f * jolt;
		p.squint = 0.0f;
		p.happy = 0.0f;
		p.look = 0.0f;
		p.glow = 0.6f;
		return p;
	}

	// The eyes during the death: posed normally until the sheet starts to fall, then carried on the
	// deflating sheet, kept round, and shut and shrunk before it is flat.
	void linger_eyes(const AnimInput &in, MonsterRig &rig) {
		const float k = saturate(in.state_t / DEATH_TIME);
		if (k < 0.15f || linger_cap_) {
			for (int s = 0; s < 2; ++s) {
				const Transform3D m = rig.model_xf(eye_[s]);
				eye_hold_[s] = Transform3D(m.basis.orthonormalized(), m.origin);
			}
			// (final fixes 2) how shut the face had them (the killing blow's squeeze): the lingering eyes never
			// open wider than this, so they go from the hit squint to shut in one act (no one-frame pop open)
			linger_closed_ = face_.closed;
			linger_cap_ = k < 0.15f;
			return;
		}
		// (final fixes) The eyes ride ON the sheet as it deflates: their place is where the squashed sheet
		// carries them (head_m * rest), only their shape is kept round (not flattened with the sheet). They
		// stay open a beat after the collapse starts (k 0.14), glance at each other, squeeze shut, and shrink
		// into the sheet before it is flat (k 0.5). Before, they were held in the air and dropped onto the
		// puddle afterwards, which read as two loose dots coming off.
		const float shut = Math::max(smoothstep01(remap01(k, 0.27f, 0.36f)), linger_closed_);
		const float gone = 1.0f - smoothstep01(remap01(k, 0.33f, 0.47f));
		const Transform3D head_m = rig.model_xf(head_);
		for (int s = 0; s < 2; ++s) {
			const Transform3D rest = rig.rest(eye_[s]);
			const Transform3D on_sheet = head_m * rest;
			const float sc = Math::max(0.001f, gone) * (1.0f + 0.1f * anim::pulse(k - 0.2f, 0.04f, 0.12f)); // a last wide look
			Transform3D want(eye_hold_[s].basis.slerp(on_sheet.basis.orthonormalized(), saturate(remap01(k, 0.15f, 0.3f))), on_sheet.origin);
			want.basis = want.basis.scaled_local(Vector3(sc, sc * lerp(1.0f, 0.08f, shut), sc));
			const Transform3D local = head_m.affine_inverse() * want;
			rig.pose(eye_[s], rest.basis.inverse() * local.basis, local.origin - rest.origin);
			const float glance = SIDE[s] * -0.5f * smoothstep01(remap01(k, 0.18f, 0.26f)) * (1.0f - shut);
			rig.pose(pupil_[s], Basis(), Vector3(glance * 0.02f, -0.004f, 0.0f));
		}
	}

	// Spawn (0.6 s): a flat sheet lying on the ground, rippling; it unfolds upward into a ghost that
	// overshoots tall, flings its arms out, floats up off the ground and pops its eyes open.
	Pose rise_pose(const AnimInput &in, const Pose &base) const {
		Pose p = base;
		const float k = saturate(in.state_t / RISE_TIME);
		const float u = remap01(k, 0.22f, 0.62f);
		const float grow = u <= 0.0f ? 0.0f : anim::ease_out_back(u, 2.2f);
		const float sy = lerp(FLAT_Y, 1.0f, grow);
		const float sxz = lerp(1.45f, 1.0f, anim::ease_out(u));
		p.flat = Vector3(sxz, Math::max(0.02f, sy), sxz);
		p.stretch = 1.0f;
		p.ground = 1.0f - smoothstep01(remap01(k, 0.55f, 0.85f));
		p.body_off = Vector3();
		p.body_rot = Vector3(0.0f, (mirror_ ? -1.0f : 1.0f) * 0.8f * (1.0f - anim::ease_out(u)), 0.0f);
		p.lean = Vector3();
		p.hem = Vector3();
		p.ripple = k < 0.22f ? 0.03f : lerp(0.03f, 0.012f, saturate(u));
		p.ripple_rate = 9.0f;
		const float fling = anim::smooth_pulse(k - 0.4f, 0.12f, 0.08f, 0.3f);
		for (int s = 0; s < 2; ++s) {
			ArmPose &a = p.arm[s];
			a.raise = lerp(lerp(-0.6f, 0.32f, smoothstep01(u)), 1.2f, fling);
			a.fwd = lerp(0.0f, 0.28f, smoothstep01(u));
			a.curl_up = lerp(0.6f, 0.12f, smoothstep01(u)) - 0.3f * fling;
			a.stretch = 1.0f;
		}
		p.happy = k < 0.78f ? 1.0f : 0.0f;
		p.shut = 1.0f - smoothstep01(remap01(k, 0.78f, 0.97f)); // (final fixes 2) the arcs swap to a shut eye that opens; (3) over ~6 frames, was 0.92
		p.wide = 1.0f + 0.22f * anim::pulse(k - 0.8f, 0.04f, 0.15f);
		p.mouth = lerp(0.3f, 0.9f, fling) * smoothstep01(u) + 0.6f * smoothstep01(remap01(k, 0.8f, 1.0f)) * (1.0f - fling);
		p.mouth_round = 0.4f * fling;
		p.look = 0.0f;
		p.glow = 0.35f;
		return p;
	}

	int body_ = -1, head_ = -1, mouth_ = -1;
	int eye_[2] = { -1, -1 }, pupil_[2] = { -1, -1 }, brow_[2] = { -1, -1 }, cheek_[2] = { -1, -1 };
	int squint_[2] = { -1, -1 }, happy_[2] = { -1, -1 }, arm_[2] = { -1, -1 };
	Vector3 eye_front_[2], shoulder_[2], mouth_front_;
	Transform3D eye_hold_[2];
	MonsterFace face_;
	HitRecoil hit_;
	anim::Weight w_idle_, w_wind_, w_attack_, w_recover_;
	anim::Spring3 lean_, arm_s_[2], vel_s_;
	anim::Spring stretch_, arm_len_[2];
	anim::Dangle hem_d_, arm_d_[2];
	anim::RandTimer fidget_timer_;
	float amt_ = 0.0f, stroke_ = 0.0f, ripple_phase_ = 0.0f, ripple_rate_ = 2.6f;
	float wind_ = 0.0f, attack_t_ = 0.0f, recover_t_ = 10.0f, hit_t_ = 10.0f, hit_spin_ = 1.0f, twirl_t_ = 10.0f;
	float phase0_ = 0.0f, glow_ = -1.0f, glow_sent_ = -1000.0f, gh_ = 0.35f, look_w_ = 0.0f, idle_for_ = 0.0f;
	bool seeded_ = false, mirror_ = false, attack_kick_ = false, linger_cap_ = false;
	float linger_closed_ = 0.0f;
};

} // namespace

TOT_MONSTER_ANIMATOR(MonsterType::Ghost, GhostAnimator)

} // namespace tot
