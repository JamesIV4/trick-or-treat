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
#include "play/vfx.h"

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

Dictionary NeighborhoodGenerator::generate_settlement(int64_t seed, SettlementKind kind, bool lane_open, int port_side) {
	return run(seed, kind, true, lane_open, port_side);
}

Dictionary NeighborhoodGenerator::run(int64_t seed, SettlementKind kind, bool embedded, bool lane_open, int port_side) {
	if (!assets_loaded_) {
		assets_->load(catalog_path_, toon_);
		assets_loaded_ = true;
	}
	SettlementPlan plan = plan_settlement(assets_, seed, kind, embedded, lane_open, port_side, false);
	const double plan_ms = plan.plan_ms;
	const Dictionary stage_ms = plan.stage_ms;
	const auto t1 = std::chrono::steady_clock::now();
	begin_build(std::move(plan));
	build_step(0);
	const auto t2 = std::chrono::steady_clock::now();
	GenContext &ctx = *ctx_;

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
	stats["plan_ms"] = plan_ms;
	stats["build_ms"] = double(std::chrono::duration_cast<std::chrono::microseconds>(t2 - t1).count()) / 1000.0;
	stats["stage_ms"] = stage_ms;
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

namespace {
std::vector<MagicPiece> magic_pieces(const GenContext &ctx);
} // namespace

// Every data stage, in order. Runs on worker threads (WorldStreamer): it touches only the GenContext it
// makes and reads the asset library.
SettlementPlan plan_settlement(AssetLibrary *assets, int64_t seed, SettlementKind kind, bool embedded, bool lane_open, int port_side, bool pack) {
	const auto t0 = std::chrono::steady_clock::now();
	SettlementPlan out;
	out.ctx = std::make_unique<GenContext>();
	GenContext &ctx = *out.ctx;
	ctx.seed = uint64_t(seed);
	ctx.assets = assets;
	ctx.kind = kind;
	ctx.embedded = embedded;
	ctx.lane_open = lane_open;
	ctx.port_side = port_side;
	const bool town = kind == SettlementKind::Town;
	// Per-stage timings for the --sweep (Stage 2a load-time report).
	Dictionary stage_ms;
	auto tick = [&, last = t0](const char *name) mutable {
		const auto now = std::chrono::steady_clock::now();
		stage_ms[name] = double(std::chrono::duration_cast<std::chrono::microseconds>(now - last).count()) / 1000.0;
		last = now;
	};

	plan_street(ctx);
	tick("street");
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
	tick("lots_houses");
	build_street(ctx);
	build_boundary(ctx);
	tick("street_mesh_boundary");
	for (Lot &lot : ctx.lots) {
		if (town) {
			dress_shopfront(ctx, lot);
		} else {
			dress_yard(ctx, lot);
		}
	}
	tick("yards");
	place_street_rhythm(ctx);
	if (town) {
		place_town_square(ctx);
	} else {
		place_pocket_parks(ctx);
	}
	register_prop_collisions(ctx);
	tick("rhythm_parks");
	weave_candy(ctx);
	tick("candy");
	validate_traversal(ctx);
	tick("validate");
	plan_magic_paths(ctx);
	scatter_dressing(ctx);
	tick("magic_scatter");
	ctx.stats.props = int(ctx.props.size());
	const auto t1 = std::chrono::steady_clock::now();
	out.plan_ms = double(std::chrono::duration_cast<std::chrono::microseconds>(t1 - t0).count()) / 1000.0;
	out.stage_ms = stage_ms;
	if (pack) {
		// The surface arrays of every mesh build_nodes() makes from a MeshBuilder, and the magic trail's.
		out.packed = true;
		out.meshes.push_back(ctx.street_mesh.arrays());
		out.meshes.push_back(ctx.boundary_mesh.arrays());
		for (const MeshBuilder &b : ctx.lot_mesh) {
			out.meshes.push_back(b.arrays());
		}
		for (const SpringSpec &sp : ctx.springs) {
			out.meshes.push_back(sp.mesh.arrays());
		}
		out.magic = magic_pieces(ctx);
		out.strings = strings_data(ctx);
		out.pack_ms = double(std::chrono::duration_cast<std::chrono::microseconds>(std::chrono::steady_clock::now() - t1).count()) / 1000.0;
	}
	return out;
}

NeighborhoodGenerator::~NeighborhoodGenerator() {
	// A settlement dropped in the middle of its collision stage: the bodies collecting shapes are not in the
	// tree yet, so nothing else frees them.
	for (StaticBody3D *b : { coll_.solids, coll_.structures, coll_.boundary, coll_.fences }) {
		if (b && !b->get_parent()) {
			memdelete(b);
		}
	}
	for (auto &kv : coll_.bouncy) {
		if (kv.second && !kv.second->get_parent()) {
			memdelete(kv.second);
		}
	}
}

uint64_t NeighborhoodGenerator::build_clock_us() {
	return uint64_t(std::chrono::duration_cast<std::chrono::microseconds>(std::chrono::steady_clock::now().time_since_epoch()).count());
}

String NeighborhoodGenerator::build_report() const {
	static const char *names[] = { "root", "lots", "springs", "collision", "props", "doors", "lights", "strings", "scatter", "candy", "fireflies", "magic", "signs" };
	String s;
	for (int i = 0; i < 13; ++i) {
		s += vformat("%s %.1f/%.1f ", names[i], build_ms_[i], build_max_ms_[i]);
	}
	return s;
}

void NeighborhoodGenerator::begin_build(SettlementPlan &&plan, bool hidden) {
	for (int i = 0; i < 13; ++i) {
		build_ms_[i] = build_max_ms_[i] = 0.0f;
	}
	ctx_ = std::move(plan.ctx);
	hide_new_ = hidden;
	hidden_.clear();
	reveal_i_ = 0;
	packed_ = plan.packed;
	packed_meshes_ = std::move(plan.meshes);
	packed_magic_ = std::move(plan.magic);
	packed_strings_ = std::move(plan.strings);
	build_stage_ = 0;
	build_i_ = 0;
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
	// The road the point belongs to (inside a side road's corridor, past its mouth: that road).
	const RoadHit h = ctx_->roads.nearest(p);
	if (h.road < 0) {
		return 0.0f;
	}
	const Vector3 t = ctx_->roads.roads[h.road].tangent_at(h.s_ext);
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
	close_house(lot, true);
}

namespace {
// Porch glow while a house still has candy to give, and once it's done for the night.
constexpr float PORCH_OPEN = 1.7f, PORCH_DONE = 0.45f;   // porch light energy
constexpr float LANTERN_OPEN = 1.35f, LANTERN_DONE = 0.2f; // lantern glow_boost
constexpr float JACK_DONE = 0.12f;                        // a blown-out jack-o'-lantern
constexpr float CLOSE_TIME = 1.2f;                        // the porch winds down over this long
} // namespace

void NeighborhoodGenerator::close_house(int lot, bool animate) {
	if (lot < 0 || lot >= int(glow_.size()) || glow_[lot].closed) {
		return;
	}
	LotGlow &g = glow_[lot];
	g.closed = true;
	g.t = animate ? 0.0f : CLOSE_TIME + 1.0f;
	build_thanks_sign(lot);
	if (animate) {
		// The candles blow out with a little puff of smoke.
		if (Vfx *vfx = Vfx::get()) {
			for (const Vector3 &p : g.jack_pos) {
				vfx->smoke(p + Vector3(0, 0.35f, 0), Color(0.85f, 0.85f, 0.9f), 0.45f, 3);
			}
		}
	}
}

// A hanging wooden "Thank you!" sign under the porch light (or the shop's awning), facing the
// street, with a small glowing lantern of its own beside it so the closed porch still feels warm.
void NeighborhoodGenerator::build_thanks_sign(int lot) {
	if (!world_ || lot < 0 || lot >= int(ctx_->lots.size())) {
		return;
	}
	const Lot &L = ctx_->lots[lot];
	const Basis face = L.xf.basis.orthonormalized(); // +Z toward the street
	Node3D *root = memnew(Node3D);
	root->set_name(vformat("ThanksSign%d", lot));
	const Vector3 at = L.porch_light + face.xform(Vector3(0, -0.62f, 0.12f));
	root->set_transform(Transform3D(face.scaled(Vector3(1.3f, 1.3f, 1.3f)), at));
	put(world_, root);
	MeshBuilder b;
	// Two little chains from a bracket, and a rounded plank with a painted border.
	b.box(Vector3(0, 0.36f, 0), Vector3(0.36f, 0.025f, 0.03f), pal::LAMP_BLACK);
	for (float sx : { -0.26f, 0.26f }) {
		b.box(Vector3(sx, 0.24f, 0), Vector3(0.008f, 0.12f, 0.008f), pal::IRON);
	}
	b.rounded_box(Vector3(0, 0, 0), Vector3(0.34f, 0.13f, 0.025f), 0.03f, mat_cell(pal::WOOD_LIGHT, mat::PLANKS));
	b.rounded_box(Vector3(0, 0, 0.012f), Vector3(0.31f, 0.105f, 0.018f), 0.02f, mat_cell(pal::BUNTING_CREAM, mat::FLAT));
	MeshInstance3D *board = memnew(MeshInstance3D);
	board->set_mesh(b.build());
	board->set_material_override(toon_);
	root->add_child(board);
	Label3D *txt = memnew(Label3D);
	txt->set_text("Thank you!");
	txt->set_font(display_font());
	txt->set_font_size(64);
	txt->set_pixel_size(0.0028f);
	txt->set_modulate(Color(0.55f, 0.22f, 0.12f));
	txt->set_outline_size(0);
	txt->set_position(Vector3(0, 0.03f, 0.034f));
	fit_label(txt, 0.54f, 0.1f);
	root->add_child(txt);
	Label3D *sub = memnew(Label3D);
	sub->set_text("Happy Halloween");
	sub->set_font(display_font());
	sub->set_font_size(32);
	sub->set_pixel_size(0.0028f);
	sub->set_modulate(Color(0.45f, 0.3f, 0.6f));
	sub->set_outline_size(0);
	sub->set_position(Vector3(0, -0.058f, 0.034f));
	fit_label(sub, 0.5f, 0.05f);
	root->add_child(sub);
	// The sign's own lantern, hanging beside it.
	MeshBuilder lb;
	const Vector3 lp(0.52f, 0.05f, 0.02f);
	lb.box(lp + Vector3(0, 0.31f, 0), Vector3(0.006f, 0.1f, 0.006f), pal::IRON);
	lb.cylinder(lp + Vector3(0, 0.15f, 0), 0.06f, 0.05f, 8, pal::LAMP_BLACK, 0.02f);
	lb.blob(lp + Vector3(0, 0.08f, 0), Vector3(0.055f, 0.07f, 0.055f), pal::GLOW_LANTERN, 1);
	lb.cylinder(lp + Vector3(0, -0.005f, 0), 0.05f, 0.025f, 8, pal::LAMP_BLACK);
	MeshInstance3D *lan = memnew(MeshInstance3D);
	lan->set_mesh(lb.build());
	lan->set_material_override(toon_);
	root->add_child(lan);
	OmniLight3D *o = memnew(OmniLight3D);
	o->set_position(lp + Vector3(0, 0.08f, 0.15f));
	o->set_color(Color(1.0f, 0.78f, 0.45f));
	o->set_param(Light3D::PARAM_ENERGY, 0.9f);
	o->set_param(Light3D::PARAM_RANGE, 3.2f);
	o->set_param(Light3D::PARAM_ATTENUATION, 1.4f);
	o->set_shadow(false);
	root->add_child(o);
	glow_[lot].sign = root;
	glow_[lot].sign_rest = root->get_transform();
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
	// Houses done for the night: the porch winds down, and the thank-you sign drops in and swings.
	for (LotGlow &g : glow_) {
		if (!g.closed || g.t > CLOSE_TIME + 3.0f) {
			continue;
		}
		g.t += dt;
		const float k = Math::smoothstep(0.0f, CLOSE_TIME, g.t);
		for (GeometryInstance3D *gi : g.lanterns) {
			gi->set_instance_shader_parameter("glow_boost", Math::lerp(LANTERN_OPEN, LANTERN_DONE, k));
		}
		const float blow = Math::smoothstep(0.0f, 0.25f, g.t); // candles go out quickly
		for (GeometryInstance3D *gi : g.jacks) {
			gi->set_instance_shader_parameter("glow_boost", Math::lerp(1.0f, JACK_DONE, blow));
		}
		for (OmniLight3D *o : g.jack_lights) {
			o->set_visible(blow < 0.99f);
			o->set_param(Light3D::PARAM_ENERGY, Math::lerp(o->get_param(Light3D::PARAM_ENERGY), 0.0f, blow));
		}
		if (g.porch) {
			g.porch->set_param(Light3D::PARAM_ENERGY, Math::lerp(PORCH_OPEN, PORCH_DONE, k));
		}
		if (g.sign) {
			// Drops from above, bounces, then a soft swing that settles.
			const float s = Math::max(0.0f, g.t - 0.3f);
			const float drop = s < 0.35f ? (1.0f - s / 0.35f) * (1.0f - s / 0.35f) * 0.6f : std::abs(std::sin((s - 0.35f) * 9.0f)) * 0.06f * std::exp(-(s - 0.35f) * 5.0f);
			const float swing = std::sin(s * 5.0f) * 0.12f * std::exp(-s * 1.3f);
			g.sign->set_visible(g.t > 0.3f);
			Transform3D xf = g.sign_rest;
			xf.origin += Vector3(0, drop, 0);
			xf.basis = xf.basis * Basis(Vector3(1, 0, 0), swing);
			g.sign->set_transform(xf);
		}
	}
}

// ---------------------------------------------------------------- scene construction
// build_step() goes through these in order; the long ones (lot meshes, collision, props, doors, scatter,
// magic) resume where the last call stopped (build_i_).
enum BuildStage { BS_ROOT, BS_LOTS, BS_SPRINGS, BS_COLLISION, BS_PROPS, BS_DOORS, BS_LIGHTS, BS_STRINGS, BS_SCATTER, BS_CANDY, BS_FIREFLIES, BS_MAGIC, BS_SIGNS, BS_COUNT };

void NeighborhoodGenerator::build_nodes() {
	packed_ = false;
	packed_meshes_.clear();
	packed_magic_.clear();
	build_stage_ = 0;
	build_i_ = 0;
	build_step(0);
}

MeshInstance3D *NeighborhoodGenerator::add_mesh(MeshBuilder &b, const String &name, int packed_index) {
	if (b.empty()) {
		return nullptr;
	}
	MeshInstance3D *mi = memnew(MeshInstance3D);
	mi->set_name(name);
	if (packed_ && packed_index < int(packed_meshes_.size())) {
		mi->set_mesh(MeshBuilder::mesh_from_arrays(packed_meshes_[packed_index]));
		packed_meshes_[packed_index] = Array(); // let go now (freeing them all at the end is a few ms in one go)
	} else {
		mi->set_mesh(b.build());
	}
	mi->set_material_override(toon_);
	put(world_, mi);
	return mi;
}

void NeighborhoodGenerator::put(Node *parent, Node *child) {
	if (Node3D *n = Object::cast_to<Node3D>(child)) {
		shown_.push_back(n); // what conceal_step() hides again
		if (hide_new_) {
			n->set_visible(false); // out of the tree: no propagation yet
			hidden_.push_back(n);
		}
	}
	parent->add_child(child);
}

bool NeighborhoodGenerator::reveal_step(uint64_t deadline_us) {
	budget_.deadline = deadline_us;
	budget_.did = false;
	for (; reveal_i_ < hidden_.size(); ++reveal_i_) {
		if (budget_.over()) {
			return false;
		}
		hidden_[reveal_i_]->set_visible(true);
		budget_.did = true;
	}
	hidden_.clear();
	reveal_i_ = 0;
	hide_new_ = false;
	return true;
}

bool NeighborhoodGenerator::conceal_step(uint64_t deadline_us) {
	if (!world_) {
		return true;
	}
	budget_.deadline = deadline_us;
	budget_.did = false;
	if (!concealing_) {
		// What reveal_step() will show again: every node the build (or a later thanks sign) put up that is
		// showing now (the spine pieces and house branches that have faded out stay hidden).
		concealing_ = true;
		conceal_list_.clear();
		conceal_i_ = 0;
	}
	for (; conceal_i_ < shown_.size(); ++conceal_i_) {
		if (budget_.over()) {
			return false;
		}
		Node3D *n = shown_[conceal_i_];
		if (n->is_visible()) {
			n->set_visible(false);
			conceal_list_.push_back(n);
			budget_.did = true;
		}
	}
	hidden_ = std::move(conceal_list_);
	conceal_list_.clear();
	reveal_i_ = 0;
	concealing_ = false;
	return true;
}

bool NeighborhoodGenerator::build_step(uint64_t deadline_us) {
	budget_.deadline = deadline_us;
	budget_.did = false;
	while (build_stage_ >= 0) {
		if (budget_.over()) {
			return false;
		}
		GenContext &ctx = *ctx_;
		bool done = true;
		const uint64_t t_unit = build_clock_us();
		const int stage = build_stage_;
		switch (build_stage_) {
			case BS_ROOT: {
				if (world_) {
					remove_child(world_);
					world_->queue_free();
					world_ = nullptr;
					candy_ = nullptr;
				}
				doors_.clear();
				glow_.clear();
				squishies_.clear();
				shown_.clear();
				world_ = memnew(Node3D);
				world_->set_name("World");
				add_child(world_);
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
					put(world_, mi);
					StaticBody3D *body = memnew(StaticBody3D);
					body->set_name("GroundBody");
					CollisionShape3D *cs = memnew(CollisionShape3D);
					Ref<WorldBoundaryShape3D> plane;
					plane.instantiate();
					cs->set_shape(plane);
					body->add_child(cs);
					world_->add_child(body);
				}
				add_mesh(ctx.street_mesh, "Street", 0);
				add_mesh(ctx.boundary_mesh, "Boundary", 1);
				glow_.resize(ctx.lots.size());
				break;
			}
			case BS_LOTS:
				// House, fence and landmark geometry is authored in lot-local space.
				for (; build_i_ < ctx.lots.size(); ++build_i_) {
					if (budget_.over()) {
						return false;
					}
					if (MeshInstance3D *mi = add_mesh(ctx.lot_mesh[build_i_], String("Lot") + String::num_int64(int64_t(build_i_)), 2 + int(build_i_))) {
						mi->set_transform(ctx.lots[build_i_].xf);
					}
					budget_.did = true;
				}
				break;
			case BS_SPRINGS:
				// Springy building parts (shop awnings): their own nodes so they can dip when bounced on.
				for (size_t k = 0; k < ctx.springs.size(); ++k) {
					SpringSpec &sp = ctx.springs[k];
					if (MeshInstance3D *mi = add_mesh(sp.mesh, "Awning", 2 + int(ctx.lots.size() + k))) {
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
				break;
			case BS_COLLISION:
				done = build_collision(world_);
				break;
			case BS_PROPS:
				done = build_props(world_);
				break;
			case BS_DOORS:
				done = build_doors(world_);
				break;
			case BS_LIGHTS:
				done = build_lights(world_);
				break;
			case BS_STRINGS:
				done = build_strings(world_);
				break;
			case BS_SCATTER:
				done = build_scatter(world_);
				break;
			case BS_CANDY:
				build_candy(world_);
				break;
			case BS_FIREFLIES:
				build_fireflies(world_);
				break;
			case BS_MAGIC:
				done = build_magic(world_);
				break;
			case BS_SIGNS:
				build_signs(world_);
				break;
			default:
				break;
		}
		const float spent = float(build_clock_us() - t_unit) / 1000.0f;
		build_ms_[stage] += spent;
		build_max_ms_[stage] = Math::max(build_max_ms_[stage], spent);
		budget_.did = true;
		if (!done) {
			return false;
		}
		build_i_ = 0;
		if (++build_stage_ >= BS_COUNT) {
			build_stage_ = -1;
			packed_meshes_.clear();
			packed_magic_.clear();
			set_process(true);
		}
	}
	return true;
}

void NeighborhoodGenerator::rebuild_candy(const std::vector<char> &taken) {
	candy_filter_ = taken;
	if (!world_ || !ctx_) {
		return;
	}
	if (candy_) {
		shown_.erase(std::remove(shown_.begin(), shown_.end(), static_cast<Node3D *>(candy_)), shown_.end());
		hidden_.erase(std::remove(hidden_.begin(), hidden_.end(), static_cast<Node3D *>(candy_)), hidden_.end());
		world_->remove_child(candy_);
		candy_->queue_free();
		candy_ = nullptr;
	}
	build_candy(world_);
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
		if (s.width > 0.0f) {
			fit_label(l, s.width, s.height > 0.0f ? s.height : 1e3f);
		}
		l->set_draw_flag(Label3D::FLAG_DOUBLE_SIDED, false);
		l->set_draw_flag(Label3D::FLAG_SHADED, false);
		put(world, l);
	}
}

// Magic guide trail meshes: the shader spirals three strands around these centrelines. The geometry is
// plain data (magic_pieces, also run ahead on the preload worker); the nodes go up one piece per unit.
bool NeighborhoodGenerator::build_magic(Node3D *world) {
	if (build_i_ == 0) {
		magic_nodes_.assign(ctx_->lots.size(), MagicNode());
		spine_.clear();
		if (magic_mat_.is_null()) {
			return true;
		}
		if (!packed_) {
			packed_magic_ = magic_pieces(*ctx_);
		}
	}
	for (; build_i_ < packed_magic_.size(); ++build_i_) {
		if (budget_.over()) {
			return false;
		}
		const MagicPiece &mp = packed_magic_[build_i_];
		MeshInstance3D *mi = memnew(MeshInstance3D);
		mi->set_name(mp.lot >= 0 ? String("MagicTrail") + String::num_int64(mp.lot) : String("MagicSpine"));
		mi->set_mesh(MeshBuilder::mesh_from_arrays(mp.arrays));
		packed_magic_[build_i_].arrays = Array();
		mi->set_material_override(magic_mat_);
		mi->set_cast_shadows_setting(GeometryInstance3D::SHADOW_CASTING_SETTING_OFF);
		mi->set_extra_cull_margin(1.0f);
		mi->set_instance_shader_parameter("fade", mp.lot >= 0 ? 1.0f : 0.7f);
		put(world, mi);
		if (mp.lot >= 0) {
			if (mp.lot < int(magic_nodes_.size())) {
				magic_nodes_[mp.lot].node = mi;
			}
		} else {
			SpineChunk c;
			c.node = mi;
			c.lots = mp.lots;
			spine_.push_back(c);
		}
		budget_.did = true;
	}
	return true;
}

namespace {
std::vector<MagicPiece> magic_pieces(const GenContext &ctx) {
	std::vector<MagicPiece> out;
	for (const MagicPath &path : ctx.magic) {
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
		// The trail mesh over pts[i0..i1] (distances stay global, so pieces join seamlessly).
		auto piece = [&](size_t i0, size_t i1) {
			std::vector<Vector3> v, n;
			std::vector<float> tan;
			std::vector<Vector2> uv, uv2;
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
						tan.insert(tan.end(), { t.x, t.y, t.z, 1.0f });
						uv.push_back(Vector2(dist[k], sd[corner]));
						uv2.push_back(Vector2(float(strand), dist[k] / total));
					}
				}
			}
			PackedVector3Array pv, pn;
			PackedFloat32Array pt;
			PackedVector2Array puv, puv2;
			pv.resize(int64_t(v.size()));
			pn.resize(int64_t(n.size()));
			pt.resize(int64_t(tan.size()));
			puv.resize(int64_t(uv.size()));
			puv2.resize(int64_t(uv2.size()));
			std::copy(v.begin(), v.end(), pv.ptrw());
			std::copy(n.begin(), n.end(), pn.ptrw());
			std::copy(tan.begin(), tan.end(), pt.ptrw());
			std::copy(uv.begin(), uv.end(), puv.ptrw());
			std::copy(uv2.begin(), uv2.end(), puv2.ptrw());
			Array arrays;
			arrays.resize(Mesh::ARRAY_MAX);
			arrays[Mesh::ARRAY_VERTEX] = pv;
			arrays[Mesh::ARRAY_NORMAL] = pn;
			arrays[Mesh::ARRAY_TANGENT] = pt;
			arrays[Mesh::ARRAY_TEX_UV] = puv;
			arrays[Mesh::ARRAY_TEX_UV2] = puv2;
			MagicPiece mp;
			mp.lot = path.lot;
			mp.arrays = arrays;
			return mp;
		};
		if (path.lot >= 0) {
			out.push_back(piece(0, pts.size() - 1));
			continue;
		}
		// The spine: about 10 m per piece, each tagged with the houses near it.
		for (size_t i0 = 0; i0 + 1 < pts.size();) {
			size_t i1 = i0 + 1;
			while (i1 + 1 < pts.size() && dist[i1] - dist[i0] < 10.0f) {
				++i1;
			}
			MagicPiece mp = piece(i0, i1);
			mp.lot = -1;
			const Vector3 mid = pts[(i0 + i1) / 2];
			for (size_t l = 0; l < ctx.lots.size(); ++l) {
				const Vector3 d = ctx.lots[l].doorbell - mid;
				if (Vector2(d.x, d.z).length() < 16.0f) {
					mp.lots.push_back(int(l));
				}
			}
			out.push_back(std::move(mp));
			i0 = i1;
		}
	}
	return out;
}
} // namespace

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
	// As dense over the bigger neighbourhoods as over the old 100 x 60 m street (towns keep theirs).
	const float area = (ctx_->bound_x1 - ctx_->bound_x0) * (ctx_->bound_z1 - ctx_->bound_z0);
	p->set_amount(ctx_->kind == SettlementKind::Town ? 260 : Math::clamp(int(260.0f * area / 6000.0f), 260, 700));
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
	put(world, p);
}

