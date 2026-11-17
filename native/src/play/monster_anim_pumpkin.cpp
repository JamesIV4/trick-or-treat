// Pumpkin Hopper (a HOP monster): the animator for its v2 rig.
//
// Part tree (art/team/monsters/batch04/build_monsters2.py, pumpkin()):
//   body (meshless joint on the ground) > head (the lobed shell with the carved face; pivot at its bottom)
//        > mouth (the grin's lower-lip band: the head is carved with the ROAR, hiding this drops the jaw),
//          mouth_glow (the light plate behind the mouth), stem (soft: one curled tube), leaf_a .. leaf_d
//          (soft collar leaves pinned at the stem), tendril_l / tendril_r (soft curls), eye_l / eye_r
//          (the light plate behind each carved triangle) > pupil_l / pupil_r (a small white-hot flame),
//          brow_l / brow_r (shell wedges hidden in the flesh above the eyes; pivot 0.42 m behind the face,
//          a turn about local X slides them down into the holes: anger), arm_l / arm_r (vine, ball at the
//          shoulder) > hand_l / hand_r (three leaves)
//   body > foot_l / foot_r (root feet under the shell)
// The shell carries the squash and stretch (about its bottom, so the feet keep their shape), the rock
// and the look. A CARVED JACK-O'-LANTERN (polish round): no pupils and no lids. The light is graded from
// the outside in: the head's carved walls (rig.glow(head)), the plates (eyes, mouth_glow) and the flames,
// each driven here. Life without pupils: the flames drift toward the player and flicker; a blink, a hit
// and death shrink the plates and snuff the flames; anger slants the eyes with the brows; the wind-up
// and the slam open the mouth wide.
//
// A frame: a locomotion pose (idle breath, the hop cycle read from on_floor / vel / hop_in), state
// layers mixed over it by weight (wind-up, slam, dizzy recover, hit), springs (squash, rock, arms, the
// stem / leaf / tendril Dangles), one solve. Rise pops the stem out of the ground and inflates the
// shell; death caves it in and snuffs the candle.

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

constexpr int L = 0, R = 1; // left (+X) and right (-X)
constexpr float SIDE[2] = { 1.0f, -1.0f };
constexpr int NL = 4; // collar leaves

// ---- tuning --------------------------------------------------------------------------------------
constexpr float HOP_SPEED = 4.5f; // horizontal speed of a chase hop (tuning.speed * 1.4)
constexpr float RISE_TIME = 0.6f, DEATH_TIME = 0.6f;
constexpr float HIT_TIME = 0.62f; // the visual hit reaction; Hurt itself is 0.28 s
constexpr float DIZZY_TIME = 1.5f; // the dizzy re-inflate after a slam (Recover lasts 1.6 .. 2.4 s)
constexpr float FIDGET_TIME = 1.1f;
// Candle light at rest: eye_glow drives the eye plates, the mouth plate and the flames (each scaled
// below); face_glow drives the head's carved walls.
constexpr float EYE_GLOW = 1.0f, FACE_GLOW = 1.0f;
constexpr float PLATE_K = 0.9f, MOUTH_K = 1.0f, FLAME_K = 1.7f; // per-part gain on eye_glow
constexpr float BROW_DROP = 0.11f; // radians about the brow's pivot (0.42 m behind the face): about 4.6 cm
constexpr float FLAME_TRAVEL = 0.12f; // of the eye plate's reach
// The slam squash: a soft floor instead of a hard clamp, so the flattest frames pass through and
// rebound instead of sitting as a pancake.
constexpr float SQUASH_KNEE = 0.8f, SQUASH_SOFT = 0.24f; // below the knee the scale eases toward knee - soft
// The leap held in place (the game as it is: the leap velocity is lost, see CHECKPOINT "Real-play review"):
// Attack lasts about 0.155 s on the ground and the game's landing puff and hit come as it ends, so the
// pumpkin pops up in a short hop timed to come down on that frame (the slam).
constexpr float HELD_HOP_TIME = 0.155f, HELD_HOP_H = 0.13f, LEAP_VY = 7.2f;

struct Pose {
	float sy = 1.0f; // volume preserving squash / stretch of the shell (springy)
	float flat = 1.0f; // extra Y scale (cave-in), not springy
	float spread = 1.0f; // extra X / Z scale (cave-in), not springy
	float size = 1.0f; // uniform scale (rise inflate)
	Vector3 rock; // the shell's rotation vector about its bottom (x > 0 tips the top forward, z rolls), springy
	Vector3 lean; // root lean (rotation vector, facing frame), springy
	float lift = 0.0f; // root lift (m): fidget bounce
	float sink = 0.0f; // root drop (m): buried before the rise
	float ground = 0.0f; // 1 = put the picture on the ground (spawn drop, a death in the air)
	float raise = 0.0f, fwd = 0.0f, twist = 0.0f; // both arms: up, forward, palm turned down
	float raise_d = 0.0f; // asymmetric raise (+ = left up, right down)
	float hand = 0.0f; // hands curled forward
	float tuck = 0.0f; // feet: toes down and drawn up (in the air)
	float splay = 0.0f; // feet turned out (landing, cave-in)
	float leaf = 0.0f; // collar leaves lifted (+) / flattened (-), radians
	Vector3 stem; // authored stem bend (rotation vector, head space)
	float eye_glow = EYE_GLOW, face_glow = FACE_GLOW;
	float shut = 0.0f, wide = 1.0f, squint = 0.0f, pupil = 1.0f; // pupil = flame size
	float anger = 0.0f; // brows down into the eyes
	float roar = 0.0f; // > 0.5: the mouth wide open (lower lip hidden)
	float look = -1.0f;
	float tremble = 0.0f;
	float dizzy = 0.0f;
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
	d.rock = d.rock.lerp(s.rock, w);
	d.lean = d.lean.lerp(s.lean, w);
	d.lift = lerp(d.lift, s.lift, w);
	d.sink = lerp(d.sink, s.sink, w);
	d.ground = lerp(d.ground, s.ground, w);
	d.raise = lerp(d.raise, s.raise, w);
	d.fwd = lerp(d.fwd, s.fwd, w);
	d.twist = lerp(d.twist, s.twist, w);
	d.raise_d = lerp(d.raise_d, s.raise_d, w);
	d.hand = lerp(d.hand, s.hand, w);
	d.tuck = lerp(d.tuck, s.tuck, w);
	d.splay = lerp(d.splay, s.splay, w);
	d.leaf = lerp(d.leaf, s.leaf, w);
	d.stem = d.stem.lerp(s.stem, w);
	d.eye_glow = lerp(d.eye_glow, s.eye_glow, w);
	d.face_glow = lerp(d.face_glow, s.face_glow, w);
	d.shut = lerp(d.shut, s.shut, w);
	d.wide = lerp(d.wide, s.wide, w);
	d.squint = lerp(d.squint, s.squint, w);
	d.pupil = lerp(d.pupil, s.pupil, w);
	d.anger = lerp(d.anger, s.anger, w);
	d.roar = lerp(d.roar, s.roar, w);
	d.look = lerp(d.look, s.look, w);
	d.tremble = lerp(d.tremble, s.tremble, w);
	d.dizzy = lerp(d.dizzy, s.dizzy, w);
}

