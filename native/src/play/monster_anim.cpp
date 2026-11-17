#include "play/monster_anim.h"

#include <godot_cpp/core/math.hpp>
#include <godot_cpp/variant/utility_functions.hpp>

#include <cmath>

#include "play/monsters.h"

using namespace godot;

namespace tot {

// ================================================================ registry
namespace {
MonsterAnimatorFactory *registry() {
	static MonsterAnimatorFactory table[int(MonsterType::Count)] = {};
	return table;
}

// The original monster animation, moved here unchanged from Monster::_process: a whole-body pop on
// spawn, squash and spin on death, a squashing shake while winding up, a walking bob, and the two
// swinging parts (wings, arms, legs) of a v1 model. Used for every rig_version 1 monster, and as the
// stand-in for a v2 model whose own animator is not written yet (its extra parts stay at rest).
class LegacyAnimator : public MonsterAnimator {
public:
	void bind(MonsterRig &rig) override {
		part_a_ = rig.slot("part_a");
		part_b_ = rig.slot("part_b");
	}
	bool distance_lod() const override { return false; } // exactly as before, at any distance

	void update(const AnimInput &in, MonsterRig &rig, Transform3D &root) override {
		using State = MonsterAnimState;
		const MonsterTuning &t = *in.tuning;
		const float speed = in.speed;
		float s = 1.0f, sy = 1.0f, bob = 0.0f, tilt = 0.0f, spin = 0.0f;
		if (in.state == State::Rise) {
			const float k = Math::clamp(in.state_t / rise_time(), 0.0f, 1.0f);
			s = k < 0.7f ? k / 0.7f * 1.15f : Math::lerp(1.15f, 1.0f, (k - 0.7f) / 0.3f);
			sy = s;
		} else if (in.state == State::Dying) {
			const float k = Math::clamp(in.state_t / death_time(), 0.0f, 1.0f);
			s = 1.0f + k * 0.4f;
			sy = Math::max(0.01f, 1.0f - k);
			spin = k * 6.0f;
		} else if (in.state == State::Windup) {
			const float k = Math::clamp(in.state_t / t.windup, 0.0f, 1.0f);
			const float shake = std::sin(in.t * 60.0f) * 0.05f * k;
			s = 1.0f + k * (t.attack == MonsterTuning::PUFF ? 0.35f : 0.12f);
			sy = 1.0f - k * 0.18f + shake;
			tilt = t.attack == MonsterTuning::CHARGE ? 0.25f * k : -0.15f * k;
		} else if (in.state == State::Recover && t.attack == MonsterTuning::CHARGE && in.dizzy > 0.0f) {
			spin = in.t * 8.0f; // dizzy after a charge
		}
		if (!t.flying && in.state != State::Dying) {
			bob = in.on_floor ? std::abs(std::sin(in.t * (6.0f + speed * 2.0f))) * 0.06f * Math::min(speed, 1.0f) : 0.0f;
			if (t.hops && !in.on_floor) {
				sy *= 1.12f;
				s *= 0.94f;
			}
		}
		if (in.type == MonsterType::Ghost) {
			tilt += std::sin(in.t * 1.7f) * 0.08f;
		}
		root = Transform3D(Basis(Vector3(0, 1, 0), in.yaw + spin) * Basis(Vector3(1, 0, 0), tilt).scaled(Vector3(s, sy, s)), Vector3(0, bob, 0));
		// Limbs.
		const float swing = std::sin(in.t * (5.0f + speed * 2.2f));
		switch (in.type) {
			case MonsterType::Bat: {
				const float flap = std::sin(in.t * (in.state == State::Attack ? 26.0f : 16.0f)) * 0.8f;
				rig.pose(part_a_, Basis(Vector3(0, 0, 1), -flap));
				rig.pose(part_b_, Basis(Vector3(0, 0, 1), flap));
				break;
			}
			case MonsterType::Spider:
				rig.pose(part_a_, Basis(Vector3(0, 1, 0), swing * 0.25f * Math::min(speed, 1.5f)));
				rig.pose(part_b_, Basis(Vector3(0, 1, 0), -swing * 0.25f * Math::min(speed, 1.5f)));
				break;
			case MonsterType::Mummy:
			case MonsterType::Scarecrow: {
				const float raise = in.state == State::Windup || in.state == State::Attack ? -0.5f : 0.0f;
				rig.pose(part_a_, Basis(Vector3(1, 0, 0), raise + swing * 0.15f));
				rig.pose(part_b_, Basis(Vector3(1, 0, 0), raise - swing * 0.15f));
				break;
			}
			default:
				rig.pose(part_a_, Basis(Vector3(1, 0, 0), swing * 0.6f * Math::min(speed, 1.0f)));
				rig.pose(part_b_, Basis(Vector3(1, 0, 0), -swing * 0.6f * Math::min(speed, 1.0f)));
		}
	}

private:
	int part_a_ = -1, part_b_ = -1;
};

// Peak overshoot of an underdamped spring kicked from rest, as a fraction of v / w: a kick of
// peak * w / this swings out to exactly `peak`.
float kick_peak_factor(float zeta) {
	const float z = Math::clamp(zeta, 0.01f, 0.99f);
	const float r = std::sqrt(1.0f - z * z);
	return std::exp(-z * std::atan2(r, z) / r);
}
} // namespace

bool register_monster_animator(MonsterType type, MonsterAnimatorFactory make) {
	if (int(type) >= 0 && int(type) < int(MonsterType::Count)) {
		registry()[int(type)] = make;
	}
	return true;
}

MonsterAnimator *make_animator(MonsterType type, int rig_version) {
	if (rig_version >= 2) {
		if (MonsterAnimatorFactory make = registry()[int(type)]) {
			return make();
		}
		static bool warned[int(MonsterType::Count)] = {};
		if (!warned[int(type)]) {
			warned[int(type)] = true;
			UtilityFunctions::push_warning("Trick or Treat: ", monster_name(type), " has a v2 model but no animator registered; using the legacy one.");
		}
	}
	return new LegacyAnimator();
}

// ================================================================ shared helpers
Transform3D root_pose(float yaw, const Vector3 &lean, const Vector3 &scale, const Vector3 &offset) {
	Basis b = Basis(anim::quat_from_rotvec(lean));
	if (scale != Vector3(1, 1, 1)) {
		b = b.scaled(scale);
	}
	return Transform3D(Basis(Vector3(0, 1, 0), yaw) * b, offset);
}

// ---------------------------------------------------------------- MonsterFace
void MonsterFace::add_eye(MonsterRig &rig, const char *eye, const char *pupil, bool follows) {
	if (n_ >= MAX_EYES) {
		return;
	}
	const int e = rig.slot(StringName(eye));
	if (e < 0) {
		return;
	}
	eye_[n_] = e;
	pupil_[n_] = pupil && pupil[0] ? rig.slot(StringName(pupil)) : -1;
	follows_[n_] = follows;
	++n_;
}

void MonsterFace::bind(MonsterRig &rig, uint32_t seed) {
	seed_ = seed;
	n_ = 0;
	head_ = rig.slot("head");
	add_eye(rig, "eye_l", "pupil_l");
	add_eye(rig, "eye_r", "pupil_r");
	add_eye(rig, "eye", "pupil");
	static const char *small[] = { "eye_s0", "eye_s1", "eye_s2", "eye_s3", "eye_s4", "eye_s5" };
	for (const char *s : small) {
		add_eye(rig, s, "", false);
	}
}

void MonsterFace::update(const AnimInput &in, MonsterRig &rig) {
	using State = MonsterAnimState;
	if (!seeded_) {
		seeded_ = true;
		blink_ = anim::Blink(in.seed ^ (seed_ * 0x9E3779B9u) ^ 0x51ED27u);
		blink_.min_gap = 1.8f;
		blink_.max_gap = 5.0f;
		if (blink_speed > 0.0f && blink_speed != 1.0f) {
			blink_.close_time /= blink_speed;
			blink_.hold_time /= blink_speed;
			blink_.open_time /= blink_speed;
		}
	}
	if (wake_hold > 0.0f && in.state == State::Rise) {
		blink_.hold_open(wake_hold); // renewed every frame of the spawn: counts down once it is over
	}
	// A hit squeezes the eyes shut for a moment.
	if (in.since_hit < last_since_hit_) {
		blink_.force(0.22f);
	}
	last_since_hit_ = in.since_hit;
	const bool far = in.lod > 0;
	float c = far ? 0.0f : blink_.step(in.dt);
	if (blink_.is_blinking() && !was_blinking_) {
		ripple_t_ = 0.0f;
	}
	was_blinking_ = blink_.is_blinking();
	ripple_t_ += in.dt;
	// (final fixes 2) `shut` closes at once but opens at most 1 / open_time per second: the eyes never snap open
	// (bat, scarecrow, toadstool after the rise; the ghost's happy face).
	const float want_shut = anim::saturate(shut);
	if (far || open_time <= 0.0f || want_shut >= shut_eased_) {
		shut_eased_ = want_shut;
	} else {
		shut_eased_ = Math::max(want_shut, shut_eased_ - in.dt / open_time);
	}
	const float forced = shut_eased_;
	closed = Math::max(c, forced);

	// Where to look, in model space (the facing frame: immune to the root's squash and spin).
	float w = look_weight >= 0.0f ? anim::saturate(look_weight) : (in.chasing ? 1.0f : 0.0f);
	if (in.state == State::Rise || in.state == State::Dying || far) {
		w = 0.0f;
	}
	const Vector3 target = in.player_local + Vector3(0, look_height, 0);
	if (far) {
		look_.reset();
		head = Vector2();
	} else if (head_ >= 0) {
		const Transform3D hr = rig.model_rest(head_);
		head = look_.step(hr.basis.xform_inv(target - hr.origin), max_yaw, max_pitch, w, head_lag, in.dt);
	}

	// Eyes: lids, pupils.
	const float k = far ? 1.0f : anim::damp_factor(pupil_lag, in.dt);
	int small_i = 0;
	for (int i = 0; i < n_; ++i) {
		float ci = closed;
		if (!follows_[i]) {
			// The small eyes blink one after another.
			ci = far ? forced : Math::max(anim::smooth_pulse(ripple_t_ - 0.05f * float(++small_i), 0.09f, 0.03f, 0.14f), forced);
		}
		rig.pose(eye_[i], Basis(), Vector3(), Vector3(wide, wide * anim::lerp(1.0f, closed_scale, ci), wide));
		if (pupil_[i] >= 0 && follows_[i]) {
			Vector2 want;
			if (w > 0.0f) {
				want = anim::pupil_offset(rig.model_rest(eye_[i]), target, 1.0f) * w;
			}
			slide_[i] += (want - slide_[i]) * k;
			const float r = pupil_travel * rig.reach(eye_[i]);
			rig.pose(pupil_[i], Basis(), Vector3(slide_[i].x * r, slide_[i].y * r, 0.0f));
		}
	}

	// Eye glow: dim at rest so the pupils read, flaring through the wind-up and the attack.
	if (glow_rest >= 0.0f) {
		float want = glow_rest;
		if (in.state == State::Windup) {
			want = anim::lerp(glow_rest, glow_flare, anim::smoothstep01(in.windup));
		} else if (in.state == State::Attack) {
			want = glow_flare;
		}
		glow_ = glow_ < 0.0f || far ? want : anim::damp(glow_, want, 0.05f, in.dt);
		if (std::fabs(glow_ - glow_sent_) > 0.01f) {
			glow_sent_ = glow_;
			for (int i = 0; i < n_; ++i) {
				rig.glow(eye_[i], glow_);
				rig.glow(pupil_[i], glow_);
			}
		}
	}
}

// ---------------------------------------------------------------- HitRecoil
Vector3 HitRecoil::update(const AnimInput &in) {
	fresh = in.since_hit < last_since_hit_;
	last_since_hit_ = in.since_hit;
	const Basis facing(Vector3(0, 1, 0), in.yaw);
	if (fresh) {
		// Tip the top the way the blow pushed: a turn about the horizontal axis square to it. The
		// lean is kept in world space, because a knocked monster turns to face the way it is thrown
		// while the flinch is still playing.
		const Vector3 d = facing.xform(in.hit_dir_local);
		lean_.kick(Vector3(d.z, 0.0f, -d.x) * (angle * anim::TAU * hz / kick_peak_factor(zeta)));
	}
	ouch = 1.0f - anim::smoothstep01(in.since_hit / 0.35f);
	return facing.xform_inv(lean_.step(Vector3(), hz, zeta, in.dt));
}

// ---------------------------------------------------------------- Gait
float Gait::step(const AnimInput &in, float stride, float full_speed) {
	const bool grounded = in.on_floor || (in.tuning && in.tuning->flying);
	if (grounded) {
		anim::gait_phase(phase, in.speed, stride, in.dt);
	}
	const float want = grounded && full_speed > 0.0f ? anim::saturate(in.speed / full_speed) : 0.0f;
	amount = in.lod > 0 ? want : anim::damp(amount, want, 0.08f, in.dt);
	return phase;
}

// ---------------------------------------------------------------- RootSquash
Vector3 RootSquash::update(const AnimInput &in, float target) {
	landed = false;
	if (!primed_) {
		primed_ = true;
		was_floor_ = in.on_floor;
		s_.reset(target - 1.0f);
	}
	if (!in.on_floor) {
		fall_ = in.vel_local.y;
	} else if (!was_floor_) {
		landed = true;
		if (fall_ < 0.0f) {
			s_.kick(fall_ * land_gain * anim::TAU * hz / kick_peak_factor(zeta));
		}
		fall_ = 0.0f;
	}
	was_floor_ = in.on_floor;
	if (in.lod > 0) {
		s_.reset(target - 1.0f);
	} else {
		s_.step(target - 1.0f, hz, zeta, in.dt);
	}
	return anim::squash_stretch(Math::clamp(1.0f + s_.x, min, max));
}

} // namespace tot