// The bodies collect their shapes out of the tree (a time-sliced build adds them over several calls) and
// join the world together at the end, as before.
bool NeighborhoodGenerator::build_collision(Node3D *world) {
	CollisionBodies &cb = coll_;
	if (!cb.solids) { // (not build_i_ == 0: a slice can end before the first box)
		// Layers: 1 = solid world, 2 = playground boundary, 3 (value 4) = big structures the camera
		// pulls in front of (house bodies, roofs, garages).
		cb.solids = memnew(StaticBody3D);
		cb.solids->set_name("Solids");
		cb.structures = memnew(StaticBody3D);
		cb.structures->set_name("Structures");
		cb.structures->set_collision_layer(1 | 4);
		cb.boundary = memnew(StaticBody3D);
		cb.boundary->set_name("Boundary");
		cb.boundary->set_collision_layer(2);
		// Yard fences: solid like the rest of the world, but tagged so fireballs pass straight through.
		cb.fences = memnew(StaticBody3D);
		cb.fences->set_name("Fences");
		cb.fences->set_meta("shot_through", true);
		cb.bouncy.clear();
	}
	StaticBody3D *solids = cb.solids, *structures = cb.structures, *boundary = cb.boundary, *fences = cb.fences;
	// One body per launch speed (inflatables, mini and big trampolines).
	std::map<int, StaticBody3D *> &bouncy = cb.bouncy;
	auto bouncy_body = [&](float speed) {
		StaticBody3D *&b = bouncy[int(std::round(speed * 10.0f))];
		if (!b) {
			b = memnew(StaticBody3D);
			b->set_name(String("Bouncy") + String::num(speed, 1));
			b->set_meta("bounce", speed);
		}
		return b;
	};
	for (; build_i_ < ctx_->boxes.size(); ++build_i_) {
		if (budget_.over()) {
			return false;
		}
		budget_.did = true;
		const OBox &b = ctx_->boxes[build_i_];
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
	cb = CollisionBodies();
	return true;
}

bool NeighborhoodGenerator::build_props(Node3D *world) {
	if (!props_root_) {
		props_root_ = memnew(Node3D);
		props_root_->set_name("Props");
		world->add_child(props_root_);
	}
	Node3D *root = props_root_;
	for (; build_i_ < ctx_->props.size(); ++build_i_) {
		if (budget_.over()) {
			return false;
		}
		budget_.did = true;
		const PropPlacement &pp = ctx_->props[build_i_];
		Node3D *n = assets_->instantiate(pp.id);
		n->set_transform(pp.xf);
		put(root, n);
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
			// Lit while the house still has candy to give; they dim once it's trick-or-treated.
			set_instance_param_recursive(n, "glow_boost", LANTERN_OPEN);
			collect_geometry(n, glow_[pp.lot].lanterns);
		}
		if (pp.id.rfind("jackolantern", 0) == 0 && pp.lot >= 0 && pp.lot < int(ctx_->lots.size()) &&
				pp.xf.origin.distance_to(ctx_->lots[pp.lot].porch_light) < 6.0f) {
			// Jack-o'-lanterns on and by the porch blow out when the house is done.
			collect_geometry(n, glow_[pp.lot].jacks);
			glow_[pp.lot].jack_pos.push_back(pp.xf.origin);
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
	props_root_ = nullptr;
	return true;
}

bool NeighborhoodGenerator::build_doors(Node3D *world) {
	if (build_i_ == 0) {
		doors_.assign(ctx_->lots.size(), DoorNode());
	}
	for (; build_i_ < ctx_->doors.size(); ++build_i_) {
		if (budget_.over()) {
			return false;
		}
		budget_.did = true;
		const DoorSpec &d = ctx_->doors[build_i_];
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
		put(world, hinge);
		if (d.lot >= 0) {
			doors_[d.lot].hinge = pivot;
		}
	}
	return true;
}

bool NeighborhoodGenerator::build_lights(Node3D *world) {
	if (lights_root_) {
		return build_light_nodes();
	}
	Node3D *root = memnew(Node3D);
	root->set_name("Lights");
	put(world, root);
	lights_root_ = root;
	std::vector<LightSpec> &lights = light_list_;
	lights.clear();
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
	return build_light_nodes();
}

// One OmniLight3D per piece of work (a time-sliced build).
bool NeighborhoodGenerator::build_light_nodes() {
	Node3D *root = lights_root_;
	for (; build_i_ < light_list_.size(); ++build_i_) {
		if (budget_.over()) {
			return false;
		}
		budget_.did = true;
		const LightSpec &l = light_list_[build_i_];
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
		if (l.kind == LIGHT_JACK && l.lot >= 0 && l.lot < int(ctx_->lots.size()) && l.pos.distance_to(ctx_->lots[l.lot].porch_light) < 6.0f) {
			glow_[l.lot].jack_lights.push_back(o);
		}
	}
	ctx_->stats.lights = int(light_list_.size());
	light_list_.clear();
	lights_root_ = nullptr;
	return true;
}

// The strung lights' wires and where each bulb, jack and pennant hangs: plain data (run ahead on the
// preload worker with the plan).
StringsData strings_data(const GenContext &ctx) {
	StringsData out;
	MeshBuilder wire;
	std::vector<Transform3D> *items = out.items;
	for (const StringLight &s : ctx.strings) {
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
	out.wire = wire.arrays();
	return out;
}

// The wires, then one MultiMesh per kind, a piece of work each.
bool NeighborhoodGenerator::build_strings(Node3D *world) {
	if (ctx_->strings.empty()) {
		return true;
	}
	// Round bulbs, mini jack-o'-lanterns, and bunting pennants in three alternating colours.
	static const char *ids[] = { "string_bulb", "string_jack", "bunting_flag_orange", "bunting_flag_purple", "bunting_flag_cream" };
	constexpr int KINDS = 5;
	if (build_i_ == 0) {
		if (!packed_) {
			packed_strings_ = strings_data(*ctx_);
		}
		MeshInstance3D *mi = memnew(MeshInstance3D);
		mi->set_name("StringWires");
		mi->set_mesh(MeshBuilder::mesh_from_arrays(packed_strings_.wire));
		mi->set_material_override(toon_);
		put(world, mi);
		build_i_ = 1;
		budget_.did = true;
	}
	const std::vector<Transform3D> *items = packed_strings_.items;
	for (; build_i_ <= size_t(KINDS); ++build_i_) {
		const int k = int(build_i_) - 1;
		if (items[k].empty()) {
			continue;
		}
		if (budget_.over()) {
			return false;
		}
		budget_.did = true;
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
		put(world, mmi);
	}
	packed_strings_ = StringsData();
	return true;
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

bool NeighborhoodGenerator::build_scatter(Node3D *world) {
	// Grass: one MultiMesh per lot, plus one per 24 m tile of open ground (culled by distance),
	// coloured through instance colours.
	if (build_i_ == 0 && scatter_clump_.is_null()) {
		scatter_clump_ = grass_clump_mesh();
		std::map<int64_t, std::vector<const ScatterItem *>> groups;
		for (const ScatterItem &s : ctx_->scatter) {
			if (s.kind != 0) {
				continue;
			}
			const int64_t key = s.lot >= 0 ? int64_t(s.lot) : 100000 + int64_t(std::floor(s.xf.origin.x / 24.0f)) * 1000 + int64_t(std::floor(s.xf.origin.z / 24.0f) + 500);
			groups[key].push_back(&s);
		}
		scatter_groups_.assign(groups.begin(), groups.end());
	}
	const Ref<ArrayMesh> clump = scatter_clump_;
	for (; build_i_ < scatter_groups_.size(); ++build_i_) {
		if (budget_.over()) {
			return false;
		}
		budget_.did = true;
		const auto &kv = scatter_groups_[build_i_];
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
		put(world, mmi);
	}
	if (budget_.over()) {
		return false;
	}
	scatter_groups_.clear();
	scatter_clump_.unref();
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
		put(world, mmi);
	}
	return true;
}

void NeighborhoodGenerator::build_candy(Node3D *world) {
	register_candy_kinds(*assets_);
	std::vector<Ref<Mesh>> meshes;
	for (int k = 0; k < candy_kind_count(); ++k) {
		meshes.push_back(assets_->merged_mesh(candy_kind_id(k)));
	}
	candy_ = memnew(CandyField);
	candy_->set_name("CandyField");
	put(world, candy_);
	candy_->setup(meshes, candy_mat_, halo_mat_, int(ctx_->lots.size()));
	for (size_t i = 0; i < ctx_->candy.size(); ++i) {
		if (i < candy_filter_.size() && candy_filter_[i]) {
			continue; // collected on an earlier visit
		}
		CandySpec c = ctx_->candy[i];
		c.id = int(i);
		// Placement chose the tier; pick this piece's kind from everything in that tier. Everyday and
		// chocolate pieces of one rhythm group (a cluster, an arc, a run) share one kind per tier, so each
		// shape reads as one.
		const bool grouped = c.group >= 0 && candy_tier(c.kind) <= 1;
		c.kind = candy_kind_of_tier(candy_tier(c.kind), sub_seed(ctx_->seed, 0xCA4D, grouped ? 0x100000u + uint64_t(c.group) : uint64_t(i)));
		candy_->add(c);
	}
	candy_->finalize();
}

} // namespace tot
