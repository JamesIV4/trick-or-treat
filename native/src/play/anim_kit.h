#pragma once
// Procedural animation toolkit shared by the kid rig (DragonController) and the monster animators.
// Header-only, namespace tot::anim. No allocation, no global state; every stepped helper keeps its
// state in the struct you own and gives the same motion at any frame rate from 20 to 240 fps.
//
// Conventions (the same as the art rigs):
//   - metres, seconds, radians. Y up, +Z is a character's front, +X its left.
//   - a joint pose is relative to the joint's rest transform:
//       node transform = Transform3D(rest.basis * R * S, rest.origin + pos)      (see apply_pose)
//   - Euler angles are Godot's default YXZ order, i.e. what Node3D::set_rotation takes.
//   - "hz" is a spring's natural frequency in cycles per second, "zeta" its damping ratio:
//       zeta 1 = no overshoot (follow), 0.5 = one visible overshoot, 0.2 = jelly (rings 3-4 times).
//
// Contents, in file order:
//   1. scalar helpers      saturate, lerp, remap01, wrap_angle, angle_delta, lerp_angle
//   2. easing              smoothstep01 ... ease_out_bounce, anticipate, pulse, arc, squash_stretch
//   3. rotation helpers    quat_axis_angle, quat_from_rotvec, quat_from_euler, quat_between, rotate,
//                          slerp_safe
//   4. smoothing           damp_factor, damp (float / Vector3), damp_angle, damp_quat, exp_decay
//   5. springs             Spring, SpringAngle, Spring3 (exact integration; cannot explode)
//   6. dangling parts      Dangle, DangleChain<N>: tails, ears, hat tips, capes, bandage ends, buckets
//   7. noise and timers    hash01, Rand, noise1, noise3, fbm1, fidget, RandTimer, Blink
//   8. looking             look_dir_local, look_angles, look_split, look_euler, pupil_offset, LookAt
//   9. limbs               two_bone_ik, two_bone_mid, FootPlant, gait_phase
//  10. poses               JointPose, Pose<N>, apply_pose, PoseBlender<N>, Weight, StateWeights<N>

#include <godot_cpp/variant/basis.hpp>
#include <godot_cpp/variant/quaternion.hpp>
#include <godot_cpp/variant/transform3d.hpp>
#include <godot_cpp/variant/vector2.hpp>
#include <godot_cpp/variant/vector3.hpp>

#include <cmath>
#include <cstdint>

