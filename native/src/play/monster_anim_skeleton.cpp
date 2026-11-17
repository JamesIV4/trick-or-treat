// Rattlebones (the skeleton, a THROW monster): the animator for its v2 rig.
//
// Part tree (art/team/monsters/batch04/build_monsters2.py):
//   body (pelvis) > chest > head > jaw, eye_l / eye_r (dark socket linings) > pupil_l / pupil_r (the lights)
//                         > bandana (soft front flap), bandana_tail (soft)
//                         > arm_l > forearm_l > hand_l            (+X side: the open hand that points)
//                         > arm_r > forearm_r > hand_r > bone     (-X side: the throwing arm)
//                 > leg_l > shin_l > foot_l,  leg_r > shin_r > foot_r
// Arms and legs hang straight down at rest and are posed with two-bone IK: the hands in the chest's
// space (so they ride its twist), the feet in model space (so they stay planted while the pelvis
// bounces and turns).
//
// How a frame is built: a locomotion pose (idle to a bow-legged bouncy walk, by distance travelled),
// then state layers mixed over it by weight (wind-up, throw follow-through, pulling a new bone and
// laughing, flinch, fidgets), then springs (bobble head, landing dip, bandana), then the solve.
// Spawning and dying blend the whole solved pose with a heap of bones on the ground (apply_pile).
//
// The throw and the projectile: Monster spawns the bone on the physics tick the wind-up ends and asks
// muzzle() where from. So the whip forward happens at the END of the wind-up, the hand is held at
// the release point for its last frames, the held bone is hidden on the first frame of Attack (the
// frame the projectile first shows) and the Attack state plays the follow-through.

#include "play/monster_anim.h"

#include <godot_cpp/core/math.hpp>

#include <algorithm>
#include <cmath>
#include <vector>

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
constexpr float STRIDE = 0.62f; // metres per full walk cycle (a left and a right step)
constexpr float STEP_H = 0.075f; // how high a swinging foot lifts
constexpr float CROUCH = 0.04f; // the pelvis sits this far below rest, so the knees are always bowed
constexpr float STANCE = 0.04f; // feet this much wider than the hips
constexpr float TOE_OUT = 0.4f; // duck feet, radians
constexpr float CHASE_SPEED = 1.9f; // its speed when chasing (tuning.speed): the gait is fullest there
constexpr float ARM_REACH = 0.325f; // just short of the arm's 0.33 m, so a pointing elbow keeps a hint of bend
constexpr float WHIP_START = 0.80f, WHIP_END = 0.955f; // fractions of the wind-up: the arm whips forward between them
constexpr float RISE_TIME = 0.6f, DEATH_TIME = 0.6f;
constexpr float NEW_BONE_AT = 0.5f; // seconds into Recover when the new bone pops into the hand
// Spawning, the skull lies beside the heap on the open hand's side (+X; its neck pivot, model space, never
// mirrored), tipped over outward, and hops on last (apply_pile), starting at this fraction of the rise.
constexpr float SKULL_RISE_AT = 0.5f;
const Vector3 SKULL_RISE_POS(0.6f, 0.06f, 0.12f);
const Quaternion SKULL_RISE_ROT = Quaternion::from_euler(Vector3(0.25f, 0.5f, -0.4f));

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
	// Poses upper and mid so the end joint reaches `target`, bending toward `pole` (both in the space
	// the upper joint hangs in). Returns the mid joint's basis in that space, to orient the end part.
	Basis solve(MonsterRig &rig, const Vector3 &hinge, const Vector3 &target, const Vector3 &pole) const {
		const anim::Ik2 ik = anim::two_bone_ik(root, a, b, hinge, target, pole, 2.5f);
		rig.pose(upper, Basis(ik.upper));
		rig.pose(mid, Basis(ik.lower));
		return Basis(ik.upper) * Basis(ik.lower);
	}
};

// Everything the solve needs for one frame. Layers are mixed into it field by field.
struct Pose {
	Vector3 pelvis_off; // metres, model space, on top of the rest position
	Vector3 pelvis_rot, chest_rot, head_rot; // Euler (pitch, yaw, roll); head_rot goes on top of the look
	Vector3 hand[2], pole[2]; // wrist targets and elbow poles, chest space
	Vector3 bone_dir = Vector3(0, 1, 0); // where the held bone points, chest space
	Vector3 foot[2]; // ankle targets, model space
	float foot_pitch[2] = { 0.0f, 0.0f }, foot_yaw[2] = { 0.0f, 0.0f };
	float jaw = 0.0f; // radians open
	float point = 0.0f; // 0..1: the left arm points at the player instead of going to hand[L]
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
	d.bone_dir = d.bone_dir.lerp(s.bone_dir, w);
	for (int i = 0; i < 2; ++i) {
		d.hand[i] = d.hand[i].lerp(s.hand[i], w);
		d.pole[i] = d.pole[i].lerp(s.pole[i], w);
		d.foot[i] = d.foot[i].lerp(s.foot[i], w);
		d.foot_pitch[i] = lerp(d.foot_pitch[i], s.foot_pitch[i], w);
		d.foot_yaw[i] = lerp(d.foot_yaw[i], s.foot_yaw[i], w);
	}
	d.jaw = lerp(d.jaw, s.jaw, w);
	d.point = lerp(d.point, s.point, w);
}

// The throwing hand's path round the big skull, in chest space: azimuth about the chest's up axis
// (0 = straight ahead, 90 = out to its right, 140 = behind its right shoulder), distance from that
// axis and height. Staying 0.33 m out keeps the fist and the bone clear of the skull.
Vector3 sling(float az_deg, float radius, float height) {
	const float a = Math::deg_to_rad(az_deg);
	return Vector3(-radius * std::sin(a), height, radius * std::cos(a));
}

Quaternion lying(float yaw, float side) { // a hanging bone laid flat on the ground, pointing along `yaw`
	return anim::quat_axis_angle(Vector3(0, 1, 0), yaw) * anim::quat_axis_angle(Vector3(0, 0, 1), side * PI * 0.5f);
}

