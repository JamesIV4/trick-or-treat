// Bat (a SWOOP monster): the animator for its v2 rig.
//
// Part tree (art/team/monsters/batch04/build_monsters2.py, bat()):
//   body (the torso, pivot at the origin = the flyer's centre)
//     > head (pivot at the neck) > ear_l / ear_r (soft shells, also turned as a whole), tuft (soft),
//         eye_l / eye_r > pupil_l / pupil_r, brow_l / brow_r, jaw (the open mouth, scaled in Y about the
//         lip line), alt_squint_l / alt_squint_r (> < eyes, hidden until shown)
//     > wing_l / wing_r (arm, thumb, inner membrane; identity rest: raise about Z, sweep about Y, twist
//         about X, all in body axes)
//         > wingtip_l / wingtip_r (fingers and outer membrane; rest frame +Y along the first finger,
//           which is where the inner membrane ends: folding about local Y never opens a gap)
//     > foot_l / foot_r (tiny paws on a spring)
//
// A frame: a locomotion pose (hover flap, cruise, chase), state layers mixed over it by weight (wind-up:
// rear back, wings high and shivering; dive: wings swept into a V, pitched along its velocity, ears flat;
// recover: a fast shallow flutter and a head shake; hit; death: wings wrap into a spiral and it spins
// away; rise: hanging upside down wrapped in its wings, it flips upright and unfurls), then springs
// (wing bases, lean, squash, ears, tuft, feet), then one solve. The flap itself is a phase generator:
// the inner wing beats about the body's Z, the outer wing folds back about its hinge on the upstroke
// (a quarter phase behind), the body bobs against the beat.

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
constexpr float CRUISE_SPEED = 3.6f; // tuning.speed (it wanders at half)
constexpr float RISE_TIME = 0.6f, DEATH_TIME = 0.6f;
constexpr float HIT_TIME = 0.62f; // the visual hit reaction; Hurt itself is 0.28 s
constexpr float SHAKE_TIME = 0.75f; // the recover's head shake
constexpr float STRIKE_AT = 0.8f; // the dive reaches for this height above the player's feet (the kid's chest)

struct WingPose {
	float raise = 0.18f; // about the body's Z through the shoulder (+ = up)
	float sweep = 0.05f; // about the vertical (+ = tip back)
	float twist = 0.0f; // about the span (+ = membrane back)
	float fold = 0.0f; // the outer wing about its hinge (+ = folded back, - = wrapped forward)
};

struct Pose {
	Vector3 body_off; // the body joint, model space
	Vector3 body_rot; // Euler (pitch +forward, yaw, roll) about the body's centre
	float stretch = 1.0f; // volume preserving, through a spring
	Vector3 lean; // root lean, through a spring (rotation vector, facing frame)
	Vector3 tumble; // root lean NOT through the spring (the rise's flip, the death's tilt)
	float spin = 0.0f; // root turn on top of the facing
	float scale = 1.0f; // whole-bat scale (rise, poof)
	Vector3 root_off;
	WingPose wing[2];
	// the beat
	float flap_amp = 0.62f, flap_rate = 3.4f; // radians, Hz
	float flap_sweep = 0.2f, flap_twist = 0.16f, flap_fold = 0.8f, bob = 0.028f;
	float shiver = 0.0f;
	// head and face
	Vector3 head_rot; // Euler added after the look
	float ear_back = -0.15f, ear_out = 0.0f; // ears turned back (-) / forward (+), spread out (+)
	float tuck = 0.2f; // feet swung back
	float mouth = 0.45f, mouth_round = 0.0f;
	float frown = 0.0f, brow_raise = 0.0f;
	float squint = 0.0f, shut = 0.0f, wide = 1.0f;
	float iris = 0.22f, glint = 0.4f; // glow of the iris (pupil part) and of the glints (eye part)
	float look = 0.0f;
};

void mix(WingPose &d, const WingPose &s, float w) {
	d.raise = lerp(d.raise, s.raise, w);
	d.sweep = lerp(d.sweep, s.sweep, w);
	d.twist = lerp(d.twist, s.twist, w);
	d.fold = lerp(d.fold, s.fold, w);
}

