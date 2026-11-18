#pragma once
// Movement metrics shared by the dragon controller and the level generator, so candy arcs trace
// jumps the player can actually make. Gravity and launch speed are derived from a designed jump
// height and time to apex (Pittman, "Building a Better Jump", GDC 2016) rather than tuned
// directly; the fall is heavier than the rise and the apex hangs while jump is held (Thorson,
// "Celeste & Forgiveness").

#include <vector>

namespace tot::move {

constexpr float RUN_SPEED = 7.9f;
constexpr float JUMP_HEIGHT = 1.9f; // full-hold jump, metres
constexpr float JUMP_APEX_T = 0.37f; // seconds to apex
constexpr float GRAVITY = 2.0f * JUMP_HEIGHT / (JUMP_APEX_T * JUMP_APEX_T); // ~27.8
constexpr float JUMP_V = 2.0f * JUMP_HEIGHT / JUMP_APEX_T; // ~10.3
constexpr float FALL_MULT = 1.6f; // gravity multiplier after the peak
constexpr float APEX_BAND = 1.8f; // |vy| below this with jump held: apex hang
constexpr float APEX_MULT = 0.5f;
constexpr float MAX_FALL = 17.0f;
constexpr float FLAP_V = 8.35f; // ~1.25 m more from the apex
constexpr float JUMP_CUT = 0.55f; // vertical speed kept when jump is released early
constexpr float COYOTE = 0.12f;
constexpr float BUFFER = 0.15f;

inline float gravity_at(float vy, bool held) {
	if (held && vy < APEX_BAND && vy > -APEX_BAND) {
		return GRAVITY * APEX_MULT;
	}
	return vy < 0.0f ? GRAVITY * FALL_MULT : GRAVITY;
}

struct ArcPoint {
	float t, y; // seconds since takeoff, height above takeoff
};

// The full-hold jump the controller performs, optionally with the wing flap at the apex, sampled
// every `dt` until it has fallen `floor` metres below the takeoff.
inline std::vector<ArcPoint> simulate_jump(bool flap, float floor = -6.0f, float dt = 1.0f / 120.0f) {
	std::vector<ArcPoint> out;
	float t = 0.0f, y = 0.0f, vy = JUMP_V;
	bool flapped = !flap;
	out.push_back({ 0.0f, 0.0f });
	while (y > floor && t < 4.0f) {
		if (!flapped && vy <= 0.0f) {
			vy = FLAP_V;
			flapped = true;
		}
		vy = vy - gravity_at(vy, true) * dt;
		if (vy < -MAX_FALL) {
			vy = -MAX_FALL;
		}
		y += vy * dt;
		t += dt;
		out.push_back({ t, y });
	}
	return out;
}

// Time at which the arc comes down through height `dh` (relative to takeoff), or -1 if it never
// gets that high.
inline float landing_time(const std::vector<ArcPoint> &arc, float dh) {
	for (size_t i = 1; i < arc.size(); ++i) {
		if (arc[i].y < arc[i - 1].y && arc[i - 1].y >= dh && arc[i].y < dh) {
			const float f = (arc[i - 1].y - dh) / (arc[i - 1].y - arc[i].y);
			return arc[i - 1].t + (arc[i].t - arc[i - 1].t) * f;
		}
	}
	return -1.0f;
}

// Stage 3c: bounce pads (car hoods, trampolines, inflatables, shop awnings). Mirrors dragon_controller.cpp:
// landing sets vy to the pad's launch speed, x SUPER_BOUNCE (1.45) with jump held on contact (the "full jump");
// while the launch carries the kid there is no apex hang; the wing flap (a fresh press) sets vy = FLAP_V and
// brings the floaty hold back; the air spin lifts vy to at least SPIN_LIFT once.
constexpr float BOUNCE_SUPER_MULT = 1.45f; // dragon_controller.cpp SUPER_BOUNCE
constexpr float SPIN_LIFT = 2.2f; // dragon_controller.cpp: the air spin's max(vy, 2.2)
// Where the bucket sweeps candy up: a piece is collected while it is within this height above the feet
// (festival_game.cpp: CandyField::collect from the feet + 0.55 with a 1.15 m reach).
constexpr float COLLECT_HIGH = 1.7f;

// Feet above the pad at the top of the bounce, flap pressed at the apex (the best moment: it sets vy, it
// doesn't add to it) and the spin at the next apex. cut: a plain bounce with jump not held, whose rise the
// controller cuts to JUMP_CUT once its 0.3 s grace runs out (the worst case for "a bounce alone").
inline float bounce_apex(float speed, bool full_jump, bool flap, bool spin = false, bool cut = false, float dt = 1.0f / 120.0f) {
	float vy = speed * (full_jump ? BOUNCE_SUPER_MULT : 1.0f), y = 0.0f, best = 0.0f, t = 0.0f;
	bool flapped = !flap, spun = !spin, launch = true, was_cut = !cut;
	for (int i = 0; i < 4800; ++i) {
		t += dt;
		if (!was_cut && t > 0.3f && vy > 0.0f) {
			vy *= JUMP_CUT;
			was_cut = true;
		}
		if (vy <= 0.0f && !flapped) {
			vy = FLAP_V;
			flapped = true;
			launch = false;
		} else if (vy <= 0.0f && !spun) {
			vy = vy > SPIN_LIFT ? vy : SPIN_LIFT;
			spun = true;
		}
		vy -= gravity_at(vy, !launch) * dt;
		y += vy * dt;
		best = y > best ? y : best;
		if (vy < 0.0f && flapped && spun) {
			break;
		}
	}
	return best;
}

inline float height_at(const std::vector<ArcPoint> &arc, float t) {
	for (size_t i = 1; i < arc.size(); ++i) {
		if (arc[i].t >= t) {
			const float f = (t - arc[i - 1].t) / (arc[i].t - arc[i - 1].t);
			return arc[i - 1].y + (arc[i].y - arc[i - 1].y) * f;
		}
	}
	return arc.back().y;
}

} // namespace tot::move
