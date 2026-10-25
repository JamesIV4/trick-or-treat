#pragma once
// The Pumpkin Cruiser: a kid's battery ride-on car the dragon kid can magically summon once it is
// bought. Point-and-go arcade driving (it turns toward the stick direction, relative to the
// camera), a hop on jump, chunky suspension tilt on the hills, spinning wheels and headlights.

#include <godot_cpp/classes/character_body3d.hpp>
#include <godot_cpp/classes/material.hpp>
#include <godot_cpp/classes/mesh_instance3d.hpp>
#include <godot_cpp/classes/spot_light3d.hpp>

namespace tot {

class RideOnCar : public godot::CharacterBody3D {
	GDCLASS(RideOnCar, godot::CharacterBody3D)

public:
	void build(const godot::Ref<godot::Material> &toon);
	void _physics_process(double delta) override;
	void _process(double delta) override;
	void place(const godot::Vector3 &pos, float yaw);
	void set_driving(bool on);
	bool driving() const { return driving_; }
	godot::Transform3D seat() const;
	float speed() const { return speed_; }
	float yaw() const { return yaw_; }
	godot::Vector3 forward() const;

protected:
	static void _bind_methods();

private:
	godot::Node3D *visual_ = nullptr;
	godot::MeshInstance3D *wheels_[4] = {};
	godot::SpotLight3D *lamp_ = nullptr;
	bool driving_ = false;
	float yaw_ = 0.0f, speed_ = 0.0f, wheel_spin_ = 0.0f, bounce_ = 0.0f, bounce_v_ = 0.0f;
	godot::Basis tilt_;
	bool was_floor_ = true;
};

} // namespace tot
