// Goo Slime (a THROW monster that hops): the animator for its v2 rig.
//
// Part tree (art/team/monsters/batch04/build_monsters2.py, slime()):
//   body (meshless joint on the ground) > head (the whole gumdrop of jelly, pivot on the ground)
//        > skirt (the ring of drips: spreads, ripples by cloth_wave), curl (ONE soft tube pinned in the
//          peak: a Dangle plus authored flicks), eye_l / eye_r > pupil_l / pupil_r, brow_l / brow_r,
//          cheek_l / cheek_r (puff), mouth (scaled open about the lip line; X-narrowed to an O),
//          alt_squint_l / _r (> <), alt_happy_l / _r (^ ^), bubble_a .. bubble_d (slide on springs),
//          alt_goo (never shown: the look of the thrown goo, handed to Combat by thrown_part)
//   socket_muzzle on the head, in front of the mouth: where the goo leaves.
// The head carries all the jelly: a volume-preserving squash / stretch on a ringing spring, an extra
// spread and flatten (puddle), a uniform size (recover), and a shear wobble on a second spring (the
// top lags starts, stops and hops) composed with the look turn. Everything on the face rides on it.
//
// A frame: a locomotion pose (idle breathing, the hop cycle read from on_floor / vel / hop_in), state
// layers mixed over it by weight (wind-up, lob, recover, hit), springs (squash, shear, face slide,
// bubbles, skirt spread, curl), one solve. Rise gathers it up out of a puddle; death splats it into one.

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
constexpr int NB = 4; // sliding bubbles

// ---- tuning --------------------------------------------------------------------------------------
constexpr float HOP_SPEED = 2.4f; // horizontal speed of a chase hop (tuning.speed * 1.4)
constexpr float RISE_TIME = 0.6f, DEATH_TIME = 0.6f;
constexpr float LOB_TIME = 0.3f; // the THROW attack state (monsters.cpp attack_step, default)
constexpr float HIT_TIME = 0.7f; // the visual hit reaction; Hurt itself is 0.28 s
constexpr float COOLDOWN = 3.0f; // tuning.cooldown (it recovers over this, plus up to 0.8 s)
constexpr float PUDDLE_Y = 0.06f; // y scale of the jelly as a puddle
constexpr float FIDGET_TIME = 0.9f;

struct Pose {
	float sy = 1.0f; // volume preserving squash / stretch target (springy)
	float flat = 1.0f; // extra y scale (puddle), not springy
	float spread = 1.0f; // extra x / z scale (puddle), not springy
	float size = 1.0f; // uniform scale (it shrinks after a lob and regrows)
	Vector3 shear; // shear target: the top's lean per metre of height (x = its left, z = forward)
	Vector3 lean; // root lean (rotation vector, facing frame)
	float skirt = 1.0f; // the drips' spread
	float ripple = 0.003f, ripple_rate = 3.0f;
	Vector3 curl; // authored curl bend (rotation vector, head space: x < 0 cocked back, z < 0 curled over)
	float mouth = 0.85f, mouth_round = 0.0f, puff = 0.0f;
	float frown = 0.0f, brow_raise = 0.0f;
	float squint = 0.0f, happy = 0.0f, shut = 0.0f, wide = 1.0f;
	float glow = 0.35f; // glint glow
	float look = -1.0f; // MonsterFace look weight (-1 = while chasing)
	float face_y = 0.0f; // the face's slide on the jelly (m, head space)
	float tremble = 0.0f;
	float ground = 0.0f; // 1 = drop the picture onto the ground (puddle during spawn / death)
};

void mix(Pose &d, const Pose &s, float w) {
	if (w <= 0.0f) {
		return;
	}
	w = saturate(w);
	d.sy = lerp(d.sy, s.sy, w);
	d.flat = lerp(d.flat, s.flat, w);
	d.spread = lerp(d.spread, s.spread, w);
	d.size = lerp(d.size, s.size, w);
	d.shear = d.shear.lerp(s.shear, w);
	d.lean = d.lean.lerp(s.lean, w);
	d.skirt = lerp(d.skirt, s.skirt, w);
	d.ripple = lerp(d.ripple, s.ripple, w);
	d.ripple_rate = lerp(d.ripple_rate, s.ripple_rate, w);
	d.curl = d.curl.lerp(s.curl, w);
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
	d.face_y = lerp(d.face_y, s.face_y, w);
	d.tremble = lerp(d.tremble, s.tremble, w);
	d.ground = lerp(d.ground, s.ground, w);
}

// Basis that slides the top of a part sideways by `sh` per unit of height (a jelly's wobble).
Basis shear_basis(const Vector3 &sh) {
	Basis b;
	b.set_column(1, Vector3(sh.x, 1.0f, sh.z));
	return b;
}

class SlimeAnimator : public MonsterAnimator {
public:
	float rise_time() const override { return RISE_TIME; }
	float death_time() const override { return DEATH_TIME; }
	float telegraph_gain() const override { return 1.8f; } // (final fixes 2) linear body_flash: at least ~0.9 of BEFORE's pulse (flash_metric.py)
	float hit_flash_gain() const override { return 1.4f; } // (final fixes 2) the fade after the white peak (linear body_flash)
	int thrown_part(const MonsterRig &rig) const override { return goo_; }