void mix(Pose &d, const Pose &s, float w) {
	if (w <= 0.0f) {
		return;
	}
	w = saturate(w);
	d.body_off = d.body_off.lerp(s.body_off, w);
	d.body_rot = d.body_rot.lerp(s.body_rot, w);
	d.stretch = lerp(d.stretch, s.stretch, w);
	d.lean = d.lean.lerp(s.lean, w);
	d.tumble = d.tumble.lerp(s.tumble, w);
	d.spin = lerp(d.spin, s.spin, w);
	d.scale = lerp(d.scale, s.scale, w);
	d.root_off = d.root_off.lerp(s.root_off, w);
	for (int i = 0; i < 2; ++i) {
		mix(d.wing[i], s.wing[i], w);
	}
	d.flap_amp = lerp(d.flap_amp, s.flap_amp, w);
	d.flap_rate = lerp(d.flap_rate, s.flap_rate, w);
	d.flap_sweep = lerp(d.flap_sweep, s.flap_sweep, w);
	d.flap_twist = lerp(d.flap_twist, s.flap_twist, w);
	d.flap_fold = lerp(d.flap_fold, s.flap_fold, w);
	d.bob = lerp(d.bob, s.bob, w);
	d.shiver = lerp(d.shiver, s.shiver, w);
	d.head_rot = d.head_rot.lerp(s.head_rot, w);
	d.ear_back = lerp(d.ear_back, s.ear_back, w);
	d.ear_out = lerp(d.ear_out, s.ear_out, w);
	d.tuck = lerp(d.tuck, s.tuck, w);
	d.mouth = lerp(d.mouth, s.mouth, w);
	d.mouth_round = lerp(d.mouth_round, s.mouth_round, w);
	d.frown = lerp(d.frown, s.frown, w);
	d.brow_raise = lerp(d.brow_raise, s.brow_raise, w);
	d.squint = lerp(d.squint, s.squint, w);
	d.shut = lerp(d.shut, s.shut, w);
	d.wide = lerp(d.wide, s.wide, w);
	d.iris = lerp(d.iris, s.iris, w);
	d.glint = lerp(d.glint, s.glint, w);
	d.look = lerp(d.look, s.look, w);
}

class BatAnimator : public MonsterAnimator {
public:
	float rise_time() const override { return RISE_TIME; }
	float death_time() const override { return DEATH_TIME; }
	float telegraph_gain() const override { return 2.3f; } // (final fixes 3) 2.2 -> 2.3 (2.4 measured 107 / 93% of BEFORE, flash_metric.py; the reviewer's pulse ~0.8 -> 0.84)
	float hit_flash_gain() const override { return 2.0f; } // (final fixes 2) the fade after the white peak (linear body_flash)

	void bind(MonsterRig &rig) override {
		body_ = rig.slot("body");
		head_ = rig.slot("head");
		jaw_ = rig.slot("jaw");
		tuft_ = rig.slot("tuft");
		static const char *names[2][9] = {
			{ "eye_l", "pupil_l", "brow_l", "alt_squint_l", "ear_l", "wing_l", "wingtip_l", "foot_l", "" },
			{ "eye_r", "pupil_r", "brow_r", "alt_squint_r", "ear_r", "wing_r", "wingtip_r", "foot_r", "" },
		};
		for (int s = 0; s < 2; ++s) {
			eye_[s] = rig.slot(names[s][0]);
			pupil_[s] = rig.slot(names[s][1]);
			brow_[s] = rig.slot(names[s][2]);
			squint_[s] = rig.slot(names[s][3]);
			ear_[s] = rig.slot(names[s][4]);
			wing_[s] = rig.slot(names[s][5]);
			tip_[s] = rig.slot(names[s][6]);
			foot_[s] = rig.slot(names[s][7]);
			ear_dir_[s] = rig.tip(ear_[s]).normalized();
			ear_len_[s] = Math::max(0.05f, rig.reach(ear_[s]));
			// the ear bends from just above its sunk base along its length
			rig.soft(ear_[s], ear_dir_[s], ear_len_[s], 0.03f);
			rig.show(squint_[s], false);
		}
		tuft_len_ = Math::max(0.04f, rig.reach(tuft_));
		rig.soft(tuft_, Vector3(0, 1, 0), tuft_len_, 0.006f);
		face_.bind(rig);
		face_.max_yaw = 0.6f;
		face_.max_pitch = 0.45f; // it hovers above the player: the look is mostly down
		face_.look_height = 0.6f;
		face_.pupil_travel = 0.16f;
		face_.closed_scale = 0.08f;
		face_.wake_hold = 1.2f; // (final fixes) it wakes with its eyes shut; no second blink right after (one is enough)
		face_.glow_rest = -1.0f; // the iris and glint glows are driven here
		// Soft velvet fur and membranes, never a wet shine on the big shapes; glossy eyes, wet mouth.
		for (int i = 0; i < rig.count(); ++i) {
			rig.gloss(i, -0.45f);
		}
		rig.param_all("rim_strength", 0.14f);
		rig.gloss(jaw_, 0.25f);
		for (int s = 0; s < 2; ++s) {
			rig.gloss(eye_[s], 0.55f); // the white of the eye: a small tight highlight
			rig.gloss(pupil_[s], -1.0f); // the glowing iris: no sheen over it
		}
	}