Basis rotvec_basis(const Vector3 &rv) {
	const float a = rv.length();
	return a > 1e-6f ? Basis(rv / a, a) : Basis();
}

class PumpkinAnimator : public MonsterAnimator {
public:
	float rise_time() const override { return RISE_TIME; }
	float death_time() const override { return DEATH_TIME; }
	float telegraph_gain() const override { return 1.3f; } // (final fixes 2) linear body_flash: at least ~0.9 of BEFORE's pulse (flash_metric.py)
	float hit_flash_gain() const override { return 1.3f; } // (final fixes 2) the fade after the white peak (linear body_flash)

	void bind(MonsterRig &rig) override {
		body_ = rig.slot("body");
		head_ = rig.slot("head");
		stem_ = rig.slot("stem");
		static const char *leaves[NL] = { "leaf_a", "leaf_b", "leaf_c", "leaf_d" };
		for (int i = 0; i < NL; ++i) {
			leaf_[i] = rig.slot(leaves[i]);
		}
		static const char *names[2][6] = {
			{ "eye_l", "pupil_l", "arm_l", "hand_l", "foot_l", "tendril_l" },
			{ "eye_r", "pupil_r", "arm_r", "hand_r", "foot_r", "tendril_r" },
		};
		for (int s = 0; s < 2; ++s) {
			eye_[s] = rig.slot(names[s][0]);
			pupil_[s] = rig.slot(names[s][1]);
			arm_[s] = rig.slot(names[s][2]);
			hand_[s] = rig.slot(names[s][3]);
			foot_[s] = rig.slot(names[s][4]);
			tendril_[s] = rig.slot(names[s][5]);
		}
		mouth_ = rig.slot("mouth");
		mouth_glow_ = rig.slot("mouth_glow");
		brow_[L] = rig.slot("brow_l");
		brow_[R] = rig.slot("brow_r");
		face_.bind(rig);
		face_.max_yaw = 0.45f; // the whole pumpkin turns: a little goes a long way
		face_.max_pitch = 0.14f;
		face_.look_height = 0.4f;
		face_.glow_rest = -1.0f; // the candle light is driven here
		// A toy pumpkin: a low sheen on the big shell (a glossy shell clips under the key light), matte
		// leaves and vine, a warm rim (the candle light leaking round the edge); the lights are matte (a
		// light, not a lens) and cast no shadow (inside the carved pockets they would only darken it).
		for (int i = 0; i < rig.count(); ++i) {
			rig.gloss(i, 0.08f);
			rig.param(i, "rim_strength", 0.0f);
		}
		for (int part : { head_, mouth_, brow_[L], brow_[R] }) {
			rig.gloss(part, 0.12f);
			rig.param(part, "rim_strength", 0.1f);
			rig.param(part, "rim_color", Color(1.0f, 0.55f, 0.18f));
		}
		rig.gloss(mouth_glow_, -2.0f);
		rig.cast_shadow(mouth_glow_, false);
		for (int s = 0; s < 2; ++s) {
			rig.gloss(eye_[s], -2.0f);
			rig.gloss(pupil_[s], -2.0f);
			rig.cast_shadow(eye_[s], false);
			rig.cast_shadow(brow_[s], false);
			rig.soft(tendril_[s], Vector3(), -1.0f, 0.015f);
		}
		rig.soft(stem_, Vector3(0, 1, 0), -1.0f, 0.03f);
		for (int i = 0; i < NL; ++i) {
			rig.soft(leaf_[i], Vector3(), -1.0f, 0.02f);
			Vector3 d = rig.tip(leaf_[i]);
			d.y = 0.0f;
			leaf_dir_[i] = d.length() > 1e-4f ? d.normalized() : Vector3(0, 0, 1);
		}
	}

