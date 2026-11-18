#include "world/world_streamer.h"
#include "core/fonts.h"

#include <godot_cpp/classes/collision_shape3d.hpp>
#include <godot_cpp/classes/concave_polygon_shape3d.hpp>
#include <godot_cpp/classes/label3d.hpp>
#include <godot_cpp/classes/cylinder_shape3d.hpp>
#include <godot_cpp/classes/box_shape3d.hpp>
#include <godot_cpp/classes/multi_mesh.hpp>
#include <godot_cpp/classes/multi_mesh_instance3d.hpp>
#include <godot_cpp/classes/omni_light3d.hpp>
#include <godot_cpp/classes/scene_tree.hpp>
#include <godot_cpp/classes/static_body3d.hpp>
#include <godot_cpp/classes/worker_thread_pool.hpp>
#include <godot_cpp/core/class_db.hpp>
#include <godot_cpp/core/math.hpp>
#include <godot_cpp/classes/engine.hpp>
#include <godot_cpp/classes/os.hpp>
#include <godot_cpp/classes/performance.hpp>
#include <godot_cpp/classes/time.hpp>
#include <godot_cpp/variant/utility_functions.hpp>

#include <algorithm>
#include <chrono>
#include <cmath>
#include <functional>

#include "core/noise.h"
#include "gen/candy_kinds.h"
#include "gen/neighborhood_generator.h"
#include "mesh/mesh_builder.h"
#include "play/candy_field.h"
#include "play/sweep.h"

using namespace godot;