	void update(const AnimInput &in, MonsterRig &rig, Transform3D &root) override {
		using State = MonsterAnimState;
		const float dt = anim::safe_dt(in.dt);
		const bool far = in.lod > 0;
		if (!seeded_) {
			seeded_ = true;
			mirror_ = ((in.seed >> 5) & 1u) != 0;
			phase0_ = anim::hash01(in.seed) * TAU;
			flap_ = phase0_;
		}
		const bool dying = in.state == State::Dying, rising = in.state == State::Rise;
		const bool alive = !dying && !rising;

		// ---- clocks and one-shots
		const float hspeed = alive && in.state != State::Windup && in.state != State::Attack ? in.speed : 0.0f;
		amt_ = far ? saturate(hspeed / CRUISE_SPEED) : anim::damp(amt_, saturate(hspeed / CRUISE_SPEED), 0.2f, dt);
		const Vector3 recoil = hit_.update(in);
		if (hit_.fresh && alive) {
			hit_t_ = 0.0f;
			hit_spin_ = (mirror_ ? -1.0f : 1.0f) * (in.hit_dir_local.x >= 0.0f ? 1.0f : -1.0f);
		}
		hit_t_ += dt;
		bool attack_kick = false, recover_kick = false;
		if (in.state == State::Attack) {
			if (in.entered) {
				attack_kick = true;
			}
			attack_t_ = in.state_t;
		}
		if (in.state == State::Recover) {
			if (in.entered && in.prev_state == State::Attack) {
				recover_kick = true;
				shake_t_ = 0.0f;
			}
			recover_t_ = in.state_t;
		}
		shake_t_ += dt;
		if (rising) {
			rise_t_ = in.state_t;
		}
		if (in.state == State::Windup) {
			wind_ = in.windup;
		}

		// ---- the pose: locomotion, then the state layers
		Pose p = locomotion(in);
		w_wind_.step(in.state == State::Windup, 0.06f, 0.08f, dt);
		w_attack_.step(in.state == State::Attack, 0.05f, 0.3f, dt);
		w_recover_.step(in.state == State::Recover, 0.05f, 0.35f, dt);
		w_rise_.step(rising, 0.0f, 0.22f, dt);
		if (w_wind_.w > 0.0f) {
			mix(p, windup(in, p, wind_), w_wind_.w);
		}
		if (w_attack_.w > 0.0f) {
			mix(p, dive(in, p, attack_t_), w_attack_.w);
		}
		if (w_recover_.w > 0.0f) {
			const float fade = 1.0f - smoothstep01(remap01(recover_t_, 0.9f, 1.4f));
			mix(p, flutter(in, p, recover_t_), w_recover_.w * fade);
		}
		const float hit_w = alive ? smoothstep01(hit_t_ / 0.04f) * (1.0f - smoothstep01(remap01(hit_t_, HIT_TIME - 0.25f, HIT_TIME))) : 0.0f;
		if (hit_w > 0.0f) {
			mix(p, hit_pose(in, p, hit_t_), hit_w);
		}
		if (w_rise_.w > 0.0f) {
			mix(p, rise_pose(in, p, rise_t_), w_rise_.w);
		}
		if (dying) {
			mix(p, death_pose(in, p), smoothstep01(in.state_t / 0.05f));
		}

		// ---- the beat: a phase generator; amplitude and rate come from the mixed pose, so they never pop
		flap_ += TAU * p.flap_rate * dt;
		if (flap_ > TAU * 64.0f) {
			flap_ -= TAU * 64.0f;
		}
		const float sn = std::sin(flap_), cs = std::cos(flap_);
		const float ampk = saturate(p.flap_amp / 0.62f);
		// the outer wing folds back while the inner one rises (cos > 0), and opens out on the downstroke
		const float up = 0.5f + 0.5f * cs;
		const float fold_osc = p.flap_fold * ampk * up * std::sqrt(up);
		const float shiv = p.shiver * (anim::noise1(in.t * 30.0f, in.seed) * 0.8f + std::sin(in.t * 47.0f) * 0.4f);

		// ---- springs
		if (far) {
			lean_.reset(p.lean);
			stretch_.reset(p.stretch - 1.0f);
		} else {
			// banks into turns, pitches into starts and stops
			if (alive) {
				lean_.kick(Vector3(in.accel_local.z * 0.05f, 0.0f, -in.accel_local.x * 0.07f) * (dt * 10.0f));
			}
			lean_.step(p.lean, 3.0f, 0.45f, dt);
			if (hit_.fresh) {
				stretch_.kick(-2.2f);
			}
			if (attack_kick) {
				stretch_.kick(1.8f);
			}
			if (recover_kick) {
				stretch_.kick(-1.6f);
			}
			stretch_.step(p.stretch - 1.0f, 5.0f, 0.35f, dt);
		}
		const float stretch = Math::clamp(1.0f + stretch_.x, 0.6f, 1.45f);

		// ---- solve: body (bob against the beat), head (look + extra), face
		Vector3 boff = p.body_off;
		boff.y += -p.bob * ampk * sn;
		Vector3 brot = p.body_rot;
		brot.x += 0.05f * ampk * cs; // a small rock with each beat
		brot += Vector3(0.6f, 0.3f, 0.8f) * (0.02f * shiv);
		rig.pose(body_, Basis::from_euler(brot), boff);
		const float lw = saturate(p.look);
		look_w_ = far ? lw : anim::damp(look_w_, lw, 0.1f, dt);
		face_.look_weight = look_w_;
		const bool intent = in.state == State::Windup || in.state == State::Attack;
		face_.head_lag = anim::damp(face_.head_lag, intent ? 0.04f : 0.16f, 0.08f, dt);
		face_.wide = p.wide;
		face_.shut = p.shut;
		face_.update(in, rig);
		Vector3 hrot = p.head_rot;
		hrot.x += -0.04f * ampk * cs; // the head rides the beat a little behind the body
		// It hovers above whoever it watches: the look may tip the head well down but only a little up
		// (a target high overhead rolled the face back and showed its chin).
		const Vector2 look(face_.head.x, Math::clamp(face_.head.y, -0.12f, face_.max_pitch));
		rig.pose(head_, Basis::from_euler(anim::look_euler(look)) * Basis::from_euler(hrot));
		// the same for the pupils: they may look down to the rim but only a little up
		for (int s = 0; s < 2; ++s) {
			if (Node3D *pn = rig.node(pupil_[s])) {
				Vector3 off = pn->get_position() - rig.rest(pupil_[s]).origin;
				if (off.y > 0.004f) {
					off.y = 0.004f;
					rig.pose(pupil_[s], Basis(), off);
				}
			}
		}

		// eyes: > < while hurt or dying; the glows (iris on the pupil part, glints on the eye part)
		const bool squint = p.squint > 0.5f;
		for (int s = 0; s < 2; ++s) {
			rig.show(squint_[s], squint);
			if (squint) {
				rig.pose(eye_[s], Basis(), Vector3(), Vector3(1.0f, 0.02f, 1.0f));
			}
		}
		iris_ = iris_ < 0.0f || far ? p.iris : anim::damp(iris_, p.iris, 0.05f, dt);
		glint_ = glint_ < 0.0f || far ? p.glint : anim::damp(glint_, p.glint, 0.05f, dt);
		if (std::fabs(iris_ - iris_sent_) > 0.01f || std::fabs(glint_ - glint_sent_) > 0.01f) {
			iris_sent_ = iris_;
			glint_sent_ = glint_;
			for (int s = 0; s < 2; ++s) {
				rig.glow(pupil_[s], iris_);
				rig.glow(eye_[s], glint_);
			}
		}
		// mouth (scaled in Y about the lip line; round narrows it), brows
		const float mo = Math::clamp(p.mouth, 0.0f, 1.25f);
		rig.show(jaw_, mo > 0.03f);
		rig.pose(jaw_, Basis(), Vector3(), Vector3(1.0f - 0.35f * saturate(p.mouth_round), Math::max(mo, 0.02f), 1.0f));
		for (int s = 0; s < 2; ++s) {
			rig.pose(brow_[s], Basis(Vector3(0, 0, 1), SIDE[s] * p.frown), Vector3(0.0f, p.brow_raise, 0.0f));
		}

		// ---- wings: the base pose on springs, the beat on top; the outer wing folds about its hinge
		for (int s = 0; s < 2; ++s) {
			const WingPose &w = p.wing[s];
			const Vector3 want(w.raise, w.sweep, w.fold);
			if (far || !alive) {
				wing_s_[s].reset(want);
			} else {
				if (hit_.fresh) {
					wing_s_[s].kick(Vector3(7.0f, -2.0f, -4.0f));
				}
				if (attack_kick) {
					wing_s_[s].kick(Vector3(-2.0f, 5.0f, 3.0f));
				}
				wing_s_[s].step(want, 6.5f, 0.5f, dt);
			}
			const Vector3 b = wing_s_[s].x;
			const float sd = SIDE[s];
			const float side_shiv = shiv * (s == L ? 1.0f : -0.8f);
			const float raise = b.x + p.flap_amp * sn + 0.06f * side_shiv;
			const float sweep = b.y + p.flap_sweep * ampk * sn;
			const float twist = w.twist + p.flap_twist * ampk * cs + 0.05f * side_shiv;
			const float fold = b.z + fold_osc + 0.05f * side_shiv;
			rig.pose(wing_[s], Basis(Vector3(0, 1, 0), sd * sweep) * Basis(Vector3(0, 0, 1), sd * raise) * Basis(Vector3(1, 0, 0), twist));
			rig.pose(tip_[s], Basis(Vector3(0, 1, 0), -sd * fold));
		}

		// ---- ears, tuft, feet: turned by the pose, thrown about by the motion (Dangles)
		const Basis facing(Vector3(0, 1, 0), in.yaw + p.spin);
		auto world_of = [&](int slot) {
			const Transform3D m = rig.model_rest(slot);
			return Transform3D(facing * m.basis, in.origin_world + facing.xform(m.origin));
		};
		const Vector3 updraft(0.0f, -2.0f * in.vel_local.y, 0.0f);
		for (int s = 0; s < 2; ++s) {
			rig.pose(ear_[s], Basis(Vector3(1, 0, 0), p.ear_back) * Basis(Vector3(0, 0, 1), SIDE[s] * p.ear_out));
			Vector3 eb;
			if (!far && alive) {
				if (hit_.fresh) {
					ear_d_[s].kick(facing.xform(in.hit_dir_local) * 1.6f + Vector3(0, 1.0f, 0));
				}
				anim::DangleParams ep;
				ep.len = ear_len_[s];
				ep.hz = 4.2f;
				ep.zeta = 0.22f;
				ep.drag = 0.12f;
				ep.inertia = 1.0f;
				ep.limit = 0.55f;
				eb = ear_d_[s].step(world_of(ear_[s]), ear_dir_[s], ep, dt, updraft * 0.5f);
			} else {
				ear_d_[s].reset();
			}
			rig.bend(ear_[s], eb);

			Vector3 fb;
			if (!far && alive) {
				if (hit_.fresh) {
					foot_d_[s].kick(facing.xform(in.hit_dir_local) * 1.5f);
				}
				anim::DangleParams fp;
				fp.len = 0.06f;
				fp.hz = 3.0f;
				fp.zeta = 0.28f;
				fp.gravity = 1.0f;
				fp.drag = 0.3f;
				fp.inertia = 1.0f;
				fp.limit = 0.9f;
				fb = foot_d_[s].step(world_of(foot_[s]), Vector3(0, -1, 0), fp, dt);
			} else {
				foot_d_[s].reset();
			}
			const float kick = 0.12f * std::sin(in.t * 5.3f + float(s) * 2.1f + phase0_) * (1.0f - saturate(p.tuck)); // little paddles
			rig.pose(foot_[s], Basis(anim::quat_from_rotvec(fb)) * Basis(Vector3(1, 0, 0), p.tuck + kick));
		}
		Vector3 tb;
		if (!far && alive) {
			if (hit_.fresh) {
				tuft_d_.kick(facing.xform(in.hit_dir_local) * 1.2f);
			}
			anim::DangleParams tp;
			tp.len = tuft_len_;
			tp.hz = 3.6f;
			tp.zeta = 0.2f;
			tp.drag = 0.2f;
			tp.limit = 0.75f;
			tb = tuft_d_.step(world_of(tuft_), Vector3(0, 1, 0), tp, dt, updraft);
		} else {
			tuft_d_.reset();
		}
		rig.bend(tuft_, tb);

		// ---- the root: facing (+ spin), springy lean, the flinch, squash, scale
		Vector3 lean = lean_.x + recoil * 0.8f;
		if (!alive) {
			lean = dying ? lean * (1.0f - smoothstep01(in.state_t / 0.15f)) : Vector3();
		}
		lean += p.tumble;
		const Vector3 sq = anim::squash_stretch(stretch) * Math::max(0.001f, p.scale);
		// The strike (visual only): the game counts the dive's hit as soon as the bat is within 0.9 m across
		// and 1.8 m up, which in play is still above the kid's head. So close to its target the bat reaches
		// for the kid's chest (up to 0.8 m), and springs back off it when the dive ends (or a hit stops it).
		Vector3 reach;
		// (held a moment into Recover when the dive ended early: that is the game counting the hit, the bite)
		const bool bite = in.state == State::Recover && in.prev_state == State::Attack && attack_t_ < 0.78f && in.state_t < 0.1f;
		if (alive && (in.state == State::Attack || bite)) {
			const Vector3 tgt = in.player_local + Vector3(0.0f, STRIKE_AT, 0.0f);
			const float len = tgt.length();
			const float near = 1.0f - smoothstep01(remap01(len, 1.1f, 2.6f));
			if (len > 1e-3f) {
				reach = tgt / len * (Math::clamp(len - 0.3f, 0.0f, 0.8f) * near);
			}
		}
		if (far) {
			reach_s_.reset(reach);
		} else {
			reach_s_.step(reach, 7.0f, 0.6f, dt);
		}
		root = root_pose(in.yaw + p.spin, lean, sq, p.root_off + Basis(Vector3(0, 1, 0), in.yaw).xform(reach_s_.x));
	}

private:
	// Hovering: a steady beat, a gentle roll and a curious head tilt. Cruising: pitched into the flight,
	// wings swept back, ears streaming. Chasing: a faster beat, an excited grin, the head on the player.
	Pose locomotion(const AnimInput &in) const {
		Pose p;
		const float amt = amt_;
		const float run = in.chasing ? 1.0f : 0.0f;
		p.flap_rate = 3.3f + 1.5f * amt + 0.4f * run;
		p.flap_amp = 0.6f + 0.12f * amt;
		p.flap_sweep = 0.18f + 0.06f * amt;
		p.flap_fold = 0.8f;
		p.bob = 0.03f;
		for (int s = 0; s < 2; ++s) {
			p.wing[s].raise = 0.2f - 0.05f * amt;
			p.wing[s].sweep = 0.04f + 0.22f * amt;
			p.wing[s].twist = 0.08f * amt;
			p.wing[s].fold = 0.05f + 0.1f * amt;
		}
		const float idle = 1.0f - amt;
		p.body_rot = Vector3(0.0f, 0.0f, 0.06f * std::sin(in.t * 1.3f + phase0_) * idle);
		p.lean = Vector3(0.06f + 0.3f * amt, 0.0f, 0.0f);
		p.head_rot = Vector3(-0.5f * p.lean.x, 0.0f, 0.12f * std::sin(in.t * 0.7f + phase0_ * 0.5f) * idle);
		p.ear_back = -0.12f - 0.5f * amt;
		p.ear_out = 0.05f * idle;
		p.tuck = 0.15f + 0.55f * amt;
		p.mouth = lerp(0.42f, 0.75f, run);
		p.frown = 0.12f * run;
		p.brow_raise = 0.003f * idle * (1.0f - run);
		p.look = run;
		p.iris = 0.2f;
		p.glint = 0.4f;
		return p;
	}

