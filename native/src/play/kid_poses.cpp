// The kid's pose library: one function per state, each filling a KidRig::Pose relative to the rest
// pose. KidRig::update() blends them (kid_rig.cpp).
//
// Conventions (Y up, +Z the kid's front, +X the kid's left; angles in radians):
//   pitch (about X): positive swings a hanging limb's tip BACK and tips the torso or head FORWARD
//                    (a nod); negative swings a limb forward and up. (An arm already raised above
//                    the shoulder by out tips the other way: positive brings it forward.) The head
//                    is as wide as the shoulders, so arms go overhead by out, past its side.
//   out:             abduction, positive lifts the limb away from the body on its own side (both sides).
//   knee, elbow:     flex, 0 = the rest pose, positive bends (the shin folds back, the forearm folds
//                    forward). The v2 arms rest with about 0.38 of bend already in them, so an elbow
//                    of -0.35 is a straight arm.
//   hand:            pitch about the wrist, negative tips the fist (and the wand) forward and up.
//   yaw (about Y):   positive turns the front toward the kid's left.
//   roll (about Z):  positive tips the top toward the kid's right.
// The left arm holds the wand, the right arm carries the candy bucket (which swings on its own
// spring, so the bucket arm stays calmer than the wand arm).
// On models without shins or forearms the rig folds knee and elbow bends into the whole limb (the
// leg swings back a little and shortens; the arm swings forward), so the same poses read on both.
//
// Height: a pose either calls stand() and only bends the knees (the rig then stands the kid on its
// lowest sole), or sets the pelvis height itself through body()'s offset (the strides do: they know
// which foot is down).

#include "play/kid_rig.h"

#include <cmath>

using namespace godot;

namespace tot {
namespace kid_pose {

using namespace anim;

const Vector3 CAST_ARM(-1.45f, 0.0f, -0.2f);
const Vector3 HANG_LIFT(0.0f, 0.43f, 0.14f);

namespace {

Quaternion E(float x, float y, float z) {
	return quat_from_euler(Vector3(x, y, z));
}

// Writes joints by name. `flip` mirrors left and right (the jump poses alternate their lead leg).
struct Set {
	Pose &p;
	bool flip = false;

