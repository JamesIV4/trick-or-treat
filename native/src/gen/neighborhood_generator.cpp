#include "gen/neighborhood_generator.h"
#include "core/fonts.h"

#include <godot_cpp/classes/box_shape3d.hpp>
#include <godot_cpp/classes/collision_shape3d.hpp>
#include <godot_cpp/classes/cylinder_shape3d.hpp>
#include <godot_cpp/classes/engine.hpp>
#include <godot_cpp/classes/gpu_particles3d.hpp>
#include <godot_cpp/classes/gradient.hpp>
#include <godot_cpp/classes/gradient_texture1_d.hpp>
#include <godot_cpp/classes/gradient_texture2d.hpp>
#include <godot_cpp/classes/label3d.hpp>
#include <godot_cpp/classes/particle_process_material.hpp>
#include <godot_cpp/classes/quad_mesh.hpp>
#include <godot_cpp/classes/standard_material3d.hpp>
#include <godot_cpp/classes/multi_mesh.hpp>
#include <godot_cpp/classes/multi_mesh_instance3d.hpp>
#include <godot_cpp/classes/static_body3d.hpp>
#include <godot_cpp/classes/world_boundary_shape3d.hpp>
#include <godot_cpp/core/class_db.hpp>
#include <godot_cpp/core/math.hpp>
#include <godot_cpp/variant/utility_functions.hpp>

#include <algorithm>
#include <chrono>
#include <cmath>
#include <map>

#include "gen/candy_kinds.h"
#include "play/candy_field.h"

using namespace godot;