	// The wind-up: it rears back and up, wings raised high and shivering (the beat drops to a tremble),
	// ears pricked then pinned, chin down glaring at its target with narrowed, flaring eyes; the mouth
	// clamps into a fanged grin, then opens in a screech; in the last fifth it tips forward, cocked.
	Pose windup(const AnimInput &in, const Pose &base, float w) const {
		Pose p = base;
		const float a = smoothstep01(remap01(w, 0.0f, 0.3f));
		const float cock = smoothstep01(remap01(w, 0.8f, 1.0f));
		p.lean = Vector3(lerp(base.lean.x * (1.0f - a) - 0.62f * a, 0.18f, cock), 0.0f, 0.0f);
		p.body_off = Vector3(0.0f, 0.11f * a * (1.0f - 0.5f * cock), -0.09f * a);
		p.body_rot = Vector3(0.0f, 0.0f, 0.0f);
		p.stretch = lerp(lerp(1.0f, 1.06f, a), 0.9f, cock);
		p.flap_amp = lerp(base.flap_amp, 0.05f, a);
		p.flap_rate = lerp(base.flap_rate, 13.0f, a);
		p.flap_fold = lerp(base.flap_fold, 0.2f, a);
		p.flap_sweep = lerp(base.flap_sweep, 0.0f, a);
		p.bob = lerp(base.bob, 0.006f, a);
		p.shiver = a;
		for (int s = 0; s < 2; ++s) {
			WingPose &wp = p.wing[s];
			// high and spread out to the sides (a little forward), so both wings frame it from the front
			wp.raise = lerp(lerp(base.wing[s].raise, 1.0f, a), 0.95f, cock);
			wp.sweep = lerp(lerp(base.wing[s].sweep, -0.12f, a), 0.55f, cock);
			wp.twist = lerp(base.wing[s].twist, -0.25f, a);
			wp.fold = lerp(lerp(base.wing[s].fold, 0.12f, a), 0.5f, cock);
		}
		p.head_rot = Vector3(0.32f * a * (1.0f - cock) + 0.05f * cock, 0.0f, 0.0f);
		p.ear_back = lerp(lerp(base.ear_back, 0.18f, a), -0.75f, cock);
		p.ear_out = 0.12f * a * (1.0f - cock);
		p.tuck = lerp(base.tuck, 0.65f, a);
		const float screech = smoothstep01(remap01(w, 0.55f, 0.8f));
		p.mouth = lerp(lerp(base.mouth, 0.12f, a), 1.08f, screech);
		p.mouth_round = 0.2f * screech;
		p.frown = 0.38f * a;
		p.brow_raise = -0.004f * a;
		p.wide = lerp(lerp(1.0f, 0.86f, a), 1.14f, cock);
		p.iris = lerp(0.2f, 0.9f, smoothstep01(remap01(w, 0.15f, 0.9f)));
		p.glint = lerp(0.4f, 1.5f, smoothstep01(remap01(w, 0.3f, 0.95f)));
		p.look = 1.0f;
		return p;
	}