namespace tot::anim {

using godot::Basis;
using godot::Quaternion;
using godot::Transform3D;
using godot::Vector2;
using godot::Vector3;

// ------------------------------------------------------------------------------------------------
// 1. Scalar helpers
// ------------------------------------------------------------------------------------------------

constexpr float PI = 3.14159265358979f;
constexpr float TAU = 6.28318530717959f;

// Longest step the stateful helpers will simulate. A hitch or a paused window is treated as 0.1 s.
constexpr float MAX_DT = 0.1f;

// dt clamped to [0, MAX_DT]; negative and NaN become 0.
inline float safe_dt(float dt) {
	return dt > 0.0f ? (dt < MAX_DT ? dt : MAX_DT) : 0.0f;
}

constexpr float saturate(float x) {
	return x < 0.0f ? 0.0f : (x > 1.0f ? 1.0f : x);
}

constexpr float clampf(float x, float lo, float hi) {
	return x < lo ? lo : (x > hi ? hi : x);
}

constexpr float lerp(float a, float b, float t) {
	return a + (b - a) * t;
}

// 0 at `a`, 1 at `b`, clamped outside. `a` may be greater than `b` (a falling ramp).
constexpr float remap01(float x, float a, float b) {
	return a == b ? (x >= b ? 1.0f : 0.0f) : saturate((x - a) / (b - a));
}

// Angle wrapped into [-PI, PI).
inline float wrap_angle(float a) {
	return a - TAU * std::floor((a + PI) / TAU);
}

// Shortest signed turn from `from` to `to`, in [-PI, PI).
inline float angle_delta(float from, float to) {
	return wrap_angle(to - from);
}

// Interpolates the short way round.
inline float lerp_angle(float a, float b, float t) {
	return a + angle_delta(a, b) * t;
}

// ------------------------------------------------------------------------------------------------
// 2. Easing. Every curve takes t in 0..1 (clamped) and runs 0 -> 1 unless noted.
// ------------------------------------------------------------------------------------------------

// Slow in, slow out. The default for anything that starts and ends at rest.
constexpr float smoothstep01(float t) {
	return saturate(t) * saturate(t) * (3.0f - 2.0f * saturate(t));
}

// As smoothstep01 but with zero acceleration at both ends (softer starts and stops).
constexpr float smootherstep01(float t) {
	return saturate(t) * saturate(t) * saturate(t) * (saturate(t) * (saturate(t) * 6.0f - 15.0f) + 10.0f);
}

// Starts slow, ends fast (falling, winding up).
constexpr float ease_in(float t) {
	return saturate(t) * saturate(t);
}

// Starts fast, ends slow (settling, a thrown limb coming to rest).
constexpr float ease_out(float t) {
	return 1.0f - (1.0f - saturate(t)) * (1.0f - saturate(t));
}

constexpr float ease_in_cubic(float t) {
	return saturate(t) * saturate(t) * saturate(t);
}

constexpr float ease_out_cubic(float t) {
	return 1.0f - (1.0f - saturate(t)) * (1.0f - saturate(t)) * (1.0f - saturate(t));
}

// Cubic slow-in slow-out; snappier through the middle than smoothstep01.
constexpr float ease_in_out(float t) {
	return saturate(t) < 0.5f
			? 4.0f * saturate(t) * saturate(t) * saturate(t)
			: 1.0f - 4.0f * (1.0f - saturate(t)) * (1.0f - saturate(t)) * (1.0f - saturate(t));
}

// Overshoots past 1 and settles back: pops, inflations, a cap slamming down. `s` is the amount of
// overshoot (1.70158 gives about 10 percent, 3 about 25 percent).
constexpr float ease_out_back(float t, float s = 1.70158f) {
	return 1.0f + (s + 1.0f) * (saturate(t) - 1.0f) * (saturate(t) - 1.0f) * (saturate(t) - 1.0f) + s * (saturate(t) - 1.0f) * (saturate(t) - 1.0f);
}

// Dips below 0 first, then accelerates to 1: a pull-back that is part of the move itself.
constexpr float ease_in_back(float t, float s = 1.70158f) {
	return (s + 1.0f) * saturate(t) * saturate(t) * saturate(t) - s * saturate(t) * saturate(t);
}

// Shoots past 1 and rings down (about three wobbles): jelly, springy antennae, a dizzy recover.
inline float ease_out_elastic(float t) {
	if (t <= 0.0f) {
		return 0.0f;
	}
	if (t >= 1.0f) {
		return 1.0f;
	}
	return std::pow(2.0f, -10.0f * t) * std::sin((t * 10.0f - 0.75f) * (TAU / 3.0f)) + 1.0f;
}

// Hits 1 and bounces three times, never passing it: something hard dropped on the ground.
inline float ease_out_bounce(float t) {
	t = saturate(t);
	constexpr float n = 7.5625f, d = 2.75f;
	if (t < 1.0f / d) {
		return n * t * t;
	}
	if (t < 2.0f / d) {
		t -= 1.5f / d;
		return n * t * t + 0.75f;
	}
	if (t < 2.5f / d) {
		t -= 2.25f / d;
		return n * t * t + 0.9375f;
	}
	t -= 2.625f / d;
	return n * t * t + 0.984375f;
}

// Anticipation: eases back to -`back` during the first `split` of the time, pauses there for an
// instant, then whips to 1 and settles. Use it on the main axis of a throw, pounce or slam.
inline float anticipate(float t, float back = 0.25f, float split = 0.3f) {
	t = saturate(t);
	if (split > 0.0f && t < split) {
		return -back * smoothstep01(t / split);
	}
	const float u = split < 1.0f ? (t - split) / (1.0f - split) : 1.0f;
	return -back + (1.0f + back) * ease_out_cubic(u);
}

// 0 -> 1 -> 0 envelope over time `t` in seconds from the trigger: rises for `in`, holds, falls for
// `out`, and is 0 before and after. For hit flashes, recoil, one-shot additive poses.
inline float smooth_pulse(float t, float in, float hold, float out) {
	if (t <= 0.0f) {
		return 0.0f;
	}
	if (t < in) {
		return smoothstep01(t / in);
	}
	t -= in;
	if (t <= hold) {
		return 1.0f;
	}
	t -= hold;
	return t < out ? 1.0f - smoothstep01(t / out) : 0.0f;
}

// smooth_pulse without the hold.
inline float pulse(float t, float attack, float release) {
	return smooth_pulse(t, attack, 0.0f, release);
}

// Parabolic hop: 0 at t = 0 and 1, 1 at t = 0.5. Foot lift, hop height.
constexpr float arc(float t) {
	return 4.0f * saturate(t) * (1.0f - saturate(t));
}

// Volume-preserving squash and stretch as a scale: `s` on Y, 1/sqrt(s) on X and Z.
// s < 1 squashes (0.7 is a heavy landing), s > 1 stretches (1.15 is a fast jump).
inline Vector3 squash_stretch(float s) {
	const float inv = 1.0f / std::sqrt(s > 0.05f ? s : 0.05f);
	return Vector3(inv, s, inv);
}

// ------------------------------------------------------------------------------------------------
// 3. Rotation helpers (all quaternions are unit length)
// ------------------------------------------------------------------------------------------------

// Rotation of `angle` radians about the unit vector `axis`.
inline Quaternion quat_axis_angle(const Vector3 &axis, float angle) {
	const float s = std::sin(angle * 0.5f);
	return Quaternion(axis.x * s, axis.y * s, axis.z * s, std::cos(angle * 0.5f));
}

// Rotation vector (axis * angle in radians, the format Dangle outputs) to a quaternion.
inline Quaternion quat_from_rotvec(const Vector3 &rv) {
	const float a = rv.length();
	return a < 1e-6f ? Quaternion() : quat_axis_angle(rv / a, a);
}

// Euler angles (x pitch, y yaw, z roll; YXZ order like Node3D::set_rotation) to a quaternion.
inline Quaternion quat_from_euler(const Vector3 &euler) {
	return Quaternion::from_euler(euler);
}

// `v` rotated by `q`.
inline Vector3 rotate(const Quaternion &q, const Vector3 &v) {
	const Vector3 u(q.x, q.y, q.z);
	const Vector3 uv = u.cross(v);
	return v + (uv * q.w + u.cross(uv)) * 2.0f;
}

// Some unit vector perpendicular to `n` (any length of `n` except zero).
inline Vector3 any_perp(const Vector3 &n) {
	const float ax = std::fabs(n.x), ay = std::fabs(n.y), az = std::fabs(n.z);
	const Vector3 other = (ax <= ay && ax <= az) ? Vector3(1, 0, 0) : (ay <= az ? Vector3(0, 1, 0) : Vector3(0, 0, 1));
	return n.cross(other).normalized();
}

// The part of `v` perpendicular to the unit vector `n`, normalised. Falls back to `fallback`'s
// perpendicular part, then to any perpendicular, when `v` is parallel to `n`.
inline Vector3 perp_toward(const Vector3 &n, const Vector3 &v, const Vector3 &fallback = Vector3()) {
	Vector3 p = v - n * v.dot(n);
	if (p.length_squared() < 1e-10f) {
		p = fallback - n * fallback.dot(n);
		if (p.length_squared() < 1e-10f) {
			return any_perp(n);
		}
	}
	return p.normalized();
}

// Shortest rotation taking the unit vector `from` onto the unit vector `to`.
inline Quaternion quat_between(const Vector3 &from, const Vector3 &to) {
	const float d = from.dot(to);
	if (d < -0.99999f) {
		return quat_axis_angle(any_perp(from), PI);
	}
	const Vector3 c = from.cross(to);
	const float s = std::sqrt((1.0f + d) * 2.0f);
	return Quaternion(c.x / s, c.y / s, c.z / s, s * 0.5f);
}

// Slerp that always goes the short way, falls back to a straight blend for nearly equal inputs
// and renormalises, so it is safe on quaternions that have drifted slightly off unit length.
inline Quaternion slerp_safe(const Quaternion &a, Quaternion b, float t) {
	float d = a.dot(b);
	if (d < 0.0f) {
		b = -b;
		d = -d;
	}
	float ka = 1.0f - t, kb = t;
	if (d < 0.9995f) {
		const float th = std::acos(d), s = std::sin(th);
		ka = std::sin((1.0f - t) * th) / s;
		kb = std::sin(t * th) / s;
	}
	const Quaternion q = a * ka + b * kb;
	const float len = std::sqrt(q.dot(q));
	return len > 1e-8f ? q / len : Quaternion();
}

// ------------------------------------------------------------------------------------------------
// 4. Exponential smoothing: the cheap "ease toward a value" with no velocity state. Use a spring
//    instead when the value should overshoot or must not change speed abruptly.
// ------------------------------------------------------------------------------------------------

// Fraction of the remaining distance covered in `dt` when half is covered every `half_life`
// seconds. 0.03 s is snappy, 0.1 s is soft, 0.3 s is lazy. A half life of 0 snaps.
inline float damp_factor(float half_life, float dt) {
	return half_life > 1e-6f ? 1.0f - std::exp(-0.69314718f * safe_dt(dt) / half_life) : 1.0f;
}

inline float damp(float cur, float target, float half_life, float dt) {
	return cur + (target - cur) * damp_factor(half_life, dt);
}

inline Vector3 damp(const Vector3 &cur, const Vector3 &target, float half_life, float dt) {
	return cur + (target - cur) * damp_factor(half_life, dt);
}

// damp for angles: goes the short way round.
inline float damp_angle(float cur, float target, float half_life, float dt) {
	return cur + angle_delta(cur, target) * damp_factor(half_life, dt);
}

inline Quaternion damp_quat(const Quaternion &cur, const Quaternion &target, float half_life, float dt) {
	return slerp_safe(cur, target, damp_factor(half_life, dt));
}

// The same smoothing written with a rate (per second) instead of a half life: rate = 0.693 / half_life.
inline float exp_decay(float cur, float target, float rate, float dt) {
	return cur + (target - cur) * (1.0f - std::exp(-rate * safe_dt(dt)));
}

// ------------------------------------------------------------------------------------------------
// 5. Springs. Integrated with the exact solution of the damped oscillator, so they are stable and
//    give the same curve for any dt and any stiffness.
//
//    Tuning: hz sets the speed (1-2 heavy and floppy, 3-5 lively, 8+ stiff and twitchy), zeta the
//    character (1 follows without overshoot, 0.4-0.6 one soft overshoot, 0.15-0.3 wobbles).
//    A kick of v units per second swings out by roughly v / (TAU * hz).
// ------------------------------------------------------------------------------------------------

// One step of the oscillator as a 2x2 matrix on (offset from target, velocity). Build it once and
// reuse it when many springs share hz, zeta and dt.
struct SpringCoefs {
	float xx = 1.0f, xv = 0.0f, vx = 0.0f, vv = 1.0f;
};

inline SpringCoefs spring_coefs(float hz, float zeta, float dt) {
	SpringCoefs c;
	if (!(dt > 0.0f) || !(hz > 0.0f)) {
		return c;
	}
	const float w = TAU * hz;
	const float z = zeta > 0.0f ? zeta : 0.0f;
	if (z < 0.999f) { // underdamped: rings
		const float wd = w * std::sqrt(1.0f - z * z), a = z * w;
		const float e = std::exp(-a * dt), cs = std::cos(wd * dt), sn = std::sin(wd * dt);
		c.xx = e * (cs + a / wd * sn);
		c.xv = e * sn / wd;
		c.vx = -e * (w * w / wd) * sn;
		c.vv = e * (cs - a / wd * sn);
	} else if (z < 1.001f) { // critically damped: fastest approach without overshoot
		const float e = std::exp(-w * dt);
		c.xx = e * (1.0f + w * dt);
		c.xv = e * dt;
		c.vx = -e * w * w * dt;
		c.vv = e * (1.0f - w * dt);
	} else { // overdamped: sluggish
		const float s = w * std::sqrt(z * z - 1.0f);
		const float r1 = -z * w + s, r2 = -z * w - s;
		const float e1 = std::exp(r1 * dt), e2 = std::exp(r2 * dt), inv = 1.0f / (r1 - r2);
		c.xx = (r1 * e2 - r2 * e1) * inv;
		c.xv = (e1 - e2) * inv;
		c.vx = r1 * r2 * (e2 - e1) * inv;
		c.vv = (r1 * e1 - r2 * e2) * inv;
	}
	return c;
}

// Conversions for springs specified as stiffness k and damping d per unit mass (x'' = -k x - d x').
inline float hz_from_k(float k) {
	return std::sqrt(k > 0.0f ? k : 0.0f) / TAU;
}

inline float zeta_from_kd(float k, float d) {
	return k > 0.0f ? d / (2.0f * std::sqrt(k)) : 1.0f;
}

inline SpringCoefs spring_coefs_kd(float k, float d, float dt) {
	return spring_coefs(hz_from_k(k), zeta_from_kd(k, d), dt);
}

// Scalar spring.
struct Spring {
	float x = 0.0f, v = 0.0f;