namespace tot {

void set_instance_param_recursive(Node *node, const StringName &name, const Variant &value) {
	if (GeometryInstance3D *g = Object::cast_to<GeometryInstance3D>(node)) {
		g->set_instance_shader_parameter(name, value);
	}
	for (int i = 0; i < node->get_child_count(); ++i) {
		set_instance_param_recursive(node->get_child(i), name, value);
	}
}

static void collect_geometry(Node *node, std::vector<GeometryInstance3D *> &out) {
	if (GeometryInstance3D *g = Object::cast_to<GeometryInstance3D>(node)) {
		out.push_back(g);
	}
	for (int i = 0; i < node->get_child_count(); ++i) {
		collect_geometry(node->get_child(i), out);
	}
}

void NeighborhoodGenerator::_bind_methods() {
	ClassDB::bind_method(D_METHOD("generate", "seed"), &NeighborhoodGenerator::generate);
	ClassDB::bind_method(D_METHOD("set_toon_material", "material"), &NeighborhoodGenerator::set_toon_material);
	ClassDB::bind_method(D_METHOD("get_toon_material"), &NeighborhoodGenerator::get_toon_material);
	ClassDB::bind_method(D_METHOD("set_candy_material", "material"), &NeighborhoodGenerator::set_candy_material);
	ClassDB::bind_method(D_METHOD("get_candy_material"), &NeighborhoodGenerator::get_candy_material);
	ClassDB::bind_method(D_METHOD("set_halo_material", "material"), &NeighborhoodGenerator::set_halo_material);
	ClassDB::bind_method(D_METHOD("get_halo_material"), &NeighborhoodGenerator::get_halo_material);
	ClassDB::bind_method(D_METHOD("set_catalog_path", "path"), &NeighborhoodGenerator::set_catalog_path);
	ClassDB::bind_method(D_METHOD("get_catalog_path"), &NeighborhoodGenerator::get_catalog_path);
	ClassDB::bind_method(D_METHOD("get_spawn_position"), &NeighborhoodGenerator::get_spawn_position);
	ClassDB::bind_method(D_METHOD("get_spawn_yaw"), &NeighborhoodGenerator::get_spawn_yaw);
	ClassDB::bind_method(D_METHOD("get_moon_direction"), &NeighborhoodGenerator::get_moon_direction);
	ClassDB::bind_method(D_METHOD("get_center"), &NeighborhoodGenerator::get_center);
	ClassDB::bind_method(D_METHOD("street_yaw_at", "position"), &NeighborhoodGenerator::street_yaw_at);
	ClassDB::bind_method(D_METHOD("get_doorbells"), &NeighborhoodGenerator::get_doorbells);
	ClassDB::bind_method(D_METHOD("get_lot_count"), &NeighborhoodGenerator::get_lot_count);
	ClassDB::bind_method(D_METHOD("describe_lot", "lot"), &NeighborhoodGenerator::describe_lot);
	ClassDB::bind_method(D_METHOD("open_door", "lot"), &NeighborhoodGenerator::open_door);
	ClassDB::bind_method(D_METHOD("light_up_house", "lot"), &NeighborhoodGenerator::light_up_house);
	ClassDB::bind_method(D_METHOD("get_candy_field"), &NeighborhoodGenerator::get_candy_field);
	ClassDB::bind_method(D_METHOD("instantiate_asset", "id"), &NeighborhoodGenerator::instantiate_asset);
	ADD_PROPERTY(PropertyInfo(Variant::OBJECT, "toon_material", PROPERTY_HINT_RESOURCE_TYPE, "Material"), "set_toon_material", "get_toon_material");
	ADD_PROPERTY(PropertyInfo(Variant::OBJECT, "candy_material", PROPERTY_HINT_RESOURCE_TYPE, "Material"), "set_candy_material", "get_candy_material");
	ADD_PROPERTY(PropertyInfo(Variant::OBJECT, "halo_material", PROPERTY_HINT_RESOURCE_TYPE, "Material"), "set_halo_material", "get_halo_material");
	ClassDB::bind_method(D_METHOD("set_magic_material", "material"), &NeighborhoodGenerator::set_magic_material);
	ClassDB::bind_method(D_METHOD("get_magic_material"), &NeighborhoodGenerator::get_magic_material);
	ClassDB::bind_method(D_METHOD("visit_house", "lot"), &NeighborhoodGenerator::visit_house);
	ADD_PROPERTY(PropertyInfo(Variant::OBJECT, "magic_material", PROPERTY_HINT_RESOURCE_TYPE, "Material"), "set_magic_material", "get_magic_material");
	ClassDB::bind_method(D_METHOD("set_grass_material", "material"), &NeighborhoodGenerator::set_grass_material);
	ClassDB::bind_method(D_METHOD("get_grass_material"), &NeighborhoodGenerator::get_grass_material);
	ADD_PROPERTY(PropertyInfo(Variant::OBJECT, "grass_material", PROPERTY_HINT_RESOURCE_TYPE, "Material"), "set_grass_material", "get_grass_material");
	ADD_PROPERTY(PropertyInfo(Variant::STRING, "catalog_path", PROPERTY_HINT_FILE, "*.json"), "set_catalog_path", "get_catalog_path");
	ADD_SIGNAL(MethodInfo("generated", PropertyInfo(Variant::DICTIONARY, "stats")));
}

Dictionary NeighborhoodGenerator::generate(int64_t seed) {
	return run(seed, SettlementKind::Neighborhood, false, false);
}

Dictionary NeighborhoodGenerator::generate_settlement(int64_t seed, SettlementKind kind, bool lane_open) {
	return run(seed, kind, true, lane_open);
}

Dictionary NeighborhoodGenerator::run(int64_t seed, SettlementKind kind, bool embedded, bool lane_open) {
	const auto t0 = std::chrono::steady_clock::now();
	if (!assets_loaded_) {
		assets_->load(catalog_path_, toon_);
		assets_loaded_ = true;
	}
	ctx_ = std::make_unique<GenContext>();
	GenContext &ctx = *ctx_;
	ctx.seed = uint64_t(seed);
	ctx.assets = assets_;
	ctx.kind = kind;
	ctx.embedded = embedded;
	ctx.lane_open = lane_open;
	const bool town = kind == SettlementKind::Town;

	plan_street(ctx);
	if (town) {
		plan_town_lots(ctx);
		assign_shops(ctx);
		for (Lot &lot : ctx.lots) {
			plan_shop(ctx, lot);
		}
		for (Lot &lot : ctx.lots) {
			build_shop(ctx, lot);
			plan_shop_routes(ctx, lot);
		}
	} else {
		plan_lots(ctx);
		assign_households(ctx);
		for (Lot &lot : ctx.lots) {
			plan_house(ctx, lot);
		}
		for (Lot &lot : ctx.lots) {
			build_house(ctx, lot);
			plan_routes(ctx, lot);
		}
	}
	build_street(ctx);
	build_boundary(ctx);
	for (Lot &lot : ctx.lots) {
		if (town) {
			dress_shopfront(ctx, lot);
		} else {
			dress_yard(ctx, lot);
		}
	}
	place_street_rhythm(ctx);
	if (town) {
		place_town_square(ctx);
	} else {
		place_pocket_parks(ctx);
	}
	register_prop_collisions(ctx);
	weave_candy(ctx);
	validate_traversal(ctx);
	plan_magic_paths(ctx);
	scatter_dressing(ctx);
	ctx.stats.props = int(ctx.props.size());
	const auto t1 = std::chrono::steady_clock::now();
	build_nodes();
	const auto t2 = std::chrono::steady_clock::now();

	Dictionary stats;
	stats["seed"] = seed;
	stats["lots"] = int(ctx.lots.size());
	stats["props"] = ctx.stats.props;
	stats["candy"] = ctx.stats.candy;
	stats["candy_raised"] = ctx.stats.candy_raised;
	stats["candy_dropped"] = ctx.stats.candy_dropped;
	stats["platforms"] = ctx.stats.platforms;
	stats["reachable_platforms"] = ctx.stats.reachable_platforms;
	stats["lights"] = ctx.stats.lights;
	stats["openness_failures"] = ctx.stats.openness_failures;
	stats["assets_published"] = assets_->published_count();
	stats["plan_ms"] = double(std::chrono::duration_cast<std::chrono::microseconds>(t1 - t0).count()) / 1000.0;
	stats["build_ms"] = double(std::chrono::duration_cast<std::chrono::microseconds>(t2 - t1).count()) / 1000.0;
	PackedStringArray notes;
	for (const std::string &n : ctx.stats.notes) {
		notes.push_back(String(n.c_str()));
	}
	stats["notes"] = notes;
	Array households;
	for (const Lot &lot : ctx.lots) {
		households.push_back(describe_lot(lot.index));
	}
	stats["households"] = households;
	emit_signal("generated", stats);
	return stats;
}

String NeighborhoodGenerator::describe_lot(int lot) const {
	if (!ctx_ || lot < 0 || lot >= int(ctx_->lots.size())) {
		return String();
	}
	const Lot &l = ctx_->lots[lot];
	String s = String::num_int64(lot) + ": " + persona_name(l.persona);
	if (l.landmark != Landmark::None) {
		s += String(" - ") + landmark_name(l.landmark);
	}
	s += String(" (") + String::num_int64(l.house.storeys) + (l.house.garage ? " storey, garage)" : " storey)");
	return s;
}

float NeighborhoodGenerator::street_yaw_at(const Vector3 &p) const {
	if (!ctx_) {
		return 0.0f;
	}
	const StreetPlan &s = ctx_->street;
	Vector3 t = (s.at(p.x + 1.0f) - s.at(p.x - 1.0f)).normalized();
	// Inside the side lane, follow the lane instead.
	if (std::abs(p.x - s.lane_x) < s.edge() && (p.z - s.lane_origin.z) * s.lane_dir > s.edge()) {
		t = Vector3(0, 0, float(s.lane_dir));
	}
	return std::atan2(-t.x, -t.z);
}

Vector3 NeighborhoodGenerator::get_center() const {
	if (!ctx_) {
		return Vector3();
	}
	return Vector3((ctx_->bound_x0 + ctx_->bound_x1) * 0.5f, 0, (ctx_->bound_z0 + ctx_->bound_z1) * 0.5f);
}

PackedVector3Array NeighborhoodGenerator::get_doorbells() const {
	PackedVector3Array out;
	if (ctx_) {
		for (const Lot &l : ctx_->lots) {
			out.push_back(l.doorbell);
		}
	}
	return out;
}

Node *NeighborhoodGenerator::get_candy_field() const {
	return candy_;
}

Node3D *NeighborhoodGenerator::instantiate_asset(const String &id) {
	if (!assets_loaded_) {
		assets_->load(catalog_path_, toon_);
		assets_loaded_ = true;
	}
	return assets_->instantiate(std::string(id.utf8().get_data()));
}

bool NeighborhoodGenerator::asset_published(const String &id) {
	if (!assets_loaded_) {
		assets_->load(catalog_path_, toon_);
		assets_loaded_ = true;
	}
	return assets_->get(std::string(id.utf8().get_data()))->published;
}

void NeighborhoodGenerator::open_door(int lot) {
	if (lot >= 0 && lot < int(doors_.size())) {
		doors_[lot].target = -1.9f;
	}
	visit_house(lot);
}

void NeighborhoodGenerator::light_up_house(int lot) {
	if (lot >= 0 && lot < int(glow_.size())) {
		glow_[lot].lit = true;
		glow_[lot].pulse = 1.0f;
	}
}

void NeighborhoodGenerator::_process(double delta) {
	const float dt = float(delta);
	for (DoorNode &d : doors_) {
		if (d.hinge && std::abs(d.angle - d.target) > 0.001f) {
			d.angle = Math::lerp(d.angle, d.target, 1.0f - std::exp(-dt * 5.0f));
			d.hinge->set_rotation(Vector3(0, d.angle, 0));
		}
	}
	for (Squishy &s : squishies_) {
		if (s.t <= 0.0f || !s.node) {
			continue;
		}
		if (s.mode == SQUISH_HOOD) {
			// The hood flaps up on its hinge, hangs a moment, then drops shut with a little rattle.
			s.t = Math::max(0.0f, s.t - dt * 1.4f);
			const float u = 1.0f - s.t;
			float lift = u < 0.18f ? Math::smoothstep(0.0f, 0.18f, u) : 1.0f - Math::smoothstep(0.45f, 0.85f, u);
			if (u > 0.85f) {
				lift = std::abs(std::sin((u - 0.85f) / 0.15f * Math_PI * 2.0f)) * 0.06f * (1.0f - u) / 0.15f;
			}
			s.node->set_transform(Transform3D(s.rest.basis * Basis(Vector3(1, 0, 0), -1.05f * lift), s.rest.origin));
			continue;
		}
		s.t = Math::max(0.0f, s.t - dt * 2.2f);
		const float k = std::sin((1.0f - s.t) * Math_PI * 4.0f) * s.t * s.t * 0.35f;
		if (s.mode == SQUISH_DIP) {
			// The awning sags under the landing and springs back about its wall hinge.
			s.node->set_transform(Transform3D(s.rest.basis * Basis(Vector3(1, 0, 0), k * 0.5f), s.rest.origin));
			continue;
		}
		// Damped wobble: squash down, spring up, settle.
		s.node->set_transform(Transform3D(s.rest.basis.scaled(Vector3(1.0f + k * 0.6f, 1.0f - k, 1.0f + k * 0.6f)), s.rest.origin));
	}
	// The street spine: a piece fades once every house near it has been visited; pieces with no
	// house near them go once every house in the area has.
	if (!spine_.empty()) {
		bool all = true;
		for (const MagicNode &mn : magic_nodes_) {
			all = all && mn.visited;
		}
		for (SpineChunk &c : spine_) {
			if (c.gone || !c.node) {
				continue;
			}
			bool done = !c.lots.empty() || all;
			for (int l : c.lots) {
				done = done && l < int(magic_nodes_.size()) && magic_nodes_[l].visited;
			}
			if (all) {
				done = true;
			}
			if (done) {
				c.fade = Math::max(0.0f, c.fade - dt * 0.35f);
				c.node->set_instance_shader_parameter("fade", c.fade);
				if (c.fade <= 0.0f) {
					c.node->set_visible(false);
					c.gone = true;
				}
			}
		}
	}
	// Visited houses: their guide branch fades out so the trail always points somewhere new.
	for (MagicNode &mn : magic_nodes_) {
		if (mn.node && mn.visited && mn.fade > 0.0f) {
			mn.fade = Math::max(0.0f, mn.fade - dt * 0.5f);
			mn.node->set_instance_shader_parameter("fade", mn.fade);
			if (mn.fade <= 0.0f) {
				mn.node->set_visible(false);
			}
		}
	}
	for (LotGlow &g : glow_) {
		if (!g.lit || g.pulse <= 0.0f) {
			continue;
		}
		// Celebration pulse that settles to "lanterns lit".
		g.pulse = Math::max(0.0f, g.pulse - dt * 0.6f);
		const float boost = 1.7f + g.pulse * 2.5f;
		for (GeometryInstance3D *gi : g.lanterns) {
			gi->set_instance_shader_parameter("glow_boost", boost);
		}
		if (g.porch) {
			g.porch->set_param(Light3D::PARAM_ENERGY, 1.7f + g.pulse * 2.5f);
		}
	}
}

// ---------------------------------------------------------------- scene construction
void NeighborhoodGenerator::build_nodes() {
	if (world_) {
		remove_child(world_);
		world_->queue_free();
		world_ = nullptr;
		candy_ = nullptr;
	}
	doors_.clear();
	glow_.clear();
	squishies_.clear();
	world_ = memnew(Node3D);
	world_->set_name("World");
	add_child(world_);
	GenContext &ctx = *ctx_;

	// Ground: an endless lawn plane with a world boundary for collision (the open world's terrain
	// provides the ground for embedded settlements).
	if (!ctx.embedded) {
		MeshBuilder g;
		const float r = 260.0f;
		g.quad_out(Vector3(-r, 0, -r), Vector3(r, 0, -r), Vector3(r, 0, r), Vector3(-r, 0, r), mat_cell(pal::LAWN_DARK, mat::GRASS), Vector3(0, -1, 0));
		MeshInstance3D *mi = memnew(MeshInstance3D);
		mi->set_name("Ground");
		mi->set_mesh(g.build());
		mi->set_material_override(toon_);
		world_->add_child(mi);
		StaticBody3D *body = memnew(StaticBody3D);
		body->set_name("GroundBody");
		CollisionShape3D *cs = memnew(CollisionShape3D);
		Ref<WorldBoundaryShape3D> plane;
		plane.instantiate();
		cs->set_shape(plane);
		body->add_child(cs);
		world_->add_child(body);
	}
	auto add_mesh = [&](MeshBuilder &b, const String &name) -> MeshInstance3D * {
		if (b.empty()) {
			return nullptr;
		}
		MeshInstance3D *mi = memnew(MeshInstance3D);
		mi->set_name(name);
		mi->set_mesh(b.build());
		mi->set_material_override(toon_);
		world_->add_child(mi);
		return mi;
	};
	add_mesh(ctx.street_mesh, "Street");
	add_mesh(ctx.boundary_mesh, "Boundary");
	glow_.resize(ctx.lots.size());
	// House, fence and landmark geometry is authored in lot-local space.
	for (size_t i = 0; i < ctx.lots.size(); ++i) {
		if (MeshInstance3D *mi = add_mesh(ctx.lot_mesh[i], String("Lot") + String::num_int64(int64_t(i)))) {
			mi->set_transform(ctx.lots[i].xf);
		}
	}
	// Springy building parts (shop awnings): their own nodes so they can dip when bounced on.
	for (SpringSpec &sp : ctx.springs) {
		if (MeshInstance3D *mi = add_mesh(sp.mesh, "Awning")) {
			mi->set_transform(sp.xf);
			Squishy s;
			s.node = mi;
			s.rest = sp.xf;
			s.mode = SQUISH_DIP;
			s.anchor = sp.anchor;
			s.reach = sp.reach;
			squishies_.push_back(s);
		}
	}
	build_collision(world_);
	build_props(world_);
	build_doors(world_);
	build_lights(world_);
	build_strings(world_);
	build_scatter(world_);
	build_candy(world_);
	build_fireflies(world_);
	build_magic(world_);
	build_signs(world_);
	set_process(true);
}

// Shop signs: chunky outlined lettering on the sign boards.
void NeighborhoodGenerator::build_signs(Node3D *world) {
	for (const SignSpec &s : ctx_->signs) {
		Label3D *l = memnew(Label3D);
		l->set_font(display_font());
		l->set_text(String(s.text.c_str()));
		l->set_font_size(96);
		l->set_outline_size(22);
		l->set_pixel_size(0.0042f * s.size);
		l->set_modulate(s.color);
		l->set_outline_modulate(Color(0.12f, 0.05f, 0.08f));
		l->set_transform(s.xf);
		l->set_draw_flag(Label3D::FLAG_DOUBLE_SIDED, false);
		l->set_draw_flag(Label3D::FLAG_SHADED, false);
		world->add_child(l);
	}
}

// Magic guide trail meshes: the shader spirals three strands around these centrelines.
void NeighborhoodGenerator::build_magic(Node3D *world) {
	magic_nodes_.assign(ctx_->lots.size(), MagicNode());
	spine_.clear();
	if (magic_mat_.is_null()) {
		return;
	}
	for (const MagicPath &path : ctx_->magic) {
		if (path.points.size() < 2) {
			continue;
		}
		// Resample a Catmull-Rom spline every 0.25 m.
		std::vector<Vector3> pts;
		const auto &p = path.points;
		for (size_t i = 0; i + 1 < p.size(); ++i) {
			const Vector3 &a = p[i == 0 ? 0 : i - 1], &b = p[i], &c = p[i + 1], &d = p[std::min(i + 2, p.size() - 1)];
			const int steps = std::max(1, int(b.distance_to(c) / 0.25f));
			for (int k = 0; k < steps; ++k) {
				const float t = float(k) / steps, t2 = t * t, t3 = t2 * t;
				pts.push_back(((b * 2.0f) + (-a + c) * t + (a * 2.0f - b * 5.0f + c * 4.0f - d) * t2 + (-a + b * 3.0f - c * 3.0f + d) * t3) * 0.5f);
			}
		}
		pts.push_back(p.back());
		std::vector<float> dist(pts.size(), 0.0f);
		for (size_t i = 1; i < pts.size(); ++i) {
			dist[i] = dist[i - 1] + pts[i].distance_to(pts[i - 1]);
		}
		const float total = Math::max(dist.back(), 0.01f);
		// Builds the trail mesh over pts[i0..i1] (distances stay global, so pieces join seamlessly).
		auto piece = [&](size_t i0, size_t i1) -> MeshInstance3D * {
		PackedVector3Array v, n;
		PackedFloat32Array tan;
		PackedVector2Array uv, uv2;
		for (int strand = 0; strand < 3; ++strand) {
			for (size_t i = i0; i + 1 <= i1 && i + 1 < pts.size(); ++i) {
				for (int corner = 0; corner < 6; ++corner) {
					static const int idx[6] = { 0, 1, 0, 0, 1, 1 };
					static const float sd[6] = { 0, 0, 1, 1, 0, 1 };
					const size_t k = i + idx[corner];
					const Vector3 t = (pts[std::min(k + 1, pts.size() - 1)] - pts[k > 0 ? k - 1 : 0]).normalized();
					Vector3 nn = Vector3(0, 1, 0) - t * t.y;
					nn = nn.length_squared() > 1e-4f ? nn.normalized() : Vector3(1, 0, 0);
					v.push_back(pts[k]);
					n.push_back(nn);
					tan.append_array(PackedFloat32Array({ t.x, t.y, t.z, 1.0f }));
					uv.push_back(Vector2(dist[k], sd[corner]));
					uv2.push_back(Vector2(float(strand), dist[k] / total));
				}
			}
		}
		Array arrays;
		arrays.resize(Mesh::ARRAY_MAX);
		arrays[Mesh::ARRAY_VERTEX] = v;
		arrays[Mesh::ARRAY_NORMAL] = n;
		arrays[Mesh::ARRAY_TANGENT] = tan;
		arrays[Mesh::ARRAY_TEX_UV] = uv;
		arrays[Mesh::ARRAY_TEX_UV2] = uv2;
		Ref<ArrayMesh> mesh;
		mesh.instantiate();
		mesh->add_surface_from_arrays(Mesh::PRIMITIVE_TRIANGLES, arrays);
		MeshInstance3D *mi = memnew(MeshInstance3D);
		mi->set_name(path.lot >= 0 ? String("MagicTrail") + String::num_int64(path.lot) : String("MagicSpine"));
		mi->set_mesh(mesh);
		mi->set_material_override(magic_mat_);
		mi->set_cast_shadows_setting(GeometryInstance3D::SHADOW_CASTING_SETTING_OFF);
		mi->set_extra_cull_margin(1.0f);
		mi->set_instance_shader_parameter("fade", path.lot >= 0 ? 1.0f : 0.7f);
		world->add_child(mi);
		return mi;
		};
		if (path.lot >= 0) {
			MeshInstance3D *mi = piece(0, pts.size() - 1);
			if (path.lot < int(magic_nodes_.size())) {
				magic_nodes_[path.lot].node = mi;
			}
			continue;
		}
		// The spine: about 10 m per piece, each tagged with the houses near it.
		for (size_t i0 = 0; i0 + 1 < pts.size();) {
			size_t i1 = i0 + 1;
			while (i1 + 1 < pts.size() && dist[i1] - dist[i0] < 10.0f) {
				++i1;
			}
			SpineChunk c;
			c.node = piece(i0, i1);
			const Vector3 mid = pts[(i0 + i1) / 2];
			for (size_t l = 0; l < ctx_->lots.size(); ++l) {
				const Vector3 d = ctx_->lots[l].doorbell - mid;
				if (Vector2(d.x, d.z).length() < 16.0f) {
					c.lots.push_back(int(l));
				}
			}
			spine_.push_back(c);
			i0 = i1;
		}
	}
}

void NeighborhoodGenerator::squash_near(const Vector3 &pos) {
	Squishy *best = nullptr;
	float best_d = 1e9f;
	for (Squishy &s : squishies_) {
		const float d = Vector2(s.anchor.x - pos.x, s.anchor.z - pos.z).length();
		if (d < s.reach && d < best_d) {
			best_d = d;
			best = &s;
		}
	}
	if (best) {
		best->t = 1.0f;
	}
}

void NeighborhoodGenerator::visit_house(int lot) {
	if (lot >= 0 && lot < int(magic_nodes_.size())) {
		magic_nodes_[lot].visited = true;
	}
}

void NeighborhoodGenerator::build_fireflies(Node3D *world) {
	const Vector3 center((ctx_->bound_x0 + ctx_->bound_x1) * 0.5f, 1.5f, (ctx_->bound_z0 + ctx_->bound_z1) * 0.5f);
	const Vector3 ext((ctx_->bound_x1 - ctx_->bound_x0) * 0.5f, 1.2f, (ctx_->bound_z1 - ctx_->bound_z0) * 0.5f);
	GPUParticles3D *p = memnew(GPUParticles3D);
	p->set_name("Fireflies");
	p->set_amount(260);
	p->set_lifetime(9.0);
	p->set_pre_process_time(9.0);
	p->set_position(center);
	p->set_visibility_aabb(AABB(-ext - Vector3(2, 2, 2), (ext + Vector3(2, 2, 2)) * 2.0f));
	p->set_cast_shadows_setting(GeometryInstance3D::SHADOW_CASTING_SETTING_OFF);
	Ref<ParticleProcessMaterial> pm;
	pm.instantiate();
	pm->set_emission_shape(ParticleProcessMaterial::EMISSION_SHAPE_BOX);
	pm->set_emission_box_extents(ext);
	pm->set_gravity(Vector3(0, 0, 0));
	pm->set_direction(Vector3(0, 1, 0));
	pm->set_spread(180.0f);
	pm->set_param_min(ParticleProcessMaterial::PARAM_INITIAL_LINEAR_VELOCITY, 0.05f);
	pm->set_param_max(ParticleProcessMaterial::PARAM_INITIAL_LINEAR_VELOCITY, 0.35f);
	pm->set_turbulence_enabled(true);
	pm->set_turbulence_noise_strength(1.2f);
	pm->set_turbulence_noise_scale(3.5f);
	pm->set_turbulence_noise_speed_random(0.4f);
	pm->set_param_min(ParticleProcessMaterial::PARAM_TURB_INFLUENCE_OVER_LIFE, 0.04f);
	pm->set_param_max(ParticleProcessMaterial::PARAM_TURB_INFLUENCE_OVER_LIFE, 0.1f);
	pm->set_param_min(ParticleProcessMaterial::PARAM_SCALE, 0.6f);
	pm->set_param_max(ParticleProcessMaterial::PARAM_SCALE, 1.3f);
	Ref<Gradient> fade;
	fade.instantiate();
	fade->set_offsets(PackedFloat32Array({ 0.0f, 0.2f, 0.35f, 0.5f, 0.65f, 0.8f, 1.0f }));
	PackedColorArray cols;
	for (float a : { 0.0f, 1.0f, 0.3f, 1.0f, 0.25f, 1.0f, 0.0f }) {
		cols.push_back(Color(1, 1, 1, a));
	}
	fade->set_colors(cols);
	Ref<GradientTexture1D> ramp;
	ramp.instantiate();
	ramp->set_gradient(fade);
	pm->set_color_ramp(ramp);
	p->set_process_material(pm);
	Ref<StandardMaterial3D> mat;
	mat.instantiate();
	mat->set_shading_mode(BaseMaterial3D::SHADING_MODE_UNSHADED);
	mat->set_billboard_mode(BaseMaterial3D::BILLBOARD_PARTICLES);
	mat->set_transparency(BaseMaterial3D::TRANSPARENCY_ALPHA);
	mat->set_blend_mode(BaseMaterial3D::BLEND_MODE_ADD);
	mat->set_flag(BaseMaterial3D::FLAG_ALBEDO_FROM_VERTEX_COLOR, true);
	mat->set_albedo(Color(1.6f, 1.9f, 0.7f));
	mat->set_flag(BaseMaterial3D::FLAG_DISABLE_FOG, true);
	// Fade out right in front of the lens so they never become big blobs.
	mat->set_distance_fade(BaseMaterial3D::DISTANCE_FADE_PIXEL_ALPHA);
	mat->set_distance_fade_min_distance(1.5f);
	mat->set_distance_fade_max_distance(5.0f);
	// Soft round glow instead of a square quad.
	Ref<Gradient> dot;
	dot.instantiate();
	dot->set_offsets(PackedFloat32Array({ 0.0f, 0.25f, 1.0f }));
	dot->set_colors(PackedColorArray({ Color(1, 1, 1, 1), Color(1, 1, 1, 0.55f), Color(1, 1, 1, 0) }));
	Ref<GradientTexture2D> glow;
	glow.instantiate();
	glow->set_gradient(dot);
	glow->set_fill(GradientTexture2D::FILL_RADIAL);
	glow->set_fill_from(Vector2(0.5f, 0.5f));
	glow->set_fill_to(Vector2(0.5f, 0.0f));
	glow->set_width(32);
	glow->set_height(32);
	mat->set_texture(BaseMaterial3D::TEXTURE_ALBEDO, glow);
	Ref<QuadMesh> quad;
	quad.instantiate();
	quad->set_size(Vector2(0.2f, 0.2f));
	quad->set_material(mat);
	p->set_draw_pass_mesh(0, quad);
	world->add_child(p);
}

void NeighborhoodGenerator::build_collision(Node3D *world) {
	// Layers: 1 = solid world, 2 = playground boundary, 3 (value 4) = big structures the camera
	// pulls in front of (house bodies, roofs, garages).
	StaticBody3D *solids = memnew(StaticBody3D);
	solids->set_name("Solids");
	StaticBody3D *structures = memnew(StaticBody3D);
	structures->set_name("Structures");
	structures->set_collision_layer(1 | 4);
	StaticBody3D *boundary = memnew(StaticBody3D);
	boundary->set_name("Boundary");
	boundary->set_collision_layer(2);
	// Yard fences: solid like the rest of the world, but tagged so fireballs pass straight through.
	StaticBody3D *fences = memnew(StaticBody3D);
	fences->set_name("Fences");
	fences->set_meta("shot_through", true);
	// One body per launch speed (inflatables, mini and big trampolines).
	std::map<int, StaticBody3D *> bouncy;
	auto bouncy_body = [&](float speed) {
		StaticBody3D *&b = bouncy[int(std::round(speed * 10.0f))];
		if (!b) {
			b = memnew(StaticBody3D);
			b->set_name(String("Bouncy") + String::num(speed, 1));
			b->set_meta("bounce", speed);
		}
		return b;
	};
	for (const OBox &b : ctx_->boxes) {
		CollisionShape3D *cs = memnew(CollisionShape3D);
		if (b.cylinder) {
			Ref<CylinderShape3D> c;
			c.instantiate();
			c->set_radius(b.half.x);
			c->set_height(b.half.y * 2.0f);
			cs->set_shape(c);
		} else {
			Ref<BoxShape3D> s;
			s.instantiate();
			s->set_size(b.half * 2.0f);
			cs->set_shape(s);
		}
		cs->set_transform(b.xf);
		const float volume = b.half.x * b.half.y * b.half.z * 8.0f;
		StaticBody3D *body = b.bouncy ? bouncy_body(b.bounce) : b.boundary ? boundary : b.shot_through ? fences : (volume > 8.0f && b.lot >= 0 && !b.cylinder) ? structures : solids;
		body->add_child(cs);
	}
	world->add_child(solids);
	world->add_child(structures);
	world->add_child(boundary);
	world->add_child(fences);
	for (auto &kv : bouncy) {
		world->add_child(kv.second);
	}
}

void NeighborhoodGenerator::build_props(Node3D *world) {
	Node3D *root = memnew(Node3D);
	root->set_name("Props");
	world->add_child(root);
	for (const PropPlacement &pp : ctx_->props) {
		Node3D *n = assets_->instantiate(pp.id);
		n->set_transform(pp.xf);
		root->add_child(n);
		if (pp.wind > 0.0f) {
			set_instance_param_recursive(n, "wind", pp.wind);
		}
		const AssetInfo *info = assets_->get(pp.id);
		if (info->has_hood && pp.collide) {
			// Cars: the hood is a trampoline that flaps up on its hinge.
			if (Node3D *hood = Object::cast_to<Node3D>(n->find_child("hood", true, false))) {
				Squishy s;
				s.node = hood;
				s.rest = hood->get_transform();
				s.mode = SQUISH_HOOD;
				s.anchor = pp.xf.xform(info->hood.get_center());
				s.reach = Math::max(info->hood.size.x, info->hood.size.z) * 0.5f * pp.xf.basis.get_scale().x + 0.5f;
				squishies_.push_back(s);
			}
		} else if (pp.bouncy) {
			Squishy s;
			s.node = n;
			s.rest = pp.xf;
			s.anchor = pp.xf.origin;
			squishies_.push_back(s);
		}
		if (pp.id == "porch_lantern" && pp.lot >= 0) {
			// Unlit until the house is cleared: the porch lanterns are diegetic progress.
			set_instance_param_recursive(n, "glow_boost", 0.18f);
			collect_geometry(n, glow_[pp.lot].lanterns);
		}
		const AssetInfo *a = assets_->get(pp.id);
		auto it = a->sockets.find("light");
		if (it != a->sockets.end()) {
			LightSpec ls;
			ls.pos = pp.xf.xform(it->second.origin);
			ls.lot = pp.lot;
			if (pp.id == "lamp_post") {
				ls.kind = LIGHT_LAMP;
				ls.color = Color(1.0f, 0.78f, 0.5f);
				ls.energy = 1.5f;
				ls.range = 10.0f;
			} else if (pp.id.rfind("jackolantern", 0) == 0) {
				ls.kind = LIGHT_JACK;
				ls.color = Color(1.0f, 0.5f, 0.15f);
				ls.energy = 1.1f;
				ls.range = 2.6f;
			} else {
				continue;
			}
			ctx_->lights.push_back(ls);
		}
	}
}

void NeighborhoodGenerator::build_doors(Node3D *world) {
	doors_.assign(ctx_->lots.size(), DoorNode());
	for (const DoorSpec &d : ctx_->doors) {
		MeshBuilder b;
		const float w = d.width, h = d.height;
		b.set_uv_axes(Vector3(0, -1, 0), Vector3(1, 0, 0));
		b.rounded_box(Vector3(w * 0.5f, h * 0.5f, 0.0f), Vector3(w * 0.5f, h * 0.5f, 0.05f), 0.03f, d.cell);
		b.clear_uv_axes();
		b.box(Vector3(w * 0.5f, h * 0.72f, 0.055f), Vector3(w * 0.22f, h * 0.12f, 0.01f), pal::GLOW_WINDOW);
		b.box(Vector3(w * 0.5f, h * 0.3f, 0.055f), Vector3(w * 0.3f, h * 0.15f, 0.012f), d.trim_cell);
		b.blob(Vector3(w * 0.86f, h * 0.48f, 0.09f), Vector3(0.05f, 0.05f, 0.05f), pal::FOIL_GOLD, 0);
		Node3D *hinge = memnew(Node3D);
		hinge->set_transform(d.hinge);
		Node3D *pivot = memnew(Node3D);
		hinge->add_child(pivot);
		MeshInstance3D *mi = memnew(MeshInstance3D);
		mi->set_mesh(b.build());
		mi->set_material_override(toon_);
		pivot->add_child(mi);
		if (d.lot >= 0 && ctx_->kind == SettlementKind::Neighborhood) {
			// An autumn wreath on every front door (it swings open with the door).
			Node3D *wreath = assets_->instantiate("door_wreath");
			wreath->set_position(Vector3(w * 0.5f, h * 0.68f, 0.06f));
			pivot->add_child(wreath);
		}
		world->add_child(hinge);
		if (d.lot >= 0) {
			doors_[d.lot].hinge = pivot;
		}
	}
}

void NeighborhoodGenerator::build_lights(Node3D *world) {
	Node3D *root = memnew(Node3D);
	root->set_name("Lights");
	world->add_child(root);
	std::vector<LightSpec> lights;
	for (const Lot &lot : ctx_->lots) {
		LightSpec porch;
		porch.pos = lot.porch_light;
		porch.color = Color(1.0f, 0.58f, 0.26f);
		porch.energy = 1.7f;
		porch.range = 8.0f;
		porch.lot = lot.index;
		porch.kind = LIGHT_PORCH;
		lights.push_back(porch);
	}
	std::vector<LightSpec> rest = ctx_->lights;
	std::stable_sort(rest.begin(), rest.end(), [](const LightSpec &a, const LightSpec &b) { return int(a.kind) < int(b.kind); });
	// Budget: porches first, then street lamps, then a few jack-o'-lanterns (spread out).
	const int budget = 32;
	for (const LightSpec &l : rest) {
		if (int(lights.size()) >= budget) {
			break;
		}
		if (l.kind == LIGHT_JACK) {
			bool close = false;
			for (const LightSpec &o : lights) {
				if (o.pos.distance_to(l.pos) < 6.0f) {
					close = true;
					break;
				}
			}
			if (close) {
				continue;
			}
		}
		lights.push_back(l);
	}
	for (const LightSpec &l : lights) {
		OmniLight3D *o = memnew(OmniLight3D);
		o->set_position(l.pos);
		o->set_color(l.color);
		o->set_param(Light3D::PARAM_ENERGY, l.energy);
		o->set_param(Light3D::PARAM_RANGE, l.range);
		o->set_param(Light3D::PARAM_ATTENUATION, 1.3f);
		o->set_shadow(false);
		root->add_child(o);
		if (l.kind == LIGHT_PORCH && l.lot >= 0) {
			glow_[l.lot].porch = o;
		}
	}
	ctx_->stats.lights = int(lights.size());
}

void NeighborhoodGenerator::build_strings(Node3D *world) {
	if (ctx_->strings.empty()) {
		return;
	}
	// Round bulbs, mini jack-o'-lanterns, and bunting pennants in three alternating colours.
	static const char *ids[] = { "string_bulb", "string_jack", "bunting_flag_orange", "bunting_flag_purple", "bunting_flag_cream" };
	constexpr int KINDS = 5;
	MeshBuilder wire;
	std::vector<Transform3D> items[KINDS];
	for (const StringLight &s : ctx_->strings) {
		const float len = s.a.distance_to(s.b);
		const float spacing = s.style == STRING_BULBS ? 0.45f : (s.style == STRING_JACKS ? 0.45f : 0.33f);
		const int n = std::max(4, int(len / spacing));
		Vector3 prev = s.a;
		for (int i = 1; i <= n; ++i) {
			const float t = float(i) / n;
			Vector3 p = s.a.lerp(s.b, t) - Vector3(0, s.sag * 4.0f * t * (1.0f - t), 0);
			const Vector3 d = p - prev;
			const float l = d.length();
			if (l > 0.01f) {
				const Vector3 f = d / l;
				Vector3 up = std::abs(f.y) > 0.9f ? Vector3(1, 0, 0) : Vector3(0, 1, 0);
				const Vector3 side = f.cross(up).normalized();
				up = side.cross(f);
				wire.set_transform(Transform3D(Basis(side, up, f), (p + prev) * 0.5f));
				wire.box(Vector3(), Vector3(0.012f, 0.012f, l * 0.5f), pal::CHARCOAL);
			}
			if (i < n) {
				if (s.style == STRING_BULBS) {
					items[0].push_back(Transform3D(Basis(), p));
				} else {
					// Hang straight down, front turned away from the wire toward out.
					Vector3 x = Vector3(s.b.x - s.a.x, 0, s.b.z - s.a.z).normalized();
					Vector3 z = x.cross(Vector3(0, 1, 0));
					if (z.dot(s.out) < 0.0f) {
						x = -x;
						z = -z;
					}
					if (s.style == STRING_JACKS) {
						// Chunky bulbs, every other one hanging a little lower, each turned a touch.
						const float sway = ((i * 37) % 7 - 3) * 0.06f;
						const Basis b = Basis(x, Vector3(0, 1, 0), z).rotated(Vector3(0, 1, 0), sway).scaled(Vector3(1.6f, 1.6f, 1.6f));
						items[1].push_back(Transform3D(b, p - Vector3(0, (i & 1) ? 0.05f : 0.0f, 0)));
					} else {
						items[2 + (i % 3)].push_back(Transform3D(Basis(x, Vector3(0, 1, 0), z), p));
					}
				}
			}
			prev = p;
		}
	}
	MeshInstance3D *mi = memnew(MeshInstance3D);
	mi->set_name("StringWires");
	mi->set_mesh(wire.build());
	mi->set_material_override(toon_);
	world->add_child(mi);
	for (int k = 0; k < KINDS; ++k) {
		if (items[k].empty()) {
			continue;
		}
		Ref<MultiMesh> mm;
		mm.instantiate();
		mm->set_transform_format(MultiMesh::TRANSFORM_3D);
		mm->set_mesh(k == 0 ? assets_->mesh(ids[k]) : assets_->merged_mesh(ids[k]));
		mm->set_instance_count(int(items[k].size()));
		for (int i = 0; i < int(items[k].size()); ++i) {
			mm->set_instance_transform(i, items[k][i]);
		}
		MultiMeshInstance3D *mmi = memnew(MultiMeshInstance3D);
		mmi->set_name(String("Strung_") + ids[k]);
		mmi->set_multimesh(mm);
		mmi->set_material_override(toon_);
		mmi->set_cast_shadows_setting(GeometryInstance3D::SHADOW_CASTING_SETTING_OFF);
		world->add_child(mmi);
	}
}
// A clump of curved grass blades; UV.y runs 0 at the root to 1 at the tip (read by grass.gdshader).
Ref<ArrayMesh> grass_clump_mesh() {
	PackedVector3Array v, n;
	PackedVector2Array uv;
	Rng rng(77);
	const int blades = 9;
	for (int b = 0; b < blades; ++b) {
		const float a = rng.range(0.0f, Math_TAU);
		const Vector3 dir(std::cos(a), 0, std::sin(a));
		const Vector3 base = dir * rng.range(0.0f, 0.09f);
		const Vector3 side = Vector3(0, 1, 0).cross(dir).normalized() * rng.range(0.022f, 0.032f);
		const float hgt = rng.range(0.16f, 0.34f);
		const float lean = rng.range(0.05f, 0.14f);
		Vector3 rows[3];
		float widths[3] = { 1.0f, 0.6f, 0.0f };
		for (int r = 0; r < 3; ++r) {
			const float t = float(r) / 2.0f;
			rows[r] = base + Vector3(0, hgt * t, 0) + dir * (lean * t * t);
		}
		const Vector3 nrm = (dir + Vector3(0, 0.6f, 0)).normalized();
		auto push = [&](const Vector3 &p, float t) {
			v.push_back(p);
			n.push_back(nrm);
			uv.push_back(Vector2(0.5f, t));
		};
		for (int r = 0; r < 2; ++r) {
			const float t0 = float(r) / 2.0f, t1 = float(r + 1) / 2.0f;
			const Vector3 a0 = rows[r] - side * widths[r], a1 = rows[r] + side * widths[r];
			const Vector3 b0 = rows[r + 1] - side * widths[r + 1], b1 = rows[r + 1] + side * widths[r + 1];
			push(a0, t0);
			push(b0, t1);
			push(a1, t0);
			push(a1, t0);
			push(b0, t1);
			push(b1, t1);
		}
	}
	Array arrays;
	arrays.resize(Mesh::ARRAY_MAX);
	arrays[Mesh::ARRAY_VERTEX] = v;
	arrays[Mesh::ARRAY_NORMAL] = n;
	arrays[Mesh::ARRAY_TEX_UV] = uv;
	Ref<ArrayMesh> mesh;
	mesh.instantiate();
	mesh->add_surface_from_arrays(Mesh::PRIMITIVE_TRIANGLES, arrays);
	return mesh;
}

void NeighborhoodGenerator::build_scatter(Node3D *world) {
	// Grass: one MultiMesh per lot, plus one per 24 m tile of open ground (culled by distance),
	// coloured through instance colours.
	Ref<ArrayMesh> clump = grass_clump_mesh();
	std::map<int64_t, std::vector<const ScatterItem *>> groups;
	for (const ScatterItem &s : ctx_->scatter) {
		if (s.kind != 0) {
			continue;
		}
		const int64_t key = s.lot >= 0 ? int64_t(s.lot) : 100000 + int64_t(std::floor(s.xf.origin.x / 24.0f)) * 1000 + int64_t(std::floor(s.xf.origin.z / 24.0f) + 500);
		groups[key].push_back(&s);
	}
	for (const auto &kv : groups) {
		const int64_t lot = kv.first;
		const std::vector<const ScatterItem *> &items = kv.second;
		Ref<MultiMesh> mm;
		mm.instantiate();
		mm->set_transform_format(MultiMesh::TRANSFORM_3D);
		mm->set_use_colors(true);
		mm->set_mesh(clump);
		mm->set_instance_count(int(items.size()));
		for (int i = 0; i < int(items.size()); ++i) {
			mm->set_instance_transform(i, items[i]->xf);
			mm->set_instance_color(i, items[i]->color);
		}
		MultiMeshInstance3D *mmi = memnew(MultiMeshInstance3D);
		mmi->set_name(String("Grass") + String::num_int64(int64_t(lot)));
		mmi->set_multimesh(mm);
		mmi->set_material_override(grass_mat_.is_valid() ? grass_mat_ : toon_);
		mmi->set_cast_shadows_setting(GeometryInstance3D::SHADOW_CASTING_SETTING_OFF);
		mmi->set_visibility_range_end(70.0f);
		mmi->set_visibility_range_end_margin(10.0f);
		mmi->set_visibility_range_fade_mode(GeometryInstance3D::VISIBILITY_RANGE_FADE_SELF);
		world->add_child(mmi);
	}
	// Leaf litter and clover.
	MeshBuilder tuft, leaves, clover;
	const int leaf_cells[] = { pal::MAPLE_RED, pal::MAPLE_ORANGE, pal::GOLDEN_LEAF, pal::RUST_LEAF };
	for (int i = 0; i < 4; ++i) {
		const float a = i * 1.7f;
		const Vector3 c(std::cos(a) * 0.18f, 0.012f + i * 0.004f, std::sin(a) * 0.18f);
		const Vector3 u = Vector3(std::cos(a + 0.8f), 0, std::sin(a + 0.8f)) * 0.09f;
		const Vector3 v = Vector3(-u.z, 0, u.x) * 0.6f;
		leaves.quad_out(c - u, c - v, c + u, c + v, leaf_cells[i], c - Vector3(0, 1, 0));
	}
	clover.blob(Vector3(0, 0.03f, 0), Vector3(0.16f, 0.04f, 0.16f), pal::HEDGE, 0);
	Ref<Mesh> meshes[3] = { tuft.build(), leaves.build(), clover.build() };
	if (assets_->get("leaf_scatter")->published) {
		meshes[1] = assets_->merged_mesh("leaf_scatter"); // a patch of painted maple leaves per item
	}
	for (int kind = 1; kind < 3; ++kind) {
		std::vector<Transform3D> xs;
		for (const ScatterItem &s : ctx_->scatter) {
			if (s.kind == kind) {
				xs.push_back(s.xf);
			}
		}
		if (xs.empty()) {
			continue;
		}
		Ref<MultiMesh> mm;
		mm.instantiate();
		mm->set_transform_format(MultiMesh::TRANSFORM_3D);
		mm->set_mesh(meshes[kind]);
		mm->set_instance_count(int(xs.size()));
		for (int i = 0; i < int(xs.size()); ++i) {
			mm->set_instance_transform(i, xs[i]);
		}
		MultiMeshInstance3D *mmi = memnew(MultiMeshInstance3D);
		mmi->set_name(String("Scatter") + String::num_int64(kind));
		mmi->set_multimesh(mm);
		mmi->set_material_override(toon_);
		mmi->set_cast_shadows_setting(GeometryInstance3D::SHADOW_CASTING_SETTING_OFF);
		world->add_child(mmi);
	}
}

void NeighborhoodGenerator::build_candy(Node3D *world) {
	register_candy_kinds(*assets_);
	std::vector<Ref<Mesh>> meshes;
	for (int k = 0; k < candy_kind_count(); ++k) {
		meshes.push_back(assets_->merged_mesh(candy_kind_id(k)));
	}
	candy_ = memnew(CandyField);
	candy_->set_name("CandyField");
	world->add_child(candy_);
	candy_->setup(meshes, candy_mat_, halo_mat_, int(ctx_->lots.size()));
	for (size_t i = 0; i < ctx_->candy.size(); ++i) {
		if (i < candy_filter_.size() && candy_filter_[i]) {
			continue; // collected on an earlier visit
		}
		CandySpec c = ctx_->candy[i];
		c.id = int(i);
		// Placement chose the tier; pick this piece's kind from everything in that tier.
		c.kind = candy_kind_of_tier(candy_tier(c.kind), sub_seed(ctx_->seed, 0xCA4D, i));
		candy_->add(c);
	}
	candy_->finalize();
}

} // namespace tot
