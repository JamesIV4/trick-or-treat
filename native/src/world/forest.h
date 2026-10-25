#pragma once
// Procedural low-poly woods kit: layered pines, bare spooky trees, bushes, mossy rocks, stumps,
// fallen logs and clusters of softly glowing mushrooms. Built once with the shared palette and
// painted materials, then instanced by the world streamer.

#include <godot_cpp/classes/array_mesh.hpp>

namespace tot {

struct ForestKit {
	enum Kind { PINE_A, PINE_B, PINE_C, SPOOKY_A, SPOOKY_B, BUSH, ROCK_A, ROCK_B, STUMP, SHROOMS, LOG, COUNT };
	godot::Ref<godot::ArrayMesh> mesh[COUNT];
	float trunk_radius[COUNT] = {};
	float trunk_height[COUNT] = {};
	void build();
};

} // namespace tot
