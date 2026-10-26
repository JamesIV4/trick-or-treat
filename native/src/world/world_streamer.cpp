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
#include <godot_cpp/classes/static_body3d.hpp>
#include <godot_cpp/core/class_db.hpp>
#include <godot_cpp/core/math.hpp>
#include <godot_cpp/classes/os.hpp>
#include <godot_cpp/classes/time.hpp>
#include <godot_cpp/variant/utility_functions.hpp>

#include <algorithm>
#include <cmath>

#include "core/noise.h"
#include "gen/neighborhood_generator.h"
#include "mesh/mesh_builder.h"
#include "play/candy_field.h"

using namespace godot;

namespace tot {

namespace {
constexpr float CHUNK_LOAD = 170.0f, CHUNK_DROP = 230.0f;
constexpr float AREA_LOAD = 95.0f, AREA_DROP = 175.0f; // distance from the footprint rectangle
constexpr float HORIZON_R = 1100.0f, HORIZON_HOLE = 150.0f, HORIZON_STEP = 22.0f;

float rect_distance(const RegionPlan &r, float x, float z) {
	const float dx = Math::max(0.0f, Math::max(r.x0 - x, x - r.x1));
	const float dz = Math::max(0.0f, Math::max(r.z0 - z, z - r.z1));
	return std::sqrt(dx * dx + dz * dz);
}
} // namespace

void WorldStreamer::_bind_methods() {
}

void WorldStreamer::setup(NeighborhoodGenerator *templ, uint64_t seed) {
	if (OS::get_singleton()->get_cmdline_user_args().has("--dump-roads")) {
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
	templ_ = templ;
	toon_ = templ->get_toon_material();
	grass_ = templ->get_grass_material();
	plan_.reset(seed);
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
	Rng rng(sub_seed(seed, 0x300D));
	const float az = rng.range(0.0f, Math_TAU), elev = rng.range(0.16f, 0.22f);
	moon_dir_ = Vector3(std::cos(az) * std::cos(elev), std::sin(elev), std::sin(az) * std::cos(elev));
	horizon_center_ = Vector3(1e9f, 0, 0);
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

void WorldStreamer::update(const Vector3 &focus, bool blocking) {
	// Settlements first (they are the destination), at most one per frame unless blocking.
	int fi, fj;
	WorldPlan::region_of(focus.x, focus.z, fi, fj);
	bool loaded = false;
	for (int di = -1; di <= 1 && (blocking || !loaded); ++di) {
		for (int dj = -1; dj <= 1 && (blocking || !loaded); ++dj) {
			const RegionPlan &r = plan_.region(fi + di, fj + dj);
			if (r.settlement() && !areas_.count(area_key(r.i, r.j)) && rect_distance(r, focus.x, focus.z) < AREA_LOAD) {
				load_area(r);
				loaded = true;
			}
		}
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
	std::sort(want.begin(), want.end(), [](const Want &a, const Want &b) { return a.d < b.d; });
	const int budget = blocking ? int(want.size()) : 1;
	for (int k = 0; k < budget && k < int(want.size()); ++k) {
		build_chunk(want[k].x, want[k].z);
	}
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
	if (Vector2(focus.x - horizon_center_.x, focus.z - horizon_center_.z).length() > 70.0f) {
		build_horizon(focus);
	}
}

// ---------------------------------------------------------------- terrain chunk
void WorldStreamer::build_chunk(int cx, int cz) {
	const int N = int(CHUNK / STEP);
	const float ox = cx * CHUNK, oz = cz * CHUNK;
	std::vector<float> h((N + 1) * (N + 1));
	std::vector<float> settle((N + 1) * (N + 1));
	for (int z = 0; z <= N; ++z) {
		for (int x = 0; x <= N; ++x) {
			const float wx = ox + x * STEP, wz = oz + z * STEP;
			h[z * (N + 1) + x] = plan_.height(wx, wz);
			settle[z * (N + 1) + x] = plan_.settlement_mask(wx, wz);
		}
	}
	Node3D *root = memnew(Node3D);
	root->set_name(String("Chunk_") + String::num_int64(cx) + "_" + String::num_int64(cz));
	add_child(root);
	chunks_[ckey(cx, cz)] = Chunk{ root };
	const uint64_t cseed = sub_seed(plan_.seed(), 0xC4C4, uint64_t(int64_t(cx) + 100000), uint64_t(int64_t(cz) + 100000));

	// Ground: flat-shaded hills, lawn under settlements, meadow and moss in the woods (fallen leaves
	// are scattered as meshes, so the floor doesn't show blocky colour cells).
	// Inside a settlement's footprint the visible ground sits 4 cm under the level the settlement is
	// built on (its lawns, streets and paths lie a few mm above that level and would depth-fight with
	// it); collision stays at the true level.
	auto vis = [&](int x, int z) { return h[z * (N + 1) + x] - (settle[z * (N + 1) + x] > 0.999f ? 0.04f : 0.0f); };
	MeshBuilder m;
	PackedVector3Array faces;
	faces.resize(N * N * 6);
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
				const float n = noise::fbm(plan_.seed() ^ 0x77, cxw / 14.0f, czw / 14.0f, 2);
				const float woods = plan_.forest_density(cxw, czw);
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
			faces.set(fi++, ca);
			faces.set(fi++, cb);
			faces.set(fi++, cc);
			faces.set(fi++, ca);
			faces.set(fi++, cc);
			faces.set(fi++, cd);
		}
	}

	// Roads: segments whose midpoint falls inside this chunk, with gravel shoulders, a dashed
	// centre line and a warm street lamp (every other one with a jack-o'-lantern) every ~30 m.
	const RoadPlan *roads[18];
	int nroads = 0;
	plan_.nearby_roads(ox + CHUNK * 0.5f, oz + CHUNK * 0.5f, roads, nroads);
	struct Post {
		Vector3 p;
		float yaw;
	};
	std::vector<Post> posts;
	std::vector<Transform3D> road_leaves;
	for (int k = 0; k < nroads; ++k) {
		const std::vector<Vector3> &pts = roads[k]->pts;
		// Where a road leaves a settlement it starts exactly as wide as the street and only a hair
		// above it, then narrows to a country road and lifts clear of the terrain over ~10 m.
		std::vector<float> cum(pts.size(), 0.0f);
		for (size_t i = 1; i < pts.size(); ++i) {
			cum[i] = cum[i - 1] + Vector2(pts[i].x - pts[i - 1].x, pts[i].z - pts[i - 1].z).length();
		}
		const float total = cum.back();
		const bool town_a = plan_.settlement_mask(pts.front().x, pts.front().z) > 0.99f;
		const bool town_b = plan_.settlement_mask(pts.back().x, pts.back().z) > 0.99f;
		auto near_end = [&](size_t i, float range) {
			return Math::max(town_a ? 1.0f - Math::smoothstep(0.0f, range, cum[i]) : 0.0f, town_b ? 1.0f - Math::smoothstep(0.0f, range, total - cum[i]) : 0.0f);
		};
		auto half_at = [&](size_t i) { return WorldPlan::ROAD_HALF + (StreetPlan().half_road + 0.1f - WorldPlan::ROAD_HALF) * near_end(i, 10.0f); };
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
			if (leaf_patch_.is_valid() && (s * 7 + k) % 3 == 0) {
				const float sd = (s & 1) ? 1.0f : -1.0f;
				road_leaves.push_back(Transform3D(Basis(Vector3(0, 1, 0), float(s) * 2.3f), P(s, l0, sd * (hr + 0.2f), 0.065f)));
			}
			if (s % 9 == 4) {
				const float sd = (s / 9) % 2 ? 1.0f : -1.0f;
				posts.push_back({ P(s, l0, sd * (hr + 1.3f), 0.0f), std::atan2(l0.x, l0.z) });
			}
		}
	}
	AssetLibrary &lib = templ_->assets();
	const AssetInfo *lamp = lib.get("lamp_post");
	for (size_t i = 0; i < posts.size(); ++i) {
		const Post &p = posts[i];
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
	// Woods junctions get a fingerpost naming where each road leads.
	{
		int ri, rj;
		WorldPlan::region_of(ox + CHUNK * 0.5f, oz + CHUNK * 0.5f, ri, rj);
		for (int di = -1; di <= 1; ++di) {
			for (int dj = -1; dj <= 1; ++dj) {
				const RegionPlan &r = plan_.region(ri + di, rj + dj);
				if (!r.settlement() && r.origin.x >= ox && r.origin.x < ox + CHUNK && r.origin.z >= oz && r.origin.z < oz + CHUNK) {
					build_signpost(m, root, r);
				}
			}
		}
	}
	MeshInstance3D *mi = memnew(MeshInstance3D);
	mi->set_name("Ground");
	mi->set_mesh(m.build());
	mi->set_material_override(toon_);
	root->add_child(mi);
	StaticBody3D *body = memnew(StaticBody3D);
	body->set_collision_layer(1 | 8);
	Ref<ConcavePolygonShape3D> shape;
	shape.instantiate();
	shape->set_faces(faces);
	shape->set_backface_collision_enabled(true);
	CollisionShape3D *cs = memnew(CollisionShape3D);
	cs->set_shape(shape);
	body->add_child(cs);
	root->add_child(body);

	// Woods and meadow dressing on a jittered grid, never on roads, junctions or in settlements.
	Rng rng(cseed);
	std::vector<std::vector<Transform3D>> inst(ForestKit::COUNT + 4); // + 3 maples, + fallen leaves
	const int LEAVES = ForestKit::COUNT + 3;
	inst[LEAVES] = road_leaves;
	std::vector<Transform3D> grass;
	std::vector<Color> grass_col;
	StaticBody3D *trees = memnew(StaticBody3D);
	trees->set_collision_layer(1);
	root->add_child(trees);
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
			if (plan_.settlement_mask(wx, wz) > 0.02f) {
				continue;
			}
			const float rd = plan_.road_distance(wx, wz);
			if (rd < WorldPlan::ROAD_HALF + 3.0f) {
				continue;
			}
			bool junction = false;
			for (int di = -1; di <= 1 && !junction; ++di) {
				for (int dj = -1; dj <= 1; ++dj) {
					const RegionPlan &r = plan_.region(fi_i + di, fi_j + dj);
					if (!r.settlement() && Vector2(wx - r.origin.x, wz - r.origin.z).length() < 12.0f) {
						junction = true;
						break;
					}
				}
			}
			if (junction) {
				continue;
			}
			const float dens = plan_.forest_density(wx, wz);
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
				if (leaf_patch_.is_valid() && (kind >= ForestKit::COUNT || kind <= ForestKit::SPOOKY_B)) {
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
					Ref<BoxShape3D> bx;
					bx.instantiate();
					bx->set_size(Vector3(3.2f, 0.7f, 0.7f) * sc);
					CollisionShape3D *c = memnew(CollisionShape3D);
					c->set_shape(bx);
					c->set_transform(Transform3D(Basis(Vector3(0, 1, 0), yaw), p + Basis(Vector3(0, 1, 0), yaw).xform(Vector3(1.6f, 0.35f, 0)) * sc));
					trees->add_child(c);
				} else if (radius > 0.0f) {
					Ref<CylinderShape3D> cyl;
					cyl.instantiate();
					cyl->set_radius(radius * sc);
					cyl->set_height(height * sc);
					CollisionShape3D *c = memnew(CollisionShape3D);
					c->set_shape(cyl);
					c->set_position(p + Vector3(0, height * sc * 0.5f, 0));
					trees->add_child(c);
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
				rd[z * (N + 1) + x] = plan_.road_distance(ox + x * STEP, oz + z * STEP);
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
				const float n = noise::fbm(plan_.seed() ^ 0x77, (ox + (ix + 0.5f) * STEP) / 14.0f, (oz + (iz + 0.5f) * STEP) / 14.0f, 2);
				const float woods = plan_.forest_density(wx, wz);
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
	for (int kind = 0; kind < ForestKit::COUNT + 4; ++kind) {
		if (inst[kind].empty()) {
			continue;
		}
		Ref<MultiMesh> mm;
		mm.instantiate();
		mm->set_transform_format(MultiMesh::TRANSFORM_3D);
		mm->set_mesh(kind == LEAVES ? leaf_patch_ : (kind >= ForestKit::COUNT ? maples_[kind - ForestKit::COUNT] : Ref<Mesh>(kit_.mesh[kind])));
		mm->set_instance_count(int(inst[kind].size()));
		for (int i = 0; i < int(inst[kind].size()); ++i) {
			mm->set_instance_transform(i, inst[kind][i]);
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
	if (!grass.empty() && grass_.is_valid()) {
		Ref<MultiMesh> mm;
		mm.instantiate();
		mm->set_transform_format(MultiMesh::TRANSFORM_3D);
		mm->set_use_colors(true);
		mm->set_mesh(grass_mesh_);
		mm->set_instance_count(int(grass.size()));
		for (int i = 0; i < int(grass.size()); ++i) {
			mm->set_instance_transform(i, grass[i]);
			mm->set_instance_color(i, grass_col[i]);
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
		}	}
}

// ---------------------------------------------------------------- horizon
// A coarse ring of hills out to the fog, with a hole where the detailed chunks are, sunk a little
// so it never pokes through them.
void WorldStreamer::build_horizon(const Vector3 &center) {
	horizon_center_ = center;
	const float cx = std::round(center.x / HORIZON_STEP) * HORIZON_STEP, cz = std::round(center.z / HORIZON_STEP) * HORIZON_STEP;
	const int n = int(HORIZON_R / HORIZON_STEP);
	std::vector<float> h((2 * n + 1) * (2 * n + 1));
	for (int z = -n; z <= n; ++z) {
		for (int x = -n; x <= n; ++x) {
			const float wx = cx + x * HORIZON_STEP, wz = cz + z * HORIZON_STEP;
			h[(z + n) * (2 * n + 1) + (x + n)] = plan_.natural(wx, wz) - 1.2f;
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
	if (!horizon_) {
		horizon_ = memnew(MeshInstance3D);
		horizon_->set_name("Horizon");
		horizon_->set_cast_shadows_setting(GeometryInstance3D::SHADOW_CASTING_SETTING_OFF);
		add_child(horizon_);
	}
	horizon_->set_mesh(m.build());
	horizon_->set_material_override(toon_);
}

// ---------------------------------------------------------------- settlements
void WorldStreamer::load_area(const RegionPlan &r) {
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
	gen->generate_settlement(settlement_seed(r), r.kind == RegionKind::Town ? SettlementKind::Town : SettlementKind::Neighborhood, r.lane_open);
	UtilityFunctions::print("Loaded ", String(r.name.c_str()), " in ", int((Time::get_singleton()->get_ticks_usec() - t0) / 1000), " ms");
	a.gen = gen;
	AreaState &st = *a.state;
	const int lots = gen->get_lot_count();
	if (!st.visited) {
		st.visited = true;
		st.candy_total = gen->candy_field() ? gen->candy_field()->total_value() : 0;
		st.candy_taken.assign(gen->context()->candy.size(), 0);
		st.rung.assign(lots, 0);
		st.lot_cleared.assign(lots, 0);
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
	if (on_area_loaded) {
		on_area_loaded(stored);
	}
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