class SkeletonAnimator : public MonsterAnimator {
public:
	float rise_time() const override { return RISE_TIME; }
	float death_time() const override { return DEATH_TIME; }
	float telegraph_gain() const override { return 1.15f; } // (final fixes 2) linear body_flash: at least ~0.9 of BEFORE's pulse (flash_metric.py)
	float hit_flash_gain() const override { return 0.9f; } // (final fixes 2) the fade after the white peak (linear body_flash)
	int thrown_part(const MonsterRig &rig) const override { return rig.slot("bone"); } // the projectile is this bone

	// The middle of the held bone. The hand is held at the release point when this is asked.
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
		chest_ = rig.slot("chest");
		head_ = rig.slot("head");
		jaw_ = rig.slot("jaw");
		bone_ = rig.slot("bone");
		bandana_ = rig.slot("bandana");
		tail_ = rig.slot("bandana_tail");
		arm_[L].bind(rig, "arm_l", "forearm_l", "hand_l");
		arm_[R].bind(rig, "arm_r", "forearm_r", "hand_r");
		leg_[L].bind(rig, "leg_l", "shin_l", "foot_l");
		leg_[R].bind(rig, "leg_r", "shin_r", "foot_r");
		for (int s = 0; s < 2; ++s) {
			ankle_rest_[s] = rig.model_rest(leg_[s].end).origin; // every part is at rest here
		}
		face_.bind(rig);
		face_.max_yaw = 0.9f; // the skull stays on the player while the chest coils 35 degrees away
		face_.max_pitch = 0.4f;
		face_.look_height = 0.75f;
		// The eyes are a soft teal light inside each dark socket (M.socket_eye): eye_l / eye_r are the
		// socket linings, pupil_l / pupil_r the lights. MonsterFace still turns the head and runs the
		// blinks; the lights are posed and lit here (pose_eyes), after it.
		face_.glow_rest = -1.0f;
		for (int s = 0; s < 2; ++s) {
			socket_[s] = rig.slot(s == L ? "eye_l" : "eye_r");
			light_[s] = rig.slot(s == L ? "pupil_l" : "pupil_r");
		}
		// All the bone is matte, chalky bone, the skull and the body alike (polish round: the body kept a
		// smooth toy sheen and read as a second material; a broad highlight on the big skull read as a
		// plastic toy), the sockets fully matte (a highlight in the dark reads as a glass lens), no sheen
		// on the cloth. The rim light is turned down from the monsters' usual 0.35: on pale bone it
		// washes the form out.
		for (int i = 0; i < rig.count(); ++i) {
			rig.gloss(i, -1.6f);
		}
		rig.param_all("rim_strength", 0.12f);
		for (int s = 0; s < 2; ++s) {
			rig.gloss(socket_[s], -2.0f);
			rig.gloss(light_[s], 0.0f);
		}
		for (int s : { bandana_, tail_ }) {
			rig.gloss(s, 0.0f); // cloth: a highlight there is a flash across the whole chest
		}
		// Soft parts: the flap runs down the chest from its pin (the neck ring stays stiff), the tails
		// are measured by distance from the knot.
		flap_dir_ = Vector3(0.0f, -1.0f, 0.22f).normalized();
		rig.soft(bandana_, flap_dir_, 0.15f, 0.012f);
		rig.soft(tail_);
		tail_dir_ = rig.tip(tail_).length_squared() > 1e-8f ? rig.tip(tail_).normalized() : Vector3(0, -1, 0);