	void update(const AnimInput &in, MonsterRig &rig, Transform3D &root) override {
		using State = MonsterAnimState;
		const float dt = anim::safe_dt(in.dt);
		const bool far = in.lod > 0;
		if (!seeded_) {
			seeded_ = true;
			fidget_timer_ = anim::RandTimer(in.seed ^ 0x9a3u);
			phase0_ = anim::hash01(in.seed) * TAU;
		}
		const bool dying = in.state == State::Dying, rising = in.state == State::Rise;
		const bool alive = !dying && !rising;
		if (rising && in.entered) {
			squash_.reset();
			rock_.reset();
			lean_.reset();
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
		air_w_ = far ? (air ? 1.0f : 0.0f) : anim::damp(air_w_, air ? 1.0f : 0.0f, 0.035f, dt);
		const Vector3 hv(in.vel_local.x, 0.0f, in.vel_local.z);
		if (air) {
			air_hv_ = hv;
		}
		// the slam: a landing that ends the leap (Attack ends on the tick it lands)
		const bool slam_land = landed && (in.state == State::Attack || (in.state == State::Recover && in.state_t < 0.1f));
		if (slam_land) {
			slam_t_ = 0.0f;
		}
		slam_t_ += dt;

		// ---- clocks and one-shots
		const Vector3 recoil = hit_.update(in);
		if (hit_.fresh && alive) {
			hit_t_ = 0.0f;
		}
		hit_t_ += dt;
		const float hspeed = alive ? in.speed : 0.0f;
		gait_.step(in, 0.34f, 1.0f);
		const bool idle = in.state == State::Roam && !in.chasing && in.speed < 0.2f && !air && in.hop_in <= 0.0f;
		idle_for_ = idle ? idle_for_ + dt : 0.0f;
		fidget_t_ += dt;
		if (!idle) {
			fidget_timer_.reset();
		} else if (idle_for_ > 5.0f && fidget_t_ > FIDGET_TIME + 1.0f && fidget_timer_.step(dt, 2.6f, 4.6f)) {
			fidget_t_ = 0.0f;
			fidget_n_++;
		}

		// ---- the pose: locomotion, then the state layers
		Pose p = locomotion(in, air, hspeed);
		if (idle && fidget_t_ < FIDGET_TIME) {
			mix(p, fidget(in, p, fidget_t_), anim::smooth_pulse(fidget_t_, 0.1f, FIDGET_TIME - 0.35f, 0.25f));
		}
		if (in.state == State::Recover) {
			recover_t_ = in.state_t;
		} else if (in.state != State::Attack) {
			recover_t_ = 10.0f;
		}
		w_wind_.step(in.state == State::Windup, 0.06f, 0.05f, dt);
		w_attack_.step(in.state == State::Attack, 0.04f, 0.08f, dt);
		w_dizzy_.step(in.state == State::Recover && recover_t_ < DIZZY_TIME, 0.05f, 0.45f, dt);
		if (in.state == State::Windup) {
			wind_ = in.windup;
		}
		if (w_wind_.w > 0.0f) {
			// A wind-up can start in the middle of a roaming hop: the body squash, rock and arm draw wait for
			// the ground (the face and the light do not); otherwise it flattens into a disc in mid air.
			Pose w = windup(in, p, wind_);
			keep_air_body(w, p, air_w_);
			mix(p, w, w_wind_.w);
		}
		if (in.state == State::Attack) {
			attack_t_ = in.state_t;
			if (in.entered) {
				leap_kick_ = true;
				held_ = true;
			}
			if (!in.on_floor || std::fabs(in.vel_local.y) > 0.5f) {
				held_ = false; // it really flies (in play the leap would; held in place it does not)
			}
		}
		// held in place: a short hop up and down onto the game's landing frame, the pose following its fake
		// vertical speed (up, over the top, down)
		const float hop_u = saturate(attack_t_ / HELD_HOP_TIME);
		if (alive && held_ && in.state == State::Attack) {
			hop_h_ = HELD_HOP_H * anim::arc(hop_u);
		} else {
			hop_h_ = Math::max(0.0f, hop_h_ - dt * 2.0f); // knocked out of the hop: come down, never snap
		}
		const float leap_vy = held_ ? LEAP_VY * (1.0f - 2.0f * hop_u) : in.vel_local.y;
		if (w_attack_.w > 0.0f) {
			mix(p, leap(in, p, attack_t_, leap_vy), w_attack_.w);
		}
		if (w_dizzy_.w > 0.0f) {
			mix(p, dizzy(in, p, Math::min(recover_t_, DIZZY_TIME)), w_dizzy_.w);
		}
		const float hit_w = alive ? smoothstep01(hit_t_ / 0.04f) * (1.0f - smoothstep01(remap01(hit_t_, HIT_TIME - 0.25f, HIT_TIME))) : 0.0f;
		if (hit_w > 0.0f) {
			mix(p, hit_pose(in, p, hit_t_), hit_w);
		}
		if (dying) {
			mix(p, death_pose(in, p), smoothstep01(in.state_t / 0.04f));
		}
		if (rising) {
			mix(p, rise_pose(in, p), 1.0f);
		}

		// ---- springs: squash (a ringing pumpkin), rock, lean, arms
		const Basis facing(Vector3(0, 1, 0), in.yaw);
		if (far) {
			squash_.reset(p.sy - 1.0f);
			rock_.reset(p.rock);
			lean_.reset(p.lean);
			for (int s = 0; s < 2; ++s) {
				arm_s_[s].reset(Vector3(p.raise + SIDE[s] * p.raise_d, p.fwd, p.twist));
			}
		} else {
			if (launched) {
				squash_.kick(6.5f);
				rock_.kick(Vector3(-1.4f, 0.0f, 0.0f) * saturate(hv.length() / HOP_SPEED)); // the top stays behind
				for (int s = 0; s < 2; ++s) {
					arm_s_[s].kick(Vector3(-7.0f, 0.0f, 0.0f)); // the arms are left behind, then fly up
				}
			}
			// the slam: Recover starts on the tick the leap lands (or, held in place, after 0.15 s)
			if (in.state == State::Recover && in.entered && in.prev_state == State::Attack && !landed) {
				squash_.kick(-12.0f); // into a stiffer spring (below): a short sharp squash, then an elastic rebound
				for (int s = 0; s < 2; ++s) {
					arm_s_[s].kick(Vector3(-9.0f, 0.0f, 0.0f));
				}
			}
			if (landed) {
				const float k = slam_land ? 2.0f : 1.0f;
				squash_.kick(-Math::min(15.0f, 1.6f * land_speed * k));
				rock_.kick(Vector3(saturate(air_hv_.length() / HOP_SPEED) * 2.4f, 0.0f, 0.0f)); // the top carries on
				for (int s = 0; s < 2; ++s) {
					arm_s_[s].kick(Vector3(-1.6f * land_speed * k, 0.0f, 0.0f)); // slap down
				}
			}
			if (hit_.fresh && alive) {
				squash_.kick(-7.0f);
				for (int s = 0; s < 2; ++s) {
					arm_s_[s].kick(Vector3(9.0f, -2.0f, 0.0f));
				}
			}
			if (leap_kick_) {
				squash_.kick(8.0f);
			}
			if (alive) {
				// starts, stops and turns: the top lags
				rock_.kick(Vector3(-in.accel_local.z, 0.0f, in.accel_local.x) * (0.03f * dt));
			}
			// the slam rings on a stiffer spring for its first 0.45 s, so the flattest frames pass in two or three
			// frames and it bounces back up instead of sitting flat
			const bool slam_ring = in.state == State::Recover && recover_t_ < 0.45f;
			squash_.step(p.sy - 1.0f, slam_ring ? 6.0f : 4.6f, slam_ring ? 0.2f : 0.24f, dt);
			rock_.step(p.rock, 3.6f, 0.3f, dt);
			lean_.step(p.lean, 3.0f, 0.45f, dt);
			for (int s = 0; s < 2; ++s) {
				const Vector3 want(p.raise + SIDE[s] * p.raise_d, p.fwd, p.twist);
				arm_s_[s].step(want, 3.0f, 0.3f, dt);
			}
		}
		// a soft floor: below the knee the squash eases in (never a hard flat clamp), so the flattest
		// frames pass through and the spring carries it straight into the rebound
		float sy = 1.0f + squash_.x;
		if (sy < SQUASH_KNEE) {
			sy = SQUASH_KNEE - SQUASH_SOFT * std::tanh((SQUASH_KNEE - sy) / SQUASH_SOFT);
		}
		sy = Math::min(sy, 1.45f);

		// ---- solve: body, feet, the shell (squash, rock, look), face, arms, soft parts
		rig.pose(body_, Basis());
		// feet: tucked in the air, splayed on a landing or a cave-in, shuffling when it slides along
		const float shuffle = (alive && !air && in.state != State::Roam) ? saturate(hspeed / 1.0f) : 0.0f;
		for (int s = 0; s < 2; ++s) {
			const float ph = gait_.phase * TAU + (s == L ? 0.0f : PI);
			const float step_up = 0.025f * shuffle * Math::max(0.0f, std::sin(ph));
			const float step_sw = 0.25f * shuffle * std::cos(ph);
			const Basis fb = Basis(Vector3(0, 1, 0), SIDE[s] * 0.35f * p.splay) * Basis(Vector3(1, 0, 0), 0.55f * p.tuck - step_sw);
			rig.pose(foot_[s], fb, Vector3(SIDE[s] * 0.03f * p.splay, step_up + 0.03f * p.tuck, 0.0f));
		}
		const float lw = p.look < 0.0f ? (in.chasing ? 1.0f : 0.0f) : saturate(p.look);
		look_w_ = far ? lw : anim::damp(look_w_, lw, 0.12f, dt);
		face_.look_weight = look_w_;
		const bool intent = in.state == State::Windup || in.state == State::Attack;
		face_.head_lag = anim::damp(face_.head_lag, intent ? 0.04f : 0.16f, 0.08f, dt);
		face_.wide = p.wide;
		face_.shut = p.shut;
		face_.update(in, rig);
		Vector3 hscale = anim::squash_stretch(sy) * p.size;
		hscale.y *= p.flat;
		hscale.x *= p.spread;
		hscale.z *= p.spread;
		const float trem = p.tremble * std::sin(in.t * 46.0f);
		hscale.x *= 1.0f + 0.02f * trem;
		Vector3 rock = rock_.x;
		const float rl = rock.length();
		if (rl > 0.6f) {
			rock *= 0.6f / rl;
		}
		const Basis look = Basis::from_euler(anim::look_euler(face_.head));
		// a rocking ball rolls on its round bottom: lift it as it tips so the edge never sinks in
		const Vector3 hoff(0.0f, 0.22f * std::fabs(rock.z) + 0.12f * std::fabs(rock.x), 0.0f);
		rig.pose(head_, rotvec_basis(rock) * look, hoff, hscale);

		// ---- the carved face: plates, flames, brows, the roar and the light
		// A blink, a hit and death (face_.closed, from MonsterFace) shrink each eye's light plate toward its
		// centre and dim it, and snuff the flame: the light in the eye dips, nothing shutters across it.
		const float closed = saturate(face_.closed);
		const float flare = saturate((p.eye_glow - EYE_GLOW) / 0.8f); // the wind-up / leap / death-start flare
		for (int s = 0; s < 2; ++s) {
			const float ps = lerp(1.0f, 0.42f, closed);
			rig.pose(eye_[s], Basis(), Vector3(), Vector3(ps, ps * (1.0f - 0.22f * p.squint), 1.0f));
		}
		// flames: drift toward the player (damped), flicker with the candle, swim when dizzy, knocked by a hit
		{
			const bool look_on = alive && !far;
			const Vector3 target = in.player_local + Vector3(0.0f, face_.look_height, 0.0f);
			const float ouch = alive ? hit_.ouch : 0.0f;
			const float k = far ? 1.0f : anim::damp_factor(0.07f, dt);
			for (int s = 0; s < 2; ++s) {
				if (eye_[s] < 0 || pupil_[s] < 0) {
					continue;
				}
				Vector2 want;
				if (look_on && look_w_ > 0.0f) {
					want = anim::pupil_offset(rig.model_rest(eye_[s]), target, 1.0f) * look_w_;
				}
				want += Vector2(0.4f * SIDE[s], -0.5f) * ouch; // knocked askew and down
				if (p.dizzy > 0.0f) {
					const float a = in.t * 9.0f + (s == L ? 0.0f : PI * 0.6f);
					want += Vector2(std::cos(a), std::sin(a)) * (0.9f * p.dizzy);
				}
				flame_look_[s] += (want - flame_look_[s]) * k;
				const float travel = FLAME_TRAVEL * rig.reach(eye_[s]);
				const float fl1 = far ? 0.0f : anim::noise1(in.t * 9.0f + float(s) * 3.1f, in.seed + 41u + uint32_t(s));
				const float fl2 = far ? 0.0f : anim::noise1(in.t * 17.0f + float(s) * 1.7f, in.seed + 57u + uint32_t(s));
				const float sz = Math::max(0.001f, (1.0f - closed) * p.pupil * (1.0f + 0.1f * fl1) * (1.0f - 0.45f * ouch));
				const float tall = 1.0f + 0.14f * fl2 + 0.25f * flare;
				const Basis fb = Basis(Vector3(0, 0, 1), 0.08f * fl2) * Basis().scaled(Vector3(sz, sz * tall, 1.0f));
				rig.pose(pupil_[s], fb, Vector3(flame_look_[s].x * travel, flame_look_[s].y * travel, 0.0f));
			}
		}
		// brows: shell wedges that slide down into the top of each hole (anger), on a quick spring
		anger_ = far ? p.anger : anim::damp(anger_, p.anger, 0.05f, dt);
		for (int s = 0; s < 2; ++s) {
			rig.pose(brow_[s], Basis(Vector3(1, 0, 0), BROW_DROP * saturate(anger_)));
		}
		// the roar: the lower lip is hidden (the head is carved with the open mouth); each switch is covered
		// by the candle guttering for a moment
		const bool roar = roar_on_ ? p.roar > 0.35f : p.roar > 0.65f;
		if (roar != roar_on_) {
			roar_on_ = roar;
			gutter_ = 1.0f;
			rig.show(mouth_, !roar);
		}
		gutter_ = Math::max(0.0f, gutter_ - dt / 0.09f);
		// the candle: a slow waver on top of the shader's own flicker; hit dims it, death snuffs it
		const float waver = far ? 1.0f : 1.0f + 0.1f * anim::noise1(in.t * 2.3f, in.seed + 7u) + 0.05f * anim::noise1(in.t * 7.0f, in.seed + 9u);
		eye_glow_ = eye_glow_ < 0.0f || far ? p.eye_glow : anim::damp(eye_glow_, p.eye_glow, 0.03f, dt);
		face_glow_ = face_glow_ < 0.0f || far ? p.face_glow : anim::damp(face_glow_, p.face_glow, 0.03f, dt);
		const float gut = 1.0f - 0.45f * gutter_;
		const float eg = Math::max(0.0f, eye_glow_ * waver * gut), fg = Math::max(0.0f, face_glow_ * waver * gut);
		const float glows[4] = { eg * PLATE_K * lerp(1.0f, 0.3f, closed), eg * MOUTH_K, eg * FLAME_K * (1.0f + 0.5f * flare), fg };
		const int parts[4][2] = { { eye_[L], eye_[R] }, { mouth_glow_, -1 }, { pupil_[L], pupil_[R] }, { head_, -1 } };
		for (int i = 0; i < 4; ++i) {
			if (std::fabs(glows[i] - glow_sent_[i]) > 0.01f) {
				glow_sent_[i] = glows[i];
				for (int part : parts[i]) {
					rig.glow(part, glows[i]);
				}
			}
		}

		// arms: FK about the shoulder (raise, then turn forward), counter-scaled against the shell's squash
		// so the vines keep their length; the hands flop behind the arm's motion (overlap)
		const Vector3 inv(1.0f / hscale.x, 1.0f / hscale.y, 1.0f / hscale.z);
		for (int s = 0; s < 2; ++s) {
			const Vector3 a = arm_s_[s].x;
			const float sway = alive ? 0.06f * std::sin(in.t * 1.7f + phase0_ + float(s) * 1.9f) : 0.0f;
			const Basis ab = Basis(Vector3(0, 1, 0), -SIDE[s] * a.y) * Basis(Vector3(0, 0, 1), SIDE[s] * (Math::clamp(a.x, -0.9f, 1.6f) + sway)) *
					Basis(Vector3(1, 0, 0), a.z);
			rig.pose(arm_[s], Basis::from_scale(inv) * ab); // head * this = ab: the arm undistorted (rest basis is identity)
			const float flop = Math::clamp(-0.09f * arm_s_[s].v.x, -0.7f, 0.7f);
			const float flutter = air && alive ? 0.12f * std::sin(in.t * 23.0f + float(s) * 2.0f) : 0.0f;
			rig.pose(hand_[s], Basis(Vector3(0, 0, 1), SIDE[s] * (flop + flutter)) * Basis(Vector3(0, 1, 0), -SIDE[s] * p.hand));
		}

		// soft parts: the stem, the collar leaves and the tendrils, each thrown about by the shell's motion
		if (!far && alive) {
			if (hit_.fresh) {
				const Vector3 k = facing.xform(in.hit_dir_local) * 1.6f + Vector3(0, 0.6f, 0);
				stem_d_.kick(k);
				for (int i = 0; i < NL; ++i) {
					leaf_d_[i].kick(k + Vector3(0, 1.2f, 0));
				}
				for (int s = 0; s < 2; ++s) {
					ten_d_[s].kick(k);
				}
			}
			if (landed) {
				stem_d_.kick(Vector3(0.0f, -0.35f * land_speed, 0.0f));
				for (int i = 0; i < NL; ++i) {
					leaf_d_[i].kick(Vector3(0.0f, -0.25f * land_speed, 0.0f));
				}
				for (int s = 0; s < 2; ++s) {
					ten_d_[s].kick(Vector3(0.0f, -0.4f * land_speed, 0.0f));
				}
			}
			if (launched || leap_kick_) {
				for (int i = 0; i < NL; ++i) {
					leaf_d_[i].kick(Vector3(0.0f, -0.8f, 0.0f));
				}
			}
		}
		const Vector3 up(0, 1, 0);
		auto frame_of = [&](int slot) {
			const Transform3D m = rig.model_rest(slot);
			return Transform3D(facing * m.basis.orthonormalized(), in.origin_world + facing.xform(m.origin));
		};
		// stem
		{
			Vector3 b = p.stem;
			if (!far && alive && stem_ >= 0) {
				anim::DangleParams sp;
				sp.len = Math::max(0.08f, rig.reach(stem_));
				sp.hz = 3.2f;
				sp.zeta = 0.22f;
				sp.inertia = 1.0f;
				sp.limit = 0.6f;
				b += stem_d_.step(frame_of(stem_), up, sp, dt);
			} else {
				stem_d_.reset();
			}
			rig.bend(stem_, b);
		}
		// collar leaves: lifted / flattened by the pose, fluttering in the air; never pushed into the shell
		const float flutter_amt = alive ? saturate(airborne_t_ / 0.1f) : 0.0f;
		for (int i = 0; i < NL; ++i) {
			const Vector3 axis = leaf_dir_[i].cross(up).normalized(); // + lifts the tip
			float lift = p.leaf + flutter_amt * (0.1f * std::sin(in.t * 21.0f + float(i) * 1.7f) + Math::clamp(-0.05f * in.vel_local.y, -0.12f, 0.25f));
			lift += alive ? 0.035f * anim::noise1(in.t * 0.9f + float(i), in.seed + 31u * uint32_t(i)) : 0.0f;
			Vector3 b = axis * lift;
			if (!far && alive && leaf_[i] >= 0) {
				anim::DangleParams lp;
				lp.len = Math::max(0.08f, rig.reach(leaf_[i]));
				lp.hz = 2.6f;
				lp.zeta = 0.22f;
				lp.inertia = 0.9f;
				lp.limit = 0.45f;
				b += leaf_d_[i].step(frame_of(leaf_[i]), leaf_dir_[i], lp, dt);
			} else {
				leaf_d_[i].reset();
			}
			const float down = b.dot(axis);
			if (down < -0.06f) {
				b += axis * (-0.06f - down);
			}
			rig.bend(leaf_[i], b);
		}
		// tendrils: springy curls
		for (int s = 0; s < 2; ++s) {
			Vector3 b;
			if (!far && alive && tendril_[s] >= 0) {
				anim::DangleParams tp;
				tp.len = Math::max(0.06f, rig.reach(tendril_[s]));
				tp.hz = 2.4f;
				tp.zeta = 0.16f;
				tp.inertia = 1.2f;
				tp.limit = 0.7f;
				Vector3 rd = rig.tip(tendril_[s]);
				if (rd.length() < 1e-4f) {
					rd = up;
				}
				b = ten_d_[s].step(frame_of(tendril_[s]), rd, tp, dt);
			} else {
				ten_d_[s].reset();
			}
			if (dying) {
				b += Vector3(0.0f, 0.0f, -SIDE[s] * 0.7f * smoothstep01(in.state_t / 0.35f)); // they droop
			}
			rig.bend(tendril_[s], b);
		}
		leap_kick_ = false;

		// ---- the root: facing, lean (springy), the flinch, lift, buried / dropped to the ground
		Vector3 lean = lean_.x + recoil * 0.55f;
		Vector3 offset(0.0f, p.lift - p.sink + hop_h_, 0.0f);
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
			// (final fixes 2) over about 8 frames (0.86 .. 1 was 3 visible ones), gone just before the state ends
			rs = Math::max(0.001f, 1.0f - smoothstep01(remap01(in.state_t / DEATH_TIME, 0.74f, 0.97f)));
		}
		root = root_pose(in.yaw, lean, Vector3(rs, rs, rs), offset);
	}

private:
	// Sitting: a slow candle-lit breath, arms drifting. Hopping: a crouch as the next hop comes, a
	// stretch on the way up, round at the top, stretched falling (the landing squash comes from the
	// spring), arms flung up behind the jump, feet tucked, leaves fluttering.
	Pose locomotion(const AnimInput &in, bool air, float hspeed) const {
		Pose p;
		const float breath = std::sin(in.t * 2.0f + phase0_);
		p.sy = 1.0f + 0.018f * breath;
		p.rock = Vector3(0.0f, 0.0f, 0.02f * anim::noise1(in.t * 0.35f, in.seed + 3u));
		p.raise = -0.18f + 0.05f * breath;
		p.fwd = 0.1f;
		p.hand = 0.1f;
		p.leaf = 0.02f * breath;
		p.look = -1.0f;
		const float chase = in.chasing ? 1.0f : 0.0f;
		p.face_glow = FACE_GLOW + 0.15f * chase;
		p.eye_glow = EYE_GLOW + 0.05f * chase;
		if (air) {
			const float vy = in.vel_local.y;
			p.sy = vy > 0.0f ? 1.0f + Math::min(0.16f, 0.035f * vy) : 1.0f + Math::min(0.08f, -0.02f * vy);
			const float fwd = saturate(Vector2(in.vel_local.x, in.vel_local.z).length() / HOP_SPEED);
			p.lean = Vector3(0.12f * fwd, 0.0f, 0.0f);
			p.rock = Vector3(vy > 0.0f ? -0.06f * fwd : 0.08f * fwd, 0.0f, 0.0f);
			p.raise = vy > 0.0f ? 0.75f : lerp(0.75f, 0.25f, saturate(-vy / 4.0f));
			p.fwd = -0.15f;
			p.hand = -0.2f;
			p.tuck = 1.0f;
			p.wide = 1.06f;
			p.stem = Vector3(-0.15f * fwd, 0.0f, 0.0f);
		} else if (in.hop_in > 0.0f && in.hop_in < 0.17f && in.state == MonsterAnimState::Roam) {
			// the crouch before the next hop: down, arms back
			const float c = smoothstep01(remap01(in.hop_in, 0.17f, 0.03f));
			p.sy = lerp(p.sy, 0.84f, c);
			p.raise = lerp(p.raise, -0.3f, c);
			p.fwd = lerp(p.fwd, -0.3f, c);
			p.splay = 0.4f * c;
			p.leaf = lerp(p.leaf, 0.08f, c);
		} else if (in.on_floor && in.hop_in <= 0.0f && hspeed > 0.2f) {
			// sliding along (Recover, a knock): a little waddle
			p.rock.z += 0.05f * std::sin(gait_.phase * TAU);
		}
		return p;
	}