	void root(float pitch, float yaw, float roll, const Vector3 &pos = Vector3(), const Vector3 &scale = Vector3(1, 1, 1)) {
		p[KidRig::J_ROOT] = JointPose(E(pitch, yaw, roll), pos, scale);
	}
	void body(float pitch, float yaw, float roll, const Vector3 &pos = Vector3(), const Vector3 &scale = Vector3(1, 1, 1)) {
		p[KidRig::J_BODY] = JointPose(E(pitch, yaw, roll), pos, scale);
	}
	void head(float pitch, float yaw, float roll, const Vector3 &pos = Vector3()) {
		p[KidRig::J_HEAD] = JointPose(E(pitch, yaw, roll), pos);
	}
	void arm_l(float pitch, float out, float elbow, float twist = 0.0f) {
		p[KidRig::J_ARM_L].rot = E(pitch, twist, out);
		p[KidRig::J_FOREARM_L].rot = E(-elbow, 0, 0);
	}
	void arm_r(float pitch, float out, float elbow, float twist = 0.0f) {
		p[KidRig::J_ARM_R].rot = E(pitch, -twist, -out);
		p[KidRig::J_FOREARM_R].rot = E(-elbow, 0, 0);
	}
	void hand_l(float pitch, float roll = 0.0f) { p[KidRig::J_HAND_L].rot = E(pitch, 0, roll); }
	void hand_r(float pitch, float roll = 0.0f) { p[KidRig::J_HAND_R].rot = E(pitch, 0, -roll); }
	void leg_l(float thigh, float knee, float out = 0.0f) {
		p[flip ? KidRig::J_LEG_R : KidRig::J_LEG_L].rot = E(thigh, 0, flip ? -out : out);
		p[flip ? KidRig::J_SHIN_R : KidRig::J_SHIN_L].rot = E(knee, 0, 0);
	}
	void leg_r(float thigh, float knee, float out = 0.0f) {
		p[flip ? KidRig::J_LEG_L : KidRig::J_LEG_R].rot = E(thigh, 0, flip ? out : -out);
		p[flip ? KidRig::J_SHIN_L : KidRig::J_SHIN_R].rot = E(knee, 0, 0);
	}
	// Tail: lift raises the tip, yaw swings it to the kid's left.
	void tail(float lift, float yaw, float roll = 0.0f) { p[KidRig::J_TAIL].rot = E(lift, -yaw, roll); }
	// Wings, mirrored: up raises the tips, back sweeps them behind the kid.
	void wings(float up, float back, float twist = 0.0f) {
		p[KidRig::J_WING_L].rot = E(twist, back, up);
		p[KidRig::J_WING_R].rot = E(twist, -back, -up);
	}
	// The rig stands the kid on its lowest sole (for poses that only bend the knees).
	void stand(float amount = 1.0f) { p[KidRig::J_AUX].pos.x = amount; }
};

// ------------------------------------------------------------------------------------------------
// The stride. A foot on the ground must not slide, so the stance is solved rather than swung: the
// sole's pivot (the heel while the foot comes down and lies flat, the toe once the heel peels up)
// moves back under the hip at exactly the ground speed, and the thigh angle that puts it there is
// found from the leg's measurements. The pelvis height follows from the leg. Between stances the
// leg swings through the air and the body hops.
// ------------------------------------------------------------------------------------------------

struct Pt {
	float z, y; // forward and up, from the hip joint
};

// Where a sole point ends up for a thigh pitch and a knee flex.
Pt leg_point(const KidRig::LegGeom &g, float thigh, float knee, const Vector3 &p) {
	if (g.shin) {
		const float b = thigh + knee; // the shoe's pitch
		return { -g.thigh * std::sin(thigh) + p.y * std::sin(b) + p.z * std::cos(b), -g.thigh * std::cos(thigh) + p.y * std::cos(b) - p.z * std::sin(b) };
	}
	// No knee joint: the rig swings the whole leg back by part of the flex and shortens it.
	const float a = thigh + knee * g.legacy_knee, py = p.y * clampf(std::cos(knee * 0.5f), 0.6f, 1.0f);
	return { py * std::sin(a) + p.z * std::cos(a), py * std::cos(a) - p.z * std::sin(a) };
}

struct LegPose {
	float thigh, knee, pelvis;
};

struct Gait {
	float duty = 0.4f; // the most of a stride one foot spends on the ground
	float reach = 0.3f; // the most ground a planted foot covers, metres
	float center = 0.015f; // how far ahead of the hip the middle of the step is
	float drive = 0.35f; // how far past its landing angle the thigh drives forward in the air (the high knee)
	float fold = 1.5f; // how far the shoe pitches heel-up as it is carried under the hip
	float hop = 3.0f; // exaggeration of the hop between stances
	// The swing's timing, as fractions of the time in the air: when the heel kick peaks, when the shoe
	// is level again (and how level: its pitch then), when the thigh passes under the hip (and its
	// angle there), when the knee is at its highest.
	float fold_at = 0.16f, level_at = 0.42f, level = 0.25f, lead_at = 0.2f, lead = -0.1f, high_at = 0.45f;
	float clear = 0.016f; // the air kept under the swinging shoe at mid swing, metres
};

// The swinging leg is drawn up into the plush body by up to this much (metres) to clear the ground:
// the thigh is too short to lift the big shoe by bending alone while the other foot is planted.
constexpr float HIP_HIKE = 0.04f;

// A smooth curve through `n` keys { x, y } (x rising): a Catmull-Rom spline, clamped at the ends.
float curve(const float (*k)[2], int n, float x) {
	if (x <= k[0][0]) {
		return k[0][1];
	}
	if (x >= k[n - 1][0]) {
		return k[n - 1][1];
	}
	int i = 0;
	while (i + 2 < n && x > k[i + 1][0]) {
		++i;
	}
	const auto slope = [&](int j) {
		const int a = j > 0 ? j - 1 : j, b = j + 1 < n ? j + 1 : j;
		return (k[b][1] - k[a][1]) / (k[b][0] - k[a][0]);
	};
	const float dx = k[i + 1][0] - k[i][0], t = (x - k[i][0]) / dx, t2 = t * t, t3 = t2 * t;
	return (2.0f * t3 - 3.0f * t2 + 1.0f) * k[i][1] + (t3 - 2.0f * t2 + t) * dx * slope(i) + (-2.0f * t3 + 3.0f * t2) * k[i + 1][1] + (t3 - t2) * dx * slope(i + 1);
}

float lowest(const KidRig::LegGeom &g, float thigh, float knee) {
	return Math::min(leg_point(g, thigh, knee, g.heel).y, leg_point(g, thigh, knee, g.toe).y);
}

// The stance at progress s (0 = the foot lands, 1 = it leaves), covering `travel` metres of ground.
// The leg is nearly straight and the big shoe rolls: it lands on the heel with the toe up, lies flat
// as the hip passes over it (half way), then the heel peels up and it pushes off the toe.
LegPose stance(const KidRig::LegGeom &g, const Gait &gt, float s, float travel) {
	const bool on_toe = s >= 0.5f;
	const Vector3 &pivot = on_toe ? g.toe : g.heel;
	const float target = (on_toe ? g.toe.z : g.heel.z) + travel * (0.5f - s) + gt.center;
	LegPose out;
	out.knee = 0.05f + 0.12f * std::sin(PI * s); // gives a little under the weight
	float lo = -1.3f, hi = 1.3f; // the pivot's z falls as the thigh swings back
	for (int i = 0; i < 16; ++i) {
		const float mid = 0.5f * (lo + hi);
		if (leg_point(g, mid, out.knee, pivot).z > target) {
			lo = mid;
		} else {
			hi = mid;
		}
	}
	out.thigh = 0.5f * (lo + hi);
	out.pelvis = -(g.hip_y + lowest(g, out.thigh, out.knee)); // the lower of heel and toe is on the ground
	return out;
}

// Both legs and the pelvis height for the stride clock `u` (the left foot lands at 0).
float stride(Set &s, const KidPoseCtx &c, const Gait &gt) {
	static const KidRig::LegGeom fallback;
	const KidRig::LegGeom &g = c.leg ? *c.leg : fallback;
	const float period = TAU / Math::max(c.gait_rate, 4.0f);
	const float ground = Math::max(c.speed, 0.0f) * period; // metres covered in one stride
	const float travel = Math::min(gt.reach, ground * gt.duty);
	const float duty = ground > 1e-4f ? clampf(travel / ground, 0.08f, gt.duty) : gt.duty;
	const LegPose first = stance(g, gt, 0.0f, travel), last = stance(g, gt, 1.0f, travel);
	const float amount = saturate(travel / 0.12f); // creeping: hardly any knee lift
	float pelvis = 0.0f;
	bool planted = false, swinging[2] = { false, false };
	float swing_v[2] = { 0.0f, 0.0f };
	LegPose leg[2];
	for (int side = 0; side < 2; ++side) {
		float u = c.gait + (side ? PI : 0.0f);
		u -= TAU * std::floor(u / TAU);
		LegPose &l = leg[side];
		if (u < TAU * duty) {
			l = stance(g, gt, u / (TAU * duty), travel);
			pelvis = planted ? Math::min(pelvis, l.pelvis) : l.pelvis;
			planted = true;
		} else {
			// Through the air. A big shoe on a short leg scrapes the ground unless it is carried: the
			// heel kicks up behind as the thigh comes forward (the shoe passes under the hip folded
			// right up, while the body is still up in its hop), the knee drives high with the shoe
			// level, then the leg reaches out to land on the heel, toe up. Curves through four keys
			// for the thigh and for the shoe's pitch; the knee is the difference.
			const float v = (u - TAU * duty) / (TAU * (1.0f - duty));
			const float b1 = last.thigh + last.knee, b0 = first.thigh + first.knee;
			const float high = first.thigh - gt.drive * amount;
			const float thigh_keys[5][2] = { { 0.0f, last.thigh }, { gt.lead_at, lerp(lerp(last.thigh, first.thigh, 0.3f), gt.lead, amount) }, { gt.high_at, high }, { 0.72f, high },
				{ 1.0f, first.thigh } };
			const float pitch_keys[5][2] = { { 0.0f, b1 }, { gt.fold_at, lerp(b1, Math::max(b1, gt.fold), amount) }, { gt.level_at, lerp(lerp(b1, b0, 0.45f), gt.level, amount) },
				{ 0.72f, lerp(lerp(b1, b0, 0.75f), 0.0f, amount) }, { 1.0f, b0 } };
			l.thigh = curve(thigh_keys, 5, v);
			l.knee = clampf(curve(pitch_keys, 5, v) - l.thigh, 0.0f, 1.5f);
			swinging[side] = true;
			swing_v[side] = v;
		}
	}
	if (!planted) {
		// Between stances: a hop from the height the last foot left at to the height the next one lands at.
		const float h = c.gait - PI * std::floor(c.gait / PI); // each half stride is the same
		const float f = saturate((h - TAU * duty) / Math::max(PI - TAU * duty, 1e-3f));
		const float air = (0.5f - duty) * period;
		pelvis = lerp(last.pelvis, first.pelvis, smoothstep01(f)) + Math::min(0.06f, 0.006f * amount + 9.8f * air * air / 8.0f * gt.hop * c.bounce) * arc(f);
	}
	// Whatever clearance the shoe still lacks it gets by being drawn up into the body, and beyond that
	// by swinging out sideways a little (a waddle).
	float out[2] = { 0.0f, 0.0f }, hike[2] = { 0.0f, 0.0f };
	const float floor_y = -(g.hip_y + pelvis);
	for (int side = 0; side < 2; ++side) {
		if (swinging[side]) {
			const float low = lowest(g, leg[side].thigh, leg[side].knee);
			const float need = floor_y + gt.clear * amount * std::sqrt(Math::max(0.0f, std::sin(PI * swing_v[side]))) - low; // (off the ground at once, not skimming it)
			if (need > 0.0f && low < -0.05f) {
				hike[side] = Math::min(need, HIP_HIKE);
				out[side] = std::acos(clampf(1.0f + (need - hike[side]) / low, 0.75f, 1.0f));
			}
		}
	}
	s.leg_l(leg[0].thigh, leg[0].knee, out[0]);
	s.leg_r(leg[1].thigh, leg[1].knee, out[1]);
	s.p[KidRig::J_LEG_L].pos.y = hike[0];
	s.p[KidRig::J_LEG_R].pos.y = hike[1];
	return pelvis;
}

} // namespace

// Standing: the weight drifts from foot to foot, the free knee relaxes onto its toe, the head and
// the wand wander. Breathing is its own additive layer.
void idle(Pose &p, const KidPoseCtx &c) {
	Set s{ p };
	const float shift = clampf(0.8f * anim::fidget(c.time * 0.17f, 11u) + 0.3f * std::sin(c.time * 1.9f), -1.0f, 1.0f); // which foot carries the weight
	const float nod = anim::fidget(c.time * 0.23f, 23u), drift = anim::fidget(c.time * 0.31f, 37u);
	// A kid never quite stands still: a little bounce at the knees in time with the breath.
	const float bounce = 0.5f + 0.5f * std::sin(c.time * TAU * 0.84f - 1.2f);
	s.stand();
	s.body(0.02f + 0.02f * nod, 0.1f * shift, -0.075f * shift, Vector3(0.036f * shift, 0, 0));
	s.head(-0.03f + 0.05f * nod, -0.1f * shift + 0.09f * drift, 0.09f * shift);
	s.arm_l(0.06f * nod + 0.07f * shift - 0.05f * bounce, 0.05f, 0.25f + 0.08f * drift + 0.08f * bounce);
	s.hand_l(-0.12f + 0.16f * drift);
	s.arm_r(-0.04f - 0.06f * shift, 0.06f + 0.03f * bounce, 0.2f + 0.05f * nod);
	// The feet stay put while the hips drift: both legs lean back against the shift, and the leg
	// without the weight bends at the knee with its heel off the ground.
	const float free_l = saturate(-shift), free_r = saturate(shift);
	s.leg_l(-0.1f * free_l - 0.07f * bounce, 0.05f + 0.26f * free_l + 0.14f * bounce, -0.115f * shift);
	s.leg_r(-0.1f * free_r - 0.07f * bounce, 0.05f + 0.26f * free_r + 0.14f * bounce, 0.115f * shift);
	if (c.tail && !c.swish) {
		// A lazy wag; the soft bend makes the tip trail the root.
		s.tail(0.06f * std::sin(c.time * 1.1f) + 0.05f * bounce, 0.3f * std::sin(c.time * 1.7f));
	} else if (c.tail) {
		s.tail(0.04f + 0.03f * std::sin(c.time * 0.65f), 0.08f * std::sin(c.time * 1.3f));
	}
	if (c.wings) {
		const float w = 0.13f * std::sin(c.time * 1.76f) + 0.05f * std::sin(c.time * 4.3f);
		s.wings(w - 0.05f, 0.12f - w * 0.5f);
	}
}

// A brisk trot: short quick steps (the legs are short), the body rocking over each planted foot,
// the wand arm swinging against the legs.
void walk(Pose &p, const KidPoseCtx &c) {
	Set s{ p };
	Gait gt;
	// The other foot is down for most of the swing, so the leg comes through early with the shoe level.
	gt.fold = 1.2f;
	gt.fold_at = 0.12f;
	gt.level_at = 0.28f;
	gt.level = 0.0f;
	gt.lead_at = 0.15f;
	gt.lead = -0.3f;
	gt.high_at = 0.38f;
	const float pelvis = stride(s, c, gt);
	const float u = c.gait, cs = std::cos(u);
	const float a = saturate(c.speed / 2.2f); // creeping: everything smaller
	s.body(0.07f + 0.02f * a, 0.1f * a * cs, -0.045f * a * std::sin(u - 0.5f), Vector3(0.012f * a * std::sin(u - 0.5f), pelvis, 0));
	s.head(-0.05f + 0.035f * a * std::sin(2.0f * u - 1.2f), -0.07f * a * cs, 0.04f * a * std::sin(u - 0.5f));
	// Arms swing against the legs; the bucket arm only a little.
	s.arm_l(0.04f + 0.5f * a * cs, 0.07f, 0.5f + 0.3f * a * saturate(-cs));
	s.hand_l(-0.1f - 0.22f * a * std::sin(u - 0.4f));
	s.arm_r(-0.08f - 0.16f * a * cs, 0.13f, 0.3f + 0.12f * a * saturate(cs));
	if (c.tail) {
		s.tail(0.06f + 0.04f * std::sin(2.0f * u), (c.swish ? 0.08f : 0.2f) * std::cos(u - 0.9f));
	}
	if (c.wings) {
		const float w = 0.11f * a * std::sin(2.0f * u - 0.6f);
		s.wings(w, 0.15f);
	}
}

// A bouncy toon run: big strides with a float between them, the wand arm pumping, the torso
// leaning in and twisting against the legs.
void run(Pose &p, const KidPoseCtx &c) {
	Set s{ p };
	Gait gt;
	gt.duty = 0.3f;
	gt.reach = 0.4f;
	gt.center = 0.04f;
	gt.drive = 0.5f;
	gt.fold = 1.9f;
	gt.hop = 2.2f;
	gt.fold_at = 0.14f;
	gt.level_at = 0.33f;
	gt.level = 0.0f;
	gt.lead_at = 0.17f;
	gt.lead = -0.3f;
	gt.high_at = 0.4f;
	gt.clear = 0.03f;
	const float pelvis = stride(s, c, gt);
	const float u = c.gait, cs = std::cos(u), sn = std::sin(u);
	s.body(0.27f, 0.15f * cs, -0.05f * std::sin(u - 0.4f), Vector3(0.012f * sn, pelvis, 0));
	s.head(-0.17f + 0.04f * std::sin(2.0f * u - 0.9f), -0.1f * cs, 0.035f * sn);
	s.arm_l(0.15f + 0.95f * cs, 0.12f, 1.2f - 0.35f * cs);
	s.hand_l(-0.3f * sn);
	s.arm_r(-0.1f - 0.22f * cs, 0.3f, 0.3f + 0.1f * cs); // the bucket stays low, out to the side
	if (c.tail) {
		s.tail((c.swish ? 0.12f : 0.22f) + 0.06f * std::sin(2.0f * u - 0.5f), (c.swish ? 0.1f : 0.22f) * std::cos(u - 1.0f));
	}
	if (c.wings) {
		s.wings(0.1f + 0.16f * std::sin(2.0f * u - 0.8f), 0.45f);
	}
}

// Going up: one knee driving, the other leg trailing, the wand punching the sky.
void rise(Pose &p, const KidPoseCtx &c) {
	Set s{ p, (c.parity & 1) != 0 };
	s.body(-0.12f, 0, 0);
	s.head(-0.22f, 0, 0);
	s.leg_l(-1.1f, 1.4f);
	s.leg_r(0.42f, 0.35f);
	s.arm_l(0.25f, 2.3f, -0.25f); // up past the side of the big head, not through it
	s.hand_l(-0.3f);
	s.arm_r(0.45f, 0.45f, 0.1f);
	if (c.tail) {
		s.tail(-0.3f, 0);
	}
	if (c.wings) {
		s.wings(-0.45f, 0.25f); // swept down by the climb
	}
}

// The top of the jump: knees tucked, arms wide.
void apex(Pose &p, const KidPoseCtx &c) {
	Set s{ p, (c.parity & 1) != 0 };
	s.body(0.02f, 0, 0);
	s.head(-0.06f, 0, 0);
	s.leg_l(-1.05f, 1.3f, 0.1f);
	s.leg_r(-0.6f, 1.1f, 0.1f);
	s.arm_l(-0.3f, 1.35f, 0.1f);
	s.hand_l(-0.2f);
	s.arm_r(-0.15f, 0.9f, 0.2f);
	if (c.tail) {
		s.tail(0.12f, 0);
	}
	if (c.wings) {
		s.wings(0.4f, -0.1f);
	}
}

// Coming down: legs reaching for the ground, arms floating up.
void fall(Pose &p, const KidPoseCtx &c) {
	Set s{ p, (c.parity & 1) != 0 };
	const float flut = std::sin(c.time * 17.0f) * 0.05f;
	s.body(0.07f, 0, 0);
	s.head(0.17f, 0, 0);
	s.leg_l(-0.35f, 0.3f, 0.07f);
	s.leg_r(0.12f, 0.4f, 0.07f);
	s.arm_l(-0.35f, 1.85f + flut, 0.1f);
	s.hand_l(0.25f + 2.0f * flut);
	s.arm_r(-0.25f, 1.2f - flut, 0.3f);
	if (c.tail) {
		s.tail(0.5f, 0.06f * std::sin(c.time * 9.0f));
	}
	if (c.wings) {
		s.wings(0.7f + 2.0f * flut, 0.0f);
	}
}

// Gliding after the double jump. With wings: stretched out flat on them. Without: floating down
// under the wand's magic like a parachute (or, for the cat, with every limb spread).
void glide(Pose &p, const KidPoseCtx &c) {
	Set s{ p };
	const float t = c.time;
	if (c.wings) {
		const float flutter = std::sin(t * 18.0f) * 0.05f;
		s.root(0.42f, 0, 0.07f * std::sin(t * 1.3f), Vector3(0, 0.014f * std::sin(t * 2.6f), 0));
		s.body(0.05f, 0, 0);
		s.head(-0.42f, 0.05f * std::sin(t * 0.9f), 0);
		s.leg_l(0.4f + 0.09f * std::sin(t * 3.1f), 0.45f + 0.15f * std::sin(t * 3.1f + 0.8f));
		s.leg_r(0.52f - 0.09f * std::sin(t * 3.1f), 0.5f - 0.15f * std::sin(t * 3.1f + 0.8f));
		s.arm_l(0.6f, 0.45f, -0.3f); // both arms swept back along the body
		s.hand_l(0.2f);
		s.arm_r(0.35f, 0.4f, 0.0f);
		// The rest pose angles the wings about 33 degrees up: this lowers them to a slight dihedral.
		s.wings(-0.45f + flutter, 0.1f);
		if (c.tail) {
			s.tail(0.3f, 0.1f * std::sin(t * 2.0f));
		}
		return;
	}
	const float sway = std::sin(t * 1.5f), kick = std::sin(t * 3.2f);
	s.root(-0.06f, 0, 0.07f * sway, Vector3(0, 0.014f * std::sin(t * 3.0f), 0));
	s.body(-0.04f, 0, -0.03f * sway);
	s.head(0.13f, 0, -0.05f * sway);
	if (c.spread) {
		// a flying cat: legs splayed wide with the knees soft and the feet paddling a little
		s.leg_l(-0.2f + 0.1f * kick, 0.42f + 0.12f * kick, 0.8f);
		s.leg_r(-0.2f - 0.1f * kick, 0.42f - 0.12f * kick, 0.8f);
		s.arm_l(-0.25f, 1.55f + 0.05f * kick, -0.1f);
		s.arm_r(-0.2f, 1.3f - 0.05f * kick, 0.0f);
	} else {
		s.leg_l(-0.25f + 0.22f * kick, 0.55f + 0.25f * std::sin(t * 3.2f + 1.0f), 0.1f);
		s.leg_r(-0.25f - 0.22f * kick, 0.55f - 0.25f * std::sin(t * 3.2f + 1.0f), 0.1f);
		s.arm_l(-0.25f + 0.08f * sway, 1.95f, 0.2f); // the wand held high
		s.hand_l(-0.2f + 0.15f * sway);
		s.arm_r(-0.2f, 1.1f, 0.2f);
	}
	if (c.tail) {
		s.tail(0.45f, 0.1f * sway);
	}
}

float roll_head_down(float k) {
	// The head's direction turns forward by the tumble (k * TAU) plus the tuck's own pitch (body 0.42 + head 0.5).
	return saturate(-std::cos(k * TAU + 0.92f));
}

// Tight tuck: knees to the chest, arms hugging them, chin down. The controller tumbles the root.
// Final fixes (2026-11-16): the tumble turns about a fixed point on the kid's axis (ROLL_CENTER), and the tuck puts the
// neck in front of it, so a forward-pitched head swings out at a larger radius and went ~20 cm into the road on every
// costume (more tuck measured worse). While the head points down the back and the neck straighten a little, bringing
// the head over the turning point (decision 18; KidRig::ground_guard lifts the ball for the rest).
void roll(Pose &p, const KidPoseCtx &c, float k) {
	Set s{ p };
	const float down = roll_head_down(k);
	s.body(0.42f * (1.0f - 0.6f * down), 0, 0);
	s.head(0.5f * (1.0f - 0.7f * down), 0, 0);
	if (c.costume == KID_PUMPKIN) {
		// The round pumpkin body would swallow knees tucked to the chest: the legs go out wide (frog-like) so the
		// striped legs and the shoes stay outside the ball, the arms out round it.
		s.body(0.3f * (1.0f - 0.6f * down), 0, 0);
		s.leg_l(-1.2f, 1.75f, 0.62f);
		s.leg_r(-1.2f, 1.75f, 0.62f);
		s.arm_l(-0.7f, 0.35f, 0.9f);
		s.hand_l(0.3f);
		s.arm_r(-0.45f, 0.55f, 0.7f);
	} else {
		s.leg_l(-1.75f, 1.5f);
		s.leg_r(-1.75f, 1.5f);
		s.arm_l(-0.95f, -0.2f, 1.35f);
		s.hand_l(0.3f);
		s.arm_r(-0.45f, 0.5f, 0.9f); // the bucket held out beside the knees, clear of the face
	}
	if (c.tail) {
		s.tail(0.35f, 0);
	}
	if (c.wings) {
		s.wings(-0.35f, 0.85f);
	}
}

// Ground pound. Stall: wound up in a tuck, arms overhead. Dive: stretched feet first, everything
// streaming up behind.
void pound(Pose &p, const KidPoseCtx &c, bool stall) {
	Set s{ p };
	if (stall) {
		s.root(-0.38f, 0, 0);
		s.body(-0.08f, 0, 0);
		s.head(-0.2f, 0, 0);
		const float splay = c.costume == KID_PUMPKIN ? 0.55f : 0.0f; // knees out of the pumpkin's round body
		s.leg_l(-1.6f + 0.4f * (splay > 0.0f), 1.5f, splay);
		s.leg_r(-1.6f + 0.4f * (splay > 0.0f), 1.5f, splay);
		s.arm_l(0.2f, 2.5f, 0.3f);
		s.hand_l(-0.4f);
		s.arm_r(0.1f, 1.9f, 0.5f);
		if (c.tail) {
			s.tail(-0.2f, 0);
		}
		if (c.wings) {
			s.wings(0.5f, 0.2f);
		}
		return;
	}
	s.root(0.05f, 0, 0, Vector3(), Vector3(0.94f, 1.14f, 0.94f));
	s.body(0.04f, 0, 0);
	s.head(0.22f, 0, 0);
	s.leg_l(-0.12f, 0.0f, 0.05f);
	s.leg_r(0.06f, 0.0f, 0.05f);
	s.arm_l(-0.2f, 2.55f, -0.3f);
	s.hand_l(0.3f);
	s.arm_r(-0.2f, 2.0f, 0.0f);
	if (c.tail) {
		s.tail(0.95f, 0);
	}
	if (c.wings) {
		s.wings(0.95f, 0.1f);
	}
}

// Hanging from a ledge: the chin over the lip, the wand hand on top of it (the rig puts the hand
// on the lip itself), the bucket held out to the side clear of the wall, the legs dangling.
// `sway` is the body's pendulum swing (radians).
void hang(Pose &p, const KidPoseCtx &c, float sway) {
	Set s{ p };
	const float kick = std::sin(c.time * 2.4f) * 0.16f;
	s.root(0, 0, 0, HANG_LIFT);
	s.body(0.05f + sway, 0, 0.04f);
	s.head(-0.22f - sway * 0.6f, -0.1f, -0.05f);
	s.arm_l(-1.25f - sway, 0.25f, 0.2f); // (models with elbows: replaced by the reach for the lip)
	s.hand_l(-0.6f);
	s.arm_r(0.55f + 0.1f * kick, 1.0f, -0.2f);
	s.leg_l(0.2f + kick + sway * 0.7f, 0.45f + 1.6f * saturate(kick));
	s.leg_r(0.2f - kick + sway * 0.7f, 0.45f + 1.6f * saturate(-kick));
	if (c.tail) {
		s.tail(-0.2f, 0.16f * std::sin(c.time * 1.6f));
	}
	if (c.wings) {
		s.wings(-0.15f + 0.1f * std::sin(c.time * 5.0f), 0.3f);
	}
}

// Climbing up over the lip (a push up on the wand hand, one knee over), or vaulting a fence on one hand.
void climb(Pose &p, const KidPoseCtx &c, float k, bool vault) {
	Set s{ p };
	(void)c;
	if (vault) {
		const float a = std::sin(saturate(k) * PI);
		s.root(0.1f * a, 0, 0.55f * a);
		s.body(0.15f * a, 0, 0);
		s.head(-0.1f * a, 0, -0.3f * a);
		s.arm_l(-0.5f * a, 0.5f * a, 0.3f);
		s.arm_r(-0.4f, 0.9f * a, 0.3f);
		s.leg_l(-1.0f * a, 1.2f * a, 0.2f * a);
		s.leg_r(-0.7f * a, 1.3f * a, 0.1f * a);
		return;
	}
	const float up = smoothstep01(k / 0.55f); // the body rising past the lip
	const float knee = smooth_pulse(k - 0.25f, 0.17f, 0.18f, 0.3f);
	const float over = std::sin(saturate(k) * PI);
	s.root(0, 0, 0, Vector3(0, HANG_LIFT.y * (1.0f - up), HANG_LIFT.z * (1.0f - smoothstep01((k - 0.1f) / 0.5f))));
	s.body(0.05f + 0.4f * over, 0, 0);
	s.head(-0.22f * (1.0f - up) - 0.15f * over, 0, 0);
	// The wand arm goes from reaching forward to pushing down past the hip.
	s.arm_l(lerp(-1.25f, 0.45f, up), lerp(0.25f, 0.15f, up), lerp(0.2f, 0.6f, over));
	s.hand_l(lerp(-0.6f, 0.2f, up));
	s.arm_r(lerp(0.55f, 0.1f, up), lerp(1.0f, 0.3f, up), 0.2f * over);
	s.leg_l(0.2f - 1.6f * knee, 0.45f + 1.0f * knee);
	s.leg_r(0.25f * (1.0f - up), 0.45f * (1.0f - up) + 0.2f);
}

// Knocked back: thrown off the feet, curled round the hit with the limbs flung toward it, shaking.
// away_local: the way the kid is thrown, in the kid's own space (-Z = backward).
void hurt(Pose &p, const KidPoseCtx &c, float t, const Vector3 &away_local) {
	Set s{ p };
	const float shake = 1.0f - smoothstep01(t / 0.32f);
	const float j1 = std::sin(c.time * 61.0f) * shake, j2 = std::sin(c.time * 47.0f + 1.3f) * shake;
	const float flail = std::sin(t * 19.0f) * (1.0f - smoothstep01(t / 0.6f)); // the limbs whip once or twice
	// Lean with the throw: thrown backward tips the whole kid back, sideways tips them over that way.
	const float back = -away_local.z, side = away_local.x;
	s.root(-0.45f * back + 0.03f * j1, 0, -0.4f * side + 0.03f * j2, Vector3(0.012f * j2, 0, 0.012f * j1));
	s.body(0.3f * back, 0, 0.1f * side);
	s.head(0.34f * back + 0.05f * j1, 0.12f * j2, 0.15f * side);
	s.arm_l(-1.3f + 0.3f * flail, 0.9f, 0.5f + 0.4f * flail);
	s.hand_l(-0.4f * flail);
	s.arm_r(-0.8f - 0.25f * flail, 0.75f, 0.6f - 0.3f * flail);
	s.leg_l(-0.95f + 0.2f * flail, 0.9f - 0.4f * flail, 0.2f);
	s.leg_r(-0.45f - 0.2f * flail, 1.1f + 0.3f * flail, 0.2f);
	if (c.tail) {
		s.tail(-0.35f, 0.2f * j1);
	}
	if (c.wings) {
		s.wings(0.6f + 0.2f * j2, -0.2f);
	}
}

// Riding the car: seated.
void mounted(Pose &p, const KidPoseCtx &c) {
	Set s{ p };
	s.body(-0.06f, 0, 0.02f * std::sin(c.time * 1.3f));
	s.head(0.02f, 0.05f * std::sin(c.time * 0.7f), 0);
	s.leg_l(-1.5f, 1.3f, 0.12f);
	s.leg_r(-1.5f, 1.3f, 0.12f);
	s.arm_l(-0.75f, 0.1f, 0.6f);
	s.hand_l(-0.3f);
	s.arm_r(-0.25f, 0.2f, 0.4f);
	if (c.tail) {
		s.tail(0.5f, 0);
	}
	if (c.wings) {
		s.wings(0.1f, 0.4f);
	}
}

// The spin attack (the controller turns the root one full turn): arms flung out straight, a low wide stance.
void spin(Pose &p, const KidPoseCtx &c, float k) {
	Set s{ p };
	const float a = std::sin(saturate(k) * PI);
	s.stand();
	s.root(0, 0, 0.08f * a);
	s.body(0.12f, 0, 0.08f);
	s.head(0.05f, 0, -0.14f);
	s.arm_l(0.0f, 1.25f, -0.35f);
	s.hand_l(-0.25f);
	s.arm_r(0.0f, 1.0f, -0.25f);
	s.leg_l(-0.3f, 0.75f, 0.24f);
	s.leg_r(-0.05f, 0.75f, 0.24f);
	if (c.tail) {
		s.tail(c.swish ? 0.0f : 0.3f, 0); // straight out behind, sweeping round
	}
	if (c.wings) {
		s.wings(-0.3f, 0.65f);
	}
}

// The wand flick, `t` seconds after it. The shot has already left, so it starts on the pointing
// pose (CAST_ARM, forearm and hand at rest: KidRig::cast_now() reads the tip there) and all the
// acting is follow-through: the wand whips up with the recoil as the body rocks back, hangs there,
// then swings down past the hip and settles. KidRig applies the arm as an override and the rest
// (torso, head, legs, the bucket arm) as an additive layer.
void cast(Pose &p, const KidPoseCtx &c, float t) {
	Set s{ p };
	(void)c;
	const float kick = ease_out_cubic(t / 0.09f) * (1.0f - smoothstep01((t - 0.15f) / 0.27f)); // the recoil: up fast, a beat at the top, eased off
	const float lunge = smooth_pulse(t, 0.05f, 0.2f, 0.4f); // the body behind the shot
	const float down = smoothstep01((t - 0.22f) / 0.3f); // the arm coming down again
	const float swing = std::sin(saturate((t - 0.36f) / 0.44f) * PI); // ... past the rest and back
	p[KidRig::J_ARM_L].rot = E(lerp(CAST_ARM.x - 0.7f * kick, 0.35f * swing, down), 0, lerp(CAST_ARM.z + 0.3f * kick, 0.08f, down));
	p[KidRig::J_FOREARM_L].rot = E(-(0.95f * kick * (1.0f - down) + 0.3f * swing), 0, 0);
	p[KidRig::J_HAND_L].rot = E(0.55f * kick * (1.0f - down) - 0.4f * swing, 0, 0); // the wrist cocks back, then trails the arm down
	s.body(-0.24f * kick + 0.12f * lunge, -0.38f * lunge, 0);
	s.head(0.14f * kick, 0.26f * lunge, 0);
	s.arm_r(0.8f * lunge, 0.3f * lunge, 0.2f * lunge); // the bucket arm thrown back for balance
	// A half step into the shot.
	p[KidRig::J_LEG_L].rot = E(-0.5f * lunge, 0, 0.06f * lunge);
	p[KidRig::J_SHIN_L].rot = E(0.5f * lunge, 0, 0);
	p[KidRig::J_LEG_R].rot = E(0.32f * lunge, 0, -0.06f * lunge);
	p[KidRig::J_SHIN_R].rot = E(0.25f * lunge, 0, 0);
	if (c.tail) {
		p[KidRig::J_TAIL].rot = E(0.45f * kick, 0.25f * lunge, 0);
	}
	if (c.wings) {
		p[KidRig::J_WING_L].rot = E(0, -0.25f * kick, 0.55f * kick);
		p[KidRig::J_WING_R].rot = E(0, 0.25f * kick, -0.55f * kick);
	}
}

// The double jump. With wings: two big beats, back arched, knees up. Without: a star jump with a
// flourish of the wand.
void flap(Pose &p, const KidPoseCtx &c, float k) {
	Set s{ p };
	k = saturate(k);
	if (c.wings) {
		const float beat = std::cos(k * TAU * 2.0f); // up, down, up, down
		const float fade = 1.0f - 0.3f * k;
		s.wings(0.85f * beat * fade, -0.3f * beat * fade, -0.15f * std::sin(k * TAU * 2.0f));
		s.body(-0.2f * fade - 0.06f * beat, 0, 0);
		s.head(-0.12f + 0.1f * beat, 0, 0);
		s.leg_l(-1.0f + 0.1f * beat, 1.4f, 0.05f);
		s.leg_r(-0.75f - 0.1f * beat, 1.3f, 0.05f);
		s.arm_l(-0.45f - 0.25f * beat, 0.8f, 0.7f + 0.3f * beat);
		s.hand_l(-0.3f * beat);
		s.arm_r(-0.25f, 0.65f, 0.6f);
		if (c.tail) {
			s.tail(-0.25f - 0.25f * beat, 0);
		}
		return;
	}
	const float a = std::sin(std::pow(k, 0.6f) * PI); // out fast, back slowly
	const float circ = k * TAU * 1.25f;
	s.root(0, 0, 0, Vector3(), Vector3(1.0f + 0.04f * a, 1.0f + 0.05f * a, 1.0f + 0.04f * a));
	s.body(-0.12f * a, 0, 0);
	s.head(-0.18f * a, 0, 0);
	s.leg_l(-0.1f, 0.1f, 0.62f * a);
	s.leg_r(-0.1f, 0.1f, 0.62f * a);
	s.arm_l(-0.3f + 0.35f * std::sin(circ) * a, 1.9f * a + 0.25f * std::cos(circ) * a, -0.2f * a); // the wand draws a ring overhead
	s.hand_l(0.4f * std::sin(circ) * a, 0.4f * std::cos(circ) * a);
	s.arm_r(-0.2f, 1.35f * a, 0.0f);
	if (c.tail) {
		s.tail(0.3f * a, 0);
	}
}

// Additive: breathing. The chest fills, the shoulders and arms ride up and out with it.
void breath(Pose &p, const KidPoseCtx &c, float amount) {
	const float br = std::sin(c.time * TAU * 0.42f) * amount, late = std::sin(c.time * TAU * 0.42f - 0.7f) * amount;
	p[KidRig::J_BODY].scale = Vector3(1.0f + 0.014f * br, 1.0f + 0.032f * br, 1.0f + 0.03f * br);
	p[KidRig::J_HEAD].pos = Vector3(0, 0.008f * br, 0);
	p[KidRig::J_HEAD].rot = E(-0.022f * late, 0, 0);
	p[KidRig::J_ARM_L].rot = E(0, 0, 0.045f * br);
	p[KidRig::J_ARM_R].rot = E(0, 0, -0.045f * br);
	p[KidRig::J_FOREARM_L].rot = E(-0.05f * late, 0, 0);
	p[KidRig::J_FOREARM_R].rot = E(-0.04f * late, 0, 0);
	if (c.wings) {
		p[KidRig::J_WING_L].rot = E(0, 0, 0.05f * late);
		p[KidRig::J_WING_R].rot = E(0, 0, -0.05f * late);
	}
}

// Additive, `t` seconds after a jump: the push-off the takeoff itself has no time for. The pelvis
// trails the rising body for a few frames with the knees still bent and the arms thrown back.
void launch(Pose &p, const KidPoseCtx &c, float t) {
	(void)c;
	const float a = 1.0f - smoothstep01(t / 0.14f);
	p[KidRig::J_ROOT].pos = Vector3(0, -0.05f * a, 0);
	p[KidRig::J_BODY].rot = E(0.18f * a, 0, 0);
	p[KidRig::J_LEG_L].rot = E(-0.4f * a, 0, 0);
	p[KidRig::J_LEG_R].rot = E(-0.35f * a, 0, 0);
	p[KidRig::J_SHIN_L].rot = E(0.9f * a, 0, 0);
	p[KidRig::J_SHIN_R].rot = E(0.9f * a, 0, 0);
	p[KidRig::J_ARM_L].rot = E(0.75f * a, 0, 0.1f * a);
	p[KidRig::J_ARM_R].rot = E(0.4f * a, 0, -0.1f * a);
	p[KidRig::J_FOREARM_L].rot = E(-0.3f * a, 0, 0);
}

// Additive, `t` seconds after touching down: knees give, the torso and head dip, the arms swing
// down and out for balance and bounce back a little late. strength 0.25 (a hop) .. 1 (a long
// fall) .. 1.4 (a ground pound).
void land(Pose &p, const KidPoseCtx &c, float t, float strength) {
	const float a = smooth_pulse(t, 0.05f, 0.03f, 0.32f) * strength;
	const float late = smooth_pulse(t - 0.04f, 0.07f, 0.03f, 0.36f) * strength; // arms and head follow the body
	const float knee = 1.15f * a;
	// The pumpkin's knees go out instead of up under its round body (it would swallow them).
	const float lp = c.costume == KID_PUMPKIN ? -0.5f : -0.72f, lo = c.costume == KID_PUMPKIN ? 0.36f : 0.14f;
	p[KidRig::J_LEG_L].rot = E(lp * a, 0, lo * a);
	p[KidRig::J_LEG_R].rot = E(lp * a, 0, -lo * a);
	p[KidRig::J_SHIN_L].rot = E(knee, 0, 0);
	p[KidRig::J_SHIN_R].rot = E(knee, 0, 0);
	p[KidRig::J_BODY].rot = E(0.2f * a, 0, 0);
	p[KidRig::J_HEAD].rot = E(0.05f * late, 0, 0);
	p[KidRig::J_ARM_L].rot = E(0.3f * late, 0, 0.5f * late);
	p[KidRig::J_ARM_R].rot = E(0.2f * late, 0, -0.35f * late);
	p[KidRig::J_FOREARM_L].rot = E(-0.3f * late, 0, 0);
	p[KidRig::J_HAND_L].rot = E(0.3f * (a - late), 0, 0);
	if (c.wings) {
		p[KidRig::J_WING_L].rot = E(0, 0, -0.4f * late);
		p[KidRig::J_WING_R].rot = E(0, 0, 0.4f * late);
	}
}

// Additive, `t` seconds after candy lands in the bucket: a look down into it as the bucket arm
// lifts it; from a chocolate up, a happy hop with the wand in the air.
// `look` and `lift` (0..1) are eased by the rig, so candy landing again and again holds the pose
// instead of restarting it: restarting snapped the head back to neutral on every pickup. `hop_t` is
// seconds into the happy hop (negative: no hop); the rig lets a hop finish before starting another.
void celebrate(Pose &p, const KidPoseCtx &c, float look, float lift, float hop_t, int tier) {
	(void)c;
	const float dur = CELEBRATE_HOP_LENGTH;
	const float a = look;
	const float t = hop_t;
	// The head only tips toward the bucket (the eyes do the looking): a full turn hides the face.
	p[KidRig::J_HEAD].rot = E(0.14f * a, -0.12f * a, 0.1f * a);
	p[KidRig::J_BODY].rot = E(0.04f * a, -0.08f * a, 0.05f * a);
	p[KidRig::J_ARM_R].rot = E(-0.45f * lift, 0, -0.22f * lift);
	p[KidRig::J_FOREARM_R].rot = E(-0.75f * lift, 0, 0);
	if (hop_t >= 0.0f) {
		const float u = saturate(t / (dur - 0.12f));
		const float hop = arc(u) * (tier >= 3 ? 0.17f : 0.11f);
		const float cheer = smooth_pulse(t, 0.08f, dur - 0.34f, 0.26f);
		p[KidRig::J_ROOT].pos = Vector3(0, hop, 0);
		p[KidRig::J_LEG_L].rot = E(-0.7f * arc(u), 0, 0.1f * arc(u));
		p[KidRig::J_LEG_R].rot = E(-0.4f * arc(u), 0, -0.1f * arc(u));
		p[KidRig::J_SHIN_L].rot = E(1.1f * arc(u), 0, 0);
		p[KidRig::J_SHIN_R].rot = E(0.8f * arc(u), 0, 0);
		p[KidRig::J_ARM_L].rot = E(0.15f * cheer, 0, 2.3f * cheer); // the wand up past the side of the head
		p[KidRig::J_FOREARM_L].rot = E(0.3f * cheer, 0, 0);
		p[KidRig::J_HAND_L].rot = E(-0.35f * cheer + 0.3f * cheer * std::sin(t * 30.0f), 0, 0); // and waggled
		p[KidRig::J_HEAD].rot = E((0.14f - 0.34f * arc(u)) * a, -0.12f * a * (1.0f - 0.6f * arc(u)), 0.1f * a * (1.0f - 2.0f * arc(u)));
	}
}

// Additive: braking hard from a run. Heels dug in, leaning back, the wand arm out in front.
void skid(Pose &p, const KidPoseCtx &c) {
	(void)c;
	p[KidRig::J_BODY].rot = E(-0.3f, 0, 0);
	p[KidRig::J_HEAD].rot = E(0.14f, 0, 0);
	p[KidRig::J_LEG_L].rot = E(-0.55f, 0, 0.05f);
	p[KidRig::J_LEG_R].rot = E(-0.3f, 0, -0.05f);
	p[KidRig::J_SHIN_L].rot = E(0.15f, 0, 0);
	p[KidRig::J_SHIN_R].rot = E(0.5f, 0, 0);
	p[KidRig::J_ARM_L].rot = E(-0.7f, 0, 0.45f);
	p[KidRig::J_FOREARM_L].rot = E(0.25f, 0, 0);
	p[KidRig::J_ARM_R].rot = E(0.15f, 0, -0.28f); // the bucket held out to the side, not thrown forward
}

// Additive: a quick shiver of every limb, `t` seconds after the knock that set it off.
// The skeleton's clatter: a sharp onset (40 ms), every joint jittering at its own rate (11 to 14 Hz, so
// they never move in step and it reads as loose bones rather than a buzzing toy), dying away over
// RATTLE_LENGTH. The hands and forearms flop most, the knees knock, the head bobbles.
void rattle(Pose &p, const KidPoseCtx &c, float t, float amount) {
	const float a = amount * smoothstep01(t / 0.04f) * (1.0f - smoothstep01(t / RATTLE_LENGTH));
	const float j1 = std::sin(c.time * 79.0f) * a, j2 = std::sin(c.time * 67.0f + 1.7f) * a, j3 = std::sin(c.time * 91.0f + 0.6f) * a;
	p[KidRig::J_HEAD].rot = E(0.09f * j1, 0.12f * j2, 0.1f * j3);
	p[KidRig::J_HEAD].pos = Vector3(0, 0.006f * std::fabs(j3), 0);
	p[KidRig::J_BODY].rot = E(0.035f * j2, 0.06f * j3, 0.035f * j1);
	p[KidRig::J_ARM_L].rot = E(0.26f * j2, 0, 0.2f * j1);
	p[KidRig::J_ARM_R].rot = E(0.24f * j3, 0, -0.2f * j2);
	p[KidRig::J_FOREARM_L].rot = E(0.38f * j3, 0, 0);
	p[KidRig::J_FOREARM_R].rot = E(0.38f * j1, 0, 0);
	p[KidRig::J_HAND_L].rot = E(0.45f * j1, 0, 0.25f * j2);
	p[KidRig::J_HAND_R].rot = E(0.4f * j2, 0, -0.25f * j3);
	p[KidRig::J_LEG_L].rot = E(0, 0, 0.07f * j1);
	p[KidRig::J_LEG_R].rot = E(0, 0, -0.07f * j1);
	p[KidRig::J_SHIN_L].rot = E(0.14f * saturate(j2), 0, 0);
	p[KidRig::J_SHIN_R].rot = E(0.14f * saturate(j3), 0, 0);
}

// The skeleton's fidget: 0..1 how hard the shimmy shakes (0.3 .. 1.45 s), for the cords' swing in KidRig.
float skeleton_shimmy(float t) {
	return smooth_pulse(t - 0.3f, 0.12f, 1.0f, 0.3f);
}

// Additive: playing to a camera that has come up close, `t` seconds into the act (it loops after
// MUG_LENGTH). A little bounce and a tilt of the head with the grin, then a shy look away and back.
// KidRig::face_want() does the face on the same clock.
void mug(Pose &p, const KidPoseCtx &c, float t) {
	(void)c;
	const float grin = smooth_pulse(t - 0.55f, 0.16f, 0.95f, 0.3f);
	const float bounce = std::sin(saturate((t - 0.55f) / 0.5f) * PI);
	const float shy = smooth_pulse(t - 1.72f, 0.1f, 0.2f, 0.16f);
	const float second = smooth_pulse(t - 3.4f, 0.2f, 0.8f, 0.3f);
	p[KidRig::J_HEAD].rot = E(-0.05f * grin + 0.12f * shy, -0.3f * shy, -0.13f * grin + 0.1f * second);
	p[KidRig::J_HEAD].pos = Vector3(0, 0.006f * bounce, 0);
	p[KidRig::J_BODY].rot = E(-0.03f * grin, -0.05f * shy, -0.03f * grin);
	p[KidRig::J_BODY].pos = Vector3(0, 0.012f * bounce, 0);
	p[KidRig::J_ARM_L].rot = E(0, 0, 0.12f * grin);
	p[KidRig::J_ARM_R].rot = E(0, 0, -0.12f * grin);
}

float fidget_length(int kind) {
	static const float len[FIDGET_KINDS] = { 1.5f, 1.5f, 1.9f, 1.3f, 1.6f };
	return len[kind < 0 || kind >= FIDGET_KINDS ? 0 : kind];
}

float fidget_length(int kind, int costume) {
	if (kind == 4 && costume == KID_WITCH) {
		return 2.3f; // the hat slips, the hand comes up, pushes it straight, a nod
	}
	if (kind == 4 && costume == KID_SKELETON) {
		return 2.2f; // a "brrr" hunch, the rattling shimmy, a ta-da
	}
	if (kind == 4 && costume == KID_PUMPKIN) {
		return 2.5f; // a dip, a hop, the head wobble that whips the stem, a pleased tilt
	}
	return fidget_length(kind);
}

// The pumpkin's fidget: the head's side-to-side wobble (0.7 .. 1.95 s, about 2.6 Hz), -1..1.
float pumpkin_wobble(float t) {
	return std::sin((t - 0.7f) * TAU * 2.6f) * smooth_pulse(t - 0.7f, 0.1f, 0.75f, 0.4f);
}

// The witch's hat in her fidget: it slips down over her left eye (0.05 .. 0.4 s), stays there while the
// wand hand comes up for the brim, and is pushed back straight (1.2 .. 1.5 s) with a little overshoot.
float witch_hat_askew(float t) {
	const float slip = smooth_pulse(t - 0.05f, 0.3f, 0.85f, 0.3f);
	const float settle = t > 1.5f ? -0.22f * std::sin((t - 1.5f) * 11.0f) * std::exp(-(t - 1.5f) * 4.5f) : 0.0f;
	return slip + settle;
}

// Additive idle fidgets, `t` seconds in:
//   0 a peek into the bucket      1 a twirl of the wand        2 a look round both ways
//   3 a foot tap                  4 the costume's own (see below)
void fidget(Pose &p, const KidPoseCtx &c, int kind, float t) {
	const float len = fidget_length(kind, c.costume);
	const float a = smooth_pulse(t, 0.22f, len - 0.55f, 0.33f);
	switch (kind) {
		case 0:
			p[KidRig::J_HEAD].rot = E(0.34f * a, -0.32f * a, -0.12f * a); // (the eyes go the rest of the way)
			p[KidRig::J_BODY].rot = E(0.05f * a, -0.1f * a, 0.05f * a);
			p[KidRig::J_ARM_R].rot = E(-0.45f * a, 0, -0.2f * a);
			p[KidRig::J_FOREARM_R].rot = E(-0.85f * a + 0.08f * a * std::sin(t * 9.0f), 0, 0);
			break;
		case 1: {
			// The forearm comes up and the wrist draws rings with the wand's tip.
			const float ang = t * 11.0f;
			p[KidRig::J_ARM_L].rot = E(-0.45f * a, 0, 0.3f * a);
			p[KidRig::J_FOREARM_L].rot = E(-0.95f * a, 0, 0);
			p[KidRig::J_HAND_L].rot = E(-0.35f * a + 0.45f * a * std::sin(ang), 0, 0.45f * a * std::cos(ang));
			p[KidRig::J_HEAD].rot = E(-0.05f * a, 0.3f * a, 0.06f * a);
			p[KidRig::J_BODY].rot = E(0, 0.08f * a, 0);
			break;
		}
		case 2: {
			const float sweep = std::sin(saturate((t - 0.15f) / (len - 0.3f)) * TAU);
			p[KidRig::J_HEAD].rot = E(-0.06f * a, 0.6f * sweep * a, 0);
			p[KidRig::J_BODY].rot = E(0, 0.14f * sweep * a, 0);
			break;
		}
		case 3: {
			// The right foot out on its toe, the heel tapping.
			const float tap = saturate(std::sin(t * 15.0f)) * a;
			p[KidRig::J_LEG_R].rot = E(-0.5f * a - 0.18f * tap, 0, -0.1f * a);
			p[KidRig::J_SHIN_R].rot = E(0.5f * a + 0.42f * tap, 0, 0);
			p[KidRig::J_BODY].rot = E(0, 0.05f * a, 0.05f * a);
			p[KidRig::J_HEAD].rot = E(0.08f * a + 0.03f * tap, 0, -0.08f * a);
			p[KidRig::J_ARM_L].rot = E(0, 0, 0.12f * a); // the wand hand on the hip, more or less
			p[KidRig::J_FOREARM_L].rot = E(-0.5f * a, 0, 0);
			break;
		}
		default: {
			const float up = std::sin(saturate(t / len) * PI);
			switch (c.costume) {
				case KID_GHOST: {
					// "Boo!": a lean in with both arms thrown up and forward under the sheet, the hands
					// flapping, a little bob on the toes on the "boo" itself (KidRig puffs the cowl open).
					const float wig = std::sin(t * 16.0f) * a;
					const float boo = smooth_pulse(t, 0.3f, 0.45f, 0.45f);
					p[KidRig::J_ARM_L].rot = E(-1.05f * a - 0.3f * boo, 0, 0.8f * a + 0.12f * wig);
					p[KidRig::J_ARM_R].rot = E(-0.75f * a - 0.2f * boo, 0, -0.65f * a + 0.1f * wig);
					p[KidRig::J_FOREARM_L].rot = E(-0.55f * a, 0, 0);
					p[KidRig::J_FOREARM_R].rot = E(-0.45f * a, 0, 0);
					p[KidRig::J_HAND_L].rot = E(-0.35f * a + 0.4f * wig, 0, 0);
					p[KidRig::J_HAND_R].rot = E(-0.2f * a + 0.3f * wig, 0, 0);
					p[KidRig::J_ROOT].pos = Vector3(0, 0.025f * boo, 0);
					p[KidRig::J_BODY].rot = E(0.06f * a + 0.14f * boo, 0, 0.04f * wig);
					p[KidRig::J_HEAD].rot = E(-0.06f * a - 0.12f * boo, 0, -0.06f * wig);
					break;
				}
				case KID_CAT: {
					// Ear and tail flick: the head cocks toward the left ear while it twitches twice, then
					// toward the right one; the tail root whips across and back (KidRig flicks the ears and
					// kicks the tail's spring, so the whip runs down the tail).
					const float l = smooth_pulse(t, 0.18f, 0.42f, 0.25f), r = smooth_pulse(t - 0.82f, 0.15f, 0.3f, 0.3f);
					const float whip = std::sin(saturate(t / len) * TAU * 1.5f) * a;
					p[KidRig::J_HEAD].rot = E(-0.05f * a, 0.14f * (l - r), -0.17f * (l - r));
					p[KidRig::J_BODY].rot = E(0, 0.04f * (l - r), -0.03f * (l - r));
					p[KidRig::J_TAIL].rot = E(0.22f * up, 0.5f * whip, 0);
					p[KidRig::J_ARM_L].rot = E(-0.15f * a, 0, 0.12f * a);
					p[KidRig::J_FOREARM_L].rot = E(-0.25f * a, 0, 0);
					break;
				}
				case KID_WITCH: {
					// Straightens her hat: it slips down over her left eye (KidRig tips it, witch_hat_askew), the
					// head dips with it; the wand hand comes up to the brim, pushes it back up with a little
					// wiggle while the head tips up into it, then the hand comes down and she gives a pleased nod.
					const float slip = witch_hat_askew(t);
					const float reach = smooth_pulse(t - 0.25f, 0.45f, 1.0f, 0.45f);
					const float push = smooth_pulse(t - 1.1f, 0.25f, 0.15f, 0.3f);
					const float wig = std::sin((t - 1.1f) * 17.0f) * push;
					const float nod = smooth_pulse(t - 1.75f, 0.2f, 0.1f, 0.3f);
					(void)a;
					// The arm goes up and OUT to the side, almost straight, so the hand meets the left brim beside the
					// head and the wand points up and away (pitched forward, the hand came to the face and the wand
					// crossed the eye).
					p[KidRig::J_ARM_L].rot = E(-0.12f * reach, 0, 2.05f * reach + 0.06f * wig);
					p[KidRig::J_FOREARM_L].rot = E(0.1f * reach - 0.12f * push, 0, 0);
					p[KidRig::J_HAND_L].rot = E(0.25f * wig, 0, 0.25f * reach);
					p[KidRig::J_HEAD].rot = E(0.1f * slip - 0.1f * push + 0.12f * nod, 0.12f * reach, -0.1f * slip - 0.06f * reach + 0.05f * push);
					p[KidRig::J_BODY].rot = E(0.02f * nod, 0.05f * reach, 0.05f * reach);
					p[KidRig::J_ARM_R].rot = E(0, 0, -0.08f * reach);
					break;
				}
				case KID_SKELETON: {
					// "Brrr ... rattle rattle ... ta-da!": a shivery hunch (shoulders up, head tucked, knees in), then a
					// loose-boned shimmy - hips and shoulders wiggling against each other at 4.5 Hz, arms out and
					// dangling, hands flopping, knees knocking, every joint clattering on top - and a little ta-da
					// with the wand arm out and the head cocked. KidRig rattles the limbs at the start (rattle_t_)
					// and swings the hood cords with the shimmy (skeleton_shimmy), the face chatters its teeth.
					(void)a;
					const float hunch = smooth_pulse(t, 0.1f, 0.18f, 0.2f);
					const float shake = skeleton_shimmy(t);
					const float tada = smooth_pulse(t - 1.5f, 0.15f, 0.25f, 0.3f);
					const float w = t * TAU * 4.5f;
					const float sw = std::sin(w), sw2 = std::sin(w * 2.0f + 0.7f);
					// The fast bone clatter on top of the shimmy is off (user, 2026-11-16: no bone rattle on the skeleton
					// costume); the shimmy itself stays. Was sin(t * 83) and sin(t * 71 + 1.3), times shake.
					const float jit = 0.0f, jit2 = 0.0f;
					// (Round 1's amplitudes - body yaw 0.15, arms +-0.14 - hardly read at showcase distance.)
					p[KidRig::J_ROOT].pos = Vector3(0, 0.014f * std::fabs(sw2) * shake, 0);
					p[KidRig::J_BODY].rot = E(0.07f * hunch - 0.05f * tada + 0.04f * jit, 0.27f * sw * shake, 0.12f * sw * shake - 0.05f * tada);
					p[KidRig::J_BODY].scale = Vector3(1.0f + 0.015f * hunch, 1.0f - 0.035f * hunch + 0.02f * tada, 1.0f + 0.015f * hunch);
					// The head bobbles against the hips (a loose skull on a loose neck).
					p[KidRig::J_HEAD].rot = E(0.14f * hunch - 0.1f * tada + 0.07f * jit2, -0.22f * sw * shake + 0.08f * jit, -0.2f * sw * shake + 0.18f * tada);
					p[KidRig::J_HEAD].pos = Vector3(0, -0.012f * hunch, 0);
					// Arms out and loose, flapping up and down against each other with the hips; the ta-da raises the
					// wand arm up and out, straight, the wand up.
					p[KidRig::J_ARM_L].rot = E(-0.15f * shake + 0.15f * sw2 * shake - 0.35f * tada + 0.14f * jit, 0,
							-0.12f * hunch + (0.55f + 0.28f * sw) * shake + 1.05f * tada);
					p[KidRig::J_ARM_R].rot = E(-0.1f * shake - 0.12f * sw2 * shake + 0.14f * jit2, 0, 0.1f * hunch - (0.45f - 0.26f * sw) * shake - 0.3f * tada);
					p[KidRig::J_FOREARM_L].rot = E(-0.45f * hunch - (0.3f + 0.4f * sw) * shake + 0.3f * tada, 0, 0);
					p[KidRig::J_FOREARM_R].rot = E(-0.45f * hunch - (0.3f - 0.4f * sw) * shake, 0, 0);
					p[KidRig::J_HAND_L].rot = E(0.6f * sw * shake - 0.45f * tada, 0, 0.35f * jit);
					p[KidRig::J_HAND_R].rot = E(-0.55f * sw * shake, 0, 0.35f * jit2);
					// Knees knocking in and out, the heels lifting in turn.
					p[KidRig::J_LEG_L].rot = E(-0.1f * hunch, 0, -0.06f * hunch + 0.16f * sw * shake);
					p[KidRig::J_LEG_R].rot = E(-0.1f * hunch, 0, 0.06f * hunch - 0.16f * sw * shake);
					p[KidRig::J_SHIN_L].rot = E(0.2f * hunch + 0.22f * saturate(sw) * shake, 0, 0);
					p[KidRig::J_SHIN_R].rot = E(0.2f * hunch + 0.22f * saturate(-sw) * shake, 0, 0);
					break;
				}
				case KID_PUMPKIN: {
					// "Bobble": a dip, a hop with the arms flung out, a squashy landing, then the head wobbles from
					// side to side (the stem, the leaves and the tendrils whip after it: KidRig adds pumpkin_wobble
					// to the stem's bend), and a pleased tilt of the head at the end. The round body squashes and
					// stretches with it.
					(void)a;
					const float dip = smooth_pulse(t, 0.12f, 0.06f, 0.14f);
					const float u = saturate((t - 0.3f) / 0.34f);
					const float h = u > 0.0f && u < 1.0f ? arc(u) : 0.0f;
					const float sq = smooth_pulse(t - 0.62f, 0.04f, 0.03f, 0.2f);
					const float wob = pumpkin_wobble(t);
					const float env = smooth_pulse(t - 0.7f, 0.1f, 0.75f, 0.4f);
					const float tilt = smooth_pulse(t - 1.85f, 0.22f, 0.2f, 0.25f);
					const float arms = smooth_pulse(t - 0.28f, 0.1f, 1.45f, 0.45f);
					// (round 1: head roll 0.26, hop 5.5 cm hardly read at showcase distance)
					p[KidRig::J_ROOT].pos = Vector3(0, 0.075f * h, 0);
					p[KidRig::J_BODY].rot = E(0.09f * dip - 0.05f * h + 0.05f * sq, 0.12f * wob, -0.11f * wob);
					p[KidRig::J_BODY].scale = Vector3(1.0f + 0.04f * (dip + sq) - 0.025f * h, 1.0f - 0.06f * (dip + sq) + 0.05f * h, 1.0f + 0.04f * (dip + sq) - 0.025f * h);
					p[KidRig::J_HEAD].rot = E(0.1f * dip - 0.1f * h - 0.06f * tilt, 0.08f * wob, 0.38f * wob + 0.18f * tilt);
					p[KidRig::J_LEG_L].rot = E(-0.35f * dip - 0.45f * h - 0.3f * sq, 0, 0.08f * (dip + sq) + 0.12f * h);
					p[KidRig::J_LEG_R].rot = E(-0.35f * dip - 0.35f * h - 0.3f * sq, 0, -0.08f * (dip + sq) - 0.12f * h);
					p[KidRig::J_SHIN_L].rot = E(0.7f * dip + 0.9f * h + 0.6f * sq, 0, 0);
					p[KidRig::J_SHIN_R].rot = E(0.7f * dip + 0.7f * h + 0.6f * sq, 0, 0);
					// Arms flung out on the hop, flapping a little with the wobble, back down as the head tilts.
					p[KidRig::J_ARM_L].rot = E(-0.25f * arms, 0, -0.1f * dip + 0.55f * arms + 0.5f * h + 0.16f * wob * env);
					p[KidRig::J_ARM_R].rot = E(-0.15f * arms, 0, 0.1f * dip - 0.35f * arms - 0.4f * h + 0.12f * wob * env);
					p[KidRig::J_FOREARM_L].rot = E(-0.35f * arms, 0, 0);
					p[KidRig::J_FOREARM_R].rot = E(-0.25f * arms, 0, 0);
					p[KidRig::J_HAND_L].rot = E(-0.3f * arms + 0.25f * wob, 0, 0);
					break;
				}
				default: {
					// A stretch. With wings they open wide and shake out; a tail lifts and thumps down twice.
					p[KidRig::J_BODY].rot = E(-0.1f * a, 0, 0);
					p[KidRig::J_BODY].scale = Vector3(1.0f - 0.02f * a, 1.0f + 0.045f * a, 1.0f - 0.02f * a);
					p[KidRig::J_HEAD].rot = E(-0.16f * a, 0, 0.05f * a * std::sin(t * 6.0f));
					p[KidRig::J_ARM_L].rot = E(-0.2f * a, 0, 0.7f * a);
					p[KidRig::J_FOREARM_L].rot = E(0.3f * a, 0, 0);
					p[KidRig::J_ARM_R].rot = E(-0.1f * a, 0, -0.4f * a);
					p[KidRig::J_SHIN_L].rot = E(0.0f, 0, 0);
					if (c.wings) {
						p[KidRig::J_WING_L].rot = E(0, -0.3f * up, 0.65f * up + 0.09f * a * std::sin(t * 21.0f));
						p[KidRig::J_WING_R].rot = E(0, 0.3f * up, -0.65f * up - 0.09f * a * std::sin(t * 21.0f));
					}
					if (c.tail) {
						p[KidRig::J_TAIL].rot = E(0.4f * up - 0.42f * saturate(std::sin(t * 9.0f - 1.0f)) * a, 0, 0);
					}
					break;
				}
			}
			break;
		}
	}
}

} // namespace kid_pose
} // namespace tot