	float step(float target, const SpringCoefs &c) {
		const float e = x - target;
		x = target + c.xx * e + c.xv * v;
		v = c.vx * e + c.vv * v;
		return x;
	}
	// Underdamped jiggle or damped follow, depending on zeta.
	float step(float target, float hz, float zeta, float dt) { return step(target, spring_coefs(hz, zeta, dt)); }
	// Critically damped follow that needs only a time: about 60 percent there after `smooth_time`,
	// 95 percent after 2.4 times that. Never overshoots a still target.
	float follow(float target, float smooth_time, float dt) {
		if (smooth_time <= 1e-5f) {
			reset(target);
			return x;
		}
		return step(target, spring_coefs(1.0f / (PI * smooth_time), 1.0f, dt));
	}
	// Adds velocity: a hit, a landing, a flick.
	void kick(float dv) { v += dv; }
	void reset(float x0 = 0.0f) {
		x = x0;
		v = 0.0f;
	}
};

// Scalar spring on an angle: chases the target the short way round and keeps x in [-PI, PI).
struct SpringAngle {
	float x = 0.0f, v = 0.0f;

	float step(float target, const SpringCoefs &c) {
		const float e = angle_delta(target, x);
		x = wrap_angle(target + c.xx * e + c.xv * v);
		v = c.vx * e + c.vv * v;
		return x;
	}
	float step(float target, float hz, float zeta, float dt) { return step(target, spring_coefs(hz, zeta, dt)); }
	float follow(float target, float smooth_time, float dt) {
		if (smooth_time <= 1e-5f) {
			reset(target);
			return x;
		}
		return step(target, spring_coefs(1.0f / (PI * smooth_time), 1.0f, dt));
	}
	void kick(float dv) { v += dv; }
	void reset(float x0 = 0.0f) {
		x = wrap_angle(x0);
		v = 0.0f;
	}
};

// Vector spring (positions, offsets, Euler triples that stay well inside +-PI).
struct Spring3 {
	Vector3 x, v;

	// (These return references so the result may be ignored: godot::Vector3 is [[nodiscard]].)
	const Vector3 &step(const Vector3 &target, const SpringCoefs &c) {
		const Vector3 e = x - target;
		x = target + e * c.xx + v * c.xv;
		v = e * c.vx + v * c.vv;
		return x;
	}
	const Vector3 &step(const Vector3 &target, float hz, float zeta, float dt) { return step(target, spring_coefs(hz, zeta, dt)); }
	const Vector3 &follow(const Vector3 &target, float smooth_time, float dt) {
		if (smooth_time <= 1e-5f) {
			reset(target);
			return x;
		}
		return step(target, spring_coefs(1.0f / (PI * smooth_time), 1.0f, dt));
	}
	void kick(const Vector3 &dv) { v += dv; }
	void reset(const Vector3 &x0 = Vector3()) {
		x = x0;
		v = Vector3();
	}
};

// ------------------------------------------------------------------------------------------------
// 6. Dangling parts: one two-axis spring per floppy part. The part is thrown about by the motion
//    of whatever it is pinned to (start, stop, turn, land), pulled by gravity and drag, and returns
//    to its authored rest pose. The result is a rotation vector in the part's own space, which feeds
//      - a shader bend on one continuous mesh (soft_bend / bend uniforms: tail, cape, hair, hem), or
//      - a rigid swing of the node about its origin (bell, bucket, hat): quat_from_rotvec(bend).
// ------------------------------------------------------------------------------------------------

struct DangleParams {
	float len = 0.2f; // pin-to-tip distance in the part's local units (the monster RigPart reach)
	float hz = 2.0f; // return-to-rest frequency: 1-1.5 cape and sheet, 2-3 hair and tails, 4-6 ears and stems
	float zeta = 0.35f; // 0.2 keeps swinging, 0.5 settles after one overshoot, 1 just lags
	float gravity = 0.0f; // m/s^2 of world-down pull on the tip: 0 holds the authored shape; a part
	//                       sticking out sideways sags by gravity / (len * (TAU * hz)^2) radians
	float drag = 0.0f; // 0..1 air drag: above 0 the tip also trails behind steady movement (try 0.1-0.3)
	float inertia = 1.0f; // how hard the parent's movement throws it; lower for stiff or tiny parts
	float limit = 1.0f; // largest swing away from rest, radians
};

struct Dangle {
	Vector3 bend; // state and output: rotation vector (axis * radians) in the part's local space
	Vector3 rate; // its rate of change, radians per second
	Vector3 rest_tip, parent_vel; // where the rest tip was last step (world), and its smoothed velocity
	Vector3 pending_kick; // world-space kick waiting for the next step
	bool primed = false;