	// The dive (up to 0.8 s at 6.5 m/s): wings swept back into a V, the body pitched along its velocity,
	// stretched; ears flat, feet trailing, mouth wide, eyes blazing.
	Pose dive(const AnimInput &in, const Pose &base, float t) const {
		Pose p = base;
		const float burst = smoothstep01(t / 0.08f);
		const float h = Vector2(in.vel_local.x, in.vel_local.z).length();
		// (capped below 1 rad: the lean spring overshoots, and a steep dive onto a kid below read as hanging upside down)
		const float pitch = Math::clamp(std::atan2(-in.vel_local.y, Math::max(h, 0.5f)), -0.3f, 0.8f);
		p.lean = Vector3(Math::max(0.42f, pitch) * burst, 0.0f, 0.0f);
		p.body_off = Vector3();
		p.body_rot = Vector3();
		p.stretch = lerp(0.9f, 1.16f, burst);
		p.flap_amp = 0.06f;
		p.flap_rate = 9.0f;
		p.flap_fold = 0.15f;
		p.flap_sweep = 0.0f;
		p.flap_twist = 0.05f;
		p.bob = 0.0f;
		p.shiver = 0.25f * (1.0f - burst);
		for (int s = 0; s < 2; ++s) {
			WingPose &wp = p.wing[s];
			wp.raise = 0.88f;
			wp.sweep = 0.82f;
			wp.twist = 0.3f;
			wp.fold = 0.62f;
		}
		p.head_rot = Vector3(-0.45f * p.lean.x, 0.0f, 0.0f);
		p.ear_back = -1.1f;
		p.ear_out = -0.1f;
		p.tuck = 1.0f;
		p.mouth = 1.2f;
		p.mouth_round = 0.15f;
		p.frown = 0.32f;
		p.brow_raise = -0.003f;
		p.wide = 1.16f;
		p.iris = 0.9f;
		p.glint = 1.6f;
		p.look = 1.0f;
		return p;
	}