	// Idle fidget (after 5 s sitting): a happy bounce on the spot, facing whoever is looking, a rock to
	// one side and back, the eyes crinkled, the leaves flipping up.
	Pose fidget(const AnimInput &in, const Pose &base, float t) const {
		Pose p = base;
		const float side = (fidget_n_ & 1) ? 1.0f : -1.0f;
		const float dip = anim::pulse(t - 0.02f, 0.12f, 0.12f);
		const float b1 = anim::arc(remap01(t, 0.18f, 0.48f)), b2 = anim::arc(remap01(t, 0.52f, 0.76f));
		p.sy = base.sy - 0.12f * dip + 0.07f * (b1 + 0.6f * b2);
		p.lift = 0.06f * b1 + 0.03f * b2;
		p.rock = base.rock + Vector3(0.0f, 0.0f, side * 0.12f * std::sin(PI * saturate(t / 0.9f)));
		p.raise = base.raise + 0.5f * (b1 + b2) * 0.8f;
		p.raise_d = side * 0.25f * b1;
		p.tuck = 0.6f * Math::max(b1, b2);
		p.leaf = 0.12f * (b1 + b2);
		p.squint = 0.6f * smoothstep01(remap01(t, 0.1f, 0.25f)) * (1.0f - smoothstep01(remap01(t, 0.85f, 1.0f)));
		p.face_glow = base.face_glow + 0.25f * (b1 + b2);
		p.look = 0.0f;
		return p;
	}

