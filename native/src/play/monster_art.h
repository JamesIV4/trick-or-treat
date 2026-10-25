#pragma once
// The monster roster's look: ten cute-spooky creatures, the nest each one crawls out of, and the
// big thorn vines. Built procedurally with the shared palette and painted materials. Bodies stand
// on the origin facing +Z; parts (wings, arms, legs) swing around their pivots.

#include <godot_cpp/classes/array_mesh.hpp>
#include <godot_cpp/variant/vector3.hpp>

#include <vector>

namespace tot {

enum class MonsterType { PumpkinHopper, Bat, Skeleton, Ghost, Gremlin, Mummy, Slime, Scarecrow, Spider, Toadstool, Count };

const char *monster_name(MonsterType t);
const char *nest_name(MonsterType t);

struct MonsterArt {
	godot::Ref<godot::ArrayMesh> body, part_a, part_b, nest;
	godot::Vector3 pivot_a, pivot_b;
	float height = 1.0f, radius = 0.4f; // collision capsule
	float nest_radius = 1.0f, nest_height = 1.5f;
};

struct MonsterArtKit {
	MonsterArt art[int(MonsterType::Count)];
	godot::Ref<godot::ArrayMesh> vine[3];
	godot::Ref<godot::ArrayMesh> gate_base, gate_lantern, shop_cart;
	void build();
	const MonsterArt &operator[](MonsterType t) const { return art[int(t)]; }
};

// A creeping thorn vine following `pts` (relative to the first point).
godot::Ref<godot::ArrayMesh> build_tendril_mesh(const std::vector<godot::Vector3> &pts, float width, uint32_t seed);

} // namespace tot
