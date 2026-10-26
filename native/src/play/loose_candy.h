#pragma once
// Candy that is loose in the world: what spills from the bucket when the kid gets hit (blinking
// before it disappears) and what monsters, nests and vines drop. It counts toward the candy you
// carry, never toward an area's collection goal.

#include <godot_cpp/classes/quad_mesh.hpp>
#include <godot_cpp/classes/material.hpp>
#include <godot_cpp/classes/mesh.hpp>
#include <godot_cpp/classes/mesh_instance3d.hpp>
#include <godot_cpp/classes/node3d.hpp>

#include <functional>
#include <vector>

#include "core/rng.h"

namespace tot {

class LooseCandy : public godot::Node3D {
	GDCLASS(LooseCandy, godot::Node3D)

public:
	void setup(const std::vector<godot::Ref<godot::Mesh>> &meshes, const godot::Ref<godot::Material> &mat);
	// Throws `value` worth of candy out of `from` (a handful of pieces, bigger kinds for big
	// spills). Spilled candy blinks and vanishes sooner than dropped loot.
	void spill(const godot::Vector3 &from, int value, bool spilled);
	// A brief fountain: count pieces (mostly everyday candy, a few treats) shot up one after another
	// over about a second and raining down around rom.
	void fountain(const godot::Vector3 &from, int count);
	// Pulls in and collects pieces near the player; returns the value collected.
	int collect(const godot::Vector3 &player, float radius, float dt);
	// A piece hanging in the air where it is put (trailer shots: candy trails and jump arcs).
	void hover(const godot::Vector3 &pos, int kind, float ttl);
	// How far beyond the pickup radius pieces start flying to the kid (trailer shots turn it off).
	void set_reach(float extra) { reach_extra_ = extra; }
	void clear();
	int count() const { return int(pieces_.size()); }
	std::function<void(int kind, const godot::Vector3 &pos)> on_pickup;

	void _process(double delta) override;

protected:
	static void _bind_methods();

private:
	struct Piece {
		godot::MeshInstance3D *node = nullptr;
		godot::MeshInstance3D *glow = nullptr; // the soft column of light under it
		godot::Vector3 pos, vel;
		float life = 0.0f, ttl = 8.0f, home_t = 0.0f, blink = 0.0f; // blink: phase in cycles, advanced by dt
		int kind = 0, value = 1, trail = -1;
		bool resting = false, homing = false;
	};
	std::vector<godot::Ref<godot::Mesh>> meshes_;
	godot::Ref<godot::Material> mat_;
	godot::Ref<godot::QuadMesh> beam_mesh_;
	godot::Ref<godot::Material> beam_mat_;
	void remove(size_t i);
	std::vector<Piece> pieces_;
	float reach_extra_ = 1.6f;
	struct Fountain {
		godot::Vector3 from;
		int left = 0;
		float next = 0.0f;
	};
	std::vector<Fountain> fountains_;
	void launch(const godot::Vector3 &from, int kind, const godot::Vector3 &vel, float ttl);
	Rng rng_ = Rng(77);
};

} // namespace tot
