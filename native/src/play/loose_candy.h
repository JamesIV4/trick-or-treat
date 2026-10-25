#pragma once
// Candy that is loose in the world: what spills from the bucket when the kid gets hit (blinking
// before it disappears) and what monsters, nests and vines drop. It counts toward the candy you
// carry, never toward an area's collection goal.

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
	// Pulls in and collects pieces near the player; returns the value collected.
	int collect(const godot::Vector3 &player, float radius, float dt);
	void clear();
	int count() const { return int(pieces_.size()); }
	std::function<void(int kind, const godot::Vector3 &pos)> on_pickup;

	void _process(double delta) override;

protected:
	static void _bind_methods();

private:
	struct Piece {
		godot::MeshInstance3D *node = nullptr;
		godot::Vector3 pos, vel;
		float life = 0.0f, ttl = 8.0f;
		int kind = 0, value = 1, trail = -1;
		bool resting = false, homing = false;
	};
	std::vector<godot::Ref<godot::Mesh>> meshes_;
	godot::Ref<godot::Material> mat_;
	std::vector<Piece> pieces_;
	Rng rng_ = Rng(77);
};

} // namespace tot
