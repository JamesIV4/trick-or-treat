// The Spider (a POUNCE monster): the animator for its v2 rig.
//
// Part tree (art/team/monsters/batch04/build_monsters2.py, spider()):
//   body (the thorax hub) > head > eye_l / eye_r > pupil_l / pupil_r, eye_s0..2 (each a mirrored pair of
//                                  small eyes, inner to outer), fang_l / fang_r, jaw (the open mouth, scaled
//                                  in Y about the lip line), tuft (soft), alt_squint (> <, hidden)
//                         > abdomen > alt_silk (the drop thread of the spawn, hidden)
//                         > leg_l0..3 / leg_r0..3 > shin_l0..3 / shin_r0..3      (0 = front)
// Every leg is a two-bone IK chain solved in the hub's space; the shin's IK end is the foot's contact point.
//
// Walking: the feet are planted in the WORLD and step in two alternating groups of four (l0 r1 l2 r3 and
// r0 l1 r2 l3). One phase clock, advanced by the distance travelled and by turning, says which group is in
// the air and how far through its swing it is; a swinging foot flies on an arc from where it lifted to a
// spot a quarter stride ahead of its home (where it would stand at rest under the moving body), so the
// planted feet pass under the body and never slide. Standing still, the clock runs on only until every foot
// is home again (small settling steps after a stop, a turn on the spot or a change of stance such as the
// wind-up's splay). Foot heights follow the terrain function, relative to the ground under the origin. The
// hub's height is sprung: it dips when a group lands and when the spider lands from a pounce.
// Off the ground (the pounce, a knock, the spawn, the death) the feet follow air poses in the hub's space
// instead: tucked, star-spread, reaching, flailing or curled; the two blend by weight, so nothing pops.

#include "play/monster_anim.h"

#include <godot_cpp/classes/node3d.hpp>
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

constexpr int NLEG = 8;
constexpr float SIDE[2] = { 1.0f, -1.0f }; // left (+X), right (-X)
constexpr float RISE_TIME = 0.6f, DEATH_TIME = 0.6f;
constexpr float CHASE_SPEED = 2.4f; // tuning.speed: the scuttle is fullest there
constexpr float SETTLE = 0.028f; // a foot further than this from home takes a settling step
constexpr float SPIN_HUB_Y = 0.33f; // the death roll turns about a point this high (model space)
// The pounce's own timeline (LEAP_TIME, the poses are timed on it). Held in place (the game as it is: the
// leap velocity is lost, see CHECKPOINT "Real-play review"), the Attack lasts about 0.155 s and the game's
// landing puff (and its hit) come as it ends, so the held leap plays that timeline in LEAP_HELD real seconds
// at LEAP_H high, and touches down when Attack ends: the visible landing and the puff are the same frame.
constexpr float LEAP_TIME = 0.5f, LEAP_HELD = 0.155f, LEAP_H = 0.12f;

struct Leg {
	int upper = -1, mid = -1;
	int side = 0, k = 0;
	bool group_a = false;
	Vector3 root, a, b; // hip in the hub's space, knee in the leg's space, foot contact in the shin's space
	Vector3 out; // unit, horizontal: the way the leg points (model and hub space at rest)
	Vector3 rest_foot; // model space

	void solve(MonsterRig &rig, const Vector3 &target, const Vector3 &pole) const {
		const anim::Ik2 ik = anim::two_bone_ik(root, a, b, Vector3(), target, pole, 2.6f);
		rig.pose(upper, Basis(ik.upper));
		rig.pose(mid, Basis(ik.lower));
	}
};

// Everything but the feet, mixed field by field.
struct Pose {
	Vector3 body_off, body_rot, abd_rot, head_rot; // hub offset (model), Euler (pitch, yaw, roll)
	Vector3 abd_scale = Vector3(1, 1, 1);
	float splay = 0.0f; // stance: feet this much further out (metres)
	float jaw = 0.0f; // 0 shut .. 1 wide
	float fang = 0.0f; // 0 at rest .. 1 spread wide
	float wide = 1.0f, shut = 0.0f, squint = 0.0f;
	float iris = 0.25f, glint = 0.45f, jack = 0.75f; // glows: iris (pupil parts), glints (eye parts), the jack face
};

void mix(Pose &d, const Pose &s, float w) {
	if (w <= 0.0f) {
		return;
	}
	w = saturate(w);
	d.body_off = d.body_off.lerp(s.body_off, w);
	d.body_rot = d.body_rot.lerp(s.body_rot, w);
	d.abd_rot = d.abd_rot.lerp(s.abd_rot, w);
	d.head_rot = d.head_rot.lerp(s.head_rot, w);
	d.abd_scale = d.abd_scale.lerp(s.abd_scale, w);
	d.splay = lerp(d.splay, s.splay, w);
	d.jaw = lerp(d.jaw, s.jaw, w);
	d.fang = lerp(d.fang, s.fang, w);
	d.wide = lerp(d.wide, s.wide, w);
	d.shut = lerp(d.shut, s.shut, w);
	d.squint = lerp(d.squint, s.squint, w);
	d.iris = lerp(d.iris, s.iris, w);
	d.glint = lerp(d.glint, s.glint, w);
	d.jack = lerp(d.jack, s.jack, w);
}

class SpiderAnimator : public MonsterAnimator {
public:
	float rise_time() const override { return RISE_TIME; }
	float death_time() const override { return DEATH_TIME; }
	float telegraph_gain() const override { return 1.9f; } // (final fixes 2) linear body_flash: at least ~0.9 of BEFORE's pulse (flash_metric.py)
	float hit_flash_gain() const override { return 1.4f; } // (final fixes 2) the fade after the white peak (linear body_flash)