namespace tot {

namespace {
constexpr float CHUNK_LOAD = 170.0f, CHUNK_DROP = 230.0f;
constexpr float AREA_LOAD = 95.0f, AREA_DROP = 175.0f; // distance from the footprint rectangle
constexpr float HORIZON_R = 1100.0f, HORIZON_HOLE = 150.0f, HORIZON_STEP = 22.0f;
// Area preloading. A settlement whose footprint comes within PRELOAD of the player is planned on a
// worker and built hidden; a built one is kept (hidden, cached) until it is past EVICT or the cache is full.
// CACHE counts every settlement not attached (planning, building or built). Distances from the footprint.
constexpr float PRELOAD = 260.0f, EVICT = 420.0f;
constexpr int CACHE = 4, PLAN_TASKS = 2; // defaults (--preload-cache=N, --preload-tasks=N)
// A missing chunk this close to the player is built at once on the main thread (the ground under the kid).
constexpr float CHUNK_URGENT = 60.0f;

float rect_distance(const RegionPlan &r, float x, float z) {
	const float dx = Math::max(0.0f, Math::max(r.x0 - x, x - r.x1));
	const float dz = Math::max(0.0f, Math::max(r.z0 - z, z - r.z1));
	return std::sqrt(dx * dx + dz * dz);
}

uint64_t now_us() {
	return uint64_t(std::chrono::duration_cast<std::chrono::microseconds>(std::chrono::steady_clock::now().time_since_epoch()).count());
}
float ms_since(uint64_t t0) {
	return float(now_us() - t0) / 1000.0f;
}
bool past(uint64_t deadline) {
	return deadline != 0 && now_us() >= deadline;
}
} // namespace

// ---------------------------------------------------------------- worker jobs
// Each job is a heap object handed to WorkerThreadPool::add_native_task; the main thread owns it again
// once is_task_completed() and wait_for_task_completion() have returned.
struct WorldStreamer::AreaJob {
	AssetLibrary *assets = nullptr;
	int64_t seed = 0;
	SettlementKind kind = SettlementKind::Neighborhood;
	bool lane_open = false;
	int port_side = 0;
	PrepareFn prepare;
	SettlementPlan plan;
	std::shared_ptr<void> prepared;
	float wall_ms = 0.0f;
	static void run(void *ud) {
		AreaJob *j = static_cast<AreaJob *>(ud);
		const uint64_t t0 = now_us();
		j->plan = plan_settlement(j->assets, j->seed, j->kind, true, j->lane_open, j->port_side, true);
		if (j->prepare) {
			j->prepared = j->prepare(*j->plan.ctx);
		}
		j->wall_ms = ms_since(t0);
	}
};

// The game's prepare step again for a settlement kept in the cache (its plan is still on the node: read
// only, by the worker and the main thread alike, so they can share it).
struct WorldStreamer::PrepJob {
	PrepareFn prepare;
	const GenContext *ctx = nullptr;
	std::shared_ptr<void> prepared;
	static void run(void *ud) {
		PrepJob *j = static_cast<PrepJob *>(ud);
		j->prepared = j->prepare(*j->ctx);
	}
};

struct WorldStreamer::ChunkData {
	int cx = 0, cz = 0;
	MeshBuilder m;             // ground + road quads (a signpost adds its boards on the main thread)
	Array arrays;              // m packed on the worker (unused when there is a signpost)
	PackedVector3Array faces;  // collision triangles at the true heights
	struct Post {
		Vector3 p;
		float yaw;
	};
	std::vector<Post> posts;
	std::vector<RegionPlan> signposts; // woods junctions in this chunk
	std::vector<std::vector<Transform3D>> inst; // per ForestKit kind, + 3 maples, + fallen leaves
	struct TreeShape {
		bool log = false;
		Transform3D xf; // log: the box's transform
		Vector3 size;   // log: box size
		float radius = 0.0f, height = 0.0f;
		Vector3 pos; // trunk: cylinder centre
	};
	std::vector<TreeShape> shapes;
	std::vector<Transform3D> grass;
	std::vector<Color> grass_col;
};

struct WorldStreamer::ChunkJob {
	const WorldStreamer *streamer = nullptr;
	WorldPlan *plan = nullptr;
	ChunkData data;
	float wall_ms = 0.0f;
	static void run(void *ud) {
		ChunkJob *j = static_cast<ChunkJob *>(ud);
		const uint64_t t0 = now_us();
		j->streamer->make_chunk_data(*j->plan, j->data);
		j->data.arrays = j->data.m.arrays();
		j->wall_ms = ms_since(t0);
	}
};

// The world plan's regions around the player (7 x 7), worked out ahead on a worker's own plan: base()
// plans each settlement's street and lots, tens of milliseconds a region. The streamer copies them into its
// plan and the chunk worker's (WorldPlan::adopt_regions), so neither ever plans a region itself in time.
struct WorldStreamer::RegionJob {
	WorldPlan *plan = nullptr;
	int ri = 0, rj = 0;
	static void run(void *ud) {
		RegionJob *j = static_cast<RegionJob *>(ud);
		for (int r = 0; r <= 3; ++r) { // nearest rings first
			for (int di = -r; di <= r; ++di) {
				for (int dj = -r; dj <= r; ++dj) {
					if (std::max(std::abs(di), std::abs(dj)) == r) {
						j->plan->region(j->ri + di, j->rj + dj);
					}
				}
			}
		}
	}
};

struct WorldStreamer::HorizonJob {
	const WorldPlan *plan = nullptr;
	Vector3 center;
	Array arrays;
	static void run(void *ud) {
		HorizonJob *j = static_cast<HorizonJob *>(ud);
		const Vector3 center = j->center;
		const float cx = std::round(center.x / HORIZON_STEP) * HORIZON_STEP, cz = std::round(center.z / HORIZON_STEP) * HORIZON_STEP;
		const int n = int(HORIZON_R / HORIZON_STEP);
		std::vector<float> h((2 * n + 1) * (2 * n + 1));
		for (int z = -n; z <= n; ++z) {
			for (int x = -n; x <= n; ++x) {
				const float wx = cx + x * HORIZON_STEP, wz = cz + z * HORIZON_STEP;
				h[(z + n) * (2 * n + 1) + (x + n)] = j->plan->natural(wx, wz) - 1.2f; // natural(): pure noise, no caches
			}
		}
		MeshBuilder m;
		for (int z = -n; z < n; ++z) {
			for (int x = -n; x < n; ++x) {
				const float x0 = cx + x * HORIZON_STEP, z0 = cz + z * HORIZON_STEP;
				const float mx = x0 + HORIZON_STEP * 0.5f - center.x, mz = z0 + HORIZON_STEP * 0.5f - center.z;
				const float d = std::sqrt(mx * mx + mz * mz);
				if (d < HORIZON_HOLE || d > HORIZON_R) {
					continue;
				}
				auto H = [&](int ix, int iz) { return h[(iz + n) * (2 * n + 1) + (ix + n)]; };
				const Vector3 a(x0, H(x, z), z0), b(x0 + HORIZON_STEP, H(x + 1, z), z0);
				const Vector3 c(x0 + HORIZON_STEP, H(x + 1, z + 1), z0 + HORIZON_STEP), dd(x0, H(x, z + 1), z0 + HORIZON_STEP);
				m.quad_out(a, b, c, dd, mat_cell(pal::LAWN_DARK, mat::GRASS), (a + c) * 0.5f - Vector3(0, 1, 0));
			}
		}
		j->arrays = m.arrays();
	}
};

void WorldStreamer::_bind_methods() {
}

WorldStreamer::~WorldStreamer() {
	wait_all();
}

void WorldStreamer::_notification(int p_what) {
	if (p_what == NOTIFICATION_EXIT_TREE) {
		wait_all();
		const PackedStringArray args = OS::get_singleton()->get_cmdline_user_args();
		bool want = args.has("--stream-stats");
		for (const String &a : args) {
			want = want || a.begins_with("--fly-test=");
		}
		if (want) {
			print_stats();
		}
	}
}

// Every worker task finished (setup, exit): plans land in their Pre, chunk and horizon jobs are dropped.
void WorldStreamer::wait_all() {
	WorkerThreadPool *pool = WorkerThreadPool::get_singleton();
	for (auto &kv : pre_) {
		poll_plan(kv.second, true);
		wait_prep(kv.second);
	}
	if (chunk_task_ >= 0) {
		pool->wait_for_task_completion(chunk_task_);
		chunk_task_ = -1;
		delete chunk_job_;
		chunk_job_ = nullptr;
	}
	if (region_task_ >= 0) {
		pool->wait_for_task_completion(region_task_);
		region_task_ = -1;
		delete region_job_;
		region_job_ = nullptr;
	}
	if (horizon_task_ >= 0) {
		pool->wait_for_task_completion(horizon_task_);
		horizon_task_ = -1;
		delete horizon_job_;
		horizon_job_ = nullptr;
	}
}

void WorldStreamer::setup(NeighborhoodGenerator *templ, uint64_t seed) {
	const PackedStringArray args = OS::get_singleton()->get_cmdline_user_args();
	if (args.has("--dump-roads")) {
		WorldPlan p;
		p.reset(seed);
		for (int i = -1; i <= 1; ++i) {
			for (int j = -1; j <= 1; ++j) {
				const RegionPlan &r = p.region(i, j);
				UtilityFunctions::print("REGION ", i, ",", j, " kind=", int(r.kind), " origin=", r.origin, " E=", r.ports[PORT_E].pos, " W=", r.ports[PORT_W].pos, " ", String(r.name.c_str()));
				for (int k = 0; k < 2; ++k) {
					const RoadPlan *road = k == 0 ? p.road(i, j, i + 1, j) : p.road(i, j, i, j + 1);
					if (road) {
						UtilityFunctions::print("  ROAD ", k == 0 ? "E" : "N", " pts=", int(road->pts.size()), " a=", road->pts.front(), " mid=", road->pts[road->pts.size() / 2], " b=", road->pts.back());
					}
				}
			}
		}
	}
	preload_ = !args.has("--no-preload");
	state_test_ = args.has("--stream-state-test");
	state_test_frame_ = 0;
	plan_tasks_ = PLAN_TASKS;
	cache_ = CACHE;
	preload_dist_ = PRELOAD;
	for (const String &a : args) {
		if (a.begins_with("--preload-tasks=")) {
			plan_tasks_ = Math::max(1, int(a.substr(16).to_int()));
		} else if (a.begins_with("--preload-dist=")) {
			preload_dist_ = float(a.substr(15).to_float()); // a stress test: many plans at once
		} else if (a.begins_with("--preload-cache=")) {
			cache_ = Math::max(1, int(a.substr(16).to_int()));
		}
	}
	// Nothing of the old world may outlive this: worker tasks first (they read the plan and the asset
	// library), then the prebuilt settlements (they would carry the old night's state).
	wait_all();
	{
		std::vector<uint64_t> keys;
		for (auto &kv : pre_) {
			keys.push_back(kv.first);
		}
		for (uint64_t k : keys) {
			drop_pre(k);
		}
		free_update(0);
	}
	templ_ = templ;
	toon_ = templ->get_toon_material();
	grass_ = templ->get_grass_material();
	plan_.reset(seed);
	chunk_plan_ = std::make_unique<WorldPlan>();
	chunk_plan_->reset(seed);
	adopted_regions_ = 0;
	region_plan_ = std::make_unique<WorldPlan>();
	region_plan_->reset(seed);
	warmed_i_ = 1 << 30;
	chunk_adopt_pending_ = false;
	for (auto &kv : chunks_) {
		kv.second.root->queue_free();
	}
	chunks_.clear();
	std::vector<uint64_t> keys;
	for (auto &kv : areas_) {
		keys.push_back(kv.first);
	}
	for (uint64_t k : keys) {
		unload_area(k);
	}
	finishing_.clear();
	states_.clear();
	if (!kit_.mesh[0].is_valid()) {
		kit_.build();
		// Painted Blender pines replace the procedural cone stacks once they're published.
		const char *pines[3] = { "tree_pine_a", "tree_pine_b", "tree_pine_a" };
		for (int k = 0; k < 3; ++k) {
			if (templ->assets().get(pines[k])->published) {
				const Ref<ArrayMesh> m = templ->assets().merged_mesh(pines[k]);
				if (m.is_valid()) {
					kit_.mesh[ForestKit::PINE_A + k] = m;
				}
			}
		}
		const char *ids[3] = { "tree_maple_red", "tree_maple_orange", "tree_maple_gold" };
		for (int i = 0; i < 3; ++i) {
			maples_[i] = templ->assets().merged_mesh(ids[i]);
		}
		if (templ->assets().get("leaf_scatter")->published) {
			leaf_patch_ = templ->assets().merged_mesh("leaf_scatter");
		}
		grass_mesh_ = grass_clump_mesh();
	}
	// The plan reads the candy kinds (a static table) on the workers: they must be registered by now
	// (festival_game.cpp does it before the first world starts).
	register_candy_kinds(templ->assets());
	Rng rng(sub_seed(seed, 0x300D));
	const float az = rng.range(0.0f, Math_TAU), elev = rng.range(0.16f, 0.22f);
	moon_dir_ = Vector3(std::cos(az) * std::cos(elev), std::sin(elev), std::sin(az) * std::cos(elev));
	horizon_center_ = Vector3(1e9f, 0, 0);
	heading_ = Vector3();
	last_update_us_ = 0;
}

std::vector<Area *> WorldStreamer::areas() {
	std::vector<Area *> out;
	for (auto &kv : areas_) {
		out.push_back(&kv.second);
	}
	return out;
}

Area *WorldStreamer::area(uint64_t key) {
	auto it = areas_.find(key);
	return it == areas_.end() ? nullptr : &it->second;
}

Area *WorldStreamer::area_at(const Vector3 &p, float margin) {
	for (auto &kv : areas_) {
		if (rect_distance(kv.second.plan, p.x, p.z) <= margin) {
			return &kv.second;
		}
	}
	return nullptr;
}

AreaState &WorldStreamer::state(int i, int j) {
	return states_[area_key(i, j)];
}

namespace {
PackedByteArray to_bytes(const std::vector<char> &v) {
	PackedByteArray b;
	b.resize(int64_t(v.size()));
	for (size_t i = 0; i < v.size(); ++i) {
		b.set(int64_t(i), uint8_t(v[i]));
	}
	return b;
}

std::vector<char> from_bytes(const Variant &v) {
	const PackedByteArray b = v;
	std::vector<char> out(size_t(b.size()));
	for (int64_t i = 0; i < b.size(); ++i) {
		out[size_t(i)] = char(b[i]);
	}
	return out;
}

// A visited area's saved progress against the layout it attaches with (WORLD_REWORK.md, "House goals and
// candy bug"): the per-lot and per-candy arrays sized to the layout (a save from an older layout with fewer
// lots left every door past its count unringable), and the house rule, cleared = trick-or-treated + all of
// the house's placed candy taken: a house marked cleared without a ring (older saves) is not, and one rung
// with none of its candy left is (quietly: the older rule left it waiting for doorbell treats, which an
// unload never brings back). The game keeps the rule while attached (FestivalGame::try_clear_house).
void reconcile_state(AreaState &st, const GenContext &ctx) {
	const size_t lots = ctx.lots.size();
	st.rung.resize(lots, 0);
	st.lot_cleared.resize(lots, 0);
	st.candy_taken.resize(ctx.candy.size(), 0);
	std::vector<int> left(lots, 0);
	for (size_t i = 0; i < ctx.candy.size(); ++i) {
		const int l = ctx.candy[i].lot;
		if (l >= 0 && l < int(lots) && !st.candy_taken[i]) {
			left[size_t(l)]++;
		}
	}
	for (size_t l = 0; l < lots; ++l) {
		st.lot_cleared[l] = st.rung[l] && (st.lot_cleared[l] || left[l] == 0) ? 1 : 0;
	}
}
} // namespace

Dictionary WorldStreamer::export_states() const {
	Dictionary out;
	for (const auto &kv : states_) {
		const AreaState &s = kv.second;
		if (!s.visited) {
			continue;
		}
		Dictionary d;
		d["freed"] = s.freed;
		d["complete"] = s.complete;
		d["candy_total"] = s.candy_total;
		d["candy_collected"] = s.candy_collected;
		d["candy_taken"] = to_bytes(s.candy_taken);
		d["rung"] = to_bytes(s.rung);
		d["lot_cleared"] = to_bytes(s.lot_cleared);
		d["spawner_down"] = to_bytes(s.spawner_down);
		d["vine_down"] = to_bytes(s.vine_down);
		d["heart_down"] = to_bytes(s.heart_down);
		out[String::num_uint64(kv.first >> 32) + ":" + String::num_uint64(kv.first & 0xFFFFFFFFu)] = d; // halves: each fits a signed parse
	}
	return out;
}

void WorldStreamer::import_states(const Dictionary &d) {
	const Array keys = d.keys();
	for (int64_t i = 0; i < keys.size(); ++i) {
		const String k = keys[i];
		const Dictionary s = d[keys[i]];
		const PackedStringArray halves = k.split(":");
		if (halves.size() != 2) {
			continue;
		}
		AreaState &st = states_[(uint64_t(halves[0].to_int()) << 32) | uint64_t(halves[1].to_int())];
		st.visited = true;
		st.freed = bool(s.get("freed", false));
		st.complete = bool(s.get("complete", false));
		st.candy_total = int(s.get("candy_total", 0));
		st.candy_collected = int(s.get("candy_collected", 0));
		st.candy_taken = from_bytes(s.get("candy_taken", PackedByteArray()));
		st.rung = from_bytes(s.get("rung", PackedByteArray()));
		st.lot_cleared = from_bytes(s.get("lot_cleared", PackedByteArray()));
		st.spawner_down = from_bytes(s.get("spawner_down", PackedByteArray()));
		st.vine_down = from_bytes(s.get("vine_down", PackedByteArray()));
		st.heart_down = from_bytes(s.get("heart_down", PackedByteArray()));
	}
}

// ---------------------------------------------------------------- timing report
void WorldStreamer::print_stats() const {
	auto summary = [](const char *name, std::vector<float> v) {
		if (v.empty()) {
			return String(name) + " n=0";
		}
		std::sort(v.begin(), v.end());
		double sum = 0.0;
		for (float x : v) {
			sum += x;
		}
		auto pct = [&](float p) { return v[std::min(v.size() - 1, size_t(p * float(v.size() - 1) + 0.5f))]; };
		return vformat("%s n=%d mean=%.2f p50=%.2f p99=%.2f max=%.2f", name, int(v.size()), sum / double(v.size()), pct(0.5f), pct(0.99f), v.back());
	};
	auto ints = [](const std::vector<int> &v) {
		std::vector<float> f(v.begin(), v.end());
		return f;
	};
	UtilityFunctions::print("STREAM_STATS mode=", preload_ ? "preload" : "sync");
	auto histogram = [](const char *name, const std::vector<float> &v) {
		static const float edges[] = { 8.4f, 17.0f, 20.0f, 25.0f, 33.4f, 40.0f, 100.0f, 250.0f, 1000.0f };
		int bins[10] = {};
		for (float x : v) {
			int b = 0;
			while (b < 9 && x >= edges[b]) {
				++b;
			}
			++bins[b];
		}
		return vformat("%s <8.4:%d <17:%d <20:%d <25:%d <33.4:%d <40:%d <100:%d <250:%d <1000:%d >=1000:%d", name, bins[0], bins[1], bins[2], bins[3], bins[4], bins[5], bins[6],
				bins[7], bins[8], bins[9]);
	};
	UtilityFunctions::print("STREAM_STATS histogram ", histogram("interval_ms", stats_.interval_ms));
	UtilityFunctions::print("STREAM_STATS histogram ", histogram("frame_ms", stats_.frame_ms));
	UtilityFunctions::print("STREAM_STATS ", summary("frame_ms", stats_.frame_ms));
	UtilityFunctions::print("STREAM_STATS ", summary("interval_ms", stats_.interval_ms));
	UtilityFunctions::print("STREAM_STATS ", summary("after_attach_ms", stats_.after_attach_ms));
	UtilityFunctions::print("STREAM_STATS ", summary("region_ms", stats_.region_ms));
	UtilityFunctions::print("STREAM_STATS ", summary("chunk_ms", stats_.chunk_ms));
	UtilityFunctions::print("STREAM_STATS ", summary("chunk_job_ms", stats_.chunk_job_ms));
	UtilityFunctions::print("STREAM_STATS ", summary("adopt_ms", stats_.adopt_ms));
	UtilityFunctions::print("STREAM_STATS ", summary("horizon_ms", stats_.horizon_ms));
	UtilityFunctions::print("STREAM_STATS ", summary("attach_ms", stats_.attach_ms));
	UtilityFunctions::print("STREAM_STATS ", summary("attach_total_ms", stats_.attach_total_ms));
	UtilityFunctions::print("STREAM_STATS ", summary("attach_frames", ints(stats_.attach_frames)));
	UtilityFunctions::print("STREAM_STATS ", summary("callback_ms", stats_.callback_ms));
	UtilityFunctions::print("STREAM_STATS ", summary("loaded_step_ms", stats_.loaded_step_ms));
	UtilityFunctions::print("STREAM_STATS ", summary("gen_ms", stats_.gen_ms));
	UtilityFunctions::print("STREAM_STATS ", summary("plan_ms", stats_.plan_ms));
	UtilityFunctions::print("STREAM_STATS ", summary("build_ms", stats_.build_ms));
	UtilityFunctions::print("STREAM_STATS ", summary("build_frames", ints(stats_.build_frames)));
	UtilityFunctions::print(vformat("STREAM_STATS pipeline_compilations draw=%d surface=%d mesh=%d specialization=%d", stats_.compilations[0], stats_.compilations[1],
			stats_.compilations[2], stats_.compilations[3]));
	UtilityFunctions::print("STREAM_STATS peak_concurrent_plans=", peak_planning_);
	UtilityFunctions::print("STREAM_STATS frames_over_33ms=", stats_.long_frames, " of_them_with_5ms_or_more_streaming=", stats_.long_streaming);
	UtilityFunctions::print("STREAM_STATS fallbacks=", stats_.fallbacks, " chunk_fallbacks=", stats_.chunk_fallbacks, " reused=", stats_.reused, " blocking_loads=", stats_.blocking_loads);
	String worst;
	std::vector<float> f = stats_.frame_ms;
	std::sort(f.begin(), f.end(), std::greater<float>());
	for (size_t i = 0; i < f.size() && i < 12; ++i) {
		worst += String::num(f[i], 1) + " ";
	}
	UtilityFunctions::print("STREAM_STATS worst_frames_ms ", worst);
}

// --stream-probe: what making a built settlement live costs in Godot (re-entering the tree, flipping
// visibility, flipping processing), measured on a settlement the old path just built.
namespace {
int count_nodes(Node *n) {
	int c = 1;
	for (int i = 0; i < n->get_child_count(); ++i) {
		c += count_nodes(n->get_child(i));
	}
	return c;
}
} // namespace

void WorldStreamer::probe_area(NeighborhoodGenerator *gen) {
	const int nodes = count_nodes(gen);
	uint64_t t = now_us();
	gen->set_visible(false);
	const float hide = ms_since(t);
	t = now_us();
	gen->set_visible(true);
	const float show = ms_since(t);
	t = now_us();
	gen->set_process_mode(PROCESS_MODE_DISABLED);
	const float dis = ms_since(t);
	t = now_us();
	gen->set_process_mode(PROCESS_MODE_INHERIT);
	const float en = ms_since(t);
	t = now_us();
	remove_child(gen);
	const float rem = ms_since(t);
	t = now_us();
	add_child(gen);
	const float add = ms_since(t);
	UtilityFunctions::print(vformat("STREAM_PROBE nodes=%d hide=%.2f show=%.2f disable=%.2f enable=%.2f remove=%.2f add=%.2f", nodes, hide, show, dis, en, rem, add));
}

// --stream-verify=<file>: every settlement made live is fingerprinted twice: its plan (SweepDump's per-
// category hashes, into <file>) and its node tree (names, classes, transforms, mesh bounds and sizes,
// instance counts: a "VERIFY" log line), so a preloaded world can be compared with a --no-preload one.
namespace {
struct TreeHash {
	uint64_t h = 1469598103934665603ull;
	void bytes(const void *p, size_t n) {
		const unsigned char *c = static_cast<const unsigned char *>(p);
		for (size_t i = 0; i < n; ++i) {
			h = (h ^ c[i]) * 1099511628211ull;
		}
	}
	void f(float x) { bytes(&x, sizeof x); }
	void i(int64_t x) { bytes(&x, sizeof x); }
	void s(const String &x) {
		const CharString u = x.utf8();
		bytes(u.get_data(), size_t(u.length()));
	}
	void xf(const Transform3D &t) {
		for (int r = 0; r < 3; ++r) {
			f(t.basis.rows[r].x);
			f(t.basis.rows[r].y);
			f(t.basis.rows[r].z);
		}
		f(t.origin.x);
		f(t.origin.y);
		f(t.origin.z);
	}
	void aabb(const AABB &b) {
		f(b.position.x);
		f(b.position.y);
		f(b.position.z);
		f(b.size.x);
		f(b.size.y);
		f(b.size.z);
	}
};

void hash_tree(Node *n, TreeHash &h, int &count) {
	++count;
	h.s(n->get_class());
	const String name = n->get_name();
	if (!name.begins_with("@")) {
		h.s(name); // (an engine-made "@Class@123" name counts nodes made anywhere: not part of the layout)
	}
	if (Node3D *n3 = Object::cast_to<Node3D>(n)) {
		h.xf(n3->get_transform());
	}
	if (MeshInstance3D *mi = Object::cast_to<MeshInstance3D>(n)) {
		const Ref<Mesh> m = mi->get_mesh();
		if (m.is_valid()) {
			h.aabb(m->get_aabb());
			h.i(m->get_surface_count());
			if (ArrayMesh *am = Object::cast_to<ArrayMesh>(m.ptr())) {
				for (int s = 0; s < am->get_surface_count(); ++s) {
					h.i(am->surface_get_array_len(s));
				}
			}
		}
	}
	if (MultiMeshInstance3D *mmi = Object::cast_to<MultiMeshInstance3D>(n)) {
		const Ref<MultiMesh> mm = mmi->get_multimesh();
		if (mm.is_valid()) {
			h.i(mm->get_instance_count());
			for (int k = 0; k < mm->get_instance_count(); ++k) {
				h.xf(mm->get_instance_transform(k));
			}
		}
	}
	if (Label3D *l = Object::cast_to<Label3D>(n)) {
		h.s(l->get_text());
	}
	if (CollisionShape3D *cs = Object::cast_to<CollisionShape3D>(n)) {
		if (cs->get_shape().is_valid()) {
			h.s(cs->get_shape()->get_class());
		}
	}
	for (int c = 0; c < n->get_child_count(); ++c) {
		hash_tree(n->get_child(c), h, count);
	}
}
} // namespace

void WorldStreamer::verify_area(const Area &a) {
	String path;
	for (const String &arg : OS::get_singleton()->get_cmdline_user_args()) {
		if (arg.begins_with("--stream-verify=")) {
			path = arg.substr(16);
		}
	}
	if (path.is_empty()) {
		return;
	}
	if (!verify_) {
		verify_ = std::make_shared<SweepDump>();
		verify_->open(std::string(path.utf8().get_data()), "");
	}
	const std::string label = std::string("area_") + std::to_string(a.plan.i) + "_" + std::to_string(a.plan.j);
	verify_->settlement(label, settlement_seed(a.plan), *a.gen->context(), nullptr);
	TreeHash h;
	int count = 0;
	hash_tree(a.gen, h, count);
	// Geometry instances hold a slot each in the renderer's global shader-parameter buffer when their material
	// has instance uniforms (toon.gdshader: 13): the area's, and everything the streamer holds right now.
	std::function<void(Node *, int &, int &)> geo = [&](Node *n, int &all, int &lights) {
		if (Object::cast_to<GeometryInstance3D>(n)) {
			++all;
		}
		if (Object::cast_to<Light3D>(n)) {
			++lights;
		}
		for (int c = 0; c < n->get_child_count(); ++c) {
			geo(n->get_child(c), all, lights);
		}
	};
	int area_geo = 0, area_lights = 0, all_geo = 0, all_lights = 0;
	geo(a.gen, area_geo, area_lights);
	geo(this, all_geo, all_lights);
	UtilityFunctions::print(vformat("VERIFY %s %d,%d nodes=%d tree=%s geometry=%d lights=%d streamer_geometry=%d streamer_lights=%d", String(a.plan.name.c_str()), a.plan.i, a.plan.j,
			count, String::num_uint64(h.h, 16), area_geo, area_lights, all_geo, all_lights));
}

// --stream-state-test: headless check that progress survives leaving and coming back. The streamer's
// focus walks its own path from the home spawn (the kid stays put): a pause in which the candy of the
// home area's lot 0 is rung and its candy collected through the real CandyField (so the game's own handlers
// mark candy_taken and lot_cleared) and lot 1's doorbell is rung as the game does; out 350 m east and back
// (the home area is left and comes back from the cache, never rebuilt); out 800 m and back (evicted,
// planned and built again with the saved state). Every time home attaches, its nodes are checked
// against its AreaState ("STATE_TEST" lines), then the run quits.
void WorldStreamer::state_test_step() {
	const RegionPlan &home = plan_.region(0, 0);
	const Vector3 s = home.spawn;
	const float step = 0.25f; // m per frame (15 m/s at 60 fps)
	const int f = state_test_frame_++;
	struct Leg {
		int hold;
		float out;
	};
	static const Leg legs[] = { { 150, 350.0f }, { 120, 800.0f }, { 150, 0.0f } };
	int t = f;
	Vector3 p = s;
	bool done = true;
	for (const Leg &leg : legs) {
		if (t < leg.hold) {
			done = false;
			break;
		}
		t -= leg.hold;
		const int frames = int(leg.out * 2.0f / step);
		if (t < frames) {
			const float d = t * step;
			p = s + Vector3(d < leg.out ? d : 2.0f * leg.out - d, 0.0f, 0.0f);
			done = false;
			break;
		}
		t -= frames;
	}
	state_test_focus_ = p;
	Area *a = area(area_key(0, 0));
	if (f >= 40 && f < 140 && a && a->gen->candy_field()) {
		if (f == 40 && int(a->state->rung.size()) > 0) {
			// Lot 0 is rung too: a house is cleared only once trick-or-treated and all its candy is taken.
			a->state->rung[0] = 1;
			a->gen->open_door(0);
		}
		// Collect lot 0's candy: the CandyField's real pickup (homing, then the collected signals).
		const GenContext *ctx = a->gen->context();
		Vector3 c;
		int n = 0;
		for (const CandySpec &cs : ctx->candy) {
			if (cs.lot == 0) {
				c += cs.pos;
				++n;
			}
		}
		if (n > 0) {
			c /= float(n);
			c.y = 0.0f;
			a->gen->candy_field()->collect(c, 14.0f, 12.0f, 1.0f / 60.0f);
		}
		if (f == 41 && a->gen->get_lot_count() > 1 && int(a->state->rung.size()) > 1) {
			a->state->rung[1] = 1; // what the game does at a doorbell
			a->gen->open_door(1);
		}
	}
	if (done) {
		UtilityFunctions::print("STATE_TEST done: ", state_test_attaches_, " home attaches checked, ", state_test_failures_, " failures");
		state_test_ = false;
		get_tree()->quit();
	}
}

void WorldStreamer::state_test_check(const Area &a) {
	const AreaState &st = *a.state;
	const GenContext *ctx = a.gen->context();
	int taken = 0, cleared = 0, rung = 0, bad_doors = 0, bad_houses = 0;
	for (char c : st.candy_taken) {
		taken += c ? 1 : 0;
	}
	for (int l = 0; l < a.gen->get_lot_count(); ++l) {
		const bool want_closed = l < int(st.lot_cleared.size()) && st.lot_cleared[l];
		const bool want_open = l < int(st.rung.size()) && st.rung[l];
		cleared += want_closed ? 1 : 0;
		rung += want_open ? 1 : 0;
		bad_houses += a.gen->house_closed(l) != want_closed ? 1 : 0;
		bad_doors += a.gen->door_open(l) != want_open ? 1 : 0;
	}
	const CandyField *cf = a.gen->candy_field();
	const int alive = cf ? cf->total() - cf->collected() : -1;
	const int want_alive = int(ctx->candy.size()) - taken;
	const bool ok = alive == want_alive && bad_houses == 0 && bad_doors == 0;
	++state_test_attaches_;
	state_test_failures_ += ok ? 0 : 1;
	UtilityFunctions::print(vformat("STATE_TEST attach %d (%s): candy taken %d, field showing %d of %d (want %d), houses cleared %d (%d wrong), doors rung %d (%d wrong): %s",
			state_test_attaches_, a.gen->get_parent() ? "node" : "?", taken, alive, int(ctx->candy.size()), want_alive, cleared, bad_houses, rung, bad_doors, ok ? "OK" : "FAIL"));
}

// What streaming is doing (for the long-frame lines): each settlement on its way and the jobs out.
String WorldStreamer::stream_state() const {
	static const char *names[] = { "planning", "planned", "building", "ready", "revealing", "applying", "concealing" };
	String s;
	for (const auto &kv : pre_) {
		s += String(kv.second.plan.name.c_str()) + ":" + names[kv.second.stage];
		if (kv.second.stage == Pre::BUILDING && kv.second.gen) {
			s += String::num_int64(kv.second.gen->build_stage());
		}
		s += " ";
	}
	s += vformat("finishing %d dying %d chunk-job %d region-job %d horizon-job %d", int(finishing_.size()), int(dying_.size()), chunk_task_ >= 0 ? 1 : 0, region_task_ >= 0 ? 1 : 0,
			horizon_task_ >= 0 ? 1 : 0);
	return s;
}

// ---------------------------------------------------------------- update
void WorldStreamer::update(const Vector3 &focus_in, bool blocking) {
	const uint64_t t_frame = now_us();
	blocking_now_ = blocking;
	Vector3 focus = focus_in;
	if (!blocking && state_test_) {
		// --stream-state-test: the streamer walks its own path (the player stays put).
		state_test_step();
		focus = state_test_focus_;
	}
	if (!blocking && !fps_cap_applied_) {
		// --stream-fps=N: cap the frame rate for timing runs (after settings.gd has applied its own), so a
		// headless walk has a real frame budget and real idle time.
		fps_cap_applied_ = true;
		for (const String &a : OS::get_singleton()->get_cmdline_user_args()) {
			if (a.begins_with("--stream-fps=")) {
				Engine::get_singleton()->set_max_fps(a.substr(13).to_int());
			}
		}
	}
	// The per-frame budget for streaming work on the main thread: what a 60 Hz frame (or the frame cap's)
	// has left after the game's own main-thread work last frame (process + physics, less what streaming
	// took) and ~4 ms for the renderer's CPU side, kept between 1 and 3 ms. Idle time a frame cap sleeps
	// away does not count as work.
	if (!blocking) {
		if (last_update_us_ != 0) {
			const float interval = float(t_frame - last_update_us_) / 1000.0f;
			// The whole frame, rendering included: what the player feels (a first sight of new shaders or
			// pipelines would show here, in the second after an attach).
			stats_.interval_ms.push_back(interval);
			// Pipeline compilations last frame (Godot 4.4+): DRAW ones stall the frame (not precompiled);
			// SURFACE / MESH ones are queued when instances and meshes are made; SPECIALIZATION in the background.
			Performance *pm = Performance::get_singleton();
			// (The monitors count since the start: this frame's are the differences.)
			const int64_t comp_now[4] = { int64_t(pm->get_monitor(Performance::PIPELINE_COMPILATIONS_DRAW)), int64_t(pm->get_monitor(Performance::PIPELINE_COMPILATIONS_SURFACE)),
				int64_t(pm->get_monitor(Performance::PIPELINE_COMPILATIONS_MESH)), int64_t(pm->get_monitor(Performance::PIPELINE_COMPILATIONS_SPECIALIZATION)) };
			int comp[4];
			for (int c = 0; c < 4; ++c) {
				comp[c] = int(comp_now[c] - stats_.compilations[c]);
				stats_.compilations[c] = comp_now[c];
			}
			const int comp_draw = comp[0], comp_surface = comp[1], comp_mesh = comp[2], comp_spec = comp[3];
			if (interval > 33.4f) {
				++stats_.long_frames;
				stats_.long_streaming += last_spent_us_ >= 5000 ? 1 : 0;
				UtilityFunctions::print(vformat("Long frame: %.1f ms (streaming took %.1f of it, %d frames after an attach; pipeline compilations draw %d surface %d mesh %d specialization %d; process max %.1f physics max %.1f ms; %s)",
						interval, float(last_spent_us_) / 1000.0f, since_attach_, comp_draw, comp_surface, comp_mesh, comp_spec, pm->get_monitor(Performance::TIME_PROCESS) * 1000.0,
						pm->get_monitor(Performance::TIME_PHYSICS_PROCESS) * 1000.0, stream_state()));
			}
			if (since_attach_ < 120) {
				stats_.after_attach_ms.push_back(interval);
			}
			++since_attach_;
			// TIME_PROCESS / TIME_PHYSICS_PROCESS are the worst frame of the last second (Godot refreshes them
			// once a second) and include streaming's own time: take off streaming's own worst of the last ~2 s.
			Performance *perf = Performance::get_singleton();
			const float busy = float(perf->get_monitor(Performance::TIME_PROCESS) + perf->get_monitor(Performance::TIME_PHYSICS_PROCESS)) * 1000.0f;
			own_ms_ring_[own_ring_i_++ % OWN_RING] = float(last_spent_us_) / 1000.0f;
			float own_max = 0.0f;
			for (float v : own_ms_ring_) {
				own_max = Math::max(own_max, v);
			}
			const float game = Math::max(0.0f, busy - own_max);
			game_ms_ema_ = Math::lerp(game_ms_ema_, game, 0.1f);
			if (game > game_ms_ema_) {
				game_ms_ema_ = game; // a heavy frame counts at once, a light one slowly
			}
		}
		const int fps = Engine::get_singleton()->get_max_fps();
		const float target = 1000.0f / float(fps > 0 ? Math::min(fps, 60) : 60);
		budget_us_ = int64_t(Math::clamp(target - game_ms_ema_ - 4.0f, 1.0f, 3.0f) * 1000.0f);
		const Vector3 d = focus - last_focus_;
		if (d.length() > 0.01f && d.length() < 50.0f) {
			heading_ = heading_.lerp(Vector3(d.x, 0, d.z).normalized(), 0.05f);
		}
	}
	last_focus_ = focus;
	const uint64_t deadline = blocking ? 0 : t_frame + uint64_t(budget_us_);

	float part_ms[6] = {};
	int fi, fj;
	WorldPlan::region_of(focus.x, focus.z, fi, fj);
	part_ms[4] = ms_since(t_frame); // everything before here (the budget, the state test)
	if (preload_) {
		const uint64_t tr = now_us();
		regions_update(fi, fj, blocking);
		part_ms[5] = ms_since(tr);
	}
	if (preload_ && !blocking) {
		const uint64_t tc = now_us();
		chunks_update(focus, blocking, deadline); // the ground first; settlements build in what is left
		part_ms[0] = ms_since(tc);
	}
	if (!preload_) {
		// The old path: settlements first (they are the destination), at most one per frame unless blocking.
		bool loaded = false;
		float region_ms = 0.0f;
		for (int di = -1; di <= 1 && (blocking || !loaded); ++di) {
			for (int dj = -1; dj <= 1 && (blocking || !loaded); ++dj) {
				const uint64_t t_r = now_us();
				const RegionPlan &r = plan_.region(fi + di, fj + dj);
				region_ms += ms_since(t_r);
				if (r.settlement() && !areas_.count(area_key(r.i, r.j)) && rect_distance(r, focus.x, focus.z) < AREA_LOAD) {
					load_area(r);
					loaded = true;
				}
			}
		}
		if (!blocking) {
			stats_.region_ms.push_back(region_ms);
		}
		std::vector<uint64_t> drop;
		for (auto &kv : areas_) {
			if (rect_distance(kv.second.plan, focus.x, focus.z) > AREA_DROP) {
				drop.push_back(kv.first);
			}
		}
		for (uint64_t k : drop) {
			unload_area(k);
		}
	} else {
		const uint64_t tp = now_us();
		preload_update(focus, fi, fj, deadline);
		part_ms[1] = ms_since(tp);
		const uint64_t tf = now_us();
		free_update(blocking ? 0 : deadline);
		part_ms[2] = ms_since(tf);
	}
	if (!preload_ || blocking) {
		chunks_update(focus, blocking, deadline);
	}
	const uint64_t th = now_us();
	horizon_update(focus, blocking);
	part_ms[3] = ms_since(th);
	last_spent_us_ = now_us() - t_frame;
	last_update_us_ = blocking ? 0 : t_frame; // a blocking update (a load, a teleport) is not a frame to measure
	if (!blocking) {
		const float ms = ms_since(t_frame);
		stats_.frame_ms.push_back(ms);
		if (ms > 8.0f && preload_) {
			UtilityFunctions::print(vformat("Stream frame %.1f ms (start %.1f, regions %.1f, chunks %.1f, settlements %.1f, freeing %.1f, horizon %.1f; budget %.1f ms, preloads %d, attached %d)", ms, part_ms[4], part_ms[5], part_ms[0], part_ms[1], part_ms[2], part_ms[3], float(budget_us_) / 1000.0f, int(pre_.size()), int(areas_.size())));
		}
	} else {
		++stats_.blocking_loads;
	}
}

// ---------------------------------------------------------------- area preloading
// Lower is sooner: distance to the footprint, less when the player is heading for it, less for the
// settlements a road from the player's region leads to.
float WorldStreamer::area_score(const RegionPlan &r, const Vector3 &focus) const {
	float s = rect_distance(r, focus.x, focus.z);
	const Vector3 c((r.x0 + r.x1) * 0.5f, 0.0f, (r.z0 + r.z1) * 0.5f);
	const Vector3 to = Vector3(c.x - focus.x, 0.0f, c.z - focus.z);
	if (to.length() > 1.0f && heading_.length() > 0.1f) {
		s -= 60.0f * Math::max(0.0f, to.normalized().dot(heading_.normalized()));
	}
	return s;
}

void WorldStreamer::start_plan(const RegionPlan &r, float score) {
	const uint64_t k = area_key(r.i, r.j);
	Pre &p = pre_[k];
	p = Pre();
	p.plan = r;
	p.score = score;
	p.queued_us = now_us();
	AreaJob *j = new AreaJob();
	j->assets = &templ_->assets();
	j->seed = settlement_seed(r);
	j->kind = r.kind == RegionKind::Town ? SettlementKind::Town : SettlementKind::Neighborhood;
	j->lane_open = r.lane_open;
	j->port_side = r.hood_port;
	if (on_area_prepare) {
		j->prepare = on_area_prepare(r);
	}
	p.job = j;
	p.stage = Pre::PLANNING;
	p.task = WorkerThreadPool::get_singleton()->add_native_task(&AreaJob::run, j, false, "Plan settlement");
}

// A plan the worker has finished: its result moves into the Pre (wait: block until it has).
bool WorldStreamer::poll_plan(Pre &p, bool wait) {
	if (p.stage != Pre::PLANNING) {
		return true;
	}
	WorkerThreadPool *pool = WorkerThreadPool::get_singleton();
	if (!wait && !pool->is_task_completed(p.task)) {
		return false;
	}
	pool->wait_for_task_completion(p.task);
	p.task = -1;
	p.plan_ms = p.job->wall_ms;
	stats_.plan_ms.push_back(p.job->wall_ms);
	p.stage = Pre::PLANNED;
	return true;
}

// The generator node goes into the tree now, hidden; build_step() fills it in over the next frames.
void WorldStreamer::begin_area_build(uint64_t key, Pre &p) {
	const RegionPlan &r = p.plan;
	AreaState &st = states_[key];
	NeighborhoodGenerator *gen = memnew(NeighborhoodGenerator);
	gen->set_name(String(r.name.c_str()));
	gen->set_toon_material(templ_->get_toon_material());
	gen->set_candy_material(templ_->get_candy_material());
	gen->set_grass_material(templ_->get_grass_material());
	gen->set_magic_material(templ_->get_magic_material());
	gen->share_assets(&templ_->assets());
	p.filter = st.candy_taken;
	gen->set_candy_filter(p.filter);
	gen->set_position(r.origin);
	add_child(gen);
	gen->begin_build(std::move(p.job->plan), true);
	p.prepared = std::move(p.job->prepared);
	delete p.job;
	p.job = nullptr;
	p.gen = gen;
	p.stage = Pre::BUILDING;
}

// The player is here and the preload is not ready: everything that is left, now (counted and printed).
void WorldStreamer::finish_now(uint64_t key, Pre &p, const char *why) {
	const uint64_t t0 = now_us();
	const int stage = int(p.stage);
	if (p.stage == Pre::PLANNING) {
		poll_plan(p, true);
	}
	if (p.stage == Pre::PLANNED) {
		begin_area_build(key, p);
	}
	if (p.stage == Pre::BUILDING) {
		const uint64_t tb = now_us();
		p.gen->build_step(0);
		p.build_ms += ms_since(tb);
		p.build_frames++;
		stats_.build_ms.push_back(p.build_ms);
		stats_.build_frames.push_back(p.build_frames);
		p.stage = Pre::READY;
	}
	if (p.stage == Pre::CONCEALING) {
		p.gen->conceal_step(0);
		p.stage = Pre::READY;
	}
	if (why) {
		++stats_.fallbacks;
		static const char *names[] = { "planning", "planned", "building", "ready", "revealing", "applying", "concealing" };
		UtilityFunctions::print(vformat("Area preload fallback (%s): %s was %s, finished in %.1f ms", why, String(p.plan.name.c_str()), names[stage], ms_since(t0)));
	}
}

// Shown, state applied, handed to the game: the settlement is live.
void WorldStreamer::attach_area(uint64_t key, Pre &p) {
	wait_prep(p); // a cached settlement's prepare step: long done by now
	Area a;
	a.key = key;
	a.plan = p.plan;
	a.state = &states_[key];
	a.gen = p.gen;
	a.prepared = std::move(p.prepared);
	Area &stored = areas_[key] = a;
	since_attach_ = 0;
	verify_area(stored);
	const uint64_t t_cb = now_us();
	if (on_area_loaded) {
		on_area_loaded(stored);
	}
	stored.prepared.reset();
	if (on_area_loaded_step) {
		if (blocking_now_) {
			on_area_loaded_step(stored, 0);
		} else {
			finishing_.push_back(key);
		}
	}
	const float cb = ms_since(t_cb);
	if (!blocking_now_) {
		stats_.callback_ms.push_back(cb);
	}
	if (state_test_ && key == area_key(0, 0)) {
		state_test_check(stored);
	}
}

// The player has left: the game lets go of it, it hides over the next frames and waits in the cache.
void WorldStreamer::detach_area(uint64_t key) {
	auto it = areas_.find(key);
	if (it == areas_.end()) {
		return;
	}
	const uint64_t t0 = now_us();
	if (on_area_unloading) {
		on_area_unloading(it->second);
	}
	if (it->second.extras) {
		it->second.extras->queue_free();
		it->second.extras = nullptr;
	}
	NeighborhoodGenerator *gen = it->second.gen;
	UtilityFunctions::print(vformat("Detached %s: %.2f ms (kept, hiding)", String(it->second.plan.name.c_str()), ms_since(t0)));
	// The game connected the candy field's signals in on_area_loaded and will again on the next one.
	if (CandyField *cf = gen->candy_field()) {
		for (const char *sig : { "candy_collected", "lot_emptied", "sequence_completed" }) {
			const TypedArray<Dictionary> list = cf->get_signal_connection_list(sig);
			for (int64_t i = 0; i < list.size(); ++i) {
				const Dictionary c = list[i];
				cf->disconnect(sig, Callable(c["callable"]));
			}
		}
	}
	Pre p;
	p.plan = it->second.plan;
	p.gen = gen;
	p.reused = true;
	p.filter = states_[key].candy_taken; // what its candy field shows (state only changes while attached)
	p.stage = Pre::CONCEALING;
	if (on_area_prepare && gen->context()) {
		// The next attach wants the game's prepared data again (the monsters' walk grid and nest layout).
		p.prep = new PrepJob();
		p.prep->prepare = on_area_prepare(p.plan);
		p.prep->ctx = gen->context();
		p.prep_task = WorkerThreadPool::get_singleton()->add_native_task(&PrepJob::run, p.prep, false, "Prepare settlement");
	}
	areas_.erase(it);
	finishing_.erase(std::remove(finishing_.begin(), finishing_.end(), key), finishing_.end());
	pre_[key] = std::move(p);
}

void WorldStreamer::wait_prep(Pre &p) {
	if (p.prep_task >= 0) {
		WorkerThreadPool::get_singleton()->wait_for_task_completion(p.prep_task);
		p.prep_task = -1;
		p.prepared = std::move(p.prep->prepared);
	}
	delete p.prep;
	p.prep = nullptr;
}

// The game's per-area work after on_area_loaded (nests, vines), a piece at a time.
void WorldStreamer::finishing_update(uint64_t deadline) {
	for (size_t i = 0; i < finishing_.size();) {
		auto it = areas_.find(finishing_[i]);
		bool done = true;
		if (it != areas_.end() && on_area_loaded_step) {
			const uint64_t t0 = now_us();
			done = on_area_loaded_step(it->second, deadline == 0 ? 0 : std::max(deadline, t0 + 500));
			const float spent = ms_since(t0);
			if (deadline != 0) {
				stats_.loaded_step_ms.push_back(spent);
				if (spent > float(budget_us_) / 1000.0f + 2.0f) {
					UtilityFunctions::print(vformat("on_area_loaded_step over budget: %s %.1f ms", String(it->second.plan.name.c_str()), spent));
				}
			}
		}
		if (done) {
			finishing_.erase(finishing_.begin() + int64_t(i));
		} else {
			++i;
		}
	}
}

void WorldStreamer::drop_pre(uint64_t key) {
	auto it = pre_.find(key);
	if (it == pre_.end()) {
		return;
	}
	Pre &p = it->second;
	if (p.task >= 0) {
		// The worker still has it: wait (a plan is a few hundred ms at most; only on setup or eviction of
		// a far area, which is rare: eviction skips areas still planning).
		WorkerThreadPool::get_singleton()->wait_for_task_completion(p.task);
		p.task = -1;
	}
	delete p.job;
	p.job = nullptr;
	wait_prep(p); // (it reads the node's plan: before the node goes)
	if (p.gen) {
		if (blocking_now_ || !preload_) {
			p.gen->queue_free();
		} else {
			p.gen->set_process(false); // its _process reaches into the nodes going away
			dying_.push_back(p.gen);   // freed a few hundred nodes a frame (free_update)
		}
		p.gen = nullptr;
	}
	pre_.erase(it);
}

// A dropped settlement comes apart over several frames, under the frame's budget and at most ~250 nodes
// a frame (they are deleted at the end of the frame, outside the budget's clock): small pieces whole,
// big containers opened up, a collision body first taken out of the tree whole (it leaves the physics
// space once) and then emptied out of the tree (taking shapes off a body in the space would rebuild its
// shape each time).
void WorldStreamer::free_update(uint64_t deadline) {
	const bool all = deadline == 0;
	int budget = all ? 1 << 30 : 250;
	bool did = false;
	while (budget > 0 && !dying_.empty()) {
		if (did && past(deadline)) {
			break;
		}
		did = true;
		Node *n = dying_.back();
		const int cc = n->get_child_count();
		if (cc == 0) {
			dying_.pop_back();
			if (Node *parent = n->get_parent()) {
				parent->remove_child(n);
			}
			n->queue_free();
			--budget;
			continue;
		}
		Node *ch = n->get_child(cc - 1);
		if (ch->get_child_count() > 32) {
			if (ch->is_inside_tree() && Object::cast_to<CollisionObject3D>(ch)) {
				n->remove_child(ch); // out of the space and the tree, shapes and all
			}
			dying_.push_back(ch);
			continue;
		}
		budget -= 1 + ch->get_child_count();
		n->remove_child(ch);
		ch->queue_free();
	}
}

void WorldStreamer::preload_update(const Vector3 &focus, int fi, int fj, uint64_t deadline) {
	const bool blocking = deadline == 0;
	// 1. Settlements the player has reached: show and attach (a not-ready one is finished now).
	float region_ms = 0.0f;
	for (int di = -1; di <= 1; ++di) {
		for (int dj = -1; dj <= 1; ++dj) {
			const uint64_t t_r = now_us();
			const RegionPlan &r = plan_.region(fi + di, fj + dj);
			region_ms += ms_since(t_r);
			const uint64_t k = area_key(r.i, r.j);
			if (!r.settlement() || areas_.count(k) || rect_distance(r, focus.x, focus.z) >= AREA_LOAD) {
				continue;
			}
			auto it = pre_.find(k);
			if (it == pre_.end()) {
				start_plan(r, 0.0f);
				it = pre_.find(k);
				finish_now(k, it->second, blocking ? nullptr : "not started");
			} else if (it->second.stage < Pre::READY || it->second.stage == Pre::CONCEALING) {
				finish_now(k, it->second, blocking ? nullptr : (it->second.stage == Pre::CONCEALING ? nullptr : "not ready"));
			}
			Pre &p = it->second;
			if (p.stage == Pre::READY) {
				p.stage = Pre::REVEALING;
				p.attach_ms = p.attach_worst = 0.0f;
				p.attach_frames = 0;
			}
		}
	}
	// 2. Showing ones: reveal, then the saved state (per lot), then attach. Spread under the budget, but
	// never stalled by it: at least one piece each frame.
	std::vector<uint64_t> live;
	for (auto &kv : pre_) {
		Pre &p = kv.second;
		if (p.stage != Pre::REVEALING && p.stage != Pre::APPLYING) {
			continue;
		}
		const uint64_t t0 = now_us();
		// Showing is capped at 1.5 ms a frame whatever the budget: the frame that makes an area live stays small.
		const uint64_t dl = blocking ? 0 : std::min(std::max(deadline, t0 + 1000), t0 + 1500);
		bool done = false;
		if (p.stage == Pre::REVEALING && p.gen->reveal_step(dl)) {
			p.stage = Pre::APPLYING;
			p.apply_i = 0;
			AreaState &st = states_[kv.first];
			if (st.visited && p.gen->context()) {
				reconcile_state(st, *p.gen->context());
			}
			if (st.visited && st.candy_taken != p.filter) {
				// Candy taken since this build (or since it was cached): rebuild the field with the state as it is.
				p.gen->rebuild_candy(st.candy_taken);
				p.filter = st.candy_taken;
			}
			if (!st.visited) {
				st.visited = true;
				st.candy_total = p.gen->candy_field() ? p.gen->candy_field()->total_value() : 0;
				st.candy_taken.assign(p.gen->context()->candy.size(), 0);
				st.rung.assign(p.gen->get_lot_count(), 0);
				st.lot_cleared.assign(p.gen->get_lot_count(), 0);
				p.filter = st.candy_taken;
			}
		}
		if (p.stage == Pre::APPLYING) {
			AreaState &st = states_[kv.first];
			const int lots = p.gen->get_lot_count();
			done = true;
			for (; int(p.apply_i) < lots && int(p.apply_i) < int(st.rung.size()); ++p.apply_i) {
				if (p.apply_i > 0 && past(dl)) {
					done = false;
					break;
				}
				const int l = int(p.apply_i);
				if (st.rung[l]) {
					p.gen->open_door(l);
				}
				if (l < int(st.lot_cleared.size()) && st.lot_cleared[l]) {
					p.gen->close_house(l, false); // idempotent: a reused area has it already
				}
			}
		}
		const float spent = ms_since(t0);
		p.attach_ms += spent;
		p.attach_worst = Math::max(p.attach_worst, spent);
		p.attach_frames++;
		if (done) {
			live.push_back(kv.first);
		}
	}
	for (uint64_t k : live) {
		Pre p = std::move(pre_[k]);
		pre_.erase(k);
		const uint64_t t0 = now_us();
		if (p.reused) {
			++stats_.reused;
		}
		attach_area(k, p);
		const float last = ms_since(t0);
		p.attach_ms += last;
		p.attach_worst = Math::max(p.attach_worst, last);
		if (!blocking) {
			stats_.attach_ms.push_back(p.attach_worst);
			stats_.attach_total_ms.push_back(p.attach_ms);
			stats_.attach_frames.push_back(p.attach_frames);
		}
		UtilityFunctions::print(vformat("Attached %s%s: worst frame %.2f ms, %.1f ms over %d frames (on_area_loaded %.2f ms)", String(p.plan.name.c_str()), p.reused ? " (cached)" : "",
				p.attach_worst, p.attach_ms, p.attach_frames, last));
	}
	finishing_update(blocking ? 0 : deadline);
	// 3. Settlements left behind: into the cache.
	{
		std::vector<uint64_t> drop;
		for (auto &kv : areas_) {
			if (rect_distance(kv.second.plan, focus.x, focus.z) > AREA_DROP) {
				drop.push_back(kv.first);
			}
		}
		std::sort(drop.begin(), drop.end());
		for (uint64_t k : drop) {
			detach_area(k);
		}
	}
	// 4. Which settlements to have ready: everything within PRELOAD in the 5 x 5 regions around, best
	// first; evict what is past EVICT, then the worst over the cache size (never one being shown).
	struct Cand {
		float score;
		uint64_t key;
		const RegionPlan *r;
	};
	std::vector<Cand> want;
	for (int di = -2; di <= 2; ++di) {
		for (int dj = -2; dj <= 2; ++dj) {
			const uint64_t t_r = now_us();
			const RegionPlan &r = plan_.region(fi + di, fj + dj);
			region_ms += ms_since(t_r);
			const uint64_t k = area_key(r.i, r.j);
			if (!r.settlement() || areas_.count(k) || rect_distance(r, focus.x, focus.z) > preload_dist_) {
				continue;
			}
			want.push_back({ area_score(r, focus), k, &r });
		}
	}
	if (!blocking) {
		stats_.region_ms.push_back(region_ms);
		if (region_ms > 2.0f) {
			UtilityFunctions::print(vformat("World plan regions planned on the main thread: %.1f ms", region_ms));
		}
	}
	std::sort(want.begin(), want.end(), [](const Cand &a, const Cand &b) { return a.score != b.score ? a.score < b.score : a.key < b.key; });
	for (auto &kv : pre_) {
		kv.second.score = area_score(kv.second.plan, focus);
	}
	{
		std::vector<uint64_t> evict;
		for (auto &kv : pre_) {
			const Pre &p = kv.second;
			if (p.stage == Pre::REVEALING || p.stage == Pre::APPLYING || p.stage == Pre::PLANNING) {
				continue;
			}
			if (rect_distance(p.plan, focus.x, focus.z) > preload_dist_ + (EVICT - PRELOAD)) {
				evict.push_back(kv.first);
			}
		}
		for (uint64_t k : evict) {
			drop_pre(k);
		}
	}
	int planning = 0;
	for (auto &kv : pre_) {
		planning += kv.second.stage == Pre::PLANNING ? 1 : 0;
	}
	for (const Cand &c : want) {
		if (pre_.count(c.key)) {
			continue;
		}
		if (planning >= plan_tasks_) {
			break;
		}
		if (int(pre_.size()) >= cache_) {
			// Full: make room only by dropping a settlement that is further down the list than this one.
			uint64_t worst = 0;
			float worst_score = -1e9f;
			for (auto &kv : pre_) {
				const Pre &p = kv.second;
				if (p.stage == Pre::REVEALING || p.stage == Pre::APPLYING || p.stage == Pre::PLANNING) {
					continue;
				}
				if (p.score > worst_score) {
					worst_score = p.score;
					worst = kv.first;
				}
			}
			// Hysteresis: only for a clearly better one (60 m), so two settlements at similar range never swap.
			if (worst_score < c.score + 60.0f || !pre_.count(worst)) {
				break;
			}
			drop_pre(worst);
		}
		start_plan(*c.r, c.score);
		++planning;
	}
	{
		int now_planning = 0;
		for (auto &kv : pre_) {
			now_planning += kv.second.stage == Pre::PLANNING ? 1 : 0;
		}
		peak_planning_ = std::max(peak_planning_, now_planning);
	}
	// 5. Finished plans.
	for (auto &kv : pre_) {
		poll_plan(kv.second, false);
	}
	// 6. Main-thread work under the budget: hide what was left behind, then build the best planned one.
	if (blocking) {
		return;
	}
	for (auto &kv : pre_) {
		Pre &p = kv.second;
		if (p.stage == Pre::CONCEALING && !past(deadline)) {
			if (p.gen->conceal_step(deadline)) {
				p.stage = Pre::READY;
			}
		}
	}
	while (!past(deadline)) {
		Pre *best = nullptr;
		uint64_t best_key = 0;
		for (auto &kv : pre_) {
			Pre &p = kv.second;
			if ((p.stage == Pre::PLANNED || p.stage == Pre::BUILDING) && (!best || p.score < best->score)) {
				best = &p;
				best_key = kv.first;
			}
		}
		if (!best) {
			break;
		}
		const uint64_t tb = now_us();
		if (best->stage == Pre::PLANNED) {
			begin_area_build(best_key, *best);
		}
		const int stage_before = best->gen->build_stage();
		const bool done = best->gen->build_step(deadline);
		const float spent = ms_since(tb);
		best->build_ms += spent;
		best->build_frames++;
		if (spent > float(budget_us_) / 1000.0f + 2.0f) {
			UtilityFunctions::print(vformat("Preload slice over budget: %s %.1f ms (build stage %d -> %d)", String(best->plan.name.c_str()), spent, stage_before, best->gen->build_stage()));
		}
		if (done) {
			best->stage = Pre::READY;
			stats_.build_ms.push_back(best->build_ms);
			stats_.build_frames.push_back(best->build_frames);
			UtilityFunctions::print(vformat("Preloaded %s: plan %.0f ms (worker), build %.1f ms over %d frames, %.0f ms after it was queued [%s]", String(best->plan.name.c_str()),
					best->plan_ms, best->build_ms, best->build_frames, ms_since(best->queued_us), best->gen->build_report()));
		}
	}
}

void WorldStreamer::regions_update(int fi, int fj, bool blocking) {
	WorkerThreadPool *pool = WorkerThreadPool::get_singleton();
	if (region_task_ >= 0 && (blocking || pool->is_task_completed(region_task_))) {
		pool->wait_for_task_completion(region_task_);
		region_task_ = -1;
		delete region_job_;
		region_job_ = nullptr;
		const uint64_t ta = now_us();
		plan_.adopt_regions(*region_plan_);
		if (chunk_task_ < 0) {
			chunk_plan_->adopt_regions(*region_plan_);
		} else {
			chunk_adopt_pending_ = true; // when the chunk worker is done with its plan
		}
		stats_.adopt_ms.push_back(ms_since(ta));
	}
	if (region_task_ < 0 && (fi != warmed_i_ || fj != warmed_j_)) {
		warmed_i_ = fi;
		warmed_j_ = fj;
		region_job_ = new RegionJob();
		region_job_->plan = region_plan_.get();
		region_job_->ri = fi;
		region_job_->rj = fj;
		region_task_ = pool->add_native_task(&RegionJob::run, region_job_, false, "World plan regions");
	}
}

// ---------------------------------------------------------------- terrain chunks
void WorldStreamer::chunks_update(const Vector3 &focus, bool blocking, uint64_t deadline) {
	// Terrain chunks nearest first.
	const int r = int(std::ceil(CHUNK_LOAD / CHUNK));
	const int cx0 = int(std::floor(focus.x / CHUNK)), cz0 = int(std::floor(focus.z / CHUNK));
	struct Want {
		float d;
		int x, z;
	};
	std::vector<Want> want;
	for (int dx = -r; dx <= r; ++dx) {
		for (int dz = -r; dz <= r; ++dz) {
			const int x = cx0 + dx, z = cz0 + dz;
			const float d = Vector2((x + 0.5f) * CHUNK - focus.x, (z + 0.5f) * CHUNK - focus.z).length();
			if (d < CHUNK_LOAD && !chunks_.count(ckey(x, z))) {
				want.push_back({ d, x, z });
			}
		}
	}
	std::sort(want.begin(), want.end(), [](const Want &a, const Want &b) { return a.d != b.d ? a.d < b.d : (a.x != b.x ? a.x < b.x : a.z < b.z); });
	WorkerThreadPool *pool = WorkerThreadPool::get_singleton();
	if (!preload_ || blocking) {
		// Old path (and blocking updates): on the main thread, one a frame or all of them.
		if (blocking && chunk_task_ >= 0) {
			pool->wait_for_task_completion(chunk_task_);
			chunk_task_ = -1;
			if (!chunks_.count(ckey(chunk_job_->data.cx, chunk_job_->data.cz))) {
				finish_chunk(chunk_job_->data);
			}
			delete chunk_job_;
			chunk_job_ = nullptr;
		}
		const int budget = blocking ? int(want.size()) : 1;
		for (int k = 0; k < budget && k < int(want.size()); ++k) {
			if (chunks_.count(ckey(want[k].x, want[k].z))) {
				continue;
			}
			const uint64_t t_c = now_us();
			build_chunk(want[k].x, want[k].z);
			if (!blocking) {
				stats_.chunk_ms.push_back(ms_since(t_c));
			}
		}
	} else {
		// A finished worker chunk: its nodes now (about a millisecond), then the next one to the worker.
		if (chunk_task_ >= 0 && pool->is_task_completed(chunk_task_)) {
			pool->wait_for_task_completion(chunk_task_);
			chunk_task_ = -1;
			stats_.chunk_job_ms.push_back(chunk_job_->wall_ms);
			if (chunk_adopt_pending_ && region_task_ < 0) {
				chunk_plan_->adopt_regions(*region_plan_);
				chunk_adopt_pending_ = false;
			}
			if (chunk_plan_->region_count() != adopted_regions_) {
				const uint64_t ta = now_us();
				plan_.adopt_regions(*chunk_plan_);
				adopted_regions_ = chunk_plan_->region_count();
				stats_.adopt_ms.push_back(ms_since(ta));
			}
			const ChunkData &d = chunk_job_->data;
			const bool wanted = Vector2((d.cx + 0.5f) * CHUNK - focus.x, (d.cz + 0.5f) * CHUNK - focus.z).length() < CHUNK_DROP;
			if (wanted && !chunks_.count(ckey(d.cx, d.cz))) {
				const uint64_t t_c = now_us();
				finish_chunk(chunk_job_->data);
				stats_.chunk_ms.push_back(ms_since(t_c));
			}
			delete chunk_job_;
			chunk_job_ = nullptr;
		}
		// The ground right by the player cannot wait for the worker.
		if (!want.empty() && want[0].d < CHUNK_URGENT && !(chunk_job_ && chunk_job_->data.cx == want[0].x && chunk_job_->data.cz == want[0].z)) {
			const uint64_t t_c = now_us();
			build_chunk(want[0].x, want[0].z);
			stats_.chunk_ms.push_back(ms_since(t_c));
			++stats_.chunk_fallbacks;
			UtilityFunctions::print(vformat("Chunk preload fallback: %d,%d at %.0f m built on the main thread", want[0].x, want[0].z, want[0].d));
		}
		if (chunk_task_ < 0) {
			for (const Want &w : want) {
				if (chunks_.count(ckey(w.x, w.z))) {
					continue;
				}
				chunk_job_ = new ChunkJob();
				chunk_job_->streamer = this;
				chunk_job_->plan = chunk_plan_.get();
				chunk_job_->data.cx = w.x;
				chunk_job_->data.cz = w.z;
				chunk_task_ = pool->add_native_task(&ChunkJob::run, chunk_job_, false, "Terrain chunk");
				break;
			}
		}
	}
	(void)deadline;
	std::vector<uint64_t> gone;
	for (auto &kv : chunks_) {
		const int x = int(int32_t(kv.first >> 32)), z = int(int32_t(kv.first & 0xFFFFFFFFu));
		if (Vector2((x + 0.5f) * CHUNK - focus.x, (z + 0.5f) * CHUNK - focus.z).length() > CHUNK_DROP) {
			gone.push_back(kv.first);
		}
	}
	for (uint64_t k : gone) {
		chunks_[k].root->queue_free();
		chunks_.erase(k);
	}
}

void WorldStreamer::horizon_update(const Vector3 &focus, bool blocking) {
	WorkerThreadPool *pool = WorkerThreadPool::get_singleton();
	if (horizon_task_ >= 0 && (blocking || pool->is_task_completed(horizon_task_))) {
		pool->wait_for_task_completion(horizon_task_);
		horizon_task_ = -1;
		const uint64_t t_h = now_us();
		if (!horizon_) {
			horizon_ = memnew(MeshInstance3D);
			horizon_->set_name("Horizon");
			horizon_->set_cast_shadows_setting(GeometryInstance3D::SHADOW_CASTING_SETTING_OFF);
			add_child(horizon_);
		}
		horizon_->set_mesh(MeshBuilder::mesh_from_arrays(horizon_job_->arrays));
		horizon_->set_material_override(toon_);
		if (!blocking) {
			stats_.horizon_ms.push_back(ms_since(t_h));
		}
		delete horizon_job_;
		horizon_job_ = nullptr;
	}
	if (horizon_task_ < 0 && Vector2(focus.x - horizon_center_.x, focus.z - horizon_center_.z).length() > 70.0f) {
		if (!preload_ || blocking || !horizon_) {
			const uint64_t t_h = now_us();
			build_horizon(focus);
			if (!blocking) {
				stats_.horizon_ms.push_back(ms_since(t_h));
			}
		} else {
			horizon_center_ = focus; // the ring is in hand; the mesh follows in a few frames
			horizon_job_ = new HorizonJob();
			horizon_job_->plan = &plan_;
			horizon_job_->center = focus;
			horizon_task_ = pool->add_native_task(&HorizonJob::run, horizon_job_, false, "Horizon");
		}
	}
}

// The old one-shot chunk: data and nodes on the calling (main) thread.
void WorldStreamer::build_chunk(int cx, int cz) {
	ChunkData d;
	d.cx = cx;
	d.cz = cz;
	make_chunk_data(plan_, d);
	finish_chunk(d);
}

// Heights, ground and road mesh, collision faces, lamp posts, signpost spots, woods dressing and grass:
// plain data from `plan` (the main thread's plan, or the workers' own copy) and the forest kit's sizes.
void WorldStreamer::make_chunk_data(WorldPlan &plan, ChunkData &out) const {
	const int cx = out.cx, cz = out.cz;
	const int N = int(CHUNK / STEP);
	const float ox = cx * CHUNK, oz = cz * CHUNK;
	std::vector<float> h((N + 1) * (N + 1));
	std::vector<float> settle((N + 1) * (N + 1));
	for (int z = 0; z <= N; ++z) {
		for (int x = 0; x <= N; ++x) {
			const float wx = ox + x * STEP, wz = oz + z * STEP;
			h[z * (N + 1) + x] = plan.height(wx, wz);
			settle[z * (N + 1) + x] = plan.settlement_mask(wx, wz);
		}
	}
	const uint64_t cseed = sub_seed(plan.seed(), 0xC4C4, uint64_t(int64_t(cx) + 100000), uint64_t(int64_t(cz) + 100000));
	const bool has_leaves = leaf_patch_.is_valid();

	// Ground: flat-shaded hills, lawn under settlements, meadow and moss in the woods (fallen leaves
	// are scattered as meshes, so the floor doesn't show blocky colour cells).
	// Inside a settlement's footprint the visible ground sits 4 cm under the level the settlement is
	// built on (its lawns, streets and paths lie a few mm above that level and would depth-fight with
	// it); collision stays at the true level.
	auto vis = [&](int x, int z) { return h[z * (N + 1) + x] - (settle[z * (N + 1) + x] > 0.999f ? 0.04f : 0.0f); };
	MeshBuilder &m = out.m;
	PackedVector3Array &faces = out.faces;
	faces.resize(N * N * 6);
	Vector3 *fw = faces.ptrw();
	int fi = 0;
	for (int z = 0; z < N; ++z) {
		for (int x = 0; x < N; ++x) {
			const Vector3 a(ox + x * STEP, vis(x, z), oz + z * STEP);
			const Vector3 b(ox + (x + 1) * STEP, vis(x + 1, z), oz + z * STEP);
			const Vector3 c(ox + (x + 1) * STEP, vis(x + 1, z + 1), oz + (z + 1) * STEP);
			const Vector3 d(ox + x * STEP, vis(x, z + 1), oz + (z + 1) * STEP);
			const float cxw = ox + (x + 0.5f) * STEP, czw = oz + (z + 0.5f) * STEP;
			const float s = settle[z * (N + 1) + x];
			int cell;
			if (s > 0.6f) {
				cell = mat_cell(pal::LAWN_DARK, mat::GRASS);
			} else {
				const float n = noise::fbm(plan.seed() ^ 0x77, cxw / 14.0f, czw / 14.0f, 2);
				const float woods = plan.forest_density(cxw, czw);
				if (woods > 0.55f && n > 0.15f) {
					cell = mat_cell(pal::MOSS, mat::GRASS);
				} else {
					cell = mat_cell(n < -0.3f ? pal::MOSS : (n < 0.1f ? pal::LAWN_DARK : pal::LAWN), mat::GRASS);
				}
			}
			m.quad_out(a, b, c, d, cell, (a + c) * 0.5f - Vector3(0, 1, 0));
			// Same diagonal (a-c) as the visual mesh, at the true heights.
			const Vector3 ca(a.x, h[z * (N + 1) + x], a.z), cb(b.x, h[z * (N + 1) + x + 1], b.z);
			const Vector3 cc(c.x, h[(z + 1) * (N + 1) + x + 1], c.z), cd(d.x, h[(z + 1) * (N + 1) + x], d.z);
			fw[fi++] = ca;
			fw[fi++] = cb;
			fw[fi++] = cc;
			fw[fi++] = ca;
			fw[fi++] = cc;
			fw[fi++] = cd;
		}
	}

	// Roads: segments whose midpoint falls inside this chunk, with gravel shoulders, a dashed
	// centre line and a warm street lamp (every other one with a jack-o'-lantern) every ~30 m.
	const RoadPlan *roads[18];
	int nroads = 0;
	plan.nearby_roads(ox + CHUNK * 0.5f, oz + CHUNK * 0.5f, roads, nroads);
	std::vector<ChunkData::Post> &posts = out.posts;
	out.inst.assign(ForestKit::COUNT + 4, {}); // + 3 maples, + fallen leaves
	const int LEAVES = ForestKit::COUNT + 3;
	std::vector<Transform3D> &road_leaves = out.inst[LEAVES];
	for (int k = 0; k < nroads; ++k) {
		const std::vector<Vector3> &pts = roads[k]->pts;
		// Where a road leaves a settlement it starts exactly as wide as the street and only a hair
		// above it, then narrows to a country road and lifts clear of the terrain over ~10 m.
		std::vector<float> cum(pts.size(), 0.0f);
		for (size_t i = 1; i < pts.size(); ++i) {
			cum[i] = cum[i - 1] + Vector2(pts[i].x - pts[i - 1].x, pts[i].z - pts[i - 1].z).length();
		}
		const float total = cum.back();
		const bool town_a = plan.settlement_mask(pts.front().x, pts.front().z) > 0.99f;
		const bool town_b = plan.settlement_mask(pts.back().x, pts.back().z) > 0.99f;
		auto near_end = [&](size_t i, float range) {
			return Math::max(town_a ? 1.0f - Math::smoothstep(0.0f, range, cum[i]) : 0.0f, town_b ? 1.0f - Math::smoothstep(0.0f, range, total - cum[i]) : 0.0f);
		};
		auto half_at = [&](size_t i) { return WorldPlan::ROAD_HALF + (Road().half_road + 0.1f - WorldPlan::ROAD_HALF) * near_end(i, 10.0f); };
		auto lift_at = [&](size_t i) { return Math::lerp(0.06f, 0.015f, near_end(i, 4.0f)); };
		for (size_t s = 0; s + 1 < pts.size(); ++s) {
			const Vector3 mid = (pts[s] + pts[s + 1]) * 0.5f;
			if (mid.x < ox || mid.x >= ox + CHUNK || mid.z < oz || mid.z >= oz + CHUNK) {
				continue;
			}
			auto left_at = [&](size_t i) {
				const Vector3 t = pts[std::min(i + 1, pts.size() - 1)] - pts[i > 0 ? i - 1 : 0];
				return Vector3(-t.z, 0, t.x).normalized();
			};
			const Vector3 l0 = left_at(s), l1 = left_at(s + 1);
			auto P = [&](size_t i, const Vector3 &l, float off, float lift) { return pts[i] + l * off + Vector3(0, lift, 0); };
			const float hr = WorldPlan::ROAD_HALF;
			const float h0 = half_at(s), h1 = half_at(s + 1), y0 = lift_at(s), y1 = lift_at(s + 1);
			m.quad_out(P(s, l0, -h0, y0), P(s + 1, l1, -h1, y1), P(s + 1, l1, h1, y1), P(s, l0, h0, y0), mat_cell(pal::ASPHALT, mat::ASPHALT),
					mid - Vector3(0, 1, 0));
			for (float sd : { -1.0f, 1.0f }) {
				m.quad_out(P(s, l0, sd * h0, y0), P(s + 1, l1, sd * h1, y1), P(s + 1, l1, sd * (h1 + 0.9f), y1 - 0.01f), P(s, l0, sd * (h0 + 0.9f), y0 - 0.01f),
						mat_cell(pal::STONE, mat::SOIL), mid - Vector3(0, 1, 0));
			}
			if (s % 2 == 0) {
				m.quad_out(P(s, l0, -0.09f, y0 + 0.02f), P(s + 1, l1, -0.09f, y1 + 0.02f), P(s + 1, l1, 0.09f, y1 + 0.02f), P(s, l0, 0.09f, y0 + 0.02f), pal::ROAD_LINE,
						mid - Vector3(0, 1, 0));
			}
			if (has_leaves && (s * 7 + k) % 3 == 0) {
				const float sd = (s & 1) ? 1.0f : -1.0f;
				road_leaves.push_back(Transform3D(Basis(Vector3(0, 1, 0), float(s) * 2.3f), P(s, l0, sd * (hr + 0.2f), 0.065f)));
			}
			if (s % 9 == 4) {
				const float sd = (s / 9) % 2 ? 1.0f : -1.0f;
				posts.push_back({ P(s, l0, sd * (hr + 1.3f), 0.0f), std::atan2(l0.x, l0.z) });
			}
		}
	}
	// Woods junctions get a fingerpost naming where each road leads (built with the nodes: its boards are
	// sized to the lettering).
	{
		int ri, rj;
		WorldPlan::region_of(ox + CHUNK * 0.5f, oz + CHUNK * 0.5f, ri, rj);
		for (int di = -1; di <= 1; ++di) {
			for (int dj = -1; dj <= 1; ++dj) {
				const RegionPlan &r = plan.region(ri + di, rj + dj);
				if (!r.settlement() && r.origin.x >= ox && r.origin.x < ox + CHUNK && r.origin.z >= oz && r.origin.z < oz + CHUNK) {
					out.signposts.push_back(r);
				}
			}
		}
	}

	// Woods and meadow dressing on a jittered grid, never on roads, junctions or in settlements.
	Rng rng(cseed);
	std::vector<std::vector<Transform3D>> &inst = out.inst;
	std::vector<Transform3D> &grass = out.grass;
	std::vector<Color> &grass_col = out.grass_col;
	auto ground_at = [&](float wx, float wz) {
		const float fx = Math::clamp((wx - ox) / STEP, 0.0f, float(N) - 0.001f), fz = Math::clamp((wz - oz) / STEP, 0.0f, float(N) - 0.001f);
		const int ix = int(fx), iz = int(fz);
		const float u = fx - ix, v = fz - iz;
		const float h00 = h[iz * (N + 1) + ix], h10 = h[iz * (N + 1) + ix + 1], h01 = h[(iz + 1) * (N + 1) + ix], h11 = h[(iz + 1) * (N + 1) + ix + 1];
		// Match the a-c diagonal triangulation.
		return u >= v ? h00 + (h10 - h00) * u + (h11 - h10) * v : h00 + (h11 - h01) * u + (h01 - h00) * v;
	};
	int fi_i, fi_j;
	WorldPlan::region_of(ox + CHUNK * 0.5f, oz + CHUNK * 0.5f, fi_i, fi_j);
	const float cell = 5.5f;
	for (float gz = oz + cell * 0.5f; gz < oz + CHUNK; gz += cell) {
		for (float gx = ox + cell * 0.5f; gx < ox + CHUNK; gx += cell) {
			const float wx = gx + rng.range(-2.2f, 2.2f), wz = gz + rng.range(-2.2f, 2.2f);
			const float roll = rng.f(), pick = rng.f(), yaw = rng.range(0.0f, Math_TAU), sc = rng.range(0.85f, 1.2f);
			if (wx < ox || wx >= ox + CHUNK || wz < oz || wz >= oz + CHUNK) {
				continue;
			}
			if (plan.settlement_mask(wx, wz) > 0.02f) {
				continue;
			}
			const float rd = plan.road_distance(wx, wz);
			if (rd < WorldPlan::ROAD_HALF + 3.0f) {
				continue;
			}
			bool junction = false;
			for (int di = -1; di <= 1 && !junction; ++di) {
				for (int dj = -1; dj <= 1; ++dj) {
					const RegionPlan &r = plan.region(fi_i + di, fi_j + dj);
					if (!r.settlement() && Vector2(wx - r.origin.x, wz - r.origin.z).length() < 12.0f) {
						junction = true;
						break;
					}
				}
			}
			if (junction) {
				continue;
			}
			const float dens = plan.forest_density(wx, wz);
			const Vector3 p(wx, ground_at(wx, wz) - 0.05f, wz);
			int kind = -1;
			if (roll < dens * 0.85f) {
				if (pick < 0.42f) {
					kind = ForestKit::PINE_A + rng.irange(0, 2);
				} else if (pick < 0.7f) {
					kind = ForestKit::COUNT + rng.irange(0, 2); // maples
				} else if (pick < 0.84f) {
					kind = ForestKit::SPOOKY_A + rng.irange(0, 1);
				} else {
					kind = ForestKit::BUSH;
				}
			} else if (roll < dens * 0.85f + 0.07f) {
				const int small[] = { ForestKit::ROCK_A, ForestKit::ROCK_B, ForestKit::STUMP, ForestKit::SHROOMS, ForestKit::SHROOMS, ForestKit::LOG, ForestKit::BUSH };
				kind = small[rng.irange(0, 6)];
			}
			if (kind >= 0) {
				inst[kind].push_back(Transform3D(Basis(Vector3(0, 1, 0), yaw).scaled(Vector3(sc, sc, sc)), p));
				if (has_leaves && (kind >= ForestKit::COUNT || kind <= ForestKit::SPOOKY_B)) {
					const float a = rng.range(0.0f, Math_TAU), r = rng.range(0.5f, 1.8f);
					const float lx = wx + std::cos(a) * r, lz = wz + std::sin(a) * r;
					if (lx >= ox && lx < ox + CHUNK && lz >= oz && lz < oz + CHUNK) {
						const float ls = rng.range(1.0f, 1.6f);
						inst[LEAVES].push_back(Transform3D(Basis(Vector3(0, 1, 0), yaw * 1.7f).scaled(Vector3(ls, 1.0f, ls)), Vector3(lx, ground_at(lx, lz) + 0.02f, lz)));
					}
				}
				float radius = 0.0f, height = 0.0f;
				if (kind >= ForestKit::COUNT) {
					radius = 0.32f;
					height = 3.0f;
				} else {
					radius = kit_.trunk_radius[kind];
					height = kit_.trunk_height[kind];
				}
				if (kind == ForestKit::LOG) {
					ChunkData::TreeShape t;
					t.log = true;
					t.size = Vector3(3.2f, 0.7f, 0.7f) * sc;
					t.xf = Transform3D(Basis(Vector3(0, 1, 0), yaw), p + Basis(Vector3(0, 1, 0), yaw).xform(Vector3(1.6f, 0.35f, 0)) * sc);
					out.shapes.push_back(t);
				} else if (radius > 0.0f) {
					ChunkData::TreeShape t;
					t.radius = radius * sc;
					t.height = height * sc;
					t.pos = p + Vector3(0, height * sc * 0.5f, 0);
					out.shapes.push_back(t);
				}
			}
		}
	}
	// Grass over every grassy bit of terrain (meadows, verges, clearings, the forest floor more
	// sparsely), tinted to the ground beneath; never on roads and shoulders, and not inside a
	// settlement's footprint (the settlement grows its own to match its lawns and verges).
	{
		Rng grng(cseed ^ 0x6A55);
		std::vector<float> rd((N + 1) * (N + 1));
		for (int z = 0; z <= N; ++z) {
			for (int x = 0; x <= N; ++x) {
				rd[z * (N + 1) + x] = plan.road_distance(ox + x * STEP, oz + z * STEP);
			}
		}
		auto lin = [](int cell) {
			const unsigned c = pal::RGB[cell];
			return Color(float((c >> 16) & 255) / 255.0f, float((c >> 8) & 255) / 255.0f, float(c & 255) / 255.0f).srgb_to_linear();
		};
		const float GAP = 1.2f;
		for (float gz = oz + GAP * 0.5f; gz < oz + CHUNK; gz += GAP) {
			for (float gx = ox + GAP * 0.5f; gx < ox + CHUNK; gx += GAP) {
				const float wx = gx + grng.range(-0.55f, 0.55f), wz = gz + grng.range(-0.55f, 0.55f);
				const int ix = Math::clamp(int((wx - ox) / STEP), 0, N - 1), iz = Math::clamp(int((wz - oz) / STEP), 0, N - 1);
				float near_road = 1e9f, cover = 0.0f;
				for (int k = 0; k < 4; ++k) {
					const int vi = (iz + (k >> 1)) * (N + 1) + ix + (k & 1);
					near_road = Math::min(near_road, rd[vi]);
					cover = Math::max(cover, settle[vi]);
				}
				if (cover > 0.999f || near_road < WorldPlan::ROAD_HALF + 1.6f) {
					continue;
				}
				const float n = noise::fbm(plan.seed() ^ 0x77, (ox + (ix + 0.5f) * STEP) / 14.0f, (oz + (iz + 0.5f) * STEP) / 14.0f, 2);
				const float woods = plan.forest_density(wx, wz);
				if (woods > 0.55f && grng.f() > 0.35f) {
					continue; // sparser on the forest floor
				}
				const int c = settle[iz * (N + 1) + ix] > 0.6f ? pal::LAWN_DARK : ((woods > 0.55f && n > 0.15f) || n < -0.3f ? pal::MOSS : (n < 0.1f ? pal::LAWN_DARK : pal::LAWN));
				const float gs = grng.range(0.8f, 1.35f);
				grass.push_back(Transform3D(Basis(Vector3(0, 1, 0), grng.range(0.0f, Math_TAU)).scaled(Vector3(gs, gs * grng.range(0.8f, 1.3f), gs)),
						Vector3(wx, ground_at(wx, wz) - 0.02f, wz)));
				grass_col.push_back(lin(c) * grng.range(0.85f, 1.12f));
			}
		}
	}
}

// The chunk's nodes, in the order the one-shot build always made them.
void WorldStreamer::finish_chunk(ChunkData &d) {
	const int cx = d.cx, cz = d.cz;
	Node3D *root = memnew(Node3D);
	root->set_name(String("Chunk_") + String::num_int64(cx) + "_" + String::num_int64(cz));
	add_child(root);
	chunks_[ckey(cx, cz)] = Chunk{ root };
	const int LEAVES = ForestKit::COUNT + 3;

	AssetLibrary &lib = templ_->assets();
	const AssetInfo *lamp = lib.get("lamp_post");
	for (size_t i = 0; i < d.posts.size(); ++i) {
		const ChunkData::Post &p = d.posts[i];
		// The lamp faces the road; posts sit on the left (+) or right (-) shoulder.
		const Transform3D xf(Basis(Vector3(0, 1, 0), p.yaw), p.p);
		Node3D *post = lib.instantiate("lamp_post");
		post->set_transform(xf);
		root->add_child(post);
		auto it = lamp->sockets.find("light");
		OmniLight3D *o = memnew(OmniLight3D);
		o->set_position(xf.xform(it != lamp->sockets.end() ? it->second.origin : Vector3(0, 3.2f, 0)));
		o->set_color(Color(1.0f, 0.78f, 0.5f));
		o->set_param(Light3D::PARAM_ENERGY, 1.5f);
		o->set_param(Light3D::PARAM_RANGE, 10.0f);
		o->set_param(Light3D::PARAM_ATTENUATION, 1.3f);
		root->add_child(o);
		if (((cx + cz + int(i)) & 1) == 0) {
			static const char *jacks[] = { "jackolantern_happy", "jackolantern_wink", "jackolantern_toothy" };
			Node3D *jack = lib.instantiate(jacks[((cx * 7 + cz * 3 + int(i)) % 3 + 3) % 3]);
			jack->set_transform(xf * Transform3D(Basis(Vector3(0, 1, 0), 0.3f).scaled(Vector3(0.8f, 0.8f, 0.8f)), Vector3(0.55f, 0, 0.35f)));
			root->add_child(jack);
		}
	}
	for (const RegionPlan &r : d.signposts) {
		build_signpost(d.m, root, r);
	}
	MeshInstance3D *mi = memnew(MeshInstance3D);
	mi->set_name("Ground");
	mi->set_mesh(d.signposts.empty() && !d.arrays.is_empty() ? MeshBuilder::mesh_from_arrays(d.arrays) : d.m.build());
	mi->set_material_override(toon_);
	root->add_child(mi);
	StaticBody3D *body = memnew(StaticBody3D);
	body->set_collision_layer(1 | 8);
	Ref<ConcavePolygonShape3D> shape;
	shape.instantiate();
	shape->set_faces(d.faces);
	shape->set_backface_collision_enabled(true);
	CollisionShape3D *cs = memnew(CollisionShape3D);
	cs->set_shape(shape);
	body->add_child(cs);
	root->add_child(body);

	// Tree trunks and logs: the shapes go on the body before it joins the tree (one shape build, not one
	// per trunk).
	StaticBody3D *trees = memnew(StaticBody3D);
	trees->set_collision_layer(1);
	for (const ChunkData::TreeShape &t : d.shapes) {
		CollisionShape3D *c = memnew(CollisionShape3D);
		if (t.log) {
			Ref<BoxShape3D> bx;
			bx.instantiate();
			bx->set_size(t.size);
			c->set_shape(bx);
			c->set_transform(t.xf);
		} else {
			Ref<CylinderShape3D> cyl;
			cyl.instantiate();
			cyl->set_radius(t.radius);
			cyl->set_height(t.height);
			c->set_shape(cyl);
			c->set_position(t.pos);
		}
		trees->add_child(c);
	}
	root->add_child(trees);
	for (int kind = 0; kind < ForestKit::COUNT + 4; ++kind) {
		if (d.inst[kind].empty()) {
			continue;
		}
		Ref<MultiMesh> mm;
		mm.instantiate();
		mm->set_transform_format(MultiMesh::TRANSFORM_3D);
		mm->set_mesh(kind == LEAVES ? leaf_patch_ : (kind >= ForestKit::COUNT ? maples_[kind - ForestKit::COUNT] : Ref<Mesh>(kit_.mesh[kind])));
		mm->set_instance_count(int(d.inst[kind].size()));
		for (int i = 0; i < int(d.inst[kind].size()); ++i) {
			mm->set_instance_transform(i, d.inst[kind][i]);
		}
		MultiMeshInstance3D *mmi = memnew(MultiMeshInstance3D);
		mmi->set_multimesh(mm);
		mmi->set_material_override(toon_);
		const bool small = kind == ForestKit::SHROOMS || kind == ForestKit::ROCK_A || kind == ForestKit::STUMP || kind == ForestKit::BUSH || kind == LEAVES;
		mmi->set_cast_shadows_setting(small ? GeometryInstance3D::SHADOW_CASTING_SETTING_OFF : GeometryInstance3D::SHADOW_CASTING_SETTING_ON);
		mmi->set_visibility_range_end(small ? 90.0f : 190.0f);
		if ((kind >= ForestKit::COUNT && kind != LEAVES) || kind <= ForestKit::SPOOKY_B) {
			mmi->set_instance_shader_parameter("wind", 0.6f);
		}
		root->add_child(mmi);
	}
	if (!d.grass.empty() && grass_.is_valid()) {
		Ref<MultiMesh> mm;
		mm.instantiate();
		mm->set_transform_format(MultiMesh::TRANSFORM_3D);
		mm->set_use_colors(true);
		mm->set_mesh(grass_mesh_);
		mm->set_instance_count(int(d.grass.size()));
		for (int i = 0; i < int(d.grass.size()); ++i) {
			mm->set_instance_transform(i, d.grass[i]);
			mm->set_instance_color(i, d.grass_col[i]);
		}
		MultiMeshInstance3D *mmi = memnew(MultiMeshInstance3D);
		mmi->set_multimesh(mm);
		mmi->set_material_override(grass_);
		mmi->set_cast_shadows_setting(GeometryInstance3D::SHADOW_CASTING_SETTING_OFF);
		mmi->set_visibility_range_end(60.0f);
		mmi->set_visibility_range_end_margin(10.0f);
		mmi->set_visibility_range_fade_mode(GeometryInstance3D::VISIBILITY_RANGE_FADE_SELF);
		root->add_child(mmi);
	}
}

// A wooden fingerpost beside a woods junction: one arrow board per road, lettered with the place
// it leads to (both faces), stacked up the post.
void WorldStreamer::build_signpost(MeshBuilder &m, Node3D *root, const RegionPlan &r) {
	struct Arm {
		Vector3 dir;
		std::string name;
	};
	std::vector<Arm> arms;
	Vector3 sum;
	const int nb[4][2] = { { 1, 0 }, { -1, 0 }, { 0, 1 }, { 0, -1 } };
	for (const auto &d : nb) {
		const RoadPlan *road = plan_.road(r.i, r.j, r.i + d[0], r.j + d[1]);
		if (!road || road->pts.size() < 3) {
			continue;
		}
		const bool front = road->pts.front().distance_to(r.origin) < road->pts.back().distance_to(r.origin);
		const Vector3 a = front ? road->pts[0] : road->pts.back(), b = front ? road->pts[2] : road->pts[road->pts.size() - 3];
		const Vector3 dir = Vector3(b.x - a.x, 0, b.z - a.z).normalized();
		arms.push_back({ dir, plan_.region(r.i + d[0], r.j + d[1]).name });
		sum += dir;
	}
	if (arms.empty()) {
		return;
	}
	// Stand in the widest gap between the roads, clear of the asphalt.
	Vector3 away = sum.length() > 0.3f ? -sum.normalized() : Vector3(0.7071f, 0, 0.7071f);
	Vector3 p = r.origin + away * (WorldPlan::ROAD_HALF + 2.4f);
	for (int k = 0; k < 8 && plan_.road_distance(p.x, p.z) < WorldPlan::ROAD_HALF + 1.2f; ++k) {
		away = away.rotated(Vector3(0, 1, 0), Math_PI * 0.25f);
		p = r.origin + away * (WorldPlan::ROAD_HALF + 2.4f);
	}
	p.y = plan_.height(p.x, p.z) - 0.05f;
	const int wood = mat_cell(pal::WOOD_DARK, mat::WOOD), board = mat_cell(pal::WOOD_LIGHT, mat::PLANKS);
	m.cylinder(p, 0.09f, 2.9f, 6, wood, 0.07f);
	// A jack-o'-lantern on top lights the boards.
	Node3D *jack = templ_->assets().instantiate("jackolantern_happy");
	jack->set_transform(Transform3D(Basis(Vector3(0, 1, 0), std::atan2(-away.x, -away.z)).scaled(Vector3(0.6f, 0.6f, 0.6f)), p + Vector3(0, 2.9f, 0)));
	root->add_child(jack);
	OmniLight3D *glow = memnew(OmniLight3D);
	glow->set_color(Color(1.0f, 0.6f, 0.28f));
	glow->set_param(Light3D::PARAM_ENERGY, 1.1f);
	glow->set_param(Light3D::PARAM_RANGE, 6.0f);
	glow->set_position(p + Vector3(0, 3.2f, 0) - away * 0.6f);
	root->add_child(glow);
	for (size_t k = 0; k < arms.size(); ++k) {
		const Vector3 d = arms[k].dir;
		const Vector3 side = Vector3(0, 1, 0).cross(d);
		const float y = 2.45f - 0.48f * float(k);
		// The lettering sets the board: a long place name gets a longer arm (never smaller text).
		const String name = String::utf8(arms[k].name.c_str()).to_upper();
		Label3D *labels[2];
		for (int f = 0; f < 2; ++f) {
			Label3D *l = memnew(Label3D);
			l->set_font(display_font());
			l->set_text(name);
			l->set_font_size(64);
			l->set_outline_size(14);
			l->set_pixel_size(0.0056f);
			l->set_modulate(Color(1.0f, 0.98f, 0.94f));
			l->set_outline_modulate(Color(0.22f, 0.12f, 0.07f));
			l->set_draw_flag(Label3D::FLAG_DOUBLE_SIDED, false);
			fit_label(l, 1e3f, 0.32f); // only the height is fixed (the board is 0.4 m tall)
			labels[f] = l;
		}
		const float half = Math::max(0.6f, label_width(labels[0]) * 0.5f + 0.12f);
		const Vector3 c = p + Vector3(0, y, 0) + d * (half + 0.02f);
		m.set_transform(Transform3D(Basis(d, Vector3(0, 1, 0), d.cross(Vector3(0, 1, 0))), c));
		m.rounded_box(Vector3(), Vector3(half, 0.2f, 0.035f), 0.02f, board);
		m.set_transform(Transform3D());
		const Vector3 tip = c + d * (half + 0.18f);
		for (float sd : { -1.0f, 1.0f }) {
			m.tri_out(c + d * half + Vector3(0, 0.2f, 0) + side * (0.035f * sd), tip + side * (0.035f * sd), c + d * half - Vector3(0, 0.2f, 0) + side * (0.035f * sd), board,
					c + d * half - side * sd);
		}
		for (int f = 0; f < 2; ++f) {
			// Each face reads toward the arrow's point.
			const Vector3 face = side * (f == 0 ? -1.0f : 1.0f);
			const Vector3 x = face.cross(Vector3(0, 1, 0)) * -1.0f;
			labels[f]->set_transform(Transform3D(Basis(x, Vector3(0, 1, 0), face), c + face * 0.045f + d * 0.05f));
			root->add_child(labels[f]);
		}
	}
}

// ---------------------------------------------------------------- horizon
// A coarse ring of hills out to the fog, with a hole where the detailed chunks are, sunk a little
// so it never pokes through them.
void WorldStreamer::build_horizon(const Vector3 &center) {
	horizon_center_ = center;
	HorizonJob j;
	j.plan = &plan_;
	j.center = center;
	HorizonJob::run(&j);
	if (!horizon_) {
		horizon_ = memnew(MeshInstance3D);
		horizon_->set_name("Horizon");
		horizon_->set_cast_shadows_setting(GeometryInstance3D::SHADOW_CASTING_SETTING_OFF);
		add_child(horizon_);
	}
	horizon_->set_mesh(MeshBuilder::mesh_from_arrays(j.arrays));
	horizon_->set_material_override(toon_);
}

// ---------------------------------------------------------------- settlements (old path)
void WorldStreamer::load_area(const RegionPlan &r) {
	const uint64_t t_attach = now_us();
	const uint64_t k = area_key(r.i, r.j);
	Area a;
	a.key = k;
	a.plan = r;
	a.state = &states_[k];
	NeighborhoodGenerator *gen = memnew(NeighborhoodGenerator);
	gen->set_name(String(r.name.c_str()));
	gen->set_toon_material(templ_->get_toon_material());
	gen->set_candy_material(templ_->get_candy_material());
	gen->set_grass_material(templ_->get_grass_material());
	gen->set_magic_material(templ_->get_magic_material());
	gen->share_assets(&templ_->assets());
	gen->set_candy_filter(a.state->candy_taken);
	add_child(gen);
	gen->set_position(r.origin);
	const uint64_t t0 = Time::get_singleton()->get_ticks_usec();
	const Dictionary gst = gen->generate_settlement(settlement_seed(r), r.kind == RegionKind::Town ? SettlementKind::Town : SettlementKind::Neighborhood, r.lane_open, r.hood_port);
	UtilityFunctions::print("Loaded ", String(r.name.c_str()), " in ", int((Time::get_singleton()->get_ticks_usec() - t0) / 1000), " ms (plan ", gst.get("plan_ms", 0), ", build ",
			gst.get("build_ms", 0), ", lots ", gst.get("lots", 0), ", props ", gst.get("props", 0), ")");
	if (!blocking_now_) {
		stats_.gen_ms.push_back(float(Time::get_singleton()->get_ticks_usec() - t0) / 1000.0f);
	}
	if (OS::get_singleton()->get_cmdline_user_args().has("--stream-probe")) {
		probe_area(gen);
	}
	a.gen = gen;
	AreaState &st = *a.state;
	const int lots = gen->get_lot_count();
	if (!st.visited) {
		st.visited = true;
		st.candy_total = gen->candy_field() ? gen->candy_field()->total_value() : 0;
		st.candy_taken.assign(gen->context()->candy.size(), 0);
		st.rung.assign(lots, 0);
		st.lot_cleared.assign(lots, 0);
	} else if (gen->context()) {
		reconcile_state(st, *gen->context());
	}
	for (int l = 0; l < lots && l < int(st.rung.size()); ++l) {
		if (st.rung[l]) {
			gen->open_door(l);
		}
		if (st.lot_cleared[l]) {
			gen->close_house(l, false);
		}
	}
	Area &stored = areas_[k] = a;
	since_attach_ = 0;
	verify_area(stored);
	const uint64_t t_cb = now_us();
	if (on_area_loaded) {
		on_area_loaded(stored);
	}
	if (on_area_loaded_step) {
		on_area_loaded_step(stored, 0);
	}
	if (state_test_ && k == area_key(0, 0)) {
		state_test_check(stored);
	}
	if (!blocking_now_) {
		stats_.callback_ms.push_back(ms_since(t_cb));
		stats_.attach_ms.push_back(ms_since(t_attach));
		stats_.attach_total_ms.push_back(ms_since(t_attach));
		stats_.attach_frames.push_back(1);
	}
	UtilityFunctions::print(vformat("Attached %s: %.1f ms (on_area_loaded %.1f ms)", String(r.name.c_str()), ms_since(t_attach), ms_since(t_cb)));
}

void WorldStreamer::unload_area(uint64_t key) {
	auto it = areas_.find(key);
	if (it == areas_.end()) {
		return;
	}
	if (on_area_unloading) {
		on_area_unloading(it->second);
	}
	if (it->second.extras) {
		it->second.extras->queue_free();
	}
	it->second.gen->queue_free();
	areas_.erase(it);
}

} // namespace tot