	// Recover: it pulls up out of the dive with a fast shallow flutter, shakes its head (ears flapping),
	// and settles with a smug grin.
	Pose flutter(const AnimInput &in, const Pose &base, float t) const {
		Pose p = base;
		const float in_w = smoothstep01(t / 0.1f);
		const float settle = smoothstep01(remap01(t, 0.35f, 0.9f));
		p.flap_rate = lerp(7.5f, base.flap_rate, settle);
		p.flap_amp = lerp(0.3f, base.flap_amp, settle);
		p.flap_fold = lerp(0.35f, base.flap_fold, settle);
		p.bob = lerp(0.014f, base.bob, settle);
		for (int s = 0; s < 2; ++s) {
			WingPose &wp = p.wing[s];
			wp.raise = lerp(0.42f, base.wing[s].raise, settle);
			wp.sweep = lerp(0.12f, base.wing[s].sweep, settle);
		}
		p.lean = Vector3(lerp(-0.22f, base.lean.x, settle), 0.0f, 0.0f);
		// the head shake: three quick swings, decaying
		const float st = shake_t_ - 0.12f;
		const float env = anim::smooth_pulse(st, 0.05f, 0.25f, 0.25f);
		const float sh = std::sin(Math::max(0.0f, st) * TAU * 4.6f) * env;
		p.head_rot = Vector3(base.head_rot.x, 0.42f * sh, 0.18f * sh);
		p.body_rot.z += 0.08f * sh;
		p.shut = 0.7f * env; // eyes screwed shut while it shakes
		p.ear_out = 0.25f * env;
		p.mouth = lerp(0.3f, 0.6f, settle);
		p.frown = -0.1f * settle;
		p.brow_raise = 0.004f * settle;
		p.look = 0.5f * settle * (in.chasing ? 1.0f : 0.0f);
		p.iris = lerp(0.5f, 0.2f, settle);
		p.glint = lerp(0.8f, 0.4f, settle);
		(void)in_w;
		return p;
	}