	void bind(MonsterRig &rig) override {
		body_ = rig.slot("body");
		abd_ = rig.slot("abdomen");
		head_ = rig.slot("head");
		jaw_ = rig.slot("jaw");
		tuft_ = rig.slot("tuft");
		squint_ = rig.slot("alt_squint");
		silk_ = rig.slot("alt_silk");
		fang_[0] = rig.slot("fang_l");
		fang_[1] = rig.slot("fang_r");
		eye_[0] = rig.slot("eye_l");
		eye_[1] = rig.slot("eye_r");
		pupil_[0] = rig.slot("pupil_l");
		pupil_[1] = rig.slot("pupil_r");
		for (int k = 0; k < 3; ++k) {
			small_[k] = rig.slot(StringName(String("eye_s") + String::num_int64(k)));
		}
		static const char *uppers[NLEG] = { "leg_l0", "leg_l1", "leg_l2", "leg_l3", "leg_r0", "leg_r1", "leg_r2", "leg_r3" };
		static const char *mids[NLEG] = { "shin_l0", "shin_l1", "shin_l2", "shin_l3", "shin_r0", "shin_r1", "shin_r2", "shin_r3" };
		for (int i = 0; i < NLEG; ++i) {
			Leg &L = leg_[i];
			L.upper = rig.slot(uppers[i]);
			L.mid = rig.slot(mids[i]);
			L.side = i < 4 ? 0 : 1;
			L.k = i % 4;
			L.group_a = (L.k % 2 == 0) == (L.side == 0); // l0 r1 l2 r3
			L.root = rig.rest(L.upper).origin;
			L.a = rig.rest(L.mid).origin;
			// The foot contact: the shin's farthest vertex, dropped onto the ground plane of the rest pose.
			const Vector3 knee_model = rig.model_rest(L.mid).origin;
			const Vector3 tip = rig.tip(L.mid);
			L.b = Vector3(tip.x, -knee_model.y, tip.z);
			L.rest_foot = knee_model + L.b;
			Vector3 o = L.rest_foot - rig.model_rest(L.upper).origin;
			o.y = 0.0f;
			L.out = o.length() > 1e-4f ? o.normalized() : Vector3(SIDE[L.side], 0, 0);
		}
		face_.bind(rig);
		face_.max_yaw = 0.55f;
		face_.max_pitch = 0.35f;
		face_.look_height = 0.55f;
		face_.pupil_travel = 0.2f;
		face_.glow_rest = -1.0f; // the glows are driven here
		rig.show(squint_, false);
		rig.show(silk_, false);
		// Fur is matte, the abdomen has a soft sheen, the whites of the eyes a small tight highlight.
		for (int i = 0; i < rig.count(); ++i) {
			rig.gloss(i, -0.45f);
		}
		rig.param_all("rim_strength", 0.14f);
		for (int i = 0; i < NLEG; ++i) {
			// the pale lilac tips under the full rim read as frosted jelly: a softer rim, no sheen at all
			rig.param(leg_[i].mid, "rim_strength", 0.06f);
			rig.gloss(leg_[i].mid, -1.0f);
		}
		rig.gloss(abd_, -0.3f); // (final fixes) a soft broad sheen on the plush pile; 0.22 made a hard hotspot (a plastic ball)
		rig.gloss(jaw_, 0.25f);
		for (int s = 0; s < 2; ++s) {
			rig.gloss(eye_[s], 0.55f);
			rig.gloss(pupil_[s], -1.0f);
			rig.gloss(fang_[s], 0.15f);
		}
		for (int k = 0; k < 3; ++k) {
			rig.gloss(small_[k], 0.4f);
		}
		rig.soft(tuft_, Vector3(0, 1, 0), -1.0f, 0.008f);
		tuft_len_ = Math::max(0.04f, rig.reach(tuft_));
		hub_rest_ = rig.rest(body_);
		spin_ = rig.model_rest(silk_).origin;
	}

