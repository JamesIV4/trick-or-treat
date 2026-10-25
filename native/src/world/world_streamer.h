#pragma once
// Streams the infinite world around the player: rolling-hill terrain chunks with collision,
// country roads with lantern posts, woods (pines, spooky trees, rocks, glowing mushrooms), a
// low-detail horizon ring, and whole neighborhoods and towns that are generated when the player
// comes near and freed when they leave. Per-area progress survives unloading.

#include <godot_cpp/classes/material.hpp>
#include <godot_cpp/classes/mesh_instance3d.hpp>
#include <godot_cpp/classes/node3d.hpp>

#include <functional>
#include <unordered_map>
#include <vector>

#include "world/forest.h"
#include "world/world_plan.h"

namespace tot {

class NeighborhoodGenerator;

// Progress in one settlement, kept while it is unloaded.
struct AreaState {
	bool visited = false;
	bool freed = false; // every infestation cleared: the travel point works
	int candy_total = 0, candy_collected = 0;
	std::vector<char> candy_taken;
	std::vector<char> rung;
	std::vector<char> lot_cleared;
	std::vector<char> spawner_down;
	std::vector<char> vine_down;
	std::vector<char> heart_down;
};

struct Area {
	uint64_t key = 0;
	RegionPlan plan;
	NeighborhoodGenerator *gen = nullptr;
	AreaState *state = nullptr;
	godot::Node3D *extras = nullptr; // monsters, spawners, vines, travel point (owned by the game)
	godot::Vector3 to_local(const godot::Vector3 &p) const { return p - plan.origin; }
	godot::Vector3 to_world(const godot::Vector3 &p) const { return p + plan.origin; }
};

class WorldStreamer : public godot::Node3D {
	GDCLASS(WorldStreamer, godot::Node3D)

public:
	static constexpr float CHUNK = 48.0f;
	static constexpr float STEP = 2.0f;

	// `templ` provides the materials and the loaded asset library.
	void setup(NeighborhoodGenerator *templ, uint64_t seed);
	// Streams around `focus`; with `blocking` everything in range is built before returning.
	void update(const godot::Vector3 &focus, bool blocking = false);
	WorldPlan &plan() { return plan_; }
	float ground(float x, float z) { return plan_.height(x, z); }
	std::vector<Area *> areas();
	Area *area(uint64_t key);
	// The loaded settlement whose footprint (grown by `margin`) contains p.
	Area *area_at(const godot::Vector3 &p, float margin = 0.0f);
	AreaState &state(int i, int j);
	static uint64_t area_key(int i, int j) { return (uint64_t(uint32_t(i)) << 32) | uint64_t(uint32_t(j)); }
	godot::Vector3 moon_direction() const { return moon_dir_; }
	int chunk_count() const { return int(chunks_.size()); }

	std::function<void(Area &)> on_area_loaded;
	std::function<void(Area &)> on_area_unloading;

protected:
	static void _bind_methods();

private:
	struct Chunk {
		godot::Node3D *root = nullptr;
	};
	void build_chunk(int cx, int cz);
	void build_horizon(const godot::Vector3 &center);
	void load_area(const RegionPlan &r);
	void unload_area(uint64_t key);
	static uint64_t ckey(int x, int z) { return (uint64_t(uint32_t(x)) << 32) | uint64_t(uint32_t(z)); }

	WorldPlan plan_;
	NeighborhoodGenerator *templ_ = nullptr;
	godot::Ref<godot::Material> toon_, grass_;
	ForestKit kit_;
	godot::Ref<godot::Mesh> maples_[3];
	godot::Ref<godot::Mesh> grass_mesh_;
	std::unordered_map<uint64_t, Chunk> chunks_;
	std::unordered_map<uint64_t, Area> areas_;
	std::unordered_map<uint64_t, AreaState> states_;
	godot::MeshInstance3D *horizon_ = nullptr;
	godot::Vector3 horizon_center_ = godot::Vector3(1e9f, 0, 0);
	godot::Vector3 moon_dir_ = godot::Vector3(0.35f, 0.3f, -0.88f).normalized();
};

} // namespace tot