	// While in the air (weight a), a state layer keeps the air pose's body (squash, rock, lean, arms, feet).
	static void keep_air_body(Pose &d, const Pose &air_pose, float a) {
		if (a <= 0.0f) {
			return;
		}
		d.sy = lerp(d.sy, air_pose.sy, a);
		d.rock = d.rock.lerp(air_pose.rock, a);
		d.lean = d.lean.lerp(air_pose.lean, a);
		d.raise = lerp(d.raise, air_pose.raise, a);
		d.fwd = lerp(d.fwd, air_pose.fwd, a);
		d.twist = lerp(d.twist, air_pose.twist, a);
		d.splay = lerp(d.splay, air_pose.splay, a);
		d.tuck = lerp(d.tuck, air_pose.tuck, a);
		d.tremble = lerp(d.tremble, air_pose.tremble, a);
	}

	// The wind-up (0.45 s): a deep squash, rocking side to side, the face flaring hot, the eyes
	// narrowed on the player, arms drawn back and up, leaves lifting, the stem coiling back; at the end
	// it sinks deepest and tips back, cocked.
	Pose windup(const AnimInput &in, const Pose &base, float w) const {
		Pose p = base;
		const float a = smoothstep01(remap01(w, 0.0f, 0.4f));
		const float cock = smoothstep01(remap01(w, 0.75f, 1.0f));
		p.sy = lerp(1.0f, 0.74f, a) - 0.05f * cock;
		const float rk = std::sin(remap01(w, 0.05f, 0.8f) * TAU * 2.0f) * (1.0f - cock);
		p.rock = Vector3(-0.1f * cock, 0.0f, 0.17f * rk * a);
		p.lean = Vector3(-0.05f * a, 0.0f, 0.0f);
		p.raise = lerp(base.raise, 0.55f, a);
		p.fwd = lerp(base.fwd, -0.55f, a);
		p.twist = 0.3f * a;
		p.hand = -0.3f * a;
		p.splay = 0.6f * a;
		p.tuck = 0.0f;
		p.leaf = 0.18f * a;
		p.stem = Vector3(-0.35f * a, 0.0f, 0.0f);
		p.eye_glow = lerp(EYE_GLOW, 1.35f, smoothstep01(remap01(w, 0.1f, 0.8f)));
		p.face_glow = lerp(FACE_GLOW, 2.0f, smoothstep01(remap01(w, 0.1f, 0.8f)));
		p.squint = 0.0f;
		p.anger = smoothstep01(remap01(w, 0.08f, 0.5f));
		p.roar = smoothstep01(remap01(w, 0.5f, 0.62f)); // the jaw drops as it cocks
		p.pupil = lerp(1.0f, 1.35f, a); // the flames flare up
		p.wide = 1.0f;
		p.tremble = cock;
		p.look = 1.0f;
		return p;
	}

