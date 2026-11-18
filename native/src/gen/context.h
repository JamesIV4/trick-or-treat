#pragma once
// Generation context and stage entry points. NeighborhoodGenerator runs the stages in order:
// street -> lots -> households -> houses -> yards/props -> traversal -> candy -> dressing -> validate.

#include <cstdint>
#include <functional>
#include <memory>
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
	float width = 0.0f, height = 0.0f; // the board the text has to fit inside (m; 0 = no limit)
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
	int lot_rejects = 0; // Stage 2b: road layouts rerolled for a house count outside the band
	int road_rejects[16] = {}; // why road layouts were rerolled (street.cpp HOOD_REJECT codes)
	// Validation per CandyPattern (lot and street candy alike): pieces dropped / lifted out of solids.
	int pattern_dropped[CPAT_COUNT] = {};
	int pattern_raised[CPAT_COUNT] = {};
	int bounce_reject[3] = {}; // Stage 3c: bounce pads passed over for a column: axis blocked, a canopy, another platform in reach
	std::vector<std::string> notes;
};

struct GenContext {
	uint64_t seed = 1;
	AssetLibrary *assets = nullptr;
	SettlementKind kind = SettlementKind::Neighborhood;
	bool embedded = false; // part of the open world: no ground plane, hay walls or backdrop silhouettes
	bool lane_open = false; // a road continues past the side-lane end
	// Neighbourhoods (Stage 2a): the branch road that leaves as the N/S port, +1 north (+z), -1 south, 0 none;
	// PORT_FROM_SEED (standalone): decided by the seed. WorldPlan decides it in the open world.
	static constexpr int PORT_FROM_SEED = 2;
	// Towns (Stage 4c): TOWN_PORTS + a mask of the N/S port roads to build (1: north, 2: south), decided by
	// WorldPlan so every one meets a road; PORT_FROM_SEED: the seed decides (standalone).
	static constexpr int TOWN_PORTS = 16;
	int port_side = PORT_FROM_SEED;
	// Stage 4c: the town's layout template, size class and square (plan_street / plan_town_lots).
	TownTemplate town_template = TownTemplate::Cross;
	TownSize town_size = TownSize::Medium;
	int town_ports = 0; // the N/S port roads built (1 north, 2 south)
	TownSquare square;
	std::vector<Lot> plazas; // Stage 4c: paved footprints (the square and the little plazas; open grass keeps off)
	// Neighbourhoods: road 0 the main road (W port to E port), then the branches (T junctions off road 0) ending in
	// cul-de-sac bulbs or the N/S port. Towns (Stage 4c): road 0 the main street (W to E), then side streets (T or
	// + crossings) ending in a hedge (DeadEnd) or running out as an N/S port.
	RoadNet roads;
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
	bool hood_lots_ready = false; // plan_street placed the hood's lots for the final road net (Stage 2b)
	Vector3 moon_dir = Vector3(0.35f, 0.42f, -0.84f); // toward the moon

	Rng rng(uint64_t stage, uint64_t a = 0, uint64_t b = 0) const { return Rng(sub_seed(seed, stage, a, b), stage * 2 + 1); }

	// Stage 3b: a neighbourhood's graded ground (null: flat at 0, towns until Stage 4c). Shared, read-only.
	std::shared_ptr<const HoodTerrain> terrain;
	// Ground height under a settlement-local point: the terrain lattice the player walks on.
	float ground(float x, float z) const { return terrain ? terrain->at(x, z) : 0.0f; }
	// The ground under lot-local (lx, lz), relative to the lot's pad (lot-local y).
	float lot_ground(const Lot &lot, float lx, float lz) const;
	// Inside the lot's house, porch (and its flight) or garage footprint, grown by pad.
	bool in_structure(const Lot &lot, float lx, float lz, float pad = 0.0f) const;
	// How far a yard thing standing at lot-local (lx, lz) is lifted to sit on the ground: lot_ground outside
	// the buildings, 0 on them (things on a porch, a roof or a building's own solids stay with the pad).
	float yard_lift(const Lot &lot, float lx, float lz) const;
	const Road &road_of(const Lot &lot) const { return roads.roads[lot.road]; }

	// Adds a collision/platform box in lot-local space (lot < 0: world space).
	void add_box(int lot, Vector3 center, Vector3 half, bool walkable, float yaw = 0.0f, Basis extra = Basis());
	void add_prop(int lot, const std::string &id, Vector3 local_pos, float local_yaw, float scale = 1.0f, float wind = 0.0f);
	bool occupied_at(int lot, float x, float z, float r) const;
	void occupy(int lot, float x, float z, float r);
	// Distance from a lot-local xz point to the lot's planned loop route (keep-out for props).
	float route_distance(int lot, float x, float z) const;
};