	void update(const AnimInput &in, MonsterRig &rig, Transform3D &root) override {
		using State = MonsterAnimState;
		const float dt = anim::safe_dt(in.dt);
		const bool far = in.lod > 0;
		if (!seeded_) {
			seeded_ = true;
			fidget_ = anim::RandTimer(in.seed ^ 0xF1D6u);
			click_ = anim::RandTimer(in.seed ^ 0xC11Cu);
			mirror_ = ((in.seed >> 3) & 1u) != 0;
			phase0_ = anim::hash01(in.seed) * TAU;
			last_yaw_ = in.yaw;
		}
		const bool dying = in.state == State::Dying, rising = in.state == State::Rise;
		const bool alive = !dying && !rising;
		const float run = saturate(in.speed / CHASE_SPEED);
		const Vector3 recoil = hit_.update(in);
		if (!in.on_floor) {
			fall_ = in.vel_local.y;
		}
		const bool landed = in.on_floor && !was_floor_;
		was_floor_ = in.on_floor;
		hurt_t_ = in.since_hit;

		// ---- clocks
		breath_ph_ += dt * TAU * 0.42f;
		wig_ph_ += dt * TAU * lerp(4.0f, 8.5f, saturate(in.windup));
		click_t_ += dt;
		if (alive && (in.state == State::Roam || in.state == State::Recover) && click_.step(dt, in.chasing ? 1.2f : 2.5f, in.chasing ? 2.6f : 5.5f)) {
			click_t_ = 0.0f;
		}
		const bool idle = in.state == State::Roam && !in.chasing && in.speed < 0.12f && in.on_floor;
		fidget_t_ += dt;
		if (!idle) {
			fidget_t_ = Math::max(fidget_t_, 10.0f);
			fidget_.reset();
		} else if (fidget_t_ > 2.2f && fidget_.step(dt, 1.5f, 3.5f)) {
			fidget_kind_ = fidget_.count % 2;
			fidget_t_ = 0.0f;
			fidget_leg_ = (fidget_.count / 2) % 2 == 0 ? 0 : 4;
		}

		// ---- the pose: locomotion, then the state layers
		Pose p = locomotion(in, run);
		w_wind_.step(in.state == State::Windup, 0.08f, 0.16f, dt);
		if (in.state == State::Windup) {
			wind_ = in.windup;
		}
		if (w_wind_.w > 0.0f) {
			mix(p, windup(in, wind_), w_wind_.w);
		}
		// The pounce runs on its own clock from the first frame of Attack. In play the body really flies
		// (about half a second) and the phases follow its vertical speed. Held in place (the showcase holds
		// a monster still through its attack: velocity 0, on the floor), it leaps in place on the same
		// timeline instead, so the tuck, the star at the top and the landing always play.
		if (in.state == State::Attack && in.entered) {
			leap_t_ = 0.0f;
			leap_on_ = alive;
			leap_real_ = false;
		}
		leap_in_ = false;
		const float h_was = leap_h_;
		leap_h_ = 0.0f;
		if (leap_on_) {
			if (in.state == State::Attack && (!in.on_floor || std::fabs(in.vel_local.y) > 0.5f)) {
				leap_real_ = true;
			}
			leap_t_ += leap_real_ ? dt : dt * (LEAP_TIME / LEAP_HELD);
			if (in.state != State::Attack && in.state != State::Recover) {
				leap_on_ = false; // knocked out of it, or dead
			} else if (leap_real_) {
				leap_in_ = in.state == State::Attack;
				leap_vy_ = in.vel_local.y;
			} else {
				const float u = leap_t_ / LEAP_TIME;
				leap_in_ = u < 1.0f && in.state == State::Attack; // the game's landing (Recover) ends it
				leap_vy_ = 3.2f * (1.0f - 2.0f * u);
				leap_h_ = LEAP_H * anim::arc(u) * smoothstep01(u / 0.22f); // eased off the ground (same apex and landing)
				if (!leap_in_) {
					leap_on_ = false;
					land_kick(); // the landing of the leap in place
				}
			}
		}
		if (!leap_in_) {
			leap_h_ = Math::max(0.0f, h_was - dt * 2.0f); // landed or knocked out of it: come down, never snap
		}
		w_pounce_.step(leap_in_, 0.04f, 0.14f, dt);
		if (leap_in_) {
			pounce_t_ = leap_t_;
			pounce_vy_ = leap_vy_;
		}
		if (w_pounce_.w > 0.0f) {
			mix(p, pounce(pounce_t_, pounce_vy_), w_pounce_.w);
		}
		if (in.state == State::Recover && in.entered && in.prev_state == State::Attack && leap_real_) {
			land_kick(); // the real landing
		}
		land_t_ += dt;
		if (in.state == State::Recover) {
			recover(in, p);
		}
		const float ouch = alive ? anim::smooth_pulse(hurt_t_, 0.03f, 0.14f, 0.4f) : 0.0f;
		if (ouch > 0.0f) {
			Pose h = p;
			h.body_rot += Vector3(-0.32f, 0.0f, 0.22f * (mirror_ ? 1.0f : -1.0f));
			h.abd_rot += Vector3(-0.25f, 0.0f, 0.0f);
			h.abd_scale = Vector3(1.12f, 0.84f, 1.08f);
			h.jaw = 0.75f;
			h.fang = 1.0f;
			h.squint = 1.0f;
			h.iris = 0.12f;
			h.jack = 0.35f;
			h.splay = p.splay + 0.05f;
			mix(p, h, ouch);
		}
		fidgets(in, p);

		// ---- springs: the hub's height (landing dips), the abdomen's lag, the tuft
		if (landed && alive && !(in.state == State::Recover && in.prev_state == State::Attack && land_t_ < 0.05f)) {
			dip_.kick(Math::clamp(fall_, -8.0f, 0.0f) * 0.32f);
			abd_spring_.kick(Vector3(Math::clamp(-fall_, 0.0f, 8.0f) * 0.6f, 0.0f, 0.0f));
		}
		if (hit_.fresh) {
			abd_spring_.kick(Vector3(-4.0f, 0.0f, 5.0f * (mirror_ ? 1.0f : -1.0f)));
			tuft_dangle_.kick(Vector3(0.0f, 2.0f, 0.0f));
		}
		if (far) {
			dip_.reset();
			abd_spring_.reset();
		} else {
			abd_spring_.kick(Vector3(-in.accel_local.z * 0.05f, in.accel_local.x * 0.04f, 0.0f) * dt * 10.0f);
			dip_.step(0.0f, 4.6f, 0.38f, dt);
			abd_spring_.step(Vector3(), 3.2f, 0.32f, dt);
		}
		const float dip = Math::clamp(dip_.x, -0.13f, 0.05f);
		p.body_off.y += dip;
		p.abd_scale *= Vector3(1.0f - 0.6f * Math::min(dip, 0.0f), 1.0f + 1.2f * Math::min(dip, 0.0f), 1.0f - 0.3f * Math::min(dip, 0.0f));

		// ---- the feet
		const Vector3 *air = nullptr;
		float air_w = 0.0f;
		air_targets(in, p, ouch, air_w);
		air = air_;
		plant(in, p, run, air_w, dt);

		// ---- spawn and death drive the whole body through the root
		Vector3 lean, extra;
		Vector3 pivot(0.0f, SPIN_HUB_Y, 0.0f);
		float root_scale = 1.0f;
		if (alive) {
			extra.y = leap_h_;
		} else if (rising) {
			rise(in, p, lean, extra, pivot, rig);
		} else if (dying) {
			death(in, p, lean, extra, pivot, root_scale);
		}

		// ---- solve: hub, abdomen, face, head, jaw, fangs
		const Basis hub_rot = Basis(anim::quat_from_rotvec(alive ? recoil * 0.9f : Vector3())) * Basis::from_euler(p.body_rot + tilt_);
		rig.pose(body_, hub_rot, p.body_off);
		const Vector3 ar = p.abd_rot + abd_spring_.x * 0.06f;
		rig.pose(abd_, Basis::from_euler(ar), Vector3(), p.abd_scale);
		face_.look_weight = in.state == State::Windup || in.state == State::Attack ? 1.0f : (look_away_ ? 0.0f : -1.0f);
		face_.head_lag = anim::damp(face_.head_lag, in.state == State::Windup ? 0.04f : 0.15f, 0.08f, dt);
		face_.wide = p.wide;
		face_.shut = p.shut;
		face_.update(in, rig);
		// It looks up at whoever it watches, but only a little (the showcase's chase lure is 40 m up).
		const Vector2 look(face_.head.x, Math::max(face_.head.y, -0.12f));
		rig.pose(head_, Basis::from_euler(anim::look_euler(look)) * Basis::from_euler(p.head_rot));
		for (int s = 0; s < 2; ++s) {
			if (Node3D *pn = rig.node(pupil_[s])) {
				Vector3 off = pn->get_position() - rig.rest(pupil_[s]).origin;
				if (off.y > 0.004f) {
					off.y = 0.004f;
					rig.pose(pupil_[s], Basis(), off);
				}
			}
		}
		const bool squint = p.squint > 0.5f;
		rig.show(squint_, squint);
		if (squint) {
			for (int s = 0; s < 2; ++s) {
				rig.pose(eye_[s], Basis(), Vector3(), Vector3(1.0f, 0.02f, 1.0f));
			}
			for (int k = 0; k < 3; ++k) {
				rig.pose(small_[k], Basis(), Vector3(), Vector3(1.0f, 0.1f, 1.0f));
			}
		}
		set_glows(rig, p);
		const float mo = Math::clamp(p.jaw, 0.0f, 1.2f);
		rig.show(jaw_, mo > 0.03f);
		rig.pose(jaw_, Basis(), Vector3(), Vector3(1.0f + 0.1f * mo, Math::max(mo, 0.02f), 1.0f));
		for (int s = 0; s < 2; ++s) {
			const float spread = 0.42f * p.fang;
			rig.pose(fang_[s], Basis(Vector3(0, 0, 1), SIDE[s] * spread) * Basis(Vector3(1, 0, 0), -0.35f * p.fang - 0.25f * mo));
		}

		// ---- legs: IK in the hub's space
		const Transform3D hub_model = rig.model_xf(body_);
		const Transform3D to_hub = hub_model.affine_inverse();
		for (int i = 0; i < NLEG; ++i) {
			const Leg &L = leg_[i];
			const Vector3 planted = to_hub.xform(foot_model_[i]);
			Vector3 target = planted.lerp(air[i], air_w);
			if (fidget_kind_ == 0 && fidget_t_ < 1.3f && i == fidget_leg_) { // a front foot taps the ground
				const float env = anim::smooth_pulse(fidget_t_, 0.2f, 0.85f, 0.25f);
				const float tap = std::fabs(std::sin(fidget_t_ * TAU * 2.4f));
				target += Vector3(0.0f, 0.03f + 0.07f * tap, 0.06f) * env;
			}
			const Vector3 pole = L.root + L.out * 0.3f + Vector3(0.0f, 0.5f, 0.0f);
			L.solve(rig, target, pole);
		}

		// ---- tuft
		if (far) {
			tuft_dangle_.reset();
			rig.bend(tuft_, Vector3());
		} else {
			anim::DangleParams tp;
			tp.len = tuft_len_;
			tp.hz = 3.6f;
			tp.zeta = 0.22f;
			tp.drag = 0.15f;
			tp.limit = 0.8f;
			rig.bend(tuft_, tuft_dangle_.step(rig.world_rest(tuft_), Vector3(0, 1, 0), tp, dt));
		}

		// ---- the root
		const Basis yawb(Vector3(0, 1, 0), in.yaw);
		const Basis lb = Basis(anim::quat_from_rotvec(lean));
		// turned (and shrunk) about `pivot`, so a roll or a flip does not swing the body through the ground
		const Basis rb = yawb * lb * Basis::from_scale(Vector3(root_scale, root_scale, root_scale));
		root = Transform3D(rb, yawb.xform(pivot - lb.xform(pivot) * root_scale) + extra);
	}

private:
	// ---- locomotion: a low scuttle; the hub sways as the groups alternate, the abdomen breathes and lags
	Pose locomotion(const AnimInput &in, float run) const {
		Pose p;
		const float amt = saturate(in.speed / 0.6f);
		const float ph = phase_ * TAU;
		const float breath = std::sin(breath_ph_);
		p.body_off = Vector3(0.0f, -0.015f * run + 0.004f * breath * (1.0f - amt), 0.0f);
		p.body_rot = Vector3(0.05f * run + 0.012f * breath * (1.0f - amt), 0.05f * amt * std::sin(ph), 0.035f * amt * std::sin(ph + 0.6f) - Math::clamp(in.accel_local.x * 0.02f, -0.12f, 0.12f));
		p.abd_rot = Vector3(-0.05f * run + 0.03f * breath, -0.07f * amt * std::sin(ph - 0.7f), 0.03f * amt * std::sin(ph - 0.2f));
		const float b = 1.0f + 0.03f * breath;
		p.abd_scale = Vector3(1.0f + 0.012f * breath, b, 1.0f + 0.012f * breath);
		// a curious little head tilt standing about, bobbing with the steps when moving
		const float tilt = anim::noise1(in.t * 0.35f + phase0_, in.seed + 11u) * (1.0f - amt);
		p.head_rot = Vector3(0.04f * amt * std::sin(ph * 2.0f), 0.0f, 0.18f * tilt);
		const float clk = anim::pulse(click_t_, 0.04f, 0.3f);
		p.fang = 0.5f * clk * (0.5f + 0.5f * std::sin(in.t * TAU * 11.0f)) + 0.12f * run;
		p.jack = 0.75f + 0.12f * breath;
		if (in.chasing) {
			p.wide = 1.06f;
			p.iris = 0.35f;
		}
		return p;
	}