	// Forget all motion (teleport, costume swap, respawn). The next step starts from rest. A jump
	// of more than 4 m in one step is also treated as a teleport, but keeps the current bend.
	void reset() {
		bend = rate = pending_kick = Vector3();
		primed = false;
	}
	// Hold a given bend with no motion (roll, spin). Stepping again resumes smoothly from it.
	void freeze(const Vector3 &bend_local) {
		bend = bend_local;
		rate = pending_kick = Vector3();
		primed = false;
	}
	// Throw the tip, world-space metres per second (hit reactions, a flick fidget).
	void kick(const Vector3 &dv_world) { pending_kick += dv_world; }
	// Local-space displacement of the tip from rest, for shaders that take a tip offset.
	Vector3 tip_offset(const Vector3 &rest_dir, float len) const {
		const Vector3 r = rest_dir.normalized();
		return (rotate(quat_from_rotvec(bend), r) - r) * len;
	}

	// frame:    world transform of the part's REST frame (the parent's global transform times the
	//           part's rest transform). Do not pass the node's own global transform if this
	//           result rotates that node, or it will feed back on itself.
	//           For a part that should hang straight down whatever its parent does (a bucket), pass
	//           Transform3D(Basis(), grip_world) with rest_dir (0,-1,0); the result is then a
	//           world-space rotation vector and hz is the pendulum's swing rate (about 1.5).
	// rest_dir: direction from the pin to the tip in the part's local space (tail (0,0,-1), ear (0,1,0)).
	// accel:    extra world-space acceleration on the tip in m/s^2: wind, the updraft of a fall
	//           (0, -vy * 2, 0), a push outward during a spin.
	// Returns `bend`. (A small-swing model: forces along the part's rest direction do nothing.)
	const Vector3 &step(const Transform3D &frame, const Vector3 &rest_dir, const DangleParams &p, float dt, const Vector3 &accel = Vector3()) {
		dt = safe_dt(dt);
		const Vector3 r = rest_dir.normalized();
		const Vector3 rv = frame.basis.xform(r * p.len);
		const float len = rv.length();
		if (dt <= 0.0f || len < 1e-6f) {
			return bend;
		}
		const Vector3 rhat = rv / len;
		const Vector3 tip = frame.origin + rv;
		// A world-space movement u of the tip is a rotation of (rhat x u) / len about the pin. This
		// turns it into the part's local axes (the frame may carry a uniform scale).
		const float to_local_scale = p.len / (len * len);
		const auto to_local = [&](const Vector3 &u) { return frame.basis.xform_inv(rhat.cross(u)) * to_local_scale; };

		const Vector3 moved = primed ? tip - rest_tip : Vector3();
		if (!primed || moved.length_squared() > 16.0f) { // first step, or a teleport (4 m in one step)
			if (primed) {
				rate = Vector3();
			}
			parent_vel = Vector3();
			primed = true;
		} else {
			// The parent's motion reaches the tip two ways: a change of speed throws it (the tip keeps
			// its momentum), and any movement the smoothed speed does not account for shifts the pin
			// under it. The speed is smoothed because a body moved by physics ticks advances in hops
			// whenever the game renders faster than it ticks; raw per-frame speed would be all spikes.
			const Vector3 pv = damp(parent_vel, moved / dt, 0.03f, dt);
			rate -= to_local(pv - parent_vel) * p.inertia;
			bend -= to_local(moved - pv * dt) * p.inertia;
			parent_vel = pv;
		}
		rest_tip = tip;
		rate += to_local(pending_kick);
		pending_kick = Vector3();

		if (p.hz > 0.0f) {
			const float w = TAU * p.hz;
			const Vector3 eq = to_local(Vector3(0.0f, -p.gravity, 0.0f) + accel - parent_vel * (2.0f * p.zeta * w * p.drag)) / (w * w);
			const SpringCoefs c = spring_coefs(p.hz, p.zeta, dt);
			const Vector3 e = bend - eq;
			bend = eq + e * c.xx + rate * c.xv;
			rate = e * c.vx + rate * c.vv;
		} else {
			bend = rate = Vector3();
		}

		// Bends are about axes square to the part (no twist) and no larger than the limit.
		bend -= r * bend.dot(r);
		rate -= r * rate.dot(r);
		const float ang = bend.length(), limit = clampf(p.limit, 0.0f, PI * 0.95f);
		if (ang > limit) {
			const Vector3 n = bend / ang;
			bend = n * limit;
			const float out = rate.dot(n);
			if (out > 0.0f) {
				rate -= n * out;
			}
		}
		return bend;
	}
};

// A chain of N dangles for nested rigid parts (stem > leaf, hat > hat tip, wing > wingtip), each
// pinned to the tip of the one before. Not for subdividing one floppy mesh into segments: that
// reads as segments. Use a single Dangle and a shader bend for those.
template <int N>
struct DangleChain {
	Dangle link[N];

	void reset() {
		for (int i = 0; i < N; ++i) {
			link[i].reset();
		}
	}
	void kick(const Vector3 &dv_world) {
		for (int i = 0; i < N; ++i) {
			link[i].kick(dv_world);
		}
	}
	// root:     world rest frame of link 0 (as for Dangle::step).
	// offsets:  N entries; offsets[i] is link i's origin in link i-1's local space (offsets[0] unused).
	// rest_dir: pin-to-tip direction of the last link; inner links point at the next link's origin.
	// p:        shared tuning; p.len is the last link's length, inner lengths come from the offsets.
	// soften:   each link's hz is the previous link's times this (below 1 = floppier toward the tip).
	// out:      N local rotations, one per link: node basis = rest basis * Basis(out[i]).
	void step(const Transform3D &root, const Vector3 *offsets, const Vector3 &rest_dir, DangleParams p, float dt, Quaternion *out, const Vector3 &accel = Vector3(), float soften = 0.85f) {
		Transform3D frame = root;
		const float tip_len = p.len;
		for (int i = 0; i < N; ++i) {
			if (i > 0) {
				frame.basis = frame.basis * Basis(out[i - 1]);
				frame.origin = frame.xform(offsets[i]);
				p.hz *= soften;
			}
			Vector3 dir = rest_dir;
			p.len = tip_len;
			if (i + 1 < N && offsets[i + 1].length_squared() > 1e-8f) {
				dir = offsets[i + 1];
				p.len = dir.length();
			}
			out[i] = quat_from_rotvec(link[i].step(frame, dir, p, dt, accel));
		}
	}
};

// ------------------------------------------------------------------------------------------------
// 7. Noise and timers
// ------------------------------------------------------------------------------------------------

inline uint32_t hash32(uint32_t x) {
	x ^= x >> 16;
	x *= 0x7feb352dU;
	x ^= x >> 15;
	x *= 0x846ca68bU;
	x ^= x >> 16;
	return x;
}

// Hash to [0, 1).
inline float hash01(uint32_t x) {
	return float(hash32(x) >> 8) * (1.0f / 16777216.0f);
}

// Tiny random stream for animation variety (not for gameplay: use tot::Rng there). Seed it with
// something per character to make it repeatable; left unseeded it seeds itself from its own
// address on first use, so two characters never blink or fidget in step.
struct Rand {
	uint32_t s = 0;