// Stages (implemented across street.cpp, houses.cpp, yards.cpp, candy.cpp).
void plan_street(GenContext &ctx); // builds ctx.roads (needs no assets: WorldPlan calls it)
// Stage 4c: a town's roads (template, size, ports, spawn); plan_street calls it for towns.
void plan_town_roads(GenContext &ctx);
// A 30 m stretch of road M within ~10 degrees of its direction at station st starts within 32 m of it (the
// town cine shot needs one by the spawn).
bool town_straight_near(const Road &M, float st);
// Neighbourhoods: plan_street already places the lots (Stage 2b: a layout is kept only when its house count
// lands in the band), so this keeps them; plan_hood_lots is the placer itself.
void plan_lots(GenContext &ctx);
void plan_hood_lots(GenContext &ctx);
// Lot::prev / next: neighbours along the same side of the same road, by station.
void link_lot_neighbours(GenContext &ctx);
// Station stretches of a road's frontage on side that no lot and no side-road mouth covers, longer
// than min_len, within the built stretch (lots_s0 + 1 .. lots_s1 - 1). Pocket parks, the town square.
std::vector<std::pair<float, float>> frontage_gaps(const GenContext &ctx, int road, int side, float min_len);
// Penetration depth of two lot footprints (oriented rectangles in xz; <= 0: apart or just touching).
float lot_overlap(const Lot &a, const Lot &b);
// How far a lot's footprint reaches into road corridors (asphalt to sidewalk edge): straight corridors,
// bulb rings, fillets and the straight run-on past each Port end (35 m). own: its own road's straight
// corridor and bulb ring only; other: everything else. Negative: clear by that much.
void lot_corridor_reach(const GenContext &ctx, const Lot &lot, float &own, float &other);
// Lot-local z (+ toward the road) where the curb starts straight in front of lot-local x on the lot's road
// (the sidewalk + verge depth there; Road::walk on a straight road, more on a bulb or the inside of a bend).
float frontage_reach(const GenContext &ctx, const Lot &lot, float x);
void assign_households(GenContext &ctx);
// Stage 3b (neighbourhoods): grades the roads, sets each lot's pad (lot.xf.origin.y) and builds ctx.terrain.
// Needs the lots' house plans (plan_house); runs before the first build_house.
void plan_hood_terrain(GenContext &ctx);
// A lot-local rectangle laid on the ground (the terrain lattice clipped to it, lift above it): lawns, drives.
// Flat at lift without terrain.
void drape_lot_rect(const GenContext &ctx, MeshBuilder &m, const Lot &lot, float x0, float x1, float z0, float z1, float lift, int cell);
// Stage 3c: how a ground prop takes the surface under it. Flat, low things lie on the ground's plane (tilt clamped);
// tall things stand plumb with their base sunk to the lowest ground under them; cars rest on their four wheels.
enum class PropSettle { Flat, Upright, Car };
PropSettle prop_settle_kind(const std::string &id);
struct GroundFit {
	Vector3 up = Vector3(0, 1, 0); // the fitted plane's normal
	float y = 0.0f; // the plane's height under the prop's origin, lowered so no sample is beneath it (nothing floats)
	float y_centred = 0.0f; // the same plane centred on the samples (each within +-twist): a car on its wheels
	float centre_y = 0.0f;
	float low = 0.0f; // the lowest surface sample (an upright thing sinks to it)
	float bury = 0.0f; // the deepest sample above the lowered plane
	float twist = 0.0f;
};
// surface(x, z) in the same space as p (a lot-local prop: yard_lift); the footprint is [x0, x1] x [z0, z1] in the
// prop's frame (turned by yaw, scaled), sampled n x n.
GroundFit fit_footprint(const std::function<float(float, float)> &surface, const Vector3 &p, float yaw, float scale, float x0, float x1, float z0, float z1,
		float max_tilt, int n);
// The rotation taking +y to up (the shortest one).
Basis tilt_to(const Vector3 &up);
// The lowest surface under a round base of radius r at p (centre and a ring of 8): where an upright thing sinks to.
float footprint_low(const std::function<float(float, float)> &surface, const Vector3 &p, float r);
// The walking surface at a settlement point: the street's own surfaces (asphalt, curb, verge, sidewalk) where they
// are laid, else the terrain lattice.
float street_surface(const GenContext &ctx, const Vector3 &p);
// Stage 3c: a shop lot's surfaces (lot-local, over its pad): 2 the level paving strip in front of the shop, 1 the
// doorstep and its treads, 0 the sidewalk or the service yard's ground; and the height of that surface.
int shop_support(const Lot &lot, float lx, float lz);
float shop_surface(const GenContext &ctx, const Lot &lot, float lx, float lz);
// Stage 3c: street and park props (lot -1) on their surface, as Yard::put does for the yards'. Before collisions.
void settle_street_props(GenContext &ctx);
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
// Stage 3c: which boxes the kid can get onto (walkable ones from the ground, then jump + flap or a bounce pad's
// throw between them), and a bounce pad's reach (feet above its top: full jump + flap, less a margin).
std::vector<char> platform_reach(const GenContext &ctx, bool bounce = true); // bounce: false leaves the pads' throws out
float bounce_reach(float speed);
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