	// ---- the wind-up: crouch and splay, then the cat's wiggle (the abdomen up and swinging, faster and
	// faster), fangs clicking, a hiss; in the last fifth it loads (drops lower, the nose comes up)
	Pose windup(const AnimInput &in, float w) const {
		Pose p;
		const float c = smoothstep01(remap01(w, 0.0f, 0.3f));
		const float env = smoothstep01(remap01(w, 0.22f, 0.38f)) * (1.0f - smoothstep01(remap01(w, 0.84f, 0.95f)));
		const float load = smoothstep01(remap01(w, 0.8f, 1.0f));
		const float tremble = anim::noise1(in.t * 30.0f, in.seed + 5u) * env;
		p.body_off = Vector3(0.004f * tremble, -0.1f * c - 0.035f * load, -0.035f * c - 0.025f * load);
		p.body_rot = Vector3(0.14f * c - 0.3f * load, 0.0f, 0.02f * tremble);
		const float wig = std::sin(wig_ph_);
		p.abd_rot = Vector3(0.34f * c - 0.2f * load, 0.32f * env * wig, -0.12f * env * wig);
		p.abd_scale = Vector3(1.0f, 1.0f + 0.04f * env * std::sin(wig_ph_ * 2.0f), 1.0f);
		p.head_rot = Vector3(-0.08f * c, 0.0f, 0.0f);
		p.splay = 0.065f * c;
		p.jaw = 0.18f * env + 0.85f * load;
		p.fang = 0.4f * c + 0.6f * env * (0.5f + 0.5f * std::sin(in.t * TAU * 9.0f)) + 0.6f * load;
		p.wide = 1.0f - 0.1f * env + 0.22f * load;
		p.iris = lerp(0.3f, 1.0f, w);
		p.glint = lerp(0.45f, 1.4f, w);
		p.jack = lerp(0.8f, 1.9f, c) + 0.3f * env * std::sin(wig_ph_);
		return p;
	}

