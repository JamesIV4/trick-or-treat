#pragma once

#include <godot_cpp/classes/material.hpp>
#include <godot_cpp/classes/mesh_instance3d.hpp>
#include <godot_cpp/classes/node3d.hpp>
#include <godot_cpp/classes/omni_light3d.hpp>
#include <godot_cpp/variant/dictionary.hpp>

#include <map>
#include <memory>
#include <vector>

#include "gen/asset_library.h"
#include "gen/context.h"

namespace godot {
class StaticBody3D;
}

namespace tot {

class CandyField;

// One magic-trail mesh piece (a house's branch, or ~10 m of the street spine), its surface arrays
// made ahead of the node.
struct MagicPiece {
	int lot = -1; // -1: a spine piece
	godot::Array arrays;
	std::vector<int> lots; // spine pieces: the houses near it
};

// The strung lights: wire mesh arrays and the hanging items per kind (bulbs, jacks, 3 pennant colours).
struct StringsData {
	godot::Array wire;
	std::vector<Transform3D> items[5];
};
StringsData strings_data(const GenContext &ctx);

// A settlement planned off the scene tree: every data stage of the generator (street, lots, houses,
// yards, props, candy, magic, scatter) and, with `pack`, the mesh surface arrays. No node, resource or
// engine-singleton work, so WorldStreamer runs it on a worker thread; `assets` is only read.
// Anything a later stage adds to the plan belongs in plan_settlement (neighborhood_generator.cpp).
struct SettlementPlan {
	std::unique_ptr<GenContext> ctx;
	bool packed = false;
	std::vector<godot::Array> meshes; // packed: street, boundary, each lot, each spring (in that order)
	std::vector<MagicPiece> magic;    // packed: the magic trail pieces in build order
	StringsData strings;              // packed: the strung lights
	godot::Dictionary stage_ms;
	double plan_ms = 0.0, pack_ms = 0.0;
};
SettlementPlan plan_settlement(AssetLibrary *assets, int64_t seed, SettlementKind kind, bool embedded, bool lane_open, int port_side, bool pack);

// Runs the ten generation stages for a seed and builds the playable neighborhood under itself.
class NeighborhoodGenerator : public godot::Node3D {
	GDCLASS(NeighborhoodGenerator, godot::Node3D)

public:
	godot::Dictionary generate(int64_t seed);
	// Open-world use: a neighborhood or town that is one piece of the streamed world (no ground
	// plane of its own, open street ends; `lane_open` when a road continues past the side lane).
	// port_side: a neighbourhood's N/S port road (GenContext::port_side; WorldPlan's RegionPlan::hood_port).
	godot::Dictionary generate_settlement(int64_t seed, SettlementKind kind, bool lane_open, int port_side = GenContext::PORT_FROM_SEED);
	// Two-phase generation (area preloading): plan_settlement() on any thread, then on the main thread
	// begin_build() and build_step() until it returns true. build_step() stops once `deadline_us`
	// (build_clock_us() time; 0 = no deadline) has passed, after at least one piece of work, so the nodes
	// go up over several frames in exactly the order build_nodes() makes them. run() is the two together.
	// hidden: every node the build adds directly to the settlement's root (and every prop) starts hidden,
	// so the build can run in the tree unseen; reveal_step() then shows them, in build order, under the
	// same deadline rule (one set_visible each: the cost is spread, never one whole-tree toggle).
	void begin_build(SettlementPlan &&plan, bool hidden = false);
	~NeighborhoodGenerator();
	bool build_step(uint64_t deadline_us);
	bool reveal_step(uint64_t deadline_us);
	// The reverse, for a built settlement kept for later (WorldStreamer's cache): hides what is showing,
	// sliced the same way; a later reveal_step() shows exactly that again.
	bool conceal_step(uint64_t deadline_us);
	bool building() const { return build_stage_ >= 0; }
	int build_stage() const { return build_stage_; } // for timing reports
	// Per build stage: total ms / the longest single piece of work (timing reports).
	godot::String build_report() const;
	// For WorldStreamer's --stream-state-test: a house's door open (rung) / done for the night.
	bool door_open(int lot) const { return lot >= 0 && lot < int(doors_.size()) && doors_[lot].target < -1.0f; }
	bool house_closed(int lot) const { return lot >= 0 && lot < int(glow_.size()) && glow_[lot].closed; }
	static uint64_t build_clock_us();
	// The candy filter changed after the build (WorldStreamer: state moved between a preload and its
	// attach): rebuild the candy field with it.
	void rebuild_candy(const std::vector<char> &taken);
	// Share one loaded asset library between all the settlements of the world.
	void share_assets(AssetLibrary *lib) {
		assets_ = lib;
		assets_loaded_ = true;
	}
	const GenContext *context() const { return ctx_.get(); }
	// Candy pieces (by index) to leave out of the next build: taken on an earlier visit.
	void set_candy_filter(const std::vector<char> &taken) { candy_filter_ = taken; }