		// The heap of bones a spawn assembles from and a death collapses into: where each big part
		// lies (model space, the pivot's position) and when it moves, as fractions of the state.
		pile_.clear();
		auto add = [&](const char *name, const Vector3 &pos, const Quaternion &rot, float fall, float rise) {
			const int s = rig.slot(name);
			if (s >= 0) {
				pile_.push_back({ s, pos, rot, fall, rise });
			}
		};
		const auto euler = [](float pitch, float yaw, float roll) { return Quaternion::from_euler(Vector3(pitch, yaw, roll)); };
		add("body", Vector3(0.10f, 0.075f, 0.07f), euler(1.3f, 0.5f, 0.0f), 0.06f, 0.18f);
		add("chest", Vector3(-0.02f, 0.105f, 0.11f), euler(-1.25f, -0.25f, 0.1f), 0.12f, 0.26f);
		add("head", Vector3(0.0f, 0.19f, 0.0f), euler(0.14f, -0.3f, 0.2f), 0.2f, 0.5f);
		add("arm_l", Vector3(0.21f, 0.05f, 0.10f), lying(-1.2f, 1.0f), 0.08f, 0.34f);
		add("forearm_l", Vector3(0.09f, 0.13f, -0.10f), lying(0.8f, 0.8f), 0.12f, 0.38f);
		add("hand_l", Vector3(0.23f, 0.045f, -0.12f), euler(1.4f, 0.4f, 0.0f), 0.16f, 0.42f);
		add("arm_r", Vector3(-0.17f, 0.05f, 0.02f), lying(1.0f, -1.0f), 0.08f, 0.34f);
		add("forearm_r", Vector3(-0.10f, 0.05f, -0.19f), lying(-0.4f, 1.0f), 0.12f, 0.38f);
		add("hand_r", Vector3(-0.21f, 0.06f, 0.17f), euler(0.0f, 0.3f, -1.3f), 0.16f, 0.42f);
		add("leg_l", Vector3(0.03f, 0.06f, 0.21f), lying(-0.5f, -1.0f), 0.03f, 0.12f);
		add("shin_l", Vector3(0.21f, 0.05f, -0.02f), lying(1.9f, 1.0f), 0.0f, 0.06f);
		add("foot_l", Vector3(0.15f, 0.075f, 0.18f), euler(0.0f, 0.6f, 0.0f), 0.05f, 0.0f);
		add("leg_r", Vector3(-0.23f, 0.06f, -0.06f), lying(2.4f, 1.0f), 0.03f, 0.12f);
		add("shin_r", Vector3(-0.06f, 0.05f, 0.23f), lying(0.3f, 1.0f), 0.0f, 0.06f);
		add("foot_r", Vector3(-0.17f, 0.075f, 0.12f), euler(0.0f, -0.9f, 0.0f), 0.05f, 0.0f);
		std::sort(pile_.begin(), pile_.end(), [](const Scatter &a, const Scatter &b) { return a.slot < b.slot; }); // parents first
	}

	void update(const AnimInput &in, MonsterRig &rig, Transform3D &root) override {
		using State = MonsterAnimState;
		const float dt = anim::safe_dt(in.dt);
		const bool far = in.lod > 0;
		if (!seeded_) {
			seeded_ = true;
			chatter_ = anim::RandTimer(in.seed ^ 0x51u);
			fidget_ = anim::RandTimer(in.seed ^ 0xF1D6u);
			mirror_ = ((in.seed >> 3) & 1u) != 0;
			phase0_ = anim::hash01(in.seed) * TAU;
		}
		const bool dying = in.state == State::Dying, rising = in.state == State::Rise;
		const bool alive = !dying && !rising;

		// ---- clocks and one-shots
		AnimInput walk = in;
		if (dying) {
			walk.speed = 0.0f; // a dead body keeps its last velocity; the legs must not keep walking
		}
		const float last_phase = gait_.phase;
		gait_.step(walk, STRIDE, 0.35f);
		const float amt = gait_.amount;
		const float run = saturate(walk.speed / CHASE_SPEED);
		const Vector3 recoil = hit_.update(in); // sets hit_.fresh and hit_.ouch
		const bool landed = in.on_floor && !was_floor_;
		if (!in.on_floor) {
			fall_ = in.vel_local.y;
		}
		was_floor_ = in.on_floor;

		// The held bone: gone the frame the projectile appears, back with a pop.
		if (in.state == State::Attack && in.entered) {
			have_bone_ = false;
		} else if (!have_bone_ && ((in.state == State::Recover && in.state_t >= NEW_BONE_AT) || (in.state != State::Attack && in.state != State::Recover))) {
			have_bone_ = true;
			bone_pop_ = 0.0f;
		}
		bone_pop_ += dt;
		rig.show(bone_, have_bone_);

		// Jaw chatter bursts, and idle fidgets when it stands about with nobody to chase.
		chatter_t_ += dt;
		if (alive && (in.state == State::Roam || in.state == State::Recover) && chatter_.step(dt, in.chasing ? 1.4f : 2.6f, in.chasing ? 3.2f : 6.0f)) {
			chatter_t_ = 0.0f;
		}
		const bool idle = in.state == State::Roam && !in.chasing && in.speed < 0.15f && in.on_floor;
		fidget_t_ += dt;
		if (!idle) {
			fidget_t_ = Math::max(fidget_t_, 10.0f); // drop a fidget the moment it has something to do
			fidget_.reset();
		} else if (fidget_t_ > fidget_len() + 0.4f && fidget_.step(dt, 1.6f, 3.6f)) {
			fidget_kind_ = fidget_.count % 3;
			fidget_t_ = 0.0f;
		}

		// ---- the pose: locomotion, then the state layers
		Pose p = locomotion(in, amt, run);
		w_wind_.step(in.state == State::Windup, 0.1f, 0.12f, dt);
		w_attack_.step(in.state == State::Attack, 0.0f, 0.3f, dt);
		w_air_.step(alive && !in.on_floor, 0.07f, 0.1f, dt);
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
			mix(p, follow_through(attack_t_), w_attack_.w);
		}
		float laugh = 0.0f;
		if (in.state == State::Recover) {
			laugh = recover(in, p);
		}
		fidgets(in, p);
		// A flinch: the whole body is thrown open like a star (arms flung wide and high, one higher
		// than the other, chest arched away from the blow, jaw dropped), so the recoil reads as a
		// silhouette even under the hit flash.
		const float ouch = alive ? hit_.ouch : 0.0f;
		if (ouch > 0.0f) {
			Pose h = p;
			const float lop = mirror_ ? -1.0f : 1.0f; // which arm goes higher
			for (int s = 0; s < 2; ++s) {
				const float up = 0.15f + 0.07f * lop * SIDE[s];
				h.hand[s] = arm_[s].root + Vector3(SIDE[s] * 0.27f, up, -0.02f - 0.1f * in.hit_dir_local.z);
				h.pole[s] = arm_[s].root + Vector3(SIDE[s] * 0.3f, -0.4f, -0.2f);
			}
			h.bone_dir = Vector3(-0.5f, 0.8f, -0.3f);
			h.chest_rot += Vector3(-0.22f, 0.0f, 0.1f * lop);
			h.head_rot += Vector3(-0.2f, 0.0f, -0.3f * lop);
			h.jaw = 0.5f;
			h.point = 0.0f;
			mix(p, h, ouch);
		}

		// ---- springs
		// Rattle: a fast shiver through the loose bones, with speed, with a flinch and through the coil.
		const float rattle = far ? 0.0f : saturate(0.55f * run + 0.9f * ouch + 0.5f * w_wind_.w * (1.0f - whip(wind_)));
		const auto jit = [&](uint32_t k) { return anim::noise1(in.t * 24.0f, in.seed + k * 7919u) * rattle; };
		p.chest_rot.z += 0.025f * jit(0);
		p.head_rot.z += 0.035f * jit(1);
		p.head_rot.x += 0.025f * jit(2);
		p.hand[L] += Vector3(jit(3), jit(4), jit(5)) * 0.01f;
		p.hand[R] += Vector3(jit(6), jit(7), jit(8)) * 0.008f;
		p.jaw += 0.05f * std::fabs(jit(9));
		// Jaw chatter and the laugh.
		const float chat = anim::pulse(chatter_t_, 0.05f, 0.5f);
		p.jaw += 0.2f * chat * (0.5f + 0.5f * std::sin(in.t * TAU * 14.0f));
		p.jaw += laugh * (0.1f + 0.22f * (0.5f + 0.5f * std::sin(in.t * TAU * 7.0f)));
		if (dying) {
			p.jaw = lerp(p.jaw, 0.45f, smoothstep01(in.state_t / 0.2f));
		}
		// Bobble head: a loose spring on the neck, nodded by every footfall, thrown by starts, stops,
		// hits and landings.
		if (far) {
			bobble_.reset();
			dip_.reset();
		} else {
			if (amt > 0.2f && int(std::floor(last_phase * 2.0f)) != int(std::floor(gait_.phase * 2.0f))) {
				const float side = gait_.phase < 0.5f ? 1.0f : -1.0f;
				bobble_.kick(Vector3(1.5f, 0.0f, 0.9f * side) * amt * (0.6f + 0.4f * run));
			}
			bobble_.kick(Vector3(-in.accel_local.z, 0.0f, in.accel_local.x) * (0.9f * dt));
			if (hit_.fresh) {
				bobble_.kick(Vector3(in.hit_dir_local.z, 0.0f, -in.hit_dir_local.x) * 6.0f);
			}
			if (landed) {
				bobble_.kick(Vector3(3.0f, 0.0f, 0.0f));
				dip_.kick(Math::clamp(fall_, -8.0f, 0.0f) * 0.33f);
				chatter_t_ = 0.2f; // teeth clack
			}
			bobble_.step(Vector3(), 3.0f, 0.2f, dt);
			if (bobble_.x.length() > 0.55f) {
				bobble_.x = bobble_.x.normalized() * 0.55f;
			}
			dip_.step(0.0f, 4.5f, 0.45f, dt);
		}
		p.pelvis_off.y += Math::clamp(dip_.x, -0.09f, 0.03f);
		// The skull hops off the neck when hit and drops back.
		const float hop_t = in.since_hit / 0.34f;
		const float skull_hop = alive && hop_t < 1.0f ? 0.11f * anim::arc(hop_t) : 0.0f;

		// ---- solve: pelvis, chest, head
		rig.pose(body_, Basis::from_euler(p.pelvis_rot), p.pelvis_off);
		const Basis chest_basis = Basis(anim::quat_from_rotvec(recoil * 0.55f)) * Basis::from_euler(p.chest_rot);
		rig.pose(chest_, chest_basis);
		const bool throwing = in.state == State::Windup || in.state == State::Attack;
		face_.look_weight = throwing ? 1.0f : (look_away_ ? 0.0f : -1.0f);
		// The skull stays on its target through the throw: with the usual lag it would be carried round
		// by the chest's whip and show the camera the back of its head.
		face_.head_lag = anim::damp(face_.head_lag, throwing ? 0.03f : 0.15f, 0.08f, dt);
		face_.wide = 1.0f + 0.14f * w_wind_.w * smoothstep01(remap01(wind_, 0.0f, 0.4f)) + 0.1f * laugh;
		face_.shut = dying ? smoothstep01(in.state_t / 0.15f) : (rising ? 1.0f - smoothstep01(remap01(in.state_t / RISE_TIME, 0.8f, 0.97f)) : 0.0f);
		face_.update(in, rig);
		pose_eyes(in, rig, throwing, laugh, far, dt);
		rig.pose(head_,Basis::from_euler(anim::look_euler(face_.head)) * Basis::from_euler(p.head_rot) * Basis(anim::quat_from_rotvec(bobble_.x)),
				Vector3(0.0f, skull_hop, 0.0f));
		rig.pose(jaw_, Basis(Vector3(1, 0, 0), Math::clamp(p.jaw, 0.0f, 0.5f)));

		// ---- arms (chest space)
		if (p.point > 0.0f) {
			// Aim the left arm at the player's chest: the player in the chest's own space.
			const Transform3D chest_model = rig.model_xf(chest_);
			Vector3 dir = chest_model.affine_inverse().xform(in.player_local + Vector3(0.0f, 0.8f, 0.0f)) - arm_[L].root;
			dir.y = Math::clamp(dir.y, -0.25f * dir.length(), 0.4f * dir.length());
			if (dir.length_squared() > 1e-6f) {
				p.hand[L] = p.hand[L].lerp(arm_[L].root + dir.normalized() * ARM_REACH, saturate(p.point));
			}
		}
		const Vector3 elbow_hinge(-1, 0, 0); // folds the forearm forward
		arm_[L].solve(rig, elbow_hinge, p.hand[L], p.pole[L]);
		const Basis fore_r = arm_[R].solve(rig, elbow_hinge, p.hand[R], p.pole[R]);
		// The fist turns so the bone (its local +Z) points along bone_dir; smoothed, so it never snaps.
		const Vector3 bd = p.bone_dir.length_squared() > 1e-6f ? p.bone_dir.normalized() : Vector3(0, 1, 0);
		const Quaternion fist = anim::quat_between(Vector3(0, 0, 1), fore_r.xform_inv(bd).normalized());
		fist_ = fist_primed_ && !far ? anim::damp_quat(fist_, fist, 0.025f, dt) : fist;
		fist_primed_ = true;
		rig.pose(arm_[R].end, Basis(fist_));
		// The left hand trails its forearm a little.
		rig.pose(arm_[L].end, Basis(Vector3(1, 0, 0), 0.25f * (1.0f - p.point) - 0.3f * ouch));

		// ---- the held bone: pops in, and is tossed and caught in one of the fidgets
		Basis bone_rot;
		Vector3 bone_off;
		const float pop = anim::ease_out_back(saturate(bone_pop_ / 0.3f), 3.0f);
		if (fidget_kind_ == 1 && fidget_t_ < fidget_len()) {
			const float u = fidget_t_ / fidget_len();
			const float air = remap01(u, 0.18f, 0.85f);
			bone_off = Vector3(0.0f, 0.0f, 0.24f * anim::arc(air)); // along the bone: up, in the carry pose
			bone_rot = Basis(Vector3(1, 0, 0), TAU * smoothstep01(air));
		}
		rig.pose(bone_, bone_rot, bone_off, Vector3(pop, pop, pop));

		// ---- legs (feet are planted in model space; the legs hang from the posed pelvis)
		const Transform3D body_model = rig.model_xf(body_);
		const Transform3D to_body = body_model.affine_inverse();
		const Vector3 knee_hinge(1, 0, 0); // folds the shin back
		for (int s = 0; s < 2; ++s) {
			Vector3 foot = p.foot[s];
			float pitch = p.foot_pitch[s];
			const Vector3 hip = body_model.xform(leg_[s].root);
			if (w_air_.w > 0.0f) { // off the ground: the legs dangle and pedal
				// Knocked into the air by a hit, they kick apart; otherwise they hang and pedal.
				const Vector3 hang = hip + Vector3(SIDE[s] * (0.035f + 0.17f * ouch), -0.3f + 0.07f * ouch, 0.04f + 0.06f * std::sin(in.t * 11.0f + float(s) * PI) + 0.08f * ouch * SIDE[s]);
				foot = foot.lerp(hang, w_air_.w);
				pitch = lerp(pitch, 0.55f, w_air_.w);
			}
			const Vector3 pole = hip + Vector3(SIDE[s] * 0.5f, -0.1f, 0.55f); // knees out and forward: bow legs
			const Basis shin = body_model.basis * leg_[s].solve(rig, knee_hinge, to_body.xform(foot), to_body.xform(pole));
			const Basis want = Basis(Vector3(0, 1, 0), p.foot_yaw[s]) * Basis(Vector3(1, 0, 0), pitch);
			rig.pose(leg_[s].end, shin.inverse() * want);
		}

		// ---- bandana: one soft mesh each, thrown about by the chest
		if (far) {
			flap_.reset();
			tails_.reset();
			rig.bend(bandana_, Vector3());
			rig.bend(tail_, Vector3());
		} else {
			if (hit_.fresh) {
				const Vector3 d = Basis(Vector3(0, 1, 0), in.yaw).xform(in.hit_dir_local);
				flap_.kick(d * 2.5f + Vector3(0, 1.5f, 0));
				tails_.kick(d * 3.0f + Vector3(0, 2.0f, 0));
			}
			anim::DangleParams fp;
			fp.len = 0.15f;
			fp.hz = 2.6f;
			fp.zeta = 0.3f;
			fp.drag = 0.2f;
			fp.limit = 0.9f;
			rig.bend(bandana_, flap_.step(rig.world_rest(bandana_), flap_dir_, fp, dt));
			anim::DangleParams tp;
			tp.len = Math::max(rig.reach(tail_), 0.05f);
			tp.hz = 1.9f;
			tp.zeta = 0.25f;
			tp.drag = 0.35f;
			tp.limit = 1.2f;
			rig.bend(tail_, tails_.step(rig.world_rest(tail_), tail_dir_, tp, dt));
		}

		// ---- spawn and death: the solved pose blended with the heap of bones
		float root_scale = 1.0f;
		if (rising) {
			const float k = saturate(in.state_t / RISE_TIME);
			apply_pile(rig, k, true);
			root_scale = lerp(0.35f, 1.0f, anim::ease_out(saturate(k / 0.12f)));
		} else if (dying) {
			const float k = saturate(in.state_t / DEATH_TIME);
			apply_pile(rig, k, false);
			root_scale = 1.0f - 0.9f * smoothstep01(remap01(k, 0.92f, 1.0f)); // the heap winks out
		}

		// ---- the root: facing, and half the flinch (the chest took the rest)
		root = root_pose(in.yaw, alive ? recoil * 0.45f : Vector3(), Vector3(root_scale, root_scale, root_scale));
	}