	// ---- the pounce (Attack): stretched off the ground nose up, legs tucked, star-spread at the top, then
	// nose down reaching for the landing; mouth wide, fangs out, eyes blazing
	Pose pounce(float t, float vy) const {
		Pose p;
		const float launch = 1.0f - smoothstep01(t / 0.12f);
		// the crouch is let go over the first 0.14 s (it was dropped in two frames: a 14 cm pop)
		const float hold = 1.0f - smoothstep01(t / 0.14f);
		p.body_off = Vector3(0.0f, -0.135f * hold, -0.06f * hold);
		p.splay = 0.065f * hold;
		p.body_rot = Vector3(Math::clamp(-vy * 0.06f, -0.35f, 0.35f), 0.0f, 0.0f);
		p.abd_rot = Vector3(Math::clamp(-vy * 0.055f, -0.3f, 0.3f), 0.0f, 0.0f);
		p.abd_scale = Vector3(1.0f - 0.06f * launch, 1.0f - 0.08f * launch, 1.0f + 0.14f * launch);
		p.jaw = 1.0f;
		p.fang = 1.0f;
		p.wide = 1.22f;
		p.iris = 1.0f;
		p.glint = 1.5f;
		p.jack = 1.6f;
		p.head_rot = Vector3(-0.1f, 0.0f, 0.0f);
		return p;
	}

	// ---- Recover: the landing dip is a spring kick (see update); then a smug little settle and a nom
	void recover(const AnimInput &in, Pose &p) const {
		const float t = in.state_t;
		const float nom = anim::smooth_pulse(t - 0.35f, 0.1f, 0.5f, 0.3f);
		p.fang = Math::max(p.fang, nom * (0.5f + 0.5f * std::sin(in.t * TAU * 7.0f)));
		p.jaw = Math::max(p.jaw, 0.25f * nom * (0.5f + 0.5f * std::sin(in.t * TAU * 7.0f)));
		const float shake = anim::smooth_pulse(t - 0.2f, 0.08f, 0.3f, 0.2f);
		p.abd_rot.y += 0.14f * shake * std::sin(in.t * TAU * 6.0f);
		p.jack = lerp(p.jack, 1.2f, 1.0f - smoothstep01(t / 0.8f));
	}

	void fidgets(const AnimInput &in, Pose &p) {
		look_away_ = false;
		if (fidget_t_ >= 1.6f) {
			return;
		}
		if (fidget_kind_ == 0) { // tap a front foot; look at it
			const float env = anim::smooth_pulse(fidget_t_, 0.2f, 0.85f, 0.25f);
			p.head_rot += Vector3(0.12f, (fidget_leg_ == 0 ? 0.3f : -0.3f), 0.0f) * env;
			look_away_ = true;
		} else { // a slow look round with a head tilt and a ripple of the small eyes
			const float env = anim::smooth_pulse(fidget_t_, 0.3f, 0.8f, 0.4f);
			const float u = fidget_t_ / 1.6f;
			p.head_rot += Vector3(-0.05f, 0.45f * std::sin(TAU * u), 0.2f * std::sin(TAU * u)) * env;
			p.body_rot.y += 0.12f * std::sin(TAU * u) * env;
			if (fidget_t_ < 0.02f) {
				face_.blink_now();
			}
			look_away_ = true;
		}
	}