	bool muzzle(const MonsterRig &rig, Vector3 &out_world) const override {
		Transform3D xf;
		if (!rig.socket_world("muzzle", xf)) {
			return false;
		}
		out_world = xf.origin;
		return true;
	}

	void bind(MonsterRig &rig) override {
		body_ = rig.slot("body");
		head_ = rig.slot("head");
		skirt_ = rig.slot("skirt");
		curl_ = rig.slot("curl");
		mouth_ = rig.slot("mouth");
		goo_ = rig.slot("alt_goo");
		static const char *names[2][6] = {
			{ "eye_l", "pupil_l", "brow_l", "cheek_l", "alt_squint_l", "alt_happy_l" },
			{ "eye_r", "pupil_r", "brow_r", "cheek_r", "alt_squint_r", "alt_happy_r" },
		};
		for (int s = 0; s < 2; ++s) {
			eye_[s] = rig.slot(names[s][0]);
			pupil_[s] = rig.slot(names[s][1]);
			brow_[s] = rig.slot(names[s][2]);
			cheek_[s] = rig.slot(names[s][3]);
			squint_[s] = rig.slot(names[s][4]);
			happy_[s] = rig.slot(names[s][5]);
		}
		static const char *bnames[NB] = { "bubble_a", "bubble_b", "bubble_c", "bubble_d" };
		for (int i = 0; i < NB; ++i) {
			bubble_[i] = rig.slot(bnames[i]);
		}
		face_.bind(rig);
		face_.max_yaw = 0.5f;
		face_.max_pitch = 0.12f; // the whole jelly turns: a little goes a long way
		face_.look_height = 0.5f;
		face_.pupil_travel = 0.2f;
		face_.closed_scale = 0.08f;
		face_.glow_rest = -1.0f; // the glints' glow is driven here
		// Wet jelly: a tight catchlight on the body (gloss), a warm lime rim (the light through it),
		// glossy eyes with a matte iris and pupil (no chrome lens rim), matte brows.
		const Color rim(0.8f, 1.0f, 0.3f);
		for (int i = 0; i < rig.count(); ++i) {
			rig.gloss(i, 0.5f);
			rig.param(i, "rim_strength", 0.0f);
		}
		for (int slot : { head_, skirt_, curl_, cheek_[0], cheek_[1], bubble_[0], bubble_[1], bubble_[2], bubble_[3] }) {
			rig.param(slot, "rim_strength", 0.3f);
			rig.param(slot, "rim_color", rim);
		}
		for (int i = 0; i < NB; ++i) {
			rig.gloss(bubble_[i], 0.9f);
		}
		rig.gloss(mouth_, -2.0f); // a dark mouth stays dark (no sheen greying the cavity)
		for (int s = 0; s < 2; ++s) {
			rig.gloss(eye_[s], 0.35f);
			rig.gloss(pupil_[s], -2.0f);
			rig.gloss(brow_[s], -1.0f);
			rig.gloss(squint_[s], -1.0f);
			rig.gloss(happy_[s], -1.0f);
			rig.param(eye_[s], "rim_strength", 0.08f);
			rig.show(squint_[s], false);
			rig.show(happy_[s], false);
		}
		rig.show(goo_, false);
		// the curl bends from just above its pin, by distance from it
		rig.soft(curl_, Vector3(), -1.0f, 0.04f);
		for (int i = 0; i < NB; ++i) {
			bubble_rest_[i] = rig.rest(bubble_[i]).origin;
		}
	}