	// The leap (Attack: launch to landing, about 0.65 s): stretched tall off the ground, tipped back with
	// both arms flung up over its head, then pitching forward over the top into the slam; the face
	// blazing all the way down. The flat landing itself is the squash spring's kick (slam_land).
	Pose leap(const AnimInput &in, const Pose &base, float t, float vy) const {
		Pose p = base;
		const float up = saturate(vy / LEAP_VY), downw = saturate(-vy / LEAP_VY);
		p.sy = 1.0f + 0.22f * up + 0.08f * downw;
		p.rock = Vector3(lerp(-0.3f, 0.0f, 1.0f - up) + 0.44f * downw, 0.0f, 0.0f);
		p.lean = Vector3(0.1f * downw, 0.0f, 0.0f);
		p.raise = 1.35f - 0.5f * downw;
		p.fwd = lerp(-0.3f, 0.5f, downw);
		p.twist = 0.2f;
		p.hand = -0.3f;
		p.tuck = 1.0f;
		p.leaf = 0.25f * downw - 0.05f * up;
		p.stem = Vector3(-0.35f * up + 0.2f * downw, 0.0f, 0.0f);
		p.eye_glow = 1.3f;
		p.face_glow = 2.0f;
		p.squint = 0.0f;
		p.anger = 0.8f;
		p.roar = 1.0f;
		p.pupil = 1.3f;
		p.wide = 1.08f;
		p.look = 1.0f;
		return p;
	}

