#pragma once
// Elevated three-quarter camera with a steady orientation: it looks diagonally down the street
// as set at the start of the night and only turns when the player orbits it (and stays there).
// Feeds the toon shader's occluder cutout so roofs and trees dither away around the player.

#include <godot_cpp/classes/camera3d.hpp>
#include <godot_cpp/classes/node3d.hpp>
#include <godot_cpp/core/math.hpp>

#include "core/consts.h"

namespace tot {

class FestivalCamera : public godot::Node3D {
	GDCLASS(FestivalCamera, godot::Node3D)

public:
	void _ready() override;
	void _process(double delta) override;

	void set_target(godot::Node3D *t) { target_ = t; }
	// The body whose movement the camera swings round behind (the car while driving); null = the target.
	void set_follow_body(godot::Node3D *b) { follow_body_ = b; }
	// Street direction (look yaw) near the player; the rig picks the end the player is heading to.
	void set_guide_yaw(float yaw) { guide_ = yaw; }
	void snap();
	void shake(float amount) { shake_ = godot::Math::max(shake_, amount); }
	void set_overview(bool on) { overview_ = on; }
	// Main-menu backdrop: the camera slowly circles the player.
	void set_attract(bool on) { attract_ = on; }
	// Trailer shots: the camera is placed directly (no rig, no occluder cutout).
	void set_cinematic(bool on) { cinematic_ = on; }
	void set_cinematic_pose(const godot::Transform3D &xf, float fov) {
		cine_xf_ = xf;
		cine_fov_ = fov;
	}
	void set_overview_distance(float d) { overview_dist_ = d; } // review aid
	void set_heading_sign(float s) { heading_sign_ = s >= 0.0f ? 1.0f : -1.0f; }
	// Points the camera's look direction (yaw) at a world direction, e.g. a house for review shots.
	void face_direction(const godot::Vector3 &dir);
	godot::Camera3D *camera() const { return cam_; }
	// Player options (the Options menu, scripts/settings.gd).
	void set_orbit_speed(float s) { orbit_speed_ = godot::Math::clamp(s, 0.2f, 3.0f); }
	void set_invert_orbit(bool on) { invert_orbit_ = on; }
	void set_auto_follow(bool on) { auto_follow_ = on; }
	void set_shake_scale(float s) { shake_scale_ = godot::Math::clamp(s, 0.0f, 1.5f); }
	void set_base_fov(float f) { base_fov_ = godot::Math::clamp(f, 45.0f, 80.0f); }

protected:
	static void _bind_methods();

private:
	godot::Camera3D *cam_ = nullptr;
	godot::Node3D *target_ = nullptr, *follow_body_ = nullptr;
	float guide_ = 0.0f;
	float heading_sign_ = 1.0f; // +1 looking along the street tangent, -1 the other way
	float yaw_ = 0.0f, user_ = 0.0f;
	float dist_ = 9.5f, pitch_ = 0.62f, arm_ = 9.5f;
	float shake_ = 0.0f, time_ = 0.0f;
	float orbit_quiet_ = 10.0f; // seconds since the player last orbited the camera
	float follow_sign_ = 0.0f; // which way the follow turn last went
	godot::Vector3 focus_;
	bool overview_ = false, attract_ = false, cinematic_ = false;
	godot::Transform3D cine_xf_;
	float cine_fov_ = 45.0f;
	float overview_dist_ = 46.0f;
	float orbit_speed_ = 1.0f, shake_scale_ = 1.0f, base_fov_ = 56.0f;
	bool invert_orbit_ = false, auto_follow_ = true;
};

} // namespace tot