	// ---- air poses (the hub's space) and their weight
	void air_targets(const AnimInput &in, const Pose &p, float ouch, float &w) {
		using State = MonsterAnimState;
		const bool dying = in.state == State::Dying, rising = in.state == State::Rise;
		float want = 0.0f;
		for (int i = 0; i < NLEG; ++i) {
			const Leg &L = leg_[i];
			const float front = L.k < 2 ? 1.0f : -1.0f;
			const Vector3 tuck = L.root + L.out * 0.15f + Vector3(0.0f, -0.17f, 0.03f * front);
			const Vector3 star = L.root + L.out * 0.62f + Vector3(0.0f, -0.02f, 0.0f);
			const Vector3 reach = L.root + L.out * 0.44f + Vector3(0.0f, -0.3f, 0.12f * front);
			const Vector3 push = L.root + L.out * 0.4f + Vector3(0.0f, -0.32f, -0.12f);
			Vector3 t;
			if (dying) {
				const float k = in.state_t / DEATH_TIME;
				const float twitch = (1.0f - k) * anim::noise1(in.t * 9.0f + float(i) * 3.1f, in.seed + 31u);
				const Vector3 curl = L.root + L.out * 0.07f + Vector3(0.0f, -0.12f + 0.04f * twitch, 0.0f);
				t = star.lerp(curl, smoothstep01(remap01(k, 0.06f, 0.32f)));
			} else if (rising) {
				const float k = in.state_t / RISE_TIME;
				const float paddle = 0.04f * std::sin(in.t * 14.0f + float(i) * 1.7f);
				const Vector3 hang = tuck + Vector3(0.0f, paddle, paddle * 0.6f);
				t = hang.lerp(star, smoothstep01(remap01(k, 0.45f, 0.65f)));
			} else if (leap_in_) {
				const float vy = leap_vy_;
				const float ws = 1.0f - smoothstep01(std::fabs(vy) / 3.0f);
				const Vector3 other = vy > 0.0f ? tuck : reach;
				t = other.lerp(star, ws).lerp(push, 1.0f - smoothstep01(leap_t_ / 0.09f));
			} else if (ouch > 0.0f || in.since_hit < 0.7f) {
				const float kick = std::sin(in.t * 17.0f + float(i) * 2.3f);
				t = L.root + L.out * (0.46f + 0.08f * kick) + Vector3(0.0f, -0.1f + 0.1f * kick, 0.0f);
			} else {
				t = reach.lerp(star, 0.3f);
			}
			air_[i] = t;
		}
		if (dying) {
			want = 1.0f;
		} else if (rising) {
			want = 1.0f - smoothstep01(remap01(in.state_t / RISE_TIME, 0.66f, 0.85f));
		} else if (leap_in_ && !leap_real_) {
			const float u = leap_t_ / LEAP_TIME;
			want = u > 0.02f && u < 0.85f ? 1.0f : 0.0f; // back on the ground (0.1 s blend) as it touches down
		} else if (!in.on_floor) {
			want = 1.0f;
		}
		// in the air at once (a tuck reads instantly), back on the ground over 0.1 s
		const float dt = anim::safe_dt(in.dt);
		if (want > w_air_) {
			w_air_ = Math::min(want, w_air_ + dt / 0.05f);
		} else {
			w_air_ = Math::max(want, w_air_ - dt / 0.1f);
		}
		if (rising || dying) {
			w_air_ = want;
		}
		w = smoothstep01(w_air_);
	}

	// The walkable height under (x, z): kerbs, sidewalks and porch steps included when the game gives a surface
	// probe (a physics ray), else the bare terrain (a foot beside a kerb then sank into it or floated over the
	// road). Rays are cached on a 5 cm grid, so a planted or slowly moving foot costs nothing.
	float surface_y(const AnimInput &in, float x, float z) {
		if (in.surface && *in.surface) {
			const int ix = int(std::floor(x * 20.0f)), iz = int(std::floor(z * 20.0f));
			SurfCache &c = surf_[((uint32_t(ix) * 73856093u) ^ (uint32_t(iz) * 19349663u)) % SURF_N];
			if (!c.valid || c.ix != ix || c.iz != iz || std::fabs(c.hint - in.origin_world.y) > 0.3f) {
				c = SurfCache{ ix, iz, in.origin_world.y, (*in.surface)(x, z, in.origin_world.y), true };
			}
			return c.y;
		}
		if (in.ground && *in.ground) {
			return (*in.ground)(x, z);
		}
		return in.origin_world.y;
	}

	float foot_y(const AnimInput &in, float x, float z) {
		if (!in.surface && (!in.ground || !*in.ground)) {
			return in.origin_world.y;
		}
		const float g0 = surface_y(in, in.origin_world.x, in.origin_world.z);
		return in.origin_world.y + Math::clamp(surface_y(in, x, z) - g0, -0.25f, 0.25f);
	}