	// Hit (visual 0.62 s): knocked into a tumble, wings flung up and open, ears and feet thrown, eyes
	// squeezed > <, an 'oof' mouth; then it flaps hard to right itself.
	Pose hit_pose(const AnimInput &in, const Pose &base, float t) const {
		Pose p = base;
		const float snap = smoothstep01(remap01(t, 0.0f, 0.04f)) * (1.0f - smoothstep01(remap01(t, 0.22f, 0.5f)));
		const float wob = std::sin(t * TAU * 3.2f) * std::exp(-4.5f * t);
		p.spin = hit_spin_ * (0.5f * snap + 0.12f * wob);
		p.body_rot = Vector3(-0.15f * snap, 0.0f, 0.12f * wob);
		p.stretch = 1.0f - 0.1f * snap;
		p.flap_amp = lerp(base.flap_amp, 0.75f, 1.0f - snap);
		p.flap_rate = lerp(base.flap_rate, 6.0f, smoothstep01(remap01(t, 0.15f, 0.3f)));
		for (int s = 0; s < 2; ++s) {
			WingPose fl;
			fl.raise = 1.2f;
			fl.sweep = -0.3f;
			fl.twist = 0.0f;
			fl.fold = -0.25f;
			mix(p.wing[s], fl, snap);
		}
		p.flap_amp *= 1.0f - 0.9f * snap;
		p.ear_back = lerp(base.ear_back, 0.35f, snap);
		p.ear_out = 0.3f * snap;
		p.tuck = lerp(base.tuck, -0.3f, snap);
		p.squint = snap > 0.3f ? 1.0f : 0.0f;
		p.mouth = lerp(base.mouth, 0.95f, snap);
		p.mouth_round = 0.7f * snap;
		p.frown = -0.3f * snap;
		p.brow_raise = 0.008f * snap;
		p.wide = 1.0f + 0.12f * Math::max(0.0f, -wob);
		p.iris = 0.1f;
		p.glint = 0.25f;
		return p;
	}

	// Death (0.6 s): a jolt (eyes huge, wings flung up), then the wings fold round it into a spiral and it
	// spins away, falling, shrinking into the game's poof.
	Pose death_pose(const AnimInput &in, const Pose &base) const {
		Pose p = base;
		const float k = saturate(in.state_t / DEATH_TIME);
		const float jolt = smoothstep01(remap01(k, 0.0f, 0.08f)) * (1.0f - smoothstep01(remap01(k, 0.12f, 0.3f)));
		const float wrap = smoothstep01(remap01(k, 0.12f, 0.45f));
		const float sd = mirror_ ? -1.0f : 1.0f;
		p.spin = sd * TAU * 2.0f * anim::ease_in(remap01(k, 0.12f, 1.0f));
		p.tumble = Vector3(0.25f * wrap, 0.0f, sd * 0.45f * wrap);
		p.scale = 1.0f - anim::ease_in(remap01(k, 0.5f, 1.0f)) * 0.97f;
		p.root_off = Vector3(0.0f, -0.35f * anim::ease_in(remap01(k, 0.15f, 1.0f)) + 0.05f * jolt, 0.0f);
		p.lean = Vector3();
		p.stretch = 1.0f + 0.1f * jolt;
		p.flap_amp = 0.0f;
		p.flap_fold = 0.0f;
		p.bob = 0.0f;
		p.shiver = 0.0f;
		for (int s = 0; s < 2; ++s) {
			WingPose &wp = p.wing[s];
			wp.raise = lerp(1.25f, -0.25f, wrap);
			wp.sweep = lerp(-0.1f, -1.3f, wrap);
			wp.twist = 0.0f;
			wp.fold = lerp(-0.2f, -1.65f, wrap);
		}
		p.head_rot = Vector3(0.2f * wrap, 0.0f, 0.0f);
		p.ear_back = lerp(0.3f, -0.8f, wrap);
		p.ear_out = 0.0f;
		p.tuck = lerp(-0.2f, 0.8f, wrap);
		p.squint = k > 0.3f ? 1.0f : 0.0f;
		p.wide = 1.0f + 0.3f * jolt;
		p.mouth = lerp(1.05f, 0.5f, wrap);
		p.mouth_round = 0.8f;
		p.frown = -0.3f;
		p.brow_raise = 0.01f * jolt;
		p.look = 0.0f;
		p.iris = 0.6f * (1.0f - wrap);
		p.glint = 0.6f * (1.0f - wrap);
		return p;
	}