	void update(const AnimInput &in, MonsterRig &rig, Transform3D &root) override {
		using State = MonsterAnimState;
		const float dt = anim::safe_dt(in.dt);
		const bool far = in.lod > 0;
		if (!seeded_) {
			seeded_ = true;
			fidget_timer_ = anim::RandTimer(in.seed ^ 0x5113u);
			phase0_ = anim::hash01(in.seed) * TAU;
			for (int i = 0; i < NB; ++i) {
				bubble_seed_[i] = in.seed * 7u + uint32_t(i) * 131u + 17u;
			}
		}
		const bool dying = in.state == State::Dying, rising = in.state == State::Rise;
		const bool alive = !dying && !rising;
		if (dying && in.entered) {
			linger_cap_ = true;
		}
		if (rising && in.entered) {
			squash_.reset();
			shear_.reset();
		}

		// ---- the hop: take-off and landing from the body's own motion
		airborne_t_ = in.on_floor ? 0.0f : airborne_t_ + dt;
		const bool air = !in.on_floor && airborne_t_ > 0.04f;
		bool launched = false, landed = false;
		float land_speed = 0.0f;
		if (alive) {
			if (air && !was_air_ && in.vel_local.y > 1.5f) {
				launched = true;
			}
			if (air) {
				fall_ = Math::min(fall_, in.vel_local.y);
			}
			if (!air && was_air_) {
				land_speed = -fall_;
				landed = land_speed > 1.0f;
			}
			if (!air) {
				fall_ = 0.0f;
			}
		}
		was_air_ = air;
		const Vector3 hv(in.vel_local.x, 0.0f, in.vel_local.z);
		if (air) {
			air_hv_ = hv;
		}

		// ---- clocks and one-shots
		const float hspeed = alive && in.state != State::Windup ? in.speed : 0.0f;
		amt_ = far ? saturate(hspeed / HOP_SPEED) : anim::damp(amt_, saturate(hspeed / HOP_SPEED), 0.2f, dt);
		const Vector3 recoil = hit_.update(in);
		if (hit_.fresh && alive) {
			hit_t_ = 0.0f;
		}
		hit_t_ += dt;
		ripple_phase_ += dt * ripple_rate_;
		splash_ *= std::exp(-3.5f * dt);
		if (landed) {
			splash_ = Math::min(0.03f, splash_ + 0.0035f * land_speed);
			splash_phase_ = 0.0f;
		}
		splash_phase_ += dt * 16.0f;
		hopping_ = anim::damp(hopping_, air || in.hop_in > 0.0f ? 1.0f : 0.0f, 0.3f, dt);

		const bool idle = in.state == State::Roam && !in.chasing && in.speed < 0.2f && !air;
		idle_for_ = idle ? idle_for_ + dt : 0.0f;
		fidget_t_ += dt;
		if (!idle) {
			fidget_timer_.reset();
		} else if (idle_for_ > 5.0f && fidget_t_ > FIDGET_TIME + 1.0f && fidget_timer_.step(dt, 2.4f, 4.4f)) {
			fidget_t_ = 0.0f;
		}

		// ---- the pose: locomotion, then the state layers
		Pose p = locomotion(in, air);
		if (idle && fidget_t_ < FIDGET_TIME) {
			mix(p, fidget(in, p, fidget_t_), anim::smooth_pulse(fidget_t_, 0.08f, FIDGET_TIME - 0.3f, 0.22f));
		}
		w_wind_.step(in.state == State::Windup, 0.08f, 0.06f, dt);
		w_attack_.step(in.state == State::Attack, 0.03f, 0.2f, dt);
		w_recover_.step(in.state == State::Recover, 0.08f, 0.5f, dt);
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
			mix(p, lob(in, p, attack_t_), w_attack_.w);
		}
		if (in.state == State::Recover) {
			recover_t_ = in.state_t;
			recover_k_ = saturate(1.0f - in.cooldown / COOLDOWN);
		}
		if (w_recover_.w > 0.0f) {
			mix(p, recover(in, p, recover_t_, recover_k_), w_recover_.w);
		}
		const float hit_w = alive ? smoothstep01(hit_t_ / 0.04f) * (1.0f - smoothstep01(remap01(hit_t_, HIT_TIME - 0.25f, HIT_TIME))) : 0.0f;
		if (hit_w > 0.0f) {
			mix(p, hit_pose(in, p, hit_t_), hit_w);
		}
		if (dying) {
			mix(p, death_pose(in, p), smoothstep01(in.state_t / 0.05f));
		}
		if (rising) {
			mix(p, rise_pose(in, p), 1.0f);
		}

		// ---- springs: squash (a ringing ~5.5 Hz jelly), shear wobble, face slide, skirt, lean
		const Basis facing(Vector3(0, 1, 0), in.yaw);
		if (far) {
			squash_.reset(p.sy - 1.0f);
			shear_.reset(p.shear);
			face_s_.reset(p.face_y);
			skirt_s_.reset(p.skirt - 1.0f);
			lean_.reset(p.lean);
		} else {
			if (launched) {
				squash_.kick(7.0f);
				shear_.kick(-hv * 0.7f); // the top stays behind as the foot leaves
				face_s_.kick(0.35f);
				skirt_s_.kick(-0.6f);
			}
			if (landed) {
				squash_.kick(-Math::min(12.0f, 2.1f * land_speed));
				shear_.kick(air_hv_ * 0.55f); // the top carries on as the foot stops
				face_s_.kick(-0.13f * land_speed);
				skirt_s_.kick(Math::min(1.0f, 0.2f * land_speed));
			}
			if (hit_.fresh && alive) {
				squash_.kick(-9.0f);
				shear_.kick(in.hit_dir_local * 2.2f);
				face_s_.kick(-0.5f);
				skirt_s_.kick(0.8f);
			}
			if (attack_kick_) {
				squash_.kick(9.0f);
				shear_.kick(Vector3(0.0f, 0.0f, 1.4f));
				face_s_.kick(0.5f);
			}
			// starts, stops and turns: the top lags the foot
			if (alive) {
				shear_.kick(Vector3(-in.accel_local.x, 0.0f, -in.accel_local.z) * (0.035f * dt));
			}
			squash_.step(p.sy - 1.0f, 5.5f, 0.26f, dt);
			shear_.step(p.shear, 3.4f, 0.2f, dt);
			face_s_.step(p.face_y, 4.5f, 0.3f, dt);
			skirt_s_.step(p.skirt - 1.0f, 3.8f, 0.3f, dt);
			lean_.step(p.lean, 3.0f, 0.45f, dt);
		}
		const float sy = Math::clamp(1.0f + squash_.x, 0.45f, 1.6f);
		Vector3 shear = shear_.x;
		shear.y = 0.0f;
		const float sl = shear.length();
		if (sl > 0.32f) {
			shear *= 0.32f / sl;
		}
		ripple_rate_ = p.ripple_rate;

