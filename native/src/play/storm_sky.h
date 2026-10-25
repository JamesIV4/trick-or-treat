#pragma once
// Cute-but-stormy sky: chunky clouds drifting on slow rings, soft lavender heat lightning inside
// them (dim, slow, capped, and switchable for accessibility), and little Vs of bats across the moon.

#include <godot_cpp/classes/directional_light3d.hpp>
#include <godot_cpp/classes/node3d.hpp>
#include <godot_cpp/classes/shader_material.hpp>

#include <vector>

#include "core/rng.h"

namespace tot {

class NeighborhoodGenerator;

class StormSky : public godot::Node3D {
	GDCLASS(StormSky, godot::Node3D)

public:
	void setup(NeighborhoodGenerator *gen, const godot::Vector3 &center, const godot::Vector3 &moon_dir, uint64_t seed);
	void _process(double delta) override;
	void set_lightning_enabled(bool e) { lightning_ = e; }
	bool get_lightning_enabled() const { return lightning_; }
	void set_flash_light(godot::DirectionalLight3D *l) { flash_light_ = l; }
	void set_sky_material(const godot::Ref<godot::ShaderMaterial> &m) { sky_mat_ = m; }
	// Clouds and bats ring this point (the player, in the open world).
	void set_center(const godot::Vector3 &c) { center_ = c; }

protected:
	static void _bind_methods();

private:
	struct Cloud {
		godot::Node3D *node;
		float angle, radius, height, speed, scale;
	};
	struct Bat {
		godot::Node3D *node;
		godot::Node3D *wing_l = nullptr, *wing_r = nullptr;
		godot::Vector3 from, to;
		float t, dur, phase;
	};
	void spawn_bats();
	std::vector<Cloud> clouds_;
	std::vector<Bat> bats_;
	NeighborhoodGenerator *gen_ = nullptr;
	godot::DirectionalLight3D *flash_light_ = nullptr;
	godot::Ref<godot::ShaderMaterial> sky_mat_;
	godot::Vector3 center_, moon_dir_;
	Rng rng_;
	bool lightning_ = true;
	float next_flash_ = 12.0f, flash_t_ = -1.0f, next_bats_ = 6.0f;
};

} // namespace tot