	// After the slam (Recover): flattened by the impact, it re-inflates in a wobbly way, dizzy: the shell
	// rolls round in a cone, the pupils swim in circles, the eyes half shut, arms hanging flat out, the
	// stem drooped; a shake at the end and it is itself again.
	Pose dizzy(const AnimInput &in, const Pose &base, float t) const {
		Pose p = base;
		const float k = saturate(t / DIZZY_TIME);
		const float inflate = anim::ease_out_elastic(saturate(remap01(t, 0.05f, 0.95f)));
		const float dz = (1.0f - smoothstep01(remap01(k, 0.55f, 0.92f))) * smoothstep01(t / 0.15f);
		p.sy = lerp(0.76f, 1.0f, inflate);
		p.size = lerp(0.94f, 1.0f, smoothstep01(remap01(t, 0.1f, 1.1f)));
		const float a = in.t * 7.5f;
		p.rock = Vector3(0.1f * std::sin(a) * dz, 0.0f, 0.13f * std::cos(a) * dz);
		const float shake = anim::pulse(t - 1.05f, 0.06f, 0.3f);
		p.rock.y += 0.0f;
		p.raise = lerp(-0.45f, base.raise, smoothstep01(remap01(t, 0.25f, 1.2f)));
		p.fwd = lerp(0.6f, base.fwd, smoothstep01(remap01(t, 0.25f, 1.2f)));
		p.twist = lerp(1.2f, 0.0f, smoothstep01(remap01(t, 0.1f, 0.6f)));
		p.raise_d = 0.15f * std::sin(a * 0.5f) * dz;
		p.splay = lerp(1.0f, 0.0f, smoothstep01(remap01(t, 0.2f, 0.9f)));
		p.leaf = -0.04f * dz;
		p.stem = Vector3(0.0f, 0.0f, 0.5f * dz * std::sin(a + 1.0f)) + Vector3(0.2f * shake * std::sin(t * 60.0f), 0.0f, 0.0f);
		p.eye_glow = lerp(0.9f, EYE_GLOW, smoothstep01(remap01(t, 0.2f, 1.0f)));
		p.face_glow = lerp(1.4f, FACE_GLOW, smoothstep01(remap01(t, 0.2f, 1.0f)));
		p.squint = 0.45f * dz;
		p.roar = t < 0.32f ? 1.0f : 0.0f; // the slam knocks the wind out of it: mouth wide, then the grin
		p.anger = 0.0f;
		p.dizzy = dz;
		p.look = 0.0f;
		p.rock.z += 0.12f * shake * std::sin(t * 55.0f);
		return p;
	}

	// Hit (visual 0.62 s): squashed and knocked aside, the light knocked down to a dim glow, eyes
	// squeezed shut, arms flung up, leaves and stem whipped; it wobbles back and the light comes back.
	Pose hit_pose(const AnimInput &in, const Pose &base, float t) const {
		Pose p = base;
		const float snap = smoothstep01(remap01(t, 0.0f, 0.04f)) * (1.0f - smoothstep01(remap01(t, 0.2f, 0.5f)));
		const float dim = 1.0f - smoothstep01(remap01(t, 0.25f, 0.6f));
		p.sy = lerp(base.sy, 0.84f, snap);
		p.rock = base.rock + Vector3(-in.hit_dir_local.z, 0.0f, in.hit_dir_local.x) * (-0.18f * snap);
		p.raise = lerp(base.raise, 1.1f, snap);
		p.fwd = lerp(base.fwd, -0.3f, snap);
		p.hand = -0.4f * snap;
		p.splay = 0.6f * snap;
		p.leaf = 0.2f * snap;
		p.shut = t < 0.26f ? 1.0f : 0.0f;
		p.anger = 0.0f;
		p.roar = 0.0f;
		p.eye_glow = lerp(base.eye_glow, 0.18f, dim);
		p.face_glow = lerp(base.face_glow, 0.3f, dim);
		p.look = 0.0f;
		return p;
	}