		// ---- solve: head (squash, shear, look), face, bubbles, skirt, curl
		Vector3 hscale = anim::squash_stretch(sy) * p.size;
		hscale.y *= p.flat;
		hscale.x *= p.spread;
		hscale.z *= p.spread;
		const float trem = p.tremble * std::sin(in.t * 47.0f);
		hscale.y *= 1.0f + 0.025f * trem;
		rig.pose(body_, Basis());
		const float lw = p.look < 0.0f ? (in.chasing ? 1.0f : 0.0f) : saturate(p.look);
		look_w_ = far ? lw : anim::damp(look_w_, lw, 0.12f, dt);
		face_.look_weight = look_w_;
		const bool intent = in.state == State::Windup || in.state == State::Attack;
		face_.head_lag = anim::damp(face_.head_lag, intent ? 0.05f : 0.18f, 0.08f, dt);
		face_.wide = p.wide;
		face_.shut = p.shut;
		face_.update(in, rig);
		const Basis look = Basis::from_euler(anim::look_euler(face_.head));
		rig.pose(head_, shear_basis(shear + Vector3(0.012f * trem, 0.0f, 0.0f)) * look, Vector3(), hscale);

		// the face slides on the jelly, lagging every bounce
		const Vector3 fo(0.0f, Math::clamp(face_s_.x, -0.03f, 0.03f), 0.0f);
		const bool squint = p.squint > 0.5f, happy = !squint && p.happy > 0.5f;
		const float eye_sy = p.wide * lerp(1.0f, face_.closed_scale, face_.closed);
		for (int s = 0; s < 2; ++s) {
			rig.show(squint_[s], squint);
			rig.show(happy_[s], happy);
			const float ey = squint || happy ? 0.02f : eye_sy;
			rig.pose(eye_[s], Basis(), fo, Vector3(p.wide, ey, p.wide));
			rig.pose(squint_[s], Basis(), fo);
			rig.pose(happy_[s], Basis(), fo);
		}
		if (dying) {
			linger_eyes(in, rig);
		}
		glow_ = glow_ < 0.0f || far ? p.glow : anim::damp(glow_, p.glow, 0.04f, dt);
		if (std::fabs(glow_ - glow_sent_) > 0.01f) {
			glow_sent_ = glow_;
			for (int s = 0; s < 2; ++s) {
				rig.glow(eye_[s], glow_);
			}
		}
		const float mo = Math::clamp(p.mouth, 0.0f, 1.25f);
		rig.show(mouth_, mo > 0.03f);
		rig.pose(mouth_, Basis(), fo, Vector3(1.0f - 0.42f * saturate(p.mouth_round), Math::max(mo, 0.02f), 1.0f));
		const float pf = p.puff;
		for (int s = 0; s < 2; ++s) {
			rig.pose(cheek_[s], Basis(), fo, Vector3(1.0f + 0.3f * pf, 1.0f + 0.28f * pf, Math::max(0.3f, 1.0f + 1.2f * pf)));
			rig.pose(brow_[s], Basis(Vector3(0, 0, 1), SIDE[s] * p.frown), fo + Vector3(0.0f, p.brow_raise, 0.0f));
		}
		// bubbles: each drifts on its own slow spring and is thrown about by bounces and hits
		for (int i = 0; i < NB; ++i) {
			const float ph = in.t * (0.35f + 0.1f * float(i));
			Vector3 want(0.008f * anim::noise1(ph, bubble_seed_[i]), 0.01f * anim::noise1(ph + 3.1f, bubble_seed_[i] + 5u), 0.0f);
			if (far || !alive) {
				bubble_s_[i].reset(want);
			} else {
				if (landed) {
					bubble_s_[i].kick(Vector3(0.0f, -0.06f * land_speed, 0.0f));
				}
				if (launched || attack_kick_) {
					bubble_s_[i].kick(Vector3(0.0f, 0.25f, 0.0f));
				}
				if (hit_.fresh) {
					bubble_s_[i].kick(Vector3(SIDE[i & 1] * 0.3f, 0.25f, 0.0f));
				}
				bubble_s_[i].step(want, 2.0f + 0.5f * float(i), 0.22f, dt);
			}
			Vector3 off = bubble_s_[i].x;
			off.x = Math::clamp(off.x, -0.03f, 0.03f);
			off.y = Math::clamp(off.y, -0.035f, 0.035f);
			// keep it on the surface: slide in the head's horizontal plane round the axis, and up / down
			const Vector3 r0 = bubble_rest_[i];
			const float rr = Vector2(r0.x, r0.z).length();
			Vector3 pos = r0;
			if (rr > 1e-3f) {
				const float a = std::atan2(r0.x, r0.z) + off.x / rr;
				pos = Vector3(std::sin(a) * rr, r0.y + off.y, std::cos(a) * rr);
				pos += Vector3(r0.x, 0.0f, r0.z).normalized() * (-0.25f * off.y); // the flank slopes in as it rises
			}
			const float pop = 1.0f + 0.12f * std::sin(in.t * (1.3f + 0.4f * float(i)) + float(i) * 1.7f);
			rig.pose(bubble_[i], Basis(), pos - r0, Vector3(pop, pop, pop));
		}
		// the skirt: the drips spread on landings and gather in the air; ripples run round them
		const float sk = Math::clamp(1.0f + skirt_s_.x, 0.85f, 1.3f);
		rig.pose(skirt_, Basis(), Vector3(), Vector3(sk, 1.0f, sk));
		const float rip = far ? 0.0f : p.ripple + splash_ + (alive ? 0.003f * amt_ : 0.0f);
		rig.ripple(skirt_, rip, ripple_phase_ + phase0_ + splash_phase_, -5.0f, 0.2f);