	// ---- foot planting in the world (see the header comment)
	void plant(const AnimInput &in, const Pose &p, float run, float air_w, float dt) {
		const Basis yawb(Vector3(0, 1, 0), in.yaw);
		const Transform3D F(yawb, in.origin_world);
		const Transform3D Finv = F.affine_inverse();
		const float dyaw = anim::angle_delta(last_yaw_, in.yaw);
		last_yaw_ = in.yaw;
		const bool teleport = planted_ && in.origin_world.distance_to(last_origin_) > 2.0f;
		last_origin_ = in.origin_world;
		Vector3 home[NLEG];
		for (int i = 0; i < NLEG; ++i) {
			const Leg &L = leg_[i];
			Vector3 h = F.xform(L.rest_foot + L.out * p.splay);
			h.y = foot_y(in, h.x, h.z);
			home[i] = h;
		}
		if (!planted_ || teleport) {
			for (int i = 0; i < NLEG; ++i) {
				foot_[i] = from_[i] = home[i];
			}
			planted_ = true;
			phase_ = 0.0f;
		}
		const bool grounded = air_w < 0.999f;
		if (!grounded) {
			// in the air: the feet go where the air pose puts them (dropped onto the ground), ready to plant
			for (int i = 0; i < NLEG; ++i) {
				Vector3 w = F.xform(hub_model_rest_xf().xform(air_[i]));
				w.y = foot_y(in, w.x, w.z);
				foot_[i] = from_[i] = w;
			}
			phase_ = std::floor(phase_ * 2.0f + 0.999f) * 0.5f; // land on a boundary: nobody mid-swing
			settling_ = true;
		} else {
			const float stride = lerp(0.36f, 0.56f, run);
			const bool moving = in.speed > 0.1f || std::fabs(dyaw) > 0.4f * dt;
			float next = phase_;
			if (moving) {
				next += (in.speed * dt + std::fabs(dyaw) * 0.45f) / stride;
				settling_ = true;
			} else if (settling_) {
				next += dt * 2.4f;
			} else {
				// standing on a boundary: start settling if a foot strays (a stance change, a push)
				for (int i = 0; i < NLEG; ++i) {
					if (Vector2(foot_[i].x - home[i].x, foot_[i].z - home[i].z).length() > SETTLE) {
						settling_ = true;
						break;
					}
				}
			}
			const int half0 = int(std::floor(phase_ * 2.0f));
			const int half1 = int(std::floor(next * 2.0f));
			if (half1 != half0) {
				// a boundary: the group that was swinging lands where it was headed; the other lifts
				const bool a_was = (half0 & 1) == 0;
				for (int i = 0; i < NLEG; ++i) {
					if (leg_[i].group_a == a_was) {
						foot_[i] = target_[i];
					}
				}
				if (!moving) {
					bool rest = true;
					for (int i = 0; i < NLEG; ++i) {
						if (Vector2(foot_[i].x - home[i].x, foot_[i].z - home[i].z).length() > SETTLE) {
							rest = false;
						}
					}
					if (rest) {
						next = float(half1) * 0.5f;
						settling_ = false;
					}
				}
				const bool a_now = (half1 & 1) == 0;
				for (int i = 0; i < NLEG; ++i) {
					if (leg_[i].group_a == a_now) {
						from_[i] = foot_[i];
					}
				}
				land_kick_ = true;
			}
			phase_ = next - std::floor(next); // one cycle = group A's half, then group B's
			const int half = int(std::floor(phase_ * 2.0f));
			const float u = phase_ * 2.0f - float(half);
			const bool a_swings = (half & 1) == 0;
			Vector3 vel_w = yawb.xform(Vector3(in.vel_local.x, 0.0f, in.vel_local.z));
			const float sp = vel_w.length();
			const Vector3 lead = sp > 1e-3f ? vel_w / sp * (stride * 0.25f * saturate(sp / 0.5f)) : Vector3();
			const float lift = 0.05f + 0.035f * run;
			for (int i = 0; i < NLEG; ++i) {
				Vector3 tw = home[i] + lead;
				tw.y = foot_y(in, tw.x, tw.z);
				target_[i] = tw;
				if (leg_[i].group_a == a_swings && (settling_ || moving) && u > 0.0f) {
					Vector3 pos = from_[i].lerp(tw, smoothstep01(u));
					// a foot that is already home only shuffles; a real step lifts fully
					const float span = Vector2(tw.x - from_[i].x, tw.z - from_[i].z).length();
					pos.y += lift * anim::arc(u) * Math::max(saturate(span / 0.1f), moving ? 0.4f : 0.0f);
					foot_[i] = pos;
				}
			}
			if (land_kick_) {
				land_kick_ = false;
				if (moving && in.on_floor) {
					dip_.kick(-0.18f - 0.25f * run);
				}
			}
		}
		for (int i = 0; i < NLEG; ++i) {
			foot_model_[i] = Finv.xform(foot_[i]);
		}
		// the terrain's slope under the body tips the hub (front/back, side to side)
		if (in.ground && *in.ground && in.on_floor) {
			const auto gy = [&](const Vector3 &m) { const Vector3 w = F.xform(m); return foot_y(in, w.x, w.z); };
			const float pitch = std::atan2(gy(Vector3(0, 0, 0.35f)) - gy(Vector3(0, 0, -0.35f)), 0.7f);
			const float roll = std::atan2(gy(Vector3(0.35f, 0, 0)) - gy(Vector3(-0.35f, 0, 0)), 0.7f);
			tilt_ = anim::damp(tilt_, Vector3(Math::clamp(-pitch, -0.3f, 0.3f), 0.0f, Math::clamp(roll, -0.3f, 0.3f)), 0.1f, dt);
		} else {
			tilt_ = anim::damp(tilt_, Vector3(), 0.1f, dt);
		}
	}

	Transform3D hub_model_rest_xf() const { return hub_rest_; }

	// ---- Rise (0.6 s): it drops in head first on a thread from its spinnerets, curled up and turning,
	// flips over as it lands (legs flung out), the thread goes, it settles and its eyes pop open
	void rise(const AnimInput &in, Pose &p, Vector3 &lean, Vector3 &extra, Vector3 &pivot, MonsterRig &rig) {
		const float k = saturate(in.state_t / RISE_TIME);
		const float drop = 1.0f - anim::ease_in_out(remap01(k, 0.0f, 0.45f));
		const float flip = anim::ease_out_back(remap01(k, 0.42f, 0.72f), 1.8f);
		const float pitch = 1.35f * (1.0f - flip);
		pivot = spin_;
		lean = Vector3(pitch, 0.7f * (1.0f - smoothstep01(remap01(k, 0.0f, 0.6f))) * (mirror_ ? -1.0f : 1.0f), 0.0f);
		const float hang = 0.72f + 0.55f * drop; // the thread end's height above its rest height
		extra = Vector3(0.0f, hang * (1.0f - smoothstep01(remap01(k, 0.42f, 0.7f))), 0.0f);
		p.shut = 1.0f - smoothstep01(remap01(k, 0.76f, 0.84f));
		p.wide = 1.0f + 0.25f * anim::smooth_pulse(k - 0.78f, 0.04f, 0.04f, 0.14f);
		p.abd_rot += Vector3(-0.15f * (1.0f - flip), 0.0f, 0.0f);
		p.fang = 0.6f * (1.0f - flip);
		p.jack = lerp(0.25f, p.jack, smoothstep01(remap01(k, 0.6f, 0.9f)));
		p.iris = lerp(0.05f, p.iris, smoothstep01(remap01(k, 0.76f, 0.9f)));
		if (k > 0.66f && !rise_landed_) {
			rise_landed_ = true;
			dip_.kick(-1.6f);
		}
		// the thread: from the spinnerets straight up, stretched, until it snaps as it flips
		const bool show = k < 0.5f;
		rig.show(silk_, show);
		if (show) {
			const Basis lb = Basis(anim::quat_from_rotvec(lean));
			const Transform3D abd_model = rig.model_xf(abd_);
			const Vector3 up_model = lb.xform_inv(Vector3(0, 1, 0));
			const Vector3 dir = abd_model.basis.xform_inv(up_model).normalized();
			const Quaternion q = anim::quat_between(Vector3(0, 0, -1), dir);
			rig.pose(silk_, Basis(q), Vector3(), Vector3(1.0f, 1.0f, 14.0f));
		}
	}