	// Death (0.6 s): a start (the light flares, eyes wide), then the shell caves in on itself, wide and
	// low, the stem flops over, the arms and leaves go limp, and the candle sputters and goes out; the
	// eyes go dark with it. The root shrinks away at the very end (behind the game's puff).
	Pose death_pose(const AnimInput &in, const Pose &base) const {
		Pose p = base;
		const float k = saturate(in.state_t / DEATH_TIME);
		const float jolt = smoothstep01(remap01(k, 0.0f, 0.06f)) * (1.0f - smoothstep01(remap01(k, 0.1f, 0.22f)));
		const float cave = anim::ease_out_back(saturate(remap01(k, 0.12f, 0.62f)), 1.4f);
		p.sy = 1.0f + 0.14f * jolt;
		p.flat = lerp(1.0f, 0.5f, cave);
		p.spread = lerp(1.0f, 1.22f, cave);
		p.size = 1.0f;
		p.rock = Vector3(0.12f * cave, 0.0f, -0.08f * cave);
		p.lean = Vector3();
		p.raise = lerp(0.7f * jolt, -0.7f, cave);
		p.fwd = lerp(0.0f, 0.3f, cave);
		p.twist = 0.9f * cave;
		p.hand = 0.4f * cave;
		p.tuck = 0.0f;
		p.splay = 1.2f * cave;
		p.leaf = 0.25f * jolt - 0.05f * cave;
		p.stem = Vector3(0.25f * cave, 0.0f, -0.85f * cave);
		// the candle: a flare, then a sputter, then out
		const float out = smoothstep01(remap01(k, 0.18f, 0.55f));
		const float sputter = 1.0f + 0.7f * std::sin(k * 70.0f) * anim::pulse(k - 0.25f, 0.05f, 0.25f);
		p.eye_glow = (lerp(EYE_GLOW, 1.5f, jolt) * (1.0f - out)) * sputter;
		p.face_glow = (lerp(FACE_GLOW, 2.2f, jolt) * (1.0f - out)) * sputter;
		p.wide = 1.0f + 0.25f * jolt;
		p.shut = smoothstep01(remap01(k, 0.3f, 0.5f));
		p.squint = 0.0f;
		p.anger = 0.0f;
		p.roar = 0.0f;
		p.pupil = 1.0f;
		p.dizzy = 0.0f;
		p.look = 0.0f;
		p.ground = smoothstep01(remap01(k, 0.05f, 0.3f));
		return p;
	}

	// Spawn (0.6 s): buried, only the stem and the tendril tips poke out of the ground, wiggling; it
	// bursts up deflated and inflates with an overshoot as it lands, the arms flung out, then the candle
	// catches and the eyes pop open.
	Pose rise_pose(const AnimInput &in, const Pose &base) const {
		Pose p = base;
		const float k = saturate(in.state_t / RISE_TIME);
		const float up = smoothstep01(remap01(k, 0.28f, 0.6f));
		const float grow = k < 0.3f ? 0.0f : anim::ease_out_back(saturate(remap01(k, 0.3f, 0.78f)), 2.2f);
		p.sink = lerp(0.47f, 0.0f, up) + 0.02f * std::sin(k * 40.0f) * (1.0f - up);
		p.ground = 1.0f - smoothstep01(remap01(k, 0.62f, 0.9f));
		p.size = Math::max(0.3f, lerp(0.55f, 1.0f, grow));
		p.sy = lerp(1.25f, 1.0f, smoothstep01(remap01(k, 0.5f, 0.85f)));
		p.rock = Vector3();
		p.lean = Vector3();
		const float wig = (1.0f - up) * std::sin(in.state_t * 30.0f);
		p.stem = Vector3(0.15f * wig, 0.0f, 0.25f * wig);
		p.raise = lerp(-0.5f, 1.0f, up) * (1.0f - smoothstep01(remap01(k, 0.75f, 1.0f))) + 0.0f;
		p.fwd = 0.0f;
		p.tuck = up * (1.0f - smoothstep01(remap01(k, 0.65f, 0.85f)));
		p.leaf = lerp(-0.05f, 0.3f, up) * (1.0f - smoothstep01(remap01(k, 0.8f, 1.0f)));
		const float lit = smoothstep01(remap01(k, 0.62f, 0.8f));
		const float catchf = 1.0f + 0.6f * anim::pulse(k - 0.7f, 0.04f, 0.18f);
		p.eye_glow = EYE_GLOW * lit * catchf;
		p.face_glow = FACE_GLOW * lit * catchf;
		p.shut = k < 0.72f ? 1.0f : 0.0f;
		p.wide = 1.0f + 0.2f * anim::pulse(k - 0.72f, 0.04f, 0.15f);
		p.look = 0.0f;
		return p;
	}

	int body_ = -1, head_ = -1, stem_ = -1, mouth_ = -1, mouth_glow_ = -1;
	int brow_[2] = { -1, -1 };
	int leaf_[NL] = { -1, -1, -1, -1 };
	Vector3 leaf_dir_[NL];
	int eye_[2] = { -1, -1 }, pupil_[2] = { -1, -1 }, arm_[2] = { -1, -1 }, hand_[2] = { -1, -1 }, foot_[2] = { -1, -1 }, tendril_[2] = { -1, -1 };
	MonsterFace face_;
	HitRecoil hit_;
	Gait gait_;
	anim::Weight w_wind_, w_attack_, w_dizzy_;
	anim::Spring squash_;
	anim::Spring3 rock_, lean_, arm_s_[2];
	anim::Dangle stem_d_, leaf_d_[NL], ten_d_[2];
	anim::RandTimer fidget_timer_;
	Vector3 air_hv_;
	float airborne_t_ = 0.0f, fall_ = 0.0f, air_w_ = 0.0f, hop_h_ = 0.0f;
	bool held_ = false;
	float wind_ = 0.0f, attack_t_ = 0.0f, recover_t_ = 10.0f, slam_t_ = 10.0f, hit_t_ = 10.0f, fidget_t_ = 10.0f;
	float phase0_ = 0.0f, look_w_ = 0.0f, idle_for_ = 0.0f;
	float eye_glow_ = -1.0f, face_glow_ = -1.0f, anger_ = 0.0f, gutter_ = 0.0f;
	float glow_sent_[4] = { -1000.0f, -1000.0f, -1000.0f, -1000.0f };
	Vector2 flame_look_[2];
	int fidget_n_ = 0;
	bool seeded_ = false, leap_kick_ = false, was_air_ = false, roar_on_ = false;
};

} // namespace

TOT_MONSTER_ANIMATOR(MonsterType::PumpkinHopper, PumpkinAnimator)

} // namespace tot
