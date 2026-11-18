#pragma once
// Plain data shared by the generator stages. All positions are world space unless noted.

#include <godot_cpp/variant/color.hpp>
#include <godot_cpp/variant/transform3d.hpp>
#include <godot_cpp/variant/vector3.hpp>

#include <cstdint>
#include <string>
#include <vector>

#include "core/consts.h"

namespace tot {

using godot::Basis;
using godot::Color;
using godot::Transform3D;
using godot::Vector3;

enum class Persona {
	PumpkinFanatics,
	GreenThumbs,
	YoungFamily,
	CozyGrandparents,
	CraftyArtists,
	Minimalists,
	SpookyCute,
	Bakers,
	Sporty,
	PetLovers,
	Stargazers,
	Tinkerers,
	Count
};

enum class Landmark {
	None,
	PumpkinPatch,
	Scarecrow,
	Sunflower,
	TreeHouse,
	HayRide,
	Cauldron,
	Inflatables,
	LanternPorch,
	CarvingContest,
	Count
};

const char *persona_name(Persona p);
const char *landmark_name(Landmark l);

enum class SettlementKind { Neighborhood, Town };

// A neighbourhood's roads, corridors and lots (at full depth) fit a square this size, centred on the
// settlement origin (Stage 2a: 152 m; Stage 2b: 2-3x the houses). WorldPlan::REGION is derived from it.
constexpr float HOOD_ENVELOPE = 270.0f;

// Stage 3b: the open world's terrain mesh is a lattice of this spacing in world x / z (each square split
// on its (x, z) - (x + 1, z + 1) diagonal; WorldStreamer::STEP must match, world_plan.cpp asserts it). A
// neighbourhood's origin sits on the lattice, so its heightfield and the terrain the player walks on are
// the same triangles.
constexpr float TERRAIN_LATTICE = 2.0f;

// A neighbourhood's ground (settlement-local): lattice vertices from (x0, z0), cut and filled to the graded
// roads and lot pads (gen/terrain.cpp); beyond the grid the natural relief carries on.
struct HoodTerrain {
	float x0 = 0.0f, z0 = 0.0f;
	int nx = 0, nz = 0;
	uint64_t seed = 0;
	std::vector<float> h; // nz rows of nx
	// The natural relief (Stage 3b round 2): a few broad hills and dips (Gaussian swells, 45-75 m radius) and a
	// faint roll, scaled to the hood's peak-to-trough relief and centred on 0.
	struct Swell {
		float x, z, r, a;
	};
	std::vector<Swell> swells;
	float scale = 1.0f, bias = 0.0f;
	float vertex(int ix, int iz) const;
	float at(float x, float z) const; // the lattice surface (exactly the terrain mesh's triangles)
};
// The natural relief a neighbourhood is built into (settlement-local, centred on 0).
float hood_natural(const HoodTerrain &t, float x, float z);

// Town shops (sign text and storefront dressing).
enum class ShopKind { Bakery, Candy, Toys, Books, Diner, Hardware, Florist, Costumes, Barber, Pets, Cinema, Pharmacy, Count };
const char *shop_sign(ShopKind k);

// Stage 4c: a town's street layout (drawn per seed) and its size class.
enum class TownTemplate {
	Cross, // a main street and one or two cross streets (T or +)
	Grid, // a straight main street crossed square-on every block (2-3 crossings, 32-60 m blocks)
	HighStreet, // a bowed high street with the square bulging off the outside of the bow
	Fork, // the main street forks in a Y
	Count
};
enum class TownSize { Small, Medium, Large };
enum class SquarePlace { Centre, Corner, End };
const char *town_template_name(TownTemplate t);

enum CandyKind {
	CANDY_PINK,
	CANDY_MINT,
	CANDY_GRAPE,
	CANDY_LEMON,
	CANDY_ORANGE,
	CANDY_CORN,
	CANDY_CHOCOLATE,
	CANDY_LOLLIPOP,
	CANDY_GOLDEN,
	CANDY_KIND_COUNT
};

// The everyday pieces (wrapped candies and candy corn) come first; each is worth 1.
constexpr int CANDY_BASIC_COUNT = CANDY_CORN + 1;

// Tier of any candy kind, classic or variety (gen/candy_kinds.cpp): 0 everyday .. 3 golden.
int candy_tier(int kind);

inline int candy_value(int kind) {
	static const int value[4] = { 1, 5, 25, 100 };
	return value[candy_tier(kind) & 3];
}

// Oriented box: collision volume and, when walkable, a platform whose top the player can stand on.
struct OBox {
	Transform3D xf; // centre and orientation
	Vector3 half;
	bool solid = true;
	bool walkable = false;
	bool bouncy = false;
	float bounce = 13.5f; // launch speed when bouncy
	bool cylinder = false; // physics shape: cylinder (radius half.x, height 2*half.y) instead of box
	bool boundary = false; // playground edge (own collision layer, ignored by the camera)
	bool shot_through = false; // yard fences: solid to walk into, but fireballs fly through
	int lot = -1;
	float top() const { return xf.origin.y + half.y; }
};

struct PropPlacement {
	std::string id;
	Transform3D xf;
	int lot = -1;
	float wind = 0.0f; // sway amount for foliage
	bool collide = true;
	bool bouncy = false; // landing on top springs the player up
	bool settled = false; // Stage 3c: already set on the ground (as part of a group); settle_street_props leaves it
};

enum LightKind { LIGHT_PORCH, LIGHT_LAMP, LIGHT_JACK, LIGHT_LANTERN };

struct LightSpec {
	Vector3 pos;
	Color color;
	float energy = 1.0f;
	float range = 6.0f;
	int lot = -1;
	LightKind kind = LIGHT_PORCH;
};

// Which placement pattern laid a piece (sweep stats and debugging only; not saved, not hashed).
enum CandyPattern {
	CPAT_OTHER, // street candy, hops across the road, bonus pieces
	CPAT_PATH, // front-walk line up to the porch steps
	CPAT_DRIVE, // driveway line (and its arc onto a parked car's hood)
	CPAT_OBSTACLE, // jump-point arc over a low fence, pumpkin cluster, hay bale or planter
	CPAT_SEQUENCE, // platform sequence: approach arc and chain
	CPAT_CROWN, // triangle over a sequence's top platform
	CPAT_ROOF, // porch-roof (or shop awning) run
	CPAT_RING, // ring round a landmark's centrepiece
	CPAT_SUMMIT, // landmark summit route
	CPAT_CLUSTER, // small ring or arc round a yard feature (porch steps, tree, pumpkins, planter)
	CPAT_LEGACY, // the pre-Stage 1e random ground rings and yard hops
	CPAT_DECK, // Stage 3c: a little ring on a house's flat roof deck
	CPAT_BOUNCE, // Stage 3c: a column over a bounce pad (car hood, trampoline, inflatable, awning), the top tier at its full jump + flap apex
	CPAT_COUNT
};

struct CandySpec {
	Vector3 pos;
	int kind = CANDY_PINK;
	int lot = -1;
	int group = -1; // rhythm group: a run of candy laid along one jump or hop sequence
	int id = -1; // index in the settlement's candy list (persists which pieces were taken)
	int pattern = CPAT_OTHER;
};

struct Circle {
	float x, z, r;
};

struct HouseSpec {
	float width = 8.0f, depth = 8.0f;
	int storeys = 1;
	float storey_h = 3.0f;
	float pitch_deg = 50.0f;
	bool ridge_along_x = true;
	float house_x = 0.0f; // lot-local centre x of the main body
	float front_z = -8.0f; // lot-local z of the front wall
	float porch_x = 0.0f, porch_w = 4.0f, porch_d = 2.4f;
	float deck_h = 0.6f;
	float door_x = 0.0f;
	bool garage = false;
	int garage_side = 1; // +1 right, -1 left
	bool chimney = true;
	int chimney_side = 1;
	bool roof_steps = false; // summit route (landmarks)
	int wall_cell = 0, roof_cell = 0, trim_cell = 0, door_cell = 0, garage_cell = 0;
	int fence_type = 0; // 0 none, 1 picket, 2 rail
	int fence_cell = 0;
	// Look and detail (all cells below are mat_cell-packed palette + material).
	int wall_mat = 0, upper_mat = 0, roof_mat = 0;
	int upper_cell = 0; // second-storey wall colour (Victorian two-tone)
	bool shutters = false;
	int shutter_cell = 0;
	bool flower_boxes = false;
	bool dormer = false;
	bool gingerbread = false;
	bool pediment = false;
	int curtain_cell = 0;
	int drive_mat = 0;
};

// Something low in a front yard the kid would jump over anyway (Stage 1e candy anchors).
enum class ObstacleKind { Fence, Pumpkins, Hay, Planter };

struct YardObstacle {
	ObstacleKind kind = ObstacleKind::Fence;
	Vector3 c; // lot-local centre on the ground
	Vector3 across; // horizontal unit direction a jump crosses it (either sense); zero: any direction (compact)
	float half_along = 0.0f; // half length along the obstacle (a fence segment); 0: compact
	float half_across = 0.3f; // half depth in the jump direction (or radius when compact)
	float top = 0.8f; // height to clear above the ground
};

// A yard feature a small candy cluster can sit round or beside (Stage 1e candy anchors).
enum class FeatureKind { Steps, Tree, Pumpkins, Planter, Bale, Doorside, Shopfront };

struct YardFeature {
	FeatureKind kind = FeatureKind::Tree;
	Vector3 c; // lot-local centre on the ground
	float r = 0.5f; // footprint radius (Steps: half width of the bottom step; its front is at c.z + r_z)
	float r_z = 0.0f; // Steps: half depth of the flight
};

struct Lot {
	int index = 0;
	int road = 0; // the road it fronts (GenContext::roads)
	int side = 1; // +1 on its road's left (the +z side of today's main street), -1 the other side
	float s = 0.0f; // station of the lot's front centre along its road
	int prev = -1, next = -1; // neighbouring lots along the same side of the same road (-1: none)
	Transform3D xf; // lot frame: origin front-centre on the sidewalk edge, +Z toward the street
	float width = 16.0f, depth = 22.0f; // width measured in stations along the road
	Persona persona = Persona::CozyGrandparents;
	Landmark landmark = Landmark::None;
	HouseSpec house;
	float drive_x = 0.0f; // lot-local driveway centre
	int passage_side = -1; // side passage (no driveway) side
	// Lot-local key points (y = height above ground).
	std::vector<Vector3> loop_route; // ground loop, two entrances
	std::vector<Vector3> roof_route; // porch-roof branch
	std::vector<Vector3> summit_route; // landmark summit branch
	// Front-yard anchors for the candy lines (Stage 1e), lot-local, y = walking surface height above the
	// lot's ground. Published by the builders (houses.cpp, yards.cpp, town.cpp) so candy never re-derives them.
	std::vector<Vector3> front_walk; // the walk from the sidewalk to the porch steps' top tread (door axis)
	std::vector<Vector3> drive_line; // driveway centreline from the sidewalk to where it stops being open
	bool car_in_drive = false;
	Vector3 car_hood; // top centre of the parked car's (bouncy) hood
	Vector3 car_hood_half; // its half extents (x across, z along the car)
	std::vector<YardObstacle> obstacles;
	std::vector<YardFeature> features; // porch steps, front tree, pumpkin clusters, planter, shop-front pieces
	bool has_centrepiece = false; // a landmark's centrepiece the candy ring goes round
	Vector3 centrepiece; // ground centre
	float centre_r = 1.0f; // footprint radius
	Vector3 doorbell; // world
	Vector3 porch_light; // world
	int faces_used = 0; // bitmask of jack-o'-lantern faces (neighbors never match)
	int shop = -1; // ShopKind for town lots, -1 for houses
	// Round a cul-de-sac bulb, facing its centre (Stage 2a). `road` is the bulb's road, `side` the half of
	// the ring it is on, and `s` carries on past the road's end round the ring toward the far point
	// (s_end + ring radius * (pi - angle from the road's direction)), so neighbours link round the ring.
	bool bulb = false;
	// Stage 3b: treads in the porch flight (3, more when the ground at its foot falls away from the pad).
	int porch_steps = 3;
	// Stage 3c: the porch flight (a shop's doorstep and its treads) as built, nearest the door first: each tread's
	// top (lot-local y), its z range and half width about the door axis. The flight never stops short of the ground
	// (equal risers, a landing every 8 treads); flight_foot is the lot-local z where it meets the ground.
	struct Tread {
		float top, z0, z1, hw;
	};
	std::vector<Tread> flight;
	float flight_riser = 0.2f, flight_foot = 0.0f;
	// Stage 3c: the front walk's flagstones as laid (lot-local: centre on the ground, half length along and half
	// width across the walk, yaw about +y), from the foot of the flight to the sidewalk.
	struct Flag {
		Vector3 c;
		float hl, hw, yaw;
	};
	std::vector<Flag> walk_flags;
	float walk_end = 0.0f; // lot-local z where the walk meets the sidewalk (0, or past a strip of ground in front of the lot)
	// Stage 3c: the flat roof deck on the truncated ridge (lot-local centre of its top and half sizes; half.x 0: none).
	Vector3 deck_c, deck_half;
	// Stage 3b: bottoms of the house, garage and porch foundations (lot-local y; 0: the slab on the pad).
	float found_house = 0.0f, found_garage = 0.0f, found_porch = 0.0f;
	// Stage 3b: a painted stone retaining wall down the -x / +x side (height of the step to the lower neighbour's
	// pad; 0: none). Only the higher of two touching lots builds it.
	float retain[2] = { 0.0f, 0.0f };
	// Ship: every retaining wall the lot stands behind, as lot-local segments on the ground plane (a -> b, y unused):
	// the side walls above (retain[k], built by the 3b code) and the walls along a line where the lot meets a lot of
	// another street behind or beside it (plan_hood_terrain decides them, build_house builds them; side = false).
	struct Wall {
		Vector3 a, b;
		float step = 0.0f; // the step the wall holds (the higher yard over the lower one)
		bool side = false; // a 3b side wall (retain[k]): built by its own code
		Vector3 out; // lot-local direction toward the lower yard
	};
	std::vector<Wall> walls;
	Vector3 to_world(float x, float y, float z) const { return xf.xform(Vector3(x, y, z)); }
	Vector3 to_world(const Vector3 &p) const { return xf.xform(p); }
	Vector3 to_local(const Vector3 &p) const { return xf.affine_inverse().xform(p); }
};

// Stage 4c: a town's square. `area` is its footprint as a lot (index -1, shop -2, no house): frontage frame on
// its road's sidewalk edge, +Z toward the road, origin.y its level pad once the terrain is planned. Its front
// strip runs with the street, then a paved ramp up or down to the pad (SQUARE_RAMP_* in terrain.cpp).
struct TownSquare {
	bool valid = false;
	SquarePlace place = SquarePlace::Centre;
	Lot area;
	float pad_z = -6.0f; // lot-local z where the level pad starts (the ramp ends)
	// Where the travel gate and the candy cart stand (settlement-local, on the pad; place_town_square).
	Vector3 gate;
	Transform3D cart;
};

// ---------------------------------------------------------------- road network
// A settlement's streets: a small tree of roads. Road 0 is the trunk (today: the main street); every
// other road branches off a parent at a T junction. A road is a centreline polyline with stations `s`
// increasing along it; lateral offsets run along `left` = (-t.z, 0, t.x) for the direction of travel t
// (toward +z on a road running toward +x; lateral > 0 is side +1). y on the centreline is the road
// surface height.
// Nothing here assumes a road is monotonic in x, straight, or at y = 0.

enum class RoadEnd {
	Port, // leaves the settlement: a country road may carry on from here
	Bulb, // cul-de-sac turning circle
	Junction, // joins its parent road
	DeadEnd, // just stops (a hedge)
};

// The nearest point of a road to a query point (horizontal distance).
struct RoadHit {
	int road = -1;
	float s = 0.0f; // station of the nearest centreline point, within the road
	float s_ext = 0.0f; // the same, carried on in a straight line past either end (beyond the road)
	float beyond = 0.0f; // how far past an end the query point is (0: alongside the road)
	float lateral = 0.0f; // signed offset from the centreline along `left` (+: side +1)
	float dist = 1e9f; // horizontal distance from the query point to `point`
	Vector3 point; // the centreline point
	Vector3 tangent, left;
	float y = 0.0f; // road surface height at `point`
};

// Surface bands across a road corridor, most road-like first (RoadNet::zone).
enum class RoadZone { Asphalt, Curb, Verge, Walk, Off };

// A stretch of one side of a road where its curb, verge and sidewalk are not built straight (a side
// road's mouth, a bulb's shoulders): fillets and junction patches take over there.
struct BandCut {
	int side = 1;
	float s0 = 0.0f, s1 = 0.0f;
};

// A rounded corner of the asphalt edge (Stage 2a): the corners of a T junction and the shoulders where a
// cul-de-sac road flares into its bulb. The asphalt edge follows the circle of radius r round c (c lies
// outside the asphalt, on the block side) between the directions u0 and u1 from c; the curb, verge and
// sidewalk bands follow it inward (radius r - curb, r - curb - verge, r - curb - walk). p0 / p1 are the
// true asphalt-edge points on the roads where the arc starts (u0 side) and ends (u1 side).
struct RoadFillet {
	Vector3 c;
	float r = 6.0f;
	Vector3 u0, u1;
	Vector3 p0, p1;
	int road = -1; // the side road (or the bulb's road) it belongs to
	bool bulb = false; // a bulb's shoulder (else a junction corner: u0 toward the parent road)
	// Where each end of the arc meets the straight bands: end 0 on road0 at station s0 on the lateral side
	// lat0 (+1 / -1); end 1 likewise, or (bulb) on the turning circle's ring.
	int road0 = -1, road1 = -1;
	float s0 = 0.0f, s1 = 0.0f;
	float lat0 = 1.0f, lat1 = 1.0f;
	float half_road = 4.0f, curb = 0.3f, verge = 1.0f, walk = 2.7f;
	bool in_sector(const Vector3 &p) const {
		const float vx = p.x - c.x, vz = p.z - c.z;
		const float cr = u0.x * u1.z - u0.z * u1.x;
		const float a = u0.x * vz - u0.z * vx, b = vx * u1.z - vz * u1.x;
		return a * cr >= 0.0f && b * cr >= 0.0f && vx * (u0.x + u1.x) + vz * (u0.z + u1.z) > 0.0f;
	}
	float r_out() const { return r - curb - walk; } // the sidewalk's outer edge
};

struct Road {
	std::vector<Vector3> pts; // centreline samples
	std::vector<Vector3> left; // horizontal unit normal per sample: (-t.z, 0, t.x) for the travel direction t
	std::vector<float> s; // station per sample, strictly increasing (need not start at 0)
	// Even sampling: s[i] = s[0] + i * step, looked up by index instead of by search.
	float step = 0.0f;
	// Legacy sine street (Stage 0 only): monotonic in x, stations are world x, and a point's station
	// is its x. Keeps today's layouts bit for bit; spline roads leave it off.
	bool x_station = false;
	// A straight road: at(s) = pts[0] + dir * (s - s[0]) exactly.
	bool straight = false;
	Vector3 dir;
	float half_road = 4.0f;
	float curb = 0.3f;
	float walk = 2.7f; // grass verge + sidewalk
	float verge = 1.0f;
	int parent = -1; // road it branches from (-1: a trunk)
	float parent_s = 0.0f; // station on the parent where it branches off
	int parent_side = 0; // the parent's side it leaves on (+1 left, -1 right)
	RoadEnd start_kind = RoadEnd::Port, end_kind = RoadEnd::Port;
	float lots_s0 = 0.0f, lots_s1 = 0.0f; // the stretch the settlement is built along (lots, dressing)
	// Stage 2a (spline roads). A Bulb end: an asphalt disc of radius bulb_r round bulb_c (the road's last
	// point); the straight bands stop at bulb_s where the shoulder fillets begin, and the ring leaves the
	// entry open within bulb_entry radians of the road's back direction (seen from bulb_c).
	Vector3 bulb_c;
	float bulb_r = 0.0f, bulb_s = 0.0f, bulb_entry = 0.0f;
	// A side road's own asphalt starts here (the junction patch covers it from the parent's edge).
	float asphalt_s = 0.0f;
	std::vector<BandCut> cuts;
	// project() acceleration: xz bounding boxes (x0, z0, x1, z1) of 16-segment chunks; empty: linear scan.
	std::vector<float> chunk_box;