		// the curl: thrown about by every bounce (a Dangle) plus the pose's flicks
		Vector3 cb = p.curl;
		if (!far && alive) {
			const Transform3D cm = rig.model_rest(curl_);
			if (hit_.fresh) {
				curl_d_.kick(facing.xform(in.hit_dir_local) * 1.2f);
			}
			if (attack_kick_) {
				curl_d_.kick(facing.xform(Vector3(0.0f, 0.0f, -1.6f)));
			}
			if (landed) {
				curl_d_.kick(Vector3(0.0f, -0.25f * land_speed, 0.0f));
			}
			const Transform3D frame(facing * cm.basis.orthonormalized(), in.origin_world + facing.xform(cm.origin));
			anim::DangleParams cp;
			cp.len = Math::max(0.08f, rig.reach(curl_));
			cp.hz = 3.0f;
			cp.zeta = 0.2f;
			cp.inertia = 1.0f;
			cp.drag = 0.15f;
			cp.limit = 0.7f;
			Vector3 rest_dir = rig.tip(curl_);
			if (rest_dir.length() < 1e-4f) {
				rest_dir = Vector3(0, 1, 0);
			}
			cb += curl_d_.step(frame, rest_dir, cp, dt);
		} else {
			curl_d_.reset();
		}
		rig.bend(curl_, cb);
		attack_kick_ = false;

		// ---- the root: facing, lean (springy), the flinch, the drop to the ground as a puddle
		Vector3 lean = lean_.x + recoil * 0.45f;
		Vector3 offset;
		if (!alive) {
			lean = rising ? Vector3() : lean * (1.0f - smoothstep01(in.state_t / 0.15f));
		}
		if (p.ground > 0.0f) {
			float gh = 0.0f;
			if (in.ground && *in.ground) {
				gh = Math::clamp(in.origin_world.y - (*in.ground)(in.origin_world.x, in.origin_world.z), 0.0f, 1.5f);
			}
			offset.y -= gh * p.ground;
		}
		float rs = 1.0f;
		if (dying) {
			rs = 1.0f - smoothstep01(remap01(in.state_t / DEATH_TIME, 0.68f, 0.98f)); // (final fixes 3) was 0.86-1.0 (read as ~3 frames)
		}
		root = root_pose(in.yaw, lean, Vector3(rs, rs, rs), offset);
	}

