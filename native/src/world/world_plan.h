#pragma once
// The infinite world's layout, deterministic from one seed. Space is a grid of REGION-sized regions;
// each is a neighborhood, a small town or a stretch of woods. Country roads link neighbouring
// regions (street end to street end, or through a woods junction), and the terrain is rolling
// hills that level out under settlements, around junctions and along the roads.

#include <godot_cpp/variant/vector3.hpp>

#include <cstdint>
#include <memory>
#include <string>
#include <unordered_map>
#include <vector>

#include "gen/types.h"

namespace tot {

enum class RegionKind { Woods, Neighborhood, Town };

enum PortSide { PORT_E, PORT_W, PORT_N, PORT_S }; // N is +z

struct Port {
	bool valid = false;
	Vector3 pos; // world; y = road height
	Vector3 dir; // outward, horizontal unit
};

struct RegionPlan {
	int i = 0, j = 0;
	RegionKind kind = RegionKind::Woods;
	uint64_t seed = 0;
	Vector3 origin; // settlement generator origin or woods junction; y = level ground height
	float x0 = 0, x1 = 0, z0 = 0, z1 = 0; // settlement footprint (world xz)
	Vector3 spawn; // settlements: world spawn point in the intersection
	float spawn_yaw = 0.0f;
	bool lane_open = false;
	int lane_side = -1; // PORT_N or PORT_S for settlements (-1: a neighbourhood without a N/S port road)
	// Neighbourhoods: the branch that runs out as the N/S port (+1 north, -1 south, 0 none), the
	// generator's GenContext::port_side. Chosen so the neighbour always has the matching port. Towns (Stage
	// 4c): GenContext::TOWN_PORTS + the mask of N/S port roads to build (1 north, 2 south), likewise matched.
	int hood_port = 0;
	Port ports[4];
	std::string name;
	// Stage 3b: a settlement's graded ground (settlement-local, relative to origin.y; towns too since Stage 4c).
	std::shared_ptr<const HoodTerrain> terrain;
	bool settlement() const { return kind != RegionKind::Woods; }
};

// The seed a settlement is generated from (the plan's street, lots and road ports must use the same).
inline int64_t settlement_seed(const RegionPlan &r) {
	return int64_t(r.seed & 0x7FFFFFFFFFFFull);
}

struct RoadPlan {
	std::vector<Vector3> pts; // world centreline every ~3 m; y = road surface height
	float x0 = 0, x1 = 0, z0 = 0, z1 = 0; // bounds of the centreline
};

class WorldPlan {
public:
	static constexpr float JITTER = 14.0f; // settlement origin offset from the region centre (each axis)
	static constexpr float BLEND = 26.0f; // hills -> settlement terrain
	// Room for a neighbourhood footprint up to the hood envelope: 2 x (reach + blend) <= REGION - 2 x jitter,
	// where reach = half the envelope + what lots may poke past it and the footprint margins (<= 8 m), rounded
	// up to 10 m (Stage 2a: 280 m for the 152 m envelope; Stage 2b: 360 m for 250 m).
	static constexpr float REGION = float(int((2.0f * (HOOD_ENVELOPE * 0.5f + 8.0f + BLEND) + 2.0f * JITTER) / 10.0f + 0.999f) * 10);
	static constexpr float ROAD_HALF = 3.4f;

	void reset(uint64_t seed);
	uint64_t seed() const { return seed_; }
	const RegionPlan &region(int i, int j);
	static void region_of(float x, float z, int &i, int &j);
	// The road between two 4-neighbouring regions (nullptr if they are not connected).
	const RoadPlan *road(int i0, int j0, int i1, int j1);
	// Final terrain height.
	float height(float x, float z);
	// Untouched rolling hills.
	float natural(float x, float z) const;
	// Distance to the nearest road centreline within ~40 m (1e9 when none).
	float road_distance(float x, float z);
	// 1 inside a settlement footprint, fading to 0 over the blend margin.
	float settlement_mask(float x, float z);
	// Woods density field in [0, 1].
	float forest_density(float x, float z) const;

	// Roads whose centreline passes within ~40 m of (x, z); `out` must hold 18.
	void nearby_roads(float x, float z, const RoadPlan **out, int &count);

	// Area preloading (WorldStreamer): the regions another plan of the same seed has worked out already (a
	// worker thread's copy; base() plans a settlement's street and lots, too slow for the main thread).
	// They are exactly what base() would make here; entries this plan has are kept. Both plans must be
	// idle (no other thread inside either). Later per-region caches should be copied here as well.
	void adopt_regions(const WorldPlan &from);
	int region_count() const { return int(base_.size()); }

private:
	const RegionPlan &base(int i, int j);
	// The region's kind alone (no footprint: safe to ask from inside base()).
	RegionKind kind_of(int i, int j) const;
	// A neighbourhood's N/S port road (+1 / -1 / 0), from the kinds around it (see world_plan.cpp).
	int hood_port(int i, int j, uint64_t seed);
	// Stage 4c: the N/S port roads a town offers (mask: 1 north, 2 south; seed only) and the ones it builds (the
	// offered ones its neighbour meets).
	int town_offer(int i, int j) const;
	int town_ports(int i, int j);
	float natural_low(float x, float z) const;
	void build_road(RoadPlan &road, const Port &a, const Port &b, uint64_t key);
	static uint64_t key(int i, int j) { return (uint64_t(uint32_t(i)) << 32) | uint64_t(uint32_t(j)); }

	uint64_t seed_ = 1;
	std::unordered_map<uint64_t, RegionPlan> base_;
	std::unordered_map<uint64_t, RegionPlan> regions_;
	std::unordered_map<uint64_t, std::unique_ptr<RoadPlan>> roads_; // null entry: no road
};

} // namespace tot
