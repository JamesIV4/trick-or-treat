#pragma once
// --sweep helpers (headless generator validation, festival_game.cpp):
//  - SweepDump: a per-settlement fingerprint of everything the generator emits (exact float bits, in
//    emission order, hashed per category) so a refactor can prove "no behaviour change".
//  - sweep_layout_stats: layout stats the world rework needs (lot overlaps, lots intruding on road
//    corridors, candy per lot, nest spacing).

#include <godot_cpp/variant/dictionary.hpp>

#include <cstdint>
#include <fstream>
#include <string>

namespace tot {

struct GenContext;
struct InfestationLayout;
class WorldPlan;

class SweepDump {
public:
	// `detail`: a category name whose every entry is also written out in full (as "D ..." lines).
	bool open(const std::string &path, const std::string &detail);
	void settlement(const std::string &label, int64_t seed, const GenContext &ctx, const InfestationLayout *inf);
	void world(int64_t seed, WorldPlan &plan);
	bool is_open() const { return out_.is_open(); }

private:
	std::ofstream out_;
	std::string detail_;
};

// Extra stats for a generated settlement (and its infestation layout, if any), merged into `st`.
void sweep_layout_stats(const GenContext &ctx, const InfestationLayout *inf, godot::Dictionary &st);
// Stage 2a: the world plan around a seed (5 x 5 regions): kinds, footprint reach against the region rule,
// neighbourhood port roads, every port matched across the boundary and joined by a country road.
void sweep_world_stats(int64_t seed, godot::Dictionary &st);
// Stage 2a: the monsters' walk grid over the settlement just generated and A* over sample routes (chases
// within aggro range, returns to the home nest from the area's edge): searches that hit the cap.
class NeighborhoodGenerator;
void sweep_nav_probe(NeighborhoodGenerator *gen, const InfestationLayout &inf, godot::Dictionary &st);
// Stage 2a: a top-down picture's worth of the settlement just generated (upward street / boundary mesh triangles with
// their palette colour, lot rectangles, street props, gate, cart, spawn, nests, street candy, magic spine) as JSON.
void sweep_geo_dump(const GenContext &ctx, const InfestationLayout &inf, const std::string &path);
// Exercises RoadNet's general path (roads doubling back in x, searched stations, y != 0, a branch) on a
// synthetic net; returns "<n> checks, <k> failed ...".
std::string roadnet_selfcheck();

} // namespace tot