private:
	struct Scatter {
		int slot;
		Vector3 pos; // the part's pivot in the heap, model space
		Quaternion rot;
		float fall, rise; // when it starts to drop (death) / to fly into place (spawn), 0..1 of the state
	};

	static float whip(float wind) { return anim::ease_in_out(remap01(wind, WHIP_START, WHIP_END)); }
	float fidget_len() const { return fidget_kind_ == 0 ? 1.5f : (fidget_kind_ == 1 ? 1.0f : 1.6f); }

	// Idle to a bow-legged, bouncy walk. The cycle is driven by distance, so the feet do not slide.
	Pose locomotion(const AnimInput &in, float amt, float run) const {
		Pose p;
		const float ph = gait_.phase * TAU;
		const float sn = std::sin(ph), cs = std::cos(ph);
		const float bounce = std::fabs(sn); // up at each mid-stance
		const float breath = std::sin(in.t * 2.1f + phase0_);
		const float sway = std::sin(in.t * 0.9f + phase0_ * 1.7f) * (1.0f - amt);
		p.pelvis_off = Vector3(0.018f * amt * sn + 0.006f * sway, -CROUCH + 0.034f * amt * bounce * (0.7f + 0.3f * run) + 0.004f * breath * (1.0f - amt), 0.0f);
		p.pelvis_rot = Vector3(0.05f * amt * run, -0.17f * amt * cs, 0.07f * amt * sn);
		p.chest_rot = Vector3(0.05f * amt + 0.1f * amt * run + 0.015f * breath, 0.22f * amt * cs, -0.06f * amt * sn + 0.012f * sway);
		p.head_rot = Vector3(-0.05f * amt, -0.07f * amt * cs, -0.02f * sway);
		for (int s = 0; s < 2; ++s) {
			float f = gait_.phase + (s == L ? 0.0f : 0.5f);
			f -= std::floor(f);
			float z, lift = 0.0f, pitch;
			if (f < 0.5f) { // planted, sliding back under the body; the heel strikes first
				z = STRIDE * (0.25f - f);
				pitch = -0.3f * (1.0f - saturate(f / 0.08f));
			} else { // swinging forward: toe down as it leaves, toe up as it reaches
				const float u = (f - 0.5f) * 2.0f;
				z = STRIDE * (-0.25f + 0.5f * smoothstep01(u));
				lift = STEP_H * std::sin(PI * u);
				pitch = u < 0.5f ? 0.55f * std::sin(TAU * u) : -0.3f * std::sin(PI * (u - 0.5f));
			}
			p.foot[s] = ankle_rest_[s] + Vector3(SIDE[s] * STANCE, lift * amt, z * amt);
			p.foot_pitch[s] = pitch * amt;
			p.foot_yaw[s] = SIDE[s] * TOE_OUT;
		}
		// Arms swing against the legs: the left hand hangs loose, the right carries the bone upright.
		p.hand[L] = arm_[L].root + Vector3(0.075f, -0.295f + 0.03f * amt * bounce, 0.035f - 0.12f * amt * cs);
		p.pole[L] = arm_[L].root + Vector3(0.3f, -0.2f, -0.4f);
		p.hand[R] = arm_[R].root + Vector3(-0.055f, -0.14f + 0.025f * amt * bounce + 0.006f * breath, 0.185f + 0.06f * amt * cs);
		p.pole[R] = arm_[R].root + Vector3(-0.3f, -0.25f, -0.35f);
		p.bone_dir = Vector3(-0.2f, 0.93f, 0.25f + 0.25f * amt * cs);
		return p;
	}

	// The wind-up: coil (right arm back behind the skull, left arm pointing, chest twisted away, feet
	// set), hold with a tremble, then whip round the skull to the release point and hold there.
	Pose windup(const AnimInput &in, float wind) const {
		Pose p;
		const float c = smoothstep01(remap01(wind, 0.0f, 0.42f));
		const float s = whip(wind);
		const float tremble = c * (1.0f - s) * std::sin(in.t * 38.0f);
		p.pelvis_off = Vector3(0.0f, -CROUCH - 0.035f * c * (1.0f - 0.5f * s), lerp(-0.04f * c, 0.045f, s));
		p.pelvis_rot = Vector3(0.0f, lerp(-0.22f * c, 0.16f, s), 0.0f);
		p.chest_rot = Vector3(lerp(-0.2f * c, 0.24f, s), lerp(-0.61f * c, 0.5f, s), 0.06f * c * (1.0f - s) + 0.01f * tremble);
		p.head_rot = Vector3(-0.06f * c, 0.0f, 0.08f * c * (1.0f - s));
		p.foot[L] = ankle_rest_[L] + Vector3(STANCE, 0.0f, 0.07f * c);
		p.foot[R] = ankle_rest_[R] + Vector3(-STANCE - 0.02f * c, 0.0f, -0.1f * c);
		p.foot_yaw[L] = TOE_OUT * 0.5f;
		p.foot_yaw[R] = -TOE_OUT - 0.4f * c;
		// The throwing hand: up and back, then round the outside of the skull.
		const Vector3 coil = sling(140.0f, 0.33f, 0.42f + 0.008f * tremble);
		const Vector3 path = sling(lerp(140.0f, 38.0f, s), lerp(0.33f, 0.36f, s) + 0.07f * std::sin(PI * s), lerp(0.42f, 0.39f, s) + 0.06f * std::sin(PI * s));
		const Vector3 carry = arm_[R].root + Vector3(-0.055f, -0.14f, 0.185f);
		p.hand[R] = s > 0.0f ? path : carry.lerp(coil, c);
		p.pole[R] = arm_[R].root + Vector3(-0.45f, -0.05f - 0.2f * s, -0.25f + 0.3f * s);
		p.bone_dir = Vector3(-0.2f, 0.93f, 0.25f).lerp(Vector3(-0.25f, 0.55f, -0.8f), c).lerp(Vector3(0.0f, 0.35f, 0.95f), s);
		// The left arm points at the player, then sweeps back as the throw comes round.
		p.hand[L] = arm_[L].root + Vector3(0.1f, -0.2f, -0.2f);
		p.pole[L] = arm_[L].root + Vector3(0.3f, -0.3f, -0.3f);
		p.point = c * (1.0f - s);
		p.jaw = 0.07f * c + 0.3f * s;
		return p;
	}

	// Attack (0.3 s, the bone is in the air): the arm carries on across the body, the back foot steps through.
	Pose follow_through(float t) const {
		Pose p;
		const float f = anim::ease_out(saturate(t / 0.2f));
		p.pelvis_off = Vector3(0.0f, -CROUCH - 0.02f * f, lerp(0.045f, 0.06f, f));
		p.pelvis_rot = Vector3(0.06f * f, lerp(0.16f, 0.22f, f), 0.0f);
		p.chest_rot = Vector3(lerp(0.24f, 0.36f, f), lerp(0.5f, 0.66f, f), -0.05f * f);
		p.head_rot = Vector3(-0.1f * f, 0.0f, 0.0f);
		p.foot[L] = ankle_rest_[L] + Vector3(STANCE, 0.0f, 0.07f);
		p.foot[R] = ankle_rest_[R] + Vector3(-STANCE - 0.02f * (1.0f - f), 0.05f * std::sin(PI * f), lerp(-0.1f, 0.03f, f));
		p.foot_yaw[L] = TOE_OUT * 0.5f;
		p.foot_yaw[R] = lerp(-TOE_OUT - 0.4f, -TOE_OUT, f);
		p.foot_pitch[R] = 0.4f * std::sin(PI * f);
		p.hand[R] = sling(38.0f, 0.36f, 0.39f).lerp(arm_[R].root + Vector3(0.14f, -0.2f, 0.22f), f);
		p.pole[R] = arm_[R].root + Vector3(-0.45f, -0.25f, 0.05f);
		p.bone_dir = Vector3(0.0f, 0.35f, 0.95f).lerp(Vector3(0.4f, -0.5f, 0.75f), f);
		p.hand[L] = arm_[L].root + Vector3(0.1f, -0.2f, -0.2f);
		p.pole[L] = arm_[L].root + Vector3(0.3f, -0.3f, -0.3f);
		p.jaw = 0.3f * (1.0f - f) + 0.05f;
		return p;
	}

	// Recover: reach behind the back, pull out a new bone (it pops in at NEW_BONE_AT), hold it up and
	// laugh. Mixed over the walk; the legs keep going. Returns the laugh's strength.
	float recover(const AnimInput &in, Pose &p) const {
		const float t = in.state_t;
		const float reach = anim::smooth_pulse(t - 0.1f, 0.2f, 0.16f, 0.2f);
		const float tada = anim::smooth_pulse(t - NEW_BONE_AT, 0.16f, 0.5f, 0.4f);
		const float laugh = anim::smooth_pulse(t - NEW_BONE_AT - 0.08f, 0.15f, 0.9f, 0.35f);
		if (reach > 0.0f) {
			Pose r = p;
			r.hand[R] = Vector3(-0.13f, 0.02f, -0.17f);
			r.pole[R] = arm_[R].root + Vector3(-0.5f, 0.0f, -0.2f);
			r.bone_dir = Vector3(0.0f, -0.6f, -0.8f);
			r.chest_rot += Vector3(-0.05f, -0.25f, 0.0f);
			mix(p, r, reach);
		}
		if (tada > 0.0f) {
			Pose r = p;
			r.hand[R] = arm_[R].root + Vector3(-0.17f, 0.1f, 0.2f);
			r.pole[R] = arm_[R].root + Vector3(-0.4f, -0.3f, -0.1f);
			r.bone_dir = Vector3(-0.15f, 1.0f, 0.1f);
			r.chest_rot += Vector3(-0.14f, 0.12f, 0.0f);
			mix(p, r, tada);
		}
		if (laugh > 0.0f) {
			const float ha = std::sin(in.t * TAU * 7.0f);
			p.pelvis_off.y += 0.012f * laugh * ha;
			p.chest_rot.z += 0.03f * laugh * ha;
			p.head_rot += Vector3(-0.22f + 0.05f * ha, 0.0f, 0.04f * ha) * laugh;
			p.hand[L] = p.hand[L].lerp(Vector3(0.07f, 0.06f + 0.012f * ha, 0.15f), laugh * 0.9f); // a hand on the belly
			p.pole[L] = p.pole[L].lerp(arm_[L].root + Vector3(0.5f, -0.1f, -0.1f), laugh);
		}
		return laugh;
	}

	// Standing about: look round, toss the bone (posed with the bone itself), scratch the skull.
	void fidgets(const AnimInput &in, Pose &p) {
		look_away_ = false;
		if (fidget_t_ >= fidget_len()) {
			return;
		}
		const float u = fidget_t_ / fidget_len();
		if (fidget_kind_ == 0) { // a slow look to one side, then the other
			const float env = anim::smooth_pulse(fidget_t_, 0.25f, fidget_len() - 0.55f, 0.3f);
			p.head_rot += Vector3(-0.05f, 0.6f * std::sin(TAU * u), 0.1f * std::sin(TAU * u)) * env;
			p.chest_rot.y += 0.15f * std::sin(TAU * u) * env;
			look_away_ = true;
		} else if (fidget_kind_ == 1) { // flick the bone up, watch it, catch it
			const float pump = anim::pulse(fidget_t_, 0.12f, 0.2f) - 0.6f * anim::pulse(fidget_t_ - fidget_len() * 0.8f, 0.06f, 0.2f);
			p.hand[R].y += 0.05f * pump;
			p.head_rot += Vector3(-0.2f, -0.25f, 0.0f) * anim::smooth_pulse(fidget_t_, 0.2f, fidget_len() - 0.5f, 0.3f);
			look_away_ = true;
		} else { // scratch the side of the skull
			const float env = anim::smooth_pulse(fidget_t_, 0.3f, fidget_len() - 0.65f, 0.35f);
			p.hand[L] = p.hand[L].lerp(Vector3(0.31f, 0.5f + 0.02f * std::sin(in.t * TAU * 5.0f), 0.1f), env);
			p.pole[L] = p.pole[L].lerp(arm_[L].root + Vector3(0.5f, 0.0f, -0.1f), env);
			p.head_rot += Vector3(0.0f, 0.1f, -0.14f) * env;
			p.jaw += 0.06f * env;
		}
	}

	// Blends the big parts between the pose just solved and the heap. Spawning: they leap up into
	// place one after another with overshoot, feet first and the skull last. Dying: they drop and
	// bounce, legs first, the skull hopping up and landing on top last.
	void apply_pile(MonsterRig &rig, float k, bool rising) {
		const int n = int(pile_.size());
		live_.resize(n);
		for (int i = 0; i < n; ++i) {
			live_[i] = rig.model_xf(pile_[i].slot); // all read before any part is moved
		}
		for (int i = 0; i < n; ++i) { // parents first
			const Scatter &sc = pile_[i];
			const bool skull = sc.slot == head_;
			float w, hop = 0.0f;
			if (rising) {
				const float u = saturate((k - sc.rise) / 0.42f);
				w = 1.0f - anim::ease_out_back(u, skull ? 1.6f : 2.2f);
				hop = 0.12f * std::sin(PI * u);
			} else {
				const float u = saturate((k - sc.fall) / (skull ? 0.66f : 0.5f));
				w = anim::ease_out_bounce(u);
				hop = skull ? 0.08f * std::sin(PI * saturate(u * 2.5f)) : 0.0f;
			}
			Vector3 pos = sc.pos;
			Quaternion rot = sc.rot;
			if (mirror_) { // every other skeleton falls the other way
				pos.x = -pos.x;
				rot = Quaternion(rot.x, -rot.y, -rot.z, rot.w);
			}
			Transform3D want;
			want.origin = live_[i].origin.lerp(pos, Math::clamp(w, -0.3f, 1.0f)) + Vector3(0.0f, hop, 0.0f);
			want.basis = Basis(anim::slerp_safe(live_[i].basis.get_rotation_quaternion(), rot, saturate(w)));
			if (rising && skull) {
				// The skull (0.53 m wide) lies BESIDE the heap, clear of where the legs, pelvis and ribcage
				// stack up, and hops on last: up first beside the body, over the top of the ribcage, then in
				// and down onto the neck. (From the middle of the heap it lay round the pelvis and then slid
				// up through the ribcage.) Same 0.42 of the state, done by 0.86 of the rise.
				const float u = saturate((k - SKULL_RISE_AT) / 0.36f);
				const float up = anim::ease_out(saturate(u / 0.55f)); // height first
				const float across = smoothstep01(remap01(u, 0.3f, 0.85f)); // then across
				const float over = 0.17f * std::sin(PI * saturate(u / 0.85f)); // clear of the ribcage's top (0.84 m)
				// always on the +X side (the open hand): the throwing arm (-X) is raised with its bone as it stands,
				// and a skull hopping on from that side passed through it (mirrored heaps included)
				const Vector3 side = SKULL_RISE_POS;
				const Vector3 to = live_[i].origin;
				want.origin = Vector3(lerp(side.x, to.x, across), lerp(side.y, to.y, up) + over, lerp(side.z, to.z, across));
				const Quaternion lie = SKULL_RISE_ROT;
				const float settle = 0.06f * std::sin(PI * remap01(u, 0.85f, 1.0f)); // a little nod as it lands
				want.basis = Basis(anim::slerp_safe(lie, live_[i].basis.get_rotation_quaternion(), smoothstep01(remap01(u, 0.1f, 0.8f)))) *
						Basis(Vector3(1, 0, 0), settle);
			}
			const int parent = rig.parent(sc.slot);
			const Transform3D local = (parent >= 0 ? rig.model_xf(parent).affine_inverse() : Transform3D()) * want;
			const Transform3D rest = rig.rest(sc.slot);
			rig.pose(sc.slot, rest.basis.inverse() * local.basis, local.origin - rest.origin);
		}
	}

	// The eye lights: each a small soft teal light inside its dark socket. It slides inside the socket
	// to look at the player, swells and brightens through the wind-up, shrinks and dims when hit, and
	// winks out (scales to nothing) when it blinks, dies or is still rising. A Y-scale blink would
	// leave a bright line in the dark, so the scale is uniform; the socket itself never moves.
	void pose_eyes(const AnimInput &in, MonsterRig &rig, bool throwing, float laugh, bool far, float dt) {
		using State = MonsterAnimState;
		float w = throwing ? 1.0f : (look_away_ ? 0.0f : (in.chasing ? 1.0f : 0.0f));
		if (in.state == State::Rise || in.state == State::Dying || far) {
			w = 0.0f;
		}
		const Vector3 target = in.player_local + Vector3(0.0f, face_.look_height, 0.0f);
		// Flare: grows through the wind-up, held through the throw, eases off in the recover.
		float flare = 0.0f;
		if (in.state == State::Windup) {
			flare = smoothstep01(in.windup);
		} else if (in.state == State::Attack) {
			flare = 1.0f;
		}
		flare_ = far ? flare : anim::damp(flare_, flare, flare > flare_ ? 0.04f : 0.12f, dt);
		const float ouch = in.state == State::Dying || in.state == State::Rise ? 0.0f : hit_.ouch;
		const float open = 1.0f - saturate(face_.closed);
		const float size = open * (1.0f + 0.45f * flare_ + 0.12f * laugh) * (1.0f - 0.5f * ouch);
		// Low at rest so the light stays teal and the bloom only softens its edge (at 1 it clips white).
		const float glow = (0.35f + 1.15f * flare_) * (1.0f - 0.55f * ouch);
		const float k = far ? 1.0f : anim::damp_factor(0.06f, dt);
		for (int s = 0; s < 2; ++s) {
			if (socket_[s] < 0 || light_[s] < 0) {
				continue;
			}
			rig.pose(socket_[s], Basis()); // undo MonsterFace's blink squash: the socket is bone
			Vector2 want;
			if (w > 0.0f) {
				want = anim::pupil_offset(rig.model_rest(socket_[s]), target, 1.0f) * w;
			}
			// A flinch knocks the lights askew for a moment (each its own way).
			want += Vector2(0.35f * SIDE[s], 0.5f) * ouch;
			look_[s] += (want - look_[s]) * k;
			const float travel = 0.22f * rig.reach(socket_[s]); // further and the rim hides the light
			const float sc = Math::max(size, 0.001f);
			rig.pose(light_[s], Basis().scaled(Vector3(sc, sc, sc)), Vector3(look_[s].x * travel, look_[s].y * travel, 0.0f));
		}
		if (std::fabs(glow - glow_sent_) > 0.01f) {
			glow_sent_ = glow;
			rig.glow(light_[L], glow);
			rig.glow(light_[R], glow);
		}
	}

	int body_ = -1, chest_ = -1, head_ = -1, jaw_ = -1, bone_ = -1, bandana_ = -1, tail_ = -1;
	int socket_[2] = { -1, -1 }, light_[2] = { -1, -1 };
	Vector2 look_[2];
	float flare_ = 0.0f, glow_sent_ = -1000.0f;
	Limb arm_[2], leg_[2];
	Vector3 ankle_rest_[2];
	Vector3 flap_dir_, tail_dir_;
	MonsterFace face_;
	HitRecoil hit_;
	Gait gait_;
	anim::Weight w_wind_, w_attack_, w_air_;
	anim::Spring3 bobble_; // the skull's wobble: a rotation vector (pitch, yaw, roll)
	anim::Spring dip_; // landing: the pelvis sinks into the knees and comes back
	anim::Dangle flap_, tails_;
	anim::RandTimer chatter_, fidget_;
	Quaternion fist_;
	std::vector<Scatter> pile_;
	std::vector<Transform3D> live_;
	float wind_ = 0.0f, attack_t_ = 0.0f;
	float chatter_t_ = 10.0f, fidget_t_ = 10.0f, bone_pop_ = 10.0f;
	float fall_ = 0.0f, phase0_ = 0.0f;
	int fidget_kind_ = 0;
	bool have_bone_ = true, was_floor_ = true, seeded_ = false, mirror_ = false, look_away_ = false, fist_primed_ = false;
};

} // namespace

TOT_MONSTER_ANIMATOR(MonsterType::Skeleton, SkeletonAnimator)

} // namespace tot