	Rand() = default;
	explicit Rand(uint32_t seed) :
			s(hash32(seed) | 1u) {}
	uint32_t next() {
		if (s == 0) {
			const uint64_t p = uint64_t(reinterpret_cast<uintptr_t>(this));
			s = hash32(uint32_t(p) ^ uint32_t(p >> 32)) | 1u;
		}
		s ^= s << 13;
		s ^= s >> 17;
		s ^= s << 5;
		return hash32(s);
	}
	float f() { return float(next() >> 8) * (1.0f / 16777216.0f); } // [0, 1)
	float range(float a, float b) { return a + (b - a) * f(); }
	bool chance(float p) { return f() < p; }
	int index(int n) { return n > 1 ? int(next() % uint32_t(n)) : 0; } // 0 .. n-1
};

// Smooth value noise in [-1, 1], one feature per unit of `t`: pass time * rate. Different seeds give
// unrelated curves. For slow drifting things: sway, a wandering gaze, candle flicker (rate 8-12).
inline float noise1(float t, uint32_t seed) {
	const float fl = std::floor(t);
	const uint32_t i = uint32_t(int32_t(fl)), h = hash32(seed + 0x9E3779B9u);
	const float a = hash01(i + h), b = hash01(i + 1u + h);
	return lerp(a, b, smootherstep01(t - fl)) * 2.0f - 1.0f;
}

// Three unrelated noise1 channels.
inline Vector3 noise3(float t, uint32_t seed) {
	return Vector3(noise1(t, seed), noise1(t + 37.17f, seed + 0x68E31DA4u), noise1(t + 71.63f, seed + 0xB5297A4Du));
}

// Layered noise1 (each octave twice as fast and half as strong), in [-1, 1].
inline float fbm1(float t, uint32_t seed, int octaves = 3) {
	float sum = 0.0f, amp = 1.0f, norm = 0.0f, f = 1.0f;
	for (int o = 0; o < octaves; ++o) {
		sum += noise1(t * f, seed + uint32_t(o) * 101u) * amp;
		norm += amp;
		amp *= 0.5f;
		f *= 2.0f;
	}
	return norm > 0.0f ? sum / norm : 0.0f;
}

// Three layered sines with seed-dependent phases and unrelated frequencies, in [-1, 1]: a cheap
// organic wobble that never visibly loops. `t` is in cycles of the slowest layer (time * hz).
// Breathing drift, weight shift, idle sway, tendril curl; one seed per joint axis.
inline float fidget(float t, uint32_t seed) {
	const float f1 = 1.7f + 0.6f * hash01(seed + 3u), f2 = 3.1f + 1.1f * hash01(seed + 4u);
	return 0.6f * std::sin(TAU * (t + hash01(seed))) + 0.3f * std::sin(TAU * (t * f1 + hash01(seed + 1u))) + 0.1f * std::sin(TAU * (t * f2 + hash01(seed + 2u)));
}

// Fires at random intervals: when to start a fidget, a jaw chatter, a weight shift.
struct RandTimer {
	Rand rng;
	float left = -1.0f; // seconds until the next fire; negative = not started
	int count = 0; // fires so far (use it to cycle through variations)

	RandTimer() = default;
	explicit RandTimer(uint32_t seed) :
			rng(seed) {}
	// True on the step it fires. The first wait is random in 0..max_gap so a crowd starts staggered.
	bool step(float dt, float min_gap, float max_gap) {
		if (left < 0.0f) {
			left = rng.range(0.0f, max_gap);
		}
		left -= safe_dt(dt);
		if (left > 0.0f) {
			return false;
		}
		left = rng.range(min_gap, max_gap);
		if (left < 0.0f) {
			left = 0.0f;
		}
		++count;
		return true;
	}
	void reset() { left = -1.0f; }
};

// Eyelids. step() returns how closed the eyes are, 0 (open) to 1 (shut), eased.
// Typical use: eye scale Y = lerp(1, 0.1, closed); show the closed-eye lash arc when closed > 0.6.
struct Blink {
	// Tuning (seconds). Defaults suit the kids; monsters read better with 1.8 / 5.0 gaps.
	float min_gap = 2.2f, max_gap = 5.5f; // wait between blinks
	float close_time = 0.09f, hold_time = 0.03f, open_time = 0.14f;
	float double_chance = 0.15f; // chance that a blink is followed at once by a second one

	Rand rng;
	float closed = 0.0f; // last value returned by step()

	Blink() = default;
	explicit Blink(uint32_t seed) :
			rng(seed) {}