	void set_toon_material(const godot::Ref<godot::Material> &m) { toon_ = m; }
	godot::Ref<godot::Material> get_toon_material() const { return toon_; }
	void set_candy_material(const godot::Ref<godot::Material> &m) { candy_mat_ = m; }
	godot::Ref<godot::Material> get_candy_material() const { return candy_mat_; }
	void set_halo_material(const godot::Ref<godot::Material> &m) { halo_mat_ = m; }
	godot::Ref<godot::Material> get_halo_material() const { return halo_mat_; }
	void set_grass_material(const godot::Ref<godot::Material> &m) { grass_mat_ = m; }
	godot::Ref<godot::Material> get_grass_material() const { return grass_mat_; }
	void set_catalog_path(const godot::String &p) { catalog_path_ = p; }
	godot::String get_catalog_path() const { return catalog_path_; }

	godot::Vector3 get_spawn_position() const { return ctx_ ? ctx_->spawn : godot::Vector3(); }
	float get_spawn_yaw() const { return ctx_ ? ctx_->spawn_yaw : 0.0f; }
	godot::Vector3 get_moon_direction() const { return ctx_ ? ctx_->moon_dir : godot::Vector3(0, 0.4f, -1).normalized(); }
	godot::Vector3 get_center() const;
	// Yaw (radians, Godot convention) of the street direction nearest to a point, for the camera.
	float street_yaw_at(const godot::Vector3 &p) const;
	godot::PackedVector3Array get_doorbells() const;
	int get_lot_count() const { return ctx_ ? int(ctx_->lots.size()) : 0; }
	godot::String describe_lot(int lot) const;
	void open_door(int lot);
	void light_up_house(int lot);
	// A house trick-or-treated (all its candy found): its porch winds down for the night (porch light,
	// lanterns and porch jack-o'-lanterns dim, the rest of the house stays cozy) and a "Thank you!"
	// sign with its own little lantern drops in under the porch light. animate = false when an area
	// streams back in already done.
	void close_house(int lot, bool animate);
	CandyField *candy_field() const { return candy_; }
	godot::Node *get_candy_field() const;
	AssetLibrary &assets() {
		if (!assets_loaded_) {
			assets_->load(catalog_path_, toon_);
			assets_loaded_ = true;
		}
		return *assets_;
	}
	godot::Node3D *instantiate_asset(const godot::String &id);
	bool asset_published(const godot::String &id);

	void _process(double delta) override;

protected:
	static void _bind_methods();

private:
	// Time-sliced build: the step build_step() is in (-1: none), the item within it, and the deadline.
	struct BuildBudget {
		uint64_t deadline = 0;
		bool did = false; // at least one piece of work is always done per build_step()
		bool over() const { return did && deadline != 0 && build_clock_us() >= deadline; }
	};
	int build_stage_ = -1;
	size_t build_i_ = 0;
	float build_ms_[13] = {}, build_max_ms_[13] = {};
	BuildBudget budget_;
	bool packed_ = false;
	std::vector<godot::Array> packed_meshes_;
	std::vector<MagicPiece> packed_magic_;
	StringsData packed_strings_;
	struct CollisionBodies {
		godot::StaticBody3D *solids = nullptr, *structures = nullptr, *boundary = nullptr, *fences = nullptr;
		std::map<int, godot::StaticBody3D *> bouncy;
	} coll_;
	godot::Node3D *props_root_ = nullptr;
	std::vector<std::pair<int64_t, std::vector<const ScatterItem *>>> scatter_groups_;
	godot::Ref<godot::ArrayMesh> scatter_clump_;
	bool hide_new_ = false;
	std::vector<godot::Node3D *> hidden_;
	std::vector<godot::Node3D *> shown_; // every node put() added (conceal_step hides the visible ones)
	size_t reveal_i_ = 0;
	bool concealing_ = false;
	std::vector<godot::Node3D *> conceal_list_;
	size_t conceal_i_ = 0;
	// add_child for the nodes a hidden build shows later (see begin_build).
	void put(godot::Node *parent, godot::Node *child);
	godot::MeshInstance3D *add_mesh(MeshBuilder &b, const godot::String &name, int packed_index);
	void build_nodes();
	bool build_collision(godot::Node3D *world);
	bool build_props(godot::Node3D *world);
	bool build_lights(godot::Node3D *world);
	bool build_light_nodes();
	godot::Node3D *lights_root_ = nullptr;
	std::vector<LightSpec> light_list_;
	bool build_doors(godot::Node3D *world);
	bool build_strings(godot::Node3D *world);
	bool build_scatter(godot::Node3D *world);
	void build_candy(godot::Node3D *world);
	void build_fireflies(godot::Node3D *world);

