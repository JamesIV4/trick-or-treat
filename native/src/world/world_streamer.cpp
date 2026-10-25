#include "world/world_streamer.h"

#include <godot_cpp/classes/collision_shape3d.hpp>
#include <godot_cpp/classes/concave_polygon_shape3d.hpp>
#include <godot_cpp/classes/cylinder_shape3d.hpp>
#include <godot_cpp/classes/box_shape3d.hpp>
#include <godot_cpp/classes/multi_mesh.hpp>
#include <godot_cpp/classes/multi_mesh_instance3d.hpp>
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
		const char *ids[3] = { "tree_maple_red", "tree_maple_orange", "tree_maple_gold" };
		for (int i = 0; i < 3; ++i) {
			maples_[i] = templ->assets().merged_mesh(ids[i]);
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

	// Ground: flat-shaded hills, lawn under settlements, meadow and leaf-litter patches in the woods.
	MeshBuilder m;
	PackedVector3Array faces;
	faces.resize(N * N * 6);
	int fi = 0;
	for (int z = 0; z < N; ++z) {
		for (int x = 0; x < N; ++x) {
			const Vector3 a(ox + x * STEP, h[z * (N + 1) + x], oz + z * STEP);
			const Vector3 b(ox + (x + 1) * STEP, h[z * (N + 1) + x + 1], oz + z * STEP);
			const Vector3 c(ox + (x + 1) * STEP, h[(z + 1) * (N + 1) + x + 1], oz + (z + 1) * STEP);
			const Vector3 d(ox + x * STEP, h[(z + 1) * (N + 1) + x], oz + (z + 1) * STEP);
			const float cxw = ox + (x + 0.5f) * STEP, czw = oz + (z + 0.5f) * STEP;
			const float s = settle[z * (N + 1) + x];
			int cell;
			if (s > 0.6f) {
				cell = mat_cell(pal::LAWN_DARK, mat::GRASS);
			} else {
				const float n = noise::fbm(plan_.seed() ^ 0x77, cxw / 14.0f, czw / 14.0f, 2);
				const float woods = plan_.forest_density(cxw, czw);
				if (woods > 0.55f && n > 0.15f) {
					cell = mat_cell(n > 0.45f ? pal::RUST_LEAF : pal::CINNAMON, n > 0.45f ? mat::LEAVES : mat::SOIL);
				} else {
					cell = mat_cell(n < -0.3f ? pal::MOSS : (n < 0.1f ? pal::LAWN_DARK : pal::LAWN), mat::GRASS);
				}
			}
			m.quad_out(a, b, c, d, cell, (a + c) * 0.5f - Vector3(0, 1, 0));
			// Same diagonal (a-c) as the visual mesh.
			faces.set(fi++, a);
			faces.set(fi++, b);
			faces.set(fi++, c);
			faces.set(fi++, a);
			faces.set(fi++, c);
			faces.set(fi++, d);
		}
	}

	// Roads: segments whose midpoint falls inside this chunk, with gravel shoulders, a dashed
	// centre line and glowing lantern posts every so often.
	const RoadPlan *roads[18];
	int nroads = 0;
	plan_.nearby_roads(ox + CHUNK * 0.5f, oz + CHUNK * 0.5f, roads, nroads);
	struct Post {
		Vector3 p;
		float yaw;
	};
	std::vector<Post> posts;
	for (int k = 0; k < nroads; ++k) {
		const std::vector<Vector3> &pts = roads[k]->pts;
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
			m.quad_out(P(s, l0, -hr, 0.06f), P(s + 1, l1, -hr, 0.06f), P(s + 1, l1, hr, 0.06f), P(s, l0, hr, 0.06f), mat_cell(pal::ASPHALT, mat::ASPHALT),
					mid - Vector3(0, 1, 0));
			for (float sd : { -1.0f, 1.0f }) {
				m.quad_out(P(s, l0, sd * hr, 0.06f), P(s + 1, l1, sd * hr, 0.06f), P(s + 1, l1, sd * (hr + 0.9f), 0.03f), P(s, l0, sd * (hr + 0.9f), 0.03f),
						mat_cell(pal::STONE, mat::PAVERS), mid - Vector3(0, 1, 0));
			}
			if (s % 2 == 0) {
				m.quad_out(P(s, l0, -0.09f, 0.075f), P(s + 1, l1, -0.09f, 0.075f), P(s + 1, l1, 0.09f, 0.075f), P(s, l0, 0.09f, 0.075f), pal::ROAD_LINE,
						mid - Vector3(0, 1, 0));
			}
			if (s % 9 == 4) {
				const float sd = (s / 9) % 2 ? 1.0f : -1.0f;
				posts.push_back({ P(s, l0, sd * (hr + 1.3f), 0.0f), std::atan2(l0.x, l0.z) });
			}
		}
	}
	for (const Post &p : posts) {
		m.cylinder(p.p, 0.09f, 1.5f, 6, mat_cell(pal::WOOD_DARK, mat::WOOD));
		m.blob(p.p + Vector3(0, 1.62f, 0), Vector3(0.2f, 0.18f, 0.2f), pal::GLOW_JACK, 1);
		m.cylinder(p.p + Vector3(0, 1.77f, 0), 0.03f, 0.08f, 5, pal::STEM_GREEN);
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
	std::vector<std::vector<Transform3D>> inst(ForestKit::COUNT + 3);
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
			// Meadow grass where the woods open up.
			if (dens < 0.5f) {
				const int clumps = 4;
				for (int g = 0; g < clumps; ++g) {
					const float gx2 = wx + rng.range(-2.6f, 2.6f), gz2 = wz + rng.range(-2.6f, 2.6f);
					if (gx2 < ox || gx2 >= ox + CHUNK || gz2 < oz || gz2 >= oz + CHUNK || plan_.road_distance(gx2, gz2) < WorldPlan::ROAD_HALF + 1.2f) {
						continue;
					}
					const float gs = rng.range(0.8f, 1.4f);
					grass.push_back(Transform3D(Basis(Vector3(0, 1, 0), rng.range(0.0f, Math_TAU)).scaled(Vector3(gs, gs * rng.range(0.8f, 1.3f), gs)),
							Vector3(gx2, ground_at(gx2, gz2), gz2)));
					grass_col.push_back(Color(0.16f, 0.32f, 0.1f) * rng.range(0.85f, 1.15f));
				}
			}
		}
	}
	for (int kind = 0; kind < ForestKit::COUNT + 3; ++kind) {
		if (inst[kind].empty()) {
			continue;
		}
		Ref<MultiMesh> mm;
		mm.instantiate();
		mm->set_transform_format(MultiMesh::TRANSFORM_3D);
		mm->set_mesh(kind >= ForestKit::COUNT ? maples_[kind - ForestKit::COUNT] : Ref<Mesh>(kit_.mesh[kind]));
		mm->set_instance_count(int(inst[kind].size()));
		for (int i = 0; i < int(inst[kind].size()); ++i) {
			mm->set_instance_transform(i, inst[kind][i]);
		}
		MultiMeshInstance3D *mmi = memnew(MultiMeshInstance3D);
		mmi->set_multimesh(mm);
		mmi->set_material_override(toon_);
		const bool small = kind == ForestKit::SHROOMS || kind == ForestKit::ROCK_A || kind == ForestKit::STUMP || kind == ForestKit::BUSH;
		mmi->set_cast_shadows_setting(small ? GeometryInstance3D::SHADOW_CASTING_SETTING_OFF : GeometryInstance3D::SHADOW_CASTING_SETTING_ON);
		mmi->set_visibility_range_end(small ? 90.0f : 190.0f);
		if (kind >= ForestKit::COUNT || kind <= ForestKit::SPOOKY_B) {
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
	gen->generate_settlement(int64_t(r.seed & 0x7FFFFFFFFFFFull), r.kind == RegionKind::Town ? SettlementKind::Town : SettlementKind::Neighborhood, r.lane_open);
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
			gen->light_up_house(l);
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