	float step(float dt) {
		dt = safe_dt(dt);
		if (suppress_ > 0.0f) {
			suppress_ -= dt;
		}
		switch (phase_) {
			case WAIT:
				if (gap_ < 0.0f) {
					gap_ = rng.range(0.3f, max_gap);
				}
				gap_ = gap_ > dt ? gap_ - dt : 0.0f;
				if (gap_ <= 0.0f && suppress_ <= 0.0f) {
					phase_ = CLOSING;
				}
				break;
			case CLOSING:
				raw_ += close_time > 1e-4f ? dt / close_time : 1.0f;
				if (raw_ >= 1.0f) {
					raw_ = 1.0f;
					phase_ = HELD;
					t_ = hold_time + extra_hold_;
					extra_hold_ = 0.0f;
				}
				break;
			case HELD:
				t_ -= dt;
				if (t_ <= 0.0f) {
					phase_ = OPENING;
				}
				break;
			case OPENING:
				raw_ -= open_time > 1e-4f ? dt / open_time : 1.0f;
				if (raw_ <= 0.0f) {
					raw_ = 0.0f;
					phase_ = WAIT;
					second_ = !second_ && rng.chance(double_chance);
					gap_ = second_ ? rng.range(0.06f, 0.14f) : rng.range(min_gap, max_gap);
				}
				break;
		}
		closed = smoothstep01(raw_);
		return closed;
	}
	// Blink now (hard landing, hurt, cast) and stay shut for `hold` extra seconds (squeezed eyes).
	void force(float hold = 0.0f) {
		if (phase_ == HELD) {
			t_ = t_ > hold_time + hold ? t_ : hold_time + hold;
		} else {
			phase_ = CLOSING;
			extra_hold_ = hold;
		}
	}
	// No automatic blinks for `seconds` (wide-eyed at a jump apex, a stare). force() still works.
	void hold_open(float seconds) { suppress_ = seconds; }
	bool is_blinking() const { return phase_ != WAIT; }
	void reset() {
		phase_ = WAIT;
		raw_ = closed = 0.0f;
		gap_ = -1.0f;
		extra_hold_ = suppress_ = 0.0f;
		second_ = false;
	}

private:
	enum Phase { WAIT, CLOSING, HELD, OPENING };
	Phase phase_ = WAIT;
	float raw_ = 0.0f, t_ = 0.0f, gap_ = -1.0f, extra_hold_ = 0.0f, suppress_ = 0.0f;
	bool second_ = false;
};

// ------------------------------------------------------------------------------------------------
// 8. Looking. Angles are returned as Vector2(yaw, pitch): yaw about +Y (positive turns the front
//    toward +X), pitch about +X (NEGATIVE looks up, as a Godot rotation.x does). Apply them with
//    look_euler(): node rotation = (pitch, yaw, 0).
// ------------------------------------------------------------------------------------------------

// Direction from a pivot to a world point, in the space the pivot's node is parented in.
// parent_world: global transform of the looking part's parent; pivot_local: the part's rest origin.
inline Vector3 look_dir_local(const Transform3D &parent_world, const Vector3 &pivot_local, const Vector3 &target_world) {
	return parent_world.affine_inverse().xform(target_world) - pivot_local;
}

// Yaw and pitch that aim +Z along `dir_local`, each clamped to +-max. A target behind the part
// clamps to the nearer side.
inline Vector2 look_angles(const Vector3 &dir_local, float max_yaw, float max_pitch) {
	const float flat = std::sqrt(dir_local.x * dir_local.x + dir_local.z * dir_local.z);
	if (flat < 1e-6f && std::fabs(dir_local.y) < 1e-6f) {
		return Vector2();
	}
	const float yaw = flat < 1e-6f ? 0.0f : std::atan2(dir_local.x, dir_local.z);
	const float pitch = std::atan2(-dir_local.y, flat);
	return Vector2(clampf(yaw, -max_yaw, max_yaw), clampf(pitch, -max_pitch, max_pitch));
}

inline Vector2 look_angles(const Transform3D &parent_world, const Vector3 &pivot_local, const Vector3 &target_world, float max_yaw, float max_pitch) {
	return look_angles(look_dir_local(parent_world, pivot_local, target_world), max_yaw, max_pitch);
}

// Splits a look between the eyes and the head: the eyes take the first +-eye_max, the head the rest.
inline void look_split(const Vector2 &total, float eye_max, Vector2 &eyes, Vector2 &head) {
	eyes = Vector2(clampf(total.x, -eye_max, eye_max), clampf(total.y, -eye_max, eye_max));
	head = total - eyes;
}

// (yaw, pitch) as the Euler triple for Node3D::set_rotation or JointPose::euler.
inline Vector3 look_euler(const Vector2 &yaw_pitch) {
	return Vector3(yaw_pitch.y, yaw_pitch.x, 0.0f);
}

// How far to slide a pupil across its eye toward a world point. eye_world is the eye's global
// transform (local +Z out of the face, +Y up); the result is an offset in the eye's local XY, at
// most max_r long. `gain` above 1 reaches the rim sooner, which reads better on small eyes.
inline Vector2 pupil_offset(const Transform3D &eye_world, const Vector3 &target_world, float max_r, float gain = 1.5f) {
	const Vector3 local = eye_world.affine_inverse().xform(target_world);
	const float len = local.length();
	if (len < 1e-6f) {
		return Vector2();
	}
	Vector2 o(local.x / len * gain, local.y / len * gain);
	const float ol = o.length();
	if (local.z < 0.0f && ol > 1e-6f) {
		o /= ol; // behind the eye: all the way to the rim
	} else if (ol > 1.0f) {
		o /= ol;
	}
	return o * max_r;
}

// Smoothed head or eye tracking. Call every frame; pass weight 0 (or any direction) to return to
// centre. Targets behind the part (beyond about 130 degrees) fade out instead of snapping the
// head from one shoulder to the other.
struct LookAt {
	Spring yaw, pitch;
	Vector2 ang; // output: smoothed (yaw, pitch)

	// dir_local: look_dir_local(...). smooth_time: 0.08 darting eyes, 0.15-0.25 a head.
	// Returns ng.
	const Vector2 &step(const Vector3 &dir_local, float max_yaw, float max_pitch, float weight, float smooth_time, float dt) {
		Vector2 want;
		if (weight > 0.0f && dir_local.length_squared() > 1e-10f) {
			const float raw_yaw = std::atan2(dir_local.x, dir_local.z);
			const float front = 1.0f - smoothstep01(remap01(std::fabs(raw_yaw), 2.2f, 2.9f));
			want = look_angles(dir_local, max_yaw, max_pitch) * (saturate(weight) * front);
		}
		ang = Vector2(yaw.follow(want.x, smooth_time, dt), pitch.follow(want.y, smooth_time, dt));
		return ang;
	}
	void reset() {
		yaw.reset();
		pitch.reset();
		ang = Vector2();
	}
};

// ------------------------------------------------------------------------------------------------
// 9. Limbs
// ------------------------------------------------------------------------------------------------

struct Ik2 {
	Quaternion upper; // rotation for the upper joint (shoulder, hip)
	Quaternion lower; // rotation for the mid joint (elbow, knee): `hinge` radians about the hinge axis
	float hinge = 0.0f; // mid joint angle relative to the rest pose (rest = 0)
	Vector3 mid, end; // solved mid joint and end positions, same space as the inputs
	bool reached = false; // false if the target was out of range; the limb then points at it
};

// Analytic two-bone IK for a limb of nested joints: upper > mid > end (arm > forearm > hand).
// Everything is in the space the upper joint is parented in, and the limb joints are assumed to
// have no rest rotation (both rig contracts use translation-only limb joints), so the results go
// straight into the nodes or into JointPose::rot.
//   root:       the upper joint's rest origin.
//   a:          the mid joint's rest origin in the upper joint's space (forearm's translation).
//   b:          the end's rest origin in the mid joint's space (hand's translation).
//   hinge_axis: the axis the mid joint bends about, in limb space. Positive rotation is "flex".
//               With +Z front, (1,0,0) folds a hanging leg's shin backward (knee forward) and
//               (-1,0,0) folds a hanging arm's forearm forward. Pass Vector3() to use the plane
//               of the rest pose.
//   target:     where the end should go.
//   pole:       a point the mid joint should bend toward (in front of a knee, behind an elbow).
//   max_flex:   the most the mid joint may fold from full extension, radians.
// The limb never over-extends or folds past max_flex: out-of-range targets give the nearest pose.
// Convert a world target with parent_global.affine_inverse().xform(target_world).
inline Ik2 two_bone_ik(const Vector3 &root, const Vector3 &a, const Vector3 &b, const Vector3 &hinge_axis, const Vector3 &target, const Vector3 &pole, float max_flex = 2.7f) {
	Ik2 out;
	const float l1 = a.length(), l2 = b.length();
	out.mid = root + a;
	out.end = out.mid + b;
	if (l1 < 1e-6f || l2 < 1e-6f) {
		return out;
	}
	Vector3 h = hinge_axis;
	if (h.length_squared() < 1e-10f) {
		h = a.cross(b);
		if (h.length_squared() < 1e-8f * l1 * l1 * l2 * l2) {
			h = any_perp(a);
		}
	}
	h = h.normalized();

	// Mid joint: rotating b about h by phi makes |a + R b| equal the distance to the target when
	// A cos(phi) + B sin(phi) = C.
	const Vector3 to = target - root;
	const float dist = to.length();
	const Vector3 b_par = h * b.dot(h), b_perp = b - b_par, b_side = h.cross(b_perp);
	const float A = a.dot(b_perp), B = a.dot(b_side), R = std::sqrt(A * A + B * B);
	const float C = (dist * dist - l1 * l1 - l2 * l2) * 0.5f - a.dot(b_par);
	float q = R > 1e-9f ? C / R : 1.0f;
	out.reached = q <= 1.0f && q >= -1.0f;
	q = clampf(q, -1.0f, 1.0f);
	float flex = std::acos(q);
	if (flex > max_flex) {
		flex = max_flex;
		out.reached = false;
	}
	const float phi = std::atan2(B, A) + flex;
	out.hinge = wrap_angle(phi);
	out.lower = quat_axis_angle(h, out.hinge);

	// Upper joint: swing the root-to-end chord onto the target direction, then twist about it so
	// the mid joint sits on the pole side.
	const Vector3 chord = a + b_par + b_perp * std::cos(phi) + b_side * std::sin(phi);
	const float cl = chord.length();
	if (cl < 1e-6f) {
		return out;
	}
	const Vector3 chat = chord / cl;
	const Vector3 n = dist > 1e-6f ? to / dist : chat;
	const Vector3 w = perp_toward(chat, a, -b_side); // where the mid joint sticks out, rest space
	const Quaternion swing = quat_between(chat, n);
	const Vector3 w1 = rotate(swing, w);
	const Vector3 p = perp_toward(n, pole - root, w1);
	const float twist = std::atan2(n.dot(w1.cross(p)), w1.dot(p));
	out.upper = quat_axis_angle(n, twist) * swing;
	const float ul = std::sqrt(out.upper.dot(out.upper));
	out.upper = ul > 1e-8f ? out.upper / ul : Quaternion();
	out.mid = root + rotate(out.upper, a);
	out.end = root + n * cl;
	return out;
}

// Position-only two-bone solve for limbs you orient yourself (straight segments of length l1 and
// l2): returns where the mid joint goes, bent toward `pole`. The reach is clamped.
inline Vector3 two_bone_mid(const Vector3 &root, const Vector3 &target, const Vector3 &pole, float l1, float l2) {
	const Vector3 to = target - root;
	const float raw = to.length();
	const Vector3 n = raw > 1e-6f ? to / raw : Vector3(0, -1, 0);
	const float d = clampf(raw, std::fabs(l1 - l2) + 1e-4f, l1 + l2 - 1e-4f);
	const float x = (l1 * l1 - l2 * l2 + d * d) / (2.0f * d);
	const float hh = l1 * l1 - x * x;
	return root + n * x + perp_toward(n, pole - root) * std::sqrt(hh > 0.0f ? hh : 0.0f);
}

// Advances a walk cycle by distance travelled, so feet do not slide: one full cycle (a left and a
// right step) per `stride` metres. `phase` is kept in [0, 1) and returned.
inline float gait_phase(float &phase, float speed, float stride, float dt) {
	if (stride > 1e-5f) {
		phase += speed / stride * safe_dt(dt);
		phase -= std::floor(phase);
	}
	return phase;
}

// One planted foot. It stays put in the world while the body moves, and when its home (where the
// foot would be if the leg were at rest, usually dropped onto the ground) has drifted more than
// half a stride away it swings to a spot ahead of the home.
struct FootPlant {
	Vector3 pos; // current planted position, world
	Vector3 from, prev_home, home_vel;
	float t = 1.0f; // swing progress; 1 = planted
	bool primed = false;

