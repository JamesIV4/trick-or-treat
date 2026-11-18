#pragma once
// Streams the infinite world around the player: rolling-hill terrain chunks with collision,
// country roads with lantern posts, woods (pines, spooky trees, rocks, glowing mushrooms), a
// low-detail horizon ring, and whole neighborhoods and towns that are generated when the player
// comes near and freed when they leave. Per-area progress survives unloading.
//
// Area preloading (art/team/world/WORLD_REWORK.md, "Area preloading"): settlements near the player are
// planned on worker threads (plan_settlement: pure data), built on the main thread a few milliseconds
// per frame, hidden, and shown and attached (on_area_loaded) when the player comes within AREA_LOAD.
// Terrain chunks and the horizon ring compute their data on a worker too. --no-preload: the old path.

#include <godot_cpp/classes/material.hpp>
#include <godot_cpp/classes/mesh_instance3d.hpp>
#include <godot_cpp/classes/node3d.hpp>
#include <godot_cpp/variant/packed_float32_array.hpp>
#include <godot_cpp/variant/packed_vector3_array.hpp>

#include <functional>
#include <map>
#include <memory>
#include <unordered_map>
#include <vector>

#include "world/forest.h"
#include "world/world_plan.h"

namespace tot {

class MeshBuilder;
class NeighborhoodGenerator;
struct GenContext;

// Progress in one settlement, kept while it is unloaded.
struct AreaState {
	bool visited = false;
	bool freed = false; // every infestation cleared: the travel point works
	bool complete = false; // all clear: freed, and every house trick-or-treated and cleared (announced once)
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
	// What on_area_prepare's worker part made for this area (null if none or it ran out of time); the
	// game takes it in on_area_loaded.
	std::shared_ptr<void> prepared;
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
	AreaState &state_of(uint64_t key) { return states_[key]; }
	// Saved games: every settlement's progress, keyed by area key (as a decimal string). Import after
	// setup() and before the first update() so the areas load with it.
	godot::Dictionary export_states() const;
	void import_states(const godot::Dictionary &d);
	static uint64_t area_key(int i, int j) { return (uint64_t(uint32_t(i)) << 32) | uint64_t(uint32_t(j)); }
	godot::Vector3 moon_direction() const { return moon_dir_; }
	int chunk_count() const { return int(chunks_.size()); }

	std::function<void(Area &)> on_area_loaded;
	std::function<void(Area &)> on_area_unloading;
	// Optional: the rest of the game's work for a just-loaded area (monster nests and vines), called after
	// on_area_loaded on the next frames until it returns true; it stops once `deadline_us`
	// (NeighborhoodGenerator::build_clock_us(); 0 = finish now) passes. Blocking updates finish it at once.
	std::function<bool(Area &, uint64_t deadline_us)> on_area_loaded_step;
	// Optional: game work to do ahead for a settlement (monster nav grid and nest layout). Called on the
	// main thread when the area's plan starts; the function it returns runs on the worker thread right
	// after the settlement plan (pure data only: no nodes, resources or engine singletons) and its result
	// lands in Area::prepared.
	using PrepareFn = std::function<std::shared_ptr<void>(const GenContext &ctx)>;
	std::function<PrepareFn(const RegionPlan &plan)> on_area_prepare;

	void _notification(int p_what);
	~WorldStreamer();

protected:
	static void _bind_methods();

private:
	// ------------------------------------------------------------ timing report
	// Main-thread cost of streaming, printed as "STREAM_STATS" lines on exit with --fly-test or --stream-stats.
	struct StreamStats {
		std::vector<float> frame_ms;    // whole update() per non-blocking frame
		std::vector<float> interval_ms; // whole frame (update to update), rendering and the game included
		std::vector<float> after_attach_ms; // the same, the 120 frames after an area attached
		std::vector<float> region_ms;   // WorldPlan region lookups per frame (first touch plans a settlement's street)
		std::vector<float> chunk_ms;    // main-thread ms per terrain chunk built
		std::vector<float> horizon_ms;  // main-thread ms per horizon rebuild
		std::vector<float> attach_ms;   // per area made live: worst single frame of its reveal + attach
		std::vector<float> attach_total_ms; // per area: all main-thread ms of its reveal + attach
		std::vector<int> attach_frames;
		std::vector<float> callback_ms; // on_area_loaded alone
		std::vector<float> loaded_step_ms; // per on_area_loaded_step call (the game's sliced nests and vines)
		std::vector<float> gen_ms;      // old path: generate_settlement on the main thread
		std::vector<float> plan_ms;     // preload: worker wall ms per settlement plan (+ prepare)
		std::vector<float> build_ms;    // preload: main-thread ms per settlement build (all its slices)
		std::vector<int> build_frames;
		std::vector<float> chunk_job_ms; // preload: worker ms per chunk's data
		std::vector<float> adopt_ms;     // preload: copying the worker's planned regions into the main plan
		int64_t compilations[4] = {};   // pipeline compilations so far: draw (stalls), surface, mesh, specialization
		int long_frames = 0, long_streaming = 0; // frames over 33 ms; of them, those streaming spent >= 5 ms in
		int fallbacks = 0;              // an area needed before its preload was ready
		int chunk_fallbacks = 0;        // a chunk next to the player built synchronously
		int reused = 0;                 // areas re-attached from the cache (no plan, no build)
		int blocking_loads = 0;
	};
	StreamStats stats_;
	int since_attach_ = 1 << 20;
	bool blocking_now_ = false;
	bool fps_cap_applied_ = false;
	void print_stats() const;
	godot::String stream_state() const;
	void probe_area(NeighborhoodGenerator *gen);
	std::shared_ptr<class SweepDump> verify_; // shared_ptr: fine with the incomplete type here
	void verify_area(const Area &a);
	bool state_test_ = false;
	int state_test_frame_ = 0, state_test_attaches_ = 0, state_test_failures_ = 0;
	godot::Vector3 state_test_focus_;
	void state_test_step();
	void state_test_check(const Area &a);