	float edge() const { return half_road + curb + walk; } // sidewalk outer edge offset
	bool bulb() const { return end_kind == RoadEnd::Bulb && bulb_r > 0.0f; }
	bool cut(int side, float st) const {
		for (const BandCut &c : cuts) {
			if (c.side == side && st >= c.s0 && st <= c.s1) {
				return true;
			}
		}
		return false;
	}
	void build_index();
	float s_begin() const { return s.front(); }
	float s_end() const { return s.back(); }
	float length() const { return s.back() - s.front(); }
	Vector3 at(float st) const; // centreline point (clamped to the road)
	Vector3 left_at(float st) const;
	Vector3 tangent_at(float st) const; // horizontal-ish unit direction of travel (central difference over +-1 m)
	Vector3 point(float st, float lateral) const { return at(st) + left_at(st) * lateral; }
	// A frontage frame at station st on `side`: origin on the sidewalk's outer edge, +Z toward the road.
	Transform3D frontage_frame(float st, int side) const;
	// Outward direction at an end (start: false, end: true), from the last few samples.
	Vector3 end_dir(bool at_end) const;
	RoadHit project(const Vector3 &p) const;
};

struct RoadJunction {
	int parent = -1, child = -1;
	float s = 0.0f; // station on the parent
	int side = 0; // parent's side the child leaves on
};

struct RoadNet {
	std::vector<Road> roads;
	std::vector<RoadFillet> fillets; // junction corners and bulb shoulders (spline roads only)
	bool empty() const { return roads.empty(); }
	// The surface under a point: asphalt (roads, bulbs, junction patches), curb, verge, sidewalk or none,
	// taking the most road-like band any road, bulb or fillet claims there (`which`: the claiming road).
	RoadZone zone(const Vector3 &p, int *which = nullptr) const;
	const Road &trunk() const { return roads[0]; }
	// Roads branching off `road`, by station.
	std::vector<RoadJunction> junctions(int road) const;
	RoadHit project(int road, const Vector3 &p) const;
	// The road a point belongs to: a branch claims points inside its own corridor (|lateral| < edge)
	// once past its mouth (s_ext > the parent's edge), and beyond its far end when that is a Port (the
	// country road carrying on); otherwise the closest trunk road (parent < 0).
	// y is that road's surface height.
	RoadHit nearest(const Vector3 &p) const;
};

} // namespace tot
