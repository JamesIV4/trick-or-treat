#pragma once
// The monster roster's look: ten cute-spooky creatures, the nest each one crawls out of, and the
// big thorn vines. Built procedurally with the shared palette and painted materials. Bodies stand
// on the origin facing +Z; parts (wings, arms, legs) swing around their pivots.

#include <godot_cpp/classes/array_mesh.hpp>
#include <godot_cpp/variant/vector3.hpp>

#include <vector>

#include "gen/rig_part.h"

namespace tot {

class AssetLibrary;

enum class MonsterType { PumpkinHopper, Bat, Skeleton, Ghost, Gremlin, Mummy, Slime, Scarecrow, Spider, Toadstool, Count };

const char *monster_name(MonsterType t);
const char *nest_name(MonsterType t);

// The Pumpkin Hopper keeps its procedural look until a v2 Blender model exists (the user preferred the
// procedural one to the first Blender version). With this on, apply_assets swaps in monster_pumpkin
// only when its glTF is a v2 rig (named parts beyond body / part_a / part_b); the old v1 glTF is never
// used. Set it to false to go back to the procedural pumpkin whatever is published.
constexpr bool PUMPKIN_HOPPER_USES_V2_MODEL = true;

struct MonsterArt {
	// The procedural model (and what a v1 glTF swaps in): a body and up to two swinging parts.
	godot::Ref<godot::ArrayMesh> body, part_a, part_b, nest;
	godot::Vector3 pivot_a, pivot_b;
	// What Monster actually builds, for every monster: the rig as a parent-first part list (see
	// gen/rig_part.h). Version 1 = body / part_a / part_b (procedural or a v1 glTF, animated by
	// LegacyAnimator); version 2 = a glTF with more named parts (animated by that monster's own animator).
	std::vector<RigPart> parts;
	std::vector<RigSocket> sockets;
	int rig_version = 1;
	// Collision capsule. Code-owned: set by the procedural builders, never by the art.
	float height = 1.0f, radius = 0.4f;
	float nest_radius = 1.0f, nest_height = 1.5f;
	// Fills `parts` from body / part_a / part_b and their pivots (rig_version 1).
	void set_legacy_parts();
};

struct MonsterArtKit {
	MonsterArt art[int(MonsterType::Count)];
	godot::Ref<godot::ArrayMesh> vine[3];
	// Thorn nest from the Blender root mass (null: built procedurally from a knot and a blob).
	godot::Ref<godot::ArrayMesh> root_mass, root_heart;
	godot::Vector3 root_heart_origin;
	godot::Ref<godot::ArrayMesh> gate_base, gate_lantern, shop_cart;
	void build();
	// Swap in the published Blender monsters and nests (monster_<kind> as a part tree, nest_<kind>)
	// wherever they exist; the procedural versions stay as fallbacks.
	void apply_assets(AssetLibrary &lib);
	const MonsterArt &operator[](MonsterType t) const { return art[int(t)]; }
};

// A creeping thorn vine following `pts` (relative to the first point).
godot::Ref<godot::ArrayMesh> build_tendril_mesh(const std::vector<godot::Vector3> &pts, float width, uint32_t seed);

} // namespace tot