	// ------------------------------------------------------------ terrain
	struct Chunk {
		godot::Node3D *root = nullptr;
	};
	// A chunk's data (heights, ground and road mesh, collision faces, posts, dressing): no nodes, so a
	// worker thread can make it. finish_chunk() makes the nodes.
	struct ChunkData;
	struct ChunkJob;
	struct HorizonJob;
	void make_chunk_data(WorldPlan &plan, ChunkData &d) const;
	void finish_chunk(ChunkData &d);
	void build_chunk(int cx, int cz);
	void build_horizon(const godot::Vector3 &center);
	void build_signpost(MeshBuilder &m, godot::Node3D *root, const RegionPlan &r);
	static uint64_t ckey(int x, int z) { return (uint64_t(uint32_t(x)) << 32) | uint64_t(uint32_t(z)); }

	// ------------------------------------------------------------ settlements
	void load_area(const RegionPlan &r); // the old synchronous path (--no-preload)
	void unload_area(uint64_t key);

	struct AreaJob;
	struct PrepJob;
	// A settlement on its way in (or kept for coming back): planning on a worker, building on the main
	// thread (hidden, in the tree), built, showing, or hiding again after being left.
	struct Pre {
		enum Stage { PLANNING, PLANNED, BUILDING, READY, REVEALING, APPLYING, CONCEALING };
		Stage stage = PLANNING;
		RegionPlan plan;
		AreaJob *job = nullptr;
		int64_t task = -1;
		PrepJob *prep = nullptr; // a cached settlement's prepare step, on a worker
		int64_t prep_task = -1;
		NeighborhoodGenerator *gen = nullptr;
		std::shared_ptr<void> prepared;
		std::vector<char> filter; // the candy filter the build used
		bool reused = false;      // built for an earlier visit (the state is on the nodes already)
		size_t apply_i = 0;
		float score = 0.0f;
		uint64_t queued_us = 0;
		float plan_ms = 0.0f, build_ms = 0.0f, attach_ms = 0.0f, attach_worst = 0.0f;
		int build_frames = 0, attach_frames = 0;
	};
	std::map<uint64_t, Pre> pre_; // ordered: deterministic iteration
	std::vector<uint64_t> finishing_; // attached areas whose on_area_loaded_step is not done
	void finishing_update(uint64_t deadline);
	std::vector<godot::Node *> dying_; // dropped settlements being freed in pieces
	void free_update(uint64_t deadline); // 0: everything now
	bool preload_ = true;
	int plan_tasks_ = 2, cache_ = 4;
	float preload_dist_ = 260.0f;
	godot::Vector3 last_focus_;
	godot::Vector3 heading_;
	uint64_t last_update_us_ = 0, last_spent_us_ = 0;
	float game_ms_ema_ = 8.0f;
	static constexpr int OWN_RING = 128;
	float own_ms_ring_[OWN_RING] = {};
	int own_ring_i_ = 0;
	int peak_planning_ = 0;
	int64_t budget_us_ = 3000;

	void start_plan(const RegionPlan &r, float score);
	bool poll_plan(Pre &p, bool wait);
	void begin_area_build(uint64_t key, Pre &p);
	void finish_now(uint64_t key, Pre &p, const char *why);
	void attach_area(uint64_t key, Pre &p);
	void detach_area(uint64_t key);
	void drop_pre(uint64_t key);
	void wait_prep(Pre &p);
	void wait_all();
	void preload_update(const godot::Vector3 &focus, int fi, int fj, uint64_t deadline);
	float area_score(const RegionPlan &r, const godot::Vector3 &focus) const;

	ChunkJob *chunk_job_ = nullptr; // at most one in flight (it owns chunk_plan_ while it runs)
	int64_t chunk_task_ = -1;
	std::unique_ptr<WorldPlan> chunk_plan_; // the workers' own copy of the world plan (it caches as it goes)
	int adopted_regions_ = 0;
	struct RegionJob;
	std::unique_ptr<WorldPlan> region_plan_; // regions planned ahead (RegionJob), copied into plan_ and chunk_plan_
	RegionJob *region_job_ = nullptr;
	int64_t region_task_ = -1;
	int warmed_i_ = 1 << 30, warmed_j_ = 0;
	bool chunk_adopt_pending_ = false;
	void regions_update(int fi, int fj, bool blocking);
	HorizonJob *horizon_job_ = nullptr;
	int64_t horizon_task_ = -1;
	void chunks_update(const godot::Vector3 &focus, bool blocking, uint64_t deadline);
	void horizon_update(const godot::Vector3 &focus, bool blocking);

	WorldPlan plan_;
	NeighborhoodGenerator *templ_ = nullptr;
	godot::Ref<godot::Material> toon_, grass_;
	ForestKit kit_;
	godot::Ref<godot::Mesh> maples_[3];
	godot::Ref<godot::Mesh> grass_mesh_;
	godot::Ref<godot::Mesh> leaf_patch_; // fallen leaves under trees and along the road edges
	std::unordered_map<uint64_t, Chunk> chunks_;
	std::unordered_map<uint64_t, Area> areas_;
	std::unordered_map<uint64_t, AreaState> states_;
	godot::MeshInstance3D *horizon_ = nullptr;
	godot::Vector3 horizon_center_ = godot::Vector3(1e9f, 0, 0);
	godot::Vector3 moon_dir_ = godot::Vector3(0.35f, 0.3f, -0.88f).normalized();
};

} // namespace tot