	godot::Dictionary run(int64_t seed, SettlementKind kind, bool embedded, bool lane_open, int port_side = GenContext::PORT_FROM_SEED);
	void build_signs(godot::Node3D *world);
	std::unique_ptr<GenContext> ctx_;
	AssetLibrary own_assets_;
	AssetLibrary *assets_ = &own_assets_;
	bool assets_loaded_ = false;
	godot::Ref<godot::Material> toon_, candy_mat_, halo_mat_, grass_mat_;
	godot::String catalog_path_ = "res://assets/catalog.json";
	godot::Node3D *world_ = nullptr;
	std::vector<char> candy_filter_;
	CandyField *candy_ = nullptr;

	struct DoorNode {
		godot::Node3D *hinge = nullptr;
		float angle = 0.0f, target = 0.0f;
	};
	std::vector<DoorNode> doors_; // indexed by lot
	struct LotGlow {
		std::vector<godot::GeometryInstance3D *> lanterns; // porch lanterns
		std::vector<godot::GeometryInstance3D *> jacks;    // jack-o'-lanterns on and by the porch
		std::vector<godot::Vector3> jack_pos;
		std::vector<godot::OmniLight3D *> jack_lights;
		godot::OmniLight3D *porch = nullptr;
		godot::Node3D *sign = nullptr;
		godot::Transform3D sign_rest;
		float t = -1.0f; // seconds since it was closed (-1: open)
		bool closed = false;
	};
	void build_thanks_sign(int lot);
	std::vector<LotGlow> glow_;
	struct MagicNode {
		godot::MeshInstance3D *node = nullptr;
		float fade = 1.0f;
		bool visited = false;
	};
	std::vector<MagicNode> magic_nodes_;
	// The street spine in ~10 m pieces, each fading once every house near it has been visited (and
	// the pieces with no house near them once the whole area has).
	struct SpineChunk {
		godot::MeshInstance3D *node = nullptr;
		std::vector<int> lots;
		float fade = 0.7f;
		bool gone = false;
	};
	std::vector<SpineChunk> spine_;
	godot::Ref<godot::Material> magic_mat_;
	bool build_magic(godot::Node3D *world);

	enum SquishMode { SQUISH_WOBBLE, SQUISH_HOOD, SQUISH_DIP };
	struct Squishy {
		godot::Node3D *node = nullptr;
		godot::Transform3D rest; // the node's own (parent-relative) rest transform
		float t = 0.0f; // > 0 while wobbling
		SquishMode mode = SQUISH_WOBBLE;
		godot::Vector3 anchor; // where landings set it off (generator space)
		float reach = 2.2f;
	};
	std::vector<Squishy> squishies_;

public:
	void visit_house(int lot);
	// Bouncy pumpkins and inflatables squash and wobble when landed on.
	void squash_near(const godot::Vector3 &pos);
	void set_magic_material(const godot::Ref<godot::Material> &m) { magic_mat_ = m; }
	godot::Ref<godot::Material> get_magic_material() const { return magic_mat_; }
};

void set_instance_param_recursive(godot::Node *node, const godot::StringName &name, const godot::Variant &value);
// A clump of curved grass blades (UV.y root to tip) for grass.gdshader.
godot::Ref<godot::ArrayMesh> grass_clump_mesh();

} // namespace tot
