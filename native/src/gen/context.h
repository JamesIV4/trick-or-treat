#pragma once
// Generation context and stage entry points. NeighborhoodGenerator runs the stages in order:
// street -> lots -> households -> houses -> yards/props -> traversal -> candy -> dressing -> validate.

#include <cstdint>
#include <string>
#include <vector>

#include "core/rng.h"
#include "gen/asset_library.h"
#include "gen/types.h"
#include "mesh/mesh_builder.h"

namespace tot {

enum Stage : uint64_t {
	STAGE_STREET = 1,
	STAGE_LOTS,
	STAGE_HOUSEHOLDS,
	STAGE_HOUSE,
	STAGE_ZONES,
	STAGE_PROPS,
	STAGE_RHYTHM,
	STAGE_TRAVERSAL,
	STAGE_CANDY,
	STAGE_DRESSING,
	STAGE_SKY,
};

struct DoorSpec {
	int lot = -1;
	Transform3D hinge; // world; door swings around local Y, panel extends along local +X
	float width = 1.1f, height = 2.1f;
	int cell = 0;
	int trim_cell = 0;
};

enum StringStyle { STRING_BULBS, STRING_JACKS, STRING_BUNTING };

struct StringLight {
	int lot = -1;
	Vector3 a, b; // world anchor points
	float sag = 0.6f;
	StringStyle style = STRING_BULBS;
	Vector3 out; // horizontal direction jacks and pennants face (zero: either way)
};

struct SignSpec {
	std::string text;
	Transform3D xf; // world; text faces local +Z
	Color color = Color(1, 0.9f, 0.6f);
	float size = 1.0f;
};

struct MagicPath {
	int lot = -1; // -1: street spine (always on); otherwise the branch to that house's door
	std::vector<Vector3> points; // world, at glow height
};

// A springy piece of a building (a shop awning) built as its own node so it can dip when bounced on.
struct SpringSpec {
	MeshBuilder mesh; // relative to xf
	Transform3D xf; // world; the hinge it dips about is local X through the origin
	Vector3 anchor; // world centre of the bouncy surface
	float reach = 2.0f; // landings within this distance of the anchor set it off
};

struct ScatterItem {
	int kind; // 0 grass clump, 1 leaf chip, 2 clover
	Transform3D xf;
	int lot = -1;
	Color color = Color(1, 1, 1);
};

struct GenStats {
	int candy = 0;
	int candy_raised = 0;
	int candy_dropped = 0;
	int helpers_added = 0;
	int platforms = 0;
	int reachable_platforms = 0;
	int props = 0;
	int lights = 0;
	int openness_failures = 0;
	int rerolls = 0;
	std::vector<std::string> notes;
};

struct GenContext {
	uint64_t seed = 1;
	AssetLibrary *assets = nullptr;
	SettlementKind kind = SettlementKind::Neighborhood;
	bool embedded = false; // part of the open world: no ground plane, hay walls or backdrop silhouettes
	bool lane_open = false; // a road continues past the side-lane end
	StreetPlan street;
	std::vector<Lot> lots;
	std::vector<OBox> boxes;
	std::vector<PropPlacement> props;
	std::vector<LightSpec> lights;
	std::vector<CandySpec> candy;
	std::vector<DoorSpec> doors;
	std::vector<StringLight> strings;
	std::vector<ScatterItem> scatter;
	std::vector<MagicPath> magic;
	std::vector<SignSpec> signs;
	std::vector<SpringSpec> springs;
	std::vector<std::vector<Circle>> occupied; // per lot, lot-local xz circles
	std::vector<MeshBuilder> lot_mesh; // houses, fences, paths per lot
	MeshBuilder street_mesh;
	MeshBuilder boundary_mesh;
	GenStats stats;
	Vector3 spawn;
	float spawn_yaw = 0.0f;
	float bound_x0 = -50, bound_x1 = 50, bound_z0 = -40, bound_z1 = 40;
	Vector3 moon_dir = Vector3(0.35f, 0.42f, -0.84f); // toward the moon

	Rng rng(uint64_t stage, uint64_t a = 0, uint64_t b = 0) const { return Rng(sub_seed(seed, stage, a, b), stage * 2 + 1); }

	// Adds a collision/platform box in lot-local space (lot < 0: world space).
	void add_box(int lot, Vector3 center, Vector3 half, bool walkable, float yaw = 0.0f, Basis extra = Basis());
	void add_prop(int lot, const std::string &id, Vector3 local_pos, float local_yaw, float scale = 1.0f, float wind = 0.0f);
	bool occupied_at(int lot, float x, float z, float r) const;
	void occupy(int lot, float x, float z, float r);
	// Distance from a lot-local xz point to the lot's planned loop route (keep-out for props).
	float route_distance(int lot, float x, float z) const;
};

// Stages (implemented across street.cpp, houses.cpp, yards.cpp, candy.cpp).
void plan_street(GenContext &ctx);
void plan_lots(GenContext &ctx);
void assign_households(GenContext &ctx);
void plan_house(GenContext &ctx, Lot &lot);
void build_house(GenContext &ctx, Lot &lot);
void plan_routes(GenContext &ctx, Lot &lot);
void build_street(GenContext &ctx);
void build_boundary(GenContext &ctx);
void dress_yard(GenContext &ctx, Lot &lot);
void place_landmark(GenContext &ctx, Lot &lot);
void place_street_rhythm(GenContext &ctx);
void place_pocket_parks(GenContext &ctx);
void register_prop_collisions(GenContext &ctx);
void weave_candy(GenContext &ctx);
void plan_magic_paths(GenContext &ctx);
void validate_traversal(GenContext &ctx);
void scatter_dressing(GenContext &ctx);
// Towns (town.cpp): narrow shop lots with continuous storefronts and a town square.
void plan_town_lots(GenContext &ctx);
void assign_shops(GenContext &ctx);
void plan_shop(GenContext &ctx, Lot &lot);
void build_shop(GenContext &ctx, Lot &lot);
void plan_shop_routes(GenContext &ctx, Lot &lot);
void dress_shopfront(GenContext &ctx, Lot &lot);
void place_town_square(GenContext &ctx);

} // namespace tot
