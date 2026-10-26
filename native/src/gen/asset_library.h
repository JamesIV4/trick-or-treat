#pragma once
// Loads game/assets/catalog.json and the published glTF props. Applies the shared toon material,
// caches bounds and sockets, and builds simple placeholders for assets that are not published yet.

#include <godot_cpp/classes/material.hpp>
#include <godot_cpp/classes/mesh.hpp>
#include <godot_cpp/classes/node3d.hpp>
#include <godot_cpp/classes/packed_scene.hpp>
#include <godot_cpp/variant/aabb.hpp>
#include <godot_cpp/variant/string.hpp>

#include <map>
#include <string>
#include <unordered_map>
#include <vector>

namespace tot {

struct AssetInfo {
	std::string id;
	godot::String path;
	std::string collision = "none"; // none | box | cylinder | trunk
	bool walkable = false;
	float radius = 0.2f, height = 2.0f; // trunk collision
	int candy_tier = -1; // candy only: 0 everyday .. 3 golden
	godot::Color glow = godot::Color(1, 0.8f, 0.4f); // candy only: outline glow colour
	float bounce = 0.0f; // > 0: landing on top springs the player up at this speed (trampolines)
	float collision_top = -1.0f; // box collision: top height when the mesh rises above it (plants in a planter)
	// Cars: the closed hood panel's bounds (asset space). The "hood" node flaps up when bounced on.
	godot::AABB hood;
	bool has_hood = false;
	godot::AABB aabb; // local bounds of the visual
	std::map<std::string, godot::Transform3D> sockets;
	godot::Ref<godot::PackedScene> scene;
	godot::Ref<godot::Mesh> placeholder;
	godot::Ref<godot::Mesh> first_mesh; // for MultiMesh use (candy, bulbs)
	bool published = false;
};

class AssetLibrary {
public:
	void load(const godot::String &catalog_path, const godot::Ref<godot::Material> &material);
	const AssetInfo *get(const std::string &id);
	// New node tree for the asset with the toon material applied. Never null (placeholder fallback).
	godot::Node3D *instantiate(const std::string &id);
	godot::Ref<godot::Mesh> mesh(const std::string &id);
	// Every mesh of the asset baked into one (for MultiMesh instancing of multi-part props).
	godot::Ref<godot::Mesh> merged_mesh(const std::string &id);
	// One named part (and everything under it) baked into a mesh centred on the part's origin, which
	// is returned in origin (asset space). Null when the asset is unpublished or has no such part.
	godot::Ref<godot::Mesh> part_mesh(const std::string &id, const godot::String &part, godot::Vector3 *origin = nullptr);
	int published_count() const { return published_; }
	// Every catalog id, sorted (deterministic order).
	std::vector<std::string> ids() const;
	int total_count() const { return int(assets_.size()); }

private:
	void prepare(AssetInfo &a);
	godot::Ref<godot::Mesh> make_placeholder(const std::string &id) const;
	std::unordered_map<std::string, AssetInfo> assets_;
	godot::Ref<godot::Material> material_;
	int published_ = 0;
};

void apply_material_recursive(godot::Node *node, const godot::Ref<godot::Material> &mat);

} // namespace tot