	bool stepping() const { return t < 1.0f; }
	void reset() {
		primed = false;
		t = 1.0f;
	}
	// home_world: rest position of the foot under the moving body.
	// stride:     distance the body covers between two steps of this foot.
	// dur:        swing time, seconds (0.08-0.15 scuttling, 0.2-0.3 walking). Keep speed * dur below
	//             half a stride (shorten dur as the body speeds up) or the feet fall behind.
	// lift:       height of the swing arc.
	// may_lift:   pass false while the neighbouring feet are in the air, so groups alternate.
	// out_world:  where the foot is now (feed it to the leg IK). Returns true on the step it lands.
	bool step(const Vector3 &home_world, float stride, float dur, float lift, float dt, Vector3 &out_world, bool may_lift = true) {
		dt = safe_dt(dt);
		if (!primed || (home_world - pos).length_squared() > 16.0f * stride * stride) {
			pos = prev_home = home_world;
			home_vel = Vector3();
			t = 1.0f;
			primed = true;
		}
		if (dt > 0.0f) { // smoothed, because a body moved by physics ticks advances in hops when rendering is faster
			home_vel = damp(home_vel, (home_world - prev_home) / dt, 0.04f, dt);
			home_vel.y = 0.0f;
		}
		prev_home = home_world;
		const Vector3 hv = home_vel;
		const float speed = hv.length();
		bool landed = false, lifted = false;
		if (t >= 1.0f && may_lift) {
			Vector3 gap = home_world - pos;
			gap.y = 0.0f;
			const float g = gap.length();
			if (g > 0.5f * stride) {
				from = pos;
				// Credit the part of this frame that came after the foot should have lifted, so
				// the step rate does not depend on the frame rate.
				t = speed * dur > 1e-6f ? clampf((g - 0.5f * stride) / (speed * dur), 0.0f, 0.5f) : 0.0f;
				lifted = true;
			}
		}
		if (t < 1.0f) {
			if (!lifted) {
				t += dur > 1e-4f ? dt / dur : 1.0f;
			}
			// Land ahead of the home so the body walks over the foot: far enough that the next lift
			// comes one stride after this one, less when creeping (under two strides a second) and
			// not at all when stopped.
			Vector3 lead;
			if (speed > 1e-5f) {
				const float reach = clampf(0.5f * stride - speed * dur, 0.0f, 0.45f * stride) * saturate(speed / (2.0f * stride));
				lead = hv * (reach / speed);
			}
			const Vector3 to = home_world + lead;
			if (t >= 0.999f) {
				t = 1.0f;
				pos = to;
				landed = true;
			} else {
				pos = from.lerp(to, smoothstep01(t));
				pos.y += lift * arc(t);
			}
		}
		out_world = pos;
		return landed;
	}
};

// ------------------------------------------------------------------------------------------------
// 10. Poses and blending
// ------------------------------------------------------------------------------------------------

// One joint's pose relative to its rest transform.
struct JointPose {
	Quaternion rot; // local rotation applied after the rest rotation
	Vector3 pos; // offset added to the rest origin (parent space)
	Vector3 scale = Vector3(1, 1, 1);

	JointPose() = default;
	JointPose(const Quaternion &r, const Vector3 &p = Vector3(), const Vector3 &s = Vector3(1, 1, 1)) :
			rot(r), pos(p), scale(s) {}
	// From Euler angles (x pitch, y yaw, z roll; YXZ like Node3D::set_rotation).
	static JointPose euler(const Vector3 &e, const Vector3 &p = Vector3(), const Vector3 &s = Vector3(1, 1, 1)) {
		return JointPose(quat_from_euler(e), p, s);
	}
};

// The node transform for a joint: Transform3D(rest.basis * R * S, rest.origin + pos).
inline Transform3D apply_pose(const Transform3D &rest, const JointPose &p) {
	return Transform3D(rest.basis * Basis(p.rot, p.scale), rest.origin + p.pos);
}

// A whole-character pose: N joints indexed by the rig's own enum. Default = the rest pose.
template <int N>
struct Pose {
	JointPose j[N];
	JointPose &operator[](int i) { return j[i]; }
	const JointPose &operator[](int i) const { return j[i]; }
};

// Builds the frame's final pose from layers. Call order each frame:
//   begin();
//   blend(pose, w)          any number: the base set (idle, walk, run, rise, fall ...). Weights are
//                           normalised, so they need not sum to 1; all-zero gives the rest pose.
//   override_with(pose, w)  then, in priority order: replaces the result so far by fraction w (roll,
//                           hurt, hang; with a mask: upper-body cast or flap).
//   additive(pose, w)       then: stacks a small pose on top (breath, lean, look-at, landing dip).
//   result()
// Rotations in the base set are averaged on the quaternion sphere (sign-corrected, renormalised),
// which is accurate while a joint's layers are within about 120 degrees of each other; overrides
// use a true slerp. `mask` has bit i set for each joint the layer may touch. `name` is optional and
// only feeds the debug readout (a rig gym label).
template <int N>
class PoseBlender {
public:
	static constexpr int MAX_LAYERS = 16;
	static constexpr uint64_t ALL = ~uint64_t(0);

