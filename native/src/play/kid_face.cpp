// The kid's face: blinks, where the eyes look, brows, mouth shapes. KidRig::update() says what the
// face should be doing (a KidFaceWant) and this eases the v2 face parts toward it:
//   eye_l / eye_r        scaled along local Y to blink, squint and widen; hidden while fully shut
//   alt_lash_l / _r      the shut eye (a lid with the lash arc), shown instead
//   iris_l / iris_r      slides in the eye's local XY to look about (the catchlights stay put)
//   brow_l / brow_r      raised along the head's Y, tilted about its Z
//   mouth_smile, alt_mouth_open, alt_mouth_o, alt_mouth_grit   one shown at a time, swapped with a pop
// Every part is optional. A model with none of them leaves the face unbound.

#include "play/kid_rig.h"

#include <godot_cpp/classes/mesh_instance3d.hpp>

#include <cmath>

using namespace godot;

namespace tot {

using namespace anim;

namespace {
constexpr float IRIS_X = 0.0052f, IRIS_Y = 0.0022f; // how far the iris slides (metres) at the full eye turn (its top and bottom follow the eye's outline)
constexpr float EYE_YAW = 0.4f, EYE_PITCH = 0.32f; // the eye turn (radians) that puts the iris at its limit
constexpr float BROW_UP = 0.0075f, BROW_DOWN = 0.0045f, BROW_TILT = 0.34f, BROW_IN = 0.002f;
constexpr float CLOSE_TIME = 0.075f, HOLD_TIME = 0.035f, OPEN_TIME = 0.13f;
constexpr float GAP_MIN = 1.7f, GAP_MAX = 4.2f, DOUBLE_CHANCE = 0.2f;
} // namespace

void KidFace::unbind() {
	any_ = false;
	for (int i = 0; i < 2; ++i) {
		eye_[i] = iris_[i] = brow_[i] = lash_[i] = nullptr;
	}
	for (Node3D *&m : mouth_) {
		m = nullptr;
	}
}

void KidFace::find(Node *n) {
	if (Node3D *n3 = Object::cast_to<Node3D>(n)) {
		const String name = n3->get_name();
		static const struct {
			const char *name;
			int slot;
		} PARTS[] = {
			{ "eye_l", 0 }, { "eye_r", 1 }, { "iris_l", 2 }, { "iris_r", 3 }, { "brow_l", 4 }, { "brow_r", 5 }, { "alt_lash_l", 6 }, { "alt_lash_r", 7 },
			{ "mouth_smile", 8 }, { "alt_mouth_open", 9 }, { "alt_mouth_o", 10 }, { "alt_mouth_grit", 11 },
		};
		for (const auto &p : PARTS) {
			if (name != String(p.name)) {
				continue;
			}
			Node3D **slot = p.slot < 2 ? &eye_[p.slot] : (p.slot < 4 ? &iris_[p.slot - 2] : (p.slot < 6 ? &brow_[p.slot - 4] : (p.slot < 8 ? &lash_[p.slot - 6] : &mouth_[p.slot - 8])));
			if (!*slot) {
				*slot = n3;
			}
		}
	}
	for (int i = 0; i < n->get_child_count(); ++i) {
		find(n->get_child(i));
	}
}

void KidFace::bind(Node *visual) {
	unbind();
	if (!visual) {
		return;
	}
	find(visual);
	for (int i = 0; i < 2; ++i) {
		if (eye_[i]) {
			eye_rest_[i] = eye_[i]->get_transform();
			any_ = true;
		}
		if (iris_[i]) {
			iris_rest_[i] = iris_[i]->get_transform();
			any_ = true;
		}
		if (brow_[i]) {
			brow_rest_[i] = brow_[i]->get_transform();
			any_ = true;
		}
		// Wet eyes: only a faint tight highlight from the toon shader on the eye (it is additive, so on the
		// dark iris and the ink lash line in the same meshes it reads as chrome streaks); the glossy look
		// comes from the two modelled catchlights. None at all now that the eye has no white: any gloss on the
		// dark rim showed as a pale arc just inside the lash line (FIDELITY_PASS decision 13). -2 = fully
		// matte (toon.gdshader): no painted highlight and no sky sheen, which turned the lash line, the rim
		// and the pupil lavender in the bright face close-up.
		// (2026-11-17) The eye is an anime ink dome again (user: eyes "more like the anime style they had before");
		// like the old ones it keeps the toon shader's soft sheen, so no gloss -2 on it. The glint stays matte.
		if (GeometryInstance3D *g = Object::cast_to<GeometryInstance3D>(iris_[i])) {
			g->set_instance_shader_parameter("gloss", -2.0f);
		}
	}
	int mouths = 0;
	for (int m = 0; m < KidFaceWant::MOUTH_COUNT; ++m) {
		if (mouth_[m]) {
			mouth_rest_[m] = mouth_[m]->get_transform();
			++mouths;
		}
	}
	any_ = any_ || mouths > 1;
	reset();
}

void KidFace::reset() {
	blink_phase_ = 0;
	blink_raw_ = closed_ = 0.0f;
	blink_hold_ = 0.0f;
	blink_second_ = was_held_ = false;
	// The first blink comes a while after a cut, never on it, and not at the same moment every time.
	++resets_;
	rng_ = Rand(0x51d0u + resets_ * 7919u);
	blink_next_ = rng_.range(1.0f, 2.4f);
	wide_.reset();
	squint_.reset();
	brow_up_.reset();
	brow_tilt_.reset();
	gaze_x_.reset();
	gaze_y_.reset();
	pop_.reset(1.0f);
	dart_ = Vector2();
	dart_next_ = 0.6f;
	mouth_now_ = KidFaceWant::SMILE;
	mouth_age_ = 1.0f;
	if (any_) {
		step(KidFaceWant(), 0.0f);
	}
}

void KidFace::step(const KidFaceWant &want, float dt) {
	if (!any_) {
		return;
	}
	dt = safe_dt(dt);

	// --- Blink.
	if (want.blink) {
		blink_hold_ = Math::max(blink_hold_, want.blink_hold);
		if (blink_phase_ == 0 || blink_phase_ == 3) {
			blink_phase_ = 1;
		} else if (blink_phase_ == 2) {
			blink_t_ = Math::max(blink_t_, HOLD_TIME + blink_hold_);
		}
	}
	switch (blink_phase_) {
		case 0:
			blink_next_ -= dt;
			if (blink_next_ <= 0.0f && want.wide < 0.5f && !want.hold_open) {
				blink_phase_ = 1;
			} else if (want.hold_open) {
				was_held_ = true;
			} else if (was_held_) {
				// The action is over: a blink soon after, as the kid settles (not the instant it ends).
				was_held_ = false;
				blink_next_ = clampf(blink_next_, 0.22f, rng_.range(0.35f, 0.9f));
			}
			break;
		case 1:
			blink_raw_ += dt / CLOSE_TIME;
			if (blink_raw_ >= 1.0f) {
				blink_raw_ = 1.0f;
				blink_phase_ = 2;
				blink_t_ = HOLD_TIME + blink_hold_;
				blink_hold_ = 0.0f;
			}
			break;
		case 2:
			blink_t_ -= dt;
			if (blink_t_ <= 0.0f) {
				blink_phase_ = 3;
			}
			break;
		default:
			blink_raw_ -= dt / OPEN_TIME;
			if (blink_raw_ <= 0.0f) {
				blink_raw_ = 0.0f;
				blink_phase_ = 0;
				blink_second_ = !blink_second_ && rng_.chance(DOUBLE_CHANCE);
				blink_next_ = blink_second_ ? rng_.range(0.07f, 0.15f) : rng_.range(GAP_MIN, GAP_MAX);
			}
			break;
	}
	closed_ = smoothstep01(blink_raw_);

	// --- Eyes: lids, and where the irises sit.
	const float wide = wide_.step(saturate(want.wide), 7.0f, 0.5f, dt);
	const float squint = saturate(squint_.step(saturate(want.squint), 6.0f, 0.7f, dt));
	// Standing about, the eyes flick to something else every so often and come back.
	dart_next_ -= dt;
	if (dart_next_ <= 0.0f) {
		if (want.lively && dart_.length_squared() < 1e-6f && rng_.chance(0.7f)) {
			dart_ = Vector2(rng_.range(-0.2f, 0.2f), rng_.range(-0.07f, 0.1f));
			dart_next_ = rng_.range(0.25f, 0.6f);
		} else {
			dart_ = Vector2();
			dart_next_ = rng_.range(0.7f, 1.9f);
		}
	}
	if (!want.lively) {
		dart_ = Vector2();
	}
	// Eyes move in quick jumps: a stiff follow, no drift.
	const float gx = gaze_x_.follow(clampf(want.gaze.x + dart_.x, -EYE_YAW, EYE_YAW), 0.028f, dt);
	const float gy = gaze_y_.follow(clampf(want.gaze.y + dart_.y, -EYE_PITCH, EYE_PITCH), 0.028f, dt);
	const Vector3 slide(gx / EYE_YAW * IRIS_X, -gy / EYE_PITCH * IRIS_Y, 0.0f);
	for (int i = 0; i < 2; ++i) {
		const bool shut = lash_[i] && closed_ > 0.6f;
		if (eye_[i]) {
			const float sy = lerp(1.0f, 0.1f, closed_) * lerp(1.0f, 0.62f, squint) * (1.0f + 0.1f * wide);
			const float sx = 1.0f + 0.05f * wide + 0.03f * squint;
			eye_[i]->set_transform(Transform3D(eye_rest_[i].basis.scaled_local(Vector3(sx, sy, 1.0f)), eye_rest_[i].origin));
			// The shut eye (alt_lash_*) is a lid lying flat on the face: the squashed eyeball must not poke through it.
			eye_[i]->set_visible(!shut);
		}
		if (lash_[i]) {
			lash_[i]->set_visible(shut);
		}
		if (iris_[i]) {
			iris_[i]->set_transform(Transform3D(iris_rest_[i].basis, iris_rest_[i].origin + iris_rest_[i].basis.xform(slide)));
		}
	}

	// --- Brows: a springy raise and tilt (they overshoot a little, like everything on a kid's face).
	const float up = brow_up_.step(clampf(want.brow_up + 0.35f * want.wide - 0.3f * want.squint, -1.0f, 1.0f), 5.5f, 0.5f, dt);
	const float tilt = brow_tilt_.step(clampf(want.brow_tilt, -1.0f, 1.0f), 5.5f, 0.55f, dt);
	for (int i = 0; i < 2; ++i) {
		if (!brow_[i]) {
			continue;
		}
		const float side = i == 0 ? 1.0f : -1.0f; // the left brow sits at +X
		const Vector3 off(-side * BROW_IN * saturate(tilt), up > 0.0f ? up * BROW_UP : up * BROW_DOWN, 0.0f);
		const Basis b = Basis(Vector3(0, 0, 1), side * tilt * BROW_TILT) * brow_rest_[i].basis;
		brow_[i]->set_transform(Transform3D(b, brow_rest_[i].origin + off));
	}

	// --- Mouth: one shape shown; a change squashes the old one for a frame or two and pops the new one in.
	int target = want.mouth >= 0 && want.mouth < KidFaceWant::MOUTH_COUNT && mouth_[want.mouth] ? want.mouth : KidFaceWant::SMILE;
	mouth_age_ += dt;
	if (target != mouth_now_ && mouth_age_ > 0.1f) {
		mouth_now_ = target;
		mouth_age_ = 0.0f;
		pop_.x = 0.55f;
		pop_.v = 9.0f;
	}
	const float pop = clampf(pop_.step(1.0f, 7.5f, 0.42f, dt), 0.4f, 1.35f);
	for (int m = 0; m < KidFaceWant::MOUTH_COUNT; ++m) {
		if (!mouth_[m]) {
			continue;
		}
		const bool on = m == mouth_now_ || (!mouth_[mouth_now_] && m == KidFaceWant::SMILE);
		mouth_[m]->set_visible(on);
		if (on) {
			// The pop is mostly in height: the mouth snaps open rather than growing.
			mouth_[m]->set_transform(Transform3D(mouth_rest_[m].basis.scaled_local(Vector3(lerp(1.0f, pop, 0.5f), pop, 1.0f)), mouth_rest_[m].origin));
		}
	}
}

} // namespace tot
