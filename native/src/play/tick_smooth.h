#pragma once

#include <godot_cpp/classes/engine.hpp>
#include <godot_cpp/classes/os.hpp>
#include <godot_cpp/core/math.hpp>
#include <godot_cpp/variant/vector3.hpp>

namespace tot {

// Where to DRAW a physics body on the frame being rendered.
//
// A body only moves on physics ticks (120 a second in this project) while frames are drawn at
// whatever rate the machine manages. Drawn where physics left it, the body stands still on some
// frames and takes a double step on others (at 160 fps: no move on 30% of frames), which shows as a
// fine judder against a camera that eases every frame: about 7 px per frame at a run, measured
// with art/team/characters_candy/batch05/jitter_test.ps1 on 2026-11-15.
//
// TickSmooth remembers where the body was after each of the last two ticks and returns the point
// between them for the current frame. The drawn body is at most one tick (8 ms) behind physics.
// Use: call tick() at the end of _physics_process; in _process, move the visual by
// offset(get_global_position()) and aim cameras at position + offset. Nothing in the simulation
// changes.
struct TickSmooth {
	godot::Vector3 prev, cur;
	bool primed = false;

	// After the body has moved in a physics tick. A jump of more than 3 m is a teleport: no smear.
	void tick(const godot::Vector3 &p) {
		if (!primed || p.distance_squared_to(cur) > 9.0f) {
			prev = cur = p;
			primed = true;
		} else {
			prev = cur;
			cur = p;
		}
	}

	void snap(const godot::Vector3 &p) {
		prev = cur = p;
		primed = true;
	}

	// How far to shift the visual from the body's position `now` (world space). Zero if the body has
	// been moved since the last tick (a teleport, or something placing it every frame), so it is
	// never drawn somewhere it no longer is.
	// (Review switch: the environment variable TOT_NO_TICK_SMOOTH turns the easing off everywhere, to
	// compare with the old behaviour.)
	godot::Vector3 offset(const godot::Vector3 &now) const {
		static const bool disabled = godot::OS::get_singleton()->has_environment("TOT_NO_TICK_SMOOTH");
		if (disabled || !primed || now.distance_squared_to(cur) > 1e-8f) {
			return godot::Vector3();
		}
		const float f = godot::Math::clamp(float(godot::Engine::get_singleton()->get_physics_interpolation_fraction()), 0.0f, 1.0f);
		return (prev - cur) * (1.0f - f);
	}
};

} // namespace tot