	// ---- Death (0.6 s): a jolt with the legs thrown out stiff, then it rolls over onto its back with the
	// legs curling in and twitching, the jack face goes out; it shrinks away under the game's puff
	void death(const AnimInput &in, Pose &p, Vector3 &lean, Vector3 &extra, Vector3 &pivot, float &root_scale) {
		const float k = saturate(in.state_t / DEATH_TIME);
		const float roll = PI * anim::ease_in_out(remap01(k, 0.05f, 0.36f)); // over before the puff hides it
		pivot = Vector3(0.0f, SPIN_HUB_Y, -0.08f);
		lean = Vector3(0.0f, 0.0f, roll * (mirror_ ? 1.0f : -1.0f));
		extra = Vector3(0.0f, 0.14f * anim::arc(remap01(k, 0.0f, 0.3f)), 0.0f);
		p.squint = k > 0.08f ? 1.0f : 0.0f;
		p.jaw = 0.65f;
		p.fang = 1.0f;
		p.wide = 1.0f + 0.25f * (1.0f - smoothstep01(k / 0.1f));
		p.jack = lerp(1.6f, 0.0f, smoothstep01(remap01(k, 0.15f, 0.7f)));
		p.iris = 0.0f;
		p.abd_scale = Vector3(1.04f, 0.92f, 1.0f);
		root_scale = 1.0f - 0.92f * smoothstep01(remap01(k, 0.86f, 1.0f));
	}

	void land_kick() {
		dip_.kick(-3.6f); // down to about 60 percent of its height, and back up past rest
		abd_spring_.kick(Vector3(5.0f, 0.0f, 0.0f));
		land_t_ = 0.0f;
	}

	void set_glows(MonsterRig &rig, const Pose &p) {
		const float iris = Math::max(0.0f, p.iris), glint = Math::max(0.0f, p.glint), jack = Math::max(0.0f, p.jack);
		if (std::fabs(iris - iris_sent_) > 0.01f || std::fabs(glint - glint_sent_) > 0.01f) {
			iris_sent_ = iris;
			glint_sent_ = glint;
			for (int s = 0; s < 2; ++s) {
				rig.glow(pupil_[s], iris);
				rig.glow(eye_[s], glint);
			}
			for (int k = 0; k < 3; ++k) {
				rig.glow(small_[k], 0.5f * iris + 0.3f * glint);
			}
		}
		if (std::fabs(jack - jack_sent_) > 0.01f) {
			jack_sent_ = jack;
			rig.glow(abd_, jack);
		}
	}

	int body_ = -1, abd_ = -1, head_ = -1, jaw_ = -1, tuft_ = -1, squint_ = -1, silk_ = -1;
	int fang_[2] = { -1, -1 }, eye_[2] = { -1, -1 }, pupil_[2] = { -1, -1 }, small_[3] = { -1, -1, -1 };
	Leg leg_[NLEG];
	Vector3 foot_[NLEG], from_[NLEG], target_[NLEG], foot_model_[NLEG], air_[NLEG];
	Transform3D hub_rest_;
	Vector3 spin_;
	Vector3 last_origin_;
	Vector3 tilt_; // terrain tilt of the hub: x pitch, z roll
	MonsterFace face_;
	HitRecoil hit_;
	anim::Weight w_wind_, w_pounce_;
	anim::Spring dip_;
	anim::Spring3 abd_spring_;
	anim::Dangle tuft_dangle_;
	anim::RandTimer fidget_, click_;
	float tuft_len_ = 0.1f;
	float phase_ = 0.0f, last_yaw_ = 0.0f, w_air_ = 0.0f;
	float wind_ = 0.0f, pounce_t_ = 0.0f, pounce_vy_ = 0.0f, land_t_ = 10.0f, hurt_t_ = 1000.0f;
	float breath_ph_ = 0.0f, wig_ph_ = 0.0f, click_t_ = 10.0f, fidget_t_ = 10.0f, fall_ = 0.0f, phase0_ = 0.0f;
	float iris_sent_ = -1.0f, glint_sent_ = -1.0f, jack_sent_ = -1.0f;
	int fidget_kind_ = 0, fidget_leg_ = 0;
	bool seeded_ = false, mirror_ = false, was_floor_ = true, planted_ = false, settling_ = false, land_kick_ = false;
	bool look_away_ = false, rise_landed_ = false;
	float leap_t_ = 10.0f, leap_vy_ = 0.0f, leap_h_ = 0.0f;
	bool leap_on_ = false, leap_real_ = false, leap_in_ = false;
	struct SurfCache {
		int ix = 0, iz = 0;
		float hint = 0.0f, y = 0.0f;
		bool valid = false;
	};
	static constexpr int SURF_N = 64;
	SurfCache surf_[SURF_N];
};

} // namespace

TOT_MONSTER_ANIMATOR(MonsterType::Spider, SpiderAnimator)

} // namespace tot