	// Spawn (0.6 s): it hangs upside down wrapped in its wings, asleep; it flips upright with an
	// overshoot, unfurls its wings with a fling, and its eyes pop open as the first beats start.
	Pose rise_pose(const AnimInput &in, const Pose &base, float t) const {
		Pose p = base;
		const float k = saturate(t / RISE_TIME);
		const float grow = anim::ease_out_back(remap01(k, 0.0f, 0.3f), 1.6f);
		const float flip = anim::ease_out_back(remap01(k, 0.16f, 0.56f), 1.4f);
		const float open = anim::ease_out_back(remap01(k, 0.34f, 0.72f), 2.0f);
		const float sd = mirror_ ? -1.0f : 1.0f;
		p.scale = lerp(0.35f, 1.0f, grow);
		p.tumble = Vector3(PI * (1.0f - flip), 0.0f, 0.0f);
		p.spin = sd * 0.4f * (1.0f - flip);
		p.lean = Vector3();
		p.stretch = 1.0f;
		p.body_rot = Vector3(0.0f, 0.0f, 0.05f * std::sin(t * 9.0f) * (1.0f - flip)); // swaying on its perch
		const float beat = smoothstep01(remap01(k, 0.6f, 0.95f));
		p.flap_amp = lerp(0.0f, base.flap_amp, beat);
		p.flap_fold = lerp(0.0f, base.flap_fold, beat);
		p.bob = lerp(0.0f, base.bob, beat);
		const float fling = anim::smooth_pulse(k - 0.45f, 0.1f, 0.05f, 0.25f);
		for (int s = 0; s < 2; ++s) {
			WingPose &wp = p.wing[s];
			wp.raise = lerp(-0.25f, base.wing[s].raise, open) + 0.6f * fling;
			wp.sweep = lerp(-1.3f, base.wing[s].sweep, open);
			wp.twist = lerp(0.0f, base.wing[s].twist, open);
			wp.fold = lerp(-1.65f, base.wing[s].fold, open) - 0.2f * fling;
		}
		p.ear_back = lerp(-0.6f, base.ear_back, flip) + 0.3f * fling;
		p.ear_out = 0.2f * fling;
		p.tuck = lerp(-0.4f, base.tuck, flip);
		const bool awake = k > 0.62f;
		p.shut = awake ? 0.0f : 1.0f;
		p.wide = 1.0f + 0.25f * anim::pulse(k - 0.62f, 0.04f, 0.2f);
		p.mouth = awake ? lerp(0.8f, base.mouth, smoothstep01(remap01(k, 0.7f, 1.0f))) : 0.15f;
		p.mouth_round = awake ? 0.3f * (1.0f - smoothstep01(remap01(k, 0.7f, 1.0f))) : 0.0f;
		p.brow_raise = 0.008f * fling;
		p.look = 0.0f;
		p.iris = awake ? 0.2f + 0.4f * anim::pulse(k - 0.62f, 0.04f, 0.25f) : 0.0f;
		p.glint = awake ? 0.4f + 0.5f * anim::pulse(k - 0.62f, 0.04f, 0.25f) : 0.0f;
		return p;
	}

	int body_ = -1, head_ = -1, jaw_ = -1, tuft_ = -1;
	int eye_[2] = { -1, -1 }, pupil_[2] = { -1, -1 }, brow_[2] = { -1, -1 }, squint_[2] = { -1, -1 };
	int ear_[2] = { -1, -1 }, wing_[2] = { -1, -1 }, tip_[2] = { -1, -1 }, foot_[2] = { -1, -1 };
	Vector3 ear_dir_[2];
	float ear_len_[2] = { 0.2f, 0.2f }, tuft_len_ = 0.1f;
	MonsterFace face_;
	HitRecoil hit_;
	anim::Weight w_wind_, w_attack_, w_recover_, w_rise_;
	anim::Spring3 lean_, wing_s_[2], reach_s_;
	anim::Spring stretch_;
	anim::Dangle ear_d_[2], foot_d_[2], tuft_d_;
	float amt_ = 0.0f, flap_ = 0.0f, phase0_ = 0.0f;
	float wind_ = 0.0f, attack_t_ = 0.0f, recover_t_ = 10.0f, hit_t_ = 10.0f, hit_spin_ = 1.0f, shake_t_ = 10.0f, rise_t_ = 0.0f;
	float iris_ = -1.0f, glint_ = -1.0f, iris_sent_ = -1000.0f, glint_sent_ = -1000.0f, look_w_ = 0.0f;
	bool seeded_ = false, mirror_ = false;
};

} // namespace

TOT_MONSTER_ANIMATOR(MonsterType::Bat, BatAnimator)

} // namespace tot