private:
	// Sitting: a slow breath and a lazy jiggle. Hopping: crouch as the next hop comes, stretch on the
	// way up, round out at the top, stretch falling, then the landing squash comes from the spring.
	Pose locomotion(const AnimInput &in, bool air) const {
		Pose p;
		const float breath = std::sin(in.t * 2.4f + phase0_);
		p.sy = 1.0f + 0.022f * breath;
		p.shear = Vector3(0.012f * anim::noise1(in.t * 0.4f, 11u + uint32_t(phase0_ * 100.0f)), 0.0f, 0.008f * std::sin(in.t * 1.2f + phase0_));
		p.skirt = 1.0f - 0.01f * breath;
		p.ripple = 0.0025f;
		p.ripple_rate = 2.5f;
		p.curl = Vector3(0.0f, 0.0f, 0.06f * std::sin(in.t * 1.6f + phase0_));
		p.mouth = 0.85f;
		p.look = -1.0f;
		const float chase = in.chasing ? 1.0f : 0.0f;
		if (air) {
			const float vy = in.vel_local.y;
			p.sy = vy > 0.0f ? 1.0f + Math::min(0.24f, 0.05f * vy) : 1.0f + Math::min(0.14f, -0.03f * vy);
			p.skirt = 0.9f;
			const float fwd = saturate(Vector2(in.vel_local.x, in.vel_local.z).length() / HOP_SPEED);
			p.lean = Vector3(0.1f * fwd, 0.0f, 0.0f);
			p.shear = Vector3(0.0f, 0.0f, vy > 0.0f ? -0.05f * fwd : 0.04f * fwd);
			p.curl = Vector3(-0.15f * saturate(vy / 5.0f), 0.0f, 0.0f);
			p.mouth = vy > 0.0f ? 1.0f : 0.7f;
			p.mouth_round = vy > 0.0f ? 0.4f : 0.0f;
			p.wide = 1.06f;
			p.face_y = 0.008f;
		} else if (in.hop_in > 0.0f && in.state == MonsterAnimState::Roam) {
			// the crouch before the next hop
			const float c = smoothstep01(remap01(in.hop_in, 0.16f, 0.02f));
			p.sy = lerp(p.sy, 0.8f, c);
			p.skirt = lerp(1.0f, 1.08f, c);
			p.face_y = -0.008f * c;
			p.mouth = lerp(p.mouth, 0.5f, c);
		}
		p.frown = 0.16f * chase;
		p.brow_raise = -0.003f * chase;
		return p;
	}

	// Idle fidget (after 5 s sitting): a happy 'boing' on the spot, facing whoever is looking: dips,
	// pops up tall, wobbles; the curl wags, eyes in happy arcs.
	Pose fidget(const AnimInput &in, const Pose &base, float t) const {
		Pose p = base;
		const float dip = anim::pulse(t - 0.02f, 0.12f, 0.12f);
		const float pop = anim::pulse(t - 0.22f, 0.1f, 0.35f);
		p.sy = base.sy - 0.14f * dip + 0.14f * pop;
		p.skirt = base.skirt + 0.06f * dip;
		p.curl = base.curl + Vector3(0.0f, 0.0f, 0.35f * std::sin(t * TAU * 2.6f) * pop);
		p.happy = t > 0.18f && t < FIDGET_TIME - 0.15f ? 1.0f : 0.0f;
		p.mouth = 0.85f;
		p.look = 0.0f;
		return p;
	}

	// The wind-up (0.8 s): it flattens wide and leans back, gathering the goo: cheeks puff, mouth
	// clamped shut, brows down, eyes narrow and flare, the curl cocks back; trembling at the end, it
	// opens its mouth into an O just before the lob.
	Pose windup(const AnimInput &in, const Pose &base, float w) const {
		Pose p = base;
		const float a = smoothstep01(remap01(w, 0.0f, 0.55f));
		const float cock = smoothstep01(remap01(w, 0.7f, 1.0f));
		const float open = smoothstep01(remap01(w, 0.88f, 1.0f));
		p.sy = lerp(1.0f, 0.82f, a) - 0.05f * cock;
		p.shear = Vector3(0.0f, 0.0f, -0.07f * a - 0.02f * cock);
		p.lean = Vector3(-0.06f * a, 0.0f, 0.0f);
		p.skirt = 1.0f + 0.06f * a;
		p.ripple = 0.004f + 0.006f * cock;
		p.ripple_rate = 8.0f;
		p.curl = Vector3(-0.45f * a - 0.15f * cock, 0.0f, -0.1f * a);
		p.puff = 0.95f * a * (1.0f - open);
		p.mouth = lerp(lerp(base.mouth, 0.03f, a), 0.75f, open);
		p.mouth_round = 0.9f * open;
		p.frown = 0.34f * a * (1.0f - 0.6f * open);
		p.brow_raise = -0.005f * a + 0.008f * open;
		p.wide = lerp(1.0f, 0.9f, a) + 0.22f * open;
		p.glow = lerp(0.35f, 1.6f, smoothstep01(remap01(w, 0.3f, 0.85f)));
		p.tremble = cock;
		p.face_y = -0.006f * a;
		p.look = 1.0f;
		p.happy = 0.0f;
		return p;
	}

	// The lob (0.3 s): it rebounds tall, leaning into the throw, mouth an O, eyes wide and blazing,
	// the curl whipping forward; then it rounds out again.
	Pose lob(const AnimInput &in, const Pose &base, float t) const {
		Pose p = base;
		const float u = saturate(t / LOB_TIME);
		const float up = 1.0f - smoothstep01(remap01(u, 0.45f, 1.0f));
		p.sy = lerp(1.0f, 1.3f, up);
		p.shear = Vector3(0.0f, 0.0f, 0.1f * up);
		p.lean = Vector3(0.1f * up, 0.0f, 0.0f);
		p.skirt = 0.95f;
		p.ripple = 0.008f;
		p.ripple_rate = 12.0f;
		p.curl = Vector3(0.5f * up, 0.0f, 0.0f);
		p.puff = 0.0f;
		p.mouth = lerp(0.5f, 1.15f, up);
		p.mouth_round = 0.85f * up;
		p.frown = -0.1f;
		p.brow_raise = 0.008f * up;
		p.wide = 1.0f + 0.25f * up;
		p.glow = lerp(0.6f, 1.6f, up);
		p.face_y = 0.01f * up;
		p.look = 1.0f;
		return p;
	}

	// Recover (the cooldown, about 3 s): pleased with itself it laughs (happy eyes, bouncing), shrunk
	// by the goo it spent, and grows back to full size over the cooldown.
	Pose recover(const AnimInput &in, const Pose &base, float t, float k) const {
		Pose p = base;
		const float in_w = smoothstep01(t / 0.15f);
		const float laugh = 1.0f - smoothstep01(remap01(t, 0.9f, 1.3f));
		const float b = std::fabs(std::sin(PI * t / 0.22f)) * std::exp(-1.6f * t);
		p.size = lerp(1.0f, lerp(0.86f, 1.0f, smoothstep01(remap01(k, 0.1f, 0.95f))), in_w);
		p.sy = base.sy + 0.08f * b * laugh;
		p.shear = base.shear + Vector3(0.025f * std::sin(t * TAU * 2.2f) * laugh * std::exp(-1.2f * t), 0.0f, 0.0f);
		// it slides along (no hops while recovering): a slug-like crawl ripple
		const float crawl = saturate(in.speed / 0.6f);
		p.sy += 0.03f * std::sin(in.t * 9.0f) * crawl;
		p.ripple = base.ripple + 0.004f * crawl;
		p.ripple_rate = lerp(base.ripple_rate, 9.0f, crawl);
		p.happy = laugh > 0.5f && t < 1.1f ? 1.0f : 0.0f;
		p.mouth = lerp(base.mouth, 0.55f + 0.35f * std::fabs(std::sin(t * 12.0f)), laugh);
		p.mouth_round = 0.0f;
		p.frown = lerp(base.frown, -0.15f, laugh);
		p.brow_raise = 0.006f * laugh;
		p.glow = 0.5f;
		p.look = lerp(-1.0f, 0.3f, laugh);
		return p;
	}

	// Hit (visual 0.7 s): splatted flat by the blow and knocked aside, eyes squeezed shut, an 'oof'
	// O mouth, brows up; it wobbles back into shape.
	Pose hit_pose(const AnimInput &in, const Pose &base, float t) const {
		Pose p = base;
		const float snap = smoothstep01(remap01(t, 0.0f, 0.04f)) * (1.0f - smoothstep01(remap01(t, 0.22f, 0.5f)));
		p.sy = lerp(base.sy, 0.8f, snap);
		p.shear = base.shear + in.hit_dir_local * (0.08f * snap);
		p.skirt = 1.0f + 0.12f * snap;
		p.ripple = 0.012f * (snap + 0.3f);
		p.ripple_rate = 14.0f;
		p.curl = Vector3(-0.3f * snap, 0.0f, 0.2f * snap);
		p.squint = snap > 0.3f ? 1.0f : 0.0f;
		p.mouth = lerp(base.mouth, 0.85f, snap);
		p.mouth_round = 0.85f * snap;
		p.puff = 0.0f;
		p.frown = -0.3f * snap;
		p.brow_raise = 0.01f * snap;
		p.glow = 0.2f;
		p.face_y = -0.01f * snap;
		return p;
	}

	// Death (0.6 s): a startled jolt (tall, eyes huge, O mouth), then it splats into a puddle; the eyes
	// stay round and settle onto it (linger_eyes), close, and it shrinks away.
	Pose death_pose(const AnimInput &in, const Pose &base) const {
		Pose p = base;
		const float k = saturate(in.state_t / DEATH_TIME);
		const float jolt = smoothstep01(remap01(k, 0.0f, 0.08f)) * (1.0f - smoothstep01(remap01(k, 0.12f, 0.25f)));
		const float de = anim::ease_in(remap01(k, 0.1f, 0.36f));
		const float sp = anim::ease_out_back(remap01(k, 0.12f, 0.55f), 1.6f);
		p.sy = 1.0f + 0.18f * jolt;
		p.flat = lerp(1.0f, PUDDLE_Y, de);
		p.spread = lerp(1.0f, 1.55f, sp);
		p.shear = Vector3();
		p.lean = Vector3();
		p.skirt = lerp(1.0f, 1.25f, sp);
		p.ripple = 0.022f * (1.0f - smoothstep01(remap01(k, 0.5f, 1.0f))) + 0.004f;
		p.ripple_rate = 14.0f;
		p.curl = Vector3(0.0f, 0.0f, 0.4f * jolt);
		p.mouth = lerp(1.0f, 0.4f, de);
		p.mouth_round = 0.9f;
		p.wide = 1.0f + 0.3f * jolt;
		p.frown = -0.3f;
		p.brow_raise = 0.012f * jolt;
		p.squint = 0.0f;
		p.happy = 0.0f;
		p.puff = 0.0f;
		p.look = 0.0f;
		p.glow = 0.5f;
		p.ground = smoothstep01(remap01(k, 0.05f, 0.35f));
		return p;
	}

	// The eyes during the splat: held round in the model frame while the jelly flattens under them,
	// they sink onto the puddle, blink, and close.
	void linger_eyes(const AnimInput &in, MonsterRig &rig) {
		const float k = saturate(in.state_t / DEATH_TIME);
		if (k < 0.1f || linger_cap_) {
			for (int s = 0; s < 2; ++s) {
				const Transform3D m = rig.model_xf(eye_[s]);
				eye_hold_[s] = Transform3D(m.basis.orthonormalized(), m.origin);
			}
			linger_cap_ = k < 0.1f;
			return;
		}
		const float fall = anim::ease_in(remap01(k, 0.1f, 0.42f));
		const float shut = smoothstep01(remap01(k, 0.62f, 0.72f));
		const float blink = anim::smooth_pulse(in.state_t - 0.27f, 0.035f, 0.02f, 0.05f);
		const Transform3D head_m = rig.model_xf(head_);
		for (int s = 0; s < 2; ++s) {
			Transform3D want = eye_hold_[s];
			want.origin.y = lerp(want.origin.y, 0.06f, fall);
			want.origin.x *= lerp(1.0f, 1.25f, fall); // the puddle spreads them apart a little
			// lying back on the puddle: tip the eye up as it lands
			want.basis = Basis(Vector3(1, 0, 0), -0.7f * fall) * want.basis;
			const float sc = lerp(1.0f, 0.85f, fall);
			want.basis = want.basis.scaled_local(Vector3(sc, sc * lerp(1.0f, 0.08f, Math::max(blink, shut)), sc));
			const Transform3D local = head_m.affine_inverse() * want;
			const Transform3D rest = rig.rest(eye_[s]);
			rig.pose(eye_[s], rest.basis.inverse() * local.basis, local.origin - rest.origin);
		}
	}

	// Spawn (0.6 s): a puddle on the ground, rippling; it gathers itself up with an overshoot (tall,
	// then round), the curl springs up last, and the eyes pop open.
	Pose rise_pose(const AnimInput &in, const Pose &base) const {
		Pose p = base;
		const float k = saturate(in.state_t / RISE_TIME);
		const float u = remap01(k, 0.2f, 0.62f);
		const float grow = u <= 0.0f ? 0.0f : anim::ease_out_back(u, 2.4f);
		p.flat = Math::max(0.05f, lerp(0.13f, 1.0f, grow));
		p.spread = lerp(1.35f, 1.0f, anim::ease_out(u));
		p.sy = 1.0f;
		p.size = 1.0f;
		p.ground = 1.0f - smoothstep01(remap01(k, 0.6f, 0.9f));
		p.shear = Vector3(0.0f, 0.0f, 0.0f);
		p.lean = Vector3();
		p.skirt = lerp(1.08f, 1.0f, anim::ease_out(u));
		p.ripple = k < 0.2f ? 0.02f : lerp(0.02f, 0.004f, saturate(u));
		p.ripple_rate = 10.0f;
		const float flick = anim::smooth_pulse(k - 0.55f, 0.1f, 0.0f, 0.3f);
		p.curl = Vector3(-0.5f * (1.0f - saturate(u)) + 0.4f * flick, 0.0f, 0.3f * flick);
		p.happy = 0.0f;
		p.shut = 1.0f - smoothstep01(remap01(k, 0.72f, 0.86f)); // (final fixes) eased open over about 5 frames, never a 1-frame snap
		p.wide = 1.0f + 0.25f * anim::pulse(k - 0.8f, 0.04f, 0.15f);
		p.mouth = lerp(0.2f, 0.9f, smoothstep01(remap01(k, 0.55f, 0.85f)));
		p.mouth_round = 0.5f * flick;
		p.look = 0.0f;
		p.glow = 0.35f;
		return p;
	}

	int body_ = -1, head_ = -1, skirt_ = -1, curl_ = -1, mouth_ = -1, goo_ = -1;
	int eye_[2] = { -1, -1 }, pupil_[2] = { -1, -1 }, brow_[2] = { -1, -1 }, cheek_[2] = { -1, -1 };
	int squint_[2] = { -1, -1 }, happy_[2] = { -1, -1 };
	int bubble_[NB] = { -1, -1, -1, -1 };
	Vector3 bubble_rest_[NB];
	uint32_t bubble_seed_[NB] = {};
	Transform3D eye_hold_[2];
	MonsterFace face_;
	HitRecoil hit_;
	anim::Weight w_wind_, w_attack_, w_recover_;
	anim::Spring squash_, face_s_, skirt_s_;
	anim::Spring3 shear_, lean_, bubble_s_[NB];
	anim::Dangle curl_d_;
	anim::RandTimer fidget_timer_;
	Vector3 air_hv_;
	float amt_ = 0.0f, ripple_phase_ = 0.0f, ripple_rate_ = 3.0f, splash_ = 0.0f, splash_phase_ = 0.0f, hopping_ = 0.0f;
	float airborne_t_ = 0.0f, fall_ = 0.0f;
	float wind_ = 0.0f, attack_t_ = 0.0f, recover_t_ = 10.0f, recover_k_ = 1.0f, hit_t_ = 10.0f, fidget_t_ = 10.0f;
	float phase0_ = 0.0f, glow_ = -1.0f, glow_sent_ = -1000.0f, look_w_ = 0.0f, idle_for_ = 0.0f;
	bool seeded_ = false, attack_kick_ = false, linger_cap_ = false, was_air_ = false;
};

} // namespace

TOT_MONSTER_ANIMATOR(MonsterType::Slime, SlimeAnimator)

} // namespace tot