	void begin() {
		static_assert(N <= 64, "PoseBlender masks hold 64 joints");
		for (int i = 0; i < N; ++i) {
			acc_.j[i].rot = Quaternion(0, 0, 0, 0);
			acc_.j[i].pos = Vector3();
			acc_.j[i].scale = Vector3();
		}
		total_ = 0.0f;
		settled_ = false;
		layers_ = 0;
	}

	void blend(const Pose<N> &p, float w, const char *name = nullptr) {
		if (w <= 0.0f) {
			return;
		}
		if (settled_) { // called out of order: do the next best thing
			override_with(p, saturate(w), ALL, name);
			return;
		}
		for (int i = 0; i < N; ++i) {
			const JointPose &s = p.j[i];
			JointPose &d = acc_.j[i];
			d.rot = d.rot + (d.rot.dot(s.rot) < 0.0f ? -s.rot : s.rot) * w;
			d.pos += s.pos * w;
			d.scale += s.scale * w;
		}
		total_ += w;
		note(name, w);
	}

	void override_with(const Pose<N> &p, float w, uint64_t mask = ALL, const char *name = nullptr) {
		settle();
		w = saturate(w);
		if (w <= 0.0f) {
			return;
		}
		for (int i = 0; i < N; ++i) {
			if (mask >> i & 1u) {
				JointPose &d = acc_.j[i];
				d.rot = slerp_safe(d.rot, p.j[i].rot, w);
				d.pos = d.pos.lerp(p.j[i].pos, w);
				d.scale = d.scale.lerp(p.j[i].scale, w);
			}
		}
		note(name, w);
	}

	// The layer's rotation is applied in the joint's parent space (before the pose so far), so a
	// head yaw stays a turn about the vertical whatever the base pose pitched; offsets add and
	// scales multiply.
	void additive(const Pose<N> &p, float w, uint64_t mask = ALL, const char *name = nullptr) {
		settle();
		w = saturate(w);
		if (w <= 0.0f) {
			return;
		}
		for (int i = 0; i < N; ++i) {
			if (mask >> i & 1u) {
				JointPose &d = acc_.j[i];
				d.rot = slerp_safe(Quaternion(), p.j[i].rot, w) * d.rot;
				d.pos += p.j[i].pos * w;
				d.scale *= Vector3(1, 1, 1).lerp(p.j[i].scale, w);
			}
		}
		note(name, w);
	}

	const Pose<N> &result() {
		settle();
		return acc_;
	}

	// Debug readout of the named layers added since begin().
	int layer_count() const { return layers_; }
	const char *layer_name(int i) const { return names_[i]; }
	float layer_weight(int i) const { return weights_[i]; }
	// Name of the heaviest named layer, or "" if none.
	const char *dominant() const {
		int best = -1;
		for (int i = 0; i < layers_; ++i) {
			if (best < 0 || weights_[i] > weights_[best]) {
				best = i;
			}
		}
		return best < 0 ? "" : names_[best];
	}

private:
	void settle() {
		if (settled_) {
			return;
		}
		settled_ = true;
		const float inv = total_ > 1e-6f ? 1.0f / total_ : 0.0f;
		for (int i = 0; i < N; ++i) {
			JointPose &d = acc_.j[i];
			const float len = std::sqrt(d.rot.dot(d.rot));
			if (inv <= 0.0f) {
				d = JointPose();
			} else {
				d.rot = len > 1e-6f ? d.rot / len : Quaternion();
				d.pos *= inv;
				d.scale *= inv;
			}
		}
	}
	void note(const char *name, float w) {
		if (name && layers_ < MAX_LAYERS) {
			names_[layers_] = name;
			weights_[layers_++] = w;
		}
	}

	Pose<N> acc_;
	float total_ = 0.0f;
	bool settled_ = false;
	int layers_ = 0;
	const char *names_[MAX_LAYERS] = {};
	float weights_[MAX_LAYERS] = {};
};

// A single on/off blend weight: ramps to 1 in t_in seconds while `on`, back to 0 in t_out seconds
// otherwise, eased at both ends. For overrides and additive layers (cast, hurt, hang, celebrate).
struct Weight {
	float raw = 0.0f; // linear ramp position
	float w = 0.0f; // eased weight, the value to use

	float step(bool on, float t_in, float t_out, float dt) {
		dt = safe_dt(dt);
		if (on) {
			raw = t_in > 1e-5f ? saturate(raw + dt / t_in) : 1.0f;
		} else {
			raw = t_out > 1e-5f ? saturate(raw - dt / t_out) : 0.0f;
		}
		w = smoothstep01(raw);
		return w;
	}
	void snap(bool on) { raw = w = on ? 1.0f : 0.0f; }
};

// Cross-fade weights for a set of N mutually exclusive states (the base locomotion set, a monster's
// roam / windup / attack / recover). The active state ramps in over its own t_in, every other
// state ramps out over its own t_out, and the weights are normalised to sum to 1, so a blend is
// complete once the slower of the two ramps has finished. Typical times are 0.06-0.15 s.
template <int N>
struct StateWeights {
	float w[N] = {}; // normalised weights: feed these to PoseBlender::blend
	float raw[N] = {};
	float t_in[N], t_out[N];
	int active = -1;

	explicit StateWeights(float in = 0.12f, float out = 0.12f) {
		for (int i = 0; i < N; ++i) {
			t_in[i] = in;
			t_out[i] = out;
		}
	}
	void set_times(int state, float in, float out) {
		t_in[state] = in;
		t_out[state] = out;
	}
	// Jump straight to a state with no blend (spawn, teleport).
	void snap(int state) {
		active = state;
		for (int i = 0; i < N; ++i) {
			raw[i] = w[i] = i == state ? 1.0f : 0.0f;
		}
	}
	// Call every frame with the current state. The first call snaps.
	void drive(int state, float dt) {
		if (active < 0) {
			snap(state);
			return;
		}
		active = state;
		dt = safe_dt(dt);
		float sum = 0.0f;
		for (int i = 0; i < N; ++i) {
			if (i == state) {
				raw[i] = t_in[i] > 1e-5f ? saturate(raw[i] + dt / t_in[i]) : 1.0f;
			} else {
				raw[i] = t_out[i] > 1e-5f ? saturate(raw[i] - dt / t_out[i]) : 0.0f;
			}
			w[i] = smoothstep01(raw[i]);
			sum += w[i];
		}
		if (sum > 1e-5f) {
			for (int i = 0; i < N; ++i) {
				w[i] /= sum;
			}
		} else if (state >= 0 && state < N) {
			w[state] = 1.0f;
		}
	}
	float operator[](int i) const { return w[i]; }
};

} // namespace tot::anim
