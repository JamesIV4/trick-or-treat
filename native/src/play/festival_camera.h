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
	// Street direction (look yaw) near the player; the rig picks the end the player is heading to.
	void set_guide_yaw(float yaw) { guide_ = yaw; }
	void snap();
	void shake(float amount) { shake_ = godot::Math::max(shake_, amount); }
	void set_overview(bool on) { overview_ = on; }
	void set_heading_sign(float s) { heading_sign_ = s >= 0.0f ? 1.0f : -1.0f; }
	// Points the camera's look direction (yaw) at a world direction, e.g. a house for review shots.
	void face_direction(const godot::Vector3 &dir);
	godot::Camera3D *camera() const { return cam_; }

protected:
	static void _bind_methods();

private:
	godot::Camera3D *cam_ = nullptr;
	godot::Node3D *target_ = nullptr;
	float guide_ = 0.0f;
	float heading_sign_ = 1.0f; // +1 looking along the street tangent, -1 the other way
	float yaw_ = 0.0f, user_ = 0.0f;
	float dist_ = 9.5f, pitch_ = 0.62f, arm_ = 9.5f;
	float shake_ = 0.0f, time_ = 0.0f;
	godot::Vector3 focus_;
	bool overview_ = false;
};

} // namespace tot
