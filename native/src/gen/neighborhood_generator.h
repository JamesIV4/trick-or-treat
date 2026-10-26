#pragma once

#include <godot_cpp/classes/material.hpp>
#include <godot_cpp/classes/mesh_instance3d.hpp>
#include <godot_cpp/classes/node3d.hpp>
#include <godot_cpp/classes/omni_light3d.hpp>
#include <godot_cpp/variant/dictionary.hpp>

#include <memory>
#include <vector>

#include "gen/asset_library.h"
#include "gen/context.h"

namespace tot {

class CandyField;

// Runs the ten generation stages for a seed and builds the playable neighborhood under itself.
class NeighborhoodGenerator : public godot::Node3D {
	GDCLASS(NeighborhoodGenerator, godot::Node3D)

public:
	godot::Dictionary generate(int64_t seed);
	// Open-world use: a neighborhood or town that is one piece of the streamed world (no ground
	// plane of its own, open street ends; `lane_open` when a road continues past the side lane).
	godot::Dictionary generate_settlement(int64_t seed, SettlementKind kind, bool lane_open);
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
	void build_nodes();
	void build_collision(godot::Node3D *world);
	void build_props(godot::Node3D *world);
	void build_lights(godot::Node3D *world);
	void build_doors(godot::Node3D *world);
	void build_strings(godot::Node3D *world);
	void build_scatter(godot::Node3D *world);
	void build_candy(godot::Node3D *world);
	void build_fireflies(godot::Node3D *world);

	godot::Dictionary run(int64_t seed, SettlementKind kind, bool embedded, bool lane_open);
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
		std::vector<godot::GeometryInstance3D *> lanterns;
		godot::OmniLight3D *porch = nullptr;
		float pulse = 0.0f;
		bool lit = false;
	};
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
	void build_magic(godot::Node3D *world);

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
